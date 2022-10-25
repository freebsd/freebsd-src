/*-
 * Copyright (c) 2020 Michael J Karels
 * Copyright (c) 2016, 2020 Jared McNeill <jmcneill@invisible.ca>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * RPi4 (BCM 2711) Gigabit Ethernet ("GENET") controller
 *
 * This driver is derived in large part from bcmgenet.c from NetBSD by
 * Jared McNeill.  Parts of the structure and other common code in
 * this driver have been copied from if_awg.c for the Allwinner EMAC,
 * also by Jared McNeill.
 */

#include "opt_device_polling.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/kernel.h>
#include <sys/endian.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/module.h>
#include <sys/taskqueue.h>
#include <sys/gpio.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_var.h>

#include <machine/bus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#define __BIT(_x)	(1 << (_x))
#include "if_genetreg.h"

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mii/mii_fdt.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>

#include "syscon_if.h"
#include "miibus_if.h"
#include "gpio_if.h"

#define	RD4(sc, reg)		bus_read_4((sc)->res[_RES_MAC], (reg))
#define	WR4(sc, reg, val)	bus_write_4((sc)->res[_RES_MAC], (reg), (val))

#define	GEN_LOCK(sc)		mtx_lock(&(sc)->mtx)
#define	GEN_UNLOCK(sc)		mtx_unlock(&(sc)->mtx)
#define	GEN_ASSERT_LOCKED(sc)	mtx_assert(&(sc)->mtx, MA_OWNED)
#define	GEN_ASSERT_UNLOCKED(sc)	mtx_assert(&(sc)->mtx, MA_NOTOWNED)

#define	TX_DESC_COUNT		GENET_DMA_DESC_COUNT
#define	RX_DESC_COUNT		GENET_DMA_DESC_COUNT

#define	TX_NEXT(n, count)		(((n) + 1) & ((count) - 1))
#define	RX_NEXT(n, count)		(((n) + 1) & ((count) - 1))

#define	TX_MAX_SEGS		20

static SYSCTL_NODE(_hw, OID_AUTO, genet, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "genet driver parameters");

/* Maximum number of mbufs to pass per call to if_input */
static int gen_rx_batch = 16 /* RX_BATCH_DEFAULT */;
SYSCTL_INT(_hw_genet, OID_AUTO, rx_batch, CTLFLAG_RDTUN,
    &gen_rx_batch, 0, "max mbufs per call to if_input");

TUNABLE_INT("hw.gen.rx_batch", &gen_rx_batch);	/* old name/interface */

/*
 * Transmitting packets with only an Ethernet header in the first mbuf
 * fails.  Examples include reflected ICMPv6 packets, e.g. echo replies;
 * forwarded IPv6/TCP packets; and forwarded IPv4/TCP packets that use NAT
 * with IPFW.  Pulling up the sizes of ether_header + ip6_hdr + icmp6_hdr
 * seems to work for both ICMPv6 and TCP over IPv6, as well as the IPv4/TCP
 * case.
 */
static int gen_tx_hdr_min = 56;		/* ether_header + ip6_hdr + icmp6_hdr */
SYSCTL_INT(_hw_genet, OID_AUTO, tx_hdr_min, CTLFLAG_RW,
    &gen_tx_hdr_min, 0, "header to add to packets with ether header only");

static struct ofw_compat_data compat_data[] = {
	{ "brcm,genet-v1",		1 },
	{ "brcm,genet-v2",		2 },
	{ "brcm,genet-v3",		3 },
	{ "brcm,genet-v4",		4 },
	{ "brcm,genet-v5",		5 },
	{ "brcm,bcm2711-genet-v5",	5 },
	{ NULL,				0 }
};

enum {
	_RES_MAC,		/* what to call this? */
	_RES_IRQ1,
	_RES_IRQ2,
	_RES_NITEMS
};

static struct resource_spec gen_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		1,	RF_ACTIVE },
	{ -1, 0 }
};

/* structure per ring entry */
struct gen_ring_ent {
	bus_dmamap_t		map;
	struct mbuf		*mbuf;
};

struct tx_queue {
	int			hwindex;		/* hardware index */
	int			nentries;
	u_int			queued;			/* or avail? */
	u_int			cur;
	u_int			next;
	u_int			prod_idx;
	u_int			cons_idx;
	struct gen_ring_ent	*entries;
};

struct rx_queue {
	int			hwindex;		/* hardware index */
	int			nentries;
	u_int			cur;
	u_int			prod_idx;
	u_int			cons_idx;
	struct gen_ring_ent	*entries;
};

struct gen_softc {
	struct resource		*res[_RES_NITEMS];
	struct mtx		mtx;
	if_t			ifp;
	device_t		dev;
	device_t		miibus;
	mii_contype_t		phy_mode;

	struct callout		stat_ch;
	struct task		link_task;
	void			*ih;
	void			*ih2;
	int			type;
	int			if_flags;
	int			link;
	bus_dma_tag_t		tx_buf_tag;
	/*
	 * The genet chip has multiple queues for transmit and receive.
	 * This driver uses only one (queue 16, the default), but is cast
	 * with multiple rings.  The additional rings are used for different
	 * priorities.
	 */
#define DEF_TXQUEUE	0
#define NTXQUEUE	1
	struct tx_queue		tx_queue[NTXQUEUE];
	struct gen_ring_ent	tx_ring_ent[TX_DESC_COUNT];  /* ring entries */

	bus_dma_tag_t		rx_buf_tag;
#define DEF_RXQUEUE	0
#define NRXQUEUE	1
	struct rx_queue		rx_queue[NRXQUEUE];
	struct gen_ring_ent	rx_ring_ent[RX_DESC_COUNT];  /* ring entries */
};

static void gen_init(void *softc);
static void gen_start(if_t ifp);
static void gen_destroy(struct gen_softc *sc);
static int gen_encap(struct gen_softc *sc, struct mbuf **mp);
static int gen_parse_tx(struct mbuf *m, int csum_flags);
static int gen_ioctl(if_t ifp, u_long cmd, caddr_t data);
static int gen_get_phy_mode(device_t dev);
static bool gen_get_eaddr(device_t dev, struct ether_addr *eaddr);
static void gen_set_enaddr(struct gen_softc *sc);
static void gen_setup_rxfilter(struct gen_softc *sc);
static void gen_reset(struct gen_softc *sc);
static void gen_enable(struct gen_softc *sc);
static void gen_dma_disable(struct gen_softc *sc);
static int gen_bus_dma_init(struct gen_softc *sc);
static void gen_bus_dma_teardown(struct gen_softc *sc);
static void gen_enable_intr(struct gen_softc *sc);
static void gen_init_txrings(struct gen_softc *sc);
static void gen_init_rxrings(struct gen_softc *sc);
static void gen_intr(void *softc);
static int gen_rxintr(struct gen_softc *sc, struct rx_queue *q);
static void gen_txintr(struct gen_softc *sc, struct tx_queue *q);
static void gen_intr2(void *softc);
static int gen_newbuf_rx(struct gen_softc *sc, struct rx_queue *q, int index);
static int gen_mapbuf_rx(struct gen_softc *sc, struct rx_queue *q, int index,
    struct mbuf *m);
static void gen_link_task(void *arg, int pending);
static void gen_media_status(if_t ifp, struct ifmediareq *ifmr);
static int gen_media_change(if_t ifp);
static void gen_tick(void *softc);

static int
gen_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "RPi4 Gigabit Ethernet");
	return (BUS_PROBE_DEFAULT);
}

