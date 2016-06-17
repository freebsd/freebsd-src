/*
 * $Id: mtdblock_ro.c,v 1.12 2001/11/20 11:42:33 dwmw2 Exp $
 *
 * Read-only version of the mtdblock device, without the 
 * read/erase/modify/writeback stuff
 */

#ifdef MTDBLOCK_DEBUG
#define DEBUGLVL debug
#endif							       


#include <linux/module.h>
#include <linux/types.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/compatmac.h>

#define MAJOR_NR MTD_BLOCK_MAJOR
#define DEVICE_NAME "mtdblock"
#define DEVICE_REQUEST mtdblock_request
#define DEVICE_NR(device) (device)
#define DEVICE_ON(device)
#define DEVICE_OFF(device)
#define DEVICE_NO_RANDOM
#include <linux/blk.h>

#if LINUX_VERSION_CODE < 0x20300
#define RQFUNC_ARG void
#define blkdev_dequeue_request(req) do {CURRENT = req->next;} while (0)
#else
#define RQFUNC_ARG request_queue_t *q
#endif

#ifdef MTDBLOCK_DEBUG
static int debug = MTDBLOCK_DEBUG;
MODULE_PARM(debug, "i");
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,14)
#define BLK_INC_USE_COUNT MOD_INC_USE_COUNT
#define BLK_DEC_USE_COUNT MOD_DEC_USE_COUNT
#else
#define BLK_INC_USE_COUNT do {} while(0)
#define BLK_DEC_USE_COUNT do {} while(0)
#endif

static int mtd_sizes[MAX_MTD_DEVICES];


static int mtdblock_open(struct inode *inode, struct file *file)
{
	struct mtd_info *mtd = NULL;

	int dev;

	DEBUG(1,"mtdblock_open\n");
	
	if (inode == 0)
		return -EINVAL;
	
	dev = MINOR(inode->i_rdev);
	
	mtd = get_mtd_device(NULL, dev);
	if (!mtd)
		return -EINVAL;
	if (MTD_ABSENT == mtd->type) {
		put_mtd_device(mtd);
		return -EINVAL;
	}

	BLK_INC_USE_COUNT;

	mtd_sizes[dev] = mtd->size>>9;

	DEBUG(1, "ok\n");

	return 0;
}

static release_t mtdblock_release(struct inode *inode, struct file *file)
{
	int dev;
	struct mtd_info *mtd;

   	DEBUG(1, "mtdblock_release\n");

	if (inode == NULL)
		release_return(-ENODEV);
   
	dev = MINOR(inode->i_rdev);
	mtd = __get_mtd_device(NULL, dev);

	if (!mtd) {
		printk(KERN_WARNING "MTD device is absent on mtd_release!\n");
		BLK_DEC_USE_COUNT;
		release_return(-ENODEV);
	}
	
	if (mtd->sync)
		mtd->sync(mtd);

	put_mtd_device(mtd);

	DEBUG(1, "ok\n");

	BLK_DEC_USE_COUNT;
	release_return(0);
}  


