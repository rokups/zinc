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
#include "zinc/zinc.h"
#include <cstring>
#include <algorithm>
#include <assert.h>
#include <climits>
#include <unordered_map>
#include "zinc_error.hpp"
#include "Utilities.hpp"
#include "mmap/FileMemoryMap.h"
#include "RollingChecksum.hpp"
#include "crypto/fnv1a.h"


#if __cplusplus >= 201402L
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wshift-count-overflow"
#   include <flat_hash_map.hpp>
#   pragma GCC diagnostic pop
#   define ZINC_USE_SKA_FLAT_HASH_MAP 1

struct StrongHashHashFunction
{
    size_t operator()(const zinc::StrongHash& strong)
    {
        return (size_t)zinc::fnv1a64(strong.data(), strong.size());
    }

    typedef ska::power_of_two_hash_policy hash_policy;
};
#else
namespace std
{
    template <>
    struct hash<zinc::StrongHash>
    {
        std::size_t operator()(const zinc::StrongHash& strong) const
        {
            return (size_t)zinc::fnv1a64(strong.data(), strong.size());
        }
    };
}
#endif


namespace zinc
{

struct ByteArrayRef
{
    size_t refcount = 1;
    ByteArray data;
};

const size_t RR_NO_MATCH = (const size_t)-1;

DeltaElement::DeltaElement(size_t block_index, size_t block_offset)
    : block_index(block_index)
    , local_offset(-1)
    , block_offset(block_offset)
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

RemoteFileHashList get_block_checksums(const void* file_data, int64_t file_size, size_t block_size,
                                       const ProgressCallback& report_progress)
{
    RemoteFileHashList hashes;
    uint8_t* fp = (uint8_t*)file_data;

    if (!file_data || !file_size || !block_size)
    {
        zinc_error<std::invalid_argument>("file_data, file_size and block_size must be positive numbers.");
        return hashes;
    }

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
        if (report_progress)
        {
            if (!report_progress(block_size, (block_index + 1) * block_size, file_size))
            {
                zinc_log("User interrupted get_block_checksums().");
                return RemoteFileHashList();
            }
        }
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
        if (report_progress)
            report_progress(last_block_size, file_size, file_size);
    }

    return hashes;
}

RemoteFileHashList get_block_checksums(const char* file_path, size_t block_size,
                                       const ProgressCallback& report_progress)
{
    FileMemoryMap mapping;
    if (mapping.open(file_path))
        return get_block_checksums(mapping.get_data(), mapping.get_size(), block_size, report_progress);
    return RemoteFileHashList();
}

