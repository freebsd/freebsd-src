#!/bin/sh

#
# Copyright (c) 2018 Dell EMC Isilon
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

# GEOM leak regression test.
# The problem was introduced in r328426 and fixed by r329375.

. ../default.cfg
[ `id -u` -ne 0 ] && echo "Must be root!" && exit 1

set -e
[ -c /dev/md$mdstart ] && mdconfig -d -u $mdstart

old=`vmstat -m | awk '/GEOM/{print $2}'`
for i in `jot 50`; do
	mdconfig -a -t swap -s 500m -u $mdstart
	newfs $newfs_flags /dev/md$mdstart > /dev/null 2>&1
	mdconfig -d -u $mdstart
done
set +e
new=`vmstat -m | awk '/GEOM/{print $2}'`
if [ $((new - old)) -gt 5 ]; then
	s=1
	echo "InUse changed from $old to $new, leaking $((new - old)) GEOMs"
else
	s=0
fi
exit $s
