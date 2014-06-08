#!/usr/bin/env python
# encoding: utf-8

import os
import sys
import textwrap

from os.path import abspath, dirname, join as pjoin
from distutils.command.build import build
from setuptools.command.develop import develop
from setuptools.command.bdist_egg import bdist_egg
from setuptools import setup, Extension, Command


here = abspath(dirname(__file__))

classifiers = (
    'Development Status :: 5 - Production/Stable',
    'Programming Language :: Python :: 2.7',
    'Programming Language :: Python :: 3',
    'Programming Language :: Python :: 3.2',
    'Programming Language :: Python :: 3.3',
    'Operating System :: POSIX :: Linux',
    'Intended Audience :: Developers',
    'Topic :: Software Development :: Libraries',
    'License :: OSI Approved :: BSD License',
    'Programming Language :: Python :: Implementation :: CPython',
)

cflags   = ['-std=c99']
input_c  = Extension('evdev._input',  sources=['evdev/input.c'],  extra_compile_args=cflags)
uinput_c = Extension('evdev._uinput', sources=['evdev/uinput.c'], extra_compile_args=cflags)
ecodes_c = Extension('evdev._ecodes', sources=['evdev/ecodes.c'], extra_compile_args=cflags)

kw = {
    'name':                 'evdev',
    'version':              '0.4.4',

    'description':          'Bindings for the linux input handling subsystem',
    'long_description':     open(pjoin(here, 'README.rst')).read(),

    'author':               'Georgi Valkov',
    'author_email':         'georgi.t.valkov@gmail.com',
    'license':              'Revised BSD License',

    'keywords':             'evdev input uinput',
    'classifiers':          classifiers,
    'url':                  'https://github.com/gvalkov/python-evdev',

    'packages':             ['evdev'],
    'ext_modules':          [input_c, uinput_c, ecodes_c],
    'tests_require':        ['pytest'],

    'include_package_data': False,
    'zip_safe':             True,
    'cmdclass':             {},
}


def create_ecodes():
    # :todo: expose as a command option
    header = '/usr/include/linux/input.h'

    if not os.path.isfile(header):
        msg = '''\
        The linux/input.h header file is missing. You will have to
        install the headers for your kernel in order to continue:

            yum install kernel-headers-$(uname -r)
            apt-get intall linux-headers-$(uname -r)
            pacman -S kernel-headers\n\n'''

        sys.stderr.write(textwrap.dedent(msg))
        sys.exit(1)

    from subprocess import check_call

    print('writing ecodes.c (using %s)' % header)
    cmd = '%s genecodes.py %s > ecodes.c' % (sys.executable, header)
    check_call(cmd, cwd="%s/evdev" % here, shell=True)


# :todo: figure out a smarter way to do this
# :note: subclassing build_ext doesn't really cut it
class BuildCommand(build):
    def run(self):
        create_ecodes()
        build.run(self)

class DevelopCommand(develop):
    def run(self):
        create_ecodes()
        develop.run(self)

class BdistEggCommand(bdist_egg):
    def run(self):
        create_ecodes()
        bdist_egg.run(self)


class PyTest(Command):
    '''setup.py test -> py.test tests'''

    user_options = []
    def initialize_options(self):
        pass

    def finalize_options(self):
        pass

    def run(self):
        from subprocess import call
        errno = call(('py.test', 'tests'))
        raise SystemExit(errno)


kw['cmdclass']['test'] = PyTest
kw['cmdclass']['build'] = BuildCommand
kw['cmdclass']['develop'] = DevelopCommand
kw['cmdclass']['bdist_egg'] = BdistEggCommand

if __name__ == '__main__':
    setup(**kw)
