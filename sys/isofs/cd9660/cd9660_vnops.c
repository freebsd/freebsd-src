/*-
 * Copyright (c) 1994
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
 *	@(#)cd9660_vnops.c	8.3 (Berkeley) 1/23/94
 * $Id: cd9660_vnops.c,v 1.18 1995/10/31 12:13:47 phk Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/resourcevar.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <miscfs/specfs/specdev.h>
#include <miscfs/fifofs/fifo.h>
#include <sys/malloc.h>
#include <sys/dir.h>

#include <isofs/cd9660/iso.h>
#include <isofs/cd9660/cd9660_node.h>
#include <isofs/cd9660/iso_rrip.h>

static int cd9660_open __P((struct vop_open_args *));
static int cd9660_close __P((struct vop_close_args *));
static int cd9660_access __P((struct vop_access_args *));
static int cd9660_getattr __P((struct vop_getattr_args *));
static int cd9660_read __P((struct vop_read_args *));
static int cd9660_ioctl __P((struct vop_ioctl_args *));
static int cd9660_select __P((struct vop_select_args *));
static int cd9660_mmap __P((struct vop_mmap_args *));
static int cd9660_seek __P((struct vop_seek_args *));
static int cd9660_readdir __P((struct vop_readdir_args *));
static int cd9660_abortop __P((struct vop_abortop_args *));
static int cd9660_lock __P((struct vop_lock_args *));
static int cd9660_unlock __P((struct vop_unlock_args *));
static int cd9660_strategy __P((struct vop_strategy_args *));
static int cd9660_print __P((struct vop_print_args *));
static int cd9660_islocked __P((struct vop_islocked_args *));
#if 0
/*
 * Mknod vnode call
 *  Actually remap the device number
 */
cd9660_mknod(ndp, vap, cred, p)
	struct nameidata *ndp;
	struct ucred *cred;
	struct vattr *vap;
	struct proc *p;
{
#ifndef	ISODEVMAP
	free(ndp->ni_pnbuf, M_NAMEI);
	vput(ndp->ni_dvp);
	vput(ndp->ni_vp);
	return EINVAL;
#else
	register struct vnode *vp;
	struct iso_node *ip;
	struct iso_dnode *dp;
	int error;

	vp = ndp->ni_vp;
	ip = VTOI(vp);

	if (ip->i_mnt->iso_ftype != ISO_FTYPE_RRIP
	    || vap->va_type != vp->v_type
	    || (vap->va_type != VCHR && vap->va_type != VBLK)) {
		free(ndp->ni_pnbuf, M_NAMEI);
		vput(ndp->ni_dvp);
		vput(ndp->ni_vp);
		return EINVAL;
	}

	dp = iso_dmap(ip->i_dev,ip->i_number,1);
	if (ip->inode.iso_rdev == vap->va_rdev || vap->va_rdev == VNOVAL) {
		/* same as the unmapped one, delete the mapping */
		remque(dp);
		FREE(dp,M_CACHE);
	} else
		/* enter new mapping */
		dp->d_dev = vap->va_rdev;

	/*
	 * Remove inode so that it will be reloaded by iget and
	 * checked to see if it is an alias of an existing entry
	 * in the inode cache.
	 */
	vput(vp);
	vp->v_type = VNON;
	vgone(vp);
	return (0);
#endif
}
#endif

/*
 * Open called.
 *
 * Nothing to do.
 */
/* ARGSUSED */
static int
cd9660_open(ap)
	struct vop_open_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	return (0);
}

/*
 * Close called
 *
 * Update the times on the inode on writeable file systems.
 */
/* ARGSUSED */
static int
cd9660_close(ap)
	struct vop_close_args /* {
		struct vnode *a_vp;
		int  a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	return (0);
}

/*
 * Check mode permission on inode pointer. Mode is READ, WRITE or EXEC.
 * The mode is shifted to select the owner/group/other fields. The
 * super user is granted all permissions.
 */
/* ARGSUSED */
static int
cd9660_access(ap)
	struct vop_access_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	return (0);
}

static int
cd9660_getattr(ap)
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;

{
	struct vnode *vp = ap->a_vp;
	register struct vattr *vap = ap->a_vap;
	register struct iso_node *ip = VTOI(vp);

	vap->va_fsid	= ip->i_dev;
	vap->va_fileid	= ip->i_number;

	vap->va_mode	= ip->inode.iso_mode;
	vap->va_nlink	= ip->inode.iso_links;
	vap->va_uid	= ip->inode.iso_uid;
	vap->va_gid	= ip->inode.iso_gid;
	vap->va_atime	= ip->inode.iso_atime;
	vap->va_mtime	= ip->inode.iso_mtime;
	vap->va_ctime	= ip->inode.iso_ctime;
	vap->va_rdev	= ip->inode.iso_rdev;

	vap->va_size	= (u_quad_t) ip->i_size;
	vap->va_flags	= 0;
	vap->va_gen = 1;
	vap->va_blocksize = ip->i_mnt->logical_block_size;
	vap->va_bytes	= (u_quad_t) ip->i_size;
	vap->va_type	= vp->v_type;
	vap->va_filerev	= 0;
	return (0);
}

