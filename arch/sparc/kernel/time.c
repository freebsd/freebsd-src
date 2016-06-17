/* $Id: time.c,v 1.59.2.1 2002/01/23 14:35:45 davem Exp $
 * linux/arch/sparc/kernel/time.c
 *
 * Copyright (C) 1995 David S. Miller (davem@caip.rutgers.edu)
 * Copyright (C) 1996 Thomas K. Dyas (tdyas@eden.rutgers.edu)
 *
 * Chris Davis (cdavis@cois.on.ca) 03/27/1998
 * Added support for the intersil on the sun4/4200
 *
 * Gleb Raiko (rajko@mech.math.msu.su) 08/18/1998
 * Support for MicroSPARC-IIep, PCI CPU.
 *
 * This file handles the Sparc specific time handling details.
 *
 * 1997-09-10	Updated NTP code according to technical memorandum Jan '96
 *		"A Kernel Model for Precision Timekeeping" by Dave Mills
 */
#include <linux/config.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/param.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/timex.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/ioport.h>

#include <asm/oplib.h>
#include <asm/segment.h>
#include <asm/timer.h>
#include <asm/mostek.h>
#include <asm/system.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/idprom.h>
#include <asm/machines.h>
#include <asm/sun4paddr.h>
#include <asm/page.h>
#include <asm/pcic.h>

extern rwlock_t xtime_lock;

enum sparc_clock_type sp_clock_typ;
spinlock_t mostek_lock = SPIN_LOCK_UNLOCKED;
unsigned long mstk48t02_regs = 0UL;
static struct mostek48t08 *mstk48t08_regs = 0;
static int set_rtc_mmss(unsigned long);
static void sbus_do_settimeofday(struct timeval *tv);

#ifdef CONFIG_SUN4
struct intersil *intersil_clock;
#define intersil_cmd(intersil_reg, intsil_cmd) intersil_reg->int_cmd_reg = \
	(intsil_cmd)

#define intersil_intr(intersil_reg, intsil_cmd) intersil_reg->int_intr_reg = \
	(intsil_cmd)

#define intersil_start(intersil_reg) intersil_cmd(intersil_reg, \
	( INTERSIL_START | INTERSIL_32K | INTERSIL_NORMAL | INTERSIL_24H |\
	  INTERSIL_INTR_ENABLE))

#define intersil_stop(intersil_reg) intersil_cmd(intersil_reg, \
	( INTERSIL_STOP | INTERSIL_32K | INTERSIL_NORMAL | INTERSIL_24H |\
	  INTERSIL_INTR_ENABLE))

#define intersil_read_intr(intersil_reg, towhere) towhere = \
	intersil_reg->int_intr_reg

#endif

static spinlock_t ticker_lock = SPIN_LOCK_UNLOCKED;

/* 32-bit Sparc specific profiling function. */
void sparc_do_profile(unsigned long pc, unsigned long o7)
{
	if(prof_buffer && current->pid) {
		extern int _stext;
		extern int __copy_user_begin, __copy_user_end;
		extern int __atomic_begin, __atomic_end;
		extern int __bzero_begin, __bzero_end;
		extern int __bitops_begin, __bitops_end;

		if ((pc >= (unsigned long) &__copy_user_begin &&
		     pc < (unsigned long) &__copy_user_end) ||
		    (pc >= (unsigned long) &__atomic_begin &&
		     pc < (unsigned long) &__atomic_end) ||
		    (pc >= (unsigned long) &__bzero_begin &&
		     pc < (unsigned long) &__bzero_end) ||
		    (pc >= (unsigned long) &__bitops_begin &&
		     pc < (unsigned long) &__bitops_end))
			pc = o7;

		pc -= (unsigned long) &_stext;
		pc >>= prof_shift;

		spin_lock(&ticker_lock);
		if(pc < prof_len)
			prof_buffer[pc]++;
		else
			prof_buffer[prof_len - 1]++;
		spin_unlock(&ticker_lock);
	}
}

__volatile__ unsigned int *master_l10_counter;
__volatile__ unsigned int *master_l10_limit;

/*
 * timer_interrupt() needs to keep up the real-time clock,
 * as well as call the "do_timer()" routine every clocktick
 */
