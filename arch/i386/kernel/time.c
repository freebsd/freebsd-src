/*
 *  linux/arch/i386/kernel/time.c
 *
 *  Copyright (C) 1991, 1992, 1995  Linus Torvalds
 *
 * This file contains the PC-specific time handling details:
 * reading the RTC at bootup, etc..
 * 1994-07-02    Alan Modra
 *	fixed set_rtc_mmss, fixed time.year for >= 2000, new mktime
 * 1995-03-26    Markus Kuhn
 *      fixed 500 ms bug at call to set_rtc_mmss, fixed DS12887
 *      precision CMOS clock update
 * 1996-05-03    Ingo Molnar
 *      fixed time warps in do_[slow|fast]_gettimeoffset()
 * 1997-09-10	Updated NTP code according to technical memorandum Jan '96
 *		"A Kernel Model for Precision Timekeeping" by Dave Mills
 * 1998-09-05    (Various)
 *	More robust do_fast_gettimeoffset() algorithm implemented
 *	(works with APM, Cyrix 6x86MX and Centaur C6),
 *	monotonic gettimeofday() with fast_get_timeoffset(),
 *	drift-proof precision TSC calibration on boot
 *	(C. Scott Ananian <cananian@alumni.princeton.edu>, Andrew D.
 *	Balsa <andrebalsa@altern.org>, Philip Gladstone <philip@raptor.com>;
 *	ported from 2.0.35 Jumbo-9 by Michael Krause <m.krause@tu-harburg.de>).
 * 1998-12-16    Andrea Arcangeli
 *	Fixed Jumbo-9 code in 2.1.131: do_gettimeofday was missing 1 jiffy
 *	because was not accounting lost_ticks.
 * 1998-12-24 Copyright (C) 1998  Andrea Arcangeli
 *	Fixed a xtime SMP race (we need the xtime_lock rw spinlock to
 *	serialize accesses to xtime/lost_ticks).
 */

#include <linux/errno.h>
#include <linux/module.h>
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

#include <asm/io.h>
#include <asm/smp.h>
#include <asm/irq.h>
#include <asm/msr.h>
#include <asm/delay.h>
#include <asm/mpspec.h>
#include <asm/uaccess.h>
#include <asm/processor.h>

#include <linux/mc146818rtc.h>
#include <linux/timex.h>
#include <linux/config.h>

#include <asm/fixmap.h>
#include <asm/cobalt.h>

/*
 * for x86_do_profile()
 */
#include <linux/irq.h>


unsigned long cpu_khz;	/* Detected as we calibrate the TSC */

/* Number of usecs that the last interrupt was delayed */
static int delay_at_last_interrupt;

static unsigned long last_tsc_low; /* lsb 32 bits of Time Stamp Counter */

/* Cached *multiplier* to convert TSC counts to microseconds.
 * (see the equation below).
 * Equal to 2^32 * (1 / (clocks per usec) ).
 * Initialized in time_init.
 */
unsigned long fast_gettimeoffset_quotient;

extern rwlock_t xtime_lock;
extern unsigned long wall_jiffies;

spinlock_t rtc_lock = SPIN_LOCK_UNLOCKED;

static inline unsigned long do_fast_gettimeoffset(void)
{
	register unsigned long eax, edx;

	/* Read the Time Stamp Counter */

	rdtsc(eax,edx);

	/* .. relative to previous jiffy (32 bits is enough) */
	eax -= last_tsc_low;	/* tsc_low delta */

	/*
         * Time offset = (tsc_low delta) * fast_gettimeoffset_quotient
         *             = (tsc_low delta) * (usecs_per_clock)
         *             = (tsc_low delta) * (usecs_per_jiffy / clocks_per_jiffy)
	 *
	 * Using a mull instead of a divl saves up to 31 clock cycles
	 * in the critical path.
         */

	__asm__("mull %2"
		:"=a" (eax), "=d" (edx)
		:"rm" (fast_gettimeoffset_quotient),
		 "0" (eax));

	/* our adjusted time offset in microseconds */
	return delay_at_last_interrupt + edx;
}

