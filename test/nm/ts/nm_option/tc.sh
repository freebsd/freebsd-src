#!/bin/sh
#
# $Id: tc.sh 2109 2011-11-07 22:04:10Z kaiwang27 $

tp1()
{
    run_without_diff "-z" $ERROR_USAGE
}

tp2()
{
    run_without_diff "-Z" $ERROR_USAGE
}

tp3()
{
    run_without_diff "-y" $ERROR_USAGE
}

tp4()
{
    run_without_diff "-Y" $ERROR_USAGE
}

tp5()
{
    run_without_diff "--aaaaaa" $ERROR_USAGE
}

tp6()
{
    run_without_diff "--+_" $ERROR_USAGE
}

tp7()
{
    run_without_diff "--help" $ERROR_OK
}

ERROR_OK="0"
ERROR_USAGE="1"

tet_startup=""
tet_cleanup=""

iclist="ic1 ic2 ic3 ic4 ic5 ic6 ic7"

ic1="tp1"
ic2="tp2"
ic3="tp3"
ic4="tp4"
ic5="tp5"
ic6="tp6"
ic7="tp7"

. $TET_SUITE_ROOT/ts/common/func.sh
. $TET_ROOT/lib/xpg3sh/tcm.sh
