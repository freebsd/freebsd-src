/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1982, 1986, 1989, 1993
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
 * 3. Neither the name of the University nor the names of its contributors
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
 */

#include <sys/param.h>
#include <sys/endian.h>
#include <sys/limits.h>

#ifndef _KERNEL
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/errno.h>
#include <ufs/ufs/dinode.h>
#include <ufs/ffs/fs.h>

uint32_t calculate_crc32c(uint32_t, const void *, size_t);
uint32_t ffs_calc_sbhash(struct fs *);
struct malloc_type;
#define UFS_MALLOC(size, type, flags) malloc(size)
#define UFS_FREE(ptr, type) free(ptr)
#define maxphys MAXPHYS

#else /* _KERNEL */
#include <sys/systm.h>
#include <sys/gsb_crc32.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/ucred.h>
#include <sys/sysctl.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/extattr.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>
#include <ufs/ffs/ffs_extern.h>
#include <ufs/ffs/fs.h>

#define UFS_MALLOC(size, type, flags) malloc(size, type, flags)
#define UFS_FREE(ptr, type) free(ptr, type)

#endif /* _KERNEL */

/*
 * Verify an inode check-hash.
 */
int
ffs_verify_dinode_ckhash(struct fs *fs, struct ufs2_dinode *dip)
{
	uint32_t ckhash, save_ckhash;

	/*
	 * Return success if unallocated or we are not doing inode check-hash.
	 */
	if (dip->di_mode == 0 || (fs->fs_metackhash & CK_INODE) == 0)
		return (0);
	/*
	 * Exclude di_ckhash from the crc32 calculation, e.g., always use
	 * a check-hash value of zero when calculating the check-hash.
	 */
	save_ckhash = dip->di_ckhash;
	dip->di_ckhash = 0;
	ckhash = calculate_crc32c(~0L, (void *)dip, sizeof(*dip));
	dip->di_ckhash = save_ckhash;
	if (save_ckhash == ckhash)
		return (0);
	return (EINVAL);
}

/*
 * Update an inode check-hash.
 */
void
ffs_update_dinode_ckhash(struct fs *fs, struct ufs2_dinode *dip)
{

	if (dip->di_mode == 0 || (fs->fs_metackhash & CK_INODE) == 0)
		return;
	/*
	 * Exclude old di_ckhash from the crc32 calculation, e.g., always use
	 * a check-hash value of zero when calculating the new check-hash.
	 */
	dip->di_ckhash = 0;
	dip->di_ckhash = calculate_crc32c(~0L, (void *)dip, sizeof(*dip));
}

/*
 * These are the low-level functions that actually read and write
 * the superblock and its associated data.
 */
static off_t sblock_try[] = SBLOCKSEARCH;
static int readsuper(void *, struct fs **, off_t, int,
	int (*)(void *, off_t, void **, int));
static void ffs_oldfscompat_read(struct fs *, ufs2_daddr_t);
static int validate_sblock(struct fs *, int);

/*
 * Read a superblock from the devfd device.
 *
 * If an alternate superblock is specified, it is read. Otherwise the
 * set of locations given in the SBLOCKSEARCH list is searched for a
 * superblock. Memory is allocated for the superblock by the readfunc and
 * is returned. If filltype is non-NULL, additional memory is allocated
 * of type filltype and filled in with the superblock summary information.
 * All memory is freed when any error is returned.
 *
 * If a superblock is found, zero is returned. Otherwise one of the
 * following error values is returned:
 *     EIO: non-existent or truncated superblock.
 *     EIO: error reading summary information.
 *     ENOENT: no usable known superblock found.
 *     EILSEQ: filesystem with wrong byte order found.
 *     ENOMEM: failed to allocate space for the superblock.
 *     EINVAL: The previous newfs operation on this volume did not complete.
 *         The administrator must complete newfs before using this volume.
 */
int
ffs_sbget(void *devfd, struct fs **fsp, off_t sblock, int flags,
    struct malloc_type *filltype,
    int (*readfunc)(void *devfd, off_t loc, void **bufp, int size))
{
	struct fs *fs;
	struct fs_summary_info *fs_si;
	int i, error;
	uint64_t size, blks;
	uint8_t *space;
	int32_t *lp;
	char *buf;

	fs = NULL;
	*fsp = NULL;
	if (sblock != UFS_STDSB) {
		if ((error = readsuper(devfd, &fs, sblock,
		    flags | UFS_ALTSBLK, readfunc)) != 0) {
			if (fs != NULL)
				UFS_FREE(fs, filltype);
			return (error);
		}
	} else {
		for (i = 0; sblock_try[i] != -1; i++) {
			if ((error = readsuper(devfd, &fs, sblock_try[i],
			     flags, readfunc)) == 0) {
				if ((flags & UFS_NOCSUM) != 0) {
					*fsp = fs;
					return (0);
				}
				break;
			}
			if (fs != NULL) {
				UFS_FREE(fs, filltype);
				fs = NULL;
			}
			if (error == ENOENT)
				continue;
			return (error);
		}
		if (sblock_try[i] == -1)
			return (ENOENT);
	}
	/*
	 * Read in the superblock summary information.
	 */
	size = fs->fs_cssize;
	blks = howmany(size, fs->fs_fsize);
	if (fs->fs_contigsumsize > 0)
		size += fs->fs_ncg * sizeof(int32_t);
	size += fs->fs_ncg * sizeof(uint8_t);
	if ((fs_si = UFS_MALLOC(sizeof(*fs_si), filltype, M_NOWAIT)) == NULL) {
		UFS_FREE(fs, filltype);
		return (ENOMEM);
	}
	bzero(fs_si, sizeof(*fs_si));
	fs->fs_si = fs_si;
	if ((space = UFS_MALLOC(size, filltype, M_NOWAIT)) == NULL) {
		UFS_FREE(fs->fs_si, filltype);
		UFS_FREE(fs, filltype);
		return (ENOMEM);
	}
	fs->fs_csp = (struct csum *)space;
	for (i = 0; i < blks; i += fs->fs_frag) {
		size = fs->fs_bsize;
		if (i + fs->fs_frag > blks)
			size = (blks - i) * fs->fs_fsize;
		buf = NULL;
		error = (*readfunc)(devfd,
		    dbtob(fsbtodb(fs, fs->fs_csaddr + i)), (void **)&buf, size);
		if (error) {
			if (buf != NULL)
				UFS_FREE(buf, filltype);
			UFS_FREE(fs->fs_csp, filltype);
			UFS_FREE(fs->fs_si, filltype);
			UFS_FREE(fs, filltype);
			return (error);
		}
		memcpy(space, buf, size);
		UFS_FREE(buf, filltype);
		space += size;
	}
	if (fs->fs_contigsumsize > 0) {
		fs->fs_maxcluster = lp = (int32_t *)space;
		for (i = 0; i < fs->fs_ncg; i++)
			*lp++ = fs->fs_contigsumsize;
		space = (uint8_t *)lp;
	}
	size = fs->fs_ncg * sizeof(uint8_t);
	fs->fs_contigdirs = (uint8_t *)space;
	bzero(fs->fs_contigdirs, size);
	*fsp = fs;
	return (0);
}

