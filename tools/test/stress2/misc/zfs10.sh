#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
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

# sendfile() over zfs. No problems seen.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
[ `uname -m` = "i386" ] && exit 0
[ `sysctl -n kern.kstack_pages` -lt 4 ] && exit 0

. ../default.cfg

[ `sysctl -n hw.physmem` -lt $(( 4 * 1024 * 1024 * 1024)) ] && exit 0
kldstat -v | grep -q zfs.ko  || { kldload zfs.ko ||
    exit 0; loaded=1; }

u1=$mdstart
u2=$((u1 + 1))
zdir=/stress2_tank/test

mdconfig -l | grep -q md$u1 && mdconfig -d -u $u1
mdconfig -l | grep -q md$u2 && mdconfig -d -u $u2

mdconfig -s 1g -u $u1 || exit 1
mdconfig -s 1g -u $u2 || exit 1

zpool list | egrep -q "^stress2_tank" && zpool destroy stress2_tank
[ -d /stress2_tank ] && rm -rf /stress2_tank
zpool create stress2_tank md$u1 md$u2 || exit 1
zfs create stress2_tank/test || exit 1

(cd ../testcases/swap; ./swap -t 5m -i 20 -l 100) &

odir=`pwd`

echo sendfile.sh
cd /tmp
sed '1,/^EOF/d' < $odir/sendfile.sh > sendfile.c
mycc -o sendfile -Wall sendfile.c -pthread || exit 1
rm -f sendfile.c
cd $zdir

in=inputFile
out=outputFile

for i in 1 2 3 4 8 16 1k 2k 3k 4k 5k 1m 2m 3m 4m 5m ; do
	rm -f $in $out
	dd if=/dev/random of=$in bs=$i count=1 status=none
	/tmp/sendfile $in $out 12345
	cmp $in $out || { echo FAIL; ls -l $in $out; }
	rm -f $in $out
done
rm -f /tmp/sendfile

echo sendfile4.sh
cd /tmp
sed '1,/^EOF/d' < $odir/sendfile4.sh > sendfile4.c
mycc -o sendfile4 -Wall -Wextra -O2 sendfile4.c || exit
rm -f sendfile4.c
cd $zdir

dd if=/dev/zero of=diskimage bs=1m count=6 status=none
/tmp/sendfile4 diskimage || echo FAIL

rm -f /tmp/sendfile4 diskimage

echo sendfile5.sh
cd /tmp
sed '1,/^EOF/d' < $odir/sendfile5.sh > sendfile5.c
mycc -o sendfile5 -Wall -Wextra -O2 sendfile5.c || exit
rm -f sendfile5.c
cd $zdir
dd if=/dev/zero of=diskimage bs=1m count=1024 status=none
/tmp/sendfile5 diskimage
rm -f /tmp/sendfile5 diskimage

echo sendfile8.sh
cd /tmp
sed '1,/^EOF/d' < $odir/sendfile8.sh > sendfile8.c
mycc -o sendfile8 -Wall -Wextra -O2 sendfile8.c || exit
rm -f sendfile8.c
cd $zdir
dd if=/dev/random of=in bs=1m count=50 status=none
/tmp/sendfile8 in out 76543
rm -f /tmp/sendfile8 in out

echo sendfile9.sh
cd /tmp
sed '1,/^EOF/d' < $odir/sendfile9.sh > sendfile9.c
mycc -o sendfile9 -Wall -Wextra -O2 sendfile9.c || exit
rm -f sendfile9.c
cd $zdir
dd if=/dev/random of=in bs=1m count=50 status=none
/tmp/sendfile9 in out 76543
rm -f /tmp/sendfile9 in out

echo sendfile12.sh
cd /tmp
sed '1,/^EOF/d' < $odir/sendfile12.sh > sendfile12.c
mycc -o sendfile12 -Wall -Wextra -O2 sendfile12.c || exit
rm -f sendfile12.c
cd $zdir
dd if=/dev/zero of=in bs=1m count=512 status=none
/tmp/sendfile12 in out 76543
rm -f /tmp/sendfile12 in out

while pkill swap; do sleep .1; done
wait

cd $odir
zfs destroy -r stress2_tank
zpool destroy stress2_tank

mdconfig -d -u $u1
mdconfig -d -u $u2
[ -n "$loaded" ] && kldunload zfs.ko
exit 0
