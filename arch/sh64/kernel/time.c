/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * arch/sh64/kernel/time.c
 *
 * Copyright (C) 2000, 2001  Paolo Alberelli
 *
 *    Original TMU/RTC code taken from sh version.
 *    Copyright (C) 1999  Tetsuya Okada & Niibe Yutaka
 *      Some code taken from i386 version.
 *      Copyright (C) 1991, 1992, 1995  Linus Torvalds
 */

#include <linux/config.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/param.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/smp.h>

#include <asm/registers.h>	 /* required by inline __asm__ stmt. */

#include <asm/processor.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/delay.h>

#include <linux/timex.h>
#include <linux/irq.h>
#include <asm/hardware.h>

#define TMU_TOCR_INIT	0x00
#define TMU0_TCR_INIT	0x0020
#define TMU_TSTR_INIT	1

/* RCR1 Bits */
#define RCR1_CF		0x80	/* Carry Flag             */
#define RCR1_CIE	0x10	/* Carry Interrupt Enable */
#define RCR1_AIE	0x08	/* Alarm Interrupt Enable */
#define RCR1_AF		0x01	/* Alarm Flag             */

/* RCR2 Bits */
#define RCR2_PEF	0x80	/* PEriodic interrupt Flag */
#define RCR2_PESMASK	0x70	/* Periodic interrupt Set  */
#define RCR2_RTCEN	0x08	/* ENable RTC              */
#define RCR2_ADJ	0x04	/* ADJustment (30-second)  */
#define RCR2_RESET	0x02	/* Reset bit               */
#define RCR2_START	0x01	/* Start bit               */

/* Clock, Power and Reset Controller */
#define	CPRC_BLOCK_OFF	0x01010000
#define CPRC_BASE	PHYS_PERIPHERAL_BLOCK + CPRC_BLOCK_OFF

#define FRQCR		(cprc_base+0x0)
#define WTCSR		(cprc_base+0x0018)
#define STBCR		(cprc_base+0x0030)

/* Time Management Unit */
#define	TMU_BLOCK_OFF	0x01020000
#define TMU_BASE	PHYS_PERIPHERAL_BLOCK + TMU_BLOCK_OFF
#define TMU0_BASE	tmu_base + 0x8 + (0xc * 0x0)
#define TMU1_BASE	tmu_base + 0x8 + (0xc * 0x1)
#define TMU2_BASE	tmu_base + 0x8 + (0xc * 0x2)

#define TMU_TOCR	tmu_base+0x0	/* Byte access */
#define TMU_TSTR	tmu_base+0x4	/* Byte access */

#define TMU0_TCOR	TMU0_BASE+0x0	/* Long access */
#define TMU0_TCNT	TMU0_BASE+0x4	/* Long access */
#define TMU0_TCR	TMU0_BASE+0x8	/* Word access */

/* Real Time Clock */
#define	RTC_BLOCK_OFF	0x01040000
#define RTC_BASE	PHYS_PERIPHERAL_BLOCK + RTC_BLOCK_OFF

#define R64CNT  	rtc_base+0x00
#define RSECCNT 	rtc_base+0x04
#define RMINCNT 	rtc_base+0x08
#define RHRCNT  	rtc_base+0x0c
#define RWKCNT  	rtc_base+0x10
#define RDAYCNT 	rtc_base+0x14
#define RMONCNT 	rtc_base+0x18
#define RYRCNT  	rtc_base+0x1c	/* 16bit */
#define RSECAR  	rtc_base+0x20
#define RMINAR  	rtc_base+0x24
#define RHRAR   	rtc_base+0x28
#define RWKAR   	rtc_base+0x2c
#define RDAYAR  	rtc_base+0x30
#define RMONAR  	rtc_base+0x34
#define RCR1    	rtc_base+0x38
#define RCR2    	rtc_base+0x3c

#ifndef BCD_TO_BIN
#define BCD_TO_BIN(val) ((val)=((val)&15) + ((val)>>4)*10)
#endif

#ifndef BIN_TO_BCD
#define BIN_TO_BCD(val) ((val)=(((val)/10)<<4) + (val)%10)
#endif

extern rwlock_t xtime_lock;
#define TICK_SIZE tick

