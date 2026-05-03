#pragma once

#include "types.hpp"
#include <string>

namespace PurnaFish {

namespace Datagen {

// Run self-play data generation
// concurrency: number of threads
// maxGames: total games to play
// filename: output file name
void generate_data(int concurrency, int maxGames, const std::string& filename);

} // namespace Datagen

} // namespace PurnaFish
