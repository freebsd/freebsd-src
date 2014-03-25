/*-
 * Copyright (c) 2014 Tycho Nightingale <tycho.nightingale@pluribusnetworks.com>
 * Copyright (c) 2011 NetApp, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
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

#include <machine/vmm.h>

#include "vmm_ktr.h"
#include "vatpic.h"
#include "vioapic.h"
#include "vatpit.h"

static MALLOC_DEFINE(M_VATPIT, "atpit", "bhyve virtual atpit (8254)");

#define	VATPIT_LOCK(vatpit)		mtx_lock_spin(&((vatpit)->mtx))
#define	VATPIT_UNLOCK(vatpit)		mtx_unlock_spin(&((vatpit)->mtx))
#define	VATPIT_LOCKED(vatpit)		mtx_owned(&((vatpit)->mtx))

#define	TIMER_SEL_MASK		0xc0
#define	TIMER_RW_MASK		0x30
#define	TIMER_MODE_MASK		0x0f
#define	TIMER_SEL_READBACK	0xc0

#define	PIT_8254_FREQ		1193182
#define	TIMER_DIV(freq, hz)	(((freq) + (hz) / 2) / (hz))

struct vatpit_callout_arg {
	struct vatpit	*vatpit;
	int		channel_num;
};


struct channel {
	int		mode;
	uint16_t	initial;	/* initial counter value */
	sbintime_t	now_sbt;	/* uptime when counter was loaded */
	uint8_t		cr[2];
	uint8_t		ol[2];
	int		crbyte;
	int		olbyte;
	int		frbyte;
	struct callout	callout;
	sbintime_t	callout_sbt;	/* target time */
	struct vatpit_callout_arg callout_arg;
};

struct vatpit {
	struct vm	*vm;
	struct mtx	mtx;

	sbintime_t	freq_sbt;

	struct channel	channel[3];
};

#define	VATPIT_CTR0(vatpit, fmt)					\
	VM_CTR0((vatpit)->vm, fmt)

#define	VATPIT_CTR1(vatpit, fmt, a1)					\
	VM_CTR1((vatpit)->vm, fmt, a1)

#define	VATPIT_CTR2(vatpit, fmt, a1, a2)				\
	VM_CTR2((vatpit)->vm, fmt, a1, a2)

#define	VATPIT_CTR3(vatpit, fmt, a1, a2, a3)				\
	VM_CTR3((vatpit)->vm, fmt, a1, a2, a3)

#define	VATPIT_CTR4(vatpit, fmt, a1, a2, a3, a4)			\
	VM_CTR4((vatpit)->vm, fmt, a1, a2, a3, a4)

static void pit_timer_start_cntr0(struct vatpit *vatpit);

static void
vatpit_callout_handler(void *a)
{
	struct vatpit_callout_arg *arg = a;
	struct vatpit *vatpit;
	struct callout *callout;
	struct channel *c;

	vatpit = arg->vatpit;
	c = &vatpit->channel[arg->channel_num];
	callout = &c->callout;

	VATPIT_CTR1(vatpit, "atpit t%d fired", arg->channel_num);

	VATPIT_LOCK(vatpit);

	if (callout_pending(callout))		/* callout was reset */
		goto done;

	if (!callout_active(callout))		/* callout was stopped */
		goto done;

	callout_deactivate(callout);

	if (c->mode == TIMER_RATEGEN) {
		pit_timer_start_cntr0(vatpit);
	}

	vatpic_pulse_irq(vatpit->vm, 0);
	vioapic_pulse_irq(vatpit->vm, 2);

done:
	VATPIT_UNLOCK(vatpit);
	return;
}

static void
pit_timer_start_cntr0(struct vatpit *vatpit)
{
	struct channel *c;
	sbintime_t delta, precision;

	c = &vatpit->channel[0];
	if (c->initial != 0) {
		delta = c->initial * vatpit->freq_sbt;
		precision = delta >> tc_precexp;
		c->callout_sbt = c->callout_sbt + delta;

		callout_reset_sbt(&c->callout, c->callout_sbt,
		    precision, vatpit_callout_handler, &c->callout_arg,
		    C_ABSOLUTE);
	}
}

static uint16_t
pit_update_counter(struct vatpit *vatpit, struct channel *c, bool latch)
{
	uint16_t lval;
	sbintime_t delta_ticks;

	/* cannot latch a new value until the old one has been consumed */
	if (latch && c->olbyte != 0)
		return (0);

	if (c->initial == 0) {
		/*
		 * This is possibly an o/s bug - reading the value of
		 * the timer without having set up the initial value.
		 *
		 * The original user-space version of this code set
		 * the timer to 100hz in this condition; do the same
		 * here.
		 */
		c->initial = TIMER_DIV(PIT_8254_FREQ, 100);
		c->now_sbt = sbinuptime();
	}

	delta_ticks = (sbinuptime() - c->now_sbt) / vatpit->freq_sbt;

	lval = c->initial - delta_ticks % c->initial;

	if (latch) {
		c->olbyte = 2;
		c->ol[1] = lval;		/* LSB */
		c->ol[0] = lval >> 8;		/* MSB */
	}

	return (lval);
}

