/*
 *  modified for EXT2FS support in Lites 1.1
 *
 *  Aug 1995, Godmar Back (gback@cs.utah.edu)
 *  University of Utah, Department of Computer Science
 */
/*	
 * Copyright (c) 1989, 1991, 1993, 1994	
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)ffs_vfsops.c	8.8 (Berkeley) 4/18/94
 */

#include "opt_quota.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/disklabel.h>
#include <sys/malloc.h>
#include <sys/stat.h>

#include <miscfs/specfs/specdev.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufs_extern.h>

#include <gnu/ext2fs/fs.h>
#include <gnu/ext2fs/ext2_extern.h>
#include <gnu/ext2fs/ext2_fs.h>
#include <gnu/ext2fs/ext2_fs_sb.h>

static int ext2_fhtovp __P((struct mount *, struct fid *, struct sockaddr *,
	    struct vnode **, int *, struct ucred **));
static int ext2_flushfiles __P((struct mount *mp, int flags, struct proc *p));
static int ext2_mount __P((struct mount *,
	    char *, caddr_t, struct nameidata *, struct proc *));
static int ext2_mountfs __P((struct vnode *, struct mount *, struct proc *));
static int ext2_reload __P((struct mount *mountp, struct ucred *cred,
			struct proc *p));
static int ext2_sbupdate __P((struct ufsmount *, int));
static int ext2_statfs __P((struct mount *, struct statfs *, struct proc *));
static int ext2_sync __P((struct mount *, int, struct ucred *, struct proc *));
static int ext2_unmount __P((struct mount *, int, struct proc *));
static int ext2_vget __P((struct mount *, ino_t, struct vnode **));
static int ext2_vptofh __P((struct vnode *, struct fid *));

static MALLOC_DEFINE(M_EXT2NODE, "EXT2 node", "EXT2 vnode private part");

static struct vfsops ext2fs_vfsops = {
	ext2_mount,
	ufs_start,		/* empty function */
	ext2_unmount,
	ufs_root,		/* root inode via vget */
	ufs_quotactl,		/* does operations associated with quotas */
	ext2_statfs,
	ext2_sync,
	ext2_vget,
	ext2_fhtovp,
	ext2_vptofh,
	ext2_init,
};

VFS_SET(ext2fs_vfsops, ext2fs, MOUNT_EXT2FS, 0);
#define bsd_malloc malloc
#define bsd_free free

static int ext2fs_inode_hash_lock;

static int	compute_sb_data __P((struct vnode * devvp,
				     struct ext2_super_block * es,
				     struct ext2_sb_info * fs));

#ifdef notyet
static int ext2_mountroot __P((void));

/*
 * Called by main() when ext2fs is going to be mounted as root.
 *
 * Name is updated by mount(8) after booting.
 */
#define ROOTNAME	"root_device"

static int
ext2_mountroot()
{
	register struct ext2_sb_info *fs;
	register struct mount *mp;
	struct proc *p = curproc;
	struct ufsmount *ump;
	u_int size;
	int error;
	
	if ((error = bdevvp(rootdev, &rootvp))) {
		printf("ext2_mountroot: can't find rootvp");
		return (error);
	}
	mp = bsd_malloc((u_long)sizeof(struct mount), M_MOUNT, M_WAITOK);
	bzero((char *)mp, (u_long)sizeof(struct mount));
	mp->mnt_op = &ext2fs_vfsops;
	mp->mnt_flag = MNT_RDONLY;
	if (bdevsw[major(rootdev)]->d_flags & D_NOCLUSTERR)
		mp->mnt_flag |= MNT_NOCLUSTERR;
	if (bdevsw[major(rootdev)]->d_flags & D_NOCLUSTERW)
		mp->mnt_flag |= MNT_NOCLUSTERW;
	if (error = ext2_mountfs(rootvp, mp, p)) {
		bsd_free(mp, M_MOUNT);
		return (error);
	}
	if (error = vfs_lock(mp)) {
		(void)ext2_unmount(mp, 0, p);
		bsd_free(mp, M_MOUNT);
		return (error);
	}
	CIRCLEQ_INSERT_HEAD(&mountlist, mp, mnt_list);
	mp->mnt_flag |= MNT_ROOTFS;
	mp->mnt_vnodecovered = NULLVP;
	ump = VFSTOUFS(mp);
	fs = ump->um_e2fs;
	bzero(fs->fs_fsmnt, sizeof(fs->fs_fsmnt));
	fs->fs_fsmnt[0] = '/';
	bcopy((caddr_t)fs->fs_fsmnt, (caddr_t)mp->mnt_stat.f_mntonname,
	    MNAMELEN);
	(void) copystr(ROOTNAME, mp->mnt_stat.f_mntfromname, MNAMELEN - 1,
	    &size);
	bzero(mp->mnt_stat.f_mntfromname + size, MNAMELEN - size);
	(void)ext2_statfs(mp, &mp->mnt_stat, p);
	vfs_unlock(mp);
	inittodr(fs->s_es->s_wtime);		/* this helps to set the time */
	return (0);
}
#endif

