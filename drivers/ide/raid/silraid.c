/*
   silraid.c  Copyright (C) 2002 Red Hat, Inc. All rights reserved.

   The contents of this file are subject to the Open Software License version 1.1
   that can be found at http://www.opensource.org/licenses/osl-1.1.txt and is 
   included herein by reference. 
   
   Alternatively, the contents of this file may be used under the
   terms of the GNU General Public License version 2 (the "GPL") as 
   distributed in the kernel source COPYING file, in which
   case the provisions of the GPL are applicable instead of the
   above.  If you wish to allow the use of your version of this file
   only under the terms of the GPL and not to allow others to use
   your version of this file under the OSL, indicate your decision
   by deleting the provisions above and replace them with the notice
   and other provisions required by the GPL.  If you do not delete
   the provisions above, a recipient may use your version of this
   file under either the OSL or the GPL.
   
   Authors: 	Arjan van de Ven <arjanv@redhat.com>
   		

*/

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

static int silraid_open(struct inode * inode, struct file * filp);
static int silraid_release(struct inode * inode, struct file * filp);
static int silraid_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);
static int silraid0_make_request (request_queue_t *q, int rw, struct buffer_head * bh);

struct disk_dev {
	int major;
	int minor;
	int device;
};

static struct disk_dev devlist[]= {
	{IDE0_MAJOR,  0,  -1 },
	{IDE0_MAJOR, 64,  -1 },
	{IDE1_MAJOR,  0,  -1 },
	{IDE1_MAJOR, 64,  -1 },
	{IDE2_MAJOR,  0,  -1 },
	{IDE2_MAJOR, 64,  -1 },
	{IDE3_MAJOR,  0,  -1 },
	{IDE3_MAJOR, 64,  -1 },
	{IDE4_MAJOR,  0,  -1 },
	{IDE4_MAJOR, 64,  -1 },
	{IDE5_MAJOR,  0,  -1 },
	{IDE5_MAJOR, 64,  -1 },
	{IDE6_MAJOR,  0,  -1 },
	{IDE6_MAJOR, 64,  -1 },
};


struct sildisk {
	kdev_t	device;
	unsigned long sectors;
	struct block_device *bdev;
	unsigned long last_pos;
};

struct silraid {
	unsigned int stride;
	unsigned int disks;
	unsigned long sectors;
	struct geom geom;
	
	struct sildisk disk[8];
	
	unsigned long cutoff[8];
	unsigned int cutoff_disks[8];
};

static struct raid_device_operations silraid0_ops = {
        open:                   silraid_open,
	release:                silraid_release,
	ioctl:			silraid_ioctl,
	make_request:		silraid0_make_request
};

static struct silraid raid[16];


static int silraid_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	unsigned int minor;
   	unsigned long sectors;

	if (!inode || !inode->i_rdev) 
		return -EINVAL;

	minor = MINOR(inode->i_rdev)>>SHIFT;
	
	switch (cmd) {

         	case BLKGETSIZE:   /* Return device size */
 			if (!arg)  return -EINVAL;
			sectors = ataraid_gendisk.part[MINOR(inode->i_rdev)].nr_sects;
			if (MINOR(inode->i_rdev)&15)
				return put_user(sectors, (unsigned long *) arg);
			return put_user(raid[minor].sectors , (unsigned long *) arg);
			break;
			

		case HDIO_GETGEO:
		{
			struct hd_geometry *loc = (struct hd_geometry *) arg;
			unsigned short bios_cyl = raid[minor].geom.cylinders; /* truncate */
			
			if (!loc) return -EINVAL;
			if (put_user(raid[minor].geom.heads, (byte *) &loc->heads)) return -EFAULT;
			if (put_user(raid[minor].geom.sectors, (byte *) &loc->sectors)) return -EFAULT;
			if (put_user(bios_cyl, (unsigned short *) &loc->cylinders)) return -EFAULT;
			if (put_user((unsigned)ataraid_gendisk.part[MINOR(inode->i_rdev)].start_sect,
				(unsigned long *) &loc->start)) return -EFAULT;
			return 0;
		}

		case HDIO_GETGEO_BIG:
		{
			struct hd_big_geometry *loc = (struct hd_big_geometry *) arg;
			if (!loc) return -EINVAL;
			if (put_user(raid[minor].geom.heads, (byte *) &loc->heads)) return -EFAULT;
			if (put_user(raid[minor].geom.sectors, (byte *) &loc->sectors)) return -EFAULT;
			if (put_user(raid[minor].geom.cylinders, (unsigned int *) &loc->cylinders)) return -EFAULT;
			if (put_user((unsigned)ataraid_gendisk.part[MINOR(inode->i_rdev)].start_sect,
				(unsigned long *) &loc->start)) return -EFAULT;
			return 0;
		}

			
		case BLKROSET:
		case BLKROGET:
		case BLKSSZGET:
			return blk_ioctl(inode->i_rdev, cmd, arg);

		default:
			printk("Invalid ioctl \n");
			return -EINVAL;
	};

	return 0;
}


