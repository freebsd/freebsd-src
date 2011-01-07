#!/bin/sh
#
# $FreeBSD$
#

if test -z "${DIR}" ; then
	DIR=$( make -V .OBJDIR )
fi
if test -z "${DIR}" ; then
	DIR=$( dirname $0 )
fi

check()
{
	NUM=$1
	shift
	if $DIR/pack $*
	then
		echo "ok $NUM"
	else
		echo "not ok $NUM"
	fi
}

(cd $DIR ; make pack) >/dev/null 2>&1

echo 1..7

check 1 0 0
check 2 0 1
check 3 1 0
check 4 1 999999
check 5 1 1000000
check 6 2147483647 999999
check 7 10000000

(cd $DIR ; make clean) >/dev/null 2>&1

exit 0
