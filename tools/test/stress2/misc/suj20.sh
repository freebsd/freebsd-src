#!/bin/sh

#
# Copyright (c) 2011 Peter Holm <pho@FreeBSD.org>
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

# Looping mksnap_ffs seen.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

# Scenario by mckusick@
#
# create a bunch of files/directories
# create a snapshot
# remove many (but not all) of those files/directories
# create some new files/directories in what remains of those
#     original files/directories.
# create another snapshot
# repeat {
#         remove many (somewhat different) of those files/directories
#         create some new files/directories in what remains of those
#             remaining files/directories.
#         create a new snapshot
#         remove oldest snapshot
# }

snap () {
	for i in `jot 5`; do
		mksnap_ffs $1 $2 2>&1 | grep -v "Resource temporarily unavailable"
		[ ! -s $2 ] && rm -f $2	|| return 0
		sleep 1
	done
	return 1
}

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > suj20.c
mycc -o suj20 -Wall -Wextra -g -O2 suj20.c
rm -f suj20.c

mount | grep "$mntpoint" | grep -q md$mdstart && umount $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 1g -u $mdstart
newfs -j md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint

cd $mntpoint
chmod 777 $mntpoint
/tmp/suj20
snap $mntpoint $mntpoint/.snap/snap1
/tmp/suj20 prune
snap $mntpoint $mntpoint/.snap/snap2
/tmp/suj20
for i in `jot 10`; do
	/tmp/suj20 prune
	/tmp/suj20
	snap $mntpoint $mntpoint/.snap/snap$((i + 2))
	sn=`ls -tU $mntpoint/.snap | tail -1`
	rm -f $mntpoint/.snap/$sn
done
cd $here

while mount | grep -q $mntpoint; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
rm -f /tmp/suj20
exit 0
EOF
#include <sys/types.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

static char buf[4096];
#define ND 100
#define NF 100

void
setup(void)
{
	int d, f, fd, i, n;
	char name[128];

	for (d = 0; d < ND; d++) {
		snprintf(name, sizeof(name), "d%03d", d);
		if (mkdir(name, 00700) == -1 && errno != EEXIST)
			err(1, "mkdir(%s)", name);
		if (chdir(name) == -1)
			err(1, "chdir(%s)", name);
		for (f = 0; f < NF; f++) {
			if (arc4random() % 100 < 33)
				continue;
			snprintf(name, sizeof(name), "f%03d", f);
			if ((fd = open(name, O_RDWR | O_CREAT | O_TRUNC, 0640)) == -1)
				err(1, "open(%s)", name);
			n = arc4random() % 10;
			for (i = 0; i < n; i++) {
				if (write(fd, buf, sizeof(buf)) != sizeof(buf))
					err(1, "write()");
			}
			close(fd);
		}
		if (chdir("..") == -1)
			err(1, "chdir(%s)", "..");
	}
}
void

prune(void)
{
	int d, f;
	char name[128];

	for (d = 0; d < ND; d++) {
		snprintf(name, sizeof(name), "d%03d", d);
		if (chdir(name) == -1)
			err(1, "chdir(%s)", name);
		for (f = 0; f < NF; f++) {
			if (arc4random() % 100 < 33)
				continue;
			snprintf(name, sizeof(name), "f%03d", f);
			if (unlink(name) == -1 && errno != ENOENT)
				err(1, "unlink(%s)", name);
		}
		if (chdir("..") == -1)
			err(1, "chdir(%s)", "..");
	}
	for (d = 0; d < ND; d++) {
		if (arc4random() % 100 > 10)
			continue;
		snprintf(name, sizeof(name), "rm -rf d%03d", d);
		system(name);
	}
}

int
main(int argc, char **argv __unused)
{
	if (argc == 1)
		setup();
	if (argc == 2)
		prune();

	return (0);
}
