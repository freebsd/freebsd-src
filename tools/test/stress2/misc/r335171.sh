#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2018 Dell EMC Isilon
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

# Handle the race between fork/vm_object_split() and faults.
#
# If fault started before vmspace_fork() locked the map, and then during
# fork, vm_map_copy_entry()->vm_object_split() is executed, it is
# possible that the fault instantiate the page into the original object
# when the page was already copied into the new object (see
# vm_map_split() for the orig/new objects terminology). This can happen
# if split found a busy page (e.g. from the fault) and slept dropping
# the objects lock, which allows the swap pager to instantiate
# read-behind pages for the fault.  Then the restart of the scan can see
# a page in the scanned range, where it was already copied to the upper
# object.

# No problems seen.

. ../default.cfg

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/template.c
mycc -o template -Wall -Wextra -O0 -g template.c || exit 1
rm -f template.c
export MAXSWAPPCT=101
(cd $odir/../testcases/swap; ./swap -t 5m -i 30 -l 100 -h) > /dev/null &
$dir/template > /dev/null
s=$?
[ -f template.core -a $s -eq 0 ] &&
    { ls -l template.core; mv template.core $dir; s=1; }
wait

rm -rf $dir/template
exit $s
EOF
#include <sys/param.h>
#include <sys/mman.h>
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

static volatile u_int *share;
static volatile char *cp;

#define PARALLEL 16
#define RUNTIME (5 * 60)
#define SYNC 0

static void
test(void)
{
	size_t len;
	pid_t pid;
	int i;

	atomic_add_int(&share[SYNC], 1);
	while (share[SYNC] != PARALLEL)
		usleep(200);
	len = 1280 * PAGE_SIZE;
	if ((cp = mmap(NULL, len, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_PRIVATE, -1, 0)) == MAP_FAILED)
		err(1, "mmap");

	for (i = 0; i < (int)(len / sizeof(char)); i += PAGE_SIZE)
		cp[i] = 1;
	usleep(arc4random() % 500000);
	if ((pid = fork()) == 0) {
		usleep(arc4random() % 1000);
		for (i = 0; i < (int)(len); i += PAGE_SIZE)
			cp[i] = 2;
		fprintf(stdout, ".\n"); fflush(stdout);
		_exit(0);
	}
	if (pid == -1)
		err(1, "fork()");
	for (i = 0; i < (int)(len / sizeof(char)); i += PAGE_SIZE)
		cp[i] = 3;
	if (waitpid(pid, NULL, 0) != pid)
		err(1, "waitpid(%d)", pid);

	_exit(0);
}

int
main(void)
{
	pid_t pids[PARALLEL];
	size_t len;
	time_t start;
	int e, i, status;

	e = 0;
	len = PAGE_SIZE;
	if ((share = mmap(NULL, len, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");

	start = time(NULL);
	while ((time(NULL) - start) < RUNTIME && e == 0) {
		share[SYNC] = 0;
		for (i = 0; i < PARALLEL; i++) {
			if ((pids[i] = fork()) == 0)
				test();
			if (pids[i] == -1)
				err(1, "fork()");
		}
		for (i = 0; i < PARALLEL; i++) {
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

	return (e);
}
