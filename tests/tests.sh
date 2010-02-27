# Common functions for shell testcases

PASS () {
    echo "PASS"
    exit 0
}

FAIL () {
    echo "FAIL" "$@"
    exit 2
}

DTC=../dtc

verbose_run () {
    if [ -z "$QUIET_TEST" ]; then
	"$@"
    else
	"$@" > /dev/null 2> /dev/null
    fi
}

verbose_run_log () {
    LOG="$1"
    shift
    "$@" > "$LOG" 2>&1
    ret=$?
    if [ -z "$QUIET_TEST" ]; then
	cat "$LOG" >&2
    fi
    return $ret
}
