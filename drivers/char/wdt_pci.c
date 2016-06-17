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
 *		JP Nollmann	:	Added support for PCI wdt501p
 *		Alan Cox	:	Split ISA and PCI cards into two drivers
 *		Jeff Garzik	:	PCI cleanups
 *		Tigran Aivazian	:	Restructured wdtpci_init_one() to handle failures
 *		Joel Becker	:	Added WDIOC_GET/SETTIMEOUT
 *		Zwane Mwaikambo :	Magic char closing, locking changes, cleanups
 *		Matt Domsch	:	nowayout module option
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
#define WDT_IS_PCI
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
#include <linux/spinlock.h>
#include <asm/semaphore.h>

#include <linux/pci.h>

#define PFX "wdt_pci: "

/*
 * Until Access I/O gets their application for a PCI vendor ID approved,
 * I don't think that it's appropriate to move these constants into the
 * regular pci_ids.h file. -- JPN 2000/01/18
 */

#ifndef PCI_VENDOR_ID_ACCESSIO
#define PCI_VENDOR_ID_ACCESSIO 0x494f
#endif
#ifndef PCI_DEVICE_ID_WDG_CSM
#define PCI_DEVICE_ID_WDG_CSM 0x22c0
#endif

static struct semaphore open_sem;
static spinlock_t wdtpci_lock;
static int expect_close = 0;

static int io;
static int irq;

/* Default timeout */
#define WD_TIMO (100*60)		/* 1 minute */
#define WD_TIMO_MAX (WD_TIMO*60)	/* 1 hour(?) */

static int wd_margin = WD_TIMO;

#ifdef CONFIG_WATCHDOG_NOWAYOUT
static int nowayout = 1;
#else
static int nowayout = 0;
#endif

MODULE_PARM(nowayout,"i");
MODULE_PARM_DESC(nowayout, "Watchdog cannot be stopped once started (default=CONFIG_WATCHDOG_NOWAYOUT)");

/*
 *	Programming support
 */
 
static void wdtpci_ctr_mode(int ctr, int mode)
{
	ctr<<=6;
	ctr|=0x30;
	ctr|=(mode<<1);
	outb_p(ctr, WDT_CR);
}

static void wdtpci_ctr_load(int ctr, int val)
{
	outb_p(val&0xFF, WDT_COUNT0+ctr);
	outb_p(val>>8, WDT_COUNT0+ctr);
}

/*
 *	Kernel methods.
 */
 
 
/**
 *	wdtpci_status:
 *	
 *	Extract the status information from a WDT watchdog device. There are
 *	several board variants so we have to know which bits are valid. Some
 *	bits default to one and some to zero in order to be maximally painful.
 *
 *	we then map the bits onto the status ioctl flags.
 */
 
static int wdtpci_status(void)
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
 *	wdtpci_interrupt:
 *	@irq:		Interrupt number
 *	@dev_id:	Unused as we don't allow multiple devices.
 *	@regs:		Unused.
 *
 *	Handle an interrupt from the board. These are raised when the status
 *	map changes in what the board considers an interesting way. That means
 *	a failure condition occuring.
 */
 
static void wdtpci_interrupt(int irq, void *dev_id, struct pt_regs *regs)
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
 *	wdtpci_ping:
 *
 *	Reload counter one with the watchdog timeout. We don't bother reloading
 *	the cascade counter. 
 */
 
static void wdtpci_ping(void)
{
	unsigned long flags;

	/* Write a watchdog value */
	spin_lock_irqsave(&wdtpci_lock, flags);
	inb_p(WDT_DC);
	wdtpci_ctr_mode(1,2);
	wdtpci_ctr_load(1,wd_margin);		/* Timeout */
	outb_p(0, WDT_DC);
	spin_unlock_irqrestore(&wdtpci_lock, flags);
}

/**
 *	wdtpci_write:
 *	@file: file handle to the watchdog
 *	@buf: buffer to write (unused as data does not matter here 
 *	@count: count of bytes
 *	@ppos: pointer to the position to write. No seeks allowed
 *
 *	A write to a watchdog device is defined as a keepalive signal. Any
 *	write of data will do, as we we don't define content meaning.
 */
 
