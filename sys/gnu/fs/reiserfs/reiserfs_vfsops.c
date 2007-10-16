/*-
 * Copyright 2000 Hans Reiser
 * See README for licensing and copyright details
 * 
 * Ported to FreeBSD by Jean-Sébastien Pédron <jspedron@club-internet.fr>
 * 
 * $FreeBSD$
 */

#include <gnu/fs/reiserfs/reiserfs_fs.h>

const char reiserfs_3_5_magic_string[] = REISERFS_SUPER_MAGIC_STRING;
const char reiserfs_3_6_magic_string[] = REISER2FS_SUPER_MAGIC_STRING;
const char reiserfs_jr_magic_string[]  = REISER2FS_JR_SUPER_MAGIC_STRING;

/*
 * Default recommended I/O size is 128k. There might be broken
 * applications that are confused by this. Use nolargeio mount option to
 * get usual i/o size = PAGE_SIZE.
 */
int reiserfs_default_io_size = 128 * 1024;

static vfs_cmount_t	reiserfs_cmount;
static vfs_fhtovp_t	reiserfs_fhtovp;
static vfs_mount_t	reiserfs_mount;
static vfs_root_t	reiserfs_root;
static vfs_statfs_t	reiserfs_statfs;
static vfs_unmount_t	reiserfs_unmount;

static int	reiserfs_mountfs(struct vnode *devvp, struct mount *mp,
		    struct thread *td);
static void	load_bitmap_info_data(struct reiserfs_sb_info *sbi,
		    struct reiserfs_bitmap_info *bi);
static int	read_bitmaps(struct reiserfs_mount *rmp);
static int	read_old_bitmaps(struct reiserfs_mount *rmp);
static int	read_super_block(struct reiserfs_mount *rmp, int offset);
static hashf_t	hash_function(struct reiserfs_mount *rmp);

static int	get_root_node(struct reiserfs_mount *rmp,
		    struct reiserfs_node **root);
uint32_t	find_hash_out(struct reiserfs_mount *rmp);

MALLOC_DEFINE(M_REISERFSMNT, "reiserfs_mount", "ReiserFS mount structure");
MALLOC_DEFINE(M_REISERFSPATH, "reiserfs_path", "ReiserFS path structure");
MALLOC_DEFINE(M_REISERFSNODE, "reiserfs_node", "ReiserFS vnode private part");

/* -------------------------------------------------------------------
 * VFS operations
 * -------------------------------------------------------------------*/

static int
reiserfs_cmount(struct mntarg *ma, void *data, int flags, struct thread *td)
{
	struct reiserfs_args args;
	int error;

	error = copyin(data, &args, sizeof(args));
	if (error)
		return (error);

	ma = mount_argsu(ma, "from", args.fspec, MAXPATHLEN);
	ma = mount_arg(ma, "export", &args.export, sizeof args.export);

	error = kernel_mount(ma, flags);

	return (error);
}

/*
 * Mount system call
 */
static int
reiserfs_mount(struct mount *mp, struct thread *td)
{
	size_t size;
	int error, len;
	mode_t accessmode;
	char *path, *fspec;
	struct vnode *devvp;
	struct vfsoptlist *opts;
	struct reiserfs_mount *rmp;
	struct reiserfs_sb_info *sbi;
	struct nameidata nd, *ndp = &nd;

	if (!(mp->mnt_flag & MNT_RDONLY))
		return EROFS;

	/* Get the new options passed to mount */
	opts = mp->mnt_optnew;

	/* `fspath' contains the mount point (eg. /mnt/linux); REQUIRED */
	vfs_getopt(opts, "fspath", (void **)&path, NULL);
	reiserfs_log(LOG_INFO, "mount point is `%s'\n", path);

	/* `from' contains the device name (eg. /dev/ad0s1); REQUIRED */
	fspec = NULL;
	error = vfs_getopt(opts, "from", (void **)&fspec, &len);
	if (!error && fspec[len - 1] != '\0')
		return (EINVAL);
	reiserfs_log(LOG_INFO, "device is `%s'\n", fspec);

	/* Handle MNT_UPDATE (mp->mnt_flag) */
	if (mp->mnt_flag & MNT_UPDATE) {
		/* For now, only NFS export is supported. */
		if (vfs_flagopt(opts, "export", NULL, 0))
			return (0);
	}

	/* Not an update, or updating the name: look up the name
	 * and verify that it refers to a sensible disk device. */
	if (fspec == NULL)
		return (EINVAL);

	NDINIT(ndp, LOOKUP, FOLLOW | LOCKLEAF, UIO_SYSSPACE, fspec, td);
	if ((error = namei(ndp)) != 0)
		return (error);
	NDFREE(ndp, NDF_ONLY_PNBUF);
	devvp = ndp->ni_vp;

	if (!vn_isdisk(devvp, &error)) {
		vput(devvp);
		return (error);
	}

	/* If mount by non-root, then verify that user has necessary
	 * permissions on the device. */
	accessmode = VREAD;
	if ((mp->mnt_flag & MNT_RDONLY) == 0)
		accessmode |= VWRITE;
	error = VOP_ACCESS(devvp, accessmode, td->td_ucred, td);
	if (error)
		error = priv_check(td, PRIV_VFS_MOUNT_PERM);
	if (error) {
		vput(devvp);
		return (error);
	}

	if ((mp->mnt_flag & MNT_UPDATE) == 0) {
		error = reiserfs_mountfs(devvp, mp, td);
	} else {
		/* TODO Handle MNT_UPDATE */
		vput(devvp);
		return (EOPNOTSUPP);
	}

	if (error) {
		vrele(devvp);
		return (error);
	}

	rmp = VFSTOREISERFS(mp);
	sbi = rmp->rm_reiserfs;

	/*
	 * Note that this strncpy() is ok because of a check at the start
	 * of reiserfs_mount().
	 */
	reiserfs_log(LOG_DEBUG, "prepare statfs data\n");
	(void)copystr(fspec, mp->mnt_stat.f_mntfromname, MNAMELEN - 1, &size);
	bzero(mp->mnt_stat.f_mntfromname + size, MNAMELEN - size);
	(void)reiserfs_statfs(mp, &mp->mnt_stat, td);

	reiserfs_log(LOG_DEBUG, "done\n");
	return (0);
}

