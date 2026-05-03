/*
 * PurnaFish Chess Engine
 * timeman.hpp — Time management
 */

#pragma once

#include "types.hpp"
#include "misc.hpp"

namespace PurnaFish {

struct SearchLimits;

class TimeManager {
public:
    void init(const SearchLimits& limits, Color us, int ply);

    TimePoint optimum() const { return optimumTime_; }
    TimePoint maximum() const { return maximumTime_; }

private:
    TimePoint optimumTime_ = 0;
    TimePoint maximumTime_ = 0;
};

} // namespace PurnaFish
