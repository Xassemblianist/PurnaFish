/*
 * PurnaFish Chess Engine
 * search.hpp — Search framework
 */

#pragma once

#include "types.hpp"
#include "position.hpp"
#include "movepick.hpp"
#include "tt.hpp"
#include "misc.hpp"
#include <atomic>
#include <vector>
#include <cstring>

namespace PurnaFish {

/// Continuation history table wrap for heap allocation
struct ContHistory {
    PieceToHistory table[2][PIECE_NB][SQUARE_NB];
};

/// Search stack — per-ply search data
struct SearchStack {
    Move*  pv;
    PieceToHistory* contHistory;
    int    ply;
    Move   currentMove;
    Move   excludedMove;
    Move   killers[2];
    Value  staticEval;
    int    statScore;
    int    moveCount;
    bool   inCheck;
    bool   ttPv;
    bool   ttHit;
};

/// Search limits from UCI
struct SearchLimits {
    TimePoint time[COLOR_NB] = {};
    TimePoint inc[COLOR_NB]  = {};
    int       movestogo      = 0;
    int       depth          = 0;
    int64_t   nodes          = 0;
    int       mate           = 0;
    TimePoint movetime       = 0;
    bool      infinite       = false;
    bool      ponder         = false;
    int       multiPV        = 1;
};

/// Root move — moves at root with their PV
struct RootMove {
    explicit RootMove(Move m) : pv(1, m) {}
    bool operator<(const RootMove& o) const { return o.score < score; }
    bool operator==(Move m) const { return pv[0] == m; }

    Value score       = -VALUE_INFINITE;
    Value previousScore = -VALUE_INFINITE;
    Value avgScore    = -VALUE_INFINITE;
    int   selDepth    = 0;
    std::vector<Move> pv;
};

/// SearchWorker — one per thread, contains search state
class SearchWorker {
public:
    SearchWorker() { clear(); }

    void clear();
    void start_search(Position& pos, const SearchLimits& limits,
                      const std::vector<RootMove>& rootMoves);

    // Search functions
    void iterative_deepening();
    Value search(Position& pos, SearchStack* ss, Value alpha, Value beta,
                 Depth depth, bool cutNode);
    Value qsearch(Position& pos, SearchStack* ss, Value alpha, Value beta, Depth depth);

    // State
    std::vector<RootMove> rootMoves;
    SearchLimits limits;
    int completedDepth = 0;
    int selDepth = 0;
    uint64_t nodes = 0;
    bool stop = false;
    int threadId = 0;

    // History tables
    ButterflyHistory mainHistory;
    CapturePieceToHistory captureHistory;
    std::unique_ptr<ContHistory> contHistory;
    CounterMoveHistory counterMoves;

    // Timing
    TimePoint startTime = 0;
    TimePoint optimumTime = 0;
    TimePoint maximumTime = 0;

private:
    Position* rootPos_ = nullptr;
    SearchStack stack_[MAX_PLY + 10];
    Move pv_table_[MAX_PLY + 10][MAX_PLY + 10];

    void update_pv(Move* pv, Move bestMove, const Move* childPv);
    void update_continuation_histories(SearchStack* ss, Piece pc, Square to, int bonus);
    void update_quiet_stats(Position& pos, SearchStack* ss, Move bestMove,
                           int bonus, Move* quietsSearched, int quietCount);
    void update_all_stats(Position& pos, SearchStack* ss, Move bestMove,
                         Value bestValue, Value beta, Square prevSq,
                         Move* quietsSearched, int quietCount,
                         Move* capturesSearched, int captureCount, Depth depth);
    int stat_bonus(Depth d) const;
    int stat_malus(Depth d) const;
    void check_time();
};

namespace Search {
    void init();
    extern int Contempt;
}

// LMR reduction table
extern int Reductions[MAX_PLY][MAX_MOVES];

} // namespace PurnaFish
