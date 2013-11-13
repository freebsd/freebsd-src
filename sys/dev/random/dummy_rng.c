/*-
 * Copyright (c) 2013 Arthur Mesh <arthurmesh@gmail.com>
 * Copyright (c) 2013 Mark R V Murray
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
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/random.h>
#include <sys/syslog.h>
#include <sys/systm.h>
#include <sys/time.h>

#include <dev/random/randomdev.h>
#include <dev/random/random_adaptors.h>

static struct mtx dummy_random_mtx;

/* If no entropy device is loaded, don't spam the console with warnings */
static int warned = 0;

/* Used to fake out unused random calls in random_adaptor */
static void
random_null_func(void)
{
}

static int
dummy_random_poll(int events __unused, struct thread *td __unused)
{

	return (0);
}

static int
dummy_random_block(int flag)
{
	int error = 0;

	mtx_lock(&dummy_random_mtx);

	/* Blocking logic */
	while (!error) {
		if (flag & O_NONBLOCK)
			error = EWOULDBLOCK;
		else {
			printf("random: dummy device blocking on read.\n");
			error = msleep(&dummy_random_block,
			    &dummy_random_mtx,
			    PUSER | PCATCH, "block", 0);
		}
	}
	mtx_unlock(&dummy_random_mtx);

	return (error);
}

static void
dummy_random_init(void)
{

	mtx_init(&dummy_random_mtx, "sleep mtx for dummy_random",
	    NULL, MTX_DEF);
}

static void
dummy_random_deinit(void)
{

	mtx_destroy(&dummy_random_mtx);
}

/* This is used only by the internal read_random(9) call, and then only
 * if no entropy processor is loaded.
 *
 * DO NOT, REPEAT, DO NOT add this to the "struct random_adaptor" below!
 *
 * Make a token effort to provide _some_ kind of output. No warranty of
 * the quality of this output is made, mainly because its lousy.
 *
 * Caveat Emptor.
 */
int
dummy_random_read_phony(void *buf, int count)
{
	u_long randval;
	int size, i;

	if (!warned) {
		log(LOG_WARNING, "random device not loaded; using insecure pseudo-random number generator\n");
		warned = 1;
	}

	/* srandom() is called in kern/init_main.c:proc0_post() */

	/* Fill buf[] with random(9) output */
	for (i = 0; i < count; i+= (int)sizeof(u_long)) {
		randval = random();
		size = MIN(count - i, sizeof(u_long));
		memcpy(&((char *)buf)[i], &randval, (size_t)size);
	}

	return (count);
}

struct random_adaptor randomdev_dummy = {
	.ra_ident = "Dummy entropy device",
	.ra_init = dummy_random_init,
	.ra_deinit = dummy_random_deinit,
	.ra_block = dummy_random_block,
	.ra_poll = dummy_random_poll,
	.ra_read = (random_adaptor_read_func_t *)random_null_func,
	.ra_reseed = (random_adaptor_reseed_func_t *)random_null_func,
	.ra_seeded = 0, /* This device can never be seeded */
	.ra_priority = 1, /* Bottom priority, so goes to last position */
};
