/*
 * Copyright (c) 2000 Hans Petter Selasky. All rights reserved.
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
 *---------------------------------------------------------------------------
 *
 *	i4b_ihfc_pnp.c - common hfc ISA PnP-bus interface
 *	-------------------------------------------------
 *
 *	- Everything which has got anything to to with "PnP" bus setup has
 *	  been put here, except the chip spesific "PnP" setup.
 *
 *
 *      last edit-date: [Tue Jan 23 16:03:33 2001]
 *
 *      $Id: i4b_ihfc_pnp.c,v 1.9 2000/09/19 13:50:36 hm Exp $
 *
 * $FreeBSD$
 *     
 *---------------------------------------------------------------------------*/

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <net/if.h>


#include <i4b/include/i4b_global.h>

#include <machine/i4b_ioctl.h>
#include <machine/i4b_trace.h>

#include <i4b/layer1/i4b_l1.h>
#include <i4b/layer1/ihfc/i4b_ihfc.h>
#include <i4b/layer1/ihfc/i4b_ihfc_ext.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <isa/isavar.h>

/*---------------------------------------------------------------------------*
 *	Softc
 *---------------------------------------------------------------------------*/
ihfc_sc_t		ihfc_softc[IHFC_MAXUNIT];

/*---------------------------------------------------------------------------*
 *	Prototypes
 *---------------------------------------------------------------------------*/
static int ihfc_isa_probe	(device_t dev);
static int ihfc_pnp_probe	(device_t dev);
static int ihfc_pnp_attach	(device_t dev);
static int ihfc_pnp_detach 	(device_t dev, u_int flag);
static int ihfc_pnp_shutdown	(device_t dev);

const struct ihfc_pnp_ids
{
	u_long	vid;		/* vendor id			*/
	int	flag;		/*				*/
	u_char	hfc;		/* chip type 			*/
	u_char	iirq;		/* internal irq			*/
	u_short	iio;		/* internal io-address		*/
	u_char  stdel;		/* S/T delay compensation	*/
}
	ihfc_pnp_ids[] =
{
	{ 0x10262750, CARD_TYPEP_16_3C,   HFC_S,  2, 0x200, 0x2d},
	{ 0x20262750, CARD_TYPEP_16_3C,   HFC_SP, 0, 0x000, 0x0f},
	{ 0x1411d805, CARD_TYPEP_ACERP10, HFC_S,  1, 0x300, 0x0e},
	{ 0 }
};

typedef const struct ihfc_pnp_ids ihfc_id_t;

/*---------------------------------------------------------------------------*
 *	PCB layout
 *
 *	IIRQx: Internal IRQ cross reference for a card
 *	IRQx : Supported IRQ's for a card
 *	IOx  : Supported IO-bases for a card
 *
 *	IO0, IRQ0, IIRQ0: TELEINT ISDN SPEED No. 1
 *		   IIRQ3: Teles 16.3c PnP (B version)
 *---------------------------------------------------------------------------*/
		      /* IRQ  ->  0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f */
#define	IIRQ0 ((const u_char []){ 0, 0, 0, 1, 2, 3, 0, 4, 0, 0, 5, 6, 0, 0, 0, 0 })
#define IRQ0  ((const u_char []){          3, 4, 5,    7,     0xa, 0xb, 0 })

#define IO0   ((const u_long []){ 0x300, 0x330, 0x278, 0x2e8, 0 })

#define IIRQ3 ((const u_char []){ 0, 0, 0, 7, 0, 1, 0, 0, 0, 2, 3, 4, 5, 0, 0, 6 })

/*---------------------------------------------------------------------------*
 *	ISA PnP setup
 *---------------------------------------------------------------------------*/
static device_method_t ihfc_pnp_methods[] = 
{
	DEVMETHOD(device_probe,		ihfc_pnp_probe),
	DEVMETHOD(device_attach,	ihfc_pnp_attach),
	DEVMETHOD(device_shutdown,	ihfc_pnp_shutdown),
	{ 0, 0 }
};                              

static driver_t ihfc_pnp_driver = 
{
	"ihfc",
	ihfc_pnp_methods,
	0,
};

static devclass_t ihfc_devclass;

DRIVER_MODULE(ihfcpnp, isa, ihfc_pnp_driver, ihfc_devclass, 0, 0);

/*---------------------------------------------------------------------------*
 *      probe for ISA "PnP" card
 *---------------------------------------------------------------------------*/
