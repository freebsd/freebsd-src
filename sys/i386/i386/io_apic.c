/*-
 * Copyright (c) 2003 John Baldwin <jhb@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_isa.h"
#include "opt_no_mixed_mode.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/apicreg.h>
#include <machine/frame.h>
#include <machine/intr_machdep.h>
#include <machine/apicvar.h>
#include <machine/segments.h>

#define IOAPIC_ISA_INTS		16
#define	IOAPIC_MEM_REGION	32
#define	IOAPIC_REDTBL_LO(i)	(IOAPIC_REDTBL + (i) * 2)
#define	IOAPIC_REDTBL_HI(i)	(IOAPIC_REDTBL_LO(i) + 1)

#define	VECTOR_EXTINT		252
#define	VECTOR_NMI		253
#define	VECTOR_SMI		254
#define	VECTOR_DISABLED		255

#define	DEST_NONE		-1
#define	DEST_EXTINT		-2

#define	TODO		printf("%s: not implemented!\n", __func__)

static MALLOC_DEFINE(M_IOAPIC, "I/O APIC", "I/O APIC structures");

/*
 * New interrupt support code..
 *
 * XXX: we really should have the interrupt cookie passed up from new-bus
 * just be a int pin, and not map 1:1 to interrupt vector number but should
 * use INTR_TYPE_FOO to set priority bands for device classes and do all the
 * magic remapping of intpin to vector in here.  For now we just cheat as on
 * ia64 and map intpin X to vector NRSVIDT + X.  Note that we assume that the
 * first IO APIC has ISA interrupts on pins 1-15.  Not sure how you are
 * really supposed to figure out which IO APIC in a system with multiple IO
 * APIC's actually has the ISA interrupts routed to it.  As far as interrupt
 * pin numbers, we use the ACPI System Interrupt number model where each
 * IO APIC has a contiguous chunk of the System Interrupt address space.
 */

/*
 * Direct the ExtINT pin on the first I/O APIC to a logical cluster of
 * CPUs rather than a physical destination of just the BSP.
 *
 * Note: This is disabled by default as test systems seem to croak with it
 * enabled.
#define ENABLE_EXTINT_LOGICAL_DESTINATION
 */

struct ioapic_intsrc {
	struct intsrc io_intsrc;
	u_int io_intpin:8;
	u_int io_vector:8;
	u_int io_activehi:1;
	u_int io_edgetrigger:1;
	u_int io_masked:1;
	int io_dest:5;
	int io_bus:4;
};

struct ioapic {
	struct pic io_pic;
	u_int io_id:8;			/* logical ID */
	u_int io_apic_id:4;
	u_int io_intbase:8;		/* System Interrupt base */
	u_int io_numintr:8;
	volatile ioapic_t *io_addr;	/* XXX: should use bus_space */
	STAILQ_ENTRY(ioapic) io_next;
	struct ioapic_intsrc io_pins[0];
};

static u_int	ioapic_read(volatile ioapic_t *apic, int reg);
static void	ioapic_write(volatile ioapic_t *apic, int reg, u_int val);
static const char *ioapic_bus_string(int bus_type);
static void	ioapic_print_vector(struct ioapic_intsrc *intpin);
static void	ioapic_enable_source(struct intsrc *isrc);
static void	ioapic_disable_source(struct intsrc *isrc, int eoi);
static void	ioapic_eoi_source(struct intsrc *isrc);
static void	ioapic_enable_intr(struct intsrc *isrc);
static int	ioapic_vector(struct intsrc *isrc);
static int	ioapic_source_pending(struct intsrc *isrc);
static int	ioapic_config_intr(struct intsrc *isrc, enum intr_trigger trig,
		    enum intr_polarity pol);
static void	ioapic_suspend(struct intsrc *isrc);
static void	ioapic_resume(struct intsrc *isrc);
static void	ioapic_program_destination(struct ioapic_intsrc *intpin);
static void	ioapic_program_intpin(struct ioapic_intsrc *intpin);
static void	ioapic_setup_mixed_mode(struct ioapic_intsrc *intpin);

