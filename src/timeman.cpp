/*
 * PurnaFish Chess Engine
 * timeman.cpp — Time management implementation
 */

#include "timeman.hpp"
#include "search.hpp"
#include <algorithm>
#include <cmath>

namespace PurnaFish {

void TimeManager::init(const SearchLimits& limits, Color us, int ply) {
    // Fixed time per move
    if (limits.movetime) {
        optimumTime_ = limits.movetime;
        maximumTime_ = limits.movetime;
        return;
    }

    // Infinite or fixed depth
    if (limits.infinite || limits.depth) {
        optimumTime_ = TimePoint(1ULL << 40);
        maximumTime_ = TimePoint(1ULL << 40);
        return;
    }

    TimePoint time = limits.time[us];
    TimePoint inc  = limits.inc[us];
    int mtg = limits.movestogo ? limits.movestogo : 50;

    // Time allocation formula
    TimePoint baseTime = std::max(TimePoint(1), time);
    TimePoint overhead = TimePoint(50); // Safety margin

    double optScale = std::min(0.0120 + std::pow(ply + 2.0, 0.44) * 0.0060,
                               0.20 * baseTime / std::max(TimePoint(1), time));
    double maxScale = std::min(7.0, 0.05 + 0.30 * std::pow(ply + 2.0, 0.24));

    // Moves to go adjustment
    if (limits.movestogo) {
        optScale = std::min(optScale, 0.95 / double(mtg));
        maxScale = std::min(maxScale, 6.5 / double(mtg));
    }

    optimumTime_ = TimePoint(std::max(1.0, optScale * (baseTime - overhead) + inc * 0.75));
    maximumTime_ = TimePoint(std::max(1.0, maxScale * optimumTime_));

    // Never use more than the available time
    maximumTime_ = std::min(maximumTime_, std::max(TimePoint(1), time - overhead));
    optimumTime_ = std::min(optimumTime_, maximumTime_);
}

} // namespace PurnaFish
