/*
 * linux/arch/i386/kernel/edd.c
 *  Copyright (C) 2002, 2003 Dell, Inc.
 *  by Matt Domsch <Matt_Domsch@dell.com>
 *  disk80 signature by Matt Domsch, Andrew Wilks, and Sandeep K. Shandilya
 *
 * BIOS Enhanced Disk Drive Services (EDD)
 * conformant to T13 Committee www.t13.org
 *   projects 1572D, 1484D, 1386D, 1226DT
 *
 * This code takes information provided by BIOS EDD calls
 * fn41 - Check Extensions Present and
 * fn48 - Get Device Parametes with EDD extensions
 * made in setup.S, copied to safe structures in setup.c,
 * and presents it in /proc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v2.0 as published by
 * the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/*
 * TODO:
 * - move edd.[ch] to better locations if/when one is decided
 */

#include <linux/module.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/stat.h>
#include <linux/ctype.h>
#include <linux/slab.h>
#include <linux/limits.h>
#include <linux/pci.h>
#include <linux/proc_fs.h>
#include <asm/edd.h>

MODULE_AUTHOR("Matt Domsch <Matt_Domsch@Dell.com>");
MODULE_DESCRIPTION("proc interface to BIOS EDD information");
MODULE_LICENSE("GPL");

#define EDD_VERSION "0.10 2003-Dec-05"
#define EDD_DEVICE_NAME_SIZE 16
#define REPORT_URL "http://domsch.com/linux/edd30/results.html"

#define left (count - (p - page) - 1)

static struct proc_dir_entry *bios_dir;

struct attr_entry {
	struct proc_dir_entry *entry;
	struct list_head node;
};

struct edd_device {
	char name[EDD_DEVICE_NAME_SIZE];
	struct edd_info *info;
	struct proc_dir_entry *dir;
	struct list_head attr_list;
};

static struct edd_device *edd_devices[EDDMAXNR];

struct edd_attribute {
	char *name;
	int (*show)(char *page, char **start, off_t off,
		    int count, int *eof, void *data);
	int (*test) (struct edd_device * edev);
};

#define EDD_DEVICE_ATTR(_name,_show,_test) \
struct edd_attribute edd_attr_##_name = { 	\
	.name = __stringify(_name), \
	.show	= _show,	    \
        .test   = _test,            \
};

static inline struct edd_info *
edd_dev_get_info(struct edd_device *edev)
{
	return edev->info;
}

static inline void
edd_dev_set_info(struct edd_device *edev, struct edd_info *info)
{
	edev->info = info;
}

static int
proc_calc_metrics(char *page, char **start, off_t off,
		  int count, int *eof, int len)
{
	if (len <= off+count) *eof = 1;
	*start = page + off;
	len -= off;
	if (len>count) len = count;
	if (len<0) len = 0;
	return len;
}

static int
edd_dump_raw_data(char *b, int count, void *data, int length)
{
	char *orig_b = b;
	char hexbuf[80], ascbuf[20], *h, *a, c;
	unsigned char *p = data;
	unsigned long column = 0;
	int length_printed = 0, d;
	const char maxcolumn = 16;
	while (length_printed < length && count > 0) {
		h = hexbuf;
		a = ascbuf;
		for (column = 0;
		     column < maxcolumn && length_printed < length; column++) {
			h += sprintf(h, "%02x ", (unsigned char) *p);
			if (!isprint(*p))
				c = '.';
			else
				c = *p;
			a += sprintf(a, "%c", c);
			p++;
			length_printed++;
		}
		/* pad out the line */
		for (; column < maxcolumn; column++) {
			h += sprintf(h, "   ");
			a += sprintf(a, " ");
		}
		d = snprintf(b, count, "%s\t%s\n", hexbuf, ascbuf);
		b += d;
		count -= d;
	}
	return (b - orig_b);
}

