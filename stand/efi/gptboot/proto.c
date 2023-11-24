/*-
 * Copyright (c) 2019 Netflix, Inc
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

#include <sys/param.h>
#include <machine/elf.h>
#include <machine/stdarg.h>
#include <stand.h>

#include <efi.h>
#include <eficonsctl.h>
#include <efichar.h>

#include "boot_module.h"
#include "paths.h"
#include "proto.h"

#include "gpt.h"
#include <sys/gpt.h>
static const uuid_t freebsd_ufs_uuid = GPT_ENT_TYPE_FREEBSD_UFS;
static char secbuf[4096] __aligned(4096);
static struct dsk dsk;
static dev_info_t *devices = NULL;
static dev_info_t *raw_device = NULL;

static EFI_GUID BlockIoProtocolGUID = BLOCK_IO_PROTOCOL;
static EFI_GUID DevicePathGUID = DEVICE_PATH_PROTOCOL;

/*
 * Shim routine for the gpt code to read in the gpt table. The
 * devinfo is always going to be for the raw device.
 */
int
drvread(struct dsk *dskp, void *buf, daddr_t lba, unsigned nblk)
{
	int size;
	EFI_STATUS status;
	dev_info_t *devinfo = (dev_info_t *)dskp->devinfo;
	EFI_BLOCK_IO *dev = devinfo->dev;

	lba = lba / (dev->Media->BlockSize / DEV_BSIZE);
	size = nblk * DEV_BSIZE;

	status = dev->ReadBlocks(dev, dev->Media->MediaId, lba, size, buf);
	if (status != EFI_SUCCESS) {
		DPRINTF("dskread: failed dev: %p, id: %u, lba: %ju, size: %d, "
		    "status: %lu\n", devinfo->dev,
		    dev->Media->MediaId, (uintmax_t)lba, size,
		    EFI_ERROR_CODE(status));
		return (-1);
	}

	return (0);
}

/*
 * Shim routine for the gpt code to write in the gpt table. The
 * devinfo is always going to be for the raw device.
 */
int
drvwrite(struct dsk *dskp, void *buf, daddr_t lba, unsigned nblk)
{
	int size;
	EFI_STATUS status;
	dev_info_t *devinfo = (dev_info_t *)dskp->devinfo;
	EFI_BLOCK_IO *dev = devinfo->dev;

	if (dev->Media->ReadOnly)
		return -1;

	lba = lba / (dev->Media->BlockSize / DEV_BSIZE);
	size = nblk * DEV_BSIZE;

	status = dev->WriteBlocks(dev, dev->Media->MediaId, lba, size, buf);
	if (status != EFI_SUCCESS) {
		DPRINTF("dskread: failed dev: %p, id: %u, lba: %ju, size: %d, "
		    "status: %lu\n", devinfo->dev,
		    dev->Media->MediaId, (uintmax_t)lba, size,
		    EFI_ERROR_CODE(status));
		return (-1);
	}

	return (0);
}

/*
 * Return the number of LBAs the drive has.
 */
uint64_t
drvsize(struct dsk *dskp)
{
	dev_info_t *devinfo = (dev_info_t *)dskp->devinfo;
	EFI_BLOCK_IO *dev = devinfo->dev;

	return (dev->Media->LastBlock + 1);
}

static int
partition_number(EFI_DEVICE_PATH *devpath)
{
	EFI_DEVICE_PATH *md;
	HARDDRIVE_DEVICE_PATH *hd;

	md = efi_devpath_last_node(devpath);
	if (md == NULL)
		return (-1);
	if (DevicePathSubType(md) != MEDIA_HARDDRIVE_DP)
		return (-1);
	hd = (HARDDRIVE_DEVICE_PATH *)md;
	return (hd->PartitionNumber);
}

/*
 * Find the raw partition for the imgpath and save it
 */
