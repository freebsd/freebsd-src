/*-
 * Copyright (c) 2000 Doug Rabson
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
 *
 *	$FreeBSD$
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/malloc.h>

#include <machine/clockvar.h>

struct ssc_time {
	u_int16_t year;
	u_int8_t month;
	u_int8_t day;
	u_int8_t hour;
	u_int8_t minute;
	u_int8_t second;
	u_int8_t pad1;
	u_int32_t nanosecond;
	int16_t timezone;
	u_int8_t daylight;
	u_int8_t pad2;
};

#define SSC_GET_RTC			65

static u_int64_t
ssc(u_int64_t in0, u_int64_t in1, u_int64_t in2, u_int64_t in3, int which)
{
	register u_int64_t ret0 __asm("r8");

	__asm __volatile("mov r15=%1\n\t"
			 "break 0x80001"
			 : "=r"(ret0)
			 : "r"(which), "r"(in0), "r"(in1), "r"(in2), "r"(in3));
	return ret0;
}

static void
sscclock_init(device_t dev)
{
}

/*
 * Get the time of day, based on the clock's value and/or the base value.
 */
static void
sscclock_get(device_t dev, time_t base, struct clocktime *ct)
{
#if 0
	struct ssc_time time;

	ssc(ia64_tpa((vm_offset_t) &time), 0, 0, 0, SSC_GET_RTC);

	ct->sec = time.second;
	ct->min = time.minute;
	ct->hour = time.hour;
	ct->dow = 0;		/* XXX */
	ct->day = time.day;
	ct->mon = time.month;
	ct->year = time.year;
#else
	bzero(ct, sizeof(struct clocktime));
#endif
}

/*
 * Reset the TODR based on the time value.
 */
static void
sscclock_set(device_t dev, struct clocktime *ct)
{
}

static int
sscclock_getsecs(device_t dev, int *secp)
{
	return ETIMEDOUT;
}

static device_method_t sscclock_methods[] = {
	/* clock interface */
	DEVMETHOD(clock_init,		sscclock_init),
	DEVMETHOD(clock_get,		sscclock_get),
	DEVMETHOD(clock_set,		sscclock_set),
	DEVMETHOD(clock_getsecs,	sscclock_getsecs),

	{ 0, 0 }
};

DEFINE_CLASS(sscclock, sscclock_methods, sizeof(struct kobj));

static void
sscclock_create(void *arg)
{
	device_t clock = (device_t)
		kobj_create(&sscclock_class, M_TEMP, M_NOWAIT);
	clockattach(clock);
}

SYSINIT(sscdev, SI_SUB_DRIVERS,SI_ORDER_MIDDLE, sscclock_create, NULL);
