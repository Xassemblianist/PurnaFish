/*
 * PurnaFish Chess Engine
 * zobrist.hpp — Zobrist hashing for position identification
 */

#pragma once

#include "types.hpp"

namespace PurnaFish {

namespace Zobrist {

extern Key psq[PIECE_NB][SQUARE_NB];
extern Key enpassant[8]; // FILE_NB
extern Key castling[CASTLING_RIGHT_NB];
extern Key side;
extern Key no_pawns;

void init();

} // namespace Zobrist

} // namespace PurnaFish
