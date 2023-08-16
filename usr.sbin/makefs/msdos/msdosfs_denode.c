/*	$NetBSD: msdosfs_denode.c,v 1.7 2015/03/29 05:52:59 agc Exp $	*/

/*-
 * SPDX-License-Identifier: BSD-4-Clause
 *
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
/*-
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

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/errno.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <util.h>

#include <fs/msdosfs/bpb.h>
#include "msdos/denode.h"
#include <fs/msdosfs/fat.h>
#include <fs/msdosfs/msdosfsmount.h>

#include "makefs.h"
#include "msdos.h"


/*
 * If deget() succeeds it returns with the gotten denode locked().
 *
 * pmp	     - address of msdosfsmount structure of the filesystem containing
 *	       the denode of interest.  The pm_dev field and the address of
 *	       the msdosfsmount structure are used.
 * dirclust  - which cluster bp contains, if dirclust is 0 (root directory)
 *	       diroffset is relative to the beginning of the root directory,
 *	       otherwise it is cluster relative.
 * diroffset - offset past begin of cluster of denode we want
 * depp	     - returns the address of the gotten denode.
 */
int
deget(struct msdosfsmount *pmp, u_long dirclust, u_long diroffset,
    int lkflags __unused, struct denode **depp)
{
	int error;
	uint64_t inode;
	struct direntry *direntptr;
	struct denode *ldep;
	struct m_buf *bp;

	MSDOSFS_DPRINTF(("deget(pmp %p, dirclust %lu, diroffset %lx, depp %p)\n",
	    pmp, dirclust, diroffset, depp));

	/*
	 * On FAT32 filesystems, root is a (more or less) normal
	 * directory
	 */
	if (FAT32(pmp) && dirclust == MSDOSFSROOT)
		dirclust = pmp->pm_rootdirblk;

	inode = (uint64_t)pmp->pm_bpcluster * dirclust + diroffset;

	ldep = ecalloc(1, sizeof(*ldep));
	ldep->de_vnode = NULL;
	ldep->de_flag = 0;
	ldep->de_dirclust = dirclust;
	ldep->de_diroffset = diroffset;
	ldep->de_inode = inode;
	ldep->de_pmp = pmp;
	ldep->de_refcnt = 1;
	fc_purge(ldep, 0);	/* init the FAT cache for this denode */
	/*
	 * Copy the directory entry into the denode area of the vnode.
	 */
	if ((dirclust == MSDOSFSROOT
	     || (FAT32(pmp) && dirclust == pmp->pm_rootdirblk))
	    && diroffset == MSDOSFSROOT_OFS) {
		/*
		 * Directory entry for the root directory. There isn't one,
		 * so we manufacture one. We should probably rummage
		 * through the root directory and find a label entry (if it
		 * exists), and then use the time and date from that entry
		 * as the time and date for the root denode.
		 */
		ldep->de_vnode = (struct vnode *)-1;

		ldep->de_Attributes = ATTR_DIRECTORY;
		ldep->de_LowerCase = 0;
		if (FAT32(pmp))
			ldep->de_StartCluster = pmp->pm_rootdirblk;
			/* de_FileSize will be filled in further down */
		else {
			ldep->de_StartCluster = MSDOSFSROOT;
			ldep->de_FileSize = pmp->pm_rootdirsize * DEV_BSIZE;
		}
		/*
		 * fill in time and date so that dos2unixtime() doesn't
		 * spit up when called from msdosfs_getattr() with root
		 * denode
		 */
		ldep->de_CHun = 0;
		ldep->de_CTime = 0x0000;	/* 00:00:00	 */
		ldep->de_CDate = (0 << DD_YEAR_SHIFT) | (1 << DD_MONTH_SHIFT)
		    | (1 << DD_DAY_SHIFT);
		/* Jan 1, 1980	 */
		ldep->de_ADate = ldep->de_CDate;
		ldep->de_MTime = ldep->de_CTime;
		ldep->de_MDate = ldep->de_CDate;
		/* leave the other fields as garbage */
	} else {
		error = m_readep(pmp, dirclust, diroffset, &bp, &direntptr);
		if (error) {
			ldep->de_Name[0] = SLOT_DELETED;

			*depp = NULL;
			return (error);
		}
		(void)DE_INTERNALIZE(ldep, direntptr);
		brelse(bp);
	}

	/*
	 * Fill in a few fields of the vnode and finish filling in the
	 * denode.  Then return the address of the found denode.
	 */
	if (ldep->de_Attributes & ATTR_DIRECTORY) {
		/*
		 * Since DOS directory entries that describe directories
		 * have 0 in the filesize field, we take this opportunity
		 * to find out the length of the directory and plug it into
		 * the denode structure.
		 */
		u_long size;

		/*
		 * XXX it sometimes happens that the "." entry has cluster
		 * number 0 when it shouldn't.  Use the actual cluster number
		 * instead of what is written in directory entry.
		 */
		if (diroffset == 0 && ldep->de_StartCluster != dirclust) {
			MSDOSFS_DPRINTF(("deget(): \".\" entry at clust %lu != %lu\n",
			    dirclust, ldep->de_StartCluster));

			ldep->de_StartCluster = dirclust;
		}

		if (ldep->de_StartCluster != MSDOSFSROOT) {
			error = pcbmap(ldep, 0xffff, 0, &size, 0);
			if (error == E2BIG) {
				ldep->de_FileSize = de_cn2off(pmp, size);
				error = 0;
			} else {
				MSDOSFS_DPRINTF(("deget(): pcbmap returned %d\n",
				    error));
			}
		}
	}
	*depp = ldep;
	return (0);
}

