/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2022 Tomohiro Kusumi <tkusumi@netbsd.org>
 * Copyright (c) 2011-2022 The DragonFly Project.  All rights reserved.
 *
 * This code is derived from software contributed to The DragonFly Project
 * by Matthew Dillon <dillon@dragonflybsd.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/tree.h>

#include "hammer2.h"

#define HAMMER2_DOP_READ	1

/*
 * Implements an abstraction layer for buffered device I/O.
 * Can be used as an OS-abstraction but the main purpose is to allow larger
 * buffers to be used against hammer2_chain's using smaller allocations,
 * without causing deadlocks.
 */
static int hammer2_io_cleanup_callback(hammer2_io_t *, void *);

static int
hammer2_io_cmp(const hammer2_io_t *io1, const hammer2_io_t *io2)
{
	if (io1->pbase < io2->pbase)
		return (-1);
	if (io1->pbase > io2->pbase)
		return (1);
	return (0);
}

RB_GENERATE_STATIC(hammer2_io_tree, hammer2_io, rbnode, hammer2_io_cmp);
RB_SCAN_INFO(hammer2_io_tree, hammer2_io);
RB_GENERATE_SCAN_STATIC(hammer2_io_tree, hammer2_io, rbnode);

struct hammer2_cleanupcb_info {
	struct hammer2_io_tree	tmptree;
	int			count;
};

static __inline void
hammer2_assert_io_refs(const hammer2_io_t *dio)
{
	hammer2_mtx_assert_ex(&dio->lock);
	KKASSERT((dio->refs & HAMMER2_DIO_MASK) != 0);
}

/*
 * Returns the locked DIO corresponding to the data|radix offset.
 */
static hammer2_io_t *
hammer2_io_alloc(hammer2_dev_t *hmp, hammer2_key_t data_off)
{
	hammer2_volume_t *vol;
	hammer2_io_t *dio, *xio, find;
	hammer2_key_t lbase, pbase, pmask;
	uint64_t refs;
	int lsize, psize;

	hammer2_mtx_assert_ex(&hmp->iotree_lock);

	psize = HAMMER2_PBUFSIZE;
	pmask = ~(hammer2_off_t)(psize - 1);
	if ((int)(data_off & HAMMER2_OFF_MASK_RADIX))
		lsize = 1 << (int)(data_off & HAMMER2_OFF_MASK_RADIX);
	else
		lsize = 0;
	lbase = data_off & ~HAMMER2_OFF_MASK_RADIX;
	pbase = lbase & pmask;

	if (pbase == 0 || ((lbase + lsize - 1) & pmask) != pbase)
		hpanic("illegal base: %016jx %016jx+%08x / %016jx\n",
		    pbase, lbase, lsize, pmask);

	/* Access or allocate dio, bump dio->refs to prevent destruction. */
	bzero(&find, sizeof(find));
	find.pbase = pbase;
	dio = RB_FIND(hammer2_io_tree, &hmp->iotree, &find);
	if (dio) {
		hammer2_mtx_ex(&dio->lock);
		refs = atomic_fetchadd_32(&dio->refs, 1);
		if ((refs & HAMMER2_DIO_MASK) == 0)
			atomic_add_int(&dio->hmp->iofree_count, -1);
	} else {
		vol = hammer2_get_volume(hmp, pbase);
		dio = malloc(sizeof(*dio), M_HAMMER2, M_WAITOK | M_ZERO);
		dio->hmp = hmp;
		dio->devvp = vol->dev->devvp;
		dio->dbase = vol->offset;
		KKASSERT((dio->dbase & HAMMER2_FREEMAP_LEVEL1_MASK) == 0);
		dio->pbase = pbase;
		dio->psize = psize;
		dio->refs = 1;
		dio->act = 5;
		hammer2_mtx_init(&dio->lock, "h2io_inplk");
		hammer2_mtx_ex(&dio->lock);
		xio = RB_INSERT(hammer2_io_tree, &hmp->iotree, dio);
		if (xio == NULL) {
			atomic_add_long(&hammer2_dio_allocs, 1);
		} else {
			refs = atomic_fetchadd_32(&xio->refs, 1);
			if ((refs & HAMMER2_DIO_MASK) == 0)
				atomic_add_int(&xio->hmp->iofree_count, -1);
			hammer2_mtx_unlock(&dio->lock);
			hammer2_mtx_destroy(&dio->lock);
			free(dio, M_HAMMER2);
			dio = xio;
			hammer2_mtx_ex(&dio->lock);
		}
	}

	dio->ticks = ticks;
	if (dio->act < 10)
		++dio->act;

	hammer2_assert_io_refs(dio);

	return (dio);
}

