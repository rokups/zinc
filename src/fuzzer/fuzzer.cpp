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
#include <zinc/zinc.h>
#include <random>
#include <signal.h>
#include <cstring>
#include <assert.h>
#include <thread>
#include "Utilities.hpp"


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

ByteArray get_random_array(int64_t length)
{
    ByteArray result;
    result.resize(static_cast<unsigned long>(length));
    for (auto i = 0; i < length; i++)
        result[i] = random<uint8_t>(' ', '~');
    return result;
}

ByteArray mix_array(const ByteArray& source, int amount)
{
    ByteArray result = source;
    while ((amount--) != 0)
    {
        auto offset_start = random<size_t>(0, source.size() - 1);
        auto move_len = random<size_t>(1, (source.size() - offset_start));
        int move_delta = random(-(int)offset_start, (int)(source.size() - offset_start - move_len));
        memmove(&result.front() + offset_start + move_delta, &result.front() + offset_start, move_len);
    }
    return result;
}

int main()
{
    signal(SIGINT, &sigint_handler);

    for (;!stop;)
    {
        fprintf(stderr, "----------------------------------------\n");
        int64_t local_data_size = random<size_t>(10, 50);
        int64_t remote_data_size = std::max<int64_t>(local_data_size + random(-20, 20), 2);
        auto block_size = random<size_t>(5, 10);
        ByteArray local_data = get_random_array(local_data_size);
        ByteArray local_data_copy = local_data;
        ByteArray remote_data = local_data;
        remote_data.resize(static_cast<unsigned long>(remote_data_size));
        remote_data = mix_array(remote_data, random(1, 5));

        auto hashes = zinc::get_block_checksums(&remote_data.front(), remote_data_size, block_size,
                                                std::thread::hardware_concurrency())->wait()->result();

        // Memory block must be multiple of block_size when calling get_differences_delta()
        local_data.resize(static_cast<unsigned long>(round_up_to_multiple(local_data_size, block_size)));
        auto delta = zinc::get_differences_delta(&local_data.front(), local_data.size(), block_size, hashes);

        // Memory block must be multiple of block_size and big enough to accomodate new data.
        local_data.resize(static_cast<unsigned long>(round_up_to_multiple(std::max(local_data.size(), remote_data.size()), block_size)));
        zinc::patch_file(&local_data.front(), local_data.size(), block_size, delta, [&](size_t block_index, size_t block_size_) {
            ByteArray result;
            auto offset = block_index * block_size_;
            auto current_block_size = std::min(remote_data_size - offset, block_size_);
            result.resize(current_block_size);
            memcpy(&result.front(), &remote_data.front() + offset, current_block_size);
            return result;
        });
        local_data.resize(static_cast<unsigned long>(remote_data_size));
        if (local_data != remote_data)
        {
            local_data_copy.push_back(0);
            remote_data.push_back(0);
            local_data.push_back(0);
            printf("Local  data: %s\n", &local_data_copy.front());
            printf("Remote data: %s\n", &remote_data.front());
            printf("Result data: %s\n", &local_data.front());
            printf("Block  size: %d\n", (int)block_size);
            assert(local_data == remote_data);
        }
        usleep(15000);
    }
}
