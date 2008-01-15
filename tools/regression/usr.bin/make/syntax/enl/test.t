#!/bin/sh

# $FreeBSD: src/tools/regression/usr.bin/make/syntax/enl/test.t,v 1.1 2005/06/01 11:26:47 harti Exp $

cd `dirname $0`
. ../../common.sh

# Description
DESC="Test escaped new-lines handling."

# Run
TEST_N=2
TEST_2_TODO="bug in parser"

eval_cmd $*
