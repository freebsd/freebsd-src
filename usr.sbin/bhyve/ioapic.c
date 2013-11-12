/*-
 * Copyright (c) 2012 NetApp, Inc.
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

#include <sys/types.h>

#include <x86/apicreg.h>
#include <machine/vmm.h>

#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <pthread.h>

#include <vmmapi.h>

#include "inout.h"
#include "mem.h"
#include "bhyverun.h"

#include <stdio.h>

static uint64_t ioapic_clearpend, ioapic_togglepend, ioapic_setpend;

#define	IOAPIC_PADDR	0xFEC00000

#define	IOREGSEL	0x00
#define	IOWIN		0x10

#define	REDIR_ENTRIES	16
#define	INTR_ASSERTED(ioapic, pin)	\
	((ioapic)->rtbl[(pin)].pinstate == true)

struct ioapic {
	int		inited;
	uint32_t	id;
	struct {
		uint64_t reg;
		bool     pinstate;
		bool     pending;
	} rtbl[REDIR_ENTRIES];

	uintptr_t	paddr;		/* gpa where the ioapic is mapped */
	uint32_t	ioregsel;
	struct memory_region *region;
	pthread_mutex_t	mtx;
};

static struct ioapic ioapics[1];	/* only a single ioapic for now */

static int ioapic_region_read(struct vmctx *vm, struct ioapic *ioapic,
    uintptr_t paddr, int size, uint64_t *data);
static int ioapic_region_write(struct vmctx *vm, struct ioapic *ioapic,
    uintptr_t paddr, int size, uint64_t data);
static int ioapic_region_handler(struct vmctx *vm, int vcpu, int dir,
    uintptr_t paddr, int size, uint64_t *val, void *arg1, long arg2);

static void
ioapic_set_pinstate(struct vmctx *ctx, int pin, bool newstate)
{
	int vector, apicid, vcpu;
	uint32_t low, high;
	struct ioapic *ioapic;
	
	ioapic = &ioapics[0];		/* assume a single ioapic */

	/* Nothing to do if interrupt pin has not changed state */
	if (ioapic->rtbl[pin].pinstate == newstate)
		return;

	ioapic->rtbl[pin].pinstate = newstate;	/* record it */

	/* Nothing to do if interrupt pin is deasserted */
	if (!INTR_ASSERTED(ioapic, pin))
		return;

	/*
	 * XXX
	 * We only deal with:
	 * - edge triggered interrupts
	 * - fixed delivery mode
	 *  Level-triggered sources will work so long as there is
	 * no sharing.
	 */
	low = ioapic->rtbl[pin].reg;
	high = ioapic->rtbl[pin].reg >> 32;
	if ((low & IOART_INTMASK) == IOART_INTMCLR &&
	    (low & IOART_DESTMOD) == IOART_DESTPHY &&
	    (low & IOART_DELMOD) == IOART_DELFIXED) {
		vector = low & IOART_INTVEC;
		apicid = high >> APIC_ID_SHIFT;
		if (apicid != 0xff) {
			/* unicast */
			vcpu = vm_apicid2vcpu(ctx, apicid);
			vm_lapic_irq(ctx, vcpu, vector);
		} else {
			/* broadcast */
			vcpu = 0;
			while (vcpu < guest_ncpus) {
				vm_lapic_irq(ctx, vcpu, vector);
				vcpu++;
			}
		}
	} else if ((low & IOART_INTMASK) != IOART_INTMCLR &&
		   low & IOART_TRGRLVL) {
		/*
		 * For level-triggered interrupts that have been
		 * masked, set the pending bit so that an interrupt
		 * will be generated on unmask and if the level is
		 * still asserted
		 */
		ioapic_setpend++;
		ioapic->rtbl[pin].pending = true;
	}
}

static void
ioapic_set_pinstate_locked(struct vmctx *ctx, int pin, bool newstate)
{
	struct ioapic *ioapic;

	if (pin < 0 || pin >= REDIR_ENTRIES)
		return;

	ioapic = &ioapics[0];

	pthread_mutex_lock(&ioapic->mtx);
	ioapic_set_pinstate(ctx, pin, newstate);
	pthread_mutex_unlock(&ioapic->mtx);
}

/*
 * External entry points require locking
 */
void
ioapic_deassert_pin(struct vmctx *ctx, int pin)
{
	ioapic_set_pinstate_locked(ctx, pin, false);
}

void
ioapic_assert_pin(struct vmctx *ctx, int pin)
{
	ioapic_set_pinstate_locked(ctx, pin, true);
}

