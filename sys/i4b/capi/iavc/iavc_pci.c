/*
 * Copyright (c) 2001 Cubical Solutions Ltd. All rights reserved.
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
 * capi/iavc/iavc_pci.c
 *		The AVM ISDN controllers' PCI bus attachment handling.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <net/if.h>

#include <machine/clock.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <vm/vm.h>
#include <vm/pmap.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>

#include <machine/i4b_debug.h>
#include <machine/i4b_ioctl.h>
#include <machine/i4b_trace.h>

#include <i4b/include/i4b_global.h>
#include <i4b/include/i4b_l3l4.h>
#include <i4b/include/i4b_mbuf.h>

#include <i4b/capi/capi.h>

#include <i4b/capi/iavc/iavc.h>

/* PCI device ids */

#define PCI_AVM_VID   0x1244
#define PCI_AVMT1_DID 0x1200
#define PCI_AVMB1_DID 0x0700

/* PCI driver linkage */

static void iavc_pci_intr(iavc_softc_t *sc);
static int iavc_pci_probe(device_t dev);
static int iavc_pci_attach(device_t dev);

static device_method_t iavc_pci_methods[] =
{
    DEVMETHOD(device_probe,	iavc_pci_probe),
    DEVMETHOD(device_attach,	iavc_pci_attach),
    { 0, 0 }
};

static driver_t iavc_pci_driver =
{
    "iavc",
    iavc_pci_methods,
    0
};

static devclass_t iavc_pci_devclass;

DRIVER_MODULE(iavc, pci, iavc_pci_driver, iavc_pci_devclass, 0, 0);

/* Driver soft contexts */

iavc_softc_t iavc_sc[IAVC_MAXUNIT];

/*---------------------------------------------------------------------------*
 *
 *---------------------------------------------------------------------------*/

static int
iavc_pci_probe(device_t dev)
{
    u_int16_t did = pci_get_device(dev);
    u_int16_t vid = pci_get_vendor(dev);

    if ((vid == PCI_AVM_VID) && (did == PCI_AVMT1_DID)) {
	device_set_desc(dev, "AVM T1 PCI");
    } else if ((vid == PCI_AVM_VID) && (did == PCI_AVMB1_DID)) {
	device_set_desc(dev, "AVM B1 PCI");
    } else {
	return(ENXIO);
    }

    return(0);
}

/*---------------------------------------------------------------------------*
 *
 *---------------------------------------------------------------------------*/

