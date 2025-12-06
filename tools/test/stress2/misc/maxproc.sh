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

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

# Test that a non root user can at most have maxproc - 10 processes.

. ../default.cfg

[ `sysctl -n kern.maxproc` -gt 37028 ] && exit 0	# Excessive run time
here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > maxproc.c
mycc -o maxproc -Wall -Wextra maxproc.c -lkvm || exit 1
rm -f maxproc.c
cd $here

/tmp/maxproc

rm -f /tmp/maxproc
exit
EOF
#include <sys/wait.h>

#include <err.h>
#include <fcntl.h>
#include <kvm.h>
#include <limits.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

enum {
	NL_NPROCS,
	NL_MAXPROC,
	NL_MARKER
};

static struct {
	int order;
	const char *name;
} namelist[] = {
	{ NL_NPROCS, "_nprocs" },
	{ NL_MAXPROC, "_maxproc" },
	{ NL_MARKER, "" },
};

#define NNAMES	(sizeof(namelist) / sizeof(*namelist))
#define MULTIUSERFUZZ 5

static struct nlist nl[NNAMES];

static void
t2(void)
{
	pid_t p;

	for (;;) {
		if ((p = fork()) == 0) {
			sleep(2);
			_exit(0);
		}
		if (p == -1)
			break;
	}
}

static void
t1(int priv)
{
	pid_t p;
	struct passwd *pw;

	if ((p = fork()) == 0) {
		if ((pw = getpwnam("nobody")) == NULL)
			err(1, "no such user: nobody");

		if (priv == 0) {
			if (setgroups(0, NULL) ||
			    setegid(pw->pw_gid) || setgid(pw->pw_gid) ||
			    seteuid(pw->pw_uid) || setuid(pw->pw_uid))
				err(1, "Can't drop privileges to \"nobody\"");
		}
		endpwent();

		t2();
		_exit(0);
	}
	waitpid(p, NULL, 0);
}

int
getprocs(void)
{
	kvm_t *kd;
	int i, nprocs, maxproc;
	char buf[_POSIX2_LINE_MAX];
	char *nlistf, *memf;

	nlistf = memf = NULL;
	for (i = 0; i < (int)NNAMES; i++)
		nl[namelist[i].order].n_name = strdup(namelist[i].name);

	if ((kd = kvm_openfiles(nlistf, memf, NULL, O_RDONLY, buf)) == NULL)
		errx(1, "kvm_openfile(%s, %s): %s", nlistf, memf, buf);
	if (kvm_nlist(kd, nl) == -1)
		errx(1, "kvm_nlist: %s", kvm_geterr(kd));
	if (kvm_read(kd, nl[NL_NPROCS].n_value, &nprocs,
	    sizeof(nprocs)) != sizeof(nprocs))
		errx(1, "kvm_read(): %s", kvm_geterr(kd));
	if (kvm_read(kd, nl[NL_MAXPROC].n_value, &maxproc,
	    sizeof(maxproc)) != sizeof(maxproc))
		errx(1, "kvm_read(): %s", kvm_geterr(kd));
	kvm_close(kd);

	return (maxproc - nprocs - 1);
}

int
main(void)
{
	int i, n;

	alarm(1200);
	n = getprocs();
	for (i = 0; i < n / 10 * 8; i++) {
		if (fork() == 0) {
			sleep(2);
			_exit(0);
		}
	}

	t1(0);

	n = getprocs();
	if (n < 10 - MULTIUSERFUZZ)
		errx(1, "FAIL: nprocs = %d\n", n);

	return (0);
}
