#!/usr/bin/env python2

"""
setup.py file for SWIG libfdt
Copyright (C) 2017 Google, Inc.
Written by Simon Glass <sjg@chromium.org>

Files to be built into the extension are provided in SOURCES
C flags to use are provided in CPPFLAGS
Object file directory is provided in OBJDIR
Version is provided in VERSION

If these variables are not given they are parsed from the Makefiles. This
allows this script to be run stand-alone, e.g.:

    ./pylibfdt/setup.py install [--prefix=...]
"""

from distutils.core import setup, Extension
import os
import re
import sys

# Decodes a Makefile assignment line into key and value (and plus for +=)
RE_KEY_VALUE = re.compile('(?P<key>\w+) *(?P<plus>[+])?= *(?P<value>.*)$')


def ParseMakefile(fname):
    """Parse a Makefile to obtain its variables.

    This collects variable assigments of the form:

        VAR = value
        VAR += more

    It does not pick out := assignments, as these are not needed here. It does
    handle line continuation.

    Returns a dict:
        key: Variable name (e.g. 'VAR')
        value: Variable value (e.g. 'value more')
    """
    makevars = {}
    with open(fname) as fd:
        prev_text = ''  # Continuation text from previous line(s)
        for line in fd.read().splitlines():
          if line and line[-1] == '\\':  # Deal with line continuation
            prev_text += line[:-1]
            continue
          elif prev_text:
            line = prev_text + line
            prev_text = ''  # Continuation is now used up
          m = RE_KEY_VALUE.match(line)
          if m:
            value = m.group('value') or ''
            key = m.group('key')

            # Appending to a variable inserts a space beforehand
            if 'plus' in m.groupdict() and key in makevars:
              makevars[key] += ' ' + value
            else:
              makevars[key] = value
    return makevars

def GetEnvFromMakefiles():
    """Scan the Makefiles to obtain the settings we need.

    This assumes that this script is being run from the top-level directory,
    not the pylibfdt directory.

    Returns:
        Tuple with:
            List of swig options
            Version string
            List of files to build
            List of extra C preprocessor flags needed
            Object directory to use (always '')
    """
    basedir = os.path.dirname(os.path.dirname(os.path.abspath(sys.argv[0])))
    swig_opts = ['-I%s' % basedir]
    makevars = ParseMakefile(os.path.join(basedir, 'Makefile'))
    version = '%s.%s.%s' % (makevars['VERSION'], makevars['PATCHLEVEL'],
                            makevars['SUBLEVEL'])
    makevars = ParseMakefile(os.path.join(basedir, 'libfdt', 'Makefile.libfdt'))
    files = makevars['LIBFDT_SRCS'].split()
    files = [os.path.join(basedir, 'libfdt', fname) for fname in files]
    files.append('pylibfdt/libfdt.i')
    cflags = ['-I%s' % basedir, '-I%s/libfdt' % basedir]
    objdir = ''
    return swig_opts, version, files, cflags, objdir


progname = sys.argv[0]
files = os.environ.get('SOURCES', '').split()
cflags = os.environ.get('CPPFLAGS', '').split()
objdir = os.environ.get('OBJDIR')
version = os.environ.get('VERSION')
swig_opts = []

# If we were called directly rather than through our Makefile (which is often
# the case with Python module installation), read the settings from the
# Makefile.
if not all((version, files, cflags, objdir)):
    swig_opts, version, files, cflags, objdir = GetEnvFromMakefiles()

libfdt_module = Extension(
    '_libfdt',
    sources = files,
    extra_compile_args = cflags,
    swig_opts = swig_opts,
)

setup(
    name='libfdt',
    version= version,
    author='Simon Glass <sjg@chromium.org>',
    description='Python binding for libfdt',
    ext_modules=[libfdt_module],
    package_dir={'': objdir},
    py_modules=['pylibfdt/libfdt'],
)
