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
#define BOOST_TEST_MODULE "TestDataSync"


#include <fstream>
#include "common.h"
#include "Utilities.hpp"


TEST_CASE ("Identical")
{
    REQUIRE(data_sync_test("abcdefghijklmnopqrstuvwxyz0123456789", "abcdefghijklmnopqrstuvwxyz0123456789", 5));
}

TEST_CASE ("BlocksSwapped")
{
    REQUIRE(data_sync_test("abcdefghijklmno34567pqrstuvwxyz01289", "abcdefghijklmnopqrstuvwxyz0123456789", 5));
}

TEST_CASE ("EndAdd")
{
    REQUIRE(data_sync_test("abcdefghijklmnopqrstuvwxyz0123456789_NEW_DATA", "abcdefghijklmnopqrstuvwxyz0123456789", 5));
}

TEST_CASE ("EndRemove")
{
    REQUIRE(data_sync_test("abcdefghijklmnopqrstuvwxyz0123456789", "abcdefghijklmnopqrstuvwxyz0123456789_NEW_DATA", 5));
}

TEST_CASE ("FrontAdd1")
{
    REQUIRE(data_sync_test("NEW_DATA_abcdefghijklmnopqrstuvwxyz0123456789", "abcdefghijklmnopqrstuvwxyz0123456789", 5));
}

TEST_CASE ("FrontAdd2")
{
    REQUIRE(data_sync_test("_abcdefghijklmnopqrstuvwxyz0123456789", "abcdefghijklmnopqrstuvwxyz0123456789", 5));
}

TEST_CASE ("FrontRemove")
{
    REQUIRE(data_sync_test("abcdefghijklmnopqrstuvwxyz0123456789", "NEW_DATA_abcdefghijklmnopqrstuvwxyz0123456789", 5));
}

TEST_CASE ("Shuffle")
{
    REQUIRE(data_sync_test("abcdefghijklmnopqrstuvwxyz0123456789", "abcdefghrstuvwxyz0123ijklmnopq456789", 5));
}

TEST_CASE ("UseExistingData")
{
    REQUIRE(data_sync_test("12345123452222212345", "00000111112222212345", 5));
}

TEST_CASE ("RefCachedBlockTwice")
{
    REQUIRE(data_sync_test("defg defg 9abc 0000 ", "1234 5678 9abc defg ", 5));
}

TEST_CASE ("SyncFiles")
{
    const char* file_remote = "/tmp/.zinc_test_remote";
    const char* file_local = "/tmp/.zinc_test_local";
    std::ofstream fo;
    fo.open(file_remote);
    fo << "abcdefghijklmnopqrstuvwxyz0123456789";
    fo.close();

    fo.open(file_local);
    fo << "abcdefghrstuvwxyz0123ijklmnopq456789";
    fo.close();

    auto get_data = [=](size_t block_index, size_t block_size) -> zinc::ByteArray
    {
        zinc::ByteArray result;
        result.resize(block_size);
        std::ifstream fi;
        fi.open(file_remote);
        fi.seekg(block_index * block_size, std::ios_base::beg);
        fi.read((char*)&result.front(), block_size);
        return result;
    };

    auto hashes = zinc::get_block_checksums(file_remote, 5);
    REQUIRE(hashes.size() > 0);
    auto delta = zinc::get_differences_delta(file_local, 5, hashes);
    REQUIRE(delta.map.size() > 0);
    REQUIRE(zinc::patch_file(file_local, zinc::get_file_size(file_remote), 5, delta, get_data) == true);

    zinc::ByteArray remote_data;
    remote_data.resize(zinc::get_file_size(file_remote));

    zinc::ByteArray local_data;
    local_data.resize(zinc::get_file_size(file_local));

    std::ifstream fi;
    fi.open(file_remote);
    fi.read((char*)&remote_data.front(), remote_data.size());
    fi.close();

    fi.open(file_local);
    fi.read((char*)&local_data.front(), local_data.size());
    fi.close();

    REQUIRE(remote_data == local_data);
}


TEST_CASE ("RefCachedBlockTwice2")
{
    REQUIRE(data_sync_test("`pO6Vqe8*+w0,M^upV$}mHKmCy`_3R#3n:|)N.Va%t_'7g*^/;1ghO6Vqe8*+w0,M^upV$}mHKmCy`_3R#3n:|)N.Va%t_'7g*^/;1gh}0Bt[ub(oRp5>uEY!%z6R8C<Bh:HpQl.\\R",
                           "zJi[=zYhQ4<,1SyKr=>G0)<(P(YUv[nx\" C-f,IJPD`r`pO6Vqe8*+w0,M^upV$}mHKmCy`_3R#3n:|)N.Va%t_'7g*^/;1gh}0Bt[ub(oRp5>uEY!%z6R8C<Bh:HpQlqQpiamP.\\R&", 17));
}

TEST_CASE ("FuzzTest1")
{
    REQUIRE(data_sync_test(",<*7Dxk:%\\7CL]R^J^#Da'hw<8Z;%=0Q7%1/mcMIeHx*VDEu48mWWaB4V\\)llLxfjsR=!YT,kLbTjWEd&[}xCb;jdu/05m\"5DD%iPevf6T#^HgIs4`R]WU437e\"oB#O#&dwSF4H3i>3/njSJYK6@HB'VziPabjbTQ[\"]Y%yQHEj=#^HgIs4`R]WU\"oB#O#&dwSF4H`1Qj;VigiO!8Jc$2`-EwRs-vq4Sokl8;MiMT@",
                           ",<*7Dxk:%\\7CL]R^ NL_6!$ZC7:J^#Da'hw<8Z;%=0Q7%1/mcMIeHx*VDEu48mWWaB4V\\)llLxfjsR=!YT,kLbTjWEd&[}xCb;jdu/05m\"5DD%iPevf6TH:,5/e>kLQ[;Sq<hd53i>3/njSJYK6@HB'VziPabjbTQ[\"]Y%yQHEj=#^HgIs4`R]WU437e\"oB#O#&dwSF4H`1Qj;VigiO!8Jc$2`-EwRs-vq4Sokl8;MiMT@p", 18));
}

TEST_CASE ("FuzzTest2")
{
    REQUIRE(data_sync_test(",hI|J@Q\\so}:6f=_yoy\\so}:6f=_\\so}:6f=_yo", "}:6f=_yoyL?k,hI|J@Q\\soOsD;E}CvfC]OS!G5", 5));
}

TEST_CASE ("IdenticalBlockDownload")
{
    REQUIRE(data_sync_test("1234_1234_000001234_", "00000000000000000000", 5));
}
