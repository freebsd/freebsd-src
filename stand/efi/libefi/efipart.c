/*-
 * Copyright (c) 2010 Marcel Moolenaar
 * All rights reserved.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/disk.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/queue.h>
#include <stddef.h>
#include <stdarg.h>

#include <bootstrap.h>

#include <efi.h>
#include <efilib.h>
#include <efiprot.h>
#include <efichar.h>
#include <disk.h>

static EFI_GUID blkio_guid = BLOCK_IO_PROTOCOL;

static int efipart_initfd(void);
static int efipart_initcd(void);
static int efipart_inithd(void);

static int efipart_strategy(void *, int, daddr_t, size_t, char *, size_t *);
static int efipart_realstrategy(void *, int, daddr_t, size_t, char *, size_t *);

static int efipart_open(struct open_file *, ...);
static int efipart_close(struct open_file *);
static int efipart_ioctl(struct open_file *, u_long, void *);

static int efipart_printfd(int);
static int efipart_printcd(int);
static int efipart_printhd(int);

/* EISA PNP ID's for floppy controllers */
#define	PNP0604	0x604
#define	PNP0700	0x700
#define	PNP0701	0x701

/* Bounce buffer max size */
#define	BIO_BUFFER_SIZE	0x4000

struct devsw efipart_fddev = {
	.dv_name = "fd",
	.dv_type = DEVT_FD,
	.dv_init = efipart_initfd,
	.dv_strategy = efipart_strategy,
	.dv_open = efipart_open,
	.dv_close = efipart_close,
	.dv_ioctl = efipart_ioctl,
	.dv_print = efipart_printfd,
	.dv_cleanup = NULL
};

struct devsw efipart_cddev = {
	.dv_name = "cd",
	.dv_type = DEVT_CD,
	.dv_init = efipart_initcd,
	.dv_strategy = efipart_strategy,
	.dv_open = efipart_open,
	.dv_close = efipart_close,
	.dv_ioctl = efipart_ioctl,
	.dv_print = efipart_printcd,
	.dv_cleanup = NULL
};

struct devsw efipart_hddev = {
	.dv_name = "disk",
	.dv_type = DEVT_DISK,
	.dv_init = efipart_inithd,
	.dv_strategy = efipart_strategy,
	.dv_open = efipart_open,
	.dv_close = efipart_close,
	.dv_ioctl = efipart_ioctl,
	.dv_print = efipart_printhd,
	.dv_cleanup = NULL
};

static pdinfo_list_t fdinfo = STAILQ_HEAD_INITIALIZER(fdinfo);
static pdinfo_list_t cdinfo = STAILQ_HEAD_INITIALIZER(cdinfo);
static pdinfo_list_t hdinfo = STAILQ_HEAD_INITIALIZER(hdinfo);

/*
 * efipart_inithandles() is used to build up the pdinfo list from
 * block device handles. Then each devsw init callback is used to
 * pick items from pdinfo and move to proper device list.
 * In ideal world, we should end up with empty pdinfo once all
 * devsw initializers are called.
 */
static pdinfo_list_t pdinfo = STAILQ_HEAD_INITIALIZER(pdinfo);

pdinfo_list_t *
efiblk_get_pdinfo_list(struct devsw *dev)
{
	if (dev->dv_type == DEVT_DISK)
		return (&hdinfo);
	if (dev->dv_type == DEVT_CD)
		return (&cdinfo);
	if (dev->dv_type == DEVT_FD)
		return (&fdinfo);
	return (NULL);
}

/* XXX this gets called way way too often, investigate */
pdinfo_t *
efiblk_get_pdinfo(struct devdesc *dev)
{
	pdinfo_list_t *pdi;
	pdinfo_t *pd = NULL;

	pdi = efiblk_get_pdinfo_list(dev->d_dev);
	if (pdi == NULL)
		return (pd);

	STAILQ_FOREACH(pd, pdi, pd_link) {
		if (pd->pd_unit == dev->d_unit)
			return (pd);
	}
	return (pd);
}

pdinfo_t *
efiblk_get_pdinfo_by_device_path(EFI_DEVICE_PATH *path)
{
	EFI_HANDLE h;
	EFI_STATUS status;
	EFI_DEVICE_PATH *devp = path;

	status = BS->LocateDevicePath(&blkio_guid, &devp, &h);
	if (EFI_ERROR(status))
		return (NULL);
	return (efiblk_get_pdinfo_by_handle(h));
}

static bool
same_handle(pdinfo_t *pd, EFI_HANDLE h)
{

	return (pd->pd_handle == h || pd->pd_alias == h);
}

