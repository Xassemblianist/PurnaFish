/*
 * PurnaFish Chess Engine
 * eval/hce.cpp — Hand-Crafted Evaluation with full evaluation terms
 *
 * Evaluation is done in centipawns from the perspective of the side to move.
 * Uses tapered evaluation (midgame vs endgame interpolation).
 */

#include "hce.hpp"
#include "../bitboard.hpp"

namespace PurnaFish::HCE {

namespace {

// ──────────────────────────────────────────────
// Piece-Square Tables (Midgame, Endgame)
// Values from White's perspective, a1=index 0
// ──────────────────────────────────────────────

struct Score {
    int mg, eg;
    Score operator+(const Score& o) const { return {mg + o.mg, eg + o.eg}; }
    Score operator-(const Score& o) const { return {mg - o.mg, eg - o.eg}; }
    Score operator*(int s) const { return {mg * s, eg * s}; }
    Score& operator+=(const Score& o) { mg += o.mg; eg += o.eg; return *this; }
    Score& operator-=(const Score& o) { mg -= o.mg; eg -= o.eg; return *this; }
};

constexpr Score S(int mg, int eg) { return {mg, eg}; }

// Material values
constexpr Score PieceVal[7] = {
    S(0, 0),        // NO_PIECE_TYPE
    S(126, 208),    // PAWN
    S(781, 854),    // KNIGHT
    S(825, 915),    // BISHOP
    S(1276, 1380),  // ROOK
    S(2538, 2682),  // QUEEN
    S(0, 0),        // KING
};

// PST for pawns (from white's a1 perspective)
constexpr int PawnPST_MG[64] = {
      0,   0,   0,   0,   0,   0,   0,   0,
     -1,  -7, -11, -35, -13,   5,   3,  -5,
     -6,  -8,   5,  11,  -6,   7,   4, -14,
      0,  -5,  11,  22,  26,  17,  -8,  -3,
     -9,  -5,  18,  31,  30,  17,  -6, -14,
     -4,  -5, -12, -10,   6,   9,  12,   5,
     -8,  10,  -8, -18, -16,   5,  16,  13,
      0,   0,   0,   0,   0,   0,   0,   0,
};

constexpr int PawnPST_EG[64] = {
      0,   0,   0,   0,   0,   0,   0,   0,
     -8,   0,  -3,  -3,   4,  -6,  -6, -11,
    -17, -14,  -8,   3,  -2, -13, -13, -20,
     -3,   0,   2,   8,   7,  -1,  -1,  -5,
     10,   3,  -2,  -8,  -6,   1,   3,  13,
     21,  18,   6,  -7,  -2,  11,  15,  25,
     27,  24,  17,  15,  15,  15,  22,  28,
      0,   0,   0,   0,   0,   0,   0,   0,
};

constexpr int KnightPST_MG[64] = {
   -175, -92, -74, -73, -73, -74, -92,-175,
    -77, -41, -27, -15, -15, -27, -41, -77,
    -61, -17,   6,  12,  12,   6, -17, -61,
    -35,   8,  40,  49,  49,  40,   8, -35,
    -34,  13,  44,  51,  51,  44,  13, -34,
    -29,  25,  54,  57,  57,  54,  25, -29,
    -51,  -7,  41,  25,  25,  41,  -7, -51,
   -167, -89, -34, -49, -49, -34, -89,-167,
};

constexpr int KnightPST_EG[64] = {
    -96, -65, -49, -21, -21, -49, -65, -96,
    -67, -54, -18,   8,   8, -18, -54, -67,
    -40, -27,  -8,  29,  29,  -8, -27, -40,
    -35,  -2,  13,  28,  28,  13,  -2, -35,
    -45, -16,   9,  39,  39,   9, -16, -45,
    -51, -44,  -4,   2,   2,  -4, -44, -51,
    -69, -50, -51,  12,  12, -51, -50, -69,
   -105, -82, -46, -14, -14, -46, -82,-105,
};

constexpr int BishopPST_MG[64] = {
    -53,  -5,  -8, -23, -23,  -8,  -5, -53,
    -15,   8,  19,  -4,  -4,  19,   8, -15,
     -7,  21,  -5,  17,  17,  -5,  21,  -7,
     -5,  11,  25,  39,  39,  25,  11,  -5,
    -12,  29,  22,  31,  31,  22,  29, -12,
    -16,   6,   1,  11,  11,   1,   6, -16,
    -17, -14,   5,   0,   0,   5, -14, -17,
    -48,   1, -14, -23, -23, -14,   1, -48,
};

constexpr int BishopPST_EG[64] = {
    -57, -30, -37, -12, -12, -37, -30, -57,
    -37, -13, -17,   1,   1, -17, -13, -37,
    -16,  -1,  -2,  10,  10,  -2,  -1, -16,
     -6,  -2,   6,  16,  16,   6,  -2,  -6,
    -16,  -1,   5,  16,  16,   5,  -1, -16,
    -16, -18,   0,   7,   7,   0, -18, -16,
    -13, -11, -11,   4,   4, -11, -11, -13,
    -40, -21, -12,  -1,  -1, -12, -21, -40,
};

constexpr int RookPST_MG[64] = {
    -31, -20, -14,  -5,  -5, -14, -20, -31,
    -21, -13,  -8,   6,   6,  -8, -13, -21,
    -25, -11,  -1,   3,   3,  -1, -11, -25,
    -13,  -5,  -4,  -6,  -6,  -4,  -5, -13,
    -27, -15,  -4,   3,   3,  -4, -15, -27,
    -22,  -2,   6,  12,  12,   6,  -2, -22,
     -2,  12,  16,  18,  18,  16,  12,  -2,
    -17, -19,  -1,   9,   9,  -1, -19, -17,
};

constexpr int RookPST_EG[64] = {
     -9, -13, -10,  -9,  -9, -10, -13,  -9,
    -12,  -9,  -1,  -2,  -2,  -1,  -9, -12,
      6,  -8,  -2,  -6,  -6,  -2,  -8,   6,
     -6,   1,  -9,   7,   7,  -9,   1,  -6,
     -5,   8,   7,  -6,  -6,   7,   8,  -5,
      6,   1,  -7,  10,  10,  -7,   1,   6,
      4,   5,  20,  -5,  -5,  20,   5,   4,
     18,   0,  19,  13,  13,  19,   0,  18,
};

constexpr int QueenPST_MG[64] = {
      3,  -5,  -5,   4,   4,  -5,  -5,   3,
     -3,   5,   8,  12,  12,   8,   5,  -3,
     -3,   6,  13,   7,   7,  13,   6,  -3,
      4,   5,   9,   8,   8,   9,   5,   4,
      0,  14,  12,   5,   5,  12,  14,   0,
     -4,  10,   6,   8,   8,   6,  10,  -4,
     -5,   6,  10,   8,   8,  10,   6,  -5,
     -2,  -2,   1,  -2,  -2,   1,  -2,  -2,
};

constexpr int QueenPST_EG[64] = {
    -69, -57, -47, -26, -26, -47, -57, -69,
    -55, -31, -22,  -4,  -4, -22, -31, -55,
    -39, -18,  -9,   3,   3,  -9, -18, -39,
    -23,  -3,  13,  24,  24,  13,  -3, -23,
    -29,  -6,   9,  21,  21,   9,  -6, -29,
    -38, -18, -12,   1,   1, -12, -18, -38,
    -50, -27, -24,  -8,  -8, -24, -27, -50,
    -75, -52, -43, -36, -36, -43, -52, -75,
};

constexpr int KingPST_MG[64] = {
    271, 327, 271, 198, 198, 271, 327, 271,
    278, 303, 234, 179, 179, 234, 303, 278,
    195, 258, 169, 120, 120, 169, 258, 195,
    164, 190, 138,  98,  98, 138, 190, 164,
    154, 179, 105,  70,  70, 105, 179, 154,
    123, 145,  81,  31,  31,  81, 145, 123,
     88, 120,  65,  33,  33,  65, 120,  88,
     59,  89,  45,  -1,  -1,  45,  89,  59,
};

constexpr int KingPST_EG[64] = {
      1,  45,  85,  76,  76,  85,  45,   1,
     53,  82, 112, 100, 100, 112,  82,  53,
     88, 130, 169, 175, 175, 169, 130,  88,
    103, 156, 172, 172, 172, 172, 156, 103,
     96, 166, 199, 199, 199, 199, 166,  96,
     92, 172, 184, 191, 191, 184, 172,  92,
     47, 121, 116, 131, 131, 116, 121,  47,
     11,  59,  73,  78,  78,  73,  59,  11,
};

const int* PST_MG[7] = {nullptr, PawnPST_MG, KnightPST_MG, BishopPST_MG,
                         RookPST_MG, QueenPST_MG, KingPST_MG};
const int* PST_EG[7] = {nullptr, PawnPST_EG, KnightPST_EG, BishopPST_EG,
                         RookPST_EG, QueenPST_EG, KingPST_EG};

// ──────────────────────────────────────────────
// Evaluation helpers
// ──────────────────────────────────────────────

int phase_value(const Position& pos) {
    int phase = 0;
    phase += popcount(pos.pieces(KNIGHT)) * 1;
    phase += popcount(pos.pieces(BISHOP)) * 1;
    phase += popcount(pos.pieces(ROOK))   * 2;
    phase += popcount(pos.pieces(QUEEN))  * 4;
    return std::min(phase, 24);
}

Score eval_material_and_pst(const Position& pos) {
    Score score = S(0, 0);

    for (PieceType pt = PAWN; pt <= KING; ++pt) {
        Bitboard wbb = pos.pieces(WHITE, pt);
        Bitboard bbb = pos.pieces(BLACK, pt);

        while (wbb) {
            Square s = pop_lsb(wbb);
            score += PieceVal[pt];
            score += S(PST_MG[pt][s], PST_EG[pt][s]);
        }
        while (bbb) {
            Square s = pop_lsb(bbb);
            // Flip square for black (mirror rank)
            Square flipped = Square(s ^ 56);
            score -= PieceVal[pt];
            score -= S(PST_MG[pt][flipped], PST_EG[pt][flipped]);
        }
    }

    return score;
}

Score eval_pawns(const Position& pos) {
    Score score = S(0, 0);

    for (Color c : {WHITE, BLACK}) {
        int sign = (c == WHITE) ? 1 : -1;
        Bitboard our_pawns   = pos.pieces(c, PAWN);
        Bitboard their_pawns = pos.pieces(~c, PAWN);

        Bitboard pawns = our_pawns;
        while (pawns) {
            Square s = pop_lsb(pawns);
            File f = file_of(s);

            // Isolated pawn
            Bitboard adj = adjacent_files_bb(s);
            if (!(adj & our_pawns))
                score += S(-15, -20) * sign;

            // Doubled pawn
            if (more_than_one(our_pawns & FileBB[f]))
                score += S(-11, -56) * sign;

            // Passed pawn
            if (!(passed_pawn_span(c, s) & their_pawns)) {
                Rank r = relative_rank(c, s);
                static const int PassedMG[8] = {0, 5, 10, 20, 40, 65, 95, 0};
                static const int PassedEG[8] = {0, 10, 20, 45, 80, 130, 190, 0};
                score += S(PassedMG[r], PassedEG[r]) * sign;
            }

            // Connected pawns
            if (pawn_attacks_bb(~c, s) & our_pawns) {
                Rank r = relative_rank(c, s);
                score += S(7 + 4 * r, 5 + 3 * r) * sign;
            }

            // Backward pawn: can't advance because square ahead is attacked by enemy pawns
            // and has no friendly pawn support behind it
            {
                Square ahead = s + pawn_push(c);
                if (is_ok(ahead) && !pos.piece_on(ahead)) {
                    Bitboard stop_attacks = pawn_attacks_bb(c, ahead) & their_pawns;
                    Bitboard support = pawn_attacks_bb(~c, s) & our_pawns;
                    if (stop_attacks && !support)
                        score += S(-12, -14) * sign;
                }
            }
        }
    }

    return score;
}

Score eval_pieces(const Position& pos) {
    Score score = S(0, 0);

    for (Color c : {WHITE, BLACK}) {
        int sign = (c == WHITE) ? 1 : -1;
        Bitboard occupied = pos.pieces();

        // Bishop pair
        if (popcount(pos.pieces(c, BISHOP)) >= 2)
            score += S(43, 60) * sign;

        // Knight outpost: advanced knight on square not attacked by enemy pawns
        Bitboard their_pawns = pos.pieces(~c, PAWN);
        Bitboard knightBB = pos.pieces(c, KNIGHT);
        Bitboard nbb = knightBB;
        while (nbb) {
            Square s = pop_lsb(nbb);
            Rank r = relative_rank(c, s);
            if (r >= RANK_4) {
                if (!(pawn_attacks_bb(c, s) & their_pawns)) {
                    score += S(15 + 5 * (r - RANK_4), 10 + 5 * (r - RANK_4)) * sign;
                }
            }
        }

        // Rook on open/semi-open file
        Bitboard rooks = pos.pieces(c, ROOK);
        while (rooks) {
            Square s = pop_lsb(rooks);
            File f = file_of(s);

            if (!(pos.pieces(PAWN) & FileBB[f]))
                score += S(47, 25) * sign;
            else if (!(pos.pieces(c, PAWN) & FileBB[f]))
                score += S(20, 10) * sign;

            // Rook on 7th rank
            if (relative_rank(c, s) == RANK_7)
                score += S(20, 40) * sign;
        }

        // Knight mobility
        Bitboard knights = pos.pieces(c, KNIGHT);
        while (knights) {
            Square s = pop_lsb(knights);
            int mob = popcount(KnightAttacks[s] & ~pos.pieces(c));
            score += S(4 * (mob - 4), 4 * (mob - 4)) * sign;
        }

        // Bishop mobility
        Bitboard bishops = pos.pieces(c, BISHOP);
        while (bishops) {
            Square s = pop_lsb(bishops);
            int mob = popcount(attacks_bb<BISHOP>(s, occupied) & ~pos.pieces(c));
            score += S(5 * (mob - 6), 5 * (mob - 6)) * sign;
        }

        // Rook mobility
        Bitboard rookBB = pos.pieces(c, ROOK);
        while (rookBB) {
            Square s = pop_lsb(rookBB);
            int mob = popcount(attacks_bb<ROOK>(s, occupied) & ~pos.pieces(c));
            score += S(3 * (mob - 7), 3 * (mob - 7)) * sign;
        }

        // Queen mobility
        Bitboard queens = pos.pieces(c, QUEEN);
        while (queens) {
            Square s = pop_lsb(queens);
            int mob = popcount(attacks_bb<QUEEN>(s, occupied) & ~pos.pieces(c));
            score += S(1 * (mob - 14), 2 * (mob - 14)) * sign;
        }
    }

    return score;
}

Score eval_king_safety(const Position& pos) {
    Score score = S(0, 0);

    for (Color c : {WHITE, BLACK}) {
        int sign = (c == WHITE) ? 1 : -1;
        Square ksq = pos.square(c, KING);
        File kf = file_of(ksq);

        // Pawn shield
        Bitboard shield_zone = (kf > FILE_A ? FileBB[kf - 1] : 0)
                             | FileBB[kf]
                             | (kf < FILE_H ? FileBB[kf + 1] : 0);

        Bitboard shield_pawns = shield_zone & pos.pieces(c, PAWN);
        int shield_count = popcount(shield_pawns);
        score += S(15 * shield_count, 0) * sign;

        // King attack zone
        Bitboard king_ring = KingAttacks[ksq];

        // Count attackers to the king zone
        Bitboard occupied = pos.pieces();
        int attack_count = 0;
        int attack_weight = 0;

        // Knights attacking king zone
        Bitboard enemy_knights = pos.pieces(~c, KNIGHT);
        while (enemy_knights) {
            Square s = pop_lsb(enemy_knights);
            if (KnightAttacks[s] & king_ring) {
                attack_count++;
                attack_weight += 52;
            }
        }

        // Bishops attacking king zone
        Bitboard enemy_bishops = pos.pieces(~c, BISHOP);
        while (enemy_bishops) {
            Square s = pop_lsb(enemy_bishops);
            if (attacks_bb<BISHOP>(s, occupied) & king_ring) {
                attack_count++;
                attack_weight += 44;
            }
        }

        // Rooks attacking king zone
        Bitboard enemy_rooks = pos.pieces(~c, ROOK);
        while (enemy_rooks) {
            Square s = pop_lsb(enemy_rooks);
            if (attacks_bb<ROOK>(s, occupied) & king_ring) {
                attack_count++;
                attack_weight += 82;
            }
        }

        // Queens attacking king zone
        Bitboard enemy_queens = pos.pieces(~c, QUEEN);
        while (enemy_queens) {
            Square s = pop_lsb(enemy_queens);
            if (attacks_bb<QUEEN>(s, occupied) & king_ring) {
                attack_count++;
                attack_weight += 128;
            }
        }

        if (attack_count >= 1) {
            int safety = attack_weight;
            // Open files near king penalty
            for (int df = -1; df <= 1; ++df) {
                int nf = kf + df;
                if (nf >= 0 && nf < 8 && !(pos.pieces(PAWN) & FileBB[nf]))
                    safety += 25;
            }
            score -= S(safety / 3, safety / 8) * sign;
        }
    }

    return score;
}

} // anonymous namespace

Score eval_space(const Position& pos) {
    Score score = S(0, 0);
    if (pos.non_pawn_material(WHITE) + pos.non_pawn_material(BLACK) < 2 * QueenValue)
        return score;

    // Central files c-f
    const Bitboard CenterFiles = FileBB[FILE_C] | FileBB[FILE_D] | FileBB[FILE_E] | FileBB[FILE_F];

    for (Color c : {WHITE, BLACK}) {
        int sign = (c == WHITE) ? 1 : -1;
        // Space region: central files, ranks 2-4 for white, ranks 7-5 for black
        Bitboard space_region = CenterFiles & (c == WHITE
            ? (Rank2BB | Rank3BB | Rank4BB)
            : (Rank7BB | Rank6BB | Rank5BB));

        // Enemy pawn attacks via shift
        Bitboard ep = pos.pieces(~c, PAWN);
        Bitboard enemy_attacks = (c == WHITE)
            ? (shift<SOUTH_EAST>(ep) | shift<SOUTH_WEST>(ep))
            : (shift<NORTH_EAST>(ep) | shift<NORTH_WEST>(ep));

        Bitboard safe = space_region & ~pos.pieces(c, PAWN) & ~enemy_attacks;
        int space = popcount(safe) + popcount(safe & pos.pieces(c));
        score += S(space, 0) * sign;
    }
    return score;
}

Value evaluate(const Position& pos) {
    Score score = eval_material_and_pst(pos);
    score += eval_pawns(pos);
    score += eval_pieces(pos);
    score += eval_king_safety(pos);
    score += eval_space(pos);

    // Tempo bonus
    score += S(20, 10);

    // Tapered evaluation
    int phase = phase_value(pos);
    int mg = score.mg;
    int eg = score.eg;
    int eval = (mg * phase + eg * (24 - phase)) / 24;

    // Return from side-to-move perspective
    return Value(pos.side_to_move() == WHITE ? eval : -eval);
}

} // namespace PurnaFish::HCE
