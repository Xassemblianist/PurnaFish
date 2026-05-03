/*
 * PurnaFish Chess Engine
 * thread.cpp — Thread pool implementation
 */

#include "thread.hpp"
#include "movegen.hpp"
#include "tt.hpp"
#include <iostream>

namespace PurnaFish {

ThreadPool Threads;

SearchThread::SearchThread(size_t idx) : id(idx) {
    worker.threadId = idx;
    thread = std::thread(&SearchThread::idle_loop, this);
}

SearchThread::~SearchThread() {
    {
        std::unique_lock<std::mutex> lk(mutex);
        exit = true;
        cv.notify_one();
    }
    if (thread.joinable())
        thread.join();
}

void SearchThread::idle_loop() {
    while (true) {
        std::unique_lock<std::mutex> lk(mutex);
        cv.wait(lk, [this] { return searching || exit; });

        if (exit) break;
        lk.unlock();

        worker.iterative_deepening();

        lk.lock();
        searching = false;
        cv.notify_all();
    }
}

void SearchThread::start_searching() {
    std::unique_lock<std::mutex> lk(mutex);
    searching = true;
    cv.notify_one();
}

void SearchThread::wait_for_search_finished() {
    std::unique_lock<std::mutex> lk(mutex);
    cv.wait(lk, [this] { return !searching; });
}

void ThreadPool::set_size(int numThreads) {
    wait_for_search();
    threads_.clear();
    for (int i = 0; i < numThreads; ++i)
        threads_.push_back(std::make_unique<SearchThread>(i));
}

uint64_t ThreadPool::nodes_searched() const {
    uint64_t nodes = 0;
    for (const auto& t : threads_)
        nodes += t->worker.nodes;
    return nodes;
}

void ThreadPool::start_search(Position& pos, const SearchLimits& limits) {
    if (threads_.empty()) return;

    wait_for_search();
    stop = false;

    TT.new_search();

    // Generate root moves
    std::vector<RootMove> rootMoves;
    for (const auto& m : MoveList(pos)) {
        if (pos.legal(m.move)) {
            rootMoves.emplace_back(m.move);
        }
    }

    if (rootMoves.empty()) {
        std::cout << "bestmove (none)" << std::endl;
        return;
    }

    // Prepare each thread
    std::string fen = pos.fen();
    for (size_t i = 0; i < threads_.size(); ++i) {
        auto& t = threads_[i];
        
        t->rootPos.set(fen, &t->rootState);
        t->worker.start_search(t->rootPos, limits, rootMoves);
    }

    // Start all threads
    for (size_t i = 0; i < threads_.size(); ++i) {
        threads_[i]->start_searching();
    }
}

void ThreadPool::wait_for_search() {
    for (auto& t : threads_)
        t->wait_for_search_finished();
}

void ThreadPool::clear() {
    wait_for_search();
    for (auto& t : threads_)
        t->worker.clear();
}

} // namespace PurnaFish
