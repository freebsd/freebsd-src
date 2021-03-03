#!/bin/sh

#
# Copyright (c) 2008 Peter Holm <pho@FreeBSD.org>
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

# Alternate buffer flush path test (Not verified).
# Regression test for r169006.
# Apply this patch to amplify the problem:
#
# diff -r1.520 vfs_bio.c
# 894c894
# <       if (bo->bo_dirty.bv_cnt > dirtybufthresh + 10) {
# ---
# >       if (bo->bo_dirty.bv_cnt > dirtybufthresh /*+ 10*/) {

. ../default.cfg

odir=`pwd`
dir=$RUNDIR/alternativeFlushPath

[ -d $dir ] && find $dir -type f | xargs rm
rm -rf $dir
mkdir -p $dir
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/alternativeFlushPath.c
mycc -o /tmp/alternativeFlushPath -Wall -Wextra alternativeFlushPath.c ||
    exit 1
rm -f alternativeFlushPath.c

for j in `jot 10`; do
   /tmp/alternativeFlushPath &
done
wait
sysctl vfs.altbufferflushes

cd $odir
rm -rf /tmp/alternativeFlushPath $dir

exit

EOF
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <err.h>

#define MAXNOFILE 500000	/* To limit runtime */

static volatile sig_atomic_t more;

static void
handler(int i __unused) {
	more = 0;
}

void
test(void)
{
	int i, j;
        char name[80];
        pid_t mypid;
        int *fd;
        struct rlimit rlp;

        if (getrlimit(RLIMIT_NOFILE, &rlp) == -1)
                err(1, "getrlimit(RLIMIT_NOFILE)");
	if (rlp.rlim_cur > MAXNOFILE)
		rlp.rlim_cur = MAXNOFILE;
	rlp.rlim_cur /= 10;
        mypid = getpid();
        fd = malloc(rlp.rlim_cur * sizeof(int));

	for (i = 0, j = 0; i < rlp.rlim_cur && more == 1; i++, j++) {
		sprintf(name, "f%05d.%05d", mypid, i);
		if ((fd[i] = open(name, O_CREAT|O_WRONLY, 0666)) == -1) {
			warn("open(%s)", name);
			more = 0;
			break;
		}
	}
	for (i = 0; i < j; i++) {
		sprintf(name, "f%05d.%05d", mypid, i);
		if (unlink(name) == -1)
			warn("unlink(%s)", name);
	}
	for (i = 0; i < j; i++) {
		if (close(fd[i]) == -1)
			warn("close(%d)", i);
	}
	free(fd);
}

int
main()
{
	more = 1;
	signal(SIGALRM, handler);
	alarm(20 * 60);
	while (more == 1)
		test();

        return(0);
}
