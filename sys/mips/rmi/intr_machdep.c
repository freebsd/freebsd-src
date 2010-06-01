/*-
 * Copyright (c) 2006-2009 RMI Corporation
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
#include <sys/kernel.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/cpuinfo.h>
#include <machine/cpuregs.h>
#include <machine/frame.h>
#include <machine/intr_machdep.h>
#include <machine/md_var.h>
#include <machine/trap.h>
#include <machine/hwfunc.h>
#include <mips/rmi/xlrconfig.h>
#include <mips/rmi/interrupt.h>
#include <mips/rmi/clock.h>
#include <mips/rmi/pic.h>

/*#include <machine/intrcnt.h>*/
static mips_intrcnt_t mips_intr_counters[XLR_MAX_INTR];
static struct intr_event *mips_intr_events[XLR_MAX_INTR];
static int intrcnt_index;

void
xlr_mask_hard_irq(void *source)
{
	uintptr_t irq = (uintptr_t) source;

	write_c0_eimr64(read_c0_eimr64() & ~(1ULL << irq));
}

void
xlr_unmask_hard_irq(void *source)
{
	uintptr_t irq = (uintptr_t) source;

	write_c0_eimr64(read_c0_eimr64() | (1ULL << irq));
}

void
xlr_cpu_establish_hardintr(const char *name, driver_filter_t * filt,
    void (*handler) (void *), void *arg, int irq, int flags, void **cookiep,
    void (*pre_ithread)(void *), void (*post_ithread)(void *),
    void (*post_filter)(void *), int (*assign_cpu)(void *, u_char))
{
	struct intr_event *ie;	/* descriptor for the IRQ */
	int errcode;

	if (irq < 0 || irq > XLR_MAX_INTR)
		panic("%s called for unknown hard intr %d", __func__, irq);

	/*
	 * FIXME locking - not needed now, because we do this only on
	 * startup from CPU0
	 */
	ie = mips_intr_events[irq];
	/* mih->cntp = &intrcnt[irq]; */
	if (ie == NULL) {
		errcode = intr_event_create(&ie, (void *)(uintptr_t) irq, 0,
		    irq, pre_ithread, post_ithread, post_filter, assign_cpu,
		    "hard intr%d:", irq);

		if (errcode) {
			printf("Could not create event for intr %d\n", irq);
			return;
		}
		mips_intr_events[irq] = ie;
	}

	intr_event_add_handler(ie, name, filt, handler, arg,
	    intr_priority(flags), flags, cookiep);
	xlr_unmask_hard_irq((void *)(uintptr_t) irq);
}

void
cpu_establish_hardintr(const char *name, driver_filter_t * filt,
    void (*handler) (void *), void *arg, int irq, int flags, void **cookiep)
{
	xlr_cpu_establish_hardintr(name, filt, handler, arg, irq,
		flags, cookiep, xlr_mask_hard_irq, xlr_unmask_hard_irq,
		NULL, NULL);
}

void
cpu_establish_softintr(const char *name, driver_filter_t * filt,
    void (*handler) (void *), void *arg, int irq, int flags,
    void **cookiep)
{
	/* we don't separate them into soft/hard like other mips */
	xlr_cpu_establish_hardintr(name, filt, handler, arg, irq,
		flags, cookiep, xlr_mask_hard_irq, xlr_unmask_hard_irq,
		NULL, NULL);
}

void
cpu_intr(struct trapframe *tf)
{
	struct intr_event *ie;
	uint64_t eirr, eimr;
	int i;

	critical_enter();

	/* find a list of enabled interrupts */
	eirr = read_c0_eirr64();
	eimr = read_c0_eimr64();
	eirr &= eimr;
	
	if (eirr == 0) { 
		critical_exit();
		return;
	}
	/*
	 * No need to clear the EIRR here. the handler is gonna write to
	 * compare which clears eirr also
	 */
	if (eirr & (1 << IRQ_TIMER)) {
		count_compare_clockhandler(tf);
		critical_exit();
		return;
	}
	
	/* FIXME sched pin >? LOCK>? */
	for (i = sizeof(eirr) * 8 - 1; i >= 0; i--) {
		if ((eirr & (1ULL << i)) == 0)
			continue;

		ie = mips_intr_events[i];
		/* Don't account special IRQs */
		switch (i) {
		case IRQ_IPI:
		case IRQ_MSGRING:
			break;
		default:
			mips_intrcnt_inc(mips_intr_counters[i]);
		}
		write_c0_eirr64(1ULL << i);
		pic_ack(i, 0);
		if (!ie || TAILQ_EMPTY(&ie->ie_handlers)) {
			printf("stray interrupt %d\n", i);
			continue;
		}
		if (intr_event_handle(ie, tf) != 0) {
			printf("stray interrupt %d\n", i);
		}
		pic_delayed_ack(i, 0);
	}
	critical_exit();
}

void
mips_intrcnt_setname(mips_intrcnt_t counter, const char *name)
{
	int idx = counter - intrcnt;

	KASSERT(counter != NULL, ("mips_intrcnt_setname: NULL counter"));

	snprintf(intrnames + (MAXCOMLEN + 1) * idx,
	    MAXCOMLEN + 1, "%-*s", MAXCOMLEN, name);
}

mips_intrcnt_t
mips_intrcnt_create(const char* name)
{
	mips_intrcnt_t counter = &intrcnt[intrcnt_index++];

	mips_intrcnt_setname(counter, name);
	return counter;
}

void
cpu_init_interrupts()
{
	int i;
	char name[MAXCOMLEN + 1];

	/*
	 * Initialize all available vectors so spare IRQ
	 * would show up in systat output 
	 */
	for (i = 0; i < XLR_MAX_INTR; i++) {
		snprintf(name, MAXCOMLEN + 1, "int%d:", i);
		mips_intr_counters[i] = mips_intrcnt_create(name);
	}
}
