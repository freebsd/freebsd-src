/*-
 * Copyright (c) 2001 Dag-Erling Coïdan Smørgrav
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_pseudofs.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>

#include <fs/pseudofs/pseudofs.h>
#include <fs/pseudofs/pseudofs_internal.h>

static MALLOC_DEFINE(M_PFSFILENO, "pfs_fileno", "pseudofs fileno bitmap");

static struct mtx pfs_fileno_mutex;

#define PFS_BITMAP_SIZE	4096
#define PFS_SLOT_BITS	(int)(sizeof(unsigned int) * CHAR_BIT)
#define PFS_BITMAP_BITS	(PFS_BITMAP_SIZE * PFS_SLOT_BITS)
struct pfs_bitmap {
	u_int32_t		 pb_offset;
	int			 pb_used;
	unsigned int		 pb_bitmap[PFS_BITMAP_SIZE];
	struct pfs_bitmap	*pb_next;
};

/*
 * Initialization
 */
void
pfs_fileno_load(void)
{
	mtx_init(&pfs_fileno_mutex, "pseudofs_fileno", NULL, MTX_DEF);
}

/*
 * Teardown
 */
void
pfs_fileno_unload(void)
{
	mtx_destroy(&pfs_fileno_mutex);
}

/*
 * Initialize fileno bitmap
 */
void
pfs_fileno_init(struct pfs_info *pi)
{
	struct pfs_bitmap *pb;

	MALLOC(pb, struct pfs_bitmap *, sizeof *pb,
	    M_PFSFILENO, M_WAITOK|M_ZERO);

	mtx_lock(&pi->pi_mutex);

	pb->pb_bitmap[0] = 07;
	pb->pb_used = 3;
	pi->pi_bitmap = pb;
	pi->pi_root->pn_fileno = 2;

	mtx_unlock(&pi->pi_mutex);
}

/*
 * Tear down fileno bitmap
 */
void
pfs_fileno_uninit(struct pfs_info *pi)
{
	struct pfs_bitmap *pb, *npb;
	int used;

	mtx_lock(&pi->pi_mutex);

	pb = pi->pi_bitmap;
	pi->pi_bitmap = NULL;

	mtx_unlock(&pi->pi_mutex);

	for (used = 0; pb; pb = npb) {
		npb = pb->pb_next;
		used += pb->pb_used;
		FREE(pb, M_PFSFILENO);
	}
#if 0
	/* we currently don't reclaim filenos */
	if (used > 2)
		printf("WARNING: %d file numbers still in use\n", used);
#endif
}

/*
 * Get the next available file number
 */
static u_int32_t
pfs_get_fileno(struct pfs_info *pi)
{
	struct pfs_bitmap *pb, *ppb;
	u_int32_t fileno;
	unsigned int *p;
	int i;

	mtx_lock(&pi->pi_mutex);

	/* look for the first page with free bits */
	for (ppb = NULL, pb = pi->pi_bitmap; pb; ppb = pb, pb = pb->pb_next)
		if (pb->pb_used != PFS_BITMAP_BITS)
			break;

	/* out of pages? */
	if (pb == NULL) {
		mtx_unlock(&pi->pi_mutex);
		MALLOC(pb, struct pfs_bitmap *, sizeof *pb,
		    M_PFSFILENO, M_WAITOK|M_ZERO);
		mtx_lock(&pi->pi_mutex);
		/* protect against possible race */
		while (ppb->pb_next)
			ppb = ppb->pb_next;
		pb->pb_offset = ppb->pb_offset + PFS_BITMAP_BITS;
		ppb->pb_next = pb;
	}

	/* find the first free slot */
	for (i = 0; i < PFS_BITMAP_SIZE; ++i)
		if (pb->pb_bitmap[i] != UINT_MAX)
			break;

	/* find the first available bit and flip it */
	fileno = pb->pb_offset + i * PFS_SLOT_BITS;
	p = &pb->pb_bitmap[i];
	for (i = 0; i < PFS_SLOT_BITS; ++i, ++fileno)
		if ((*p & (unsigned int)(1 << i)) == 0)
			break;
	KASSERT(i < PFS_SLOT_BITS,
	    ("slot has free bits, yet doesn't"));
	*p |= (unsigned int)(1 << i);
	++pb->pb_used;

	mtx_unlock(&pi->pi_mutex);

	return fileno;
}

