#!/bin/sh

# $FreeBSD: src/tools/regression/usr.bin/make/shell/path_select/test.t,v 1.2.18.1 2008/11/25 02:59:29 kensmith Exp $

cd `dirname $0`
. ../../common.sh

# Description
DESC="New path for builtin shells (2)."

# Setup
TEST_COPY_FILES="shell 755"

# Run
TEST_N=3
TEST_1="sh_test"
TEST_2="csh_test"
TEST_3="ksh_test"
TEST_3_SKIP="no ksh on FreeBSD"

eval_cmd $*
