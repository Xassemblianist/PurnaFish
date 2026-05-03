/*
 * PurnaFish Chess Engine
 * misc.hpp — Utility functions and helpers
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <cassert>

namespace PurnaFish {

// ──────────────────────────────────────────────
// Time utilities
// ──────────────────────────────────────────────

using TimePoint = std::chrono::milliseconds::rep;

inline TimePoint now() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

// ──────────────────────────────────────────────
// String utilities
// ──────────────────────────────────────────────

inline std::vector<std::string> split(const std::string& s, char delimiter = ' ') {
    std::vector<std::string> tokens;
    std::stringstream ss(s);
    std::string token;
    while (std::getline(ss, token, delimiter)) {
        if (!token.empty())
            tokens.push_back(token);
    }
    return tokens;
}

inline bool starts_with(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

// ──────────────────────────────────────────────
// Prefetch
// ──────────────────────────────────────────────

inline void prefetch(const void* addr) {
#if defined(__GNUC__) || defined(__clang__)
    __builtin_prefetch(addr);
#elif defined(_MSC_VER)
    _mm_prefetch(reinterpret_cast<const char*>(addr), _MM_HINT_T0);
#endif
}

// ──────────────────────────────────────────────
// Aligned memory allocation
// ──────────────────────────────────────────────

inline void* aligned_alloc_wrapper(size_t alignment, size_t size) {
#if defined(_MSC_VER)
    return _aligned_malloc(size, alignment);
#else
    void* ptr = nullptr;
    posix_memalign(&ptr, alignment, size);
    return ptr;
#endif
}

inline void aligned_free(void* ptr) {
#if defined(_MSC_VER)
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

// ──────────────────────────────────────────────
// PRNG (for Zobrist key generation and magic finding)
// ──────────────────────────────────────────────

class PRNG {
    uint64_t s;

    uint64_t rand64() {
        s ^= s >> 12;
        s ^= s << 25;
        s ^= s >> 27;
        return s * 2685821657736338717ULL;
    }

public:
    explicit PRNG(uint64_t seed) : s(seed) { assert(seed); }

    template<typename T>
    T rand() { return T(rand64()); }

    /// Generate sparse random number (few bits set) — good for magic numbers
    template<typename T>
    T sparse_rand() {
        return T(rand64() & rand64() & rand64());
    }
};

// ──────────────────────────────────────────────
// Engine info
// ──────────────────────────────────────────────

inline std::string engine_info() {
    return "PurnaFish 1.0.0 by PurnaFish Authors";
}

// ──────────────────────────────────────────────
// Debugging
// ──────────────────────────────────────────────

inline void dbg_print(const std::string& msg) {
#ifndef NDEBUG
    std::cerr << "[DEBUG] " << msg << std::endl;
#endif
    (void)msg;
}

} // namespace PurnaFish
