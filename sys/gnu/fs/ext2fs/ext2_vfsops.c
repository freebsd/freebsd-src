/*-
 *  modified for EXT2FS support in Lites 1.1
 *
 *  Aug 1995, Godmar Back (gback@cs.utah.edu)
 *  University of Utah, Department of Computer Science
 */
/*-
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
 * $FreeBSD$
 */

/*-
 * COPYRIGHT.INFO says this has some GPL'd code from ext2_super.c in it
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/stat.h>
#include <sys/mutex.h>

#include <geom/geom.h>
#include <geom/geom_vfs.h>

#include <gnu/fs/ext2fs/ext2_mount.h>
#include <gnu/fs/ext2fs/inode.h>

#include <gnu/fs/ext2fs/fs.h>
#include <gnu/fs/ext2fs/ext2_extern.h>
#include <gnu/fs/ext2fs/ext2_fs.h>
#include <gnu/fs/ext2fs/ext2_fs_sb.h>

static int ext2_flushfiles(struct mount *mp, int flags, struct thread *td);
static int ext2_mountfs(struct vnode *, struct mount *, struct thread *);
static int ext2_reload(struct mount *mp, struct thread *td);
static int ext2_sbupdate(struct ext2mount *, int);

static vfs_unmount_t		ext2_unmount;
static vfs_root_t		ext2_root;
static vfs_statfs_t		ext2_statfs;
static vfs_sync_t		ext2_sync;
static vfs_vget_t		ext2_vget;
static vfs_fhtovp_t		ext2_fhtovp;
static vfs_vptofh_t		ext2_vptofh;
static vfs_mount_t		ext2_mount;

MALLOC_DEFINE(M_EXT2NODE, "EXT2 node", "EXT2 vnode private part");
static MALLOC_DEFINE(M_EXT2MNT, "EXT2 mount", "EXT2 mount structure");

static struct vfsops ext2fs_vfsops = {
	.vfs_fhtovp =		ext2_fhtovp,
	.vfs_mount =		ext2_mount,
	.vfs_root =		ext2_root,	/* root inode via vget */
	.vfs_statfs =		ext2_statfs,
	.vfs_sync =		ext2_sync,
	.vfs_unmount =		ext2_unmount,
	.vfs_vget =		ext2_vget,
	.vfs_vptofh =		ext2_vptofh,
};

VFS_SET(ext2fs_vfsops, ext2fs, 0);

#define bsd_malloc malloc
#define bsd_free free

static int	ext2_check_sb_compat(struct ext2_super_block *es, struct cdev *dev,
		    int ronly);
static int	compute_sb_data(struct vnode * devvp,
		    struct ext2_super_block * es, struct ext2_sb_info * fs);

static const char *ext2_opts[] = { "from", "export" };
/*
 * VFS Operations.
 *
 * mount system call
 */
static int
ext2_mount(mp, td)
	struct mount *mp;
	struct thread *td;
{
	struct vfsoptlist *opts;
	struct vnode *devvp;
	struct ext2mount *ump = 0;
	struct ext2_sb_info *fs;
	char *path, *fspec;
	int error, flags, len;
	mode_t accessmode;
	struct nameidata nd, *ndp = &nd;

	opts = mp->mnt_optnew;

	if (vfs_filteropt(opts, ext2_opts))
		return (EINVAL);

	vfs_getopt(opts, "fspath", (void **)&path, NULL);
	/* Double-check the length of path.. */
	if (strlen(path) >= MAXMNTLEN - 1)
		return (ENAMETOOLONG);

	fspec = NULL;
	error = vfs_getopt(opts, "from", (void **)&fspec, &len);
	if (!error && fspec[len - 1] != '\0')
		return (EINVAL);

