/*-
 * Copyright (c) 1997, 1998, 1999 Nicolas Souchu
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
#include <sys/lock.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <dev/ppbus/ppbconf.h>

#include "ppbus_if.h"

#include <dev/ppbus/ppbio.h>

MODULE_VERSION(ppbus, 1);

#define DEVTOSOFTC(dev) ((struct ppb_data *)device_get_softc(dev))

/*
 * ppb_poll_bus()
 *
 * Polls the bus
 *
 * max is a delay in 10-milliseconds
 */
int
ppb_poll_bus(device_t bus, int max,
	     char mask, char status, int how)
{
	struct ppb_data *ppb = DEVTOSOFTC(bus);
	int i, j, error;
	char r;

	mtx_assert(ppb->ppc_lock, MA_OWNED);

	/* try at least up to 10ms */
	for (j = 0; j < ((how & PPB_POLL) ? max : 1); j++) {
		for (i = 0; i < 10000; i++) {
			r = ppb_rstr(bus);
			DELAY(1);
			if ((r & mask) == status)
				return (0);
		}
	}

	if (!(how & PPB_POLL)) {
	   for (i = 0; max == PPB_FOREVER || i < max-1; i++) {
		if ((ppb_rstr(bus) & mask) == status)
			return (0);

		/* wait 10 ms */
		error = mtx_sleep((caddr_t)bus, ppb->ppc_lock, PPBPRI |
		    (how == PPB_NOINTR ? 0 : PCATCH), "ppbpoll", hz/100);
		if (error != EWOULDBLOCK)
			return (error);
	   }
	}

	return (EWOULDBLOCK);
}

/*
 * ppb_get_epp_protocol()
 *
 * Return the chipset EPP protocol
 */
int
ppb_get_epp_protocol(device_t bus)
{
#ifdef INVARIANTS
	struct ppb_data *ppb = DEVTOSOFTC(bus);
#endif
	uintptr_t protocol;

	mtx_assert(ppb->ppc_lock, MA_OWNED);
	BUS_READ_IVAR(device_get_parent(bus), bus, PPC_IVAR_EPP_PROTO, &protocol);

	return (protocol);
}

/*
 * ppb_get_mode()
 *
 */
int
ppb_get_mode(device_t bus)
{
	struct ppb_data *ppb = DEVTOSOFTC(bus);

	/* XXX yet device mode = ppbus mode = chipset mode */
	mtx_assert(ppb->ppc_lock, MA_OWNED);
	return (ppb->mode);
}

/*
 * ppb_set_mode()
 *
 * Set the operating mode of the chipset, return the previous mode
 */
int
ppb_set_mode(device_t bus, int mode)
{
	struct ppb_data *ppb = DEVTOSOFTC(bus);
	int old_mode = ppb_get_mode(bus);

	mtx_assert(ppb->ppc_lock, MA_OWNED);
	if (PPBUS_SETMODE(device_get_parent(bus), mode))
		return (-1);

	/* XXX yet device mode = ppbus mode = chipset mode */
	ppb->mode = (mode & PPB_MASK);

	return (old_mode);
}

/*
 * ppb_write()
 *
 * Write charaters to the port
 */
int
ppb_write(device_t bus, char *buf, int len, int how)
{
#ifdef INVARIANTS
	struct ppb_data *ppb = DEVTOSOFTC(bus);
#endif

	mtx_assert(ppb->ppc_lock, MA_OWNED);
	return (PPBUS_WRITE(device_get_parent(bus), buf, len, how));
}

/*
 * ppb_reset_epp_timeout()
 *
 * Reset the EPP timeout bit in the status register
 */
int
ppb_reset_epp_timeout(device_t bus)
{
#ifdef INVARIANTS
	struct ppb_data *ppb = DEVTOSOFTC(bus);
#endif

	mtx_assert(ppb->ppc_lock, MA_OWNED);
	return(PPBUS_RESET_EPP(device_get_parent(bus)));
}

/*
 * ppb_ecp_sync()
 *
 * Wait for the ECP FIFO to be empty
 */
int
ppb_ecp_sync(device_t bus)
{
#ifdef INVARIANTS
	struct ppb_data *ppb = DEVTOSOFTC(bus);
#endif

	mtx_assert(ppb->ppc_lock, MA_OWNED);
	return (PPBUS_ECP_SYNC(device_get_parent(bus)));
}

/*
 * ppb_get_status()
 *
 * Read the status register and update the status info
 */
int
ppb_get_status(device_t bus, struct ppb_status *status)
{
#ifdef INVARIANTS
	struct ppb_data *ppb = DEVTOSOFTC(bus);
#endif
	register char r;

	mtx_assert(ppb->ppc_lock, MA_OWNED);

	r = status->status = ppb_rstr(bus);

	status->timeout	= r & TIMEOUT;
	status->error	= !(r & nFAULT);
	status->select	= r & SELECT;
	status->paper_end = r & PERROR;
	status->ack	= !(r & nACK);
	status->busy	= !(r & nBUSY);

	return (0);
}

void
ppb_lock(device_t bus)
{
	struct ppb_data *ppb = DEVTOSOFTC(bus);

	mtx_lock(ppb->ppc_lock);
}

void
ppb_unlock(device_t bus)
{
	struct ppb_data *ppb = DEVTOSOFTC(bus);

	mtx_unlock(ppb->ppc_lock);
}

void
_ppb_assert_locked(device_t bus, const char *file, int line)
{
#ifdef INVARIANTS
	struct ppb_data *ppb = DEVTOSOFTC(bus);

	_mtx_assert(ppb->ppc_lock, MA_OWNED, file, line);
#endif
}

void
ppb_init_callout(device_t bus, struct callout *c, int flags)
{
	struct ppb_data *ppb = DEVTOSOFTC(bus);

	callout_init_mtx(c, ppb->ppc_lock, flags);
}

int
ppb_sleep(device_t bus, void *wchan, int priority, const char *wmesg, int timo)
{
	struct ppb_data *ppb = DEVTOSOFTC(bus);

	return (mtx_sleep(wchan, ppb->ppc_lock, priority, wmesg, timo));
}
