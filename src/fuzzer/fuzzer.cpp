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
#include <stddef.h>
#include <stdint.h>
#include <zinc.h>
#include <random>
#include <signal.h>
#include <cstring>
#include <assert.h>


using namespace zinc;

bool stop = false;
std::uniform_int_distribution<unsigned int> dist(0, 0xFFFFFFFF);
std::random_device urandom("/dev/urandom");

void sigint_handler(int)
{
    stop = true;
}

template<typename T=int>
T random(T min, T max)
{
    return (dist(urandom) % (max - min)) + min;
}

ByteArray get_random_array(size_t length)
{
    ByteArray result;
    result.resize(length);
    for (auto i = 0; i < length; i++)
        result[i] = (uint8_t)random(0, 0xFF);
    return result;
}

ByteArray mix_array(const ByteArray& source, int amount)
{
    ByteArray result = source;
    while (amount--)
    {
        size_t offset_start = random<size_t>(0, source.size() - 1);
        size_t move_len = random<size_t>(1, (source.size() - offset_start));
        int move_delta = random(-(int)offset_start, (int)(source.size() - offset_start - move_len));
        memmove(&result.front() + offset_start + move_delta, &result.front() + offset_start, move_len);
    }
    return result;
}

size_t next_multiple_of(size_t n, size_t mul)
{
    if (n % mul)
        return n + mul - (n % mul);
    return n;
}

int main()
{
    signal(SIGINT, &sigint_handler);

    for (;!stop;)
    {
        size_t local_data_size = random<size_t>(0x100, 0x1000);
        size_t remote_data_size = local_data_size + random(-0x50, 0x50);
        size_t block_size = random<size_t>(0x10, 0x100);
        auto local_data = get_random_array(local_data_size);
        auto remote_data = mix_array(local_data, random(1, 0x20));
        remote_data.resize(remote_data_size);

        auto hash_list = zinc::get_block_checksums_mem(&remote_data.front(), remote_data_size, block_size);

        // Memory block must be multiple of block_size when calling get_differences_delta_mem()
        local_data.resize(next_multiple_of(local_data_size, block_size));
        auto delta = zinc::get_differences_delta_mem(&local_data.front(), local_data.size(), block_size, hash_list);

        // Memory block must be multiple of block_size and big enough to accomodate new data.
        local_data.resize(next_multiple_of(std::max(local_data.size(), remote_data.size()), block_size));
        zinc::patch_file_mem(&local_data.front(), local_data.size(), block_size, delta, [&](size_t block_index, size_t block_size) {
            ByteArray result;
            result.resize(block_size);
            memcpy(&result.front(), &remote_data.front() + (block_index * block_size), block_size);
            return result;
        });
        local_data.resize(remote_data_size);
        assert(local_data == remote_data);
    }
}
