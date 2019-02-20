/*-
 * Copyright (c) 2008-2010 Rui Paulo
 * Copyright (c) 2006 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/disk.h>
#include <sys/param.h>
#include <sys/reboot.h>
#include <stdint.h>
#include <stand.h>
#include <string.h>
#include <setjmp.h>
#include <disk.h>

#include <efi.h>
#include <efilib.h>

#include <uuid.h>

#include <bootstrap.h>
#include <smbios.h>

#ifdef EFI_ZFS_BOOT
#include <libzfs.h>
#include "efizfs.h"
#endif

#include "loader_efi.h"

struct arch_switch archsw;	/* MI/MD interface boundary */

EFI_GUID acpi = ACPI_TABLE_GUID;
EFI_GUID acpi20 = ACPI_20_TABLE_GUID;
EFI_GUID devid = DEVICE_PATH_PROTOCOL;
EFI_GUID imgid = LOADED_IMAGE_PROTOCOL;
EFI_GUID mps = MPS_TABLE_GUID;
EFI_GUID netid = EFI_SIMPLE_NETWORK_PROTOCOL;
EFI_GUID smbios = SMBIOS_TABLE_GUID;
EFI_GUID smbios3 = SMBIOS3_TABLE_GUID;
EFI_GUID dxe = DXE_SERVICES_TABLE_GUID;
EFI_GUID hoblist = HOB_LIST_TABLE_GUID;
EFI_GUID lzmadecomp = LZMA_DECOMPRESSION_GUID;
EFI_GUID mpcore = ARM_MP_CORE_INFO_TABLE_GUID;
EFI_GUID esrt = ESRT_TABLE_GUID;
EFI_GUID memtype = MEMORY_TYPE_INFORMATION_TABLE_GUID;
EFI_GUID debugimg = DEBUG_IMAGE_INFO_TABLE_GUID;
EFI_GUID fdtdtb = FDT_TABLE_GUID;
EFI_GUID inputid = SIMPLE_TEXT_INPUT_PROTOCOL;

/*
 * Number of seconds to wait for a keystroke before exiting with failure
 * in the event no currdev is found. -2 means always break, -1 means
 * never break, 0 means poll once and then reboot, > 0 means wait for
 * that many seconds. "fail_timeout" can be set in the environment as
 * well.
 */
static int fail_timeout = 5;

static bool
has_keyboard(void)
{
	EFI_STATUS status;
	EFI_DEVICE_PATH *path;
	EFI_HANDLE *hin, *hin_end, *walker;
	UINTN sz;
	bool retval = false;

	/*
	 * Find all the handles that support the SIMPLE_TEXT_INPUT_PROTOCOL and
	 * do the typical dance to get the right sized buffer.
	 */
	sz = 0;
	hin = NULL;
	status = BS->LocateHandle(ByProtocol, &inputid, 0, &sz, 0);
	if (status == EFI_BUFFER_TOO_SMALL) {
		hin = (EFI_HANDLE *)malloc(sz);
		status = BS->LocateHandle(ByProtocol, &inputid, 0, &sz,
		    hin);
		if (EFI_ERROR(status))
			free(hin);
	}
	if (EFI_ERROR(status))
		return retval;

	/*
	 * Look at each of the handles. If it supports the device path protocol,
	 * use it to get the device path for this handle. Then see if that
	 * device path matches either the USB device path for keyboards or the
	 * legacy device path for keyboards.
	 */
	hin_end = &hin[sz / sizeof(*hin)];
	for (walker = hin; walker < hin_end; walker++) {
		status = BS->HandleProtocol(*walker, &devid, (VOID **)&path);
		if (EFI_ERROR(status))
			continue;

		while (!IsDevicePathEnd(path)) {
			/*
			 * Check for the ACPI keyboard node. All PNP3xx nodes
			 * are keyboards of different flavors. Note: It is
			 * unclear of there's always a keyboard node when
			 * there's a keyboard controller, or if there's only one
			 * when a keyboard is detected at boot.
			 */
			if (DevicePathType(path) == ACPI_DEVICE_PATH &&
			    (DevicePathSubType(path) == ACPI_DP ||
				DevicePathSubType(path) == ACPI_EXTENDED_DP)) {
				ACPI_HID_DEVICE_PATH  *acpi;

				acpi = (ACPI_HID_DEVICE_PATH *)(void *)path;
				if ((EISA_ID_TO_NUM(acpi->HID) & 0xff00) == 0x300 &&
				    (acpi->HID & 0xffff) == PNP_EISA_ID_CONST) {
					retval = true;
					goto out;
				}
			/*
			 * Check for USB keyboard node, if present. Unlike a
			 * PS/2 keyboard, these definitely only appear when
			 * connected to the system.
			 */
			} else if (DevicePathType(path) == MESSAGING_DEVICE_PATH &&
			    DevicePathSubType(path) == MSG_USB_CLASS_DP) {
				USB_CLASS_DEVICE_PATH *usb;

				usb = (USB_CLASS_DEVICE_PATH *)(void *)path;
				if (usb->DeviceClass == 3 && /* HID */
				    usb->DeviceSubClass == 1 && /* Boot devices */
				    usb->DeviceProtocol == 1) { /* Boot keyboards */
					retval = true;
					goto out;
				}
			}
			path = NextDevicePathNode(path);
		}
	}
out:
	free(hin);
	return retval;
}

