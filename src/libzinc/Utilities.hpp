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


#include <cstdint>
#include <cstring>
#include <string>
#include "zinc/zinc.h"


namespace zinc
{

#if _WIN32
std::wstring to_wstring(const std::string& str);
int truncate(const char* file_path, int64_t file_size);
#endif
int64_t round_up_to_multiple(int64_t value, int64_t multiple_of);
int64_t get_file_size(const char* file_path);
int touch(const char* file_path);
StrongHash strong_hash(const void* m, size_t mlen);
std::vector<uint8_t> string_to_bytes(const std::string& str);
std::string bytes_to_string(const uint8_t* bytes, size_t blen);
template<typename T>
std::string bytes_to_string(const T& container)
{
    return bytes_to_string(&container.front(), container.size());
}

};
