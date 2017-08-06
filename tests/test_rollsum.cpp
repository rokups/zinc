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
#include <fstream>
#include "common.h"
#include "RollingChecksum.hpp"


TEST_CASE ("ChunkChecksum")
{
    char data[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    zinc::RollingChecksum sum(data, strlen(data));
    auto result = sum.digest();
    REQUIRE(result == 0x0A970D2C);
}

TEST_CASE ("ByteRolling")
{
    char data1[] = "abcdefghijklmnopqrstuvwxyz012345678";
    char data2[] = "bcdefghijklmnopqrstuvwxyz0123456789";
    zinc::RollingChecksum sum(data1, strlen(data1));
    sum.rotate('a', '9');
    REQUIRE(zinc::RollingChecksum(data2, strlen(data2)).digest() == sum.digest());
}

TEST_CASE ("RollingInAllBytes")
{
    char data[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    auto expect_digest = zinc::RollingChecksum(data, strlen(data)).digest();

    zinc::RollingChecksum sum;
    for (auto i = 0; i < strlen(data); i++)
        sum.rotate(0, (uint8_t)data[i]);

    auto rotated_digest = sum.digest();

    REQUIRE(expect_digest == rotated_digest);
}
