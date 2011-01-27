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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/apicreg.h>
#include <machine/frame.h>
#include <machine/intr_machdep.h>
#include <machine/apicvar.h>
#include <machine/resource.h>
#include <machine/segments.h>

#define IOAPIC_ISA_INTS		16
#define	IOAPIC_MEM_REGION	32
#define	IOAPIC_REDTBL_LO(i)	(IOAPIC_REDTBL + (i) * 2)
#define	IOAPIC_REDTBL_HI(i)	(IOAPIC_REDTBL_LO(i) + 1)

#define	IRQ_EXTINT		(NUM_IO_INTS + 1)
#define	IRQ_NMI			(NUM_IO_INTS + 2)
#define	IRQ_SMI			(NUM_IO_INTS + 3)
#define	IRQ_DISABLED		(NUM_IO_INTS + 4)

static MALLOC_DEFINE(M_IOAPIC, "io_apic", "I/O APIC structures");

/*
 * I/O APIC interrupt source driver.  Each pin is assigned an IRQ cookie
 * as laid out in the ACPI System Interrupt number model where each I/O
 * APIC has a contiguous chunk of the System Interrupt address space.
 * We assume that IRQs 1 - 15 behave like ISA IRQs and that all other
 * IRQs behave as PCI IRQs by default.  We also assume that the pin for
 * IRQ 0 is actually an ExtINT pin.  The apic enumerators override the
 * configuration of individual pins as indicated by their tables.
 *
 * Documentation for the I/O APIC: "82093AA I/O Advanced Programmable
 * Interrupt Controller (IOAPIC)", May 1996, Intel Corp.
 * ftp://download.intel.com/design/chipsets/datashts/29056601.pdf
 */

struct ioapic_intsrc {
	struct intsrc io_intsrc;
	u_int io_irq;
	u_int io_intpin:8;
	u_int io_vector:8;
	u_int io_cpu:8;
	u_int io_activehi:1;
	u_int io_edgetrigger:1;
	u_int io_masked:1;
	int io_bus:4;
	uint32_t io_lowreg;
};

struct ioapic {
	struct pic io_pic;
	u_int io_id:8;			/* logical ID */
	u_int io_apic_id:4;
	u_int io_intbase:8;		/* System Interrupt base */
	u_int io_numintr:8;
	volatile ioapic_t *io_addr;	/* XXX: should use bus_space */
	vm_paddr_t io_paddr;
	STAILQ_ENTRY(ioapic) io_next;
	struct ioapic_intsrc io_pins[0];
};

static u_int	ioapic_read(volatile ioapic_t *apic, int reg);
static void	ioapic_write(volatile ioapic_t *apic, int reg, u_int val);
static const char *ioapic_bus_string(int bus_type);
static void	ioapic_print_irq(struct ioapic_intsrc *intpin);
static void	ioapic_enable_source(struct intsrc *isrc);
static void	ioapic_disable_source(struct intsrc *isrc, int eoi);
static void	ioapic_eoi_source(struct intsrc *isrc);
static void	ioapic_enable_intr(struct intsrc *isrc);
static void	ioapic_disable_intr(struct intsrc *isrc);
static int	ioapic_vector(struct intsrc *isrc);
static int	ioapic_source_pending(struct intsrc *isrc);
static int	ioapic_config_intr(struct intsrc *isrc, enum intr_trigger trig,
		    enum intr_polarity pol);
static void	ioapic_resume(struct pic *pic);
static int	ioapic_assign_cpu(struct intsrc *isrc, u_int apic_id);
static void	ioapic_program_intpin(struct ioapic_intsrc *intpin);

static STAILQ_HEAD(,ioapic) ioapic_list = STAILQ_HEAD_INITIALIZER(ioapic_list);
struct pic ioapic_template = { ioapic_enable_source, ioapic_disable_source,
			       ioapic_eoi_source, ioapic_enable_intr,
			       ioapic_disable_intr, ioapic_vector,
			       ioapic_source_pending, NULL, ioapic_resume,
			       ioapic_config_intr, ioapic_assign_cpu };

