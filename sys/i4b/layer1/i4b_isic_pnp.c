/*
 *   Copyright (c) 1998 Eivind Eklund. All rights reserved.
 *
 *   Copyright (c) 1998 German Tischler. All rights reserved.
 *
 *   Copyright (c) 1998, 1999 Hellmuth Michaelis. All rights reserved. 
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
 *	$Id: i4b_isic_pnp.c,v 1.17 1999/04/20 14:28:46 hm Exp $
 *
 *      last edit-date: [Tue Apr 20 16:12:27 1999]
 *
 *---------------------------------------------------------------------------*/

#ifdef __FreeBSD__

#include "pnp.h"
#include "isic.h"
#include "opt_i4b.h"

#if (NISIC > 0) && (NPNP > 0)

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <net/if.h>

#if defined(__FreeBSD__) && __FreeBSD__ < 3
#include "ioconf.h"
extern void isicintr(int unit); 	/* XXX this gives a compiler warning */
					/* on one 2.2.7 machine but no       */
					/* warning on another one !? (-hm)   */
#endif

#if (defined(__FreeBSD_version) && __FreeBSD_version >= 300006)
extern void isicintr(int unit);
#endif

#include <machine/clock.h>
#include <i386/isa/isa_device.h>
#include <i386/isa/pnp.h>

#include <i4b/include/i4b_global.h>
#include <machine/i4b_ioctl.h>
#include <i4b/layer1/i4b_l1.h>

#define VID_TEL163PNP	0x10212750	/* Teles 16.3 PnP	*/
#define VID_CREATIXPP	0x0000980e	/* Creatix S0/16 P+P	*/
#define VID_DYNALINK	0x88167506	/* Dynalink		*/
#define VID_SEDLBAUER	0x0100274c	/* Sedlbauer WinSpeed	*/
#define VID_NICCYGO	0x5001814c	/* Neuhaus Niccy GO@	*/
#define VID_ELSAQS1P	0x33019315	/* ELSA Quickstep1000pro*/

static struct i4b_pnp_ids {
	u_long vend_id;
	char *id_str;
} i4b_pnp_ids[] = {
	{ VID_TEL163PNP,	"Teles 16.3 PnP"	},
	{ VID_CREATIXPP,	"Creatix S0/16 P+P"	},
	{ VID_DYNALINK,		"Dynalink IS64PH"	},
	{ VID_SEDLBAUER,	"Sedlbauer WinSpeed"	},
	{ VID_NICCYGO,		"Dr.Neuhaus Niccy Go@"	},
	{ VID_ELSAQS1P,		"ELSA QuickStep 1000pro"},	
	{ 0 }
};

extern struct isa_driver isicdriver;

static int isic_pnpprobe(struct isa_device *dev, unsigned int iobase2);
static char *i4b_pnp_probe(u_long csn, u_long vend_id);
static void i4b_pnp_attach(u_long csn, u_long vend_id, char *name, struct isa_device *dev);

static u_long ni4b_pnp = 0; 

static struct pnp_device i4b_pnp = {
	"i4b_pnp",
	i4b_pnp_probe,
	i4b_pnp_attach,
	&ni4b_pnp,
	&net_imask
};

DATA_SET(pnpdevice_set, i4b_pnp);

/*---------------------------------------------------------------------------*
 *	PnP probe routine
 *---------------------------------------------------------------------------*/
static char *
i4b_pnp_probe(u_long csn, u_long vend_id)
{
	struct i4b_pnp_ids *ids;
	char *string = NULL;

	/* search table of knowd id's */
	
	for(ids = i4b_pnp_ids; ids->vend_id != 0; ids++)
	{
		if(vend_id == ids->vend_id)
		{
			string = ids->id_str;
			break;
		}
	}

	if(string)
	{
		struct pnp_cinfo spci;

		read_pnp_parms(&spci, 0);

		if((spci.enable == 0) || (spci.flags & 0x01))
		{
			printf("CSN %d (%s) is disabled.\n", (int)csn, string);
			return (NULL);
		}
	}
	return(string);
}

/*---------------------------------------------------------------------------*
 *	PnP attach routine
 *---------------------------------------------------------------------------*/
