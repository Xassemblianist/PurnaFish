/*
 * PurnaFish Chess Engine
 * types.hpp — Core type definitions
 *
 * Copyright (c) 2026 PurnaFish Authors
 */

#pragma once

#include <cstdint>
#include <cstdlib>
#include <cassert>
#include <string>
#include <algorithm>

namespace PurnaFish {

// ──────────────────────────────────────────────
// Fundamental Types
// ──────────────────────────────────────────────

using Bitboard = uint64_t;
using Key      = uint64_t;

// ──────────────────────────────────────────────
// Enumerations
// ──────────────────────────────────────────────

enum Color : int {
    WHITE, BLACK, COLOR_NB = 2
};

enum PieceType : int {
    NO_PIECE_TYPE, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING,
    ALL_PIECES = 0,
    PIECE_TYPE_NB = 8
};

enum Piece : int {
    NO_PIECE,
    W_PAWN = PAWN,     W_KNIGHT, W_BISHOP, W_ROOK, W_QUEEN, W_KING,
    B_PAWN = PAWN + 8, B_KNIGHT, B_BISHOP, B_ROOK, B_QUEEN, B_KING,
    PIECE_NB = 16
};

// Squares: a1=0, b1=1, ..., h1=7, a2=8, ..., h8=63
enum Square : int {
    SQ_A1, SQ_B1, SQ_C1, SQ_D1, SQ_E1, SQ_F1, SQ_G1, SQ_H1,
    SQ_A2, SQ_B2, SQ_C2, SQ_D2, SQ_E2, SQ_F2, SQ_G2, SQ_H2,
    SQ_A3, SQ_B3, SQ_C3, SQ_D3, SQ_E3, SQ_F3, SQ_G3, SQ_H3,
    SQ_A4, SQ_B4, SQ_C4, SQ_D4, SQ_E4, SQ_F4, SQ_G4, SQ_H4,
    SQ_A5, SQ_B5, SQ_C5, SQ_D5, SQ_E5, SQ_F5, SQ_G5, SQ_H5,
    SQ_A6, SQ_B6, SQ_C6, SQ_D6, SQ_E6, SQ_F6, SQ_G6, SQ_H6,
    SQ_A7, SQ_B7, SQ_C7, SQ_D7, SQ_E7, SQ_F7, SQ_G7, SQ_H7,
    SQ_A8, SQ_B8, SQ_C8, SQ_D8, SQ_E8, SQ_F8, SQ_G8, SQ_H8,
    SQ_NONE,
    SQUARE_NB = 64
};

enum Direction : int {
    NORTH =  8,
    EAST  =  1,
    SOUTH = -8,
    WEST  = -1,

    NORTH_EAST = NORTH + EAST,
    SOUTH_EAST = SOUTH + EAST,
    SOUTH_WEST = SOUTH + WEST,
    NORTH_WEST = NORTH + WEST
};

enum File : int {
    FILE_A, FILE_B, FILE_C, FILE_D, FILE_E, FILE_F, FILE_G, FILE_H, FILE_NB
};

enum Rank : int {
    RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_NB
};

enum CastlingRights : int {
    NO_CASTLING,
    WHITE_OO  = 1,
    WHITE_OOO = 2,
    BLACK_OO  = 4,
    BLACK_OOO = 8,

    KING_SIDE  = WHITE_OO  | BLACK_OO,
    QUEEN_SIDE = WHITE_OOO | BLACK_OOO,
    WHITE_CASTLING = WHITE_OO | WHITE_OOO,
    BLACK_CASTLING = BLACK_OO | BLACK_OOO,
    ANY_CASTLING   = WHITE_CASTLING | BLACK_CASTLING,

    CASTLING_RIGHT_NB = 16
};

// Move encoding: 16-bit
// bits 0-5:   destination square
// bits 6-11:  origin square
// bits 12-13: promotion piece type (KNIGHT=0, BISHOP=1, ROOK=2, QUEEN=3)
// bits 14-15: move type (NORMAL=0, PROMOTION=1, EN_PASSANT=2, CASTLING=3)
enum MoveType : int {
    NORMAL,
    PROMOTION  = 1 << 14,
    EN_PASSANT = 2 << 14,
    CASTLING   = 3 << 14
};

enum Move : int {
    MOVE_NONE,
    MOVE_NULL = 65
};

enum Value : int {
    VALUE_ZERO     = 0,
    VALUE_DRAW     = 0,
    VALUE_KNOWN_WIN = 10000,
    VALUE_MATE     = 32000,
    VALUE_INFINITE = 32001,
    VALUE_NONE     = 32002,

