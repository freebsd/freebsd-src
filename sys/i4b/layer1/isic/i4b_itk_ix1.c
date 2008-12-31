/*-
 * Copyright (c) 1998, 1999 Martin Husemann <martin@rumolt.teuto.de>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 */

/*---------------------------------------------------------------------------
 *
 *	i4b_itk_ix1.c - ITK ix1 micro passive card driver for isdn4bsd
 *	--------------------------------------------------------------
 *      last edit-date: [Wed Jan 24 09:27:06 2001]
 *
 *---------------------------------------------------------------------------
 *
 * The ITK ix1 micro ISDN card is an ISA card with one region
 * of four io ports mapped and a fixed irq all jumpered on the card.
 * Access to the board is straight forward and simmilar to
 * the ELSA and DYNALINK cards. If a PCI version of this card
 * exists all we need is probably a pci-bus attachment, all
 * this low level routines should work imediately.
 *
 * To reset the card:
 *   - write 0x01 to ITK_CONFIG
 *   - wait >= 10 ms
 *   - write 0x00 to ITK_CONFIG
 *
 * To read or write data:
 *  - write address to ITK_ALE port
 *  - read data from or write data to ITK_ISAC_DATA port or ITK_HSCX_DATA port
 * The two HSCX channel registers are offset by HSCXA (0x00) and HSCXB (0x40).
 *
 * The probe routine was derived by trial and error from a representative
 * sample of two cards ;-) The standard way (checking HSCX versions)
 * was extended by reading a zero from a non existant HSCX register (register
 * 0xff). Reading the config register gives varying results, so this doesn't
 * seem to be used as an id register (like the Teles S0/16.3).
 *
 * If the probe fails for your card use "options ITK_PROBE_DEBUG" to get
 * additional debug output.
 *
 *---------------------------------------------------------------------------*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/i4b/layer1/isic/i4b_itk_ix1.c,v 1.13.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $");

#include "opt_i4b.h"

#if defined(ITKIX1)

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <net/if.h>

#include <i4b/include/i4b_ioctl.h>
#include <i4b/include/i4b_trace.h>
#include <i4b/layer1/i4b_l1.h>
#include <i4b/layer1/isic/i4b_isic.h>
#include <i4b/layer1/isic/i4b_hscx.h>

/* Register offsets */
#define	ITK_ISAC_DATA	0
#define	ITK_HSCX_DATA	1
#define	ITK_ALE		2
#define	ITK_CONFIG	3

/* Size of IO range to allocate for this card */
#define	ITK_IO_SIZE	4

/* Register offsets for the two HSCX channels */
#define	HSCXA	0
#define	HSCXB	0x40

static void
itkix1_read_fifo(struct l1_softc *sc, int what, void *buf, size_t size)
{
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.io_base[0]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[0]);
	switch (what)
	{
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t, h, ITK_ALE, 0);
			bus_space_read_multi_1(t, h, ITK_ISAC_DATA, buf, size);
			break;
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t, h, ITK_ALE, HSCXA);
			bus_space_read_multi_1(t, h, ITK_HSCX_DATA, buf, size);
			break;
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t, h, ITK_ALE, HSCXB);
			bus_space_read_multi_1(t, h, ITK_HSCX_DATA, buf, size);
			break;
	}
}

static void
itkix1_write_fifo(struct l1_softc *sc, int what, void *buf, size_t size)
{
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.io_base[0]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[0]);
	switch (what)
	{
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t, h, ITK_ALE, 0);
			bus_space_write_multi_1(t, h, ITK_ISAC_DATA, (u_int8_t*)buf, size);
			break;
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t, h, ITK_ALE, HSCXA);
			bus_space_write_multi_1(t, h, ITK_HSCX_DATA, (u_int8_t*)buf, size);
			break;
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t, h, ITK_ALE, HSCXB);
			bus_space_write_multi_1(t, h, ITK_HSCX_DATA, (u_int8_t*)buf, size);
			break;
	}
}

static void
itkix1_write_reg(struct l1_softc *sc, int what, bus_size_t offs, u_int8_t data)
{
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.io_base[0]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[0]);
	switch (what)
	{
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t, h, ITK_ALE, offs);
			bus_space_write_1(t, h, ITK_ISAC_DATA, data);
			break;
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t, h, ITK_ALE, HSCXA+offs);
			bus_space_write_1(t, h, ITK_HSCX_DATA, data);
			break;
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t, h, ITK_ALE, HSCXB+offs);
			bus_space_write_1(t, h, ITK_HSCX_DATA, data);
			break;
	}
}

