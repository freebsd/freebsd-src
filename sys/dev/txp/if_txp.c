/*	$OpenBSD: if_txp.c,v 1.48 2001/06/27 06:34:50 kjc Exp $	*/
/*	$FreeBSD$ */

/*
 * Copyright (c) 2001
 *	Jason L. Wright <jason@thought.net>, Theo de Raadt, and
 *	Aaron Campbell <aaron@monkey.org>.  All rights reserved.
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
 *	This product includes software developed by Jason L. Wright,
 *	Theo de Raadt and Aaron Campbell.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Driver for 3c990 (Typhoon) Ethernet ASIC
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_vlan_var.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#include <machine/in_cksum.h>

#include <net/if_media.h>

#include <net/bpf.h>

#include <vm/vm.h>              /* for vtophys */
#include <vm/pmap.h>            /* for vtophys */
#include <machine/clock.h>	/* for DELAY */
#include <machine/bus_pio.h>
#include <machine/bus_memio.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <pci/pcireg.h>
#include <pci/pcivar.h>

#define TXP_USEIOSPACE
#define __STRICT_ALIGNMENT

#include <dev/txp/if_txpreg.h>
#include <dev/txp/3c990img.h>

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif

/*
 * Various supported device vendors/types and their names.
 */
static struct txp_type txp_devs[] = {
	{ TXP_VENDORID_3COM, TXP_DEVICEID_3CR990_TX_95,
	    "3Com 3cR990-TX-95 Etherlink with 3XP Processor" },
	{ TXP_VENDORID_3COM, TXP_DEVICEID_3CR990_TX_97,
	    "3Com 3cR990-TX-97 Etherlink with 3XP Processor" },
	{ TXP_VENDORID_3COM, TXP_DEVICEID_3CR990B_TXM,
	    "3Com 3cR990B-TXM Etherlink with 3XP Processor" },
	{ TXP_VENDORID_3COM, TXP_DEVICEID_3CR990_SRV_95,
	    "3Com 3cR990-SRV-95 Etherlink Server with 3XP Processor" },
	{ TXP_VENDORID_3COM, TXP_DEVICEID_3CR990_SRV_97,
	    "3Com 3cR990-SRV-97 Etherlink Server with 3XP Processor" },
	{ TXP_VENDORID_3COM, TXP_DEVICEID_3CR990B_SRV,
	    "3Com 3cR990B-SRV Etherlink Server with 3XP Processor" },
	{ 0, 0, NULL }
};

static int txp_probe	__P((device_t));
static int txp_attach	__P((device_t));
static int txp_detach	__P((device_t));
static void txp_intr	__P((void *));
static void txp_tick	__P((void *));
static int txp_shutdown	__P((device_t));
static int txp_ioctl	__P((struct ifnet *, u_long, caddr_t));
static void txp_start	__P((struct ifnet *));
static void txp_stop	__P((struct txp_softc *));
static void txp_init	__P((void *));
static void txp_watchdog	__P((struct ifnet *));

static void txp_release_resources __P((struct txp_softc *));
static int txp_chip_init __P((struct txp_softc *));
static int txp_reset_adapter __P((struct txp_softc *));
static int txp_download_fw __P((struct txp_softc *));
static int txp_download_fw_wait __P((struct txp_softc *));
static int txp_download_fw_section __P((struct txp_softc *,
    struct txp_fw_section_header *, int));
static int txp_alloc_rings __P((struct txp_softc *));
static int txp_rxring_fill __P((struct txp_softc *));
static void txp_rxring_empty __P((struct txp_softc *));
static void txp_set_filter __P((struct txp_softc *));

static int txp_cmd_desc_numfree __P((struct txp_softc *));
static int txp_command __P((struct txp_softc *, u_int16_t, u_int16_t, u_int32_t,
    u_int32_t, u_int16_t *, u_int32_t *, u_int32_t *, int));
static int txp_command2 __P((struct txp_softc *, u_int16_t, u_int16_t,
    u_int32_t, u_int32_t, struct txp_ext_desc *, u_int8_t,
    struct txp_rsp_desc **, int));
static int txp_response __P((struct txp_softc *, u_int32_t, u_int16_t, u_int16_t,
    struct txp_rsp_desc **));
static void txp_rsp_fixup __P((struct txp_softc *, struct txp_rsp_desc *,
    struct txp_rsp_desc *));
static void txp_capabilities __P((struct txp_softc *));

static void txp_ifmedia_sts __P((struct ifnet *, struct ifmediareq *));
static int txp_ifmedia_upd __P((struct ifnet *));
#ifdef TXP_DEBUG
static void txp_show_descriptor __P((void *));
#endif
static void txp_tx_reclaim __P((struct txp_softc *, struct txp_tx_ring *));
static void txp_rxbuf_reclaim __P((struct txp_softc *));
static void txp_rx_reclaim __P((struct txp_softc *, struct txp_rx_ring *));

#ifdef TXP_USEIOSPACE
#define TXP_RES			SYS_RES_IOPORT
#define TXP_RID			TXP_PCI_LOIO
#else
#define TXP_RES			SYS_RES_MEMORY
#define TXP_RID			TXP_PCI_LOMEM
#endif

static device_method_t txp_methods[] = {
        /* Device interface */
	DEVMETHOD(device_probe,		txp_probe),
	DEVMETHOD(device_attach,	txp_attach),
	DEVMETHOD(device_detach,	txp_detach),
	DEVMETHOD(device_shutdown,	txp_shutdown),
	{ 0, 0 }
};

static driver_t txp_driver = {
	"txp",
	txp_methods,
	sizeof(struct txp_softc)
};

static devclass_t txp_devclass;

DRIVER_MODULE(if_txp, pci, txp_driver, txp_devclass, 0, 0);

static int
txp_probe(dev)
	device_t dev;
{
	struct txp_type *t;

	t = txp_devs;

	while(t->txp_name != NULL) {
		if ((pci_get_vendor(dev) == t->txp_vid) &&
		    (pci_get_device(dev) == t->txp_did)) {
			device_set_desc(dev, t->txp_name);
			return(0);
		}
		t++;
	}

	return(ENXIO);
}

