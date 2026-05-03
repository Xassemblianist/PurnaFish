/*
 * PurnaFish Chess Engine
 * tt.cpp — Transposition Table implementation
 */

#include "tt.hpp"
#include <cstring>
#include <iostream>

namespace PurnaFish {

TranspositionTable TT;

void TTEntry::save(Key k, Value v, bool pv, Bound b, Depth d, Move m, Value ev, uint8_t gen8) {
    // Preserve any existing move if we don't have one
    if (m || uint16_t(k) != key16_)
        move16_ = uint16_t(m);

    // Overwrite less valuable entries
    if (b == BOUND_EXACT || uint16_t(k) != key16_
        || d - DEPTH_OFFSET + 2 * pv > depth8_ - 4
        || (gen8 & 0xF8) != (genBound8_ & 0xF8)) {
        key16_     = uint16_t(k);
        value16_   = int16_t(v);
        eval16_    = int16_t(ev);
        depth8_    = int8_t(d - DEPTH_OFFSET);
        genBound8_ = uint8_t(gen8 | (pv << 2) | b);
    }
}

void TranspositionTable::resize(size_t mbSize) {
    aligned_free(table_);
    clusterCount_ = mbSize * 1024 * 1024 / sizeof(TTCluster);
    table_ = static_cast<TTCluster*>(aligned_alloc_wrapper(64, clusterCount_ * sizeof(TTCluster)));
    if (!table_) {
        std::cerr << "Failed to allocate " << mbSize << "MB for hash table" << std::endl;
        clusterCount_ = 0;
        return;
    }
    clear();
}

void TranspositionTable::clear() {
    std::memset(table_, 0, clusterCount_ * sizeof(TTCluster));
}

TTEntry* TranspositionTable::probe(Key key, bool& found) const {
    TTEntry* tte = first_entry(key);
    uint16_t key16 = uint16_t(key);

    for (int i = 0; i < TTCluster::ClusterSize; ++i) {
        if (tte[i].key16_ == key16 || !tte[i].depth8_) {
            tte[i].genBound8_ = uint8_t(generation8_ | (tte[i].genBound8_ & 0x7));
            found = tte[i].depth8_ != 0;
            return &tte[i];
        }
    }

    // Replace the least valuable entry
    TTEntry* replace = tte;
    for (int i = 1; i < TTCluster::ClusterSize; ++i) {
        if (replace->depth8_ - ((259 + generation8_ - replace->genBound8_) & 0xF8)
          > tte[i].depth8_ - ((259 + generation8_ - tte[i].genBound8_) & 0xF8))
            replace = &tte[i];
    }

    found = false;
    return replace;
}

int TranspositionTable::hashfull() const {
    int cnt = 0;
    for (int i = 0; i < 1000; ++i) {
        for (int j = 0; j < TTCluster::ClusterSize; ++j) {
            cnt += table_[i].entry[j].depth8_
                && (table_[i].entry[j].genBound8_ & 0xF8) == generation8_;
        }
    }
    return cnt / TTCluster::ClusterSize;
}

} // namespace PurnaFish