static int next_ioapic_base;
static u_int next_id;

SYSCTL_NODE(_hw, OID_AUTO, apic, CTLFLAG_RD, 0, "APIC options");
static int enable_extint;
SYSCTL_INT(_hw_apic, OID_AUTO, enable_extint, CTLFLAG_RDTUN, &enable_extint, 0,
    "Enable the ExtINT pin in the first I/O APIC");
TUNABLE_INT("hw.apic.enable_extint", &enable_extint);

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
ioapic_print_irq(struct ioapic_intsrc *intpin)
{

	switch (intpin->io_irq) {
	case IRQ_DISABLED:
		printf("disabled");
		break;
	case IRQ_EXTINT:
		printf("ExtINT");
		break;
	case IRQ_NMI:
		printf("NMI");
		break;
	case IRQ_SMI:
		printf("SMI");
		break;
	default:
		printf("%s IRQ %u", ioapic_bus_string(intpin->io_bus),
		    intpin->io_irq);
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
		flags = intpin->io_lowreg & ~IOART_INTMASK;
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
		flags = intpin->io_lowreg | IOART_INTMSET;
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
	 * If a pin is completely invalid or if it is valid but hasn't
	 * been enabled yet, just ensure that the pin is masked.
	 */
	mtx_assert(&icu_lock, MA_OWNED);
	if (intpin->io_irq == IRQ_DISABLED || (intpin->io_irq < NUM_IO_INTS &&
	    intpin->io_vector == 0)) {
		low = ioapic_read(io->io_addr,
		    IOAPIC_REDTBL_LO(intpin->io_intpin));
		if ((low & IOART_INTMASK) == IOART_INTMCLR)
			ioapic_write(io->io_addr,
			    IOAPIC_REDTBL_LO(intpin->io_intpin),
			    low | IOART_INTMSET);
		return;
	}

	/* Set the destination. */
	low = IOART_DESTPHY;
	high = intpin->io_cpu << APIC_ID_SHIFT;

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
	switch (intpin->io_irq) {
	case IRQ_EXTINT:
		KASSERT(intpin->io_edgetrigger,
		    ("ExtINT not edge triggered"));
		low |= IOART_DELEXINT;
		break;
	case IRQ_NMI:
		KASSERT(intpin->io_edgetrigger,
		    ("NMI not edge triggered"));
		low |= IOART_DELNMI;
		break;
	case IRQ_SMI:
		KASSERT(intpin->io_edgetrigger,
		    ("SMI not edge triggered"));
		low |= IOART_DELSMI;
		break;
	default:
		KASSERT(intpin->io_vector != 0, ("No vector for IRQ %u",
		    intpin->io_irq));
		low |= IOART_DELFIXED | intpin->io_vector;
	}

	/* Write the values to the APIC. */
	intpin->io_lowreg = low;
	ioapic_write(io->io_addr, IOAPIC_REDTBL_LO(intpin->io_intpin), low);
	value = ioapic_read(io->io_addr, IOAPIC_REDTBL_HI(intpin->io_intpin));
	value &= ~IOART_DEST;
	value |= high;
	ioapic_write(io->io_addr, IOAPIC_REDTBL_HI(intpin->io_intpin), value);
}

static int
ioapic_assign_cpu(struct intsrc *isrc, u_int apic_id)
{
	struct ioapic_intsrc *intpin = (struct ioapic_intsrc *)isrc;
	struct ioapic *io = (struct ioapic *)isrc->is_pic;
	u_int old_vector, new_vector;
	u_int old_id;

	/*
	 * keep 1st core as the destination for NMI
	 */
	if (intpin->io_irq == IRQ_NMI)
		apic_id = 0;

	/*
	 * Set us up to free the old irq.
	 */
	old_vector = intpin->io_vector;
	old_id = intpin->io_cpu;
	if (old_vector && apic_id == old_id)
		return (0);

	/*
	 * Allocate an APIC vector for this interrupt pin.  Once
	 * we have a vector we program the interrupt pin.
	 */
	new_vector = apic_alloc_vector(apic_id, intpin->io_irq);
	if (new_vector == 0)
		return (ENOSPC);

	/*
	 * Mask the old intpin if it is enabled while it is migrated.
	 *
	 * At least some level-triggered interrupts seem to need the
	 * extra DELAY() to avoid being stuck in a non-EOI'd state.
	 */
	mtx_lock_spin(&icu_lock);
	if (!intpin->io_masked && !intpin->io_edgetrigger) {
		ioapic_write(io->io_addr, IOAPIC_REDTBL_LO(intpin->io_intpin),
		    intpin->io_lowreg | IOART_INTMSET);
		mtx_unlock_spin(&icu_lock);
		DELAY(100);
		mtx_lock_spin(&icu_lock);
	}

	intpin->io_cpu = apic_id;
	intpin->io_vector = new_vector;
	if (isrc->is_handlers > 0)
		apic_enable_vector(intpin->io_cpu, intpin->io_vector);
	if (bootverbose) {
		printf("ioapic%u: routing intpin %u (", io->io_id,
		    intpin->io_intpin);
		ioapic_print_irq(intpin);
		printf(") to lapic %u vector %u\n", intpin->io_cpu,
		    intpin->io_vector);
	}
	ioapic_program_intpin(intpin);
	mtx_unlock_spin(&icu_lock);

	/*
	 * Free the old vector after the new one is established.  This is done
	 * to prevent races where we could miss an interrupt.
	 */
	if (old_vector) {
		if (isrc->is_handlers > 0)
			apic_disable_vector(old_id, old_vector);
		apic_free_vector(old_id, old_vector, intpin->io_irq);
	}
	return (0);
}

static void
ioapic_enable_intr(struct intsrc *isrc)
{
	struct ioapic_intsrc *intpin = (struct ioapic_intsrc *)isrc;

	if (intpin->io_vector == 0)
		if (ioapic_assign_cpu(isrc, intr_next_cpu()) != 0)
			panic("Couldn't find an APIC vector for IRQ %d",
			    intpin->io_irq);
	apic_enable_vector(intpin->io_cpu, intpin->io_vector);
}


static void
ioapic_disable_intr(struct intsrc *isrc)
{
	struct ioapic_intsrc *intpin = (struct ioapic_intsrc *)isrc;
	u_int vector;

	if (intpin->io_vector != 0) {
		/* Mask this interrupt pin and free its APIC vector. */
		vector = intpin->io_vector;
		apic_disable_vector(intpin->io_cpu, vector);
		mtx_lock_spin(&icu_lock);
		intpin->io_masked = 1;
		intpin->io_vector = 0;
		ioapic_program_intpin(intpin);
		mtx_unlock_spin(&icu_lock);
		apic_free_vector(intpin->io_cpu, vector, intpin->io_irq);
	}
}

static int
ioapic_vector(struct intsrc *isrc)
{
	struct ioapic_intsrc *pin;

	pin = (struct ioapic_intsrc *)isrc;
	return (pin->io_irq);
}

static int
ioapic_source_pending(struct intsrc *isrc)
{
	struct ioapic_intsrc *intpin = (struct ioapic_intsrc *)isrc;

	if (intpin->io_vector == 0)
		return 0;
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
	mtx_lock_spin(&icu_lock);
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
	mtx_unlock_spin(&icu_lock);
	return (0);
}

static void
ioapic_resume(struct pic *pic)
{
	struct ioapic *io = (struct ioapic *)pic;
	int i;

	mtx_lock_spin(&icu_lock);
	for (i = 0; i < io->io_numintr; i++)
		ioapic_program_intpin(&io->io_pins[i]);
	mtx_unlock_spin(&icu_lock);
}

/*
 * Create a plain I/O APIC object.
 */
void *
ioapic_create(vm_paddr_t addr, int32_t apic_id, int intbase)
{
	struct ioapic *io;
	struct ioapic_intsrc *intpin;
	volatile ioapic_t *apic;
	u_int numintr, i;
	uint32_t value;

	/* Map the register window so we can access the device. */
	apic = pmap_mapdev(addr, IOAPIC_MEM_REGION);
	mtx_lock_spin(&icu_lock);
	value = ioapic_read(apic, IOAPIC_VER);
	mtx_unlock_spin(&icu_lock);

	/* If it's version register doesn't seem to work, punt. */
	if (value == 0xffffffff) {
		pmap_unmapdev((vm_offset_t)apic, IOAPIC_MEM_REGION);
		return (NULL);
	}

	/* Determine the number of vectors and set the APIC ID. */
	numintr = ((value & IOART_VER_MAXREDIR) >> MAXREDIRSHIFT) + 1;
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
	} else if (intbase != next_ioapic_base && bootverbose)
		printf("ioapic%u: WARNING: intbase %d != expected base %d\n",
		    io->io_id, intbase, next_ioapic_base);
	io->io_intbase = intbase;
	next_ioapic_base = intbase + numintr;
	io->io_numintr = numintr;
	io->io_addr = apic;
	io->io_paddr = addr;

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
		intpin->io_irq = intbase + i;

		/*
		 * Assume that pin 0 on the first I/O APIC is an ExtINT pin.
		 * Assume that pins 1-15 are ISA interrupts and that all
		 * other pins are PCI interrupts.
		 */
		if (intpin->io_irq == 0)
			ioapic_set_extint(io, i);
		else if (intpin->io_irq < IOAPIC_ISA_INTS) {
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
		 * Route interrupts to the BSP by default.  Interrupts may
		 * be routed to other CPUs later after they are enabled.
		 */
		intpin->io_cpu = PCPU_GET(apic_id);
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
	return (io->io_pins[pin].io_irq);
}

int
ioapic_disable_pin(void *cookie, u_int pin)
{
	struct ioapic *io;

	io = (struct ioapic *)cookie;
	if (pin >= io->io_numintr)
		return (EINVAL);
	if (io->io_pins[pin].io_irq == IRQ_DISABLED)
		return (EINVAL);
	io->io_pins[pin].io_irq = IRQ_DISABLED;
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
	if (io->io_pins[pin].io_irq >= NUM_IO_INTS)
		return (EINVAL);
	io->io_pins[pin].io_irq = vector;
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
	if (io->io_pins[pin].io_irq >= NUM_IO_INTS)
		return (EINVAL);
	if (io->io_pins[pin].io_bus == bus_type)
		return (0);
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
	if (io->io_pins[pin].io_irq == IRQ_NMI)
		return (0);
	if (io->io_pins[pin].io_irq >= NUM_IO_INTS)
		return (EINVAL);
	io->io_pins[pin].io_bus = APIC_BUS_UNKNOWN;
	io->io_pins[pin].io_irq = IRQ_NMI;
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
	if (io->io_pins[pin].io_irq == IRQ_SMI)
		return (0);
	if (io->io_pins[pin].io_irq >= NUM_IO_INTS)
		return (EINVAL);
	io->io_pins[pin].io_bus = APIC_BUS_UNKNOWN;
	io->io_pins[pin].io_irq = IRQ_SMI;
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
	if (io->io_pins[pin].io_irq == IRQ_EXTINT)
		return (0);
	if (io->io_pins[pin].io_irq >= NUM_IO_INTS)
		return (EINVAL);
	io->io_pins[pin].io_bus = APIC_BUS_UNKNOWN;
	io->io_pins[pin].io_irq = IRQ_EXTINT;
	if (enable_extint)
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
	int activehi;

	io = (struct ioapic *)cookie;
	if (pin >= io->io_numintr || pol == INTR_POLARITY_CONFORM)
		return (EINVAL);
	if (io->io_pins[pin].io_irq >= NUM_IO_INTS)
		return (EINVAL);
	activehi = (pol == INTR_POLARITY_HIGH);
	if (io->io_pins[pin].io_activehi == activehi)
		return (0);
	io->io_pins[pin].io_activehi = activehi;
	if (bootverbose)
		printf("ioapic%u: intpin %d polarity: %s\n", io->io_id, pin,
		    pol == INTR_POLARITY_HIGH ? "high" : "low");
	return (0);
}

int
ioapic_set_triggermode(void *cookie, u_int pin, enum intr_trigger trigger)
{
	struct ioapic *io;
	int edgetrigger;

	io = (struct ioapic *)cookie;
	if (pin >= io->io_numintr || trigger == INTR_TRIGGER_CONFORM)
		return (EINVAL);
	if (io->io_pins[pin].io_irq >= NUM_IO_INTS)
		return (EINVAL);
	edgetrigger = (trigger == INTR_TRIGGER_EDGE);
	if (io->io_pins[pin].io_edgetrigger == edgetrigger)
		return (0);
	io->io_pins[pin].io_edgetrigger = edgetrigger;
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

	/* Register valid pins as interrupt sources. */
	intr_register_pic(&io->io_pic);
	for (i = 0, pin = io->io_pins; i < io->io_numintr; i++, pin++)
		if (pin->io_irq < NUM_IO_INTS)
			intr_register_source(&pin->io_intsrc);
}

/* A simple new-bus driver to consume PCI I/O APIC devices. */
static int
ioapic_pci_probe(device_t dev)
{

	if (pci_get_class(dev) == PCIC_BASEPERIPH &&
	    pci_get_subclass(dev) == PCIS_BASEPERIPH_PIC) {
		switch (pci_get_progif(dev)) {
		case PCIP_BASEPERIPH_PIC_IO_APIC:
			device_set_desc(dev, "IO APIC");
			break;
		case PCIP_BASEPERIPH_PIC_IOX_APIC:
			device_set_desc(dev, "IO(x) APIC");
			break;
		default:
			return (ENXIO);
		}
		device_quiet(dev);
		return (-10000);
	}
	return (ENXIO);
}

static int
ioapic_pci_attach(device_t dev)
{

	return (0);
}

static device_method_t ioapic_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ioapic_pci_probe),
	DEVMETHOD(device_attach,	ioapic_pci_attach),

	{ 0, 0 }
};

