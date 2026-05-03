/*
 * PurnaFish Chess Engine
 * cuda/nnue_cuda.cuh — CUDA NNUE evaluation header
 */

#pragma once

#ifdef USE_CUDA

#include "cuda_common.cuh"
#include "../nnue/nnue_common.hpp"
#include <cstdint>

namespace PurnaFish::CudaNNUE {

/// Initialize GPU resources and copy weights
bool init(const int16_t* ft_weights, const int16_t* ft_bias,
          const int8_t* w1, const int32_t* b1,
          const int8_t* w2, const int32_t* b2,
          const int8_t* w3, const int32_t* b3);

/// Batch evaluate positions on GPU
/// accumulators: [batch_size × 2 × FT_OUT_DIMS] — int16_t
/// sides: [batch_size] — which side to move (0=WHITE, 1=BLACK)
/// results: [batch_size] — output evaluation in centipawns
void batch_evaluate(const int16_t* accumulators, const int* sides,
                    int32_t* results, int batch_size, cudaStream_t stream = 0);

/// Free GPU resources
void cleanup();

/// Check if GPU NNUE is initialized
bool is_initialized();

} // namespace PurnaFish::CudaNNUE

#endif // USE_CUDA