/*
 * Unmount system call
 */
static int
reiserfs_unmount(struct mount *mp, int mntflags, struct thread *td)
{
	int error, flags = 0;
	struct reiserfs_mount *rmp;
	struct reiserfs_sb_info *sbi;

	reiserfs_log(LOG_DEBUG, "get private data\n");
	rmp = VFSTOREISERFS(mp);
	sbi = rmp->rm_reiserfs;

	/* Flangs handling */
	reiserfs_log(LOG_DEBUG, "handle mntflags\n");
	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;

	/* Flush files -> vflush */
	reiserfs_log(LOG_DEBUG, "flush vnodes\n");
	if ((error = vflush(mp, 0, flags, td)))
		return (error);

	/* XXX Super block update */

	if (sbi) {
		if (SB_AP_BITMAP(sbi)) {
			int i;
			reiserfs_log(LOG_DEBUG,
			    "release bitmap buffers (total: %d)\n",
			    SB_BMAP_NR(sbi));
			for (i = 0; i < SB_BMAP_NR(sbi); i++) {
				if (SB_AP_BITMAP(sbi)[i].bp_data) {
					free(SB_AP_BITMAP(sbi)[i].bp_data,
					    M_REISERFSMNT);
					SB_AP_BITMAP(sbi)[i].bp_data = NULL;
				}
			}

			reiserfs_log(LOG_DEBUG, "free bitmaps structure\n");
			free(SB_AP_BITMAP(sbi), M_REISERFSMNT);
			SB_AP_BITMAP(sbi) = NULL;
		}

		if (sbi->s_rs) {
			reiserfs_log(LOG_DEBUG, "free super block data\n");
			free(sbi->s_rs, M_REISERFSMNT);
			sbi->s_rs = NULL;
		}
	}

	reiserfs_log(LOG_DEBUG, "close device\n");
#if defined(si_mountpoint)
	rmp->rm_devvp->v_rdev->si_mountpoint = NULL;
#endif

	DROP_GIANT();
	g_topology_lock();
	g_wither_geom_close(rmp->rm_cp->geom, ENXIO);
	g_topology_unlock();
	PICKUP_GIANT();
	vrele(rmp->rm_devvp);

	if (sbi) {
		reiserfs_log(LOG_DEBUG, "free sbi\n");
		free(sbi, M_REISERFSMNT);
		sbi = rmp->rm_reiserfs = NULL;
	}
	if (rmp) {
		reiserfs_log(LOG_DEBUG, "free rmp\n");
		free(rmp, M_REISERFSMNT);
		rmp = NULL;
	}

	mp->mnt_data  = 0;
	MNT_ILOCK(mp);
	mp->mnt_flag &= ~MNT_LOCAL;
	MNT_IUNLOCK(mp);

	reiserfs_log(LOG_DEBUG, "done\n");
	return (error);
}

/*
 * Return the root of a filesystem.
 */ 
static int
reiserfs_root(struct mount *mp, int flags, struct vnode **vpp,
    struct thread *td)
{
	int error;
	struct vnode *vp;
	struct cpu_key rootkey;

	rootkey.on_disk_key.k_dir_id = REISERFS_ROOT_PARENT_OBJECTID;
	rootkey.on_disk_key.k_objectid = REISERFS_ROOT_OBJECTID;

	error = reiserfs_iget(mp, &rootkey, &vp, td);

	if (error == 0)
		*vpp = vp;
	return (error);
}

/*
 * The statfs syscall
 */
