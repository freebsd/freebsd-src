/*
 *	i810-tco 0.05:	TCO timer driver for i8xx chipsets
 *
 *	(c) Copyright 2000 kernel concepts <nils@kernelconcepts.de>, All Rights Reserved.
 *				http://www.kernelconcepts.de
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 *	
 *	Neither kernel concepts nor Nils Faerber admit liability nor provide
 *	warranty for any of this software. This material is provided
 *	"AS-IS" and at no charge.
 *
 *	(c) Copyright 2000	kernel concepts <nils@kernelconcepts.de>
 *				developed for
 *                              Jentro AG, Haar/Munich (Germany)
 *
 *	TCO timer driver for i8xx chipsets
 *	based on softdog.c by Alan Cox <alan@redhat.com>
 *
 *	The TCO timer is implemented in the following I/O controller hubs:
 *	(See the intel documentation on http://developer.intel.com.)
 *	82801AA & 82801AB  chip : document number 290655-003, 290677-004,
 *	82801BA & 82801BAM chip : document number 290687-002, 298242-005,
 *	82801CA & 82801CAM chip : document number 290716-001, 290718-001,
 *	82801DB & 82801E   chip : document number 290744-001, 273599-001,
 *	82801EB & 82801ER  chip : document number 252516-001
 *
 *  20000710 Nils Faerber
 *	Initial Version 0.01
 *  20000728 Nils Faerber
 *	0.02 Fix for SMI_EN->TCO_EN bit, some cleanups
 *  20011214 Matt Domsch <Matt_Domsch@dell.com>
 *	0.03 Added nowayout module option to override CONFIG_WATCHDOG_NOWAYOUT
 *	     Didn't add timeout option as i810_margin already exists.
 *  20020224 Joel Becker, Wim Van Sebroeck
 *	0.04 Support for 82801CA(M) chipset, timer margin needs to be > 3,
 *	     add support for WDIOC_SETTIMEOUT and WDIOC_GETTIMEOUT.
 *  20020412 Rob Radez <rob@osinvestor.com>, Wim Van Sebroeck
 *	0.05 Fix possible timer_alive race, add expect close support,
 *	     clean up ioctls (WDIOC_GETSTATUS, WDIOC_GETBOOTSTATUS and
 *	     WDIOC_SETOPTIONS), made i810tco_getdevice __init,
 *	     removed boot_status, removed tco_timer_read,
 *	     added support for 82801DB and 82801E chipset,
 *	     added support for 82801EB and 8280ER chipset,
 *	     general cleanup.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/miscdevice.h>
#include <linux/watchdog.h>
#include <linux/reboot.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/ioport.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include "i810-tco.h"


/* Module and version information */
#define TCO_VERSION "0.05"
#define TCO_MODULE_NAME "i810 TCO timer"
#define TCO_DRIVER_NAME   TCO_MODULE_NAME ", v" TCO_VERSION

/* Default expire timeout */
#define TIMER_MARGIN	50	/* steps of 0.6sec, 3<n<64. Default is 30 seconds */

static unsigned int ACPIBASE;
static spinlock_t tco_lock;	/* Guards the hardware */

static int i810_margin = TIMER_MARGIN;	/* steps of 0.6sec */

MODULE_PARM(i810_margin, "i");
MODULE_PARM_DESC(i810_margin, "i810-tco timeout in steps of 0.6sec, 3<n<64. Default = 50 (30 seconds)");

#ifdef CONFIG_WATCHDOG_NOWAYOUT
static int nowayout = 1;
#else
static int nowayout = 0;
#endif

MODULE_PARM(nowayout,"i");
MODULE_PARM_DESC(nowayout, "Watchdog cannot be stopped once started (default=CONFIG_WATCHDOG_NOWAYOUT)");


/*
 *	Timer active flag
 */

static unsigned long timer_alive;
static char tco_expect_close;

/*
 * Some TCO specific functions
 */


/*
 * Start the timer countdown
 */
static int tco_timer_start (void)
{
	unsigned char val;

	spin_lock(&tco_lock);
	val = inb (TCO1_CNT + 1);
	val &= 0xf7;
	outb (val, TCO1_CNT + 1);
	val = inb (TCO1_CNT + 1);
	spin_unlock(&tco_lock);
	
	if (val & 0x08)
		return -1;
	return 0;
}

/*
 * Stop the timer countdown
 */
static int tco_timer_stop (void)
{
	unsigned char val;

	spin_lock(&tco_lock);
	val = inb (TCO1_CNT + 1);
	val |= 0x08;
	outb (val, TCO1_CNT + 1);
	val = inb (TCO1_CNT + 1);
	spin_unlock(&tco_lock);
	
	if ((val & 0x08) == 0)
		return -1;
	return 0;
}

/*
 * Set the timer reload value
 */
