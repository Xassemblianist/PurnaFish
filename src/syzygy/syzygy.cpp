#include "syzygy.hpp"
#include "tbprobe.h"
#include "../movegen.hpp"
#include <iostream>

namespace PurnaFish {

namespace Tablebases {

int MaxPieces = 0;

bool init(const std::string& path) {
    bool success = tb_init(path.c_str());
    if (success) {
        MaxPieces = TB_LARGEST;
        std::cout << "info string Syzygy tablebases loaded. Max pieces: " << MaxPieces << std::endl;
    } else {
        std::cerr << "info string Failed to load Syzygy tablebases from " << path << std::endl;
    }
    return success;
}

void free() {
    tb_free();
    MaxPieces = 0;
}

Value wdl_to_value(int wdl, int ply) {
    switch (wdl) {
        case TB_WIN: return VALUE_TB_WIN - ply;
        case TB_LOSS: return -VALUE_TB_WIN + ply;
        case TB_DRAW: return VALUE_DRAW;
        case TB_CURSED_WIN: return VALUE_DRAW;
        case TB_BLESSED_LOSS: return VALUE_DRAW;
        default: return VALUE_NONE;
    }
}

bool probe_wdl(const Position& pos, Value& wdl_value) {
    if (pos.piece_count() > MaxPieces) return false;
    
    unsigned res = tb_probe_wdl(
        pos.pieces(WHITE), pos.pieces(BLACK),
        pos.pieces(KING), pos.pieces(QUEEN),
        pos.pieces(ROOK), pos.pieces(BISHOP),
        pos.pieces(KNIGHT), pos.pieces(PAWN),
        pos.rule50_count(),
        pos.castling_rights(),
        pos.ep_square() == SQ_NONE ? 0 : pos.ep_square(),
        pos.side_to_move() == WHITE
    );

    if (res == TB_RESULT_FAILED) return false;
    wdl_value = wdl_to_value(res, pos.game_ply());
    return true;
}

bool probe_root(const Position& pos, Move& bestMove, Value& dtz_value) {
    if (pos.piece_count() > MaxPieces) return false;

    unsigned results[TB_MAX_MOVES];
    unsigned res = tb_probe_root(
        pos.pieces(WHITE), pos.pieces(BLACK),
        pos.pieces(KING), pos.pieces(QUEEN),
        pos.pieces(ROOK), pos.pieces(BISHOP),
        pos.pieces(KNIGHT), pos.pieces(PAWN),
        pos.rule50_count(),
        pos.castling_rights(),
        pos.ep_square() == SQ_NONE ? 0 : pos.ep_square(),
        pos.side_to_move() == WHITE,
        results
    );

    if (res == TB_RESULT_FAILED || res == TB_RESULT_CHECKMATE || res == TB_RESULT_STALEMATE) {
        return false;
    }

    int from = TB_GET_FROM(res);
    int to = TB_GET_TO(res);
    int promotes = TB_GET_PROMOTES(res);

    // Reconstruct the move
    for (const auto& sm : MoveList(pos)) {
        Move m = sm.move;
        if (from_sq(m) == Square(from) && to_sq(m) == Square(to)) {
            if (type_of(m) == PROMOTION) {
                PieceType pt = promotes == TB_PROMOTES_QUEEN ? QUEEN :
                               promotes == TB_PROMOTES_ROOK ? ROOK :
                               promotes == TB_PROMOTES_BISHOP ? BISHOP : KNIGHT;
                if (promotion_type(m) == pt) {
                    bestMove = m;
                    break;
                }
            } else {
                bestMove = m;
                break;
            }
        }
    }

    int dtz = TB_GET_DTZ(res);
    int wdl = TB_GET_WDL(res);
    
    // Convert DTZ to a somewhat stable score
    if (wdl == TB_WIN) {
        dtz_value = VALUE_MATE - 1000 - dtz;
    } else if (wdl == TB_LOSS) {
        dtz_value = -VALUE_MATE + 1000 + dtz;
    } else {
        dtz_value = VALUE_DRAW;
    }

    return true;
}

} // namespace Tablebases
} // namespace PurnaFish