#define TICK_SIZE tick

spinlock_t i8253_lock = SPIN_LOCK_UNLOCKED;

EXPORT_SYMBOL(i8253_lock);

extern spinlock_t i8259A_lock;

#ifndef CONFIG_X86_TSC

/* This function must be called with interrupts disabled 
 * It was inspired by Steve McCanne's microtime-i386 for BSD.  -- jrs
 * 
 * However, the pc-audio speaker driver changes the divisor so that
 * it gets interrupted rather more often - it loads 64 into the
 * counter rather than 11932! This has an adverse impact on
 * do_gettimeoffset() -- it stops working! What is also not
 * good is that the interval that our timer function gets called
 * is no longer 10.0002 ms, but 9.9767 ms. To get around this
 * would require using a different timing source. Maybe someone
 * could use the RTC - I know that this can interrupt at frequencies
 * ranging from 8192Hz to 2Hz. If I had the energy, I'd somehow fix
 * it so that at startup, the timer code in sched.c would select
 * using either the RTC or the 8253 timer. The decision would be
 * based on whether there was any other device around that needed
 * to trample on the 8253. I'd set up the RTC to interrupt at 1024 Hz,
 * and then do some jiggery to have a version of do_timer that 
 * advanced the clock by 1/1024 s. Every time that reached over 1/100
 * of a second, then do all the old code. If the time was kept correct
 * then do_gettimeoffset could just return 0 - there is no low order
 * divider that can be accessed.
 *
 * Ideally, you would be able to use the RTC for the speaker driver,
 * but it appears that the speaker driver really needs interrupt more
 * often than every 120 us or so.
 *
 * Anyway, this needs more thought....		pjsg (1993-08-28)
 * 
 * If you are really that interested, you should be reading
 * comp.protocols.time.ntp!
 */

static unsigned long do_slow_gettimeoffset(void)
{
	int count;

	static int count_p = LATCH;    /* for the first call after boot */
	static unsigned long jiffies_p = 0;

	/*
	 * cache volatile jiffies temporarily; we have IRQs turned off. 
	 */
	unsigned long jiffies_t;

	/* gets recalled with irq locally disabled */
	spin_lock(&i8253_lock);
	/* timer count may underflow right here */
	outb_p(0x00, 0x43);	/* latch the count ASAP */

	count = inb_p(0x40);	/* read the latched count */

	/*
	 * We do this guaranteed double memory access instead of a _p 
	 * postfix in the previous port access. Wheee, hackady hack
	 */
 	jiffies_t = jiffies;

	count |= inb_p(0x40) << 8;
	
        /* VIA686a test code... reset the latch if count > max + 1 */
        if (count > LATCH) {
                outb_p(0x34, 0x43);
                outb_p(LATCH & 0xff, 0x40);
                outb(LATCH >> 8, 0x40);
                count = LATCH - 1;
        }
	
	spin_unlock(&i8253_lock);

	/*
	 * avoiding timer inconsistencies (they are rare, but they happen)...
	 * there are two kinds of problems that must be avoided here:
	 *  1. the timer counter underflows
	 *  2. hardware problem with the timer, not giving us continuous time,
	 *     the counter does small "jumps" upwards on some Pentium systems,
	 *     (see c't 95/10 page 335 for Neptun bug.)
	 */

/* you can safely undefine this if you don't have the Neptune chipset */

#define BUGGY_NEPTUN_TIMER

	if( jiffies_t == jiffies_p ) {
		if( count > count_p ) {
			/* the nutcase */

			int i;

			spin_lock(&i8259A_lock);
			/*
			 * This is tricky when I/O APICs are used;
			 * see do_timer_interrupt().
			 */
			i = inb(0x20);
			spin_unlock(&i8259A_lock);

			/* assumption about timer being IRQ0 */
			if (i & 0x01) {
				/*
				 * We cannot detect lost timer interrupts ... 
				 * well, that's why we call them lost, don't we? :)
				 * [hmm, on the Pentium and Alpha we can ... sort of]
				 */
				count -= LATCH;
			} else {
#ifdef BUGGY_NEPTUN_TIMER
				/*
				 * for the Neptun bug we know that the 'latch'
				 * command doesnt latch the high and low value
				 * of the counter atomically. Thus we have to 
				 * substract 256 from the counter 
				 * ... funny, isnt it? :)
				 */

				count -= 256;
#else
				printk("do_slow_gettimeoffset(): hardware timer problem?\n");
#endif
			}
		}
	} else
		jiffies_p = jiffies_t;

	count_p = count;

	count = ((LATCH-1) - count) * TICK_SIZE;
	count = (count + LATCH/2) / LATCH;

	return count;
}

