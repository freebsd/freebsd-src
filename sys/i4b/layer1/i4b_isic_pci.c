/*
 * Copyright (c) 1997, 1999 Hellmuth Michaelis. All rights reserved.
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
 *	i4b_isic_pci.c - PCI bus interface
 *	==================================
 *
 *	$Id: i4b_isic_pci.c,v 1.4 1999/04/24 20:24:02 peter Exp $
 *
 *      last edit-date: [Wed Feb 17 15:19:44 1999]
 *
 *---------------------------------------------------------------------------*/

#include "isic.h"

#if defined(__FreeBSD__)
#include "opt_i4b.h"
#include "pci.h"
#endif

#if (NISIC > 0) && (NPCI > 0)

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <net/if.h>

#ifdef __FreeBSD__
#if __FreeBSD__ >= 3
#include <sys/ioccom.h>
#else
#include <sys/ioctl.h>
#endif
#include <machine/clock.h>
#include <i386/isa/isa_device.h>
#include <pci/pcireg.h>
#include <pci/pcivar.h>
#else
#include <machine/bus.h>
#include <sys/device.h>
#endif

#ifdef __FreeBSD__
#include <machine/i4b_debug.h>
#include <machine/i4b_ioctl.h>
#else
#include <i4b/i4b_debug.h>
#include <i4b/i4b_ioctl.h>
#endif

#include <i4b/include/i4b_global.h>
#include <i4b/include/i4b_l1l2.h>
#include <i4b/include/i4b_mbuf.h>

#include <i4b/layer1/i4b_l1.h>
#include <i4b/layer1/i4b_ipac.h>
#include <i4b/layer1/i4b_isac.h>
#include <i4b/layer1/i4b_hscx.h>

#define PCI_QS1000_ID	0x10001048
#define PCI_AVMA1_ID	0x0a001244

#define MEM0_MAPOFF	0
#define PORT0_MAPOFF	4
#define PORT1_MAPOFF	12

static char* i4b_pci_probe(pcici_t tag, pcidi_t type);
static void i4b_pci_attach(pcici_t config_id, int unit);
static int isic_pciattach(int unit, u_long type, u_int iobase1, u_int iobase2);

static u_long i4b_pci_count = 0;

static struct pci_device i4b_pci_driver = {
	"isic",
	i4b_pci_probe,
	i4b_pci_attach,
	&i4b_pci_count,
	NULL
};

COMPAT_PCI_DRIVER (isic_pci, i4b_pci_driver);

static void isic_pci_intr_sc(struct isic_softc *sc);

#ifdef AVM_A1_PCI
extern void avma1pp_map_int(pcici_t, void *, unsigned *);
#endif


/*---------------------------------------------------------------------------*
 *	PCI probe routine
 *---------------------------------------------------------------------------*/
static char *
i4b_pci_probe(pcici_t tag, pcidi_t type)
{
	switch(type)
	{
		case PCI_QS1000_ID:
			return("ELSA QuickStep 1000pro PCI ISDN adapter");

		case PCI_AVMA1_ID:
			return("AVM Fritz!Card PCI ISDN adapter");
			
		default:
			if(bootverbose)
				printf("i4b_pci_probe: unknown PCI type %ul!\n", (u_int)type);
			return(NULL);
        }
	return(NULL);
}

/*---------------------------------------------------------------------------*
 *	PCI attach routine
 *---------------------------------------------------------------------------*/
static void
i4b_pci_attach(pcici_t config_id, int unit)
{
	unsigned short iobase1;
	unsigned short iobase2;	
	unsigned long type;
	struct isic_softc *sc = &isic_sc[unit];
	u_long reg1, reg2;
	
	if(unit != next_isic_unit)
	{
		printf("i4b_pci_attach: Error: new unit (%d) != next_isic_unit (%d)!\n", unit, next_isic_unit);
		return;
	}

	/* IMHO the all following should be done in the low-level driver - GJ */
	type = pci_conf_read(config_id, PCI_ID_REG);
		
	/* not all cards have their ports at the same location !!! */
	switch(type)
	{
		case PCI_QS1000_ID:
			reg1 = PCI_MAP_REG_START+PORT0_MAPOFF;
			reg2 = PCI_MAP_REG_START+PORT1_MAPOFF;
			break;

		case PCI_AVMA1_ID:
			reg1 = PCI_MAP_REG_START+PORT0_MAPOFF;
			reg2 = 0;
			break;
			
		default:
			reg1 = PCI_MAP_REG_START+PORT0_MAPOFF;
			reg2 = PCI_MAP_REG_START+PORT1_MAPOFF;
			break;
	}

	if(reg1 && !(pci_map_port(config_id, reg1, &iobase1)))
	{
		printf("i4b_pci_attach: pci_map_port 1 failed!\n");
		return;
	}

	if(reg2 && !(pci_map_port(config_id, reg2, &iobase2)))
	{
		printf("i4b_pci_attach: pci_map_port 2 failed!\n");
		return;
	}

	if(bootverbose)
		printf("i4b_pci_attach: unit %d, port0 0x%x, port1 0x%x\n", unit, iobase1, iobase2);

	if((isic_pciattach(unit, type, iobase1, iobase2)) == 0)
		return;

#ifdef AVM_A1_PCI
	/* the AVM FRTIZ!PCI needs to handle its own interrupts */
	if (type == PCI_AVMA1_ID)
	{
		avma1pp_map_int(config_id, (void *)sc, &net_imask);
		return;
	}
#endif

	/* seems like this should be done before the attach in case it fails */
	if(!(pci_map_int(config_id, (void *)isic_pci_intr_sc, (void *)sc, &net_imask)))
		return;
}

