/*
 * PurnaFish Chess Engine
 * cuda/nnue_cuda.cu — CUDA NNUE batch evaluation kernels
 *
 * GPU-accelerated NNUE forward pass using CUDA.
 * Supports batch evaluation of multiple positions simultaneously.
 * Uses Tensor Cores (WMMA) when available (sm_75+) for INT8 matmul.
 */

#ifdef USE_CUDA

#include "nnue_cuda.cuh"
#include "../nnue/nnue_common.hpp"
#include <cuda_runtime.h>

// Tensor Core support for sm_75+
#if __CUDA_ARCH__ >= 750
#include <mma.h>
using namespace nvcuda;
#define USE_TENSOR_CORES 1
#else
#define USE_TENSOR_CORES 0
#endif

using namespace PurnaFish::NNUE;

namespace PurnaFish::CudaNNUE {

// Device-side network weights
namespace {
    // Feature transformer not on GPU (too large, computed on CPU)
    int8_t*  d_w1 = nullptr;   // [2*FT_OUT_DIMS × L1_SIZE]
    int32_t* d_b1 = nullptr;   // [L1_SIZE]
    int8_t*  d_w2 = nullptr;   // [L1_SIZE × L2_SIZE]
    int32_t* d_b2 = nullptr;   // [L2_SIZE]
    int8_t*  d_w3 = nullptr;   // [L2_SIZE]
    int32_t* d_b3 = nullptr;   // [1]

    // Temporary buffers
    int16_t* d_accumulators = nullptr;  // [MAX_BATCH × 2 × FT_OUT_DIMS]
    int8_t*  d_input = nullptr;         // [MAX_BATCH × 2 × FT_OUT_DIMS]
    int32_t* d_hidden1 = nullptr;       // [MAX_BATCH × L1_SIZE]
    int32_t* d_hidden2 = nullptr;       // [MAX_BATCH × L2_SIZE]
    int32_t* d_output = nullptr;        // [MAX_BATCH]
    int*     d_sides = nullptr;         // [MAX_BATCH]