#if ISO_DEFAULT_BLOCK_SIZE >= NBPG
#ifdef DEBUG
extern int doclusterread;
#else
#define doclusterread 1
#endif
#else
/* XXX until cluster routines can handle block sizes less than one page */
#define doclusterread 0
#endif

/*
 * Vnode op for reading.
 */
static int
cd9660_read(ap)
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	register struct uio *uio = ap->a_uio;
	register struct iso_node *ip = VTOI(vp);
	register struct iso_mnt *imp;
	struct buf *bp;
	daddr_t lbn, rablock;
	off_t diff;
	int rasize, error = 0;
	long size, n, on;

	if (uio->uio_resid == 0)
		return (0);
	if (uio->uio_offset < 0)
		return (EINVAL);
	ip->i_flag |= IACC;
	imp = ip->i_mnt;
	do {
		lbn = iso_lblkno(imp, uio->uio_offset);
		on = iso_blkoff(imp, uio->uio_offset);
		n = min((unsigned)(imp->logical_block_size - on),
			uio->uio_resid);
		diff = (off_t)ip->i_size - uio->uio_offset;
		if (diff <= 0)
			return (0);
		if (diff < n)
			n = diff;
		size = iso_blksize(imp, ip, lbn);
		rablock = lbn + 1;
		if (doclusterread) {
			if (iso_lblktosize(imp, rablock) <= ip->i_size)
				error = cluster_read(vp, (off_t)ip->i_size,
						     lbn, size, NOCRED, &bp);
			else
				error = bread(vp, lbn, size, NOCRED, &bp);
		} else {
			if (vp->v_lastr + 1 == lbn &&
			    iso_lblktosize(imp, rablock) < ip->i_size) {
				rasize = iso_blksize(imp, ip, rablock);
				error = breadn(vp, lbn, size, &rablock,
					       &rasize, 1, NOCRED, &bp);
			} else
				error = bread(vp, lbn, size, NOCRED, &bp);
		}
		vp->v_lastr = lbn;
		n = min(n, size - bp->b_resid);
		if (error) {
			brelse(bp);
			return (error);
		}

		error = uiomove(bp->b_un.b_addr + on, (int)n, uio);
		brelse(bp);
	} while (error == 0 && uio->uio_resid > 0 && n != 0);
	return (error);
}

/* ARGSUSED */
static int
cd9660_ioctl(ap)
	struct vop_ioctl_args /* {
		struct vnode *a_vp;
		int  a_command;
		caddr_t	 a_data;
		int  a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	printf("You did ioctl for isofs !!\n");
	return (ENOTTY);
}

/* ARGSUSED */
static int
cd9660_select(ap)
	struct vop_select_args /* {
		struct vnode *a_vp;
		int  a_which;
		int  a_fflags;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{

	/*
	 * We should really check to see if I/O is possible.
	 */
	return (1);
}

/*
 * Mmap a file
 *
 * NB Currently unsupported.
 */
/* ARGSUSED */
static int
cd9660_mmap(ap)
	struct vop_mmap_args /* {
		struct vnode *a_vp;
		int  a_fflags;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{

	return (EINVAL);
}

/*
 * Seek on a file
 *
 * Nothing to do, so just return.
 */
/* ARGSUSED */
static int
cd9660_seek(ap)
	struct vop_seek_args /* {
		struct vnode *a_vp;
		off_t  a_oldoff;
		off_t  a_newoff;
		struct ucred *a_cred;
	} */ *ap;
{

	return (0);
}

/*
 * Structure for reading directories
 */
struct isoreaddir {
	struct dirent saveent;
	struct dirent assocent;
	struct dirent current;
	off_t saveoff;
	off_t assocoff;
	off_t curroff;
	struct uio *uio;
	off_t uio_off;
	u_int *cookiep;
	int ncookies;
	int eof;
};

static int
iso_uiodir(idp,dp,off)
	struct isoreaddir *idp;
	struct dirent *dp;
	off_t off;
{
	int error;

	dp->d_name[dp->d_namlen] = 0;
	dp->d_reclen = DIRSIZ(dp);

	if (idp->uio->uio_resid < dp->d_reclen) {
		idp->eof = 0;
		return -1;
	}

	if (idp->cookiep) {
		if (idp->ncookies <= 0) {
			idp->eof = 0;
			return -1;
		}

		*idp->cookiep++ = off;
		--idp->ncookies;
	}

	if ((error = uiomove((caddr_t)dp,dp->d_reclen,idp->uio)))
		return error;
	idp->uio_off = off;
	return 0;
}