	/*
	 * If updating, check whether changing from read-only to
	 * read/write; if there is no device name, that's all we do.
	 */
	if (mp->mnt_flag & MNT_UPDATE) {
		ump = VFSTOEXT2(mp);
		fs = ump->um_e2fs;
		error = 0;
		if (fs->s_rd_only == 0 &&
		    vfs_flagopt(opts, "ro", NULL, 0)) {
			error = VFS_SYNC(mp, MNT_WAIT, td);
			if (error)
				return (error);
			flags = WRITECLOSE;
			if (mp->mnt_flag & MNT_FORCE)
				flags |= FORCECLOSE;
			if (vfs_busy(mp, LK_NOWAIT, 0, td))
				return (EBUSY);
			error = ext2_flushfiles(mp, flags, td);
			vfs_unbusy(mp, td);
			if (!error && fs->s_wasvalid) {
				fs->s_es->s_state |= EXT2_VALID_FS;
				ext2_sbupdate(ump, MNT_WAIT);
			}
			fs->s_rd_only = 1;
			vfs_flagopt(opts, "ro", &mp->mnt_flag, MNT_RDONLY);
			DROP_GIANT();
			g_topology_lock();
			g_access(ump->um_cp, 0, -1, 0);
			g_topology_unlock();
			PICKUP_GIANT();
		}
		if (!error && (mp->mnt_flag & MNT_RELOAD))
			error = ext2_reload(mp, td);
		if (error)
			return (error);
		devvp = ump->um_devvp;
		if (fs->s_rd_only && !vfs_flagopt(opts, "ro", NULL, 0)) {
			if (ext2_check_sb_compat(fs->s_es, devvp->v_rdev, 0))
				return (EPERM);
			/*
			 * If upgrade to read-write by non-root, then verify
			 * that user has necessary permissions on the device.
			 */
			if (suser(td)) {
				vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY, td);
				if ((error = VOP_ACCESS(devvp, VREAD | VWRITE,
				    td->td_ucred, td)) != 0) {
					VOP_UNLOCK(devvp, 0, td);
					return (error);
				}
				VOP_UNLOCK(devvp, 0, td);
			}
			DROP_GIANT();
			g_topology_lock();
			error = g_access(ump->um_cp, 0, 1, 0);
			g_topology_unlock();
			PICKUP_GIANT();
			if (error)
				return (error);

			if ((fs->s_es->s_state & EXT2_VALID_FS) == 0 ||
			    (fs->s_es->s_state & EXT2_ERROR_FS)) {
				if (mp->mnt_flag & MNT_FORCE) {
					printf(
"WARNING: %s was not properly dismounted\n",
					    fs->fs_fsmnt);
				} else {
					printf(
"WARNING: R/W mount of %s denied.  Filesystem is not clean - run fsck\n",
					    fs->fs_fsmnt);
					return (EPERM);
				}
			}
			fs->s_es->s_state &= ~EXT2_VALID_FS;
			ext2_sbupdate(ump, MNT_WAIT);
			fs->s_rd_only = 0;
			MNT_ILOCK(mp);
			mp->mnt_flag &= ~MNT_RDONLY;
			MNT_IUNLOCK(mp);
		}
		if (vfs_flagopt(opts, "export", NULL, 0)) {
			/* Process export requests in vfs_mount.c. */
			return (error);
		}
	}
	/*
	 * Not an update, or updating the name: look up the name
	 * and verify that it refers to a sensible disk device.
	 */
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

	/*
	 * If mount by non-root, then verify that user has necessary
	 * permissions on the device.
	 */
	if (suser(td)) {
		accessmode = VREAD;
		if ((mp->mnt_flag & MNT_RDONLY) == 0)
			accessmode |= VWRITE;
		if ((error = VOP_ACCESS(devvp, accessmode, td->td_ucred, td)) != 0) {
			vput(devvp);
			return (error);
		}
	}

	if ((mp->mnt_flag & MNT_UPDATE) == 0) {
		error = ext2_mountfs(devvp, mp, td);
	} else {
		if (devvp != ump->um_devvp) {
			vput(devvp);
			return (EINVAL);	/* needs translation */
		} else
			vput(devvp);
	}
	if (error) {
		vrele(devvp);
		return (error);
	}
	ump = VFSTOEXT2(mp);
	fs = ump->um_e2fs;
	/*
	 * Note that this strncpy() is ok because of a check at the start
	 * of ext2_mount().
	 */
	strncpy(fs->fs_fsmnt, path, MAXMNTLEN);
	fs->fs_fsmnt[MAXMNTLEN - 1] = '\0';
	vfs_mountedfrom(mp, fspec);
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
                                    " not in group (block %lu)!\n",
                                    i, (unsigned long) gdp->bg_block_bitmap);
                        return 0;
                }
                if (gdp->bg_inode_bitmap < block ||
                    gdp->bg_inode_bitmap >= block + EXT2_BLOCKS_PER_GROUP(sb))
                {
                        printf ("ext2_check_descriptors: "
                                    "Inode bitmap for group %d"
                                    " not in group (block %lu)!\n",
                                    i, (unsigned long) gdp->bg_inode_bitmap);
                        return 0;
                }
                if (gdp->bg_inode_table < block ||
                    gdp->bg_inode_table + sb->s_itb_per_group >=
                    block + EXT2_BLOCKS_PER_GROUP(sb))
                {
                        printf ("ext2_check_descriptors: "
                                    "Inode table for group %d"
                                    " not in group (block %lu)!\n",
                                    i, (unsigned long) gdp->bg_inode_table);
                        return 0;
                }
                block += EXT2_BLOCKS_PER_GROUP(sb);
                gdp++;
        }
        return 1;
}