static int
reiserfs_statfs(struct mount *mp, struct statfs *sbp, struct thread *td)
{
	struct reiserfs_mount *rmp;
	struct reiserfs_sb_info *sbi;
	struct reiserfs_super_block *rs;

	reiserfs_log(LOG_DEBUG, "get private data\n");
	rmp = VFSTOREISERFS(mp);
	sbi = rmp->rm_reiserfs;
	rs  = sbi->s_rs;

	reiserfs_log(LOG_DEBUG, "fill statfs structure\n");
	sbp->f_bsize  = sbi->s_blocksize;
	sbp->f_iosize = sbp->f_bsize;
	sbp->f_blocks = sb_block_count(rs) - sb_bmap_nr(rs) - 1;
	sbp->f_bfree  = sb_free_blocks(rs);
	sbp->f_bavail = sbp->f_bfree;
	sbp->f_files  = 0;
	sbp->f_ffree  = 0;
	reiserfs_log(LOG_DEBUG, "  block size   = %ju\n",
	    (intmax_t)sbp->f_bsize);
	reiserfs_log(LOG_DEBUG, "  IO size      = %ju\n",
	    (intmax_t)sbp->f_iosize);
	reiserfs_log(LOG_DEBUG, "  block count  = %ju\n",
	    (intmax_t)sbp->f_blocks);
	reiserfs_log(LOG_DEBUG, "  free blocks  = %ju\n",
	    (intmax_t)sbp->f_bfree);
	reiserfs_log(LOG_DEBUG, "  avail blocks = %ju\n",
	    (intmax_t)sbp->f_bavail);
	reiserfs_log(LOG_DEBUG, "...done\n");

	if (sbp != &mp->mnt_stat) {
		reiserfs_log(LOG_DEBUG, "copying monut point info\n");
		sbp->f_type = mp->mnt_vfc->vfc_typenum;
		bcopy((caddr_t)mp->mnt_stat.f_mntonname,
		    (caddr_t)&sbp->f_mntonname[0], MNAMELEN);
		bcopy((caddr_t)mp->mnt_stat.f_mntfromname,
		    (caddr_t)&sbp->f_mntfromname[0], MNAMELEN);
		reiserfs_log(LOG_DEBUG, "  mount from: %s\n",
		    sbp->f_mntfromname);
		reiserfs_log(LOG_DEBUG, "  mount on:   %s\n",
		    sbp->f_mntonname);
		reiserfs_log(LOG_DEBUG, "...done\n");
	}

	return (0);
}

/*
 * File handle to vnode
 *
 * Have to be really careful about stale file handles:
 * - check that the inode key is valid
 * - call ffs_vget() to get the locked inode
 * - check for an unallocated inode (i_mode == 0)
 * - check that the given client host has export rights and return
 *   those rights via. exflagsp and credanonp
 */
static int
reiserfs_fhtovp(struct mount *mp, struct fid *fhp, struct vnode **vpp)
{
	int error;
	struct rfid *rfhp;
	struct vnode *nvp;
	struct cpu_key key;
	struct reiserfs_node *ip;
	struct reiserfs_sb_info *sbi;
	struct thread *td = curthread;

	rfhp = (struct rfid *)fhp;
	sbi  = VFSTOREISERFS(mp)->rm_reiserfs;

	/* Check that the key is valid */
	if (rfhp->rfid_dirid < REISERFS_ROOT_PARENT_OBJECTID &&
	    rfhp->rfid_objectid < REISERFS_ROOT_OBJECTID)
		return (ESTALE);

	reiserfs_log(LOG_DEBUG,
	    "file handle key is (dirid=%d, objectid=%d)\n",
	    rfhp->rfid_dirid, rfhp->rfid_objectid);
	key.on_disk_key.k_dir_id   = rfhp->rfid_dirid;
	key.on_disk_key.k_objectid = rfhp->rfid_objectid;

	reiserfs_log(LOG_DEBUG, "read this inode\n");
	error = reiserfs_iget(mp, &key, &nvp, td);
	if (error) {
		*vpp = NULLVP;
		return (error);
	}

	reiserfs_log(LOG_DEBUG, "check validity\n");
	ip = VTOI(nvp);
	if (ip->i_mode == 0 || ip->i_generation != rfhp->rfid_gen) {
		vput(nvp);
		*vpp = NULLVP;
		return (ESTALE);
	}

	reiserfs_log(LOG_DEBUG, "return it\n");
	*vpp = nvp;
	return (0);
}

/* -------------------------------------------------------------------
 * Functions for the journal
 * -------------------------------------------------------------------*/

int
is_reiserfs_3_5(struct reiserfs_super_block *rs)
{

	return (!strncmp(rs->s_v1.s_magic, reiserfs_3_5_magic_string,
	    strlen(reiserfs_3_5_magic_string)));
}

int
is_reiserfs_3_6(struct reiserfs_super_block *rs)
{

	return (!strncmp(rs->s_v1.s_magic, reiserfs_3_6_magic_string,
	    strlen(reiserfs_3_6_magic_string)));
}

int
is_reiserfs_jr(struct reiserfs_super_block *rs)
{

	return (!strncmp(rs->s_v1.s_magic, reiserfs_jr_magic_string,
	    strlen(reiserfs_jr_magic_string)));
}

static int
is_any_reiserfs_magic_string(struct reiserfs_super_block *rs)
{

	return ((is_reiserfs_3_5(rs) || is_reiserfs_3_6(rs) ||
	    is_reiserfs_jr(rs)));
}

/* -------------------------------------------------------------------
 * Internal functions
 * -------------------------------------------------------------------*/

/*
 * Common code for mount and mountroot
 */ 
