/*-
 * Copyright (c) 2010-2011 Solarflare Communications, Inc.
 * All rights reserved.
 *
 * This software was developed in part by Philip Paeps under contract for
 * Solarflare Communications, Inc.
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
#include <sys/condvar.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/syslog.h>
#include <sys/taskqueue.h>

#include "common/efx.h"
#include "common/efx_mcdi.h"
#include "common/efx_regs_mcdi.h"

#include "sfxge.h"

#define	SFXGE_MCDI_POLL_INTERVAL_MIN 10		/* 10us in 1us units */
#define	SFXGE_MCDI_POLL_INTERVAL_MAX 100000	/* 100ms in 1us units */
#define	SFXGE_MCDI_WATCHDOG_INTERVAL 10000000	/* 10s in 1us units */

/* Acquire exclusive access to MCDI for the duration of a request. */
static void
sfxge_mcdi_acquire(struct sfxge_mcdi *mcdi)
{

	mtx_lock(&mcdi->lock);
	KASSERT(mcdi->state != SFXGE_MCDI_UNINITIALIZED,
	    ("MCDI not initialized"));

	while (mcdi->state != SFXGE_MCDI_INITIALIZED)
		(void)cv_wait_sig(&mcdi->cv, &mcdi->lock);
	mcdi->state = SFXGE_MCDI_BUSY;

	mtx_unlock(&mcdi->lock);
}

/* Release ownership of MCDI on request completion. */
static void
sfxge_mcdi_release(struct sfxge_mcdi *mcdi)
{

	mtx_lock(&mcdi->lock);
	KASSERT((mcdi->state == SFXGE_MCDI_BUSY ||
	    mcdi->state == SFXGE_MCDI_COMPLETED),
	    ("MCDI not busy or task not completed"));

	mcdi->state = SFXGE_MCDI_INITIALIZED;
	cv_broadcast(&mcdi->cv);

	mtx_unlock(&mcdi->lock);
}

static void
sfxge_mcdi_timeout(struct sfxge_softc *sc)
{
	device_t dev = sc->dev;

	log(LOG_WARNING, "[%s%d] MC_TIMEOUT", device_get_name(dev),
		device_get_unit(dev));

	EFSYS_PROBE(mcdi_timeout);
	sfxge_schedule_reset(sc);
}

static void
sfxge_mcdi_poll(struct sfxge_softc *sc)
{
	efx_nic_t *enp;
	clock_t delay_total;
	clock_t delay_us;
	boolean_t aborted;

	delay_total = 0;
	delay_us = SFXGE_MCDI_POLL_INTERVAL_MIN;
	enp = sc->enp;

	do {
		if (efx_mcdi_request_poll(enp)) {
			EFSYS_PROBE1(mcdi_delay, clock_t, delay_total);
			return;
		}

		if (delay_total > SFXGE_MCDI_WATCHDOG_INTERVAL) {
			aborted = efx_mcdi_request_abort(enp);
			KASSERT(aborted, ("abort failed"));
			sfxge_mcdi_timeout(sc);
			return;
		}

		/* Spin or block depending on delay interval. */
		if (delay_us < 1000000)
			DELAY(delay_us);
		else
			pause("mcdi wait", delay_us * hz / 1000000);

		delay_total += delay_us;

		/* Exponentially back off the poll frequency. */
		delay_us = delay_us * 2;
		if (delay_us > SFXGE_MCDI_POLL_INTERVAL_MAX)
			delay_us = SFXGE_MCDI_POLL_INTERVAL_MAX;

	} while (1);
}

static void
sfxge_mcdi_execute(void *arg, efx_mcdi_req_t *emrp)
{
	struct sfxge_softc *sc;
	struct sfxge_mcdi *mcdi;

	sc = (struct sfxge_softc *)arg;
	mcdi = &sc->mcdi;

	sfxge_mcdi_acquire(mcdi);

	/* Issue request and poll for completion. */
	efx_mcdi_request_start(sc->enp, emrp, B_FALSE);
	sfxge_mcdi_poll(sc);

	sfxge_mcdi_release(mcdi);
}

static void
sfxge_mcdi_ev_cpl(void *arg)
{
	struct sfxge_softc *sc;
	struct sfxge_mcdi *mcdi;

	sc = (struct sfxge_softc *)arg;
	mcdi = &sc->mcdi;

	mtx_lock(&mcdi->lock);
	KASSERT(mcdi->state == SFXGE_MCDI_BUSY, ("MCDI not busy"));
	mcdi->state = SFXGE_MCDI_COMPLETED;
	cv_broadcast(&mcdi->cv);
	mtx_unlock(&mcdi->lock);
}

static void
sfxge_mcdi_exception(void *arg, efx_mcdi_exception_t eme)
{
	struct sfxge_softc *sc;
	device_t dev;

	sc = (struct sfxge_softc *)arg;
	dev = sc->dev;

	log(LOG_WARNING, "[%s%d] MC_%s", device_get_name(dev),
	    device_get_unit(dev),
	    (eme == EFX_MCDI_EXCEPTION_MC_REBOOT)
	    ? "REBOOT"
	    : (eme == EFX_MCDI_EXCEPTION_MC_BADASSERT)
	    ? "BADASSERT" : "UNKNOWN");

	EFSYS_PROBE(mcdi_exception);

	sfxge_schedule_reset(sc);
}

int
sfxge_mcdi_init(struct sfxge_softc *sc)
{
	efx_nic_t *enp;
	struct sfxge_mcdi *mcdi;
	efx_mcdi_transport_t *emtp;
	int rc;

	enp = sc->enp;
	mcdi = &sc->mcdi;
	emtp = &mcdi->transport;

	KASSERT(mcdi->state == SFXGE_MCDI_UNINITIALIZED,
	    ("MCDI already initialized"));

	mtx_init(&mcdi->lock, "sfxge_mcdi", NULL, MTX_DEF);

	mcdi->state = SFXGE_MCDI_INITIALIZED;

	emtp->emt_context = sc;
	emtp->emt_execute = sfxge_mcdi_execute;
	emtp->emt_ev_cpl = sfxge_mcdi_ev_cpl;
	emtp->emt_exception = sfxge_mcdi_exception;

	cv_init(&mcdi->cv, "sfxge_mcdi");

	if ((rc = efx_mcdi_init(enp, emtp)) != 0)
		goto fail;

	return (0);

fail:
	mtx_destroy(&mcdi->lock);
	mcdi->state = SFXGE_MCDI_UNINITIALIZED;
	return (rc);
}

void
sfxge_mcdi_fini(struct sfxge_softc *sc)
{
	struct sfxge_mcdi *mcdi;
	efx_nic_t *enp;
	efx_mcdi_transport_t *emtp;

	enp = sc->enp;
	mcdi = &sc->mcdi;
	emtp = &mcdi->transport;

	mtx_lock(&mcdi->lock);
	KASSERT(mcdi->state == SFXGE_MCDI_INITIALIZED,
	    ("MCDI not initialized"));

	efx_mcdi_fini(enp);
	bzero(emtp, sizeof(*emtp));

	cv_destroy(&mcdi->cv);
	mtx_unlock(&mcdi->lock);

	mtx_destroy(&mcdi->lock);
}
