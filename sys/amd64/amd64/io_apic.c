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

#include "opt_atpic.h"
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

#if defined(DEV_ISA) && defined(DEV_ATPIC) && !defined(NO_MIXED_MODE)
#define	MIXED_MODE
#endif

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

MALLOC_DEFINE(M_IOAPIC, "I/O APIC", "I/O APIC structures");

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

static STAILQ_HEAD(,ioapic) ioapic_list = STAILQ_HEAD_INITIALIZER(ioapic_list);
static u_int next_id, program_logical_dest;

static u_int	ioapic_read(volatile ioapic_t *apic, int reg);
static void	ioapic_write(volatile ioapic_t *apic, int reg, u_int val);
static void	ioapic_enable_source(struct intsrc *isrc);
static void	ioapic_disable_source(struct intsrc *isrc);
static void	ioapic_eoi_source(struct intsrc *isrc);
static void	ioapic_enable_intr(struct intsrc *isrc);
static int	ioapic_vector(struct intsrc *isrc);
static int	ioapic_source_pending(struct intsrc *isrc);
static void	ioapic_suspend(struct intsrc *isrc);
static void	ioapic_resume(struct intsrc *isrc);
static void	ioapic_program_destination(struct ioapic_intsrc *intpin);
#ifdef MIXED_MODE
static void	ioapic_setup_mixed_mode(struct ioapic_intsrc *intpin);
#endif

struct pic ioapic_template = { ioapic_enable_source, ioapic_disable_source,
			       ioapic_eoi_source, ioapic_enable_intr,
			       ioapic_vector, ioapic_source_pending,
			       ioapic_suspend, ioapic_resume };
	
static int next_ioapic_base, logical_clusters, current_cluster;

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
ioapic_disable_source(struct intsrc *isrc)
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
	mtx_unlock_spin(&icu_lock);
}

static void
ioapic_eoi_source(struct intsrc *isrc)
{

	lapic_eoi();
}

/*
 * Program an individual intpin's logical destination.
 */
