/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)isa.c	7.2 (Berkeley) 5/13/91
 *	$Id: intr_machdep.c,v 1.11 1998/05/31 10:53:52 bde Exp $
 */

#include "opt_auto_eoi.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/syslog.h>
#include <machine/ipl.h>
#include <machine/md_var.h>
#include <machine/segments.h>
#if defined(APIC_IO)
#include <machine/smp.h>
#include <machine/smptests.h>			/** FAST_HI */
#endif /* APIC_IO */
#include <i386/isa/isa_device.h>
#ifdef PC98
#include <pc98/pc98/pc98.h>
#include <pc98/pc98/pc98_machdep.h>
#include <pc98/pc98/epsonio.h>
#else
#include <i386/isa/isa.h>
#endif
#include <i386/isa/icu.h>
#include "vector.h"

#include <i386/isa/intr_machdep.h>
#include <sys/interrupt.h>
#ifdef APIC_IO
#include <machine/clock.h>
#endif

/* XXX should be in suitable include files */
#ifdef PC98
#define	ICU_IMR_OFFSET		2		/* IO_ICU{1,2} + 2 */
#define	ICU_SLAVEID			7
#else
#define	ICU_IMR_OFFSET		1		/* IO_ICU{1,2} + 1 */
#define	ICU_SLAVEID			2
#endif

#ifdef APIC_IO
/*
 * This is to accommodate "mixed-mode" programming for 
 * motherboards that don't connect the 8254 to the IO APIC.
 */
#define AUTO_EOI_1
#endif

u_long	*intr_countp[ICU_LEN];
inthand2_t *intr_handler[ICU_LEN];
u_int	intr_mask[ICU_LEN];
static u_int*	intr_mptr[ICU_LEN];
void	*intr_unit[ICU_LEN];

static inthand_t *fastintr[ICU_LEN] = {
	&IDTVEC(fastintr0), &IDTVEC(fastintr1),
	&IDTVEC(fastintr2), &IDTVEC(fastintr3),
	&IDTVEC(fastintr4), &IDTVEC(fastintr5),
	&IDTVEC(fastintr6), &IDTVEC(fastintr7),
	&IDTVEC(fastintr8), &IDTVEC(fastintr9),
	&IDTVEC(fastintr10), &IDTVEC(fastintr11),
	&IDTVEC(fastintr12), &IDTVEC(fastintr13),
	&IDTVEC(fastintr14), &IDTVEC(fastintr15)
#if defined(APIC_IO)
	, &IDTVEC(fastintr16), &IDTVEC(fastintr17),
	&IDTVEC(fastintr18), &IDTVEC(fastintr19),
	&IDTVEC(fastintr20), &IDTVEC(fastintr21),
	&IDTVEC(fastintr22), &IDTVEC(fastintr23)
#endif /* APIC_IO */
};

static inthand_t *slowintr[ICU_LEN] = {
	&IDTVEC(intr0), &IDTVEC(intr1), &IDTVEC(intr2), &IDTVEC(intr3),
	&IDTVEC(intr4), &IDTVEC(intr5), &IDTVEC(intr6), &IDTVEC(intr7),
	&IDTVEC(intr8), &IDTVEC(intr9), &IDTVEC(intr10), &IDTVEC(intr11),
	&IDTVEC(intr12), &IDTVEC(intr13), &IDTVEC(intr14), &IDTVEC(intr15)
#if defined(APIC_IO)
	, &IDTVEC(intr16), &IDTVEC(intr17), &IDTVEC(intr18), &IDTVEC(intr19),
	&IDTVEC(intr20), &IDTVEC(intr21), &IDTVEC(intr22), &IDTVEC(intr23)
#endif /* APIC_IO */
};

static ointhand2_t isa_strayintr;

#ifdef PC98
#define NMI_PARITY 0x04
#define NMI_EPARITY 0x02
#else
#define NMI_PARITY (1 << 7)
#define NMI_IOCHAN (1 << 6)
#define ENMI_WATCHDOG (1 << 7)
#define ENMI_BUSTIMER (1 << 6)
#define ENMI_IOSTATUS (1 << 5)
#endif

