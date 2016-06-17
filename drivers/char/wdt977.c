/*
 *	Wdt977	0.02:	A Watchdog Device for Netwinder W83977AF chip
 *
 *	(c) Copyright 1998 Rebel.com (Woody Suwalski <woody@netwinder.org>)
 *
 *			-----------------------
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 *
 *			-----------------------
 *      14-Dec-2001 Matt Domsch <Matt_Domsch@dell.com>
 *           Added nowayout module option to override CONFIG_WATCHDOG_NOWAYOUT
 */
 
#include <linux/module.h>
#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/init.h>
#include <linux/smp_lock.h>

#include <asm/io.h>
#include <asm/system.h>
#include <asm/mach-types.h>

#define WATCHDOG_MINOR	130

static	int timeout = 3;
static	int timer_alive;
static	int testmode;
static	int expect_close = 0;

#ifdef CONFIG_WATCHDOG_NOWAYOUT
static int nowayout = 1;
#else
static int nowayout = 0;
#endif

MODULE_PARM(nowayout,"i");
MODULE_PARM_DESC(nowayout, "Watchdog cannot be stopped once started (default=CONFIG_WATCHDOG_NOWAYOUT)");


/*
 *	Allow only one person to hold it open
 */
 
static int wdt977_open(struct inode *inode, struct file *file)
{
	if(timer_alive)
		return -EBUSY;
	if (nowayout) {
		MOD_INC_USE_COUNT;
	}
	timer_alive++;

	//max timeout value = 255 minutes (0xFF). Write 0 to disable WatchDog.
	if (timeout>255)
	    timeout = 255;

	printk(KERN_INFO "Watchdog: active, current timeout %d min.\n",timeout);

	// unlock the SuperIO chip
	outb(0x87,0x370); 
	outb(0x87,0x370); 
	
	//select device Aux2 (device=8) and set watchdog regs F2, F3 and F4
	//F2 has the timeout in minutes
	//F3 could be set to the POWER LED blink (with GP17 set to PowerLed)
	//   at timeout, and to reset timer on kbd/mouse activity (not now)
	//F4 is used to just clear the TIMEOUT'ed state (bit 0)
	
	outb(0x07,0x370);
	outb(0x08,0x371);
	outb(0xF2,0x370);
	outb(timeout,0x371);
	outb(0xF3,0x370);
	outb(0x00,0x371);	//another setting is 0E for kbd/mouse/LED
	outb(0xF4,0x370);
	outb(0x00,0x371);
	
	//at last select device Aux1 (dev=7) and set GP16 as a watchdog output
	if (!testmode)
	{
		outb(0x07,0x370);
		outb(0x07,0x371);
		outb(0xE6,0x370);
		outb(0x08,0x371);
	}
		
	// lock the SuperIO chip
	outb(0xAA,0x370); 

	return 0;
}

static int wdt977_release(struct inode *inode, struct file *file)
{
	/*
	 *	Shut off the timer.
	 * 	Lock it in if it's a module and we set nowayout
	 */
	lock_kernel();
	if (expect_close) {

		// unlock the SuperIO chip
		outb(0x87,0x370); 
		outb(0x87,0x370); 
	
		//select device Aux2 (device=8) and set watchdog regs F2,F3 and F4
		//F3 is reset to its default state
		//F4 can clear the TIMEOUT'ed state (bit 0) - back to default
		//We can not use GP17 as a PowerLed, as we use its usage as a RedLed
	
		outb(0x07,0x370);
		outb(0x08,0x371);
		outb(0xF2,0x370);
		outb(0xFF,0x371);
		outb(0xF3,0x370);
		outb(0x00,0x371);
		outb(0xF4,0x370);
		outb(0x00,0x371);
		outb(0xF2,0x370);
		outb(0x00,0x371);
	
		//at last select device Aux1 (dev=7) and set GP16 as a watchdog output
		outb(0x07,0x370);
		outb(0x07,0x371);
		outb(0xE6,0x370);
		outb(0x08,0x371);
	
		// lock the SuperIO chip
		outb(0xAA,0x370);
		printk(KERN_INFO "Watchdog: shutdown.\n");
	} else {
		printk(KERN_CRIT "WDT device closed unexpectedly.  WDT will not stop!\n");
	}

	timer_alive=0;
	unlock_kernel();

	return 0;
}

static ssize_t wdt977_write(struct file *file, const char *data, size_t len, loff_t *ppos)
{
	if (!nowayout) {
		size_t i;

		/* In case it was set long ago */
		expect_close = 0;

		for (i = 0; i != len; i++) {
			char c;
			if (get_user(c, data + i))
				return -EFAULT;
			if (c == 'V')
				expect_close = 1;
		}
	}

	//max timeout value = 255 minutes (0xFF). Write 0 to disable WatchDog.
	if (timeout>255)
	    timeout = 255;

	/*
	 *	Refresh the timer.
	 */
		
	//we have a hw bug somewhere, so each 977 minute is actually only 30sec
	//as such limit the max timeout to half of max of 255 minutes...
//	if (timeout>126)
//	    timeout = 126;
	
	// unlock the SuperIO chip
	outb(0x87,0x370); 
	outb(0x87,0x370); 
	
	//select device Aux2 (device=8) and kicks watchdog reg F2
	//F2 has the timeout in minutes
	
	outb(0x07,0x370);
	outb(0x08,0x371);
	outb(0xF2,0x370);
	outb(timeout,0x371);
	
	// lock the SuperIO chip
	outb(0xAA,0x370); 
	
	return 1;
}

static struct file_operations wdt977_fops=
{
	owner:		THIS_MODULE,
	write:		wdt977_write,
	open:		wdt977_open,
	release:	wdt977_release,
};

static struct miscdevice wdt977_miscdev=
{
	WATCHDOG_MINOR,
	"watchdog",
	&wdt977_fops
};

static int __init nwwatchdog_init(void)
{
	if (!machine_is_netwinder())
		return -ENODEV;

	misc_register(&wdt977_miscdev);
	printk(KERN_INFO "NetWinder Watchdog sleeping.\n");
	return 0;
}	

static void __exit nwwatchdog_exit(void)
{
	misc_deregister(&wdt977_miscdev);
}

EXPORT_NO_SYMBOLS;

module_init(nwwatchdog_init);
module_exit(nwwatchdog_exit);

MODULE_LICENSE("GPL");
