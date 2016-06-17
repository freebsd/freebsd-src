/*
 * $Id: mtdchar.c,v 1.49 2003/01/24 12:02:58 dwmw2 Exp $
 *
 * Character-device access to raw MTD devices.
 * Pure 2.4 version - compatibility cruft removed to mtdchar-compat.c
 *
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mtd/mtd.h>
#include <linux/slab.h>

#ifdef CONFIG_DEVFS_FS
#include <linux/devfs_fs_kernel.h>
static void mtd_notify_add(struct mtd_info* mtd);
static void mtd_notify_remove(struct mtd_info* mtd);

static struct mtd_notifier notifier = {
	add:	mtd_notify_add,
	remove:	mtd_notify_remove,
};

static devfs_handle_t devfs_dir_handle;
static devfs_handle_t devfs_rw_handle[MAX_MTD_DEVICES];
static devfs_handle_t devfs_ro_handle[MAX_MTD_DEVICES];
#endif

static loff_t mtd_lseek (struct file *file, loff_t offset, int orig)
{
	struct mtd_info *mtd=(struct mtd_info *)file->private_data;

	switch (orig) {
	case 0:
		/* SEEK_SET */
		file->f_pos = offset;
		break;
	case 1:
		/* SEEK_CUR */
		file->f_pos += offset;
		break;
	case 2:
		/* SEEK_END */
		file->f_pos =mtd->size + offset;
		break;
	default:
		return -EINVAL;
	}

	if (file->f_pos < 0)
		file->f_pos = 0;
	else if (file->f_pos >= mtd->size)
		file->f_pos = mtd->size - 1;

	return file->f_pos;
}



static int mtd_open(struct inode *inode, struct file *file)
{
	int minor = minor(inode->i_rdev);
	int devnum = minor >> 1;
	struct mtd_info *mtd;

	DEBUG(MTD_DEBUG_LEVEL0, "MTD_open\n");

	if (devnum >= MAX_MTD_DEVICES)
		return -ENODEV;

	/* You can't open the RO devices RW */
	if ((file->f_mode & 2) && (minor & 1))
		return -EACCES;

	mtd = get_mtd_device(NULL, devnum);
	
	if (!mtd)
		return -ENODEV;
	
	if (MTD_ABSENT == mtd->type) {
		put_mtd_device(mtd);
		return -ENODEV;
	}

	file->private_data = mtd;
		
	/* You can't open it RW if it's not a writeable device */
	if ((file->f_mode & 2) && !(mtd->flags & MTD_WRITEABLE)) {
		put_mtd_device(mtd);
		return -EACCES;
	}
		
	return 0;
} /* mtd_open */

/*====================================================================*/

static int mtd_close(struct inode *inode, struct file *file)
{
	struct mtd_info *mtd;

	DEBUG(MTD_DEBUG_LEVEL0, "MTD_close\n");

	mtd = (struct mtd_info *)file->private_data;
	
	if (mtd->sync)
		mtd->sync(mtd);
	
	put_mtd_device(mtd);

	return 0;
} /* mtd_close */

/* FIXME: This _really_ needs to die. In 2.5, we should lock the
   userspace buffer down and use it directly with readv/writev.
*/
#define MAX_KMALLOC_SIZE 0x20000

static ssize_t mtd_read(struct file *file, char *buf, size_t count,loff_t *ppos)
{
	struct mtd_info *mtd = (struct mtd_info *)file->private_data;
	size_t retlen=0;
	size_t total_retlen=0;
	int ret=0;
	int len;
	char *kbuf;
	
	DEBUG(MTD_DEBUG_LEVEL0,"MTD_read\n");

	if (*ppos + count > mtd->size)
		count = mtd->size - *ppos;

	if (!count)
		return 0;
	
	/* FIXME: Use kiovec in 2.5 to lock down the user's buffers
	   and pass them directly to the MTD functions */
	while (count) {
		if (count > MAX_KMALLOC_SIZE) 
			len = MAX_KMALLOC_SIZE;
		else
			len = count;

		kbuf=kmalloc(len,GFP_KERNEL);
		if (!kbuf)
			return -ENOMEM;
		
		ret = MTD_READ(mtd, *ppos, len, &retlen, kbuf);
		if (!ret) {
			*ppos += retlen;
			if (copy_to_user(buf, kbuf, retlen)) {
			        kfree(kbuf);
				return -EFAULT;
			}
			else
				total_retlen += retlen;

			count -= retlen;
			buf += retlen;
		}
		else {
			kfree(kbuf);
			return ret;
		}
		
		kfree(kbuf);
	}
	
	return total_retlen;
} /* mtd_read */