pdinfo_t *
efiblk_get_pdinfo_by_handle(EFI_HANDLE h)
{
	pdinfo_t *dp, *pp;

	/*
	 * Check hard disks, then cd, then floppy
	 */
	STAILQ_FOREACH(dp, &hdinfo, pd_link) {
		if (same_handle(dp, h))
			return (dp);
		STAILQ_FOREACH(pp, &dp->pd_part, pd_link) {
			if (same_handle(pp, h))
				return (pp);
		}
	}
	STAILQ_FOREACH(dp, &cdinfo, pd_link) {
		if (same_handle(dp, h))
			return (dp);
		STAILQ_FOREACH(pp, &dp->pd_part, pd_link) {
			if (same_handle(pp, h))
				return (pp);
		}
	}
	STAILQ_FOREACH(dp, &fdinfo, pd_link) {
		if (same_handle(dp, h))
			return (dp);
	}
	return (NULL);
}

static int
efiblk_pdinfo_count(pdinfo_list_t *pdi)
{
	pdinfo_t *pd;
	int i = 0;

	STAILQ_FOREACH(pd, pdi, pd_link) {
		i++;
	}
	return (i);
}

int
efipart_inithandles(void)
{
	unsigned i, nin;
	UINTN sz;
	EFI_HANDLE *hin;
	EFI_DEVICE_PATH *devpath;
	EFI_BLOCK_IO *blkio;
	EFI_STATUS status;
	pdinfo_t *pd;

	if (!STAILQ_EMPTY(&pdinfo))
		return (0);

	sz = 0;
	hin = NULL;
	status = BS->LocateHandle(ByProtocol, &blkio_guid, 0, &sz, hin);
	if (status == EFI_BUFFER_TOO_SMALL) {
		hin = malloc(sz);
		status = BS->LocateHandle(ByProtocol, &blkio_guid, 0, &sz,
		    hin);
		if (EFI_ERROR(status))
			free(hin);
	}
	if (EFI_ERROR(status))
		return (efi_status_to_errno(status));

	nin = sz / sizeof(*hin);
#ifdef EFIPART_DEBUG
	printf("%s: Got %d BLOCK IO MEDIA handle(s)\n", __func__, nin);
#endif

	for (i = 0; i < nin; i++) {
		/*
		 * Get devpath and open protocol.
		 * We should not get errors here
		 */
		if ((devpath = efi_lookup_devpath(hin[i])) == NULL)
			continue;

		status = OpenProtocolByHandle(hin[i], &blkio_guid,
		    (void **)&blkio);
		if (EFI_ERROR(status)) {
			printf("error %lu\n", EFI_ERROR_CODE(status));
			continue;
		}

		/*
		 * We assume the block size 512 or greater power of 2.
		 * Also skip devices with block size > 64k (16 is max
		 * ashift supported by zfs).
		 * iPXE is known to insert stub BLOCK IO device with
		 * BlockSize 1.
		 */
		if (blkio->Media->BlockSize < 512 ||
		    blkio->Media->BlockSize > (1 << 16) ||
		    !powerof2(blkio->Media->BlockSize)) {
			continue;
		}

		/* Allowed values are 0, 1 and power of 2. */
		if (blkio->Media->IoAlign > 1 &&
		    !powerof2(blkio->Media->IoAlign)) {
			continue;
		}

		/* This is bad. */
		if ((pd = calloc(1, sizeof(*pd))) == NULL) {
			printf("efipart_inithandles: Out of memory.\n");
			free(hin);
			return (ENOMEM);
		}
		STAILQ_INIT(&pd->pd_part);

		pd->pd_handle = hin[i];
		pd->pd_devpath = devpath;
		pd->pd_blkio = blkio;
		STAILQ_INSERT_TAIL(&pdinfo, pd, pd_link);
	}

	free(hin);
	return (0);
}

static ACPI_HID_DEVICE_PATH *
efipart_floppy(EFI_DEVICE_PATH *node)
{
	ACPI_HID_DEVICE_PATH *acpi;

	if (DevicePathType(node) == ACPI_DEVICE_PATH &&
	    DevicePathSubType(node) == ACPI_DP) {
		acpi = (ACPI_HID_DEVICE_PATH *) node;
		if (acpi->HID == EISA_PNP_ID(PNP0604) ||
		    acpi->HID == EISA_PNP_ID(PNP0700) ||
		    acpi->HID == EISA_PNP_ID(PNP0701)) {
			return (acpi);
		}
	}
	return (NULL);
}

