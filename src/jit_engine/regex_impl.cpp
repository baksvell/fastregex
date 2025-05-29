#include "regex_impl.h"
#include "simd.h"
#include <regex>
#include <string>
#include <unordered_map>
#include <mutex>
#include <algorithm>
#include <atomic>
#include <memory>
#include <vector>
#include <cctype>
#include <stdexcept>
#include <cstring>
#include <iostream>
#include <chrono>

#ifdef _WIN32
    #define EXPORT_API __declspec(dllexport)
#else
    #define EXPORT_API __attribute__((visibility("default")))
#endif

namespace {

inline bool is_digit(char c) noexcept {
    return c >= '0' && c <= '9';
}

inline bool is_word(char c) noexcept {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') ||
           c == '_';
}

inline bool is_space(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\n' ||
           c == '\r' || c == '\f' || c == '\v';
}

inline bool is_word_boundary(const char* str, size_t pos, size_t len) noexcept {
    bool prev_is_word = (pos > 0) ? is_word(str[pos-1]) : false;
    bool current_is_word = (pos < len) ? is_word(str[pos]) : false;
    return prev_is_word != current_is_word;
}

bool full_match(const std::string& str, const std::string& pattern) noexcept {
    return str == pattern;
}

bool fast_path_match(const std::string& str, const std::string& pattern) noexcept {
    if (pattern.empty()) return str.empty();

    const bool has_start_anchor = !pattern.empty() && pattern[0] == '^';
    const std::string match_pattern = has_start_anchor ? pattern : "^" + pattern;

    if (match_pattern == "^.*") return true;
    if (match_pattern == "^$" || match_pattern == "^^$") return str.empty();

    if (match_pattern == R"(^\d+$)") {
        return !str.empty() && std::all_of(str.begin(), str.end(), is_digit);
    }
    if (match_pattern == R"(^\w+$)") {
        return !str.empty() && std::all_of(str.begin(), str.end(), is_word);
    }
    if (match_pattern == R"(^\d+)") {
        return !str.empty() && is_digit(str[0]);
    }
    if (match_pattern == R"(^\w+)") {
        return !str.empty() && is_word(str[0]);
    }
    if (match_pattern == R"(^\s+)") {
        return !str.empty() && is_space(str[0]);
    }
    if (match_pattern == R"(^\b\w+\b)") {
        if (str.empty()) return false;
        return is_word_boundary(str.data(), 0, str.size()) &&
               is_word_boundary(str.data(), str.size(), str.size()) &&
               std::all_of(str.begin(), str.end(), is_word);
    }

    if (has_start_anchor && pattern.back() == '$') {
        if (pattern.size() > 1) {
            const std::string content = pattern.substr(1, pattern.size()-2);
            return full_match(str, content);
        }
        return str.empty();
    }

    if (match_pattern.find_first_of("^$.*+?|\\[](){}") == std::string::npos) {
        return str.compare(0, match_pattern.size()-1, match_pattern.substr(1)) == 0;
    }

    return false;
}

bool fast_path_search(const std::string& str, const std::string& pattern) noexcept {
    if (pattern.empty()) return !str.empty();
    if (pattern == ".*") return !str.empty();

    const bool has_start_anchor = !pattern.empty() && pattern[0] == '^';
    const bool has_end_anchor = !pattern.empty() && pattern.back() == '$';

    if (has_start_anchor && has_end_anchor) {
        const std::string content = pattern.substr(1, pattern.size()-2);
        return full_match(str, content);
    }

    if (pattern == R"(\d+)") return std::any_of(str.begin(), str.end(), is_digit);
    if (pattern == R"(\w+)") return std::any_of(str.begin(), str.end(), is_word);
    if (pattern == R"(\s+)") return std::any_of(str.begin(), str.end(), is_space);
    if (pattern == R"(\b\w+\b)") {
        for (size_t i = 0; i < str.size(); ++i) {
            if (is_word_boundary(str.data(), i, str.size())) {
                size_t j = i;
                while (j < str.size() && is_word(str[j])) ++j;
                if (j > i && (j == str.size() || is_word_boundary(str.data(), j, str.size()))) {
                    return true;
                }
            }
        }
        return false;
    }

    if (pattern.find_first_of("^$.*+?|\\[](){}") == std::string::npos) {
        return str.find(pattern) != std::string::npos;
    }

    if (has_end_anchor) {
        const std::string base = pattern.substr(0, pattern.size()-1);
        if (str.size() < base.size()) return false;
        return str.compare(str.size()-base.size(), base.size(), base) == 0;
    }

    return false;
}

} // anonymous namespace

