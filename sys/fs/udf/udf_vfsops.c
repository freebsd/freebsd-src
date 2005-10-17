/*-
 * Copyright (c) 2001, 2002 Scott Long <scottl@freebsd.org>
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

/* udf_vfsops.c */
/* Implement the VFS side of things */

/*
 * Ok, here's how it goes.  The UDF specs are pretty clear on how each data
 * structure is made up, but not very clear on how they relate to each other.
 * Here is the skinny... This demostrates a filesystem with one file in the
 * root directory.  Subdirectories are treated just as normal files, but they
 * have File Id Descriptors of their children as their file data.  As for the
 * Anchor Volume Descriptor Pointer, it can exist in two of the following three
 * places: sector 256, sector n (the max sector of the disk), or sector
 * n - 256.  It's a pretty good bet that one will exist at sector 256 though.
 * One caveat is unclosed CD media.  For that, sector 256 cannot be written,
 * so the Anchor Volume Descriptor Pointer can exist at sector 512 until the
 * media is closed.
 *
 *  Sector:
 *     256:
 *       n: Anchor Volume Descriptor Pointer
 * n - 256:	|
 *		|
 *		|-->Main Volume Descriptor Sequence
 *			|	|
 *			|	|
 *			|	|-->Logical Volume Descriptor
 *			|			  |
 *			|-->Partition Descriptor  |
 *				|		  |
 *				|		  |
 *				|-->Fileset Descriptor
 *					|
 *					|
 *					|-->Root Dir File Entry
 *						|
 *						|
 *						|-->File data:
 *						    File Id Descriptor
 *							|
 *							|
 *							|-->File Entry
 *								|
 *								|
 *								|-->File data
 */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/dirent.h>
#include <sys/fcntl.h>
#include <sys/iconv.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/vnode.h>
#include <sys/endian.h>

#include <geom/geom.h>
#include <geom/geom_vfs.h>

#include <vm/uma.h>

#include <fs/udf/ecma167-udf.h>
#include <fs/udf/osta.h>
#include <fs/udf/udf.h>
#include <fs/udf/udf_mount.h>

static MALLOC_DEFINE(M_UDFMOUNT, "UDF mount", "UDF mount structure");
MALLOC_DEFINE(M_UDFFENTRY, "UDF fentry", "UDF file entry structure");

struct iconv_functions *udf_iconv = NULL;

/* Zones */
uma_zone_t udf_zone_trans = NULL;
uma_zone_t udf_zone_node = NULL;
uma_zone_t udf_zone_ds = NULL;

static vfs_init_t      udf_init;
static vfs_uninit_t    udf_uninit;
static vfs_mount_t     udf_mount;
static vfs_root_t      udf_root;
static vfs_statfs_t    udf_statfs;
static vfs_unmount_t   udf_unmount;
static vfs_fhtovp_t	udf_fhtovp;
static vfs_vptofh_t	udf_vptofh;

static int udf_find_partmaps(struct udf_mnt *, struct logvol_desc *);

static struct vfsops udf_vfsops = {
	.vfs_fhtovp =		udf_fhtovp,
	.vfs_init =		udf_init,
	.vfs_mount =		udf_mount,
	.vfs_root =		udf_root,
	.vfs_statfs =		udf_statfs,
	.vfs_uninit =		udf_uninit,
	.vfs_unmount =		udf_unmount,
	.vfs_vget =		udf_vget,
	.vfs_vptofh =		udf_vptofh,
};
VFS_SET(udf_vfsops, udf, VFCF_READONLY);

MODULE_VERSION(udf, 1);

static int udf_mountfs(struct vnode *, struct mount *, struct thread *);