static int
edd_show_host_bus(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	struct edd_info *info = data;
	char *p = page;
	int i;

	if (!info || !page || off) {
		return proc_calc_metrics(page, start, off, count, eof, 0);
	}

	for (i = 0; i < 4; i++) {
		if (isprint(info->params.host_bus_type[i])) {
			p += snprintf(p, left, "%c", info->params.host_bus_type[i]);
		} else {
			p += snprintf(p, left, " ");
		}
	}

	if (!strncmp(info->params.host_bus_type, "ISA", 3)) {
		p += snprintf(p, left, "\tbase_address: %x\n",
			     info->params.interface_path.isa.base_address);
	} else if (!strncmp(info->params.host_bus_type, "PCIX", 4) ||
		   !strncmp(info->params.host_bus_type, "PCI", 3)) {
		p += snprintf(p, left,
			     "\t%02x:%02x.%d  channel: %u\n",
			     info->params.interface_path.pci.bus,
			     info->params.interface_path.pci.slot,
			     info->params.interface_path.pci.function,
			     info->params.interface_path.pci.channel);
	} else if (!strncmp(info->params.host_bus_type, "IBND", 4) ||
		   !strncmp(info->params.host_bus_type, "XPRS", 4) ||
		   !strncmp(info->params.host_bus_type, "HTPT", 4)) {
		p += snprintf(p, left,
			     "\tTBD: %llx\n",
			     info->params.interface_path.ibnd.reserved);

	} else {
		p += snprintf(p, left, "\tunknown: %llx\n",
			     info->params.interface_path.unknown.reserved);
	}
	return proc_calc_metrics(page, start, off, count, eof, (p - page));
}

static int
edd_show_interface(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	struct edd_info *info = data;
	char *p = page;
	int i;

	if (!info || !page || off) {
		return proc_calc_metrics(page, start, off, count, eof, 0);
	}

	for (i = 0; i < 8; i++) {
		if (isprint(info->params.interface_type[i])) {
			p += snprintf(p, left, "%c", info->params.interface_type[i]);
		} else {
			p += snprintf(p, left, " ");
		}
	}
	if (!strncmp(info->params.interface_type, "ATAPI", 5)) {
		p += snprintf(p, left, "\tdevice: %u  lun: %u\n",
			     info->params.device_path.atapi.device,
			     info->params.device_path.atapi.lun);
	} else if (!strncmp(info->params.interface_type, "ATA", 3)) {
		p += snprintf(p, left, "\tdevice: %u\n",
			     info->params.device_path.ata.device);
	} else if (!strncmp(info->params.interface_type, "SCSI", 4)) {
		p += snprintf(p, left, "\tid: %u  lun: %llu\n",
			     info->params.device_path.scsi.id,
			     info->params.device_path.scsi.lun);
	} else if (!strncmp(info->params.interface_type, "USB", 3)) {
		p += snprintf(p, left, "\tserial_number: %llx\n",
			     info->params.device_path.usb.serial_number);
	} else if (!strncmp(info->params.interface_type, "1394", 4)) {
		p += snprintf(p, left, "\teui: %llx\n",
			     info->params.device_path.i1394.eui);
	} else if (!strncmp(info->params.interface_type, "FIBRE", 5)) {
		p += snprintf(p, left, "\twwid: %llx lun: %llx\n",
			     info->params.device_path.fibre.wwid,
			     info->params.device_path.fibre.lun);
	} else if (!strncmp(info->params.interface_type, "I2O", 3)) {
		p += snprintf(p, left, "\tidentity_tag: %llx\n",
			     info->params.device_path.i2o.identity_tag);
	} else if (!strncmp(info->params.interface_type, "RAID", 4)) {
		p += snprintf(p, left, "\tidentity_tag: %x\n",
			     info->params.device_path.raid.array_number);
	} else if (!strncmp(info->params.interface_type, "SATA", 4)) {
		p += snprintf(p, left, "\tdevice: %u\n",
			     info->params.device_path.sata.device);
	} else {
		p += snprintf(p, left, "\tunknown: %llx %llx\n",
			     info->params.device_path.unknown.reserved1,
			     info->params.device_path.unknown.reserved2);
	}

	return proc_calc_metrics(page, start, off, count, eof, (p - page));
}

/**
 * edd_show_raw_data() - unparses EDD information, returned to user-space
 *
 * Returns: number of bytes written, or 0 on failure
 */
