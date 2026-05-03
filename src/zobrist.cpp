/*
 * PurnaFish Chess Engine
 * zobrist.cpp — Zobrist hash key initialization
 */

#include "zobrist.hpp"
#include "misc.hpp"

namespace PurnaFish {

namespace Zobrist {

Key psq[PIECE_NB][SQUARE_NB];
Key enpassant[8];
Key castling[CASTLING_RIGHT_NB];
Key side;
Key no_pawns;

void init() {
    PRNG rng(1070372);

    for (Piece pc = NO_PIECE; pc < Piece(PIECE_NB); ++pc)
        for (Square s = SQ_A1; s <= SQ_H8; ++s)
            psq[pc][s] = rng.rand<Key>();

    for (int f = 0; f < 8; ++f)
        enpassant[f] = rng.rand<Key>();

    // Generate independent keys for each single castling right
    Key castlingKeys[4]; // for bits 0,1,2,3
    for (int i = 0; i < 4; ++i)
        castlingKeys[i] = rng.rand<Key>();

    // Combine keys for composite castling rights
    for (int cr = 0; cr < CASTLING_RIGHT_NB; ++cr) {
        castling[cr] = 0;
        for (int i = 0; i < 4; ++i)
            if (cr & (1 << i))
                castling[cr] ^= castlingKeys[i];
    }

    side = rng.rand<Key>();
    no_pawns = rng.rand<Key>();
}

} // namespace Zobrist

} // namespace PurnaFish
