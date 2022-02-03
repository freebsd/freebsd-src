#!/bin/sh

#
# Copyright (c) 2012 Peter Holm
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

# Demonstrate that close() of an flock'd file is not atomic.
# Fails with "flock_open_close: execv(/mnt/test): Text file busy"

# Test scenario by: jhb

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

odir=`pwd`
cd /tmp
sed '1,/^EOF/d' < $odir/$0 > flock_open_close.c
rm -f /tmp/flock_open_close
mycc -o flock_open_close -Wall -Wextra -O2 -g flock_open_close.c -lpthread || exit 1
rm -f flock_open_close.c

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 1g -u $mdstart || exit 1
bsdlabel -w md$mdstart auto
newfs $newfs_flags md${mdstart}$part > /dev/null
mount /dev/md${mdstart}$part $mntpoint
chmod 777 $mntpoint

cp /bin/test $mntpoint
chown $testuser $mntpoint/test
chmod +w $mntpoint/test

su $testuser -c "/tmp/flock_open_close $mntpoint/test" &
pid=$!
while kill -0 $! 2>/dev/null; do
	mksnap_ffs $mntpoint $mntpoint/.snap/snap
	sleep 2
	rm -f $mntpoint/.snap/snap
	sleep 1
done
wait $pid
s=$?

for i in `jot 10`; do
	mount | grep -q md${mdstart}$part  && \
		umount $mntpoint && mdconfig -d -u $mdstart && break
	sleep 2
done
if mount | grep -q md${mdstart}$part; then
	fstat $mntpoint
	echo "umount $mntpoint failed"
	exit 1
fi
rm -f /tmp/flock_open_close
exit $s
EOF

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void
usage(void)
{
	fprintf(stderr, "Usage: flock_close_race <binary> [args]\n");
	exit(1);
}

static void
child(const char *binary)
{
	int fd;

	/* Exit as soon as our parent exits. */
	while (getppid() != 1) {
		fd = open(binary, O_RDWR | O_EXLOCK);
		if (fd < 0) {
			/*
			 * This may get ETXTBSY since exit() will
			 * close its open fd's (thus releasing the
			 * lock), before it releases the vmspace (and
			 * mapping of the binary).
			 */
			if (errno == ETXTBSY)
				continue;
			err(2, "can't open %s", binary);
		}
		close(fd);
	}
	exit(0);
}

static void
exec_child(char **av)
{

	(void)open(av[0], O_RDONLY | O_SHLOCK);
	execv(av[0], av);
	/* "flock_open_close: execv(/mnt/test): Text file busy" seen */
	err(127, "execv(%s)", av[0]);
}

int
main(int ac, char **av)
{
	struct stat sb;
	pid_t pid;
	int e, i, status;

	if (ac < 2)
		usage();
	if (stat(av[1], &sb) != 0)
		err(1, "stat(%s)", av[1]);
	if (!S_ISREG(sb.st_mode))
		errx(1, "%s not an executable", av[1]);

	pid = fork();
	if (pid < 0)
		err(1, "fork");
	if (pid == 0)
		child(av[1]);
	e = 0;
	for (i = 0; i < 200000; i++) {
		pid = fork();
		if (pid < 0)
			err(1, "vfork");
		if (pid == 0)
			exec_child(av + 1);
		wait(&status);
		if (WIFEXITED(status) && WEXITSTATUS(status) == 127) {
			fprintf(stderr, "FAIL\n");
			e = 1;
			break;
		}
		if (WIFEXITED(status) && WEXITSTATUS(status) != 1) {
			/* /bin/test returns 1 */
			e = 1;
			break;
		}
	}
	return (e);
}
