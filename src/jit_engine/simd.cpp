#include "simd.h"
#include <immintrin.h>
#include <cstring>
#include <cstdint>
#include <algorithm>
#include <array>
#include <vector>

#if defined(_MSC_VER)
#include <intrin.h>
#elif defined(__GNUC__) && !defined(__clang__)
#include <cpuid.h>
#ifndef __cpuid
#define __cpuid(level, a, b, c, d) __cpuid_count(level, 0, a, b, c, d)
#endif
#ifndef __cpuidex
#define __cpuidex(level, count, a, b, c, d) __cpuid_count(level, count, a, b, c, d)
#endif
#endif

#if defined(__ARM_NEON__) || defined(__aarch64__)
#include <sys/auxv.h>
#include <arm_neon.h>
#define HWCAP_NEON (1 << 12)
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

// Реализации функций проверки CPU
bool avx512_supported() noexcept {
#if defined(__AVX512F__)
    unsigned regs[4] = {0};
    __cpuid_count(0, 0, regs[0], regs[1], regs[2], regs[3]);
    if (regs[0] < 7) return false;

    __cpuid_count(7, 0, regs[0], regs[1], regs[2], regs[3]);
    return (regs[1] & (1 << 16)) &&  // AVX512F
           (regs[1] & (1 << 17)) &&  // AVX512DQ
           (regs[1] & (1 << 30));    // AVX512BW
#else
    return false;
#endif
}

bool avx2_supported() noexcept {
#if defined(__AVX2__)
    unsigned regs[4] = {0};
    __cpuid(1, regs[0], regs[1], regs[2], regs[3]);
    if (!(regs[2] & (1 << 27))) return false;  // OSXSAVE

    unsigned xcr0 = _xgetbv(0);
    if (!(xcr0 & 0x6)) return false;  // SSE and AVX states

    __cpuid_count(7, 0, regs[0], regs[1], regs[2], regs[3]);
    return (regs[1] & (1 << 5)) != 0;
#else
    return false;
#endif
}

bool sse42_supported() noexcept {
#if defined(__SSE4_2__)
    unsigned regs[4] = {0};
    __cpuid(1, regs[0], regs[1], regs[2], regs[3]);
    return (regs[2] & (1 << 20)) != 0;
#else
    return false;
#endif
}

bool neon_supported() noexcept {
#if defined(__ARM_NEON__) || defined(__aarch64__)
    unsigned long hwcaps = getauxval(AT_HWCAP);
    return (hwcaps & HWCAP_NEON) != 0;
#else
    return false;
#endif
}

bool any_simd_supported() noexcept {
    return avx512_supported() || avx2_supported() ||
           sse42_supported() || neon_supported();
}

bool is_literal_pattern(std::string_view pattern) noexcept {
    for (char c : pattern) {
        if (is_special_char(c)) return false;
    }
    return true;
}

// Основные функции поиска
bool search(const char* str, size_t len, std::string_view pattern) noexcept {
    if (len == 0 || pattern.empty() || len < pattern.size()) {
        return false;
    }

    global_stats.total_calls++;

    switch (global_simd_mode) {
        case SIMDMode::ForceAVX512:
            if (avx512_supported()) return avx512_search(str, len, pattern);
            break;
        case SIMDMode::ForceAVX2:
            if (avx2_supported()) return avx2_search(str, len, pattern);
            break;
        case SIMDMode::ForceSSE42:
            if (sse42_supported()) return sse42_search(str, len, pattern);
            break;
        case SIMDMode::ForceNEON:
            if (neon_supported()) return neon_search(str, len, pattern);
            break;
        case SIMDMode::ScalarOnly:
            return optimized_scalar_search(str, len, pattern);
        case SIMDMode::Auto:
        default:
            break;
    }

#if defined(__AVX512F__)
    if (avx512_supported() && len >= 512) {
        global_stats.avx512_count++;
        return avx512_search(str, len, pattern);
    }
#endif
#if defined(__AVX2__)
    if (avx2_supported() && len >= 256) {
        global_stats.avx2_count++;
        return avx2_search(str, len, pattern);
    }
#endif
#if defined(__SSE4_2__)
    if (sse42_supported() && len >= 128) {
        global_stats.sse42_count++;
        return sse42_search(str, len, pattern);
    }
#endif
#if defined(__ARM_NEON__)
    if (neon_supported() && len >= 64) {
        global_stats.neon_count++;
        return neon_search(str, len, pattern);
    }
#endif

    global_stats.scalar_count++;
    return optimized_scalar_search(str, len, pattern);
}

bool search_literal(const char* str, size_t len, std::string_view pattern) noexcept {
    return search(str, len, pattern);
}

bool search_word_boundary(const char* str, size_t len) noexcept {
    for (size_t i = 0; i < len; ++i) {
        bool prev = (i > 0) ? is_word_char(str[i-1]) : false;
        bool curr = is_word_char(str[i]);
        if (prev != curr) return true;
    }
    return false;
}