static STAILQ_HEAD(,ioapic) ioapic_list = STAILQ_HEAD_INITIALIZER(ioapic_list);
struct pic ioapic_template = { ioapic_enable_source, ioapic_disable_source,
			       ioapic_eoi_source, ioapic_enable_intr,
			       ioapic_vector, ioapic_source_pending,
			       ioapic_suspend, ioapic_resume,
			       ioapic_config_intr };
	
static int bsp_id, current_cluster, logical_clusters, next_ioapic_base;
static u_int mixed_mode_enabled, next_id, program_logical_dest;
#ifdef NO_MIXED_MODE
static int mixed_mode_active = 0;
#else
static int mixed_mode_active = 1;
#endif
TUNABLE_INT("hw.apic.mixed_mode", &mixed_mode_active);

static __inline void
_ioapic_eoi_source(struct intsrc *isrc)
{
	lapic_eoi();
}

static u_int
ioapic_read(volatile ioapic_t *apic, int reg)
{

	mtx_assert(&icu_lock, MA_OWNED);
	apic->ioregsel = reg;
	return (apic->iowin);
}

static void
ioapic_write(volatile ioapic_t *apic, int reg, u_int val)
{

	mtx_assert(&icu_lock, MA_OWNED);
	apic->ioregsel = reg;
	apic->iowin = val;
}

static const char *
ioapic_bus_string(int bus_type)
{

	switch (bus_type) {
	case APIC_BUS_ISA:
		return ("ISA");
	case APIC_BUS_EISA:
		return ("EISA");
	case APIC_BUS_PCI:
		return ("PCI");
	default:
		return ("unknown");
	}
}

static void
ioapic_print_vector(struct ioapic_intsrc *intpin)
{

	switch (intpin->io_vector) {
	case VECTOR_DISABLED:
		printf("disabled");
		break;
	case VECTOR_EXTINT:
		printf("ExtINT");
		break;
	case VECTOR_NMI:
		printf("NMI");
		break;
	case VECTOR_SMI:
		printf("SMI");
		break;
	default:
		printf("%s IRQ %u", ioapic_bus_string(intpin->io_bus),
		    intpin->io_vector);
	}
}

static void
ioapic_enable_source(struct intsrc *isrc)
{
	struct ioapic_intsrc *intpin = (struct ioapic_intsrc *)isrc;
	struct ioapic *io = (struct ioapic *)isrc->is_pic;
	uint32_t flags;

	mtx_lock_spin(&icu_lock);
	if (intpin->io_masked) {
		flags = ioapic_read(io->io_addr,
		    IOAPIC_REDTBL_LO(intpin->io_intpin));
		flags &= ~(IOART_INTMASK);
		ioapic_write(io->io_addr, IOAPIC_REDTBL_LO(intpin->io_intpin),
		    flags);
		intpin->io_masked = 0;
	}
	mtx_unlock_spin(&icu_lock);
}

static void
ioapic_disable_source(struct intsrc *isrc, int eoi)
{
	struct ioapic_intsrc *intpin = (struct ioapic_intsrc *)isrc;
	struct ioapic *io = (struct ioapic *)isrc->is_pic;
	uint32_t flags;

	mtx_lock_spin(&icu_lock);
	if (!intpin->io_masked && !intpin->io_edgetrigger) {
		flags = ioapic_read(io->io_addr,
		    IOAPIC_REDTBL_LO(intpin->io_intpin));
		flags |= IOART_INTMSET;
		ioapic_write(io->io_addr, IOAPIC_REDTBL_LO(intpin->io_intpin),
		    flags);
		intpin->io_masked = 1;
	}

	if (eoi == PIC_EOI)
		_ioapic_eoi_source(isrc);

	mtx_unlock_spin(&icu_lock);
}

static void
ioapic_eoi_source(struct intsrc *isrc)
{

	_ioapic_eoi_source(isrc);
}

/*
 * Completely program an intpin based on the data in its interrupt source
 * structure.
 */
