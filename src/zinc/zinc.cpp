/*
 * MIT License
 *
 * Copyright (c) 2018 Rokas Kupstys
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <zinc/zinc.h>
#include <json.hpp>
#include <CLI11.hpp>
#if !_WIN32
#   include <unistd.h>
#endif

using json = nlohmann::json;

#if _WIN32
#include <windows.h>
std::wstring to_wstring(const std::string &str)
{
    std::wstring result;
    auto needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), str.length(), 0, 0);
    if (needed > 0)
    {
        result.resize(needed);
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), str.length(), &result.front(), needed);
    }
    return result;
}

int truncate(const char *file_path, int64_t file_size)
{
    std::wstring wfile_path = to_wstring(file_path);
    std::replace(wfile_path.begin(), wfile_path.end(), '/', '\\');
    HANDLE hFile = CreateFileW(wfile_path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (!hFile || hFile == INVALID_HANDLE_VALUE)
        return (int)GetLastError();

    LARGE_INTEGER distance;
    distance.QuadPart = file_size;
    if (!SetFilePointerEx(hFile, distance, 0, FILE_BEGIN))
    {
        CloseHandle(hFile);
        return INVALID_SET_FILE_POINTER;
    }

    if (!SetEndOfFile(hFile))
    {
        auto error = GetLastError();
        CloseHandle(hFile);
        return (int)error;
    }

    CloseHandle(hFile);
    return ERROR_SUCCESS;
}
#endif

void print_progressbar(int progress)
{
    const auto length = 40;
    auto completed = static_cast<size_t>((float)length / 100 * progress);
    std::cout << "\r[" << std::string(completed, '#') << std::string(length - completed, ' ') << "]" << std::flush;
}

bool intersects(const zinc::Boundary& a, const zinc::Boundary& b)
{
    auto a_end = a.start + a.length;
    auto b_end = b.start + b.length;
    return (a.start >= b.start && a.start < b_end) || (a_end >= b.start && a_end < b_end);
}

#if _DEBUG
/// Ensure that offsets are not overwritten before they are read.
void verify_operations_list(zinc::SyncOperationList& delta)
{
    for (auto i = 0UL; i < delta.size(); i++)
    {
        const auto& a = delta[i];                       // earlier spot, source of local data
        for (auto j = i + 1; j < delta.size(); j++)
        {
            const auto& b = delta[j];                   // later operation that might copy from earlier spot

            if (b.local != nullptr)
            {
                assert(!intersects(*b.local, *a.remote));
            }
        }
    }
}
#endif

int main(int argc, char* argv[])
{
    std::string input_file;
    std::string output_file;
    std::string local_file;
    std::string remote_url;
    std::atomic<int64_t> bytes_done{0};
    int64_t bytes_total = 0;

    CLI::App parser{"File synchronization utility."};

    auto* hash_command = parser.add_subcommand("hash", "Build file hashes instead of synchronizing files.");
    hash_command->add_option("input", input_file, "Input file (binary).")->check(CLI::ExistingFile);
    hash_command->add_option("output", output_file, "Output file (json).");

    auto* sync_command = parser.add_subcommand("sync", "Synchronize local file with remote file.");
    sync_command->add_option("local_file", local_file, "Local file (binary).")->check(CLI::ExistingFile);
    sync_command->add_option("remote_url", remote_url, "Remote file url.")->check(CLI::ExistingFile);

    CLI11_PARSE(parser, argc, argv);

    if (hash_command->parsed())
    {
        if (output_file.empty())
            output_file = input_file + ".json";

        FILE* in = fopen(input_file.c_str(), "rb");
        auto boundary_future = zinc::partition_file(in, 0, &bytes_done, &bytes_total);

        auto percent_per_byte = 100.f / bytes_total;
        while (bytes_done < bytes_total)
            print_progressbar(static_cast<int>(percent_per_byte * bytes_done));
        print_progressbar(100);

        auto boundaries = boundary_future.get();
        json doc;
        for (const auto& block : boundaries)
        {
            doc.push_back({
                {"start", block.start},
                {"length", block.length},
                {"fingerprint", block.fingerprint},
                {"hash", block.hash},
            });
        }
        std::ofstream out(output_file);
        out << doc.dump(4) << std::endl;
        fclose(in);
    }
    else if (sync_command->parsed())
    {
        zinc::BoundaryList local_hashes;
        zinc::BoundaryList remote_hashes;

        // Hash local file
        FILE* local = fopen(local_file.c_str(), "rb");
        auto boundary_future = zinc::partition_file(local, 0, &bytes_done, &bytes_total);

        // Print progress
        auto percent_per_byte = 100.f / bytes_total;
        while (bytes_done < bytes_total)
            print_progressbar(static_cast<int>(percent_per_byte * bytes_done));
        print_progressbar(100);

        local_hashes = boundary_future.get();
        fclose(local);

        // Get remote file hashes
        json doc = json::parse(std::ifstream(remote_url + ".json"));
        remote_hashes.reserve(doc.size());
        for (auto& value : doc)
        {
            remote_hashes.emplace_back(zinc::Boundary{
                .start = value["start"].get<int64_t>(),
                .fingerprint = value["fingerprint"].get<uint64_t>(),
                .hash = value["hash"].get<uint64_t>(),
                .length = value["length"].get<int64_t>(),
            });
        }

        // Calculate delta
        auto delta = zinc::compare_files(local_hashes, remote_hashes);
#if _DEBUG
        verify_operations_list(delta);
#endif

        std::ifstream in(remote_url.c_str(), std::ios::binary | std::ios::in);
        std::fstream out(local_file.c_str(), std::ios::binary | std::ios::in | std::ios::out);

        if (!in.is_open() || !out.is_open())
        {
            std::cerr << "Failed to open file\n";
            return -1;
        }

        std::vector<char> buffer;
#if _DEBUG
        std::vector<char> buffer2;
        for (const auto& op : delta)
        {
            if (op.local != nullptr)
            {
                if (buffer.size() < static_cast<size_t>(op.remote->length))
                    buffer.resize(op.local->length);

                out.seekg(op.local->start, std::ios_base::beg);
                out.read(&buffer.front(), op.local->length);
                assert(zinc::detail::fnv64a((uint8_t*)&buffer[0], op.local->length) == op.remote->hash);
            }
        }
#endif
        int64_t bytes_downloaded = 0;
        int64_t bytes_copied = 0;
        for (auto i = 0UL; i < delta.size(); i++)
        {
            auto& op = delta[i];
            if (op.local == nullptr)
            {
                // Download operation
                if (buffer.size() < static_cast<size_t>(op.remote->length))
                    buffer.resize(op.remote->length);

                in.seekg(op.remote->start, std::ios_base::beg);
                in.read(&buffer.front(), op.remote->length);
                bytes_downloaded += op.remote->length;

#if _DEBUG
                assert(zinc::detail::fnv64a((uint8_t*)&buffer[0], op.remote->length) == op.remote->hash);
#endif
            }
            else
            {
                // Copy operation
                if (buffer.size() < static_cast<size_t>(op.remote->length))
                    buffer.resize(op.local->length);

                out.seekg(op.local->start, std::ios_base::beg);
                out.read(&buffer.front(), op.local->length);
                bytes_copied += op.remote->length;

#if _DEBUG
                assert(op.local->length == op.remote->length);
                if (buffer2.size() < static_cast<size_t>(op.remote->length))
                    buffer2.resize(op.remote->length);

                in.seekg(op.remote->start, std::ios_base::beg);
                in.read(&buffer2.front(), op.remote->length);
                assert(memcmp(&buffer[0], &buffer2[0], op.remote->length) == 0);
#endif
            }

            out.seekp(op.remote->start, std::ios_base::beg);
            out.write(&buffer.front(), op.remote->length);
        }

        in.close();
        out.close();

        auto file_size = remote_hashes.back().start + remote_hashes.back().length;
        truncate(local_file.c_str(), file_size);

        std::cout << std::endl;
        std::cout << "Copied bytes: " << bytes_copied << "\n";
        std::cout << "Downloaded bytes: " << bytes_downloaded << "\n";
        std::cout << "Download savings: " << 100 - int(100.0 / file_size * bytes_downloaded) << "%\n";
    }
    else
        std::cout << parser.help();

    return 0;
}
