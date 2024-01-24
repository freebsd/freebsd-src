/*-
 * Copyright (c) 2015-2016 Svatopluk Kraus
 * Copyright (c) 2015-2016 Michal Meloun
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

#include "opt_platform.h"

#include <sys/param.h>
#include <sys/systm.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/intr.h>

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
