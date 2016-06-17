/*
   hptraid.c  Copyright (C) 2001 Red Hat, Inc. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.
   
   You should have received a copy of the GNU General Public License
   (for example /usr/src/linux/COPYING); if not, write to the Free
   Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  
   
   Authors: 	Arjan van de Ven <arjanv@redhat.com>

   Based on work
   	Copyleft  (C) 2001 by Wilfried Weissmann <wweissmann@gmx.at>
	Copyright (C) 1994-96 Marc ZYNGIER <zyngier@ufr-info-p7.ibp.fr>
   Based on work done by Søren Schmidt for FreeBSD

   Changelog:
   15.06.2003 wweissmann@gmx.at
   	* correct values of raid-1 superbock
	* re-add check for availability of all disks
	* fix offset bug in raid-1 (introduced in raid 0+1 implementation)

   14.06.2003 wweissmann@gmx.at
   	* superblock has wrong "disks" value on raid-1
   	* fixup for raid-1 disknumbering
	* do _NOT_ align size to 255*63 boundary
		I WILL NOT USE FDISK TO DETERMINE THE VOLUME SIZE.
		I WILL NOT USE FDISK TO DETERMINE THE VOLUME SIZE.
		I WILL NOT USE FDISK TO DETERMINE THE VOLUME SIZE.
		I WILL NOT ...

   13.06.2003 wweissmann@gmx.at
   	* raid 0+1 support
	* check if all disks of an array are available
	* bump version number

   29.05.2003 wweissmann@gmx.at
   	* release no more devices than available on unload
	* remove static variables in raid-1 read path

*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/smp_lock.h>
#include <linux/kernel.h>
#include <linux/blkdev.h>
#include <linux/blkpg.h>
#include <linux/genhd.h>
#include <linux/ioctl.h>

#include <linux/ide.h>
#include <asm/uaccess.h>

#include "ataraid.h"
#include "hptraid.h"


static int hptraid_open(struct inode * inode, struct file * filp);
static int hptraid_release(struct inode * inode, struct file * filp);
static int hptraid_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);
static int hptraidspan_make_request (request_queue_t *q, int rw, struct buffer_head * bh);
static int hptraid0_make_request (request_queue_t *q, int rw, struct buffer_head * bh);
static int hptraid1_make_request (request_queue_t *q, int rw, struct buffer_head * bh);
static int hptraid01_make_request (request_queue_t *q, int rw, struct buffer_head * bh);



struct hptdisk {
	kdev_t	device;		/* disk-ID/raid 0+1 volume-ID */
	unsigned long sectors;
	struct block_device *bdev;
	unsigned long last_pos;
};

struct hptraid {
	unsigned int stride;	/* stripesize */
	unsigned int disks;	/* number of disks in array */
	unsigned long sectors;	/* disksize in sectors */
        u_int32_t magic_0;
        u_int32_t magic_1;
	struct geom geom;
	
	int previous;		/* most recently accessed disk in mirror */
	struct hptdisk disk[8];
	unsigned long cutoff[8];	/* raid 0 cutoff */
	unsigned int cutoff_disks[8];	
	struct hptraid * raid01;	/* sub arrays for raid 0+1 */
};

struct hptraid_dev {
	int major;
	int minor;
	int device;
};

static struct hptraid_dev devlist[]=
{

	{IDE0_MAJOR,  0, -1},
	{IDE0_MAJOR, 64, -1},
	{IDE1_MAJOR,  0, -1},
	{IDE1_MAJOR, 64, -1},
	{IDE2_MAJOR,  0, -1},
	{IDE2_MAJOR, 64, -1},
	{IDE3_MAJOR,  0, -1},
	{IDE3_MAJOR, 64, -1},
	{IDE4_MAJOR,  0, -1},
	{IDE4_MAJOR, 64, -1},
	{IDE5_MAJOR,  0, -1},
	{IDE5_MAJOR, 64, -1},
	{IDE6_MAJOR,  0, -1},
	{IDE6_MAJOR, 64, -1}
};

