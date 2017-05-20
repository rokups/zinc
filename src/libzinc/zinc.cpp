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
#include <stdarg.h>
#include <assert.h>
#include <climits>
#if __cplusplus >= 201402L
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wshift-count-overflow"
#   include <flat_hash_map.hpp>
#   pragma GCC diagnostic pop
#   define USE_SKA_FLAT_HASH_MAP 1
#else
#   include <unordered_map>
#endif
#if ZINC_WITH_STRONG_HASH_SHA1
#   include "sha1.h"
#endif
#include "RollingChecksum.hpp"
#include "Utilities.hpp"


namespace zinc
{

struct ByteArrayRef
{
    size_t refcount;
    ByteArray data;
};

inline void ZINC_LOG(const char *format, ...)
{
#if ZINC_WITH_DEBUG
    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    fprintf(stderr, "\n");
    fflush(stderr);
#endif
}

const size_t RR_NO_MATCH = (const size_t)-1;






StrongHash::StrongHash()
{
    memset(_data, 0, sizeof(_data));
}

StrongHash::StrongHash(const void* m, size_t mlen)
{
#if ZINC_WITH_STRONG_HASH_FNV
    auto p = (uint8_t*)m;
    uint64_t hash = 0xcbf29ce484222325;
    static_assert(sizeof(hash) == sizeof(_data));

    for (size_t i = 0; i < mlen; i++)
    {
        hash = hash ^ p[i];
//        hash = hash * 0x100000001b3;
        hash += (hash << 1) + (hash << 4) + (hash << 5) +
                (hash << 7) + (hash << 8) + (hash << 40);
    }
    memcpy(_data, &hash, sizeof(_data));
#else
    StrongHash hash;
    sha1_ctxt sha1;
    sha1_init(&sha1);
    sha1_loop(&sha1, (const uint8_t*)m, mlen);
    sha1_result(&sha1, _data);
#endif
}

StrongHash::StrongHash(const StrongHash& other)
{
    memcpy(_data, other._data, sizeof(_data));
}

StrongHash& StrongHash::operator=(const StrongHash& other)
{
    memcpy(_data, other._data, sizeof(_data));
    return *this;
}

bool StrongHash::operator==(const StrongHash& other)
{
    return memcmp(_data, other._data, sizeof(_data)) == 0;
}

std::string StrongHash::to_string() const
{
    std::string result;
    result.resize(sizeof(_data) * 2);
    for (size_t i = 0; i < sizeof(_data); i++)
        sprintf(&result.front() + i * 2, "%02x", _data[i]);
    return result;
}

StrongHash::StrongHash(const std::string& str)
{
    if (str.length() < (sizeof(_data) * 2))
        return;

    char buf[3];
    for (size_t i = 0; i < sizeof(_data); i++)
    {
        memcpy(buf, &str.front() + i * 2, 2);
        buf[2] = 0;
        _data[i] = (uint8_t)strtol(buf, 0, 16);
    }
}

DeltaElement::DeltaElement(size_t index, size_t offset)
    : block_index(index)
    , local_offset(offset)
{
}

BlockHashes::BlockHashes()
{
    weak = 0;
}

BlockHashes::BlockHashes(const WeakHash& weak, const StrongHash& strong)
{
    this->weak = weak;
    this->strong = strong;
}

BlockHashes::BlockHashes(const WeakHash& weak, const std::string& strong)
{
    this->weak = weak;
    this->strong = StrongHash(strong);
}


size_t get_max_blocks(int64_t file_size, size_t block_size)
{
    auto number_of_blocks = file_size / block_size;
    if ((file_size % block_size) != 0)
        ++number_of_blocks;
    assert(number_of_blocks < UINT_MAX);
    return (size_t)number_of_blocks;
}

RemoteFileHashList get_block_checksums(const void* file_data, int64_t file_size, size_t block_size)
{
    RemoteFileHashList hashes;
    uint8_t* fp = (uint8_t*)file_data;

    if (!file_data || !file_size || !block_size)
        return hashes;

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
        h.strong = StrongHash(fp, block_size);
        hashes.push_back(h);
        fp += block_size;
    }

    if (last_block_size)
    {
        BlockHashes h;
        std::vector<uint8_t> block_data;
        block_data.resize(block_size, 0);
        memcpy(&block_data.front(), fp, (size_t)last_block_size);
        h.weak = RollingChecksum(&block_data.front(), block_size).digest();
        h.strong = StrongHash(&block_data.front(), block_size);
        hashes.push_back(h);
    }

    return hashes;
}

RemoteFileHashList get_block_checksums(const char* file_path, size_t block_size)
{
    FileMemoryMap mapping;
    if (mapping.open(file_path))
        return get_block_checksums(mapping.get_data(), mapping.get_size(), block_size);
    return RemoteFileHashList();
}

