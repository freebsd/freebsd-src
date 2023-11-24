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

# Test scenario by marcus@freebsd.org

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

odir=`pwd`
cd /tmp
sed '1,/^EOF/d' < $odir/$0 > kinfo.c
mycc -o kinfo -Wall kinfo.c -lutil || exit 1
rm -f kinfo.c

mount | grep -q procfs || mount -t procfs procfs /proc
for i in `jot 20`; do
	for j in `jot 5`; do
		/tmp/kinfo &
	done

	for j in `jot 5`; do
		wait
	done
done

rm -f /tmp/kinfo
exit
EOF

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/sysctl.h>
#include <sys/param.h>
#include <sys/user.h>
#include <sys/signal.h>
#include <fcntl.h>
#include <err.h>
#include <strings.h>
#include <sys/wait.h>
#include <libutil.h>

char buf[8096];

void
handler(int i) {
	exit(0);
}

/* Stir /dev/proc */
int
churning(void) {
	pid_t r;
	int fd, status;

	for (;;) {
		r = fork();
		if (r == 0) {
			if ((fd = open("/proc/curproc/mem", O_RDONLY)) == -1)
				err(1, "open(/proc/curproc/mem)");
			bzero(buf, sizeof(buf));
			exit(0);
		}
		if (r < 0) {
			perror("fork");
			exit(2);
		}
		wait(&status);
	}
}

/* Get files for each proc */
void
list(void)
{
        struct kinfo_file *freep;
	struct kinfo_vmentry *freep_vm;
	long i;
	int cnt, name[4];
	struct kinfo_proc *kipp;
	size_t len;

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

	for (i = 0; i < len / sizeof(*kipp); i++) {

		/* The test starts here */
		freep = kinfo_getfile(kipp[i].ki_pid, &cnt);
		free(freep);

		freep_vm = kinfo_getvmmap(kipp[i].ki_pid, &cnt);
		free(freep_vm);
		/* End test */
	}
	free(kipp);
}

int
main(int argc, char **argv)
{
	pid_t r;
	signal(SIGALRM, handler);
	alarm(60);

	if ((r = fork()) == 0) {
		alarm(60);
		for (;;)
			churning();
	}
	if (r < 0) {
		perror("fork");
		exit(2);
	}

	for (;;)
		list();

	return (0);
}