static int
txp_attach(dev)
	device_t dev;
{
	struct txp_softc *sc;
	struct ifnet *ifp;
	u_int32_t command;
	u_int16_t p1;
	u_int32_t p2;
	int unit, error = 0, rid;

	sc = device_get_softc(dev);
	unit = device_get_unit(dev);
	sc->sc_dev = dev;
	sc->sc_cold = 1;

	/*
	 * Map control/status registers.
	 */
	command = pci_read_config(dev, PCIR_COMMAND, 4);
	command |= (PCIM_CMD_PORTEN|PCIM_CMD_MEMEN|PCIM_CMD_BUSMASTEREN);
	pci_write_config(dev, PCIR_COMMAND, command, 4);
	command = pci_read_config(dev, PCIR_COMMAND, 4);

#ifdef TXP_USEIOSPACE
	if (!(command & PCIM_CMD_PORTEN)) {
		device_printf(dev, "failed to enable I/O ports!\n");
		error = ENXIO;;
		goto fail;
	}
#else
	if (!(command & PCIM_CMD_MEMEN)) {
		device_printf(dev, "failed to enable memory mapping!\n");
		error = ENXIO;;
		goto fail;
	}
#endif

	rid = TXP_RID;
	sc->sc_res = bus_alloc_resource(dev, TXP_RES, &rid,
	    0, ~0, 1, RF_ACTIVE);

	if (sc->sc_res == NULL) {
		device_printf(dev, "couldn't map ports/memory\n");
		error = ENXIO;
		goto fail;
	}

	sc->sc_bt = rman_get_bustag(sc->sc_res);
	sc->sc_bh = rman_get_bushandle(sc->sc_res);

	/* Allocate interrupt */
	rid = 0;
	sc->sc_irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0, 1,
	    RF_SHAREABLE | RF_ACTIVE);

	if (sc->sc_irq == NULL) {
		device_printf(dev, "couldn't map interrupt\n");
		txp_release_resources(sc);
		error = ENXIO;
		goto fail;
	}

	error = bus_setup_intr(dev, sc->sc_irq, INTR_TYPE_NET,
	    txp_intr, sc, &sc->sc_intrhand);

	if (error) {
		txp_release_resources(sc);
		device_printf(dev, "couldn't set up irq\n");
		goto fail;
	}

	if (txp_chip_init(sc)) {
		txp_release_resources(sc);
		goto fail;
	}

	sc->sc_fwbuf = contigmalloc(32768, M_DEVBUF,
	    M_NOWAIT, 0, 0xffffffff, PAGE_SIZE, 0);
	error = txp_download_fw(sc);
	contigfree(sc->sc_fwbuf, 32768, M_DEVBUF);
	sc->sc_fwbuf = NULL;

	if (error) {
		txp_release_resources(sc);
		goto fail;
	}

	sc->sc_ldata = contigmalloc(sizeof(struct txp_ldata), M_DEVBUF,
	    M_NOWAIT, 0, 0xffffffff, PAGE_SIZE, 0);
	bzero(sc->sc_ldata, sizeof(struct txp_ldata));

	if (txp_alloc_rings(sc)) {
		txp_release_resources(sc);
		goto fail;
	}

	if (txp_command(sc, TXP_CMD_MAX_PKT_SIZE_WRITE, TXP_MAX_PKTLEN, 0, 0,
	    NULL, NULL, NULL, 1)) {
		txp_release_resources(sc);
		goto fail;
	}

	if (txp_command(sc, TXP_CMD_STATION_ADDRESS_READ, 0, 0, 0,
	    &p1, &p2, NULL, 1)) {
		txp_release_resources(sc);
		goto fail;
	}

	txp_set_filter(sc);

	sc->sc_arpcom.ac_enaddr[0] = ((u_int8_t *)&p1)[1];
	sc->sc_arpcom.ac_enaddr[1] = ((u_int8_t *)&p1)[0];
	sc->sc_arpcom.ac_enaddr[2] = ((u_int8_t *)&p2)[3];
	sc->sc_arpcom.ac_enaddr[3] = ((u_int8_t *)&p2)[2];
	sc->sc_arpcom.ac_enaddr[4] = ((u_int8_t *)&p2)[1];
	sc->sc_arpcom.ac_enaddr[5] = ((u_int8_t *)&p2)[0];

	printf("txp%d: Ethernet address %6D\n", unit,
	    sc->sc_arpcom.ac_enaddr, ":");

	sc->sc_cold = 0;

	ifmedia_init(&sc->sc_ifmedia, 0, txp_ifmedia_upd, txp_ifmedia_sts);
	ifmedia_add(&sc->sc_ifmedia, IFM_ETHER|IFM_10_T, 0, NULL);
	ifmedia_add(&sc->sc_ifmedia, IFM_ETHER|IFM_10_T|IFM_HDX, 0, NULL);
	ifmedia_add(&sc->sc_ifmedia, IFM_ETHER|IFM_10_T|IFM_FDX, 0, NULL);
	ifmedia_add(&sc->sc_ifmedia, IFM_ETHER|IFM_100_TX, 0, NULL);
	ifmedia_add(&sc->sc_ifmedia, IFM_ETHER|IFM_100_TX|IFM_HDX, 0, NULL);
	ifmedia_add(&sc->sc_ifmedia, IFM_ETHER|IFM_100_TX|IFM_FDX, 0, NULL);
	ifmedia_add(&sc->sc_ifmedia, IFM_ETHER|IFM_AUTO, 0, NULL);

	sc->sc_xcvr = TXP_XCVR_AUTO;
	txp_command(sc, TXP_CMD_XCVR_SELECT, TXP_XCVR_AUTO, 0, 0,
	    NULL, NULL, NULL, 0);
	ifmedia_set(&sc->sc_ifmedia, IFM_ETHER|IFM_AUTO);

	ifp = &sc->sc_arpcom.ac_if;
	ifp->if_softc = sc;
	ifp->if_unit = unit;
	ifp->if_name = "txp";
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = txp_ioctl;
	ifp->if_output = ether_output;
	ifp->if_start = txp_start;
	ifp->if_watchdog = txp_watchdog;
	ifp->if_init = txp_init;
	ifp->if_baudrate = 100000000;
	ifp->if_snd.ifq_maxlen = TX_ENTRIES;
	ifp->if_hwassist = 0;
	txp_capabilities(sc);

	/*
	 * Attach us everywhere
	 */
	ether_ifattach(ifp, ETHER_BPF_SUPPORTED);
	callout_handle_init(&sc->sc_tick);
	return(0);

fail:
	txp_release_resources(sc);
	return(error);
}

static int
txp_detach(dev)
	device_t dev;
{
	struct txp_softc *sc;
	struct ifnet *ifp;
	int i;

	sc = device_get_softc(dev);
	ifp = &sc->sc_arpcom.ac_if;

	txp_stop(sc);
	txp_shutdown(dev);

	ifmedia_removeall(&sc->sc_ifmedia);
	ether_ifdetach(ifp, ETHER_BPF_SUPPORTED);

	for (i = 0; i < RXBUF_ENTRIES; i++)
		free(sc->sc_rxbufs[i].rb_sd, M_DEVBUF);

	txp_release_resources(sc);

	return(0);
}

static void
txp_release_resources(sc)
	struct txp_softc *sc;
{
	device_t dev;

	dev = sc->sc_dev;

	if (sc->sc_intrhand != NULL)
		bus_teardown_intr(dev, sc->sc_irq, sc->sc_intrhand);

	if (sc->sc_irq != NULL)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->sc_irq);

	if (sc->sc_res != NULL)
		bus_release_resource(dev, TXP_RES, TXP_RID, sc->sc_res);

	if (sc->sc_ldata != NULL)
		contigfree(sc->sc_ldata, sizeof(struct txp_ldata), M_DEVBUF);

	return;
}

static int
txp_chip_init(sc)
	struct txp_softc *sc;
{
	/* disable interrupts */
	WRITE_REG(sc, TXP_IER, 0);
	WRITE_REG(sc, TXP_IMR,
	    TXP_INT_SELF | TXP_INT_PCI_TABORT | TXP_INT_PCI_MABORT |
	    TXP_INT_DMA3 | TXP_INT_DMA2 | TXP_INT_DMA1 | TXP_INT_DMA0 |
	    TXP_INT_LATCH);

	/* ack all interrupts */
	WRITE_REG(sc, TXP_ISR, TXP_INT_RESERVED | TXP_INT_LATCH |
	    TXP_INT_A2H_7 | TXP_INT_A2H_6 | TXP_INT_A2H_5 | TXP_INT_A2H_4 |
	    TXP_INT_SELF | TXP_INT_PCI_TABORT | TXP_INT_PCI_MABORT |
	    TXP_INT_DMA3 | TXP_INT_DMA2 | TXP_INT_DMA1 | TXP_INT_DMA0 |
	    TXP_INT_A2H_3 | TXP_INT_A2H_2 | TXP_INT_A2H_1 | TXP_INT_A2H_0);

	if (txp_reset_adapter(sc))
		return (-1);

	/* disable interrupts */
	WRITE_REG(sc, TXP_IER, 0);
	WRITE_REG(sc, TXP_IMR,
	    TXP_INT_SELF | TXP_INT_PCI_TABORT | TXP_INT_PCI_MABORT |
	    TXP_INT_DMA3 | TXP_INT_DMA2 | TXP_INT_DMA1 | TXP_INT_DMA0 |
	    TXP_INT_LATCH);

	/* ack all interrupts */
	WRITE_REG(sc, TXP_ISR, TXP_INT_RESERVED | TXP_INT_LATCH |
	    TXP_INT_A2H_7 | TXP_INT_A2H_6 | TXP_INT_A2H_5 | TXP_INT_A2H_4 |
	    TXP_INT_SELF | TXP_INT_PCI_TABORT | TXP_INT_PCI_MABORT |
	    TXP_INT_DMA3 | TXP_INT_DMA2 | TXP_INT_DMA1 | TXP_INT_DMA0 |
	    TXP_INT_A2H_3 | TXP_INT_A2H_2 | TXP_INT_A2H_1 | TXP_INT_A2H_0);

	return (0);
}

static int
txp_reset_adapter(sc)
	struct txp_softc *sc;
{
	u_int32_t r;
	int i;

	WRITE_REG(sc, TXP_SRR, TXP_SRR_ALL);
	DELAY(1000);
	WRITE_REG(sc, TXP_SRR, 0);

	/* Should wait max 6 seconds */
	for (i = 0; i < 6000; i++) {
		r = READ_REG(sc, TXP_A2H_0);
		if (r == STAT_WAITING_FOR_HOST_REQUEST)
			break;
		DELAY(1000);
	}

	if (r != STAT_WAITING_FOR_HOST_REQUEST) {
		device_printf(sc->sc_dev, "reset hung\n");
		return (-1);
	}

	return (0);
}

