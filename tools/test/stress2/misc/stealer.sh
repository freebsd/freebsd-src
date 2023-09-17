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

# Process stuck in objtrm wait state
# http://people.freebsd.org/~pho/stress/log/stealer.txt
# Fixed in r263328.

# "panic: freeing free block" seen:
# https://people.freebsd.org/~pho/stress/log/stealer-2.txt

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > stealer.c
mycc -o stealer -Wall -Wextra stealer.c || exit 1
rm -f stealer.c
cd $here
swapoff -a > /dev/null

dd if=/dev/zero of=$diskimage bs=1m count=1k status=none
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart
mdconfig -a -t vnode -f $diskimage -u $mdstart
swapon /dev/md$mdstart

hw=`sysctl hw.pagesize | sed 's/.*: //'`
pages=`sysctl hw.usermem  | sed 's/.*: //'`
pages=$((pages / hw))
echo "`date '+%T'` Test with $pages pages."
su $testuser -c "sh -c \"/tmp/stealer $pages\"" &
sleep 30
while swapinfo | grep -q /dev/md$mdstart; do
	swapoff /dev/md$mdstart 2>&1 |
	    grep -v "Cannot allocate memory"
	sleep 2
done
ps auxwwl | grep -v grep | grep objtrm && echo FAIL
wait

swapon -a > /dev/null

mdconfig -d -u $mdstart
rm -rf /tmp/stealer $diskimage
exit 0
EOF
#include <err.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#define N 200

void
handler(int i __unused)
{
	_exit(0);
}

void
stealer(int pages)
{
	volatile char *c;
	int i, page, size;

	page = getpagesize();
	size = pages * page;
	if ((c = malloc(size)) == 0)
		err(1, "malloc(%d pages)", pages);

	signal(SIGALRM, handler);
	alarm(3 * 60);
	for (;;) {
		i = 0;
		while (i < size) {
			c[i] = 0;
			i += page;
		}
	}
}

int
main(int argc __unused, char **argv)
{
	int i, j, n, pages, status;

	pages = atoi(argv[1]);
	n = pages / N;

	j = 0;
	for ( i = 0; i < N; i++, j++) {
		if (fork() == 0)
			stealer(n);
		pages = pages - n;
	}
	if (pages > 0) {
		j++;
		if (fork() == 0)
			stealer(pages);
	}
	while (j-- > 0)
		if (wait(&status) == -1)
			err(1, "wait()");
	return (0);
}
