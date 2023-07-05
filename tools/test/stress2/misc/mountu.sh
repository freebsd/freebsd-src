#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2012 Peter Holm <pho@FreeBSD.org>
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

# Change mount point from rw to ro with a file mapped rw
# Currently fails for NFS

# Page fault seen:
# https://people.freebsd.org/~pho/stress/log/mountu.txt
# Fixed by: r285039.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > mountu.c
mycc -o mountu -Wall -Wextra -O2 mountu.c || exit 1
rm -f mountu.c

pstat() {
	local pid
	pid=`ps ax | grep -v grep | grep /tmp/mountu | awk '{print $1}'`
	[ -n "$pid" ] && procstat -v $pid
}

ck() {
	if mount | grep $mntpoint | grep -q "read-only"; then
		if pstat $!| awk "\$2 == \"$map\"" | grep -q " rw-"; then
			echo
			echo "$1 difference"
			mount | grep $mntpoint
			printf "RW mount mapping and RO mount mapping:\n%s\n" "$r"
			pstat $! | awk "\$2 == \"$map\""
			status=$((status + 1))
		fi
	else
		echo "$1 mount point RO did not succeed"
		mount | grep $mntpoint
		status=$((status + 1))
	fi
}

status=0
file=$mntpoint/mountu.sh.file
mapfile=/tmp/mountu.sh.map
mount | grep -q "$mntpoint " && umount $mntpoint
mdconfig -l | grep -q $mdstart && mdconfig -d -u $mdstart
mdconfig -a -t swap -s 100m -u $mdstart
gpart create -s bsd md$mdstart > /dev/null
gpart add -t freebsd-ufs md$mdstart > /dev/null
part=a
newfs $newfs_flags md${mdstart}$part > /dev/null
mount /dev/md${mdstart}$part $mntpoint
chmod 777 $mntpoint

# ufs
exec 5>$mapfile
/tmp/mountu UFS $file &
pid=$!
sleep 1
map=`cat $mapfile`; rm $mapfile; exec 5>&-

r=`pstat $! | awk "\\$2 == \"$map\""`
mount -u -o ro $mntpoint 2>/dev/null || mount -fu -o ro $mntpoint
ck UFS
mount -u -o rw $mntpoint
rm -f $file
wait $pid
s=$?
[ $s -ne 139 ] && { echo "UFS exit status is $s"; status=1; }
while mount | grep -q "$mntpoint "; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart

# nfs
if ping -c 2 `echo $nfs_export | sed 's/:.*//'` > /dev/null 2>&1; then
	mount -t nfs -o tcp -o retrycnt=3 -o intr,soft -o rw $nfs_export \
	    $mntpoint
	sleep .2
	rm -f $file
	/tmp/mountu NFS $file &
	pid=$!
	sleep 1

	r=`pstat $! | awk "\\$2 == \"$map\""`
	mount -u -o ro $mntpoint 2>/dev/null ||
	    mount -fu -o ro $mntpoint 2>/dev/null
	ck NFS
	wait $pid
	s=$?
	[ $s -ne 139 ] && { echo "NFS exit status is $s"; status=1; }

	mount -u -o rw $mntpoint 2>/dev/null
	sleep .2
	[ -f $file ] && rm -f $file
	umount $mntpoint || umount $mntpoint
fi

# msdos
if [ -x /sbin/mount_msdosfs ]; then
	mdconfig -a -t swap -s 100m -u $mdstart
	gpart create -s bsd md$mdstart > /dev/null
	gpart add -t freebsd-ufs md$mdstart > /dev/null
	part=a
	newfs_msdos -F 16 -b 8192 /dev/md${mdstart}$part > /dev/null 2>&1
	mount_msdosfs -m 777 /dev/md${mdstart}$part $mntpoint
	/tmp/mountu MSDOS $file &
	pid=$!

	sleep 1
	r=`pstat $! | awk "\\$2 == \"$map\""`
	mount -u -o ro $mntpoint 2>/dev/null || mount -fu -o ro $mntpoint
	ck MSDOS
	wait $pid
	s=$?
	[ $s -ne 139 ] && { echo "MSDOS exit status is $s"; status=1; }
	mount -u -o rw $mntpoint
	rm -f $file

	while mount | grep -q "$mntpoint "; do
		umount $mntpoint || sleep 1
	done
	mdconfig -d -u $mdstart
