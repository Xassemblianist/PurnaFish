/*
 * PurnaFish Chess Engine
 * bitboard.hpp — Bitboard definitions, attack tables, and magic bitboards
 */

#pragma once

#include "types.hpp"
#include <cstring>
#include <bit>

#if defined(USE_AVX2) || defined(USE_AVX512)
#include <immintrin.h>
#endif

namespace PurnaFish {

constexpr Bitboard FileABB = 0x0101010101010101ULL;
constexpr Bitboard FileBBB = FileABB << 1;
constexpr Bitboard FileCBB = FileABB << 2;
constexpr Bitboard FileDBB = FileABB << 3;
constexpr Bitboard FileEBB = FileABB << 4;
constexpr Bitboard FileFBB = FileABB << 5;
constexpr Bitboard FileGBB = FileABB << 6;
constexpr Bitboard FileHBB = FileABB << 7;

constexpr Bitboard Rank1BB = 0xFFULL;
constexpr Bitboard Rank2BB = Rank1BB << 8;
constexpr Bitboard Rank3BB = Rank1BB << 16;
constexpr Bitboard Rank4BB = Rank1BB << 24;
constexpr Bitboard Rank5BB = Rank1BB << 32;
constexpr Bitboard Rank6BB = Rank1BB << 40;
constexpr Bitboard Rank7BB = Rank1BB << 48;
constexpr Bitboard Rank8BB = Rank1BB << 56;

constexpr Bitboard DarkSquares  = 0xAA55AA55AA55AA55ULL;
constexpr Bitboard LightSquares = ~DarkSquares;
constexpr Bitboard AllSquares   = ~Bitboard(0);

extern Bitboard FileBB[8];
extern Bitboard RankBB[8];

// Bit manipulation
inline int popcount(Bitboard b) {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_popcountll(b);
#else
    return std::popcount(b);
#endif
}

inline Square lsb(Bitboard b) {
    assert(b);
#if defined(__GNUC__) || defined(__clang__)
    return Square(__builtin_ctzll(b));
#else
    return Square(std::countr_zero(b));
#endif
}

inline Square msb(Bitboard b) {
    assert(b);
#if defined(__GNUC__) || defined(__clang__)
    return Square(63 ^ __builtin_clzll(b));
#else
    return Square(63 - std::countl_zero(b));
#endif
}

inline Square pop_lsb(Bitboard& b) {
    assert(b);
    Square s = lsb(b);
    b &= b - 1;
    return s;
}

inline bool more_than_one(Bitboard b) { return b & (b - 1); }
inline Bitboard square_bb(Square s) { return 1ULL << s; }

#ifdef USE_BMI2
inline uint64_t pext(uint64_t v, uint64_t m) { return _pext_u64(v, m); }
#endif

// Pre-computed attack tables
extern Bitboard PawnAttacks[2][64];
extern Bitboard KnightAttacks[64];
extern Bitboard KingAttacks[64];
extern Bitboard PseudoAttacks[8][64];
extern Bitboard BetweenBB[64][64];
extern Bitboard LineBB[64][64];
extern uint8_t SquareDistance[64][64];

// Magic bitboard structures
struct Magic {
    Bitboard  mask;
    Bitboard  magic;
    Bitboard* attacks;
    int       shift;

    unsigned index(Bitboard occupied) const {
#ifdef USE_BMI2
        return unsigned(pext(occupied, mask));
#else
        return unsigned(((occupied & mask) * magic) >> shift);
#endif
    }
};

extern Magic BishopMagics[64];
extern Magic RookMagics[64];

// Attack table storage
extern Bitboard BishopTable[0x1480]; // 5248 entries total
extern Bitboard RookTable[0x19000]; // 102400 entries total

// Attack lookup functions
inline Bitboard pawn_attacks_bb(Color c, Square s) { return PawnAttacks[c][s]; }

template<PieceType Pt>
inline Bitboard attacks_bb(Square s, Bitboard occupied) {
    static_assert(Pt == BISHOP || Pt == ROOK || Pt == QUEEN);
    if constexpr (Pt == BISHOP)
        return BishopMagics[s].attacks[BishopMagics[s].index(occupied)];
    else if constexpr (Pt == ROOK)
        return RookMagics[s].attacks[RookMagics[s].index(occupied)];
    else
        return attacks_bb<BISHOP>(s, occupied) | attacks_bb<ROOK>(s, occupied);
}

inline Bitboard attacks_bb(PieceType pt, Square s, Bitboard occupied) {
    switch (pt) {
        case BISHOP: return attacks_bb<BISHOP>(s, occupied);
        case ROOK:   return attacks_bb<ROOK>(s, occupied);
        case QUEEN:  return attacks_bb<QUEEN>(s, occupied);
        case KNIGHT: return KnightAttacks[s];
        case KING:   return KingAttacks[s];
        default:     return 0;
    }
}

inline Bitboard between_bb(Square s1, Square s2) { return BetweenBB[s1][s2]; }
inline Bitboard line_bb(Square s1, Square s2) { return LineBB[s1][s2]; }
inline bool aligned(Square s1, Square s2, Square s3) { return LineBB[s1][s2] & square_bb(s3); }
inline int distance(Square s1, Square s2) { return SquareDistance[s1][s2]; }

template<Direction D>
constexpr Bitboard shift(Bitboard b) {
    if constexpr (D == NORTH)      return b << 8;
    else if constexpr (D == SOUTH) return b >> 8;
    else if constexpr (D == NORTH + NORTH) return b << 16;
    else if constexpr (D == SOUTH + SOUTH) return b >> 16;
    else if constexpr (D == EAST)       return (b & ~FileHBB) << 1;
    else if constexpr (D == WEST)       return (b & ~FileABB) >> 1;
    else if constexpr (D == NORTH_EAST) return (b & ~FileHBB) << 9;
    else if constexpr (D == NORTH_WEST) return (b & ~FileABB) << 7;
    else if constexpr (D == SOUTH_EAST) return (b & ~FileHBB) >> 7;
    else if constexpr (D == SOUTH_WEST) return (b & ~FileABB) >> 9;
    else return 0;
}

inline Bitboard forward_ranks_bb(Color c, Rank r) {
    return c == WHITE ? ~Rank1BB << (8 * r) : ~Rank8BB >> (8 * (7 - r));
}

inline Bitboard forward_file_bb(Color c, Square s) {
    return forward_ranks_bb(c, rank_of(s)) & FileBB[file_of(s)];
}

inline Bitboard adjacent_files_bb(Square s) {
    File f = file_of(s);
    return (f > FILE_A ? FileBB[f - 1] : 0) | (f < FILE_H ? FileBB[f + 1] : 0);
}

inline Bitboard passed_pawn_span(Color c, Square s) {
    return forward_ranks_bb(c, rank_of(s)) & (FileBB[file_of(s)] | adjacent_files_bb(s));
}

namespace Bitboards { void init(); }
std::string pretty(Bitboard b);

} // namespace PurnaFish
