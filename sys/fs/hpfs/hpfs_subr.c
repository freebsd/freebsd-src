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
#include <fs/hpfs/hpfsmount.h>
#include <fs/hpfs/hpfs_subr.h>

u_long
hpfs_checksum(
	u_int8_t *object,
	int size)
{
	register int i;
	u_long csum=0L;
	for (i=0; i < size; i++) {
		csum += (u_long) *object++;
		csum = (csum << 7) + (csum >> (25));
	}
	return (csum);
}

void
hpfs_bmdeinit(
	struct hpfsmount *hpmp)
{
	struct buf *bp;
	int i;

	dprintf(("hpmp_bmdeinit: "));

	if (!(hpmp->hpm_mp->mnt_flag & MNT_RDONLY)) {
		/*
		 * Write down BitMap.
		 */
		for (i=0; i<hpmp->hpm_dbnum; i++) {
			dprintf(("[%d: 0x%x] ", i, hpmp->hpm_bmind[i]));

			bp = getblk(hpmp->hpm_devvp, hpmp->hpm_bmind[i],
				    BMSIZE, 0, 0, 0);
			clrbuf(bp);

			bcopy(hpmp->hpm_bitmap + BMSIZE * i, bp->b_data,
			      BMSIZE);

			bwrite(bp);
		}
	}

	FREE(hpmp->hpm_bitmap,M_HPFSMNT);
	FREE(hpmp->hpm_bmind,M_HPFSMNT);

	dprintf(("\n"));
}

/*
 * Initialize BitMap management, includes calculation of
 * available blocks number.
 */
int
hpfs_bminit(
	struct hpfsmount *hpmp)
{
	struct buf *bp;
	int error, i, k;
	u_long dbavail;

	dprintf(("hpfs_bminit: "));

	hpmp->hpm_dbnum = (hpmp->hpm_su.su_btotal + 0x3FFF) / 0x4000;

	dprintf(("0x%lx data bands, ", hpmp->hpm_dbnum));

	MALLOC(hpmp->hpm_bmind, lsn_t *, hpmp->hpm_dbnum * sizeof(lsn_t),
		M_HPFSMNT, M_WAITOK);

	MALLOC(hpmp->hpm_bitmap, u_int8_t *, hpmp->hpm_dbnum * BMSIZE,
		M_HPFSMNT, M_WAITOK);

	error = bread(hpmp->hpm_devvp, hpmp->hpm_su.su_bitmap.lsn1,
		((hpmp->hpm_dbnum + 0x7F) & ~(0x7F)) << 2, NOCRED, &bp);
	if (error) {
		brelse(bp);
		FREE(hpmp->hpm_bitmap, M_HPFSMNT);
		FREE(hpmp->hpm_bmind, M_HPFSMNT);
		dprintf((" error %d\n", error));
		return (error);
	}
	bcopy(bp->b_data, hpmp->hpm_bmind, hpmp->hpm_dbnum * sizeof(lsn_t));
	
	brelse(bp);

	/*
	 * Read in all BitMap
	 */
	for (i=0; i<hpmp->hpm_dbnum; i++) {
		dprintf(("[%d: 0x%x] ", i, hpmp->hpm_bmind[i]));

		error = bread(hpmp->hpm_devvp, hpmp->hpm_bmind[i],
				BMSIZE, NOCRED, &bp);
		if (error) {
			brelse(bp);
			FREE(hpmp->hpm_bitmap, M_HPFSMNT);
			FREE(hpmp->hpm_bmind, M_HPFSMNT);
			dprintf((" error %d\n", error));
			return (error);
		}
		bcopy(bp->b_data, hpmp->hpm_bitmap + BMSIZE * i, BMSIZE);

		brelse(bp);
	}

	/*
	 * Look througth BitMap	and count free bits
	 */
	dbavail = 0;
	for (i=0; i < hpmp->hpm_su.su_btotal >> 5; i++) {
		register u_int32_t mask;
		for (k=0, mask=1; k < 32; k++, mask<<=1)
			if(((u_int32_t *)hpmp->hpm_bitmap)[i] & mask) 
				dbavail ++;

	}
	hpmp->hpm_bavail = dbavail;