static int
edd_show_raw_data(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	struct edd_info *info = data;
	char *p = page;
	int i, warn_padding = 0, nonzero_path = 0,
		len = sizeof (*info) - 4;
	uint8_t checksum = 0, c = 0;
	if (!info || !page || off) {
		return proc_calc_metrics(page, start, off, count, eof, 0);
	}

	if (!(info->params.key == 0xBEDD || info->params.key == 0xDDBE))
		len = info->params.length;

	p += snprintf(p, left, "int13 fn48 returned data:\n\n");
	p += edd_dump_raw_data(p, left, ((char *) info) + 4, len);

	/* Spec violation.  Adaptec AIC7899 returns 0xDDBE
	   here, when it should be 0xBEDD.
	 */
	p += snprintf(p, left, "\n");
	if (info->params.key == 0xDDBE) {
		p += snprintf(p, left,
			     "Warning: Spec violation.  Key should be 0xBEDD, is 0xDDBE\n");
	}

	if (!(info->params.key == 0xBEDD || info->params.key == 0xDDBE)) {
		goto out;
	}

	for (i = 30; i <= 73; i++) {
		c = *(((uint8_t *) info) + i + 4);
		if (c)
			nonzero_path++;
		checksum += c;
	}

	if (checksum) {
		p += snprintf(p, left,
			     "Warning: Spec violation.  Device Path checksum invalid.\n");
	}

	if (!nonzero_path) {
		p += snprintf(p, left, "Error: Spec violation.  Empty device path.\n");
		goto out;
	}

	for (i = 0; i < 4; i++) {
		if (!isprint(info->params.host_bus_type[i])) {
			warn_padding++;
		}
	}
	for (i = 0; i < 8; i++) {
		if (!isprint(info->params.interface_type[i])) {
			warn_padding++;
		}
	}

	if (warn_padding) {
		p += snprintf(p, left,
			     "Warning: Spec violation.  Padding should be 0x20.\n");
	}

out:
	p += snprintf(p, left, "\nPlease check %s\n", REPORT_URL);
	p += snprintf(p, left, "to see if this device has been reported.  If not,\n");
	p += snprintf(p, left, "please send the information requested there.\n");

	return proc_calc_metrics(page, start, off, count, eof, (p - page));
}

static int
edd_show_version(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	struct edd_info *info = data;
	char *p = page;
	if (!info || !page || off) {
		return proc_calc_metrics(page, start, off, count, eof, 0);
	}

	p += snprintf(p, left, "0x%02x\n", info->version);
	return proc_calc_metrics(page, start, off, count, eof, (p - page));
}

static int
edd_show_disk80_sig(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	char *p = page;
	if ( !page || off) {
		return proc_calc_metrics(page, start, off, count, eof, 0);
	}

	p += snprintf(p, left, "0x%08x\n", edd_disk80_sig);
	return proc_calc_metrics(page, start, off, count, eof, (p - page));
}

static int
edd_show_extensions(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	struct edd_info *info = data;
	char *p = page;
	if (!info || !page || off) {
		return proc_calc_metrics(page, start, off, count, eof, 0);
	}

	if (info->interface_support & EDD_EXT_FIXED_DISK_ACCESS) {
		p += snprintf(p, left, "Fixed disk access\n");
	}
	if (info->interface_support & EDD_EXT_DEVICE_LOCKING_AND_EJECTING) {
		p += snprintf(p, left, "Device locking and ejecting\n");
	}
	if (info->interface_support & EDD_EXT_ENHANCED_DISK_DRIVE_SUPPORT) {
		p += snprintf(p, left, "Enhanced Disk Drive support\n");
	}
	if (info->interface_support & EDD_EXT_64BIT_EXTENSIONS) {
		p += snprintf(p, left, "64-bit extensions\n");
	}
	return proc_calc_metrics(page, start, off, count, eof, (p - page));
}

static int
edd_show_info_flags(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	struct edd_info *info = data;
	char *p = page;
	if (!info || !page || off) {
		return proc_calc_metrics(page, start, off, count, eof, 0);
	}

	if (info->params.info_flags & EDD_INFO_DMA_BOUNDRY_ERROR_TRANSPARENT)
		p += snprintf(p, left, "DMA boundry error transparent\n");
	if (info->params.info_flags & EDD_INFO_GEOMETRY_VALID)
		p += snprintf(p, left, "geometry valid\n");
	if (info->params.info_flags & EDD_INFO_REMOVABLE)
		p += snprintf(p, left, "removable\n");
	if (info->params.info_flags & EDD_INFO_WRITE_VERIFY)
		p += snprintf(p, left, "write verify\n");
	if (info->params.info_flags & EDD_INFO_MEDIA_CHANGE_NOTIFICATION)
		p += snprintf(p, left, "media change notification\n");
	if (info->params.info_flags & EDD_INFO_LOCKABLE)
		p += snprintf(p, left, "lockable\n");
	if (info->params.info_flags & EDD_INFO_NO_MEDIA_PRESENT)
		p += snprintf(p, left, "no media present\n");
	if (info->params.info_flags & EDD_INFO_USE_INT13_FN50)
		p += snprintf(p, left, "use int13 fn50\n");
	return proc_calc_metrics(page, start, off, count, eof, (p - page));
}