static int
txp_download_fw(sc)
	struct txp_softc *sc;
{
	struct txp_fw_file_header *fileheader;
	struct txp_fw_section_header *secthead;
	int sect;
	u_int32_t r, i, ier, imr;

	ier = READ_REG(sc, TXP_IER);
	WRITE_REG(sc, TXP_IER, ier | TXP_INT_A2H_0);

	imr = READ_REG(sc, TXP_IMR);
	WRITE_REG(sc, TXP_IMR, imr | TXP_INT_A2H_0);

	for (i = 0; i < 10000; i++) {
		r = READ_REG(sc, TXP_A2H_0);
		if (r == STAT_WAITING_FOR_HOST_REQUEST)
			break;
		DELAY(50);
	}
	if (r != STAT_WAITING_FOR_HOST_REQUEST) {
		device_printf(sc->sc_dev, "not waiting for host request\n");
		return (-1);
	}

	/* Ack the status */
	WRITE_REG(sc, TXP_ISR, TXP_INT_A2H_0);

	fileheader = (struct txp_fw_file_header *)tc990image;
	if (bcmp("TYPHOON", fileheader->magicid, sizeof(fileheader->magicid))) {
		device_printf(sc->sc_dev, "fw invalid magic\n");
		return (-1);
	}

	/* Tell boot firmware to get ready for image */
	WRITE_REG(sc, TXP_H2A_1, fileheader->addr);
	WRITE_REG(sc, TXP_H2A_0, TXP_BOOTCMD_RUNTIME_IMAGE);

	if (txp_download_fw_wait(sc)) {
		device_printf(sc->sc_dev, "fw wait failed, initial\n");
		return (-1);
	}

	secthead = (struct txp_fw_section_header *)(((u_int8_t *)tc990image) +
	    sizeof(struct txp_fw_file_header));

	for (sect = 0; sect < fileheader->nsections; sect++) {
		if (txp_download_fw_section(sc, secthead, sect))
			return (-1);
		secthead = (struct txp_fw_section_header *)
		    (((u_int8_t *)secthead) + secthead->nbytes +
		    sizeof(*secthead));
	}

	WRITE_REG(sc, TXP_H2A_0, TXP_BOOTCMD_DOWNLOAD_COMPLETE);

	for (i = 0; i < 10000; i++) {
		r = READ_REG(sc, TXP_A2H_0);
		if (r == STAT_WAITING_FOR_BOOT)
			break;
		DELAY(50);
	}
	if (r != STAT_WAITING_FOR_BOOT) {
		device_printf(sc->sc_dev, "not waiting for boot\n");
		return (-1);
	}

	WRITE_REG(sc, TXP_IER, ier);
	WRITE_REG(sc, TXP_IMR, imr);

	return (0);
}

static int
txp_download_fw_wait(sc)
	struct txp_softc *sc;
{
	u_int32_t i, r;

	for (i = 0; i < 10000; i++) {
		r = READ_REG(sc, TXP_ISR);
		if (r & TXP_INT_A2H_0)
			break;
		DELAY(50);
	}

	if (!(r & TXP_INT_A2H_0)) {
		device_printf(sc->sc_dev, "fw wait failed comm0\n");
		return (-1);
	}

	WRITE_REG(sc, TXP_ISR, TXP_INT_A2H_0);

	r = READ_REG(sc, TXP_A2H_0);
	if (r != STAT_WAITING_FOR_SEGMENT) {
		device_printf(sc->sc_dev, "fw not waiting for segment\n");
		return (-1);
	}
	return (0);
}

static int
txp_download_fw_section(sc, sect, sectnum)
	struct txp_softc *sc;
	struct txp_fw_section_header *sect;
	int sectnum;
{
	vm_offset_t dma;
	int rseg, err = 0;
	struct mbuf m;
	u_int16_t csum;

	/* Skip zero length sections */
	if (sect->nbytes == 0)
		return (0);

	/* Make sure we aren't past the end of the image */
	rseg = ((u_int8_t *)sect) - ((u_int8_t *)tc990image);
	if (rseg >= sizeof(tc990image)) {
		device_printf(sc->sc_dev, "fw invalid section address, "
		    "section %d\n", sectnum);
		return (-1);
	}

	/* Make sure this section doesn't go past the end */
	rseg += sect->nbytes;
	if (rseg >= sizeof(tc990image)) {
		device_printf(sc->sc_dev, "fw truncated section %d\n",
		    sectnum);
		return (-1);
	}

	bcopy(((u_int8_t *)sect) + sizeof(*sect), sc->sc_fwbuf, sect->nbytes);
	dma = vtophys(sc->sc_fwbuf);

	/*
	 * dummy up mbuf and verify section checksum
	 */
	m.m_type = MT_DATA;
	m.m_next = m.m_nextpkt = NULL;
	m.m_len = sect->nbytes;
	m.m_data = sc->sc_fwbuf;
	m.m_flags = 0;
	csum = in_cksum(&m, sect->nbytes);
	if (csum != sect->cksum) {
		device_printf(sc->sc_dev, "fw section %d, bad "
		    "cksum (expected 0x%x got 0x%x)\n",
		    sectnum, sect->cksum, csum);
		err = -1;
		goto bail;
	}

	WRITE_REG(sc, TXP_H2A_1, sect->nbytes);
	WRITE_REG(sc, TXP_H2A_2, sect->cksum);
	WRITE_REG(sc, TXP_H2A_3, sect->addr);
	WRITE_REG(sc, TXP_H2A_4, 0);
	WRITE_REG(sc, TXP_H2A_5, dma & 0xffffffff);
	WRITE_REG(sc, TXP_H2A_0, TXP_BOOTCMD_SEGMENT_AVAILABLE);

	if (txp_download_fw_wait(sc)) {
		device_printf(sc->sc_dev, "fw wait failed, "
		    "section %d\n", sectnum);
		err = -1;
	}

bail:
	return (err);
}

static void 
txp_intr(vsc)
	void *vsc;
{
	struct txp_softc *sc = vsc;
	struct txp_hostvar *hv = sc->sc_hostvar;
	u_int32_t isr;

	/* mask all interrupts */
	WRITE_REG(sc, TXP_IMR, TXP_INT_RESERVED | TXP_INT_SELF |
	    TXP_INT_A2H_7 | TXP_INT_A2H_6 | TXP_INT_A2H_5 | TXP_INT_A2H_4 |
	    TXP_INT_A2H_2 | TXP_INT_A2H_1 | TXP_INT_A2H_0 |
	    TXP_INT_DMA3 | TXP_INT_DMA2 | TXP_INT_DMA1 | TXP_INT_DMA0 |
	    TXP_INT_PCI_TABORT | TXP_INT_PCI_MABORT |  TXP_INT_LATCH);

	isr = READ_REG(sc, TXP_ISR);
	while (isr) {
		WRITE_REG(sc, TXP_ISR, isr);

		if ((*sc->sc_rxhir.r_roff) != (*sc->sc_rxhir.r_woff))
			txp_rx_reclaim(sc, &sc->sc_rxhir);
		if ((*sc->sc_rxlor.r_roff) != (*sc->sc_rxlor.r_woff))
			txp_rx_reclaim(sc, &sc->sc_rxlor);

		if (hv->hv_rx_buf_write_idx == hv->hv_rx_buf_read_idx)
			txp_rxbuf_reclaim(sc);

		if (sc->sc_txhir.r_cnt && (sc->sc_txhir.r_cons !=
		    TXP_OFFSET2IDX(*(sc->sc_txhir.r_off))))
			txp_tx_reclaim(sc, &sc->sc_txhir);

		if (sc->sc_txlor.r_cnt && (sc->sc_txlor.r_cons !=
		    TXP_OFFSET2IDX(*(sc->sc_txlor.r_off))))
			txp_tx_reclaim(sc, &sc->sc_txlor);

		isr = READ_REG(sc, TXP_ISR);
	}

	/* unmask all interrupts */
	WRITE_REG(sc, TXP_IMR, TXP_INT_A2H_3);

	txp_start(&sc->sc_arpcom.ac_if);

	return;
}

