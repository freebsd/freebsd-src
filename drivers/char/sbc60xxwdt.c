/*
 *	60xx Single Board Computer Watchdog Timer driver for Linux 2.2.x
 *
 *      Based on acquirewdt.c by Alan Cox.
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 *	
 *	The author does NOT admit liability nor provide warranty for 
 *	any of this software. This material is provided "AS-IS" in 
 *      the hope that it may be useful for others.
 *
 *	(c) Copyright 2000    Jakob Oestergaard <jakob@ostenfeld.dk>
 *
 *           12/4 - 2000      [Initial revision]
 *           25/4 - 2000      Added /dev/watchdog support
 *           09/5 - 2001      [smj@oro.net] fixed fop_write to "return 1" on success
 *
 *
 *  Theory of operation:
 *  A Watchdog Timer (WDT) is a hardware circuit that can 
 *  reset the computer system in case of a software fault.
 *  You probably knew that already.
 *
 *  Usually a userspace daemon will notify the kernel WDT driver
 *  via the /proc/watchdog special device file that userspace is
 *  still alive, at regular intervals.  When such a notification
 *  occurs, the driver will usually tell the hardware watchdog
 *  that everything is in order, and that the watchdog should wait
 *  for yet another little while to reset the system.
 *  If userspace fails (RAM error, kernel bug, whatever), the
 *  notifications cease to occur, and the hardware watchdog will
 *  reset the system (causing a reboot) after the timeout occurs.
 *
 *  This WDT driver is different from the other Linux WDT 
 *  drivers in several ways:
 *  *)  The driver will ping the watchdog by itself, because this
 *      particular WDT has a very short timeout (one second) and it
 *      would be insane to count on any userspace daemon always
 *      getting scheduled within that time frame.
 *  *)  This driver expects the userspace daemon to send a specific
 *      character code ('V') to /dev/watchdog before closing the
 *      /dev/watchdog file.  If the userspace daemon closes the file
 *      without sending this special character, the driver will assume
 *      that the daemon (and userspace in general) died, and will
 *      stop pinging the WDT without disabling it first.  This will
 *      cause a reboot.
 *
 *  Why `V' ?  Well, `V' is the character in ASCII for the value 86,
 *  and we all know that 86 is _the_ most random number in the universe.
 *  Therefore it is the letter that has the slightest chance of occuring
 *  by chance, when the system becomes corrupted.
 *
 */

#include <linux/module.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/miscdevice.h>
#include <linux/watchdog.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/fcntl.h>
#include <linux/smp_lock.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/init.h>

#define OUR_NAME "sbc60xxwdt"

/*
 * You must set these - The driver cannot probe for the settings
 */
 
#define WDT_STOP 0x45
#define WDT_START 0x443

/*
 * The 60xx board can use watchdog timeout values from one second
 * to several minutes.  The default is one second, so if we reset
 * the watchdog every ~250ms we should be safe.
 */

#define WDT_INTERVAL (HZ/4+1)

/*
 * We must not require too good response from the userspace daemon.
 * Here we require the userspace daemon to send us a heartbeat
 * char to /dev/watchdog every 10 seconds.
 * If the daemon pulses us every 5 seconds, we can still afford
 * a 5 second scheduling delay on the (high priority) daemon. That
 * should be sufficient for a box under any load.
 */

#define WDT_HEARTBEAT (HZ * 10)

static void wdt_timer_ping(unsigned long);
static struct timer_list timer;
static unsigned long next_heartbeat;
static int wdt_is_open;
static int wdt_expect_close;

/*
 *	Whack the dog
 */

static void wdt_timer_ping(unsigned long data)
{
	/* If we got a heartbeat pulse within the WDT_US_INTERVAL
	 * we agree to ping the WDT 
	 */
	if(time_before(jiffies, next_heartbeat)) 
	{
		/* Ping the WDT by reading from WDT_START */
		inb_p(WDT_START);
		/* Re-set the timer interval */
		timer.expires = jiffies + WDT_INTERVAL;
		add_timer(&timer);
	} else {
		printk(OUR_NAME ": Heartbeat lost! Will not ping the watchdog\n");
	}
}

/* 
 * Utility routines
 */

static void wdt_startup(void)
{
	next_heartbeat = jiffies + WDT_HEARTBEAT;

	/* Start the timer */
	timer.expires = jiffies + WDT_INTERVAL;	
	add_timer(&timer);
	printk(OUR_NAME ": Watchdog timer is now enabled.\n");  
}

static void wdt_turnoff(void)
{
	/* Stop the timer */
	del_timer(&timer);
	inb_p(WDT_STOP);
	printk(OUR_NAME ": Watchdog timer is now disabled...\n");
}


/*
 * /dev/watchdog handling
 */