static unsigned long partition_map_normal(unsigned long block, unsigned long partition_off, unsigned long partition_size, int stride)
{
	return block + partition_off;
}

static int silraid0_make_request (request_queue_t *q, int rw, struct buffer_head * bh)
{
	unsigned long rsect;
	unsigned long rsect_left,rsect_accum = 0;
	unsigned long block;
	unsigned int disk=0,real_disk=0;
	int i;
	int device;
	struct silraid *thisraid;

	rsect = bh->b_rsector;
	
	/* Ok. We need to modify this sector number to a new disk + new sector number. 
	 * If there are disks of different sizes, this gets tricky. 
	 * Example with 3 disks (1Gb, 4Gb and 5 GB):
	 * The first 3 Gb of the "RAID" are evenly spread over the 3 disks.
	 * Then things get interesting. The next 2Gb (RAID view) are spread across disk 2 and 3
	 * and the last 1Gb is disk 3 only.
	 *
	 * the way this is solved is like this: We have a list of "cutoff" points where everytime
	 * a disk falls out of the "higher" count, we mark the max sector. So once we pass a cutoff
	 * point, we have to divide by one less.
	 */
	
	device = (bh->b_rdev >> SHIFT)&MAJOR_MASK;
	thisraid = &raid[device];
	if (thisraid->stride==0)
		thisraid->stride=1;

	/* Partitions need adding of the start sector of the partition to the requested sector */
	
	rsect = partition_map_normal(rsect, ataraid_gendisk.part[MINOR(bh->b_rdev)].start_sect, ataraid_gendisk.part[MINOR(bh->b_rdev)].nr_sects, thisraid->stride);

	/* Woops we need to split the request to avoid crossing a stride barrier */
	if ((rsect/thisraid->stride) != ((rsect+(bh->b_size/512)-1)/thisraid->stride)) {
		return -1;  
	}
	
	rsect_left = rsect;
	
	for (i=0;i<8;i++) {
		if (thisraid->cutoff_disks[i]==0)
			break;
		if (rsect > thisraid->cutoff[i]) {
			/* we're in the wrong area so far */
			rsect_left -= thisraid->cutoff[i];
			rsect_accum += thisraid->cutoff[i]/thisraid->cutoff_disks[i];
		} else {
			block = rsect_left / thisraid->stride;
			disk = block % thisraid->cutoff_disks[i];
			block = (block / thisraid->cutoff_disks[i]) * thisraid->stride;
			rsect = rsect_accum + (rsect_left % thisraid->stride) + block;
			break;
		}
	}
	
	for (i=0;i<8;i++) {
		if ((disk==0) && (thisraid->disk[i].sectors > rsect_accum)) {
			real_disk = i;
			break;
		}
		if ((disk>0) && (thisraid->disk[i].sectors >= rsect_accum)) {
			disk--;
		}
		
	}
	disk = real_disk;
		
	
	/*
	 * The new BH_Lock semantics in ll_rw_blk.c guarantee that this
	 * is the only IO operation happening on this bh.
	 */
	bh->b_rdev = thisraid->disk[disk].device;
	bh->b_rsector = rsect;

	/*
	 * Let the main block layer submit the IO and resolve recursion:
	 */
	return 1;
}

#include "silraid.h"

static unsigned long calc_silblock_offset (int major,int minor)
{
	unsigned long lba = 0, cylinders;
	kdev_t dev;
	ide_drive_t *ideinfo;
	
	dev = MKDEV(major,minor);
	ideinfo = ide_info_ptr (dev, 0);
	if (ideinfo==NULL)
		return 0;
	
	
	/* last sector second to last cylinder */
	if (ideinfo->head==0) 
		return 0;
	if (ideinfo->sect==0)
		return 0;
	cylinders = (ideinfo->capacity / (ideinfo->head*ideinfo->sect));
	lba = (cylinders - 1) * (ideinfo->head*ideinfo->sect);
	lba = lba - ideinfo->head -1;
	
//	return 80417215;  
	printk("Guestimating sector %li for superblock\n",lba);
	return lba;

}



static int read_disk_sb (int major, int minor, unsigned char *buffer,int bufsize)
{
	int ret = -EINVAL;
	struct buffer_head *bh = NULL;
	kdev_t dev = MKDEV(major,minor);
	unsigned long sb_offset;
	
	if (blksize_size[major]==NULL)   /* device doesn't exist */
		return -EINVAL;
                       

	/*
	 * Calculate the position of the superblock,
	 * it's at first sector of the last cylinder
	 */
	sb_offset = calc_silblock_offset(major,minor)/8;
	/* The /8 transforms sectors into 4Kb blocks */

	if (sb_offset==0)
		return -1;	
	
	set_blocksize (dev, 4096);

	bh = bread (dev, sb_offset, 4096);
	
	if (bh) {
		memcpy (buffer, bh->b_data, bufsize);
	} else {
		printk(KERN_ERR "silraid: Error reading superblock.\n");
		goto abort;
	}
	ret = 0;
abort:
	if (bh)
		brelse (bh); return ret;
}