static void
txp_rx_reclaim(sc, r)
	struct txp_softc *sc;
	struct txp_rx_ring *r;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	struct txp_rx_desc *rxd;
	struct mbuf *m;
	struct txp_swdesc *sd = NULL;
	u_int32_t roff, woff;
	struct ether_header *eh = NULL;

	roff = *r->r_roff;
	woff = *r->r_woff;
	rxd = r->r_desc + (roff / sizeof(struct txp_rx_desc));

	while (roff != woff) {

		if (rxd->rx_flags & RX_FLAGS_ERROR) {
			device_printf(sc->sc_dev, "error 0x%x\n",
			    rxd->rx_stat);
			ifp->if_ierrors++;
			goto next;
		}

		/* retrieve stashed pointer */
		sd = rxd->rx_sd;

		m = sd->sd_mbuf;
		sd->sd_mbuf = NULL;

		m->m_pkthdr.len = m->m_len = rxd->rx_len;

#ifdef __STRICT_ALIGNMENT
		{
			/*
			 * XXX Nice chip, except it won't accept "off by 2"
			 * buffers, so we're force to copy.  Supposedly
			 * this will be fixed in a newer firmware rev
			 * and this will be temporary.
			 */
			struct mbuf *mnew;

			MGETHDR(mnew, M_DONTWAIT, MT_DATA);
			if (mnew == NULL) {
				m_freem(m);
				goto next;
			}
			if (m->m_len > (MHLEN - 2)) {
				MCLGET(mnew, M_DONTWAIT);
				if (!(mnew->m_flags & M_EXT)) {
					m_freem(mnew);
					m_freem(m);
					goto next;
				}
			}
			mnew->m_pkthdr.rcvif = ifp;
			m_adj(mnew, 2);
			mnew->m_pkthdr.len = mnew->m_len = m->m_len;
			m_copydata(m, 0, m->m_pkthdr.len, mtod(mnew, caddr_t));
			m_freem(m);
			m = mnew;
		}
#endif

		if (rxd->rx_stat & RX_STAT_IPCKSUMBAD)
			m->m_pkthdr.csum_flags |= CSUM_IP_CHECKED;
		else if (rxd->rx_stat & RX_STAT_IPCKSUMGOOD)
		 	m->m_pkthdr.csum_flags |=
			    CSUM_IP_CHECKED|CSUM_IP_VALID;

		if ((rxd->rx_stat & RX_STAT_TCPCKSUMGOOD) ||
		    (rxd->rx_stat & RX_STAT_UDPCKSUMGOOD)) {
			m->m_pkthdr.csum_flags |=
			    CSUM_DATA_VALID|CSUM_PSEUDO_HDR;
			m->m_pkthdr.csum_data = 0xffff;
		}

		eh = mtod(m, struct ether_header *);
		/* Remove header from mbuf and pass it on. */
		m_adj(m, sizeof(struct ether_header));

		if (rxd->rx_stat & RX_STAT_VLAN) {
			VLAN_INPUT_TAG(eh, m, htons(rxd->rx_vlan >> 16));
			goto next;
		}

		ether_input(ifp, eh, m);

next:

		roff += sizeof(struct txp_rx_desc);
		if (roff == (RX_ENTRIES * sizeof(struct txp_rx_desc))) {
			roff = 0;
			rxd = r->r_desc;
		} else
			rxd++;
		woff = *r->r_woff;
	}

	*r->r_roff = woff;

	return;
}

static void
txp_rxbuf_reclaim(sc)
	struct txp_softc *sc;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	struct txp_hostvar *hv = sc->sc_hostvar;
	struct txp_rxbuf_desc *rbd;
	struct txp_swdesc *sd;
	u_int32_t i;

	if (!(ifp->if_flags & IFF_RUNNING))
		return;

	i = sc->sc_rxbufprod;
	rbd = sc->sc_rxbufs + i;

	while (1) {
		sd = rbd->rb_sd;
		if (sd->sd_mbuf != NULL)
			break;

		MGETHDR(sd->sd_mbuf, M_DONTWAIT, MT_DATA);
		if (sd->sd_mbuf == NULL)
			goto err_sd;

		MCLGET(sd->sd_mbuf, M_DONTWAIT);
		if ((sd->sd_mbuf->m_flags & M_EXT) == 0)
			goto err_mbuf;
		sd->sd_mbuf->m_pkthdr.rcvif = ifp;
		sd->sd_mbuf->m_pkthdr.len = sd->sd_mbuf->m_len = MCLBYTES;

		rbd->rb_paddrlo = vtophys(mtod(sd->sd_mbuf, vm_offset_t))
		    & 0xffffffff;
		rbd->rb_paddrhi = 0;

		hv->hv_rx_buf_write_idx = TXP_IDX2OFFSET(i);

		if (++i == RXBUF_ENTRIES) {
			i = 0;
			rbd = sc->sc_rxbufs;
		} else
			rbd++;
	}

	sc->sc_rxbufprod = i;

	return;

err_mbuf:
	m_freem(sd->sd_mbuf);
err_sd:
	free(sd, M_DEVBUF);
}

/*
 * Reclaim mbufs and entries from a transmit ring.
 */
static void
txp_tx_reclaim(sc, r)
	struct txp_softc *sc;
	struct txp_tx_ring *r;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	u_int32_t idx = TXP_OFFSET2IDX(*(r->r_off));
	u_int32_t cons = r->r_cons, cnt = r->r_cnt;
	struct txp_tx_desc *txd = r->r_desc + cons;
	struct txp_swdesc *sd = sc->sc_txd + cons;
	struct mbuf *m;

	while (cons != idx) {
		if (cnt == 0)
			break;

		if ((txd->tx_flags & TX_FLAGS_TYPE_M) ==
		    TX_FLAGS_TYPE_DATA) {
			m = sd->sd_mbuf;
			if (m != NULL) {
				m_freem(m);
				txd->tx_addrlo = 0;
				txd->tx_addrhi = 0;
				ifp->if_opackets++;
			}
		}
		ifp->if_flags &= ~IFF_OACTIVE;

		if (++cons == TX_ENTRIES) {
			txd = r->r_desc;
			cons = 0;
			sd = sc->sc_txd;
		} else {
			txd++;
			sd++;
		}

		cnt--;
	}

	r->r_cons = cons;
	r->r_cnt = cnt;
	if (cnt == 0)
		ifp->if_timer = 0;
}

static int
txp_shutdown(dev)
	device_t dev;
{
	struct txp_softc *sc;

	sc = device_get_softc(dev);

	/* mask all interrupts */
	WRITE_REG(sc, TXP_IMR,
	    TXP_INT_SELF | TXP_INT_PCI_TABORT | TXP_INT_PCI_MABORT |
	    TXP_INT_DMA3 | TXP_INT_DMA2 | TXP_INT_DMA1 | TXP_INT_DMA0 |
	    TXP_INT_LATCH);

	txp_command(sc, TXP_CMD_TX_DISABLE, 0, 0, 0, NULL, NULL, NULL, 0);
	txp_command(sc, TXP_CMD_RX_DISABLE, 0, 0, 0, NULL, NULL, NULL, 0);
	txp_command(sc, TXP_CMD_HALT, 0, 0, 0, NULL, NULL, NULL, 0);

	return(0);
}

static int
txp_alloc_rings(sc)
	struct txp_softc *sc;
{
	struct txp_boot_record *boot;
	struct txp_ldata *ld;
	u_int32_t r;
	int i;

	ld = sc->sc_ldata;
	boot = &ld->txp_boot;

	/* boot record */
	sc->sc_boot = boot;

	/* host variables */
	bzero(&ld->txp_hostvar, sizeof(struct txp_hostvar));
	boot->br_hostvar_lo = vtophys(&ld->txp_hostvar);
	boot->br_hostvar_hi = 0;
	sc->sc_hostvar = (struct txp_hostvar *)&ld->txp_hostvar;

	/* hi priority tx ring */
	boot->br_txhipri_lo = vtophys(&ld->txp_txhiring);;
	boot->br_txhipri_hi = 0;
	boot->br_txhipri_siz = TX_ENTRIES * sizeof(struct txp_tx_desc);
	sc->sc_txhir.r_reg = TXP_H2A_1;
	sc->sc_txhir.r_desc = (struct txp_tx_desc *)&ld->txp_txhiring;
	sc->sc_txhir.r_cons = sc->sc_txhir.r_prod = sc->sc_txhir.r_cnt = 0;
	sc->sc_txhir.r_off = &sc->sc_hostvar->hv_tx_hi_desc_read_idx;

	/* lo priority tx ring */
	boot->br_txlopri_lo = vtophys(&ld->txp_txloring);
	boot->br_txlopri_hi = 0;
	boot->br_txlopri_siz = TX_ENTRIES * sizeof(struct txp_tx_desc);
	sc->sc_txlor.r_reg = TXP_H2A_3;
	sc->sc_txlor.r_desc = (struct txp_tx_desc *)&ld->txp_txloring;
	sc->sc_txlor.r_cons = sc->sc_txlor.r_prod = sc->sc_txlor.r_cnt = 0;
	sc->sc_txlor.r_off = &sc->sc_hostvar->hv_tx_lo_desc_read_idx;

