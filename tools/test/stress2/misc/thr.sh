#!/bin/sh

#
# Copyright (c) 2013 Peter Holm <pho@FreeBSD.org>
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

# Used for PAE test

. ../default.cfg

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/thr.c
mycc -o thr -Wall -Wextra thr.c -pthread || exit 1
rm -f thr.c

touch thr.continue
max=0
s=0
start=`date '+%s'`
while [ -r thr.continue ]; do
	./thr &
	sleep 2
	n=`ps -xH | egrep "thr$" | grep -v grep | wc -l | sed 's/ //g'`
	[ $max -lt $n ] && max=$n
	[ $max -gt 5000 ] && break
	[ `date '+%s'` -gt $((start + 120)) ] && break
done
killall thr
wait
[ $max -lt 2000 ] &&
    { s=1; printf "FAIL\nMax threads created is %d\n" $max; }

rm -f /tmp/thr /tmp/thr.continue
exit $s

EOF

#include <err.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define NTHREADS 1499
static	pthread_t threads[NTHREADS];

void *
thr_routine(void *arg __unused)
{
	sleep(120);
	return (0);
}

int
main(void)
{
	int i, r;

	for (i = 0; i < NTHREADS; i++) {
		if ((r = pthread_create(&threads[i], NULL, thr_routine, 0)) != 0) {
			unlink("thr.continue");
			sleep(120);
			errc(1, r, "pthread_create()");
		}
	}

	for (i = 0; i < NTHREADS; i++)
		if ((r = pthread_join(threads[i], NULL)) != 0)
			errc(1, r, "pthread_join(%d)", i);

	return (0);
}
