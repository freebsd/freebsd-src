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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>

#include <machine/smptests.h>	/** TEST_TEST1 */
#include <machine/smp.h>
#include <machine/mpapic.h>
#include <machine/segments.h>

#include <i386/isa/intr_machdep.h>	/* Xspuriousint() */
#include <i386/isa/icu.h>		/* apic_imen */

/* EISA Edge/Level trigger control registers */
#define ELCR0	0x4d0			/* eisa irq 0-7 */
#define ELCR1	0x4d1			/* eisa irq 8-15 */

/*
 * pointers to pmapped apic hardware.
 */

#if defined(APIC_IO)
volatile ioapic_t	**ioapic;
#endif	/* APIC_IO */

/*
 * Enable APIC, configure interrupts.
 */
void
apic_initialize(void)
{
	u_int   temp;

	/* setup LVT1 as ExtINT */
	temp = lapic.lvt_lint0;
	temp &= ~(APIC_LVT_M | APIC_LVT_TM | APIC_LVT_IIPP | APIC_LVT_DM);
	if (PCPU_GET(cpuid) == 0)
		temp |= 0x00000700;	/* process ExtInts */
	else
		temp |= 0x00010700;	/* mask ExtInts */
	lapic.lvt_lint0 = temp;

	/* setup LVT2 as NMI, masked till later... */
	temp = lapic.lvt_lint1;
	temp &= ~(APIC_LVT_M | APIC_LVT_TM | APIC_LVT_IIPP | APIC_LVT_DM);
	temp |= 0x00010400;		/* masked, edge trigger, active hi */

	lapic.lvt_lint1 = temp;

	/* set the Task Priority Register as needed */
	temp = lapic.tpr;
	temp &= ~APIC_TPR_PRIO;		/* clear priority field */
	temp |= LOPRIO_LEVEL;		/* allow INT arbitration */

	lapic.tpr = temp;

	/* enable the local APIC */
	temp = lapic.svr;
	temp |= APIC_SVR_SWEN;		/* software enable APIC */
	temp &= ~APIC_SVR_FOCUS;	/* enable 'focus processor' */

	/* set the 'spurious INT' vector */
	if ((XSPURIOUSINT_OFFSET & APIC_SVR_VEC_FIX) != APIC_SVR_VEC_FIX)
		panic("bad XSPURIOUSINT_OFFSET: 0x%08x", XSPURIOUSINT_OFFSET);
	temp &= ~APIC_SVR_VEC_PROG;	/* clear (programmable) vector field */
	temp |= (XSPURIOUSINT_OFFSET & APIC_SVR_VEC_PROG);

#if defined(TEST_TEST1)
	if (PCPU_GET(cpuid) == GUARD_CPU) {
		temp &= ~APIC_SVR_SWEN;	/* software DISABLE APIC */
	}
#endif  /** TEST_TEST1 */

	lapic.svr = temp;

	if (bootverbose)
		apic_dump("apic_initialize()");
}


/*
 * dump contents of local APIC registers
 */
void
apic_dump(char* str)
{
	printf("SMP: CPU%d %s:\n", PCPU_GET(cpuid), str);
	printf("     lint0: 0x%08x lint1: 0x%08x TPR: 0x%08x SVR: 0x%08x\n",
		lapic.lvt_lint0, lapic.lvt_lint1, lapic.tpr, lapic.svr);
}


#if defined(APIC_IO)

/*
 * IO APIC code,
 */

#define IOAPIC_ISA_INTS		16
#define REDIRCNT_IOAPIC(A) \
	    ((int)((io_apic_versions[(A)] & IOART_VER_MAXREDIR) >> MAXREDIRSHIFT) + 1)

static int trigger(int apic, int pin, u_int32_t * flags);
static void polarity(int apic, int pin, u_int32_t * flags, int level);

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

void
io_apic_set_id(int apic, int id)
{
	u_int32_t ux;
	
	ux = io_apic_read(apic, IOAPIC_ID);	/* get current contents */
	if (((ux & APIC_ID_MASK) >> 24) != id) {
		printf("Changing APIC ID for IO APIC #%d"
		       " from %d to %d on chip\n",
		       apic, ((ux & APIC_ID_MASK) >> 24), id);
		ux &= ~APIC_ID_MASK;	/* clear the ID field */
		ux |= (id << 24);
		io_apic_write(apic, IOAPIC_ID, ux);	/* write new value */
		ux = io_apic_read(apic, IOAPIC_ID);	/* re-read && test */
		if (((ux & APIC_ID_MASK) >> 24) != id)
			panic("can't control IO APIC #%d ID, reg: 0x%08x",
			      apic, ux);
	}
}


