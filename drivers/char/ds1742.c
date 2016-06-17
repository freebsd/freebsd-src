/*
 * linux/drivers/char/ds1742.c
 *
 * Dallas DS1742 Real Time Clock driver
 *
 * Copyright (C) 2003 TimeSys Corp.
 *                    S. James Hill (James.Hill@timesys.com)
 *                                  (sjhill@realitydiluted.com)
 *
 * Copyright (C) 2001 MontaVista Software Inc.
 *                    ahennessy@mvista.com
 *
 * Copyright (C) 2000-2001 Toshiba Corporation
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 *  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 *  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 *  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 *  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 *  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fcntl.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <asm/debug.h>
#include <asm/mc146818rtc.h>
#include <asm/time.h>
#include <asm/uaccess.h>

#define DS1742_VERSION          "2.0"

/*
 * Registers
 */
#define RTC_CONTROL		(rtc_base + 0x7f8)
#define RTC_CENTURY		(rtc_base + 0x7f8)
#define RTC_SECONDS		(rtc_base + 0x7f9)
#define RTC_MINUTES		(rtc_base + 0x7fa)
#define RTC_HOURS		(rtc_base + 0x7fb)
#define RTC_DAY			(rtc_base + 0x7fc)
#define RTC_DATE		(rtc_base + 0x7fd)
#define RTC_MONTH		(rtc_base + 0x7fe)
#define RTC_YEAR		(rtc_base + 0x7ff)

#define RTC_CENTURY_MASK	0x3f
#define RTC_SECONDS_MASK	0x7f
#define RTC_DAY_MASK		0x07

/*
 * Bits in the Control/Century register
 */
#define RTC_WRITE		0x80
#define RTC_READ		0x40

/*
 * Bits in the Seconds register
 */
#define RTC_STOP		0x80

/*
 * Bits in the Day register
 */
#define RTC_BATT_FLAG		0x80
#define RTC_FREQ_TEST		0x40

/*
 * Conversion between binary and BCD
 */
#define BCD_TO_BIN(val) 	(((val)&15) + ((val)>>4)*10)
#define BIN_TO_BCD(val) 	((((val)/10)<<4) + (val)%10)

/*
 * CMOS Year Epoch
 */
#define	EPOCH			2000

/*
 * The entry /dev/rtc is being used
 */
#define RTC_IS_OPEN		0x1

static unsigned long rtc_base = 0;
static unsigned long rtc_status = 0;
static spinlock_t rtc_lock;

extern void to_tm(unsigned long tim, struct rtc_time * tm);

static unsigned long rtc_ds1742_get_time(void)
{
	unsigned int year, month, day, hour, minute, second;
	unsigned int century;

	CMOS_WRITE(RTC_READ, RTC_CONTROL);
	second = BCD_TO_BIN(CMOS_READ(RTC_SECONDS) & RTC_SECONDS_MASK);
	minute = BCD_TO_BIN(CMOS_READ(RTC_MINUTES));
	hour = BCD_TO_BIN(CMOS_READ(RTC_HOURS));
	day = BCD_TO_BIN(CMOS_READ(RTC_DATE));
	month = BCD_TO_BIN(CMOS_READ(RTC_MONTH));
	year = BCD_TO_BIN(CMOS_READ(RTC_YEAR));
	century = BCD_TO_BIN(CMOS_READ(RTC_CENTURY) & RTC_CENTURY_MASK);
	CMOS_WRITE(0, RTC_CONTROL);

	year += century * 100;

	return mktime(year, month, day, hour, minute, second);
}

static int rtc_ds1742_set_time(unsigned long t)
{
	struct rtc_time tm;
	u8 year, month, day, hour, minute, second;
	u8 cmos_year, cmos_month, cmos_day, cmos_hour, cmos_minute, cmos_second;
	int cmos_century;

	CMOS_WRITE(RTC_READ, RTC_CONTROL);
	cmos_second = (u8)(CMOS_READ(RTC_SECONDS) & RTC_SECONDS_MASK);
	cmos_minute = (u8)CMOS_READ(RTC_MINUTES);
	cmos_hour = (u8)CMOS_READ(RTC_HOURS);
	cmos_day = (u8)CMOS_READ(RTC_DATE);
	cmos_month = (u8)CMOS_READ(RTC_MONTH);
	cmos_year = (u8)CMOS_READ(RTC_YEAR);
	cmos_century = CMOS_READ(RTC_CENTURY) & RTC_CENTURY_MASK;

	CMOS_WRITE(RTC_WRITE, RTC_CONTROL);

	/* convert */
	to_tm(t, &tm);

	/* check each field one by one */
	year = BIN_TO_BCD(tm.tm_year - EPOCH);
	if (year != cmos_year) {
		CMOS_WRITE(year,RTC_YEAR);
	}

	month = BIN_TO_BCD(tm.tm_mon + 1);
	if (month != (cmos_month & 0x1f)) {
		CMOS_WRITE((month & 0x1f) | (cmos_month & ~0x1f),RTC_MONTH);
	}

	day = BIN_TO_BCD(tm.tm_mday);
	if (day != cmos_day)
		CMOS_WRITE(day, RTC_DATE);

	if (cmos_hour & 0x40) {
		/* 12 hour format */
		hour = 0x40;
		if (tm.tm_hour > 12) {
			hour |= 0x20 | (BIN_TO_BCD(hour-12) & 0x1f);
		} else {
			hour |= BIN_TO_BCD(tm.tm_hour);
		}
	} else {
		/* 24 hour format */
		hour = BIN_TO_BCD(tm.tm_hour) & 0x3f;
	}
	if (hour != cmos_hour) CMOS_WRITE(hour, RTC_HOURS);

	minute = BIN_TO_BCD(tm.tm_min);
	if (minute !=  cmos_minute) {
		CMOS_WRITE(minute, RTC_MINUTES);
	}

	second = BIN_TO_BCD(tm.tm_sec);
	if (second !=  cmos_second) {
		CMOS_WRITE(second & RTC_SECONDS_MASK,RTC_SECONDS);
	}

	/* RTC_CENTURY and RTC_CONTROL share same address... */
	CMOS_WRITE(cmos_century, RTC_CONTROL);

	return 0;
}

