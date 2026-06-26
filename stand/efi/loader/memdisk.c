/*
 * Copyright (c) 2026 Netflix, Inc. Written by Warner Losh
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Derived from memdisk_uefi.c
 * Copyright 2025 Richard Russo
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#include "loader_efi.h"
#include <efilib.h>
#include <Protocol/RamDisk.h>
#include "decompress.h"
#include <ipxe_download.h>
#include <sys/_param.h>

#define ULL(x) ((unsigned long long)(x))

static EFI_GUID ipxeGuid = IPXE_DOWNLOAD_PROTOCOL_GUID;
static EFI_GUID ramdiskGuid = EFI_RAM_DISK_PROTOCOL_GUID;
static EFI_GUID virtual_disk_guid = EFI_VIRTUAL_DISK_GUID;
static EFI_GUID virtual_cd_guid = EFI_VIRTUAL_CD_GUID;

static IPXE_DOWNLOAD_PROTOCOL *ipxe_download;
static EFI_RAM_DISK_PROTOCOL *ram_disk;

struct dl_state;
typedef struct dl_state dl_state;

static struct dl_state
{
	bool in_progress;
	size_t size;
	EFI_STATUS status;
	decomp_state *dctx;
} dl;

static void
download_cleanup(dl_state *ctx)
{
	if (ctx->dctx)
		decomp_fini(ctx->dctx, true);
	ctx->in_progress = false;
}

static EFI_STATUS EFIAPI
download_data(IN VOID *Context, IN VOID *Buffer, IN UINTN BufferLength, IN UINTN FileOffset)
{
	dl_state *ctx = Context;
	decomp_state *dctx = ctx->dctx;

	/*
	 * Make a note of the size when we're hinted about it. But once
	 * we start the download, ignore the hints.
	 */
	if (ctx->size == 0 && BufferLength == 0) {
		printf("We know we will download %llu bytes\n", ULL(FileOffset));
		ctx->size = FileOffset;
		ctx->status = EFI_SUCCESS;
		return (EFI_SUCCESS);
	}

	/*
	 * Peek into the first chunk to see the format of the data.
	 */
	if (FileOffset == 0) {
		dctx = decomp_init((uint8_t *)Buffer, (size_t)BufferLength, ctx->size);
		if (dctx == NULL) {
			ctx->in_progress = false;
			ctx->status = EFI_VOLUME_CORRUPTED;
			return (ctx->status);
		}
		ctx->dctx = dctx;
	}

	enum step_return sr = decomp_step(dctx, Buffer, BufferLength, FileOffset);
	if (sr == err) {
		printf("Error on download\n");
		decomp_fini(dctx, true);
		return (EFI_VOLUME_CORRUPTED);
	}

	unsigned long long sofar = FileOffset + BufferLength;
#define MB  1000000
	if (sofar / MB != FileOffset / MB) {
		if (ctx->size)
			printf("%dMB / %dMB (%d%%)\r",
			    (int)(sofar / MB),
			    (int)(ctx->size / MB),
			    (int)(100 * sofar / ctx->size));
		else
			printf("%dMB\r", (int)(sofar / MB));
	}
	return (EFI_SUCCESS);
}

static void EFIAPI
download_finish(IN VOID *Context, IN EFI_STATUS Status)
{
	dl_state *ctx = Context;

	ctx->in_progress = false;
	ctx->status = Status;
	if (ctx->dctx)
		decomp_fini(ctx->dctx, EFI_ERROR(Status));
}

static void
do_download_ramdisk(CHAR8 *url, bool is_disk)
{
	EFI_STATUS Status;
	EFI_GUID disk_type = is_disk ? virtual_disk_guid : virtual_cd_guid;
	EFI_DEVICE_PATH_PROTOCOL *ram_disk_path;
	IPXE_DOWNLOAD_FILE token;
	dl_state *ctx = &dl;

	Status = BS->LocateProtocol(&ipxeGuid, NULL, (void**)&ipxe_download);
	if (EFI_ERROR(Status))
		return; /* most uses won't have this, don't whine */
	Status = BS->LocateProtocol(&ramdiskGuid, NULL, (void**)&ram_disk);
	if (EFI_ERROR(Status))
		return; /* XXX whine about it? */

	printf("Downloading %s as a %s\n", url, is_disk ? "disk" : "cd");
	ctx->in_progress = true;
	Status = ipxe_download->Start(ipxe_download, url, download_data, download_finish,
	    &dl, &token);
	if (EFI_ERROR(Status)) {
		printf("Couldn't start download %u\n", (unsigned)Status);
		download_cleanup(ctx);
		return;
	}
	while (ctx->in_progress) {
		ipxe_download->Poll(ipxe_download);
	}
	if (EFI_ERROR(ctx->status)) {
		printf("Download had error %u\n", (unsigned)ctx->status);
		download_cleanup(ctx);
		return;
	}
	if (ctx->size == 0) {
		printf("Nothing downloaded\n");
		download_cleanup(ctx);
		return;
	}

	printf("\nDownloaded %llu bytes, actual size %llu -- registering ramdisk\n",
	    ULL(ctx->size), ULL(decomp_buffer_length(ctx->dctx)));

	/*
	 * Register the RamDisk with UEFI. This registers it so the rest of the
	 * boot loader can see it as a block device.
	 */
	Status = ram_disk->Register(decomp_buffer(ctx->dctx), decomp_buffer_length(ctx->dctx),
	    &disk_type, NULL, &ram_disk_path);
	if (EFI_ERROR(Status)) {
		printf("failed to register ram disk %u\n", (unsigned)Status);
		download_cleanup(ctx);
		return;
	}

	CHAR16 *text = efi_devpath_name(ram_disk_path);
	if (text != NULL) {
		CHAR8 uefi_path[1024];
		printf("Installed RAM disk as %S\n", text);

		cpy16to8(text, uefi_path, sizeof(uefi_path));
		setenv("uefi_ignore_boot_mgr", "true", 1);
		setenv("uefi_rootdev", uefi_path, 1);
		efi_free_devpath_name(text);
	} else {
		printf("Installed RAM disk to unknown device type\n");
	}
}

/*
 * Scan the command line for memdisk=url or memcd=url. Do nothing if that's not
 * present, otherwise try to download that image.
 *
 * Open Question: Do we want some way to chain boot into the /boot/loader.efi or
 * \efi\boot\bootXXXXX.efi inside the ram disk we load? If so, how do we keep
 * from infinite chainbooting? Also, I don't understand the load it but don't save
 * it option...
 */
void
maybe_download_ramdisk(int argc, CHAR16 **argv)
{
	char var[256];

	for (int i = 0; i < argc; i++) {
		cpy16to8(argv[i], var, sizeof(var));
		if (strncmp(var, "memdisk=", 8) == 0) {
			do_download_ramdisk(var + 8, true);
			return;
		}
		if (strncmp(var, "memcd=", 6) == 0) {
			do_download_ramdisk(var + 6, false);
			return;
		}
	}
	return;
}
