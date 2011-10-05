#!/bin/sh

name=$1
pid=$2

ec=0

if [ "$(uname -s)" = "Darwin" ] ; then
    echo "leaks check on $name ($pid)"
    leaks -exclude __CFInitialize $pid > leaks-log 2>&1 || \
        { echo "leaks failed: $?"; cat leaks-log; exit 1; }

    env pid=${pid} \
    perl -e 'my $excluded = 0; my $num = -1; while (<>) {
if (/Process $ENV{pid}: (\d+) leaks for \d+ total leaked bytes/) { $num = $1;}
if (/(\d+) leaks excluded/) { $excluded = $1;}
}
exit 1 if ($num != 0 && $num != $excluded);
exit 0;' leaks-log || \
	{ echo "Memory leak in $name" ; echo ""; cat leaks-log; ec=1; }

    # [ "$ec" != "0" ] && { env PS1=": leaks-debugger !!!! ; " bash ; }

fi

kill $pid
sleep 3
kill -9 $pid 2> /dev/null

rm -f leaks-log

exit $ec
