/*
 *	Acquire Single Board Computer Watchdog Timer driver for Linux 2.1.x
 *
 *      Based on wdt.c. Original copyright messages:
 *
 *	(c) Copyright 1996 Alan Cox <alan@redhat.com>, All Rights Reserved.
 *				http://www.redhat.com
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 *	
 *	Neither Alan Cox nor CymruNet Ltd. admit liability nor provide 
 *	warranty for any of this software. This material is provided 
 *	"AS-IS" and at no charge.	
 *
 *	(c) Copyright 1995    Alan Cox <alan@redhat.com>
 *
 *      14-Dec-2001 Matt Domsch <Matt_Domsch@dell.com>
 *          Added nowayout module option to override CONFIG_WATCHDOG_NOWAYOUT
 *          Can't add timeout - driver doesn't allow changing value
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/miscdevice.h>
#include <linux/watchdog.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/fcntl.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/smp_lock.h>

static int acq_is_open;
static spinlock_t acq_lock;
static int expect_close = 0;

/*
 *	You must set these - there is no sane way to probe for this board.
 */
 
#define WDT_STOP 0x43
#define WDT_START 0x443

#ifdef CONFIG_WATCHDOG_NOWAYOUT
static int nowayout = 1;
#else
static int nowayout = 0;
#endif

MODULE_PARM(nowayout,"i");
MODULE_PARM_DESC(nowayout, "Watchdog cannot be stopped once started (default=CONFIG_WATCHDOG_NOWAYOUT)");

/*
 *	Kernel methods.
 */
 

static void acq_ping(void)
{
	/* Write a watchdog value */
	inb_p(WDT_START);
}

static ssize_t acq_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	/*  Can't seek (pwrite) on this device  */
	if (ppos != &file->f_pos)
		return -ESPIPE;

	if(count)
	{
		if (!nowayout)
		{
			size_t i;

			expect_close = 0;

			for (i = 0; i != count; i++) {
				char c;
				if (get_user(c, buf + i))
					return -EFAULT;
				if (c == 'V')
					expect_close = 1;
			}
		}

		acq_ping();
		return 1;
	}
	return 0;
}

static ssize_t acq_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	return -EINVAL;
}



static int acq_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
	unsigned long arg)
{
	static struct watchdog_info ident=
	{
		WDIOF_KEEPALIVEPING | WDIOF_MAGICCLOSE, 1, "Acquire WDT"
	};
	
	switch(cmd)
	{
	case WDIOC_GETSUPPORT:
	  if (copy_to_user((struct watchdog_info *)arg, &ident, sizeof(ident)))
	    return -EFAULT;
	  break;
	  
	case WDIOC_GETSTATUS:
	  if (copy_to_user((int *)arg, &acq_is_open,  sizeof(int)))
	    return -EFAULT;
	  break;

	case WDIOC_KEEPALIVE:
	  acq_ping();
	  break;

	default:
	  return -ENOTTY;
	}
	return 0;
}

static int acq_open(struct inode *inode, struct file *file)
{
	switch(MINOR(inode->i_rdev))
	{
		case WATCHDOG_MINOR:
			spin_lock(&acq_lock);
			if(acq_is_open)
			{
				spin_unlock(&acq_lock);
				return -EBUSY;
			}
			if (nowayout) {
				MOD_INC_USE_COUNT;
			}
			/*
			 *	Activate 
			 */
			acq_is_open=1;
			inb_p(WDT_START);      
			spin_unlock(&acq_lock);
			return 0;
		default:
			return -ENODEV;
	}
}

static int acq_close(struct inode *inode, struct file *file)
{
	lock_kernel();
	if(MINOR(inode->i_rdev)==WATCHDOG_MINOR)
	{
		spin_lock(&acq_lock);
		if (expect_close)
		{
			inb_p(WDT_STOP);
		}
		else
		{
			printk(KERN_CRIT "WDT closed unexpectedly.  WDT will not stop!\n");
		}
		acq_is_open=0;
		spin_unlock(&acq_lock);
	}
	unlock_kernel();
	return 0;
}

/*
 *	Notifier for system down
 */

static int acq_notify_sys(struct notifier_block *this, unsigned long code,
	void *unused)
{
	if(code==SYS_DOWN || code==SYS_HALT)
	{
		/* Turn the card off */
		inb_p(WDT_STOP);
	}
	return NOTIFY_DONE;
}
 
/*
 *	Kernel Interfaces
 */
 
 
static struct file_operations acq_fops = {
	owner:		THIS_MODULE,
	read:		acq_read,
	write:		acq_write,
	ioctl:		acq_ioctl,
	open:		acq_open,
	release:	acq_close,
};

static struct miscdevice acq_miscdev=
{
	WATCHDOG_MINOR,
	"watchdog",
	&acq_fops
};


/*
 *	The WDT card needs to learn about soft shutdowns in order to
 *	turn the timebomb registers off. 
 */
 
static struct notifier_block acq_notifier=
{
	acq_notify_sys,
	NULL,
	0
};

static int __init acq_init(void)
{
	printk("WDT driver for Acquire single board computer initialising.\n");

	spin_lock_init(&acq_lock);
	if (misc_register(&acq_miscdev))
		return -ENODEV;
	request_region(WDT_STOP, 1, "Acquire WDT");
	request_region(WDT_START, 1, "Acquire WDT");
	register_reboot_notifier(&acq_notifier);
	return 0;
}
	
static void __exit acq_exit(void)
{
	misc_deregister(&acq_miscdev);
	unregister_reboot_notifier(&acq_notifier);
	release_region(WDT_STOP,1);
	release_region(WDT_START,1);
}

module_init(acq_init);
module_exit(acq_exit);

MODULE_LICENSE("GPL");
EXPORT_NO_SYMBOLS;
