#!/bin/sh

#
# Copyright (c) 2016 EMC Corp.
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

# It seems to be possible for ptrace(PT_ATTACH) to race with the delivery
# of a signal to the same process.

# $ while ./pt.sh; do date; done
# 15. juli 2016 kl. 05.59.05 CEST
# 15. juli 2016 kl. 06.03.07 CEST
#  UID   PID  PPID CPU PRI NI   VSZ  RSS MWCHAN STAT TT     TIME COMMAND
# 1001   863   862   0  44  0 13880 5268 wait   Is    0  0:00,13 -bash (bash)
# 1001 21053   863   0  52  0 13188 3088 wait   I+    0  0:00,01 /bin/sh ./pt.sh
# 1001 21096 21053   0  52  0 10544 2308 wait   I+    0  0:00,00 /tmp/pt
# 1001 21103 21096   0  20  0 12852 2456 wait   S+    0  0:00,00 pt: main (pt)
# 1001 21103 21096   0  35  0 12852 2456 wait   I+    0  0:55,30 pt: main (pt)
# 1001 21104 21096   0  72  0     0    0 -      Z+    0  0:00,00 <defunct>
# 1001 21105 21096   0  72  0     0    0 -      Z+    0  0:00,00 <defunct>
# 1001 21116 21103   0 103  0 10544 2336 -      RX+   0  4:22,41 pt: spinner (pt)
# 1001 37711 21103   0  20  0 21200 2960 -      R+    0  0:00,00 ps -Hl
# 1001   890   879   0  20  0 22184 6196 select Ss+   1  1:21,86 top -s 1
# 1001 85222 85221   0  21  0 13880 5276 ttyin  Is+   2  0:00,23 -bash (bash)
# SIGALRM state 2, pid 21116
# $

# Fixed by r303423.

. ../default.cfg

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/ptrace7.c
mycc -o ptrace7 -Wall -Wextra -O0 -g ptrace7.c -pthread || exit 1
rm -f ptrace7.c
cd $odir

/tmp/ptrace7
s=$?

while pgrep -q swap; do
	pkill -9 swap
done
rm -rf /tmp/ptrace7
exit $s

EOF
#include <sys/param.h>
#include <sys/ptrace.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#ifdef __FreeBSD__
#include <pthread_np.h>
#define	__NP__
#endif
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static pid_t pid;
static pthread_barrier_t barr;
static int cont;
static int state;

#define PARALLEL 8
#define RUNTIME (4 * 60)

static void
ahandler(int i __unused)
{
	system("ps -Hl");
	fprintf(stderr, "SIGALRM state %d, pid %d\n", state, pid);
	exit(1);
}

static void
handler(int i __unused)
{
}

static void *
t1(void *data __unused)
{
	int status;

#ifdef __NP__
	pthread_set_name_np(pthread_self(), __func__);
#endif
	while (cont == 1) {
		state = 1;
		if (ptrace(PT_ATTACH, pid, 0, 0) == -1)
			err(1, "ptrace(%d)", pid);

		state = 2;
		if (waitpid(pid, &status, 0) == -1)
			err(1, "waitpid");
		else if (!WIFSTOPPED(status))
			errx(1, "failed to stop child");
		state = 3;
		if (ptrace(PT_DETACH, pid, 0, 0) == -1)
			err(1, "ptrace");
		state = 4;
		usleep(arc4random() % 100 + 50);
	}

	return (NULL);
}

static void *
t2(void *data __unused)
{
#ifdef __NP__
	pthread_set_name_np(pthread_self(), __func__);
#endif
	while (cont == 1) {
		if (kill(pid, SIGHUP) == -1)
			err(1, "kill");
		usleep(arc4random() % 100 + 50);
	}

	return (NULL);
}

static void
test(void)
{
	pthread_t tid[2];
	struct sigaction sa;
	int r, status;

	r = pthread_barrier_wait(&barr);
	if (r != 0 && r != PTHREAD_BARRIER_SERIAL_THREAD)
	    errc(1, r, "pthread_barrier_wait");

	sa.sa_handler = ahandler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(SIGALRM, &sa, NULL) == -1)
		err(1, "sigaction");
	alarm(RUNTIME + 60);

	sa.sa_handler = handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(SIGHUP, &sa, NULL) == -1)
		err(1, "sigaction");

	setproctitle("%s", "spinner");
	if ((pid = fork()) == 0) {
		for(;;)
			getuid();

		_exit(0);
	}

	cont = 1;
	if ((r = pthread_create(&tid[0], NULL, t1, NULL)) == -1)
		errc(1, r, "pthread_create");
	if ((r = pthread_create(&tid[1], NULL, t2, NULL)) == -1)
		errc(1, r, "pthread_create");

	setproctitle("%s", "main");
	sleep(RUNTIME);

	cont = 0;
	if ((r = pthread_join(tid[0], NULL)) == -1)
			errc(1, r, "pthread_join");
	if ((r = pthread_join(tid[1], NULL)) == -1)
			errc(1, r, "pthread_join");
	if (kill(pid, SIGKILL) != 0)
		err(1, "kill(%d)", pid);
	if (waitpid(pid, &status, 0) == -1)
		err(1, "waitpid(%d)", pid);

	exit(0);
}

int
main(void)
{
	pthread_barrierattr_t attr;
	int e, i, pids[PARALLEL], r, status;

	if ((r = pthread_barrierattr_init(&attr)) != 0)
		errc(1, r, "pthread_barrierattr_init");
	if ((r = pthread_barrierattr_setpshared(&attr,
	    PTHREAD_PROCESS_SHARED)) != 0)
		errc(1, r, "pthread_barrierattr_setpshared");
	if ((r = pthread_barrier_init(&barr, &attr, PARALLEL)) != 0)
		errc(1, r, "pthread_barrier_init");

	e = 0;
	for (i = 0; i < PARALLEL; i++) {
		if ((pids[i] = fork()) == 0)
			test();
	}
	for (i = 0; i < PARALLEL; i++) {
		if (waitpid(pids[i], &status, 0) == -1)
			err(1, "waitpid%d)", pids[i]);
		e += status == 0 ? 0 : 1;
	}

	if ((r = pthread_barrier_destroy(&barr)) > 0)
		errc(1, r, "pthread_barrier_destroy");

	return (e);
}
