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

#include <atomic>
#include <chrono>
#include <thread>
#include "File.hpp"


namespace zinc
{

template<typename T>
class Task : public ITask<T>
{
protected:
    IFile* _file = nullptr;
    int64_t _bytes_total = 0;
    std::atomic<int64_t> _bytes_done;
    std::atomic<bool> _cancel;
    size_t _thread_count = 0;
    std::vector<std::thread> _pool;
    T _result;
    std::mutex _lock_result;

public:
    Task(IFile* file, size_t thread_count)
        : _file(file)
        , _bytes_total(file->get_size())
        , _thread_count(thread_count)
    {
        _cancel.store(false);
        _bytes_done.store(0);
    }

    Task(const Task& other) = delete;

    ~Task() override
    {
        delete _file;
        _file = nullptr;
    }

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
        return !_cancel.load() && _bytes_total == _bytes_done && _file->is_valid();
    }

    T& result() override
    {
        delete _file;
        _file = nullptr;
        return _result;
    }

    void cancel() override
    {
        _cancel.store(true);
    }

    Task<T>* wait() override
    {
        for (auto& t : _pool)
            t.join();
        _pool.clear();

        return this;
    }

    int64_t size_total() override
    {
        return _bytes_total;
    }

    int64_t size_done() override
    {
        return _bytes_done;
    }
};

}