void timer_interrupt(int irq, void *dev_id, struct pt_regs * regs)
{
	/* last time the cmos clock got updated */
	static long last_rtc_update;

#ifndef CONFIG_SMP
	if(!user_mode(regs))
		sparc_do_profile(regs->pc, regs->u_regs[UREG_RETPC]);
#endif

#ifdef CONFIG_SUN4
	if((idprom->id_machtype == (SM_SUN4 | SM_4_260)) ||
	   (idprom->id_machtype == (SM_SUN4 | SM_4_110))) {
		int temp;
        	intersil_read_intr(intersil_clock, temp);
		/* re-enable the irq */
		enable_pil_irq(10);
	}
#endif
	clear_clock_irq();

	write_lock(&xtime_lock);

	do_timer(regs);

	/* Determine when to update the Mostek clock. */
	if ((time_status & STA_UNSYNC) == 0 &&
	    xtime.tv_sec > last_rtc_update + 660 &&
	    xtime.tv_usec >= 500000 - ((unsigned) tick) / 2 &&
	    xtime.tv_usec <= 500000 + ((unsigned) tick) / 2) {
	  if (set_rtc_mmss(xtime.tv_sec) == 0)
	    last_rtc_update = xtime.tv_sec;
	  else
	    last_rtc_update = xtime.tv_sec - 600; /* do it again in 60 s */
	}
	write_unlock(&xtime_lock);
}

/* Kick start a stopped clock (procedure from the Sun NVRAM/hostid FAQ). */
static void __init kick_start_clock(void)
{
	struct mostek48t02 *regs = (struct mostek48t02 *)mstk48t02_regs;
	unsigned char sec;
	int i, count;

	prom_printf("CLOCK: Clock was stopped. Kick start ");

	spin_lock_irq(&mostek_lock);

	/* Turn on the kick start bit to start the oscillator. */
	regs->creg |= MSTK_CREG_WRITE;
	regs->sec &= ~MSTK_STOP;
	regs->hour |= MSTK_KICK_START;
	regs->creg &= ~MSTK_CREG_WRITE;

	spin_unlock_irq(&mostek_lock);

	/* Delay to allow the clock oscillator to start. */
	sec = MSTK_REG_SEC(regs);
	for (i = 0; i < 3; i++) {
		while (sec == MSTK_REG_SEC(regs))
			for (count = 0; count < 100000; count++)
				/* nothing */ ;
		prom_printf(".");
		sec = regs->sec;
	}
	prom_printf("\n");

	spin_lock_irq(&mostek_lock);

	/* Turn off kick start and set a "valid" time and date. */
	regs->creg |= MSTK_CREG_WRITE;
	regs->hour &= ~MSTK_KICK_START;
	MSTK_SET_REG_SEC(regs,0);
	MSTK_SET_REG_MIN(regs,0);
	MSTK_SET_REG_HOUR(regs,0);
	MSTK_SET_REG_DOW(regs,5);
	MSTK_SET_REG_DOM(regs,1);
	MSTK_SET_REG_MONTH(regs,8);
	MSTK_SET_REG_YEAR(regs,1996 - MSTK_YEAR_ZERO);
	regs->creg &= ~MSTK_CREG_WRITE;

	spin_unlock_irq(&mostek_lock);

	/* Ensure the kick start bit is off. If it isn't, turn it off. */
	while (regs->hour & MSTK_KICK_START) {
		prom_printf("CLOCK: Kick start still on!\n");

		spin_lock_irq(&mostek_lock);
		regs->creg |= MSTK_CREG_WRITE;
		regs->hour &= ~MSTK_KICK_START;
		regs->creg &= ~MSTK_CREG_WRITE;
		spin_unlock_irq(&mostek_lock);
	}

	prom_printf("CLOCK: Kick start procedure successful.\n");
}

/* Return nonzero if the clock chip battery is low. */
static __inline__ int has_low_battery(void)
{
	struct mostek48t02 *regs = (struct mostek48t02 *)mstk48t02_regs;
	unsigned char data1, data2;

	spin_lock_irq(&mostek_lock);
	data1 = regs->eeprom[0];	/* Read some data. */
	regs->eeprom[0] = ~data1;	/* Write back the complement. */
	data2 = regs->eeprom[0];	/* Read back the complement. */
	regs->eeprom[0] = data1;	/* Restore the original value. */
	spin_unlock_irq(&mostek_lock);

	return (data1 == data2);	/* Was the write blocked? */
}

