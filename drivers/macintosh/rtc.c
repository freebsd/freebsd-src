/*
 * Linux/PowerPC Real Time Clock Driver
 *
 * heavily based on:
 * Linux/SPARC Real Time Clock Driver
 * Copyright (C) 1996 Thomas K. Dyas (tdyas@eden.rutgers.edu)
 *
 * This is a little driver that lets a user-level program access
 * the PPC clocks chip. It is no use unless you
 * use the modified clock utility.
 *
 * Get the modified clock utility from:
 *   ftp://vger.rutgers.edu/pub/linux/Sparc/userland/clock.c
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/fcntl.h>
#include <linux/poll.h>
#include <linux/init.h>
#include <linux/mc146818rtc.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/machdep.h>

#include <asm/time.h>

static int rtc_busy = 0;

/* Retrieve the current date and time from the real time clock. */
void get_rtc_time(struct rtc_time *t)
{
	unsigned long nowtime;

	nowtime = (ppc_md.get_rtc_time)();

	to_tm(nowtime, t);

	t->tm_year -= 1900;
	t->tm_mon -= 1; /* Make sure userland has a 0-based month */
}

/* Set the current date and time in the real time clock. */
void set_rtc_time(struct rtc_time *t)
{
	unsigned long nowtime;

	nowtime = mktime(t->tm_year+1900, t->tm_mon+1, t->tm_mday,
			t->tm_hour, t->tm_min, t->tm_sec);

	(ppc_md.set_rtc_time)(nowtime);
}

static int rtc_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
	unsigned long arg)
{
	struct rtc_time rtc_tm;

	switch (cmd)
	{
	case RTC_RD_TIME:
		if (ppc_md.get_rtc_time)
		{
			memset(&rtc_tm, 0, sizeof(struct rtc_time));
			get_rtc_time(&rtc_tm);

			if (copy_to_user((struct rtc_time*)arg, &rtc_tm, sizeof(struct rtc_time)))
				return -EFAULT;

			return 0;
		}
		else
			return -EINVAL;

	case RTC_SET_TIME:
		if (!capable(CAP_SYS_TIME))
			return -EPERM;

		if (ppc_md.set_rtc_time)
		{
			if (copy_from_user(&rtc_tm, (struct rtc_time*)arg, sizeof(struct rtc_time)))
				return -EFAULT;

			set_rtc_time(&rtc_tm);

			return 0;
		}
		else
			return -EINVAL;

	default:
		return -EINVAL;
	}
}

static int rtc_open(struct inode *inode, struct file *file)
{
	if (rtc_busy)
		return -EBUSY;

	rtc_busy = 1;

	MOD_INC_USE_COUNT;

	return 0;
}

static int rtc_release(struct inode *inode, struct file *file)
{
	MOD_DEC_USE_COUNT;
	rtc_busy = 0;
	return 0;
}

static struct file_operations rtc_fops = {
	owner:		THIS_MODULE,
	llseek:		no_llseek,
	ioctl:		rtc_ioctl,
	open:		rtc_open,
	release:	rtc_release
};

static struct miscdevice rtc_dev = { RTC_MINOR, "rtc", &rtc_fops };

EXPORT_NO_SYMBOLS;

static int __init rtc_init(void)
{
	int error;

	error = misc_register(&rtc_dev);
	if (error) {
		printk(KERN_ERR "rtc: unable to get misc minor\n");
		return error;
	}

	return 0;
}

static void __exit rtc_exit(void)
{
	misc_deregister(&rtc_dev);
}

module_init(rtc_init);
module_exit(rtc_exit);
MODULE_LICENSE("GPL");
