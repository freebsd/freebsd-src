/*-
 * Copyright (c) 1998 Robert Nordier
 * All rights reserved.
 * Copyright (c) 2001 Robert Drehmel
 * All rights reserved.
 * Copyright (c) 2014 Nathan Whitehorn
 * All rights reserved.
 * Copyright (c) 2015 Eric McCorkle
 * All rights reverved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <stdarg.h>
#include <stdbool.h>
#include <sys/cdefs.h>
#include <sys/param.h>
#include <efi.h>

#include "boot_module.h"

static dev_info_t *devinfo;
static dev_info_t *devices;

static int
dskread(void *buf, u_int64_t lba, int nblk)
{
	int size;
	EFI_STATUS status;

	lba = lba / (devinfo->dev->Media->BlockSize / DEV_BSIZE);
	size = nblk * DEV_BSIZE;

	status = devinfo->dev->ReadBlocks(devinfo->dev,
	    devinfo->dev->Media->MediaId, lba, size, buf);

	if (status != EFI_SUCCESS) {
		DPRINTF("dskread: failed dev: %p, id: %u, lba: %lu, size: %d, "
		    "status: %lu\n", devinfo->dev,
		    devinfo->dev->Media->MediaId, lba, size,
		    EFI_ERROR_CODE(status));
		return (-1);
	}

	return (0);
}

#include "ufsread.c"

static ssize_t
fsstat(ufs_ino_t inode)
{
#ifndef UFS2_ONLY
	static struct ufs1_dinode dp1;
#endif
#ifndef UFS1_ONLY
	static struct ufs2_dinode dp2;
#endif
	static struct fs fs;
	static ufs_ino_t inomap;
	char *blkbuf;
	void *indbuf;
	size_t n, size;
	static ufs2_daddr_t blkmap, indmap;

	blkbuf = dmadat->blkbuf;
	indbuf = dmadat->indbuf;
	if (!dsk_meta) {
		inomap = 0;
		for (n = 0; sblock_try[n] != -1; n++) {
			if (dskread(dmadat->sbbuf, sblock_try[n] / DEV_BSIZE,
			    SBLOCKSIZE / DEV_BSIZE))
				return (-1);
			memcpy(&fs, dmadat->sbbuf, sizeof(struct fs));
			if ((
#if defined(UFS1_ONLY)
			    fs.fs_magic == FS_UFS1_MAGIC
#elif defined(UFS2_ONLY)
			    (fs.fs_magic == FS_UFS2_MAGIC &&
			    fs.fs_sblockloc == sblock_try[n])
#else
			    fs.fs_magic == FS_UFS1_MAGIC ||
			    (fs.fs_magic == FS_UFS2_MAGIC &&
			    fs.fs_sblockloc == sblock_try[n])
#endif
			    ) &&
			    fs.fs_bsize <= MAXBSIZE &&
			    fs.fs_bsize >= (int32_t)sizeof(struct fs))
				break;
		}
		if (sblock_try[n] == -1) {
			return (-1);
		}
		dsk_meta++;
	} else
		memcpy(&fs, dmadat->sbbuf, sizeof(struct fs));
	if (!inode)
		return (0);
	if (inomap != inode) {
		n = IPERVBLK(&fs);
		if (dskread(blkbuf, INO_TO_VBA(&fs, n, inode), DBPERVBLK))
			return (-1);
		n = INO_TO_VBO(n, inode);
#if defined(UFS1_ONLY)
		memcpy(&dp1, (struct ufs1_dinode *)blkbuf + n,
		    sizeof(struct ufs1_dinode));
#elif defined(UFS2_ONLY)
		memcpy(&dp2, (struct ufs2_dinode *)blkbuf + n,
		    sizeof(struct ufs2_dinode));
#else
		if (fs.fs_magic == FS_UFS1_MAGIC)
			memcpy(&dp1, (struct ufs1_dinode *)(void *)blkbuf + n,
			    sizeof(struct ufs1_dinode));
		else
			memcpy(&dp2, (struct ufs2_dinode *)(void *)blkbuf + n,
			    sizeof(struct ufs2_dinode));
#endif
		inomap = inode;
		fs_off = 0;
		blkmap = indmap = 0;
	}
	size = DIP(di_size);
	n = size - fs_off;

	return (n);
}

static struct dmadat __dmadat;

static EFI_STATUS
probe(dev_info_t* dev)
{

	devinfo = dev;
	dmadat = &__dmadat;
	if (fsread(0, NULL, 0) < 0)
		return (EFI_UNSUPPORTED);

	add_device(&devices, dev);

	return (EFI_SUCCESS);
}

static EFI_STATUS
try_load(dev_info_t *dev, const char *loader_path, void **bufp, size_t *bufsize)
{
	ufs_ino_t ino;
	EFI_STATUS status;
	size_t size;
	ssize_t read;
	void *buf;

	dsk_meta = 0;
	devinfo = dev;
	if ((ino = lookup(loader_path)) == 0)
		return (EFI_NOT_FOUND);

	size = fsstat(ino);
	if (size <= 0) {
		printf("Failed to fsstat %s ino: %d\n", loader_path, ino);
		return (EFI_INVALID_PARAMETER);
	}

	if ((status = bs->AllocatePool(EfiLoaderData, size, &buf)) !=
	    EFI_SUCCESS) {
		printf("Failed to allocate read buffer (%lu)\n",
		    EFI_ERROR_CODE(status));
		return (status);
	}

	read = fsread(ino, buf, size);
	if ((size_t)read != size) {
		printf("Failed to read %s (%zd != %zu)\n", loader_path, read,
		    size);
		(void)bs->FreePool(buf);
		return (EFI_INVALID_PARAMETER);
	}

	*bufp = buf;
	*bufsize = size;

	return (EFI_SUCCESS);
}

static EFI_STATUS
load(const char *loader_path, dev_info_t **devinfop, void **buf,
    size_t *bufsize)
{
	dev_info_t *dev;
	EFI_STATUS status;

	for (dev = devices; dev != NULL; dev = dev->next) {
		status = try_load(dev, loader_path, buf, bufsize);
		if (status == EFI_SUCCESS) {
			*devinfop = dev;
			return (EFI_SUCCESS);
		} else if (status != EFI_NOT_FOUND) {
			return (status);
		}
	}

	return (EFI_NOT_FOUND);
}

static void
status()
{
	int i;
	dev_info_t *dev;

	for (dev = devices, i = 0; dev != NULL; dev = dev->next, i++)
		;

	printf("%s found ", ufs_module.name);
	switch (i) {
	case 0:
		printf("no partitions\n");
		break;
	case 1:
		printf("%d partition\n", i);
		break;
	default:
		printf("%d partitions\n", i);
	}
}

const boot_module_t ufs_module =
{
	.name = "UFS",
	.probe = probe,
	.load = load,
	.status = status
};
