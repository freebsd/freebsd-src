/****************************************************************************/
/*
 * linux/arch/sh/kernel/rtc-snapgear.c -- Secureedge5410 RTC code
 *
 *  Copyright (C) 2002  David McCullough <davidm@snapgear.com>
 *
 * The SecureEdge5410 can have one of 2 real time clocks, the SH
 * built in version or the preferred external DS1302.  Here we work out
 * each to see what we have and then run with it.
 */
/****************************************************************************/

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/rtc.h>
#include <linux/mc146818rtc.h>

#include <asm/io.h>
#include <asm/rtc.h>
#include <asm/mc146818rtc.h>

/****************************************************************************/

static int use_ds1302 = 0;

/****************************************************************************/
/*
 *	we need to implement a DS1302 driver here that can operate in
 *	conjunction with the builtin rtc driver which is already quite friendly
 */
/*****************************************************************************/

#define	RTC_CMD_READ	0x81		/* Read command */
#define	RTC_CMD_WRITE	0x80		/* Write command */

#define	RTC_ADDR_YEAR	0x06		/* Address of year register */
#define	RTC_ADDR_DAY	0x05		/* Address of day of week register */
#define	RTC_ADDR_MON	0x04		/* Address of month register */
#define	RTC_ADDR_DATE	0x03		/* Address of day of month register */
#define	RTC_ADDR_HOUR	0x02		/* Address of hour register */
#define	RTC_ADDR_MIN	0x01		/* Address of minute register */
#define	RTC_ADDR_SEC	0x00		/* Address of second register */

#define	RTC_RESET	0x1000
#define	RTC_IODATA	0x0800
#define	RTC_SCLK	0x0400

#define set_dirp(x)
#define get_dirp(x) 0
#define set_dp(x)	SECUREEDGE_WRITE_IOPORT(x, 0x1c00)
#define get_dp(x)	SECUREEDGE_READ_IOPORT()

static void ds1302_sendbits(unsigned int val)
{
	int	i;

	for (i = 8; (i); i--, val >>= 1) {
		set_dp((get_dp() & ~RTC_IODATA) | ((val & 0x1) ? RTC_IODATA : 0));
		set_dp(get_dp() | RTC_SCLK);	// clock high
		set_dp(get_dp() & ~RTC_SCLK);	// clock low
	}
}

static unsigned int ds1302_recvbits(void)
{
	unsigned int	val;
	int		i;

	for (i = 0, val = 0; (i < 8); i++) {
		val |= (((get_dp() & RTC_IODATA) ? 1 : 0) << i);
		set_dp(get_dp() | RTC_SCLK);	// clock high
		set_dp(get_dp() & ~RTC_SCLK);	// clock low
	}
	return(val);
}

static unsigned int ds1302_readbyte(unsigned int addr)
{
	unsigned int	val;
	unsigned long	flags;

#if 0
	printk("SnapGear RTC: ds1302_readbyte(addr=%x)\n", addr);
#endif

	save_flags(flags); cli();
	set_dirp(get_dirp() | RTC_RESET | RTC_IODATA | RTC_SCLK);
	set_dp(get_dp() & ~(RTC_RESET | RTC_IODATA | RTC_SCLK));

	set_dp(get_dp() | RTC_RESET);
	ds1302_sendbits(((addr & 0x3f) << 1) | RTC_CMD_READ);
	set_dirp(get_dirp() & ~RTC_IODATA);
	val = ds1302_recvbits();
	set_dp(get_dp() & ~RTC_RESET);
	restore_flags(flags);

	return(val);
}

static void ds1302_writebyte(unsigned int addr, unsigned int val)
{
	unsigned long	flags;

#if 0
	printk("SnapGear RTC: ds1302_writebyte(addr=%x)\n", addr);
#endif

	save_flags(flags); cli();
	set_dirp(get_dirp() | RTC_RESET | RTC_IODATA | RTC_SCLK);
	set_dp(get_dp() & ~(RTC_RESET | RTC_IODATA | RTC_SCLK));
	set_dp(get_dp() | RTC_RESET);
	ds1302_sendbits(((addr & 0x3f) << 1) | RTC_CMD_WRITE);
	ds1302_sendbits(val);
	set_dp(get_dp() & ~RTC_RESET);
	restore_flags(flags);
}

