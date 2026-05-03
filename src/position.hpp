/*
 * PurnaFish Chess Engine
 * position.hpp — Board representation and game state
 */

#pragma once

#include "types.hpp"
#include "bitboard.hpp"
#include "zobrist.hpp"
#include <string>
#include <deque>

namespace PurnaFish {

// Forward declarations
struct NNUE_Accumulator;

/// StateInfo stores all information needed to restore a position
/// when we undo a move. Arranged as a linked list.
struct StateInfo {
    // Copied from previous state
    Key    pawnKey;
    Key    materialKey;
    Value  nonPawnMaterial[COLOR_NB];
    int    castlingRights;
    int    rule50;
    int    pliesFromNull;
    Square epSquare;

    // Computed on do_move
    Key        key;
    Bitboard   checkersBB;
    StateInfo* previous;
    Bitboard   blockersForKing[COLOR_NB];
    Bitboard   pinners[COLOR_NB];
    Bitboard   checkSquares[PIECE_TYPE_NB];
    Piece      capturedPiece;
    int        repetition;
};

constexpr auto StartFEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

class Position {
public:
    Position() = default;
    Position(const Position&) = delete;
    Position& operator=(const Position&) = delete;

    // Setup
    Position& set(const std::string& fen, StateInfo* si);
    std::string fen() const;

    // Piece access
    Bitboard pieces(PieceType pt = ALL_PIECES) const;
    Bitboard pieces(PieceType pt1, PieceType pt2) const;
    Bitboard pieces(Color c) const;
    Bitboard pieces(Color c, PieceType pt) const;
    Bitboard pieces(Color c, PieceType pt1, PieceType pt2) const;
    Piece    piece_on(Square s) const;
    Square   ep_square() const;
    bool     empty(Square s) const;
    int      count(Piece pc) const;
    int      count(Color c, PieceType pt) const;
    Square   square(Color c, PieceType pt) const; // Assumes only one (e.g., king)

    // Castling
    CastlingRights castling_rights() const;
    bool can_castle(CastlingRights cr) const;
    Square castling_rook_square(CastlingRights cr) const;

    // Checking
    Bitboard checkers() const;
    Bitboard blockers_for_king(Color c) const;
    Bitboard check_squares(PieceType pt) const;
    Bitboard pinners(Color c) const;
    bool is_discovery_check_on_king(Color c, Move m) const;

    // Attacks
    Bitboard attackers_to(Square s) const;
    Bitboard attackers_to(Square s, Bitboard occupied) const;
    Bitboard slider_blockers(Bitboard sliders, Square s, Bitboard& pinners) const;

    // Properties of moves
    bool legal(Move m) const;
    bool pseudo_legal(Move m) const;
    bool gives_check(Move m) const;
    bool capture(Move m) const;
    bool capture_stage(Move m) const;
    Piece moved_piece(Move m) const;

    // Making/unmaking moves
    void do_move(Move m, StateInfo& new_st);
    void do_move(Move m, StateInfo& new_st, bool gives_check);
    void undo_move(Move m);
    void do_null_move(StateInfo& new_st);
    void undo_null_move();

    // Static exchange evaluation
    bool see_ge(Move m, Value threshold) const;

    // Game state
    Color    side_to_move() const;
    int      game_ply() const;
    Key      key() const;
    Key      pawn_key() const;
    Key      material_key() const;
    Value    non_pawn_material(Color c) const;
    Value    non_pawn_material() const;
    int      rule50_count() const;
    bool     is_draw(int ply) const;
    bool     has_game_cycle(int ply) const;
    bool     has_repeated() const;
    int      piece_count() const;

    // State info
    StateInfo* state() const;

    // Display
    std::string to_string() const;

    // NNUE support
    void put_piece(Piece pc, Square s);
    void remove_piece(Square s);
    void move_piece(Square from, Square to);

private:
    // Board data
    Piece    board_[SQUARE_NB] = {};
    Bitboard byTypeBB_[PIECE_TYPE_NB] = {};
    Bitboard byColorBB_[COLOR_NB] = {};
    int      pieceCount_[PIECE_NB] = {};
    int      castlingRightsMask_[SQUARE_NB] = {};
    Square   castlingRookSquare_[CASTLING_RIGHT_NB] = {};
    Bitboard castlingPath_[CASTLING_RIGHT_NB] = {};

    Color      sideToMove_ = WHITE;
    int        gamePly_ = 0;
    StateInfo* st_ = nullptr;