class RegexCache {
private:
    struct CacheEntry {
        std::regex compiled;
        std::atomic<size_t> hits{0};
        std::chrono::steady_clock::time_point last_used;
        const bool is_match_pattern;

        explicit CacheEntry(std::regex&& re, bool is_match) noexcept
            : compiled(std::move(re)),
              last_used(std::chrono::steady_clock::now()),
              is_match_pattern(is_match) {}
    };

    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::unique_ptr<CacheEntry>> cache_;
    const size_t max_size_ = 100;
    std::atomic<size_t> miss_count_{0};

    static const std::vector<std::string> common_patterns_;

    void precompile_common_patterns() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& pattern : common_patterns_) {
            try {
                auto flags = std::regex_constants::ECMAScript |
                             std::regex_constants::optimize;
                const bool is_match = pattern[0] == '^';
                cache_[pattern] = std::make_unique<CacheEntry>(
                    std::regex(pattern, flags), is_match
                );
            } catch (...) {}
        }
    }

    void evict_least_used() noexcept {
        auto oldest = cache_.begin();
        for (auto it = cache_.begin(); it != cache_.end(); ++it) {
            if (it->second->last_used < oldest->second->last_used) {
                oldest = it;
            }
        }
        cache_.erase(oldest);
    }

public:
    RegexCache() {
        precompile_common_patterns();
    }

    bool match(const std::string& str, const std::string& pattern) noexcept {
        std::string match_pattern = pattern;
        if (pattern.empty() || pattern[0] != '^') {
            match_pattern = "^" + pattern;
        }

        if (bool fast_result = fast_path_match(str, match_pattern); fast_result) {
            return fast_result;
        }

        std::unique_lock<std::mutex> lock(mutex_);
        try {
            auto& entry = get_or_create_entry(match_pattern, true);
            lock.unlock();

            entry->hits++;
            entry->last_used = std::chrono::steady_clock::now();
            return std::regex_search(str, entry->compiled,
                                   std::regex_constants::match_continuous);
        } catch (...) {
            miss_count_++;
            return false;
        }
    }

    bool search(const std::string& str, const std::string& pattern) noexcept {
        if (bool fast_result = fast_path_search(str, pattern); fast_result) {
            return fast_result;
        }

        std::unique_lock<std::mutex> lock(mutex_);
        try {
            auto& entry = get_or_create_entry(pattern, false);
            lock.unlock();

            entry->hits++;
            entry->last_used = std::chrono::steady_clock::now();

            if (!pattern.empty() && pattern[0] == '^' && pattern.back() == '$') {
                return std::regex_match(str, entry->compiled);
            }

            if (!pattern.empty() && pattern[0] == '^') {
                return std::regex_search(str, entry->compiled,
                                      std::regex_constants::match_continuous);
            }

            return std::regex_search(str, entry->compiled);
        } catch (...) {
            miss_count_++;
            return false;
        }
    }

    void clear() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_.clear();
    }

    size_t size() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return cache_.size();
    }

    size_t miss_count() const noexcept {
        return miss_count_.load();
    }

    std::unique_ptr<CacheEntry>& get_or_create_entry(const std::string& pattern, bool is_match) {
        if (auto it = cache_.find(pattern); it != cache_.end()) {
            return it->second;
        }

        if (cache_.size() >= max_size_) {
            evict_least_used();
        }

        try {
            auto flags = std::regex_constants::ECMAScript |
                        std::regex_constants::optimize;
            auto entry = std::make_unique<CacheEntry>(
                std::regex(pattern, flags), is_match
            );
            return cache_[pattern] = std::move(entry);
        } catch (const std::regex_error& e) {
            throw std::runtime_error("Invalid regex pattern '" + pattern + "': " + e.what());
        }
    }
};