static int
ext2_check_sb_compat(es, dev, ronly)
	struct ext2_super_block *es;
	struct cdev *dev;
	int ronly;
{

	if (es->s_magic != EXT2_SUPER_MAGIC) {
		printf("ext2fs: %s: wrong magic number %#x (expected %#x)\n",
		    devtoname(dev), es->s_magic, EXT2_SUPER_MAGIC);
		return (1);
	}
	if (es->s_rev_level > EXT2_GOOD_OLD_REV) {
		if (es->s_feature_incompat & ~EXT2_FEATURE_INCOMPAT_SUPP) {
			printf(
"WARNING: mount of %s denied due to unsupported optional features\n",
			    devtoname(dev));
			return (1);
		}
		if (!ronly &&
		    (es->s_feature_ro_compat & ~EXT2_FEATURE_RO_COMPAT_SUPP)) {
			printf(
"WARNING: R/W mount of %s denied due to unsupported optional features\n",
			    devtoname(dev));
			return (1);
		}
	}
	return (0);
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
		M_EXT2MNT, M_WAITOK);

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
	    bsd_free(fs->s_group_desc, M_EXT2MNT);
	    printf("EXT2-fs: unable to read group descriptors (%d)\n", error);
	    return EIO;
	}
	LCK_BUF(fs->s_group_desc[i])
    }
    if(!ext2_check_descriptors(fs)) {
	    for (j = 0; j < db_count; j++)
		    ULCK_BUF(fs->s_group_desc[j])
	    bsd_free(fs->s_group_desc, M_EXT2MNT);
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
    if (es->s_rev_level == EXT2_GOOD_OLD_REV || (es->s_feature_ro_compat &
        EXT2_FEATURE_RO_COMPAT_LARGE_FILE) == 0)
	fs->fs_maxfilesize = 0x7fffffff;
    else
	fs->fs_maxfilesize = 0x7fffffffffffffff;
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
ext2_reload(struct mount *mp, struct thread *td)
{
	struct vnode *vp, *mvp, *devvp;
	struct inode *ip;
	struct buf *bp;
	struct ext2_super_block * es;
	struct ext2_sb_info *fs;
	int error;

	if ((mp->mnt_flag & MNT_RDONLY) == 0)
		return (EINVAL);
	/*
	 * Step 1: invalidate all cached meta-data.
	 */
	devvp = VFSTOEXT2(mp)->um_devvp;
	vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY, td);
	if (vinvalbuf(devvp, 0, td, 0, 0) != 0)
		panic("ext2_reload: dirty1");
	VOP_UNLOCK(devvp, 0, td);

	/*
	 * Step 2: re-read superblock from disk.
	 * constants have been adjusted for ext2
	 */
	if ((error = bread(devvp, SBLOCK, SBSIZE, NOCRED, &bp)) != 0)
		return (error);
	es = (struct ext2_super_block *)bp->b_data;
	if (ext2_check_sb_compat(es, devvp->v_rdev, 0) != 0) {
		brelse(bp);
		return (EIO);		/* XXX needs translation */
	}
	fs = VFSTOEXT2(mp)->um_e2fs;
	bcopy(bp->b_data, fs->s_es, sizeof(struct ext2_super_block));

	if((error = compute_sb_data(devvp, es, fs)) != 0) {
		brelse(bp);
		return error;
	}
#ifdef UNKLAR
	if (fs->fs_sbsize < SBSIZE)
		bp->b_flags |= B_INVAL;
#endif
	brelse(bp);

loop:
	MNT_ILOCK(mp);
	MNT_VNODE_FOREACH(vp, mp, mvp) {
		VI_LOCK(vp);
		if (vp->v_iflag & VI_DOOMED) {
			VI_UNLOCK(vp);
			continue;
		}
		MNT_IUNLOCK(mp);
		/*
		 * Step 4: invalidate all cached file data.
		 */
		if (vget(vp, LK_EXCLUSIVE | LK_INTERLOCK, td)) {
			MNT_VNODE_FOREACH_ABORT(mp, mvp);
			goto loop;
		}
		if (vinvalbuf(vp, 0, td, 0, 0))
			panic("ext2_reload: dirty2");
		/*
		 * Step 5: re-read inode data for all active vnodes.
		 */
		ip = VTOI(vp);
		error =
		    bread(devvp, fsbtodb(fs, ino_to_fsba(fs, ip->i_number)),
		    (int)fs->s_blocksize, NOCRED, &bp);
		if (error) {
			VOP_UNLOCK(vp, 0, td);
			vrele(vp);
			MNT_VNODE_FOREACH_ABORT(mp, mvp);
			return (error);
		}
		ext2_ei2i((struct ext2_inode *) ((char *)bp->b_data +
		    EXT2_INODE_SIZE * ino_to_fsbo(fs, ip->i_number)), ip);
		brelse(bp);
		VOP_UNLOCK(vp, 0, td);
		vrele(vp);
		MNT_ILOCK(mp);
	}
	MNT_IUNLOCK(mp);
	return (0);
}

