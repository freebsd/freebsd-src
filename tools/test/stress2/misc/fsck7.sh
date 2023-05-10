#!/bin/sh
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2022 Peter Holm <pho@FreeBSD.org>
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

# fsck -B test scenario

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
. ../default.cfg
mycc -o /tmp/zapsb -Wall -Wextra ../tools/zapsb.c || exit 1

set -e
log=/tmp/fsck7.sh.log
md=/dev/md$mdstart
[ -c $md ] && mdconfig -d -u $mdstart
mdconfig -a -t swap -s 5g -u $mdstart
newfs -U $md > /dev/null 2>&1
mount $md $mntpoint
set +e

jot 500 | xargs -P0 -I% touch $mntpoint/a%
umount $mntpoint
/tmp/zapsb $md

echo "Expect:
    Superblock check-hash failed"
mount -f $md $mntpoint

fsck -B -p -t ufs $md &

jot 500 | xargs -P0 -I% rm $mntpoint/a%
jot 500 | xargs -P0 -I% touch $mntpoint/b%

wait
umount $mntpoint
mount $md $mntpoint || exit 1	# The SB should be fixed at this point
umount $mntpoint
fsck -fy $md 2>&1 | tee $log
grep -q 'FILE SYSTEM IS CLEAN' $log && s=0 || s=1
mdconfig -d -u $mdstart
rm -f $log /tmp/zapsb
exit $s