static int
udf_init(struct vfsconf *foo)
{

	/*
	 * This code used to pre-allocate a certain number of pages for each
	 * pool, reducing the need to grow the zones later on.  UMA doesn't
	 * advertise any such functionality, unfortunately =-<
	 */
	udf_zone_trans = uma_zcreate("UDF translation buffer, zone", MAXNAMLEN *
	    sizeof(unicode_t), NULL, NULL, NULL, NULL, 0, 0);

	udf_zone_node = uma_zcreate("UDF Node zone", sizeof(struct udf_node),
	    NULL, NULL, NULL, NULL, 0, 0);

	udf_zone_ds = uma_zcreate("UDF Dirstream zone",
	    sizeof(struct udf_dirstream), NULL, NULL, NULL, NULL, 0, 0);

	if ((udf_zone_node == NULL) || (udf_zone_trans == NULL) ||
	    (udf_zone_ds == NULL)) {
		printf("Cannot create allocation zones.\n");
		return (ENOMEM);
	}

	return 0;
}

static int
udf_uninit(struct vfsconf *foo)
{

	if (udf_zone_trans != NULL) {
		uma_zdestroy(udf_zone_trans);
		udf_zone_trans = NULL;
	}

	if (udf_zone_node != NULL) {
		uma_zdestroy(udf_zone_node);
		udf_zone_node = NULL;
	}

	if (udf_zone_ds != NULL) {
		uma_zdestroy(udf_zone_ds);
		udf_zone_ds = NULL;
	}

	return (0);
}

static int
udf_mount(struct mount *mp, struct thread *td)
{
	struct vnode *devvp;	/* vnode of the mount device */
	struct udf_mnt *imp = 0;
	struct export_args *export;
	struct vfsoptlist *opts;
	char *fspec, *cs_disk, *cs_local;
	int error, len, *udf_flags;
	struct nameidata nd, *ndp = &nd;

	opts = mp->mnt_optnew;

	/*
	 * Unconditionally mount as read-only.
	 */
	mp->mnt_flag |= MNT_RDONLY;

	/*
	 * No root filesystem support.  Probably not a big deal, since the
	 * bootloader doesn't understand UDF.
	 */
	if (mp->mnt_flag & MNT_ROOTFS)
		return (ENOTSUP);

	fspec = NULL;
	error = vfs_getopt(opts, "from", (void **)&fspec, &len);
	if (!error && fspec[len - 1] != '\0')
		return (EINVAL);

	if (mp->mnt_flag & MNT_UPDATE) {
		if (fspec == NULL) {
			error = vfs_getopt(opts, "export", (void **)&export,
			    &len);
			if (error || len != sizeof(struct export_args))
				return (EINVAL);
			return (vfs_export(mp, export));
		}
	}

	/* Check that the mount device exists */
	if (fspec == NULL)
		return (EINVAL);
	NDINIT(ndp, LOOKUP, FOLLOW | LOCKLEAF, UIO_SYSSPACE, fspec, td);
	if ((error = namei(ndp)))
		return (error);
	NDFREE(ndp, NDF_ONLY_PNBUF);
	devvp = ndp->ni_vp;

	if (vn_isdisk(devvp, &error) == 0) {
		vput(devvp);
		return (error);
	}

	/* Check the access rights on the mount device */
	error = VOP_ACCESS(devvp, VREAD, td->td_ucred, td);
	if (error)
		error = suser(td);
	if (error) {
		vput(devvp);
		return (error);
	}

	if ((error = udf_mountfs(devvp, mp, td))) {
		vrele(devvp);
		return (error);
	}

	imp = VFSTOUDFFS(mp);

	udf_flags = NULL;
	error = vfs_getopt(opts, "flags", (void **)&udf_flags, &len);
	if (error || len != sizeof(int))
		return (EINVAL);
	imp->im_flags = *udf_flags;

	if (imp->im_flags & UDFMNT_KICONV && udf_iconv) {
		cs_disk = NULL;
		error = vfs_getopt(opts, "cs_disk", (void **)&cs_disk, &len);
		if (!error && cs_disk[len - 1] != '\0')
			return (EINVAL);
		cs_local = NULL;
		error = vfs_getopt(opts, "cs_local", (void **)&cs_local, &len);
		if (!error && cs_local[len - 1] != '\0')
			return (EINVAL);
		udf_iconv->open(cs_local, cs_disk, &imp->im_d2l);
#if 0
		udf_iconv->open(cs_disk, cs_local, &imp->im_l2d);
#endif
	}

	vfs_mountedfrom(mp, fspec);
	return 0;
};

