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

#include "opt_platform.h"

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
#include <sys/pmc.h>
#include <sys/pmckern.h>
#include <sys/smp.h>

#include <machine/atomic.h>
#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/cpu.h>
#include <machine/smp.h>

#ifdef ARM_INTRNG
#include "pic_if.h"

#ifdef SMP
static struct intr_irqsrc ipi_sources[INTR_IPI_COUNT];
static u_int ipi_next_num;
#endif
#endif

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

#ifdef ARM_INTRNG
#ifdef SMP
/*
 *  Lookup IPI source.
 */
static struct intr_irqsrc *
intr_ipi_lookup(u_int ipi)
{

	if (ipi >= INTR_IPI_COUNT)
		panic("%s: no such IPI %u", __func__, ipi);

	return (&ipi_sources[ipi]);
}

/*
 *  interrupt controller dispatch function for IPIs. It should
 *  be called straight from the interrupt controller, when associated
 *  interrupt source is learned. Or from anybody who has an interrupt
 *  source mapped.
 */
void
intr_ipi_dispatch(struct intr_irqsrc *isrc, struct trapframe *tf)
{
	void *arg;

	KASSERT(isrc != NULL, ("%s: no source", __func__));

	intr_ipi_increment_count(isrc->isrc_count, PCPU_GET(cpuid));

	/*
	 * Supply ipi filter with trapframe argument
	 * if none is registered.
	 */
	arg = isrc->isrc_arg != NULL ? isrc->isrc_arg : tf;
	isrc->isrc_ipifilter(arg);
}

/*
 *  Map IPI into interrupt controller.
 *
 *  Not SMP coherent.
 */
static int
ipi_map(struct intr_irqsrc *isrc, u_int ipi)
{
	boolean_t is_percpu;
	int error;

	if (ipi >= INTR_IPI_COUNT)
		panic("%s: no such IPI %u", __func__, ipi);

	KASSERT(intr_irq_root_dev != NULL, ("%s: no root attached", __func__));

	isrc->isrc_type = INTR_ISRCT_NAMESPACE;
	isrc->isrc_nspc_type = INTR_IRQ_NSPC_IPI;
	isrc->isrc_nspc_num = ipi_next_num;

	error = PIC_REGISTER(intr_irq_root_dev, isrc, &is_percpu);
	if (error == 0) {
		isrc->isrc_dev = intr_irq_root_dev;
		ipi_next_num++;
	}
	return (error);
}

/*
 *  Setup IPI handler to interrupt source.
 *
 *  Note that there could be more ways how to send and receive IPIs
 *  on a platform like fast interrupts for example. In that case,
 *  one can call this function with ASIF_NOALLOC flag set and then
 *  call intr_ipi_dispatch() when appropriate.
 *
 *  Not SMP coherent.
 */
int
intr_ipi_set_handler(u_int ipi, const char *name, intr_ipi_filter_t *filter,
    void *arg, u_int flags)
{
	struct intr_irqsrc *isrc;
	int error;

	if (filter == NULL)
		return(EINVAL);

	isrc = intr_ipi_lookup(ipi);
	if (isrc->isrc_ipifilter != NULL)
		return (EEXIST);

	if ((flags & AISHF_NOALLOC) == 0) {
		error = ipi_map(isrc, ipi);
		if (error != 0)
			return (error);
	}

	isrc->isrc_ipifilter = filter;
	isrc->isrc_arg = arg;
	isrc->isrc_handlers = 1;
	isrc->isrc_count = intr_ipi_setup_counters(name);
	isrc->isrc_index = 0; /* it should not be used in IPI case */

	if (isrc->isrc_dev != NULL) {
		PIC_ENABLE_INTR(isrc->isrc_dev, isrc);
		PIC_ENABLE_SOURCE(isrc->isrc_dev, isrc);
	}
	return (0);
}

/*
 *  Send IPI thru interrupt controller.
 */
void
pic_ipi_send(cpuset_t cpus, u_int ipi)
{
	struct intr_irqsrc *isrc;

	isrc = intr_ipi_lookup(ipi);

	KASSERT(intr_irq_root_dev != NULL, ("%s: no root attached", __func__));
	PIC_IPI_SEND(intr_irq_root_dev, isrc, cpus);
}
#endif
#endif
