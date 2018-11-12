/*-
 * Copyright (c) 2000-2013 Mark R V Murray
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 Build this by going:

cc -g -O0 -pthread -DRANDOM_<alg> -DRANDOM_DEBUG -I../.. -lstdthreads -Wall \
	unit_test.c \
	yarrow.c \
	fortuna.c \
	hash.c \
	../../crypto/rijndael/rijndael-api-fst.c \
	../../crypto/rijndael/rijndael-alg-fst.c \
	../../crypto/sha2/sha2.c \
	-o unit_test
./unit_test

Where <alg> is YARROW or FORTUNA.
*/

#include <sys/types.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <threads.h>
#include <unistd.h>

#include "unit_test.h"

#ifdef RANDOM_YARROW
#include "dev/random/yarrow.h"
#endif
#ifdef RANDOM_FORTUNA
#include "dev/random/fortuna.h"
#endif

#define NUM_THREADS	  3

static volatile int stopseeding = 0;

void
random_adaptor_unblock(void)
{

#if 0
	if (mtx_trylock(&random_reseed_mtx) == thrd_busy)
		printf("Mutex held. Good.\n");
	else {
		printf("Mutex not held. PANIC!!\n");
		thrd_exit(0);
	}
#endif
	printf("random: unblocking device.\n");
}

static int
RunHarvester(void *arg __unused)
{
	int i, r;
	struct harvest_event e;

	for (i = 0; ; i++) {
		if (stopseeding)
			break;
		if (i % 1000 == 0)
			printf("Harvest: %d\n", i);
		r = random()%10;
		e.he_somecounter = i;
		*((uint64_t *)e.he_entropy) = random();
		e.he_size = 8;
		e.he_bits = random()%4;
		e.he_destination = i;
		e.he_source = (i + 3)%7;
		e.he_next = NULL;
#ifdef RANDOM_YARROW
		random_yarrow_process_event(&e);
#endif
#ifdef RANDOM_FORTUNA
		random_fortuna_process_event(&e);
#endif
		usleep(r);
	}

	printf("Thread #0 ends\n");

	thrd_exit(0);

	return (0);
}

static int
WriteCSPRNG(void *threadid)
{
	uint8_t *buf;
	int i;

	printf("Thread #1 starts\n");

	for (i = 0; ; i++) {
		if (stopseeding)
			break;
		buf = malloc(4096);
		if (i % 1000 == 0)
			printf("Thread write 1 - %d\n", i);
		if (buf != NULL) {
#ifdef RANDOM_YARROW
			random_yarrow_write(buf, i);
#endif
#ifdef RANDOM_FORTUNA
			random_fortuna_write(buf, i);
#endif
			free(buf);
		}
		usleep(1000000);
	}

	printf("Thread #1 ends\n");

	thrd_exit(0);

	return (0);
}

static int
ReadCSPRNG(void *threadid)
{
	size_t tid;
	uint8_t *buf;
	int i;

	tid = (size_t)threadid;
	printf("Thread #%zd starts\n", tid);

#ifdef RANDOM_YARROW
	while (!random_yarrow_seeded())
#endif
#ifdef RANDOM_FORTUNA
	while (!random_fortuna_seeded())
#endif
	{
#ifdef RANDOM_YARROW
		random_yarrow_read(NULL, 0);
		random_yarrow_read(NULL, 1);
#endif
#ifdef RANDOM_FORTUNA
		random_fortuna_read(NULL, 0);
		random_fortuna_read(NULL, 1);
#endif
		usleep(100);
	}

	for (i = 0; i < 100000; i++) {
		buf = malloc(i);
		if (i % 1000 == 0)
			printf("Thread read %zd - %d %p\n", tid, i, buf);
		if (buf != NULL) {
#ifdef RANDOM_YARROW
			random_yarrow_read(NULL, 0);
			random_yarrow_read(buf, i);
			random_yarrow_read(NULL, 1);
#endif
#ifdef RANDOM_FORTUNA
			random_fortuna_read(NULL, 0);
			random_fortuna_read(buf, i);
			random_fortuna_read(NULL, 1);
#endif
#if 0
			{
			int j;

			for (j = 0; j < i; j++) {
				printf(" %02X", buf[j]);
				if (j % 32 == 31 || j == i - 1)
					printf("\n");
			}
			}
#endif
			free(buf);
		}
		usleep(100);
	}

	printf("Thread #%zd ends\n", tid);

	thrd_exit(0);

	return (0);
}

int
main(int argc, char *argv[])
{
	thrd_t threads[NUM_THREADS];
	int rc;
	long t;

#ifdef RANDOM_YARROW
	random_yarrow_init_alg();
#endif
#ifdef RANDOM_FORTUNA
	random_fortuna_init_alg();
#endif

	for (t = 0; t < NUM_THREADS; t++) {
		printf("In main: creating thread %ld\n", t);
		rc = thrd_create(&threads[t], (t == 0 ? RunHarvester : (t == 1 ? WriteCSPRNG : ReadCSPRNG)), t);
		if (rc != thrd_success) {
			printf("ERROR; return code from thrd_create() is %d\n", rc);
			exit(-1);
		}
	}

	for (t = 2; t < NUM_THREADS; t++)
		thrd_join(threads[t], &rc);

	stopseeding = 1;

	thrd_join(threads[1], &rc);
	thrd_join(threads[0], &rc);

#ifdef RANDOM_YARROW
	random_yarrow_deinit_alg();
#endif
#ifdef RANDOM_FORTUNA
	random_fortuna_deinit_alg();
#endif

	/* Last thing that main() should do */
	thrd_exit(0);

	return (0);
}
