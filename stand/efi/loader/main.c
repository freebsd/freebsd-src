/*-
 * Copyright (c) 2008-2010 Rui Paulo
 * Copyright (c) 2006 Marcel Moolenaar
 * All rights reserved.
 *
 * Copyright (c) 2016-2019 Netflix, Inc. written by M. Warner Losh
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

#include <stand.h>

#include <sys/disk.h>
#include <sys/param.h>
#include <sys/reboot.h>
#include <sys/boot.h>
#ifdef EFI_ZFS_BOOT
#include <sys/zfs_bootenv.h>
#endif
#include <paths.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <stdint.h>
#include <string.h>
#include <setjmp.h>
#include <disk.h>
#include <dev_net.h>
#include <net.h>

#include <efi.h>
#include <efilib.h>
#include <efichar.h>
#include <efirng.h>

#include <uuid.h>

#include <bootstrap.h>
#include <smbios.h>

#include "efizfs.h"
#include "framebuffer.h"

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

/*
 * Current boot variable
 */
UINT16 boot_current;

/*
 * Image that we booted from.
 */
EFI_LOADED_IMAGE *boot_img;

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
		status = OpenProtocolByHandle(*walker, &devid, (void **)&path);
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
set_currdev(const char *devname)
{

	env_setenv("currdev", EV_VOLATILE, devname, efi_setcurrdev,
	    env_nounset);
	/*
	 * Don't execute hook here; the loaddev hook makes it immutable
	 * once we've determined what the proper currdev is.
	 */
	env_setenv("loaddev", EV_VOLATILE | EV_NOHOOK, devname, env_noset,
	    env_nounset);
}

static void
set_currdev_devdesc(struct devdesc *currdev)
{
	const char *devname;

	devname = devformat(currdev);
	printf("Setting currdev to %s\n", devname);
	set_currdev(devname);
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
			currdev.d_slice = D_SLICENONE;
			currdev.d_partition = D_PARTNONE;
		} else {
			currdev.dd.d_unit = dp->pd_parent->pd_unit;
			currdev.d_slice = dp->pd_unit;
			currdev.d_partition = D_PARTISGPT; /* XXX Assumes GPT */
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

	return (stat(PATH_DEFAULTS_LOADER_CONF, &st) == 0 ||
#ifdef PATH_BOOTABLE_TOKEN
	    stat(PATH_BOOTABLE_TOKEN, &st) == 0 || /* non-standard layout */
#endif
	    stat(PATH_KERNEL, &st) == 0);
}

#ifdef EFI_ZFS_BOOT
static bool
probe_zfs_currdev(uint64_t guid)
{
	char *devname;
	struct zfs_devdesc currdev;
	char *buf = NULL;
	bool rv;

	currdev.dd.d_dev = &zfs_dev;
	currdev.dd.d_unit = 0;
	currdev.pool_guid = guid;
	currdev.root_guid = 0;
	set_currdev_devdesc((struct devdesc *)&currdev);
	devname = devformat(&currdev.dd);
	init_zfs_boot_options(devname);

	rv = sanity_check_currdev();
	if (rv) {
		buf = malloc(VDEV_PAD_SIZE);
		if (buf != NULL) {
			if (zfs_get_bootonce(&currdev, OS_BOOTONCE, buf,
			    VDEV_PAD_SIZE) == 0) {
				printf("zfs bootonce: %s\n", buf);
				set_currdev(buf);
				setenv("zfs-bootonce", buf, 1);
			}
			free(buf);
			(void) zfs_attach_nvstore(&currdev);
		}
	}
	return (rv);
}
#endif

