/*-
 * Copyright (c) 2002 Adaptec Inc.
 * All rights reserved.
 *
 * Written by: David Jeffery
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
 * $FreeBSD$
 */


#include <dev/ips/ips.h>
static int ips_pci_free(ips_softc_t *sc);

static int ips_pci_probe(device_t dev)
{

        if ((pci_get_vendor(dev) == IPS_VENDOR_ID) &&
	    (pci_get_device(dev) == IPS_MORPHEUS_DEVICE_ID)) {
		device_set_desc(dev, "IBM ServeRAID Adapter");
                return 0;
        } else if ((pci_get_vendor(dev) == IPS_VENDOR_ID) &&
	    (pci_get_device(dev) == IPS_COPPERHEAD_DEVICE_ID)) {
		device_set_desc(dev, "IBM ServeRAID Adapter");
		return (0);
        }
        return(ENXIO);
}

static int ips_pci_attach(device_t dev)
{
        u_int32_t command;
        int tval;
        ips_softc_t *sc;


	tval = 0;
	if (resource_int_value(device_get_name(dev), device_get_unit(dev),
	    "disable", &tval) == 0 && tval) {
		device_printf(dev, "device is disabled\n");
		/* but return 0 so the !$)$)*!$*) unit isn't reused */
		return (0);
	}
        DEVICE_PRINTF(1, dev, "in attach.\n");
        sc = (ips_softc_t *)device_get_softc(dev);
        if(!sc){
                printf("how is sc NULL?!\n");
                return (ENXIO);
        }
        bzero(sc, sizeof(ips_softc_t));
        sc->dev = dev;

        if(pci_get_device(dev) == IPS_MORPHEUS_DEVICE_ID){
		sc->ips_adapter_reinit = ips_morpheus_reinit;
                sc->ips_adapter_intr = ips_morpheus_intr;
		sc->ips_issue_cmd    = ips_issue_morpheus_cmd;
        } else if(pci_get_device(dev) == IPS_COPPERHEAD_DEVICE_ID){
		sc->ips_adapter_reinit = ips_copperhead_reinit;
                sc->ips_adapter_intr = ips_copperhead_intr;
		sc->ips_issue_cmd    = ips_issue_copperhead_cmd;
	} else
                goto error;
        /* make sure busmastering is on */
        command = pci_read_config(dev, PCIR_COMMAND, 1);
	command |= PCIM_CMD_BUSMASTEREN;
	pci_write_config(dev, PCIR_COMMAND, command, 1);
        /* seting up io space */
        sc->iores = NULL;
        if(command & PCIM_CMD_MEMEN){
                PRINTF(10, "trying MEMIO\n");
		if(pci_get_device(dev) == IPS_MORPHEUS_DEVICE_ID)
                	sc->rid = PCIR_MAPS;
		else
			sc->rid = PCIR_MAPS + 4;
                sc->iotype = SYS_RES_MEMORY;
                sc->iores = bus_alloc_resource(dev, sc->iotype, &sc->rid, 0, ~0, 1, RF_ACTIVE);
        }
        if(!sc->iores && command & PCIM_CMD_PORTEN){
                PRINTF(10, "trying PORTIO\n");
                sc->rid = PCIR_MAPS;
                sc->iotype = SYS_RES_IOPORT;
                sc->iores = bus_alloc_resource(dev, sc->iotype, &sc->rid, 0, ~0, 1, RF_ACTIVE);
        }
        if(sc->iores == NULL){
                device_printf(dev, "resource allocation failed\n");
                return (ENXIO);
        }
        sc->bustag = rman_get_bustag(sc->iores);
        sc->bushandle = rman_get_bushandle(sc->iores);
        /*allocate an interrupt. when does the irq become active? after leaving attach? */
        sc->irqrid = 0;
        if(!(sc->irqres = bus_alloc_resource(dev, SYS_RES_IRQ, &sc->irqrid, 0, ~0, 1, RF_SHAREABLE | RF_ACTIVE))){
                device_printf(dev, "irq allocation failed\n");
                goto error;
        }
	if(bus_setup_intr(dev, sc->irqres, INTR_TYPE_BIO, sc->ips_adapter_intr, sc, &sc->irqcookie)){
                device_printf(dev, "irq setup failed\n");
                goto error;
        }
	if (bus_dma_tag_create(	/* parent    */	NULL,
				/* alignemnt */	1,
				/* boundary  */	0,
				/* lowaddr   */	BUS_SPACE_MAXADDR_32BIT,
				/* highaddr  */	BUS_SPACE_MAXADDR,
				/* filter    */	NULL,
				/* filterarg */	NULL,
				/* maxsize   */	BUS_SPACE_MAXSIZE_32BIT,
				/* numsegs   */	IPS_MAX_SG_ELEMENTS,
				/* maxsegsize*/	BUS_SPACE_MAXSIZE_32BIT,
				/* flags     */	0,
				&sc->adapter_dmatag) != 0) {
                printf("IPS can't alloc dma tag\n");
                goto error;
        }
	if(ips_adapter_init(sc))
		goto error;
        sc->configured = 1;
        return 0;
error:
	ips_pci_free(sc);
        return (ENXIO);
}

static int ips_pci_free(ips_softc_t *sc)
{
	if(sc->adapter_dmatag)
		bus_dma_tag_destroy(sc->adapter_dmatag);
	if(sc->irqcookie)
                bus_teardown_intr(sc->dev, sc->irqres, sc->irqcookie);
        if(sc->irqres)
               bus_release_resource(sc->dev, SYS_RES_IRQ, sc->irqrid, sc->irqres);
        if(sc->iores)
                bus_release_resource(sc->dev, sc->iotype, sc->rid, sc->iores);
	sc->configured = 0;
	return 0;
}

static int ips_pci_detach(device_t dev)
{
        ips_softc_t *sc;
        DEVICE_PRINTF(1, dev, "detaching ServeRaid\n");
        sc = (ips_softc_t *) device_get_softc(dev);
	if (sc->configured) {
		sc->configured = 0;
		ips_flush_cache(sc);
		if(ips_adapter_free(sc))
			return EBUSY;
		ips_pci_free(sc);
		mtx_destroy(&sc->cmd_mtx);
	}
	return 0;
}

static int ips_pci_shutdown(device_t dev)
{
	ips_softc_t *sc = (ips_softc_t *) device_get_softc(dev);
	if (sc->configured) {
		ips_flush_cache(sc);
	}
	return 0;
}

static device_method_t ips_driver_methods[] = {
        DEVMETHOD(device_probe, ips_pci_probe),
        DEVMETHOD(device_attach, ips_pci_attach),
        DEVMETHOD(device_detach, ips_pci_detach),
	DEVMETHOD(device_shutdown, ips_pci_shutdown),
        {0,0}
};

static driver_t ips_pci_driver = {
        "ips",
        ips_driver_methods,
        sizeof(ips_softc_t),
};

static devclass_t ips_devclass;
DRIVER_MODULE(ips, pci, ips_pci_driver, ips_devclass, 0, 0);
