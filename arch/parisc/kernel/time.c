/*
 *  linux/arch/parisc/kernel/time.c
 *
 *  Copyright (C) 1991, 1992, 1995  Linus Torvalds
 *  Modifications for ARM (C) 1994, 1995, 1996,1997 Russell King
 *  Copyright (C) 1999 SuSE GmbH, (Philipp Rumpf, prumpf@tux.org)
 *
 * 1994-07-02  Alan Modra
 *             fixed set_rtc_mmss, fixed time.year for >= 2000, new mktime
 * 1998-12-20  Updated NTP code according to technical memorandum Jan '96
 *             "A Kernel Model for Precision Timekeeping" by Dave Mills
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
#include <linux/init.h>
#include <linux/smp.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/param.h>
#include <asm/pdc.h>
#include <asm/led.h>

#include <linux/timex.h>

/* xtime and wall_jiffies keep wall-clock time */
extern unsigned long wall_jiffies;
extern rwlock_t xtime_lock;

static long clocktick;	/* timer cycles per tick */
static long halftick;

#ifdef CONFIG_SMP
extern void smp_do_timer(struct pt_regs *regs);
#endif

static inline void
parisc_do_profile(unsigned long pc)
{
	extern char _stext;

	if (!prof_buffer)
		return;

	pc -= (unsigned long) &_stext;
	pc >>= prof_shift;
	/*
	 * Don't ignore out-of-bounds PC values silently,
	 * put them into the last histogram slot, so if
	 * present, they will show up as a sharp peak.
	 */
	if (pc > prof_len - 1)
		pc = prof_len - 1;
	atomic_inc((atomic_t *)&prof_buffer[pc]);
}

void timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	long now = mfctl(16);
	long next_tick;
	int nticks;
	int cpu = smp_processor_id();

	/* initialize next_tick to time at last clocktick */

	next_tick = cpu_data[cpu].it_value;

	/* since time passes between the interrupt and the mfctl()
	 * above, it is never true that last_tick + clocktick == now.  If we
	 * never miss a clocktick, we could set next_tick = last_tick + clocktick
	 * but maybe we'll miss ticks, hence the loop.
	 *
	 * Variables are *signed*.
	 */

	nticks = 0;
	while((next_tick - now) < halftick) {
		next_tick += clocktick;
		nticks++;
	}
	mtctl(next_tick, 16);
	cpu_data[cpu].it_value = next_tick;

	while (nticks--) {
#ifdef CONFIG_SMP
		smp_do_timer(regs);
#endif
		if (cpu == 0) {
			extern int pc_in_user_space;
			write_lock(&xtime_lock);
#ifndef CONFIG_SMP
			if (!user_mode(regs))
				parisc_do_profile(regs->iaoq[0]);
			else
				parisc_do_profile(&pc_in_user_space);
#endif
			do_timer(regs);
			write_unlock(&xtime_lock);
		}
	}
    
#ifdef CONFIG_CHASSIS_LCD_LED
	/* Only schedule the led tasklet on cpu 0, and only if it
	 * is enabled.
	 */
	if (cpu == 0 && !atomic_read(&led_tasklet.count))
		tasklet_schedule(&led_tasklet);
#endif

	/* check soft power switch status */
	if (cpu == 0 && !atomic_read(&power_tasklet.count))
		tasklet_schedule(&power_tasklet);
}

/*** converted from ia64 ***/
/*
 * Return the number of micro-seconds that elapsed since the last
 * update to wall time (aka xtime aka wall_jiffies).  The xtime_lock
 * must be at least read-locked when calling this routine.
 */
static inline unsigned long
gettimeoffset (void)
{
#ifndef CONFIG_SMP
	/*
	 * FIXME: This won't work on smp because jiffies are updated by cpu 0.
	 *    Once parisc-linux learns the cr16 difference between processors,
	 *    this could be made to work.
	 */
	long last_tick;
	long elapsed_cycles;

	/* it_value is the intended time of the next tick */
	last_tick = cpu_data[smp_processor_id()].it_value;

	/* Subtract one tick and account for possible difference between
	 * when we expected the tick and when it actually arrived.
	 * (aka wall vs real)
	 */
	last_tick -= clocktick * (jiffies - wall_jiffies + 1);
	elapsed_cycles = mfctl(16) - last_tick;

	/* the precision of this math could be improved */
	return elapsed_cycles / (PAGE0->mem_10msec / 10000);
#else
	return 0;
#endif
}

void
do_gettimeofday (struct timeval *tv)
{
	unsigned long flags, usec, sec;

	read_lock_irqsave(&xtime_lock, flags);
	{
		usec = gettimeoffset();
	
		sec = xtime.tv_sec;
		usec += xtime.tv_usec;
	}
	read_unlock_irqrestore(&xtime_lock, flags);

	while (usec >= 1000000) {
		usec -= 1000000;
		++sec;
	}

	tv->tv_sec = sec;
	tv->tv_usec = usec;
}

void
do_settimeofday (struct timeval *tv)
{
	write_lock_irq(&xtime_lock);
	{
		/*
		 * This is revolting. We need to set "xtime"
		 * correctly. However, the value in this location is
		 * the value at the most recent update of wall time.
		 * Discover what correction gettimeofday would have
		 * done, and then undo it!
		 */
		tv->tv_usec -= gettimeoffset();
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
	}
	write_unlock_irq(&xtime_lock);
}


void __init time_init(void)
{
	unsigned long next_tick;
	static struct pdc_tod tod_data;

	clocktick = (100 * PAGE0->mem_10msec) / HZ;
	halftick = clocktick / 2;

	/* Setup clock interrupt timing */

	next_tick = mfctl(16);
	next_tick += clocktick;
	cpu_data[smp_processor_id()].it_value = next_tick;

	/* kick off Itimer (CR16) */
	mtctl(next_tick, 16);

	if(pdc_tod_read(&tod_data) == 0) {
		write_lock_irq(&xtime_lock);
		xtime.tv_sec = tod_data.tod_sec;
		xtime.tv_usec = tod_data.tod_usec;
		write_unlock_irq(&xtime_lock);
	} else {
		printk(KERN_ERR "Error reading tod clock\n");
	        xtime.tv_sec = 0;
		xtime.tv_usec = 0;
	}
}
