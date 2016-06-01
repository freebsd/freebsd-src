/*-
 * Copyright (c) 2010 Marcel Moolenaar
 * All rights reserved.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/time.h>
#include <stddef.h>
#include <stdarg.h>

#include <bootstrap.h>

#include <efi.h>
#include <efilib.h>
#include <efiprot.h>

static EFI_GUID blkio_guid = BLOCK_IO_PROTOCOL;

static int efipart_init(void);
static int efipart_strategy(void *, int, daddr_t, size_t, size_t, char *,
    size_t *);
static int efipart_realstrategy(void *, int, daddr_t, size_t, size_t, char *,
    size_t *);
static int efipart_open(struct open_file *, ...);
static int efipart_close(struct open_file *);
static void efipart_print(int);

struct devsw efipart_dev = {
	.dv_name = "part",
	.dv_type = DEVT_DISK,
	.dv_init = efipart_init,
	.dv_strategy = efipart_strategy,
	.dv_open = efipart_open,
	.dv_close = efipart_close,
	.dv_ioctl = noioctl,
	.dv_print = efipart_print,
	.dv_cleanup = NULL
};

/*
 * info structure to support bcache
 */
struct pdinfo {
	int	pd_unit;	/* unit number */
	int	pd_open;	/* reference counter */
	void	*pd_bcache;	/* buffer cache data */
};
static struct pdinfo *pdinfo;
static int npdinfo = 0;

#define PD(dev)         (pdinfo[(dev)->d_unit])

static int
efipart_init(void) 
{
	EFI_BLOCK_IO *blkio;
	EFI_DEVICE_PATH *devpath, *devpathcpy, *tmpdevpath, *node;
	EFI_HANDLE *hin, *hout, *aliases, handle;
	EFI_STATUS status;
	UINTN sz;
	u_int n, nin, nout;
	int err;

	sz = 0;
	hin = NULL;
	status = BS->LocateHandle(ByProtocol, &blkio_guid, 0, &sz, 0);
	if (status == EFI_BUFFER_TOO_SMALL) {
		hin = (EFI_HANDLE *)malloc(sz * 3);
		status = BS->LocateHandle(ByProtocol, &blkio_guid, 0, &sz,
		    hin);
		if (EFI_ERROR(status))
			free(hin);
	}
	if (EFI_ERROR(status))
		return (efi_status_to_errno(status));

	/* Filter handles to only include FreeBSD partitions. */
	nin = sz / sizeof(EFI_HANDLE);
	hout = hin + nin;
	aliases = hout + nin;
	nout = 0;

	bzero(aliases, nin * sizeof(EFI_HANDLE));
	pdinfo = malloc(nin * sizeof(*pdinfo));
	if (pdinfo == NULL)
		return (ENOMEM);

	for (n = 0; n < nin; n++) {
		devpath = efi_lookup_devpath(hin[n]);
		if (devpath == NULL) {
			continue;
		}

		status = BS->HandleProtocol(hin[n], &blkio_guid,
		    (void**)&blkio);
		if (EFI_ERROR(status))
			continue;
		if (!blkio->Media->LogicalPartition)
			continue;

		/*
		 * If we come across a logical partition of subtype CDROM
		 * it doesn't refer to the CD filesystem itself, but rather
		 * to any usable El Torito boot image on it. In this case
		 * we try to find the parent device and add that instead as
		 * that will be the CD filesystem.
		 */
		node = efi_devpath_last_node(devpath);
		if (DevicePathType(node) == MEDIA_DEVICE_PATH &&
		    DevicePathSubType(node) == MEDIA_CDROM_DP) {
			devpathcpy = efi_devpath_trim(devpath);
			tmpdevpath = devpathcpy;
			status = BS->LocateDevicePath(&blkio_guid, &tmpdevpath,
			    &handle);
			free(devpathcpy);
			if (EFI_ERROR(status))
				continue;
			hout[nout] = handle;
			aliases[nout] = hin[n];
		} else
			hout[nout] = hin[n];
		nout++;
		pdinfo[npdinfo].pd_open = 0;
		pdinfo[npdinfo].pd_bcache = NULL;
		pdinfo[npdinfo].pd_unit = npdinfo;
		npdinfo++;
	}

	bcache_add_dev(npdinfo);
	err = efi_register_handles(&efipart_dev, hout, aliases, nout);
	free(hin);
	return (err);
}

static void
efipart_print(int verbose)
{
	char line[80];
	EFI_BLOCK_IO *blkio;
	EFI_HANDLE h;
	EFI_STATUS status;
	u_int unit;

	pager_open();
	for (unit = 0, h = efi_find_handle(&efipart_dev, 0);
	    h != NULL; h = efi_find_handle(&efipart_dev, ++unit)) {
		sprintf(line, "    %s%d:", efipart_dev.dv_name, unit);
		if (pager_output(line))
			break;

		status = BS->HandleProtocol(h, &blkio_guid, (void **)&blkio);
		if (!EFI_ERROR(status)) {
			sprintf(line, "    %llu blocks",
			    (unsigned long long)(blkio->Media->LastBlock + 1));
			if (pager_output(line))
				break;
			if (blkio->Media->RemovableMedia)
				if (pager_output(" (removable)"))
					break;
		}
		if (pager_output("\n"))
			break;
	}
	pager_close();
}

