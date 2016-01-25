/*-
 * Copyright (c) 1998 Robert Nordier
 * All rights reserved.
 * Copyright (c) 2001 Robert Drehmel
 * All rights reserved.
 * Copyright (c) 2014 Nathan Whitehorn
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are freely
 * permitted provided that the above copyright notice and this
 * paragraph and the following disclaimer are duplicated in all
 * such forms.
 *
 * This software is provided "AS IS" and without any express or
 * implied warranties, including, without limitation, the implied
 * warranties of merchantability and fitness for a particular
 * purpose.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/dirent.h>
#include <machine/elf.h>
#include <machine/stdarg.h>
#include <stand.h>

#include <efi.h>
#include <eficonsctl.h>

#define _PATH_LOADER	"/boot/loader.efi"
#define _PATH_KERNEL	"/boot/kernel/kernel"

#define BSIZEMAX	16384

void panic(const char *fmt, ...) __dead2;
void putchar(int c);

static int domount(EFI_DEVICE_PATH *device, EFI_BLOCK_IO *blkio, int quiet);
static void load(const char *fname);

EFI_SYSTEM_TABLE *systab;
EFI_HANDLE *image;

static EFI_GUID BlockIoProtocolGUID = BLOCK_IO_PROTOCOL;
static EFI_GUID DevicePathGUID = DEVICE_PATH_PROTOCOL;
static EFI_GUID LoadedImageGUID = LOADED_IMAGE_PROTOCOL;
static EFI_GUID ConsoleControlGUID = EFI_CONSOLE_CONTROL_PROTOCOL_GUID;

static EFI_BLOCK_IO *bootdev;
static EFI_DEVICE_PATH *bootdevpath;
static EFI_HANDLE *bootdevhandle;

EFI_STATUS efi_main(EFI_HANDLE Ximage, EFI_SYSTEM_TABLE* Xsystab)
{
	EFI_HANDLE handles[128];
	EFI_BLOCK_IO *blkio;
	UINTN i, nparts = sizeof(handles), cols, rows, max_dim, best_mode;
	EFI_STATUS status;
	EFI_DEVICE_PATH *devpath;
	EFI_BOOT_SERVICES *BS;
	EFI_CONSOLE_CONTROL_PROTOCOL *ConsoleControl = NULL;
	SIMPLE_TEXT_OUTPUT_INTERFACE *conout = NULL;
	char *path = _PATH_LOADER;

	systab = Xsystab;
	image = Ximage;

	BS = systab->BootServices;
	status = BS->LocateProtocol(&ConsoleControlGUID, NULL,
	    (VOID **)&ConsoleControl);
	if (status == EFI_SUCCESS)
		(void)ConsoleControl->SetMode(ConsoleControl,
		    EfiConsoleControlScreenText);
	/*
	 * Reset the console and find the best text mode.
	 */
	conout = systab->ConOut;
	conout->Reset(conout, TRUE);
	max_dim = best_mode = 0;
	for (i = 0; ; i++) {
		status = conout->QueryMode(conout, i, &cols, &rows);
		if (EFI_ERROR(status))
			break;
		if (cols * rows > max_dim) {
			max_dim = cols * rows;
			best_mode = i;
		}
	}
	if (max_dim > 0)
		conout->SetMode(conout, best_mode);
	conout->EnableCursor(conout, TRUE);
	conout->ClearScreen(conout);

	printf(" \n>> FreeBSD EFI boot block\n");
	printf("   Loader path: %s\n", path);

	status = systab->BootServices->LocateHandle(ByProtocol,
	    &BlockIoProtocolGUID, NULL, &nparts, handles);
	nparts /= sizeof(handles[0]);

	for (i = 0; i < nparts; i++) {
		status = systab->BootServices->HandleProtocol(handles[i],
		    &DevicePathGUID, (void **)&devpath);
		if (EFI_ERROR(status))
			continue;

		while (!IsDevicePathEnd(NextDevicePathNode(devpath)))
			devpath = NextDevicePathNode(devpath);

		status = systab->BootServices->HandleProtocol(handles[i],
		    &BlockIoProtocolGUID, (void **)&blkio);
		if (EFI_ERROR(status))
			continue;

		if (!blkio->Media->LogicalPartition)
			continue;

		if (domount(devpath, blkio, 1) >= 0)
			break;
	}

	if (i == nparts)
		panic("No bootable partition found");

	bootdevhandle = handles[i];
	load(path);

	panic("Load failed");

	return EFI_SUCCESS;
}

static int
dskread(void *buf, u_int64_t lba, int nblk)
{
	EFI_STATUS status;
	int size;

	lba = lba / (bootdev->Media->BlockSize / DEV_BSIZE);
	size = nblk * DEV_BSIZE;
	status = bootdev->ReadBlocks(bootdev, bootdev->Media->MediaId, lba,
	    size, buf);

	if (EFI_ERROR(status))
		return (-1);

	return (0);
}

