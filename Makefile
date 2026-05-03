# PurnaFish Chess Engine — Makefile
# Build: make
# Build with CUDA: make USE_CUDA=1
# Debug: make DEBUG=1
# Clean: make clean

CXX      = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -Wpedantic -Wno-unused-parameter
LDFLAGS  = -lpthread

# Build type
ifdef DEBUG
    CXXFLAGS += -g -O0 -fsanitize=address,undefined
    LDFLAGS  += -fsanitize=address,undefined
else
    CXXFLAGS += -O3 -DNDEBUG -flto
    LDFLAGS  += -flto
endif

# SIMD support
CXXFLAGS += -mpopcnt -msse4.1 -DUSE_SSE41
ifneq ($(NO_AVX2),1)
    CXXFLAGS += -mavx2 -DUSE_AVX2
endif
ifneq ($(NO_BMI2),1)
    CXXFLAGS += -mbmi2 -DUSE_BMI2
endif
ifdef USE_AVX512
    CXXFLAGS += -mavx512f -mavx512bw -mavx512dq -DUSE_AVX512
endif

# Source files
SRCDIR = src
SOURCES = \
    $(SRCDIR)/main.cpp \
    $(SRCDIR)/bitboard.cpp \
    $(SRCDIR)/zobrist.cpp \
    $(SRCDIR)/position.cpp \
    $(SRCDIR)/movegen.cpp \
    $(SRCDIR)/movepick.cpp \
    $(SRCDIR)/search.cpp \
    $(SRCDIR)/tt.cpp \
    $(SRCDIR)/evaluate.cpp \
    $(SRCDIR)/eval/hce.cpp \
    $(SRCDIR)/uci.cpp \
    $(SRCDIR)/thread.cpp \
    $(SRCDIR)/timeman.cpp \
    $(SRCDIR)/see.cpp \
    $(SRCDIR)/misc.cpp \
    $(SRCDIR)/perft.cpp \
    $(SRCDIR)/datagen.cpp \
    $(SRCDIR)/nnue/nnue_eval.cpp \
    $(SRCDIR)/syzygy/syzygy.cpp

OBJECTS = $(SOURCES:.cpp=.o)
OBJECTS += $(SRCDIR)/syzygy/tbprobe.o

# CUDA support
ifdef USE_CUDA
    NVCC     = nvcc
    NVFLAGS  = -std=c++20 -O3 --gpu-architecture=sm_75 -DUSE_CUDA
    CXXFLAGS += -DUSE_CUDA
    LDFLAGS  += -lcudart -L/usr/local/cuda/lib64

    CUDA_SOURCES = $(SRCDIR)/cuda/nnue_cuda.cu
    CUDA_OBJECTS = $(CUDA_SOURCES:.cu=.o)
    OBJECTS += $(CUDA_OBJECTS)
endif

TARGET = purnafish

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $@ $(LDFLAGS)
	@echo "Build complete: $(TARGET)"

$(SRCDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) -I$(SRCDIR) -c $< -o $@

$(SRCDIR)/syzygy/tbprobe.o: $(SRCDIR)/syzygy/tbprobe.c
	gcc -O3 -Wall -Wextra -c $< -o $@

ifdef USE_CUDA
$(SRCDIR)/cuda/%.o: $(SRCDIR)/cuda/%.cu
	$(NVCC) $(NVFLAGS) -I$(SRCDIR) -c $< -o $@
endif

clean:
	rm -f $(OBJECTS) $(TARGET)
	@echo "Clean complete"

# Dependencies
$(SRCDIR)/main.o: $(SRCDIR)/bitboard.hpp $(SRCDIR)/zobrist.hpp $(SRCDIR)/search.hpp $(SRCDIR)/uci.hpp
$(SRCDIR)/bitboard.o: $(SRCDIR)/bitboard.hpp $(SRCDIR)/types.hpp $(SRCDIR)/misc.hpp
$(SRCDIR)/position.o: $(SRCDIR)/position.hpp $(SRCDIR)/bitboard.hpp $(SRCDIR)/zobrist.hpp
$(SRCDIR)/movegen.o: $(SRCDIR)/movegen.hpp $(SRCDIR)/position.hpp $(SRCDIR)/bitboard.hpp
$(SRCDIR)/search.o: $(SRCDIR)/search.hpp $(SRCDIR)/evaluate.hpp $(SRCDIR)/movegen.hpp $(SRCDIR)/tt.hpp
$(SRCDIR)/uci.o: $(SRCDIR)/uci.hpp $(SRCDIR)/position.hpp $(SRCDIR)/search.hpp $(SRCDIR)/thread.hpp
