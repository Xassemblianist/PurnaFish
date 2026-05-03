/*
 * PurnaFish Chess Engine
 * nnue/nnue_feature_transformer.hpp — Feature transformer layer
 *
 * Implements the first layer of the NNUE network: a sparse input to
 * dense accumulator transformation that can be incrementally updated.
 */

#pragma once

#include "nnue_common.hpp"
#include "nnue_accumulator.hpp"
#include "../position.hpp"

#if defined(USE_AVX2)
#include <immintrin.h>
#endif

namespace PurnaFish::NNUE {

struct FeatureTransformer {
    // Weights: [HALF_DIMENSIONS][FT_OUT_DIMS] — stored in column-major for SIMD
    alignas(64) int16_t weight[HALF_DIMENSIONS * FT_OUT_DIMS];
    alignas(64) int16_t bias[FT_OUT_DIMS];

    /// Full refresh: compute accumulator from scratch
    void refresh(Accumulator& acc, const Position& pos, Color perspective) const {
        // Start with bias
        std::memcpy(acc.accumulation[perspective], bias, sizeof(bias));

        Square ksq = pos.square(perspective, KING);

        // Add active features
        Bitboard occupied = pos.pieces() & ~pos.pieces(KING); // Exclude kings

        while (occupied) {
            Square sq = pop_lsb(occupied);
            Piece pc = pos.piece_on(sq);
            int idx = make_index(perspective, ksq, sq, pc, color_of(pc));

            // Add weight row to accumulator
            const int16_t* w = &weight[idx * FT_OUT_DIMS];

#if defined(USE_AVX2)
            for (int i = 0; i < FT_OUT_DIMS; i += 16) {
                __m256i acc_vec = _mm256_load_si256((__m256i*)&acc.accumulation[perspective][i]);
                __m256i w_vec   = _mm256_load_si256((__m256i*)&w[i]);
                acc_vec = _mm256_add_epi16(acc_vec, w_vec);
                _mm256_store_si256((__m256i*)&acc.accumulation[perspective][i], acc_vec);
            }
#else
            for (int i = 0; i < FT_OUT_DIMS; ++i)
                acc.accumulation[perspective][i] += w[i];
#endif
        }

        acc.computed[perspective] = true;
    }

    /// Incremental update: add/remove features
    void update_add(Accumulator& acc, Color perspective, int featureIdx) const {
        const int16_t* w = &weight[featureIdx * FT_OUT_DIMS];
#if defined(USE_AVX2)
        for (int i = 0; i < FT_OUT_DIMS; i += 16) {
            __m256i a = _mm256_load_si256((__m256i*)&acc.accumulation[perspective][i]);
            __m256i b = _mm256_load_si256((__m256i*)&w[i]);
            _mm256_store_si256((__m256i*)&acc.accumulation[perspective][i],
                              _mm256_add_epi16(a, b));
        }
#else
        for (int i = 0; i < FT_OUT_DIMS; ++i)
            acc.accumulation[perspective][i] += w[i];
#endif
    }

    void update_sub(Accumulator& acc, Color perspective, int featureIdx) const {
        const int16_t* w = &weight[featureIdx * FT_OUT_DIMS];
#if defined(USE_AVX2)
        for (int i = 0; i < FT_OUT_DIMS; i += 16) {
            __m256i a = _mm256_load_si256((__m256i*)&acc.accumulation[perspective][i]);
            __m256i b = _mm256_load_si256((__m256i*)&w[i]);
            _mm256_store_si256((__m256i*)&acc.accumulation[perspective][i],
                              _mm256_sub_epi16(a, b));
        }
#else
        for (int i = 0; i < FT_OUT_DIMS; ++i)
            acc.accumulation[perspective][i] -= w[i];
#endif
    }
};

} // namespace PurnaFish::NNUE
