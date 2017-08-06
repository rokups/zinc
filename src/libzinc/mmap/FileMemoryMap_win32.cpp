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
#include <windows.h>

namespace zinc
{

bool FileMemoryMap::open(const char* file_path)
{
    _fd = CreateFile(file_path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, 0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                     0);
    if (!_fd || _fd == INVALID_HANDLE_VALUE)
    {
        zinc_error<std::system_error>("FileMapping could not open file.", GetLastError());
        return false;
    }

    LARGE_INTEGER file_size = {};
    if (!GetFileSizeEx(_fd, &file_size))
    {
        zinc_error<std::system_error>("FileMapping could not get file size.", GetLastError());
        return false;
    }
    _size = file_size.QuadPart;

    _mapping = CreateFileMapping(_fd, 0, PAGE_READWRITE, file_size.HighPart, file_size.LowPart, 0);
    if (!_mapping || _mapping == INVALID_HANDLE_VALUE)
    {
        zinc_error<std::system_error>("FileMapping create file mapping.", GetLastError());
        return false;
    }

    _data = MapViewOfFile(_mapping, FILE_MAP_WRITE, 0, 0, _size);
    if (!is_open())
    {
        zinc_error<std::system_error>("FileMapping could not get map file data.", GetLastError());
        return false;
    }
    return true;
}

void FileMemoryMap::close()
{
    if (is_open())
    {
        UnmapViewOfFile(_data);
        _data = 0;
    }
    if (_mapping && _mapping != INVALID_HANDLE_VALUE)
    {
        CloseHandle(_mapping);
        _mapping = INVALID_HANDLE_VALUE;
    }
    if (_fd && _fd != INVALID_HANDLE_VALUE)
    {
        CloseHandle(_fd);
        _fd = INVALID_HANDLE_VALUE;
    }
}

}
