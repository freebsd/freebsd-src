/*-
 * Copyright (c) 2008 Peter Holm <pho@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/sysctl.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern char *optarg;
static pid_t pid;
static int edirs, dirs, files, fs;
static int max, rnd;
static char *buffer;
static char here[MAXPATHLEN + 1];

static void
mkDir2(char *path) {
	int fd, i, j, len;
	char newPath[MAXPATHLEN + 1];
	char file[128];

	if (mkdir(path, 0700) == -1)
		err(1, "mkdir(%s)", path);
	if (chdir(path) == -1)
		err(1, "chdir(%s) @ %d", path, __LINE__);

	for (i = 0; i < max; i++) {
		sprintf(newPath,"d%d", i);
		if (mkdir(newPath, 0700) == -1)
			err(1, "mkdir(%s) @ %d", newPath, __LINE__);
		if (chdir(newPath) == -1)
			err(1, "chdir(%s) @ %d", newPath, __LINE__);

		len = fs;
		for (j = 0; j <  files; j++) {
			if (rnd)
				len = arc4random() % fs + 1;
			sprintf(file,"f%05d", j);
			if ((fd = creat(file, 0660)) == -1) {
				if (errno != EINTR) {
					err(1, "%d: creat(%s)", j, file);
					break;
				}
			}
			if (write(fd, buffer, len) != len)
				err(1, "%d: write(%s), %s:%d", j, file, __FILE__, __LINE__);

			if (fd != -1 && close(fd) == -1)
				err(2, "%d: close(%d)", j, j);

		}
		for (j = 0; j <  edirs; j++) {
			sprintf(newPath,"e%d", j);
			if (mkdir(newPath, 0700) == -1)
				err(1, "mkdir(%s) @ %d", newPath, __LINE__);
		}
	}
	chdir(here);
}

static void
mkDir(void) {
	int i;
	char path[MAXPATHLEN + 1];

	for (i = 0; i < dirs; i++) {
		sprintf(path,"fstool.%06d.%d", pid, i);
		mkDir2(path);
	}
}

static void
rmFile(void)
{
	int j;
	char file[128], newPath[128];

	for (j = 0; j <  files; j++) {
		sprintf(file,"f%05d", j);
		(void) unlink(file);
	}
	for (j = 0; j <  edirs; j++) {
		sprintf(newPath,"e%d", j);
		if (rmdir(newPath) == -1)
			err(1, "rmdir(%s) @ %d", newPath, __LINE__);
	}
}

static void
rmDir2(char *path) {
	int i, j;
	char newPath[10];

	if (chdir(path) == -1)
		err(1, "chdir(%s)", path);

	for (i = 0; i < max; i++) {
		sprintf(newPath,"d%d", i);
		if (chdir(newPath) == -1)
			err(1, "chdir(%s)", newPath);
	}
	for (j = 0; j < max; j++) {
		rmFile();
		if (chdir ("..") == -1)
			err(1, "chdir(\"..\")");
		sprintf(newPath,"d%d", max - j - 1);
		if (rmdir(newPath) == -1)
			err(1, "rmdir(%s) @ %d", newPath, __LINE__);
	}
	if (chdir ("..") == -1)
		err(1, "chdir(\"..\")");
	if (rmdir(path) == -1)
		err(1, "rmdir(%s) @ %d", path, __LINE__);
}

static void
rmDir(void) {
	int i;
	char path[MAXPATHLEN + 1];

	for (i = 0; i < dirs; i++) {
		sprintf(path,"fstool.%06d.%d", pid, i);
		rmDir2(path);
	}
}

int
main(int argc, char **argv)
{
	int c, i, levels, leave, times;
	char ch = 0;

	edirs = 0;
	dirs = 1;
	files = 5;
	fs = 1024;
	leave = 0;
	levels = 1;
	times = 1;

	while ((c = getopt(argc, argv, "ad:e:ln:r:f:s:t:")) != -1)
		switch (c) {
			case 'a':
				rnd = 1;
				break;
			case 'd':
				dirs = atoi(optarg);
				break;
			case 'e':
				edirs = atoi(optarg);
				break;
			case 'f':
				files = atoi(optarg);
				break;
			case 'l':
				leave = 1;
				break;
			case 'n':
				levels = atoi(optarg);
				break;
			case 's':
				sscanf(optarg, "%d%c", &fs, &ch);
				if (ch == 'k' || ch == 'K')
					fs = fs * 1024;
				if (ch == 'm' || ch == 'M')
					fs = fs * 1024 * 1024;
				break;
			case 't':
				times = atoi(optarg);
				break;
			default:
				fprintf(stderr,
				    "Usage: %s [ -a ] "
				    "[ -d <parallel dirs> ] [ -e <dirs> ] "
				    "[ -f <num> ][ -l] [ -n <depth> ] "
				    " [ -s <file size> ]\n", argv[0]);
				printf("   -a: random file size 1-s.\n");
				printf("   -d: Tree width (defaults to 1).\n");
				printf("   -e: Directories at each level.\n");
				printf("   -f: Number of files.\n");
				printf("   -l: Leave the files and dirs.\n");
				printf("   -n: Tree depth.\n");
				printf("   -s: Size of each file.\n");
				printf("   -t: Number of times to repeat\n");
				exit(1);
		}

	if (times != 1)
		leave = 0;
	max = levels;
	pid = getpid();
	if ((buffer = calloc(1, fs)) == NULL)
		err(1, "calloc(%d)", fs);
	if (getcwd(here, sizeof(here)) == NULL)
		err(1, "getcwd()");

	for (i = 0; i < times; i++) {
		mkDir();
		if (leave == 0)
			rmDir();
	}

	return (0);
}
