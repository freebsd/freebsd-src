/*
 *
 * BRIEF MODULE DESCRIPTION
 *	Galileo EV96100 rtc routines.
 *
 * Copyright 2000 MontaVista Software Inc.
 * Author: MontaVista Software, Inc.
 *         	ppopov@mvista.com or source@mvista.com
 *
 * This file was derived from Carsten Langgaard's
 * arch/mips/mips-boards/atlas/atlas_rtc.c.
 *
 * Carsten Langgaard, carstenl@mips.com
 * Copyright (C) 1999,2000 MIPS Technologies, Inc.  All rights reserved.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <linux/init.h>
#include <linux/kernel_stat.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/timex.h>

#include <asm/mipsregs.h>
#include <asm/ptrace.h>


#define ALLINTS (IE_IRQ0 | IE_IRQ1 | IE_IRQ2 | IE_IRQ3 | IE_IRQ4 | IE_IRQ5)

extern volatile unsigned long wall_jiffies;
unsigned long missed_heart_beats = 0;

static unsigned long r4k_offset; /* Amount to increment compare reg each time */
static unsigned long r4k_cur;    /* What counter should be at next timer irq */
extern rwlock_t xtime_lock;

static inline void ack_r4ktimer(unsigned long newval)
{
	write_c0_compare(newval);
}

static int set_rtc_mmss(unsigned long nowtime)
{
    /* EV96100 does not have a real time clock */
    int retval = 0;

    return retval;
}



/*
 * Figure out the r4k offset, the amount to increment the compare
 * register for each time tick.
 * Use the RTC to calculate offset.
 */
static unsigned long __init cal_r4koff(void)
{
	unsigned long count;
        count = 300000000/2;
	return (count / HZ);
}


static unsigned long __init get_mips_time(void)
{
	unsigned int year, mon, day, hour, min, sec;

        year = 2000;
        mon = 10;
        day = 31;
        hour = 0;
        min = 0;
        sec = 0;
	return mktime(year, mon, day, hour, min, sec);
}


/*
 * called from start_kernel()
 */
void __init time_init(void)
{

        unsigned int est_freq;

	r4k_offset = cal_r4koff();

	est_freq = 2*r4k_offset*HZ;
	est_freq += 5000;    /* round */
	est_freq -= est_freq%10000;
	printk("CPU frequency %d.%02d MHz\n", est_freq/1000000,
	       (est_freq%1000000)*100/1000000);
	r4k_cur = (read_c0_count() + r4k_offset);

	write_c0_compare(r4k_cur);

	change_c0_status(ST0_IM, IE_IRQ5);		/* FIX ME */
}

/* This is for machines which generate the exact clock. */
#define USECS_PER_JIFFY (1000000/HZ)

/* Cycle counter value at the previous timer interrupt.. */

static unsigned int timerhi = 0, timerlo = 0;

/*
 * FIXME: Does playing with the RP bit in c0_status interfere with this code?
 */
static unsigned long do_fast_gettimeoffset(void)
{
	u32 count;
	unsigned long res, tmp;

	/* Last jiffy when do_fast_gettimeoffset() was called. */
	static unsigned long last_jiffies=0;
	unsigned long quotient;

	/*
	 * Cached "1/(clocks per usec)*2^32" value.
	 * It has to be recalculated once each jiffy.
	 */
	static unsigned long cached_quotient=0;

	tmp = jiffies;

	quotient = cached_quotient;

	if (tmp && last_jiffies != tmp) {
		last_jiffies = tmp;
		__asm__(".set\tnoreorder\n\t"
			".set\tnoat\n\t"
			".set\tmips3\n\t"
			"lwu\t%0,%2\n\t"
			"dsll32\t$1,%1,0\n\t"
			"or\t$1,$1,%0\n\t"
			"ddivu\t$0,$1,%3\n\t"
			"mflo\t$1\n\t"
			"dsll32\t%0,%4,0\n\t"
			"nop\n\t"
			"ddivu\t$0,%0,$1\n\t"
			"mflo\t%0\n\t"
			".set\tmips0\n\t"
			".set\tat\n\t"
			".set\treorder"
			:"=&r" (quotient)
			:"r" (timerhi),
			 "m" (timerlo),
			 "r" (tmp),
			 "r" (USECS_PER_JIFFY));
		cached_quotient = quotient;
	}

	/* Get last timer tick in absolute kernel time */
	count = read_c0_count();

	/* .. relative to previous jiffy (32 bits is enough) */
	count -= timerlo;

	__asm__("multu\t%1,%2\n\t"
		"mfhi\t%0"
		:"=r" (res)
		:"r" (count),
		 "r" (quotient));

	/*
 	 * Due to possible jiffies inconsistencies, we need to check
	 * the result so that we'll get a timer that is monotonic.
	 */
	if (res >= USECS_PER_JIFFY)
		res = USECS_PER_JIFFY-1;

	return res;
}

void do_gettimeofday(struct timeval *tv)
{
	unsigned long flags;

	read_lock_irqsave (&xtime_lock, flags);
	*tv = xtime;
	tv->tv_usec += do_fast_gettimeoffset();

	/*
	 * xtime is atomically updated in timer_bh. jiffies - wall_jiffies
	 * is nonzero if the timer bottom half hasnt executed yet.
	 */
	if (jiffies - wall_jiffies)
		tv->tv_usec += USECS_PER_JIFFY;

	read_unlock_irqrestore (&xtime_lock, flags);

	if (tv->tv_usec >= 1000000) {
		tv->tv_usec -= 1000000;
		tv->tv_sec++;
	}
}

void do_settimeofday(struct timeval *tv)
{
	write_lock_irq (&xtime_lock);

	/* This is revolting. We need to set the xtime.tv_usec correctly.
	 * However, the value in this location is value at the last tick.
	 * Discover what correction gettimeofday would have done, and then
	 * undo it!
	 */
	tv->tv_usec -= do_fast_gettimeoffset();

	if (tv->tv_usec < 0) {
		tv->tv_usec += 1000000;
		tv->tv_sec--;
	}

	xtime = *tv;
	time_adjust = 0;		/* stop active adjtime() */
	time_status |= STA_UNSYNC;
	time_maxerror = NTP_PHASE_LIMIT;
	time_esterror = NTP_PHASE_LIMIT;

	write_unlock_irq (&xtime_lock);
}

/*
 * There are a lot of conceptually broken versions of the MIPS timer interrupt
 * handler floating around.  This one is rather different, but the algorithm
 * is probably more robust.
 */
void mips_timer_interrupt(struct pt_regs *regs)
{
        int irq = 7; /* FIX ME */

	if (r4k_offset == 0) {
            goto null;
        }

	do {
		kstat.irqs[0][irq]++;
		do_timer(regs);
		r4k_cur += r4k_offset;
		ack_r4ktimer(r4k_cur);

	} while (((unsigned long)read_c0_count()
                    - r4k_cur) < 0x7fffffff);
	return;

null:
	ack_r4ktimer(0);
}
