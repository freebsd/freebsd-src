#!/bin/sh

# $FreeBSD: src/tools/regression/usr.bin/make/shell/replace/test.t,v 1.2.20.1 2009/04/15 03:14:26 kensmith Exp $

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
