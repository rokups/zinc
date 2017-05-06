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
/*
    Example:
    # 1. On the remote system containing latest version of file
    >>> remote_file_hashes = get_block_checksums(remote_file_data, data_size, block_size)

    # 2. On the local system system after having received `remote_file_hashes`
    >>> delta = get_differences_delta(local_file_data, local_file_size, block_size, remote_file_hashes)

    # 3. Ensure that local file is at least as big as remote file.

    # 4. To patch old file to new file in-place:
    >>> void get_data_cb(size_t block_index, size_t block_size, void* user_data)
    >>> {
    >>>     ByteArray data;
    >>>     // Download `block_size` number of remote file bytes starting at `block_index * block_size` position.
    >>>     return data;
    >>> }
    >>> patch_file_mem(local_file_data, local_file_size, block_size, delta, get_data_cb, user_data)

    # 5. Truncate local file size to that of a remote file.
*/
#pragma once

#include <map>
#include <vector>
#include <unistd.h>
#include <functional>

namespace zinc
{

#ifdef ZINC_FNV
    typedef uint64_t StrongHash;
#else
    typedef uint8_t StrongHash[20];
#endif
typedef uint32_t WeakHash;

struct BlockHashes
{
    WeakHash weak;
    StrongHash strong;
};

typedef std::vector<uint8_t>                                            ByteArray;
/// A list of offsets of currenty present data in not yet updated file. -1 value signifies a missing block.
typedef std::vector<size_t>                                             DeltaMap;
/// Strong and weak hashes for each block.
typedef std::vector<BlockHashes>                                        RemoteFileHashList;
/// A callback that should obtain block data at specified index and return it.
typedef std::function<ByteArray(size_t block_index, size_t block_size)> FetchBlockCallback;
/// A callback for reporting progress. Return true if patching should continue, false if patching should terminate.
typedef std::function<bool(size_t bytes_done_now,
                           size_t bytes_done_total, size_t file_size)>  ProgressCallback;


RemoteFileHashList get_block_checksums_mem(void *file_data, size_t file_size, size_t block_size);
RemoteFileHashList get_block_checksums(const char* file_path, size_t block_size);

DeltaMap get_differences_delta_mem(const void *file_data, size_t file_size, size_t block_size,
                                   const RemoteFileHashList &hashes, ProgressCallback report_progress=ProgressCallback());
DeltaMap get_differences_delta(const char* file_path, size_t block_size, const RemoteFileHashList& hashes,
                               ProgressCallback report_progress=ProgressCallback());

/// `file_data` must be at least as big as remote data block.
bool patch_file_mem(void *file_data, size_t file_size, size_t block_size, DeltaMap &delta, FetchBlockCallback get_data,
                    ProgressCallback report_progress=ProgressCallback());
/// Patch file and truncate it to `file_final_size`.
bool patch_file(const char* file_path, size_t file_final_size, size_t block_size, DeltaMap& delta,
                FetchBlockCallback get_data, ProgressCallback report_progress=ProgressCallback());

};
