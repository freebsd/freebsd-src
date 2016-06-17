/*
 * MEDLEY SOFTWARE RAID DRIVER (Silicon Image 3112 and others)
 *
 * Copyright (C) 2003 Thomas Horsten <thomas@horsten.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307	 USA
 * Copyright (C) 2003 Thomas Horsten <thomas@horsten.com>
 * All Rights Reserved.
 *
 * This driver uses the ATA RAID driver framework and is based on
 * code from Arjan van de Ven's silraid.c and hptraid.c.
 *
 * It is a driver for the Medley software RAID, which is used by
 * some IDE controllers, including the Silicon Image 3112 SATA
 * controller found onboard many modern motherboards, and the
 * CMD680 stand-alone PCI RAID controller.
 *
 * The author has only tested this on the Silicon Image SiI3112.
 * If you have any luck using more than 2 drives, and/or more
 * than one RAID set, and/or any other chip than the SiI3112,
 * please let me know by sending me mail at the above address.
 *
 * Currently, only striped mode is supported for these RAIDs.
 *
 * You are welcome to contact me if you have any questions or
 * suggestions for improvement.
 *
 * Change history:
 *
 * 20040310 - thomas@horsten.com
 *   Removed C99 style variable declarations that confused gcc-2.x
 *   Fixed a bug where more than one RAID set would not be detected correctly
 *   General cleanup for submission to kernel
 *
 * 20031012 - thomas@horsten.com
 *   Added support for BLKRRPART ioctl to re-read partition table
 *
 * 20030801 - thomas@horsten.com
 *   First test release
 *
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/smp_lock.h>
#include <linux/blkdev.h>
#include <linux/blkpg.h>
#include <linux/genhd.h>
#include <linux/ioctl.h>

#include <linux/ide.h>
#include <asm/uaccess.h>

#include "ataraid.h"

/*
 * These options can be tuned if the need should occur.
 *
 * Even better, this driver could be changed to allocate the structures
 * dynamically.
 */
#define MAX_DRIVES_PER_SET 8
#define MAX_MEDLEY_ARRAYS 4

/*
 * Set to 1 only if you are debugging the driver, or if it doesn't work
 * the way you expect and you want to to report it.
 *
 * This will produce lots of kernel messages, some of which might
 * help me figure out what is going wrong).
 */
#define DEBUGGING_MEDLEY 0

#if DEBUGGING_MEDLEY
#define dprintk(fmt, args...) printk(fmt, ##args)
#else
#define dprintk(fmt, args...)
#endif

/*
 * Medley RAID metadata structure.
 *
 * The metadata structure is based on the ATA drive ID from the drive itself,
 * with the RAID information in the vendor specific regions.
 *
 * We do not use all the fields, since we only support Striped Sets.
 */
struct medley_metadata {
	u8  driveid0[46];
	u8  ascii_version[8];
	u8  driveid1[52];
	u32 total_sectors_low;
	u32 total_sectors_high;
	u16 reserved0;
	u8  driveid2[142];
	u16 product_id;
	u16 vendor_id;
	u16 minor_ver;
	u16 major_ver;
	u16 creation_timestamp[3];
	u16 chunk_size;
	u16 reserved1;
	u8  drive_number;
	u8  raid_type;
	u8  drives_per_striped_set;
	u8  striped_set_number;
	u8  drives_per_mirrored_set;
	u8  mirrored_set_number;
	u32 rebuild_ptr_low;
	u32 rebuild_ptr_high;
	u32 incarnation_no;
	u8  member_status;
	u8  mirrored_set_state;
	u8  reported_device_location;
	u8  member_location;
	u8  auto_rebuild;
	u8  reserved3[17];
	u16 checksum;
};

/*
 * This struct holds the information about a Medley array
 */
struct medley_array {
	u8       drives;
	u16      chunk_size;
	u32      sectors_per_row;
	u8       chunk_size_log;
	u16      present;
	u16      timestamp[3];
	u32      sectors;
	int      registered;
	atomic_t valid;
	int      access;

	kdev_t   members[MAX_DRIVES_PER_SET];
	struct   block_device *bdev[MAX_DRIVES_PER_SET];
};

