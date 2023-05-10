#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2021 Peter Holm <pho@FreeBSD.org>
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

# O_CREAT / unlink() timing test with different FFS options.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
[ `uname -m` = "i386" ] && exit 0 # very long runtime

. ../default.cfg
[ $# -eq 0 ] && half=1	# SU and SUJ workaround
first=1
export LANG=en_US.ISO8859-1
odir=`pwd`
dir=$mntpoint

cd /tmp
sed '1,/^EOF/d' < $odir/$0 > perf.c
mycc -o perf -Wall -Wextra perf.c || exit 1
rm -f perf.c
cd $odir

mount | grep -q "on $mntpoint " && umount $mntpoint
mdconfig -l | grep md$mdstart > /dev/null &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 2g -u $mdstart

tst() {
	local i j k s

	s=0
	cd $dir
	inodes=`df -ik $mntpoint | tail -1 | \
	    awk '{printf "%d\n", $7}'`
#	SU and SUJ tests fail with ENOSPC
	[ $half ] &&
	    i=$((inodes / 4)) ||
	    i=$(((inodes - 500) / 2))
	[ $first -eq 1 ] &&
		printf "Using %'\''d inodes out of a total of %'\''d.\n" \
			$((i * 2)) $inodes
	first=0

	for k in `jot 3`; do
		pids=""
		for j in `jot 2`; do
			/tmp/perf $i &
			pids="$pids $!"
		done
		for pid in $pids; do
			wait $pid; r=$?
			[ $r -ne 0 ] && s=$r
		done
	done
	cd $odir
	return $s
}

s=0
for i in "" "-U" "-j"; do
	newfs $i /dev/md$mdstart > /dev/null 2>&1
	mount /dev/md$mdstart $mntpoint

	t1=`date +%s`
	tst; r=$?
	t2=$((`date +%s` - t1))

	umount -f $mntpoint
	t2=$((`date +%s` - t1))
	[ $t2 -eq 0 ] && t2=1
	[ -z "$base" ] && base=$t2
	pct=$(((t2 - base) * 100 / base))
	printf '%3d seconds elapsed for newfs option "%2s" (%+4d%%)\n' \
	    $t2 "$i" $pct
	[ $pct -gt 10 ] && s=111
	[ $s -eq 0 -a $r -ne 0 ] && s=$r
done
rm -f /tmp/perf
mdconfig -d -u $mdstart
exit $s
EOF
#include <sys/types.h>
#include <sys/stat.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
main(int argc __unused, char **argv)
{
	pid_t pid;
	int64_t size;
	int e, fd, i, j;
	char file[128];

	size = atol(argv[1]);

	e = 0;
	pid = getpid();
	for (j = 0; j < size; j++) {
		sprintf(file,"p%05d.%05d", pid, j);
		if ((fd = open(file, O_RDWR | O_CREAT | O_TRUNC,
		    DEFFILEMODE)) == -1) {
			e = errno;
			if (errno != EINTR) {
				warn("open(%s)", file);
				printf("break out at %d, errno %d\n", j,
				    errno);
				break;
			}
		}
		close(fd);
	}

	for (i = --j; i >= 0; i--) {
		sprintf(file,"p%05d.%05d", pid, i);
		if (unlink(file) == -1)
			err(3, "unlink(%s)", file);

	}

	return (e);
}
