#!/bin/sh

# $FreeBSD: src/tools/regression/usr.bin/make/shell/replace/test.t,v 1.2.16.1 2008/10/02 02:57:24 kensmith Exp $

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
