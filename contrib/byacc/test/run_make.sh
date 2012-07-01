#!/bin/sh
# $Id: run_make.sh,v 1.9 2012/01/15 22:35:01 tom Exp $
# vi:ts=4 sw=4:

# do a test-compile on each of the ".c" files in the test-directory

BISON=`bison --version 2>/dev/null | head -n 1 | sed -e 's/^[^0-9.]*//' -e 's/[^0-9.]*$//'`

if test $# = 1
then
	PROG_DIR=`pwd`
	TEST_DIR=$1
else
	PROG_DIR=..
	TEST_DIR=.
fi

MY_MAKE="make -f $PROG_DIR/makefile srcdir=$PROG_DIR VPATH=$TEST_DIR"

echo '** '`date`
for input in ${TEST_DIR}/*.c
do
	test -f "$input" || continue

	obj=`basename "$input" .c`.o

	$MY_MAKE $obj C_FILES=$input
	test -f $obj && rm $obj

	DEFS=
	case $input in #(vi
	${TEST_DIR}/pure_*)
		# DEFS="-DYYLEX_PARAM=flag -DYYLEX_PARAM_TYPE=int"
		;;
	esac

	if test "x$DEFS" != "x"
	then
		$MY_MAKE $obj C_FILES=$input DEFINES="$DEFS"
		test -f $obj && rm -f $obj
	fi
done

if test -n "$BISON"
then
	echo "** compare with bison $BISON"
	for input in ${TEST_DIR}/*.y
	do
		test -f "$input" || continue

		# Bison does not support pure-parser from command-line.
		# Also, its support for %expect is generally broken.
		# Work around these issues using a temporary file.

		echo "... testing $input"
		rm -f run_make.[coy]

		case $input in
		pure_*)
			if test -z `fgrep -l '%pure-parser' $input`
			then
				echo "%pure-parser" >>run_make.y
			fi
			;;
		esac

		sed -e '/^%expect/s,%expect.*,,' $input >>run_make.y

		bison -y run_make.y
		sed -e '/^#line/s,"run_make.y","'$input'",' y.tab.c >run_make.c

		rm -f y.tab.c

		input=run_make.c
		object=run_make.o
		if test -f $input
		then
			$MY_MAKE $object DEFINES='-DYYENABLE_NLS=0 -DYYLTYPE_IS_TRIVIAL=1 -DYYSTACK_USE_ALLOCA=0 -DYYMAXDEPTH=0'
		else
			echo "?? $input not found"
		fi
		rm -f run_make.[coy]
	done
fi

YACC=
for name in /usr/ccs/bin/yacc
do
	if test -f $name
	then
		YACC=$name
	fi
done

if test -n "$YACC"
then
	echo "** compare with $YACC"
	for input in ${TEST_DIR}/*.y
	do
		test -f "$input" || continue

		echo "... testing $input"
		rm -f run_make.[coy]

		case $input in
		pure_*)
			echo "... skipping $input"
			continue;
			;;
		*)
			if fgrep '%pure-parser' $input >/dev/null ||
			   fgrep '%parse-param' $input >/dev/null ||
			   fgrep '%lex-param' $input >/dev/null ||
			   fgrep 'YYLEX_PARAM' $input >/dev/null
			then
				echo "... skipping $input"
				continue;
			fi
			;;
		esac

		sed -e '/^%expect/s,%expect.*,,' $input >>run_make.y

		$YACC run_make.y
		sed -e '/^#line/s,"run_make.y","'$input'",' y.tab.c >run_make.c

		rm -f y.tab.c

		input=run_make.c
		object=run_make.o
		if test -f $input
		then
			$MY_MAKE $object
		else
			echo "?? $input not found"
		fi
		rm -f run_make.[coy]
	done
fi
