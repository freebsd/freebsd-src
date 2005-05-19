#!/bin/sh

# $FreeBSD$

cd `dirname $0`
. ../../common.sh

desc_test()
{
	echo "Select the builtin sh shell."
}

eval_cmd $1
