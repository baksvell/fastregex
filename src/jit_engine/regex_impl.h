// regex_impl.h
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <chrono>
#include <string>
#include <regex>
#include <stdexcept> // Для std::runtime_error

// Unified export macros
#ifdef _WIN32
    #ifdef FASTREGEX_EXPORTS
        #define EXPORT_API __declspec(dllexport)
    #else
        #define EXPORT_API __declspec(dllimport)
    #endif
#else
    #define EXPORT_API __attribute__((visibility("default")))
#endif

// Regex compilation flags
enum RegexCompileFlags {
    REGEX_DEFAULT      = 0,       // Default behavior
    REGEX_IGNORECASE   = 1 << 0,  // Case insensitive matching
    REGEX_MULTILINE    = 1 << 1,  // Multiline mode (^/$ match line boundaries)
    REGEX_DOTALL       = 1 << 2,  // Dot matches newline characters
    REGEX_OPTIMIZE     = 1 << 3,  // Enable aggressive optimizations
    REGEX_LITERAL      = 1 << 4,  // Treat pattern as literal string
    REGEX_UTF8         = 1 << 5   // Enable UTF-8 mode
};

// Match result flags
enum MatchResultFlags {
    MATCH_SUCCESS       = 1 << 0,  // Successful match
    MATCH_PARTIAL       = 1 << 1,  // Partial match
    MATCH_INVALID_UTF8  = 1 << 2   // Invalid UTF-8 sequence detected
};

// SIMD acceleration modes
enum SIMDMode {
    SIMD_AUTO = 0,
    SIMD_AVX512,
    SIMD_AVX2,
    SIMD_SSE42,
    SIMD_NEON,
    SIMD_SCALAR
};

#ifdef __cplusplus
extern "C" {
#endif

// Core operations
EXPORT_API bool llvm_regex_match_impl(const char* str, size_t len, const char* pattern);
EXPORT_API bool llvm_regex_search_impl(const char* str, size_t len, const char* pattern);

// Compilation and execution
EXPORT_API void* llvm_regex_compile_ex(const char* pattern, int flags, bool optimize_simple);
EXPORT_API bool llvm_regex_exec(void* compiled_regex, const char* str, size_t len, bool full_match);

// Memory management
EXPORT_API void llvm_regex_free(void* compiled_regex);
EXPORT_API size_t llvm_regex_get_memory_usage(void* compiled_regex);

// Cache management
EXPORT_API void llvm_regex_clear_cache_full(bool clear_jit_cache);
EXPORT_API size_t llvm_regex_get_cache_size();
EXPORT_API void llvm_regex_set_cache_limits(size_t max_entries, size_t max_memory_mb);
EXPORT_API void llvm_regex_purge_cache();

// SIMD acceleration
EXPORT_API bool llvm_regex_simd_search(const char* str, size_t len, const char* pattern, int simd_mode);

// Diagnostics and validation
EXPORT_API bool llvm_regex_validate_pattern(const char* pattern, char* error_buf, size_t error_len);
EXPORT_API size_t llvm_regex_get_stats(int stat_type);  // 0-cache hits, 1-misses, 2-allocations

// Thread safety control
EXPORT_API void llvm_regex_set_thread_safe(bool enabled);
EXPORT_API bool llvm_regex_is_thread_safe();

#ifdef __cplusplus
} // extern "C"

namespace fastregex::internal {

class CompiledRegex {
public:
    CompiledRegex(const char* pattern, int flags, bool optimize_simple = true, std::string* error_msg = nullptr)
        : handle_(nullptr), compile_time_(0)
    {
        auto start = std::chrono::high_resolution_clock::now();
        handle_ = llvm_regex_compile_ex(pattern, flags, optimize_simple);
        auto end = std::chrono::high_resolution_clock::now();
        compile_time_ = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        if (!handle_ && error_msg) {
            char error_buf[256];
            if (llvm_regex_validate_pattern(pattern, error_buf, sizeof(error_buf))) {
                *error_msg = error_buf;
            } else {
                *error_msg = "Unknown regex compilation error";
            }
        }
    }

    ~CompiledRegex() {
        if (handle_) {
            llvm_regex_free(handle_);
        }
    }

    // Non-copyable
    CompiledRegex(const CompiledRegex&) = delete;
    CompiledRegex& operator=(const CompiledRegex&) = delete;

    // Movable
    CompiledRegex(CompiledRegex&& other) noexcept
        : handle_(other.handle_), compile_time_(other.compile_time_) {
        other.handle_ = nullptr;
        other.compile_time_ = std::chrono::microseconds(0);
    }

    bool match(const char* str, size_t len) const {
        return handle_ ? llvm_regex_exec(handle_, str, len, true) : false;
    }

    bool search(const char* str, size_t len) const {
        return handle_ ? llvm_regex_exec(handle_, str, len, false) : false;
    }

    size_t memory_usage() const {
        return handle_ ? llvm_regex_get_memory_usage(handle_) : 0;
    }

    std::chrono::microseconds get_compile_time() const {
        return compile_time_;
    }

    explicit operator bool() const { return handle_ != nullptr; }

private:
    void* handle_;
    std::chrono::microseconds compile_time_;
};

// Optimized version for simple patterns
class OptimizedRegex : public CompiledRegex {
public:
    explicit OptimizedRegex(const char* pattern, std::string* error_msg = nullptr)
        : CompiledRegex(pattern, REGEX_OPTIMIZE | REGEX_LITERAL, true, error_msg) {}
};

} // namespace fastregex::internal
#endif