/*
 * Acquire the requested dio.
 * If DIO_GOOD is set the buffer already exists and is good to go.
 */
hammer2_io_t *
hammer2_io_getblk(hammer2_dev_t *hmp, int btype, off_t lbase, int lsize, int op)
{
	hammer2_io_t *dio;
	daddr_t lblkno;
	off_t peof;
	int error, hce;

	KKASSERT(op == HAMMER2_DOP_READ);
	KKASSERT((1 << (int)(lbase & HAMMER2_OFF_MASK_RADIX)) == lsize);

	hammer2_mtx_ex(&hmp->iotree_lock);
	dio = hammer2_io_alloc(hmp, lbase);
	hammer2_assert_io_refs(dio); /* dio locked + refs > 0 */
	hammer2_mtx_unlock(&hmp->iotree_lock);

	if (dio->refs & HAMMER2_DIO_GOOD) {
		hammer2_mtx_unlock(&dio->lock);
		return (dio);
	}

	KKASSERT(dio->bp == NULL);
	if (btype == HAMMER2_BREF_TYPE_DATA)
		hce = hammer2_cluster_data_read;
	else
		hce = hammer2_cluster_meta_read;

	lblkno = (dio->pbase - dio->dbase) / DEV_BSIZE;
	if (hce > 0) {
		peof = (dio->pbase + HAMMER2_SEGMASK64) & ~HAMMER2_SEGMASK64;
		peof -= dio->dbase;
		error = cluster_read(dio->devvp, peof, lblkno, dio->psize,
		    NOCRED, HAMMER2_PBUFSIZE * hce, hce, 0, &dio->bp);
	} else {
		error = bread(dio->devvp, lblkno, dio->psize, NOCRED, &dio->bp);
	}

	if (dio->bp)
		BUF_KERNPROC(dio->bp);
	dio->error = error;
	if (error == 0)
		dio->refs |= HAMMER2_DIO_GOOD;

	hammer2_mtx_unlock(&dio->lock);

	/* XXX error handling */

	return (dio);
}

/*
 * Release our ref on *diop.
 * On the 1->0 transition we clear DIO_GOOD and dispose of dio->bp.
 */