/* Probe for the real time clock chip on Sun4 */
static __inline__ void sun4_clock_probe(void)
{
#ifdef CONFIG_SUN4
	int temp;
	struct resource r;

	memset(&r, 0, sizeof(r));
	if( idprom->id_machtype == (SM_SUN4 | SM_4_330) ) {
		sp_clock_typ = MSTK48T02;
		r.start = sun4_clock_physaddr;
		mstk48t02_regs = sbus_ioremap(&r, 0,
				       sizeof(struct mostek48t02), 0);
		mstk48t08_regs = 0;  /* To catch weirdness */
		intersil_clock = 0;  /* just in case */

		/* Kick start the clock if it is completely stopped. */
		if (mostek_read(mstk48t02_regs + MOSTEK_SEC) & MSTK_STOP)
			kick_start_clock();
	} else if( idprom->id_machtype == (SM_SUN4 | SM_4_260)) {
		/* intersil setup code */
		printk("Clock: INTERSIL at %8x ",sun4_clock_physaddr);
		sp_clock_typ = INTERSIL;
		r.start = sun4_clock_physaddr;
		intersil_clock = (struct intersil *) 
		    sbus_ioremap(&r, 0, sizeof(*intersil_clock), "intersil");
		mstk48t02_regs = 0;  /* just be sure */
		mstk48t08_regs = 0;  /* ditto */
		/* initialise the clock */

		intersil_intr(intersil_clock,INTERSIL_INT_100HZ);

		intersil_start(intersil_clock);

		intersil_read_intr(intersil_clock, temp);
                while (!(temp & 0x80))
                        intersil_read_intr(intersil_clock, temp);

                intersil_read_intr(intersil_clock, temp);
                while (!(temp & 0x80))
                        intersil_read_intr(intersil_clock, temp);

		intersil_stop(intersil_clock);

	}
#endif
}

/* Probe for the mostek real time clock chip. */
static __inline__ void clock_probe(void)
{
	struct linux_prom_registers clk_reg[2];
	char model[128];
	register int node, cpuunit, bootbus;
	struct resource r;

	cpuunit = bootbus = 0;
	memset(&r, 0, sizeof(r));

	/* Determine the correct starting PROM node for the probe. */
	node = prom_getchild(prom_root_node);
	switch (sparc_cpu_model) {
	case sun4c:
		break;
	case sun4m:
		node = prom_getchild(prom_searchsiblings(node, "obio"));
		break;
	case sun4d:
		node = prom_getchild(bootbus = prom_searchsiblings(prom_getchild(cpuunit = prom_searchsiblings(node, "cpu-unit")), "bootbus"));
		break;
	default:
		prom_printf("CLOCK: Unsupported architecture!\n");
		prom_halt();
	}

	/* Find the PROM node describing the real time clock. */
	sp_clock_typ = MSTK_INVALID;
	node = prom_searchsiblings(node,"eeprom");
	if (!node) {
		prom_printf("CLOCK: No clock found!\n");
		prom_halt();
	}

	/* Get the model name and setup everything up. */
	model[0] = '\0';
	prom_getstring(node, "model", model, sizeof(model));
	if (strcmp(model, "mk48t02") == 0) {
		sp_clock_typ = MSTK48T02;
		if (prom_getproperty(node, "reg", (char *) clk_reg, sizeof(clk_reg)) == -1) {
			prom_printf("clock_probe: FAILED!\n");
			prom_halt();
		}
		if (sparc_cpu_model == sun4d)
			prom_apply_generic_ranges (bootbus, cpuunit, clk_reg, 1);
		else
			prom_apply_obio_ranges(clk_reg, 1);
		/* Map the clock register io area read-only */
		r.flags = clk_reg[0].which_io;
		r.start = clk_reg[0].phys_addr;
		mstk48t02_regs = sbus_ioremap(&r, 0,
		    sizeof(struct mostek48t02), "mk48t02");
		mstk48t08_regs = 0;  /* To catch weirdness */
	} else if (strcmp(model, "mk48t08") == 0) {
		sp_clock_typ = MSTK48T08;
		if(prom_getproperty(node, "reg", (char *) clk_reg,
				    sizeof(clk_reg)) == -1) {
			prom_printf("clock_probe: FAILED!\n");
			prom_halt();
		}
		if (sparc_cpu_model == sun4d)
			prom_apply_generic_ranges (bootbus, cpuunit, clk_reg, 1);
		else
			prom_apply_obio_ranges(clk_reg, 1);
		/* Map the clock register io area read-only */
		/* XXX r/o attribute is somewhere in r.flags */
		r.flags = clk_reg[0].which_io;
		r.start = clk_reg[0].phys_addr;
		mstk48t08_regs = (struct mostek48t08 *) sbus_ioremap(&r, 0,
		    sizeof(struct mostek48t08), "mk48t08");

		mstk48t02_regs = (unsigned long)&mstk48t08_regs->regs;
	} else {
		prom_printf("CLOCK: Unknown model name '%s'\n",model);
		prom_halt();
	}

	/* Report a low battery voltage condition. */
	if (has_low_battery())
		printk(KERN_CRIT "NVRAM: Low battery voltage!\n");

	/* Kick start the clock if it is completely stopped. */
	if (mostek_read(mstk48t02_regs + MOSTEK_SEC) & MSTK_STOP)
		kick_start_clock();
}

