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
 *	i4b_isic_isa.c - ISA bus interface
 *	==================================
 *
 * $FreeBSD$ 
 *
 *      last edit-date: [Mon Jul 26 10:59:51 1999]
 *
 *---------------------------------------------------------------------------*/

#ifdef __FreeBSD__
#include "isic.h"
#include "opt_i4b.h"
#elif defined(__bsdi__)
#include "isic.h"
#else
#define NISIC 1
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
#elif defined(__bsdi__)
#include <sys/device.h>
#include <i386/isa/isavar.h>
#else
#include <sys/device.h>
#if defined(__NetBSD__) && defined(amiga)
#include <machine/bus.h>
#else
#include <dev/isa/isavar.h>
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

#ifdef __FreeBSD__

#if !(defined(__FreeBSD_version)) || (defined(__FreeBSD_version) && __FreeBSD_version >= 300006)
void isicintr ( int unit );
#endif

void isicintr_sc(struct isic_softc *sc);

static int isicprobe(struct isa_device *dev);
int isicattach(struct isa_device *dev);

struct isa_driver isicdriver = {
	isicprobe,
	isicattach,
	"isic",
	0
};

int next_isic_unit = 0;
struct isic_softc isic_sc[ISIC_MAXUNIT];

#elif defined(__bsdi__)
	/* XXX */
#else

#ifdef NetBSD1_3
#if NetBSD1_3 < 2
struct cfdriver isic_cd = {
	NULL, "isic", DV_DULL
};
#endif
#endif

#if defined (__OpenBSD__)
struct cfdriver isic_cd = {
	NULL, "isic", DV_DULL
};
#endif

#endif

/* parameter and format for message producing e.g. "isic0: " */

#ifdef __FreeBSD__
#define	ISIC_FMT	"isic%d: "
#define	ISIC_PARM	dev->id_unit
#define	TERMFMT	" "
#else
#define	ISIC_FMT	"%s: "
#define	ISIC_PARM	sc->sc_dev.dv_xname
#define	TERMFMT	"\n"
#endif

extern void isic_settrace(int unit, int val);		/*XXX*/
extern int isic_gettrace(int unit);			/*XXX*/

#ifdef __FreeBSD__
/*---------------------------------------------------------------------------*
 *	isic - non-pnp device driver probe routine
 *---------------------------------------------------------------------------*/
static int
isicprobe(struct isa_device *dev)
{
	int ret = 0;

	if(dev->id_unit != next_isic_unit)
	{
		printf("isicprobe: Error: new unit (%d) != next_isic_unit (%d)!\n", dev->id_unit, next_isic_unit);
		return(0);
	}
		
	switch(dev->id_flags)
	{
#ifdef TEL_S0_8
		case FLAG_TELES_S0_8:
			ret = isic_probe_s08(dev);
			break;
#endif

#ifdef TEL_S0_16
		case FLAG_TELES_S0_16:
			ret = isic_probe_s016(dev);
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

#ifdef ELSA_PCC16
		case FLAG_ELSA_PCC16:
			ret = isic_probe_Eqs1pi(dev, 0);
			break;
#endif

		default:
			break;
	}
	return(ret);
}
#elif defined(__bsdi__)
/*---------------------------------------------------------------------------*
 *	isic - pnp device driver probe routine
 *---------------------------------------------------------------------------*/
int
isapnp_isicmatch(struct device *parent, struct cfdata *cf, struct isa_attach_args *ia)
{
#ifdef DYNALINK
	if (isapnp_match_dynalink(parent, cf, ia))
		return 1;
#endif
	return 0;
}
/*---------------------------------------------------------------------------*
 *	isic - non-pnp device driver probe routine
 *---------------------------------------------------------------------------*/