/*
 * Common code for mount and mountroot
 */
static int
ext2_mountfs(devvp, mp, td)
	struct vnode *devvp;
	struct mount *mp;
	struct thread *td;
{
	struct ext2mount *ump;
	struct buf *bp;
	struct ext2_sb_info *fs;
	struct ext2_super_block * es;
	struct cdev *dev = devvp->v_rdev;
	struct g_consumer *cp;
	struct bufobj *bo;
	int error;
	int ronly;

	ronly = vfs_flagopt(mp->mnt_optnew, "ro", NULL, 0);
	/* XXX: use VOP_ACESS to check FS perms */
	DROP_GIANT();
	g_topology_lock();
	error = g_vfs_open(devvp, &cp, "ext2fs", ronly ? 0 : 1);
	g_topology_unlock();
	PICKUP_GIANT();
	VOP_UNLOCK(devvp, 0, td);
	if (error)
		return (error);

	/* XXX: should we check for some sectorsize or 512 instead? */
	if (((SBSIZE % cp->provider->sectorsize) != 0) ||
	    (SBSIZE < cp->provider->sectorsize)) {
		DROP_GIANT();
		g_topology_lock();
		g_vfs_close(cp, td);
		g_topology_unlock();
		PICKUP_GIANT();
		return (EINVAL);
	}