DeltaMap get_differences_delta(const void* file_data, int64_t file_size, size_t block_size,
                               const RemoteFileHashList& hashes, const ProgressCallback& report_progress)
{
    if (file_size % block_size)
    {
        zinc_error<std::invalid_argument>("file_size must be multiple of block_size.");
        return DeltaMap();
    }

    DeltaMap delta;
    delta.map.reserve(hashes.size());
    for (size_t block_index = 0; block_index < hashes.size(); block_index++)
        delta.map.push_back(DeltaElement(block_index, block_index * block_size));

    if (!file_data)
    {
        zinc_log("File is not present, delta equals to full download.");
        return delta;
    }

    // Sort hashes for easier lookup
#if ZINC_USE_SKA_FLAT_HASH_MAP
    ska::flat_hash_map<WeakHash, ska::flat_hash_map<StrongHash, int64_t, StrongHashHashFunction>> lookup_table;
    lookup_table.reserve(hashes.size());
    ska::flat_hash_map<StrongHash, std::set<int64_t>, StrongHashHashFunction> identical_blocks;
    identical_blocks.reserve(file_size / block_size + 1);
    ska::flat_hash_map<int64_t, StrongHash> local_hash_cache;
    local_hash_cache.reserve(file_size / block_size + 1);
#else
    std::unordered_map<WeakHash, std::vector<std::pair<int64_t, const BlockHashes*>>> lookup_table;
    std::unordered_map<StrongHash, std::set<int64_t>> identical_blocks;
    std::unordered_map<int64_t, StrongHash> local_hash_cache;
#endif


    RollingChecksum weak;
    const uint8_t* fp = (const uint8_t*)file_data;
    int64_t last_progress_report = 0;
    int64_t bytes_consumed = 0;
    {
        int64_t block_index = 0;
        for (auto it = hashes.begin(); it != hashes.end(); it++)
        {
            auto& h = *it;
            lookup_table[h.weak][h.strong] = block_index;
            identical_blocks[h.strong].insert(block_index);
            block_index++;
        }
    }

    // Remove lists of identical hashes if list has one entry. In this case block is not identical with any other block.
    for (auto it = identical_blocks.begin(); it != identical_blocks.end(); it++)
    {
        if (it->second.size() > 1)
            delta.identical_blocks.push_back(std::move(it->second));
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
    const auto last_local_hash_check_offset = file_size - block_size;
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

        WeakHash weak_digest = weak.digest();
        auto it = lookup_table.find(weak_digest);
        if (it != lookup_table.end())
        {
            auto strong_hash = StrongHash(w_start, current_block_size);
            auto jt = it->second.find(strong_hash);
            if (jt != it->second.end())
            {
                int64_t this_block_index = jt->second;
                auto local_offset = w_start - fp;
                auto block_offset = this_block_index * block_size;

                // In some cases current block may contain identical data with some later blocks. However these
                // later blocks may already have correct data present. This check avoids moving data between blocks
                // if they are identical already.
                if (local_offset != block_offset && block_offset < last_local_hash_check_offset)
                {
                    auto lh_it = local_hash_cache.find(block_offset);
                    bool is_identical = false;
                    if (lh_it == local_hash_cache.end())
                    {
                        StrongHash local_hash(&fp[block_offset], block_size);
                        local_hash_cache[block_offset] = local_hash;
                        is_identical = local_hash == strong_hash;
                    }
                    else
                        is_identical = lh_it->second == strong_hash;

                    if (is_identical)
                    {
                        // Block at `this_block_index` contains identical data as currently inspected block. No need
                        // to update that block.
                        weak.clear();
                        continue;
                    }
                }

                delta.map[this_block_index].local_offset = w_start - fp;
                weak.clear();
            }
        }
    }

    // Ensure that 100% of progress is reported.
    if (report_progress)
    {
        auto remaining_bytes = bytes_consumed - last_progress_report;
        if (remaining_bytes > 0)
            report_progress(remaining_bytes, file_size, file_size);
    }
    return delta;
}

DeltaMap get_differences_delta(const char* file_path, size_t block_size, const RemoteFileHashList& hashes,
                               const ProgressCallback& report_progress)
{
    FileMemoryMap mapping;
    auto file_size = get_file_size(file_path);
    if (file_size > 0)
    {
        auto err = truncate(file_path, round_up_to_multiple(file_size, block_size));
        if (err != 0)
        {
            zinc_error<std::system_error>("Could not truncate file_path to required size.", err);
            return DeltaMap();
        }
        // We open file mapping only if file exists. If file is missing and mapping is not opened then
        // get_differences_delta() call will get null pointer for file data and will return delta for the full download.
        mapping.open(file_path);
    }
    return get_differences_delta(mapping.get_data(), mapping.get_size(), block_size, hashes, report_progress);
}

