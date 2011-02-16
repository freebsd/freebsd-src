#!/bin/sh

# $FreeBSD: src/tools/regression/usr.bin/make/shell/replace/test.t,v 1.2.22.1.6.1 2010/12/21 17:09:25 kensmith Exp $

cd `dirname $0`
. ../../common.sh

# Description
DESC="Check that the shell can be replaced."

# Setup
TEST_COPY_FILES="shell 755"

# Run
TEST_N=2
TEST_1=
TEST_2=-j2

eval_cmd $*
