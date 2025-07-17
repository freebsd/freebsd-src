#!/bin/sh

#
# Copyright (c) 2024 Peter Holm <pho@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

# Demonstrate issue described in:
# [Bug 276002] nfscl: data corruption using both copy_file_range and mmap'd I/O

# This version only uses mapped read/write, read(2)/write(2) and ftruncate(2)

# Issue seen:

# 19:50:53 Start test of mmap47.sh
# 19:51:56, elapsed 0 days, 00:01.03
# 19:53:01, elapsed 0 days, 00:02.08
# 19:54:06, elapsed 0 days, 00:03.13
# 19:55:40, elapsed 0 days, 00:04.47
# 617c617
# < 0023200    80  81  82  83  84  85  86  87  88  89  8a  8b  8c  8d  8e  8f
# ---
# > 0023200    80  81  82  83  84  85  86  87  88  89  8a  8b  8c  8d  8e  ee
# 256 -rw-------  1 root wheel 262144 Mar  1 19:56 file
# 256 -rw-------  1 root wheel 262144 Mar  1 19:55 file.orig
# 19:56:44, elapsed 0 days, 00:05.51
# Failed with exit code 2 after 5 loops of mmap47.sh.
# 19:56 /usr/src/tools/test/stress2/misc $ 

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
. ../default.cfg
set -u
prog=$(basename "$0" .sh)
log=/tmp/$prog.log
serial=/tmp/$prog.serial
grep -q $mntpoint /etc/exports ||
    { echo "$mntpoint missing from /etc/exports"; exit 0; }
rpcinfo 2>/dev/null | grep -q mountd || exit 0

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

	while (go == -1)
		usleep(50);
	while (go == 1) {
		i = arc4random() % siz;
		c = cp[i];
		if (c != 0x77) /* No unused vars here */
			usleep(arc4random() % 200);
	}
	return (0);
}

static void *
memwrite(void *arg __unused)
{
	int i;
	char c;

	while (go == -1)
		usleep(50);
	while (go == 1) {
		i = arc4random() % siz;
		pthread_mutex_lock(&write_mutex);
		c = cp[i];
		cp[i] = 0xee;	/* This value seems to linger with NFS */
		cp[i] = c;
		pthread_mutex_unlock(&write_mutex);
		usleep(arc4random() % 200);
	}
	return (0);
}

static void *
wr(void *arg __unused)
{
	off_t pos;
	int r, s;
	char buf[1024];

	while (go == -1)
		usleep(50);
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
		usleep(arc4random() % 200);
	}
	return (0);
}

/* Both ftruncate() and fdatasync() triggers the problem */

static void *
tr(void *arg __unused)
{
	while (go == -1)
		usleep(50);
	while (go == 1) {
#if 0
		if (ftruncate(fd, siz) == -1) /* No size change */
			err(1, "truncate)");
#else
		if (fdatasync(fd) == -1)
			err(1, "fdatasync()");
#endif
		usleep(arc4random() % 1000);
	}
	return (0);
}

int
main(int argc, char *argv[])
{
	struct stat st;
	pthread_t tp[31];
	int e, i, idx;

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

	go = -1;
	pthread_mutex_init(&write_mutex, NULL);
	idx = 0;
	for (i = 0; i < (int)(arc4random() % 10 + 1); i++) {
		if ((e = pthread_create(&tp[idx++], NULL, memread, NULL)) != 0)
			errc(1, e, "pthread_create");
	}
	for (i = 0; i < (int)(arc4random() % 10 + 1); i++) {
		if ((e = pthread_create(&tp[idx++], NULL, memwrite, NULL)) != 0)
			errc(1, e, "pthread_create");
	}
	for (i = 0; i < (int)(arc4random() % 10 + 1); i++) {
		if ((e = pthread_create(&tp[idx++], NULL, wr, NULL)) != 0)
			errc(1, e, "pthread_create");
	}
	if ((e = pthread_create(&tp[idx++], NULL, tr, NULL)) != 0)
		errc(1, e, "pthread_create");

	sleep(1);
	go = 1;
	sleep(60);
	go = 0;
	for (i = 0; i < idx; i++)
		pthread_join(tp[i], NULL);
	if (munmap(cp, siz) == -1)
		err(1, "munmap()");
	close(fd);
}
EOF
mycc -o /tmp/$prog -Wall -Wextra -O0 /tmp/$prog.c -lpthread || exit 1

mycc -o $serial -Wall -Wextra -O2 ../tools/serial.c || exit 1
mount | grep -q "on $mntpoint " && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart && mdconfig -d -u $mdstart
mdconfig -s 5g -u $mdstart
newfs -n $newfs_flags /dev/md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint

mp2=${mntpoint}2
mkdir -p $mp2
mount | grep -q "on $mp2 " && umount -f $mp2
mount -t nfs -o retrycnt=3 127.0.0.1:$mntpoint $mp2 || exit 1
sleep .2

here=`pwd`
cd $mp2
$here/../testcases/swap/swap -t 5m -i 20 > /dev/null &
sleep 2

size=262144
$serial file $size
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
rm -f $serial /tmp/$prog /tmp/$prog.c $log
exit $s
