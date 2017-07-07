zinc - the reverse-rsync
==========================

Disclaimer: **this library has nothing to do with rsync** other than idea.

**WARNING:** This code is very immature, likely buggy and a subject to change. It would be great if it saw some testing
from people other than me, however if you use it for anything serious and it breaks - you get to keep both pieces.

rsync has become a synonym to efficient data synchronization. There are few issues however: server does heavy lifting
and GPL license.

There was an attempt to bring rsync fully into a client by [zsync](http://zsync.moria.org.uk/) project, however it
appears to not be maintained any more, it's license is also non-permissive and code of zsync is not easily embeddable
into other projects. This library is a humble attempt to fix these issues.

### Features

* Block level file synchronization - downloads only missing pieces, reuses existing data.
* No special server setup - any http(s) server supporting `Range` header will do.
* Files are updated in-place - huge files of tens of gigabytes will not be copied and only changed parts will be
written. Your SSD will be happy.
* Progress reporting callbacks.
* c++11 required, though faster with c++14.
* Python bindings.
* Example implementation of synchronization tool written in python and c++.
* Multithreaded.
* Free as in freedom - use it however you like, in open source (preferably) or proprietary software, no strings attached.

### How it works

      +-------------------------------- Server -------------------------------+
      | block_checksums = get_block_checksums(new_file_path, block_size)      |
      +-----------------------------------------------------------------------+
                                        |
                               [ Transport (http) ]
                                        |
    +---------------------------------- Client ---------------------------------+
    | delta = get_differences_delta(old_file_path, block_size, block_checksums) |
    | void get_data_cb(size_t block_index, size_t block_size)                   |
    | {                                                                         |
    |     ByteArray data;                                                       |
    |     // Download `block_size` number of bytes from `new_file_path`,        |
    |     // starting at `block_index * block_size` position.                   |
    |     return data;                                                          |
    | }                                                                         |
    | patch_file(old_file_path, new_file_size, block_size, delta, get_data_cb)  |
    +---------------------------------------------------------------------------+

On the served end `block_checksums` should be calculated once and written to a file for retrieval by client. Library is
transport-agnostic and you may use any transport you desire. Http is named as a suggested transport because it
eliminates need of any custom server setup and is most convenient option available today.

As you can see from diagram above process of synchronizing data is composed of three steps:

1. Hashing: Latest version of the file is split into even blocks and for every block strong and weak hashes are calculated.
2. Delta calculation: Client obtains the list of block hashes, then calculates a rolling checksum on the local file. Using rolling checksum and list of remote file hashes algorithm determines what file parts need to be kept/copied/downloaded. Local file data will be reused as much as possible.
3. Patching: Using a calculated delta map blocks in local file are rearranged much like a puzzle pieces, missing pieces are downloaded.

### Other similar software
* [rsync](https://rsync.samba.org/) - inspiration of zinc
* [zsync](http://zsync.moria.org.uk/) - inspiration of zinc
* [xdelta](http://xdelta.org/) - delta updates library
* [goodsync](https://www.goodsync.com/) - proprietary block level synchronization utility