void __init rtc_ds1742_init(unsigned long base)
{
	u8  cmos_second;

	/* remember the base */
	rtc_base = base;
	db_assert((rtc_base & 0xe0000000) == KSEG1);

	/* set the function pointers */
	rtc_get_time = rtc_ds1742_get_time;
	rtc_set_time = rtc_ds1742_set_time;

	/* clear oscillator stop bit */
	CMOS_WRITE(RTC_READ, RTC_CONTROL);
	cmos_second = (u8)(CMOS_READ(RTC_SECONDS) & RTC_SECONDS_MASK);
	CMOS_WRITE(RTC_WRITE, RTC_CONTROL);
	CMOS_WRITE(cmos_second, RTC_SECONDS); /* clear msb */
	CMOS_WRITE(0, RTC_CONTROL);
}

static int get_ds1742_status(char *buf)
{
	char *p;
	struct rtc_time tm;
	unsigned long curr_time;

	curr_time = rtc_ds1742_get_time();
	to_tm(curr_time, &tm);

	p = buf;

	/*
	 * There is no way to tell if the luser has the RTC set for local
	 * time or for Universal Standard Time (GMT). Probably local though.
	 */
	p += sprintf(p,
		     "rtc_time\t: %02d:%02d:%02d\n"
		     "rtc_date\t: %04d-%02d-%02d\n"
		     "rtc_epoch\t: %04lu\n",
		     tm.tm_hour, tm.tm_min, tm.tm_sec,
		     tm.tm_year, tm.tm_mon + 1, tm.tm_mday, 0L);

	return p - buf;
}

static int ds1742_read_proc(char *page, char **start, off_t off, int count,
				int *eof, void *data)
{
	int len = get_ds1742_status(page);
	if (len <= off + count)
		*eof = 1;
	*start = page + off;
	len -= off;
	if (len > count)
		len = count;
	if (len < 0)
		len = 0;
	return len;
}

void rtc_ds1742_wait(void)
{
	while (CMOS_READ(RTC_SECONDS) & 1);
	while (!(CMOS_READ(RTC_SECONDS) & 1));
}

static int ds1742_ioctl(struct inode *inode, struct file *file,
				unsigned int cmd, unsigned long arg)
{
	struct rtc_time rtc_tm;
	ulong curr_time;

	switch (cmd) {
	case RTC_RD_TIME:	/* Read the time/date from RTC  */
		curr_time = rtc_ds1742_get_time();
		to_tm(curr_time, &rtc_tm);
		rtc_tm.tm_year -= 1900;
		return copy_to_user((void *) arg, &rtc_tm, sizeof(rtc_tm)) ? 
			-EFAULT : 0;
	case RTC_SET_TIME:	/* Set the RTC */
		if (!capable(CAP_SYS_TIME))
			return -EACCES;

		if (copy_from_user(&rtc_tm, 
				   (struct rtc_time *) arg,
		                   sizeof(struct rtc_time))) 
			return -EFAULT;

		curr_time = mktime(rtc_tm.tm_year + 1900,
				   rtc_tm.tm_mon + 1, /* tm_mon starts from 0 */
				   rtc_tm.tm_mday,
				   rtc_tm.tm_hour,
				   rtc_tm.tm_min, 
				   rtc_tm.tm_sec);
		return rtc_ds1742_set_time(curr_time);
	default:
		return -EINVAL;
	}
}

static int ds1742_open(struct inode *inode, struct file *file)
{
	spin_lock_irq(&rtc_lock);

	if (rtc_status & RTC_IS_OPEN) {
		spin_unlock_irq(&rtc_lock);
		return -EBUSY;
	}

	rtc_status |= RTC_IS_OPEN;

	spin_unlock_irq(&rtc_lock);
	return 0;
}

static int ds1742_release(struct inode *inode, struct file *file)
{
	spin_lock_irq(&rtc_lock);
	rtc_status &= ~RTC_IS_OPEN;
	spin_unlock_irq(&rtc_lock);
	return 0;
}

static struct file_operations ds1742_fops = {
	owner:THIS_MODULE,
	llseek:no_llseek,
	ioctl:ds1742_ioctl,
	open:ds1742_open,
	release:ds1742_release,
};

static struct miscdevice ds1742_dev = {
	RTC_MINOR,
	"rtc",
	&ds1742_fops
};

static int __init ds1742_init(void)
{
	printk(KERN_INFO "DS1742 Real Time Clock Driver v%s\n", DS1742_VERSION);
	misc_register(&ds1742_dev);
	create_proc_read_entry("driver/rtc", 0, 0, ds1742_read_proc, NULL);
	return 0;
}

static void __exit ds1742_exit(void)
{
	remove_proc_entry("driver/rtc", NULL);
	misc_deregister(&ds1742_dev);
}

module_init(ds1742_init);
module_exit(ds1742_exit);
EXPORT_NO_SYMBOLS;

MODULE_AUTHOR("Steven J. Hill");
MODULE_LICENSE("GPL");
