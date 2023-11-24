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

# No problems seen

# fsync: giving up on dirty 0xc9659438: tag devfs, type VCHR
#     usecount 1, writecount 0, refcount 41 mountedhere 0xc96a2000
#     flags (VI_ACTIVE)
#     v_object 0xc9824aa0 ref 0 pages 355 cleanbuf 37 dirtybuf 2
#     lock type devfs: EXCL by thread 0xccbca6a0 (pid 42225, dd, tid 100234)
# #0 0xc0c26533 at __lockmgr_args+0xae3
# #1 0xc0d06cc3 at vop_stdlock+0x53
# #2 0xc1212cc0 at VOP_LOCK1_APV+0xe0
# #3 0xc0d2b24a at _vn_lock+0xba
# #4 0xc0f3f22b at ffs_sync+0x34b
# #5 0xc0f22343 at softdep_ast_cleanup_proc+0x213
# #6 0xc0ca7fbb at userret+0x1ab
# #7 0xc11e0618 at syscall+0x5e8
# #8 0xc11cb09e at Xint0x80_syscall+0x2e
#         dev label/tmp

. ../default.cfg

[ -d $RUNDIR ] || mkdir -p $RUNDIR &&
	find $RUNDIR -name "split.*" -delete
export LANG=C
s=0
kfree=`df -k $RUNDIR | tail -1 | awk '{print $4}'`
[ $kfree -gt 100 ] && kfree=$((kfree - 100))
parallel=$((`sysctl -n hw.ncpu` + 1))
parallel=2
cap=$((9 * 1024 * 1024))	# 9GB
mx=$((kfree / parallel / 2))
[ $mx -gt $cap ] && mx=$cap

run() {
	local blocks md5 md5a orig s spmax spmin

	s=0
	cd $RUNDIR
	blocks=`jot -r 1 1 $mx`
	dd if=/dev/random of=file.$1 bs=1023 count=$blocks status=none
	orig=`ls -l file.$1 | tail -1`
	md5=`md5 -q file.$1`
	spmin=$((1023 * 4))
	spmax=$((1023 * blocks / 2))
	rm -f split.$1.*
	split -a 4 -b `jot -r 1 $spmin $spmax` file.$1 split.$1.
	rm file.$1
	cat split.$1.* > file.$1
	rm split.$1.*
	md5a=`md5 -q file.$1`
	if [ $md5 != $md5a ]; then
		echo FAIL
		echo "orig $orig $md5"
		orig=`ls -l file.$1 | tail -1`
		echo "new  $orig $md5a"
		s=1
	fi
	rm file.$1
	return $s

}

for i in `jot $parallel`; do
	run $i &
	pids="$pids $!"
done
for pid in $pids; do
	wait $pid
	[ $? -ne 0 ] && s=1
done
exit $s
