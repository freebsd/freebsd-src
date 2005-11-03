#!/bin/sh

# $FreeBSD: src/tools/regression/usr.bin/make/shell/path/test.t,v 1.2 2005/05/31 14:13:03 harti Exp $

cd `dirname $0`
. ../../common.sh

# Description
DESC="New path for builtin shells."

# Setup
TEST_COPY_FILES="sh 755"
TEST_LINKS="sh csh	sh ksh"

# Run
TEST_N=3
TEST_1="sh_test"
TEST_2="csh_test"
TEST_3="ksh_test"
TEST_3_SKIP="no ksh on FreeBSD"

eval_cmd $*
