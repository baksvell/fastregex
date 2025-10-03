#include "fastregex.h"
#include "simd.h"
#include <iostream>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <memory>
#include <stdexcept>
#include <cstring>
#include <regex>
#include <string_view>

namespace fastregex {

// 1. Потокобезопасный кэш скомпилированных регулярных выражений
class RegexCache {
private:
    struct CacheEntry {
        std::regex compiled_regex;
        std::chrono::steady_clock::time_point last_used;
        size_t hit_count;
        bool jit_compiled;

        CacheEntry(const std::string& pattern, std::regex_constants::syntax_option_type flags, bool jit)
            : compiled_regex(pattern, flags),
              last_used(std::chrono::steady_clock::now()),
              hit_count(0),
              jit_compiled(jit) {}
    };

    std::unordered_map<std::string, CacheEntry> cache_;
    size_t max_size_;
    mutable std::mutex mutex_;
    std::atomic<size_t> total_requests_{0};
    std::atomic<size_t> cache_hits_{0};

public:
    explicit RegexCache(size_t max_size = 100) : max_size_(max_size) {}

    // Удаляем операторы копирования/присваивания
    RegexCache(const RegexCache&) = delete;
    RegexCache& operator=(const RegexCache&) = delete;

    std::pair<std::regex*, bool> get(const std::string& pattern, std::regex_constants::syntax_option_type flags) {
        std::lock_guard<std::mutex> lock(mutex_);
        total_requests_++;

        std::string key = pattern + "_" + std::to_string(static_cast<int>(flags));
        auto it = cache_.find(key);
        if (it == cache_.end()) {
            return {nullptr, false};
        }

        cache_hits_++;
        it->second.last_used = std::chrono::steady_clock::now();
        it->second.hit_count++;
        return {&it->second.compiled_regex, it->second.jit_compiled};
    }

    void put(const std::string& pattern, std::regex_constants::syntax_option_type flags, bool jit_compiled) {
        std::lock_guard<std::mutex> lock(mutex_);

        std::string key = pattern + "_" + std::to_string(static_cast<int>(flags));
        if (cache_.find(key) != cache_.end()) {
            return;
        }

        if (cache_.size() >= max_size_) {
            auto oldest = std::min_element(
                cache_.begin(), cache_.end(),
                [](const auto& a, const auto& b) {
                    return a.second.last_used < b.second.last_used;
                });
            cache_.erase(oldest);
        }

        try {
            cache_.emplace(key, CacheEntry(pattern, flags, jit_compiled));
        } catch (const std::regex_error& e) {
            // Игнорируем ошибки компиляции regex
        }
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_.clear();
        total_requests_ = 0;
        cache_hits_ = 0;
    }

    void resize(size_t new_size) {
        std::lock_guard<std::mutex> lock(mutex_);
        max_size_ = new_size;
        while (cache_.size() > max_size_) {
            auto oldest = std::min_element(
                cache_.begin(), cache_.end(),
                [](const auto& a, const auto& b) {
                    return a.second.last_used < b.second.last_used;
                });
            cache_.erase(oldest);
        }
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return cache_.size();
    }

    double hit_rate() const {
        return total_requests_ > 0 ?
               static_cast<double>(cache_hits_) / total_requests_ : 0.0;
    }
};

static RegexCache regex_cache;

// 2. Вспомогательные функции
namespace {
    bool is_simple_literal(const std::string& pattern) noexcept {
        return pattern.find_first_of("^$.*+?|\\[](){}") == std::string::npos;
    }

    // Оптимизированный поиск буквальной строки
    bool search_literal(const char* str, size_t len,
                       const char* pat, size_t pat_len) noexcept {
        if (len < pat_len) return false;

        const char first = pat[0];
        const char* end = str + len - pat_len + 1;

        while ((str = static_cast<const char*>(memchr(str, first, end - str)))) {
            if (memcmp(str, pat, pat_len) == 0) {
                return true;
            }
            str++;
        }
        return false;
    }

    // Быстрая проверка цифр
    bool check_digits(const char* str, size_t len) noexcept {
        for (size_t i = 0; i < len; ++i) {
            if (str[i] >= '0' && str[i] <= '9') {
                return true;
            }
        }
        return false;
    }

