/*
 * Copyright (c) 1995 - 2001 John Hay.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY John Hay ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL John Hay BE LIABLE
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

/*
 * Programming assumptions and other issues.
 *
 * The descriptors of a DMA channel will fit in a 16K memory window.
 *
 * The buffers of a transmit DMA channel will fit in a 16K memory window.
 *
 * Only the ISA bus cards with X.21 and V.35 is tested.
 *
 * When interface is going up, handshaking is set and it is only cleared
 * when the interface is down'ed.
 *
 * There should be a way to set/reset Raw HDLC/PPP, Loopback, DCE/DTE,
 * internal/external clock, etc.....
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/bus_pio.h>
#include <machine/bus_memio.h>
#include <sys/rman.h>

#include <isa/isavar.h>
#include "isa_if.h"

#include <dev/ic/hd64570.h>
#include <dev/ar/if_arregs.h>

#ifdef TRACE
#define TRC(x)               x
#else
#define TRC(x)
#endif

#define TRCL(x)              x

static int ar_isa_probe (device_t);
static int ar_isa_attach (device_t);

static struct isa_pnp_id ar_ids[] = {
	{0,		NULL}
};

static device_method_t ar_methods[] = {
	DEVMETHOD(device_probe,		ar_isa_probe),
	DEVMETHOD(device_attach,	ar_isa_attach),
	DEVMETHOD(device_detach,	ar_detach),
	{ 0, 0 }
};

static driver_t ar_isa_driver = {
	"ar",
	ar_methods,
	sizeof (struct ar_hardc)
};

devclass_t ar_devclass;

DRIVER_MODULE(if_ar, isa, ar_isa_driver, ar_devclass, 0, 0);

/*
 * Probe to see if it is there.
 * Get its information and fill it in.
 */
static int
ar_isa_probe(device_t device)
{
	int error;
	u_long membase, memsize, port_start, port_count;

	error = ISA_PNP_PROBE(device_get_parent(device), device, ar_ids);
	if(error == ENXIO || error == 0)
		return (error);

	if((error = ar_allocate_ioport(device, 0, ARC_IO_SIZ))) {
		return (ENXIO);
	}

	/*
	 * Now see if the card is realy there.
	 *
	 * XXX For now I just check the undocumented ports
	 * for "570". We will probably have to do more checking.
	 */
	error = bus_get_resource(device, SYS_RES_IOPORT, 0, &port_start,
	    &port_count);

	if((inb(port_start + AR_ID_5) != '5') ||
	   (inb(port_start + AR_ID_7) != '7') ||
	   (inb(port_start + AR_ID_0) != '0')) {
		ar_deallocate_resources(device);
		return (ENXIO);
	}
	membase = bus_get_resource_start(device, SYS_RES_MEMORY, 0);
	memsize = inb(port_start + AR_REV);
	memsize = 1 << ((memsize & AR_WSIZ_MSK) >> AR_WSIZ_SHFT);
	memsize *= ARC_WIN_SIZ;
	error = bus_set_resource(device, SYS_RES_MEMORY, 0, membase, memsize);
	ar_deallocate_resources(device);

	return (error);
}

/*
 * Malloc memory for the softc structures.
 * Reset the card to put it in a known state.
 * Register the ports on the adapter.
 * Fill in the info for each port.
 * Attach each port to sppp and bpf.
 */
static int
ar_isa_attach(device_t device)
{
	u_int tmp;
	u_long irq, junk;
	struct ar_hardc *hc;

	hc = (struct ar_hardc *)device_get_softc(device);
	if(ar_allocate_ioport(device, 0, ARC_IO_SIZ))
		return (ENXIO);
	hc->bt = rman_get_bustag(hc->res_ioport);
	hc->bh = rman_get_bushandle(hc->res_ioport);

	hc->iobase = rman_get_start(hc->res_ioport);

	tmp = inb(hc->iobase + AR_BMI);
	hc->bustype = tmp & AR_BUS_MSK;
	hc->memsize = (tmp & AR_MEM_MSK) >> AR_MEM_SHFT;
	hc->memsize = 1 << hc->memsize;
	hc->memsize <<= 16;
	hc->interface[0] = (tmp & AR_IFACE_MSK);
	hc->interface[1] = hc->interface[0];
	hc->interface[2] = hc->interface[0];
	hc->interface[3] = hc->interface[0];
	tmp = inb(hc->iobase + AR_REV);
	hc->revision = tmp & AR_REV_MSK;
	hc->winsize = 1 << ((tmp & AR_WSIZ_MSK) >> AR_WSIZ_SHFT);
	hc->winsize *= ARC_WIN_SIZ;
	hc->winmsk = hc->winsize - 1;
	hc->numports = inb(hc->iobase + AR_PNUM);
	hc->handshake = inb(hc->iobase + AR_HNDSH);

	if(ar_allocate_memory(device, 0, hc->winsize))
		return (ENXIO);

	hc->mem_start = rman_get_virtual(hc->res_memory);
	hc->mem_end = hc->mem_start + hc->winsize;
	hc->cunit = device_get_unit(device);

	switch(hc->interface[0]) {
	case AR_IFACE_EIA_232:
		printf("ar%d: The EIA 232 interface is not supported.\n",
			hc->cunit);
		ar_deallocate_resources(device);
		return (ENXIO);
	case AR_IFACE_V_35:
		break;
	case AR_IFACE_EIA_530:
		printf("ar%d: WARNING: The EIA 530 interface is untested.\n",
			hc->cunit);
		break;
	case AR_IFACE_X_21:
		break;
	case AR_IFACE_COMBO:
		printf("ar%d: WARNING: The COMBO interface is untested.\n",
			hc->cunit);
		break;
	}

	/*
	 * Do a little sanity check.
	 */
	if((hc->numports > NPORT) || (hc->memsize > (512*1024))) {
		ar_deallocate_resources(device);
		return (ENXIO);
	}

	if(ar_allocate_irq(device, 0, 1))
		return (ENXIO);

	if(bus_get_resource(device, SYS_RES_IRQ, 0, &irq, &junk)) {
		ar_deallocate_resources(device);
		return (ENXIO);
	}
	hc->isa_irq = irq;

	if(ar_attach(device)) {
		ar_deallocate_resources(device);
		return (ENXIO);
	}

	return (0);
}







/*
 ********************************* END ************************************
 */