static struct raid_device_operations hptraidspan_ops = {
	open:                   hptraid_open,
	release:                hptraid_release,
	ioctl:			hptraid_ioctl,
	make_request:		hptraidspan_make_request
};

static struct raid_device_operations hptraid0_ops = {
	open:                   hptraid_open,
	release:                hptraid_release,
	ioctl:			hptraid_ioctl,
	make_request:		hptraid0_make_request
};

static struct raid_device_operations hptraid1_ops = {
	open:                   hptraid_open,
	release:                hptraid_release,
	ioctl:			hptraid_ioctl,
	make_request:		hptraid1_make_request
};


static struct raid_device_operations hptraid01_ops = {
	open:                   hptraid_open,
	release:                hptraid_release,
	ioctl:			hptraid_ioctl,
	make_request:		hptraid01_make_request
};

static __initdata struct {
	struct raid_device_operations *op;
	u_int8_t type;
	char label[8];
} oplist[] = {
	{&hptraid0_ops, HPT_T_RAID_0, "RAID 0"},
	{&hptraid1_ops, HPT_T_RAID_1, "RAID 1"},
	{&hptraidspan_ops, HPT_T_SPAN, "SPAN"},
	{&hptraid01_ops, HPT_T_RAID_01_RAID_0, "RAID 0+1"},
	{0, 0}
};

static struct hptraid raid[14];

static int hptraid_ioctl(struct inode *inode, struct file *file,
		unsigned int cmd, unsigned long arg)
{
	unsigned int minor;
	unsigned char val;
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
			unsigned short bios_cyl;
			
			if (!loc) return -EINVAL;
			val = 255;
			if (put_user(val, (byte *) &loc->heads)) return -EFAULT;
			val=63;
			if (put_user(val, (byte *) &loc->sectors)) return -EFAULT;
			bios_cyl = raid[minor].sectors/63/255;
			if (put_user(bios_cyl, (unsigned short *) &loc->cylinders)) return -EFAULT;
			if (put_user((unsigned)ataraid_gendisk.part[MINOR(inode->i_rdev)].start_sect,
				(unsigned long *) &loc->start)) return -EFAULT;
			return 0;
		}

		case HDIO_GETGEO_BIG:
		{
			struct hd_big_geometry *loc = (struct hd_big_geometry *) arg;
			unsigned int bios_cyl;
			if (!loc) return -EINVAL;
			val = 255;
			if (put_user(val, (byte *) &loc->heads)) return -EFAULT;
			val = 63;
			if (put_user(val, (byte *) &loc->sectors)) return -EFAULT;
			bios_cyl = raid[minor].sectors/63/255;
			if (put_user(bios_cyl, (unsigned int *) &loc->cylinders)) return -EFAULT;
			if (put_user((unsigned)ataraid_gendisk.part[MINOR(inode->i_rdev)].start_sect,
				(unsigned long *) &loc->start)) return -EFAULT;
			return 0;
		}
			
		default:
			return blk_ioctl(inode->i_rdev, cmd, arg);
	};

	return 0;
}