static int
vatpit_update_mode(struct vatpit *vatpit, uint8_t val)
{
	struct channel *c;
	int sel, rw, mode;

	sel = val & TIMER_SEL_MASK;
	rw = val & TIMER_RW_MASK;
	mode = val & TIMER_MODE_MASK;

	if (sel == TIMER_SEL_READBACK)
		return (-1);

	if (rw != TIMER_LATCH && rw != TIMER_16BIT)
		return (-1);

	if (rw != TIMER_LATCH) {
		/*
		 * Counter mode is not affected when issuing a
		 * latch command.
		 */
		if (mode != TIMER_INTTC &&
		    mode != TIMER_RATEGEN &&
		    mode != TIMER_SQWAVE &&
		    mode != TIMER_SWSTROBE)
			return (-1);
	}

	c = &vatpit->channel[sel >> 6];
	if (rw == TIMER_LATCH)
		pit_update_counter(vatpit, c, true);
	else {
		c->mode = mode;
		c->olbyte = 0;	/* reset latch after reprogramming */
	}

	return (0);
}

static int
vatpit_get_out(struct vatpit *vatpit, int channel)
{
	struct channel *c;
	sbintime_t delta_ticks;
	int out;

	c = &vatpit->channel[channel];

	switch (c->mode) {
	case TIMER_INTTC:
		delta_ticks = (sbinuptime() - c->now_sbt) / vatpit->freq_sbt;
		out = ((c->initial - delta_ticks) <= 0);
		break;
	default:
		out = 0;
		break;
	}

	return (out);
}

int
vatpit_handler(void *vm, int vcpuid, struct vm_exit *vmexit)
{
	struct vatpit *vatpit;
	struct channel *c;
	int port;
	uint8_t val;
	int error;

	vatpit = vm_atpit(vm);

	if (vmexit->u.inout.bytes != 1)
		return (-1);

	val = vmexit->u.inout.eax;
	port = vmexit->u.inout.port;

	if (port == TIMER_MODE) {
		if (vmexit->u.inout.in != 0) {
			VATPIT_CTR0(vatpit, "vatpit attempt to read mode");
			return (-1);
		}

		VATPIT_LOCK(vatpit);
		error = vatpit_update_mode(vatpit, val);
		VATPIT_UNLOCK(vatpit);

		return (error);
	}

	/* counter ports */
	KASSERT(port >= TIMER_CNTR0 && vmexit->u.inout.port <= TIMER_CNTR2,
	    ("invalid port 0x%x", port));
	c = &vatpit->channel[port - TIMER_CNTR0];

	VATPIT_LOCK(vatpit);
	if (vmexit->u.inout.in) {
		/*
		 * The spec says that once the output latch is completely
		 * read it should revert to "following" the counter. Use
		 * the free running counter for this case (i.e. Linux
		 * TSC calibration). Assuming the access mode is 16-bit,
		 * toggle the MSB/LSB bit on each read.
		 */
		if (c->olbyte == 0) {
			uint16_t tmp;

			tmp = pit_update_counter(vatpit, c, false);
			if (c->frbyte)
				tmp >>= 8;
			tmp &= 0xff;
			vmexit->u.inout.eax = tmp;
			c->frbyte ^= 1;
		}  else
			vmexit->u.inout.eax = c->ol[--c->olbyte];
	} else {
		c->cr[c->crbyte++] = vmexit->u.inout.eax;
		if (c->crbyte == 2) {
			c->frbyte = 0;
			c->crbyte = 0;
			c->initial = c->cr[0] | (uint16_t)c->cr[1] << 8;
			c->now_sbt = sbinuptime();
			/* Start an interval timer for channel 0 */
			if (port == TIMER_CNTR0) {
				c->callout_sbt = c->now_sbt;
				pit_timer_start_cntr0(vatpit);
			}
			if (c->initial == 0)
				c->initial = 0xffff;
		}
	}
	VATPIT_UNLOCK(vatpit);

	return (0);
}

struct vatpit *
vatpit_init(struct vm *vm)
{
	struct vatpit *vatpit;
	struct bintime bt;
	struct vatpit_callout_arg *arg;
	int i;

	vatpit = malloc(sizeof(struct vatpit), M_VATPIT, M_WAITOK | M_ZERO);
	vatpit->vm = vm;

	mtx_init(&vatpit->mtx, "vatpit lock", NULL, MTX_SPIN);

	FREQ2BT(PIT_8254_FREQ, &bt);
	vatpit->freq_sbt = bttosbt(bt);

	for (i = 0; i < 3; i++) {
		callout_init(&vatpit->channel[i].callout, true);
		arg = &vatpit->channel[i].callout_arg;
		arg->vatpit = vatpit;
		arg->channel_num = i;
	}

	return (vatpit);
}

void
vatpit_cleanup(struct vatpit *vatpit)
{
	int i;

	for (i = 0; i < 3; i++)
		callout_drain(&vatpit->channel[i].callout);

	free(vatpit, M_VATPIT);
}