static int tco_timer_settimer (unsigned char tmrval)
{
	unsigned char val;

	/* from the specs: */
	/* "Values of 0h-3h are ignored and should not be attempted" */
	if (tmrval > 0x3f || tmrval < 0x04)
		return -1;
	
	spin_lock(&tco_lock);
	val = inb (TCO1_TMR);
	val &= 0xc0;
	val |= tmrval;
	outb (val, TCO1_TMR);
	val = inb (TCO1_TMR);
	spin_unlock(&tco_lock);
	
	if ((val & 0x3f) != tmrval)
		return -1;

	return 0;
}

/*
 * Reload (trigger) the timer. Lock is needed so we dont reload it during
 * a reprogramming event
 */

static void tco_timer_reload (void)
{
	spin_lock(&tco_lock);
	outb (0x01, TCO1_RLD);
	spin_unlock(&tco_lock);
}

/*
 *	Allow only one person to hold it open
 */

static int i810tco_open (struct inode *inode, struct file *file)
{
	if (test_and_set_bit(0, &timer_alive))
		return -EBUSY;

	/*
	 *      Reload and activate timer
	 */
	tco_timer_reload ();
	tco_timer_start ();
	return 0;
}

static int i810tco_release (struct inode *inode, struct file *file)
{
	/*
	 *      Shut off the timer.
	 */
	if (tco_expect_close == 42 && !nowayout) {
		tco_timer_stop ();
	} else {
		tco_timer_reload ();
		printk(KERN_CRIT TCO_MODULE_NAME ": Unexpected close, not stopping watchdog!\n");
	}
	clear_bit(0, &timer_alive);
	tco_expect_close = 0;
	return 0;
}

static ssize_t i810tco_write (struct file *file, const char *data,
			      size_t len, loff_t * ppos)
{
	/*  Can't seek (pwrite) on this device  */
	if (ppos != &file->f_pos)
		return -ESPIPE;

	/* See if we got the magic character 'V' and reload the timer */
	if (len) {
		size_t i;

		tco_expect_close = 0;

		/* scan to see wether or not we got the magic character */
		for (i = 0; i != len; i++) {
			u8 c;
			if(get_user(c, data+i))
				return -EFAULT;
			if (c == 'V')
				tco_expect_close = 42;
		}

		/* someone wrote to us, we should reload the timer */
		tco_timer_reload ();
		return 1;
	}
	return 0;
}

static int i810tco_ioctl (struct inode *inode, struct file *file,
			  unsigned int cmd, unsigned long arg)
{
	int new_margin, u_margin;
	int options, retval = -EINVAL;

	static struct watchdog_info ident = {
		options:		WDIOF_SETTIMEOUT |
					WDIOF_KEEPALIVEPING |
					WDIOF_MAGICCLOSE,
		firmware_version:	0,
		identity:		"i810 TCO timer",
	};
	switch (cmd) {
		default:
			return -ENOTTY;
		case WDIOC_GETSUPPORT:
			if (copy_to_user
			    ((struct watchdog_info *) arg, &ident, sizeof (ident)))
				return -EFAULT;
			return 0;
		case WDIOC_GETSTATUS:
		case WDIOC_GETBOOTSTATUS:
			return put_user (0, (int *) arg);
		case WDIOC_SETOPTIONS:
			if (get_user (options, (int *) arg))
				return -EFAULT;
			if (options & WDIOS_DISABLECARD) {
				tco_timer_stop ();
				retval = 0;
			}
			if (options & WDIOS_ENABLECARD) {
				tco_timer_reload ();
				tco_timer_start ();
				retval = 0;
			}
			return retval;
		case WDIOC_KEEPALIVE:
			tco_timer_reload ();
			return 0;
		case WDIOC_SETTIMEOUT:
			if (get_user (u_margin, (int *) arg))
				return -EFAULT;
			new_margin = (u_margin * 10 + 5) / 6;
			if ((new_margin < 4) || (new_margin > 63))
				return -EINVAL;
			if (tco_timer_settimer ((unsigned char) new_margin))
			    return -EINVAL;
			i810_margin = new_margin;
			tco_timer_reload ();
			/* Fall */
		case WDIOC_GETTIMEOUT:
			return put_user ((int)(i810_margin * 6 / 10), (int *) arg);
	}
}

/*
 * Data for PCI driver interface
 *
 * This data only exists for exporting the supported
 * PCI ids via MODULE_DEVICE_TABLE.  We do not actually
 * register a pci_driver, because someone else might one day
 * want to register another driver on the same PCI id.
 */
