/*
 * Copyright (c) 1997, 2000 Hellmuth Michaelis. All rights reserved.
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
 *	i4b_isic_isa.c - ISA bus interface
 *	==================================
 *
 *	$Id: i4b_isic_isa.c,v 1.3 2000/05/29 15:41:41 hm Exp $ 
 *
 * $FreeBSD$
 *
 *      last edit-date: [Fri Oct 13 16:00:46 2000]
 *
 *---------------------------------------------------------------------------*/

#include "isic.h"
#include "opt_i4b.h"

#if NISIC > 0

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <net/if.h>


#include <machine/i4b_debug.h>
#include <machine/i4b_ioctl.h>
#include <machine/i4b_trace.h>

#include <i4b/layer1/isic/i4b_isic.h>
#include <i4b/layer1/isic/i4b_ipac.h>
#include <i4b/layer1/isic/i4b_isac.h>
#include <i4b/layer1/isic/i4b_hscx.h>

#include <sys/bus.h>
#include <isa/isavar.h>

struct l1_softc l1_sc[ISIC_MAXUNIT];

static int isic_isa_probe(device_t dev);
static int isic_isa_attach(device_t dev);

static device_method_t isic_methods[] = {
	DEVMETHOD(device_probe,		isic_isa_probe),
	DEVMETHOD(device_attach,	isic_isa_attach),
	{ 0, 0 }
};

static driver_t isic_driver = {
	"isic",
	isic_methods,
	0
};

static devclass_t isic_devclass;

DRIVER_MODULE(isic, isa, isic_driver, isic_devclass, 0, 0);

/*---------------------------------------------------------------------------*
 *	probe for ISA non-PnP cards
 *---------------------------------------------------------------------------*/
static int
isic_isa_probe(device_t dev)
{
	int ret = ENXIO;

	if(isa_get_vendorid(dev))	/* no PnP probes here */
		return ENXIO;

	switch(device_get_flags(dev))
	{
#ifdef TEL_S0_16
		case FLAG_TELES_S0_16:
			ret = isic_probe_s016(dev);
			break;
#endif

#ifdef TEL_S0_8
		case FLAG_TELES_S0_8:
			ret = isic_probe_s08(dev);
			break;
#endif

#ifdef ELSA_PCC16
		case FLAG_ELSA_PCC16:
			ret = isic_probe_Epcc16(dev);
			break;
#endif

#ifdef TEL_S0_16_3
		case FLAG_TELES_S0_163:
			ret = isic_probe_s0163(dev);		
			break;
#endif

#ifdef AVM_A1
		case FLAG_AVM_A1:
			ret = isic_probe_avma1(dev);
			break;
#endif

#ifdef USR_STI
		case FLAG_USR_ISDN_TA_INT:
			ret = isic_probe_usrtai(dev);		
			break;
#endif

#ifdef ITKIX1
		case FLAG_ITK_IX1:
			ret = isic_probe_itkix1(dev);
			break;
#endif

		default:
			printf("isic%d: probe, unknown flag: %d\n",
				device_get_unit(dev), device_get_flags(dev));
			break;
	}
	return(ret);
}

/*---------------------------------------------------------------------------*
 *	attach for ISA non-PnP cards
 *---------------------------------------------------------------------------*/
static int
isic_isa_attach(device_t dev)
{
	int ret = ENXIO;

	struct l1_softc *sc = &l1_sc[device_get_unit(dev)];

	sc->sc_unit = device_get_unit(dev);
	
	/* card dependent setup */

	switch(sc->sc_flags)
	{
#ifdef TEL_S0_16
		case FLAG_TELES_S0_16:
			ret = isic_attach_s016(dev);
			break;
#endif

#ifdef TEL_S0_8
		case FLAG_TELES_S0_8:
			ret = isic_attach_s08(dev);
			break;
#endif

#ifdef ELSA_PCC16
		case FLAG_ELSA_PCC16:
			ret = isic_attach_Epcc16(dev);
			break;
#endif

#ifdef TEL_S0_16_3
		case FLAG_TELES_S0_163:
			ret = isic_attach_s0163(dev);
			break;
#endif

#ifdef AVM_A1
		case FLAG_AVM_A1:
			ret = isic_attach_avma1(dev);
			break;
#endif

#ifdef USR_STI
		case FLAG_USR_ISDN_TA_INT:
			ret = isic_attach_usrtai(dev);		
			break;
#endif

#ifdef ITKIX1
		case FLAG_ITK_IX1:
			ret = isic_attach_itkix1(dev);
			break;
#endif

		default:
			printf("isic%d: attach, unknown flag: %d\n",
				device_get_unit(dev), device_get_flags(dev));
			break;
	}

	if(ret)
		return(ret);
		
	ret = isic_attach_common(dev);

	return(ret);
}
#endif /* NISIC > 0 */