/*
 * Check the descriptor tag for both the correct id and correct checksum.
 * Return zero if all is good, EINVAL if not.
 */
int
udf_checktag(struct desc_tag *tag, uint16_t id)
{
	uint8_t *itag;
	uint8_t i, cksum = 0;

	itag = (uint8_t *)tag;

	if (tag->id != id)
		return (EINVAL);

	for (i = 0; i < 15; i++)
		cksum = cksum + itag[i];
	cksum = cksum - itag[4];

	if (cksum == tag->cksum)
		return (0);

	return (EINVAL);
}

static int
udf_mountfs(struct vnode *devvp, struct mount *mp, struct thread *td) {
	struct buf *bp = NULL;
	struct anchor_vdp avdp;
	struct udf_mnt *udfmp = NULL;
	struct part_desc *pd;
	struct logvol_desc *lvd;
	struct fileset_desc *fsd;
	struct file_entry *root_fentry;
	uint32_t sector, size, mvds_start, mvds_end;
	uint32_t logical_secsize;
	uint32_t fsd_offset = 0;
	uint16_t part_num = 0, fsd_part = 0;
	int error = EINVAL;
	int logvol_found = 0, part_found = 0, fsd_found = 0;
	int bsize;
	struct g_consumer *cp;
	struct bufobj *bo;

	DROP_GIANT();
	g_topology_lock();
	error = g_vfs_open(devvp, &cp, "udf", 0);
	g_topology_unlock();
	PICKUP_GIANT();
	VOP_UNLOCK(devvp, 0, td);
	if (error)
		return error;

	bo = &devvp->v_bufobj;

	/* XXX: should be M_WAITOK */
	MALLOC(udfmp, struct udf_mnt *, sizeof(struct udf_mnt), M_UDFMOUNT,
	    M_NOWAIT | M_ZERO);
	if (udfmp == NULL) {
		printf("Cannot allocate UDF mount struct\n");
		error = ENOMEM;
		goto bail;
	}

	mp->mnt_data = (qaddr_t)udfmp;
	mp->mnt_stat.f_fsid.val[0] = dev2udev(devvp->v_rdev);
	mp->mnt_stat.f_fsid.val[1] = mp->mnt_vfc->vfc_typenum;
	mp->mnt_flag |= MNT_LOCAL;
	udfmp->im_mountp = mp;
	udfmp->im_dev = devvp->v_rdev;
	udfmp->im_devvp = devvp;
	udfmp->im_d2l = NULL;
	udfmp->im_cp = cp;
	udfmp->im_bo = bo;

#if 0
	udfmp->im_l2d = NULL;
#endif
	/*
	 * The UDF specification defines a logical sectorsize of 2048
	 * for DVD media.
	 */
	logical_secsize = 2048;

	if (((logical_secsize % cp->provider->sectorsize) != 0) ||
	    (logical_secsize < cp->provider->sectorsize)) {
		DROP_GIANT();
		g_topology_lock();
		g_vfs_close(cp, td);
		g_topology_unlock();
		PICKUP_GIANT();
		return (EINVAL);
	}

	bsize = cp->provider->sectorsize;

	/* 
	 * Get the Anchor Volume Descriptor Pointer from sector 256.
	 * XXX Should also check sector n - 256, n, and 512.
	 */
	sector = 256;
	if ((error = bread(devvp, sector * btodb(logical_secsize), bsize,
			   NOCRED, &bp)) != 0)
		goto bail;
	if ((error = udf_checktag((struct desc_tag *)bp->b_data, TAGID_ANCHOR)))
		goto bail;

	bcopy(bp->b_data, &avdp, sizeof(struct anchor_vdp));
	brelse(bp);
	bp = NULL;

	/*
	 * Extract the Partition Descriptor and Logical Volume Descriptor
	 * from the Volume Descriptor Sequence.
	 * XXX Should we care about the partition type right now?
	 * XXX What about multiple partitions?
	 */
	mvds_start = le32toh(avdp.main_vds_ex.loc);
	mvds_end = mvds_start + (le32toh(avdp.main_vds_ex.len) - 1) / bsize;
	for (sector = mvds_start; sector < mvds_end; sector++) {
		if ((error = bread(devvp, sector * btodb(logical_secsize),
				   bsize, NOCRED, &bp)) != 0) {
			printf("Can't read sector %d of VDS\n", sector);
			goto bail;
		}
		lvd = (struct logvol_desc *)bp->b_data;
		if (!udf_checktag(&lvd->tag, TAGID_LOGVOL)) {
			udfmp->bsize = le32toh(lvd->lb_size);
			udfmp->bmask = udfmp->bsize - 1;
			udfmp->bshift = ffs(udfmp->bsize) - 1;
			fsd_part = le16toh(lvd->_lvd_use.fsd_loc.loc.part_num);
			fsd_offset = le32toh(lvd->_lvd_use.fsd_loc.loc.lb_num);
			if (udf_find_partmaps(udfmp, lvd))
				break;
			logvol_found = 1;
		}
		pd = (struct part_desc *)bp->b_data;
		if (!udf_checktag(&pd->tag, TAGID_PARTITION)) {
			part_found = 1;
			part_num = le16toh(pd->part_num);
			udfmp->part_len = le32toh(pd->part_len);
			udfmp->part_start = le32toh(pd->start_loc);
		}

		brelse(bp); 
		bp = NULL;
		if ((part_found) && (logvol_found))
			break;
	}

	if (!part_found || !logvol_found) {
		error = EINVAL;
		goto bail;
	}

	if (fsd_part != part_num) {
		printf("FSD does not lie within the partition!\n");
		error = EINVAL;
		goto bail;
	}


	/*
	 * Grab the Fileset Descriptor
	 * Thanks to Chuck McCrobie <mccrobie@cablespeed.com> for pointing
	 * me in the right direction here.
	 */
	sector = udfmp->part_start + fsd_offset;
	if ((error = RDSECTOR(devvp, sector, udfmp->bsize, &bp)) != 0) {
		printf("Cannot read sector %d of FSD\n", sector);
		goto bail;
	}
	fsd = (struct fileset_desc *)bp->b_data;
	if (!udf_checktag(&fsd->tag, TAGID_FSD)) {
		fsd_found = 1;
		bcopy(&fsd->rootdir_icb, &udfmp->root_icb,
		    sizeof(struct long_ad));
	}

	brelse(bp);
	bp = NULL;

	if (!fsd_found) {
		printf("Couldn't find the fsd\n");
		error = EINVAL;
		goto bail;
	}

	/*
	 * Find the file entry for the root directory.
	 */
	sector = le32toh(udfmp->root_icb.loc.lb_num) + udfmp->part_start;
	size = le32toh(udfmp->root_icb.len);
	if ((error = udf_readlblks(udfmp, sector, size, &bp)) != 0) {
		printf("Cannot read sector %d\n", sector);
		goto bail;
	}

	root_fentry = (struct file_entry *)bp->b_data;
	if ((error = udf_checktag(&root_fentry->tag, TAGID_FENTRY))) {
		printf("Invalid root file entry!\n");
		goto bail;
	}

	brelse(bp);
	bp = NULL;

	return 0;

bail:
	if (udfmp != NULL)
		FREE(udfmp, M_UDFMOUNT);
	if (bp != NULL)
		brelse(bp);
	DROP_GIANT();
	g_topology_lock();
	g_vfs_close(cp, td);
	g_topology_unlock();
	PICKUP_GIANT();
	return error;
};