static struct medley_array raid[MAX_MEDLEY_ARRAYS];

/*
 * Here we keep the offset of the ATARAID device ID's compared to our
 * own (this will normally be 0, unless another ATARAID driver has
 * registered some arrays before us).
 */
static int medley_devid_offset = 0;

/*
 * This holds the number of detected arrays.
 */
static int medley_arrays = 0;

/*
 * Wait queue for opening device (used when re-reading partition table)
 */
static DECLARE_WAIT_QUEUE_HEAD(medley_wait_open);

/*
 * The interface functions used by the ataraid framework.
 */
static int medley_open(struct inode *inode, struct file *filp);
static int medley_release(struct inode *inode, struct file *filp);
static int medley_ioctl(struct inode *inode, struct file *file,
			unsigned int cmd, unsigned long arg);
static int medley_make_request(request_queue_t * q, int rw,
			       struct buffer_head *bh);

static struct raid_device_operations medley_ops = {
	open:         medley_open,
	release:      medley_release,
	ioctl:        medley_ioctl,
	make_request: medley_make_request
};

/*
 * This is the list of devices to probe.
 */
static const kdev_t probelist[] = {
	MKDEV(IDE0_MAJOR, 0),
	MKDEV(IDE0_MAJOR, 64),
	MKDEV(IDE1_MAJOR, 0),
	MKDEV(IDE1_MAJOR, 64),
	MKDEV(IDE2_MAJOR, 0),
	MKDEV(IDE2_MAJOR, 64),
	MKDEV(IDE3_MAJOR, 0),
	MKDEV(IDE3_MAJOR, 64),
	MKDEV(IDE4_MAJOR, 0),
	MKDEV(IDE4_MAJOR, 64),
	MKDEV(IDE5_MAJOR, 0),
	MKDEV(IDE5_MAJOR, 64),
	MKDEV(IDE6_MAJOR, 0),
	MKDEV(IDE6_MAJOR, 64),
	MKDEV(0, 0)
};

/*
 * Handler for ioctl calls to the virtual device
 */
static int medley_ioctl(struct inode *inode, struct file *file,
			unsigned int cmd, unsigned long arg)
{
	unsigned int minor;
	unsigned long sectors;
	int devminor = (inode->i_rdev >> SHIFT) & MAJOR_MASK;
	int device = devminor - medley_devid_offset;
	int partition;

	dprintk("medley_ioctl\n");

	minor = MINOR(inode->i_rdev) >> SHIFT;

	switch (cmd) {

	case BLKGETSIZE:	/* Return device size */
		if (!arg)
			return -EINVAL;
		sectors =
		    ataraid_gendisk.part[MINOR(inode->i_rdev)].nr_sects;
		dprintk("medley_ioctl: BLKGETSIZE\n");
		if (MINOR(inode->i_rdev) & 15)
			return put_user(sectors, (unsigned long *) arg);
		return put_user(raid[minor - medley_devid_offset].sectors,
				(unsigned long *) arg);
		break;

	case HDIO_GETGEO: {
			struct hd_geometry *loc =
			    (struct hd_geometry *) arg;
			unsigned short bios_cyl = (unsigned short)
			    (raid[minor].sectors / 255 / 63);	/* truncate */

			dprintk("medley_ioctl: HDIO_GETGEO\n");
			if (!loc)
				return -EINVAL;
			if (put_user(255, (byte *) & loc->heads))
				return -EFAULT;
			if (put_user(63, (byte *) & loc->sectors))
				return -EFAULT;
			if (put_user
			    (bios_cyl, (unsigned short *) &loc->cylinders))
				return -EFAULT;
			if (put_user
			    ((unsigned) ataraid_gendisk.
			     part[MINOR(inode->i_rdev)].start_sect,
			     (unsigned long *) &loc->start))
				return -EFAULT;
			return 0;
		}

	case HDIO_GETGEO_BIG: {
			struct hd_big_geometry *loc =
			    (struct hd_big_geometry *) arg;

			dprintk("medley_ioctl: HDIO_GETGEO_BIG\n");
			if (!loc)
				return -EINVAL;
			if (put_user(255, (byte *) & loc->heads))
				return -EFAULT;
			if (put_user(63, (byte *) & loc->sectors))
				return -EFAULT;
			if (put_user
			    (raid[minor - medley_devid_offset].sectors /
			     255 / 63, (unsigned int *) &loc->cylinders))
				return -EFAULT;
			if (put_user
			    ((unsigned) ataraid_gendisk.
			     part[MINOR(inode->i_rdev)].start_sect,
			     (unsigned long *) &loc->start))
				return -EFAULT;
			return 0;
		}

	case BLKROSET:
	case BLKROGET:
	case BLKSSZGET:
		dprintk("medley_ioctl: BLK*\n");
		return blk_ioctl(inode->i_rdev, cmd, arg);

	case BLKRRPART:	/* Re-read partition tables */
		if (!capable(CAP_SYS_ADMIN))
			return -EACCES;
		if (minor != 0)
			return -EINVAL;
		if (atomic_read(&(raid[device].valid)) == 0)
			return -EINVAL;

		atomic_set(&(raid[device].valid), 0);
		if (raid[device].access != 1) {
			atomic_set(&(raid[device].valid), 1);
			return -EBUSY;
		}

		for (partition = 15; partition >= 0; partition--) {
			invalidate_device(MKDEV(ATARAID_MAJOR,
						partition + devminor), 1);
			ataraid_gendisk.part[partition +
					     devminor].start_sect = 0;
			ataraid_gendisk.part[partition +
					     devminor].nr_sects = 0;
		}
		ataraid_register_disk(device, raid[device].sectors);
		atomic_set(&(raid[device].valid), 1);
		wake_up(&medley_wait_open);
		return 0;

	default:
		return -EINVAL;
	}

	return 0;
}

