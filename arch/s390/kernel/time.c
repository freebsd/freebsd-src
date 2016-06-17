/*
 *  arch/s390/kernel/time.c
 *
 *  S390 version
 *    Copyright (C) 1999 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Author(s): Hartmut Penner (hp@de.ibm.com),
 *               Martin Schwidefsky (schwidefsky@de.ibm.com),
 *               Denis Joseph Barrow (djbarrow@de.ibm.com,barrow_dj@yahoo.com)
 *
 *  Derived from "arch/i386/kernel/time.c"
 *    Copyright (C) 1991, 1992, 1995  Linus Torvalds
 */

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
#include <linux/types.h>

#include <asm/uaccess.h>
#include <asm/delay.h>
#include <asm/s390_ext.h>

#include <linux/timex.h>
#include <linux/config.h>

#include <asm/irq.h>

/* change this if you have some constant time drift */
#define USECS_PER_JIFFY     ((unsigned long) 1000000/HZ)
#define CLK_TICKS_PER_JIFFY ((unsigned long) USECS_PER_JIFFY << 12)

#define TICK_SIZE tick

static ext_int_info_t ext_int_info_timer;
static uint64_t init_timer_cc;

extern rwlock_t xtime_lock;
extern unsigned long wall_jiffies;

void tod_to_timeval(__u64 todval, struct timeval *xtime)
{
        const int high_bit = 0x80000000L;
        const int c_f4240 = 0xf4240L;
        const int c_7a120 = 0x7a120;
	/* We have to divide the 64 bit value todval by 4096
	 * (because the 2^12 bit is the one that changes every 
         * microsecond) and then split it into seconds and
         * microseconds. A value of max (2^52-1) divided by
         * the value 0xF4240 can yield a max result of approx
         * (2^32.068). Thats to big to fit into a signed int
	 *   ... hacking time!
         */
	asm volatile ("L     2,%1\n\t"
		      "LR    3,2\n\t"
		      "SRL   2,12\n\t"
		      "SLL   3,20\n\t"
		      "L     4,%O1+4(%R1)\n\t"
		      "SRL   4,12\n\t"
		      "OR    3,4\n\t"  /* now R2/R3 contain (todval >> 12) */
		      "SR    4,4\n\t"
		      "CL    2,%2\n\t"
		      "JL    .+12\n\t"
		      "S     2,%2\n\t"
		      "L     4,%3\n\t"
                      "D     2,%4\n\t"
		      "OR    3,4\n\t"
		      "ST    2,%O0+4(%R0)\n\t"
		      "ST    3,%0"
		      : "=m" (*xtime) : "m" (todval),
		        "m" (c_7a120), "m" (high_bit), "m" (c_f4240)
		      : "cc", "memory", "2", "3", "4" );
}

static inline unsigned long do_gettimeoffset(void) 
{
	__u64 now;

	asm ("STCK 0(%0)" : : "a" (&now) : "memory", "cc");
        now = (now - init_timer_cc) >> 12;
	/* We require the offset from the latest update of xtime */
	now -= (__u64) wall_jiffies*USECS_PER_JIFFY;
	return (unsigned long) now;
}

/*
 * This version of gettimeofday has microsecond resolution.
 */
void do_gettimeofday(struct timeval *tv)
{
	unsigned long flags;
	unsigned long usec, sec;

	read_lock_irqsave(&xtime_lock, flags);
	sec = xtime.tv_sec;
	usec = xtime.tv_usec + do_gettimeoffset();
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
	/* This is revolting. We need to set the xtime.tv_usec
	 * correctly. However, the value in this location is
	 * is value at the last tick.
	 * Discover what correction gettimeofday
	 * would have done, and then undo it!
	 */
	tv->tv_usec -= do_gettimeoffset();

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
 * timer_interrupt() needs to keep up the real-time clock,
 * as well as call the "do_timer()" routine every clocktick
 */

#ifdef CONFIG_SMP
extern __u16 boot_cpu_addr;
#endif

static void do_comparator_interrupt(struct pt_regs *regs, __u16 error_code)
{
	int cpu = smp_processor_id();

	irq_enter(cpu, 0);

	/*
	 * set clock comparator for next tick
	 */
        S390_lowcore.jiffy_timer += CLK_TICKS_PER_JIFFY;
        asm volatile ("SCKC %0" : : "m" (S390_lowcore.jiffy_timer));

#ifdef CONFIG_SMP
	if (S390_lowcore.cpu_data.cpu_addr == boot_cpu_addr)
		write_lock(&xtime_lock);

	update_process_times(user_mode(regs));

	if (S390_lowcore.cpu_data.cpu_addr == boot_cpu_addr) {
		do_timer(regs);
		write_unlock(&xtime_lock);
	}
#else
	do_timer(regs);
#endif

	irq_exit(cpu, 0);
}

/*
 * Start the clock comparator on the current CPU
 */
void init_cpu_timer(void)
{
	unsigned long cr0;

	S390_lowcore.jiffy_timer = (__u64) jiffies * CLK_TICKS_PER_JIFFY;
	S390_lowcore.jiffy_timer += init_timer_cc + CLK_TICKS_PER_JIFFY;
	asm volatile ("SCKC %0" : : "m" (S390_lowcore.jiffy_timer));
        /* allow clock comparator timer interrupt */
        asm volatile ("STCTL 0,0,%0" : "=m" (cr0) : : "memory");
        cr0 |= 0x800;
        asm volatile ("LCTL 0,0,%0" : : "m" (cr0) : "memory");
}

/*
 * Initialize the TOD clock and the CPU timer of
 * the boot cpu.
 */
void __init time_init(void)
{
        __u64 set_time_cc;
	int cc;

        /* kick the TOD clock */
        asm volatile ("STCK 0(%1)\n\t"
                      "IPM  %0\n\t"
                      "SRL  %0,28" : "=r" (cc) : "a" (&init_timer_cc) 
				   : "memory", "cc");
        switch (cc) {
        case 0: /* clock in set state: all is fine */
                break;
        case 1: /* clock in non-set state: FIXME */
                printk("time_init: TOD clock in non-set state\n");
                break;
        case 2: /* clock in error state: FIXME */
                printk("time_init: TOD clock in error state\n");
                break;
        case 3: /* clock in stopped or not-operational state: FIXME */
                printk("time_init: TOD clock stopped/non-operational\n");
                break;
        }

	/* set xtime */
        set_time_cc = init_timer_cc - 0x8126d60e46000000LL +
                      (0x3c26700LL*1000000*4096);
        tod_to_timeval(set_time_cc, &xtime);

        /* request the 0x1004 external interrupt */
        if (register_early_external_interrupt(0x1004, do_comparator_interrupt,
					      &ext_int_info_timer) != 0)
                panic("Couldn't request external interrupt 0x1004");

        /* init CPU timer */
        init_cpu_timer();
}
