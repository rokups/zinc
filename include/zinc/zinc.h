/*
 * MIT License
 *
 * Copyright (c) 2018 Rokas Kupstys
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


#include <atomic>
#include <cstdint>
#include <future>
#include <vector>


namespace zinc
{

/// Descriptor of chunk boundary.
struct Boundary
{
    /// Chunk start.
    int64_t start;
    /// Chunk fingerprint which is buzhash checksum of first `Parameters::window_length` bytes.
    uint64_t fingerprint;
    /// fnv64a hash of entire chunk.
    uint64_t hash;
    /// Length of the chunk.
    int64_t length;
};
using BoundaryList = std::vector<Boundary>;

/// Descriptor of sync operation.
struct SyncOperation
{
    /// A remote block information. Destination in new local file.
    const Boundary* remote;
    /// A local block information. Source in local file when data exists. May be null, in which case block does not exist locally.
    const Boundary* local;
};
using SyncOperationList = std::vector<SyncOperation>;

/// Parameters for chunking algorithm and progress reporting.
struct Parameters
{
    /// Window size for buzhash algorithm. Fingerprint is buzhash(&block_start, window_length).
    unsigned window_length = 4095;
    /// Blocks size less than specified here will be removed.
    unsigned min_block_size = 512 * 1024;
    /// Blocks with size more than specified here will be split into smaller blocks of equal size.
    unsigned max_block_size = 8 * 1024 * 1024;
    /// Number of bits checked by rolling hash. Increasing this number will increase average block size and vice versa.
    unsigned match_bits = 21;
    /// Buffer size used when reading file from disk.
    size_t read_buffer_size = 10 * 1024 * 1024;
};

/// Partition file into blocks.
/// \param file input.
/// \param max_threads number of threads to use. Passing 0 will use as many threads as there are CPU cores.
/// \param bytes_done optional output parameter for monitoring operation progress.
/// \param bytes_to_process optional output parameter returning number of bytes that will be processed. Operation is finished when bytes_done == bytes_to_process.
/// \param cancel set to true when async operation should be terminated prematurely.
/// \param parameters for chunking algorithm. Do not use unless you know what you are doing.
/// \return a list of boundaries.
std::future<BoundaryList> partition_file(FILE* file, size_t max_threads = 0, std::atomic<int64_t>* bytes_done = nullptr,
    int64_t* bytes_to_process = nullptr, std::atomic<bool>* cancel = nullptr, const Parameters* parameters = nullptr);

/// Compare file blocks and produce delta operations list.
/// \param local_file a BoundaryList produced from local (old) file.
/// \param remote_file a BoundaryList produced from remote (new) file.
/// \return a list of delta sync operations.
SyncOperationList compare_files(const BoundaryList& local_file, const BoundaryList& remote_file);

namespace detail
{
/// Compute a rolling hash on a block of memory.
uint32_t buzhash(const uint8_t* data, uint32_t len);
/// Roll one byte out and one byte in.
uint32_t buzhash_update(uint32_t sum, uint8_t remove, uint8_t add, uint32_t len);
/// Compute strong hash.
uint64_t fnv64a(const uint8_t* data, size_t length, uint64_t hash = 14695981039346656037UL);
}

}
