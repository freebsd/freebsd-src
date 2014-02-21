#!/bin/sh
# $Id: run_test.sh,v 1.8 2012/01/15 11:50:35 tom Exp $
# vi:ts=4 sw=4:

if test $# = 1
then
	PROG_DIR=`pwd`
	TEST_DIR=$1
else
	PROG_DIR=..
	TEST_DIR=.
fi

YACC=$PROG_DIR/yacc

tmpfile=temp$$
rm -f test-*

echo '** '`date`
for input in ${TEST_DIR}/*.y
do
	case $input in
	test*)
		echo "?? ignored $input"
		;;
	*)
		root=`basename $input .y`
		ROOT="test-$root"
		prefix=${root}_

		OPTS=
		OPT2=
		TYPE=".output .tab.c .tab.h"
		case $input in
		${TEST_DIR}/code_*)
			OPTS="$OPTS -r"
			TYPE="$TYPE .code.c"
			prefix=`echo "$prefix" | sed -e 's/^code_//'`
			;;
		${TEST_DIR}/pure_*)
			OPTS="$OPTS -P"
			prefix=`echo "$prefix" | sed -e 's/^pure_//'`
			;;
		${TEST_DIR}/quote_*)
			OPT2="-s"
			;;
		esac

		for opt2 in "" $OPT2
		do
			$YACC $OPTS $opt2 -v -d -p $prefix -b $ROOT${opt2} $input
			for type in $TYPE
			do
				REF=${TEST_DIR}/${root}${opt2}${type}
				CMP=${ROOT}${opt2}${type}
				if test ! -f $CMP
				then
					echo "...not found $CMP"
				else
					sed	-e s,$CMP,$REF, \
						-e /YYPATCH/d \
						-e 's,#line \([1-9][0-9]*\) "'$TEST_DIR'/,#line \1 ",' \
						< $CMP >$tmpfile \
						&& mv $tmpfile $CMP
					if test ! -f $REF
					then
						mv $CMP $REF
						echo "...saved $REF"
					elif ( cmp -s $REF $CMP )
					then
						echo "...ok $REF"
						rm -f $CMP
					else
						echo "...diff $REF"
						diff -u $REF $CMP
					fi
				fi
			done
		done
		;;
	esac
done