static unsigned short checksum1(unsigned short *buffer)
{
	int i;
	int sum = 0;
	for (i=0; i<0x13f/2; i++)
		sum += buffer[i];	
	return (-sum)&0xFFFF;
}

static int cookie = 0;

static void __init probedisk(int devindex,int device, int raidlevel)
{
	int i;
	int major, minor;
        struct signature *superblock;
	static unsigned char block[4096];
	struct block_device *bdev;

	if (devlist[devindex].device!=-1) /* already assigned to another array */
		return;
	
	major = devlist[devindex].major;
	minor = devlist[devindex].minor; 

        if (read_disk_sb(major,minor,(unsigned char*)&block,sizeof(block)))
        	return;
                                                                                                                 
        superblock = (struct signature*)&block[4096-512];
        
        if (superblock->unknown[0] != 'Z') /* Need better check here */
        	return;
        	
        if (superblock->checksum1 != checksum1((unsigned short*)superblock))
        	return;  
        
        

	if (superblock->raidlevel!=raidlevel) /* different raidlevel */
		return;

	/* This looks evil. But basically, we have to search for our adapternumber
	   in the arraydefinition, both of which are in the superblock */	
	 i = superblock->disk_in_set;

	bdev = bdget(MKDEV(major,minor));
        if (bdev && blkdev_get(bdev, FMODE_READ|FMODE_WRITE, 0, BDEV_RAW) == 0) {
		raid[device].disk[i].bdev = bdev;
	}
	raid[device].disk[i].device = MKDEV(major,minor);
	raid[device].disk[i].sectors = superblock->thisdisk_sectors; 
	raid[device].stride = superblock->raid0_sectors_per_stride;
	raid[device].disks = superblock->disks_in_set;
	raid[device].sectors = superblock->array_sectors;
	raid[device].geom.heads = 255;
	raid[device].geom.sectors = 63;
	raid[device].geom.cylinders =  raid[device].sectors / raid[device].geom.heads / raid[device].geom.sectors; 
	
	devlist[devindex].device=device;
	               
}

static void __init fill_cutoff(int device)
{
	int i,j;
	unsigned long smallest;
	unsigned long bar;
	int count;
	
	bar = 0;
	for (i=0;i<8;i++) {
		smallest = ~0;
		for (j=0;j<8;j++) 
			if ((raid[device].disk[j].sectors < smallest) && (raid[device].disk[j].sectors>bar))
				smallest = raid[device].disk[j].sectors;
		count = 0;
		for (j=0;j<8;j++) 
			if (raid[device].disk[j].sectors >= smallest)
				count++;
				
		smallest = smallest * count;
		bar = smallest;
		raid[device].cutoff[i] = smallest;
		raid[device].cutoff_disks[i] = count;
	}
}
			   
static __init int silraid_init_one(int device,int raidlevel)
{
	int i, count;

	for (i=0; i<14; i++) 
		probedisk(i, device, raidlevel);
	
	if (raidlevel==0)
		fill_cutoff(device);
	
	/* Initialize the gendisk structure */
	
	ataraid_register_disk(device,raid[device].sectors);        
		
	count=0;
	
	for (i=0;i<8;i++) {
		if (raid[device].disk[i].device!=0) {
			printk(KERN_INFO "Drive %i is %li Mb (%i / %i) \n",
				i,raid[device].disk[i].sectors/2048,MAJOR(raid[device].disk[i].device),MINOR(raid[device].disk[i].device));
			count++;
		}
	}
	if (count) {
		printk(KERN_INFO "Raid%i array consists of %i drives. \n",raidlevel,count);
		return 0;
	} else {
		return -ENODEV;
	}
}

static __init int silraid_init(void)
{
	int retval, device, count = 0;

	do {
		cookie = 0;
		device=ataraid_get_device(&silraid0_ops);
		if (device<0)
			break;
		retval = silraid_init_one(device,0);
		if (retval) {
			ataraid_release_device(device);
			break;
		} else {
			count++;
		}
	} while (1);

	if (count) {
		printk(KERN_INFO "driver for Silicon Image(tm) Medley(tm) hardware version 0.0.1\n");
		return 0;
	}
	printk(KERN_DEBUG "driver for Silicon Image(tm) Medley(tm) hardware version 0.0.1: No raid array found\n");
	return -ENODEV;
}

static void __exit silraid_exit (void)
{
	int i,device;
	for (device = 0; device<16; device++) {
		for (i=0;i<8;i++) {
			struct block_device *bdev = raid[device].disk[i].bdev;
			raid[device].disk[i].bdev = NULL;
			if (bdev)
				blkdev_put(bdev, BDEV_RAW);
		}	
		if (raid[device].sectors)
			ataraid_release_device(device);
	}
}

static int silraid_open(struct inode * inode, struct file * filp) 
{
	MOD_INC_USE_COUNT;
	return 0;
}
static int silraid_release(struct inode * inode, struct file * filp)
{	
	MOD_DEC_USE_COUNT;
	return 0;
}

module_init(silraid_init);
module_exit(silraid_exit);
MODULE_LICENSE("GPL and additional rights");