static int
gen_attach(device_t dev)
{
	struct ether_addr eaddr;
	struct gen_softc *sc;
	int major, minor, error, mii_flags;
	bool eaddr_found;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->type = ofw_bus_search_compatible(dev, compat_data)->ocd_data;

	if (bus_alloc_resources(dev, gen_spec, sc->res) != 0) {
		device_printf(dev, "cannot allocate resources for device\n");
		error = ENXIO;
		goto fail;
	}

	major = (RD4(sc, GENET_SYS_REV_CTRL) & REV_MAJOR) >> REV_MAJOR_SHIFT;
	if (major != REV_MAJOR_V5) {
		device_printf(dev, "version %d is not supported\n", major);
		error = ENXIO;
		goto fail;
	}
	minor = (RD4(sc, GENET_SYS_REV_CTRL) & REV_MINOR) >> REV_MINOR_SHIFT;
	device_printf(dev, "GENET version 5.%d phy 0x%04x\n", minor,
		RD4(sc, GENET_SYS_REV_CTRL) & REV_PHY);

	mtx_init(&sc->mtx, device_get_nameunit(dev), MTX_NETWORK_LOCK, MTX_DEF);
	callout_init_mtx(&sc->stat_ch, &sc->mtx, 0);
	TASK_INIT(&sc->link_task, 0, gen_link_task, sc);

	error = gen_get_phy_mode(dev);
	if (error != 0)
		goto fail;

	bzero(&eaddr, sizeof(eaddr));
	eaddr_found = gen_get_eaddr(dev, &eaddr);

	/* reset core */
	gen_reset(sc);

	gen_dma_disable(sc);

	/* Setup DMA */
	error = gen_bus_dma_init(sc);
	if (error != 0) {
		device_printf(dev, "cannot setup bus dma\n");
		goto fail;
	}

	/* Setup ethernet interface */
	sc->ifp = if_alloc(IFT_ETHER);
	if_setsoftc(sc->ifp, sc);
	if_initname(sc->ifp, device_get_name(dev), device_get_unit(dev));
	if_setflags(sc->ifp, IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST);
	if_setstartfn(sc->ifp, gen_start);
	if_setioctlfn(sc->ifp, gen_ioctl);
	if_setinitfn(sc->ifp, gen_init);
	if_setsendqlen(sc->ifp, TX_DESC_COUNT - 1);
	if_setsendqready(sc->ifp);
#define GEN_CSUM_FEATURES	(CSUM_UDP | CSUM_TCP)
	if_sethwassist(sc->ifp, GEN_CSUM_FEATURES);
	if_setcapabilities(sc->ifp, IFCAP_VLAN_MTU | IFCAP_HWCSUM |
	    IFCAP_HWCSUM_IPV6);
	if_setcapenable(sc->ifp, if_getcapabilities(sc->ifp));

	/* Install interrupt handlers */
	error = bus_setup_intr(dev, sc->res[_RES_IRQ1],
	    INTR_TYPE_NET | INTR_MPSAFE, NULL, gen_intr, sc, &sc->ih);
	if (error != 0) {
		device_printf(dev, "cannot setup interrupt handler1\n");
		goto fail;
	}

	error = bus_setup_intr(dev, sc->res[_RES_IRQ2],
	    INTR_TYPE_NET | INTR_MPSAFE, NULL, gen_intr2, sc, &sc->ih2);
	if (error != 0) {
		device_printf(dev, "cannot setup interrupt handler2\n");
		goto fail;
	}

	/* Attach MII driver */
	mii_flags = 0;
	switch (sc->phy_mode)
	{
	case MII_CONTYPE_RGMII_ID:
		mii_flags |= MIIF_RX_DELAY | MIIF_TX_DELAY;
		break;
	case MII_CONTYPE_RGMII_RXID:
		mii_flags |= MIIF_RX_DELAY;
		break;
	case MII_CONTYPE_RGMII_TXID:
		mii_flags |= MIIF_TX_DELAY;
		break;
	default:
		break;
	}
	error = mii_attach(dev, &sc->miibus, sc->ifp, gen_media_change,
	    gen_media_status, BMSR_DEFCAPMASK, MII_PHY_ANY, MII_OFFSET_ANY,
	    mii_flags);
	if (error != 0) {
		device_printf(dev, "cannot attach PHY\n");
		goto fail;
	}

	/* If address was not found, create one based on the hostid and name. */
	if (eaddr_found == 0)
		ether_gen_addr(sc->ifp, &eaddr);
	/* Attach ethernet interface */
	ether_ifattach(sc->ifp, eaddr.octet);

fail:
	if (error)
		gen_destroy(sc);
	return (error);
}

/* Free resources after failed attach.  This is not a complete detach. */
static void
gen_destroy(struct gen_softc *sc)
{

	if (sc->miibus) {	/* can't happen */
		device_delete_child(sc->dev, sc->miibus);
		sc->miibus = NULL;
	}
	bus_teardown_intr(sc->dev, sc->res[_RES_IRQ1], sc->ih);
	bus_teardown_intr(sc->dev, sc->res[_RES_IRQ2], sc->ih2);
	gen_bus_dma_teardown(sc);
	callout_drain(&sc->stat_ch);
	if (mtx_initialized(&sc->mtx))
		mtx_destroy(&sc->mtx);
	bus_release_resources(sc->dev, gen_spec, sc->res);
	if (sc->ifp != NULL) {
		if_free(sc->ifp);
		sc->ifp = NULL;
	}
}

static int
gen_get_phy_mode(device_t dev)
{
	struct gen_softc *sc;
	phandle_t node;
	mii_contype_t type;
	int error = 0;

	sc = device_get_softc(dev);
	node = ofw_bus_get_node(dev);
	type = mii_fdt_get_contype(node);

	switch (type) {
	case MII_CONTYPE_RGMII:
	case MII_CONTYPE_RGMII_ID:
	case MII_CONTYPE_RGMII_RXID:
	case MII_CONTYPE_RGMII_TXID:
		sc->phy_mode = type;
		break;
	default:
		device_printf(dev, "unknown phy-mode '%s'\n",
		    mii_fdt_contype_to_name(type));
		error = ENXIO;
		break;
	}

	return (error);
}

static bool
gen_get_eaddr(device_t dev, struct ether_addr *eaddr)
{
	struct gen_softc *sc;
	uint32_t maclo, machi, val;
	phandle_t node;

	sc = device_get_softc(dev);

	node = ofw_bus_get_node(dev);
	if (OF_getprop(node, "mac-address", eaddr->octet,
	    ETHER_ADDR_LEN) != -1 ||
	    OF_getprop(node, "local-mac-address", eaddr->octet,
	    ETHER_ADDR_LEN) != -1 ||
	    OF_getprop(node, "address", eaddr->octet, ETHER_ADDR_LEN) != -1)
		return (true);

	device_printf(dev, "No Ethernet address found in fdt!\n");
	maclo = machi = 0;

	val = RD4(sc, GENET_SYS_RBUF_FLUSH_CTRL);
	if ((val & GENET_SYS_RBUF_FLUSH_RESET) == 0) {
		maclo = htobe32(RD4(sc, GENET_UMAC_MAC0));
		machi = htobe16(RD4(sc, GENET_UMAC_MAC1) & 0xffff);
	}

	if (maclo == 0 && machi == 0) {
		if (bootverbose)
			device_printf(dev,
			    "No Ethernet address found in controller\n");
		return (false);
	} else {
		eaddr->octet[0] = maclo & 0xff;
		eaddr->octet[1] = (maclo >> 8) & 0xff;
		eaddr->octet[2] = (maclo >> 16) & 0xff;
		eaddr->octet[3] = (maclo >> 24) & 0xff;
		eaddr->octet[4] = machi & 0xff;
		eaddr->octet[5] = (machi >> 8) & 0xff;
		return (true);
	}
}

static void
gen_reset(struct gen_softc *sc)
{
	uint32_t val;

	val = RD4(sc, GENET_SYS_RBUF_FLUSH_CTRL);
	val |= GENET_SYS_RBUF_FLUSH_RESET;
	WR4(sc, GENET_SYS_RBUF_FLUSH_CTRL, val);
	DELAY(10);

	val &= ~GENET_SYS_RBUF_FLUSH_RESET;
	WR4(sc, GENET_SYS_RBUF_FLUSH_CTRL, val);
	DELAY(10);

	WR4(sc, GENET_SYS_RBUF_FLUSH_CTRL, 0);
	DELAY(10);

	WR4(sc, GENET_UMAC_CMD, 0);
	WR4(sc, GENET_UMAC_CMD,
	    GENET_UMAC_CMD_LCL_LOOP_EN | GENET_UMAC_CMD_SW_RESET);
	DELAY(10);
	WR4(sc, GENET_UMAC_CMD, 0);

	WR4(sc, GENET_UMAC_MIB_CTRL, GENET_UMAC_MIB_RESET_RUNT |
	    GENET_UMAC_MIB_RESET_RX | GENET_UMAC_MIB_RESET_TX);
	WR4(sc, GENET_UMAC_MIB_CTRL, 0);
}

