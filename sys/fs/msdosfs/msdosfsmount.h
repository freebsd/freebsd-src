/*	$Id: msdosfsmount.h,v 1.11 1997/03/03 17:36:11 bde Exp $ */
/*	$NetBSD: msdosfsmount.h,v 1.7 1994/08/21 18:44:17 ws Exp $	*/

/*-
 * Copyright (C) 1994 Wolfgang Solfrank.
 * Copyright (C) 1994 TooLs GmbH.
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

#ifdef KERNEL

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_MSDOSFSMNT);
#endif

/*
 * Layout of the mount control block for a msdos file system.
 */
struct msdosfsmount {
	struct mount *pm_mountp;/* vfs mount struct for this fs */
	dev_t pm_dev;		/* block special device mounted */
	uid_t pm_mounter;	/* uid of the user who mounted the FS */
	uid_t pm_uid;		/* uid to set as owner of the files */
	gid_t pm_gid;		/* gid to set as owner of the files */
	mode_t pm_mask;		/* mask to and with file protection bits */
	struct vnode *pm_devvp;	/* vnode for block device mntd */
	struct bpb50 pm_bpb;	/* BIOS parameter blk for this fs */
	u_long pm_fatblk;	/* block # of first FAT */
	u_long pm_rootdirblk;	/* block # of root directory */
	u_long pm_rootdirsize;	/* size in blocks (not clusters) */
	u_long pm_firstcluster;	/* block number of first cluster */
	u_long pm_nmbrofclusters;	/* # of clusters in filesystem */
	u_long pm_maxcluster;	/* maximum cluster number */
	u_long pm_freeclustercount;	/* number of free clusters */
	u_long pm_bnshift;	/* shift file offset right this amount to get a block number */
	u_long pm_brbomask;	/* and a file offset with this mask to get block rel offset */
	u_long pm_cnshift;	/* shift file offset right this amount to get a cluster number */
	u_long pm_crbomask;	/* and a file offset with this mask to get cluster rel offset */
	u_long pm_bpcluster;	/* bytes per cluster */
	u_long pm_depclust;	/* directory entries per cluster */
	u_long pm_fmod;		/* ~0 if fs is modified, this can rollover to 0	*/
	u_long pm_fatblocksize;	/* size of fat blocks in bytes */
	u_long pm_fatblocksec;	/* size of fat blocks in sectors */
	u_long pm_fatsize;	/* size of fat in bytes */
	u_int *pm_inusemap;	/* ptr to bitmap of in-use clusters */
	char pm_ronly;		/* read only if non-zero */
	char pm_waitonfat;	/* wait for writes of the fat to complete, when 0 use bdwrite, else use bwrite */
	struct netexport pm_export;	/* export information */
};

/* Number of bits in one pm_inusemap item: */
#define	N_INUSEBITS	(8 * sizeof(u_int))

/*
 * How to compute pm_cnshift and pm_crbomask.
 *
 * pm_crbomask = (pm_SectPerClust * pm_BytesPerSect) - 1
 * if (bytesperclust == * 0)
 * 	return EBADBLKSZ;
 * bit = 1;
 * for (i = 0; i < 32; i++) {
 *	if (bit & bytesperclust) {
 *		if (bit ^ bytesperclust)
 *			return EBADBLKSZ;
 *		pm_cnshift = * i;
 *		break;
 *	}
 *	bit <<= 1;
 * }
 */

/*
 * Shorthand for fields in the bpb contained in the msdosfsmount structure.
 */
#define	pm_BytesPerSec	pm_bpb.bpbBytesPerSec
#define	pm_SectPerClust	pm_bpb.bpbSecPerClust
#define	pm_ResSectors	pm_bpb.bpbResSectors
#define	pm_FATs		pm_bpb.bpbFATs
#define	pm_RootDirEnts	pm_bpb.bpbRootDirEnts
#define	pm_Sectors	pm_bpb.bpbSectors
#define	pm_Media	pm_bpb.bpbMedia
#define	pm_FATsecs	pm_bpb.bpbFATsecs
#define	pm_SecPerTrack	pm_bpb.bpbSecPerTrack
#define	pm_Heads	pm_bpb.bpbHeads
#define	pm_HiddenSects	pm_bpb.bpbHiddenSecs
#define	pm_HugeSectors	pm_bpb.bpbHugeSectors

/*
 * Map a cluster number into a filesystem relative block number.
 */
#define	cntobn(pmp, cn) \
	((((cn)-CLUST_FIRST) * (pmp)->pm_SectPerClust) + (pmp)->pm_firstcluster)

/*
 * Map a filesystem relative block number back into a cluster number.
 */
#define	bntocn(pmp, bn) \
	((((bn) - pmp->pm_firstcluster)/ (pmp)->pm_SectPerClust) + CLUST_FIRST)

/*
 * Calculate block number for directory entry in root dir, offset dirofs
 */
#define	roottobn(pmp, dirofs) \
	(((dirofs) / (pmp)->pm_depclust) * (pmp)->pm_SectPerClust \
	+ (pmp)->pm_rootdirblk)

/*
 * Calculate block number for directory entry at cluster dirclu, offset
 * dirofs
 */
#define	detobn(pmp, dirclu, dirofs) \
	((dirclu) == MSDOSFSROOT \
	 ? roottobn((pmp), (dirofs)) \
	 : cntobn((pmp), (dirclu)))

/*
 * Convert pointer to buffer -> pointer to direntry
 */
#define	bptoep(pmp, bp, dirofs) \
	((struct direntry *)((bp)->b_data)	\
	 + (dirofs) % (pmp)->pm_depclust)


/*
 * Convert filesize to block number
 */
#define de_blk(pmp, off) \
	((off) >> (pmp)->pm_cnshift)

/*
 * Clusters required to hold size bytes
 */
#define	de_clcount(pmp, size) \
	(((size) + (pmp)->pm_bpcluster - 1) >> (pmp)->pm_cnshift)

int msdosfs_init __P((struct vfsconf *vfsp));

#endif /* KERNEL */

/*
 *  Arguments to mount MSDOS filesystems.
 */
struct msdosfs_args {
	char	*fspec;		/* blocks special holding the fs to mount */
	struct	export_args export;	/* network export information */
	uid_t	uid;		/* uid that owns msdosfs files */
	gid_t	gid;		/* gid that owns msdosfs files */
	mode_t	mask;		/* mask to be applied for msdosfs perms */
};

#endif /* !_MSDOSFS_MSDOSFSMOUNT_H_ */