static void
ioapic_program_intpin(struct ioapic_intsrc *intpin)
{
	struct ioapic *io = (struct ioapic *)intpin->io_intsrc.is_pic;
	uint32_t low, high, value;

	/*
	 * For pins routed via mixed mode or disabled, just ensure that
	 * they are masked.
	 */
	if (intpin->io_dest == DEST_EXTINT ||
	    intpin->io_vector == VECTOR_DISABLED) {
		low = ioapic_read(io->io_addr,
		    IOAPIC_REDTBL_LO(intpin->io_intpin));
		if ((low & IOART_INTMASK) == IOART_INTMCLR)
			ioapic_write(io->io_addr,
			    IOAPIC_REDTBL_LO(intpin->io_intpin),
			    low | IOART_INTMSET);
		return;
	}

	/* Set the destination. */
	if (intpin->io_dest == DEST_NONE) {
		low = IOART_DESTPHY;
		high = bsp_id << APIC_ID_SHIFT;
	} else {
		low = IOART_DESTLOG;
		high = (intpin->io_dest << APIC_ID_CLUSTER_SHIFT |
		    APIC_ID_CLUSTER_ID) << APIC_ID_SHIFT;
	}

	/* Program the rest of the low word. */
	if (intpin->io_edgetrigger)
		low |= IOART_TRGREDG;
	else
		low |= IOART_TRGRLVL;
	if (intpin->io_activehi)
		low |= IOART_INTAHI;
	else
		low |= IOART_INTALO;
	if (intpin->io_masked)
		low |= IOART_INTMSET;
	switch (intpin->io_vector) {
	case VECTOR_EXTINT:
		KASSERT(intpin->io_edgetrigger,
		    ("EXTINT not edge triggered"));
		low |= IOART_DELEXINT;
		break;
	case VECTOR_NMI:
		KASSERT(intpin->io_edgetrigger,
		    ("NMI not edge triggered"));
		low |= IOART_DELNMI;
		break;
	case VECTOR_SMI:
		KASSERT(intpin->io_edgetrigger,
		    ("SMI not edge triggered"));
		low |= IOART_DELSMI;
		break;
	default:
		low |= IOART_DELLOPRI | apic_irq_to_idt(intpin->io_vector);
	}

	/* Write the values to the APIC. */
	mtx_lock_spin(&icu_lock);
	ioapic_write(io->io_addr, IOAPIC_REDTBL_LO(intpin->io_intpin), low);
	value = ioapic_read(io->io_addr, IOAPIC_REDTBL_HI(intpin->io_intpin));
	value &= ~IOART_DEST;
	value |= high;
	ioapic_write(io->io_addr, IOAPIC_REDTBL_HI(intpin->io_intpin), value);
	mtx_unlock_spin(&icu_lock);
}

/*
 * Program an individual intpin's logical destination.
 */
static void
ioapic_program_destination(struct ioapic_intsrc *intpin)
{
	struct ioapic *io = (struct ioapic *)intpin->io_intsrc.is_pic;

	KASSERT(intpin->io_dest != DEST_NONE,
	    ("intpin not assigned to a cluster"));
	KASSERT(intpin->io_dest != DEST_EXTINT,
	    ("intpin routed via ExtINT"));
	if (bootverbose) {
		printf("ioapic%u: routing intpin %u (", io->io_id,
		    intpin->io_intpin);
		ioapic_print_vector(intpin);
		printf(") to cluster %u\n", intpin->io_dest);
	}
	ioapic_program_intpin(intpin);
}

static void
ioapic_assign_cluster(struct ioapic_intsrc *intpin)
{

	/*
	 * Assign this intpin to a logical APIC cluster in a
	 * round-robin fashion.  We don't actually use the logical
	 * destination for this intpin until after all the CPU's
	 * have been started so that we don't end up with interrupts
	 * that don't go anywhere.  Another alternative might be to
	 * start up the CPU's earlier so that they can handle interrupts
	 * sooner.
	 */
	intpin->io_dest = current_cluster;
	current_cluster++;
	if (current_cluster >= logical_clusters)
		current_cluster = 0;
	if (program_logical_dest)
		ioapic_program_destination(intpin);
}

static void
ioapic_enable_intr(struct intsrc *isrc)
{
	struct ioapic_intsrc *intpin = (struct ioapic_intsrc *)isrc;

	KASSERT(intpin->io_dest != DEST_EXTINT,
	    ("ExtINT pin trying to use ioapic enable_intr method"));
	if (intpin->io_dest == DEST_NONE) {
		ioapic_assign_cluster(intpin);
		lapic_enable_intr(intpin->io_vector);
	}
}

