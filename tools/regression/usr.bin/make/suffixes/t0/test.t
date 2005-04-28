#!/bin/sh

# $FreeBSD$

cd `dirname $0`
. ../../common.sh

setup_test()
{
	cp TEST1.a $WORK_DIR
}

run_test()
{
	cd $WORK_DIR
        $MAKE_PROG -r 1>stdout 2>stderr
        echo $? >status
}

clean_test()
{
	rm -f TEST1.b
}

desc_test()
{
	echo "Basic suffix operation."
}

eval_cmd $1
