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
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libufs.h>

int
getino(struct uufsd *disk, void **dino, ino_t inode, int *mode)
{
	ino_t min, max;
	caddr_t inoblock;
	struct ufs1_dinode *dp1;
	struct ufs2_dinode *dp2;
	struct fs *fs;

	ERROR(disk, NULL);

	fs = &disk->d_fs;
	inoblock = disk->d_inoblock;
	min = disk->d_inomin;
	max = disk->d_inomax;

	if (inoblock == NULL) {
		inoblock = malloc(fs->fs_bsize);
		if (inoblock == NULL) {
			ERROR(disk, "unable to allocate inode block");
			return (-1);
		}
		disk->d_inoblock = inoblock;
	}
	if (inode >= min && inode < max)
		goto gotit;
	bread(disk, fsbtodb(fs, ino_to_fsba(fs, inode)), inoblock,
	    fs->fs_bsize);
	disk->d_inomin = min = inode - (inode & INOPB(fs));
	disk->d_inomax = max = min + INOPB(fs);
gotit:	switch (disk->d_ufs) {
	case 1:
		dp1 = &((struct ufs1_dinode *)inoblock)[inode - min];
		*mode = dp1->di_mode & IFMT;
		*dino = dp1;
		return (0);
	case 2:
		dp2 = &((struct ufs2_dinode *)inoblock)[inode - min];
		*mode = dp2->di_mode & IFMT;
		*dino = dp2;
		return (0);
	default:
		break;
	}
	ERROR(disk, "unknown UFS filesystem type");
	return (-1);
}