    VALUE_TB_WIN          = VALUE_MATE - 1000,
    VALUE_TB_LOSS         = -VALUE_TB_WIN,
    VALUE_MATE_IN_MAX_PLY = VALUE_MATE - 256,
    VALUE_MATED_IN_MAX_PLY = -VALUE_MATE_IN_MAX_PLY,

    // Piece values (centipawns)
    PawnValue   = 208,
    KnightValue = 781,
    BishopValue = 825,
    RookValue   = 1276,
    QueenValue  = 2538,
};

enum Depth : int {
    DEPTH_QS_CHECKS    =  0,
    DEPTH_QS_NO_CHECKS = -1,
    DEPTH_NONE         = -6,
    DEPTH_OFFSET       = -7  // For TT storage
};

enum Bound : uint8_t {
    BOUND_NONE  = 0,
    BOUND_UPPER = 1,
    BOUND_LOWER = 2,
    BOUND_EXACT = BOUND_UPPER | BOUND_LOWER
};

// ──────────────────────────────────────────────
// Constants
// ──────────────────────────────────────────────

constexpr int MAX_MOVES = 256;
constexpr int MAX_PLY   = 246;

// ──────────────────────────────────────────────
// Operator overloads for enums
// ──────────────────────────────────────────────

#define ENABLE_INCR_OPERATORS_ON(T)                         \
    constexpr T  operator++(T& d, int) { T old = d; d = T(int(d) + 1); return old; } \
    constexpr T& operator++(T& d) { return d = T(int(d) + 1); }       \
    constexpr T  operator--(T& d, int) { T old = d; d = T(int(d) - 1); return old; } \
    constexpr T& operator--(T& d) { return d = T(int(d) - 1); }

ENABLE_INCR_OPERATORS_ON(PieceType)
ENABLE_INCR_OPERATORS_ON(Piece)
ENABLE_INCR_OPERATORS_ON(Square)
ENABLE_INCR_OPERATORS_ON(File)
ENABLE_INCR_OPERATORS_ON(Rank)

#undef ENABLE_INCR_OPERATORS_ON

constexpr Direction operator+(Direction d1, Direction d2) { return Direction(int(d1) + int(d2)); }
constexpr Direction operator-(Direction d1, Direction d2) { return Direction(int(d1) - int(d2)); }
constexpr Direction operator-(Direction d) { return Direction(-int(d)); }
constexpr Direction operator*(int i, Direction d) { return Direction(i * int(d)); }

constexpr Square operator+(Square s, Direction d) { return Square(int(s) + int(d)); }
constexpr Square operator-(Square s, Direction d) { return Square(int(s) - int(d)); }
constexpr Square& operator+=(Square& s, Direction d) { return s = s + d; }
constexpr Square& operator-=(Square& s, Direction d) { return s = s - d; }

constexpr Color operator~(Color c) { return Color(c ^ BLACK); }

constexpr CastlingRights operator|(CastlingRights cr1, CastlingRights cr2) {
    return CastlingRights(int(cr1) | int(cr2));
}
constexpr CastlingRights operator&(CastlingRights cr1, CastlingRights cr2) {
    return CastlingRights(int(cr1) & int(cr2));
}
constexpr CastlingRights& operator|=(CastlingRights& cr1, CastlingRights cr2) {
    return cr1 = cr1 | cr2;
}
constexpr CastlingRights& operator&=(CastlingRights& cr1, CastlingRights cr2) {
    return cr1 = cr1 & cr2;
}
constexpr CastlingRights operator~(CastlingRights cr) {
    return CastlingRights(~int(cr) & 15);
}

constexpr Value operator+(Value v1, Value v2) { return Value(int(v1) + int(v2)); }
constexpr Value operator-(Value v1, Value v2) { return Value(int(v1) - int(v2)); }
constexpr Value operator-(Value v) { return Value(-int(v)); }
constexpr Value operator+(Value v, int i) { return Value(int(v) + i); }
constexpr Value operator-(Value v, int i) { return Value(int(v) - i); }
constexpr Value operator+(int i, Value v) { return Value(i + int(v)); }
constexpr Value& operator+=(Value& v1, Value v2) { return v1 = v1 + v2; }
constexpr Value& operator-=(Value& v1, Value v2) { return v1 = v1 - v2; }
constexpr Value& operator+=(Value& v, int i) { return v = Value(int(v) + i); }
constexpr Value operator*(int i, Value v) { return Value(i * int(v)); }
constexpr Value operator*(Value v, int i) { return Value(int(v) * i); }
constexpr Value operator/(Value v, int i) { return Value(int(v) / i); }
constexpr Value& operator*=(Value& v, int i) { return v = v * i; }

constexpr Depth operator+(Depth d1, int i) { return Depth(int(d1) + i); }
constexpr Depth operator-(Depth d1, int i) { return Depth(int(d1) - i); }
constexpr Depth& operator+=(Depth& d, int i) { return d = d + i; }
constexpr Depth& operator-=(Depth& d, int i) { return d = d - i; }

// ──────────────────────────────────────────────
// Utility functions
// ──────────────────────────────────────────────

constexpr Square make_square(File f, Rank r) {
    return Square((r << 3) + f);
}

constexpr File file_of(Square s) { return File(s & 7); }
constexpr Rank rank_of(Square s) { return Rank(s >> 3); }

constexpr Rank relative_rank(Color c, Rank r) {
    return Rank(r ^ (c * 7));
}

constexpr Rank relative_rank(Color c, Square s) {
    return relative_rank(c, rank_of(s));
}

constexpr Square relative_square(Color c, Square s) {
    return Square(s ^ (c * 56));
}

constexpr Direction pawn_push(Color c) {
    return c == WHITE ? NORTH : SOUTH;
}

constexpr Piece make_piece(Color c, PieceType pt) {
    return Piece((c << 3) + pt);
}

constexpr PieceType type_of(Piece pc) {
    return PieceType(pc & 7);
}

constexpr Color color_of(Piece pc) {
    assert(pc != NO_PIECE);
    return Color(pc >> 3);
}

constexpr bool is_ok(Square s) {
    return s >= SQ_A1 && s <= SQ_H8;
}

constexpr bool is_ok(Move m) {
    return m != MOVE_NONE && m != MOVE_NULL;
}

// Move construction/extraction
constexpr Move make_move(Square from, Square to) {
    return Move((from << 6) + to);
}

template<MoveType T>
constexpr Move make(Square from, Square to, PieceType pt = KNIGHT) {
    return Move(T + ((pt - KNIGHT) << 12) + (from << 6) + to);
}

constexpr Square from_sq(Move m) {
    return Square((m >> 6) & 0x3F);
}

constexpr Square to_sq(Move m) {
    return Square(m & 0x3F);
}

constexpr int from_to(Move m) {
    return m & 0xFFF;
}

constexpr MoveType type_of(Move m) {
    return MoveType(m & (3 << 14));
}

constexpr PieceType promotion_type(Move m) {
    return PieceType(((m >> 12) & 3) + KNIGHT);
}

// Value helpers
constexpr Value mate_in(int ply) { return Value(int(VALUE_MATE) - ply); }
constexpr Value mated_in(int ply) { return Value(-int(VALUE_MATE) + ply); }

// Piece values lookup
constexpr Value PieceValue[PIECE_TYPE_NB] = {
    VALUE_ZERO, Value(PawnValue), Value(KnightValue), Value(BishopValue),
    Value(RookValue), Value(QueenValue), VALUE_ZERO, VALUE_ZERO
};

// ──────────────────────────────────────────────
// Structs
// ──────────────────────────────────────────────

/// ScoredMove for move ordering
struct ScoredMove {
    Move move;
    int  score;

    operator Move() const { return move; }
    void operator=(Move m) { move = m; }
};

inline bool operator<(const ScoredMove& a, const ScoredMove& b) {
    return a.score < b.score;
}

} // namespace PurnaFish
