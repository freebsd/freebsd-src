/*
 * Copyright (c) 1999, 2000
 *	Adrian Chadd <adrian@FreeBSD.org>
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
 *	@(#)ffs_vnops.c	8.15 (Berkeley) 5/14/95
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/resourcevar.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/stat.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/conf.h>
#include <sys/poll.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <sys/dirent.h>

#include <machine/limits.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>
#include <vm/vm_zone.h>

#include <ufs/ufs/extattr.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>
#include <ufs/ufs/dir.h>

#include <ufs/ffs/fs.h>
#include <ufs/ffs/ffs_extern.h>

#include <ufs/ifs/ifs_extern.h>

/* IFS debugging */
#undef DEBUG_IFS_READDIR

/* Declare our trampling into the FFS code */
extern int	ffs_fsync (struct vop_fsync_args *);
static int	ffs_getpages(struct vop_getpages_args *);
static int	ffs_putpages(struct vop_putpages_args *);
static int	ffs_read(struct vop_read_args *);
static int	ffs_write(struct vop_write_args *);

static int	ifs_noop(struct vop_generic_args *);
static int	ifs_getattr(struct vop_getattr_args *);
static int	ifs_create(struct vop_create_args *);
static int	ifs_makeinode(int mode, struct vnode *, struct vnode **,
		    struct componentname *);
static int	ifs_remove(struct vop_remove_args *);
static int	ifs_readdir(struct vop_readdir_args *);
static int	ifs_dirremove(struct vnode *, struct inode *, int, int); 



/* Global vfs data structures for ifs. */
vop_t **ifs_vnodeop_p;
static struct vnodeopv_entry_desc ifs_vnodeop_entries[] = {
	{ &vop_default_desc,		(vop_t *) ufs_vnoperate },
	{ &vop_fsync_desc,		(vop_t *) ffs_fsync },
	{ &vop_getpages_desc,		(vop_t *) ffs_getpages },
	{ &vop_putpages_desc,		(vop_t *) ffs_putpages },
	{ &vop_read_desc,		(vop_t *) ffs_read },
	{ &vop_reallocblks_desc,	(vop_t *) ffs_reallocblks },
	{ &vop_write_desc,		(vop_t *) ffs_write },

	{ &vop_lookup_desc,		(vop_t *) ifs_lookup },
	{ &vop_getattr_desc,		(vop_t *) ifs_getattr },
	{ &vop_create_desc,		(vop_t *) ifs_create },
	{ &vop_remove_desc,		(vop_t *) ifs_remove },
	{ &vop_readdir_desc,		(vop_t *) ifs_readdir },

/* NULL operations for ifs */
	{ &vop_cachedlookup_desc,	(vop_t *) ifs_noop },
	{ &vop_mkdir_desc,		(vop_t *) ifs_noop },
	{ &vop_mknod_desc,		(vop_t *) ifs_noop },
	{ &vop_readlink_desc,		(vop_t *) ifs_noop },
	{ &vop_rename_desc,		(vop_t *) ifs_noop },
	{ &vop_rmdir_desc,		(vop_t *) ifs_noop },
	{ &vop_symlink_desc,		(vop_t *) ifs_noop },
	{ &vop_link_desc,		(vop_t *) ifs_noop },
	{ &vop_whiteout_desc,		(vop_t *) ifs_noop },

	{ NULL, NULL }
};
static struct vnodeopv_desc ifs_vnodeop_opv_desc =
	{ &ifs_vnodeop_p, ifs_vnodeop_entries };

vop_t **ifs_specop_p;
static struct vnodeopv_entry_desc ifs_specop_entries[] = {
	{ &vop_default_desc,		(vop_t *) ufs_vnoperatespec },
	{ &vop_fsync_desc,		(vop_t *) ffs_fsync },
	{ NULL, NULL }
};
static struct vnodeopv_desc ifs_specop_opv_desc =
	{ &ifs_specop_p, ifs_specop_entries };

vop_t **ifs_fifoop_p;
static struct vnodeopv_entry_desc ifs_fifoop_entries[] = {
	{ &vop_default_desc,		(vop_t *) ufs_vnoperatefifo },
	{ &vop_fsync_desc,		(vop_t *) ffs_fsync },
	{ NULL, NULL }
};
static struct vnodeopv_desc ifs_fifoop_opv_desc =
	{ &ifs_fifoop_p, ifs_fifoop_entries };

VNODEOP_SET(ifs_vnodeop_opv_desc);
VNODEOP_SET(ifs_specop_opv_desc);
VNODEOP_SET(ifs_fifoop_opv_desc);

#include <ufs/ufs/ufs_readwrite.c>


static int
ifs_noop(ap)
	struct vop_generic_args *ap;
{
	return EOPNOTSUPP;
}


