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

#include <sys/param.h>
#include <sys/stdarg.h>

#include <machine/elf.h>

#include <stand.h>

#include <efi.h>
#include <eficonsctl.h>
#include <efichar.h>

#include "boot_module.h"
#include "paths.h"
#include "proto.h"

static EFI_GUID BlockIoProtocolGUID = BLOCK_IO_PROTOCOL;
static EFI_GUID DevicePathGUID = DEVICE_PATH_PROTOCOL;

#ifndef EFI_DEBUG
static const char *prio_str[] = {
	"error",
	"not supported",
	"good",
	"better"
};
#endif

/*
 * probe_handle determines if the passed handle represents a logical partition
 * if it does it uses each module in order to probe it and if successful it
 * returns EFI_SUCCESS.
 */
static int
probe_handle(EFI_HANDLE h, EFI_DEVICE_PATH *imgpath)
{
	dev_info_t *devinfo;
	EFI_BLOCK_IO *blkio;
	EFI_DEVICE_PATH *devpath;
	EFI_STATUS status;
	UINTN i;
	int preferred;

	/* Figure out if we're dealing with an actual partition. */
	status = OpenProtocolByHandle(h, &DevicePathGUID, (void **)&devpath);
	if (status == EFI_UNSUPPORTED)
		return (0);

	if (status != EFI_SUCCESS) {
		DPRINTF("\nFailed to query DevicePath (%lu)\n",
		    EFI_ERROR_CODE(status));
		return (-1);
	}
#ifdef EFI_DEBUG
	{
		CHAR16 *text = efi_devpath_name(devpath);
		DPRINTF("probing: %S ", text);
		efi_free_devpath_name(text);
	}
#endif
	status = OpenProtocolByHandle(h, &BlockIoProtocolGUID, (void **)&blkio);
	if (status == EFI_UNSUPPORTED)
		return (0);

	if (status != EFI_SUCCESS) {
		DPRINTF("\nFailed to query BlockIoProtocol (%lu)\n",
		    EFI_ERROR_CODE(status));
		return (-1);
	}

	if (!blkio->Media->LogicalPartition)
		return (0);

	preferred = efi_devpath_same_disk(imgpath, devpath);

	/* Run through each module, see if it can load this partition */
	devinfo = malloc(sizeof(*devinfo));
	if (devinfo == NULL) {
		DPRINTF("\nFailed to allocate devinfo\n");
		return (-1);
	}
	devinfo->dev = blkio;
	devinfo->devpath = devpath;
	devinfo->devhandle = h;
	devinfo->preferred = preferred;
	devinfo->next = NULL;

	for (i = 0; i < num_boot_modules; i++) {
		devinfo->devdata = NULL;

		status = boot_modules[i]->probe(devinfo);
		if (status == EFI_SUCCESS)
			return (preferred + 1);
	}
	free(devinfo);

	return (0);
}

/*
 * load_loader attempts to load the loader image data.
 *
 * It tries each module and its respective devices, identified by mod->probe,
 * in order until a successful load occurs at which point it returns EFI_SUCCESS
 * and EFI_NOT_FOUND otherwise.
 *
 * Only devices which have preferred matching the preferred parameter are tried.
 */
static EFI_STATUS
load_loader(const boot_module_t **modp, dev_info_t **devinfop, void **bufp,
    size_t *bufsize, int preferred)
{
	UINTN i;
	dev_info_t *dev;
	const boot_module_t *mod;

	for (i = 0; i < num_boot_modules; i++) {
		mod = boot_modules[i];
		for (dev = mod->devices(); dev != NULL; dev = dev->next) {
			if (dev->preferred != preferred)
				continue;

			if (mod->load(PATH_LOADER_EFI, dev, bufp, bufsize) ==
			    EFI_SUCCESS) {
				*devinfop = dev;
				*modp = mod;
				return (EFI_SUCCESS);
			}
		}
	}

	return (EFI_NOT_FOUND);
}

void
choice_protocol(EFI_HANDLE *handles, UINTN nhandles, EFI_DEVICE_PATH *imgpath)
{
	UINT16 boot_current;
	size_t sz;
	UINT16 boot_order[100];
	unsigned i;
	int rv;
	EFI_STATUS status;
	const boot_module_t *mod;
	dev_info_t *dev;
	void *loaderbuf;
	size_t loadersize;

	/* Report UEFI Boot Manager Protocol details */
	boot_current = 0;
	sz = sizeof(boot_current);
	if (efi_global_getenv("BootCurrent", &boot_current, &sz) == EFI_SUCCESS) {
		printf("   BootCurrent: %04x\n", boot_current);

		sz = sizeof(boot_order);
		if (efi_global_getenv("BootOrder", &boot_order, &sz) == EFI_SUCCESS) {
			printf("   BootOrder:");
			for (i = 0; i < sz / sizeof(boot_order[0]); i++)
				printf(" %04x%s", boot_order[i],
				    boot_order[i] == boot_current ? "[*]" : "");
			printf("\n");
		}
	}

#ifdef TEST_FAILURE
	/*
	 * For testing failover scenarios, it's nice to be able to fail fast.
	 * Define TEST_FAILURE to create a boot1.efi that always fails after
	 * reporting the boot manager protocol details.
	 */
	BS->Exit(IH, EFI_OUT_OF_RESOURCES, 0, NULL);
#endif

	/* Scan all partitions, probing with all modules. */
	printf("   Probing %zu block devices...", nhandles);
	DPRINTF("\n");
	for (i = 0; i < nhandles; i++) {
		rv = probe_handle(handles[i], imgpath);
#ifdef EFI_DEBUG
		printf("%c", "x.+*"[rv + 1]);
#else
		printf("%s\n", prio_str[rv + 1]);
#endif
	}
	printf(" done\n");


	/* Status summary. */
	for (i = 0; i < num_boot_modules; i++) {
		printf("    ");
		boot_modules[i]->status();
	}

	status = load_loader(&mod, &dev, &loaderbuf, &loadersize, 1);
	if (status != EFI_SUCCESS) {
		status = load_loader(&mod, &dev, &loaderbuf, &loadersize, 0);
		if (status != EFI_SUCCESS) {
			printf("Failed to load '%s'\n", PATH_LOADER_EFI);
			return;
		}
	}

	try_boot(mod, dev, loaderbuf, loadersize);
}
