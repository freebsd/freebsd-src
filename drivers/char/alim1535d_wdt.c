/*
 *	Watchdog for the 7101 PMU version found in the ALi1535 chipsets
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
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

static spinlock_t ali_lock;	/* Guards the hardware */

#ifdef CONFIG_WATCHDOG_NOWAYOUT
static int nowayout = 1;
#else
static int nowayout = 0;
#endif

MODULE_PARM(nowayout,"i");
MODULE_PARM_DESC(nowayout, "Watchdog cannot be stopped once started (default=CONFIG_WATCHDOG_NOWAYOUT)");

static unsigned long timer_alive;
static char ali_expect_close;
static u32 ali_timeout = 60;			/* 60 seconds */
static u32 ali_timeout_bits = 1 | (1<<7);	/* 1 count in minutes */

static struct pci_dev *ali_pci;

/**
 *	ali_timer_start		-	start watchdog countdown
 *	@dev: PCI device of the PMU
 *
 *	Starts the timer running providing the timer has a counter
 *	configuration set.
 */
 
static void ali_timer_start(struct pci_dev *pdev)
{
	u32 val;

	spin_lock(&ali_lock);
	
	pci_read_config_dword(pdev, 0xCC, &val);
	val &= ~0x3F;	/* Mask count */
	val |= (1<<25) | ali_timeout_bits;
	pci_write_config_dword(pdev, 0xCC, val);
	spin_unlock(&ali_lock);
}

/**
 *	ali_timer_stop	-	stop the timer countdown
 *	@pdev: PCI device of the PMU
 *
 *	Stop the ALi watchdog countdown
 */
 
static void ali_timer_stop (struct pci_dev *pdev)
{
	u32 val;

	spin_lock(&ali_lock);
	pci_read_config_dword(pdev, 0xCC, &val);
	val &= ~0x3F;	/* Mask count to zero (disabled) */
	val &= ~(1<<25);/* and for safety mask the reset enable */
	pci_write_config_dword(pdev, 0xCC, val);
	spin_unlock(&ali_lock);
}

/**
 *	ali_timer_settimer	-	compute the timer reload value
 *	@pdev: PCI device of the PMU
 *	@t: time in seconds
 *
 *	Computes the timeout values needed and then restarts the timer
 *	running with the new timeout values
 */

static int ali_timer_settimer(struct pci_dev *pdev, unsigned long t)
{
	if(t < 60)
		ali_timeout_bits = t|(1<<6);
	else if(t < 3600)
		ali_timeout_bits = (t/60)|(1<<7);
	else if(t < 18000)
		ali_timeout_bits = (t/300)|(1<<6)|(1<<7);
	else return -EINVAL;
	
	ali_timeout = t;
	ali_timer_start(pdev);
	return 0;
}

/**
 *	ali_open	-	handle open of ali watchdog
 *	@inode: inode from VFS
 *	@file: file from VFS
 *
 *	Open the ALi watchdog device. Ensure only one person opens it
 *	at a time. Also start the watchdog running.
 */

static int ali_open(struct inode *inode, struct file *file)
{
	if (test_and_set_bit(0, &timer_alive))
		return -EBUSY;
	ali_timer_start (ali_pci);
	return 0;
}

/**
 *	ali_release	-	close an ALi watchdog
 *	@inode: inode from VFS
 *	@file: file from VFS
 *
 *	Close the ALi watchdog device. Actual shutdown of the timer
 *	only occurs if the magic sequence has been set or nowayout is 
 *	disabled
 */
 
static int ali_release (struct inode *inode, struct file *file)
{
	/*
	 *      Shut off the timer.
	 */
	if (ali_expect_close == 42 && !nowayout) {
		ali_timer_stop(ali_pci);
	} else {
		ali_timer_start(ali_pci);
		printk(KERN_CRIT  "ali1535_wdt: Unexpected close, not stopping watchdog!\n");
	}
	clear_bit(0, &timer_alive);
	ali_expect_close = 0;
	return 0;
}

/**
 *	ali_write	-	writes to ALi watchdog
 *	@file: file from VFS
 *	@data: user address of data
 *	@len: length of data
 *	@ppos: pointer to the file offset
 *
 *	Handle a write to the ALi watchdog. Writing to the file pings
 *	the watchdog and resets it. Writing the magic 'V' sequence allows
 *	the next close to turn off the watchdog.
 */
 
static ssize_t ali_write (struct file *file, const char *data,
			      size_t len, loff_t * ppos)
{
	/*  Can't seek (pwrite) on this device  */
	if (ppos != &file->f_pos)
		return -ESPIPE;

	/* See if we got the magic character 'V' and reload the timer */
	if (len) {
		size_t i;

		ali_expect_close = 0;

		/* scan to see wether or not we got the magic character */
		for (i = 0; i != len; i++) {
			u8 c;
			if(get_user(c, data+i))
				return -EFAULT;
			if (c == 'V')
				ali_expect_close = 42;
		}

		/* someone wrote to us, we should reload the timer */
		ali_timer_start(ali_pci);
		return 1;
	}
	return 0;
}

