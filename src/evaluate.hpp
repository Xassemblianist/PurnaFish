/*
 * PurnaFish Chess Engine
 * evaluate.hpp — Evaluation interface
 */

#pragma once

#include "types.hpp"
#include "position.hpp"

namespace PurnaFish {

namespace Eval {

/// Main evaluation function — uses NNUE if available, otherwise HCE
Value evaluate(const Position& pos);

} // namespace Eval

} // namespace PurnaFish
