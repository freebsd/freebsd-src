/*
 *	Industrial Computer Source WDT500/501 driver for Linux 2.1.x
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
 *	Release 0.08.
 *
 *	Fixes
 *		Dave Gregorich	:	Modularisation and minor bugs
 *		Alan Cox	:	Added the watchdog ioctl() stuff
 *		Alan Cox	:	Fixed the reboot problem (as noted by
 *					Matt Crocker).
 *		Alan Cox	:	Added wdt= boot option
 *		Alan Cox	:	Cleaned up copy/user stuff
 *		Tim Hockin	:	Added insmod parameters, comment cleanup
 *					Parameterized timeout
 *		Tigran Aivazian	:	Restructured wdt_init() to handle failures
 *		Joel Becker	:	Added WDIOC_GET/SETTIMEOUT
 *		Matt Domsch	:	Added nowayout module option
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/smp_lock.h>
#include <linux/miscdevice.h>
#include <linux/watchdog.h>
#include "wd501p.h"
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/fcntl.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/init.h>

static unsigned long wdt_is_open;
static int expect_close;

/*
 *	You must set these - there is no sane way to probe for this board.
 *	You can use wdt=x,y to set these now.
 */
 
static int io=0x240;
static int irq=11;

/* Default margin */
#define WD_TIMO (100*60)		/* 1 minute */

static int wd_margin = WD_TIMO;

#ifdef CONFIG_WATCHDOG_NOWAYOUT
static int nowayout = 1;
#else
static int nowayout = 0;
#endif

MODULE_PARM(nowayout,"i");
MODULE_PARM_DESC(nowayout, "Watchdog cannot be stopped once started (default=CONFIG_WATCHDOG_NOWAYOUT)");

#ifndef MODULE

/**
 *	wdt_setup:
 *	@str: command line string
 *
 *	Setup options. The board isn't really probe-able so we have to
 *	get the user to tell us the configuration. Sane people build it 
 *	modular but the others come here.
 */
 
static int __init wdt_setup(char *str)
{
	int ints[4];

	str = get_options (str, ARRAY_SIZE(ints), ints);

	if (ints[0] > 0)
	{
		io = ints[1];
		if(ints[0] > 1)
			irq = ints[2];
	}

	return 1;
}

__setup("wdt=", wdt_setup);

#endif /* !MODULE */

MODULE_PARM(io, "i");
MODULE_PARM_DESC(io, "WDT io port (default=0x240)");
MODULE_PARM(irq, "i");
MODULE_PARM_DESC(irq, "WDT irq (default=11)");
 
/*
 *	Programming support
 */
 
static void wdt_ctr_mode(int ctr, int mode)
{
	ctr<<=6;
	ctr|=0x30;
	ctr|=(mode<<1);
	outb_p(ctr, WDT_CR);
}

static void wdt_ctr_load(int ctr, int val)
{
	outb_p(val&0xFF, WDT_COUNT0+ctr);
	outb_p(val>>8, WDT_COUNT0+ctr);
}

/*
 *	Kernel methods.
 */
 
 
/**
 *	wdt_status:
 *	
 *	Extract the status information from a WDT watchdog device. There are
 *	several board variants so we have to know which bits are valid. Some
 *	bits default to one and some to zero in order to be maximally painful.
 *
 *	we then map the bits onto the status ioctl flags.
 */
 
static int wdt_status(void)
{
	/*
	 *	Status register to bit flags
	 */
	 
	int flag=0;
	unsigned char status=inb_p(WDT_SR);
	status|=FEATUREMAP1;
	status&=~FEATUREMAP2;	
	
	if(!(status&WDC_SR_TGOOD))
		flag|=WDIOF_OVERHEAT;
	if(!(status&WDC_SR_PSUOVER))
		flag|=WDIOF_POWEROVER;
	if(!(status&WDC_SR_PSUUNDR))
		flag|=WDIOF_POWERUNDER;
	if(!(status&WDC_SR_FANGOOD))
		flag|=WDIOF_FANFAULT;
	if(status&WDC_SR_ISOI0)
		flag|=WDIOF_EXTERN1;
	if(status&WDC_SR_ISII1)
		flag|=WDIOF_EXTERN2;
	return flag;
}

