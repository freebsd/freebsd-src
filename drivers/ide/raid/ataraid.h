/*
   ataraid.h  Copyright (C) 2001 Red Hat, Inc. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.
   
   You should have received a copy of the GNU General Public License
   (for example /usr/src/linux/COPYING); if not, write to the Free
   Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  
   
   Authors: 	Arjan van de Ven <arjanv@redhat.com>
   		
   
*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <asm/semaphore.h>
#include <linux/blkdev.h>
#include <linux/blkpg.h>
#include <linux/genhd.h>
#include <linux/ioctl.h>

#include <linux/ide.h>
#include <asm/uaccess.h>

#define ATAMAJOR 114
#define SHIFT 4
#define MINOR_MASK 15
#define MAJOR_MASK 15

                                        
/* raid_device_operations is a light struct block_device_operations with an
   added method for make_request */
struct raid_device_operations {
	int (*open) (struct inode *, struct file *);
	int (*release) (struct inode *, struct file *);
	int (*ioctl) (struct inode *, struct file *, unsigned, unsigned long);
	int (*make_request) (request_queue_t *q, int rw, struct buffer_head * bh);
};


struct geom {
	unsigned char heads;
	unsigned int cylinders;
	unsigned char sectors;
};

/* structure for the splitting of bufferheads */

struct ataraid_bh_private {
	struct buffer_head *parent;
	atomic_t count;
};

extern struct gendisk ataraid_gendisk;

extern int ataraid_get_device(struct raid_device_operations *fops);
extern void ataraid_release_device(int device);
extern int get_blocksize(kdev_t dev);
extern void ataraid_register_disk(int device,long size);
extern struct buffer_head *ataraid_get_bhead(void);
extern struct ataraid_bh_private *ataraid_get_private(void);
extern void ataraid_end_request(struct buffer_head *bh, int uptodate);