static void
set_currdev_devdesc(struct devdesc *currdev)
{
	const char *devname;

	devname = efi_fmtdev(currdev);

	printf("Setting currdev to %s\n", devname);

	env_setenv("currdev", EV_VOLATILE, devname, efi_setcurrdev, env_nounset);
	env_setenv("loaddev", EV_VOLATILE, devname, env_noset, env_nounset);
}

static void
set_currdev_devsw(struct devsw *dev, int unit)
{
	struct devdesc currdev;

	currdev.d_dev = dev;
	currdev.d_unit = unit;

	set_currdev_devdesc(&currdev);
}

static void
set_currdev_pdinfo(pdinfo_t *dp)
{

	/*
	 * Disks are special: they have partitions. if the parent
	 * pointer is non-null, we're a partition not a full disk
	 * and we need to adjust currdev appropriately.
	 */
	if (dp->pd_devsw->dv_type == DEVT_DISK) {
		struct disk_devdesc currdev;

		currdev.dd.d_dev = dp->pd_devsw;
		if (dp->pd_parent == NULL) {
			currdev.dd.d_unit = dp->pd_unit;
			currdev.d_slice = -1;
			currdev.d_partition = -1;
		} else {
			currdev.dd.d_unit = dp->pd_parent->pd_unit;
			currdev.d_slice = dp->pd_unit;
			currdev.d_partition = 255;	/* Assumes GPT */
		}
		set_currdev_devdesc((struct devdesc *)&currdev);
	} else {
		set_currdev_devsw(dp->pd_devsw, dp->pd_unit);
	}
}

static bool
sanity_check_currdev(void)
{
	struct stat st;

	return (stat("/boot/defaults/loader.conf", &st) == 0 ||
	    stat("/boot/kernel/kernel", &st) == 0);
}

#ifdef EFI_ZFS_BOOT
static bool
probe_zfs_currdev(uint64_t guid)
{
	char *devname;
	struct zfs_devdesc currdev;

	currdev.dd.d_dev = &zfs_dev;
	currdev.dd.d_unit = 0;
	currdev.pool_guid = guid;
	currdev.root_guid = 0;
	set_currdev_devdesc((struct devdesc *)&currdev);
	devname = efi_fmtdev(&currdev);
	init_zfs_bootenv(devname);

	return (sanity_check_currdev());
}
#endif

static bool
try_as_currdev(pdinfo_t *hd, pdinfo_t *pp)
{
	uint64_t guid;

#ifdef EFI_ZFS_BOOT
	/*
	 * If there's a zpool on this device, try it as a ZFS
	 * filesystem, which has somewhat different setup than all
	 * other types of fs due to imperfect loader integration.
	 * This all stems from ZFS being both a device (zpool) and
	 * a filesystem, plus the boot env feature.
	 */
	if (efizfs_get_guid_by_handle(pp->pd_handle, &guid))
		return (probe_zfs_currdev(guid));
#endif
	/*
	 * All other filesystems just need the pdinfo
	 * initialized in the standard way.
	 */
	set_currdev_pdinfo(pp);
	return (sanity_check_currdev());
}

