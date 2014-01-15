#!/bin/sh
#
# $Id: tc.sh 2085 2011-10-27 05:06:47Z jkoshy $

tp1()
{
    test_debug_syms1 $TEST_FILE "$TEST_FILE-debug-syms.txt"
}

tp2()
{
    test_debug_syms2 $TEST_FILE "$TEST_FILE-debug-syms.txt"
}

startup()
{
    uudecode "$TEST_FILE.uu"
}

cleanup()
{
    rm -f $TEST_FILE
}

TEST_FILE="test_obj"

tet_startup="startup"
tet_cleanup="cleanup"

iclist="ic1 ic2"

ic1="tp1"
ic2="tp2"

. $TET_SUITE_ROOT/ts/common/func.sh
. $TET_ROOT/lib/xpg3sh/tcm.sh