static ssize_t wdtpci_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	/*  Can't seek (pwrite) on this device  */
	if (ppos != &file->f_pos)
		return -ESPIPE;

	if (count) {
		if (!nowayout) {
			size_t i;

			expect_close = 0;

			for (i = 0; i != count; i++) {
				char c;
				if(get_user(c, buf+i))
					return -EFAULT;
				if (c == 'V')
					expect_close = 1;
			}
		}
		wdtpci_ping();
	}

	return count;
}

/**
 *	wdtpci_read:
 *	@file: file handle to the watchdog board
 *	@buf: buffer to write 1 byte into
 *	@count: length of buffer
 *	@ptr: offset (no seek allowed)
 *
 *	Read reports the temperature in degrees Fahrenheit. The API is in
 *	fahrenheit. It was designed by an imperial measurement luddite.
 */
 
static ssize_t wdtpci_read(struct file *file, char *buf, size_t count, loff_t *ptr)
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
 *	wdtpci_ioctl:
 *	@inode: inode of the device
 *	@file: file handle to the device
 *	@cmd: watchdog command
 *	@arg: argument pointer
 *
 *	The watchdog API defines a common set of functions for all watchdogs
 *	according to their available features. We only actually usefully support
 *	querying capabilities and current status. 
 */
 
static int wdtpci_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
	unsigned long arg)
{
	int new_margin;
	static struct watchdog_info ident=
	{
		WDIOF_OVERHEAT|WDIOF_POWERUNDER|WDIOF_POWEROVER
			|WDIOF_EXTERN1|WDIOF_EXTERN2|WDIOF_FANFAULT
			|WDIOF_SETTIMEOUT|WDIOF_MAGICCLOSE,
		1,
		"WDT500/501PCI"
	};
	
	ident.options&=WDT_OPTION_MASK;	/* Mask down to the card we have */
	switch(cmd)
	{
		default:
			return -ENOTTY;
		case WDIOC_GETSUPPORT:
			return copy_to_user((struct watchdog_info *)arg, &ident, sizeof(ident))?-EFAULT:0;

		case WDIOC_GETSTATUS:
			return put_user(wdtpci_status(),(int *)arg);
		case WDIOC_GETBOOTSTATUS:
			return put_user(0, (int *)arg);
		case WDIOC_KEEPALIVE:
			wdtpci_ping();
			return 0;
		case WDIOC_SETTIMEOUT:
			if (get_user(new_margin, (int *)arg))
				return -EFAULT;
			/* Arbitrary, can't find the card's limits */
			new_margin *= 100;
			if ((new_margin < 0) || (new_margin > WD_TIMO_MAX))
				return -EINVAL;
			wd_margin = new_margin;
			wdtpci_ping();
			/* Fall */
		case WDIOC_GETTIMEOUT:
			return put_user(wd_margin / 100, (int *)arg);
	}
}

/**
 *	wdtpci_open:
 *	@inode: inode of device
 *	@file: file handle to device
 *
 *	One of our two misc devices has been opened. The watchdog device is
 *	single open and on opening we load the counters. Counter zero is a 
 *	100Hz cascade, into counter 1 which downcounts to reboot. When the
 *	counter triggers counter 2 downcounts the length of the reset pulse
 *	which set set to be as long as possible. 
 */
 
