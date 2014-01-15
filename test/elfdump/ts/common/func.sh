#!/bin/sh
#
# $Id: func.sh 2083 2011-10-27 04:41:39Z jkoshy $

tpstart() # write test purpose banner and initialise variables
{
    tet_infoline "$*"
    FAIL=N
}

tpresult() # give test purpose result
{
    # $1 is result code to give if FAIL=N (default PASS)
    if [ $FAIL = N ]; then
	tet_result ${1-PASS}
    else
	tet_result FAIL
    fi
}

check_exit() # execute command (saving output) and check exit code
{
    # $1 is command, $2 is expected exit code (0 or "N" for non-zero)
    eval "$1" > out.stdout 2> out.stderr
    CODE=$?
    if [ $2 = 0 -a $CODE -ne 0 ]; then
	tet_infoline "Command ($1) gave exit code $CODE, expected 0"
	FAIL=Y
    elif [ $2 != 0 -a $CODE -eq 0 ]; then
	tet_infoline "Command ($1) gave exit code $CODE, expected non-zero"
	FAIL=Y
    fi
}

check_nostdout() # check that nothing went to stdout
{
    if [ -s out.stdout ]; then
	tet_infoline "Unexpected output written to stdout, as shown below:"
	infofile out.stdout stdout:
	FAIL=Y
    fi
}

check_nostderr() # check that nothing went to stderr
{
    if [ -s out.stderr ]; then
	tet_infoline "Unexpected output written to stderr, as shown below:"
	infofile out.stderr stderr:
	FAIL=Y
    fi
}

check_stderr() # check that stderr matches expected error
{
    # $1 is file containing expected error
    # if no argument supplied, just check out.stderr is not empty

    case $1 in
    "")
	if [ ! -s out.stderr ];	then
	    tet_infoline "Expected output to stderr, but none written"
	    FAIL=Y
	fi
	;;
    *)
	diff -uN out.stderr ${1}.err > diff.out 2> /dev/null
	if [ $? -ne 0 ]; then
	    tet_infoline "Incorrect output written to stderr, as shown below"
	    infofile "diff.out" "diff:"
	    FAIL=Y
	fi
	;;
    esac
}

check_stdout() # check that stdout matches expected output
{
    # $1 is file containing expected output
    # if no argument supplied, just check out.stdout is not empty

    case $1 in
    "")
	if [ ! -s out.stdout ]
	then
	    tet_infoline "Expected output to stdout, but none written"
	    FAIL=Y
	fi
	;;
    *)
	diff -uN out.stdout ${1}.out > diff.out 2> /dev/null
	if [ $? -ne 0 ]; then
	    tet_infoline "Incorrect output written to stdout, as shown below"
	    infofile "diff.out" "diff:"
	    FAIL=Y
	fi
	;;
    esac
}

infofile() # write file to journal using tet_infoline
{
    # $1 is file name, $2 is prefix for tet_infoline

    prefix=$2
    while read line
    do
	tet_infoline "$prefix$line"
    done < $1
}

run()
{
    tpstart
    cmdline=`echo $1 | sed -e 's/@/ -/g' -e 's/%/ /g'`
    tet_infoline "$cmdline"
    check_exit "$TET_SUITE_ROOT/../../elfdump/elfdump $cmdline" 0
    check_stderr $1
    check_stdout $1
    tpresult
}

cleanup()
{
    rm -f out.stdout
    rm -f out.stderr
    rm -f diff.out
}
