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

struct uufsd *
ufs_disk_ctor(const char *name)
{
	struct uufsd *new;

	DEBUG(NULL);

	new = malloc(sizeof(*new));
	if (new == NULL) {
		DEBUG(NULL);
		return NULL;
	}

	if (ufs_disk_fillout(new, name) == -1) {
		DEBUG(NULL);
		free(new);
		return NULL;
	}

	return new;
}

void
ufs_disk_dtor(struct uufsd **disk)
{
	DEBUG(NULL);
	ufs_disk_close(*disk);
	free(*disk);
	*disk = NULL;
}

int
ufs_disk_close(struct uufsd *disk)
{
	DEBUG(NULL);
	close(disk->d_fd);
	if (disk->d_inoblock != NULL) {
		free(disk->d_inoblock);
		disk->d_inoblock = NULL;
	}
	return 0;
}

int
ufs_disk_fillout(struct uufsd *disk, const char *name)
{
	int fd;

	DEBUG(NULL);

	fd = open(name, O_RDONLY);
	if (fd == -1) {
		DEBUG("open");
		return -1;
	}

	disk->d_bsize = 1;
	disk->d_fd = fd;
	disk->d_inoblock = NULL;
	disk->d_inomin = 0;
	disk->d_inomax = 0;
	disk->d_name = name;
	disk->d_ufs = 0;

	if (sbread(disk) == -1) {
		DEBUG(NULL);
		return -1;
	}

	return 0;
}
