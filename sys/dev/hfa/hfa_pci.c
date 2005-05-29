/*-
 * Copyright (c) 2002 Matthew N. Dodd <winter@jurai.net>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 *
 * ===================================
 * HARP  |  Host ATM Research Platform
 * ===================================
 *
 *
 * This Host ATM Research Platform ("HARP") file (the "Software") is
 * made available by Network Computing Services, Inc. ("NetworkCS")
 * "AS IS".  NetworkCS does not provide maintenance, improvements or
 * support of any kind.
 *
 * NETWORKCS MAKES NO WARRANTIES OR REPRESENTATIONS, EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE, AS TO ANY ELEMENT OF THE
 * SOFTWARE OR ANY SUPPORT PROVIDED IN CONNECTION WITH THIS SOFTWARE.
 * In no event shall NetworkCS be responsible for any damages, including
 * but not limited to consequential damages, arising from or relating to
 * any use of the Software or related support.
 *
 * Copyright 1994-1998 Network Computing Services, Inc.
 *
 * Copies of this Software may be made, however, the above copyright
 * notice must be reproduced on all copies.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <sys/bus.h>
#include <sys/conf.h>

#include <sys/module.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h> 

#include <net/if.h>
#include <netatm/port.h>
#include <netatm/queue.h>
#include <netatm/atm.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_sap.h>
#include <netatm/atm_cm.h>
#include <netatm/atm_if.h>
#include <netatm/atm_stack.h>
#include <netatm/atm_pcb.h>
#include <netatm/atm_var.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/hfa/fore.h>
#include <dev/hfa/fore_aali.h>
#include <dev/hfa/fore_slave.h>
#include <dev/hfa/fore_stats.h>
#include <dev/hfa/fore_var.h>
#include <dev/hfa/fore_include.h>

#include <dev/hfa/hfa_freebsd.h>

static int hfa_pci_probe(device_t);
static int hfa_pci_attach(device_t);

#define	FORE_PCA200EPC_ID	0x0300

static int
hfa_pci_probe (dev)
	device_t dev;
{
	if ((pci_get_vendor(dev) == FORE_VENDOR_ID) &&
	    (pci_get_device(dev) == FORE_PCA200EPC_ID)) {
		device_set_desc(dev, "FORE Systems PCA-200EPC ATM");
		return (BUS_PROBE_DEFAULT);
	}

	return (ENXIO);
}

static int
hfa_pci_attach (dev)
	device_t dev;
{
	struct hfa_softc *sc;
	Fore_unit *fup;
	u_int32_t command;
	vm_offset_t va;
	int error;

	sc = device_get_softc(dev);
	fup = &sc->fup;
	error = 0;

	pci_enable_busmaster(dev);

	sc->mem_rid = PCA200E_PCI_MEMBASE;
	sc->mem_type = SYS_RES_MEMORY;
	sc->irq_rid = 0;

	error = hfa_alloc(dev);
	if (error) {
		device_printf(dev, "hfa_alloc() failed.\n");
		goto fail;
	}

	va = (vm_offset_t) rman_get_virtual(sc->mem);

	fup->fu_ram = (Fore_mem *)va;
	fup->fu_ramsize = PCA200E_RAM_SIZE;
	fup->fu_mon = (Mon960 *)(fup->fu_ram + MON960_BASE);
	fup->fu_ctlreg = (Fore_reg *)(va + PCA200E_HCR_OFFSET);
	fup->fu_imask = (Fore_reg *)(va + PCA200E_IMASK_OFFSET);
	fup->fu_psr = (Fore_reg *)(va + PCA200E_PSR_OFFSET);

	/*
	 * Convert Endianess of Slave RAM accesses
	 */
	command = pci_read_config(dev, PCA200E_PCI_MCTL, 4);
	command |= PCA200E_MCTL_SWAP;
	pci_write_config(dev, PCA200E_PCI_MCTL, command, 4);

        /*
         * Map interrupt in
         */
	error = bus_setup_intr(dev, sc->irq, INTR_TYPE_NET,
	                            hfa_intr, sc, &sc->irq_ih);
	if (error) {
	        device_printf(dev, "Interrupt handler setup failed.\n");
	        goto fail;
	}

	fup->fu_config.ac_bustype = BUS_PCI;
	fup->fu_config.ac_busslot = (pci_get_bus(dev) << 8)| pci_get_slot(dev);

	switch (pci_get_device(dev)) {
	case FORE_PCA200EPC_ID:
		fup->fu_config.ac_device = DEV_FORE_PCA200E;
		break;
	default:
		fup->fu_config.ac_device = DEV_UNKNOWN;
		break;
	}

	error = hfa_attach(dev);
	if (error) {
		device_printf(dev, "hfa_attach() failed.\n");
		goto fail;
	}

	return (0);

fail:
	hfa_detach(dev);

	return (error);
}

static device_method_t hfa_pci_methods[] = {
	DEVMETHOD(device_probe,		hfa_pci_probe),
	DEVMETHOD(device_attach,	hfa_pci_attach),

	DEVMETHOD(device_detach,	hfa_detach),

	{ 0, 0 }
};

static driver_t hfa_pci_driver = {
	"hfa",
	hfa_pci_methods,
	sizeof(struct hfa_softc)
};

DRIVER_MODULE(hfa, pci, hfa_pci_driver, hfa_devclass, 0, 0);
MODULE_DEPEND(hfa, hfa, 1, 1, 1);
MODULE_DEPEND(hfa, pci, 1, 1, 1);
