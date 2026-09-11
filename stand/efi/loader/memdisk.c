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
#include <bootstrap.h>
#include <dev_net.h>
#include <efilib.h>
#include <Protocol/RamDisk.h>
#include "decompress.h"
#include <ipxe_download.h>
#include <sys/_param.h>

#define ULL(x) ((unsigned long long)(x))
#define DOWNLOAD_BUFSIZE	(64 * 1024)

static EFI_GUID ipxeGuid = IPXE_DOWNLOAD_PROTOCOL_GUID;
static EFI_GUID ramdiskGuid = EFI_RAM_DISK_PROTOCOL_GUID;
static EFI_GUID virtual_disk_guid = EFI_VIRTUAL_DISK_GUID;
static EFI_GUID virtual_cd_guid = EFI_VIRTUAL_CD_GUID;

static IPXE_DOWNLOAD_PROTOCOL *ipxe_download;
static EFI_RAM_DISK_PROTOCOL *ram_disk;

static bool
download_cancel_requested(void)
{
	int c;

	if (!ischar())
		return (false);
	c = getchar();
	return (c == '\033');
}

struct dl_state;
typedef struct dl_state dl_state;

static struct dl_state
{
	bool in_progress;
	size_t size;
	EFI_STATUS status;
	decomp_state *dctx;
	bool complete;
} dl;

static void
download_cleanup(dl_state *ctx)
{
	if (ctx->dctx)
		decomp_fini(ctx->dctx, true);
	ctx->in_progress = false;
}

