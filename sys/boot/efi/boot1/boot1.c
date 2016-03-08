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
#include "paths.h"

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
 * nodes_match returns TRUE if the imgpath isn't NULL and the nodes match,
 * FALSE otherwise.
 */
static BOOLEAN
nodes_match(EFI_DEVICE_PATH *imgpath, EFI_DEVICE_PATH *devpath)
{
	int len;

	if (imgpath == NULL || imgpath->Type != devpath->Type ||
	    imgpath->SubType != devpath->SubType)
		return (FALSE);

	len = DevicePathNodeLength(imgpath);
	if (len != DevicePathNodeLength(devpath))
		return (FALSE);

	return (memcmp(imgpath, devpath, (size_t)len) == 0);
}

/*
 * device_paths_match returns TRUE if the imgpath isn't NULL and all nodes
 * in imgpath and devpath match up to their respect occurances of a media
 * node, FALSE otherwise.
 */
static BOOLEAN
device_paths_match(EFI_DEVICE_PATH *imgpath, EFI_DEVICE_PATH *devpath)
{

	if (imgpath == NULL)
		return (FALSE);

	while (!IsDevicePathEnd(imgpath) && !IsDevicePathEnd(devpath)) {
		if (IsDevicePathType(imgpath, MEDIA_DEVICE_PATH) &&
		    IsDevicePathType(devpath, MEDIA_DEVICE_PATH))
			return (TRUE);

		if (!nodes_match(imgpath, devpath))
			return (FALSE);

		imgpath = NextDevicePathNode(imgpath);
		devpath = NextDevicePathNode(devpath);
	}

	return (FALSE);
}

/*
 * devpath_last returns the last non-path end node in devpath.
 */
static EFI_DEVICE_PATH *
devpath_last(EFI_DEVICE_PATH *devpath)
{

	while (!IsDevicePathEnd(NextDevicePathNode(devpath)))
		devpath = NextDevicePathNode(devpath);

	return (devpath);
}

/*
 * devpath_node_str is a basic output method for a devpath node which
 * only understands a subset of the available sub types.
 *
 * If we switch to UEFI 2.x then we should update it to use:
 * EFI_DEVICE_PATH_TO_TEXT_PROTOCOL.
 */
