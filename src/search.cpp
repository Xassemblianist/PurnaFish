/*
 * PurnaFish Chess Engine
 * search.cpp — Alpha-Beta search with all modern pruning techniques
 */

#include "search.hpp"
#include "thread.hpp"
#include "evaluate.hpp"
#include "movegen.hpp"
#include "tt.hpp"
#include "timeman.hpp"
#include "syzygy/syzygy.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>

namespace PurnaFish {

int Reductions[MAX_PLY][MAX_MOVES];
int Search::Contempt = 0;

void Search::init() {
    for (int i = 1; i < MAX_PLY; ++i)
        for (int j = 1; j < MAX_MOVES; ++j)
            Reductions[i][j] = int(21.9 * std::log(i) * std::log(j) + 0.5) / 10;
}

void SearchWorker::clear() {
    mainHistory.clear();
    captureHistory.clear();
    counterMoves.clear();

    if (!contHistory)
        contHistory = std::make_unique<ContHistory>();

    for (int i = 0; i < 2; ++i)
        for (int j = 0; j < PIECE_NB; ++j)
            for (int k = 0; k < SQUARE_NB; ++k)
                contHistory->table[i][j][k].clear();
    
    completedDepth = 0;
    nodes = 0;
}

int SearchWorker::stat_bonus(Depth d) const {
    return std::min(336 * d - 547, 1460);
}

int SearchWorker::stat_malus(Depth d) const {
    return std::min(480 * d - 320, 1800);
}

void SearchWorker::update_pv(Move* pv, Move bestMove, const Move* childPv) {
    *pv++ = bestMove;
    if (childPv)
        while (*childPv != MOVE_NONE)
            *pv++ = *childPv++;
    *pv = MOVE_NONE;
}

void SearchWorker::update_continuation_histories(SearchStack* ss, Piece pc, Square to, int bonus) {
    if (ss->ply >= 1 && (ss-1)->contHistory)
        (ss-1)->contHistory->update(pc, to, bonus);
    if (ss->ply >= 2 && (ss-2)->contHistory)
        (ss-2)->contHistory->update(pc, to, bonus);
}

void SearchWorker::update_quiet_stats(Position& pos, SearchStack* ss, Move bestMove,
                                      int bonus, Move* quietsSearched, int quietCount) {
    Color us = pos.side_to_move();

    // Update killers
    if (ss->killers[0] != bestMove) {
        ss->killers[1] = ss->killers[0];
        ss->killers[0] = bestMove;
    }

    mainHistory.update(us, bestMove, bonus);
    update_continuation_histories(ss, pos.moved_piece(bestMove), to_sq(bestMove), bonus);

    // Update counter move
    if (ss->ply >= 1) {
        Square prevSq = to_sq((ss-1)->currentMove);
        counterMoves.update(pos.piece_on(prevSq), prevSq, bestMove);
    }

    // Penalize other quiets
    for (int i = 0; i < quietCount; ++i) {
        mainHistory.update(us, quietsSearched[i], -bonus);
        update_continuation_histories(ss, pos.moved_piece(quietsSearched[i]),
                                      to_sq(quietsSearched[i]), -bonus);
    }
}

void SearchWorker::update_all_stats(Position& pos, SearchStack* ss, Move bestMove,
                                    Value bestValue, Value beta, Square prevSq,
                                    Move* quietsSearched, int quietCount,
                                    Move* capturesSearched, int captureCount, Depth depth) {
    int bonus = stat_bonus(depth);

    if (!pos.capture(bestMove)) {
        update_quiet_stats(pos, ss, bestMove, bonus, quietsSearched, quietCount);
    } else {
        captureHistory.update(pos.moved_piece(bestMove), to_sq(bestMove),
                              type_of(pos.piece_on(to_sq(bestMove))), bonus);
    }

    // Penalize captures that didn't cause cutoff
    for (int i = 0; i < captureCount; ++i) {
        Move m = capturesSearched[i];
        captureHistory.update(pos.moved_piece(m), to_sq(m),
                              type_of(pos.piece_on(to_sq(m))), -bonus);
    }
}

void SearchWorker::check_time() {
    if (nodes % 4096 == 0) {
        TimePoint elapsed = now() - startTime;
        if (!limits.infinite && !limits.ponder) {
            if (elapsed >= maximumTime)
                Threads.stop = true;
        }
    }
}

// ──────────────────────────────────────────────
// Iterative Deepening
// ──────────────────────────────────────────────

void SearchWorker::start_search(Position& pos, const SearchLimits& lim,
                                const std::vector<RootMove>& moves) {
    rootPos_ = &pos;
    limits = lim;
    rootMoves = moves;
    Threads.stop = false;
    nodes = 0;
    startTime = now();

    // Time management
    TimeManager tm;
    tm.init(limits, pos.side_to_move(), pos.game_ply());
    optimumTime = tm.optimum();
    maximumTime = tm.maximum();
}

void SearchWorker::iterative_deepening() {
    Position& pos = *rootPos_;
    // PV table is used per-ply in search() calls
    Value bestValue = -VALUE_INFINITE;
    Value alpha, beta, delta;

    // Initialize search stack
    std::memset(stack_, 0, sizeof(stack_));
    // stack_[0..3] are used for (ss-1), (ss-2), etc lookbacks
    // We start searching from stack_[4] which has ply=0
    for (int i = 0; i < MAX_PLY + 10; ++i) {
        stack_[i].ply = i - 4; // so stack_[4].ply == 0 (root)
        stack_[i].contHistory = &contHistory->table[0][0][0];
    }

    int maxDepth = limits.depth ? limits.depth : MAX_PLY;

    // Iterative deepening loop
    for (int depth = 1; depth <= maxDepth && !Threads.stop; ++depth) {
        selDepth = 0;

        // Reset scores
        for (auto& rm : rootMoves)
            rm.previousScore = rm.score;

        // Aspiration window
        if (depth >= 4) {
            delta = Value(9 + int(bestValue) * int(bestValue) / 13765);
            alpha = std::max(bestValue - delta, -VALUE_INFINITE);
            beta  = std::min(bestValue + delta,  VALUE_INFINITE);
        } else {
            alpha = -VALUE_INFINITE;
            beta  =  VALUE_INFINITE;
        }

        // Aspiration window loop
        int failedHighCnt = 0;
        while (true) {
            bestValue = search(pos, &stack_[4], alpha, beta, Depth(depth), false);

            // Sort root moves
            std::stable_sort(rootMoves.begin(), rootMoves.end());

            if (Threads.stop)
                break;

            // Widen aspiration window
            if (bestValue <= alpha) {
                beta = (alpha + beta) / 2;
                alpha = std::max(bestValue - delta, -VALUE_INFINITE);
                failedHighCnt = 0;
            } else if (bestValue >= beta) {
                beta = std::min(bestValue + delta, VALUE_INFINITE);
                ++failedHighCnt;
            } else {
                break;
            }

            delta += delta / 3;
        }

        completedDepth = depth;

        if (Threads.stop)
            break;

        if (threadId == 0) {
            TimePoint elapsed = std::max(now() - startTime, TimePoint(1));
            std::ostringstream ss;
            ss << "info depth " << depth
               << " seldepth " << selDepth
               << " score ";

            Value v = rootMoves[0].score;
            if (std::abs(int(v)) < int(VALUE_MATE_IN_MAX_PLY))
                ss << "cp " << v;
            else
                ss << "mate " << (v > 0 ? (VALUE_MATE - v + 1) / 2 : -(VALUE_MATE + v) / 2);

            // Compute total nodes across all threads roughly or just local for now
            // Let's print just local nodes until ThreadPool exposes total nodes
            ss << " nodes " << Threads.nodes_searched()
               << " nps " << (Threads.nodes_searched() * 1000 / elapsed)
               << " hashfull " << TT.hashfull()
               << " time " << elapsed
               << " pv";

            for (Move m : rootMoves[0].pv)
                ss << " " << (char)('a' + file_of(from_sq(m)))
                   << (char)('1' + rank_of(from_sq(m)))
                   << (char)('a' + file_of(to_sq(m)))
                   << (char)('1' + rank_of(to_sq(m)));

            std::cout << ss.str() << std::endl;
        }

        // Time management: check if we should stop
        if (!limits.infinite && !limits.ponder && !limits.depth) {
            if (now() - startTime > optimumTime) {
                if (threadId == 0) Threads.stop = true;
                break;
            }
        }
    }

    if (threadId == 0) {
        // Output best move
        Move best = rootMoves[0].pv[0];
        std::cout << "bestmove "
                  << (char)('a' + file_of(from_sq(best)))
                  << (char)('1' + rank_of(from_sq(best)))
                  << (char)('a' + file_of(to_sq(best)))
                  << (char)('1' + rank_of(to_sq(best)));

        if (type_of(best) == PROMOTION) {
            const char promoChar[] = {' ', ' ', 'n', 'b', 'r', 'q'};
            std::cout << promoChar[promotion_type(best)];
        }

        std::cout << std::endl;
        Threads.stop = true;
    }
}

// ──────────────────────────────────────────────
// Main Search (Alpha-Beta + Pruning)
// ──────────────────────────────────────────────

Value SearchWorker::search(Position& pos, SearchStack* ss, Value alpha, Value beta,
                           Depth depth, bool cutNode) {
    bool rootNode = (ss->ply == 0); // Root is at ply 0
    bool pvNode   = (beta - alpha != 1);

    // Check for draw or maximum ply
    if (!rootNode && (pos.is_draw(ss->ply) || ss->ply >= MAX_PLY))
        return ss->ply >= MAX_PLY && !ss->inCheck ? Eval::evaluate(pos) : VALUE_DRAW;

    // Quiescence search at horizon
    if (depth <= 0)
        return qsearch(pos, ss, alpha, beta, Depth(0));

    assert(-VALUE_INFINITE <= alpha && alpha < beta && beta <= VALUE_INFINITE);

    nodes++;
    check_time();

    // Mate distance pruning
    alpha = std::max(mated_in(ss->ply), alpha);
    beta  = std::min(mate_in(ss->ply + 1), beta);
    if (alpha >= beta)
        return alpha;

    ss->pv = pv_table_[ss->ply];
    ss->pv[0] = MOVE_NONE;

    Move  bestMove = MOVE_NONE;
    Value bestValue = -VALUE_INFINITE;


    ss->inCheck = pos.checkers();
    ss->moveCount = 0;

    // Syzygy tablebase probing
    if (Tablebases::MaxPieces && pos.piece_count() <= Tablebases::MaxPieces 
        && !pos.castling_rights() && pos.rule50_count() == 0) {
        
        Value tbValue;
        if (Tablebases::probe_wdl(pos, tbValue)) {
            int ply = ss->ply;
            if (!rootNode) {
                if (tbValue > VALUE_DRAW) return tbValue - ply;
                if (tbValue < VALUE_DRAW) return tbValue + ply;
                return VALUE_DRAW;
            }
        }
    }

    (ss+1)->ttPv = false;
    (ss+1)->excludedMove = MOVE_NONE;
    (ss+2)->killers[0] = (ss+2)->killers[1] = MOVE_NONE;

    // Transposition table lookup
    bool  ttHit;
    Key   posKey = pos.key();
    TTEntry* tte = TT.probe(posKey, ttHit);
    Value ttValue = ttHit ? tte->value() : VALUE_NONE;
    Move  ttMove  = ttHit ? tte->move()  : MOVE_NONE;
    bool  ttPv    = pvNode || (ttHit && tte->is_pv());

    ss->ttHit = ttHit;
    ss->ttPv  = ttPv;

    // TT cutoff (non-PV nodes)
    if (!pvNode && ttHit && tte->depth() >= depth
        && (tte->bound() & (ttValue >= beta ? BOUND_LOWER : BOUND_UPPER))) {
        return ttValue;
    }

    // Static evaluation
    Value eval;
    if (ss->inCheck) {
        eval = ss->staticEval = VALUE_NONE;
    } else if (ttHit) {
        eval = ss->staticEval = tte->eval() != VALUE_NONE ? tte->eval() : Eval::evaluate(pos);
        if (ttValue != VALUE_NONE
            && (tte->bound() & (ttValue > eval ? BOUND_LOWER : BOUND_UPPER)))
            eval = ttValue;
    } else {
        eval = ss->staticEval = Eval::evaluate(pos);
        tte->save(posKey, VALUE_NONE, ttPv, BOUND_NONE, DEPTH_NONE, MOVE_NONE,
                  eval, TT.generation());
    }

    bool improving = !ss->inCheck
        && ss->ply >= 2
        && (ss-2)->staticEval != VALUE_NONE
        && ss->staticEval > (ss-2)->staticEval;

    // ──────── Pruning techniques ────────

    if (!pvNode && !ss->inCheck) {
        // Reverse Futility Pruning (Static Null Move Pruning)
        if (depth < 9 && eval - 74 * (depth - improving) >= beta
            && eval >= beta && eval < VALUE_KNOWN_WIN)
            return eval;

        // Null Move Pruning
        if (eval >= beta && eval >= ss->staticEval
            && ss->staticEval >= beta - 21 * depth + 330
            && ss->ply >= 1 && (ss-1)->currentMove != MOVE_NULL
            && pos.non_pawn_material(pos.side_to_move())) {

            Depth R = Depth(4 + depth / 6 + std::min(3, int(eval - beta) / 200));

            ss->currentMove = MOVE_NULL;
            ss->contHistory = &contHistory->table[0][0][0];

            StateInfo st;
            pos.do_null_move(st);
            Value nullValue = -search(pos, ss + 1, -beta, -beta + 1, depth - R, !cutNode);
            pos.undo_null_move();

            if (nullValue >= beta) {
                return nullValue >= VALUE_TB_WIN ? beta : nullValue;
            }
        }

        // Razoring
        if (depth <= 1 && eval + 510 <= alpha)
            return qsearch(pos, ss, alpha, beta, Depth(0));

        // Futility Pruning condition (set flag for later use)
    }

    // ProbCut
    if (!pvNode && depth >= 5 && std::abs(beta) < VALUE_MATE_IN_MAX_PLY) {
        Value pbBeta = beta + 200;
        Depth pbDepth = depth - 4;

        if (!(ttHit && tte->depth() >= pbDepth && tte->value() < pbBeta)) {
            MovePicker mp_prob(pos, ttMove, Value(1));
            Move m;
            bool skip = false;
            while ((m = mp_prob.next_move(skip)) != MOVE_NONE) {
                if (!pos.legal(m)) continue;
                StateInfo st;
                pos.do_move(m, st, pos.gives_check(m));
                Value pbValue = -search(pos, ss + 1, -pbBeta, -pbBeta + 1, pbDepth, !cutNode);
                pos.undo_move(m);

                if (pbValue >= pbBeta)
                    return pbValue;
            }
        }
    }

    // Initialize move picker
    const PieceToHistory* contHistPtrs[2] = {
        (ss-1)->contHistory, (ss-2)->contHistory
    };

    Move countermove = MOVE_NONE;
    if (ss->ply >= 1 && (ss-1)->currentMove != MOVE_NULL) {
        Square prevSq = to_sq((ss-1)->currentMove);
        countermove = counterMoves.get(pos.piece_on(prevSq), prevSq);
    }

    // Internal Iterative Reduction
    if (!ttMove && depth >= 4)
        depth = Depth(depth - 1);

    MovePicker mp(pos, ttMove, depth, &mainHistory, &captureHistory,
                  contHistPtrs, countermove, ss->killers);

    Move quietsSearched[64];
    Move capturesSearched[32];
    int quietCount = 0, captureCount = 0;
    int moveCount = 0;
    bool skipQuiets = false;

    // Move loop
    Move move;
    while ((move = mp.next_move(skipQuiets)) != MOVE_NONE) {
        if (move == ss->excludedMove)
            continue;
            
        if (!pos.legal(move))
            continue;

        moveCount++;
        ss->moveCount = moveCount;

        bool isCapture = pos.capture(move);
        bool givesCheck = pos.gives_check(move);
        int newDepth = depth - 1;

        // ──────── Move-level pruning ────────

        if (!rootNode && bestValue > VALUE_MATED_IN_MAX_PLY) {
            // Late Move Pruning
            if (!isCapture && !givesCheck && !pvNode) {
                int lmpThreshold = (3 + depth * depth) / (2 - improving);
                if (moveCount > lmpThreshold) {
                    skipQuiets = true;
                    continue;
                }
            }

            // Futility Pruning
            if (!isCapture && !givesCheck && depth < 9 && !ss->inCheck) {
                Value futilityValue = ss->staticEval + 150 * depth;
                if (futilityValue <= alpha) {
                    skipQuiets = true;
                    continue;
                }
            }

            // SEE pruning for captures
            if (depth < 8 && !pos.see_ge(move, isCapture ? Value(-85 * depth) : Value(-52 * depth)))
                continue;
        }

        // Extensions
        int extension = 0;

        // Check extension
        if (givesCheck && (depth < 10 || pvNode))
            extension = 1;

        // Singular extension
        if (!rootNode && depth >= 8 && move == ttMove && !ss->excludedMove
            && ttHit && tte->depth() >= depth - 3
            && (tte->bound() & BOUND_LOWER)) {
            Value singularBeta = ttValue - 2 * depth;
            Depth singularDepth = Depth((depth - 1) / 2);

            ss->excludedMove = move;
            Value value = search(pos, ss, singularBeta - 1, singularBeta,
                                singularDepth, cutNode);
            ss->excludedMove = MOVE_NONE;

            if (value < singularBeta) {
                extension = 1;
                // Double extension for very singular moves
                if (value < singularBeta - 20 && !pvNode)
                    extension = 2;
            } else if (singularBeta >= beta) {
                return singularBeta; // Multi-cut pruning
            }
        }

        newDepth += extension;

        // Prefetch TT entry for the next position
        prefetch(TT.first_entry(pos.key() ^ Zobrist::psq[pos.moved_piece(move)][to_sq(move)]));

        // Make the move
        ss->currentMove = move;
        ss->contHistory = &contHistory->table[ss->inCheck][pos.moved_piece(move)][to_sq(move)];

        StateInfo st;
        pos.do_move(move, st, givesCheck);

        // Late Move Reductions
        Value value = alpha + 1; // Ensure PV search triggers for first move
        if (depth >= 2 && moveCount > 1 + rootNode) {
            int r = Reductions[depth][moveCount];

            // Adjust reduction
            if (!pvNode) r++;
            if (cutNode) r += 2;
            if (!isCapture && !givesCheck) r++;
            if (improving) r--;

            // Reduce based on history
            if (!isCapture) {
                int histScore = 2 * mainHistory.get(pos.side_to_move(), move);
                if (contHistPtrs[0])
                    histScore += (*contHistPtrs[0]).get(pos.moved_piece(move), to_sq(move));
                if (contHistPtrs[1])
                    histScore += (*contHistPtrs[1]).get(pos.moved_piece(move), to_sq(move));
                r -= histScore / 8000;
            }

            Depth d = std::max(Depth(1), Depth(newDepth - r));

            value = -search(pos, ss + 1, -(alpha + 1), -alpha, d, true);

            // Re-search at full depth if LMR failed high
            if (value > alpha && d < newDepth) {
                value = -search(pos, ss + 1, -(alpha + 1), -alpha, Depth(newDepth), !cutNode);
            }
        } else if (!pvNode || moveCount > 1) {
            value = -search(pos, ss + 1, -(alpha + 1), -alpha, Depth(newDepth), !cutNode);
        }

        // PV search: re-search with full window
        if (pvNode && (moveCount == 1 || (value > alpha && value != VALUE_ZERO))) {
            (ss+1)->pv = pv_table_[ss->ply + 1];
            (ss+1)->pv[0] = MOVE_NONE;
            value = -search(pos, ss + 1, -beta, -alpha, Depth(newDepth), false);
        }

        pos.undo_move(move);

        if (Threads.stop)
            return VALUE_ZERO;

        // Track searched moves for history updates
        if (isCapture && captureCount < 32)
            capturesSearched[captureCount++] = move;
        else if (!isCapture && quietCount < 64)
            quietsSearched[quietCount++] = move;

        // Update best value
        if (value > bestValue) {
            bestValue = value;

            if (value > alpha) {
                bestMove = move;
                alpha = value;

                if (pvNode) {
                    update_pv(ss->pv, move, (ss+1)->pv);
                }

                if (value >= beta) {
                    // Beta cutoff
                    update_all_stats(pos, ss, bestMove, bestValue, beta,
                                    ss->ply >= 1 ? to_sq((ss-1)->currentMove) : SQ_NONE,
                                    quietsSearched, quietCount,
                                    capturesSearched, captureCount, depth);
                    break;
                }
            }
        }
    }

    // Checkmate / Stalemate
    if (moveCount == 0)
        bestValue = ss->excludedMove ? alpha
                  : ss->inCheck      ? mated_in(ss->ply)
                                     : VALUE_DRAW;

    // Store in TT
    if (!ss->excludedMove && !Threads.stop) {
        Bound b = bestValue >= beta  ? BOUND_LOWER
                : pvNode && bestMove ? BOUND_EXACT
                                     : BOUND_UPPER;
        tte->save(posKey, bestValue, ttPv, b, depth, bestMove,
                  ss->staticEval, TT.generation());
    }

    if (rootNode) {
        // Update root moves
        for (auto& rm : rootMoves) {
            if (rm.pv[0] == bestMove) {
                rm.score = bestValue;
                rm.selDepth = selDepth;
                rm.pv.clear();
                for (Move* m = ss->pv; *m != MOVE_NONE; ++m)
                    rm.pv.push_back(*m);
                break;
            }
        }
    }

    return bestValue;
}

// ──────────────────────────────────────────────
// Quiescence Search
// ──────────────────────────────────────────────

Value SearchWorker::qsearch(Position& pos, SearchStack* ss, Value alpha, Value beta, Depth depth) {
    bool pvNode = (beta - alpha != 1);
    bool inCheck = pos.checkers();

    nodes++;
    check_time();

    if (ss->ply >= MAX_PLY)
        return !inCheck ? Eval::evaluate(pos) : VALUE_DRAW;

    if (ss->ply > selDepth)
        selDepth = ss->ply;

    // Draw check
    if (pos.is_draw(ss->ply))
        return VALUE_DRAW;

    // TT lookup
    bool ttHit;
    Key posKey = pos.key();
    TTEntry* tte = TT.probe(posKey, ttHit);
    Value ttValue = ttHit ? tte->value() : VALUE_NONE;
    Move  ttMove  = ttHit ? tte->move()  : MOVE_NONE;
    bool  ttPv    = pvNode || (ttHit && tte->is_pv());

    if (!pvNode && ttHit && tte->depth() >= depth
        && (tte->bound() & (ttValue >= beta ? BOUND_LOWER : BOUND_UPPER)))
        return ttValue;

    Value bestValue, futilityBase;

    if (inCheck) {
        bestValue = -VALUE_INFINITE;
        ss->staticEval = VALUE_NONE;
    } else {
        if (ttHit) {
            bestValue = ss->staticEval = tte->eval() != VALUE_NONE ? tte->eval() : Eval::evaluate(pos);
            if (ttValue != VALUE_NONE
                && (tte->bound() & (ttValue > bestValue ? BOUND_LOWER : BOUND_UPPER)))
                bestValue = ttValue;
        } else {
            bestValue = ss->staticEval = Eval::evaluate(pos);
        }

        // Stand pat
        if (bestValue >= beta) {
            if (!ttHit)
                tte->save(posKey, bestValue, false, BOUND_LOWER, DEPTH_NONE,
                          MOVE_NONE, ss->staticEval, TT.generation());
            return bestValue;
        }

        alpha = std::max(alpha, bestValue);
        futilityBase = bestValue + 200;
    }

    Move bestMove = MOVE_NONE;
    int moveCount = 0;

    MovePicker mp(pos, ttMove, depth, &mainHistory, &captureHistory);

    Move move;
    while ((move = mp.next_move()) != MOVE_NONE) {
        if (!pos.legal(move))
            continue;

        moveCount++;
        bool givesCheck = pos.gives_check(move);

        // Futility pruning in QSearch
        if (bestValue > VALUE_MATED_IN_MAX_PLY && !givesCheck && !inCheck) {
            Value futilityValue = futilityBase + PieceValue[type_of(pos.piece_on(to_sq(move)))];
            if (futilityValue <= alpha) {
                bestValue = std::max(bestValue, futilityValue);
                continue;
            }

            if (!pos.see_ge(move, Value(1)))
                continue;
        }

        ss->currentMove = move;
        ss->contHistory = &contHistory->table[inCheck][pos.moved_piece(move)][to_sq(move)];

        StateInfo st;
        pos.do_move(move, st, givesCheck);
        Value value = -qsearch(pos, ss + 1, -beta, -alpha, depth - 1);
        pos.undo_move(move);

        if (value > bestValue) {
            bestValue = value;
            if (value > alpha) {
                bestMove = move;
                alpha = value;
                if (value >= beta)
                    break;
            }
        }
    }

    if (inCheck && moveCount == 0)
        return mated_in(ss->ply);

    Bound b = bestValue >= beta ? BOUND_LOWER : BOUND_UPPER;
    tte->save(posKey, bestValue, ttPv, b, depth, bestMove,
              ss->staticEval, TT.generation());

    return bestValue;
}

} // namespace PurnaFish