extern unsigned long wall_jiffies;
extern unsigned long volatile jiffies;

static unsigned long tmu_base, rtc_base;
unsigned long cprc_base;

/* Variables to allow interpolation of time of day to resolution better than a
 * jiffy. */

/* This is effectively protected by xtime_lock */
static unsigned long ctc_last_interrupt;
static unsigned long long usecs_per_jiffy = 1000000/HZ; /* Approximation */

#define CTC_JIFFY_SCALE_SHIFT 40

/* 2**CTC_JIFFY_SCALE_SHIFT / ctc_ticks_per_jiffy */
static unsigned long long scaled_recip_ctc_ticks_per_jiffy;

/* Estimate number of microseconds that have elapsed since the last timer tick,
   by scaling the delta that has occured in the CTC register.

   WARNING WARNING WARNING : This algorithm relies on the CTC decrementing at
   the CPU clock rate.  If the CPU sleeps, the CTC stops counting.  Bear this
   in mind if enabling SLEEP_WORKS in process.c.  In that case, this algorithm
   probably needs to use TMU.TCNT0 instead.  This will work even if the CPU is
   sleeping, though will be coarser.

   FIXME : What if usecs_per_tick is moving around too much, e.g. if an adjtime
   is running or if the freq or tick arguments of adjtimex are modified after
   we have calibrated the scaling factor?  This will result in either a jump at
   the end of a tick period, or a wrap backwards at the start of the next one,
   if the application is reading the time of day often enough.  I think we
   ought to do better than this.  For this reason, usecs_per_jiffy is left
   separated out in the calculation below.  This allows some future hook into
   the adjtime-related stuff in kernel/timer.c to remove this hazard.

*/

static unsigned long usecs_since_tick(void)
{
	unsigned long long current_ctc;
	long ctc_ticks_since_interrupt;
	unsigned long long ull_ctc_ticks_since_interrupt;
	unsigned long result;

	unsigned long long mul1_out;
	unsigned long long mul1_out_high;
	unsigned long long mul2_out_low, mul2_out_high;
	
	/* Read CTC register */
	asm ("getcon cr62, %0" : "=r" (current_ctc));
	/* Note, the CTC counts down on each CPU clock, not up.
	   Note(2), use long type to get correct wraparound arithmetic when
	   the counter crosses zero. */
	ctc_ticks_since_interrupt = (long) ctc_last_interrupt - (long) current_ctc;
	ull_ctc_ticks_since_interrupt = (unsigned long long) ctc_ticks_since_interrupt;

	/* Inline assembly to do 32x32x32->64 multiplier */
	asm volatile ("mulu.l %1, %2, %0" :
	     "=r" (mul1_out) :
	     "r" (ull_ctc_ticks_since_interrupt), "r" (usecs_per_jiffy));
	
	mul1_out_high = mul1_out >> 32;

	asm volatile ("mulu.l %1, %2, %0" :
	     "=r" (mul2_out_low) :
	     "r" (mul1_out), "r" (scaled_recip_ctc_ticks_per_jiffy));

#if 1
	asm volatile ("mulu.l %1, %2, %0" :
	     "=r" (mul2_out_high) :
	     "r" (mul1_out_high), "r" (scaled_recip_ctc_ticks_per_jiffy));
#endif

	result = (unsigned long) (((mul2_out_high << 32) + mul2_out_low) >> CTC_JIFFY_SCALE_SHIFT);

	return result;
}

void do_gettimeofday(struct timeval *tv)
{
	unsigned long flags;
	unsigned long usec, sec;

	read_lock_irqsave(&xtime_lock, flags);
	usec = usecs_since_tick();
	{
		unsigned long lost = jiffies - wall_jiffies;
		if (lost)
			usec += lost * (1000000 / HZ);
	}
	sec = xtime.tv_sec;
	usec += xtime.tv_usec;
	read_unlock_irqrestore(&xtime_lock, flags);

	while (usec >= 1000000) {
		usec -= 1000000;
		sec++;
	}

	tv->tv_sec = sec;
	tv->tv_usec = usec;
}

