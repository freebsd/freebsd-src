#! /bin/sh

# Arguments:
#   $1 - source file to compile and compare with fdtdump output of the
#	  compiled file.

. ./tests.sh

dts="$1"
dtb="${dts}.dtb"
out="${dts}.out"
LOG=tmp.log.$$

files="$dtb $out $LOG"

rm -f $files
trap "rm -f $files" 0

verbose_run_log_check "$LOG" $VALGRIND $DTC -O dtb $dts -o $dtb
$FDTDUMP ${dtb} | grep -v "//" >${out}

if diff -w $dts $out >/dev/null; then
    PASS
else
    if [ -z "$QUIET_TEST" ]; then
	echo "DIFF :-:"
	diff -u -w $dts $out
    fi
    FAIL "Results differ from expected"
fi