static int
reiserfs_mountfs(struct vnode *devvp, struct mount *mp, struct thread *td)
{
	int error, old_format = 0;
	struct reiserfs_mount *rmp;
	struct reiserfs_sb_info *sbi;
	struct reiserfs_super_block *rs;
	struct cdev *dev = devvp->v_rdev;

#if (__FreeBSD_version >= 600000)
	struct g_consumer *cp;
	struct bufobj *bo;
#endif

	//ronly = (mp->mnt_flag & MNT_RDONLY) != 0;

#if (__FreeBSD_version < 600000)
	/*
	 * Disallow multiple mounts of the same device.
	 * Disallow mounting of a device that is currently in use
	 * (except for root, which might share swap device for miniroot).
	 * Flush out any old buffers remaining from a previous use.
	 */
	if ((error = vfs_mountedon(devvp)) != 0)
		return (error);
	if (vcount(devvp) > 1)
		return (EBUSY);

	error = vinvalbuf(devvp, V_SAVE, td->td_ucred, td, 0, 0);
	if (error) {
		VOP_UNLOCK(devvp, 0, td);
		return (error);
	}

	/*
	 * Open the device in read-only, 'cause we don't support write
	 * for now
	 */
	error = VOP_OPEN(devvp, FREAD, FSCRED, td, NULL);
	VOP_UNLOCK(devvp, 0, td);
	if (error)
		return (error);
#else
	DROP_GIANT();
	g_topology_lock();
	error = g_vfs_open(devvp, &cp, "reiserfs", /* read-only */ 0);
	g_topology_unlock();
	PICKUP_GIANT();
	VOP_UNLOCK(devvp, 0, td);
	if (error)
		return (error);

	bo = &devvp->v_bufobj;
	bo->bo_private = cp;
	bo->bo_ops = g_vfs_bufops;
#endif

	if (devvp->v_rdev->si_iosize_max != 0)
		mp->mnt_iosize_max = devvp->v_rdev->si_iosize_max;
	if (mp->mnt_iosize_max > MAXPHYS)
		mp->mnt_iosize_max = MAXPHYS;

	rmp = NULL;
	sbi = NULL;

	/* rmp contains any information about this specific mount */
	rmp = malloc(sizeof *rmp, M_REISERFSMNT, M_WAITOK | M_ZERO);
	if (!rmp) {
		error = (ENOMEM);
		goto out;
	}
	sbi = malloc(sizeof *sbi, M_REISERFSMNT, M_WAITOK | M_ZERO);
	if (!sbi) {
		error = (ENOMEM);
		goto out;
	}
	rmp->rm_reiserfs = sbi;
	rmp->rm_mountp   = mp;
	rmp->rm_devvp    = devvp;
	rmp->rm_dev      = dev;
#if (__FreeBSD_version >= 600000)
	rmp->rm_bo       = &devvp->v_bufobj;
	rmp->rm_cp       = cp;
#endif

	/* Set default values for options: non-aggressive tails */
	REISERFS_SB(sbi)->s_mount_opt = (1 << REISERFS_SMALLTAIL);
	REISERFS_SB(sbi)->s_rd_only   = 1;
	REISERFS_SB(sbi)->s_devvp     = devvp;

	/* Read the super block */
	if ((error = read_super_block(rmp, REISERFS_OLD_DISK_OFFSET)) == 0) {
		/* The read process succeeded, it's an old format */
		old_format = 1;
	} else if ((error = read_super_block(rmp, REISERFS_DISK_OFFSET)) != 0) {
		reiserfs_log(LOG_ERR, "can not find a ReiserFS filesystem\n");
		goto out;
	}

	rs = SB_DISK_SUPER_BLOCK(sbi);

	/*
	 * Let's do basic sanity check to verify that underlying device is
	 * not smaller than the filesystem. If the check fails then abort and
	 * scream, because bad stuff will happen otherwise.
	 */
#if 0
	if (s->s_bdev && s->s_bdev->bd_inode &&
	    i_size_read(s->s_bdev->bd_inode) <
	    sb_block_count(rs) * sb_blocksize(rs)) {
		reiserfs_log(LOG_ERR,
		    "reiserfs: filesystem cannot be mounted because it is "
		    "bigger than the device.\n");
		reiserfs_log(LOG_ERR, "reiserfs: you may need to run fsck "
		    "rr may be you forgot to reboot after fdisk when it "
		    "told you to.\n");
		goto out;
	}
#endif

	/*
	 * XXX This is from the original Linux code, but why affecting 2 values
	 * to the same variable?
	 */
	sbi->s_mount_state = SB_REISERFS_STATE(sbi);
	sbi->s_mount_state = REISERFS_VALID_FS;

	if ((error = (old_format ?
	    read_old_bitmaps(rmp) : read_bitmaps(rmp)))) {
		reiserfs_log(LOG_ERR, "unable to read bitmap\n");
		goto out;
	}

	/* Make data=ordered the default */
	if (!reiserfs_data_log(sbi) && !reiserfs_data_ordered(sbi) &&
	    !reiserfs_data_writeback(sbi)) {
		REISERFS_SB(sbi)->s_mount_opt |= (1 << REISERFS_DATA_ORDERED);
	}

	if (reiserfs_data_log(sbi)) {
		reiserfs_log(LOG_INFO, "using journaled data mode\n");
	} else if (reiserfs_data_ordered(sbi)) {
		reiserfs_log(LOG_INFO, "using ordered data mode\n");
	} else {
		reiserfs_log(LOG_INFO, "using writeback data mode\n");
	}

	/* TODO Not yet supported */
#if 0
	if(journal_init(sbi, jdev_name, old_format, commit_max_age)) {
		reiserfs_log(LOG_ERR, "unable to initialize journal space\n");
		goto out;
	} else {
		jinit_done = 1 ; /* once this is set, journal_release must
				    be called if we error out of the mount */
	}

	if (reread_meta_blocks(sbi)) {
		reiserfs_log(LOG_ERR,
		    "unable to reread meta blocks after journal init\n");
		goto out;
	}
#endif

	/* Define and initialize hash function */
	sbi->s_hash_function = hash_function(rmp);

	if (sbi->s_hash_function == NULL) {
		reiserfs_log(LOG_ERR, "couldn't determined hash function\n");
		error = (EINVAL);
		goto out;
	}

	if (is_reiserfs_3_5(rs) ||
	    (is_reiserfs_jr(rs) && SB_VERSION(sbi) == REISERFS_VERSION_1))
		bit_set(&(sbi->s_properties), REISERFS_3_5);
	else
		bit_set(&(sbi->s_properties), REISERFS_3_6);

	mp->mnt_data = rmp;
	mp->mnt_stat.f_fsid.val[0] = dev2udev(dev);
	mp->mnt_stat.f_fsid.val[1] = mp->mnt_vfc->vfc_typenum;
	MNT_ILOCK(mp);
	mp->mnt_flag |= MNT_LOCAL;
	MNT_IUNLOCK(mp);
#if defined(si_mountpoint)
	devvp->v_rdev->si_mountpoint = mp;
#endif

	return (0);

out:
	reiserfs_log(LOG_INFO, "*** error during mount ***\n");
	if (sbi) {
		if (SB_AP_BITMAP(sbi)) {
			int i;
			for (i = 0; i < SB_BMAP_NR(sbi); i++) {
				if (!SB_AP_BITMAP(sbi)[i].bp_data)
					break;
				free(SB_AP_BITMAP(sbi)[i].bp_data, M_REISERFSMNT);
			}
			free(SB_AP_BITMAP(sbi), M_REISERFSMNT);
		}

		if (sbi->s_rs) {
			free(sbi->s_rs, M_REISERFSMNT);
			sbi->s_rs = NULL;
		}
	}

#if (__FreeBSD_version < 600000)
	(void)VOP_CLOSE(devvp, FREAD, NOCRED, td);
#else
	if (cp != NULL) {
		DROP_GIANT();
		g_topology_lock();
		g_wither_geom_close(cp->geom, ENXIO);
		g_topology_unlock();
		PICKUP_GIANT();
	}
#endif

	if (sbi)
		free(sbi, M_REISERFSMNT);
	if (rmp)
		free(rmp, M_REISERFSMNT);
	return (error);
}

