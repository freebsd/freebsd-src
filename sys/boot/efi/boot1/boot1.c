/*-
 * Copyright (c) 1998 Robert Nordier
 * All rights reserved.
 * Copyright (c) 2001 Robert Drehmel
 * All rights reserved.
 * Copyright (c) 2014 Nathan Whitehorn
 * All rights reserved.
 * Copyright (c) 2015 Eric McCorkle
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
#include <machine/elf.h>
#include <machine/stdarg.h>
#include <stand.h>

#include <efi.h>
#include <eficonsctl.h>

#include "boot_module.h"

#define _PATH_LOADER	"/boot/loader.efi"

static const boot_module_t *boot_modules[] =
{
#ifdef EFI_ZFS_BOOT
	&zfs_module,
#endif
#ifdef EFI_UFS_BOOT
	&ufs_module
#endif
};

#define NUM_BOOT_MODULES (sizeof(boot_modules) / sizeof(boot_module_t*))
/* The initial number of handles used to query EFI for partitions. */
#define NUM_HANDLES_INIT	24

void putchar(int c);
EFI_STATUS efi_main(EFI_HANDLE Ximage, EFI_SYSTEM_TABLE* Xsystab);

static void try_load(const boot_module_t* mod);
static EFI_STATUS probe_handle(EFI_HANDLE h);

EFI_SYSTEM_TABLE *systab;
EFI_BOOT_SERVICES *bs;
static EFI_HANDLE *image;

static EFI_GUID BlockIoProtocolGUID = BLOCK_IO_PROTOCOL;
static EFI_GUID DevicePathGUID = DEVICE_PATH_PROTOCOL;
static EFI_GUID LoadedImageGUID = LOADED_IMAGE_PROTOCOL;
static EFI_GUID ConsoleControlGUID = EFI_CONSOLE_CONTROL_PROTOCOL_GUID;

/*
 * Provide Malloc / Free backed by EFIs AllocatePool / FreePool which ensures
 * memory is correctly aligned avoiding EFI_INVALID_PARAMETER returns from
 * EFI methods.
 */
void *
Malloc(size_t len, const char *file __unused, int line __unused)
{
	void *out;

	if (bs->AllocatePool(EfiLoaderData, len, &out) == EFI_SUCCESS)
		return (out);

	return (NULL);
}

void
Free(void *buf, const char *file __unused, int line __unused)
{
	(void)bs->FreePool(buf);
}

/*
 * This function only returns if it fails to load the kernel. If it
 * succeeds, it simply boots the kernel.
 */
void
try_load(const boot_module_t *mod)
{
	size_t bufsize;
	void *buf;
	dev_info_t *dev;
	EFI_HANDLE loaderhandle;
	EFI_LOADED_IMAGE *loaded_image;
	EFI_STATUS status;

	status = mod->load(_PATH_LOADER, &dev, &buf, &bufsize);
	if (status == EFI_NOT_FOUND)
		return;

	if (status != EFI_SUCCESS) {
		printf("%s failed to load %s (%lu)\n", mod->name, _PATH_LOADER,
		    EFI_ERROR_CODE(status));
		return;
	}

	if ((status = bs->LoadImage(TRUE, image, dev->devpath, buf, bufsize,
	    &loaderhandle)) != EFI_SUCCESS) {
		printf("Failed to load image provided by %s, size: %zu, (%lu)\n",
		     mod->name, bufsize, EFI_ERROR_CODE(status));
		return;
	}

	if ((status = bs->HandleProtocol(loaderhandle, &LoadedImageGUID,
	    (VOID**)&loaded_image)) != EFI_SUCCESS) {
		printf("Failed to query LoadedImage provided by %s (%lu)\n",
		    mod->name, EFI_ERROR_CODE(status));
		return;
	}

	loaded_image->DeviceHandle = dev->devhandle;

	if ((status = bs->StartImage(loaderhandle, NULL, NULL)) !=
	    EFI_SUCCESS) {
		printf("Failed to start image provided by %s (%lu)\n",
		    mod->name, EFI_ERROR_CODE(status));
		return;
	}
}