static int
find_currdev(EFI_LOADED_IMAGE *img)
{
	pdinfo_t *dp, *pp;
	EFI_DEVICE_PATH *devpath, *copy;
	EFI_HANDLE h;
	CHAR16 *text;
	struct devsw *dev;
	int unit;
	uint64_t extra;

#ifdef EFI_ZFS_BOOT
	/*
	 * Did efi_zfs_probe() detect the boot pool? If so, use the zpool
	 * it found, if it's sane. ZFS is the only thing that looks for
	 * disks and pools to boot. This may change in the future, however,
	 * if we allow specifying which pool to boot from via UEFI variables
	 * rather than the bootenv stuff that FreeBSD uses today.
	 */
	if (pool_guid != 0) {
		printf("Trying ZFS pool\n");
		if (probe_zfs_currdev(pool_guid))
			return (0);
	}
#endif /* EFI_ZFS_BOOT */

	/*
	 * Try to find the block device by its handle based on the
	 * image we're booting. If we can't find a sane partition,
	 * search all the other partitions of the disk. We do not
	 * search other disks because it's a violation of the UEFI
	 * boot protocol to do so. We fail and let UEFI go on to
	 * the next candidate.
	 */
	dp = efiblk_get_pdinfo_by_handle(img->DeviceHandle);
	if (dp != NULL) {
		text = efi_devpath_name(dp->pd_devpath);
		if (text != NULL) {
			printf("Trying ESP: %S\n", text);
			efi_free_devpath_name(text);
		}
		set_currdev_pdinfo(dp);
		if (sanity_check_currdev())
			return (0);
		if (dp->pd_parent != NULL) {
			dp = dp->pd_parent;
			STAILQ_FOREACH(pp, &dp->pd_part, pd_link) {
				text = efi_devpath_name(pp->pd_devpath);
				if (text != NULL) {
					printf("And now the part: %S\n", text);
					efi_free_devpath_name(text);
				}
				/*
				 * Roll up the ZFS special case
				 * for those partitions that have
				 * zpools on them 
				 */
				if (try_as_currdev(dp, pp))
					return (0);
			}
		}
	} else {
		printf("Can't find device by handle\n");
	}

	/*
	 * Try the device handle from our loaded image first.  If that
	 * fails, use the device path from the loaded image and see if
	 * any of the nodes in that path match one of the enumerated
	 * handles. Currently, this handle list is only for netboot.
	 */
	if (efi_handle_lookup(img->DeviceHandle, &dev, &unit, &extra) == 0) {
		set_currdev_devsw(dev, unit);
		if (sanity_check_currdev())
			return (0);
	}

	copy = NULL;
	devpath = efi_lookup_image_devpath(IH);
	while (devpath != NULL) {
		h = efi_devpath_handle(devpath);
		if (h == NULL)
			break;

		free(copy);
		copy = NULL;

		if (efi_handle_lookup(h, &dev, &unit, &extra) == 0) {
			set_currdev_devsw(dev, unit);
			if (sanity_check_currdev())
				return (0);
		}

		devpath = efi_lookup_devpath(h);
		if (devpath != NULL) {
			copy = efi_devpath_trim(devpath);
			devpath = copy;
		}
	}
	free(copy);

	return (ENOENT);
}

static bool
interactive_interrupt(const char *msg)
{
	time_t now, then, last;

	last = 0;
	now = then = getsecs();
	printf("%s\n", msg);
	if (fail_timeout == -2)		/* Always break to OK */
		return (true);
	if (fail_timeout == -1)		/* Never break to OK */
		return (false);
	do {
		if (last != now) {
			printf("press any key to interrupt reboot in %d seconds\r",
			    fail_timeout - (int)(now - then));
			last = now;
		}

		/* XXX no pause or timeout wait for char */
		if (ischar())
			return (true);
		now = getsecs();
	} while (now - then < fail_timeout);
	return (false);
}

