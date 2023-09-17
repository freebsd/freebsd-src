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

static void efi_panic(EFI_STATUS s, const char *fmt, ...) __dead2 __printflike(2, 3);

const boot_module_t *boot_modules[] =
{
#ifdef EFI_ZFS_BOOT
	&zfs_module,
#endif
#ifdef EFI_UFS_BOOT
	&ufs_module
#endif
};
const UINTN num_boot_modules = nitems(boot_modules);

static EFI_GUID BlockIoProtocolGUID = BLOCK_IO_PROTOCOL;
static EFI_GUID DevicePathGUID = DEVICE_PATH_PROTOCOL;
static EFI_GUID LoadedImageGUID = LOADED_IMAGE_PROTOCOL;
static EFI_GUID ConsoleControlGUID = EFI_CONSOLE_CONTROL_PROTOCOL_GUID;

static EFI_PHYSICAL_ADDRESS heap;
static UINTN heapsize;

/*
 * try_boot only returns if it fails to load the loader. If it succeeds
 * it simply boots, otherwise it returns the status of last EFI call.
 */
EFI_STATUS
try_boot(const boot_module_t *mod, dev_info_t *dev, void *loaderbuf, size_t loadersize)
{
	size_t bufsize, cmdsize;
	void *buf;
	char *cmd;
	EFI_HANDLE loaderhandle;
	EFI_LOADED_IMAGE *loaded_image;
	EFI_STATUS status;

	/*
	 * Read in and parse the command line from /boot.config or /boot/config,
	 * if present. We'll pass it the next stage via a simple ASCII
	 * string. loader.efi has a hack for ASCII strings, so we'll use that to
	 * keep the size down here. We only try to read the alternate file if
	 * we get EFI_NOT_FOUND because all other errors mean that the boot_module
	 * had troubles with the filesystem. We could return early, but we'll let
	 * loading the actual kernel sort all that out. Since these files are
	 * optional, we don't report errors in trying to read them.
	 */
	cmd = NULL;
	cmdsize = 0;
	status = mod->load(PATH_DOTCONFIG, dev, &buf, &bufsize);
	if (status == EFI_NOT_FOUND)
		status = mod->load(PATH_CONFIG, dev, &buf, &bufsize);
	if (status == EFI_SUCCESS) {
		cmdsize = bufsize + 1;
		cmd = malloc(cmdsize);
		if (cmd == NULL)
			goto errout;
		memcpy(cmd, buf, bufsize);
		cmd[bufsize] = '\0';
		free(buf);
		buf = NULL;
	}

	/*
	 * See if there's any env variables the module wants to set. If so,
	 * append it to any config present.
	 */
	if (mod->extra_env != NULL) {
		const char *env = mod->extra_env();
		if (env != NULL) {
			size_t newlen = cmdsize + strlen(env) + 1;

			cmd = realloc(cmd, newlen);
			if (cmd == NULL)
				goto errout;
			if (cmdsize > 0)
				strlcat(cmd, " ", newlen);
			strlcat(cmd, env, newlen);
			cmdsize = strlen(cmd);
			free(__DECONST(char *, env));
		}
	}

	if ((status = BS->LoadImage(TRUE, IH, efi_devpath_last_node(dev->devpath),
	    loaderbuf, loadersize, &loaderhandle)) != EFI_SUCCESS) {
		printf("Failed to load image provided by %s, size: %zu, (%lu)\n",
		     mod->name, loadersize, EFI_ERROR_CODE(status));
		goto errout;
	}

	status = OpenProtocolByHandle(loaderhandle, &LoadedImageGUID,
	    (void **)&loaded_image);
	if (status != EFI_SUCCESS) {
		printf("Failed to query LoadedImage provided by %s (%lu)\n",
		    mod->name, EFI_ERROR_CODE(status));
		goto errout;
	}

	if (cmd != NULL)
		printf("    command args: %s\n", cmd);

	loaded_image->DeviceHandle = dev->devhandle;
	loaded_image->LoadOptionsSize = cmdsize;
	loaded_image->LoadOptions = cmd;

	DPRINTF("Starting '%s' in 5 seconds...", PATH_LOADER_EFI);
	DSTALL(1000000);
	DPRINTF(".");
	DSTALL(1000000);
	DPRINTF(".");
	DSTALL(1000000);
	DPRINTF(".");
	DSTALL(1000000);
	DPRINTF(".");
	DSTALL(1000000);
	DPRINTF(".\n");

	if ((status = BS->StartImage(loaderhandle, NULL, NULL)) !=
	    EFI_SUCCESS) {
		printf("Failed to start image provided by %s (%lu)\n",
		    mod->name, EFI_ERROR_CODE(status));
		loaded_image->LoadOptionsSize = 0;
		loaded_image->LoadOptions = NULL;
	}

errout:
	if (cmd != NULL)
		free(cmd);
	if (buf != NULL)
		free(buf);
	if (loaderbuf != NULL)
		free(loaderbuf);

	return (status);
}

