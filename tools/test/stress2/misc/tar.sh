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

# tar x simulator
# Create 100.000 files of size 64k; a total of 6GB

. ../default.cfg

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/tar.c
mycc -o tar -Wall -Wextra -O0 -g tar.c || exit 1
rm -f tar.c
cd $odir

export LANG=C
wdir=`dirname $diskimage`
rm -rf $wdir/tar.tmp
[ `df -k $wdir | tail -1 | awk '{print $4}'` -lt \
    $((6 * 1024 * 1024)) ] &&
    echo "Not enough disk space on $wdir." && exit 0

for i in `jot $(sysctl -n hw.ncpu)`; do
	/tmp/tar $wdir | ministat -n | tail -2 &
	sleep 3
done
wait

rm -rf /tmp/tar $wdir/tar.tmp
exit 0

EOF
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#define RUNTIME 1200

char buf[64*1024];

int
main(int argc, char *argv[])
{
	struct timeval start, stop, diff;
	time_t stime;
	uint64_t mx, usec;
	int fd, i, j, k;
	char path[128];

	if (argc != 2)
		errx(1, "Usage: %s <path>", argv[0]);
	mx = 0;
	stime = time(NULL);
	snprintf(path, sizeof(path), "%s/tar.tmp", argv[1]);
	mkdir(path, 0777);
	for (i = 0; i < 100; i++) {
		snprintf(path, sizeof(path), "%s/tar.tmp/d%d", argv[1], i);
		if (mkdir(path, 0777) == -1)
			if (errno != EEXIST)
				err(1, "mkdir(%s)", path);
		for (j = 0; j < 100; j++) {
			snprintf(path, sizeof(path), "%s/tar.tmp/d%d/d%d",
			    argv[1], i, j);
			if (mkdir(path, 0777) == -1)
				if (errno != EEXIST)
					err(1, "mkdir(%s)", path);
		}
	}
	for (i = 0; i < 100; i++) {
		for (j = 0; j < 100; j++) {
			for (k = 0; k < 10; k++) {
				snprintf(path, sizeof(path),
				    "%s/tar.tmp/d%d/d%d/f%d", argv[1], i, j, k);
				gettimeofday(&start, NULL);
				unlink(path);
				if ((fd = open(path, O_RDWR | O_CREAT, 0644))
				    == -1)
					err(1, "open(%s)", path);
				write(fd, buf, sizeof(buf));
				close(fd);
				gettimeofday(&stop, NULL);
				timersub(&stop, &start, &diff);
				usec  = ((uint64_t)1000000 *
				    diff.tv_sec + diff.tv_usec);
				if (usec > mx)
					mx = usec;
				fprintf(stdout, "%.3f %s\n",
				    (double)usec / 1000000, path);
			}
		}
		if (time(NULL) - stime > RUNTIME) {
			fprintf(stderr, "Timed out at %s.\n", path);
			break;
		}
	}
	if (mx > 5000000) {
//		fprintf(stderr, "%.3f %s : FAIL\n", (double)mx / 1000000, path);
		return (1);
	} else
		return (0);
}
