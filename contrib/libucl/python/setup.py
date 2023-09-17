try:
    from setuptools import setup, Extension
    # setuptools doesn't support template param for MANIFEST.in
    from setuptools.command.egg_info import manifest_maker
    manifest_maker.template = 'python/MANIFEST.in'
except ImportError:
    from distutils.core import setup, Extension

import os
import sys

LIB_ROOT = os.path.abspath(os.path.join(__file__, os.pardir, os.pardir))  
if os.getcwd() != LIB_ROOT:
    os.chdir(LIB_ROOT)
if LIB_ROOT not in sys.path:
    sys.path.append(LIB_ROOT)

tests_require = []

if sys.version < '2.7':
    tests_require.append('unittest2')

uclmodule = Extension(
    'ucl',
    libraries=['ucl', 'curl'],
    sources=['python/src/uclmodule.c'],
    include_dirs=['include'],
    language='c',
)

ucl_lib = {
    'sources': ['src/' + fn for fn in os.listdir('src') if fn.endswith('.c')],
    'include_dirs': ['include', 'src', 'uthash', 'klib'],
    'macros': [('CURL_FOUND', '1')],
}

# sdist setup() will pull in the *.c files automatically, but not headers
# MANIFEST.in will include the headers for sdist only
template = 'python/MANIFEST.in'

# distutils assume setup.py is in the root of the project
# we need to include C source from the parent so trick it
in_ucl_root = 'setup.py' in os.listdir('python')
if in_ucl_root:
    os.link('python/setup.py', 'setup.py')

setup(
    name = 'ucl',
    version = '0.8.1',
    description = 'ucl parser and emitter',
    ext_modules = [uclmodule],
    template=template, # no longer supported with setuptools but doesn't hurt
    libraries = [('ucl', ucl_lib)],
    test_suite = 'tests',
    tests_require = tests_require,
    author = "Eitan Adler, Denis Volpato Martins",
    author_email = "lists@eitanadler.com",
    url = "https://github.com/vstakhov/libucl/",
    license = "MIT",
    classifiers = [
        "Development Status :: 3 - Alpha",
        "Intended Audience :: Developers",
        "License :: DFSG approved",
        "License :: OSI Approved :: MIT License",
        "Programming Language :: C",
        "Programming Language :: Python :: 2",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: Implementation :: CPython",
        "Topic :: Software Development :: Libraries",
    ]
)

# clean up the trick after the build
if in_ucl_root:
    os.unlink("setup.py")
