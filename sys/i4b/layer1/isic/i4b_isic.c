/*
 * Copyright (c) 1997, 2001 Hellmuth Michaelis. All rights reserved.
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
 *	i4b_isic.c - global isic stuff
 *	==============================
 *
 * $FreeBSD$
 *
 *      last edit-date: [Wed Jan 24 09:29:42 2001]
 *
 *---------------------------------------------------------------------------*/

#include "isic.h"
#include "opt_i4b.h"

#if NISIC > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <net/if.h>

#include <machine/i4b_debug.h>
#include <machine/i4b_ioctl.h>
#include <machine/i4b_trace.h>

#include <i4b/layer1/i4b_l1.h>

#include <i4b/layer1/isic/i4b_isic.h>
#include <i4b/layer1/isic/i4b_isic_ext.h>
#include <i4b/layer1/isic/i4b_ipac.h>
#include <i4b/layer1/isic/i4b_isac.h>
#include <i4b/layer1/isic/i4b_hscx.h>

#include <i4b/include/i4b_global.h>

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

/* jump table for multiplex routines */
struct i4b_l1mux_func isic_l1mux_func = {
	isic_ret_linktab,
	isic_set_linktab,
	isic_mph_command_req,
	isic_ph_data_req,
	isic_ph_activate_req,
};

/*---------------------------------------------------------------------------*
 *	isic - device driver interrupt routine
 *---------------------------------------------------------------------------*/
void
isicintr(struct l1_softc *sc)
{
	if(sc->sc_ipac == 0)	/* HSCX/ISAC interupt routine */
	{
		u_char was_hscx_irq = 0;
		u_char was_isac_irq = 0;

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
				was_hscx_irq = 1;			
			}
			
			if(hscx_irq_stat & (HSCX_ISTA_ICA | HSCX_ISTA_EXA))
			{
				isic_hscx_irq(sc,
				    HSCX_READ(HSCX_CH_A, H_ISTA) & ~HSCX_A_IMASK,
				    HSCX_CH_A,
				    hscx_irq_stat & HSCX_ISTA_EXA);
				was_hscx_irq = 1;
			}
	
			if(isac_irq_stat)
			{
				isic_isac_irq(sc, isac_irq_stat); /* isac handler */
				was_isac_irq = 1;
			}
		}

		HSCX_WRITE(0, H_MASK, 0xff);
		ISAC_WRITE(I_MASK, 0xff);
		HSCX_WRITE(1, H_MASK, 0xff);
	
#ifdef ELSA_QS1ISA
		DELAY(80);
		
		if((sc->sc_cardtyp == CARD_TYPEP_ELSAQS1ISA) && (sc->clearirq))
		{
			sc->clearirq(sc);
		}
#else
		DELAY(100);
#endif	
	
		HSCX_WRITE(0, H_MASK, HSCX_A_IMASK);
		ISAC_WRITE(I_MASK, ISAC_IMASK);
		HSCX_WRITE(1, H_MASK, HSCX_B_IMASK);
	}
	else	/* IPAC interrupt routine */
	{
		register u_char ipac_irq_stat;
		register u_char was_ipac_irq = 0;

		for(;;)
		{
			/* get global irq status */
			
			ipac_irq_stat = (IPAC_READ(IPAC_ISTA)) & 0x3f;
			
			/* check hscx a */
			
			if(ipac_irq_stat & (IPAC_ISTA_ICA | IPAC_ISTA_EXA))
			{
				/* HSCX A interrupt */
				isic_hscx_irq(sc, HSCX_READ(HSCX_CH_A, H_ISTA),
						HSCX_CH_A,
						ipac_irq_stat & IPAC_ISTA_EXA);
				was_ipac_irq = 1;			
			}
			if(ipac_irq_stat & (IPAC_ISTA_ICB | IPAC_ISTA_EXB))
			{
				/* HSCX B interrupt */
				isic_hscx_irq(sc, HSCX_READ(HSCX_CH_B, H_ISTA),
						HSCX_CH_B,
						ipac_irq_stat & IPAC_ISTA_EXB);
				was_ipac_irq = 1;			
			}
			if(ipac_irq_stat & IPAC_ISTA_ICD)
			{
				/* ISAC interrupt */
				isic_isac_irq(sc, ISAC_READ(I_ISTA));
				was_ipac_irq = 1;
			}
			if(ipac_irq_stat & IPAC_ISTA_EXD)
			{
				/* force ISAC interrupt handling */
				isic_isac_irq(sc, ISAC_ISTA_EXI);
				was_ipac_irq = 1;
			}
	
			/* do as long as there are pending irqs in the chip */
			if(!ipac_irq_stat)
				break;
		}

		IPAC_WRITE(IPAC_MASK, 0xff);
		DELAY(50);
		IPAC_WRITE(IPAC_MASK, 0xc0);
	}		
}