static void
gen_enable(struct gen_softc *sc)
{
	u_int val;

	WR4(sc, GENET_UMAC_MAX_FRAME_LEN, 1536);

	val = RD4(sc, GENET_RBUF_CTRL);
	val |= GENET_RBUF_ALIGN_2B;
	WR4(sc, GENET_RBUF_CTRL, val);

	WR4(sc, GENET_RBUF_TBUF_SIZE_CTRL, 1);

	/* Enable transmitter and receiver */
	val = RD4(sc, GENET_UMAC_CMD);
	val |= GENET_UMAC_CMD_TXEN;
	val |= GENET_UMAC_CMD_RXEN;
	WR4(sc, GENET_UMAC_CMD, val);

	/* Enable interrupts */
	gen_enable_intr(sc);
	WR4(sc, GENET_INTRL2_CPU_CLEAR_MASK,
	    GENET_IRQ_TXDMA_DONE | GENET_IRQ_RXDMA_DONE);
}

static void
gen_disable_intr(struct gen_softc *sc)
{
	/* Disable interrupts */
	WR4(sc, GENET_INTRL2_CPU_SET_MASK, 0xffffffff);
	WR4(sc, GENET_INTRL2_CPU_CLEAR_MASK, 0xffffffff);
}

static void
gen_disable(struct gen_softc *sc)
{
	uint32_t val;

	/* Stop receiver */
	val = RD4(sc, GENET_UMAC_CMD);
	val &= ~GENET_UMAC_CMD_RXEN;
	WR4(sc, GENET_UMAC_CMD, val);

	/* Stop transmitter */
	val = RD4(sc, GENET_UMAC_CMD);
	val &= ~GENET_UMAC_CMD_TXEN;
	WR4(sc, GENET_UMAC_CMD, val);

	/* Disable Interrupt */
	gen_disable_intr(sc);
}

static void
gen_enable_offload(struct gen_softc *sc)
{
	uint32_t check_ctrl, buf_ctrl;

	check_ctrl = RD4(sc, GENET_RBUF_CHECK_CTRL);
	buf_ctrl  = RD4(sc, GENET_RBUF_CTRL);
	if ((if_getcapenable(sc->ifp) & IFCAP_RXCSUM) != 0) {
		check_ctrl |= GENET_RBUF_CHECK_CTRL_EN;
		buf_ctrl |= GENET_RBUF_64B_EN;
	} else {
		check_ctrl &= ~GENET_RBUF_CHECK_CTRL_EN;
		buf_ctrl &= ~GENET_RBUF_64B_EN;
	}
	WR4(sc, GENET_RBUF_CHECK_CTRL, check_ctrl);
	WR4(sc, GENET_RBUF_CTRL, buf_ctrl);

	buf_ctrl  = RD4(sc, GENET_TBUF_CTRL);
	if ((if_getcapenable(sc->ifp) & (IFCAP_TXCSUM | IFCAP_TXCSUM_IPV6)) !=
	    0)
		buf_ctrl |= GENET_RBUF_64B_EN;
	else
		buf_ctrl &= ~GENET_RBUF_64B_EN;
	WR4(sc, GENET_TBUF_CTRL, buf_ctrl);
}

static void
gen_dma_disable(struct gen_softc *sc)
{
	int val;

	val = RD4(sc, GENET_TX_DMA_CTRL);
	val &= ~GENET_TX_DMA_CTRL_EN;
	val &= ~GENET_TX_DMA_CTRL_RBUF_EN(GENET_DMA_DEFAULT_QUEUE);
	WR4(sc, GENET_TX_DMA_CTRL, val);

	val = RD4(sc, GENET_RX_DMA_CTRL);
	val &= ~GENET_RX_DMA_CTRL_EN;
	val &= ~GENET_RX_DMA_CTRL_RBUF_EN(GENET_DMA_DEFAULT_QUEUE);
	WR4(sc, GENET_RX_DMA_CTRL, val);
}

static int
gen_bus_dma_init(struct gen_softc *sc)
{
	device_t dev = sc->dev;
	int i, error;

	error = bus_dma_tag_create(
	    bus_get_dma_tag(dev),	/* Parent tag */
	    4, 0,			/* alignment, boundary */
	    BUS_SPACE_MAXADDR_40BIT,	/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    MCLBYTES, TX_MAX_SEGS,	/* maxsize, nsegs */
	    MCLBYTES,			/* maxsegsize */
	    0,				/* flags */
	    NULL, NULL,			/* lockfunc, lockarg */
	    &sc->tx_buf_tag);
	if (error != 0) {
		device_printf(dev, "cannot create TX buffer tag\n");
		return (error);
	}

	for (i = 0; i < TX_DESC_COUNT; i++) {
		error = bus_dmamap_create(sc->tx_buf_tag, 0,
		    &sc->tx_ring_ent[i].map);
		if (error != 0) {
			device_printf(dev, "cannot create TX buffer map\n");
			return (error);
		}
	}

	error = bus_dma_tag_create(
	    bus_get_dma_tag(dev),	/* Parent tag */
	    4, 0,			/* alignment, boundary */
	    BUS_SPACE_MAXADDR_40BIT,	/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    MCLBYTES, 1,		/* maxsize, nsegs */
	    MCLBYTES,			/* maxsegsize */
	    0,				/* flags */
	    NULL, NULL,			/* lockfunc, lockarg */
	    &sc->rx_buf_tag);
	if (error != 0) {
		device_printf(dev, "cannot create RX buffer tag\n");
		return (error);
	}

	for (i = 0; i < RX_DESC_COUNT; i++) {
		error = bus_dmamap_create(sc->rx_buf_tag, 0,
		    &sc->rx_ring_ent[i].map);
		if (error != 0) {
			device_printf(dev, "cannot create RX buffer map\n");
			return (error);
		}
	}
	return (0);
}

static void
gen_bus_dma_teardown(struct gen_softc *sc)
{
	int i, error;

	if (sc->tx_buf_tag != NULL) {
		for (i = 0; i < TX_DESC_COUNT; i++) {
			error = bus_dmamap_destroy(sc->tx_buf_tag,
			    sc->tx_ring_ent[i].map);
			sc->tx_ring_ent[i].map = NULL;
			if (error)
				device_printf(sc->dev,
				    "%s: bus_dmamap_destroy failed: %d\n",
				    __func__, error);
		}
		error = bus_dma_tag_destroy(sc->tx_buf_tag);
		sc->tx_buf_tag = NULL;
		if (error)
			device_printf(sc->dev,
			    "%s: bus_dma_tag_destroy failed: %d\n", __func__,
			    error);
	}

	if (sc->tx_buf_tag != NULL) {
		for (i = 0; i < RX_DESC_COUNT; i++) {
			error = bus_dmamap_destroy(sc->rx_buf_tag,
			    sc->rx_ring_ent[i].map);
			sc->rx_ring_ent[i].map = NULL;
			if (error)
				device_printf(sc->dev,
				    "%s: bus_dmamap_destroy failed: %d\n",
				    __func__, error);
		}
		error = bus_dma_tag_destroy(sc->rx_buf_tag);
		sc->rx_buf_tag = NULL;
		if (error)
			device_printf(sc->dev,
			    "%s: bus_dma_tag_destroy failed: %d\n", __func__,
			    error);
	}
}

static void
gen_enable_intr(struct gen_softc *sc)
{

	WR4(sc, GENET_INTRL2_CPU_CLEAR_MASK,
	    GENET_IRQ_TXDMA_DONE | GENET_IRQ_RXDMA_DONE);
}

