#!/bin/sh

# $FreeBSD: src/tools/regression/usr.bin/make/suffixes/src_wild1/test.t,v 1.1.16.1 2008/10/02 02:57:24 kensmith Exp $

cd `dirname $0`
. ../../common.sh

# Description
DESC="Source wildcard expansion."

# Setup
TEST_COPY_FILES="TEST1.a 644	TEST2.a 644"

# Reset
TEST_CLEAN="TEST1.b"

# Run
TEST_N=1
TEST_1="-r test1"

eval_cmd $*