const std::vector<std::string> RegexCache::common_patterns_ = {
    R"(^\d+$)", R"(^\w+$)", R"(^\d+)", R"(^\w+)", R"(^\s+)",
    R"(^.*)", R"(^$)", R"(^\b\w+\b)", R"(\d+)", R"(\w+)",
    R"(\s+)", R"(.*)", R"(\b\w+\b)", R"([a-zA-Z]+)", R"([0-9]+)"
};

static RegexCache global_cache;

extern "C" {
    EXPORT_API bool llvm_regex_match_impl(const char* str, size_t len, const char* pattern) {
        if (!str || !pattern) return false;
        try {
            return global_cache.match(std::string(str, len), pattern);
        } catch (...) {
            return false;
        }
    }

    EXPORT_API bool llvm_regex_search_impl(const char* str, size_t len, const char* pattern) {
        if (!str || !pattern) return false;
        try {
            return global_cache.search(std::string(str, len), pattern);
        } catch (...) {
            return false;
        }
    }

    EXPORT_API void* llvm_regex_compile_ex(const char* pattern, int flags, bool optimize_simple) {
        try {
            if (!pattern) return nullptr;

            auto regex_flags = std::regex_constants::ECMAScript;

            if (flags & REGEX_IGNORECASE)
                regex_flags |= std::regex_constants::icase;
            if (flags & REGEX_MULTILINE)
                regex_flags |= std::regex_constants::ECMAScript; // multiline по умолчанию в ECMAScript
            if (flags & REGEX_DOTALL)
                regex_flags |= std::regex_constants::ECMAScript; // dotall по умолчанию в ECMAScript
            if (flags & REGEX_OPTIMIZE || optimize_simple)
                regex_flags |= std::regex_constants::optimize;

            auto* compiled = new std::regex(pattern, regex_flags);
            return static_cast<void*>(compiled);
        } catch (...) {
            return nullptr;
        }
    }

    EXPORT_API bool llvm_regex_exec(void* compiled_regex, const char* str, size_t len, bool full_match) {
        if (!compiled_regex || !str) return false;

        try {
            auto& regex = *static_cast<std::regex*>(compiled_regex);
            const std::string s(str, len);

            if (full_match) {
                return std::regex_match(s, regex);
            } else {
                return std::regex_search(s, regex);
            }
        } catch (...) {
            return false;
        }
    }

    EXPORT_API void llvm_regex_free(void* compiled_regex) {
        if (compiled_regex) {
            delete static_cast<std::regex*>(compiled_regex);
        }
    }

    EXPORT_API void llvm_regex_clear_cache_full(bool clear_jit_cache) {
        (void)clear_jit_cache;
        global_cache.clear();
    }

    EXPORT_API size_t llvm_regex_get_total_cache_size() {
        return global_cache.size();
    }

    EXPORT_API void llvm_regex_set_cache_limits(size_t max_entries, size_t max_memory_mb) {
        (void)max_entries;
        (void)max_memory_mb;
    }

    EXPORT_API bool llvm_regex_simd_search(const char* str, size_t len,
                                         const char* pattern, int simd_mode) {
        (void)simd_mode;
        if (!str || !pattern) return false;
        return SIMDRegex::search(str, len, pattern);
    }

    EXPORT_API bool llvm_regex_validate_pattern(const char* pattern, char* error_buf, size_t error_len) {
        if (!pattern) return false;

        try {
            std::regex tmp(pattern);
            return true;
        } catch (const std::regex_error& e) {
            if (error_buf && error_len > 0) {
                const std::string msg = e.what();
                const size_t copy_len = std::min(msg.size(), error_len - 1);
                strncpy(error_buf, msg.c_str(), copy_len);
                error_buf[copy_len] = '\0';
            }
            return false;
        }
    }

    EXPORT_API size_t llvm_regex_get_stats(int stat_type) {
        switch (stat_type) {
            case 0: return global_cache.miss_count();
            case 1: return global_cache.size();
            default: return static_cast<size_t>(-1);
        }
    }
}