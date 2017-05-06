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
#include "zinc.h"
#include <cstring>
#include <algorithm>
#include <assert.h>
#include <stdarg.h>
#include <unordered_map>
#include <hash_map>
#include <chrono>
#include <iostream>

#ifndef ZINC_FNV
#   include "sha1.h"
#endif
#include "RollingChecksum.hpp"
#include "Utilities.hpp"

namespace zinc
{

inline void ZINC_LOG(const char *format, ...)
{
#if ZINC_DEBUG
    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    fprintf(stderr, "\n");
    fflush(stderr);
#endif
}

const size_t RR_NO_MATCH = (const size_t)-1;

#ifdef ZINC_FNV
uint64_t fnv1a64(const uint8_t* data, size_t len, uint64_t hash = 0xcbf29ce484222325)
{
    for (size_t i = 0; i < len; i++)
    {
        hash = hash ^ data[i];
        hash = hash * 0x100000001b3;
    }
    return hash;
}

uint64_t fnv1a64(const void* data, size_t len, uint64_t hash = 0xcbf29ce484222325)
{
    return fnv1a64((uint8_t*)data, len, hash);
}

StrongHash get_strong_hash(const void* data, size_t len)
{
    return fnv1a64(data, len);
}

bool equals(StrongHash a, StrongHash b)
{
    return a == b;
}
#else
StrongHash get_strong_hash(const void* data, size_t len)
{
    StrongHash hash;
    sha1_ctxt sha1;
    sha1_init(&sha1);
    sha1_loop(&sha1, (const uint8_t*)data, len);
    sha1_result(&sha1, &hash);
    return hash;
}

bool equals(const StrongHash& a, const StrongHash& b)
{
    return memcmp(&a, &b, sizeof(a)) == 0;
}
#endif

size_t get_max_blocks(size_t file_size, size_t block_size)
{
    auto number_of_blocks = file_size / block_size;
    if ((file_size % block_size) != 0)
        ++number_of_blocks;
    return number_of_blocks;
}

RemoteFileHashList get_block_checksums_mem(void *file_data, size_t file_size, size_t block_size)
{
    RemoteFileHashList hashes;
    uint8_t* fp = (uint8_t*)file_data;

    auto number_of_blocks = get_max_blocks(file_size, block_size);
    hashes.reserve(number_of_blocks);

    // Last block may be smaller. We will pad it with zeros. In that case we process one less block here and do special
    // treatment of last block after this loop.
    auto last_block_size = file_size % block_size;
    if (last_block_size)
        number_of_blocks -= 1;

    for (size_t block_index = 0; block_index < number_of_blocks; ++block_index)
    {
        BlockHashes h;
        h.weak = RollingChecksum(fp, block_size).digest();
        h.strong = get_strong_hash(fp, block_size);
        hashes.push_back(h);
        fp += block_size;
    }

    if (last_block_size)
    {
        BlockHashes h;
        std::vector<uint8_t> block_data;
        block_data.resize(block_size, 0);
        memcpy(&block_data.front(), fp, last_block_size);
        h.weak = RollingChecksum(&block_data.front(), block_size).digest();
        h.strong = get_strong_hash(&block_data.front(), block_size);
        hashes.push_back(h);
    }

    return hashes;
}

RemoteFileHashList get_block_checksums(const char* file_path, size_t block_size)
{
    FileMemoryMap mapping;
    if (mapping.open(file_path))
        return get_block_checksums_mem(mapping.get_data(), mapping.get_size(), block_size);
    return RemoteFileHashList();
}

DeltaMap get_differences_delta_mem(const void *file_data, size_t file_size, size_t block_size,
                                   const RemoteFileHashList &hashes, ProgressCallback report_progress)
{
    if (file_size % block_size)
    {
        ZINC_LOG("File data must be multiple of a block size.");
        return DeltaMap();
    }

    DeltaMap delta;
    delta.resize(hashes.size(), RR_NO_MATCH);
    // Sort hashes for easier lookup
    __gnu_cxx::hash_map<WeakHash, std::vector<std::pair<int32_t, const BlockHashes*>>> lookup_table;
    lookup_table.resize(hashes.size());
    int32_t block_index = 0;
    RollingChecksum weak;
    const uint8_t* fp = (const uint8_t*)file_data;
    const uint8_t* fp_end = &fp[file_size];
    const uint8_t* w_start = fp - block_size;
    const uint8_t* w_end = w_start + block_size;
    size_t last_progress_report = 0;
    size_t bytes_consumed = 0;

    for (auto& h : hashes)
        lookup_table[h.weak].push_back(std::make_pair(block_index++, &h));

    auto report_progress_internal = [&]()
    {
        bool continue_ = true;
        if (report_progress)
        {
            size_t bytes_since_last_report = bytes_consumed - last_progress_report;
            if (bytes_since_last_report >= block_size)
            {
                continue_ = report_progress(bytes_since_last_report, bytes_consumed, file_size);
                last_progress_report = bytes_consumed;
            }
        }
        return continue_;
    };

    auto consume_full_block = [&]()
    {
        weak.clear();
        w_start += block_size;
        w_end = w_start + block_size;
        weak.update(w_start, block_size);
        bytes_consumed += block_size;
        return report_progress_internal();
    };

    auto rotate_weak_checksum = [&]()
    {
        weak.rotate(*w_start, *w_end);
        w_start++;
        w_end = std::min(++w_end, fp_end);
        bytes_consumed += 1;
        return report_progress_internal();
    };

    consume_full_block();

    while (w_end < fp_end)
    {
        auto it = lookup_table.find(weak.digest());
        if (it != lookup_table.end())
        {
            bool found = false;
            auto& block_list = it->second;
            auto strong_hash = get_strong_hash(w_start, block_size);
            for (auto& pair : block_list)
            {
                const BlockHashes& h = *pair.second;
                if (equals(strong_hash, h.strong))
                {
                    int32_t this_block_index = pair.first;
                    delta[this_block_index] = w_start - fp;
                    found = true;
                    break;
                }
            }
            if (found)
            {
                if (!consume_full_block())
                    return DeltaMap();
            }
            else
            {
                if (!rotate_weak_checksum())
                    return DeltaMap();
            }
        }
        else if (!rotate_weak_checksum())
             return DeltaMap();
    }
    return delta;
}

DeltaMap get_differences_delta(const char* file_path, size_t block_size, const RemoteFileHashList& hashes,
                               ProgressCallback report_progress)
{
    FileMemoryMap mapping;
    if (mapping.open(file_path, block_size))
        return get_differences_delta_mem(mapping.get_data(), mapping.get_size(), block_size, hashes, report_progress);
    return DeltaMap();
}

bool patch_file_mem(void *file_data, size_t file_size, size_t block_size, DeltaMap &delta, FetchBlockCallback get_data,
                    ProgressCallback report_progress)
{
    if (file_size % block_size)
    {
        ZINC_LOG("File data must be multiple of a block size.");
        return false;
    }

    uint8_t* fp = (uint8_t*)file_data;
    size_t block_offset = 0;
    size_t block_index = 0;
    std::map<size_t, ByteArray> block_cache;

//    for (auto it = delta.begin(); it != delta.end(); ++it)
    while (delta.size())
    {
        if (report_progress)
        {
            auto consumed_bytes = block_index * block_size;
            auto consumed_block = block_size;
            if (consumed_bytes > file_size)
            {
                consumed_bytes = file_size;
                consumed_block = file_size % block_size;
            }
            if (!report_progress(consumed_block, consumed_bytes, file_size))
                return false;
        }

        auto& from_local_offset = delta.front();
        // If local file data matches remote file data
        if (from_local_offset != block_offset)
        {
            // Cache blocks that will be needed later
            // TODO: Cache may be filled faster than it's blocks are used. Wise thing to do would be placing cached blocks to their destinations as soon as they are cached.
            for (auto jt = delta.begin(); jt != delta.end(); ++jt)
            {
                auto need_offset = *jt;
                if (need_offset != RR_NO_MATCH)
                {
                    if (block_offset <= need_offset && need_offset < (block_offset + block_size))
                    {
                        if (block_cache.find(need_offset) == block_cache.end())
                        {
                            ByteArray block(block_size);
                            memcpy(&block.front(), &fp[need_offset], block_size);
                            block_cache[need_offset] = std::move(block);
                            ZINC_LOG("%d offset cached", need_offset);
                        }
                    }
                }
            }
            ////////////////////////////////////////////////////////////////////////////////////////////////////////////

            if (from_local_offset == RR_NO_MATCH)
            {
                ZINC_LOG("%02d block download", block_index);
                auto data = get_data(block_index, block_size);
                memcpy(&fp[block_offset], &data.front(), data.size());
            }
            else
            {
                auto it_cache = block_cache.find(from_local_offset);
                if (it_cache != block_cache.end())
                {   // Use cached block if it exists
                    auto& data = it_cache->second;
                    memmove(&fp[block_offset], &data.front(), data.size());
                    ZINC_LOG("%02d index using cached offset %d", block_index, from_local_offset);

                    // Remove block from cache only if delta map does not reference block later
                    if (std::find(delta.begin() + 1, delta.end(), it_cache->first) == delta.end())
                    {
                        block_cache.erase(it_cache);
                        ZINC_LOG("removed cached block offset %d", from_local_offset);
                    }
                }
                else
                {   // Copy block from later file position
                    // Make sure we are not copying from previous file position because it was already overwritten.
                    // Also source block can not overlap with destination block. This case is handled by caching block.
                    assert(from_local_offset >= (block_offset + block_size));
                    memmove(&fp[block_offset], &fp[from_local_offset], block_size);
                    ZINC_LOG("%02d index using file data offset %d", block_index, from_local_offset);
                }
            }
        }
        else
            ZINC_LOG("%02d block matches", block_index);

        block_offset += block_size;
        block_index += 1;
        // TODO: this is probably inefficient.
        delta.erase(delta.begin());
    }
    return true;
}

bool patch_file(const char* file_path, size_t file_final_size, size_t block_size, DeltaMap& delta,
                FetchBlockCallback get_data, ProgressCallback report_progress)
{
    struct stat st = {};
    if (stat(file_path, &st) == -1)
        return false;

    auto max_required_size = block_size * delta.size();
    if (st.st_size < max_required_size)
    {
        if (truncate(file_path, max_required_size) != 0)
            return false;
    }

    FileMemoryMap mapping;
    if (!mapping.open(file_path, block_size))
        return false;

    if (patch_file_mem(mapping.get_data(), mapping.get_size(), block_size, delta, get_data, report_progress))
    {
        mapping.close();
        return truncate(file_path, file_final_size) == 0;
    }
    return false;
}

};
