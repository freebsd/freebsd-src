#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2022 Peter Holm <pho@FreeBSD.org>
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

# Demonstrate lingering child process after a timeout(1) timeout
# Test scenario suggestions by kib

# Fixed by: 709783373e57 - main - Fix another race between fork(2) and
# PROC_REAP_KILL subtree

eval prog=reaper.$$
cat > /tmp/$prog.c <<EOF
#include <sys/wait.h>
#include <err.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#define MAXP 10000

int
main(void)
{
	pid_t pids[MAXP];
	int i, parallel;
	char *cmdline[] = { "SLP", "86400", NULL };

	for (;;) {
		parallel = arc4random() % MAXP + 1;
		for (i = 0; i < parallel; i++) {
			if ((pids[i] = fork()) == 0) {
				setproctitle("child");
				if ((arc4random() % 100) < 10) {
					if (execve(cmdline[0], cmdline,
					    NULL) == -1)
				                err(1, "execve");
				} else {
					for (;;)
						pause();
				}
				_exit(0); /* never reached */
			}
			if (pids[i] == -1)
				err(1, "fork()");
		}

		usleep(arc4random() % 500);

		for (i = 0; i < parallel; i++) {
			if (kill(pids[i], SIGINT) == -1)
				err(1, "kill(%d)", pids[i]);
			if (waitpid(pids[i], NULL, 0) != pids[i])
				err(1, "waitpid(%d)", pids[i]);
		}
	}
}
EOF
sed -i '' "s#SLP#/tmp/$prog.sleep#" /tmp/$prog.c
cc -o /tmp/$prog -Wall -Wextra -O0 /tmp/$prog.c || exit 1
rm /tmp/$prog.c

cp /bin/sleep /tmp/$prog.sleep
n=1
start=`date +%s`
while true; do
	timeout 2s /tmp/$prog
	for i in `jot 50`; do
		pgrep -q $prog || break
		sleep .5
	done
	if pgrep -q $prog; then
		e=$((`date +%s` - start))
		echo "Failed in loop #$n after $e seconds."
		pgrep "$prog|timeout" | xargs ps -lp
		pkill $prog
		rm -f /tmp/$prog /tmp/$prog.sleep
		exit 1
	fi
	[ $((`date +%s` - start)) -ge 600 ] && break
	n=$((n + 1))
done
rm /tmp/$prog /tmp/$prog.sleep
exit 0
