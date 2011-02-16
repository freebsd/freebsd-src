#!/bin/sh

# $FreeBSD: src/tools/regression/usr.bin/make/basic/t2/test.t,v 1.2.26.1 2010/12/21 17:10:29 kensmith Exp $

cd `dirname $0`
. ../../common.sh

# Description
DESC="A Makefile file with only a 'all:' file dependency specification, and shell command."

# Run
TEST_N=1
TEST_1=

eval_cmd $*