static int
udf_unmount(struct mount *mp, int mntflags, struct thread *td)
{
	struct udf_mnt *udfmp;
	int error, flags = 0;

	udfmp = VFSTOUDFFS(mp);

	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;

	if ((error = vflush(mp, 0, flags, td)))
		return (error);

	if (udfmp->im_flags & UDFMNT_KICONV && udf_iconv) {
		if (udfmp->im_d2l)
			udf_iconv->close(udfmp->im_d2l);
#if 0
		if (udfmp->im_l2d)
			udf_iconv->close(udfmp->im_l2d);
#endif
	}

	DROP_GIANT();
	g_topology_lock();
	g_vfs_close(udfmp->im_cp, td);
	g_topology_unlock();
	PICKUP_GIANT();
	vrele(udfmp->im_devvp);

	if (udfmp->s_table != NULL)
		FREE(udfmp->s_table, M_UDFMOUNT);

	FREE(udfmp, M_UDFMOUNT);

	mp->mnt_data = (qaddr_t)0;
	mp->mnt_flag &= ~MNT_LOCAL;

	return (0);
}

static int
udf_root(struct mount *mp, int flags, struct vnode **vpp, struct thread *td)
{
	struct udf_mnt *udfmp;
	struct vnode *vp;
	ino_t id;
	int error;

	udfmp = VFSTOUDFFS(mp);

	id = udf_getid(&udfmp->root_icb);

	error = udf_vget(mp, id, LK_EXCLUSIVE, vpp);
	if (error)
		return error;

	vp = *vpp;
	vp->v_vflag |= VV_ROOT;

	return (0);
}