/*
 * "queue" is the software queue index (0-4); "qid" is the hardware index
 * (0-16).  "base" is the starting index in the ring array.
 */
static void
gen_init_txring(struct gen_softc *sc, int queue, int qid, int base,
    int nentries)
{
	struct tx_queue *q;
	uint32_t val;

	q = &sc->tx_queue[queue];
	q->entries = &sc->tx_ring_ent[base];
	q->hwindex = qid;
	q->nentries = nentries;

	/* TX ring */

	q->queued = 0;
	q->cons_idx = q->prod_idx = 0;

	WR4(sc, GENET_TX_SCB_BURST_SIZE, 0x08);

	WR4(sc, GENET_TX_DMA_READ_PTR_LO(qid), 0);
	WR4(sc, GENET_TX_DMA_READ_PTR_HI(qid), 0);
	WR4(sc, GENET_TX_DMA_CONS_INDEX(qid), 0);
	WR4(sc, GENET_TX_DMA_PROD_INDEX(qid), 0);
	WR4(sc, GENET_TX_DMA_RING_BUF_SIZE(qid),
	    (nentries << GENET_TX_DMA_RING_BUF_SIZE_DESC_SHIFT) |
	    (MCLBYTES & GENET_TX_DMA_RING_BUF_SIZE_BUF_LEN_MASK));
	WR4(sc, GENET_TX_DMA_START_ADDR_LO(qid), 0);
	WR4(sc, GENET_TX_DMA_START_ADDR_HI(qid), 0);
	WR4(sc, GENET_TX_DMA_END_ADDR_LO(qid),
	    TX_DESC_COUNT * GENET_DMA_DESC_SIZE / 4 - 1);
	WR4(sc, GENET_TX_DMA_END_ADDR_HI(qid), 0);
	WR4(sc, GENET_TX_DMA_MBUF_DONE_THRES(qid), 1);
	WR4(sc, GENET_TX_DMA_FLOW_PERIOD(qid), 0);
	WR4(sc, GENET_TX_DMA_WRITE_PTR_LO(qid), 0);
	WR4(sc, GENET_TX_DMA_WRITE_PTR_HI(qid), 0);

	WR4(sc, GENET_TX_DMA_RING_CFG, __BIT(qid));	/* enable */

	/* Enable transmit DMA */
	val = RD4(sc, GENET_TX_DMA_CTRL);
	val |= GENET_TX_DMA_CTRL_EN;
	val |= GENET_TX_DMA_CTRL_RBUF_EN(qid);
	WR4(sc, GENET_TX_DMA_CTRL, val);
}

/*
 * "queue" is the software queue index (0-4); "qid" is the hardware index
 * (0-16).  "base" is the starting index in the ring array.
 */
static void
gen_init_rxring(struct gen_softc *sc, int queue, int qid, int base,
    int nentries)
{
	struct rx_queue *q;
	uint32_t val;
	int i;

	q = &sc->rx_queue[queue];
	q->entries = &sc->rx_ring_ent[base];
	q->hwindex = qid;
	q->nentries = nentries;
	q->cons_idx = q->prod_idx = 0;

	WR4(sc, GENET_RX_SCB_BURST_SIZE, 0x08);

	WR4(sc, GENET_RX_DMA_WRITE_PTR_LO(qid), 0);
	WR4(sc, GENET_RX_DMA_WRITE_PTR_HI(qid), 0);
	WR4(sc, GENET_RX_DMA_PROD_INDEX(qid), 0);
	WR4(sc, GENET_RX_DMA_CONS_INDEX(qid), 0);
	WR4(sc, GENET_RX_DMA_RING_BUF_SIZE(qid),
	    (nentries << GENET_RX_DMA_RING_BUF_SIZE_DESC_SHIFT) |
	    (MCLBYTES & GENET_RX_DMA_RING_BUF_SIZE_BUF_LEN_MASK));
	WR4(sc, GENET_RX_DMA_START_ADDR_LO(qid), 0);
	WR4(sc, GENET_RX_DMA_START_ADDR_HI(qid), 0);
	WR4(sc, GENET_RX_DMA_END_ADDR_LO(qid),
	    RX_DESC_COUNT * GENET_DMA_DESC_SIZE / 4 - 1);
	WR4(sc, GENET_RX_DMA_END_ADDR_HI(qid), 0);
	WR4(sc, GENET_RX_DMA_XON_XOFF_THRES(qid),
	    (5 << GENET_RX_DMA_XON_XOFF_THRES_LO_SHIFT) | (RX_DESC_COUNT >> 4));
	WR4(sc, GENET_RX_DMA_READ_PTR_LO(qid), 0);
	WR4(sc, GENET_RX_DMA_READ_PTR_HI(qid), 0);

	WR4(sc, GENET_RX_DMA_RING_CFG, __BIT(qid));	/* enable */

	/* fill ring */
	for (i = 0; i < RX_DESC_COUNT; i++)
		gen_newbuf_rx(sc, &sc->rx_queue[DEF_RXQUEUE], i);

	/* Enable receive DMA */
	val = RD4(sc, GENET_RX_DMA_CTRL);
	val |= GENET_RX_DMA_CTRL_EN;
	val |= GENET_RX_DMA_CTRL_RBUF_EN(qid);
	WR4(sc, GENET_RX_DMA_CTRL, val);
}

static void
gen_init_txrings(struct gen_softc *sc)
{
	int base = 0;
#ifdef PRI_RINGS
	int i;

	/* init priority rings */
	for (i = 0; i < PRI_RINGS; i++) {
		gen_init_txring(sc, i, i, base, TX_DESC_PRICOUNT);
		sc->tx_queue[i].queue = i;
		base += TX_DESC_PRICOUNT;
		dma_ring_conf |= 1 << i;
		dma_control |= DMA_RENABLE(i);
	}
#endif

	/* init GENET_DMA_DEFAULT_QUEUE (16) */
	gen_init_txring(sc, DEF_TXQUEUE, GENET_DMA_DEFAULT_QUEUE, base,
	    TX_DESC_COUNT);
	sc->tx_queue[DEF_TXQUEUE].hwindex = GENET_DMA_DEFAULT_QUEUE;
}

static void
gen_init_rxrings(struct gen_softc *sc)
{
	int base = 0;
#ifdef PRI_RINGS
	int i;

	/* init priority rings */
	for (i = 0; i < PRI_RINGS; i++) {
		gen_init_rxring(sc, i, i, base, TX_DESC_PRICOUNT);
		sc->rx_queue[i].queue = i;
		base += TX_DESC_PRICOUNT;
		dma_ring_conf |= 1 << i;
		dma_control |= DMA_RENABLE(i);
	}
#endif

	/* init GENET_DMA_DEFAULT_QUEUE (16) */
	gen_init_rxring(sc, DEF_RXQUEUE, GENET_DMA_DEFAULT_QUEUE, base,
	    RX_DESC_COUNT);
	sc->rx_queue[DEF_RXQUEUE].hwindex = GENET_DMA_DEFAULT_QUEUE;

}

static void
gen_stop(struct gen_softc *sc)
{
	int i;
	struct gen_ring_ent *ent;

	GEN_ASSERT_LOCKED(sc);

	callout_stop(&sc->stat_ch);
	if_setdrvflagbits(sc->ifp, 0, IFF_DRV_RUNNING);
	gen_reset(sc);
	gen_disable(sc);
	gen_dma_disable(sc);

	/* Clear the tx/rx ring buffer */
	for (i = 0; i < TX_DESC_COUNT; i++) {
		ent = &sc->tx_ring_ent[i];
		if (ent->mbuf != NULL) {
			bus_dmamap_sync(sc->tx_buf_tag, ent->map,
				BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->tx_buf_tag, ent->map);
			m_freem(ent->mbuf);
			ent->mbuf = NULL;
		}
	}

	for (i = 0; i < RX_DESC_COUNT; i++) {
		ent = &sc->rx_ring_ent[i];
		if (ent->mbuf != NULL) {
			bus_dmamap_sync(sc->rx_buf_tag, ent->map,
			    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->rx_buf_tag, ent->map);
			m_freem(ent->mbuf);
			ent->mbuf = NULL;
		}
	}
}

