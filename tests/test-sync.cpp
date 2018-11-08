#include <stdio.h>
#define CATCH_CONFIG_MAIN
#include <catch.hpp>
#include <zinc/zinc.h>


zinc::Parameters get_parameters()
{
    zinc::Parameters p;
    p.window_length = 4;
    p.min_block_size = 5;
    p.max_block_size = 20;
    p.match_bits = 4;
    return p;
}

#if _WIN32
FILE* fmemopen(void* data, int len, const char* mode)
{
    FILE* file = tmpfile();
    fwrite(data, 1, len, file);
    rewind(file);
    return file;
}

void fclose(FILE* fp, void* data, size_t len)
{
    fflush(fp);
    rewind(fp);
    fread(data, 1, len, fp);
    fclose(fp);
}
#endif

bool data_sync_test(std::string old_data, const std::string& new_data)
{
    auto parameters = get_parameters();

    FILE* old_fp = fmemopen((void*)old_data.data(), old_data.length(), "rb");
    FILE* new_fp = fmemopen((void*)new_data.data(), new_data.length(), "rb");

    auto old_parts = zinc::partition_file(old_fp, 1, nullptr, nullptr, nullptr, &parameters).get();
    auto new_parts = zinc::partition_file(new_fp, 1, nullptr, nullptr, nullptr, &parameters).get();

    // Ensure old file has enough space for new content
    fclose(old_fp);
    old_data.resize(std::max(old_data.length(), new_data.length()), 0);
    old_fp = fmemopen((void*)old_data.data(), old_data.size(), "r+b");

    zinc::SyncOperationList delta = zinc::compare_files(old_parts, new_parts);

    std::string buffer;
    for (const auto& op : delta)
    {
        if (buffer.size() < op.remote->length)
            buffer.resize(op.remote->length);

        if (op.local == nullptr)
        {
            fseek(new_fp, op.remote->start, SEEK_SET);
            fread(const_cast<char*>(buffer.data()), 1, op.remote->length, new_fp);
        }
        else
        {
            fseek(old_fp, op.local->start, SEEK_SET);
            fread(const_cast<char*>(buffer.data()), 1, op.local->length, old_fp);
        }

        fseek(old_fp, op.remote->start, SEEK_SET);
        fwrite(&buffer[0], 1, op.remote->length, old_fp);
    }

#if _WIN32
    fclose(old_fp, (void*)old_data.data(), old_data.length());
    fclose(new_fp, (void*)new_data.data(), new_data.length());
#else
    fclose(old_fp);
    fclose(new_fp);
#endif

    old_data.resize(new_data.size());
    if (old_data == new_data)
        return true;
    else
    {
        WARN(old_data + " != " + new_data);
        return false;
    }
}


TEST_CASE("Identical")
{
    REQUIRE(data_sync_test("abcdefghijklmnopqrstuvwxyz0123456789", "abcdefghijklmnopqrstuvwxyz0123456789"));
}

TEST_CASE("BlocksSwapped")
{
    REQUIRE(data_sync_test("abcdefghijklmno34567pqrstuvwxyz01289", "abcdefghijklmnopqrstuvwxyz0123456789"));
}

TEST_CASE("EndAdd")
{
    REQUIRE(data_sync_test("abcdefghijklmnopqrstuvwxyz0123456789_NEW_DATA", "abcdefghijklmnopqrstuvwxyz0123456789"));
}

TEST_CASE("EndRemove")
{
    REQUIRE(data_sync_test("abcdefghijklmnopqrstuvwxyz0123456789", "abcdefghijklmnopqrstuvwxyz0123456789_NEW_DATA"));
}

TEST_CASE("FrontAdd1")
{
    REQUIRE(data_sync_test("NEW_DATA_abcdefghijklmnopqrstuvwxyz0123456789", "abcdefghijklmnopqrstuvwxyz0123456789"));
}