/*
 * Handle a NMI, possibly a machine check.
 * return true to panic system, false to ignore.
 */
int
isa_nmi(cd)
	int cd;
{
#ifdef PC98
 	int port = inb(0x33);
	if (epson_machine_id == 0x20)
		epson_outb(0xc16, epson_inb(0xc16) | 0x1);
	if (port & NMI_PARITY) {
		panic("BASE RAM parity error, likely hardware failure.");
	} else if (port & NMI_EPARITY) {
		panic("EXTENDED RAM parity error, likely hardware failure.");
	} else {
		printf("\nNMI Resume ??\n");
		return(0);
	}
#else /* IBM-PC */
	int isa_port = inb(0x61);
	int eisa_port = inb(0x461);

	if (isa_port & NMI_PARITY)
		panic("RAM parity error, likely hardware failure.");

	if (isa_port & NMI_IOCHAN)
		panic("I/O channel check, likely hardware failure.");

	/*
	 * On a real EISA machine, this will never happen.  However it can
	 * happen on ISA machines which implement XT style floating point
	 * error handling (very rare).  Save them from a meaningless panic.
	 */
	if (eisa_port == 0xff)
		return(0);

	if (eisa_port & ENMI_WATCHDOG)
		panic("EISA watchdog timer expired, likely hardware failure.");

	if (eisa_port & ENMI_BUSTIMER)
		panic("EISA bus timeout, likely hardware failure.");

	if (eisa_port & ENMI_IOSTATUS)
		panic("EISA I/O port status error.");

	printf("\nNMI ISA %x, EISA %x\n", isa_port, eisa_port);
	return(0);
#endif
}

/*
 * Fill in default interrupt table (in case of spuruious interrupt
 * during configuration of kernel, setup interrupt control unit
 */
void
isa_defaultirq()
{
	int i;

	/* icu vectors */
	for (i = 0; i < ICU_LEN; i++)
		icu_unset(i, (inthand2_t *)NULL);

	/* initialize 8259's */
	outb(IO_ICU1, 0x11);		/* reset; program device, four bytes */

	outb(IO_ICU1+ICU_IMR_OFFSET, NRSVIDT);	/* starting at this vector index */
	outb(IO_ICU1+ICU_IMR_OFFSET, IRQ_SLAVE);		/* slave on line 7 */
#ifdef PC98
#ifdef AUTO_EOI_1
	outb(IO_ICU1+ICU_IMR_OFFSET, 0x1f);		/* (master) auto EOI, 8086 mode */
#else
	outb(IO_ICU1+ICU_IMR_OFFSET, 0x1d);		/* (master) 8086 mode */
#endif
#else /* IBM-PC */
#ifdef AUTO_EOI_1
	outb(IO_ICU1+ICU_IMR_OFFSET, 2 | 1);		/* auto EOI, 8086 mode */
#else
	outb(IO_ICU1+ICU_IMR_OFFSET, 1);		/* 8086 mode */
#endif
#endif /* PC98 */
	outb(IO_ICU1+ICU_IMR_OFFSET, 0xff);		/* leave interrupts masked */
	outb(IO_ICU1, 0x0a);		/* default to IRR on read */
#ifndef PC98
	outb(IO_ICU1, 0xc0 | (3 - 1));	/* pri order 3-7, 0-2 (com2 first) */
#endif /* !PC98 */

	outb(IO_ICU2, 0x11);		/* reset; program device, four bytes */
	outb(IO_ICU2+ICU_IMR_OFFSET, NRSVIDT+8); /* staring at this vector index */
	outb(IO_ICU2+ICU_IMR_OFFSET, ICU_SLAVEID);         /* my slave id is 7 */
#ifdef PC98
	outb(IO_ICU2+ICU_IMR_OFFSET,9);              /* 8086 mode */
#else /* IBM-PC */
#ifdef AUTO_EOI_2
	outb(IO_ICU2+ICU_IMR_OFFSET, 2 | 1);		/* auto EOI, 8086 mode */
#else
	outb(IO_ICU2+ICU_IMR_OFFSET,1);		/* 8086 mode */
#endif
#endif /* PC98 */
	outb(IO_ICU2+ICU_IMR_OFFSET, 0xff);          /* leave interrupts masked */
	outb(IO_ICU2, 0x0a);		/* default to IRR on read */
}

