#! /bin/sh

. ./tests.sh

LOG=tmp.log.$$
EXPECT=tmp.expect.$$
rm -f $LOG $EXPECT
trap "rm -f $LOG $EXPECT" 0

expect="$1"
printf '%b\n' "$expect" > $EXPECT
shift

verbose_run_log_check "$LOG" $VALGRIND $DTGET "$@"

if cmp $EXPECT $LOG>/dev/null; then
    PASS
else
    if [ -z "$QUIET_TEST" ]; then
	echo "EXPECTED :-:"
	cat $EXPECT
    fi
    FAIL "Results differ from expected"
fi
