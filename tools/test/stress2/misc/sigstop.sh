#!/bin/sh

#
# Copyright (c) 2013 EMC Corp.
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

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

# Regression test.
# Will display "FAIL exit status = 0" without r240467

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > sigstop.c
mycc -o sigstop -Wall -Wextra sigstop.c || exit 1
rm -f sigstop.c
cd $here

su $testuser -c "/tmp/sigstop"

rm -f /tmp/sigstop
exit

EOF
#include <sys/types.h>
#include <err.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

#define PARALLEL 4

void
test(void)
{
	pid_t pid;
	int i, status;

	for (i = 0; i < 1000; i++) {
		if ((pid = fork()) == 0) {
			_exit(64);
		}
		if (pid == -1)
			err(1, "fork()");
		if (kill(pid, SIGSTOP) == -1)
			warn("sigstop");
		if (kill(pid, SIGCONT) == -1)
			warn("sigcont");
		wait(&status);
		if (WEXITSTATUS(status) != 64) {
			fprintf(stderr, "FAIL exit status = %d\n",
			    WEXITSTATUS(status));
			break;
		}
	}
	_exit(0);
}

int
main(void)
{
	int i;

	for (i = 0; i < PARALLEL; i++) {
		if (fork() == 0)
			test();
	}

	for (i = 0; i < PARALLEL; i++)
		wait(NULL);

	return (0);
}
