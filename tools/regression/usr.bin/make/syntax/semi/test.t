#!/bin/sh

# $FreeBSD: src/tools/regression/usr.bin/make/syntax/semi/test.t,v 1.1.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

cd `dirname $0`
. ../../common.sh

# Description
DESC="Test semicolon handling."

# Run
TEST_N=2
TEST_1_TODO="parser bug"

eval_cmd $*
