/*
 * linux/drivers/char/misc.c
 *
 * Generic misc open routine by Johan Myreen
 *
 * Based on code from Linus
 *
 * Teemu Rantanen's Microsoft Busmouse support and Derrick Cole's
 *   changes incorporated into 0.97pl4
 *   by Peter Cervasio (pete%q106fm.uucp@wupost.wustl.edu) (08SEP92)
 *   See busmouse.c for particulars.
 *
 * Made things a lot mode modular - easy to compile in just one or two
 * of the misc drivers, as they are now completely independent. Linus.
 *
 * Support for loadable modules. 8-Sep-95 Philip Blundell <pjb27@cam.ac.uk>
 *
 * Fixed a failing symbol register to free the device registration
 *		Alan Cox <alan@lxorguk.ukuu.org.uk> 21-Jan-96
 *
 * Dynamic minors and /proc/mice by Alessandro Rubini. 26-Mar-96
 *
 * Renamed to misc and miscdevice to be more accurate. Alan Cox 26-Mar-96
 *
 * Handling of mouse minor numbers for kerneld:
 *  Idea by Jacques Gelinas <jack@solucorp.qc.ca>,
 *  adapted by Bjorn Ekwall <bj0rn@blox.se>
 *  corrected by Alan Cox <alan@lxorguk.ukuu.org.uk>
 *
 * Changes for kmod (from kerneld):
 *	Cyrus Durgin <cider@speakeasy.org>
 *
 * Added devfs support. Richard Gooch <rgooch@atnf.csiro.au>  10-Jan-1998
 */

#include <linux/module.h>
#include <linux/config.h>

#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/miscdevice.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/stat.h>
#include <linux/init.h>

#include <linux/tty.h>
#include <linux/selection.h>
#include <linux/kmod.h>

#include "busmouse.h"

/*
 * Head entry for the doubly linked miscdevice list
 */
static struct miscdevice misc_list = { 0, "head", NULL, &misc_list, &misc_list };
static DECLARE_MUTEX(misc_sem);

/*
 * Assigned numbers, used for dynamic minors
 */
#define DYNAMIC_MINORS 64 /* like dynamic majors */
static unsigned char misc_minors[DYNAMIC_MINORS / 8];

extern int psaux_init(void);
extern int rtc_DP8570A_init(void);
extern int rtc_MK48T08_init(void);
extern int ds1286_init(void);
extern int pmu_device_init(void);
extern int tosh_init(void);
extern int i8k_init(void);
extern int lcd_init(void);

static int misc_read_proc(char *buf, char **start, off_t offset,
			  int len, int *eof, void *private)
{
	struct miscdevice *p;
	int written;

	written=0;
	for (p = misc_list.next; p != &misc_list && written < len; p = p->next) {
		written += sprintf(buf+written, "%3i %s\n",p->minor, p->name ?: "");
		if (written < offset) {
			offset -= written;
			written = 0;
		}
	}
	*start = buf + offset;
	written -= offset;
	if(written > len) {
		*eof = 0;
		return len;
	}
	*eof = 1;
	return (written<0) ? 0 : written;
}


static int misc_open(struct inode * inode, struct file * file)
{
	int minor = MINOR(inode->i_rdev);
	struct miscdevice *c;
	int err = -ENODEV;
	struct file_operations *old_fops, *new_fops = NULL;
	
	down(&misc_sem);
	
	c = misc_list.next;

	while ((c != &misc_list) && (c->minor != minor))
		c = c->next;
	if (c != &misc_list)
		new_fops = fops_get(c->fops);
	if (!new_fops) {
		char modname[20];
		up(&misc_sem);
		sprintf(modname, "char-major-%d-%d", MISC_MAJOR, minor);
		request_module(modname);
		down(&misc_sem);
		c = misc_list.next;
		while ((c != &misc_list) && (c->minor != minor))
			c = c->next;
		if (c == &misc_list || (new_fops = fops_get(c->fops)) == NULL)
			goto fail;
	}

	err = 0;
	old_fops = file->f_op;
	file->f_op = new_fops;
	if (file->f_op->open) {
		err=file->f_op->open(inode,file);
		if (err) {
			fops_put(file->f_op);
			file->f_op = fops_get(old_fops);
		}
	}
	fops_put(old_fops);
fail:
	up(&misc_sem);
	return err;
}