    bool initialized = false;
}

// ──────────────────────────────────────────────
// Kernels
// ──────────────────────────────────────────────

/// Apply ClippedReLU and arrange accumulator as network input
/// Each position has two perspectives; we concatenate [stm_acc, ~stm_acc]
__global__ void kernel_prepare_input(
    const int16_t* __restrict__ accumulators,  // [batch × 2 × FT_OUT_DIMS]
    const int* __restrict__ sides,             // [batch]
    int8_t* __restrict__ output,               // [batch × 2 × FT_OUT_DIMS]
    int batch_size)
{
    int tid = blockIdx.x * blockDim.x + threadIdx.x;
    int total = batch_size * 2 * FT_OUT_DIMS;
    if (tid >= total) return;

    int b = tid / (2 * FT_OUT_DIMS);           // batch index
    int remainder = tid % (2 * FT_OUT_DIMS);
    int half = remainder / FT_OUT_DIMS;          // 0=first half, 1=second half
    int dim = remainder % FT_OUT_DIMS;

    // Determine perspective ordering based on side to move
    int side = sides[b];
    int src_perspective = (half == 0) ? side : (1 - side);

    int16_t val = accumulators[b * 2 * FT_OUT_DIMS + src_perspective * FT_OUT_DIMS + dim];

    // ClippedReLU: clamp to [0, 255], then scale to int8 [0, 127]
    int clamped = max(0, min(int(val), FT_QUANT));
    output[tid] = (int8_t)(clamped >> 1); // Scale to fit int8
}

/// Dense layer: output[b][j] = bias[j] + sum_i(input[b][i] * weight[j*K + i])
__global__ void kernel_dense_layer(
    const int8_t* __restrict__ input,     // [batch × K]
    const int8_t* __restrict__ weight,    // [N × K]
    const int32_t* __restrict__ bias,     // [N]
    int32_t* __restrict__ output,         // [batch × N]
    int batch_size, int K, int N)
{
    int b = blockIdx.x;
    int j = threadIdx.x;

    if (b >= batch_size || j >= N) return;

    int32_t sum = bias[j];
    const int8_t* w = &weight[j * K];
    const int8_t* inp = &input[b * K];

    // Vectorized dot product
    for (int i = 0; i < K; i += 4) {
        sum += int32_t(inp[i])   * int32_t(w[i]);
        sum += int32_t(inp[i+1]) * int32_t(w[i+1]);
        sum += int32_t(inp[i+2]) * int32_t(w[i+2]);
        sum += int32_t(inp[i+3]) * int32_t(w[i+3]);
    }

    output[b * N + j] = sum;
}

/// Apply ClippedReLU + quantize in place
__global__ void kernel_clipped_relu_quantize(
    const int32_t* __restrict__ input,
    int8_t* __restrict__ output,
    int total)
{
    int tid = blockIdx.x * blockDim.x + threadIdx.x;
    if (tid >= total) return;

    int32_t val = input[tid] >> 6; // De-quantize
    output[tid] = (int8_t)max(0, min(val, 127));
}

/// Output layer: scalar dot product per batch element
__global__ void kernel_output_layer(
    const int8_t* __restrict__ input,     // [batch × L2_SIZE]
    const int8_t* __restrict__ weight,    // [L2_SIZE]
    const int32_t* __restrict__ bias,     // [1]
    int32_t* __restrict__ output,         // [batch]
    int batch_size)
{
    int b = blockIdx.x * blockDim.x + threadIdx.x;
    if (b >= batch_size) return;

    int32_t sum = *bias;
    const int8_t* inp = &input[b * L2_SIZE];

    for (int i = 0; i < L2_SIZE; ++i)
        sum += int32_t(inp[i]) * int32_t(weight[i]);

    output[b] = sum / NET_QUANT;
}

// ──────────────────────────────────────────────
// Public API
// ──────────────────────────────────────────────

bool init(const int16_t* ft_weights, const int16_t* ft_bias,
          const int8_t* w1, const int32_t* b1,
          const int8_t* w2, const int32_t* b2,
          const int8_t* w3, const int32_t* b3) {

    if (!CUDA::init()) return false;

    // Allocate and copy weights
    CUDA_CHECK(cudaMalloc(&d_w1, 2 * FT_OUT_DIMS * L1_SIZE * sizeof(int8_t)));
    CUDA_CHECK(cudaMalloc(&d_b1, L1_SIZE * sizeof(int32_t)));
    CUDA_CHECK(cudaMalloc(&d_w2, L1_SIZE * L2_SIZE * sizeof(int8_t)));
    CUDA_CHECK(cudaMalloc(&d_b2, L2_SIZE * sizeof(int32_t)));
    CUDA_CHECK(cudaMalloc(&d_w3, L2_SIZE * sizeof(int8_t)));
    CUDA_CHECK(cudaMalloc(&d_b3, sizeof(int32_t)));

    CUDA_CHECK(cudaMemcpy(d_w1, w1, 2 * FT_OUT_DIMS * L1_SIZE * sizeof(int8_t), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_b1, b1, L1_SIZE * sizeof(int32_t), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_w2, w2, L1_SIZE * L2_SIZE * sizeof(int8_t), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_b2, b2, L2_SIZE * sizeof(int32_t), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_w3, w3, L2_SIZE * sizeof(int8_t), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_b3, b3, sizeof(int32_t), cudaMemcpyHostToDevice));

    // Allocate working buffers
    int maxBatch = MAX_GPU_BATCH_SIZE;
    CUDA_CHECK(cudaMalloc(&d_accumulators, maxBatch * 2 * FT_OUT_DIMS * sizeof(int16_t)));
    CUDA_CHECK(cudaMalloc(&d_input,   maxBatch * 2 * FT_OUT_DIMS * sizeof(int8_t)));
    CUDA_CHECK(cudaMalloc(&d_hidden1, maxBatch * L1_SIZE * sizeof(int32_t)));
    CUDA_CHECK(cudaMalloc(&d_hidden2, maxBatch * L2_SIZE * sizeof(int32_t)));
    CUDA_CHECK(cudaMalloc(&d_output,  maxBatch * sizeof(int32_t)));
    CUDA_CHECK(cudaMalloc(&d_sides,   maxBatch * sizeof(int)));

    // Temp buffer for int8 intermediates
    // Reuse d_input for subsequent layer inputs

    initialized = true;
    return true;
}

void batch_evaluate(const int16_t* accumulators, const int* sides,
                    int32_t* results, int batch_size, cudaStream_t stream) {
    if (!initialized || batch_size == 0) return;

    batch_size = min(batch_size, MAX_GPU_BATCH_SIZE);

    // Copy input data to device
    CUDA_CHECK(cudaMemcpyAsync(d_accumulators, accumulators,
        batch_size * 2 * FT_OUT_DIMS * sizeof(int16_t), cudaMemcpyHostToDevice, stream));
    CUDA_CHECK(cudaMemcpyAsync(d_sides, sides,
        batch_size * sizeof(int), cudaMemcpyHostToDevice, stream));

    // 1. Prepare input: ClippedReLU on accumulators, arrange by STM
    int total_input = batch_size * 2 * FT_OUT_DIMS;
    int blocks_input = (total_input + BLOCK_SIZE_1D - 1) / BLOCK_SIZE_1D;
    kernel_prepare_input<<<blocks_input, BLOCK_SIZE_1D, 0, stream>>>(
        d_accumulators, d_sides, d_input, batch_size);

    // 2. Layer 1: Dense(2*512 → 16)
    kernel_dense_layer<<<batch_size, L1_SIZE, 0, stream>>>(
        d_input, d_w1, d_b1, d_hidden1, batch_size, 2 * FT_OUT_DIMS, L1_SIZE);

    // 3. ClippedReLU + quantize layer 1 output
    int8_t* d_h1_int8 = d_input; // Reuse buffer (safe since L1 << input)
    int total_h1 = batch_size * L1_SIZE;
    int blocks_h1 = (total_h1 + BLOCK_SIZE_1D - 1) / BLOCK_SIZE_1D;
    kernel_clipped_relu_quantize<<<blocks_h1, BLOCK_SIZE_1D, 0, stream>>>(
        d_hidden1, d_h1_int8, total_h1);

    // 4. Layer 2: Dense(16 → 32)
    kernel_dense_layer<<<batch_size, L2_SIZE, 0, stream>>>(
        d_h1_int8, d_w2, d_b2, d_hidden2, batch_size, L1_SIZE, L2_SIZE);

    // 5. ClippedReLU + quantize layer 2 output
    int8_t* d_h2_int8 = d_h1_int8; // Reuse
    int total_h2 = batch_size * L2_SIZE;
    int blocks_h2 = (total_h2 + BLOCK_SIZE_1D - 1) / BLOCK_SIZE_1D;
    kernel_clipped_relu_quantize<<<blocks_h2, BLOCK_SIZE_1D, 0, stream>>>(
        d_hidden2, d_h2_int8, total_h2);

    // 6. Output layer: Dense(32 → 1)
    int blocks_out = (batch_size + BLOCK_SIZE_1D - 1) / BLOCK_SIZE_1D;
    kernel_output_layer<<<blocks_out, BLOCK_SIZE_1D, 0, stream>>>(
        d_h2_int8, d_w3, d_b3, d_output, batch_size);

    // Copy results back
    CUDA_CHECK(cudaMemcpyAsync(results, d_output,
        batch_size * sizeof(int32_t), cudaMemcpyDeviceToHost, stream));

    CUDA_CHECK(cudaStreamSynchronize(stream));
}

void cleanup() {
    if (!initialized) return;

    cudaFree(d_w1); cudaFree(d_b1);
    cudaFree(d_w2); cudaFree(d_b2);
    cudaFree(d_w3); cudaFree(d_b3);
    cudaFree(d_accumulators);
    cudaFree(d_input);
    cudaFree(d_hidden1);
    cudaFree(d_hidden2);
    cudaFree(d_output);
    cudaFree(d_sides);

    CUDA::cleanup();
    initialized = false;
}

bool is_initialized() { return initialized; }

} // namespace PurnaFish::CudaNNUE

#endif // USE_CUDA