static int
efipart_open(struct open_file *f, ...)
{
	va_list args;
	struct devdesc *dev;
	EFI_BLOCK_IO *blkio;
	EFI_HANDLE h;
	EFI_STATUS status;

	va_start(args, f);
	dev = va_arg(args, struct devdesc*);
	va_end(args);

	h = efi_find_handle(&efipart_dev, dev->d_unit);
	if (h == NULL)
		return (EINVAL);

	status = BS->HandleProtocol(h, &blkio_guid, (void **)&blkio);
	if (EFI_ERROR(status))
		return (efi_status_to_errno(status));

	if (!blkio->Media->MediaPresent)
		return (EAGAIN);

	dev->d_opendata = blkio;
	PD(dev).pd_open++;
	if (PD(dev).pd_bcache == NULL)
		PD(dev).pd_bcache = bcache_allocate();
	return (0);
}

static int
efipart_close(struct open_file *f)
{
	struct devdesc *dev;

	dev = (struct devdesc *)(f->f_devdata);
	if (dev->d_opendata == NULL)
		return (EINVAL);

	dev->d_opendata = NULL;
	PD(dev).pd_open--;
	if (PD(dev).pd_open == 0) {
		bcache_free(PD(dev).pd_bcache);
		PD(dev).pd_bcache = NULL;
	}
	return (0);
}

/*
 * efipart_readwrite()
 * Internal equivalent of efipart_strategy(), which operates on the
 * media-native block size. This function expects all I/O requests
 * to be within the media size and returns an error if such is not
 * the case.
 */
static int
efipart_readwrite(EFI_BLOCK_IO *blkio, int rw, daddr_t blk, daddr_t nblks,
    char *buf)
{
	EFI_STATUS status;

	if (blkio == NULL)
		return (ENXIO);
	if (blk < 0 || blk > blkio->Media->LastBlock)
		return (EIO);
	if ((blk + nblks - 1) > blkio->Media->LastBlock)
		return (EIO);

	switch (rw) {
	case F_READ:
		status = blkio->ReadBlocks(blkio, blkio->Media->MediaId, blk,
		    nblks * blkio->Media->BlockSize, buf);
		break;
	case F_WRITE:
		if (blkio->Media->ReadOnly)
			return (EROFS);
		status = blkio->WriteBlocks(blkio, blkio->Media->MediaId, blk,
		    nblks * blkio->Media->BlockSize, buf);
		break;
	default:
		return (ENOSYS);
	}

	if (EFI_ERROR(status))
		printf("%s: rw=%d, status=%lu\n", __func__, rw, (u_long)status);
	return (efi_status_to_errno(status));
}

static int
efipart_strategy(void *devdata, int rw, daddr_t blk, size_t offset,
    size_t size, char *buf, size_t *rsize)
{
	struct bcache_devdata bcd;
	struct devdesc *dev;

	dev = (struct devdesc *)devdata;
	bcd.dv_strategy = efipart_realstrategy;
	bcd.dv_devdata = devdata;
	bcd.dv_cache = PD(dev).pd_bcache;
	return (bcache_strategy(&bcd, rw, blk, offset, size,
	    buf, rsize));
}

static int
efipart_realstrategy(void *devdata, int rw, daddr_t blk, size_t offset,
    size_t size, char *buf, size_t *rsize)
{
	struct devdesc *dev = (struct devdesc *)devdata;
	EFI_BLOCK_IO *blkio;
	off_t off;
	char *blkbuf;
	size_t blkoff, blksz;
	int error;

	if (dev == NULL || blk < 0)
		return (EINVAL);

	blkio = dev->d_opendata;
	if (blkio == NULL)
		return (ENXIO);

	if (size == 0 || (size % 512) != 0)
		return (EIO);

	off = blk * 512;
	/* make sure we don't read past disk end */
	if ((off + size) / blkio->Media->BlockSize - 1 >
	    blkio->Media->LastBlock) {
		size = blkio->Media->LastBlock + 1 -
		    off / blkio->Media->BlockSize;
		size = size * blkio->Media->BlockSize;
	}

	if (rsize != NULL)
		*rsize = size;

        if ((size % blkio->Media->BlockSize == 0) &&
	    ((blk * 512) % blkio->Media->BlockSize == 0))
                return (efipart_readwrite(blkio, rw,
		    blk * 512 / blkio->Media->BlockSize,
		    size / blkio->Media->BlockSize, buf));

	/*
	 * The block size of the media is not a multiple of I/O.
	 */
	blkbuf = malloc(blkio->Media->BlockSize);
	if (blkbuf == NULL)
		return (ENOMEM);

	error = 0;
	blk = off / blkio->Media->BlockSize;
	blkoff = off % blkio->Media->BlockSize;
	blksz = blkio->Media->BlockSize - blkoff;
	while (size > 0) {
		error = efipart_readwrite(blkio, rw, blk, 1, blkbuf);
		if (error)
			break;
		if (size < blksz)
			blksz = size;
		bcopy(blkbuf + blkoff, buf, blksz);
		buf += blksz;
		size -= blksz;
		blk++;
		blkoff = 0;
		blksz = blkio->Media->BlockSize;
	}

	free(blkbuf);
	return (error);
}
