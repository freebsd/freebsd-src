#!/bin/sh

# $FreeBSD: src/tools/regression/usr.bin/make/basic/t3/test.t,v 1.2.22.1.6.1 2010/12/21 17:09:25 kensmith Exp $

cd `dirname $0`
. ../../common.sh

# Description
DESC="No Makefile file, no command line target."

# Run
TEST_N=1
TEST_1=

eval_cmd $*
