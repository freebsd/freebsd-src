/*-
 * Copyright (c) 2006 Fill this file and put your name here
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
#include <machine/cpuinfo.h>
#include <machine/cpuregs.h>
#include <machine/frame.h>
#include <machine/intr_machdep.h>
#include <machine/md_var.h>
#include <machine/trap.h>

static struct intr_event *hardintr_events[NHARD_IRQS];
static struct intr_event *softintr_events[NSOFT_IRQS];

#ifdef notyet
static int intrcnt_tab[NHARD_IRQS + NSOFT_IRQS];
static int intrcnt_index = 0;
static int last_printed = 0;
#endif

void
mips_mask_irq(void)
{

	printf("Unimplemented: %s\n", __func__);
}

void
mips_unmask_irq(void)
{

	printf("Unimplemented: %s\n", __func__);
}

void
cpu_establish_hardintr(const char *name, driver_filter_t *filt,
    void (*handler)(void*), void *arg, int irq, int flags, void **cookiep)
{
	struct intr_event *event;
	int error;

	printf("Establish HARD IRQ %d: filt %p handler %p arg %p\n",
	    irq, filt, handler, arg);
	/*
	 * We have 6 levels, but thats 0 - 5 (not including 6)
	 */
	if (irq < 0 || irq >= NHARD_IRQS)
		panic("%s called for unknown hard intr %d", __func__, irq);

	event = hardintr_events[irq];
	if (event == NULL) {
		error = intr_event_create(&event, (void *)irq, 0,
		    (mask_fn)mips_mask_irq, (mask_fn)mips_unmask_irq,
		    (mask_fn)mips_unmask_irq, NULL, "hard intr%d:", irq);
		if (error)
			return;
		hardintr_events[irq] = event;
#ifdef notyet
		last_printed += snprintf(intrnames + last_printed,
		    MAXCOMLEN + 1, "hard irq%d: %s", irq, name);
		last_printed++;
		intrcnt_tab[irq] = intrcnt_index;
		intrcnt_index++;
#endif

	}

	intr_event_add_handler(event, name, filt, handler, arg,
	    intr_priority(flags), flags, cookiep);

	mips_wr_status(mips_rd_status() | (((1 << irq) << 8) << 2));
}

void
cpu_establish_softintr(const char *name, driver_filter_t *filt,
    void (*handler)(void*), void *arg, int irq, int flags,
    void **cookiep)
{
	struct intr_event *event;
	int error;

	printf("Establish SOFT IRQ %d: filt %p handler %p arg %p\n",
	    irq, filt, handler, arg);
	if (irq < 0 || irq > NSOFT_IRQS)
		panic("%s called for unknown hard intr %d", __func__, irq);

	event = softintr_events[irq];
	if (event == NULL) {
		error = intr_event_create(&event, (void *)irq, 0,
		    (mask_fn)mips_mask_irq, (mask_fn)mips_unmask_irq,
		    (mask_fn)mips_unmask_irq, NULL, "intr%d:", irq);
		if (error)
			return;
		softintr_events[irq] = event;
	}

	intr_event_add_handler(event, name, filt, handler, arg,
	    intr_priority(flags), flags, cookiep);

	mips_wr_status(mips_rd_status() | (((1<< irq) << 8)));
}

void
cpu_intr(struct trapframe *tf)
{
	struct intr_handler *ih;
	struct intr_event *event;
	register_t cause;
	int hard, i, intr, thread;

	critical_enter();

	cause = mips_rd_cause();
	intr = (cause & MIPS_INT_MASK) >> 8;
	cause &= ~MIPS_INT_MASK;
	mips_wr_cause(cause);
	while ((i = fls(intr)) != 0) {
		intr &= ~(1 << (i - 1));
		switch (i) {
		case 1: case 2:
			/* Software interrupt. */
			i--; /* Get a 0-offset interrupt. */
			hard = 0;
			event = softintr_events[i];
			break;
		default:
			/* Hardware interrupt. */
			i -= 2; /* Trim software interrupt bits. */
			i--; /* Get a 0-offset interrupt. */
			hard = 1;
			event = hardintr_events[i];
			break;
		}

		if (!event || TAILQ_EMPTY(&event->ie_handlers)) {
			printf("stray %s interrupt %d\n",
			    hard ? "hard" : "soft", i);
			continue;
		}

		/* Execute fast handlers. */
		thread = 0;
		TAILQ_FOREACH(ih, &event->ie_handlers, ih_next) {
			if (ih->ih_filter == NULL)
				thread = 1;
			else
				ih->ih_filter(ih->ih_argument ?
				    ih->ih_argument : tf);
		}

		/* Schedule thread if needed. */
		if (thread)
			intr_event_schedule_thread(event);
	}

	KASSERT(i == 0, ("all interrupts handled"));

	critical_exit();
}
