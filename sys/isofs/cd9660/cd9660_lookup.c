/*-
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley
 * by Pace Willisson (pace@blitz.com).  The Rock Ridge Extension
 * Support code is derived from software contributed to Berkeley
 * by Atsushi Murai (amurai@spec.co.jp).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)ufs_lookup.c	7.33 (Berkeley) 5/19/91
 *
 *	@(#)cd9660_lookup.c	8.2 (Berkeley) 1/23/94
 * $Id: cd9660_lookup.c,v 1.7.4.1 1995/10/26 09:16:53 davidg Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/buf.h>
#include <sys/file.h>
#include <sys/vnode.h>
#include <sys/mount.h>

#include <isofs/cd9660/iso.h>
#include <isofs/cd9660/cd9660_node.h>
#include <isofs/cd9660/iso_rrip.h>
#include <isofs/cd9660/cd9660_rrip.h>

struct	nchstats iso_nchstats;

/*
 * Convert a component of a pathname into a pointer to a locked inode.
 * This is a very central and rather complicated routine.
 * If the file system is not maintained in a strict tree hierarchy,
 * this can result in a deadlock situation (see comments in code below).
 *
 * The flag argument is LOOKUP, CREATE, RENAME, or DELETE depending on
 * whether the name is to be looked up, created, renamed, or deleted.
 * When CREATE, RENAME, or DELETE is specified, information usable in
 * creating, renaming, or deleting a directory entry may be calculated.
 * If flag has LOCKPARENT or'ed into it and the target of the pathname
 * exists, lookup returns both the target and its parent directory locked.
 * When creating or renaming and LOCKPARENT is specified, the target may
 * not be ".".  When deleting and LOCKPARENT is specified, the target may
 * be "."., but the caller must check to ensure it does an vrele and iput
 * instead of two iputs.
 *
 * Overall outline of ufs_lookup:
 *
 *	check accessibility of directory
 *	look for name in cache, if found, then if at end of path
 *	  and deleting or creating, drop it, else return name
 *	search for name in directory, to found or notfound
 * notfound:
 *	if creating, return locked directory, leaving info on available slots
 *	else return error
 * found:
 *	if at end of path and deleting, return information to allow delete
 *	if at end of path and rewriting (RENAME and LOCKPARENT), lock target
 *	  inode and return info to allow rewrite
 *	if not at end, add name to cache; if at end and neither creating
 *	  nor deleting, add name to cache
 *
 * NOTE: (LOOKUP | LOCKPARENT) currently returns the parent inode unlocked.
 */
