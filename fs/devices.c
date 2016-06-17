/*
 *  linux/fs/devices.c
 *
 * (C) 1993 Matthias Urlichs -- collected common code and tables.
 * 
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  Added kerneld support: Jacques Gelinas and Bjorn Ekwall
 *  (changed to kmod)
 */

#include <linux/config.h>
#include <linux/fs.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/stat.h>
#include <linux/fcntl.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/smp_lock.h>
#ifdef CONFIG_KMOD
#include <linux/kmod.h>

#include <linux/tty.h>

/* serial module kmod load support */
struct tty_driver *get_tty_driver(kdev_t device);
#define isa_tty_dev(ma)	(ma == TTY_MAJOR || ma == TTYAUX_MAJOR)
#define need_serial(ma,mi) (get_tty_driver(MKDEV(ma,mi)) == NULL)
#endif

struct device_struct {
	const char * name;
	struct file_operations * fops;
};

static rwlock_t chrdevs_lock = RW_LOCK_UNLOCKED;
static struct device_struct chrdevs[MAX_CHRDEV];

extern int get_blkdev_list(char *);

int get_device_list(char * page)
{
	int i;
	int len;

	len = sprintf(page, "Character devices:\n");
	read_lock(&chrdevs_lock);
	for (i = 0; i < MAX_CHRDEV ; i++) {
		if (chrdevs[i].fops) {
			len += sprintf(page+len, "%3d %s\n", i, chrdevs[i].name);
		}
	}
	read_unlock(&chrdevs_lock);
	len += get_blkdev_list(page+len);
	return len;
}

/*
	Return the function table of a device.
	Load the driver if needed.
	Increment the reference count of module in question.
*/
static struct file_operations * get_chrfops(unsigned int major, unsigned int minor)
{
	struct file_operations *ret = NULL;

	if (!major || major >= MAX_CHRDEV)
		return NULL;

	read_lock(&chrdevs_lock);
	ret = fops_get(chrdevs[major].fops);
	read_unlock(&chrdevs_lock);
#ifdef CONFIG_KMOD
	if (ret && isa_tty_dev(major)) {
		lock_kernel();
		if (need_serial(major,minor)) {
			/* Force request_module anyway, but what for? */
			fops_put(ret);
			ret = NULL;
		}
		unlock_kernel();
	}
	if (!ret) {
		char name[20];
		sprintf(name, "char-major-%d", major);
		request_module(name);

		read_lock(&chrdevs_lock);
		ret = fops_get(chrdevs[major].fops);
		read_unlock(&chrdevs_lock);
	}
#endif
	return ret;
}

int register_chrdev(unsigned int major, const char * name, struct file_operations *fops)
{
	if (major == 0) {
		write_lock(&chrdevs_lock);
		for (major = MAX_CHRDEV-1; major > 0; major--) {
			if (chrdevs[major].fops == NULL) {
				chrdevs[major].name = name;
				chrdevs[major].fops = fops;
				write_unlock(&chrdevs_lock);
				return major;
			}
		}
		write_unlock(&chrdevs_lock);
		return -EBUSY;
	}
	if (major >= MAX_CHRDEV)
		return -EINVAL;
	write_lock(&chrdevs_lock);
	if (chrdevs[major].fops && chrdevs[major].fops != fops) {
		write_unlock(&chrdevs_lock);
		return -EBUSY;
	}
	chrdevs[major].name = name;
	chrdevs[major].fops = fops;
	write_unlock(&chrdevs_lock);
	return 0;
}

int unregister_chrdev(unsigned int major, const char * name)
{
	if (major >= MAX_CHRDEV)
		return -EINVAL;
	write_lock(&chrdevs_lock);
	if (!chrdevs[major].fops || strcmp(chrdevs[major].name, name)) {
		write_unlock(&chrdevs_lock);
		return -EINVAL;
	}
	chrdevs[major].name = NULL;
	chrdevs[major].fops = NULL;
	write_unlock(&chrdevs_lock);
	return 0;
}

/*
 * Called every time a character special file is opened
 */
int chrdev_open(struct inode * inode, struct file * filp)
{
	int ret = -ENODEV;

	filp->f_op = get_chrfops(MAJOR(inode->i_rdev), MINOR(inode->i_rdev));
	if (filp->f_op) {
		ret = 0;
		if (filp->f_op->open != NULL) {
			lock_kernel();
			ret = filp->f_op->open(inode,filp);
			unlock_kernel();
		}
	}
	return ret;
}

/*
 * Dummy default file-operations: the only thing this does
 * is contain the open that then fills in the correct operations
 * depending on the special file...
 */
static struct file_operations def_chr_fops = {
	open:		chrdev_open,
};

/*
 * Print device name (in decimal, hexadecimal or symbolic)
 * Note: returns pointer to static data!
 */
const char * kdevname(kdev_t dev)
{
	static char buffer[32];
	sprintf(buffer, "%02x:%02x", MAJOR(dev), MINOR(dev));
	return buffer;
}

const char * cdevname(kdev_t dev)
{
	static char buffer[32];
	const char * name = chrdevs[MAJOR(dev)].name;

	if (!name)
		name = "unknown-char";
	sprintf(buffer, "%s(%d,%d)", name, MAJOR(dev), MINOR(dev));
	return buffer;
}
  
static int sock_no_open(struct inode *irrelevant, struct file *dontcare)
{
	return -ENXIO;
}

static struct file_operations bad_sock_fops = {
	open:		sock_no_open
};

void init_special_inode(struct inode *inode, umode_t mode, int rdev)
{
	inode->i_mode = mode;
	if (S_ISCHR(mode)) {
		inode->i_fop = &def_chr_fops;
		inode->i_rdev = to_kdev_t(rdev);
		inode->i_cdev = cdget(rdev);
	} else if (S_ISBLK(mode)) {
		inode->i_fop = &def_blk_fops;
		inode->i_rdev = to_kdev_t(rdev);
	} else if (S_ISFIFO(mode))
		inode->i_fop = &def_fifo_fops;
	else if (S_ISSOCK(mode))
		inode->i_fop = &bad_sock_fops;
	else
		printk(KERN_DEBUG "init_special_inode: bogus imode (%o)\n", mode);
}