	/* high priority rx ring */
	boot->br_rxhipri_lo = vtophys(&ld->txp_rxhiring);
	boot->br_rxhipri_hi = 0;
	boot->br_rxhipri_siz = RX_ENTRIES * sizeof(struct txp_rx_desc);
	sc->sc_rxhir.r_desc = (struct txp_rx_desc *)&ld->txp_rxhiring;
	sc->sc_rxhir.r_roff = &sc->sc_hostvar->hv_rx_hi_read_idx;
	sc->sc_rxhir.r_woff = &sc->sc_hostvar->hv_rx_hi_write_idx;

	/* low priority rx ring */
	boot->br_rxlopri_lo = vtophys(&ld->txp_rxloring);
	boot->br_rxlopri_hi = 0;
	boot->br_rxlopri_siz = RX_ENTRIES * sizeof(struct txp_rx_desc);
	sc->sc_rxlor.r_desc = (struct txp_rx_desc *)&ld->txp_rxloring;
	sc->sc_rxlor.r_roff = &sc->sc_hostvar->hv_rx_lo_read_idx;
	sc->sc_rxlor.r_woff = &sc->sc_hostvar->hv_rx_lo_write_idx;

	/* command ring */
	bzero(&ld->txp_cmdring, sizeof(struct txp_cmd_desc) * CMD_ENTRIES);
	boot->br_cmd_lo = vtophys(&ld->txp_cmdring);
	boot->br_cmd_hi = 0;
	boot->br_cmd_siz = CMD_ENTRIES * sizeof(struct txp_cmd_desc);
	sc->sc_cmdring.base = (struct txp_cmd_desc *)&ld->txp_cmdring;
	sc->sc_cmdring.size = CMD_ENTRIES * sizeof(struct txp_cmd_desc);
	sc->sc_cmdring.lastwrite = 0;

	/* response ring */
	bzero(&ld->txp_rspring, sizeof(struct txp_rsp_desc) * RSP_ENTRIES);
	boot->br_resp_lo = vtophys(&ld->txp_rspring);
	boot->br_resp_hi = 0;
	boot->br_resp_siz = CMD_ENTRIES * sizeof(struct txp_rsp_desc);
	sc->sc_rspring.base = (struct txp_rsp_desc *)&ld->txp_rspring;
	sc->sc_rspring.size = RSP_ENTRIES * sizeof(struct txp_rsp_desc);
	sc->sc_rspring.lastwrite = 0;

	/* receive buffer ring */
	boot->br_rxbuf_lo = vtophys(&ld->txp_rxbufs);
	boot->br_rxbuf_hi = 0;
	boot->br_rxbuf_siz = RXBUF_ENTRIES * sizeof(struct txp_rxbuf_desc);
	sc->sc_rxbufs = (struct txp_rxbuf_desc *)&ld->txp_rxbufs;

	for (i = 0; i < RXBUF_ENTRIES; i++) {
		struct txp_swdesc *sd;
		if (sc->sc_rxbufs[i].rb_sd != NULL)
			continue;
		sc->sc_rxbufs[i].rb_sd = malloc(sizeof(struct txp_swdesc),
		    M_DEVBUF, M_NOWAIT);
		if (sc->sc_rxbufs[i].rb_sd == NULL)
			return(ENOBUFS);
		sd = sc->sc_rxbufs[i].rb_sd;
		sd->sd_mbuf = NULL;
	}
	sc->sc_rxbufprod = 0;

	/* zero dma */
	bzero(&ld->txp_zero, sizeof(u_int32_t));
	boot->br_zero_lo = vtophys(&ld->txp_zero);
	boot->br_zero_hi = 0;

	/* See if it's waiting for boot, and try to boot it */
	for (i = 0; i < 10000; i++) {
		r = READ_REG(sc, TXP_A2H_0);
		if (r == STAT_WAITING_FOR_BOOT)
			break;
		DELAY(50);
	}

	if (r != STAT_WAITING_FOR_BOOT) {
		device_printf(sc->sc_dev, "not waiting for boot\n");
		return(ENXIO);
	}

	WRITE_REG(sc, TXP_H2A_2, 0);
	WRITE_REG(sc, TXP_H2A_1, vtophys(sc->sc_boot));
	WRITE_REG(sc, TXP_H2A_0, TXP_BOOTCMD_REGISTER_BOOT_RECORD);

	/* See if it booted */
	for (i = 0; i < 10000; i++) {
		r = READ_REG(sc, TXP_A2H_0);
		if (r == STAT_RUNNING)
			break;
		DELAY(50);
	}
	if (r != STAT_RUNNING) {
		device_printf(sc->sc_dev, "fw not running\n");
		return(ENXIO);
	}

	/* Clear TX and CMD ring write registers */
	WRITE_REG(sc, TXP_H2A_1, TXP_BOOTCMD_NULL);
	WRITE_REG(sc, TXP_H2A_2, TXP_BOOTCMD_NULL);
	WRITE_REG(sc, TXP_H2A_3, TXP_BOOTCMD_NULL);
	WRITE_REG(sc, TXP_H2A_0, TXP_BOOTCMD_NULL);

	return (0);
}

static int
txp_ioctl(ifp, command, data)
	struct ifnet *ifp;
	u_long command;
	caddr_t data;
{
	struct txp_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	int s, error = 0;

	s = splnet();

	if ((error = ether_ioctl(ifp, command, data)) > 0) {
		splx(s);
		return error;
	}

	switch(command) {
	case SIOCSIFADDR:
	case SIOCGIFADDR:
	case SIOCSIFMTU:
		error = ether_ioctl(ifp, command, data);
		break;
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			txp_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				txp_stop(sc);
		}
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		/*
		 * Multicast list has changed; set the hardware
		 * filter accordingly.
		 */
		txp_set_filter(sc);
		error = 0;
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_ifmedia, command);
		break;
	default:
		error = EINVAL;
		break;
	}

	(void)splx(s);

	return(error);
}

static int
txp_rxring_fill(sc)
	struct txp_softc *sc;
{
	int i;
	struct ifnet *ifp;
	struct txp_swdesc *sd;

	ifp = &sc->sc_arpcom.ac_if;

	for (i = 0; i < RXBUF_ENTRIES; i++) {
		sd = sc->sc_rxbufs[i].rb_sd;
		MGETHDR(sd->sd_mbuf, M_DONTWAIT, MT_DATA);
		if (sd->sd_mbuf == NULL)
			return(ENOBUFS);

		MCLGET(sd->sd_mbuf, M_DONTWAIT);
		if ((sd->sd_mbuf->m_flags & M_EXT) == 0) {
			m_freem(sd->sd_mbuf);
			return(ENOBUFS);
		}
		sd->sd_mbuf->m_pkthdr.len = sd->sd_mbuf->m_len = MCLBYTES;
		sd->sd_mbuf->m_pkthdr.rcvif = ifp;

		sc->sc_rxbufs[i].rb_paddrlo =
		    vtophys(mtod(sd->sd_mbuf, vm_offset_t));
		sc->sc_rxbufs[i].rb_paddrhi = 0;
	}

	sc->sc_hostvar->hv_rx_buf_write_idx = (RXBUF_ENTRIES - 1) *
	    sizeof(struct txp_rxbuf_desc);

	return(0);
}

static void
txp_rxring_empty(sc)
	struct txp_softc *sc;
{
	int i;
	struct txp_swdesc *sd;

	if (sc->sc_rxbufs == NULL)
		return;

	for (i = 0; i < RXBUF_ENTRIES; i++) {
		if (&sc->sc_rxbufs[i] == NULL)
			continue;
		sd = sc->sc_rxbufs[i].rb_sd;
		if (sd == NULL)
			continue;
		if (sd->sd_mbuf != NULL) {
			m_freem(sd->sd_mbuf);
			sd->sd_mbuf = NULL;
		}
	}

	return;
}

