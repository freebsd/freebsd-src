#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2021 Mark Johnston <markj@freebsd.org>
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

# 'panic: vm_fault_hold: fault on nofault entry, addr: 0x33522000' seen.
# Fixed by 351262

cat > /tmp/midi.c <<EOF
#include <err.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <unistd.h>

#define NTHREADS	16

static _Atomic(int) threads;
static int fd;

static void *
t(void *data __unused)
{
	char buf[4096];
	ssize_t n;
	off_t off;

	(void)atomic_fetch_add(&threads, 1);
	while (atomic_load(&threads) != NTHREADS)
		;

	for (;;) {
		arc4random_buf(&off, sizeof(off));
		if ((n = pread(fd, buf, sizeof(buf), off)) >= 0)
			write(STDOUT_FILENO, buf, n);
	}

	return (NULL);
}

int
main(void)
{
	pthread_t tid[NTHREADS];
	int error, i;

	fd = open("/dev/midistat", O_RDONLY);
	if (fd < 0)
		err(1, "open");

	for (i = 0; i < NTHREADS; i++)
		if ((error = pthread_create(&tid[i], NULL, t, NULL)) != 0)
			errc(1, error, "pthread_create");
	for (i = 0; i < NTHREADS; i++)
		if ((error = pthread_join(tid[i], NULL)) != 0)
			errc(1, error, "pthread_join");

	return (0);
}
EOF
cc -o /tmp/midi -Wall -Wextra -O2 /tmp/midi.c -lpthread

start=`date +%s`
while [ $((`date +%s` - start)) -lt 120 ]; do
	timeout 10 /tmp/midi | strings | head -20
done

rm -f /tmp/midi /tmp/midi.c /tmp/midi.core
exit 0
