zinc - the reverse-rsync
==========================

[![Build Status](https://travis-ci.org/rokups/zinc.svg?branch=master)](https://travis-ci.org/rokups/zinc)

**WARNING:** **DO NOT USE IT WITH IMPORTANT AND/OR NOT BACKED UP DATA**. This code is of beta quality. I am fairly 
certain it will not corrupt your data. Code was fuzz-tested for weeks without failures and is in use in a live 
environment with rather small (few hundred) users without issues. However, you never know.

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

### Example

Project comes with a testing tool `zinc` which is used mainly for debugging. Tool is reading and writing local files.
Example below was performed in tmpfs, ISO files were about 500MB and tested CPU was i7-6800K (6 core / 12 thread). Due 
to tmpfs you may consider test timing results as benchmark of core algorithm as file reading/writing basically happened 
in memory.
```sh
/tmp % # Test files are rather old Archliux ISOs
/tmp % sha1sum archlinux-2017.0*.iso
77508eaf2be9f15e22b1cc920db12d4e90c2cf77  archlinux-2017.06.01-x86_64.iso
dd03d811211c332d29155069d8e4bb2306c70f33  archlinux-2017.07.01-x86_64.iso

/tmp % # Hash new ISO file. Produced json file is hosted on a remote (web)server along with the ISO
/tmp % ./zinc --hash ./archlinux-2017.07.01-x86_64.iso ./archlinux-2017.07.01-x86_64.iso.json

/tmp % # Client system obtains json file with hashes from a remote server and finds different and matching blocks
/tmp % # Client system then moves existing matching blocks to their new locations while downloading missing blocks from remote server
/tmp % time ./zinc ./archlinux-2017.07.01-x86_64.iso ./archlinux-2017.06.01-x86_64.iso
Calculating delta : [##################################################] 100.00
Patching file     : [##################################################] 100.00
./zinc ./archlinux-2017.07.01-x86_64.iso   70.23s user 0.24s system 977% cpu 7.206 total

/tmp % # ISO was updated in less than a second
/tmp % sha1sum archlinux-2017.0*.iso
dd03d811211c332d29155069d8e4bb2306c70f33  archlinux-2017.06.01-x86_64.iso
dd03d811211c332d29155069d8e4bb2306c70f33  archlinux-2017.07.01-x86_64.iso
```

### Other similar software
* [rsync](https://rsync.samba.org/) - inspiration of zinc
* [zsync](http://zsync.moria.org.uk/) - inspiration of zinc
* [xdelta](http://xdelta.org/) - delta updates library
* [goodsync](https://www.goodsync.com/) - proprietary block level synchronization utility
