/*-
 * Copyright (c) 1998, 1999 Semen Ustimenko (semenu@FreeBSD.org)
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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <sys/bio.h>
#include <sys/buf.h>

#include <fs/hpfs/hpfs.h>
#include <fs/hpfs/hpfs_subr.h>

#define	AE_DONE		0		/* Nothing to change */
#define	AE_SPLIT	2		/* Split was done, ranp is valid */

int		hpfs_addextentr (struct hpfsmount *, lsn_t, alleaf_t *,
			         alnode_t *, u_long *);
int		hpfs_allocalsec (struct hpfsmount *, lsn_t, struct buf **);
int		hpfs_alblk2alsec (struct hpfsmount *, alblk_t *, alsec_t **,
				  struct buf **);
int		hpfs_splitalsec (struct hpfsmount *, alsec_t *, alsec_t **,
				 struct buf **);
int		hpfs_concatalsec (struct hpfsmount *, alsec_t *, alsec_t *,
				  alnode_t *);

/*
 * Map file offset to disk offset. hpfsnode have to be locked.
 */
int
hpfs_hpbmap(hp, bn, bnp, runp)
	struct hpfsnode *hp;
	daddr_t  bn;
	daddr_t *bnp;
	int *runp;
{
	struct buf *bp;
	alblk_t * abp;
	alleaf_t *alp;
	alnode_t *anp;
	int error, i;

	dprintf(("hpfs_hpbmap(0x%x, 0x%x): ",hp->h_no, bn));

	bp = NULL;
	abp = &hp->h_fn.fn_ab;
	alp = (alleaf_t *)&hp->h_fn.fn_abd;
	anp = (alnode_t *)&hp->h_fn.fn_abd;

dive:
	if (abp->ab_flag & AB_NODES) {
		for (i=0; i<abp->ab_busycnt; i++, anp++) {
			dprintf(("[0x%x,0x%x] ",anp->an_nextoff,anp->an_lsn));
			if (bn < anp->an_nextoff) {
				alsec_t *asp;

				dprintf(("< found | "));

				if (bp)
					brelse(bp);
				error = bread(hp->h_devvp, anp->an_lsn, 
					      DEV_BSIZE, NOCRED, &bp);
				if (error) {
					printf("hpfs_hpbmap: bread error\n");
					brelse(bp);
					return (error);
				}

				asp = (alsec_t *) bp->b_data;
				if (asp->as_magic != AS_MAGIC) {
					brelse(bp);
					printf("hpfs_hpbmap: "
					       "MAGIC DOESN'T MATCH");
					return (EINVAL);
				}

				abp = &asp->as_ab;
				alp = (alleaf_t *)&asp->as_abd;
				anp = (alnode_t *)&asp->as_abd;

				goto dive;
			}
		}
	} else {
		for (i=0; i<abp->ab_busycnt; i++, alp++) {
			dprintf(("[0x%x,0x%x,0x%x] ",
				 alp->al_off,alp->al_len,alp->al_lsn));

			if ((bn >= alp->al_off) &&
			    (!alp->al_len || (bn < alp->al_off + alp->al_len))) {
				dprintf(("found, "));

				*bnp = bn - alp->al_off + alp->al_lsn;

				dprintf((" 0x%x ", *bnp));

				if (runp != NULL) {
					if (alp->al_len)
						*runp = alp->al_off - 1 +
							alp->al_len - bn;
					else
						*runp = 3; /* XXX */

					dprintf((" 0x%x cont", *runp));
				}

				if (bp)
					brelse(bp);

				dprintf(("\n"));
				return (0);
			}
		}
	}

	dprintf(("END, notfound\n"));
	if (bp)
		brelse(bp);

	dprintf(("hpfs_hpbmap: offset too big\n"));

	return (EFBIG);
}

/*
 * Find place and preinitialize AlSec structure
 * AlBlk is initialized to contain AlLeafs.
 */
int
hpfs_allocalsec (
	struct hpfsmount *hpmp,
	lsn_t parlsn,
	struct buf **bpp)
{
	alsec_t * asp;
	struct buf * bp;
	lsn_t lsn;
	int error;

	*bpp = NULL;

	error = hpfs_bmfblookup(hpmp, &lsn);
	if (error) {
		printf("hpfs_allocalsec: CAN'T ALLOC SPACE FOR AlSec\n");
		return (error);
	}

	error = hpfs_bmmarkbusy(hpmp, lsn, 1);
	if (error) 
		return (error);

