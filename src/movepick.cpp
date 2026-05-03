/*
 * PurnaFish Chess Engine
 * movepick.cpp — Move ordering implementation
 */

#include "movepick.hpp"
#include "tt.hpp"
#include <algorithm>

namespace PurnaFish {

// Main search constructor
MovePicker::MovePicker(const Position& pos, Move ttMove, Depth depth,
                       const ButterflyHistory* mh,
                       const CapturePieceToHistory* cph,
                       const PieceToHistory** contHist,
                       Move countermove, const Move* killers)
    : pos_(pos), mainHistory_(mh), captureHistory_(cph), contHistory_(contHist),
      ttMove_(ttMove), countermove_(countermove), depth_(depth)
{
    if (killers) {
        killers_[0] = killers[0];
        killers_[1] = killers[1];
    }

    stage_ = pos.checkers() ? EVASION_TT : MAIN_TT;

    // Validate TT move
    if (ttMove_ && !pos.pseudo_legal(ttMove_))
        ttMove_ = MOVE_NONE;
}

// Quiescence search constructor
MovePicker::MovePicker(const Position& pos, Move ttMove, Depth depth,
                       const ButterflyHistory* mh,
                       const CapturePieceToHistory* cph)
    : pos_(pos), mainHistory_(mh), captureHistory_(cph), ttMove_(ttMove), depth_(depth)
{
    stage_ = QSEARCH_TT;
    if (ttMove_ && !pos.pseudo_legal(ttMove_))
        ttMove_ = MOVE_NONE;
}

// ProbCut constructor
MovePicker::MovePicker(const Position& pos, Move ttMove, Value threshold)
    : pos_(pos), ttMove_(ttMove), threshold_(threshold)
{
    stage_ = PROBCUT_INIT;
    if (ttMove_ && (!pos.pseudo_legal(ttMove_) || !pos.capture(ttMove_) || !pos.see_ge(ttMove_, threshold_)))
        ttMove_ = MOVE_NONE;
}

ScoredMove* MovePicker::select_best(ScoredMove* begin, ScoredMove* end) {
    std::swap(*begin, *std::max_element(begin, end));
    return begin;
}

void MovePicker::score_captures() {
    for (auto& m : std::ranges::subrange(cur_, endMoves_)) {
        m.score = 7 * int(PieceValue[type_of(pos_.piece_on(to_sq(m.move)))]);
        if (captureHistory_)
            m.score += captureHistory_->get(pos_.moved_piece(m.move), to_sq(m.move),
                                            type_of(pos_.piece_on(to_sq(m.move))));
    }
}

void MovePicker::score_quiets() {
    Color us = pos_.side_to_move();
    for (auto& m : std::ranges::subrange(cur_, endMoves_)) {
        m.score = 2 * mainHistory_->get(us, m.move);
        if (contHistory_) {
            m.score += 2 * (*contHistory_[0]).get(pos_.moved_piece(m.move), to_sq(m.move));
            m.score +=     (*contHistory_[1]).get(pos_.moved_piece(m.move), to_sq(m.move));
        }
    }
}

Move MovePicker::next_move(bool skipQuiets) {
    switch (stage_) {
    case MAIN_TT:
    case EVASION_TT:
    case QSEARCH_TT:
        ++stage_;
        if (ttMove_)
            return ttMove_;
        [[fallthrough]];

    case CAPTURE_INIT:
    case EVASION_INIT:
    case QCAPTURE_INIT:
        cur_ = endBadCaptures_ = moves_;
        endMoves_ = (stage_ == EVASION_INIT)
            ? generate<EVASIONS>(pos_, cur_)
            : generate<CAPTURES>(pos_, cur_);
        if (stage_ == EVASION_INIT)
            score_quiets(); // score evasions by history
        else
            score_captures();
        ++stage_;
        [[fallthrough]];

    case GOOD_CAPTURE:
        while (cur_ < endMoves_) {
            ScoredMove* best = select_best(cur_, endMoves_);
            Move m = best->move;
            cur_++;
            if (m != ttMove_) {
                if (pos_.see_ge(m, Value(-best->score / 18)))
                    return m;
                // Bad capture — save for later
                *(endBadCaptures_++) = {m, best->score};
            }
        }
        ++stage_;
        [[fallthrough]];

    case KILLER_1:
        ++stage_;
        if (killers_[0] && killers_[0] != ttMove_ && pos_.pseudo_legal(killers_[0]) && !pos_.capture(killers_[0]))
            return killers_[0];
        [[fallthrough]];

    case KILLER_2:
        ++stage_;
        if (killers_[1] && killers_[1] != ttMove_ && pos_.pseudo_legal(killers_[1]) && !pos_.capture(killers_[1]))
            return killers_[1];
        [[fallthrough]];

    case COUNTER:
        ++stage_;
        if (countermove_ && countermove_ != ttMove_
            && countermove_ != killers_[0] && countermove_ != killers_[1]
            && pos_.pseudo_legal(countermove_) && !pos_.capture(countermove_))
            return countermove_;
        [[fallthrough]];

    case QUIET_INIT:
        if (!skipQuiets) {
            cur_ = endBadCaptures_;
            endMoves_ = generate<QUIETS>(pos_, cur_);
            score_quiets();
            // Partial sort: only sort the best ones
            auto* begin = cur_;
            auto* end = endMoves_;
            // Sort quiets in descending order of score
            std::partial_sort(begin, begin + std::min(int(end - begin), 10), end,
                [](const ScoredMove& a, const ScoredMove& b) { return a.score > b.score; });
        }
        ++stage_;
        [[fallthrough]];

    case QUIET_MOVE:
        if (!skipQuiets) {
            while (cur_ < endMoves_) {
                Move m = cur_->move;
                cur_++;
                if (m != ttMove_ && m != killers_[0] && m != killers_[1] && m != countermove_)
                    return m;
            }
        }
        ++stage_;
        cur_ = moves_;
        endMoves_ = endBadCaptures_;
        [[fallthrough]];

    case BAD_CAPTURE:
        while (cur_ < endMoves_) {
            Move m = cur_->move;
            cur_++;
            if (m != ttMove_)
                return m;
        }
        return MOVE_NONE;

    case EVASION_MOVE:
        while (cur_ < endMoves_) {
            ScoredMove* best = select_best(cur_, endMoves_);
            Move m = best->move;
            cur_++;
            if (m != ttMove_)
                return m;
        }
        return MOVE_NONE;

    case QCAPTURE:
        while (cur_ < endMoves_) {
            ScoredMove* best = select_best(cur_, endMoves_);
            Move m = best->move;
            cur_++;
            if (m != ttMove_)
                return m;
        }
        return MOVE_NONE;

    case PROBCUT_INIT:
        cur_ = moves_;
        endMoves_ = generate<CAPTURES>(pos_, cur_);
        score_captures();
        ++stage_;
        [[fallthrough]];

    case PROBCUT:
        while (cur_ < endMoves_) {
            ScoredMove* best = select_best(cur_, endMoves_);
            Move m = best->move;
            cur_++;
            if (m != ttMove_ && pos_.see_ge(m, threshold_))
                return m;
        }
        return MOVE_NONE;

    default:
        return MOVE_NONE;
    }

    return MOVE_NONE;
}

} // namespace PurnaFish
