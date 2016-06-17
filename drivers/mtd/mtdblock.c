/* 
 * Direct MTD block device access
 *
 * $Id: mtdblock.c,v 1.51 2001/11/20 11:42:33 dwmw2 Exp $
 *
 * 02-nov-2000	Nicolas Pitre		Added read-modify-write with cache
 */

#include <linux/config.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
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
/* for old kernels... */
#ifndef QUEUE_EMPTY
#define QUEUE_EMPTY  (!CURRENT)
#endif
#if LINUX_VERSION_CODE < 0x20300
#define QUEUE_PLUGGED (blk_dev[MAJOR_NR].plug_tq.sync)
#else
#define QUEUE_PLUGGED (blk_dev[MAJOR_NR].request_queue.plugged)
#endif

#ifdef CONFIG_DEVFS_FS
#include <linux/devfs_fs_kernel.h>
static void mtd_notify_add(struct mtd_info* mtd);
static void mtd_notify_remove(struct mtd_info* mtd);
static struct mtd_notifier notifier = {
        mtd_notify_add,
        mtd_notify_remove,
        NULL
};
static devfs_handle_t devfs_dir_handle = NULL;
static devfs_handle_t devfs_rw_handle[MAX_MTD_DEVICES];
#endif

static struct mtdblk_dev {
	struct mtd_info *mtd; /* Locked */
	int count;
	struct semaphore cache_sem;
	unsigned char *cache_data;
	unsigned long cache_offset;
	unsigned int cache_size;
	enum { STATE_EMPTY, STATE_CLEAN, STATE_DIRTY } cache_state;
} *mtdblks[MAX_MTD_DEVICES];

static spinlock_t mtdblks_lock;

static int mtd_sizes[MAX_MTD_DEVICES];
static int mtd_blksizes[MAX_MTD_DEVICES];

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,14)
#define BLK_INC_USE_COUNT MOD_INC_USE_COUNT
#define BLK_DEC_USE_COUNT MOD_DEC_USE_COUNT
#else
#define BLK_INC_USE_COUNT do {} while(0)
#define BLK_DEC_USE_COUNT do {} while(0)
#endif

/*
 * Cache stuff...
 * 
 * Since typical flash erasable sectors are much larger than what Linux's
 * buffer cache can handle, we must implement read-modify-write on flash
 * sectors for each block write requests.  To avoid over-erasing flash sectors
 * and to speed things up, we locally cache a whole flash sector while it is
 * being written to until a different sector is required.
 */

static void erase_callback(struct erase_info *done)
{
	wait_queue_head_t *wait_q = (wait_queue_head_t *)done->priv;
	wake_up(wait_q);
}

static int erase_write (struct mtd_info *mtd, unsigned long pos, 
			int len, const char *buf)
{
	struct erase_info erase;
	DECLARE_WAITQUEUE(wait, current);
	wait_queue_head_t wait_q;
	size_t retlen;
	int ret;

	/*
	 * First, let's erase the flash block.
	 */

	init_waitqueue_head(&wait_q);
	erase.mtd = mtd;
	erase.callback = erase_callback;
	erase.addr = pos;
	erase.len = len;
	erase.priv = (u_long)&wait_q;

	set_current_state(TASK_INTERRUPTIBLE);
	add_wait_queue(&wait_q, &wait);

	ret = MTD_ERASE(mtd, &erase);
	if (ret) {
		set_current_state(TASK_RUNNING);
		remove_wait_queue(&wait_q, &wait);
		printk (KERN_WARNING "mtdblock: erase of region [0x%lx, 0x%x] "
				     "on \"%s\" failed\n",
			pos, len, mtd->name);
		return ret;
	}

	schedule();  /* Wait for erase to finish. */
	remove_wait_queue(&wait_q, &wait);

	/*
	 * Next, writhe data to flash.
	 */

	ret = MTD_WRITE (mtd, pos, len, &retlen, buf);
	if (ret)
		return ret;
	if (retlen != len)
		return -EIO;
	return 0;
}