static int wdtpci_open(struct inode *inode, struct file *file)
{
	unsigned long flags;

	switch(MINOR(inode->i_rdev))
	{
		case WATCHDOG_MINOR:
			if (down_trylock(&open_sem))
				return -EBUSY;

			if (nowayout) {
				MOD_INC_USE_COUNT;
			}
			/*
			 *	Activate 
			 */
			spin_lock_irqsave(&wdtpci_lock, flags);
			
			inb_p(WDT_DC);		/* Disable */

			/*
			 * "pet" the watchdog, as Access says.
			 * This resets the clock outputs.
			 */
				
			wdtpci_ctr_mode(2,0);
			outb_p(0, WDT_DC);

			inb_p(WDT_DC);

			outb_p(0, WDT_CLOCK);	/* 2.0833MHz clock */
			inb_p(WDT_BUZZER);	/* disable */
			inb_p(WDT_OPTONOTRST);	/* disable */
			inb_p(WDT_OPTORST);	/* disable */
			inb_p(WDT_PROGOUT);	/* disable */
			wdtpci_ctr_mode(0,3);
			wdtpci_ctr_mode(1,2);
			wdtpci_ctr_mode(2,1);
			wdtpci_ctr_load(0,20833);	/* count at 100Hz */
			wdtpci_ctr_load(1,wd_margin);/* Timeout 60 seconds */
			/* DO NOT LOAD CTR2 on PCI card! -- JPN */
			outb_p(0, WDT_DC);	/* Enable */
			spin_unlock_irqrestore(&wdtpci_lock, flags);
			return 0;
		case TEMP_MINOR:
			return 0;
		default:
			return -ENODEV;
	}
}

/**
 *	wdtpci_close:
 *	@inode: inode to board
 *	@file: file handle to board
 *
 *	The watchdog has a configurable API. There is a religious dispute 
 *	between people who want their watchdog to be able to shut down and 
 *	those who want to be sure if the watchdog manager dies the machine
 *	reboots. In the former case we disable the counters, in the latter
 *	case you have to open it again very soon.
 */
 
