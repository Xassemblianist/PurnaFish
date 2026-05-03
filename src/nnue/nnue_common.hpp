/*
 * PurnaFish Chess Engine
 * nnue/nnue_common.hpp — NNUE common types and constants
 */

#pragma once

#include "../types.hpp"
#include <cstdint>
#include <algorithm>

namespace PurnaFish::NNUE {

// Network architecture constants
// HalfKP-like: King position × Piece-Square features
constexpr int NUM_KING_SQUARES   = 64;
constexpr int NUM_PIECE_TYPES    = 10; // 5 piece types × 2 colors (excluding kings)
constexpr int NUM_SQUARES        = 64;
constexpr int HALF_DIMENSIONS    = NUM_KING_SQUARES * NUM_PIECE_TYPES * NUM_SQUARES; // 40960

// Network layer sizes
constexpr int FT_OUT_DIMS = 512;   // Feature transformer output (per side)
constexpr int L1_SIZE     = 16;    // Hidden layer 1
constexpr int L2_SIZE     = 32;    // Hidden layer 2
constexpr int OUTPUT_SIZE = 1;     // Final output

// Quantization constants
constexpr int FT_QUANT  = 255;     // Feature transformer quantization
constexpr int NET_QUANT = 64;      // Network output quantization

// Activation
inline int clipped_relu(int x) {
    return std::clamp(x, 0, FT_QUANT);
}

inline int squared_clipped_relu(int x) {
    int c = std::clamp(x, 0, FT_QUANT);
    return (c * c) >> 8;
}

} // namespace PurnaFish::NNUE
