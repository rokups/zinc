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
#if ZINC_WITH_EXCEPTIONS
#   include <exception>
#endif
#if __cplusplus >= 201402L
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wshift-count-overflow"
#   include <flat_hash_map.hpp>
#   pragma GCC diagnostic pop
#   define ZINC_USE_SKA_FLAT_HASH_MAP 1
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
    size_t refcount = 1;
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
        const auto message = "file_data, file_size and block_size must be positive numbers.";
#if ZINC_WITH_EXCEPTIONS
        throw std::invalid_argument(message);
#else
        ZINC_LOG(message);
        return hashes;
#endif
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
                ZINC_LOG("User interrupted get_block_checksums().");
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
        const auto message = "file_size must be multiple of block_size.";
#if ZINC_WITH_EXCEPTIONS
        throw std::invalid_argument(message);
#else
        ZINC_LOG(message);
        return DeltaMap();
#endif
    }

    DeltaMap delta;
    delta.reserve(hashes.size());
    for (size_t block_index = 0; block_index < hashes.size(); block_index++)
        delta.push_back(DeltaElement(block_index, block_index * block_size));

    // When file is not present delta equals to full download. This is not an error.
    if (!file_data)
        return delta;

    // Sort hashes for easier lookup
#if ZINC_USE_SKA_FLAT_HASH_MAP
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
            const auto message = "Could not truncate file_path to required size.";
#if ZINC_WITH_EXCEPTIONS
            throw std::system_error(err, std::system_category(), message);
#else
            ZINC_LOG(message);
            return DeltaMap();
#endif
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
        const auto message = "File data must be multiple of a block size.";
#if ZINC_WITH_EXCEPTIONS
        throw std::invalid_argument(message);
#else
        ZINC_LOG(message);
        return false;
#endif
    }

    if (file_size < 1)
    {
        const auto message = "file_size must be a positive number.";
#if ZINC_WITH_EXCEPTIONS
        throw std::invalid_argument(message);
#else
        ZINC_LOG(message);
        return false;
#endif
}

    // Reference cache maps block index to a list of other blocks that are very likely to use data at specified index.
    std::vector<std::vector<DeltaElement>> ref_cache((unsigned int)(file_size / block_size));
    {
        size_t block_index = 0;
        for (auto it = delta.begin(); it != delta.end(); it++)
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
    block_cache.reserve(std::max((size_t)10, delta.size() / 10));
#else
    std::unordered_map<size_t, ByteArrayRef> block_cache;
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
            ZINC_LOG("% 4d offset +cache refcount=1", cacheable.local_offset);
        }
        else
        {
            ByteArrayRef &cached_block = (*block_cache_it).second;
            cached_block.refcount++;
            ZINC_LOG("% 4d offset +cache refcount=%d", cacheable.local_offset, cached_block.refcount);
        }
        // Prioritize cached blocks so we can evict data from cache as soon as possible.
        priority_index.push_back(cacheable.block_index);
    };

    while (delta.size())
    {
        DeltaElement de;
        if (priority_index.size())
        {
            // Handle blocks which have data cached first so cache can be cleared up.
            auto index = priority_index.back();
            if (index >= delta.size())
            {
                // This block was already handled normally.
                priority_index.pop_back();
                continue;
            }
            auto& de_ref = delta[index];
            de = de_ref;
            de_ref.block_index = -1;                // Mark block as invalid so it is not handled twice in this loop.
            priority_index.pop_back();
        }
        else
        {
            // Handle blocks normally, walking from the back of the file.
            de = delta.back();
            delta.pop_back();
            if (!de.is_valid())
            {
                // This block was already handled by prioritizing cached blocks.
                continue;
            }
        }

        // Correct data is already in place.
        if (!de.is_done())
        {
            // Cache data if writing current block overwrites data needed by any other blocks.
            {
                DeltaElement maybe_cache;
                size_t cached_blocks = 0;

                // Check three clusters of cacheable blocks. Blocks in these clusters have a chance to overlap with
                // current position we are about to write. If any block overlaps with current block - data required for
                // that block will be cached.
                for (auto check_index = std::max<int64_t>(de.block_index - 1, 0),
                          end_index = std::min<int64_t>(de.block_index + 2, ref_cache.size());
                     check_index < end_index; check_index++)
                {
                    auto& subcache = ref_cache[check_index];
                    for (auto it = subcache.begin(); it != subcache.end();)
                    {
                        auto& cacheable = *it;
                        // Make sure block requires data from position that overlaps with current block.
                        if (llabs(cacheable.local_offset - de.block_offset) < block_size)
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

                if (cached_blocks == 1 && maybe_cache.block_index == de.block_index)
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

            if (de.is_download())
            {
                ZINC_LOG("% 4d offset download", de.block_offset);
                auto data = get_data(de.block_index, block_size);
                memcpy(&fp[de.block_offset], &data.front(), data.size());
            }
            else
            {
                auto it_cache = block_cache.find(de.local_offset);
                if (it_cache != block_cache.end())
                {   // Use cached block if it exists
                    ByteArrayRef& cached_block = it_cache->second;
                    cached_block.refcount--;
                    memmove(&fp[de.block_offset], &cached_block.data.front(), cached_block.data.size());
                    ZINC_LOG("% 4d offset using cached offset %d", de.block_offset, de.local_offset);

                    // Remove block from cache only if delta map does not reference block later
                    if (cached_block.refcount == 0)
                    {
                        block_cache.erase(it_cache);
                        ZINC_LOG("% 4d offset -cache", de.local_offset);
                    }
                }
                else
                {   // Copy block from later file position
                    memmove(&fp[de.block_offset], &fp[de.local_offset], block_size);
                    ZINC_LOG("% 4d offset using file data offset %d", de.block_offset, de.local_offset);
                    // Delete handled block from reference cache lookup table if it exists there. This prevents handled
                    // blocks form getting cached as that cached data would not be needed any more.
                    auto& subcache = ref_cache[de.local_offset / block_size];
                    auto it = std::find(subcache.begin(), subcache.end(), de);
                    if (it != subcache.end())
                        subcache.erase(it);
                }
            }
        }

        if (report_progress)
        {
            if (!report_progress(block_size, file_size - delta.size() * block_size, file_size))
            {
                ZINC_LOG("User interrupted patch_file().");
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
        const auto message = "file_final_size must be positive number.";
#if ZINC_WITH_EXCEPTIONS
        throw std::invalid_argument(message);
#else
        ZINC_LOG(message);
        return false;
#endif
    }

    auto file_size = get_file_size(file_path);
    if (file_size < 0)
    {
        touch(file_path);
        file_size = 0;
    }

    // Local file must be at least as big as remote file and it must be padded to size multiple of block_size.
    auto max_required_size = std::max<int64_t>(block_size * delta.size(), round_up_to_multiple(file_size, block_size));
    auto err = truncate(file_path, max_required_size);
    if (err != 0)
    {
        const auto message = "Could not truncate file_path to required size.";
#if ZINC_WITH_EXCEPTIONS
        throw std::system_error(err, std::system_category(), message);
#else
        ZINC_LOG(message);
        return false;
#endif
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
            const auto message = "Could not truncate file_path to file_final_size.";
#if ZINC_WITH_EXCEPTIONS
            throw std::system_error(err, std::system_category(), message);
#else
            ZINC_LOG(message);
            return false;
#endif
        }
        return true;
    }
#if ZINC_WITH_EXCEPTIONS
    else
        throw std::runtime_error("Unknown error.");
#else
    ZINC_LOG("Unknown error.");
    return false;
#endif
}

};
