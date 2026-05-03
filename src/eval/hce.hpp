/*
 * PurnaFish Chess Engine
 * eval/hce.hpp — Hand-Crafted Evaluation
 */

#pragma once

#include "../types.hpp"
#include "../position.hpp"

namespace PurnaFish::HCE {

Value evaluate(const Position& pos);

} // namespace PurnaFish::HCE
