#!/bin/sh

usage()
{
    cat >&2 <<__EOF__
A harness for test cases in the DTrace test suite.

usage: $(basename $0) <testfile>
__EOF__
    exit 1
}

gettag()
{
    local tag

    tag=$(basename $1)
    tag=${tag#*.}
    tag=${tag%%[a-z.]*}
    echo $tag
}

runtest()
{
    local cfile ofile ddep odep dflags exe exstatus pid retval status

    exstatus=0
    retval=0

    case $TFILE in
    drp.DTRACEDROP_*.d|err.*.d|tst.*.d)
        case $TFILE in
        drp.DTRACEDROP_*.d)
            dflags="-x droptags"
            tag=$(gettag "$TFILE")
            ;;
        err.D_*.d)
            exstatus=1
            dflags="-x errtags"
            tag=$(gettag "$TFILE")
            ;;
        err.*.d)
            exstatus=1
            ;;
        esac

        exe=${TFILE%.*}.exe
        cfile=${TFILE%.*}.c
        ddep=${TFILE%.*}.d
        ddep=${ddep#tst.}
        if [ -r "$cfile" -a -r "$ddep" ]; then
            # The tst.xxx.c corresponding to the tst.xxx.d is usually
            # (cross-)compiled on the build host and installed in the target
            # system as an executable. However, when there is an associated
            # xxx.d file to be included in that executable, we must compile
            # xxx.d in the target system at runtime because DTrace currently
            # doesn't support cross-compilation and will always use the build
            # host's ELF format when calling `dtrace -G`.
            # TODO add support for cross-compilation in DTrace and move it back
            #      to dtrace.test.mk.
            ofile="${cfile%.c}.o"
            odep="${ddep%.d}.o"
            dtrace -C -x nolibs -h -s "$ddep"
            cc -O0 -g -I. -o "$ofile" -c "$cfile"
            dtrace -C -x nolibs -G -s "$ddep" "$ofile"
            cc -O0 -g -I. -o "$exe" "$ofile" "$odep"
            rm -f "${ddep%.d}.h" "$ofile" "$odep"
        fi

        if [ -f "$exe" -a -x "$exe" ]; then
            ./$exe &
            pid=$!
            dflags="$dflags ${pid}"
        fi

        dtrace -C -s "${TFILE}" $dflags >$STDOUT 2>$STDERR
        status=$?

        if [ $status -ne $exstatus ]; then
            ERRMSG="dtrace exited with status ${status}, expected ${exstatus}"
            retval=1
        elif [ -n "${tag}" ] && ! grep -Fq " [${tag}] " ${STDERR}; then
            ERRMSG="dtrace's error output did not contain expected tag ${tag}"
            retval=1
        fi

        if [ -n "$pid" ]; then
            kill -0 $pid >/dev/null 2>&1 && kill -9 $pid >/dev/null 2>&1
            wait
        fi
        ;;
    err.*.ksh|tst.*.ksh)
        expr "$TFILE" : 'err.*' >/dev/null && exstatus=1

        tst=$TFILE ksh -p "$TFILE" /usr/sbin/dtrace >$STDOUT 2>$STDERR
        status=$?

        if [ $status -ne $exstatus ]; then
            ERRMSG="script exited with status ${status}, expected ${exstatus}"
            retval=1
        fi
        ;;
    *)
        ERRMSG="unexpected test file name $TFILE"
        retval=1
        ;;
    esac

    if [ $retval -eq 0 ] && \
        head -n 1 $STDOUT | grep -q -E '^#!/.*ksh$'; then
        ksh -p $STDOUT
        retval=$?
    fi

    return $retval
}

[ $# -eq 1 ] || usage

readonly STDERR=$(mktemp)
readonly STDOUT=$(mktemp)
readonly TFILE=$(basename $1)
readonly EXOUT=${TFILE}.out

kldload -n dtrace_test
cd $(dirname $1)
runtest
RESULT=$?

if [ $RESULT -eq 0 -a -f $EXOUT -a -r $EXOUT ] && \
   ! cmp $STDOUT $EXOUT >/dev/null 2>&1; then
    ERRMSG="test output mismatch"
    RESULT=1
fi

if [ $RESULT -ne 0 ]; then
    echo "test $TFILE failed: $ERRMSG" >&2
    if [ $(stat -f '%z' $STDOUT) -gt 0 ]; then
        cat >&2 <<__EOF__
test stdout:
--
$(cat $STDOUT)
--
test stdout diff:
--
$(diff -u $EXOUT $STDOUT)
--
__EOF__
    fi
    if [ $(stat -f '%z' $STDERR) -gt 0 ]; then
        cat >&2 <<__EOF__
test stderr:
--
$(cat $STDERR)
--
__EOF__
    fi
fi

rm -f $STDERR $STDOUT
exit $RESULT
