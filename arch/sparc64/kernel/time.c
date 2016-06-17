/* $Id: time.c,v 1.41.2.2 2002/03/03 04:08:10 davem Exp $
 * time.c: UltraSparc timer and TOD clock support.
 *
 * Copyright (C) 1997 David S. Miller (davem@caip.rutgers.edu)
 * Copyright (C) 1998 Eddie C. Dost   (ecd@skynet.be)
 *
 * Based largely on code which is:
 *
 * Copyright (C) 1996 Thomas K. Dyas (tdyas@eden.rutgers.edu)
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
#include <linux/ioport.h>
#include <linux/mc146818rtc.h>
#include <linux/delay.h>
#include <linux/timer.h>

#include <asm/oplib.h>
#include <asm/mostek.h>
#include <asm/timer.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/sbus.h>
#include <asm/fhc.h>
#include <asm/pbm.h>
#include <asm/ebus.h>
#include <asm/isa.h>
#include <asm/starfire.h>
#include <asm/smp.h>

extern rwlock_t xtime_lock;
extern unsigned long wall_jiffies;

spinlock_t mostek_lock = SPIN_LOCK_UNLOCKED;
spinlock_t rtc_lock = SPIN_LOCK_UNLOCKED;
unsigned long mstk48t02_regs = 0UL;
#ifdef CONFIG_PCI
unsigned long ds1287_regs = 0UL;
#endif

static unsigned long mstk48t08_regs = 0UL;
static unsigned long mstk48t59_regs = 0UL;

static int set_rtc_mmss(unsigned long);

struct sparc64_tick_ops *tick_ops;

#define TICK_PRIV_BIT	(1UL << 63)

static void tick_disable_protection(void)
{
	/* Set things up so user can access tick register for profiling
	 * purposes.  Also workaround BB_ERRATA_1 by doing a dummy
	 * read back of %tick after writing it.
	 */
	__asm__ __volatile__(
	"	ba,pt	%%xcc, 1f\n"
	"	 nop\n"
	"	.align	64\n"
	"1:	rd	%%tick, %%g2\n"
	"	add	%%g2, 6, %%g2\n"
	"	andn	%%g2, %0, %%g2\n"
	"	wrpr	%%g2, 0, %%tick\n"
	"	rdpr	%%tick, %%g0"
	: /* no outputs */
	: "r" (TICK_PRIV_BIT)
	: "g2");
}

static void tick_init_tick(unsigned long offset)
{
	tick_disable_protection();

	__asm__ __volatile__(
	"	rd	%%tick, %%g1\n"
	"	andn	%%g1, %1, %%g1\n"
	"	ba,pt	%%xcc, 1f\n"
	"	 add	%%g1, %0, %%g1\n"
	"	.align	64\n"
	"1:	wr	%%g1, 0x0, %%tick_cmpr\n"
	"	rd	%%tick_cmpr, %%g0"
	: /* no outputs */
	: "r" (offset), "r" (TICK_PRIV_BIT)
	: "g1");
}

static unsigned long tick_get_tick(void)
{
	unsigned long ret;

	__asm__ __volatile__("rd	%%tick, %0\n\t"
			     "mov	%0, %0"
			     : "=r" (ret));

	return ret & ~TICK_PRIV_BIT;
}

static unsigned long tick_get_compare(void)
{
	unsigned long ret;

	__asm__ __volatile__("rd	%%tick_cmpr, %0\n\t"
			     "mov	%0, %0"
			     : "=r" (ret));

	return ret;
}

static unsigned long tick_add_compare(unsigned long adj)
{
	unsigned long new_compare;

	/* Workaround for Spitfire Errata (#54 I think??), I discovered
	 * this via Sun BugID 4008234, mentioned in Solaris-2.5.1 patch
	 * number 103640.
	 *
	 * On Blackbird writes to %tick_cmpr can fail, the
	 * workaround seems to be to execute the wr instruction
	 * at the start of an I-cache line, and perform a dummy
	 * read back from %tick_cmpr right after writing to it. -DaveM
	 */
	__asm__ __volatile__("rd	%%tick_cmpr, %0\n\t"
			     "ba,pt	%%xcc, 1f\n\t"
			     " add	%0, %1, %0\n\t"
			     ".align	64\n"
			     "1:\n\t"
			     "wr	%0, 0, %%tick_cmpr\n\t"
			     "rd	%%tick_cmpr, %%g0"
			     : "=&r" (new_compare)
			     : "r" (adj));

	return new_compare;
}

