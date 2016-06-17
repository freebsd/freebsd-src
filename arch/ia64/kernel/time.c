/*
 * linux/arch/ia64/kernel/time.c
 *
 * Copyright (C) 1998-2001 Hewlett-Packard Co
 * Copyright (C) 1998-2000 Stephane Eranian <eranian@hpl.hp.com>
 * Copyright (C) 1999-2001 David Mosberger <davidm@hpl.hp.com>
 * Copyright (C) 1999 Don Dugger <don.dugger@intel.com>
 * Copyright (C) 1999-2000 VA Linux Systems
 * Copyright (C) 1999-2000 Walt Drummond <drummond@valinux.com>
 */
#include <linux/config.h>

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/interrupt.h>
#include <linux/efi.h>

#include <asm/delay.h>
#include <asm/hw_irq.h>
#include <asm/ptrace.h>
#include <asm/sal.h>
#include <asm/system.h>

extern rwlock_t xtime_lock;
extern unsigned long wall_jiffies;
extern unsigned long last_time_offset;

#ifdef CONFIG_IA64_DEBUG_IRQ

unsigned long last_cli_ip;

#endif

static void
do_profile (unsigned long ip)
{
	extern unsigned long prof_cpu_mask;
	extern char _stext;

	if (!prof_buffer)
		return;

	if (!((1UL << smp_processor_id()) & prof_cpu_mask))
		return;

	ip -= (unsigned long) &_stext;
	ip >>= prof_shift;
	/*
	 * Don't ignore out-of-bounds IP values silently, put them into the last
	 * histogram slot, so if present, they will show up as a sharp peak.
	 */
	if (ip > prof_len - 1)
		ip = prof_len - 1;

	atomic_inc((atomic_t *) &prof_buffer[ip]);
}

/*
 * Return the number of micro-seconds that elapsed since the last update to jiffy.  The
 * xtime_lock must be at least read-locked when calling this routine.
 */
static inline unsigned long
gettimeoffset (void)
{
	unsigned long elapsed_cycles, lost = jiffies - wall_jiffies;
	unsigned long now, last_tick;
#	define time_keeper_id	0	/* smp_processor_id() of time-keeper */

	last_tick = (cpu_data(time_keeper_id)->itm_next
		     - (lost + 1)*cpu_data(time_keeper_id)->itm_delta);

	now = ia64_get_itc();
	if ((long) (now - last_tick) < 0) {
		printk(KERN_ERR "CPU %d: now < last_tick (now=0x%lx,last_tick=0x%lx)!\n",
		       smp_processor_id(), now, last_tick);
		return last_time_offset;
	}
	elapsed_cycles = now - last_tick;
	return (elapsed_cycles*local_cpu_data->usec_per_cyc) >> IA64_USEC_PER_CYC_SHIFT;
}