	bo = &devvp->v_bufobj;
	bo->bo_private = cp;
	bo->bo_ops = g_vfs_bufops;
	if (devvp->v_rdev->si_iosize_max != 0)
		mp->mnt_iosize_max = devvp->v_rdev->si_iosize_max;
	if (mp->mnt_iosize_max > MAXPHYS)
		mp->mnt_iosize_max = MAXPHYS;

	bp = NULL;
	ump = NULL;
	if ((error = bread(devvp, SBLOCK, SBSIZE, NOCRED, &bp)) != 0)
		goto out;
	es = (struct ext2_super_block *)bp->b_data;
	if (ext2_check_sb_compat(es, dev, ronly) != 0) {
		error = EINVAL;		/* XXX needs translation */
		goto out;
	}
	if ((es->s_state & EXT2_VALID_FS) == 0 ||
	    (es->s_state & EXT2_ERROR_FS)) {
		if (ronly || (mp->mnt_flag & MNT_FORCE)) {
			printf(
"WARNING: Filesystem was not properly dismounted\n");
		} else {
			printf(
"WARNING: R/W mount denied.  Filesystem is not clean - run fsck\n");
			error = EPERM;
			goto out;
		}
	}
	ump = bsd_malloc(sizeof *ump, M_EXT2MNT, M_WAITOK);
	bzero((caddr_t)ump, sizeof *ump);
	/* I don't know whether this is the right strategy. Note that
	   we dynamically allocate both an ext2_sb_info and an ext2_super_block
	   while Linux keeps the super block in a locked buffer
	 */
	ump->um_e2fs = bsd_malloc(sizeof(struct ext2_sb_info), 
		M_EXT2MNT, M_WAITOK);
	ump->um_e2fs->s_es = bsd_malloc(sizeof(struct ext2_super_block), 
		M_EXT2MNT, M_WAITOK);
	bcopy(es, ump->um_e2fs->s_es, (u_int)sizeof(struct ext2_super_block));
	if ((error = compute_sb_data(devvp, ump->um_e2fs->s_es, ump->um_e2fs)))
		goto out;
	/*
	 * We don't free the group descriptors allocated by compute_sb_data()
	 * until ext2_unmount().  This is OK since the mount will succeed.
	 */
	brelse(bp);
	bp = NULL;
	fs = ump->um_e2fs;
	fs->s_rd_only = ronly;	/* ronly is set according to mnt_flags */
	/* if the fs is not mounted read-only, make sure the super block is 
	   always written back on a sync()
	 */
	fs->s_wasvalid = fs->s_es->s_state & EXT2_VALID_FS ? 1 : 0;
	if (ronly == 0) {
		fs->s_dirt = 1;		/* mark it modified */
		fs->s_es->s_state &= ~EXT2_VALID_FS;	/* set fs invalid */
	}
	mp->mnt_data = (qaddr_t)ump;
	mp->mnt_stat.f_fsid.val[0] = dev2udev(dev);
	mp->mnt_stat.f_fsid.val[1] = mp->mnt_vfc->vfc_typenum;
	mp->mnt_maxsymlinklen = EXT2_MAXSYMLINKLEN;
	MNT_ILOCK(mp);
	mp->mnt_flag |= MNT_LOCAL;
	MNT_IUNLOCK(mp);
	ump->um_mountp = mp;
	ump->um_dev = dev;
	ump->um_devvp = devvp;
	ump->um_bo = &devvp->v_bufobj;
	ump->um_cp = cp;
	/* setting those two parameters allowed us to use
	   ufs_bmap w/o changse !
	*/
	ump->um_nindir = EXT2_ADDR_PER_BLOCK(fs);
	ump->um_bptrtodb = fs->s_es->s_log_block_size + 1;
	ump->um_seqinc = EXT2_FRAGS_PER_BLOCK(fs);
	if (ronly == 0) 
		ext2_sbupdate(ump, MNT_WAIT);
	return (0);
out:
	if (bp)
		brelse(bp);
	if (cp != NULL) {
		DROP_GIANT();
		g_topology_lock();
		g_vfs_close(cp, td);
		g_topology_unlock();
		PICKUP_GIANT();
	}
	if (ump) {
		bsd_free(ump->um_e2fs->s_es, M_EXT2MNT);
		bsd_free(ump->um_e2fs, M_EXT2MNT);
		bsd_free(ump, M_EXT2MNT);
		mp->mnt_data = (qaddr_t)0;
	}
	return (error);
}

