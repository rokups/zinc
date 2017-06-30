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
#include "Utilities.hpp"
#if ZINC_WITH_EXCEPTIONS
#   include <system_error>
#endif

namespace zinc
{

FileMemoryMap::FileMemoryMap()
{
#if _WIN32
    _fd = INVALID_HANDLE_VALUE;
    _mapping = INVALID_HANDLE_VALUE;
#else
    _fd = -1;
#endif
    _size = 0;
    _data = 0;
}

#if _WIN32

bool FileMemoryMap::open(const char* file_path)
{
    _fd = CreateFile(file_path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, 0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                     0);
    if (!_fd || _fd == INVALID_HANDLE_VALUE)
#if ZINC_WITH_EXCEPTIONS
        throw std::system_error(GetLastError(), std::system_category(), "FileMapping could not open file.");
#else
        return false;
#endif

    LARGE_INTEGER file_size = {};
    if (!GetFileSizeEx(_fd, &file_size))
    {
#if ZINC_WITH_EXCEPTIONS
        throw std::system_error(errno, std::system_category(), "FileMapping could not get file size.");
#else
        return false;
#endif
    }
    _size = file_size.QuadPart;

    _mapping = CreateFileMapping(_fd, 0, PAGE_READWRITE, file_size.HighPart, file_size.LowPart, 0);
    if (!_mapping || _mapping == INVALID_HANDLE_VALUE)
    {
#if ZINC_WITH_EXCEPTIONS
        throw std::system_error(GetLastError(), std::system_category(), "FileMapping create file mapping.");
#else
        return false;
#endif
    }

    _data = MapViewOfFile(_mapping, FILE_MAP_WRITE, 0, 0, _size);
    if (!is_open())
    {
#if ZINC_WITH_EXCEPTIONS
        throw std::system_error(GetLastError(), std::system_category(), "FileMapping could not get map file data.");
#else
        return false;
#endif
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

#else
bool FileMemoryMap::open(const char* file_path)
{
    struct stat st = {};
    _fd = ::open(file_path, O_RDWR);

    if (_fd == -1)
    {
#if ZINC_WITH_EXCEPTIONS
        throw std::system_error(errno, std::system_category(), "FileMapping could not open file.");
#else
        return false;
#endif
    }

    if (fstat(_fd, &st) == -1)
    {
#if ZINC_WITH_EXCEPTIONS
        throw std::system_error(errno, std::system_category(), "FileMapping could not get file size.");
#else
        return false;
#endif
    }

    _size = st.st_size;

    _data = mmap(0, _size, PROT_READ | PROT_WRITE, MAP_SHARED, _fd, 0);
    if (!is_open())
    {
#if ZINC_WITH_EXCEPTIONS
        throw std::system_error(errno, std::system_category(), "FileMapping could not get map file data.");
#else
        return false;
#endif
    }

    return true;
}
/// Close memory mapping.
void FileMemoryMap::close()
{
    if (is_open())
    {
        munmap(_data, _size);
        _data = 0;
    }

    if (_fd != -1)
    {
        ::close(_fd);
        _fd = -1;
    }
}
#endif

#if _WIN32
std::wstring to_wstring(const std::string &str)
{
    std::wstring result;
    auto needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), str.length(), 0, 0);
    if (needed > 0)
    {
        result.resize(needed);
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), str.length(), &result.front(), needed);
    }
    return result;
}

int truncate(const char *file_path, int64_t file_size)
{
    HANDLE hFile = CreateFileW(to_wstring(file_path).c_str(), GENERIC_WRITE, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (!hFile || hFile == INVALID_HANDLE_VALUE)
        return (int)GetLastError();

    LARGE_INTEGER distance;
    distance.QuadPart = file_size;
    if (SetFilePointerEx(hFile, distance, 0, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
    {
        CloseHandle(hFile);
        return INVALID_SET_FILE_POINTER;
    }

    if (!SetEndOfFile(hFile))
    {
        auto error = GetLastError();
        CloseHandle(hFile);
        return (int)error;
    }

    CloseHandle(hFile);
    return ERROR_SUCCESS;
}
#endif

int64_t round_up_to_multiple(int64_t value, int64_t multiple_of)
{
    auto remainder = value % multiple_of;
    if (value && remainder)
        value += multiple_of - remainder;
    return value;
}

int64_t get_file_size(const char* file_path)
{
#if _WIN32
    struct _stat32i64 st = {};
    if (_wstat32i64(to_wstring(file_path).c_str(), &st) == 0)
        return st.st_size;
#else
    struct stat64 st = {};
    if (stat64(file_path, &st) == 0)
        return st.st_size;
#endif
    return -1;
}

int touch(const char* file_path)
{
#if _WIN32
    if (auto fp = _wfopen(to_wstring(file_path).c_str(), L"a+"))
#else
    if (auto fp = fopen(file_path, "a+"))
#endif
    {
        fclose(fp);
        return 0;
    }

    return -1;
}

uint64_t fnv1a64(const void* data, size_t dlen)
{
    auto p = (uint8_t*)data;
    uint64_t hash = 0xcbf29ce484222325;

    for (size_t i = 0; i < dlen; i++)
    {
        hash = hash ^ p[i];
//        hash = hash * 0x100000001b3;
        hash += (hash << 1) + (hash << 4) + (hash << 5) +
                (hash << 7) + (hash << 8) + (hash << 40);
    }
    return hash;
}

}