static unsigned long tick_add_tick(unsigned long adj, unsigned long offset)
{
	unsigned long new_tick, tmp;

	/* Also need to handle Blackbird bug here too. */
	__asm__ __volatile__("rd	%%tick, %0\n\t"
			     "add	%0, %2, %0\n\t"
			     "wrpr	%0, 0, %%tick\n\t"
			     "andn	%0, %4, %1\n\t"
			     "ba,pt	%%xcc, 1f\n\t"
			     " add	%1, %3, %1\n\t"
			     ".align	64\n"
			     "1:\n\t"
			     "wr	%1, 0, %%tick_cmpr\n\t"
			     "rd	%%tick_cmpr, %%g0"
			     : "=&r" (new_tick), "=&r" (tmp)
			     : "r" (adj), "r" (offset), "r" (TICK_PRIV_BIT));

	return new_tick;
}

static struct sparc64_tick_ops tick_operations = {
	.init_tick	=	tick_init_tick,
	.get_tick	=	tick_get_tick,
	.get_compare	=	tick_get_compare,
	.add_tick	=	tick_add_tick,
	.add_compare	=	tick_add_compare,
	.softint_mask	=	1UL << 0,
};

static void stick_init_tick(unsigned long offset)
{
	tick_disable_protection();

	/* Let the user get at STICK too. */
	__asm__ __volatile__(
	"	rd	%%asr24, %%g2\n"
	"	andn	%%g2, %0, %%g2\n"
	"	wr	%%g2, 0, %%asr24"
	: /* no outputs */
	: "r" (TICK_PRIV_BIT)
	: "g1", "g2");

	__asm__ __volatile__(
	"	rd	%%asr24, %%g1\n"
	"	andn	%%g1, %1, %%g1\n"
	"	add	%%g1, %0, %%g1\n"
	"	wr	%%g1, 0x0, %%asr25"
	: /* no outputs */
	: "r" (offset), "r" (TICK_PRIV_BIT)
	: "g1");
}

static unsigned long stick_get_tick(void)
{
	unsigned long ret;

	__asm__ __volatile__("rd	%%asr24, %0"
			     : "=r" (ret));

	return ret & ~TICK_PRIV_BIT;
}

static unsigned long stick_get_compare(void)
{
	unsigned long ret;

	__asm__ __volatile__("rd	%%asr25, %0"
			     : "=r" (ret));

	return ret;
}

static unsigned long stick_add_tick(unsigned long adj, unsigned long offset)
{
	unsigned long new_tick, tmp;

	__asm__ __volatile__("rd	%%asr24, %0\n\t"
			     "add	%0, %2, %0\n\t"
			     "wr	%0, 0, %%asr24\n\t"
			     "andn	%0, %4, %1\n\t"
			     "add	%1, %3, %1\n\t"
			     "wr	%1, 0, %%asr25"
			     : "=&r" (new_tick), "=&r" (tmp)
			     : "r" (adj), "r" (offset), "r" (TICK_PRIV_BIT));

	return new_tick;
}

static unsigned long stick_add_compare(unsigned long adj)
{
	unsigned long new_compare;

	__asm__ __volatile__("rd	%%asr25, %0\n\t"
			     "add	%0, %1, %0\n\t"
			     "wr	%0, 0, %%asr25"
			     : "=&r" (new_compare)
			     : "r" (adj));

	return new_compare;
}

static struct sparc64_tick_ops stick_operations = {
	.init_tick	=	stick_init_tick,
	.get_tick	=	stick_get_tick,
	.get_compare	=	stick_get_compare,
	.add_tick	=	stick_add_tick,
	.add_compare	=	stick_add_compare,
	.softint_mask	=	1UL << 16,
};

/* On Hummingbird the STICK/STICK_CMPR register is implemented
 * in I/O space.  There are two 64-bit registers each, the
 * first holds the low 32-bits of the value and the second holds
 * the high 32-bits.
 *
 * Since STICK is constantly updating, we have to access it carefully.
 *
 * The sequence we use to read is:
 * 1) read low
 * 2) read high
 * 3) read low again, if it rolled over increment high by 1
 *
 * Writing STICK safely is also tricky:
 * 1) write low to zero
 * 2) write high
 * 3) write low
 */
#define HBIRD_STICKCMP_ADDR	0x1fe0000f060UL
#define HBIRD_STICK_ADDR	0x1fe0000f070UL

static unsigned long __hbird_read_stick(void)
{
	unsigned long ret, tmp1, tmp2, tmp3;
	unsigned long addr = HBIRD_STICK_ADDR;

	__asm__ __volatile__("ldxa	[%1] %5, %2\n\t"
			     "add	%1, 0x8, %1\n\t"
			     "ldxa	[%1] %5, %3\n\t"
			     "sub	%1, 0x8, %1\n\t"
			     "ldxa	[%1] %5, %4\n\t"
			     "cmp	%4, %2\n\t"
			     "blu,a,pn	%%xcc, 1f\n\t"
			     " add	%3, 1, %3\n"
			     "1:\n\t"
			     "sllx	%3, 32, %3\n\t"
			     "or	%3, %4, %0\n\t"
			     : "=&r" (ret), "=&r" (addr),
			       "=&r" (tmp1), "=&r" (tmp2), "=&r" (tmp3)
			     : "i" (ASI_PHYS_BYPASS_EC_E), "1" (addr));

	return ret;
}

