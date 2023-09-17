#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2019 Dell EMC Isilon
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

old=`vmstat -m | awk '/GEOM/{print $2}'`
set -e
mount | grep -q "on $mntpoint " && umount $mntpoint
mdconfig -l | grep -q md$mdstart && mdconfig -d -u $mdstart
mdconfig -a -t swap -s 1g -u $mdstart
newfs -U /dev/md$mdstart > /dev/null
set +e
start=` date +%s`
while [ $((`date +%s` - start)) -lt 60 ]; do
	mount /dev/md$mdstart $mntpoint
	while mount | grep -q "on $mntpoint "; do
		umount $mntpoint
	done
done
mdconfig -d -u $mdstart

new=`vmstat -m | awk '/GEOM/{print $2}'`
if [ $((new - old)) -gt 5 ]; then
        s=1
	vmstat -m | sed -n '1p;/GEOM/p'
else
        s=0
fi
exit $s
