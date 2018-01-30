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
#include "HashBlocksTask.h"
#include "Utilities.hpp"


zinc::HashBlocksTask::HashBlocksTask()
    : Task(nullptr, 0, 0)
{}

zinc::HashBlocksTask::HashBlocksTask(const void* file_data, int64_t file_size, size_t block_size, size_t thread_count)
    : _block_size(block_size)
      , Task(file_data, file_size, thread_count)
{
    queue_tasks();
}

zinc::HashBlocksTask::HashBlocksTask(const char* file_name, size_t block_size, size_t thread_count)
    : _block_size(block_size)
      , Task(nullptr, 0, thread_count)
{
    if (_mapping.open(file_name))
    {
        _file_data = static_cast<const uint8_t*>(_mapping.get_data());
        _bytes_total = _mapping.get_size();
        queue_tasks();
    }
}

void zinc::HashBlocksTask::queue_tasks()
{
    auto block_count = _bytes_total / _block_size;
    if ((_bytes_total % _block_size) != 0)
        ++block_count;

    _result.resize(block_count);

    auto i = 0;
    auto blocks_per_thread = std::max<size_t>(block_count / _thread_count + 1, 1);
    for (; block_count >= blocks_per_thread; i++)
    {
        _pool.enqueue(&HashBlocksTask::process, this, i * blocks_per_thread, blocks_per_thread);
        block_count -= blocks_per_thread;
    }

    // Last thread may get less blocks to process.
    if (block_count > 0)
        _pool.enqueue(&HashBlocksTask::process, this, i * blocks_per_thread, block_count);
}

void zinc::HashBlocksTask::process(size_t block_start, size_t block_count)
{
    const uint8_t* fp = _file_data + block_start * _block_size;

    // Pre-process last possibly incomplete block first.
    {
        block_count--;
        auto last_block_size = std::min<int64_t>(_bytes_total - (block_start + block_count) * _block_size, _block_size);
        auto fp_last = fp + block_count * _block_size;
        std::vector<uint8_t> padded_block;
        if (last_block_size < _block_size)
        {
            padded_block.resize(_block_size, 0);
            memcpy(&padded_block.front(), fp_last, last_block_size);
            fp_last = &padded_block.front();
        }

        BlockHashes& h = _result[block_start + block_count];
        h.weak = RollingChecksum(fp_last, _block_size).digest();
        h.strong = strong_hash(fp_last, _block_size);
        _bytes_done += last_block_size;
    }

    // Process other blocks as usual, their size is equal to `block_size`.
    for (size_t block_index = block_start, block_end = block_start + block_count;
         block_index < block_end; ++block_index)
    {
        if (_cancel.load())
        {
            zinc_log("User interrupted get_block_checksums().");
            return;
        }

        BlockHashes& h = _result[block_index];
        h.weak = RollingChecksum(fp, _block_size).digest();
        h.strong = strong_hash(fp, _block_size);
        fp += _block_size;
        _bytes_done += _block_size;
    }
}
