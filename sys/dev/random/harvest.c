/*-
 * Copyright (c) 2000 Mark R V Murray
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/kthread.h>
#include <sys/mutex.h>
#include <sys/poll.h>
#include <sys/select.h>
#include <sys/random.h>

#include <machine/cpu.h>

#include <crypto/blowfish/blowfish.h>

#include <dev/random/hash.h>
#include <dev/random/yarrow.h>

static u_int read_random_phony(void *, u_int);

/* hold the address of the routine which is actually called if
 * the randomdev is loaded
 */
static void (*reap_func)(u_int64_t, void *, u_int, u_int, u_int, u_int) = NULL;
static u_int (*read_func)(void *, u_int) = read_random_phony;

/* Initialise the harvester at load time */
void
random_init_harvester(void (*reaper)(u_int64_t, void *, u_int, u_int, u_int, u_int), u_int (*reader)(void *, u_int))
{
	reap_func = reaper;
	read_func = reader;
}

/* Deinitialise the harvester at unload time */
void
random_deinit_harvester(void)
{
	reap_func = NULL;
	read_func = read_random_phony;
}

/* Entropy harvesting routine. This is supposed to be fast; do
 * not do anything slow in here!
 * Implemented as in indirect call to allow non-inclusion of
 * the entropy device.
 */
void
random_harvest(void *entropy, u_int count, u_int bits, u_int frac, u_int origin)
{
	if (reap_func)
		(*reap_func)(get_cyclecount(), entropy, count, bits, frac, origin);
}

/* Userland-visible version of read_random */
u_int
read_random(void *buf, u_int count)
{
	return (*read_func)(buf, count);
}

/* If the entropy device is not loaded, make a token effort to
 * provide _some_ kind of randomness. This should only be used
 * inside other RNG's, like arc4random(9).
 */
static u_int
read_random_phony(void *buf, u_int count)
{
	u_long randval;
	int size, i;
	static int initialised = 0;

	/* Try to give random(9) a half decent initialisation
	 * DO NOT make the mistake of thinking this is secure!!
	 */
	if (!initialised)
		srandom((u_long)get_cyclecount());

	/* Fill buf[] with random(9) output */
	for (i = 0; i < count; i+= sizeof(u_long)) {
		randval = random();
		size = (count - i) < sizeof(u_long) ? (count - i) : sizeof(u_long);
		memcpy(&((char *)buf)[i], &randval, size);
	}

	return count;
}

/* Helper routine to enable kthread_exit() to work while the module is
 * being (or has been) unloaded.
 * This routine is in this file because it is always linked into the kernel,
 * and will thus never be unloaded. This is critical for unloadable modules
 * that have threads.
 */
void
random_set_wakeup_exit(void *control)
{
	wakeup(control);
	mtx_enter(&Giant, MTX_DEF);
	kthread_exit(0);
	/* NOTREACHED */
}
