#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2019 Dell EMC Isilon
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

# No problems seen.

cat > /tmp/sem_post.c <<EOF
#include <err.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static sem_t semaphore;

static void *
threadfunc(void *data __unused) {

	for (;;) {
		usleep(arc4random() % 100);
		if (sem_wait(&semaphore) == -1)
			err(1, "sem_wait");
		usleep(arc4random() % 100);
		if (arc4random() % 100 < 3)
			sleep(1);
		if (sem_post(&semaphore) == -1)
			err(1, "sem_post");
	}

	return (NULL);
}

int
main(void) {
	pthread_t mythread;
	time_t start;
	int r;

	// initialize semaphore and set value to 1
	if (sem_init(&semaphore, 0, 1) == -1)
		err(1, "sem_init");

	r = pthread_create(&mythread, NULL, threadfunc, NULL);
	if (r != 0)
		errc(1, r, "pthread_create");

	usleep(50);
	alarm(300);
	start = time(NULL);
	while (time(NULL) - start < 120) {
		usleep(arc4random() % 100);
		if (sem_wait(&semaphore) == -1)
			err(1, "sem_wait");
		usleep(arc4random() % 100);
		if (arc4random() % 100 < 3)
			sleep(1);
		if (sem_post(&semaphore) == -1)
			err(1, "sem_post");
	}
	if ((r = pthread_kill(mythread, SIGINT)) != 0)
		errc(1, r, "pthread_kill");
	if ((r = pthread_join(mythread, NULL)) != 0)
		errc(1, r, "pthread_join");

	return (0);
}
EOF
cc -o /tmp/sem_post -Wall -Wextra -O2 /tmp/sem_post.c -lpthread || exit 1
(cd ../testcases/swap; ./swap -t 3m -i 30 -h -l 100) &
sleep 20
for i in `jot 128`; do
	/tmp/sem_post > /dev/null &
done
while pgrep -q sem_post; do sleep .5; done
while pkill swap; do :; done
wait
rm -f /tmp/sem_post /tmp/sem_post.c
exit 0
