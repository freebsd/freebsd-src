#!/bin/sh

#
# Copyright (c) 2013 EMC Corp.
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

# Test scenario for the change of a global SU lock to a per filesystem lock.

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > pfl.c
mycc -o pfl -Wall -Wextra pfl.c || exit 1
rm -f pfl.c
cd $here

md1=$mdstart
md2=$((mdstart + 1))
mp1=${mntpoint}$md1
mp2=${mntpoint}$md2
mkdir -p $mp1 $mp2

usermem=`sysctl -n hw.usermem`
[ `swapinfo | wc -l` -eq 1 ] && usermem=$((usermem/100*80))
size=$((2 * 1024 * 1024 * 1024))	# Ideal disk size is 2G
[ $((size * 2)) -gt $usermem ] && size=$((usermem / 2))
size=$((size / 1024 / 1024))

opt=$([ $((`date '+%s'` % 2)) -eq 0 ] && echo "-j" || echo "-U")
[ "$newfs_flags" = "-U" ] || opt=""
mount | grep "on $mp1 " | grep -q /dev/md && umount -f $mp1
[ -c /dev/md$md1 ] &&  mdconfig -d -u $md1
mdconfig -a -t swap -s ${size}m -u $md1
newfs $opt md${md1} > /dev/null
mount /dev/md${md1} $mp1
chmod 777 $mp1

mount | grep "on $mp2 " | grep -q /dev/md && umount -f $mp2
[ -c /dev/md$md2 ] &&  mdconfig -d -u $md2
mdconfig -a -t swap -s ${size}m -u $md2
newfs $opt md${md2} > /dev/null
mount /dev/md${md2} $mp2
chmod 777 $mp2

su $testuser -c "cd $mp1; /tmp/pfl" &
pids=$!
su $testuser -c "cd $mp2; /tmp/pfl" &
pids="$pids $!"
sleep .5
s=0
start=`date '+%s'`
while pgrep -q pfl; do
	if [ $((`date '+%s'`- start)) -gt 900 ]; then
		s=1
		echo "$0 timed out."
		pkill -9 pfl
	fi
	sleep 10
done
for p in $pids; do
	wait $p
	[ $? -ne 0 ] && s=2
done

while mount | grep "$mp2 " | grep -q /dev/md; do
	umount $mp2 || sleep 1
done
mdconfig -d -u $md2
while mount | grep "$mp1 " | grep -q /dev/md; do
	umount $mp1 || sleep 1
done
rm -f /tmp/pfl
mdconfig -d -u $md1
exit $s

EOF
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define PARALLEL 10

static void
test(void)
{
	pid_t pid;
	int fd, i, j;
	char file[128];

	pid = getpid();
	sprintf(file,"d%05d", pid);
	if (mkdir(file, 0740) == -1)
		err(1, "mkdir(%s)", file);
	chdir(file);
	for (j = 0; j < 10000; j++) {
		sprintf(file,"p%05d.%05d", pid, j);
		if ((fd = open(file, O_CREAT | O_TRUNC | O_WRONLY, 0644)) ==
		    -1) {
			if (errno != EINTR) {
				warn("mkdir(%s). %s:%d", file, __FILE__,
				    __LINE__);
				unlink("continue");
				break;
			}
		}
		if (arc4random() % 100 < 10)
			if (write(fd, "1", 1) != 1)
				err(1, "write()");
		close(fd);

	}
	sleep(3);

	for (i = --j; i >= 0; i--) {
		sprintf(file,"p%05d.%05d", pid, i);
		if (unlink(file) == -1)
			err(3, "unlink(%s)", file);

	}
	chdir("..");
	sprintf(file,"d%05d", pid);
	if (rmdir(file) == -1)
		err(3, "unlink(%s)", file);
}

int
main(void)
{
	pid_t pids[PARALLEL];
	int e, fd, j, k, s;

	umask(0);
	if ((fd = open("continue", O_CREAT, 0644)) == -1)
		err(1, "open()");
	close(fd);
	e = 0;
	for (j = 0; j < PARALLEL; j++) {
		if ((pids[j] = fork()) == 0) {
			for (k = 0; k < 40; k++)
				test();
			_exit(0);
		}
	}

	for (j = 0; j < PARALLEL; j++) {
		if (waitpid(pids[j], &s, 0) == -1)
			err(1, "waitpid(%d)", pids[j]);
		e += s == 0 ? 0 : 1;
	}

	return (e);
}
