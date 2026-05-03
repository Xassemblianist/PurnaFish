cmake_minimum_required(VERSION 3.20)
project(PurnaFish VERSION 1.0.0 LANGUAGES CXX)

# ──────────────────────────────────────────────
# C++ Standard
# ──────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ──────────────────────────────────────────────
# Build Type
# ──────────────────────────────────────────────
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ──────────────────────────────────────────────
# Optional CUDA
# ──────────────────────────────────────────────
option(PURNAFISH_USE_CUDA "Enable CUDA GPU acceleration" OFF)

if(PURNAFISH_USE_CUDA)
    include(CheckLanguage)
    check_language(CUDA)
    if(CMAKE_CUDA_COMPILER)
        enable_language(CUDA)
        set(CMAKE_CUDA_STANDARD 20)
        set(CMAKE_CUDA_STANDARD_REQUIRED ON)
        add_compile_definitions(USE_CUDA)
        message(STATUS "PurnaFish: CUDA enabled")
    else()
        message(WARNING "PurnaFish: CUDA requested but no compiler found. Building CPU-only.")
        set(PURNAFISH_USE_CUDA OFF)
    endif()
endif()

# ──────────────────────────────────────────────
# SIMD Detection
# ──────────────────────────────────────────────
include(CheckCXXCompilerFlag)

option(PURNAFISH_USE_AVX512 "Enable AVX-512 instructions" OFF)
option(PURNAFISH_USE_AVX2   "Enable AVX2 instructions"    ON)
option(PURNAFISH_USE_SSE41  "Enable SSE4.1 instructions"  ON)
option(PURNAFISH_USE_BMI2   "Enable BMI2 (PEXT) instructions" ON)

# ──────────────────────────────────────────────
# Source Files
# ──────────────────────────────────────────────
set(PURNAFISH_SOURCES
    src/main.cpp
    src/bitboard.cpp
    src/zobrist.cpp
    src/position.cpp
    src/movegen.cpp
    src/movepick.cpp
    src/search.cpp
    src/tt.cpp
    src/evaluate.cpp
    src/eval/hce.cpp
    src/uci.cpp
    src/thread.cpp
    src/timeman.cpp
    src/see.cpp
    src/misc.cpp
    src/perft.cpp
    src/datagen.cpp
    src/syzygy/tbprobe.c
    src/syzygy/syzygy.cpp
)

# NNUE sources
list(APPEND PURNAFISH_SOURCES
    src/nnue/nnue_eval.cpp
)

# CUDA sources
if(PURNAFISH_USE_CUDA)
    list(APPEND PURNAFISH_SOURCES
        src/cuda/nnue_cuda.cu
    )
endif()

# ──────────────────────────────────────────────
# Executable
# ──────────────────────────────────────────────
add_executable(purnafish ${PURNAFISH_SOURCES})

target_include_directories(purnafish PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# ──────────────────────────────────────────────
# Compiler Flags
# ──────────────────────────────────────────────
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(purnafish PRIVATE
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter
        $<$<CONFIG:Release>:-O3 -DNDEBUG -flto>
        $<$<CONFIG:Debug>:-g -O0 -fsanitize=address,undefined>
    )
    target_link_options(purnafish PRIVATE
        $<$<CONFIG:Release>:-flto>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
    )

    # SIMD flags
    if(PURNAFISH_USE_AVX512)
        target_compile_options(purnafish PRIVATE -mavx512f -mavx512bw -mavx512dq)
        target_compile_definitions(purnafish PRIVATE USE_AVX512)
    endif()
    if(PURNAFISH_USE_AVX2)
        target_compile_options(purnafish PRIVATE -mavx2)
        target_compile_definitions(purnafish PRIVATE USE_AVX2)
    endif()
    if(PURNAFISH_USE_SSE41)
        target_compile_options(purnafish PRIVATE -msse4.1)
        target_compile_definitions(purnafish PRIVATE USE_SSE41)
    endif()
    if(PURNAFISH_USE_BMI2)
        target_compile_options(purnafish PRIVATE -mbmi2)
        target_compile_definitions(purnafish PRIVATE USE_BMI2)
    endif()

    # Always enable popcnt
    target_compile_options(purnafish PRIVATE -mpopcnt)
endif()

# Threading
find_package(Threads REQUIRED)
target_link_libraries(purnafish PRIVATE Threads::Threads)

# CUDA linking
if(PURNAFISH_USE_CUDA)
    set_target_properties(purnafish PROPERTIES
        CUDA_SEPARABLE_COMPILATION ON
        CUDA_ARCHITECTURES "75;86;89"
    )
    target_link_libraries(purnafish PRIVATE CUDA::cudart)
endif()

# ──────────────────────────────────────────────
# Install
# ──────────────────────────────────────────────
install(TARGETS purnafish DESTINATION bin)

message(STATUS "PurnaFish v${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  CUDA:       ${PURNAFISH_USE_CUDA}")
message(STATUS "  AVX2:       ${PURNAFISH_USE_AVX2}")
message(STATUS "  AVX-512:    ${PURNAFISH_USE_AVX512}")
message(STATUS "  BMI2:       ${PURNAFISH_USE_BMI2}")
