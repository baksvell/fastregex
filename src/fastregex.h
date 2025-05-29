// fastregex.h
#pragma once

#include "regex_impl.h"  // Для объявлений llvm_regex_* функций
#include <string>
#include <atomic>
#include <chrono>
#include <memory>
#include <cstddef>
#include <vector>
#include <regex>

namespace fastregex {

enum class RegexFlags {
    NONE = 0,
    IGNORECASE = 1 << 0,  // Case-insensitive matching
    MULTILINE  = 1 << 1,  // Multi-line mode (^/$ match start/end of line)
    DOTALL     = 1 << 2,  // Dot matches newline characters
    OPTIMIZE   = 1 << 3,  // Enable aggressive optimizations
    LITERAL    = 1 << 4   // Treat pattern as literal string
};

// Bitwise operations for flags
constexpr RegexFlags operator|(RegexFlags a, RegexFlags b) noexcept {
    return static_cast<RegexFlags>(static_cast<int>(a) | static_cast<int>(b));
}

constexpr RegexFlags operator&(RegexFlags a, RegexFlags b) noexcept {
    return static_cast<RegexFlags>(static_cast<int>(a) & static_cast<int>(b));
}

constexpr bool has_flag(RegexFlags flags, RegexFlags test) noexcept {
    return (flags & test) != RegexFlags::NONE;
}

class FastRegex {
public:
    // Construction and initialization
    FastRegex(const std::string& pattern,
             bool use_simd = true,
             bool debug = false,
             RegexFlags flags = RegexFlags::NONE);
    ~FastRegex();

    // Non-copyable but movable
    FastRegex(const FastRegex&) = delete;
    FastRegex& operator=(const FastRegex&) = delete;
    FastRegex(FastRegex&&) noexcept;
    FastRegex& operator=(FastRegex&&) noexcept;

    // Matching operations
    bool match(const std::string& s) const;
    bool match(const char* str, size_t len) const;

    bool search(const std::string& s) const;
    bool search(const char* str, size_t len) const;

    bool contains(const std::string& s) const { return search(s); }

    std::vector<std::string> find_all(const std::string& s) const;
    std::string replace(const std::string& s, const std::string& replacement) const;

    // Property access
    const std::string& pattern() const noexcept { return pattern_; }
    size_t pattern_length() const noexcept { return pattern_.length(); }

    bool use_simd() const noexcept { return use_simd_; }
    void set_use_simd(bool value) noexcept { use_simd_ = value; }

    bool debug_mode() const noexcept { return debug_mode_; }
    void set_debug_mode(bool value) noexcept { debug_mode_ = value; }

    RegexFlags flags() const noexcept { return flags_; }
    void set_flags(RegexFlags flags) noexcept { flags_ = flags; }

    // Performance and cache management
    static void clear_cache() noexcept;
    static size_t cache_size() noexcept;
    static double hit_rate() noexcept;
    static void set_cache_capacity(size_t max_size) noexcept;

    // Diagnostics
    bool is_jit_compiled() const noexcept;
    std::chrono::microseconds compile_time() const noexcept;

private:
    struct Impl {
        void* compiled_regex = nullptr;
        bool jit_compiled = false;
        std::chrono::microseconds compile_time{0};
    };

    std::unique_ptr<Impl> impl_;
    std::string pattern_;
    bool use_simd_;
    bool debug_mode_;
    RegexFlags flags_;
    mutable std::chrono::steady_clock::time_point last_used_;

    // Implementation details
    bool match_impl(const char* str, size_t len) const;
    bool search_impl(const char* str, size_t len) const;

    void ensure_compiled() const;
    void compile_pattern();
};

// Helper functions
bool is_valid_pattern(const std::string& pattern) noexcept;
std::string escape_special_chars(const std::string& pattern);

} // namespace fastregex