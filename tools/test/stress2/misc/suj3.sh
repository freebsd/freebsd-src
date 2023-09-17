#!/bin/sh

#
# Copyright (c) 2010 Peter Holm <pho@FreeBSD.org>
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

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

# Sector size > 512 test.

# "panic: Memory modified after free ..." seen.
# https://people.freebsd.org/~pho/stress/log/suj3-2.txt

. ../default.cfg

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 1g -u $mdstart

dd if=/dev/random of=/tmp/suj3.key bs=64 count=1 > /dev/null 2>&1
echo test | geli init -s 4096 -J - -K /tmp/suj3.key /dev/md$mdstart > /dev/null
echo test | geli attach -j - -k /tmp/suj3.key /dev/md$mdstart
newfs /dev/md$mdstart.eli > /dev/null

tunefs -j enable /dev/md$mdstart.eli
mount /dev/md$mdstart.eli $mntpoint
chmod 777 $mntpoint

export RUNDIR=$mntpoint/stressX
export runRUNTIME=5m

mount | grep -q md$mdstart.eli && \
	su $testuser -c "cd ..; ./run.sh rw.cfg"

while mount | grep $mntpoint | grep -q /dev/md; do
	umount $mntpoint || sleep 1
done
checkfs /dev/md$mdstart.eli; s=$?
geli kill /dev/md$mdstart.eli
mdconfig -d -u $mdstart
rm -f /tmp/suj3.key
exit $s
