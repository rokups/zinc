zinc - the reverse-rsync
========================

[![Build Status](https://travis-ci.org/rokups/zinc.svg?branch=master)](https://travis-ci.org/rokups/zinc)

**WARNING:** **DO NOT USE IT WITH IMPORTANT AND/OR NOT BACKED UP DATA**. This code is of alpha quality.

rsync has become a synonym to efficient data synchronization. There are few issues however: server does heavy lifting
and GPL license.

There was an attempt to bring rsync fully into a client by [zsync](http://zsync.moria.org.uk/) project, however it
appears to not be maintained any more, it's license is also non-permissive and code of zsync is not easily embeddable
into other projects. This library is a humble attempt to fix these issues.

### Features

* Block level file synchronization - downloads only missing pieces, reuses existing data.
* No special server setup - any http(s) server supporting `Range` header will do.
* Files are updated in-place - huge files of tens of gigabytes will not be copied and only changed parts will be written. Your SSD will be happy.
* Progress reporting callbacks.
* c++11 required.
* Example implementation of synchronization tool written in c++.
* Multithreaded.
* Free as in freedom - use it however you like, in open source (preferably) or proprietary software, no strings attached.

### How it works

      +-------------------------------- Server -------------------------------+
      | new_boundary_list = partition_file(new_file);                         |
      +-----------------------------------------------------------------------+
                                        |
                         [ Transport (for example http) ]
                                        |
      +---------------------------------- Client -----------------------------+
      | new_boundary_list = partition_file(old_file);                         |
      | delta = compare_files(old_boundary_list, new_boundary_list);          |
      | // Patch file                                                         |
      | for (const auto& operation : delta)                                   |
      | {                                                                     |
      |     if (operation.local == nullptr)                                   |
      |     {                                                                 |
      |         // Download block from remote file                            |
      |         auto* remote = operation.remote;                              |
      |         void* data = download_block(remote->start, remote->length);   |
      |         fp_old->seek(remote->start);                                  |
      |         fp_old->write(data, remote->length);                          |
      |     }                                                                 |
      |     else                                                              |
      |     {                                                                 |
      |         // Copy block from local file                                 |
      |         fp_old->seek(operation.local->start);                         |
      |         void* data = fp_old->read(operation.local->length);           |
      |         fp_old->seek(operation.remote->start);                        |
      |         fp_old->write(data, operation.local->length);                 |
      |     }                                                                 |
      | }                                                                     |
      +-----------------------------------------------------------------------+

On the served end `new_boundary_list` should be calculated once and written to a file for retrieval by client. Library
is transport-agnostic and you may use any transport you desire. Http is named as a suggested transport because it
eliminates need of any custom server setup and is most convenient option available today.

As you can see from diagram above process of synchronizing data is composed of three steps:

1. Hashing: Latest version of the file is split into variable size blocks and for every block strong and weak hashes are calculated.
2. Delta calculation: Client obtains the list of block, then splits a local file to variable size blocks and finally compares block lists of both files and determines which parts should be moved and which parts should be downloaded.
3. Patching: Using a calculated delta map blocks in local file are rearranged much like a puzzle pieces, missing pieces are downloaded.

### Example

Project comes with a testing tool `zinc` which is used mainly for debugging. Tool is reading and writing local files.
Example below was performed in tmpfs, test files are two tar archives. `new.tar` contains 10 binary files 10MB each.
`old.tar` is a copy of `new.tar` with one (middle) file removed and has a 10MB "hole" in the middle of file. Test 
performed on a i7-6800K CPU (6 core / 12 thread). Due to tmpfs you may consider test timing results as benchmark of core
 algorithm as file reading/writing basically happened in memory.
```sh
/tmp % sha1sum *.tar
6b9d22479a91b25347842f161eff53eab050b5d1  new.tar
71cf71c7d1433682a4b0577d982dcd5956233e7c  old.tar

/tmp % # Hash new ISO file. Produced json file is hosted on a remote (web)server along with the ISO
/tmp % zinc hash new.tar 
[########################################]

/tmp % ls new.tar*
new.tar  new.tar.json

/tmp % # Client system obtains json file with hashes from a remote server and finds different and matching blocks
/tmp % # Client system then moves existing matching blocks to their new locations while downloading missing blocks from remote server
/tmp % time zinc sync old.tar new.tar
[########################################]
Copied bytes: 51987553
Downloaded bytes: 14265606
Download savings: 87%
zinc sync old.tar   0.73s user 0.33s system 533% cpu 0.199 total

/tmp % # File was updated in less than a second
/tmp % sha1sum *.tar
6b9d22479a91b25347842f161eff53eab050b5d1  new.tar
6b9d22479a91b25347842f161eff53eab050b5d1  old.tar
```

### Other similar software
* [rsync](https://rsync.samba.org/) - inspiration of zinc
* [zsync](http://zsync.moria.org.uk/) - inspiration of zinc
* [xdelta](http://xdelta.org/) - delta updates library
* [goodsync](https://www.goodsync.com/) - proprietary block level synchronization utility
