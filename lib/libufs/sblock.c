/*
 * Copyright (c) 2002 Juli Mallett.  All rights reserved.
 *
 * This software was written by Juli Mallett <jmallett@FreeBSD.org> for the
 * FreeBSD project.  Redistribution and use in source and binary forms, with
 * or without modification, are permitted provided that the following
 * conditions are met:
 *
 * 1. Redistribution of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistribution in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/disklabel.h>
#include <sys/stat.h>

#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/dinode.h>
#include <ufs/ffs/fs.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <libufs.h>

static int superblocks[] = SBLOCKSEARCH;

int
sbread(struct uufsd *disk)
{
	struct fs *fs;
	int sb, superblock;

	ERROR(disk, NULL);

	fs = &disk->d_fs;
	superblock = superblocks[0];

	for (sb = 0; (superblock = superblocks[sb]) != -1; sb++) {
		if (bread(disk, superblock, disk->d_sb, SBLOCKSIZE) == -1) {
			ERROR(disk, "non-existent or truncated superblock");
			return -1;
		}
		if (fs->fs_magic == FS_UFS1_MAGIC)
			disk->d_ufs = 1;
		if ((fs->fs_magic == FS_UFS2_MAGIC) &&
		    (fs->fs_sblockloc == superblock))
			disk->d_ufs = 2;
		if ((fs->fs_bsize <= MAXBSIZE) &&
		    (fs->fs_bsize >= sizeof(*fs))) {
			if (disk->d_ufs)
				break;
		}
		disk->d_ufs = 0;
	}
	if (superblock == -1 || disk->d_ufs == 0) {
		/*
		 * Other error cases will result in errno being set, here we
		 * must set it to indicate no superblock could be found with
		 * which to associate this disk/filesystem.
		 */
		ERROR(disk, "no usable known superblock found");
		errno = ENOENT;
		return -1;
	}
	disk->d_bsize = fs->fs_fsize / fsbtodb(fs, 1);
	disk->d_sblock = superblock / disk->d_bsize;
	return 0;
}

int
sbwrite(struct uufsd *disk, int all)
{
	struct fs *fs;
	int i;

	ERROR(disk, NULL);

	fs = &disk->d_fs;

	if (bwrite(disk, disk->d_sblock, fs, SBLOCKSIZE) == -1) {
		ERROR(disk, "failed to write superblock");
		return -1;
	}
	if (all) {
		for (i = 0; i < fs->fs_ncg; i++)
			if (bwrite(disk, fsbtodb(fs, cgsblock(fs, i)),
			    fs, SBLOCKSIZE) == -1) {
				ERROR(disk, "failed to update a superblock");
				return -1;
			}
	}
	return 0;
}
