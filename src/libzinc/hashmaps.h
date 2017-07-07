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


#include <unordered_map>
#include "crypto/fnv1a.h"


namespace std
{

template <>
struct hash<zinc::StrongHash>
{
    std::size_t operator()(const zinc::StrongHash& strong) const
    {
        return (size_t)zinc::fnv1a64(strong.data(), strong.size());
    }
};

}

#if __cplusplus >= 201402L
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wshift-count-overflow"
#   include <flat_hash_map.hpp>
#   pragma GCC diagnostic pop
#   define ZINC_USE_SKA_FLAT_HASH_MAP 1

struct StrongHashHashFunction
{
    size_t operator()(const zinc::StrongHash& strong)
    {
        return (size_t)zinc::fnv1a64(strong.data(), strong.size());
    }

    typedef ska::power_of_two_hash_policy hash_policy;
};
#endif
