/*
 * Copyright (c) 1996, by Steve Passe
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the developer may NOT be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: mpapic.c,v 1.1 1997/04/26 11:45:15 peter Exp $
 */

#include <sys/types.h>
#include <sys/sysproto.h>
#include <sys/systm.h>

#include <machine/smp.h>
#include <machine/mpapic.h>
#include <machine/smptests.h>	/** TEST_LOPRIO, TEST_IPI */
#include <machine/cpufunc.h>
#include <machine/segments.h>


/* EISA Edge/Level trigger control registers */
#define ELCR0	0x4d0		/* eisa irq 0-7 */
#define ELCR1	0x4d1		/* eisa irq 8-15 */

/*
 * pointers to pmapped apic hardware.
 */
volatile u_int *apic_base;

#if defined(APIC_IO)
volatile u_int *io_apic_base;
#endif	/* APIC_IO */

/*
 * enable APIC, configure interrupts
 */
void
apic_initialize(int is_bsp)
{
	u_int   temp;

	if (is_bsp) {
		/* setup LVT1 as ExtINT */
		temp = apic_base[APIC_LVT1];
		temp &= 0xfffe58ff;
		temp |= 0x00000700;
		apic_base[APIC_LVT1] = temp;
	}
	/* setup LVT2 as NMI */
	temp = apic_base[APIC_LVT2];
	temp &= 0xfffe58ff;
	temp |= 0xffff0400;
	apic_base[APIC_LVT2] = temp;

	/* set the Task Priority Register as needed */
	temp = apic_base[APIC_TPR];
	temp &= ~APIC_TPR_PRIO;	/* clear priority field */

#if defined(TEST_LOPRIO)
	if (is_bsp)
		temp |= 0x10;	/* allow INT arbitration */
	else
		temp |= 0xff;	/* disallow INT arbitration */
#endif	/* TEST_LOPRIO */

	apic_base[APIC_TPR] = temp;

	/* enable the beast */
	temp = apic_base[APIC_SVR];
	temp |= APIC_SVR_SWEN;	/* software enable APIC */
	temp &= ~APIC_SVR_FOCUS;/* enable 'focus processor' */
#if 0
	temp |= 0x20;		/** FIXME: 2f == strayIRQ15 */
#endif
	apic_base[APIC_SVR] = temp;
}


#if !defined(FAST_IPI)

/*
 * send an IPI INTerrupt containing 'vector' to CPUs in 'target_map'
 * 'target_map' is a bitfiled of length 14,
 *   APIC #0 == bit 0, ..., APIC #14 == bit 14
 *   NOTE: these are LOGICAL APIC IDs
 */
int
selected_procs_ipi(int target_map, int vector)
{
	return selected_apic_ipi(target_map, vector, APIC_DELMODE_FIXED);
}

/*
 * send an IPI INTerrupt containing 'vector' to all CPUs, including myself
 */
int
all_procs_ipi(int vector)
{
	return apic_ipi(APIC_DEST_ALLISELF, vector, APIC_DELMODE_FIXED);
}

/*
 * send an IPI INTerrupt containing 'vector' to all CPUs EXCEPT myself
 */
int
all_but_self_ipi(int vector)
{
	return apic_ipi(APIC_DEST_ALLESELF, vector, APIC_DELMODE_FIXED);
}

/*
 * send an IPI INTerrupt containing 'vector' to myself
 */
int
self_ipi(int vector)
{
	return apic_ipi(APIC_DEST_SELF, vector, APIC_DELMODE_FIXED);
}

/*
 * send APIC IPI 'vector' to 'destType' via 'deliveryMode'
 *
 *  destType is 1 of: APIC_DEST_SELF, APIC_DEST_ALLISELF, APIC_DEST_ALLESELF
 *  vector is any valid SYSTEM INT vector
 *  delivery_mode is 1 of: APIC_DELMODE_FIXED, APIC_DELMODE_LOWPRIO
 */
