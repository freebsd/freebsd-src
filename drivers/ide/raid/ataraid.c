/*
   ataraid.c  Copyright (C) 2001 Red Hat, Inc. All rights reserved.

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
#include <linux/mm.h>
#include <asm/semaphore.h>
#include <linux/sched.h>
#include <linux/smp_lock.h>
#include <linux/blkdev.h>
#include <linux/blkpg.h>
#include <linux/genhd.h>
#include <linux/ioctl.h>
#include <linux/kdev_t.h>
#include <linux/swap.h>

#include <linux/ide.h>
#include <asm/uaccess.h>

#include "ataraid.h"

                                        
static int ataraid_hardsect_size[256];
static int ataraid_blksize_size[256];

static struct raid_device_operations* ataraid_ops[16];

static int ataraid_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);
static int ataraid_open(struct inode * inode, struct file * filp);
static int ataraid_release(struct inode * inode, struct file * filp);
static void ataraid_split_request(request_queue_t *q, int rw, struct buffer_head * bh);


struct gendisk ataraid_gendisk;
static int ataraid_gendisk_sizes[256];
static int ataraid_readahead[256];

static struct block_device_operations ataraid_fops = {
	owner:			THIS_MODULE,
	open:                   ataraid_open,
	release:                ataraid_release,
	ioctl:                  ataraid_ioctl,
};
                


static DECLARE_MUTEX(ataraid_sem);

/* Bitmap for the devices currently in use */
static unsigned int ataraiduse;


/* stub fops functions */

static int ataraid_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)  
{
	int minor;
	minor = MINOR(inode->i_rdev)>>SHIFT;
	
	if ((ataraid_ops[minor])&&(ataraid_ops[minor]->ioctl))
		return (ataraid_ops[minor]->ioctl)(inode,file,cmd,arg);
	return -EINVAL;
}

static int ataraid_open(struct inode * inode, struct file * filp)
{
	int minor;
	minor = MINOR(inode->i_rdev)>>SHIFT;

	if ((ataraid_ops[minor])&&(ataraid_ops[minor]->open))
		return (ataraid_ops[minor]->open)(inode,filp);
	return -EINVAL;
}


static int ataraid_release(struct inode * inode, struct file * filp)
{
	int minor;
	minor = MINOR(inode->i_rdev)>>SHIFT;

	if ((ataraid_ops[minor])&&(ataraid_ops[minor]->release))
		return (ataraid_ops[minor]->release)(inode,filp);
	return -EINVAL;
}

static int ataraid_make_request (request_queue_t *q, int rw, struct buffer_head * bh)
{
	int minor;
	int retval;
	minor = MINOR(bh->b_rdev)>>SHIFT;

	if ((ataraid_ops[minor])&&(ataraid_ops[minor]->make_request)) {
		
		retval= (ataraid_ops[minor]->make_request)(q,rw,bh);
		if (retval == -1) {
			ataraid_split_request(q,rw,bh);		
			return 0;
		} else
			return retval;
	}
	return -EINVAL;
}

struct buffer_head *ataraid_get_bhead(void)
{
	void *ptr = NULL;
	while (!ptr) {
		ptr=kmalloc(sizeof(struct buffer_head),GFP_NOIO);
		if (!ptr) {
			__set_current_state(TASK_RUNNING);
			yield();
		}
	}
	return ptr;
}

EXPORT_SYMBOL(ataraid_get_bhead);

struct ataraid_bh_private *ataraid_get_private(void)
{
	void *ptr = NULL;
	while (!ptr) {
		ptr=kmalloc(sizeof(struct ataraid_bh_private),GFP_NOIO);
		if (!ptr) {
			__set_current_state(TASK_RUNNING);
			yield();
		}
	}
	return ptr;
}

EXPORT_SYMBOL(ataraid_get_private);

void ataraid_end_request(struct buffer_head *bh, int uptodate)
{
	struct ataraid_bh_private *private = bh->b_private;

	if (private==NULL)
		BUG();

	if (atomic_dec_and_test(&private->count)) {
		private->parent->b_end_io(private->parent,uptodate);
		private->parent = NULL;
		kfree(private);
	}
	kfree(bh);
}

EXPORT_SYMBOL(ataraid_end_request);

