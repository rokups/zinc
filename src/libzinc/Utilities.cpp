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
#include <sys/stat.h>
#if _WIN32
#   include <windows.h>
#endif


namespace zinc
{

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

}
