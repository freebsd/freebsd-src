/*
   pdcraid.c  Copyright (C) 2001 Red Hat, Inc. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.
   
   You should have received a copy of the GNU General Public License
   (for example /usr/src/linux/COPYING); if not, write to the Free
   Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  
   
   Authors: 	Arjan van de Ven <arjanv@redhat.com>
   		
   Based on work done by Søren Schmidt for FreeBSD  

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

static int pdcraid_open(struct inode * inode, struct file * filp);
static int pdcraid_release(struct inode * inode, struct file * filp);
static int pdcraid_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);
static int pdcraid0_make_request (request_queue_t *q, int rw, struct buffer_head * bh);
static int pdcraid1_make_request (request_queue_t *q, int rw, struct buffer_head * bh);

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


struct pdcdisk {
	kdev_t	device;
	unsigned long sectors;
	struct block_device *bdev;
	unsigned long last_pos;
};

struct pdcraid {
	unsigned int stride;
	unsigned int disks;
	unsigned long sectors;
	struct geom geom;
	
	struct pdcdisk disk[8];
	
	unsigned long cutoff[8];
	unsigned int cutoff_disks[8];
};

static struct raid_device_operations pdcraid0_ops = {
        open:                   pdcraid_open,
	release:                pdcraid_release,
	ioctl:			pdcraid_ioctl,
	make_request:		pdcraid0_make_request
};

static struct raid_device_operations pdcraid1_ops = {
        open:                   pdcraid_open,
	release:                pdcraid_release,
	ioctl:			pdcraid_ioctl,
	make_request:		pdcraid1_make_request
};

static struct pdcraid raid[16];


static int pdcraid_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
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

		default:
			return blk_ioctl(inode->i_rdev, cmd, arg);
	};

	return 0;
}


static unsigned long partition_map_normal(unsigned long block, unsigned long partition_off, unsigned long partition_size, int stride)
{
	return block + partition_off;
}

static int pdcraid0_make_request (request_queue_t *q, int rw, struct buffer_head * bh)
{
	unsigned long rsect;
	unsigned long rsect_left,rsect_accum = 0;
	unsigned long block;
	unsigned int disk=0,real_disk=0;
	int i;
	int device;
	struct pdcraid *thisraid;

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

static int pdcraid1_write_request(request_queue_t *q, int rw, struct buffer_head * bh)
{
	struct buffer_head *bh1;
	struct ataraid_bh_private *private;
	int device;
	int i;

	device = (bh->b_rdev >> SHIFT)&MAJOR_MASK;
	private = ataraid_get_private();
	if (private==NULL)
		BUG();

	private->parent = bh;
	
	atomic_set(&private->count,raid[device].disks);


	for (i = 0; i< raid[device].disks; i++) { 
		bh1=ataraid_get_bhead();
		/* If this ever fails we're doomed */
		if (!bh1)
			BUG();
	
		/* dupe the bufferhead and update the parts that need to be different */
		memcpy(bh1, bh, sizeof(*bh));
		
		bh1->b_end_io = ataraid_end_request;
		bh1->b_private = private;
		bh1->b_rsector += ataraid_gendisk.part[MINOR(bh->b_rdev)].start_sect; /* partition offset */
		bh1->b_rdev = raid[device].disk[i].device;

		/* update the last known head position for the drive */
		raid[device].disk[i].last_pos = bh1->b_rsector+(bh1->b_size>>9);

		generic_make_request(rw,bh1);
	}
	return 0;
}

static int pdcraid1_read_request (request_queue_t *q, int rw, struct buffer_head * bh)
{
	int device;
	int dist;
	int bestsofar,bestdist,i;
	static int previous;

	/* Reads are simple in principle. Pick a disk and go. 
	   Initially I cheat by just picking the one which the last known
	   head position is closest by.
	   Later on, online/offline checking and performance needs adding */
	
	device = (bh->b_rdev >> SHIFT)&MAJOR_MASK;
	bh->b_rsector += ataraid_gendisk.part[MINOR(bh->b_rdev)].start_sect;

	bestsofar = 0; 
	bestdist = raid[device].disk[0].last_pos - bh->b_rsector;
	if (bestdist<0) 
		bestdist=-bestdist;
	if (bestdist>4095)
		bestdist=4095;

	for (i=1 ; i<raid[device].disks; i++) {
		dist = raid[device].disk[i].last_pos - bh->b_rsector;
		if (dist<0) 
			dist = -dist;
		if (dist>4095)
			dist=4095;
		
		if (bestdist==dist) {  /* it's a tie; try to do some read balancing */
			if ((previous>bestsofar)&&(previous<=i))  
				bestsofar = i;
			previous = (previous + 1) % raid[device].disks;
		} else if (bestdist>dist) {
			bestdist = dist;
			bestsofar = i;
		}
	
	}
	
	bh->b_rdev = raid[device].disk[bestsofar].device; 
	raid[device].disk[bestsofar].last_pos = bh->b_rsector+(bh->b_size>>9);

	/*
	 * Let the main block layer submit the IO and resolve recursion:
	 */
                          	
	return 1;
}


static int pdcraid1_make_request (request_queue_t *q, int rw, struct buffer_head * bh)
{
	/* Read and Write are totally different cases; split them totally here */
	if (rw==READA)
		rw = READ;
	
	if (rw==READ)
		return pdcraid1_read_request(q,rw,bh);
	else
		return pdcraid1_write_request(q,rw,bh);
}

