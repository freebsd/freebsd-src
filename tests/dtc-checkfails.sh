#! /bin/sh

. ./tests.sh

for x; do
    shift
    if [ "$x" = "-n" ]; then
	for x; do
	    shift
	    if [ "$x" = "--" ]; then
		break;
	    fi
	    NOCHECKS="$NOCHECKS $x"
	done
	break;
    fi
    if [ "$x" = "--" ]; then
	break;
    fi
    YESCHECKS="$YESCHECKS $x"
done

LOG=tmp.log.$$
rm -f $LOG
trap "rm -f $LOG" 0

verbose_run_log "$LOG" $VALGRIND "$DTC" -o /dev/null "$@"
ret="$?"

FAIL_IF_SIGNAL $ret

for c in $YESCHECKS; do
    if ! grep -E "^(ERROR)|(Warning) \($c\):" $LOG > /dev/null; then
	FAIL "Failed to trigger check \"$c\""
    fi
done

for c in $NOCHECKS; do
    if grep -E "^(ERROR)|(Warning) \($c\):" $LOG > /dev/null; then
	FAIL "Incorrectly triggered check \"$c\""
    fi
done

PASS