static void ds1302_reset(void)
{
	unsigned long	flags;
	/* Hardware dependant reset/init */
	save_flags(flags); cli();
	set_dirp(get_dirp() | RTC_RESET | RTC_IODATA | RTC_SCLK);
	set_dp(get_dp() & ~(RTC_RESET | RTC_IODATA | RTC_SCLK));
	restore_flags(flags);
}

/*****************************************************************************/

static inline int bcd2int(int val)
{
	return((((val & 0xf0) >> 4) * 10) + (val & 0xf));
}

static inline int int2bcd(int val)
{
	return(((val / 10) << 4) + (val % 10));
}

/*****************************************************************************/
/*
 *	Write and Read some RAM in the DS1302,  if it works assume it's there
 *	Otherwise use the SH4 internal RTC
 */

void __init secureedge5410_rtc_init(void)
{
	unsigned char *test = "snapgear";
	int i;

	ds1302_reset();

	use_ds1302 = 1;

	for (i = 0; test[i]; i++)
		ds1302_writebyte(32 + i, test[i]);

	for (i = 0; test[i]; i++)
		if (ds1302_readbyte(32 + i) != test[i]) {
			use_ds1302 = 0;
			break;
		}

	printk("SnapGear RTC: using %s rtc.\n", use_ds1302 ? "ds1302" : "internal");
}

/****************************************************************************/
/*
 *	our generic interface that chooses the correct code to use
 */

void snapgear_rtc_gettimeofday(struct timeval *tv)
{
	unsigned int sec, min, hr, day, mon, yr;

	if (!use_ds1302) {
		sh_rtc_gettimeofday(tv);
		return;
	}

 	sec = bcd2int(ds1302_readbyte(RTC_ADDR_SEC));
 	min = bcd2int(ds1302_readbyte(RTC_ADDR_MIN));
 	hr  = bcd2int(ds1302_readbyte(RTC_ADDR_HOUR));
 	day = bcd2int(ds1302_readbyte(RTC_ADDR_DATE));
 	mon = bcd2int(ds1302_readbyte(RTC_ADDR_MON));
 	yr  = bcd2int(ds1302_readbyte(RTC_ADDR_YEAR));

bad_time:
	if (yr > 99 || mon < 1 || mon > 12 || day > 31 || day < 1 ||
	    hr > 23 || min > 59 || sec > 59) {
		printk(KERN_ERR
		       "SnapGear RTC: invalid value, resetting to 1 Jan 2000\n");
		ds1302_writebyte(RTC_ADDR_MIN,  min = 0);
		ds1302_writebyte(RTC_ADDR_HOUR, hr  = 0);
		ds1302_writebyte(RTC_ADDR_DAY,        7);
		ds1302_writebyte(RTC_ADDR_DATE, day = 1);
		ds1302_writebyte(RTC_ADDR_MON,  mon = 1);
		ds1302_writebyte(RTC_ADDR_YEAR, yr  = 0);
		ds1302_writebyte(RTC_ADDR_SEC,  sec = 0);
	}

	tv->tv_sec = mktime(2000 + yr, mon, day, hr, min, sec);
	if (tv->tv_sec < 0) {
#if 0
		printk("BAD TIME %d %d %d %d %d %d\n", yr, mon, day, hr, min, sec);
#endif
		yr = 100;
		goto bad_time;
	}
	tv->tv_usec = 0;
}

