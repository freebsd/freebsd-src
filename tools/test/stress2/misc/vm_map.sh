#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
# Copyright (c) 2019 Dell EMC Isilon
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

# Test scenario based on alc@'s suggestion for D19826
# No problems seen.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/vm_map.c
mycc -o vm_map -Wall -Wextra -O2 vm_map.c || exit 1
rm -f vm_map.c

pages=$((`sysctl -n hw.usermem` / `sysctl -n hw.pagesize`))
[ `sysctl -n vm.swap_total` -eq 0 ] &&
    pages=$((pages / 10 * 8))
proccontrol -m aslr -s disable /tmp/vm_map $pages

rm -f /tmp/vm_map
exit $s
EOF
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static long n;

#define RUNTIME (2 * 60)
#define INCARNATIONS 32

int
test(void)
{
	size_t len;
	time_t start;
	void *a;
	char **c;
	int i;

	a = NULL;
	c = calloc(n, sizeof(char *));
	len = PAGE_SIZE;
        start = time(NULL);
        while ((time(NULL) - start) < RUNTIME) {
		for (i = 0; i < n; i++) {
			if ((a = mmap((void *)a, len, PROT_READ | PROT_WRITE,
			    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
				err(1, "mmap");
			c[i] = a;
			*c[i] = 1;
			a += PAGE_SIZE;
		}
		for (i = 0; i < n; i++) {
			if (munmap(c[i], PAGE_SIZE) == -1)
				err(1, "munmap(%p)", c[i]);
		}
		a = NULL;
	}

	_exit(0);
}

int
main(int argc, char *argv[])
{
	int i;

	if (argc != 2) {
		fprintf(stderr, "Usage %s <pages>\n", argv[0]);
		exit(1);
	}
	n = atol(argv[1]) / INCARNATIONS;
	for (i = 0; i < INCARNATIONS; i++)
		if (fork() == 0)
			test();

	for (i = 0; i < INCARNATIONS; i++)
		wait(NULL);

	return (0);
}
