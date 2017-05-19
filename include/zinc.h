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

#include <map>
#include <vector>
#include <functional>
#include <stdint.h>
#if !_WIN32
#	include <unistd.h>
#endif

namespace zinc
{

typedef uint32_t WeakHash;

class StrongHash
{
public:
    StrongHash();
    StrongHash(const void* m, size_t mlen);
    StrongHash(const std::string& str);
    StrongHash(const StrongHash& other);
    StrongHash& operator=(const StrongHash& other);
    bool operator==(const StrongHash& other);
    std::string to_string() const;

protected:
#ifdef ZINC_FNV
    uint8_t _data[8];
#else
    uint8_t _data[20];
#endif
};

struct BlockHashes
{
    BlockHashes();
	BlockHashes(const WeakHash& weak_, const StrongHash& strong_);
	BlockHashes(const WeakHash& weak_, const std::string& strong_);
    BlockHashes(const BlockHashes& other) = default;
    BlockHashes& operator=(const BlockHashes& other) = default;

    WeakHash weak;
    StrongHash strong;
};

struct DeltaElement
{
    DeltaElement(size_t index, size_t offset);

    size_t block_index;
    size_t local_offset;
};

typedef std::vector<uint8_t>                                                                     ByteArray;
/// A list of offsets of currenty present data in not yet updated file. -1 value signifies a missing block.
typedef std::vector<DeltaElement>                                                                DeltaMap;
/// Strong and weak hashes for each block.
typedef std::vector<BlockHashes>                                                                 RemoteFileHashList;
/// A callback that should obtain block data at specified index and return it.
typedef std::function<ByteArray(size_t block_index, size_t block_size)>                          FetchBlockCallback;
/// A callback for reporting progress. Return true if patching should continue, false if patching should terminate.
typedef std::function<bool(int64_t bytes_done_now, int64_t bytes_done_total, int64_t file_size)> ProgressCallback;


RemoteFileHashList get_block_checksums_mem(void *file_data, int64_t file_size, size_t block_size);
RemoteFileHashList get_block_checksums(const char* file_path, size_t block_size);

DeltaMap get_differences_delta_mem(const void* file_data, int64_t file_size, size_t block_size,
								   const RemoteFileHashList& hashes,
								   const ProgressCallback& report_progress = ProgressCallback());
DeltaMap get_differences_delta(const char* file_path, size_t block_size, const RemoteFileHashList& hashes,
							   const ProgressCallback& report_progress = ProgressCallback());

/// `file_data` must be at least as big as remote data block.
bool patch_file_mem(void* file_data, int64_t file_size, size_t block_size, DeltaMap& delta,
					const FetchBlockCallback& get_data,
					const ProgressCallback& report_progress = ProgressCallback());
/// Patch file and truncate it to `file_final_size`.
bool patch_file(const char* file_path, int64_t file_final_size, size_t block_size, DeltaMap& delta,
				const FetchBlockCallback& get_data, const ProgressCallback& report_progress = ProgressCallback());

};