static void ataraid_split_request(request_queue_t *q, int rw, struct buffer_head * bh)
{
	struct buffer_head *bh1,*bh2;
	struct ataraid_bh_private *private;
	bh1=ataraid_get_bhead();
	bh2=ataraid_get_bhead();

	/* If either of those ever fails we're doomed */
	if ((!bh1)||(!bh2))
		BUG();
	private = ataraid_get_private();
	if (private==NULL)
		BUG();
	
	memcpy(bh1, bh, sizeof(*bh));
	memcpy(bh2, bh, sizeof(*bh));
	
	bh1->b_end_io = ataraid_end_request;
	bh2->b_end_io = ataraid_end_request;

	bh2->b_rsector += bh->b_size >> 10;
	bh1->b_size /= 2;
	bh2->b_size /= 2;
	private->parent = bh;

	bh1->b_private = private;
	bh2->b_private = private;
	atomic_set(&private->count,2);

	bh2->b_data +=  bh->b_size/2;

	generic_make_request(rw,bh1);
	generic_make_request(rw,bh2);
}




/* device register / release functions */


int ataraid_get_device(struct raid_device_operations *fops)
{
	int bit;
	down(&ataraid_sem);
	if (ataraiduse==~0U) {
		up(&ataraid_sem);
		return -ENODEV;
	}
	bit=ffz(ataraiduse); 
	ataraiduse |= 1<<bit;
	ataraid_ops[bit] = fops;
	up(&ataraid_sem);
	return bit;
}

void ataraid_release_device(int device)
{
	down(&ataraid_sem);
	
	if ((ataraiduse & (1<<device))==0)
		BUG();	/* device wasn't registered at all */
	
	ataraiduse &= ~(1<<device);
	ataraid_ops[device] = NULL;
	up(&ataraid_sem);
}

void ataraid_register_disk(int device,long size)
{
	register_disk(&ataraid_gendisk, MKDEV(ATAMAJOR,16*device),16,
		&ataraid_fops,size);

}

static __init int ataraid_init(void) 
{
	int i;
        for(i=0;i<256;i++)
	{
        	ataraid_hardsect_size[i] = 512;
		ataraid_blksize_size[i] = 1024;  
		ataraid_readahead[i] = 1023;
	}
	
	if (blksize_size[ATAMAJOR]==NULL)
		blksize_size[ATAMAJOR] = ataraid_blksize_size;
	if (hardsect_size[ATAMAJOR]==NULL)
		hardsect_size[ATAMAJOR] = ataraid_hardsect_size;
	
	
	/* setup the gendisk structure */	
	ataraid_gendisk.part = kmalloc(256 * sizeof(struct hd_struct),GFP_KERNEL);
	if (ataraid_gendisk.part==NULL) {
		printk(KERN_ERR "ataraid: Couldn't allocate memory, aborting \n");
		return -1;
	}
	
	memset(&ataraid_gendisk.part[0],0,256*sizeof(struct hd_struct));
	
	
	ataraid_gendisk.major       = ATAMAJOR;
	ataraid_gendisk.major_name  = "ataraid";
	ataraid_gendisk.minor_shift = 4;
	ataraid_gendisk.max_p	    = 15;
	ataraid_gendisk.sizes	    = &ataraid_gendisk_sizes[0];
	ataraid_gendisk.nr_real	    = 16;
	ataraid_gendisk.fops        = &ataraid_fops;
	
	
	add_gendisk(&ataraid_gendisk);
			
	if (register_blkdev(ATAMAJOR, "ataraid", &ataraid_fops)) {
		printk(KERN_ERR "ataraid: Could not get major %d \n",ATAMAJOR);
		return -1;
	}
	
	                
	
	blk_queue_make_request(BLK_DEFAULT_QUEUE(ATAMAJOR),ataraid_make_request);
                                                                                     	
	return 0;                                                        	
}


static void __exit ataraid_exit(void)
{
	unregister_blkdev(ATAMAJOR, "ataraid");
	hardsect_size[ATAMAJOR] = NULL;
	blk_size[ATAMAJOR] = NULL;
	blksize_size[ATAMAJOR] = NULL;                       
	max_readahead[ATAMAJOR] = NULL;

	del_gendisk(&ataraid_gendisk);
        
	if (ataraid_gendisk.part) {
		kfree(ataraid_gendisk.part);
		ataraid_gendisk.part = NULL;
	}
}

module_init(ataraid_init);
module_exit(ataraid_exit);



EXPORT_SYMBOL(ataraid_get_device);
EXPORT_SYMBOL(ataraid_release_device);
EXPORT_SYMBOL(ataraid_gendisk);
EXPORT_SYMBOL(ataraid_register_disk);
MODULE_LICENSE("GPL");

