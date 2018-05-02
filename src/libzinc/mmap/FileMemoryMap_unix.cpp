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
#include "FileMemoryMap.h"
#include "zinc_error.hpp"
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>


namespace zinc
{

bool FileMemoryMap::open(const char* file_path)
{
    struct stat st = {};
    _fd = ::open(file_path, O_RDWR);

    if (_fd == -1)
    {
        zinc_error<std::system_error>("FileMapping could not open file.", errno);
        return false;
    }

    if (fstat(_fd, &st) == -1)
    {
        zinc_error<std::system_error>("FileMapping could not get file size.", errno);
        return false;
    }

    _size = st.st_size;

    _data = mmap(nullptr, static_cast<size_t>(_size), PROT_READ | PROT_WRITE, MAP_SHARED, _fd, 0);
    if (!is_open())
    {
        zinc_error<std::system_error>("FileMapping could not get map file data.", errno);
        return false;
    }

    return true;
}

/// Close memory mapping.
void FileMemoryMap::close()
{
    if (is_open())
    {
        munmap(_data, static_cast<size_t>(_size));
        _data = nullptr;
    }

    if (_fd != -1)
    {
        ::close(_fd);
        _fd = -1;
    }
}

}
