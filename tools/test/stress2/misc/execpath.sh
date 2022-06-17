#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
# Copyright (c) 2021 Peter Holm <pho@FreeBSD.org>
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

# Bug 248184 - readlink("/proc/curproc/file") returns arbitrary correct name for programs with more than one link (name)
# Test scenario for D32611 "exec: provide right hardlink name in AT_EXECPATH"

. ../default.cfg
[ `id -u` -ne 0 ] && echo "Must be root!" && exit 1

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/execpath.c
mycc -o execpath -Wall -Wextra -O0 -g execpath.c || exit 1
rm -f execpath.c
cd $odir

set -e
mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 2g -u $mdstart
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
set +e

cd $mntpoint
mkdir d1 d2
cp /bin/sleep .
ln sleep d1/sleep
ln sleep d2/sleep
s=0
for p in $mntpoint/sleep $mntpoint/d1/sleep $mntpoint/d2/sleep; do
	(cd `dirname $p`; ./`basename $p` 10) &
	/bin/sleep .2
	path=`procstat binary $! | awk "/$!/ {print \\$NF}"`
	kill $!
	wait
	[ "$path" != "$p" ] && { s=1; echo "fail path=$path : prog=$p"; }
done
mv /tmp/execpath d1
ln d1/execpath d2/execpath

r=`./d1/execpath`
echo $r | grep -q "/d1/" || { s=1; echo "fail: $r. Expected d1 @ 1"; }
r=`./d2/execpath`
echo $r | grep -q "/d2/" || { s=1; echo "fail: $r. Expected d2 @ 2"; }
r=`(cd d1; ./execpath)`
echo $r | grep -q "/d1/" || { s=1; echo "fail: $r. Expected d1 @ 3"; }
r=`(cd d2; ./execpath)`
echo $r | grep -q "/d2/" || { s=1; echo "fail: $r. Expected d2 @ 4"; }

cd $odir

for i in `jot 6`; do
	mount | grep -q "on $mntpoint " || break
	umount $mntpoint && break || sleep 10
	[ $i -eq 6 ] &&
	    { echo FATAL; fstat -mf $mntpoint; exit 1; }
done
mdconfig -d -u $mdstart
exit $s

EOF
/* Test scenario by Tobias Kortkamp <tobik@freebsd.org> */

#include <sys/auxv.h>
#include <limits.h>
#include <stdio.h>

int
main(void)
{
	char pathname[PATH_MAX];

	elf_aux_info(AT_EXECPATH, pathname, PATH_MAX);
	puts(pathname);
	return 0;
}
