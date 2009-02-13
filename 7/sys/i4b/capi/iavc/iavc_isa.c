/*-
 * Copyright (c) 2001, 2002 Hellmuth Michaelis. All rights reserved.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <net/if.h>


#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <isa/isavar.h>

#include <i4b/include/i4b_debug.h>
#include <i4b/include/i4b_ioctl.h>
#include <i4b/include/i4b_trace.h>

#include <i4b/include/i4b_global.h>
#include <i4b/include/i4b_l3l4.h>
#include <i4b/include/i4b_mbuf.h>

#include <i4b/capi/capi.h>

#include <i4b/capi/iavc/iavc.h>

/* ISA driver linkage */

static void iavc_isa_intr(iavc_softc_t *sc);
static int iavc_isa_probe(device_t dev);
static int iavc_isa_attach(device_t dev);

static device_method_t iavc_isa_methods[] =
{
    DEVMETHOD(device_probe,	iavc_isa_probe),
    DEVMETHOD(device_attach,	iavc_isa_attach),
    { 0, 0 }
};

static driver_t iavc_isa_driver =
{
    "iavc",
    iavc_isa_methods,
    0
};

static devclass_t iavc_isa_devclass;

DRIVER_MODULE(iavc, isa, iavc_isa_driver, iavc_isa_devclass, 0, 0);

#define B1_IOLENGTH	0x20

static int b1_irq_table[] =
{0, 0, 0, 192, 32, 160, 96, 224, 0, 64, 80, 208, 48, 0, 0, 112};
/*        3    4   5    6   7       9   10  11   12        15 */

/*---------------------------------------------------------------------------*
 *	device probe
 *---------------------------------------------------------------------------*/

static int
iavc_isa_probe(device_t dev)
{
	struct iavc_softc *sc;
	int ret = ENXIO;
	int unit = device_get_unit(dev);
	
	if(isa_get_vendorid(dev))	/* no PnP probes here */
		return ENXIO;

	/* check max unit range */
	
	if (unit >= IAVC_MAXUNIT)
	{
		printf("iavc%d: too many units\n", unit);
		return(ENXIO);	
	}

	sc = iavc_find_sc(unit);	/* get softc */	
	
	sc->sc_unit = unit;

	if (!(sc->sc_resources.io_base[0] =
		bus_alloc_resource(dev, SYS_RES_IOPORT,
			&sc->sc_resources.io_rid[0],
			0UL, ~0UL, B1_IOLENGTH, RF_ACTIVE)))
	{
		printf("iavc%d: can't allocate io region\n", unit);
		return(ENXIO);                                       
	}

	sc->sc_iobase = rman_get_start(sc->sc_resources.io_base[0]);

	switch(sc->sc_iobase)
	{
		case 0x150:
		case 0x250:
		case 0x300:
		case 0x340:
			break;
		default:
			printf("iavc%d: ERROR, invalid i/o base addr 0x%x configured!\n", sc->sc_unit, sc->sc_iobase);
			bus_release_resource(dev, SYS_RES_IOPORT,
					sc->sc_resources.io_rid[0],
		                        sc->sc_resources.io_base[0]);
		return(ENXIO);
	}	
	
	sc->sc_io_bt = rman_get_bustag(sc->sc_resources.io_base[0]);
	sc->sc_io_bh = rman_get_bushandle(sc->sc_resources.io_base[0]);

	/* setup characteristics */

	sc->sc_t1 = FALSE;
	sc->sc_dma = FALSE;

	sc->sc_capi.card_type = CARD_TYPEC_AVM_B1_ISA;
	sc->sc_capi.sc_nbch = 2;

	b1_reset(sc);
	DELAY(100);

	ret = b1_detect(sc);

	if(ret)
	{
		printf("iavc%d: no card ? b1_detect returns %0x02x\n",
			sc->sc_unit, ret);
		return(ENXIO);
	}

	DELAY(100);

	b1_reset(sc);
	
	DELAY(100);

	if(bootverbose)
	{
		printf("iavc%d: class = 0x%02x, rev = 0x%02x\n", sc->sc_unit,
			iavc_read_port(sc, B1_ANALYSE),
			iavc_read_port(sc, B1_REVISION));
	}

	device_set_desc(dev, "AVM B1 ISA");
	return(0);
}

