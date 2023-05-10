#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2020 Mark Johnston <markj@FreeBSD.org>
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

# "Fatal trap 9: general protection fault while in kernel mode" seen:
# https://people.freebsd.org/~pho/stress/log/sendfile15-4.txt

# Mark wrote:
# It took me some time but I managed to get a repro.  Here is what I did:
# - Create a malloc-backed md(4) device with a UFS filesystem.  Configure
#   a gnop provider with -q 100 -d 10, i.e., always delay reads by 10ms.
# - Write a small file (e.g., 4KB) to the filesystem.
# - In a loop, unmount the filesystem, mount it, and run the test program.
# The test program:
# - creates a PF_LOCAL socket pair <s1, s2>,
# - opens the input file,
# - sends the file over s1,
# - closes s1,
# - sleeps for a second before exiting (and closing s2)
#
# Using a separate filesystem ensures that the input file is not cached
# when the test program runs, so sendfile will perform an async getpages
# and place the "future" mbufs in s2's receive buffer.  The gnop delay
# ensures that the I/O request will not be completed before s1 is closed,
# and because s1 is closed uipc_ready() will free the promised mbufs and
# return ECONNRESET.  Because of the sleep, s2's receive buffer will not
# be scanned until after the uipc_ready() call.

# Fixed by r359778

[ `id -u` -ne 0 ] && echo "Must be root!" && exit 1
kldstat | grep -q geom_nop || { gnop load 2>/dev/null || exit 0 &&
    notloaded=1; }
gnop status || exit 1

. ../default.cfg

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/sendfile18.c
mycc -o sendfile18 -Wall -Wextra -O0 -g sendfile18.c || exit 1
rm -f sendfile18.c
cd $odir

set -e
mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 2g -u $mdstart
gnop create /dev/md$mdstart
newfs $newfs_flags /dev/md$mdstart.nop > /dev/null
mount /dev/md$mdstart.nop $mntpoint
chmod 777 $mntpoint
gnop configure -q 100 -d 10 /dev/md$mdstart.nop
set +e

dd if=/dev/zero of=$mntpoint/file bs=4k count=1 status=none

start=`date +%s`
while [ $((`date +%s` - start)) -lt 5 ]; do
	umount $mntpoint
	mount /dev/md$mdstart.nop $mntpoint
	/tmp/sendfile18 $mntpoint/file
done
umount $mntpoint

gnop destroy /dev/md$mdstart.nop
mdconfig -d -u $mdstart
rm $dir/sendfile18
[ $notloaded ] && gnop unload

exit 0
EOF
#include <sys/socket.h>

#include <err.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

int
main(int argc __unused, char **argv)
{
        int fd, sd[2];

        if (socketpair(PF_LOCAL, SOCK_STREAM, 0, sd) != 0)
                err(1, "socketpair");

        fd = open(argv[1], O_RDONLY | O_DIRECT);
        if (fd < 0)
                err(1, "open");

        if (sendfile(fd, sd[0], 0, 4096, NULL, NULL, 0) != 0)
                err(1, "sendfile");
        close(sd[0]);
        sleep(1);

        return (0);
}
