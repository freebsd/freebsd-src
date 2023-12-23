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

# pts leak seen.
# Fixed in r313496.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
kldstat -v | grep -q pty || { kldload pty || exit 0; }

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > pts2.c
mycc -o pts2 -Wall -Wextra -O2 pts2.c || exit 1
rm -f pts2.c

pts=`vmstat -m | awk '/ pts / {print $2}'`
for i in `jot 10`; do
	/tmp/pts2
done
new=`vmstat -m | awk '/ pts / {print $2}'`
s=0
[ $((new - pts)) -gt 1 ] && { s=1; echo "Leaked $((new - pts)) pts."; }

rm -f /tmp/pts2
exit $s
EOF
#include <sys/ioctl.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>

const char *master = "/dev/ptmx";
int
main(void)
{
        int fd, fd2, slave;
        char sl[80];

        if ((fd = open(master, O_RDONLY)) == -1)
                err(1, "open(%s)", master);

        if (ioctl(fd, TIOCGPTN, &slave) == -1)
                err(1, "ioctl");

        snprintf(sl, sizeof(sl), "/dev/pts/%d", slave);

        if ((fd2 = open(sl, O_RDONLY | O_EXLOCK)) == -1)
                err(1, "open(%s)", sl);

        return (0);
}
