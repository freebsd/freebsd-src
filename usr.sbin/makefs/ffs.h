/*	$NetBSD: ffs.h,v 1.2 2004/12/20 20:51:42 jmc Exp $	*/

/*-
 * SPDX-License-Identifier: BSD-4-Clause
 *
 * Copyright (c) 2001-2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Luke Mewburn for Wasabi Systems, Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _FFS_H
#define _FFS_H

#include <ufs/ufs/dinode.h>
#include <ufs/ffs/fs.h>
#include <stdbool.h>

typedef struct {
	char	label[MAXVOLLEN];	/* volume name/label */
	int	bsize;		/* block size */
	int	fsize;		/* fragment size */
	int	cpg;		/* cylinders per group */
	int	cpgflg;		/* cpg was specified by user */
	int	density;	/* bytes per inode */
	bool	min_inodes;	/* allocate minimum number of inodes */
	int	ntracks;	/* number of tracks */
	int	nsectors;	/* number of sectors */
	int	rpm;		/* rpm */
	int	minfree;	/* free space threshold */
	int	optimization;	/* optimization (space or time) */
	int	maxcontig;	/* max contiguous blocks to allocate */
	int	rotdelay;	/* rotational delay between blocks */
	int	maxbpg;		/* maximum blocks per file in a cyl group */
	int	nrpos;		/* # of distinguished rotational positions */
	int	avgfilesize;	/* expected average file size */
	int	avgfpdir;	/* expected # of files per directory */
	int	version;	/* filesystem version (1 = FFS, 2 = UFS2) */
	int	maxbsize;	/* maximum extent size */
	int	maxblkspercg;	/* max # of blocks per cylinder group */
	int	softupdates;	/* soft updates */
		/* XXX: support `old' file systems ? */
} ffs_opt_t;

#endif /* _FFS_H */
