/*	$NetBSD: intr.c,v 1.12 2003/07/15 00:24:41 lukem Exp $	*/

/*-
 * Copyright (c) 2004 Olivier Houchard.
 * Copyright (c) 1994-1998 Mark Brinicombe.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Mark Brinicombe
 *	for the NetBSD Project.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Soft interrupt and other generic interrupt functions.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/syslog.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/conf.h>
#include <machine/atomic.h>
#include <machine/intr.h>
#include <machine/cpu.h>

#define	INTRNAME_LEN	(MAXCOMLEN + 1)

typedef void (*mask_fn)(void *);

static struct intr_event *intr_events[NIRQ];

void	arm_irq_handler(struct trapframe *);

void (*arm_post_filter)(void *) = NULL;
int (*arm_config_irq)(int irq, enum intr_trigger trig,
    enum intr_polarity pol) = NULL;

/* Data for statistics reporting. */
u_long intrcnt[NIRQ];
char intrnames[NIRQ * INTRNAME_LEN];
size_t sintrcnt = sizeof(intrcnt);
size_t sintrnames = sizeof(intrnames);

/*
 * Pre-format intrnames into an array of fixed-size strings containing spaces.
 * This allows us to avoid the need for an intermediate table of indices into
 * the names and counts arrays, while still meeting the requirements and
 * assumptions of vmstat(8) and the kdb "show intrcnt" command, the two
 * consumers of this data.
 */
static void
intr_init(void *unused)
{
	int i;

	for (i = 0; i < NIRQ; ++i) {
		snprintf(&intrnames[i * INTRNAME_LEN], INTRNAME_LEN, "%-*s",
		    INTRNAME_LEN - 1, "");
	}
}

SYSINIT(intr_init, SI_SUB_INTR, SI_ORDER_FIRST, intr_init, NULL);

void
arm_setup_irqhandler(const char *name, driver_filter_t *filt,
    void (*hand)(void*), void *arg, int irq, int flags, void **cookiep)
{
	struct intr_event *event;
	int error;

	if (irq < 0 || irq >= NIRQ)
		return;
	event = intr_events[irq];
	if (event == NULL) {
		error = intr_event_create(&event, (void *)irq, 0, irq,
		    (mask_fn)arm_mask_irq, (mask_fn)arm_unmask_irq,
		    arm_post_filter, NULL, "intr%d:", irq);
		if (error)
			return;
		intr_events[irq] = event;
		snprintf(&intrnames[irq * INTRNAME_LEN], INTRNAME_LEN, 
		    "irq%d: %-*s", irq, INTRNAME_LEN - 1, name);
	}
	intr_event_add_handler(event, name, filt, hand, arg,
	    intr_priority(flags), flags, cookiep);
}

int
arm_remove_irqhandler(int irq, void *cookie)
{
	struct intr_event *event;
	int error;

	event = intr_events[irq];
	arm_mask_irq(irq);
	
	error = intr_event_remove_handler(cookie);

	if (!TAILQ_EMPTY(&event->ie_handlers))
		arm_unmask_irq(irq);
	return (error);
}

void dosoftints(void);
void
dosoftints(void)
{
}

void
arm_irq_handler(struct trapframe *frame)
{
	struct intr_event *event;
	int i;

	PCPU_INC(cnt.v_intr);
	i = -1;
	while ((i = arm_get_next_irq(i)) != -1) {
		intrcnt[i]++;
		event = intr_events[i];
		if (intr_event_handle(event, frame) != 0) {
			/* XXX: Log stray IRQs */
			arm_mask_irq(i);
		}
	}
}

/*
 * arm_irq_memory_barrier()
 *
 * Ensure all writes to device memory have reached devices before proceeding.
 *
 * This is intended to be called from the post-filter and post-thread routines
 * of an interrupt controller implementation.  A peripheral device driver should
 * use bus_space_barrier() if it needs to ensure a write has reached the
 * hardware for some reason other than clearing interrupt conditions.
 *
 * The need for this function arises from the ARM weak memory ordering model.
 * Writes to locations mapped with the Device attribute bypass any caches, but
 * are buffered.  Multiple writes to the same device will be observed by that
 * device in the order issued by the cpu.  Writes to different devices may
 * appear at those devices in a different order than issued by the cpu.  That
 * is, if the cpu writes to device A then device B, the write to device B could
 * complete before the write to device A.
 *
 * Consider a typical device interrupt handler which services the interrupt and
 * writes to a device status-acknowledge register to clear the interrupt before
 * returning.  That write is posted to the L2 controller which "immediately"
 * places it in a store buffer and automatically drains that buffer.  This can
 * be less immediate than you'd think... There may be no free slots in the store
 * buffers, so an existing buffer has to be drained first to make room.  The
 * target bus may be busy with other traffic (such as DMA for various devices),
 * delaying the drain of the store buffer for some indeterminate time.  While
 * all this delay is happening, execution proceeds on the CPU, unwinding its way
 * out of the interrupt call stack to the point where the interrupt driver code
 * is ready to EOI and unmask the interrupt.  The interrupt controller may be
 * accessed via a faster bus than the hardware whose handler just ran; the write
 * to unmask and EOI the interrupt may complete quickly while the device write
 * to ack and clear the interrupt source is still lingering in a store buffer
 * waiting for access to a slower bus.  With the interrupt unmasked at the
 * interrupt controller but still active at the device, as soon as interrupts
 * are enabled on the core the device re-interrupts immediately: now you've got
 * a spurious interrupt on your hands.
 *
 * The right way to fix this problem is for every device driver to use the
 * proper bus_space_barrier() calls in its interrupt handler.  For ARM a single
 * barrier call at the end of the handler would work.  This would have to be
 * done to every driver in the system, not just arm-specific drivers.
 *
 * Another potential fix is to map all device memory as Strongly-Ordered rather
 * than Device memory, which takes the store buffers out of the picture.  This
 * has a pretty big impact on overall system performance, because each strongly
 * ordered memory access causes all L2 store buffers to be drained.
 *
 * A compromise solution is to have the interrupt controller implementation call
 * this function to establish a barrier between writes to the interrupt-source
 * device and writes to the interrupt controller device.
 *
 * This takes the interrupt number as an argument, and currently doesn't use it.
 * The plan is that maybe some day there is a way to flag certain interrupts as
 * "memory barrier safe" and we can avoid this overhead with them.
 */
void
arm_irq_memory_barrier(uintptr_t irq)
{

	dsb();
	cpu_l2cache_drain_writebuf();
}

