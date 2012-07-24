#! /bin/sh

. ./tests.sh

if [ "$1" = "-n" ]; then
    NEG="$1"
    shift
fi

OUTPUT="$1"
shift

verbose_run $VALGRIND "$DTC" -o "$OUTPUT" "$@"
ret="$?"

FAIL_IF_SIGNAL $ret

if [ -n "$NEG" ]; then
    if [ ! -e "$OUTPUT" ]; then
	FAIL "Produced no output"
    fi
else
    if [ -e "$OUTPUT" ]; then
	FAIL "Incorrectly produced output"
    fi
fi

rm -f "$OUTPUT"

PASS
