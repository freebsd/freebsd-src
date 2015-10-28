import distutils.ccompiler
import distutils.sysconfig
from distutils.core import setup, Extension
import os


compiler  = distutils.ccompiler.new_compiler()
search_paths=[os.path.expanduser('~/{}'), '/opt/local/{}', '/usr/local/{}', '/usr/{}']
lib_paths = [ a.format("lib") for a in search_paths]
inc_paths = [ a.format("include") for a in search_paths]

uclmodule = Extension('ucl',
        include_dirs = inc_paths,
        library_dirs = lib_paths,
        libraries = ['ucl'],
        sources = ['src/uclmodule.c'],
        runtime_library_dirs = lib_paths,
        language='c')

setup(name='ucl',
    version='1.0',
    description='ucl parser and emmitter',
    ext_modules = [uclmodule],
    author="Eitan Adler",
    author_email="lists@eitanadler.com",
    url="https://github.com/vstakhov/libucl/",
    license="MIT",
    classifiers=["Development Status :: 3 - Alpha",
        "Intended Audience :: Developers",
        "License :: DFSG approved",
        "License :: OSI Approved :: MIT License",
        "Programming Language :: C",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: Implementation :: CPython",
        "Topic :: Software Development :: Libraries",
        ]
    )
