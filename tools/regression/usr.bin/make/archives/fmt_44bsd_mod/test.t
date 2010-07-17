#!/bin/sh

# $FreeBSD: src/tools/regression/usr.bin/make/archives/fmt_44bsd_mod/test.t,v 1.1.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $

cd `dirname $0`
. ../../common.sh

# Description
DESC="Archive parsing (modified BSD4.4 format)."

# Setup
TEST_COPY_FILES="libtest.a 644"

# Run
TEST_N=7

eval_cmd $*
