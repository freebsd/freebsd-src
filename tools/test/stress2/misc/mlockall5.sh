#!/bin/sh

#
# Copyright (c) 2015 EMC Corp.
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

# Regression test for r287591:

# From the commit log:
# Remove a check which caused spurious SIGSEGV on usermode access to the
# mapped address without valid pte installed, when parallel wiring of
# the entry happen.  The entry must be copy on write.  If entry is COW
# but was already copied, and parallel wiring set
# MAP_ENTRY_IN_TRANSITION, vm_fault() would sleep waiting for the
# MAP_ENTRY_IN_TRANSITION flag to clear.  After that, the fault handler
# is restarted and vm_map_lookup() or vm_map_lookup_locked() trip over
# the check.  Note that this is race, if the address is accessed after
# the wiring is done, the entry does not fault at all.

# Test scenario by kib@.

. ../default.cfg

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/mlockall5.c
mycc -o mlockall5 -Wall -Wextra -O0 -g mlockall5.c -lpthread || exit 1
rm -f mlockall5.c
cd $odir

mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 512m -u $mdstart || exit 1
newfs $newfs_flags -n md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint || exit 1

(cd $mntpoint; /tmp/mlockall5 || echo FAIL)

n=0
while mount | grep "on $mntpoint " | grep -q /dev/md; do
	umount $mntpoint || sleep 1
	n=$((n + 1))
	[ $n -gt 10 ] && { echo FAIL; exit 1; }
done
mdconfig -d -u $mdstart
rm -rf /tmp/mlockall5
exit

EOF
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
#include <unistd.h>

size_t clen;
volatile u_int share;
int ps;
char *c;

void *
touch(void *arg __unused)
{

	int i;

	while (share == 0)
		;
	for (i = 0; i < (int)clen; i += ps)
		c[i] = 1;

	return (NULL);
}

void *
ml(void *arg __unused)
{
	while (share == 0)
		;
	if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1)
		err(1, "mlock");

	return (NULL);
}

int
test(void)
{
	pthread_t tid[2];
	int i, rc, status;

	if (fork() == 0) {
		alarm(60);
		rc = pthread_create(&tid[0], NULL, touch, NULL);
		if (rc != 0)
			errc(1, rc, "pthread_create()");
		rc = pthread_create(&tid[1], NULL, ml, NULL);
		if (rc != 0)
			errc(1, rc, "pthread_create()");
		share = 1;
		for (i = 0; i < (int)nitems(tid); i++) {
			rc = pthread_join(tid[i], NULL);
			if (rc != 0)
				errc(1, rc, "pthread_join(%d)", i);
		}
		_exit(0);
	}

	if (wait(&status) == -1)
		err(1, "wait");

	return (WTERMSIG(status));
}

int
main(void)
{
	int s;

	ps = getpagesize();
	clen = 32 * 1024;
	if ((c = mmap(NULL, clen, PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED,
	    -1, 0)) == MAP_FAILED)
		err(1, "mmap");

	s = test();

	return (s);
}
