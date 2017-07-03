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
#include "zinc/StrongHash.h"
#include <string.h>
#include "crypto/fnv1a.h"
#if ZINC_WITH_STRONG_HASH_SHA1
#   include "crypto/sha1.h"
#endif

namespace zinc
{

StrongHash::StrongHash()
{
    memset(_data, 0, sizeof(_data));
}

StrongHash::StrongHash(const void* m, size_t mlen)
{
#if ZINC_WITH_STRONG_HASH_FNV
    auto hash = fnv1a64(m, mlen);
    static_assert(sizeof(hash) == sizeof(_data));
    *(uint64_t*)_data = hash;
#else
    sha1_ctxt sha1;
    sha1_init(&sha1);
    sha1_loop(&sha1, (const uint8_t*)m, mlen);
    sha1_result(&sha1, _data);
#endif
}

StrongHash::StrongHash(const StrongHash& other)
{
    memcpy(_data, other._data, sizeof(_data));
}

StrongHash& StrongHash::operator=(const StrongHash& other)
{
    memcpy(_data, other._data, sizeof(_data));
    return *this;
}

bool StrongHash::operator==(const StrongHash& other) const
{
    return memcmp(_data, other._data, sizeof(_data)) == 0;
}

std::string StrongHash::to_string() const
{
    std::string result;
    result.resize(sizeof(_data) * 2);
    for (size_t i = 0; i < sizeof(_data); i++)
        sprintf(&result.front() + i * 2, "%02x", _data[i]);
    return result;
}

StrongHash::StrongHash(const std::string& str)
{
    if (str.length() < (sizeof(_data) * 2))
        return;

    char buf[3];
    for (size_t i = 0; i < sizeof(_data); i++)
    {
        memcpy(buf, &str.front() + i * 2, 2);
        buf[2] = 0;
        _data[i] = (uint8_t)strtol(buf, 0, 16);
    }
}

}