/*
 * Handler to map a request to the real device.
 * If the request cannot be made because it spans multiple disks,
 * we return -1, otherwise we modify the request and return 1.
 */
static int medley_make_request(request_queue_t * q, int rw,
			       struct buffer_head *bh)
{
	u8 disk;
	u32 rsect = bh->b_rsector;
	int device =
	    ((bh->b_rdev >> SHIFT) & MAJOR_MASK) - medley_devid_offset;
	struct medley_array *r = raid + device;

	/* Add the partition offset */
	rsect = rsect + ataraid_gendisk.part[MINOR(bh->b_rdev)].start_sect;

	dprintk("medley_make_request, rsect=%ul\n", rsect);

	/* Detect if the request crosses a chunk barrier */
	if (r->chunk_size_log) {
		if (((rsect & (r->chunk_size - 1)) +
		     (bh->b_size / 512)) > (1 << r->chunk_size_log)) {
			return -1;
		}
	} else {
		if ((rsect / r->chunk_size) !=
		    ((rsect + (bh->b_size / 512) - 1) / r->chunk_size)) {
			return -1;
		}
	}

	/*
	 * Medley arrays are simple enough, since the smallest disk decides the
	 * number of sectors used per disk. So there is no need for the cutoff
	 * magic found in other drivers like hptraid.
	 */
	if (r->chunk_size_log) {
		/* We save some expensive operations (1 div, 1 mul, 1 mod),
		 * if the chunk size is a power of 2, which is true in most
		 * cases (at least with my version of the RAID BIOS).
		 */
		disk = (rsect >> r->chunk_size_log) % r->drives;
		rsect = ((rsect / r->sectors_per_row) <<
			 r->chunk_size_log) + (rsect & (r->chunk_size -
							1));
	} else {
		disk = (rsect / r->chunk_size) % r->drives;
		rsect = rsect / r->sectors_per_row * r->chunk_size +
		    rsect % r->chunk_size;
	}

	dprintk("medley_make_request :-), disk=%d, rsect=%ul\n", disk,
		rsect);
	bh->b_rdev = r->members[disk];
	bh->b_rsector = rsect;
	return 1;
}

/*
 * Find out which array a drive belongs to, and add it to that array.
 * If it is not a member of a detected array, add a new array for it.
 */
