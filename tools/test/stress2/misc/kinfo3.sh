#!/bin/sh

#
# Copyright (c) 2008 Peter Holm <pho@FreeBSD.org>
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

# Test scenario by marcus@freebsd.org and kib@freebsd.org

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

odir=`pwd`
cd /tmp
sed '1,/^EOF/d' < $odir/$0 > kinfo3.c
mycc -o kinfo3 -Wall -Wextra -O0 kinfo3.c -lutil -pthread || exit 1
rm -f kinfo3.c

s=0
mount | grep -q procfs || mount -t procfs procfs /proc
start=`date '+%s'`
while [ $((`date '+%s'` - start)) -lt 1200 ]; do
	pids=""
	for i in `jot 5`; do
		timeout 5m /tmp/kinfo3 &
		pids="$pids $!"
	done
	for pid in $pids; do
		wait $pid
		r=$?
		[ $r -ne 0 ] && { s=1; echo "Exit code $r"; break; }
	done
done

rm -f /tmp/kinfo3
exit $s
EOF

#include <sys/types.h>
#include <sys/signal.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <sys/wait.h>

#include <err.h>
#include <fcntl.h>
#include <libutil.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

static char buf[8096];
static volatile sig_atomic_t more;

static void
handler(int i __unused) {

	more = 0;
}

static void *
thr(void *arg __unused)
{
	int fd;

	if ((fd = open("/proc/curproc/mem", O_RDONLY)) == -1)
		err(1, "open(/proc/curproc/mem)");
	close(fd);
	return (0);
}

/* Stir /dev/proc */
static int
churning(void) {
	pid_t r;
	pthread_t threads[5];
	int i, status;;

	while(more) {
		r = fork();
		if (r == 0) {
			for (i = 0; i < 5; i++) {
				if ((r = pthread_create(&threads[i], NULL, thr, 0)) != 0)
					errc(1, r, "pthread_create()");
			}
			for (i = 0; i < 5; i++) {
				if ((r = pthread_join(threads[i], NULL)) != 0)
						errc(1, r, "pthread_join(%d)", i);
			}

			bzero(buf, sizeof(buf));
			_exit(0);
		}
		if (r < 0) {
			perror("fork");
			exit(2);
		}
		wait(&status);
	}
	_exit(0);
}

/* Get files for each proc */
static void
list(void)
{
	struct kinfo_proc *kipp;
	struct kinfo_vmentry *freep_vm;
        struct kinfo_file *freep, *kif __unused;
	size_t len;
	long i, j;
	int cnt, name[4];

	name[0] = CTL_KERN;
	name[1] = KERN_PROC;
	name[2] = KERN_PROC_PROC;

	len = 0;
	if (sysctl(name, 3, NULL, &len, NULL, 0) < 0)
		err(-1, "sysctl: kern.proc.all");

	kipp = malloc(len);
	if (kipp == NULL)
		err(1, "malloc");

	if (sysctl(name, 3, kipp, &len, NULL, 0) < 0) {
		free(kipp);
//		warn("sysctl: kern.proc.all");
		return;
	}

	for (i = 0; i < (long)(len / sizeof(*kipp)); i++) {

		/* The test starts here */
		freep = kinfo_getfile(kipp[i].ki_pid, &cnt);
		for (j = 0; j < cnt && freep; j++) {
			kif = &freep[j];
//			printf("%d : %s\n", kif->kf_fd, kif->kf_path);
		}
		free(freep);

		freep_vm = kinfo_getvmmap(kipp[i].ki_pid, &cnt);
		free(freep_vm);
		/* End test */
	}
	free(kipp);
}

int
main(void)
{
	pid_t r;

	signal(SIGALRM, handler);
	alarm(30);

	more = 1;
	if ((r = fork()) == 0) {
		alarm(30);
		while(more)
			churning();
	}
	if (r < 0) {
		perror("fork");
		exit(2);
	}

	while(more)
		list();

	return (0);
}