/*
 * VFS Operations.
 *
 * mount system call
 */
static int
ext2_mount(mp, path, data, ndp, p)
	register struct mount *mp;	
	char *path;
	caddr_t data;		/* this is actually a (struct ufs_args *) */
	struct nameidata *ndp;
	struct proc *p;
{
	struct vnode *devvp;
	struct ufs_args args;
	struct ufsmount *ump = 0;
	register struct ext2_sb_info *fs;
	u_int size;
	int error, flags;

	if (error = copyin(data, (caddr_t)&args, sizeof (struct ufs_args)))
		return (error);
	/*
	 * If updating, check whether changing from read-only to
	 * read/write; if there is no device name, that's all we do.
	 * Disallow clearing MNT_NOCLUSTERR and MNT_NOCLUSTERW flags,
	 * if block device requests.
	 */
	if (mp->mnt_flag & MNT_UPDATE) {
		ump = VFSTOUFS(mp);
		fs = ump->um_e2fs;
		error = 0;
		if (bdevsw[major(ump->um_dev)]->d_flags & D_NOCLUSTERR)
			mp->mnt_flag |= MNT_NOCLUSTERR;
		if (bdevsw[major(ump->um_dev)]->d_flags & D_NOCLUSTERW)
			mp->mnt_flag |= MNT_NOCLUSTERW;
		if (fs->s_rd_only == 0 && (mp->mnt_flag & MNT_RDONLY)) {
			flags = WRITECLOSE;
			if (mp->mnt_flag & MNT_FORCE)
				flags |= FORCECLOSE;
			if (vfs_busy(mp, LK_NOWAIT, 0, p))
				return (EBUSY);
			error = ext2_flushfiles(mp, flags, p);
			vfs_unbusy(mp, p);
		}
		if (!error && (mp->mnt_flag & MNT_RELOAD))
			error = ext2_reload(mp, ndp->ni_cnd.cn_cred, p);
		if (error)
			return (error);
		if (fs->s_rd_only && (mp->mnt_kern_flag & MNTK_WANTRDWR))
			fs->s_rd_only = 0;
		if (fs->s_rd_only == 0) {
			/* don't say it's clean */
			fs->s_es->s_state &= ~EXT2_VALID_FS;
			ext2_sbupdate(ump, MNT_WAIT);
		}
		if (args.fspec == 0) {
			/*
			 * Process export requests.
			 */
			return (vfs_export(mp, &ump->um_export, &args.export));
		}
	}
	/*
	 * Not an update, or updating the name: look up the name
	 * and verify that it refers to a sensible block device.
	 */
	NDINIT(ndp, LOOKUP, FOLLOW, UIO_USERSPACE, args.fspec, p);
	if (error = namei(ndp))
		return (error);
	devvp = ndp->ni_vp;

	if (devvp->v_type != VBLK) {
		vrele(devvp);
		return (ENOTBLK);
	}
	if (major(devvp->v_rdev) >= nblkdev) {
		vrele(devvp);
		return (ENXIO);
	}
	if ((mp->mnt_flag & MNT_UPDATE) == 0) {
		if (bdevsw[major(devvp->v_rdev)]->d_flags & D_NOCLUSTERR)
			mp->mnt_flag |= MNT_NOCLUSTERR;
		if (bdevsw[major(devvp->v_rdev)]->d_flags & D_NOCLUSTERW)
			mp->mnt_flag |= MNT_NOCLUSTERW;
		error = ext2_mountfs(devvp, mp, p);
	} else {
		if (devvp != ump->um_devvp)
			error = EINVAL;	/* needs translation */
		else
			vrele(devvp);
	}
	if (error) {
		vrele(devvp);
		return (error);
	}
	ump = VFSTOUFS(mp);
	fs = ump->um_e2fs;
	(void) copyinstr(path, fs->fs_fsmnt, sizeof(fs->fs_fsmnt) - 1, &size);
	bzero(fs->fs_fsmnt + size, sizeof(fs->fs_fsmnt) - size);
	bcopy((caddr_t)fs->fs_fsmnt, (caddr_t)mp->mnt_stat.f_mntonname,
	    MNAMELEN);
	(void) copyinstr(args.fspec, mp->mnt_stat.f_mntfromname, MNAMELEN - 1, 
	    &size);
	bzero(mp->mnt_stat.f_mntfromname + size, MNAMELEN - size);
	(void)ext2_statfs(mp, &mp->mnt_stat, p);
	return (0);
}

/*
 * checks that the data in the descriptor blocks make sense
 * this is taken from ext2/super.c
 */
