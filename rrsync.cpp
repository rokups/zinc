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

#ifndef ZINC_FNV
#   include "sha1.h"
#endif
#include "RollingChecksum.hpp"
#include "Utilities.hpp"

namespace zinc
{

struct StrongHashImpl : public StrongHash
{
    StrongHashImpl();
    StrongHashImpl(const void* data, size_t len);
    bool operator==(const StrongHash& other);
};

#if RR_DEBUG

#   include <stdarg.h>

inline void RR_LOG(const char* format, ...)
{
    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    fprintf(stderr, "\n");
    fflush(stderr);
}

#else
#   define RR_LOG(...)
#endif

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

#endif

StrongHashImpl::StrongHashImpl()
{
    memset(&_data, 0, sizeof(_data));
}

StrongHashImpl::StrongHashImpl(const void* data, size_t len)
{
#ifdef ZINC_FNV
    _data = fnv1a64(data, len);
#else
    sha1_ctxt sha1;
    sha1_init(&sha1);
    sha1_loop(&sha1, (const uint8_t*)data, len);
    sha1_result(&sha1, &_data);
#endif
}

bool StrongHashImpl::operator==(const StrongHash& other)
{
#ifdef ZINC_FNV
    return _data == other._data;
#else
    return memcmp(&_data, &other._data, sizeof(_data)) == 0;
#endif
}

size_t get_max_blocks(size_t file_size, size_t block_size)
{
    auto number_of_blocks = file_size / block_size;
    if ((file_size % block_size) != 0)
        ++number_of_blocks;
    return number_of_blocks;
}

RemoteFileHashList get_block_checksums(void* file_data, size_t file_size, size_t block_size)
{
    RemoteFileHashList hashes;
    uint8_t* fp = (uint8_t*)file_data;

    auto number_of_blocks = get_max_blocks(file_size, block_size);
    hashes.reserve(number_of_blocks);

    for (uint32_t block_index = 0; block_index < number_of_blocks; ++block_index)
    {
        auto current_block_size = std::min(block_size, file_size - (block_index * block_size));
        BlockHashes h;
        h.index = block_index;
        h.weak = RollingChecksum(fp, current_block_size).digest();
        h.strong = StrongHashImpl(fp, current_block_size);
        hashes.push_back(h);
        fp += current_block_size;
    }

    return hashes;
}

RemoteFileHashList get_block_checksums(const char* file_path, size_t block_size)
{
    FileMapping mapping;
    if (mapping.open(file_path))
        return get_block_checksums(mapping.get_data(), mapping.get_size(), block_size);
    return RemoteFileHashList();
}

DeltaMap get_differences_delta(const void* file_data, size_t file_size, size_t block_size,
                               const RemoteFileHashList& hashes)
{
    DeltaMap delta;
    delta.resize(hashes.size(), RR_NO_MATCH);
    const uint8_t* fp = (const uint8_t*)file_data;

    uint32_t weak32;
    uint16_t weak16;
    uint8_t weak8;

    // Sort hashes for easier lookup
    std::map<uint8_t, std::map<uint16_t, std::vector<BlockHashes>>> lookup_table;
    for (auto&& h : hashes)
    {
        weak32 = h.weak;
        weak16 = (uint16_t)((weak32 & 0xFFFF) ^ (weak32 >> 16));
        weak8 = (uint8_t)((weak16 & 0xFF) ^ (weak16 >> 8));
        lookup_table[weak8][weak16].push_back(h);
    }

    RollingChecksum weak;
    StrongHashImpl strong = {};

    const uint8_t* fp_end = &fp[file_size];
    const uint8_t* w_start = fp - block_size;
    const uint8_t* w_end = w_start + block_size;
    while (w_end < fp_end)
    {
        // Consume data for rolling checksum
        if (weak.count() == 0)
        {
            w_start += block_size;
            w_end = std::min(w_start + block_size, fp_end);
            weak.update(w_start, block_size);
        } else
        {
            weak.rotate(*w_start, *w_end);
            w_start++;
            w_end = std::min(++w_end, fp_end);
        }

        // Calculate helper-hashes
        weak32 = weak.digest();
        weak16 = (uint16_t)((weak32 & 0xFFFF) ^ (weak32 >> 16));
        weak8 = (uint8_t)((weak16 & 0xFF) ^ (weak16 >> 8));

        auto it8 = lookup_table.find(weak8);
        if (it8 != lookup_table.end())
        {
            auto& lookup_table16 = it8->second;
            auto it16 = lookup_table16.find(weak16);
            if (it16 != lookup_table16.end())
            {
                auto& lookup_table32 = it16->second;
                bool have_strong = false;
                for (auto& h : lookup_table32)
                {
                    if (h.weak == weak32)
                    {
                        if (!have_strong)
                        {
                            have_strong = true;
                            strong = StrongHashImpl(w_start, block_size);
                        }
                        if (strong == h.strong)
                        {
                            delta[h.index] = w_start - fp;
                            weak.clear();
                            break;
                        }
                    }
                }
            }
        }
    }

    return delta;
}

DeltaMap get_differences_delta(const char* file_path, size_t block_size, const RemoteFileHashList& hashes)
{
    FileMapping mapping;
    if (mapping.open(file_path))
        return get_differences_delta(mapping.get_data(), mapping.get_size(), block_size, hashes);
    return DeltaMap();
}

bool patch_file(void* file_data, size_t file_size, size_t block_size, DeltaMap& delta, FetchBlockCallback get_data)
{
    uint8_t* fp = (uint8_t*)file_data;
    size_t block_offset = 0;
    size_t block_index = 0;
    std::map<size_t, ByteArray> block_cache;

//    for (auto it = delta.begin(); it != delta.end(); ++it)
    while (delta.size())
    {
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
                            auto cached_block_size = std::min(block_size, file_size - need_offset);
                            ByteArray block(block_size);
                            memcpy(&block.front(), &fp[need_offset], cached_block_size);
                            block_cache[need_offset] = block;
                            RR_LOG("%d offset cached", need_offset);
                        }
                    }
                }
            }
            ////////////////////////////////////////////////////////////////////////////////////////////////////////////

            if (from_local_offset == RR_NO_MATCH)
            {
                RR_LOG("%02d block download", block_index);
                auto data = get_data(block_index, block_size);
                memcpy(&fp[block_offset], &data.front(), data.size());
            } else
            {
                auto current_block_size = std::min(block_size, file_size - block_offset);
                auto it_cache = block_cache.find(from_local_offset);
                if (it_cache != block_cache.end())
                {   // Use cached block if it exists
                    auto& data = it_cache->second;
                    assert(current_block_size == data.size());
                    memmove(&fp[block_offset], &data.front(), current_block_size);
                    RR_LOG("%02d index using cached offset %d", block_index, from_local_offset);

                    // Remove block from cache only if delta map does not reference block later
                    if (std::find(delta.begin() + 1, delta.end(), it_cache->first) == delta.end())
                    {
                        block_cache.erase(it_cache);
                        RR_LOG("removed cached block offset %d", from_local_offset);
                    }
                } else
                {   // Copy block from later file position
                    // Make sure we are not copying from previous file position because it was already overwritten.
                    // Also source block can not overlap with destination block. This case is handled by caching block.
                    assert(from_local_offset >= (block_offset + block_size));
                    memmove(&fp[block_offset], &fp[from_local_offset], current_block_size);
                    RR_LOG("%02d index using file data offset %d", block_index, from_local_offset);
                }
            }
        } else
            RR_LOG("%02d block matches", block_index);

        block_offset += block_size;
        block_index += 1;
        delta.erase(delta.begin());
    }
    return true;
}

bool patch_file(const char* file_path, size_t block_size, DeltaMap& delta, FetchBlockCallback get_data)
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

    FileMapping mapping;
    if (!mapping.open(file_path))
        return false;

    return patch_file(mapping.get_data(), mapping.get_size(), block_size, delta, get_data);
}

};
