/*
 * Copyright (c) 2014 The DragonFly Project.  All rights reserved.
 *
 * This code is derived from software contributed to The DragonFly Project
 * by Matthew Dillon <dillon@backplane.com> and was subsequently ported
 * to FreeBSD by Michael Gmelin <freebsd@grem.de>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Intel fourth generation mobile cpus integrated I2C device, smbus driver.
 *
 * See ig4_reg.h for datasheet reference and notes.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/errno.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sx.h>
#include <sys/syslog.h>
#include <sys/bus.h>

#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/smbus/smbconf.h>

#include "smbus_if.h"

#include <dev/ichiic/ig4_reg.h>
#include <dev/ichiic/ig4_var.h>

static int ig4iic_pci_detach(device_t dev);

#define PCI_CHIP_LYNXPT_LP_I2C_1	0x9c618086
#define PCI_CHIP_LYNXPT_LP_I2C_2	0x9c628086

static int
ig4iic_pci_probe(device_t dev)
{
	switch(pci_get_devid(dev)) {
	case PCI_CHIP_LYNXPT_LP_I2C_1:
		device_set_desc(dev, "Intel Lynx Point-LP I2C Controller-1");
		break;
	case PCI_CHIP_LYNXPT_LP_I2C_2:
		device_set_desc(dev, "Intel Lynx Point-LP I2C Controller-2");
		break;
	default:
		return (ENXIO);
	}
	return (BUS_PROBE_DEFAULT);
}

static int
ig4iic_pci_attach(device_t dev)
{
	ig4iic_softc_t *sc = device_get_softc(dev);
	int error;

	bzero(sc, sizeof(*sc));

	mtx_init(&sc->io_lock, "IG4 I/O lock", NULL, MTX_DEF);
	sx_init(&sc->call_lock, "IG4 call lock");

	sc->dev = dev;
	sc->regs_rid = PCIR_BAR(0);
	sc->regs_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
					  &sc->regs_rid, RF_ACTIVE);
	if (sc->regs_res == NULL) {
		device_printf(dev, "unable to map registers\n");
		ig4iic_pci_detach(dev);
		return (ENXIO);
	}
	sc->intr_rid = 0;
	if (pci_alloc_msi(dev, &sc->intr_rid)) {
		device_printf(dev, "Using MSI\n");
	}
	sc->intr_res = bus_alloc_resource_any(dev, SYS_RES_IRQ,
					  &sc->intr_rid, RF_SHAREABLE | RF_ACTIVE);
	if (sc->intr_res == NULL) {
		device_printf(dev, "unable to map interrupt\n");
		ig4iic_pci_detach(dev);
		return (ENXIO);
	}
	sc->pci_attached = 1;

	error = ig4iic_attach(sc);
	if (error)
		ig4iic_pci_detach(dev);

	return (error);
}

static int
ig4iic_pci_detach(device_t dev)
{
	ig4iic_softc_t *sc = device_get_softc(dev);
	int error;

	if (sc->pci_attached) {
		error = ig4iic_detach(sc);
		if (error)
			return (error);
		sc->pci_attached = 0;
	}

	if (sc->intr_res) {
		bus_release_resource(dev, SYS_RES_IRQ,
				     sc->intr_rid, sc->intr_res);
		sc->intr_res = NULL;
	}
	if (sc->intr_rid != 0)
		pci_release_msi(dev);
	if (sc->regs_res) {
		bus_release_resource(dev, SYS_RES_MEMORY,
				     sc->regs_rid, sc->regs_res);
		sc->regs_res = NULL;
	}
	if (mtx_initialized(&sc->io_lock)) {
		mtx_destroy(&sc->io_lock);
		sx_destroy(&sc->call_lock);
	}

	return (0);
}

static device_method_t ig4iic_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, ig4iic_pci_probe),
	DEVMETHOD(device_attach, ig4iic_pci_attach),
	DEVMETHOD(device_detach, ig4iic_pci_detach),

	/* SMBus methods from ig4_smb.c */
	DEVMETHOD(smbus_callback, ig4iic_smb_callback),
	DEVMETHOD(smbus_quick, ig4iic_smb_quick),
	DEVMETHOD(smbus_sendb, ig4iic_smb_sendb),
	DEVMETHOD(smbus_recvb, ig4iic_smb_recvb),
	DEVMETHOD(smbus_writeb, ig4iic_smb_writeb),
	DEVMETHOD(smbus_writew, ig4iic_smb_writew),
	DEVMETHOD(smbus_readb, ig4iic_smb_readb),
	DEVMETHOD(smbus_readw, ig4iic_smb_readw),
	DEVMETHOD(smbus_pcall, ig4iic_smb_pcall),
	DEVMETHOD(smbus_bwrite, ig4iic_smb_bwrite),
	DEVMETHOD(smbus_bread, ig4iic_smb_bread),
	DEVMETHOD(smbus_trans, ig4iic_smb_trans),

	DEVMETHOD_END
};

static driver_t ig4iic_pci_driver = {
	"ig4iic",
	ig4iic_pci_methods,
	sizeof(struct ig4iic_softc)
};

static devclass_t ig4iic_pci_devclass;

DRIVER_MODULE(ig4iic, pci, ig4iic_pci_driver, ig4iic_pci_devclass, 0, 0);
MODULE_DEPEND(ig4iic, pci, 1, 1, 1);
MODULE_DEPEND(ig4iic, smbus, SMBUS_MINVER, SMBUS_PREFVER, SMBUS_MAXVER);
MODULE_VERSION(ig4iic, 1);
