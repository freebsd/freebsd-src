#!/bin/sh

#
# Copyright (c) 2011 Peter Holm <pho@FreeBSD.org>
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

# Scenario from kern/159971
# bstg0003.c by Kirk Russell <kirk ba23 org>

# panic: ino 0xc84c9b00(0x3C8209) 65554, 32780 != 65570
# https://people.freebsd.org/~pho/stress/log/suj23.txt

# panic: first_unlinked_inodedep: prev != next. inodedep = 0xcadf9e00
# https://people.freebsd.org/~pho/stress/log/jeff091.txt

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > suj23.c
mycc -o suj23 -Wall -Wextra -O2 suj23.c
rm -f suj23.c

mount | grep "on $mntpoint " | grep -q md$mdstart && umount $mntpoint
[ -c /dev/md$mdstart ] && mdconfig -d -u $mdstart

mdconfig -a -t swap -s 1g -u $mdstart
newfs -j md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
chmod 777 $mntpoint

su $testuser -c '/tmp/suj23'

while mount | grep -q "on $mntpoint "; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
rm -f /tmp/suj23
exit 0
EOF
/*
 * Copyright 2011 Kirk J. Russell
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#include <unistd.h>
#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/wait.h>

#define RUNTIME 600

static char *bstg_pathstore[] = {
	"/mnt/111/z",
	"/mnt/111/aaaa",
	"/mnt/111/bbbbb",
	"/mnt/111/ccccc",
	"/mnt/111/d",
	"/mnt/111/e",
	"/mnt/111/ffffff.fff.f",
	"/mnt/111/gggggggggggg",
	"/mnt/111/hhhh",
	"/mnt/111/iiiii.ii",
	"/mnt/111/jjjj.jj.jjjjjjjj",
	"/mnt/111/kkkk.kkkkkkkk",
	"/mnt/111/lllll",
	"/mnt/222/z",
	"/mnt/222/aaaa",
	"/mnt/222/bbbbb",
	"/mnt/222/ccccc",
	"/mnt/222/d",
	"/mnt/222/e",
	"/mnt/222/ffffff.fff.f",
	"/mnt/222/gggggggggggg",
	"/mnt/222/hhhh",
	"/mnt/222/iiiii.ii",
	"/mnt/222/jjjj.jj.jjjjjjjj",
	"/mnt/222/kkkk.kkkkkkkk",
	"/mnt/222/lllll",
	"/mnt/333/z",
	"/mnt/333/aaaa",
	"/mnt/333/bbbbb",
	"/mnt/333/ccccc",
	"/mnt/333/d",
	"/mnt/333/e",
	"/mnt/333/ffffff.fff.f",
	"/mnt/333/gggggggggggg",
	"/mnt/333/hhhh",
	"/mnt/333/iiiii.ii",
	"/mnt/333/jjjj.jj.jjjjjjjj",
	"/mnt/333/kkkk.kkkkkkkk",
	"/mnt/333/lllll",
	"/mnt/444/z",
	"/mnt/444/aaaa",
	"/mnt/444/bbbbb",
	"/mnt/444/ccccc",
	"/mnt/444/d",
	"/mnt/444/e",
	"/mnt/444/ffffff.fff.f",
	"/mnt/444/gggggggggggg",
	"/mnt/444/hhhh",
	"/mnt/444/iiiii.ii",
	"/mnt/444/jjjj.jj.jjjjjjjj",
	"/mnt/444/kkkk.kkkkkkkk",
	"/mnt/444/lllll",
	"/mnt/555/z",
	"/mnt/555/aaaa",
	"/mnt/555/bbbbb",
	"/mnt/555/ccccc",
	"/mnt/555/d",
	"/mnt/555/e",
	"/mnt/555/ffffff.fff.f",
	"/mnt/555/gggggggggggg",
	"/mnt/555/hhhh",
	"/mnt/555/iiiii.ii",
	"/mnt/555/jjjj.jj.jjjjjjjj",
	"/mnt/555/kkkk.kkkkkkkk",
	"/mnt/555/lllll",
	"/mnt/666/z",
	"/mnt/666/aaaa",
	"/mnt/666/bbbbb",
	"/mnt/666/ccccc",
	"/mnt/666/d",
	"/mnt/666/e",
	"/mnt/666/ffffff.fff.f",
	"/mnt/666/gggggggggggg",
	"/mnt/666/hhhh",
	"/mnt/666/iiiii.ii",
	"/mnt/666/jjjj.jj.jjjjjjjj",
	"/mnt/666/kkkk.kkkkkkkk",
	"/mnt/666/lllll",
	"/mnt/777/z",
	"/mnt/777/aaaa",
	"/mnt/777/bbbbb",
	"/mnt/777/ccccc",
	"/mnt/777/d",
	"/mnt/777/e",
	"/mnt/777/ffffff.fff.f",
	"/mnt/777/gggggggggggg",
	"/mnt/777/hhhh",
	"/mnt/777/iiiii.ii",
	"/mnt/777/jjjj.jj.jjjjjjjj",
	"/mnt/777/kkkk.kkkkkkkk",
	"/mnt/777/lllll",
	"/mnt/888/z",
	"/mnt/888/aaaa",
	"/mnt/888/bbbbb",
	"/mnt/888/ccccc",
	"/mnt/888/d",
	"/mnt/888/e",
	"/mnt/888/ffffff.fff.f",
	"/mnt/888/gggggggggggg",
	"/mnt/888/hhhh",
	"/mnt/888/iiiii.ii",
	"/mnt/888/jjjj.jj.jjjjjjjj",
	"/mnt/888/kkkk.kkkkkkkk",
	"/mnt/888/lllll",
	"/mnt/999/z",
	"/mnt/999/aaaa",
	"/mnt/999/bbbbb",
	"/mnt/999/ccccc",
	"/mnt/999/d",
	"/mnt/999/e",
	"/mnt/999/ffffff.fff.f",
	"/mnt/999/gggggggggggg",
	"/mnt/999/hhhh",
	"/mnt/999/iiiii.ii",
	"/mnt/999/jjjj.jj.jjjjjjjj",
	"/mnt/999/kkkk.kkkkkkkk",
	"/mnt/999/lllll",
	"/mnt/aaa/z",
	"/mnt/aaa/aaaa",
	"/mnt/aaa/bbbbb",
	"/mnt/aaa/ccccc",
	"/mnt/aaa/d",
	"/mnt/aaa/e",
	"/mnt/aaa/ffffff.fff.f",
	"/mnt/aaa/gggggggggggg",
	"/mnt/aaa/hhhh",
	"/mnt/aaa/iiiii.ii",
	"/mnt/aaa/jjjj.jj.jjjjjjjj",
	"/mnt/aaa/kkkk.kkkkkkkk",
	"/mnt/aaa/lllll",
	"/mnt/bbb/z",
	"/mnt/bbb/aaaa",
	"/mnt/bbb/bbbbb",
	"/mnt/bbb/ccccc",
	"/mnt/bbb/d",
	"/mnt/bbb/e",
	"/mnt/bbb/ffffff.fff.f",
	"/mnt/bbb/gggggggggggg",
	"/mnt/bbb/hhhh",
	"/mnt/bbb/iiiii.ii",
	"/mnt/bbb/jjjj.jj.jjjjjjjj",
	"/mnt/bbb/kkkk.kkkkkkkk",
	"/mnt/bbb/lllll",
	"/mnt/ccc/z",
	"/mnt/ccc/aaaa",
	"/mnt/ccc/bbbbb",
	"/mnt/ccc/ccccc",
	"/mnt/ccc/d",
	"/mnt/ccc/e",
	"/mnt/ccc/ffffff.fff.f",
	"/mnt/ccc/gggggggggggg",
	"/mnt/ccc/hhhh",
	"/mnt/ccc/iiiii.ii",
	"/mnt/ccc/jjjj.jj.jjjjjjjj",
	"/mnt/ccc/kkkk.kkkkkkkk",
	"/mnt/ccc/lllll",
	"/mnt/ddd/z",
	"/mnt/ddd/aaaa",
	"/mnt/ddd/bbbbb",
	"/mnt/ddd/ccccc",
	"/mnt/ddd/d",
	"/mnt/ddd/e",
	"/mnt/ddd/ffffff.fff.f",
	"/mnt/ddd/gggggggggggg",
	"/mnt/ddd/hhhh",
	"/mnt/ddd/iiiii.ii",
	"/mnt/ddd/jjjj.jj.jjjjjjjj",
	"/mnt/ddd/kkkk.kkkkkkkk",
	"/mnt/ddd/lllll",
	"/mnt/eee/z",
	"/mnt/eee/aaaa",
	"/mnt/eee/bbbbb",
	"/mnt/eee/ccccc",
	"/mnt/eee/d",
	"/mnt/eee/e",
	"/mnt/eee/ffffff.fff.f",
	"/mnt/eee/gggggggggggg",
	"/mnt/eee/hhhh",
	"/mnt/eee/iiiii.ii",
	"/mnt/eee/jjjj.jj.jjjjjjjj",
	"/mnt/eee/kkkk.kkkkkkkk",
	"/mnt/eee/lllll",
	"/mnt/fff/z",
	"/mnt/fff/aaaa",
	"/mnt/fff/bbbbb",
	"/mnt/fff/ccccc",
	"/mnt/fff/d",
	"/mnt/fff/e",
	"/mnt/fff/ffffff.fff.f",
	"/mnt/fff/gggggggggggg",
	"/mnt/fff/hhhh",
	"/mnt/fff/iiiii.ii",
	"/mnt/fff/jjjj.jj.jjjjjjjj",
	"/mnt/fff/kkkk.kkkkkkkk",
	"/mnt/fff/lllll"
};

char *
bstg_pathstore_get()
{
	return bstg_pathstore[rand() %
		     ((sizeof(bstg_pathstore) / sizeof(bstg_pathstore[0])))];
}

void
dogcore()
{
	pid_t sleepchild, gcorechild;
	extern char **environ;

	/* create a child for the gcore target */
	if ((sleepchild = fork()) == 0) {
		sleep(30);
		_exit(1);
	} else if (sleepchild > 0) {
		char *token[] = {NULL, NULL, NULL, NULL, NULL};
		char buf[64];
		int status;

		/* use the first process as the target */
		snprintf(buf, sizeof(buf), "%d", sleepchild);
		token[0] = "gcore";
		token[1] = "-c";
		token[2] = bstg_pathstore_get();
		token[3] = buf;
		assert(token[4] == NULL);

		if ((gcorechild = fork()) > 0) {
			waitpid(gcorechild, &status, 0);
		} else if (gcorechild == 0) {
			execve("/usr/bin/gcore", token, environ);
			_exit(1);
		}
		kill(sleepchild, SIGKILL);
		waitpid(sleepchild, &status, 0);
	}
}

