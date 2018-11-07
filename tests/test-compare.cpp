#define CATCH_CONFIG_MAIN
#include <catch.hpp>
#include <zinc/zinc.h>

TEST_CASE("identical files")
{
    zinc::BoundaryList a {
        {.start = 0, .fingerprint = 10, .hash = 11, .length = 5},
        {.start = 5, .fingerprint = 20, .hash = 22, .length = 5},
        {.start = 10, .fingerprint = 30, .hash = 33, .length = 5},
    };
    zinc::BoundaryList b {
        {.start = 0, .fingerprint = 10, .hash = 11, .length = 5},
        {.start = 5, .fingerprint = 20, .hash = 22, .length = 5},
        {.start = 10, .fingerprint = 30, .hash = 33, .length = 5},
    };

    auto result = zinc::compare_files(a, b);

    REQUIRE(result.size() == 0);
}

TEST_CASE("overlapping ranges")
{
    zinc::BoundaryList a {
        {.start = 0, .fingerprint = 10, .hash = 11, .length = 5},
        {.start = 5, .fingerprint = 20, .hash = 22, .length = 5},
        {.start = 10, .fingerprint = 30, .hash = 33, .length = 5},
    };
    zinc::BoundaryList b {
        {.start = 0, .fingerprint = 30, .hash = 33, .length = 5},   // swapped with last
        {.start = 5, .fingerprint = 20, .hash = 22, .length = 5},
        {.start = 10, .fingerprint = 10, .hash = 11, .length = 5},  // swapped with first
    };

    auto result = zinc::compare_files(a, b);

    REQUIRE(result.size() == 2);    // Should have two actions, one move and one download.

    if (result[0].local == nullptr)
        REQUIRE(result[1].local != nullptr);
    else
        REQUIRE(result[0].local != nullptr);
}

TEST_CASE("copy from overwritten")
{
    zinc::BoundaryList a {
        {.start = 0, .fingerprint = 10, .hash = 11, .length = 5},
        {.start = 5, .fingerprint = 20, .hash = 22, .length = 5},
        {.start = 10, .fingerprint = 30, .hash = 33, .length = 5},
    };
    zinc::BoundaryList b {
        {.start = 0, .fingerprint = 100, .hash = 110, .length = 5}, // download block
        {.start = 5, .fingerprint = 20, .hash = 22, .length = 5},
        {.start = 10, .fingerprint = 10, .hash = 11, .length = 5},  // copy from first downloaded block
    };

    auto result = zinc::compare_files(a, b);

    REQUIRE(result.size() == 2);    // Should have two actions, one move and one download.

    // Copy from first block
    REQUIRE(result[0].local != nullptr);
    REQUIRE(result[0].local->start == 0);
    REQUIRE(result[0].remote->start == 10);

    // Download first block
    REQUIRE(result[1].local == nullptr);
    REQUIRE(result[1].remote->start == 0);
}

TEST_CASE("reorder intersecting")
{
    zinc::BoundaryList a {
        {.start = 0, .fingerprint = 10, .hash = 11, .length = 5},
        {.start = 5, .fingerprint = 20, .hash = 22, .length = 5},
        {.start = 10, .fingerprint = 30, .hash = 33, .length = 5},
    };
    zinc::BoundaryList b {
        {.start = 0, .fingerprint = 100, .hash = 110, .length = 10},
        {.start = 10, .fingerprint = 10, .hash = 11, .length = 5},
    };
    auto result = zinc::compare_files(a, b);

    REQUIRE(result.size() == 2);    // Should have two actions, one move and one download.

    // Copy from first block
    REQUIRE(result[0].local != nullptr);
    REQUIRE(result[0].local->start == 0);
    REQUIRE(result[0].remote->start == 10);

    // Download first block
    REQUIRE(result[1].local == nullptr);
    REQUIRE(result[1].remote->start == 0);
}