void
do_settimeofday (struct timeval *tv)
{
	write_lock_irq(&xtime_lock);
	{
		/*
		 * This is revolting. We need to set "xtime" correctly. However, the value
		 * in this location is the value at the most recent update of wall time.
		 * Discover what correction gettimeofday would have done, and then undo
		 * it!
		 */
		tv->tv_usec -= gettimeoffset();

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

void
do_gettimeofday (struct timeval *tv)
{
	unsigned long flags, usec, sec, old;

	read_lock_irqsave(&xtime_lock, flags);
	{
		usec = gettimeoffset();

		/*
		 * Ensure time never goes backwards, even when ITC on different CPUs are
		 * not perfectly synchronized.
		 */
		do {
			old = last_time_offset;
			if (usec <= old) {
				usec = old;
				break;
			}
		} while (cmpxchg(&last_time_offset, old, usec) != old);

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

static void
timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned long new_itm;

	new_itm = local_cpu_data->itm_next;

	if (!time_after(ia64_get_itc(), new_itm))
		printk(KERN_ERR "Oops: timer tick before it's due (itc=%lx,itm=%lx)\n",
		       ia64_get_itc(), new_itm);

	while (1) {
		/*
		 * Do kernel PC profiling here.  We multiply the instruction number by
		 * four so that we can use a prof_shift of 2 to get instruction-level
		 * instead of just bundle-level accuracy.
		 */
		if (!user_mode(regs))
			do_profile(regs->cr_iip + 4*ia64_psr(regs)->ri);

#ifdef CONFIG_SMP
		smp_do_timer(regs);
#endif
		new_itm += local_cpu_data->itm_delta;

		if (smp_processor_id() == 0) {
			/*
			 * Here we are in the timer irq handler. We have irqs locally
			 * disabled, but we don't know if the timer_bh is running on
			 * another CPU. We need to avoid to SMP race by acquiring the
			 * xtime_lock.
			 */
			write_lock(&xtime_lock);
			do_timer(regs);
			local_cpu_data->itm_next = new_itm;
			write_unlock(&xtime_lock);
		} else
			local_cpu_data->itm_next = new_itm;

		if (time_after(new_itm, ia64_get_itc()))
			break;
	}

	do {
	    /*
	     * If we're too close to the next clock tick for comfort, we increase the
	     * saftey margin by intentionally dropping the next tick(s).  We do NOT update
	     * itm.next because that would force us to call do_timer() which in turn would
	     * let our clock run too fast (with the potentially devastating effect of
	     * losing monotony of time).
	     */
	    while (!time_after(new_itm, ia64_get_itc() + local_cpu_data->itm_delta/2))
	      new_itm += local_cpu_data->itm_delta;
	    ia64_set_itm(new_itm);
	    /* double check, in case we got hit by a (slow) PMI: */
	} while (time_after_eq(ia64_get_itc(), new_itm));
}

/*
 * Encapsulate access to the itm structure for SMP.
 */
void __init
ia64_cpu_local_tick (void)
{
	int cpu = smp_processor_id();
	unsigned long shift = 0, delta;

	/* arrange for the cycle counter to generate a timer interrupt: */
	ia64_set_itv(IA64_TIMER_VECTOR);

	delta = local_cpu_data->itm_delta;
	/*
	 * Stagger the timer tick for each CPU so they don't occur all at (almost) the
	 * same time:
	 */
	if (cpu) {
		unsigned long hi = 1UL << ia64_fls(cpu);
		shift = (2*(cpu - hi) + 1) * delta/hi/2;
	}
	local_cpu_data->itm_next = ia64_get_itc() + delta + shift;
	ia64_set_itm(local_cpu_data->itm_next);
}

void __init
ia64_init_itm (void)
{
	unsigned long platform_base_freq, itc_freq, drift;
	struct pal_freq_ratio itc_ratio, proc_ratio;
	long status;

	/*
	 * According to SAL v2.6, we need to use a SAL call to determine the platform base
	 * frequency and then a PAL call to determine the frequency ratio between the ITC
	 * and the base frequency.
	 */
	status = ia64_sal_freq_base(SAL_FREQ_BASE_PLATFORM, &platform_base_freq, &drift);
	if (status != 0) {
		printk(KERN_ERR "SAL_FREQ_BASE_PLATFORM failed: %s\n", ia64_sal_strerror(status));
	} else {
		status = ia64_pal_freq_ratios(&proc_ratio, 0, &itc_ratio);
		if (status != 0)
			printk(KERN_ERR "PAL_FREQ_RATIOS failed with status=%ld\n", status);
	}
	if (status != 0) {
		/* invent "random" values */
		printk(KERN_ERR
		       "SAL/PAL failed to obtain frequency info---inventing reasonably values\n");
		platform_base_freq = 100000000;
		itc_ratio.num = 3;
		itc_ratio.den = 1;
	}
	if (platform_base_freq < 40000000) {
		printk(KERN_ERR "Platform base frequency %lu bogus---resetting to 75MHz!\n",
		       platform_base_freq);
		platform_base_freq = 75000000;
	}
	if (!proc_ratio.den)
		proc_ratio.den = 1;	/* avoid division by zero */
	if (!itc_ratio.den)
		itc_ratio.den = 1;	/* avoid division by zero */

	itc_freq = (platform_base_freq*itc_ratio.num)/itc_ratio.den;
	local_cpu_data->itm_delta = (itc_freq + HZ/2) / HZ;
	printk(KERN_INFO "CPU %d: base freq=%lu.%03luMHz, ITC ratio=%lu/%lu, "
	       "ITC freq=%lu.%03luMHz\n", smp_processor_id(),
	       platform_base_freq / 1000000, (platform_base_freq / 1000) % 1000,
	       itc_ratio.num, itc_ratio.den, itc_freq / 1000000, (itc_freq / 1000) % 1000);

	local_cpu_data->proc_freq = (platform_base_freq*proc_ratio.num)/proc_ratio.den;
	local_cpu_data->itc_freq = itc_freq;
	local_cpu_data->cyc_per_usec = (itc_freq + 500000) / 1000000;
	local_cpu_data->usec_per_cyc = ((1000000UL<<IA64_USEC_PER_CYC_SHIFT)
					+ itc_freq/2)/itc_freq;

	/* Setup the CPU local timer tick */
	ia64_cpu_local_tick();
}

static struct irqaction timer_irqaction = {
	.handler =	timer_interrupt,
	.flags =	SA_INTERRUPT,
	.name =		"timer"
};

void __init
time_init (void)
{
	register_percpu_irq(IA64_TIMER_VECTOR, &timer_irqaction);
	efi_gettimeofday((struct timeval *) &xtime);
	ia64_init_itm();
}
