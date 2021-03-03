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

# Demonstrate resource starvation using msync(2).

. ../default.cfg

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/db.c
mycc -o db -Wall -Wextra -O0 -g db.c -lpthread || exit 1
rm -f db.c
cd $odir

dd if=/dev/zero of=$diskimage bs=1m count=10 status=none

/tmp/db $diskimage &

start=`date '+%s'`
ls -l $diskimage > /dev/null # Will wait for more than 90 seconds
[ `date '+%s'` -gt $((start + 90)) ] && fail="yes"
wait
e=$((`date +%s` - start))
[ $fail ] &&
    echo "Time for a ls is $((e / 60)) minutes $((e % 60)) seconds"
rm -f /tmp/db $diskimage
#[ $fail ] && exit 1 || exit 0 # Known issue
exit 0

EOF
#include <sys/types.h>
#include <sys/param.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <pthread.h>
#ifdef __FreeBSD__
#include <pthread_np.h>
#define	__NP__
#endif
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
size_t len;
void *p;
int wthreads;

#define BZ 128		/* buffer size */
#define RUNTIME 180	/* runtime for test */
#define RTHREADS 64	/* reader threads */
#define WTHREADS 64	/* writer threads */

void *
wt(void *arg __unused)
{
	time_t start;
	int64_t pos;
	void *c;
	int r;
	char buf[BZ];

#ifdef __NP__
	pthread_set_name_np(pthread_self(), __func__);
#endif
	if ((r = pthread_mutex_lock(&mutex)) != 0)
		errc(1, r, "pthread_mutex_lock");
	wthreads++;
	if ((r = pthread_mutex_unlock(&mutex)) != 0)
		errc(1, r, "pthread_mutex_unlock");

	start = time(NULL);
	while (time(NULL) - start < RUNTIME) {
		pos = arc4random() % (len / BZ);
		pos = pos * BZ;
		c = p + pos;
		bcopy(buf, c, BZ);
		c = (void *)trunc_page((unsigned long)c);
		if (msync((void *)c, round_page(BZ), MS_SYNC) == -1)
			err(1, "msync(%p)", c);
		usleep(10000 + arc4random() % 1000);
	}

	if ((r = pthread_mutex_lock(&mutex)) != 0)
		errc(1, r, "pthread_mutex_lock");
	wthreads--;
	if ((r = pthread_mutex_unlock(&mutex)) != 0)
		errc(1, r, "pthread_mutex_unlock");

	return (NULL);
}

void *
rt(void *arg __unused)
{
	int64_t pos;
	char buf[BZ], *c;

#ifdef __NP__
	pthread_set_name_np(pthread_self(), __func__);
#endif
	c = p;
	do {
		pos = arc4random() % (len / BZ);
		pos = pos * BZ;
		bcopy(&c[pos], buf, BZ);
		usleep(10000 + arc4random() % 1000);
	} while (wthreads != 0);

	return (NULL);
}

int
main(int argc, char *argv[])
{
	pthread_t cp[RTHREADS + WTHREADS];
	struct stat st;
	int fd, i, j, rc;

	if (argc != 2)
		errx(1, "Usage: %s <file>", argv[0]);
	if ((fd = open(argv[1], O_RDWR)) == -1)
		err(1, "open %s", argv[1]);
	if (fstat(fd, &st) == -1)
		err(1, "stat");
	len = round_page(st.st_size);
	p = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if ((void *)p == MAP_FAILED)
		err(1, "mmap");

	i = 0;
	for (j = 0; j < WTHREADS; j++) {
		if ((rc = pthread_create(&cp[i++], NULL, wt, NULL)) != 0)
			errc(1, rc, "pthread_create()");
	}
	usleep(100);
	for (j = 0; j < RTHREADS; j++) {
		if ((rc = pthread_create(&cp[i++], NULL, rt, NULL)) != 0)
			errc(1, rc, "pthread_create()");
	}

	for (j = 0; j < RTHREADS + WTHREADS; j++)
		pthread_join(cp[--i], NULL);

	return (0);
}