static int write_cached_data (struct mtdblk_dev *mtdblk)
{
	struct mtd_info *mtd = mtdblk->mtd;
	int ret;

	if (mtdblk->cache_state != STATE_DIRTY)
		return 0;

	DEBUG(MTD_DEBUG_LEVEL2, "mtdblock: writing cached data for \"%s\" "
			"at 0x%lx, size 0x%x\n", mtd->name, 
			mtdblk->cache_offset, mtdblk->cache_size);
	
	ret = erase_write (mtd, mtdblk->cache_offset, 
			   mtdblk->cache_size, mtdblk->cache_data);
	if (ret)
		return ret;

	/*
	 * Here we could argably set the cache state to STATE_CLEAN.
	 * However this could lead to inconsistency since we will not 
	 * be notified if this content is altered on the flash by other 
	 * means.  Let's declare it empty and leave buffering tasks to
	 * the buffer cache instead.
	 */
	mtdblk->cache_state = STATE_EMPTY;
	return 0;
}


static int do_cached_write (struct mtdblk_dev *mtdblk, unsigned long pos, 
			    int len, const char *buf)
{
	struct mtd_info *mtd = mtdblk->mtd;
	unsigned int sect_size = mtdblk->cache_size;
	size_t retlen;
	int ret;

	DEBUG(MTD_DEBUG_LEVEL2, "mtdblock: write on \"%s\" at 0x%lx, size 0x%x\n",
		mtd->name, pos, len);
	
	if (!sect_size)
		return MTD_WRITE (mtd, pos, len, &retlen, buf);

	while (len > 0) {
		unsigned long sect_start = (pos/sect_size)*sect_size;
		unsigned int offset = pos - sect_start;
		unsigned int size = sect_size - offset;
		if( size > len ) 
			size = len;

		if (size == sect_size) {
			/* 
			 * We are covering a whole sector.  Thus there is no
			 * need to bother with the cache while it may still be
			 * useful for other partial writes.
			 */
			ret = erase_write (mtd, pos, size, buf);
			if (ret)
				return ret;
		} else {
			/* Partial sector: need to use the cache */

			if (mtdblk->cache_state == STATE_DIRTY &&
			    mtdblk->cache_offset != sect_start) {
				ret = write_cached_data(mtdblk);
				if (ret) 
					return ret;
			}

			if (mtdblk->cache_state == STATE_EMPTY ||
			    mtdblk->cache_offset != sect_start) {
				/* fill the cache with the current sector */
				mtdblk->cache_state = STATE_EMPTY;
				ret = MTD_READ(mtd, sect_start, sect_size, &retlen, mtdblk->cache_data);
				if (ret)
					return ret;
				if (retlen != sect_size)
					return -EIO;

				mtdblk->cache_offset = sect_start;
				mtdblk->cache_size = sect_size;
				mtdblk->cache_state = STATE_CLEAN;
			}

			/* write data to our local cache */
			memcpy (mtdblk->cache_data + offset, buf, size);
			mtdblk->cache_state = STATE_DIRTY;
		}

		buf += size;
		pos += size;
		len -= size;
	}

	return 0;
}


static int do_cached_read (struct mtdblk_dev *mtdblk, unsigned long pos, 
			   int len, char *buf)
{
	struct mtd_info *mtd = mtdblk->mtd;
	unsigned int sect_size = mtdblk->cache_size;
	size_t retlen;
	int ret;

	DEBUG(MTD_DEBUG_LEVEL2, "mtdblock: read on \"%s\" at 0x%lx, size 0x%x\n", 
			mtd->name, pos, len);
	
	if (!sect_size)
		return MTD_READ (mtd, pos, len, &retlen, buf);

	while (len > 0) {
		unsigned long sect_start = (pos/sect_size)*sect_size;
		unsigned int offset = pos - sect_start;
		unsigned int size = sect_size - offset;
		if (size > len) 
			size = len;

		/*
		 * Check if the requested data is already cached
		 * Read the requested amount of data from our internal cache if it
		 * contains what we want, otherwise we read the data directly
		 * from flash.
		 */
		if (mtdblk->cache_state != STATE_EMPTY &&
		    mtdblk->cache_offset == sect_start) {
			memcpy (buf, mtdblk->cache_data + offset, size);
		} else {
			ret = MTD_READ (mtd, pos, size, &retlen, buf);
			if (ret)
				return ret;
			if (retlen != size)
				return -EIO;
		}

		buf += size;
		pos += size;
		len -= size;
	}

	return 0;
}