#include "pdcraid.h"

static unsigned long calc_pdcblock_offset (int major,int minor)
{
	unsigned long lba = 0;
	kdev_t dev;
	ide_drive_t *ideinfo;
	
	dev = MKDEV(major,minor);
	ideinfo = ide_info_ptr (dev, 0);
	if (ideinfo==NULL)
		return 0;
	
	
	/* first sector of the last cluster */
	if (ideinfo->head==0) 
		return 0;
	if (ideinfo->sect==0)
		return 0;
	if (ideinfo->head!=255) {
		lba = (ideinfo->capacity / (ideinfo->head*ideinfo->sect));
		lba = lba * (ideinfo->head*ideinfo->sect);
		lba = lba - ideinfo->sect; }
	else {
		lba = ideinfo->capacity - ideinfo->sect;
	}

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
	sb_offset = calc_pdcblock_offset(major,minor)/8;
	/* The /8 transforms sectors into 4Kb blocks */

	if (sb_offset==0)
		return -1;	
	
	set_blocksize (dev, 4096);

	bh = bread (dev, sb_offset, 4096);
	
	if (bh) {
		memcpy (buffer, bh->b_data, bufsize);
	} else {
		printk(KERN_ERR "pdcraid: Error reading superblock.\n");
		goto abort;
	}
	ret = 0;
abort:
	if (bh)
		brelse (bh);
	return ret;
}

static unsigned int calc_sb_csum (unsigned int* ptr)
{	
	unsigned int sum;
	int count;
	
	sum = 0;
	for (count=0;count<511;count++)
		sum += *ptr++;
	
	return sum;
}

static int cookie = 0;

static void __init probedisk(int devindex,int device, int raidlevel)
{
	int i;
	int major, minor;
        struct promise_raid_conf *prom;
	static unsigned char block[4096];
	struct block_device *bdev;

	if (devlist[devindex].device!=-1) /* already assigned to another array */
		return;
	
	major = devlist[devindex].major;
	minor = devlist[devindex].minor; 

        if (read_disk_sb(major,minor,(unsigned char*)&block,sizeof(block)))
        	return;
                                                                                                                 
        prom = (struct promise_raid_conf*)&block[512];

        /* the checksums must match */
	if (prom->checksum != calc_sb_csum((unsigned int*)prom))
		return;
	if (prom->raid.type!=raidlevel) /* different raidlevel */
		return;

	if ((cookie!=0) && (cookie != prom->raid.magic_1)) /* different array */
		return;
	
	cookie = prom->raid.magic_1;

	/* This looks evil. But basically, we have to search for our adapternumber
	   in the arraydefinition, both of which are in the superblock */	
        for (i=0;(i<prom->raid.total_disks)&&(i<8);i++) {
        	if ( (prom->raid.disk[i].channel== prom->raid.channel) &&
        	     (prom->raid.disk[i].device == prom->raid.device) ) {

        	        bdev = bdget(MKDEV(major,minor));
        	        if (bdev && blkdev_get(bdev, FMODE_READ|FMODE_WRITE, 0, BDEV_RAW) == 0) {
				raid[device].disk[i].bdev = bdev;
			}
			raid[device].disk[i].device = MKDEV(major,minor);
			raid[device].disk[i].sectors = prom->raid.disk_secs;
			raid[device].stride = (1<<prom->raid.raid0_shift);
			raid[device].disks = prom->raid.total_disks;
			raid[device].sectors = prom->raid.total_secs;
			raid[device].geom.heads = prom->raid.heads+1;
			raid[device].geom.sectors = prom->raid.sectors;
			raid[device].geom.cylinders = prom->raid.cylinders+1;
			devlist[devindex].device=device;
        	     }
        }
	               
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
			   
static __init int pdcraid_init_one(int device,int raidlevel)
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

static __init int pdcraid_init(void)
{
	int retval, device, count = 0;

	do {
		cookie = 0;
		device=ataraid_get_device(&pdcraid0_ops);
		if (device<0)
			break;
		retval = pdcraid_init_one(device,0);
		if (retval) {
			ataraid_release_device(device);
			break;
		} else {
			count++;
		}
	} while (1);

	do {
	
		cookie = 0;
		device=ataraid_get_device(&pdcraid1_ops);
		if (device<0)
			break;
		retval = pdcraid_init_one(device,1);
		if (retval) {
			ataraid_release_device(device);
			break;
		} else {
			count++;
		}
	} while (1);

	if (count) {
		printk(KERN_INFO "Promise Fasttrak(tm) Softwareraid driver for linux version 0.03beta\n");
		return 0;
	}
	printk(KERN_DEBUG "Promise Fasttrak(tm) Softwareraid driver 0.03beta: No raid array found\n");
	return -ENODEV;
}

static void __exit pdcraid_exit (void)
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

static int pdcraid_open(struct inode * inode, struct file * filp) 
{
	MOD_INC_USE_COUNT;
	return 0;
}
static int pdcraid_release(struct inode * inode, struct file * filp)
{	
	MOD_DEC_USE_COUNT;
	return 0;
}

module_init(pdcraid_init);
module_exit(pdcraid_exit);
MODULE_LICENSE("GPL");