int
isa_isicmatch(struct device *parent, struct cfdata *cf, struct isa_attach_args *ia)
{
	int ret = 0;

	switch(cf->cf_flags)
	{
#ifdef TEL_S0_8
		case FLAG_TELES_S0_8:
			ret = isic_probe_s08(parent, cf, ia);
			break;
#endif

#ifdef TEL_S0_16
		case FLAG_TELES_S0_16:
			ret = isic_probe_s016(parent, cf, ia);
			break;
#endif

#ifdef TEL_S0_16_3
		case FLAG_TELES_S0_163:
			ret = isic_probe_s0163(parent, cf, ia);		
			break;
#endif

#ifdef AVM_A1
		case FLAG_AVM_A1:
			ret = isic_probe_avma1(parent, cf, ia);
			break;
#endif

#ifdef USR_STI
		case FLAG_USR_ISDN_TA_INT:
			ret = isic_probe_usrtai(parent, cf, ia);		
			break;
#endif

#ifdef ITKIX1
		case FLAG_ITK_IX1:
			ret = isic_probe_itkix1(parent, cf, ia);
			break;
#endif

#ifdef ELSA_PCC16
                case FLAG_ELSA_PCC16:
			ret = isic_probe_Eqs1pi(dev, 0);
			break;
#endif

		default:
			break;
	}
	return(ret);
}

#else

/*---------------------------------------------------------------------------*
 *	isic - device driver probe routine, dummy for NetBSD/OpenBSD
 *---------------------------------------------------------------------------*/
int
isicprobe(struct isic_attach_args *args)
{
	return 1;
}

#endif /* __FreeBSD__ */

#ifdef __FreeBSD__

/*---------------------------------------------------------------------------*
 *	isic - non-pnp device driver attach routine
 *---------------------------------------------------------------------------*/
int
isicattach(struct isa_device *dev)
{
	return(isic_realattach(dev, 0));
}

/*---------------------------------------------------------------------------*
 *	isic - non-pnp and pnp device driver attach routine
 *---------------------------------------------------------------------------*/
int
isic_realattach(struct isa_device *dev, unsigned int iobase2)

#elif defined(__bsdi__)

/*---------------------------------------------------------------------------*
 *	isic - non-pnp device driver attach routine
 *---------------------------------------------------------------------------*/
int
isa_isicattach(struct device *parent, struct device *self, struct isa_attach_args *ia)

#else /* ! __FreeBSD__ */

int
isicattach(int flags, struct isic_softc *sc)