/*---------------------------------------------------------------------------*
 *	isic_recover - try to recover from irq lockup
 *---------------------------------------------------------------------------*/
void
isic_recover(struct l1_softc *sc)
{
	u_char byte;
	
	/* get hscx irq status from hscx b ista */

	byte = HSCX_READ(HSCX_CH_B, H_ISTA);

	NDBGL1(L1_ERROR, "HSCX B: ISTA = 0x%x", byte);

	if(byte & HSCX_ISTA_ICA)
		NDBGL1(L1_ERROR, "HSCX A: ISTA = 0x%x", (u_char)HSCX_READ(HSCX_CH_A, H_ISTA));

	if(byte & HSCX_ISTA_EXB)
		NDBGL1(L1_ERROR, "HSCX B: EXIR = 0x%x", (u_char)HSCX_READ(HSCX_CH_B, H_EXIR));

	if(byte & HSCX_ISTA_EXA)
		NDBGL1(L1_ERROR, "HSCX A: EXIR = 0x%x", (u_char)HSCX_READ(HSCX_CH_A, H_EXIR));

	/* get isac irq status */

	byte = ISAC_READ(I_ISTA);

	NDBGL1(L1_ERROR, "  ISAC: ISTA = 0x%x", byte);
	
	if(byte & ISAC_ISTA_EXI)
		NDBGL1(L1_ERROR, "  ISAC: EXIR = 0x%x", (u_char)ISAC_READ(I_EXIR));

	if(byte & ISAC_ISTA_CISQ)
	{
		byte = ISAC_READ(I_CIRR);
	
		NDBGL1(L1_ERROR, "  ISAC: CISQ = 0x%x", byte);
		
		if(byte & ISAC_CIRR_SQC)
			NDBGL1(L1_ERROR, "  ISAC: SQRR = 0x%x", (u_char)ISAC_READ(I_SQRR));
	}

	NDBGL1(L1_ERROR, "HSCX B: IMASK = 0x%x", HSCX_B_IMASK);
	NDBGL1(L1_ERROR, "HSCX A: IMASK = 0x%x", HSCX_A_IMASK);

	HSCX_WRITE(0, H_MASK, 0xff);
	HSCX_WRITE(1, H_MASK, 0xff);
	DELAY(100);	
	HSCX_WRITE(0, H_MASK, HSCX_A_IMASK);
	HSCX_WRITE(1, H_MASK, HSCX_B_IMASK);
	DELAY(100);

	NDBGL1(L1_ERROR, "  ISAC: IMASK = 0x%x", ISAC_IMASK);

	ISAC_WRITE(I_MASK, 0xff);	
	DELAY(100);
	ISAC_WRITE(I_MASK, ISAC_IMASK);
}

/*---------------------------------------------------------------------------*
 *	isic_attach_common - common attach routine for all busses
 *---------------------------------------------------------------------------*/