/*
 * unmount system call
 */
static int
ext2_unmount(mp, mntflags, td)
	struct mount *mp;
	int mntflags;
	struct thread *td;
{
	struct ext2mount *ump;
	struct ext2_sb_info *fs;
	int error, flags, ronly, i;

	flags = 0;
	if (mntflags & MNT_FORCE) {
		if (mp->mnt_flag & MNT_ROOTFS)
			return (EINVAL);
		flags |= FORCECLOSE;
	}
	if ((error = ext2_flushfiles(mp, flags, td)) != 0)
		return (error);
	ump = VFSTOEXT2(mp);
	fs = ump->um_e2fs;
	ronly = fs->s_rd_only;
	if (ronly == 0) {
		if (fs->s_wasvalid)
			fs->s_es->s_state |= EXT2_VALID_FS;
		ext2_sbupdate(ump, MNT_WAIT);
	}

	/* release buffers containing group descriptors */
	for(i = 0; i < fs->s_db_per_group; i++) 
		ULCK_BUF(fs->s_group_desc[i])
	bsd_free(fs->s_group_desc, M_EXT2MNT);

	/* release cached inode/block bitmaps */
        for (i = 0; i < EXT2_MAX_GROUP_LOADED; i++)
                if (fs->s_inode_bitmap[i])
			ULCK_BUF(fs->s_inode_bitmap[i])

        for (i = 0; i < EXT2_MAX_GROUP_LOADED; i++)
                if (fs->s_block_bitmap[i])
			ULCK_BUF(fs->s_block_bitmap[i])

	DROP_GIANT();
	g_topology_lock();
	g_vfs_close(ump->um_cp, td);
	g_topology_unlock();
	PICKUP_GIANT();
	vrele(ump->um_devvp);
	bsd_free(fs->s_es, M_EXT2MNT);
	bsd_free(fs, M_EXT2MNT);
	bsd_free(ump, M_EXT2MNT);
	mp->mnt_data = (qaddr_t)0;
	MNT_ILOCK(mp);
	mp->mnt_flag &= ~MNT_LOCAL;
	MNT_IUNLOCK(mp);
	return (error);
}

/*
 * Flush out all the files in a filesystem.
 */
static int
ext2_flushfiles(mp, flags, td)
	struct mount *mp;
	int flags;
	struct thread *td;
{
	int error;

	error = vflush(mp, 0, flags, td);
	return (error);
}

/*
 * Get file system statistics.
 * taken from ext2/super.c ext2_statfs
 */
static int
ext2_statfs(mp, sbp, td)
	struct mount *mp;
	struct statfs *sbp;
	struct thread *td;
{
        unsigned long overhead;
	struct ext2mount *ump;
	struct ext2_sb_info *fs;
	struct ext2_super_block *es;
	int i, nsb;

	ump = VFSTOEXT2(mp);
	fs = ump->um_e2fs;
	es = fs->s_es;

	if (es->s_magic != EXT2_SUPER_MAGIC)
		panic("ext2_statfs - magic number spoiled");

	/*
	 * Compute the overhead (FS structures)
	 */
	if (es->s_feature_ro_compat & EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER) {
		nsb = 0;
		for (i = 0 ; i < fs->s_groups_count; i++)
			if (ext2_group_sparse(i))
				nsb++;
	} else
		nsb = fs->s_groups_count;
	overhead = es->s_first_data_block + 
	    /* Superblocks and block group descriptors: */
	    nsb * (1 + fs->s_db_per_group) +
	    /* Inode bitmap, block bitmap, and inode table: */
	    fs->s_groups_count * (1 + 1 + fs->s_itb_per_group);

