#include "regex_jit.h"
#include "simd.h"
#include "regex_impl.h"
#include <llvm/ExecutionEngine/JITSymbol.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h>
#include <llvm/ExecutionEngine/Orc/Shared/ExecutorSymbolDef.h>
#include <llvm/ExecutionEngine/Orc/Core.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Utils.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Type.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/Error.h>
#include <unordered_map>
#include <mutex>
#include <stdexcept>
#include <chrono>
#include <atomic>
#include <iostream>
#include <cstddef>

// Инициализация LLVM компонентов
extern "C" {
    void LLVMInitializeX86AsmParser();
    void LLVMInitializeX86Disassembler();
}

using namespace llvm;
using namespace llvm::orc;

#define DEBUG_LOG 1
#if DEBUG_LOG
    #define LOG(msg) std::cerr << "[LOG] " << msg << std::endl
#else
    #define LOG(msg)
#endif

void RegexJIT::initializeLLVM() {
    static std::once_flag flag;
    std::call_once(flag, []() {
        InitializeNativeTarget();
        InitializeNativeTargetAsmPrinter();
        LLVMInitializeX86AsmParser();
        LLVMInitializeX86Disassembler();
    });
}

RegexJIT::RegexJIT(const std::string& pattern) :
    context_(std::make_unique<LLVMContext>()),
    module_(std::make_unique<Module>("regex_jit", *context_)),
    builder_(std::make_unique<IRBuilder<>>(*context_)),
    pattern_(pattern) {

    try {
        initializeLLVM();

        // Создаем функции match и search
        FunctionType* func_type = FunctionType::get(
            builder_->getInt1Ty(),
            {builder_->getPtrTy(), builder_->getInt64Ty()},
            false
        );

        match_func_ = Function::Create(func_type, Function::ExternalLinkage, "match", module_.get());
        search_func_ = Function::Create(func_type, Function::ExternalLinkage, "search", module_.get());

        generateIR();
        optimizeModule(*module_);

        // Настройка JIT
        auto jit = cantFail(LLJITBuilder()
            .setObjectLinkingLayerCreator(
                [](ExecutionSession& ES, const Triple& /*TT*/) {
                    auto GetMemoryManager = []() -> std::unique_ptr<SectionMemoryManager> {
                        return std::make_unique<SectionMemoryManager>();
                    };
                    auto ObjLinkingLayer = std::make_unique<RTDyldObjectLinkingLayer>(
                        ES, GetMemoryManager);

                    ObjLinkingLayer->setOverrideObjectFlagsWithResponsibilityFlags(true);
                    ObjLinkingLayer->setAutoClaimResponsibilityForObjectSymbols(true);

                    return ObjLinkingLayer;
                })
            .create());

        auto& jd = jit->getMainJITDylib();
        jd.addGenerator(
            cantFail(DynamicLibrarySearchGenerator::GetForCurrentProcess(
                jit->getDataLayout().getGlobalPrefix())));

        // Добавление символов
        auto& ES = jit->getExecutionSession();
        auto match_sym = ES.intern("llvm_regex_match_impl");
        auto search_sym = ES.intern("llvm_regex_search_impl");

        SymbolMap symbols;
        symbols[match_sym] = ExecutorSymbolDef(
            ExecutorAddr::fromPtr(llvm_regex_match_impl),
            JITSymbolFlags::Exported);
        symbols[search_sym] = ExecutorSymbolDef(
            ExecutorAddr::fromPtr(llvm_regex_search_impl),
            JITSymbolFlags::Exported);

        cantFail(jd.define(absoluteSymbols(symbols)));

        ThreadSafeModule tsm(std::move(module_), std::move(context_));
        cantFail(jit->addIRModule(std::move(tsm)));

        // Получаем указатели на функции
        auto match_addr = cantFail(jit->lookup("match"));
        auto search_addr = cantFail(jit->lookup("search"));

        compiled_match_func_ = reinterpret_cast<bool(*)(const char*, size_t)>(match_addr.toPtr<void()>());
        compiled_search_func_ = reinterpret_cast<bool(*)(const char*, size_t)>(search_addr.toPtr<void()>());

        jit_ = std::move(jit);
        initialized_ = true;
        is_simple_pattern_ = pattern_.find_first_of("^$.*+?|\\[](){}") == std::string::npos;
        LOG("Successfully compiled pattern: " + pattern);
    } catch (const std::exception& e) {
        error_ = std::string("JIT compilation error: ") + e.what();
        cleanup();
        throw std::runtime_error(error_);
    } catch (...) {
        error_ = "Unknown error during JIT compilation";
        cleanup();
        throw std::runtime_error(error_);
    }
}

