/*
 * PurnaFish Chess Engine
 * movepick.hpp — Move ordering for search efficiency
 */

#pragma once

#include "types.hpp"
#include "position.hpp"
#include "movegen.hpp"

namespace PurnaFish {

// History heuristic table: [color][from][to] scores
struct ButterflyHistory {
    int table[COLOR_NB][SQUARE_NB][SQUARE_NB] = {};

    int get(Color c, Move m) const { return table[c][from_sq(m)][to_sq(m)]; }
    void update(Color c, Move m, int bonus) {
        auto& entry = table[c][from_sq(m)][to_sq(m)];
        entry += bonus - entry * std::abs(bonus) / 16384;
    }
    void clear() { std::memset(table, 0, sizeof(table)); }
};

// Continuation history: [piece][to] for previous move context
struct PieceToHistory {
    int table[PIECE_NB][SQUARE_NB] = {};

    int get(Piece pc, Square to) const { return table[pc][to]; }
    void update(Piece pc, Square to, int bonus) {
        auto& entry = table[pc][to];
        entry += bonus - entry * std::abs(bonus) / 29952;
    }
    void clear() { std::memset(table, 0, sizeof(table)); }
};

// Capture history: [piece][to][captured_type]
struct CapturePieceToHistory {
    int table[PIECE_NB][SQUARE_NB][PIECE_TYPE_NB] = {};

    int get(Piece pc, Square to, PieceType captured) const { return table[pc][to][captured]; }
    void update(Piece pc, Square to, PieceType captured, int bonus) {
        auto& entry = table[pc][to][captured];
        entry += bonus - entry * std::abs(bonus) / 10692;
    }
    void clear() { std::memset(table, 0, sizeof(table)); }
};

// Counter move: [piece][to] -> best response move
struct CounterMoveHistory {
    Move table[PIECE_NB][SQUARE_NB] = {};
    Move get(Piece pc, Square to) const { return table[pc][to]; }
    void update(Piece pc, Square to, Move m) { table[pc][to] = m; }
    void clear() { std::memset(table, 0, sizeof(table)); }
};

/// MovePicker — staged move generation and ordering
class MovePicker {
public:
    // For main search
    MovePicker(const Position& pos, Move ttMove, Depth depth,
               const ButterflyHistory* mh,
               const CapturePieceToHistory* cph,
               const PieceToHistory** contHist,
               Move countermove, const Move* killers);

    // For quiescence search
    MovePicker(const Position& pos, Move ttMove, Depth depth,
               const ButterflyHistory* mh,
               const CapturePieceToHistory* cph);

    // For ProbCut
    MovePicker(const Position& pos, Move ttMove, Value threshold);

    Move next_move(bool skipQuiets = false);

private:
    template<typename Pred>
    ScoredMove* partition(ScoredMove* begin, ScoredMove* end, Pred pred);

    ScoredMove* select_best(ScoredMove* begin, ScoredMove* end);
    void score_captures();
    void score_quiets();

    enum PickType {
        MAIN_TT, CAPTURE_INIT, GOOD_CAPTURE,
        KILLER_1, KILLER_2, COUNTER,
        QUIET_INIT, QUIET_MOVE,
        BAD_CAPTURE,
        EVASION_TT, EVASION_INIT, EVASION_MOVE,
        PROBCUT_INIT, PROBCUT,
        QSEARCH_TT, QCAPTURE_INIT, QCAPTURE
    };

    const Position& pos_;
    const ButterflyHistory* mainHistory_ = nullptr;
    const CapturePieceToHistory* captureHistory_ = nullptr;
    const PieceToHistory** contHistory_ = nullptr;
    Move ttMove_;
    Move killers_[2] = {};
    Move countermove_ = MOVE_NONE;
    ScoredMove moves_[MAX_MOVES];
    ScoredMove *cur_, *endMoves_, *endBadCaptures_;
    int stage_;
    Depth depth_;
    Value threshold_ = VALUE_ZERO;
};

} // namespace PurnaFish
