/*
 * Copyright (c) 1998, Larry Lile
 * All rights reserved.
 *
 * For latest sources and information on this driver, please
 * go to http://anarchy.stdio.com.
 *
 * Questions, comments or suggestions should be directed to
 * Larry Lile <lile@stdio.com>.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/iso88025.h>
#include <net/if_media.h>
#include <net/bpf.h>

#include <vm/vm.h>              /* for vtophys */
#include <vm/pmap.h>            /* for vtophys */

#include <machine/bus.h>
#include <machine/resource.h>

#include <sys/bus.h>
#include <sys/rman.h>

#if (__FreeBSD_version < 500000)
#include <pci/pcireg.h>
#include <pci/pcivar.h>
#else
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#endif

#include "contrib/dev/oltr/trlld.h"
#include "contrib/dev/oltr/if_oltrvar.h"

static int oltr_pci_probe	__P((device_t));
static int oltr_pci_attach	__P((device_t));
static int oltr_pci_detach	__P((device_t));
static void oltr_pci_shutdown	__P((device_t));

extern TRlldDriver_t LldDriver;

struct AdapterNameEntry {
	int type;
	char *name; } ;

static struct AdapterNameEntry AdapterNameList[] = {
	{ 10, "Olicom PCI 16/4 Adapter (OC-3136)" },
	{ 11, "Olicom PCI 16/4 Adapter (OC-3136)" },
	{ 12, "Olicom PCI/II 16/4 Adapter (OC-3137)" },
	{ 13, "Olicom PCI 16/4 Adapter (OC-3139)" },
	{ 14, "Olicom RapidFire 3140 16/4 PCI Adapter (OC-3140)" },
	{ 15, "Olicom RapidFire 3141 Fiber Adapter (OC-3141)" },
	{ 19, "Olicom RapidFire 3540 100/16/4 Adapter (OC-3540)" },
	{  0, "Olicom Unsupported Adapter" }
};

static device_method_t oltr_methods[] = {
	DEVMETHOD(device_probe,		oltr_pci_probe),
	DEVMETHOD(device_attach,	oltr_pci_attach),
	DEVMETHOD(device_detach,	oltr_pci_detach),
	DEVMETHOD(device_shutdown,	oltr_pci_shutdown),
        { 0, 0 }
};

static driver_t oltr_driver = {
	"oltr",
	oltr_methods,
	sizeof(struct oltr_softc)
};

static devclass_t oltr_devclass;

DRIVER_MODULE(oltr, pci, oltr_driver, oltr_devclass, 0, 0);
MODULE_DEPEND(oltr, pci, 1, 1, 1);
MODULE_DEPEND(oltr, iso88025, 1, 1, 1);

static int
oltr_pci_probe(device_t dev)
{
        int                     i, rc;
        char                    PCIConfigHeader[64];
        TRlldAdapterConfig_t    config;
	struct AdapterNameEntry	*list = AdapterNameList;

        if ((pci_get_vendor(dev) == PCI_VENDOR_OLICOM) &&
           ((pci_get_device(dev) == 0x0001) ||
	    (pci_get_device(dev) == 0x0004) ||
            (pci_get_device(dev) == 0x0005) ||
	    (pci_get_device(dev) == 0x0007) ||
            (pci_get_device(dev) == 0x0008))) {

                for (i = 0; i < sizeof(PCIConfigHeader); i++)
                        PCIConfigHeader[i] = pci_read_config(dev, i, 1);

                rc = TRlldPCIConfig(&LldDriver, &config, PCIConfigHeader);
                if (rc == TRLLD_PCICONFIG_FAIL) {
                        device_printf(dev, "TRlldPciConfig failed!\n");
                        return(ENXIO);
                }
                if (rc == TRLLD_PCICONFIG_VERSION) {
                        device_printf(dev, "wrong LLD version\n");
                        return(ENXIO);
                }
		while (list->type != 0 && list->type != config.type)
			list++;
                device_set_desc(dev, list->name);
                return(0);
        }
        return(ENXIO);
}

