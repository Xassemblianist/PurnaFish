/*
 * PurnaFish Chess Engine
 * cuda/cuda_common.cuh — CUDA utilities and error checking
 */

#pragma once

#ifdef USE_CUDA

#include <cuda_runtime.h>
#include <cstdio>
#include <cstdlib>

#define CUDA_CHECK(call) do { \
    cudaError_t err = (call); \
    if (err != cudaSuccess) { \
        fprintf(stderr, "PurnaFish CUDA Error: %s at %s:%d\n", \
                cudaGetErrorString(err), __FILE__, __LINE__); \
        exit(EXIT_FAILURE); \
    } \
} while(0)

// Warp size
constexpr int WARP_SIZE = 32;

// Block sizes for different kernels
constexpr int BLOCK_SIZE_1D = 256;
constexpr int BLOCK_SIZE_2D = 16;

// Maximum batch size for GPU evaluation
constexpr int MAX_GPU_BATCH_SIZE = 512;

namespace PurnaFish::CUDA {

/// Initialize CUDA device
inline bool init() {
    int deviceCount = 0;
    cudaGetDeviceCount(&deviceCount);
    if (deviceCount == 0) {
        fprintf(stderr, "PurnaFish: No CUDA-capable GPU found\n");
        return false;
    }

    cudaDeviceProp prop;
    CUDA_CHECK(cudaGetDeviceProperties(&prop, 0));
    printf("info string PurnaFish GPU: %s (SM %d.%d, %d SMs, %.1f GB)\n",
           prop.name, prop.major, prop.minor,
           prop.multiProcessorCount,
           prop.totalGlobalMem / (1024.0 * 1024.0 * 1024.0));

    CUDA_CHECK(cudaSetDevice(0));
    return true;
}

/// Cleanup CUDA resources
inline void cleanup() {
    cudaDeviceReset();
}

} // namespace PurnaFish::CUDA

#endif // USE_CUDA
