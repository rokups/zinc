#
# MIT License
#
# Copyright (c) 2017 Rokas Kupstys
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#
cmake_minimum_required(VERSION 3.3.2)
project(zinc)

if (NOT ZINC_DISTUTILS_BUILD AND "${CMAKE_SOURCE_DIR}" STREQUAL "${zinc_SOURCE_DIR}")
    set(ZINC_STANDALONE_BUILD 1)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
endif ()

set(CMAKE_CXX_STANDARD 14)
if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    set (IS_DEBUG ON)
endif ()

option (ZINC_WITH_TESTS "Build and run tests" ON)

if (CMAKE_COMPILER_IS_GNUCXX)
    option(ZINC_WITH_COVERAGE "Enable gcov coverage testing." ${IS_DEBUG})
else ()
    set(ZINC_WITH_COVERAGE OFF)
endif ()
option(ZINC_WITH_EXCEPTIONS "Enable exceptions" ON)
option(ZINC_WITH_LOGGING "Enable verbose logging" ${IS_DEBUG})
option(ZINC_WITH_PYTHON "Enable python bindings" ON)
option(ZINC_WITH_ADDRESS_SANITIZER "Enable address sanitizer in debug builds" ${IS_DEBUG})
option(ZINC_NO_MMAP "Use file reading/writing API instead of memory-mapping file" OFF)

add_subdirectory(external)
add_subdirectory(src)

if (ZINC_WITH_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
