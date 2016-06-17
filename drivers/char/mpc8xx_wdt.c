/*
 * mpc8xx_wdt.c - MPC8xx watchdog userspace interface
 *
 * Copyright (C) 2002 Florian Schirmer <jolt@tuxbox.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/watchdog.h>
#include <asm/8xx_immap.h>
#include <asm/uaccess.h>

extern int m8xx_wdt_get_timeout(void);
extern void m8xx_wdt_reset(void);

static struct semaphore wdt_sem;
static int wdt_status;

static struct watchdog_info ident = {
	.identity = "MPC8xx watchdog",
	.options = WDIOF_KEEPALIVEPING,
};

static void
mpc8xx_wdt_handler_disable(void)
{
	volatile immap_t *imap = (volatile immap_t *) IMAP_ADDR;

	imap->im_sit.sit_piscr &= ~(PISCR_PIE | PISCR_PTE);

	printk(KERN_NOTICE "mpc8xx_wdt: keep-alive handler deactivated\n");
}

static void
mpc8xx_wdt_handler_enable(void)
{
	volatile immap_t *imap = (volatile immap_t *) IMAP_ADDR;

	imap->im_sit.sit_piscr |= PISCR_PIE | PISCR_PTE;

	printk(KERN_NOTICE "mpc8xx_wdt: keep-alive handler activated\n");
}

static int
mpc8xx_wdt_open(struct inode *inode, struct file *file)
{
	switch (MINOR(inode->i_rdev)) {
	case WATCHDOG_MINOR:
		if (down_trylock(&wdt_sem))
			return -EBUSY;

		m8xx_wdt_reset();
		mpc8xx_wdt_handler_disable();
		break;

	default:
		return -ENODEV;
	}

	return 0;
}

static int
mpc8xx_wdt_release(struct inode *inode, struct file *file)
{
	m8xx_wdt_reset();

#if !defined(CONFIG_WATCHDOG_NOWAYOUT)
	mpc8xx_wdt_handler_enable();
#endif

	up(&wdt_sem);

	return 0;
}

static ssize_t
mpc8xx_wdt_write(struct file *file, const char *data, size_t len, loff_t * ppos)
{
	/* Can't seek (pwrite) on this device */
	if (ppos != &file->f_pos)
		return -ESPIPE;

	if (!len)
		return 0;

	m8xx_wdt_reset();

	return 1;
}

static int
mpc8xx_wdt_ioctl(struct inode *inode, struct file *file,
		 unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case WDIOC_GETSUPPORT:
		if (copy_to_user((void *) arg, &ident, sizeof (ident)))
			return -EFAULT;
		break;

	case WDIOC_GETSTATUS:
	case WDIOC_GETBOOTSTATUS:
		if (put_user(wdt_status, (int *) arg))
			return -EFAULT;
		wdt_status &= ~WDIOF_KEEPALIVEPING;
		break;

	case WDIOC_KEEPALIVE:
		m8xx_wdt_reset();
		wdt_status |= WDIOF_KEEPALIVEPING;
		break;

	case WDIOC_GETTIMEOUT:
		{
			int timeout = m8xx_wdt_get_timeout();
			if (put_user(timeout, (int *) arg))
				return -EFAULT;
			break;
		}

	default:
		return -ENOTTY;
	}

	return 0;
}

static struct file_operations mpc8xx_wdt_fops = {
	.owner = THIS_MODULE,
	.write = mpc8xx_wdt_write,
	.ioctl = mpc8xx_wdt_ioctl,
	.open = mpc8xx_wdt_open,
	.release = mpc8xx_wdt_release,
};

static struct miscdevice mpc8xx_wdt_miscdev = {
	.minor = WATCHDOG_MINOR,
	.name = "watchdog",
	.fops = &mpc8xx_wdt_fops,
};

static int __init
mpc8xx_wdt_init(void)
{
	int ret;

	sema_init(&wdt_sem, 1);

	if ((ret = misc_register(&mpc8xx_wdt_miscdev))) {
		printk(KERN_WARNING
		       "mpc8xx_wdt: could not register userspace interface\n");
		return ret;
	}

	return 0;
}

static void __exit
mpc8xx_wdt_exit(void)
{
	misc_deregister(&mpc8xx_wdt_miscdev);

	m8xx_wdt_reset();
	mpc8xx_wdt_handler_enable();
}

module_init(mpc8xx_wdt_init);
module_exit(mpc8xx_wdt_exit);

MODULE_AUTHOR("Florian Schirmer <jolt@tuxbox.org>");
MODULE_DESCRIPTION("MPC8xx watchdog driver");
MODULE_LICENSE("GPL");