/*
 * Truncate the file described by dep to the length specified by length.
 */
int
detrunc(struct denode *dep, u_long length, int flags, struct ucred *cred)
{
	int error;
	u_long eofentry;
	u_long chaintofree;
	daddr_t bn;
	int boff;
	int isadir = dep->de_Attributes & ATTR_DIRECTORY;
	struct m_buf *bp;
	struct msdosfsmount *pmp = dep->de_pmp;

	MSDOSFS_DPRINTF(("detrunc(): file %s, length %lu, flags %x\n",
	    dep->de_Name, length, flags));

	/*
	 * Disallow attempts to truncate the root directory since it is of
	 * fixed size.  That's just the way dos filesystems are.  We use
	 * the VROOT bit in the vnode because checking for the directory
	 * bit and a startcluster of 0 in the denode is not adequate to
	 * recognize the root directory at this point in a file or
	 * directory's life.
	 */
	if (dep->de_vnode != NULL && !FAT32(pmp)) {
		MSDOSFS_DPRINTF(("detrunc(): can't truncate root directory, "
		    "clust %ld, offset %ld\n",
		    dep->de_dirclust, dep->de_diroffset));

		return (EINVAL);
	}

	if (dep->de_FileSize < length)
		return deextend(dep, length, cred);

	/*
	 * If the desired length is 0 then remember the starting cluster of
	 * the file and set the StartCluster field in the directory entry
	 * to 0.  If the desired length is not zero, then get the number of
	 * the last cluster in the shortened file.  Then get the number of
	 * the first cluster in the part of the file that is to be freed.
	 * Then set the next cluster pointer in the last cluster of the
	 * file to CLUST_EOFE.
	 */
	if (length == 0) {
		chaintofree = dep->de_StartCluster;
		dep->de_StartCluster = 0;
		eofentry = ~0ul;
	} else {
		error = pcbmap(dep, de_clcount(pmp, length) - 1, 0,
		    &eofentry, 0);
		if (error) {
			MSDOSFS_DPRINTF(("detrunc(): pcbmap fails %d\n",
			    error));
			return (error);
		}
	}

	fc_purge(dep, de_clcount(pmp, length));

	/*
	 * If the new length is not a multiple of the cluster size then we
	 * must zero the tail end of the new last cluster in case it
	 * becomes part of the file again because of a seek.
	 */
	if ((boff = length & pmp->pm_crbomask) != 0) {
		if (isadir) {
			bn = cntobn(pmp, eofentry);
			error = bread((void *)pmp->pm_devvp, bn,
			    pmp->pm_bpcluster, 0, &bp);
			if (error) {
				brelse(bp);
				MSDOSFS_DPRINTF(("detrunc(): bread fails %d\n",
				    error));

				return (error);
			}
			memset(bp->b_data + boff, 0, pmp->pm_bpcluster - boff);
			bwrite(bp);
		}
	}

	/*
	 * Write out the updated directory entry.  Even if the update fails
	 * we free the trailing clusters.
	 */
	dep->de_FileSize = length;
	if (!isadir)
		dep->de_flag |= DE_UPDATE|DE_MODIFIED;
	MSDOSFS_DPRINTF(("detrunc(): eofentry %lu\n", eofentry));

	/*
	 * If we need to break the cluster chain for the file then do it
	 * now.
	 */
	if (eofentry != ~0ul) {
		error = fatentry(FAT_GET_AND_SET, pmp, eofentry,
				 &chaintofree, CLUST_EOFE);
		if (error) {
			MSDOSFS_DPRINTF(("detrunc(): fatentry errors %d\n",
			    error));
			return (error);
		}
		fc_setcache(dep, FC_LASTFC, de_cluster(pmp, length - 1),
		    eofentry);
	}

	/*
	 * Now free the clusters removed from the file because of the
	 * truncation.
	 */
	if (chaintofree != 0 && !MSDOSFSEOF(pmp, chaintofree))
		freeclusterchain(pmp, chaintofree);

	return (0);
}

/*
 * Extend the file described by dep to length specified by length.
 */
int
deextend(struct denode *dep, u_long length, struct ucred *cred)
{
	struct msdosfsmount *pmp = dep->de_pmp;
	u_long count;
	int error;

	/*
	 * The root of a DOS filesystem cannot be extended.
	 */
	if (dep->de_vnode != NULL && !FAT32(pmp))
		return (EINVAL);

	/*
	 * Directories cannot be extended.
	 */
	if (dep->de_Attributes & ATTR_DIRECTORY)
		return (EISDIR);

	if (length <= dep->de_FileSize)
		return (E2BIG);

	/*
	 * Compute the number of clusters to allocate.
	 */
	count = de_clcount(pmp, length) - de_clcount(pmp, dep->de_FileSize);
	if (count > 0) {
		if (count > pmp->pm_freeclustercount)
			return (ENOSPC);
		error = m_extendfile(dep, count, NULL, NULL, DE_CLEAR);
		if (error) {
			/* truncate the added clusters away again */
			(void) detrunc(dep, dep->de_FileSize, 0, cred);
			return (error);
		}
	}

	/*
	 * Zero extend file range; ubc_zerorange() uses ubc_alloc() and a
	 * memset(); we set the write size so ubc won't read in file data that
	 * is zero'd later.
	 */
	dep->de_FileSize = length;
	dep->de_flag |= DE_UPDATE | DE_MODIFIED;
	return 0;
}