static unsigned long __hbird_read_compare(void)
{
	unsigned long low, high;
	unsigned long addr = HBIRD_STICKCMP_ADDR;

	__asm__ __volatile__("ldxa	[%2] %3, %0\n\t"
			     "add	%2, 0x8, %2\n\t"
			     "ldxa	[%2] %3, %1"
			     : "=&r" (low), "=&r" (high), "=&r" (addr)
			     : "i" (ASI_PHYS_BYPASS_EC_E), "2" (addr));

	return (high << 32UL) | low;
}

static void __hbird_write_stick(unsigned long val)
{
	unsigned long low = (val & 0xffffffffUL);
	unsigned long high = (val >> 32UL);
	unsigned long addr = HBIRD_STICK_ADDR;

	__asm__ __volatile__("stxa	%%g0, [%0] %4\n\t"
			     "add	%0, 0x8, %0\n\t"
			     "stxa	%3, [%0] %4\n\t"
			     "sub	%0, 0x8, %0\n\t"
			     "stxa	%2, [%0] %4"
			     : "=&r" (addr)
			     : "0" (addr), "r" (low), "r" (high),
			       "i" (ASI_PHYS_BYPASS_EC_E));
}

static void __hbird_write_compare(unsigned long val)
{
	unsigned long low = (val & 0xffffffffUL);
	unsigned long high = (val >> 32UL);
	unsigned long addr = HBIRD_STICKCMP_ADDR + 0x8UL;

	__asm__ __volatile__("stxa	%3, [%0] %4\n\t"
			     "sub	%0, 0x8, %0\n\t"
			     "stxa	%2, [%0] %4"
			     : "=&r" (addr)
			     : "0" (addr), "r" (low), "r" (high),
			       "i" (ASI_PHYS_BYPASS_EC_E));
}

static void hbtick_init_tick(unsigned long offset)
{
	unsigned long val;

	tick_disable_protection();

	/* XXX This seems to be necessary to 'jumpstart' Hummingbird
	 * XXX into actually sending STICK interrupts.  I think because
	 * XXX of how we store %tick_cmpr in head.S this somehow resets the
	 * XXX {TICK + STICK} interrupt mux.  -DaveM
	 */
	__hbird_write_stick(__hbird_read_stick());

	val = __hbird_read_stick() & ~TICK_PRIV_BIT;
	__hbird_write_compare(val + offset);
}

static unsigned long hbtick_get_tick(void)
{
	return __hbird_read_stick() & ~TICK_PRIV_BIT;
}

static unsigned long hbtick_get_compare(void)
{
	return __hbird_read_compare();
}

static unsigned long hbtick_add_tick(unsigned long adj, unsigned long offset)
{
	unsigned long val;

	val = __hbird_read_stick() + adj;
	__hbird_write_stick(val);

	val &= ~TICK_PRIV_BIT;
	__hbird_write_compare(val + offset);

	return val;
}

static unsigned long hbtick_add_compare(unsigned long adj)
{
	unsigned long val = __hbird_read_compare() + adj;

	val &= ~TICK_PRIV_BIT;
	__hbird_write_compare(val);

	return val;
}

static struct sparc64_tick_ops hbtick_operations = {
	.init_tick	=	hbtick_init_tick,
	.get_tick	=	hbtick_get_tick,
	.get_compare	=	hbtick_get_compare,
	.add_tick	=	hbtick_add_tick,
	.add_compare	=	hbtick_add_compare,
	.softint_mask	=	1UL << 0,
};

/* timer_interrupt() needs to keep up the real-time clock,
 * as well as call the "do_timer()" routine every clocktick
 *
 * NOTE: On SUN5 systems the ticker interrupt comes in using 2
 *       interrupts, one at level14 and one with softint bit 0.
 */
unsigned long timer_tick_offset;
unsigned long timer_tick_compare;
unsigned long timer_ticks_per_usec_quotient;

static __inline__ void timer_check_rtc(void)
{
	/* last time the cmos clock got updated */
	static long last_rtc_update;

	/* Determine when to update the Mostek clock. */
	if ((time_status & STA_UNSYNC) == 0 &&
	    xtime.tv_sec > last_rtc_update + 660 &&
	    xtime.tv_usec >= 500000 - ((unsigned) tick) / 2 &&
	    xtime.tv_usec <= 500000 + ((unsigned) tick) / 2) {
		if (set_rtc_mmss(xtime.tv_sec) == 0)
			last_rtc_update = xtime.tv_sec;
		else
			last_rtc_update = xtime.tv_sec - 600;
			/* do it again in 60 s */
	}
}

