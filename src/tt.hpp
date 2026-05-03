/*
 * PurnaFish Chess Engine
 * tt.hpp — Transposition Table
 */

#pragma once

#include "types.hpp"
#include "misc.hpp"
#include <cstring>

namespace PurnaFish {

/// TTEntry packs data into 10 bytes for cache efficiency.
/// 3 entries fit in a 32-byte cache line (with 2 padding bytes).
struct TTEntry {
    Move  move()      const { return Move(move16_); }
    Value value()     const { return Value(value16_); }
    Value eval()      const { return Value(eval16_); }
    Depth depth()     const { return Depth(depth8_ + DEPTH_OFFSET); }
    bool  is_pv()     const { return bool(genBound8_ & 0x4); }
    Bound bound()     const { return Bound(genBound8_ & 0x3); }
    void  save(Key k, Value v, bool pv, Bound b, Depth d, Move m, Value ev, uint8_t gen8);

private:
    friend class TranspositionTable;
    uint16_t key16_;
    int16_t  value16_;
    int16_t  eval16_;
    uint16_t move16_;
    int8_t   depth8_;
    uint8_t  genBound8_;
};

/// TTCluster: 3 entries = 30 bytes, padded to 32 bytes
struct TTCluster {
    static constexpr int ClusterSize = 3;
    TTEntry entry[ClusterSize];
    char padding[2];
};

static_assert(sizeof(TTCluster) == 32, "TTCluster must be 32 bytes");

class TranspositionTable {
public:
    ~TranspositionTable() { aligned_free(table_); }

    void resize(size_t mbSize);
    void clear();
    void new_search() { generation8_ += 8; } // Increment by 8 to avoid collision with bound bits

    TTEntry* probe(Key key, bool& found) const;
    int hashfull() const;
    TTEntry* first_entry(Key key) const;

    uint8_t generation() const { return generation8_; }

private:
    size_t     clusterCount_ = 0;
    TTCluster* table_        = nullptr;
    uint8_t    generation8_  = 0;
};

inline TTEntry* TranspositionTable::first_entry(Key key) const {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
    using u128 = unsigned __int128;
    return &table_[size_t((u128(key) * u128(clusterCount_)) >> 64)].entry[0];
#pragma GCC diagnostic pop
}

extern TranspositionTable TT;

} // namespace PurnaFish
