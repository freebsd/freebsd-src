#!/bin/sh

#
# Copyright (c) 2024 Peter Holm <pho@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

# Demonstrate issue described in:
# [Bug 276002] nfscl: data corruption using both copy_file_range and mmap'd I/O

# This version only uses mapped read/write, read(2)/write(2), fstat(2)  and ftruncate(2)

# Issue seen:
# 20241003 10:04:24 all: mmap48.sh
# 5257c5257
# < 0244200    80  81  82  83  84  85  86  87  88  89  8a  8b  8c  8d  8e  8f
# ---
# > 0244200    80  81  82  83  84  85  86  87  88  ee  8a  8b  8c  8d  8e  8f
# 256 -rw-------  1 root wheel 262144 Oct  3 10:05 file
# 256 -rw-------  1 root wheel 262144 Oct  3 10:04 file.orig
# FAIL mmap48.sh exit code 2

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

#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <pthread.h>
#include <pthread_np.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static off_t siz;
static pthread_mutex_t write_mutex;
static int fd, go;
static char *cp;

#define THREADS 100

static void *
memread(void *arg __unused)
{
	int i;
	char c;

	if (arc4random() % 100 < 10)
		return (0);
	pthread_set_name_np(pthread_self(), __func__);
	while (go == -1)
		usleep(50);
	while (go == 1) {
		i = arc4random() % siz;
		c = cp[i];
		if (c != 0x77) /* No unused vars here */
			usleep(arc4random() % 400);
	}
	return (NULL);
}

static void *
memwrite(void *arg __unused)
{
	int i;
	char c;

	if (arc4random() % 100 < 10)
		return (0);
	pthread_set_name_np(pthread_self(), __func__);
	while (go == -1)
		usleep(50);
	while (go == 1) {
		i = arc4random() % siz;
		pthread_mutex_lock(&write_mutex);
		c = cp[i];
		cp[i] = 0xee;	/* This value seems to linger with NFS */
		cp[i] = c;
		pthread_mutex_unlock(&write_mutex);
		usleep(arc4random() % 400);
	}
	return (NULL);
}

static void *
wr(void *arg __unused)
{
	off_t pos;
	int r, s;
	char buf[1024];

	if (arc4random() % 100 < 10)
		return (0);
	pthread_set_name_np(pthread_self(), __func__);
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
	return (NULL);
}

/* Both ftruncate() and fdatasync() triggers the problem */

static void *
sy(void *arg __unused)
{

	if (arc4random() % 100 < 10)
		return (0);
	pthread_set_name_np(pthread_self(), __func__);
	while (go == -1)
		usleep(50);
	while (go == 1) {
		if (fdatasync(fd) == -1)
			err(1, "fdatasync()");
		usleep(arc4random() % 1000);
	}
	return (NULL);
}

static void *
tr(void *arg __unused)
{

	if (arc4random() % 100 < 10)
		return (0);
	pthread_set_name_np(pthread_self(), __func__);
	while (go == -1)
		usleep(50);
	while (go == 1) {
		if (ftruncate(fd, siz) == -1) /* No size change */
			err(1, "truncate)");
		usleep(arc4random() % 1000);
	}
	return (NULL);
}

static void *
fs(void *arg __unused)
{
	struct stat st;

	if (arc4random() % 100 < 10)
		return (0);
	pthread_set_name_np(pthread_self(), __func__);
	while (go == -1)
		usleep(50);
	while (go == 1) {
		if (fstat(fd, &st) == -1)
			err(1, "stat()");
		usleep(arc4random() % 1000);
	}
	return (NULL);
}

int
main(int argc, char *argv[])
{
	struct stat st;
	pthread_t tp[THREADS];
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
	for (i = 0; i < (int)(arc4random() % 3 + 1); i++) {
		if ((e = pthread_create(&tp[idx++], NULL, memread, NULL)) != 0)
			errc(1, e, "pthread_create");
	}
	for (i = 0; i < (int)(arc4random() % 3 + 1); i++) {
		if ((e = pthread_create(&tp[idx++], NULL, memwrite, NULL)) != 0)
			errc(1, e, "pthread_create");
	}
	for (i = 0; i < (int)(arc4random() % 3 + 1); i++) {
		if ((e = pthread_create(&tp[idx++], NULL, wr, NULL)) != 0)
			errc(1, e, "pthread_create");
	}
	for (i = 0; i < (int)(arc4random() % 3 + 1); i++) {
		if ((e = pthread_create(&tp[idx++], NULL, fs, NULL)) != 0)
			errc(1, e, "pthread_create");
	}
	for (i = 0; i < (int)(arc4random() % 3 + 1); i++) {
		if ((e = pthread_create(&tp[idx++], NULL, tr, NULL)) != 0)
			errc(1, e, "pthread_create");
	}
	for (i = 0; i < (int)(arc4random() % 3 + 1); i++) {
		if ((e = pthread_create(&tp[idx++], NULL, sy, NULL)) != 0)
			errc(1, e, "pthread_create");
	}
	assert(idx <= THREADS);

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
#$here/../testcases/swap/swap -t 5m -i 20 > /dev/null &
sleep 2

size=262144
$serial file $size
cp file file.orig

s=0
#ktrace -id -f $here/ktrace.out /tmp/$prog file || s=1
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
