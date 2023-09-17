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

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
. ../default.cfg

# "mount: /dev/md10: Invalid fstype: Invalid argument" seen.
# Reported by: soralx@cydem.org
# Fixed by: 017367c1146a

set -eu
prog=`basename ${0%.sh}`
log=/tmp/$prog.log
s=0
mdconfig -a -t swap -s 32T -u $mdstart
newfs -L tst0 -U -b 65536 -f 8192 -d 1048576 -g 131072 -h 16 -i 1048576 \
    /dev/md$mdstart > /dev/null || s=1
set +e
mount /dev/md$mdstart $mntpoint && umount $mntpoint || s=$((s | 2))
fsck_ffs -fy md$mdstart > $log 2>&1 || s=$((s | 4))
grep -Eq "IS CLEAN|MARKED CLEAN" $log || s=$((s | 8))
grep -Eq "WAS MODIFIED" $log && s=$((s | 16))
[ $s -ne 0 ] && tail -10 $log
mdconfig -d -u $mdstart
rm -f $log

exit $s