static void
txp_init(xsc)
	void *xsc;
{
	struct txp_softc *sc;
	struct ifnet *ifp;
	u_int16_t p1;
	u_int32_t p2;
	int s;

	sc = xsc;
	ifp = &sc->sc_arpcom.ac_if;

	if (ifp->if_flags & IFF_RUNNING)
		return;

	txp_stop(sc);

	s = splnet();

	txp_command(sc, TXP_CMD_MAX_PKT_SIZE_WRITE, TXP_MAX_PKTLEN, 0, 0,
	    NULL, NULL, NULL, 1);

	/* Set station address. */
	((u_int8_t *)&p1)[1] = sc->sc_arpcom.ac_enaddr[0];
	((u_int8_t *)&p1)[0] = sc->sc_arpcom.ac_enaddr[1];
	((u_int8_t *)&p2)[3] = sc->sc_arpcom.ac_enaddr[2];
	((u_int8_t *)&p2)[2] = sc->sc_arpcom.ac_enaddr[3];
	((u_int8_t *)&p2)[1] = sc->sc_arpcom.ac_enaddr[4];
	((u_int8_t *)&p2)[0] = sc->sc_arpcom.ac_enaddr[5];
	txp_command(sc, TXP_CMD_STATION_ADDRESS_WRITE, p1, p2, 0,
	    NULL, NULL, NULL, 1);

	txp_set_filter(sc);

	txp_rxring_fill(sc);

	txp_command(sc, TXP_CMD_TX_ENABLE, 0, 0, 0, NULL, NULL, NULL, 1);
	txp_command(sc, TXP_CMD_RX_ENABLE, 0, 0, 0, NULL, NULL, NULL, 1);

	WRITE_REG(sc, TXP_IER, TXP_INT_RESERVED | TXP_INT_SELF |
	    TXP_INT_A2H_7 | TXP_INT_A2H_6 | TXP_INT_A2H_5 | TXP_INT_A2H_4 |
	    TXP_INT_A2H_2 | TXP_INT_A2H_1 | TXP_INT_A2H_0 |
	    TXP_INT_DMA3 | TXP_INT_DMA2 | TXP_INT_DMA1 | TXP_INT_DMA0 |
	    TXP_INT_PCI_TABORT | TXP_INT_PCI_MABORT |  TXP_INT_LATCH);
	WRITE_REG(sc, TXP_IMR, TXP_INT_A2H_3);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;
	ifp->if_timer = 0;

	sc->sc_tick = timeout(txp_tick, sc, hz);

	splx(s);
}

static void
txp_tick(vsc)
	void *vsc;
{
	struct txp_softc *sc = vsc;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	struct txp_rsp_desc *rsp = NULL;
	struct txp_ext_desc *ext;
	int s;

	s = splnet();
	txp_rxbuf_reclaim(sc);

	if (txp_command2(sc, TXP_CMD_READ_STATISTICS, 0, 0, 0, NULL, 0,
	    &rsp, 1))
		goto out;
	if (rsp->rsp_numdesc != 6)
		goto out;
	if (txp_command(sc, TXP_CMD_CLEAR_STATISTICS, 0, 0, 0,
	    NULL, NULL, NULL, 1))
		goto out;
	ext = (struct txp_ext_desc *)(rsp + 1);

	ifp->if_ierrors += ext[3].ext_2 + ext[3].ext_3 + ext[3].ext_4 +
	    ext[4].ext_1 + ext[4].ext_4;
	ifp->if_oerrors += ext[0].ext_1 + ext[1].ext_1 + ext[1].ext_4 +
	    ext[2].ext_1;
	ifp->if_collisions += ext[0].ext_2 + ext[0].ext_3 + ext[1].ext_2 +
	    ext[1].ext_3;
	ifp->if_opackets += rsp->rsp_par2;
	ifp->if_ipackets += ext[2].ext_3;

out:
	if (rsp != NULL)
		free(rsp, M_DEVBUF);

	splx(s);
	sc->sc_tick = timeout(txp_tick, sc, hz);

	return;
}

static void
txp_start(ifp)
	struct ifnet *ifp;
{
	struct txp_softc *sc = ifp->if_softc;
	struct txp_tx_ring *r = &sc->sc_txhir;
	struct txp_tx_desc *txd;
	struct txp_frag_desc *fxd;
	struct mbuf *m, *m0;
	struct txp_swdesc *sd;
	u_int32_t firstprod, firstcnt, prod, cnt;
	struct ifvlan		*ifv;

	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

	prod = r->r_prod;
	cnt = r->r_cnt;

	while (1) {
		IF_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			break;

		firstprod = prod;
		firstcnt = cnt;

		sd = sc->sc_txd + prod;
		sd->sd_mbuf = m;

		if ((TX_ENTRIES - cnt) < 4)
			goto oactive;

		txd = r->r_desc + prod;

		txd->tx_flags = TX_FLAGS_TYPE_DATA;
		txd->tx_numdesc = 0;
		txd->tx_addrlo = 0;
		txd->tx_addrhi = 0;
		txd->tx_totlen = 0;
		txd->tx_pflags = 0;

		if (++prod == TX_ENTRIES)
			prod = 0;

		if (++cnt >= (TX_ENTRIES - 4))
			goto oactive;

		if ((m->m_flags & (M_PROTO1|M_PKTHDR)) == (M_PROTO1|M_PKTHDR) &&
		    m->m_pkthdr.rcvif != NULL) {
			ifv = m->m_pkthdr.rcvif->if_softc;
			txd->tx_pflags = TX_PFLAGS_VLAN |
			    (htons(ifv->ifv_tag) << TX_PFLAGS_VLANTAG_S);
		}

		if (m->m_pkthdr.csum_flags & CSUM_IP)
			txd->tx_pflags |= TX_PFLAGS_IPCKSUM;

#if 0
		if (m->m_pkthdr.csum_flags & CSUM_TCP)
			txd->tx_pflags |= TX_PFLAGS_TCPCKSUM;
		if (m->m_pkthdr.csum_flags & CSUM_UDP)
			txd->tx_pflags |= TX_PFLAGS_UDPCKSUM;
#endif

		fxd = (struct txp_frag_desc *)(r->r_desc + prod);
		for (m0 = m; m0 != NULL; m0 = m0->m_next) {
			if (m0->m_len == 0)
				continue;
			if (++cnt >= (TX_ENTRIES - 4))
				goto oactive;

			txd->tx_numdesc++;

			fxd->frag_flags = FRAG_FLAGS_TYPE_FRAG;
			fxd->frag_rsvd1 = 0;
			fxd->frag_len = m0->m_len;
			fxd->frag_addrlo = vtophys(mtod(m0, vm_offset_t));
			fxd->frag_addrhi = 0;
			fxd->frag_rsvd2 = 0;

			if (++prod == TX_ENTRIES) {
				fxd = (struct txp_frag_desc *)r->r_desc;
				prod = 0;
			} else
				fxd++;

		}

		ifp->if_timer = 5;

		if (ifp->if_bpf)
			bpf_mtap(ifp, m);
		WRITE_REG(sc, r->r_reg, TXP_IDX2OFFSET(prod));
	}

	r->r_prod = prod;
	r->r_cnt = cnt;
	return;

oactive:
	ifp->if_flags |= IFF_OACTIVE;
	r->r_prod = firstprod;
	r->r_cnt = firstcnt;
	IF_PREPEND(&ifp->if_snd, m);
	return;
}

/*
 * Handle simple commands sent to the typhoon
 */
static int
txp_command(sc, id, in1, in2, in3, out1, out2, out3, wait)
	struct txp_softc *sc;
	u_int16_t id, in1, *out1;
	u_int32_t in2, in3, *out2, *out3;
	int wait;
{
	struct txp_rsp_desc *rsp = NULL;

	if (txp_command2(sc, id, in1, in2, in3, NULL, 0, &rsp, wait))
		return (-1);

	if (!wait)
		return (0);

	if (out1 != NULL)
		*out1 = rsp->rsp_par1;
	if (out2 != NULL)
		*out2 = rsp->rsp_par2;
	if (out3 != NULL)
		*out3 = rsp->rsp_par3;
	free(rsp, M_DEVBUF);
	return (0);
}

