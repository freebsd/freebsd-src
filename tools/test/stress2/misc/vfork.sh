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

# Regression test for r246484.

# "panic: failed to set signal flags for ast p ... fl 4" seen.
# Fixed in r302999.

. ../default.cfg

cd /tmp
cat > vfork1.c <<- EOF
#include <err.h>
#include <stdio.h>
#include <unistd.h>

int
main(void)
{
	pid_t pid;

	fprintf(stderr, "%d\n", getpid());
	if ((pid = vfork()) == 0) {
#if 0
		if (ptrace(PT_TRACE_ME, 0, 0, 0) == -1)
			err(1, "PT_TRACEME");
#endif
		sleep(30);
		_exit(0);
	}
	if (pid == -1)
		err(1, "vfork");

	return (0);
}
EOF
mycc -o vfork1 -Wall -Wextra -g vfork1.c
rm  vfork1.c

cat > vfork2.c <<- EOF
#include <sys/types.h>
#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/ptrace.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

int
main(int argc, char **argv)
{
	pid_t pid, rpid;
	struct rusage ru;
	int status;

	if (argc != 2)
		errx(1, "Usage: %s <pid>", argv[0]);
	pid = atoi(argv[1]);

	if (pid == -1)
		err(1, "fork()");
	if (ptrace(PT_ATTACH, pid, NULL, 0) == -1)
		err(1, "ptrace(%d) attach", pid);
	if (wait(NULL) == -1)
		err(1, "wait");
	bzero(&ru, sizeof(ru));
	usleep(2000);
	if ((rpid = wait4(-1, &status, WNOHANG, &ru)) == -1) {
			err(0, "OK wait4");
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

	return (0);
}
EOF
mycc -o vfork2 -Wall -Wextra -g vfork2.c
rm  vfork2.c

./vfork1 &
sleep .2
childpid=`ps -lx | grep -v grep | grep vfork1 |
    tail -1 | grep nanslp | awk '{print $2}'`
# Seen before fix:
# failed to set signal flags properly for ast()
./vfork2 $childpid
s=$?

rm -f vfork1 vfork2
exit $s
