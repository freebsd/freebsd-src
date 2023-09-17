#!/bin/sh

#
# Copyright (c) 2011 Peter Holm <pho@FreeBSD.org>
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

# pread(2) fuzzing inspired by the iknowthis test suite
# by Tavis Ormandy <taviso  cmpxchg8b com>

# Fixed in r227527.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > pread.c
mycc -o pread -Wall -Wextra pread.c
rm -f pread.c

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart

mount -t tmpfs tmpfs $mntpoint
cp -a /usr/include $mntpoint
echo "Testing tmpfs(5)"
/tmp/pread $mntpoint
while mount | grep -q "on $mntpoint "; do
	umount $mntpoint || sleep 1
done

echo "Testing fdescfs(5)"
mount -t fdescfs null /dev/fd
for i in `jot 100`; do
	/tmp/pread /dev/fd
done

while mount | grep -q "on /dev/fd "; do
	umount /dev/fd || sleep 1
done

echo "Testing procfs(5)"
mount -t procfs procfs $mntpoint
/tmp/pread $mntpoint
while mount | grep -q "on $mntpoint "; do
	umount $mntpoint || sleep 1
done

mdconfig -a -t swap -s 1g -u $mdstart || exit 1
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
cp -a /usr/include $mntpoint
echo "Testing FFS"
/tmp/pread $mntpoint
while mount | grep -q "on $mntpoint "; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart

mount -t nullfs /bin $mntpoint
echo "Testing nullfs(5)"
/tmp/pread $mntpoint
while mount | grep -q "on $mntpoint "; do
	umount $mntpoint || sleep 1
done

echo "Testing procfs(5)"
mount -t procfs procfs $mntpoint
/tmp/pread $mntpoint
while mount | grep -q "on $mntpoint "; do
	umount $mntpoint || sleep 1
done

echo "Testing devfs(8)"
mount -t devfs devfs $mntpoint
/tmp/pread $mntpoint
while mount | grep -q "on $mntpoint "; do
	umount $mntpoint || sleep 1
done

rm -f /tmp/pread
exit 0
EOF
#include <sys/types.h>
#include <strings.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/wait.h>

static void
hand(int i __unused) {	/* handler */
	_exit(1);
}

int
test(char *path)
{

	FTS		*fts;
	FTSENT		*p;
	int		ftsoptions;
	char		*args[2];
	int buf[64], fd;

	signal(SIGSEGV, hand);
	signal(SIGABRT, hand);
	ftsoptions = FTS_PHYSICAL;
	args[0] = path;
	args[1] = 0;

	if ((fts = fts_open(args, ftsoptions, NULL)) == NULL)
		err(1, "fts_open");

	while ((p = fts_read(fts)) != NULL) {
		if ((fd = open(p->fts_path, O_RDONLY)) == -1) {
			if (errno != EACCES && errno != ENXIO)
				warn("open(%s)", p->fts_path);
			continue;
		}
		alarm(1);
		pread(fd, (void *)0xdeadc0de, 0x7ffffff, 0xffffffff);
		pread(fd, buf, 0x7ffffff, 0xffffffff);
		pread(fd, buf, sizeof(buf), 0xffffffff);
		pread(fd, buf, sizeof(buf), 0);
		close(fd);
	}
	fts_close(fts);

	exit(0);
}

int
main(int argc __unused, char **argv)
{
	int i;
        struct passwd *pw;

        if ((pw = getpwnam("nobody")) == NULL)
                err(1, "no such user: nobody");

        if (setgroups(1, &pw->pw_gid) ||
            setegid(pw->pw_gid) || setgid(pw->pw_gid) ||
            seteuid(pw->pw_uid) || setuid(pw->pw_uid))
                err(1, "Can't drop privileges to \"nobody\"");
        endpwent();

	if (daemon(0, 0) == -1)
		err(1, "daemon()");

	for (i = 0; i < 10; i++) {
		if (fork() == 0)
			test(argv[1]);
		wait(NULL);
	}

	return (0);
}