static int
ihfc_pnp_probe(device_t dev)
{
	u_int 	       unit = device_get_unit(dev);	/* get unit	  */
	u_int32_t	vid = isa_get_vendorid(dev); 	/* vendor id	  */
	ihfc_id_t      *ids = &ihfc_pnp_ids[0];		/* ids ptr	  */
	ihfc_sc_t 	*sc = &ihfc_softc[unit];	/* softc	  */
	u_char	       flag = 0;			/* flag		  */
	void         *dummy = 0;			/* a dummy	  */

	HFC_VAR;

	if (unit >= IHFC_MAXUNIT)
	{
		printf("ihfc%d: Error, unit %d >= IHFC_MAXUNIT", unit, unit);
		return ENXIO;
	}

	if (!vid) return ihfc_isa_probe(dev);

	HFC_BEG;

	for ( ;(ids->vid); ids++)
	{
		if (ids->vid == vid)
		{
			flag = 0;

			bzero(sc, sizeof(ihfc_sc_t));		/* reset data structure.*
								 * Zero is default for  *
								 * most, so calling the *
								 * int. handler now will*
								 * not be a problem.    */

			S_IOBASE[0] = bus_alloc_resource(
				dev, SYS_RES_IOPORT, &S_IORID[0],
				0UL, ~0UL, 2, RF_ACTIVE
				);

			S_IRQ = bus_alloc_resource(
				dev, SYS_RES_IRQ, &S_IRQRID,
				0UL, ~0UL, 1, RF_ACTIVE
				);

			S_DLP     = IHFC_DLP;		/* set D-priority	*/
			S_HFC	  = ids->hfc;		/* set chip type	*/
			S_I4BFLAG = ids->flag;		/* set flag		*/
			S_NTMODE  = IHFC_NTMODE;	/* set mode		*/
			S_STDEL   = ids->stdel;		/* set delay		*/

			S_I4BUNIT = L0IHFCUNIT(unit);	/* set "i4b" unit	*/
			S_TRACE   = TRACE_OFF;		/* set trace mask	*/
			S_UNIT 	  = unit;		/* set up unit numbers	*/

			if (S_IOBASE[0] && S_IRQ)
			{
				if (ids->iio)
				{
					S_IIO  = ids->iio;
					S_IIRQ = ids->iirq;
				}
				else
				{
					S_IIO  = rman_get_start(S_IOBASE[0]) & 0x3ff;
					S_IIRQ = IIRQ3[rman_get_start(S_IRQ) & 0xf];
				}

				/* setup interrupt routine now to avvoid stray	*
				 * interrupts.					*/

				bus_setup_intr(dev, S_IRQ, INTR_TYPE_NET, (void(*)(void*))
					HFC_INTR, sc, &dummy);

				flag = 1;

				if (!HFC_CONTROL(sc, 1))
				{
					HFC_END;
					return 0;	/* success */
				}
				else
				{
					printf("ihfc%d: Chip seems corrupted. "
					"Please hard reboot your computer!\n",
					unit);					
				}
			}

			ihfc_pnp_detach(dev, flag);
		}
	}

	HFC_END;
	return ENXIO;	/* failure */
}

/*---------------------------------------------------------------------------*
 *      probe for "ISA" cards
 *---------------------------------------------------------------------------*/
static int
ihfc_isa_probe(device_t dev)
{
	u_int 	        unit = device_get_unit(dev);	/* get unit	  */
	ihfc_sc_t        *sc = &ihfc_softc[unit];	/* softc	  */
	const u_char    *irq = &IRQ0[0]; 		/* irq's to try   */
	const u_long *iobase = &IO0[0];			/* iobases to try */
	u_char	        flag = 0;			/* flag		  */
	void          *dummy = 0;			/* a dummy	  */

	HFC_VAR;

	bzero(sc, sizeof(ihfc_sc_t));		/* reset data structure	 *
						 * We must reset the     *
						 * datastructure here,	 *
						 * else we risk zero-out *
						 * our gotten resources. */
	HFC_BEG;

  j0:	while(*irq) 	/* get supported IRQ */
	{
		if ((S_IRQ = bus_alloc_resource(
			dev, SYS_RES_IRQ, &S_IRQRID,
			*irq, *irq, 1, RF_ACTIVE
			)
		   ))
				break;
		else
				irq++;
	}

	while(*iobase)	/* get supported IO-PORT */
	{
		if ((S_IOBASE[0] = bus_alloc_resource(
		   	dev, SYS_RES_IOPORT, &S_IORID[0],
			*iobase, *iobase, 2, RF_ACTIVE
			)
		   ))
			 	break;
		else
				iobase++;
	}

	flag = 0;

	if (*irq && *iobase)	/* we got our resources, now test chip */
	{
		S_DLP     = IHFC_DLP;		/* set D-priority	*/
		S_HFC	  = HFC_1;		/* set chip type	*/
		S_I4BFLAG = CARD_TYPEP_TELEINT_NO_1; /* set flag	*/
		S_NTMODE  = IHFC_NTMODE;	/* set mode		*/
		S_STDEL   = 0x00;		/* set delay (not used)	*/

		S_I4BUNIT = L0IHFCUNIT(unit);	/* set "i4b" unit	*/
		S_TRACE   = TRACE_OFF;		/* set trace mask	*/
		S_UNIT 	  = unit;		/* set up unit numbers	*/

		S_IIRQ	  = IIRQ0[*irq];	/* set internal irq	*/
		S_IIO	  = *iobase;		/* set internal iobase	*/

		/* setup interrupt routine now to avvoid stray	*
		 * interrupts.					*/

		bus_setup_intr(dev, S_IRQ, INTR_TYPE_NET, (void(*)(void*))
			HFC_INTR, sc, &dummy);

		flag = 1;

		if (!HFC_CONTROL(sc, 1))
		{
			device_set_desc(dev, "TELEINT ISDN SPEED No. 1");

			HFC_END;
			return 0;	/* success */
		}
	}

	ihfc_pnp_detach(dev, flag);

	if (*irq && *++iobase) goto j0;	/* try again */

	HFC_END;

	printf("ihfc%d: Chip not found. "
	"A hard reboot may help!\n", unit);

	return ENXIO;	/* failure */
}

