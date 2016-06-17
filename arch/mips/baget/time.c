/*
 * time.c: Baget/MIPS specific time handling details
 *
 * Copyright (C) 1998 Gleb Raiko & Vladimir Roganov
 */

#include <linux/errno.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/param.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/timex.h>
#include <linux/spinlock.h>

#include <asm/bootinfo.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/ptrace.h>
#include <asm/system.h>

#include <asm/baget/baget.h>

extern rwlock_t xtime_lock;

/*
 *  To have precision clock, we need to fix available clock frequency
 */
#define FREQ_NOM  79125  /* Baget frequency ratio */
#define FREQ_DEN  10000

static inline int timer_intr_valid(void)
{
	static unsigned long long ticks, valid_ticks;

	if (ticks++ * FREQ_DEN >= valid_ticks * FREQ_NOM) {
		/*
		 *  We need no overflow checks,
		 *  due baget unable to work 3000 years...
		 *  At least without reboot...
		 */
		valid_ticks++;
		return 1;
	}
	return 0;
}

void static timer_interrupt(int irq, void *dev_id, struct pt_regs * regs)
{
	if (timer_intr_valid()) {
		sti();
		do_timer(regs);
	}
}

static void __init timer_enable(void)
{
	unsigned char ss0cr0 = vic_inb(VIC_SS0CR0);
	ss0cr0 &= ~VIC_SS0CR0_TIMER_FREQ_MASK;
	ss0cr0 |= VIC_SS0CR0_TIMER_FREQ_1000HZ;
	vic_outb(ss0cr0, VIC_SS0CR0);

	vic_outb(VIC_INT_IPL(6)|VIC_INT_NOAUTO|VIC_INT_EDGE|
		 VIC_INT_LOW|VIC_INT_ENABLE, VIC_LINT2);
}

static struct irqaction timer_irq  =
{ timer_interrupt, SA_INTERRUPT, 0, "timer", NULL, NULL};

void __init time_init(void)
{
	if (setup_baget_irq(BAGET_VIC_TIMER_IRQ, &timer_irq) < 0)
		printk("time_init: unable request irq for system timer\n");
	timer_enable();
	/* We don't call sti() here, because it is too early for baget */
}

void do_gettimeofday(struct timeval *tv)
{
	unsigned long flags;

	read_lock_irqsave (&xtime_lock, flags);
	*tv = xtime;
	read_unlock_irqrestore (&xtime_lock, flags);
}

void do_settimeofday(struct timeval *tv)
{
	write_lock_irq (&xtime_lock);
	xtime = *tv;
	time_adjust = 0;		/* stop active adjtime() */
	time_status |= STA_UNSYNC;
	time_maxerror = NTP_PHASE_LIMIT;
	time_esterror = NTP_PHASE_LIMIT;
	write_unlock_irq (&xtime_lock);
}
