#!/bin/sh

# $FreeBSD: src/tools/regression/usr.bin/make/syntax/enl/test.t,v 1.1.12.1 2008/10/02 02:57:24 kensmith Exp $

cd `dirname $0`
. ../../common.sh

# Description
DESC="Test escaped new-lines handling."

# Run
TEST_N=2
TEST_2_TODO="bug in parser"

eval_cmd $*