/*
 * Caught a stray interrupt, notify
 */
static void
isa_strayintr(d)
	int d;
{

	/* DON'T BOTHER FOR NOW! */
	/* for some reason, we get bursts of intr #7, even if not enabled! */
	/*
	 * Well the reason you got bursts of intr #7 is because someone
	 * raised an interrupt line and dropped it before the 8259 could
	 * prioritize it.  This is documented in the intel data book.  This
	 * means you have BAD hardware!  I have changed this so that only
	 * the first 5 get logged, then it quits logging them, and puts
	 * out a special message. rgrimes 3/25/1993
	 */
	/*
	 * XXX TODO print a different message for #7 if it is for a
	 * glitch.  Glitches can be distinguished from real #7's by
	 * testing that the in-service bit is _not_ set.  The test
	 * must be done before sending an EOI so it can't be done if
	 * we are using AUTO_EOI_1.
	 */
	if (intrcnt[NR_DEVICES + d] <= 5)
		log(LOG_ERR, "stray irq %d\n", d);
	if (intrcnt[NR_DEVICES + d] == 5)
		log(LOG_CRIT,
		    "too many stray irq %d's; not logging any more\n", d);
}

/*
 * Return a bitmap of the current interrupt requests.  This is 8259-specific
 * and is only suitable for use at probe time.
 */
intrmask_t
isa_irq_pending()
{
	u_char irr1;
	u_char irr2;

	irr1 = inb(IO_ICU1);
	irr2 = inb(IO_ICU2);
	return ((irr2 << 8) | irr1);
}

int
update_intr_masks(void)
{
	int intr, n=0;
	u_int mask,*maskptr;

	for (intr=0; intr < ICU_LEN; intr ++) {
#if defined(APIC_IO)
		/* no 8259 SLAVE to ignore */
#else
		if (intr==ICU_SLAVEID) continue;	/* ignore 8259 SLAVE output */
#endif /* APIC_IO */
		maskptr = intr_mptr[intr];
		if (!maskptr) continue;
		*maskptr |= 1 << intr;
		mask = *maskptr;
		if (mask != intr_mask[intr]) {
#if 0
			printf ("intr_mask[%2d] old=%08x new=%08x ptr=%p.\n",
				intr, intr_mask[intr], mask, maskptr);
#endif
			intr_mask[intr]=mask;
			n++;
		}

	}
	return (n);
}

/*
 * The find_device_id function is only required because of the way the
 * device names are currently stored for reporting in systat or vmstat.
 * In fact, those programs should be modified to use the sysctl interface
 * to obtain a list of driver names by traversing intreclist_head[irq].
 */
static int
find_device_id(int irq)
{
	char buf[16];
	char *cp;
	int free_id, id;

	sprintf(buf, "pci irq%d", irq);
	cp = intrnames;
	/* default to 0, which corresponds to clk0 */
	free_id = 0;

	for (id = 0; id < NR_DEVICES; id++) {
		if (strcmp(cp, buf) == 0)
			return (id);
		if (free_id == 0 && strcmp(cp, "pci irqnn") == 0)
			free_id = id;
		while (*cp++ != '\0');
	}
#if 0
	if (free_id == 0) {
		/*
		 * All pci irq counters are in use, perhaps because config
		 * is old so there aren't any. Abuse the clk0 counter.
		 */
		printf("\tcounting shared irq%d as clk0 irq\n", irq);
	}
#endif
	return (free_id);
}

void
update_intrname(int intr, int device_id)
{
	char	*cp;
	int	id;

	if (device_id == -1)
		device_id = find_device_id(intr);

	if ((u_int)device_id >= NR_DEVICES)
		return;

	intr_countp[intr] = &intrcnt[device_id];

	for (cp = intrnames, id = 0; id <= device_id; id++)
		while (*cp++ != '\0')
			;
	if (cp > eintrnames)
		return;
	if (intr < 10) {
		cp[-3] = intr + '0';
		cp[-2] = ' ';
	} else if (intr < 20) {
		cp[-3] = '1';
		cp[-2] = intr - 10 + '0';
	} else {
		cp[-3] = '2';
		cp[-2] = intr - 20 + '0';
	}
}


