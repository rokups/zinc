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
#include "mmap/FileMemoryMap.h"
#include "zinc/zinc.h"
#include "hashmaps.h"
#include "Task.hpp"


namespace zinc
{

class DeltaResolver : public Task<DeltaMap>
{
public:
    /// Construct dummy task that does nothing.
    DeltaResolver();
    /// Construct from memory block.
    DeltaResolver(const void* file_data, int64_t file_size, size_t block_size, const RemoteFileHashList& hashes,
                  size_t thread_count);
    /// Construct from file.
    DeltaResolver(const char* file_name, size_t block_size, const RemoteFileHashList& hashes, size_t thread_count);

    ~DeltaResolver() override = default;

    DeltaMap& result() override;

protected:
    FileMemoryMap _mapping;
    const RemoteFileHashList* _hashes;
    int64_t _block_size = 0;
#if ZINC_USE_SKA_FLAT_HASH_MAP
    ska::flat_hash_map<WeakHash, ska::flat_hash_map<StrongHash, int64_t, StrongHashHashFunction>> lookup_table;
#else
    std::unordered_map<WeakHash, std::vector<std::pair<int64_t, const BlockHashes*>>> lookup_table;
#endif

    /// Queues tasks to threadpool for processing blocks.
    void queue_tasks();
    /// Hash some blocks of file. Several instances of this method may run in parallel.
    void process(int64_t start_index, int64_t block_length);
};

}
