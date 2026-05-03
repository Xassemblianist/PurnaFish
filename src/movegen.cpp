/*
 * PurnaFish Chess Engine
 * movegen.cpp — Move generation implementation
 */

#include "movegen.hpp"
#include "bitboard.hpp"

namespace PurnaFish {

namespace {

template<Color Us, GenType Type>
ScoredMove* generate_pawn_moves(const Position& pos, ScoredMove* moveList, Bitboard target) {
    constexpr Color     Them     = ~Us;
    constexpr Bitboard  TRank7BB = (Us == WHITE ? Rank7BB : Rank2BB);
    constexpr Bitboard  TRank3BB = (Us == WHITE ? Rank3BB : Rank6BB);
    constexpr Direction Up       = pawn_push(Us);
    constexpr Direction UpRight  = (Us == WHITE ? NORTH_EAST : SOUTH_WEST);
    constexpr Direction UpLeft   = (Us == WHITE ? NORTH_WEST : SOUTH_EAST);

    Bitboard emptySquares = ~pos.pieces();
    Bitboard enemies = (Type == EVASIONS ? pos.pieces(Them) & target : pos.pieces(Them));
    Bitboard pawnsOn7    = pos.pieces(Us, PAWN) & TRank7BB;
    Bitboard pawnsNotOn7 = pos.pieces(Us, PAWN) & ~TRank7BB;

    // Single and double pawn pushes (quiet moves)
    if constexpr (Type != CAPTURES) {
        Bitboard b1 = shift<Up>(pawnsNotOn7) & emptySquares;
        Bitboard b2 = shift<Up>(b1 & TRank3BB) & emptySquares;

        if constexpr (Type == EVASIONS) {
            b1 &= target;
            b2 &= target;
        }

        while (b1) {
            Square to = pop_lsb(b1);
            *moveList++ = {make_move(to - Up, to), 0};
        }
        while (b2) {
            Square to = pop_lsb(b2);
            *moveList++ = {make_move(to - Up - Up, to), 0};
        }
    }

    // Promotions
    if (pawnsOn7) {
        Bitboard b1 = shift<UpRight>(pawnsOn7) & enemies;
        Bitboard b2 = shift<UpLeft>(pawnsOn7)  & enemies;
        Bitboard b3 = shift<Up>(pawnsOn7)       & emptySquares;

        if constexpr (Type == EVASIONS) b3 &= target;

        while (b1) {
            Square to = pop_lsb(b1);
            Square from = to - UpRight;
            *moveList++ = {make<PROMOTION>(from, to, QUEEN),  0};
            *moveList++ = {make<PROMOTION>(from, to, ROOK),   0};
            *moveList++ = {make<PROMOTION>(from, to, BISHOP), 0};
            *moveList++ = {make<PROMOTION>(from, to, KNIGHT), 0};
        }
        while (b2) {
            Square to = pop_lsb(b2);
            Square from = to - UpLeft;
            *moveList++ = {make<PROMOTION>(from, to, QUEEN),  0};
            *moveList++ = {make<PROMOTION>(from, to, ROOK),   0};
            *moveList++ = {make<PROMOTION>(from, to, BISHOP), 0};
            *moveList++ = {make<PROMOTION>(from, to, KNIGHT), 0};
        }
        while (b3) {
            Square to = pop_lsb(b3);
            Square from = to - Up;
            *moveList++ = {make<PROMOTION>(from, to, QUEEN),  0};
            *moveList++ = {make<PROMOTION>(from, to, ROOK),   0};
            *moveList++ = {make<PROMOTION>(from, to, BISHOP), 0};
            *moveList++ = {make<PROMOTION>(from, to, KNIGHT), 0};
        }
    }

    // Captures (non-promotion)
    if constexpr (Type != QUIETS) {
        Bitboard b1 = shift<UpRight>(pawnsNotOn7) & enemies;
        Bitboard b2 = shift<UpLeft>(pawnsNotOn7)  & enemies;

        while (b1) {
            Square to = pop_lsb(b1);
            *moveList++ = {make_move(to - UpRight, to), 0};
        }
        while (b2) {
            Square to = pop_lsb(b2);
            *moveList++ = {make_move(to - UpLeft, to), 0};
        }

        // En passant
        if (pos.ep_square() != SQ_NONE) {
            Bitboard ep_pawns = pawnsNotOn7 & pawn_attacks_bb(Them, pos.ep_square());
            while (ep_pawns) {
                Square from = pop_lsb(ep_pawns);
                *moveList++ = {make<EN_PASSANT>(from, pos.ep_square()), 0};
            }
        }
    }

    return moveList;
}

template<Color Us, PieceType Pt>
ScoredMove* generate_piece_moves(const Position& pos, ScoredMove* moveList, Bitboard target) {
    static_assert(Pt != KING && Pt != PAWN);

    Bitboard bb = pos.pieces(Us, Pt);
    while (bb) {
        Square from = pop_lsb(bb);
        Bitboard attacks = (Pt == KNIGHT) ? KnightAttacks[from]
                         : (Pt == BISHOP) ? attacks_bb<BISHOP>(from, pos.pieces())
                         : (Pt == ROOK)   ? attacks_bb<ROOK>(from, pos.pieces())
                                          : attacks_bb<QUEEN>(from, pos.pieces());
        attacks &= target;

        while (attacks) {
            Square to = pop_lsb(attacks);
            *moveList++ = {make_move(from, to), 0};
        }
    }
    return moveList;
}

template<Color Us, GenType Type>
ScoredMove* generate_all(const Position& pos, ScoredMove* moveList) {
    static_assert(Type != LEGAL);

    Bitboard target;
    if constexpr (Type == CAPTURES)
        target = pos.pieces(~Us);
    else if constexpr (Type == QUIETS || Type == QUIET_CHECKS)
        target = ~pos.pieces();
    else if constexpr (Type == NON_EVASIONS)
        target = ~pos.pieces(Us);
    else if constexpr (Type == EVASIONS) {
        Square ksq = pos.square(Us, KING);
        Bitboard checkers = pos.checkers();

        // Double check: only king moves are possible
        if (more_than_one(checkers)) {
            Bitboard b = KingAttacks[ksq] & ~pos.pieces(Us);
            while (b) {
                Square to = pop_lsb(b);
                *moveList++ = {make_move(ksq, to), 0};
            }
            return moveList;
        }

        // Single check: block or capture the checker
        Square checker_sq = lsb(checkers);
        target = between_bb(ksq, checker_sq) | square_bb(checker_sq);
    }

    moveList = generate_pawn_moves<Us, Type>(pos, moveList, target);
    moveList = generate_piece_moves<Us, KNIGHT>(pos, moveList, target);
    moveList = generate_piece_moves<Us, BISHOP>(pos, moveList, target);
    moveList = generate_piece_moves<Us, ROOK>(pos, moveList, target);
    moveList = generate_piece_moves<Us, QUEEN>(pos, moveList, target);

    // King moves (always generated, except for double check which returns early above)
    Square ksq = pos.square(Us, KING);
    Bitboard b = KingAttacks[ksq] & ~pos.pieces(Us);
    while (b) {
        Square to = pop_lsb(b);
        *moveList++ = {make_move(ksq, to), 0};
    }

    // Castling
    if constexpr (Type != CAPTURES && Type != EVASIONS) {
        if (pos.can_castle(Us == WHITE ? WHITE_OO : BLACK_OO)) {
            Square kfrom = ksq;
            Square rfrom = pos.castling_rook_square(Us == WHITE ? WHITE_OO : BLACK_OO);
            Square kto = make_square(FILE_G, rank_of(kfrom));
            Square rto = make_square(FILE_F, rank_of(kfrom));
            Bitboard path = (between_bb(kfrom, rfrom) | square_bb(kto) | square_bb(rto)) 
                          & ~(square_bb(kfrom) | square_bb(rfrom));
            if (!(path & pos.pieces()))
                *moveList++ = {make<CASTLING>(kfrom, rfrom), 0};
        }
        if (pos.can_castle(Us == WHITE ? WHITE_OOO : BLACK_OOO)) {
            Square kfrom = ksq;
            Square rfrom = pos.castling_rook_square(Us == WHITE ? WHITE_OOO : BLACK_OOO);
            Square kto = make_square(FILE_C, rank_of(kfrom));
            Square rto = make_square(FILE_D, rank_of(kfrom));
            Bitboard path = (between_bb(kfrom, rfrom) | square_bb(kto) | square_bb(rto))
                          & ~(square_bb(kfrom) | square_bb(rfrom));
            if (!(path & pos.pieces()))
                *moveList++ = {make<CASTLING>(kfrom, rfrom), 0};
        }
    }

    return moveList;
}

} // anonymous namespace

// Template instantiations
template<GenType Type>
ScoredMove* generate(const Position& pos, ScoredMove* moveList) {
    static_assert(Type != LEGAL);
    return pos.side_to_move() == WHITE
        ? generate_all<WHITE, Type>(pos, moveList)
        : generate_all<BLACK, Type>(pos, moveList);
}

template ScoredMove* generate<CAPTURES>(const Position&, ScoredMove*);
template ScoredMove* generate<QUIETS>(const Position&, ScoredMove*);
template ScoredMove* generate<QUIET_CHECKS>(const Position&, ScoredMove*);
template ScoredMove* generate<EVASIONS>(const Position&, ScoredMove*);
template ScoredMove* generate<NON_EVASIONS>(const Position&, ScoredMove*);

template<>
ScoredMove* generate<LEGAL>(const Position& pos, ScoredMove* moveList) {
    ScoredMove* cur = moveList;
    ScoredMove  buf[MAX_MOVES];

    ScoredMove* end = pos.checkers()
        ? generate<EVASIONS>(pos, buf)
        : generate<NON_EVASIONS>(pos, buf);

    for (ScoredMove* it = buf; it != end; ++it) {
        if (pos.legal(it->move))
            *cur++ = *it;
    }
    return cur;
}

} // namespace PurnaFish
