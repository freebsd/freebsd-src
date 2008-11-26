#!/bin/sh

# $FreeBSD: src/tools/regression/usr.bin/make/archives/fmt_oldbsd/test.t,v 1.1.16.1 2008/10/02 02:57:24 kensmith Exp $

cd `dirname $0`
. ../../common.sh

# Description
DESC="Archive parsing (old BSD format)."

# Setup
TEST_COPY_FILES="libtest.a 644"

# Run
TEST_N=7

eval_cmd $*
