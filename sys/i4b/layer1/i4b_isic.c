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
 *	i4b_isic.c - global isic stuff
 *	==============================
 *
 * $FreeBSD$ 
 *
 *      last edit-date: [Mon Jul 26 10:59:56 1999]
 *
 *---------------------------------------------------------------------------*/

#ifdef __FreeBSD__
#include "isic.h"
#include "opt_i4b.h"
#else
#define NISIC	1
#endif
#if NISIC > 0

#include <sys/param.h>
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
#include <sys/ioccom.h>
#else
#include <sys/ioctl.h>
#endif
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <net/if.h>

#ifdef __FreeBSD__
#include <machine/clock.h>
#include <i386/isa/isa_device.h>
#else
#include <sys/device.h>
#if defined(__NetBSD__) && defined(amiga)
#include <machine/bus.h>
#else
#ifndef __bsdi__
#include <dev/isa/isavar.h>
#endif
#endif
#endif

#ifdef __FreeBSD__
#include <machine/i4b_debug.h>
#include <machine/i4b_ioctl.h>
#include <machine/i4b_trace.h>
#else
#include <i4b/i4b_debug.h>
#include <i4b/i4b_ioctl.h>
#include <i4b/i4b_trace.h>
#endif

#include <i4b/layer1/i4b_l1.h>
#include <i4b/layer1/i4b_ipac.h>
#include <i4b/layer1/i4b_isac.h>
#include <i4b/layer1/i4b_hscx.h>

#include <i4b/include/i4b_l1l2.h>
#include <i4b/include/i4b_mbuf.h>
#include <i4b/include/i4b_global.h>

void isic_settrace(int unit, int val);
int isic_gettrace(int unit);

#ifdef __bsdi__
static int isicmatch(struct device *parent, struct cfdata *cf, void *aux);
static void isicattach(struct device *parent, struct device *self, void *aux);
struct cfdriver isiccd =
	{ NULL, "isic", isicmatch, isicattach, DV_IFNET,
	  sizeof(struct isic_softc) };

int isa_isicmatch(struct device *parent, struct cfdata *cf, struct isa_attach_args *);
int isapnp_isicmatch(struct device *parent, struct cfdata *cf, struct isa_attach_args *);
int isa_isicattach(struct device *parent, struct device *self, struct isa_attach_args *ia);

static int
isicmatch(struct device *parent, struct cfdata *cf, void *aux)
{
	struct isa_attach_args *ia = (struct isa_attach_args *) aux;
	if (ia->ia_bustype == BUS_PCMCIA) {
		ia->ia_irq = IRQNONE;
		/* return 1;	Not yet */
		return 0;	/* for now */
	}
	if (ia->ia_bustype == BUS_PNP) {
		return isapnp_isicmatch(parent, cf, ia);
	}
	return isa_isicmatch(parent, cf, ia);
}

static void
isicattach(struct device *parent, struct device *self, void *aux)
{
	struct isa_attach_args *ia = (struct isa_attach_args *) aux;
	struct isic_softc *sc = (struct isic_softc *)self;

	sc->sc_flags = sc->sc_dev.dv_flags;
	isa_isicattach(parent, self, ia);
	isa_establish(&sc->sc_id, &sc->sc_dev);
	sc->sc_ih.ih_fun = isicintr;
	sc->sc_ih.ih_arg = (void *)sc;
	intr_establish(ia->ia_irq, &sc->sc_ih, DV_NET);
	/* Could add a shutdown hook here... */
}
#endif

#ifdef __FreeBSD__
void isicintr_sc(struct isic_softc *sc);
#if !(defined(__FreeBSD_version)) || (defined(__FreeBSD_version) && __FreeBSD_version >= 300006)
void isicintr(int unit);
#endif
#else
/* XXX - hack, going away soon! */
struct isic_softc *isic_sc[ISIC_MAXUNIT];
#endif

/*---------------------------------------------------------------------------*
 *	isic - device driver interrupt routine
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__

void
isicintr_sc(struct isic_softc *sc)
{
	isicintr(sc->sc_unit);
}

void
isicintr(int unit)
{
	register struct isic_softc *sc = &isic_sc[unit];
#else
int
isicintr(void *arg)
{
	struct isic_softc *sc = arg;
#endif
	
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

#ifdef NOTDEF

#if !defined(amiga) && !defined(atari) /* XXX should be: #if INTS_ARE_SHARED */
#ifdef ELSA_QS1ISA
		if(sc->sc_cardtyp != CARD_TYPEP_ELSAQS1ISA)
		{
#endif		
			if((was_hscx_irq == 0) && (was_isac_irq == 0))
				DBGL1(L1_ERROR, "isicintr", ("WARNING: unit %d, No IRQ from HSCX/ISAC!\n", sc->sc_unit));
#ifdef ELSA_QS1ISA
		}
#endif	
#endif /* !AMIGA && !ATARI */

