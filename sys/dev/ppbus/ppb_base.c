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
 *
 * $FreeBSD: src/sys/dev/ppbus/ppb_base.c,v 1.10.2.1 2000/08/01 23:26:26 n_hibma Exp $
 *
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>

#include <machine/clock.h>

#include <dev/ppbus/ppbconf.h>
  
#include "ppbus_if.h"

#include <dev/ppbus/ppbio.h>
  
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
	int i, j, error;
	char r;

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

		switch (how) {
		case PPB_NOINTR:
			/* wait 10 ms */
			tsleep((caddr_t)bus, PPBPRI, "ppbpoll", hz/100);
			break;

		case PPB_INTR:
		default:
			/* wait 10 ms */
			if (((error = tsleep((caddr_t)bus, PPBPRI | PCATCH,
			    "ppbpoll", hz/100)) != EWOULDBLOCK) != 0) {
				return (error);
			}
			break;
		}
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
	uintptr_t protocol;

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

	if (PPBUS_SETMODE(device_get_parent(bus), mode))
		return -1;

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
	register char r;

	r = status->status = ppb_rstr(bus);

	status->timeout	= r & TIMEOUT;
	status->error	= !(r & nFAULT);
	status->select	= r & SELECT;
	status->paper_end = r & PERROR;
	status->ack	= !(r & nACK);
	status->busy	= !(r & nBUSY);

	return (0);
}
