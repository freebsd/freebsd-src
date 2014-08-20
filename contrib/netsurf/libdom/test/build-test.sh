#!/bin/bash
#
# This is a simple script to recompile a C test file.
# Usage:
# This script is designed to run under test output directory.
# 
# You should firstly run "run-test.sh", which will genrate a test output directory. In that
# directory, there are C source files and corresponding executables.
#
# ../../../build-test.sh some-test-converted-c-file.c
#
#  This file is part of libdom test suite.
#  Licensed under the MIT License,
#                 http://www.opensource.org/licenses/mit-license.php
#  Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>

src="testutils/comparators.c testutils/domtsasserts.c testutils/foreach.c testutils/list.c testutils/load.c testutils/utils.c testutils/domtscondition.c"
domdir="../build-Linux-Linux-debug-lib-static"
ldflags="-L$domdir -ldom -L/usr/local/lib  -lwapcaplet -L/usr/lib -lxml2 -lhubbub -lparserutils"
#ldflags="-L/usr/lib -lm -lz -L$domdir -ldom -L/usr/local/lib  -lwapcaplet -lxml2 -lhubbub -lparserutils"
cflags="-Itestutils/ -I../bindings/xml  -I../include -I../bindings/hubbub -I/usr/local/include"

sf="$1";
echo $sf;
cwd=$(pwd);
cd ../../../
exe=${sf%%.c};
cfile="$cwd"/"$sf";
gcc -g $cflags $src $cfile $ldflags -o $exe;
mv $exe $cwd;
cd $cwd;
