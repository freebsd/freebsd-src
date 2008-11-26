#!/bin/sh

# $FreeBSD: src/tools/regression/usr.bin/make/basic/t3/test.t,v 1.2.16.1 2008/10/02 02:57:24 kensmith Exp $

cd `dirname $0`
. ../../common.sh

# Description
DESC="No Makefile file, no command line target."

# Run
TEST_N=1
TEST_1=

eval_cmd $*