/*---------------------------------------------------------------------------*
 *	isic - pci device driver attach routine
 *---------------------------------------------------------------------------*/
static int
isic_pciattach(int unit, u_long type, u_int iobase1, u_int iobase2)
{
	int ret = 0;
	struct isic_softc *sc = &isic_sc[unit];

  	static char *ISACversion[] = {
  		"2085 Version A1/A2 or 2086/2186 Version 1.1",
		"2085 Version B1",
		"2085 Version B2",
		"2085 Version V2.3 (B3)",
		"Unknown Version"
	};

	static char *HSCXversion[] = {
		"82525 Version A1",
		"Unknown (0x01)",
		"82525 Version A2",
		"Unknown (0x03)",
		"82525 Version A3",
		"82525 or 21525 Version 2.1",
		"Unknown Version"
	};

	switch(type)
	{
#ifdef ELSA_QS1PCI
		case PCI_QS1000_ID:
			ret = isic_attach_Eqs1pp(unit, iobase1, iobase2);
			break;
#endif
#ifdef AVM_A1_PCI
		case PCI_AVMA1_ID:
			ret = isic_attach_avma1pp(unit, iobase1, iobase2);
	   		if (ret)
	     			next_isic_unit++;
			return(ret);
#endif
		default:
			break;
	}

	if(ret == 0)
		return(ret);

	sc->sc_isac_version = 0;
	sc->sc_hscx_version = 0;
	
	sc->sc_unit = unit;

	if(sc->sc_ipac)
	{
		ret = IPAC_READ(IPAC_ID);

		switch(ret)
		{
			case 0x01:
				printf("isic%d: IPAC PSB2115 Version 1.1\n", unit);
				break;
	
			default:
				printf("isic%d: Error, IPAC version %d unknown!\n",
					unit, ret);
				return(0);
				break;
		}
	}
	else
	{
		sc->sc_isac_version = ((ISAC_READ(I_RBCH)) >> 5) & 0x03;
	
		switch(sc->sc_isac_version)
		{
			case ISAC_VA:
			case ISAC_VB1:
	                case ISAC_VB2:
			case ISAC_VB3:
				printf("isic%d: ISAC %s (IOM-%c)\n",
					unit,
					ISACversion[sc->sc_isac_version],
					sc->sc_bustyp == BUS_TYPE_IOM1 ? '1' : '2');
				break;
	
			default:
				printf("isic%d: Error, ISAC version %d unknown!\n",
					unit, sc->sc_isac_version);
				return(0);
				break;
		}
	
		sc->sc_hscx_version = HSCX_READ(0, H_VSTR) & 0xf;
	
		switch(sc->sc_hscx_version)
		{
			case HSCX_VA1:
			case HSCX_VA2:
			case HSCX_VA3:
			case HSCX_V21:
				printf("isic%d: HSCX %s\n",
					unit,
					HSCXversion[sc->sc_hscx_version]);
				break;
				
			default:
				printf("isic%d: Error, HSCX version %d unknown!\n",
					unit, sc->sc_hscx_version);
				return(0);
				break;
		}
	}
	
	/* ISAC setup */
	
	isic_isac_init(sc);

	/* HSCX setup */

	isic_bchannel_setup(sc->sc_unit, HSCX_CH_A, BPROT_NONE, 0);
	
	isic_bchannel_setup(sc->sc_unit, HSCX_CH_B, BPROT_NONE, 0);

	/* setup linktab */

	isic_init_linktab(sc);

	/* set trace level */

	sc->sc_trace = TRACE_OFF;

	sc->sc_state = ISAC_IDLE;

	sc->sc_ibuf = NULL;
	sc->sc_ib = NULL;
	sc->sc_ilen = 0;

	sc->sc_obuf = NULL;
	sc->sc_op = NULL;
	sc->sc_ol = 0;
	sc->sc_freeflag = 0;

	sc->sc_obuf2 = NULL;
	sc->sc_freeflag2 = 0;

#if defined(__FreeBSD__) && __FreeBSD__ >=3
	callout_handle_init(&sc->sc_T3_callout);
	callout_handle_init(&sc->sc_T4_callout);	
#endif
	
	/* init higher protocol layers */
	
	MPH_Status_Ind(sc->sc_unit, STI_ATTACH, sc->sc_cardtyp);

	next_isic_unit++;

	return(1);
}

