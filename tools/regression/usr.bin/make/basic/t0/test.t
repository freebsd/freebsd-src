#!/bin/sh

# $FreeBSD$

cd `dirname $0`
. ../../common.sh

setup_test()
{
	cp /dev/null ${WORK_DIR}/Makefile
}

desc_test()
{
	echo "An empty Makefile file."
}

eval_cmd $1
