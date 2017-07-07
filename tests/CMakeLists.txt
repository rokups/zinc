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
find_package(Boost QUIET COMPONENTS unit_test_framework)
if (Boost_FOUND)
    include_directories (${Boost_INCLUDE_DIRS})
    file(GLOB TEST_SRCS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} test_*.cpp)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests)
    foreach (test_source_file ${TEST_SRCS})
        get_filename_component (test_name ${test_source_file} NAME_WE)
        add_executable (${test_name} ${test_source_file} common.h common.cpp)
        target_link_libraries (${test_name} -lasan libzinc ${Boost_LIBRARIES} -lstdc++ -lgcov)
        target_compile_options(${test_name} PRIVATE -fsanitize=address -g -O0 -fprofile-arcs -ftest-coverage)
        add_test(
            NAME ${test_name}
            COMMAND ${test_name}
        )
    endforeach (test_source_file)
endif ()

if (ZINC_WITH_COVERAGE)
    set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMake)
    include(CodeCoverage)
    setup_target_for_coverage(coverage test_driver coverage)
endif ()