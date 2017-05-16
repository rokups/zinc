#define BOOST_TEST_MODULE "FrontRemove"
#include "common.h"


BOOST_AUTO_TEST_CASE (FrontRemove)
{
    // Data removed from the front
    BOOST_CHECK(data_sync_test("abcdefghijklmnopqrstuvwxyz0123456789", "NEW_DATA_abcdefghijklmnopqrstuvwxyz0123456789", 5));
}
