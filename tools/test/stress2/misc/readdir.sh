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

# readdir(3) fuzzing inspired by the iknowthis test suite
# by Tavis Ormandy <taviso  cmpxchg8b com>

# "panic: kmem_malloc(1328054272): kmem_map too small" seen

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > readdir.c
mycc -o readdir -Wall -Wextra readdir.c || exit 1
rm -f readdir.c

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart

mount -t tmpfs tmpfs $mntpoint
echo "Testing tmpfs(5)"
cp -a /usr/include $mntpoint
/tmp/readdir $mntpoint
umount $mntpoint

echo "Testing fdescfs(5)"
kldstat -v | grep -q fdescfs || { kldload fdescfs.ko; loaded=1; }
mount -t fdescfs null /dev/fd
/tmp/readdir /dev/fd
umount /dev/fd
[ $unload ] && kldunload fdescfs.ko

echo "Testing procfs(5)"
mount -t procfs procfs $mntpoint
/tmp/readdir $mntpoint
umount $mntpoint

if ping -c 2 `echo $nfs_export | sed 's/:.*//'` > /dev/null 2>&1; then
	echo "Testing nfs"
	mount -t nfs -o nfsv3,tcp,nolockd,retrycnt=3,soft,timeout=1 \
	    $nfs_export $mntpoint
	/tmp/readdir $mntpoint
	umount $mntpoint
fi

mdconfig -a -t swap -s 1g -u $mdstart || exit 1
newfs md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
cp -a /usr/include $mntpoint
echo "Testing UFS"
/tmp/readdir $mntpoint
umount $mntpoint
mdconfig -d -u $mdstart

mdconfig -a -t swap -s 1g -u $mdstart || exit 1
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
cp -a /usr/include $mntpoint
echo "Testing FFS"
/tmp/readdir $mntpoint
umount $mntpoint
mdconfig -d -u $mdstart

mount -t nullfs /bin $mntpoint
echo "Testing nullfs(5)"
/tmp/readdir $mntpoint
umount $mntpoint

rm -f /tmp/readdir
exit 0
EOF
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>

#include <dirent.h>
#include <err.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#define RUNTIME 120

/* copy from /usr/src/lib/libc/gen/gen-private.h */
struct _telldir;		/* see telldir.h */
struct pthread_mutex;

/*
 * Structure describing an open directory.
 *
 * NOTE. Change structure layout with care, at least dd_fd field has to
 * remain unchanged to guarantee backward compatibility.
 */
struct _dirdesc {
	int	dd_fd;		/* file descriptor associated with directory */
	long	dd_loc;		/* offset in current buffer */
	long	dd_size;	/* amount of data returned by getdirentries */
	char	*dd_buf;	/* data buffer */
	int	dd_len;		/* size of data buffer */
	long	dd_seek;	/* magic cookie returned by getdirentries */
	long	dd_rewind;	/* magic cookie for rewinding */
	int	dd_flags;	/* flags for readdir */
	struct pthread_mutex	*dd_lock;	/* lock */
	struct _telldir *dd_td;	/* telldir position recording */
};
/* End copy */

static void
hand(int i __unused) {	/* handler */
	_exit(1);
}

static void
test(char *path)
{
	DIR *dirp, fuzz;
	int i;

	signal(SIGSEGV, hand);
	alarm(300);
	for (i = 0; i < 2000; i++) {
		if ((dirp = opendir(path)) == NULL)
			break;
		bcopy(dirp, &fuzz, sizeof(fuzz));
		fuzz.dd_len = arc4random();
		readdir(&fuzz);
		closedir(dirp);
	}

	_exit(0);
}

int
main(int argc __unused, char **argv)
{
	time_t start;

	start = time(NULL);
	while (time(NULL) - start < RUNTIME) {
		if (fork() == 0)
			test(argv[1]);
		wait(NULL);
	}

	return (0);
}
