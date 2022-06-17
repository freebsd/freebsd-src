#!/bin/sh

#
# Copyright (c) 2015 EMC Corp.
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

# Show invalid fts_info value:

# FAULT
# -rw-------  1 root  wheel  - 4 13 jan 09:25 ./lockf.0.3676
# fts_path: ./lockf.0.3676
# fts_info: 13 FTS_SLNONE
# fts_errno: 0 No error: 0

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

cat > /tmp/fts2.c <<EOF
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define LOOPS 1
#define PARALLEL 7

pid_t pid;
time_t start;
int fd;
char file[128];

char *txt[] = {
	"NULL",
	"FTS_D",
	"FTS_DC",
	"FTS_DEFAULT",
	"FTS_DNR",
	"FTS_DOT",
	"FTS_DP",
	"FTS_ERR",
	"FTS_F",
	"FTS_INIT",
	"FTS_NS",
	"FTS_NSOK",
	"FTS_SL",
	"FTS_SLNONE",
	"FTS_W",
	"15",
	"16",
	"17",
};

int
get(void) {
	int sem;
	if (lockf(fd, F_LOCK, 0) == -1)
		err(1, "lockf(%s, F_LOCK)", file);
	if (read(fd, &sem, sizeof(sem)) != sizeof(sem))
		err(1, "get: read(%d)", fd);
	if (lseek(fd, 0, SEEK_SET) == -1)
		err(1, "lseek");
	if (lockf(fd, F_ULOCK, 0) == -1)
		err(1, "lockf(%s, F_ULOCK)", file);
	return (sem);
}

void
incr(void) {
	int sem;
	if (lockf(fd, F_LOCK, 0) == -1)
		err(1, "lockf(%s, F_LOCK)", file);
	if (read(fd, &sem, sizeof(sem)) != sizeof(sem))
		err(1, "incr: read(%d)", fd);
	if (lseek(fd, 0, SEEK_SET) == -1)
		err(1, "lseek");
	sem++;
	if (write(fd, &sem, sizeof(sem)) != sizeof(sem))
		err(1, "incr: read");
	if (lseek(fd, 0, SEEK_SET) == -1)
		err(1, "lseek");
	if (lockf(fd, F_ULOCK, 0) == -1)
		err(1, "lockf(%s, F_ULOCK)", file);
}

void
tlockf(void)
{
	int i;
	int sem = 0;

	usleep(arc4random() % 10000);
	sprintf(file, "lockf.0.%d", getpid());
	if ((fd = open(file,O_CREAT | O_TRUNC | O_RDWR, 0600)) == -1)
		err(1, "creat(%s)", file);
	if (write(fd, &sem, sizeof(sem)) != sizeof(sem))
		err(1, "write");
	if (lseek(fd, 0, SEEK_SET) == -1)
		err(1, "lseek");

	pid = fork();
	if (pid == -1) {
		perror("fork");
		exit(2);
	}

	if (pid == 0) {	/* child */
		for (i = 0; i < 100; i++) {
			while ((get() & 1) == 0)
				;
			incr();
		}
		exit(0);
	} else {	/* parent */
		for (i = 0; i < 100; i++) {
			while ((get() & 1) == 1)
				;
			incr();
		}
	}
	close(fd);
	waitpid(pid, &i, 0);
	unlink(file);
}

void
tmkdir(void)
{
	pid_t pid;
	int i, j;
	char name[80];

	setproctitle(__func__);
	usleep(arc4random() % 10000);
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
}

void
tfts(void)
{
	FTS *fts;
	FTSENT *p;
	int ftsoptions, i;
	char *args[2];
	char help[80];

	usleep(arc4random() % 10000);
	ftsoptions = FTS_LOGICAL;
	args[0] = ".";
	args[1] = 0;

	for (i = 0; i < 10; i++) {
		if ((fts = fts_open(args, ftsoptions, NULL)) == NULL)
			err(1, "fts_open");

		while ((p = fts_read(fts)) != NULL) {
			if (p->fts_info == FTS_D ||   /* preorder directory   */
			    p->fts_info == FTS_DNR || /* unreadable directory */
			    p->fts_info == FTS_DOT || /* dot or dot-dot       */
			    p->fts_info == FTS_DP ||  /* postorder directory  */
			    p->fts_info == FTS_F ||   /* regular file         */
			    p->fts_info == FTS_NS)    /* stat(2) failed       */
				continue;
			fprintf(stderr, "FAULT\n");
			sprintf(help, "ls -lo %s", p->fts_path);
			system(help);
			fprintf(stderr, "fts_path: %s\n", p->fts_path);
			fprintf(stderr, "fts_info: %d %s\n", p->fts_info,
			    txt[p->fts_info]);
			fprintf(stderr, "fts_errno: %d %s\n", p->fts_errno,
			   strerror(p->fts_errno));
		}

		if (errno != 0 && errno != ENOENT)
			err(1, "fts_read");
		if (fts_close(fts) == -1)
			err(1, "fts_close()");
	}
}

void
test(void)
{

	start = time(NULL);
	if (fork() == 0) {
		while (time(NULL) - start < 60)
			tmkdir();
		_exit(0);
	}
	if (fork() == 0) {
		while (time(NULL) - start < 60)
			tlockf();
		_exit(0);
	}
	if (fork() == 0) {
		while (time(NULL) - start < 60)
			tfts();
		_exit(0);
	}

	wait(NULL);
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
mycc -o /tmp/fts2 -Wall -Wextra -O0 -g /tmp/fts2.c || exit 1

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] && mdconfig -d -u $mdstart

mdconfig -a -t swap -s 1g -u $mdstart || exit 1
newfs md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint

(cd $mntpoint; /tmp/fts2)

while mount | grep $mntpoint | grep -q /dev/md; do
        umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
rm /tmp/fts2 /tmp/fts2.c