static int wdtpci_release(struct inode *inode, struct file *file)
{

	if (MINOR(inode->i_rdev)==WATCHDOG_MINOR) {
		unsigned long flags;
		if (expect_close) {
			spin_lock_irqsave(&wdtpci_lock, flags);
			inb_p(WDT_DC);		/* Disable counters */
			wdtpci_ctr_load(2,0);	/* 0 length reset pulses now */
			spin_unlock_irqrestore(&wdtpci_lock, flags);
		} else {
			printk(KERN_CRIT PFX "Unexpected close, not stopping timer!");
			wdtpci_ping();
		}
		up(&open_sem);
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

static int wdtpci_notify_sys(struct notifier_block *this, unsigned long code,
	void *unused)
{
	unsigned long flags;

	if (code==SYS_DOWN || code==SYS_HALT) {
		/* Turn the card off */
		spin_lock_irqsave(&wdtpci_lock, flags);
		inb_p(WDT_DC);
		wdtpci_ctr_load(2,0);
		spin_unlock_irqrestore(&wdtpci_lock, flags);
	}
	return NOTIFY_DONE;
}
 
/*
 *	Kernel Interfaces
 */
 
 
static struct file_operations wdtpci_fops = {
	owner:		THIS_MODULE,
	llseek:		no_llseek,
	read:		wdtpci_read,
	write:		wdtpci_write,
	ioctl:		wdtpci_ioctl,
	open:		wdtpci_open,
	release:	wdtpci_release,
};

static struct miscdevice wdtpci_miscdev=
{
	WATCHDOG_MINOR,
	"watchdog",
	&wdtpci_fops
};

#ifdef CONFIG_WDT_501
static struct miscdevice temp_miscdev=
{
	TEMP_MINOR,
	"temperature",
	&wdtpci_fops
};
#endif

/*
 *	The WDT card needs to learn about soft shutdowns in order to
 *	turn the timebomb registers off. 
 */
 
static struct notifier_block wdtpci_notifier=
{
	wdtpci_notify_sys,
	NULL,
	0
};


static int __init wdtpci_init_one (struct pci_dev *dev,
				   const struct pci_device_id *ent)
{
	static int dev_count = 0;
	int ret = -EIO;

	dev_count++;
	if (dev_count > 1) {
		printk (KERN_ERR PFX
			"this driver only supports 1 device\n");
		return -ENODEV;
	}
	
	sema_init(&open_sem, 1);
	spin_lock_init(&wdtpci_lock);

	irq = dev->irq;
	io = pci_resource_start (dev, 2);
	printk ("WDT501-P(PCI-WDG-CSM) driver 0.07 at %X "
		"(Interrupt %d)\n", io, irq);

	if (pci_enable_device (dev))
		goto out;

	if (request_region (io, 16, "wdt-pci") == NULL) {
		printk (KERN_ERR PFX "I/O %d is not free.\n", io);
		goto out;
	}

	if (request_irq (irq, wdtpci_interrupt, SA_INTERRUPT | SA_SHIRQ,
			 "wdt-pci", &wdtpci_miscdev)) {
		printk (KERN_ERR PFX "IRQ %d is not free.\n", irq);
		goto out_reg;
	}

	ret = misc_register (&wdtpci_miscdev);
	if (ret) {
		printk (KERN_ERR PFX "can't misc_register on minor=%d\n", WATCHDOG_MINOR);
		goto out_irq;
	}

	ret = register_reboot_notifier (&wdtpci_notifier);
	if (ret) {
		printk (KERN_ERR PFX "can't register_reboot_notifier on minor=%d\n", WATCHDOG_MINOR);
		goto out_misc;
	}
#ifdef CONFIG_WDT_501
	ret = misc_register (&temp_miscdev);
	if (ret) {
		printk (KERN_ERR PFX "can't misc_register (temp) on minor=%d\n", TEMP_MINOR);
		goto out_rbt;
	}
#endif

	ret = 0;
out:
	return ret;

#ifdef CONFIG_WDT_501
out_rbt:
	unregister_reboot_notifier(&wdtpci_notifier);
#endif
out_misc:
	misc_deregister(&wdtpci_miscdev);
out_irq:
	free_irq(irq, &wdtpci_miscdev);
out_reg:
	release_region (io, 16);
	goto out;
}


static void __devexit wdtpci_remove_one (struct pci_dev *pdev)
{
	/* here we assume only one device will ever have
	 * been picked up and registered by probe function */
	unregister_reboot_notifier(&wdtpci_notifier);
#ifdef CONFIG_WDT_501_PCI
	misc_deregister(&temp_miscdev);
#endif	
	misc_deregister(&wdtpci_miscdev);
	free_irq(irq, &wdtpci_miscdev);
	release_region(io, 16);
}


static struct pci_device_id wdtpci_pci_tbl[] __initdata = {
	{ PCI_VENDOR_ID_ACCESSIO, PCI_DEVICE_ID_WDG_CSM, PCI_ANY_ID, PCI_ANY_ID, },
	{ 0, }, /* terminate list */
};
MODULE_DEVICE_TABLE(pci, wdtpci_pci_tbl);


static struct pci_driver wdtpci_driver = {
	name:		"wdt-pci",
	id_table:	wdtpci_pci_tbl,
	probe:		wdtpci_init_one,
	remove:		__devexit_p(wdtpci_remove_one),
};


/**
 *	wdtpci_cleanup:
 *
 *	Unload the watchdog. You cannot do this with any file handles open.
 *	If your watchdog is set to continue ticking on close and you unload
 *	it, well it keeps ticking. We won't get the interrupt but the board
 *	will not touch PC memory so all is fine. You just have to load a new
 *	module in 60 seconds or reboot.
 */
 
static void __exit wdtpci_cleanup(void)
{
	pci_unregister_driver (&wdtpci_driver);
}


/**
 * 	wdtpci_init:
 *
 *	Set up the WDT watchdog board. All we have to do is grab the
 *	resources we require and bitch if anyone beat us to them.
 *	The open() function will actually kick the board off.
 */
 
static int __init wdtpci_init(void)
{
	int rc = pci_register_driver (&wdtpci_driver);
	
	if (rc < 1)
		return -ENODEV;
	
	return 0;
}


module_init(wdtpci_init);
module_exit(wdtpci_cleanup);

MODULE_AUTHOR("JP Nollmann, Alan Cox");
MODULE_DESCRIPTION("Driver for the ICS PCI watchdog cards");
MODULE_LICENSE("GPL");

EXPORT_NO_SYMBOLS;
