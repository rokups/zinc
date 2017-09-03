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
#pragma once


#include "ThreadPool.h"
#include <chrono>


namespace zinc
{

template<typename T>
class Task : public ITask<T>
{
protected:
    const uint8_t* _file_data = nullptr;
    int64_t _bytes_total = 0;
    std::atomic<int64_t> _bytes_done;
    std::atomic<bool> _cancel;
    size_t _thread_count = 0;
    ThreadPool _pool;
    T _result;

public:
    Task(const void* file_data, int64_t file_size, size_t thread_count)
        : _file_data(static_cast<const uint8_t*>(file_data))
          , _bytes_total(file_size)
          , _thread_count(thread_count)
          , _pool(thread_count)
    {
        _cancel.store(false);
        _bytes_done.store(0);
    }

    Task(const Task& other) = delete;

    ~Task() override = default;

    float progress() const override
    {
        return 100.f / _bytes_total * _bytes_done;
    }

    bool is_done() const override
    {
        return _cancel.load() || _bytes_total == _bytes_done;
    }

    bool success() const override
    {
        return !_cancel.load() && _bytes_total == _bytes_done && _file_data > 0;
    }

    T& result() override
    {
        return _result;
    }

    void cancel() override
    {
        _cancel.store(true);
    }

    Task<T>* wait() override
    {
        using namespace std::chrono_literals;

        while (!is_done())
            std::this_thread::sleep_for(300ms);

        return this;
    }
};

}
