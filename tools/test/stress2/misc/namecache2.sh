#!/bin/sh

#
# Copyright (c) 2013 Peter Holm
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

# UFS cache inconsistancy for rename(2) demonstrated
# Fails with:
#    ls -ali /mnt
#    ls: tfa1022: No such file or directory
# Fixed by r248422

# Test scenario obtained from Rick Miller <vmiller at hostileadmin com>

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

#  This threaded test is designed for MP.
[ `sysctl hw.ncpu | sed 's/.* //'` -eq 1 ] && exit 0

. ../default.cfg

odir=`pwd`
cd /tmp
sed '1,/^EOF/d' < $odir/$0 > namecache2.c
rm -f /tmp/namecache2
mycc -o namecache2 -Wall -Wextra -g -O2 namecache2.c -lpthread || exit 1
rm -f namecache2.c
cd $odir

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 1g -u $mdstart
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint

(cd $mntpoint; /tmp/namecache2)

f=`(cd $mntpoint; echo *)`
if [ "$f" != '*' ]; then
	echo FAIL
	echo "echo $mntpoint/*"
	echo $mntpoint/*
	 echo ""
	echo "ls -ali $mntpoint"
	ls -ali $mntpoint
	echo ""
	echo "fsdb -r /dev/md$mdstart"
	fsdb -r /dev/md$mdstart <<-EF
	ls
	quit
	EF
fi

while mount | grep $mntpoint | grep -q /dev/md; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
rm -f /tmp/namecache2
exit 0
EOF
/*
 * NOTE: This must be run with the current working directory on a local UFS
 * disk partition, to demonstrate a FreeBSD namecache bug. I have never seen
 * this bug happen with an NFS partition.
 */

#include <pthread.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

int stopping = false;
char *pFilename = 0;

static void    *
statThread(void *arg __unused)
{

	struct stat statData;
	int rc;

	for (;;) {
		while (pFilename == 0) {
			if (stopping)
				return 0;
		}

		rc = stat(pFilename, &statData);
		if (rc < 0 && errno != ENOENT) {
			printf(" statThread stat() on %s failed with errno %d\n",
			       pFilename, errno);
			return 0;
		}
	}

	return 0;
}

int
main(void)
{
	char filename1 [20], filename2[20], filename3[20];
	pthread_t threadId;
	struct stat statData;
	int result, fd;
	unsigned int number;
	struct timespec	period;
	time_t start;

	sprintf(filename1, "tfa0");
	fd = open(filename1, O_CREAT, S_IRWXU);
	if (fd < 0) {
		printf("open(O_CREAT) on %s failed with errno %d\n", filename1, errno);
		return 0;
	}
	if (close(fd) < 0) {
		printf("close() on %s failed with errno %d\n", filename1, errno);
		return 0;
	}
	result = pthread_create(&threadId, NULL, statThread, NULL);
	if (result < 0)
		errc(1, result, "pthread_create()");

	start = time(NULL);
	for (number = 0; number < 0x001FFFFF; number += 2) {
		sprintf(filename1, "tfa%u", number);
		sprintf(filename2, "tfa%u", number + 1);
		sprintf(filename3, "tfa%u", number + 2);
		if (rename(filename1, filename2) < 0) {
			printf(" rename1() from %s to %s failed with errno %d\n",
			       filename1, filename2, errno);
			return 0;
		}
		pFilename = filename3;

		if (rename(filename2, filename3) < 0) {
			printf(" rename2() from %s to %s failed with errno %d\n",
			       filename2, filename3, errno);
			return 0;
		}
		pFilename = 0;
		period.tv_sec = 0;
		period.tv_nsec = 500;
		nanosleep(&period, 0);

		if (stat(filename3, &statData) < 0) {
			printf("stat(%s) failed with errno %d\n", filename3, errno);
			stopping = true;
			period.tv_sec = 0;
			period.tv_nsec = 500;
			nanosleep(&period, 0);
			return 0;
		}
		if (time(NULL) - start > 1200) {
			fprintf(stderr, "Test timed out.\n");
			break;
		}
	}
	unlink(filename3);

	return 0;
}