int
parse_args(int argc, CHAR16 *argv[], bool has_kbd)
{
	int i, j, howto;
	bool vargood;
	char var[128];

	/*
	 * Parse the args to set the console settings, etc
	 * boot1.efi passes these in, if it can read /boot.config or /boot/config
	 * or iPXE may be setup to pass these in. Or the optional argument in the
	 * boot environment was used to pass these arguments in (in which case
	 * neither /boot.config nor /boot/config are consulted).
	 *
	 * Loop through the args, and for each one that contains an '=' that is
	 * not the first character, add it to the environment.  This allows
	 * loader and kernel env vars to be passed on the command line.  Convert
	 * args from UCS-2 to ASCII (16 to 8 bit) as they are copied (though this
	 * method is flawed for non-ASCII characters).
	 */
	howto = 0;
	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			for (j = 1; argv[i][j] != 0; j++) {
				int ch;

				ch = argv[i][j];
				switch (ch) {
				case 'a':
					howto |= RB_ASKNAME;
					break;
				case 'd':
					howto |= RB_KDB;
					break;
				case 'D':
					howto |= RB_MULTIPLE;
					break;
				case 'h':
					howto |= RB_SERIAL;
					break;
				case 'm':
					howto |= RB_MUTE;
					break;
				case 'p':
					howto |= RB_PAUSE;
					break;
				case 'P':
					if (!has_kbd)
						howto |= RB_SERIAL | RB_MULTIPLE;
					break;
				case 'r':
					howto |= RB_DFLTROOT;
					break;
				case 's':
					howto |= RB_SINGLE;
					break;
				case 'S':
					if (argv[i][j + 1] == 0) {
						if (i + 1 == argc) {
							setenv("comconsole_speed", "115200", 1);
						} else {
							cpy16to8(&argv[i + 1][0], var,
							    sizeof(var));
							setenv("comconsole_speed", var, 1);
						}
						i++;
						break;
					} else {
						cpy16to8(&argv[i][j + 1], var,
						    sizeof(var));
						setenv("comconsole_speed", var, 1);
						break;
					}
				case 'v':
					howto |= RB_VERBOSE;
					break;
				}
			}
		} else {
			vargood = false;
			for (j = 0; argv[i][j] != 0; j++) {
				if (j == sizeof(var)) {
					vargood = false;
					break;
				}
				if (j > 0 && argv[i][j] == '=')
					vargood = true;
				var[j] = (char)argv[i][j];
			}
			if (vargood) {
				var[j] = 0;
				putenv(var);
			}
		}
	}
	return (howto);
}


