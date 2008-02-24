/*-
 * Copyright 2000 Hans Reiser
 * See README for licensing and copyright details
 * 
 * Ported to FreeBSD by Jean-Sébastien Pédron <jspedron@club-internet.fr>
 * 
 * $FreeBSD: src/sys/gnu/fs/reiserfs/reiserfs_vnops.c,v 1.2 2007/02/15 22:08:34 pjd Exp $
 */

#include <gnu/fs/reiserfs/reiserfs_fs.h>

static vop_access_t	reiserfs_access;
static vop_bmap_t	reiserfs_bmap;
static vop_getattr_t	reiserfs_getattr;
static vop_open_t	reiserfs_open;
static vop_pathconf_t	reiserfs_pathconf;
static vop_readlink_t	reiserfs_readlink;
static vop_strategy_t	reiserfs_strategy;
static vop_vptofh_t	reiserfs_vptofh;

/* Global vfs data structures for ReiserFS */
struct vop_vector reiserfs_vnodeops = {
	.vop_default      = &default_vnodeops,

	.vop_access       = reiserfs_access,
	.vop_bmap         = reiserfs_bmap,
	.vop_cachedlookup = reiserfs_lookup,
	.vop_getattr      = reiserfs_getattr,
	.vop_inactive     = reiserfs_inactive,
	.vop_lookup       = vfs_cache_lookup,
	.vop_open         = reiserfs_open,
	.vop_reclaim      = reiserfs_reclaim,
	.vop_read         = reiserfs_read,
	.vop_readdir      = reiserfs_readdir,
	.vop_readlink     = reiserfs_readlink,
	.vop_pathconf     = reiserfs_pathconf,
	.vop_strategy     = reiserfs_strategy,
	.vop_vptofh       = reiserfs_vptofh,
};

struct vop_vector reiserfs_specops = {
	.vop_default  = &default_vnodeops,

	.vop_access   = reiserfs_access,
	.vop_getattr  = reiserfs_getattr,
	.vop_inactive = reiserfs_inactive,
	.vop_reclaim  = reiserfs_reclaim,
};

/* -------------------------------------------------------------------
 * vnode operations
 * -------------------------------------------------------------------*/

static int
reiserfs_access(struct vop_access_args *ap)
{
	int error;
	struct vnode *vp = ap->a_vp;
	struct reiserfs_node *ip = VTOI(vp);
	mode_t mode = ap->a_mode;

	/*
	 * Disallow write attempts on read-only file systems; unless the file
	 * is a socket, fifo, or a block or character device resident on the
	 * file system.
	 */
	if (mode & VWRITE) {
		switch (vp->v_type) {
		case VDIR:
		case VLNK:
		case VREG:
			if (vp->v_mount->mnt_flag & MNT_RDONLY) {
				reiserfs_log(LOG_DEBUG,
				    "no write access (read-only fs)\n");
				return (EROFS);
			}
			break;
		default:
			break;
		}
	}

	/* If immutable bit set, nobody gets to write it. */
	if ((mode & VWRITE) && (ip->i_flags & (IMMUTABLE | SF_SNAPSHOT))) {
		reiserfs_log(LOG_DEBUG, "no write access (immutable)\n");
		return (EPERM);
	}

	error = vaccess(vp->v_type, ip->i_mode, ip->i_uid, ip->i_gid,
	    ap->a_mode, ap->a_cred, NULL);
	return (error);
}

static int
reiserfs_getattr(struct vop_getattr_args *ap)
{
	struct vnode *vp         = ap->a_vp;
	struct vattr *vap        = ap->a_vap;
	struct reiserfs_node *ip = VTOI(vp);

	vap->va_fsid      = dev2udev(ip->i_dev);
	vap->va_fileid    = ip->i_number;
	vap->va_mode      = ip->i_mode & ~S_IFMT;
	vap->va_nlink     = ip->i_nlink;
	vap->va_uid       = ip->i_uid;
	vap->va_gid       = ip->i_gid;
	//XXX vap->va_rdev      = ip->i_rdev;
	vap->va_size      = ip->i_size;
	vap->va_atime     = ip->i_atime;
	vap->va_mtime     = ip->i_mtime;
	vap->va_ctime     = ip->i_ctime;
	vap->va_flags     = ip->i_flags;
	vap->va_gen       = ip->i_generation;
	vap->va_blocksize = vp->v_mount->mnt_stat.f_iosize;
	vap->va_bytes     = dbtob((u_quad_t)ip->i_blocks);
	vap->va_type      = vp->v_type;
	//XXX vap->va_filerev   = ip->i_modrev;

	return (0);
}