static int
edd_show_default_cylinders(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	struct edd_info *info = data;
	char *p = page;
	if (!info || !page || off) {
		return proc_calc_metrics(page, start, off, count, eof, 0);
	}

	p += snprintf(p, left, "0x%x\n", info->params.num_default_cylinders);
	return proc_calc_metrics(page, start, off, count, eof, (p - page));
}

static int
edd_show_default_heads(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	struct edd_info *info = data;
	char *p = page;
	if (!info || !page || off) {
		return proc_calc_metrics(page, start, off, count, eof, 0);
	}

	p += snprintf(p, left, "0x%x\n", info->params.num_default_heads);
	return proc_calc_metrics(page, start, off, count, eof, (p - page));
}

static int
edd_show_default_sectors_per_track(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	struct edd_info *info = data;
	char *p = page;
	if (!info || !page || off) {
		return proc_calc_metrics(page, start, off, count, eof, 0);
	}

	p += snprintf(p, left, "0x%x\n", info->params.sectors_per_track);
	return proc_calc_metrics(page, start, off, count, eof, (p - page));
}

static int
edd_show_sectors(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	struct edd_info *info = data;
	char *p = page;
	if (!info || !page || off) {
		return proc_calc_metrics(page, start, off, count, eof, 0);
	}

	p += snprintf(p, left, "0x%llx\n", info->params.number_of_sectors);
	return proc_calc_metrics(page, start, off, count, eof, (p - page));
}

static int
edd_has_default_cylinders(struct edd_device *edev)
{
	struct edd_info *info = edd_dev_get_info(edev);
	if (!edev || !info)
		return 0;
	return info->params.num_default_cylinders > 0;
}

static int
edd_has_default_heads(struct edd_device *edev)
{
	struct edd_info *info = edd_dev_get_info(edev);
	if (!edev || !info)
		return 0;
	return info->params.num_default_heads > 0;
}

static int
edd_has_default_sectors_per_track(struct edd_device *edev)
{
	struct edd_info *info = edd_dev_get_info(edev);
	if (!edev || !info)
		return 0;
	return info->params.sectors_per_track > 0;
}

static int
edd_has_edd30(struct edd_device *edev)
{
	struct edd_info *info = edd_dev_get_info(edev);
	int i, nonzero_path = 0;
	char c;

	if (!edev || !info)
		return 0;

	if (!(info->params.key == 0xBEDD || info->params.key == 0xDDBE)) {
		return 0;
	}

	for (i = 30; i <= 73; i++) {
		c = *(((uint8_t *) info) + i + 4);
		if (c) {
			nonzero_path++;
			break;
		}
	}
	if (!nonzero_path) {
		return 0;
	}

	return 1;
}

static int
edd_has_disk80_sig(struct edd_device *edev)
{
	struct edd_info *info = edd_dev_get_info(edev);
	if (!edev || !info)
		return 0;
	return info->device == 0x80;
}

static EDD_DEVICE_ATTR(raw_data, edd_show_raw_data, NULL);
static EDD_DEVICE_ATTR(version, edd_show_version, NULL);
static EDD_DEVICE_ATTR(extensions, edd_show_extensions, NULL);
static EDD_DEVICE_ATTR(info_flags, edd_show_info_flags, NULL);
static EDD_DEVICE_ATTR(sectors, edd_show_sectors, NULL);
static EDD_DEVICE_ATTR(default_cylinders, edd_show_default_cylinders,
		       edd_has_default_cylinders);
static EDD_DEVICE_ATTR(default_heads, edd_show_default_heads,
		       edd_has_default_heads);
static EDD_DEVICE_ATTR(default_sectors_per_track,
		       edd_show_default_sectors_per_track,
		       edd_has_default_sectors_per_track);
static EDD_DEVICE_ATTR(interface, edd_show_interface,edd_has_edd30);
static EDD_DEVICE_ATTR(host_bus, edd_show_host_bus, edd_has_edd30);
static EDD_DEVICE_ATTR(mbr_signature, edd_show_disk80_sig, edd_has_disk80_sig);

