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

# Test scenario by kib@freebsd.org

# Test of patch for Giant trick in cdevsw

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
[ -d /usr/src/sys ] || exit 0
builddir=`sysctl kern.version | grep @ | sed 's/.*://'`
[ -d "$builddir" ] && export KERNBUILDDIR=$builddir || exit 0
export SYSDIR=`echo $builddir | sed 's#/sys.*#/sys#'`

. ../default.cfg

odir=`pwd`
dir=$RUNDIR/fpclone
[ ! -d $dir ] && mkdir -p $dir

cd $dir
cat > Makefile <<EOF
KMOD= fpclone
SRCS= fpclone.c

.include <bsd.kmod.mk>
EOF

sed '1,/^EOF2/d' < $odir/fpclone.sh > fpclone.c
make
kldload $dir/fpclone.ko

sed '1,/^EOF2/d' < $odir/$0 > fpclone2.c
mycc -o /tmp/fpclone2 -Wall fpclone2.c
rm -f fpclone2.c

cd $odir
for i in `jot 10`; do
	/tmp/fpclone2 &
done

for i in `jot 10`; do
	wait
done
kldstat
kldunload $dir/fpclone.ko
rm -rf $dir /tmp/fpclone2
exit

EOF2
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <err.h>

int
main(int argc, char **argv)
{
	int fd;
	int i;
	char buf[80];

	for (i = 0; i < 10000; i++) {
		if ((fd = open("/dev/fpclone", O_RDONLY)) == -1)
			err(1, "open(/dev/fpclone");
		if (read(fd, buf, sizeof(buf)) <= 0)
			err(1, "read");
		if (dup2(fd, 10) == -1)
			err(1, "dup");
		if (dup2(fd, 11) == -1)
			err(1, "dup");
		if (dup2(fd, 12) == -1)
			err(1, "dup");
		if (close(fd) == -1)
			err(1, "close(%d)", fd);
		if (close(10) == -1)
			err(1, "close(%d)", 10);
		if (close(11) == -1)
			err(1, "close(%d)", 11);
		if (close(12) == -1)
			err(1, "close(%d)", 12);
	}
	if ((fd = open("/dev/fpclone", O_WRONLY)) == -1)
			err(1, "open(/dev/fpclone");
	if (write(fd, "xxx", 3) == -1 && errno != ENODEV)
			err(1, "write");
	if (close(fd) == -1)
		err(1, "close(%d)", fd);

	return (0);
}