EFI_STATUS
main(int argc, CHAR16 *argv[])
{
	EFI_GUID *guid;
	int howto, i;
	UINTN k;
	bool has_kbd;
	char *s;
	EFI_DEVICE_PATH *imgpath;
	CHAR16 *text;
	EFI_STATUS status;
	UINT16 boot_current;
	size_t sz;
	UINT16 boot_order[100];
	EFI_LOADED_IMAGE *img;

	archsw.arch_autoload = efi_autoload;
	archsw.arch_getdev = efi_getdev;
	archsw.arch_copyin = efi_copyin;
	archsw.arch_copyout = efi_copyout;
	archsw.arch_readin = efi_readin;
#ifdef EFI_ZFS_BOOT
	/* Note this needs to be set before ZFS init. */
	archsw.arch_zfs_probe = efi_zfs_probe;
#endif

        /* Get our loaded image protocol interface structure. */
	BS->HandleProtocol(IH, &imgid, (VOID**)&img);

#ifdef EFI_ZFS_BOOT
	/* Tell ZFS probe code where we booted from */
	efizfs_set_preferred(img->DeviceHandle);
#endif
	/* Init the time source */
	efi_time_init();

	has_kbd = has_keyboard();

	/*
	 * XXX Chicken-and-egg problem; we want to have console output
	 * early, but some console attributes may depend on reading from
	 * eg. the boot device, which we can't do yet.  We can use
	 * printf() etc. once this is done.
	 */
	cons_probe();

	/*
	 * Initialise the block cache. Set the upper limit.
	 */
	bcache_init(32768, 512);

	howto = parse_args(argc, argv, has_kbd);

	bootenv_set(howto);

	/*
	 * XXX we need fallback to this stuff after looking at the ConIn, ConOut and ConErr variables
	 */
	if (howto & RB_MULTIPLE) {
		if (howto & RB_SERIAL)
			setenv("console", "comconsole efi" , 1);
		else
			setenv("console", "efi comconsole" , 1);
	} else if (howto & RB_SERIAL) {
		setenv("console", "comconsole" , 1);
	} else
		setenv("console", "efi", 1);

	if (efi_copy_init()) {
		printf("failed to allocate staging area\n");
		return (EFI_BUFFER_TOO_SMALL);
	}

	if ((s = getenv("fail_timeout")) != NULL)
		fail_timeout = strtol(s, NULL, 10);

	/*
	 * Scan the BLOCK IO MEDIA handles then
	 * march through the device switch probing for things.
	 */
	if ((i = efipart_inithandles()) == 0) {
		for (i = 0; devsw[i] != NULL; i++)
			if (devsw[i]->dv_init != NULL)
				(devsw[i]->dv_init)();
	} else
		printf("efipart_inithandles failed %d, expect failures", i);

	printf("Command line arguments:");
	for (i = 0; i < argc; i++)
		printf(" %S", argv[i]);
	printf("\n");

	printf("Image base: 0x%lx\n", (u_long)img->ImageBase);
	printf("EFI version: %d.%02d\n", ST->Hdr.Revision >> 16,
	    ST->Hdr.Revision & 0xffff);
	printf("EFI Firmware: %S (rev %d.%02d)\n", ST->FirmwareVendor,
	    ST->FirmwareRevision >> 16, ST->FirmwareRevision & 0xffff);

	printf("\n%s", bootprog_info);

	/* Determine the devpath of our image so we can prefer it. */
	text = efi_devpath_name(img->FilePath);
	if (text != NULL) {
		printf("   Load Path: %S\n", text);
		efi_setenv_freebsd_wcs("LoaderPath", text);
		efi_free_devpath_name(text);
	}

	status = BS->HandleProtocol(img->DeviceHandle, &devid, (void **)&imgpath);
	if (status == EFI_SUCCESS) {
		text = efi_devpath_name(imgpath);
		if (text != NULL) {
			printf("   Load Device: %S\n", text);
			efi_setenv_freebsd_wcs("LoaderDev", text);
			efi_free_devpath_name(text);
		}
	}

	boot_current = 0;
	sz = sizeof(boot_current);
	efi_global_getenv("BootCurrent", &boot_current, &sz);
	printf("   BootCurrent: %04x\n", boot_current);

	sz = sizeof(boot_order);
	efi_global_getenv("BootOrder", &boot_order, &sz);
	printf("   BootOrder:");
	for (i = 0; i < sz / sizeof(boot_order[0]); i++)
		printf(" %04x%s", boot_order[i],
		    boot_order[i] == boot_current ? "[*]" : "");
	printf("\n");

	/*
	 * Disable the watchdog timer. By default the boot manager sets
	 * the timer to 5 minutes before invoking a boot option. If we
	 * want to return to the boot manager, we have to disable the
	 * watchdog timer and since we're an interactive program, we don't
	 * want to wait until the user types "quit". The timer may have
	 * fired by then. We don't care if this fails. It does not prevent
	 * normal functioning in any way...
	 */
	BS->SetWatchdogTimer(0, 0, 0, NULL);

	/*
	 * Try and find a good currdev based on the image that was booted.
	 * It might be desirable here to have a short pause to allow falling
	 * through to the boot loader instead of returning instantly to follow
	 * the boot protocol and also allow an escape hatch for users wishing
	 * to try something different.
	 */
	if (find_currdev(img) != 0)
		if (!interactive_interrupt("Failed to find bootable partition"))
			return (EFI_NOT_FOUND);

	efi_init_environment();
	setenv("LINES", "24", 1);	/* optional */

#if !defined(__arm__)
	for (k = 0; k < ST->NumberOfTableEntries; k++) {
		guid = &ST->ConfigurationTable[k].VendorGuid;
		if (!memcmp(guid, &smbios, sizeof(EFI_GUID))) {
			char buf[40];

			snprintf(buf, sizeof(buf), "%p",
			    ST->ConfigurationTable[k].VendorTable);
			setenv("hint.smbios.0.mem", buf, 1);
			smbios_detect(ST->ConfigurationTable[k].VendorTable);
			break;
		}
	}
#endif

	interact();			/* doesn't return */

	return (EFI_SUCCESS);		/* keep compiler happy */
}