static void mtdblock_request(RQFUNC_ARG)
{
   struct request *current_request;
   unsigned int res = 0;
   struct mtd_info *mtd;

   while (1)
   {
      /* Grab the Request and unlink it from the request list, INIT_REQUEST
       	 will execute a return if we are done. */
      INIT_REQUEST;
      current_request = CURRENT;
   
      if (MINOR(current_request->rq_dev) >= MAX_MTD_DEVICES)
      {
	 printk("mtd: Unsupported device!\n");
	 end_request(0);
	 continue;
      }
      
      // Grab our MTD structure

      mtd = __get_mtd_device(NULL, MINOR(current_request->rq_dev));
      if (!mtd) {
	      printk("MTD device %d doesn't appear to exist any more\n", CURRENT_DEV);
	      end_request(0);
      }

      if (current_request->sector << 9 > mtd->size ||
	  (current_request->sector + current_request->current_nr_sectors) << 9 > mtd->size)
      {
	 printk("mtd: Attempt to read past end of device!\n");
	 printk("size: %x, sector: %lx, nr_sectors %lx\n", mtd->size, 
		current_request->sector, current_request->current_nr_sectors);
	 end_request(0);
	 continue;
      }
      
      /* Remove the request we are handling from the request list so nobody messes
         with it */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,0)
      /* Now drop the lock that the ll_rw_blk functions grabbed for us
         and process the request. This is necessary due to the extreme time
         we spend processing it. */
      spin_unlock_irq(&io_request_lock);
#endif

      // Handle the request
      switch (current_request->cmd)
      {
         size_t retlen;

	 case READ:
	 if (MTD_READ(mtd,current_request->sector<<9, 
		      current_request->current_nr_sectors << 9, 
		      &retlen, current_request->buffer) == 0)
	    res = 1;
	 else
	    res = 0;
	 break;
	 
	 case WRITE:

	 /* printk("mtdblock_request WRITE sector=%d(%d)\n",current_request->sector,
		current_request->current_nr_sectors);
	 */

	 // Read only device
	 if ((mtd->flags & MTD_CAP_RAM) == 0)
	 {
	    res = 0;
	    break;
	 }

	 // Do the write
	 if (MTD_WRITE(mtd,current_request->sector<<9, 
		       current_request->current_nr_sectors << 9, 
		       &retlen, current_request->buffer) == 0)
	    res = 1;
	 else
	    res = 0;
	 break;
	 
	 // Shouldn't happen
	 default:
	 printk("mtd: unknown request\n");
	 break;
      }

      // Grab the lock and re-thread the item onto the linked list
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,0)
	spin_lock_irq(&io_request_lock);
#endif
	end_request(res);
   }
}



static int mtdblock_ioctl(struct inode * inode, struct file * file,
		      unsigned int cmd, unsigned long arg)
{
	struct mtd_info *mtd;

	mtd = __get_mtd_device(NULL, MINOR(inode->i_rdev));

	if (!mtd) return -EINVAL;

	switch (cmd) {
	case BLKGETSIZE:   /* Return device size */
		return put_user((mtd->size >> 9), (unsigned long *) arg);

#ifdef BLKGETSIZE64
	case BLKGETSIZE64:
		return put_user((u64)mtd->size, (u64 *)arg);
#endif

	case BLKFLSBUF:
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,0)
		if(!capable(CAP_SYS_ADMIN))  return -EACCES;
#endif
		fsync_dev(inode->i_rdev);
		invalidate_buffers(inode->i_rdev);
		if (mtd->sync)
			mtd->sync(mtd);
		return 0;

	default:
		return -ENOTTY;
	}
}

#if LINUX_VERSION_CODE < 0x20326
static struct file_operations mtd_fops =
{
	open: mtdblock_open,
	ioctl: mtdblock_ioctl,
	release: mtdblock_release,
	read: block_read,
	write: block_write
};
#else
static struct block_device_operations mtd_fops = 
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,14)
	owner: THIS_MODULE,
#endif
	open: mtdblock_open,
	release: mtdblock_release,
	ioctl: mtdblock_ioctl
};
#endif

int __init init_mtdblock(void)
{
	int i;

	if (register_blkdev(MAJOR_NR,DEVICE_NAME,&mtd_fops)) {
		printk(KERN_NOTICE "Can't allocate major number %d for Memory Technology Devices.\n",
		       MTD_BLOCK_MAJOR);
		return -EAGAIN;
	}
	
	/* We fill it in at open() time. */
	for (i=0; i< MAX_MTD_DEVICES; i++) {
		mtd_sizes[i] = 0;
	}
	
	/* Allow the block size to default to BLOCK_SIZE. */
	blksize_size[MAJOR_NR] = NULL;
	blk_size[MAJOR_NR] = mtd_sizes;
	
	blk_init_queue(BLK_DEFAULT_QUEUE(MAJOR_NR), &mtdblock_request);
	return 0;
}

static void __exit cleanup_mtdblock(void)
{
	unregister_blkdev(MAJOR_NR,DEVICE_NAME);
	blk_size[MAJOR_NR] = NULL;
	blk_cleanup_queue(BLK_DEFAULT_QUEUE(MAJOR_NR));
}

module_init(init_mtdblock);
module_exit(cleanup_mtdblock);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Erwin Authried <eauth@softsys.co.at> et al.");
MODULE_DESCRIPTION("Simple read-only block device emulation access to MTD devices");