void sparc64_do_profile(unsigned long pc, unsigned long o7)
{
	if (prof_buffer && current->pid) {
		extern int _stext;
		extern int rwlock_impl_begin, rwlock_impl_end;
		extern int atomic_impl_begin, atomic_impl_end;
		extern int __memcpy_begin, __memcpy_end;
		extern int __bzero_begin, __bzero_end;
		extern int __bitops_begin, __bitops_end;

		if ((pc >= (unsigned long) &atomic_impl_begin &&
		     pc < (unsigned long) &atomic_impl_end) ||
		    (pc >= (unsigned long) &rwlock_impl_begin &&
		     pc < (unsigned long) &rwlock_impl_end) ||
		    (pc >= (unsigned long) &__memcpy_begin &&
		     pc < (unsigned long) &__memcpy_end) ||
		    (pc >= (unsigned long) &__bzero_begin &&
		     pc < (unsigned long) &__bzero_end) ||
		    (pc >= (unsigned long) &__bitops_begin &&
		     pc < (unsigned long) &__bitops_end))
			pc = o7;

		pc -= (unsigned long) &_stext;
		pc >>= prof_shift;

		if(pc >= prof_len)
			pc = prof_len - 1;
		atomic_inc((atomic_t *)&prof_buffer[pc]);
	}
}

static void timer_interrupt(int irq, void *dev_id, struct pt_regs * regs)
{
	unsigned long ticks, pstate;

	write_lock(&xtime_lock);

	do {
#ifndef CONFIG_SMP
		if ((regs->tstate & TSTATE_PRIV) != 0)
			sparc64_do_profile(regs->tpc, regs->u_regs[UREG_RETPC]);
#endif
		do_timer(regs);

		/* Guarentee that the following sequences execute
		 * uninterrupted.
		 */
		__asm__ __volatile__("rdpr	%%pstate, %0\n\t"
				     "wrpr	%0, %1, %%pstate"
				     : "=r" (pstate)
				     : "i" (PSTATE_IE));

		timer_tick_compare = tick_ops->add_compare(timer_tick_offset);
		ticks = tick_ops->get_tick();

		/* Restore PSTATE_IE. */
		__asm__ __volatile__("wrpr	%0, 0x0, %%pstate"
				     : /* no outputs */
				     : "r" (pstate));
	} while (time_after_eq(ticks, timer_tick_compare));

	timer_check_rtc();

	write_unlock(&xtime_lock);
}

#ifdef CONFIG_SMP
void timer_tick_interrupt(struct pt_regs *regs)
{
	write_lock(&xtime_lock);

	do_timer(regs);

	/*
	 * Only keep timer_tick_offset uptodate, but don't set TICK_CMPR.
	 */
	timer_tick_compare = tick_ops->get_compare() + timer_tick_offset;

	timer_check_rtc();

	write_unlock(&xtime_lock);
}
#endif

/* Kick start a stopped clock (procedure from the Sun NVRAM/hostid FAQ). */
static void __init kick_start_clock(void)
{
	unsigned long regs = mstk48t02_regs;
	u8 sec, tmp;
	int i, count;

	prom_printf("CLOCK: Clock was stopped. Kick start ");

	spin_lock_irq(&mostek_lock);

	/* Turn on the kick start bit to start the oscillator. */
	tmp = mostek_read(regs + MOSTEK_CREG);
	tmp |= MSTK_CREG_WRITE;
	mostek_write(regs + MOSTEK_CREG, tmp);
	tmp = mostek_read(regs + MOSTEK_SEC);
	tmp &= ~MSTK_STOP;
	mostek_write(regs + MOSTEK_SEC, tmp);
	tmp = mostek_read(regs + MOSTEK_HOUR);
	tmp |= MSTK_KICK_START;
	mostek_write(regs + MOSTEK_HOUR, tmp);
	tmp = mostek_read(regs + MOSTEK_CREG);
	tmp &= ~MSTK_CREG_WRITE;
	mostek_write(regs + MOSTEK_CREG, tmp);

	spin_unlock_irq(&mostek_lock);

	/* Delay to allow the clock oscillator to start. */
	sec = MSTK_REG_SEC(regs);
	for (i = 0; i < 3; i++) {
		while (sec == MSTK_REG_SEC(regs))
			for (count = 0; count < 100000; count++)
				/* nothing */ ;
		prom_printf(".");
		sec = MSTK_REG_SEC(regs);
	}
	prom_printf("\n");

	spin_lock_irq(&mostek_lock);

	/* Turn off kick start and set a "valid" time and date. */
	tmp = mostek_read(regs + MOSTEK_CREG);
	tmp |= MSTK_CREG_WRITE;
	mostek_write(regs + MOSTEK_CREG, tmp);
	tmp = mostek_read(regs + MOSTEK_HOUR);
	tmp &= ~MSTK_KICK_START;
	mostek_write(regs + MOSTEK_HOUR, tmp);
	MSTK_SET_REG_SEC(regs,0);
	MSTK_SET_REG_MIN(regs,0);
	MSTK_SET_REG_HOUR(regs,0);
	MSTK_SET_REG_DOW(regs,5);
	MSTK_SET_REG_DOM(regs,1);
	MSTK_SET_REG_MONTH(regs,8);
	MSTK_SET_REG_YEAR(regs,1996 - MSTK_YEAR_ZERO);
	tmp = mostek_read(regs + MOSTEK_CREG);
	tmp &= ~MSTK_CREG_WRITE;
	mostek_write(regs + MOSTEK_CREG, tmp);

	spin_unlock_irq(&mostek_lock);

	/* Ensure the kick start bit is off. If it isn't, turn it off. */
	while (mostek_read(regs + MOSTEK_HOUR) & MSTK_KICK_START) {
		prom_printf("CLOCK: Kick start still on!\n");

		spin_lock_irq(&mostek_lock);

		tmp = mostek_read(regs + MOSTEK_CREG);
		tmp |= MSTK_CREG_WRITE;
		mostek_write(regs + MOSTEK_CREG, tmp);

		tmp = mostek_read(regs + MOSTEK_HOUR);
		tmp &= ~MSTK_KICK_START;
		mostek_write(regs + MOSTEK_HOUR, tmp);

		tmp = mostek_read(regs + MOSTEK_CREG);
		tmp &= ~MSTK_CREG_WRITE;
		mostek_write(regs + MOSTEK_CREG, tmp);

		spin_unlock_irq(&mostek_lock);
	}

	prom_printf("CLOCK: Kick start procedure successful.\n");
}