static ssize_t fop_write(struct file * file, const char * buf, size_t count, loff_t * ppos)
{
	/* We can't seek */
	if(ppos != &file->f_pos)
		return -ESPIPE;

	/* See if we got the magic character */
	if(count) 
	{
		size_t ofs;

		/* note: just in case someone wrote the magic character
		 * five months ago... */
		wdt_expect_close = 0;

		/* now scan */
		for(ofs = 0; ofs != count; ofs++) 
		{
			char c;
			if(get_user(c, buf+ofs))
				return -EFAULT;
			if(c == 'V')
				wdt_expect_close = 1;
		}
		/* Well, anyhow someone wrote to us, we should return that favour */
		next_heartbeat = jiffies + WDT_HEARTBEAT;
		return 1;
	}
	return 0;
}

static ssize_t fop_read(struct file * file, char * buf, size_t count, loff_t * ppos)
{
	/* No can do */
	return -EINVAL;
}

static int fop_open(struct inode * inode, struct file * file)
{
	switch(MINOR(inode->i_rdev)) 
	{
		case WATCHDOG_MINOR:
			/* Just in case we're already talking to someone... */
			if(wdt_is_open)
				return -EBUSY;
			/* Good, fire up the show */
			wdt_is_open = 1;
			wdt_startup();
			return 0;

		default:
			return -ENODEV;
	}
}

static int fop_close(struct inode * inode, struct file * file)
{
	lock_kernel();
	if(MINOR(inode->i_rdev) == WATCHDOG_MINOR) 
	{
		if(wdt_expect_close)
			wdt_turnoff();
		else {
			del_timer(&timer);
			printk(OUR_NAME ": device file closed unexpectedly. Will not stop the WDT!\n");
		}
	}
	wdt_is_open = 0;
	unlock_kernel();
	return 0;
}

static int fop_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
	unsigned long arg)
{
	static struct watchdog_info ident=
	{
		WDIOF_MAGICCLOSE,
		1,
		"SB60xx"
	};
	
	switch(cmd)
	{
		default:
			return -ENOTTY;
		case WDIOC_GETSUPPORT:
			return copy_to_user((struct watchdog_info *)arg, &ident, sizeof(ident))?-EFAULT:0;
		case WDIOC_KEEPALIVE:
			next_heartbeat = jiffies + WDT_HEARTBEAT;
			return 0;
	}
}

static struct file_operations wdt_fops = {
	owner:		THIS_MODULE,
	llseek:		no_llseek,
	read:		fop_read,
	write:		fop_write,
	open:		fop_open,
	release:	fop_close,
	ioctl:		fop_ioctl
};

static struct miscdevice wdt_miscdev = {
	WATCHDOG_MINOR,
	"watchdog",
	&wdt_fops
};

/*
 *	Notifier for system down
 */

static int wdt_notify_sys(struct notifier_block *this, unsigned long code,
	void *unused)
{
	if(code==SYS_DOWN || code==SYS_HALT) 
		wdt_turnoff();
	return NOTIFY_DONE;
}
 
/*
 *	The WDT needs to learn about soft shutdowns in order to
 *	turn the timebomb registers off. 
 */
 
static struct notifier_block wdt_notifier=
{
	wdt_notify_sys,
	0,
	0
};

static void __exit sbc60xxwdt_unload(void)
{
	wdt_turnoff();

	/* Deregister */
	misc_deregister(&wdt_miscdev);

	unregister_reboot_notifier(&wdt_notifier);
	release_region(WDT_START,1);
//	release_region(WDT_STOP,1);
}

static int __init sbc60xxwdt_init(void)
{
	int rc = -EBUSY;

//	We cannot reserve 0x45 - the kernel already has!
//	if (!request_region(WDT_STOP, 1, "SBC 60XX WDT"))
//		goto err_out;
	if (!request_region(WDT_START, 1, "SBC 60XX WDT"))
		goto err_out_region1;

	init_timer(&timer);
	timer.function = wdt_timer_ping;
	timer.data = 0;

	rc = misc_register(&wdt_miscdev);
	if (rc)
		goto err_out_region2;

	rc = register_reboot_notifier(&wdt_notifier);
	if (rc)
		goto err_out_miscdev;

	printk(KERN_INFO OUR_NAME ": WDT driver for 60XX single board computer initialised.\n");
	
	return 0;

err_out_miscdev:
	misc_deregister(&wdt_miscdev);
err_out_region2:
	release_region(WDT_START,1);
err_out_region1:
	release_region(WDT_STOP,1);
/* err_out: */
	return rc;
}

module_init(sbc60xxwdt_init);
module_exit(sbc60xxwdt_unload);

MODULE_LICENSE("GPL");
EXPORT_NO_SYMBOLS;