static int ext2_check_descriptors (struct ext2_sb_info * sb)
{
        int i;
        int desc_block = 0;
        unsigned long block = sb->s_es->s_first_data_block;
        struct ext2_group_desc * gdp = NULL;

        /* ext2_debug ("Checking group descriptors"); */

        for (i = 0; i < sb->s_groups_count; i++)
        {
		/* examine next descriptor block */
                if ((i % EXT2_DESC_PER_BLOCK(sb)) == 0)
                        gdp = (struct ext2_group_desc *) 
				sb->s_group_desc[desc_block++]->b_data;
                if (gdp->bg_block_bitmap < block ||
                    gdp->bg_block_bitmap >= block + EXT2_BLOCKS_PER_GROUP(sb))
                {
                        printf ("ext2_check_descriptors: "
                                    "Block bitmap for group %d"
                                    " not in group (block %lu)!",
                                    i, (unsigned long) gdp->bg_block_bitmap);
                        return 0;
                }
                if (gdp->bg_inode_bitmap < block ||
                    gdp->bg_inode_bitmap >= block + EXT2_BLOCKS_PER_GROUP(sb))
                {
                        printf ("ext2_check_descriptors: "
                                    "Inode bitmap for group %d"
                                    " not in group (block %lu)!",
                                    i, (unsigned long) gdp->bg_inode_bitmap);
                        return 0;
                }
                if (gdp->bg_inode_table < block ||
                    gdp->bg_inode_table + sb->s_itb_per_group >=
                    block + EXT2_BLOCKS_PER_GROUP(sb))
                {
                        printf ("ext2_check_descriptors: "
                                    "Inode table for group %d"
                                    " not in group (block %lu)!",
                                    i, (unsigned long) gdp->bg_inode_table);
                        return 0;
                }
                block += EXT2_BLOCKS_PER_GROUP(sb);
                gdp++;
        }
        return 1;
}

/*
 * this computes the fields of the  ext2_sb_info structure from the
 * data in the ext2_super_block structure read in
 */
static int compute_sb_data(devvp, es, fs)
	struct vnode * devvp;
	struct ext2_super_block * es;
	struct ext2_sb_info * fs;
{
    int db_count, error;
    int i, j;
    int logic_sb_block = 1;	/* XXX for now */

#if 1
#define V(v)  
#else
#define V(v)  printf(#v"= %d\n", fs->v);
#endif

    fs->s_blocksize = EXT2_MIN_BLOCK_SIZE << es->s_log_block_size; 
    V(s_blocksize)
    fs->s_bshift = EXT2_MIN_BLOCK_LOG_SIZE + es->s_log_block_size;
    V(s_bshift)
    fs->s_fsbtodb = es->s_log_block_size + 1;
    V(s_fsbtodb)
    fs->s_qbmask = fs->s_blocksize - 1;
    V(s_bmask)
    fs->s_blocksize_bits = EXT2_BLOCK_SIZE_BITS(es);
    V(s_blocksize_bits)
    fs->s_frag_size = EXT2_MIN_FRAG_SIZE << es->s_log_frag_size;
    V(s_frag_size)
    if (fs->s_frag_size)
	fs->s_frags_per_block = fs->s_blocksize / fs->s_frag_size;
    V(s_frags_per_block)
    fs->s_blocks_per_group = es->s_blocks_per_group;
    V(s_blocks_per_group)
    fs->s_frags_per_group = es->s_frags_per_group;
    V(s_frags_per_group)
    fs->s_inodes_per_group = es->s_inodes_per_group;
    V(s_inodes_per_group)
    fs->s_inodes_per_block = fs->s_blocksize / EXT2_INODE_SIZE;
    V(s_inodes_per_block)
    fs->s_itb_per_group = fs->s_inodes_per_group /fs->s_inodes_per_block;
    V(s_itb_per_group)
    fs->s_desc_per_block = fs->s_blocksize / sizeof (struct ext2_group_desc);
    V(s_desc_per_block)
    /* s_resuid / s_resgid ? */
    fs->s_groups_count = (es->s_blocks_count -
			  es->s_first_data_block +
			  EXT2_BLOCKS_PER_GROUP(fs) - 1) /
			 EXT2_BLOCKS_PER_GROUP(fs);
    V(s_groups_count)
    db_count = (fs->s_groups_count + EXT2_DESC_PER_BLOCK(fs) - 1) /
	EXT2_DESC_PER_BLOCK(fs);
    fs->s_db_per_group = db_count;
    V(s_db_per_group)

    fs->s_group_desc = bsd_malloc(db_count * sizeof (struct buf *),
		M_UFSMNT, M_WAITOK);

    /* adjust logic_sb_block */
    if(fs->s_blocksize > SBSIZE) 
	/* Godmar thinks: if the blocksize is greater than 1024, then
	   the superblock is logically part of block zero. 
	 */
        logic_sb_block = 0;
    
    for (i = 0; i < db_count; i++) {
	error = bread(devvp , fsbtodb(fs, logic_sb_block + i + 1), 
		fs->s_blocksize, NOCRED, &fs->s_group_desc[i]);
	if(error) {
	    for (j = 0; j < i; j++)
		brelse(fs->s_group_desc[j]);
	    bsd_free(fs->s_group_desc, M_UFSMNT);
	    printf("EXT2-fs: unable to read group descriptors (%d)\n", error);
	    return EIO;
	}
	/* Set the B_LOCKED flag on the buffer, then brelse() it */
	LCK_BUF(fs->s_group_desc[i])
    }
    if(!ext2_check_descriptors(fs)) {
	    for (j = 0; j < db_count; j++)
		    ULCK_BUF(fs->s_group_desc[j])
	    bsd_free(fs->s_group_desc, M_UFSMNT);
	    printf("EXT2-fs: (ext2_check_descriptors failure) "
		   "unable to read group descriptors\n");
	    return EIO;
    }