#ifdef MD_IMAGE_SIZE
static bool
probe_md_currdev(void)
{
	extern struct devsw md_dev;
	bool rv;

	set_currdev_devsw(&md_dev, 0);
	rv = sanity_check_currdev();
	if (!rv)
		printf("MD not present\n");
	return (rv);
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

/*
 * Sometimes we get filenames that are all upper case
 * and/or have backslashes in them. Filter all this out
 * if it looks like we need to do so.
 */
static void
fix_dosisms(char *p)
{
	while (*p) {
		if (isupper(*p))
			*p = tolower(*p);
		else if (*p == '\\')
			*p = '/';
		p++;
	}
}

#define SIZE(dp, edp) (size_t)((intptr_t)(void *)edp - (intptr_t)(void *)dp)

enum { BOOT_INFO_OK = 0, BAD_CHOICE = 1, NOT_SPECIFIC = 2  };
static int
match_boot_info(char *boot_info, size_t bisz)
{
	uint32_t attr;
	uint16_t fplen;
	size_t len;
	char *walker, *ep;
	EFI_DEVICE_PATH *dp, *edp, *first_dp, *last_dp;
	pdinfo_t *pp;
	CHAR16 *descr;
	char *kernel = NULL;
	FILEPATH_DEVICE_PATH  *fp;
	struct stat st;
	CHAR16 *text;

	/*
	 * FreeBSD encodes its boot loading path into the boot loader
	 * BootXXXX variable. We look for the last one in the path
	 * and use that to load the kernel. However, if we only find
	 * one DEVICE_PATH, then there's nothing specific and we should
	 * fall back.
	 *
	 * In an ideal world, we'd look at the image handle we were
	 * passed, match up with the loader we are and then return the
	 * next one in the path. This would be most flexible and cover
	 * many chain booting scenarios where you need to use this
	 * boot loader to get to the next boot loader. However, that
	 * doesn't work. We rarely have the path to the image booted
	 * (just the device) so we can't count on that. So, we do the
	 * next best thing: we look through the device path(s) passed
	 * in the BootXXXX variable. If there's only one, we return
	 * NOT_SPECIFIC. Otherwise, we look at the last one and try to
	 * load that. If we can, we return BOOT_INFO_OK. Otherwise we
	 * return BAD_CHOICE for the caller to sort out.
	 */
	if (bisz < sizeof(attr) + sizeof(fplen) + sizeof(CHAR16))
		return NOT_SPECIFIC;
	walker = boot_info;
	ep = walker + bisz;
	memcpy(&attr, walker, sizeof(attr));
	walker += sizeof(attr);
	memcpy(&fplen, walker, sizeof(fplen));
	walker += sizeof(fplen);
	descr = (CHAR16 *)(intptr_t)walker;
	len = ucs2len(descr);
	walker += (len + 1) * sizeof(CHAR16);
	last_dp = first_dp = dp = (EFI_DEVICE_PATH *)walker;
	edp = (EFI_DEVICE_PATH *)(walker + fplen);
	if ((char *)edp > ep)
		return NOT_SPECIFIC;
	while (dp < edp && SIZE(dp, edp) > sizeof(EFI_DEVICE_PATH)) {
		text = efi_devpath_name(dp);
		if (text != NULL) {
			printf("   BootInfo Path: %S\n", text);
			efi_free_devpath_name(text);
		}
		last_dp = dp;
		dp = (EFI_DEVICE_PATH *)((char *)dp + efi_devpath_length(dp));
	}

	/*
	 * If there's only one item in the list, then nothing was
	 * specified. Or if the last path doesn't have a media
	 * path in it. Those show up as various VenHw() nodes
	 * which are basically opaque to us. Don't count those
	 * as something specifc.
	 */
	if (last_dp == first_dp) {
		printf("Ignoring Boot%04x: Only one DP found\n", boot_current);
		return NOT_SPECIFIC;
	}
	if (efi_devpath_to_media_path(last_dp) == NULL) {
		printf("Ignoring Boot%04x: No Media Path\n", boot_current);
		return NOT_SPECIFIC;
	}

	/*
	 * OK. At this point we either have a good path or a bad one.
	 * Let's check.
	 */
	pp = efiblk_get_pdinfo_by_device_path(last_dp);
	if (pp == NULL) {
		printf("Ignoring Boot%04x: Device Path not found\n", boot_current);
		return BAD_CHOICE;
	}
	set_currdev_pdinfo(pp);
	if (!sanity_check_currdev()) {
		printf("Ignoring Boot%04x: sanity check failed\n", boot_current);
		return BAD_CHOICE;
	}

	/*
	 * OK. We've found a device that matches, next we need to check the last
	 * component of the path. If it's a file, then we set the default kernel
	 * to that. Otherwise, just use this as the default root.
	 *
	 * Reminder: we're running very early, before we've parsed the defaults
	 * file, so we may need to have a hack override.
	 */
	dp = efi_devpath_last_node(last_dp);
	if (DevicePathType(dp) !=  MEDIA_DEVICE_PATH ||
	    DevicePathSubType(dp) != MEDIA_FILEPATH_DP) {
		printf("Using Boot%04x for root partition\n", boot_current);
		return (BOOT_INFO_OK);		/* use currdir, default kernel */
	}
	fp = (FILEPATH_DEVICE_PATH *)dp;
	ucs2_to_utf8(fp->PathName, &kernel);
	if (kernel == NULL) {
		printf("Not using Boot%04x: can't decode kernel\n", boot_current);
		return (BAD_CHOICE);
	}
	if (*kernel == '\\' || isupper(*kernel))
		fix_dosisms(kernel);
	if (stat(kernel, &st) != 0) {
		free(kernel);
		printf("Not using Boot%04x: can't find %s\n", boot_current,
		    kernel);
		return (BAD_CHOICE);
	}
	setenv("kernel", kernel, 1);
	free(kernel);
	text = efi_devpath_name(last_dp);
	if (text) {
		printf("Using Boot%04x %S + %s\n", boot_current, text,
		    kernel);
		efi_free_devpath_name(text);
	}

	return (BOOT_INFO_OK);
}

/*
 * Look at the passed-in boot_info, if any. If we find it then we need
 * to see if we can find ourselves in the boot chain. If we can, and
 * there's another specified thing to boot next, assume that the file
 * is loaded from / and use that for the root filesystem. If can't
 * find the specified thing, we must fail the boot. If we're last on
 * the list, then we fallback to looking for the first available /
 * candidate (ZFS, if there's a bootable zpool, otherwise a UFS
 * partition that has either /boot/defaults/loader.conf on it or
 * /boot/kernel/kernel (the default kernel) that we can use.
 *
 * We always fail if we can't find the right thing. However, as
 * a concession to buggy UEFI implementations, like u-boot, if
 * we have determined that the host is violating the UEFI boot
 * manager protocol, we'll signal the rest of the program that
 * a drop to the OK boot loader prompt is possible.
 */
static int
find_currdev(bool do_bootmgr, bool is_last,
    char *boot_info, size_t boot_info_sz)
{
	pdinfo_t *dp, *pp;
	EFI_DEVICE_PATH *devpath, *copy;
	EFI_HANDLE h;
	CHAR16 *text;
	struct devsw *dev;
	int unit;
	uint64_t extra;
	int rv;
	char *rootdev;

	/*
	 * First choice: if rootdev is already set, use that, even if
	 * it's wrong.
	 */
	rootdev = getenv("rootdev");
	if (rootdev != NULL) {
		printf("    Setting currdev to configured rootdev %s\n",
		    rootdev);
		set_currdev(rootdev);
		return (0);
	}

	/*
	 * Second choice: If uefi_rootdev is set, translate that UEFI device
	 * path to the loader's internal name and use that.
	 */
	do {
		rootdev = getenv("uefi_rootdev");
		if (rootdev == NULL)
			break;
		devpath = efi_name_to_devpath(rootdev);
		if (devpath == NULL)
			break;
		dp = efiblk_get_pdinfo_by_device_path(devpath);
		efi_devpath_free(devpath);
		if (dp == NULL)
			break;
		printf("    Setting currdev to UEFI path %s\n",
		    rootdev);
		set_currdev_pdinfo(dp);
		return (0);
	} while (0);

	/*
	 * Third choice: If we can find out image boot_info, and there's
	 * a follow-on boot image in that boot_info, use that. In this
	 * case root will be the partition specified in that image and
	 * we'll load the kernel specified by the file path. Should there
	 * not be a filepath, we use the default. This filepath overrides
	 * loader.conf.
	 */
	if (do_bootmgr) {
		rv = match_boot_info(boot_info, boot_info_sz);
		switch (rv) {
		case BOOT_INFO_OK:	/* We found it */
			return (0);
		case BAD_CHOICE:	/* specified file not found -> error */
			/* XXX do we want to have an escape hatch for last in boot order? */
			return (ENOENT);
		} /* Nothing specified, try normal match */
	}

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

#ifdef MD_IMAGE_SIZE
	/*
	 * If there is an embedded MD, try to use that.
	 */
	printf("Trying MD\n");
	if (probe_md_currdev())
		return (0);
#endif /* MD_IMAGE_SIZE */

	/*
	 * Try to find the block device by its handle based on the
	 * image we're booting. If we can't find a sane partition,
	 * search all the other partitions of the disk. We do not
	 * search other disks because it's a violation of the UEFI
	 * boot protocol to do so. We fail and let UEFI go on to
	 * the next candidate.
	 */
	dp = efiblk_get_pdinfo_by_handle(boot_img->DeviceHandle);
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
			pdinfo_t *espdp = dp;
			dp = dp->pd_parent;
			STAILQ_FOREACH(pp, &dp->pd_part, pd_link) {
				/* Already tried the ESP */
				if (espdp == pp)
					continue;
				/*
				 * Roll up the ZFS special case
				 * for those partitions that have
				 * zpools on them.
				 */
				text = efi_devpath_name(pp->pd_devpath);
				if (text != NULL) {
					printf("Trying: %S\n", text);
					efi_free_devpath_name(text);
				}
				if (try_as_currdev(dp, pp))
					return (0);
			}
		}
	}

	/*
	 * Try the device handle from our loaded image first.  If that
	 * fails, use the device path from the loaded image and see if
	 * any of the nodes in that path match one of the enumerated
	 * handles. Currently, this handle list is only for netboot.
	 */
	if (efi_handle_lookup(boot_img->DeviceHandle, &dev, &unit, &extra) == 0) {
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

static int
parse_args(int argc, CHAR16 *argv[])
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
	for (i = 0; i < argc; i++) {
		cpy16to8(argv[i], var, sizeof(var));
		howto |= boot_parse_arg(var);
	}

	return (howto);
}

