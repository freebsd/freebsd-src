/* $Id: time.c,v 1.1.1.1.2.6 2003/07/16 18:43:55 yoshii Exp $
 *
 *  linux/arch/sh/kernel/time.c
 *
 *  Copyright (C) 1999  Tetsuya Okada & Niibe Yutaka
 *  Copyright (C) 2000  Philipp Rumpf <prumpf@tux.org>
 *  Copyright (C) 2003 Takashi Kusuda <kusuda-takashi@hitachi-ul.co.jp>
 *
 *  Some code taken from i386 version.
 *    Copyright (C) 1991, 1992, 1995  Linus Torvalds
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

#include <asm/processor.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/delay.h>
#include <asm/machvec.h>
#include <asm/rtc.h>
#ifdef CONFIG_SH_KGDB
#include <asm/kgdb.h>
#endif

#include <linux/timex.h>
#include <linux/irq.h>

#define TMU_TOCR_INIT	0x00	/* Don't output RTC clock */

#define TMU0_TCR_INIT	0x0020	/* Clock/4, rising edge; interrupt on */
#define TMU0_TCR_CALIB	0x0000	/* Clock/4, rising edge; no interrupt */
#define TMU0_TSTR_INIT	0x01	/* Bit to turn on TMU0 */

#define TMU1_TCR_INIT	0x0000	/* Clock/4, rising edge; no interrupt */
#define TMU1_TSTR_INIT  0x02	/* Bit to turn on TMU1 */

#if defined(__sh3__)
#if defined(CONFIG_CPU_SUBTYPE_SH7300)
#define TMU_TSTR        0xA412FE92      /* Byte access */

#define TMU0_TCOR       0xA412FE94      /* Long access */
#define TMU0_TCNT       0xA412FE98      /* Long access */
#define TMU0_TCR        0xA412FE9C      /* Word access */

#define TMU1_TCOR	0xA412FEA0	/* Long access */
#define TMU1_TCNT	0xA412FEA4	/* Long access */
#define TMU1_TCR	0xA412FEA8	/* Word access */

#define FRQCR           0xA415FF80
#else
#define TMU_TOCR	0xfffffe90	/* Byte access */
#define TMU_TSTR	0xfffffe92	/* Byte access */

#define TMU0_TCOR	0xfffffe94	/* Long access */
#define TMU0_TCNT	0xfffffe98	/* Long access */
#define TMU0_TCR	0xfffffe9c	/* Word access */

#define TMU1_TCOR	0xfffffea0	/* Long access */
#define TMU1_TCNT	0xfffffea4	/* Long access */
#define TMU1_TCR	0xfffffea8	/* Word access */

#define FRQCR		0xffffff80
#endif
#elif defined(__SH4__)
#define TMU_TOCR	0xffd80000	/* Byte access */
#define TMU_TSTR	0xffd80004	/* Byte access */

#define TMU0_TCOR	0xffd80008	/* Long access */
#define TMU0_TCNT	0xffd8000c	/* Long access */
#define TMU0_TCR	0xffd80010	/* Word access */

#define TMU1_TCOR	0xffd80014	/* Long access */
#define TMU1_TCNT	0xffd80018	/* Long access */
#define TMU1_TCR	0xffd8001c	/* Word access */

#define FRQCR		0xffc00000

/* Core Processor Version Register */
#define CCN_PVR		0xff000030
#define CCN_PVR_CHIP_SHIFT 24
#define CCN_PVR_CHIP_MASK  0xff
#define CCN_PVR_CHIP_ST40STB1 0x4

#ifdef CONFIG_CPU_SUBTYPE_ST40
#define CLOCKGEN_MEMCLKCR 0xbb040038
#define MEMCLKCR_RATIO_MASK 0x7
#endif /* CONFIG_CPU_SUBTYPE_ST40 */
#endif /* __sh3__ or __SH4__ */

extern rwlock_t xtime_lock;
extern unsigned long wall_jiffies;
#define TICK_SIZE tick

static unsigned long do_gettimeoffset(void)
{
	int count;

	static int count_p = 0x7fffffff;    /* for the first call after boot */
	static unsigned long jiffies_p = 0;

	/*
	 * cache volatile jiffies temporarily; we have IRQs turned off. 
	 */
	unsigned long jiffies_t;

	/* timer count may underflow right here */
	count = ctrl_inl(TMU0_TCNT);	/* read the latched count */

 	jiffies_t = jiffies;

	/*
	 * avoiding timer inconsistencies (they are rare, but they happen)...
	 * there is one kind of problem that must be avoided here:
	 *  1. the timer counter underflows
	 */

	if( jiffies_t == jiffies_p ) {
		if( count > count_p ) {
			/* the nutcase */

			if(ctrl_inw(TMU0_TCR) & 0x100) { /* Check UNF bit */
				/*
				 * We cannot detect lost timer interrupts ... 
				 * well, that's why we call them lost, don't we? :)
				 * [hmm, on the Pentium and Alpha we can ... sort of]
				 */
				count -= LATCH;
			} else {
				printk("do_slow_gettimeoffset(): hardware timer problem?\n");
			}
		}
	} else
		jiffies_p = jiffies_t;

	count_p = count;

	count = ((LATCH-1) - count) * TICK_SIZE;
	count = (count + LATCH/2) / LATCH;

	return count;
}

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