static unsigned long (*do_gettimeoffset)(void) = do_slow_gettimeoffset;


/* IBM Summit (EXA) Cyclone Timer code*/
#ifdef CONFIG_X86_SUMMIT

#define CYCLONE_CBAR_ADDR 0xFEB00CD0
#define CYCLONE_PMCC_OFFSET 0x51A0
#define CYCLONE_MPMC_OFFSET 0x51D0
#define CYCLONE_MPCS_OFFSET 0x51A8
#define CYCLONE_TIMER_FREQ 100000000

int use_cyclone = 0;
int __init cyclone_setup(char *str) 
{
	use_cyclone = 1;
	return 1;
}

static u32* volatile cyclone_timer;	/* Cyclone MPMC0 register */
static u32 last_cyclone_timer;

static inline void mark_timeoffset_cyclone(void)
{
	int count;
	unsigned long lost;
	unsigned long delta = last_cyclone_timer;
	spin_lock(&i8253_lock);
	/* quickly read the cyclone timer */
	if(cyclone_timer)
		last_cyclone_timer = cyclone_timer[0];

	/* calculate delay_at_last_interrupt */
	outb_p(0x00, 0x43);     /* latch the count ASAP */

	count = inb_p(0x40);    /* read the latched count */
	count |= inb(0x40) << 8;
	spin_unlock(&i8253_lock);

	/*lost tick compensation*/
	delta = last_cyclone_timer - delta;	
	delta /= (CYCLONE_TIMER_FREQ/1000000);
	delta += delay_at_last_interrupt;
	lost = delta/(1000000/HZ);
	if (lost >= 2)
		jiffies += lost-1;
               
	count = ((LATCH-1) - count) * TICK_SIZE;
	delay_at_last_interrupt = (count + LATCH/2) / LATCH;
}

static unsigned long do_gettimeoffset_cyclone(void)
{
	u32 offset;

	if(!cyclone_timer)
		return delay_at_last_interrupt;

	/* Read the cyclone timer */
	offset = cyclone_timer[0];

	/* .. relative to previous jiffy */
	offset = offset - last_cyclone_timer;

	/* convert cyclone ticks to microseconds */	
	/* XXX slow, can we speed this up? */
	offset = offset/(CYCLONE_TIMER_FREQ/1000000);

	/* our adjusted time offset in microseconds */
	return delay_at_last_interrupt + offset;
}