/*---------------------------------------------------------------------------*
 *	isic - PCI device driver interrupt routine
 *---------------------------------------------------------------------------*/
static void
isic_pci_intr_sc(struct isic_softc *sc)
{
	if(sc->sc_ipac == 0)	/* HSCX/ISAC interupt routine */
	{
		register u_char hscx_irq_stat;
		register u_char isac_irq_stat;

		for(;;)
		{
			/* get hscx irq status from hscx b ista */
			hscx_irq_stat =
	 	    	    HSCX_READ(HSCX_CH_B, H_ISTA) & ~HSCX_B_IMASK;
	
			/* get isac irq status */
			isac_irq_stat = ISAC_READ(I_ISTA);
	
			/* do as long as there are pending irqs in the chips */
			if(!hscx_irq_stat && !isac_irq_stat)
				break;
	
			if(hscx_irq_stat & (HSCX_ISTA_RME | HSCX_ISTA_RPF |
					    HSCX_ISTA_RSC | HSCX_ISTA_XPR |
					    HSCX_ISTA_TIN | HSCX_ISTA_EXB))
			{
				isic_hscx_irq(sc, hscx_irq_stat,
						HSCX_CH_B,
						hscx_irq_stat & HSCX_ISTA_EXB);
			}
			
			if(hscx_irq_stat & (HSCX_ISTA_ICA | HSCX_ISTA_EXA))
			{
				isic_hscx_irq(sc,
				    HSCX_READ(HSCX_CH_A, H_ISTA) & ~HSCX_A_IMASK,
				    HSCX_CH_A,
				    hscx_irq_stat & HSCX_ISTA_EXA);
			}
	
			if(isac_irq_stat)
			{	 /* isac handler */
				isic_isac_irq(sc, isac_irq_stat);
			}
		}
			
		HSCX_WRITE(0, H_MASK, 0xff);
		ISAC_WRITE(I_MASK, 0xff);
		HSCX_WRITE(1, H_MASK, 0xff);
	
		DELAY(100);
	
		HSCX_WRITE(0, H_MASK, HSCX_A_IMASK);
		ISAC_WRITE(I_MASK, ISAC_IMASK);
		HSCX_WRITE(1, H_MASK, HSCX_B_IMASK);
	}
	else	/* IPAC interrupt routine */
	{
		register u_char ipac_irq_stat;

		for(;;)
		{
			/* get global irq status */
			
			ipac_irq_stat = (IPAC_READ(IPAC_ISTA)) & 0x3f;

			/* do as long as there are pending irqs in the chip */
			if(!ipac_irq_stat)
				break;

			/* check hscx a */
			
			if(ipac_irq_stat & (IPAC_ISTA_ICA | IPAC_ISTA_EXA))
			{
				/* HSCX A interrupt */
				isic_hscx_irq(sc, HSCX_READ(HSCX_CH_A, H_ISTA),
						HSCX_CH_A,
						ipac_irq_stat & IPAC_ISTA_EXA);
			}
			if(ipac_irq_stat & (IPAC_ISTA_ICB | IPAC_ISTA_EXB))
			{
				/* HSCX B interrupt */
				isic_hscx_irq(sc, HSCX_READ(HSCX_CH_B, H_ISTA),
						HSCX_CH_B,
						ipac_irq_stat & IPAC_ISTA_EXB);
			}
			if(ipac_irq_stat & (IPAC_ISTA_ICD | IPAC_ISTA_EXD))
			{
				/* ISAC interrupt */
				isic_isac_irq(sc, ISAC_READ(I_ISTA));
			}
		}

		IPAC_WRITE(IPAC_MASK, 0xff);
		DELAY(50);
		IPAC_WRITE(IPAC_MASK, 0xc0);
	}		
}

#endif /* (NISIC > 0) && (NPCI > 0) */

