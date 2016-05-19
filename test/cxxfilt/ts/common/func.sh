#!/bin/sh
#
# $Id$

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

check_rlt() # execute command (saving output) and check exit code
{
    # $1 is command, $2 is expected exit code (0 or "N" for non-zero)
    RLT=`$1`
    CODE=$?
    if [ $2 = 0 -a $CODE -ne 0 ]; then
	tet_infoline "Command ($1) gave exit code $CODE, expected 0"
	FAIL=Y
    elif [ $2 != 0 -a $CODE -eq 0 ]; then
	tet_infoline "Command ($1) gave exit code $CODE, expected non-zero"
	FAIL=Y
    fi

    # $3 is expected result.
    if [ "$RLT" != "$3" ]; then
	tet_infoline "Command ($1) gave wrong result:"
	tet_infoline "$RLT"
	tet_infoline "expected:"
	tet_infoline "$3"
	FAIL=Y
    fi
}

run()
{
    tpstart "Running test '$1'"
    check_rlt "$TET_SUITE_ROOT/../../cxxfilt/c++filt $1" 0 "$2"
    tpresult
}