DEFINE_CLASS_0(ioapic, ioapic_pci_driver, ioapic_pci_methods, 0);

static devclass_t ioapic_devclass;
DRIVER_MODULE(ioapic, pci, ioapic_pci_driver, ioapic_devclass, 0, 0);

/*
 * A new-bus driver to consume the memory resources associated with
 * the APICs in the system.  On some systems ACPI or PnPBIOS system
 * resource devices may already claim these resources.  To keep from
 * breaking those devices, we attach ourself to the nexus device after
 * legacy0 and acpi0 and ignore any allocation failures.
 */
static void
apic_identify(driver_t *driver, device_t parent)
{

	/*
	 * Add at order 12.  acpi0 is probed at order 10 and legacy0
	 * is probed at order 11.
	 */
	if (lapic_paddr != 0)
		BUS_ADD_CHILD(parent, 12, "apic", 0);
}

static int
apic_probe(device_t dev)
{

	device_set_desc(dev, "APIC resources");
	device_quiet(dev);
	return (0);
}

static void
apic_add_resource(device_t dev, int rid, vm_paddr_t base, size_t length)
{
	int error;

	error = bus_set_resource(dev, SYS_RES_MEMORY, rid, base, length);
	if (error)
		panic("apic_add_resource: resource %d failed set with %d", rid,
		    error);
	bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, 0);
}

static int
apic_attach(device_t dev)
{
	struct ioapic *io;
	int i;

	/* Reserve the local APIC. */
	apic_add_resource(dev, 0, lapic_paddr, sizeof(lapic_t));
	i = 1;
	STAILQ_FOREACH(io, &ioapic_list, io_next) {
		apic_add_resource(dev, i, io->io_paddr, IOAPIC_MEM_REGION);
		i++;
	}
	return (0);
}

static device_method_t apic_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,	apic_identify),
	DEVMETHOD(device_probe,		apic_probe),
	DEVMETHOD(device_attach,	apic_attach),

	{ 0, 0 }
};

DEFINE_CLASS_0(apic, apic_driver, apic_methods, 0);

static devclass_t apic_devclass;
DRIVER_MODULE(apic, nexus, apic_driver, apic_devclass, 0, 0);
