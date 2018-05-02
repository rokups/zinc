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
    /// Construct file. This instance becomes owner of `file` pointer.
    HashBlocksTask(IFile* file, size_t block_size, size_t thread_count);

    ~HashBlocksTask() override = default;

protected:
    int64_t _block_size = 0;

    /// Queues tasks to threadpool for processing blocks.
    void queue_tasks();
    /// Hash some blocks of file. Several instances of this method may run in parallel.
    void process(size_t block_start, size_t block_count);
};

}