static void
gen_init_locked(struct gen_softc *sc)
{
	struct mii_data *mii;
	if_t ifp;

	mii = device_get_softc(sc->miibus);
	ifp = sc->ifp;

	GEN_ASSERT_LOCKED(sc);

	if (if_getdrvflags(ifp) & IFF_DRV_RUNNING)
		return;

	switch (sc->phy_mode)
	{
	case MII_CONTYPE_RGMII:
	case MII_CONTYPE_RGMII_ID:
	case MII_CONTYPE_RGMII_RXID:
	case MII_CONTYPE_RGMII_TXID:
		WR4(sc, GENET_SYS_PORT_CTRL, GENET_SYS_PORT_MODE_EXT_GPHY);
		break;
	default:
		WR4(sc, GENET_SYS_PORT_CTRL, 0);
	}

	gen_set_enaddr(sc);

	/* Setup RX filter */
	gen_setup_rxfilter(sc);

	gen_init_txrings(sc);
	gen_init_rxrings(sc);
	gen_enable(sc);
	gen_enable_offload(sc);

	if_setdrvflagbits(ifp, IFF_DRV_RUNNING, IFF_DRV_OACTIVE);

	mii_mediachg(mii);
	callout_reset(&sc->stat_ch, hz, gen_tick, sc);
}

static void
gen_init(void *softc)
{
        struct gen_softc *sc;

        sc = softc;
	GEN_LOCK(sc);
	gen_init_locked(sc);
	GEN_UNLOCK(sc);
}

static uint8_t ether_broadcastaddr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

static void
gen_setup_rxfilter_mdf(struct gen_softc *sc, u_int n, const uint8_t *ea)
{
	uint32_t addr0 = (ea[0] << 8) | ea[1];
	uint32_t addr1 = (ea[2] << 24) | (ea[3] << 16) | (ea[4] << 8) | ea[5];

	WR4(sc, GENET_UMAC_MDF_ADDR0(n), addr0);
	WR4(sc, GENET_UMAC_MDF_ADDR1(n), addr1);
}

static u_int
gen_setup_multi(void *arg, struct sockaddr_dl *sdl, u_int count)
{
	struct gen_softc *sc = arg;

	/* "count + 2" to account for unicast and broadcast */
	gen_setup_rxfilter_mdf(sc, count + 2, LLADDR(sdl));
	return (1);		/* increment to count */
}

static void
gen_setup_rxfilter(struct gen_softc *sc)
{
	struct ifnet *ifp = sc->ifp;
	uint32_t cmd, mdf_ctrl;
	u_int n;

	GEN_ASSERT_LOCKED(sc);

	cmd = RD4(sc, GENET_UMAC_CMD);

	/*
	 * Count the required number of hardware filters. We need one
	 * for each multicast address, plus one for our own address and
	 * the broadcast address.
	 */
	n = if_llmaddr_count(ifp) + 2;

	if (n > GENET_MAX_MDF_FILTER)
		ifp->if_flags |= IFF_ALLMULTI;
	else
		ifp->if_flags &= ~IFF_ALLMULTI;

	if ((ifp->if_flags & (IFF_PROMISC|IFF_ALLMULTI)) != 0) {
		cmd |= GENET_UMAC_CMD_PROMISC;
		mdf_ctrl = 0;
	} else {
		cmd &= ~GENET_UMAC_CMD_PROMISC;
		gen_setup_rxfilter_mdf(sc, 0, ether_broadcastaddr);
		gen_setup_rxfilter_mdf(sc, 1, IF_LLADDR(ifp));
		(void) if_foreach_llmaddr(ifp, gen_setup_multi, sc);
		mdf_ctrl = (__BIT(GENET_MAX_MDF_FILTER) - 1)  &~
		    (__BIT(GENET_MAX_MDF_FILTER - n) - 1);
	}

	WR4(sc, GENET_UMAC_CMD, cmd);
	WR4(sc, GENET_UMAC_MDF_CTRL, mdf_ctrl);
}

static void
gen_set_enaddr(struct gen_softc *sc)
{
	uint8_t *enaddr;
	uint32_t val;
	if_t ifp;

	GEN_ASSERT_LOCKED(sc);

	ifp = sc->ifp;

	/* Write our unicast address */
	enaddr = IF_LLADDR(ifp);
	/* Write hardware address */
	val = enaddr[3] | (enaddr[2] << 8) | (enaddr[1] << 16) |
	    (enaddr[0] << 24);
	WR4(sc, GENET_UMAC_MAC0, val);
	val = enaddr[5] | (enaddr[4] << 8);
	WR4(sc, GENET_UMAC_MAC1, val);
}

static void
gen_start_locked(struct gen_softc *sc)
{
	struct mbuf *m;
	if_t ifp;
	int err;

	GEN_ASSERT_LOCKED(sc);

	if (!sc->link)
		return;

	ifp = sc->ifp;

	if ((if_getdrvflags(ifp) & (IFF_DRV_RUNNING|IFF_DRV_OACTIVE)) !=
	    IFF_DRV_RUNNING)
		return;

	while (true) {
		m = if_dequeue(ifp);
		if (m == NULL)
			break;

		err = gen_encap(sc, &m);
		if (err != 0) {
			if (err == ENOBUFS)
				if_setdrvflagbits(ifp, IFF_DRV_OACTIVE, 0);
			else if (m == NULL)
				if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
			if (m != NULL)
				if_sendq_prepend(ifp, m);
			break;
		}
		if_bpfmtap(ifp, m);
	}
}

static void
gen_start(if_t ifp)
{
	struct gen_softc *sc;

	sc = if_getsoftc(ifp);

	GEN_LOCK(sc);
	gen_start_locked(sc);
	GEN_UNLOCK(sc);
}

/* Test for any delayed checksum */
#define CSUM_DELAY_ANY	(CSUM_TCP | CSUM_UDP | CSUM_IP6_TCP | CSUM_IP6_UDP)

