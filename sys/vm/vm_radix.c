/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2013 EMC Corp.
 * Copyright (c) 2011 Jeffrey Roberson <jeff@freebsd.org>
 * Copyright (c) 2008 Mayur Shardul <mayur.shardul@gmail.com>
 * All rights reserved.
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * Path-compressed radix trie implementation.
 * The following code is not generalized into a general purpose library
 * because there are way too many parameters embedded that should really
 * be decided by the library consumers.  At the same time, consumers
 * of this code must achieve highest possible performance.
 *
 * The implementation takes into account the following rationale:
 * - Size of the nodes should be as small as possible but still big enough
 *   to avoid a large maximum depth for the trie.  This is a balance
 *   between the necessity to not wire too much physical memory for the nodes
 *   and the necessity to avoid too much cache pollution during the trie
 *   operations.
 * - There is not a huge bias toward the number of lookup operations over
 *   the number of insert and remove operations.  This basically implies
 *   that optimizations supposedly helping one operation but hurting the
 *   other might be carefully evaluated.
 * - On average not many nodes are expected to be fully populated, hence
 *   level compression may just complicate things.
 */

#include <sys/cdefs.h>
#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/libkern.h>
#include <sys/pctrie.h>
#include <sys/proc.h>
#include <sys/vmmeter.h>
#include <sys/smr.h>
#include <sys/smr_types.h>

#include <vm/uma.h>
#include <vm/vm.h>
#include <vm/vm_radix.h>

static uma_zone_t vm_radix_node_zone;
smr_t vm_radix_smr;

void *
vm_radix_node_alloc(struct pctrie *ptree)
{
	return (uma_zalloc_smr(vm_radix_node_zone, M_NOWAIT));
}

void
vm_radix_node_free(struct pctrie *ptree, void *node)
{
	uma_zfree_smr(vm_radix_node_zone, node);
}

#ifndef UMA_USE_DMAP
void vm_radix_reserve_kva(void);
/*
 * Reserve the KVA necessary to satisfy the node allocation.
 * This is mandatory in architectures not supporting direct
 * mapping as they will need otherwise to carve into the kernel maps for
 * every node allocation, resulting into deadlocks for consumers already
 * working with kernel maps.
 */
void
vm_radix_reserve_kva(void)
{

	/*
	 * Calculate the number of reserved nodes, discounting the pages that
	 * are needed to store them.
	 */
	if (!uma_zone_reserve_kva(vm_radix_node_zone,
	    ((vm_paddr_t)vm_cnt.v_page_count * PAGE_SIZE) / (PAGE_SIZE +
	    pctrie_node_size())))
		panic("%s: unable to reserve KVA", __func__);
}
#endif

/*
 * Initialize the UMA slab zone.
 */
void
vm_radix_zinit(void)
{

	vm_radix_node_zone = uma_zcreate("RADIX NODE", pctrie_node_size(),
	    NULL, NULL, pctrie_zone_init, NULL,
	    PCTRIE_PAD, UMA_ZONE_VM | UMA_ZONE_SMR);
	vm_radix_smr = uma_zone_get_smr(vm_radix_node_zone);
}

void
vm_radix_wait(void)
{
	uma_zwait(vm_radix_node_zone);
}
