#define BOOST_TEST_MODULE "EndRemove"
#include "common.h"


BOOST_AUTO_TEST_CASE (EndRemove)
{
    // Data removed from the end
    BOOST_CHECK(data_sync_test("abcdefghijklmnopqrstuvwxyz0123456789", "abcdefghijklmnopqrstuvwxyz0123456789_NEW_DATA", 5));
}