int
icu_setup(int intr, inthand2_t *handler, void *arg, u_int *maskptr, int flags)
{
#ifdef FAST_HI
	int		select;		/* the select register is 8 bits */
	int		vector;
	u_int32_t	value;		/* the window register is 32 bits */
#endif /* FAST_HI */
	u_long	ef;
	u_int	mask = (maskptr ? *maskptr : 0);

#if defined(APIC_IO)
	if ((u_int)intr >= ICU_LEN)	/* no 8259 SLAVE to ignore */
#else
	if ((u_int)intr >= ICU_LEN || intr == ICU_SLAVEID)
#endif /* APIC_IO */
	if (intr_handler[intr] != isa_strayintr)
		return (EBUSY);

	ef = read_eflags();
	disable_intr();
	intr_handler[intr] = handler;
	intr_mptr[intr] = maskptr;
	intr_mask[intr] = mask | (1 << intr);
	intr_unit[intr] = arg;
#ifdef FAST_HI
	if (flags & INTR_FAST) {
		vector = TPR_FAST_INTS + intr;
		setidt(vector, fastintr[intr],
		       SDT_SYS386IGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	}
	else {
		vector = TPR_SLOW_INTS + intr;
#ifdef APIC_INTR_REORDER
#ifdef APIC_INTR_HIGHPRI_CLOCK
		/* XXX: Hack (kludge?) for more accurate clock. */
		if (intr == apic_8254_intr || intr == 8) {
			vector = TPR_FAST_INTS + intr;
		}
#endif
#endif
		setidt(vector, slowintr[intr],
		       SDT_SYS386IGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	}
#ifdef APIC_INTR_REORDER
	set_lapic_isrloc(intr, vector);
#endif
	/*
	 * XXX MULTIPLE_IOAPICSXXX
	 * Reprogram the vector in the IO APIC.
	 */
	select = (intr * 2) + IOAPIC_REDTBL0;
	value = io_apic_read(0, select) & ~IOART_INTVEC;
	io_apic_write(0, select, value | vector);
#else
	setidt(ICU_OFFSET + intr,
	       flags & INTR_FAST ? fastintr[intr] : slowintr[intr],
	       SDT_SYS386IGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
#endif /* FAST_HI */
	INTREN(1 << intr);
	MPINTR_UNLOCK();
	write_eflags(ef);
	return (0);
}

void
register_imask(dvp, mask)
	struct isa_device *dvp;
	u_int	mask;
{
	if (dvp->id_alive && dvp->id_irq) {
		int	intr;

		intr = ffs(dvp->id_irq) - 1;
		intr_mask[intr] = mask | (1 <<intr);
	}
	(void) update_intr_masks();
}

int
icu_unset(intr, handler)
	int	intr;
	inthand2_t *handler;
{
	u_long	ef;

	if ((u_int)intr >= ICU_LEN || handler != intr_handler[intr])
		return (EINVAL);

	INTRDIS(1 << intr);
	ef = read_eflags();
	disable_intr();
	intr_countp[intr] = &intrcnt[NR_DEVICES + intr];
	intr_handler[intr] = isa_strayintr;
	intr_mptr[intr] = NULL;
	intr_mask[intr] = HWI_MASK | SWI_MASK;
	intr_unit[intr] = intr;
#ifdef FAST_HI_XXX
	/* XXX how do I re-create dvp here? */
	setidt(flags & INTR_FAST ? TPR_FAST_INTS + intr : TPR_SLOW_INTS + intr,
	    slowintr[intr], SDT_SYS386IGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
#else /* FAST_HI */
#ifdef APIC_INTR_REORDER
	set_lapic_isrloc(intr, ICU_OFFSET + intr);
#endif
	setidt(ICU_OFFSET + intr, slowintr[intr], SDT_SYS386IGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));
#endif /* FAST_HI */
	MPINTR_UNLOCK();
	write_eflags(ef);
	return (0);
}
