/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
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

#include <ufs/ufs/extattr.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/dinode.h>
#include <ufs/ffs/fs.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <libufs.h>

static int handle_disk_read(struct uufsd *, struct fs *, int);

/*
 * Read the standard superblock.
 *
 * The following option flags can be or'ed into disk->d_lookupflags:
 *
 * UFS_NOMSG indicates that superblock inconsistency error messages
 *    should not be printed.
 *
 * UFS_NOCSUM causes only the superblock itself to be returned, but does
 *    not read in any auxillary data structures like the cylinder group
 *    summary information.
 */
int
sbread(struct uufsd *disk)
{
	struct fs *fs;
	int error;

	error = sbget(disk->d_fd, &fs, disk->d_sblockloc, disk->d_lookupflags);
	return (handle_disk_read(disk, fs, error));
}

/*
 * Make an extensive search to find a superblock. If the superblock
 * in the standard place cannot be used, try looking for one of the
 * backup superblocks.
 *
 * The flags parameter is made up of the following or'ed together options:
 *
 * UFS_NOMSG indicates that superblock inconsistency error messages
 *    should not be printed.
 *
 * UFS_NOCSUM causes only the superblock itself to be returned, but does
 *    not read in any auxillary data structures like the cylinder group
 *    summary information.
 */
int
sbfind(struct uufsd *disk, int flags)
{
	struct fs *fs;
	int error;

	error = sbsearch(disk->d_fd, &fs, flags);
	return (handle_disk_read(disk, fs, error));
}

static int
handle_disk_read(struct uufsd *disk, struct fs *fs, int error)
{

	ERROR(disk, NULL);
	if (error != 0) {
		switch (error) {
		case EIO:
			ERROR(disk, "non-existent or truncated superblock");
			break;
		case ENOENT:
			ERROR(disk, "no usable known superblock found");
			break;
		case EINTEGRITY:
			ERROR(disk, "superblock check-hash failure");
			break;
		case ENOSPC:
			ERROR(disk, "failed to allocate space for superblock "
			    "information");
			break;
		case EINVAL:
			ERROR(disk, "The previous newfs operation on this "
			    "volume did not complete.\nYou must complete "
			    "newfs before using this volume.");
			break;
		default:
			ERROR(disk, "unknown superblock read error");
			errno = EIO;
			break;
		}
		disk->d_ufs = 0;
		return (-1);
	}
	memcpy(&disk->d_fs, fs, fs->fs_sbsize);
	free(fs);
	fs = &disk->d_fs;
	if (fs->fs_magic == FS_UFS1_MAGIC)
		disk->d_ufs = 1;
	if (fs->fs_magic == FS_UFS2_MAGIC)
		disk->d_ufs = 2;
	disk->d_bsize = fs->fs_fsize / fsbtodb(fs, 1);
	disk->d_sblock = fs->fs_sblockloc / disk->d_bsize;
	disk->d_si = fs->fs_si;
	return (0);
}

int
sbwrite(struct uufsd *disk, int all)
{
	struct fs *fs;
	int rv;

	ERROR(disk, NULL);

	rv = ufs_disk_write(disk);
	if (rv == -1) {
		ERROR(disk, "failed to open disk for writing");
		return (-1);
	}

	fs = &disk->d_fs;
	if ((errno = sbput(disk->d_fd, fs, all ? fs->fs_ncg : 0)) != 0) {
		switch (errno) {
		case EIO:
			ERROR(disk, "failed to write superblock");
			break;
		default:
			ERROR(disk, "unknown superblock write error");
			errno = EIO;
			break;
		}
		return (-1);
	}
	return (0);
}

/*
 * These are the low-level functions that actually read and write
 * the superblock and its associated data. The actual work is done by
 * the functions ffs_sbget and ffs_sbput in /sys/ufs/ffs/ffs_subr.c.
 */
static int use_pread(void *devfd, off_t loc, void **bufp, int size);
static int use_pwrite(void *devfd, off_t loc, void *buf, int size);

/*
 * The following two functions read a superblock. Their flags
 * parameter are made up of the following or'ed together options:
 *
 * UFS_NOMSG indicates that superblock inconsistency error messages
 *    should not be printed.
 *
 * UFS_NOCSUM causes only the superblock itself to be returned, but does
 *    not read in any auxillary data structures like the cylinder group
 *    summary information.
 *
 * Read a superblock from the devfd device allocating memory returned
 * in fsp.
 */
int
sbget(int devfd, struct fs **fsp, off_t sblockloc, int flags)
{
	int error;

	error = ffs_sbget(&devfd, fsp, sblockloc, flags, "user", use_pread);
	fflush(NULL); /* flush any messages */
	return (error);
}

/*
 * Make an extensive search of the devfd device to find a superblock.
 * If the superblock in the standard place cannot be used, try looking
 * for one of the backup superblocks. If found, memory is allocated and
 * returned in fsp.
 */
int
sbsearch(int devfd, struct fs **fsp, int flags)
{
	int error;

	error = ffs_sbsearch(&devfd, fsp, flags, "user", use_pread);
	fflush(NULL); /* flush any messages */
	return (error);
}

/*
 * A read function for use by user-level programs using libufs.
 */
static int
use_pread(void *devfd, off_t loc, void **bufp, int size)
{
	int fd;

	fd = *(int *)devfd;
	if ((*bufp = malloc(size)) == NULL)
		return (ENOSPC);
	if (pread(fd, *bufp, size, loc) != size)
		return (EIO);
	return (0);
}

/*
 * Write a superblock to the devfd device from the memory pointed to by fs.
 * Also write out the superblock summary information but do not free the
 * summary information memory.
 *
 * Additionally write out numaltwrite of the alternate superblocks. Use
 * fs->fs_ncg to write out all of the alternate superblocks.
 */
int
sbput(int devfd, struct fs *fs, int numaltwrite)
{
	struct csum *savedcsp;
	off_t savedactualloc;
	int i, error;

	error = ffs_sbput(&devfd, fs, fs->fs_sblockactualloc, use_pwrite);
	fflush(NULL); /* flush any messages */
	if (error != 0 || numaltwrite == 0)
		return (error);
	savedactualloc = fs->fs_sblockactualloc;
	if (fs->fs_si != NULL) {
		savedcsp = fs->fs_csp;
		fs->fs_csp = NULL;
	}
	for (i = 0; i < numaltwrite; i++) {
		fs->fs_sblockactualloc = dbtob(fsbtodb(fs, cgsblock(fs, i)));
		if ((error = ffs_sbput(&devfd, fs, fs->fs_sblockactualloc,
		     use_pwrite)) != 0) {
			fflush(NULL); /* flush any messages */
			fs->fs_sblockactualloc = savedactualloc;
			fs->fs_csp = savedcsp;
			return (error);
		}
	}
	fs->fs_sblockactualloc = savedactualloc;
	if (fs->fs_si != NULL)
		fs->fs_csp = savedcsp;
	fflush(NULL); /* flush any messages */
	return (0);
}

/*
 * A write function for use by user-level programs using sbput in libufs.
 */
static int
use_pwrite(void *devfd, off_t loc, void *buf, int size)
{
	int fd;

	fd = *(int *)devfd;
	if (pwrite(fd, buf, size, loc) != size)
		return (EIO);
	return (0);
}
