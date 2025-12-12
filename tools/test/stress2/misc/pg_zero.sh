#!/bin/sh

# Regression test for:
# D53963: vm_fault: only rely on PG_ZERO when the page was newly allocated
# Test scenario suggestions by: kib

# Problem seen:
# b[0] is 4. Expected 0
# b[0] is 4. Expected 0

. ../default.cfg

set -u
prog=$(basename "$0" .sh)
cat > /tmp/$prog.c <<EOF
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int debug = 0;
static int fail, fd, ok, parallel;
static volatile int done, go;
static char *buf;
static char *path = "/dev/md$mdstart";

#define PARALLEL 3
#define RUNTIME 180

void *
wr(void *arg __unused)
{
	int n;

	while (done != 1) {
		while (go == 0 && done != 1) {
			usleep(10);
		}
		go = 2;
		n = write(fd, buf, 512);
		if (n == -1 && errno != EFAULT)
			warn("write()");
		if (debug == 1) {
			if (n == -1)
				fail++;
			else
				ok++;
		}
	}

	return (NULL);
}

int
test(void)
{
	size_t len;
	int i, pagesize;
	char *a, *b, *c, *p;

	pagesize = sysconf(_SC_PAGESIZE);
	len = 3 * pagesize;
	if ((p = mmap(NULL, len, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap(%ld pages", len);
	if (mlock(p, len) == -1)
		err(1, "mlock()");

	a = p;
	b = a + pagesize;
	buf = b;
	c = b + pagesize;

	memset(a, 2, pagesize);
	memset(b, 4, pagesize);
	memset(c, 8, pagesize);

	if (munlock(b, pagesize) == -1)
		err(1, "munlock(b)");
	go = 1;
	while (go == 1)
		usleep(10);
	if (munmap(b, pagesize) == -1)
		err(1, "munmap(b)");

	if (mmap(b, pagesize, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_FIXED, -1, 0) == MAP_FAILED)
		err(1, "mmap(%d pages", pagesize);

	for (i = 0; i < pagesize; i++) {
		if (b[i] != 0) {
			fprintf(stderr, "b[%d] is %d. Expected 0\n", i, (int)b[i]);
			return (1);
		}
	}
	go = 0;

	if (munmap(p, len) == -1)
		err(1, "Final munmap()");

	return (0);
}

int
run(void)
{
	pthread_t tid;
	time_t start;
	int e;

	ok = fail = 0;
	go = 0;
	e = pthread_create(&tid, NULL, wr, NULL);
	if (e)
		errc(1, e, "pthread_create()");
	fail = ok = 0;
	start = time(NULL);
	while ((time(NULL) - start) < 30) {
		if (lseek(fd, 0, SEEK_SET) == -1)
			err(1, "lseek(0)");
		if (fsync(fd) != 0)
			err(1, "fsync()");
		if ((e = test()) != 0)
			break;
	}
	done = 1;
	pthread_join(tid, NULL);
	if (debug == 1)
		fprintf(stderr, "Fail = %3d, OK = %5d, parallel = %d\n", fail, ok, parallel);
	_exit(e);
}

int
main(void)
{
	pid_t pids[PARALLEL];
	time_t start;
	int e, i, status;

	if ((fd = open(path, O_WRONLY)) == -1)
		err(1, "open(%s)", path);
	e = 0;
	start = time(NULL);
	while ((time(NULL) - start) < RUNTIME && e == 0) {
		parallel = arc4random() % PARALLEL + 1;
		for (i = 0; i < parallel; i++) {
			if ((pids[i] = fork()) == 0)
				run();
			if (pids[i] == -1)
				err(1, "fork()");
		}
		for (i = 0; i < parallel; i++) {
			if (waitpid(pids[i], &status, 0) == -1)
				err(1, "waitpid(%d)", pids[i]);
			if (status != 0) {
				if (WIFSIGNALED(status))
					fprintf(stderr,
					    "pid %d exit signal %d\n",
					    pids[i], WTERMSIG(status));
			}
			e += status == 0 ? 0 : 1;
		}
	}
	close(fd);

	return (e);
}
EOF

mycc -o /tmp/$prog -Wall -Wextra -O0 -g /tmp/$prog.c -pthread || exit 1

mdconfig -l | grep -q md$mdstart && mdconfig -d -u $mdstart
truncate -s 2g $diskimage
mdconfig -a -t vnode -f $diskimage -u $mdstart
../testcases/swap/swap -t 3m -i 20 -l 100 > /dev/null &
sleep 3
cd /tmp; ./$prog; s=$?; cd -
while pkill swap; do sleep .1; done
wait
rm -f /tmp/$prog.c /tmp/$prog $diskimage
mdconfig -d -u $mdstart
exit $s
