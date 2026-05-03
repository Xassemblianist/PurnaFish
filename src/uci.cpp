/*
 * PurnaFish Chess Engine
 * uci.cpp — UCI Protocol implementation
 */

#include "uci.hpp"
#include "types.hpp"
#include "position.hpp"
#include "search.hpp"
#include "thread.hpp"
#include "tt.hpp"
#include "movegen.hpp"
#include "perft.hpp"
#include "misc.hpp"
#include "nnue/nnue_eval.hpp"
#include "syzygy/syzygy.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <deque>

namespace PurnaFish {

namespace {

Position rootPos;
std::deque<StateInfo> stateHistory;

Move parse_move(const Position& pos, const std::string& str) {
    if (str.length() < 4) return MOVE_NONE;

    Square from = make_square(File(str[0] - 'a'), Rank(str[1] - '1'));
    Square to   = make_square(File(str[2] - 'a'), Rank(str[3] - '1'));

    for (const auto& sm : MoveList(pos)) {
        Move m = sm.move;
        if (from_sq(m) == from && to_sq(m) == to) {
            if (type_of(m) == PROMOTION) {
                char promo = str.length() > 4 ? str[4] : 'q';
                PieceType pt = promo == 'n' ? KNIGHT : promo == 'b' ? BISHOP :
                               promo == 'r' ? ROOK : QUEEN;
                if (promotion_type(m) == pt) return m;
            } else {
                return m;
            }
        }
    }
    return MOVE_NONE;
}

void cmd_position(std::istringstream& is) {
    std::string token;
    is >> token;

    stateHistory.clear();
    stateHistory.emplace_back();

    if (token == "startpos") {
        rootPos.set(StartFEN, &stateHistory.back());
        is >> token; // consume "moves" if present
    } else if (token == "fen") {
        std::string fen;
        while (is >> token && token != "moves")
            fen += token + " ";
        rootPos.set(fen, &stateHistory.back());
    }

    // Apply moves
    while (is >> token) {
        Move m = parse_move(rootPos, token);
        if (m == MOVE_NONE) break;
        stateHistory.emplace_back();
        rootPos.do_move(m, stateHistory.back());
    }
}

void cmd_go(std::istringstream& is) {
    SearchLimits limits;
    std::string token;

    while (is >> token) {
        if      (token == "wtime")     is >> limits.time[WHITE];
        else if (token == "btime")     is >> limits.time[BLACK];
        else if (token == "winc")      is >> limits.inc[WHITE];
        else if (token == "binc")      is >> limits.inc[BLACK];
        else if (token == "movestogo") is >> limits.movestogo;
        else if (token == "depth")     is >> limits.depth;
        else if (token == "nodes")     is >> limits.nodes;
        else if (token == "movetime")  is >> limits.movetime;
        else if (token == "infinite")  limits.infinite = true;
        else if (token == "ponder")    limits.ponder = true;
        else if (token == "perft") {
            int depth;
            is >> depth;
            perft_divide(rootPos, depth);
            return;
        }
    }

    Threads.start_search(rootPos, limits);
}

void cmd_setoption(std::istringstream& is) {
    std::string token, name, value;

    is >> token; // "name"
    while (is >> token && token != "value")
        name += (name.empty() ? "" : " ") + token;
    while (is >> token)
        value += (value.empty() ? "" : " ") + token;

    if (name == "Hash") {
        int mb = std::stoi(value);
        TT.resize(std::max(1, std::min(mb, 65536)));
    } else if (name == "Threads") {
        int n = std::stoi(value);
        Threads.set_size(std::max(1, std::min(n, 512)));
    } else if (name == "Clear Hash") {
        TT.clear();
    } else if (name == "EvalFile") {
        NNUE::load(value);
    } else if (name == "SyzygyPath") {
        Tablebases::init(value);
    }
}

} // anonymous namespace

namespace UCI {

void init() {
    // Default initialization
    TT.resize(64);         // 64 MB default hash
    Threads.set_size(1);   // Single thread default
}

void loop() {
    init();

    stateHistory.emplace_back();
    rootPos.set(StartFEN, &stateHistory.back());

    std::string line, token;

    while (std::getline(std::cin, line)) {
        std::istringstream is(line);
        is >> token;

        if (token == "uci") {
            std::cout << "id name PurnaFish 1.1.0" << std::endl;
            std::cout << "id author PurnaFish Authors" << std::endl;
            std::cout << std::endl;
            std::cout << "option name Hash type spin default 64 min 1 max 65536" << std::endl;
            std::cout << "option name Threads type spin default 1 min 1 max 512" << std::endl;
            std::cout << "option name Clear Hash type button" << std::endl;
            std::cout << "option name EvalFile type string default <empty>" << std::endl;
            std::cout << "option name SyzygyPath type string default <empty>" << std::endl;
            std::cout << "uciok" << std::endl;
        }
        else if (token == "isready") {
            std::cout << "readyok" << std::endl;
        }
        else if (token == "ucinewgame") {
            TT.clear();
            Threads.clear();
        }
        else if (token == "position") {
            cmd_position(is);
        }
        else if (token == "go") {
            cmd_go(is);
        }
        else if (token == "stop") {
            Threads.stop = true;
        }
        else if (token == "setoption") {
            cmd_setoption(is);
        }
        else if (token == "d") {
            std::cout << rootPos.to_string() << std::endl;
        }
        else if (token == "quit" || token == "exit") {
            break;
        }
        else if (token == "perft") {
            int depth;
            if (is >> depth)
                perft_divide(rootPos, depth);
        }
        else if (token == "bench") {
            // Quick benchmark
            SearchLimits limits;
            limits.depth = 12;
            Threads.start_search(rootPos, limits);
        }
    }
}

} // namespace UCI

} // namespace PurnaFish
