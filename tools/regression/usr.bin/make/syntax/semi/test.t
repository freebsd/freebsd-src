#!/bin/sh

# $FreeBSD: src/tools/regression/usr.bin/make/syntax/semi/test.t,v 1.1.12.1 2010/02/10 00:26:20 kensmith Exp $

cd `dirname $0`
. ../../common.sh

# Description
DESC="Test semicolon handling."

# Run
TEST_N=2
TEST_1_TODO="parser bug"

eval_cmd $*
