/*
 * $Id: mtdcore.c,v 1.34 2003/01/24 23:32:25 dwmw2 Exp $
 *
 * Core registration and callback routines for MTD
 * drivers and users.
 *
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/major.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/mtd/compatmac.h>
#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
#endif

#include <linux/mtd/mtd.h>

static DECLARE_MUTEX(mtd_table_mutex);
static struct mtd_info *mtd_table[MAX_MTD_DEVICES];
static struct mtd_notifier *mtd_notifiers = NULL;

/**
 *	add_mtd_device - register an MTD device
 *	@mtd: pointer to new MTD device info structure
 *
 *	Add a device to the list of MTD devices present in the system, and
 *	notify each currently active MTD 'user' of its arrival. Returns
 *	zero on success or 1 on failure, which currently will only happen
 *	if the number of present devices exceeds MAX_MTD_DEVICES (i.e. 16)
 */

int add_mtd_device(struct mtd_info *mtd)
{
	int i;

	down(&mtd_table_mutex);

	for (i=0; i< MAX_MTD_DEVICES; i++)
		if (!mtd_table[i])
		{
			struct mtd_notifier *not=mtd_notifiers;

			mtd_table[i] = mtd;
			mtd->index = i;
			DEBUG(0, "mtd: Giving out device %d to %s\n",i, mtd->name);
			while (not)
			{
				(*(not->add))(mtd);
				not = not->next;
			}
			up(&mtd_table_mutex);
			MOD_INC_USE_COUNT;
			return 0;
		}
	
	up(&mtd_table_mutex);
	return 1;
}

/**
 *	del_mtd_device - unregister an MTD device
 *	@mtd: pointer to MTD device info structure
 *
 *	Remove a device from the list of MTD devices present in the system,
 *	and notify each currently active MTD 'user' of its departure.
 *	Returns zero on success or 1 on failure, which currently will happen
 *	if the requested device does not appear to be present in the list.
 */

int del_mtd_device (struct mtd_info *mtd)
{
	struct mtd_notifier *not=mtd_notifiers;
	int i;
	
	down(&mtd_table_mutex);

	for (i=0; i < MAX_MTD_DEVICES; i++)
	{
		if (mtd_table[i] == mtd)
		{
			while (not)
			{
				(*(not->remove))(mtd);
				not = not->next;
			}
			mtd_table[i] = NULL;
			up (&mtd_table_mutex);
			MOD_DEC_USE_COUNT;
			return 0;
		}
	}

	up(&mtd_table_mutex);
	return 1;
}

/**
 *	register_mtd_user - register a 'user' of MTD devices.
 *	@new: pointer to notifier info structure
 *
 *	Registers a pair of callbacks function to be called upon addition
 *	or removal of MTD devices. Causes the 'add' callback to be immediately
 *	invoked for each MTD device currently present in the system.
 */

void register_mtd_user (struct mtd_notifier *new)
{
	int i;

	down(&mtd_table_mutex);

	new->next = mtd_notifiers;
	mtd_notifiers = new;

 	MOD_INC_USE_COUNT;
	
	for (i=0; i< MAX_MTD_DEVICES; i++)
		if (mtd_table[i])
			new->add(mtd_table[i]);

	up(&mtd_table_mutex);
}

/**
 *	register_mtd_user - unregister a 'user' of MTD devices.
 *	@new: pointer to notifier info structure
 *
 *	Removes a callback function pair from the list of 'users' to be
 *	notified upon addition or removal of MTD devices. Causes the
 *	'remove' callback to be immediately invoked for each MTD device
 *	currently present in the system.
 */

int unregister_mtd_user (struct mtd_notifier *old)
{
	struct mtd_notifier **prev = &mtd_notifiers;
	struct mtd_notifier *cur;
	int i;

	down(&mtd_table_mutex);

	while ((cur = *prev)) {
		if (cur == old) {
			*prev = cur->next;

			MOD_DEC_USE_COUNT;

			for (i=0; i< MAX_MTD_DEVICES; i++)
				if (mtd_table[i])
					old->remove(mtd_table[i]);
			
			up(&mtd_table_mutex);
			return 0;
		}
		prev = &cur->next;
	}
	up(&mtd_table_mutex);
	return 1;
}


/**
 *	__get_mtd_device - obtain a validated handle for an MTD device
 *	@mtd: last known address of the required MTD device
 *	@num: internal device number of the required MTD device
 *
 *	Given a number and NULL address, return the num'th entry in the device
 *	table, if any.	Given an address and num == -1, search the device table
 *	for a device with that address and return if it's still present. Given
 *	both, return the num'th driver only if its address matches. Return NULL
 *	if not. get_mtd_device() increases the use count, but
 *	__get_mtd_device() doesn't - you should generally use get_mtd_device().
 */
	
struct mtd_info *__get_mtd_device(struct mtd_info *mtd, int num)
{
	struct mtd_info *ret = NULL;
	int i;

	down(&mtd_table_mutex);

	if (num == -1) {
		for (i=0; i< MAX_MTD_DEVICES; i++)
			if (mtd_table[i] == mtd)
				ret = mtd_table[i];
	} else if (num < MAX_MTD_DEVICES) {
		ret = mtd_table[num];
		if (mtd && mtd != ret)
			ret = NULL;
	}
	
	up(&mtd_table_mutex);
	return ret;
}


/* default_mtd_writev - default mtd writev method for MTD devices that
 *			dont implement their own
 */

