/*
 * MixCom Watchdog: A Simple Hardware Watchdog Device
 * Based on Softdog driver by Alan Cox and PC Watchdog driver by Ken Hollis
 *
 * Author: Gergely Madarasz <gorgo@itc.hu>
 *
 * Copyright (c) 1999 ITConsult-Pro Co. <info@itc.hu>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 * Version 0.1 (99/04/15):
 *		- first version
 *
 * Version 0.2 (99/06/16):
 *		- added kernel timer watchdog ping after close
 *		  since the hardware does not support watchdog shutdown
 *
 * Version 0.3 (99/06/21):
 *		- added WDIOC_GETSTATUS and WDIOC_GETSUPPORT ioctl calls
 *
 * Version 0.3.1 (99/06/22):
 *		- allow module removal while internal timer is active,
 *		  print warning about probable reset
 *
 * Version 0.4 (99/11/15):
 *		- support for one more type board
 *
 * Version 0.5 (2001/12/14) Matt Domsch <Matt_Domsch@dell.com>
 * 		- added nowayout module option to override CONFIG_WATCHDOG_NOWAYOUT
 *	
 */

#define VERSION "0.5" 
  
#include <linux/module.h>
#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/miscdevice.h>
#include <linux/ioport.h>
#include <linux/watchdog.h>
#include <linux/reboot.h>
#include <linux/init.h>
#include <linux/smp_lock.h>
#include <asm/uaccess.h>
#include <asm/io.h>

static int mixcomwd_ioports[] = { 0x180, 0x280, 0x380, 0x000 };

#define MIXCOM_WATCHDOG_OFFSET 0xc10
#define MIXCOM_ID 0x11
#define FLASHCOM_WATCHDOG_OFFSET 0x4
#define FLASHCOM_ID 0x18

static long mixcomwd_opened; /* long req'd for setbit --RR */

static int watchdog_port;

static int mixcomwd_timer_alive;
static struct timer_list mixcomwd_timer;
static int expect_close = 0;

#ifdef CONFIG_WATCHDOG_NOWAYOUT
static int nowayout = 1;
#else
static int nowayout = 0;
#endif

MODULE_PARM(nowayout,"i");
MODULE_PARM_DESC(nowayout, "Watchdog cannot be stopped once started (default=CONFIG_WATCHDOG_NOWAYOUT)");

static void mixcomwd_ping(void)
{
	outb_p(55,watchdog_port);
	return;
}

static void mixcomwd_timerfun(unsigned long d)
{
	mixcomwd_ping();
	
	mod_timer(&mixcomwd_timer,jiffies+ 5*HZ);
}

/*
 *	Allow only one person to hold it open
 */
 
static int mixcomwd_open(struct inode *inode, struct file *file)
{
	if(test_and_set_bit(0,&mixcomwd_opened)) {
		return -EBUSY;
	}
	mixcomwd_ping();
	
	if (nowayout) {
		MOD_INC_USE_COUNT;
	}
	if(mixcomwd_timer_alive) {
		del_timer(&mixcomwd_timer);
		mixcomwd_timer_alive=0;
	} 
	return 0;
}

static int mixcomwd_release(struct inode *inode, struct file *file)
{

	lock_kernel();
	if (expect_close) {
		if(mixcomwd_timer_alive) {
			printk(KERN_ERR "mixcomwd: release called while internal timer alive");
			unlock_kernel();
			return -EBUSY;
		}
		init_timer(&mixcomwd_timer);
		mixcomwd_timer.expires=jiffies + 5 * HZ;
		mixcomwd_timer.function=mixcomwd_timerfun;
		mixcomwd_timer.data=0;
		mixcomwd_timer_alive=1;
		add_timer(&mixcomwd_timer);
	} else {
		printk(KERN_CRIT "mixcomwd: WDT device closed unexpectedly.  WDT will not stop!\n");
	}

	clear_bit(0,&mixcomwd_opened);
	unlock_kernel();
	return 0;
}