static pdinfo_t *
efipart_find_parent(pdinfo_list_t *pdi, EFI_DEVICE_PATH *devpath)
{
	pdinfo_t *pd, *part;

	STAILQ_FOREACH(pd, pdi, pd_link) {
		if (efi_devpath_is_prefix(pd->pd_devpath, devpath))
			return (pd);
		part = efipart_find_parent(&pd->pd_part, devpath);
		if (part != NULL)
			return (part);
	}
	return (NULL);
}

static int
efipart_initfd(void)
{
	EFI_DEVICE_PATH *node;
	ACPI_HID_DEVICE_PATH *acpi;
	pdinfo_t *parent, *fd;

restart:
	STAILQ_FOREACH(fd, &pdinfo, pd_link) {
		if ((node = efi_devpath_last_node(fd->pd_devpath)) == NULL)
			continue;

		if ((acpi = efipart_floppy(node)) == NULL)
			continue;

		STAILQ_REMOVE(&pdinfo, fd, pdinfo, pd_link);
		parent = efipart_find_parent(&pdinfo, fd->pd_devpath);
		if (parent != NULL) {
			STAILQ_REMOVE(&pdinfo, parent, pdinfo, pd_link);
			parent->pd_alias = fd->pd_handle;
			parent->pd_unit = acpi->UID;
			free(fd);
			fd = parent;
		} else {
			fd->pd_unit = acpi->UID;
		}
		fd->pd_devsw = &efipart_fddev;
		STAILQ_INSERT_TAIL(&fdinfo, fd, pd_link);
		goto restart;
	}

	bcache_add_dev(efiblk_pdinfo_count(&fdinfo));
	return (0);
}

/*
 * Add or update entries with new handle data.
 */
static void
efipart_cdinfo_add(pdinfo_t *cd)
{
	pdinfo_t *pd, *last;

	STAILQ_FOREACH(pd, &cdinfo, pd_link) {
		if (efi_devpath_is_prefix(pd->pd_devpath, cd->pd_devpath)) {
			last = STAILQ_LAST(&pd->pd_part, pdinfo, pd_link);
			if (last != NULL)
				cd->pd_unit = last->pd_unit + 1;
			else
				cd->pd_unit = 0;
			cd->pd_parent = pd;
			cd->pd_devsw = &efipart_cddev;
			STAILQ_INSERT_TAIL(&pd->pd_part, cd, pd_link);
			return;
		}
	}

	last = STAILQ_LAST(&cdinfo, pdinfo, pd_link);
	if (last != NULL)
		cd->pd_unit = last->pd_unit + 1;
	else
		cd->pd_unit = 0;

	cd->pd_parent = NULL;
	cd->pd_devsw = &efipart_cddev;
	STAILQ_INSERT_TAIL(&cdinfo, cd, pd_link);
}

static bool
efipart_testcd(EFI_DEVICE_PATH *node, EFI_BLOCK_IO *blkio)
{
	if (DevicePathType(node) == MEDIA_DEVICE_PATH &&
	    DevicePathSubType(node) == MEDIA_CDROM_DP) {
		return (true);
	}

	/* cd drive without the media. */
	if (blkio->Media->RemovableMedia &&
	    !blkio->Media->MediaPresent) {
		return (true);
	}

	return (false);
}

static void
efipart_updatecd(void)
{
	EFI_DEVICE_PATH *devpath, *node;
	EFI_STATUS status;
	pdinfo_t *parent, *cd;

restart:
	STAILQ_FOREACH(cd, &pdinfo, pd_link) {
		if ((node = efi_devpath_last_node(cd->pd_devpath)) == NULL)
			continue;

		if (efipart_floppy(node) != NULL)
			continue;

		/* Is parent of this device already registered? */
		parent = efipart_find_parent(&cdinfo, cd->pd_devpath);
		if (parent != NULL) {
			STAILQ_REMOVE(&pdinfo, cd, pdinfo, pd_link);
			efipart_cdinfo_add(cd);
			goto restart;
		}

		if (!efipart_testcd(node, cd->pd_blkio))
			continue;

		/* Find parent and unlink both parent and cd from pdinfo */
		STAILQ_REMOVE(&pdinfo, cd, pdinfo, pd_link);
		parent = efipart_find_parent(&pdinfo, cd->pd_devpath);
		if (parent != NULL) {
			STAILQ_REMOVE(&pdinfo, parent, pdinfo, pd_link);
			efipart_cdinfo_add(parent);
		}

		if (parent == NULL)
			parent = efipart_find_parent(&cdinfo, cd->pd_devpath);

		/*
		 * If we come across a logical partition of subtype CDROM
		 * it doesn't refer to the CD filesystem itself, but rather
		 * to any usable El Torito boot image on it. In this case
		 * we try to find the parent device and add that instead as
		 * that will be the CD filesystem.
		 */
		if (DevicePathType(node) == MEDIA_DEVICE_PATH &&
		    DevicePathSubType(node) == MEDIA_CDROM_DP &&
		    parent == NULL) {
			parent = calloc(1, sizeof(*parent));
			if (parent == NULL) {
				printf("efipart_updatecd: out of memory\n");
				/* this device is lost but try again. */
				free(cd);
				goto restart;
			}

			devpath = efi_devpath_trim(cd->pd_devpath);
			if (devpath == NULL) {
				printf("efipart_updatecd: out of memory\n");
				/* this device is lost but try again. */
				free(parent);
				free(cd);
				goto restart;
			}
			parent->pd_devpath = devpath;
			status = BS->LocateDevicePath(&blkio_guid,
			    &parent->pd_devpath, &parent->pd_handle);
			free(devpath);
			if (EFI_ERROR(status)) {
				printf("efipart_updatecd: error %lu\n",
				    EFI_ERROR_CODE(status));
				free(parent);
				free(cd);
				goto restart;
			}
			parent->pd_devpath =
			    efi_lookup_devpath(parent->pd_handle);
			efipart_cdinfo_add(parent);
		}

		efipart_cdinfo_add(cd);
		goto restart;
	}
}