    for (i = 0; i < EXT2_MAX_GROUP_LOADED; i++) {
	    fs->s_inode_bitmap_number[i] = 0;
	    fs->s_inode_bitmap[i] = NULL;
	    fs->s_block_bitmap_number[i] = 0;
	    fs->s_block_bitmap[i] = NULL;
    }
    fs->s_loaded_inode_bitmaps = 0;
    fs->s_loaded_block_bitmaps = 0;
    return 0;
}

/*
 * Reload all incore data for a filesystem (used after running fsck on
 * the root filesystem and finding things to fix). The filesystem must
 * be mounted read-only.
 *
 * Things to do to update the mount:
 *	1) invalidate all cached meta-data.
 *	2) re-read superblock from disk.
 *	3) re-read summary information from disk.
 *	4) invalidate all inactive vnodes.
 *	5) invalidate all cached file data.
 *	6) re-read inode data for all active vnodes.
 */
static int
ext2_reload(mountp, cred, p)
	register struct mount *mountp;
	struct ucred *cred;
	struct proc *p;
{
	register struct vnode *vp, *nvp, *devvp;
	struct inode *ip;
	struct buf *bp;
	struct ext2_super_block * es;
	struct ext2_sb_info *fs;
	int error;

	if ((mountp->mnt_flag & MNT_RDONLY) == 0)
		return (EINVAL);
	/*
	 * Step 1: invalidate all cached meta-data.
	 */
	devvp = VFSTOUFS(mountp)->um_devvp;
	if (vinvalbuf(devvp, 0, cred, p, 0, 0))
		panic("ext2_reload: dirty1");
	/*
	 * Step 2: re-read superblock from disk.
	 * constants have been adjusted for ext2
	 */
	if (error = bread(devvp, SBLOCK, SBSIZE, NOCRED, &bp))
		return (error);
	es = (struct ext2_super_block *)bp->b_data;
	if (es->s_magic != EXT2_SUPER_MAGIC) {
		if(es->s_magic == EXT2_PRE_02B_MAGIC)
		    printf("This filesystem bears the magic number of a pre "
			   "0.2b version of ext2. This is not supported by "
			   "Lites.\n");
		else
		    printf("Wrong magic number: %x (expected %x for ext2 fs\n",
			es->s_magic, EXT2_SUPER_MAGIC);
		brelse(bp);
		return (EIO);		/* XXX needs translation */
	}
	fs = VFSTOUFS(mountp)->um_e2fs;
	bcopy(bp->b_data, fs->s_es, sizeof(struct ext2_super_block));

	if(error = compute_sb_data(devvp, es, fs)) {
		brelse(bp);
		return error;
	}
#ifdef UNKLAR
	if (fs->fs_sbsize < SBSIZE)
		bp->b_flags |= B_INVAL;
#endif
	brelse(bp);

loop:
	for (vp = mountp->mnt_vnodelist.lh_first; vp != NULL; vp = nvp) {
		nvp = vp->v_mntvnodes.le_next;
		/*
		 * Step 4: invalidate all inactive vnodes.
		 */
		if (vp->v_usecount == 0) {
			vgone(vp);
			continue;
		}
		/*
		 * Step 5: invalidate all cached file data.
		 */
		if (vget(vp, LK_EXCLUSIVE, p))
			goto loop;
		if (vinvalbuf(vp, 0, cred, p, 0, 0))
			panic("ext2_reload: dirty2");
		/*
		 * Step 6: re-read inode data for all active vnodes.
		 */
		ip = VTOI(vp);
		if (error =
		    bread(devvp, fsbtodb(fs, ino_to_fsba(fs, ip->i_number)),
		    (int)fs->s_blocksize, NOCRED, &bp)) {
			vput(vp);
			return (error);
		}
		ext2_ei2di((struct ext2_inode *) ((char *)bp->b_data + 
			EXT2_INODE_SIZE * ino_to_fsbo(fs, ip->i_number)), 
			&ip->i_din);
		brelse(bp);
		vput(vp);
		if (vp->v_mount != mountp)
			goto loop;
	}
	return (0);
}

/*
 * Common code for mount and mountroot
 */
static int
ext2_mountfs(devvp, mp, p)
	register struct vnode *devvp;
	struct mount *mp;
	struct proc *p;
{
	register struct ufsmount *ump;
	struct buf *bp;
	register struct ext2_sb_info *fs;
	struct ext2_super_block * es;
	dev_t dev = devvp->v_rdev;
	struct partinfo dpart;
	int havepart = 0;
	int error, i, size;
	int ronly;