static int
udf_statfs(struct mount *mp, struct statfs *sbp, struct thread *td)
{
	struct udf_mnt *udfmp;

	udfmp = VFSTOUDFFS(mp);

	sbp->f_bsize = udfmp->bsize;
	sbp->f_iosize = udfmp->bsize;
	sbp->f_blocks = udfmp->part_len;
	sbp->f_bfree = 0;
	sbp->f_bavail = 0;
	sbp->f_files = 0;
	sbp->f_ffree = 0;
	return 0;
}

int
udf_vget(struct mount *mp, ino_t ino, int flags, struct vnode **vpp)
{
	struct buf *bp;
	struct vnode *devvp;
	struct udf_mnt *udfmp;
	struct thread *td;
	struct vnode *vp;
	struct udf_node *unode;
	struct file_entry *fe;
	int error, sector, size;

	error = vfs_hash_get(mp, ino, flags, curthread, vpp, NULL, NULL);
	if (error || *vpp != NULL)
		return (error);

	td = curthread;
	udfmp = VFSTOUDFFS(mp);

	unode = uma_zalloc(udf_zone_node, M_WAITOK | M_ZERO);

	if ((error = udf_allocv(mp, &vp, td))) {
		printf("Error from udf_allocv\n");
		uma_zfree(udf_zone_node, unode);
		return (error);
	}

	unode->i_vnode = vp;
	unode->hash_id = ino;
	unode->udfmp = udfmp;
	vp->v_data = unode;

	error = vfs_hash_insert(vp, ino, flags, curthread, vpp, NULL, NULL);
	if (error || *vpp != NULL)
		return (error);

	/*
	 * Copy in the file entry.  Per the spec, the size can only be 1 block.
	 */
	sector = ino + udfmp->part_start;
	devvp = udfmp->im_devvp;
	if ((error = RDSECTOR(devvp, sector, udfmp->bsize, &bp)) != 0) {
		printf("Cannot read sector %d\n", sector);
		vput(vp);
		brelse(bp);
		*vpp = NULL;
		return (error);
	}

	fe = (struct file_entry *)bp->b_data;
	if (udf_checktag(&fe->tag, TAGID_FENTRY)) {
		printf("Invalid file entry!\n");
		vput(vp);
		brelse(bp);
		*vpp = NULL;
		return (ENOMEM);
	}
	size = UDF_FENTRY_SIZE + le32toh(fe->l_ea) + le32toh(fe->l_ad);
	MALLOC(unode->fentry, struct file_entry *, size, M_UDFFENTRY,
	    M_NOWAIT | M_ZERO);
	if (unode->fentry == NULL) {
		printf("Cannot allocate file entry block\n");
		vput(vp);
		brelse(bp);
		*vpp = NULL;
		return (ENOMEM);
	}

	bcopy(bp->b_data, unode->fentry, size);
	
	brelse(bp);
	bp = NULL;

	switch (unode->fentry->icbtag.file_type) {
	default:
		vp->v_type = VBAD;
		break;
	case 4:
		vp->v_type = VDIR;
		break;
	case 5:
		vp->v_type = VREG;
		break;
	case 6:
		vp->v_type = VBLK;
		break;
	case 7:
		vp->v_type = VCHR;
		break;
	case 9:
		vp->v_type = VFIFO;
		break;
	case 10:
		vp->v_type = VSOCK;
		break;
	case 12:
		vp->v_type = VLNK;
		break;
	}
	*vpp = vp;

	return (0);
}

