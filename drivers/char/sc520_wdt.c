/*
 *	AMD Elan SC520 processor Watchdog Timer driver for Linux 2.4.x
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
 *           9/27 - 2001      [Initial release]
 *	
 *	Additional fixes Alan Cox
 *	-	Fixed formatting
 *	-	Removed debug printks
 *	-	Fixed SMP built kernel deadlock
 *	-	Switched to private locks not lock_kernel
 *	-	Used ioremap/writew/readw
 *	-	Added NOWAYOUT support
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
 *
 *  This driver uses memory mapped IO, and spinlock.
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

/*
 * The SC520 can timeout anywhere from 492us to 32.21s.
 * If we reset the watchdog every ~250ms we should be safe.
 */

#define WDT_INTERVAL (HZ/4+1)

/*
 * We must not require too good response from the userspace daemon.
 * Here we require the userspace daemon to send us a heartbeat
 * char to /dev/watchdog every 30 seconds.
 */

#define WDT_HEARTBEAT (HZ * 30)

/*
 * AMD Elan SC520 timeout value is 492us times a power of 2 (0-7)
 *
 *   0: 492us    2: 1.01s    4: 4.03s   6: 16.22s
 *   1: 503ms    3: 2.01s    5: 8.05s   7: 32.21s
 */

#define TIMEOUT_EXPONENT ( 1 << 3 )  /* 0x08 = 2.01s */

/* #define MMCR_BASE_DEFAULT 0xfffef000 */
#define MMCR_BASE_DEFAULT ((__u16 *)0xffffe)
#define OFFS_WDTMRCTL ((unsigned int)0xcb0)
#define WDT_ENB 0x8000		/* [15] Watchdog Timer Enable */
#define WDT_WRST_ENB 0x4000	/* [14] Watchdog Timer Reset Enable */

#define OUR_NAME "sc520_wdt"

#define WRT_DOG(data) *wdtmrctl=data

static __u16 *wdtmrctl;

static void wdt_timer_ping(unsigned long);
static struct timer_list timer;
static unsigned long next_heartbeat;
static unsigned long wdt_is_open;
static int wdt_expect_close;

#ifdef CONFIG_WATCHDOG_NOWAYOUT
static int nowayout = 1;
#else
static int nowayout = 0;
#endif

MODULE_PARM(nowayout,"i");
MODULE_PARM_DESC(nowayout, "Watchdog cannot be stopped once started (default=CONFIG_WATCHDOG_NOWAYOUT)");

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
		writew(0xAAAA, wdtmrctl);
		writew(0x5555, wdtmrctl);
		spin_unlock(&wdt_spinlock);

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

static void wdt_config(int writeval)
{
	__u16 dummy;
	unsigned long flags;

	/* buy some time (ping) */
	spin_lock_irqsave(&wdt_spinlock, flags);
	dummy=readw(wdtmrctl);  /* ensure write synchronization */
	writew(0xAAAA, wdtmrctl);
	writew(0x5555, wdtmrctl);
	/* make WDT configuration register writable one time */
	writew(0x3333, wdtmrctl);
	writew(0xCCCC, wdtmrctl);
	/* write WDT configuration register */
	writew(writeval, wdtmrctl);
	spin_unlock_irqrestore(&wdt_spinlock, flags);
}

static void wdt_startup(void)
{
	next_heartbeat = jiffies + WDT_HEARTBEAT;

	/* Start the timer */
	timer.expires = jiffies + WDT_INTERVAL;	
	add_timer(&timer);

	wdt_config(WDT_ENB | WDT_WRST_ENB | TIMEOUT_EXPONENT);
	printk(OUR_NAME ": Watchdog timer is now enabled.\n");  
}

static void wdt_turnoff(void)
{
	if (!nowayout) {
		/* Stop the timer */
		del_timer(&timer);
		wdt_config(0);
		printk(OUR_NAME ": Watchdog timer is now disabled...\n");
	}
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
		for(ofs = 0; ofs != count; ofs++) {
			char c;
			if (get_user(c, buf + ofs))
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

static int fop_open(struct inode * inode, struct file * file)
{
	switch(MINOR(inode->i_rdev)) 
	{
		case WATCHDOG_MINOR:
			/* Just in case we're already talking to someone... */
			if(test_and_set_bit(0, &wdt_is_open))
				return -EBUSY;
			/* Good, fire up the show */
			wdt_startup();
			if (nowayout) {
				MOD_INC_USE_COUNT;
			}
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

static long long fop_llseek(struct file *file, long long offset, int origin)
{
	return -ESPIPE;
}

static int fop_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
	unsigned long arg)
{
	static struct watchdog_info ident=
	{
		WDIOF_MAGICCLOSE,
		1,
		"SC520"
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
	llseek:		fop_llseek,
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

static void __exit sc520_wdt_unload(void)
{
	wdt_turnoff();

	/* Deregister */
	misc_deregister(&wdt_miscdev);
	iounmap(wdtmrctl);
	unregister_reboot_notifier(&wdt_notifier);
}

static int __init sc520_wdt_init(void)
{
	int rc = -EBUSY;
	unsigned long cbar;

	spin_lock_init(&wdt_spinlock);

	init_timer(&timer);
	timer.function = wdt_timer_ping;
	timer.data = 0;

	rc = misc_register(&wdt_miscdev);
	if (rc)
		goto err_out_region2;

	rc = register_reboot_notifier(&wdt_notifier);
	if (rc)
		goto err_out_miscdev;

	/* get the Base Address Register */
	cbar = inl_p(0xfffc);
	printk(OUR_NAME ": CBAR: 0x%08lx\n", cbar);
	/* check if MMCR aliasing bit is set */
	if (cbar & 0x80000000) {
		printk(OUR_NAME ": MMCR Aliasing enabled.\n");
		wdtmrctl = (__u16 *)(cbar & 0x3fffffff);
	} else {
		printk(OUR_NAME "!!! WARNING !!!\n"
		  "\t MMCR Aliasing found NOT enabled!\n"
		  "\t Using default value of: %p\n"
		  "\t This has not been tested!\n"
		  "\t Please email Scott Jennings <smj@oro.net>\n"
		  "\t  and Bill Jennings <bj@oro.net> if it works!\n"
		  , MMCR_BASE_DEFAULT
		  );
	  wdtmrctl = MMCR_BASE_DEFAULT;
	}

	wdtmrctl = (__u16 *)((char *)wdtmrctl + OFFS_WDTMRCTL);
	wdtmrctl = ioremap((unsigned long)wdtmrctl, 2);
	printk(KERN_INFO OUR_NAME ": WDT driver for SC520 initialised.\n");

	return 0;

err_out_miscdev:
	misc_deregister(&wdt_miscdev);
err_out_region2:
	return rc;
}

module_init(sc520_wdt_init);
module_exit(sc520_wdt_unload);

MODULE_AUTHOR("Scott and Bill Jennings");
MODULE_DESCRIPTION("Driver for watchdog timer in AMD \"Elan\" SC520 uProcessor");
MODULE_LICENSE("GPL");
EXPORT_NO_SYMBOLS;
