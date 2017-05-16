#define BOOST_TEST_MODULE "Shuffle"
#include "common.h"


BOOST_AUTO_TEST_CASE (Shuffle)
{
    // Local data moved around
    BOOST_CHECK(data_sync_test("abcdefghijklmnopqrstuvwxyz0123456789", "abcdefghrstuvwxyz0123ijklmnopq456789", 5));
}
