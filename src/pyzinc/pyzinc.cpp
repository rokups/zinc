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
#include <cmath>
#include <zinc/zinc.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>
#include <pybind11/functional.h>

namespace py = pybind11;
using namespace zinc;

PYBIND11_PLUGIN(pyzinc)
{
    py::module m("pyzinc", "Python bindings for zinc");

    py::bind_vector<RemoteFileHashList>(m, "RemoteFileHashList");
    py::bind_vector<ByteArray>(m, "ByteArray");

    py::class_<BlockHashes>(m, "BlockHashes")
        .def_readonly("weak", &BlockHashes::weak)
        .def_readonly("strong", &BlockHashes::strong);

    py::class_<DeltaElement>(m, "DeltaElement")
        .def_readonly("block_index", &DeltaElement::block_index)
        .def_readonly("local_offset", &DeltaElement::local_offset)
        .def_readonly("block_offset", &DeltaElement::block_offset);

    py::class_<ITask<RemoteFileHashList>>(m, "HashBlocksTask")
        .def("progress", &ITask<RemoteFileHashList>::progress)
        .def("is_done", &ITask<RemoteFileHashList>::is_done)
        .def("result", &ITask<RemoteFileHashList>::result)
        .def("cancel", &ITask<RemoteFileHashList>::cancel)
        .def("success", &ITask<RemoteFileHashList>::success)
        .def("wait", &ITask<RemoteFileHashList>::wait);

    py::class_<ITask<DeltaMap>>(m, "ResolveDeltaTask")
        .def("progress", &ITask<RemoteFileHashList>::progress)
        .def("is_done", &ITask<RemoteFileHashList>::is_done)
        .def("result", &ITask<RemoteFileHashList>::result)
        .def("cancel", &ITask<RemoteFileHashList>::cancel)
        .def("success", &ITask<RemoteFileHashList>::success)
        .def("wait", &ITask<RemoteFileHashList>::wait);

    py::class_<DeltaMap>(m, "DeltaMap")
        .def_readonly("map", &DeltaMap::map)
        .def_readonly("identical_blocks", &DeltaMap::identical_blocks)
        .def("is_empty", &DeltaMap::is_empty);

    m.def("get_block_checksums", (std::unique_ptr<ITask<RemoteFileHashList>>(*)(const void*, int64_t, size_t, size_t))&get_block_checksums, "");
    m.def("get_block_checksums", (std::unique_ptr<ITask<RemoteFileHashList>>(*)(const char*, size_t, size_t))&get_block_checksums, "");
    m.def("get_differences_delta", (std::unique_ptr<ITask<DeltaMap>>(*)(const void*, int64_t, size_t, const RemoteFileHashList&, size_t))&get_differences_delta, "");
    m.def("get_differences_delta", (std::unique_ptr<ITask<DeltaMap>>(*)(const char*, size_t, const RemoteFileHashList&, size_t))&get_differences_delta, "");
    m.def("patch_file", (bool(*)(void*, int64_t, size_t, DeltaMap&, const FetchBlockCallback&, const ProgressCallback&))&patch_file, "");
    m.def("patch_file", (bool(*)(const char*, int64_t, size_t, DeltaMap&, const FetchBlockCallback&, const ProgressCallback&))&patch_file, "");

    return m.ptr();
}
