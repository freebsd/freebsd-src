/*	$Id: cardbus.c,v 1.1.2.1 1999/02/16 16:44:35 haya Exp $	*/

/*
 * Copyright (c) 1997 and 1998 HAYAKAWA Koichi.  All rights reserved.
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
 *	This product includes software developed by HAYAKAWA Koichi.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/* FreeBSD/newconfig version. UCHIYAMA Yasushi 1999 */
/* $FreeBSD: src/sys/dev/cardbus/cardbus.c,v 1.1 1999/11/18 07:21:50 imp Exp $ */
#define CARDBUS_DEBUG

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/syslog.h>

#include <machine/bus.h>

#include <dev/cardbus/pccardcis.h>

#include <dev/cardbus/cardbusreg.h>
#include <dev/cardbus/cardbusvar.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>

#include <dev/ic/i82365reg.h>
#include <dev/ic/i82365reg.h>

#include <dev/pci/pccbbreg.h>
#include <dev/pci/pccbbvar.h>

#if defined CARDBUS_DEBUG
#define STATIC
#define DPRINTF(a) printf a
#define DDELAY(x) delay((x)*1000*1000)
#else
#define STATIC static
#define DPRINTF(a)
#endif

STATIC void cardbusattach __P((struct device *, struct device *, void *));
STATIC int cardbusmatch __P((struct device *, struct cfdata *, void *));
static int cardbussubmatch __P((struct device *, struct cfdata *, void *));
static int cardbusprint __P((void *, const char *));

static u_int8_t *decode_tuple __P((u_int8_t *));
static int decode_tuples __P((u_int8_t *, int));
static char *tuple_name __P((int));

struct cfattach cardbus_ca = {
    sizeof(struct cardbus_softc), cardbusmatch, cardbusattach
};

STATIC int
cardbusmatch(parent, cf, aux)
    struct device *parent;
    struct cfdata *cf;
    void *aux;
{
    struct cbslot_attach_args *cba = aux;

    /* which slot? */
    if (cf->cbslotcf_slot != CBSLOT_UNK_SLOT &&
	cf->cbslotcf_slot != cba->cba_function) {

	DPRINTF(("cardbusmatch: function differs %d <=> %d\n",
		 cf->cbslotcf_slot, cba->cba_function));

	return 0;
    }

    if (cba->cba_function < 0 || cba->cba_function > 255) {
	return 0;
    }

    return 1;
}

void
cardslot_if_setup (struct cardbus_softc *csc)
{
    csc->sc_if.if_card_attach = cardbus_attach_card;
}

STATIC void
cardbusattach(parent, self, aux)
    struct device *parent;
    struct device *self;
    void *aux;
{
    struct pccbb_softc *psc = (struct pccbb_softc *)parent;
    struct cardbus_softc *sc = (void *)self;
    struct cbslot_attach_args *cba = aux;
    int cdstatus;

    sc->sc_bus = cba->cba_bus;
    sc->sc_device = cba->cba_function;
    sc->sc_intrline = cba->cba_intrline;

    printf(" bus %d device %d\n", sc->sc_bus, sc->sc_device);

    sc->sc_iot = cba->cba_iot;	        /* CardBus I/O space tag */
    sc->sc_memt = cba->cba_memt;	/* CardBus MEM space tag */
    sc->sc_dmat = cba->cba_dmat;	/* DMA tag */

    sc->sc_cc = cba->cba_cc;
    sc->sc_cf = cba->cba_cf;
    cardslot_if_setup (sc);
    cdstatus = 0;

    if ((cdstatus = (sc->sc_cf->cardbus_ctrl)(sc->sc_cc, CARDBUS_CD))) {
	DPRINTF(("cardbusattach: CardBus card found [0x%x]\n", cdstatus));
	psc->sc_cbdev = cardbus_attach_card(sc);
    }
}

/**********************************************************************
* int cardbus_attach_card(struct cardbus_softc *sc)
*    This functions attaches the card on the slot: turns on power,
*    reads and analyses tuple, sets consifuration index.
***********************************************************************/
struct device *
cardbus_attach_card(sc)
    struct cardbus_softc *sc;
{
    struct device *attached_device = NULL;
    cardbus_chipset_tag_t cc;
    cardbus_function_tag_t cf;
    int cdstatus;
    cardbustag_t tag;
    cardbusreg_t id, class, cis_ptr, bhlcr;
    u_int8_t tuple[2048];
    int function, max_func, device;

    cc = sc->sc_cc;
    cf = sc->sc_cf;

    DPRINTF(("cardbus_attach_card: cb%d start\n", sc->sc_dev.dv_unit));

