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
#include <sys/linker.h>
#include <sys/libkern.h>
#include <sys/mbuf.h>
#include <sys/random.h>
#include <sys/time.h>
#include <crypto/blowfish/blowfish.h>

#include <dev/randomdev/hash.h>
#include <dev/randomdev/yarrow.h>

/* hold the address of the routine which is actually called if
 * the ramdomdev is loaded
 */
static void (*reap)(struct timespec *, void *, u_int, u_int, u_int, u_int) = NULL;

/* Initialise the harvester at load time */
void
random_init_harvester(void (*reaper)(struct timespec *, void *, u_int, u_int, u_int, u_int))
{
	reap = reaper;
}

/* Deinitialise the harvester at unload time */
void
random_deinit_harvester(void)
{
	reap = NULL;
}

/* Entropy harvesting routine. This is supposed to be fast; do
 * not do anything slow in here!
 * Implemented as in indirect call to allow non-inclusion of
 * the entropy device.
 */
void
random_harvest(void *entropy, u_int count, u_int bits, u_int frac, u_int origin)
{
	struct timespec timebuf;

	if (reap) {
		nanotime(&timebuf);
		(*reap)(&timebuf, entropy, count, bits, frac, origin);
	}
}

/* Helper routines to enable kthread_exit() to work while the module is
 * being (or has been) unloaded.
 */
void
random_set_wakeup(int *var, int value)
{
	*var = value;
	wakeup(var);
}

void
random_set_wakeup_exit(int *var, int value, int exitval)
{
	random_set_wakeup(var, value);
	kthread_exit(exitval);
}
