#!/bin/sh

# $FreeBSD: src/tools/regression/usr.bin/make/syntax/semi/test.t,v 1.1.8.1 2009/04/15 03:14:26 kensmith Exp $

cd `dirname $0`
. ../../common.sh

# Description
DESC="Test semicolon handling."

# Run
TEST_N=2
TEST_1_TODO="parser bug"

eval_cmd $*
