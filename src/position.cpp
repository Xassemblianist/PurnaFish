/*
 * PurnaFish Chess Engine
 * position.cpp — Board representation, FEN, do_move/undo_move
 */

#include "position.hpp"
#include "misc.hpp"
#include <sstream>
#include <cstring>
#include <iostream>

namespace PurnaFish {

namespace {

const std::string PieceToChar(" PNBRQK  pnbrqk");

Piece char_to_piece(char c) {
    switch (c) {
        case 'P': return W_PAWN;   case 'N': return W_KNIGHT; case 'B': return W_BISHOP;
        case 'R': return W_ROOK;   case 'Q': return W_QUEEN;  case 'K': return W_KING;
        case 'p': return B_PAWN;   case 'n': return B_KNIGHT; case 'b': return B_BISHOP;
        case 'r': return B_ROOK;   case 'q': return B_QUEEN;  case 'k': return B_KING;
        default:  return NO_PIECE;
    }
}

} // anonymous namespace

Position& Position::set(const std::string& fen, StateInfo* si) {
    std::memset(this, 0, sizeof(Position));
    std::memset(si, 0, sizeof(StateInfo));
    st_ = si;

    std::istringstream ss(fen);
    std::string token;
    Square sq = SQ_A8;

    // 1. Piece placement
    ss >> token;
    for (char c : token) {
        if (c == '/') {
            sq = Square(sq - 16); // Next rank down
        } else if (c >= '1' && c <= '8') {
            sq = Square(sq + (c - '0'));
        } else {
            put_piece(char_to_piece(c), sq);
            ++sq;
        }
    }

    // 2. Side to move
    ss >> token;
    sideToMove_ = (token == "w") ? WHITE : BLACK;

    // 3. Castling rights
    ss >> token;
    for (char c : token) {
        Color cr_color = (c >= 'a' && c <= 'z') ? BLACK : WHITE;
        Square ksq = square(cr_color, KING);
        char upper = (c >= 'a' && c <= 'z') ? c - 32 : c;

        if (upper == 'K') {
            // Find rightmost rook
            for (Square s = Square(ksq + 1); s <= (cr_color == WHITE ? SQ_H1 : SQ_H8); ++s)
                if (piece_on(s) == make_piece(cr_color, ROOK)) {
                    set_castling_right(cr_color, s);
                    break;
                }
        } else if (upper == 'Q') {
            // Find leftmost rook
            for (Square s = Square(ksq - 1); s >= (cr_color == WHITE ? SQ_A1 : SQ_A8); --s)
                if (piece_on(s) == make_piece(cr_color, ROOK)) {
                    set_castling_right(cr_color, s);
                    break;
                }
        }
    }

    // 4. En passant square
    ss >> token;
    if (token != "-") {
        File f = File(token[0] - 'a');
        Rank r = Rank(token[1] - '1');
        st_->epSquare = make_square(f, r);
    } else {
        st_->epSquare = SQ_NONE;
    }

    // 5. Half move clock
    ss >> st_->rule50;

    // 6. Full move number
    ss >> gamePly_;
    gamePly_ = std::max(2 * (gamePly_ - 1), 0) + (sideToMove_ == BLACK);

    set_state();
    return *this;
}

void Position::set_state() const {
    st_->key = 0;
    st_->pawnKey = Zobrist::no_pawns;
    st_->materialKey = 0;
    st_->nonPawnMaterial[WHITE] = st_->nonPawnMaterial[BLACK] = VALUE_ZERO;
    st_->checkersBB = attackers_to(square(sideToMove_, KING)) & pieces(~sideToMove_);

    for (Bitboard b = pieces(); b; ) {
        Square s = pop_lsb(b);
        Piece pc = piece_on(s);
        st_->key ^= Zobrist::psq[pc][s];

        if (type_of(pc) == PAWN)
            st_->pawnKey ^= Zobrist::psq[pc][s];
        else if (type_of(pc) != KING)
            st_->nonPawnMaterial[color_of(pc)] += PieceValue[type_of(pc)];
    }

    if (st_->epSquare != SQ_NONE)
        st_->key ^= Zobrist::enpassant[file_of(st_->epSquare)];

    if (sideToMove_ == BLACK)
        st_->key ^= Zobrist::side;

    st_->key ^= Zobrist::castling[st_->castlingRights];

    set_check_info();
}

void Position::set_check_info() const {
    Color them = ~sideToMove_;
    Square ksq = square(them, KING);

    st_->checkSquares[PAWN]   = pawn_attacks_bb(them, ksq);
    st_->checkSquares[KNIGHT] = KnightAttacks[ksq];
    st_->checkSquares[BISHOP] = attacks_bb<BISHOP>(ksq, pieces());
    st_->checkSquares[ROOK]   = attacks_bb<ROOK>(ksq, pieces());
    st_->checkSquares[QUEEN]  = st_->checkSquares[BISHOP] | st_->checkSquares[ROOK];
    st_->checkSquares[KING]   = 0;

    // Compute blockers and pinners
    st_->blockersForKing[WHITE] = slider_blockers(pieces(BLACK), square(WHITE, KING), st_->pinners[WHITE]);
    st_->blockersForKing[BLACK] = slider_blockers(pieces(WHITE), square(BLACK, KING), st_->pinners[BLACK]);
}

void Position::set_castling_right(Color c, Square rfrom) {
    Square kfrom = square(c, KING);
    CastlingRights cr = (c == WHITE) ?
        (rfrom > kfrom ? WHITE_OO : WHITE_OOO) :
        (rfrom > kfrom ? BLACK_OO : BLACK_OOO);

    st_->castlingRights |= cr;
    castlingRightsMask_[kfrom] |= cr;
    castlingRightsMask_[rfrom] |= cr;
    castlingRookSquare_[cr] = rfrom;

    // Castling path: squares between king start and king end, and rook start and rook end
    Square kto = make_square(cr & KING_SIDE ? FILE_G : FILE_C, rank_of(kfrom));
    Square rto = make_square(cr & KING_SIDE ? FILE_F : FILE_D, rank_of(rfrom));

    castlingPath_[cr] = (between_bb(rfrom, rto) | between_bb(kfrom, kto) |
                         square_bb(rto) | square_bb(kto))
                       & ~(square_bb(kfrom) | square_bb(rfrom));
}

Bitboard Position::attackers_to(Square s) const {
    return attackers_to(s, pieces());
}

Bitboard Position::attackers_to(Square s, Bitboard occupied) const {
    return (pawn_attacks_bb(BLACK, s) & pieces(WHITE, PAWN))
         | (pawn_attacks_bb(WHITE, s) & pieces(BLACK, PAWN))
         | (KnightAttacks[s]         & pieces(KNIGHT))
         | (attacks_bb<ROOK>(s, occupied)   & pieces(ROOK, QUEEN))
         | (attacks_bb<BISHOP>(s, occupied) & pieces(BISHOP, QUEEN))
         | (KingAttacks[s]           & pieces(KING));
}

Bitboard Position::slider_blockers(Bitboard sliders, Square s, Bitboard& p) const {
    Bitboard blockers = 0;
    p = 0;

    Bitboard snipers = ((attacks_bb<ROOK>(s, 0)   & pieces(ROOK, QUEEN))
                       | (attacks_bb<BISHOP>(s, 0) & pieces(BISHOP, QUEEN))) & sliders;

    Bitboard occupancy = pieces() ^ snipers;

    while (snipers) {
        Square sniperSq = pop_lsb(snipers);
        Bitboard b = between_bb(s, sniperSq) & occupancy;

        if (b && !more_than_one(b)) {
            blockers |= b;
            if (b & pieces(color_of(piece_on(s))))
                p |= square_bb(sniperSq);
        }
    }
    return blockers;
}

bool Position::legal(Move m) const {
    Color us = sideToMove_;
    Square from = from_sq(m);
    Square to = to_sq(m);

    // En passant
    if (type_of(m) == EN_PASSANT) {
        Square ksq = square(us, KING);
        Square capsq = to - pawn_push(us);
        Bitboard occupied = (pieces() ^ square_bb(from) ^ square_bb(capsq)) | square_bb(to);
        return !(attacks_bb<ROOK>(ksq, occupied) & pieces(~us, ROOK, QUEEN))
            && !(attacks_bb<BISHOP>(ksq, occupied) & pieces(~us, BISHOP, QUEEN));
    }

    // Castling: check that king doesn't pass through attacked squares
    if (type_of(m) == CASTLING) {
        to = make_square(to > from ? FILE_G : FILE_C, rank_of(from));
        Direction step = (to > from) ? EAST : WEST;

        for (Square s = from; s != to; s += step)
            if (attackers_to(s, pieces()) & pieces(~us))
                return false;

        // Check destination
        return !(attackers_to(to, pieces()) & pieces(~us));
    }

    // King moves
    if (type_of(piece_on(from)) == KING)
        return !(attackers_to(to, pieces() ^ square_bb(from)) & pieces(~us));

    // If in check, non-king moves must capture or block the single checker
    if (checkers()) {
        if (more_than_one(checkers()))
            return false;
        if (!((between_bb(lsb(checkers()), square(us, KING)) | checkers()) & square_bb(to)))
            return false;
    }

    // Pinned pieces can only move along the pin line
    return !(blockers_for_king(us) & square_bb(from))
        || aligned(from, to, square(us, KING));
}

bool Position::gives_check(Move m) const {
    Square from = from_sq(m);
    Square to = to_sq(m);
    PieceType pt = type_of(piece_on(from));

    // Direct check
    if (check_squares(pt) & square_bb(to))
        return true;

    // Discovered check
    if (blockers_for_king(~sideToMove_) & square_bb(from)) {
        if (!aligned(from, to, square(~sideToMove_, KING)))
            return true;
    }

    switch (type_of(m)) {
        case NORMAL:
            return false;

        case PROMOTION:
            return attacks_bb(promotion_type(m), to, pieces() ^ square_bb(from))
                 & square_bb(square(~sideToMove_, KING));

        case EN_PASSANT: {
            Square capsq = make_square(file_of(to), rank_of(from));
            Bitboard b = (pieces() ^ square_bb(from) ^ square_bb(capsq)) | square_bb(to);
            Square ksq = square(~sideToMove_, KING);
            return (attacks_bb<ROOK>(ksq, b)   & pieces(sideToMove_, ROOK, QUEEN))
                 | (attacks_bb<BISHOP>(ksq, b) & pieces(sideToMove_, BISHOP, QUEEN));
        }

        case CASTLING: {
            Square rto = make_square(to > from ? FILE_F : FILE_D, rank_of(from));
            Square ksq = square(~sideToMove_, KING);
            return attacks_bb<ROOK>(rto, pieces()) & square_bb(ksq);
        }

        default:
            return false;
    }
}

void Position::do_move(Move m, StateInfo& newSt) {
    do_move(m, newSt, gives_check(m));
}

void Position::do_move(Move m, StateInfo& newSt, bool givesCheck) {
    Key k = st_->key ^ Zobrist::side;

    // Copy relevant state
    std::memcpy(&newSt, st_, offsetof(StateInfo, key));
    newSt.previous = st_;
    st_ = &newSt;

    ++gamePly_;
    ++st_->rule50;
    st_->pliesFromNull++;

    Color us = sideToMove_;
    Color them = ~us;
    Square from = from_sq(m);
    Square to = to_sq(m);
    Piece pc = piece_on(from);
    Piece captured = type_of(m) == EN_PASSANT ? make_piece(them, PAWN) : piece_on(to);

    assert(color_of(pc) == us);

    // Handle castling
    if (type_of(m) == CASTLING) {
        Square rfrom, rto;
        // 'to' in our encoding stores the rook square for castling
        bool kingSide = to > from;
        rfrom = to; // Original rook square
        rto = make_square(kingSide ? FILE_F : FILE_D, rank_of(from));
        to = make_square(kingSide ? FILE_G : FILE_C, rank_of(from));

        // Move king and rook
        Piece rook_pc = make_piece(us, ROOK);
        remove_piece(from);
        remove_piece(rfrom);
        board_[from] = board_[rfrom] = NO_PIECE; // Prevent put_piece from double counting
        put_piece(pc, to);
        put_piece(rook_pc, rto);

        k ^= Zobrist::psq[pc][from] ^ Zobrist::psq[pc][to];
        k ^= Zobrist::psq[rook_pc][rfrom] ^ Zobrist::psq[rook_pc][rto];

        captured = NO_PIECE;
    }

    if (captured) {
        Square capsq = to;

        if (type_of(m) == EN_PASSANT) {
            capsq -= pawn_push(us);
            assert(piece_on(capsq) == make_piece(them, PAWN));
        }

        remove_piece(capsq);

        if (type_of(captured) == PAWN)
            st_->pawnKey ^= Zobrist::psq[captured][capsq];
        else
            st_->nonPawnMaterial[them] -= PieceValue[type_of(captured)];

        k ^= Zobrist::psq[captured][capsq];
        st_->materialKey ^= Zobrist::psq[captured][capsq]; // simplified
        st_->rule50 = 0;
    }

    // Update hash for en passant
    if (st_->epSquare != SQ_NONE) {
        k ^= Zobrist::enpassant[file_of(st_->epSquare)];
        st_->epSquare = SQ_NONE;
    }

    // Update castling rights
    if (st_->castlingRights && (castlingRightsMask_[from] | castlingRightsMask_[to])) {
        k ^= Zobrist::castling[st_->castlingRights];
        st_->castlingRights &= ~(castlingRightsMask_[from] | castlingRightsMask_[to]);
        k ^= Zobrist::castling[st_->castlingRights];
    }

    // Move the piece (if not castling, which was already handled)
    if (type_of(m) != CASTLING) {
        k ^= Zobrist::psq[pc][from] ^ Zobrist::psq[pc][to];
        move_piece(from, to);
    }

    // Handle pawn specifics
    if (type_of(pc) == PAWN) {
        // Set en passant square for double push
        if ((int(to) ^ int(from)) == 16) {
            st_->epSquare = to - pawn_push(us);
            k ^= Zobrist::enpassant[file_of(st_->epSquare)];
        }

        // Handle promotions
        if (type_of(m) == PROMOTION) {
            Piece promotion = make_piece(us, promotion_type(m));
            remove_piece(to);
            put_piece(promotion, to);

            k ^= Zobrist::psq[pc][to] ^ Zobrist::psq[promotion][to];
            st_->pawnKey ^= Zobrist::psq[pc][to];
            st_->nonPawnMaterial[us] += PieceValue[promotion_type(m)];
        }

        st_->pawnKey ^= Zobrist::psq[pc][from] ^ Zobrist::psq[pc][to];
        st_->rule50 = 0;
    }

    st_->capturedPiece = captured;
    st_->key = k;
    sideToMove_ = them;

    // Update check info
    st_->checkersBB = givesCheck ? attackers_to(square(them, KING)) & pieces(us) : Bitboard(0);
    set_check_info();

    // Repetition detection
    st_->repetition = 0;
    int end = std::min(st_->rule50, st_->pliesFromNull);
    if (end >= 4) {
        StateInfo* stp = st_->previous->previous;
        for (int i = 4; i <= end; i += 2) {
            stp = stp->previous->previous;
            if (stp->key == st_->key) {
                st_->repetition = stp->repetition ? -i : i;
                break;
            }
        }
    }
}

void Position::undo_move(Move m) {
    sideToMove_ = ~sideToMove_;
    Color us = sideToMove_;
    Square from = from_sq(m);
    Square to = to_sq(m);

    if (type_of(m) == CASTLING) {
        bool kingSide = to > from;
        Square rfrom = to;
        Square rto = make_square(kingSide ? FILE_F : FILE_D, rank_of(from));
        to = make_square(kingSide ? FILE_G : FILE_C, rank_of(from));

        Piece rook_pc = make_piece(us, ROOK);
        remove_piece(to);
        remove_piece(rto);
        board_[to] = board_[rto] = NO_PIECE;
        put_piece(make_piece(us, KING), from);
        put_piece(rook_pc, rfrom);
    } else {
        if (type_of(m) == PROMOTION) {
            remove_piece(to);
            put_piece(make_piece(us, PAWN), to);
        }

        move_piece(to, from);

        if (st_->capturedPiece) {
            Square capsq = to;
            if (type_of(m) == EN_PASSANT)
                capsq -= pawn_push(us);
            put_piece(st_->capturedPiece, capsq);
        }
    }

    st_ = st_->previous;
    --gamePly_;
}

void Position::do_null_move(StateInfo& newSt) {
    assert(!checkers());

    std::memcpy(&newSt, st_, sizeof(StateInfo));
    newSt.previous = st_;
    st_ = &newSt;

    st_->key ^= Zobrist::side;

    if (st_->epSquare != SQ_NONE) {
        st_->key ^= Zobrist::enpassant[file_of(st_->epSquare)];
        st_->epSquare = SQ_NONE;
    }

    ++st_->rule50;
    st_->pliesFromNull = 0;
    sideToMove_ = ~sideToMove_;

    set_check_info();
    st_->repetition = 0;
}

void Position::undo_null_move() {
    st_ = st_->previous;
    sideToMove_ = ~sideToMove_;
}

bool Position::is_draw(int ply) const {
    if (st_->rule50 > 99) {
        if (!checkers()) return true;
        // Check if there's a legal move (to distinguish stalemate)
    }

    return st_->repetition && st_->repetition < ply;
}

bool Position::has_repeated() const {
    StateInfo* stp = st_;
    int e = std::min(st_->rule50, st_->pliesFromNull);
    while (e-- >= 4) {
        stp = stp->previous;
        if (stp->repetition)
            return true;
    }
    return false;
}

bool Position::has_game_cycle(int ply) const {
    int end = std::min(st_->rule50, st_->pliesFromNull);

    if (end < 3) return false;

    Key originalKey = st_->key;
    StateInfo* stp = st_->previous;

    for (int i = 3; i <= end; i += 2) {
        stp = stp->previous->previous;
        Key moveKey = originalKey ^ stp->key;
        // Simplified cycle detection
        if (moveKey == 0) {
            if (i < ply) return true;
        }
    }
    return false;
}

// Static Exchange Evaluation
bool Position::see_ge(Move m, Value threshold) const {
    if (type_of(m) != NORMAL)
        return VALUE_ZERO >= threshold;

    Square from = from_sq(m);
    Square to = to_sq(m);

    int swap = int(PieceValue[type_of(piece_on(to))]) - int(threshold);
    if (swap < 0) return false;

    swap = int(PieceValue[type_of(piece_on(from))]) - swap;
    if (swap <= 0) return true;

    assert(color_of(piece_on(from)) == sideToMove_);
    Bitboard occupied = pieces() ^ square_bb(from) ^ square_bb(to);
    Color stm = sideToMove_;
    Bitboard attackers = attackers_to(to, occupied);
    Bitboard stmAttackers;
    int res = 1;

    while (true) {
        stm = ~stm;
        attackers &= occupied;
        stmAttackers = attackers & pieces(stm);

        if (!stmAttackers) break;

        // Don't allow pinned pieces to attack (simplified)
        if (pinners(~stm) & occupied) {
            stmAttackers &= ~blockers_for_king(stm);
            if (!stmAttackers) break;
        }

        res ^= 1;

        // Pick least valuable attacker
        Bitboard bb;
        if ((bb = stmAttackers & pieces(PAWN))) {
            if ((swap = int(PawnValue) - swap) < res) break;
            occupied ^= square_bb(lsb(bb));
            attackers |= attacks_bb<BISHOP>(to, occupied) & pieces(BISHOP, QUEEN);
        } else if ((bb = stmAttackers & pieces(KNIGHT))) {
            if ((swap = int(KnightValue) - swap) < res) break;
            occupied ^= square_bb(lsb(bb));
        } else if ((bb = stmAttackers & pieces(BISHOP))) {
            if ((swap = int(BishopValue) - swap) < res) break;
            occupied ^= square_bb(lsb(bb));
            attackers |= attacks_bb<BISHOP>(to, occupied) & pieces(BISHOP, QUEEN);
        } else if ((bb = stmAttackers & pieces(ROOK))) {
            if ((swap = int(RookValue) - swap) < res) break;
            occupied ^= square_bb(lsb(bb));
            attackers |= attacks_bb<ROOK>(to, occupied) & pieces(ROOK, QUEEN);
        } else if ((bb = stmAttackers & pieces(QUEEN))) {
            if ((swap = int(QueenValue) - swap) < res) break;
            occupied ^= square_bb(lsb(bb));
            attackers |= (attacks_bb<BISHOP>(to, occupied) & pieces(BISHOP, QUEEN))
                       | (attacks_bb<ROOK>(to, occupied)   & pieces(ROOK, QUEEN));
        } else { // King
            return (attackers & ~pieces(stm)) ? res ^ 1 : res;
        }
    }
    return bool(res);
}

std::string Position::fen() const {
    std::ostringstream ss;

    for (Rank r = RANK_8; r >= RANK_1; --r) {
        for (File f = FILE_A; f <= FILE_H; ++f) {
            int emptyCnt = 0;
            for (; f <= FILE_H && empty(make_square(f, r)); ++f)
                ++emptyCnt;
            if (emptyCnt) ss << emptyCnt;
            if (f <= FILE_H) {
                Piece pc = piece_on(make_square(f, r));
                if (pc != NO_PIECE) ss << PieceToChar[pc];
            }
        }
        if (r > RANK_1) ss << '/';
    }

    ss << (sideToMove_ == WHITE ? " w " : " b ");

    if (can_castle(WHITE_OO))  ss << 'K';
    if (can_castle(WHITE_OOO)) ss << 'Q';
    if (can_castle(BLACK_OO))  ss << 'k';
    if (can_castle(BLACK_OOO)) ss << 'q';
    if (!can_castle(ANY_CASTLING)) ss << '-';

    ss << ' ';
    if (ep_square() == SQ_NONE) {
        ss << '-';
    } else {
        ss << char('a' + file_of(ep_square())) << char('1' + rank_of(ep_square()));
    }

    ss << ' ' << st_->rule50 << ' ' << 1 + (gamePly_ - (sideToMove_ == BLACK)) / 2;
    return ss.str();
}

std::string Position::to_string() const {
    std::ostringstream ss;
    ss << "\n +---+---+---+---+---+---+---+---+\n";
    for (Rank r = RANK_8; r >= RANK_1; --r) {
        for (File f = FILE_A; f <= FILE_H; ++f) {
            Piece pc = piece_on(make_square(f, r));
            ss << " | " << (pc != NO_PIECE ? PieceToChar[pc] : ' ');
        }
        ss << " | " << (1 + r) << "\n +---+---+---+---+---+---+---+---+\n";
    }
    ss << "   a   b   c   d   e   f   g   h\n\n";
    ss << "Fen: " << fen() << "\nKey: " << std::hex << key() << std::dec << "\n";
    return ss.str();
}

bool Position::pseudo_legal(Move m) const {
    Color us = sideToMove_;
    Square from = from_sq(m);
    Square to = to_sq(m);
    Piece pc = moved_piece(m);

    // If the from square doesn't have our piece, it's not pseudo-legal
    if (pc == NO_PIECE || color_of(pc) != us)
        return false;

    // The destination square cannot contain our own piece
    if (pieces(us) & square_bb(to))
        return false;

    // Handle special moves
    if (type_of(m) == EN_PASSANT) {
        return type_of(pc) == PAWN
            && to == ep_square()
            && (pawn_attacks_bb(us, from) & square_bb(to));
    }

    if (type_of(m) == CASTLING) {
        // Simplified: check that it's a valid castling pattern
        return type_of(pc) == KING
            && piece_on(to) == make_piece(us, ROOK);
    }

    if (type_of(m) == PROMOTION) {
        return type_of(pc) == PAWN
            && relative_rank(us, to) == RANK_8;
    }

    // Normal moves
    if (type_of(pc) == PAWN) {
        // Pawn pushes and captures
        Bitboard target_sq = square_bb(to);

        if (pawn_attacks_bb(us, from) & pieces(~us) & target_sq)
            return true;

        Direction push = pawn_push(us);
        if (to == from + push && empty(to))
            return true;

        if (relative_rank(us, from) == RANK_2
            && to == from + 2 * push
            && empty(Square(from + push)) && empty(to))
            return true;

        return false;
    }

    // For other pieces, check if the target is in the attack set
    return attacks_bb(type_of(pc), from, pieces()) & square_bb(to);
}

} // namespace PurnaFish
