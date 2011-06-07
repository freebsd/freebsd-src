#!/bin/sh
#
# $FreeBSD$
#

DIR=`dirname $0`
LCDIR=`dirname $0`/../../usr.bin/lastcomm
ARCH=`uname -m`

check()
{
	NUM=$1
	shift
	if diff -q - $1
	then
		echo "ok $NUM"
	else
		echo "not ok $NUM"
	fi
}

cp $LCDIR/v1-$ARCH-acct.in $DIR/v1-$ARCH-acct.in
cp $LCDIR/v2-$ARCH-acct.in $DIR/v2-$ARCH-acct.in

echo 1..13

# Command listings of the two acct versions
sa -u $DIR/v1-$ARCH-acct.in | check 1 $DIR/v1-$ARCH-u.out
sa -u $DIR/v2-$ARCH-acct.in | check 2 $DIR/v2-$ARCH-u.out

# Plain summaries of user/process
sa -i $DIR/v1-$ARCH-acct.in | check 3 $DIR/v1-$ARCH-sav.out
sa -im $DIR/v1-$ARCH-acct.in | check 4 $DIR/v1-$ARCH-usr.out

# Backward compatibility of v1 summary files
sa -P $DIR/v1-$ARCH-sav.in -U $DIR/v1-$ARCH-usr.in /dev/null |
	check 5 $DIR/v1-$ARCH-sav.out
sa -m -P $DIR/v1-$ARCH-sav.in -U $DIR/v1-$ARCH-usr.in /dev/null |
	check 6 $DIR/v1-$ARCH-usr.out

# Convert old summary format to new 
cp $DIR/v1-$ARCH-sav.in $DIR/v2c-$ARCH-sav.in
cp $DIR/v1-$ARCH-usr.in $DIR/v2c-$ARCH-usr.in
sa -s -P $DIR/v2c-$ARCH-sav.in -U $DIR/v2c-$ARCH-usr.in /dev/null >/dev/null
sa -P $DIR/v2c-$ARCH-sav.in -U $DIR/v2c-$ARCH-usr.in /dev/null |
	check 7 $DIR/v1-$ARCH-sav.out
sa -m -P $DIR/v2c-$ARCH-sav.in -U $DIR/v2c-$ARCH-usr.in /dev/null |
	check 8 $DIR/v1-$ARCH-usr.out

# Reading v2 summary files
sa -P $DIR/v2-$ARCH-sav.in -U $DIR/v2-$ARCH-usr.in /dev/null |
	check 9 $DIR/v1-$ARCH-sav.out
sa -m -P $DIR/v2-$ARCH-sav.in -U $DIR/v2-$ARCH-usr.in /dev/null |
	check 10 $DIR/v1-$ARCH-usr.out

# Summarize
sa -is -P $DIR/v2c-$ARCH-sav.in -U $DIR/v2c-$ARCH-usr.in $DIR/v1-$ARCH-acct.in >/dev/null
sa -P $DIR/v2c-$ARCH-sav.in -U $DIR/v2c-$ARCH-usr.in /dev/null |
	check 11 $DIR/v1-$ARCH-sav.out
sa -m -P $DIR/v2c-$ARCH-sav.in -U $DIR/v2c-$ARCH-usr.in /dev/null |
	check 12 $DIR/v1-$ARCH-usr.out

# Accumulate
cp $LCDIR/v1-$ARCH-acct.in $DIR/v1-$ARCH-acct.in
sa -is -P $DIR/v2c-$ARCH-sav.in -U $DIR/v2c-$ARCH-usr.in $DIR/v1-$ARCH-acct.in >/dev/null
cp $LCDIR/v1-$ARCH-acct.in $DIR/v1-$ARCH-acct.in
sa -s -P $DIR/v2c-$ARCH-sav.in -U $DIR/v2c-$ARCH-usr.in $DIR/v1-$ARCH-acct.in >$DIR/double
cp $LCDIR/v1-$ARCH-acct.in $DIR/v1-$ARCH-acct.in
sa -i $DIR/v1-$ARCH-acct.in $DIR/v1-$ARCH-acct.in | check 13 $DIR/double

# Clean up
rm $DIR/double $DIR/v2c-$ARCH-usr.in $DIR/v2c-$ARCH-sav.in $DIR/v1-$ARCH-acct.in $DIR/v2-$ARCH-acct.in

exit 0