static int
devpath_node_str(char *buf, size_t size, EFI_DEVICE_PATH *devpath)
{

	switch (devpath->Type) {
	case MESSAGING_DEVICE_PATH:
		switch (devpath->SubType) {
		case MSG_ATAPI_DP: {
			ATAPI_DEVICE_PATH *atapi;

			atapi = (ATAPI_DEVICE_PATH *)(void *)devpath;
			return snprintf(buf, size, "ata(%s,%s,0x%x)",
			    (atapi->PrimarySecondary == 1) ?  "Sec" : "Pri",
			    (atapi->SlaveMaster == 1) ?  "Slave" : "Master",
			    atapi->Lun);
		}
		case MSG_USB_DP: {
			USB_DEVICE_PATH *usb;

			usb = (USB_DEVICE_PATH *)devpath;
			return snprintf(buf, size, "usb(0x%02x,0x%02x)",
			    usb->ParentPortNumber, usb->InterfaceNumber);
		}
		case MSG_SCSI_DP: {
			SCSI_DEVICE_PATH *scsi;

			scsi = (SCSI_DEVICE_PATH *)(void *)devpath;
			return snprintf(buf, size, "scsi(0x%02x,0x%02x)",
			    scsi->Pun, scsi->Lun);
		}
		case MSG_SATA_DP: {
			SATA_DEVICE_PATH *sata;

			sata = (SATA_DEVICE_PATH *)(void *)devpath;
			return snprintf(buf, size, "sata(0x%x,0x%x,0x%x)",
			    sata->HBAPortNumber, sata->PortMultiplierPortNumber,
			    sata->Lun);
		}
		default:
			return snprintf(buf, size, "msg(0x%02x)",
			    devpath->SubType);
		}
		break;
	case HARDWARE_DEVICE_PATH:
		switch (devpath->SubType) {
		case HW_PCI_DP: {
			PCI_DEVICE_PATH *pci;

			pci = (PCI_DEVICE_PATH *)devpath;
			return snprintf(buf, size, "pci(0x%02x,0x%02x)",
			    pci->Device, pci->Function);
		}
		default:
			return snprintf(buf, size, "hw(0x%02x)",
			    devpath->SubType);
		}
		break;
	case ACPI_DEVICE_PATH: {
		ACPI_HID_DEVICE_PATH *acpi;

		acpi = (ACPI_HID_DEVICE_PATH *)(void *)devpath;
		if ((acpi->HID & PNP_EISA_ID_MASK) == PNP_EISA_ID_CONST) {
			switch (EISA_ID_TO_NUM(acpi->HID)) {
			case 0x0a03:
				return snprintf(buf, size, "pciroot(0x%x)",
				    acpi->UID);
			case 0x0a08:
				return snprintf(buf, size, "pcieroot(0x%x)",
				    acpi->UID);
			case 0x0604:
				return snprintf(buf, size, "floppy(0x%x)",
				    acpi->UID);
			case 0x0301:
				return snprintf(buf, size, "keyboard(0x%x)",
				    acpi->UID);
			case 0x0501:
				return snprintf(buf, size, "serial(0x%x)",
				    acpi->UID);
			case 0x0401:
				return snprintf(buf, size, "parallelport(0x%x)",
				    acpi->UID);
			default:
				return snprintf(buf, size, "acpi(pnp%04x,0x%x)",
				    EISA_ID_TO_NUM(acpi->HID), acpi->UID);
			}
		}

		return snprintf(buf, size, "acpi(0x%08x,0x%x)", acpi->HID,
		    acpi->UID);
	}
	case MEDIA_DEVICE_PATH:
		switch (devpath->SubType) {
		case MEDIA_CDROM_DP: {
			CDROM_DEVICE_PATH *cdrom;

			cdrom = (CDROM_DEVICE_PATH *)(void *)devpath;
			return snprintf(buf, size, "cdrom(%x)",
			    cdrom->BootEntry);
		}
		case MEDIA_HARDDRIVE_DP: {
			HARDDRIVE_DEVICE_PATH *hd;

			hd = (HARDDRIVE_DEVICE_PATH *)(void *)devpath;
			return snprintf(buf, size, "hd(%x)",
			    hd->PartitionNumber);
		}
		default:
			return snprintf(buf, size, "media(0x%02x)",
			    devpath->SubType);
		}
	case BBS_DEVICE_PATH:
		return snprintf(buf, size, "bbs(0x%02x)", devpath->SubType);
	case END_DEVICE_PATH_TYPE:
		return (0);
	}

	return snprintf(buf, size, "type(0x%02x, 0x%02x)", devpath->Type,
	    devpath->SubType);
}

/*
 * devpath_strlcat appends a text description of devpath to buf but not more
 * than size - 1 characters followed by NUL-terminator.
 */
int
devpath_strlcat(char *buf, size_t size, EFI_DEVICE_PATH *devpath)
{
	size_t len, used;
	const char *sep;

	sep = "";
	used = 0;
	while (!IsDevicePathEnd(devpath)) {
		len = snprintf(buf, size - used, "%s", sep);
		used += len;
		if (used > size)
			return (used);
		buf += len;

		len = devpath_node_str(buf, size - used, devpath);
		used += len;
		if (used > size)
			return (used);
		buf += len;
		devpath = NextDevicePathNode(devpath);
		sep = ":";
	}

	return (used);
}

/*
 * devpath_str is convenience method which returns the text description of
 * devpath using a static buffer, so it isn't thread safe!
 */
