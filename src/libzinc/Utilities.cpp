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
#if ZINC_WITH_STRONG_HASH_SHA1
#   include "crypto/sha1.h"
#else
#   include "crypto/fnv1a.h"
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
    std::wstring wfile_path = to_wstring(file_path);
    std::replace(wfile_path.begin(), wfile_path.end(), '/', '\\');
    HANDLE hFile = CreateFileW(wfile_path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (!hFile || hFile == INVALID_HANDLE_VALUE)
        return (int)GetLastError();

    LARGE_INTEGER distance;
    distance.QuadPart = file_size;
    if (!SetFilePointerEx(hFile, distance, 0, FILE_BEGIN))
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
    struct _stat64 st = {};
    if (_wstat64(to_wstring(file_path).c_str(), &st) == 0)
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

StrongHash strong_hash(const void* m, size_t mlen)
{
    StrongHash result{};
#if ZINC_WITH_STRONG_HASH_FNV
    auto hash = fnv1a64(m, mlen);
    static_assert(sizeof(hash) == sizeof(_data));
    memcpy(result.data(), &hash, sizeof(hash))
    static_assert(result.size() == sizeof(hash));
#else
    sha1_ctxt sha1{};
    sha1_init(&sha1);
    sha1_loop(&sha1, (const uint8_t*)m, mlen);
    sha1_result(&sha1, result.data());
    static_assert(result.size() == SHA1_RESULTLEN);
#endif
    return result;
}

std::vector<uint8_t> string_to_bytes(const std::string& str)
{
    std::vector<uint8_t> result{};

    if (str.length() != (result.size() * 2))
        return result;

    char buf[3];
    for (size_t i = 0; i < result.size(); i++)
    {
        memcpy(buf, &str.front() + i * 2, 2);
        buf[2] = 0;
        result[i] = (uint8_t)strtol(buf, nullptr, 16);
    }

    return result;
}

std::string bytes_to_string(const uint8_t* bytes, size_t blen)
{
    std::string result;
    result.resize(blen * 2);
    for (size_t i = 0; i < blen; i++)
        sprintf(&result.front() + i * 2, "%02x", bytes[i]);
    return result;
}

}