static ssize_t mixcomwd_write(struct file *file, const char *data, size_t len, loff_t *ppos)
{
	if (ppos != &file->f_pos) {
		return -ESPIPE;
	}

	if(len)
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
		mixcomwd_ping();
		return 1;
	}
	return 0;
}

static int mixcomwd_ioctl(struct inode *inode, struct file *file,
	unsigned int cmd, unsigned long arg)
{
	int status;
        static struct watchdog_info ident = {
		WDIOF_KEEPALIVEPING | WDIOF_MAGICCLOSE,
		1, "MixCOM watchdog"
	};
                                        
	switch(cmd)
	{
		case WDIOC_GETSTATUS:
			status=mixcomwd_opened;
			if (!nowayout) {
				status|=mixcomwd_timer_alive;
			}
			if (copy_to_user((int *)arg, &status, sizeof(int))) {
				return -EFAULT;
			}
			break;
		case WDIOC_GETSUPPORT:
			if (copy_to_user((struct watchdog_info *)arg, &ident, 
			    sizeof(ident))) {
				return -EFAULT;
			}
			break;
		case WDIOC_KEEPALIVE:
			mixcomwd_ping();
			break;
		default:
			return -ENOTTY;
	}
	return 0;
}

static struct file_operations mixcomwd_fops=
{
	owner:		THIS_MODULE,
	write:		mixcomwd_write,
	ioctl:		mixcomwd_ioctl,
	open:		mixcomwd_open,
	release:	mixcomwd_release,
};

static struct miscdevice mixcomwd_miscdev=
{
	WATCHDOG_MINOR,
	"watchdog",
	&mixcomwd_fops
};

static int __init mixcomwd_checkcard(int port)
{
	int id;

	if(check_region(port+MIXCOM_WATCHDOG_OFFSET,1)) {
		return 0;
	}
	
	id=inb_p(port + MIXCOM_WATCHDOG_OFFSET) & 0x3f;
	if(id!=MIXCOM_ID) {
		return 0;
	}
	return 1;
}

static int __init flashcom_checkcard(int port)
{
	int id;
	
	if(check_region(port + FLASHCOM_WATCHDOG_OFFSET,1)) {
		return 0;
	}
	
	id=inb_p(port + FLASHCOM_WATCHDOG_OFFSET);
 	if(id!=FLASHCOM_ID) {
		return 0;
	}
 	return 1;
 }
 
static int __init mixcomwd_init(void)
{
	int i;
	int ret;
	int found=0;

	for (i = 0; !found && mixcomwd_ioports[i] != 0; i++) {
		if (mixcomwd_checkcard(mixcomwd_ioports[i])) {
			found = 1;
			watchdog_port = mixcomwd_ioports[i] + MIXCOM_WATCHDOG_OFFSET;
		}
	}
	
	/* The FlashCOM card can be set up at 0x300 -> 0x378, in 0x8 jumps */
	for (i = 0x300; !found && i < 0x380; i+=0x8) {
		if (flashcom_checkcard(i)) {
			found = 1;
			watchdog_port = i + FLASHCOM_WATCHDOG_OFFSET;
		}
	}
	
	if (!found) {
		printk("mixcomwd: No card detected, or port not available.\n");
		return -ENODEV;
	}

	request_region(watchdog_port,1,"MixCOM watchdog");
		
	ret = misc_register(&mixcomwd_miscdev);
	if (ret)
		return ret;
	
	printk(KERN_INFO "MixCOM watchdog driver v%s, watchdog port at 0x%3x\n",VERSION,watchdog_port);

	return 0;
}	

static void __exit mixcomwd_exit(void)
{
	if (!nowayout) {
		if(mixcomwd_timer_alive) {
			printk(KERN_WARNING "mixcomwd: I quit now, hardware will"
				" probably reboot!\n");
			del_timer(&mixcomwd_timer);
			mixcomwd_timer_alive=0;
		}
	}
	release_region(watchdog_port,1);
	misc_deregister(&mixcomwd_miscdev);
}

module_init(mixcomwd_init);
module_exit(mixcomwd_exit);

MODULE_LICENSE("GPL");
EXPORT_NO_SYMBOLS;
