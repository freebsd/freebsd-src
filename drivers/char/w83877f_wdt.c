/*
 *	W83877F Computer Watchdog Timer driver for Linux 2.4.x
 *
 *      Based on acquirewdt.c by Alan Cox,
 *           and sbc60xxwdt.c by Jakob Oestergaard <jakob@ostenfeld.dk>
 *     
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 *	
 *	The authors do NOT admit liability nor provide warranty for 
 *	any of this software. This material is provided "AS-IS" in 
 *      the hope that it may be useful for others.
 *
 *	(c) Copyright 2001    Scott Jennings <linuxdrivers@oro.net>
 *
 *           4/19 - 2001      [Initial revision]
 *           9/27 - 2001      Added spinlocking
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
 *  This WDT driver is different from most other Linux WDT
 *  drivers in that the driver will ping the watchdog by itself,
 *  because this particular WDT has a very short timeout (1.6
 *  seconds) and it would be insane to count on any userspace
 *  daemon always getting scheduled within that time frame.
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

#define OUR_NAME "w83877f_wdt"

#define ENABLE_W83877F_PORT 0x3F0
#define ENABLE_W83877F 0x87
#define DISABLE_W83877F 0xAA
#define WDT_PING 0x443
#define WDT_REGISTER 0x14
#define WDT_ENABLE 0x9C
#define WDT_DISABLE 0x8C

/*
 * The W83877F seems to be fixed at 1.6s timeout (at least on the
 * EMACS PC-104 board I'm using). If we reset the watchdog every
 * ~250ms we should be safe.  */

#define WDT_INTERVAL (HZ/4+1)

/*
 * We must not require too good response from the userspace daemon.
 * Here we require the userspace daemon to send us a heartbeat
 * char to /dev/watchdog every 30 seconds.
 */

#define WDT_HEARTBEAT (HZ * 30)

static void wdt_timer_ping(unsigned long);
static struct timer_list timer;
static unsigned long next_heartbeat;
static unsigned long wdt_is_open;
static int wdt_expect_close;
static spinlock_t wdt_spinlock;

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
		/* Ping the WDT */
		spin_lock(&wdt_spinlock);

		/* Ping the WDT by reading from WDT_PING */
		inb_p(WDT_PING);

		/* Re-set the timer interval */
		timer.expires = jiffies + WDT_INTERVAL;
		add_timer(&timer);

		spin_unlock(&wdt_spinlock);

	} else {
		printk(OUR_NAME ": Heartbeat lost! Will not ping the watchdog\n");
	}
}

/* 
 * Utility routines
 */

static void wdt_change(int writeval)
{
	unsigned long flags;
	spin_lock_irqsave(&wdt_spinlock, flags);

	/* buy some time */
	inb_p(WDT_PING);

	/* make W83877F available */
	outb_p(ENABLE_W83877F,  ENABLE_W83877F_PORT);
	outb_p(ENABLE_W83877F,  ENABLE_W83877F_PORT);

	/* enable watchdog */
	outb_p(WDT_REGISTER,    ENABLE_W83877F_PORT);
	outb_p(writeval,        ENABLE_W83877F_PORT+1);

	/* lock the W8387FF away */
	outb_p(DISABLE_W83877F, ENABLE_W83877F_PORT);

	spin_unlock_irqrestore(&wdt_spinlock, flags);
}

static void wdt_startup(void)
{
	next_heartbeat = jiffies + WDT_HEARTBEAT;

	/* Start the timer */
	timer.expires = jiffies + WDT_INTERVAL;	
	add_timer(&timer);

	wdt_change(WDT_ENABLE);

	printk(OUR_NAME ": Watchdog timer is now enabled.\n");  
}

static void wdt_turnoff(void)
{
	/* Stop the timer */
	del_timer(&timer);

	wdt_change(WDT_DISABLE);

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
			if(get_user(c, buf + ofs))
				return -EFAULT;
			if(c == 'V')
				wdt_expect_close = 1;
		}

		/* someone wrote to us, we should restart timer */
		next_heartbeat = jiffies + WDT_HEARTBEAT;
		return 1;
	};
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
			if(test_and_set_bit(0, &wdt_is_open)) {
				spin_unlock(&wdt_spinlock);
				return -EBUSY;
			}
			/* Good, fire up the show */
			wdt_startup();
			return 0;

		default:
			return -ENODEV;
	}
}

static int fop_close(struct inode * inode, struct file * file)
{
	if(MINOR(inode->i_rdev) == WATCHDOG_MINOR) 
	{
		if(wdt_expect_close)
			wdt_turnoff();
		else {
			del_timer(&timer);
			printk(OUR_NAME ": device file closed unexpectedly. Will not stop the WDT!\n");
		}
	}
	clear_bit(0, &wdt_is_open);
	return 0;
}

static int fop_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	static struct watchdog_info ident=
	{
		WDIOF_MAGICCLOSE,
		1,
		"W83877F"
	};
	
	switch(cmd)
	{
		default:
			return -ENOIOCTLCMD;
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

static void __exit w83877f_wdt_unload(void)
{
	wdt_turnoff();

	/* Deregister */
	misc_deregister(&wdt_miscdev);

	unregister_reboot_notifier(&wdt_notifier);
	release_region(WDT_PING,1);
	release_region(ENABLE_W83877F_PORT,2);
}

static int __init w83877f_wdt_init(void)
{
	int rc = -EBUSY;

	spin_lock_init(&wdt_spinlock);

	if (!request_region(ENABLE_W83877F_PORT, 2, "W83877F WDT"))
		goto err_out;
	if (!request_region(WDT_PING, 1, "W8387FF WDT"))
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

	printk(KERN_INFO OUR_NAME ": WDT driver for W83877F initialised.\n");
	
	return 0;

err_out_miscdev:
	misc_deregister(&wdt_miscdev);
err_out_region2:
	release_region(WDT_PING,1);
err_out_region1:
	release_region(ENABLE_W83877F_PORT,2);
err_out:
	return rc;
}

module_init(w83877f_wdt_init);
module_exit(w83877f_wdt_unload);

MODULE_AUTHOR("Scott and Bill Jennings");
MODULE_DESCRIPTION("Driver for watchdog timer in w83877f chip");
MODULE_LICENSE("GPL");
EXPORT_NO_SYMBOLS;