/*---------------------------------------------------------------------------*
 *	attach
 *---------------------------------------------------------------------------*/
static int
iavc_isa_attach(device_t dev)
{
	struct iavc_softc *sc;
	void *ih = 0;
	int unit = device_get_unit(dev);
	int irq;
	
	sc = iavc_find_sc(unit);	/* get softc */	
	
	sc->sc_resources.irq_rid = 0;
	
	if(!(sc->sc_resources.irq =
		bus_alloc_resource_any(dev, SYS_RES_IRQ,
			&sc->sc_resources.irq_rid, RF_ACTIVE)))
	{
		printf("iavc%d: can't allocate irq\n",unit);
		bus_release_resource(dev, SYS_RES_IOPORT,
				sc->sc_resources.io_rid[0],
	                        sc->sc_resources.io_base[0]);
		return(ENXIO);
	}

	irq = rman_get_start(sc->sc_resources.irq);

	if(b1_irq_table[irq] == 0)
	{
		printf("iavc%d: ERROR, illegal irq %d configured!\n",unit, irq);
		bus_release_resource(dev, SYS_RES_IOPORT,
				sc->sc_resources.io_rid[0],
	                        sc->sc_resources.io_base[0]);
		bus_release_resource(dev, SYS_RES_IRQ,
				sc->sc_resources.irq_rid,
				sc->sc_resources.irq);
		return(ENXIO);
	}
	
	memset(&sc->sc_txq, 0, sizeof(struct ifqueue));
	sc->sc_txq.ifq_maxlen = sc->sc_capi.sc_nbch * 4;

        if(!mtx_initialized(&sc->sc_txq.ifq_mtx))
		mtx_init(&sc->sc_txq.ifq_mtx, "i4b_ivac_isa", NULL, MTX_DEF);

	sc->sc_intr = FALSE;
	sc->sc_state = IAVC_DOWN;
	sc->sc_blocked = FALSE;

	/* setup capi link */
	
	sc->sc_capi.load = iavc_load;
	sc->sc_capi.reg_appl = iavc_register;
	sc->sc_capi.rel_appl = iavc_release;
	sc->sc_capi.send = iavc_send;
	sc->sc_capi.ctx = (void*) sc;

	if (capi_ll_attach(&sc->sc_capi))
	{
		printf("iavc%d: capi attach failed\n", unit);
		return(ENXIO);
	}

	/* setup the interrupt */

	if(bus_setup_intr(dev, sc->sc_resources.irq, INTR_TYPE_NET,
		      NULL, (void(*)(void*))iavc_isa_intr,
		      sc, &ih))
	{
		printf("iavc%d: irq setup failed\n", unit);
		bus_release_resource(dev, SYS_RES_IOPORT,
				sc->sc_resources.io_rid[0],
	                        sc->sc_resources.io_base[0]);
		bus_release_resource(dev, SYS_RES_IRQ,
				sc->sc_resources.irq_rid,
				sc->sc_resources.irq);
		return(ENXIO);
	}

	/* the board is now ready to be loaded */

	return(0);
}

/*---------------------------------------------------------------------------*
 *	setup interrupt
 *---------------------------------------------------------------------------*/
void
b1isa_setup_irq(struct iavc_softc *sc)
{
	int irq = rman_get_start(sc->sc_resources.irq);
	
	if(bootverbose)
		printf("iavc%d: using irq %d\n", sc->sc_unit, irq);

	/* enable the interrupt */

	b1io_outp(sc, B1_INSTAT, 0x00);
	b1io_outp(sc, B1_RESET, b1_irq_table[irq]);
	b1io_outp(sc, B1_INSTAT, 0x02);
}	

/*---------------------------------------------------------------------------*
 *	IRQ handler
 *---------------------------------------------------------------------------*/
static void
iavc_isa_intr(struct iavc_softc *sc)
{
	iavc_handle_intr(sc);
}
