/*-
 * Copyright (c) 2001,2002 Søren Schmidt <sos@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/stdarg.h>
#include <machine/resource.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <pci/pcivar.h>
#include <pci/pcireg.h>

#include "dev/pst/pst-iop.h"

static int
iop_pci_probe(device_t dev)
{
    /* tested with actual hardware kindly donated by Promise */
    if (pci_get_devid(dev) == 0x19628086 && pci_get_subvendor(dev) == 0x105a) {
	device_set_desc(dev, "Promise SuperTrak SX6000 ATA RAID controller");
	return 0;
    } 

    /* this should work as well (not tested no hardware) */
    if (pci_get_devid(dev) == 0x09628086 && pci_get_subvendor(dev) == 0x105a) {
	device_set_desc(dev, "Promise SuperTrak 100 ATA RAID controller");
	return 0;
    } 

    return ENXIO;
}

static int
iop_pci_attach(device_t dev)
{
    struct iop_softc *sc = device_get_softc(dev);
    int rid;

    bzero(sc, sizeof(struct iop_softc));

    /* get resources */
    rid = 0x10;
    sc->r_mem = 
	bus_alloc_resource(dev, SYS_RES_MEMORY, &rid, 0, ~0, 1, RF_ACTIVE);

    if (!sc->r_mem)
	return 0;

    rid = 0x00;
    sc->r_irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0,
				   1, RF_SHAREABLE | RF_ACTIVE);

    /* now setup the infrastructure to talk to the device */
    pci_write_config(dev, PCIR_COMMAND,
		     pci_read_config(dev, PCIR_COMMAND, 1) |
		     PCIM_CMD_MEMEN | PCIM_CMD_BUSMASTEREN, 1);

    sc->ibase = rman_get_virtual(sc->r_mem);
    sc->phys_ibase = vtophys(sc->ibase);
    sc->reg = (struct i2o_registers *)sc->ibase;
    sc->dev = dev;

    if (!iop_init(sc))
	return 0;

    return bus_generic_attach(dev);
}

static device_method_t pst_pci_methods[] = {
    DEVMETHOD(device_probe,		iop_pci_probe),
    DEVMETHOD(device_attach,		iop_pci_attach),
    { 0, 0 }
};

static driver_t pst_pci_driver = {
    "pstpci",
    pst_pci_methods,
    sizeof(struct iop_softc),
};

static devclass_t pst_pci_devclass;

DRIVER_MODULE(pstpci, pci, pst_pci_driver, pst_pci_devclass, 0, 0);