int
cd9660_lookup(ap)
	struct vop_lookup_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
	} */ *ap;
{
	register struct vnode *vdp;	/* vnode for directory being searched */
	register struct iso_node *dp;	/* inode for directory being searched */
	register struct iso_mnt *imp;	/* file system that directory is in */
	struct buf *bp;			/* a buffer of directory entries */
	struct iso_directory_record *ep = 0;/* the current directory entry */
	int entryoffsetinblock;		/* offset of ep in bp's buffer */
	int saveoffset = 0;		/* offset of last directory entry in dir */
	int numdirpasses;		/* strategy for directory search */
	doff_t endsearch;		/* offset to end directory search */
	struct iso_node *pdp;		/* saved dp during symlink work */
	struct iso_node *tdp;		/* returned by iget */
	int lockparent;			/* 1 => lockparent flag is set */
	int wantparent;			/* 1 => wantparent or lockparent flag */
	int error;
	ino_t ino = 0;
	int reclen;
	u_short namelen;
	int isoflags;
	char altname[NAME_MAX];
	int res;
	int assoc, len;
	char *name;
	struct vnode **vpp = ap->a_vpp;
	struct componentname *cnp = ap->a_cnp;
	struct ucred *cred = cnp->cn_cred;
	int flags = cnp->cn_flags;
	int nameiop = cnp->cn_nameiop;

	bp = NULL;
	*vpp = NULL;
	vdp = ap->a_dvp;
	dp = VTOI(vdp);
	imp = dp->i_mnt;
	lockparent = flags & LOCKPARENT;
	wantparent = flags & (LOCKPARENT|WANTPARENT);

	/*
	 * Check accessiblity of directory.
	 */
	if (vdp->v_type != VDIR)
	    return (ENOTDIR);
	if ((error = VOP_ACCESS(vdp, VEXEC, cred, cnp->cn_proc)))
		return (error);
	if ((flags & ISLASTCN) && (vdp->v_mount->mnt_flag & MNT_RDONLY) &&
	    (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME))
		return (EROFS);

	/*
	 * We now have a segment name to search for, and a directory to search.
	 *
	 * Before tediously performing a linear scan of the directory,
	 * check the name cache to see if the directory/name pair
	 * we are looking for is known already.
	 */
	if ((error = cache_lookup(vdp, vpp, cnp))) {
		int vpid;	/* capability number of vnode */

		if (error == ENOENT)
			return (error);
#ifdef PARANOID
		if ((vdp->v_flag & VROOT) && (flags & ISDOTDOT))
			panic("ufs_lookup: .. through root");
#endif
		/*
		 * Get the next vnode in the path.
		 * See comment below starting `Step through' for
		 * an explaination of the locking protocol.
		 */
		pdp = dp;
		dp = VTOI(*vpp);
		vdp = *vpp;
		vpid = vdp->v_id;
		if (pdp == dp) {
			VREF(vdp);
			error = 0;
		} else if (flags & ISDOTDOT) {
			ISO_IUNLOCK(pdp);
			error = vget(vdp, 1);
			if (!error && lockparent && (flags & ISLASTCN))
				ISO_ILOCK(pdp);
		} else {
			error = vget(vdp, 1);
			if (!lockparent || error || !(flags & ISLASTCN))
				ISO_IUNLOCK(pdp);
		}
		/*
		 * Check that the capability number did not change
		 * while we were waiting for the lock.
		 */
		if (!error) {
			if (vpid == vdp->v_id)
				return (0);
			iso_iput(dp);
			if (lockparent && pdp != dp && (flags & ISLASTCN))
				ISO_IUNLOCK(pdp);
		}
		ISO_ILOCK(pdp);
		dp = pdp;
		vdp = ITOV(dp);
		*vpp = NULL;
	}

	len = cnp->cn_namelen;
	name = cnp->cn_nameptr;
	/*
	 * A leading `=' means, we are looking for an associated file
	 */
	if ((assoc = (imp->iso_ftype != ISO_FTYPE_RRIP && *name == ASSOCCHAR)))
	{
		len--;
		name++;
	}

	/*
	 * If there is cached information on a previous search of
	 * this directory, pick up where we last left off.
	 * We cache only lookups as these are the most common
	 * and have the greatest payoff. Caching CREATE has little
	 * benefit as it usually must search the entire directory
	 * to determine that the entry does not exist. Caching the
	 * location of the last DELETE or RENAME has not reduced
	 * profiling time and hence has been removed in the interest
	 * of simplicity.
	 */
	if (nameiop != LOOKUP || dp->i_diroff == 0 ||
	    dp->i_diroff > dp->i_size) {
		entryoffsetinblock = 0;
		dp->i_offset = 0;
		numdirpasses = 1;
	} else {
		dp->i_offset = dp->i_diroff;
		entryoffsetinblock = iso_blkoff(imp, dp->i_offset);
		if (entryoffsetinblock != 0) {
			if ((error = iso_blkatoff(dp, dp->i_offset, &bp)))
				return (error);
		}
		numdirpasses = 2;
		iso_nchstats.ncs_2passes++;
	}
	endsearch = roundup(dp->i_size, imp->logical_block_size);

searchloop:
	while (dp->i_offset < endsearch) {
		/*
		 * If offset is on a block boundary,
		 * read the next directory block.
		 * Release previous if it exists.
		 */
		if (iso_blkoff(imp, dp->i_offset) == 0) {
			if (bp != NULL)
				brelse(bp);
			if ((error = iso_blkatoff(dp, dp->i_offset, &bp)))
				return (error);
			entryoffsetinblock = 0;
		}
		/*
		 * Get pointer to next entry.
		 */
		ep = (struct iso_directory_record *)
			(bp->b_un.b_addr + entryoffsetinblock);

		reclen = isonum_711 (ep->length);
		if (reclen == 0) {
			/* skip to next block, if any */
			dp->i_offset =
				roundup(dp->i_offset, imp->logical_block_size);
			continue;
		}

		if (reclen < ISO_DIRECTORY_RECORD_SIZE)
			/* illegal entry, stop */
			break;

		if (entryoffsetinblock + reclen > imp->logical_block_size)
			/* entries are not allowed to cross boundaries */
			break;

		/*
		 * Check for a name match.
		 */
		namelen = isonum_711(ep->name_len);
		isoflags = isonum_711(imp->iso_ftype == ISO_FTYPE_HIGH_SIERRA?
				      &ep->date[6]: ep->flags);

		if (reclen < ISO_DIRECTORY_RECORD_SIZE + namelen)
			/* illegal entry, stop */
			break;

		switch (imp->iso_ftype) {
		default:
			if (!(isoflags & 4) == !assoc) {
				if ((len == 1
				     && *name == '.')
				    || (flags & ISDOTDOT)) {
					if (namelen == 1
					    && ep->name[0] == ((flags & ISDOTDOT) ? 1 : 0)) {
						/*
						 * Save directory entry's inode number and
						 * reclen in ndp->ni_ufs area, and release
						 * directory buffer.
						 */
						isodirino(&dp->i_ino,ep,imp);
						goto found;
					}
					if (namelen != 1
					    || ep->name[0] != 0)
						goto notfound;
				} else if (!(res = isofncmp(name,len,
							    ep->name,namelen))) {
					if (isoflags & 2)
						isodirino(&ino,ep,imp);
					else
						ino = dbtob(bp->b_blkno)
							+ entryoffsetinblock;
					saveoffset = dp->i_offset;
				} else if (ino)
					goto foundino;
#ifdef	NOSORTBUG	/* On some CDs directory entries are not sorted correctly */
				else if (res < 0)
					goto notfound;
				else if (res > 0 && numdirpasses == 2)
					numdirpasses++;
#endif
			}
			break;
		case ISO_FTYPE_RRIP:
			if (isonum_711(ep->flags)&2)
				isodirino(&ino,ep,imp);
			else
				ino = dbtob(bp->b_blkno) + entryoffsetinblock;
			dp->i_ino = ino;
			cd9660_rrip_getname(ep,altname,&namelen,&dp->i_ino,imp);
			if (namelen == cnp->cn_namelen
			    && !bcmp(name,altname,namelen))
				goto found;
			ino = 0;
			break;
		}
		dp->i_offset += reclen;
		entryoffsetinblock += reclen;
	}
	if (ino) {
foundino:
		dp->i_ino = ino;
		if (saveoffset != dp->i_offset) {
			if (iso_lblkno(imp,dp->i_offset)
			    != iso_lblkno(imp,saveoffset)) {
				if (bp != NULL)
					brelse(bp);
				if ((error = iso_blkatoff(dp, saveoffset, &bp)))
					return (error);
			}
			ep = (struct iso_directory_record *)(bp->b_un.b_addr
							     + iso_blkoff(imp,saveoffset));
			dp->i_offset = saveoffset;
		}
		goto found;
	}
notfound:
	/*
	 * If we started in the middle of the directory and failed
	 * to find our target, we must check the beginning as well.
	 */
	if (numdirpasses == 2) {
		numdirpasses--;
		dp->i_offset = 0;
		endsearch = dp->i_diroff;
		goto searchloop;
	}
	if (bp != NULL)
		brelse(bp);
	/*
	 * Insert name into cache (as non-existent) if appropriate.
	 */
	if (cnp->cn_flags & MAKEENTRY)
		cache_enter(vdp, *vpp, cnp);
	if (nameiop == CREATE || nameiop == RENAME)
		return (EROFS);
	return (ENOENT);

found:
	if (numdirpasses == 2)
		iso_nchstats.ncs_pass2++;
	if (bp != NULL)
		brelse(bp);

	/*
	 * Found component in pathname.
	 * If the final component of path name, save information
	 * in the cache as to where the entry was found.
	 */
	if ((flags & ISLASTCN) && nameiop == LOOKUP)
		dp->i_diroff = dp->i_offset;

	/*
	 * Step through the translation in the name.  We do not `iput' the
	 * directory because we may need it again if a symbolic link
	 * is relative to the current directory.  Instead we save it
	 * unlocked as "pdp".  We must get the target inode before unlocking
	 * the directory to insure that the inode will not be removed
	 * before we get it.  We prevent deadlock by always fetching
	 * inodes from the root, moving down the directory tree. Thus
	 * when following backward pointers ".." we must unlock the
	 * parent directory before getting the requested directory.
	 * There is a potential race condition here if both the current
	 * and parent directories are removed before the `iget' for the
	 * inode associated with ".." returns.  We hope that this occurs
	 * infrequently since we cannot avoid this race condition without
	 * implementing a sophisticated deadlock detection algorithm.
	 * Note also that this simple deadlock detection scheme will not
	 * work if the file system has any hard links other than ".."
	 * that point backwards in the directory structure.
	 */
	pdp = dp;
	/*
	 * If ino is different from dp->i_ino,
	 * it's a relocated directory.
	 */
	if (flags & ISDOTDOT) {
		ISO_IUNLOCK(pdp);	/* race to get the inode */
		if ((error = iso_iget(dp,dp->i_ino,
				     dp->i_ino != ino,
				     &tdp,ep))) {
			ISO_ILOCK(pdp);
			return (error);
		}
		if (lockparent && (flags & ISLASTCN))
			ISO_ILOCK(pdp);
		*vpp = ITOV(tdp);
	} else if (dp->i_number == dp->i_ino) {
		VREF(vdp);	/* we want ourself, ie "." */
		*vpp = vdp;
	} else {
		if ((error = iso_iget(dp,dp->i_ino,dp->i_ino!=ino,&tdp,ep)))
			return (error);
		if (!lockparent || !(flags & ISLASTCN))
			ISO_IUNLOCK(pdp);
		*vpp = ITOV(tdp);
	}

	/*
	 * Insert name into cache if appropriate.
	 */
	if (cnp->cn_flags & MAKEENTRY)
		cache_enter(vdp, *vpp, cnp);
	return (0);
}

/*
 * Return buffer with contents of block "offset"
 * from the beginning of directory "ip".  If "res"
 * is non-zero, fill it in with a pointer to the
 * remaining space in the directory.
 */
int
iso_blkatoff(ip, offset, bpp)
	struct iso_node *ip;
	doff_t offset;
	struct buf **bpp;
{
	register struct iso_mnt *imp = ip->i_mnt;
	daddr_t lbn = iso_lblkno(imp,offset);
	int bsize = iso_blksize(imp,ip,lbn);
	struct buf *bp;
	int error;

	if ((error = bread(ITOV(ip),lbn,bsize,NOCRED,&bp))) {
		brelse(bp);
		*bpp = 0;
		return (error);
	}
	*bpp = bp;

	return (0);
}