static EFI_STATUS
download_chunk(dl_state *ctx, void *buffer, size_t length, size_t offset)
{
	decomp_state *dctx = ctx->dctx;
	enum step_return sr;

	if (offset == 0 && length == 0) {
		printf("Starting the download\n");
		return (EFI_SUCCESS);
	}

	/*
	 * Make a note of the size when we're hinted about it.
	 */
	if (length == 0) {
		printf("We know we will download %llu bytes\n", ULL(offset));
		ctx->size = offset;
		ctx->status = EFI_SUCCESS;
		return (EFI_SUCCESS);
	}

	/*
	 * Peek into the first chunk to see the format of the data.
	 */
	if (offset == 0) {
		dctx = decomp_init(buffer, length, ctx->size);
		if (dctx == NULL) {
			ctx->in_progress = false;
			ctx->status = EFI_VOLUME_CORRUPTED;
			return (ctx->status);
		}
		ctx->dctx = dctx;
	}

	sr = decomp_step(dctx, buffer, length, offset);
	if (sr == err) {
		printf("Error on download\n");
		return (EFI_VOLUME_CORRUPTED);
	}
	ctx->complete = (sr == done);

	unsigned long long sofar = offset + length;
#define MB  1000000
	if (sofar / MB != offset / MB) {
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

static EFI_STATUS EFIAPI
download_data(IN VOID *Context, IN VOID *Buffer, IN UINTN BufferLength,
    IN UINTN FileOffset)
{

	return (download_chunk(Context, Buffer, BufferLength, FileOffset));
}

static void EFIAPI
download_finish(IN VOID *Context, IN EFI_STATUS Status)
{
	dl_state *ctx = Context;

	ctx->in_progress = false;
	ctx->status = Status;
	if (ctx->dctx != NULL && !EFI_ERROR(Status))
		decomp_fini(ctx->dctx, false);
}

static int
fallback_to_md(EFI_PHYSICAL_ADDRESS pa, size_t len)
{
	int unit;

	unit = md_register((void *)(uintptr_t)pa, len, MD_FLAG_KERNEL);
	if (unit < 0) {
		printf("Could not register downloaded image as an md: %s\n",
		    strerror(errno));
		return (errno);
	}
	setenv("uefi_ignore_boot_mgr", "true", 1);
	return (0);
}

int
download_md_image(const char *url)
{
	struct stat sb;
	dl_state ctx;
	uint8_t *buf;
	size_t offset;
	ssize_t nread;
	int error, fd;

	fd = open(url, O_RDONLY);
	if (fd < 0)
		return (errno);
	if (fstat(fd, &sb) != 0) {
		error = errno;
		goto out_close;
	}
	if (sb.st_size <= 0 || (uintmax_t)sb.st_size > SIZE_MAX) {
		error = EINVAL;
		goto out_close;
	}

	buf = malloc(DOWNLOAD_BUFSIZE);
	if (buf == NULL) {
		error = ENOMEM;
		goto out_close;
	}
	memset(&ctx, 0, sizeof(ctx));
	ctx.size = sb.st_size;
	offset = 0;
	printf("Press Esc to cancel.\n");
	while ((nread = read(fd, buf, DOWNLOAD_BUFSIZE)) > 0) {
		if (download_cancel_requested()) {
			printf("\nDownload cancelled.\n");
			error = ECANCELED;
			goto out_decomp;
		}
		if (EFI_ERROR(download_chunk(&ctx, buf, nread, offset))) {
			error = EIO;
			goto out_decomp;
		}
		offset += nread;
	}
	if (nread < 0) {
		error = errno;
		goto out_decomp;
	}
	if (offset != ctx.size || !ctx.complete) {
		error = EIO;
		goto out_decomp;
	}

	decomp_fini(ctx.dctx, false);
	error = fallback_to_md(decomp_buffer(ctx.dctx),
	    decomp_buffer_length(ctx.dctx));
	goto out_free;

out_decomp:
	if (ctx.dctx != NULL)
		decomp_fini(ctx.dctx, true);
out_free:
	free(buf);
out_close:
	close(fd);
	return (error);
}

void
maybe_download_initmd(void)
{
	struct devdesc dev;
	const char *url;
	int error;

	if (efi_find_handle(&efinet_dev, 0) == NULL)
		return;

	memset(&dev, 0, sizeof(dev));
	dev.d_dev = &efinet_dev;
	dev.d_unit = 0;
	error = net_configure(&dev);
	if (error != 0) {
		printf("Could not configure net0 for initmd: %s\n",
		    strerror(error));
		return;
	}

	url = getenv("dhcp.initmd");
	if (url == NULL || *url == '\0')
		return;

	printf("Downloading initmd from %s\n", url);
	error = download_md_image(url);
	if (error != 0 && error != ECANCELED)
		printf("Could not download initmd: %s\n", strerror(error));
}

static void
do_download_ramdisk(CHAR8 *url, bool is_disk)
{
	EFI_STATUS Status;
	EFI_GUID disk_type = is_disk ? virtual_disk_guid : virtual_cd_guid;
	EFI_DEVICE_PATH_PROTOCOL *ram_disk_path;
	IPXE_DOWNLOAD_FILE token;
	dl_state *ctx = &dl;
	int error;

	printf("Downloading %s as a %s\n", url, is_disk ? "disk" : "cd");
	printf("Press Esc to cancel.\n");
	memset(ctx, 0, sizeof(*ctx));
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
		if (!ctx->in_progress)
			break;
		if (download_cancel_requested()) {
			printf("\nCancelling download...\n");
			Status = ipxe_download->Abort(ipxe_download, token,
			    EFI_ABORTED);
			if (EFI_ERROR(Status)) {
				printf("Could not cancel download %u\n",
				    (unsigned)Status);
			}
		}
	}
	if (ctx->status == EFI_ABORTED) {
		printf("Download cancelled.\n");
		download_cleanup(ctx);
		return;
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

	/* ram_disk will be NULL if this fails */
	BS->LocateProtocol(&ramdiskGuid, NULL, (void**)&ram_disk);

	/*
	 * If there's no ram_disk protocol installed in this firmware, do the
	 * next best thing by saving a pointer and using that later.
	 */
	if (ram_disk == NULL) {
		printf("No RamDisk protocol, falling back to md image\n");
		error = fallback_to_md(decomp_buffer(ctx->dctx), decomp_buffer_length(ctx->dctx));
		if (error) {
			printf("Failed to register as an MD device\n");
			download_cleanup(ctx);
		}
		return;
	}

	/*
	 * Register the RamDisk with UEFI. This registers it so the rest of the
	 * boot loader can see it as a block device.
	 */
	Status = ram_disk->Register(decomp_buffer(ctx->dctx), decomp_buffer_length(ctx->dctx),
	    &disk_type, NULL, &ram_disk_path);
	if (EFI_ERROR(Status)) {
		printf("failed to register ram disk %u, falling back to md image\n", (unsigned)Status);
		error = fallback_to_md(decomp_buffer(ctx->dctx), decomp_buffer_length(ctx->dctx));
		if (error) {
			printf("Failed to register as an MD device\n");
			download_cleanup(ctx);
		}
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
 * present, otherwise try to download that image. Returns true when we've tried
 * to download an image, whether successful or not.
 *
 * Open Question: Do we want some way to chain boot into the /boot/loader.efi or
 * \efi\boot\bootXXXXX.efi inside the ram disk we load? If so, how do we keep
 * from infinite chainbooting? Also, I don't understand the load it but don't save
 * it option...
 */
bool
maybe_download_ramdisk(int argc, CHAR16 **argv)
{
	char var[256];
	EFI_STATUS status;

	status = BS->LocateProtocol(&ipxeGuid, NULL, (void **)&ipxe_download);
	if (EFI_ERROR(status)) {
		ipxe_download = NULL;
		return (false);
	}

	for (int i = 0; i < argc; i++) {
		cpy16to8(argv[i], var, sizeof(var));
		if (strncmp(var, "memdisk=", 8) == 0) {
			do_download_ramdisk(var + 8, true);
			return (true);
		}
		if (strncmp(var, "memcd=", 6) == 0) {
			do_download_ramdisk(var + 6, false);
			return (true);
		}
	}
	return (false);
}
