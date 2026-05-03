/*
 * PurnaFish Chess Engine
 * thread.hpp — Thread management for Lazy SMP
 */

#pragma once

#include "search.hpp"
#include "position.hpp"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <memory>
#include <atomic>

namespace PurnaFish {

class SearchThread {
public:
    SearchThread(size_t idx);
    virtual ~SearchThread();

    void start_searching();
    void wait_for_search_finished();
    void idle_loop();

    SearchWorker worker;
    std::thread thread;
    std::mutex mutex;
    std::condition_variable cv;
    bool exit = false;
    bool searching = false;
    size_t id;

    Position rootPos;
    StateInfo rootState;
};

class ThreadPool {
public:
    ThreadPool() = default;
    ~ThreadPool() { set_size(0); }

    void set_size(int numThreads);
    void start_search(Position& pos, const SearchLimits& limits);
    void wait_for_search();
    void clear();

    uint64_t nodes_searched() const;

    SearchWorker* main() { return threads_.empty() ? nullptr : &threads_[0]->worker; }
    int size() const { return int(threads_.size()); }

    std::atomic<bool> stop;

private:
    std::vector<std::unique_ptr<SearchThread>> threads_;
};

extern ThreadPool Threads;

} // namespace PurnaFish