static void
ioapic_program_destination(struct ioapic_intsrc *intpin)
{
	struct ioapic *io = (struct ioapic *)intpin->io_intsrc.is_pic;
	uint32_t value;

	KASSERT(intpin->io_dest != DEST_NONE,
	    ("intpin not assigned to a cluster"));
	KASSERT(intpin->io_dest != DEST_EXTINT,
	    ("intpin routed via ExtINT"));
	if (bootverbose) {
		printf("ioapic%u: routing intpin %u (", io->io_id,
		    intpin->io_intpin);
		if (intpin->io_vector == VECTOR_EXTINT)
			printf("ExtINT");
		else
			printf("IRQ %u", intpin->io_vector);
		printf(") to cluster %u\n", intpin->io_dest);
	}
	mtx_lock_spin(&icu_lock);
	value = ioapic_read(io->io_addr, IOAPIC_REDTBL_LO(intpin->io_intpin));
	value &= ~IOART_DESTMOD;
	value |= IOART_DESTLOG;
	ioapic_write(io->io_addr, IOAPIC_REDTBL_LO(intpin->io_intpin), value);
	value = ioapic_read(io->io_addr, IOAPIC_REDTBL_HI(intpin->io_intpin));
	value &= ~IOART_DEST;
	value |= (intpin->io_dest << APIC_ID_CLUSTER_SHIFT |
	    APIC_ID_CLUSTER_ID) << APIC_ID_SHIFT;
	ioapic_write(io->io_addr, IOAPIC_REDTBL_HI(intpin->io_intpin), value);
	mtx_unlock_spin(&icu_lock);
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

static void
ioapic_suspend(struct intsrc *isrc)
{

	TODO;
}

static void
ioapic_resume(struct intsrc *isrc)
{

	TODO;
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
		 * Assume that pin 0 on the first IO APIC is an ExtINT pin by
		 * default.  Assume that intpins 1-15 are ISA interrupts and
		 * use suitable defaults for those.  Assume that all other
		 * intpins are PCI interrupts.  Enable the ExtINT pin by
		 * default but mask all other pins.
		 */
		if (intpin->io_vector == 0) {
			intpin->io_activehi = 1;
			intpin->io_edgetrigger = 1;
			intpin->io_vector = VECTOR_EXTINT;
			intpin->io_masked = 0;
		} else if (intpin->io_vector < IOAPIC_ISA_INTS) {
			intpin->io_activehi = 1;
			intpin->io_edgetrigger = 1;
			intpin->io_masked = 1;
		} else {
			intpin->io_activehi = 0;
			intpin->io_edgetrigger = 0;
			intpin->io_masked = 1;
		}

		/*
		 * Start off without a logical cluster destination until
		 * the pin is enabled.
		 */
		intpin->io_dest = DEST_NONE;
		if (bootverbose) {
			printf("ioapic%u: intpin %d -> ",  io->io_id, i);
			if (intpin->io_vector == VECTOR_EXTINT)
				printf("ExtINT");
			else
				printf("irq %u", intpin->io_vector);
			printf(" (%s, active%s)\n", intpin->io_edgetrigger ?
			    "edge" : "level", intpin->io_activehi ? "hi" :
			    "lo");
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
ioapic_set_nmi(void *cookie, u_int pin)
{
	struct ioapic *io;

	io = (struct ioapic *)cookie;
	if (pin >= io->io_numintr)
		return (EINVAL);
	if (io->io_pins[pin].io_vector >= NUM_IO_INTS)
		return (EINVAL);
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
	if (io->io_pins[pin].io_vector >= NUM_IO_INTS)
		return (EINVAL);
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
	if (io->io_pins[pin].io_vector >= NUM_IO_INTS)
		return (EINVAL);
	io->io_pins[pin].io_vector = VECTOR_EXTINT;
	io->io_pins[pin].io_masked = 0;
	io->io_pins[pin].io_edgetrigger = 1;
	io->io_pins[pin].io_activehi = 1;
	if (bootverbose)
		printf("ioapic%u: Routing external 8259A's -> intpin %d\n",
		    io->io_id, pin);
	return (0);
}

int
ioapic_set_polarity(void *cookie, u_int pin, char activehi)
{
	struct ioapic *io;

	io = (struct ioapic *)cookie;
	if (pin >= io->io_numintr)
		return (EINVAL);
	if (io->io_pins[pin].io_vector >= NUM_IO_INTS)
		return (EINVAL);
	io->io_pins[pin].io_activehi = activehi;
	if (bootverbose)
		printf("ioapic%u: intpin %d polarity: %s\n", io->io_id, pin,
		    activehi ? "active-hi" : "active-lo");
	return (0);
}

int
ioapic_set_triggermode(void *cookie, u_int pin, char edgetrigger)
{
	struct ioapic *io;

	io = (struct ioapic *)cookie;
	if (pin >= io->io_numintr)
		return (EINVAL);
	if (io->io_pins[pin].io_vector >= NUM_IO_INTS)
		return (EINVAL);
	io->io_pins[pin].io_edgetrigger = edgetrigger;
	if (bootverbose)
		printf("ioapic%u: intpin %d trigger: %s\n", io->io_id, pin,
		    edgetrigger ? "edge" : "level");
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
	for (i = 0, pin = io->io_pins; i < io->io_numintr; i++, pin++) {
		/*
		 * Finish initializing the pins by programming the vectors
		 * and delivery mode.
		 */
		if (pin->io_vector == VECTOR_DISABLED)
			continue;
		flags = IOART_DESTPHY;
		if (pin->io_edgetrigger)
			flags |= IOART_TRGREDG;
		else
			flags |= IOART_TRGRLVL;
		if (pin->io_activehi)
			flags |= IOART_INTAHI;
		else
			flags |= IOART_INTALO;
		if (pin->io_masked)
			flags |= IOART_INTMSET;
		switch (pin->io_vector) {
		case VECTOR_EXTINT:
			KASSERT(pin->io_edgetrigger,
			    ("EXTINT not edge triggered"));
			flags |= IOART_DELEXINT;
			break;
		case VECTOR_NMI:
			KASSERT(pin->io_edgetrigger,
			    ("NMI not edge triggered"));
			flags |= IOART_DELNMI;
			break;
		case VECTOR_SMI:
			KASSERT(pin->io_edgetrigger,
			    ("SMI not edge triggered"));
			flags |= IOART_DELSMI;
			break;
		default:
			flags |= IOART_DELLOPRI |
			    apic_irq_to_idt(pin->io_vector);
		}
		mtx_lock_spin(&icu_lock);
		ioapic_write(apic, IOAPIC_REDTBL_LO(i), flags);

		/*
		 * Route interrupts to the BSP by default using physical
		 * addressing.  Vectored interrupts get readdressed using
		 * logical IDs to CPU clusters when they are enabled.
		 */
		flags = ioapic_read(apic, IOAPIC_REDTBL_HI(i));
		flags &= ~IOART_DEST;
		flags |= PCPU_GET(apic_id) << APIC_ID_SHIFT;
		ioapic_write(apic, IOAPIC_REDTBL_HI(i), flags);
		mtx_unlock_spin(&icu_lock);
		if (pin->io_vector < NUM_IO_INTS) {
#ifdef MIXED_MODE
			/* Route IRQ0 via the 8259A using mixed mode. */
			if (pin->io_vector == 0)
				ioapic_setup_mixed_mode(pin);
			else
#endif
				intr_register_source(&pin->io_intsrc);
		}
			
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

#ifdef MIXED_MODE
/*
 * Support for mixed-mode interrupt sources.  These sources route an ISA
 * IRQ through the 8259A's via the ExtINT on pin 0 of the I/O APIC that
 * routes the ISA interrupts.  We just ignore the intpins that use this
 * mode and allow the atpic driver to register its interrupt source for
 * that IRQ instead.
 */

void
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

#endif /* MIXED_MODE */