static void __init init_cyclone_clock(void)
{
	u32* reg;	
	u32 base;		/* saved cyclone base address */
	u32 pageaddr;	/* page that contains cyclone_timer register */
	u32 offset;		/* offset from pageaddr to cyclone_timer register */
	int i;
	
	printk(KERN_INFO "Summit chipset: Starting Cyclone Counter.\n");

	/* find base address */
	pageaddr = (CYCLONE_CBAR_ADDR)&PAGE_MASK;
	offset = (CYCLONE_CBAR_ADDR)&(~PAGE_MASK);
	set_fixmap_nocache(FIX_CYCLONE_TIMER, pageaddr);
	reg = (u32*)(fix_to_virt(FIX_CYCLONE_TIMER) + offset);
	if(!reg){
		printk(KERN_ERR "Summit chipset: Could not find valid CBAR register.\n");
		use_cyclone = 0;
		return;
	}
	base = *reg;	
	if(!base){
		printk(KERN_ERR "Summit chipset: Could not find valid CBAR value.\n");
		use_cyclone = 0;
		return;
	}
	
	/* setup PMCC */
	pageaddr = (base + CYCLONE_PMCC_OFFSET)&PAGE_MASK;
	offset = (base + CYCLONE_PMCC_OFFSET)&(~PAGE_MASK);
	set_fixmap_nocache(FIX_CYCLONE_TIMER, pageaddr);
	reg = (u32*)(fix_to_virt(FIX_CYCLONE_TIMER) + offset);
	if(!reg){
		printk(KERN_ERR "Summit chipset: Could not find valid PMCC register.\n");
		use_cyclone = 0;
		return;
	}
	reg[0] = 0x00000001;

	/* setup MPCS */
	pageaddr = (base + CYCLONE_MPCS_OFFSET)&PAGE_MASK;
	offset = (base + CYCLONE_MPCS_OFFSET)&(~PAGE_MASK);
	set_fixmap_nocache(FIX_CYCLONE_TIMER, pageaddr);
	reg = (u32*)(fix_to_virt(FIX_CYCLONE_TIMER) + offset);
	if(!reg){
		printk(KERN_ERR "Summit chipset: Could not find valid MPCS register.\n");
		use_cyclone = 0;
		return;
	}
	reg[0] = 0x00000001;

	/* map in cyclone_timer */
	pageaddr = (base + CYCLONE_MPMC_OFFSET)&PAGE_MASK;
	offset = (base + CYCLONE_MPMC_OFFSET)&(~PAGE_MASK);
	set_fixmap_nocache(FIX_CYCLONE_TIMER, pageaddr);
	cyclone_timer = (u32*)(fix_to_virt(FIX_CYCLONE_TIMER) + offset);
	if(!cyclone_timer){
		printk(KERN_ERR "Summit chipset: Could not find valid MPMC register.\n");
		use_cyclone = 0;
		return;
	}

	/*quick test to make sure its ticking*/
	for(i=0; i<3; i++){
		u32 old = cyclone_timer[0];
		int stall = 100;
		while(stall--) barrier();
		if(cyclone_timer[0] == old){
			printk(KERN_ERR "Summit chipset: Counter not counting! DISABLED\n");
			cyclone_timer = 0;
			use_cyclone = 0;
			return;
		}
	}
	/* Everything looks good, so set do_gettimeoffset */
	do_gettimeoffset = do_gettimeoffset_cyclone;	
}
void __cyclone_delay(unsigned long loops)
{
	unsigned long bclock, now;
	if(!cyclone_timer)
		return;
	bclock = cyclone_timer[0];
	do {
		rep_nop();
		now = cyclone_timer[0];
	} while ((now-bclock) < loops);
}
#endif /* CONFIG_X86_SUMMIT */

#else

#define do_gettimeoffset()	do_fast_gettimeoffset()

#endif

/* No-cyclone stubs */
#ifndef CONFIG_X86_SUMMIT
int __init cyclone_setup(char *str) 
{
	printk(KERN_ERR "cyclone: Kernel not compiled with CONFIG_X86_SUMMIT, cannot use the cyclone-timer.\n");
	return 1;
}

const int use_cyclone = 0;
static void mark_timeoffset_cyclone(void) {}
static void init_cyclone_clock(void) {}
void __cyclone_delay(unsigned long loops) {}
#endif /* CONFIG_X86_SUMMIT */

/*
 * This version of gettimeofday has microsecond resolution
 * and better than microsecond precision on fast x86 machines with TSC.
 */
void do_gettimeofday(struct timeval *tv)
{
	unsigned long flags;
	unsigned long usec, sec;

	read_lock_irqsave(&xtime_lock, flags);
	usec = do_gettimeoffset();
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
	/*
	 * This is revolting. We need to set "xtime" correctly. However, the
	 * value in this location is the value at the most recent update of
	 * wall time.  Discover what correction gettimeofday() would have
	 * made, and then undo it!
	 */
	tv->tv_usec -= do_gettimeoffset();
	tv->tv_usec -= (jiffies - wall_jiffies) * (1000000 / HZ);

	while (tv->tv_usec < 0) {
		tv->tv_usec += 1000000;
		tv->tv_sec--;
	}

	xtime = *tv;
	time_adjust = 0;		/* stop active adjtime() */
	time_status |= STA_UNSYNC;
	time_maxerror = NTP_PHASE_LIMIT;
	time_esterror = NTP_PHASE_LIMIT;
	write_unlock_irq(&xtime_lock);
}

