/*-
 * Copyright (c) 2013 Tycho Nightingale <tycho.nightingale@pluribusnetworks.com>
 * Copyright (c) 2013 Neel Natu <neel@freebsd.org>
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
 *
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/cpuset.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <x86/apicreg.h>
#include <machine/vmm.h>

#include "vmm_ktr.h"
#include "vmm_lapic.h"
#include "vioapic.h"

#define	IOREGSEL	0x00
#define	IOWIN		0x10

#define	REDIR_ENTRIES	16
#define	INTR_ASSERTED(vioapic, pin) ((vioapic)->rtbl[(pin)].pinstate == true)

struct vioapic {
	struct vm	*vm;
	struct mtx	mtx;
	uint32_t	id;
	uint32_t	ioregsel;
	struct {
		uint64_t reg;
		bool     pinstate;
		bool     pending;
	} rtbl[REDIR_ENTRIES];
};

#define	VIOAPIC_LOCK(vioapic)		mtx_lock(&((vioapic)->mtx))
#define	VIOAPIC_UNLOCK(vioapic)		mtx_unlock(&((vioapic)->mtx))
#define	VIOAPIC_LOCKED(vioapic)		mtx_owned(&((vioapic)->mtx))

static MALLOC_DEFINE(M_VIOAPIC, "vioapic", "bhyve virtual ioapic");

#define	VIOAPIC_CTR1(vioapic, fmt, a1)					\
	VM_CTR1((vioapic)->vm, fmt, a1)

#define	VIOAPIC_CTR2(vioapic, fmt, a1, a2)				\
	VM_CTR2((vioapic)->vm, fmt, a1, a2)

#define	VIOAPIC_CTR3(vioapic, fmt, a1, a2, a3)				\
	VM_CTR3((vioapic)->vm, fmt, a1, a2, a3)

#ifdef KTR
static const char *
pinstate_str(bool asserted)
{

	if (asserted)
		return ("asserted");
	else
		return ("deasserted");
}
#endif

static void
vioapic_set_pinstate(struct vioapic *vioapic, int pin, bool newstate)
{
	int vector, apicid, vcpuid;
	uint32_t low, high;
	cpuset_t dmask;

	KASSERT(pin >= 0 && pin < REDIR_ENTRIES,
	    ("vioapic_set_pinstate: invalid pin number %d", pin));

	KASSERT(VIOAPIC_LOCKED(vioapic),
	    ("vioapic_set_pinstate: vioapic is not locked"));

	VIOAPIC_CTR2(vioapic, "ioapic pin%d %s", pin, pinstate_str(newstate));

	/* Nothing to do if interrupt pin has not changed state */
	if (vioapic->rtbl[pin].pinstate == newstate)
		return;

	vioapic->rtbl[pin].pinstate = newstate;	/* record it */

	/* Nothing to do if interrupt pin is deasserted */
	if (!INTR_ASSERTED(vioapic, pin))
		return;

	/*
	 * XXX
	 * We only deal with:
	 * - edge triggered interrupts
	 * - fixed delivery mode
	 *  Level-triggered sources will work so long as there is no sharing.
	 */
	low = vioapic->rtbl[pin].reg;
	high = vioapic->rtbl[pin].reg >> 32;
	if ((low & IOART_INTMASK) == IOART_INTMCLR &&
	    (low & IOART_DESTMOD) == IOART_DESTPHY &&
	    (low & IOART_DELMOD) == IOART_DELFIXED) {
		vector = low & IOART_INTVEC;
		apicid = high >> APIC_ID_SHIFT;
		if (apicid != 0xff) {
			/* unicast */
			vcpuid = vm_apicid2vcpuid(vioapic->vm, apicid);
			VIOAPIC_CTR3(vioapic, "ioapic pin%d triggering "
			    "intr vector %d on vcpuid %d", pin, vector, vcpuid);
			lapic_set_intr(vioapic->vm, vcpuid, vector);
		} else {
			/* broadcast */
			VIOAPIC_CTR2(vioapic, "ioapic pin%d triggering intr "
			    "vector %d on all vcpus", pin, vector);
			dmask = vm_active_cpus(vioapic->vm);
			while ((vcpuid = CPU_FFS(&dmask)) != 0) {
				vcpuid--;
				CPU_CLR(vcpuid, &dmask);
				lapic_set_intr(vioapic->vm, vcpuid, vector);
			}
		}
	} else if ((low & IOART_INTMASK) != IOART_INTMCLR &&
		   (low & IOART_TRGRLVL) != 0) {
		/*
		 * For level-triggered interrupts that have been
		 * masked, set the pending bit so that an interrupt
		 * will be generated on unmask and if the level is
		 * still asserted
		 */
		VIOAPIC_CTR1(vioapic, "ioapic pin%d interrupt pending", pin);
		vioapic->rtbl[pin].pending = true;
	}
}

static int
vioapic_set_irqstate(struct vm *vm, int irq, bool state)
{
	struct vioapic *vioapic;

	if (irq < 0 || irq >= REDIR_ENTRIES)
		return (EINVAL);

	vioapic = vm_ioapic(vm);

	VIOAPIC_LOCK(vioapic);
	vioapic_set_pinstate(vioapic, irq, state);
	VIOAPIC_UNLOCK(vioapic);

	return (0);
}

int
vioapic_assert_irq(struct vm *vm, int irq)
{

	return (vioapic_set_irqstate(vm, irq, true));
}

