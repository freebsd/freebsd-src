#!/bin/sh

# $FreeBSD$

cd `dirname $0`
. ../../common.sh

setup_test()
{
	cp libtest.a ${WORK_DIR}
}

desc_test()
{
	echo "Archive parsing (old BSD format)."
}

eval_cmd $1