/*
 * In order to set the CMOS clock precisely, set_rtc_mmss has to be
 * called 500 ms after the second nowtime has started, because when
 * nowtime is written into the registers of the CMOS clock, it will
 * jump to the next second precisely 500 ms later. Check the Motorola
 * MC146818A or Dallas DS12887 data sheet for details.
 *
 * BUG: This routine does not handle hour overflow properly; it just
 *      sets the minutes. Usually you'll only notice that after reboot!
 */
static int set_rtc_mmss(unsigned long nowtime)
{
	int retval = 0;
	int real_seconds, real_minutes, cmos_minutes;
	unsigned char save_control, save_freq_select;

	/* gets recalled with irq locally disabled */
	spin_lock(&rtc_lock);
	save_control = CMOS_READ(RTC_CONTROL); /* tell the clock it's being set */
	CMOS_WRITE((save_control|RTC_SET), RTC_CONTROL);

	save_freq_select = CMOS_READ(RTC_FREQ_SELECT); /* stop and reset prescaler */
	CMOS_WRITE((save_freq_select|RTC_DIV_RESET2), RTC_FREQ_SELECT);

	cmos_minutes = CMOS_READ(RTC_MINUTES);
	if (!(save_control & RTC_DM_BINARY) || RTC_ALWAYS_BCD)
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
		real_minutes += 30;		/* correct for half hour time zone */
	real_minutes %= 60;

	if (abs(real_minutes - cmos_minutes) < 30) {
		if (!(save_control & RTC_DM_BINARY) || RTC_ALWAYS_BCD) {
			BIN_TO_BCD(real_seconds);
			BIN_TO_BCD(real_minutes);
		}
		CMOS_WRITE(real_seconds,RTC_SECONDS);
		CMOS_WRITE(real_minutes,RTC_MINUTES);
	} else {
		printk(KERN_WARNING
		       "set_rtc_mmss: can't update from %d to %d\n",
		       cmos_minutes, real_minutes);
		retval = -1;
	}

	/* The following flags have to be released exactly in this order,
	 * otherwise the DS12887 (popular MC146818A clone with integrated
	 * battery and quartz) will not reset the oscillator and will not
	 * update precisely 500 ms later. You won't find this mentioned in
	 * the Dallas Semiconductor data sheets, but who believes data
	 * sheets anyway ...                           -- Markus Kuhn
	 */
	CMOS_WRITE(save_control, RTC_CONTROL);
	CMOS_WRITE(save_freq_select, RTC_FREQ_SELECT);
	spin_unlock(&rtc_lock);

	return retval;
}

/* last time the cmos clock got updated */
static long last_rtc_update;

int timer_ack;

/*
 * timer_interrupt() needs to keep up the real-time clock,
 * as well as call the "do_timer()" routine every clocktick
 */
static inline void do_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
#ifdef CONFIG_X86_IO_APIC
	if (timer_ack) {
		/*
		 * Subtle, when I/O APICs are used we have to ack timer IRQ
		 * manually to reset the IRR bit for do_slow_gettimeoffset().
		 * This will also deassert NMI lines for the watchdog if run
		 * on an 82489DX-based system.
		 */
		spin_lock(&i8259A_lock);
		outb(0x0c, 0x20);
		/* Ack the IRQ; AEOI will end it automatically. */
		inb(0x20);
		spin_unlock(&i8259A_lock);
	}
#endif

#ifdef CONFIG_VISWS
	/* Clear the interrupt */
	co_cpu_write(CO_CPU_STAT,co_cpu_read(CO_CPU_STAT) & ~CO_STAT_TIMEINTR);
#endif
	do_timer(regs);
/*
 * In the SMP case we use the local APIC timer interrupt to do the
 * profiling, except when we simulate SMP mode on a uniprocessor
 * system, in that case we have to call the local interrupt handler.
 */