	bp = getblk(hpmp->hpm_devvp, lsn, DEV_BSIZE, 0, 0, 0);
	clrbuf(bp);

	/* Fill AlSec info */
	asp = (alsec_t *) bp->b_data;
	asp->as_magic = AS_MAGIC;
	asp->as_self = lsn;
	asp->as_parent = parlsn;

	/* Fill AlBlk */
	asp->as_ab.ab_flag = 0;
	asp->as_ab.ab_busycnt = 0;
	asp->as_ab.ab_freecnt = 0x28;
	asp->as_ab.ab_freeoff = sizeof(alblk_t);

	*bpp = bp;

	return (0);
}

/*
 * Split AlSec structure into new allocated:
 * allocate new AlSec; then move second half of asp's entries in
 * into it; set proper flags.
 *
 * IF AlSec CONTAINS AlNodes, THEN YOU ALMOST EVERYTIME HAVE TO
 * FIX LAST AlNode in OLD AlSec (NEXTOFF TO BE 0xFFFFFFFF).
 * TOGETHER WITH FIXING ALL CHILDREN'S AlSecs (THEY HAVE GOT NEW PARENT).
 */
int
hpfs_splitalsec (
	struct hpfsmount *hpmp,
	alsec_t *asp,
	alsec_t **naspp,
	struct buf **nbpp)
{
	alsec_t *nasp;
	struct buf *nbp;
	alblk_t *abp;
	alblk_t *nabp;
	int error, n1, n2, sz;

	error = hpfs_allocalsec(hpmp, asp->as_parent, &nbp);
	if (error)
		return (error);

	nasp = (alsec_t *)nbp->b_data;
	nabp = &nasp->as_ab;
	abp = &asp->as_ab;

	n1 = (abp->ab_busycnt + 1) / 2;
	n2 = (abp->ab_busycnt - n1);
	sz = (abp->ab_flag & AB_NODES) ? sizeof(alnode_t) : sizeof(alleaf_t);

	bcopy((caddr_t)abp + sizeof(alblk_t) + n1 * sz, 
	      (caddr_t)nabp + sizeof(alblk_t), n2 * sz);

	nabp->ab_flag = abp->ab_flag;
	nabp->ab_busycnt = n2;
	nabp->ab_freecnt = (0x1e0 / sz - n2);
	nabp->ab_freeoff += n2 * sz;

	abp->ab_busycnt -= n1;
	abp->ab_freecnt += n1;
	abp->ab_freeoff -= n1 * sz;

	*naspp = nasp;
	*nbpp = nbp;

	return (0);
}

/*
 * Try to concatenate two AlSec's
 *
 * Moves all entries from AlSec corresponding (as1p, aanp[1]) into 
 * corresponding aanp[0] one. If not enought space, then return ENOSPC.
 *
 * WARNING! YOU HAVE TO FIX aanp VALUES YOURSELF LATER:
 * aanp[0].an_nextoff = aanp[1].an_nextoff;
 */
int
hpfs_concatalsec (
	struct hpfsmount *hpmp,
	alsec_t *as0p,
	alsec_t *as1p,
	alnode_t *aanp)
{
	alblk_t *ab0p;
	alblk_t *ab1p;
	int sz;
	
	dprintf(("hpfs_concatalsec: AlSecs at 0x%x and 0x%x \n",
		as0p->as_self,as1p->as_self));

	ab0p = &as0p->as_ab;
	ab1p = &as1p->as_ab;
	sz = (ab0p->ab_flag & AB_NODES) ? sizeof(alnode_t) : sizeof(alleaf_t);

	if (ab0p->ab_freecnt > ab1p->ab_busycnt) {
		/*
		 * Concatenate AlSecs
		 */
		if (ab0p->ab_flag & AB_NODES) 
			AB_LASTANP(ab0p)->an_nextoff = aanp[0].an_nextoff;

		bcopy (AB_ALNODE(ab1p), AB_FREEANP(ab0p),
			 ab1p->ab_busycnt * sz);

		AB_ADDNREC(ab0p, sz, ab1p->ab_busycnt);

		return (0);
	} else {
		/* Not enought space to concatenate */
		return (ENOSPC);
	}
}

/*
 * Transform AlBlk structure into new allocated 
 * AlSec.
 *
 * DOESN'T SET AlSec'S PARENT LSN.
 */
int
hpfs_alblk2alsec (
	struct hpfsmount *hpmp,
	alblk_t *abp,
	alsec_t **naspp,
	struct buf **nbpp)
{
	alsec_t *nasp;
	alblk_t *nabp;
	struct buf *nbp;
	int error, sz;

