#! /bin/sh

. ./tests.sh

verbose_run $VALGRIND "$DTC" -o/dev/null "$@"
ret="$?"

if [ "$ret" -gt 127 ]; then
    FAIL "dtc killed by signal (ret=$ret)"
elif [ "$ret" != "1" ]; then
    FAIL "dtc returned incorrect status $ret instead of 1"
fi

PASS