static void
setenv_int(const char *key, int val)
{
	char buf[20];

	snprintf(buf, sizeof(buf), "%d", val);
	setenv(key, buf, 1);
}

/*
 * Parse ConOut (the list of consoles active) and see if we can find a
 * serial port and/or a video port. It would be nice to also walk the
 * ACPI name space to map the UID for the serial port to a port. The
 * latter is especially hard.
 */
int
parse_uefi_con_out(void)
{
	int how, rv;
	int vid_seen = 0, com_seen = 0, seen = 0;
	size_t sz;
	char buf[4096], *ep;
	EFI_DEVICE_PATH *node;
	ACPI_HID_DEVICE_PATH  *acpi;
	UART_DEVICE_PATH  *uart;
	bool pci_pending;

	how = 0;
	sz = sizeof(buf);
	rv = efi_global_getenv("ConOut", buf, &sz);
	if (rv != EFI_SUCCESS)
		rv = efi_global_getenv("ConOutDev", buf, &sz);
	if (rv != EFI_SUCCESS) {
		/*
		 * If we don't have any ConOut default to both. If we have GOP
		 * make video primary, otherwise just make serial primary. In
		 * either case, try to use both the 'efi' console which will use
		 * the GOP, if present and serial. If there's an EFI BIOS that
		 * omits this, but has a serial port redirect, we'll
		 * unavioidably get doubled characters (but we'll be right in
		 * all the other more common cases).
		 */
		if (efi_has_gop())
			how = RB_MULTIPLE;
		else
			how = RB_MULTIPLE | RB_SERIAL;
		setenv("console", "efi,comconsole", 1);
		goto out;
	}
	ep = buf + sz;
	node = (EFI_DEVICE_PATH *)buf;
	while ((char *)node < ep) {
		if (IsDevicePathEndType(node)) {
			if (pci_pending && vid_seen == 0)
				vid_seen = ++seen;
		}
		pci_pending = false;
		if (DevicePathType(node) == ACPI_DEVICE_PATH &&
		    (DevicePathSubType(node) == ACPI_DP ||
		    DevicePathSubType(node) == ACPI_EXTENDED_DP)) {
			/* Check for Serial node */
			acpi = (void *)node;
			if (EISA_ID_TO_NUM(acpi->HID) == 0x501) {
				setenv_int("efi_8250_uid", acpi->UID);
				com_seen = ++seen;
			}
		} else if (DevicePathType(node) == MESSAGING_DEVICE_PATH &&
		    DevicePathSubType(node) == MSG_UART_DP) {
			com_seen = ++seen;
			uart = (void *)node;
			setenv_int("efi_com_speed", uart->BaudRate);
		} else if (DevicePathType(node) == ACPI_DEVICE_PATH &&
		    DevicePathSubType(node) == ACPI_ADR_DP) {
			/* Check for AcpiAdr() Node for video */
			vid_seen = ++seen;
		} else if (DevicePathType(node) == HARDWARE_DEVICE_PATH &&
		    DevicePathSubType(node) == HW_PCI_DP) {
			/*
			 * Note, vmware fusion has a funky console device
			 *	PciRoot(0x0)/Pci(0xf,0x0)
			 * which we can only detect at the end since we also
			 * have to cope with:
			 *	PciRoot(0x0)/Pci(0x1f,0x0)/Serial(0x1)
			 * so only match it if it's last.
			 */
			pci_pending = true;
		}
		node = NextDevicePathNode(node);
	}

	/*
	 * Truth table for RB_MULTIPLE | RB_SERIAL
	 * Value		Result
	 * 0			Use only video console
	 * RB_SERIAL		Use only serial console
	 * RB_MULTIPLE		Use both video and serial console
	 *			(but video is primary so gets rc messages)
	 * both			Use both video and serial console
	 *			(but serial is primary so gets rc messages)
	 *
	 * Try to honor this as best we can. If only one of serial / video
	 * found, then use that. Otherwise, use the first one we found.
	 * This also implies if we found nothing, default to video.
	 */
	how = 0;
	if (vid_seen && com_seen) {
		how |= RB_MULTIPLE;
		if (com_seen < vid_seen)
			how |= RB_SERIAL;
	} else if (com_seen)
		how |= RB_SERIAL;
out:
	return (how);
}