static struct edd_attribute *def_attrs[] = {
	&edd_attr_raw_data,
	&edd_attr_version,
	&edd_attr_extensions,
	&edd_attr_info_flags,
	&edd_attr_sectors,
	&edd_attr_default_cylinders,
	&edd_attr_default_heads,
	&edd_attr_default_sectors_per_track,
	&edd_attr_interface,
	&edd_attr_host_bus,
	&edd_attr_mbr_signature,
	NULL,
};

static inline void
edd_device_unregister(struct edd_device *edev)
{
	struct list_head *pos, *next;
	struct attr_entry *ae;

	list_for_each_safe(pos, next, &edev->attr_list) {
		ae = list_entry(pos, struct attr_entry, node);
		remove_proc_entry(ae->entry->name, edev->dir);
		list_del(&ae->node);
		kfree(ae);
	}

	remove_proc_entry(edev->dir->name, bios_dir);
}

static int
edd_populate_dir(struct edd_device *edev)
{
	struct edd_attribute *attr;
	struct attr_entry *ae;
	int i;
	int error = 0;

	for (i = 0; (attr=def_attrs[i]); i++) {
		if (!attr->test || (attr->test && attr->test(edev))) {
			ae = kmalloc(sizeof (*ae), GFP_KERNEL);
			if (ae == NULL) {
				error = 1;
				break;
			}
			INIT_LIST_HEAD(&ae->node);
			ae->entry =
				create_proc_read_entry(attr->name, 0444,
						       edev->dir, attr->show,
						       edd_dev_get_info(edev));
			if (ae->entry == NULL) {
				error = 1;
				break;
			}
			list_add(&ae->node, &edev->attr_list);
		}
	}

	if (error)
		return error;

	return 0;
}

static int
edd_make_dir(struct edd_device *edev)
{
	int error=1;

	edev->dir = proc_mkdir(edev->name, bios_dir);
	if (edev->dir != NULL) {
		edev->dir->mode = (S_IFDIR | S_IRWXU | S_IRUGO | S_IXUGO);
		error = edd_populate_dir(edev);
	}
	return error;
}

static int
edd_device_register(struct edd_device *edev, int i)
{
	int error;

	if (!edev)
		return 1;
	memset(edev, 0, sizeof (*edev));
	INIT_LIST_HEAD(&edev->attr_list);
	edd_dev_set_info(edev, &edd[i]);
	snprintf(edev->name, EDD_DEVICE_NAME_SIZE, "int13_dev%02x",
		 edd[i].device);
	error = edd_make_dir(edev);
	return error;
}

/**
 * edd_init() - creates /proc/bios tree of EDD data
 *
 * This assumes that eddnr and edd were
 * assigned in setup.c already.
 */
static int __init
edd_init(void)
{
	unsigned int i;
	int rc = 0;
	struct edd_device *edev;

	printk(KERN_INFO "BIOS EDD facility v%s, %d devices found\n",
	       EDD_VERSION, eddnr);

	if (!eddnr) {
		printk(KERN_INFO "EDD information not available.\n");
		return 1;
	}

	bios_dir = proc_mkdir("bios", NULL);
	if (bios_dir == NULL)
		return 1;

	for (i = 0; i < eddnr && i < EDDMAXNR && !rc; i++) {
		edev = kmalloc(sizeof (*edev), GFP_KERNEL);
		if (!edev) {
			rc = 1;
			break;
		}

		rc = edd_device_register(edev, i);
		if (rc) {
			break;
		}
		edd_devices[i] = edev;
	}

	if (rc) {
		for (i = 0; i < eddnr && i < EDDMAXNR; i++) {
			if ((edev = edd_devices[i])) {
				edd_device_unregister(edev);
				kfree(edev);
			}
		}

		remove_proc_entry(bios_dir->name, NULL);
	}

	return rc;
}

static void __exit
edd_exit(void)
{
	int i;
	struct edd_device *edev;

	for (i = 0; i < eddnr && i < EDDMAXNR; i++) {
		if ((edev = edd_devices[i])) {
			edd_device_unregister(edev);
			kfree(edev);
		}
	}

	remove_proc_entry(bios_dir->name, NULL);
}

module_init(edd_init);
module_exit(edd_exit);
