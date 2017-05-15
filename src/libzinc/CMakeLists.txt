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

file (GLOB_RECURSE SOURCE_FILES *.c *.cpp *.h *.hpp)
file (GLOB_RECURSE HEADER_FILES ${zinc_SOURCE_DIR}/include/*.h)
add_library (zinc STATIC ${SOURCE_FILES} ${HEADER_FILES})
target_include_directories(zinc
    PUBLIC ${zinc_SOURCE_DIR}/include
    INTERNAL ${CMAKE_CURRENT_SOURCE_DIR}
)
if (NOT MSVC)
    target_compile_options(zinc PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-std=c++14>)
    if (NOT WIN32)
        # Required for unixes, enabled by default already on mingw.
        target_compile_options(zinc PRIVATE -fPIC)
    endif()
endif ()
target_compile_definitions(zinc PUBLIC -DZINC_FNV=1)
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions (zinc PUBLIC -DZINC_DEBUG=1)
endif ()
target_link_libraries(zinc flat_hash_map)
