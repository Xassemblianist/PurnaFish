/*
 * PurnaFish Chess Engine
 * bitboard.cpp — Magic bitboard initialization and attack table generation
 */

#include "bitboard.hpp"
#include "misc.hpp"
#include <sstream>

namespace PurnaFish {

Bitboard FileBB[8];
Bitboard RankBB[8];
Bitboard PawnAttacks[2][64];
Bitboard KnightAttacks[64];
Bitboard KingAttacks[64];
Bitboard PseudoAttacks[8][64];
Bitboard BetweenBB[64][64];
Bitboard LineBB[64][64];
uint8_t  SquareDistance[64][64];

Magic    BishopMagics[64];
Magic    RookMagics[64];
Bitboard BishopTable[0x1480];
Bitboard RookTable[0x19000];

namespace {

// Sliding attacks computation (for initialization only)
Bitboard sliding_attack(PieceType pt, Square sq, Bitboard occupied) {
    Bitboard attacks = 0;
    Direction rook_dirs[4]   = {NORTH, SOUTH, EAST, WEST};
    Direction bishop_dirs[4] = {NORTH_EAST, SOUTH_EAST, SOUTH_WEST, NORTH_WEST};
    Direction* dirs = (pt == ROOK) ? rook_dirs : bishop_dirs;

    for (int i = 0; i < 4; ++i) {
        Square s = sq;
        while (true) {
            s += dirs[i];
            if (!is_ok(s) || distance(s, s - dirs[i]) > 2)
                break;
            attacks |= square_bb(s);
            if (occupied & square_bb(s))
                break;
        }
    }
    return attacks;
}

// Pre-computed magic numbers (well-tested values)
// These are known good magics for each square
constexpr Bitboard RookMagicNumbers[64] = {
    0x0080001020400080ULL, 0x0040001000200040ULL, 0x0080081000200080ULL, 0x0080040800100080ULL,
    0x0080020400080080ULL, 0x0080010200040080ULL, 0x0080008001000200ULL, 0x0080002040800100ULL,
    0x0000800020400080ULL, 0x0000400020005000ULL, 0x0000801000200080ULL, 0x0000800800100080ULL,
    0x0000800400080080ULL, 0x0000800200040080ULL, 0x0000800100020080ULL, 0x0000800040800100ULL,
    0x0000208000400080ULL, 0x0000404000201000ULL, 0x0000808010002000ULL, 0x0000808008001000ULL,
    0x0000808004000800ULL, 0x0000808002000400ULL, 0x0000010100020004ULL, 0x0000020000408104ULL,
    0x0000208080004000ULL, 0x0000200040005000ULL, 0x0000100080200080ULL, 0x0000080080100080ULL,
    0x0000040080080080ULL, 0x0000020080040080ULL, 0x0000010080800200ULL, 0x0000800080004100ULL,
    0x0000204000800080ULL, 0x0000200040401000ULL, 0x0000100080802000ULL, 0x0000080080801000ULL,
    0x0000040080800800ULL, 0x0000020080800400ULL, 0x0000020001010004ULL, 0x0000800040800100ULL,
    0x0000204000808000ULL, 0x0000200040008080ULL, 0x0000100020008080ULL, 0x0000080010008080ULL,
    0x0000040008008080ULL, 0x0000020004008080ULL, 0x0000010002008080ULL, 0x0000004081020004ULL,
    0x0000204000800080ULL, 0x0000200040008080ULL, 0x0000100020008080ULL, 0x0000080010008080ULL,
    0x0000040008008080ULL, 0x0000020004008080ULL, 0x0000800100020080ULL, 0x0000800041000080ULL,
    0x00FFFCDDFCED714AULL, 0x007FFCDDFCED714AULL, 0x003FFFCDFFD88096ULL, 0x0000040810002101ULL,
    0x0001000204080011ULL, 0x0001000204000801ULL, 0x0001000082000401ULL, 0x0001FFFAABFAD1A2ULL,
};

constexpr Bitboard BishopMagicNumbers[64] = {
    0x0002020202020200ULL, 0x0002020202020000ULL, 0x0004010202000000ULL, 0x0004040080000000ULL,
    0x0001104000000000ULL, 0x0000821040000000ULL, 0x0000410410400000ULL, 0x0000104104104000ULL,
    0x0000040404040400ULL, 0x0000020202020200ULL, 0x0000040102020000ULL, 0x0000040400800000ULL,
    0x0000011040000000ULL, 0x0000008210400000ULL, 0x0000004104104000ULL, 0x0000002082082000ULL,
    0x0004000808080800ULL, 0x0002000404040400ULL, 0x0001000202020200ULL, 0x0000800802004000ULL,
    0x0000800400A00000ULL, 0x0000200100884000ULL, 0x0000400082082000ULL, 0x0000200041041000ULL,
    0x0002080010101000ULL, 0x0001040008080800ULL, 0x0000208004010400ULL, 0x0000404004010200ULL,
    0x0000840000802000ULL, 0x0000404002011000ULL, 0x0000808001041000ULL, 0x0000404000820800ULL,
    0x0001041000202000ULL, 0x0000820800101000ULL, 0x0000104400080800ULL, 0x0000020080080080ULL,
    0x0000404040040100ULL, 0x0000808100020100ULL, 0x0001010100020800ULL, 0x0000808080010400ULL,
    0x0000820820004000ULL, 0x0000410410002000ULL, 0x0000208208001000ULL, 0x0000002084000800ULL,
    0x0000000020880000ULL, 0x0000001002020000ULL, 0x0000040408020000ULL, 0x0000040820020000ULL,
    0x0001010101000200ULL, 0x0000505005000100ULL, 0x0002020202000100ULL, 0x0000104104100020ULL,
    0x0000002082000810ULL, 0x0000000020841000ULL, 0x0000000000208800ULL, 0x0000000010020200ULL,
    0x0000000404080200ULL, 0x0000000020200200ULL, 0x0000000010020080ULL, 0x0000000008020100ULL,
    0x0000000004010040ULL, 0x0000000002080020ULL, 0x0000000000400810ULL, 0x0000000000200204ULL,
};

constexpr int RookBits[64] = {
    12, 11, 11, 11, 11, 11, 11, 12,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    12, 11, 11, 11, 11, 11, 11, 12,
};

constexpr int BishopBits[64] = {
    6, 5, 5, 5, 5, 5, 5, 6,
    5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 7, 7, 7, 7, 5, 5,
    5, 5, 7, 9, 9, 7, 5, 5,
    5, 5, 7, 9, 9, 7, 5, 5,
    5, 5, 7, 7, 7, 7, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5,
    6, 5, 5, 5, 5, 5, 5, 6,
};

Bitboard init_magics_mask(PieceType pt, Square s) {
    Bitboard edges = ((Rank1BB | Rank8BB) & ~RankBB[rank_of(s)]) |
                     ((FileABB | FileHBB) & ~FileBB[file_of(s)]);
    return sliding_attack(pt, s, 0) & ~edges;
}

void init_magics(PieceType pt, Bitboard table[], Magic magics[],
                 const Bitboard magic_numbers[], const int bits[]) {
    Bitboard occupancy[4096], reference[4096];
    int size = 0;

    for (Square s = SQ_A1; s <= SQ_H8; ++s) {
        Magic& m = magics[s];
        m.mask  = init_magics_mask(pt, s);
        m.shift = 64 - bits[s];
        m.magic = magic_numbers[s];

        // Set pointer into the shared attack table
        m.attacks = s == SQ_A1 ? table : magics[s - 1].attacks + size;

        // Enumerate all subsets of the mask
        Bitboard b = 0;
        size = 0;
        do {
            occupancy[size] = b;
            reference[size] = sliding_attack(pt, s, b);
            size++;
            b = (b - m.mask) & m.mask; // Carry-Rippler
        } while (b);

        // Fill the attack table
        for (int i = 0; i < size; ++i) {
            unsigned idx = m.index(occupancy[i]);
            m.attacks[idx] = reference[i];
        }
    }
}

} // anonymous namespace

void Bitboards::init() {
    // File and Rank bitboards
    for (int i = 0; i < 8; ++i) {
        FileBB[i] = FileABB << i;
        RankBB[i] = Rank1BB << (8 * i);
    }

    // Square distance
    for (Square s1 = SQ_A1; s1 <= SQ_H8; ++s1)
        for (Square s2 = SQ_A1; s2 <= SQ_H8; ++s2)
            SquareDistance[s1][s2] = std::max(std::abs(file_of(s1) - file_of(s2)),
                                              std::abs(rank_of(s1) - rank_of(s2)));

    // Knight attacks
    for (Square s = SQ_A1; s <= SQ_H8; ++s) {
        int r = rank_of(s), f = file_of(s);
        KnightAttacks[s] = 0;
        int offsets[8][2] = {{-2,-1},{-2,1},{-1,-2},{-1,2},{1,-2},{1,2},{2,-1},{2,1}};
        for (auto& [dr, df] : offsets) {
            int nr = r + dr, nf = f + df;
            if (nr >= 0 && nr < 8 && nf >= 0 && nf < 8)
                KnightAttacks[s] |= square_bb(make_square(File(nf), Rank(nr)));
        }
    }

    // King attacks
    for (Square s = SQ_A1; s <= SQ_H8; ++s) {
        int r = rank_of(s), f = file_of(s);
        KingAttacks[s] = 0;
        for (int dr = -1; dr <= 1; ++dr)
            for (int df = -1; df <= 1; ++df) {
                if (dr == 0 && df == 0) continue;
                int nr = r + dr, nf = f + df;
                if (nr >= 0 && nr < 8 && nf >= 0 && nf < 8)
                    KingAttacks[s] |= square_bb(make_square(File(nf), Rank(nr)));
            }
    }

    // Pawn attacks
    for (Square s = SQ_A1; s <= SQ_H8; ++s) {
        Bitboard bb = square_bb(s);
        PawnAttacks[WHITE][s] = shift<NORTH_WEST>(bb) | shift<NORTH_EAST>(bb);
        PawnAttacks[BLACK][s] = shift<SOUTH_WEST>(bb) | shift<SOUTH_EAST>(bb);
    }

    // Initialize magic bitboards
    init_magics(ROOK,   RookTable,   RookMagics,   RookMagicNumbers,   RookBits);
    init_magics(BISHOP, BishopTable, BishopMagics, BishopMagicNumbers, BishopBits);

    // Pseudo attacks and lines
    for (Square s1 = SQ_A1; s1 <= SQ_H8; ++s1) {
        PseudoAttacks[BISHOP][s1] = attacks_bb<BISHOP>(s1, 0);
        PseudoAttacks[ROOK][s1]   = attacks_bb<ROOK>(s1, 0);
        PseudoAttacks[QUEEN][s1]  = PseudoAttacks[BISHOP][s1] | PseudoAttacks[ROOK][s1];
        PseudoAttacks[KNIGHT][s1] = KnightAttacks[s1];
        PseudoAttacks[KING][s1]   = KingAttacks[s1];

        for (Square s2 = SQ_A1; s2 <= SQ_H8; ++s2) {
            if (PseudoAttacks[BISHOP][s1] & square_bb(s2)) {
                LineBB[s1][s2] = (attacks_bb<BISHOP>(s1, 0) & attacks_bb<BISHOP>(s2, 0))
                                 | square_bb(s1) | square_bb(s2);
                BetweenBB[s1][s2] = attacks_bb<BISHOP>(s1, square_bb(s2))
                                  & attacks_bb<BISHOP>(s2, square_bb(s1));
            } else if (PseudoAttacks[ROOK][s1] & square_bb(s2)) {
                LineBB[s1][s2] = (attacks_bb<ROOK>(s1, 0) & attacks_bb<ROOK>(s2, 0))
                                 | square_bb(s1) | square_bb(s2);
                BetweenBB[s1][s2] = attacks_bb<ROOK>(s1, square_bb(s2))
                                  & attacks_bb<ROOK>(s2, square_bb(s1));
            }
        }
    }
}

std::string pretty(Bitboard b) {
    std::ostringstream ss;
    ss << "+---+---+---+---+---+---+---+---+\n";
    for (Rank r = RANK_8; r >= RANK_1; --r) {
        for (File f = FILE_A; f <= FILE_H; ++f)
            ss << (b & square_bb(make_square(f, r)) ? "| X " : "|   ");
        ss << "| " << (1 + r) << "\n+---+---+---+---+---+---+---+---+\n";
    }
    ss << "  a   b   c   d   e   f   g   h\n";
    return ss.str();
}

} // namespace PurnaFish
