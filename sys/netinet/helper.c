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
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/queue.h>
#include <sys/systm.h>

#include <netinet/helper.h>
#include <netinet/helper_module.h>


struct hlpr_head helpers = STAILQ_HEAD_INITIALIZER(helpers);
static int num_datablocks = 0;


int
init_datablocks(uintptr_t **array_head, int *nblocks)
{
	struct helper *h;
	int i = 0;

	if(num_datablocks <= 0)
		return (0);


	*array_head = malloc(num_datablocks * sizeof(uintptr_t), M_HLPR, M_NOWAIT
	| M_ZERO);

	printf("Malloced ptr %p for %d data blocks\n", *array_head, num_datablocks);
	STAILQ_FOREACH(h, &helpers, entries) {
		KASSERT(i < num_datablocks, ("Badness!\n"));
		if (h->block_init != NULL) {
			printf("Calling block_init(%p) for helper: %p\n",
			(*array_head)+i, h);
			h->block_init((*array_head)+i);
		}
		i++;
	}

	*nblocks = num_datablocks;

	return (0);
}

int
destroy_datablocks(uintptr_t **array_head, int nblocks)
{
	struct helper *h;
	int i = 0;
	//for (; nblocks >= 0; nblocks--)
	//	h->block_destroy();

	STAILQ_FOREACH(h, &helpers, entries) {
		if (h->block_destroy != NULL) {
			printf("Calling block_destroy(%p) for helper: %p\n",
			array_head[i], h);
			h->block_destroy(array_head[i++]);
		}
	}

	return (0);
}

int
register_helper(struct helper *h)
{
	/*for hooks in hlpr
		register hlpr_callback for hook

	if !errorgt
		h->dynamic_id = X
	*/
	printf("Register helper 0x%p\n", h);

	if (h->flags | HLPR_NEEDS_DATABLOCK)
		num_datablocks++;

	STAILQ_INSERT_TAIL(&helpers, h, entries);

	return (0);
}

int
deregister_helper(struct helper *h)
{
	printf("Deregister helper 0x%p\n", h);

	STAILQ_REMOVE(&helpers, h, helper, entries);
	num_datablocks--;
	return (0);
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
