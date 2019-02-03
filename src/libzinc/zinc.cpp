/*
 * MIT License
 *
 * Copyright (c) 2018 Rokas Kupstys
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
#if _WIN32
#   include <windows.h>
#   include <io.h>
#else
#   include <sys/stat.h>
#   include <unistd.h>
#endif
#include <unordered_map>
#include <algorithm>
#include <functional>
#include <cassert>
#include "zinc/zinc.h"

using namespace zinc::detail;

namespace zinc
{

using ByteArray = std::vector<uint8_t>;

const Parameters default_parameters{};

// Distance for integral types
template<typename Iter>
typename std::enable_if<std::is_integral<Iter>::value, long>::type
distance(Iter start, Iter end) { return end - start; }

// Distance for iterators
template<typename Iter>
typename std::enable_if<!std::is_integral<Iter>::value, long>::type
distance(Iter start, Iter end) { return std::distance(start, end); }

template<typename Iter, typename... Args>
void parallel_for(Iter start, Iter end, int num_threads, std::function<void(Iter start, Iter end, Args...)> functor, Args... args)
{
    if (num_threads == 0)
        num_threads = std::thread::hardware_concurrency();

    auto total_count = distance(start, end);
    auto per_thread_count = total_count / num_threads;
    auto remainder = total_count % num_threads;

    if (num_threads > 0)
    {
        // Parallel execution
        std::vector<std::thread> my_threads(static_cast<unsigned long>(num_threads));

        auto i = 0;
        for (; i < num_threads - 1; i++)
        {
            int part_start = start + i * per_thread_count;
            my_threads[i] = std::thread(functor, part_start, part_start + per_thread_count, args...);
        }

        // Last thread handles remainder as well
        int part_start = start + i * per_thread_count;
        my_threads[i] = std::thread(functor, part_start, part_start + per_thread_count + remainder, args...);

        std::for_each(my_threads.begin(), my_threads.end(), std::mem_fn(&std::thread::join));
    }
    else
    {
        // Sequential execution (pass -1 for number of threads)
        auto i = 0;
        for (; i < num_threads - 1; i++)
        {
            int part_start = start + i * per_thread_count;
            functor(part_start, part_start + per_thread_count, args...);
        }

        // Last thread handles remainder as well
        int part_start = start + i * per_thread_count;
        functor(part_start, part_start + per_thread_count + remainder, args...);
    }
}

/// Tracks progress reporting when range of bytes needs to be processed multiple times.
struct DividedProgress
{
    int64_t bytes_total = 0;
    std::atomic<int64_t> bytes_buffer{0};
    std::atomic<int64_t>* bytes_done;
    unsigned times = 0;

    DividedProgress(int64_t bytes_total_, unsigned times_, std::atomic<int64_t>* progress_result)
        : bytes_total(bytes_total_)
        , bytes_done(progress_result)
        , times(times_)
    {
    }

    /// After consuming `bytes` * `times` amount of bytes, bytes_done() will return `bytes`.
    inline void consume(int64_t bytes)
    {
        if (bytes_done == nullptr)
            return;

        bytes += bytes_buffer.exchange(0);
        bytes_buffer.fetch_add(bytes % times);
        bytes_done->fetch_add(bytes / times);
    }

    inline void flush()
    {
        if (bytes_done == nullptr)
            return;

        bytes_done->fetch_add(bytes_buffer);
        bytes_buffer = 0;
    }
};

int64_t get_file_size(FILE* file)
{
    if (!file)
        return 0;

    auto pos = ftell(file);
    fseek(file, 0, SEEK_END);

    auto result = ftell(file);
    fseek(file, pos, SEEK_SET);

    return result;
}

FILE* duplicate_file(FILE* file, const char* access)
{
    if (file == nullptr)
        return nullptr;

#if __linux__
    auto fd = fileno(file);
    if (fd < 0)
    {
        // Chances are we are dealing with handle from fmemopen()
        return fmemopen(file->_IO_buf_base, (uintptr_t)file->_IO_buf_end - (uintptr_t)file->_IO_buf_base, access);
    }
    else
    {
        char link[32];
        snprintf(link, sizeof(link), "/proc/self/fd/%d", fd);
        return fopen(link, access);
    }
#elif _WIN32
    if (HANDLE file_handle = reinterpret_cast<HANDLE>(_get_osfhandle(fileno(file))))
    {
        FILE_NAME_INFO info{ };
        if (GetFileInformationByHandleEx(file_handle, FileNameInfo, &info, sizeof(info)))
        {
            wchar_t w_access[8];
            for (int i = 0, end = strlen(access) + 1; i < end; i++)
                w_access[i] = access[i];
            return _wfopen(info.FileName, w_access);
        }
    }
    return nullptr;
#else
#   error TODO later
#endif
}

//////////////////////////////////////////////// file partitioning /////////////////////////////////////////////////////

BoundaryList partition_file_task(FILE* file, size_t max_threads, std::atomic<int64_t>* bytes_done,
    std::atomic<bool>* cancel, const Parameters* parameters)
{
    if (!file)
        return {};

    auto file_size = get_file_size(file);

    if (max_threads == 0)
        max_threads = std::thread::hardware_concurrency();
    max_threads = std::max<size_t>(max_threads, 1);
    if (static_cast<unsigned long long>(file_size) < max_threads * parameters->window_length)
        // This is a very tiny file, no poing in many threads.
        max_threads = 1;

    BoundaryList result;
    result.resize(1);                                               // File start always contains a fake split point.

    DividedProgress progress(file_size, 2, bytes_done);             // Will process file twice

    // Partition file
    {
        std::mutex result_lock{};
        std::function<void(int64_t, int64_t, FILE*)> worker([&](int64_t start_offset, int64_t end_offset, FILE* wfile)
        {
            BoundaryList local_result;
            wfile = duplicate_file(wfile, "rb");
            assert(wfile);

            int64_t length = end_offset - start_offset;
            std::vector<uint8_t> buffer;
            buffer.resize(parameters->read_buffer_size, 0);
            auto sz = buffer.size();
            (void)(sz);

            const auto mask = (1U << parameters->match_bits) - 1U;
            auto buffer_size = std::min<int64_t>(length, parameters->read_buffer_size);

            fseek(wfile, start_offset, SEEK_SET);
            int64_t read = fread(&buffer[0], 1, static_cast<size_t>(buffer_size), wfile);
            if (read != buffer_size)
            {
                fclose(wfile);
                return;
            }

            auto* data = &buffer[0];
            auto* data_start = data;
            auto window_length = parameters->window_length;
            auto fingerprint = buzhash(data, window_length);

            do
            {
                for (auto* end = data_start + buffer_size - window_length; data < end;)
                {
                    if ((fingerprint & mask) == 0)
                    {
                        auto block_start = start_offset + (data - data_start);
                        local_result.emplace_back(Boundary{.start = block_start, .fingerprint = fingerprint, .hash = 0, .length = 0});
                    }

                    fingerprint = buzhash_update(fingerprint, data[0], data[window_length], window_length);
                    data++;
                }

                auto bytes_consumed = buffer_size - std::min<int64_t>(window_length, buffer_size);

                progress.consume(bytes_consumed);

                length -= bytes_consumed;
                start_offset += bytes_consumed;

                if (cancel != nullptr && cancel->load(std::memory_order_relaxed))
                {
                    fclose(wfile);
                    return;
                }

                buffer_size = std::min<int64_t>(length, parameters->read_buffer_size);
                fseek(wfile, start_offset, SEEK_SET);
                read = fread(&buffer[0], 1, static_cast<size_t>(buffer_size), wfile);
                if (read != buffer_size)
                {
                    fclose(wfile);
                    return;
                }
                data = data_start;
            } while (length > window_length);

            progress.consume(length);

            fclose(wfile);

            // Insert results to main collection. This is done at the end in order to reduce contention on result_lock.
            result_lock.lock();
            result.insert(result.end(), local_result.begin(), local_result.end());
            result_lock.unlock();
        });
        parallel_for(0L, file_size, (int)max_threads, worker, file);
        // Results were appended out of order. We need them sorted.
        std::sort(result.begin(), result.end(), [](const Boundary& a, const Boundary& b) { return a.start < b.start; });
    }

    if (cancel != nullptr && cancel->load(std::memory_order_relaxed))
        return result;

    // Ensuring block sizes happens here in a single thread in order to ensure consistent results no matter how many
    // worker threads were used.
    if (result.size() > 1)
    {
        // Remove blocks that are too small
        auto next_offset = file_size;
        for (auto it = result.rbegin(); it != result.rend(); it++)
        {
            auto& split = *it;
            if (next_offset - split.start < parameters->min_block_size)
                result.erase((it + 1).base());
            next_offset = split.start;
        }
    }

    // Split big blocks into smaller ones
    auto prev_offset = 0L;
    std::vector<uint8_t> buffer;
    buffer.resize(parameters->window_length);

    for (auto it = result.begin(); it != result.end(); it++)
    {
        auto& split = *it;
        auto block_size = split.start - prev_offset;
        prev_offset = split.start;
        if (block_size > parameters->max_block_size)
        {
            auto new_blocks_count = block_size / parameters->max_block_size;
            auto new_block_size = block_size / (new_blocks_count + 1);
            BoundaryList splits;

            for (auto i = 1L; i < new_blocks_count + 1; i++)
            {
                auto start = split.start - (i * new_block_size);
                fseek(file, start, SEEK_SET);
                auto len = fread(&buffer[0], 1, parameters->window_length, file);
                auto fingerprint = buzhash(&buffer[0], static_cast<uint32_t>(len));
                splits.emplace_back(Boundary{.start = start, .fingerprint = fingerprint, .hash = 0, .length = 0});
            }

            it = result.insert(it, splits.rbegin(), splits.rend()) + splits.size();
        }
    }

    // Finalize fake split point
    {
        auto first_block_size = result.size() > 1 ? result[1].start : file_size;
        if (buffer.size() < static_cast<size_t>(first_block_size))
            buffer.resize(static_cast<unsigned long>(first_block_size));

        result[0] = Boundary{.start = 0, .fingerprint = 0, .hash = 0, .length = 0};

        fseek(file, 0, SEEK_SET);
        auto len = fread(&buffer[0], 1, parameters->window_length, file);
        result[0].fingerprint = buzhash(&buffer[0], static_cast<uint32_t>(len));

        fseek(file, 0, SEEK_SET);
        len = fread(&buffer[0], 1, static_cast<size_t>(first_block_size), file);
        result[0].hash = fnv64a(&buffer[0], len);
    }

    // Calculate block lengths
    for (size_t i = 0, end = result.size(); i < end; i++)
    {
        auto& block = result[i];
        auto next_block_start = i + 1 < result.size() ? result[i + 1].start : file_size;
        block.length = next_block_start - block.start;
    }

    parallel_for(0UL, result.size(), (int)max_threads, std::function<void(unsigned long, unsigned long, FILE*)>(
        [&](unsigned long item_start, unsigned long item_end, FILE* wfile)
    {
        wfile = duplicate_file(wfile, "rb");
        assert(wfile);

        std::vector<uint8_t> wbuffer;
        for (auto i = item_start; i < item_end; i++)
        {
            auto& block = result[i];
            fseek(wfile, block.start, SEEK_SET);
            if (wbuffer.size() < static_cast<size_t>(block.length))
                wbuffer.resize(static_cast<size_t>(block.length));

            auto len = fread(&wbuffer[0], 1, static_cast<size_t>(block.length), wfile);
            block.hash = fnv64a(&wbuffer[0], len);
            progress.consume(block.length);
            if (cancel != nullptr && cancel->load(std::memory_order_relaxed))
            {
                fclose(wfile);
                break;
            }
        }

        fclose(wfile);
    }), file);

    progress.flush();

    return result;
}

std::future<BoundaryList> partition_file(FILE* file, size_t max_threads, std::atomic<int64_t>* bytes_done,
    int64_t* bytes_to_process, std::atomic<bool>* cancel, const Parameters* parameters)
{
    if (bytes_done != nullptr)
        bytes_done->exchange(0);

    if (parameters == nullptr)
        parameters = &default_parameters;

    if (bytes_to_process != nullptr)
        *bytes_to_process = get_file_size(file);

    return std::async(std::launch::async, &partition_file_task, file, max_threads, bytes_done, cancel, parameters);
}

//////////////////////////////////////////////// file comparison ///////////////////////////////////////////////////////

std::unordered_map<int64_t, std::vector<const Boundary*>> create_boundary_lookup_table(const BoundaryList& boundary_list)
{
    std::unordered_map<int64_t, std::vector<const Boundary*>> result;
    for (const auto& boundary : boundary_list)
        result[boundary.hash].emplace_back(&boundary);
    return result;
}

bool intersects(const Boundary& a, const Boundary& b)
{
    auto a_end = a.start + a.length;
    auto b_end = b.start + b.length;
    return (b.start <= a.start && a.start < b_end) || (a.start <= b.start && b.start < a_end);
}

SyncOperationList compare_files(const BoundaryList& local_file, const BoundaryList& remote_file)
{
    SyncOperationList result;
    result.reserve(remote_file.size());

    auto local_file_table = create_boundary_lookup_table(local_file);

    // Iterate remote file and produce instructions to reassemble remote file from pieces available locally.
    for (const auto& block : remote_file)
    {
        enum Status
        {
            NotFound,               // Block is not present in local file
            Copied,                 // Block is present in local file and can be copied
            Present,                // Block is present in local file at required location, no action needed
        } status = NotFound;

        auto it_local_blocks = local_file_table.find(block.hash);
        if (it_local_blocks != local_file_table.end())
        {
            const Boundary* found = nullptr;
            for (const auto& local_block : it_local_blocks->second)
            {
                if (local_block->fingerprint == block.fingerprint && local_block->hash == block.hash && local_block->length == block.length)
                {
                    // Block was found in local file
                    if (local_block->start != block.start)
                    {
                        // Block was moved
                        found = local_block;
                        status = Copied;
                    }
                    else
                    {
                        // Exactly same block at required position exists in a local file. No need to do anything.
                        status = Present;
                        break;
                    }
                }
            }

            if (status == Copied)
                result.emplace_back(SyncOperation{.remote = &block, .local = found});
        }

        if (status == NotFound)
        {
            // Block does not exist in local file. Download.
            result.emplace_back(SyncOperation{.remote = &block, .local = nullptr});
        }
    }

    // Sort operations list
    for (auto i = 0UL; i < result.size(); i++)
    {
        for (auto j = i + 1; j < result.size(); j++)
        {
            auto& a = result[i];
            auto& b = result[j];

            // Solve circular dependencies, two blocks depending on each other's local data.
            if (a.local != nullptr && b.local != nullptr)
            {
                if (intersects(*a.remote, *b.local) && intersects(*b.remote, *a.local))
                {
                    // Force next block download
                    b.local = nullptr;
                }
            }

            // Make blocks with local source come before blocks that write to local source position
            if (b.local != nullptr && intersects(*a.remote, *b.local))
            {
                result.insert(result.begin() + i, b);
                result.erase(result.begin() + j + 1);
            }
        }
    }

    return result;
}

}
