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

# Stress test UFS2 file systems by introducing single bit errors in the FS
# fsck should fix the FS no matter how damaged, but e.g. this panic has been seen:
#
# panic(c0912b65,dfe96000,0,c09e4060,ef48c778,...) at panic+0x14b
# vm_fault(c1868000,dfe96000,1,0) at vm_fault+0x1e0
# trap_pfault(ef48c894,0,dfe96000) at trap_pfault+0x137
# trap(dfe90008,ef480028,c0690028,d0560000,dfe96000,...) at trap+0x341
# calltrap() at calltrap+0x5
# --- trap 0xc, eip = 0xc08785a6, esp = 0xef48c8d4, ebp = 0xef48c958 ---
# generic_bcopy(c81cd570,d0508000,c5ead600,c87b81c0,0,...) at generic_bcopy+0x1a
# ffs_mount(d0508000,c5ead600,0,c09b0860,c5ecfc3c,...) at ffs_mount+0xa14
# vfs_domount(c5ead600,cd8c7280,ccb75080,0,...) at vfs_domount+0x687
# vfs_donmount(c5ead600,0,ef48cc04) at vfs_donmount+0x2ef
# kernel_mount(c5660960,0,bfbfec86,0,fffffffe,...) at kernel_mount+0x6d
# ffs_cmount(c5660960,bfbfde50,0,c5ead600,c09b0860,...) at ffs_cmount+0x5d
# mount(c5ead600,ef48cd04) at mount+0x156
# syscall(3b,3b,3b,804abcf,bfbfe8e4,...) at syscall+0x22f

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

D=$diskimage

tst() {
   rm -f $D
   truncate -s 2M $D
   mdconfig -a -t vnode -f $D -u $mdstart
   newfs -b 8192 -f 1024 $newfs_flags /dev/md$mdstart > /dev/null 2>&1
   mount /dev/md$mdstart $mntpoint
   cp /etc/passwd /etc/group /etc/hosts $mntpoint
   cp -r /usr/include/ufs $mntpoint
   umount $mntpoint

   for i in `jot 50`; do
      ./fuzz -n 50 $D
      if fsck -f -y /dev/md$mdstart 2>&1 | egrep "^[A-Z]" > /dev/null; then
         if fsck -f -y /dev/md$mdstart 2>&1 | egrep "^[A-Z]" > /dev/null; then
            if fsck -f -y /dev/md$mdstart 2>&1 | egrep "^[A-Z]" > /dev/null; then
               echo "fsck is giving up in loop $i!"
               break
            fi
         fi
      fi
      sync;sync;sync
      if mount /dev/md$mdstart $mntpoint; then
         ls -l $mntpoint > /dev/null
         find $mntpoint  -exec dd if={} of=/dev/null bs=1m count=3 \; > /dev/null 2>&1
         umount $mntpoint
      else
         echo "Giving up at loop $i"
         break
      fi
   done
   mdconfig -d -u $mdstart
   rm -f $D
}

odir=`pwd`
dir=/tmp

cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/fuzz.c
mycc -o fuzz -Wall fuzz.c
rm -f fuzz.c

for j in `jot 10`; do
   date '+%T'
   tst
done
rm -f fuzz

exit

EOF
#include <sys/stat.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void
usage(void)
{
	fprintf(stderr, "%s {-n <num|-v} <file>\n", getprogname());
	exit(1);
}

static long
random_long(long mi, long ma)
{
	return (arc4random()  % (ma - mi + 1) + mi);
}

int
main(int argc, char **argv)
{
	long pos;
	int ch, fd, i, times = 1, verbose = 0;
	unsigned char bit, buf, mask, old;
	struct stat sb;

	while ((ch = getopt(argc, argv, "n:v")) != -1) {
		switch(ch) {
		case 'n':	/* Bits to alter */
			if (sscanf(optarg, "%d", &times) != 1)
				usage();
			break;
		case 'v':	/* verbose flag */
			verbose += 1;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 0)
		usage();

	if ((fd = open(argv[0], O_RDWR)) == -1)
		err(1, "open(%s)", argv[0]);
	if (fstat(fd, &sb) == -1)
		err(1, "stat(%s)", argv[0]);

	for (i = 0; i < times; i++) {
		pos = random_long(0, sb.st_size - 1);
		if (lseek(fd, pos, SEEK_SET) == -1)
			err(1, "fseek(%d, %ld)", fd, pos);
		if (read(fd, &buf, 1) != 1)
			err(1, "read(%d)", fd);
		bit = random_long(0,7);
		mask = ~(1 << bit);
		old = buf;
		buf = (buf & mask) | (~buf & ~mask);
		if (verbose > 0)
			printf("Change %2x to %2x at %4ld "
			    "by flipping bit %d\n",
					old, buf, pos, bit);
		if (lseek(fd, pos, SEEK_SET) == -1)
			err(1, "fseek(%d, %ld)", fd, pos);
		if (write(fd, &buf, 1) != 1)
			err(1, "write(%d)", fd);
	}
	close(fd);

	return (0);
}