static int
gen_encap(struct gen_softc *sc, struct mbuf **mp)
{
	bus_dmamap_t map;
	bus_dma_segment_t segs[TX_MAX_SEGS];
	int error, nsegs, cur, first, i, index, offset;
	uint32_t csuminfo, length_status, csum_flags = 0, csumdata;
	struct mbuf *m;
	struct statusblock *sb = NULL;
	struct tx_queue *q;
	struct gen_ring_ent *ent;

	GEN_ASSERT_LOCKED(sc);

	q = &sc->tx_queue[DEF_TXQUEUE];

	m = *mp;

	/*
	 * Don't attempt to send packets with only an Ethernet header in
	 * first mbuf; see comment above with gen_tx_hdr_min.
	 */
	if (m->m_len == sizeof(struct ether_header)) {
		m = m_pullup(m, MIN(m->m_pkthdr.len, gen_tx_hdr_min));
		if (m == NULL) {
			if (sc->ifp->if_flags & IFF_DEBUG)
				device_printf(sc->dev,
				    "header pullup fail\n");
			*mp = NULL;
			return (ENOMEM);
		}
	}

	if ((if_getcapenable(sc->ifp) & (IFCAP_TXCSUM | IFCAP_TXCSUM_IPV6)) !=
	    0) {
		csum_flags = m->m_pkthdr.csum_flags;
		csumdata = m->m_pkthdr.csum_data;
		M_PREPEND(m, sizeof(struct statusblock), M_NOWAIT);
		if (m == NULL) {
			if (sc->ifp->if_flags & IFF_DEBUG)
				device_printf(sc->dev, "prepend fail\n");
			*mp = NULL;
			return (ENOMEM);
		}
		offset = gen_parse_tx(m, csum_flags);
		sb = mtod(m, struct statusblock *);
		if ((csum_flags & CSUM_DELAY_ANY) != 0) {
			csuminfo = (offset << TXCSUM_OFF_SHIFT) |
			    (offset + csumdata);
			csuminfo |= TXCSUM_LEN_VALID;
			if (csum_flags & (CSUM_UDP | CSUM_IP6_UDP))
				csuminfo |= TXCSUM_UDP;
			sb->txcsuminfo = csuminfo;
		} else
			sb->txcsuminfo = 0;
	}

	*mp = m;

	cur = first = q->cur;
	ent = &q->entries[cur];
	map = ent->map;
	error = bus_dmamap_load_mbuf_sg(sc->tx_buf_tag, map, m, segs,
	    &nsegs, BUS_DMA_NOWAIT);
	if (error == EFBIG) {
		m = m_collapse(m, M_NOWAIT, TX_MAX_SEGS);
		if (m == NULL) {
			device_printf(sc->dev,
			    "gen_encap: m_collapse failed\n");
			m_freem(*mp);
			*mp = NULL;
			return (ENOMEM);
		}
		*mp = m;
		error = bus_dmamap_load_mbuf_sg(sc->tx_buf_tag, map, m,
		    segs, &nsegs, BUS_DMA_NOWAIT);
		if (error != 0) {
			m_freem(*mp);
			*mp = NULL;
		}
	}
	if (error != 0) {
		device_printf(sc->dev,
		    "gen_encap: bus_dmamap_load_mbuf_sg failed\n");
		return (error);
	}
	if (nsegs == 0) {
		m_freem(*mp);
		*mp = NULL;
		return (EIO);
	}

	/* Remove statusblock after mapping, before possible requeue or bpf. */
	if (sb != NULL) {
		m->m_data += sizeof(struct statusblock);
		m->m_len -= sizeof(struct statusblock);
		m->m_pkthdr.len -= sizeof(struct statusblock);
	}
	if (q->queued + nsegs > q->nentries) {
		bus_dmamap_unload(sc->tx_buf_tag, map);
		return (ENOBUFS);
	}

	bus_dmamap_sync(sc->tx_buf_tag, map, BUS_DMASYNC_PREWRITE);

	index = q->prod_idx & (q->nentries - 1);
	for (i = 0; i < nsegs; i++) {
		ent = &q->entries[cur];
		length_status = GENET_TX_DESC_STATUS_QTAG_MASK;
		if (i == 0) {
			length_status |= GENET_TX_DESC_STATUS_SOP |
			    GENET_TX_DESC_STATUS_CRC;
			if ((csum_flags & CSUM_DELAY_ANY) != 0)
				length_status |= GENET_TX_DESC_STATUS_CKSUM;
		}
		if (i == nsegs - 1)
			length_status |= GENET_TX_DESC_STATUS_EOP;

		length_status |= segs[i].ds_len <<
		    GENET_TX_DESC_STATUS_BUFLEN_SHIFT;

		WR4(sc, GENET_TX_DESC_ADDRESS_LO(index),
		    (uint32_t)segs[i].ds_addr);
		WR4(sc, GENET_TX_DESC_ADDRESS_HI(index),
		    (uint32_t)(segs[i].ds_addr >> 32));
		WR4(sc, GENET_TX_DESC_STATUS(index), length_status);

		++q->queued;
		cur = TX_NEXT(cur, q->nentries);
		index = TX_NEXT(index, q->nentries);
	}

	q->prod_idx += nsegs;
	q->prod_idx &= GENET_TX_DMA_PROD_CONS_MASK;
	/* We probably don't need to write the producer index on every iter */
	if (nsegs != 0)
		WR4(sc, GENET_TX_DMA_PROD_INDEX(q->hwindex), q->prod_idx);
	q->cur = cur;

	/* Store mbuf in the last segment */
	q->entries[first].mbuf = m;

	return (0);
}

/*
 * Parse a packet to find the offset of the transport header for checksum
 * offload.  Ensure that the link and network headers are contiguous with
 * the status block, or transmission fails.
 */
static int
gen_parse_tx(struct mbuf *m, int csum_flags)
{
	int offset, off_in_m;
	bool copy = false, shift = false;
	u_char *p, *copy_p = NULL;
	struct mbuf *m0 = m;
	uint16_t ether_type;

	if (m->m_len == sizeof(struct statusblock)) {
		/* M_PREPEND placed statusblock at end; move to beginning */
		m->m_data = m->m_pktdat;
		copy_p = mtodo(m, sizeof(struct statusblock));
		m = m->m_next;
		off_in_m = 0;
		p = mtod(m, u_char *);
		copy = true;
	} else {
		/*
		 * If statusblock is not at beginning of mbuf (likely),
		 * then remember to move mbuf contents down before copying
		 * after them.
		 */
		if ((m->m_flags & M_EXT) == 0 && m->m_data != m->m_pktdat)
			shift = true;
		p = mtodo(m, sizeof(struct statusblock));
		off_in_m = sizeof(struct statusblock);
	}

/*
 * If headers need to be copied contiguous to statusblock, do so.
 * If copying to the internal mbuf data area, and the status block
 * is not at the beginning of that area, shift the status block (which
 * is empty) and following data.
 */
#define COPY(size) {							\
	int hsize = size;						\
	if (copy) {							\
		if (shift) {						\
			u_char *p0;					\
			shift = false;					\
			p0 = mtodo(m0, sizeof(struct statusblock));	\
			m0->m_data = m0->m_pktdat;			\
			bcopy(p0, mtodo(m0, sizeof(struct statusblock)),\
			    m0->m_len - sizeof(struct statusblock));	\
			copy_p = mtodo(m0, m0->m_len);			\
		}							\
		bcopy(p, copy_p, hsize);				\
		m0->m_len += hsize;					\
		m->m_len -= hsize;					\
		m->m_data += hsize;					\
	}								\
	copy_p += hsize;						\
}

	KASSERT((sizeof(struct statusblock) + sizeof(struct ether_vlan_header) +
	    sizeof(struct ip6_hdr) <= MLEN), ("%s: mbuf too small", __func__));

	if (((struct ether_header *)p)->ether_type == htons(ETHERTYPE_VLAN)) {
		offset = sizeof(struct ether_vlan_header);
		ether_type = ntohs(((struct ether_vlan_header *)p)->evl_proto);
		COPY(sizeof(struct ether_vlan_header));
		if (m->m_len == off_in_m + sizeof(struct ether_vlan_header)) {
			m = m->m_next;
			off_in_m = 0;
			p = mtod(m, u_char *);
			copy = true;
		} else {
			off_in_m += sizeof(struct ether_vlan_header);
			p += sizeof(struct ether_vlan_header);
		}
	} else {
		offset = sizeof(struct ether_header);
		ether_type = ntohs(((struct ether_header *)p)->ether_type);
		COPY(sizeof(struct ether_header));
		if (m->m_len == off_in_m + sizeof(struct ether_header)) {
			m = m->m_next;
			off_in_m = 0;
			p = mtod(m, u_char *);
			copy = true;
		} else {
			off_in_m += sizeof(struct ether_header);
			p += sizeof(struct ether_header);
		}
	}
	if (ether_type == ETHERTYPE_IP) {
		COPY(((struct ip *)p)->ip_hl << 2);
		offset += ((struct ip *)p)->ip_hl << 2;
	} else if (ether_type == ETHERTYPE_IPV6) {
		COPY(sizeof(struct ip6_hdr));
		offset += sizeof(struct ip6_hdr);
	} else {
		/*
		 * Unknown whether most other cases require moving a header;
		 * ARP works without.  However, Wake On LAN packets sent
		 * by wake(8) via BPF need something like this.
		 */
		COPY(MIN(gen_tx_hdr_min, m->m_len));
		offset += MIN(gen_tx_hdr_min, m->m_len);
	}
	return (offset);
#undef COPY
}

static void
gen_intr(void *arg)
{
	struct gen_softc *sc = arg;
	uint32_t val;

	GEN_LOCK(sc);

	val = RD4(sc, GENET_INTRL2_CPU_STAT);
	val &= ~RD4(sc, GENET_INTRL2_CPU_STAT_MASK);
	WR4(sc, GENET_INTRL2_CPU_CLEAR, val);

	if (val & GENET_IRQ_RXDMA_DONE)
		gen_rxintr(sc, &sc->rx_queue[DEF_RXQUEUE]);

	if (val & GENET_IRQ_TXDMA_DONE) {
		gen_txintr(sc, &sc->tx_queue[DEF_TXQUEUE]);
		if (!if_sendq_empty(sc->ifp))
			gen_start_locked(sc);
	}

	GEN_UNLOCK(sc);
}

