#!/bin/sh

# $FreeBSD: src/tools/regression/usr.bin/make/archives/fmt_44bsd_mod/test.t,v 1.1.20.1 2009/04/15 03:14:26 kensmith Exp $

cd `dirname $0`
. ../../common.sh

# Description
DESC="Archive parsing (modified BSD4.4 format)."

# Setup
TEST_COPY_FILES="libtest.a 644"

# Run
TEST_N=7

eval_cmd $*