void __init sbus_time_init(void)
{
	unsigned int year, mon, day, hour, min, sec;
	struct mostek48t02 *mregs;

#ifdef CONFIG_SUN4
	int temp;
	struct intersil *iregs;
#endif

	BTFIXUPSET_CALL(bus_do_settimeofday, sbus_do_settimeofday, BTFIXUPCALL_NORM);
	btfixup();

	if (ARCH_SUN4)
		sun4_clock_probe();
	else
		clock_probe();

	sparc_init_timers(timer_interrupt);
	
#ifdef CONFIG_SUN4
	if(idprom->id_machtype == (SM_SUN4 | SM_4_330)) {
#endif
	mregs = (struct mostek48t02 *)mstk48t02_regs;
	if(!mregs) {
		prom_printf("Something wrong, clock regs not mapped yet.\n");
		prom_halt();
	}		
	spin_lock_irq(&mostek_lock);
	mregs->creg |= MSTK_CREG_READ;
	sec = MSTK_REG_SEC(mregs);
	min = MSTK_REG_MIN(mregs);
	hour = MSTK_REG_HOUR(mregs);
	day = MSTK_REG_DOM(mregs);
	mon = MSTK_REG_MONTH(mregs);
	year = MSTK_CVT_YEAR( MSTK_REG_YEAR(mregs) );
	xtime.tv_sec = mktime(year, mon, day, hour, min, sec);
	xtime.tv_usec = 0;
	mregs->creg &= ~MSTK_CREG_READ;
	spin_unlock_irq(&mostek_lock);
#ifdef CONFIG_SUN4
	} else if(idprom->id_machtype == (SM_SUN4 | SM_4_260) ) {
		/* initialise the intersil on sun4 */

		iregs=intersil_clock;
		if(!iregs) {
			prom_printf("Something wrong, clock regs not mapped yet.\n");
			prom_halt();
		}

		intersil_intr(intersil_clock,INTERSIL_INT_100HZ);
		disable_pil_irq(10);
		intersil_stop(iregs);
		intersil_read_intr(intersil_clock, temp);

		temp = iregs->clk.int_csec;

		sec = iregs->clk.int_sec;
		min = iregs->clk.int_min;
		hour = iregs->clk.int_hour;
		day = iregs->clk.int_day;
		mon = iregs->clk.int_month;
		year = MSTK_CVT_YEAR(iregs->clk.int_year);

		enable_pil_irq(10);
		intersil_start(iregs);

		xtime.tv_sec = mktime(year, mon, day, hour, min, sec);
		xtime.tv_usec = 0;
		printk("%u/%u/%u %u:%u:%u\n",day,mon,year,hour,min,sec);
	}
#endif

	/* Now that OBP ticker has been silenced, it is safe to enable IRQ. */
	__sti();
}

void __init time_init(void)
{
#ifdef CONFIG_PCI
	extern void pci_time_init(void);
	if (pcic_present()) {
		pci_time_init();
		return;
	}
#endif
	sbus_time_init();
}

extern __inline__ unsigned long do_gettimeoffset(void)
{
	struct tasklet_struct *t;
	unsigned long offset = 0;
	unsigned int count;

	count = (*master_l10_counter >> 10) & 0x1fffff;

	t = &bh_task_vec[TIMER_BH];
	if (test_bit(TASKLET_STATE_SCHED, &t->state))
		offset = 1000000;

	return offset + count;
}

/* This need not obtain the xtime_lock as it is coded in
 * an implicitly SMP safe way already.
 */
