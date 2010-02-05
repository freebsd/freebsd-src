/*-
 * Copyright (c) 2010 Lawrence Stewart <lstewart@freebsd.org>
 * All rights reserved.
 *
 * This software was developed at the Centre for Advanced Internet
 * Architectures, Swinburne University, by Lawrence Stewart,
 * made possible in part by a grant from the FreeBSD Foundation.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/queue.h>
#include <sys/rwlock.h>
#include <sys/systm.h>

#include <netinet/helper.h>
#include <netinet/helper_module.h>

static struct rwlock helper_list_lock;
RW_SYSINIT(helperlistlock, &helper_list_lock, "helper list lock");

static STAILQ_HEAD(hlpr_head, helper) helpers = STAILQ_HEAD_INITIALIZER(helpers);

static int num_dblocks = 0;

/* Monotonically increasing ID assigned to helpers on registration. */
static int helper_id = 0;

static struct helper * get_helper(int id);

/*
 * Public KPI functions.
 */
int
init_helper_dblocks(struct helper_dblock **dblocks, int *nblocks)
{
	struct helper *h;
	struct helper_dblock *dblock;
	int i = 0, error = 0;

	HELPER_LIST_RLOCK();

	if (num_dblocks == 0) {
		HELPER_LIST_RUNLOCK();
		return (0);
	}

	*dblocks = malloc(num_dblocks * sizeof(struct helper_dblock), M_HELPER,
	    M_NOWAIT | M_ZERO);

	if (*dblocks != NULL) {
		printf("Malloced ptr %p for %d data blocks\n", *dblocks, num_dblocks);
		STAILQ_FOREACH(h, &helpers, h_next) {
			if (h->block_init != NULL) {
				dblock = dblocks[i];
				h->block_init(&dblock->block);
				dblock->id = h->id;
				printf("dblock[%d]: id=%d, block=%p\n", i,
				dblock->id, dblock->block);
			}
			i++;
		}
		*nblocks = num_dblocks;
	} else
		error = ENOMEM;

	HELPER_LIST_RUNLOCK();
	return (error);
}

int
destroy_helper_dblocks(struct helper_dblock *array_head, int nblocks)
{
	struct helper *h;

	HELPER_LIST_WLOCK();

	for (nblocks--; nblocks >= 0; nblocks--) {
		h = get_helper(array_head[nblocks].id);
		if (h->block_destroy != NULL)
			h->block_destroy(array_head[nblocks].block);
	}

	HELPER_LIST_WUNLOCK();
	return (0);
}

int
register_helper(struct helper *h)
{
	printf("Register helper 0x%p\n", h);

	HELPER_LIST_WLOCK();

	if (h->flags | HELPER_NEEDS_DBLOCK)
		num_dblocks++;

	h->id = helper_id++;

	STAILQ_INSERT_TAIL(&helpers, h, h_next);

	HELPER_LIST_WUNLOCK();

	return (0);
}

int
deregister_helper(struct helper *h)
{
	printf("Deregister helper 0x%p\n", h);

	HELPER_LIST_WLOCK();
	STAILQ_REMOVE(&helpers, h, helper, h_next);
	num_dblocks--;
	HELPER_LIST_WUNLOCK();

	/* Block unload if there are still consumers to avoid mem leak*/

	return (0);
}

/*
 * Private KPI functions.
 */
static struct helper *
get_helper(int id)
{
	struct helper *h;

	HELPER_LIST_LOCK_ASSERT();

	STAILQ_FOREACH(h, &helpers, h_next) {
		if (h->id == id)
			return (h);
	}
	return (NULL);
}

/*
 * Handles kld related events. Returns 0 on success, non-zero on failure.
 */
int
hlpr_modevent(module_t mod, int event_type, void *data)
{
	int error = 0;
	struct helper *h = (struct helper *)data;

	switch(event_type) {
		case MOD_LOAD:
			if (h->mod_init != NULL)
				error = h->mod_init();
			if (!error)
				error = register_helper(h);
			break;

		case MOD_QUIESCE:
			error = deregister_helper(h);
			if (!error && h->mod_destroy != NULL)
				h->mod_destroy();
			break;

		case MOD_SHUTDOWN:
		case MOD_UNLOAD:
			break;

		default:
			return EINVAL;
			break;
	}

	return (error);
}
