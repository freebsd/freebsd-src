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

#include <vm/uma.h>

#include <netinet/helper.h>
#include <netinet/helper_module.h>

static struct rwlock helper_list_lock;
RW_SYSINIT(helperlistlock, &helper_list_lock, "helper list lock");

static STAILQ_HEAD(helper_head, helper) helpers = STAILQ_HEAD_INITIALIZER(helpers);

static int num_dblocks = 0;

/* Monotonically increasing ID assigned to helpers on registration. */
static int32_t helper_id = 0;

static struct helper * get_helper(int32_t id);

/*
 * Public KPI functions.
 */
int
init_helper_dblocks(struct helper_dblocks *hdbs)
{
	struct helper *h;
	struct helper_dblock *dblock;
	int i = 0, error = 0;

	KASSERT(hdbs != NULL, ("struct helper_dblocks not initialised!"));

	HELPER_LIST_RLOCK();

	if (num_dblocks == 0) {
		HELPER_LIST_RUNLOCK();
		return (0);
	}

	/* XXXLAS: Should only allocate for helpers of the appropriate class. */
	hdbs->blocks = malloc(num_dblocks * sizeof(struct helper_dblock), M_HELPER,
	    M_NOWAIT | M_ZERO);

	if (hdbs->blocks != NULL) {
		printf("Malloced ptr %p for %d data blocks\n", hdbs->blocks,
		    hdbs->nblocks);
		STAILQ_FOREACH(h, &helpers, h_next) {
			if (h->h_flags & HELPER_NEEDS_DBLOCK) {
				dblock = hdbs->blocks+i;
				dblock->hd_block = uma_zalloc(h->h_zone,
				    M_NOWAIT);
				/*
				if (dblock[i]->block == NULL) {
					XXX: Free all previous dblocks.
					error = ENOMEM
					break;
				}
				*/
				dblock->hd_id = h->h_id;
				printf("dblock[%d]: id=%d, block=%p\n", i,
				dblock->hd_id, dblock->hd_block);
				i++;
			}
		}
		hdbs->nblocks = i;
	} else
		error = ENOMEM;

	HELPER_LIST_RUNLOCK();
	return (error);
}

int
destroy_helper_dblocks(struct helper_dblocks *hdbs)
{
	struct helper *h;
	int32_t nblocks = hdbs->nblocks;

	HELPER_LIST_WLOCK();

	for (nblocks--; nblocks >= 0; nblocks--) {
		if ((h = get_helper(hdbs->blocks[nblocks].hd_id)) != NULL)
			uma_zfree(h->h_zone, hdbs->blocks[nblocks].hd_block);
	}

	HELPER_LIST_WUNLOCK();
	free(hdbs->blocks, M_HELPER);
	return (0);
}

int
register_helper(struct helper *h)
{
	printf("Register helper %p\n", h);

	HELPER_LIST_WLOCK();

	if (h->h_flags | HELPER_NEEDS_DBLOCK)
		num_dblocks++;

	h->h_id = helper_id++;
	STAILQ_INSERT_TAIL(&helpers, h, h_next);
	HELPER_LIST_WUNLOCK();
	return (0);
}

int
deregister_helper(struct helper *h)
{
	printf("Deregister helper %p\n", h);

	/*
	HHOOK_WLOCK
	Remove this helper's hooks
	HHOOK_WUNLOCK
	*/

	HELPER_LIST_WLOCK();
	STAILQ_REMOVE(&helpers, h, helper, h_next);
	if (h->h_flags | HELPER_NEEDS_DBLOCK)
		num_dblocks--;
	HELPER_LIST_WUNLOCK();
	return (0);
}

int32_t
get_helper_id(char *hname)
{
	struct helper *h;
	int32_t id = -1;

	HELPER_LIST_RLOCK();
	STAILQ_FOREACH(h, &helpers, h_next) {
		if (strncmp(h->h_name, hname, HELPER_NAME_MAXLEN) == 0) {
			id = h->h_id;
			break;
		}
	}
	HELPER_LIST_RUNLOCK();
	return (id);
}

void *
get_helper_dblock(struct helper_dblocks *hdbs, int32_t id)
{
	uint32_t nblocks = hdbs->nblocks;

	for (nblocks--; nblocks >= 0; nblocks--) {
		if (hdbs->blocks[nblocks].hd_id == id)
			return (hdbs->blocks[nblocks].hd_block);
	}
	return (NULL);
}

/*
 * Private KPI functions.
 */
static struct helper *
get_helper(int32_t id)
{
	struct helper *h;

	HELPER_LIST_LOCK_ASSERT();

	STAILQ_FOREACH(h, &helpers, h_next) {
		if (h->h_id == id)
			return (h);
	}
	return (NULL);
}

/*
 * Handles kld related events. Returns 0 on success, non-zero on failure.
 */
int
helper_modevent(module_t mod, int event_type, void *data)
{
	int error = 0;
	struct helper_modevent_data *hmd = (struct helper_modevent_data *)data;

	switch(event_type) {
		case MOD_LOAD:
			if (hmd->helper->h_flags & HELPER_NEEDS_DBLOCK) {
				if (hmd->uma_zsize <= 0) {
					printf("Use DECLARE_HELPER_UMA() instead!\n");
					error = EDOOFUS;
					break;
				}
				hmd->helper->h_zone =
				    uma_zcreate(hmd->name, hmd->uma_zsize,
				    hmd->umactor, hmd->umadtor, NULL, NULL, 0,
				    0);
				if (hmd->helper->h_zone == NULL) {
					error = ENOMEM;
					break;
				}
			}
			strlcpy(hmd->helper->h_name, hmd->name,
			    HELPER_NAME_MAXLEN);
			if (hmd->helper->mod_init != NULL)
				error = hmd->helper->mod_init();
			if (!error)
				error = register_helper(hmd->helper);
			break;

		case MOD_QUIESCE:
			error = deregister_helper(hmd->helper);
			uma_zdestroy(hmd->helper->h_zone);
			if (!error && hmd->helper->mod_destroy != NULL)
				hmd->helper->mod_destroy();
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
