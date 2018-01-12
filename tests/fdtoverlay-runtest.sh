#! /bin/sh

# Run script for fdtoverlay tests
# We run fdtoverlay to generate a target device tree, thn fdtget to check it

# Usage
#    fdtoverlay-runtest.sh name expected_output dtb_file node property flags value

. ./tests.sh

LOG=tmp.log.$$
EXPECT=tmp.expect.$$
rm -f $LOG $EXPECT
trap "rm -f $LOG $EXPECT" 0

expect="$1"
echo $expect >$EXPECT
node="$2"
property="$3"
flags="$4"
basedtb="$5"
targetdtb="$6"
shift 6
overlays="$@"

# First run fdtoverlay
verbose_run_check $VALGRIND "$FDTOVERLAY" -i "$basedtb" -o "$targetdtb" $overlays

# Now fdtget to read the value
verbose_run_log_check "$LOG" $VALGRIND "$DTGET" "$targetdtb" "$node" "$property" $flags

if cmp $EXPECT $LOG >/dev/null; then
    PASS
else
    if [ -z "$QUIET_TEST" ]; then
	echo "EXPECTED :-:"
	cat $EXPECT
    fi
    FAIL "Results differ from expected"
fi