int
isic_attach_common(device_t dev)
{
	char *drvid = NULL;
	int unit = device_get_unit(dev);
	struct l1_softc *sc = &l1_sc[unit];
	
	sc->sc_unit = unit;
	
	sc->sc_isac_version = 0;
	sc->sc_hscx_version = 0;

	if(sc->sc_ipac)
	{
		sc->sc_ipac_version = IPAC_READ(IPAC_ID);

		switch(sc->sc_ipac_version)
  		{
			case IPAC_V11:
			case IPAC_V12:
  				break;

  			default:
  				printf("isic%d: Error, IPAC version %d unknown!\n",
  					unit, sc->sc_ipac_version);
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
				break;
	
			default:
				printf("isic%d: Error, ISAC version %d unknown!\n",
					unit, sc->sc_isac_version);
				return ENXIO;
				break;
		}

		sc->sc_hscx_version = HSCX_READ(0, H_VSTR) & 0xf;

		switch(sc->sc_hscx_version)
		{
			case HSCX_VA1:
			case HSCX_VA2:
			case HSCX_VA3:
			case HSCX_V21:
				break;
				
			default:
				printf("isic%d: Error, HSCX version %d unknown!\n",
					unit, sc->sc_hscx_version);
				return ENXIO;
				break;
		}
	}
	
	isic_isac_init(sc);		/* ISAC setup */

	/* HSCX setup */

	isic_bchannel_setup(sc->sc_unit, HSCX_CH_A, BPROT_NONE, 0);
	
	isic_bchannel_setup(sc->sc_unit, HSCX_CH_B, BPROT_NONE, 0);

	isic_init_linktab(sc);		/* setup linktab */

	sc->sc_trace = TRACE_OFF;	/* set trace level */

	sc->sc_state = ISAC_IDLE;	/* set state */

	sc->sc_ibuf = NULL;		/* input buffering */
	sc->sc_ib = NULL;
	sc->sc_ilen = 0;

	sc->sc_obuf = NULL;		/* output buffering */
	sc->sc_op = NULL;
	sc->sc_ol = 0;
	sc->sc_freeflag = 0;

	sc->sc_obuf2 = NULL;		/* second output buffer */
	sc->sc_freeflag2 = 0;

	/* timer setup */
	
	callout_handle_init(&sc->sc_T3_callout);
	callout_handle_init(&sc->sc_T4_callout);	
	
	/* init higher protocol layers */
	
	i4b_l1_mph_status_ind(L0ISICUNIT(sc->sc_unit), STI_ATTACH, sc->sc_cardtyp, &isic_l1mux_func);

	/* announce manufacturer and card type for ISA cards */
	
	switch(sc->sc_cardtyp)
	{
		case CARD_TYPEP_8:
			drvid = "Teles S0/8 (or compatible)";
			break;

		case CARD_TYPEP_16:
			drvid = "Teles S0/16 (or compatible)";
			break;

		case CARD_TYPEP_16_3:
			drvid = "Teles S0/16.3";
			break;

		case CARD_TYPEP_AVMA1:
			drvid = "AVM A1 or Fritz!Card Classic";
			break;

		case CARD_TYPEP_PCFRITZ:
			drvid = "AVM Fritz!Card PCMCIA";
			break;

		case CARD_TYPEP_USRTA:
			drvid = "USRobotics Sportster ISDN TA intern";
			break;

		case CARD_TYPEP_ITKIX1:
			drvid = "ITK ix1 micro";
			break;

		case CARD_TYPEP_PCC16:
			drvid = "ELSA MicroLink ISDN/PCC-16";
			break;

		default:
			drvid = NULL;	/* pnp/pci cards announce themselves */
			break;
	}

	if(drvid)
		printf("isic%d: %s\n", unit, drvid);
	
	if(bootverbose)
	{
		/* announce chip versions */
		
		if(sc->sc_ipac)
		{
			if(sc->sc_ipac_version == IPAC_V11)
				printf("isic%d: IPAC PSB2115 Version 1.1\n", unit);
			else
				printf("isic%d: IPAC PSB2115 Version 1.2\n", unit);
		}
		else
		{
			printf("isic%d: ISAC %s (IOM-%c)\n",
				unit,
				ISACversion[sc->sc_isac_version],
				sc->sc_bustyp == BUS_TYPE_IOM1 ? '1' : '2');

			printf("isic%d: HSCX %s\n",
				unit,
				HSCXversion[sc->sc_hscx_version]);
		}
	}
	return 0;
}

/*---------------------------------------------------------------------------*
 *	isic_detach_common - common detach routine for all busses
 *---------------------------------------------------------------------------*/
void
isic_detach_common(device_t dev)
{
	struct l1_softc *sc = &l1_sc[device_get_unit(dev)];
	int i;

	sc->sc_cardtyp = CARD_TYPEP_UNK;

	/* free interrupt resources */
	
	if(sc->sc_resources.irq)
	{
		/* tear down interupt handler */
		bus_teardown_intr(dev, sc->sc_resources.irq,
					(void(*)(void *))isicintr);

		/* free irq */
		bus_release_resource(dev, SYS_RES_IRQ,
					sc->sc_resources.irq_rid,
					sc->sc_resources.irq);
		sc->sc_resources.irq_rid = 0;
		sc->sc_resources.irq = 0;
	}

	/* free memory resource */
	
	if(sc->sc_resources.mem)
	{
		bus_release_resource(dev,SYS_RES_MEMORY,
					sc->sc_resources.mem_rid,
					sc->sc_resources.mem);
		sc->sc_resources.mem_rid = 0;
		sc->sc_resources.mem = 0;
	}

	/* free iobases */

	for(i=0; i < INFO_IO_BASES ; i++)
	{
		if(sc->sc_resources.io_base[i])
		{
			bus_release_resource(dev, SYS_RES_IOPORT,
						sc->sc_resources.io_rid[i],
						sc->sc_resources.io_base[i]);
			sc->sc_resources.io_rid[i] = 0;
			sc->sc_resources.io_base[i] = 0;			
		}
	}
}

#endif /* NISIC > 0 */

