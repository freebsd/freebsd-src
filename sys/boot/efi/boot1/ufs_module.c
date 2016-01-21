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

static struct dmadat __dmadat;

static int
init_dev(dev_info_t* dev)
{

	devinfo = dev;
	dmadat = &__dmadat;

	return fsread(0, NULL, 0);
}

static EFI_STATUS
probe(dev_info_t* dev)
{

	if (init_dev(dev) < 0)
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

	if (init_dev(dev) < 0)
		return (EFI_UNSUPPORTED);

	if ((ino = lookup(loader_path)) == 0)
		return (EFI_NOT_FOUND);

	if (fsread_size(ino, NULL, 0, &size) < 0 || size <= 0) {
		printf("Failed to read size of '%s' ino: %d\n", loader_path,
		    ino);
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
		printf("Failed to read '%s' (%zd != %zu)\n", loader_path, read,
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