/**
 *	wdt_interrupt:
 *	@irq:		Interrupt number
 *	@dev_id:	Unused as we don't allow multiple devices.
 *	@regs:		Unused.
 *
 *	Handle an interrupt from the board. These are raised when the status
 *	map changes in what the board considers an interesting way. That means
 *	a failure condition occuring.
 */
 
void wdt_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	/*
	 *	Read the status register see what is up and
	 *	then printk it. 
	 */
	 
	unsigned char status=inb_p(WDT_SR);
	
	status|=FEATUREMAP1;
	status&=~FEATUREMAP2;	
	
	printk(KERN_CRIT "WDT status %d\n", status);
	
	if(!(status&WDC_SR_TGOOD))
		printk(KERN_CRIT "Overheat alarm.(%d)\n",inb_p(WDT_RT));
	if(!(status&WDC_SR_PSUOVER))
		printk(KERN_CRIT "PSU over voltage.\n");
	if(!(status&WDC_SR_PSUUNDR))
		printk(KERN_CRIT "PSU under voltage.\n");
	if(!(status&WDC_SR_FANGOOD))
		printk(KERN_CRIT "Possible fan fault.\n");
	if(!(status&WDC_SR_WCCR))
#ifdef SOFTWARE_REBOOT
#ifdef ONLY_TESTING
		printk(KERN_CRIT "Would Reboot.\n");
#else		
		printk(KERN_CRIT "Initiating system reboot.\n");
		machine_restart(NULL);
#endif		
#else
		printk(KERN_CRIT "Reset in 5ms.\n");
#endif		
}


/**
 *	wdt_ping:
 *
 *	Reload counter one with the watchdog timeout. We don't bother reloading
 *	the cascade counter. 
 */
 
static void wdt_ping(void)
{
	/* Write a watchdog value */
	inb_p(WDT_DC);
	wdt_ctr_mode(1,2);
	wdt_ctr_load(1,wd_margin);		/* Timeout */
	outb_p(0, WDT_DC);
}

/**
 *	wdt_write:
 *	@file: file handle to the watchdog
 *	@buf: buffer to write (unused as data does not matter here 
 *	@count: count of bytes
 *	@ppos: pointer to the position to write. No seeks allowed
 *
 *	A write to a watchdog device is defined as a keepalive signal. Any
 *	write of data will do, as we we don't define content meaning.
 */
 
static ssize_t wdt_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	/*  Can't seek (pwrite) on this device  */
	if (ppos != &file->f_pos)
		return -ESPIPE;

	if(count)
	{
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
		wdt_ping();
		return 1;
	}
	return 0;
}

/**
 *	wdt_read:
 *	@file: file handle to the watchdog board
 *	@buf: buffer to write 1 byte into
 *	@count: length of buffer
 *	@ptr: offset (no seek allowed)
 *
 *	Read reports the temperature in degrees Fahrenheit. The API is in
 *	farenheit. It was designed by an imperial measurement luddite.
 */
 
static ssize_t wdt_read(struct file *file, char *buf, size_t count, loff_t *ptr)
{
	unsigned short c=inb_p(WDT_RT);
	unsigned char cp;
	
	/*  Can't seek (pread) on this device  */
	if (ptr != &file->f_pos)
		return -ESPIPE;

	switch(MINOR(file->f_dentry->d_inode->i_rdev))
	{
		case TEMP_MINOR:
			c*=11;
			c/=15;
			cp=c+7;
			if(copy_to_user(buf,&cp,1))
				return -EFAULT;
			return 1;
		default:
			return -EINVAL;
	}
}

/**
 *	wdt_ioctl:
 *	@inode: inode of the device
 *	@file: file handle to the device
 *	@cmd: watchdog command
 *	@arg: argument pointer
 *
 *	The watchdog API defines a common set of functions for all watchdogs
 *	according to their available features. We only actually usefully support
 *	querying capabilities and current status. 
 */
 
