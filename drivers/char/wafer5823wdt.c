/*
 *	ICP Wafer 5823 Single Board Computer WDT driver for Linux 2.4.x
 *      http://www.icpamerica.com/wafer_5823.php
 *      May also work on other similar models
 *
 *	(c) Copyright 2002 Justin Cormack <justin@street-vision.com>
 *
 *      Release 0.02
 *
 *	Based on advantechwdt.c which is based on wdt.c.
 *	Original copyright messages:
 *
 *	(c) Copyright 1996-1997 Alan Cox <alan@redhat.com>, All Rights Reserved.
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
 *	(c) Copyright 1995    Alan Cox <alan@lxorguk.ukuu.org.uk>
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/watchdog.h>
#include <linux/ioport.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/init.h>
#include <linux/spinlock.h>

static unsigned long wafwdt_is_open;
static spinlock_t wafwdt_lock;
static int expect_close = 0;

/*
 *	You must set these - there is no sane way to probe for this board.
 *
 *      To enable, write the timeout value in seconds (1 to 255) to I/O
 *      port WDT_START, then read the port to start the watchdog. To pat
 *      the dog, read port WDT_STOP to stop the timer, then read WDT_START
 *      to restart it again.
 */

#define WDT_START 0x443
#define WDT_STOP 0x843

#define WD_TIMO 60		/* 1 minute */
static int wd_margin = WD_TIMO;

#ifdef CONFIG_WATCHDOG_NOWAYOUT
static int nowayout = 1;
#else
static int nowayout = 0;
#endif

MODULE_PARM(nowayout,"i");
MODULE_PARM_DESC(nowayout, "Watchdog cannot be stopped once started (default=CONFIG_WATCHDOG_NOWAYOUT)");

static void wafwdt_ping(void)
{
	/* pat watchdog */
        spin_lock(&wafwdt_lock);
	inb_p(WDT_STOP);
	inb_p(WDT_START);
        spin_unlock(&wafwdt_lock);
}

static void wafwdt_start(void)
{
	/* start up watchdog */
	outb_p(wd_margin, WDT_START);
	inb_p(WDT_START);
}

static void
wafwdt_stop(void)
{
	/* stop watchdog */
	inb_p(WDT_STOP);
}

static ssize_t wafwdt_write(struct file *file, const char *buf, size_t count, loff_t * ppos)
{
	/*  Can't seek (pwrite) on this device  */
	if (ppos != &file->f_pos)
		return -ESPIPE;

	if (count) {
		if (!nowayout) {
			size_t i;

			/* In case it was set long ago */
			expect_close = 0;

			for (i = 0; i != count; i++) {
				char c;
				if (get_user(c, buf + i))
					return -EFAULT;
				if (c == 'V')
					expect_close = 1;
			}
		}
		wafwdt_ping();
		return 1;
	}
	return 0;
}

static int wafwdt_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
	     unsigned long arg)
{
	int new_margin;
	static struct watchdog_info ident = {
		WDIOF_KEEPALIVEPING |
		WDIOF_SETTIMEOUT |
		WDIOF_MAGICCLOSE,
		1, "Wafer 5823 WDT"
	};
	int one=1;

	switch (cmd) {
	case WDIOC_GETSUPPORT:
		if (copy_to_user
		    ((struct watchdog_info *) arg, &ident, sizeof (ident)))
			return -EFAULT;
		break;

	case WDIOC_GETSTATUS:
		if (copy_to_user((int *) arg, &one, sizeof (int)))
			return -EFAULT;
		break;

	case WDIOC_KEEPALIVE:
		wafwdt_ping();
		break;

	case WDIOC_SETTIMEOUT:
		if (get_user(new_margin, (int *)arg))
			return -EFAULT;
		if ((new_margin < 1) || (new_margin > 255))
			return -EINVAL;
		wd_margin = new_margin;
		wafwdt_stop();
		wafwdt_start();
		/* Fall */
	case WDIOC_GETTIMEOUT:
		return put_user(wd_margin, (int *)arg);

	default:
		return -ENOTTY;
	}
	return 0;
}

static int wafwdt_open(struct inode *inode, struct file *file)
{
	if (test_and_set_bit(0, &wafwdt_is_open))
		return -EBUSY;
	wafwdt_start();
	return 0;
}

static int
wafwdt_close(struct inode *inode, struct file *file)
{
	clear_bit(0, &wafwdt_is_open);
	if (expect_close) {
        	wafwdt_stop();
	} else {
		printk(KERN_CRIT "WDT device closed unexpectedly.  WDT will not stop!\n");
	}
	return 0;
}

/*
 *	Notifier for system down
 */

static int wafwdt_notify_sys(struct notifier_block *this, unsigned long code, void *unused)
{
	if (code == SYS_DOWN || code == SYS_HALT) {
		/* Turn the WDT off */
		wafwdt_stop();
	}
	return NOTIFY_DONE;
}

/*
 *	Kernel Interfaces
 */

static struct file_operations wafwdt_fops = {
	owner:THIS_MODULE,
	write:wafwdt_write,
	ioctl:wafwdt_ioctl,
	open:wafwdt_open,
	release:wafwdt_close,
};

static struct miscdevice wafwdt_miscdev = {
	WATCHDOG_MINOR,
	"watchdog",
	&wafwdt_fops
};

/*
 *	The WDT needs to learn about soft shutdowns in order to
 *	turn the timebomb registers off. 
 */

static struct notifier_block wafwdt_notifier = {
	wafwdt_notify_sys,
	NULL,
	0
};

static int __init wafwdt_init(void)
{
	printk(KERN_INFO "WDT driver for Wafer 5823 single board computer initialising.\n");

	spin_lock_init(&wafwdt_lock);
	if(!request_region(WDT_STOP, 1, "Wafer 5823 WDT"))
		goto error;
	if(!request_region(WDT_START, 1, "Wafer 5823 WDT"))
		goto error2;
	if(misc_register(&wafwdt_miscdev)<0)
		goto error3;
	register_reboot_notifier(&wafwdt_notifier);
	return 0;
error3:
	release_region(WDT_START, 1);
error2:
	release_region(WDT_STOP, 1);
error:
	return -ENODEV;
}

static void __exit wafwdt_exit(void)
{
	misc_deregister(&wafwdt_miscdev);
	unregister_reboot_notifier(&wafwdt_notifier);
	release_region(WDT_STOP, 1);
	release_region(WDT_START, 1);
}

module_init(wafwdt_init);
module_exit(wafwdt_exit);

MODULE_AUTHOR("Justin Cormack");
MODULE_LICENSE("GPL");
EXPORT_NO_SYMBOLS;

/* end of wafer5823wdt.c */