	sbp->f_bsize = EXT2_FRAG_SIZE(fs);	
	sbp->f_iosize = EXT2_BLOCK_SIZE(fs);
	sbp->f_blocks = es->s_blocks_count - overhead;
	sbp->f_bfree = es->s_free_blocks_count; 
	sbp->f_bavail = sbp->f_bfree - es->s_r_blocks_count; 
	sbp->f_files = es->s_inodes_count; 
	sbp->f_ffree = es->s_free_inodes_count; 
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
ext2_sync(mp, waitfor, td)
	struct mount *mp;
	int waitfor;
	struct thread *td;
{
	struct vnode *mvp, *vp;
	struct inode *ip;
	struct ext2mount *ump = VFSTOEXT2(mp);
	struct ext2_sb_info *fs;
	int error, allerror = 0;

	fs = ump->um_e2fs;
	if (fs->s_dirt != 0 && fs->s_rd_only != 0) {		/* XXX */
		printf("fs = %s\n", fs->fs_fsmnt);
		panic("ext2_sync: rofs mod");
	}
	/*
	 * Write back each (modified) inode.
	 */
	MNT_ILOCK(mp);
loop:
	MNT_VNODE_FOREACH(vp, mp, mvp) {
		VI_LOCK(vp);
		if (vp->v_type == VNON || (vp->v_iflag & VI_DOOMED)) {
			VI_UNLOCK(vp);
			continue;
		}
		MNT_IUNLOCK(mp);
		ip = VTOI(vp);
		if ((ip->i_flag &
		    (IN_ACCESS | IN_CHANGE | IN_MODIFIED | IN_UPDATE)) == 0 &&
		    (vp->v_bufobj.bo_dirty.bv_cnt == 0 ||
		    waitfor == MNT_LAZY)) {
			VI_UNLOCK(vp);
			MNT_ILOCK(mp);
			continue;
		}
		error = vget(vp, LK_EXCLUSIVE | LK_NOWAIT | LK_INTERLOCK, td);
		if (error) {
			MNT_ILOCK(mp);
			if (error == ENOENT) {
				MNT_VNODE_FOREACH_ABORT_ILOCKED(mp, mvp);
				goto loop;
			}
			continue;
		}
		if ((error = VOP_FSYNC(vp, waitfor, td)) != 0)
			allerror = error;
		VOP_UNLOCK(vp, 0, td);
		vrele(vp);
		MNT_ILOCK(mp);
	}
	MNT_IUNLOCK(mp);
	/*
	 * Force stale file system control information to be flushed.
	 */
	if (waitfor != MNT_LAZY) {
		vn_lock(ump->um_devvp, LK_EXCLUSIVE | LK_RETRY, td);
		if ((error = VOP_FSYNC(ump->um_devvp, waitfor, td)) != 0)
			allerror = error;
		VOP_UNLOCK(ump->um_devvp, 0, td);
	}
	/*
	 * Write back modified superblock.
	 */
	if (fs->s_dirt != 0) {
		fs->s_dirt = 0;
		fs->s_es->s_wtime = time_second;
		if ((error = ext2_sbupdate(ump, waitfor)) != 0)
			allerror = error;
	}
	return (allerror);
}

/*
 * Look up an EXT2FS dinode number to find its incore vnode, otherwise read it
 * in from disk.  If it is in core, wait for the lock bit to clear, then
 * return the inode locked.  Detection and handling of mount points must be
 * done by the calling routine.
 */
static int
ext2_vget(mp, ino, flags, vpp)
	struct mount *mp;
	ino_t ino;
	int flags;
	struct vnode **vpp;
{
	struct ext2_sb_info *fs;
	struct inode *ip;
	struct ext2mount *ump;
	struct buf *bp;
	struct vnode *vp;
	struct cdev *dev;
	int i, error;
	int used_blocks;

	error = vfs_hash_get(mp, ino, flags, curthread, vpp, NULL, NULL);
	if (error || *vpp != NULL)
		return (error);

	ump = VFSTOEXT2(mp);
	dev = ump->um_dev;