static int wdt_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
	unsigned long arg)
{
	int new_margin;

	static struct watchdog_info ident=
	{
		WDIOF_OVERHEAT|WDIOF_POWERUNDER|WDIOF_POWEROVER
			|WDIOF_EXTERN1|WDIOF_EXTERN2|WDIOF_FANFAULT
			|WDIOF_SETTIMEOUT|WDIOF_MAGICCLOSE,
		1,
		"WDT500/501"
	};
	
	ident.options&=WDT_OPTION_MASK;	/* Mask down to the card we have */
	switch(cmd)
	{
		default:
			return -ENOTTY;
		case WDIOC_GETSUPPORT:
			return copy_to_user((struct watchdog_info *)arg, &ident, sizeof(ident))?-EFAULT:0;

		case WDIOC_GETSTATUS:
			return put_user(wdt_status(),(int *)arg);
		case WDIOC_GETBOOTSTATUS:
			return put_user(0, (int *)arg);
		case WDIOC_KEEPALIVE:
			wdt_ping();
			return 0;
		case WDIOC_SETTIMEOUT:
			if (get_user(new_margin, (int *)arg))
				return -EFAULT;
			/* Arbitrary, can't find the card's limits */
			if ((new_margin < 0) || (new_margin > 60))
				return -EINVAL;
			wd_margin = new_margin * 100;
			wdt_ping();
			/* Fall */
		case WDIOC_GETTIMEOUT:
			return put_user(wd_margin / 100, (int *)arg);
	}
}

/**
 *	wdt_open:
 *	@inode: inode of device
 *	@file: file handle to device
 *
 *	One of our two misc devices has been opened. The watchdog device is
 *	single open and on opening we load the counters. Counter zero is a 
 *	100Hz cascade, into counter 1 which downcounts to reboot. When the
 *	counter triggers counter 2 downcounts the length of the reset pulse
 *	which set set to be as long as possible. 
 */
 
static int wdt_open(struct inode *inode, struct file *file)
{
	switch(MINOR(inode->i_rdev))
	{
		case WATCHDOG_MINOR:
			if(test_and_set_bit(0, &wdt_is_open))
				return -EBUSY;
			/*
			 *	Activate 
			 */
	 
			wdt_is_open=1;
			inb_p(WDT_DC);		/* Disable */
			wdt_ctr_mode(0,3);
			wdt_ctr_mode(1,2);
			wdt_ctr_mode(2,0);
			wdt_ctr_load(0, 8948);		/* count at 100Hz */
			wdt_ctr_load(1,wd_margin);	/* Timeout 120 seconds */
			wdt_ctr_load(2,65535);
			outb_p(0, WDT_DC);	/* Enable */
			return 0;
		case TEMP_MINOR:
			return 0;
		default:
			return -ENODEV;
	}
}

/**
 *	wdt_close:
 *	@inode: inode to board
 *	@file: file handle to board
 *
 *	The watchdog has a configurable API. There is a religious dispute 
 *	between people who want their watchdog to be able to shut down and 
 *	those who want to be sure if the watchdog manager dies the machine
 *	reboots. In the former case we disable the counters, in the latter
 *	case you have to open it again very soon.
 */
 
static int wdt_release(struct inode *inode, struct file *file)
{
	if(MINOR(inode->i_rdev)==WATCHDOG_MINOR)
	{
		if (expect_close) {
			inb_p(WDT_DC);		/* Disable counters */
			wdt_ctr_load(2,0);	/* 0 length reset pulses now */
		} else {
			printk(KERN_CRIT "wdt: WDT device closed unexpectedly.  WDT will not stop!\n");
		}
		clear_bit(0, &wdt_is_open);
	}
	return 0;
}

/**
 *	notify_sys:
 *	@this: our notifier block
 *	@code: the event being reported
 *	@unused: unused
 *
 *	Our notifier is called on system shutdowns. We want to turn the card
 *	off at reboot otherwise the machine will reboot again during memory
 *	test or worse yet during the following fsck. This would suck, in fact
 *	trust me - if it happens it does suck.
 */