RegexJIT::~RegexJIT() {
    cleanup();
}

void RegexJIT::cleanup() {
    compiled_match_func_ = nullptr;
    compiled_search_func_ = nullptr;
    jit_.reset();
    builder_.reset();
    module_.reset();
    context_.reset();
    initialized_ = false;
}

bool RegexJIT::match(const char* str, size_t len) const {
    if (!initialized_ || !compiled_match_func_) {
        LOG("Match failed: JIT not initialized");
        return false;
    }
    if (!str) {
        LOG("Match failed: null string");
        return false;
    }
    try {
        return compiled_match_func_(str, len);
    } catch (...) {
        LOG("Match failed: exception in compiled function");
        return false;
    }
}

bool RegexJIT::search(const char* str, size_t len) const {
    if (!initialized_ || !compiled_search_func_) {
        LOG("Search failed: JIT not initialized");
        return false;
    }
    if (!str) {
        LOG("Search failed: null string");
        return false;
    }
    try {
        return compiled_search_func_(str, len);
    } catch (...) {
        LOG("Search failed: exception in compiled function");
        return false;
    }
}

Value* RegexJIT::createCharComparison(Value* str_ptr, Value* idx, char c) {
    Value* char_ptr = builder_->CreateGEP(builder_->getInt8Ty(), str_ptr, idx);
    Value* current_char = builder_->CreateLoad(builder_->getInt8Ty(), char_ptr);
    return builder_->CreateICmpEQ(current_char, builder_->getInt8(c));
}

void RegexJIT::generateSimpleLiteralIR(Function* func, bool /*is_match*/) {
    BasicBlock* entry = BasicBlock::Create(*context_, "entry", func);
    BasicBlock* check = BasicBlock::Create(*context_, "check", func);
    BasicBlock* exit = BasicBlock::Create(*context_, "exit", func);

    Value* str_ptr = func->getArg(0);
    Value* str_len = func->getArg(1);
    Value* pattern_len = builder_->getInt64(pattern_.size());

    builder_->SetInsertPoint(entry);

    // Для match проверяем только что строка не короче паттерна
    Value* len_condition = builder_->CreateICmpUGE(str_len, pattern_len);
    builder_->CreateCondBr(len_condition, check, exit);

    builder_->SetInsertPoint(check);
    Value* found = builder_->getTrue();
    for (size_t i = 0; i < pattern_.size(); ++i) {
        Value* cmp = createCharComparison(str_ptr, builder_->getInt64(i), pattern_[i]);
        found = builder_->CreateAnd(found, cmp);
    }
    builder_->CreateBr(exit);

    builder_->SetInsertPoint(exit);
    PHINode* result = builder_->CreatePHI(builder_->getInt1Ty(), 2);
    result->addIncoming(builder_->getFalse(), entry);
    result->addIncoming(found, check);
    builder_->CreateRet(result);
}

void RegexJIT::generateGenericIR(Function* func, bool is_match) {
    BasicBlock* entry = BasicBlock::Create(*context_, "entry", func);
    builder_->SetInsertPoint(entry);

    FunctionType* regex_type = FunctionType::get(
        builder_->getInt1Ty(),
        {builder_->getPtrTy(), builder_->getInt64Ty(), builder_->getPtrTy()},
        false
    );

    const char* func_name = is_match ? "llvm_regex_match_impl" : "llvm_regex_search_impl";
    FunctionCallee regex_func = module_->getOrInsertFunction(func_name, regex_type);

    Value* pattern_str = builder_->CreateGlobalString(pattern_);
    Value* result = builder_->CreateCall(
        regex_func,
        {func->getArg(0), func->getArg(1), pattern_str}
    );
    builder_->CreateRet(result);
}

