/*-
 * Copyright (c) 2015 Microsoft Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
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
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/smp.h>
#include <sys/time.h>
#include <sys/timeet.h>

#include "hv_vmbus_priv.h"

#define HV_TIMER_FREQUENCY		(10 * 1000 * 1000LL) /* 100ns period */
#define HV_MAX_DELTA_TICKS		0xffffffffLL
#define HV_MIN_DELTA_TICKS		1LL

static struct eventtimer et;
static uint64_t periodticks[MAXCPU];

static inline uint64_t
sbintime2tick(sbintime_t time)
{
	struct timespec val;

	val = sbttots(time);
	return val.tv_sec * HV_TIMER_FREQUENCY + val.tv_nsec / 100;
}

static int
hv_et_start(struct eventtimer *et, sbintime_t firsttime, sbintime_t periodtime)
{
	union hv_timer_config timer_cfg;
	uint64_t current;

	timer_cfg.as_uint64 = 0;
	timer_cfg.auto_enable = 1;
	timer_cfg.sintx = HV_VMBUS_TIMER_SINT;

	periodticks[curcpu] = sbintime2tick(periodtime);
	if (firsttime == 0)
		firsttime = periodtime;

	current = rdmsr(HV_X64_MSR_TIME_REF_COUNT);
	current += sbintime2tick(firsttime);

	wrmsr(HV_X64_MSR_STIMER0_CONFIG, timer_cfg.as_uint64);
	wrmsr(HV_X64_MSR_STIMER0_COUNT, current);

	return (0);
}

static int
hv_et_stop(struct eventtimer *et)
{
	wrmsr(HV_X64_MSR_STIMER0_CONFIG, 0);
	wrmsr(HV_X64_MSR_STIMER0_COUNT, 0);

	return (0);
}

void
hv_et_intr(struct trapframe *frame)
{
	union hv_timer_config timer_cfg;
	struct trapframe *oldframe;
	struct thread *td;

	if (periodticks[curcpu] != 0) {
		uint64_t tick = sbintime2tick(periodticks[curcpu]);
		timer_cfg.as_uint64 = rdmsr(HV_X64_MSR_STIMER0_CONFIG);
		timer_cfg.enable = 0;
		timer_cfg.auto_enable = 1;
		timer_cfg.periodic = 1;
		periodticks[curcpu] = 0;

		wrmsr(HV_X64_MSR_STIMER0_CONFIG, timer_cfg.as_uint64);
		wrmsr(HV_X64_MSR_STIMER0_COUNT, tick);
	}

	if (et.et_active) {
		td = curthread;
		td->td_intr_nesting_level++;
		oldframe = td->td_intr_frame;
		td->td_intr_frame = frame;
		et.et_event_cb(&et, et.et_arg);
		td->td_intr_frame = oldframe;
		td->td_intr_nesting_level--;
	}
}

void
hv_et_init(void)
{
	et.et_name = "HyperV";
	et.et_flags = ET_FLAGS_ONESHOT | ET_FLAGS_PERCPU | ET_FLAGS_PERIODIC;
	et.et_quality = 1000;
	et.et_frequency = HV_TIMER_FREQUENCY;
	et.et_min_period = (1LL << 32) / HV_TIMER_FREQUENCY;
	et.et_max_period = HV_MAX_DELTA_TICKS * ((1LL << 32) / HV_TIMER_FREQUENCY);
	et.et_start = hv_et_start;
	et.et_stop = hv_et_stop;
	et.et_priv = &et;
	et_register(&et);
}

