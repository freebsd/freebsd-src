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

. ../default.cfg
[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

export LANG=C
disk=/tmp/dumpfs.sh.`date +%Y%m%dT%H%M%S`.img
dump=${disk%.img}.dump
good=/tmp/dumpfs.sh.good
md=bc9011de1e3c33a3baf2bf195708417e # 20190902
md=b86221bacc4651b0a92032f6989302c4 # 20200710
md=a6f60b37ffca5fb61fc54ccbd00ecbcb # 20210109

s=0

dd if=/dev/zero of=$disk bs=512 count=20480 status=none
[ -c /dev/md$mdstart ] && mdconfig -d -u $mdstart
mdconfig -a -f $disk -u $mdstart
newfs -U /dev/md$mdstart > /dev/null || exit 1

mount /dev/md$mdstart $mntpoint || exit 1

here=`pwd`
cd $mntpoint
for i in `jot 10`; do
	echo $i
done > file1
cp file1 file2
cp file2 file3
mv file1 filea
rm file2

cd $here
umount $mntpoint
dumpfs /dev/md$mdstart | expand > $dump
r=`sed 's/time  *[A-Z].*//;s/id .*//' < $dump | md5`
if [ $md != $r ]; then
	echo "$r != $md"
	s=1
	[ -f $good ] && diff $good $dump
else
	[ ! -f $good ] && mv $dump $good # save good dump
fi
mdconfig -d -u $mdstart
[ $s -eq 0 ] && rm -f $disk $dump
rm -f $disk $dump # for now
exit $s
