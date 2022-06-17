#!/bin/sh

#
# Copyright (c) 2017 Dell EMC Isilon
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

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

# No problems seen.

. ../default.cfg

[ -d /usr/include ] || exit 0
here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > fts3.c
mycc -o fts3 -Wall -Wextra fts3.c || exit 1
rm -f fts3.c
cd $here

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 3g -u $mdstart || exit 1
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint

start=`date '+%s'`
while [ $((`date '+%s'` - start)) -lt 120 ]; do
	pids=
	for i in `jot 20`; do
		cp -r /usr/include $mntpoint/$i &
		pids="$pids $!"
	done
	for p in $pids; do
		wait $p
	done
	(cd $mntpoint && rm -rf *)
done &

pid=$!
s=0
while kill -0 $pid 2> /dev/null; do
	/tmp/fts3 $mntpoint || { s=1; break; }
done
kill $pid > /dev/null 2>&1
wait $pid

for i in `jot 6`; do
	umount $mntpoint && break || sleep 10
done
[ $i -eq 6 ] && s=2
mdconfig -d -u $mdstart
rm -f /tmp/fts3
exit $s
EOF
#include <sys/param.h>

#include <err.h>
#include <errno.h>
#include <fts.h>
#include <libutil.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int
test(char *path)
{

	FTS *fts;
	FTSENT *p;
	int ftsoptions;
	char *args[2];

	ftsoptions = FTS_PHYSICAL;
	args[0] = path;
	args[1] = 0;

	if ((fts = fts_open(args, ftsoptions, NULL)) == NULL)
		err(1, "fts_open");

	while ((p = fts_read(fts)) != NULL)
		;

	if (errno != 0 && errno != ENOENT)
		err(1, "fts_read");
	if (fts_close(fts) == -1)
		err(1, "fts_close()");

	return (0);
}

int
main(int argc, char **argv)
{
	int i;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <path>\n", argv[0]);
		exit(1);
	}
	alarm(120);
	for (i = 0; i < 100; i++)
		test(argv[1]);

	return (0);
}
