#!/bin/sh

# $FreeBSD$

cd `dirname $0`
. ../../common.sh

desc_test()
{
	echo "Check that a command line with a builtin is passed to the shell."
}

eval_cmd $1