void do_settimeofday(struct timeval *tv)
{
	write_lock_irq(&xtime_lock);
	xtime = *tv;
	time_adjust = 0;		/* stop active adjtime() */
	time_status |= STA_UNSYNC;
	time_maxerror = NTP_PHASE_LIMIT;
	time_esterror = NTP_PHASE_LIMIT;
	write_unlock_irq(&xtime_lock);
}

static int set_rtc_time(unsigned long nowtime)
{
	int retval = 0;
	int real_seconds, real_minutes, cmos_minutes;

	ctrl_outb(RCR2_RESET, RCR2);  /* Reset pre-scaler & stop RTC */

	cmos_minutes = ctrl_inb(RMINCNT);
	BCD_TO_BIN(cmos_minutes);

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
		BIN_TO_BCD(real_seconds);
		BIN_TO_BCD(real_minutes);
		ctrl_outb(real_seconds, RSECCNT);
		ctrl_outb(real_minutes, RMINCNT);
	} else {
		printk(KERN_WARNING
		       "set_rtc_time: can't update from %d to %d\n",
		       cmos_minutes, real_minutes);
		retval = -1;
	}

	ctrl_outb(RCR2_RTCEN|RCR2_START, RCR2);  /* Start RTC */

	return retval;
}

/* last time the RTC clock got updated */
static long last_rtc_update = 0;

static inline void sh64_do_profile(unsigned long pc)
{
	extern int _stext;

	/* Don't profile cpu_idle..  */
	if (!prof_buffer || !current->pid)
		return;

	pc -= (unsigned long) &_stext;
	pc >>= prof_shift;

	/*
	 * Don't ignore out-of-bounds PC values silently, put them into the
	 * last histogram slot, so if present, they will show up as a sharp
	 * peak.
	 */
	if (pc > prof_len - 1)
		pc = prof_len - 1;

	/* We could just be sloppy and not lock against a re-entry on this
	   increment, but the profiling code won't always be linked in anyway. */
	atomic_inc((atomic_t *)&prof_buffer[pc]);
}

/*
 * timer_interrupt() needs to keep up the real-time clock,
 * as well as call the "do_timer()" routine every clocktick
 */
static inline void do_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned long long current_ctc;
	asm ("getcon cr62, %0" : "=r" (current_ctc));
	ctc_last_interrupt = (unsigned long) current_ctc;
	
	do_timer(regs);

	if (!user_mode(regs))
		sh64_do_profile(regs->pc);

#ifdef CONFIG_HEARTBEAT
	extern void heartbeat(void);

        heartbeat();
#endif

	/*
	 * If we have an externally synchronized Linux clock, then update
	 * RTC clock accordingly every ~11 minutes. Set_rtc_mmss() has to be
	 * called as close as possible to 500 ms before the new second starts.
	 */
	if ((time_status & STA_UNSYNC) == 0 &&
	    xtime.tv_sec > last_rtc_update + 660 &&
	    xtime.tv_usec >= 500000 - ((unsigned) tick) / 2 &&
	    xtime.tv_usec <= 500000 + ((unsigned) tick) / 2) {
		if (set_rtc_time(xtime.tv_sec) == 0)
			last_rtc_update = xtime.tv_sec;
		else
			last_rtc_update = xtime.tv_sec - 600; /* do it again in 60 s */
	}
}

/*
 * This is the same as the above, except we _also_ save the current
 * Time Stamp Counter value at the time of the timer interrupt, so that
 * we later on can estimate the time of day more exactly.
 */
static void timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned long timer_status;

	/* Clear UNF bit */
	timer_status = ctrl_inw(TMU0_TCR);
	timer_status &= ~0x100;
	ctrl_outw(timer_status, TMU0_TCR);
	
	/*
	 * Here we are in the timer irq handler. We just have irqs locally
	 * disabled but we don't know if the timer_bh is running on the other
	 * CPU. We need to avoid to SMP race with it. NOTE: we don' t need
	 * the irq version of write_lock because as just said we have irq
	 * locally disabled. -arca
	 */
	write_lock(&xtime_lock);
	do_timer_interrupt(irq, NULL, regs);
	write_unlock(&xtime_lock);
}


