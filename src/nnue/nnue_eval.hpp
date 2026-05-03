/*
 * PurnaFish Chess Engine
 * nnue/nnue_eval.hpp — NNUE evaluation interface
 */

#pragma once

#include "nnue_common.hpp"
#include "../types.hpp"
#include "../position.hpp"
#include <string>

namespace PurnaFish::NNUE {

/// Load NNUE weights from a file
bool load(const std::string& filename);

/// Check if NNUE is loaded and ready
bool is_loaded();

/// Evaluate position using NNUE
Value evaluate(const Position& pos);

/// Get NNUE filename
const std::string& filename();

} // namespace PurnaFish::NNUE
