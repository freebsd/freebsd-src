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
#include <machine/bus_memio.h>
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

#include <dev/hea/eni_stats.h>
#include <dev/hea/eni.h>
#include <dev/hea/eni_suni.h>
#include <dev/hea/eni_var.h>

#include <dev/hea/hea_freebsd.h>

static int hea_pci_probe(device_t);
static int hea_pci_attach(device_t);

#define	ENI_VENDORID		0x111A
#define	ENI_DEVICEID_ENI155PF	0x0000
#define	ENI_DEVICEID_ENI155PA	0x0002

#define	ADP_VENDORID		0x9004
#define	ADP_DEVICEID_AIC5900	0x5900
#define	ADP_DEVICEID_AIC5905	0x5905

struct hea_pci_type {
	u_int16_t	vid;
	u_int16_t	did;
	char *		name;
} hea_pci_devs[] = {
	{ ENI_VENDORID, ENI_DEVICEID_ENI155PF,
		"Efficient Networks 155P-MF1 (FPGA) ATM Adapter" },
	{ ENI_VENDORID, ENI_DEVICEID_ENI155PA,
		"Efficient Networks 155P-MF1 (ASIC) ATM Adapter" },
	{ ADP_VENDORID, ADP_DEVICEID_AIC5900,
		"ANA-5910/5930/5940 ATM155 & 25 LAN Adapter" },
	{ ADP_VENDORID, ADP_DEVICEID_AIC5905,
		"ANA-5910A/5930A/5940A ATM Adapter" },
	{ 0, 0, NULL },
};

static int
hea_pci_probe (dev)
	device_t	dev;
{
	struct hea_pci_type *	t = hea_pci_devs;

	while (t->name != NULL) {
		if ((pci_get_vendor(dev) == t->vid) &&
		    (pci_get_device(dev) == t->did)) {
			device_set_desc(dev, t->name);
			return(0);
		}
		t++;
	}

	return(ENXIO);
}

static int
hea_pci_attach (dev)
	device_t	dev;
{
	struct hea_softc *sc;
	Eni_unit *eup;
	u_int32_t command;
	vm_offset_t va;
	int error;

	sc = device_get_softc(dev);
	eup = &sc->eup;
	error = 0;

	pci_enable_busmaster(dev);

	sc->mem_rid = PCIR_BAR(0);
	sc->mem_type = SYS_RES_MEMORY;
	sc->irq_rid = 0;

	error = hea_alloc(dev);
	if (error) {
		device_printf(dev, "hea_alloc() failed.\n");
		goto fail;
	}

	va = (vm_offset_t) rman_get_virtual(sc->mem);

	eup->eu_base = (Eni_mem)va;
	eup->eu_ram = (Eni_mem)(eup->eu_base + RAM_OFFSET);

	/*
	 * Convert Endianess on DMA
	 */
	command = pci_read_config(dev, PCI_CONTROL_REG, 4);
	command |= ENDIAN_SWAP_DMA;
	pci_write_config(dev, PCI_CONTROL_REG, command, 4);

	/*
	 * Map interrupt in
	 */
	error = bus_setup_intr(dev, sc->irq, INTR_TYPE_NET,
				    hea_intr, sc, &sc->irq_ih);
	if (error) {
		device_printf(dev, "Interrupt handler setup failed.\n");
		goto fail;
	}

	eup->eu_config.ac_bustype = BUS_PCI;
	eup->eu_config.ac_busslot = (pci_get_bus(dev) << 8)| pci_get_slot(dev);

	switch (pci_get_vendor(dev)) {
	case ENI_VENDORID:
		eup->eu_type = TYPE_ENI;	
		break;
	case ADP_VENDORID:
		eup->eu_type = TYPE_ADP;	
		break;
	default:
		eup->eu_type = TYPE_UNKNOWN;	
		break;
	}

	error = hea_attach(dev);
	if (error) {
	        device_printf(dev, "hea_attach() failed.\n");
	        goto fail;
	}

	return (0);

fail:
	hea_detach(dev);

	return (error);
}

static device_method_t hea_pci_methods[] = {
	DEVMETHOD(device_probe,		hea_pci_probe),
	DEVMETHOD(device_attach,	hea_pci_attach),

	DEVMETHOD(device_detach,	hea_detach),

	{ 0, 0 }
};

static driver_t hea_pci_driver = {
	"hea",
	hea_pci_methods,
	sizeof(struct hea_softc)
};

DRIVER_MODULE(hea, pci, hea_pci_driver, hea_devclass, 0, 0);
MODULE_DEPEND(hea, pci, 1, 1, 1);
MODULE_DEPEND(hea, hea, 1, 1, 1);