int
apic_ipi(int dest_type, int vector, int delivery_mode)
{
#if defined(APICIPI_BANDAID)
#define MAX_SPIN1	10000000
#define MAX_SPIN2	1000

	u_long  icr_lo;
	int     x;

	/* "lazy delivery", ie we only barf if they stack up on us... */
	for (x = MAX_SPIN1; x; --x) {
		if ((apic_base[APIC_ICR_LOW] & APIC_DELSTAT_MASK) == 0)
			break;
	}
	if (x == 0) {
		printf("apic_ipi was stuck\n");
		panic("\n");
	}
	/* build IRC_LOW */
	icr_lo = (apic_base[APIC_ICR_LOW] & APIC_RESV2_MASK)
	    | dest_type | delivery_mode | vector;

	/* write APIC ICR */
	apic_base[APIC_ICR_LOW] = icr_lo;

	/* wait for pending status end */
	for (x = MAX_SPIN2; x; --x) {
		if ((apic_base[APIC_ICR_LOW] & APIC_DELSTAT_MASK) == 0)
			break;
	}
	if (x == 0)
		printf("apic_ipi might be stuck\n");

	/** FIXME: return result */
	return 0;
#else				/* APICIPI_BANDAID */
	u_long  icr_lo;

	/* build IRC_LOW */
	icr_lo = (apic_base[APIC_ICR_LOW] & APIC_RESV2_MASK)
	    | dest_type | delivery_mode | vector;

	/* write APIC ICR */
	apic_base[APIC_ICR_LOW] = icr_lo;

	/* wait for pending status end */
	while (apic_base[APIC_ICR_LOW] & APIC_DELSTAT_MASK)
		 /* spin */ ;

	/** FIXME: return result */
	return 0;
#endif	/* APICIPI_BANDAID */
}
#endif	/* FAST_IPI */


/*
 * send APIC IPI 'vector' to 'target's via 'delivery_mode'
 *
 *  target contains a bitfield with a bit set for selected APICs.
 *  vector is any valid SYSTEM INT vector
 *  delivery_mode is 1 of: APIC_DELMODE_FIXED, APIC_DELMODE_LOWPRIO
 */
int
selected_apic_ipi(u_int target, int vector, int delivery_mode)
{
	int     x;
	int     status;
	u_long  icr_hi;

	if (target & ~0x7fff)
		return -1;	/* only 15 targets allowed */

	for (status = 0, x = 0; x <= 14; ++x)
		if (target & (1 << x)) {
			/* write the destination field for the target AP */
			icr_hi = apic_base[APIC_ICR_HI] & ~APIC_ID_MASK;
			icr_hi |= (CPU_TO_ID(x) << 24);
			apic_base[APIC_ICR_HI] = icr_hi;

			/* send the IPI */
			if (apic_ipi(APIC_DEST_DESTFLD, vector, delivery_mode) == -1)
				status |= (1 << x);
		}
	return status;
}


#if defined(APIC_IO)

/*
 * IO APIC code,
 */

#define IOAPIC_ISA_INTS		16
#define REDIRCNT_IOAPIC(A) \
	    ((int)((io_apic_versions[(A)] & IOART_VER_MAXREDIR) >> MAXREDIRSHIFT) + 1)

static int trigger __P((int apic, int pin, u_int32_t * flags));
static void polarity __P((int apic, int pin, u_int32_t * flags, int level));


/*
 * setup I/O APIC
 */

#ifdef HOW_8259_IS_PROGRAMMED
outb(IO_ICU1, 0x11);		/* reset; program device, four bytes */
outb(IO_ICU1 + 1, NRSVIDT);	/* starting at this vector index */
outb(IO_ICU1 + 1, 1 << 2);	/* slave on line 2 */
outb(IO_ICU1 + 1, 1);		/* 8086 mode */

outb(IO_ICU1 + 1, 0xff);	/* leave interrupts masked */
outb(IO_ICU1, 0x0a);		/* default to IRR on read */
outb(IO_ICU1, 0xc0 | (3 - 1));	/* pri order 3-7, 0-2 (com2 first) */

outb(IO_ICU2, 0x11);		/* reset; program device, four bytes */
outb(IO_ICU2 + 1, NRSVIDT + 8);	/* staring at this vector index */
outb(IO_ICU2 + 1, 2);		/* my slave id is 2 */
outb(IO_ICU2 + 1, 1);		/* 8086 mode */

outb(IO_ICU2 + 1, 0xff);	/* leave interrupts masked */
outb(IO_ICU2, 0x0a);		/* default to IRR on read */
#endif	/* HOW_8259_IS_PROGRAMMED */


#if defined(TEST_LOPRIO)
#define DEFAULT_FLAGS		\
	((u_int32_t)		\
	 (IOART_INTMSET |	\
	  IOART_DESTPHY |	\
	  IOART_DELLOPRI))