static struct file_operations misc_fops = {
	owner:		THIS_MODULE,
	open:		misc_open,
};


/**
 *	misc_register	-	register a miscellaneous device
 *	@misc: device structure
 *	
 *	Register a miscellaneous device with the kernel. If the minor
 *	number is set to %MISC_DYNAMIC_MINOR a minor number is assigned
 *	and placed in the minor field of the structure. For other cases
 *	the minor number requested is used.
 *
 *	The structure passed is linked into the kernel and may not be
 *	destroyed until it has been unregistered.
 *
 *	A zero is returned on success and a negative errno code for
 *	failure.
 */
 
int misc_register(struct miscdevice * misc)
{
	static devfs_handle_t devfs_handle, dir;
	struct miscdevice *c;
	
	if (misc->next || misc->prev)
		return -EBUSY;
	down(&misc_sem);
	c = misc_list.next;

	while ((c != &misc_list) && (c->minor != misc->minor))
		c = c->next;
	if (c != &misc_list) {
		up(&misc_sem);
		return -EBUSY;
	}

	if (misc->minor == MISC_DYNAMIC_MINOR) {
		int i = DYNAMIC_MINORS;
		while (--i >= 0)
			if ( (misc_minors[i>>3] & (1 << (i&7))) == 0)
				break;
		if (i<0)
		{
			up(&misc_sem);
			return -EBUSY;
		}
		misc->minor = i;
	}
	if (misc->minor < DYNAMIC_MINORS)
		misc_minors[misc->minor >> 3] |= 1 << (misc->minor & 7);
	if (!devfs_handle)
		devfs_handle = devfs_mk_dir (NULL, "misc", NULL);
	dir = strchr (misc->name, '/') ? NULL : devfs_handle;
	misc->devfs_handle =
		devfs_register (dir, misc->name, DEVFS_FL_NONE,
				MISC_MAJOR, misc->minor,
				S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP,
				misc->fops, NULL);

	/*
	 * Add it to the front, so that later devices can "override"
	 * earlier defaults
	 */
	misc->prev = &misc_list;
	misc->next = misc_list.next;
	misc->prev->next = misc;
	misc->next->prev = misc;
	up(&misc_sem);
	return 0;
}

/**
 *	misc_deregister - unregister a miscellaneous device
 *	@misc: device to unregister
 *
 *	Unregister a miscellaneous device that was previously
 *	successfully registered with misc_register(). Success
 *	is indicated by a zero return, a negative errno code
 *	indicates an error.
 */

int misc_deregister(struct miscdevice * misc)
{
	int i = misc->minor;
	if (!misc->next || !misc->prev)
		return -EINVAL;
	down(&misc_sem);
	misc->prev->next = misc->next;
	misc->next->prev = misc->prev;
	misc->next = NULL;
	misc->prev = NULL;
	devfs_unregister (misc->devfs_handle);
	if (i < DYNAMIC_MINORS && i>0) {
		misc_minors[i>>3] &= ~(1 << (misc->minor & 7));
	}
	up(&misc_sem);
	return 0;
}

EXPORT_SYMBOL(misc_register);
EXPORT_SYMBOL(misc_deregister);

int __init misc_init(void)
{
	create_proc_read_entry("misc", 0, 0, misc_read_proc, NULL);
#ifdef CONFIG_MVME16x
	rtc_MK48T08_init();
#endif
#ifdef CONFIG_BVME6000
	rtc_DP8570A_init();
#endif
#ifdef CONFIG_SGI_DS1286
	ds1286_init();
#endif
#ifdef CONFIG_PMAC_PBOOK
	pmu_device_init();
#endif
#ifdef CONFIG_TOSHIBA
	tosh_init();
#endif
#ifdef CONFIG_COBALT_LCD
	lcd_init();
#endif
#ifdef CONFIG_I8K
	i8k_init();
#endif
	if (devfs_register_chrdev(MISC_MAJOR,"misc",&misc_fops)) {
		printk("unable to get major %d for misc devices\n",
		       MISC_MAJOR);
		return -EIO;
	}
	return 0;
}
