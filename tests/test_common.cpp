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
#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE "CommonTests"
#include <boost/test/unit_test.hpp>
#include <zinc.h>
#include <cstdlib>
using namespace std::placeholders;
using namespace zinc;

const auto block_size = 5;

ByteArray string_to_array(const char* string)
{
    auto len = strlen(string);
    ByteArray result(len);
    memcpy(&result.front(), string, len);
    return result;
}

ByteArray get_data(size_t block_index, size_t block_size, void* user_data)
{
    ByteArray& source = *(ByteArray*)user_data;
    ByteArray result(std::min(block_size, source.size() - (block_index * block_size)));
    memcpy(&result.front(), &source[block_index * block_size], result.size());
    return result;
}

bool data_sync_test(const char* remote, const char* local)
{
    ByteArray data_remote = string_to_array(remote);
    ByteArray data_local = string_to_array(local);

    // Ensure local data has enough bytes for remote data
    auto local_file_size = std::max(data_local.size(), data_remote.size());
    if (auto remainder = local_file_size % block_size)
        local_file_size += block_size - remainder;
    data_local.resize(local_file_size, 0);

    auto checksums = get_block_checksums_mem(&data_remote.front(), data_remote.size(), block_size);
    auto delta = get_differences_delta_mem(&data_local.front(), data_local.size(), block_size, checksums);
    patch_file_mem(&data_local.front(), data_local.size(), block_size, delta, std::bind(get_data, _1, _2, &data_remote));

    // Ensure local data does not have more bytes than remote data
    if (data_local.size() > data_remote.size())
        data_local.resize(data_remote.size());

    return data_local == data_remote;
}

BOOST_AUTO_TEST_CASE (common_tests)
{
    // Identical data
    BOOST_CHECK(data_sync_test("abcdefghijklmnopqrstuvwxyz0123456789", "abcdefghijklmnopqrstuvwxyz0123456789"));

    // New data in the front
    // TODO: NEW_DATA_ length not being multiple of block size causes last block redownload
    BOOST_CHECK(data_sync_test("NEW_DATA_abcdefghijklmnopqrstuvwxyz0123456789", "abcdefghijklmnopqrstuvwxyz0123456789"));
    BOOST_CHECK(data_sync_test("_abcdefghijklmnopqrstuvwxyz0123456789", "abcdefghijklmnopqrstuvwxyz0123456789"));

    // New data at the end
    BOOST_CHECK(data_sync_test("abcdefghijklmnopqrstuvwxyz0123456789_NEW_DATA", "abcdefghijklmnopqrstuvwxyz0123456789"));

    // Data removed from the end
    BOOST_CHECK(data_sync_test("abcdefghijklmnopqrstuvwxyz0123456789", "abcdefghijklmnopqrstuvwxyz0123456789_NEW_DATA"));

    // Data removed from the front
    BOOST_CHECK(data_sync_test("abcdefghijklmnopqrstuvwxyz0123456789", "NEW_DATA_abcdefghijklmnopqrstuvwxyz0123456789"));

    // Remote data moved around
    BOOST_CHECK(data_sync_test("abcdefghijklvwxyz0123mnopqrstu456789", "abcdefghijklmnopqrstuvwxyz0123456789"));

    // Local data moved around
    BOOST_CHECK(data_sync_test("abcdefghijklmnopqrstuvwxyz0123456789", "abcdefghrstuvwxyz0123ijklmnopq456789"));
}