/* last time the RTC clock got updated */
static long last_rtc_update;

static __inline__ void sh_do_profile (unsigned long pc)
{
	extern int _stext;

	if (!prof_buffer)
		return;

	if(pc >= 0xa0000000UL && pc < 0xc0000000UL)
		pc -= 0x20000000;
	pc -= (unsigned long) &_stext;
	pc >>= prof_shift;
	/*
	 * Don't ignore out-of-bounds PC values silently,
	 * put them into the last histogram slot, so if
	 * present, they will show up as a sharp peak.
	 */
	if (pc > prof_len-1)
		pc = prof_len-1;
	prof_buffer[pc]++;
}

/*
 * timer_interrupt() needs to keep up the real-time clock,
 * as well as call the "do_timer()" routine every clocktick
 */
static inline void do_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	do_timer(regs);

	if (!user_mode(regs))
		sh_do_profile(regs->pc);

#ifdef CONFIG_HEARTBEAT
	if (sh_mv.mv_heartbeat != NULL) 
		sh_mv.mv_heartbeat();
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
		if (sh_mv.mv_rtc_settimeofday(&xtime) == 0)
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

static unsigned int __init get_timer_frequency(void)
{
	u32 freq;
	struct timeval tv1, tv2;
	unsigned long diff_usec;
	unsigned long factor;

	/* Setup the timer:  We don't want to generate interrupts, just
	 * have it count down at its natural rate.
	 */
	ctrl_outb(0, TMU_TSTR);
#if !defined(CONFIG_CPU_SUBTYPE_SH7300)
	ctrl_outb(TMU_TOCR_INIT, TMU_TOCR);
#endif
	ctrl_outw(TMU0_TCR_CALIB, TMU0_TCR);
	ctrl_outl(0xffffffff, TMU0_TCOR);
	ctrl_outl(0xffffffff, TMU0_TCNT);

	rtc_gettimeofday(&tv2);

	do {
		rtc_gettimeofday(&tv1);
	} while (tv1.tv_usec == tv2.tv_usec && tv1.tv_sec == tv2.tv_sec);

	/* actually start the timer */
	ctrl_outb(TMU0_TSTR_INIT, TMU_TSTR);

	do {
		rtc_gettimeofday(&tv2);
	} while (tv1.tv_usec == tv2.tv_usec && tv1.tv_sec == tv2.tv_sec);

	freq = 0xffffffff - ctrl_inl(TMU0_TCNT);
	if (tv2.tv_usec < tv1.tv_usec) {
		tv2.tv_usec += 1000000;
		tv2.tv_sec--;
	}

	diff_usec = (tv2.tv_sec - tv1.tv_sec) * 1000000 + (tv2.tv_usec - tv1.tv_usec);

	/* this should work well if the RTC has a precision of n Hz, where
	 * n is an integer.  I don't think we have to worry about the other
	 * cases. */
	factor = (1000000 + diff_usec/2) / diff_usec;

	if (factor * diff_usec > 1100000 ||
	    factor * diff_usec <  900000)
		panic("weird RTC (diff_usec %ld)", diff_usec);

	return freq * factor;
}

static unsigned int sh_pclk_freq __initdata = CONFIG_SH_PCLK_FREQ;
static int __init sh_pclk_setup(char *str)
{
	unsigned int freq;
	if (get_option(&str, &freq))
		sh_pclk_freq = freq;
	return 1;
}
__setup("sh_pclk=", sh_pclk_setup);

static struct irqaction irq0  = { timer_interrupt, SA_INTERRUPT, 0, "timer", NULL, NULL};