/*
 * Read the super block
 */
static int
read_super_block(struct reiserfs_mount *rmp, int offset)
{
	struct buf *bp;
	int error, bits;
	struct reiserfs_super_block *rs;
	struct reiserfs_sb_info *sbi;
	uint16_t fs_blocksize;

	if (offset == REISERFS_OLD_DISK_OFFSET) {
		reiserfs_log(LOG_DEBUG,
		    "reiserfs/super: read old format super block\n");
	} else {
		reiserfs_log(LOG_DEBUG,
		    "reiserfs/super: read new format super block\n");
	}

	/* Read the super block */
	if ((error = bread(rmp->rm_devvp, offset * btodb(REISERFS_BSIZE),
	    REISERFS_BSIZE, NOCRED, &bp)) != 0) {
		reiserfs_log(LOG_ERR, "can't read device\n");
		return (error);
	}

	/* Get it from the buffer data */
	rs = (struct reiserfs_super_block *)bp->b_data;
	if (!is_any_reiserfs_magic_string(rs)) {
		brelse(bp);
		return (EINVAL);
	}

	fs_blocksize = sb_blocksize(rs);
	brelse(bp);
	bp = NULL;

	if (fs_blocksize <= 0) {
		reiserfs_log(LOG_ERR, "unexpected null block size");
		return (EINVAL);
	}

	/* Read the super block (for double check)
	 * We can't read the same blkno with a different size: it causes
	 * panic() if INVARIANTS is set. So we keep REISERFS_BSIZE */
	if ((error = bread(rmp->rm_devvp,
	    offset * REISERFS_BSIZE / fs_blocksize * btodb(fs_blocksize),
	    REISERFS_BSIZE, NOCRED, &bp)) != 0) {
		reiserfs_log(LOG_ERR, "can't reread the super block\n");
		return (error);
	}

	rs = (struct reiserfs_super_block *)bp->b_data;
	if (sb_blocksize(rs) != fs_blocksize) {
		reiserfs_log(LOG_ERR, "unexpected block size "
		    "(found=%u, expected=%u)\n",
		    sb_blocksize(rs), fs_blocksize);
		brelse(bp);
		return (EINVAL);
	}

	reiserfs_log(LOG_DEBUG, "magic: `%s'\n", rs->s_v1.s_magic);
	reiserfs_log(LOG_DEBUG, "label: `%s'\n", rs->s_label);
	reiserfs_log(LOG_DEBUG, "block size:     %6d\n", sb_blocksize(rs));
	reiserfs_log(LOG_DEBUG, "block count:    %6u\n",
	    rs->s_v1.s_block_count);
	reiserfs_log(LOG_DEBUG, "bitmaps number: %6u\n",
	    rs->s_v1.s_bmap_nr);

	if (rs->s_v1.s_root_block == -1) {
		log(LOG_ERR,
		    "reiserfs: Unfinished reiserfsck --rebuild-tree run "
		    "detected. Please\n"
		    "run reiserfsck --rebuild-tree and wait for a "
		    "completion. If that\n"
		    "fails, get newer reiserfsprogs package");
		brelse(bp);
		return (EINVAL);
	}

	sbi = rmp->rm_reiserfs;
	sbi->s_blocksize = fs_blocksize;

	for (bits = 9, fs_blocksize >>= 9; fs_blocksize >>= 1; bits++)
		;
	sbi->s_blocksize_bits = bits;

	/* Copy the buffer and release it */
	sbi->s_rs = malloc(sizeof *rs, M_REISERFSMNT, M_WAITOK | M_ZERO);
	if (!sbi->s_rs) {
		reiserfs_log(LOG_ERR, "can not read the super block\n");
		brelse(bp);
		return (ENOMEM);
	}
	bcopy(rs, sbi->s_rs, sizeof(struct reiserfs_super_block));
	brelse(bp);

	if (is_reiserfs_jr(rs)) {
		if (sb_version(rs) == REISERFS_VERSION_2)
			reiserfs_log(LOG_INFO, "found reiserfs format \"3.6\""
			    " with non-standard journal");
		else if (sb_version(rs) == REISERFS_VERSION_1)
			reiserfs_log(LOG_INFO, "found reiserfs format \"3.5\""
			    " with non-standard journal");
		else {
			reiserfs_log(LOG_ERR, "found unknown "
			    "format \"%u\" of reiserfs with non-standard magic",
			    sb_version(rs));
			return (EINVAL);
		}
	} else {
		/*
		 * s_version of standard format may contain incorrect
		 * information, so we just look at the magic string
		 */
		reiserfs_log(LOG_INFO,
		    "found reiserfs format \"%s\" with standard journal\n",
		    is_reiserfs_3_5(rs) ? "3.5" : "3.6");
	}

	return (0);
}