int default_mtd_writev(struct mtd_info *mtd, const struct iovec *vecs,
		       unsigned long count, loff_t to, size_t *retlen)
{
	unsigned long i;
	size_t totlen = 0, thislen;
	int ret = 0;

	if(!mtd->write) {
		ret = -EROFS;
	} else {
		for (i=0; i<count; i++) {
			if (!vecs[i].iov_len)
				continue;
			ret = mtd->write(mtd, to, vecs[i].iov_len, &thislen, vecs[i].iov_base);
			totlen += thislen;
			if (ret || thislen != vecs[i].iov_len)
				break;
			to += vecs[i].iov_len;
		}
	}
	if (retlen)
		*retlen = totlen;
	return ret;
}


/* default_mtd_readv - default mtd readv method for MTD devices that dont
 *		       implement their own
 */

int default_mtd_readv(struct mtd_info *mtd, struct iovec *vecs,
		      unsigned long count, loff_t from, size_t *retlen)
{
	unsigned long i;
	size_t totlen = 0, thislen;
	int ret = 0;

	if(!mtd->read) {
		ret = -EIO;
	} else {
		for (i=0; i<count; i++) {
			if (!vecs[i].iov_len)
				continue;
			ret = mtd->read(mtd, from, vecs[i].iov_len, &thislen, vecs[i].iov_base);
			totlen += thislen;
			if (ret || thislen != vecs[i].iov_len)
				break;
			from += vecs[i].iov_len;
		}
	}
	if (retlen)
		*retlen = totlen;
	return ret;
}


EXPORT_SYMBOL(add_mtd_device);
EXPORT_SYMBOL(del_mtd_device);
EXPORT_SYMBOL(__get_mtd_device);
EXPORT_SYMBOL(register_mtd_user);
EXPORT_SYMBOL(unregister_mtd_user);
EXPORT_SYMBOL(default_mtd_writev);
EXPORT_SYMBOL(default_mtd_readv);

/*====================================================================*/
/* Power management code */

#ifdef CONFIG_PM

#include <linux/pm.h>

static struct pm_dev *mtd_pm_dev = NULL;

static int mtd_pm_callback(struct pm_dev *dev, pm_request_t rqst, void *data)
{
	int ret = 0, i;

	if (down_trylock(&mtd_table_mutex))
		return -EAGAIN;
	if (rqst == PM_SUSPEND) {
		for (i = 0; ret == 0 && i < MAX_MTD_DEVICES; i++) {
			if (mtd_table[i] && mtd_table[i]->suspend)
				ret = mtd_table[i]->suspend(mtd_table[i]);
		}
	} else i = MAX_MTD_DEVICES-1;

	if (rqst == PM_RESUME || ret) {
		for ( ; i >= 0; i--) {
			if (mtd_table[i] && mtd_table[i]->resume)
				mtd_table[i]->resume(mtd_table[i]);
		}
	}
	up(&mtd_table_mutex);
	return ret;
}
#endif

/*====================================================================*/
/* Support for /proc/mtd */

#ifdef CONFIG_PROC_FS

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,0)
static struct proc_dir_entry *proc_mtd;
#endif

static inline int mtd_proc_info (char *buf, int i)
{
	struct mtd_info *this = mtd_table[i];

	if (!this)
		return 0;

	return sprintf(buf, "mtd%d: %8.8x %8.8x \"%s\"\n", i, this->size,
		       this->erasesize, this->name);
}

static int mtd_read_proc ( char *page, char **start, off_t off,int count
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,0)
                       ,int *eof, void *data_unused
#else
                        ,int unused
#endif
			)
{
	int len, l, i;
        off_t   begin = 0;

	down(&mtd_table_mutex);

	len = sprintf(page, "dev:    size   erasesize  name\n");
        for (i=0; i< MAX_MTD_DEVICES; i++) {

                l = mtd_proc_info(page + len, i);
                len += l;
                if (len+begin > off+count)
                        goto done;
                if (len+begin < off) {
                        begin += len;
                        len = 0;
                }
        }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,0)
        *eof = 1;
#endif

done:
	up(&mtd_table_mutex);
        if (off >= len+begin)
                return 0;
        *start = page + (off-begin);
        return ((count < begin+len-off) ? count : begin+len-off);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,0)
struct proc_dir_entry mtd_proc_entry = {
        0,                 /* low_ino: the inode -- dynamic */
        3, "mtd",     /* len of name and name */
        S_IFREG | S_IRUGO, /* mode */
        1, 0, 0,           /* nlinks, owner, group */
        0, NULL,           /* size - unused; operations -- use default */
        &mtd_read_proc,   /* function used to read data */
        /* nothing more */
    };
#endif

#endif /* CONFIG_PROC_FS */

/*====================================================================*/
/* Init code */

int __init init_mtd(void)
{
#ifdef CONFIG_PROC_FS
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,0)
	if ((proc_mtd = create_proc_entry( "mtd", 0, 0 )))
	  proc_mtd->read_proc = mtd_read_proc;
#else
        proc_register_dynamic(&proc_root,&mtd_proc_entry);
#endif
#endif

#if LINUX_VERSION_CODE < 0x20212
	init_mtd_devices();
#endif

#ifdef CONFIG_PM
	mtd_pm_dev = pm_register(PM_UNKNOWN_DEV, 0, mtd_pm_callback);
#endif
	return 0;
}

static void __exit cleanup_mtd(void)
{
#ifdef CONFIG_PM
	if (mtd_pm_dev) {
		pm_unregister(mtd_pm_dev);
		mtd_pm_dev = NULL;
	}
#endif

#ifdef CONFIG_PROC_FS
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,0)
        if (proc_mtd)
          remove_proc_entry( "mtd", 0);
#else
        proc_unregister(&proc_root,mtd_proc_entry.low_ino);
#endif
#endif
}

module_init(init_mtd);
module_exit(cleanup_mtd);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("David Woodhouse <dwmw2@infradead.org>");
MODULE_DESCRIPTION("Core MTD registration and access routines");
