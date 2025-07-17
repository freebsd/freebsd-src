#!/bin/sh

#
# Copyright (c) 2016 EMC Corp.
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

# Demonstrate VM leakage. Not seen on FreeBSD HEAD.

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > thr3.c
mycc -o thr3 -Wall -Wextra -O0 thr3.c -lpthread || exit 1
rm -f thr3.c

./thr3 &
pid=$!

log=/tmp/$0.$$
trap "rm -f $log" EXIT INT
r=`ps -Ovsz -p $pid | head -1`
echo "        $r" > $log
export max=0
export n=0
while true; do
	sleep 30
	vsz=`ps -Ovsz -p $pid | tail -1 | awk '{print $2}'`
	[ -z "$vsz" -o "$vsz" = VSZ ] && break
	if [ $vsz -gt $max ]; then
		max=$vsz
		r=`ps -Ovsz -p $pid | tail -1`
		echo "`date '+%T'` $r"
		n=$((n + 1))
	fi
done >> $log 2>&1
[ $n -gt 1 ] && cat $log
wait
rm -f thr3
[ $n -gt 1 ] && exit 1 || exit 0
EOF
#include <sys/types.h>
#include <sys/time.h>

#include <err.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define NTHREADS 256
#define RUNTIME (3 * 60)

static void *
thr_routine(void *arg __unused)
{
	getuid();
	return (NULL);
}

int
main(void)
{
	pthread_t threads[NTHREADS];
	time_t start;
	int i, r;

	start = time(NULL);
	while (time(NULL) - start < RUNTIME) {
		for (i = 0; i < NTHREADS; i++)
			if ((r = pthread_create(&threads[i], NULL,
			    thr_routine, NULL)) != 0)
				errc(1, r, "pthread_create()");

		for (i = 0; i < NTHREADS; i++)
			if ((r = pthread_join(threads[i], NULL)) != 0)
				errc(1, r, "pthread_join(%d)", i);
	}

	return (0);
}
