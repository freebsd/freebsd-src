/*-
 * Copyright (c) 2015 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
 *
 * Portions of this software were developed by SRI International and the
 * University of Cambridge Computer Laboratory under DARPA/AFRL contract
 * FA8750-10-C-0237 ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Portions of this software were developed by the University of Cambridge
 * Computer Laboratory as part of the CTSRD Project, with support from the
 * UK Higher Education Innovation Fund (HEIF).
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/interrupt.h>

#include <machine/clock.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/frame.h>
#include <machine/intr.h>

enum {
	IRQ_SOFTWARE,
	IRQ_TIMER,
	IRQ_HTIF,
	NIRQS
};

u_long intrcnt[NIRQS];
size_t sintrcnt = sizeof(intrcnt);

char intrnames[NIRQS * (MAXCOMLEN + 1) * 2];
size_t sintrnames = sizeof(intrnames);

static struct intr_event *intr_events[NIRQS];
static riscv_intrcnt_t riscv_intr_counters[NIRQS];

static int intrcnt_index;

riscv_intrcnt_t
riscv_intrcnt_create(const char* name)
{
	riscv_intrcnt_t counter;

	counter = &intrcnt[intrcnt_index++];
	riscv_intrcnt_setname(counter, name);

	return (counter);
}

void
riscv_intrcnt_setname(riscv_intrcnt_t counter, const char *name)
{
	int i;

	i = (counter - intrcnt);

	KASSERT(counter != NULL, ("riscv_intrcnt_setname: NULL counter"));

	snprintf(intrnames + (MAXCOMLEN + 1) * i,
	    MAXCOMLEN + 1, "%-*s", MAXCOMLEN, name);
}

static void
riscv_mask_irq(void *source)
{
	uintptr_t irq;

	irq = (uintptr_t)source;

	switch (irq) {
	case IRQ_TIMER:
		csr_clear(sie, SIE_STIE);
		break;
	case IRQ_SOFTWARE:
		csr_clear(sie, SIE_SSIE);
		break;
	default:
		panic("Unknown irq %d\n", irq);
	}
}

static void
riscv_unmask_irq(void *source)
{
	uintptr_t irq;

	irq = (uintptr_t)source;

	switch (irq) {
	case IRQ_TIMER:
		csr_set(sie, SIE_STIE);
		break;
	case IRQ_SOFTWARE:
		csr_set(sie, SIE_SSIE);
		break;
	default:
		panic("Unknown irq %d\n", irq);
	}
}

void
riscv_init_interrupts(void)
{
	char name[MAXCOMLEN + 1];
	int i;

	for (i = 0; i < NIRQS; i++) {
		snprintf(name, MAXCOMLEN + 1, "int%d:", i);
		riscv_intr_counters[i] = riscv_intrcnt_create(name);
	}
}

int
riscv_setup_intr(const char *name, driver_filter_t *filt,
    void (*handler)(void*), void *arg, int irq, int flags, void **cookiep)
{
	struct intr_event *event;
	int error;

	if (irq < 0 || irq >= NIRQS)
		panic("%s: unknown intr %d", __func__, irq);

	event = intr_events[irq];
	if (event == NULL) {
		error = intr_event_create(&event, (void *)(uintptr_t)irq, 0,
		    irq, riscv_mask_irq, riscv_unmask_irq,
		    NULL, NULL, "int%d", irq);
		if (error)
			return (error);
		intr_events[irq] = event;
		riscv_unmask_irq((void*)(uintptr_t)irq);
	}

	intr_event_add_handler(event, name, filt, handler, arg,
	    intr_priority(flags), flags, cookiep);

	riscv_intrcnt_setname(riscv_intr_counters[irq],
			     event->ie_fullname);

	return (0);
}

int
riscv_teardown_intr(void *ih)
{

	/* TODO */

	return (0);
}

int
riscv_config_intr(u_int irq, enum intr_trigger trig, enum intr_polarity pol)
{

	/* There is no configuration for interrupts */

	return (0);
}

void
riscv_cpu_intr(struct trapframe *frame)
{
	struct intr_event *event;
	int active_irq;

	critical_enter();

	KASSERT(frame->tf_scause & EXCP_INTR,
		("riscv_cpu_intr: wrong frame passed"));

	active_irq = (frame->tf_scause & EXCP_MASK);

	switch (active_irq) {
	case IRQ_SOFTWARE:
	case IRQ_TIMER:
		event = intr_events[active_irq];
		/* Update counters */
		atomic_add_long(riscv_intr_counters[active_irq], 1);
		PCPU_INC(cnt.v_intr);
		break;
	case IRQ_HTIF:
		/* HTIF interrupts are only handled in machine mode */
		panic("%s: HTIF interrupt", __func__);
		break;
	default:
		event = NULL;
	}

	if (!event || TAILQ_EMPTY(&event->ie_handlers) ||
	    (intr_event_handle(event, frame) != 0))
		printf("stray interrupt %d\n", active_irq);

	critical_exit();
}