	/*
	 * If this MALLOC() is performed after the getnewvnode()
	 * it might block, leaving a vnode with a NULL v_data to be
	 * found by ext2_sync() if a sync happens to fire right then,
	 * which will cause a panic because ext2_sync() blindly
	 * dereferences vp->v_data (as well it should).
	 */
	ip = malloc(sizeof(struct inode), M_EXT2NODE, M_WAITOK | M_ZERO);

	/* Allocate a new vnode/inode. */
	if ((error = getnewvnode("ext2fs", mp, &ext2_vnodeops, &vp)) != 0) {
		*vpp = NULL;
		free(ip, M_EXT2NODE);
		return (error);
	}
	vp->v_data = ip;
	ip->i_vnode = vp;
	ip->i_e2fs = fs = ump->um_e2fs;
	ip->i_number = ino;

	error = vfs_hash_insert(vp, ino, flags, curthread, vpp, NULL, NULL);
	if (error || *vpp != NULL)
		return (error);

	/* Read in the disk contents for the inode, copy into the inode. */
#if 0
printf("ext2_vget(%d) dbn= %d ", ino, fsbtodb(fs, ino_to_fsba(fs, ino)));
#endif
	if ((error = bread(ump->um_devvp, fsbtodb(fs, ino_to_fsba(fs, ino)),
	    (int)fs->s_blocksize, NOCRED, &bp)) != 0) {
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
	ext2_ei2i((struct ext2_inode *) ((char *)bp->b_data + EXT2_INODE_SIZE *
			ino_to_fsbo(fs, ino)), ip);
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
	if ((error = ext2_vinit(mp, &ext2_fifoops, &vp)) != 0) {
		vput(vp);
		*vpp = NULL;
		return (error);
	}
	/*
	 * Finish inode initialization now that aliasing has been resolved.
	 */
	ip->i_devvp = ump->um_devvp;
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
ext2_fhtovp(mp, fhp, vpp)
	struct mount *mp;
	struct fid *fhp;
	struct vnode **vpp;
{
	struct inode *ip;
	struct ufid *ufhp;
	struct vnode *nvp;
	struct ext2_sb_info *fs;
	int error;

	ufhp = (struct ufid *)fhp;
	fs = VFSTOEXT2(mp)->um_e2fs;
	if (ufhp->ufid_ino < ROOTINO ||
	    ufhp->ufid_ino > fs->s_groups_count * fs->s_es->s_inodes_per_group)
		return (ESTALE);

	error = VFS_VGET(mp, ufhp->ufid_ino, LK_EXCLUSIVE, &nvp);
	if (error) {
		*vpp = NULLVP;
		return (error);
	}
	ip = VTOI(nvp);
	if (ip->i_mode == 0 ||
	    ip->i_gen != ufhp->ufid_gen || ip->i_nlink <= 0) {
		vput(nvp);
		*vpp = NULLVP;
		return (ESTALE);
	}
	*vpp = nvp;
	vnode_create_vobject_off(*vpp, 0, curthread);
	return (0);
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
	struct inode *ip;
	struct ufid *ufhp;

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
	struct ext2mount *mp;
	int waitfor;
{
	struct ext2_sb_info *fs = mp->um_e2fs;
	struct ext2_super_block *es = fs->s_es;
	struct buf *bp;
	int error = 0;
/*
printf("\nupdating superblock, waitfor=%s\n", waitfor == MNT_WAIT ? "yes":"no");
*/
	bp = getblk(mp->um_devvp, SBLOCK, SBSIZE, 0, 0, 0);
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

/*
 * Return the root of a filesystem.
 */
static int
ext2_root(mp, flags, vpp, td)
	struct mount *mp;
	int flags;
	struct vnode **vpp;
	struct thread *td;
{
	struct vnode *nvp;
	int error;

	error = VFS_VGET(mp, (ino_t)ROOTINO, LK_EXCLUSIVE, &nvp);
	if (error)
		return (error);
	*vpp = nvp;
	return (0);
}