void
ioapic_init(int which)
{
	struct mem_range memp;
	struct ioapic *ioapic;
	int error;
	int i;

	assert(which == 0);

	ioapic = &ioapics[which];
	assert(ioapic->inited == 0);

	bzero(ioapic, sizeof(struct ioapic));

	pthread_mutex_init(&ioapic->mtx, NULL);

	/* Initialize all redirection entries to mask all interrupts */
	for (i = 0; i < REDIR_ENTRIES; i++)
		ioapic->rtbl[i].reg = 0x0001000000010000UL;

	ioapic->paddr = IOAPIC_PADDR;

	/* Register emulated memory region */
	memp.name = "ioapic";
	memp.flags = MEM_F_RW;
	memp.handler = ioapic_region_handler;
	memp.arg1 = ioapic;
	memp.arg2 = which;
	memp.base = ioapic->paddr;
	memp.size = sizeof(struct IOAPIC);
	error = register_mem(&memp);

	assert (error == 0);

	ioapic->inited = 1;
}

static uint32_t
ioapic_read(struct ioapic *ioapic, uint32_t addr)
{
	int regnum, pin, rshift;

	assert(ioapic->inited);

	regnum = addr & 0xff;
	switch (regnum) {
	case IOAPIC_ID:
		return (ioapic->id);
		break;
	case IOAPIC_VER:
		return ((REDIR_ENTRIES << MAXREDIRSHIFT) | 0x11);
		break;
	case IOAPIC_ARB:
		return (ioapic->id);
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

		return (ioapic->rtbl[pin].reg >> rshift);
	}

	return (0);
}

static void
ioapic_write(struct vmctx *vm, struct ioapic *ioapic, uint32_t addr,
    uint32_t data)
{
	int regnum, pin, lshift;

	assert(ioapic->inited);

	regnum = addr & 0xff;
	switch (regnum) {
	case IOAPIC_ID:
		ioapic->id = data & APIC_ID_MASK;
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

		ioapic->rtbl[pin].reg &= ~((uint64_t)0xffffffff << lshift);
		ioapic->rtbl[pin].reg |= ((uint64_t)data << lshift);
	
		if (ioapic->rtbl[pin].pending &&
		    ((ioapic->rtbl[pin].reg & IOART_INTMASK) ==
		         IOART_INTMCLR)) {
			ioapic->rtbl[pin].pending = false;
			ioapic_clearpend++;
			/*
			 * Inject the deferred level-triggered int if it is
			 * still asserted. Simulate by toggling the pin
			 * off and then on.
			 */
			if (ioapic->rtbl[pin].pinstate == true) {
				ioapic_togglepend++;
				ioapic_set_pinstate(vm, pin, false);
				ioapic_set_pinstate(vm, pin, true);
			}
		}
	}
}

static int
ioapic_region_read(struct vmctx *vm, struct ioapic *ioapic, uintptr_t paddr,
    int size, uint64_t *data)
{
	int offset;

	offset = paddr - ioapic->paddr;

	/*
	 * The IOAPIC specification allows 32-bit wide accesses to the
	 * IOREGSEL (offset 0) and IOWIN (offset 16) registers.
	 */
	if (size != 4 || (offset != IOREGSEL && offset != IOWIN)) {
#if 1
		printf("invalid access to ioapic%d: size %d, offset %d\n",
		       (int)(ioapic - ioapics), size, offset);
#endif
		*data = 0;
		return (0);
	}

	if (offset == IOREGSEL)
		*data = ioapic->ioregsel;
	else
		*data = ioapic_read(ioapic, ioapic->ioregsel);

	return (0);
}

static int
ioapic_region_write(struct vmctx *vm, struct ioapic *ioapic, uintptr_t paddr,
    int size, uint64_t data)
{
	int offset;

	offset = paddr - ioapic->paddr;

	/*
	 * The ioapic specification allows 32-bit wide accesses to the
	 * IOREGSEL (offset 0) and IOWIN (offset 16) registers.
	 */
	if (size != 4 || (offset != IOREGSEL && offset != IOWIN)) {
#if 1
		printf("invalid access to ioapic%d: size %d, offset %d\n",
		       (int)(ioapic - ioapics), size, offset);
#endif
		return (0);
	}

	if (offset == IOREGSEL)
		ioapic->ioregsel = data;
	else
		ioapic_write(vm, ioapic, ioapic->ioregsel, data);

	return (0);
}

static int
ioapic_region_handler(struct vmctx *vm, int vcpu, int dir, uintptr_t paddr,
    int size, uint64_t *val, void *arg1, long arg2)
{
	struct ioapic *ioapic;
	int which;

	ioapic = arg1;
	which = arg2;

	assert(ioapic == &ioapics[which]);

	pthread_mutex_lock(&ioapic->mtx);
	if (dir == MEM_F_READ)
		ioapic_region_read(vm, ioapic, paddr, size, val);
	else
		ioapic_region_write(vm, ioapic, paddr, size, *val);
	pthread_mutex_unlock(&ioapic->mtx);

	return (0);
}
