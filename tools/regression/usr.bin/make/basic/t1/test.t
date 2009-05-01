#!/bin/sh

# $FreeBSD: src/tools/regression/usr.bin/make/basic/t1/test.t,v 1.2.20.1 2009/04/15 03:14:26 kensmith Exp $

cd `dirname $0`
. ../../common.sh

# Description
DESC="A Makefile file with only a 'all:' file dependency specification."

# Run
TEST_N=1
TEST_1=

eval_cmd $*
