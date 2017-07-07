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
#include "DeltaResolver.h"
#include "RollingChecksum.hpp"


namespace zinc
{

DeltaResolver::DeltaResolver(
    const void* file_data, int64_t file_size, size_t block_size, const RemoteFileHashList& hashes,
    const ProgressCallback& report_progress, size_t concurrent_threads)
    : hashes(hashes)
      , report_progress(report_progress)
      , file_data((uint8_t*)file_data)
      , file_size(file_size)
      , block_size(block_size)
      , concurrent_threads(concurrent_threads)
{
    bytes_consumed_total = 0;
#if ZINC_USE_SKA_FLAT_HASH_MAP
    lookup_table.reserve(hashes.size());
        identical_blocks.reserve(file_size / block_size + 1);
#endif

    delta.map.reserve(hashes.size());
    for (size_t block_index = 0; block_index < hashes.size(); block_index++)
        delta.map.push_back(DeltaElement(block_index, block_index * block_size));

    RollingChecksum weak;
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
}

void DeltaResolver::add_thread(int64_t start, int64_t length)
{
    thread_extents.push_back({start, length});
}

void DeltaResolver::wait()
{
    bool done = false;
    for (; !done;)
    {
        lock.lock();
        while (active_threads.size() < concurrent_threads && thread_extents.size())
        {
            active_threads.push_back(std::thread(std::bind(&DeltaResolver::resolve_concurrent, this,
                                                           thread_extents.back().first,
                                                           thread_extents.back().second)));
            thread_extents.pop_back();
        }
        done = active_threads.size() < 1;
        lock.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        auto bytes_total = bytes_consumed_total.load();
        auto bytes_done_now = bytes_total - last_progress_report;
        if (bytes_done_now)
        {
            report_progress(bytes_done_now, bytes_total, file_size);
            last_progress_report = bytes_total;
        }
    }
}

void DeltaResolver::resolve_concurrent(int64_t start_index, int64_t block_length)
{
    lock.lock();
    lock.unlock();

    block_length = std::min(file_size - start_index, block_length);                 // Make sure we do not
    // go out of bounds.
#if ZINC_USE_SKA_FLAT_HASH_MAP
    ska::flat_hash_map<int64_t, StrongHash> local_hash_cache;
        local_hash_cache.reserve(file_size / block_size + 1);
#else
    std::unordered_map<int64_t, StrongHash> local_hash_cache;
#endif

    // `- block_size` at the end compensates for w_start being increased on the first pass when weak checksum is
    // empty. Checksum is always empty on the first pass.
    const uint8_t* w_start = file_data + start_index - block_size;
    if (start_index >= block_size)
    {
        // `- block_size + 1` adjusts starting pointer so that we start consuming bytes in specified block from the
        // first one. Naturally it must include `block_size - 1` bytes from previous block so that rolling hash is
        // calculated through entire file. Previous block stops after consuming last byte before `start_index`.
        w_start = w_start - block_size + 1;
    }
    RollingChecksum weak;
    WeakHash last_failed_weak = 0;
    int64_t bytes_consumed = 0;
    int64_t bytes_consumed_last_report = 0;
    bool last_failed = false;
    const auto last_local_hash_check_offset = file_size - block_size;

    for (; bytes_consumed < block_length;)
    {
        // Progress reporting.
        {
            auto bytes_consumed_diff = bytes_consumed - bytes_consumed_last_report;
            if (bytes_consumed_diff >= block_size)
            {
                bytes_consumed_total += bytes_consumed_diff;
                bytes_consumed_last_report = bytes_consumed;
            }
        }

        size_t current_block_size = (size_t)std::min<int64_t>(block_size, block_length - bytes_consumed);
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
        if (last_failed && last_failed_weak == weak_digest)
        {
            // Corner-case optimization. Some data may contain repeating patterns of identical data that is present
            // in local file but not present in remote file. In such cases lookup_table and StrongHash calculations
            // would be performed continously for every byte consumed by rolling checksum and would yield no
            // results. We save last failed weak checksum and when window is moved by one byte and when new rolling
            // checksum is identical to last one we assume there will be no match and continue rolling in next byte.
            // This optimization may cause some data blocks to not be reused in rare cases of weak checksum
            // collisions, however it saves us from some data slowing algorithm to a crawl therefore it is
            // well-worth trading in few blocks of bandwidth for speed gain. This issue was observed in updating
            // archlinux-2017.06.01-x86_64.iso -> archlinux-2017.07.01-x86_64.iso
            continue;
        }

        auto it = lookup_table.find(weak_digest);
        if (it != lookup_table.end())
        {
            auto strong_hash = StrongHash(w_start, current_block_size);
            auto jt = it->second.find(strong_hash);
            if (jt != it->second.end())
            {
                if (last_failed)
                    last_failed = false;

                int64_t this_block_index = jt->second;
                auto local_offset = w_start - file_data;
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
                        StrongHash local_hash(&file_data[block_offset], block_size);
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

                delta.map[this_block_index].local_offset = w_start - file_data;
                weak.clear();
            }
            else
            {
                last_failed = true;
                last_failed_weak = weak_digest;
            }
        }
        else
        {
            last_failed = true;
            last_failed_weak = weak_digest;
        }
    }

    // Ensure all bytes are reported.
    bytes_consumed_total += bytes_consumed - bytes_consumed_last_report;

    lock.lock();
    for (auto it = active_threads.begin(); it != active_threads.end(); it++)
    {
        if ((*it).get_id() == std::this_thread::get_id())
        {
            (*it).detach();
            active_threads.erase(it);
            break;
        }
    }
    lock.unlock();
}
}
