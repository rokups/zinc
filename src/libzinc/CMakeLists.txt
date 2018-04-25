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
if (WIN32)
    list(REMOVE_ITEM SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/mmap/FileMemoryMap_unix.cpp)
else ()
    list(REMOVE_ITEM SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/mmap/FileMemoryMap_win32.cpp)
endif ()
add_library (libzinc STATIC ${SOURCE_FILES} ${HEADER_FILES})
target_include_directories(libzinc
    PUBLIC ${zinc_SOURCE_DIR}/include
    INTERNAL ${CMAKE_CURRENT_SOURCE_DIR}
)
if (NOT MSVC)
    target_compile_options(libzinc PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-std=c++14>)
    if (NOT WIN32)
        # Required for unixes, enabled by default already on mingw.
        target_compile_options(libzinc PUBLIC -fPIC)
    endif()
endif ()
if (ZINC_WITH_STRONG_HASH_FNV)
    target_compile_definitions(libzinc PUBLIC -DZINC_WITH_STRONG_HASH_FNV=1)
else ()
    target_compile_definitions(libzinc PUBLIC -DZINC_WITH_STRONG_HASH_SHA1=1)
endif ()
if (ZINC_WITH_LOGGING)
    target_compile_definitions(libzinc PUBLIC -DZINC_WITH_LOGGING=1)
endif ()
if (ZINC_WITH_ADDRESS_SANITIZER AND IS_DEBUG)
    target_compile_options(libzinc PUBLIC -fsanitize=address)
    target_link_libraries(libzinc -lasan)
endif ()
if (ZINC_WITH_COVERAGE)
    target_compile_options(libzinc PUBLIC -g -O0 -fprofile-arcs -ftest-coverage)
    target_link_libraries(libzinc -lgcov)
endif ()
if (ZINC_WITH_EXCEPTIONS)
    target_compile_definitions(libzinc PUBLIC -DZINC_WITH_EXCEPTIONS=1)
endif ()
if (ZINC_NO_MMAP)
    target_compile_definitions(libzinc PUBLIC -DZINC_NO_MMAP=1)
endif ()
target_link_libraries(libzinc flat_hash_map ${CMAKE_THREAD_LIBS_INIT})

message("ZINC_WITH_LOGGING:           ${ZINC_WITH_LOGGING}")
message("ZINC_WITH_ADDRESS_SANITIZER: ${ZINC_WITH_ADDRESS_SANITIZER}")
message("ZINC_WITH_COVERAGE:          ${ZINC_WITH_COVERAGE}")
message("ZINC_WITH_EXCEPTIONS:        ${ZINC_WITH_EXCEPTIONS}")