/*
 * Free a file number
 */
static void
pfs_free_fileno(struct pfs_info *pi, u_int32_t fileno)
{
	struct pfs_bitmap *pb;
	unsigned int *p;
	int i;

	mtx_lock(&pi->pi_mutex);

	/* find the right page */
	for (pb = pi->pi_bitmap;
	     pb && fileno >= PFS_BITMAP_BITS;
	     pb = pb->pb_next, fileno -= PFS_BITMAP_BITS)
		/* nothing */ ;
	KASSERT(pb,
	    ("fileno isn't in any bitmap"));

	/* find the right bit in the right slot and flip it */
	p = &pb->pb_bitmap[fileno / PFS_SLOT_BITS];
	i = fileno % PFS_SLOT_BITS;
	KASSERT(*p & (unsigned int)(1 << i),
	    ("fileno is already free"));
	*p &= ~((unsigned int)(1 << i));
	--pb->pb_used;

	mtx_unlock(&pi->pi_mutex);
	printf("pfs_free_fileno(): reclaimed %d\n", fileno);
}

/*
 * Allocate a file number
 */
void
pfs_fileno_alloc(struct pfs_info *pi, struct pfs_node *pn)
{
	/* make sure our parent has a file number */
	if (pn->pn_parent && !pn->pn_parent->pn_fileno)
		pfs_fileno_alloc(pi, pn->pn_parent);

	switch (pn->pn_type) {
	case pfstype_root:
	case pfstype_dir:
	case pfstype_file:
	case pfstype_symlink:
	case pfstype_procdir:
		pn->pn_fileno = pfs_get_fileno(pi);
		break;
	case pfstype_this:
		KASSERT(pn->pn_parent != NULL,
		    ("pfstype_this node has no parent"));
		pn->pn_fileno = pn->pn_parent->pn_fileno;
		break;
	case pfstype_parent:
		KASSERT(pn->pn_parent != NULL,
		    ("pfstype_parent node has no parent"));
		if (pn->pn_parent == pi->pi_root) {
			pn->pn_fileno = pn->pn_parent->pn_fileno;
			break;
		}
		KASSERT(pn->pn_parent->pn_parent != NULL,
		    ("pfstype_parent node has no grandparent"));
		pn->pn_fileno = pn->pn_parent->pn_parent->pn_fileno;
		break;
	case pfstype_none:
		KASSERT(0,
		    ("pfs_fileno_alloc() called for pfstype_none node"));
		break;
	}

#if 0
	printf("pfs_fileno_alloc(): %s: ", pi->pi_name);
	if (pn->pn_parent) {
		if (pn->pn_parent->pn_parent) {
			printf("%s/", pn->pn_parent->pn_parent->pn_name);
		}
		printf("%s/", pn->pn_parent->pn_name);
	}
	printf("%s -> %d\n", pn->pn_name, pn->pn_fileno);
#endif
}

/*
 * Release a file number
 */
void
pfs_fileno_free(struct pfs_info *pi, struct pfs_node *pn)
{
	switch (pn->pn_type) {
	case pfstype_root:
	case pfstype_dir:
	case pfstype_file:
	case pfstype_symlink:
	case pfstype_procdir:
		pfs_free_fileno(pi, pn->pn_fileno);
		break;
	case pfstype_this:
	case pfstype_parent:
		/* ignore these, as they don't "own" their file number */
		break;
	case pfstype_none:
		KASSERT(0,
		    ("pfs_fileno_free() called for pfstype_none node"));
		break;
	}
}