static void
i4b_pnp_attach(u_long csn, u_long vend_id, char *name, struct isa_device *dev)
{
	struct pnp_cinfo spci;
	struct isa_device *isa_devp;

	if(dev->id_unit != next_isic_unit)
	{
		printf("i4b_pnp_attach: Error: new unit (%d) != next_isic_unit (%d)!\n", dev->id_unit, next_isic_unit);
		return;
	}

	if(dev->id_unit >= ISIC_MAXUNIT)
	{
		printf("isic%d: Error, unit %d >= ISIC_MAXUNIT for %s\n",
		        dev->id_unit, dev->id_unit, name);
		return;
	}

	if(read_pnp_parms(&spci, 0) == 0)
	{
		printf("isic%d: read_pnp_parms error for %s\n",
		        dev->id_unit, name);
		return;
	}

	if(bootverbose)
	{
		printf("isic%d: vendorid = 0x%08x port0 = 0x%04x, port1 = 0x%04x, irq = %d\n",
			dev->id_unit, spci.vendor_id, spci.port[0], spci.port[1], spci.irq[0]);
	}
	
	dev->id_iobase = spci.port[0];
	dev->id_irq = (1 << spci.irq[0]);
	dev->id_intr = (inthand2_t *) isicintr;
	dev->id_drq = -1;

/* XXX add dev->id_alive init here ! ?? */

	switch(spci.vendor_id)
	{
		case VID_TEL163PNP:
			dev->id_flags = FLAG_TELES_S0_163_PnP;
			break;
		case VID_CREATIXPP:
			dev->id_flags = FLAG_CREATIX_S0_PnP;
			break;
		case VID_DYNALINK:
			dev->id_flags = FLAG_DYNALINK;
			break;
		case VID_SEDLBAUER:
			dev->id_flags = FLAG_SWS;
			break;
		case VID_NICCYGO:
			dev->id_flags = FLAG_DRN_NGO;
			break;
		case VID_ELSAQS1P:
			dev->id_flags = FLAG_ELSA_QS1P_ISA;
			break;
	}

	write_pnp_parms(&spci, 0);
	enable_pnp_card();
	
	if(dev->id_driver == NULL)
	{
		dev->id_driver = &isicdriver;
#if(defined(__FreeBSD_version) && __FreeBSD_version >= 400004)
		dev->id_id = isa_compat_nextid();
#else
		isa_devp = find_isadev(isa_devtab_net, &isicdriver, 0);

		if(isa_devp != NULL)
		{
			dev->id_id = isa_devp->id_id;
		}
#endif
	}

	if((dev->id_alive = isic_pnpprobe(dev, spci.port[1])) != 0)
	{
/* XXX dev->id_alive is the size of the port area used ! */
		isic_realattach(dev, spci.port[1]);
	}
	else
	{
		printf("isic%d: probe failed!\n", dev->id_unit);
	}
}

/*---------------------------------------------------------------------------*
 *	isic - pnp device driver probe routine
 *---------------------------------------------------------------------------*/
static int
isic_pnpprobe(struct isa_device *dev, unsigned int iobase2)
{
	int ret = 0;

	switch(dev->id_flags)
	{
#ifdef TEL_S0_16_3_P
		case FLAG_TELES_S0_163_PnP:
			ret = isic_probe_s0163P(dev, iobase2);
			break;
#endif

#ifdef CRTX_S0_P
		case FLAG_CREATIX_S0_PnP:
			ret = isic_probe_Cs0P(dev, iobase2);
			break;
#endif

#ifdef DRN_NGO
		case FLAG_DRN_NGO:
			ret = isic_probe_drnngo(dev, iobase2);
			break;
#endif

#ifdef SEDLBAUER
		case FLAG_SWS:
			ret = 8;	/* pnp only, nothing to probe */
			break;
#endif

#ifdef DYNALINK
		case FLAG_DYNALINK:
			ret = isic_probe_Dyn(dev, iobase2);
			break;
#endif

#ifdef ELSA_QS1ISA
		case FLAG_ELSA_QS1P_ISA:
			ret = isic_probe_Eqs1pi(dev, iobase2);
			break;
#endif
		default:
			break;
	}
	return(ret);
}

#endif /* (NISIC > 0) && (NPNP > 0) */
#endif /* __FreeBSD__ */
