/****************************************************************************/
/* 
 * linux/arch/sh/kernel/setup_snapgear.c
 *
 * Copyright (C) 2002  David McCullough <davidm@snapgear.com>
 *
 * Based on files with the following comments:
 *
 *           Copyright (C) 2000  Kazumoto Kojima
 *
 *           Modified for 7751 Solution Engine by
 *           Ian da Silva and Jeremy Siegel, 2001.
 */
/****************************************************************************/

#include <linux/config.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/sched.h>

#include <asm/irq.h>
#include <asm/io.h>

/****************************************************************************/
/*
 * EraseConfig handling functions
 */

static void
eraseconfig_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
#if defined(CONFIG_SH_SECUREEDGE5410)
{
	volatile char dummy = * (volatile char *) 0xb8000000;
}
#endif
#ifdef CONFIG_LEDMAN
	ledman_signalreset();
#else
	printk("SnapGear: erase switch interrupt!\n");
#endif
}

static int __init
eraseconfig_init(void)
{
	printk("SnapGear: EraseConfig init\n");
	/* Setup "EraseConfig" switch on external IRQ 0 */
	if (request_irq(IRL0_IRQ, eraseconfig_interrupt, SA_INTERRUPT,
				"Erase Config", NULL))
		printk("SnapGear: failed to register IRQ%d for Reset witch\n",
				IRL0_IRQ);
	else
		printk("SnapGear: registered EraseConfig switch on IRQ%d\n",
				IRL0_IRQ);
	return(0);
}

module_init(eraseconfig_init);

/****************************************************************************/
/*
 * Initialize IRQ setting
 *
 * IRL0 = erase switch
 * IRL1 = eth0
 * IRL2 = eth1
 * IRL3 = crypto
 */

void __init
init_snapgear_IRQ(void)
{
	/* enable individual interrupt mode for externals */
	ctrl_outw(ctrl_inw(INTC_ICR) | INTC_ICR_IRLM, INTC_ICR);

	printk("Setup SnapGear IRQ/IPR ...\n");

	make_ipr_irq(IRL0_IRQ, IRL0_IPR_ADDR, IRL0_IPR_POS, IRL0_PRIORITY);
	make_ipr_irq(IRL1_IRQ, IRL1_IPR_ADDR, IRL1_IPR_POS, IRL1_PRIORITY);
	make_ipr_irq(IRL2_IRQ, IRL2_IPR_ADDR, IRL2_IPR_POS, IRL2_PRIORITY);
	make_ipr_irq(IRL3_IRQ, IRL3_IPR_ADDR, IRL3_IPR_POS, IRL3_PRIORITY);
}

/****************************************************************************/
/*
 *	Fast poll interrupt simulator.
 */

#define FAST_POLL	1000
//#define FAST_POLL_INTR

#define FASTTIMER_IRQ   17
#define FASTTIMER_IPR_ADDR  INTC_IPRA
#define FASTTIMER_IPR_POS    2
#define FASTTIMER_PRIORITY   3

#ifdef FAST_POLL_INTR
#define TMU1_TCR_INIT	0x0020
#else
#define TMU1_TCR_INIT	0
#endif
#define TMU_TSTR_INIT	1
#define TMU1_TCR_CALIB	0x0000
#define TMU_TOCR	0xffd80000	/* Byte access */
#define TMU_TSTR	0xffd80004	/* Byte access */
#define TMU1_TCOR	0xffd80014	/* Long access */
#define TMU1_TCNT	0xffd80018	/* Long access */
#define TMU1_TCR	0xffd8001c	/* Word access */


#ifdef FAST_POLL_INTR
static void fast_timer_irq(int irq, void *dev_instance, struct pt_regs *regs)
{
	unsigned long timer_status;
    timer_status = ctrl_inw(TMU1_TCR);
	timer_status &= ~0x100;
	ctrl_outw(timer_status, TMU1_TCR);
}
#endif

/*
 * return the current ticks on the fast timer
 */

unsigned long
fast_timer_count(void)
{
	return(ctrl_inl(TMU1_TCNT));
}

/*
 * setup a fast timer for profiling etc etc
 */

static void
setup_fast_timer()
{
	unsigned long interval;

#ifdef FAST_POLL_INTR
	interval = (current_cpu_data.module_clock/4 + FAST_POLL/2) / FAST_POLL;

	make_ipr_irq(FASTTIMER_IRQ, FASTTIMER_IPR_ADDR, FASTTIMER_IPR_POS,
			FASTTIMER_PRIORITY);

	printk("SnapGear: %dHz fast timer on IRQ %d\n",FAST_POLL,FASTTIMER_IRQ);

	if (request_irq(FASTTIMER_IRQ, fast_timer_irq, 0, "SnapGear fast timer",
			NULL) != 0)
		printk("%s(%d): request_irq() failed?\n", __FILE__, __LINE__);
#else
	printk("SnapGear: fast timer running\n",FAST_POLL,FASTTIMER_IRQ);
	interval = 0xffffffff;
#endif

	ctrl_outb(ctrl_inb(TMU_TSTR) & ~0x2, TMU_TSTR); /* disable timer 1 */
	ctrl_outw(TMU1_TCR_INIT, TMU1_TCR);
	ctrl_outl(interval, TMU1_TCOR);
	ctrl_outl(interval, TMU1_TCNT);
	ctrl_outb(ctrl_inb(TMU_TSTR) | 0x2, TMU_TSTR); /* enable timer 1 */

	printk("Timer count 1 = 0x%x\n", fast_timer_count());
	udelay(1000);
	printk("Timer count 2 = 0x%x\n", fast_timer_count());
}

/****************************************************************************/
/*
 * Initialize the board
 */

void __init
setup_snapgear(void)
{
	/* XXX: RTC setting comes here */
#ifdef CONFIG_RTC
	secureedge5410_rtc_init();
#endif
 	// setup_fast_timer();
}

/****************************************************************************/