/*
 * Try to read a superblock from the location specified by sblockloc.
 * Return zero on success or an errno on failure.
 */
static int
readsuper(void *devfd, struct fs **fsp, off_t sblockloc, int flags,
    int (*readfunc)(void *devfd, off_t loc, void **bufp, int size))
{
	struct fs *fs;
	int error, res;
	uint32_t ckhash;

	error = (*readfunc)(devfd, sblockloc, (void **)fsp, SBLOCKSIZE);
	if (error != 0)
		return (error);
	fs = *fsp;
	if (fs->fs_magic == FS_BAD_MAGIC)
		return (EINVAL);
	/*
	 * For UFS1 with a 65536 block size, the first backup superblock
	 * is at the same location as the UFS2 superblock. Since SBLOCK_UFS2
	 * is the first location checked, the first backup is the superblock
	 * that will be accessed. Here we fail the lookup so that we can
	 * retry with the correct location for the UFS1 superblock.
	 */
	if (fs->fs_magic == FS_UFS1_MAGIC && (flags & UFS_ALTSBLK) == 0 &&
	    fs->fs_bsize == SBLOCK_UFS2 && sblockloc == SBLOCK_UFS2)
		return (ENOENT);
	ffs_oldfscompat_read(fs, sblockloc);
	if ((error = validate_sblock(fs, flags)) > 0)
		return (error);
	/*
	 * If the filesystem has been run on a kernel without
	 * metadata check hashes, disable them.
	 */
	if ((fs->fs_flags & FS_METACKHASH) == 0)
		fs->fs_metackhash = 0;
	/*
	 * Clear any check-hashes that are not maintained
	 * by this kernel. Also clear any unsupported flags.
	 */
	fs->fs_metackhash &= CK_SUPPORTED;
	fs->fs_flags &= FS_SUPPORTED;
	if (fs->fs_ckhash != (ckhash = ffs_calc_sbhash(fs))) {
		if ((flags & (UFS_NOMSG | UFS_NOHASHFAIL)) ==
		    (UFS_NOMSG | UFS_NOHASHFAIL))
			return (0);
		if ((flags & UFS_NOMSG) != 0)
			return (EINTEGRITY);
#ifdef _KERNEL
		res = uprintf("Superblock check-hash failed: recorded "
		    "check-hash 0x%x != computed check-hash 0x%x%s\n",
		    fs->fs_ckhash, ckhash,
		    (flags & UFS_NOHASHFAIL) != 0 ? " (Ignored)" : "");
#else
		res = 0;
#endif
		/*
		 * Print check-hash failure if no controlling terminal
		 * in kernel or always if in user-mode (libufs).
		 */
		if (res == 0)
			printf("Superblock check-hash failed: recorded "
			    "check-hash 0x%x != computed check-hash "
			    "0x%x%s\n", fs->fs_ckhash, ckhash,
			    (flags & UFS_NOHASHFAIL) ? " (Ignored)" : "");
		if ((flags & UFS_NOHASHFAIL) != 0)
			return (0);
		return (EINTEGRITY);
	}
	/* Have to set for old filesystems that predate this field */
	fs->fs_sblockactualloc = sblockloc;
	/* Not yet any summary information */
	fs->fs_si = NULL;
	return (0);
}

/*
 * Sanity checks for loading old filesystem superblocks.
 * See ffs_oldfscompat_write below for unwound actions.
 *
 * XXX - Parts get retired eventually.
 * Unfortunately new bits get added.
 */
static void
ffs_oldfscompat_read(struct fs *fs, ufs2_daddr_t sblockloc)
{
	uint64_t maxfilesize;

	/*
	 * If not yet done, update fs_flags location and value of fs_sblockloc.
	 */
	if ((fs->fs_old_flags & FS_FLAGS_UPDATED) == 0) {
		fs->fs_flags = fs->fs_old_flags;
		fs->fs_old_flags |= FS_FLAGS_UPDATED;
		fs->fs_sblockloc = sblockloc;
	}
	switch (fs->fs_magic) {
	case FS_UFS2_MAGIC:
		/* No changes for now */
		break;

	case FS_UFS1_MAGIC:
		/*
		 * If not yet done, update UFS1 superblock with new wider fields
		 */
		if (fs->fs_maxbsize != fs->fs_bsize) {
			fs->fs_maxbsize = fs->fs_bsize;
			fs->fs_time = fs->fs_old_time;
			fs->fs_size = fs->fs_old_size;
			fs->fs_dsize = fs->fs_old_dsize;
			fs->fs_csaddr = fs->fs_old_csaddr;
			fs->fs_cstotal.cs_ndir = fs->fs_old_cstotal.cs_ndir;
			fs->fs_cstotal.cs_nbfree = fs->fs_old_cstotal.cs_nbfree;
			fs->fs_cstotal.cs_nifree = fs->fs_old_cstotal.cs_nifree;
			fs->fs_cstotal.cs_nffree = fs->fs_old_cstotal.cs_nffree;
		}
		if (fs->fs_old_inodefmt < FS_44INODEFMT) {
			fs->fs_maxfilesize = ((uint64_t)1 << 31) - 1;
			fs->fs_qbmask = ~fs->fs_bmask;
			fs->fs_qfmask = ~fs->fs_fmask;
		}
		fs->fs_save_maxfilesize = fs->fs_maxfilesize;
		maxfilesize = (uint64_t)0x80000000 * fs->fs_bsize - 1;
		if (fs->fs_maxfilesize > maxfilesize)
			fs->fs_maxfilesize = maxfilesize;
		break;
	}
	/* Compatibility for old filesystems */
	if (fs->fs_avgfilesize <= 0)
		fs->fs_avgfilesize = AVFILESIZ;
	if (fs->fs_avgfpdir <= 0)
		fs->fs_avgfpdir = AFPDIR;
}

