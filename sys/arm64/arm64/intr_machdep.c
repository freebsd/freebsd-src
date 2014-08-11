/*-
 * Copyright (c) 2006 Oleksandr Tymoshenko
 * Copyright (c) 2002-2004 Juli Mallett <jmallett@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * From: projects/arm64/sys/mips/mips/intr_machdep.c r233318
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: projects/arm64/sys/mips/mips/intr_machdep.c 233318 2012-03-22 17:47:52Z gonzo $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/cpuset.h>
#include <sys/interrupt.h>

#include <machine/intr.h>

#include "pic_if.h"

#define	NIRQS	1024

static struct intr_event *intr_events[NIRQS];
static device_t root_pic;
static u_int num_irq;

#if 0
static struct intr_event *softintr_events[NSOFT_IRQS];
static mips_intrcnt_t mips_intr_counters[NSOFT_IRQS + NHARD_IRQS];

static int intrcnt_index;

mips_intrcnt_t
mips_intrcnt_create(const char* name)
{
	mips_intrcnt_t counter = &intrcnt[intrcnt_index++];

	mips_intrcnt_setname(counter, name);
	return counter;
}

void
mips_intrcnt_setname(mips_intrcnt_t counter, const char *name)
{
	int idx = counter - intrcnt;

	KASSERT(counter != NULL, ("mips_intrcnt_setname: NULL counter"));

	snprintf(intrnames + (MAXCOMLEN + 1) * idx,
	    MAXCOMLEN + 1, "%-*s", MAXCOMLEN, name);
}
#endif

void
arm_mask_irq(u_int irq)
{

	PIC_MASK(root_pic, irq);
}

void
arm_unmask_irq(u_int irq)
{

	PIC_UNMASK(root_pic, irq);
}

static void
intr_pre_ithread(void *arg)
{
	u_int irq = (uintptr_t)arg;

	PIC_PRE_ITHREAD(root_pic, irq);
}

static void
intr_post_ithread(void *arg)
{
	u_int irq = (uintptr_t)arg;

	PIC_POST_ITHREAD(root_pic, irq);
}

static void
intr_post_filter(void *arg)
{
	u_int irq = (uintptr_t)arg;

	PIC_POST_FILTER(root_pic, irq);
}

void
cpu_set_pic(device_t pic, u_int nirq)
{

	KASSERT(root_pic == NULL, ("Unable to set the pic twice"));
	KASSERT(nirq <= NIRQS, ("PIC is trying to handle too many IRQs"));

	num_irq = nirq;
	root_pic = pic;
}

void
cpu_establish_intr(const char *name, driver_filter_t *filt,
    void (*handler)(void*), void *arg, int irq, int flags, void **cookiep)
{
	struct intr_event *event;
	int error;

	if (irq < 0 || irq >= num_irq)
		panic("%s called for unknown intr %d", __func__, irq);

	/* TODO: Add locking for the intr_events array */
	event = intr_events[irq];
	if (event == NULL) {
		error = intr_event_create(&event, (void *)(uintptr_t)irq, 0,
		    irq, intr_pre_ithread, intr_post_ithread,
		    intr_post_filter, NULL, "int%d", irq);
		if (error)
			return;
		intr_events[irq] = event;
		PIC_UNMASK(root_pic, irq);
	}

	intr_event_add_handler(event, name, filt, handler, arg,
	    intr_priority(flags), flags, cookiep);

#if 0
	mips_intrcnt_setname(mips_intr_counters[NSOFT_IRQS + irq],
			     event->ie_fullname);
#endif
}

void
cpu_dispatch_intr(u_int irq, struct trapframe *tf)
{
	struct intr_event *event;

	event = intr_events[irq];
	if (intr_event_handle(event, tf) != 0) {
		/* Stray irq */
		printf("Stray IRQ %u\n", irq);
		arm_mask_irq(irq);
	}
}

void
cpu_intr(struct trapframe *tf)
{
	critical_enter();
	PIC_DISPATCH(root_pic, tf);
	critical_exit();
}

