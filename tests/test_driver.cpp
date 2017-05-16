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
