#!/bin/sh

# $FreeBSD$

cd `dirname $0`
. ../../common.sh

setup_test()
{
	cat > ${WORK_DIR}/Makefile << _EOF_
all:
	echo hello
_EOF_
}

desc_test()
{
	echo "A Makefile file with only a 'all:' file dependency specification, and shell command."
}

eval_cmd $1