void RegexJIT::generateIR() {
    // Проверяем, является ли паттерн простым литералом без спецсимволов
    is_simple_pattern_ = pattern_.find_first_of("^$.*+?|\\[](){}") == std::string::npos;

    if (is_simple_pattern_) {
        // Быстрый путь для простых строк
        generateSimpleLiteralIR(match_func_, true);
        generateSimpleLiteralIR(search_func_, false);
    } else {
        // Общий случай для сложных паттернов
        generateGenericIR(match_func_, true);
        generateGenericIR(search_func_, false);
    }

    if (verifyModule(*module_, &errs())) {
        throw std::runtime_error("Generated IR module verification failed");
    }
}

void RegexJIT::optimizeModule(llvm::Module& module) {
    PassBuilder pb;
    LoopAnalysisManager lam;
    FunctionAnalysisManager fam;
    CGSCCAnalysisManager cgam;
    ModuleAnalysisManager mam;

    pb.registerModuleAnalyses(mam);
    pb.registerCGSCCAnalyses(cgam);
    pb.registerFunctionAnalyses(fam);
    pb.registerLoopAnalyses(lam);
    pb.crossRegisterProxies(lam, fam, cgam, mam);

    ModulePassManager mpm = pb.buildPerModuleDefaultPipeline(OptimizationLevel::O3);
    mpm.run(module, mam);
}

RegexCache::RegexCache(size_t max_size) : max_size_(max_size) {
    const std::vector<std::string> common_patterns = {
        R"(\d+)", R"(\w+)", R"(\s+)", R"(\b\w+\b)", R"(.*)", R"(^$)",
        R"([a-zA-Z]+)", R"([0-9]+)", R"(\S+)", R"(\D+)",
        R"(^\d+$)", R"(^\w+$)", R"(\d{4}-\d{2}-\d{2})",
        R"([a-z]+)", R"([A-Z]+)", R"([0-9a-fA-F]+)",
        R"(^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$)"
    };

    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& pattern : common_patterns) {
        try {
            auto& entry = cache_[pattern];
            entry.jit = std::make_unique<RegexJIT>(pattern);
            entry.last_used = std::chrono::steady_clock::now();
            entry.hit_count = 0;
            LOG("Precompiled pattern: " + pattern);
        } catch (...) {
            LOG("Failed to precompile pattern: " + pattern);
        }
    }
}

RegexCache::~RegexCache() {
    clear();
}

RegexJIT* RegexCache::getJIT(const std::string& pattern) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::steady_clock::now();

    auto it = cache_.find(pattern);
    if (it != cache_.end()) {
        it->second.last_used = now;
        it->second.hit_count++;
        return it->second.jit.get();
    }

    if (cache_.size() >= max_size_) {
        auto oldest = cache_.begin();
        for (auto it = cache_.begin(); it != cache_.end(); ++it) {
            if (it->second.last_used < oldest->second.last_used) {
                oldest = it;
            }
        }
        LOG("Evicting old pattern: " + oldest->first);
        cache_.erase(oldest);
    }

    try {
        auto& entry = cache_[pattern];
        entry.jit = std::make_unique<RegexJIT>(pattern);
        entry.last_used = now;
        entry.hit_count = 0;
        LOG("Compiled new pattern: " + pattern);
        return entry.jit.get();
    } catch (...) {
        cache_.erase(pattern);
        throw std::runtime_error("Failed to compile pattern: " + pattern);
    }
}

void RegexCache::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.clear();
    LOG("Cache cleared");
}

size_t RegexCache::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_.size();
}

RegexCache global_regex_cache(100);

extern "C" {
    bool fastregex_match(const char* pattern, const char* text) {
        if (!pattern || !text) return false;
        try {
            return global_regex_cache.getJIT(pattern)->match(text, strlen(text));
        } catch (...) {
            return false;
        }
    }

    bool fastregex_search(const char* pattern, const char* text) {
        if (!pattern || !text) return false;
        try {
            return global_regex_cache.getJIT(pattern)->search(text, strlen(text));
        } catch (...) {
            return false;
        }
    }

    void fastregex_clear_cache() {
        global_regex_cache.clear();
    }

    size_t fastregex_cache_size() {
        return global_regex_cache.size();
    }
}