void medley_add_raiddrive(kdev_t dev, struct medley_metadata *md)
{
	int c;

	dprintk("Candidate drive %02x:%02x - drive %d of %d, stride %d, "
		"sectors %ul (%d MB)\n",
		MAJOR(dev), MINOR(dev), md->drive_number,
		md->drives_per_striped_set, md->chunk_size,
		md->total_sectors_low,
		md->total_sectors_low / 1024 / 1024 / 2);

	for (c = 0; c < medley_arrays; c++) {
		if ((raid[c].timestamp[0] == md->creation_timestamp[0]) &&
		    (raid[c].timestamp[1] == md->creation_timestamp[1]) &&
		    (raid[c].timestamp[2] == md->creation_timestamp[2]) &&
		    (raid[c].drives == md->drives_per_striped_set) &&
		    (raid[c].chunk_size == md->chunk_size) &&
		    ((raid[c].present & (1 << md->drive_number)) == 0)) {
			dprintk("Existing array %d\n", c);
			raid[c].present |= (1 << md->drive_number);
			raid[c].members[md->drive_number] = dev;
			break;
		}
	}
	if (c == medley_arrays) {
		dprintk("New array %d\n", medley_arrays);
		if (medley_arrays == MAX_MEDLEY_ARRAYS) {
			printk(KERN_ERR "Medley RAID: "
			       "Too many RAID sets detected - you can change "
			       "the max in the driver.\n");
		} else {
			raid[c].timestamp[0] = md->creation_timestamp[0];
			raid[c].timestamp[1] = md->creation_timestamp[1];
			raid[c].timestamp[2] = md->creation_timestamp[2];
			raid[c].drives = md->drives_per_striped_set;
			raid[c].chunk_size = md->chunk_size;
			raid[c].sectors_per_row = md->chunk_size *
			    md->drives_per_striped_set;

			/* Speedup if chunk size is a power of 2 */
			if (((raid[c].chunk_size - 1) &
			     (raid[c].chunk_size)) == 0) {
				raid[c].chunk_size_log =
				    ffs(raid[c].chunk_size) - 1;
			} else {
				raid[c].chunk_size_log = 0;
			}
			raid[c].present = (1 << md->drive_number);
			raid[c].members[md->drive_number] = dev;
			if (md->major_ver == 1) {
				raid[c].sectors = ((u32 *) (md))[27];
			} else {
				raid[c].sectors = md->total_sectors_low;
			}
			medley_arrays++;
		}
	}
}

/*
 * Read the Medley metadata from a drive.
 * Returns the bh if it was found, otherwise NULL.
 */
struct buffer_head *medley_get_metadata(kdev_t dev)
{
	struct buffer_head *bh = NULL;
	struct pci_dev *pcidev;
	u32 lba;
	int pos;
	struct medley_metadata *md;

	ide_drive_t *drvinfo = ide_info_ptr(dev, 0);
	if ((drvinfo == NULL) || drvinfo->capacity < 1) {
		return NULL;
	}

	dprintk("Probing %02x:%02x\n", MAJOR(dev), MINOR(dev));

	/* If this drive is not on a PCI controller, it is not Medley RAID.
	 * Medley matches the PCI device ID with the metadata to check if
	 * it is valid. Unfortunately it is the only reliable way to identify
	 * the superblock */
	pcidev = drvinfo->hwif ? drvinfo->hwif->pci_dev : NULL;
	if (!pcidev) {
		return NULL;
	}

	/*
	 * 4 copies of the metadata exist, in the following 4 sectors:
	 * last, last-0x200, last-0x400, last-0x600.
	 *
	 * We must try each of these in order, until we find the metadata.
	 * FIXME: This does not take into account drives with 48/64-bit LBA
	 * addressing, even though the Medley RAID version 2 supports these.
	 */
	lba = drvinfo->capacity - 1;
	for (pos = 0; pos < 4; pos++, lba -= 0x200) {
		bh = bread(dev, lba, 512);
		if (!bh) {
			printk(KERN_ERR "Medley RAID (%02x:%02x): "
			       "Error reading metadata (lba=%d)\n",
			       MAJOR(dev), MINOR(dev), lba);
			break;
		}

		/* A valid Medley RAID has the PCI vendor/device ID of its
		 * IDE controller, and the correct checksum. */
		md = (void *) (bh->b_data);

		if (pcidev->vendor == md->vendor_id &&
		    pcidev->device == md->product_id) {
			u16 checksum = 0;
			u16 *p = (void *) (bh->b_data);
			int c;
			for (c = 0; c < 160; c++) {
				checksum += *p++;
			}
			dprintk
			    ("Probing %02x:%02x csum=%d, major_ver=%d\n",
			     MAJOR(dev), MINOR(dev), checksum,
			     md->major_ver);
			if (((checksum == 0xffff) && (md->major_ver == 1))
			    || (checksum == 0)) {
				dprintk("Probing %02x:%02x VALID\n",
					MAJOR(dev), MINOR(dev));
				break;
			}
		}
		/* Was not a valid superblock */
		if (bh) {
			brelse(bh);
			bh = NULL;
		}
	}
	return bh;
}

