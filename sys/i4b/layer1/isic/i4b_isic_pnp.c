/*
 *   Copyright (c) 1998 Eivind Eklund. All rights reserved.
 *
 *   Copyright (c) 1998, 1999 German Tischler. All rights reserved.
 *
 *   Copyright (c) 1998, 2001 Hellmuth Michaelis. All rights reserved. 
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *   3. Neither the name of the author nor the names of any co-contributors
 *      may be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *   4. Altered versions must be plainly marked as such, and must not be
 *      misrepresented as being the original software and/or documentation.
 *   
 *   THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 *   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *   ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 *   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 *   OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 *   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 *   SUCH DAMAGE.
 *
 *---------------------------------------------------------------------------
 *
 *	i4b_isic_pnp.c - i4b pnp support
 *	--------------------------------
 *
 * $FreeBSD$
 *
 *      last edit-date: [Fri Jan 26 14:01:04 2001]
 *
 *---------------------------------------------------------------------------*/

#include "opt_i4b.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <net/if.h>

#include <machine/i4b_ioctl.h>
#include <i4b/layer1/isic/i4b_isic.h>

#include <isa/isavar.h>

#define VID_TEL163PNP		0x10212750	/* Teles 16.3 PnP	*/
#define VID_CREATIXPP		0x0000980e	/* Creatix S0/16 P+P	*/
#define VID_DYNALINK		0x88167506	/* Dynalink		*/
#define VID_SEDLBAUER		0x0100274c	/* Sedlbauer WinSpeed	*/
#define VID_NICCYGO		0x5001814c	/* Neuhaus Niccy GO@	*/
#define VID_ELSAQS1P		0x33019315	/* ELSA Quickstep1000pro*/
#define VID_ITK0025 		0x25008b26	/* ITK Ix1 Micro V3	*/
#define VID_AVMPNP		0x0009cd06	/* AVM Fritz! PnP	*/
#define VID_SIESURF2		0x2000254d	/* Siemens I-Surf 2.0 PnP*/
#define VID_ASUSCOM_IPAC	0x90167506	/* Asuscom (with IPAC)	*/	
#define VID_EICON_DIVA_20	0x7100891c	/* Eicon DIVA 2.0 ISAC/HSCX */
#define VID_EICON_DIVA_202	0xa100891c      /* Eicon DIVA 2.02 IPAC	*/
#define VID_COMPAQ_M610		0x0210110e	/* Compaq Microcom 610	*/

static struct isic_pnp_ids {
	u_long vend_id;
	char *id_str;
} isic_pnp_ids[] = {
#if defined(TEL_S0_16_3_P) || defined(CRTX_S0_P) || defined(COMPAQ_M610)
	{ VID_TEL163PNP,	"Teles S0/16.3 PnP"		},
	{ VID_CREATIXPP,	"Creatix S0/16 PnP"		},
	{ VID_COMPAQ_M610,	"Compaq Microcom 610"		},
#endif
#ifdef DYNALINK
	{ VID_DYNALINK,		"Dynalink IS64PH"		},
#endif
#ifdef SEDLBAUER
	{ VID_SEDLBAUER,	"Sedlbauer WinSpeed"		},
#endif
#ifdef DRN_NGO
	{ VID_NICCYGO,		"Dr.Neuhaus Niccy Go@"		},
#endif
#ifdef ELSA_QS1ISA
	{ VID_ELSAQS1P,		"ELSA QuickStep 1000pro"	},	
#endif
#ifdef ITKIX1
	{ VID_ITK0025,		"ITK ix1 Micro V3.0"    	},
#endif
#ifdef AVM_PNP
	{ VID_AVMPNP,		"AVM Fritz!Card PnP"		},	
#endif
#ifdef SIEMENS_ISURF2
	{ VID_SIESURF2,		"Siemens I-Surf 2.0 PnP"	},
#endif
#ifdef ASUSCOM_IPAC
 	{ VID_ASUSCOM_IPAC,	"Asuscom ISDNLink 128 PnP"	},
#endif
#ifdef EICON_DIVA
	{ VID_EICON_DIVA_20,	"Eicon.Diehl DIVA 2.0 ISA PnP"	},
	{ VID_EICON_DIVA_202,	"Eicon.Diehl DIVA 2.02 ISA PnP"	},
#endif
	{ 0, 0 }
};

static int isic_pnp_probe(device_t dev);
static int isic_pnp_attach(device_t dev);

static device_method_t isic_pnp_methods[] = {
	DEVMETHOD(device_probe,		isic_pnp_probe),
	DEVMETHOD(device_attach,	isic_pnp_attach),
	{ 0, 0 }
};
                                
static driver_t isic_pnp_driver = {
	"isic",
	isic_pnp_methods,
	0,
};

static devclass_t isic_devclass;

DRIVER_MODULE(isicpnp, isa, isic_pnp_driver, isic_devclass, 0, 0);

/*---------------------------------------------------------------------------*
 *      probe for ISA PnP cards
 *---------------------------------------------------------------------------*/
static int
isic_pnp_probe(device_t dev)
{
	struct isic_pnp_ids *ids;			/* pnp id's */
	char *string = NULL;				/* the name */
	u_int32_t vend_id = isa_get_vendorid(dev); 	/* vendor id */

	/* search table of knowd id's */
	
	for(ids = isic_pnp_ids; ids->vend_id != 0; ids++)
	{
		if(vend_id == ids->vend_id)
		{
			string = ids->id_str;
			break;
		}
	}
	
	if(string)		/* set name if we have one */
	{
		device_set_desc(dev, string);	/* set description */
		return 0;
	}
	else
	{
		return ENXIO;
	}
}

