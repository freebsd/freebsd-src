/*	$Id: denode.h,v 1.8 1995/11/09 08:17:21 bde Exp $ */
/*	$NetBSD: denode.h,v 1.8 1994/08/21 18:43:49 ws Exp $	*/

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

/*
 * This is the pc filesystem specific portion of the vnode structure.
 *
 * To describe a file uniquely the de_dirclust, de_diroffset, and
 * de_StartCluster fields are used.
 *
 * de_dirclust contains the cluster number of the directory cluster
 *	containing the entry for a file or directory.
 * de_diroffset is the index into the cluster for the entry describing
 *	a file or directory.
 * de_StartCluster is the number of the first cluster of the file or directory.
 *
 * Now to describe the quirks of the pc filesystem.
 * - Clusters 0 and 1 are reserved.
 * - The first allocatable cluster is 2.
 * - The root directory is of fixed size and all blocks that make it up
 *   are contiguous.
 * - Cluster 0 refers to the root directory when it is found in the
 *   startcluster field of a directory entry that points to another directory.
 * - Cluster 0 implies a 0 length file when found in the start cluster field
 *   of a directory entry that points to a file.
 * - You can't use the cluster number 0 to derive the address of the root
 *   directory.
 * - Multiple directory entries can point to a directory. The entry in the
 *   parent directory points to a child directory.  Any directories in the
 *   child directory contain a ".." entry that points back to the parent.
 *   The child directory itself contains a "." entry that points to itself.
 * - The root directory does not contain a "." or ".." entry.
 * - Directory entries for directories are never changed once they are created
 *   (except when removed).  The size stays 0, and the last modification time
 *   is never changed.  This is because so many directory entries can point to
 *   the physical clusters that make up a directory.  It would lead to an
 *   update nightmare.
 * - The length field in a directory entry pointing to a directory contains 0
 *   (always).  The only way to find the end of a directory is to follow the
 *   cluster chain until the "last cluster" marker is found.
 *
 * My extensions to make this house of cards work.  These apply only to the in
 * memory copy of the directory entry.
 * - A reference count for each denode will be kept since dos doesn't keep such
 *   things.
 */

/*
 * Internal pseudo-offset for (nonexistent) directory entry for the root
 * dir in the root dir
 */
#define	MSDOSFSROOT_OFS	0x1fffffff

/*
 * The fat cache structure. fc_fsrcn is the filesystem relative cluster
 * number that corresponds to the file relative cluster number in this
 * structure (fc_frcn).
 */
struct fatcache {
	u_short fc_frcn;	/* file relative cluster number */
	u_short fc_fsrcn;	/* filesystem relative cluster number */
};

/*
 * The fat entry cache as it stands helps make extending files a "quick"
 * operation by avoiding having to scan the fat to discover the last
 * cluster of the file. The cache also helps sequential reads by
 * remembering the last cluster read from the file.  This also prevents us
 * from having to rescan the fat to find the next cluster to read.  This
 * cache is probably pretty worthless if a file is opened by multiple
 * processes.
 */
#define	FC_SIZE		2	/* number of entries in the cache */
#define	FC_LASTMAP	0	/* entry the last call to pcbmap() resolved
				 * to */
#define	FC_LASTFC	1	/* entry for the last cluster in the file */

#define	FCE_EMPTY	0xffff	/* doesn't represent an actual cluster # */

/*
 * Set a slot in the fat cache.
 */
#define	fc_setcache(dep, slot, frcn, fsrcn) \
	(dep)->de_fc[slot].fc_frcn = frcn; \
	(dep)->de_fc[slot].fc_fsrcn = fsrcn;

/*
 * This is the in memory variant of a dos directory entry.  It is usually
 * contained within a vnode.
 */