/* Return nonzero if the clock chip battery is low. */
static int __init has_low_battery(void)
{
	unsigned long regs = mstk48t02_regs;
	u8 data1, data2;

	spin_lock_irq(&mostek_lock);

	data1 = mostek_read(regs + MOSTEK_EEPROM);	/* Read some data. */
	mostek_write(regs + MOSTEK_EEPROM, ~data1);	/* Write back the complement. */
	data2 = mostek_read(regs + MOSTEK_EEPROM);	/* Read back the complement. */
	mostek_write(regs + MOSTEK_EEPROM, data1);	/* Restore original value. */

	spin_unlock_irq(&mostek_lock);

	return (data1 == data2);	/* Was the write blocked? */
}

#ifndef BCD_TO_BIN
#define BCD_TO_BIN(val) (((val)&15) + ((val)>>4)*10)
#endif

#ifndef BIN_TO_BCD
#define BIN_TO_BCD(val) ((((val)/10)<<4) + (val)%10)
#endif

/* Probe for the real time clock chip. */
static void __init set_system_time(void)
{
	unsigned int year, mon, day, hour, min, sec;
	unsigned long mregs = mstk48t02_regs;
#ifdef CONFIG_PCI
	unsigned long dregs = ds1287_regs;
#else
	unsigned long dregs = 0UL;
#endif
	u8 tmp;

	if (!mregs && !dregs) {
		prom_printf("Something wrong, clock regs not mapped yet.\n");
		prom_halt();
	}		

	if (mregs) {
		spin_lock_irq(&mostek_lock);

		/* Traditional Mostek chip. */
		tmp = mostek_read(mregs + MOSTEK_CREG);
		tmp |= MSTK_CREG_READ;
		mostek_write(mregs + MOSTEK_CREG, tmp);

		sec = MSTK_REG_SEC(mregs);
		min = MSTK_REG_MIN(mregs);
		hour = MSTK_REG_HOUR(mregs);
		day = MSTK_REG_DOM(mregs);
		mon = MSTK_REG_MONTH(mregs);
		year = MSTK_CVT_YEAR( MSTK_REG_YEAR(mregs) );
	} else {
		int i;

		/* Dallas 12887 RTC chip. */

		/* Stolen from arch/i386/kernel/time.c, see there for
		 * credits and descriptive comments.
		 */
		for (i = 0; i < 1000000; i++) {
			if (CMOS_READ(RTC_FREQ_SELECT) & RTC_UIP)
				break;
			udelay(10);
		}
		for (i = 0; i < 1000000; i++) {
			if (!(CMOS_READ(RTC_FREQ_SELECT) & RTC_UIP))
				break;
			udelay(10);
		}
		do {
			sec  = CMOS_READ(RTC_SECONDS);
			min  = CMOS_READ(RTC_MINUTES);
			hour = CMOS_READ(RTC_HOURS);
			day  = CMOS_READ(RTC_DAY_OF_MONTH);
			mon  = CMOS_READ(RTC_MONTH);
			year = CMOS_READ(RTC_YEAR);
		} while (sec != CMOS_READ(RTC_SECONDS));
		if (!(CMOS_READ(RTC_CONTROL) & RTC_DM_BINARY) || RTC_ALWAYS_BCD) {
			BCD_TO_BIN(sec);
			BCD_TO_BIN(min);
			BCD_TO_BIN(hour);
			BCD_TO_BIN(day);
			BCD_TO_BIN(mon);
			BCD_TO_BIN(year);
		}
		if ((year += 1900) < 1970)
			year += 100;
	}

	xtime.tv_sec = mktime(year, mon, day, hour, min, sec);
	xtime.tv_usec = 0;

	if (mregs) {
		tmp = mostek_read(mregs + MOSTEK_CREG);
		tmp &= ~MSTK_CREG_READ;
		mostek_write(mregs + MOSTEK_CREG, tmp);

		spin_unlock_irq(&mostek_lock);
	}
}

