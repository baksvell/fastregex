#include "simd.h"
#include <cstring>
#include <cstdint>
#include <algorithm>
#include <array>
#include <vector>
#include <string>
#include <string_view>

#ifdef _MSC_VER
#include <intrin.h>
#else
#include <immintrin.h>
#endif

namespace SIMDRegex {

namespace {
    constexpr std::array<char, 14> SPECIAL_CHARS = {
        '^', '$', '.', '*', '+', '?', '|', '\\', '[', ']', '(', ')', '{', '}'
    };

    inline bool is_special_char(char c) noexcept {
        return std::find(SPECIAL_CHARS.begin(), SPECIAL_CHARS.end(), c) != SPECIAL_CHARS.end();
    }

    inline bool is_word_char(char c) noexcept {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
               (c >= '0' && c <= '9') || c == '_';
    }
}

// Инициализация статических членов
SIMDMode global_simd_mode = SIMDMode::Auto;
SIMDStats global_stats{};

// Реализации функций проверки CPU (упрощенные для совместимости)
bool avx512_supported() noexcept {
#ifdef _MSC_VER
    int regs[4];
    __cpuid(regs, 0);
    if (regs[0] < 7) return false;
    
    __cpuidex(regs, 7, 0);
    return (regs[1] & (1 << 16)) &&  // AVX512F
           (regs[1] & (1 << 17)) &&  // AVX512DQ
           (regs[1] & (1 << 30));    // AVX512BW
#else
    return false;
#endif
}

bool avx2_supported() noexcept {
#ifdef _MSC_VER
    int regs[4];
    __cpuid(regs, 1);
    if (!(regs[2] & (1 << 27))) return false;  // OSXSAVE
    
    unsigned long long xcr0 = _xgetbv(0);
    if (!(xcr0 & 0x6)) return false;  // SSE and AVX states
    
    __cpuidex(regs, 7, 0);
    return (regs[1] & (1 << 5)) != 0;
#else
    return false;
#endif
}

bool sse42_supported() noexcept {
#ifdef _MSC_VER
    int regs[4];
    __cpuid(regs, 1);
    return (regs[2] & (1 << 20)) != 0;
#else
    return false;
#endif
}

bool neon_supported() noexcept {
    return false;  // Не поддерживается на Windows
}

bool any_simd_supported() noexcept {
    return avx512_supported() || avx2_supported() || sse42_supported() || neon_supported();
}

// Упрощенные реализации SIMD функций
bool search(const char* str, size_t len, std::string_view pattern) noexcept {
    if (!str || len == 0 || pattern.empty()) return false;
    
    // Простая реализация без SIMD для совместимости
    return std::search(str, str + len, pattern.begin(), pattern.end()) != str + len;
}

bool is_literal_pattern(std::string_view pattern) noexcept {
    return std::none_of(pattern.begin(), pattern.end(), is_special_char);
}

// Управление режимом SIMD
void set_simd_mode(SIMDMode mode) noexcept {
    global_simd_mode = mode;
}

SIMDMode get_simd_mode() noexcept {
    return global_simd_mode;
}

// Статистика
SIMDStats& get_simd_stats() noexcept {
    return global_stats;
}

void reset_simd_stats() noexcept {
    global_stats.total_calls = 0;
    global_stats.avx512_count = 0;
    global_stats.avx2_count = 0;
    global_stats.sse42_count = 0;
    global_stats.neon_count = 0;
    global_stats.scalar_count = 0;
}

} // namespace SIMDRegex