COMMAND_SET(reboot, "reboot", "reboot the system", command_reboot);

static int
command_reboot(int argc, char *argv[])
{
	int i;

	for (i = 0; devsw[i] != NULL; ++i)
		if (devsw[i]->dv_cleanup != NULL)
			(devsw[i]->dv_cleanup)();

	RS->ResetSystem(EfiResetCold, EFI_SUCCESS, 0, NULL);

	/* NOTREACHED */
	return (CMD_ERROR);
}

COMMAND_SET(quit, "quit", "exit the loader", command_quit);

static int
command_quit(int argc, char *argv[])
{
	exit(0);
	return (CMD_OK);
}

COMMAND_SET(memmap, "memmap", "print memory map", command_memmap);

static int
command_memmap(int argc, char *argv[])
{
	UINTN sz;
	EFI_MEMORY_DESCRIPTOR *map, *p;
	UINTN key, dsz;
	UINT32 dver;
	EFI_STATUS status;
	int i, ndesc;
	char line[80];
	static char *types[] = {
	    "Reserved",
	    "LoaderCode",
	    "LoaderData",
	    "BootServicesCode",
	    "BootServicesData",
	    "RuntimeServicesCode",
	    "RuntimeServicesData",
	    "ConventionalMemory",
	    "UnusableMemory",
	    "ACPIReclaimMemory",
	    "ACPIMemoryNVS",
	    "MemoryMappedIO",
	    "MemoryMappedIOPortSpace",
	    "PalCode"
	};

	sz = 0;
	status = BS->GetMemoryMap(&sz, 0, &key, &dsz, &dver);
	if (status != EFI_BUFFER_TOO_SMALL) {
		printf("Can't determine memory map size\n");
		return (CMD_ERROR);
	}
	map = malloc(sz);
	status = BS->GetMemoryMap(&sz, map, &key, &dsz, &dver);
	if (EFI_ERROR(status)) {
		printf("Can't read memory map\n");
		return (CMD_ERROR);
	}

	ndesc = sz / dsz;
	snprintf(line, sizeof(line), "%23s %12s %12s %8s %4s\n",
	    "Type", "Physical", "Virtual", "#Pages", "Attr");
	pager_open();
	if (pager_output(line)) {
		pager_close();
		return (CMD_OK);
	}

	for (i = 0, p = map; i < ndesc;
	     i++, p = NextMemoryDescriptor(p, dsz)) {
		printf("%23s %012jx %012jx %08jx ", types[p->Type],
		    (uintmax_t)p->PhysicalStart, (uintmax_t)p->VirtualStart,
		    (uintmax_t)p->NumberOfPages);
		if (p->Attribute & EFI_MEMORY_UC)
			printf("UC ");
		if (p->Attribute & EFI_MEMORY_WC)
			printf("WC ");
		if (p->Attribute & EFI_MEMORY_WT)
			printf("WT ");
		if (p->Attribute & EFI_MEMORY_WB)
			printf("WB ");
		if (p->Attribute & EFI_MEMORY_UCE)
			printf("UCE ");
		if (p->Attribute & EFI_MEMORY_WP)
			printf("WP ");
		if (p->Attribute & EFI_MEMORY_RP)
			printf("RP ");
		if (p->Attribute & EFI_MEMORY_XP)
			printf("XP ");
		if (pager_output("\n"))
			break;
	}

	pager_close();
	return (CMD_OK);
}

COMMAND_SET(configuration, "configuration", "print configuration tables",
    command_configuration);

static const char *
guid_to_string(EFI_GUID *guid)
{
	static char buf[40];

	sprintf(buf, "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
	    guid->Data1, guid->Data2, guid->Data3, guid->Data4[0],
	    guid->Data4[1], guid->Data4[2], guid->Data4[3], guid->Data4[4],
	    guid->Data4[5], guid->Data4[6], guid->Data4[7]);
	return (buf);
}