struct denode {
	struct denode *de_next;	/* Hash chain forward */
	struct denode **de_prev; /* Hash chain back */
	struct vnode *de_vnode;	/* addr of vnode we are part of */
	struct vnode *de_devvp;	/* vnode of blk dev we live on */
	u_long de_flag;		/* flag bits */
	dev_t de_dev;		/* device where direntry lives */
	u_long de_dirclust;	/* cluster of the directory file containing this entry */
	u_long de_diroffset;	/* ordinal of this entry in the directory */
	u_long de_fndclust;	/* cluster of found dir entry */
	u_long de_fndoffset;	/* offset of found dir entry */
	long de_refcnt;		/* reference count */
	struct msdosfsmount *de_pmp;	/* addr of our mount struct */
	struct lockf *de_lockf;	/* byte level lock list */
	pid_t de_lockholder;	/* current lock holder */
	pid_t de_lockwaiter;	/* lock wanter */
	/* the next two fields must be contiguous in memory... */
	u_char de_Name[8];	/* name, from directory entry */
	u_char de_Extension[3];	/* extension, from directory entry */
	u_char de_Attributes;	/* attributes, from directory entry */
	u_short de_Time;	/* creation time */
	u_short de_Date;	/* creation date */
	u_short de_StartCluster; /* starting cluster of file */
	u_long de_FileSize;	/* size of file in bytes */
	struct fatcache de_fc[FC_SIZE];	/* fat cache */
	u_quad_t de_modrev;	/* Revision level for lease. */
};

/*
 * Values for the de_flag field of the denode.
 */
#define	DE_LOCKED	0x0001	/* directory entry is locked */
#define	DE_WANTED	0x0002	/* someone wants this de */
#define	DE_UPDATE	0x0004	/* modification time update request */
#define	DE_MODIFIED	0x0080	/* denode has been modified, but DE_UPDATE
				 * isn't set */

/*
 * Transfer directory entries between internal and external form.
 * dep is a struct denode * (internal form),
 * dp is a struct direntry * (external form).
 */
#define DE_INTERNALIZE(dep, dp)			\
	(bcopy((dp)->deName, (dep)->de_Name, 11),	\
	 (dep)->de_Attributes = (dp)->deAttributes,	\
	 (dep)->de_Time = getushort((dp)->deTime),	\
	 (dep)->de_Date = getushort((dp)->deDate),	\
	 (dep)->de_StartCluster = getushort((dp)->deStartCluster), \
	 (dep)->de_FileSize = getulong((dp)->deFileSize))

#define DE_EXTERNALIZE(dp, dep)				\
	(bcopy((dep)->de_Name, (dp)->deName, 11),	\
	 bzero((dp)->deReserved, 10),                   \
	 (dp)->deAttributes = (dep)->de_Attributes,	\
	 putushort((dp)->deTime, (dep)->de_Time),	\
	 putushort((dp)->deDate, (dep)->de_Date),	\
	 putushort((dp)->deStartCluster, (dep)->de_StartCluster), \
	 putulong((dp)->deFileSize, (dep)->de_FileSize))

#define	de_forw		de_chain[0]
#define	de_back		de_chain[1]

#ifdef KERNEL

#define	VTODE(vp)	((struct denode *)(vp)->v_data)
#define	DETOV(de)	((de)->de_vnode)

#define	DE_TIMES(dep, t) \
	if ((dep)->de_flag & DE_UPDATE) { \
		if (!((dep)->de_Attributes & ATTR_DIRECTORY)) { \
			struct timespec DE_TIMES_ts; \
			(dep)->de_flag |= DE_MODIFIED; \
			TIMEVAL_TO_TIMESPEC((t), &DE_TIMES_ts); \
			unix2dostime(&DE_TIMES_ts, &(dep)->de_Date, \
				     &(dep)->de_Time); \
			(dep)->de_Attributes |= ATTR_ARCHIVE; \
		} \
		(dep)->de_flag &= ~DE_UPDATE; \
	}

/*
 * This overlays the fid structure (see mount.h)
 */
struct defid {
	u_short defid_len;	/* length of structure */
	u_short defid_pad;	/* force long alignment */

	u_long defid_dirclust;	/* cluster this dir entry came from */
	u_long defid_dirofs;	/* index of entry within the cluster */

	/* u_long	defid_gen;	 generation number */
};

extern vop_t **msdosfs_vnodeop_p;

int msdosfs_lookup __P((struct vop_lookup_args *));
int msdosfs_inactive __P((struct vop_inactive_args *));
int msdosfs_reclaim __P((struct vop_reclaim_args *));

/*
 * Internal service routine prototypes.
 */
int deget __P((struct msdosfsmount * pmp, u_long dirclust, u_long diroffset, struct direntry * direntptr, struct denode ** depp));
#endif	/* KERNEL */
