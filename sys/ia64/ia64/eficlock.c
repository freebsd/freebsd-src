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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/malloc.h>

#include <machine/clockvar.h>
#include <machine/efi.h>

static void
eficlock_init(kobj_t dev)
{
}

/*
 * Get the time of day, based on the clock's value and/or the base value.
 */
static void
eficlock_get(kobj_t dev, time_t base, struct clocktime *ct)
{
	struct efi_tm tm;

	efi_get_time(&tm);

	ct->sec = tm.tm_sec;
	ct->min = tm.tm_min;
	ct->hour = tm.tm_hour;
	ct->dow = 0;		/* XXX not used */
	ct->day = tm.tm_mday;
	ct->mon = tm.tm_mon;
	ct->year = tm.tm_year - 1900;
}

/*
 * Reset the TODR based on the time value.
 */
static void
eficlock_set(kobj_t dev, struct clocktime *ct)
{
	struct efi_tm tm;
	efi_status status;

	efi_get_time(&tm);
	tm.tm_sec = ct->sec;
	tm.tm_min = ct->min;
	tm.tm_hour = ct->hour;
	tm.tm_mday = ct->day;
	tm.tm_mon = ct->mon;
	tm.tm_year = ct->year + 1900;
	status = efi_set_time(&tm);
	if (status)
		printf("eficlock_set: could not set TODR\n");
}

static int
eficlock_getsecs(kobj_t dev, int *secp)
{
	return ETIMEDOUT;
}

static device_method_t eficlock_methods[] = {
	/* clock interface */
	DEVMETHOD(clock_init,		eficlock_init),
	DEVMETHOD(clock_get,		eficlock_get),
	DEVMETHOD(clock_set,		eficlock_set),
	DEVMETHOD(clock_getsecs,	eficlock_getsecs),

	{ 0, 0 }
};

DEFINE_CLASS(eficlock, eficlock_methods, sizeof(struct kobj));

static void
eficlock_create(void *arg)
{
	kobj_t clock;
	clock = (kobj_t)
		kobj_create(&eficlock_class, M_TEMP, M_NOWAIT);
	clockattach(clock);
}

SYSINIT(eficlock, SI_SUB_DRIVERS,SI_ORDER_MIDDLE, eficlock_create, NULL);