static int
efipart_initcd(void)
{
	efipart_updatecd();

	bcache_add_dev(efiblk_pdinfo_count(&cdinfo));
	return (0);
}

static bool
efipart_hdinfo_add_node(pdinfo_t *hd, EFI_DEVICE_PATH *node)
{
	pdinfo_t *pd, *ptr;

	if (node == NULL)
		return (false);

	/* Find our disk device. */
	STAILQ_FOREACH(pd, &hdinfo, pd_link) {
		if (efi_devpath_is_prefix(pd->pd_devpath, hd->pd_devpath))
			break;
	}
	if (pd == NULL)
		return (false);

	/* If the node is not MEDIA_HARDDRIVE_DP, it is sub-partition. */
	if (DevicePathSubType(node) != MEDIA_HARDDRIVE_DP) {
		STAILQ_FOREACH(ptr, &pd->pd_part, pd_link) {
			if (efi_devpath_is_prefix(ptr->pd_devpath,
			    hd->pd_devpath))
				break;
		}
		/*
		 * ptr == NULL means we have handles in unexpected order
		 * and we would need to re-order the partitions later.
		 */
		if (ptr != NULL)
			pd = ptr;
	}

	/* Add the partition. */
	if (DevicePathSubType(node) == MEDIA_HARDDRIVE_DP) {
		hd->pd_unit = ((HARDDRIVE_DEVICE_PATH *)node)->PartitionNumber;
	} else {
		ptr = STAILQ_LAST(&pd->pd_part, pdinfo, pd_link);
		if (ptr != NULL)
			hd->pd_unit = ptr->pd_unit + 1;
		else
			hd->pd_unit = 0;
	}
	hd->pd_parent = pd;
	hd->pd_devsw = &efipart_hddev;

	STAILQ_INSERT_TAIL(&pd->pd_part, hd, pd_link);
	return (true);
}

static void
efipart_hdinfo_add(pdinfo_t *hd, EFI_DEVICE_PATH *node)
{
	pdinfo_t *pd, *last;

	if (efipart_hdinfo_add_node(hd, node))
		return;

	last = STAILQ_LAST(&hdinfo, pdinfo, pd_link);
	if (last != NULL)
		hd->pd_unit = last->pd_unit + 1;
	else
		hd->pd_unit = 0;

	/* Add the disk. */
	hd->pd_devsw = &efipart_hddev;
	STAILQ_INSERT_TAIL(&hdinfo, hd, pd_link);
}

/*
 * The MEDIA_FILEPATH_DP has device name.
 * From U-Boot sources it looks like names are in the form
 * of typeN:M, where type is interface type, N is disk id
 * and M is partition id.
 */
