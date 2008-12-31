#!/bin/sh

# $FreeBSD: src/tools/regression/usr.bin/make/basic/t0/test.t,v 1.2.18.1 2008/11/25 02:59:29 kensmith Exp $

cd `dirname $0`
. ../../common.sh

# Description
DESC="An empty Makefile file and no target given."

# Setup
TEST_TOUCH="Makefile ''"

# Run
TEST_N=1
TEST_1=

eval_cmd $*
