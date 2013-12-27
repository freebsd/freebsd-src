/*
 * Copyright (C) 2001 Jason Evans <jasone@freebsd.org>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice(s), this list of conditions and the following disclaimer
 *    unmodified other than the allowable addition of one or more
 *    copyright notices.
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
 * Test thread stack guard functionality.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>

#define FRAME_SIZE	1024
#define FRAME_OVERHEAD	  40

struct args
{
	void	*top;	/* Top of thread's initial stack frame. */
	int	cur;	/* Recursion depth. */
	int	max;	/* Maximum recursion depth. */
};

void *
recurse(void *args)
{
	int		top;
	struct args	*parms = (struct args *)args;
	char		filler[FRAME_SIZE - FRAME_OVERHEAD];

	/* Touch the memory in this stack frame. */
	top = 0xa5;
	memset(filler, 0xa5, sizeof(filler));

	if (parms->top == NULL) {
		/* Initial stack frame. */
		parms->top = (void*)&top;
	}

	/*
	 * Make sure frame size is what we expect.  Getting this right involves
	 * hand tweaking, so just print a warning rather than aborting.
	 */
	if (parms->top - (void *)&top != FRAME_SIZE * parms->cur) {
		fprintf(stderr,
		    "Stack size (%ld) != expected (%ld), frame %ld\n",
		    (long)parms->top - (long)&top,
		    (long)(FRAME_SIZE * parms->cur), (long)parms->cur);
	}

	parms->cur++;
	if (parms->cur < parms->max)
		recurse(args);

	return NULL;
}


int
main(int argc, char **argv)
{
	size_t		def_stacksize, def_guardsize;
	size_t		stacksize, guardsize;
	pthread_t	thread;
	pthread_attr_t	attr;
	struct args	args;

	if (argc != 3) {
		fprintf(stderr, "usage: guard_b <stacksize> <guardsize>\n");
		exit(1);
	}
	fprintf(stderr, "Test begin\n");

	stacksize = strtoul(argv[1], NULL, 10);
	guardsize = strtoul(argv[2], NULL, 10);

	assert(pthread_attr_init(&attr) == 0);
	/*
	 * Exercise the attribute APIs more thoroughly than is strictly
	 * necessary for the meat of this test program.
	 */
	assert(pthread_attr_getstacksize(&attr, &def_stacksize) == 0);
	assert(pthread_attr_getguardsize(&attr, &def_guardsize) == 0);
	if (def_stacksize != stacksize) {
		assert(pthread_attr_setstacksize(&attr, stacksize) == 0);
		assert(pthread_attr_getstacksize(&attr, &def_stacksize) == 0);
		assert(def_stacksize == stacksize);
	}
	if (def_guardsize != guardsize) {
		assert(pthread_attr_setguardsize(&attr, guardsize) == 0);
		assert(pthread_attr_getguardsize(&attr, &def_guardsize) == 0);
		assert(def_guardsize >= guardsize);
	}

	/*
	 * Create a thread that will come just short of overflowing the thread
	 * stack.  We need to leave a bit of breathing room in case the thread
	 * is context switched, and we also have to take care not to call any
	 * functions in the deepest stack frame.
	 */
	args.top = NULL;
	args.cur = 0;
	args.max = (stacksize / FRAME_SIZE) - 1;
	fprintf(stderr, "No overflow:\n");
	assert(pthread_create(&thread, &attr, recurse, &args) == 0);
	assert(pthread_join(thread, NULL) == 0);
	
	/*
	 * Create a thread that will barely of overflow the thread stack.  This
	 * should cause a segfault.
	 */
	args.top = NULL;
	args.cur = 0;
	args.max = (stacksize / FRAME_SIZE) + 1;
	fprintf(stderr, "Overflow:\n");
	assert(pthread_create(&thread, &attr, recurse, &args) == 0);
	assert(pthread_join(thread, NULL) == 0);

	/* Not reached. */
	fprintf(stderr, "Unexpected success\n");
	abort();

	return 0;
}