static void
probe_handle(EFI_HANDLE h, EFI_DEVICE_PATH *imgpath)
{
	dev_info_t *devinfo;
	EFI_BLOCK_IO *blkio;
	EFI_DEVICE_PATH *devpath, *trimmed = NULL;
	EFI_STATUS status;

	/* Figure out if we're dealing with an actual partition. */
	status = OpenProtocolByHandle(h, &DevicePathGUID, (void **)&devpath);
	if (status != EFI_SUCCESS)
		return;
#ifdef EFI_DEBUG
	{
		CHAR16 *text = efi_devpath_name(devpath);
		DPRINTF("probing: %S ", text);
		efi_free_devpath_name(text);
	}
#endif
	/*
	 * The RAW device is the same as the imgpath with the last
	 * MEDIA_DEVICE bit trimmed off. imgpath will end with the
	 * MEDIA_DEVICE for the ESP we booted off of.
	 */
	if (!efi_devpath_same_disk(imgpath, devpath)) {
		trimmed = efi_devpath_trim(imgpath);
		if (!efi_devpath_match(trimmed, devpath)) {
			free(trimmed);
			DPRINTF("Not the same disk\n");
			return;
		}
	}
	status = OpenProtocolByHandle(h, &BlockIoProtocolGUID, (void **)&blkio);
	if (status != EFI_SUCCESS) {
		DPRINTF("Can't get the block I/O protocol block\n");
		return;
	}
	devinfo = malloc(sizeof(*devinfo));
	if (devinfo == NULL) {
		DPRINTF("Failed to allocate devinfo\n");
		return;
	}
	devinfo->dev = blkio;
	devinfo->devpath = devpath;
	devinfo->devhandle = h;
	devinfo->preferred = 1;
	devinfo->next = NULL;
	devinfo->devdata = NULL;
	if (trimmed == NULL) {
		DPRINTF("Found partition %d\n", partition_number(devpath));
		add_device(&devices, devinfo);
	} else {
		free(trimmed);
		DPRINTF("Found raw device\n");
		if (raw_device) {
			printf(BOOTPROG": Found two raw devices, inconceivable?\n");
			return;
		}
		raw_device = devinfo;
	}
}

static void
probe_handles(EFI_HANDLE *handles, UINTN nhandles, EFI_DEVICE_PATH *imgpath)
{
	UINTN i;

	for (i = 0; i < nhandles; i++)
		probe_handle(handles[i], imgpath);
}

static dev_info_t *
find_partition(int part)
{
	dev_info_t *dev;

	if (part == 0)
		return (NULL);
	for (dev = devices; dev != NULL; dev = dev->next)
		if (partition_number(dev->devpath) == part)
			break;
	return (dev);
}

void
choice_protocol(EFI_HANDLE *handles, UINTN nhandles, EFI_DEVICE_PATH *imgpath)
{
	const boot_module_t *mod = &ufs_module;
	dev_info_t *bootdev;
	void *loaderbuf;
	size_t loadersize;
	int parts;
	const char *fn = PATH_LOADER_EFI;

	/*
	 * Probe the provided handles to find the partitions that
	 * are on the same drive.
	 */
	probe_handles(handles, nhandles, imgpath);
	dsk.devinfo = raw_device;
	if (dsk.devinfo == NULL) {
		printf(BOOTPROG": unable to find raw disk to read gpt\n");
		return;
	}

	/*
	 * Read in the GPT table, and then find the right partition.
	 * gptread, gptfind and gptfaileboot are shared with the
	 * BIOS version of the gptboot program.
	 */
	if (gptread(&dsk, secbuf) == -1) {
		printf(BOOTPROG ": unable to load GPT\n");
		return;
	}
	// XXX:
	// real gptboot can parse a command line before trying this loop.
	// But since we don't parse anything at all, hard wire the partition
	// to be -1 (meaning look for the next one).
	parts = 0;
	while (gptfind(&freebsd_ufs_uuid, &dsk, -1) != -1) {
		parts++;
		bootdev = find_partition(dsk.part);
		if (bootdev == NULL) {
			printf(BOOTPROG": Can't find partition %d\n",
			    dsk.part);
			goto next;
		}
		if (mod->load(fn, bootdev, &loaderbuf, &loadersize) !=
		    EFI_SUCCESS) {
			printf(BOOTPROG": Can't load %s from partition %d\n",
			    fn, dsk.part);
			goto next;
		}
		try_boot(mod, bootdev, loaderbuf, loadersize);
next:
		gptbootfailed(&dsk);
	}
	if (parts == 0)
		printf("%s: no UFS partition was found\n", BOOTPROG);
}
