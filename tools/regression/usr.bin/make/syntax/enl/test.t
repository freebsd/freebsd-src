#!/bin/sh

# $FreeBSD: src/tools/regression/usr.bin/make/syntax/enl/test.t,v 1.2.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

cd `dirname $0`
. ../../common.sh

# Description
DESC="Test escaped new-lines handling."

# Run
TEST_N=5
TEST_2_TODO="bug in parser"

eval_cmd $*
