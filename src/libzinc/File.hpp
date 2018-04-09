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


#include <fstream>
#include "mmap/FileMemoryMap.h"

namespace zinc
{

class IFile
{
public:
    using DataPointer = std::unique_ptr<const void, void(*)(const void*)>;

    virtual DataPointer read(int64_t offset, int64_t length) = 0;
    virtual void write(const void* data, int64_t offset, int64_t length) = 0;
    virtual bool is_valid() { return true; }
    virtual int64_t get_size() = 0;
};

class Buffer : public IFile
{
public:
    explicit Buffer(void* data, int64_t dlen)
        : _data(reinterpret_cast<uint8_t*>(data))
        , _size(dlen)
    {
    }

    DataPointer read(int64_t offset, int64_t length) override
    {
        assert(offset + length <= _size);
        return DataPointer(reinterpret_cast<const void*>(_data + offset), [](const void*) { });
    }

    void write(const void* data, int64_t offset, int64_t length) override
    {
        assert(offset + length <= _size);
        memmove((uint8_t*)_data + offset, data, length);
    }

    bool is_valid() override { return _data != nullptr && _size > 0; }
    int64_t get_size() override { return _size; }

protected:
    uint8_t* _data = nullptr;
    int64_t _size = 0;
};

class MemoryMappedFile : public IFile
{
public:
    explicit MemoryMappedFile(const char* file_path)
    {
        _mmap.open(file_path);
    }

    DataPointer read(int64_t offset, int64_t length) override
    {
        assert(offset + length <= _mmap.get_size());
        return DataPointer(reinterpret_cast<const void*>(reinterpret_cast<const uint8_t*>(_mmap.get_data()) + offset), [](const void*) { });
    }

    void write(const void* data, int64_t offset, int64_t length) override
    {
        assert(offset + length <= _mmap.get_size());
        memmove((uint8_t*)_mmap.get_data() + offset, data, length);
    }

    bool is_valid() override { return _mmap.is_open(); }
    int64_t get_size() override { return _mmap.get_size(); }

protected:
    FileMemoryMap _mmap;
};

class File : public IFile
{
public:
    explicit File(const char* file_path)
    {
        _fp.open(file_path, std::ios_base::in | std::ios_base::out | std::ios_base::binary | std::ios_base::ate);
        if (!_fp.is_open())
            return;

        _size = _fp.tellg();
    }

    DataPointer read(int64_t offset, int64_t length) override
    {
        assert(offset + length <= _size);
        std::lock_guard<std::mutex> lock(_m);
        void* buffer = malloc(length);
        _fp.seekg(offset);
        _fp.read(static_cast<char*>(buffer), length);
        return DataPointer(buffer, [](const void* p) { free(const_cast<void*>(p)); });
    }

    void write(const void* data, int64_t offset, int64_t length) override
    {
        assert(offset + length <= _size);
        std::lock_guard<std::mutex> lock(_m);
        _fp.seekp(offset);
        _fp.write((char*)data, length);
    }

    bool is_valid() override { return _fp.is_open(); }
    int64_t get_size() override { return _size; }

protected:
    std::fstream _fp;
    int64_t _size = 0;
    std::mutex _m;
};

}