static int
iso_shipdir(idp)
	struct isoreaddir *idp;
{
	struct dirent *dp;
	int cl, sl, assoc;
	int error;
	char *cname, *sname;

	cl = idp->current.d_namlen;
	cname = idp->current.d_name;
assoc = (cl > 1) && (*cname == ASSOCCHAR);
	if (assoc) {
		cl--;
		cname++;
	}

	dp = &idp->saveent;
	sname = dp->d_name;
	if (!(sl = dp->d_namlen)) {
		dp = &idp->assocent;
		sname = dp->d_name + 1;
		sl = dp->d_namlen - 1;
	}
	if (sl > 0) {
		if (sl != cl
		    || bcmp(sname,cname,sl)) {
			if (idp->assocent.d_namlen) {
				if ((error = iso_uiodir(idp,&idp->assocent,idp->assocoff)))
					return error;
				idp->assocent.d_namlen = 0;
			}
			if (idp->saveent.d_namlen) {
				if ((error = iso_uiodir(idp,&idp->saveent,idp->saveoff)))
					return error;
				idp->saveent.d_namlen = 0;
			}
		}
	}
	idp->current.d_reclen = DIRSIZ(&idp->current);
	if (assoc) {
		idp->assocoff = idp->curroff;
		bcopy(&idp->current,&idp->assocent,idp->current.d_reclen);
	} else {
		idp->saveoff = idp->curroff;
		bcopy(&idp->current,&idp->saveent,idp->current.d_reclen);
	}
	return 0;
}

/*
 * Vnode op for readdir
 * XXX make sure everything still works now that eofflagp and cookiep
 * are no longer args.
 */
