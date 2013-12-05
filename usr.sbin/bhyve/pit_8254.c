/*-
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
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/time.h>
#include <machine/vmm.h>

#include <machine/clock.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <vmmapi.h>

#include "bhyverun.h"
#include "inout.h"
#include "mevent.h"
#include "pit_8254.h"

#define	TIMER_SEL_MASK		0xc0
#define	TIMER_RW_MASK		0x30
#define	TIMER_MODE_MASK		0x0f
#define	TIMER_SEL_READBACK	0xc0

#define	TIMER_DIV(freq, hz)	(((freq) + (hz) / 2) / (hz))

#define	PIT_8254_FREQ		1193182
static const int nsecs_per_tick = 1000000000 / PIT_8254_FREQ;

struct counter {
	struct vmctx	*ctx;
	struct mevent	*tevp;	
	struct timeval	tv;		/* uptime when counter was loaded */
	int		mode;
	uint16_t	initial;	/* initial counter value */
	uint8_t		cr[2];
	uint8_t		ol[2];
	int		crbyte;
	int		olbyte;
	int		frbyte;
};


static void
timevalfix(struct timeval *t1)
{

	if (t1->tv_usec < 0) {
		t1->tv_sec--;
		t1->tv_usec += 1000000;
	}
	if (t1->tv_usec >= 1000000) {
		t1->tv_sec++;
		t1->tv_usec -= 1000000;
	}
}

static void
timevalsub(struct timeval *t1, const struct timeval *t2)
{

	t1->tv_sec -= t2->tv_sec;
	t1->tv_usec -= t2->tv_usec;
	timevalfix(t1);
}

static uint64_t pit_mev_count;

static void
pit_mevent_cb(int fd, enum ev_type type, void *param)
{
	struct counter *c;

	c = param;

	pit_mev_count++;

	vm_ioapic_pulse_irq(c->ctx, 2);

	/*
	 * Delete the timer for one-shots
	 */
	if (c->mode != TIMER_RATEGEN) {
		mevent_delete(c->tevp);
		c->tevp = NULL;
	}
}

static void
pit_timer_start(struct vmctx *ctx, struct counter *c)
{
	int msecs;

	if (c->initial != 0) {
		msecs = c->initial * nsecs_per_tick / 1000000;
		if (msecs == 0)
			msecs = 1;

		if (c->tevp == NULL)
			c->tevp = mevent_add(msecs, EVF_TIMER, pit_mevent_cb,
			    c);
	}
}

static uint16_t
pit_update_counter(struct counter *c, int latch)
{
	struct timeval tv2;
	uint16_t lval;
	uint64_t delta_nsecs, delta_ticks;

	/* cannot latch a new value until the old one has been consumed */
	if (latch && c->olbyte != 0)
		return (0);

	if (c->initial == 0 || c->initial == 1) {
		/*
		 * XXX the program that runs the VM can be stopped and
		 * restarted at any time. This means that state that was
		 * created by the guest is destroyed between invocations
		 * of the program.
		 *
		 * If the counter's initial value is not programmed we
		 * assume a value that would be set to generate 100
		 * interrupts per second.
		 */
		c->initial = TIMER_DIV(PIT_8254_FREQ, 100);
		gettimeofday(&c->tv, NULL);
	}
	
	(void)gettimeofday(&tv2, NULL);
	timevalsub(&tv2, &c->tv);
	delta_nsecs = tv2.tv_sec * 1000000000 + tv2.tv_usec * 1000;
	delta_ticks = delta_nsecs / nsecs_per_tick;

	lval = c->initial - delta_ticks % c->initial;

	if (latch) {
		c->olbyte = 2;
		c->ol[1] = lval;		/* LSB */
		c->ol[0] = lval >> 8;		/* MSB */
	}

	return (lval);
}

static int
pit_8254_handler(struct vmctx *ctx, int vcpu, int in, int port, int bytes,
		 uint32_t *eax, void *arg)
{
	int sel, rw, mode;
	uint8_t val;
	struct counter *c;

	static struct counter counter[3];

	if (bytes != 1)
		return (-1);

	val = *eax;

	if (port == TIMER_MODE) {
		assert(in == 0);
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
		
		c = &counter[sel >> 6];
		c->ctx = ctx;
		c->mode = mode;
		if (rw == TIMER_LATCH)
			pit_update_counter(c, 1);
		else
			c->olbyte = 0;	/* reset latch after reprogramming */
		
		return (0);
	}

	/* counter ports */
	assert(port >= TIMER_CNTR0 && port <= TIMER_CNTR2);
	c = &counter[port - TIMER_CNTR0];

	if (in) {
		/*
		 * The spec says that once the output latch is completely
		 * read it should revert to "following" the counter. Use
		 * the free running counter for this case (i.e. Linux
		 * TSC calibration). Assuming the access mode is 16-bit,
		 * toggle the MSB/LSB bit on each read.
		 */
		if (c->olbyte == 0) {
			uint16_t tmp;

			tmp = pit_update_counter(c, 0);
			if (c->frbyte)
				tmp >>= 8;
			tmp &= 0xff;
			*eax = tmp;
			c->frbyte ^= 1;
		}  else
			*eax = c->ol[--c->olbyte];
	} else {
		c->cr[c->crbyte++] = *eax;
		if (c->crbyte == 2) {
			c->frbyte = 0;
			c->crbyte = 0;
			c->initial = c->cr[0] | (uint16_t)c->cr[1] << 8;
			/* Start an interval timer for counter 0 */
			if (port == 0x40)
				pit_timer_start(ctx, c);
			if (c->initial == 0)
				c->initial = 0xffff;
			gettimeofday(&c->tv, NULL);
		}
	}

	return (0);
}

INOUT_PORT(8254, TIMER_MODE, IOPORT_F_OUT, pit_8254_handler);
INOUT_PORT(8254, TIMER_CNTR0, IOPORT_F_INOUT, pit_8254_handler);
INOUT_PORT(8254, TIMER_CNTR1, IOPORT_F_INOUT, pit_8254_handler);
INOUT_PORT(8254, TIMER_CNTR2, IOPORT_F_INOUT, pit_8254_handler);