	/*
	 * Disallow multiple mounts of the same device.
	 * Disallow mounting of a device that is currently in use
	 * (except for root, which might share swap device for miniroot).
	 * Flush out any old buffers remaining from a previous use.
	 */
	if (error = vfs_mountedon(devvp))
		return (error);
	if (vcount(devvp) > 1 && devvp != rootvp)
		return (EBUSY);
	if (error = vinvalbuf(devvp, V_SAVE, p->p_ucred, p, 0, 0))
		return (error);
#ifdef READONLY
/* turn on this to force it to be read-only */
	mp->mnt_flag |= MNT_RDONLY;
#endif

	ronly = (mp->mnt_flag & MNT_RDONLY) != 0;
	if (error = VOP_OPEN(devvp, ronly ? FREAD : FREAD|FWRITE, FSCRED, p))
		return (error);
	if (VOP_IOCTL(devvp, DIOCGPART, (caddr_t)&dpart, FREAD, NOCRED, p) != 0)
		size = DEV_BSIZE;
	else {
		havepart = 1;
		size = dpart.disklab->d_secsize;
	}

	bp = NULL;
	ump = NULL;
	if (error = bread(devvp, SBLOCK, SBSIZE, NOCRED, &bp))
		goto out;
	es = (struct ext2_super_block *)bp->b_data;
	if (es->s_magic != EXT2_SUPER_MAGIC) {
		if(es->s_magic == EXT2_PRE_02B_MAGIC)
		    printf("This filesystem bears the magic number of a pre "
			   "0.2b version of ext2. This is not supported by "
			   "Lites.\n");
		else
		    printf("Wrong magic number: %x (expected %x for EXT2FS)\n",
			es->s_magic, EXT2_SUPER_MAGIC);
		error = EINVAL;		/* XXX needs translation */
		goto out;
	}
	ump = bsd_malloc(sizeof *ump, M_UFSMNT, M_WAITOK);
	bzero((caddr_t)ump, sizeof *ump);
	ump->um_malloctype = M_EXT2NODE;
	ump->um_blkatoff = ext2_blkatoff;
	ump->um_truncate = ext2_truncate;
	ump->um_update = ext2_update;
	ump->um_valloc = ext2_valloc;
	ump->um_vfree = ext2_vfree;
	/* I don't know whether this is the right strategy. Note that
	   we dynamically allocate both a ext2_sb_info and a ext2_super_block
	   while Linux keeps the super block in a locked buffer
	 */
	ump->um_e2fs = bsd_malloc(sizeof(struct ext2_sb_info), 
		M_UFSMNT, M_WAITOK);
	ump->um_e2fs->s_es = bsd_malloc(sizeof(struct ext2_super_block), 
		M_UFSMNT, M_WAITOK);
	bcopy(es, ump->um_e2fs->s_es, (u_int)sizeof(struct ext2_super_block));
	if(error = compute_sb_data(devvp, ump->um_e2fs->s_es, ump->um_e2fs)) {
		brelse(bp);
		return error;
	}
	brelse(bp);
	bp = NULL;
	fs = ump->um_e2fs;
	fs->s_rd_only = ronly;	/* ronly is set according to mnt_flags */
	if (!(fs->s_es->s_state & EXT2_VALID_FS)) {
		printf("WARNING: %s was not properly dismounted\n",
			fs->fs_fsmnt);
	}
	/* if the fs is not mounted read-only, make sure the super block is 
	   always written back on a sync()
	 */
	if (ronly == 0) {
		fs->s_dirt = 1;		/* mark it modified */
		fs->s_es->s_state &= ~EXT2_VALID_FS;	/* set fs invalid */
	}
	mp->mnt_data = (qaddr_t)ump;
	mp->mnt_stat.f_fsid.val[0] = (long)dev;
	mp->mnt_stat.f_fsid.val[1] = MOUNT_EXT2FS;
	mp->mnt_maxsymlinklen = EXT2_MAXSYMLINKLEN;
	mp->mnt_flag |= MNT_LOCAL;
	ump->um_mountp = mp;
	ump->um_dev = dev;
	ump->um_devvp = devvp;
	/* setting those two parameters allows us to use 
	   ufs_bmap w/o changse !
	*/
	ump->um_nindir = EXT2_ADDR_PER_BLOCK(fs);
	ump->um_bptrtodb = fs->s_es->s_log_block_size + 1;
	ump->um_seqinc = EXT2_FRAGS_PER_BLOCK(fs);
	for (i = 0; i < MAXQUOTAS; i++)
		ump->um_quotas[i] = NULLVP; 
	devvp->v_specmountpoint = mp;
	if (ronly == 0) 
		ext2_sbupdate(ump, MNT_WAIT);
	return (0);
out:
	if (bp)
		brelse(bp);
	(void)VOP_CLOSE(devvp, ronly ? FREAD : FREAD|FWRITE, NOCRED, p);
	if (ump) {
		bsd_free(ump->um_fs, M_UFSMNT);
		bsd_free(ump, M_UFSMNT);
		mp->mnt_data = (qaddr_t)0;
	}
	return (error);
}

/*
 * unmount system call
 */
