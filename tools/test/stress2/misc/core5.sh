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

# The core file vnode is unreferenced before notification is sent.

# Problem reported by sbruno@
# http://people.freebsd.org/~pho/stress/log/core5.txt
# Fixed by r279237.

# 20150714 Slowdown seen with core5 waiting in vlruwk.
# sysctl vfs.vlru_allow_cache_src=1 used to resolve this.
# For now change MAXVNODES from 1.000 to 4.000.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > core5.c
mycc -o core5 -Wall -Wextra -O0 -g core5.c || exit 1
rm -f core5.c

cat > core5-dumper.c << EOT
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

int
main(int argc __unused, char *argv[])
{
	time_t start;
	char core[80];

	snprintf(core, sizeof(core), "%s.core", argv[0]);

	if (unlink(core) == -1)
		if (errno != ENOENT)
			warn("unlink(%s)", core);

	start = time(NULL);
	while (time(NULL) - start < 600) {
		if (fork() == 0)
			raise(SIGSEGV);
		wait(NULL);
	}
	if (unlink(core) == -1)
		if (errno != ENOENT)
			warn("unlink(%s)", core);

	return (0);
}
EOT
mycc -o core5-dumper -Wall -Wextra -O0 -g core5-dumper.c || exit 1
rm -f core5-dumper.c
for i in `jot 10`; do
	cp core5-dumper core5-dumper$i
done
rm -f core5-dumper

mount | grep -q "on $mntpoint " && umount $mntpoint
[ -c /dev/md$mdstart ] && mdconfig -d -u $mdstart

mdconfig -a -t malloc -s 1g -u $mdstart

newfs -b 4096 -f 512 -i 2048 md$mdstart > /dev/null
mount -o async /dev/md$mdstart $mntpoint || exit 1

cp /tmp/core5 $mntpoint
mkdir $mntpoint/dir
cd $mntpoint

mp2=${mntpoint}2
[ -d $mp2 ] || mkdir $mp2
mount | grep -q "on $mp2 " && umount $mp2
mount -o size=2g -t tmpfs tmpfs $mp2 || exit 1
for i in `jot 10`; do
	(cd $mp2; /tmp/core5-dumper$i ) &
done
maxvnodes=`sysctl -n kern.maxvnodes`
trap "sysctl kern.maxvnodes=$maxvnodes > /dev/null" EXIT INT
$mntpoint/core5 $mntpoint/dir
wait
umount $mp2

cd $here
while mount | grep -q "on $mntpoint "; do
        umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
rm -f /tmp/core5 /tmp/core5-dumper* /tmp/core5-dumper*.core
exit 0
EOF
#include <sys/wait.h>
#include <sys/sysctl.h>

#include <err.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define MAXVNODES 4000
#define NBFILES 10000
#define PARALLEL 4
#define RTIME (10 * 60)

char *path;

void
test(int n)
{
	int fd, i;
	char file[80];

	usleep(arc4random() % 1000);
	for (i = 0; i < NBFILES; i++) {
		snprintf(file, sizeof(file), "%s/f%d.%06d", path, n, i);
		if ((fd = open(file, O_CREAT, 0644)) == -1) {
			warn("open(%s)", file);
			break;
		}
		close(fd);
	}
	for (i = 0; i < NBFILES; i++) {
		snprintf(file, sizeof(file), "%s/f%d.%06d", path, n, i);
		if (unlink(file) == -1)
			err(1, "unlink(%s)", file);
	}

	_exit(0);
}

int
main(int argc, char *argv[])
{
	size_t len;
	time_t start;
	unsigned long nv, maxvnodes;
	int j;

	if (argc != 2)
		errx(1, "Usage: %s <path>", argv[0]);
	path = argv[1];

	nv = MAXVNODES;
	len = sizeof(maxvnodes);
	if (sysctlbyname("kern.maxvnodes", &maxvnodes, &len, &nv,
	    sizeof(nv)) != 0)
		err(1, "sysctl kern.maxvnodes 1");

	start = time(NULL);
	while (time(NULL) - start < RTIME) {
		for (j = 0; j < PARALLEL; j++)
			if (fork() == 0)
				test(j);

		for (j = 0; j < PARALLEL; j++)
			wait(NULL);
	}

	if (sysctlbyname("kern.maxvnodes", NULL, NULL, &maxvnodes,
	    sizeof(maxvnodes)) != 0)
		err(1, "sysctl kern.maxvnodes 2");

	return (0);
}