void __init clock_probe(void)
{
	struct linux_prom_registers clk_reg[2];
	char model[128];
	int node, busnd = -1, err;
	unsigned long flags;
	struct linux_central *cbus;
#ifdef CONFIG_PCI
	struct linux_ebus *ebus = NULL;
	struct isa_bridge *isa_br = NULL;
#endif
	static int invoked;

	if (invoked)
		return;
	invoked = 1;


	if (this_is_starfire) {
		/* davem suggests we keep this within the 4M locked kernel image */
		static char obp_gettod[256];
		static u32 unix_tod;

		sprintf(obp_gettod, "h# %08x unix-gettod",
			(unsigned int) (long) &unix_tod);
		prom_feval(obp_gettod);
		xtime.tv_sec = unix_tod;
		xtime.tv_usec = 0;
		return;
	}

	__save_and_cli(flags);

	cbus = central_bus;
	if (cbus != NULL)
		busnd = central_bus->child->prom_node;

	/* Check FHC Central then EBUSs then ISA bridges then SBUSs.
	 * That way we handle the presence of multiple properly.
	 *
	 * As a special case, machines with Central must provide the
	 * timer chip there.
	 */
#ifdef CONFIG_PCI
	if (ebus_chain != NULL) {
		ebus = ebus_chain;
		if (busnd == -1)
			busnd = ebus->prom_node;
	}
	if (isa_chain != NULL) {
		isa_br = isa_chain;
		if (busnd == -1)
			busnd = isa_br->prom_node;
	}
#endif
	if (sbus_root != NULL && busnd == -1)
		busnd = sbus_root->prom_node;

	if (busnd == -1) {
		prom_printf("clock_probe: problem, cannot find bus to search.\n");
		prom_halt();
	}

	node = prom_getchild(busnd);

	while (1) {
		if (!node)
			model[0] = 0;
		else
			prom_getstring(node, "model", model, sizeof(model));
		if (strcmp(model, "mk48t02") &&
		    strcmp(model, "mk48t08") &&
		    strcmp(model, "mk48t59") &&
		    strcmp(model, "m5819") &&
		    strcmp(model, "m5819p") &&
		    strcmp(model, "ds1287")) {
			if (cbus != NULL) {
				prom_printf("clock_probe: Central bus lacks timer chip.\n");
				prom_halt();
			}

		   	if (node != 0)
				node = prom_getsibling(node);
#ifdef CONFIG_PCI
			while ((node == 0) && ebus != NULL) {
				ebus = ebus->next;
				if (ebus != NULL) {
					busnd = ebus->prom_node;
					node = prom_getchild(busnd);
				}
			}
			while ((node == 0) && isa_br != NULL) {
				isa_br = isa_br->next;
				if (isa_br != NULL) {
					busnd = isa_br->prom_node;
					node = prom_getchild(busnd);
				}
			}
#endif
			if (node == 0) {
				prom_printf("clock_probe: Cannot find timer chip\n");
				prom_halt();
			}
			continue;
		}

		err = prom_getproperty(node, "reg", (char *)clk_reg,
				       sizeof(clk_reg));
		if(err == -1) {
			prom_printf("clock_probe: Cannot get Mostek reg property\n");
			prom_halt();
		}

		if (cbus != NULL) {
			apply_fhc_ranges(central_bus->child, clk_reg, 1);
			apply_central_ranges(central_bus, clk_reg, 1);
		}
#ifdef CONFIG_PCI
		else if (ebus != NULL) {
			struct linux_ebus_device *edev;

			for_each_ebusdev(edev, ebus)
				if (edev->prom_node == node)
					break;
			if (edev == NULL) {
				if (isa_chain != NULL)
					goto try_isa_clock;
				prom_printf("%s: Mostek not probed by EBUS\n",
					    __FUNCTION__);
				prom_halt();
			}

			if (!strcmp(model, "ds1287") ||
			    !strcmp(model, "m5819") ||
			    !strcmp(model, "m5819p")) {
				ds1287_regs = edev->resource[0].start;
			} else {
				mstk48t59_regs = edev->resource[0].start;
				mstk48t02_regs = mstk48t59_regs + MOSTEK_48T59_48T02;
			}
			break;
		}
		else if (isa_br != NULL) {
			struct isa_device *isadev;

try_isa_clock:
			for_each_isadev(isadev, isa_br)
				if (isadev->prom_node == node)
					break;
			if (isadev == NULL) {
				prom_printf("%s: Mostek not probed by ISA\n");
				prom_halt();
			}
			if (!strcmp(model, "ds1287") ||
			    !strcmp(model, "m5819") ||
			    !strcmp(model, "m5819p")) {
				ds1287_regs = isadev->resource.start;
			} else {
				mstk48t59_regs = isadev->resource.start;
				mstk48t02_regs = mstk48t59_regs + MOSTEK_48T59_48T02;
			}
			break;
		}
#endif
		else {
			if (sbus_root->num_sbus_ranges) {
				int nranges = sbus_root->num_sbus_ranges;
				int rngc;

				for (rngc = 0; rngc < nranges; rngc++)
					if (clk_reg[0].which_io ==
					    sbus_root->sbus_ranges[rngc].ot_child_space)
						break;
				if (rngc == nranges) {
					prom_printf("clock_probe: Cannot find ranges for "
						    "clock regs.\n");
					prom_halt();
				}
				clk_reg[0].which_io =
					sbus_root->sbus_ranges[rngc].ot_parent_space;
				clk_reg[0].phys_addr +=
					sbus_root->sbus_ranges[rngc].ot_parent_base;
			}
		}

		if(model[5] == '0' && model[6] == '2') {
			mstk48t02_regs = (((u64)clk_reg[0].phys_addr) |
					  (((u64)clk_reg[0].which_io)<<32UL));
		} else if(model[5] == '0' && model[6] == '8') {
			mstk48t08_regs = (((u64)clk_reg[0].phys_addr) |
					  (((u64)clk_reg[0].which_io)<<32UL));
			mstk48t02_regs = mstk48t08_regs + MOSTEK_48T08_48T02;
		} else {
			mstk48t59_regs = (((u64)clk_reg[0].phys_addr) |
					  (((u64)clk_reg[0].which_io)<<32UL));
			mstk48t02_regs = mstk48t59_regs + MOSTEK_48T59_48T02;
		}
		break;
	}

	if (mstk48t02_regs != 0UL) {
		/* Report a low battery voltage condition. */
		if (has_low_battery())
			prom_printf("NVRAM: Low battery voltage!\n");

		/* Kick start the clock if it is completely stopped. */
		if (mostek_read(mstk48t02_regs + MOSTEK_SEC) & MSTK_STOP)
			kick_start_clock();
	}

	set_system_time();
	
	__restore_flags(flags);
}