	return (0);
}

int
hpfs_cmpfname (
	struct hpfsmount *hpmp,
	char * uname,
	int ulen,
	char * dname,
	int dlen,
	u_int16_t cp)
{
	register int i, res;

	for (i = 0; i < ulen && i < dlen; i++) {
		res = hpfs_toupper(hpmp, hpfs_u2d(hpmp, uname[i]), cp) - 
		      hpfs_toupper(hpmp, dname[i], cp);
		if (res)
			return res;
	}
	return (ulen - dlen);
}

int
hpfs_cpstrnnicmp (
	struct hpfsmount *hpmp,
	char * str1,
	int str1len,
	u_int16_t str1cp,
	char * str2,
	int str2len,
	u_int16_t str2cp)
{
	int i, res;

	for (i = 0; i < str1len && i < str2len; i++) {
		res = (int)hpfs_toupper(hpmp, ((u_char *)str1)[i], str1cp) - 
		      (int)hpfs_toupper(hpmp, ((u_char *)str2)[i], str2cp);
		if (res)
			return res;
	}
	return (str1len - str2len);
}


int
hpfs_cpload (
	struct hpfsmount *hpmp,
	struct cpiblk *cpibp,
	struct cpdblk *cpdbp)
{
	struct buf *bp;
	struct cpdsec * cpdsp;
	int error, i;

	error = bread(hpmp->hpm_devvp, cpibp->b_cpdsec, DEV_BSIZE, NOCRED, &bp);
	if (error) {
		brelse(bp);
		return (error);
	}

	cpdsp = (struct cpdsec *)bp->b_data;

	for (i=cpdsp->d_cpfirst; i<cpdsp->d_cpcnt; i++) {
		if (cpdsp->d_cpdblk[i].b_cpid == cpibp->b_cpid) {
			bcopy(cpdsp->d_cpdblk + i, cpdbp, 
			      sizeof(struct cpdblk));

			brelse(bp);

			return (0);
		}
	}

	brelse(bp);

	return (ENOENT);
}


/*
 * Initialize Code Page information management.
 * Load all copdepages in memory.
 */
int
hpfs_cpinit (
	struct hpfsmount *hpmp,
	struct hpfs_args *argsp)
{
	struct buf *bp;
	int error, i;
	lsn_t lsn;
	int cpicnt;
	struct cpisec * cpisp;
	struct cpiblk * cpibp;
	struct cpdblk * cpdbp;

	dprintf(("hpfs_cpinit: \n"));

	if (argsp->flags & HPFSMNT_TABLES) {
		bcopy(argsp->d2u, hpmp->hpm_d2u, sizeof(u_char) * 0x80);
		bcopy(argsp->u2d, hpmp->hpm_u2d, sizeof(u_char) * 0x80);
	} else {
		for (i=0x0; i<0x80;i++) {
			hpmp->hpm_d2u[i] = i + 0x80;
			hpmp->hpm_u2d[i] = i + 0x80;
		}
	}

	cpicnt = hpmp->hpm_sp.sp_cpinum;

	MALLOC(hpmp->hpm_cpdblk, struct cpdblk *,	
		cpicnt * sizeof(struct cpdblk), M_HPFSMNT, M_WAITOK);

	cpdbp = hpmp->hpm_cpdblk;
	lsn = hpmp->hpm_sp.sp_cpi;

	while (cpicnt > 0) {
		error = bread(hpmp->hpm_devvp, lsn, DEV_BSIZE, NOCRED, &bp);
		if (error) {
			brelse(bp);
			return (error);
		}

		cpisp = (struct cpisec *)bp->b_data;

		cpibp = cpisp->s_cpi;
		for (i=0; i<cpisp->s_cpicnt; i++, cpicnt --, cpdbp++, cpibp++) {
			dprintf(("hpfs_cpinit: Country: %d, CP: %d (%d)\n",
				 cpibp->b_country, cpibp->b_cpid, 
				 cpibp->b_vcpid));

			error = hpfs_cpload(hpmp, cpibp, cpdbp);
			if (error) {
				brelse(bp);
				return (error);
			}
		}
		lsn = cpisp->s_next;
		brelse(bp);
	}

