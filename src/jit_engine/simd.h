// simd.h
#pragma once

#include <string_view>
#include <array>
#include <cstddef>
#include <algorithm>
#include <atomic>
#include <vector>
#include <cstring>

// Platform-specific intrinsics
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

#if defined(__AVX512F__) || defined(__AVX2__) || defined(__SSE4_2__)
#include <immintrin.h>
#elif defined(__SSE4_2__)
#include <nmmintrin.h>
#endif

#if defined(__ARM_NEON__) || defined(__aarch64__)
#include <arm_neon.h>
#endif

namespace SIMDRegex {

// Configuration constants
constexpr size_t MIN_SIMD_LENGTH = 64;       // Minimum data size for SIMD optimizations
constexpr size_t OPTIMAL_SIMD_LENGTH = 1024; // Optimal chunk size for processing
constexpr size_t MAX_PATTERN_LENGTH = 128;   // Maximum pattern length for SIMD
constexpr size_t SIMD_ALIGNMENT = 64;        // Memory alignment requirement

// CPU feature detection
bool avx512_supported() noexcept;
bool avx2_supported() noexcept;
bool sse42_supported() noexcept;
bool neon_supported() noexcept;
bool any_simd_supported() noexcept;

// Pattern classification utilities
namespace detail {
    constexpr std::array<char, 14> SPECIAL_REGEX_CHARS = {
        '^', '$', '.', '*', '+', '?', '|', '\\',
        '[', ']', '(', ')', '{', '}'
    };

    constexpr bool is_special_char(char c) noexcept {
        for (char sc : SPECIAL_REGEX_CHARS) {
            if (c == sc) return true;
        }
        return false;
    }

    constexpr bool is_word_char(char c) noexcept {
        return (c >= 'a' && c <= 'z') ||
               (c >= 'A' && c <= 'Z') ||
               (c >= '0' && c <= '9') ||
               c == '_';
    }
}

// Pattern analysis
bool is_literal_pattern(std::string_view pattern) noexcept;
bool is_trivial_pattern(std::string_view pattern) noexcept;

// Core search functions
bool search(const char* str, size_t len, std::string_view pattern) noexcept;
bool search_literal(const char* str, size_t len, std::string_view pattern) noexcept;
bool search_word_boundary(const char* str, size_t len) noexcept;

// Architecture-specific implementations
bool avx512_search(const char* str, size_t len, std::string_view pattern) noexcept;
bool avx2_search(const char* str, size_t len, std::string_view pattern) noexcept;
bool sse42_search(const char* str, size_t len, std::string_view pattern) noexcept;
bool neon_search(const char* str, size_t len, std::string_view pattern) noexcept;

// Scalar fallback implementations
bool naive_search(const char* str, size_t len, std::string_view pattern) noexcept;
bool optimized_scalar_search(const char* str, size_t len, std::string_view pattern) noexcept;

// SIMD control mode
enum class SIMDMode {
    Auto,         // Automatic detection (default)
    ForceAVX512,  // Force AVX-512 if available
    ForceAVX2,    // Force AVX2 if available
    ForceSSE42,   // Force SSE4.2 if available
    ForceNEON,    // Force NEON if available
    ScalarOnly    // Disable all SIMD optimizations
};

// SIMD mode management
void set_simd_mode(SIMDMode mode) noexcept;
SIMDMode get_simd_mode() noexcept;

// Performance monitoring structure
struct SIMDStats {
    std::atomic<size_t> total_calls{0};
    std::atomic<size_t> avx512_count{0};
    std::atomic<size_t> avx2_count{0};
    std::atomic<size_t> sse42_count{0};
    std::atomic<size_t> neon_count{0};
    std::atomic<size_t> scalar_count{0};

    // Delete copy constructor/assignment
    SIMDStats() = default;
    SIMDStats(const SIMDStats&) = delete;
    SIMDStats& operator=(const SIMDStats&) = delete;

    // Allow move operations
    SIMDStats(SIMDStats&&) = default;
    SIMDStats& operator=(SIMDStats&&) = default;

    void reset() noexcept {
        total_calls.store(0);
        avx512_count.store(0);
        avx2_count.store(0);
        sse42_count.store(0);
        neon_count.store(0);
        scalar_count.store(0);
    }
};

// Performance monitoring interface
SIMDStats& get_simd_stats() noexcept;
void reset_simd_stats() noexcept;
float get_simd_utilization() noexcept;

// Advanced pattern matching
bool match_multiple_patterns(
    const char* str,
    size_t len,
    const std::vector<std::string_view>& patterns
) noexcept;

} // namespace SIMDRegex