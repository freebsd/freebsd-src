#!/bin/sh

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

# The issue, found by Maxim, is that sometimes partial truncate could
# create a UFS inode where the last byte is not populated.
# Fixed by r295950.

# Test scenario by Maxim Sobolev <sobomax@sippysoft.com>

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

mount | grep -q "$mntpoint" && umount $mntpoint
mdconfig -l | grep -q $mdstart &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 1g -u $mdstart

if [ $# -eq 0 ]; then
	newfs -n $newfs_flags md$mdstart > /dev/null
else
	newfs -n md$mdstart > /dev/null
fi
mount /dev/md$mdstart $mntpoint

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > truncate6.c
mycc -o truncate6 -Wall -Wextra -O2 truncate6.c -lutil
rm -f truncate6.c

cd $mntpoint

/tmp/truncate6
inode=$(ls -ail | awk '/file/ {print $1}')

cd $here
rm -f /tmp/truncate6

while mount | grep -q md$rt; do
	umount $mntpoint || sleep 1
done

full=$(
fsdb -r /dev/md$mdstart <<QUOTE
inode $inode
blocks
quit
QUOTE
)
full=`echo "$full" | sed '/Last Mounted/,+6d'`
r=`echo "$full" | tail -1`
expect="	lbn 3 blkno 4712-4719"
if [ "$r" != "$expect" ]; then
	e=1
	echo "FAIL Expected \"$expect\", got \"$r\"."
	echo "$full" | tail -3
else
	e=0
fi

mdconfig -d -u $mdstart
exit $e
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
main(void)
{
	off_t hole, data, pos;
	int fd;
	char tempname[] = "file";

	pos = 1024 * 128 + 1; // 131073
	if ((fd = open(tempname, O_WRONLY | O_CREAT | O_TRUNC, DEFFILEMODE)) ==
	    -1)
		err(1, "open(%s)", tempname);
	if (ftruncate(fd, pos) < 0)
		err(1, "ftruncate()");
	hole = lseek(fd, 0, SEEK_HOLE); // 0
	data = lseek(fd, 0, SEEK_DATA); // 131072
	if (ftruncate(fd, data) < 0)
		err(1, "ftruncate() 2");
	close(fd);
	if (hole != 0 && data != 131072) {
		printf("--> hole = %jd, data = %jd, pos = %jd\n",
		    (intmax_t)hole, (intmax_t)data, (intmax_t)pos);
		exit (1);
	}

	return (0);
}