static int
ioapic_vector(struct intsrc *isrc)
{
	struct ioapic_intsrc *pin;

	pin = (struct ioapic_intsrc *)isrc;
	return (pin->io_vector);
}

static int
ioapic_source_pending(struct intsrc *isrc)
{
	struct ioapic_intsrc *intpin = (struct ioapic_intsrc *)isrc;

	return (lapic_intr_pending(intpin->io_vector));
}

static int
ioapic_config_intr(struct intsrc *isrc, enum intr_trigger trig,
    enum intr_polarity pol)
{
	struct ioapic_intsrc *intpin = (struct ioapic_intsrc *)isrc;
	struct ioapic *io = (struct ioapic *)isrc->is_pic;
	int changed;

	KASSERT(!(trig == INTR_TRIGGER_CONFORM || pol == INTR_POLARITY_CONFORM),
	    ("%s: Conforming trigger or polarity\n", __func__));

	/*
	 * EISA interrupts always use active high polarity, so don't allow
	 * them to be set to active low.
	 *
	 * XXX: Should we write to the ELCR if the trigger mode changes for
	 * an EISA IRQ or an ISA IRQ with the ELCR present?
	 */
	if (intpin->io_bus == APIC_BUS_EISA)
		pol = INTR_POLARITY_HIGH;
	changed = 0;
	if (intpin->io_edgetrigger != (trig == INTR_TRIGGER_EDGE)) {
		if (bootverbose)
			printf("ioapic%u: Changing trigger for pin %u to %s\n",
			    io->io_id, intpin->io_intpin,
			    trig == INTR_TRIGGER_EDGE ? "edge" : "level");
		intpin->io_edgetrigger = (trig == INTR_TRIGGER_EDGE);
		changed++;
	}
	if (intpin->io_activehi != (pol == INTR_POLARITY_HIGH)) {
		if (bootverbose)
			printf("ioapic%u: Changing polarity for pin %u to %s\n",
			    io->io_id, intpin->io_intpin,
			    pol == INTR_POLARITY_HIGH ? "high" : "low");
		intpin->io_activehi = (pol == INTR_POLARITY_HIGH);
		changed++;
	}
	if (changed)
		ioapic_program_intpin(intpin);
	return (0);
}

static void
ioapic_suspend(struct intsrc *isrc)
{

	TODO;
}

static void
ioapic_resume(struct intsrc *isrc)
{

	ioapic_program_intpin((struct ioapic_intsrc *)isrc);
}

/*
 * APIC enumerators call this function to indicate that the 8259A AT PICs
 * are available and that mixed mode can be used.
 */
void
ioapic_enable_mixed_mode(void)
{

	mixed_mode_enabled = 1;
}

/*
 * Allocate and return a logical cluster ID.  Note that the first time
 * this is called, it returns cluster 0.  ioapic_enable_intr() treats
 * the two cases of logical_clusters == 0 and logical_clusters == 1 the
 * same: one cluster of ID 0 exists.  The logical_clusters == 0 case is
 * for UP kernels, which should never call this function.
 */
int
ioapic_next_logical_cluster(void)
{

	if (logical_clusters >= APIC_MAX_CLUSTER)
		panic("WARNING: Local APIC cluster IDs exhausted!");
	return (logical_clusters++);
}

/*
 * Create a plain I/O APIC object.
 */
