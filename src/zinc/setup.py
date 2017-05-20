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
import sys
from setuptools import setup, Extension
from distutils.command.build_ext import build_ext

cflags = {
    'mingw32': [],
    'unix': []
}
lflags = {}
build_definitions = [
    ('ZINC_WITH_EXCEPTIONS', '1')
]

if '--strong-hash-fnv' in sys.argv:
    sys.argv.remove('--strong-hash-fnv')
    build_definitions.append(('ZINC_WITH_STRONG_HASH_FNV', '1'))
else:
    build_definitions.append(('ZINC_WITH_STRONG_HASH_SHA1', '1'))

if '--force-cxx11' in sys.argv:
    sys.argv.remove('--force-cxx11')
    for compiler in ('mingw32', 'unix'):
        cflags[compiler].append('-std=c++11')
else:
    for compiler in ('mingw32', 'unix'):
        cflags[compiler].append('-std=c++14')


class CompilerBuildFlagManager(build_ext):
    def build_extensions(self):
        compiler_type = self.compiler.compiler_type
        if compiler_type in cflags:
            for ext in self.extensions:
                ext.extra_compile_args = cflags[compiler_type]
        if compiler_type in lflags:
            for ext in self.extensions:
                ext.extra_link_args = lflags[compiler_type]
        build_ext.build_extensions(self)

setup(
    name='zinc',
    version='0.0.1',
    author='Rokas Kupstys',
    description='Example utility on usage of libzinc',
    license='MIT',
    url='https://github.com/rokups/zinc',
    classifiers=[
        'Development Status :: 3 - Alpha',
        'Environment :: Console',
        'Intended Audience :: Developers',
        'Topic :: Utilities',
        'License :: OSI Approved :: MIT License',
        'Topic :: Internet :: WWW/HTTP',
    ],
    scripts=['zinc'],
    ext_modules=[
        Extension(
            'pyzinc',
            ['pyzinc.cpp', '../libzinc/zinc.cpp', '../libzinc/sha1.c', '../libzinc/Utilities.cpp'],
            include_dirs=[
                '../../include',
                '../../external/pybind11/include',
                '../../external/flat_hash_map'
            ],
            define_macros=build_definitions,
            language='c++'
        ),
    ],
    cmdclass={'build_ext': CompilerBuildFlagManager}
)
