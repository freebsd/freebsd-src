/* $FreeBSD$ */
/*	$NetBSD: msdosfsmount.h,v 1.17 1997/11/17 15:37:07 ws Exp $	*/

/*-
 * Copyright (C) 1994, 1995, 1997 Wolfgang Solfrank.
 * Copyright (C) 1994, 1995, 1997 TooLs GmbH.
 * All rights reserved.
 * Original code by Paul Popelka (paulp@uts.amdahl.com) (see below).
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Written by Paul Popelka (paulp@uts.amdahl.com)
 *
 * You can do anything you want with this software, just don't say you wrote
 * it, and don't remove this notice.
 *
 * This software is provided "as is".
 *
 * The author supplies this software to be publicly redistributed on the
 * understanding that the author is not responsible for the correct
 * functioning of this software in any circumstances and is not liable for
 * any damages caused by this software.
 *
 * October 1992
 */

#ifndef _MSDOSFS_MSDOSFSMOUNT_H_
#define	_MSDOSFS_MSDOSFSMOUNT_H_

#ifdef _KERNEL

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_MSDOSFSMNT);
#endif

/*
 * Layout of the mount control block for a msdos file system.
 */
struct msdosfsmount {
	struct mount *pm_mountp;/* vfs mount struct for this fs */
	dev_t pm_dev;		/* block special device mounted */
	uid_t pm_uid;		/* uid to set as owner of the files */
	gid_t pm_gid;		/* gid to set as owner of the files */
	mode_t pm_mask;		/* mask to and with file protection bits 
				   for files */
	mode_t pm_dirmask;	/* mask to and with file protection bits
				   for directories */
	struct vnode *pm_devvp;	/* vnode for block device mntd */
	struct bpb50 pm_bpb;	/* BIOS parameter blk for this fs */
	u_long pm_BlkPerSec;	/* How many DEV_BSIZE blocks fit inside a physical sector */
	u_long pm_FATsecs;	/* actual number of fat sectors */
	u_long pm_fatblk;	/* block # of first FAT */
	u_long pm_rootdirblk;	/* block # (cluster # for FAT32) of root directory number */
	u_long pm_rootdirsize;	/* size in blocks (not clusters) */
	u_long pm_firstcluster;	/* block number of first cluster */
	u_long pm_maxcluster;	/* maximum cluster number */
	u_long pm_freeclustercount;	/* number of free clusters */
	u_long pm_cnshift;	/* shift file offset right this amount to get a cluster number */
	u_long pm_crbomask;	/* and a file offset with this mask to get cluster rel offset */
	u_long pm_bnshift;	/* shift file offset right this amount to get a block number */
	u_long pm_bpcluster;	/* bytes per cluster */
	u_long pm_fmod;		/* ~0 if fs is modified, this can rollover to 0	*/
	u_long pm_fatblocksize;	/* size of fat blocks in bytes */
	u_long pm_fatblocksec;	/* size of fat blocks in sectors */
	u_long pm_fatsize;	/* size of fat in bytes */
	u_int32_t pm_fatmask;	/* mask to use for fat numbers */
	u_long pm_fsinfo;	/* fsinfo block number */
	u_long pm_nxtfree;	/* next free cluster in fsinfo block */
	u_int pm_fatmult;	/* these 2 values are used in fat */
	u_int pm_fatdiv;	/*	offset computation */
	u_int pm_curfat;	/* current fat for FAT32 (0 otherwise) */
	u_int *pm_inusemap;	/* ptr to bitmap of in-use clusters */
	u_int pm_flags;		/* see below */
	struct netexport pm_export;	/* export information */
	u_int16_t pm_u2w[128];  /* Local->Unicode table */
	u_int8_t  pm_ul[128];   /* Local upper->lower table */
	u_int8_t  pm_lu[128];   /* Local lower->upper table */
	u_int8_t  pm_d2u[128];  /* DOS->local table */
	u_int8_t  pm_u2d[128];  /* Local->DOS table */
};
/* Byte offset in FAT on filesystem pmp, cluster cn */
#define	FATOFS(pmp, cn)	((cn) * (pmp)->pm_fatmult / (pmp)->pm_fatdiv)


#define	VFSTOMSDOSFS(mp)	((struct msdosfsmount *)mp->mnt_data)

/* Number of bits in one pm_inusemap item: */
#define	N_INUSEBITS	(8 * sizeof(u_int))

/*
 * Shorthand for fields in the bpb contained in the msdosfsmount structure.
 */
#define	pm_BytesPerSec	pm_bpb.bpbBytesPerSec
#define	pm_ResSectors	pm_bpb.bpbResSectors
#define	pm_FATs		pm_bpb.bpbFATs
#define	pm_RootDirEnts	pm_bpb.bpbRootDirEnts
#define	pm_Sectors	pm_bpb.bpbSectors
#define	pm_Media	pm_bpb.bpbMedia
#define	pm_SecPerTrack	pm_bpb.bpbSecPerTrack
#define	pm_Heads	pm_bpb.bpbHeads
#define	pm_HiddenSects	pm_bpb.bpbHiddenSecs
#define	pm_HugeSectors	pm_bpb.bpbHugeSectors