static int
txp_command2(sc, id, in1, in2, in3, in_extp, in_extn, rspp, wait)
	struct txp_softc *sc;
	u_int16_t id, in1;
	u_int32_t in2, in3;
	struct txp_ext_desc *in_extp;
	u_int8_t in_extn;
	struct txp_rsp_desc **rspp;
	int wait;
{
	struct txp_hostvar *hv = sc->sc_hostvar;
	struct txp_cmd_desc *cmd;
	struct txp_ext_desc *ext;
	u_int32_t idx, i;
	u_int16_t seq;

	if (txp_cmd_desc_numfree(sc) < (in_extn + 1)) {
		device_printf(sc->sc_dev, "no free cmd descriptors\n");
		return (-1);
	}

	idx = sc->sc_cmdring.lastwrite;
	cmd = (struct txp_cmd_desc *)(((u_int8_t *)sc->sc_cmdring.base) + idx);
	bzero(cmd, sizeof(*cmd));

	cmd->cmd_numdesc = in_extn;
	cmd->cmd_seq = seq = sc->sc_seq++;
	cmd->cmd_id = id;
	cmd->cmd_par1 = in1;
	cmd->cmd_par2 = in2;
	cmd->cmd_par3 = in3;
	cmd->cmd_flags = CMD_FLAGS_TYPE_CMD |
	    (wait ? CMD_FLAGS_RESP : 0) | CMD_FLAGS_VALID;

	idx += sizeof(struct txp_cmd_desc);
	if (idx == sc->sc_cmdring.size)
		idx = 0;

	for (i = 0; i < in_extn; i++) {
		ext = (struct txp_ext_desc *)(((u_int8_t *)sc->sc_cmdring.base) + idx);
		bcopy(in_extp, ext, sizeof(struct txp_ext_desc));
		in_extp++;
		idx += sizeof(struct txp_cmd_desc);
		if (idx == sc->sc_cmdring.size)
			idx = 0;
	}

	sc->sc_cmdring.lastwrite = idx;

	WRITE_REG(sc, TXP_H2A_2, sc->sc_cmdring.lastwrite);

	if (!wait)
		return (0);

	for (i = 0; i < 10000; i++) {
		idx = hv->hv_resp_read_idx;
		if (idx != hv->hv_resp_write_idx) {
			*rspp = NULL;
			if (txp_response(sc, idx, id, seq, rspp))
				return (-1);
			if (*rspp != NULL)
				break;
		}
		DELAY(50);
	}
	if (i == 1000 || (*rspp) == NULL) {
		device_printf(sc->sc_dev, "0x%x command failed\n", id);
		return (-1);
	}

	return (0);
}

static int
txp_response(sc, ridx, id, seq, rspp)
	struct txp_softc *sc;
	u_int32_t ridx;
	u_int16_t id;
	u_int16_t seq;
	struct txp_rsp_desc **rspp;
{
	struct txp_hostvar *hv = sc->sc_hostvar;
	struct txp_rsp_desc *rsp;

	while (ridx != hv->hv_resp_write_idx) {
		rsp = (struct txp_rsp_desc *)(((u_int8_t *)sc->sc_rspring.base) + ridx);

		if (id == rsp->rsp_id && rsp->rsp_seq == seq) {
			*rspp = (struct txp_rsp_desc *)malloc(
			    sizeof(struct txp_rsp_desc) * (rsp->rsp_numdesc + 1),
			    M_DEVBUF, M_NOWAIT);
			if ((*rspp) == NULL)
				return (-1);
			txp_rsp_fixup(sc, rsp, *rspp);
			return (0);
		}

		if (rsp->rsp_flags & RSP_FLAGS_ERROR) {
			device_printf(sc->sc_dev, "response error!\n");
			txp_rsp_fixup(sc, rsp, NULL);
			ridx = hv->hv_resp_read_idx;
			continue;
		}

		switch (rsp->rsp_id) {
		case TXP_CMD_CYCLE_STATISTICS:
		case TXP_CMD_MEDIA_STATUS_READ:
			break;
		case TXP_CMD_HELLO_RESPONSE:
			device_printf(sc->sc_dev, "hello\n");
			break;
		default:
			device_printf(sc->sc_dev, "unknown id(0x%x)\n",
			    rsp->rsp_id);
		}

		txp_rsp_fixup(sc, rsp, NULL);
		ridx = hv->hv_resp_read_idx;
		hv->hv_resp_read_idx = ridx;
	}

	return (0);
}

static void
txp_rsp_fixup(sc, rsp, dst)
	struct txp_softc *sc;
	struct txp_rsp_desc *rsp, *dst;
{
	struct txp_rsp_desc *src = rsp;
	struct txp_hostvar *hv = sc->sc_hostvar;
	u_int32_t i, ridx;

	ridx = hv->hv_resp_read_idx;

	for (i = 0; i < rsp->rsp_numdesc + 1; i++) {
		if (dst != NULL)
			bcopy(src, dst++, sizeof(struct txp_rsp_desc));
		ridx += sizeof(struct txp_rsp_desc);
		if (ridx == sc->sc_rspring.size) {
			src = sc->sc_rspring.base;
			ridx = 0;
		} else
			src++;
		sc->sc_rspring.lastwrite = hv->hv_resp_read_idx = ridx;
	}
	
	hv->hv_resp_read_idx = ridx;
}

static int
txp_cmd_desc_numfree(sc)
	struct txp_softc *sc;
{
	struct txp_hostvar *hv = sc->sc_hostvar;
	struct txp_boot_record *br = sc->sc_boot;
	u_int32_t widx, ridx, nfree;

	widx = sc->sc_cmdring.lastwrite;
	ridx = hv->hv_cmd_read_idx;

	if (widx == ridx) {
		/* Ring is completely free */
		nfree = br->br_cmd_siz - sizeof(struct txp_cmd_desc);
	} else {
		if (widx > ridx)
			nfree = br->br_cmd_siz -
			    (widx - ridx + sizeof(struct txp_cmd_desc));
		else
			nfree = ridx - widx - sizeof(struct txp_cmd_desc);
	}

	return (nfree / sizeof(struct txp_cmd_desc));
}

static void
txp_stop(sc)
	struct txp_softc *sc;
{
	struct ifnet *ifp;

	ifp = &sc->sc_arpcom.ac_if;

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	untimeout(txp_tick, sc, sc->sc_tick);

	txp_command(sc, TXP_CMD_TX_DISABLE, 0, 0, 0, NULL, NULL, NULL, 1);
	txp_command(sc, TXP_CMD_RX_DISABLE, 0, 0, 0, NULL, NULL, NULL, 1);

	txp_rxring_empty(sc);

	return;
}

static void
txp_watchdog(ifp)
	struct ifnet *ifp;
{
	return;
}

static int
txp_ifmedia_upd(ifp)
	struct ifnet *ifp;
{
	struct txp_softc *sc = ifp->if_softc;
	struct ifmedia *ifm = &sc->sc_ifmedia;
	u_int16_t new_xcvr;

	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
		return (EINVAL);

	if (IFM_SUBTYPE(ifm->ifm_media) == IFM_10_T) {
		if ((ifm->ifm_media & IFM_GMASK) == IFM_FDX)
			new_xcvr = TXP_XCVR_10_FDX;
		else
			new_xcvr = TXP_XCVR_10_HDX;
	} else if (IFM_SUBTYPE(ifm->ifm_media) == IFM_100_TX) {
		if ((ifm->ifm_media & IFM_GMASK) == IFM_FDX)
			new_xcvr = TXP_XCVR_100_FDX;
		else
			new_xcvr = TXP_XCVR_100_HDX;
	} else if (IFM_SUBTYPE(ifm->ifm_media) == IFM_AUTO) {
		new_xcvr = TXP_XCVR_AUTO;
	} else
		return (EINVAL);

	/* nothing to do */
	if (sc->sc_xcvr == new_xcvr)
		return (0);

	txp_command(sc, TXP_CMD_XCVR_SELECT, new_xcvr, 0, 0,
	    NULL, NULL, NULL, 0);
	sc->sc_xcvr = new_xcvr;

	return (0);
}

static void
txp_ifmedia_sts(ifp, ifmr)
	struct ifnet *ifp;
	struct ifmediareq *ifmr;
{
	struct txp_softc *sc = ifp->if_softc;
	struct ifmedia *ifm = &sc->sc_ifmedia;
	u_int16_t bmsr, bmcr, anlpar;

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	if (txp_command(sc, TXP_CMD_PHY_MGMT_READ, 0, MII_BMSR, 0,
	    &bmsr, NULL, NULL, 1))
		goto bail;
	if (txp_command(sc, TXP_CMD_PHY_MGMT_READ, 0, MII_BMSR, 0,
	    &bmsr, NULL, NULL, 1))
		goto bail;

	if (txp_command(sc, TXP_CMD_PHY_MGMT_READ, 0, MII_BMCR, 0,
	    &bmcr, NULL, NULL, 1))
		goto bail;

	if (txp_command(sc, TXP_CMD_PHY_MGMT_READ, 0, MII_ANLPAR, 0,
	    &anlpar, NULL, NULL, 1))
		goto bail;

	if (bmsr & BMSR_LINK)
		ifmr->ifm_status |= IFM_ACTIVE;