DeltaMap get_differences_delta(const void* file_data, int64_t file_size, size_t block_size,
                               const RemoteFileHashList& hashes, const ProgressCallback& report_progress)
{
    if (file_size % block_size)
    {
        ZINC_LOG("File data must be multiple of a block size.");
        return DeltaMap();
    }

    DeltaMap delta;
    delta.reserve(hashes.size());
    for (size_t i = 0; i < hashes.size(); i++)
        delta.push_back(DeltaElement(i, RR_NO_MATCH));

    // When file is not present delta equals to full download.
    if (!file_data)
        return delta;

    // Sort hashes for easier lookup
#if USE_SKA_FLAT_HASH_MAP
    ska::flat_hash_map<WeakHash, std::vector<std::pair<int32_t, const BlockHashes*>>> lookup_table;
    lookup_table.reserve(hashes.size());
#else
    std::unordered_map<WeakHash, std::vector<std::pair<int32_t, const BlockHashes*>>> lookup_table;
#endif
    RollingChecksum weak;
    const uint8_t* fp = (const uint8_t*)file_data;
    int64_t last_progress_report = 0;
    int64_t bytes_consumed = 0;
    {
        int32_t block_index = 0;
        for (auto it = hashes.begin(); it != hashes.end(); it++)
        {
            auto& h = *it;
            lookup_table[h.weak].push_back(std::make_pair(block_index++, &h));
        }
    }

    auto report_progress_internal = [&]() -> bool
    {
        bool continue_ = true;
        if (report_progress)
        {
            int64_t bytes_since_last_report = bytes_consumed - last_progress_report;
            if (bytes_since_last_report >= (int64_t)block_size)
            {
                continue_ = report_progress(bytes_since_last_report, bytes_consumed, file_size);
                last_progress_report = bytes_consumed;
            }
        }
        return continue_;
    };

    const uint8_t* w_start = fp - block_size;
    for (;bytes_consumed < file_size;)
    {
        if (!report_progress_internal())
            return DeltaMap();

        size_t current_block_size = (size_t)std::min<int64_t>(block_size, file_size - bytes_consumed);
        if (weak.isEmpty())
        {
            w_start += current_block_size;
            bytes_consumed += current_block_size;
            weak.update(w_start, current_block_size);
        }
        else
        {
            bytes_consumed += 1;
            weak.rotate(*w_start, w_start[block_size]);
            ++w_start;
        }

        auto weak_digest = weak.digest();
        auto it = lookup_table.find(weak_digest);
        if (it != lookup_table.end())
        {
            auto& block_list = it->second;
            auto strong_hash = StrongHash(w_start, current_block_size);
            for (auto jt = block_list.begin(); jt != block_list.end(); jt++)
            {
                auto& pair = *jt;
                const BlockHashes& h = *pair.second;
                if (strong_hash == h.strong)
                {
                    int32_t this_block_index = pair.first;
                    delta[this_block_index].local_offset = w_start - fp;
                    weak.clear();
                }
            }
        }
    }
    // Ensure that 100% of progress is reported.
    auto remaining_bytes = bytes_consumed - last_progress_report;
    if (remaining_bytes > 0)
        report_progress(remaining_bytes, file_size, file_size);
    return delta;
}

DeltaMap get_differences_delta(const char* file_path, size_t block_size, const RemoteFileHashList& hashes,
                               const ProgressCallback& report_progress)
{
    FileMemoryMap mapping;
    auto file_size = get_file_size(file_path);
    if (file_size > 0)
    {
        if (truncate(file_path, round_up_to_multiple(file_size, block_size)) != 0)
            return DeltaMap();
        mapping.open(file_path);
    }
    return get_differences_delta(mapping.get_data(), mapping.get_size(), block_size, hashes, report_progress);
}