void *
ioapic_create(uintptr_t addr, int32_t apic_id, int intbase)
{
	struct ioapic *io;
	struct ioapic_intsrc *intpin;
	volatile ioapic_t *apic;
	u_int numintr, i;
	uint32_t value;

	apic = (ioapic_t *)pmap_mapdev(addr, IOAPIC_MEM_REGION);
	mtx_lock_spin(&icu_lock);
	numintr = ((ioapic_read(apic, IOAPIC_VER) & IOART_VER_MAXREDIR) >>
	    MAXREDIRSHIFT) + 1;
	mtx_unlock_spin(&icu_lock);
	io = malloc(sizeof(struct ioapic) +
	    numintr * sizeof(struct ioapic_intsrc), M_IOAPIC, M_WAITOK);
	io->io_pic = ioapic_template;
	mtx_lock_spin(&icu_lock);
	io->io_id = next_id++;
	io->io_apic_id = ioapic_read(apic, IOAPIC_ID) >> APIC_ID_SHIFT;	
	if (apic_id != -1 && io->io_apic_id != apic_id) {
		ioapic_write(apic, IOAPIC_ID, apic_id << APIC_ID_SHIFT);
		mtx_unlock_spin(&icu_lock);
		io->io_apic_id = apic_id;
		printf("ioapic%u: Changing APIC ID to %d\n", io->io_id,
		    apic_id);
	} else
		mtx_unlock_spin(&icu_lock);
	if (intbase == -1) {
		intbase = next_ioapic_base;
		printf("ioapic%u: Assuming intbase of %d\n", io->io_id,
		    intbase);
	} else if (intbase != next_ioapic_base)
		printf("ioapic%u: WARNING: intbase %d != expected base %d\n",
		    io->io_id, intbase, next_ioapic_base);
	io->io_intbase = intbase;
	next_ioapic_base = intbase + numintr;
	io->io_numintr = numintr;
	io->io_addr = apic;

	/*
	 * Initialize pins.  Start off with interrupts disabled.  Default
	 * to active-hi and edge-triggered for ISA interrupts and active-lo
	 * and level-triggered for all others.
	 */
	bzero(io->io_pins, sizeof(struct ioapic_intsrc) * numintr);
	mtx_lock_spin(&icu_lock);
	for (i = 0, intpin = io->io_pins; i < numintr; i++, intpin++) {
		intpin->io_intsrc.is_pic = (struct pic *)io;
		intpin->io_intpin = i;
		intpin->io_vector = intbase + i;

		/*
		 * Assume that pin 0 on the first I/O APIC is an ExtINT pin
		 * if mixed mode is enabled and an ISA interrupt if not.
		 * Assume that pins 1-15 are ISA interrupts and that all
		 * other pins are PCI interrupts.
		 */
		if (intpin->io_vector == 0 && mixed_mode_enabled)
			ioapic_set_extint(io, i);
		else if (intpin->io_vector < IOAPIC_ISA_INTS) {
			intpin->io_bus = APIC_BUS_ISA;
			intpin->io_activehi = 1;
			intpin->io_edgetrigger = 1;
			intpin->io_masked = 1;
		} else {
			intpin->io_bus = APIC_BUS_PCI;
			intpin->io_activehi = 0;
			intpin->io_edgetrigger = 0;
			intpin->io_masked = 1;
		}

		/*
		 * Route interrupts to the BSP by default using physical
		 * addressing.  Vectored interrupts get readdressed using
		 * logical IDs to CPU clusters when they are enabled.
		 */
		intpin->io_dest = DEST_NONE;
		if (bootverbose && intpin->io_vector != VECTOR_DISABLED) {
			printf("ioapic%u: intpin %d -> ",  io->io_id, i);
			ioapic_print_vector(intpin);
			printf(" (%s, %s)\n", intpin->io_edgetrigger ?
			    "edge" : "level", intpin->io_activehi ? "high" :
			    "low");
		}
		value = ioapic_read(apic, IOAPIC_REDTBL_LO(i));
		ioapic_write(apic, IOAPIC_REDTBL_LO(i), value | IOART_INTMSET);
	}
	mtx_unlock_spin(&icu_lock);

	return (io);
}

int
ioapic_get_vector(void *cookie, u_int pin)
{
	struct ioapic *io;

	io = (struct ioapic *)cookie;
	if (pin >= io->io_numintr)
		return (-1);
	return (io->io_pins[pin].io_vector);
}

int
ioapic_disable_pin(void *cookie, u_int pin)
{
	struct ioapic *io;

	io = (struct ioapic *)cookie;
	if (pin >= io->io_numintr)
		return (EINVAL);
	if (io->io_pins[pin].io_vector == VECTOR_DISABLED)
		return (EINVAL);
	io->io_pins[pin].io_vector = VECTOR_DISABLED;
	if (bootverbose)
		printf("ioapic%u: intpin %d disabled\n", io->io_id, pin);
	return (0);
}

int
ioapic_remap_vector(void *cookie, u_int pin, int vector)
{
	struct ioapic *io;

	io = (struct ioapic *)cookie;
	if (pin >= io->io_numintr || vector < 0)
		return (EINVAL);
	if (io->io_pins[pin].io_vector >= NUM_IO_INTS)
		return (EINVAL);
	io->io_pins[pin].io_vector = vector;
	if (bootverbose)
		printf("ioapic%u: Routing IRQ %d -> intpin %d\n", io->io_id,
		    vector, pin);
	return (0);
}