/* Return POSIX pathconf information applicable to ReiserFS filesystems */
static int
reiserfs_pathconf(struct vop_pathconf_args *ap)
{
	switch (ap->a_name) {
	case _PC_LINK_MAX:
		*ap->a_retval = REISERFS_LINK_MAX;
		return (0);
	case _PC_NAME_MAX:
		*ap->a_retval =
		    REISERFS_MAX_NAME(VTOI(ap->a_vp)->i_reiserfs->s_blocksize);
		return (0);
	case _PC_PATH_MAX:
		*ap->a_retval = PATH_MAX;
		return (0);
	case _PC_PIPE_BUF:
		*ap->a_retval = PIPE_BUF;
		return (0);
	case _PC_CHOWN_RESTRICTED:
		*ap->a_retval = 1;
		return (0);
	case _PC_NO_TRUNC:
		*ap->a_retval = 1;
		return (0);
	default:
		return (EINVAL);
	}
}

static int
reiserfs_open(struct vop_open_args *ap)
{
	/* Files marked append-only must be opened for appending. */
	if ((VTOI(ap->a_vp)->i_flags & APPEND) &&
	    (ap->a_mode & (FWRITE | O_APPEND)) == FWRITE)
		return (EPERM);

	vnode_create_vobject(ap->a_vp, VTOI(ap->a_vp)->i_size, ap->a_td);

	return (0);
}

/* Return target name of a symbolic link */
static int
reiserfs_readlink(struct vop_readlink_args *ap)
{
	struct vnode *vp = ap->a_vp;

	reiserfs_log(LOG_DEBUG, "redirect to VOP_READ()\n");
	return (VOP_READ(vp, ap->a_uio, 0, ap->a_cred));
}

/* Bmap converts the logical block number of a file to its physical
 * block number on the disk. */
static int
reiserfs_bmap(ap)
	struct vop_bmap_args /* {
				struct vnode *a_vp;
				daddr_t a_bn;
				struct bufobj **a_bop;
				daddr_t *a_bnp;
				int *a_runp;
				int *a_runb;
				} */ *ap;
{
	daddr_t blkno;
	struct buf *bp;
	struct cpu_key key;
	struct item_head *ih;

	struct vnode *vp = ap->a_vp;
	struct reiserfs_node *ip = VTOI(vp);
	struct reiserfs_sb_info *sbi = ip->i_reiserfs;
	INITIALIZE_PATH(path);

	/* Prepare the key to look for the 'block'-th block of file
	 * (XXX we suppose that statfs.f_iosize == sbi->s_blocksize) */
	make_cpu_key(&key, ip, (off_t)ap->a_bn * sbi->s_blocksize + 1,
	    TYPE_ANY, 3);

	/* Search item */
	if (search_for_position_by_key(sbi, &key, &path) != POSITION_FOUND) {
		reiserfs_log(LOG_DEBUG, "position not found\n");
		pathrelse(&path);
		return (ENOENT);
	}

	bp = get_last_bp(&path);
	ih = get_ih(&path);

	if (is_indirect_le_ih(ih)) {
		/* Indirect item can be read by the underlying layer, instead of
		 * VOP_STRATEGY. */
		int i;
		uint32_t *ind_item = (uint32_t *)B_I_PITEM(bp, ih);
		reiserfs_log(LOG_DEBUG, "found an INDIRECT item\n");
		blkno = get_block_num(ind_item, path.pos_in_item);

		/* Read-ahead */
		if (ap->a_runb) {
			uint32_t count = 0;
			for (i = path.pos_in_item - 1; i >= 0; --i) {
				if ((blkno - get_block_num(ind_item, i)) !=
				    count + 1)
					break;
				++count;
			}

			/*
			 * This count isn't expressed in DEV_BSIZE base but
			 * in fs' own block base
			 * (see sys/vm/vnode_pager.c:vnode_pager_addr())
			 */
			*ap->a_runb = count;
			reiserfs_log(LOG_DEBUG,
			    " read-ahead: %d blocks before\n", *ap->a_runb);
		}
		if (ap->a_runp) {
			uint32_t count = 0;
			/*
			 * ih is an uint32_t array, that's why we use
			 * its length (in bytes) divided by 4 to know
			 * the number of items
			 */
			for (i = path.pos_in_item + 1;
			    i < ih_item_len(ih) / 4; ++i) {
				if ((get_block_num(ind_item, i) - blkno) !=
				    count + 1)
					break;
				++count;
			}

			/*
			 * This count isn't expressed in DEV_BSIZE base but
			 * in fs' own block base
			 * (see sys/vm/vnode_pager.c:vnode_pager_addr()) */
			*ap->a_runp = count;
			reiserfs_log(LOG_DEBUG,
			    " read-ahead: %d blocks after\n", *ap->a_runp);
		}

		/* Indirect items can be read using the device VOP_STRATEGY */
		if (ap->a_bop)
			*ap->a_bop = &VTOI(ap->a_vp)->i_devvp->v_bufobj;

		/* Convert the block number into DEV_BSIZE base */
		blkno *= btodb(sbi->s_blocksize);
	} else {
		/*
		 * Direct item are not DEV_BSIZE aligned, VOP_STRATEGY will
		 * have to handle this case specifically
		 */
		reiserfs_log(LOG_DEBUG, "found a DIRECT item\n");
		blkno = ap->a_bn;

		if (ap->a_runp)
			*ap->a_runp = 0;
		if (ap->a_runb)
			*ap->a_runb = 0;

		/* Direct item must be read by reiserfs_strategy */
		if (ap->a_bop)
			*ap->a_bop = &vp->v_bufobj;
	}

	if (ap->a_bnp)
		*ap->a_bnp = blkno;

	pathrelse(&path);

	if (ap->a_bnp) {
		reiserfs_log(LOG_DEBUG, "logical block: %ju (%ju),"
		    " physical block: %ju (%ju)\n",
		    (intmax_t)ap->a_bn,
		    (intmax_t)(ap->a_bn / btodb(sbi->s_blocksize)),
		    (intmax_t)*ap->a_bnp,
		    (intmax_t)(*ap->a_bnp / btodb(sbi->s_blocksize)));
	}

	return (0);
}

