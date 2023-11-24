#!/bin/sh

#
# Copyright (c) 2012 Peter Holm <pho@FreeBSD.org>
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

# Test of access to /dev/sndstat

# Permanent "/dev/sndstat: Device busy" seen.
# "panic: sbuf_put_byte called with finished or corrupt sbuf" seen.
# Fixed in r234932

. ../default.cfg

[ -r /dev/sndstat ] || exit 0

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > sndstat.c
mycc -o sndstat -Wall -Wextra sndstat.c -lpthread
rm -f sndstat.c

/tmp/sndstat > /dev/null

rm -f /tmp/sndstat
exit 0
EOF
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <dirent.h>
#include <err.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/wait.h>
#include <unistd.h>

int fd;
char path[] = "/dev/sndstat";

void *
test1(void *arg __unused)
{

        int sfd, i, n;
	char buf[512];

	n = 0;
        for (i = 0; i < 5; i++) {
                if ((sfd = open(path, O_RDONLY)) == -1)
                        continue;

                read(sfd, buf, sizeof(buf));
		fprintf(stdout, "%s\n", buf);

		n++;
                close(sfd);
        }
	if (n == 0) {
                if ((sfd = open(path, O_RDONLY)) == -1)
			warn("FAIL open(%s)", path);
		if (sfd > 0)
			close(sfd);
	}

        return (0);
}

void *
test2(void *arg __unused)
{
	char buf[512];

	bzero(buf, sizeof(buf));
	if (read(fd, buf, sizeof(buf)) != -1)
		fprintf(stdout, "%s\n", buf);
	else
		warn("read()");

        return (0);
}

int
main(void)
{
        pthread_t rp[10];
        int e, i, j;

	/* Parallel open test */
        for (i = 0; i < 10; i++) {
                if ((e = pthread_create(&rp[i], NULL, test1, NULL)) != 0)
                        errc(1, e, "pthread_create");
        }
        for (i = 0; i < 10; i++)
                pthread_join(rp[i], NULL);

	/* Parallel read test */
	for (i = 0; i < 10; i++) {
		if ((fd = open(path, O_RDONLY)) == -1) {
			warn("open()");
			continue;
		}
		for (j = 0; j < 4; j++)
			if ((e = pthread_create(&rp[j], NULL, test2, NULL)) != 0)
				errc(1, e, "pthread_create");
		for (j = 0; j < 4; j++)
			pthread_join(rp[j], NULL);

		close(fd);
	}

        return (0);
}
