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

# "panic: vm_page_free_prep: freeing mapped page 0x657936c" seen.
# https://people.freebsd.org/~pho/stress/log/sem_wait.txt
# Fixed by r350005

. ../default.cfg

cat > /tmp/sem_wait.c <<EOF
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static sem_t *semaphore;

static void *
test(void) {

	setproctitle("%s", __func__);
	alarm(300);
	for (;;) {
		if (sem_wait(semaphore) == -1)
			err(1, "sem_wait");
		if (sem_post(semaphore) == -1)
			err(1, "sem_post");
	}
}

int
main(void) {
	pid_t pid;
	size_t len;
	time_t start;

	setproctitle("%s", __func__);
	alarm(300);
	len = PAGE_SIZE;
	if ((semaphore = mmap(NULL, len, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");

	// initialize semaphore and set value to 1
	if (sem_init(semaphore, 1, 1) == -1)
		err(1, "sem_init");

	if ((pid = fork()) == 0)
		test();
	if (pid == -1)
		err(1, "fork()");

	usleep(50);
	alarm(300);
	start = time(NULL);
	while (time(NULL) - start < 60) {
		if (sem_wait(semaphore) == -1)
			err(1, "sem_wait");
		if (sem_post(semaphore) == -1)
			err(1, "sem_post");
	}
	kill(pid, SIGHUP);
	if (waitpid(pid, NULL, 0) != pid)
		err(1, "waitpid()");

	return (0);
}
EOF
mycc -o /tmp/sem_wait -Wall -Wextra -O2 /tmp/sem_wait.c || exit 1
timeout 6m /tmp/sem_wait; s=$?
[ $s -eq 124 ] && echo "Timed out"
rm -f /tmp/sem_wait /tmp/sem_wait.c
exit $s
