#!/bin/sh
# $FreeBSD: src/tools/regression/lib/libc/nss/test-getusershell.t,v 1.1.6.1 2008/11/25 02:59:29 kensmith Exp $

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