    // Internal helpers
    void set_castling_right(Color c, Square rfrom);
    void set_state() const;
    void set_check_info() const;
};

// ──────────────────────────────────────────────
// Inline implementations
// ──────────────────────────────────────────────

inline Color    Position::side_to_move()  const { return sideToMove_; }
inline int      Position::game_ply()      const { return gamePly_; }
inline Key      Position::key()           const { return st_->key; }
inline Key      Position::pawn_key()      const { return st_->pawnKey; }
inline Key      Position::material_key()  const { return st_->materialKey; }
inline int      Position::rule50_count()  const { return st_->rule50; }
inline Piece    Position::piece_on(Square s) const { return board_[s]; }
inline bool     Position::empty(Square s)    const { return board_[s] == NO_PIECE; }
inline Square   Position::ep_square()        const { return st_->epSquare; }
inline Bitboard Position::checkers()         const { return st_->checkersBB; }
inline Bitboard Position::blockers_for_king(Color c)  const { return st_->blockersForKing[c]; }
inline Bitboard Position::check_squares(PieceType pt) const { return st_->checkSquares[pt]; }
inline Bitboard Position::pinners(Color c)   const { return st_->pinners[c]; }
inline StateInfo* Position::state()          const { return st_; }

inline CastlingRights Position::castling_rights() const {
    return CastlingRights(st_->castlingRights);
}

inline bool Position::can_castle(CastlingRights cr) const {
    return st_->castlingRights & cr;
}

inline Square Position::castling_rook_square(CastlingRights cr) const {
    return castlingRookSquare_[cr];
}

inline Bitboard Position::pieces(PieceType pt) const {
    return byTypeBB_[pt];
}

inline Bitboard Position::pieces(PieceType pt1, PieceType pt2) const {
    return byTypeBB_[pt1] | byTypeBB_[pt2];
}

inline Bitboard Position::pieces(Color c) const {
    return byColorBB_[c];
}

inline Bitboard Position::pieces(Color c, PieceType pt) const {
    return byColorBB_[c] & byTypeBB_[pt];
}

inline Bitboard Position::pieces(Color c, PieceType pt1, PieceType pt2) const {
    return byColorBB_[c] & (byTypeBB_[pt1] | byTypeBB_[pt2]);
}

inline int Position::count(Piece pc) const {
    return pieceCount_[pc];
}

inline int Position::count(Color c, PieceType pt) const {
    return pieceCount_[make_piece(c, pt)];
}

inline Square Position::square(Color c, PieceType pt) const {
    assert(count(c, pt) == 1);
    return lsb(pieces(c, pt));
}

inline Value Position::non_pawn_material(Color c) const {
    return st_->nonPawnMaterial[c];
}

inline Value Position::non_pawn_material() const {
    return non_pawn_material(WHITE) + non_pawn_material(BLACK);
}

inline Piece Position::moved_piece(Move m) const {
    return board_[from_sq(m)];
}

inline bool Position::capture(Move m) const {
    assert(is_ok(m));
    return (!empty(to_sq(m)) && type_of(m) != CASTLING) || type_of(m) == EN_PASSANT;
}

inline bool Position::capture_stage(Move m) const {
    return capture(m) || type_of(m) == PROMOTION;
}

inline int Position::piece_count() const {
    return popcount(pieces());
}

inline void Position::put_piece(Piece pc, Square s) {
    board_[s] = pc;
    byTypeBB_[ALL_PIECES] |= byTypeBB_[type_of(pc)] |= square_bb(s);
    byColorBB_[color_of(pc)] |= square_bb(s);
    pieceCount_[pc]++;
    pieceCount_[make_piece(color_of(pc), ALL_PIECES)]++;
}

inline void Position::remove_piece(Square s) {
    Piece pc = board_[s];
    byTypeBB_[ALL_PIECES] ^= square_bb(s);
    byTypeBB_[type_of(pc)] ^= square_bb(s);
    byColorBB_[color_of(pc)] ^= square_bb(s);
    board_[s] = NO_PIECE;
    pieceCount_[pc]--;
    pieceCount_[make_piece(color_of(pc), ALL_PIECES)]--;
}

inline void Position::move_piece(Square from, Square to) {
    Piece pc = board_[from];
    Bitboard fromTo = square_bb(from) | square_bb(to);
    byTypeBB_[ALL_PIECES] ^= fromTo;
    byTypeBB_[type_of(pc)] ^= fromTo;
    byColorBB_[color_of(pc)] ^= fromTo;
    board_[from] = NO_PIECE;
    board_[to]   = pc;
}

} // namespace PurnaFish