void
parse_loader_efi_config(EFI_HANDLE h, const char *env_fn)
{
	pdinfo_t *dp;
	struct stat st;
	int fd = -1;
	char *env = NULL;

	dp = efiblk_get_pdinfo_by_handle(h);
	if (dp == NULL)
		return;
	set_currdev_pdinfo(dp);
	if (stat(env_fn, &st) != 0)
		return;
	fd = open(env_fn, O_RDONLY);
	if (fd == -1)
		return;
	env = malloc(st.st_size + 1);
	if (env == NULL)
		goto out;
	if (read(fd, env, st.st_size) != st.st_size)
		goto out;
	env[st.st_size] = '\0';
	boot_parse_cmdline(env);
out:
	free(env);
	close(fd);
}

static void
read_loader_env(const char *name, char *def_fn, bool once)
{
	UINTN len;
	char *fn, *freeme = NULL;

	len = 0;
	fn = def_fn;
	if (efi_freebsd_getenv(name, NULL, &len) == EFI_BUFFER_TOO_SMALL) {
		freeme = fn = malloc(len + 1);
		if (fn != NULL) {
			if (efi_freebsd_getenv(name, fn, &len) != EFI_SUCCESS) {
				free(fn);
				fn = NULL;
				printf(
			    "Can't fetch FreeBSD::%s we know is there\n", name);
			} else {
				/*
				 * if tagged as 'once' delete the env variable so we
				 * only use it once.
				 */
				if (once)
					efi_freebsd_delenv(name);
				/*
				 * We malloced 1 more than len above, then redid the call.
				 * so now we have room at the end of the string to NUL terminate
				 * it here, even if the typical idium would have '- 1' here to
				 * not overflow. len should be the same on return both times.
				 */
				fn[len] = '\0';
			}
		} else {
			printf(
		    "Can't allocate %d bytes to fetch FreeBSD::%s env var\n",
			    len, name);
		}
	}
	if (fn) {
		printf("    Reading loader env vars from %s\n", fn);
		parse_loader_efi_config(boot_img->DeviceHandle, fn);
	}
}