void
dowrite()
{
	struct iovec data[] = {
		{"12", 2},
		{NULL, 0},
		{"12345678", 8},
	};
	static int fd = -1;

	if (fd == -1) {
		/* keep existing file open during life of this process */
		fd = open(bstg_pathstore_get(), O_RDWR | O_NONBLOCK | O_NOCTTY);
	}
	data[1].iov_base = bstg_pathstore_get();
	data[1].iov_len = strlen((char *)data[1].iov_base);
	ftruncate(fd, 0);
	pwritev(fd, data, 3, 0);
}

void
dounlink()
{
	unlink(bstg_pathstore_get());
}

void
dolink()
{
	link(bstg_pathstore_get(), bstg_pathstore_get());
}

void
domkdir()
{
	char **pdir;
	static char *bstg_dirs[] = {
		"/mnt/111", "/mnt/222", "/mnt/333", "/mnt/444",
		"/mnt/555", "/mnt/666", "/mnt/777", "/mnt/888",
		"/mnt/999", "/mnt/aaa", "/mnt/bbb", "/mnt/ccc",
		"/mnt/ddd", "/mnt/eee", "/mnt/fff", NULL
	};

	for (pdir = bstg_dirs; *pdir; pdir++) {
		if (mkdir(*pdir, 0777) == -1)
			err(1, "mkdir(%s)", *pdir);
	}
}

void
dosync()
{
	sync();
}

int
main()
{
	time_t start;
	unsigned x;
	int i, status;
	void (*funcs[]) () = {
		dogcore,
		dowrite,
		dounlink,
		dolink,
		dowrite,
		dounlink,
		dolink,
		dowrite,
		dosync,
		dowrite,
		dounlink,
		dolink,
		dowrite,
		dounlink,
		dolink,
		dowrite,
	};

	/* we only can domkdir() once at startup */
	domkdir();

	/* create 128 children that loop forever running 4 operations */
	dosync();
	for (x = 0; x < 128; x++) {
		if (fork() == 0) {
			/* give child a new seed for the pathname selection */
			srand(x);

			start = time(NULL);
			for (i = 0; i < 1000; i++) {
				/* each child will start looping at different
				 * function */
				(*funcs[x++ % 16]) ();
				if (time(NULL) - start > RUNTIME)
					break;
			}
			/* we never expect this code to run */
			_exit(1);
		}
	}

	/* block forever for all our children */
	while (wait(&status) > 0);
	return 0;
}
