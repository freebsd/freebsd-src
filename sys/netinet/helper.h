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

#ifndef	_NETINET_HELPER_H_
#define	_NETINET_HELPER_H_


struct helper_dblock {
	/* ID of the helper this data block is associated with */
	int id;

	void *block;
};

#define HELPER_NAME_MAXLEN 16
struct helper {
	char		name[HELPER_NAME_MAXLEN];
	/* Init global module state on kldload. */
	int (*mod_init) (void);

	/* Cleanup global module state on kldunload. */
	int (*mod_destroy) (void);

	uint16_t	flags;
	uint32_t	class;

	uma_zone_t	zone;

	//STAILQ hooks; /* which hooks does this helper want to be called from */
	unsigned int id; /* ID assigned by system to this hlpr's data in the
	dynamic array */


	STAILQ_ENTRY(helper) h_next;
};

/* Helper flags */
#define HELPER_NEEDS_DBLOCK	0x0001

/* Helper classes */
#define HELPER_CLASS_TCP	0x00000001

int	init_helper_dblocks(struct helper_dblock **dblocks, int *nblocks);
int	destroy_helper_dblocks(struct helper_dblock *dblocks, int nblocks);
int	register_helper(struct helper *h);
int	deregister_helper(struct helper *h);
int	get_helper_id(char *hname);
void *	get_helper_dblock(struct helper_dblock *dblocks,
    int nblocks, int id);

#define	HELPER_LIST_WLOCK() rw_wlock(&helper_list_lock)
#define	HELPER_LIST_WUNLOCK() rw_wunlock(&helper_list_lock)
#define	HELPER_LIST_RLOCK() rw_rlock(&helper_list_lock)
#define	HELPER_LIST_RUNLOCK() rw_runlock(&helper_list_lock)
#define	HELPER_LIST_LOCK_ASSERT() rw_assert(&helper_list_lock, RA_LOCKED)

#endif /* _NETINET_HELPER_H_ */
