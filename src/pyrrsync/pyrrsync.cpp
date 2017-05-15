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
#include <zinc.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

namespace py = pybind11;
using namespace zinc;

PYBIND11_PLUGIN(pyzinc)
{
    py::module m("pyzinc", "Python bindings for zinc");

    py::class_<BlockHashes>(m, "BlockHashes")
            .def_readonly("weak", &BlockHashes::weak)
            .def_readonly("strong", &BlockHashes::strong);
    py::class_<DeltaElement>(m, "DeltaElement")
            .def_readonly("block_index", &DeltaElement::block_index)
            .def_readonly("local_offset", &DeltaElement::local_offset);
    py::bind_vector<RemoteFileHashList>(m, "RemoteFileHashList");
    py::bind_vector<DeltaMap>(m, "DeltaMap");

    m.def("get_block_checksums", &get_block_checksums, "");
    m.def("get_block_checksums_mem", &get_block_checksums_mem, "");
    m.def("get_differences_delta", &get_differences_delta, "");
    m.def("get_differences_delta_mem", &get_differences_delta_mem, "");
    m.def("patch_file", &patch_file, "");
    m.def("patch_file_mem", &patch_file_mem, "");

    return m.ptr();
}