/* This is gets the master TICK_INT timer going. */
static unsigned long sparc64_init_timers(void (*cfunc)(int, void *, struct pt_regs *))
{
	unsigned long pstate, clock;
	int node, err;
#ifdef CONFIG_SMP
	extern void smp_tick_init(void);
#endif

	if (tlb_type == spitfire) {
		unsigned long ver, manuf, impl;

		__asm__ __volatile__ ("rdpr %%ver, %0"
				      : "=&r" (ver));
		manuf = ((ver >> 48) & 0xffff);
		impl = ((ver >> 32) & 0xffff);
		if (manuf == 0x17 && impl == 0x13) {
			/* Hummingbird, aka Ultra-IIe */
			tick_ops = &hbtick_operations;
			node = prom_root_node;
			clock = prom_getint(node, "stick-frequency");
		} else {
			tick_ops = &tick_operations;
			node = linux_cpus[0].prom_node;
			clock = prom_getint(node, "clock-frequency");
		}
	} else {
		tick_ops = &stick_operations;
		node = prom_root_node;
		clock = prom_getint(node, "stick-frequency");
	}
	timer_tick_offset = clock / HZ;

#ifdef CONFIG_SMP
	smp_tick_init();
#endif

	/* Register IRQ handler. */
	err = request_irq(build_irq(0, 0, 0UL, 0UL), cfunc, SA_STATIC_ALLOC,
			  "timer", NULL);

	if(err) {
		prom_printf("Serious problem, cannot register TICK_INT\n");
		prom_halt();
	}

	/* Guarentee that the following sequences execute
	 * uninterrupted.
	 */
	__asm__ __volatile__("rdpr	%%pstate, %0\n\t"
			     "wrpr	%0, %1, %%pstate"
			     : "=r" (pstate)
			     : "i" (PSTATE_IE));

	tick_ops->init_tick(timer_tick_offset);

	/* Restore PSTATE_IE. */
	__asm__ __volatile__("wrpr	%0, 0x0, %%pstate"
			     : /* no outputs */
			     : "r" (pstate));

	sti();

	return clock;
}

