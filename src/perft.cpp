/*
 * PurnaFish Chess Engine
 * perft.cpp — Perft implementation for move generation testing
 */

#include "perft.hpp"
#include "movegen.hpp"
#include "misc.hpp"
#include <iostream>

namespace PurnaFish {

uint64_t perft(Position& pos, int depth) {
    if (depth == 0)
        return 1;

    MoveList moves(pos);
    if (depth == 1)
        return moves.size();

    uint64_t nodes = 0;
    StateInfo st;

    for (const auto& sm : moves) {
        pos.do_move(sm.move, st);
        nodes += perft(pos, depth - 1);
        pos.undo_move(sm.move);
    }

    return nodes;
}

void perft_divide(Position& pos, int depth) {
    MoveList moves(pos);
    uint64_t total = 0;

    TimePoint start = now();

    for (const auto& sm : moves) {
        StateInfo st;
        pos.do_move(sm.move, st);
        uint64_t cnt = (depth > 1) ? perft(pos, depth - 1) : 1;
        pos.undo_move(sm.move);

        Move m = sm.move;
        std::cout << char('a' + file_of(from_sq(m)))
                  << char('1' + rank_of(from_sq(m)))
                  << char('a' + file_of(to_sq(m)))
                  << char('1' + rank_of(to_sq(m)));

        if (type_of(m) == PROMOTION) {
            const char promo[] = {' ', ' ', 'n', 'b', 'r', 'q'};
            std::cout << promo[promotion_type(m)];
        }

        std::cout << ": " << cnt << std::endl;
        total += cnt;
    }

    TimePoint elapsed = now() - start;
    std::cout << "\nTotal: " << total
              << "\nTime:  " << elapsed << " ms"
              << "\nNPS:   " << (elapsed > 0 ? total * 1000 / elapsed : total)
              << std::endl;
}

} // namespace PurnaFish