static void
efipart_hdinfo_add_filepath(pdinfo_t *hd, FILEPATH_DEVICE_PATH *node)
{
	char *pathname, *p;
	int len;
	pdinfo_t *last;

	last = STAILQ_LAST(&hdinfo, pdinfo, pd_link);
	if (last != NULL)
		hd->pd_unit = last->pd_unit + 1;
	else
		hd->pd_unit = 0;

	/* FILEPATH_DEVICE_PATH has 0 terminated string */
	len = ucs2len(node->PathName);
	if ((pathname = malloc(len + 1)) == NULL) {
		printf("Failed to add disk, out of memory\n");
		free(hd);
		return;
	}
	cpy16to8(node->PathName, pathname, len + 1);
	p = strchr(pathname, ':');

	/*
	 * Assume we are receiving handles in order, first disk handle,
	 * then partitions for this disk. If this assumption proves
	 * false, this code would need update.
	 */
	if (p == NULL) {	/* no colon, add the disk */
		hd->pd_devsw = &efipart_hddev;
		STAILQ_INSERT_TAIL(&hdinfo, hd, pd_link);
		free(pathname);
		return;
	}
	p++;	/* skip the colon */
	errno = 0;
	hd->pd_unit = (int)strtol(p, NULL, 0);
	if (errno != 0) {
		printf("Bad unit number for partition \"%s\"\n", pathname);
		free(pathname);
		free(hd);
		return;
	}

	/*
	 * We should have disk registered, if not, we are receiving
	 * handles out of order, and this code should be reworked
	 * to create "blank" disk for partition, and to find the
	 * disk based on PathName compares.
	 */
	if (last == NULL) {
		printf("BUG: No disk for partition \"%s\"\n", pathname);
		free(pathname);
		free(hd);
		return;
	}
	/* Add the partition. */
	hd->pd_parent = last;
	hd->pd_devsw = &efipart_hddev;
	STAILQ_INSERT_TAIL(&last->pd_part, hd, pd_link);
	free(pathname);
}

static void
efipart_updatehd(void)
{
	EFI_DEVICE_PATH *devpath, *node;
	EFI_STATUS status;
	pdinfo_t *parent, *hd;

restart:
	STAILQ_FOREACH(hd, &pdinfo, pd_link) {
		if ((node = efi_devpath_last_node(hd->pd_devpath)) == NULL)
			continue;

		if (efipart_floppy(node) != NULL)
			continue;

		if (efipart_testcd(node, hd->pd_blkio))
			continue;

		if (DevicePathType(node) == HARDWARE_DEVICE_PATH &&
		    (DevicePathSubType(node) == HW_PCI_DP ||
		     DevicePathSubType(node) == HW_VENDOR_DP)) {
			STAILQ_REMOVE(&pdinfo, hd, pdinfo, pd_link);
			efipart_hdinfo_add(hd, NULL);
			goto restart;
		}

		if (DevicePathType(node) == MEDIA_DEVICE_PATH &&
		    DevicePathSubType(node) == MEDIA_FILEPATH_DP) {
			STAILQ_REMOVE(&pdinfo, hd, pdinfo, pd_link);
			efipart_hdinfo_add_filepath(hd,
			    (FILEPATH_DEVICE_PATH *)node);
			goto restart;
		}

		STAILQ_REMOVE(&pdinfo, hd, pdinfo, pd_link);
		parent = efipart_find_parent(&pdinfo, hd->pd_devpath);
		if (parent != NULL) {
			STAILQ_REMOVE(&pdinfo, parent, pdinfo, pd_link);
			efipart_hdinfo_add(parent, NULL);
		} else {
			parent = efipart_find_parent(&hdinfo, hd->pd_devpath);
		}

		if (DevicePathType(node) == MEDIA_DEVICE_PATH &&
		    DevicePathSubType(node) == MEDIA_HARDDRIVE_DP &&
		    parent == NULL) {
			parent = calloc(1, sizeof(*parent));
			if (parent == NULL) {
				printf("efipart_updatehd: out of memory\n");
				/* this device is lost but try again. */
				free(hd);
				goto restart;
			}

			devpath = efi_devpath_trim(hd->pd_devpath);
			if (devpath == NULL) {
				printf("efipart_updatehd: out of memory\n");
				/* this device is lost but try again. */
				free(parent);
				free(hd);
				goto restart;
			}

			parent->pd_devpath = devpath;
			status = BS->LocateDevicePath(&blkio_guid,
			    &parent->pd_devpath, &parent->pd_handle);
			free(devpath);
			if (EFI_ERROR(status)) {
				printf("efipart_updatehd: error %lu\n",
				    EFI_ERROR_CODE(status));
				free(parent);
				free(hd);
				goto restart;
			}

			parent->pd_devpath =
			    efi_lookup_devpath(&parent->pd_handle);

			efipart_hdinfo_add(parent, NULL);
		}

		efipart_hdinfo_add(hd, node);
		goto restart;
	}
}

static int
efipart_inithd(void)
{

	efipart_updatehd();

	bcache_add_dev(efiblk_pdinfo_count(&hdinfo));
	return (0);
}