TEST_CASE("FrontAdd2")
{
    REQUIRE(data_sync_test("_abcdefghijklmnopqrstuvwxyz0123456789", "abcdefghijklmnopqrstuvwxyz0123456789"));
}

TEST_CASE("FrontRemove")
{
    REQUIRE(data_sync_test("abcdefghijklmnopqrstuvwxyz0123456789", "NEW_DATA_abcdefghijklmnopqrstuvwxyz0123456789"));
}

TEST_CASE("Shuffle")
{
    REQUIRE(data_sync_test("abcdefghijklmnopqrstuvwxyz0123456789", "abcdefghrstuvwxyz0123ijklmnopq456789"));
}

TEST_CASE("UseExistingData")
{
    REQUIRE(data_sync_test("12345123452222212345", "00000111112222212345"));
}

TEST_CASE("RefCachedBlockTwice")
{
    REQUIRE(data_sync_test("defg defg 9abc 0000 ", "1234 5678 9abc defg "));
}

TEST_CASE("RefCachedBlockTwice2")
{
    REQUIRE(data_sync_test(
        "`pO6Vqe8*+w0,M^upV$}mHKmCy`_3R#3n:|)N.Va%t_'7g*^/;1ghO6Vqe8*+w0,M^upV$}mHKmCy`_3R#3n:|)N.Va%t_'7g*^/;1gh}0Bt[ub(oRp5>uEY!%z6R8C<Bh:HpQl.\\R",
        "zJi[=zYhQ4<,1SyKr=>G0)<(P(YUv[nx\" C-f,IJPD`r`pO6Vqe8*+w0,M^upV$}mHKmCy`_3R#3n:|)N.Va%t_'7g*^/;1gh}0Bt[ub(oRp5>uEY!%z6R8C<Bh:HpQlqQpiamP.\\R&"));
}

TEST_CASE("FuzzTest1")
{
    REQUIRE(data_sync_test(
        ",<*7Dxk:%\\7CL]R^J^#Da'hw<8Z;%=0Q7%1/mcMIeHx*VDEu48mWWaB4V\\)llLxfjsR=!YT,kLbTjWEd&[}xCb;jdu/05m\"5DD%iPevf6T#^HgIs4`R]WU437e\"oB#O#&dwSF4H3i>3/njSJYK6@HB'VziPabjbTQ[\"]Y%yQHEj=#^HgIs4`R]WU\"oB#O#&dwSF4H`1Qj;VigiO!8Jc$2`-EwRs-vq4Sokl8;MiMT@",
        ",<*7Dxk:%\\7CL]R^ NL_6!$ZC7:J^#Da'hw<8Z;%=0Q7%1/mcMIeHx*VDEu48mWWaB4V\\)llLxfjsR=!YT,kLbTjWEd&[}xCb;jdu/05m\"5DD%iPevf6TH:,5/e>kLQ[;Sq<hd53i>3/njSJYK6@HB'VziPabjbTQ[\"]Y%yQHEj=#^HgIs4`R]WU437e\"oB#O#&dwSF4H`1Qj;VigiO!8Jc$2`-EwRs-vq4Sokl8;MiMT@p"));
}

TEST_CASE("FuzzTest2")
{
    REQUIRE(data_sync_test(",hI|J@Q\\so}:6f=_yoy\\so}:6f=_\\so}:6f=_yo", "}:6f=_yoyL?k,hI|J@Q\\soOsD;E}CvfC]OS!G5"));
}

TEST_CASE("IdenticalBlockDownload")
{
    REQUIRE(data_sync_test("1234_1234_000001234_", "00000000000000000000"));
}

TEST_CASE("FuzzTest3")
{
    REQUIRE(data_sync_test("h'10{'6rI8RI5N@RI5N@u+!BkRI5N@u+!Bk29H0<p+n{ZIu{*", "h'10 |Av2{'6rI8RI5N@u+!Bk2I,Qq){QkZIuX/"));
}