static int
cd9660_readdir(ap)
	struct vop_readdir_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
	} */ *ap;
{
	register struct uio *uio = ap->a_uio;
	struct isoreaddir *idp;
	int entryoffsetinblock;
	int error = 0;
	int endsearch;
	struct iso_directory_record *ep;
	u_short elen;
	int reclen;
	int isoflags;
	struct iso_mnt *imp;
	struct iso_node *ip;
	struct buf *bp = NULL;
	u_short tmplen;
	int ncookies = 0;
	u_int *cookies = NULL;

	ip = VTOI(ap->a_vp);
	imp = ip->i_mnt;

	MALLOC(idp,struct isoreaddir *,sizeof(*idp),M_TEMP,M_WAITOK);
	idp->saveent.d_namlen = 0;
	idp->assocent.d_namlen = 0;
	idp->uio = uio;
	if (ap->a_ncookies != NULL) {
		/*
		 * Guess the number of cookies needed.
		 */
		ncookies = uio->uio_resid / 16;
		MALLOC(cookies, u_int *, ncookies * sizeof(u_int), M_TEMP, M_WAITOK);
		idp->cookiep = cookies;
		idp->ncookies = ncookies;
	} else
		idp->cookiep = 0;
	idp->eof = 0;
	idp->curroff = uio->uio_offset;

	entryoffsetinblock = iso_blkoff(imp, idp->curroff);
	if (entryoffsetinblock != 0) {
		if ((error = iso_blkatoff(ip, idp->curroff, &bp))) {
			FREE(idp,M_TEMP);
			return (error);
		}
	}

	endsearch = ip->i_size;

	while (idp->curroff < endsearch) {
		/*
		 * If offset is on a block boundary,
		 * read the next directory block.
		 * Release previous if it exists.
		 */

		if (iso_blkoff(imp, idp->curroff) == 0) {
			if (bp != NULL)
				brelse(bp);
			if ((error = iso_blkatoff(ip, idp->curroff, &bp)))
				break;
			entryoffsetinblock = 0;
		}
		/*
		 * Get pointer to next entry.
		 */

		ep = (struct iso_directory_record *)
			(bp->b_un.b_addr + entryoffsetinblock);

		reclen = isonum_711 (ep->length);
		isoflags = isonum_711(imp->iso_ftype == ISO_FTYPE_HIGH_SIERRA?
				      &ep->date[6]: ep->flags);
		if (reclen == 0) {
			/* skip to next block, if any */
			idp->curroff = roundup (idp->curroff,
						imp->logical_block_size);
			continue;
		}

		if (reclen < ISO_DIRECTORY_RECORD_SIZE) {
			error = EINVAL;
			/* illegal entry, stop */
			break;
		}

		if (entryoffsetinblock + reclen > imp->logical_block_size) {
			error = EINVAL;
			/* illegal directory, so stop looking */
			break;
		}

		/* XXX: be more intelligent if we can */
		idp->current.d_type = DT_UNKNOWN;

		idp->current.d_namlen = isonum_711 (ep->name_len);
		if (isoflags & 2)
			isodirino(&idp->current.d_fileno,ep,imp);
		else
			idp->current.d_fileno = dbtob(bp->b_blkno) +
				idp->curroff;

		if (reclen < ISO_DIRECTORY_RECORD_SIZE + idp->current.d_namlen) {
			error = EINVAL;
			/* illegal entry, stop */
			break;
		}

		idp->curroff += reclen;
		/*
		 *
		 */
		switch (imp->iso_ftype) {
		case ISO_FTYPE_RRIP:
			cd9660_rrip_getname(ep,idp->current.d_name,
					   &tmplen,
					   &idp->current.d_fileno,imp);
			idp->current.d_namlen = tmplen;
			if (idp->current.d_namlen)
				error = iso_uiodir(idp,&idp->current,idp->curroff);
			break;
		default: /* ISO_FTYPE_DEFAULT || ISO_FTYPE_9660 || ISO_FTYPE_HIGH_SIERRA*/
			strcpy(idp->current.d_name,"..");
			switch (ep->name[0]) {
			case 0:
				idp->current.d_namlen = 1;
				error = iso_uiodir(idp,&idp->current,idp->curroff);
				break;
			case 1:
				idp->current.d_namlen = 2;
				error = iso_uiodir(idp,&idp->current,idp->curroff);
				break;
			default:
				isofntrans(ep->name,idp->current.d_namlen,
					   idp->current.d_name, &elen,
					   imp->iso_ftype == ISO_FTYPE_9660,
					   isoflags & 4);
				idp->current.d_namlen = (u_char)elen;
				if (imp->iso_ftype == ISO_FTYPE_DEFAULT)
					error = iso_shipdir(idp);
				else
					error = iso_uiodir(idp,&idp->current,idp->curroff);
				break;
			}
		}
		if (error)
			break;

		entryoffsetinblock += reclen;
	}

	if (!error && imp->iso_ftype == ISO_FTYPE_DEFAULT) {
		idp->current.d_namlen = 0;
		error = iso_shipdir(idp);
	}
	if (error < 0)
		error = 0;

	if (ap->a_ncookies != NULL) {
		if (error)
			FREE(cookies, M_TEMP);
		else {
			/*
			 * Work out the number of cookies actually used.
			 */
			*ap->a_ncookies = ncookies - idp->ncookies;
			*ap->a_cookies = cookies;
		}
	}

	if (bp)
		brelse (bp);

	uio->uio_offset = idp->uio_off;
	if (ap->a_eofflag)
	    *ap->a_eofflag = idp->eof;

	FREE(idp,M_TEMP);

	return (error);
}

/*
 * Return target name of a symbolic link
 * Shouldn't we get the parent vnode and read the data from there?
 * This could eventually result in deadlocks in cd9660_lookup.
 * But otherwise the block read here is in the block buffer two times.
 */
typedef struct iso_directory_record ISODIR;
typedef struct iso_node		    ISONODE;
typedef struct iso_mnt		    ISOMNT;
static int
cd9660_readlink(ap)
	struct vop_readlink_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
	} */ *ap;
{
	ISONODE	*ip;
	ISODIR	*dirp;
	ISOMNT	*imp;
	struct	buf *bp;
	u_short	symlen;
	int	error;
	char	*symname;

	ip  = VTOI(ap->a_vp);
	imp = ip->i_mnt;

	if (imp->iso_ftype != ISO_FTYPE_RRIP)
		return EINVAL;

	/*
	 * Get parents directory record block that this inode included.
	 */
	error = bread(imp->im_devvp,
		      iso_dblkno(imp, ip->i_number),
		      imp->logical_block_size,
		      NOCRED,
		      &bp);
	if (error) {
		brelse(bp);
		return EINVAL;
	}

	/*
	 * Setup the directory pointer for this inode
	 */
	dirp = (ISODIR *)(bp->b_un.b_addr + (ip->i_number & imp->im_bmask));
#ifdef DEBUG
	printf("lbn=%d,off=%d,bsize=%d,DEV_BSIZE=%d, dirp= %08x, b_addr=%08x, offset=%08x(%08x)\n",
	       (daddr_t)(ip->i_number >> imp->im_bshift),
	       ip->i_number & imp->im_bmask,
	       imp->logical_block_size,
	       DEV_BSIZE,
	       dirp,
	       bp->b_un.b_addr,
	       ip->i_number,
	       ip->i_number & imp->im_bmask );
#endif

	/*
	 * Just make sure, we have a right one....
	 *   1: Check not cross boundary on block
	 */
	if ((ip->i_number & imp->im_bmask) + isonum_711(dirp->length)
	    > imp->logical_block_size) {
		brelse(bp);
		return EINVAL;
	}

	/*
	 * Now get a buffer
	 * Abuse a namei buffer for now.
	 */
	MALLOC(symname,char *,MAXPATHLEN,M_NAMEI,M_WAITOK);

	/*
	 * Ok, we just gathering a symbolic name in SL record.
	 */
	if (cd9660_rrip_getsymname(dirp,symname,&symlen,imp) == 0) {
		FREE(symname,M_NAMEI);
		brelse(bp);
		return EINVAL;
	}
	/*
	 * Don't forget before you leave from home ;-)
	 */
	brelse(bp);

	/*
	 * return with the symbolic name to caller's.
	 */
	error = uiomove(symname,symlen,ap->a_uio);

	FREE(symname,M_NAMEI);

	return error;
}