int
vioapic_deassert_irq(struct vm *vm, int irq)
{

	return (vioapic_set_irqstate(vm, irq, false));
}

static uint32_t
vioapic_read(struct vioapic *vioapic, uint32_t addr)
{
	int regnum, pin, rshift;

	regnum = addr & 0xff;
	switch (regnum) {
	case IOAPIC_ID:
		return (vioapic->id);
		break;
	case IOAPIC_VER:
		return ((REDIR_ENTRIES << MAXREDIRSHIFT) | 0x11);
		break;
	case IOAPIC_ARB:
		return (vioapic->id);
		break;
	default:
		break;
	}

	/* redirection table entries */
	if (regnum >= IOAPIC_REDTBL &&
	    regnum < IOAPIC_REDTBL + REDIR_ENTRIES * 2) {
		pin = (regnum - IOAPIC_REDTBL) / 2;
		if ((regnum - IOAPIC_REDTBL) % 2)
			rshift = 32;
		else
			rshift = 0;

		return (vioapic->rtbl[pin].reg >> rshift);
	}

	return (0);
}

static void
vioapic_write(struct vioapic *vioapic, uint32_t addr, uint32_t data)
{
	int regnum, pin, lshift;

	regnum = addr & 0xff;
	switch (regnum) {
	case IOAPIC_ID:
		vioapic->id = data & APIC_ID_MASK;
		break;
	case IOAPIC_VER:
	case IOAPIC_ARB:
		/* readonly */
		break;
	default:
		break;
	}

	/* redirection table entries */
	if (regnum >= IOAPIC_REDTBL &&
	    regnum < IOAPIC_REDTBL + REDIR_ENTRIES * 2) {
		pin = (regnum - IOAPIC_REDTBL) / 2;
		if ((regnum - IOAPIC_REDTBL) % 2)
			lshift = 32;
		else
			lshift = 0;

		vioapic->rtbl[pin].reg &= ~((uint64_t)0xffffffff << lshift);
		vioapic->rtbl[pin].reg |= ((uint64_t)data << lshift);

		VIOAPIC_CTR2(vioapic, "ioapic pin%d redir table entry %#lx",
		    pin, vioapic->rtbl[pin].reg);

		if (vioapic->rtbl[pin].pending &&
		    ((vioapic->rtbl[pin].reg & IOART_INTMASK) ==
		    IOART_INTMCLR)) {
			vioapic->rtbl[pin].pending = false;
			/*
			 * Inject the deferred level-triggered int if it is
			 * still asserted. Simulate by toggling the pin
			 * off and then on.
			 */
			if (vioapic->rtbl[pin].pinstate == true) {
				VIOAPIC_CTR1(vioapic, "ioapic pin%d pending "
				    "interrupt delivered", pin);
				vioapic_set_pinstate(vioapic, pin, false);
				vioapic_set_pinstate(vioapic, pin, true);
			} else {
				VIOAPIC_CTR1(vioapic, "ioapic pin%d pending "
				    "interrupt dismissed", pin);
			}
		}
	}
}

static int
vioapic_mmio_rw(struct vioapic *vioapic, uint64_t gpa, uint64_t *data,
    int size, bool doread)
{
	uint64_t offset;

	offset = gpa - VIOAPIC_BASE;

	/*
	 * The IOAPIC specification allows 32-bit wide accesses to the
	 * IOREGSEL (offset 0) and IOWIN (offset 16) registers.
	 */
	if (size != 4 || (offset != IOREGSEL && offset != IOWIN)) {
		if (doread)
			*data = 0;
		return (0);
	}

	VIOAPIC_LOCK(vioapic);
	if (offset == IOREGSEL) {
		if (doread)
			*data = vioapic->ioregsel;
		else
			vioapic->ioregsel = *data;
	} else {
		if (doread)
			*data = vioapic_read(vioapic, vioapic->ioregsel);
		else
			vioapic_write(vioapic, vioapic->ioregsel, *data);
	}
	VIOAPIC_UNLOCK(vioapic);

	return (0);
}

int
vioapic_mmio_read(void *vm, int vcpuid, uint64_t gpa, uint64_t *rval,
    int size, void *arg)
{
	int error;
	struct vioapic *vioapic;

	vioapic = vm_ioapic(vm);
	error = vioapic_mmio_rw(vioapic, gpa, rval, size, true);
	return (error);
}

int
vioapic_mmio_write(void *vm, int vcpuid, uint64_t gpa, uint64_t wval,
    int size, void *arg)
{
	int error;
	struct vioapic *vioapic;

	vioapic = vm_ioapic(vm);
	error = vioapic_mmio_rw(vioapic, gpa, &wval, size, false);
	return (error);
}

struct vioapic *
vioapic_init(struct vm *vm)
{
	int i;
	struct vioapic *vioapic;

	vioapic = malloc(sizeof(struct vioapic), M_VIOAPIC, M_WAITOK | M_ZERO);

	vioapic->vm = vm;
	mtx_init(&vioapic->mtx, "vioapic lock", NULL, MTX_DEF);

	/* Initialize all redirection entries to mask all interrupts */
	for (i = 0; i < REDIR_ENTRIES; i++)
		vioapic->rtbl[i].reg = 0x0001000000010000UL;

	return (vioapic);
}

void
vioapic_cleanup(struct vioapic *vioapic)
{

	free(vioapic, M_VIOAPIC);
}