static u_int8_t
itkix1_read_reg(struct l1_softc *sc, int what, bus_size_t offs)
{
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.io_base[0]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[0]);
	switch (what)
	{
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t, h, ITK_ALE, offs);
			return bus_space_read_1(t, h, ITK_ISAC_DATA);
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t, h, ITK_ALE, HSCXA+offs);
			return bus_space_read_1(t, h, ITK_HSCX_DATA);
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t, h, ITK_ALE, HSCXB+offs);
			return bus_space_read_1(t, h, ITK_HSCX_DATA);
	}
	return 0;
}

/*
 * Probe for card
 */
int
isic_probe_itkix1(device_t dev)
{	
	size_t unit = device_get_unit(dev);	/* get unit */
	struct l1_softc *sc = 0;		/* softc */
	void *ih = 0;				/* dummy */
	bus_space_tag_t    t;			/* bus things */
	bus_space_handle_t h;
	u_int8_t hd, hv1, hv2, saveale;
	int ret;

	#if defined(ITK_PROBE_DEBUG)
	printf("Checking unit %u\n", unit);
	#endif

	/* check max unit range */
	if(unit >= ISIC_MAXUNIT)
	{
		printf("isic%d: Error, unit %d >= ISIC_MAXUNIT for ITK IX1!\n",
				unit, unit);
		return ENXIO;	
	}

	sc = &l1_sc[unit];			/* get pointer to softc */
	sc->sc_unit = unit;			/* set unit */
	
	#if defined(ITK_PROBE_DEBUG)
	printf("Allocating io base...");
	#endif
	
	if(!(sc->sc_resources.io_base[0] =
		bus_alloc_resource_any(dev, SYS_RES_IOPORT,
	        		&sc->sc_resources.io_rid[0],
	                        RF_ACTIVE)))
	{
		printf("isic%d: Could not allocate i/o port for ITK IX1.\n", unit);
		return ENXIO;
	}

	#if defined(ITK_PROBE_DEBUG)
	printf("done.\n");
	#endif

	sc->sc_port = rman_get_start(sc->sc_resources.io_base[0]);
	t = rman_get_bustag(sc->sc_resources.io_base[0]);
	h = rman_get_bushandle(sc->sc_resources.io_base[0]);

	#if defined(ITK_PROBE_DEBUG)
	printf("Allocating irq...");
	#endif

	/* get our irq */
	if(!(sc->sc_resources.irq =
			bus_alloc_resource_any(dev, SYS_RES_IRQ,
						&sc->sc_resources.irq_rid,
						RF_ACTIVE)))
	{
		printf("isic%d: Could not allocate irq for ITK IX1.\n", unit);
		bus_release_resource(dev,SYS_RES_IOPORT,
				sc->sc_resources.io_rid[0],
				sc->sc_resources.io_base[0]);
		return ENXIO;
	}

	#if defined(ITK_PROBE_DEBUG)
	printf("done.\n");
	#endif

	/* get the irq number */
	sc->sc_irq = rman_get_start(sc->sc_resources.irq);

	#if defined(ITK_PROBE_DEBUG)
	printf("Setting up access routines...");
	#endif
	
	/* setup access routines */
	sc->clearirq = NULL;
	sc->readreg = itkix1_read_reg;
	sc->writereg = itkix1_write_reg;
	sc->readfifo = itkix1_read_fifo;
	sc->writefifo = itkix1_write_fifo;

	/* setup card type */	
	sc->sc_cardtyp = CARD_TYPEP_ITKIX1;

	/* setup IOM bus type */
	sc->sc_bustyp = BUS_TYPE_IOM2;

	sc->sc_ipac = 0;
	sc->sc_bfifolen = HSCX_FIFO_LEN;

	#if defined(ITK_PROBE_DEBUG)
	printf("done.\n");
	#endif

	/* register interrupt routine */

	#if defined(ITK_PROBE_DEBUG)
	printf("Setting up access interrupt...");
	#endif

	if (bus_setup_intr(dev, sc->sc_resources.irq, INTR_TYPE_NET, NULL,
			(void(*)(void *))(isicintr), sc, &ih) != 0)
	{
		printf("isic%d: Could not setup irq for ITK IX1.\n", unit);
		bus_release_resource(dev,SYS_RES_IOPORT,
				sc->sc_resources.io_rid[0],
				sc->sc_resources.io_base[0]);
		return ENXIO;
	}

	#if defined(ITK_PROBE_DEBUG)
	printf("done.\n");

	printf("Doing probe stuff...");
	#endif
	
	/* save old value of this port, we're stomping over it */
	saveale = bus_space_read_1(t, h, ITK_ALE);

	/* select invalid register */
	bus_space_write_1(t, h, ITK_ALE, 0xff);
	/* get HSCX data for this non existent register */
	hd = bus_space_read_1(t, h, ITK_HSCX_DATA);
	/* get HSCX version info */
	bus_space_write_1(t, h, ITK_ALE, HSCXA + H_VSTR);
	hv1 = bus_space_read_1(t, h, ITK_HSCX_DATA);
	bus_space_write_1(t, h, ITK_ALE, HSCXB + H_VSTR);
	hv2 = bus_space_read_1(t, h, ITK_HSCX_DATA);

	ret =  (hd == 0) && ((hv1 & 0x0f) == 0x05) && ((hv2 & 0x0f) == 0x05);
	/* succeed if version bits are OK and we got a zero from the
	 * non existent register. we found verison 0x05 and 0x04
	 * out there... */
	ret =  (hd == 0)
		&& (((hv1 & 0x0f) == 0x05) || ((hv1 & 0x0f) == 0x04))
		&& (((hv2 & 0x0f) == 0x05) || ((hv2 & 0x0f) == 0x04));

	/* retstore save value if we fail (if we succeed the old value
	 * has no meaning) */
	if (!ret)
		bus_space_write_1(t, h, ITK_ALE, saveale);

	#if defined(ITK_PROBE_DEBUG)
	printf("done.\n");

	printf("Doing second probe stuff...");
	#endif

	hv1 = HSCX_READ(0, H_VSTR) & 0xf;
	hv2 = HSCX_READ(1, H_VSTR) & 0xf;
	/* Read HSCX A/B VSTR.  Expected value is 0x05 (V2.1) or 0x04 (V2.0). */
	if((hv1 != 0x05 && hv1 != 0x04) || (hv2 != 0x05 && hv2 != 0x04))
	{
		printf("isic%d: HSCX VSTR test failed for ITK ix1 micro\n",
			unit);
		printf("isic%d: HSC0: VSTR: %#x\n",
			unit, HSCX_READ(0, H_VSTR));
		printf("isic%d: HSC1: VSTR: %#x\n",
			unit, HSCX_READ(1, H_VSTR));
		isic_detach_common(dev);
		return ENXIO;
	}  

	#if defined(ITK_PROBE_DEBUG)
	printf("done.\n");
	#endif

#if defined(ITK_PROBE_DEBUG)
	printf("\nITK ix1 micro probe: hscx = 0x%02x, v1 = 0x%02x, v2 = 0x%02x, would have %s\n",
		hd, hv1, hv2, ret ? "succeeded" : "failed");
	isic_detach_common(dev);
	return ENXIO;
#else
	if ( ret )
	{
		return 0;
	}
	else
	{
		isic_detach_common(dev);
		return ENXIO;
	}
#endif
}

/*
 *  Attach card
 */
int
isic_attach_itkix1(device_t dev)
{
	size_t unit = device_get_unit(dev);	/* get unit */
	struct l1_softc *sc = &l1_sc[unit];
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.io_base[0]); 
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[0]);

	/* setup access routines */
	sc->clearirq = NULL;
	sc->readreg = itkix1_read_reg;
	sc->writereg = itkix1_write_reg;
	sc->readfifo = itkix1_read_fifo;
	sc->writefifo = itkix1_write_fifo;

	/* setup card type */	
	sc->sc_cardtyp = CARD_TYPEP_ITKIX1;

	/* setup IOM bus type */
	sc->sc_bustyp = BUS_TYPE_IOM2;

	sc->sc_ipac = 0;
	sc->sc_bfifolen = HSCX_FIFO_LEN;

	bus_space_write_1(t, h, ITK_CONFIG, 1);
	DELAY(SEC_DELAY / 10);
	bus_space_write_1(t, h, ITK_CONFIG, 0);
	DELAY(SEC_DELAY / 10);

	return 0;
}

#endif /* defined(ITKIX1) */
