#!/bin/sh
# $FreeBSD: src/tools/regression/lib/libc/nss/test-getusershell.t,v 1.1.8.1 2009/04/15 03:14:26 kensmith Exp $

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

echo 1..1
do_test 1 'getusershell() snapshot' '-s snapshot_usershell'
