#define BOOST_TEST_MODULE "BlocksSwapped"
#include "common.h"


BOOST_AUTO_TEST_CASE (BlocksSwapped)
{
    // New data at the end
    BOOST_CHECK(data_sync_test("abcdefghijklmno34567pqrstuvwxyz01289", "abcdefghijklmnopqrstuvwxyz0123456789", 5));
}