/*
 * load_bitmap_info_data - Sets up the reiserfs_bitmap_info structure
 * from disk.
 * @sbi - superblock info for this filesystem
 * @bi  - the bitmap info to be loaded. Requires that bi->bp is valid.
 *
 * This routine counts how many free bits there are, finding the first
 * zero as a side effect. Could also be implemented as a loop of
 * test_bit() calls, or a loop of find_first_zero_bit() calls. This
 * implementation is similar to find_first_zero_bit(), but doesn't
 * return after it finds the first bit. Should only be called on fs
 * mount, but should be fairly efficient anyways.
 *
 * bi->first_zero_hint is considered unset if it == 0, since the bitmap
 * itself will invariably occupt block 0 represented in the bitmap. The
 * only exception to this is when free_count also == 0, since there will
 * be no free blocks at all.
 */
static void
load_bitmap_info_data(struct reiserfs_sb_info *sbi,
    struct reiserfs_bitmap_info *bi)
{
	unsigned long *cur;

	cur = (unsigned long *)bi->bp_data;
	while ((char *)cur < (bi->bp_data + sbi->s_blocksize)) {
		/*
		 * No need to scan if all 0's or all 1's.
		 * Since we're only counting 0's, we can simply ignore
		 * all 1's
		 */
		if (*cur == 0) {
			if (bi->first_zero_hint == 0) {
				bi->first_zero_hint =
				    ((char *)cur - bi->bp_data) << 3;
			}
			bi->free_count += sizeof(unsigned long) * 8;
		} else if (*cur != ~0L) {
			int b;

			for (b = 0; b < sizeof(unsigned long) * 8; b++) {
				if (!reiserfs_test_le_bit(b, cur)) {
					bi->free_count++;
					if (bi->first_zero_hint == 0)
						bi->first_zero_hint =
						    (((char *)cur -
						      bi->bp_data) << 3) + b;
				}
			}
		}
		cur++;
	}
}

/*
 * Read the bitmaps
 */
static int
read_bitmaps(struct reiserfs_mount *rmp)
{
	int i, bmap_nr;
	struct buf *bp = NULL;
	struct reiserfs_sb_info *sbi = rmp->rm_reiserfs;

	/* Allocate memory for the table of bitmaps */
	SB_AP_BITMAP(sbi) =
	    malloc(sizeof(struct reiserfs_bitmap_info) * SB_BMAP_NR(sbi),
		M_REISERFSMNT, M_WAITOK | M_ZERO);
	if (!SB_AP_BITMAP(sbi))
		return (ENOMEM);

	/* Read all the bitmaps */
	for (i = 0,
	    bmap_nr = (REISERFS_DISK_OFFSET_IN_BYTES / sbi->s_blocksize + 1) *
	    btodb(sbi->s_blocksize);
	    i < SB_BMAP_NR(sbi); i++, bmap_nr = sbi->s_blocksize * 8 * i) {
		SB_AP_BITMAP(sbi)[i].bp_data = malloc(sbi->s_blocksize,
		    M_REISERFSMNT, M_WAITOK | M_ZERO);
		if (!SB_AP_BITMAP(sbi)[i].bp_data)
			return (ENOMEM);
		bread(rmp->rm_devvp, bmap_nr, sbi->s_blocksize, NOCRED, &bp);
		bcopy(bp->b_data, SB_AP_BITMAP(sbi)[i].bp_data,
		    sbi->s_blocksize);
		brelse(bp);
		bp = NULL;

		/*if (!buffer_uptodate(SB_AP_BITMAP(s)[i].bh))
			ll_rw_block(READ, 1, &SB_AP_BITMAP(s)[i].bh);*/
	}

	for (i = 0; i < SB_BMAP_NR(sbi); i++) {
		/*if (!buffer_uptodate(SB_AP_BITMAP(s)[i].bh)) {
		  reiserfs_warning(s,"sh-2029: reiserfs read_bitmaps: "
		  "bitmap block (#%lu) reading failed",
		  SB_AP_BITMAP(s)[i].bh->b_blocknr);
		  for (i = 0; i < SB_BMAP_NR(s); i++)
		  brelse(SB_AP_BITMAP(s)[i].bh);
		  vfree(SB_AP_BITMAP(s));
		  SB_AP_BITMAP(s) = NULL;
		  return 1;
		  }*/
		load_bitmap_info_data(sbi, SB_AP_BITMAP(sbi) + i);
		reiserfs_log(LOG_DEBUG,
		    "%d free blocks (starting at block %ld)\n",
		    SB_AP_BITMAP(sbi)[i].free_count,
		    (long)SB_AP_BITMAP(sbi)[i].first_zero_hint);
	}

	return (0);
}