/*---------------------------------------------------------------------------*
 *      attach ISA "PnP" card
 *---------------------------------------------------------------------------*/
static int
ihfc_pnp_attach(device_t dev)
{
	u_int	   unit = device_get_unit(dev);		/* get unit	*/
	ihfc_sc_t   *sc	= &ihfc_softc[unit];		/* softc	*/
	HFC_VAR;
 
	HFC_BEG;

	ihfc_B_linkinit(sc);	/* Setup B-Channel linktabs */

	i4b_l1_mph_status_ind(S_I4BUNIT, STI_ATTACH, S_I4BFLAG, &ihfc_l1mux_func);
	
	HFC_INIT(sc, 0, 0, 1);	/* Setup D - Channel */

	HFC_INIT(sc, 2, 0, 0);	/* Init B1 - Channel */
	HFC_INIT(sc, 4, 0, 0); 	/* Init B2 - Channel */

	HFC_END;
	return 0;	/* success */

	HFC_END;
	return ENXIO;	/* failure */
}

/*---------------------------------------------------------------------------*
 *      shutdown for our ISA PnP card
 *---------------------------------------------------------------------------*/
static int
ihfc_pnp_shutdown(device_t dev)
{
	u_int	   unit = device_get_unit(dev);		/* get unit	*/
	ihfc_sc_t   *sc	= &ihfc_softc[unit];		/* softc	*/
	HFC_VAR;

	HFC_BEG;

	if (unit >= IHFC_MAXUNIT)
	{
		printf("ihfc%d: Error, unit %d >= IHFC_MAXUNIT", unit, unit);
		goto f0;
	}

	HFC_CONTROL(sc, 2);	/* shutdown chip */

	HFC_END;
	return 0;
  f0:
	HFC_END;
	return ENXIO;

}

/*---------------------------------------------------------------------------*
 *      detach for our ISA PnP card
 *
 *	flag:	bit[0] set: teardown interrupt handler too
 *---------------------------------------------------------------------------*/
static int
ihfc_pnp_detach (device_t dev, u_int flag)
{
	u_int	   unit = device_get_unit(dev);		/* get unit	*/
	ihfc_sc_t   *sc	= &ihfc_softc[unit];		/* softc	*/
	u_char 	      i;

	if (unit >= IHFC_MAXUNIT)
	{
		printf("ihfc%d: Error, unit %d >= IHFC_MAXUNIT", unit, unit);
		return 0;
	}

	/* free interrupt resources */

	if(S_IRQ)
	{
		if (flag & 1)
		{
			/* tear down interrupt handler */
			bus_teardown_intr(dev, S_IRQ, (void(*)(void *))HFC_INTR);
		}

		/* free irq */
		bus_release_resource(dev, SYS_RES_IRQ, S_IRQRID, S_IRQ);

		S_IRQRID = 0;
		S_IRQ = 0;
	}


	/* free iobases */

	for (i = IHFC_IO_BASES; i--;)
	{
		if(S_IOBASE[i])
		{
			bus_release_resource(dev, SYS_RES_IOPORT,
					S_IORID[i], S_IOBASE[i]);
			S_IORID[i] = 0;
			S_IOBASE[i] = 0;			
		}
	}

	return 0;
}
