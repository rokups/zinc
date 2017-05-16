#define BOOST_TEST_MODULE "TestDataSync"
#include "common.h"


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