#include "ufsread.c"

static ssize_t
fsstat(ufs_ino_t inode)
{
#ifndef UFS2_ONLY
	static struct ufs1_dinode dp1;
	ufs1_daddr_t addr1;
#endif
#ifndef UFS1_ONLY
	static struct ufs2_dinode dp2;
#endif
	static struct fs fs;
	static ufs_ino_t inomap;
	char *blkbuf;
	void *indbuf;
	size_t n, nb, size, off, vboff;
	ufs_lbn_t lbn;
	ufs2_daddr_t addr2, vbaddr;
	static ufs2_daddr_t blkmap, indmap;
	u_int u;

	blkbuf = dmadat->blkbuf;
	indbuf = dmadat->indbuf;
	if (!dsk_meta) {
		inomap = 0;
		for (n = 0; sblock_try[n] != -1; n++) {
			if (dskread(dmadat->sbbuf, sblock_try[n] / DEV_BSIZE,
			    SBLOCKSIZE / DEV_BSIZE))
				return -1;
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
			    fs.fs_bsize >= sizeof(struct fs))
				break;
		}
		if (sblock_try[n] == -1) {
			return -1;
		}
		dsk_meta++;
	} else
		memcpy(&fs, dmadat->sbbuf, sizeof(struct fs));
	if (!inode)
		return 0;
	if (inomap != inode) {
		n = IPERVBLK(&fs);
		if (dskread(blkbuf, INO_TO_VBA(&fs, n, inode), DBPERVBLK))
			return -1;
		n = INO_TO_VBO(n, inode);
#if defined(UFS1_ONLY)
		memcpy(&dp1, (struct ufs1_dinode *)blkbuf + n,
		    sizeof(struct ufs1_dinode));
#elif defined(UFS2_ONLY)
		memcpy(&dp2, (struct ufs2_dinode *)blkbuf + n,
		    sizeof(struct ufs2_dinode));
#else
		if (fs.fs_magic == FS_UFS1_MAGIC)
			memcpy(&dp1, (struct ufs1_dinode *)blkbuf + n,
			    sizeof(struct ufs1_dinode));
		else
			memcpy(&dp2, (struct ufs2_dinode *)blkbuf + n,
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

static int
domount(EFI_DEVICE_PATH *device, EFI_BLOCK_IO *blkio, int quiet)
{

	dmadat = &__dmadat;
	bootdev = blkio;
	bootdevpath = device;
	if (fsread(0, NULL, 0)) {
		if (!quiet)
			printf("domount: can't read superblock\n");
		return (-1);
	}
	if (!quiet)
		printf("Succesfully mounted UFS filesystem\n");
	return (0);
}

static void
load(const char *fname)
{
	ufs_ino_t ino;
	EFI_STATUS status;
	EFI_HANDLE loaderhandle;
	EFI_LOADED_IMAGE *loaded_image;
	void *buffer;
	size_t bufsize;

	if ((ino = lookup(fname)) == 0) {
		printf("File %s not found\n", fname);
		return;
	}

	bufsize = fsstat(ino);
	status = systab->BootServices->AllocatePool(EfiLoaderData,
	    bufsize, &buffer);
	fsread(ino, buffer, bufsize);

	/* XXX: For secure boot, we need our own loader here */
	status = systab->BootServices->LoadImage(TRUE, image, bootdevpath,
	    buffer, bufsize, &loaderhandle);
	if (EFI_ERROR(status))
		printf("LoadImage failed with error %lu\n",
		    EFI_ERROR_CODE(status));

	status = systab->BootServices->HandleProtocol(loaderhandle,
	    &LoadedImageGUID, (VOID**)&loaded_image);
	if (EFI_ERROR(status))
		printf("HandleProtocol failed with error %lu\n",
		    EFI_ERROR_CODE(status));

	loaded_image->DeviceHandle = bootdevhandle;

	status = systab->BootServices->StartImage(loaderhandle, NULL, NULL);
	if (EFI_ERROR(status))
		printf("StartImage failed with error %lu\n",
		    EFI_ERROR_CODE(status));
}

void
panic(const char *fmt, ...)
{
	va_list ap;

	printf("panic: ");
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	printf("\n");

	while (1) {}
}

void
putchar(int c)
{
	CHAR16 buf[2];

	if (c == '\n') {
		buf[0] = '\r';
		buf[1] = 0;
		systab->ConOut->OutputString(systab->ConOut, buf);
	}
	buf[0] = c;
	buf[1] = 0;
	systab->ConOut->OutputString(systab->ConOut, buf);
}