	error = hpfs_allocalsec(hpmp, 0, &nbp);
	if (error)
		return (error);

	nasp = (alsec_t *)nbp->b_data;
	nabp = &nasp->as_ab;

	sz = (abp->ab_flag & AB_NODES) ? sizeof(alnode_t) : sizeof(alleaf_t);

	bcopy (abp, nabp, sizeof(alblk_t) + sz * abp->ab_busycnt);

	nabp->ab_freecnt = 0x1e0 / sz - nabp->ab_busycnt;

	*naspp = nasp;
	*nbpp = nbp;

	return (0);
}

/*
 * Allocate len blocks and concatenate them to file.
 * If we hadn't found contignous run of len blocks, concatenate
 * as much as we can, and return.
 * 
 */
int
hpfs_addextent (
	struct hpfsmount *hpmp,
	struct hpfsnode *hp,
	u_long len)
{
	alblk_t *rabp;
	alnode_t ranp[2];
	alleaf_t al;
	int error;
	u_long pf;

	/*
	 * We don't know for now start lsn of block
	 */
	al.al_lsn = ~0;
	al.al_len = len;
	al.al_off = (hp->h_fn.fn_size + DEV_BSIZE - 1) >> DEV_BSHIFT;

	rabp = &hp->h_fn.fn_ab;

	/* Init AlBlk if this is first extent */
	if (al.al_off == 0) {
		lsn_t nlsn;
		u_long nlen;

		dprintf(("hpfs_addextent: init AlBlk in root\n"));

		rabp->ab_busycnt = 0;
		rabp->ab_freecnt = 0x8;
		rabp->ab_freeoff = sizeof(alblk_t);
		rabp->ab_flag = 0;

		error = hpfs_bmlookup (hpmp, 0, hp->h_no + 1, al.al_len, &nlsn, &nlen);
		if (error)
			return (error);

		error = hpfs_bmmarkbusy(hpmp, nlsn, nlen);
		if (error)
			return (error);
						
		dprintf(("hpfs_addextent: new: 0x%x 0x%lx, ", nlsn, nlen));

		AL_SET(AB_FREEALP(rabp), al.al_off, nlen, nlsn);
		AB_ADDAL(rabp);

		al.al_off += nlen;
		al.al_len -= nlen;
	}

retry:
	dprintf(("hpfs_addextent: AlBlk: [0x%x, 0x%x, 0x%x] need: 0x%x\n",
		 rabp->ab_freecnt, rabp->ab_busycnt, rabp->ab_flag, al.al_len));

	while ((al.al_len) && (rabp->ab_freecnt > 0)) {
		if (rabp->ab_flag & AB_NODES) {
			alnode_t *anp;
			/*
			 * This is level containing AlNodes, so try to 
			 * insert recursively into last entry.
			 */
			anp = AB_LASTANP(rabp);
			dprintf(("hpfs_addextent: AlNode: [0x%x,0x%x] \n",
				 anp->an_nextoff,anp->an_lsn));

			/*
			 * Try to insert...
			 */
			error = hpfs_addextentr (hpmp, anp->an_lsn, &al, ranp, &pf);
			if (error) {
				printf("hpfs_addextent: FAILED %d\n",error);
				return (error);
			}

			switch (pf) {
			case AE_SPLIT:
				dprintf(("hpfs_addextent: successful (split)\n"));
				/*
				 * Then hpfs_addextentr has split tree below, now
				 * we need to fix this level. Particulary:
				 * fix last AlNode and add another one.
				 */

				bcopy(ranp, AB_LASTANP(rabp), sizeof(alnode_t) * 2);
				AB_ADDAN(rabp);
				break;

			default:
			case AE_DONE:
				dprintf(("hpfs_addextent: successful\n"));
				break;
			}
		} else {
			alleaf_t *alp;

			alp = AB_LASTALP(rabp);
			dprintf(("hpfs_addextent: AlLeaf: [0x%x,0x%x,0x%x] \n",
				 alp->al_off,alp->al_len,alp->al_lsn));

			/* Check if we trying to add in right place */
			if (alp->al_off + alp->al_len == al.al_off) {
				lsn_t nlsn;
				u_long nlen;

				/*
				 * Search bitmap for block begining from
				 * alp->al_lsn + alp->al_len and long of ralp->al_len
				 */
				error = hpfs_bmlookup (hpmp, 0,
					alp->al_lsn + alp->al_len, al.al_len, &nlsn, &nlen);
				if (error)
					return (error);

				error = hpfs_bmmarkbusy(hpmp, nlsn, nlen);
				if (error)
					return (error);
						
				dprintf(("hpfs_addextent: new: 0x%x 0x%lx, ", nlsn, nlen));

				if (alp->al_lsn + alp->al_len == nlsn) {
					dprintf(("extended existed leaf\n"));

					alp->al_len += nlen;
				} else {
					dprintf(("created new leaf\n"));
					AL_SET(AB_FREEALP(rabp), al.al_off, nlen, nlsn);
					AB_ADDAL(rabp);
				}
				al.al_off += nlen;
				al.al_len -= nlen;
			} else {
				printf("hpfs_addextent: INTERNAL INCONSISTENCE\n");
				return (EINVAL);
			}
		}
	}

	/*
	 * Move AlBlk contain to new AlSec (it will fit more
	 * entries) if overflowed (no more free entries).
	 */
	if (rabp->ab_freecnt <= 0) {
		struct buf *nbp;
		alsec_t * nrasp;

		dprintf(("hpfs_addextent: overflow, convt\n"));

		/*
		 * Convert AlBlk to new AlSec, it will set
		 * AB_FNPARENT also.
		 */
		rabp->ab_flag |= AB_FNPARENT;
		error = hpfs_alblk2alsec (hpmp, rabp, &nrasp, &nbp);
		if (error) {
			printf("hpfs_addextent: CAN'T CONVT\n");
			return (error);
		}
		nrasp->as_parent = hp->h_no;

		/*
		 * Scan all childrens (if exist), set new parent and
		 * clean their AB_FNPARENT flag.
		 */
		if (rabp->ab_flag & AB_NODES) {
			int i;
			alsec_t * asp;
			alnode_t * anp;
			struct buf * bp;

			anp = AB_ALNODE(rabp);
			for (i=0; i<rabp->ab_busycnt; i++) {
				error = hpfs_breadalsec(hpmp, anp->an_lsn, &bp);
				if (error)
					return (error);

				asp = (alsec_t *)bp->b_data;
				asp->as_ab.ab_flag &= ~AB_FNPARENT;
				asp->as_parent = nrasp->as_self;

				bdwrite(bp);
				anp ++;
			}
		}

		/* Convert AlBlk to contain AlNodes */
		rabp->ab_flag = AB_NODES;
		rabp->ab_busycnt = 0;
		rabp->ab_freecnt = 0xC;
		rabp->ab_freeoff = sizeof(alblk_t);

		/* Add AlNode for new allocated AlSec */
		AN_SET(AB_FREEANP(rabp), ~0, nrasp->as_self);
		AB_ADDAN(rabp);

		bdwrite(nbp);
	}

	if (al.al_len) {
		dprintf(("hpfs_addextent: root retry\n"));
		goto retry;
	}

	return (0);
}

