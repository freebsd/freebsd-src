#!/bin/sh

# $FreeBSD$

cd `dirname $0`
. ../../common.sh

setup_test()
{
	cp TEST[12].a ${WORK_DIR}
}

run_test()
{
	cd ${WORK_DIR}
        ${MAKE_PROG} -r 1>stdout 2>stderr
        echo $? >status
}

clean_test()
{
	rm -f TEST1.b
}

desc_test()
{
	echo "Source wildcard expansion."
}

eval_cmd $1