bool patch_file(void* file_data, int64_t file_size, size_t block_size, DeltaMap& delta,
                const FetchBlockCallback& get_data, const ProgressCallback& report_progress)
{
    if (file_size % block_size || file_size == 0)
    {
        ZINC_LOG("File data must be multiple of a block size.");
        return false;
    }

    // Offset cache is used for quickly determining if local data is needed anywhere else in local file.
    std::vector<std::vector<DeltaElement>> offset_cache((unsigned int)(file_size / block_size));
    size_t block_index = 0;
    for (auto it = delta.begin(); it != delta.end(); it++)
    {
        auto& d = *it;
        auto from_local_offset = d.local_offset;
        if (from_local_offset != RR_NO_MATCH && (block_index * block_size) != from_local_offset)
            offset_cache[from_local_offset / block_size].push_back(d);
        block_index++;
    }

    uint8_t* fp = (uint8_t*)file_data;
#if USE_SKA_FLAT_HASH_MAP
    ska::flat_hash_map<size_t, ByteArrayRef> block_cache;
    block_cache.reserve(std::max((size_t)10, delta.size() / 10));
#else
    std::unordered_map<size_t, ByteArrayRef> block_cache;
#endif
    while (delta.size())
    {
        block_index = delta.back().block_index;
        auto block_offset = block_index * block_size;
        auto from_local_offset = delta.back().local_offset;

        // Correct data is already in place.
        if (from_local_offset != block_offset)
        {
            // Calculate amount of blocks to be cached in advance. Is there a better way?
            size_t blocks_to_cache = 0;
            for (auto i = block_index - 1; i <= block_index && i >= 0 && i < offset_cache.size(); i++)
                blocks_to_cache = offset_cache[i].size();

            // This loop checks possibly used data of this block and previous block. This is only valid if we walk delta
            // from the end backwards.
            // TODO: Investigate if there is a risk of overcaching, where bigger part of potentially large file ends in the cache instead faster than it is being evicted from it.
            for (auto i = block_index - 1; i <= block_index && i >= 0 && i < offset_cache.size(); i++)
            {
                auto& subcache = offset_cache[i];
                if (!subcache.size())
                    continue;

                // If we have only one block to cache and it's block index matches current one - we can be sure that
                // moving data will not corrupt anything therefore we avoid block caching. On this loop iteration data
                // will be moved into proper place. If this check was not present then block would be cached and then
                // immediately removed from cache on the same iteration. It would be a pointless extra copying of memory.
                if (blocks_to_cache == 1 && subcache[0].block_index == block_index)
                    break;

                for (auto it = subcache.begin(); it != subcache.end();)
                {
                    auto& cacheable = *it;
                    size_t cacheable_local_offset = cacheable.local_offset;
                    auto block_cache_it = block_cache.find(cacheable_local_offset);
                    if (block_cache_it == block_cache.end())
                    {
                        ByteArrayRef block;
                        block.refcount = 1;
                        block.data.resize(block_size, 0);
                        memcpy(&block.data.front(), &fp[cacheable_local_offset], block_size);
                        block_cache[cacheable_local_offset] = std::move(block);
                        ZINC_LOG("% 4d offset +cache refcount=1", cacheable_local_offset);
                    }
                    else
                    {
                        ByteArrayRef &cached_block = (*block_cache_it).second;
                        cached_block.refcount++;
                        ZINC_LOG("% 4d offset +cache refcount=%d", cacheable_local_offset, cached_block.refcount);
                    }
                    it = subcache.erase(it);
                }
            }

            if (from_local_offset == RR_NO_MATCH)
            {
                ZINC_LOG("% 4d offset download", block_offset);
                auto data = get_data(block_index, block_size);
                memcpy(&fp[block_offset], &data.front(), data.size());
            }
            else
            {
                auto it_cache = block_cache.find(from_local_offset);
                if (it_cache != block_cache.end())
                {   // Use cached block if it exists
                    ByteArrayRef& cached_block = it_cache->second;
                    cached_block.refcount--;
                    memmove(&fp[block_offset], &cached_block.data.front(), cached_block.data.size());
                    ZINC_LOG("% 4d offset using cached offset %d", block_offset, from_local_offset);

                    // Remove block from cache only if delta map does not reference block later
                    if (cached_block.refcount == 0)
                    {
                        block_cache.erase(it_cache);
                        ZINC_LOG("% 4d offset -cache", from_local_offset);
                    }
                }
                else
                {   // Copy block from later file position
                    memmove(&fp[block_offset], &fp[from_local_offset], block_size);
                    ZINC_LOG("% 4d offset using file data offset %d", block_offset, from_local_offset);
                    auto& subcache = offset_cache[from_local_offset / block_size];
                    for (auto it = subcache.begin(); it != subcache.end(); it++)
                    {
                        auto& cacheable = *it;
                        if (cacheable.local_offset == from_local_offset)
                        {
                            // Delete used offset, but do it only once.
                            it = subcache.erase(it);
                            break;
                        }
                    }
                }
            }
        }

        delta.pop_back();
        if (report_progress)
        {
            if (!report_progress(block_size, file_size - delta.size() * block_size, file_size))
                return false;
        }
    }

    return true;
}

bool patch_file(const char* file_path, int64_t file_final_size, size_t block_size, DeltaMap& delta,
                const FetchBlockCallback& get_data, const ProgressCallback& report_progress)
{
    if (file_final_size <= 0)
        return false;

    auto file_size = get_file_size(file_path);
    if (file_size < 0)
    {
        fclose(fopen(file_path, "w+"));
        file_size = 0;
    }

    // Local file must be at least as big as remote file and it must be padded to size multiple of block_size.
    auto max_required_size = std::max<int64_t>(block_size * delta.size(), round_up_to_multiple(file_size, block_size));
    if (truncate(file_path, max_required_size) != 0)
        return false;

    FileMemoryMap mapping;
    if (!mapping.open(file_path))
        return false;

    if (patch_file(mapping.get_data(), mapping.get_size(), block_size, delta, get_data, report_progress))
    {
        mapping.close();
        return truncate(file_path, file_final_size) == 0;
    }
    return false;
}

};