EFI_STATUS
efi_main(EFI_HANDLE Ximage, EFI_SYSTEM_TABLE *Xsystab)
{
	EFI_HANDLE *handles;
	EFI_LOADED_IMAGE *img;
	EFI_DEVICE_PATH *imgpath;
	EFI_STATUS status;
	EFI_CONSOLE_CONTROL_PROTOCOL *ConsoleControl = NULL;
	SIMPLE_TEXT_OUTPUT_INTERFACE *conout = NULL;
	UINTN i, hsize, nhandles;
	CHAR16 *text;

	/* Basic initialization*/
	ST = Xsystab;
	IH = Ximage;
	BS = ST->BootServices;
	RS = ST->RuntimeServices;

	heapsize = 64 * 1024 * 1024;
	status = BS->AllocatePages(AllocateAnyPages, EfiLoaderData,
	    EFI_SIZE_TO_PAGES(heapsize), &heap);
	if (status != EFI_SUCCESS) {
		ST->ConOut->OutputString(ST->ConOut,
		    __DECONST(CHAR16 *,
		    L"Failed to allocate memory for heap.\r\n"));
		BS->Exit(IH, status, 0, NULL);
	}

	setheap((void *)(uintptr_t)heap, (void *)(uintptr_t)(heap + heapsize));

	/* Set up the console, so printf works. */
	status = BS->LocateProtocol(&ConsoleControlGUID, NULL,
	    (VOID **)&ConsoleControl);
	if (status == EFI_SUCCESS)
		(void)ConsoleControl->SetMode(ConsoleControl,
		    EfiConsoleControlScreenText);
	/*
	 * Reset the console enable the cursor. Later we'll choose a better
	 * console size through GOP/UGA.
	 */
	conout = ST->ConOut;
	conout->Reset(conout, TRUE);
	/* Explicitly set conout to mode 0, 80x25 */
	conout->SetMode(conout, 0);
	conout->EnableCursor(conout, TRUE);
	conout->ClearScreen(conout);

	printf("\n>> FreeBSD EFI boot block\n");
	printf("   Loader path: %s\n\n", PATH_LOADER_EFI);
	printf("   Initializing modules:");
	for (i = 0; i < num_boot_modules; i++) {
		printf(" %s", boot_modules[i]->name);
		if (boot_modules[i]->init != NULL)
			boot_modules[i]->init();
	}
	putchar('\n');

	/* Fetch all the block I/O handles, we have to search through them later */
	hsize = 0;
	BS->LocateHandle(ByProtocol, &BlockIoProtocolGUID, NULL,
	    &hsize, NULL);
	handles = malloc(hsize);
	if (handles == NULL)
		efi_panic(EFI_OUT_OF_RESOURCES, "Failed to allocate %d handles\n",
		    hsize);
	status = BS->LocateHandle(ByProtocol, &BlockIoProtocolGUID,
	    NULL, &hsize, handles);
	if (status != EFI_SUCCESS)
		efi_panic(status, "Failed to get device handles\n");
	nhandles = hsize / sizeof(*handles);

	/* Determine the devpath of our image so we can prefer it. */
	status = OpenProtocolByHandle(IH, &LoadedImageGUID, (void **)&img);
	imgpath = NULL;
	if (status == EFI_SUCCESS) {
		text = efi_devpath_name(img->FilePath);
		if (text != NULL) {
			printf("   Load Path: %S\n", text);
			efi_setenv_freebsd_wcs("Boot1Path", text);
			efi_free_devpath_name(text);
		}

		status = OpenProtocolByHandle(img->DeviceHandle,
		    &DevicePathGUID, (void **)&imgpath);
		if (status != EFI_SUCCESS) {
			DPRINTF("Failed to get image DevicePath (%lu)\n",
			    EFI_ERROR_CODE(status));
		} else {
			text = efi_devpath_name(imgpath);
			if (text != NULL) {
				printf("   Load Device: %S\n", text);
				efi_setenv_freebsd_wcs("Boot1Dev", text);
				efi_free_devpath_name(text);
			}
		}
	}

	choice_protocol(handles, nhandles, imgpath);

	/* If we get here, we're out of luck... */
	efi_panic(EFI_LOAD_ERROR, "No bootable partitions found!");
}

/*
 * add_device adds a device to the passed devinfo list.
 */
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
efi_exit(EFI_STATUS s)
{

	BS->FreePages(heap, EFI_SIZE_TO_PAGES(heapsize));
	BS->Exit(IH, s, 0, NULL);
}

void
exit(int error __unused)
{
	efi_exit(EFI_LOAD_ERROR);
}

/*
 * OK. We totally give up. Exit back to EFI with a sensible status so
 * it can try the next option on the list.
 */
static void
efi_panic(EFI_STATUS s, const char *fmt, ...)
{
	va_list ap;

	printf("panic: ");
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	printf("\n");

	efi_exit(s);
}

int getchar(void)
{
	return (-1);
}

void
putchar(int c)
{
	CHAR16 buf[2];

	if (c == '\n') {
		buf[0] = '\r';
		buf[1] = 0;
		ST->ConOut->OutputString(ST->ConOut, buf);
	}
	buf[0] = c;
	buf[1] = 0;
	ST->ConOut->OutputString(ST->ConOut, buf);
}
