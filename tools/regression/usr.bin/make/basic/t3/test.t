#!/bin/sh

# $FreeBSD$

cd `dirname $0`
. ../../common.sh

setup_test()
{
	rm -f ${WORK_DIR}/Makefile
}

desc_test()
{
	echo "No Makefile file."
}

eval_cmd $1