/*
 * Determine if this drive belongs to a Medley array.
 */
static void medley_probe_drive(int major, int minor)
{
	struct buffer_head *bh;
	kdev_t dev = MKDEV(major, minor);
	struct medley_metadata *md;

	bh = medley_get_metadata(dev);
	if (!bh)
		return;

	md = (void *) (bh->b_data);

	if (md->raid_type != 0x0) {
		printk(KERN_INFO "Medley RAID (%02x:%02x): "
		       "Sorry, this driver currently only supports "
		       "striped sets (RAID level 0).\n", major, minor);
	} else if (md->major_ver == 2 && md->total_sectors_high != 0) {
		printk(KERN_ERR "Medley RAID (%02x:%02x):"
		       "Sorry, the driver only supports 32 bit LBA disks "
		       "(disk too big).\n", major, minor);
	} else if (md->major_ver > 0 && md->major_ver > 2) {
		printk(KERN_INFO "Medley RAID (%02x:%02x): "
		       "Unsupported version (%d.%d) - this driver supports "
		       "Medley version 1.x and 2.x\n",
		       major, minor, md->major_ver, md->minor_ver);
	} else if (md->drives_per_striped_set > MAX_DRIVES_PER_SET) {
		printk(KERN_ERR "Medley RAID (%02x:%02x): "
		       "Striped set too large (%d drives) - please report "
		       "this (and change max in driver).\n",
		       major, minor, md->drives_per_striped_set);
	} else if ((md->drive_number > md->drives_per_striped_set) ||
		   (md->drives_per_striped_set < 1) ||
		   (md->chunk_size < 1)) {
		printk(KERN_ERR "Medley RAID (%02x:%02x): "
		       "Metadata appears to be corrupt.\n", major, minor);
	} else {
		/* We have a good candidate, put it in the correct array */
		medley_add_raiddrive(dev, md);
	}

	if (bh) {
		brelse(bh);
	}
}


/*
 * Taken from hptraid.c, this is called to prevent the device
 * from disappearing from under us and also nullifies the (incorrect)
 * partitions of the underlying disk.
 */
struct block_device *get_device_lock(kdev_t member)
{
	struct block_device *bdev = bdget(member);
	struct gendisk *gd;
	int minor = MINOR(member);
	int j;

	if (bdev
	    && blkdev_get(bdev, FMODE_READ | FMODE_WRITE, 0,
			  BDEV_RAW) == 0) {
		/*
		 * This is supposed to prevent others from
		 * stealing our underlying disks. Now blank
		 * the /proc/partitions table for the wrong
		 * partition table, so that scripts don't
		 * accidentally mount it and crash the kernel
		 */
		/* XXX: the 0 is an utter hack  --hch */
		gd = get_gendisk(MKDEV(MAJOR(member), 0));
		if (gd != NULL) {
			if (gd->major == MAJOR(member)) {
				for (j = 1 + (minor << gd->minor_shift);
				     j < ((minor + 1) << gd->minor_shift);
				     j++)
					gd->part[j].nr_sects = 0;
			}
		}
	}
	return bdev;
}

/*
 * Initialise the driver.
 */
