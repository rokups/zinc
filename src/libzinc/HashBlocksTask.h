#pragma once


#include <cstring>
#include <cassert>
#include <zinc/zinc.h>
#include "Task.hpp"
#include "mmap/FileMemoryMap.h"
#include "RollingChecksum.hpp"
#include "zinc_error.hpp"


namespace zinc
{

class HashBlocksTask : public Task<RemoteFileHashList>
{
public:
    /// Construct dummy task that does nothing.
    HashBlocksTask();
    /// Construct from memory block.
    HashBlocksTask(const void* file_data, int64_t file_size, size_t block_size, size_t thread_count);
    /// Construct from file.
    HashBlocksTask(const char* file_name, size_t block_size, size_t thread_count);
    /// Queues tasks to threadpool for processing blocks.
    void queue_tasks();
    /// Hash some blocks of file. Several instances of this method may run in parallel.
    void process(size_t block_start, size_t block_count);

protected:
    FileMemoryMap _mapping;
    size_t _block_size = 0;
};

}
