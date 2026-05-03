/*
 * PurnaFish Chess Engine
 * movegen.hpp — Move generation
 */

#pragma once

#include "types.hpp"
#include "position.hpp"

namespace PurnaFish {

enum GenType {
    CAPTURES,
    QUIETS,
    QUIET_CHECKS,
    EVASIONS,
    NON_EVASIONS,
    LEGAL
};

template<GenType T>
ScoredMove* generate(const Position& pos, ScoredMove* moveList);

/// Specialization for LEGAL - generates only legal moves
template<>
ScoredMove* generate<LEGAL>(const Position& pos, ScoredMove* moveList);

// Perft helper
struct MoveList {
    explicit MoveList(const Position& pos) :
        last_(generate<LEGAL>(pos, moveList_)) {}

    const ScoredMove* begin() const { return moveList_; }
    const ScoredMove* end()   const { return last_; }
    size_t size()             const { return last_ - moveList_; }
    bool   contains(Move m)  const {
        for (const auto& sm : *this)
            if (sm.move == m) return true;
        return false;
    }

private:
    ScoredMove moveList_[MAX_MOVES], *last_;
};

} // namespace PurnaFish
