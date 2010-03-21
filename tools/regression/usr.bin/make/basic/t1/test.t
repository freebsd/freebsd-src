#!/bin/sh

# $FreeBSD: src/tools/regression/usr.bin/make/basic/t1/test.t,v 1.2.24.1 2010/02/10 00:26:20 kensmith Exp $

cd `dirname $0`
. ../../common.sh

# Description
DESC="A Makefile file with only a 'all:' file dependency specification."

# Run
TEST_N=1
TEST_1=

eval_cmd $*