struct ifid {
	u_short	ifid_len;
	u_short	ifid_pad;
	int	ifid_ino;
	long	ifid_start;
};

static int
udf_fhtovp(struct mount *mp, struct fid *fhp, struct vnode **vpp)
{
	struct ifid *ifhp;
	struct vnode *nvp;
	int error;

	ifhp = (struct ifid *)fhp;

	if ((error = VFS_VGET(mp, ifhp->ifid_ino, LK_EXCLUSIVE, &nvp)) != 0) {
		*vpp = NULLVP;
		return (error);
	}

	*vpp = nvp;
	vnode_create_vobject(*vpp, 0, curthread);
	return (0);
}

static int
udf_vptofh (struct vnode *vp, struct fid *fhp)
{
	struct udf_node *node;
	struct ifid *ifhp;

	node = VTON(vp);
	ifhp = (struct ifid *)fhp;
	ifhp->ifid_len = sizeof(struct ifid);
	ifhp->ifid_ino = node->hash_id;

	return (0);
}

static int
udf_find_partmaps(struct udf_mnt *udfmp, struct logvol_desc *lvd)
{
	union udf_pmap *pmap;
	struct part_map_spare *pms;
	struct regid *pmap_id;
	struct buf *bp;
	unsigned char regid_id[UDF_REGID_ID_SIZE + 1];
	int i, ptype, psize, error;

	for (i = 0; i < le32toh(lvd->n_pm); i++) {
		pmap = (union udf_pmap *)&lvd->maps[i * UDF_PMAP_SIZE];
		ptype = pmap->data[0];
		psize = pmap->data[1];
		if (((ptype != 1) && (ptype != 2)) ||
		    ((psize != UDF_PMAP_SIZE) && (psize != 6))) {
			printf("Invalid partition map found\n");
			return (1);
		}

		if (ptype == 1) {
			/* Type 1 map.  We don't care */
			continue;
		}

		/* Type 2 map.  Gotta find out the details */
		pmap_id = (struct regid *)&pmap->data[4];
		bzero(&regid_id[0], UDF_REGID_ID_SIZE);
		bcopy(&pmap_id->id[0], &regid_id[0], UDF_REGID_ID_SIZE);

		if (bcmp(&regid_id[0], "*UDF Sparable Partition",
		    UDF_REGID_ID_SIZE)) {
			printf("Unsupported partition map: %s\n", &regid_id[0]);
			return (1);
		}

		pms = &pmap->pms;
		MALLOC(udfmp->s_table, struct udf_sparing_table *,
		    le32toh(pms->st_size), M_UDFMOUNT, M_NOWAIT | M_ZERO);
		if (udfmp->s_table == NULL)
			return (ENOMEM);

		/* Calculate the number of sectors per packet. */
		/* XXX Logical or physical? */
		udfmp->p_sectors = le16toh(pms->packet_len) / udfmp->bsize;

		/*
		 * XXX If reading the first Sparing Table fails, should look
		 * for another table.
		 */
		if ((error = udf_readlblks(udfmp, le32toh(pms->st_loc[0]),
					   le32toh(pms->st_size), &bp)) != 0) {
			if (bp != NULL)
				brelse(bp);
			printf("Failed to read Sparing Table at sector %d\n",
			    le32toh(pms->st_loc[0]));
			return (error);
		}
		bcopy(bp->b_data, udfmp->s_table, le32toh(pms->st_size));
		brelse(bp);

		if (udf_checktag(&udfmp->s_table->tag, 0)) {
			printf("Invalid sparing table found\n");
			return (EINVAL);
		}

		/* See how many valid entries there are here.  The list is
		 * supposed to be sorted. 0xfffffff0 and higher are not valid
		 */
		for (i = 0; i < le16toh(udfmp->s_table->rt_l); i++) {
			udfmp->s_table_entries = i;
			if (le32toh(udfmp->s_table->entries[i].org) >=
			    0xfffffff0)
				break;
		}
	}

	return (0);
}
