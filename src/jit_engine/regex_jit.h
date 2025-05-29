#ifndef REGEX_JIT_H
#define REGEX_JIT_H

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/Support/Error.h>
#include <memory>
#include <string>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <chrono>
#include <vector>

class RegexJIT {
private:
    // Основные компоненты LLVM
    std::unique_ptr<llvm::LLVMContext> context_;
    std::unique_ptr<llvm::Module> module_;
    std::unique_ptr<llvm::IRBuilder<>> builder_;
    std::unique_ptr<llvm::orc::LLJIT> jit_;

    // Скомпилированные функции
    llvm::Function* match_func_ = nullptr;
    llvm::Function* search_func_ = nullptr;

    // Указатели на скомпилированные функции
    bool (*compiled_match_func_)(const char*, size_t) = nullptr;
    bool (*compiled_search_func_)(const char*, size_t) = nullptr;

    // Данные паттерна и ошибок
    std::string pattern_;
    std::string error_;
    std::atomic<bool> initialized_{false};
    std::atomic<bool> is_simple_pattern_{false};

    // Инициализация LLVM (теперь потокобезопасная)
    static void initializeLLVM();

    // Вспомогательные методы для генерации IR
    llvm::Value* createCharComparison(llvm::Value* str_ptr, llvm::Value* idx, char c);
    llvm::Value* createRangeCheck(llvm::Value* val, llvm::Value* min, llvm::Value* max);
    llvm::Value* createPatternLengthCheck(llvm::Value* str_len, size_t pattern_len);

    // Методы анализа паттерна
    bool analyzePattern() const;
    bool isFixedLengthPattern() const;
    size_t getFixedLength() const;

    // Методы генерации IR
    void generateIR();
    void generateSimpleLiteralIR(llvm::Function* func, bool is_match);
    void generateFixedLengthIR(llvm::Function* func, bool is_match);
    void generateGenericIR(llvm::Function* func, bool is_match);
    void generateBoundedMatchIR(llvm::Function* func);

    // Оптимизация модуля
    void optimizeModule(llvm::Module& module);

    // Очистка ресурсов
    void cleanup();

public:
    explicit RegexJIT(const std::string& pattern);
    ~RegexJIT();

    // Интерфейс для работы с регулярными выражениями
    bool match(const char* str, size_t len) const;
    bool search(const char* str, size_t len) const;

    // Информация о состоянии
    bool is_valid() const { return initialized_.load(); }
    const std::string& last_error() const { return error_; }
    const std::string& pattern() const { return pattern_; }
    bool is_simple() const { return is_simple_pattern_.load(); }

    // Запрет копирования и перемещения
    RegexJIT(const RegexJIT&) = delete;
    RegexJIT& operator=(const RegexJIT&) = delete;
    RegexJIT(RegexJIT&&) = delete;
    RegexJIT& operator=(RegexJIT&&) = delete;
};

class RegexCache {
private:
    struct CacheEntry {
        std::unique_ptr<RegexJIT> jit;
        std::atomic<size_t> hit_count{0};
        std::chrono::steady_clock::time_point last_used;
        std::atomic<bool> is_compiling{false};
    };

    mutable std::mutex mutex_;
    std::unordered_map<std::string, CacheEntry> cache_;
    size_t max_size_;
    std::vector<std::string> common_patterns_;

    void evict_oldest();
    void precompile_common_patterns();

public:
    explicit RegexCache(size_t max_size = 100);
    ~RegexCache();

    // Основной интерфейс
    RegexJIT* getJIT(const std::string& pattern);

    // Управление кэшем
    void clear();
    size_t size() const;

    // Статистика
    size_t hit_count(const std::string& pattern) const;
    size_t miss_count() const;

    // Настройки
    void set_max_size(size_t new_size);
    void add_common_pattern(const std::string& pattern);

private:
    std::atomic<size_t> miss_count_{0};
};

#endif // REGEX_JIT_H