/*
 * Unwinding superblock updates for old filesystems.
 * See ffs_oldfscompat_read above for details.
 *
 * XXX - Parts get retired eventually.
 * Unfortunately new bits get added.
 */
void
ffs_oldfscompat_write(struct fs *fs)
{

	switch (fs->fs_magic) {
	case FS_UFS1_MAGIC:
		if (fs->fs_sblockloc != SBLOCK_UFS1 &&
		    (fs->fs_old_flags & FS_FLAGS_UPDATED) == 0) {
			printf(
			"WARNING: %s: correcting fs_sblockloc from %jd to %d\n",
			    fs->fs_fsmnt, fs->fs_sblockloc, SBLOCK_UFS1);
			fs->fs_sblockloc = SBLOCK_UFS1;
		}
		/*
		 * Copy back UFS2 updated fields that UFS1 inspects.
		 */
		fs->fs_old_time = fs->fs_time;
		fs->fs_old_cstotal.cs_ndir = fs->fs_cstotal.cs_ndir;
		fs->fs_old_cstotal.cs_nbfree = fs->fs_cstotal.cs_nbfree;
		fs->fs_old_cstotal.cs_nifree = fs->fs_cstotal.cs_nifree;
		fs->fs_old_cstotal.cs_nffree = fs->fs_cstotal.cs_nffree;
		if (fs->fs_save_maxfilesize != 0)
			fs->fs_maxfilesize = fs->fs_save_maxfilesize;
		break;
	case FS_UFS2_MAGIC:
		if (fs->fs_sblockloc != SBLOCK_UFS2 &&
		    (fs->fs_old_flags & FS_FLAGS_UPDATED) == 0) {
			printf(
			"WARNING: %s: correcting fs_sblockloc from %jd to %d\n",
			    fs->fs_fsmnt, fs->fs_sblockloc, SBLOCK_UFS2);
			fs->fs_sblockloc = SBLOCK_UFS2;
		}
		break;
	}
}

/*
 * Sanity checks for loading old filesystem inodes.
 *
 * XXX - Parts get retired eventually.
 * Unfortunately new bits get added.
 */
static int prttimechgs = 0;
#ifdef _KERNEL
SYSCTL_NODE(_vfs, OID_AUTO, ffs, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "FFS filesystem");
SYSCTL_INT(_vfs_ffs, OID_AUTO, prttimechgs, CTLFLAG_RWTUN, &prttimechgs, 0,
    "print UFS1 time changes made to inodes");
#endif /* _KERNEL */
bool
ffs_oldfscompat_inode_read(struct fs *fs, union dinodep dp, time_t now)
{
	bool change;

	change = false;
	switch (fs->fs_magic) {
	case FS_UFS2_MAGIC:
		/* No changes for now */
		break;

	case FS_UFS1_MAGIC:
		/*
		 * With the change to unsigned time values in UFS1, times set
		 * before Jan 1, 1970 will appear to be in the future. Check
		 * for future times and set them to be the current time.
		 */
		if (dp.dp1->di_ctime > now) {
			if (prttimechgs)
				printf("ctime %ud changed to %ld\n",
				    dp.dp1->di_ctime, (long)now);
			dp.dp1->di_ctime = now;
			change = true;
		}
		if (dp.dp1->di_mtime > now) {
			if (prttimechgs)
				printf("mtime %ud changed to %ld\n",
				    dp.dp1->di_mtime, (long)now);
			dp.dp1->di_mtime = now;
			dp.dp1->di_ctime = now;
			change = true;
		}
		if (dp.dp1->di_atime > now) {
			if (prttimechgs)
				printf("atime %ud changed to %ld\n",
				    dp.dp1->di_atime, (long)now);
			dp.dp1->di_atime = now;
			dp.dp1->di_ctime = now;
			change = true;
		}
		break;
	}
	return (change);
}

/*
 * Verify the filesystem values.
 */