static unsigned long get_rtc_time(void)
{
	unsigned int sec, min, hr, wk, day, mon, yr, yr100;

 again:
	do {
		ctrl_outb(0, RCR1);  /* Clear CF-bit */
		sec = ctrl_inb(RSECCNT);
		min = ctrl_inb(RMINCNT);
		hr  = ctrl_inb(RHRCNT);
		wk  = ctrl_inb(RWKCNT);
		day = ctrl_inb(RDAYCNT);
		mon = ctrl_inb(RMONCNT);
		yr  = ctrl_inw(RYRCNT);
		yr100 = (yr >> 8);
		yr &= 0xff;
	} while ((ctrl_inb(RCR1) & RCR1_CF) != 0);

	BCD_TO_BIN(yr100);
	BCD_TO_BIN(yr);
	BCD_TO_BIN(mon);
	BCD_TO_BIN(day);
	BCD_TO_BIN(hr);
	BCD_TO_BIN(min);
	BCD_TO_BIN(sec);

	if (yr > 99 || mon < 1 || mon > 12 || day > 31 || day < 1 ||
	    hr > 23 || min > 59 || sec > 59) {
		printk(KERN_ERR
		       "SH RTC: invalid value, resetting to 1 Jan 2000\n");
		ctrl_outb(RCR2_RESET, RCR2);  /* Reset & Stop */
		ctrl_outb(0, RSECCNT);
		ctrl_outb(0, RMINCNT);
		ctrl_outb(0, RHRCNT);
		ctrl_outb(6, RWKCNT);
		ctrl_outb(1, RDAYCNT);
		ctrl_outb(1, RMONCNT);
		ctrl_outw(0x2000, RYRCNT);
		ctrl_outb(RCR2_RTCEN|RCR2_START, RCR2);  /* Start */
		goto again;
	}

	return mktime(yr100 * 100 + yr, mon, day, hr, min, sec);
}

static __init unsigned int get_cpu_mhz(void)
{
	unsigned int count;
	unsigned long __dummy;
	
	/*
	** Regardless the toolchain, force the compiler to use the
	** arbitrary register r3 as a clock tick counter.
	** NOTE: r3 must be in accordance with rtc_interrupt()
	*/
	register unsigned long long  __clock_tick_count __asm__ ("r3");

	sti();
	do {} while (ctrl_inb(R64CNT) != 0);
	ctrl_outb(RCR1_CIE, RCR1); /* Enable carry interrupt */

	/*
	 * r3 is arbitrary. CDC does not support "=z".
	 */
	asm volatile("gettr	" __t0 ", %1\n\t"
		     "and	%2, r63, %2\n\t"
		     "_pta	4, " __t0 "\n\t"
		     "addi	%0, 1, %0\n\t"
		     "beq/l	%2, r63, " __t0 "\n\t"
		     "ptabs	%1, " __t0 "\n\t"
		: "=r"(count), "=r" (__dummy), "=r" (__clock_tick_count)
		: "0" (0));
	cli();
	/*
	 * SH-3:
	 * CPU clock = 4 stages * loop
	 * tst    rm,rm      if id ex
	 * bt/s   1b            if id ex
	 * add    #1,rd            if id ex
         *                            (if) pipe line stole
	 * tst    rm,rm                  if id ex
         * ....
	 *
	 *
	 * SH-4:
	 * CPU clock = 6 stages * loop
	 * I don't know why.
         * ....
	 *
	 * SH-5:
	 * CPU clock = 2 stages * loop
         * ....
	 *
	 */

#if defined (CONFIG_SH_SIMULATOR)
	/*
	 * Let's pretend we are a 5MHz SH-5 to avoid a too
	 * little timer interval. Also to keep delay
	 * calibration within a reasonable time.
	 */
	return 5000000;
#elif defined (CONFIG_ICACHE_DISABLED)
	/*
	 * Let's pretend we are a 300MHz SH-5.
	 */
	return 300000000;
#else
	/*
	 * This really is count by the number of clock cycles
         * per loop (2) by the ratio between a complete R64CNT
         * wrap-around (128) and CUI interrupt being raised (64).
	 */
	return count*2*2;
#endif
}

static void rtc_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	ctrl_outb(0, RCR1);	/* Disable Carry Interrupts */
	regs->regs[3] = 1;	/* Using r3 */
}

