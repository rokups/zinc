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
#include <cassert>
#include <cstring>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include "mmap/FileMemoryMap.h"
#include "zinc_error.hpp"
#include "Utilities.hpp"
#include "DeltaResolver.h"
#include "HashBlocksTask.h"

namespace zinc
{

struct ByteArrayRef
{
    size_t refcount = 1;
    ByteArray data;
};

inline bool is_download(DeltaElement& de)
{
    return de.local_offset == -1;
}

inline bool is_done(DeltaElement& de)
{
    return de.block_offset == de.local_offset;
}

inline bool is_copy(DeltaElement& de)
{
    return de.local_offset >= 0 && !is_done(de);
}

inline bool is_valid(DeltaElement& de)
{
    return de.block_index >= 0 && de.block_offset >= 0;
}

inline bool operator==(const DeltaElement& a, const DeltaElement& b)
{
    return a.block_index == b.block_index && a.block_offset == b.block_offset && a.local_offset == b.local_offset;
}

std::unique_ptr<ITask<RemoteFileHashList>> get_block_checksums(const void* file_data, int64_t file_size,
                                                               size_t block_size, size_t max_threads)
{
    RemoteFileHashList hashes;
    if (file_data == nullptr || file_size == 0 || block_size == 0)
    {
        zinc_error<std::invalid_argument>("file_data, file_size and block_size must be positive numbers.");
        return std::unique_ptr<HashBlocksTask>{};
    }
    return std::make_unique<HashBlocksTask>(new Buffer(const_cast<void*>(file_data), file_size), block_size,
        max_threads);
}

std::unique_ptr<ITask<RemoteFileHashList>> get_block_checksums(const char* file_path, size_t block_size,
                                                               size_t max_threads)
{
    RemoteFileHashList hashes;
    if (file_path == nullptr || block_size == 0)
    {
        zinc_error<std::invalid_argument>("file_path and block_size must not be null.");
        return std::unique_ptr<HashBlocksTask>{};
    }
#if ZINC_NO_MMAP
    auto* file = new File(file_path, File::Read);
    max_threads = 1;
#else
    auto* file = new MemoryMappedFile(file_path);
#endif
    return std::make_unique<HashBlocksTask>(file, block_size, max_threads);
}

std::unique_ptr<ITask<DeltaMap>> get_differences_delta(const void* file_data, int64_t file_size, size_t block_size,
                                                       const RemoteFileHashList& hashes, size_t max_threads)
{
    if ((file_size % block_size) != 0)
    {
        zinc_error<std::invalid_argument>("file_size must be multiple of block_size.");
        return std::unique_ptr<DeltaResolver>{};
    }

    if (file_data == nullptr)
    {
        zinc_log("File is not present, delta equals to full download.");
        return std::unique_ptr<DeltaResolver>{};
    }

    return std::make_unique<DeltaResolver>(new Buffer(const_cast<void*>(file_data), file_size), block_size, hashes, max_threads);
}

std::unique_ptr<ITask<DeltaMap>> get_differences_delta(const char* file_path, size_t block_size,
                                                       const RemoteFileHashList& hashes, size_t max_threads)
{
    auto file_size = get_file_size(file_path);
    int64_t truncate_to;
    if (file_size > 0)
        truncate_to = round_up_to_multiple(file_size, block_size);
    else
        truncate_to = block_size * hashes.size();

    auto err = truncate(file_path, truncate_to);
    if (err != 0)
    {
        zinc_error<std::system_error>("Could not truncate file_path to required size.", err);
        return std::unique_ptr<DeltaResolver>{};
    }
#if ZINC_NO_MMAP
    auto* file = new File(file_path, File::Read);
    max_threads = 1;
#else
    auto* file = new MemoryMappedFile(file_path);
#endif
    return std::make_unique<DeltaResolver>(file, block_size, hashes, max_threads);
}

bool patch_file(IFile* file, size_t block_size, DeltaMap& delta,
                const FetchBlockCallback& get_data, const ProgressCallback& report_progress)
{
    auto file_size = file->get_size();

    if ((file_size % block_size) != 0)
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
        for (auto& delta_element : delta.map)
        {
            // Only keep record of elements that require data from other parts of the file. Anything else is irrelevant.
            if (is_copy(delta_element))
                ref_cache[delta_element.local_offset / block_size].push_back(delta_element);
            block_index++;
        }
    }
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
            memcpy(&block.data.front(), file->read(cacheable.local_offset, block_size), block_size);
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

    while (!delta.is_empty())
    {
        DeltaElement* de = nullptr;
        bool priority_block = !priority_index.empty();
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
            if (!is_valid(*de))
            {
                // This block was already handled.
                delta.map.pop_back();
                continue;
            }
        }

        // Correct data is already in place.
        if (!is_done(*de))
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
                    assert(is_valid(maybe_cache));
                    cache_block(maybe_cache);
                }
            }

            if (is_download(*de))
            {
                // Here we are pretty sure we do not already have the data. If old file already had the data then delta
                // resolution would already have de->local_offset set and this branch would not be executed. If there
                // are more than one identical block then first block gets downloaded through this branch and all of
                // it's siblings get their local offset set here so download will not be carried out for the identical
                // block of data again.
                zinc_log("% 4d offset download", de->block_offset);
                auto data = get_data(de->block_index, block_size);
                file->write(&data.front(), de->block_offset, data.size());

                auto identical_blocks_it = delta.identical_blocks.find(de->block_index);
                if (identical_blocks_it != delta.identical_blocks.end())
                {
                    // Other identical blocks will copy data from just downloaded sibling.
                    for (int64_t sibling_index : identical_blocks_it->second)
                    {
                        if (sibling_index < delta.map.size())
                            delta.map[sibling_index].local_offset = de->block_offset;
                    }
                }
            }
            else
            {
                auto it_cache = block_cache.find(de->local_offset);
                if (it_cache != block_cache.end())
                {   // Use cached block if it exists
                    ByteArrayRef& cached_block = it_cache->second;
                    cached_block.refcount--;

                    file->write(&cached_block.data.front(), de->block_offset, cached_block.data.size());
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
                    auto block = file->read(de->local_offset, block_size);
                    file->write(block, de->block_offset, block_size);
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
            de->block_index = -1;                // Mark block as invalid so it is not handled twice in this loop.
        else
            delta.map.pop_back();

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

bool patch_file(void* file_data, int64_t file_size, size_t block_size, DeltaMap& delta,
    const FetchBlockCallback& get_data, const ProgressCallback& report_progress)
{
    auto file = std::make_unique<Buffer>(file_data, file_size);
    return patch_file(file.get(), block_size, delta, get_data, report_progress);
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

#if ZINC_NO_MMAP
    auto file = std::make_unique<File>(file_path, File::ReadWrite);
#else
    auto file = std::make_unique<MemoryMappedFile>(file_path);
#endif

    if (patch_file(file.get(), block_size, delta, get_data, report_progress))
    {
        file = nullptr;

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
