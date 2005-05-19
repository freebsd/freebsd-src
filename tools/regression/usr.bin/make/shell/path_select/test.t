#!/bin/sh

# $FreeBSD$

cd `dirname $0`
. ../../common.sh

desc_test()
{
	echo "New path for builtin shells (2)."
}

eval_cmd $1
