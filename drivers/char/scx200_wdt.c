/* linux/drivers/char/scx200_wdt.c 

   National Semiconductor SCx200 Watchdog support

   Copyright (c) 2001,2002 Christer Weinigel <wingel@nano-system.com>

   Som code taken from:
   National Semiconductor PC87307/PC97307 (ala SC1200) WDT driver
   (c) Copyright 2002 Zwane Mwaikambo <zwane@commfireservices.com>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The author(s) of this software shall not be held liable for damages
   of any nature resulting due to the use of this software. This
   software is provided AS-IS with no warranties. */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/watchdog.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/pci.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#include <linux/scx200.h>

#define NAME "scx200_wdt"

MODULE_AUTHOR("Christer Weinigel <wingel@nano-system.com>");
MODULE_DESCRIPTION("NatSemi SCx200 Watchdog Driver");
MODULE_LICENSE("GPL");

#ifndef CONFIG_WATCHDOG_NOWAYOUT
#define CONFIG_WATCHDOG_NOWAYOUT 0
#endif

static int margin = 60;		/* in seconds */
MODULE_PARM(margin, "i");
MODULE_PARM_DESC(margin, "Watchdog margin in seconds");

static int nowayout = CONFIG_WATCHDOG_NOWAYOUT;
MODULE_PARM(nowayout, "i");
MODULE_PARM_DESC(nowayout, "Disable watchdog shutdown on close");

static u16 wdto_restart;
static struct semaphore open_semaphore;
static unsigned expect_close;

/* Bits of the WDCNFG register */
#define W_ENABLE 0x00fa		/* Enable watchdog */
#define W_DISABLE 0x0000	/* Disable watchdog */

/* The scaling factor for the timer, this depends on the value of W_ENABLE */
#define W_SCALE (32768/1024)

static void scx200_wdt_ping(void)
{
	outw(wdto_restart, SCx200_CB_BASE + SCx200_WDT_WDTO);
}

static void scx200_wdt_update_margin(void)
{
	printk(KERN_INFO NAME ": timer margin %d seconds\n", margin);
	wdto_restart = margin * W_SCALE;
}

static void scx200_wdt_enable(void)
{
	printk(KERN_DEBUG NAME ": enabling watchdog timer, wdto_restart = %d\n", 
	       wdto_restart);

	outw(0, SCx200_CB_BASE + SCx200_WDT_WDTO);
	outb(SCx200_WDT_WDSTS_WDOVF, SCx200_CB_BASE + SCx200_WDT_WDSTS);
	outw(W_ENABLE, SCx200_CB_BASE + SCx200_WDT_WDCNFG);

	scx200_wdt_ping();
}

static void scx200_wdt_disable(void)
{
	printk(KERN_DEBUG NAME ": disabling watchdog timer\n");
		
	outw(0, SCx200_CB_BASE + SCx200_WDT_WDTO);
	outb(SCx200_WDT_WDSTS_WDOVF, SCx200_CB_BASE + SCx200_WDT_WDSTS);
	outw(W_DISABLE, SCx200_CB_BASE + SCx200_WDT_WDCNFG);
}

static int scx200_wdt_open(struct inode *inode, struct file *file)
{
        /* only allow one at a time */
        if (down_trylock(&open_semaphore))
                return -EBUSY;
	scx200_wdt_enable();
	expect_close = 0;

	return 0;
}

static int scx200_wdt_release(struct inode *inode, struct file *file)
{
	if (!expect_close) {
		printk(KERN_WARNING NAME ": watchdog device closed unexpectedly, will not disable the watchdog timer\n");
	} else if (!nowayout) {
		scx200_wdt_disable();
	}
        up(&open_semaphore);

	return 0;
}

static int scx200_wdt_notify_sys(struct notifier_block *this, 
				      unsigned long code, void *unused)
{
        if (code == SYS_HALT || code == SYS_POWER_OFF)
		if (!nowayout)
			scx200_wdt_disable();

        return NOTIFY_DONE;
}

static struct notifier_block scx200_wdt_notifier =
{
        notifier_call: scx200_wdt_notify_sys
};

