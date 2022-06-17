#!/bin/sh

#
# Copyright (c) 2013 Peter Holm <pho@FreeBSD.org>
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

# Run with marcus.cfg on a 1g swap backed MD
# "panic: vm_radix_remove: invalid key found" seen.

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > fts1.c
mycc -o fts1 -Wall -Wextra fts1.c || exit 1
rm -f fts1.c
cd $here

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 1g -u $mdstart || exit 1

newfs $newfs_flags md$mdstart > /dev/null

mount /dev/md$mdstart $mntpoint
chmod 777 $mntpoint

export runRUNTIME=20m
export RUNDIR=$mntpoint/stressX

su $testuser -c 'cd ..; ./run.sh marcus.cfg' > /dev/null &
pid=$!
while kill -0 $pid 2> /dev/null; do
	/tmp/fts1 $mntpoint
	sleep 1
done
wait

s=0
for i in `jot 6`; do
	umount $mntpoint && break || sleep 10
done
[ $i -eq 6 ] && s=1
mdconfig -d -u $mdstart
rm -f /tmp/fts1
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

	FTS		*fts;
	FTSENT		*p;
	int		ftsoptions;
	char		*args[2];

	ftsoptions = FTS_PHYSICAL;
	args[0] = path;
	args[1] = 0;

	if ((fts = fts_open(args, ftsoptions, NULL)) == NULL)
		err(1, "fts_open");

	while ((p = fts_read(fts)) != NULL) {
		switch (p->fts_info) {
			case FTS_F:			/* Ignore. */
				break;
			case FTS_D:			/* Ignore. */
				break;
			case FTS_DP:
				break;
			case FTS_DC:			/* Ignore. */
				break;
			case FTS_SL:			/* Ignore. */
				break;
			case FTS_DNR:			/* Warn, continue. */
			case FTS_ERR:
			case FTS_NS:
			case FTS_DEFAULT:
				break;
			default:
				printf("%s: default, %d\n", getprogname(), p->fts_info);
				break;
		}
	}

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

	if (argc != 2)
		errx(1, "Usage: %s <path>", argv[0]);
	alarm(600);
	for (i = 0; i < 100; i++)
		test(argv[1]);

	return (0);
}
