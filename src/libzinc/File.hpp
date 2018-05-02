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
#include <mutex>
#include "mmap/FileMemoryMap.h"

namespace zinc
{

class IFile
{
public:
    virtual ~IFile() = default;
    virtual const void* read(int64_t offset, int64_t length) = 0;
    virtual void write(const void* data, int64_t offset, int64_t length) = 0;
    virtual bool is_valid() { return true; }
    virtual int64_t get_size() = 0;
};

class Buffer : public IFile
{
public:
    inline explicit Buffer(void* data, int64_t dlen)
        : _data(reinterpret_cast<uint8_t*>(data))
        , _size(dlen)
    {
    }

    inline const void* read(int64_t offset, int64_t length) override
    {
        assert(offset + length <= _size);
        return reinterpret_cast<const void*>(_data + offset);
    }

    inline void write(const void* data, int64_t offset, int64_t length) override
    {
        assert(offset + length <= _size);
        memmove((uint8_t*)_data + offset, data, length);
    }

    inline bool is_valid() override { return _data != nullptr && _size > 0; }
    inline int64_t get_size() override { return _size; }

protected:
    uint8_t* _data = nullptr;
    int64_t _size = 0;
};

class MemoryMappedFile : public IFile
{
public:
    inline explicit MemoryMappedFile(const char* file_path)
    {
        _mmap.open(file_path);
    }

    inline const void* read(int64_t offset, int64_t length) override
    {
        assert(offset + length <= _mmap.get_size());
        return reinterpret_cast<const void*>(reinterpret_cast<const uint8_t*>(_mmap.get_data()) + offset);
    }

    inline void write(const void* data, int64_t offset, int64_t length) override
    {
        assert(offset + length <= _mmap.get_size());
        memmove((uint8_t*)_mmap.get_data() + offset, data, length);
    }

    inline bool is_valid() override { return _mmap.is_open(); }
    inline int64_t get_size() override { return _mmap.get_size(); }

protected:
    FileMemoryMap _mmap;
};

class File : public IFile
{
public:
    enum Flags : unsigned
    {
        Read = 1,
        Write = 2,
        ReadWrite = Read | Write
    };

    inline explicit File(const char* file_path, Flags flags)
    {
        auto streamFlags = std::ios_base::binary | std::ios_base::ate;
        if (flags & Read)
            streamFlags |= std::ios_base::in;
        if (flags & Write)
            streamFlags |= std::ios_base::out;

        _fp.open(file_path, streamFlags);
        if (!_fp.is_open())
            return;

        _size = _fp.tellg();
    }

    inline const void* read(int64_t offset, int64_t length) override
    {
        assert(offset + length <= _size);

        int64_t oft = offset - _buffer_pos;
        if (oft < 0 || (_buffer_size - oft) < length)
        {
            _fp.seekg(offset);
            _buffer_size = std::min<int64_t>(sizeof(_buffer), _size - offset);
            _fp.read(_buffer, _buffer_size);
            _buffer_pos = offset;
            oft = 0;
        }

        return reinterpret_cast<const void*>(&_buffer[oft]);
    }

    inline void write(const void* data, int64_t offset, int64_t length) override
    {
        assert(offset + length <= _size);
        _fp.seekp(offset);
        _fp.write((char*)data, length);
    }

    inline bool is_valid() override { return _fp.is_open(); }
    inline int64_t get_size() override { return _size; }

protected:
    std::fstream _fp;
    int64_t _size = 0;
    char _buffer[50 * 1024 * 1024]{};
    int64_t _buffer_pos = 0;
    int64_t _buffer_size = 0;
};

}
