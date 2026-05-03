/*
 * PurnaFish Chess Engine
 * main.cpp — Entry point
 *
 *  ____                        _____ _     _
 * |  _ \ _   _ _ __ _ __   __ |  ___(_)___| |__
 * | |_) | | | | '__| '_ \ / _`| |_  | / __| '_ \
 * |  __/| |_| | |  | | | | (_|| |   | \__ \ | | |
 * |_|    \__,_|_|  |_| |_|\__,|_|   |_|___/_| |_|
 *
 * A world-class chess engine with CPU SIMD and CUDA GPU acceleration.
 */

#include "bitboard.hpp"
#include "zobrist.hpp"
#include "search.hpp"
#include "uci.hpp"
#include "datagen.hpp"
#include <string>

int main(int argc, char* argv[]) {
    PurnaFish::Bitboards::init();
    PurnaFish::Zobrist::init();
    PurnaFish::Search::init();

    if (argc > 1 && std::string(argv[1]) == "datagen") {
        int concurrency = 1;
        int maxGames = 1000;
        std::string filename = "data.txt";
        
        for (int i = 2; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--threads" && i + 1 < argc) concurrency = std::stoi(argv[++i]);
            else if (arg == "--games" && i + 1 < argc) maxGames = std::stoi(argv[++i]);
            else if (arg == "--out" && i + 1 < argc) filename = argv[++i];
        }
        
        PurnaFish::Datagen::generate_data(concurrency, maxGames, filename);
        return 0;
    }

    PurnaFish::UCI::loop();
    return 0;
}