/* ARGSUSED */
static int
ifs_getattr(ap)
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;
	register struct inode *ip = VTOI(vp);
	register struct vattr *vap = ap->a_vap;

	ufs_itimes(vp);
	/*
	 * Copy from inode table
	 */
	vap->va_fsid = dev2udev(ip->i_dev);
	vap->va_fileid = ip->i_number;
	vap->va_mode = ip->i_mode & ~IFMT;
	vap->va_nlink = VFSTOUFS(vp->v_mount)->um_i_effnlink_valid ?
	    ip->i_effnlink : ip->i_nlink;
	vap->va_uid = ip->i_uid;
	vap->va_gid = ip->i_gid;
	vap->va_rdev = ip->i_rdev;
	vap->va_size = ip->i_din.di_size;
	vap->va_atime.tv_sec = ip->i_atime;
	vap->va_atime.tv_nsec = ip->i_atimensec;
	vap->va_mtime.tv_sec = ip->i_mtime;
	vap->va_mtime.tv_nsec = ip->i_mtimensec;
	vap->va_ctime.tv_sec = ip->i_ctime;
	vap->va_ctime.tv_nsec = ip->i_ctimensec;
	vap->va_flags = ip->i_flags;
	vap->va_gen = ip->i_gen;
	vap->va_blocksize = vp->v_mount->mnt_stat.f_iosize;
	vap->va_bytes = dbtob((u_quad_t)ip->i_blocks);
	vap->va_type = IFTOVT(ip->i_mode);
	vap->va_filerev = ip->i_modrev;
	return (0);
}


/*
 * Create a regular file
 */
int
ifs_create(ap)
	struct vop_create_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap;
{
	int error;

	error =
	    ifs_makeinode(MAKEIMODE(ap->a_vap->va_type, ap->a_vap->va_mode),
	    ap->a_dvp, ap->a_vpp, ap->a_cnp);
	if (error)
		return (error);
	VN_POLLEVENT(ap->a_dvp, POLLWRITE);
	return (0);
}


/*
 * Allocate a new inode.
 */
int
ifs_makeinode(mode, dvp, vpp, cnp)
	int mode;
	struct vnode *dvp;
	struct vnode **vpp;
	struct componentname *cnp;
{
	register struct inode *ip, *pdir;
	struct vnode *tvp;
	int error;

	pdir = VTOI(dvp);
#ifdef DIAGNOSTIC
	if ((cnp->cn_flags & HASBUF) == 0)
		panic("ifs_makeinode: no name");
#endif
	*vpp = NULL;
	if ((mode & IFMT) == 0)
		mode |= IFREG;
	error = UFS_VALLOC(dvp, mode, cnp->cn_cred, &tvp);
	if (error) {
		zfree(namei_zone, cnp->cn_pnbuf);
		return (error);
	}
	ip = VTOI(tvp);
	ip->i_gid = pdir->i_gid;
	ip->i_uid = cnp->cn_cred->cr_uid;
#ifdef QUOTA
	if ((error = getinoquota(ip)) ||
	    (error = chkiq(ip, 1, cnp->cn_cred, 0))) {
		UFS_VFREE(tvp, ip->i_number, mode);
		vput(tvp);
		return (error);
	}
#endif
	ip->i_flag |= IN_ACCESS | IN_CHANGE | IN_UPDATE;
	ip->i_mode = mode;
	tvp->v_type = IFTOVT(mode);	/* Rest init'd in getnewvnode(). */
	ip->i_effnlink = 1;
	ip->i_nlink = 1;
	if (DOINGSOFTDEP(tvp))
		softdep_change_linkcnt(ip);
	if ((ip->i_mode & ISGID) && !groupmember(ip->i_gid, cnp->cn_cred) &&
	    suser_xxx(cnp->cn_cred, 0, 0))
		ip->i_mode &= ~ISGID;

	if (cnp->cn_flags & ISWHITEOUT)
		ip->i_flags |= UF_OPAQUE;

	/*
	 * Make sure inode goes to disk before directory entry.
	 */
	error = UFS_UPDATE(tvp, !(DOINGSOFTDEP(tvp) | DOINGASYNC(tvp)));
	if (error)
		goto bad;
	*vpp = tvp;
	return (0);
bad:
	/*
	 * Write error occurred trying to update the inode
	 * or the directory so must deallocate the inode.
	 */
	ip->i_effnlink = 0;
	ip->i_nlink = 0;
	ip->i_flag |= IN_CHANGE;
	if (DOINGSOFTDEP(tvp))
		softdep_change_linkcnt(ip);
	vput(tvp);
	return (error);
}


int
ifs_remove(ap)
	struct vop_remove_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap;
{
	struct inode *ip;
	struct vnode *vp = ap->a_vp;
	struct vnode *dvp = ap->a_dvp;
	int error;

	ip = VTOI(vp);
	if ((ip->i_flags & (NOUNLINK | IMMUTABLE | APPEND)) ||
	    (VTOI(dvp)->i_flags & APPEND)) {
		error = EPERM;
		goto out;
	}
	error = ifs_dirremove(dvp, ip, ap->a_cnp->cn_flags, 0);
	VN_POLLEVENT(vp, POLLNLINK);
out:
	return (error);
}


