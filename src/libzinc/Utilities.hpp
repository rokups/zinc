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

#if _WIN32
#   include <windows.h>
#	undef min
#	undef max
#else
#	include <fcntl.h>
#	include <sys/stat.h>
#	include <unistd.h>
#   include <sys/mman.h>
#endif
#include <string>

namespace zinc
{

class FileMemoryMap
{
public:
    FileMemoryMap();
    ~FileMemoryMap() { close(); }

    /// Get pointer to mapped memory.
    void* get_data() { return _data; }
    /// Get size of mapped memory.
    size_t get_size() { return _size; }
    /// Verify if file mapping is open.
    bool is_open() { return _data && _data != (void*)-1; }
    /// Map file to memory. If block_size is not 0 then mapped memory segment size will be multiple of block_size.
    bool open(const char* file_path);
    /// Close memory mapping.
    void close();

protected:
#if _WIN32
    /// Memory mapping file descriptor.
    HANDLE _fd;
    HANDLE _mapping;
#else
    /// Memory mapping file descriptor.
    int _fd;
#endif
    /// Size of mapped file.
    size_t _size;
    /// Memory of mapped file.
    void* _data;
};

#if _WIN32
std::wstring to_wstring(const std::string& str);
int truncate(const char* file_path, int64_t file_size);
#endif
int64_t round_up_to_multiple(int64_t value, int64_t multiple_of);
int64_t get_file_size(const char* file_path);

};
