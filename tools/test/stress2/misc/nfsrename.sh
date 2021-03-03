#!/bin/sh

#
# Copyright (c) 2008-2013 Peter Holm <pho@FreeBSD.org>
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

. ../default.cfg

# Test scenario by jhb@

[ -z "$nfs_export" ] && exit 0
ping -c 2 `echo $nfs_export | sed 's/:.*//'` > /dev/null 2>&1 ||
    exit 0

odir=`pwd`
cd /tmp
sed '1,/^EOF/d' < $odir/$0 > nfsrename.c
mycc -o nfsrename -Wall nfsrename.c
rm -f nfsrename.c
cd $odir

mount | grep "$mntpoint" | grep nfs > /dev/null && umount $mntpoint
mount -t nfs -o tcp -o retrycnt=3 -o intr,soft -o rw $nfs_export $mntpoint

for i in `jot 10`; do
	/tmp/nfsrename  $mntpoint/nfsrename.$i > /dev/null 2>&1 &
	pids="$pids $!"
done
s=0
for i in $pids; do
	wait $i
	[ $? -ne 0 ] && s=1
done
pkill nfsrename
rm -f $mntpoint/nfsrename.*

umount $mntpoint > /dev/null 2>&1
while mount | grep "$mntpoint" | grep -q nfs; do
	umount -f $mntpoint > /dev/null 2>&1
done

rm -f /tmp/nfsrename
exit $s

EOF
/*
 * Try to expose races with doing renames over NFS that require silly
 * renames.  This results in 2 different RENAME RPCs leaving a race
 * window where the file may not exist.  It also appears that FreeBSD
 * with shared lookups in NFS can get confused and possibly reference
 * the sillyrenamed file in lookup but the file is deleted by the time
 * open gets to it.
 */

#include <err.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static char *filename;
static char *dir;

#define RUNTIME 720

static void
usage(void)
{

	fprintf(stderr, "nfsrename: [-n children] file\n");
	exit(1);
}

static void
read_file(void)
{
	FILE *fp;
	char buffer[4096];

	fp = fopen(filename, "r");
	if (fp == NULL) {
		return;
	}
	while (!feof(fp)) {
		if (fread(buffer, sizeof(buffer), 1, fp) < sizeof(buffer))
			break;
	}
	if (ferror(fp))
		warnx("fread encountered an error");
	fclose(fp);
}

static void
write_file(void)
{
	FILE *fp;
	char path[1024];
	int fd;

	snprintf(path, sizeof(path), "%s/nfsrename.XXXXXX", dir);
	fd = mkstemp(path);
	if (fd < 0) {
		warn("mkstemp");
		return;
	}

	fp = fdopen(fd, "w");
	if (fp == NULL) {
		warn("fdopen:writer");
		close(fd);
		unlink(path);
	}

	fprintf(fp, "blah blah blah garbage %ld\n", random());
	fclose(fp);
	if (rename(path, filename) < 0) {
		warn("rename");
		unlink(path);
	}
}

static void
random_sleep(int base, int slop)
{
	long val;

	val = random() % slop;
	usleep(base + val);
}

static void
child(void)
{
	time_t start;

	start = time(NULL);
	for (;;) {
		random_sleep(500, 50);
		read_file();
		if (time(NULL) - start > RUNTIME)
			errx(1, "Timed out");
	}
	exit(0);
}

int
main(int ac, char **av)
{
	time_t start;
	long i, nchild;
	char *cp;
	int ch;

	nchild = 1;
	while ((ch = getopt(ac, av, "n:")) != -1) {
		switch (ch) {
		case 'n':
			nchild = strtol(optarg, &cp, 0);
			if (*cp != '\0')
				errx(1, "Invalid count %s", optarg);
			break;
		case '?':
		default:
			usage();
		}
	}
	ac -= optind;
	av += optind;

	if (ac == 0)
		errx(1, "Missing filename");
	else if (ac > 1)
		errx(1, "Extra arguments");

	filename = av[0];
	dir = dirname(filename);
	srandomdev();
	write_file();

	for (i = 0; i < nchild; i++) {
		switch (fork()) {
		case 0:
			child();
		case -1:
			err(1, "fork");
		}
	}

	start = time(NULL);
	for (i = 0; i < 10000; i++) {
		random_sleep(1500, 1000);
		write_file();
		if (time(NULL) - start > RUNTIME)
			errx(1, "Timed out");
	}

	return (0);
}
