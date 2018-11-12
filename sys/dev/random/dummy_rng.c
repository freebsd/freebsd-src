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

#include "opt_random.h"

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/random.h>
#include <sys/syslog.h>
#include <sys/systm.h>

#include <dev/random/randomdev.h>
#include <dev/random/random_adaptors.h>

static int
dummy_random_zero(void)
{

	return (0);
}

static void
dummy_random(void)
{
}

/* ARGSUSED */
static void
dummy_random_init(void)
{

#ifdef RANDOM_DEBUG
	printf("random: %s\n", __func__);
#endif

	randomdev_init_reader(dummy_random_read_phony);
}

/* This is used only by the internal read_random(9) call, and then only
 * if no entropy processor is loaded.
 *
 * Make a token effort to provide _some_ kind of output. No warranty of
 * the quality of this output is made, mainly because its lousy.
 *
 * This is only used by the internal read_random(9) call when no other
 * adaptor is active.
 *
 * It has external scope due to the way things work in
 * randomdev_[de]init_reader() that the rest of the world doesn't need to
 * know about.
 *
 * Caveat Emptor.
 */
void
dummy_random_read_phony(uint8_t *buf, u_int count)
{
	/* If no entropy device is loaded, don't spam the console with warnings */
	u_long randval;
	size_t size, i;

	/* srandom() is called in kern/init_main.c:proc0_post() */

	/* Fill buf[] with random(9) output */
	for (i = 0; i < count; i += sizeof(randval)) {
		randval = random();
		size = MIN(count - i, sizeof(randval));
		memcpy(buf + i, &randval, (size_t)size);
	}
}

struct random_adaptor randomdev_dummy = {
	.ra_ident = "Dummy",
	.ra_priority = 1, /* Bottom priority, so goes to last position */
	.ra_reseed = dummy_random,
	.ra_seeded = (random_adaptor_seeded_func_t *)dummy_random_zero,
	.ra_read = (random_adaptor_read_func_t *)dummy_random_zero,
	.ra_write = (random_adaptor_write_func_t *)dummy_random_zero,
	.ra_init = dummy_random_init,
	.ra_deinit = dummy_random,
};