// Архитектурно-специфичные реализации
bool avx512_search(const char* str, size_t len, std::string_view pattern) noexcept {
#if defined(__AVX512F__) && defined(__AVX512BW__)
    if (pattern.empty()) return true;
    if (len < pattern.size()) return false;

    const size_t pattern_len = pattern.size();
    const char* pattern_data = pattern.data();

    if (pattern_len == 1) {
        const __m512i pattern_vec = _mm512_set1_epi8(pattern_data[0]);
        const size_t blocks = len / 64;
        const size_t remainder = len % 64;

        for (size_t i = 0; i < blocks; ++i) {
            const __m512i text = _mm512_loadu_si512(str + i * 64);
            __mmask64 mask = _mm512_cmpeq_epi8_mask(text, pattern_vec);
            if (mask != 0) return true;
        }

        if (remainder > 0) {
            const __m512i text = _mm512_maskz_loadu_epi8(
                (1ULL << remainder) - 1, str + blocks * 64);
            __mmask64 mask = _mm512_cmpeq_epi8_mask(text, pattern_vec);
            if (mask != 0) return true;
        }
        return false;
    }

    if (pattern_len <= 64) {
        alignas(64) char buf[64] = {0};
        std::memcpy(buf, pattern_data, pattern_len);
        const __m512i pattern_vec = _mm512_load_si512(buf);

        const size_t search_len = len - pattern_len + 1;
        const size_t blocks = search_len / 64;
        const size_t remainder = search_len % 64;

        for (size_t i = 0; i < blocks; ++i) {
            const __m512i text = _mm512_loadu_si512(str + i * 64);
            __mmask64 mask = _mm512_cmpeq_epi8_mask(text, pattern_vec);

            while (mask != 0) {
                const unsigned j = _tzcnt_u64(mask);
                if (std::memcmp(str + i * 64 + j, pattern_data, pattern_len) == 0) {
                    return true;
                }
                mask = _blsr_u64(mask);
            }
        }

        if (remainder > 0) {
            const __m512i text = _mm512_maskz_loadu_epi8(
                (1ULL << remainder) - 1, str + blocks * 64);
            __mmask64 mask = _mm512_cmpeq_epi8_mask(text, pattern_vec);

            while (mask != 0) {
                const unsigned j = _tzcnt_u64(mask);
                if (std::memcmp(str + blocks * 64 + j, pattern_data, pattern_len) == 0) {
                    return true;
                }
                mask = _blsr_u64(mask);
            }
        }
    }
#endif
    return avx2_search(str, len, pattern);
}

bool avx2_search(const char* str, size_t len, std::string_view pattern) noexcept {
#if defined(__AVX2__)
    if (pattern.empty()) return true;
    if (len < pattern.size()) return false;

    const size_t pattern_len = pattern.size();
    const char* pattern_data = pattern.data();

    if (pattern_len == 1) {
        const __m256i pattern_vec = _mm256_set1_epi8(pattern_data[0]);
        const size_t blocks = len / 32;
        const size_t remainder = len % 32;

        for (size_t i = 0; i < blocks; ++i) {
            const __m256i text = _mm256_loadu_si256(
                reinterpret_cast<const __m256i*>(str + i * 32));
            __m256i cmp = _mm256_cmpeq_epi8(text, pattern_vec);
            if (_mm256_movemask_epi8(cmp) != 0) {
                return true;
            }
        }

        if (remainder > 0) {
            __m256i text = _mm256_loadu_si256(
                reinterpret_cast<const __m256i*>(str + blocks * 32));
            __m256i cmp = _mm256_cmpeq_epi8(text, pattern_vec);
            if (_mm256_movemask_epi8(cmp) != 0) {
                return true;
            }
        }
        return false;
    }

    if (pattern_len <= 32) {
        alignas(32) char buf[32] = {0};
        std::memcpy(buf, pattern_data, pattern_len);
        const __m256i pattern_vec = _mm256_load_si256(
            reinterpret_cast<const __m256i*>(buf));

        const size_t search_len = len - pattern_len + 1;
        const size_t blocks = search_len / 32;
        const size_t remainder = search_len % 32;

        for (size_t i = 0; i < blocks; ++i) {
            const __m256i text = _mm256_loadu_si256(
                reinterpret_cast<const __m256i*>(str + i * 32));
            __m256i cmp = _mm256_cmpeq_epi8(text, pattern_vec);
            unsigned mask = _mm256_movemask_epi8(cmp);

            while (mask != 0) {
                unsigned j = __builtin_ctz(mask);
                if (std::memcmp(str + i * 32 + j, pattern_data, pattern_len) == 0) {
                    return true;
                }
                mask &= mask - 1;
            }
        }

        if (remainder > 0) {
            const __m256i text = _mm256_loadu_si256(
                reinterpret_cast<const __m256i*>(str + blocks * 32));
            __m256i cmp = _mm256_cmpeq_epi8(text, pattern_vec);
            unsigned mask = _mm256_movemask_epi8(cmp);

            while (mask != 0) {
                unsigned j = __builtin_ctz(mask);
                if (std::memcmp(str + blocks * 32 + j, pattern_data, pattern_len) == 0) {
                    return true;
                }
                mask &= mask - 1;
            }
        }
    }
#endif
    return sse42_search(str, len, pattern);
}

