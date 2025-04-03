#!/bin/sh

# Test scenario from Bug 64816: [nfs] [patch] mmap and/or ftruncate does not work correctly on nfs mounted file systems

. ../default.cfg

set -u
grep -q $mntpoint /etc/exports ||
    { echo "$mntpoint missing from /etc/exports"; exit 0; }
rpcinfo 2>/dev/null | grep -q mountd || exit 0

prog=$(basename "$0" .sh)
cat > /tmp/$prog.c <<EOF
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void error(char *msg)
{
	fprintf(stderr, "Error: %s\nSystem error %d: %s\n", msg, errno, strerror(errno));
	exit(-1);
}

#define SZ 1024 // Less than page size

int main(int argn, char *argv[])
{
	int fd, s;
	char buffer[SZ];
	char *map;

	if (argn!=2)
	{
		fprintf(stderr, "Usage:\n %s [filename]\n", argv[0]);
		_exit(-1);
	}

	memset(buffer, 0, SZ);
	s = 0;

	fd=open(argv[1], O_RDWR|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR);
	if (fd==-1)
		error("Could not create file");

	if (write(fd, buffer, SZ)!=SZ)
		error("Could not write buffer");

	map=mmap(NULL, SZ, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (map==MAP_FAILED)
		error("Map failed");
	map[SZ-1]=1;

	if (ftruncate(fd, SZ+1)!=0)
		error("Could not truncate file");

	if (map[SZ-1]==1)
		printf("Test passed\n");
	else {
		printf("Test failed\n");
		s = 1;
	}

	exit(s);
}
EOF
mycc -o /tmp/$prog -Wall -Wextra -O0 -g /tmp/$prog.c || exit 1

mount | grep -q "on $mntpoint " && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart && mdconfig -d -u $mdstart
mdconfig -s 1g -u $mdstart
newfs -n $newfs_flags /dev/md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint

mp2=${mntpoint}2
mkdir -p $mp2
mount | grep -q "on $mp2 " && umount -f $mp2
mount -t nfs -o retrycnt=3 127.0.0.1:$mntpoint $mp2 || exit 1
sleep .2
mount | grep  $mntpoint

cd $mp2
/tmp/$prog $prog.data; s=$?
ls -ls $mp2/$prog.data
cd -

umount $mp2
umount $mntpoint
mdconfig -d -u $mdstart
rm -f /tmp/$prog /tmp/$prog.c
exit $s
