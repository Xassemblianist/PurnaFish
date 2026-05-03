/*
 * PurnaFish Chess Engine
 * perft.hpp / perft.cpp — Move generation verification via perft
 */

#pragma once

#include "types.hpp"
#include "position.hpp"
#include <cstdint>

namespace PurnaFish {

uint64_t perft(Position& pos, int depth);
void     perft_divide(Position& pos, int depth);

} // namespace PurnaFish