static int hptraidspan_make_request (request_queue_t *q, int rw, struct buffer_head * bh)
{
	unsigned long rsect;
	unsigned int disk;
	int device;
	struct hptraid *thisraid;

	rsect = bh->b_rsector;

	device = (bh->b_rdev >> SHIFT)&MAJOR_MASK;
	thisraid = &raid[device];

	/*
	 * Partitions need adding of the start sector of the partition to the
	 * requested sector
	 */
	
	rsect += ataraid_gendisk.part[MINOR(bh->b_rdev)].start_sect;

	for (disk=0;disk<thisraid->disks;disk++) {
		if (disk==1)
			rsect+=10;
			// the "on next disk" contition check is a bit odd
		if (thisraid->disk[disk].sectors > rsect+1)
			break;
		rsect-=thisraid->disk[disk].sectors-(disk?11:1);
	}

		// request spans over 2 disks => request must be split
	if(rsect+bh->b_size/512 >= thisraid->disk[disk].sectors)
		return -1;
	
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

static int hptraid0_compute_request (struct hptraid *thisraid,
		request_queue_t *q,
		int rw, struct buffer_head * bh)
{
	unsigned long rsect_left,rsect_accum = 0;
	unsigned long block;
	unsigned int disk=0,real_disk=0;
	int i;

	/* Ok. We need to modify this sector number to a new disk + new sector
	 * number. 
	 * If there are disks of different sizes, this gets tricky. 
	 * Example with 3 disks (1Gb, 4Gb and 5 GB):
	 * The first 3 Gb of the "RAID" are evenly spread over the 3 disks.
	 * Then things get interesting. The next 2Gb (RAID view) are spread
	 * across disk 2 and 3 and the last 1Gb is disk 3 only.
	 *
	 * the way this is solved is like this: We have a list of "cutoff"
	 * points where everytime a disk falls out of the "higher" count, we
	 * mark the max sector. So once we pass a cutoff point, we have to
	 * divide by one less.
	 */
	
	if (thisraid->stride==0)
		thisraid->stride=1;

	/*
	 * Woops we need to split the request to avoid crossing a stride
	 * barrier
	 */
	if ((bh->b_rsector/thisraid->stride) !=
			((bh->b_rsector+(bh->b_size/512)-1)/thisraid->stride)) {
		return -1;
	}
			
	rsect_left = bh->b_rsector;;
	
	for (i=0;i<8;i++) {
		if (thisraid->cutoff_disks[i]==0)
			break;
		if (bh->b_rsector > thisraid->cutoff[i]) {
			/* we're in the wrong area so far */
			rsect_left -= thisraid->cutoff[i];
			rsect_accum += thisraid->cutoff[i] /
				thisraid->cutoff_disks[i];
		} else {
			block = rsect_left / thisraid->stride;
			disk = block % thisraid->cutoff_disks[i];
			block = (block / thisraid->cutoff_disks[i]) *
				thisraid->stride;
			bh->b_rsector = rsect_accum +
				(rsect_left % thisraid->stride) + block;
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
	
	/* All but the first disk have a 10 sector offset */
	if (i>0)
		bh->b_rsector+=10;
		
	
	/*
	 * The new BH_Lock semantics in ll_rw_blk.c guarantee that this
	 * is the only IO operation happening on this bh.
	 */
	 
	bh->b_rdev = thisraid->disk[disk].device;

	/*
	 * Let the main block layer submit the IO and resolve recursion:
	 */
	return 1;
}

static int hptraid0_make_request (request_queue_t *q, int rw, struct buffer_head * bh)
{
	unsigned long rsect;
	int device;

	/*
	 * save the sector, it must be restored before a request-split
	 * is performed
	 */
	rsect = bh->b_rsector;

	/*
	 * Partitions need adding of the start sector of the partition to the
	 * requested sector
	 */
	
	bh->b_rsector += ataraid_gendisk.part[MINOR(bh->b_rdev)].start_sect;

	device = (bh->b_rdev >> SHIFT)&MAJOR_MASK;
	if( hptraid0_compute_request(raid+device, q, rw, bh) != 1 ) {
			/* request must be split => restore sector */
		bh->b_rsector = rsect;
		return -1;
	}

	return 1;
}

static int hptraid1_read_request (request_queue_t *q, int rw, struct buffer_head * bh)
{
	int device;
	int dist;
	int bestsofar,bestdist,i;

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

		  /* it's a tie; try to do some read balancing */
		if (bestdist==dist) {
			if ( (raid[device].previous>bestsofar) &&
					(raid[device].previous<=i) )  
				bestsofar = i;
			raid[device].previous =
				(raid[device].previous + 1) %
				raid[device].disks;
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

static int hptraid1_write_request(request_queue_t *q, int rw, struct buffer_head * bh)
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
	
		/*
		 * dupe the bufferhead and update the parts that need to be
		 * different
		 */
		memcpy(bh1, bh, sizeof(*bh));
		
		bh1->b_end_io = ataraid_end_request;
		bh1->b_private = private;
		bh1->b_rsector += ataraid_gendisk.part[MINOR(bh->b_rdev)].start_sect; /* partition offset */
		bh1->b_rdev = raid[device].disk[i].device;

		/* update the last known head position for the drive */
		raid[device].disk[i].last_pos = bh1->b_rsector+(bh1->b_size>>9);

		if( raid[device].raid01 ) {
			if( hptraid0_compute_request(
						raid[device].raid01 +
							(bh1->b_rdev-1),
						q, rw, bh1) != 1 ) {
				/*
				 * If a split is requested then it is requested
				 * in the first iteration. This is true because
				 * of the cutoff is not used in raid 0+1.
				 */
				if(unlikely(i)) {
					BUG();
				}
				else {
					kfree(private);
					return -1;
				}
			}
		}
		generic_make_request(rw,bh1);
	}
	return 0;
}

static int hptraid1_make_request (request_queue_t *q, int rw, struct buffer_head * bh) {
	/*
	 * Read and Write are totally different cases; split them totally
	 * here
	 */
	if (rw==READA)
		rw = READ;
	
	if (rw==READ)
		return hptraid1_read_request(q,rw,bh);
	else
		return hptraid1_write_request(q,rw,bh);
}

static int hptraid01_read_request (request_queue_t *q, int rw, struct buffer_head * bh)
{
	int rsector=bh->b_rsector;
	int rdev=bh->b_rdev;

		/* select mirror volume */
	hptraid1_read_request(q, rw, bh);

		/* stripe volume is selected by "bh->b_rdev" */
	if( hptraid0_compute_request(
				raid[(bh->b_rdev >> SHIFT)&MAJOR_MASK].
					raid01 + (bh->b_rdev-1) ,
				q, rw, bh) != 1 ) {

			/* request must be split => restore sector and device */
		bh->b_rsector = rsector;
		bh->b_rdev = rdev;
		return -1;

	}

	return 1;
}

static int hptraid01_make_request (request_queue_t *q, int rw, struct buffer_head * bh) {
	/*
	 * Read and Write are totally different cases; split them totally
	 * here
	 */
	if (rw==READA)
		rw = READ;
	
	if (rw==READ)
		return hptraid01_read_request(q,rw,bh);
	else
		return hptraid1_write_request(q,rw,bh);
}

static int read_disk_sb (int major, int minor, unsigned char *buffer,int bufsize)
{
	int ret = -EINVAL;
	struct buffer_head *bh = NULL;
	kdev_t dev = MKDEV(major,minor);
	
	if (blksize_size[major]==NULL)	 /* device doesn't exist */
		return -EINVAL;
	

	/* Superblock is at 4096+412 bytes */
	set_blocksize (dev, 4096);
	bh = bread (dev, 1, 4096);

	
	if (bh) {
		memcpy (buffer, bh->b_data, bufsize);
	} else {
		printk(KERN_ERR "hptraid: Error reading superblock.\n");
		goto abort;
	}
	ret = 0;
abort:
	if (bh)
		brelse (bh);
	return ret;
}

static unsigned long maxsectors (int major,int minor)
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
	lba = (ideinfo->capacity);

	return lba;
}

static void writeentry(struct hptraid * raid, struct hptraid_dev * disk,
		int index, struct highpoint_raid_conf * prom) {

	int j=0;
	struct gendisk *gd;
	struct block_device *bdev;

	bdev = bdget(MKDEV(disk->major,disk->minor));
	if (bdev && blkdev_get(bdev,FMODE_READ|FMODE_WRITE,0,BDEV_RAW) == 0) {
		raid->disk[index].bdev = bdev;
        	/*
		 * This is supposed to prevent others from stealing our
		 * underlying disks now blank the /proc/partitions table for 
		 * the wrong partition table, so that scripts don't
		 * accidentally mount it and crash the kernel
		 */
		 /* XXX: the 0 is an utter hack  --hch */
		gd=get_gendisk(MKDEV(disk->major, 0));
		if (gd!=NULL) {
 			if (gd->major==disk->major)
 				for (j=1+(disk->minor<<gd->minor_shift);
					j<((disk->minor+1)<<gd->minor_shift);
					j++) gd->part[j].nr_sects=0;					
		}
        }
	raid->disk[index].device = MKDEV(disk->major,disk->minor);
	raid->disk[index].sectors = maxsectors(disk->major,disk->minor);
	raid->stride = (1<<prom->raid0_shift);
	raid->disks = prom->raid_disks;
	raid->sectors = prom->total_secs;
	raid->sectors += raid->sectors&1?1:0;
	raid->magic_0=prom->magic_0;
	raid->magic_1=prom->magic_1;

}

static int probedisk(struct hptraid_dev *disk, int device, u_int8_t type)
{
	int i, j;
        struct highpoint_raid_conf *prom;
	static unsigned char block[4096];
	
 	if (disk->device != -1)	/* disk is occupied? */
 		return 0;
 
 	if (maxsectors(disk->major,disk->minor)==0)
		return 0;
	
        if (read_disk_sb(disk->major,disk->minor,(unsigned char*)&block,sizeof(block)))
        	return 0;
                                                                                                                 
        prom = (struct highpoint_raid_conf*)&block[512];
                
        if (prom->magic!=  0x5a7816f0)
        	return 0;
        switch (prom->type) {
		case HPT_T_SPAN:
		case HPT_T_RAID_0:
		case HPT_T_RAID_1:
		case HPT_T_RAID_01_RAID_0:
			if(prom->type != type)
				return 0;
			break;
		default:
			printk(KERN_INFO "hptraid: unknown raid level-id %i\n",
					prom->type);
			return 0;
        }

 		/* disk from another array? */
	if (raid[device].disks) {	/* only check if raid is not empty */
		if (type == HPT_T_RAID_01_RAID_0 ) {
			if( prom->magic_1 != raid[device].magic_1) {
				return 0;
			}
		}
		else if (prom->magic_0 != raid[device].magic_0) {
				return 0;
		}
	}

	i = prom->disk_number;
	if (i<0)
		return 0;
	if (i>8) 
		return 0;

	if ( type == HPT_T_RAID_01_RAID_0 ) {

			/* allocate helper raid devices for level 0+1 */
		if (raid[device].raid01 == NULL ) {

			raid[device].raid01=
				kmalloc(2 * sizeof(struct hptraid),GFP_KERNEL);
			if ( raid[device].raid01 == NULL ) {
				printk(KERN_ERR "hptraid: out of memory\n");
				raid[device].disks=-1;
				return -ENOMEM;
			}
			memset(raid[device].raid01, 0,
					2 * sizeof(struct hptraid));
		}

			/* find free sub-stucture */
		for (j=0; j<2; j++) {
			if ( raid[device].raid01[j].disks == 0 ||
			     raid[device].raid01[j].magic_0 == prom->magic_0 )
			{
				writeentry(raid[device].raid01+j, disk,
						i, prom);
				break;
			}
		}

			/* no free slot */
		if(j == 2)
			return 0;

		raid[device].stride=raid[device].raid01[j].stride;
		raid[device].disks=j+1;
		raid[device].sectors=raid[device].raid01[j].sectors;
		raid[device].disk[j].sectors=raid[device].raid01[j].sectors;
		raid[device].magic_1=prom->magic_1;
	}
	else {
		writeentry(raid+device, disk, i, prom);
	}

	disk->device=device;
			
	return 1;
}

static void fill_cutoff(struct hptraid * device)
{
	int i,j;
	unsigned long smallest;
	unsigned long bar;
	int count;
	
	bar = 0;
	for (i=0;i<8;i++) {
		smallest = ~0;
		for (j=0;j<8;j++) 
			if ((device->disk[j].sectors < smallest) && (device->disk[j].sectors>bar))
				smallest = device->disk[j].sectors;
		count = 0;
		for (j=0;j<8;j++) 
			if (device->disk[j].sectors >= smallest)
				count++;
		
		smallest = smallest * count;		
		bar = smallest;
		device->cutoff[i] = smallest;
		device->cutoff_disks[i] = count;
		
	}
}

static int count_disks(struct hptraid * raid) {
	int i, count=0;
	for (i=0;i<8;i++) {
		if (raid->disk[i].device!=0) {
			printk(KERN_INFO "Drive %i is %li Mb \n",
				i,raid->disk[i].sectors/2048);
			count++;
		}
	}
	return count;
}

static void raid1_fixup(struct hptraid * raid) {
	int i, count=0;
	for (i=0;i<8;i++) {
			/* disknumbers and total disks values are bogus */
		if (raid->disk[i].device!=0) {
			raid->disk[count]=raid->disk[i];
			if(i > count) {
				memset(raid->disk+i, 0, sizeof(struct hptdisk));
			}
			count++;
		}
	}
	raid->disks=count;
}

static int hptraid_init_one(int device, u_int8_t type, const char * label)
{
	int i,count;
	memset(raid+device, 0, sizeof(struct hptraid));
	for (i=0; i < 14; i++) {
		if( probedisk(devlist+i, device, type) < 0 )
			return -EINVAL;
	}

	/* Initialize raid levels */
	switch (type) {
		case HPT_T_RAID_0:
			fill_cutoff(raid+device);
			break;

		case HPT_T_RAID_1:
			raid1_fixup(raid+device);
			break;

		case HPT_T_RAID_01_RAID_0:
			for(i=0; i < 2 && raid[device].raid01 && 
					raid[device].raid01[i].disks; i++) {
				fill_cutoff(raid[device].raid01+i);
					/* initialize raid 0+1 volumes */
				raid[device].disk[i].device=i+1;
			}
			break;
	}

	/* Initialize the gendisk structure */
	
	ataraid_register_disk(device,raid[device].sectors);

	/* Verify that we have all disks */

	count=count_disks(raid+device);
		
	if (count != raid[device].disks) {
		printk(KERN_INFO "%s consists of %i drives but found %i drives\n",
				label, raid[device].disks, count);
		return -ENODEV;
	}
	else if (count) {
		printk(KERN_INFO "%s consists of %i drives.\n",
				label, count);
		if (type == HPT_T_RAID_01_RAID_0 ) {
			for(i=0;i<raid[device].disks;i++) {
				count=count_disks(raid[device].raid01+i);
				if(count == raid[device].raid01[i].disks) {
					printk(KERN_ERR "Sub-Raid %i array consists of %i drives.\n",
							i, count);
				}
				else {
					printk(KERN_ERR "Sub-Raid %i array consists of %i drives but found %i disk members.\n",
							i, raid[device].raid01[i].disks,
							count);
					return -ENODEV;
				}
			}
		}
		return 0;
	}
	
	return -ENODEV; /* No more raid volumes */
}

static int hptraid_init(void)
{
 	int retval=-ENODEV;
	int device,i,count=0;
  	
	printk(KERN_INFO "Highpoint HPT370 Softwareraid driver for linux version 0.02\n");

	for(i=0; oplist[i].op; i++) {
		do
		{
			device=ataraid_get_device(oplist[i].op);
			if (device<0)
				return (count?0:-ENODEV);
			retval = hptraid_init_one(device, oplist[i].type,
					oplist[i].label);
			if (retval)
				ataraid_release_device(device);
			else
				count++;
		} while(!retval);
	}
 	return (count?0:retval);
}

static void __exit hptraid_exit (void)
{
	int i,device;
	for (device = 0; device<14; device++) {
		for (i=0;i<8;i++)  {
			struct block_device *bdev = raid[device].disk[i].bdev;
			raid[device].disk[i].bdev = NULL;
			if (bdev)
				blkdev_put(bdev, BDEV_RAW);
		}       
		if (raid[device].sectors) {
			ataraid_release_device(device);
			if( raid[device].raid01 ) {
				kfree(raid[device].raid01);
			}
		}
	}
}

static int hptraid_open(struct inode * inode, struct file * filp) 
{
	MOD_INC_USE_COUNT;
	return 0;
}
static int hptraid_release(struct inode * inode, struct file * filp)
{	
	MOD_DEC_USE_COUNT;
	return 0;
}

module_init(hptraid_init);
module_exit(hptraid_exit);
MODULE_LICENSE("GPL");
