#!/bin/sh

#
# Copyright (c) 2014 EMC Corp.
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

# Demonstrate that fts_read(3) will open a fifo for read.
# Not seen on a pristine FreeBSD.

# $ while ./fifo.sh; do date; done
# Wed Oct  1 14:07:41 CEST 2014
# Wed Oct  1 14:09:58 CEST 2014
# FAIL
# $ ps -l19547
# UID   PID  PPID CPU PRI NI   VSZ  RSS MWCHAN STAT TT     TIME COMMAND
#   0 19547 19544   0  25  0 12176 3996 fifoor I     0  0:08.19 /tmp/fifo
# $ gdb /tmp/fifo 19547
# GNU gdb 6.1.1 [FreeBSD]
# Copyright 2004 Free Software Foundation, Inc.
# GDB is free software, covered by the GNU General Public License, and you are
# welcome to change it and/or distribute copies of it under certain conditions.
# Type "show copying" to see the conditions.
# There is absolutely no warranty for GDB.  Type "show warranty" for details.
# This GDB was configured as "amd64-marcel-freebsd"...
# Attaching to program: /tmp/fifo, process 19547
# Reading symbols from /lib/libc.so.7...done.
# Loaded symbols for /lib/libc.so.7
# Reading symbols from /libexec/ld-elf.so.1...done.
# Loaded symbols for /libexec/ld-elf.so.1
# 0x00000008008a9ab8 in enc_openat () from /lib/libc.so.7
# (gdb) bt
# #0  0x00000008008a9ab8 in enc_openat () from /lib/libc.so.7
# #1  0x00000008008a581b in fts_read () from /lib/libc.so.7
# #2  0x00000008008a4f24 in fts_read () from /lib/libc.so.7
# #3  0x0000000000400ee9 in test () at /tmp/fifo.c:86
# #4  0x0000000000400fd8 in main () at /tmp/fifo.c:108
# (gdb) f 3
# #3  0x0000000000400ee9 in test () at /tmp/fifo.c:86
# 86                      while ((p = fts_read(fts)) != NULL) {
# Current language:  auto; currently minimal
# (gdb)
#

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
. ../default.cfg

cat > /tmp/fifo3.c <<EOF
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fts.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define LOOPS 50
#define PARALLEL 4

void
tmkfifo(void)
{
	pid_t pid;
	int i, j;
	char name[80];

	setproctitle(__func__);
	pid = getpid();
	for (j = 0; j < LOOPS; j++) {
		for (i = 0; i < 1000; i++) {
			snprintf(name, sizeof(name), "fifo.%d.%06d", pid, i);
			if (mkfifo(name, 0644) == -1)
				err(1, "mkfifo(%s)", name);
		}
		for (i = 0; i < 1000; i++) {
			snprintf(name, sizeof(name), "fifo.%d.%06d", pid, i);
			if (unlink(name) == -1)
				err(1, "unlink(%s)", name);
		}
	}
	_exit(0);
}

void
tmkdir(void)
{
	pid_t pid;
	int i, j;
	char name[80];

	setproctitle(__func__);
	pid = getpid();
	for (j = 0; j < LOOPS; j++) {
		for (i = 0; i < 1000; i++) {
			snprintf(name, sizeof(name), "dir.%d.%06d", pid, i);
			if (mkdir(name, 0644) == -1)
				err(1, "mkdir(%s)", name);
		}
		for (i = 0; i < 1000; i++) {
			snprintf(name, sizeof(name), "dir.%d.%06d", pid, i);
			if (rmdir(name) == -1)
				err(1, "unlink(%s)", name);
		}
	}
	_exit(0);
}

void
test(void)
{
	FTS *fts;
	FTSENT *p;
	int ftsoptions, i;
	char *args[2];

	if (fork() == 0)
		tmkfifo();
	if (fork() == 0)
		tmkdir();

	ftsoptions = FTS_PHYSICAL;
	args[0] = ".";
	args[1] = 0;

	for (i = 0; i < LOOPS; i++) {
		if ((fts = fts_open(args, ftsoptions, NULL)) == NULL)
			err(1, "fts_open");

		while ((p = fts_read(fts)) != NULL) {
#if defined(TEST)
			fprintf(stdout, "%s\n", p->fts_path);
#endif
		}

		if (errno != 0 && errno != ENOENT)
			err(1, "fts_read");
		if (fts_close(fts) == -1)
			err(1, "fts_close()");
	}
	wait(NULL);
	wait(NULL);

	_exit(0);
}

int
main(void)
{
	int i;

	for (i = 0; i < PARALLEL; i++)
		if (fork() == 0)
			test();
	for (i = 0; i < PARALLEL; i++)
		wait(NULL);

	return (0);
}
EOF
mycc -o /tmp/fifo3 -Wall -Wextra -O0 -g /tmp/fifo3.c || exit 1

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] && mdconfig -d -u $mdstart

mdconfig -a -t swap -s 1g -u $mdstart || exit 1
newfs md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint

(cd $mntpoint; /tmp/fifo3 ) &

while pgrep -q fifo3; do
	ps -lx | grep -v grep | grep -q fifoor &&
	    { echo FAIL; exit 1; }
	sleep 2
done

wait

while mount | grep $mntpoint | grep -q /dev/md; do
        umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
rm /tmp/fifo3 /tmp/fifo3.c
