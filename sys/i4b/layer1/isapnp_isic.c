/*
 *   Copyright (c) 1997, 1998 Martin Husemann. All rights reserved.
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
 *	isapnp_isic.c - ISA-P&P bus frontend for i4b_isic driver
 *	--------------------------------------------------------
 *
 * $FreeBSD: src/sys/i4b/layer1/isapnp_isic.c,v 1.5 1999/08/28 00:45:43 peter Exp $ 
 *
 *      last edit-date: [Sun May  2 11:57:08 1999]
 *
 *	-mh	original implementation
 *      -hm     NetBSD patches from Martin 
 *
 *---------------------------------------------------------------------------*/

#include <sys/types.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/systm.h>
#include <sys/malloc.h>

#include <machine/cpu.h>
#include <machine/intr.h>
#include <machine/bus.h>

#include <dev/isa/isavar.h>
#include <dev/isapnp/isapnpreg.h>
#include <dev/isapnp/isapnpvar.h>

#ifdef __FreeBSD__
#include <machine/i4b_ioctl.h>
#include <machine/i4b_trace.h>
#else
#include <i4b/i4b_ioctl.h>
#include <i4b/i4b_trace.h>
#endif

#include <i4b/layer1/i4b_l1.h>
#include <i4b/layer1/i4b_ipac.h>
#include <i4b/layer1/i4b_isac.h>
#include <i4b/layer1/i4b_hscx.h>

#include <i4b/include/i4b_l1l2.h>
#include <i4b/include/i4b_global.h>

#ifdef __BROKEN_INDIRECT_CONFIG
static int isapnp_isic_probe __P((struct device *, void *, void *));
#else
static int isapnp_isic_probe __P((struct device *, struct cfdata *, void *));
#endif
static void isapnp_isic_attach __P((struct device *, struct device *, void *));

struct cfattach isapnp_isic_ca = {
	sizeof(struct isic_softc), isapnp_isic_probe, isapnp_isic_attach
};

typedef void (*allocmaps_func)(struct isapnp_attach_args *ipa, struct isic_softc *sc);
typedef void (*attach_func)(struct isic_softc *sc);

/* map allocators */
static void generic_pnp_mapalloc(struct isapnp_attach_args *ipa, struct isic_softc *sc);
#ifdef DRN_NGO
static void ngo_pnp_mapalloc(struct isapnp_attach_args *ipa, struct isic_softc *sc);
#endif
#if defined(CRTX_S0_P) || defined(TEL_S0_16_3_P)
static void tls_pnp_mapalloc(struct isapnp_attach_args *ipa, struct isic_softc *sc);
#endif

/* card attach functions */
extern void isic_attach_Cs0P __P((struct isic_softc *sc));
extern void isic_attach_Dyn __P((struct isic_softc *sc));
extern void isic_attach_s0163P __P((struct isic_softc *sc));
extern void isic_attach_drnngo __P((struct isic_softc *sc));
extern void isic_attach_sws __P((struct isic_softc *sc));
extern void isic_attach_Eqs1pi __P((struct isic_softc *sc));

struct isapnp_isic_card_desc {
	char *devlogic;			/* ISAPNP logical device ID */
	char *name;			/* Name of the card */
	int card_type;			/* isic card type identifier */
	allocmaps_func allocmaps;	/* map allocator function */
	attach_func attach;		/* card attach and init function */
};
static const struct isapnp_isic_card_desc
isapnp_isic_descriptions[] =
{
#ifdef CRTX_S0_P
	{ "CTX0000", "Creatix ISDN S0-16 P&P", CARD_TYPEP_CS0P,
	  tls_pnp_mapalloc, isic_attach_Cs0P },
#endif
#ifdef TEL_S0_16_3_P
	{ "TAG2110", "Teles S0/PnP", CARD_TYPEP_163P,
	  tls_pnp_mapalloc, isic_attach_s0163P },
#endif
#ifdef DRN_NGO
	{ "SDA0150", "Dr. Neuhaus NICCY GO@", CARD_TYPEP_DRNNGO,
	  ngo_pnp_mapalloc, isic_attach_drnngo },
#endif
#ifdef ELSA_QS1ISA
	{ "ELS0133", "Elsa QuickStep 1000 (ISA)", CARD_TYPEP_ELSAQS1ISA,
	  generic_pnp_mapalloc, isic_attach_Eqs1pi },
#endif
#ifdef SEDLBAUER
	{ "SAG0001", "Sedlbauer WinSpeed", CARD_TYPEP_SWS,
	  generic_pnp_mapalloc, isic_attach_sws },
#endif
#ifdef DYNALINK
	{ "ASU1688", "Dynalink IS64PH", CARD_TYPEP_DYNALINK,
	  generic_pnp_mapalloc, isic_attach_Dyn },
#endif
};
#define	NUM_DESCRIPTIONS	(sizeof(isapnp_isic_descriptions)/sizeof(isapnp_isic_descriptions[0]))