static int
gen_rxintr(struct gen_softc *sc, struct rx_queue *q)
{
	if_t ifp;
	struct mbuf *m, *mh, *mt;
	struct statusblock *sb = NULL;
	int error, index, len, cnt, npkt, n;
	uint32_t status, prod_idx, total;

	ifp = sc->ifp;
	mh = mt = NULL;
	cnt = 0;
	npkt = 0;

	prod_idx = RD4(sc, GENET_RX_DMA_PROD_INDEX(q->hwindex)) &
	    GENET_RX_DMA_PROD_CONS_MASK;
	total = (prod_idx - q->cons_idx) & GENET_RX_DMA_PROD_CONS_MASK;

	index = q->cons_idx & (RX_DESC_COUNT - 1);
	for (n = 0; n < total; n++) {
		bus_dmamap_sync(sc->rx_buf_tag, q->entries[index].map,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->rx_buf_tag, q->entries[index].map);

		m = q->entries[index].mbuf;

		if ((if_getcapenable(ifp) & IFCAP_RXCSUM) != 0) {
			sb = mtod(m, struct statusblock *);
			status = sb->status_buflen;
		} else
			status = RD4(sc, GENET_RX_DESC_STATUS(index));

		len = (status & GENET_RX_DESC_STATUS_BUFLEN_MASK) >>
		    GENET_RX_DESC_STATUS_BUFLEN_SHIFT;

		/* check for errors */
		if ((status &
		    (GENET_RX_DESC_STATUS_SOP | GENET_RX_DESC_STATUS_EOP |
		    GENET_RX_DESC_STATUS_RX_ERROR)) !=
		    (GENET_RX_DESC_STATUS_SOP | GENET_RX_DESC_STATUS_EOP)) {
			if (ifp->if_flags & IFF_DEBUG)
				device_printf(sc->dev,
				    "error/frag %x csum %x\n", status,
				    sb->rxcsum);
			if_inc_counter(ifp, IFCOUNTER_IERRORS, 1);
			continue;
		}

		error = gen_newbuf_rx(sc, q, index);
		if (error != 0) {
			if_inc_counter(ifp, IFCOUNTER_IQDROPS, 1);
			if (ifp->if_flags & IFF_DEBUG)
				device_printf(sc->dev, "gen_newbuf_rx %d\n",
				    error);
			/* reuse previous mbuf */
			(void) gen_mapbuf_rx(sc, q, index, m);
			continue;
		}

		if (sb != NULL) {
			if (status & GENET_RX_DESC_STATUS_CKSUM_OK) {
				/* L4 checksum checked; not sure about L3. */
				m->m_pkthdr.csum_flags = CSUM_DATA_VALID |
				    CSUM_PSEUDO_HDR;
				m->m_pkthdr.csum_data = 0xffff;
			}
			m->m_data += sizeof(struct statusblock);
			m->m_len -= sizeof(struct statusblock);
			len -= sizeof(struct statusblock);
		}
		if (len > ETHER_ALIGN) {
			m_adj(m, ETHER_ALIGN);
			len -= ETHER_ALIGN;
		}

		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = len;
		m->m_len = len;
		if_inc_counter(ifp, IFCOUNTER_IPACKETS, 1);

		m->m_nextpkt = NULL;
		if (mh == NULL)
			mh = m;
		else
			mt->m_nextpkt = m;
		mt = m;
		++cnt;
		++npkt;

		index = RX_NEXT(index, q->nentries);

		q->cons_idx = (q->cons_idx + 1) & GENET_RX_DMA_PROD_CONS_MASK;
		WR4(sc, GENET_RX_DMA_CONS_INDEX(q->hwindex), q->cons_idx);

		if (cnt == gen_rx_batch) {
			GEN_UNLOCK(sc);
			if_input(ifp, mh);
			GEN_LOCK(sc);
			mh = mt = NULL;
			cnt = 0;
		}
	}

	if (mh != NULL) {
		GEN_UNLOCK(sc);
		if_input(ifp, mh);
		GEN_LOCK(sc);
	}

	return (npkt);
}

static void
gen_txintr(struct gen_softc *sc, struct tx_queue *q)
{
	uint32_t cons_idx, total;
	struct gen_ring_ent *ent;
	if_t ifp;
	int i, prog;

	GEN_ASSERT_LOCKED(sc);

	ifp = sc->ifp;

	cons_idx = RD4(sc, GENET_TX_DMA_CONS_INDEX(q->hwindex)) &
	    GENET_TX_DMA_PROD_CONS_MASK;
	total = (cons_idx - q->cons_idx) & GENET_TX_DMA_PROD_CONS_MASK;

	prog = 0;
	for (i = q->next; q->queued > 0 && total > 0;
	    i = TX_NEXT(i, q->nentries), total--) {
		/* XXX check for errors */

		ent = &q->entries[i];
		if (ent->mbuf != NULL) {
			bus_dmamap_sync(sc->tx_buf_tag, ent->map,
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->tx_buf_tag, ent->map);
			m_freem(ent->mbuf);
			ent->mbuf = NULL;
			if_inc_counter(ifp, IFCOUNTER_OPACKETS, 1);
		}

		prog++;
		--q->queued;
	}

	if (prog > 0) {
		q->next = i;
		if_setdrvflagbits(ifp, 0, IFF_DRV_OACTIVE);
	}

	q->cons_idx = cons_idx;
}

static void
gen_intr2(void *arg)
{
	struct gen_softc *sc = arg;

	device_printf(sc->dev, "gen_intr2\n");
}

static int
gen_newbuf_rx(struct gen_softc *sc, struct rx_queue *q, int index)
{
	struct mbuf *m;

	m = m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR);
	if (m == NULL)
		return (ENOBUFS);

	m->m_pkthdr.len = m->m_len = m->m_ext.ext_size;
	m_adj(m, ETHER_ALIGN);

	return (gen_mapbuf_rx(sc, q, index, m));
}

static int
gen_mapbuf_rx(struct gen_softc *sc, struct rx_queue *q, int index,
    struct mbuf *m)
{
	bus_dma_segment_t seg;
	bus_dmamap_t map;
	int nsegs;

	map = q->entries[index].map;
	if (bus_dmamap_load_mbuf_sg(sc->rx_buf_tag, map, m, &seg, &nsegs,
	    BUS_DMA_NOWAIT) != 0) {
		m_freem(m);
		return (ENOBUFS);
	}

	bus_dmamap_sync(sc->rx_buf_tag, map, BUS_DMASYNC_PREREAD);

	q->entries[index].mbuf = m;
	WR4(sc, GENET_RX_DESC_ADDRESS_LO(index), (uint32_t)seg.ds_addr);
	WR4(sc, GENET_RX_DESC_ADDRESS_HI(index), (uint32_t)(seg.ds_addr >> 32));

	return (0);
}