/*
 * Ufs abort op, called after namei() when a CREATE/DELETE isn't actually
 * done. If a buffer has been saved in anticipation of a CREATE, delete it.
 */
static int
cd9660_abortop(ap)
	struct vop_abortop_args /* {
		struct vnode *a_dvp;
		struct componentname *a_cnp;
	} */ *ap;
{
	if ((ap->a_cnp->cn_flags & (HASBUF | SAVESTART)) == HASBUF)
		FREE(ap->a_cnp->cn_pnbuf, M_NAMEI);
	return 0;
}

/*
 * Lock an inode.
 */
static int
cd9660_lock(ap)
	struct vop_lock_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	register struct iso_node *ip = VTOI(ap->a_vp);

	ISO_ILOCK(ip);
	return 0;
}

/*
 * Unlock an inode.
 */
static int
cd9660_unlock(ap)
	struct vop_unlock_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	register struct iso_node *ip = VTOI(ap->a_vp);

	if (!(ip->i_flag & ILOCKED))
		panic("cd9660_unlock NOT LOCKED");
	ISO_IUNLOCK(ip);
	return 0;
}

/*
 * Check for a locked inode.
 */
static int
cd9660_islocked(ap)
	struct vop_islocked_args /* {
		struct vnode *a_vp;
	} */ *ap;
{

	if (VTOI(ap->a_vp)->i_flag & ILOCKED)
		return 1;
	return 0;
}

/*
 * Calculate the logical to physical mapping if not done already,
 * then call the device strategy routine.
 */
static int
cd9660_strategy(ap)
	struct vop_strategy_args /* {
		struct buf *a_bp;
	} */ *ap;
{
	register struct buf *bp = ap->a_bp;
	register struct vnode *vp = bp->b_vp;
	register struct iso_node *ip;
	int error;

	ip = VTOI(vp);
	if (vp->v_type == VBLK || vp->v_type == VCHR)
		panic("cd9660_strategy: spec");
	if (bp->b_blkno == bp->b_lblkno) {
		if ((error =
		    VOP_BMAP(vp, bp->b_lblkno, NULL, &bp->b_blkno, NULL, NULL))) {
			bp->b_error = error;
			bp->b_flags |= B_ERROR;
			biodone(bp);
			return (error);
		}
		if ((long)bp->b_blkno == -1)
			clrbuf(bp);
	}
	if ((long)bp->b_blkno == -1) {
		biodone(bp);
		return (0);
	}
	vp = ip->i_devvp;
	bp->b_dev = vp->v_rdev;
	VOCALL (vp->v_op, VOFFSET(vop_strategy), ap);
	return (0);
}

/*
 * Print out the contents of an inode.
 */
static int
cd9660_print(ap)
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	printf("tag VT_ISOFS, isofs vnode\n");
	return 0;
}

/*
 * Unsupported operation
 */
static int
cd9660_enotsupp()
{

	return (EOPNOTSUPP);
}

/*
 * Global vfs data structures for isofs
 */
#define cd9660_create \
	((int (*) __P((struct  vop_create_args *)))cd9660_enotsupp)
#define cd9660_mknod ((int (*) __P((struct  vop_mknod_args *)))cd9660_enotsupp)
#define cd9660_setattr \
	((int (*) __P((struct  vop_setattr_args *)))cd9660_enotsupp)
#define cd9660_write ((int (*) __P((struct  vop_write_args *)))cd9660_enotsupp)
#define cd9660_fsync ((int (*) __P((struct  vop_fsync_args *)))nullop)
#define cd9660_remove \
	((int (*) __P((struct  vop_remove_args *)))cd9660_enotsupp)