/*
 * Probe card
 */
static int
#ifdef __BROKEN_INDIRECT_CONFIG
isapnp_isic_probe(parent, match, aux)
#else
isapnp_isic_probe(parent, cf, aux)
#endif
	struct device *parent;
#ifdef __BROKEN_INDIRECT_CONFIG
	void *match;
#else
	struct cfdata *cf;
#endif
	void *aux;
{
	struct isapnp_attach_args *ipa = aux;
	const struct isapnp_isic_card_desc *desc = isapnp_isic_descriptions;
	int i;

	for (i = 0; i < NUM_DESCRIPTIONS; i++, desc++)
		if (strcmp(ipa->ipa_devlogic, desc->devlogic) == 0)
	return 1;

	return 0;
}


/*---------------------------------------------------------------------------*
 *	card independend attach for ISA P&P cards
 *---------------------------------------------------------------------------*/

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

static void
isapnp_isic_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
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

	struct isic_softc *sc = (void *)self;
	struct isapnp_attach_args *ipa = aux;
	const struct isapnp_isic_card_desc *desc = isapnp_isic_descriptions;
	int i;

	for (i = 0; i < NUM_DESCRIPTIONS; i++, desc++)
		if (strcmp(ipa->ipa_devlogic, desc->devlogic) == 0)
			break;
	if (i >= NUM_DESCRIPTIONS)
		panic("could not identify isic PnP device");

	/* setup parameters */
	sc->sc_cardtyp = desc->card_type;
	sc->sc_unit = sc->sc_dev.dv_unit;
	sc->sc_irq = ipa->ipa_irq[0].num;
	desc->allocmaps(ipa, sc);

	/* announce card name */
	printf(": %s\n", desc->name);

	/* establish interrupt handler */
	isa_intr_establish(ipa->ipa_ic, ipa->ipa_irq[0].num, IST_EDGE,
		IPL_NET, isicintr, sc);

	/* init card */
	isic_sc[sc->sc_unit] = sc;		
	desc->attach(sc);

	/* announce chip versions */
	sc->sc_isac_version = 0;
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
			return;
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
			return;
			break;
	};

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

	/* init higher protocol layers */
	
	MPH_Status_Ind(sc->sc_unit, STI_ATTACH, sc->sc_cardtyp);	

	/* announce chip versions */
	
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
}

static void
generic_pnp_mapalloc(struct isapnp_attach_args *ipa, struct isic_softc *sc)
{
	sc->sc_num_mappings = 1;	/* most cards have just one mapping */
	MALLOC_MAPS(sc);		/* malloc the maps */
	sc->sc_maps[0].t = ipa->ipa_iot;	/* copy the access handles */
	sc->sc_maps[0].h = ipa->ipa_io[0].h;
	sc->sc_maps[0].size = 0;	/* foreign mapping, leave it alone */
}

#ifdef DRN_NGO
static void
ngo_pnp_mapalloc(struct isapnp_attach_args *ipa, struct isic_softc *sc)
{
	sc->sc_num_mappings = 2;	/* one data, one address mapping */
	MALLOC_MAPS(sc);		/* malloc the maps */
	sc->sc_maps[0].t = ipa->ipa_iot;	/* copy the access handles */
	sc->sc_maps[0].h = ipa->ipa_io[0].h;
	sc->sc_maps[0].size = 0;	/* foreign mapping, leave it alone */
		sc->sc_maps[1].t = ipa->ipa_iot;
		sc->sc_maps[1].h = ipa->ipa_io[1].h;
		sc->sc_maps[1].size = 0;
}
#endif

#if defined(CRTX_S0_P) || defined(TEL_S0_16_3_P)
static void
tls_pnp_mapalloc(struct isapnp_attach_args *ipa, struct isic_softc *sc)
{
	sc->sc_num_mappings = 4;	/* config, isac, 2 * hscx */
	MALLOC_MAPS(sc);		/* malloc the maps */
	sc->sc_maps[0].t = ipa->ipa_iot;	/* copy the access handles */
	sc->sc_maps[0].h = ipa->ipa_io[0].h;
	sc->sc_maps[0].size = 0;	/* foreign mapping, leave it alone */
		sc->sc_maps[1].t = ipa->ipa_iot;
		sc->sc_maps[1].h = ipa->ipa_io[0].h;
		sc->sc_maps[1].size = 0;
		sc->sc_maps[1].offset = - 0x20;
		sc->sc_maps[2].t = ipa->ipa_iot;
		sc->sc_maps[2].offset = - 0x20;
		sc->sc_maps[2].h = ipa->ipa_io[1].h;
		sc->sc_maps[2].size = 0;
		sc->sc_maps[3].t = ipa->ipa_iot;
		sc->sc_maps[3].offset = 0;
		sc->sc_maps[3].h = ipa->ipa_io[1].h;
		sc->sc_maps[3].size = 0;
}
#endif