/**
 *	ali_ioctl	-	handle watchdog ioctls
 *	@inode: VFS inode
 *	@file: VFS file pointer
 *	@cmd: ioctl number
 *	@arg: arguments to the ioctl
 *
 *	Handle the watchdog ioctls supported by the ALi driver. Really
 *	we want an extension to enable irq ack monitoring and the like
 */

static int ali_ioctl (struct inode *inode, struct file *file,
			  unsigned int cmd, unsigned long arg)
{
	int options, retval = -EINVAL;
	u32 t;
	static struct watchdog_info ident = {
		options:		WDIOF_SETTIMEOUT | WDIOF_KEEPALIVEPING,
		firmware_version:	0,
		identity:		"ALi 1535D+ TCO timer",
	};
	switch (cmd) {
		default:
			return -ENOTTY;
		case WDIOC_GETSUPPORT:
			if (copy_to_user((struct watchdog_info *) arg, &ident, sizeof (ident)))
				return -EFAULT;
			return 0;
		case WDIOC_GETSTATUS:
		case WDIOC_GETBOOTSTATUS:
			return put_user(0, (int *) arg);
		case WDIOC_SETOPTIONS:
			if (get_user (options, (int *) arg))
				return -EFAULT;
			if (options & WDIOS_DISABLECARD) {
				ali_timer_stop(ali_pci);
				retval = 0;
			}
			if (options & WDIOS_ENABLECARD) {
				ali_timer_start(ali_pci);
				retval = 0;
			}
			return retval;
		case WDIOC_KEEPALIVE:
			ali_timer_start(ali_pci);
			return 0;
		case WDIOC_SETTIMEOUT:
			if (get_user(t, (int *) arg))
				return -EFAULT;
			if (ali_timer_settimer(ali_pci, t))
			    return -EINVAL;
			ali_timer_start(ali_pci);
			/* Fall */
		case WDIOC_GETTIMEOUT:
			return put_user(ali_timeout, (int *)arg);
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
 
static struct pci_device_id ali_pci_tbl[] __initdata = {
	{ PCI_VENDOR_ID_AL, 1535, PCI_ANY_ID, PCI_ANY_ID,},
	{ 0, },
};
MODULE_DEVICE_TABLE (pci, ali_pci_tbl);


/**
 *	ali_find_watchdog	-	find a 1535 and 7101
 *	
 *	Scans the PCI hardware for a 1535 series bridge and matching 7101
 *	watchdog device. This may be overtight but it is better to be safe
 */	

static int __init ali_find_watchdog(void)
{
	struct pci_dev *pdev;
	u32 wdog;
	
	/* Check for a 1535 series bridge */
	pdev = pci_find_device(PCI_VENDOR_ID_AL, 0x1535, NULL);
	if(pdev == NULL)
		return -ENODEV;

	/* Check for the a 7101 PMU */
	pdev = pci_find_device(PCI_VENDOR_ID_AL, 0x7101, NULL);
	if(pdev == NULL)
		return -ENODEV;

	if(pci_enable_device(pdev))
		return -EIO;
		
	ali_pci = pdev;
	
	/*
	 *	Initialize the timer bits
	 */
	 
	pci_read_config_dword(pdev, 0xCC, &wdog);
	
	wdog &= ~0x3F;		/* Timer bits */
	wdog &= ~((1<<27)|(1<<26)|(1<<25)|(1<<24));	/* Issued events */
	wdog &= ~((1<<16)|(1<<13)|(1<<12)|(1<<11)|(1<<10)|(1<<9));	/* No monitor bits */

	pci_write_config_dword(pdev, 0xCC, wdog);
	return 0;
}

static struct file_operations ali_fops = {
	owner:		THIS_MODULE,
	write:		ali_write,
	ioctl:		ali_ioctl,
	open:		ali_open,
	release:	ali_release,
};

static struct miscdevice ali_miscdev = {
	minor:		WATCHDOG_MINOR,
	name:		"watchdog",
	fops:		&ali_fops,
};

/**
 *	watchdog_init	-	module initialiser
 *	
 *	Scan for a suitable watchdog and if so initialize it. Return an error
 *	if we cannot, the error causes the module to unload
 */
 
static int __init watchdog_init (void)
{
	spin_lock_init(&ali_lock);
	if (!ali_find_watchdog())
		return -ENODEV;
	if (misc_register (&ali_miscdev) != 0) {
		printk (KERN_ERR "alim1535d: cannot register watchdog device node.\n");
		return -EIO;
	}
	return 0;
}

/**
 *	watchdog_cleanup	-	unload watchdog
 *
 *	Called on the unload of a successfully installed watchdog module.
 */
 
static void __exit watchdog_cleanup (void)
{
	ali_timer_stop(ali_pci);
	misc_deregister (&ali_miscdev);
}

module_init(watchdog_init);
module_exit(watchdog_cleanup);

MODULE_AUTHOR("Alan Cox");
MODULE_DESCRIPTION("Watchdog driver for the ALi 1535+ PMU");
MODULE_LICENSE("GPL");
EXPORT_NO_SYMBOLS;
