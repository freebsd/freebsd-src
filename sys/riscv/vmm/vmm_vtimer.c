/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Ruslan Bukin <br@bsdpad.com>
 *
 * This software was developed by the University of Cambridge Computer
 * Laboratory (Department of Computer Science and Technology) under Innovate
 * UK project 105694, "Digital Security by Design (DSbD) Technology Platform
 * Prototype".
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/openfirm.h>

#include "riscv.h"

#define	VTIMER_DEFAULT_FREQ	1000000

static int
vtimer_get_timebase(uint32_t *freq)
{
	phandle_t node;
	int len;

	node = OF_finddevice("/cpus");
	if (node == -1)
		return (ENXIO);

	len = OF_getproplen(node, "timebase-frequency");
	if (len != 4)
		return (ENXIO);

	OF_getencprop(node, "timebase-frequency", freq, len);

	return (0);
}

void
vtimer_cpuinit(struct hypctx *hypctx)
{
	struct vtimer *vtimer;
	uint32_t freq;
	int error;

	vtimer = &hypctx->vtimer;
	mtx_init(&vtimer->mtx, "vtimer callout mutex", NULL, MTX_DEF);
	callout_init_mtx(&vtimer->callout, &vtimer->mtx, 0);

	error = vtimer_get_timebase(&freq);
	if (error)
		freq = VTIMER_DEFAULT_FREQ;

	vtimer->freq = freq;
}

static void
vtimer_inject_irq_callout(void *arg)
{
	struct hypctx *hypctx;
	struct hyp *hyp;

	hypctx = arg;
	hyp = hypctx->hyp;

	atomic_set_32(&hypctx->interrupts_pending, HVIP_VSTIP);
	vcpu_notify_event(vm_vcpu(hyp->vm, hypctx->cpu_id));
}

int
vtimer_set_timer(struct hypctx *hypctx, uint64_t next_val)
{
	struct vtimer *vtimer;
	sbintime_t time;
	uint64_t curtime;
	uint64_t delta;

	vtimer = &hypctx->vtimer;

	curtime = rdtime();
	if (curtime < next_val) {
		delta = next_val - curtime;
		time = delta * SBT_1S / vtimer->freq;
		atomic_clear_32(&hypctx->interrupts_pending, HVIP_VSTIP);
		callout_reset_sbt(&vtimer->callout, time, 0,
		    vtimer_inject_irq_callout, hypctx, 0);
	} else
		atomic_set_32(&hypctx->interrupts_pending, HVIP_VSTIP);

	return (0);
}
