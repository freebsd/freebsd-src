#!/bin/sh

# $FreeBSD$

cd `dirname $0`
. ../../common.sh

setup_test()
{
	cat > ${WORK_DIR}/Makefile << _EOF_
all:
_EOF_
}

desc_test()
{
	echo "A Makefile file with only a 'all:' file dependency specification."
}

eval_cmd $1
