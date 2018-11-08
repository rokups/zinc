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
#include "zinc/zinc.h"

using namespace zinc::detail;

namespace zinc
{

using ByteArray = std::vector<uint8_t>;

const Parameters default_parameters{};

struct Range
{
    int64_t start;
    int64_t length;
};

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
    char link[32];
    snprintf(link, sizeof(link), "/proc/self/fd/%d", fd);
    return fopen(link, access);
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

/// Partition data_length into part_count number of strips. First strip may be longer than the others.
std::vector<Range> partition_data(size_t part_count, int64_t data_length, int64_t min_part_length = 1)
{
    std::vector<Range> result;

    if (data_length < static_cast<int64_t>(part_count))
        part_count = static_cast<size_t>(data_length);

    int64_t avg_length = std::max<int64_t>(data_length / part_count, min_part_length);
    result.reserve(part_count);

    auto prev_offset = 0L;
    while (part_count > 0)
    {
        part_count--;
        auto length = data_length - prev_offset;
        if (part_count > 0)
            length = std::min(length, avg_length);
        if (length == 0)
            break;
        result.emplace_back(Range{.start = prev_offset, .length = length});
        prev_offset += length;
    }
    return result;
}

//////////////////////////////////////////////// file partitioning /////////////////////////////////////////////////////

BoundaryList partition_file_worker(FILE* file, int64_t start_offset, int64_t length, DividedProgress& progress,
    std::atomic<bool>* cancel, const Parameters* parameters)
{
    BoundaryList result;
    if (!file)
        return result;

    FILE* task_file = duplicate_file(file, "rb");
    if (task_file != nullptr)
        file = task_file;

    std::vector<uint8_t> buffer;
    buffer.resize(parameters->read_buffer_size);
    auto sz = buffer.size();
    (void)(sz);

    const auto mask = (1U << parameters->match_bits) - 1U;
    auto buffer_size = std::min<int64_t>(length, parameters->read_buffer_size);

    fseek(file, start_offset, SEEK_SET);
    int64_t read = fread(&buffer[0], 1, static_cast<size_t>(buffer_size), file);
    if (read != buffer_size)
    {
        fclose(file);
        return result;
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
                result.emplace_back(Boundary{.start = block_start, .fingerprint = fingerprint, .hash = 0, .length = 0});
            }

            fingerprint = buzhash_update(fingerprint, data[0], data[window_length], window_length);
            data++;
        }

        auto bytes_consumed = buffer_size - window_length;

        progress.consume(bytes_consumed);

        length -= bytes_consumed;
        start_offset += bytes_consumed;

        if (cancel != nullptr && cancel->load(std::memory_order_relaxed))
        {
            fclose(file);
            return {};
        }

        buffer_size = std::min<int64_t>(length, parameters->read_buffer_size);
        fseek(file, start_offset, SEEK_SET);
        read = fread(&buffer[0], 1, static_cast<size_t>(buffer_size), file);
        if (read != buffer_size)
        {
            fclose(file);
            return {};
        }
        data = data_start;
    } while (length > window_length);

    progress.consume(length);

    if (task_file != nullptr)
        fclose(task_file);

    return result;
}

bool compute_hashes_worker(FILE* file, BoundaryList& result, int64_t item_start, int64_t item_count,
    std::atomic<bool>* cancel, DividedProgress& progress)
{
    if (!file)
        return false;

    FILE* task_file = duplicate_file(file, "rb");
    if (task_file != nullptr)
        file = task_file;

    std::vector<uint8_t> buffer;
    for (auto i = item_start, end = item_start + item_count; i < end; i++)
    {
        auto& block = result[i];
        fseek(file, block.start, SEEK_SET);
        if (buffer.size() < static_cast<size_t>(block.length))
            buffer.resize(static_cast<size_t>(block.length));

        auto len = fread(&buffer[0], 1, static_cast<size_t>(block.length), file);
        block.hash = fnv64a(&buffer[0], len);
        progress.consume(block.length);
        if (cancel != nullptr && cancel->load(std::memory_order_relaxed))
        {
            fclose(file);
            return false;
        }
    }

    if (task_file != nullptr)
        fclose(task_file);

    return true;
}

BoundaryList partition_file_task(FILE* file, size_t max_threads, std::atomic<int64_t>* bytes_done,
    std::atomic<bool>* cancel, const Parameters* parameters)
{
    if (max_threads == 0)
        max_threads = std::thread::hardware_concurrency();
    max_threads = std::max<size_t>(max_threads, 1);

    if (!file)
        return {};

    auto file_size = get_file_size(file);
    BoundaryList result;
    result.resize(1);                                               // File start always contains a fake split point.

    DividedProgress progress(file_size, 2, bytes_done);             // Will process file twice

    // Partition file
    {
        std::vector<std::future<BoundaryList>> tasks;
        for (auto range : partition_data(max_threads, file_size, parameters->window_length))
        {
            tasks.emplace_back(std::async(std::launch::async, &partition_file_worker, file,
                range.start, range.length, std::ref(progress), cancel, parameters));
        }

        // Gather worker results
        for (auto& task : tasks)
        {
            auto task_result = task.get();
            result.insert(result.end(), task_result.begin(), task_result.end());
        }
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

    // Calculate strong hashes for blocks
    std::vector<std::future<bool>> tasks;
    for (auto range : partition_data(max_threads, result.size()))
    {
        tasks.emplace_back(std::async(std::launch::async, &compute_hashes_worker, file, std::ref(result),
            range.start, range.length, cancel, std::ref(progress)));
    }

    // Finish hashing tasks
    for (auto& task : tasks)
        task.get();

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