/*
 * Descent down to the end of tree, then search for
 * ralp->len contignous run begining from last run's end and
 * concatenate new block! If we can't find one, then...
 */
int
hpfs_addextentr (
	struct hpfsmount *hpmp,		/* Mix info */
	lsn_t rlsn,			/* LSN containing AlSec */
	alleaf_t *ralp,			/* AlLeaf to insert */
	alnode_t *ranp,			/* New AlNodes' values */
	u_long *resp)			/* Mix returning info */
{
	struct buf *rbp;
	alsec_t *rasp;
	alblk_t *rabp;
	alleaf_t *alp;
	alnode_t *anp;
	int error;
	u_long pf;
	u_long wb;

	*resp = 0;

	dprintf(("hpfs_addextentr: AlSec at 0x%x\n", rlsn));

	error = hpfs_breadalsec(hpmp, rlsn, &rbp);
	if (error)
		return (error);

	rasp = (alsec_t *)rbp->b_data;
	rabp = &rasp->as_ab;
	wb = 0;

	dprintf(("hpfs_addextentr: AlBlk: [0x%x, 0x%x, 0x%x]\n",
		 rabp->ab_freecnt, rabp->ab_busycnt, rabp->ab_flag));

	while ((ralp->al_len) && (rabp->ab_freecnt > 0)) {
		if (rabp->ab_flag & AB_NODES) {
			/*
			 * This is level containing AlNodes, so try to 
			 * insert recursively into last entry.
			 */
			anp = AB_LASTANP(rabp);
			dprintf(("hpfs_addextentr: AlNode: [0x%x,0x%x] \n",
				 anp->an_nextoff,anp->an_lsn));

			/*
			 * Try to insert...
			 */
			error = hpfs_addextentr (hpmp, anp->an_lsn, ralp, ranp, &pf);
			if (error) {
				printf("hpfs_addextentr: FAILED %d\n",error);
				goto fail;
			}

			switch (pf) {
			case AE_SPLIT:
				dprintf(("hpfs_addextentr: successful (split)\n"));
				/*
				 * Then hpfs_addextentr has split tree below, now
				 * we need to fix this level. Particulary:
				 * fix last AlNode and add another one.
				 */
				bcopy(ranp, AB_LASTANP(rabp), sizeof(alnode_t) * 2);
				AB_ADDAN(rabp);
				wb = 1;
				break;

			default:
			case AE_DONE:
				dprintf(("hpfs_addextentr: successful\n"));
				break;		
			}
		} else {
			alp = AB_LASTALP(rabp);
			dprintf(("hpfs_addextentr: AlLeaf: [0x%x,0x%x,0x%x] \n",
				 alp->al_off,alp->al_len,alp->al_lsn));

			/* Check if we trying to add in right place */
			if (alp->al_off + alp->al_len == ralp->al_off) {
				lsn_t nlsn;
				u_long nlen;
				/*
				 * Search bitmap for block begining from
				 * alp->al_lsn + alp->al_len and long of ralp->al_len
				 */
				error = hpfs_bmlookup (hpmp, 0,
					alp->al_lsn + alp->al_len, ralp->al_len, &nlsn, &nlen);
				if (error)
					goto fail;

				error = hpfs_bmmarkbusy(hpmp, nlsn, nlen);
				if (error)
					goto fail;
						
				dprintf(("hpfs_addextentr: new: 0x%x 0x%lx, ", nlsn, nlen));

				/* 
				 * If ending of existed entry fits the
				 * begining of the extent being added,
				 * then we add concatenate two extents.
				 */
				if (alp->al_lsn + alp->al_len == nlsn) {
					dprintf(("concat\n"));
					alp->al_len += nlen;
				} else {
					dprintf(("created new leaf\n"));
					AL_SET(AB_FREEALP(rabp), ralp->al_off, nlen, nlsn);
					AB_ADDAL(rabp);
				}

				ralp->al_len -= nlen;
				ralp->al_off += nlen;
			} else {
				printf("hpfs_addextentr: INTERNAL INCONSISTENCE\n");
				error = (EINVAL);
				goto fail;
			}
		}
	}

	/*
	 * Split AlBlk if overflowed.
	 */
	if (rabp->ab_freecnt <= 0) {
		struct buf *nbp;
		alsec_t * nrasp;

		dprintf(("hpfs_addextentr: overflow, split\n"));

		error = hpfs_splitalsec (hpmp, rasp, &nrasp, &nbp);
		if (error) {
			printf("hpfs_addextent: CAN'T SPLIT\n");
			goto fail;
		}

		if (rabp->ab_flag & AB_NODES) {
			int i;
			alsec_t * asp;
			alnode_t * anp;
			struct buf * bp;

			ranp[0].an_nextoff = 
				AB_LASTANP(&rasp->as_ab)->an_nextoff;

			/* We need to set left subtree's last entry
			 * offset to 0xFFFFFFFF for OS/2 to be able
			 * to read our files. It treats absence  of
			 * 0xFFFFFFFF as error.
			 */
			AB_LASTANP(&rasp->as_ab)->an_nextoff = ~0;

			/* We need to fix new allocated AlSec's
			 * children, becouse their parent has changed.
			 */
			anp = AB_ALNODE(&nrasp->as_ab);
			for (i=0; i<nrasp->as_ab.ab_busycnt; i++) {
				error = hpfs_breadalsec(hpmp, anp->an_lsn, &bp);
				if (error) {
					brelse(nbp);
					goto fail;
				}

				asp = (alsec_t *)bp->b_data;
				asp->as_parent = nrasp->as_self;

				bdwrite(bp);
				anp ++;
			}
		} else {
			ranp[0].an_nextoff = 
				AB_ALLEAF(&nrasp->as_ab)->al_off;
		}

		ranp[0].an_lsn = rasp->as_self;
		ranp[1].an_nextoff = ~0;
		ranp[1].an_lsn = nrasp->as_self;

		bdwrite(nbp);

		*resp = AE_SPLIT;
		wb = 1;
	}

	if (wb)
		bdwrite (rbp);
	else
		brelse(rbp);

	return (0);

fail:
	brelse(rbp);

	return (error);
}