bool sse42_search(const char* str, size_t len, std::string_view pattern) noexcept {
#if defined(__SSE4_2__)
    if (pattern.empty()) return true;
    if (len < pattern.size()) return false;

    const size_t pattern_len = pattern.size();
    const char* pattern_data = pattern.data();

    if (pattern_len == 1) {
        return memchr(str, pattern_data[0], len) != nullptr;
    }

    if (pattern_len <= 16) {
        __m128i pattern_vec = _mm_loadu_si128(
            reinterpret_cast<const __m128i*>(pattern_data));

        const size_t search_len = len - pattern_len + 1;
        for (size_t i = 0; i < search_len; ++i) {
            __m128i text = _mm_loadu_si128(
                reinterpret_cast<const __m128i*>(str + i));
            __m128i cmp = _mm_cmpeq_epi8(text, pattern_vec);
            if (_mm_movemask_epi8(cmp) == 0xFFFF) {
                if (std::memcmp(str + i, pattern_data, pattern_len) == 0) {
                    return true;
                }
            }
        }
    }
#endif
    return optimized_scalar_search(str, len, pattern);
}

bool neon_search(const char* str, size_t len, std::string_view pattern) noexcept {
#if defined(__ARM_NEON__) || defined(__aarch64__)
    if (pattern.empty()) return true;
    if (len < pattern.size()) return false;

    const size_t pattern_len = pattern.size();
    const char* pattern_data = pattern.data();

    if (pattern_len == 1) {
        return memchr(str, pattern_data[0], len) != nullptr;
    }

    if (pattern_len <= 16) {
        uint8x16_t pattern_vec = vld1q_u8(
            reinterpret_cast<const uint8_t*>(pattern_data));

        const size_t search_len = len - pattern_len + 1;
        for (size_t i = 0; i < search_len; ++i) {
            uint8x16_t text = vld1q_u8(
                reinterpret_cast<const uint8_t*>(str + i));
            uint8x16_t cmp = vceqq_u8(text, pattern_vec);
            if (vminvq_u8(cmp) == 0xFF) {
                if (std::memcmp(str + i, pattern_data, pattern_len) == 0) {
                    return true;
                }
            }
        }
    }
#endif
    return optimized_scalar_search(str, len, pattern);
}

// Скалярные реализации
bool naive_search(const char* str, size_t len, std::string_view pattern) noexcept {
    if (pattern.empty()) return true;
    if (len < pattern.size()) return false;

    for (size_t i = 0; i <= len - pattern.size(); ++i) {
        if (std::memcmp(str + i, pattern.data(), pattern.size()) == 0) {
            return true;
        }
    }
    return false;
}

bool optimized_scalar_search(const char* str, size_t len, std::string_view pattern) noexcept {
    if (pattern.empty()) return true;
    if (len < pattern.size()) return false;

    if (pattern.size() == 1) {
        return memchr(str, pattern[0], len) != nullptr;
    }

    if (len < 256) {
        const char* pat = pattern.data();
        const size_t pat_len = pattern.size();
        const char* end = str + len - pat_len;

        size_t shift[256];
        for (size_t i = 0; i < 256; ++i) {
            shift[i] = pat_len + 1;
        }
        for (size_t i = 0; i < pat_len; ++i) {
            shift[static_cast<uint8_t>(pat[i])] = pat_len - i;
        }

        for (const char* p = str; p <= end; ) {
            if (std::memcmp(p, pat, pat_len) == 0) {
                return true;
            }

            if (p + pat_len >= str + len) break;
            p += shift[static_cast<uint8_t>(p[pat_len])];
        }
        return false;
    }

    return naive_search(str, len, pattern);
}

// Управление режимами
void set_simd_mode(SIMDMode mode) noexcept {
    global_simd_mode = mode;
}

SIMDMode get_simd_mode() noexcept {
    return global_simd_mode;
}

// Мониторинг производительности
SIMDStats& get_simd_stats() noexcept {
    return global_stats;
}

void reset_simd_stats() noexcept {
    global_stats.total_calls.store(0);
    global_stats.avx512_count.store(0);
    global_stats.avx2_count.store(0);
    global_stats.sse42_count.store(0);
    global_stats.neon_count.store(0);
    global_stats.scalar_count.store(0);
}

float get_simd_utilization() noexcept {
    const size_t total = global_stats.total_calls.load();
    if (total == 0) return 0.0f;

    const size_t simd_used = global_stats.avx512_count.load() +
                           global_stats.avx2_count.load() +
                           global_stats.sse42_count.load() +
                           global_stats.neon_count.load();
    return static_cast<float>(simd_used) / total * 100.0f;
}

} // namespace SIMDRegex