static ssize_t scx200_wdt_write(struct file *file, const char *data, 
				     size_t len, loff_t *ppos)
{
	if (ppos != &file->f_pos)
		return -ESPIPE;

	/* check for a magic close character */
	if (len) 
	{
		size_t i;

		scx200_wdt_ping();

		expect_close = 0;
		for (i = 0; i < len; ++i) {
			char c;
			if (get_user(c, data+i))
				return -EFAULT;
			if (c == 'V')
				expect_close = 1;
		}

		return len;
	}

	return 0;
}

static int scx200_wdt_ioctl(struct inode *inode, struct file *file,
	unsigned int cmd, unsigned long arg)
{
	static struct watchdog_info ident = {
		.identity = "NatSemi SCx200 Watchdog",
		.firmware_version = 1, 
		.options = (WDIOF_SETTIMEOUT | WDIOF_KEEPALIVEPING),
	};
	int new_margin;
	
	switch (cmd) {
	default:
		return -ENOTTY;
	case WDIOC_GETSUPPORT:
		if(copy_to_user((struct watchdog_info *)arg, &ident, 
				sizeof(ident)))
			return -EFAULT;
		return 0;
	case WDIOC_GETSTATUS:
	case WDIOC_GETBOOTSTATUS:
		if (put_user(0, (int *)arg))
			return -EFAULT;
		return 0;
	case WDIOC_KEEPALIVE:
		scx200_wdt_ping();
		return 0;
	case WDIOC_SETTIMEOUT:
		if (get_user(new_margin, (int *)arg))
			return -EFAULT;
		if (new_margin < 1)
			return -EINVAL;
		margin = new_margin;
		scx200_wdt_update_margin();
		scx200_wdt_ping();
	case WDIOC_GETTIMEOUT:
		if (put_user(margin, (int *)arg))
			return -EFAULT;
		return 0;
	}
}

static struct file_operations scx200_wdt_fops = {
	.owner	 = THIS_MODULE,
	.write   = scx200_wdt_write,
	.ioctl   = scx200_wdt_ioctl,
	.open    = scx200_wdt_open,
	.release = scx200_wdt_release,
};

static struct miscdevice scx200_wdt_miscdev = {
	.minor = WATCHDOG_MINOR,
	.name  = NAME,
	.fops  = &scx200_wdt_fops,
};

static int __init scx200_wdt_init(void)
{
	int r;

	printk(KERN_DEBUG NAME ": NatSemi SCx200 Watchdog Driver\n");

	/* First check that this really is a NatSemi SCx200 CPU */
	if ((pci_find_device(PCI_VENDOR_ID_NS, 
			     PCI_DEVICE_ID_NS_SCx200_BRIDGE,
			     NULL)) == NULL)
		return -ENODEV;

	/* More sanity checks, verify that the configuration block is there */
	if (!scx200_cb_probe(SCx200_CB_BASE)) {
		printk(KERN_WARNING NAME ": no configuration block found\n");
		return -ENODEV;
	}

	if (!request_region(SCx200_CB_BASE + SCx200_WDT_OFFSET, 
			    SCx200_WDT_SIZE, 
			    "NatSemi SCx200 Watchdog")) {
		printk(KERN_WARNING NAME ": watchdog I/O region busy\n");
		return -EBUSY;
	}

	scx200_wdt_update_margin();
	scx200_wdt_disable();

	sema_init(&open_semaphore, 1);

	r = misc_register(&scx200_wdt_miscdev);
	if (r)
		return r;

	r = register_reboot_notifier(&scx200_wdt_notifier);
        if (r) {
                printk(KERN_ERR NAME ": unable to register reboot notifier");
		misc_deregister(&scx200_wdt_miscdev);
                return r;
        }

	return 0;
}

static void __exit scx200_wdt_cleanup(void)
{
        unregister_reboot_notifier(&scx200_wdt_notifier);
	misc_deregister(&scx200_wdt_miscdev);
	release_region(SCx200_CB_BASE + SCx200_WDT_OFFSET,
		       SCx200_WDT_SIZE);
}

module_init(scx200_wdt_init);
module_exit(scx200_wdt_cleanup);

/*
    Local variables:
        compile-command: "make -k -C ../.. SUBDIRS=drivers/char modules"
        c-basic-offset: 8
    End:
*/
