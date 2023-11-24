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

# /dev/md* fails to be created.
# "kernel: g_dev_taste: make_dev_p() failed (gp->name=md10, error=17)" seen.
# The cause is that the device node is removed asynchronously.

. ../default.cfg
[ `id -u` -ne 0 ] && echo "Must be root!" && exit 1

[ -c /dev/md$mdstart ] && mdconfig -d -u $mdstart
mdconfig -l | grep -q md$mdstart &&
    { echo FAIL; ls -l /dev/md$mdstart; mdconfig -lv; exit 1; }

s=0
start=`date +%s`
workaround=${workaround:-0}
while [ $((`date +%s` - start)) -lt 60 ]; do
	mdconfig -a -t swap -s 100m -u $mdstart || { s=2; break; }
	[ -c /dev/md$mdstart ] ||
	    { echo FAIL; ls -l /dev/md$mdstart; mdconfig -lv; exit 3; }
	mdconfig -d -u $mdstart || { s=4; break; }
	while [ $workaround -eq 1 -a -c /dev/md$mdstart ]; do
		echo "Note: Waiting for removal of /dev/md$mdstart"
		sleep .1
	done
done
exit $s
