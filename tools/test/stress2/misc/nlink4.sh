#!/bin/sh
#
# SPDX-License-Identifier: BSD-2-Clause
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
# rename(2) version

. ../default.cfg

cat > /tmp/nlink4.c <<EOF
#include <sys/stat.h>
#include <ufs/ufs/dinode.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int
main (void) {
	int fd, i, mx;
	char file[100];

	snprintf(file, sizeof(file), "f");
	if ((fd = open(file, O_RDWR | O_CREAT | O_TRUNC,
	    DEFFILEMODE)) == -1)
		err(1, "creat(%s)", file);
	close(fd);

	mx = UFS_LINK_MAX - 1;
	for (i = 0; i < mx; i++) {
		snprintf(file, sizeof(file), "%d", i);
		if (link("f", file) == -1)
			err(1, "link(%s, %s)", "f", file);

	}

	/* The following link(2) must fail */
	i = mx;
	snprintf(file, sizeof(file), "%d", i);
	if (link("f", file) != -1)
		err(1, "link(%s, %s)", "f", file);
	if (errno != EMLINK)
		err(1, "Must fail: link(%s, %s)", "f", file);

	/* Must succeed */
	i = 0;
	snprintf(file, sizeof(file), "%d", i);
	if (unlink(file) == -1)
		err(1, "unlink(%s)", file);
	if (rename("1", "a") == -1) {
		if (errno == EMLINK)
			fprintf(stderr, "Unexpected EMLINK\n");
		err(1, "rename(%s, %s)", "1", "a");
	}

	return (0);
}
EOF
mycc -o /tmp/nlink4 -Wall -Wextra -O2 /tmp/nlink4.c || exit 1
rm /tmp/nlink4.c

set -e
here=`pwd`
mount | grep -q "on $mntpoint " && umount -f $mntpoint
mdconfig -l | grep "md$mdstart " && mdconfig -d -u $mdstart
mdconfig -a -t swap -s 1g -u $mdstart
newfs -Un /dev/md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
set +e

cd $mntpoint
/tmp/nlink4; s=$?
n=`ls -a | wc -l`
[ $s -ne 0 ] && echo "$n files"
cd $here

umount $mntpoint
mdconfig -d -u $mdstart
rm /tmp/nlink4
exit $s