int
io_apic_get_id(int apic)
{
  return (io_apic_read(apic, IOAPIC_ID) & APIC_ID_MASK) >> 24;
}
  


/*
 * Setup the IO APIC.
 */

extern int	apic_pin_trigger;	/* 'opaque' */

void
io_apic_setup_intpin(int apic, int pin)
{
	int bus, bustype, irq;
	u_char		select;		/* the select register is 8 bits */
	u_int32_t	flags;		/* the window register is 32 bits */
	u_int32_t	target;		/* the window register is 32 bits */
	u_int32_t	vector;		/* the window register is 32 bits */
	int		level;

	target = IOART_DEST;

	select = pin * 2 + IOAPIC_REDTBL0;	/* register */
	/* 
	 * Always disable interrupts, and by default map
	 * pin X to IRQX because the disable doesn't stick
	 * and the uninitialize vector will get translated 
	 * into a panic.
	 *
	 * This is correct for IRQs 1 and 3-15.  In the other cases, 
	 * any robust driver will handle the spurious interrupt, and 
	 * the effective NOP beats a panic.
	 *
	 * A dedicated "bogus interrupt" entry in the IDT would
	 * be a nicer hack, although some one should find out 
	 * why some systems are generating interrupts when they
	 * shouldn't and stop the carnage.
	 */
	vector = NRSVIDT + pin;			/* IDT vec */
	mtx_lock_spin(&icu_lock);
	io_apic_write(apic, select,
		      (io_apic_read(apic, select) & ~IOART_INTMASK 
		       & ~0xff)|IOART_INTMSET|vector);
	mtx_unlock_spin(&icu_lock);
	
	/* we only deal with vectored INTs here */
	if (apic_int_type(apic, pin) != 0)
		return;
	
	irq = apic_irq(apic, pin);
	if (irq < 0)
		return;
	
	/* determine the bus type for this pin */
	bus = apic_src_bus_id(apic, pin);
	if (bus == -1)
		return;
	bustype = apic_bus_type(bus);
	
	if ((bustype == ISA) &&
	    (pin < IOAPIC_ISA_INTS) && 
	    (irq == pin) &&
	    (apic_polarity(apic, pin) == 0x1) &&
	    (apic_trigger(apic, pin) == 0x3)) {
		/* 
		 * A broken BIOS might describe some ISA 
		 * interrupts as active-high level-triggered.
		 * Use default ISA flags for those interrupts.
		 */
		flags = DEFAULT_ISA_FLAGS;
	} else {
		/* 
		 * Program polarity and trigger mode according to 
		 * interrupt entry.
		 */
		flags = DEFAULT_FLAGS;
		level = trigger(apic, pin, &flags);
		if (level == 1)
			apic_pin_trigger |= (1 << irq);
		polarity(apic, pin, &flags, level);
	}
	
	/* program the appropriate registers */
	if (apic != 0 || pin != irq)
		printf("IOAPIC #%d intpin %d -> irq %d\n",
		       apic, pin, irq);
	vector = NRSVIDT + irq;			/* IDT vec */
	mtx_lock_spin(&icu_lock);
	io_apic_write(apic, select, flags | vector);
	io_apic_write(apic, select + 1, target);
	mtx_unlock_spin(&icu_lock);
}

int
io_apic_setup(int apic)
{
	int		maxpin;
	int		pin;

	if (apic == 0)
		apic_pin_trigger = 0;	/* default to edge-triggered */

	maxpin = REDIRCNT_IOAPIC(apic);		/* pins in APIC */
	printf("Programming %d pins in IOAPIC #%d\n", maxpin, apic);
	
	for (pin = 0; pin < maxpin; ++pin) {
		io_apic_setup_intpin(apic, pin);
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
	  IOART_DELLOPRI))

/*
 * Setup the source of External INTerrupts.
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

	target = IOART_DEST;
	select = IOAPIC_REDTBL0 + (2 * intr);
	vector = NRSVIDT + intr;
	flags = DEFAULT_EXTINT_FLAGS;

	io_apic_write(apic, select, flags | vector);
	io_apic_write(apic, select + 1, target);

	return 0;
}
#undef DEFAULT_EXTINT_FLAGS


/*
 * Set the trigger level for an IO APIC pin.
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

		/* Use ELCR settings to determine level or edge mode */
		level = (intcontrol >> eirq) & 1;

		/*
		 * Note that on older Neptune chipset based systems, any
		 * pci interrupts often show up here and in the ELCR as well
		 * as level sensitive interrupts attributed to the EISA bus.
		 */

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
	panic("bad APIC IO INT flags");
}