bool patch_file(void* file_data, int64_t file_size, size_t block_size, DeltaMap& delta,
                const FetchBlockCallback& get_data, const ProgressCallback& report_progress)
{
    if (file_size % block_size)
    {
        zinc_error<std::invalid_argument>("File data must be multiple of a block size.");
        return false;
    }

    if (file_size < 1)
    {
        zinc_error<std::invalid_argument>("file_size must be a positive number.");
        return false;
    }

    // Reference cache maps block index to a list of other blocks that are very likely to use data at specified index.
    std::vector<std::vector<DeltaElement>> ref_cache((unsigned int)(file_size / block_size));
    {
        size_t block_index = 0;
        for (auto it = delta.map.begin(); it != delta.map.end(); it++)
        {
            auto& delta_element = *it;
            // Only keep record of elements that require data from other parts of the file. Anything else is irrelevant.
            if (delta_element.is_copy())
                ref_cache[delta_element.local_offset / block_size].push_back(delta_element);
            block_index++;
        }
    }
    uint8_t* fp = (uint8_t*)file_data;
    // Block cache is a temporary storage for blocks that will be required elsewhere in the file but are about to be
    // overwritten.
#if ZINC_USE_SKA_FLAT_HASH_MAP
    ska::flat_hash_map<int64_t, ByteArrayRef> block_cache;
    block_cache.reserve(std::max((size_t)10, delta.map.size() / 10));
#else
    std::unordered_map<int64_t, ByteArrayRef> block_cache;
#endif

    std::vector<int64_t> priority_index;
    priority_index.reserve(64);

    auto cache_block = [&](DeltaElement& cacheable)
    {
        auto block_cache_it = block_cache.find(cacheable.local_offset);
        if (block_cache_it == block_cache.end())
        {
            ByteArrayRef block{};
            block.data.resize(block_size, 0);
            memcpy(&block.data.front(), &fp[cacheable.local_offset], block_size);
            block_cache[cacheable.local_offset] = std::move(block);
            zinc_log("% 4d offset +cache refcount=1", cacheable.local_offset);
        }
        else
        {
            ByteArrayRef &cached_block = (*block_cache_it).second;
            cached_block.refcount++;
            zinc_log("% 4d offset +cache refcount=%d", cacheable.local_offset, cached_block.refcount);
        }
        // Prioritize cached blocks so we can evict data from cache as soon as possible.
        priority_index.push_back(cacheable.block_index);
    };

    // TODO: This is inefficient.
    auto find_identical_siblings = [&](int64_t block_index) -> std::set<int64_t>
    {
        for (auto index_set: delta.identical_blocks)
        {
            auto it = index_set.find(block_index);
            if (it != index_set.end())
                return index_set;
        }
        return std::set<int64_t>();
    };

    std::unordered_map<int64_t, DeltaElement> completed;
    while (!delta.is_empty())
    {
        DeltaElement* de = 0;
        bool priority_block = priority_index.size() > 0;
        if (priority_block)
        {
            // Handle blocks which have data cached first so cache can be cleared up.
            auto index = priority_index.back();
            if (index >= delta.map.size())
            {
                // This block was already handled normally.
                priority_index.pop_back();
                continue;
            }
            de = &delta.map[index];
            priority_index.pop_back();
        }
        else
        {
            // Handle blocks normally, walking from the back of the file.
            de = &delta.map.back();
            if (!de->is_valid())
            {
                // This block was already handled by prioritizing cached blocks.
                delta.map.pop_back();
                continue;
            }
        }

        // Correct data is already in place.
        if (!de->is_done())
        {
            // Cache data if writing current block overwrites data needed by any other blocks.
            {
                DeltaElement maybe_cache;
                size_t cached_blocks = 0;

                // Check three clusters of cacheable blocks. Blocks in these clusters have a chance to overlap with
                // current position we are about to write. If any block overlaps with current block - data required for
                // that block will be cached.
                for (auto check_index = std::max<int64_t>(de->block_index - 1, 0),
                          end_index = std::min<int64_t>(de->block_index + 2, ref_cache.size());
                     check_index < end_index; check_index++)
                {
                    auto& subcache = ref_cache[check_index];
                    for (auto it = subcache.begin(); it != subcache.end();)
                    {
                        auto& cacheable = *it;
                        // Make sure block requires data from position that overlaps with current block.
                        if (llabs(cacheable.local_offset - de->block_offset) < block_size)
                        {
                            if (cached_blocks > 0)
                                cache_block(cacheable);
                            else
                            {
                                // Defer caching of very first element. Read below for explanation.
                                maybe_cache = cacheable;
                            }
                            cached_blocks++;
                            it = subcache.erase(it);
                        }
                        else
                            it++;
                    }
                }

                if (cached_blocks == 1 && maybe_cache.block_index == de->block_index)
                {
                    // If we have only one block to cache and it's block index matches current one - we can be sure that
                    // moving data will not corrupt anything therefore we avoid block caching. On this loop iteration
                    // data will be moved into proper place. If this check was not present then block would be cached
                    // and then immediately removed from cache on the same iteration. It would be a pointless extra
                    // copying of memory.
                }
                else if (cached_blocks > 0)
                {
                    assert(maybe_cache.is_valid());
                    cache_block(maybe_cache);
                }
            }

            if (de->is_download())
            {
                auto identical_siblings = find_identical_siblings(de->block_index);

                bool data_copied = false;
                if (identical_siblings.size() > 0)
                {
                    for (auto identical_index: identical_siblings)
                    {
                        auto completed_it = completed.find(identical_index);
                        if (completed_it != completed.end())
                        {
                            // Should `completed` be cleaned up here?
                            zinc_log("% 4d offset use from % 4d", de->block_offset, completed_it->second.block_offset);
                            memmove(&fp[de->block_offset], &fp[completed_it->second.block_offset], block_size);
                            data_copied = true;
                            break;
                        }
                    }
                }

                // There were no identical siblings or none of them were downloaded yet.
                if (!data_copied)
                {
                    zinc_log("% 4d offset download", de->block_offset);
                    auto data = get_data(de->block_index, block_size);
                    memcpy(&fp[de->block_offset], &data.front(), data.size());
                }
            }
            else
            {
                auto it_cache = block_cache.find(de->local_offset);
                if (it_cache != block_cache.end())
                {   // Use cached block if it exists
                    ByteArrayRef& cached_block = it_cache->second;
                    cached_block.refcount--;
                    memmove(&fp[de->block_offset], &cached_block.data.front(), cached_block.data.size());
                    zinc_log("% 4d offset using cached offset %d", de->block_offset, de->local_offset);

                    // Remove block from cache only if delta map does not reference block later
                    if (cached_block.refcount == 0)
                    {
                        block_cache.erase(it_cache);
                        zinc_log("% 4d offset -cache", de->local_offset);
                    }
                }
                else
                {   // Copy block from later file position
                    memmove(&fp[de->block_offset], &fp[de->local_offset], block_size);
                    zinc_log("% 4d offset using file data offset %d", de->block_offset, de->local_offset);
                    // Delete handled block from reference cache lookup table if it exists there. This prevents handled
                    // blocks form getting cached as that cached data would not be needed any more.
                    auto& subcache = ref_cache[de->local_offset / block_size];
                    auto it = std::find(subcache.begin(), subcache.end(), *de);
                    if (it != subcache.end())
                        subcache.erase(it);
                }
            }
        }

        if (priority_block)
        {
            completed[de->block_index] = *de;
            de->block_index = -1;                // Mark block as invalid so it is not handled twice in this loop.
        }
        else
        {
            completed[de->block_index] = std::move(*de);
            delta.map.pop_back();
        }

        if (report_progress)
        {
            if (!report_progress(block_size, file_size - delta.map.size() * block_size, file_size))
            {
                zinc_log("User interrupted patch_file().");
                return false;
            }
        }
    }

    return true;
}