void
hammer2_io_putblk(hammer2_io_t **diop)
{
	hammer2_dev_t *hmp;
	hammer2_io_t *dio;
	struct buf *bp;
	struct hammer2_cleanupcb_info info;
	int dio_limit;

	dio = *diop;
	*diop = NULL;

	hammer2_mtx_ex(&dio->lock);
	if ((dio->refs & HAMMER2_DIO_MASK) == 0) {
		hammer2_mtx_unlock(&dio->lock);
		return; /* lost race */
	}
	hammer2_assert_io_refs(dio);

	/*
	 * Drop refs.
	 * On the 1->0 transition clear DIO_GOOD.
	 * On any other transition we can return early.
	 */
	if ((dio->refs & HAMMER2_DIO_MASK) == 1) {
		dio->refs--;
		dio->refs &= ~HAMMER2_DIO_GOOD;
	} else {
		dio->refs--;
		hammer2_mtx_unlock(&dio->lock);
		return;
	}

	/* Lastdrop (1->0 transition) case. */
	bp = dio->bp;
	dio->bp = NULL;

	/*
	 * HAMMER2 with write support may write out buffer here,
	 * instead of just disposing of the buffer.
	 */
	if (bp)
		brelse(bp);

	/* Update iofree_count before disposing of the dio. */
	hmp = dio->hmp;
	atomic_add_int(&hmp->iofree_count, 1);

	KKASSERT(!(dio->refs & HAMMER2_DIO_GOOD));
	hammer2_mtx_unlock(&dio->lock);
	/* Another process may come in and get/put this dio. */

	/*
	 * We cache free buffers so re-use cases can use a shared lock,
	 * but if too many build up we have to clean them out.
	 */
	hammer2_mtx_ex(&hmp->iotree_lock);
	dio_limit = hammer2_dio_limit;
	if (dio_limit < 256)
		dio_limit = 256;
	if (dio_limit > 1024*1024)
		dio_limit = 1024*1024;
	if (hmp->iofree_count > dio_limit) {
		RB_INIT(&info.tmptree);
		if (hmp->iofree_count > dio_limit) {
			info.count = hmp->iofree_count / 5;
			RB_SCAN(hammer2_io_tree, &hmp->iotree, NULL,
			    hammer2_io_cleanup_callback, &info);
		}
		hammer2_io_cleanup(hmp, &info.tmptree);
	}
	hammer2_mtx_unlock(&hmp->iotree_lock);
}

/*
 * Cleanup dio with zero refs.
 */
static int
hammer2_io_cleanup_callback(hammer2_io_t *dio, void *arg)
{
	struct hammer2_cleanupcb_info *info = arg;
	hammer2_io_t *xio __diagused;
	int act;

	/* Only putblk'd dio does not require locking. */
	hammer2_mtx_ex(&dio->lock);
	if ((dio->refs & HAMMER2_DIO_MASK) == 0) {
		if (dio->act > 0) {
			act = dio->act - (ticks - dio->ticks) / hz - 1;
			if (act > 0) {
				dio->act = act;
				hammer2_mtx_unlock(&dio->lock);
				return (0);
			}
			dio->act = 0;
		}
		KKASSERT(dio->bp == NULL);
		if (info->count > 0) {
			RB_REMOVE(hammer2_io_tree, &dio->hmp->iotree, dio);
			xio = RB_INSERT(hammer2_io_tree, &info->tmptree, dio);
			KKASSERT(xio == NULL);
			--info->count;
		}
	}
	hammer2_mtx_unlock(&dio->lock);

	return (0);
}

void
hammer2_io_cleanup(hammer2_dev_t *hmp, hammer2_io_tree_t *tree)
{
	hammer2_io_t *dio;

	while ((dio = RB_ROOT(tree)) != NULL) {
		RB_REMOVE(hammer2_io_tree, tree, dio);
		KKASSERT(dio->bp == NULL &&
		    (dio->refs & HAMMER2_DIO_MASK) == 0);

		hammer2_mtx_destroy(&dio->lock);
		free(dio, M_HAMMER2);
		atomic_add_long(&hammer2_dio_allocs, -1);
		atomic_add_int(&hmp->iofree_count, -1);
	}
}

char *
hammer2_io_data(hammer2_io_t *dio, off_t lbase)
{
	struct buf *bp;
	int off;

	bp = dio->bp;
	KASSERT(bp != NULL, ("NULL dio buf"));

	lbase -= dio->dbase;
	off = (lbase & ~HAMMER2_OFF_MASK_RADIX) - bp->b_offset;
	KASSERT(off >= 0 && off < bp->b_bufsize, ("bad offset"));

	return (bp->b_data + off);
}

int
hammer2_io_bread(hammer2_dev_t *hmp, int btype, off_t lbase, int lsize,
    hammer2_io_t **diop)
{
	*diop = hammer2_io_getblk(hmp, btype, lbase, lsize, HAMMER2_DOP_READ);
	return ((*diop)->error);
}

void
hammer2_io_bqrelse(hammer2_io_t **diop)
{
	hammer2_io_putblk(diop);
}