/*
 * Set the polarity value for an IO APIC pin.
 */
static void
polarity(int apic, int pin, u_int32_t * flags, int level)
{
	int     id;

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
		/* polarity converter always gives active high */
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
	panic("bad APIC IO INT flags");
}


/*
 * Print contents of apic_imen.
 */
void
imen_dump(void)
{
	int x;

	printf("SMP: enabled INTs: ");
	for (x = 0; x < NHWI; ++x)
		if ((apic_imen & (1 << x)) == 0)
        		printf("%d, ", x);
	printf("apic_imen: 0x%08x\n", apic_imen);
}


/*
 * Inter Processor Interrupt functions.
 */


/*
 * Send APIC IPI 'vector' to 'destType' via 'deliveryMode'.
 *
 *  destType is 1 of: APIC_DEST_SELF, APIC_DEST_ALLISELF, APIC_DEST_ALLESELF
 *  vector is any valid SYSTEM INT vector
 *  delivery_mode is 1 of: APIC_DELMODE_FIXED, APIC_DELMODE_LOWPRIO
 */
#define DETECT_DEADLOCK
int
apic_ipi(int dest_type, int vector, int delivery_mode)
{
	u_long  icr_lo;

#if defined(DETECT_DEADLOCK)
#define MAX_SPIN1	10000000
#define MAX_SPIN2	1000
	int     x;

	/* "lazy delivery", ie we only barf if they stack up on us... */
	for (x = MAX_SPIN1; x; --x) {
		if ((lapic.icr_lo & APIC_DELSTAT_MASK) == 0)
			break;
	}
	if (x == 0)
		panic("apic_ipi was stuck");
#endif  /* DETECT_DEADLOCK */

	/* build IRC_LOW */
	icr_lo = (lapic.icr_lo & APIC_RESV2_MASK)
	    | dest_type | delivery_mode | vector;

	/* write APIC ICR */
	lapic.icr_lo = icr_lo;

	/* wait for pending status end */
#if defined(DETECT_DEADLOCK)
	for (x = MAX_SPIN2; x; --x) {
		if ((lapic.icr_lo & APIC_DELSTAT_MASK) == 0)
			break;
	}
#ifdef needsattention
/*
 * XXX FIXME:
 *      The above loop waits for the message to actually be delivered.
 *      It breaks out after an arbitrary timout on the theory that it eventually
 *      will be delivered and we will catch a real failure on the next entry to
 *      this function, which would panic().
 *      We could skip this wait entirely, EXCEPT it probably protects us from
 *      other "less robust" routines that assume the message was delivered and
 *      acted upon when this function returns.  TLB shootdowns are one such
 *      "less robust" function.
 */
	if (x == 0)
		printf("apic_ipi might be stuck\n");
#endif
#undef MAX_SPIN2
#undef MAX_SPIN1
#else
	while (lapic.icr_lo & APIC_DELSTAT_MASK)
		 /* spin */ ;
#endif  /* DETECT_DEADLOCK */

	/** XXX FIXME: return result */
	return 0;
}