static int mtdblock_open(struct inode *inode, struct file *file)
{
	struct mtdblk_dev *mtdblk;
	struct mtd_info *mtd;
	int dev;

	DEBUG(MTD_DEBUG_LEVEL1,"mtdblock_open\n");
	
	if (!inode)
		return -EINVAL;
	
	dev = MINOR(inode->i_rdev);
	if (dev >= MAX_MTD_DEVICES)
		return -EINVAL;

	BLK_INC_USE_COUNT;

	mtd = get_mtd_device(NULL, dev);
	if (!mtd)
		return -ENODEV;
	if (MTD_ABSENT == mtd->type) {
		put_mtd_device(mtd);
		BLK_DEC_USE_COUNT;
		return -ENODEV;
	}
	
	spin_lock(&mtdblks_lock);

	/* If it's already open, no need to piss about. */
	if (mtdblks[dev]) {
		mtdblks[dev]->count++;
		spin_unlock(&mtdblks_lock);
		put_mtd_device(mtd);
		return 0;
	}
	
	/* OK, it's not open. Try to find it */

	/* First we have to drop the lock, because we have to
	   to things which might sleep.
	*/
	spin_unlock(&mtdblks_lock);

	mtdblk = kmalloc(sizeof(struct mtdblk_dev), GFP_KERNEL);
	if (!mtdblk) {
		put_mtd_device(mtd);
		BLK_DEC_USE_COUNT;
		return -ENOMEM;
	}
	memset(mtdblk, 0, sizeof(*mtdblk));
	mtdblk->count = 1;
	mtdblk->mtd = mtd;

	init_MUTEX (&mtdblk->cache_sem);
	mtdblk->cache_state = STATE_EMPTY;
	if ((mtdblk->mtd->flags & MTD_CAP_RAM) != MTD_CAP_RAM &&
	    mtdblk->mtd->erasesize) {
		mtdblk->cache_size = mtdblk->mtd->erasesize;
		mtdblk->cache_data = vmalloc(mtdblk->mtd->erasesize);
		if (!mtdblk->cache_data) {
			put_mtd_device(mtdblk->mtd);
			kfree(mtdblk);
			BLK_DEC_USE_COUNT;
			return -ENOMEM;
		}
	}

	/* OK, we've created a new one. Add it to the list. */

	spin_lock(&mtdblks_lock);

	if (mtdblks[dev]) {
		/* Another CPU made one at the same time as us. */
		mtdblks[dev]->count++;
		spin_unlock(&mtdblks_lock);
		put_mtd_device(mtdblk->mtd);
		vfree(mtdblk->cache_data);
		kfree(mtdblk);
		return 0;
	}

	mtdblks[dev] = mtdblk;
	mtd_sizes[dev] = mtdblk->mtd->size/1024;
	if (mtdblk->mtd->erasesize)
		mtd_blksizes[dev] = mtdblk->mtd->erasesize;
	if (mtd_blksizes[dev] > PAGE_SIZE)
		mtd_blksizes[dev] = PAGE_SIZE;
	set_device_ro (inode->i_rdev, !(mtdblk->mtd->flags & MTD_WRITEABLE));
	
	spin_unlock(&mtdblks_lock);
	
	DEBUG(MTD_DEBUG_LEVEL1, "ok\n");

	return 0;
}

static release_t mtdblock_release(struct inode *inode, struct file *file)
{
	int dev;
	struct mtdblk_dev *mtdblk;
   	DEBUG(MTD_DEBUG_LEVEL1, "mtdblock_release\n");

	if (inode == NULL)
		release_return(-ENODEV);

	dev = MINOR(inode->i_rdev);
	mtdblk = mtdblks[dev];

	down(&mtdblk->cache_sem);
	write_cached_data(mtdblk);
	up(&mtdblk->cache_sem);

	spin_lock(&mtdblks_lock);
	if (!--mtdblk->count) {
		/* It was the last usage. Free the device */
		mtdblks[dev] = NULL;
		spin_unlock(&mtdblks_lock);
		if (mtdblk->mtd->sync)
			mtdblk->mtd->sync(mtdblk->mtd);
		put_mtd_device(mtdblk->mtd);
		vfree(mtdblk->cache_data);
		kfree(mtdblk);
	} else {
		spin_unlock(&mtdblks_lock);
	}

	DEBUG(MTD_DEBUG_LEVEL1, "ok\n");

	BLK_DEC_USE_COUNT;
	release_return(0);
}  


