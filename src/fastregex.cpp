#include "fastregex.h"
#include "regex_impl.h"
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

#if defined(__SSE4_2__) || defined(__AVX2__)
#include <immintrin.h>
#endif

namespace fastregex {

// 1. Потокобезопасный кэш скомпилированных регулярных выражений
class RegexCache {
private:
    struct CacheEntry {
        void* compiled_regex;
        std::chrono::steady_clock::time_point last_used;
        size_t hit_count;
        bool jit_compiled;

        CacheEntry(void* regex, bool jit)
            : compiled_regex(regex),
              last_used(std::chrono::steady_clock::now()),
              hit_count(0),
              jit_compiled(jit) {}
    };

    std::unordered_map<std::string, CacheEntry> cache_;
    size_t max_size_;
    mutable std::mutex mutex_;
    std::atomic<size_t> total_requests_{0};
    std::atomic<size_t> cache_hits_{0};

    static void free_compiled(void* ptr) {
        if (ptr) llvm_regex_free(ptr);
    }

public:
    explicit RegexCache(size_t max_size = 100) : max_size_(max_size) {}

    // Удаляем операторы копирования/присваивания
    RegexCache(const RegexCache&) = delete;
    RegexCache& operator=(const RegexCache&) = delete;

    std::pair<void*, bool> get(const std::string& pattern) {
        std::lock_guard<std::mutex> lock(mutex_);
        total_requests_++;

        auto it = cache_.find(pattern);
        if (it == cache_.end()) {
            return {nullptr, false};
        }

        cache_hits_++;
        it->second.last_used = std::chrono::steady_clock::now();
        it->second.hit_count++;
        return {it->second.compiled_regex, it->second.jit_compiled};
    }

    void put(const std::string& pattern, void* compiled, bool jit_compiled) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (cache_.find(pattern) != cache_.end()) {
            return;
        }

        if (cache_.size() >= max_size_) {
            auto oldest = std::min_element(
                cache_.begin(), cache_.end(),
                [](const auto& a, const auto& b) {
                    return a.second.last_used < b.second.last_used;
                });
            free_compiled(oldest->second.compiled_regex);
            cache_.erase(oldest);
        }

        cache_.emplace(pattern, CacheEntry(compiled, jit_compiled));
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& entry : cache_) {
            free_compiled(entry.second.compiled_regex);
        }
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
            free_compiled(oldest->second.compiled_regex);
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

    // Ультра-оптимизированная проверка цифр
    bool check_digits_simd(const char* str, size_t len) noexcept {
#if defined(__SSE4_2__)
        const __m128i zero = _mm_set1_epi8('0');
        const __m128i nine = _mm_set1_epi8('9');
        const size_t step = 16;
        const size_t chunks = len / step;

        for (size_t i = 0; i < chunks; ++i) {
            __m128i chunk = _mm_loadu_si128(
                reinterpret_cast<const __m128i*>(str + i * step));
            __m128i gt = _mm_cmpgt_epi8(chunk, nine);
            __m128i lt = _mm_cmplt_epi8(chunk, zero);
            if (_mm_movemask_epi8(_mm_or_si128(gt, lt)) != 0xFFFF) {
                return true;
            }
        }

        // Остаток
        str += chunks * step;
        len -= chunks * step;
#endif
        // Скалярная версия
        for (size_t i = 0; i < len; ++i) {
            if (str[i] >= '0' && str[i] <= '9') {
                return true;
            }
        }
        return false;
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

    // Обработка больших данных (10MB+)
    bool process_large_data(const char* str, size_t len, const std::string& pattern, RegexFlags flags) {
        try {
            std::regex_constants::syntax_option_type std_flags =
                static_cast<std::regex_constants::syntax_option_type>(flags);

            std::regex re(pattern, std_flags);
            const size_t chunk_size = 1 * 1024 * 1024; // 1MB chunks

            for (size_t offset = 0; offset < len; offset += chunk_size) {
                size_t actual_chunk = std::min(chunk_size, len - offset);
                if (std::regex_search(str + offset, str + offset + actual_chunk, re)) {
                    return true;
                }
            }
            return false;
        } catch (const std::regex_error& e) {
            std::cerr << "Regex error: " << e.what() << " (code: " << e.code() << ")\n";
            return SIMDRegex::search(str, len, pattern);
        } catch (...) {
            return SIMDRegex::search(str, len, pattern);
        }
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

    auto [cached_regex, jit_compiled] = regex_cache.get(pattern_);
    if (cached_regex) {
        impl_->compiled_regex = cached_regex;
        impl_->jit_compiled = jit_compiled;
    } else {
        auto start = std::chrono::high_resolution_clock::now();

        impl_->compiled_regex = llvm_regex_compile_ex(
            pattern_.c_str(),
            static_cast<int>(flags_),
            is_simple_literal(pattern_));

        impl_->jit_compiled = (flags_ & RegexFlags::OPTIMIZE) != RegexFlags::NONE &&
                             SIMDRegex::any_simd_supported() &&
                             SIMDRegex::is_literal_pattern(pattern_);

        auto end = std::chrono::high_resolution_clock::now();
        impl_->compile_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        regex_cache.put(pattern_, impl_->compiled_regex, impl_->jit_compiled);
    }
}

FastRegex::~FastRegex() = default;

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
    return impl_->compiled_regex ?
           llvm_regex_exec(impl_->compiled_regex, str, len, true) :
           false;
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
        return check_digits_simd(str, len);
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

    // Обработка больших данных (>10MB)
    if (len > 10 * 1024 * 1024) {
        return process_large_data(str, len, pattern_, flags_);
    }

    // Использование SIMD для средних данных (если включено и поддерживается)
    if (use_simd_ && len >= SIMDRegex::MIN_SIMD_LENGTH && len < 10 * 1024 * 1024) {
        return SIMDRegex::search(str, len, pattern_);
    }

    // Общий случай
    return impl_->compiled_regex ?
           llvm_regex_exec(impl_->compiled_regex, str, len, false) :
           false;
}

std::vector<std::string> FastRegex::find_all(const std::string& s) const {
    std::vector<std::string> results;
    if (!impl_->compiled_regex) return results;

    const char* str = s.c_str();
    size_t len = s.size();
    size_t pos = 0;

    while (pos < len) {
        if (impl_->compiled_regex && llvm_regex_exec(impl_->compiled_regex, str + pos, len - pos, false)) {
            // Упрощенная версия без точного определения длины совпадения
            results.emplace_back(str + pos);
            break;
        }
        pos++;
    }

    return results;
}

std::string FastRegex::replace(const std::string& s, const std::string& replacement) const {
    if (!impl_->compiled_regex) return s;

    std::string result = s;
    size_t pos = 0;
    while (pos < result.size()) {
        if (search(result.c_str() + pos, result.size() - pos)) {
            // Упрощенная версия без точного определения длины совпадения
            result.replace(pos, pattern_.size(), replacement);
            pos += replacement.size();
        } else {
            pos++;
        }
    }

    return result;
}

// Управление кэшем
void FastRegex::clear_cache() noexcept {
    regex_cache.clear();
    llvm_regex_clear_cache_full(true);
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
    char error_buf[256] = {0};
    return llvm_regex_validate_pattern(pattern.c_str(), error_buf, sizeof(error_buf));
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