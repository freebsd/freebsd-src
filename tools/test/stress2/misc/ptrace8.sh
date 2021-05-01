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

# A regression test for r302919.
# Triggered a witness message:
#
# vmspace_free() called with the following non-sleepable locks held:
# shared rw vm object (vm object) r = 0 locked @ kern/sys_process.c:432

. ../default.cfg

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/ptrace8.c
mycc -o ptrace8 -Wall -Wextra -O0 -g ptrace8.c || exit 1
rm -f ptrace8.c
cd $odir

/tmp/ptrace8
s=$?

rm -rf /tmp/ptrace8
exit $s

EOF
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <machine/atomic.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

volatile u_int *share;

#define SYNC 0

int
main(void)
{
	struct ptrace_vm_entry ent;
	size_t len;
	int pid, r, status;
	char path[MAXPATHLEN + 1];

	len = PAGE_SIZE;
	if ((share = mmap(NULL, len, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");

	if ((pid = fork()) == 0) {
		while (share[SYNC] == 0)
			sleep(1);

		_exit(0);
	}

	if (ptrace(PT_ATTACH, pid, 0, 0) == -1)
		err(1, "ptrace");

	if (waitpid(pid, &status, 0) == -1)
		err(1, "waitpid");
	else if (!WIFSTOPPED(status))
		errx(1, "failed to stop child");

	ent.pve_entry = 0;
	ent.pve_path = path;
	ent.pve_pathlen = sizeof(path);
	do {
		r = ptrace(PT_VM_ENTRY, pid, (caddr_t)&ent, 0);
#if defined(DEBUG)
		if (r == 0)
			fprintf(stderr, "path = %s 0x%lx - 0x%lx\n",
			    ent.pve_path, ent.pve_start, ent.pve_end);
#endif
	} while (r == 0);
	if (r == -1 && errno != ENOENT)
		err(1, "ptrace(PT_VM_ENTRY)");

	share[SYNC] = 1;
	if (ptrace(PT_DETACH, pid, 0, 0) == -1)
		err(1, "ptrace");

	if (waitpid(pid, &status, 0) == -1)
		err(1, "waitpid(%d)", pid);

	return (status != 0);
}
