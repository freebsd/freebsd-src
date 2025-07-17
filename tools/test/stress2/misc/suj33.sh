#!/bin/sh

#
# Copyright (c) 2013 Peter Holm <pho@FreeBSD.org>
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

# Test "umount" of active file system
# Triggers "fsync: giving up on dirty" with ease.

# umount -f fails with
# "softdep_waitidle: Failed to flush worklist for 0xfffff80308cea000"
# Fixed by r356763

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

mount | grep "$mntpoint" | grep -q md$mdstart && umount $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 1g -u $mdstart
newfs -j md$mdstart  > /dev/null
mount /dev/md$mdstart $mntpoint
export RUNDIR=$mntpoint/stressX
export runRUNTIME=3m            # Run tests for three minutes
chmod 777 $mntpoint

su $testuser -c '(cd ..; ./run.sh disk.cfg)' > /dev/null 2>&1 &
sleep 20
umount $mntpoint > /dev/null 2>&1
kill $!
../tools/killall.sh || exit
wait

for i in `jot 6`; do
	mount | grep -q "on $mntpoint " || break
	umount $mntpoint && break || sleep 10
	[ $i -eq 6 ] &&
	    { echo FAIL; fstat -mf $mntpoint; exit 1; }
done
checkfs /dev/md$mdstart; s=$?
mdconfig -d -u $mdstart
exit $s
