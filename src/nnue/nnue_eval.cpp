/*
 * PurnaFish Chess Engine
 * nnue/nnue_eval.cpp — NNUE evaluation implementation
 *
 * Network architecture:
 *   Input (HalfKP features) → FeatureTransformer(512) → ClippedReLU
 *   → Linear(2×512 → 16) → ClippedReLU
 *   → Linear(16 → 32) → ClippedReLU
 *   → Linear(32 → 1)
 */

#include "nnue_eval.hpp"
#include "nnue_feature_transformer.hpp"
#include "../bitboard.hpp"
#include <fstream>
#include <iostream>
#include <cstring>
#include <memory>

#if defined(USE_AVX2)
#include <immintrin.h>
#endif

namespace PurnaFish::NNUE {

namespace {

// Network weights
struct NetworkWeights {
    FeatureTransformer ft;

    // Layer 1: 2 * FT_OUT_DIMS → L1_SIZE
    alignas(64) int8_t  w1[2 * FT_OUT_DIMS * L1_SIZE];
    alignas(64) int32_t b1[L1_SIZE];

    // Layer 2: L1_SIZE → L2_SIZE
    alignas(64) int8_t  w2[L1_SIZE * L2_SIZE];
    alignas(64) int32_t b2[L2_SIZE];

    // Output: L2_SIZE → 1
    alignas(64) int8_t  w3[L2_SIZE];
    alignas(64) int32_t b3;
};

std::unique_ptr<NetworkWeights> network;
std::string nnue_filename;
bool loaded = false;

/// Forward pass through the network (after accumulator)
int32_t forward(const Accumulator& acc, Color side) {
    if (!network) return 0;

    // Apply ClippedReLU to accumulator and create input
    // Input is [perspective_acc, other_acc] concatenated
    alignas(64) int8_t input[2 * FT_OUT_DIMS];

    Color perspectives[2] = {side, ~side};
    for (int p = 0; p < 2; ++p) {
        const int16_t* acc_data = acc.accumulation[perspectives[p]];
        int8_t* out = &input[p * FT_OUT_DIMS];

#if defined(USE_AVX2)
        const __m256i zero = _mm256_setzero_si256();
        const __m256i max_val = _mm256_set1_epi16(FT_QUANT);

        for (int i = 0; i < FT_OUT_DIMS; i += 16) {
            __m256i v = _mm256_load_si256((__m256i*)&acc_data[i]);
            v = _mm256_max_epi16(v, zero);          // clamp to 0
            v = _mm256_min_epi16(v, max_val);        // clamp to 255

            // Pack 16-bit to 8-bit
            __m256i next = _mm256_load_si256((__m256i*)&acc_data[i + 16]);
            next = _mm256_max_epi16(next, zero);
            next = _mm256_min_epi16(next, max_val);

            __m256i packed = _mm256_packs_epi16(v, next);
            packed = _mm256_permute4x64_epi64(packed, 0xD8);
            _mm256_store_si256((__m256i*)&out[i], packed);
            i += 16; // Extra increment since we process 32 at a time
        }
#else
        for (int i = 0; i < FT_OUT_DIMS; ++i)
            out[i] = int8_t(clipped_relu(acc_data[i]));
#endif
    }

    // Layer 1: input[2*512] × W1[2*512, 16] + B1[16]
    alignas(64) int32_t hidden1[L1_SIZE];
    std::memcpy(hidden1, network->b1, sizeof(hidden1));

    for (int j = 0; j < L1_SIZE; ++j) {
        int32_t sum = 0;
        const int8_t* w = &network->w1[j * 2 * FT_OUT_DIMS];
        for (int i = 0; i < 2 * FT_OUT_DIMS; ++i)
            sum += int32_t(input[i]) * int32_t(w[i]);
        hidden1[j] += sum;
    }

    // ClippedReLU + quantize
    alignas(64) int8_t h1_out[L1_SIZE];
    for (int i = 0; i < L1_SIZE; ++i)
        h1_out[i] = int8_t(std::clamp(hidden1[i] >> 6, 0, 127));

    // Layer 2: h1[16] × W2[16, 32] + B2[32]
    alignas(64) int32_t hidden2[L2_SIZE];
    std::memcpy(hidden2, network->b2, sizeof(hidden2));

    for (int j = 0; j < L2_SIZE; ++j) {
        int32_t sum = 0;
        const int8_t* w = &network->w2[j * L1_SIZE];
        for (int i = 0; i < L1_SIZE; ++i)
            sum += int32_t(h1_out[i]) * int32_t(w[i]);
        hidden2[j] += sum;
    }

    // ClippedReLU + quantize
    alignas(64) int8_t h2_out[L2_SIZE];
    for (int i = 0; i < L2_SIZE; ++i)
        h2_out[i] = int8_t(std::clamp(hidden2[i] >> 6, 0, 127));

    // Output layer: h2[32] × W3[32] + B3
    int32_t output = network->b3;
    for (int i = 0; i < L2_SIZE; ++i)
        output += int32_t(h2_out[i]) * int32_t(network->w3[i]);

    return output / NET_QUANT;
}

} // anonymous namespace

bool load(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "info string Failed to open NNUE file: " << filename << std::endl;
        return false;
    }

    network = std::make_unique<NetworkWeights>();

    // Read magic header
    uint32_t magic;
    file.read(reinterpret_cast<char*>(&magic), 4);
    if (magic != 0x50555246) { // "PURF"
        std::cerr << "info string Invalid NNUE file magic" << std::endl;
        network.reset();
        return false;
    }

    // Read network weights
    file.read(reinterpret_cast<char*>(network->ft.bias), sizeof(network->ft.bias));
    file.read(reinterpret_cast<char*>(network->ft.weight), sizeof(network->ft.weight));
    file.read(reinterpret_cast<char*>(network->w1), sizeof(network->w1));
    file.read(reinterpret_cast<char*>(network->b1), sizeof(network->b1));
    file.read(reinterpret_cast<char*>(network->w2), sizeof(network->w2));
    file.read(reinterpret_cast<char*>(network->b2), sizeof(network->b2));
    file.read(reinterpret_cast<char*>(network->w3), sizeof(network->w3));
    file.read(reinterpret_cast<char*>(&network->b3), sizeof(network->b3));

    if (!file) {
        std::cerr << "info string Failed to read NNUE weights" << std::endl;
        network.reset();
        return false;
    }

    nnue_filename = filename;
    loaded = true;
    std::cout << "info string NNUE loaded: " << filename << std::endl;
    return true;
}

bool is_loaded() { return loaded; }
const std::string& filename() { return nnue_filename; }

Value evaluate(const Position& pos) {
    if (!loaded || !network) return VALUE_ZERO;

    Accumulator acc;
    acc.clear();

    // Full refresh for both perspectives
    network->ft.refresh(acc, pos, WHITE);
    network->ft.refresh(acc, pos, BLACK);

    // Forward pass
    int32_t output = forward(acc, pos.side_to_move());

    return Value(std::clamp(output, int32_t(-VALUE_MATE_IN_MAX_PLY), int32_t(VALUE_MATE_IN_MAX_PLY)));
}

} // namespace PurnaFish::NNUE
