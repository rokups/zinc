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
#include <cstring>
#include "zinc.h"
#include "Utilities.hpp"
using namespace std::placeholders;
using namespace zinc;

#if RR_DEBUG
void RR_LOG(const char* format, ...);
#endif

void print_help()
{
    printf(
        "zinc usage:\n"
        "    -h           Hash remote file\n"
        "    -p hashes    Patch local file\n"
        "    source       Source file\n"
        "    destination  Destination file\n"
    );
}

#include <math.h>
#include <sys/time.h>
double get_time_ms()
{
    struct timeval  tv;
    gettimeofday(&tv, NULL);

    return (tv.tv_sec) * 1000.0 + (tv.tv_usec) / 1000.0;
}

int main(int argc, char* argv[])
{
    int32_t c;
    const char* f_source = 0;
    const char* f_destination = 0;
    const char* p_hash_file = 0;
    enum
    {
        ACTION_NONE,
        ACTION_HASH,
        ACTION_PATCH,
        ACTION_TEST
    } action = ACTION_NONE;
    while ((c = getopt(argc, argv, "htp:")) != -1)
    {
        switch (c)
        {
            case 'h':
                if (action != ACTION_NONE)
                {
                    print_help();
                    printf("-h and -p are mutually exclusive.\n");
                    return -1;
                }
                action = ACTION_HASH;
                break;
            case 'p':
                if (action != ACTION_NONE)
                {
                    print_help();
                    printf("-h and -p are mutually exclusive.\n");
                    return -1;
                }
                action = ACTION_PATCH;
                p_hash_file = optarg;
                break;
            case 't':
                action = ACTION_TEST;
                break;
            default:
                break;
        }
    }

    if (argc > optind)
        f_source = argv[optind++];
    else
    {
        print_help();
        printf("Source not provided\n");
        return -1;
    }
    if (argc > optind)
        f_destination = argv[optind++];
    else
    {
        print_help();
        printf("Destination not provided\n");
        return -1;
    }

    if (action == ACTION_NONE)
    {
        print_help();
        printf("Action is not selected. Provide -h or -p options.\n");
        return -1;
    }


    auto read_data = [&](size_t block_index, size_t block_size, void* user_data) -> ByteArray
    {
        ByteArray result;
        FileMapping* file_data = (FileMapping*)user_data;
        auto current_offset = block_index * block_size;
        auto max_size = std::min(block_size, file_data->get_size() - current_offset);
        if (max_size > 0)
        {
            result.resize(max_size);
            auto d = (uint8_t*)file_data->get_data();
            memcpy(&result.front(), &d[current_offset], max_size);
        }
        return result;
    };

    const size_t block_size = 4096;
    if (action == ACTION_HASH)
    {
        auto hashes = get_block_checksums(f_source, block_size);
        auto fp = fopen(f_destination, "w+b");
        for (auto h : hashes)
        {
            fwrite(&h.weak, 1, sizeof(h.weak), fp);
            fwrite(&h.strong, 1, sizeof(h.strong), fp);
        }
        fclose(fp);
    }
    else if (action == ACTION_PATCH)
    {
        RemoteFileHashList hashes;
        auto fp = fopen(p_hash_file, "rb");
        fseek(fp, 0, SEEK_END);
        auto file_size = ftell(fp);
        rewind(fp);
        hashes.reserve(file_size / (sizeof(BlockHashes::weak) + sizeof(BlockHashes::strong)));
        uint32_t index = 0;
        for (;;)
        {
            BlockHashes h;
            h.index = index++;
            if (fread(&h.weak, 1, sizeof(h.weak), fp) != sizeof(h.weak))
                break;
            if (fread(&h.strong, 1, sizeof(h.strong), fp) != sizeof(h.strong))
                break;
            hashes.push_back(h);
        }

        auto patch_start = get_time_ms();

        auto delta = get_differences_delta(f_destination, block_size, hashes);
        FileMapping mapping;
        if (!mapping.open(f_source))
        {
            printf("Could not open file '%s' mapping", f_source);
            return -1;
        }

        patch_file(f_destination, block_size, delta, std::bind(read_data, _1, _2, &mapping));

        auto patch_end = get_time_ms();
        auto patch_time = patch_end - patch_start;

        printf("Finished patching file in %d.%d seconds\n", (int)(patch_time / 1000.0), (int)(fmod(patch_time, 1000.0)));

        // Trim destination
        {
            struct stat st = { };
            stat(f_source, &st);
            if (truncate(f_destination, st.st_size) != 0)
            {
                printf("Could not truncate file '%s'", f_destination);
                return -1;
            }
        }
    }
    else if (action == ACTION_TEST)
    {
        auto hashes = get_block_checksums(f_source, block_size);
        auto patch_start = get_time_ms();
        auto delta = get_differences_delta(f_destination, block_size, hashes);
        FileMapping mapping;
        if (!mapping.open(f_source))
        {
            printf("Could not open file '%s' mapping", f_source);
            return -1;
        }
        auto patch_time = get_time_ms() - patch_start;
        printf("Finished calculating delta file in %d.%d seconds\n", (int)(patch_time / 1000.0), (int)(fmod(patch_time, 1000.0)));
        patch_start = get_time_ms();
        patch_file(f_destination, block_size, delta, std::bind(read_data, _1, _2, &mapping));
        patch_time = get_time_ms() - patch_start;
        printf("Finished patching file in %d.%d seconds\n", (int)(patch_time / 1000.0), (int)(fmod(patch_time, 1000.0)));

        // Trim destination
        {
            struct stat st = { };
            stat(f_source, &st);
            if (truncate(f_destination, st.st_size) != 0)
            {
                printf("Could not truncate file '%s'", f_destination);
                return -1;
            }
        }
    }

    return 0;
}