#define cd9660_link ((int (*) __P((struct  vop_link_args *)))cd9660_enotsupp)
#define cd9660_rename \
	((int (*) __P((struct  vop_rename_args *)))cd9660_enotsupp)
#define cd9660_mkdir ((int (*) __P((struct  vop_mkdir_args *)))cd9660_enotsupp)
#define cd9660_rmdir ((int (*) __P((struct  vop_rmdir_args *)))cd9660_enotsupp)
#define cd9660_symlink \
	((int (*) __P((struct vop_symlink_args *)))cd9660_enotsupp)
#define cd9660_pathconf \
	((int (*) __P((struct vop_pathconf_args *)))cd9660_enotsupp)
#define cd9660_advlock \
	((int (*) __P((struct vop_advlock_args *)))cd9660_enotsupp)
#define cd9660_blkatoff \
	((int (*) __P((struct  vop_blkatoff_args *)))cd9660_enotsupp)
#define cd9660_valloc ((int(*) __P(( \
		struct vnode *pvp, \
		int mode, \
		struct ucred *cred, \
		struct vnode **vpp))) cd9660_enotsupp)
#define cd9660_vfree ((int (*) __P((struct  vop_vfree_args *)))cd9660_enotsupp)
#define cd9660_truncate \
	((int (*) __P((struct  vop_truncate_args *)))cd9660_enotsupp)
#define cd9660_update \
	((int (*) __P((struct  vop_update_args *)))cd9660_enotsupp)
#define cd9660_bwrite \
	((int (*) __P((struct  vop_bwrite_args *)))cd9660_enotsupp)

/*
 * Global vfs data structures for nfs
 */
vop_t **cd9660_vnodeop_p;
static struct vnodeopv_entry_desc cd9660_vnodeop_entries[] = {
	{ &vop_default_desc, (vop_t *)vn_default_error },
	{ &vop_lookup_desc, (vop_t *)cd9660_lookup },		/* lookup */
	{ &vop_create_desc, (vop_t *)cd9660_create },		/* create */
	{ &vop_mknod_desc, (vop_t *)cd9660_mknod },		/* mknod */
	{ &vop_open_desc, (vop_t *)cd9660_open },		/* open */
	{ &vop_close_desc, (vop_t *)cd9660_close },		/* close */
	{ &vop_access_desc, (vop_t *)cd9660_access },		/* access */
	{ &vop_getattr_desc, (vop_t *)cd9660_getattr },		/* getattr */
	{ &vop_setattr_desc, (vop_t *)cd9660_setattr },		/* setattr */
	{ &vop_read_desc, (vop_t *)cd9660_read },		/* read */
	{ &vop_write_desc, (vop_t *)cd9660_write },		/* write */
	{ &vop_ioctl_desc, (vop_t *)cd9660_ioctl },		/* ioctl */
	{ &vop_select_desc, (vop_t *)cd9660_select },		/* select */
	{ &vop_mmap_desc, (vop_t *)cd9660_mmap },		/* mmap */
	{ &vop_fsync_desc, (vop_t *)cd9660_fsync },		/* fsync */
	{ &vop_seek_desc, (vop_t *)cd9660_seek },		/* seek */
	{ &vop_remove_desc, (vop_t *)cd9660_remove },		/* remove */
	{ &vop_link_desc, (vop_t *)cd9660_link },		/* link */
	{ &vop_rename_desc, (vop_t *)cd9660_rename },		/* rename */
	{ &vop_mkdir_desc, (vop_t *)cd9660_mkdir },		/* mkdir */
	{ &vop_rmdir_desc, (vop_t *)cd9660_rmdir },		/* rmdir */
	{ &vop_symlink_desc, (vop_t *)cd9660_symlink },		/* symlink */
	{ &vop_readdir_desc, (vop_t *)cd9660_readdir },		/* readdir */
	{ &vop_readlink_desc, (vop_t *)cd9660_readlink },	/* readlink */
	{ &vop_abortop_desc, (vop_t *)cd9660_abortop },		/* abortop */
	{ &vop_inactive_desc, (vop_t *)cd9660_inactive },	/* inactive */
	{ &vop_reclaim_desc, (vop_t *)cd9660_reclaim },		/* reclaim */
	{ &vop_lock_desc, (vop_t *)cd9660_lock },		/* lock */
	{ &vop_unlock_desc, (vop_t *)cd9660_unlock },		/* unlock */
	{ &vop_bmap_desc, (vop_t *)cd9660_bmap },		/* bmap */
	{ &vop_strategy_desc, (vop_t *)cd9660_strategy },	/* strategy */
	{ &vop_print_desc, (vop_t *)cd9660_print },		/* print */
	{ &vop_islocked_desc, (vop_t *)cd9660_islocked },	/* islocked */
	{ &vop_pathconf_desc, (vop_t *)cd9660_pathconf },	/* pathconf */
	{ &vop_advlock_desc, (vop_t *)cd9660_advlock },		/* advlock */
	{ &vop_blkatoff_desc, (vop_t *)cd9660_blkatoff },	/* blkatoff */
	{ &vop_valloc_desc, (vop_t *)cd9660_valloc },		/* valloc */
	{ &vop_vfree_desc, (vop_t *)cd9660_vfree },		/* vfree */
	{ &vop_truncate_desc, (vop_t *)cd9660_truncate },	/* truncate */
	{ &vop_update_desc, (vop_t *)cd9660_update },		/* update */
	{ &vop_bwrite_desc, (vop_t *)vn_bwrite },		/* bwrite */
	{ NULL, NULL }
};
static struct vnodeopv_desc cd9660_vnodeop_opv_desc =
	{ &cd9660_vnodeop_p, cd9660_vnodeop_entries };
