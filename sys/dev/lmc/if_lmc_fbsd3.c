/*-
 * Copyright (c) 1994-1997 Matt Thomas (matt@3am-software.com)
 * Copyright (c) LAN Media Corporation 1998, 1999.
 * Copyright (c) 2000 Stephen Kiernan (sk-ports@vegamuse.org)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 *
 *	$Id: if_lmc_fbsd.c,v 1.3 1999/01/12 13:27:42 explorer Exp $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * This file is INCLUDED (gross, I know, but...)
 */

#define	PCI_CONF_WRITE(r, v)	pci_conf_write(config_id, (r), (v))
#define	PCI_CONF_READ(r)	pci_conf_read(config_id, (r))
#define	PCI_GETBUSDEVINFO(sc) (sc)->lmc_pci_busno = (config_id->bus), \
			      (sc)->lmc_pci_devno = (config_id->slot) 

#ifndef COMPAT_OLDPCI
#error "The lmc device requires the old pci compatibility shims"
#endif

#if 0
static void lmc_shutdown(int howto, void * arg);
#endif

#if defined(LMC_DEVCONF)
static int
lmc_pci_shutdown(struct kern_devconf * const kdc, int force)
{
    if (kdc->kdc_unit < LMC_MAX_DEVICES) {
	lmc_softc_t * const sc = LMC_UNIT_TO_SOFTC(kdc->kdc_unit);
	if (sc != NULL)
	    lmc_shutdown(0, sc);
    }
    (void) dev_detach(kdc);
    return 0;
}
#endif

static const char*
lmc_pci_probe(pcici_t config_id, pcidi_t device_id)
{
	u_int32_t id;

	/*
	 * check first for the DEC chip we expect to find.  We expect
	 * 21140A, pass 2.2 or higher.
	 */
	if (PCI_VENDORID(device_id) != DEC_VENDORID)
		return NULL;
	if (PCI_CHIPID(device_id) != CHIPID_21140)
		return NULL;
	id = pci_conf_read(config_id, PCI_CFRV) & 0xff;
	if (id < 0x22)
		return NULL;

	/*
	 * Next, check the subsystem ID and see if it matches what we
	 * expect.
	 */
	id = pci_conf_read(config_id, PCI_SSID);
	if (PCI_VENDORID(id) != PCI_VENDOR_LMC)
		return NULL;
	if (PCI_CHIPID(id) == PCI_PRODUCT_LMC_HSSI) {
		return "Lan Media Corporation HSSI";
	}
	if (PCI_CHIPID(id) == PCI_PRODUCT_LMC_DS3) {
		return "Lan Media Corporation DS3";
	}
	if (PCI_CHIPID(id) == PCI_PRODUCT_LMC_SSI) {
		return "Lan Media Corporation SSI";
	}
	if (PCI_CHIPID(id) == PCI_PRODUCT_LMC_T1) {
		return "Lan Media Coporation T1";
	}

	return NULL;
}

static void  lmc_pci_attach(pcici_t config_id, int unit);
static u_long lmc_pci_count;

struct pci_device lmcdevice = {
	"lmc",
	lmc_pci_probe,
	lmc_pci_attach,
	&lmc_pci_count,
#if defined(LMC_DEVCONF)
	lmc_pci_shutdown,
#endif
};

#ifdef COMPAT_PCI_DRIVER
COMPAT_PCI_DRIVER(ti, lmcdevice);
#else
DATA_SET(pcidevice_set, lmcdevice);
#endif /* COMPAT_PCI_DRIVER */