#ifndef CONFIG_X86_LOCAL_APIC
	if (!user_mode(regs))
		x86_do_profile(regs->eip);
#else
	if (!using_apic_timer)
		smp_local_timer_interrupt(regs);
#endif

	/*
	 * If we have an externally synchronized Linux clock, then update
	 * CMOS clock accordingly every ~11 minutes. Set_rtc_mmss() has to be
	 * called as close as possible to 500 ms before the new second starts.
	 */
	if ((time_status & STA_UNSYNC) == 0 &&
	    xtime.tv_sec > last_rtc_update + 660 &&
	    xtime.tv_usec >= 500000 - ((unsigned) tick) / 2 &&
	    xtime.tv_usec <= 500000 + ((unsigned) tick) / 2) {
		if (set_rtc_mmss(xtime.tv_sec) == 0)
			last_rtc_update = xtime.tv_sec;
		else
			last_rtc_update = xtime.tv_sec - 600; /* do it again in 60 s */
	}
	    
#ifdef CONFIG_MCA
	if( MCA_bus ) {
		/* The PS/2 uses level-triggered interrupts.  You can't
		turn them off, nor would you want to (any attempt to
		enable edge-triggered interrupts usually gets intercepted by a
		special hardware circuit).  Hence we have to acknowledge
		the timer interrupt.  Through some incredibly stupid
		design idea, the reset for IRQ 0 is done by setting the
		high bit of the PPI port B (0x61).  Note that some PS/2s,
		notably the 55SX, work fine if this is removed.  */

		irq = inb_p( 0x61 );	/* read the current state */
		outb_p( irq|0x80, 0x61 );	/* reset the IRQ */
	}
#endif
}

static int use_tsc;

/*
 * This is the same as the above, except we _also_ save the current
 * Time Stamp Counter value at the time of the timer interrupt, so that
 * we later on can estimate the time of day more exactly.
 */
static void timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	int count;

	/*
	 * Here we are in the timer irq handler. We just have irqs locally
	 * disabled but we don't know if the timer_bh is running on the other
	 * CPU. We need to avoid to SMP race with it. NOTE: we don' t need
	 * the irq version of write_lock because as just said we have irq
	 * locally disabled. -arca
	 */
	write_lock(&xtime_lock);

	if(use_cyclone)
		mark_timeoffset_cyclone();
	else if (use_tsc) {
		/*
		 * It is important that these two operations happen almost at
		 * the same time. We do the RDTSC stuff first, since it's
		 * faster. To avoid any inconsistencies, we need interrupts
		 * disabled locally.
		 */

		/*
		 * Interrupts are just disabled locally since the timer irq
		 * has the SA_INTERRUPT flag set. -arca
		 */
	
		/* read Pentium cycle counter */

		rdtscl(last_tsc_low);

		spin_lock(&i8253_lock);
		outb_p(0x00, 0x43);     /* latch the count ASAP */

		count = inb_p(0x40);    /* read the latched count */
		count |= inb(0x40) << 8;

		/* Any unpaired read will cause the above to swap MSB/LSB
		   forever.  Try to detect this and reset the counter. 
		   
		   This happens very occasionally with buggy SMM bios
		   code at least */
		   
		if (count > LATCH) {
			printk(KERN_WARNING 
			       "i8253 count too high! resetting..\n");
			outb_p(0x34, 0x43);
			outb_p(LATCH & 0xff, 0x40);
			outb(LATCH >> 8, 0x40);
			count = LATCH - 1;
		}

		spin_unlock(&i8253_lock);

		/* Some i8253 clones hold the LATCH value visible
		   momentarily as they flip back to zero */
		if (count == LATCH) {
			count--;
		}

		count = ((LATCH-1) - count) * TICK_SIZE;
		delay_at_last_interrupt = (count + LATCH/2) / LATCH;
	}

	do_timer_interrupt(irq, NULL, regs);

	write_unlock(&xtime_lock);

}

