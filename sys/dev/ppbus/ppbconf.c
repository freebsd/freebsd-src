/*-
 * Copyright (c) 1997 Nicolas Souchu
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
 *	$Id$
 *
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/uio.h>
#include <sys/syslog.h>

#include <machine/clock.h>
#include <machine/lpt.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <i386/isa/isa.h>
#include <i386/isa/isa_device.h>

#include <dev/ppbus/ppbconf.h>

LIST_HEAD(, ppb_data)	ppbdata;	/* list of existing ppbus */

/*
 * Add a null driver so that the linker set always exists.
 */

static struct ppb_driver nulldriver = {
    NULL, NULL, "null"
};
DATA_SET(ppbdriver_set, nulldriver);


/*
 * Parallel Port Bus sleep/wakeup queue.
 */
#define PRIPPB	28	/* PSOCK < PRIPPB < PWAIT 		XXX */

/*
 * ppb_alloc_bus()
 *
 * Allocate area to store the ppbus description.
 * This function is called by ppcattach().
 */
struct ppb_data *
ppb_alloc_bus(void)
{
	struct ppb_data *ppb;
	static int ppbdata_initted = 0;		/* done-init flag */

	ppb = (struct ppb_data *) malloc(sizeof(struct ppb_data),
		M_TEMP, M_NOWAIT);

	/*
	 * Add the new parallel port bus to the list of existing ppbus.
	 */
	if (ppb) {
		bzero(ppb, sizeof(struct ppb_data));

		if (!ppbdata_initted) {		/* list not initialised */
		    LIST_INIT(&ppbdata);
		    ppbdata_initted = 1;
		}
		LIST_INSERT_HEAD(&ppbdata, ppb, ppb_chain);
	} else {
		printf("ppb_alloc_bus: cannot malloc!\n");
	}
	return(ppb);
}

/*
 * ppb_attachdevs()
 *
 * Called by ppcattach(), this function probes the ppbus and
 * attaches found devices.
 */
int
ppb_attachdevs(struct ppb_data *ppb)
{
	int error;
	struct ppb_device *dev;
	struct ppb_driver **p_drvpp, *p_drvp;
	
	LIST_INIT(&ppb->ppb_devs);	/* initialise device/driver list */
	p_drvpp = (struct ppb_driver **)ppbdriver_set.ls_items;
	
	/*
	 * Blindly try all probes here.  Later we should look at
	 * the parallel-port PnP standard, and intelligently seek
	 * drivers based on configuration first.
	 */
	while ((p_drvp = *p_drvpp++) != NULL) {
	    if (p_drvp->probe && (dev = (p_drvp->probe(ppb))) != NULL) {
		/*
		 * Add the device to the list of probed devices.
		 */
		LIST_INSERT_HEAD(&ppb->ppb_devs, dev, chain);
		
		/* Call the device's attach routine */
		(void)p_drvp->attach(dev);
	    }
	}
	return (0);
}

/*
 * ppb_request_bus()
 *
 * Allocate the device to perform transfers.
 *
 * how	: PPB_WAIT or PPB_DONTWAIT
 */
int
ppb_request_bus(struct ppb_device *dev, int how)
{
	int s, error = 0;
	struct ppb_data *ppb = dev->ppb;

	/*
	 * During initialisation, ppb is null.
	 */
	if (!ppb)
		return (0);

	while (error != EINTR) {
		s = splhigh();	
		if (ppb->ppb_owner) {
			splx(s);

			switch (how) {
			case (PPB_WAIT | PPB_INTR):
				
				error = tsleep(ppb, PRIPPB | PCATCH,
						"ppbreq", 0);
				break;
			case (PPB_WAIT):
				error = tsleep(ppb, PRIPPB, "ppbreq", 0);
				break;
			default:
				return EWOULDBLOCK;
				break;
			}

		} else {
			ppb->ppb_owner = dev;

			splx(s);
			return (0);
		}
	}

	return (EINTR);
}

/*
 * ppb_release_bus()
 *
 * Release the device allocated with ppb_request_dev()
 */
int
ppb_release_bus(struct ppb_device *dev)
{
	int s;
	struct ppb_data *ppb = dev->ppb;

	/*
	 * During initialisation, ppb is null.
	 */
	if (!ppb)
		return (0);

	s = splhigh();
	if (ppb->ppb_owner != dev) {
		splx(s);
		return (EACCES);
	}

	ppb->ppb_owner = 0;
	splx(s);

	/*
	 * Wakeup waiting processes.
	 */
	wakeup(ppb);

	return (0);
}

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

	return;
}

/*
 * ppb_reset_epp_timeout()
 *
 * Reset the EPP timeout bit in the status register.
 */
int
ppb_reset_epp_timeout(struct ppb_device *dev)
{
	struct ppb_data *ppb = dev->ppb;

	if (ppb->ppb_owner != dev)
		return (EACCES);

	(*ppb->ppb_link->adapter->reset_epp_timeout)(dev->id_unit);

	return (0);
}

/*
 * ppb_ecp_sync()
 *
 * Wait for the ECP FIFO to be empty.
 */
int
ppb_ecp_sync(struct ppb_device *dev)
{
	struct ppb_data *ppb = dev->ppb;

	if (ppb->ppb_owner != dev)
		return (EACCES);

	(*ppb->ppb_link->adapter->ecp_sync)(dev->id_unit);

	return (0);
}

/*
 * ppb_get_mode()
 *
 * Read the mode (SPP, EPP...) of the chipset.
 */
int
ppb_get_mode(struct ppb_device *dev)
{
	return (dev->ppb->ppb_link->mode);
}

/*
 * ppb_get_epp_protocol()
 *
 * Read the EPP protocol (1.9 or 1.7).
 */
int
ppb_get_epp_protocol(struct ppb_device *dev)
{
	return (dev->ppb->ppb_link->epp_protocol);
}

/*
 * ppb_get_irq()
 *
 * Return the irq, 0 if none.
 */
int
ppb_get_irq(struct ppb_device *dev)
{
	return (dev->ppb->ppb_link->id_irq);
}

/*
 * ppb_get_status()
 *
 * Read the status register and update the status info.
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
	status->paper_end = r & ERROR;
	status->ack	= !(r & nACK);
	status->busy	= !(r & nBUSY);

	return (0);
}
