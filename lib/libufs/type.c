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
#include <fstab.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libufs.h>

/* Internally, track the 'name' value, it's ours. */
#define	MINE_NAME	0x01
/* Track if its fd points to a writable device. */
#define	MINE_WRITE	0x02

struct uufsd *
ufs_disk_ctor(const char *name)
{
	struct uufsd *new;

	new = NULL;

	ERROR(new, NULL);

	new = malloc(sizeof(*new));
	if (new == NULL) {
		ERROR(new, "unable to allocate memory for disk");
		return NULL;
	}

	if (ufs_disk_fillout(new, name) == -1) {
		ERROR(new, "could not fill out disk");
		free(new);
		return NULL;
	}

	return new;
}

void
ufs_disk_dtor(struct uufsd **diskp)
{
	struct uufsd *disk;

	if (diskp != NULL)
		disk = *diskp;
	else
		return;

	ERROR(disk, NULL);

	ufs_disk_close(disk);
	free(disk);
	*diskp = NULL;
}

int
ufs_disk_close(struct uufsd *disk)
{
	ERROR(disk, NULL);
	close(disk->d_fd);
	if (disk->d_inoblock != NULL) {
		free(disk->d_inoblock);
		disk->d_inoblock = NULL;
	}
	if (disk->d_mine & MINE_NAME) {
		free((char *)(uintptr_t)disk->d_name);
		disk->d_name = NULL;
	}
	return 0;
}

int
ufs_disk_fillout(struct uufsd *disk, const char *name)
{
	if (ufs_disk_fillout_blank(disk, name) == -1) {
		return -1;
	}
	if (sbread(disk) == -1) {
		ERROR(disk, "could not read superblock to fill out disk");
		return -1;
	}
}

int
ufs_disk_fillout_blank(struct uufsd *disk, const char *name)
{
	struct stat st;
	struct fstab *fs;
	const char *oname;
	char dev[MAXPATHLEN];
	int fd;

	ERROR(disk, NULL);

	oname = name;
	fs = getfsfile(name);
	if (fs != NULL)
		name = fs->fs_spec;
again:	if (stat(name, &st) < 0) {
		if (*name != '/') {
			if (*name == 'r')
				name++;
			snprintf(dev, sizeof(dev), "%s%s", _PATH_DEV, name);
			name = dev;
			goto again;
		}
		ERROR(disk, "could not find special device");
		return -1;
	}
	fd = open(name, O_RDONLY);
	if (fd == -1) {
		ERROR(disk, "could not open special device");
		return -1;
	}

	disk->d_bsize = 1;
	disk->d_ccg = 0;
	disk->d_fd = fd;
	disk->d_inoblock = NULL;
	disk->d_inomin = 0;
	disk->d_inomax = 0;
	disk->d_lcg = 0;
	disk->d_mine = 0;
	disk->d_ufs = 0;
	disk->d_error = NULL;

	if (oname != name) {
		name = strdup(name);
		if (name == NULL) {
			ERROR(disk, "could not allocate memory for disk name");
			return -1;
		}
		disk->d_mine |= MINE_NAME;
	}
	disk->d_name = name;

	return 0;
}

int
ufs_disk_write(struct uufsd *disk)
{
	int rofd;

	ERROR(disk, NULL);

	if (disk->d_mine & MINE_WRITE)
		return 0;

	rofd = disk->d_fd;

	disk->d_fd = open(disk->d_name, O_RDWR);
	if (disk->d_fd < 0) {
		ERROR(disk, "failed to open disk for writing");
		disk->d_fd = rofd;
		return -1;
	}

	disk->d_mine |= MINE_WRITE;

	return 0;
}