static int
command_configuration(int argc, char *argv[])
{
	char line[80];
	UINTN i;

	snprintf(line, sizeof(line), "NumberOfTableEntries=%lu\n",
		(unsigned long)ST->NumberOfTableEntries);
	pager_open();
	if (pager_output(line)) {
		pager_close();
		return (CMD_OK);
	}

	for (i = 0; i < ST->NumberOfTableEntries; i++) {
		EFI_GUID *guid;

		printf("  ");
		guid = &ST->ConfigurationTable[i].VendorGuid;
		if (!memcmp(guid, &mps, sizeof(EFI_GUID)))
			printf("MPS Table");
		else if (!memcmp(guid, &acpi, sizeof(EFI_GUID)))
			printf("ACPI Table");
		else if (!memcmp(guid, &acpi20, sizeof(EFI_GUID)))
			printf("ACPI 2.0 Table");
		else if (!memcmp(guid, &smbios, sizeof(EFI_GUID)))
			printf("SMBIOS Table %p",
			    ST->ConfigurationTable[i].VendorTable);
		else if (!memcmp(guid, &smbios3, sizeof(EFI_GUID)))
			printf("SMBIOS3 Table");
		else if (!memcmp(guid, &dxe, sizeof(EFI_GUID)))
			printf("DXE Table");
		else if (!memcmp(guid, &hoblist, sizeof(EFI_GUID)))
			printf("HOB List Table");
		else if (!memcmp(guid, &lzmadecomp, sizeof(EFI_GUID)))
			printf("LZMA Compression");
		else if (!memcmp(guid, &mpcore, sizeof(EFI_GUID)))
			printf("ARM MpCore Information Table");
		else if (!memcmp(guid, &esrt, sizeof(EFI_GUID)))
			printf("ESRT Table");
		else if (!memcmp(guid, &memtype, sizeof(EFI_GUID)))
			printf("Memory Type Information Table");
		else if (!memcmp(guid, &debugimg, sizeof(EFI_GUID)))
			printf("Debug Image Info Table");
		else if (!memcmp(guid, &fdtdtb, sizeof(EFI_GUID)))
			printf("FDT Table");
		else
			printf("Unknown Table (%s)", guid_to_string(guid));
		snprintf(line, sizeof(line), " at %p\n",
		    ST->ConfigurationTable[i].VendorTable);
		if (pager_output(line))
			break;
	}

	pager_close();
	return (CMD_OK);
}


COMMAND_SET(mode, "mode", "change or display EFI text modes", command_mode);

static int
command_mode(int argc, char *argv[])
{
	UINTN cols, rows;
	unsigned int mode;
	int i;
	char *cp;
	char rowenv[8];
	EFI_STATUS status;
	SIMPLE_TEXT_OUTPUT_INTERFACE *conout;
	extern void HO(void);

	conout = ST->ConOut;

	if (argc > 1) {
		mode = strtol(argv[1], &cp, 0);
		if (cp[0] != '\0') {
			printf("Invalid mode\n");
			return (CMD_ERROR);
		}
		status = conout->QueryMode(conout, mode, &cols, &rows);
		if (EFI_ERROR(status)) {
			printf("invalid mode %d\n", mode);
			return (CMD_ERROR);
		}
		status = conout->SetMode(conout, mode);
		if (EFI_ERROR(status)) {
			printf("couldn't set mode %d\n", mode);
			return (CMD_ERROR);
		}
		sprintf(rowenv, "%u", (unsigned)rows);
		setenv("LINES", rowenv, 1);
		HO();		/* set cursor */
		return (CMD_OK);
	}

	printf("Current mode: %d\n", conout->Mode->Mode);
	for (i = 0; i <= conout->Mode->MaxMode; i++) {
		status = conout->QueryMode(conout, i, &cols, &rows);
		if (EFI_ERROR(status))
			continue;
		printf("Mode %d: %u columns, %u rows\n", i, (unsigned)cols,
		    (unsigned)rows);
	}

	if (i != 0)
		printf("Select a mode with the command \"mode <number>\"\n");

	return (CMD_OK);
}

#ifdef LOADER_FDT_SUPPORT
extern int command_fdt_internal(int argc, char *argv[]);

/*
 * Since proper fdt command handling function is defined in fdt_loader_cmd.c,
 * and declaring it as extern is in contradiction with COMMAND_SET() macro
 * (which uses static pointer), we're defining wrapper function, which
 * calls the proper fdt handling routine.
 */
static int
command_fdt(int argc, char *argv[])
{

	return (command_fdt_internal(argc, argv));
}