// TODO Not supported
static int
read_old_bitmaps(struct reiserfs_mount *rmp)
{

	return (EOPNOTSUPP);
#if 0
	int i;
	struct reiserfs_sb_info *sbi = rmp->rm_reiserfs;
	struct reiserfs_super_block *rs = SB_DISK_SUPER_BLOCK(sbi);

	/* First of bitmap blocks */
	int bmp1 = (REISERFS_OLD_DISK_OFFSET / sbi->s_blocksize) *
	    btodb(sbi->s_blocksize);

	/* Read true bitmap */
	SB_AP_BITMAP(sbi) =
	    malloc(sizeof (struct reiserfs_buffer_info *) * sb_bmap_nr(rs),
		M_REISERFSMNT, M_WAITOK | M_ZERO);
	if (!SB_AP_BITMAP(sbi))
		return 1;

	for (i = 0; i < sb_bmap_nr(rs); i ++) {
		SB_AP_BITMAP(sbi)[i].bp = getblk(rmp->rm_devvp,
		    (bmp1 + i) * btodb(sbi->s_blocksize), sbi->s_blocksize, 0, 0, 0);
		if (!SB_AP_BITMAP(sbi)[i].bp)
			return 1;
		load_bitmap_info_data(sbi, SB_AP_BITMAP(sbi) + i);
	}

	return 0;
#endif
}

/* -------------------------------------------------------------------
 * Hash detection stuff
 * -------------------------------------------------------------------*/

static int
get_root_node(struct reiserfs_mount *rmp, struct reiserfs_node **root)
{
	struct reiserfs_node *ip;
	struct reiserfs_iget_args args;

	/* Allocate the node structure */
	reiserfs_log(LOG_DEBUG, "malloc(struct reiserfs_node)\n");
	MALLOC(ip, struct reiserfs_node *, sizeof(struct reiserfs_node),
	    M_REISERFSNODE, M_WAITOK | M_ZERO);

	/* Fill the structure */
	reiserfs_log(LOG_DEBUG, "filling *ip\n");
	ip->i_dev      = rmp->rm_dev;
	ip->i_number   = REISERFS_ROOT_OBJECTID;
	ip->i_ino      = REISERFS_ROOT_PARENT_OBJECTID;
	ip->i_reiserfs = rmp->rm_reiserfs;

	/* Read the inode */
	args.objectid = ip->i_number;
	args.dirid    = ip->i_ino;
	reiserfs_log(LOG_DEBUG, "call reiserfs_read_locked_inode("
	    "objectid=%d,dirid=%d)\n", args.objectid, args.dirid);
	reiserfs_read_locked_inode(ip, &args);

	ip->i_devvp = rmp->rm_devvp;
	//XXX VREF(ip->i_devvp); Is it necessary ?

	*root = ip;
	return (0);
}

/*
 * If root directory is empty - we set default - Yura's - hash and warn
 * about it.
 * FIXME: we look for only one name in a directory. If tea and yura both
 * have the same value - we ask user to send report to the mailing list
 */
uint32_t find_hash_out(struct reiserfs_mount *rmp)
{
	int retval;
	struct cpu_key key;
	INITIALIZE_PATH(path);
	struct reiserfs_node *ip;
	struct reiserfs_sb_info *sbi;
	struct reiserfs_dir_entry de;
	uint32_t hash = DEFAULT_HASH;

	get_root_node(rmp, &ip);
	if (!ip)
		return (UNSET_HASH);

	sbi = rmp->rm_reiserfs;

	do {
		uint32_t teahash, r5hash, yurahash;

		reiserfs_log(LOG_DEBUG, "make_cpu_key\n");
		make_cpu_key(&key, ip, ~0, TYPE_DIRENTRY, 3);
		reiserfs_log(LOG_DEBUG, "search_by_entry_key for "
		    "key(objectid=%d,dirid=%d)\n",
		    key.on_disk_key.k_objectid, key.on_disk_key.k_dir_id);
		retval = search_by_entry_key(sbi, &key, &path, &de);
		if (retval == IO_ERROR) {
			pathrelse(&path);
			return (UNSET_HASH);
		}
		if (retval == NAME_NOT_FOUND)
			de.de_entry_num--;

		reiserfs_log(LOG_DEBUG, "name found\n");

		set_de_name_and_namelen(&de);

		if (deh_offset(&(de.de_deh[de.de_entry_num])) == DOT_DOT_OFFSET) {
			/* Allow override in this case */
			if (reiserfs_rupasov_hash(sbi)) {
				hash = YURA_HASH;
			}
			reiserfs_log(LOG_DEBUG,
			    "FS seems to be empty, autodetect "
			    "is using the default hash");
			break;
		}

		r5hash   = GET_HASH_VALUE(r5_hash(de.de_name, de.de_namelen));
		teahash  = GET_HASH_VALUE(keyed_hash(de.de_name,
		    de.de_namelen));
		yurahash = GET_HASH_VALUE(yura_hash(de.de_name, de.de_namelen));
		if (((teahash == r5hash) &&
		    (GET_HASH_VALUE(
		     deh_offset(&(de.de_deh[de.de_entry_num]))) == r5hash)) ||
		    ((teahash == yurahash) &&
		     (yurahash ==
		      GET_HASH_VALUE(
		      deh_offset(&(de.de_deh[de.de_entry_num]))))) ||
		    ((r5hash == yurahash) &&
		     (yurahash ==
		      GET_HASH_VALUE(
		      deh_offset(&(de.de_deh[de.de_entry_num])))))) {
			reiserfs_log(LOG_ERR,
			    "unable to automatically detect hash "
			    "function. Please mount with -o "
			    "hash={tea,rupasov,r5}");
			hash = UNSET_HASH;
			break;
		}

		if (GET_HASH_VALUE(
		    deh_offset(&(de.de_deh[de.de_entry_num]))) == yurahash) {
			reiserfs_log(LOG_DEBUG, "detected YURA hash\n");
			hash = YURA_HASH;
		} else if (GET_HASH_VALUE(
		    deh_offset(&(de.de_deh[de.de_entry_num]))) == teahash) {
			reiserfs_log(LOG_DEBUG, "detected TEA hash\n");
			hash = TEA_HASH;
		} else if (GET_HASH_VALUE(
		    deh_offset(&(de.de_deh[de.de_entry_num]))) == r5hash) {
			reiserfs_log(LOG_DEBUG, "detected R5 hash\n");
			hash = R5_HASH;
		} else {
			reiserfs_log(LOG_WARNING, "unrecognised hash function");
			hash = UNSET_HASH;
		}
	} while (0);

	pathrelse(&path);
	return (hash);
}

