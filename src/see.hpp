/*
 * PurnaFish Chess Engine
 * see.hpp — Static Exchange Evaluation (standalone utilities)
 */

#pragma once

#include "types.hpp"
#include "position.hpp"

namespace PurnaFish {

// SEE is implemented as Position::see_ge() in position.cpp
// This header provides additional SEE-related utilities

namespace SEE {

// Full SEE value computation
inline Value see_value(const Position& pos, Move m) {
    // Binary search for the threshold where SEE becomes negative
    Value lo = Value(-int(QueenValue));
    Value hi = Value(int(QueenValue));

    while (lo < hi) {
        Value mid = Value((int(lo) + int(hi) + 1) / 2);
        if (pos.see_ge(m, mid))
            lo = mid;
        else
            hi = mid - 1;
    }
    return lo;
}

} // namespace SEE

} // namespace PurnaFish
