#!/bin/sh

# Test scenario from:
# Bug 290843 - killpg deadlock against a stopped interrupted fork
# By : Bryan Drewery <bdrewery@FreeBSD.org>

# Seen:
# 0 70877  3650  7   0  0 14856  3680 sigsusp  I+    0    0:00.00 sh -x ./killpg5.sh
# 0 70881 70877 10   0  0 14856  3688 killpg r D+    0    0:00.07 sh -c trap "kill -9 %1; exit" INT; foo() { unset cmd; cmd=$(/sbin/sysctl -n vm.loadavg|/u

sh -c 'trap "kill -9 %1; exit" INT; foo() { unset cmd; cmd=$(/sbin/sysctl -n vm.loadavg|/usr/bin/awk "{print \$2,\$3,\$4}"); case "${cmd:+set}" in set) ;; *) exit 99 ;; esac }; runner() { while foo; do :; done }; launch() { local -; set -m; PS4="child+ " runner & }; set -x; while :; do launch; sleep 0.1; kill -STOP %1; kill -TERM %1; kill -CONT %1; ret=0; wait; if [ $ret -eq 99 ]; then exit 99; fi; done;' > /dev/null 2>&1 &
sleep 60
kill -9 $!
sleep .2
killpgpid=`ps -lUroot | grep -v grep | grep ' killpg ' | awk '{print $2}'`
[ -n "$killpgpid" ] && { ps -lp$killpgpid; exit 1; }	# The bug
pgrep -f 'foo()' | xargs kill > /dev/null 2>&1		# Cleanup
wait
exit 0
