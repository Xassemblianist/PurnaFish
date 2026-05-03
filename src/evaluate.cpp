/*
 * PurnaFish Chess Engine
 * evaluate.cpp — Evaluation implementation (HCE)
 */

#include "evaluate.hpp"
#include "eval/hce.hpp"
#include "nnue/nnue_eval.hpp"

namespace PurnaFish {

namespace Eval {

Value evaluate(const Position& pos) {
    if (NNUE::is_loaded()) {
        return NNUE::evaluate(pos);
    }
    return HCE::evaluate(pos);
}

} // namespace Eval

} // namespace PurnaFish