static int wdt_notify_sys(struct notifier_block *this, unsigned long code,
	void *unused)
{
	if(code==SYS_DOWN || code==SYS_HALT)
	{
		/* Turn the card off */
		inb_p(WDT_DC);
		wdt_ctr_load(2,0);
	}
	return NOTIFY_DONE;
}
 
/*
 *	Kernel Interfaces
 */
 
 
static struct file_operations wdt_fops = {
	owner:		THIS_MODULE,
	llseek:		no_llseek,
	read:		wdt_read,
	write:		wdt_write,
	ioctl:		wdt_ioctl,
	open:		wdt_open,
	release:	wdt_release,
};

static struct miscdevice wdt_miscdev=
{
	WATCHDOG_MINOR,
	"watchdog",
	&wdt_fops
};

#ifdef CONFIG_WDT_501
static struct miscdevice temp_miscdev=
{
	TEMP_MINOR,
	"temperature",
	&wdt_fops
};
#endif

/*
 *	The WDT card needs to learn about soft shutdowns in order to
 *	turn the timebomb registers off. 
 */
 
static struct notifier_block wdt_notifier=
{
	wdt_notify_sys,
	NULL,
	0
};

/**
 *	cleanup_module:
 *
 *	Unload the watchdog. You cannot do this with any file handles open.
 *	If your watchdog is set to continue ticking on close and you unload
 *	it, well it keeps ticking. We won't get the interrupt but the board
 *	will not touch PC memory so all is fine. You just have to load a new
 *	module in 60 seconds or reboot.
 */
 
static void __exit wdt_exit(void)
{
	misc_deregister(&wdt_miscdev);
#ifdef CONFIG_WDT_501	
	misc_deregister(&temp_miscdev);
#endif	
	unregister_reboot_notifier(&wdt_notifier);
	release_region(io,8);
	free_irq(irq, NULL);
}

/**
 * 	wdt_init:
 *
 *	Set up the WDT watchdog board. All we have to do is grab the
 *	resources we require and bitch if anyone beat us to them.
 *	The open() function will actually kick the board off.
 */
 
static int __init wdt_init(void)
{
	int ret;

	ret = misc_register(&wdt_miscdev);
	if (ret) {
		printk(KERN_ERR "wdt: can't misc_register on minor=%d\n", WATCHDOG_MINOR);
		goto out;
	}
	ret = request_irq(irq, wdt_interrupt, SA_INTERRUPT, "wdt501p", NULL);
	if(ret) {
		printk(KERN_ERR "wdt: IRQ %d is not free.\n", irq);
		goto outmisc;
	}
	if (!request_region(io, 8, "wdt501p")) {
		printk(KERN_ERR "wdt: IO %X is not free.\n", io);
		ret = -EBUSY;
		goto outirq;
	}
	ret = register_reboot_notifier(&wdt_notifier);
	if(ret) {
		printk(KERN_ERR "wdt: can't register reboot notifier (err=%d)\n", ret);
		goto outreg;
	}

#ifdef CONFIG_WDT_501
	ret = misc_register(&temp_miscdev);
	if (ret) {
		printk(KERN_ERR "wdt: can't misc_register (temp) on minor=%d\n", TEMP_MINOR);
		goto outrbt;
	}
#endif

	ret = 0;
	printk(KERN_INFO "WDT500/501-P driver 0.07 at %X (Interrupt %d)\n", io, irq);
out:
	return ret;

#ifdef CONFIG_WDT_501
outrbt:
	unregister_reboot_notifier(&wdt_notifier);
#endif

outreg:
	release_region(io,8);
outirq:
	free_irq(irq, NULL);
outmisc:
	misc_deregister(&wdt_miscdev);
	goto out;
}

module_init(wdt_init);
module_exit(wdt_exit);

MODULE_AUTHOR("Alan Cox");
MODULE_DESCRIPTION("Driver for ISA ICS watchdog cards (WDT500/501)");
MODULE_LICENSE("GPL");
EXPORT_NO_SYMBOLS;