caddr_t
ptov(uintptr_t x)
{
	return ((caddr_t)x);
}

EFI_STATUS
main(int argc, CHAR16 *argv[])
{
	EFI_GUID *guid;
	int howto, i, uhowto;
	UINTN k;
	bool has_kbd, is_last;
	char *s;
	EFI_DEVICE_PATH *imgpath;
	CHAR16 *text;
	EFI_STATUS rv;
	size_t sz, bosz = 0, bisz = 0;
	UINT16 boot_order[100];
	char boot_info[4096];
	char buf[32];
	bool uefi_boot_mgr;

	archsw.arch_autoload = efi_autoload;
	archsw.arch_getdev = efi_getdev;
	archsw.arch_copyin = efi_copyin;
	archsw.arch_copyout = efi_copyout;
#ifdef __amd64__
	archsw.arch_hypervisor = x86_hypervisor;
#endif
	archsw.arch_readin = efi_readin;
	archsw.arch_zfs_probe = efi_zfs_probe;

        /* Get our loaded image protocol interface structure. */
	(void) OpenProtocolByHandle(IH, &imgid, (void **)&boot_img);

	/*
	 * Chicken-and-egg problem; we want to have console output early, but
	 * some console attributes may depend on reading from eg. the boot
	 * device, which we can't do yet.  We can use printf() etc. once this is
	 * done. So, we set it to the efi console, then call console init. This
	 * gets us printf early, but also primes the pump for all future console
	 * changes to take effect, regardless of where they come from.
	 */
	setenv("console", "efi", 1);
	uhowto = parse_uefi_con_out();
#if defined(__riscv)
	/*
	 * This workaround likely is papering over a real issue
	 */
	if ((uhowto & RB_SERIAL) != 0)
		setenv("console", "comconsole", 1);
#endif
	cons_probe();

	/* Set up currdev variable to have hooks in place. */
	env_setenv("currdev", EV_VOLATILE, "", efi_setcurrdev, env_nounset);

	/* Init the time source */
	efi_time_init();

	/*
	 * Initialise the block cache. Set the upper limit.
	 */
	bcache_init(32768, 512);

	/*
	 * Scan the BLOCK IO MEDIA handles then
	 * march through the device switch probing for things.
	 */
	i = efipart_inithandles();
	if (i != 0 && i != ENOENT) {
		printf("efipart_inithandles failed with ERRNO %d, expect "
		    "failures\n", i);
	}

	for (i = 0; devsw[i] != NULL; i++)
		if (devsw[i]->dv_init != NULL)
			(devsw[i]->dv_init)();

	/*
	 * Detect console settings two different ways: one via the command
	 * args (eg -h) or via the UEFI ConOut variable.
	 */
	has_kbd = has_keyboard();
	howto = parse_args(argc, argv);
	if (!has_kbd && (howto & RB_PROBE))
		howto |= RB_SERIAL | RB_MULTIPLE;
	howto &= ~RB_PROBE;

	/*
	 * Read additional environment variables from the boot device's
	 * "LoaderEnv" file. Any boot loader environment variable may be set
	 * there, which are subtly different than loader.conf variables. Only
	 * the 'simple' ones may be set so things like foo_load="YES" won't work
	 * for two reasons.  First, the parser is simplistic and doesn't grok
	 * quotes.  Second, because the variables that cause an action to happen
	 * are parsed by the lua, 4th or whatever code that's not yet
	 * loaded. This is relative to the root directory when loader.efi is
	 * loaded off the UFS root drive (when chain booted), or from the ESP
	 * when directly loaded by the BIOS.
	 *
	 * We also read in NextLoaderEnv if it was specified. This allows next boot
	 * functionality to be implemented and to override anything in LoaderEnv.
	 */
	read_loader_env("LoaderEnv", "/efi/freebsd/loader.env", false);
	read_loader_env("NextLoaderEnv", NULL, true);

	/*
	 * We now have two notions of console. howto should be viewed as
	 * overrides. If console is already set, don't set it again.
	 */
#define	VIDEO_ONLY	0
#define	SERIAL_ONLY	RB_SERIAL
#define	VID_SER_BOTH	RB_MULTIPLE
#define	SER_VID_BOTH	(RB_SERIAL | RB_MULTIPLE)
#define	CON_MASK	(RB_SERIAL | RB_MULTIPLE)
	if (strcmp(getenv("console"), "efi") == 0) {
		if ((howto & CON_MASK) == 0) {
			/* No override, uhowto is controlling and efi cons is perfect */
			howto = howto | (uhowto & CON_MASK);
		} else if ((howto & CON_MASK) == (uhowto & CON_MASK)) {
			/* override matches what UEFI told us, efi console is perfect */
		} else if ((uhowto & (CON_MASK)) != 0) {
			/*
			 * We detected a serial console on ConOut. All possible
			 * overrides include serial. We can't really override what efi
			 * gives us, so we use it knowing it's the best choice.
			 */
			/* Do nothing */
		} else {
			/*
			 * We detected some kind of serial in the override, but ConOut
			 * has no serial, so we have to sort out which case it really is.
			 */
			switch (howto & CON_MASK) {
			case SERIAL_ONLY:
				setenv("console", "comconsole", 1);
				break;
			case VID_SER_BOTH:
				setenv("console", "efi comconsole", 1);
				break;
			case SER_VID_BOTH:
				setenv("console", "comconsole efi", 1);
				break;
				/* case VIDEO_ONLY can't happen -- it's the first if above */
			}
		}
	}

	/*
	 * howto is set now how we want to export the flags to the kernel, so
	 * set the env based on it.
	 */
	boot_howto_to_env(howto);

	if (efi_copy_init()) {
		printf("failed to allocate staging area\n");
		return (EFI_BUFFER_TOO_SMALL);
	}

	if ((s = getenv("fail_timeout")) != NULL)
		fail_timeout = strtol(s, NULL, 10);

	printf("%s\n", bootprog_info);
	printf("   Command line arguments:");
	for (i = 0; i < argc; i++)
		printf(" %S", argv[i]);
	printf("\n");

	printf("   Image base: 0x%lx\n", (unsigned long)boot_img->ImageBase);
	printf("   EFI version: %d.%02d\n", ST->Hdr.Revision >> 16,
	    ST->Hdr.Revision & 0xffff);
	printf("   EFI Firmware: %S (rev %d.%02d)\n", ST->FirmwareVendor,
	    ST->FirmwareRevision >> 16, ST->FirmwareRevision & 0xffff);
	printf("   Console: %s (%#x)\n", getenv("console"), howto);

	/* Determine the devpath of our image so we can prefer it. */
	text = efi_devpath_name(boot_img->FilePath);
	if (text != NULL) {
		printf("   Load Path: %S\n", text);
		efi_setenv_freebsd_wcs("LoaderPath", text);
		efi_free_devpath_name(text);
	}

	rv = OpenProtocolByHandle(boot_img->DeviceHandle, &devid,
	    (void **)&imgpath);
	if (rv == EFI_SUCCESS) {
		text = efi_devpath_name(imgpath);
		if (text != NULL) {
			printf("   Load Device: %S\n", text);
			efi_setenv_freebsd_wcs("LoaderDev", text);
			efi_free_devpath_name(text);
		}
	}

	if (getenv("uefi_ignore_boot_mgr") != NULL) {
		printf("    Ignoring UEFI boot manager\n");
		uefi_boot_mgr = false;
	} else {
		uefi_boot_mgr = true;
		boot_current = 0;
		sz = sizeof(boot_current);
		rv = efi_global_getenv("BootCurrent", &boot_current, &sz);
		if (rv == EFI_SUCCESS)
			printf("   BootCurrent: %04x\n", boot_current);
		else {
			boot_current = 0xffff;
			uefi_boot_mgr = false;
		}

		sz = sizeof(boot_order);
		rv = efi_global_getenv("BootOrder", &boot_order, &sz);
		if (rv == EFI_SUCCESS) {
			printf("   BootOrder:");
			for (i = 0; i < sz / sizeof(boot_order[0]); i++)
				printf(" %04x%s", boot_order[i],
				    boot_order[i] == boot_current ? "[*]" : "");
			printf("\n");
			is_last = boot_order[(sz / sizeof(boot_order[0])) - 1] == boot_current;
			bosz = sz;
		} else if (uefi_boot_mgr) {
			/*
			 * u-boot doesn't set BootOrder, but otherwise participates in the
			 * boot manager protocol. So we fake it here and don't consider it
			 * a failure.
			 */
			bosz = sizeof(boot_order[0]);
			boot_order[0] = boot_current;
			is_last = true;
		}
	}

	/*
	 * Next, find the boot info structure the UEFI boot manager is
	 * supposed to setup. We need this so we can walk through it to
	 * find where we are in the booting process and what to try to
	 * boot next.
	 */
	if (uefi_boot_mgr) {
		snprintf(buf, sizeof(buf), "Boot%04X", boot_current);
		sz = sizeof(boot_info);
		rv = efi_global_getenv(buf, &boot_info, &sz);
		if (rv == EFI_SUCCESS)
			bisz = sz;
		else
			uefi_boot_mgr = false;
	}

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
	 * Initialize the trusted/forbidden certificates from UEFI.
	 * They will be later used to verify the manifest(s),
	 * which should contain hashes of verified files.
	 * This needs to be initialized before any configuration files
	 * are loaded.
	 */
#ifdef EFI_SECUREBOOT
	ve_efi_init();
#endif

	/*
	 * Try and find a good currdev based on the image that was booted.
	 * It might be desirable here to have a short pause to allow falling
	 * through to the boot loader instead of returning instantly to follow
	 * the boot protocol and also allow an escape hatch for users wishing
	 * to try something different.
	 */
	if (find_currdev(uefi_boot_mgr, is_last, boot_info, bisz) != 0)
		if (uefi_boot_mgr &&
		    !interactive_interrupt("Failed to find bootable partition"))
			return (EFI_NOT_FOUND);

	autoload_font(false);	/* Set up the font list for console. */
	efi_init_environment();

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

COMMAND_SET(efi_seed_entropy, "efi-seed-entropy", "try to get entropy from the EFI RNG", command_seed_entropy);

static int
command_seed_entropy(int argc, char *argv[])
{
	EFI_STATUS status;
	EFI_RNG_PROTOCOL *rng;
	unsigned int size = 2048;
	void *buf;

	if (argc > 1) {
		size = strtol(argv[1], NULL, 0);
	}

	status = BS->LocateProtocol(&rng_guid, NULL, (VOID **)&rng);
	if (status != EFI_SUCCESS) {
		command_errmsg = "RNG protocol not found";
		return (CMD_ERROR);
	}

	if ((buf = malloc(size)) == NULL) {
		command_errmsg = "out of memory";
		return (CMD_ERROR);
	}

	status = rng->GetRNG(rng, NULL, size, (UINT8 *)buf);
	if (status != EFI_SUCCESS) {
		free(buf);
		command_errmsg = "GetRNG failed";
		return (CMD_ERROR);
	}

	if (file_addbuf("efi_rng_seed", "boot_entropy_platform", size, buf) != 0) {
		free(buf);
		return (CMD_ERROR);
	}

	free(buf);
	return (CMD_OK);
}

COMMAND_SET(poweroff, "poweroff", "power off the system", command_poweroff);

static int
command_poweroff(int argc __unused, char *argv[] __unused)
{
	int i;

	for (i = 0; devsw[i] != NULL; ++i)
		if (devsw[i]->dv_cleanup != NULL)
			(devsw[i]->dv_cleanup)();

	RS->ResetSystem(EfiResetShutdown, EFI_SUCCESS, 0, NULL);

	/* NOTREACHED */
	return (CMD_ERROR);
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

COMMAND_SET(memmap, "memmap", "print memory map", command_memmap);

static int
command_memmap(int argc __unused, char *argv[] __unused)
{
	UINTN sz;
	EFI_MEMORY_DESCRIPTOR *map, *p;
	UINTN key, dsz;
	UINT32 dver;
	EFI_STATUS status;
	int i, ndesc;
	char line[80];

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
		snprintf(line, sizeof(line), "%23s %012jx %012jx %08jx ",
		    efi_memory_type(p->Type), (uintmax_t)p->PhysicalStart,
		    (uintmax_t)p->VirtualStart, (uintmax_t)p->NumberOfPages);
		if (pager_output(line))
			break;

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
		if (p->Attribute & EFI_MEMORY_NV)
			printf("NV ");
		if (p->Attribute & EFI_MEMORY_MORE_RELIABLE)
			printf("MR ");
		if (p->Attribute & EFI_MEMORY_RO)
			printf("RO ");
		if (pager_output("\n"))
			break;
	}

	pager_close();
	return (CMD_OK);
}

COMMAND_SET(configuration, "configuration", "print configuration tables",
    command_configuration);

static int
command_configuration(int argc, char *argv[])
{
	UINTN i;
	char *name;

	printf("NumberOfTableEntries=%lu\n",
		(unsigned long)ST->NumberOfTableEntries);

	for (i = 0; i < ST->NumberOfTableEntries; i++) {
		EFI_GUID *guid;

		printf("  ");
		guid = &ST->ConfigurationTable[i].VendorGuid;

		if (efi_guid_to_name(guid, &name) == true) {
			printf(name);
			free(name);
		} else {
			printf("Error while translating UUID to name");
		}
		printf(" at %p\n", ST->ConfigurationTable[i].VendorTable);
	}

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
	EFI_STATUS status;
	SIMPLE_TEXT_OUTPUT_INTERFACE *conout;

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
		(void) cons_update_mode(true);
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

COMMAND_SET(lsefi, "lsefi", "list EFI handles", command_lsefi);

static void
lsefi_print_handle_info(EFI_HANDLE handle)
{
	EFI_DEVICE_PATH *devpath;
	EFI_DEVICE_PATH *imagepath;
	CHAR16 *dp_name;

	imagepath = efi_lookup_image_devpath(handle);
	if (imagepath != NULL) {
		dp_name = efi_devpath_name(imagepath);
		printf("Handle for image %S", dp_name);
		efi_free_devpath_name(dp_name);
		return;
	}
	devpath = efi_lookup_devpath(handle);
	if (devpath != NULL) {
		dp_name = efi_devpath_name(devpath);
		printf("Handle for device %S", dp_name);
		efi_free_devpath_name(dp_name);
		return;
	}
	printf("Handle %p", handle);
}

static int
command_lsefi(int argc __unused, char *argv[] __unused)
{
	char *name;
	EFI_HANDLE *buffer = NULL;
	EFI_HANDLE handle;
	UINTN bufsz = 0, i, j;
	EFI_STATUS status;
	int ret = 0;

	status = BS->LocateHandle(AllHandles, NULL, NULL, &bufsz, buffer);
	if (status != EFI_BUFFER_TOO_SMALL) {
		snprintf(command_errbuf, sizeof (command_errbuf),
		    "unexpected error: %lld", (long long)status);
		return (CMD_ERROR);
	}
	if ((buffer = malloc(bufsz)) == NULL) {
		sprintf(command_errbuf, "out of memory");
		return (CMD_ERROR);
	}

	status = BS->LocateHandle(AllHandles, NULL, NULL, &bufsz, buffer);
	if (EFI_ERROR(status)) {
		free(buffer);
		snprintf(command_errbuf, sizeof (command_errbuf),
		    "LocateHandle() error: %lld", (long long)status);
		return (CMD_ERROR);
	}

	pager_open();
	for (i = 0; i < (bufsz / sizeof (EFI_HANDLE)); i++) {
		UINTN nproto = 0;
		EFI_GUID **protocols = NULL;

		handle = buffer[i];
		lsefi_print_handle_info(handle);
		if (pager_output("\n"))
			break;
		/* device path */

		status = BS->ProtocolsPerHandle(handle, &protocols, &nproto);
		if (EFI_ERROR(status)) {
			snprintf(command_errbuf, sizeof (command_errbuf),
			    "ProtocolsPerHandle() error: %lld",
			    (long long)status);
			continue;
		}

		for (j = 0; j < nproto; j++) {
			if (efi_guid_to_name(protocols[j], &name) == true) {
				printf("  %s", name);
				free(name);
			} else {
				printf("Error while translating UUID to name");
			}
			if ((ret = pager_output("\n")) != 0)
				break;
		}
		BS->FreePool(protocols);
		if (ret != 0)
			break;
	}
	pager_close();
	free(buffer);
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

#ifdef LOADER_VERIEXEC
	if (verify_file(fd, name, 0, VE_MUST, __func__) < 0) {
		sprintf(command_errbuf, "can't verify: %s", name);
		close(fd);
		return (CMD_ERROR);
	}
#endif

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
	status = OpenProtocolByHandle(loaderhandle, &LoadedImageGUID,
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

extern struct in_addr servip;
static int
command_netserver(int argc, char *argv[])
{
	char *proto;
	n_long rootaddr;

	if (argc > 2) {
		command_errmsg = "wrong number of arguments";
		return (CMD_ERROR);
	}
	if (argc < 2) {
		proto = netproto == NET_TFTP ? "tftp://" : "nfs://";
		printf("Netserver URI: %s%s%s\n", proto, intoa(rootip.s_addr),
		    rootpath);
		return (CMD_OK);
	}
	if (argc == 2) {
		strncpy(rootpath, argv[1], sizeof(rootpath));
		rootpath[sizeof(rootpath) -1] = '\0';
		if ((rootaddr = net_parse_rootpath()) != INADDR_NONE)
			servip.s_addr = rootip.s_addr = rootaddr;
		return (CMD_OK);
	}
	return (CMD_ERROR);	/* not reached */

}

COMMAND_SET(netserver, "netserver", "change or display netserver URI",
    command_netserver);