    /* inspect initial voltage */
    if (0 == (cdstatus = (cf->cardbus_ctrl)(cc, CARDBUS_CD))) {
	DPRINTF(("cardbusattach: no CardBus card on cb%d\n", sc->sc_dev.dv_unit));
	return 0;
    }

    if (cdstatus & CARDBUS_3V_CARD) {
	cf->cardbus_power(cc, CARDBUS_VCC_3V);
    }
    (cf->cardbus_ctrl)(cc, CARDBUS_RESET);

    device = 0; /* Only one card can attach cardbus slot */
    function = 0;

    tag = cardbus_make_tag (cc, cf, sc->sc_bus, device, function);

    bhlcr = (cf->cardbus_conf_read)(cc, tag, CARDBUS_BHLC_REG);
    max_func = CARDBUS_HDRTYPE_MULTIFN(bhlcr) ? 8 : 1;

    for (function = 0; function < max_func; function++) {
	if (function)
	    tag = cardbus_make_tag (cc, cf, sc->sc_bus, device, function);

	id = (cf->cardbus_conf_read)(cc, tag, CARDBUS_ID_REG);
	if (CARDBUS_VENDOR(id) == 0xffff || CARDBUS_VENDOR(id) == 0) {
	    cardbus_free_tag (cc, cf, tag);
	    continue;
	}

	class = (cf->cardbus_conf_read)(cc, tag, CARDBUS_CLASS_REG);
	cis_ptr = (cf->cardbus_conf_read)(cc, tag, CARDBUS_CIS_REG);

	DPRINTF(("cardbus_attach_card: Vendor 0x%x, Product 0x%x, CIS 0x%x\n",
		 CARDBUS_VENDOR(id), CARDBUS_PRODUCT(id), cis_ptr));

	bzero(tuple, 2048);

	if (0 == (cis_ptr & CARDBUS_CIS_ASIMASK)) {
	    int i = cis_ptr & CARDBUS_CIS_ADDRMASK;
	    int j = 0;

	    for (; i < 0xff; i += 4) {
		u_int32_t e = (cf->cardbus_conf_read)(cc, tag, i);
		tuple[j] = 0xff & e;
		e >>= 8;
		tuple[j + 1] = 0xff & e;
		e >>= 8;
		tuple[j + 2] = 0xff & e;
		e >>= 8;
		tuple[j + 3] = 0xff & e;
		j += 4;
	    }
	}

	decode_tuples(tuple, 2048);
	if (function == 0) {
	    struct cardbus_attach_args ca;
	    cardbusreg_t intr = cardbus_conf_read(cc, cf, tag, CARDBUS_INTERRUPT_REG);

	    ca.ca_unit = sc->sc_dev.dv_unit;
	    ca.ca_cc = sc->sc_cc;
	    ca.ca_cf = sc->sc_cf;

	    ca.ca_iot = sc->sc_iot;
	    ca.ca_memt = sc->sc_memt;
	    ca.ca_dmat = sc->sc_dmat;

	    ca.ca_tag = tag;
	    ca.ca_device = device;
	    ca.ca_function = function;
	    ca.ca_id = id;
	    ca.ca_class = class;

	    ca.ca_intrline = sc->sc_intrline;

	    attached_device = config_found_sm((void *)sc, &ca, cardbusprint, cardbussubmatch);
	} else {
	    printf ("cardbus_attach_card: XXX Multi-function can't handle. function 0 only.\n");
	}
	cardbus_free_tag (cc, cf, tag);
    }
    if (!attached_device)
	cf->cardbus_power(cc, CARDBUS_VCC_0V);
    return attached_device;
}

static int
cardbussubmatch(parent, cf, aux)
    struct device *parent;
    struct cfdata *cf;
    void *aux;
{
    struct cardbus_attach_args *ca = aux;

    if (cf->cardbuscf_dev != CARDBUS_UNK_DEV &&
	cf->cardbuscf_dev != ca->ca_unit) {
	return 0;
    }
    if (cf->cardbuscf_function != CARDBUS_UNK_FUNCTION &&
	cf->cardbuscf_function != ca->ca_function) {
	return 0;
    }

    return ((*cf->cf_attach->ca_match)(parent, cf, aux));
}

static int
cardbusprint(aux, pnp)
    void *aux;
    const char *pnp;
{
    register struct cardbus_attach_args *ca = aux;
    char devinfo[256];

    if (pnp) {
	printf("vendor 0x%04x id 0x%04x at %s",
	       CARDBUS_VENDOR(ca->ca_id), CARDBUS_PRODUCT(ca->ca_id), pnp);
    }
    printf(" dev %d function %d", ca->ca_device, ca->ca_function);
    return UNCONF;
}