/* Finds out which hash names are sorted with */
static int
what_hash(struct reiserfs_mount *rmp)
{
	uint32_t code;
	struct reiserfs_sb_info *sbi = rmp->rm_reiserfs;

	find_hash_out(rmp);
	code = sb_hash_function_code(SB_DISK_SUPER_BLOCK(sbi));

	/*
	 * reiserfs_hash_detect() == true if any of the hash mount options
	 * were used. We must check them to make sure the user isn't using a
	 * bad hash value
	 */
	if (code == UNSET_HASH || reiserfs_hash_detect(sbi))
		code = find_hash_out(rmp);

	if (code != UNSET_HASH && reiserfs_hash_detect(sbi)) {
		/*
		 * Detection has found the hash, and we must check against
		 * the mount options
		 */
		if (reiserfs_rupasov_hash(sbi) && code != YURA_HASH) {
			reiserfs_log(LOG_ERR, "error, %s hash detected, "
			    "unable to force rupasov hash",
			    reiserfs_hashname(code));
			code = UNSET_HASH;
		} else if (reiserfs_tea_hash(sbi) && code != TEA_HASH) {
			reiserfs_log(LOG_ERR, "error, %s hash detected, "
			    "unable to force tea hash",
			    reiserfs_hashname(code));
			code = UNSET_HASH;
		} else if (reiserfs_r5_hash(sbi) && code != R5_HASH) {
			reiserfs_log(LOG_ERR, "error, %s hash detected, "
			    "unable to force r5 hash",
			    reiserfs_hashname(code));
			code = UNSET_HASH;
		}
	} else {
		/*
		 * Find_hash_out was not called or could not determine
		 * the hash
		 */
		if (reiserfs_rupasov_hash(sbi)) {
			code = YURA_HASH;
		} else if (reiserfs_tea_hash(sbi)) {
			code = TEA_HASH;
		} else if (reiserfs_r5_hash(sbi)) {
			code = R5_HASH;
		}
	}

	/* TODO Not supported yet */
#if 0
	/* If we are mounted RW, and we have a new valid hash code, update
	 * the super */
	if (code != UNSET_HASH &&
	    !(s->s_flags & MS_RDONLY) &&
	    code != sb_hash_function_code(SB_DISK_SUPER_BLOCK(s))) {
		set_sb_hash_function_code(SB_DISK_SUPER_BLOCK(s), code);
	}
#endif

	return (code);
}

/* Return pointer to appropriate function */
static hashf_t
hash_function(struct reiserfs_mount *rmp)
{

	switch (what_hash(rmp)) {
	case TEA_HASH:
		reiserfs_log(LOG_INFO, "using tea hash to sort names\n");
		return (keyed_hash);
	case YURA_HASH:
		reiserfs_log(LOG_INFO, "using rupasov hash to sort names\n");
		return (yura_hash);
	case R5_HASH:
		reiserfs_log(LOG_INFO, "using r5 hash to sort names\n");
		return (r5_hash);
	}

	return (NULL);
}

/* -------------------------------------------------------------------
 * VFS registration
 * -------------------------------------------------------------------*/

static struct vfsops reiser_vfsops = {
	.vfs_cmount	= reiserfs_cmount,
	.vfs_mount	= reiserfs_mount,
	.vfs_unmount	= reiserfs_unmount,
	//.vfs_checkexp	= reiserfs_checkexp,
	//.vfs_extattrctl = reiserfs_extattrctl,
	.vfs_fhtovp	= reiserfs_fhtovp,
	//.vfs_quotactl	= reiserfs_quotactl,
	.vfs_root	= reiserfs_root,
	//.vfs_start	= reiserfs_start,
	.vfs_statfs	= reiserfs_statfs,
	//.vfs_sync	= reiserfs_sync,
	//.vfs_vget	= reiserfs_vget,
};

VFS_SET(reiser_vfsops, reiserfs, VFCF_READONLY);