    // Быстрая проверка хештегов
    bool check_hashtag(const char* str, size_t len) noexcept {
        if (len < 2 || str[0] != '#') return false;

        for (size_t i = 1; i < len; ++i) {
            char c = str[i];
            if (!((c >= 'a' && c <= 'z') ||
                  (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') ||
                  c == '_')) {
                return false;
            }
        }
        return true;
    }

    // Оптимизация для URL
    bool check_url(const char* str, size_t len) noexcept {
        if (len < 4) return false;

        // Быстрая проверка http:// или https://
        if (len >= 7 && str[0] == 'h' && str[1] == 't' && str[2] == 't' && str[3] == 'p') {
            if (str[4] == ':' && str[5] == '/' && str[6] == '/') return true;
            if (len >= 8 && str[4] == 's' && str[5] == ':' && str[6] == '/' && str[7] == '/') return true;
        }

        // Проверка www.
        return (str[0] == 'w' && str[1] == 'w' && str[2] == 'w' && str[3] == '.');
    }
}

// Реализация методов FastRegex
FastRegex::FastRegex(const std::string& pattern, bool use_simd, bool debug, RegexFlags flags)
    : impl_(std::make_unique<Impl>()),
      pattern_(pattern),
      use_simd_(use_simd),
      debug_mode_(debug),
      flags_(flags) {

    if (pattern.empty()) {
        throw std::invalid_argument("Pattern cannot be empty");
    }

    // Конвертируем флаги в std::regex флаги
    std::regex_constants::syntax_option_type std_flags = std::regex_constants::ECMAScript;
    if (has_flag(flags, RegexFlags::IGNORECASE)) {
        std_flags |= std::regex_constants::icase;
    }

    auto [cached_regex, found] = regex_cache.get(pattern_, std_flags);
    if (found && cached_regex) {
        impl_->compiled_regex = cached_regex;
        impl_->jit_compiled = false;
    } else {
        auto start = std::chrono::high_resolution_clock::now();

        try {
            impl_->compiled_regex = new std::regex(pattern_, std_flags);
            impl_->jit_compiled = false;
            
            // Проверяем, можно ли использовать JIT для этого паттерна
            if (use_simd_ && SIMDRegex::any_simd_supported() && 
                SIMDRegex::is_literal_pattern(std::string_view(pattern_))) {
                impl_->jit_compiled = true;
            }
        } catch (const std::regex_error& e) {
            impl_->compiled_regex = nullptr;
            impl_->jit_compiled = false;
        }

        auto end = std::chrono::high_resolution_clock::now();
        impl_->compile_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        regex_cache.put(pattern_, std_flags, impl_->jit_compiled);
    }
}

FastRegex::~FastRegex() {
    if (impl_->compiled_regex) {
        delete static_cast<std::regex*>(impl_->compiled_regex);
    }
}

FastRegex::FastRegex(FastRegex&&) noexcept = default;
FastRegex& FastRegex::operator=(FastRegex&&) noexcept = default;

bool FastRegex::match(const std::string& s) const {
    return match(s.c_str(), s.size());
}

bool FastRegex::match(const char* str, size_t len) const {
    if (!str) return false;

    // Быстрые проверки для общих случаев
    if (pattern_ == ".*") return true;
    if (pattern_.empty()) return len == 0;
    if (len == 0) return pattern_ == "^$" || pattern_ == ".*";

    // Оптимизация для точного совпадения
    if (pattern_.size() > 1 && pattern_[0] == '^' && pattern_.back() == '$') {
        std::string content = pattern_.substr(1, pattern_.size()-2);
        if (is_simple_literal(content)) {
            return len == content.size() &&
                   memcmp(str, content.c_str(), len) == 0;
        }
    }

    // Общий случай
    if (impl_->compiled_regex) {
        try {
            std::string s(str, len);
            return std::regex_match(s, *static_cast<std::regex*>(impl_->compiled_regex));
        } catch (const std::regex_error& e) {
            return false;
        }
    }
    return false;
}

bool FastRegex::search(const std::string& s) const {
    return search(s.c_str(), s.size());
}

bool FastRegex::search(const char* str, size_t len) const {
    if (!str) return false;
    if (pattern_.empty()) return len > 0;
    if (pattern_ == ".*") return len > 0;
    if (len == 0) return pattern_ == "^$";

    // Специальные оптимизации для часто используемых паттернов
    if (pattern_ == R"(\d+)") {
        return check_digits(str, len);
    }

    if (pattern_ == R"(#\w+)") {
        return check_hashtag(str, len);
    }

    if (pattern_ == R"(https?://\S+|www\.\S+)") {
        return check_url(str, len);
    }

    // Оптимизация для буквальных строк
    if (is_simple_literal(pattern_)) {
        return search_literal(str, len, pattern_.data(), pattern_.size());
    }

    // Использование SIMD для средних данных (если включено и поддерживается)
    if (use_simd_ && len >= SIMDRegex::MIN_SIMD_LENGTH && SIMDRegex::any_simd_supported()) {
        return SIMDRegex::search(str, len, std::string_view(pattern_));
    }

    // Общий случай
    if (impl_->compiled_regex) {
        try {
            std::string s(str, len);
            return std::regex_search(s, *static_cast<std::regex*>(impl_->compiled_regex));
        } catch (const std::regex_error& e) {
            return false;
        }
    }
    return false;
}

std::vector<std::string> FastRegex::find_all(const std::string& s) const {
    std::vector<std::string> results;
    if (!impl_->compiled_regex) return results;

    try {
        std::sregex_iterator iter(s.begin(), s.end(), *static_cast<std::regex*>(impl_->compiled_regex));
        std::sregex_iterator end;

        for (; iter != end; ++iter) {
            results.push_back(iter->str());
        }
    } catch (const std::regex_error& e) {
        // Игнорируем ошибки
    }

    return results;
}

std::string FastRegex::replace(const std::string& s, const std::string& replacement) const {
    if (!impl_->compiled_regex) return s;

    try {
        return std::regex_replace(s, *static_cast<std::regex*>(impl_->compiled_regex), replacement);
    } catch (const std::regex_error& e) {
        return s;
    }
}

// Управление кэшем
void FastRegex::clear_cache() noexcept {
    regex_cache.clear();
}

size_t FastRegex::cache_size() noexcept {
    return regex_cache.size();
}

double FastRegex::hit_rate() noexcept {
    return regex_cache.hit_rate();
}

void FastRegex::set_cache_capacity(size_t max_size) noexcept {
    regex_cache.resize(max_size);
}

// Диагностика
bool FastRegex::is_jit_compiled() const noexcept {
    return impl_->jit_compiled;
}

std::chrono::microseconds FastRegex::compile_time() const noexcept {
    return impl_->compile_time;
}

// Вспомогательные функции
bool is_valid_pattern(const std::string& pattern) noexcept {
    try {
        std::regex re(pattern);
        return true;
    } catch (const std::regex_error& e) {
        return false;
    }
}

std::string escape_special_chars(const std::string& pattern) {
    std::string result;
    result.reserve(pattern.size() * 2);

    for (char c : pattern) {
        if (strchr("^$.*+?|\\[](){}", c)) {
            result += '\\';
        }
        result += c;
    }
    return result;
}

} // namespace fastregex
