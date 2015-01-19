/*-
 * Copyright (c) 2002, 2003 Gordon Tetlow
 * Copyright (c) 2006 Pawel Jakub Dawidek <pjd@FreeBSD.org>
 * Copyright (c) 2014 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Edward Tomasz Napierala under sponsorship
 * from the FreeBSD Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ufs/ufs/dinode.h>
#include <ufs/ffs/fs.h>

#include "fstyp.h"

static const int superblocks[] = SBLOCKSEARCH;

int
fstyp_ufs(FILE *fp, char *label, size_t labelsize)
{
	int sb, superblock;
	struct fs *fs;

	/*
	 * Walk through the standard places that superblocks hide and look
	 * for UFS magic. If we find magic, then check that the size in the
	 * superblock corresponds to the size of the underlying provider.
	 * Finally, look for a volume label and create an appropriate
	 * provider based on that.
	 */
	for (sb = 0; (superblock = superblocks[sb]) != -1; sb++) {
		fs = (struct fs *)read_buf(fp, superblock, SBLOCKSIZE);
		if (fs == NULL)
			continue;
		/*
		 * Check for magic. We also need to check if file system size is equal
		 * to providers size, because sysinstall(8) used to bogusly put first
		 * partition at offset 0 instead of 16, and glabel/ufs would find file
		 * system on slice instead of partition.
		 */
#ifdef notyet
		if (fs->fs_magic == FS_UFS1_MAGIC && fs->fs_fsize > 0 &&
		    ((pp->mediasize / fs->fs_fsize == fs->fs_old_size) ||
		    (pp->mediasize / fs->fs_fsize == fs->fs_providersize))) {
		    	/* Valid UFS1. */
		} else if (fs->fs_magic == FS_UFS2_MAGIC && fs->fs_fsize > 0 &&
		    ((pp->mediasize / fs->fs_fsize == fs->fs_size) ||
		    (pp->mediasize / fs->fs_fsize == fs->fs_providersize))) {
		    	/* Valid UFS2. */
		} else {
			g_free(fs);
			continue;
		}
#else
		if (fs->fs_magic == FS_UFS1_MAGIC && fs->fs_fsize > 0) {
		    	/* Valid UFS1. */
		} else if (fs->fs_magic == FS_UFS2_MAGIC && fs->fs_fsize > 0) {
		    	/* Valid UFS2. */
		} else {
			free(fs);
			continue;
		}
#endif

		if (fs->fs_sblockloc != superblock || fs->fs_ncg < 1 ||
		    fs->fs_bsize < MINBSIZE ||
		    (size_t)fs->fs_bsize < sizeof(struct fs)) {
			free(fs);
			continue;
		}

		strlcpy(label, fs->fs_volname, labelsize);

		free(fs);
		return (0);
	}

	return (1);
}