#define DEFAULT_ISA_FLAGS	\
	((u_int32_t)		\
	 (IOART_INTMSET |	\
	  IOART_TRGREDG |	\
	  IOART_INTAHI |	\
	  IOART_DESTPHY |	\
	  IOART_DELLOPRI))
#else
#define DEFAULT_FLAGS		\
	((u_int32_t)		\
	 (IOART_INTMSET |	\
	  IOART_DESTPHY |	\
	  IOART_DELFIXED))

#define DEFAULT_ISA_FLAGS	\
	((u_int32_t)		\
	 (IOART_INTMSET |	\
	  IOART_TRGREDG |	\
	  IOART_INTAHI |	\
	  IOART_DESTPHY |	\
	  IOART_DELFIXED))
#endif	/* TEST_LOPRIO */

/*
 *
 */
int
io_apic_setup(int apic)
{
	int     maxpin;
	u_char  select;		/* the select register is 8 bits */
	u_int32_t flags;	/* the window register is 32 bits */
	u_int32_t target;	/* the window register is 32 bits */
	u_int32_t vector;	/* the window register is 32 bits */
	int     pin, level;

#if defined(TEST_LOPRIO)
	target = IOART_DEST;
#else
	target = boot_cpu_id << 24;
#endif	/* TEST_LOPRIO */

	if (apic == 0) {
		/* APIC[0] INT0 thru INT15 default to ISA */
		select = IOAPIC_REDTBL0;
		vector = NRSVIDT;
		for (pin = 0;
		    pin < IOAPIC_ISA_INTS;
		    ++pin, ++vector, select += 2) {
			io_apic_write(apic, select, DEFAULT_ISA_FLAGS | vector);
			io_apic_write(apic, select + 1, target);
		}

		/* remainder of APIC[0] */
		if ((maxpin = REDIRCNT_IOAPIC(apic)) > IOAPIC_ISA_INTS) {
			select = IOAPIC_REDTBL16;
			vector = NRSVIDT + IOAPIC_ISA_INTS;
			for (pin = IOAPIC_ISA_INTS;
			    pin < maxpin;
			    ++pin, ++vector, select += 2) {

				if (apic_int_type(apic, pin) != 0)
					continue;

				flags = DEFAULT_FLAGS;
				level = trigger(apic, pin, &flags);
				polarity(apic, pin, &flags, level);

				io_apic_write(apic, select, flags | vector);
				io_apic_write(apic, select + 1, target);
			}
		}
	} else {

		/* program entry according to MP table. */
#if defined(MULTIPLE_IOAPICS)
#error MULTIPLE_IOAPICSXXX
#else
		panic("ioApicSetup: apic #%d\n", apic);
#endif	/* MULTIPLE_IOAPICS */

	}

	/* return GOOD status */
	return 0;
}
#undef DEFAULT_ISA_FLAGS
#undef DEFAULT_FLAGS


#define DEFAULT_EXTINT_FLAGS	\
	((u_int32_t)		\
	 (IOART_INTMSET |	\
	  IOART_TRGREDG |	\
	  IOART_INTAHI |	\
	  IOART_DESTPHY |	\
	  IOART_DELEXINT))

/*
 *
 */
int
ext_int_setup(int apic, int intr)
{
	u_char  select;		/* the select register is 8 bits */
	u_int32_t flags;	/* the window register is 32 bits */
	u_int32_t target;	/* the window register is 32 bits */
	u_int32_t vector;	/* the window register is 32 bits */

	if (apic_int_type(apic, intr) != 3)
		return -1;

/** FIXME: where should we steer ExtInts ??? */
#if defined(BROADCAST_EXTINT)
	target = IOART_DEST;
#else
	target = boot_cpu_id << 24;
#endif	/* BROADCAST_EXTINT */

	select = IOAPIC_REDTBL0 + (2 * intr);
	vector = NRSVIDT + intr;
	flags = DEFAULT_EXTINT_FLAGS;

	io_apic_write(apic, select, flags | vector);
	io_apic_write(apic, select + 1, target);

	return 0;
}
#undef DEFAULT_EXTINT_FLAGS


/*
 *
 */
