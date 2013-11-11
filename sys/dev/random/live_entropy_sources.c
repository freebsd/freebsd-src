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
 */

#include <sys/param.h>
__FBSDID("$FreeBSD$");

#include <sys/kernel.h>
#include <sys/libkern.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/random.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/unistd.h>

#include <machine/cpu.h>

#include <dev/random/randomdev.h>
#include <dev/random/randomdev_soft.h>
#include <dev/random/random_adaptors.h>
#include <dev/random/random_harvestq.h>

#include "live_entropy_sources.h"

LIST_HEAD(les_head, live_entropy_sources);
static struct les_head les_sources = LIST_HEAD_INITIALIZER(les_sources);

/*
 * The live_lock protects the consistency of the "struct les_head les_sources"
 */
static struct sx les_lock; /* need a sleepable lock */

void
live_entropy_source_register(struct live_entropy_source *rsource)
{
	struct live_entropy_sources *lles;

	KASSERT(rsource != NULL, ("invalid input to %s", __func__));

	lles = malloc(sizeof(struct live_entropy_sources), M_ENTROPY, M_WAITOK);
	lles->lles_rsource = rsource;

	sx_xlock(&les_lock);
	LIST_INSERT_HEAD(&les_sources, lles, lles_entries);
	sx_xunlock(&les_lock);
}

void
live_entropy_source_deregister(struct live_entropy_source *rsource)
{
	struct live_entropy_sources *lles = NULL;

	KASSERT(rsource != NULL, ("invalid input to %s", __func__));

	sx_xlock(&les_lock);
	LIST_FOREACH(lles, &les_sources, lles_entries)
		if (lles->lles_rsource == rsource) {
			LIST_REMOVE(lles, lles_entries);
			break;
		}
	sx_xunlock(&les_lock);
	if (lles != NULL)
		free(lles, M_ENTROPY);
}

static int
live_entropy_source_handler(SYSCTL_HANDLER_ARGS)
{
	/* XXX: FIX!! Fixed array size */
	char buf[128];
	struct live_entropy_sources *lles;
	int count;

	sx_slock(&les_lock);

	buf[0] = '\0';
	count = 0;
	LIST_FOREACH(lles, &les_sources, lles_entries) {
		strcat(buf, (count++ ? "," : ""));
		strcat(buf, lles->lles_rsource->les_ident);
	}

	sx_sunlock(&les_lock);

	return (SYSCTL_OUT(req, buf, strlen(buf)));
}

/*
 * Run through all "live" sources reading entropy for the given
 * number of rounds, which should be a multiple of the number
 * of entropy accumulation pools in use; 2 for Yarrow and 32
 * for Fortuna.
 *
 * BEWARE!!!
 * This function runs inside the RNG thread! Don't do anything silly!
 * Remember that we are NOT holding harvest_mtx on entry!
 */
/* XXXRW: get_cyclecount() is cheap on most modern hardware, where cycle
 * counters are built in, but on older hardware it will do a real time clock
 * read which can be quite expensive.
 */
void
live_entropy_sources_feed(void)
{
	static struct harvest_event event;
	static u_int dest = 0;
	struct live_entropy_sources *lles;
	int i, n;

	sx_slock(&les_lock);

	/*
	 * Walk over all of live entropy sources, and feed their output
	 * to the system-wide RNG.
	 */
	LIST_FOREACH(lles, &les_sources, lles_entries) {

		/* XXX: FIX!! "2" is the number of pools in Yarrow */
		for (i = 0; i < 2; i++) {
			/*
			 * This should be quick, since it's a live entropy
			 * source.
			 */
			/* XXX: FIX!! Whine loudly if this didn't work. */
			n = lles->lles_rsource->les_read(event.he_entropy, HARVESTSIZE);
			event.he_somecounter = get_cyclecount();
			event.he_size = n;
			event.he_bits = (n*8)/2;
			event.he_source = lles->lles_rsource->les_source;
			event.he_destination = dest++;

			/* Do the actual entropy insertion */
			harvest_process_event(&event);
		}

	}

	sx_sunlock(&les_lock);
}

static void
live_entropy_sources_init(void *unused)
{

	SYSCTL_PROC(_kern_random, OID_AUTO, live_entropy_sources,
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE,
	    NULL, 0, live_entropy_source_handler, "",
	    "List of Active Live Entropy Sources");

	sx_init(&les_lock, "live_entropy_sources");
}

static void
live_entropy_sources_deinit(void *unused)
{

	sx_destroy(&les_lock);
}

SYSINIT(random_adaptors, SI_SUB_DRIVERS, SI_ORDER_FIRST,
    live_entropy_sources_init, NULL);
SYSUNINIT(random_adaptors, SI_SUB_DRIVERS, SI_ORDER_FIRST,
    live_entropy_sources_deinit, NULL);