fi

# tmpfs
mount -t tmpfs null $mntpoint
chmod 777 $mntpoint

/tmp/mountu TMPFS $file &
pid=$!

sleep 1
r=`pstat $! | awk "\\$2 == \"$map\""`
mount -u -o ro $mntpoint 2>/dev/null || mount -fu -o ro $mntpoint
ck TMPFS
sleep 1
mount -u -o rw $mntpoint
rm -f $file
wait $pid
s=$?
[ $s -ne 139 ] && { echo "TMPFS exit status is $s"; status=1; }
while mount | grep -q "$mntpoint "; do
	umount $mntpoint || sleep 1
done

rm -f /tmp/mountu
exit 0
EOF
/* kib@ noted:
   UFS/NFS/msdosfs reclaim vnode on rw->ro forced remount, and
   change the type of the underying object to OBJT_DEAD, but leave
   the pages on the object queue and installed in the page tables.
   Applications can read/write already mapped pages, but cannot
   page in new pages, cannot observe possible further modifications
   to already mapped pages (if ro->rw remount happen later), and
   their updates to pages are not flushed to file.

   It is impossible to mimic this behaviour for tmpfs.
 */
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/ucontext.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define STARTADDR 0x0U
#define ADRSPACE 0x0640000U

static void
sighandler(int signo, siginfo_t *si, void *uc1)
{
	ucontext_t *uc;

	uc = uc1;
	printf("SIG%s at %p, addr %p\n", sys_signame[signo], si->si_addr,
#if defined(__i386__)
	    (void *)uc->uc_mcontext.mc_eip);
#else
	    (void *)uc->uc_mcontext.mc_rip);
#endif
	exit(1);
}

int
main(int argc __unused, char **argv)
{
	struct passwd *pw;
	struct sigaction sa;
	void *p;
	size_t len;
	int fd;
	char *name, *path;
	volatile char *c;

	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = sighandler;
	sa.sa_flags = SA_SIGINFO;
	if (sigaction(SIGSEGV, &sa, NULL) == -1)
		err(1, "sigaction(SIGSEGV)");
	if (sigaction(SIGBUS, &sa, NULL) == -1)
		err(1, "sigaction(SIGBUS)");

	if ((pw = getpwnam("nobody")) == NULL)
		err(1, "no such user: nobody");

	if (setgroups(1, &pw->pw_gid) ||
	    setegid(pw->pw_gid) || setgid(pw->pw_gid) ||
	    seteuid(pw->pw_uid) || setuid(pw->pw_uid))
		err(1, "Can't drop privileges to \"nobody\"");
	endpwent();

	p = (void *)STARTADDR;
	len = ADRSPACE;

	name = argv[1];
	path = argv[2];
	if ((fd = open(path, O_CREAT | O_TRUNC | O_RDWR, 0622)) == -1)
		err(1,"open(%s)", path);
	if (ftruncate(fd, len) == -1)
		err(1, "ftruncate");
	if ((p = mmap(p, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) ==
			MAP_FAILED) {
		if (errno == ENOMEM) {
			warn("mmap()");
			return (1);
		}
		err(1, "mmap(1)");
	}
	dprintf(5, "%p\n", p);

	for (c = p; (void *)c < p + len; c += PAGE_SIZE) {
		*c = 1;
	}

	close(fd);
	sleep(5);
	fprintf(stderr, "%s: Late read start.\n", name);
	for (c = p; (void *)c < p + len; c += PAGE_SIZE) {
		*c;
	}
	fprintf(stderr, "%s: Late read complete.\n", name);

	fprintf(stderr, "%s: Late write start.\n", name);
	for (c = p; (void *)c < p + len; c += PAGE_SIZE) {
		*c = 1;
	}
	fprintf(stderr, "%s: Late write complete.\n", name);

	return (0);
}
