/*
 *   Copyright (c) 1998 Martin Husemann. All rights reserved.
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
 *	pcmcia_isic.c - pcmcia bus frontend for i4b_isic driver
 *	-------------------------------------------------------
 *
 * $FreeBSD: src/sys/i4b/layer1/pcmcia_isic.c,v 1.5 1999/08/28 00:45:44 peter Exp $ 
 *
 *      last edit-date: [Tue Apr 20 14:09:16 1999]
 *
 *	-mh	original implementation
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

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciadevs.h>

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

#include <i4b/layer1/pcmcia_isic.h>

static int pcmcia_isic_match __P((struct device *, struct cfdata *, void *));
static void pcmcia_isic_attach __P((struct device *, struct device *, void *));
static const struct isic_pcmcia_card_entry * find_matching_card __P((struct pcmcia_attach_args *pa));
static int pcmcia_isicattach __P((struct isic_softc *sc));

struct cfattach pcmcia_isic_ca = {
	sizeof(struct pcmcia_isic_softc), pcmcia_isic_match, pcmcia_isic_attach
};

struct isic_pcmcia_card_entry {
	int32_t vendor;		/* vendor ID */
	int32_t product;	/* product ID */
	char *cis1_info[4];	/* CIS info to match */
	char *name;		/* name of controller */
	int function;		/* expected PCMCIA function type */
	int card_type;		/* card type found */
	isic_pcmcia_attach_func attach;	/* card initialization */
};

static const struct isic_pcmcia_card_entry card_list[] = {

#ifdef AVM_A1_PCMCIA
    {   PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
    	{ "AVM", "ISDN A", NULL, NULL },
        "AVM Fritz!Card", PCMCIA_FUNCTION_NETWORK,
	CARD_TYPEP_PCFRITZ, isic_attach_fritzpcmcia },
#endif

#ifdef ELSA_ISDNMC
    {	PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
    	{ "ELSA GmbH, Aachen", "MicroLink ISDN/MC ", NULL, NULL },
        "ELSA MicroLink ISDN/MC", PCMCIA_FUNCTION_NETWORK,
	CARD_TYPEP_ELSAMLIMC, isic_attach_elsaisdnmc },
    {	PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
    	{ "ELSA AG, Aachen", "MicroLink ISDN/MC ", NULL, NULL },
        "ELSA MicroLink ISDN/MC", PCMCIA_FUNCTION_NETWORK,
	CARD_TYPEP_ELSAMLIMC, isic_attach_elsaisdnmc },
#endif

#ifdef ELSA_MCALL
    {	0x105, 0x410a,
        { "ELSA", "MicroLink MC all", NULL, NULL },
        "ELSA MicroLink MCall", PCMCIA_FUNCTION_NETWORK,
	CARD_TYPEP_ELSAMLMCALL, isic_attach_elsamcall },
#endif

};
#define	NUM_MATCH_ENTRIES	(sizeof(card_list)/sizeof(card_list[0]))

static const struct isic_pcmcia_card_entry *
find_matching_card(pa)
	struct pcmcia_attach_args *pa;
{
	int i, j;

	for (i = 0; i < NUM_MATCH_ENTRIES; i++) {
		if (card_list[i].vendor != PCMCIA_VENDOR_INVALID && pa->card->manufacturer != card_list[i].vendor)
			continue;
		if (card_list[i].product != PCMCIA_PRODUCT_INVALID && pa->card->product != card_list[i].product)
				continue;
		if (pa->pf->function != card_list[i].function)
			continue;
		for (j = 0; j < 4; j++) {
			if (card_list[i].cis1_info[j] == NULL)
				continue;	/* wildcard */
			if (pa->card->cis1_info[j] == NULL)
				break;		/* not available */
			if (strcmp(pa->card->cis1_info[j], card_list[i].cis1_info[j]) != 0)
				break;		/* mismatch */
		}
		if (j >= 4)
			break;
	}
	if (i >= NUM_MATCH_ENTRIES)
		return NULL;

	return &card_list[i];
}

/*
 * Match card
 */
static int
pcmcia_isic_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct pcmcia_attach_args *pa = aux;

	if (!find_matching_card(pa))
		return 0;

	return 1;
}

/*
 * Attach the card
 */
static void
pcmcia_isic_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pcmcia_isic_softc *psc = (void*) self;
	struct isic_softc *sc = &psc->sc_isic;
	struct pcmcia_attach_args *pa = aux;
	struct pcmcia_config_entry *cfe;
	const struct isic_pcmcia_card_entry * cde;

	/* Which card is it? */
	cde = find_matching_card(pa);
	if (cde == NULL) return; /* oops - not found?!? */

	psc->sc_pf = pa->pf;
	cfe = pa->pf->cfe_head.sqh_first;

	/* Enable the card */
	pcmcia_function_init(pa->pf, cfe);
	pcmcia_function_enable(pa->pf);

	if (!cde->attach(psc, cfe, pa))
		return;		/* Ooops ? */

	sc->sc_unit = sc->sc_dev.dv_unit;

	/* Announce card name */
	printf(": %s\n", cde->name);

	/* MI initilization */
	pcmcia_isicattach(sc);

	/* setup interrupt */
	psc->sc_ih = pcmcia_intr_establish(pa->pf, IPL_NET, isicintr, sc);
}

/*---------------------------------------------------------------------------*
 *	card independend attach for pcmicia cards
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

int
pcmcia_isicattach(struct isic_softc *sc)
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

	isic_sc[sc->sc_unit] = sc;		
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

	return(1);
}

