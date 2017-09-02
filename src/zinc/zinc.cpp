/**
 * MIT License
 *
 * Copyright (c) 2017 Rokas Kupstys
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
#include <functional>
#include <thread>
#include <chrono>
#include <zinc/zinc.h>
#include <json.hpp>
#include <CLI11.hpp>
#include "../libzinc/Utilities.hpp"

using namespace nlohmann;
using namespace std::placeholders;
using namespace std::chrono_literals;

class ProgressBar
{
public:
    ProgressBar() = default;
    ProgressBar(const std::string& message)
        : _message(message)
    {
    }

    void update(double done_percent, bool done)
    {
        printf("%s : [", _message.c_str());
        for (auto i = 0; i < (done_percent / 2); i++)
            printf("#");
        for (auto i = 0; i < 50 - (done_percent / 2); i++)
            printf(" ");

        printf("] %.2f", (float)done_percent);

        if (done)
            printf("\n");
        else
            printf("\r");

        fflush(stdout);
    }

protected:
    std::string _message;
};

class FileReader
{
public:
    FileReader(const std::string& file_path)
    {
        _fp = fopen(file_path.c_str(), "rb");
        if (!_fp)
        {
            std::string message = "Could not access file '" + file_path + "'.";
            throw std::system_error(errno, std::system_category(), message);
        }
    }

    ~FileReader()
    {
        if (_fp)
            fclose(_fp);
    }

    zinc::ByteArray get_data(int64_t block_index, size_t block_size)
    {
        zinc::ByteArray result;
        result.resize(block_size);
        fseek(_fp, block_index * block_size, SEEK_SET);
        auto size = fread(&result.front(), 1, block_size, _fp);
        result.resize(size);
        return result;
    };

protected:
    FILE* _fp = 0;
};

size_t suggest_block_size(int64_t file_size)
{
    return (size_t)std::max(5 * 1024, std::min(4 * 1024 * 1024, int(file_size / 512)));
}

void hex_to_bytes(const std::string& input, uint8_t* output)
{
    for (auto i = 0; i < input.length() / 2; i++)
        output[i] = (uint8_t)strtol(input.substr(static_cast<unsigned long>(i * 2), 2).c_str(), nullptr, 16);
}

int main(int argc, char* argv[])
{
    bool hash_file = false;
    std::string input_file;
    std::string output_file;

    CLI::App parser{"File synchronization utility."};
    parser.add_flag("--hash", hash_file, "Build file hashes instead of synchronizing files.");
    parser.add_option("input", input_file, "Input file.")->check(CLI::ExistingFile);
    parser.add_option("output", output_file, "Output file.");

    if (argc < 2)
    {
        std::cout << parser.help();
        return 0;
    }

    try
    {
        parser.parse(argc, argv);
    }
    catch (const CLI::ParseError &e)
    {
        return parser.exit(e);
    }

    ProgressBar bar;
    auto progress_report = [&](int64_t bytes_done_now, int64_t bytes_done_total, int64_t file_size_)
    {
        assert(bytes_done_total <= file_size_);
        bar.update(100.0 / file_size_ * bytes_done_total, bytes_done_total == file_size_);
        return true;
    };

    try
    {
        if (hash_file)
        {
            auto file_size = zinc::get_file_size(input_file.c_str());
            bar = ProgressBar("Hashing file");
            auto block_size = suggest_block_size(file_size);
            auto checksums_task = zinc::get_block_checksums(input_file.c_str(), block_size, std::thread::hardware_concurrency());

            do
            {
                std::this_thread::sleep_for(150ms);
                bar.update(checksums_task->progress(), checksums_task->is_done());
            } while (!checksums_task->is_done());

            json output;
            output["file_size"] = file_size;
            output["block_size"] = block_size;
            output["blocks"] = {};
            for (const auto& h: checksums_task->result())
                output["blocks"].push_back(json::array({h.weak, h.strong.to_string()}));

            if (FILE* fp = fopen(output_file.c_str(), "w+b"))
            {
                auto result = output.dump(4);
                fwrite(result.c_str(), 1, result.length(), fp);
                fclose(fp);
            }
            else
            {
                std::cerr << "Could not open '" << output_file << "' for writing.\n";
                return -1;
            }
        }
        else
        {
            // Get hashes from json.
            json file_manifest = json::parse(std::ifstream(input_file + ".json"));
            const auto& blocks = file_manifest["blocks"];
            auto block_size = file_manifest["block_size"].get<size_t>();
            auto file_size = file_manifest["file_size"].get<int64_t>();

            zinc::RemoteFileHashList hashes;
            hashes.reserve(blocks.size());
            for (auto it = blocks.begin(); it != blocks.end(); it++)
            {
                zinc::BlockHashes h;
                h.weak = (*it)[0].get<zinc::WeakHash>();
                hex_to_bytes((*it)[1].get<std::string>(), (uint8_t*)h.strong.data());
                hashes.push_back(h);
            }

            bar = ProgressBar("Calculating delta");
            auto delta = zinc::get_differences_delta(output_file.c_str(), block_size, hashes, progress_report);

            FileReader reader(input_file);
            bar = ProgressBar("Patching file    ");
            zinc::patch_file(output_file.c_str(), file_size, block_size, delta, std::bind(&FileReader::get_data, &reader, _1, _2), progress_report);
        }
    }
    catch (std::invalid_argument& e)
    {
        std::cerr << e.what();
        return -1;
    }
    catch (std::system_error& e)
    {
        std::cerr << e.what() << "(" << e.code() << ")";
        return -1;
    }

    return 0;
}