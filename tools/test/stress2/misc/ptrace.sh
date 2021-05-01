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

# wait4(2) / ptrace(2) regression test.

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > ptrace.c
mycc -o ptrace -Wall -Wextra -g ptrace.c || exit 1
rm -f ptrace.c
cd $here

/tmp/ptrace

rm -f /tmp/ptrace
exit

EOF
#include <sys/types.h>
#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <strings.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>

void
test(void)
{
	pid_t pid, rpid;
	struct rusage ru;
	int status;

	if ((pid = fork()) == 0) {
		usleep(2000);
		_exit(64);
	}
	if (pid == -1)
		err(1, "fork()");
	if (ptrace(PT_ATTACH, pid, NULL, 0) == -1)
		err(1, "ptrace(%d) attach", pid);
	if (wait(NULL) == -1)
		err(1, "wait");
	bzero(&ru, sizeof(ru));
	usleep(2000);
	if ((rpid = wait4(-1, &status, WNOHANG, &ru)) == -1) {
		if (errno == ECHILD)
			warn("FAIL");
		else
			err(1, "wait4");
	}
	if (rpid == 0) {
//		fprintf(stderr, "No rusage info.\n");
		if (ptrace(PT_DETACH, pid, NULL, 0) == -1)
			err(1, "ptrace(%d) detach", pid);
		if (wait(&status) == -1)
			err(1, "wait");
	} else {
		fprintf(stderr, "FAIL Got unexpected rusage.\n");
		if (ru.ru_utime.tv_sec != 0)
			fprintf(stderr, "FAIL tv_sec\n");
	}
	if (status != 0x4000)
		fprintf(stderr, "FAIL Child exit status 0x%x\n", status);

	_exit(0);
}

int
main(void)
{
	test();

	return (0);
}
