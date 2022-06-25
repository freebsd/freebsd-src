#!/bin/sh
#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
# Copyright (c) 2022 Peter Holm <pho@FreeBSD.org>
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

# https://reviews.freebsd.org/D35514
# Rename dir a/b to c/d

# Test scenario suggestion by kib

. ../default.cfg

cat > /tmp/nlink5.c <<EOF
#include <sys/stat.h>
#include <ufs/ufs/dinode.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

int
main (void) {
	int i, mx;
	char dir[100];

	mx = UFS_LINK_MAX - 2; /* UFS_LINK_MAX = 32767 */
	for (i = 0; i < mx; i++) {
		snprintf(dir, sizeof(dir), "%d", i);
		if (mkdir(dir, 0700) == -1)
			err(1, "mkdir(%s)", dir);
	}

	/* The following mkdir(2) must fail */
	i = mx;
	snprintf(dir, sizeof(dir), "%d", i);
	if (mkdir(dir, 0700) != -1)	/* this must fail */
		err(1, "mkdir(%s)", dir);
	if (errno != EMLINK)
		err(1, "Must fail: mkdir(%s)", dir);

	/* Must succeed */
	i = 0;
	snprintf(dir, sizeof(dir), "%d", i);
	if (rmdir(dir) == -1)
		err(1, "rmdir(%s)", dir);
	snprintf(dir, sizeof(dir), "%s", "x");
	if (mkdir(dir, 0700) == -1)
		err(1, "mkdir(%s)", dir);

	/* Make room for two top level directories */
	if (rmdir("1") == -1)
		err(1, "rmdir(1)");
	if (rmdir("2") == -1)
		err(1, "rmdir(2)");

	/* mkdir a c */
	snprintf(dir, sizeof(dir), "%s", "a");
	if (mkdir(dir, 0700) == -1)
		err(1, "mkdir(%s)", dir);
	snprintf(dir, sizeof(dir), "%s", "c");
	if (mkdir(dir, 0700) == -1)
		err(1, "mkdir(%s)", dir);

	/* mkdir a/b */
	snprintf(dir, sizeof(dir), "%s/%s", "a", "b");
	if (mkdir(dir, 0700) == -1)
		err(1, "mkdir(%s)", dir);

	/* mv a/b c/d */
	if (rename("a/b", "c/d") == -1)
		err(1, "rename(a/b, c/d)");

	return (0);
}
EOF
mycc -o /tmp/nlink5 -Wall -Wextra -O2 /tmp/nlink5.c || exit 1
rm /tmp/nlink5.c

set -e
here=`pwd`
mount | grep -q "on $mntpoint " && umount -f $mntpoint
mdconfig -l | grep "md$mdstart " && mdconfig -d -u $mdstart
mdconfig -a -t swap -s 1g -u $mdstart
newfs -Un /dev/md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
set +e

cd $mntpoint
/tmp/nlink5; s=$?
n=`find . -type d -maxdepth 1 | wc -l`
[ $n -ne 32766 ] && s=2
cd $here

umount $mntpoint
mdconfig -d -u $mdstart
rm /tmp/nlink5
exit $s