static void
lmc_pci_attach(pcici_t config_id, int unit)
{
	lmc_softc_t *sc;
	int retval;
	u_int32_t revinfo, cfdainfo, id, ssid;
#if !defined(LMC_IOMAPPED)
	vm_offset_t pa_csrs;
#endif
	unsigned csroffset = LMC_PCI_CSROFFSET;
	unsigned csrsize = LMC_PCI_CSRSIZE;
	lmc_csrptr_t csr_base;
	lmc_spl_t s;

	if (unit >= LMC_MAX_DEVICES) {
		printf("lmc%d", unit);
		printf(": not configured; limit of %d reached or exceeded\n",
		       LMC_MAX_DEVICES);
		return;
	}

	/*
	 * allocate memory for the softc
	 */
	sc = (lmc_softc_t *) malloc(sizeof(*sc), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (sc == NULL)
		return;

	revinfo  = PCI_CONF_READ(PCI_CFRV) & 0xFF;
	id       = PCI_CONF_READ(PCI_CFID);
	cfdainfo = PCI_CONF_READ(PCI_CFDA);
	ssid = pci_conf_read(config_id, PCI_SSID);
	switch (PCI_CHIPID(ssid)) {
	case PCI_PRODUCT_LMC_HSSI:
		sc->lmc_media = &lmc_hssi_media;
		break;
	case PCI_PRODUCT_LMC_DS3:
		sc->lmc_media = &lmc_ds3_media;
		break;
	case PCI_PRODUCT_LMC_SSI:
		sc->lmc_media = &lmc_ssi_media;
		break;
	case PCI_PRODUCT_LMC_T1:
		sc->lmc_media = &lmc_t1_media;
		break;
	}

	/*
	 * allocate memory for the device descriptors
	 */
	sc->lmc_rxdescs = (tulip_desc_t *)malloc(sizeof(tulip_desc_t) * LMC_RXDESCS, M_DEVBUF, M_NOWAIT);
	sc->lmc_txdescs = (tulip_desc_t *)malloc(sizeof(tulip_desc_t) * LMC_TXDESCS, M_DEVBUF, M_NOWAIT);
	if (sc->lmc_rxdescs == NULL || sc->lmc_txdescs == NULL) {
		if (sc->lmc_rxdescs)
			free((caddr_t) sc->lmc_rxdescs, M_DEVBUF);
		if (sc->lmc_txdescs)
			free((caddr_t) sc->lmc_txdescs, M_DEVBUF);
		free((caddr_t) sc, M_DEVBUF);
		return;
	}

	PCI_GETBUSDEVINFO(sc);

	sc->lmc_chipid = LMC_21140A;
	sc->lmc_features |= LMC_HAVE_STOREFWD;
	if (sc->lmc_chipid == LMC_21140A && revinfo <= 0x22)
		sc->lmc_features |= LMC_HAVE_RXBADOVRFLW;

	if (cfdainfo & (TULIP_CFDA_SLEEP | TULIP_CFDA_SNOOZE)) {
		cfdainfo &= ~(TULIP_CFDA_SLEEP | TULIP_CFDA_SNOOZE);
		PCI_CONF_WRITE(PCI_CFDA, cfdainfo);
		DELAY(11 * 1000);
	}

	sc->lmc_unit = unit;
	sc->lmc_name = "lmc";
	sc->lmc_revinfo = revinfo;
#if defined(LMC_IOMAPPED)
	retval = pci_map_port(config_id, PCI_CBIO, &csr_base);
#else
	retval = pci_map_mem(config_id, PCI_CBMA, (vm_offset_t *) &csr_base,
			     &pa_csrs);
#endif

	if (!retval) {
		free((caddr_t) sc->lmc_rxdescs, M_DEVBUF);
		free((caddr_t) sc->lmc_txdescs, M_DEVBUF);
		free((caddr_t) sc, M_DEVBUF);
		return;
	}
	tulips[unit] = sc;

	lmc_initcsrs(sc, csr_base + csroffset, csrsize);
	lmc_initring(sc, &sc->lmc_rxinfo, sc->lmc_rxdescs,
		       LMC_RXDESCS);
	lmc_initring(sc, &sc->lmc_txinfo, sc->lmc_txdescs,
		       LMC_TXDESCS);

	lmc_gpio_mkinput(sc, 0xff);
	sc->lmc_gpio = 0;  /* drive no signals yet */

	sc->lmc_media->defaults(sc);

	sc->lmc_media->set_link_status(sc, 0); /* down */

	/*
	 * Make sure there won't be any interrupts or such...
	 */
	LMC_CSR_WRITE(sc, csr_busmode, TULIP_BUSMODE_SWRESET);
	/*
	 * Wait 10 microseconds (actually 50 PCI cycles but at 
	 * 33MHz that comes to two microseconds but wait a
	 * bit longer anyways)
	 */
	DELAY(100);

	switch (sc->ictl.cardtype) {
	case LMC_CARDTYPE_HSSI:
		printf(LMC_PRINTF_FMT ": HSSI, ", LMC_PRINTF_ARGS);
		break;
	case LMC_CARDTYPE_DS3:
		printf(LMC_PRINTF_FMT ": DS3, ", LMC_PRINTF_ARGS);
		break;
	case LMC_CARDTYPE_SSI:
		printf(LMC_PRINTF_FMT ": SSI, ", LMC_PRINTF_ARGS);
		break;
	}

	lmc_read_macaddr(sc);
	printf("lmc%d: pass %d.%d, serial " LMC_EADDR_FMT "\n", unit,
	       (sc->lmc_revinfo & 0xF0) >> 4, sc->lmc_revinfo & 0x0F,
	       LMC_EADDR_ARGS(sc->lmc_enaddr));

	if (!pci_map_int (config_id, lmc_intr_normal, (void*) sc, &net_imask)) {
		printf(LMC_PRINTF_FMT ": couldn't map interrupt\n",
		       LMC_PRINTF_ARGS);
		return;
	}

#if 0
#if !defined(LMC_DEVCONF)
	at_shutdown(lmc_shutdown, sc, SHUTDOWN_POST_SYNC);
#endif
#endif

	s = LMC_RAISESPL();
	lmc_dec_reset(sc);
	lmc_reset(sc);
	lmc_attach(sc);
	LMC_RESTORESPL(s);
}

#if 0
static void
lmc_shutdown(int howto, void * arg)
{
	lmc_softc_t * const sc = arg;
	LMC_CSR_WRITE(sc, csr_busmode, TULIP_BUSMODE_SWRESET);
	DELAY(10);

	sc->lmc_miireg16 = 0;  /* deassert ready, and all others too */
printf("lmc: 5\n");
	lmc_led_on(sc, LMC_MII16_LED_ALL);
}
#endif
