/*-
 * Copyright (c) 2014 Tycho Nightingale <tycho.nightingale@pluribusnetworks.com>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/cpuset.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/systm.h>

#include <x86/apicreg.h>
#include <dev/ic/i8259.h>

#include <machine/vmm.h>

#include "vmm_ktr.h"
#include "vmm_lapic.h"
#include "vioapic.h"
#include "vatpic.h"

static MALLOC_DEFINE(M_VATPIC, "atpic", "bhyve virtual atpic (8259)");

#define	VATPIC_LOCK(vatpic)		mtx_lock_spin(&((vatpic)->mtx))
#define	VATPIC_UNLOCK(vatpic)		mtx_unlock_spin(&((vatpic)->mtx))
#define	VATPIC_LOCKED(vatpic)		mtx_owned(&((vatpic)->mtx))

enum irqstate {
	IRQSTATE_ASSERT,
	IRQSTATE_DEASSERT,
	IRQSTATE_PULSE
};

struct atpic {
	bool		ready;
	int		icw_num;
	int		rd_cmd_reg;

	bool		aeoi;
	bool		poll;
	bool		rotate;

	int		irq_base;
	uint8_t		request;	/* Interrupt Request Register (IIR) */
	uint8_t		service;	/* Interrupt Service (ISR) */
	uint8_t		mask;		/* Interrupt Mask Register (IMR) */

	int		acnt[8];	/* sum of pin asserts and deasserts */
	int		priority;	/* current pin priority */
};

struct vatpic {
	struct vm	*vm;
	struct mtx	mtx;
	struct atpic	atpic[2];
	uint8_t		elc[2];

	bool		intr_raised;
};

#define	VATPIC_CTR0(vatpic, fmt)					\
	VM_CTR0((vatpic)->vm, fmt)

#define	VATPIC_CTR1(vatpic, fmt, a1)					\
	VM_CTR1((vatpic)->vm, fmt, a1)

#define	VATPIC_CTR2(vatpic, fmt, a1, a2)				\
	VM_CTR2((vatpic)->vm, fmt, a1, a2)

#define	VATPIC_CTR3(vatpic, fmt, a1, a2, a3)				\
	VM_CTR3((vatpic)->vm, fmt, a1, a2, a3)

#define	VATPIC_CTR4(vatpic, fmt, a1, a2, a3, a4)			\
	VM_CTR4((vatpic)->vm, fmt, a1, a2, a3, a4)


static __inline int
vatpic_get_highest_isrpin(struct atpic *atpic)
{
	int bit, pin;
	int i;

	for (i = 0; i <= 7; i++) {
		pin = ((i + 7 - atpic->priority) & 0x7);
                bit = (1 << pin);

		if (atpic->service & bit)
			return (pin);
	}

	return (-1);
}

static __inline int
vatpic_get_highest_irrpin(struct atpic *atpic)
{
	int bit, pin;
	int i, j;

	for (i = 0; i <= 7; i++) {
		pin = ((i + 7 - atpic->priority) & 0x7);
		bit = (1 << pin);
		if (atpic->service & bit)
			break;
	}

	for (j = 0; j < i; j++) {
		pin = ((j + 7 - atpic->priority) & 0x7);
		bit = (1 << pin);
		if (atpic->request & bit && (~atpic->mask & bit))
			return (pin);
	}

	return (-1);
}

static void
vatpic_notify_intr(struct vatpic *vatpic)
{
	struct atpic *atpic;
	int pin;

	KASSERT(VATPIC_LOCKED(vatpic), ("vatpic_notify_intr not locked"));

	if (vatpic->intr_raised == true)
		return;

	/* XXX master only */
	atpic = &vatpic->atpic[0];

	if ((pin = vatpic_get_highest_irrpin(atpic)) != -1) {
		VATPIC_CTR4(vatpic, "atpic notify pin = %d "
		    "(imr 0x%x irr 0x%x isr 0x%x)", pin,
		    atpic->mask, atpic->request, atpic->service);

		/*
		 * PIC interrupts are routed to both the Local APIC
		 * and the I/O APIC to support operation in 1 of 3
		 * modes.
		 *
		 * 1. Legacy PIC Mode: the PIC effectively bypasses
		 * all APIC components.  In mode '1' the local APIC is
		 * disabled and LINT0 is reconfigured as INTR to
		 * deliver the PIC interrupt directly to the CPU.
		 *
		 * 2. Virtual Wire Mode: the APIC is treated as a
		 * virtual wire which delivers interrupts from the PIC
		 * to the CPU.  In mode '2' LINT0 is programmed as
		 * ExtINT to indicate that the PIC is the source of
		 * the interrupt.
		 *
		 * 3. Symmetric I/O Mode: PIC interrupts are fielded
		 * by the I/O APIC and delivered to the appropriate
		 * CPU.  In mode '3' the I/O APIC input 0 is
		 * programmed as ExtINT to indicate that the PIC is
		 * the source of the interrupt.
		 */
		lapic_set_local_intr(vatpic->vm, -1, APIC_LVT_LINT0);
		vioapic_pulse_irq(vatpic->vm, 0);
		vatpic->intr_raised = true;
	} else {
		VATPIC_CTR3(vatpic, "atpic no eligible interrupts "
		    "(imr 0x%x irr 0x%x isr 0x%x)",
		    atpic->mask, atpic->request, atpic->service);
	}
}

