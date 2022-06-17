#!/bin/sh

#
# Copyright (c) 2013 EMC Corp.
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

# "panic: softdep_deallocate_dependencies: dangling deps" seen:
# http://people.freebsd.org/~pho/stress/log/sigxcpu7.txt

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > sigxcpu.c
mycc -o sigxcpu -Wall -Wextra sigxcpu.c
rm -f sigxcpu.c
cd $here

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 4g -u $mdstart || exit 1

newfs $newfs_flags md$mdstart > /dev/null

mount /dev/md$mdstart $mntpoint
chmod 777 $mntpoint

export runRUNTIME=4m
export RUNDIR=$mntpoint/stressX
set `df -ik $mntpoint | tail -1 | awk '{print $4,$7}'`
export KBLOCKS=$(($1 / 4))
export INODES=$(($2 / 4))

su $testuser -c 'ulimit -t 3; cd ..; ./run.sh marcus.cfg' > /dev/null 2>&1 &
sleep 5
for i in `jot 10`; do
	/tmp/sigxcpu $mntpoint
done
kill $! > /dev/null 2>&1
../tools/killall.sh || ../tools/killall.sh
wait

for i in `jot 6`; do
	mount | grep -q "on $mntpoint " || break
	umount $mntpoint && break || sleep 10
	[ $i -eq 6 ] &&
	    { echo FATAL; fstat -mf $mntpoint; exit 1; }
done
mdconfig -d -u $mdstart
rm -f /tmp/sigxcpu
exit 0
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

static void
hand(int i __unused) {	/* handler */
	_exit(1);
}

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
	signal(SIGALRM, hand);
	alarm(20);
	for (i = 0; i < 100; i++)
		test(argv[1]);

	return (0);
}