	return (0);
}

int
hpfs_cpdeinit (
	struct hpfsmount *hpmp)
{
	dprintf(("hpmp_cpdeinit: "));
	FREE(hpmp->hpm_cpdblk,M_HPFSMNT);
	return (0);
}

/*
 * Lookup for a run of blocks.
 */
int
hpfs_bmlookup (
	struct hpfsmount *hpmp,
	u_long flags,	/* 1 means we want right len blocks in run, not less */
	lsn_t lsn,		/* We want near this one */
	u_long len,		/* We want such long */
	lsn_t *lsnp,	/* We got here */
	u_long *lenp)	/* We got this long */
{
	u_int32_t * bitmap;
	register u_int32_t mask;
	int i,k;
	int cband, vcband;
	u_int bandsz;
	int count;

	dprintf(("hpfs_bmlookup: lsn: 0x%x, len 0x%lx | Step1\n", lsn, len));

	if (lsn > hpmp->hpm_su.su_btotal) {
		printf("hpfs_bmlookup: OUT OF VOLUME\n");
		return ENOSPC;
	}
	if (len > hpmp->hpm_bavail) {
		printf("hpfs_bmlookup: OUT OF SPACE\n");
		return ENOSPC;
	}
 	i = lsn >> 5;
	k = lsn & 0x1F;
	mask = 1 << k;
	bitmap = (u_int32_t *)hpmp->hpm_bitmap + i;

	if (*bitmap & mask) {
		*lsnp = lsn;
		*lenp = 0;
		for (; k < 32; k++, mask<<=1) {
			if (*bitmap & mask)
				(*lenp) ++;
			else {
				if (flags & 1)
					goto step2;
				else 
					return (0);
			}

			if (*lenp == len)
				return (0);
		}

		bitmap++;
		i++;
		for (; i < hpmp->hpm_su.su_btotal >> 5; i++, bitmap++) {
			for (k=0, mask=1; k < 32; k++, mask<<=1) {
				if (*bitmap & mask)
					(*lenp) ++;
				else {
					if (flags & 1)
						goto step2;
					else 
						return (0);
				}

				if (*lenp == len)
					return (0);
			}
		}
		return (0);
	}

step2:
	/*
	 * Lookup all bands begining from cband, lookup for first block
	 */
	cband = (lsn >> 14);
	dprintf(("hpfs_bmlookup: Step2: band 0x%x (0x%lx)\n",
		 cband, hpmp->hpm_dbnum));
	for (vcband = 0; vcband < hpmp->hpm_dbnum; vcband ++, cband++) {
		cband = cband % hpmp->hpm_dbnum;
		bandsz = min (hpmp->hpm_su.su_btotal - (cband << 14), 0x4000);
		dprintf(("hpfs_bmlookup: band: %d, sz: 0x%x\n", cband, bandsz));

		bitmap = (u_int32_t *)hpmp->hpm_bitmap + (cband << 9);
		*lsnp = cband << 14;
		*lenp = 0;
		count = 0;
		for (i=0; i < bandsz >> 5; i++, bitmap++) {
			for (k=0, mask=1; k < 32; k++, mask<<=1) {
				if (*bitmap & mask) {
					if (count) {
						(*lenp) ++;
					} else {
						count = 1;
						*lsnp = (cband << 14) + (i << 5) + k;
						*lenp = 1;
					}
				} else {
					if ((*lenp) && !(flags & 1)) {
						return (0);
					} else {
						count = 0;
					}
				}

				if (*lenp == len)
					return (0);
			}
		}
		if (cband == hpmp->hpm_dbnum - 1)  {
			if ((*lenp) && !(flags & 1)) {
				return (0);
			} else {
				count = 0;
			}
		}
	}

	return (ENOSPC);
}

