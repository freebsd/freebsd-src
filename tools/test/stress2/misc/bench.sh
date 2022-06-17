#!/bin/sh

#
# Copyright (c) 2017 Dell EMC Isilon
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

# "benchmark" for file system use, using no physical disk.

. ../default.cfg
[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

export LANG=C
dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/bench.c
mycc -o bench -Wall -Wextra -O0 -g bench.c || exit 1
rm -f bench.c
cd $odir

mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 1g -u $mdstart || exit 1

log=/tmp/stress2.d/bench.sh.log
[ -f $log ] && old=`tail -1 $log | awk '{print $2}'`
tmp=/tmp/bench.sh.tmp
s=0
for j in `jot 5`; do
	newfs -n -b 4096 -f 512 -i 1024 md$mdstart > \
	    /dev/null
	mount -o async /dev/md$mdstart $mntpoint
	/usr/bin/time sh -c "(cd $mntpoint; /tmp/bench)" 2>&1 | \
	    awk '{print $1}'
	[ $? -ne 0 ] && s=1
	umount $mntpoint
done | ministat -n | tail -1 | awk '{printf "%.3f %.3f\n",$6,$7}' > $tmp
r=`cat $tmp`
echo "`date +%Y%m%d%H%M` $r `uname -a`" >> $log
tail -5 $log | cut -c 1-92
rm $tmp

if [ $old ]; then
    awk -v old=$old -v new=$(echo $r | awk '{print $1}') \
    'BEGIN {if ((new - old) * 100 / old > 5) exit 1; else exit 0}'
    s=$?
fi

for i in `jot 6`; do
	mount | grep -q "on $mntpoint " || break
	umount $mntpoint && break || sleep 10
done
[ $i -eq 6 ] && exit 1
mdconfig -d -u $mdstart
rm -rf /tmp/bench
exit 0

EOF
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define LOOPS 50000
#define TESTS 6
#define TIMEOUT 600

static void (*functions[TESTS])();

static void
t1(void)
{
	int fd, i;
	char file[128];

	alarm(TIMEOUT);
	for (i = 0; i < LOOPS; i++) {
		if (i % 1000 == 0)
			setproctitle("%s @ %d", __func__, i);
		snprintf(file, sizeof(file), "t1.%06d.%03d", getpid(), i);
		if ((fd = open(file, O_RDWR | O_CREAT | O_TRUNC,
		    DEFFILEMODE)) == -1)
			err(1, "open(%s)", file);
		close(fd);
		if (unlink(file) == -1)
			err(1, "unlink(%s)", file);
		usleep(100);
	}
	_exit(0);
}

static void
t2(void)
{
	int fd, i;
	char file[128];

	alarm(TIMEOUT);
	for (i = 0; i < LOOPS; i++) {
		if (i % 1000 == 0)
			setproctitle("%s @ %d", __func__, i);
		snprintf(file, sizeof(file), "t2.%06d.%03d", getpid(), i);
		if ((fd = open(file, O_RDWR | O_CREAT | O_TRUNC,
		    DEFFILEMODE)) == -1)
			err(1, "open(%s)", file);
		close(fd);
		usleep(100);
	}
	for (i = 0; i < LOOPS; i++) {
		snprintf(file, sizeof(file), "t2.%06d.%03d", getpid(), i);
		if (unlink(file) == -1)
			err(1, "unlink(%s)", file);
	}
	_exit(0);
}

static void
t3(void)
{
	int fd, i;
	char dir[128], file[128];

	alarm(TIMEOUT);
	snprintf(dir, sizeof(dir), "t3.%06d.dir", getpid());
	if (mkdir(dir, 700) == -1)
		err(1, "mkdir(%s)", dir);
	for (i = 0; i < LOOPS; i++) {
		if (i % 1000 == 0)
			setproctitle("%s @ %d", __func__, i);
		snprintf(file, sizeof(file), "%s/t3.%06d.%03d", dir,
		    getpid(), i);
		if ((fd = open(file, O_RDWR | O_CREAT | O_TRUNC,
		    DEFFILEMODE)) == -1)
			err(1, "open(%s)", file);
		close(fd);
		if (unlink(file) == -1)
			err(1, "unlink(%s)", file);
		usleep(100);
	}
	if (rmdir(dir) == -1)
		err(1, "rmdir(%s)", dir);

	_exit(0);
}

static void
t4(void)
{
	int fd, i;
	char file[128], new[128];

	alarm(TIMEOUT);
	for (i = 0; i < LOOPS / 2; i++) {
		if (i % 1000 == 0)
			setproctitle("%s @ %d", __func__, i);
		snprintf(file, sizeof(file), "t4.%06d.%03d", getpid(), i);
		if ((fd = open(file, O_RDWR | O_CREAT | O_TRUNC,
		    DEFFILEMODE)) == -1)
			err(1, "open(%s)", file);
		close(fd);
		snprintf(new, sizeof(new), "t4.%06d.%03d.new", getpid(), i);
		if (rename(file, new) == -1)
			err(1, "rename(%s, %s)", file, new);
		if (unlink(new) == -1)
			err(1, "unlink(%s)", new);
		usleep(100);
	}
	_exit(0);
}

static void
t5(void)
{
	int fd, i;
	char buf[512], file[128];

	alarm(TIMEOUT);
	memset(buf, 0, sizeof(buf));
	for (i = 0; i < LOOPS; i++) {
		if (i % 1000 == 0)
			setproctitle("%s @ %d", __func__, i);
		snprintf(file, sizeof(file), "t5.%06d.%03d", getpid(), i);
		if ((fd = open(file, O_RDWR | O_CREAT | O_TRUNC,
		    DEFFILEMODE)) == -1)
			err(1, "open(%s)", file);
		if (write(fd, buf, sizeof(buf)) != sizeof(buf))
			err(1, "write(%s)", file);
		close(fd);
		if (unlink(file) == -1)
			err(1, "unlink(%s)", file);
		usleep(100);
	}
	_exit(0);
}

static void
t6(void)
{
	int fd, i;
	char buf[512], file[128];

	alarm(TIMEOUT);
	memset(buf, 0, sizeof(buf));
	for (i = 0; i < LOOPS / 2; i++) {
		if (i % 1000 == 0)
			setproctitle("%s/write @ %d", __func__, i);
		snprintf(file, sizeof(file), "t6.%06d.%03d", getpid(), i);
		if ((fd = open(file, O_RDWR | O_CREAT | O_TRUNC,
		    DEFFILEMODE)) == -1)
			err(1, "open(%s)", file);
		if (write(fd, buf, sizeof(buf)) != sizeof(buf))
			err(1, "write(%s)", file);
		close(fd);
	}
	for (i = 0; i < LOOPS / 2; i++) {
		if (i % 1000 == 0)
			setproctitle("%s/read @ %d", __func__, i);
		snprintf(file, sizeof(file), "t6.%06d.%03d", getpid(), i);
		if ((fd = open(file, O_RDONLY)) == -1)
			err(1, "open(%s)", file);
		if (read(fd, buf, sizeof(buf)) != sizeof(buf))
			err(1, "write(%s)", file);
		close(fd);
		usleep(100);
	}
	for (i = 0; i < LOOPS / 2; i++) {
		snprintf(file, sizeof(file), "t6.%06d.%03d", getpid(), i);
		if (unlink(file) == -1)
			err(1, "unlink(%s)", file);
	}
	_exit(0);
}

static int
test(void)
{
	pid_t pids[TESTS];
	int e, i, status;

	e = 0;
	for (i = 0; i < TESTS; i++)
		if ((pids[i] = fork()) == 0)
			functions[i]();
	for (i = 0; i < TESTS; i++) {
		if (waitpid(pids[i], &status, 0) != pids[i])
			err(1, "waitpid(%d)", pids[i]);
		e += status != 0;
	}

	return (e);
}

int
main(void)
{
	int e;

	functions[0] = &t1;
	functions[1] = &t2;
	functions[2] = &t3;
	functions[3] = &t4;
	functions[4] = &t5;
	functions[5] = &t6;

	e = test();

	return (e);
}