int snapgear_rtc_settimeofday(const struct timeval *tv)
{
	int retval = 0;
	int real_seconds, real_minutes, cmos_minutes;
	unsigned long nowtime;

	if (!use_ds1302) {
		sh_rtc_settimeofday(tv);
		return(0);
	}

/*
 *	This is called direct from the kernel timer handling code.
 *	It is supposed to synchronize the kernel clock to the RTC.
 */

	nowtime = tv->tv_sec;

#if 1
	printk("SnapGear RTC: snapgear_rtc_settimeofday(nowtime=%ld)\n", nowtime);
#endif

	/* STOP RTC */
	ds1302_writebyte(RTC_ADDR_SEC, ds1302_readbyte(RTC_ADDR_SEC) | 0x80);

	cmos_minutes = bcd2int(ds1302_readbyte(RTC_ADDR_MIN));

	/*
	 * since we're only adjusting minutes and seconds,
	 * don't interfere with hour overflow. This avoids
	 * messing with unknown time zones but requires your
	 * RTC not to be off by more than 15 minutes
	 */
	real_seconds = nowtime % 60;
	real_minutes = nowtime / 60;
	if (((abs(real_minutes - cmos_minutes) + 15)/30) & 1)
		real_minutes += 30;	/* correct for half hour time zone */
	real_minutes %= 60;

	if (abs(real_minutes - cmos_minutes) < 30) {
		ds1302_writebyte(RTC_ADDR_MIN, int2bcd(real_minutes));
		ds1302_writebyte(RTC_ADDR_SEC, int2bcd(real_seconds));
	} else {
		printk(KERN_WARNING
		       "SnapGear RTC: can't update from %d to %d\n",
		       cmos_minutes, real_minutes);
		retval = -1;
	}

	/* START RTC */
	ds1302_writebyte(RTC_ADDR_SEC, ds1302_readbyte(RTC_ADDR_SEC) & ~0x80);
	return(0);
}

unsigned char secureedge5410_cmos_read(int addr)
{
	unsigned char val = 0;

	if (!use_ds1302)
		return(__CMOS_READ(addr, w));

	switch(addr) {
	case RTC_SECONDS:       val = ds1302_readbyte(RTC_ADDR_SEC);  break;
	case RTC_SECONDS_ALARM:                                       break;
	case RTC_MINUTES:       val = ds1302_readbyte(RTC_ADDR_MIN);  break;
	case RTC_MINUTES_ALARM:                                       break;
	case RTC_HOURS:         val = ds1302_readbyte(RTC_ADDR_HOUR); break;
	case RTC_HOURS_ALARM:                                         break;
	case RTC_DAY_OF_WEEK:   val = ds1302_readbyte(RTC_ADDR_DAY);  break;
	case RTC_DAY_OF_MONTH:  val = ds1302_readbyte(RTC_ADDR_DATE); break;
	case RTC_MONTH:         val = ds1302_readbyte(RTC_ADDR_MON);  break;
	case RTC_YEAR:          val = ds1302_readbyte(RTC_ADDR_YEAR); break;
	case RTC_REG_A:         /* RTC_FREQ_SELECT */                 break;
	case RTC_REG_B:	        /* RTC_CONTROL */                     break;
	case RTC_REG_C:	        /* RTC_INTR_FLAGS */                  break;
	case RTC_REG_D:         val = RTC_VRT /* RTC_VALID */;        break;
	default:                                                      break;
	}

	return(val);
}

void secureedge5410_cmos_write(unsigned char val, int addr)
{
	if (!use_ds1302) {
		__CMOS_WRITE(val, addr, w);
		return;
	}

	switch(addr) {
	case RTC_SECONDS:       ds1302_writebyte(RTC_ADDR_SEC, val);  break;
	case RTC_SECONDS_ALARM:                                       break;
	case RTC_MINUTES:       ds1302_writebyte(RTC_ADDR_MIN, val);  break;
	case RTC_MINUTES_ALARM:                                       break;
	case RTC_HOURS:         ds1302_writebyte(RTC_ADDR_HOUR, val); break;
	case RTC_HOURS_ALARM:                                         break;
	case RTC_DAY_OF_WEEK:   ds1302_writebyte(RTC_ADDR_DAY, val);  break;
	case RTC_DAY_OF_MONTH:  ds1302_writebyte(RTC_ADDR_DATE, val); break;
	case RTC_MONTH:         ds1302_writebyte(RTC_ADDR_MON, val);  break;
	case RTC_YEAR:          ds1302_writebyte(RTC_ADDR_YEAR, val); break;
	case RTC_REG_A:         /* RTC_FREQ_SELECT */                 break;
	case RTC_REG_B:        	/* RTC_CONTROL */                     break;
	case RTC_REG_C:	        /* RTC_INTR_FLAGS */                  break;
	case RTC_REG_D:	        /* RTC_VALID */                       break;
	default:                                                      break;
	}
}

/****************************************************************************/