static int
ext2_unmount(mp, mntflags, p)
	struct mount *mp;
	int mntflags;
	struct proc *p;
{
	register struct ufsmount *ump;
	register struct ext2_sb_info *fs;
	int error, flags, ronly, i;

	flags = 0;
	if (mntflags & MNT_FORCE) {
		if (mp->mnt_flag & MNT_ROOTFS)
			return (EINVAL);
		flags |= FORCECLOSE;
	}
	if (error = ext2_flushfiles(mp, flags, p))
		return (error);
	ump = VFSTOUFS(mp);
	fs = ump->um_e2fs;
	ronly = fs->s_rd_only;
	if (!ronly) {
		fs->s_es->s_state |= EXT2_VALID_FS;	/* was fs_clean = 1 */
		ext2_sbupdate(ump, MNT_WAIT);
	}

	/* release buffers containing group descriptors */
	for(i = 0; i < fs->s_db_per_group; i++) 
		ULCK_BUF(fs->s_group_desc[i])

	/* release cached inode/block bitmaps */
        for (i = 0; i < EXT2_MAX_GROUP_LOADED; i++)
                if (fs->s_inode_bitmap[i])
			ULCK_BUF(fs->s_inode_bitmap[i])

        for (i = 0; i < EXT2_MAX_GROUP_LOADED; i++)
                if (fs->s_block_bitmap[i])
			ULCK_BUF(fs->s_block_bitmap[i])

	ump->um_devvp->v_specmountpoint = NULL;
	error = VOP_CLOSE(ump->um_devvp, ronly ? FREAD : FREAD|FWRITE,
		NOCRED, p);
	vrele(ump->um_devvp);
	bsd_free(fs->s_es, M_UFSMNT);
	bsd_free(fs, M_UFSMNT);
	bsd_free(ump, M_UFSMNT);
	mp->mnt_data = (qaddr_t)0;
	mp->mnt_flag &= ~MNT_LOCAL;
	return (error);
}

/*
 * Flush out all the files in a filesystem.
 */
static int
ext2_flushfiles(mp, flags, p)
	register struct mount *mp;
	int flags;
	struct proc *p;
{
	register struct ufsmount *ump;
	int error;
#if QUOTA
	int i;
#endif

	ump = VFSTOUFS(mp);
#if QUOTA
	if (mp->mnt_flag & MNT_QUOTA) {
		if (error = vflush(mp, NULLVP, SKIPSYSTEM|flags))
			return (error);
		for (i = 0; i < MAXQUOTAS; i++) {
			if (ump->um_quotas[i] == NULLVP)
				continue;
			quotaoff(p, mp, i);
		}
		/*
		 * Here we fall through to vflush again to ensure
		 * that we have gotten rid of all the system vnodes.
		 */
	}
#endif
	error = vflush(mp, NULLVP, flags);
	return (error);
}

/*
 * Get file system statistics.
 * taken from ext2/super.c ext2_statfs
 */
static int
ext2_statfs(mp, sbp, p)
	struct mount *mp;
	register struct statfs *sbp;
	struct proc *p;
{
        unsigned long overhead;
	unsigned long overhead_per_group;

	register struct ufsmount *ump;
	register struct ext2_sb_info *fs;
	register struct ext2_super_block *es;

	ump = VFSTOUFS(mp);
	fs = ump->um_e2fs;
	es = fs->s_es;

	if (es->s_magic != EXT2_SUPER_MAGIC)
		panic("ext2_statfs - magic number spoiled");

	/*
	 * Compute the overhead (FS structures)
	 */
	overhead_per_group = 1 /* super block */ +
			     fs->s_db_per_group +
			     1 /* block bitmap */ +
			     1 /* inode bitmap */ +
			     fs->s_itb_per_group;
	overhead = es->s_first_data_block + 
		   fs->s_groups_count * overhead_per_group;

	sbp->f_type = MOUNT_EXT2FS;
	sbp->f_bsize = EXT2_FRAG_SIZE(fs);	
	sbp->f_iosize = EXT2_BLOCK_SIZE(fs);
	sbp->f_blocks = es->s_blocks_count - overhead;
	sbp->f_bfree = es->s_free_blocks_count; 
	sbp->f_bavail = sbp->f_bfree - es->s_r_blocks_count; 
	sbp->f_files = es->s_inodes_count; 
	sbp->f_ffree = es->s_free_inodes_count; 
	if (sbp != &mp->mnt_stat) {
		bcopy((caddr_t)mp->mnt_stat.f_mntonname,
			(caddr_t)&sbp->f_mntonname[0], MNAMELEN);
		bcopy((caddr_t)mp->mnt_stat.f_mntfromname,
			(caddr_t)&sbp->f_mntfromname[0], MNAMELEN);
	}
	return (0);
}

/*
 * Go through the disk queues to initiate sandbagged IO;
 * go through the inodes to write those that have been modified;
 * initiate the writing of the super block if it has been modified.
 *
 * Note: we are always called with the filesystem marked `MPBUSY'.
 */
