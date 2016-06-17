/*
 *	Advantech Single Board Computer WDT driver for Linux 2.4.x
 *
 *	(c) Copyright 2000-2001 Marek Michalkiewicz <marekm@linux.org.pl>
 *
 *	Based on acquirewdt.c which is based on wdt.c.
 *	Original copyright messages:
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
 *	14-Dec-2001 Matt Domsch <Matt_Domsch@dell.com>
 *	    Added nowayout module option to override CONFIG_WATCHDOG_NOWAYOUT
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/watchdog.h>
#include <linux/ioport.h>
#include <linux/fcntl.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/init.h>

static unsigned long advwdt_is_open;
static char adv_expect_close;

/*
 *	You must set these - there is no sane way to probe for this board.
 *
 *	To enable or restart, write the timeout value in seconds (1 to 63)
 *	to I/O port wdt_start.  To disable, read I/O port wdt_stop.
 *	Both are 0x443 for most boards (tested on a PCA-6276VE-00B1), but
 *	check your manual (at least the PCA-6159 seems to be different -
 *	the manual says wdt_stop is 0x43, not 0x443).
 *	(0x43 is also a write-only control register for the 8254 timer!)
 */
 
static int wdt_stop = 0x443;
static int wdt_start = 0x443;

static int wd_margin = 60; /* 60 sec default timeout */

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

#ifndef MODULE

static int __init adv_setup(char *str)
{
	int ints[4];

	str = get_options(str, ARRAY_SIZE(ints), ints);

	if(ints[0] > 0){
		wdt_stop = ints[1];
		if(ints[0] > 1)
			wdt_start = ints[2];
	}

	return 1;
}

__setup("advwdt=", adv_setup);

#endif /* !MODULE */

MODULE_PARM(wdt_stop, "i");
MODULE_PARM_DESC(wdt_stop, "Advantech WDT 'stop' io port (default 0x443)");
MODULE_PARM(wdt_start, "i");
MODULE_PARM_DESC(wdt_start, "Advantech WDT 'start' io port (default 0x443)");

static void
advwdt_ping(void)
{
	/* Write a watchdog value */
	outb_p(wd_margin, wdt_start);
}

static void
advwdt_disable(void)
{
	inb_p(wdt_stop);
}

static ssize_t
advwdt_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	/*  Can't seek (pwrite) on this device  */
	if (ppos != &file->f_pos)
		return -ESPIPE;

	if (count) {
		if (!nowayout) {
			size_t i;

			adv_expect_close = 0;
	
			for (i = 0; i != count; i++) {
				char c;
				if(get_user(c, buf+i))
					return -EFAULT;
				if (c == 'V')
					adv_expect_close = 42;
			}
		}
			advwdt_ping();
	}
	return count;
}

static int
advwdt_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
	  unsigned long arg)
{
	int new_margin;
	static struct watchdog_info ident = {
		options:		WDIOF_KEEPALIVEPING | WDIOF_SETTIMEOUT | WDIOF_MAGICCLOSE,
		firmware_version:	0,
		identity:		"Advantech WDT"
	};

	switch (cmd) {
	case WDIOC_GETSUPPORT:
	  if (copy_to_user((struct watchdog_info *)arg, &ident, sizeof(ident)))
	    return -EFAULT;
	  break;

	case WDIOC_GETSTATUS:
	case WDIOC_GETBOOTSTATUS:
	  return put_user(0, (int *)arg);

	case WDIOC_KEEPALIVE:
	  advwdt_ping();
	  break;

	case WDIOC_SETTIMEOUT:
	  if (get_user(new_margin, (int *)arg))
		  return -EFAULT;
	  if ((new_margin < 1) || (new_margin > 63))
		  return -EINVAL;
	  wd_margin = new_margin;
	  advwdt_ping();
	  /* Fall */

	case WDIOC_GETTIMEOUT:
	  return put_user(wd_margin, (int *)arg);

	case WDIOC_SETOPTIONS:
	{
	  int options, retval = -EINVAL;

	  if (get_user(options, (int *)arg))
	    return -EFAULT;

	  if (options & WDIOS_DISABLECARD) {
	    advwdt_disable();
	    retval = 0;
	  }

	  if (options & WDIOS_ENABLECARD) {
	    advwdt_ping();
	    retval = 0;
	  }

	  return retval;
	}

	default:
	  return -ENOTTY;
	}
	return 0;
}

static int
advwdt_open(struct inode *inode, struct file *file)
{
	if (test_and_set_bit(0, &advwdt_is_open))
		return -EBUSY;
	/*
	 *	Activate
	 */

	advwdt_ping();
	return 0;
}

static int
advwdt_close(struct inode *inode, struct file *file)
{
	if (adv_expect_close == 42) {
		advwdt_disable();
	} else {
		printk(KERN_CRIT "advancetechwdt: Unexpected close, not stopping watchdog!\n");
		advwdt_ping();
	}
	clear_bit(0, &advwdt_is_open);
	adv_expect_close = 0;
	return 0;
}

/*
 *	Notifier for system down
 */

static int
advwdt_notify_sys(struct notifier_block *this, unsigned long code,
	void *unused)
{
	if (code == SYS_DOWN || code == SYS_HALT) {
		/* Turn the WDT off */
		advwdt_disable();
	}
	return NOTIFY_DONE;
}
 
/*
 *	Kernel Interfaces
 */
 
static struct file_operations advwdt_fops = {
	owner:		THIS_MODULE,
	llseek:		no_llseek,
	write:		advwdt_write,
	ioctl:		advwdt_ioctl,
	open:		advwdt_open,
	release:	advwdt_close,
};

static struct miscdevice advwdt_miscdev = {
	minor:		WATCHDOG_MINOR,
	name:		"watchdog",
	fops:		&advwdt_fops,
};

/*
 *	The WDT needs to learn about soft shutdowns in order to
 *	turn the timebomb registers off.
 */
 
static struct notifier_block advwdt_notifier = {
	advwdt_notify_sys,
	NULL,
	0
};

static int __init
advwdt_init(void)
{
	printk(KERN_INFO "WDT driver for Advantech single board computer initialising.\n");

	misc_register(&advwdt_miscdev);
	if(wdt_stop != wdt_start)
		request_region(wdt_stop, 1, "Advantech WDT");
	request_region(wdt_start, 1, "Advantech WDT");
	register_reboot_notifier(&advwdt_notifier);
	return 0;
}

static void __exit
advwdt_exit(void)
{
	misc_deregister(&advwdt_miscdev);
	unregister_reboot_notifier(&advwdt_notifier);
	if(wdt_stop != wdt_start)
		release_region(wdt_stop,1);
	release_region(wdt_start,1);
}

module_init(advwdt_init);
module_exit(advwdt_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marek Michalkiewicz <marekm@linux.org.pl>");
MODULE_DESCRIPTION("Advantech Single Board Computer WDT driver");
EXPORT_NO_SYMBOLS;

/* end of advantechwdt.c */