COMMAND_SET(fdt, "fdt", "flattened device tree handling", command_fdt);
#endif

/*
 * Chain load another efi loader.
 */
static int
command_chain(int argc, char *argv[])
{
	EFI_GUID LoadedImageGUID = LOADED_IMAGE_PROTOCOL;
	EFI_HANDLE loaderhandle;
	EFI_LOADED_IMAGE *loaded_image;
	EFI_STATUS status;
	struct stat st;
	struct devdesc *dev;
	char *name, *path;
	void *buf;
	int fd;

	if (argc < 2) {
		command_errmsg = "wrong number of arguments";
		return (CMD_ERROR);
	}

	name = argv[1];

	if ((fd = open(name, O_RDONLY)) < 0) {
		command_errmsg = "no such file";
		return (CMD_ERROR);
	}

	if (fstat(fd, &st) < -1) {
		command_errmsg = "stat failed";
		close(fd);
		return (CMD_ERROR);
	}

	status = BS->AllocatePool(EfiLoaderCode, (UINTN)st.st_size, &buf);
	if (status != EFI_SUCCESS) {
		command_errmsg = "failed to allocate buffer";
		close(fd);
		return (CMD_ERROR);
	}
	if (read(fd, buf, st.st_size) != st.st_size) {
		command_errmsg = "error while reading the file";
		(void)BS->FreePool(buf);
		close(fd);
		return (CMD_ERROR);
	}
	close(fd);
	status = BS->LoadImage(FALSE, IH, NULL, buf, st.st_size, &loaderhandle);
	(void)BS->FreePool(buf);
	if (status != EFI_SUCCESS) {
		command_errmsg = "LoadImage failed";
		return (CMD_ERROR);
	}
	status = BS->HandleProtocol(loaderhandle, &LoadedImageGUID,
	    (void **)&loaded_image);

	if (argc > 2) {
		int i, len = 0;
		CHAR16 *argp;

		for (i = 2; i < argc; i++)
			len += strlen(argv[i]) + 1;

		len *= sizeof (*argp);
		loaded_image->LoadOptions = argp = malloc (len);
		loaded_image->LoadOptionsSize = len;
		for (i = 2; i < argc; i++) {
			char *ptr = argv[i];
			while (*ptr)
				*(argp++) = *(ptr++);
			*(argp++) = ' ';
		}
		*(--argv) = 0;
	}

	if (efi_getdev((void **)&dev, name, (const char **)&path) == 0) {
#ifdef EFI_ZFS_BOOT
		struct zfs_devdesc *z_dev;
#endif
		struct disk_devdesc *d_dev;
		pdinfo_t *hd, *pd;

		switch (dev->d_dev->dv_type) {
#ifdef EFI_ZFS_BOOT
		case DEVT_ZFS:
			z_dev = (struct zfs_devdesc *)dev;
			loaded_image->DeviceHandle =
			    efizfs_get_handle_by_guid(z_dev->pool_guid);
			break;
#endif
		case DEVT_NET:
			loaded_image->DeviceHandle =
			    efi_find_handle(dev->d_dev, dev->d_unit);
			break;
		default:
			hd = efiblk_get_pdinfo(dev);
			if (STAILQ_EMPTY(&hd->pd_part)) {
				loaded_image->DeviceHandle = hd->pd_handle;
				break;
			}
			d_dev = (struct disk_devdesc *)dev;
			STAILQ_FOREACH(pd, &hd->pd_part, pd_link) {
				/*
				 * d_partition should be 255
				 */
				if (pd->pd_unit == (uint32_t)d_dev->d_slice) {
					loaded_image->DeviceHandle =
					    pd->pd_handle;
					break;
				}
			}
			break;
		}
	}

	dev_cleanup();
	status = BS->StartImage(loaderhandle, NULL, NULL);
	if (status != EFI_SUCCESS) {
		command_errmsg = "StartImage failed";
		free(loaded_image->LoadOptions);
		loaded_image->LoadOptions = NULL;
		status = BS->UnloadImage(loaded_image);
		return (CMD_ERROR);
	}

	return (CMD_ERROR);	/* not reached */
}

COMMAND_SET(chain, "chain", "chain load file", command_chain);