/* The quotient formula is taken from the IA64 port. */
void __init time_init(void)
{
	unsigned long clock = sparc64_init_timers(timer_interrupt);

	timer_ticks_per_usec_quotient =
		(((1000000UL << 30) +
		  (clock / 2)) / clock);
}

static __inline__ unsigned long do_gettimeoffset(void)
{
	unsigned long ticks = tick_ops->get_tick();

	ticks += timer_tick_offset;
	ticks -= timer_tick_compare;

	return (ticks * timer_ticks_per_usec_quotient) >> 30UL;
}

void do_settimeofday(struct timeval *tv)
{
	if (this_is_starfire)
		return;

	write_lock_irq(&xtime_lock);

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

static int set_rtc_mmss(unsigned long nowtime)
{
	int real_seconds, real_minutes, chip_minutes;
	unsigned long mregs = mstk48t02_regs;
#ifdef CONFIG_PCI
	unsigned long dregs = ds1287_regs;
#else
	unsigned long dregs = 0UL;
#endif
	unsigned long flags;
	u8 tmp;

	/* 
	 * Not having a register set can lead to trouble.
	 * Also starfire doesn't have a tod clock.
	 */
	if (!mregs && !dregs) 
		return -1;

	if (mregs) {
		spin_lock_irqsave(&mostek_lock, flags);

		/* Read the current RTC minutes. */
		tmp = mostek_read(mregs + MOSTEK_CREG);
		tmp |= MSTK_CREG_READ;
		mostek_write(mregs + MOSTEK_CREG, tmp);

		chip_minutes = MSTK_REG_MIN(mregs);

		tmp = mostek_read(mregs + MOSTEK_CREG);
		tmp &= ~MSTK_CREG_READ;
		mostek_write(mregs + MOSTEK_CREG, tmp);

		/*
		 * since we're only adjusting minutes and seconds,
		 * don't interfere with hour overflow. This avoids
		 * messing with unknown time zones but requires your
		 * RTC not to be off by more than 15 minutes
		 */
		real_seconds = nowtime % 60;
		real_minutes = nowtime / 60;
		if (((abs(real_minutes - chip_minutes) + 15)/30) & 1)
			real_minutes += 30;	/* correct for half hour time zone */
		real_minutes %= 60;

		if (abs(real_minutes - chip_minutes) < 30) {
			tmp = mostek_read(mregs + MOSTEK_CREG);
			tmp |= MSTK_CREG_WRITE;
			mostek_write(mregs + MOSTEK_CREG, tmp);

			MSTK_SET_REG_SEC(mregs,real_seconds);
			MSTK_SET_REG_MIN(mregs,real_minutes);

			tmp = mostek_read(mregs + MOSTEK_CREG);
			tmp &= ~MSTK_CREG_WRITE;
			mostek_write(mregs + MOSTEK_CREG, tmp);

			spin_unlock_irqrestore(&mostek_lock, flags);

			return 0;
		} else {
			spin_unlock_irqrestore(&mostek_lock, flags);

			return -1;
		}
	} else {
		int retval = 0;
		unsigned char save_control, save_freq_select;

		/* Stolen from arch/i386/kernel/time.c, see there for
		 * credits and descriptive comments.
		 */
		spin_lock_irqsave(&rtc_lock, flags);
		save_control = CMOS_READ(RTC_CONTROL); /* tell the clock it's being set */
		CMOS_WRITE((save_control|RTC_SET), RTC_CONTROL);

		save_freq_select = CMOS_READ(RTC_FREQ_SELECT); /* stop and reset prescaler */
		CMOS_WRITE((save_freq_select|RTC_DIV_RESET2), RTC_FREQ_SELECT);

		chip_minutes = CMOS_READ(RTC_MINUTES);
		if (!(save_control & RTC_DM_BINARY) || RTC_ALWAYS_BCD)
			BCD_TO_BIN(chip_minutes);
		real_seconds = nowtime % 60;
		real_minutes = nowtime / 60;
		if (((abs(real_minutes - chip_minutes) + 15)/30) & 1)
			real_minutes += 30;
		real_minutes %= 60;

		if (abs(real_minutes - chip_minutes) < 30) {
			if (!(save_control & RTC_DM_BINARY) || RTC_ALWAYS_BCD) {
				BIN_TO_BCD(real_seconds);
				BIN_TO_BCD(real_minutes);
			}
			CMOS_WRITE(real_seconds,RTC_SECONDS);
			CMOS_WRITE(real_minutes,RTC_MINUTES);
		} else {
			printk(KERN_WARNING
			       "set_rtc_mmss: can't update from %d to %d\n",
			       chip_minutes, real_minutes);
			retval = -1;
		}

		CMOS_WRITE(save_control, RTC_CONTROL);
		CMOS_WRITE(save_freq_select, RTC_FREQ_SELECT);
		spin_unlock_irqrestore(&rtc_lock, flags);

		return retval;
	}
}
