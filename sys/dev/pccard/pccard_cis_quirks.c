/*	$NetBSD: pcmcia_cis_quirks.c,v 1.6 2000/04/12 21:07:55 scw Exp $ */
/* $FreeBSD$ */

#define	PCCARDDEBUG

/*
 * Copyright (c) 1998 Marc Horowitz.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Marc Horowitz.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/types.h>

#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <dev/pccard/pccarddevs.h>
#include <dev/pccard/pccardreg.h>
#include <dev/pccard/pccardvar.h>

/* There are cards out there whose CIS flat-out lies.  This file
   contains struct pccard_function chains for those devices. */

/* these structures are just static templates which are then copied
   into "live" allocated structures */

struct pccard_function pccard_3cxem556_func0 = {
	0,			/* function number */
	PCCARD_FUNCTION_NETWORK,
	0x07,			/* last cfe number */
	0x800,			/* ccr_base */
	0x63,			/* ccr_mask */
};

struct pccard_config_entry pccard_3cxem556_func0_cfe0 = {
	0x07,			/* cfe number */
	PCCARD_CFE_IO8 | PCCARD_CFE_IO16 | PCCARD_CFE_IRQLEVEL,
	PCCARD_IFTYPE_IO,
	1,			/* num_iospace */
	4,			/* iomask */
	{ { 0x0010, 0 } },	/* iospace */
	0xffff,			/* irqmask */
	0,			/* num_memspace */
	{ },			/* memspace */
	0,			/* maxtwins */
};

static struct pccard_function pccard_3cxem556_func1 = {
	1,			/* function number */
	PCCARD_FUNCTION_SERIAL,
	0x27,			/* last cfe number */
	0x900,			/* ccr_base */
	0x63,			/* ccr_mask */
};

static struct pccard_config_entry pccard_3cxem556_func1_cfe0 = {
	0x27,			/* cfe number */
	PCCARD_CFE_IO8 | PCCARD_CFE_IRQLEVEL,
	PCCARD_IFTYPE_IO,
	1,			/* num_iospace */
	3,			/* iomask */
	{ { 0x0008, 0 } },	/* iospace */
	0xffff,			/* irqmask */
	0,			/* num_memspace */
	{ },			/* memspace */
	0,			/* maxtwins */
};

static struct pccard_function pccard_3ccfem556bi_func0 = {
	0,			/* function number */
	PCCARD_FUNCTION_NETWORK,
	0x07,			/* last cfe number */
	0x1000,			/* ccr_base */
	0x267,			/* ccr_mask */
};

static struct pccard_config_entry pccard_3ccfem556bi_func0_cfe0 = {
	0x07,			/* cfe number */
	PCCARD_CFE_IO8 | PCCARD_CFE_IO16 | PCCARD_CFE_IRQLEVEL,
	PCCARD_IFTYPE_IO,
	1,			/* num_iospace */
	5,			/* iomask */
	{ { 0x0020, 0 } },	/* iospace */
	0xffff,			/* irqmask */
	0,			/* num_memspace */
	{ },			/* memspace */
	0,			/* maxtwins */
};

static struct pccard_function pccard_3ccfem556bi_func1 = {
	1,			/* function number */
	PCCARD_FUNCTION_SERIAL,
	0x27,			/* last cfe number */
	0x1100,			/* ccr_base */
	0x277,			/* ccr_mask */
};

static struct pccard_config_entry pccard_3ccfem556bi_func1_cfe0 = {
	0x27,			/* cfe number */
	PCCARD_CFE_IO8 | PCCARD_CFE_IRQLEVEL,
	PCCARD_IFTYPE_IO,
	1,			/* num_iospace */
	3,			/* iomask */
	{ { 0x0008, 0 } },	/* iospace */
	0xffff,			/* irqmask */
	0,			/* num_memspace */
	{ },			/* memspace */
	0,			/* maxtwins */
};

static struct pccard_function pccard_sveclancard_func0 = {
	0,			/* function number */
	PCCARD_FUNCTION_NETWORK,
	0x1,			/* last cfe number */
	0x100,			/* ccr_base */
	0x1,			/* ccr_mask */
};

static struct pccard_config_entry pccard_sveclancard_func0_cfe0 = {
	0x1,			/* cfe number */
	PCCARD_CFE_MWAIT_REQUIRED | PCCARD_CFE_RDYBSY_ACTIVE |
	PCCARD_CFE_WP_ACTIVE | PCCARD_CFE_BVD_ACTIVE | PCCARD_CFE_IO16,
	PCCARD_IFTYPE_IO,
	1,			/* num_iospace */
	5,			/* iomask */
	{ { 0x20, 0x300 } },	/* iospace */
	0xdeb8,			/* irqmask */
	0,			/* num_memspace */
	{ },			/* memspace */
	0,			/* maxtwins */
};

static struct pccard_function pccard_ndc_nd5100_func0 = {
	0,			/* function number */
	PCCARD_FUNCTION_NETWORK,
	0x23,			/* last cfe number */
	0x3f8,			/* ccr_base */
	0x3,			/* ccr_mask */
};

static struct pccard_config_entry pccard_ndc_nd5100_func0_cfe0 = {
	0x20,			/* cfe number */
	PCCARD_CFE_MWAIT_REQUIRED | PCCARD_CFE_IO16 | PCCARD_CFE_IRQLEVEL,
	PCCARD_IFTYPE_IO,
	1,			/* num_iospace */
	5,			/* iomask */
	{ { 0x20, 0x300 } },	/* iospace */
	0xdeb8,			/* irqmask */
	0,			/* num_memspace */
	{ },			/* memspace */
	0,			/* maxtwins */
};