static int
efipart_print_common(struct devsw *dev, pdinfo_list_t *pdlist, int verbose)
{
	int ret = 0;
	EFI_BLOCK_IO *blkio;
	EFI_STATUS status;
	EFI_HANDLE h;
	pdinfo_t *pd;
	CHAR16 *text;
	struct disk_devdesc pd_dev;
	char line[80];

	if (STAILQ_EMPTY(pdlist))
		return (0);

	printf("%s devices:", dev->dv_name);
	if ((ret = pager_output("\n")) != 0)
		return (ret);

	STAILQ_FOREACH(pd, pdlist, pd_link) {
		h = pd->pd_handle;
		if (verbose) {	/* Output the device path. */
			text = efi_devpath_name(efi_lookup_devpath(h));
			if (text != NULL) {
				printf("  %S", text);
				efi_free_devpath_name(text);
				if ((ret = pager_output("\n")) != 0)
					break;
			}
		}
		snprintf(line, sizeof(line),
		    "    %s%d", dev->dv_name, pd->pd_unit);
		printf("%s:", line);
		status = OpenProtocolByHandle(h, &blkio_guid, (void **)&blkio);
		if (!EFI_ERROR(status)) {
			printf("    %llu",
			    blkio->Media->LastBlock == 0? 0:
			    (unsigned long long) (blkio->Media->LastBlock + 1));
			if (blkio->Media->LastBlock != 0) {
				printf(" X %u", blkio->Media->BlockSize);
			}
			printf(" blocks");
			if (blkio->Media->MediaPresent) {
				if (blkio->Media->RemovableMedia)
					printf(" (removable)");
			} else {
				printf(" (no media)");
			}
			if ((ret = pager_output("\n")) != 0)
				break;
			if (!blkio->Media->MediaPresent)
				continue;

			pd->pd_blkio = blkio;
			pd_dev.dd.d_dev = dev;
			pd_dev.dd.d_unit = pd->pd_unit;
			pd_dev.d_slice = D_SLICENONE;
			pd_dev.d_partition = D_PARTNONE;
			ret = disk_open(&pd_dev, blkio->Media->BlockSize *
			    (blkio->Media->LastBlock + 1),
			    blkio->Media->BlockSize);
			if (ret == 0) {
				ret = disk_print(&pd_dev, line, verbose);
				disk_close(&pd_dev);
				if (ret != 0)
					return (ret);
			} else {
				/* Do not fail from disk_open() */
				ret = 0;
			}
		} else {
			if ((ret = pager_output("\n")) != 0)
				break;
		}
	}
	return (ret);
}

static int
efipart_printfd(int verbose)
{
	return (efipart_print_common(&efipart_fddev, &fdinfo, verbose));
}

static int
efipart_printcd(int verbose)
{
	return (efipart_print_common(&efipart_cddev, &cdinfo, verbose));
}

static int
efipart_printhd(int verbose)
{
	return (efipart_print_common(&efipart_hddev, &hdinfo, verbose));
}

static int
efipart_open(struct open_file *f, ...)
{
	va_list args;
	struct disk_devdesc *dev;
	pdinfo_t *pd;
	EFI_BLOCK_IO *blkio;
	EFI_STATUS status;

	va_start(args, f);
	dev = va_arg(args, struct disk_devdesc *);
	va_end(args);
	if (dev == NULL)
		return (EINVAL);

	pd = efiblk_get_pdinfo((struct devdesc *)dev);
	if (pd == NULL)
		return (EIO);

	if (pd->pd_blkio == NULL) {
		status = OpenProtocolByHandle(pd->pd_handle, &blkio_guid,
		    (void **)&pd->pd_blkio);
		if (EFI_ERROR(status))
			return (efi_status_to_errno(status));
	}

	blkio = pd->pd_blkio;
	if (!blkio->Media->MediaPresent)
		return (EAGAIN);

	pd->pd_open++;
	if (pd->pd_bcache == NULL)
		pd->pd_bcache = bcache_allocate();

	if (dev->dd.d_dev->dv_type == DEVT_DISK) {
		int rc;

		rc = disk_open(dev,
		    blkio->Media->BlockSize * (blkio->Media->LastBlock + 1),
		    blkio->Media->BlockSize);
		if (rc != 0) {
			pd->pd_open--;
			if (pd->pd_open == 0) {
				pd->pd_blkio = NULL;
				bcache_free(pd->pd_bcache);
				pd->pd_bcache = NULL;
			}
		}
		return (rc);
	}
	return (0);
}