/* not static: needed by APM */
unsigned long get_cmos_time(void)
{
	unsigned int year, mon, day, hour, min, sec;
	int i;

	spin_lock(&rtc_lock);
	/* The Linux interpretation of the CMOS clock register contents:
	 * When the Update-In-Progress (UIP) flag goes from 1 to 0, the
	 * RTC registers show the second which has precisely just started.
	 * Let's hope other operating systems interpret the RTC the same way.
	 */
	/* read RTC exactly on falling edge of update flag */
	for (i = 0 ; i < 1000000 ; i++)	/* may take up to 1 second... */
		if (CMOS_READ(RTC_FREQ_SELECT) & RTC_UIP)
			break;
	for (i = 0 ; i < 1000000 ; i++)	/* must try at least 2.228 ms */
		if (!(CMOS_READ(RTC_FREQ_SELECT) & RTC_UIP))
			break;
	do { /* Isn't this overkill ? UIP above should guarantee consistency */
		sec = CMOS_READ(RTC_SECONDS);
		min = CMOS_READ(RTC_MINUTES);
		hour = CMOS_READ(RTC_HOURS);
		day = CMOS_READ(RTC_DAY_OF_MONTH);
		mon = CMOS_READ(RTC_MONTH);
		year = CMOS_READ(RTC_YEAR);
	} while (sec != CMOS_READ(RTC_SECONDS));
	if (!(CMOS_READ(RTC_CONTROL) & RTC_DM_BINARY) || RTC_ALWAYS_BCD)
	  {
	    BCD_TO_BIN(sec);
	    BCD_TO_BIN(min);
	    BCD_TO_BIN(hour);
	    BCD_TO_BIN(day);
	    BCD_TO_BIN(mon);
	    BCD_TO_BIN(year);
	  }
	spin_unlock(&rtc_lock);
	if ((year += 1900) < 1970)
		year += 100;
	return mktime(year, mon, day, hour, min, sec);
}

static struct irqaction irq0  = { timer_interrupt, SA_INTERRUPT, 0, "timer", NULL, NULL};

/* ------ Calibrate the TSC ------- 
 * Return 2^32 * (1 / (TSC clocks per usec)) for do_fast_gettimeoffset().
 * Too much 64-bit arithmetic here to do this cleanly in C, and for
 * accuracy's sake we want to keep the overhead on the CTC speaker (channel 2)
 * output busy loop as low as possible. We avoid reading the CTC registers
 * directly because of the awkward 8-bit access mechanism of the 82C54
 * device.
 */

#define CALIBRATE_LATCH	(5 * LATCH)
#define CALIBRATE_TIME	(5 * 1000020/HZ)

static unsigned long __init calibrate_tsc(void)
{
       /* Set the Gate high, disable speaker */
	outb((inb(0x61) & ~0x02) | 0x01, 0x61);

	/*
	 * Now let's take care of CTC channel 2
	 *
	 * Set the Gate high, program CTC channel 2 for mode 0,
	 * (interrupt on terminal count mode), binary count,
	 * load 5 * LATCH count, (LSB and MSB) to begin countdown.
	 */
	outb(0xb0, 0x43);			/* binary, mode 0, LSB/MSB, Ch 2 */
	outb(CALIBRATE_LATCH & 0xff, 0x42);	/* LSB of count */
	outb(CALIBRATE_LATCH >> 8, 0x42);	/* MSB of count */

	{
		unsigned long startlow, starthigh;
		unsigned long endlow, endhigh;
		unsigned long count;

		rdtsc(startlow,starthigh);
		count = 0;
		do {
			count++;
		} while ((inb(0x61) & 0x20) == 0);
		rdtsc(endlow,endhigh);

		last_tsc_low = endlow;

		/* Error: ECTCNEVERSET */
		if (count <= 1)
			goto bad_ctc;

		/* 64-bit subtract - gcc just messes up with long longs */
		__asm__("subl %2,%0\n\t"
			"sbbl %3,%1"
			:"=a" (endlow), "=d" (endhigh)
			:"g" (startlow), "g" (starthigh),
			 "0" (endlow), "1" (endhigh));

		/* Error: ECPUTOOFAST */
		if (endhigh)
			goto bad_ctc;

		/* Error: ECPUTOOSLOW */
		if (endlow <= CALIBRATE_TIME)
			goto bad_ctc;

		__asm__("divl %2"
			:"=a" (endlow), "=d" (endhigh)
			:"r" (endlow), "0" (0), "1" (CALIBRATE_TIME));

		return endlow;
	}

	/*
	 * The CTC wasn't reliable: we got a hit on the very first read,
	 * or the CPU was so fast/slow that the quotient wouldn't fit in
	 * 32 bits..
	 */
bad_ctc:
	return 0;
}