/*
 * Lookup a single free block.	XXX Need locking on BitMap operations
 * VERY STUPID ROUTINE!!!
 */
int
hpfs_bmfblookup (
	struct hpfsmount *hpmp,
	lsn_t *lp)
{
	u_int32_t * bitmap;
	int i,k;

	dprintf(("hpfs_bmfblookup: "));

	bitmap = (u_int32_t *)hpmp->hpm_bitmap;
	for (i=0; i < hpmp->hpm_su.su_btotal >> 5; i++, bitmap++) {
		k = ffs(*bitmap);
		if (k) {
			*lp = (i << 5) + k - 1;
			dprintf((" found: 0x%x\n",*lp));
			return (0);
		}
	}

	return (ENOSPC);
}

/*
 * Mark contignous block of blocks.
 */
int
hpfs_bmmark (
	struct hpfsmount *hpmp,
	lsn_t bn,
	u_long bl,
	int state)
{
	u_int32_t * bitmap;
	int i, didprint = 0;

	dprintf(("hpfs_bmmark(0x%x, 0x%lx, %d): \n",bn,bl, state));

	if ((bn > hpmp->hpm_su.su_btotal) || (bn+bl > hpmp->hpm_su.su_btotal)) {
		printf("hpfs_bmmark: MARKING OUT OF VOLUME\n");
		return 0;
	}
	bitmap = (u_int32_t *)hpmp->hpm_bitmap;
	bitmap += bn >> 5;

	while (bl > 0) {
		for (i = bn & 0x1F; (i < 0x20) && (bl > 0) ; i++, bl--) {
			if (state) {
				if ( *bitmap & (1 << i)) {
					if (!didprint) {
						printf("hpfs_bmmark: ALREADY FREE\n");
						didprint = 1;
					}
				} else 
					hpmp->hpm_bavail++;

				*bitmap |= (1 << i);
			} else {
				if ((~(*bitmap)) & (1 << i)) {
					if (!didprint) {
						printf("hpfs_bmmark: ALREADY BUSY\n");
						didprint = 1;
					}
				} else 
					hpmp->hpm_bavail--;

				*bitmap &= ~(1 << i);
			}
		}
		bn = 0;
		bitmap++;
	}

	return (0);
}


int
hpfs_validateparent (
	struct hpfsnode *hp)
{
	struct hpfsnode *dhp;
	struct vnode *dvp;
	struct hpfsmount *hpmp = hp->h_hpmp;
	struct buf *bp;
	struct dirblk *dp;
	struct hpfsdirent *dep;
	lsn_t lsn, olsn;
	int level, error;

	dprintf(("hpfs_validatetimes(0x%x): [parent: 0x%x] ",
		hp->h_no, hp->h_fn.fn_parent));

	if (hp->h_no == hp->h_fn.fn_parent) {
		dhp = hp;
	} else {
		error = VFS_VGET(hpmp->hpm_mp, hp->h_fn.fn_parent,
				 LK_EXCLUSIVE, &dvp);
		if (error)
			return (error);
		dhp = VTOHP(dvp);
	}

	lsn = ((alleaf_t *)dhp->h_fn.fn_abd)->al_lsn;

	olsn = 0;
	level = 1;
	bp = NULL;

dive:
	dprintf(("[dive 0x%x] ", lsn));
	if (bp != NULL)
		brelse(bp);
	error = bread(dhp->h_devvp, lsn, D_BSIZE, NOCRED, &bp);
	if (error)
		goto failed;

	dp = (struct dirblk *) bp->b_data;
	if (dp->d_magic != D_MAGIC) {
		printf("hpfs_validatetimes: magic doesn't match\n");
		error = EINVAL;
		goto failed;
	}

	dep = D_DIRENT(dp);