bool patch_file(const char* file_path, int64_t file_final_size, size_t block_size, DeltaMap& delta,
                const FetchBlockCallback& get_data, const ProgressCallback& report_progress)
{
    if (file_final_size <= 0)
    {
        zinc_error<std::invalid_argument>("file_final_size must be positive number.");
        return false;
    }

    auto file_size = get_file_size(file_path);
    if (file_size < 0)
    {
        touch(file_path);
        file_size = 0;
    }

    // Local file must be at least as big as remote file and it must be padded to size multiple of block_size.
    auto max_required_size = std::max<int64_t>(block_size * delta.map.size(), round_up_to_multiple(file_size, block_size));
    auto err = truncate(file_path, max_required_size);
    if (err != 0)
    {
        zinc_error<std::system_error>("Could not truncate file_path to required size.", err);
        return false;
    }

    FileMemoryMap mapping;
    if (!mapping.open(file_path))
        return false;

    if (patch_file(mapping.get_data(), mapping.get_size(), block_size, delta, get_data, report_progress))
    {
        mapping.close();
        err = truncate(file_path, file_final_size);
        if (err != 0)
        {
            zinc_error<std::system_error>("Could not truncate file_path to file_final_size.", err);
            return false;
        }
        return true;
    }

    zinc_error<std::runtime_error>("Unknown error.");
    return false;
}

};