#endif /* NOTDEF */
			
		HSCX_WRITE(0, H_MASK, 0xff);
		ISAC_WRITE(I_MASK, 0xff);
		HSCX_WRITE(1, H_MASK, 0xff);
	
#ifdef ELSA_QS1ISA
		DELAY(80);
		
		if(sc->sc_cardtyp == CARD_TYPEP_ELSAQS1ISA)
		if (sc->clearirq)
		{
			sc->clearirq(sc);
		}
#else
		DELAY(100);
#endif	
	
		HSCX_WRITE(0, H_MASK, HSCX_A_IMASK);
		ISAC_WRITE(I_MASK, ISAC_IMASK);
		HSCX_WRITE(1, H_MASK, HSCX_B_IMASK);
#ifndef __FreeBSD__
		return(was_hscx_irq || was_isac_irq);
#endif
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
#ifdef NOTDEF
		if(was_ipac_irq == 0)
			DBGL1(L1_ERROR, "isicintr", ("WARNING: unit %d, No IRQ from IPAC!\n", sc->sc_unit));
#endif
		IPAC_WRITE(IPAC_MASK, 0xff);
		DELAY(50);
		IPAC_WRITE(IPAC_MASK, 0xc0);

#ifndef __FreeBSD__
		return(was_ipac_irq);
#endif
	}		
}

/*---------------------------------------------------------------------------*
 *	isic_settrace
 *---------------------------------------------------------------------------*/
void
isic_settrace(int unit, int val)
{
#ifdef __FreeBSD__
	struct isic_softc *sc = &isic_sc[unit];
#else
	struct isic_softc *sc = isic_find_sc(unit);
#endif
	sc->sc_trace = val;
}

/*---------------------------------------------------------------------------*
 *	isic_gettrace
 *---------------------------------------------------------------------------*/
int
isic_gettrace(int unit)
{
#ifdef __FreeBSD__
	struct isic_softc *sc = &isic_sc[unit];
#else
	struct isic_softc *sc = isic_find_sc(unit);
#endif
	return(sc->sc_trace);
}

/*---------------------------------------------------------------------------*
 *	isic_recovery - try to recover from irq lockup
 *---------------------------------------------------------------------------*/
void
isic_recover(struct isic_softc *sc)
{
	u_char byte;
	
	/* get hscx irq status from hscx b ista */

	byte = HSCX_READ(HSCX_CH_B, H_ISTA);

	DBGL1(L1_ERROR, "isic_recover", ("HSCX B: ISTA = 0x%x\n", byte));

	if(byte & HSCX_ISTA_ICA)
		DBGL1(L1_ERROR, "isic_recover", ("HSCX A: ISTA = 0x%x\n", (u_char)HSCX_READ(HSCX_CH_A, H_ISTA)));

	if(byte & HSCX_ISTA_EXB)
		DBGL1(L1_ERROR, "isic_recover", ("HSCX B: EXIR = 0x%x\n", (u_char)HSCX_READ(HSCX_CH_B, H_EXIR)));

	if(byte & HSCX_ISTA_EXA)
		DBGL1(L1_ERROR, "isic_recover", ("HSCX A: EXIR = 0x%x\n", (u_char)HSCX_READ(HSCX_CH_A, H_EXIR)));

	/* get isac irq status */

	byte = ISAC_READ(I_ISTA);

	DBGL1(L1_ERROR, "isic_recover", ("  ISAC: ISTA = 0x%x\n", byte));
	
	if(byte & ISAC_ISTA_EXI)
		DBGL1(L1_ERROR, "isic_recover", ("  ISAC: EXIR = 0x%x\n", (u_char)ISAC_READ(I_EXIR)));

	if(byte & ISAC_ISTA_CISQ)
	{
		byte = ISAC_READ(I_CIRR);
	
		DBGL1(L1_ERROR, "isic_recover", ("  ISAC: CISQ = 0x%x\n", byte));
		
		if(byte & ISAC_CIRR_SQC)
			DBGL1(L1_ERROR, "isic_recover", ("  ISAC: SQRR = 0x%x\n", (u_char)ISAC_READ(I_SQRR)));
	}

	DBGL1(L1_ERROR, "isic_recover", ("HSCX B: IMASK = 0x%x\n", HSCX_B_IMASK));
	DBGL1(L1_ERROR, "isic_recover", ("HSCX A: IMASK = 0x%x\n", HSCX_A_IMASK));

	HSCX_WRITE(0, H_MASK, 0xff);
	HSCX_WRITE(1, H_MASK, 0xff);
	DELAY(100);	
	HSCX_WRITE(0, H_MASK, HSCX_A_IMASK);
	HSCX_WRITE(1, H_MASK, HSCX_B_IMASK);
	DELAY(100);

	DBGL1(L1_ERROR, "isic_recover", ("  ISAC: IMASK = 0x%x\n", ISAC_IMASK));

	ISAC_WRITE(I_MASK, 0xff);	
	DELAY(100);
	ISAC_WRITE(I_MASK, ISAC_IMASK);
}

#endif /* NISIC > 0 */

