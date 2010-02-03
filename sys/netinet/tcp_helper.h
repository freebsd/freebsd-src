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
 *
 * $FreeBSD$
 */

#ifndef	_TCP_HELPER_H
#define	_TCP_HELPER_H


struct helper {
	/* Init global module state on kldload. */
	int (*mod_init) (void);

	/* Cleanup global module state on kldunload. */
	int (*mod_destroy) (void);

	int (*block_init) (uintptr_t *data);
	int (*block_destroy) (uintptr_t *data);

	uint16_t	flags;

	//STAILQ hooks; /* which hooks does this helper want to be called from */
	//STAILQ struct helper_data;
	int dynamic_id; /* ID assigned by system to this hlpr's data in the
	dynamic array */


	STAILQ_ENTRY(helper) entries;
};

/* Helper flags */
#define HLPR_NEEDS_DATABLOCK	0x0001

extern	STAILQ_HEAD(hlpr_head, helper) helpers;

int	init_datablocks(uintptr_t **array_head, int *nblocks);
int	destroy_datablocks(uintptr_t **array_head, int nblocks);
int	register_helper(struct helper *h);
int	deregister_helper(struct helper *h);

#endif /* _TCP_HELPER_H */
