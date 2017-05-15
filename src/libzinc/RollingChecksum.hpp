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

namespace zinc
{

class RollingChecksum
{
    uint32_t _a;
    uint32_t _b;
    size_t _count;

public:
    inline RollingChecksum(const void* data = 0, size_t dlen = 0)
		: _a(0), _b(0), _count(0)
    {
        update(data, dlen);
    }

    void update(const void* data, size_t dlen)
    {
        if (!data || !dlen)
            return;
        _count = dlen;
        uint8_t* d = (uint8_t*)data;
        for (size_t i = 0; i < dlen; i++)
        {
            auto byte = d[i];
            _a += byte;
            _b += (dlen - i) * byte;
        }
    }

    inline uint32_t digest()
    {
        return (_b << 16) | _a;
    }

    inline void rotate(uint8_t out, uint8_t in)
    {
        _a -= out - in;
        _b -= out * _count - _a;
    }

    inline void clear()
    {
        _a = 0;
        _b = 0;
        _count = 0;
    }

    inline bool isEmpty() const { return _count == 0; }
};

}