static int
vatpic_icw1(struct vatpic *vatpic, struct atpic *atpic, uint8_t val)
{
	VATPIC_CTR1(vatpic, "atpic icw1 0x%x", val);

	atpic->ready = false;

	atpic->icw_num = 1;
	atpic->mask = 0;
	atpic->priority = 0;
	atpic->rd_cmd_reg = 0;

	if ((val & ICW1_SNGL) != 0) {
		VATPIC_CTR0(vatpic, "vatpic cascade mode required");
		return (-1);
	}

	if ((val & ICW1_IC4) == 0) {
		VATPIC_CTR0(vatpic, "vatpic icw4 required");
		return (-1);
	}

	atpic->icw_num++;

	return (0);
}

static int
vatpic_icw2(struct vatpic *vatpic, struct atpic *atpic, uint8_t val)
{
	VATPIC_CTR1(vatpic, "atpic icw2 0x%x", val);

	atpic->irq_base = val & 0xf8;

	atpic->icw_num++;

	return (0);
}

static int
vatpic_icw3(struct vatpic *vatpic, struct atpic *atpic, uint8_t val)
{
	VATPIC_CTR1(vatpic, "atpic icw3 0x%x", val);

	atpic->icw_num++;

	return (0);
}

static int
vatpic_icw4(struct vatpic *vatpic, struct atpic *atpic, uint8_t val)
{
	VATPIC_CTR1(vatpic, "atpic icw4 0x%x", val);

	if ((val & ICW4_8086) == 0) {
		VATPIC_CTR0(vatpic, "vatpic microprocessor mode required");
		return (-1);
	}

	if ((val & ICW4_AEOI) != 0)
		atpic->aeoi = true;

	atpic->icw_num = 0;
	atpic->ready = true;

	return (0);
}

static int
vatpic_ocw1(struct vatpic *vatpic, struct atpic *atpic, uint8_t val)
{
	VATPIC_CTR1(vatpic, "atpic ocw1 0x%x", val);

	atpic->mask = val & 0xff;

	return (0);
}

static int
vatpic_ocw2(struct vatpic *vatpic, struct atpic *atpic, uint8_t val)
{
	VATPIC_CTR1(vatpic, "atpic ocw2 0x%x", val);

	atpic->rotate = ((val & OCW2_R) != 0);

	if ((val & OCW2_EOI) != 0) {
		int isr_bit;

		if ((val & OCW2_SL) != 0) {
			/* specific EOI */
			isr_bit = val & 0x7;
		} else {
			/* non-specific EOI */
			isr_bit = vatpic_get_highest_isrpin(atpic);
		}

		if (isr_bit != -1) {
			atpic->service &= ~(1 << isr_bit);

			if (atpic->rotate)
				atpic->priority = isr_bit;
		}
	} else if ((val & OCW2_SL) != 0 && atpic->rotate == true) {
		/* specific priority */
		atpic->priority = val & 0x7;
	}

	return (0);
}

static int
vatpic_ocw3(struct vatpic *vatpic, struct atpic *atpic, uint8_t val)
{
	VATPIC_CTR1(vatpic, "atpic ocw3 0x%x", val);

	atpic->poll = ((val & OCW3_P) != 0);

	if (val & OCW3_RR) {
		/* read register command */
		atpic->rd_cmd_reg = val & OCW3_RIS;
	}

	return (0);
}