static struct irqaction irq0  = { timer_interrupt, SA_INTERRUPT, 0, "timer", NULL, NULL};
static struct irqaction irq1  = { rtc_interrupt, SA_INTERRUPT, 0, "rtc", NULL, NULL};

void __init time_init(void)
{
	unsigned int cpu_clock, master_clock, bus_clock, module_clock;
	unsigned long interval;
	unsigned long frqcr, ifc, pfc;
	static int ifc_table[] = { 2, 4, 6, 8, 10, 12, 16, 24 };
#define bfc_table ifc_table	/* Same */
#define pfc_table ifc_table	/* Same */

	tmu_base = onchip_remap(TMU_BASE, 1024, "TMU");
	if (tmu_base == 0UL) {
		panic("Unable to remap TMU\n");
	}

	rtc_base = onchip_remap(RTC_BASE, 1024, "RTC");
	if (rtc_base == 0UL) {
		panic("Unable to remap RTC\n");
	}

	cprc_base = onchip_remap(CPRC_BASE, 1024, "CPRC");
	if (cprc_base == 0UL) {
		panic("Unable to remap CPRC\n");
	}

	xtime.tv_sec = get_rtc_time();
	xtime.tv_usec = 0;

	setup_irq(TIMER_IRQ, &irq0);
	setup_irq(RTC_IRQ, &irq1);

	/* Check how fast it is.. */
	cpu_clock = get_cpu_mhz();

	/* FIXME : Are these divides OK?  Note careful order of operations to
	 * maintain reasonable precision and avoid overflow. */
	scaled_recip_ctc_ticks_per_jiffy = ((1ULL << CTC_JIFFY_SCALE_SHIFT) / (unsigned long long)(cpu_clock / HZ));
	
	disable_irq(RTC_IRQ);

	printk("CPU clock: %d.%02dMHz\n",
	       (cpu_clock / 1000000), (cpu_clock % 1000000)/10000);
	{
		unsigned short bfc;
		frqcr = ctrl_inl(FRQCR);
		ifc  = ifc_table[(frqcr>> 6) & 0x0007];
		bfc  = bfc_table[(frqcr>> 3) & 0x0007];
		pfc  = pfc_table[(frqcr>> 12) & 0x0007];
		master_clock = cpu_clock * ifc;
		bus_clock = master_clock/bfc;
	}

	printk("Bus clock: %d.%02dMHz\n",
	       (bus_clock/1000000), (bus_clock % 1000000)/10000);
	module_clock = master_clock/pfc;
	printk("Module clock: %d.%02dMHz\n",
	       (module_clock/1000000), (module_clock % 1000000)/10000);
	interval = (module_clock/(HZ*4));

	printk("Interval = %ld\n", interval);

	current_cpu_data.cpu_clock    = cpu_clock;
	current_cpu_data.master_clock = master_clock;
	current_cpu_data.bus_clock    = bus_clock;
	current_cpu_data.module_clock = module_clock;

	/* Start TMU0 */
	ctrl_outb(TMU_TOCR_INIT, TMU_TOCR);
	ctrl_outw(TMU0_TCR_INIT, TMU0_TCR);
	ctrl_outl(interval, TMU0_TCOR);
	ctrl_outl(interval, TMU0_TCNT);
	ctrl_outb(TMU_TSTR_INIT, TMU_TSTR);
}

void enter_deep_standby(void)
{
	/* Disable watchdog timer */
	ctrl_outl(0xa5000000, WTCSR);
	/* Configure deep standby on sleep */
	ctrl_outl(0x03, STBCR);

#ifdef CONFIG_SH_CAYMAN
	{
		extern void mach_alphanum(int position, unsigned char value);
		extern void mach_alphanum_brightness(int setting);
		char halted[] = "Halted. ";
		int i;
		mach_alphanum_brightness(6); /* dimmest setting above off */
		for (i=0; i<8; i++) {
			mach_alphanum(i, halted[i]);
		}
		asm __volatile__ ("synco");
	}
#endif

	asm __volatile__ ("sleep");
	asm __volatile__ ("synci");
	asm __volatile__ ("nop");
	asm __volatile__ ("nop");
	asm __volatile__ ("nop");
	asm __volatile__ ("nop");
	panic("Unexpected wakeup!\n");
}