static ssize_t mtd_write(struct file *file, const char *buf, size_t count,loff_t *ppos)
{
	struct mtd_info *mtd = (struct mtd_info *)file->private_data;
	char *kbuf;
	size_t retlen;
	size_t total_retlen=0;
	int ret=0;
	int len;

	DEBUG(MTD_DEBUG_LEVEL0,"MTD_write\n");
	
	if (*ppos == mtd->size)
		return -ENOSPC;
	
	if (*ppos + count > mtd->size)
		count = mtd->size - *ppos;

	if (!count)
		return 0;

	while (count) {
		if (count > MAX_KMALLOC_SIZE) 
			len = MAX_KMALLOC_SIZE;
		else
			len = count;

		kbuf=kmalloc(len,GFP_KERNEL);
		if (!kbuf) {
			printk("kmalloc is null\n");
			return -ENOMEM;
		}

		if (copy_from_user(kbuf, buf, len)) {
			kfree(kbuf);
			return -EFAULT;
		}
		
	        ret = (*(mtd->write))(mtd, *ppos, len, &retlen, kbuf);
		if (!ret) {
			*ppos += retlen;
			total_retlen += retlen;
			count -= retlen;
			buf += retlen;
		}
		else {
			kfree(kbuf);
			return ret;
		}
		
		kfree(kbuf);
	}

	return total_retlen;
} /* mtd_write */

/*======================================================================

    IOCTL calls for getting device parameters.

======================================================================*/
static void mtd_erase_callback (struct erase_info *instr)
{
	wake_up((wait_queue_head_t *)instr->priv);
}

