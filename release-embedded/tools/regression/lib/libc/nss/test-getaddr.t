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

echo 1..6
#Tests with hints.ai_family is set to PF_UNSPEC
do_test 1 'getaddrinfo() (PF_UNSPEC)' '-f mach'
do_test 2 'getaddrinfo() snapshot (PF_UNSPEC)' '-f mach -s snapshot_ai'

#Tests with hints.ai_family is set to PF_INET
do_test 3 'getaddrinfo() (PF_INET)' '-f mach'
do_test 4 'getaddrinfo() snapshot (PF_INET)' '-4 -f mach -s snapshot_ai4'

#Tests with hints.ai_family is set to PF_INET6
do_test 5 'getaddrinfo() (PF_INET6)' '-f mach'
do_test 6 'getaddrinfo() snapshot (PF_INET6)' '-6 -f mach -s snapshot_ai6'