static int
trigger(int apic, int pin, u_int32_t * flags)
{
	int     id;
	int     eirq;
	int     level;
	static int intcontrol = -1;

	switch (apic_trigger(apic, pin)) {

	case 0x00:
		break;

	case 0x01:
		*flags &= ~IOART_TRGRLVL;	/* *flags |= IOART_TRGREDG */
		return 0;

	case 0x03:
		*flags |= IOART_TRGRLVL;
		return 1;

	case -1:
	default:
		goto bad;
	}

	if ((id = apic_src_bus_id(apic, pin)) == -1)
		goto bad;

	switch (apic_bus_type(id)) {
	case ISA:
		*flags &= ~IOART_TRGRLVL;	/* *flags |= IOART_TRGREDG; */
		return 0;

	case EISA:
		eirq = apic_src_bus_irq(apic, pin);
		if (eirq < 0 || eirq > 15) {
			printf("EISA IRQ %d?!?!\n", eirq);
			goto bad;
		}
		if (intcontrol == -1) {
			intcontrol = inb(ELCR1) << 8;
			intcontrol |= inb(ELCR0);
			printf("EISA INTCONTROL = %08x\n", intcontrol);
		}
		/* EISA IRQ's are identical to ISA irq's, regardless of
		 * whether they are edge or level since they go through the
		 * level/polarity converter gadget. */
		level = 0;

		if (level)
			*flags |= IOART_TRGRLVL;
		else
			*flags &= ~IOART_TRGRLVL;

		return level;

	case PCI:
		*flags |= IOART_TRGRLVL;
		return 1;

	case -1:
	default:
		goto bad;
	}

bad:
	panic("bad APIC IO INT flags\n");
}


/*
 *
 */
static void
polarity(int apic, int pin, u_int32_t * flags, int level)
{
	int     id;
	int     eirq;
	int     pol;

	switch (apic_polarity(apic, pin)) {

	case 0x00:
		break;

	case 0x01:
		*flags &= ~IOART_INTALO;	/* *flags |= IOART_INTAHI */
		return;

	case 0x03:
		*flags |= IOART_INTALO;
		return;

	case -1:
	default:
		goto bad;
	}

	if ((id = apic_src_bus_id(apic, pin)) == -1)
		goto bad;

	switch (apic_bus_type(id)) {
	case ISA:
		*flags &= ~IOART_INTALO;	/* *flags |= IOART_INTAHI */
		return;

	case EISA:
		eirq = apic_src_bus_irq(apic, pin);
		if (eirq < 0 || eirq > 15) {
			printf("EISA POL: IRQ %d??\n", eirq);
			goto bad;
		}
		/* XXX EISA IRQ's are identical to ISA irq's, regardless of
		 * whether they are edge or level since they go through the
		 * level/polarity converter gadget. */

		if (level == 1)	/* XXX Always false */
			pol = 0;/* If level, active low */
		else
			pol = 1;/* If edge, high edge */

		if (pol == 0)
			*flags |= IOART_INTALO;
		else
			*flags &= ~IOART_INTALO;

		return;

	case PCI:
		*flags |= IOART_INTALO;
		return;

	case -1:
	default:
		goto bad;
	}

bad:
	panic("bad APIC IO INT flags\n");
}

/*
 * set INT mask bit for each bit set in 'mask'.
 * clear INT mask bit for all others.
 * only consider lower 24 bits in mask.
 */
#define IO_MASK		(imen & 0x00ffffff)
#define IO_FIELD	0x00ffffff

void
write_io_apic_mask24(int apic, u_int32_t mask)
{
	int     x, y;
	u_char  select;		/* the select register is 8 bits */
	u_int32_t low_reg;	/* the window register is 32 bits */
	u_int32_t diffs;

	mask &= IO_FIELD;	/* safety valve, only use 24 bits */
	if (mask == IO_MASK)	/* check for same value as current */
		return;

	diffs = mask ^ IO_MASK;	/* record differences */

	for (x = 0, y = REDIRCNT_IOAPIC(apic); x < y; ++x) {
		if (!(diffs & (1 << x)))
			continue;	/* no change, skip */

		select = IOAPIC_REDTBL + (x * 2);	/* calculate addr */
		low_reg = io_apic_read(apic, select);	/* read contents */

		if (mask & (1 << x))
			low_reg |= IOART_INTMASK;	/* set mask */
		else
			low_reg &= ~IOART_INTMASK;	/* clear mask */

		io_apic_write(apic, select, low_reg);	/* new value */
	}
}


#if defined(READY)
/*
 * set INT mask bit for each bit set in 'mask'.
 * ignore INT mask bit for all others.
 * only consider lower 24 bits in mask.
 */
