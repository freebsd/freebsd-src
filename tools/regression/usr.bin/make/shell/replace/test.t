#!/bin/sh

# $FreeBSD$

cd `dirname $0`
. ../../common.sh

desc_test()
{
	echo "Check that the shell can be replaced."
}

eval_cmd $1