static struct pccard_cis_quirk pccard_cis_quirks[] = {
	{ PCCARD_VENDOR_3COM, PCCARD_PRODUCT_3COM_3CXEM556, PCCARD_CIS_INVALID, 
	  &pccard_3cxem556_func0, &pccard_3cxem556_func0_cfe0 },
	{ PCCARD_VENDOR_3COM, PCCARD_PRODUCT_3COM_3CXEM556, PCCARD_CIS_INVALID,
	  &pccard_3cxem556_func1, &pccard_3cxem556_func1_cfe0 },
	{ PCCARD_VENDOR_3COM, PCCARD_PRODUCT_3COM_3CXEM556INT, PCCARD_CIS_INVALID, 
	  &pccard_3cxem556_func0, &pccard_3cxem556_func0_cfe0 },
	{ PCCARD_VENDOR_3COM, PCCARD_PRODUCT_3COM_3CXEM556INT, PCCARD_CIS_INVALID,
	  &pccard_3cxem556_func1, &pccard_3cxem556_func1_cfe0 },
	{ PCCARD_VENDOR_3COM, PCCARD_PRODUCT_3COM_3CCFEM556BI,
	  PCCARD_CIS_INVALID,
	  &pccard_3ccfem556bi_func0, &pccard_3ccfem556bi_func0_cfe0 },
	{ PCCARD_VENDOR_3COM, PCCARD_PRODUCT_3COM_3CCFEM556BI,
	  PCCARD_CIS_INVALID,
	  &pccard_3ccfem556bi_func1, &pccard_3ccfem556bi_func1_cfe0 },
	{ PCCARD_VENDOR_INVALID, PCCARD_PRODUCT_INVALID, PCCARD_CIS_SVEC_LANCARD,
	  &pccard_sveclancard_func0, &pccard_sveclancard_func0_cfe0 },
	{ PCCARD_VENDOR_INVALID, PCCARD_PRODUCT_INVALID, PCCARD_CIS_NDC_ND5100_E,
	  &pccard_ndc_nd5100_func0, &pccard_ndc_nd5100_func0_cfe0 },
};
	
static int n_pccard_cis_quirks =
	sizeof(pccard_cis_quirks)/sizeof(pccard_cis_quirks[0]);

void pccard_check_cis_quirks(device_t dev)
{
	struct pccard_softc *sc = PCCARD_SOFTC(dev);
	int wiped = 0;
	int i, j;
	struct pccard_function *pf, *pf_next, *pf_last;
	struct pccard_config_entry *cfe, *cfe_next;

	pf = NULL;
	pf_last = NULL;

	for (i=0; i<n_pccard_cis_quirks; i++) {
		if ((sc->card.manufacturer == pccard_cis_quirks[i].manufacturer) &&
			(sc->card.product == pccard_cis_quirks[i].product) &&
			(((sc->card.manufacturer != PCCARD_VENDOR_INVALID) &&
			  (sc->card.product != PCCARD_PRODUCT_INVALID)) ||
			 ((sc->card.manufacturer == PCCARD_VENDOR_INVALID) &&
			  (sc->card.product == PCCARD_PRODUCT_INVALID) &&
			  sc->card.cis1_info[0] &&
			  (strcmp(sc->card.cis1_info[0],
					  pccard_cis_quirks[i].cis1_info[0]) == 0) &&
			  sc->card.cis1_info[1] &&
			  (strcmp(sc->card.cis1_info[1],
					  pccard_cis_quirks[i].cis1_info[1]) == 0)))) {
			if (!wiped) {
				if (pccard_verbose) {
					device_printf(dev, "using CIS quirks for ");
					for (j = 0; j < 4; j++) {
						if (sc->card.cis1_info[j] == NULL)
							break;
						if (j)
							printf(", ");
						printf("%s", sc->card.cis1_info[j]);
					}
					printf("\n");
				}

				for (pf = STAILQ_FIRST(&sc->card.pf_head); pf != NULL;
				     pf = pf_next) {
					for (cfe = STAILQ_FIRST(&pf->cfe_head); cfe != NULL;
					     cfe = cfe_next) {
						cfe_next = STAILQ_NEXT(cfe, cfe_list);
						free(cfe, M_DEVBUF);
					}
					pf_next = STAILQ_NEXT(pf, pf_list);
					free(pf, M_DEVBUF);
				}

				STAILQ_INIT(&sc->card.pf_head);
				wiped = 1;
			}

			if (pf_last == pccard_cis_quirks[i].pf) {
				cfe = malloc(sizeof(*cfe), M_DEVBUF, M_NOWAIT);
				*cfe = *pccard_cis_quirks[i].cfe;

				STAILQ_INSERT_TAIL(&pf->cfe_head, cfe, cfe_list);
			} else {
				pf = malloc(sizeof(*pf), M_DEVBUF, M_NOWAIT);
				*pf = *pccard_cis_quirks[i].pf;
				STAILQ_INIT(&pf->cfe_head);

				cfe = malloc(sizeof(*cfe), M_DEVBUF, M_NOWAIT);
				*cfe = *pccard_cis_quirks[i].cfe;

				STAILQ_INSERT_TAIL(&pf->cfe_head, cfe, cfe_list);
				STAILQ_INSERT_TAIL(&sc->card.pf_head, pf, pf_list);

				pf_last = pccard_cis_quirks[i].pf;
			}
		}
	}
}
