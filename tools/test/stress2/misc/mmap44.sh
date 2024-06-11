#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2024 Peter Holm <pho@FreeBSD.org>
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

# Demonstrate issue described in:
# [Bug 276002] nfscl: data corruption using both copy_file_range and mmap'd I/O

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
. ../default.cfg
set -u
prog=$(basename "$0" .sh)
log=/tmp/$prog.log
grep -q $mntpoint /etc/exports ||
    { echo "$mntpoint missing from /etc/exports"; exit 0; }

cat > /tmp/$prog.c <<EOF
#include <sys/mman.h>
#include <sys/stat.h>

#include <err.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static off_t siz;
static pthread_mutex_t write_mutex;
static int fd, go;
static char *cp;

static void *
memread(void *arg __unused)
{
	int i;
	char c;

	while (go == 1) {
		i = arc4random() % siz;
		c = cp[i];
		if (c != 0x77) /* No unused vars here */
			usleep(arc4random() % 400);
	}
	return (0);
}

static void *
memwrite(void *arg __unused)
{
	int i;
	char c;

	while (go == 1) {
		i = arc4random() % siz;
		pthread_mutex_lock(&write_mutex);
		c = cp[i];
		cp[i] = 0xee;	/* This value seems to linger with NFS */
		cp[i] = c;
		pthread_mutex_unlock(&write_mutex);
		usleep(arc4random() % 400);
	}
	return (0);
}

static void *
wr(void *arg __unused)
{
	off_t pos;
	int r, s;
	char buf[1024];

	while (go == 1) {
		s = arc4random() % sizeof(buf) + 1;
		pos = arc4random() % (siz - s);
		pthread_mutex_lock(&write_mutex);
		if (lseek(fd, pos, SEEK_SET) == -1)
			err(1, "lseek(%d)", (int)pos);
		if ((r = read(fd, buf, s)) != s) {
			fprintf(stderr, "r = %d, s = %d, pos = %d\n", r, s, (int)pos);
			err(1, "read():2");
		}
		if (lseek(fd, pos, SEEK_SET) == -1)
			err(1, "lseek(%d)", (int)pos);
		if (write(fd, buf, s) != s)
			err(1, "write()");
		pthread_mutex_unlock(&write_mutex);
		usleep(arc4random() % 400);
	}
	return (0);
}

static void *
s1(void *arg __unused)
{

	while (go == 1) {
		if (fdatasync(fd) == -1)
			err(1, "fdatasync()");
		usleep(arc4random() % 1000);
	}
	return (0);
}

static void *
s2(void *arg __unused)
{

	while (go == 1) {
		if (fsync(fd) == -1)
			err(1, "fdatasync()");
		usleep(arc4random() % 1000);
	}
	return (0);
}

static void *
tr(void *arg __unused)
{
	int i, s;
	char buf[1024];

	memset(buf, 0x5a, sizeof(buf));
	while (go == 1) {
		pthread_mutex_lock(&write_mutex);
		if (lseek(fd, arc4random() % siz, SEEK_END) == -1)
			err(1, "lseek() END");
		s = sizeof(buf);
		for (i = 0; i < 50; i++) {
			if (write(fd, buf, s) != s)
				warn("write()");
		}
		if (ftruncate(fd, siz) == -1)
			err(1, "truncate()");
		pthread_mutex_unlock(&write_mutex);
		usleep(arc4random() % 400);
	}
	return (0);
}

int
main(int argc, char *argv[])
{
	struct stat st;
	pthread_t tp[6];
	int e, i;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <file>\n", argv[0]);
		exit(1);
	}
	if ((fd = open(argv[1], O_RDWR)) == -1)
		err(1, "open(%s)", argv[1]);
	if (fstat(fd, &st) == -1)
		err(1, "stat(%s)", argv[1]);
	siz = st.st_size;
	cp = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (cp == MAP_FAILED)
		err(1, "mmap()");

	go = 1;
	pthread_mutex_init(&write_mutex, NULL);
	if ((e = pthread_create(&tp[0], NULL, memwrite, NULL)) != 0)
		errc(1, e, "pthread_create");
	if ((e = pthread_create(&tp[1], NULL, memread, NULL)) != 0)
		errc(1, e, "pthread_create");
	if ((e = pthread_create(&tp[2], NULL, wr, NULL)) != 0)
		errc(1, e, "pthread_create");
	if ((e = pthread_create(&tp[3], NULL, s1, NULL)) != 0)
		errc(1, e, "pthread_create");
	if ((e = pthread_create(&tp[4], NULL, s2, NULL)) != 0)
		errc(1, e, "pthread_create");
	if ((e = pthread_create(&tp[5], NULL, tr, NULL)) != 0)
		errc(1, e, "pthread_create");

	sleep(60);
	go = 0;
	for (i = 0; i < (int)(sizeof(tp) / sizeof(tp[0])); i++)
		pthread_join(tp[i], NULL);
	if (munmap(cp, siz) == -1)
		err(1, "munmap()");
	close(fd);
}
EOF
mycc -o /tmp/$prog -Wall -Wextra -O0 /tmp/$prog.c -lpthread || exit 1

mycc -o /tmp/serial -Wall -Wextra -O2 ../tools/serial.c || exit 1
mount | grep -q "on $mntpoint " && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart && mdconfig -d -u $mdstart
mdconfig -s 5g -u $mdstart
newfs -n $newfs_flags /dev/md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint

mp2=${mntpoint}2
mkdir -p $mp2
mount | grep -q "on $mp2 " && umount -f $mp2
mount -t nfs 127.0.0.1:$mntpoint $mp2; s=$?
sleep .2

here=`pwd`
mount | grep $mntpoint
cd $mp2
$here/../testcases/swap/swap -t 5m -i 20 > /dev/null &
sleep 2

size=262144
/tmp/serial file $size
cp file file.orig

s=0
/tmp/$prog file || s=1

while pgrep -q swap; do pkill swap; done
wait
if ! cmp -s file.orig file; then
	od -t x1 file.orig > /var/tmp/$prog.file1
	od -t x1 file      > /var/tmp/$prog.file2
	diff /var/tmp/$prog.file1 /var/tmp/$prog.file2 > $log
	head -20 $log
	rm /var/tmp/$prog.file1 /var/tmp/$prog.file2
	ls -ls file.orig file
	s=2
fi

cd $here
umount $mp2
umount $mntpoint
mdconfig -d -u $mdstart
rm -f /tmp/serial /tmp/$prog /tmp/$prog.c $log
exit $s