static int
efipart_close(struct open_file *f)
{
	struct disk_devdesc *dev;
	pdinfo_t *pd;

	dev = (struct disk_devdesc *)(f->f_devdata);
	if (dev == NULL)
		return (EINVAL);

	pd = efiblk_get_pdinfo((struct devdesc *)dev);
	if (pd == NULL)
		return (EINVAL);

	pd->pd_open--;
	if (pd->pd_open == 0) {
		pd->pd_blkio = NULL;
		bcache_free(pd->pd_bcache);
		pd->pd_bcache = NULL;
	}
	if (dev->dd.d_dev->dv_type == DEVT_DISK)
		return (disk_close(dev));
	return (0);
}

static int
efipart_ioctl(struct open_file *f, u_long cmd, void *data)
{
	struct disk_devdesc *dev;
	pdinfo_t *pd;
	int rc;

	dev = (struct disk_devdesc *)(f->f_devdata);
	if (dev == NULL)
		return (EINVAL);

	pd = efiblk_get_pdinfo((struct devdesc *)dev);
	if (pd == NULL)
		return (EINVAL);

	if (dev->dd.d_dev->dv_type == DEVT_DISK) {
		rc = disk_ioctl(dev, cmd, data);
		if (rc != ENOTTY)
			return (rc);
	}

	switch (cmd) {
	case DIOCGSECTORSIZE:
		*(u_int *)data = pd->pd_blkio->Media->BlockSize;
		break;
	case DIOCGMEDIASIZE:
		*(uint64_t *)data = pd->pd_blkio->Media->BlockSize *
		    (pd->pd_blkio->Media->LastBlock + 1);
		break;
	default:
		return (ENOTTY);
	}

	return (0);
}

/*
 * efipart_readwrite()
 * Internal equivalent of efipart_strategy(), which operates on the
 * media-native block size. This function expects all I/O requests
 * to be within the media size and returns an error if such is not
 * the case.
 */
static int
efipart_readwrite(EFI_BLOCK_IO *blkio, int rw, daddr_t blk, daddr_t nblks,
    char *buf)
{
	EFI_STATUS status;

	if (blkio == NULL)
		return (ENXIO);
	if (blk < 0 || blk > blkio->Media->LastBlock)
		return (EIO);
	if ((blk + nblks - 1) > blkio->Media->LastBlock)
		return (EIO);

	switch (rw & F_MASK) {
	case F_READ:
		status = blkio->ReadBlocks(blkio, blkio->Media->MediaId, blk,
		    nblks * blkio->Media->BlockSize, buf);
		break;
	case F_WRITE:
		if (blkio->Media->ReadOnly)
			return (EROFS);
		status = blkio->WriteBlocks(blkio, blkio->Media->MediaId, blk,
		    nblks * blkio->Media->BlockSize, buf);
		break;
	default:
		return (ENOSYS);
	}

	if (EFI_ERROR(status)) {
		printf("%s: rw=%d, blk=%ju size=%ju status=%lu\n", __func__, rw,
		    blk, nblks, EFI_ERROR_CODE(status));
	}
	return (efi_status_to_errno(status));
}

static int
efipart_strategy(void *devdata, int rw, daddr_t blk, size_t size,
    char *buf, size_t *rsize)
{
	struct bcache_devdata bcd;
	struct disk_devdesc *dev;
	pdinfo_t *pd;

	dev = (struct disk_devdesc *)devdata;
	if (dev == NULL)
		return (EINVAL);

	pd = efiblk_get_pdinfo((struct devdesc *)dev);
	if (pd == NULL)
		return (EINVAL);

	if (pd->pd_blkio->Media->RemovableMedia &&
	    !pd->pd_blkio->Media->MediaPresent)
		return (ENXIO);

	bcd.dv_strategy = efipart_realstrategy;
	bcd.dv_devdata = devdata;
	bcd.dv_cache = pd->pd_bcache;

	if (dev->dd.d_dev->dv_type == DEVT_DISK) {
		daddr_t offset;

		offset = dev->d_offset * pd->pd_blkio->Media->BlockSize;
		offset /= 512;
		return (bcache_strategy(&bcd, rw, blk + offset,
		    size, buf, rsize));
	}
	return (bcache_strategy(&bcd, rw, blk, size, buf, rsize));
}

