#define BOOST_TEST_MODULE "FrontAdd"
#include "common.h"


BOOST_AUTO_TEST_CASE (FrontAdd)
{
    // New data in the front
    // TODO: NEW_DATA_ length not being multiple of block size causes last block redownload
    BOOST_CHECK(data_sync_test("NEW_DATA_abcdefghijklmnopqrstuvwxyz0123456789", "abcdefghijklmnopqrstuvwxyz0123456789", 5));
//    BOOST_CHECK(data_sync_test("_abcdefghijklmnopqrstuvwxyz0123456789", "abcdefghijklmnopqrstuvwxyz0123456789", 5));
}