/* Does simply the same as reiserfs_read. It's called when reiserfs_bmap find
 * an direct item. */
static int
reiserfs_strategy(struct vop_strategy_args /* {
					      struct vnode *a_vp;
					      struct buf *a_bp;
					      } */ *ap)
{
	int error;
	struct uio auio;
	struct iovec aiov;
	struct reiserfs_node *ip;
	struct buf *bp = ap->a_bp;
	struct vnode *vp = ap->a_vp;

	reiserfs_log(LOG_DEBUG, "logical block: %ju,"
	    " physical block: %ju\n", (intmax_t)bp->b_lblkno,
	    (intmax_t)bp->b_blkno);

	ip = VTOI(vp);

	if (bp->b_iocmd == BIO_READ) {
		/* Prepare the uio structure */
		reiserfs_log(LOG_DEBUG, "prepare uio structure\n");
		aiov.iov_base = bp->b_data;
		aiov.iov_len  = MIN(bp->b_bcount, ip->i_size);
		reiserfs_log(LOG_DEBUG, "  vector length: %ju\n",
		    (intmax_t)aiov.iov_len);

		auio.uio_iov    = &aiov;
		auio.uio_iovcnt = 1;
		auio.uio_offset = 0;
		auio.uio_rw     = UIO_READ;
		auio.uio_segflg = UIO_SYSSPACE;
		auio.uio_td     = curthread;
		auio.uio_resid  = bp->b_bcount;
		reiserfs_log(LOG_DEBUG, "  buffer length: %u\n",
		    auio.uio_resid);

		reiserfs_log(LOG_DEBUG, "reading block #%ju\n",
		    (intmax_t)bp->b_blkno);
		error = reiserfs_get_block(ip, bp->b_blkno, 0, &auio);
	} else {
		/* No write support yet */
		error = (EOPNOTSUPP);
		bp->b_error    = error;
		bp->b_ioflags |= BIO_ERROR;
	}

	bufdone(bp);
	return (error);
}

/*
 * Vnode pointer to File handle
 */
static int
reiserfs_vptofh(struct vop_vptofh_args /* {
					  struct vnode *a_vp;
					  struct fid *a_fhp;
					  } */ *ap)
{
	struct rfid *rfhp;
	struct reiserfs_node *ip;

	ip = VTOI(ap->a_vp);
	reiserfs_log(LOG_DEBUG,
	    "fill *fhp with inode (dirid=%d, objectid=%d)\n",
	    ip->i_ino, ip->i_number);

	rfhp = (struct rfid *)ap->a_fhp;
	rfhp->rfid_len      = sizeof(struct rfid);
	rfhp->rfid_dirid    = ip->i_ino;
	rfhp->rfid_objectid = ip->i_number;
	rfhp->rfid_gen      = ip->i_generation;

	reiserfs_log(LOG_DEBUG, "return it\n");
	return (0);
}