static __init int medley_init(void)
{
	int c, d;

	memset(raid, 0, MAX_MEDLEY_ARRAYS * sizeof(struct medley_array));

	/* Probe each of the drives on our list */
	for (c = 0; probelist[c] != MKDEV(0, 0); c++) {
		medley_probe_drive(MAJOR(probelist[c]),
				   MINOR(probelist[c]));
	}

	/* Check if the detected sets are complete */
	for (c = 0; c < medley_arrays; c++) {
		if (raid[c].present != (1 << raid[c].drives) - 1) {
			printk(KERN_ERR "Medley RAID: "
			       "Incomplete RAID set deleted - disks:");
			for (d = 0; c < raid[c].drives; c++) {
				if (raid[c].present & (1 << d)) {
					printk(" %02x:%02x",
					       MAJOR(raid[c].members[d]),
					       MINOR(raid[c].members[d]));
				}
			}
			printk("\n");
			if (c + 1 < medley_arrays) {
				memmove(raid + c + 1, raid + c,
					(medley_arrays - c -
					 1) * sizeof(struct medley_array));
			}
			medley_arrays--;
		}
	}

	/* Register any remaining array(s) */
	for (c = 0; c < medley_arrays; c++) {
		int device = ataraid_get_device(&medley_ops);
		if (device < 0) {
			printk(KERN_ERR "Medley RAID: "
			       "Could not get ATARAID device.\n");
			break;
		}
		if (c == 0) {
			/* First array, compute offset to our device ID's */
			medley_devid_offset = device;
			dprintk("Medley_devid_offset: %d\n",
				medley_devid_offset);
		} else if (device - medley_devid_offset != c) {
			printk(KERN_ERR "Medley RAID: "
			       "ATARAID gave us an illegal device ID.\n");
			ataraid_release_device(device);
			break;
		}

		printk(KERN_INFO "Medley RAID: "
		       "Striped set %d consists of %d disks, total %dMiB "
		       "- disks:",
		       c, raid[c].drives,
		       raid[c].sectors / 1024 / 1024 / 2);
		for (d = 0; d < raid[c].drives; d++) {
			printk(" %02x:%02x", MAJOR(raid[c].members[d]),
			       MINOR(raid[c].members[d]));
			raid[c].bdev[d] = get_device_lock(raid[c].members[d]);
		}
		printk("\n");
		raid[c].registered = 1;
		atomic_set(&(raid[c].valid), 1);
		ataraid_register_disk(c, raid[c].sectors);
	}

	if (medley_arrays > 0) {
		printk(KERN_INFO "Medley RAID: %d active RAID set%s\n",
		       medley_arrays, medley_arrays == 1 ? "" : "s");
		return 0;
	}

	printk(KERN_INFO "Medley RAID: No usable RAID sets found\n");
	return -ENODEV;
}

/*
 * Remove the arrays and clean up.
 */
static void __exit medley_exit(void)
{
	int device, d;
	for (device = 0; device < medley_arrays; device++) {
		for (d = 0; d < raid[device].drives; d++) {
			if (raid[device].bdev[d]) {
				blkdev_put(raid[device].bdev[d], BDEV_RAW);
				raid[device].bdev[d] = NULL;
			}
		}
		if (raid[device].registered) {
			ataraid_release_device(device +
					       medley_devid_offset);
			raid[device].registered = 0;
		}
	}
}

/*
 * Called to open the virtual device
 */
static int medley_open(struct inode *inode, struct file *filp)
{
	int device = ((inode->i_rdev >> SHIFT) & MAJOR_MASK) -
	    medley_devid_offset;
	dprintk("medley_open\n");

	if (device < medley_arrays) {
		while (!atomic_read(&(raid[device].valid)))
			sleep_on(&medley_wait_open);
		raid[device].access++;
		MOD_INC_USE_COUNT;
		return (0);
	}
	return -ENODEV;
}

/*
 * Called to release the handle on the virtual device
 */
static int medley_release(struct inode *inode, struct file *filp)
{
	int device = ((inode->i_rdev >> SHIFT) & MAJOR_MASK) -
	    medley_devid_offset;
	dprintk("medley_release\n");
	raid[device].access--;
	MOD_DEC_USE_COUNT;
	return 0;
}

module_init(medley_init);
module_exit(medley_exit);
MODULE_LICENSE("GPL");