static int
ext2_sync(mp, waitfor, cred, p)
	struct mount *mp;
	int waitfor;
	struct ucred *cred;
	struct proc *p;
{
	register struct vnode *vp;
	register struct inode *ip;
	register struct ufsmount *ump = VFSTOUFS(mp);
	register struct ext2_sb_info *fs;
	int error, allerror = 0;

	fs = ump->um_e2fs;
	/*
	 * Write back modified superblock.
	 * Consistency check that the superblock
	 * is still in the buffer cache.
	 */
	if (fs->s_dirt) {
		if (fs->s_rd_only != 0) {		/* XXX */
			printf("fs = %s\n", fs->fs_fsmnt);
			panic("update: rofs mod");
		}
		fs->s_dirt = 0;
		fs->s_es->s_wtime = time_second;
		allerror = ext2_sbupdate(ump, waitfor);
	}
	/*
	 * Write back each (modified) inode.
	 */
loop:
	for (vp = mp->mnt_vnodelist.lh_first;
	     vp != NULL;
	     vp = vp->v_mntvnodes.le_next) {
		/*
		 * If the vnode that we are about to sync is no longer
		 * associated with this mount point, start over.
		 */
		if (vp->v_mount != mp)
			goto loop;
		if (VOP_ISLOCKED(vp))
			continue;
		ip = VTOI(vp);
		if ((ip->i_flag &
		    (IN_ACCESS | IN_CHANGE | IN_MODIFIED | IN_UPDATE)) == 0 &&
		    vp->v_dirtyblkhd.lh_first == NULL)
			continue;
		if (vget(vp, LK_EXCLUSIVE, p))
			goto loop;
		if (error = VOP_FSYNC(vp, cred, waitfor, p))
			allerror = error;
		vput(vp);
	}
	/*
	 * Force stale file system control information to be flushed.
	 */
	if (error = VOP_FSYNC(ump->um_devvp, cred, waitfor, p))
		allerror = error;
#if QUOTA
	qsync(mp);
#endif
	return (allerror);
}

/*
 * Look up a EXT2FS dinode number to find its incore vnode, otherwise read it
 * in from disk.  If it is in core, wait for the lock bit to clear, then
 * return the inode locked.  Detection and handling of mount points must be
 * done by the calling routine.
 */
static int
ext2_vget(mp, ino, vpp)
	struct mount *mp;
	ino_t ino;
	struct vnode **vpp;
{
	register struct ext2_sb_info *fs;
	register struct inode *ip;
	struct ufsmount *ump;
	struct buf *bp;
	struct vnode *vp;
	dev_t dev;
	int i, error;
	int used_blocks;

	ump = VFSTOUFS(mp);
	dev = ump->um_dev;
restart:
	if ((*vpp = ufs_ihashget(dev, ino)) != NULL)
		return (0);

	/*
	 * Lock out the creation of new entries in the FFS hash table in
	 * case getnewvnode() or MALLOC() blocks, otherwise a duplicate
	 * may occur!
	 */
	if (ext2fs_inode_hash_lock) {
		while (ext2fs_inode_hash_lock) {
			ext2fs_inode_hash_lock = -1;
			tsleep(&ext2fs_inode_hash_lock, PVM, "e2vget", 0);
		}
		goto restart;
	}
	ext2fs_inode_hash_lock = 1;

	/*
	 * If this MALLOC() is performed after the getnewvnode()
	 * it might block, leaving a vnode with a NULL v_data to be
	 * found by ext2_sync() if a sync happens to fire right then,
	 * which will cause a panic because ext2_sync() blindly
	 * dereferences vp->v_data (as well it should).
	 */
	MALLOC(ip, struct inode *, sizeof(struct inode), M_EXT2NODE, M_WAITOK);

	/* Allocate a new vnode/inode. */
	if (error = getnewvnode(VT_UFS, mp, ext2_vnodeop_p, &vp)) {
		if (ext2fs_inode_hash_lock < 0)
			wakeup(&ext2fs_inode_hash_lock);
		ext2fs_inode_hash_lock = 0;
		*vpp = NULL;
		FREE(ip, M_EXT2NODE);
		return (error);
	}
	bzero((caddr_t)ip, sizeof(struct inode));
	vp->v_data = ip;
	ip->i_vnode = vp;
	ip->i_e2fs = fs = ump->um_e2fs;
	ip->i_dev = dev;
	ip->i_number = ino;
#if QUOTA
	for (i = 0; i < MAXQUOTAS; i++)
		ip->i_dquot[i] = NODQUOT;
#endif
	/*
	 * Put it onto its hash chain and lock it so that other requests for
	 * this inode will block if they arrive while we are sleeping waiting
	 * for old data structures to be purged or for the contents of the
	 * disk portion of this inode to be read.
	 */
	ufs_ihashins(ip);

	if (ext2fs_inode_hash_lock < 0)
		wakeup(&ext2fs_inode_hash_lock);
	ext2fs_inode_hash_lock = 0;