static int mtd_ioctl(struct inode *inode, struct file *file,
		     u_int cmd, u_long arg)
{
	struct mtd_info *mtd = (struct mtd_info *)file->private_data;
	int ret = 0;
	u_long size;
	
	DEBUG(MTD_DEBUG_LEVEL0, "MTD_ioctl\n");

	size = (cmd & IOCSIZE_MASK) >> IOCSIZE_SHIFT;
	if (cmd & IOC_IN) {
		ret = verify_area(VERIFY_READ, (char *)arg, size);
		if (ret) return ret;
	}
	if (cmd & IOC_OUT) {
		ret = verify_area(VERIFY_WRITE, (char *)arg, size);
		if (ret) return ret;
	}
	
	switch (cmd) {
	case MEMGETREGIONCOUNT:
		if (copy_to_user((int *) arg, &(mtd->numeraseregions), sizeof(int)))
			return -EFAULT;
		break;

	case MEMGETREGIONINFO:
	{
		struct region_info_user ur;

		if (copy_from_user(	&ur, 
					(struct region_info_user *)arg, 
					sizeof(struct region_info_user))) {
			return -EFAULT;
		}

		if (ur.regionindex >= mtd->numeraseregions)
			return -EINVAL;
		if (copy_to_user((struct mtd_erase_region_info *) arg, 
				&(mtd->eraseregions[ur.regionindex]),
				sizeof(struct mtd_erase_region_info)))
			return -EFAULT;
		break;
	}

	case MEMGETINFO:
		if (copy_to_user((struct mtd_info *)arg, mtd,
				 sizeof(struct mtd_info_user)))
			return -EFAULT;
		break;

	case MEMERASE:
	{
		struct erase_info *erase;

		if(!(file->f_mode & 2))
			return -EPERM;

		erase=kmalloc(sizeof(struct erase_info),GFP_KERNEL);
		if (!erase)
			ret = -ENOMEM;
		else {
			wait_queue_head_t waitq;
			DECLARE_WAITQUEUE(wait, current);

			init_waitqueue_head(&waitq);

			memset (erase,0,sizeof(struct erase_info));
			if (copy_from_user(&erase->addr, (u_long *)arg,
					   2 * sizeof(u_long))) {
				kfree(erase);
				return -EFAULT;
			}
			erase->mtd = mtd;
			erase->callback = mtd_erase_callback;
			erase->priv = (unsigned long)&waitq;
			
			/*
			  FIXME: Allow INTERRUPTIBLE. Which means
			  not having the wait_queue head on the stack.
			  
			  If the wq_head is on the stack, and we
			  leave because we got interrupted, then the
			  wq_head is no longer there when the
			  callback routine tries to wake us up.
			*/
			ret = mtd->erase(mtd, erase);
			if (!ret) {
				set_current_state(TASK_UNINTERRUPTIBLE);
				add_wait_queue(&waitq, &wait);
				if (erase->state != MTD_ERASE_DONE &&
				    erase->state != MTD_ERASE_FAILED)
					schedule();
				remove_wait_queue(&waitq, &wait);
				set_current_state(TASK_RUNNING);

				ret = (erase->state == MTD_ERASE_FAILED)?-EIO:0;
			}
			kfree(erase);
		}
		break;
	}

	case MEMWRITEOOB:
	{
		struct mtd_oob_buf buf;
		void *databuf;
		ssize_t retlen;
		
		if(!(file->f_mode & 2))
			return -EPERM;

		if (copy_from_user(&buf, (struct mtd_oob_buf *)arg, sizeof(struct mtd_oob_buf)))
			return -EFAULT;
		
		if (buf.length > 0x4096)
			return -EINVAL;

		if (!mtd->write_oob)
			ret = -EOPNOTSUPP;
		else
			ret = verify_area(VERIFY_READ, (char *)buf.ptr, buf.length);

		if (ret)
			return ret;

		databuf = kmalloc(buf.length, GFP_KERNEL);
		if (!databuf)
			return -ENOMEM;
		
		if (copy_from_user(databuf, buf.ptr, buf.length)) {
			kfree(databuf);
			return -EFAULT;
		}

		ret = (mtd->write_oob)(mtd, buf.start, buf.length, &retlen, databuf);

		if (copy_to_user((void *)arg + sizeof(u_int32_t), &retlen, sizeof(u_int32_t)))
			ret = -EFAULT;

		kfree(databuf);
		break;

	}

	case MEMREADOOB:
	{
		struct mtd_oob_buf buf;
		void *databuf;
		ssize_t retlen;

		if (copy_from_user(&buf, (struct mtd_oob_buf *)arg, sizeof(struct mtd_oob_buf)))
			return -EFAULT;
		
		if (buf.length > 0x4096)
			return -EINVAL;

		if (!mtd->read_oob)
			ret = -EOPNOTSUPP;
		else
			ret = verify_area(VERIFY_WRITE, (char *)buf.ptr, buf.length);

		if (ret)
			return ret;

		databuf = kmalloc(buf.length, GFP_KERNEL);
		if (!databuf)
			return -ENOMEM;
		
		ret = (mtd->read_oob)(mtd, buf.start, buf.length, &retlen, databuf);

		if (copy_to_user((void *)arg + sizeof(u_int32_t), &retlen, sizeof(u_int32_t)))
			ret = -EFAULT;
		else if (retlen && copy_to_user(buf.ptr, databuf, retlen))
			ret = -EFAULT;
		
		kfree(databuf);
		break;
	}

	case MEMLOCK:
	{
		unsigned long adrs[2];

		if (copy_from_user(adrs ,(void *)arg, 2* sizeof(unsigned long)))
			return -EFAULT;

		if (!mtd->lock)
			ret = -EOPNOTSUPP;
		else
			ret = mtd->lock(mtd, adrs[0], adrs[1]);
		break;
	}

	case MEMUNLOCK:
	{
		unsigned long adrs[2];

		if (copy_from_user(adrs, (void *)arg, 2* sizeof(unsigned long)))
			return -EFAULT;

		if (!mtd->unlock)
			ret = -EOPNOTSUPP;
		else
			ret = mtd->unlock(mtd, adrs[0], adrs[1]);
		break;
	}

	case MEMWRITEDATA:
	{
		struct mtd_oob_buf buf;
		void *databuf;
		ssize_t retlen;
		
		if (copy_from_user(&buf, (struct mtd_oob_buf *)arg, sizeof(struct mtd_oob_buf)))
			return -EFAULT;
		
		if (buf.length > 0x4096)
			return -EINVAL;

		if (!mtd->write_ecc)
			ret = -EOPNOTSUPP;
		else
			ret = verify_area(VERIFY_READ, (char *)buf.ptr, buf.length);

		if (ret)
			return ret;

		databuf = kmalloc(buf.length, GFP_KERNEL);
		if (!databuf)
			return -ENOMEM;
		
		if (copy_from_user(databuf, buf.ptr, buf.length)) {
			kfree(databuf);
			return -EFAULT;
		}

		ret = (mtd->write_ecc)(mtd, buf.start, buf.length, &retlen, databuf, NULL, 0);

		if (copy_to_user((void *)arg + sizeof(u_int32_t), &retlen, sizeof(u_int32_t)))
			ret = -EFAULT;

		kfree(databuf);
		break;

	}

	case MEMREADDATA:
	{
		struct mtd_oob_buf buf;
		void *databuf;
		ssize_t retlen = 0;

		if (copy_from_user(&buf, (struct mtd_oob_buf *)arg, sizeof(struct mtd_oob_buf)))
			return -EFAULT;
		
		if (buf.length > 0x4096)
			return -EINVAL;

		if (!mtd->read_ecc)
			ret = -EOPNOTSUPP;
		else
			ret = verify_area(VERIFY_WRITE, (char *)buf.ptr, buf.length);

		if (ret)
			return ret;

		databuf = kmalloc(buf.length, GFP_KERNEL);
		if (!databuf)
			return -ENOMEM;
		
		ret = (mtd->read_ecc)(mtd, buf.start, buf.length, &retlen, databuf, NULL, 0);

		if (copy_to_user((void *)arg + sizeof(u_int32_t), &retlen, sizeof(u_int32_t)))
			ret = -EFAULT;
		else if (retlen && copy_to_user(buf.ptr, databuf, retlen))
			ret = -EFAULT;
		
		kfree(databuf);
		break;
	}

		
	default:
		DEBUG(MTD_DEBUG_LEVEL0, "Invalid ioctl %x (MEMGETINFO = %x)\n", cmd, MEMGETINFO);
		ret = -ENOTTY;
	}

	return ret;
} /* memory_ioctl */

