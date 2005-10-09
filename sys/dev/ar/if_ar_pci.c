/*-
 * Copyright (c) 1999 - 2001 John Hay.
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
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/ic/hd64570.h>
#include <dev/ar/if_arregs.h>

#ifdef TRACE
#define TRC(x)               x
#else
#define TRC(x)
#endif

#define TRCL(x)              x

static int	ar_pci_probe(device_t);
static int	ar_pci_attach(device_t);

static device_method_t ar_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ar_pci_probe),
	DEVMETHOD(device_attach,	ar_pci_attach),
	DEVMETHOD(device_detach,	ar_detach),
	{ 0, 0 }
};

static driver_t ar_pci_driver = {
	"ar",
	ar_pci_methods,
	sizeof(struct ar_hardc),
};

DRIVER_MODULE(ar, pci, ar_pci_driver, ar_devclass, 0, 0);
MODULE_DEPEND(ar, pci, 1, 1, 1);

static int
ar_pci_probe(device_t device)
{
	u_int32_t	type = pci_get_devid(device);

	switch(type) {
	case 0x5012114f:
		device_set_desc(device, "Digi SYNC/570i-PCI 2 port");
		return (BUS_PROBE_DEFAULT);
		break;
	case 0x5010114f:
		printf("Digi SYNC/570i-PCI 2 port (mapped below 1M)\n");
		printf("Please change the jumper to select linear mode.\n");
		break;
	case 0x5013114f:
		device_set_desc(device, "Digi SYNC/570i-PCI 4 port");
		return (BUS_PROBE_DEFAULT);
		break;
	case 0x5011114f:
		printf("Digi SYNC/570i-PCI 4 port (mapped below 1M)\n");
		printf("Please change the jumper to select linear mode.\n");
		break;
	default:
		break;
	}
	return (ENXIO);
}

static int
ar_pci_attach(device_t device)
{
	int error;
	u_int i, tmp;
	struct ar_hardc *hc;

	hc = (struct ar_hardc *)device_get_softc(device);
	bzero(hc, sizeof(struct ar_hardc));

	error = ar_allocate_plx_memory(device, 0x10, 1);
	if(error)
		goto errexit;

	error = ar_allocate_memory(device, 0x18, 1);
	if(error)
		goto errexit;

	error = ar_allocate_irq(device, 0, 1);
	if(error)
		goto errexit;

	hc->mem_start = rman_get_virtual(hc->res_memory);

	hc->cunit = device_get_unit(device);
	hc->sca[0] = (sca_regs *)(hc->mem_start + AR_PCI_SCA_1_OFFSET);
	hc->sca[1] = (sca_regs *)(hc->mem_start + AR_PCI_SCA_2_OFFSET);
	hc->orbase = (u_char *)(hc->mem_start + AR_PCI_ORBASE_OFFSET);

	tmp = hc->orbase[AR_BMI * 4];
	hc->bustype = tmp & AR_BUS_MSK;
	hc->memsize = (tmp & AR_MEM_MSK) >> AR_MEM_SHFT;
	hc->memsize = 1 << hc->memsize;
	hc->memsize <<= 16;
	hc->interface[0] = (tmp & AR_IFACE_MSK);
	tmp = hc->orbase[AR_REV * 4];
	hc->revision = tmp & AR_REV_MSK;
	hc->winsize = (1 << ((tmp & AR_WSIZ_MSK) >> AR_WSIZ_SHFT)) * 16 * 1024;
	hc->mem_end = (caddr_t)(hc->mem_start + hc->winsize);
	hc->winmsk = hc->winsize - 1;
	hc->numports = hc->orbase[AR_PNUM * 4];
	hc->handshake = hc->orbase[AR_HNDSH * 4];

	for(i = 1; i < hc->numports; i++)
		hc->interface[i] = hc->interface[0];

	TRC(printf("arp%d: bus %x, rev %d, memstart %p, winsize %d, "
	    "winmsk %x, interface %x\n",
	    unit, hc->bustype, hc->revision, hc->mem_start, hc->winsize,
	    hc->winmsk, hc->interface[0]));

	ar_attach(device);

	/* Magic to enable the card to generate interrupts. */
	bus_space_write_1(rman_get_bustag(hc->res_plx_memory),
	    rman_get_bushandle(hc->res_plx_memory), 0x69, 0x09);

	return (0);

errexit:
	ar_deallocate_resources(device);
	return (ENXIO);
}