int
ioapic_set_bus(void *cookie, u_int pin, int bus_type)
{
	struct ioapic *io;

	if (bus_type < 0 || bus_type > APIC_BUS_MAX)
		return (EINVAL);
	io = (struct ioapic *)cookie;
	if (pin >= io->io_numintr)
		return (EINVAL);
	if (io->io_pins[pin].io_vector >= NUM_IO_INTS)
		return (EINVAL);
	io->io_pins[pin].io_bus = bus_type;
	if (bootverbose)
		printf("ioapic%u: intpin %d bus %s\n", io->io_id, pin,
		    ioapic_bus_string(bus_type));
	return (0);
}

int
ioapic_set_nmi(void *cookie, u_int pin)
{
	struct ioapic *io;

	io = (struct ioapic *)cookie;
	if (pin >= io->io_numintr)
		return (EINVAL);
	if (io->io_pins[pin].io_vector == VECTOR_NMI)
		return (0);
	if (io->io_pins[pin].io_vector >= NUM_IO_INTS)
		return (EINVAL);
	io->io_pins[pin].io_bus = APIC_BUS_UNKNOWN;
	io->io_pins[pin].io_vector = VECTOR_NMI;
	io->io_pins[pin].io_masked = 0;
	io->io_pins[pin].io_edgetrigger = 1;
	io->io_pins[pin].io_activehi = 1;
	if (bootverbose)
		printf("ioapic%u: Routing NMI -> intpin %d\n",
		    io->io_id, pin);
	return (0);
}

int
ioapic_set_smi(void *cookie, u_int pin)
{
	struct ioapic *io;

	io = (struct ioapic *)cookie;
	if (pin >= io->io_numintr)
		return (EINVAL);
	if (io->io_pins[pin].io_vector == VECTOR_SMI)
		return (0);
	if (io->io_pins[pin].io_vector >= NUM_IO_INTS)
		return (EINVAL);
	io->io_pins[pin].io_bus = APIC_BUS_UNKNOWN;
	io->io_pins[pin].io_vector = VECTOR_SMI;
	io->io_pins[pin].io_masked = 0;
	io->io_pins[pin].io_edgetrigger = 1;
	io->io_pins[pin].io_activehi = 1;
	if (bootverbose)
		printf("ioapic%u: Routing SMI -> intpin %d\n",
		    io->io_id, pin);
	return (0);
}

int
ioapic_set_extint(void *cookie, u_int pin)
{
	struct ioapic *io;

	io = (struct ioapic *)cookie;
	if (pin >= io->io_numintr)
		return (EINVAL);
	if (io->io_pins[pin].io_vector == VECTOR_EXTINT)
		return (0);
	if (io->io_pins[pin].io_vector >= NUM_IO_INTS)
		return (EINVAL);
	io->io_pins[pin].io_bus = APIC_BUS_UNKNOWN;
	io->io_pins[pin].io_vector = VECTOR_EXTINT;

	/* Enable this pin if mixed mode is available and active. */
	if (mixed_mode_enabled && mixed_mode_active)
		io->io_pins[pin].io_masked = 0;
	else
		io->io_pins[pin].io_masked = 1;
	io->io_pins[pin].io_edgetrigger = 1;
	io->io_pins[pin].io_activehi = 1;
	if (bootverbose)
		printf("ioapic%u: Routing external 8259A's -> intpin %d\n",
		    io->io_id, pin);
	return (0);
}

int
ioapic_set_polarity(void *cookie, u_int pin, enum intr_polarity pol)
{
	struct ioapic *io;

	io = (struct ioapic *)cookie;
	if (pin >= io->io_numintr || pol == INTR_POLARITY_CONFORM)
		return (EINVAL);
	if (io->io_pins[pin].io_vector >= NUM_IO_INTS)
		return (EINVAL);
	io->io_pins[pin].io_activehi = (pol == INTR_POLARITY_HIGH);
	if (bootverbose)
		printf("ioapic%u: intpin %d polarity: %s\n", io->io_id, pin,
		    pol == INTR_POLARITY_HIGH ? "high" : "low");
	return (0);
}

