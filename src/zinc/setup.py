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
from Tools.scripts.objgraph import definitions

from setuptools import setup, Extension

cpp_args = ['-std=c++11']
setup(
    name='zinc',
    version='0.0.1',
    author='Rokas Kupstys',
    description='Example utility on usage of libzinc',
    license='MIT',
    url='https://github.com/rokups/zinc',
    classifiers=[
        'Development Status :: 3 - Alpha',
        'Topic :: Utilities',
        'License :: OSI Approved :: MIT License',
    ],
    scripts=['zinc'],
    ext_modules=[
        Extension(
            'pyzinc',
            ['pyzinc.cpp', '../libzinc/zinc.cpp', '../libzinc/sha1.c', '../libzinc/Utilities.cpp'],
            include_dirs=[
                '../../include',
                '../../external/pybind11/include'
            ],
            define_macros=[('ZINC_FNV', '1')],
            language='c++',
            extra_compile_args=cpp_args,
        ),
    ],
)