/*---------------------------------------------------------------------------*
 *      attach for ISA PnP cards
 *---------------------------------------------------------------------------*/
static int
isic_pnp_attach(device_t dev)
{
	u_int32_t vend_id = isa_get_vendorid(dev);	/* vendor id */
	unsigned int unit = device_get_unit(dev);	/* get unit */
	const char *name = device_get_desc(dev);	/* get description */
	struct l1_softc *sc = 0;			/* softc */
	void *ih = 0;					/* a dummy */
	int ret;
 
	/* see if we are out of bounds */
	
	if(unit >= ISIC_MAXUNIT)
	{
		printf("isic%d: Error, unit %d >= ISIC_MAXUNIT for %s\n", unit, unit, name);
		return ENXIO;
	}

	/* get information structure for this unit */

	sc = &l1_sc[unit];

	/* get io_base */
	if(!(sc->sc_resources.io_base[0] =
			bus_alloc_resource(dev, SYS_RES_IOPORT,
						&sc->sc_resources.io_rid[0],
						0UL, ~0UL, 1, RF_ACTIVE ) ))
	{
		printf("isic_pnp_attach: Couldn't get my io_base.\n");
		return ENXIO;                                       
	}
	
	/* will not be used for pnp devices */

	sc->sc_port = rman_get_start(sc->sc_resources.io_base[0]);

	/* get irq, release io_base if we don't get it */

	if(!(sc->sc_resources.irq =
			bus_alloc_resource(dev, SYS_RES_IRQ,
					   &sc->sc_resources.irq_rid,
					   0UL, ~0UL, 1, RF_ACTIVE)))
	{
		printf("isic%d: Could not get irq.\n",unit);
		isic_detach_common(dev);
		return ENXIO;                                       
	}
	
	/* not needed */
	sc->sc_irq = rman_get_start(sc->sc_resources.irq);


	/* set flag so we know what this card is */

	ret = ENXIO;
	
	switch(vend_id)
	{
#if defined(TEL_S0_16_3_P) || defined(CRTX_S0_P) || defined(COMPAQ_M610)
		case VID_TEL163PNP:
			sc->sc_cardtyp = CARD_TYPEP_163P;
			ret = isic_attach_Cs0P(dev);
			break;

		case VID_CREATIXPP:
			sc->sc_cardtyp = CARD_TYPEP_CS0P;
			ret = isic_attach_Cs0P(dev);
			break;

		case VID_COMPAQ_M610:
			sc->sc_cardtyp = CARD_TYPEP_COMPAQ_M610;
			ret = isic_attach_Cs0P(dev);
			break;
#endif
#ifdef DYNALINK
		case VID_DYNALINK:
			sc->sc_cardtyp = CARD_TYPEP_DYNALINK;
			ret = isic_attach_Dyn(dev);
			break;
#endif
#ifdef SEDLBAUER
		case VID_SEDLBAUER:
			sc->sc_cardtyp = CARD_TYPEP_SWS;
			ret = isic_attach_sws(dev);
			break;
#endif
#ifdef DRN_NGO
		case VID_NICCYGO:
			sc->sc_cardtyp = CARD_TYPEP_DRNNGO;
			ret = isic_attach_drnngo(dev);
			break;
#endif
#ifdef ELSA_QS1ISA
		case VID_ELSAQS1P:
			sc->sc_cardtyp = CARD_TYPEP_ELSAQS1ISA;
			ret = isic_attach_Eqs1pi(dev);
			break;
#endif
#ifdef ITKIX1
		case VID_ITK0025:
			sc->sc_cardtyp = CARD_TYPEP_ITKIX1;
			ret = isic_attach_itkix1(dev);
			break;
#endif			
#ifdef SIEMENS_ISURF2
		case VID_SIESURF2:
			sc->sc_cardtyp = CARD_TYPEP_SIE_ISURF2;
			ret = isic_attach_siemens_isurf(dev);
			break;
#endif
#ifdef ASUSCOM_IPAC
		case VID_ASUSCOM_IPAC:
			sc->sc_cardtyp = CARD_TYPEP_ASUSCOMIPAC;
			ret = isic_attach_asi(dev);
			break;
#endif
#ifdef EICON_DIVA
		case VID_EICON_DIVA_20:
			sc->sc_cardtyp = CARD_TYPEP_DIVA_ISA;
			ret = isic_attach_diva(dev);
			break;
		
		case VID_EICON_DIVA_202:
			sc->sc_cardtyp = CARD_TYPEP_DIVA_ISA;
			ret = isic_attach_diva_ipac(dev);
			break;
#endif
		default:
			printf("isic%d: Error, no driver for %s\n", unit, name);
			ret = ENXIO;
			break;		
	}

	if(ret)
	{
		isic_detach_common(dev);
		return ENXIO;                                       
	}		
		
	if(isic_attach_common(dev))
	{
		/* unset flag */
		sc->sc_cardtyp = CARD_TYPEP_UNK;

		/* free irq here, it hasn't been attached yet */
		bus_release_resource(dev,SYS_RES_IRQ,sc->sc_resources.irq_rid,
					sc->sc_resources.irq);
		sc->sc_resources.irq = 0;
		isic_detach_common(dev);
		return ENXIO;
	}
	else
	{
		/* setup intr routine */
		bus_setup_intr(dev,sc->sc_resources.irq,INTR_TYPE_NET,
				(void(*)(void*))isicintr,
				sc,&ih);
		return 0;
	}
}
