#!/bin/sh

#
# Copyright (c) 2016 Dell EMC Isilon
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

# Demonstrate deadlock
# https://people.freebsd.org/~pho/stress/log/umountf10.txt

# Test scenario suggestion by: kib
# Fixed in r308618.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
. ../default.cfg

cont=/tmp/umountf10.continue
dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/umountf10.c
mycc -o umountf10 -Wall -Wextra -O2 -g umountf10.c || exit 1
rm -f umountf10.c
cd $odir

mount | grep -q "on $mntpoint " && umount -f $mntpoint
[ -c /dev/md$mdstart ] && mdconfig -d -u $mdstart

mdconfig -a -t swap -s 512m -u $mdstart
gpart create -s GPT md$mdstart > /dev/null || exit 1
gpart add -t freebsd-ufs md$mdstart > /dev/null || exit 1
newfs -n $newfs_flags md${mdstart}p1 > /dev/null || exit 1
mount /dev/md${mdstart}p1 $mntpoint
touch $mntpoint/marker $cont
trap "rm -f $cont" EXIT INT

daemon sh -c "(cd $odir/../testcases/swap; ./swap -t 4m -i 20)" > \
    /dev/null 2>&1
sleep 5

for i in `jot 4`; do
	/tmp/umountf10 $mntpoint &
	pid="$pid $!"
done

for i in `jot 10`; do
	while [ -e $cont ]; do procstat -f $pid > /dev/null 2>&1; done &
	pid2="$pid2 $!"
done

while [ -e $cont ]; do find $mntpoint -ls > /dev/null 2>&1; done &
tpid=$!

start=`date '+%s'`
while [ $((`date '+%s'` - start)) -lt 300 ]; do
	umount -f $mntpoint 2>/dev/null &&
	    mount /dev/md${mdstart}p1 $mntpoint
done
while pgrep -q swap; do
	pkill -9 swap
done
kill $pid $pid2 $tpid
wait

umount $mntpoint
rm -f $mntpoint/file.* /tmp/umountf10
mdconfig -d -u $mdstart

exit 0
EOF
#include <sys/param.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
	int fd;
	char file[MAXPATHLEN + 1];
	char marker[MAXPATHLEN + 1];

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <file path>", argv[0]);
		exit(1);
	}

	alarm(600);
	snprintf(file, sizeof(file), "%s/file.%06d", argv[1], getpid());
	snprintf(marker, sizeof(marker), "%s/marker", argv[1]);
	for (;;) {
		if (access(marker, R_OK) == -1)
			continue;
		if ((fd = open(file, O_RDWR | O_CREAT | O_APPEND,
		    DEFFILEMODE)) == -1) {
			if (errno != ENOENT && errno != EBUSY)
				warn("open(%s)", file);
			continue;
		}
		write(fd, "a", 1);
		usleep(arc4random() % 400);
		close(fd);
		unlink(argv[1]);
	}

	return(0);
}