#define ILOG2(num)	(fls(num) - 1)
#ifdef STANDALONE_SMALL
#define MPRINT(...)	do { } while (0)
#else
#define MPRINT(...)	if (prtmsg) printf(__VA_ARGS__)
#endif
#define FCHK(lhs, op, rhs, fmt)						\
	if (lhs op rhs) {						\
		MPRINT("UFS%d superblock failed: %s (" #fmt ") %s %s ("	\
		    #fmt ")\n", fs->fs_magic == FS_UFS1_MAGIC ? 1 : 2,	\
		    #lhs, (intmax_t)lhs, #op, #rhs, (intmax_t)rhs);	\
		if (error < 0)						\
			return (ENOENT);				\
		if (error == 0)						\
			error = ENOENT;					\
	}
#define WCHK(lhs, op, rhs, fmt)						\
	if (lhs op rhs) {						\
		MPRINT("UFS%d superblock failed: %s (" #fmt ") %s %s ("	\
		    #fmt ")%s\n", fs->fs_magic == FS_UFS1_MAGIC ? 1 : 2,\
		    #lhs, (intmax_t)lhs, #op, #rhs, (intmax_t)rhs, wmsg);\
		if (error == 0)						\
			error = warnerr;				\
		if (warnerr == 0)					\
			lhs = rhs;					\
	}
#define FCHK2(lhs1, op1, rhs1, lhs2, op2, rhs2, fmt)			\
	if (lhs1 op1 rhs1 && lhs2 op2 rhs2) {				\
		MPRINT("UFS%d superblock failed: %s (" #fmt ") %s %s ("	\
		    #fmt ") && %s (" #fmt ") %s %s (" #fmt ")\n",	\
		    fs->fs_magic == FS_UFS1_MAGIC ? 1 : 2, #lhs1,	\
		    (intmax_t)lhs1, #op1, #rhs1, (intmax_t)rhs1, #lhs2,	\
		    (intmax_t)lhs2, #op2, #rhs2, (intmax_t)rhs2);	\
		if (error < 0)						\
			return (ENOENT);				\
		if (error == 0)						\
			error = ENOENT;					\
	}

static int
validate_sblock(struct fs *fs, int flags)
{
	uint64_t i, sectorsize;
	uint64_t maxfilesize, sizepb;
	int error, prtmsg, warnerr;
	char *wmsg;

	error = 0;
	sectorsize = dbtob(1);
	prtmsg = ((flags & UFS_NOMSG) == 0);
	warnerr = (flags & UFS_NOWARNFAIL) == UFS_NOWARNFAIL ? 0 : ENOENT;
	wmsg = warnerr ? "" : " (Ignored)";
	/*
	 * Check for endian mismatch between machine and filesystem.
	 */
	if (((fs->fs_magic != FS_UFS2_MAGIC) &&
	    (bswap32(fs->fs_magic) == FS_UFS2_MAGIC)) ||
	    ((fs->fs_magic != FS_UFS1_MAGIC) &&
	    (bswap32(fs->fs_magic) == FS_UFS1_MAGIC))) {
		MPRINT("UFS superblock failed due to endian mismatch "
		    "between machine and filesystem\n");
		return(EILSEQ);
	}
	/*
	 * If just validating for recovery, then do just the minimal
	 * checks needed for the superblock fields needed to find
	 * alternate superblocks.
	 */
	if ((flags & UFS_FSRONLY) == UFS_FSRONLY &&
	    (fs->fs_magic == FS_UFS1_MAGIC || fs->fs_magic == FS_UFS2_MAGIC)) {
		error = -1; /* fail on first error */
		if (fs->fs_magic == FS_UFS2_MAGIC) {
			FCHK(fs->fs_sblockloc, !=, SBLOCK_UFS2, %#jx);
		} else if (fs->fs_magic == FS_UFS1_MAGIC) {
			FCHK(fs->fs_sblockloc, <, 0, %jd);
			FCHK(fs->fs_sblockloc, >, SBLOCK_UFS1, %jd);
		}
		FCHK(fs->fs_frag, <, 1, %jd);
		FCHK(fs->fs_frag, >, MAXFRAG, %jd);
		FCHK(fs->fs_bsize, <, MINBSIZE, %jd);
		FCHK(fs->fs_bsize, >, MAXBSIZE, %jd);
		FCHK(fs->fs_bsize, <, roundup(sizeof(struct fs), DEV_BSIZE),
		    %jd);
		FCHK(fs->fs_fsize, <, sectorsize, %jd);
		FCHK(fs->fs_fsize * fs->fs_frag, !=, fs->fs_bsize, %jd);
		FCHK(powerof2(fs->fs_fsize), ==, 0, %jd);
		FCHK(fs->fs_sbsize, >, SBLOCKSIZE, %jd);
		FCHK(fs->fs_sbsize, <, (signed)sizeof(struct fs), %jd);
		FCHK(fs->fs_sbsize % sectorsize, !=, 0, %jd);
		FCHK(fs->fs_fpg, <, 3 * fs->fs_frag, %jd);
		FCHK(fs->fs_ncg, <, 1, %jd);
		FCHK(fs->fs_fsbtodb, !=, ILOG2(fs->fs_fsize / sectorsize), %jd);
		FCHK(fs->fs_old_cgoffset, <, 0, %jd);
		FCHK2(fs->fs_old_cgoffset, >, 0, ~fs->fs_old_cgmask, <, 0, %jd);
		FCHK(fs->fs_old_cgoffset * (~fs->fs_old_cgmask), >, fs->fs_fpg,
		    %jd);
		FCHK(fs->fs_sblkno, !=, roundup(
		    howmany(fs->fs_sblockloc + SBLOCKSIZE, fs->fs_fsize),
		    fs->fs_frag), %jd);
		FCHK(CGSIZE(fs), >, fs->fs_bsize, %jd);
		/* Only need to validate these if reading in csum data */
		if ((flags & UFS_NOCSUM) != 0)
			return (error);
		FCHK((uint64_t)fs->fs_ipg * fs->fs_ncg, >,
		    (((int64_t)(1)) << 32) - INOPB(fs), %jd);
		FCHK(fs->fs_cstotal.cs_nifree, <, 0, %jd);
		FCHK(fs->fs_cstotal.cs_nifree, >,
		    (uint64_t)fs->fs_ipg * fs->fs_ncg, %jd);
		FCHK(fs->fs_cstotal.cs_ndir, >,
		    ((uint64_t)fs->fs_ipg * fs->fs_ncg) -
		    fs->fs_cstotal.cs_nifree, %jd);
		FCHK(fs->fs_size, <, 8 * fs->fs_frag, %jd);
		FCHK(fs->fs_size, <=, ((int64_t)fs->fs_ncg - 1) * fs->fs_fpg,
		    %jd);
		FCHK(fs->fs_size, >, (int64_t)fs->fs_ncg * fs->fs_fpg, %jd);
		FCHK(fs->fs_csaddr, <, 0, %jd);
		FCHK(fs->fs_cssize, !=,
		    fragroundup(fs, fs->fs_ncg * sizeof(struct csum)), %jd);
		FCHK(fs->fs_csaddr + howmany(fs->fs_cssize, fs->fs_fsize), >,
		    fs->fs_size, %jd);
		FCHK(fs->fs_csaddr, <, cgdmin(fs, dtog(fs, fs->fs_csaddr)),
		    %jd);
		FCHK(dtog(fs, fs->fs_csaddr + howmany(fs->fs_cssize,
		    fs->fs_fsize)), >, dtog(fs, fs->fs_csaddr), %jd);
		return (error);
	}
	if (fs->fs_magic == FS_UFS2_MAGIC) {
		if ((flags & UFS_ALTSBLK) == 0)
			FCHK2(fs->fs_sblockactualloc, !=, SBLOCK_UFS2,
			    fs->fs_sblockactualloc, !=, 0, %jd);
		FCHK(fs->fs_sblockloc, !=, SBLOCK_UFS2, %#jx);
		FCHK(fs->fs_maxsymlinklen, !=, ((UFS_NDADDR + UFS_NIADDR) *
			sizeof(ufs2_daddr_t)), %jd);
		FCHK(fs->fs_nindir, !=, fs->fs_bsize / sizeof(ufs2_daddr_t),
		    %jd);
		FCHK(fs->fs_inopb, !=,
		    fs->fs_bsize / sizeof(struct ufs2_dinode), %jd);
	} else if (fs->fs_magic == FS_UFS1_MAGIC) {
		if ((flags & UFS_ALTSBLK) == 0)
			FCHK(fs->fs_sblockactualloc, >, SBLOCK_UFS1, %jd);
		FCHK(fs->fs_sblockloc, <, 0, %jd);
		FCHK(fs->fs_sblockloc, >, SBLOCK_UFS1, %jd);
		FCHK(fs->fs_nindir, !=, fs->fs_bsize / sizeof(ufs1_daddr_t),
		    %jd);
		FCHK(fs->fs_inopb, !=,
		    fs->fs_bsize / sizeof(struct ufs1_dinode), %jd);
		FCHK(fs->fs_maxsymlinklen, !=, ((UFS_NDADDR + UFS_NIADDR) *
			sizeof(ufs1_daddr_t)), %jd);
		WCHK(fs->fs_old_inodefmt, !=, FS_44INODEFMT, %jd);
		WCHK(fs->fs_old_rotdelay, !=, 0, %jd);
		WCHK(fs->fs_old_rps, !=, 60, %jd);
		WCHK(fs->fs_old_nspf, !=, fs->fs_fsize / sectorsize, %jd);
		WCHK(fs->fs_old_interleave, !=, 1, %jd);
		WCHK(fs->fs_old_trackskew, !=, 0, %jd);
		WCHK(fs->fs_old_cpc, !=, 0, %jd);
		WCHK(fs->fs_old_postblformat, !=, 1, %jd);
		FCHK(fs->fs_old_nrpos, !=, 1, %jd);
		WCHK(fs->fs_old_nsect, !=, fs->fs_old_spc, %jd);
		WCHK(fs->fs_old_npsect, !=, fs->fs_old_spc, %jd);
	} else {
		/* Bad magic number, so assume not a superblock */
		return (ENOENT);
	}
	FCHK(fs->fs_bsize, <, MINBSIZE, %jd);
	FCHK(fs->fs_bsize, >, MAXBSIZE, %jd);
	FCHK(fs->fs_bsize, <, roundup(sizeof(struct fs), DEV_BSIZE), %jd);
	FCHK(powerof2(fs->fs_bsize), ==, 0, %jd);
	FCHK(fs->fs_frag, <, 1, %jd);
	FCHK(fs->fs_frag, >, MAXFRAG, %jd);
	FCHK(fs->fs_frag, !=, numfrags(fs, fs->fs_bsize), %jd);
	FCHK(fs->fs_fsize, <, sectorsize, %jd);
	FCHK(fs->fs_fsize * fs->fs_frag, !=, fs->fs_bsize, %jd);
	FCHK(powerof2(fs->fs_fsize), ==, 0, %jd);
	FCHK(fs->fs_fpg, <, 3 * fs->fs_frag, %jd);
	FCHK(fs->fs_ncg, <, 1, %jd);
	FCHK(fs->fs_ipg, <, fs->fs_inopb, %jd);
	FCHK((uint64_t)fs->fs_ipg * fs->fs_ncg, >,
	    (((int64_t)(1)) << 32) - INOPB(fs), %jd);
	FCHK(fs->fs_cstotal.cs_nifree, <, 0, %jd);
	FCHK(fs->fs_cstotal.cs_nifree, >, (uint64_t)fs->fs_ipg * fs->fs_ncg,
	    %jd);
	FCHK(fs->fs_cstotal.cs_ndir, <, 0, %jd);
	FCHK(fs->fs_cstotal.cs_ndir, >,
	    ((uint64_t)fs->fs_ipg * fs->fs_ncg) - fs->fs_cstotal.cs_nifree,
	    %jd);
	FCHK(fs->fs_sbsize, >, SBLOCKSIZE, %jd);
	FCHK(fs->fs_sbsize, <, (signed)sizeof(struct fs), %jd);
	/* fix for misconfigured filesystems */
	if (fs->fs_maxbsize == 0)
		fs->fs_maxbsize = fs->fs_bsize;
	FCHK(fs->fs_maxbsize, <, fs->fs_bsize, %jd);
	FCHK(powerof2(fs->fs_maxbsize), ==, 0, %jd);
	FCHK(fs->fs_maxbsize, >, FS_MAXCONTIG * fs->fs_bsize, %jd);
	FCHK(fs->fs_bmask, !=, ~(fs->fs_bsize - 1), %#jx);
	FCHK(fs->fs_fmask, !=, ~(fs->fs_fsize - 1), %#jx);
	FCHK(fs->fs_qbmask, !=, ~fs->fs_bmask, %#jx);
	FCHK(fs->fs_qfmask, !=, ~fs->fs_fmask, %#jx);
	FCHK(fs->fs_bshift, !=, ILOG2(fs->fs_bsize), %jd);
	FCHK(fs->fs_fshift, !=, ILOG2(fs->fs_fsize), %jd);
	FCHK(fs->fs_fragshift, !=, ILOG2(fs->fs_frag), %jd);
	FCHK(fs->fs_fsbtodb, !=, ILOG2(fs->fs_fsize / sectorsize), %jd);
	FCHK(fs->fs_old_cgoffset, <, 0, %jd);
	FCHK2(fs->fs_old_cgoffset, >, 0, ~fs->fs_old_cgmask, <, 0, %jd);
	FCHK(fs->fs_old_cgoffset * (~fs->fs_old_cgmask), >, fs->fs_fpg, %jd);
	FCHK(CGSIZE(fs), >, fs->fs_bsize, %jd);
	/*
	 * If anything has failed up to this point, it is usafe to proceed
	 * as checks below may divide by zero or make other fatal calculations.
	 * So if we have any errors at this point, give up.
	 */
	if (error)
		return (error);
	FCHK(fs->fs_sbsize % sectorsize, !=, 0, %jd);
	FCHK(fs->fs_ipg % fs->fs_inopb, !=, 0, %jd);
	FCHK(fs->fs_sblkno, !=, roundup(
	    howmany(fs->fs_sblockloc + SBLOCKSIZE, fs->fs_fsize),
	    fs->fs_frag), %jd);
	FCHK(fs->fs_cblkno, !=, fs->fs_sblkno +
	    roundup(howmany(SBLOCKSIZE, fs->fs_fsize), fs->fs_frag), %jd);
	FCHK(fs->fs_iblkno, !=, fs->fs_cblkno + fs->fs_frag, %jd);
	FCHK(fs->fs_dblkno, !=, fs->fs_iblkno + fs->fs_ipg / INOPF(fs), %jd);
	FCHK(fs->fs_cgsize, >, fs->fs_bsize, %jd);
	FCHK(fs->fs_cgsize, <, fs->fs_fsize, %jd);
	FCHK(fs->fs_cgsize % fs->fs_fsize, !=, 0, %jd);
	/*
	 * This test is valid, however older versions of growfs failed
	 * to correctly update fs_dsize so will fail this test. Thus we
	 * exclude it from the requirements.
	 */
#ifdef notdef
	WCHK(fs->fs_dsize, !=, fs->fs_size - fs->fs_sblkno -
		fs->fs_ncg * (fs->fs_dblkno - fs->fs_sblkno) -
		howmany(fs->fs_cssize, fs->fs_fsize), %jd);
#endif
	WCHK(fs->fs_metaspace, <, 0, %jd);
	WCHK(fs->fs_metaspace, >, fs->fs_fpg / 2, %jd);
	WCHK(fs->fs_minfree, >, 99, %jd%%);
	maxfilesize = fs->fs_bsize * UFS_NDADDR - 1;
	for (sizepb = fs->fs_bsize, i = 0; i < UFS_NIADDR; i++) {
		sizepb *= NINDIR(fs);
		maxfilesize += sizepb;
	}
	WCHK(fs->fs_maxfilesize, >, maxfilesize, %jd);
	/*
	 * These values have a tight interaction with each other that
	 * makes it hard to tightly bound them. So we can only check
	 * that they are within a broader possible range.
	 *
	 * The size cannot always be accurately determined, but ensure
	 * that it is consistent with the number of cylinder groups (fs_ncg)
	 * and the number of fragments per cylinder group (fs_fpg). Ensure
	 * that the summary information size is correct and that it starts
	 * and ends in the data area of the same cylinder group.
	 */
	FCHK(fs->fs_size, <, 8 * fs->fs_frag, %jd);
	FCHK(fs->fs_size, <=, ((int64_t)fs->fs_ncg - 1) * fs->fs_fpg, %jd);
	FCHK(fs->fs_size, >, (int64_t)fs->fs_ncg * fs->fs_fpg, %jd);
	/*
	 * If we are not requested to read in the csum data stop here
	 * as the correctness of the remaining values is only important
	 * to bound the space needed to be allocated to hold the csum data.
	 */
	if ((flags & UFS_NOCSUM) != 0)
		return (error);
	FCHK(fs->fs_csaddr, <, 0, %jd);
	FCHK(fs->fs_cssize, !=,
	    fragroundup(fs, fs->fs_ncg * sizeof(struct csum)), %jd);
	FCHK(fs->fs_csaddr + howmany(fs->fs_cssize, fs->fs_fsize), >,
	    fs->fs_size, %jd);
	FCHK(fs->fs_csaddr, <, cgdmin(fs, dtog(fs, fs->fs_csaddr)), %jd);
	FCHK(dtog(fs, fs->fs_csaddr + howmany(fs->fs_cssize, fs->fs_fsize)), >,
	    dtog(fs, fs->fs_csaddr), %jd);
	/*
	 * With file system clustering it is possible to allocate
	 * many contiguous blocks. The kernel variable maxphys defines
	 * the maximum transfer size permitted by the controller and/or
	 * buffering. The fs_maxcontig parameter controls the maximum
	 * number of blocks that the filesystem will read or write
	 * in a single transfer. It is calculated when the filesystem
	 * is created as maxphys / fs_bsize. The loader uses a maxphys
	 * of 128K even when running on a system that supports larger
	 * values. If the filesystem was built on a system that supports
	 * a larger maxphys (1M is typical) it will have configured
	 * fs_maxcontig for that larger system. So we bound the upper
	 * allowable limit for fs_maxconfig to be able to at least 
	 * work with a 1M maxphys on the smallest block size filesystem:
	 * 1M / 4096 == 256. There is no harm in allowing the mounting of
	 * filesystems that make larger than maxphys I/O requests because
	 * those (mostly 32-bit machines) can (very slowly) handle I/O
	 * requests that exceed maxphys.
	 */
	WCHK(fs->fs_maxcontig, <, 0, %jd);
	WCHK(fs->fs_maxcontig, >, MAX(256, maxphys / fs->fs_bsize), %jd);
	FCHK2(fs->fs_maxcontig, ==, 0, fs->fs_contigsumsize, !=, 0, %jd);
	FCHK2(fs->fs_maxcontig, >, 1, fs->fs_contigsumsize, !=,
	    MIN(fs->fs_maxcontig, FS_MAXCONTIG), %jd);
	return (error);
}

/*
 * Make an extensive search to find a superblock. If the superblock
 * in the standard place cannot be used, try looking for one of the
 * backup superblocks.
 *
 * Flags are made up of the following or'ed together options:
 *
 * UFS_NOMSG indicates that superblock inconsistency error messages
 *    should not be printed.
 *
 * UFS_NOCSUM causes only the superblock itself to be returned, but does
 *    not read in any auxillary data structures like the cylinder group
 *    summary information.
 */
int
ffs_sbsearch(void *devfd, struct fs **fsp, int reqflags,
    struct malloc_type *filltype,
    int (*readfunc)(void *devfd, off_t loc, void **bufp, int size))
{
	struct fsrecovery *fsr;
	struct fs *protofs;
	void *fsrbuf;
	char *cp;
	long nocsum, flags, msg, cg;
	off_t sblk, secsize;
	int error;

	msg = (reqflags & UFS_NOMSG) == 0;
	nocsum = reqflags & UFS_NOCSUM;
	/*
	 * Try normal superblock read and return it if it works.
	 *
	 * Suppress messages if it fails until we find out if
	 * failure can be avoided.
	 */
	flags = UFS_NOMSG | nocsum;
	error = ffs_sbget(devfd, fsp, UFS_STDSB, flags, filltype, readfunc);
	/*
	 * If successful or endian error, no need to try further.
	 */
	if (error == 0 || error == EILSEQ) {
		if (msg && error == EILSEQ)
			printf("UFS superblock failed due to endian mismatch "
			    "between machine and filesystem\n");
		return (error);
	}
	/*
	 * First try: ignoring hash failures.
	 */
	flags |= UFS_NOHASHFAIL;
	if (msg)
		flags &= ~UFS_NOMSG;
	if (ffs_sbget(devfd, fsp, UFS_STDSB, flags, filltype, readfunc) == 0)
		return (0);
	/*
	 * Next up is to check if fields of the superblock that are
	 * needed to find backup superblocks are usable.
	 */
	if (msg)
		printf("Attempted recovery for standard superblock: failed\n");
	flags = UFS_FSRONLY | UFS_NOHASHFAIL | UFS_NOCSUM | UFS_NOMSG;
	if (ffs_sbget(devfd, &protofs, UFS_STDSB, flags, filltype,
	    readfunc) == 0) {
		if (msg)
			printf("Attempt extraction of recovery data from "
			    "standard superblock.\n");
	} else {
		/*
		 * Final desperation is to see if alternate superblock
		 * parameters have been saved in the boot area.
		 */
		if (msg)
			printf("Attempted extraction of recovery data from "
			    "standard superblock: failed\nAttempt to find "
			    "boot zone recovery data.\n");
		/*
		 * Look to see if recovery information has been saved.
		 * If so we can generate a prototype superblock based
		 * on that information.
		 *
		 * We need fragments-per-group, number of cylinder groups,
		 * location of the superblock within the cylinder group, and
		 * the conversion from filesystem fragments to disk blocks.
		 *
		 * When building a UFS2 filesystem, newfs(8) stores these
		 * details at the end of the boot block area at the start
		 * of the filesystem partition. If they have been overwritten
		 * by a boot block, we fail.  But usually they are there
		 * and we can use them.
		 *
		 * We could ask the underlying device for its sector size,
		 * but some devices lie. So we just try a plausible range.
		 */
		error = ENOENT;
		fsrbuf = NULL;
		for (secsize = dbtob(1); secsize <= SBLOCKSIZE; secsize *= 2)
			if ((error = (*readfunc)(devfd, (SBLOCK_UFS2 - secsize),
			    &fsrbuf, secsize)) == 0)
				break;
		if (error != 0)
			goto trynowarn;
		cp = fsrbuf; /* type change to keep compiler happy */
		fsr = (struct fsrecovery *)&cp[secsize - sizeof *fsr];
		if (fsr->fsr_magic != FS_UFS2_MAGIC ||
		    (protofs = UFS_MALLOC(SBLOCKSIZE, filltype, M_NOWAIT))
		    == NULL) {
			UFS_FREE(fsrbuf, filltype);
			goto trynowarn;
		}
		memset(protofs, 0, sizeof(struct fs));
		protofs->fs_fpg = fsr->fsr_fpg;
		protofs->fs_fsbtodb = fsr->fsr_fsbtodb;
		protofs->fs_sblkno = fsr->fsr_sblkno;
		protofs->fs_magic = fsr->fsr_magic;
		protofs->fs_ncg = fsr->fsr_ncg;
		UFS_FREE(fsrbuf, filltype);
	}
	/*
	 * Scan looking for alternative superblocks.
	 */
	flags = nocsum;
	if (!msg)
		flags |= UFS_NOMSG;
	for (cg = 0; cg < protofs->fs_ncg; cg++) {
		sblk = fsbtodb(protofs, cgsblock(protofs, cg));
		if (msg)
			printf("Try cg %ld at sblock loc %jd\n", cg,
			    (intmax_t)sblk);
		if (ffs_sbget(devfd, fsp, dbtob(sblk), flags, filltype,
		    readfunc) == 0) {
			if (msg)
				printf("Succeeded with alternate superblock "
				    "at %jd\n", (intmax_t)sblk);
			UFS_FREE(protofs, filltype);
			return (0);
		}
	}
	UFS_FREE(protofs, filltype);
	/*
	 * Our alternate superblock strategies failed. Our last ditch effort
	 * is to see if the standard superblock has only non-critical errors.
	 */
trynowarn:
	flags = UFS_NOWARNFAIL | UFS_NOMSG | nocsum;
	if (msg) {
		printf("Finding an alternate superblock failed.\nCheck for "
		    "only non-critical errors in standard superblock\n");
		flags &= ~UFS_NOMSG;
	}
	if (ffs_sbget(devfd, fsp, UFS_STDSB, flags, filltype, readfunc) != 0) {
		if (msg)
			printf("Failed, superblock has critical errors\n");
		return (ENOENT);
	}
	if (msg)
		printf("Success, using standard superblock with "
		    "non-critical errors.\n");
	return (0);
}

/*
 * Write a superblock to the devfd device from the memory pointed to by fs.
 * Write out the superblock summary information if it is present.
 *
 * If the write is successful, zero is returned. Otherwise one of the
 * following error values is returned:
 *     EIO: failed to write superblock.
 *     EIO: failed to write superblock summary information.
 */
int
ffs_sbput(void *devfd, struct fs *fs, off_t loc,
    int (*writefunc)(void *devfd, off_t loc, void *buf, int size))
{
	struct fs_summary_info *fs_si;
	int i, error, blks, size;
	uint8_t *space;

	/*
	 * If there is summary information, write it first, so if there
	 * is an error, the superblock will not be marked as clean.
	 */
	if (fs->fs_si != NULL && fs->fs_csp != NULL) {
		blks = howmany(fs->fs_cssize, fs->fs_fsize);
		space = (uint8_t *)fs->fs_csp;
		for (i = 0; i < blks; i += fs->fs_frag) {
			size = fs->fs_bsize;
			if (i + fs->fs_frag > blks)
				size = (blks - i) * fs->fs_fsize;
			if ((error = (*writefunc)(devfd,
			     dbtob(fsbtodb(fs, fs->fs_csaddr + i)),
			     space, size)) != 0)
				return (error);
			space += size;
		}
	}
	fs->fs_fmod = 0;
	ffs_oldfscompat_write(fs);
#ifdef _KERNEL
	fs->fs_time = time_second;
#else /* User Code */
	fs->fs_time = time(NULL);
#endif
	/* Clear the pointers for the duration of writing. */
	fs_si = fs->fs_si;
	fs->fs_si = NULL;
	fs->fs_ckhash = ffs_calc_sbhash(fs);
	error = (*writefunc)(devfd, loc, fs, fs->fs_sbsize);
	/*
	 * A negative error code is returned when a copy of the
	 * superblock has been made which is discarded when the I/O
	 * is done. So the fs_si field does not and indeed cannot be
	 * restored after the write is done. Convert the error code
	 * back to its usual positive value when returning it.
	 */
	if (error < 0)
		return (-error - 1);
	fs->fs_si = fs_si;
	return (error);
}

/*
 * Calculate the check-hash for a superblock.
 */
uint32_t
ffs_calc_sbhash(struct fs *fs)
{
	uint32_t ckhash, save_ckhash;

	/*
	 * A filesystem that was using a superblock ckhash may be moved
	 * to an older kernel that does not support ckhashes. The
	 * older kernel will clear the FS_METACKHASH flag indicating
	 * that it does not update hashes. When the disk is moved back
	 * to a kernel capable of ckhashes it disables them on mount:
	 *
	 *	if ((fs->fs_flags & FS_METACKHASH) == 0)
	 *		fs->fs_metackhash = 0;
	 *
	 * This leaves (fs->fs_metackhash & CK_SUPERBLOCK) == 0) with an
	 * old stale value in the fs->fs_ckhash field. Thus the need to
	 * just accept what is there.
	 */
	if ((fs->fs_metackhash & CK_SUPERBLOCK) == 0)
		return (fs->fs_ckhash);

	save_ckhash = fs->fs_ckhash;
	fs->fs_ckhash = 0;
	/*
	 * If newly read from disk, the caller is responsible for
	 * verifying that fs->fs_sbsize <= SBLOCKSIZE.
	 */
	ckhash = calculate_crc32c(~0L, (void *)fs, fs->fs_sbsize);
	fs->fs_ckhash = save_ckhash;
	return (ckhash);
}

/*
 * Update the frsum fields to reflect addition or deletion
 * of some frags.
 */
void
ffs_fragacct(struct fs *fs, int fragmap, int32_t fraglist[], int cnt)
{
	int inblk;
	int field, subfield;
	int siz, pos;

	inblk = (int)(fragtbl[fs->fs_frag][fragmap]) << 1;
	fragmap <<= 1;
	for (siz = 1; siz < fs->fs_frag; siz++) {
		if ((inblk & (1 << (siz + (fs->fs_frag % NBBY)))) == 0)
			continue;
		field = around[siz];
		subfield = inside[siz];
		for (pos = siz; pos <= fs->fs_frag; pos++) {
			if ((fragmap & field) == subfield) {
				fraglist[siz] += cnt;
				pos += siz;
				field <<= siz;
				subfield <<= siz;
			}
			field <<= 1;
			subfield <<= 1;
		}
	}
}

/*
 * block operations
 *
 * check if a block is available
 */
int
ffs_isblock(struct fs *fs, unsigned char *cp, ufs1_daddr_t h)
{
	unsigned char mask;

	switch ((int)fs->fs_frag) {
	case 8:
		return (cp[h] == 0xff);
	case 4:
		mask = 0x0f << ((h & 0x1) << 2);
		return ((cp[h >> 1] & mask) == mask);
	case 2:
		mask = 0x03 << ((h & 0x3) << 1);
		return ((cp[h >> 2] & mask) == mask);
	case 1:
		mask = 0x01 << (h & 0x7);
		return ((cp[h >> 3] & mask) == mask);
	default:
#ifdef _KERNEL
		panic("ffs_isblock");
#endif
		break;
	}
	return (0);
}

/*
 * check if a block is free
 */
int
ffs_isfreeblock(struct fs *fs, uint8_t *cp, ufs1_daddr_t h)
{

	switch ((int)fs->fs_frag) {
	case 8:
		return (cp[h] == 0);
	case 4:
		return ((cp[h >> 1] & (0x0f << ((h & 0x1) << 2))) == 0);
	case 2:
		return ((cp[h >> 2] & (0x03 << ((h & 0x3) << 1))) == 0);
	case 1:
		return ((cp[h >> 3] & (0x01 << (h & 0x7))) == 0);
	default:
#ifdef _KERNEL
		panic("ffs_isfreeblock");
#endif
		break;
	}
	return (0);
}

/*
 * take a block out of the map
 */
void
ffs_clrblock(struct fs *fs, uint8_t *cp, ufs1_daddr_t h)
{

	switch ((int)fs->fs_frag) {
	case 8:
		cp[h] = 0;
		return;
	case 4:
		cp[h >> 1] &= ~(0x0f << ((h & 0x1) << 2));
		return;
	case 2:
		cp[h >> 2] &= ~(0x03 << ((h & 0x3) << 1));
		return;
	case 1:
		cp[h >> 3] &= ~(0x01 << (h & 0x7));
		return;
	default:
#ifdef _KERNEL
		panic("ffs_clrblock");
#endif
		break;
	}
}

/*
 * put a block into the map
 */
void
ffs_setblock(struct fs *fs, unsigned char *cp, ufs1_daddr_t h)
{

	switch ((int)fs->fs_frag) {
	case 8:
		cp[h] = 0xff;
		return;
	case 4:
		cp[h >> 1] |= (0x0f << ((h & 0x1) << 2));
		return;
	case 2:
		cp[h >> 2] |= (0x03 << ((h & 0x3) << 1));
		return;
	case 1:
		cp[h >> 3] |= (0x01 << (h & 0x7));
		return;
	default:
#ifdef _KERNEL
		panic("ffs_setblock");
#endif
		break;
	}
}

/*
 * Update the cluster map because of an allocation or free.
 *
 * Cnt == 1 means free; cnt == -1 means allocating.
 */
void
ffs_clusteracct(struct fs *fs, struct cg *cgp, ufs1_daddr_t blkno, int cnt)
{
	int32_t *sump;
	int32_t *lp;
	uint8_t *freemapp, *mapp;
	int i, start, end, forw, back, map;
	uint64_t bit;

	if (fs->fs_contigsumsize <= 0)
		return;
	freemapp = cg_clustersfree(cgp);
	sump = cg_clustersum(cgp);
	/*
	 * Allocate or clear the actual block.
	 */
	if (cnt > 0)
		setbit(freemapp, blkno);
	else
		clrbit(freemapp, blkno);
	/*
	 * Find the size of the cluster going forward.
	 */
	start = blkno + 1;
	end = start + fs->fs_contigsumsize;
	if (end >= cgp->cg_nclusterblks)
		end = cgp->cg_nclusterblks;
	mapp = &freemapp[start / NBBY];
	map = *mapp++;
	bit = 1U << (start % NBBY);
	for (i = start; i < end; i++) {
		if ((map & bit) == 0)
			break;
		if ((i & (NBBY - 1)) != (NBBY - 1)) {
			bit <<= 1;
		} else {
			map = *mapp++;
			bit = 1;
		}
	}
	forw = i - start;
	/*
	 * Find the size of the cluster going backward.
	 */
	start = blkno - 1;
	end = start - fs->fs_contigsumsize;
	if (end < 0)
		end = -1;
	mapp = &freemapp[start / NBBY];
	map = *mapp--;
	bit = 1U << (start % NBBY);
	for (i = start; i > end; i--) {
		if ((map & bit) == 0)
			break;
		if ((i & (NBBY - 1)) != 0) {
			bit >>= 1;
		} else {
			map = *mapp--;
			bit = 1U << (NBBY - 1);
		}
	}
	back = start - i;
	/*
	 * Account for old cluster and the possibly new forward and
	 * back clusters.
	 */
	i = back + forw + 1;
	if (i > fs->fs_contigsumsize)
		i = fs->fs_contigsumsize;
	sump[i] += cnt;
	if (back > 0)
		sump[back] -= cnt;
	if (forw > 0)
		sump[forw] -= cnt;
	/*
	 * Update cluster summary information.
	 */
	lp = &sump[fs->fs_contigsumsize];
	for (i = fs->fs_contigsumsize; i > 0; i--)
		if (*lp-- > 0)
			break;
	fs->fs_maxcluster[cgp->cg_cgx] = i;
}