#endif /* __FreeBSD__ */
{
	int ret = 0;
	char *drvid;

#ifdef __FreeBSD__

	struct isic_softc *sc = &isic_sc[dev->id_unit];
#define	PARM	dev
#define	PARM2	dev, iobase2
#define	FLAGS	dev->id_flags

#elif defined(__bsdi__)

	struct isic_softc *sc = (struct isic_softc *)self;
#define	PARM	parent, self, ia
#define	PARM2	parent, self, ia
#define	FLAGS	sc->sc_flags

#else

#define PARM	sc
#define PARM2	sc
#define	FLAGS	flags

#endif /* __FreeBSD__ */

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

	/* done in bus specific attach code for other OS */

#ifdef __FreeBSD__
	if(dev->id_unit != next_isic_unit)
	{
/*XXX*/		printf("isicattach: Error: new unit (%d) != next_isic_unit (%d)!\n", dev->id_unit, next_isic_unit);
		return(0);
	}

	sc->sc_unit = dev->id_unit;
#else
	isic_sc[sc->sc_unit] = sc;
#endif
	
	/* card dependent setup */
	switch(FLAGS)
	{
#ifdef DYNALINK
		case FLAG_DYNALINK:
			ret = isic_attach_Dyn(PARM2);
			break;
#endif

#ifdef TEL_S0_8
		case FLAG_TELES_S0_8:
			ret = isic_attach_s08(PARM);
			break;
#endif

#ifdef TEL_S0_16
		case FLAG_TELES_S0_16:
			ret = isic_attach_s016(PARM);
			break;
#endif

#ifdef TEL_S0_16_3
		case FLAG_TELES_S0_163:
			ret = isic_attach_s0163(PARM);		
			break;
#endif

#ifdef AVM_A1
		case FLAG_AVM_A1:
			ret = isic_attach_avma1(PARM);
			break;
#endif

#ifdef USR_STI
		case FLAG_USR_ISDN_TA_INT:
			ret = isic_attach_usrtai(PARM);		
			break;
#endif

#ifdef ITKIX1
		case FLAG_ITK_IX1:
			ret = isic_attach_itkix1(PARM);
			break;
#endif

#ifdef ELSA_PCC16
		case FLAG_ELSA_PCC16:
			ret = isic_attach_Eqs1pi(dev, 0);
			break;
#endif

#ifdef amiga
		case FLAG_BLMASTER:
			ret = 1; /* full detection was done in caller */
			break;
#endif

/* ======================================================================
 * Only P&P cards follow below!!!
 */

#ifdef __FreeBSD__		/* we've already splitted all non-ISA stuff
				   out of this ISA specific part for the other
				   OS */

#ifdef AVM_A1_PCMCIA
		case FLAG_AVM_A1_PCMCIA:
                      ret = isic_attach_fritzpcmcia(PARM);
			break;
#endif

#ifdef TEL_S0_16_3_P
		case FLAG_TELES_S0_163_PnP:
			ret = isic_attach_s0163P(PARM2);
			break;
#endif

#ifdef CRTX_S0_P
		case FLAG_CREATIX_S0_PnP:
			ret = isic_attach_Cs0P(PARM2);		
			break;
#endif

#ifdef DRN_NGO
		case FLAG_DRN_NGO:
			ret = isic_attach_drnngo(PARM2);		
			break;
#endif

#ifdef SEDLBAUER
		case FLAG_SWS:
			ret = isic_attach_sws(PARM);
			break;
#endif

#ifdef ELSA_QS1ISA
		case FLAG_ELSA_QS1P_ISA:
			ret = isic_attach_Eqs1pi(PARM2);
			break;
#endif

#ifdef AVM_PNP
		case FLAG_AVM_PNP:
			ret = isic_attach_avm_pnp(PARM2);
			ret = 0;
			break;
#endif

#ifdef SIEMENS_ISURF2
		case FLAG_SIEMENS_ISURF2:
			ret = isic_attach_siemens_isurf(PARM2);
			break;
#endif

#ifdef ASUSCOM_IPAC
		case FLAG_ASUSCOM_IPAC:
			ret = isic_attach_asi(PARM2);
			break;
#endif

#endif /* __FreeBSD__ / P&P specific part */

		default:
			break;
	}

	if(ret == 0)
		return(0);
		
	if(sc->sc_ipac)
	{
		ret = IPAC_READ(IPAC_ID);

		if(ret != IPAC_V11)
		{
			printf("isic%d: Error, IPAC version %d unknown!\n",
					sc->sc_unit, ret);
			return(0);
		}
	}
	else
	{

		sc->sc_isac_version = 0;
	sc->sc_hscx_version = 0;

	if(sc->sc_ipac)
		{
		ret = IPAC_READ(IPAC_ID);
	
		switch(ret)
		{
			case 0x01:
				printf("isic%d: IPAC PSB2115 Version 1.1\n", sc->sc_unit);
				break;
	
			default:
				printf("isic%d: Error, IPAC version %d unknown!\n",
					sc->sc_unit, ret);
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
				printf(ISIC_FMT "Error, ISAC version %d unknown!\n",
				ISIC_PARM, sc->sc_isac_version);
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
				break;
				
			default:
				printf(ISIC_FMT "Error, HSCX version %d unknown!\n",
					ISIC_PARM, sc->sc_hscx_version);
				return(0);
				break;
		}
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
	
	/* announce manufacturer and card type */
	
	switch(FLAGS)
	{
		case FLAG_TELES_S0_8:
			drvid = "Teles S0/8 or Niccy 1008";
			break;

		case FLAG_TELES_S0_16:
			drvid = "Teles S0/16, Creatix ISDN S0-16 or Niccy 1016";
			break;

		case FLAG_TELES_S0_163:
			drvid = "Teles S0/16.3";
			break;

		case FLAG_AVM_A1:
			drvid = "AVM A1 or AVM Fritz!Card";
			break;

		case FLAG_AVM_A1_PCMCIA:
			drvid = "AVM PCMCIA Fritz!Card";
			break;

		case FLAG_TELES_S0_163_PnP:
			drvid = "Teles S0/PnP";
			break;

		case FLAG_CREATIX_S0_PnP:
			drvid = "Creatix ISDN S0-16 P&P";
			break;

		case FLAG_USR_ISDN_TA_INT:
			drvid = "USRobotics Sportster ISDN TA intern";
			break;

		case FLAG_DRN_NGO:
			drvid = "Dr. Neuhaus NICCY Go@";
			break;

		case FLAG_DYNALINK:
			drvid = "Dynalink IS64PH";
			break;

		case FLAG_SWS:
			drvid = "Sedlbauer WinSpeed";
			break;

		case FLAG_BLMASTER:
			/* board announcement was done by caller */
			drvid = (char *)0;
			break;

		case FLAG_ELSA_QS1P_ISA:
			drvid = "ELSA QuickStep 1000pro (ISA)";
			break;

		case FLAG_ITK_IX1:
			drvid = "ITK ix1 micro";
			break;

		case FLAG_ELSA_PCC16:
			drvid = "ELSA PCC-16";
			break;

		case FLAG_ASUSCOM_IPAC:
			drvid = "Asuscom ISDNlink 128K PnP";
			break;

		case FLAG_SIEMENS_ISURF2:
			drvid = "Siemens I-Surf 2.0";
			break;

		default:
			drvid = "ERROR, unknown flag used";
			break;
	}
#ifndef __FreeBSD__
	printf("\n");
#endif
	if (drvid)
		printf(ISIC_FMT "%s\n", ISIC_PARM, drvid);

	/* announce chip versions */
	
	if(sc->sc_ipac)
	{
		printf(ISIC_FMT "IPAC PSB2115 Version 1.1\n", ISIC_PARM);
	}
	else
	{
		if(sc->sc_isac_version >= ISAC_UNKN)
		{
			printf(ISIC_FMT "ISAC Version UNKNOWN (VN=0x%x)" TERMFMT,
					ISIC_PARM,
					sc->sc_isac_version);
			sc->sc_isac_version = ISAC_UNKN;
		}
		else
		{
			printf(ISIC_FMT "ISAC %s (IOM-%c)" TERMFMT,
					ISIC_PARM,
					ISACversion[sc->sc_isac_version],
					sc->sc_bustyp == BUS_TYPE_IOM1 ? '1' : '2');
		}
	
#ifdef __FreeBSD__
		printf("(Addr=0x%lx)\n", (u_long)ISAC_BASE);
#endif
		
		if(sc->sc_hscx_version >= HSCX_UNKN)
		{
			printf(ISIC_FMT "HSCX Version UNKNOWN (VN=0x%x)" TERMFMT,
					ISIC_PARM,
					sc->sc_hscx_version);
			sc->sc_hscx_version = HSCX_UNKN;
		}
		else
		{
			printf(ISIC_FMT "HSCX %s" TERMFMT,
					ISIC_PARM,
					HSCXversion[sc->sc_hscx_version]);
		}
	
#ifdef __FreeBSD__	
		printf("(AddrA=0x%lx, AddrB=0x%lx)\n", (u_long)HSCX_A_BASE, (u_long)HSCX_B_BASE);

#endif /* __FreeBSD__ */
	}

#ifdef __FreeBSD__	
	next_isic_unit++;

#if defined(__FreeBSD_version) && __FreeBSD_version >= 300003

	/* set the interrupt handler - no need to change isa_device.h */
	dev->id_intr = (inthand2_t *)isicintr;

#endif

#endif /* __FreeBSD__ */

	return(1);
#undef PARM
#undef FLAGS
}
	
#endif /* NISIC > 0 */
