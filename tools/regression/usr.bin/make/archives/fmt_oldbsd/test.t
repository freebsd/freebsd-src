#!/bin/sh

# $FreeBSD: src/tools/regression/usr.bin/make/archives/fmt_oldbsd/test.t,v 1.1.26.1 2010/12/21 17:10:29 kensmith Exp $

cd `dirname $0`
. ../../common.sh

# Description
DESC="Archive parsing (old BSD format)."

# Setup
TEST_COPY_FILES="libtest.a 644"

# Run
TEST_N=7

eval_cmd $*
