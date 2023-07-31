#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2023 Peter Holm <pho@FreeBSD.org>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

# Another killpg(2) test scenario. No problems seen.

. ../default.cfg
export prog=$(basename "$0" .sh)
set -u

cat > /tmp/$prog.c <<EOF
#include <sys/wait.h>
#include <err.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#define PARALLEL 2

int
test(void)
{
	pid_t pid;
	time_t start;

	start = time(NULL);
	while (time(NULL) - start < 300) {
		if ((pid = fork()) == -1)
			err(1, "fork()");
		if (pid == 0) {
			if (arc4random() % 100 < 20)
				usleep(arc4random() % 5000);
			_exit(0); /* Not reached */
		}
		if (waitpid(pid, NULL, 0) != pid)
			err(1, "waitpid()");
	}
	_exit(0);
}

int
main(void)
{
	pid_t pids[PARALLEL];
	int i;

	for (i = 0; i < PARALLEL; i++) {
			test();
	}
	for (i = 0; i < PARALLEL; i++) {
		if (waitpid(pids[i], NULL, 0) != pids[i])
			err(1, "waotpid() main");
	}

}
EOF
mycc -o /tmp/$prog -Wall -Wextra -O2 /tmp/$prog.c || exit 1

export MAXSWAPPCT=80
../testcases/swap/swap -t 2m -i 5 > /dev/null 2>&1 &
sleep .5
start=`date +%s`
while [ $((`date +%s` - start)) -lt 120 ]; do
	for i in `jot 200 100`; do
		(
		sl=$prog.$i
		sleep=/tmp/$sl
		cp /tmp/$prog $sleep
		su $testuser -c "$sleep & $sleep & $sleep &" & pid=$!
		for j in `jot 10`; do
			pgrep -q "$sl" && break
			sleep .5
		done
		pgrep -q "$sl" || { echo "No start"; exit  1; }
		pgid=`pgrep "$sl" | xargs ps -jp | sed 1d | \
		    tail -1 | awk '{print $4}'`
		[ -z "$pgid" ] && { echo "Zero pgid:$pgid"; ps aj | \
		    sed -n "1p;/$sl/p"; exit 1; }
		sleep 1.`jot -r 1 2 9`
		kill -- -$pgid || { echo "kill -$pgid failed"; exit 1; }
		wait $pid
		rm -f $sleep
		) &
	done
	while [ `ps -U$testuser | wc -l` -gt 1 ] ; do sleep 2; done
done
while pkill swap; do :; done
wait
rm /tmp/$prog /tmp/$prog.c
exit 0
