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

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

namespace zinc
{

class FileMemoryMap
{
public:
    FileMemoryMap() { }
    ~FileMemoryMap() { close(); }

    /// Get pointer to mapped memory.
    void* get_data() { return _data; }
    /// Get size of mapped memory.
    size_t get_size() { return _size; }
    /// Verify if file mapping is open.
    bool is_open() { return _size > 0 && _fd != -1 && _data != (void*)-1; }
    /// Map file to memory. If block_size is not 0 then mapped memory segment size will be multiple of block_size.
    bool open(const char* file_path, size_t block_size=0)
    {
        struct stat st = {};
        _fd = ::open(file_path, O_RDWR);

        if (_fd == -1)
            return false;//"FileMapping could not open file");

        if (fstat(_fd, &st) == -1)
            return false;//"FileMapping could not get file size");

        _size = (size_t)st.st_size;
        if (block_size)
        {
            if (auto remainder = _size % block_size)
            {
                auto add_len = block_size - remainder;
                fallocate(_fd, 0, _size, add_len);
                _size += add_len;
            }
        }

        _data = mmap(0, _size, PROT_READ | PROT_WRITE, MAP_SHARED, _fd, 0);
        if (_data == (void*)-1)
            return false;//"FileMapping could not get map file data");

        return true;
    }
    /// Close memory mapping.
    void close()
    {
        if (_data != (void*)-1)
        {
            munmap(_data, _size);
            _data = (void*)-1;
        }

        if (_fd != -1)
        {
            ::close(_fd);
            _fd = -1;
        }
    }

protected:
    /// Size of mapped file.
    size_t _size = 0;
    /// Memory of mapped file.
    void* _data = (void*)-1;
    /// Memory mapping file descriptor.
    int _fd = -1;
};

};