/* 
 * This is a special request_fn because it is executed in a process context 
 * to be able to sleep independently of the caller.  The io_request_lock 
 * is held upon entry and exit.
 * The head of our request queue is considered active so there is no need 
 * to dequeue requests before we are done.
 */
static void handle_mtdblock_request(void)
{
	struct request *req;
	struct mtdblk_dev *mtdblk;
	unsigned int res;

	for (;;) {
		INIT_REQUEST;
		req = CURRENT;
		spin_unlock_irq(&io_request_lock);
		mtdblk = mtdblks[MINOR(req->rq_dev)];
		res = 0;

		if (MINOR(req->rq_dev) >= MAX_MTD_DEVICES)
			panic("%s: minor out of bounds", __FUNCTION__);

		if ((req->sector + req->current_nr_sectors) > (mtdblk->mtd->size >> 9))
			goto end_req;

		// Handle the request
		switch (req->cmd)
		{
			int err;

			case READ:
			down(&mtdblk->cache_sem);
			err = do_cached_read (mtdblk, req->sector << 9, 
					req->current_nr_sectors << 9,
					req->buffer);
			up(&mtdblk->cache_sem);
			if (!err)
				res = 1;
			break;

			case WRITE:
			// Read only device
			if ( !(mtdblk->mtd->flags & MTD_WRITEABLE) ) 
				break;

			// Do the write
			down(&mtdblk->cache_sem);
			err = do_cached_write (mtdblk, req->sector << 9,
					req->current_nr_sectors << 9, 
					req->buffer);
			up(&mtdblk->cache_sem);
			if (!err)
				res = 1;
			break;
		}

end_req:
		spin_lock_irq(&io_request_lock);
		end_request(res);
	}
}

static volatile int leaving = 0;
static DECLARE_MUTEX_LOCKED(thread_sem);
static DECLARE_WAIT_QUEUE_HEAD(thr_wq);

int mtdblock_thread(void *dummy)
{
	struct task_struct *tsk = current;
	DECLARE_WAITQUEUE(wait, tsk);

	/* we might get involved when memory gets low, so use PF_MEMALLOC */
	tsk->flags |= PF_MEMALLOC;
	strcpy(tsk->comm, "mtdblockd");
	spin_lock_irq(&tsk->sigmask_lock);
	sigfillset(&tsk->blocked);
	recalc_sigpending(tsk);
	spin_unlock_irq(&tsk->sigmask_lock);
	daemonize();

	while (!leaving) {
		add_wait_queue(&thr_wq, &wait);
		set_current_state(TASK_INTERRUPTIBLE);
		spin_lock_irq(&io_request_lock);
		if (QUEUE_EMPTY || QUEUE_PLUGGED) {
			spin_unlock_irq(&io_request_lock);
			schedule();
			remove_wait_queue(&thr_wq, &wait); 
		} else {
			remove_wait_queue(&thr_wq, &wait); 
			set_current_state(TASK_RUNNING);
			handle_mtdblock_request();
			spin_unlock_irq(&io_request_lock);
		}
	}

	up(&thread_sem);
	return 0;
}

#if LINUX_VERSION_CODE < 0x20300
#define RQFUNC_ARG void
#else
#define RQFUNC_ARG request_queue_t *q
#endif

static void mtdblock_request(RQFUNC_ARG)
{
	/* Don't do anything, except wake the thread if necessary */
	wake_up(&thr_wq);
}