void
set_io_apic_mask24(apic, u_int32_t bits)
{
	int     x, y;
	u_char  select;		/* the select register is 8 bits */
	u_int32_t low_reg;	/* the window register is 32 bits */
	u_int32_t diffs;

	bits &= IO_FIELD;	/* safety valve, only use 24 bits */
	diffs = bits & ~IO_MASK;/* clear AND needing 'set'ing */
	if (!diffs)
		return;

#error imen/IOApicMask NOT merged in setIOApicMask24()

	for (x = 0, y = REDIRCNT_IOAPIC(0); x < y; ++x) {
		if (!(diffs & (1 << x)))
			continue;	/* no change, skip */

		select = IOAPIC_REDTBL + (x * 2);	/* calculate addr */
		low_reg = io_apic_read(apic, select);	/* read contents */

		lowReg |= IOART_INTMASK;	/* set mask */

		io_apic_write(apic, select, low_reg);	/* new value */
	}
}
#endif	/* READY */


#if defined(READY)
/*
 * clear INT mask bit for each bit set in 'mask'.
 * ignore INT mask bit for all others.
 * only consider lower 24 bits in mask.
 */
void
clr_io_apic_mask24(int apic, u_int32_t bits)
{
	int     x, y;
	u_char  select;		/* the select register is 8 bits */
	u_int32_t low_reg;	/* the window register is 32 bits */
	u_int32_t diffs;

	bits &= IO_FIELD;	/* safety valve, only use 24 bits */
	diffs = bits & IO_MASK;	/* set AND needing 'clr'ing */
	if (!diffs)
		return;

#error imen/IOApicMask NOT merged in clrIOApicMask24()

	for (x = 0, y = REDIRCNT_IOAPIC(apic); x < y; ++x) {
		if (!(diffs & (1 << x)))
			continue;	/* no change, skip */

		select = IOAPIC_REDTBL + (x * 2);	/* calculate addr */
		low_reg = io_apic_read(apic, select);	/* read contents */

		low_reg &= ~IOART_INTMASK;	/* clear mask */

		io_apic_write(apic, select, low_reg);	/* new value */
	}
}
#endif	/* READY */

#undef IO_FIELD
#undef IO_MASK

#endif	/* APIC_IO */


/*
 * timer code, in development...
 * suggested by rgrimes@gndrsh.aac.dev.com
 */

/** FIXME: temp hack till we can determin bus clock */
#ifndef BUS_CLOCK
#define BUS_CLOCK	66000000
#define bus_clock()	66000000
#endif

#if defined(READY)

int acquire_apic_timer __P((void));
int release_apic_timer __P((void));

/*
 * acquire the APIC timer for exclusive use
 */
int
acquire_apic_timer(void)
{
#if 1
	return 0;
#else
	/** FIXME: make this really do something */
	panic("APIC timer in use when attempting to aquire\n");
#endif
}


/*
 * return the APIC timer
 */
int
release_apic_timer(void)
{
#if 1
	return 0;
#else
	/** FIXME: make this really do something */
	panic("APIC timer was already released\n");
#endif
}
#endif	/* READY */


/*
 * load a 'downcount time' in uSeconds
 */
void
set_apic_timer(int value)
{
	u_long  lvtt;
	long    ticks_per_microsec;

	/* calculate divisor and count from value:
	 * 
	 * timeBase == CPU bus clock divisor == [1,2,4,8,16,32,64,128] value ==
	 * time in uS */
	apic_base[APIC_TDCR] = APIC_TDCR_1;
	ticks_per_microsec = bus_clock() / 1000000;

	/* configure timer as one-shot */
	lvtt = apic_base[APIC_LVTT];
	lvtt &= ~(APIC_LVTT_VECTOR | APIC_LVTT_DS | APIC_LVTT_M | APIC_LVTT_TM);
	lvtt |= APIC_LVTT_M;	/* no INT, one-shot */
	apic_base[APIC_LVTT] = lvtt;

	/* */
	apic_base[APIC_TICR] = value * ticks_per_microsec;
}


/*
 * read remaining time in timer
 */
int
read_apic_timer(void)
{
#if 0
	/** FIXME: we need to return the actual remaining time,
         *         for now we just return the remaining count.
         */
#else
	return apic_base[APIC_TCCR];
#endif
}


/*
 * spin-style delay, set delay time in uS, spin till it drains
 */
void
u_sleep(int count)
{
	set_apic_timer(count);
	while (read_apic_timer())
		 /* spin */ ;
}
