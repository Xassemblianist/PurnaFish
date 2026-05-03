/*
 * PurnaFish Chess Engine
 * nnue/nnue_accumulator.hpp — Efficiently updatable accumulator
 */

#pragma once

#include "nnue_common.hpp"
#include <cstring>

namespace PurnaFish::NNUE {

struct Accumulator {
    alignas(64) int16_t accumulation[2][FT_OUT_DIMS]; // [perspective][dim]
    bool computed[2] = {false, false};

    void clear() {
        std::memset(accumulation, 0, sizeof(accumulation));
        computed[0] = computed[1] = false;
    }
};

// Feature index computation for HalfKP-like architecture
inline int make_index(Color perspective, Square king_sq, Square piece_sq,
                       Piece piece, Color piece_color) {
    // Map piece to index: (piece_type - 1) + (piece_color == perspective ? 0 : 5)
    int pt = type_of(piece) - 1; // 0-4 for PAWN..QUEEN
    int pc = (piece_color == perspective) ? 0 : 5;
    int piece_idx = pt + pc;

    // For black perspective, flip squares
    if (perspective == BLACK) {
        king_sq = Square(king_sq ^ 56);
        piece_sq = Square(piece_sq ^ 56);
    }

    return king_sq * NUM_PIECE_TYPES * NUM_SQUARES
         + piece_idx * NUM_SQUARES
         + piece_sq;
}

} // namespace PurnaFish::NNUE
