/*
 * Copyright (C) 2001 Jason Evans <jasone@freebsd.org>.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice(s), this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified other than the possible
 *    addition of one or more copyright notices.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice(s), this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER(S) ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 * Test for leaked joined threads.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <errno.h>
#include <string.h>
#include <pthread.h>

#define	NITERATIONS	16384
#define	MAXGROWTH	16384

void *
thread_entry(void *a_arg)
{
	return NULL;
}

int
main(void)
{
	pthread_t	thread;
	int		i, error;
	char		*brk, *nbrk;
	unsigned	growth;

	fprintf(stderr, "Test begin\n");

	/* Get an initial brk value. */
	brk = sbrk(0);

	/* Create threads and join them, one at a time. */
	for (i = 0; i < NITERATIONS; i++) {
		if ((error = pthread_create(&thread, NULL, thread_entry, NULL))
		    != 0) {
			fprintf(stderr, "Error in pthread_create(): %s\n",
			    strerror(error));
			exit(1);
		}
		if ((error = pthread_join(thread, NULL)) != 0) {
			fprintf(stderr, "Error in pthread_join(): %s\n",
			    strerror(error));
			exit(1);
		}
	}

	/* Get a final brk value. */
	nbrk = sbrk(0);

	/*
	 * Check that the amount of heap space allocated is below an acceptable
	 * threshold.  We could just compare brk and nbrk, but the test could
	 * conceivably break if the internals of the threads library changes.
	 */
	if (nbrk > brk) {
		/* Heap grows up. */
		growth = nbrk - brk;
	} else if (nbrk <= brk) {
		/* Heap grows down, or no growth. */
		growth = brk - nbrk;
	}

	if (growth > MAXGROWTH) {
		fprintf(stderr, "Heap growth exceeded maximum (%u > %u)\n",
		    growth, MAXGROWTH);
	}
#if (0)
	else {
		fprintf(stderr, "Heap growth acceptable (%u <= %u)\n",
		    growth, MAXGROWTH);
	}
#endif

	fprintf(stderr, "Test end\n");
	return 0;
}