	/* Read in the disk contents for the inode, copy into the inode. */
#if 0
printf("ext2_vget(%d) dbn= %d ", ino, fsbtodb(fs, ino_to_fsba(fs, ino)));
#endif
	if (error = bread(ump->um_devvp, fsbtodb(fs, ino_to_fsba(fs, ino)),
	    (int)fs->s_blocksize, NOCRED, &bp)) {
		/*
		 * The inode does not contain anything useful, so it would
		 * be misleading to leave it on its hash chain. With mode
		 * still zero, it will be unlinked and returned to the free
		 * list by vput().
		 */
		vput(vp);
		brelse(bp);
		*vpp = NULL;
		return (error);
	}
	/* convert ext2 inode to dinode */
	ext2_ei2di((struct ext2_inode *) ((char *)bp->b_data + EXT2_INODE_SIZE *
			ino_to_fsbo(fs, ino)), &ip->i_din);
	ip->i_block_group = ino_to_cg(fs, ino);
	ip->i_next_alloc_block = 0;
	ip->i_next_alloc_goal = 0;
	ip->i_prealloc_count = 0;
	ip->i_prealloc_block = 0;
        /* now we want to make sure that block pointers for unused
           blocks are zeroed out - ext2_balloc depends on this 
	   although for regular files and directories only
	*/
	if(S_ISDIR(ip->i_mode) || S_ISREG(ip->i_mode)) {
		used_blocks = (ip->i_size+fs->s_blocksize-1) / fs->s_blocksize;
		for(i = used_blocks; i < EXT2_NDIR_BLOCKS; i++)
			ip->i_db[i] = 0;
	}
/*
	ext2_print_inode(ip);
*/
	brelse(bp);

	/*
	 * Initialize the vnode from the inode, check for aliases.
	 * Note that the underlying vnode may have changed.
	 */
	if (error = ufs_vinit(mp, ext2_specop_p, ext2_fifoop_p, &vp)) {
		vput(vp);
		*vpp = NULL;
		return (error);
	}
	/*
	 * Finish inode initialization now that aliasing has been resolved.
	 */
	ip->i_devvp = ump->um_devvp;
	VREF(ip->i_devvp);
	/*
	 * Set up a generation number for this inode if it does not
	 * already have one. This should only happen on old filesystems.
	 */
	if (ip->i_gen == 0) {
		ip->i_gen = random() / 2 + 1;
		if ((vp->v_mount->mnt_flag & MNT_RDONLY) == 0)
			ip->i_flag |= IN_MODIFIED;
	}
	*vpp = vp;
	return (0);
}

/*
 * File handle to vnode
 *
 * Have to be really careful about stale file handles:
 * - check that the inode number is valid
 * - call ext2_vget() to get the locked inode
 * - check for an unallocated inode (i_mode == 0)
 * - check that the given client host has export rights and return
 *   those rights via. exflagsp and credanonp
 */
static int
ext2_fhtovp(mp, fhp, nam, vpp, exflagsp, credanonp)
	register struct mount *mp;
	struct fid *fhp;
	struct sockaddr *nam;
	struct vnode **vpp;
	int *exflagsp;
	struct ucred **credanonp;
{
	register struct ufid *ufhp;
	struct ext2_sb_info *fs;

	ufhp = (struct ufid *)fhp;
	fs = VFSTOUFS(mp)->um_e2fs;
	if (ufhp->ufid_ino < ROOTINO ||
	    ufhp->ufid_ino >= fs->s_groups_count * fs->s_es->s_inodes_per_group)
		return (ESTALE);
	return (ufs_check_export(mp, ufhp, nam, vpp, exflagsp, credanonp));
}

/*
 * Vnode pointer to File handle
 */
/* ARGSUSED */
static int
ext2_vptofh(vp, fhp)
	struct vnode *vp;
	struct fid *fhp;
{
	register struct inode *ip;
	register struct ufid *ufhp;

	ip = VTOI(vp);
	ufhp = (struct ufid *)fhp;
	ufhp->ufid_len = sizeof(struct ufid);
	ufhp->ufid_ino = ip->i_number;
	ufhp->ufid_gen = ip->i_gen;
	return (0);
}

/*
 * Write a superblock and associated information back to disk.
 */
static int
ext2_sbupdate(mp, waitfor)
	struct ufsmount *mp;
	int waitfor;
{
	register struct ext2_sb_info *fs = mp->um_e2fs;
	register struct ext2_super_block *es = fs->s_es;
	register struct buf *bp;
	int i, error = 0;
/*
printf("\nupdating superblock, waitfor=%s\n", waitfor == MNT_WAIT ? "yes":"no");
*/
	bp = getblk(mp->um_devvp, SBLOCK, SBSIZE, 0, 0);
	bcopy((caddr_t)es, bp->b_data, (u_int)sizeof(struct ext2_super_block));
	if (waitfor == MNT_WAIT)
		error = bwrite(bp);
	else
		bawrite(bp);

	/*
	 * The buffers for group descriptors, inode bitmaps and block bitmaps
	 * are not busy at this point and are (hopefully) written by the
	 * usual sync mechanism. No need to write them here
		 */

	return (error);
}