/*
 * Convert pointer to buffer -> pointer to direntry
 */
#define	bptoep(pmp, bp, dirofs) \
	((struct direntry *)(((bp)->b_data)	\
	 + ((dirofs) & (pmp)->pm_crbomask)))

/*
 * Convert block number to cluster number
 */
#define	de_bn2cn(pmp, bn) \
	((bn) >> ((pmp)->pm_cnshift - (pmp)->pm_bnshift))

/*
 * Convert cluster number to block number
 */
#define	de_cn2bn(pmp, cn) \
	((cn) << ((pmp)->pm_cnshift - (pmp)->pm_bnshift))

/*
 * Convert file offset to cluster number
 */
#define de_cluster(pmp, off) \
	((off) >> (pmp)->pm_cnshift)

/*
 * Clusters required to hold size bytes
 */
#define	de_clcount(pmp, size) \
	(((size) + (pmp)->pm_bpcluster - 1) >> (pmp)->pm_cnshift)

/*
 * Convert file offset to block number
 */
#define de_blk(pmp, off) \
	(de_cn2bn(pmp, de_cluster((pmp), (off))))

/*
 * Convert cluster number to file offset
 */
#define	de_cn2off(pmp, cn) \
	((cn) << (pmp)->pm_cnshift)

/*
 * Convert block number to file offset
 */
#define	de_bn2off(pmp, bn) \
	((bn) << (pmp)->pm_bnshift)
/*
 * Map a cluster number into a filesystem relative block number.
 */
#define	cntobn(pmp, cn) \
	(de_cn2bn((pmp), (cn)-CLUST_FIRST) + (pmp)->pm_firstcluster)

/*
 * Calculate block number for directory entry in root dir, offset dirofs
 */
#define	roottobn(pmp, dirofs) \
	(de_blk((pmp), (dirofs)) + (pmp)->pm_rootdirblk)

/*
 * Calculate block number for directory entry at cluster dirclu, offset
 * dirofs
 */
#define	detobn(pmp, dirclu, dirofs) \
	((dirclu) == MSDOSFSROOT \
	 ? roottobn((pmp), (dirofs)) \
	 : cntobn((pmp), (dirclu)))

/*
 * Calculate fsinfo block size
 */
#define	fsi_size(pmp) \
	(1024 << ((pmp)->pm_BlkPerSec >> 2))

int msdosfs_init __P((struct vfsconf *vfsp));
int msdosfs_uninit __P((struct vfsconf *vfsp));
int msdosfs_mountroot __P((void));

#endif /* _KERNEL */

/*
 *  Arguments to mount MSDOS filesystems.
 */
struct msdosfs_args {
	char	*fspec;		/* blocks special holding the fs to mount */
	struct	export_args export;	/* network export information */
	uid_t	uid;		/* uid that owns msdosfs files */
	gid_t	gid;		/* gid that owns msdosfs files */
	mode_t	mask;		/* file mask to be applied for msdosfs perms */
	mode_t	dirmask;	/* dir  mask to be applied for msdosfs perms */
	int	flags;		/* see below */
	int magic;		/* version number */
	u_int16_t u2w[128];     /* Local->Unicode table */
	u_int8_t  ul[128];      /* Local upper->lower table */
	u_int8_t  lu[128];      /* Local lower->upper table */
	u_int8_t  d2u[128];     /* DOS->local table */
	u_int8_t  u2d[128];     /* Local->DOS table */
};

/*
 * Msdosfs mount options:
 */
#define	MSDOSFSMNT_SHORTNAME	1	/* Force old DOS short names only */
#define	MSDOSFSMNT_LONGNAME	2	/* Force Win'95 long names */
#define	MSDOSFSMNT_NOWIN95	4	/* Completely ignore Win95 entries */
#ifndef __FreeBSD__
#define	MSDOSFSMNT_GEMDOSFS	8	/* This is a gemdos-flavour */
#endif
#define MSDOSFSMNT_U2WTABLE     0x10    /* Local->Unicode and local<->DOS   */
					/* tables loaded                    */
#define MSDOSFSMNT_ULTABLE      0x20    /* Local upper<->lower table loaded */
/* All flags above: */
#define	MSDOSFSMNT_MNTOPT \
	(MSDOSFSMNT_SHORTNAME|MSDOSFSMNT_LONGNAME|MSDOSFSMNT_NOWIN95 \
	 /*|MSDOSFSMNT_GEMDOSFS*/|MSDOSFSMNT_U2WTABLE|MSDOSFSMNT_ULTABLE)
#define	MSDOSFSMNT_RONLY	0x80000000	/* mounted read-only	*/
#define	MSDOSFSMNT_WAITONFAT	0x40000000	/* mounted synchronous	*/
#define	MSDOSFS_FATMIRROR	0x20000000	/* FAT is mirrored */

#define MSDOSFS_ARGSMAGIC	0xe4eff300

#endif /* !_MSDOSFS_MSDOSFSMOUNT_H_ */