static int
apic_ipi_singledest(int cpu, int vector, int delivery_mode)
{
	u_long  icr_lo;
	u_long  icr_hi;
	u_long  eflags;

#if defined(DETECT_DEADLOCK)
#define MAX_SPIN1	10000000
#define MAX_SPIN2	1000
	int     x;

	/* "lazy delivery", ie we only barf if they stack up on us... */
	for (x = MAX_SPIN1; x; --x) {
		if ((lapic.icr_lo & APIC_DELSTAT_MASK) == 0)
			break;
	}
	if (x == 0)
		panic("apic_ipi was stuck");
#endif  /* DETECT_DEADLOCK */

	eflags = read_eflags();
	__asm __volatile("cli" : : : "memory");
	icr_hi = lapic.icr_hi & ~APIC_ID_MASK;
	icr_hi |= (CPU_TO_ID(cpu) << 24);
	lapic.icr_hi = icr_hi;

	/* build IRC_LOW */
	icr_lo = (lapic.icr_lo & APIC_RESV2_MASK)
	    | APIC_DEST_DESTFLD | delivery_mode | vector;

	/* write APIC ICR */
	lapic.icr_lo = icr_lo;
	write_eflags(eflags);

	/* wait for pending status end */
#if defined(DETECT_DEADLOCK)
	for (x = MAX_SPIN2; x; --x) {
		if ((lapic.icr_lo & APIC_DELSTAT_MASK) == 0)
			break;
	}
#ifdef needsattention
/*
 * XXX FIXME:
 *      The above loop waits for the message to actually be delivered.
 *      It breaks out after an arbitrary timout on the theory that it eventually
 *      will be delivered and we will catch a real failure on the next entry to
 *      this function, which would panic().
 *      We could skip this wait entirely, EXCEPT it probably protects us from
 *      other "less robust" routines that assume the message was delivered and
 *      acted upon when this function returns.  TLB shootdowns are one such
 *      "less robust" function.
 */
	if (x == 0)
		printf("apic_ipi might be stuck\n");
#endif
#undef MAX_SPIN2
#undef MAX_SPIN1
#else
	while (lapic.icr_lo & APIC_DELSTAT_MASK)
		 /* spin */ ;
#endif  /* DETECT_DEADLOCK */

	/** XXX FIXME: return result */
	return 0;
}


/*
 * Send APIC IPI 'vector' to 'target's via 'delivery_mode'.
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

	if (target & ~0x7fff)
		return -1;	/* only 15 targets allowed */

	for (status = 0, x = 0; x <= 14; ++x)
		if (target & (1 << x)) {

			/* send the IPI */
			if (apic_ipi_singledest(x, vector, 
						delivery_mode) == -1)
				status |= (1 << x);
		}
	return status;
}

#endif	/* APIC_IO */


/*
 * Timer code, in development...
 *  - suggested by rgrimes@gndrsh.aac.dev.com
 */

/** XXX FIXME: temp hack till we can determin bus clock */
#ifndef BUS_CLOCK
#define BUS_CLOCK	66000000
#define bus_clock()	66000000
#endif

#if defined(READY)
int acquire_apic_timer(void);
int release_apic_timer(void);

/*
 * Acquire the APIC timer for exclusive use.
 */
int
acquire_apic_timer(void)
{
#if 1
	return 0;
#else
	/** XXX FIXME: make this really do something */
	panic("APIC timer in use when attempting to aquire");
#endif
}


/*
 * Return the APIC timer.
 */
int
release_apic_timer(void)
{
#if 1
	return 0;
#else
	/** XXX FIXME: make this really do something */
	panic("APIC timer was already released");
#endif
}
#endif	/* READY */


/*
 * Load a 'downcount time' in uSeconds.
 */
void
set_apic_timer(int value)
{
	u_long  lvtt;
	long    ticks_per_microsec;

	/*
	 * Calculate divisor and count from value:
	 * 
	 *  timeBase == CPU bus clock divisor == [1,2,4,8,16,32,64,128]
	 *  value == time in uS
	 */
	lapic.dcr_timer = APIC_TDCR_1;
	ticks_per_microsec = bus_clock() / 1000000;

	/* configure timer as one-shot */
	lvtt = lapic.lvt_timer;
	lvtt &= ~(APIC_LVTT_VECTOR | APIC_LVTT_DS | APIC_LVTT_M | APIC_LVTT_TM);
	lvtt |= APIC_LVTT_M;			/* no INT, one-shot */
	lapic.lvt_timer = lvtt;

	/* */
	lapic.icr_timer = value * ticks_per_microsec;
}


/*
 * Read remaining time in timer.
 */
int
read_apic_timer(void)
{
#if 0
	/** XXX FIXME: we need to return the actual remaining time,
         *         for now we just return the remaining count.
         */
#else
	return lapic.ccr_timer;
#endif
}


/*
 * Spin-style delay, set delay time in uS, spin till it drains.
 */
void
u_sleep(int count)
{
	set_apic_timer(count);
	while (read_apic_timer())
		 /* spin */ ;
}

/*
 * IOAPIC access helper functions.
 */
u_int
io_apic_read(int idx, int reg)
{
	volatile ioapic_t *apic;

	apic = ioapic[idx];
	apic->ioregsel = reg;
	return apic->iowin;
}

void
io_apic_write(int idx, int reg, u_int value)
{
	volatile ioapic_t *apic;

	apic = ioapic[idx];
	apic->ioregsel = reg;
	apic->iowin = value;
}