void __init time_init(void)
{
	unsigned int cpu_clock, master_clock, bus_clock, module_clock;
#ifdef CONFIG_CPU_SUBTYPE_ST40
	unsigned int memory_clock;
#endif
	unsigned int timer_freq;
	unsigned short frqcr, ifc, pfc, bfc;
	unsigned long interval;
#if defined(__sh3__)
#if defined(CONFIG_CPU_SUBTYPE_SH7300)
	static int pfc_table[] = { 1, 2, 3, 4, 6 };
#else
	static int ifc_table[] = { 1, 2, 4, 1, 3, 1, 1, 1 };
	static int pfc_table[] = { 1, 2, 4, 1, 3, 6, 1, 1 };
	static int stc_table[] = { 1, 2, 4, 8, 3, 6, 1, 1 };
#endif
#elif defined(__SH4__)
	static int ifc_table[] = { 1, 2, 3, 4, 6, 8, 1, 1 };
#define bfc_table ifc_table	/* Same */
	static int pfc_table[] = { 2, 3, 4, 6, 8, 2, 2, 2 };

#ifdef CONFIG_CPU_SUBTYPE_ST40
	struct frqcr_data {
		unsigned short frqcr;
		struct {
			unsigned char multiplier;
			unsigned char divisor;
		} factor[3];
	};

	static struct frqcr_data st40_frqcr_table[] = {		
		{ 0x000, {{1,1}, {1,1}, {1,2}}},
		{ 0x002, {{1,1}, {1,1}, {1,4}}},
		{ 0x004, {{1,1}, {1,1}, {1,8}}},
		{ 0x008, {{1,1}, {1,2}, {1,2}}},
		{ 0x00A, {{1,1}, {1,2}, {1,4}}},
		{ 0x00C, {{1,1}, {1,2}, {1,8}}},
		{ 0x011, {{1,1}, {2,3}, {1,6}}},
		{ 0x013, {{1,1}, {2,3}, {1,3}}},
		{ 0x01A, {{1,1}, {1,2}, {1,4}}},
		{ 0x01C, {{1,1}, {1,2}, {1,8}}},
		{ 0x023, {{1,1}, {2,3}, {1,3}}},
		{ 0x02C, {{1,1}, {1,2}, {1,8}}},
		{ 0x048, {{1,2}, {1,2}, {1,4}}},
		{ 0x04A, {{1,2}, {1,2}, {1,6}}},
		{ 0x04C, {{1,2}, {1,2}, {1,8}}},
		{ 0x05A, {{1,2}, {1,3}, {1,6}}},
		{ 0x05C, {{1,2}, {1,3}, {1,6}}},
		{ 0x063, {{1,2}, {1,4}, {1,4}}},
		{ 0x06C, {{1,2}, {1,4}, {1,8}}},
		{ 0x091, {{1,3}, {1,3}, {1,6}}},
		{ 0x093, {{1,3}, {1,3}, {1,6}}},
		{ 0x0A3, {{1,3}, {1,6}, {1,6}}},
		{ 0x0DA, {{1,4}, {1,4}, {1,8}}},
		{ 0x0DC, {{1,4}, {1,4}, {1,8}}},
		{ 0x0EC, {{1,4}, {1,8}, {1,8}}},
		{ 0x123, {{1,4}, {1,4}, {1,8}}},
		{ 0x16C, {{1,4}, {1,8}, {1,8}}},
	};

	struct memclk_data {
		unsigned char multiplier;
		unsigned char divisor;
	};
	static struct memclk_data st40_memclk_table[8] = {
		{1,1},	// 000
		{1,2},	// 001
		{1,3},	// 010
		{2,3},	// 011
		{1,4},	// 100
		{1,6},	// 101
		{1,8},	// 110
		{1,8}	// 111
	};
#endif
#endif

	if(rtc_gettimeofday)
		rtc_gettimeofday(&xtime);
	else{
        	xtime.tv_sec = mktime(2000, 1, 1, 0, 0, 0);
        	xtime.tv_usec = 0;
	}

	setup_irq(TIMER_IRQ, &irq0);

	if( sh_pclk_freq ){
		module_clock = sh_pclk_freq;
	}else{
		timer_freq = get_timer_frequency();
		module_clock = timer_freq * 4;
	}

#if defined(__sh3__)
	{
		unsigned short tmp;

		frqcr = ctrl_inw(FRQCR);
#if defined(CONFIG_CPU_SUBTYPE_SH7300)
                bfc = ((frqcr & 0x0700) >> 8)+1;
                ifc = ((frqcr & 0x0070) >> 4)+1;
                tmp = frqcr & 0x0007;
                pfc = pfc_table[tmp];
#else
		tmp  = (frqcr & 0x8000) >> 13;
		tmp |= (frqcr & 0x0030) >>  4;
		bfc = stc_table[tmp];
		tmp  = (frqcr & 0x4000) >> 12;
		tmp |= (frqcr & 0x000c) >> 2;
		ifc  = ifc_table[tmp];
		tmp  = (frqcr & 0x2000) >> 11;
		tmp |= frqcr & 0x0003;
		pfc = pfc_table[tmp];
#endif
	}
#elif defined(__SH4__)
	{
#ifdef CONFIG_CPU_SUBTYPE_ST40
		unsigned long pvr;

		/* This should probably be moved into the SH3 probing code, and then use the processor
		 * structure to determine which CPU we are running on.
		 */
		pvr = ctrl_inl(CCN_PVR);
		printk("PVR %08x\n", pvr);

		if (((pvr >>CCN_PVR_CHIP_SHIFT) & CCN_PVR_CHIP_MASK) == CCN_PVR_CHIP_ST40STB1) {
			/* Unfortunatly the STB1 FRQCR values are different from the 7750 ones */
			struct frqcr_data *d;
			int a;
			unsigned long memclkcr;
			struct memclk_data *e;

			for (a=0; a<ARRAY_SIZE(st40_frqcr_table); a++) {
				d = &st40_frqcr_table[a];
				if (d->frqcr == (frqcr & 0x1ff))
					break;
			}
			if (a == ARRAY_SIZE(st40_frqcr_table)) {
				d = st40_frqcr_table;
				printk("ERROR: Unrecognised FRQCR value, using default multipliers\n");
			}

			memclkcr = ctrl_inl(CLOCKGEN_MEMCLKCR);
			e = &st40_memclk_table[memclkcr & MEMCLKCR_RATIO_MASK];

			printk("Clock multipliers: CPU: %d/%d Bus: %d/%d Mem: %d/%d Periph: %d/%d\n",
			       d->factor[0].multiplier, d->factor[0].divisor,
			       d->factor[1].multiplier, d->factor[1].divisor,
			       e->multiplier,           e->divisor,
			       d->factor[2].multiplier, d->factor[2].divisor);
			
			master_clock = module_clock * d->factor[2].divisor    / d->factor[2].multiplier;
			bus_clock    = master_clock * d->factor[1].multiplier / d->factor[1].divisor;
			memory_clock = master_clock * e->multiplier           / e->divisor;
			cpu_clock    = master_clock * d->factor[0].multiplier / d->factor[0].divisor;
			goto skip_calc;
		} else
#endif
		{
			frqcr = ctrl_inw(FRQCR);

			ifc  = ifc_table[(frqcr>> 6) & 0x0007];
			bfc  = bfc_table[(frqcr>> 3) & 0x0007];
			pfc = pfc_table[frqcr & 0x0007];
		}
	}
#endif
	master_clock = module_clock * pfc;
	bus_clock = master_clock / bfc;
	cpu_clock = master_clock / ifc;
#ifdef CONFIG_CPU_SUBTYPE_ST40
 skip_calc:
#endif
	printk("CPU clock: %d.%02dMHz\n",
	       (cpu_clock / 1000000), (cpu_clock % 1000000)/10000);
	printk("Bus clock: %d.%02dMHz\n",
	       (bus_clock/1000000), (bus_clock % 1000000)/10000);
#ifdef CONFIG_CPU_SUBTYPE_ST40
	printk("Memory clock: %d.%02dMHz\n",
	       (memory_clock/1000000), (memory_clock % 1000000)/10000);
#endif
	printk("Module clock: %d.%02dMHz\n",
	       (module_clock/1000000), (module_clock % 1000000)/10000);
	interval = (module_clock/4 + HZ/2) / HZ;

	printk("Interval = %ld\n", interval);

	current_cpu_data.cpu_clock    = cpu_clock;
	current_cpu_data.master_clock = master_clock;
	current_cpu_data.bus_clock    = bus_clock;
#ifdef CONFIG_CPU_SUBTYPE_ST40
	current_cpu_data.memory_clock = memory_clock;
#endif
	current_cpu_data.module_clock = module_clock;

	/* Stop all timers */
	ctrl_outb(0, TMU_TSTR);
#if !defined(CONFIG_CPU_SUBTYPE_SH7300)
	ctrl_outb(TMU_TOCR_INIT, TMU_TOCR);
#endif

	/* Start TMU0 (jiffy interrupts) */
	ctrl_outw(TMU0_TCR_INIT, TMU0_TCR);
	ctrl_outl(interval, TMU0_TCOR);
	ctrl_outl(interval, TMU0_TCNT);
	ctrl_outb(TMU0_TSTR_INIT, TMU_TSTR);

#if defined(CONFIG_START_TMU1)
	/* Start TMU1 (free-running) */
	ctrl_outw(TMU1_TCR_INIT, TMU1_TCR);
	ctrl_outl(0xffffffff, TMU1_TCOR);
	ctrl_outl(0xffffffff, TMU1_TCNT);
	ctrl_outb((ctrl_inb(TMU_TSTR) | TMU1_TSTR_INIT), TMU_TSTR);
#endif

#if defined(CONFIG_SH_KGDB)
	/*
	 * Set up kgdb as requested. We do it here because the serial
	 * init uses the timer vars we just set up for figuring baud.
	 */
        kgdb_init();
#endif

}