char *
devpath_str(EFI_DEVICE_PATH *devpath)
{
	static char buf[256];

	devpath_strlcat(buf, sizeof(buf), devpath);

	return buf;
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
    size_t *bufsize, BOOLEAN preferred)
{
	UINTN i;
	dev_info_t *dev;
	const boot_module_t *mod;

	for (i = 0; i < NUM_BOOT_MODULES; i++) {
		if (boot_modules[i] == NULL)
			continue;
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

/*
 * try_boot only returns if it fails to load the loader. If it succeeds
 * it simply boots, otherwise it returns the status of last EFI call.
 */
static EFI_STATUS
try_boot()
{
	size_t bufsize, loadersize, cmdsize;
	void *buf, *loaderbuf;
	char *cmd;
	dev_info_t *dev;
	const boot_module_t *mod;
	EFI_HANDLE loaderhandle;
	EFI_LOADED_IMAGE *loaded_image;
	EFI_STATUS status;

	status = load_loader(&mod, &dev, &loaderbuf, &loadersize, TRUE);
	if (status != EFI_SUCCESS) {
		status = load_loader(&mod, &dev, &loaderbuf, &loadersize,
		    FALSE);
		if (status != EFI_SUCCESS) {
			printf("Failed to load '%s'\n", PATH_LOADER_EFI);
			return (status);
		}
	}

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

	if ((status = bs->LoadImage(TRUE, image, devpath_last(dev->devpath),
	    loaderbuf, loadersize, &loaderhandle)) != EFI_SUCCESS) {
		printf("Failed to load image provided by %s, size: %zu, (%lu)\n",
		     mod->name, bufsize, EFI_ERROR_CODE(status));
		goto errout;
	}

	if ((status = bs->HandleProtocol(loaderhandle, &LoadedImageGUID,
	    (VOID**)&loaded_image)) != EFI_SUCCESS) {
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

	if ((status = bs->StartImage(loaderhandle, NULL, NULL)) !=
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

/*
 * probe_handle determines if the passed handle represents a logical partition
 * if it does it uses each module in order to probe it and if successful it
 * returns EFI_SUCCESS.
 */
static EFI_STATUS
probe_handle(EFI_HANDLE h, EFI_DEVICE_PATH *imgpath, BOOLEAN *preferred)
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

	DPRINTF("probing: %s\n", devpath_str(devpath));

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

	*preferred = device_paths_match(imgpath, devpath);

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
		devinfo->preferred = *preferred;
		devinfo->next = NULL;

		status = boot_modules[i]->probe(devinfo);
		if (status == EFI_SUCCESS)
			return (EFI_SUCCESS);
		(void)bs->FreePool(devinfo);
	}

	return (EFI_UNSUPPORTED);
}

/*
 * probe_handle_status calls probe_handle and outputs the returned status
 * of the call.
 */
static void
probe_handle_status(EFI_HANDLE h, EFI_DEVICE_PATH *imgpath)
{
	EFI_STATUS status;
	BOOLEAN preferred;

	status = probe_handle(h, imgpath, &preferred);
	
	DPRINTF("probe: ");
	switch (status) {
	case EFI_UNSUPPORTED:
		printf(".");
		DPRINTF(" not supported\n");
		break;
	case EFI_SUCCESS:
		if (preferred) {
			printf("%c", '*');
			DPRINTF(" supported (preferred)\n");
		} else {
			printf("%c", '+');
			DPRINTF(" supported\n");
		}
		break;
	default:
		printf("x");
		DPRINTF(" error (%lu)\n", EFI_ERROR_CODE(status));
		break;
	}
	DSTALL(500000);
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
	printf("   Loader path: %s\n\n", PATH_LOADER_EFI);
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
	DPRINTF("\n");

	/* Determine the devpath of our image so we can prefer it. */
	status = bs->HandleProtocol(image, &LoadedImageGUID, (VOID**)&img);
	imgpath = NULL;
	if (status == EFI_SUCCESS) {
		status = bs->HandleProtocol(img->DeviceHandle, &DevicePathGUID,
		    (void **)&imgpath);
		if (status != EFI_SUCCESS)
			DPRINTF("Failed to get image DevicePath (%lu)\n",
			    EFI_ERROR_CODE(status));
		DPRINTF("boot1 imagepath: %s\n", devpath_str(imgpath));
	}

	for (i = 0; i < nhandles; i++)
		probe_handle_status(handles[i], imgpath);
	printf(" done\n");

	/* Status summary. */
	for (i = 0; i < NUM_BOOT_MODULES; i++) {
		if (boot_modules[i] != NULL) {
			printf("    ");
			boot_modules[i]->status();
		}
	}

	try_boot();

	/* If we get here, we're out of luck... */
	panic("No bootable partitions found!");
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