void __init time_init(void)
{
	extern int x86_udelay_tsc;
	
	xtime.tv_sec = get_cmos_time();
	xtime.tv_usec = 0;

/*
 * If we have APM enabled or the CPU clock speed is variable
 * (CPU stops clock on HLT or slows clock to save power)
 * then the TSC timestamps may diverge by up to 1 jiffy from
 * 'real time' but nothing will break.
 * The most frequent case is that the CPU is "woken" from a halt
 * state by the timer interrupt itself, so we get 0 error. In the
 * rare cases where a driver would "wake" the CPU and request a
 * timestamp, the maximum error is < 1 jiffy. But timestamps are
 * still perfectly ordered.
 * Note that the TSC counter will be reset if APM suspends
 * to disk; this won't break the kernel, though, 'cuz we're
 * smart.  See arch/i386/kernel/apm.c.
 */
 	/*
 	 *	Firstly we have to do a CPU check for chips with
 	 * 	a potentially buggy TSC. At this point we haven't run
 	 *	the ident/bugs checks so we must run this hook as it
 	 *	may turn off the TSC flag.
 	 *
 	 *	NOTE: this doesnt yet handle SMP 486 machines where only
 	 *	some CPU's have a TSC. Thats never worked and nobody has
 	 *	moaned if you have the only one in the world - you fix it!
 	 */
 
 	dodgy_tsc();

 	if(use_cyclone)
		init_cyclone_clock();
		
	if (cpu_has_tsc) {
		unsigned long tsc_quotient = calibrate_tsc();
		if (tsc_quotient) {
			fast_gettimeoffset_quotient = tsc_quotient;
			/* XXX: This is messy
			 * However, we want to allow for the cyclone timer 
			 * to work w/ or w/o the TSCs being avaliable
			 *      -johnstul@us.ibm.com
			 */
			if(!use_cyclone){
				/*
				 *	We could be more selective here I suspect
				 *	and just enable this for the next intel chips ?
			 	 */
				use_tsc = 1;
				x86_udelay_tsc = 1;
#ifndef do_gettimeoffset
				do_gettimeoffset = do_fast_gettimeoffset;
#endif
			}
			/* report CPU clock rate in Hz.
			 * The formula is (10^6 * 2^32) / (2^32 * 1 / (clocks/us)) =
			 * clock/second. Our precision is about 100 ppm.
			 */
			{	unsigned long eax=0, edx=1000;
				__asm__("divl %2"
		       		:"=a" (cpu_khz), "=d" (edx)
        	       		:"r" (tsc_quotient),
	                	"0" (eax), "1" (edx));
				printk("Detected %lu.%03lu MHz processor.\n", cpu_khz / 1000, cpu_khz % 1000);
			}
		}
	}


#ifdef CONFIG_VISWS
	printk("Starting Cobalt Timer system clock\n");

	/* Set the countdown value */
	co_cpu_write(CO_CPU_TIMEVAL, CO_TIME_HZ/HZ);

	/* Start the timer */
	co_cpu_write(CO_CPU_CTRL, co_cpu_read(CO_CPU_CTRL) | CO_CTRL_TIMERUN);

	/* Enable (unmask) the timer interrupt */
	co_cpu_write(CO_CPU_CTRL, co_cpu_read(CO_CPU_CTRL) & ~CO_CTRL_TIMEMASK);

	/* Wire cpu IDT entry to s/w handler (and Cobalt APIC to IDT) */
	setup_irq(CO_IRQ_TIMER, &irq0);
#else
	setup_irq(0, &irq0);
#endif
}