EFI_STATUS
efi_main(EFI_HANDLE Ximage, EFI_SYSTEM_TABLE *Xsystab)
{
	EFI_HANDLE *handles;
	EFI_STATUS status;
	EFI_CONSOLE_CONTROL_PROTOCOL *ConsoleControl = NULL;
	SIMPLE_TEXT_OUTPUT_INTERFACE *conout = NULL;
	UINTN i, max_dim, best_mode, cols, rows, hsize, nhandles;

	/* Basic initialization*/
	systab = Xsystab;
	image = Ximage;
	bs = Xsystab->BootServices;

	/* Set up the console, so printf works. */
	status = bs->LocateProtocol(&ConsoleControlGUID, NULL,
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

	printf("\n>> FreeBSD EFI boot block\n");
	printf("   Loader path: %s\n\n", _PATH_LOADER);
	printf("   Initializing modules:");
	for (i = 0; i < NUM_BOOT_MODULES; i++) {
		if (boot_modules[i] == NULL)
			continue;

		printf(" %s", boot_modules[i]->name);
		if (boot_modules[i]->init != NULL)
			boot_modules[i]->init();
	}
	putchar('\n');

	/* Get all the device handles */
	hsize = (UINTN)NUM_HANDLES_INIT * sizeof(EFI_HANDLE);
	if ((status = bs->AllocatePool(EfiLoaderData, hsize, (void **)&handles))
	    != EFI_SUCCESS)
		panic("Failed to allocate %d handles (%lu)", NUM_HANDLES_INIT,
		    EFI_ERROR_CODE(status));

	status = bs->LocateHandle(ByProtocol, &BlockIoProtocolGUID, NULL,
	    &hsize, handles);
	switch (status) {
	case EFI_SUCCESS:
		break;
	case EFI_BUFFER_TOO_SMALL:
		(void)bs->FreePool(handles);
		if ((status = bs->AllocatePool(EfiLoaderData, hsize,
		    (void **)&handles) != EFI_SUCCESS)) {
			panic("Failed to allocate %zu handles (%lu)", hsize /
			    sizeof(*handles), EFI_ERROR_CODE(status));
		}
		status = bs->LocateHandle(ByProtocol, &BlockIoProtocolGUID,
		    NULL, &hsize, handles);
		if (status != EFI_SUCCESS)
			panic("Failed to get device handles (%lu)\n",
			    EFI_ERROR_CODE(status));
		break;
	default:
		panic("Failed to get device handles (%lu)",
		    EFI_ERROR_CODE(status));
	}

	/* Scan all partitions, probing with all modules. */
	nhandles = hsize / sizeof(*handles);
	printf("   Probing %zu block devices...", nhandles);
	for (i = 0; i < nhandles; i++) {
		status = probe_handle(handles[i]);
		switch (status) {
		case EFI_UNSUPPORTED:
			printf(".");
			break;
		case EFI_SUCCESS:
			printf("+");
			break;
		default:
			printf("x");
			break;
		}
	}
	printf(" done\n");

	/* Status summary. */
	for (i = 0; i < NUM_BOOT_MODULES; i++) {
		if (boot_modules[i] != NULL) {
			printf("    ");
			boot_modules[i]->status();
		}
	}

	/* Select a partition to boot by trying each module in order. */
	for (i = 0; i < NUM_BOOT_MODULES; i++)
		if (boot_modules[i] != NULL)
			try_load(boot_modules[i]);

	/* If we get here, we're out of luck... */
	panic("No bootable partitions found!");
}

static EFI_STATUS
probe_handle(EFI_HANDLE h)
{
	dev_info_t *devinfo;
	EFI_BLOCK_IO *blkio;
	EFI_DEVICE_PATH *devpath;
	EFI_STATUS status;
	UINTN i;

	/* Figure out if we're dealing with an actual partition. */
	status = bs->HandleProtocol(h, &DevicePathGUID, (void **)&devpath);
	if (status == EFI_UNSUPPORTED)
		return (status);

	if (status != EFI_SUCCESS) {
		DPRINTF("\nFailed to query DevicePath (%lu)\n",
		    EFI_ERROR_CODE(status));
		return (status);
	}

	while (!IsDevicePathEnd(NextDevicePathNode(devpath)))
		devpath = NextDevicePathNode(devpath);

	status = bs->HandleProtocol(h, &BlockIoProtocolGUID, (void **)&blkio);
	if (status == EFI_UNSUPPORTED)
		return (status);

	if (status != EFI_SUCCESS) {
		DPRINTF("\nFailed to query BlockIoProtocol (%lu)\n",
		    EFI_ERROR_CODE(status));
		return (status);
	}

	if (!blkio->Media->LogicalPartition)
		return (EFI_UNSUPPORTED);

	/* Run through each module, see if it can load this partition */
	for (i = 0; i < NUM_BOOT_MODULES; i++) {
		if (boot_modules[i] == NULL)
			continue;

		if ((status = bs->AllocatePool(EfiLoaderData,
		    sizeof(*devinfo), (void **)&devinfo)) !=
		    EFI_SUCCESS) {
			DPRINTF("\nFailed to allocate devinfo (%lu)\n",
			    EFI_ERROR_CODE(status));
			continue;
		}
		devinfo->dev = blkio;
		devinfo->devpath = devpath;
		devinfo->devhandle = h;
		devinfo->devdata = NULL;
		devinfo->next = NULL;

		status = boot_modules[i]->probe(devinfo);
		if (status == EFI_SUCCESS)
			return (EFI_SUCCESS);
		(void)bs->FreePool(devinfo);
	}

	return (EFI_UNSUPPORTED);
}

void
add_device(dev_info_t **devinfop, dev_info_t *devinfo)
{
	dev_info_t *dev;

	if (*devinfop == NULL) {
		*devinfop = devinfo;
		return;
	}

	for (dev = *devinfop; dev->next != NULL; dev = dev->next)
		;

	dev->next = devinfo;
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