void do_gettimeofday(struct timeval *tv)
{
	/* Load doubles must be used on xtime so that what we get
	 * is guarenteed to be atomic, this is why we can run this
	 * with interrupts on full blast.  Don't touch this... -DaveM
	 */
	__asm__ __volatile__(
	"sethi	%hi(master_l10_counter), %o1\n\t"
	"ld	[%o1 + %lo(master_l10_counter)], %g3\n\t"
	"sethi	%hi(xtime), %g2\n"
	"1:\n\t"
	"ldd	[%g2 + %lo(xtime)], %o4\n\t"
	"ld	[%g3], %o1\n\t"
	"ldd	[%g2 + %lo(xtime)], %o2\n\t"
	"xor	%o4, %o2, %o2\n\t"
	"xor	%o5, %o3, %o3\n\t"
	"orcc	%o2, %o3, %g0\n\t"
	"bne	1b\n\t"
	" cmp	%o1, 0\n\t"
	"bge	1f\n\t"
	" srl	%o1, 0xa, %o1\n\t"
	"sethi	%hi(tick), %o3\n\t"
	"ld	[%o3 + %lo(tick)], %o3\n\t"
	"sethi	%hi(0x1fffff), %o2\n\t"
	"or	%o2, %lo(0x1fffff), %o2\n\t"
	"add	%o5, %o3, %o5\n\t"
	"and	%o1, %o2, %o1\n"
	"1:\n\t"
	"add	%o5, %o1, %o5\n\t"
	"sethi	%hi(1000000), %o2\n\t"
	"or	%o2, %lo(1000000), %o2\n\t"
	"cmp	%o5, %o2\n\t"
	"bl,a	1f\n\t"
	" st	%o4, [%o0 + 0x0]\n\t"
	"add	%o4, 0x1, %o4\n\t"
	"sub	%o5, %o2, %o5\n\t"
	"st	%o4, [%o0 + 0x0]\n"
	"1:\n\t"
	"st	%o5, [%o0 + 0x4]\n");
}

void do_settimeofday(struct timeval *tv)
{
	write_lock_irq(&xtime_lock);
	bus_do_settimeofday(tv);
	write_unlock_irq(&xtime_lock);
}

static void sbus_do_settimeofday(struct timeval *tv)
{
	tv->tv_usec -= do_gettimeoffset();
	if(tv->tv_usec < 0) {
		tv->tv_usec += 1000000;
		tv->tv_sec--;
	}
	xtime = *tv;
	time_adjust = 0;		/* stop active adjtime() */
	time_status |= STA_UNSYNC;
	time_maxerror = NTP_PHASE_LIMIT;
	time_esterror = NTP_PHASE_LIMIT;
}

/*
 * BUG: This routine does not handle hour overflow properly; it just
 *      sets the minutes. Usually you won't notice until after reboot!
 */
static int set_rtc_mmss(unsigned long nowtime)
{
	int real_seconds, real_minutes, mostek_minutes;
	struct mostek48t02 *regs = (struct mostek48t02 *)mstk48t02_regs;
	unsigned long flags;
#ifdef CONFIG_SUN4
	struct intersil *iregs = intersil_clock;
	int temp;
#endif

	/* Not having a register set can lead to trouble. */
	if (!regs) {
#ifdef CONFIG_SUN4
		if(!iregs)
		return -1;
	 	else {
			temp = iregs->clk.int_csec;

			mostek_minutes = iregs->clk.int_min;

			real_seconds = nowtime % 60;
			real_minutes = nowtime / 60;
			if (((abs(real_minutes - mostek_minutes) + 15)/30) & 1)
				real_minutes += 30;	/* correct for half hour time zone */
			real_minutes %= 60;

			if (abs(real_minutes - mostek_minutes) < 30) {
				intersil_stop(iregs);
				iregs->clk.int_sec=real_seconds;
				iregs->clk.int_min=real_minutes;
				intersil_start(iregs);
			} else {
				printk(KERN_WARNING
			       "set_rtc_mmss: can't update from %d to %d\n",
				       mostek_minutes, real_minutes);
				return -1;
			}
			
			return 0;
		}
#endif
	}

	spin_lock_irqsave(&mostek_lock, flags);
	/* Read the current RTC minutes. */
	regs->creg |= MSTK_CREG_READ;
	mostek_minutes = MSTK_REG_MIN(regs);
	regs->creg &= ~MSTK_CREG_READ;

	/*
	 * since we're only adjusting minutes and seconds,
	 * don't interfere with hour overflow. This avoids
	 * messing with unknown time zones but requires your
	 * RTC not to be off by more than 15 minutes
	 */
	real_seconds = nowtime % 60;
	real_minutes = nowtime / 60;
	if (((abs(real_minutes - mostek_minutes) + 15)/30) & 1)
		real_minutes += 30;	/* correct for half hour time zone */
	real_minutes %= 60;

	if (abs(real_minutes - mostek_minutes) < 30) {
		regs->creg |= MSTK_CREG_WRITE;
		MSTK_SET_REG_SEC(regs,real_seconds);
		MSTK_SET_REG_MIN(regs,real_minutes);
		regs->creg &= ~MSTK_CREG_WRITE;
		spin_unlock_irqrestore(&mostek_lock, flags);
		return 0;
	} else {
		spin_unlock_irqrestore(&mostek_lock, flags);
		return -1;
	}
}
