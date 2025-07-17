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

#include <err.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "stress.h"

#define NTHREADS 256

static volatile int done = 0;

int
setup(int nb __unused)
{
	return (0);
}

void
cleanup(void)
{
}

static void *
thr1(void *arg __unused)
{
	return (0);
}

static void *
thr2(void *arg __unused)
{
	while (done == 0)
		pthread_yield();
	return (0);
}

int
test(void)
{
	pthread_t threads[NTHREADS];
	int i;
	int r;

	for (i = 0; i < NTHREADS; i++)
		if ((r = pthread_create(&threads[i], NULL, thr1, 0)) != 0)
			errc(1, r, "pthread_create()");

	for (i = 0; i < NTHREADS; i++)
		if (pthread_join(threads[i], NULL) != 0)
			errc(1, r, "pthread_join(%d)", i);

	for (i = 0; i < NTHREADS; i++)
		if ((r = pthread_create(&threads[i], NULL, thr2, 0)) != 0)
			errc(1, r, "pthread_create()");
	done = 1;

	for (i = 0; i < NTHREADS; i++)
		if ((r = pthread_join(threads[i], NULL)) != 0)
			errc(1, r, "pthread_join(%d)", i);

	return (0);
}