static struct pci_device_id i810tco_pci_tbl[] __initdata = {
	{ PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82801AA_0,	PCI_ANY_ID, PCI_ANY_ID, },
	{ PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82801AB_0,	PCI_ANY_ID, PCI_ANY_ID, },
	{ PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82801BA_0,	PCI_ANY_ID, PCI_ANY_ID, },
	{ PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82801BA_10,	PCI_ANY_ID, PCI_ANY_ID, },
	{ PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82801CA_0,	PCI_ANY_ID, PCI_ANY_ID, },
	{ PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82801CA_12,	PCI_ANY_ID, PCI_ANY_ID, },
	{ PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82801DB_0,	PCI_ANY_ID, PCI_ANY_ID, },
	{ PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82801E_0,	PCI_ANY_ID, PCI_ANY_ID, },
	{ PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82801EB_0,	PCI_ANY_ID, PCI_ANY_ID, },
	{ 0, },
};
MODULE_DEVICE_TABLE (pci, i810tco_pci_tbl);

static struct pci_dev *i810tco_pci;

static unsigned char __init i810tco_getdevice (void)
{
	struct pci_dev *dev;
	u8 val1, val2;
	u16 badr;
	/*
	 *      Find the PCI device
	 */

	pci_for_each_dev(dev) {
		if (pci_match_device(i810tco_pci_tbl, dev)) {
			i810tco_pci = dev;
			break;
		}
	}

	if (i810tco_pci) {
		/*
		 *      Find the ACPI base I/O address which is the base
		 *      for the TCO registers (TCOBASE=ACPIBASE + 0x60)
		 *      ACPIBASE is bits [15:7] from 0x40-0x43
		 */
		pci_read_config_byte (i810tco_pci, 0x40, &val1);
		pci_read_config_byte (i810tco_pci, 0x41, &val2);
		badr = ((val2 << 1) | (val1 >> 7)) << 7;
		ACPIBASE = badr;
		/* Something's wrong here, ACPIBASE has to be set */
		if (badr == 0x0001 || badr == 0x0000) {
			printk (KERN_ERR TCO_MODULE_NAME " init: failed to get TCOBASE address\n");
			return 0;
		}
		/*
		 * Check chipset's NO_REBOOT bit
		 */
		pci_read_config_byte (i810tco_pci, 0xd4, &val1);
		if (val1 & 0x02) {
			val1 &= 0xfd;
			pci_write_config_byte (i810tco_pci, 0xd4, val1);
			pci_read_config_byte (i810tco_pci, 0xd4, &val1);
			if (val1 & 0x02) {
				printk (KERN_ERR TCO_MODULE_NAME " init: failed to reset NO_REBOOT flag, reboot disabled by hardware\n");
				return 0;	/* Cannot reset NO_REBOOT bit */
			}
		}
		/* Set the TCO_EN bit in SMI_EN register */
		val1 = inb (SMI_EN + 1);
		val1 &= 0xdf;
		outb (val1, SMI_EN + 1);
		/* Clear out the (probably old) status */
		outb (0, TCO1_STS);
		outb (3, TCO2_STS);
		return 1;
	}
	return 0;
}

static struct file_operations i810tco_fops = {
	owner:		THIS_MODULE,
	write:		i810tco_write,
	ioctl:		i810tco_ioctl,
	open:		i810tco_open,
	release:	i810tco_release,
};

static struct miscdevice i810tco_miscdev = {
	minor:		WATCHDOG_MINOR,
	name:		"watchdog",
	fops:		&i810tco_fops,
};

static int __init watchdog_init (void)
{
	spin_lock_init(&tco_lock);
	if (!i810tco_getdevice () || i810tco_pci == NULL)
		return -ENODEV;
	if (!request_region (TCOBASE, 0x10, "i810 TCO")) {
		printk (KERN_ERR TCO_MODULE_NAME
			": I/O address 0x%04x already in use\n",
			TCOBASE);
		return -EIO;
	}
	if (misc_register (&i810tco_miscdev) != 0) {
		release_region (TCOBASE, 0x10);
		printk (KERN_ERR TCO_MODULE_NAME ": cannot register miscdev\n");
		return -EIO;
	}
	tco_timer_settimer ((unsigned char) i810_margin);
	tco_timer_reload ();

	printk (KERN_INFO TCO_DRIVER_NAME
		": timer margin: %d sec (0x%04x) (nowayout=%d)\n",
		(int) (i810_margin * 6 / 10), TCOBASE, nowayout);
	return 0;
}

static void __exit watchdog_cleanup (void)
{
	u8 val;

	/* Reset the timer before we leave */
	tco_timer_reload ();
	/* Set the NO_REBOOT bit to prevent later reboots, just for sure */
	pci_read_config_byte (i810tco_pci, 0xd4, &val);
	val |= 0x02;
	pci_write_config_byte (i810tco_pci, 0xd4, val);
	release_region (TCOBASE, 0x10);
	misc_deregister (&i810tco_miscdev);
}

module_init(watchdog_init);
module_exit(watchdog_cleanup);

MODULE_AUTHOR("Nils Faerber");
MODULE_DESCRIPTION("TCO timer driver for i8xx chipsets");
MODULE_LICENSE("GPL");
EXPORT_NO_SYMBOLS;