static int
iavc_pci_attach(device_t dev)
{
    struct iavc_softc *sc;
    void *ih = 0;
    u_int16_t did = pci_get_device(dev);
    int unit = device_get_unit(dev), ret;
	
    /* check max unit range */
	
    if (unit >= IAVC_MAXUNIT) {
	printf("iavc%d: too many units\n", unit);
	return(ENXIO);	
    }	

    sc = iavc_find_sc(unit);	/* get softc */	
	
    sc->sc_unit = unit;

    /* use the i/o mapped base address */
	
    sc->sc_resources.io_rid[0] = 0x14;
	
    if (!(sc->sc_resources.io_base[0] =
	 bus_alloc_resource(dev, SYS_RES_IOPORT,
			    &sc->sc_resources.io_rid[0],
			    0UL, ~0UL, 1, RF_ACTIVE))) {
	printf("iavc%d: can't allocate io region\n", unit);
	return(ENXIO);                                       
    }

    sc->sc_iobase = rman_get_start(sc->sc_resources.io_base[0]);
    sc->sc_io_bt = rman_get_bustag(sc->sc_resources.io_base[0]);
    sc->sc_io_bh = rman_get_bushandle(sc->sc_resources.io_base[0]);

    /* use the memory mapped DMA controller */
	
    sc->sc_resources.mem_rid = 0x10;
	
    if (!(sc->sc_resources.mem =
	 bus_alloc_resource(dev, SYS_RES_MEMORY,
			    &sc->sc_resources.mem_rid,
			    0UL, ~0UL, 1, RF_ACTIVE))) {
	printf("iavc%d: can't allocate memory region\n", unit);
	return(ENXIO);                                       
    }

    sc->sc_membase = rman_get_start(sc->sc_resources.mem);
    sc->sc_mem_bt = rman_get_bustag(sc->sc_resources.mem);
    sc->sc_mem_bh = rman_get_bushandle(sc->sc_resources.mem);

    /* do some detection */

    sc->sc_t1 = FALSE;
    sc->sc_dma = FALSE;
    b1dma_reset(sc);

    if (did == PCI_AVMT1_DID) {
	sc->sc_capi.card_type = CARD_TYPEC_AVM_T1_PCI;
	sc->sc_capi.sc_nbch = 30;
	ret = t1_detect(sc);
	if (ret) {
	    if (ret < 6) {
		printf("iavc%d: no card detected?\n", sc->sc_unit);
	    } else {
		printf("iavc%d: black box not on\n", sc->sc_unit);
	    }
	    return(ENXIO);
	} else {
	    sc->sc_dma = TRUE;
	    sc->sc_t1 = TRUE;
	}

    } else if (did == PCI_AVMB1_DID) {
	sc->sc_capi.card_type = CARD_TYPEC_AVM_B1_PCI;
	sc->sc_capi.sc_nbch = 2;
	ret = b1dma_detect(sc);
	if (ret) {
	    ret = b1_detect(sc);
	    if (ret) {
		printf("iavc%d: no card detected?\n", sc->sc_unit);
		return(ENXIO);
	    }
	} else {
	    sc->sc_dma = TRUE;
	}
    }

    if (sc->sc_dma) b1dma_reset(sc);
#if 0
    if (sc->sc_t1) t1_reset(sc);
    else b1_reset(sc);
#endif

    /* of course we need an interrupt */
    
    sc->sc_resources.irq_rid = 0x00;
	
    if(!(sc->sc_resources.irq =
	 bus_alloc_resource(dev, SYS_RES_IRQ,
			    &sc->sc_resources.irq_rid,
			    0UL, ~0UL, 1, RF_SHAREABLE|RF_ACTIVE))) {
	printf("iavc%d: can't allocate irq\n",unit);
	return(ENXIO);
    }

    /* finalize our own context */

    memset(&sc->sc_txq, 0, sizeof(struct ifqueue));
    sc->sc_txq.ifq_maxlen = sc->sc_capi.sc_nbch * 4;

    if(!mtx_initialized(&sc->sc_txq.ifq_mtx))
	    mtx_init(&sc->sc_txq.ifq_mtx, "i4b_ivac_pci", NULL, MTX_DEF);
    
    sc->sc_intr = FALSE;
    sc->sc_state = IAVC_DOWN;
    sc->sc_blocked = FALSE;

    /* setup capi link */
	
    sc->sc_capi.load = iavc_load;
    sc->sc_capi.reg_appl = iavc_register;
    sc->sc_capi.rel_appl = iavc_release;
    sc->sc_capi.send = iavc_send;
    sc->sc_capi.ctx = (void*) sc;

    if (capi_ll_attach(&sc->sc_capi)) {
	printf("iavc%d: capi attach failed\n", unit);
	return(ENXIO);
    }

    /* setup the interrupt */

    if(bus_setup_intr(dev, sc->sc_resources.irq, INTR_TYPE_NET,
		      (void(*)(void*))iavc_pci_intr,
		      sc, &ih)) {
	printf("iavc%d: irq setup failed\n", unit);
	return(ENXIO);
    }

    /* the board is now ready to be loaded */

    return(0);
}

/*---------------------------------------------------------------------------*
 *	IRQ handler
 *---------------------------------------------------------------------------*/

static void
iavc_pci_intr(struct iavc_softc *sc)
{
    iavc_handle_intr(sc);
}
