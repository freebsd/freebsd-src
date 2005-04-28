#!/bin/sh
#
# Common code used run regression tests for usr.bin/make.
#
# $FreeBSD$

#
# Output usage messsage.
#
print_usage()
{
	echo "Usage: $0 command"
	echo "	clean	- remove temp files (get initial state)"
	echo "	compare	- compare result of test to expected"
	echo "	desc	- print description of test"
	echo "	diff	- print diffs between results and expected"
	echo "	harness	- produce output suiteable for Test::Harness"
	echo "	run	- run the {test, compare, clean}"
	echo "	test	- run test case"
	echo "	update	- update the expected with current results"
}

#
# Check if the test result is the same as the expected result.
#
# $1	Input file
#
hack_cmp()
{
	local EXPECTED RESULT
	EXPECTED="expected.$1"
	RESULT=${WORK_DIR}/$1

	if [ -f $EXPECTED ]; then
		diff -q $EXPECTED $RESULT 1> /dev/null 2> /dev/null
		return $?
	else
		return 1	# FAIL
	fi
}

#
# Check if the test result is the same as the expected result.
#
# $1	Input file
#
hack_diff()
{
	local EXPECTED RESULT
	EXPECTED="expected.$1"
	RESULT=${WORK_DIR}/$1

	echo diff -u $EXPECTED $RESULT
	if [ -f $EXPECTED ]; then
		diff -u $EXPECTED $RESULT
		return $?
	else
		return 1	# FAIL
	fi
}

#
# Default setup_test() function.
#
# The default function just does nothing.
#
# Both the variables SRC_BASE WORK_BASE are available.
#
setup_test()
{
}

#
# Default run_test() function.  It can be replace by the
# user specified regression test.
#
# Both the variables SRC_BASE WORK_BASE are available.
#
# Note: this function executes from a subshell.
#
run_test()
{
	cd ${WORK_DIR}
        $MAKE_PROG 1> stdout 2> stderr
        echo $? > status
}

#
# Default clean routine
#
clean_test()
{
}

#
# Clean working directory
#
eval_clean()
{
	rm -f ${WORK_DIR}/stdout
	rm -f ${WORK_DIR}/stderr
	rm -f ${WORK_DIR}/status
	clean_test
}

#
# Compare results with expected results
#
eval_compare()
{
	hack_cmp stdout || FAIL="stdout $FAIL"
	hack_cmp stderr || FAIL="stderr $FAIL"
	hack_cmp status || FAIL="status $FAIL"

	if [ ! -z "$FAIL" ]; then
		FAIL=`echo $FAIL`
		echo "$SUBDIR: Test failed {$FAIL}"
	fi
}

#
# Compare results with expected results for prove(1)
#
eval_hcompare()
{
	FAIL=
	hack_cmp stdout || FAIL="stdout $FAIL"
	hack_cmp stderr || FAIL="stderr $FAIL"
	hack_cmp status || FAIL="status $FAIL"

	if [ ! -z "$FAIL" ]; then
		FAIL=`echo $FAIL`
		echo "not ok 1 $SUBDIR # reason: {$FAIL}"
	else
		echo "ok 1 $SUBDIR"
	fi
}

#
# Print description
#
eval_desc()
{
	echo -n "$SUBDIR: "
	desc_test
}

#
# Prepare and run the test
#
eval_test()
{
	[ -d ${WORK_DIR} ] || mkdir -p ${WORK_DIR}
	if [ -f Makefile ] ; then
		cp Makefile ${WORK_DIR}
	fi
	setup_test
	( run_test )
}

#
# Diff current and expected results
#
eval_diff()
{
	eval_test
	echo "------------------------"
	echo "- $SUBDIR"
	echo "------------------------"
	hack_diff stdout
	hack_diff stderr
	hack_diff status
}

#
# Run the test for prove(1)
#
eval_harness()
{
	echo 1..1
	eval_test
	eval_hcompare
	eval_clean
}

#
# Run the test
#
eval_run()
{
	eval_test
	eval_compare
	eval_clean
}

#
# Update expected results
#
eval_update()
{
	eval_test
	cat ${WORK_DIR}/stdout > expected.stdout
	cat ${WORK_DIR}/stderr > expected.stderr
	cat ${WORK_DIR}/status > expected.status
}

#
# Note: Uses global variable $DIR which might be assigned by
#	the script which sourced this file.
#
eval_cmd()
{
	if [ $# -eq 0 ] ; then
		set -- harness
	fi

	case $1 in
	clean|compare|hcompare|desc|diff|harness|run|test|update)
		eval eval_$1
		;;
	*)
		print_usage
		;;
	esac
}

#
# Parse command line arguments.
#
args=`getopt m:w:v $*`
if [ $? != 0 ]; then
	echo 'Usage: ...'
	exit 2
fi
set -- $args
for i; do
	case "$i" in
	-m)
		MAKE_PROG="$2"
		shift
		shift
		;;
	-w)
		WORK_BASE="$2"
		shift
		shift
		;;
	-v)
		VERBOSE=1
		shift
		;;
	--)
		shift
		break
		;;
	esac
done

#
# Determine our sub-directory. Argh.
#
SRC_DIR=`pwd`
SRC_BASE=`while [ ! -f common.sh ] ; do cd .. ; done ; pwd`
SUBDIR=`echo ${SRC_DIR} | sed "s@${SRC_BASE}/@@"`

WORK_BASE=${WORK_BASE:-"/tmp/$USER.make.test"}
WORK_DIR=${WORK_BASE}/${SUBDIR}
MAKE_PROG=${MAKE_PROG:-/usr/bin/make}

export MAKE_PROG
export VERBOSE
export SRC_BASE
export WORK_BASE
export SUBDIR
export SRC_DIR
export WORK_DIR