/*
 * Recursive routine walking down the b-tree and deallocating all
 * extents above bn. Returns *resp != 0 if alblk was totally 
 * deallocated and may be freed. Tries to keep b-tree.
 *
 * (XXXX) NOTE! THIS ROUTINE WILL NEVER DECREMENT DEPTH OF
 * THE TREE.
 */
int
hpfs_truncatealblk (
	struct hpfsmount *hpmp,
	alblk_t *abp,
	lsn_t bn,
	int *resp)
{
	int error;
	alleaf_t *alp;
	alnode_t *anp;
	alsec_t *asp;
	struct buf *bp;

	dprintf(("hpfs_truncatealblk: AlBlk: [0x%x,0x%x, 0x%x]\n",
		 abp->ab_freecnt, abp->ab_busycnt, abp->ab_flag));

	if (abp->ab_flag & AB_NODES) {
		/*
		 * Scan array of AlNodes backward,
		 * diving in recursion if needed
		 */
		anp = AB_LASTANP(abp);

		while (abp->ab_busycnt && (bn <= anp->an_nextoff)) {
			dprintf(("hpfs_truncatealblk: AlNode: [0x%x,0x%x] \n",
				anp->an_nextoff,anp->an_lsn));

			error = hpfs_breadalsec(hpmp, anp->an_lsn, &bp);
			if (error)
				return (error);

			asp = (alsec_t *)bp->b_data;

			error = hpfs_truncatealblk (hpmp,
					&asp->as_ab, bn, resp);
			if (error) {
				brelse(bp);
				return (error);
			}

			if (*resp) {
				brelse (bp);

				error = hpfs_bmmarkfree(hpmp,
						anp->an_lsn, 1);
				if (error)
					return (error);

				AB_RMAN(abp);
				anp --;
			} else {
				/* 
				 * We have deallocated some entries, some space
				 * migth been freed, then try to concat two 
				 * last AlSec.
				 */
				anp->an_nextoff = ~0;
				if (abp->ab_busycnt >= 2) {
					alsec_t *as0p;
					struct buf *b0p;

					error = hpfs_breadalsec(hpmp,
							(anp-1)->an_lsn, &b0p);
					if (error)
						return (error);

					as0p = (alsec_t *)b0p->b_data;
					error = hpfs_concatalsec(hpmp,
							as0p, asp, anp - 1);
					if (error == ENOSPC) {
						/* Not enought space */
						brelse (b0p);
						bdwrite (bp);
					} else if (error == 0) {
						/* All OK  */
						(anp-1)->an_nextoff = anp->an_nextoff;

						bdwrite (b0p);
						brelse (bp);

						error = hpfs_bmmarkfree(hpmp,
								anp->an_lsn, 1);
						if (error)
							return (error);

						AB_RMAN(abp);
					} else {
						/* True error */
						brelse (b0p);
						brelse (bp);
						return (error);
					}
				} else {
					/* Nowhere to concatenate */
					bdwrite (bp);
				}

				/* There can not be any more entries
				 * over greater bn, becouse last AlSec
				 * wasn't freed totally. So go out.
				 */
				break;
			}
		}

		if (abp->ab_busycnt == 0)
			*resp = 1;
		else
			*resp = 0;
	} else {
		/*
		 * Scan array of AlLeafs backward,
		 * free all above bn.
		 */
		alp = AB_LASTALP(abp);

		while (abp->ab_busycnt && (bn < alp->al_off + alp->al_len)){
			dprintf(("hpfs_truncatealblk: AlLeaf: [0x%x,0x%x,0x%x] \n",
				 alp->al_off,alp->al_len,alp->al_lsn));

			if (bn <= alp->al_off) {
				error = hpfs_bmmarkfree(hpmp, alp->al_lsn,
						alp->al_len);
				if (error)
					return (error);

				AB_RMAL(abp);
				alp --;
			} else if ((bn > alp->al_off) &&
			    	   (bn < alp->al_off + alp->al_len)){
				error = hpfs_bmmarkfree(hpmp,
						alp->al_lsn + bn - alp->al_off,
						alp->al_len - bn + alp->al_off);
				if (error)
					return (error);

				alp->al_len = bn - alp->al_off;

				break;
			} else
				break;
		}
	}

	/* Signal parent deallocation, if need */
	if (abp->ab_busycnt == 0) 
		*resp = 1;
	else
		*resp = 0;

	return (0);
}