VNODEOP_SET(cd9660_vnodeop_opv_desc);

/*
 * Special device vnode ops
 */
vop_t **cd9660_specop_p;
static struct vnodeopv_entry_desc cd9660_specop_entries[] = {
	{ &vop_default_desc, (vop_t *)vn_default_error },
	{ &vop_lookup_desc, (vop_t *)spec_lookup },		/* lookup */
	{ &vop_create_desc, (vop_t *)cd9660_create },		/* create */
	{ &vop_mknod_desc, (vop_t *)cd9660_mknod },		/* mknod */
	{ &vop_open_desc, (vop_t *)spec_open },			/* open */
	{ &vop_close_desc, (vop_t *)spec_close },		/* close */
	{ &vop_access_desc, (vop_t *)cd9660_access },		/* access */
	{ &vop_getattr_desc, (vop_t *)cd9660_getattr },		/* getattr */
	{ &vop_setattr_desc, (vop_t *)cd9660_setattr },		/* setattr */
	{ &vop_read_desc, (vop_t *)spec_read },			/* read */
	{ &vop_write_desc, (vop_t *)spec_write },		/* write */
	{ &vop_ioctl_desc, (vop_t *)spec_ioctl },		/* ioctl */
	{ &vop_select_desc, (vop_t *)spec_select },		/* select */
	{ &vop_mmap_desc, (vop_t *)spec_mmap },			/* mmap */
	{ &vop_fsync_desc, (vop_t *)spec_fsync },		/* fsync */
	{ &vop_seek_desc, (vop_t *)spec_seek },			/* seek */
	{ &vop_remove_desc, (vop_t *)cd9660_remove },		/* remove */
	{ &vop_link_desc, (vop_t *)cd9660_link },		/* link */
	{ &vop_rename_desc, (vop_t *)cd9660_rename },		/* rename */
	{ &vop_mkdir_desc, (vop_t *)cd9660_mkdir },		/* mkdir */
	{ &vop_rmdir_desc, (vop_t *)cd9660_rmdir },		/* rmdir */
	{ &vop_symlink_desc, (vop_t *)cd9660_symlink },		/* symlink */
	{ &vop_readdir_desc, (vop_t *)spec_readdir },		/* readdir */
	{ &vop_readlink_desc, (vop_t *)spec_readlink },		/* readlink */
	{ &vop_abortop_desc, (vop_t *)spec_abortop },		/* abortop */
	{ &vop_inactive_desc, (vop_t *)cd9660_inactive },	/* inactive */
	{ &vop_reclaim_desc, (vop_t *)cd9660_reclaim },		/* reclaim */
	{ &vop_lock_desc, (vop_t *)cd9660_lock },		/* lock */
	{ &vop_unlock_desc, (vop_t *)cd9660_unlock },		/* unlock */
	{ &vop_bmap_desc, (vop_t *)spec_bmap },			/* bmap */
	{ &vop_strategy_desc, (vop_t *)spec_strategy },		/* strategy */
	{ &vop_print_desc, (vop_t *)cd9660_print },		/* print */
	{ &vop_islocked_desc, (vop_t *)cd9660_islocked },	/* islocked */
	{ &vop_pathconf_desc, (vop_t *)spec_pathconf },		/* pathconf */
	{ &vop_advlock_desc, (vop_t *)spec_advlock },		/* advlock */
	{ &vop_blkatoff_desc, (vop_t *)spec_blkatoff },		/* blkatoff */
	{ &vop_valloc_desc, (vop_t *)spec_valloc },		/* valloc */
	{ &vop_vfree_desc, (vop_t *)spec_vfree },		/* vfree */
	{ &vop_truncate_desc, (vop_t *)spec_truncate },		/* truncate */
	{ &vop_update_desc, (vop_t *)cd9660_update },		/* update */
 	{ &vop_getpages_desc, (vop_t *)spec_getpages},		/* getpages */
	{ &vop_bwrite_desc, (vop_t *)vn_bwrite },		/* bwrite */
	{ NULL, NULL }
};
static struct vnodeopv_desc cd9660_specop_opv_desc =
	{ &cd9660_specop_p, cd9660_specop_entries };
