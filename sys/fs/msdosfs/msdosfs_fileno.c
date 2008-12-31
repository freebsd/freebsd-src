/*-
 * Copyright (c) 2003-2004 Tim J. Robbins.
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
 */
/*
 * Compress 64-bit file numbers into temporary, unique 32-bit file numbers.
 * This is needed because the algorithm we use to calculate these numbers
 * generates 64-bit quantities, but struct dirent's d_fileno member and
 * struct vnodeattr's va_fileid member only have space for 32 bits.
 *
 * 32-bit file numbers are generated sequentially, and stored in a
 * red-black tree, indexed on 64-bit file number. The mappings do not
 * persist across reboots (or unmounts); anything that relies on this
 * (e.g. NFS) will not work correctly. This scheme consumes 32 bytes
 * of kernel memory per file (on i386), and it may be possible for a user
 * to cause a panic by creating millions of tiny files.
 *
 * As an optimization, we split the file number space between statically
 * allocated and dynamically allocated. File numbers less than
 * FILENO_FIRST_DYN are left unchanged and do not have any tree nodes
 * allocated to them.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/fs/msdosfs/msdosfs_fileno.c,v 1.5.6.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/mutex.h>

#include <fs/msdosfs/bpb.h>
#include <fs/msdosfs/direntry.h>
#include <fs/msdosfs/msdosfsmount.h>

static MALLOC_DEFINE(M_MSDOSFSFILENO, "msdosfs_fileno", "MSDOSFS fileno mapping node");

static struct mtx fileno_mtx;
MTX_SYSINIT(fileno, &fileno_mtx, "MSDOSFS fileno", MTX_DEF);

RB_PROTOTYPE(msdosfs_filenotree, msdosfs_fileno, mf_tree,
    msdosfs_fileno_compare)

static int msdosfs_fileno_compare(struct msdosfs_fileno *,
    struct msdosfs_fileno *);

#define	FILENO_FIRST_DYN	0xf0000000

/* Initialize file number mapping structures. */
void
msdosfs_fileno_init(mp)
	struct mount *mp;
{
	struct msdosfsmount *pmp = VFSTOMSDOSFS(mp);

	RB_INIT(&pmp->pm_filenos);
	pmp->pm_nfileno = FILENO_FIRST_DYN;
        if (pmp->pm_HugeSectors > 0xffffffff /
	    (pmp->pm_BytesPerSec / sizeof(struct direntry)) + 1)
		pmp->pm_flags |= MSDOSFS_LARGEFS;
}

/* Free 32-bit file number generation structures. */
void
msdosfs_fileno_free(mp)
	struct mount *mp;
{
	struct msdosfsmount *pmp = VFSTOMSDOSFS(mp);
	struct msdosfs_fileno *mf, *next;

	for (mf = RB_MIN(msdosfs_filenotree, &pmp->pm_filenos); mf != NULL;
	    mf = next) {
		next = RB_NEXT(msdosfs_filenotree, &pmp->pm_filenos, mf);
		RB_REMOVE(msdosfs_filenotree, &pmp->pm_filenos, mf);
		free(mf, M_MSDOSFSFILENO);
	}
}

/* Map a 64-bit file number into a 32-bit one. */
uint32_t
msdosfs_fileno_map(mp, fileno)
	struct mount *mp;
	uint64_t fileno;
{
	struct msdosfsmount *pmp = VFSTOMSDOSFS(mp);
	struct msdosfs_fileno key, *mf, *tmf;
	uint32_t mapped;

	if ((pmp->pm_flags & MSDOSFS_LARGEFS) == 0) {
		KASSERT((uint32_t)fileno == fileno,
		    ("fileno >32 bits but not a large fs?"));
		return ((uint32_t)fileno);
	}
	if (fileno < FILENO_FIRST_DYN)
		return ((uint32_t)fileno);
	mtx_lock(&fileno_mtx);
	key.mf_fileno64 = fileno;
	mf = RB_FIND(msdosfs_filenotree, &pmp->pm_filenos, &key);
	if (mf != NULL) {
		mapped = mf->mf_fileno32;
		mtx_unlock(&fileno_mtx);
		return (mapped);
	}
	if (pmp->pm_nfileno < FILENO_FIRST_DYN)
		panic("msdosfs_fileno_map: wraparound");
	mtx_unlock(&fileno_mtx);
	mf = malloc(sizeof(*mf), M_MSDOSFSFILENO, M_WAITOK);
	mtx_lock(&fileno_mtx);
	tmf = RB_FIND(msdosfs_filenotree, &pmp->pm_filenos, &key);
	if (tmf != NULL) {
		mapped = tmf->mf_fileno32;
		mtx_unlock(&fileno_mtx);
		free(mf, M_MSDOSFSFILENO);
		return (mapped);
	}
	mf->mf_fileno64 = fileno;
	mapped = mf->mf_fileno32 = pmp->pm_nfileno++;
	RB_INSERT(msdosfs_filenotree, &pmp->pm_filenos, mf);
	mtx_unlock(&fileno_mtx);
	return (mapped);
}

/* Compare by 64-bit file number. */
static int
msdosfs_fileno_compare(fa, fb)
	struct msdosfs_fileno *fa, *fb;
{

	if (fa->mf_fileno64 > fb->mf_fileno64)
		return (1);
	else if (fa->mf_fileno64 < fb->mf_fileno64)
		return (-1);
	return (0);
}

RB_GENERATE(msdosfs_filenotree, msdosfs_fileno, mf_tree,
    msdosfs_fileno_compare)
