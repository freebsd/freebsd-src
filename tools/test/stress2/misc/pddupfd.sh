#!/bin/sh

set -u
prog=$(basename "$0" .sh)
cat > /tmp/$prog.c <<EOF
/* \$Id: pddupfd.c,v 1.6 2026/07/04 17:48:47 kostik Exp kostik $ */

#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/procdesc.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <machine/atomic.h>
#include <err.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define MY_SYS_pdopenpid	603
#define MY_SYS_pddupfd		604

static int
my_pdopenpid(pid_t pid, int flags)
{
	return (syscall(MY_SYS_pdopenpid, pid, flags));
}

static int
my_pddupfd(pid_t pid, int remote_fd, int flags)
{
	return (syscall(MY_SYS_pddupfd, pid, remote_fd, flags));
}

static void
check(int fd, int child, int *comm)
{
	struct __wrusage wu;
	struct __siginfo si;
	int error, f, pp, rfd, status, workfd;
	char cmd[128];

	error = pdgetpid(fd, &pp);
	if (error == -1)
		err(1, "pgdetpid");
	printf("child pid %d %d\n", child, pp);

	while ((rfd = (int)atomic_load_int(comm)) == -1)
		;
	if (rfd == -2)
		errx(1, "open in child failed");
	workfd = my_pddupfd(fd, rfd, 0);
	if (workfd == -1)
		err(1, "pddupfd");

	f = fcntl(workfd, F_GETFD);
	if (f == -1)
		err(1, "F_GETFD %d", workfd);
	f &= ~FD_CLOEXEC;
	error = fcntl(workfd, F_SETFD, f);
	if (error == -1)
		err(1, "F_SETFD %d", workfd);

	snprintf(cmd, sizeof(cmd), "cat 0<&%d", workfd);
	system(cmd);
	close(workfd);

	atomic_store_int(comm, -1);
	
	error = pdwait(fd, &status, WEXITED, &wu, &si);
	if (error == -1)
		err(1, "pdwait");
	printf("exited pid %d status %#x si.si_status %#x si.si_code %#x\n",
	    pp, status, si.si_status, si.si_code);
}

int
main(void)
{
	int *comm;
	pid_t child;
	int fd, workfd;

	comm = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED |
	    MAP_ANONYMOUS, -1, 0);
	if (comm == MAP_FAILED)
		err(1, "mmap");
	atomic_store_int(comm, -1);

	child = fork();
	if (child == -1)
		err(1, "fork");
	if (child == 0) {
		workfd = open("/etc/passwd", O_RDONLY);
		if (workfd == -1) {
			atomic_store_int(comm, -2);
			err(1, "open");
		}
		atomic_store_int(comm, workfd);
		while ((int)atomic_load_int(comm) >= 0)
			sleep(1);
		_exit(0x123);
	}

	fd = my_pdopenpid(child, 0);
	if (fd == -1)
		err(1, "pdopenpid");

	check(fd, child, comm);
	close(fd);
}
EOF

cc -o /tmp/$prog -Wall -Wextra -g -O0 /tmp/$prog.c || exit 1

../testcases/swap/swap -t 2m -i 20 &
sleep 2

cd /tmp
s=0
start=`date +%s`
while [ $((`date +%s`- start)) -lt 120 ]; do
	./$prog > /dev/null ; s=$?
done
[ $s -eq 124 ] && s=0
while pkill swap; do :; done
wait
cd -

rm /tmp/$prog /tmp/$prog.c
exit $s
