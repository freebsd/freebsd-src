/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (C) 2010 Nathan Whitehorn
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _POWERPC_AIM_MMU_OEA64_H
#define _POWERPC_AIM_MMU_OEA64_H

#include "opt_pmap.h"

#include <vm/vm_extern.h>
#include <machine/mmuvar.h>

struct dump_context {
	u_long ptex;
	u_long ptex_end;
	size_t blksz;
};

extern const struct mmu_kobj oea64_mmu;

/*
 * Helper routines
 */

/* Allocate physical memory for use in moea64_bootstrap. */
vm_offset_t	moea64_bootstrap_alloc(vm_size_t size, vm_size_t align);
/* Set an LPTE structure to match the contents of a PVO */
void	moea64_pte_from_pvo(const struct pvo_entry *pvo, struct lpte *lpte);

/*
 * Flags
 */

#define MOEA64_PTE_PROT_UPDATE	1
#define MOEA64_PTE_INVALIDATE	2

/*
 * Bootstrap subroutines
 *
 * An MMU_BOOTSTRAP() implementation looks like this:
 *   moea64_early_bootstrap();
 *   Allocate Page Table
 *   moea64_mid_bootstrap();
 *   Add mappings for MMU resources
 *   moea64_late_bootstrap();
 */

void		moea64_early_bootstrap(vm_offset_t kernelstart,
		    vm_offset_t kernelend);
void		moea64_mid_bootstrap(vm_offset_t kernelstart,
		    vm_offset_t kernelend);
void		moea64_late_bootstrap(vm_offset_t kernelstart,
		    vm_offset_t kernelend);

/* "base" install method for initializing moea64 pmap ifuncs */
void		moea64_install(void);

int64_t		moea64_pte_replace(struct pvo_entry *, int);
int64_t		moea64_pte_insert(struct pvo_entry *);
int64_t		moea64_pte_unset(struct pvo_entry *);
int64_t		moea64_pte_clear(struct pvo_entry *, uint64_t);
int64_t		moea64_pte_synch(struct pvo_entry *);
int64_t		moea64_pte_insert_sp(struct pvo_entry *);
int64_t		moea64_pte_unset_sp(struct pvo_entry *);
int64_t		moea64_pte_replace_sp(struct pvo_entry *);

typedef int64_t	(*moea64_pte_replace_t)(struct pvo_entry *, int);
typedef int64_t	(*moea64_pte_insert_t)(struct pvo_entry *);
typedef int64_t	(*moea64_pte_unset_t)(struct pvo_entry *);
typedef int64_t	(*moea64_pte_clear_t)(struct pvo_entry *, uint64_t);
typedef int64_t	(*moea64_pte_synch_t)(struct pvo_entry *);
typedef int64_t	(*moea64_pte_insert_sp_t)(struct pvo_entry *);
typedef int64_t	(*moea64_pte_unset_sp_t)(struct pvo_entry *);
typedef int64_t	(*moea64_pte_replace_sp_t)(struct pvo_entry *);

struct moea64_funcs {
	moea64_pte_replace_t	pte_replace;
	moea64_pte_insert_t	pte_insert;
	moea64_pte_unset_t	pte_unset;
	moea64_pte_clear_t	pte_clear;
	moea64_pte_synch_t	pte_synch;
	moea64_pte_insert_sp_t	pte_insert_sp;
	moea64_pte_unset_sp_t	pte_unset_sp;
	moea64_pte_replace_sp_t	pte_replace_sp;
};

extern struct moea64_funcs *moea64_ops;

static inline uint64_t
moea64_pte_vpn_from_pvo_vpn(const struct pvo_entry *pvo)
{
	return ((pvo->pvo_vpn >> (ADDR_API_SHFT64 - ADDR_PIDX_SHFT)) &
	    LPTE_AVPN_MASK);
}

/*
 * Statistics
 */

#ifdef MOEA64_STATS
extern u_int	moea64_pte_valid;
extern u_int	moea64_pte_overflow;
#define STAT_MOEA64(x)	x
#else
#define	STAT_MOEA64(x) ((void)0)
#endif

/*
 * State variables
 */

extern int		moea64_large_page_shift;
extern uint64_t		moea64_large_page_size;
extern uint64_t		moea64_large_page_mask;
extern u_long		moea64_pteg_count;
extern u_long		moea64_pteg_mask;
extern int		n_slbs;
extern bool		moea64_has_lp_4k_16m;

#endif /* _POWERPC_AIM_MMU_OEA64_H */