static void
vatpic_set_pinstate(struct vatpic *vatpic, int pin, bool newstate)
{
	struct atpic *atpic;
	int oldcnt, newcnt;
	bool level;

	KASSERT(pin >= 0 && pin < 16,
	    ("vatpic_set_pinstate: invalid pin number %d", pin));
	KASSERT(VATPIC_LOCKED(vatpic),
	    ("vatpic_set_pinstate: vatpic is not locked"));

	atpic = &vatpic->atpic[pin >> 3];

	oldcnt = atpic->acnt[pin & 0x7];
	if (newstate)
		atpic->acnt[pin & 0x7]++;
	else
		atpic->acnt[pin & 0x7]--;
	newcnt = atpic->acnt[pin & 0x7];

	if (newcnt < 0) {
		VATPIC_CTR2(vatpic, "atpic pin%d: bad acnt %d", pin, newcnt);
	}

	level = ((vatpic->elc[pin >> 3] & (1 << (pin & 0x7))) != 0);

	if ((oldcnt == 0 && newcnt == 1) || (newcnt > 0 && level == true)) {
		/* rising edge or level */
		VATPIC_CTR1(vatpic, "atpic pin%d: asserted", pin);
		atpic->request |= (1 << (pin & 0x7));
	} else if (oldcnt == 1 && newcnt == 0) {
		/* falling edge */
		VATPIC_CTR1(vatpic, "atpic pin%d: deasserted", pin);
	} else {
		VATPIC_CTR3(vatpic, "atpic pin%d: %s, ignored, acnt %d",
		    pin, newstate ? "asserted" : "deasserted", newcnt);
	}

	vatpic_notify_intr(vatpic);
}

static int
vatpic_set_irqstate(struct vm *vm, int irq, enum irqstate irqstate)
{
	struct vatpic *vatpic;
	struct atpic *atpic;

	if (irq < 0 || irq > 15)
		return (EINVAL);

	vatpic = vm_atpic(vm);
	atpic = &vatpic->atpic[irq >> 3];

	if (atpic->ready == false)
		return (0);

	VATPIC_LOCK(vatpic);
	switch (irqstate) {
	case IRQSTATE_ASSERT:
		vatpic_set_pinstate(vatpic, irq, true);
		break;
	case IRQSTATE_DEASSERT:
		vatpic_set_pinstate(vatpic, irq, false);
		break;
	case IRQSTATE_PULSE:
		vatpic_set_pinstate(vatpic, irq, true);
		vatpic_set_pinstate(vatpic, irq, false);
		break;
	default:
		panic("vatpic_set_irqstate: invalid irqstate %d", irqstate);
	}
	VATPIC_UNLOCK(vatpic);

	return (0);
}

int
vatpic_assert_irq(struct vm *vm, int irq)
{
	return (vatpic_set_irqstate(vm, irq, IRQSTATE_ASSERT));
}

int
vatpic_deassert_irq(struct vm *vm, int irq)
{
	return (vatpic_set_irqstate(vm, irq, IRQSTATE_DEASSERT));
}

int
vatpic_pulse_irq(struct vm *vm, int irq)
{
	return (vatpic_set_irqstate(vm, irq, IRQSTATE_PULSE));
}

void
vatpic_pending_intr(struct vm *vm, int *vecptr)
{
	struct vatpic *vatpic;
	struct atpic *atpic;
	int pin;

	vatpic = vm_atpic(vm);

	/* XXX master only */
	atpic = &vatpic->atpic[0];

	VATPIC_LOCK(vatpic);

	pin = vatpic_get_highest_irrpin(atpic);
	if (pin == -1)
		pin = 7;

	*vecptr = atpic->irq_base + pin;

	VATPIC_UNLOCK(vatpic);
}

void
vatpic_intr_accepted(struct vm *vm, int vector)
{
	struct vatpic *vatpic;
	struct atpic *atpic;
	int pin;

	vatpic = vm_atpic(vm);

	/* XXX master only */
	atpic = &vatpic->atpic[0];

	VATPIC_LOCK(vatpic);
	vatpic->intr_raised = false;

	pin = vector & 0x7;

	if (atpic->acnt[pin] == 0)
		atpic->request &= ~(1 << pin);

	if (atpic->aeoi == true) {
		if (atpic->rotate == true)
			atpic->priority = pin;
	} else {
		atpic->service |= (1 << pin);
	}

	vatpic_notify_intr(vatpic);

	VATPIC_UNLOCK(vatpic);
}