int
ioapic_set_triggermode(void *cookie, u_int pin, enum intr_trigger trigger)
{
	struct ioapic *io;

	io = (struct ioapic *)cookie;
	if (pin >= io->io_numintr || trigger == INTR_TRIGGER_CONFORM)
		return (EINVAL);
	if (io->io_pins[pin].io_vector >= NUM_IO_INTS)
		return (EINVAL);
	io->io_pins[pin].io_edgetrigger = (trigger == INTR_TRIGGER_EDGE);
	if (bootverbose)
		printf("ioapic%u: intpin %d trigger: %s\n", io->io_id, pin,
		    trigger == INTR_TRIGGER_EDGE ? "edge" : "level");
	return (0);
}

/*
 * Register a complete I/O APIC object with the interrupt subsystem.
 */
void
ioapic_register(void *cookie)
{
	struct ioapic_intsrc *pin;
	struct ioapic *io;
	volatile ioapic_t *apic;
	uint32_t flags;
	int i;

	io = (struct ioapic *)cookie;
	apic = io->io_addr;
	mtx_lock_spin(&icu_lock);
	flags = ioapic_read(apic, IOAPIC_VER) & IOART_VER_VERSION;
	STAILQ_INSERT_TAIL(&ioapic_list, io, io_next);
	mtx_unlock_spin(&icu_lock);
	printf("ioapic%u <Version %u.%u> irqs %u-%u on motherboard\n",
	    io->io_id, flags >> 4, flags & 0xf, io->io_intbase,
	    io->io_intbase + io->io_numintr - 1);
	bsp_id = PCPU_GET(apic_id);
	for (i = 0, pin = io->io_pins; i < io->io_numintr; i++, pin++) {
		/*
		 * Finish initializing the pins by programming the vectors
		 * and delivery mode.
		 */
		if (pin->io_vector == VECTOR_DISABLED)
			continue;
		ioapic_program_intpin(pin);
		if (pin->io_vector >= NUM_IO_INTS)
			continue;
		/*
		 * Route IRQ0 via the 8259A using mixed mode if mixed mode
		 * is available and turned on.
		 */
		if (pin->io_vector == 0 && mixed_mode_active &&
		    mixed_mode_enabled)
			ioapic_setup_mixed_mode(pin);
		else
			intr_register_source(&pin->io_intsrc);
	}
}

/*
 * Program all the intpins to use logical destinations once the AP's
 * have been launched.
 */
static void
ioapic_set_logical_destinations(void *arg __unused)
{
	struct ioapic *io;
	int i;

	program_logical_dest = 1;
	STAILQ_FOREACH(io, &ioapic_list, io_next)
	    for (i = 0; i < io->io_numintr; i++)
		    if (io->io_pins[i].io_dest != DEST_NONE &&
			io->io_pins[i].io_dest != DEST_EXTINT)
			    ioapic_program_destination(&io->io_pins[i]);
}
SYSINIT(ioapic_destinations, SI_SUB_SMP, SI_ORDER_SECOND,
    ioapic_set_logical_destinations, NULL)

/*
 * Support for mixed-mode interrupt sources.  These sources route an ISA
 * IRQ through the 8259A's via the ExtINT on pin 0 of the I/O APIC that
 * routes the ISA interrupts.  We just ignore the intpins that use this
 * mode and allow the atpic driver to register its interrupt source for
 * that IRQ instead.
 */

static void
ioapic_setup_mixed_mode(struct ioapic_intsrc *intpin)
{
	struct ioapic_intsrc *extint;
	struct ioapic *io;

	/*
	 * Mark the associated I/O APIC intpin as being delivered via
	 * ExtINT and enable the ExtINT pin on the I/O APIC if needed.
	 */
	intpin->io_dest = DEST_EXTINT;
	io = (struct ioapic *)intpin->io_intsrc.is_pic;
	extint = &io->io_pins[0];
	if (extint->io_vector != VECTOR_EXTINT)
		panic("Can't find ExtINT pin to route through!");
#ifdef ENABLE_EXTINT_LOGICAL_DESTINATION
	if (extint->io_dest == DEST_NONE)
		ioapic_assign_cluster(extint);
#endif
}