VNODEOP_SET(cd9660_specop_opv_desc);

vop_t **cd9660_fifoop_p;
static struct vnodeopv_entry_desc cd9660_fifoop_entries[] = {
	{ &vop_default_desc, (vop_t *)vn_default_error },
	{ &vop_lookup_desc, (vop_t *)fifo_lookup },		/* lookup */
	{ &vop_create_desc, (vop_t *)cd9660_create },		/* create */
	{ &vop_mknod_desc, (vop_t *)cd9660_mknod },		/* mknod */
	{ &vop_open_desc, (vop_t *)fifo_open },			/* open */
	{ &vop_close_desc, (vop_t *)fifo_close },		/* close */
	{ &vop_access_desc, (vop_t *)cd9660_access },		/* access */
	{ &vop_getattr_desc, (vop_t *)cd9660_getattr },		/* getattr */
	{ &vop_setattr_desc, (vop_t *)cd9660_setattr },		/* setattr */
	{ &vop_read_desc, (vop_t *)fifo_read },			/* read */
	{ &vop_write_desc, (vop_t *)fifo_write },		/* write */
	{ &vop_ioctl_desc, (vop_t *)fifo_ioctl },		/* ioctl */
	{ &vop_select_desc, (vop_t *)fifo_select },		/* select */
	{ &vop_mmap_desc, (vop_t *)fifo_mmap },			/* mmap */
	{ &vop_fsync_desc, (vop_t *)fifo_fsync },		/* fsync */
	{ &vop_seek_desc, (vop_t *)fifo_seek },			/* seek */
	{ &vop_remove_desc, (vop_t *)cd9660_remove },		/* remove */
	{ &vop_link_desc, (vop_t *)cd9660_link },		/* link */
	{ &vop_rename_desc, (vop_t *)cd9660_rename },		/* rename */
	{ &vop_mkdir_desc, (vop_t *)cd9660_mkdir },		/* mkdir */
	{ &vop_rmdir_desc, (vop_t *)cd9660_rmdir },		/* rmdir */
	{ &vop_symlink_desc, (vop_t *)cd9660_symlink },		/* symlink */
	{ &vop_readdir_desc, (vop_t *)fifo_readdir },		/* readdir */
	{ &vop_readlink_desc, (vop_t *)fifo_readlink },		/* readlink */
	{ &vop_abortop_desc, (vop_t *)fifo_abortop },		/* abortop */
	{ &vop_inactive_desc, (vop_t *)cd9660_inactive },	/* inactive */
	{ &vop_reclaim_desc, (vop_t *)cd9660_reclaim },		/* reclaim */
	{ &vop_lock_desc, (vop_t *)cd9660_lock },		/* lock */
	{ &vop_unlock_desc, (vop_t *)cd9660_unlock },		/* unlock */
	{ &vop_bmap_desc, (vop_t *)fifo_bmap },			/* bmap */
	{ &vop_strategy_desc, (vop_t *)fifo_badop },		/* strategy */
	{ &vop_print_desc, (vop_t *)cd9660_print },		/* print */
	{ &vop_islocked_desc, (vop_t *)cd9660_islocked },	/* islocked */
	{ &vop_pathconf_desc, (vop_t *)fifo_pathconf },		/* pathconf */
	{ &vop_advlock_desc, (vop_t *)fifo_advlock },		/* advlock */
	{ &vop_blkatoff_desc, (vop_t *)fifo_blkatoff },		/* blkatoff */
	{ &vop_valloc_desc, (vop_t *)fifo_valloc },		/* valloc */
	{ &vop_vfree_desc, (vop_t *)fifo_vfree },		/* vfree */
	{ &vop_truncate_desc, (vop_t *)fifo_truncate },		/* truncate */
	{ &vop_update_desc, (vop_t *)cd9660_update },		/* update */
	{ &vop_bwrite_desc, (vop_t *)vn_bwrite },		/* bwrite */
	{ NULL, NULL }
};
static struct vnodeopv_desc cd9660_fifoop_opv_desc =
	{ &cd9660_fifoop_p, cd9660_fifoop_entries };

VNODEOP_SET(cd9660_fifoop_opv_desc);