int
vatpic_master_handler(void *vm, int vcpuid, struct vm_exit *vmexit)
{
	struct vatpic *vatpic;
	struct atpic *atpic;
	int error;
	uint8_t val;

	vatpic = vm_atpic(vm);
	atpic = &vatpic->atpic[0];

	if (vmexit->u.inout.bytes != 1)
		return (-1);

	if (vmexit->u.inout.in) {
		VATPIC_LOCK(vatpic);
		if (atpic->poll) {
			VATPIC_CTR0(vatpic, "vatpic polled mode not "
			    "supported");
			VATPIC_UNLOCK(vatpic);
			return (-1);
		} else {
			if (vmexit->u.inout.port & ICU_IMR_OFFSET) {
				/* read interrrupt mask register */
				vmexit->u.inout.eax = atpic->mask;
			} else {
				if (atpic->rd_cmd_reg == OCW3_RIS) {
					/* read interrupt service register */
					vmexit->u.inout.eax = atpic->service;
				} else {
					/* read interrupt request register */
					vmexit->u.inout.eax = atpic->request;
				}
			}
		}
		VATPIC_UNLOCK(vatpic);

		return (0);
	}

	val = vmexit->u.inout.eax;

	VATPIC_LOCK(vatpic);

	if (vmexit->u.inout.port & ICU_IMR_OFFSET) {
		if (atpic->ready) {
			error = vatpic_ocw1(vatpic, atpic, val);
		} else {
			switch (atpic->icw_num) {
			case 2:
				error = vatpic_icw2(vatpic, atpic, val);
				break;
			case 3:
				error = vatpic_icw3(vatpic, atpic, val);
				break;
			case 4:
				error = vatpic_icw4(vatpic, atpic, val);
				break;
			}
		}
	} else {
		if (val & (1 << 4))
			error = vatpic_icw1(vatpic, atpic, val);

		if (atpic->ready) {
			if (val & (1 << 3))
				error = vatpic_ocw3(vatpic, atpic, val);
			else
				error = vatpic_ocw2(vatpic, atpic, val);
		}
	}

	if (atpic->ready)
		vatpic_notify_intr(vatpic);

	VATPIC_UNLOCK(vatpic);

	return (error);
}

int
vatpic_slave_handler(void *vm, int vcpuid, struct vm_exit *vmexit)
{
	if (vmexit->u.inout.bytes != 1)
		return (-1);
 
	if (vmexit->u.inout.in) {
		if (vmexit->u.inout.port & ICU_IMR_OFFSET) {
			/* all interrupts masked */
			vmexit->u.inout.eax = 0xff;
		} else {
			vmexit->u.inout.eax = 0x00;
		}
	}
 
	/* Pretend all accesses to the slave 8259 are alright */
	return (0);
}

int
vatpic_elc_handler(void *vm, int vcpuid, struct vm_exit *vmexit)
{
	struct vatpic *vatpic;
	bool is_master;

	vatpic = vm_atpic(vm);
	is_master = (vmexit->u.inout.port == IO_ELCR1);

	if (vmexit->u.inout.bytes != 1)
		return (-1);

	if (vmexit->u.inout.in) {
		if (is_master)
			vmexit->u.inout.eax = vatpic->elc[0];
		else
			vmexit->u.inout.eax = vatpic->elc[1];
	} else {
		/*
		 * For the master PIC the cascade channel (IRQ2), the
		 * heart beat timer (IRQ0), and the keyboard
		 * controller (IRQ1) cannot be programmed for level
		 * mode.
		 *
		 * For the slave PIC the real time clock (IRQ8) and
		 * the floating point error interrupt (IRQ13) cannot
		 * be programmed for level mode.
		 */
		if (is_master)
			vatpic->elc[0] = (vmexit->u.inout.eax & 0xf8);
		else
			vatpic->elc[1] = (vmexit->u.inout.eax & 0xde);
	}

	return (0);
}

struct vatpic *
vatpic_init(struct vm *vm)
{
	struct vatpic *vatpic;

	vatpic = malloc(sizeof(struct vatpic), M_VATPIC, M_WAITOK | M_ZERO);
	vatpic->vm = vm;

	mtx_init(&vatpic->mtx, "vatpic lock", NULL, MTX_SPIN);

	return (vatpic);
}

void
vatpic_cleanup(struct vatpic *vatpic)
{
	free(vatpic, M_VATPIC);
}
