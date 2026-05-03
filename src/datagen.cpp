#include "datagen.hpp"
#include "position.hpp"
#include "search.hpp"
#include "movegen.hpp"
#include "thread.hpp"
#include "tt.hpp"
#include "syzygy/syzygy.hpp"
#include "nnue/nnue_eval.hpp"

#include <iostream>
#include <fstream>
#include <vector>
#include <random>
#include <thread>
#include <mutex>
#include <iomanip>

namespace PurnaFish {

namespace Datagen {

struct DataRecord {
    std::string fen;
    int score; // from perspective of the side to move
};

struct GameResult {
    std::vector<DataRecord> records;
    float result; // 1.0 for White, 0.0 for Black, 0.5 for Draw
};

std::mutex fileMutex;
int gamesCompleted = 0;
int totalGames = 0;

Move random_legal_move(const Position& pos, std::mt19937& rng) {
    std::vector<Move> legalMoves;
    for (const auto& sm : MoveList(pos)) {
        if (pos.legal(sm.move)) {
            legalMoves.push_back(sm.move);
        }
    }
    if (legalMoves.empty()) return MOVE_NONE;
    std::uniform_int_distribution<size_t> dist(0, legalMoves.size() - 1);
    return legalMoves[dist(rng)];
}

void play_game(std::ofstream& outFile, int gameIdx) {
    std::mt19937 rng(std::random_device{}() + gameIdx);
    
    // 1. Play 8 random moves from startpos
    Position pos;
    StateInfo st[512];
    pos.set(StartFEN, &st[0]);

    for (int i = 0; i < 8; ++i) {
        Move m = random_legal_move(pos, rng);
        if (m == MOVE_NONE) return; // Dead end
        pos.do_move(m, st[i + 1]);
    }

    GameResult game;
    int ply = 8;
    float finalResult = 0.5f;

    SearchLimits limits;
    limits.depth = 8; // Shallow depth for datagen

    while (true) {
        MoveList ml(pos);
        if (ml.size() == 0) {
            if (pos.checkers()) finalResult = pos.side_to_move() == WHITE ? 0.0f : 1.0f;
            else finalResult = 0.5f;
            break;
        }

        if (pos.rule50_count() >= 100) {
            finalResult = 0.5f;
            break;
        }

        // Search the position
        Threads.start_search(pos, limits);
        Threads.wait_for_search();
        
        Move bestMove = Threads.main()->rootMoves[0].pv[0];
        Value score = Threads.main()->rootMoves[0].score;

        if (bestMove == MOVE_NONE) {
            finalResult = 0.5f; // Failsafe
            break;
        }

        // Syzygy Adjudication
        Value tbValue;
        if (Tablebases::MaxPieces && pos.piece_count() <= Tablebases::MaxPieces && !pos.castling_rights()) {
            if (Tablebases::probe_wdl(pos, tbValue)) {
                if (tbValue > VALUE_DRAW) finalResult = pos.side_to_move() == WHITE ? 1.0f : 0.0f;
                else if (tbValue < VALUE_DRAW) finalResult = pos.side_to_move() == WHITE ? 0.0f : 1.0f;
                else finalResult = 0.5f;
                break;
            }
        }

        // Save record (skip captures/checks for stability)
        if (!pos.checkers() && !pos.capture(bestMove) && std::abs(score) < VALUE_MATE_IN_MAX_PLY) {
            game.records.push_back({pos.fen(), pos.side_to_move() == WHITE ? score : -score});
        }

        pos.do_move(bestMove, st[ply + 1]);
        ply++;

        // Mate adjudication
        if (std::abs(score) > VALUE_MATE_IN_MAX_PLY) {
            finalResult = score > 0 ? 
                (pos.side_to_move() == WHITE ? 1.0f : 0.0f) :
                (pos.side_to_move() == WHITE ? 0.0f : 1.0f);
            break;
        }
    }

    // Write to file
    std::lock_guard<std::mutex> lock(fileMutex);
    for (const auto& rec : game.records) {
        outFile << rec.fen << " | " << rec.score << " | " << finalResult << "\n";
    }
    gamesCompleted++;
    if (gamesCompleted % 10 == 0) {
        std::cout << "info string Datagen: " << gamesCompleted << " / " << totalGames << " games completed.\n";
    }
}

void generate_data(int concurrency, int maxGames, const std::string& filename) {
    totalGames = maxGames;
    gamesCompleted = 0;

    std::ofstream outFile(filename);
    if (!outFile) {
        std::cerr << "Failed to open output file " << filename << std::endl;
        return;
    }

    std::cout << "info string Starting datagen. Output: " << filename << std::endl;
    
    // Initialize search
    TT.resize(16); // 16MB is enough for depth 8
    Threads.set_size(1); // 1 thread for the search itself, we aren't parallelizing games in this simple loop
    
    // We will just do a simple loop for now
    for (int i = 0; i < maxGames; ++i) {
        play_game(outFile, i);
    }

    std::cout << "info string Datagen finished." << std::endl;
}

} // namespace Datagen
} // namespace PurnaFish
