#define BOOST_TEST_MODULE "EndAdd"
#include "common.h"


BOOST_AUTO_TEST_CASE (EndAdd)
{
    // New data at the end
    BOOST_CHECK(data_sync_test("abcdefghijklmnopqrstuvwxyz0123456789_NEW_DATA", "abcdefghijklmnopqrstuvwxyz0123456789", 5));
}