/**********************************************************************
* void *cardbus_intr_establish(cc, cf, irq, level, func, arg)
*   Interrupt handler of pccard.
*  args:
*   cardbus_chipset_tag_t *cc
*   int irq: 
**********************************************************************/
void *
cardbus_intr_establish(cc, cf, irq, level, func, arg)
    cardbus_chipset_tag_t cc;
    cardbus_function_tag_t cf;
    cardbus_intr_handle_t irq;
    int level;
    int (*func) __P((void *));
    void *arg;
{
    DPRINTF(("- cardbus_intr_establish: irq %d\n", irq));

    return (*cf->cardbus_intr_establish)(cc, irq, level, func, arg);
}

/**********************************************************************
* void cardbus_intr_disestablish(cc, cf, handler)
*   Interrupt handler of pccard.
*  args:
*   cardbus_chipset_tag_t *cc
**********************************************************************/
void
cardbus_intr_disestablish(cc, cf, handler)
    cardbus_chipset_tag_t cc;
    cardbus_function_tag_t cf;
    void *handler;
{
    DPRINTF(("- cardbus_intr_disestablish\n"));

    (*cf->cardbus_intr_disestablish)(cc, handler);
    return;
}

/**********************************************************************
* below this line, there are some functions for decoding tuples.
* They should go out from this file.
**********************************************************************/
static int
decode_tuples(tuple, buflen)
    u_int8_t *tuple;
    int buflen;
{
    u_int8_t *tp = tuple;

    if (CISTPL_LINKTARGET != *tuple) {
	DPRINTF(("WRONG TUPLE\n"));
	return 0;
    }

    while (NULL != (tp = decode_tuple(tp))) {
	if (tuple + buflen < tp) {
	    break;
	}
    }
  
    return 1;
}

static u_int8_t *
decode_tuple(tuple)
    u_int8_t *tuple;
{
    u_int8_t type;
    u_int8_t len;
    int i;

    type = tuple[0];
    len = tuple[1] + 2;

    printf("tuple: %s len %d\n", tuple_name(type), len);
    if (CISTPL_END == type) {
	return NULL;
    }

    for (i = 0; i < len; ++i) {
	if (i % 16 == 0) {
	    printf("  0x%2x:", i);
	}
	printf(" %x",tuple[i]);
	if (i % 16 == 15) {
	    printf("\n");
	}
    }
    if (i % 16 != 0) {
	printf("\n");
    }

    return tuple + len;
}

static char *
tuple_name(type)
    int type;
{
    static char *tuple_name_s [] = {
	"TPL_NULL", "TPL_DEVICE", "Reserved", "Reserved", /* 0-3 */
	"CONFIG_CB", "CFTABLE_ENTRY_CB", "Reserved", "BAR", /* 4-7 */
	"Reserved", "Reserved", "Reserved", "Reserved", /* 8-B */
	"Reserved", "Reserved", "Reserved", "Reserved", /* C-F */
	"CHECKSUM", "LONGLINK_A", "LONGLINK_C", "LINKTARGET",	/* 10-13 */
	"NO_LINK", "VERS_1", "ALTSTR", "DEVICE_A",
	"JEDEC_C", "JEDEC_A", "CONFIG", "CFTABLE_ENTRY",
	"DEVICE_OC", "DEVICE_OA", "DEVICE_GEO", "DEVICE_GEO_A",
	"MANFID", "FUNCID", "FUNCE", "SWIL", /* 20-23 */
	"Reserved", "Reserved", "Reserved", "Reserved", /* 24-27 */
	"Reserved", "Reserved", "Reserved", "Reserved", /* 28-2B */
	"Reserved", "Reserved", "Reserved", "Reserved", /* 2C-2F */
	"Reserved", "Reserved", "Reserved", "Reserved", /* 30-33 */
	"Reserved", "Reserved", "Reserved", "Reserved", /* 34-37 */
	"Reserved", "Reserved", "Reserved", "Reserved", /* 38-3B */
	"Reserved", "Reserved", "Reserved", "Reserved", /* 3C-3F */
	"VERS_2", "FORMAT", "GEOMETRY", "BYTEORDER",
	"DATE", "BATTERY", "ORG"
    };
#define NAME_LEN(x) (sizeof x / sizeof(x[0]))

    if (type > 0 && type < NAME_LEN(tuple_name_s)) {
	return tuple_name_s[type];
    } else if (0xff == type) {
	return "END";
    } else {
	return "Reserved";
    }
}
