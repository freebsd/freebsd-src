#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026 Maksym Sobolyev <sobomax@FreeBSD.org>
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

# Forced unmount racing mkdir(2) in a directory at UFS_LINK_MAX on SU.
#
# With soft updates, rmdir(2) lowers the parent's i_effnlink at once and
# its i_nlink only when the removal is on disk.  A mkdir(2) in between
# finds i_nlink at UFS_LINK_MAX and goes through ufs_sync_nlink(), which
# syncs the file system and restarts.  The sync used to busy the mount
# and sleep in vfs_busy() while an unmount was in progress, with the
# mkdir(2) still counted as a write by vn_start_write(), while the unmount
# waited in vfs_write_suspend() for that write to end.  The unmount and
# every later vfs_busy() of the mount point hung.
#
# Set dtrace=1 to count, while an unmount is in progress, the vfs_busy()
# calls by caller, and the mkdir(2) calls that synced and restarted with
# ERELOOKUP.  Without the fix, a vfs_busy() from ufs_sync_nlink() here is
# the deadlock; with it, the mkdir(2) calls show the race was hit.

[ `id -u` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

UFS_LINK_MAX=`grep UFS_LINK_MAX /usr/include/ufs/ufs/dinode.h 2>/dev/null`
[ -z "$UFS_LINK_MAX" ] && exit 0
UFS_LINK_MAX=`echo $UFS_LINK_MAX | awk '{print $3}'`
nworkers=8
runtime=${runtime:-300}
log=/tmp/nlink6.log

cat > /tmp/nlink6.c <<EOF
#include <sys/stat.h>
#include <ufs/ufs/dinode.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/*
 * "setup": fill directory d with subdirectories up to UFS_LINK_MAX, the
 * first nworkers of them named w<id>.
 * "work <d> <id> <seconds>": remove and recreate <d>/w<id> in a loop.
 * Work relative to <d>: a lookup through the mount point would wait for
 * the unmount on the covered vnode lock and never race with it.
 */
int
main(int argc, char *argv[])
{
	char path[1024];
	time_t start;
	int i, nworkers, secs;

	if (argc == 3 && strcmp(argv[1], "setup") == 0) {
		nworkers = atoi(argv[2]);
		if (mkdir("d", 0755) == -1)
			err(1, "mkdir(d)");
		for (i = 0; i < UFS_LINK_MAX - 2; i++) {
			if (i < nworkers)
				snprintf(path, sizeof(path), "d/w%d", i);
			else
				snprintf(path, sizeof(path), "d/%d", i);
			if (mkdir(path, 0755) == -1)
				err(1, "mkdir(%s)", path);
		}
		return (0);
	}
	if (argc != 5 || strcmp(argv[1], "work") != 0)
		errx(1, "usage: nlink6 setup <n> | work <dir> <id> <seconds>");
	if (chdir(argv[2]) == -1)
		err(1, "chdir(%s)", argv[2]);
	snprintf(path, sizeof(path), "w%s", argv[3]);
	secs = atoi(argv[4]);
	start = time(NULL);
	while (time(NULL) - start < secs) {
		/* Errors are expected once the file system is gone. */
		if (rmdir(path) == -1 || mkdir(path, 0755) == -1)
			usleep(1000);
	}
	return (0);
}
EOF
mycc -o /tmp/nlink6 -Wall -Wextra -O2 /tmp/nlink6.c || exit 1
rm /tmp/nlink6.c

set -e
mount | grep -q "on $mntpoint " && umount -f $mntpoint
mdconfig -l | grep -q "md$mdstart" && mdconfig -d -u $mdstart
mdconfig -a -t swap -s 1g -u $mdstart
newfs -Un /dev/md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
(cd $mntpoint; /tmp/nlink6 setup $nworkers)
umount $mntpoint
set +e

if [ "$dtrace" = 1 ]; then
	# The unmount-in-progress flag (MNTK_UNMOUNT) is 0x01000000.
	dtrace -q -n 'fbt::vfs_busy:entry
	    /(args[0]->mnt_kern_flag & 0x01000000) != 0/
	    { @b[stack(2)] = count(); }
	fbt::ufs_mkdir:entry { self->mp = args[0]->a_dvp->v_mount; }
	fbt::ufs_mkdir:return
	    /self->mp != NULL && (int)arg1 == -5 &&
	    (self->mp->mnt_kern_flag & 0x01000000) != 0/
	    { @m = count(); }
	fbt::ufs_mkdir:return { self->mp = NULL; }
	END {
		printf("vfs_busy() during unmount, by caller:");
		printa(@b);
		printa("mkdir(2) synced and restarted during unmount: %@d\n",
		    @m);
	}' > $log.dtrace &
	dpid=$!
	sleep 5
fi

s=0
start=`date +%s`
n=0
while [ $((`date +%s` - start)) -lt $runtime ]; do
	mount /dev/md$mdstart $mntpoint || { s=1; break; }
	pids=
	for i in `jot $nworkers 0`; do
		/tmp/nlink6 work $mntpoint/d $i 10 &
		pids="$pids $!"
	done
	sleep `jot -r 1 1 5`
	umount -f $mntpoint &
	upid=$!
	t=0
	while kill -0 $upid 2>/dev/null; do
		sleep 1
		t=$((t + 1))
		if [ $t -eq 120 ]; then
			echo "FAIL: umount -f $mntpoint is stuck"
			procstat -kk $upid `pgrep -x nlink6`
			if [ -n "$dpid" ]; then
				kill -TERM $dpid
				wait $dpid
				cat $log.dtrace
			fi
			# The mount point stays busy, leave everything as is.
			exit 1
		fi
	done
	wait $upid $pids
	n=$((n + 1))
	if ! fsck_ffs -fy /dev/md$mdstart > $log 2>&1; then
		echo "FAIL: fsck_ffs exit $? after cycle $n"
		tail -20 $log
		s=1
		break
	fi
done
echo "$n forced unmounts"

if [ -n "$dpid" ]; then
	kill -TERM $dpid
	wait $dpid
	cat $log.dtrace
fi
mdconfig -d -u $mdstart
rm -f /tmp/nlink6 $log $log.dtrace
exit $s