/*
 * highly cutdown ufs_dirremove, since we're not updating
 * any directory entries. :-)
 */
static int
ifs_dirremove(struct vnode *dvp, struct inode *ip, int flags, int isrmdir) 
{
	int error;
 
	if (ip) {
		ip->i_effnlink--;
		ip->i_flag |= IN_CHANGE;
		ip->i_nlink--;
		error = 0;
	} else
		error = ENOENT;
	return (error);
}


/*
 * ifs_readdir
 *
 * Do the directory listing, representing the allocated inodes
 * making up this filesystem.
 *
 */

static int
ifs_readdir(ap)
	struct vop_readdir_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
		int *a_eofflag;
		int *ncookies;
		u_long **a_cookies;
	} */ *ap;
{
	int inodenum;
	struct dirent *dent, *lastdp = NULL;
	struct dirent *tmpdp;
	char *dirbuf;
	struct inode *ip = VTOI(ap->a_vp); /* To get the mount info later */
	struct mount *mp = ap->a_vp->v_mount;
	struct fs *fs = VFSTOUFS(mp)->um_fs;
	int maxnuminode = fs->fs_ncg * fs->fs_ipg;
	int error = 0;
	int count;
	int dircount = 0;
	int copylen = 0;
	char iret;

	/*
	 * Get the offset, which represents the inode we're going to
	 * start from
	 */
	inodenum = ap->a_uio->uio_offset;
#ifdef DEBUG_IFS_READDIR
	printf("ifs_readdir: starting with inode %d\n", inodenum);
#endif
	if (inodenum < 0)
		return EINVAL;
	/*
	 * Next, get the buffer size, round it down to a dirent, and
	 * figure out how many allocated inodes we need to match
	 */
	count = ap->a_uio->uio_resid;
	/*
	 * Next, create a dirbuf to fill with directory entries
	 */
	MALLOC(tmpdp, struct dirent *, sizeof (struct dirent), M_TEMP, M_WAITOK);
	MALLOC(dirbuf, char *, count, M_TEMP, M_WAITOK);
	dent = (struct dirent *)dirbuf;
	/* now, keep reading until we run out of inodes */
	while (inodenum <= maxnuminode) {
		/* Get bitmap info and see if we bother with this cg */
		iret = ifs_isinodealloc(ip, inodenum);
		if (iret == IFS_INODE_EMPTYCG) {
			/* Skip this CG */
			/* Next cg please */
			inodenum -= inodenum % fs->fs_ipg;
			inodenum += fs->fs_ipg;
			continue;
		}
		/* Allocated and not special? */
		if ((inodenum > 2) && iret == IFS_INODE_ISALLOC) {
			/* Create a new entry */
			sprintf(tmpdp->d_name, "%d", inodenum);
			tmpdp->d_fileno = inodenum;
			tmpdp->d_type = DT_REG;
			tmpdp->d_namlen = strlen(tmpdp->d_name);
			tmpdp->d_reclen = DIRECTSIZ(tmpdp->d_namlen);
			/* Make sure we have enough space for this entry */
			if (tmpdp->d_reclen > count)
				break;
			/* Copy it to the given buffer */
			bcopy(tmpdp, dent, tmpdp->d_reclen);
			/* Decrement the count */
			count -= dent->d_reclen;
			copylen += dent->d_reclen;
			lastdp = dent;
			/* Increment the offset pointer */
			dent = (struct dirent *)((char *)dent + dent->d_reclen);
			dircount++;
		}
		/* Increment the inode number we are checking */
		inodenum++;
	}
	/* End */
#ifdef DEBUG_IFS_READDIR
	printf("ifs_readdir: copied %d directories\n", dircount);
#endif
	/*
	 * Get the last dent updated, and make the record d_reclen last the whole
	 * buffer.
	 */
	if (lastdp != NULL) {
		/* Update the length of the last entry */
		lastdp->d_reclen += count;
	}
	/* Copy the data out */
#ifdef DEBUG_IFS_READDIR
	printf("ifs_readdir: copied %d bytes\n", copylen);
#endif
	error = uiomove(dirbuf, copylen, ap->a_uio);
	/* Free memory we've used */
	FREE(dirbuf, M_TEMP);
	FREE(tmpdp, M_TEMP);
	/* Set uio_offset to the last inode number */
	ap->a_uio->uio_offset = inodenum;
	/* Handle EOF/eofflag */
	if ((inodenum >= maxnuminode) && (ap->a_eofflag != NULL)) {
		*ap->a_eofflag = 1;
#ifdef DEBUG_IFS_READDIR
		printf("ifs_readdir: setting EOF flag\n");
#endif
	}
#ifdef DEBUG_IFS_READDIR
	printf("ifs_readdir: new offset: %d\n", inodenum);
#endif
	return error;
}