static int
oltr_pci_attach(device_t dev)
{
        int 			i, s, scratch_size;
	u_long 			command;
	char 			PCIConfigHeader[64];
	struct oltr_softc		*sc = device_get_softc(dev);

        s = splimp();

       	bzero(sc, sizeof(struct oltr_softc));
	sc->unit = device_get_unit(dev);
	sc->state = OL_UNKNOWN;

	for (i = 0; i < sizeof(PCIConfigHeader); i++)
		PCIConfigHeader[i] = pci_read_config(dev, i, 1);

	switch(TRlldPCIConfig(&LldDriver, &sc->config, PCIConfigHeader)) {
	case TRLLD_PCICONFIG_OK:
		break;
	case TRLLD_PCICONFIG_SET_COMMAND:
		device_printf(dev, "enabling bus master mode\n");
		command = pci_read_config(dev, PCIR_COMMAND, 4);
		pci_write_config(dev, PCIR_COMMAND,
			(command | PCIM_CMD_BUSMASTEREN), 4);
		command = pci_read_config(dev, PCIR_COMMAND, 4);
		if (!(command & PCIM_CMD_BUSMASTEREN)) {
			device_printf(dev, "failed to enable bus master mode\n");
			goto config_failed;
		}
		break;
	case TRLLD_PCICONFIG_FAIL:
		device_printf(dev, "TRlldPciConfig failed!\n");
		goto config_failed;
		break;
	case TRLLD_PCICONFIG_VERSION:
		device_printf(dev, "wrong LLD version\n");
		goto config_failed;
		break;
	}
	device_printf(dev, "MAC address %6D\n", sc->config.macaddress, ":");

	scratch_size = TRlldAdapterSize();
	if (bootverbose)
		device_printf(dev, "adapter memory block size %d bytes\n", scratch_size);
	sc->TRlldAdapter = (TRlldAdapter_t)malloc(scratch_size, M_DEVBUF, M_NOWAIT);
	if (sc->TRlldAdapter == NULL) {
		device_printf(dev, "couldn't allocate scratch buffer (%d bytes)\n", scratch_size);
		goto config_failed;
	}
	sc->TRlldAdapter_phys = vtophys(sc->TRlldAdapter);

	/*
	 * Allocate RX/TX Pools
	 */
	for (i = 0; i < RING_BUFFER_LEN; i++) {
		sc->rx_ring[i].index = i;
		sc->rx_ring[i].data = (char *)malloc(RX_BUFFER_LEN, M_DEVBUF, M_NOWAIT);
		sc->rx_ring[i].address = vtophys(sc->rx_ring[i].data);
		sc->tx_ring[i].index = i;
		sc->tx_ring[i].data = (char *)malloc(TX_BUFFER_LEN, M_DEVBUF, M_NOWAIT);
		sc->tx_ring[i].address = vtophys(sc->tx_ring[i].data);
		if ((!sc->rx_ring[i].data) || (!sc->tx_ring[i].data)) {
			device_printf(dev, "unable to allocate ring buffers\n");
			while (i > 0) {
				if (sc->rx_ring[i].data)
					free(sc->rx_ring[i].data, M_DEVBUF);
				if (sc->tx_ring[i].data)
					free(sc->tx_ring[i].data, M_DEVBUF);
				i--;
			}
			goto config_failed;
		}
	}
	
	if (oltr_attach(dev) == -1)
		goto config_failed;

        splx(s);
        return(0);

config_failed:

        splx(s);
        return(ENXIO);
}

static int
oltr_pci_detach(device_t dev)
{
	struct oltr_softc	*sc = device_get_softc(dev);
	struct ifnet		*ifp = &sc->arpcom.ac_if;
	int s, i;

	device_printf(dev, "driver unloading\n");

	s = splimp();

	iso88025_ifdetach(ifp, ISO88025_BPF_SUPPORTED);
	if (sc->state > OL_CLOSED)
		oltr_stop(sc);

	untimeout(oltr_poll, (void *)sc, sc->oltr_poll_ch);
	/*untimeout(oltr_stat, (void *)sc, sc->oltr_stat_ch);*/

	bus_teardown_intr(dev, sc->irq_res, sc->oltr_intrhand);
	bus_release_resource(dev, SYS_RES_IRQ, 0, sc->irq_res);

	/* Deallocate all dynamic memory regions */
	for (i = 0; i < RING_BUFFER_LEN; i++) {
		free(sc->rx_ring[i].data, M_DEVBUF);
		free(sc->tx_ring[i].data, M_DEVBUF);
	}
	if (sc->work_memory)
		free(sc->work_memory, M_DEVBUF);
	free(sc->TRlldAdapter, M_DEVBUF);

	(void)splx(s);

	return(0);
}

static void
oltr_pci_shutdown(device_t dev)
{
	struct oltr_softc		*sc = device_get_softc(dev);

	device_printf(dev, "oltr_pci_shutdown called\n");

	if (sc->state > OL_CLOSED)
		oltr_stop(sc);

	return;
}
