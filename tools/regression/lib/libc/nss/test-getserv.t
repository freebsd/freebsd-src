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
do_test 1 'getservbyname()'        '-n'
do_test 2 'getservbyport()'        '-p'
do_test 3 'getservent()'           '-e'
do_test 4 'getservent() 2-pass'    '-2'
do_test 5 'building snapshot, if needed'  '-s snapshot_serv'
do_test 6 'getservbyname() snapshot'      '-n -s snapshot_serv'
do_test 7 'getservbyport() snapshot'      '-p -s snapshot_serv'
do_test 8 'getservent() snapshot'         '-e -s snapshot_serv'
