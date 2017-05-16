#define BOOST_TEST_MODULE "Identical"
#include "common.h"


BOOST_AUTO_TEST_CASE (Identical)
{
    // Identical data
    BOOST_CHECK(data_sync_test("abcdefghijklmnopqrstuvwxyz0123456789", "abcdefghijklmnopqrstuvwxyz0123456789", 5));
}
