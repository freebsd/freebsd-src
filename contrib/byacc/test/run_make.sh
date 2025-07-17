#!/bin/sh
# $Id: run_make.sh,v 1.21 2022/11/06 20:57:33 tom Exp $
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
THIS_DIR=`pwd`

: "${FGREP:=grep -F}"
ifBTYACC=`$FGREP -l 'define YYBTYACC' config.h > /dev/null; test $? != 0; echo $?`

if test "$ifBTYACC" = 0; then
	REF_DIR=${TEST_DIR}/yacc
else
	REF_DIR=${TEST_DIR}/btyacc
fi

MY_MAKE="make -f $PROG_DIR/makefile srcdir=$PROG_DIR"

run_make() {
	C_FILE=`basename "$1"`
	O_FILE=`basename "$C_FILE" .c`.o
	shift
	RETEST=`unset CDPATH; cd $TEST_DIR; pwd`
	cd "$REF_DIR"
	test -f "$I_FILE" && rm "$I_FILE"
	make -f "$PROG_DIR/makefile" EXTRA_CFLAGS=-I$RETEST srcdir="$PROG_DIR" "$O_FILE" "$@"
	test -f "$O_FILE" && rm "$O_FILE"
	cd "$THIS_DIR"
}

echo "** `date`"
echo "** program is in $PROG_DIR"
echo "** test-files in $REF_DIR"

for input in ${REF_DIR}/*.c
do
	case $input in #(vi
	${REF_DIR}/err_*|\
	${REF_DIR}/test-err_*)
		continue
		;;
	esac

	test -f "$input" || continue

	run_make "$input"

	DEFS=
	case $input in #(vi
	${REF_DIR}/pure_*)
		# DEFS="-DYYLEX_PARAM=flag -DYYLEX_PARAM_TYPE=int"
		;;
	esac

	if test "x$DEFS" != "x"
	then
		run_make "$input" DEFINES="$DEFS"
	fi
done

if test -n "$BISON"
then
	echo "** compare with bison $BISON"
	for input in ${TEST_DIR}/*.y
	do
		test -f "$input" || continue
		case $input in
		${TEST_DIR}/err_*|\
		${TEST_DIR}/test-err_*)
			continue
			;;
		${TEST_DIR}/ok_syntax*|\
		${TEST_DIR}/varsyntax*)
			# Bison does not support all byacc legacy syntax
			continue
			;;
		${TEST_DIR}/btyacc_*)
			# Bison does not support the btyacc []-action & inherited attribute extensions.
			continue
			;;
		esac

		# Bison does not support pure-parser from command-line.
		# Also, its support for %expect is generally broken.
		# Work around these issues using a temporary file.

		echo "... testing $input"
		rm -f run_make.[coy]

		case $input in
		${TEST_DIR}/pure_*)
			if test -z "`$FGREP -i -l '%pure-parser' "$input"`"
			then
				echo "%pure-parser" >>run_make.y
			fi
			;;
		esac

		sed -e '/^%expect/s,%expect.*,,' "$input" >>run_make.y

		case $BISON in
		[3-9].[0-9]*.[0-9]*)
			bison -Wno-other -Wno-conflicts-sr -Wconflicts-rr -y -Wno-yacc run_make.y
			;;
		*)
			bison -y run_make.y
			;;
		esac
		if test -f "y.tab.c"
		then
			sed -e '/^#line/s,"run_make.y","'"$input"'",' y.tab.c >run_make.c

			rm -f y.tab.c

			input=run_make.c
			object=run_make.o
			if test -f $input
			then
				$MY_MAKE $object DEFINES='-DYYENABLE_NLS=0 -DYYLTYPE_IS_TRIVIAL=1 -DYYSTACK_USE_ALLOCA=0 -DYYMAXDEPTH=0'
			else
				echo "?? $input not found"
			fi
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
			if $FGREP -i '%pure-parser' "$input" >/dev/null ||
			   $FGREP -i '%parse-param' "$input" >/dev/null ||
			   $FGREP -i '%lex-param' "$input" >/dev/null ||
			   $FGREP -i '%token-table' "$input" >/dev/null ||
			   $FGREP 'YYLEX_PARAM' "$input" >/dev/null
			then
				echo "... skipping $input"
				continue;
			fi
			;;
		esac

		sed -e '/^%expect/s,%expect.*,,' "$input" >>run_make.y

		$YACC run_make.y
		if test -f y.tab.c
		then
			sed -e '/^#line/s,"run_make.y","'"$input"'",' y.tab.c >run_make.c

			rm -f y.tab.c

			input=run_make.c
			object=run_make.o
			if test -f $input
			then
				$MY_MAKE $object
			else
				echo "?? $input not found"
			fi
		fi
		rm -f run_make.[coy]
	done
fi
