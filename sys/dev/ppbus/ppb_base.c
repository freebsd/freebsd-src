/*-
 * Copyright (c) 1997, 1998 Nicolas Souchu
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
 *	$Id: ppb_base.c,v 1.7 1999/01/27 20:09:19 dillon Exp $
 *
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <machine/clock.h>

#include <dev/ppbus/ppbconf.h>

/*
 * ppb_intr()
 *
 * Function called by ppcintr() when an intr occurs.
 */
void
ppb_intr(struct ppb_link *pl)
{
	struct ppb_data *ppb = pl->ppbus;

	/*
	 * Call chipset dependent code.
	 * Should be filled at chipset initialisation if needed.
	 */
	if (pl->adapter->intr_handler)
		(*pl->adapter->intr_handler)(pl->adapter_unit);

	/*
	 * Call upper handler iff the bus is owned by a device and
	 * this device has specified an interrupt handler.
	 */
	if (ppb->ppb_owner && ppb->ppb_owner->intr)
		(*ppb->ppb_owner->intr)(ppb->ppb_owner->id_unit);
	if (ppb->ppb_owner && ppb->ppb_owner->bintr)
		(*ppb->ppb_owner->bintr)(ppb->ppb_owner);

	return;
}

/*
 * ppb_poll_device()
 *
 * Polls the device
 *
 * max is a delay in 10-milliseconds
 */
int
ppb_poll_device(struct ppb_device *dev, int max,
		char mask, char status, int how)
{
	int i, j, error;
	char r;

	/* try at least up to 10ms */
	for (j = 0; j < ((how & PPB_POLL) ? max : 1); j++) {
		for (i = 0; i < 10000; i++) {
			r = ppb_rstr(dev);
			DELAY(1);
			if ((r & mask) == status)
				return (0);
		}
	}

	if (!(how & PPB_POLL)) {
	   for (i = 0; max == PPB_FOREVER || i < max-1; i++) {
		if ((ppb_rstr(dev) & mask) == status)
			return (0);

		switch (how) {
		case PPB_NOINTR:
			/* wait 10 ms */
			tsleep((caddr_t)dev, PPBPRI, "ppbpoll", hz/100);
			break;

		case PPB_INTR:
		default:
			/* wait 10 ms */
			if (((error = tsleep((caddr_t)dev, PPBPRI | PCATCH,
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
 * ppb_set_mode()
 *
 * Set the operating mode of the chipset
 */
int
ppb_set_mode(struct ppb_device *dev, int mode)
{
	struct ppb_data *ppb = dev->ppb;
	int old_mode = ppb_get_mode(dev);

	if ((*ppb->ppb_link->adapter->setmode)(
			ppb->ppb_link->adapter_unit, mode))
		return (-1);

	/* XXX yet device mode = ppbus mode = chipset mode */
	dev->mode = ppb->mode = (mode & PPB_MASK);

	return (old_mode);
}

/*
 * ppb_write()
 *
 * Write charaters to the port
 */
int
ppb_write(struct ppb_device *dev, char *buf, int len, int how)
{
	struct ppb_data *ppb = dev->ppb;

	return (ppb->ppb_link->adapter->write(ppb->ppb_link->adapter_unit,
						buf, len, how));
}

/*
 * ppb_reset_epp_timeout()
 *
 * Reset the EPP timeout bit in the status register
 */
int
ppb_reset_epp_timeout(struct ppb_device *dev)
{
	struct ppb_data *ppb = dev->ppb;

	if (ppb->ppb_owner != dev)
		return (EACCES);

	(*ppb->ppb_link->adapter->reset_epp_timeout)(ppb->ppb_link->adapter_unit);

	return (0);
}

/*
 * ppb_ecp_sync()
 *
 * Wait for the ECP FIFO to be empty
 */
int
ppb_ecp_sync(struct ppb_device *dev)
{
	struct ppb_data *ppb = dev->ppb;

	if (ppb->ppb_owner != dev)
		return (EACCES);

	(*ppb->ppb_link->adapter->ecp_sync)(ppb->ppb_link->adapter_unit);

	return (0);
}

/*
 * ppb_get_status()
 *
 * Read the status register and update the status info
 */
int
ppb_get_status(struct ppb_device *dev, struct ppb_status *status)
{
	struct ppb_data *ppb = dev->ppb;
	register char r;

	if (ppb->ppb_owner != dev)
		return (EACCES);

	r = status->status = ppb_rstr(dev);

	status->timeout	= r & TIMEOUT;
	status->error	= !(r & nFAULT);
	status->select	= r & SELECT;
	status->paper_end = r & PERROR;
	status->ack	= !(r & nACK);
	status->busy	= !(r & nBUSY);

	return (0);
}
