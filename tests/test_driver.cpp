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


BOOST_AUTO_TEST_CASE (Identical)
{
    BOOST_CHECK(data_sync_test("abcdefghijklmnopqrstuvwxyz0123456789", "abcdefghijklmnopqrstuvwxyz0123456789", 5));
}

BOOST_AUTO_TEST_CASE (BlocksSwapped)
{
    BOOST_CHECK(data_sync_test("abcdefghijklmno34567pqrstuvwxyz01289", "abcdefghijklmnopqrstuvwxyz0123456789", 5));
}

BOOST_AUTO_TEST_CASE (EndAdd)
{
    BOOST_CHECK(data_sync_test("abcdefghijklmnopqrstuvwxyz0123456789_NEW_DATA", "abcdefghijklmnopqrstuvwxyz0123456789", 5));
}

BOOST_AUTO_TEST_CASE (EndRemove)
{
    BOOST_CHECK(data_sync_test("abcdefghijklmnopqrstuvwxyz0123456789", "abcdefghijklmnopqrstuvwxyz0123456789_NEW_DATA", 5));
}

BOOST_AUTO_TEST_CASE (FrontAdd1)
{
    BOOST_CHECK(data_sync_test("NEW_DATA_abcdefghijklmnopqrstuvwxyz0123456789", "abcdefghijklmnopqrstuvwxyz0123456789", 5));
}

BOOST_AUTO_TEST_CASE (FrontAdd2)
{
    BOOST_CHECK(data_sync_test("_abcdefghijklmnopqrstuvwxyz0123456789", "abcdefghijklmnopqrstuvwxyz0123456789", 5));
}

BOOST_AUTO_TEST_CASE (FrontRemove)
{
    BOOST_CHECK(data_sync_test("abcdefghijklmnopqrstuvwxyz0123456789", "NEW_DATA_abcdefghijklmnopqrstuvwxyz0123456789", 5));
}

BOOST_AUTO_TEST_CASE (Shuffle)
{
    BOOST_CHECK(data_sync_test("abcdefghijklmnopqrstuvwxyz0123456789", "abcdefghrstuvwxyz0123ijklmnopq456789", 5));
}

BOOST_AUTO_TEST_CASE (UseExistingData)
{
    BOOST_CHECK(data_sync_test("12345123452222212345", "00000111112222212345", 5));
}

BOOST_AUTO_TEST_CASE (RefCachedBlockTwice)
{
    BOOST_CHECK(data_sync_test("defg defg 9abc 0000 ", "1234 5678 9abc defg ", 5));
}

BOOST_AUTO_TEST_CASE (SyncFiles)
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
    BOOST_CHECK(hashes.size() > 0);
    auto delta = zinc::get_differences_delta(file_local, 5, hashes);
    BOOST_CHECK(delta.size() > 0);
    BOOST_CHECK(zinc::patch_file(file_local, zinc::get_file_size(file_remote), 5, delta, get_data) == true);

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

    BOOST_CHECK(remote_data == local_data);
}


BOOST_AUTO_TEST_CASE (RefCachedBlockTwice2)
{
    BOOST_CHECK(data_sync_test("`pO6Vqe8*+w0,M^upV$}mHKmCy`_3R#3n:|)N.Va%t_'7g*^/;1ghO6Vqe8*+w0,M^upV$}mHKmCy`_3R#3n:|)N.Va%t_'7g*^/;1gh}0Bt[ub(oRp5>uEY!%z6R8C<Bh:HpQl.\\R",
                               "zJi[=zYhQ4<,1SyKr=>G0)<(P(YUv[nx\" C-f,IJPD`r`pO6Vqe8*+w0,M^upV$}mHKmCy`_3R#3n:|)N.Va%t_'7g*^/;1gh}0Bt[ub(oRp5>uEY!%z6R8C<Bh:HpQlqQpiamP.\\R&", 17));
}

BOOST_AUTO_TEST_CASE (FuzzTest1)
{
    BOOST_CHECK(data_sync_test(",<*7Dxk:%\\7CL]R^J^#Da'hw<8Z;%=0Q7%1/mcMIeHx*VDEu48mWWaB4V\\)llLxfjsR=!YT,kLbTjWEd&[}xCb;jdu/05m\"5DD%iPevf6T#^HgIs4`R]WU437e\"oB#O#&dwSF4H3i>3/njSJYK6@HB'VziPabjbTQ[\"]Y%yQHEj=#^HgIs4`R]WU\"oB#O#&dwSF4H`1Qj;VigiO!8Jc$2`-EwRs-vq4Sokl8;MiMT@",
                               ",<*7Dxk:%\\7CL]R^ NL_6!$ZC7:J^#Da'hw<8Z;%=0Q7%1/mcMIeHx*VDEu48mWWaB4V\\)llLxfjsR=!YT,kLbTjWEd&[}xCb;jdu/05m\"5DD%iPevf6TH:,5/e>kLQ[;Sq<hd53i>3/njSJYK6@HB'VziPabjbTQ[\"]Y%yQHEj=#^HgIs4`R]WU437e\"oB#O#&dwSF4H`1Qj;VigiO!8Jc$2`-EwRs-vq4Sokl8;MiMT@p", 18));
}
