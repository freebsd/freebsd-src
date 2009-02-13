#!/bin/sh
#
# $FreeBSD$
#

DIR=`dirname $0`
ARCH=`uname -m`

check()
{
	NUM=$1
	shift
	# Remove tty field, which varies between systems.
	awk '{$4 = ""; print}' |
	if diff -q - $DIR/$1
	then
		echo "ok $NUM"
	else
		echo "not ok $NUM"
	fi
}


cat $DIR/v1-$ARCH-acct.in $DIR/v2-$ARCH-acct.in >$DIR/v1v2-$ARCH-acct.in
cat $DIR/v2-$ARCH.out $DIR/v1-$ARCH.out >$DIR/v1v2-$ARCH.out

echo 1..6

lastcomm -cesuS -f $DIR/v1-$ARCH-acct.in | check 1 v1-$ARCH.out
lastcomm -cesuS -f - <$DIR/v1-$ARCH-acct.in | tail -r | check 2 v1-$ARCH.out
lastcomm -cesuS -f $DIR/v2-$ARCH-acct.in | check 3 v2-$ARCH.out
lastcomm -cesuS -f - <$DIR/v2-$ARCH-acct.in | tail -r | check 4 v2-$ARCH.out
lastcomm -cesuS -f $DIR/v1v2-$ARCH-acct.in | check 5 v1v2-$ARCH.out
lastcomm -cesuS -f - <$DIR/v1v2-$ARCH-acct.in | tail -r | check 6 v1v2-$ARCH.out

rm $DIR/v1v2-$ARCH-acct.in
rm $DIR/v1v2-$ARCH.out

exit 0
