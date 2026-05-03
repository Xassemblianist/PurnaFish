# PurnaFish Chess Engine 🐟

PurnaFish is a high-performance, UCI-compliant chess engine written in C++. It features a modern threaded search architecture and a hand-crafted evaluation (HCE), with upcoming support for NNUE (Efficiently Updatable Neural Networks).

## Features
- **UCI Compliant**: Fully supports the Universal Chess Interface (UCI).
- **Multi-Threaded**: Scales efficiently across multiple CPU cores.
- **Fast Move Generation**: Optimized bitboard-based legal move generator (~135M NPS).
- **Evaluation**: 
  - Current: Hand-Crafted Evaluation (HCE) with PSTs, mobility, and pawn structure.
  - Planned: NNUE (HalfKP architecture) with GPU acceleration for training.
- **Tablebase Support**: Integrated Syzygy tablebase probing.
- **Stable Architecture**: Resolved critical stack-overflow and race condition issues for competitive play.

## Performance
- **NPS**: ~130-140 Million nodes per second (Move generation on modern hardware).
- **Stability**: Tested with rigorous perft and self-play benchmarks.

## Build Instructions
### Linux
```bash
make clean
make -j$(nproc)
```
For the final optimized release:
```bash
make clean
make -j$(nproc) CXXFLAGS="-std=c++20 -O3 -DNDEBUG -mpopcnt -msse4.1 -DUSE_SSE41 -mavx2 -DUSE_AVX2 -mbmi2 -DUSE_BMI2 -Isrc" LDFLAGS="-lpthread"
```

## Future Roadmap
- [ ] Training NNUE using 5070 Ti hardware.
- [ ] Implementing AVX-512 kernels for NNUE forward pass.
- [ ] Reaching 3000+ Elo on TCEC-level hardware.

## License
MIT License
