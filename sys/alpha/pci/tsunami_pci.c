/*-
 * Copyright (c) 1999 Andrew Gallatin
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
 * $FreeBSD: src/sys/alpha/pci/tsunami_pci.c,v 1.6.2.1 2000/06/07 01:45:03 jhb Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <machine/md_var.h>
#include <sys/rman.h>
#include <pci/pcivar.h>
#include <alpha/pci/tsunamireg.h>
#include <alpha/pci/tsunamivar.h>


static devclass_t	pcib_devclass;

int tsunami_hoses[TSUNAMI_MAXHOSES+1] = {1,-1,-1,-1,-1};

int tsunami_maxhoseno = 0;

extern int tsunami_num_pchips;

/* 
 * This comment attempts to explain why and how we are mapping from
 * the DEQ assigned bus numbers to the FreeBSD assigned numbers.
 *
 * FreeBSD must number buses with monotonically increasing numbers for a 
 * variety of reasons (pciconf, newbus, etc).  
 *
 * DEQ numbers them (from what I can tell) on a per-hose bases. And
 * for some reason they seem to always leave bus 1 unused.
 * 
 * When generating config space addrs, we need to know if we are
 * directly on the primary bus on that hose, or if we are behind a ppb.
 * We keep track of this by assigning hoses monotonically increasing
 * numbers.  This fits nicely with DEQ not using bus number 1; I
 * assume that is what it as intended for. I guess we'll see if they 
 * come out with a system using more than one pchip..
 *
 * Next, we must attempt to map the FreeBSD assigned numbers to the
 * numbers assigned by DEQ in order to generate proper config space 
 * addrs.  We store the next number past the 0th bus of our hose and
 * do subtraction to determine what the DEQ number should have been,
 * given a FreeBSD bus number.  This is disgusting & quite possibly
 * wrong.  
 */

int
tsunami_bus_within_hose(int hose, int bus)
{
	if (hose == bus)
		return 0;
	else 
		return ( (bus - tsunami_hoses[hose]) + 1);
}

/* 
 * this function supports pciconf ioctls 
 */

int 
tsunami_hose_from_bus(int bus)
{
	int i;

	if (bus < tsunami_maxhoseno)
		return bus;

	for (i = 1; i <= TSUNAMI_MAXHOSES && tsunami_hoses[i] != -1; i++){
		if(tsunami_hoses[i] >= bus)
			return i-1;
	}

	return i-1;
}


static int
tsunami_pcib_probe(device_t dev)
{
	static int error;
	device_t child;
	int lastbus;

	device_set_desc(dev, "21271 PCI host bus adapter");

	child = device_add_child(dev, "pci", -1);

	if (tsunami_maxhoseno) {
		    lastbus = (device_get_unit(child) - 1);
		    if (lastbus == 0) /* didn't have a ppb on hose 0 */
			    lastbus++;
		    tsunami_hoses[tsunami_maxhoseno] = lastbus;
	}
	if ((error = device_delete_child(dev, child)))
		panic("tsunami_pcib_probe: device_delete_child failed\n");
	
	child = device_add_child(dev, "pci", tsunami_maxhoseno);

	if (tsunami_maxhoseno != device_get_unit(child)) {
		printf("tsunami_pcib_probe: wanted unit %d ", 
		    tsunami_maxhoseno);
		printf(" got unit %d\n", device_get_unit(child));
		panic("tsunami_pcib_probe: incorrect bus numbering");
	}
	tsunami_maxhoseno++;
	return 0;
}

static int
tsunami_pcib_read_ivar(device_t dev, device_t child, int which, u_long *result)
{
	if (which == PCIB_IVAR_HOSE) {
		*result = *(int*) device_get_ivars(dev);
		return 0;
	}
	return ENOENT;
}

static device_method_t tsunami_pcib_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		tsunami_pcib_probe),
	DEVMETHOD(device_attach,	bus_generic_attach),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_read_ivar,	tsunami_pcib_read_ivar),
	DEVMETHOD(bus_alloc_resource,	bus_generic_alloc_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	alpha_platform_pci_setup_intr),
	DEVMETHOD(bus_teardown_intr,	alpha_platform_pci_teardown_intr),

	{ 0, 0 }
};


static driver_t tsunami_pcib_driver = {
	"pcib",
	tsunami_pcib_methods,
	1,
};


DRIVER_MODULE(pcib, tsunami, tsunami_pcib_driver, pcib_devclass, 0, 0);


