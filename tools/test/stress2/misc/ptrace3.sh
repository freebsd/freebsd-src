#!/bin/sh

#
# Copyright (c) 2014 EMC Corp.
# All rights reserved.
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

# A regression test for a bug introduced in r269656.
# Page fault in proc_realparent+0x70 seen.
# Fixed in r270024.

# Test scenario by Mark Johnston markj@

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > ptrace3.c
mycc -o ptrace3 -Wall -Wextra -g ptrace3.c || exit 1
rm -f ptrace3.c
cd $here

/tmp/ptrace3

rm -f /tmp/ptrace3
exit
EOF
#include <sys/types.h>
#include <sys/ptrace.h>
#include <sys/wait.h>

#include <err.h>
#include <signal.h>
#include <unistd.h>

/*
 * A regression test for a bug introduced in r269656. The test process p creates
 * child processes c1 and c2 which do nothing but sleep; a third child process
 * (c3) uses ptrace(2) to reparent c1 and c2 from p to c3. Then c3 detaches from
 * c1 and c2, causing a crash when c1 and c2 are removed from p's now-corrupt
 * orphan list.
 */

pid_t
sleeper()
{
	pid_t p;

	p = fork();
	if (p == -1)
		err(1, "fork");
	else if (p == 0)
		while (1)
			sleep(1000000);
	return (p);
}

void
attach(pid_t p)
{
	int status;

	if (ptrace(PT_ATTACH, p, 0, 0) == -1)
		err(1, "ptrace");

	if (waitpid(p, &status, 0) == -1)
		err(1, "waitpid");
	else if (!WIFSTOPPED(status))
		errx(1, "failed to stop child");
}

void
detach(pid_t p)
{

	if (ptrace(PT_DETACH, p, 0, 0) == -1)
		err(1, "ptrace");
}

int
main(void)
{
	int status;
	pid_t c1, c2, p;

	c1 = sleeper();
	c2 = sleeper();

	p = fork();
	if (p == -1) {
		err(1, "fork");
	} else if (p == 0) {
		attach(c1);
		attach(c2);
		detach(c1);
		detach(c2);
	} else {
		if (waitpid(p, &status, 0) == -1)
			err(1, "waitpid");
		if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
			errx(1, "child exited abnormally");
	}

	/* Clean up. */
	(void)kill(c1, SIGKILL);
	(void)kill(c2, SIGKILL);

	return (0);
}
