#pragma once
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
    >>> patch_file(local_file_data, local_file_size, block_size, delta, get_data_cb, user_data)

    # 5. Truncate local file size to that of a remote file.
*/

#include <map>
#include <vector>
#include <unistd.h>
#include <functional>

namespace zinc
{

struct StrongHash
{
#ifdef ZINC_FNV
    uint64_t _data;
#else
    uint8_t _data[20];
#endif
};

struct BlockHashes
{
    uint32_t index;
    uint32_t weak;
    StrongHash strong;
};

typedef std::vector<size_t>         DeltaMap;
typedef std::vector<uint8_t>        ByteArray;
typedef std::vector<BlockHashes>    RemoteFileHashList;
typedef std::function<ByteArray(size_t block_index, size_t block_size)> FetchBlockCallback;


RemoteFileHashList get_block_checksums(void* file_data, size_t file_size, size_t block_size);
RemoteFileHashList get_block_checksums(const char* file_path, size_t block_size);

DeltaMap get_differences_delta(const void* file_data, size_t file_size, size_t block_size, const RemoteFileHashList& hashes);
DeltaMap get_differences_delta(const char* file_path, size_t block_size, const RemoteFileHashList& hashes);

// `file_data` must be at least as big as remote data block.
bool patch_file(void* file_data, size_t file_size, size_t block_size, DeltaMap& delta, FetchBlockCallback get_data);
// Caller must truncate `file_path` file to appropriate size.
bool patch_file(const char* file_path, size_t block_size, DeltaMap& delta, FetchBlockCallback get_data);

};