static struct file_operations mtd_fops = {
	owner:		THIS_MODULE,
	llseek:		mtd_lseek,     	/* lseek */
	read:		mtd_read,	/* read */
	write: 		mtd_write, 	/* write */
	ioctl:		mtd_ioctl,	/* ioctl */
	open:		mtd_open,	/* open */
	release:	mtd_close,	/* release */
};


#ifdef CONFIG_DEVFS_FS
/* Notification that a new device has been added. Create the devfs entry for
 * it. */

static void mtd_notify_add(struct mtd_info* mtd)
{
	char name[8];

	if (!mtd)
		return;

	sprintf(name, "%d", mtd->index);
	devfs_rw_handle[mtd->index] = devfs_register(devfs_dir_handle, name,
			DEVFS_FL_DEFAULT, MTD_CHAR_MAJOR, mtd->index*2,
			S_IFCHR | S_IRUGO | S_IWUGO,
			&mtd_fops, NULL);

	sprintf(name, "%dro", mtd->index);
	devfs_ro_handle[mtd->index] = devfs_register(devfs_dir_handle, name,
			DEVFS_FL_DEFAULT, MTD_CHAR_MAJOR, mtd->index*2+1,
			S_IFCHR | S_IRUGO,
			&mtd_fops, NULL);
}

static void mtd_notify_remove(struct mtd_info* mtd)
{
	if (!mtd)
		return;

	devfs_unregister(devfs_rw_handle[mtd->index]);
	devfs_unregister(devfs_ro_handle[mtd->index]);
}
#endif

static int __init init_mtdchar(void)
{
#ifdef CONFIG_DEVFS_FS
	if (devfs_register_chrdev(MTD_CHAR_MAJOR, "mtd", &mtd_fops))
	{
		printk(KERN_NOTICE "Can't allocate major number %d for Memory Technology Devices.\n",
		       MTD_CHAR_MAJOR);
		return -EAGAIN;
	}

	devfs_dir_handle = devfs_mk_dir(NULL, "mtd", NULL);

	register_mtd_user(&notifier);
#else
	if (register_chrdev(MTD_CHAR_MAJOR, "mtd", &mtd_fops))
	{
		printk(KERN_NOTICE "Can't allocate major number %d for Memory Technology Devices.\n",
		       MTD_CHAR_MAJOR);
		return -EAGAIN;
	}
#endif

	return 0;
}

static void __exit cleanup_mtdchar(void)
{
#ifdef CONFIG_DEVFS_FS
	unregister_mtd_user(&notifier);
	devfs_unregister(devfs_dir_handle);
	devfs_unregister_chrdev(MTD_CHAR_MAJOR, "mtd");
#else
	unregister_chrdev(MTD_CHAR_MAJOR, "mtd");
#endif
}

module_init(init_mtdchar);
module_exit(cleanup_mtdchar);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("David Woodhouse <dwmw2@infradead.org>");
MODULE_DESCRIPTION("Direct character-device access to MTD devices");
