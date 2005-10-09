#!/bin/sh

# $FreeBSD$

cd `dirname $0`
. ../../common.sh

# Description
DESC="Test escaped new-lines handling."

# Run
TEST_N=2
TEST_2_TODO="bug in parser"

eval_cmd $*
