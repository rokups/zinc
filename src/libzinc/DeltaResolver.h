/**
 * MIT License
 *
 * Copyright (c) 2017 Rokas Kupstys
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#pragma once


#include <thread>
#include <mutex>
#include <atomic>
#include <ThreadPool.h>
#include "zinc/zinc.h"
#include "hashmaps.h"

namespace zinc
{

class DeltaResolver
{
protected:
    const RemoteFileHashList& hashes;
    const ProgressCallback& report_progress;
    const uint8_t* file_data = nullptr;
    int64_t file_size = 0;
    size_t block_size = 0;
    ThreadPool pool;
    std::vector<std::future<void>> pending_tasks;

#if ZINC_USE_SKA_FLAT_HASH_MAP
    ska::flat_hash_map<WeakHash, ska::flat_hash_map<StrongHash, int64_t, StrongHashHashFunction>> lookup_table;
    ska::flat_hash_map<StrongHash, std::set<int64_t>, StrongHashHashFunction> identical_blocks;
#else
    std::unordered_map<WeakHash, std::vector<std::pair<int64_t, const BlockHashes*>>> lookup_table;
    std::unordered_map<StrongHash, std::set<int64_t>> identical_blocks;
#endif
    int64_t last_progress_report = 0;
    std::atomic<int64_t> bytes_consumed_total;
    size_t concurrent_threads;
public:
    DeltaMap delta;

    DeltaResolver(const void* file_data, int64_t file_size, size_t block_size, const RemoteFileHashList& hashes,
                  const ProgressCallback& report_progress, size_t concurrent_threads);

    /// Start delta resolving in parallel. Method returns immediately. `thread_chunk_size` specifies how much data one
    /// thread should process. No more than `concurrent_threads` number of threads specified in constructor will run at
    /// a time.
    void start(int64_t thread_chunk_size=10 * 1024 * 1024);
    /// Return number of bytes already processed.
    int64_t bytes_done() const;
    /// Waits for delta resolver to finish all work. Starts threads as needed and reports progress.
    void wait();

protected:
    /// Thread worker.
    void resolve_concurrent(int64_t start_index, int64_t block_length);
};

}