static int
gen_ioctl(if_t ifp, u_long cmd, caddr_t data)
{
	struct gen_softc *sc;
	struct mii_data *mii;
	struct ifreq *ifr;
	int flags, enable, error;

	sc = if_getsoftc(ifp);
	mii = device_get_softc(sc->miibus);
	ifr = (struct ifreq *)data;
	error = 0;

	switch (cmd) {
	case SIOCSIFFLAGS:
		GEN_LOCK(sc);
		if (if_getflags(ifp) & IFF_UP) {
			if (if_getdrvflags(ifp) & IFF_DRV_RUNNING) {
				flags = if_getflags(ifp) ^ sc->if_flags;
				if ((flags & (IFF_PROMISC|IFF_ALLMULTI)) != 0)
					gen_setup_rxfilter(sc);
			} else
				gen_init_locked(sc);
		} else {
			if (if_getdrvflags(ifp) & IFF_DRV_RUNNING)
				gen_stop(sc);
		}
		sc->if_flags = if_getflags(ifp);
		GEN_UNLOCK(sc);
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (if_getdrvflags(ifp) & IFF_DRV_RUNNING) {
			GEN_LOCK(sc);
			gen_setup_rxfilter(sc);
			GEN_UNLOCK(sc);
		}
		break;

	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, cmd);
		break;

	case SIOCSIFCAP:
		enable = if_getcapenable(ifp);
		flags = ifr->ifr_reqcap ^ enable;
		if (flags & IFCAP_RXCSUM)
			enable ^= IFCAP_RXCSUM;
		if (flags & IFCAP_RXCSUM_IPV6)
			enable ^= IFCAP_RXCSUM_IPV6;
		if (flags & IFCAP_TXCSUM)
			enable ^= IFCAP_TXCSUM;
		if (flags & IFCAP_TXCSUM_IPV6)
			enable ^= IFCAP_TXCSUM_IPV6;
		if (enable & (IFCAP_TXCSUM | IFCAP_TXCSUM_IPV6))
			if_sethwassist(ifp, GEN_CSUM_FEATURES);
		else
			if_sethwassist(ifp, 0);
		if_setcapenable(ifp, enable);
		if (if_getdrvflags(ifp) & IFF_DRV_RUNNING)
			gen_enable_offload(sc);
		break;

	default:
		error = ether_ioctl(ifp, cmd, data);
		break;
	}
	return (error);
}

static void
gen_tick(void *softc)
{
	struct gen_softc *sc;
	struct mii_data *mii;
	if_t ifp;
	int link;

	sc = softc;
	ifp = sc->ifp;
	mii = device_get_softc(sc->miibus);

	GEN_ASSERT_LOCKED(sc);

	if ((if_getdrvflags(ifp) & IFF_DRV_RUNNING) == 0)
		return;

	link = sc->link;
	mii_tick(mii);
	if (sc->link && !link)
		gen_start_locked(sc);

	callout_reset(&sc->stat_ch, hz, gen_tick, sc);
}

#define	MII_BUSY_RETRY		1000

static int
gen_miibus_readreg(device_t dev, int phy, int reg)
{
	struct gen_softc *sc;
	int retry, val;

	sc = device_get_softc(dev);
	val = 0;

	WR4(sc, GENET_MDIO_CMD, GENET_MDIO_READ |
	    (phy << GENET_MDIO_ADDR_SHIFT) | (reg << GENET_MDIO_REG_SHIFT));
	val = RD4(sc, GENET_MDIO_CMD);
	WR4(sc, GENET_MDIO_CMD, val | GENET_MDIO_START_BUSY);
	for (retry = MII_BUSY_RETRY; retry > 0; retry--) {
		if (((val = RD4(sc, GENET_MDIO_CMD)) &
		    GENET_MDIO_START_BUSY) == 0) {
			if (val & GENET_MDIO_READ_FAILED)
				return (0);	/* -1? */
			val &= GENET_MDIO_VAL_MASK;
			break;
		}
		DELAY(10);
	}

	if (retry == 0)
		device_printf(dev, "phy read timeout, phy=%d reg=%d\n",
		    phy, reg);

	return (val);
}

static int
gen_miibus_writereg(device_t dev, int phy, int reg, int val)
{
	struct gen_softc *sc;
	int retry;

	sc = device_get_softc(dev);

	WR4(sc, GENET_MDIO_CMD, GENET_MDIO_WRITE |
	    (phy << GENET_MDIO_ADDR_SHIFT) | (reg << GENET_MDIO_REG_SHIFT) |
	    (val & GENET_MDIO_VAL_MASK));
	val = RD4(sc, GENET_MDIO_CMD);
	WR4(sc, GENET_MDIO_CMD, val | GENET_MDIO_START_BUSY);
	for (retry = MII_BUSY_RETRY; retry > 0; retry--) {
		val = RD4(sc, GENET_MDIO_CMD);
		if ((val & GENET_MDIO_START_BUSY) == 0)
			break;
		DELAY(10);
	}
	if (retry == 0)
		device_printf(dev, "phy write timeout, phy=%d reg=%d\n",
		    phy, reg);

	return (0);
}

static void
gen_update_link_locked(struct gen_softc *sc)
{
	struct mii_data *mii;
	uint32_t val;
	u_int speed;

	GEN_ASSERT_LOCKED(sc);

	if ((if_getdrvflags(sc->ifp) & IFF_DRV_RUNNING) == 0)
		return;
	mii = device_get_softc(sc->miibus);

	if ((mii->mii_media_status & (IFM_ACTIVE | IFM_AVALID)) ==
	    (IFM_ACTIVE | IFM_AVALID)) {
		switch (IFM_SUBTYPE(mii->mii_media_active)) {
		case IFM_1000_T:
		case IFM_1000_SX:
			speed = GENET_UMAC_CMD_SPEED_1000;
			sc->link = 1;
			break;
		case IFM_100_TX:
			speed = GENET_UMAC_CMD_SPEED_100;
			sc->link = 1;
			break;
		case IFM_10_T:
			speed = GENET_UMAC_CMD_SPEED_10;
			sc->link = 1;
			break;
		default:
			sc->link = 0;
			break;
		}
	} else
		sc->link = 0;

	if (sc->link == 0)
		return;

	val = RD4(sc, GENET_EXT_RGMII_OOB_CTRL);
	val &= ~GENET_EXT_RGMII_OOB_OOB_DISABLE;
	val |= GENET_EXT_RGMII_OOB_RGMII_LINK;
	val |= GENET_EXT_RGMII_OOB_RGMII_MODE_EN;
	if (sc->phy_mode == MII_CONTYPE_RGMII)
		val |= GENET_EXT_RGMII_OOB_ID_MODE_DISABLE;
	else
		val &= ~GENET_EXT_RGMII_OOB_ID_MODE_DISABLE;
	WR4(sc, GENET_EXT_RGMII_OOB_CTRL, val);

	val = RD4(sc, GENET_UMAC_CMD);
	val &= ~GENET_UMAC_CMD_SPEED;
	val |= speed;
	WR4(sc, GENET_UMAC_CMD, val);
}

static void
gen_link_task(void *arg, int pending)
{
	struct gen_softc *sc;

	sc = arg;

	GEN_LOCK(sc);
	gen_update_link_locked(sc);
	GEN_UNLOCK(sc);
}

static void
gen_miibus_statchg(device_t dev)
{
	struct gen_softc *sc;

	sc = device_get_softc(dev);

	taskqueue_enqueue(taskqueue_swi, &sc->link_task);
}

static void
gen_media_status(if_t ifp, struct ifmediareq *ifmr)
{
	struct gen_softc *sc;
	struct mii_data *mii;

	sc = if_getsoftc(ifp);
	mii = device_get_softc(sc->miibus);

	GEN_LOCK(sc);
	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
	GEN_UNLOCK(sc);
}

static int
gen_media_change(if_t ifp)
{
	struct gen_softc *sc;
	struct mii_data *mii;
	int error;

	sc = if_getsoftc(ifp);
	mii = device_get_softc(sc->miibus);

	GEN_LOCK(sc);
	error = mii_mediachg(mii);
	GEN_UNLOCK(sc);

	return (error);
}

static device_method_t gen_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		gen_probe),
	DEVMETHOD(device_attach,	gen_attach),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	gen_miibus_readreg),
	DEVMETHOD(miibus_writereg,	gen_miibus_writereg),
	DEVMETHOD(miibus_statchg,	gen_miibus_statchg),

	DEVMETHOD_END
};

static driver_t gen_driver = {
	"genet",
	gen_methods,
	sizeof(struct gen_softc),
};

DRIVER_MODULE(genet, simplebus, gen_driver, 0, 0);
DRIVER_MODULE(miibus, genet, miibus_driver, 0, 0);
MODULE_DEPEND(genet, ether, 1, 1, 1);
MODULE_DEPEND(genet, miibus, 1, 1, 1);