	if (olsn) {
		dprintf(("[restore 0x%x] ", olsn));

		while(!(dep->de_flag & DE_END) ) {
			if((dep->de_flag & DE_DOWN) &&
			   (olsn == DE_DOWNLSN(dep)))
					 break;
			dep = (hpfsdirent_t *)((caddr_t)dep + dep->de_reclen);
		}

		if((dep->de_flag & DE_DOWN) && (olsn == DE_DOWNLSN(dep))) {
			if (dep->de_flag & DE_END)
				goto blockdone;

			if (hp->h_no == dep->de_fnode) {
				dprintf(("[found] "));
				goto readdone;
			}

			dep = (hpfsdirent_t *)((caddr_t)dep + dep->de_reclen);
		} else {
			printf("hpfs_validatetimes: ERROR! oLSN not found\n");
			error = EINVAL;
			goto failed;
		}
	}

	olsn = 0;

	while(!(dep->de_flag & DE_END)) {
		if(dep->de_flag & DE_DOWN) {
			lsn = DE_DOWNLSN(dep);
			level++;
			goto dive;
		}

		if (hp->h_no == dep->de_fnode) {
			dprintf(("[found] "));
			goto readdone;
		}

		dep = (hpfsdirent_t *)((caddr_t)dep + dep->de_reclen);
	}

	if(dep->de_flag & DE_DOWN) {
		dprintf(("[enddive] "));
		lsn = DE_DOWNLSN(dep);
		level++;
		goto dive;
	}

blockdone:
	dprintf(("[EOB] "));
	olsn = lsn;
	lsn = dp->d_parent;
	level--;
	dprintf(("[level %d] ", level));
	if (level > 0)
		goto dive;	/* undive really */

	goto failed;

readdone:
	bcopy(dep->de_name,hp->h_name,dep->de_namelen);
	hp->h_name[dep->de_namelen] = '\0';
	hp->h_namelen = dep->de_namelen;
	hp->h_ctime = dep->de_ctime;
	hp->h_atime = dep->de_atime;
	hp->h_mtime = dep->de_mtime;
	hp->h_flag |= H_PARVALID;

	dprintf(("[readdone]"));

failed:
	dprintf(("\n"));
	if (bp != NULL)
		brelse(bp);
	if (hp != dhp)
		vput(dvp);

	return (error);
}

struct timespec
hpfstimetounix (
	u_long hptime)
{
	struct timespec t;

	t.tv_nsec = 0;
	t.tv_sec = hptime;

	return t;
}

/*
 * Write down changes done to parent dir, these are only times for now. 
 * hpfsnode have to be locked.
 */
int
hpfs_updateparent (
	struct hpfsnode *hp)
{
	struct hpfsnode *dhp;
	struct vnode *dvp;
	struct hpfsdirent *dep;
	struct buf * bp;
	int error;

	dprintf(("hpfs_updateparent(0x%x): \n", hp->h_no));

	if (!(hp->h_flag & H_PARCHANGE))
		return (0);

	if (!(hp->h_flag & H_PARVALID)) {
		error = hpfs_validateparent (hp);
		if (error)
			return (error);
	}

	if (hp->h_no == hp->h_fn.fn_parent) {
		dhp = hp;
	} else {
		error = VFS_VGET(hp->h_hpmp->hpm_mp, hp->h_fn.fn_parent,
				 LK_EXCLUSIVE, &dvp);
		if (error)
			return (error);
		dhp = VTOHP(dvp);
	}

	error = hpfs_genlookupbyname (dhp, hp->h_name, hp->h_namelen,
					&bp, &dep);
	if (error) {
		goto failed;
	}

	dep->de_atime = hp->h_atime;
	dep->de_mtime = hp->h_mtime;
	dep->de_size = hp->h_fn.fn_size;

	bdwrite (bp);

	hp->h_flag &= ~H_PARCHANGE;

	error = 0;
failed:
	if (hp != dhp)
		vput(dvp);

	return (0);
}

/*
 * Write down on disk changes done to fnode. hpfsnode have to be locked.
 */
int
hpfs_update (
	struct hpfsnode *hp)
{
	struct buf * bp;

	dprintf(("hpfs_update(0x%x): \n", hp->h_no));

	if (!(hp->h_flag & H_CHANGE))
		return (0);

