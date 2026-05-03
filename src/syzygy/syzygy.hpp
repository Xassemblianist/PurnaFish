#pragma once

#include "../types.hpp"
#include "../position.hpp"
#include <string>

namespace PurnaFish {

namespace Tablebases {

extern int MaxPieces;

bool init(const std::string& path);
void free();

bool probe_wdl(const Position& pos, Value& wdl_value);
bool probe_root(const Position& pos, Move& bestMove, Value& dtz_value);

Value wdl_to_value(int wdl, int ply);

} // namespace Tablebases

} // namespace PurnaFish