static int
efipart_realstrategy(void *devdata, int rw, daddr_t blk, size_t size,
    char *buf, size_t *rsize)
{
	struct disk_devdesc *dev = (struct disk_devdesc *)devdata;
	pdinfo_t *pd;
	EFI_BLOCK_IO *blkio;
	uint64_t off, disk_blocks, d_offset = 0;
	char *blkbuf;
	size_t blkoff, blksz, bio_size;
	unsigned ioalign;
	bool need_buf;
	int rc;
	uint64_t diskend, readstart;

	if (dev == NULL || blk < 0)
		return (EINVAL);

	pd = efiblk_get_pdinfo((struct devdesc *)dev);
	if (pd == NULL)
		return (EINVAL);

	blkio = pd->pd_blkio;
	if (blkio == NULL)
		return (ENXIO);

	if (size == 0 || (size % 512) != 0)
		return (EIO);

	off = blk * 512;
	/*
	 * Get disk blocks, this value is either for whole disk or for
	 * partition.
	 */
	disk_blocks = 0;
	if (dev->dd.d_dev->dv_type == DEVT_DISK) {
		if (disk_ioctl(dev, DIOCGMEDIASIZE, &disk_blocks) == 0) {
			/* DIOCGMEDIASIZE does return bytes. */
			disk_blocks /= blkio->Media->BlockSize;
		}
		d_offset = dev->d_offset;
	}
	if (disk_blocks == 0)
		disk_blocks = blkio->Media->LastBlock + 1 - d_offset;

	/* make sure we don't read past disk end */
	if ((off + size) / blkio->Media->BlockSize > d_offset + disk_blocks) {
		diskend = d_offset + disk_blocks;
		readstart = off / blkio->Media->BlockSize;

		if (diskend <= readstart) {
			if (rsize != NULL)
				*rsize = 0;

			return (EIO);
		}
		size = diskend - readstart;
		size = size * blkio->Media->BlockSize;
	}

	need_buf = true;
	/* Do we need bounce buffer? */
	if ((size % blkio->Media->BlockSize == 0) &&
	    (off % blkio->Media->BlockSize == 0))
		need_buf = false;

	/* Do we have IO alignment requirement? */
	ioalign = blkio->Media->IoAlign;
	if (ioalign == 0)
		ioalign++;

	if (ioalign > 1 && (uintptr_t)buf != roundup2((uintptr_t)buf, ioalign))
		need_buf = true;

	if (need_buf) {
		for (bio_size = BIO_BUFFER_SIZE; bio_size > 0;
		    bio_size -= blkio->Media->BlockSize) {
			blkbuf = memalign(ioalign, bio_size);
			if (blkbuf != NULL)
				break;
		}
	} else {
		blkbuf = buf;
		bio_size = size;
	}

	if (blkbuf == NULL)
		return (ENOMEM);

	if (rsize != NULL)
		*rsize = size;

	rc = 0;
	blk = off / blkio->Media->BlockSize;
	blkoff = off % blkio->Media->BlockSize;

	while (size > 0) {
		size_t x = min(size, bio_size);

		if (x < blkio->Media->BlockSize)
			x = 1;
		else
			x /= blkio->Media->BlockSize;

		switch (rw & F_MASK) {
		case F_READ:
			blksz = blkio->Media->BlockSize * x - blkoff;
			if (size < blksz)
				blksz = size;

			rc = efipart_readwrite(blkio, rw, blk, x, blkbuf);
			if (rc != 0)
				goto error;

			if (need_buf)
				bcopy(blkbuf + blkoff, buf, blksz);
			break;
		case F_WRITE:
			rc = 0;
			if (blkoff != 0) {
				/*
				 * We got offset to sector, read 1 sector to
				 * blkbuf.
				 */
				x = 1;
				blksz = blkio->Media->BlockSize - blkoff;
				blksz = min(blksz, size);
				rc = efipart_readwrite(blkio, F_READ, blk, x,
				    blkbuf);
			} else if (size < blkio->Media->BlockSize) {
				/*
				 * The remaining block is not full
				 * sector. Read 1 sector to blkbuf.
				 */
				x = 1;
				blksz = size;
				rc = efipart_readwrite(blkio, F_READ, blk, x,
				    blkbuf);
			} else {
				/* We can write full sector(s). */
				blksz = blkio->Media->BlockSize * x;
			}

			if (rc != 0)
				goto error;
			/*
			 * Put your Data In, Put your Data out,
			 * Put your Data In, and shake it all about
			 */
			if (need_buf)
				bcopy(buf, blkbuf + blkoff, blksz);
			rc = efipart_readwrite(blkio, F_WRITE, blk, x, blkbuf);
			if (rc != 0)
				goto error;
			break;
		default:
			/* DO NOTHING */
			rc = EROFS;
			goto error;
		}

		blkoff = 0;
		buf += blksz;
		size -= blksz;
		blk += x;
	}

error:
	if (rsize != NULL)
		*rsize -= size;

	if (need_buf)
		free(blkbuf);
	return (rc);
}