static int mtdblock_ioctl(struct inode * inode, struct file * file,
		      unsigned int cmd, unsigned long arg)
{
	struct mtdblk_dev *mtdblk;

	mtdblk = mtdblks[MINOR(inode->i_rdev)];

#ifdef PARANOIA
	if (!mtdblk)
		BUG();
#endif

	switch (cmd) {
	case BLKGETSIZE:   /* Return device size */
		return put_user((mtdblk->mtd->size >> 9), (unsigned long *) arg);

#ifdef BLKGETSIZE64
	case BLKGETSIZE64:
		return put_user((u64)mtdblk->mtd->size, (u64 *)arg);
#endif
		
	case BLKFLSBUF:
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,0)
		if(!capable(CAP_SYS_ADMIN))
			return -EACCES;
#endif
		fsync_dev(inode->i_rdev);
		invalidate_buffers(inode->i_rdev);
		down(&mtdblk->cache_sem);
		write_cached_data(mtdblk);
		up(&mtdblk->cache_sem);
		if (mtdblk->mtd->sync)
			mtdblk->mtd->sync(mtdblk->mtd);
		return 0;

	default:
		return -EINVAL;
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

#ifdef CONFIG_DEVFS_FS
/* Notification that a new device has been added. Create the devfs entry for
 * it. */

static void mtd_notify_add(struct mtd_info* mtd)
{
        char name[8];

        if (!mtd || mtd->type == MTD_ABSENT)
                return;

        sprintf(name, "%d", mtd->index);
        devfs_rw_handle[mtd->index] = devfs_register(devfs_dir_handle, name,
                        DEVFS_FL_DEFAULT, MTD_BLOCK_MAJOR, mtd->index,
                        S_IFBLK | S_IRUGO | S_IWUGO,
                        &mtd_fops, NULL);
}

static void mtd_notify_remove(struct mtd_info* mtd)
{
        if (!mtd || mtd->type == MTD_ABSENT)
                return;

        devfs_unregister(devfs_rw_handle[mtd->index]);
}
#endif

int __init init_mtdblock(void)
{
	int i;

	spin_lock_init(&mtdblks_lock);
#ifdef CONFIG_DEVFS_FS
	if (devfs_register_blkdev(MTD_BLOCK_MAJOR, DEVICE_NAME, &mtd_fops))
	{
		printk(KERN_NOTICE "Can't allocate major number %d for Memory Technology Devices.\n",
			MTD_BLOCK_MAJOR);
		return -EAGAIN;
	}

	devfs_dir_handle = devfs_mk_dir(NULL, DEVICE_NAME, NULL);
	register_mtd_user(&notifier);
#else
	if (register_blkdev(MAJOR_NR,DEVICE_NAME,&mtd_fops)) {
		printk(KERN_NOTICE "Can't allocate major number %d for Memory Technology Devices.\n",
		       MTD_BLOCK_MAJOR);
		return -EAGAIN;
	}
#endif
	
	/* We fill it in at open() time. */
	for (i=0; i< MAX_MTD_DEVICES; i++) {
		mtd_sizes[i] = 0;
		mtd_blksizes[i] = BLOCK_SIZE;
	}
	init_waitqueue_head(&thr_wq);
	/* Allow the block size to default to BLOCK_SIZE. */
	blksize_size[MAJOR_NR] = mtd_blksizes;
	blk_size[MAJOR_NR] = mtd_sizes;
	
	blk_init_queue(BLK_DEFAULT_QUEUE(MAJOR_NR), &mtdblock_request);
	kernel_thread (mtdblock_thread, NULL, CLONE_FS|CLONE_FILES|CLONE_SIGHAND);
	return 0;
}

static void __exit cleanup_mtdblock(void)
{
	leaving = 1;
	wake_up(&thr_wq);
	down(&thread_sem);
#ifdef CONFIG_DEVFS_FS
	unregister_mtd_user(&notifier);
	devfs_unregister(devfs_dir_handle);
	devfs_unregister_blkdev(MTD_BLOCK_MAJOR, DEVICE_NAME);
#else
	unregister_blkdev(MAJOR_NR,DEVICE_NAME);
#endif
	blk_cleanup_queue(BLK_DEFAULT_QUEUE(MAJOR_NR));
	blksize_size[MAJOR_NR] = NULL;
	blk_size[MAJOR_NR] = NULL;
}

module_init(init_mtdblock);
module_exit(cleanup_mtdblock);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nicolas Pitre <nico@cam.org> et al.");
MODULE_DESCRIPTION("Caching read/erase/writeback block device emulation access to MTD devices");