	if (bmcr & BMCR_ISO) {
		ifmr->ifm_active |= IFM_NONE;
		ifmr->ifm_status = 0;
		return;
	}

	if (bmcr & BMCR_LOOP)
		ifmr->ifm_active |= IFM_LOOP;

	if (bmcr & BMCR_AUTOEN) {
		if ((bmsr & BMSR_ACOMP) == 0) {
			ifmr->ifm_active |= IFM_NONE;
			return;
		}

		if (anlpar & ANLPAR_T4)
			ifmr->ifm_active |= IFM_100_T4;
		else if (anlpar & ANLPAR_TX_FD)
			ifmr->ifm_active |= IFM_100_TX|IFM_FDX;
		else if (anlpar & ANLPAR_TX)
			ifmr->ifm_active |= IFM_100_TX;
		else if (anlpar & ANLPAR_10_FD)
			ifmr->ifm_active |= IFM_10_T|IFM_FDX;
		else if (anlpar & ANLPAR_10)
			ifmr->ifm_active |= IFM_10_T;
		else
			ifmr->ifm_active |= IFM_NONE;
	} else
		ifmr->ifm_active = ifm->ifm_cur->ifm_media;
	return;

bail:
	ifmr->ifm_active |= IFM_NONE;
	ifmr->ifm_status &= ~IFM_AVALID;
}

#ifdef TXP_DEBUG
static void
txp_show_descriptor(d)
	void *d;
{
	struct txp_cmd_desc *cmd = d;
	struct txp_rsp_desc *rsp = d;
	struct txp_tx_desc *txd = d;
	struct txp_frag_desc *frgd = d;

	switch (cmd->cmd_flags & CMD_FLAGS_TYPE_M) {
	case CMD_FLAGS_TYPE_CMD:
		/* command descriptor */
		printf("[cmd flags 0x%x num %d id %d seq %d par1 0x%x par2 0x%x par3 0x%x]\n",
		    cmd->cmd_flags, cmd->cmd_numdesc, cmd->cmd_id, cmd->cmd_seq,
		    cmd->cmd_par1, cmd->cmd_par2, cmd->cmd_par3);
		break;
	case CMD_FLAGS_TYPE_RESP:
		/* response descriptor */
		printf("[rsp flags 0x%x num %d id %d seq %d par1 0x%x par2 0x%x par3 0x%x]\n",
		    rsp->rsp_flags, rsp->rsp_numdesc, rsp->rsp_id, rsp->rsp_seq,
		    rsp->rsp_par1, rsp->rsp_par2, rsp->rsp_par3);
		break;
	case CMD_FLAGS_TYPE_DATA:
		/* data header (assuming tx for now) */
		printf("[data flags 0x%x num %d totlen %d addr 0x%x/0x%x pflags 0x%x]",
		    txd->tx_flags, txd->tx_numdesc, txd->tx_totlen,
		    txd->tx_addrlo, txd->tx_addrhi, txd->tx_pflags);
		break;
	case CMD_FLAGS_TYPE_FRAG:
		/* fragment descriptor */
		printf("[frag flags 0x%x rsvd1 0x%x len %d addr 0x%x/0x%x rsvd2 0x%x]",
		    frgd->frag_flags, frgd->frag_rsvd1, frgd->frag_len,
		    frgd->frag_addrlo, frgd->frag_addrhi, frgd->frag_rsvd2);
		break;
	default:
		printf("[unknown(%x) flags 0x%x num %d id %d seq %d par1 0x%x par2 0x%x par3 0x%x]\n",
		    cmd->cmd_flags & CMD_FLAGS_TYPE_M,
		    cmd->cmd_flags, cmd->cmd_numdesc, cmd->cmd_id, cmd->cmd_seq,
		    cmd->cmd_par1, cmd->cmd_par2, cmd->cmd_par3);
		break;
	}
}
#endif

static void
txp_set_filter(sc)
	struct txp_softc *sc;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	u_int32_t crc, carry, hashbit, hash[2];
	u_int16_t filter;
	u_int8_t octet;
	int i, j, mcnt = 0;
	struct ifmultiaddr *ifma;
	char *enm;

	if (ifp->if_flags & IFF_PROMISC) {
		filter = TXP_RXFILT_PROMISC;
		goto setit;
	}

	filter = TXP_RXFILT_DIRECT;

	if (ifp->if_flags & IFF_BROADCAST)
		filter |= TXP_RXFILT_BROADCAST;

	if (ifp->if_flags & IFF_ALLMULTI)
		filter |= TXP_RXFILT_ALLMULTI;
	else {
		hash[0] = hash[1] = 0;

		for (ifma = ifp->if_multiaddrs.lh_first; ifma != NULL;
		    ifma = ifma->ifma_link.le_next) {
			if (ifma->ifma_addr->sa_family != AF_LINK)
				continue;

			enm = LLADDR((struct sockaddr_dl *)ifma->ifma_addr);
			mcnt++;
			crc = 0xffffffff;

			for (i = 0; i < ETHER_ADDR_LEN; i++) {
				octet = enm[i];
				for (j = 0; j < 8; j++) {
					carry = ((crc & 0x80000000) ? 1 : 0) ^
					    (octet & 1);
					crc <<= 1;
					octet >>= 1;
					if (carry)
						crc = (crc ^ TXP_POLYNOMIAL) |
						    carry;
				}
			}
			hashbit = (u_int16_t)(crc & (64 - 1));
			hash[hashbit / 32] |= (1 << hashbit % 32);
		}

		if (mcnt > 0) {
			filter |= TXP_RXFILT_HASHMULTI;
			txp_command(sc, TXP_CMD_MCAST_HASH_MASK_WRITE,
			    2, hash[0], hash[1], NULL, NULL, NULL, 0);
		}
	}

setit:

	txp_command(sc, TXP_CMD_RX_FILTER_WRITE, filter, 0, 0,
	    NULL, NULL, NULL, 1);

	return;
}

static void
txp_capabilities(sc)
	struct txp_softc *sc;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	struct txp_rsp_desc *rsp = NULL;
	struct txp_ext_desc *ext;

	if (txp_command2(sc, TXP_CMD_OFFLOAD_READ, 0, 0, 0, NULL, 0, &rsp, 1))
		goto out;

	if (rsp->rsp_numdesc != 1)
		goto out;
	ext = (struct txp_ext_desc *)(rsp + 1);

	sc->sc_tx_capability = ext->ext_1 & OFFLOAD_MASK;
	sc->sc_rx_capability = ext->ext_2 & OFFLOAD_MASK;

	if (rsp->rsp_par2 & rsp->rsp_par3 & OFFLOAD_VLAN) {
		sc->sc_tx_capability |= OFFLOAD_VLAN;
		sc->sc_rx_capability |= OFFLOAD_VLAN;
	}

#if 0
	/* not ready yet */
	if (rsp->rsp_par2 & rsp->rsp_par3 & OFFLOAD_IPSEC) {
		sc->sc_tx_capability |= OFFLOAD_IPSEC;
		sc->sc_rx_capability |= OFFLOAD_IPSEC;
		ifp->if_capabilities |= IFCAP_IPSEC;
	}
#endif

	if (rsp->rsp_par2 & rsp->rsp_par3 & OFFLOAD_IPCKSUM) {
		sc->sc_tx_capability |= OFFLOAD_IPCKSUM;
		sc->sc_rx_capability |= OFFLOAD_IPCKSUM;
		ifp->if_hwassist |= CSUM_IP;
	}

	if (rsp->rsp_par2 & rsp->rsp_par3 & OFFLOAD_TCPCKSUM) {
#if 0
		sc->sc_tx_capability |= OFFLOAD_TCPCKSUM;
#endif
		sc->sc_rx_capability |= OFFLOAD_TCPCKSUM;
#if 0
		ifp->if_capabilities |= CSUM_TCP;
#endif
	}

	if (rsp->rsp_par2 & rsp->rsp_par3 & OFFLOAD_UDPCKSUM) {
#if 0
		sc->sc_tx_capability |= OFFLOAD_UDPCKSUM;
#endif
		sc->sc_rx_capability |= OFFLOAD_UDPCKSUM;
#if 0
		ifp->if_capabilities |= CSUM_UDP;
#endif
	}

	if (txp_command(sc, TXP_CMD_OFFLOAD_WRITE, 0,
	    sc->sc_tx_capability, sc->sc_rx_capability, NULL, NULL, NULL, 1))
		goto out;

out:
	if (rsp != NULL)
		free(rsp, M_DEVBUF);

	return;
}