	bp = getblk(hp->h_devvp, hp->h_no, FNODESIZE, 0, 0, 0);
	clrbuf(bp);

	bcopy (&hp->h_fn, bp->b_data, sizeof(struct fnode));
	bdwrite (bp);

	hp->h_flag &= ~H_CHANGE;

	if (hp->h_flag & H_PARCHANGE)
		return (hpfs_updateparent(hp));

	return (0);
}

/*
 * Truncate file to specifed size. hpfsnode have to be locked.
 */
int
hpfs_truncate (
	struct hpfsnode *hp,
	u_long size)
{
	struct hpfsmount *hpmp = hp->h_hpmp;
	lsn_t newblen, oldblen;
	int error, pf;

	dprintf(("hpfs_truncate(0x%x, 0x%x -> 0x%lx): ",
		hp->h_no, hp->h_fn.fn_size, size));

	newblen = (size + DEV_BSIZE - 1) >> DEV_BSHIFT;
	oldblen = (hp->h_fn.fn_size + DEV_BSIZE - 1) >> DEV_BSHIFT;

	dprintf(("blen: 0x%x -> 0x%x\n", oldblen, newblen));

	error = hpfs_truncatealblk (hpmp, &hp->h_fn.fn_ab, newblen, &pf);
	if (error)
		return (error);
	if (pf) {
		hp->h_fn.fn_ab.ab_flag = 0;
		hp->h_fn.fn_ab.ab_freecnt = 0x8;
		hp->h_fn.fn_ab.ab_busycnt = 0x0;
		hp->h_fn.fn_ab.ab_freeoff = sizeof(alblk_t);
	}

	hp->h_fn.fn_size = size;

	hp->h_flag |= (H_CHANGE | H_PARCHANGE);

	dprintf(("hpfs_truncate: successful\n"));

	return (0);
}

/*
 * Enlarge file to specifed size. hpfsnode have to be locked.
 */
int
hpfs_extend (
	struct hpfsnode *hp,
	u_long size)
{
	struct hpfsmount *hpmp = hp->h_hpmp;
	lsn_t newblen, oldblen;
	int error;

	dprintf(("hpfs_extend(0x%x, 0x%x -> 0x%lx): ",
		hp->h_no, hp->h_fn.fn_size, size));

	if (hpmp->hpm_bavail < 0x10) 
		return (ENOSPC);

	newblen = (size + DEV_BSIZE - 1) >> DEV_BSHIFT;
	oldblen = (hp->h_fn.fn_size + DEV_BSIZE - 1) >> DEV_BSHIFT;

	dprintf(("blen: 0x%x -> 0x%x\n", oldblen, newblen));

	error = hpfs_addextent(hpmp, hp, newblen - oldblen);
	if (error) {
		printf("hpfs_extend: FAILED TO ADD EXTENT %d\n", error);
		return (error);
	}

	hp->h_fn.fn_size = size;

	hp->h_flag |= (H_CHANGE | H_PARCHANGE);

	dprintf(("hpfs_extend: successful\n"));

	return (0);
}

/*
 * Read AlSec structure, and check if magic is valid.
 * You don't need to brelse buf on error.
 */
int
hpfs_breadstruct (
	struct hpfsmount *hpmp,
	lsn_t lsn,
	u_int len,
	u_int32_t magic,
	struct buf **bpp)
{
	struct buf *bp;
	u_int32_t *mp;
	int error;

	dprintf(("hpfs_breadstruct: reading at 0x%x\n", lsn));

	*bpp = NULL;

	error = bread(hpmp->hpm_devvp, lsn, len, NOCRED, &bp);
	if (error) {
		brelse(bp);
		return (error);
	}
	mp = (u_int32_t *) bp->b_data;
	if (*mp != magic) {
		brelse(bp);
		printf("hpfs_breadstruct: MAGIC DOESN'T MATCH (0x%08x != 0x%08x)\n",
			*mp, magic);
		return (EINVAL);
	}

	*bpp = bp;

	return (0);
}

