#!/bin/sh
# $FreeBSD$

do_test() {
	number=$1
	comment=$2
	opt=$3
	if ./$executable $opt; then
		echo "ok $number - $comment"
	else
		echo "not ok $number - $comment"
	fi
}

cd `dirname $0`

executable=`basename $0 .t`

make $executable 2>&1 > /dev/null

echo 1..8
do_test 1 'getrpcbyname()'        '-n'
do_test 2 'getrpcbynumber()'      '-v'
do_test 3 'getrpcent()'           '-e'
do_test 4 'getrpcent() 2-pass'    '-2'
do_test 5 'building snapshot, if needed'  '-s snapshot_rpc'
do_test 6 'getrpcbyname() snapshot'       '-n -s snapshot_rpc'
do_test 7 'getrpcbynumber() snapshot'     '-v -s snapshot_rpc'
do_test 8 'getrpcent() snapshot'          '-e -s snapshot_rpc'
