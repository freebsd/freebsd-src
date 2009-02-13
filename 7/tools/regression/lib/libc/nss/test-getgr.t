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
do_test 1 'getgrnam()'        '-n'
do_test 2 'getgrgid()'        '-g'
do_test 3 'getgrent()'           '-e'
do_test 4 'getgrent() 2-pass'    '-2'
do_test 5 'building snapshot, if needed'  '-s snapshot_grp'
do_test 6 'getgrnam() snapshot'           '-n -s snapshot_grp'
do_test 7 'getgrgid() snapshot'           '-g -s snapshot_grp'
do_test 8 'getgrent() snapshot'           '-e -s snapshot_grp'
