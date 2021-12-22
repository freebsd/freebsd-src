/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2021 Alstom Group.
 * Copyright (c) 2021 Semihalf.
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/socket.h>
#include <sys/sockio.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_var.h>
#include <net/if_types.h>
#include <net/if_media.h>
#include <net/iflib.h>

#include <dev/enetc/enetc_hw.h>
#include <dev/enetc/enetc.h>
#include <dev/enetc/enetc_mdio.h>
#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "ifdi_if.h"
#include "miibus_if.h"

static device_register_t		enetc_register;

static ifdi_attach_pre_t		enetc_attach_pre;
static ifdi_attach_post_t		enetc_attach_post;
static ifdi_detach_t			enetc_detach;

static ifdi_tx_queues_alloc_t		enetc_tx_queues_alloc;
static ifdi_rx_queues_alloc_t		enetc_rx_queues_alloc;
static ifdi_queues_free_t		enetc_queues_free;

static ifdi_init_t			enetc_init;
static ifdi_stop_t			enetc_stop;

static ifdi_msix_intr_assign_t		enetc_msix_intr_assign;
static ifdi_tx_queue_intr_enable_t	enetc_tx_queue_intr_enable;
static ifdi_rx_queue_intr_enable_t	enetc_rx_queue_intr_enable;
static ifdi_intr_enable_t		enetc_intr_enable;
static ifdi_intr_disable_t		enetc_intr_disable;

static int	enetc_isc_txd_encap(void*, if_pkt_info_t);
static void	enetc_isc_txd_flush(void*, uint16_t, qidx_t);
static int	enetc_isc_txd_credits_update(void*, uint16_t, bool);
static int	enetc_isc_rxd_available(void*, uint16_t, qidx_t, qidx_t);
static int	enetc_isc_rxd_pkt_get(void*, if_rxd_info_t);
static void	enetc_isc_rxd_refill(void*, if_rxd_update_t);
static void	enetc_isc_rxd_flush(void*, uint16_t, uint8_t, qidx_t);

static void	enetc_vlan_register(if_ctx_t, uint16_t);
static void	enetc_vlan_unregister(if_ctx_t, uint16_t);

static uint64_t	enetc_get_counter(if_ctx_t, ift_counter);
static int	enetc_promisc_set(if_ctx_t, int);
static int	enetc_mtu_set(if_ctx_t, uint32_t);
static void	enetc_setup_multicast(if_ctx_t);
static void	enetc_timer(if_ctx_t, uint16_t);
static void	enetc_update_admin_status(if_ctx_t);

static miibus_readreg_t		enetc_miibus_readreg;
static miibus_writereg_t	enetc_miibus_writereg;
static miibus_linkchg_t		enetc_miibus_linkchg;
static miibus_statchg_t		enetc_miibus_statchg;

static int			enetc_media_change(if_t);
static void			enetc_media_status(if_t, struct ifmediareq*);

static int			enetc_fixed_media_change(if_t);
static void			enetc_fixed_media_status(if_t, struct ifmediareq*);

static void			enetc_max_nqueues(struct enetc_softc*, int*, int*);
static int			enetc_setup_phy(struct enetc_softc*);

static void			enetc_get_hwaddr(struct enetc_softc*);
static void			enetc_set_hwaddr(struct enetc_softc*);
static int			enetc_setup_rss(struct enetc_softc*);

static void			enetc_init_hw(struct enetc_softc*);
static void			enetc_init_ctrl(struct enetc_softc*);
static void			enetc_init_tx(struct enetc_softc*);
static void			enetc_init_rx(struct enetc_softc*);

static int			enetc_ctrl_send(struct enetc_softc*,
				    uint16_t, uint16_t, iflib_dma_info_t);

static const char enetc_driver_version[] = "1.0.0";

static pci_vendor_info_t enetc_vendor_info_array[] = {
	PVID(PCI_VENDOR_FREESCALE, ENETC_DEV_ID_PF,
	    "Freescale ENETC PCIe Gigabit Ethernet Controller"),
	PVID_END
};

#define ENETC_IFCAPS (IFCAP_VLAN_MTU | IFCAP_RXCSUM | IFCAP_JUMBO_MTU | \
	IFCAP_VLAN_HWTAGGING | IFCAP_VLAN_HWFILTER)

static device_method_t enetc_methods[] = {
	DEVMETHOD(device_register,	enetc_register),
	DEVMETHOD(device_probe,		iflib_device_probe),
	DEVMETHOD(device_attach,	iflib_device_attach),
	DEVMETHOD(device_detach,	iflib_device_detach),
	DEVMETHOD(device_shutdown,	iflib_device_shutdown),
	DEVMETHOD(device_suspend,	iflib_device_suspend),
	DEVMETHOD(device_resume,	iflib_device_resume),

	DEVMETHOD(miibus_readreg,	enetc_miibus_readreg),
	DEVMETHOD(miibus_writereg,	enetc_miibus_writereg),
	DEVMETHOD(miibus_linkchg,	enetc_miibus_linkchg),
	DEVMETHOD(miibus_statchg,	enetc_miibus_statchg),

	DEVMETHOD(bus_setup_intr,		bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,		bus_generic_teardown_intr),
	DEVMETHOD(bus_release_resource,		bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource,	bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource,	bus_generic_deactivate_resource),
	DEVMETHOD(bus_adjust_resource,		bus_generic_adjust_resource),
	DEVMETHOD(bus_alloc_resource,		bus_generic_alloc_resource),

	DEVMETHOD_END
};

static driver_t enetc_driver = {
	"enetc", enetc_methods, sizeof(struct enetc_softc)
};

static devclass_t enetc_devclass;
DRIVER_MODULE(miibus, enetc, miibus_fdt_driver, miibus_fdt_devclass, NULL, NULL);
/* Make sure miibus gets procesed first. */
DRIVER_MODULE_ORDERED(enetc, pci, enetc_driver, enetc_devclass, NULL, NULL,
    SI_ORDER_ANY);
MODULE_VERSION(enetc, 1);

IFLIB_PNP_INFO(pci, enetc, enetc_vendor_info_array);

MODULE_DEPEND(enetc, ether, 1, 1, 1);
MODULE_DEPEND(enetc, iflib, 1, 1, 1);
MODULE_DEPEND(enetc, miibus, 1, 1, 1);

static device_method_t enetc_iflib_methods[] = {
	DEVMETHOD(ifdi_attach_pre,		enetc_attach_pre),
	DEVMETHOD(ifdi_attach_post,		enetc_attach_post),
	DEVMETHOD(ifdi_detach,			enetc_detach),

	DEVMETHOD(ifdi_init,			enetc_init),
	DEVMETHOD(ifdi_stop,			enetc_stop),

	DEVMETHOD(ifdi_tx_queues_alloc,		enetc_tx_queues_alloc),
	DEVMETHOD(ifdi_rx_queues_alloc,		enetc_rx_queues_alloc),
	DEVMETHOD(ifdi_queues_free,		enetc_queues_free),

	DEVMETHOD(ifdi_msix_intr_assign,	enetc_msix_intr_assign),
	DEVMETHOD(ifdi_tx_queue_intr_enable,	enetc_tx_queue_intr_enable),
	DEVMETHOD(ifdi_rx_queue_intr_enable,	enetc_rx_queue_intr_enable),
	DEVMETHOD(ifdi_intr_enable,		enetc_intr_enable),
	DEVMETHOD(ifdi_intr_disable,		enetc_intr_disable),

	DEVMETHOD(ifdi_vlan_register,		enetc_vlan_register),
	DEVMETHOD(ifdi_vlan_unregister,		enetc_vlan_unregister),

	DEVMETHOD(ifdi_get_counter,		enetc_get_counter),
	DEVMETHOD(ifdi_mtu_set,			enetc_mtu_set),
	DEVMETHOD(ifdi_multi_set,		enetc_setup_multicast),
	DEVMETHOD(ifdi_promisc_set,		enetc_promisc_set),
	DEVMETHOD(ifdi_timer,			enetc_timer),
	DEVMETHOD(ifdi_update_admin_status,	enetc_update_admin_status),

	DEVMETHOD_END
};

static driver_t enetc_iflib_driver = {
	"enetc", enetc_iflib_methods, sizeof(struct enetc_softc)
};

static struct if_txrx enetc_txrx = {
	.ift_txd_encap = enetc_isc_txd_encap,
	.ift_txd_flush = enetc_isc_txd_flush,
	.ift_txd_credits_update = enetc_isc_txd_credits_update,
	.ift_rxd_available = enetc_isc_rxd_available,
	.ift_rxd_pkt_get = enetc_isc_rxd_pkt_get,
	.ift_rxd_refill = enetc_isc_rxd_refill,
	.ift_rxd_flush = enetc_isc_rxd_flush
};

static struct if_shared_ctx enetc_sctx_init = {
	.isc_magic = IFLIB_MAGIC,

	.isc_q_align = ENETC_RING_ALIGN,

	.isc_tx_maxsize = ENETC_MAX_FRAME_LEN,
	.isc_tx_maxsegsize = PAGE_SIZE,

	.isc_rx_maxsize = ENETC_MAX_FRAME_LEN,
	.isc_rx_maxsegsize = ENETC_MAX_FRAME_LEN,
	.isc_rx_nsegments = ENETC_MAX_SCATTER,

	.isc_admin_intrcnt = 0,

	.isc_nfl = 1,
	.isc_nrxqs = 1,
	.isc_ntxqs = 1,

	.isc_vendor_info = enetc_vendor_info_array,
	.isc_driver_version = enetc_driver_version,
	.isc_driver = &enetc_iflib_driver,

	.isc_flags = IFLIB_DRIVER_MEDIA | IFLIB_PRESERVE_TX_INDICES,
	.isc_ntxd_min = {ENETC_MIN_DESC},
	.isc_ntxd_max = {ENETC_MAX_DESC},
	.isc_ntxd_default = {ENETC_DEFAULT_DESC},
	.isc_nrxd_min = {ENETC_MIN_DESC},
	.isc_nrxd_max = {ENETC_MAX_DESC},
	.isc_nrxd_default = {ENETC_DEFAULT_DESC}
};

static void*
enetc_register(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (NULL);

	return (&enetc_sctx_init);
}

static void
enetc_max_nqueues(struct enetc_softc *sc, int *max_tx_nqueues,
    int *max_rx_nqueues)
{
	uint32_t val;

	val = ENETC_PORT_RD4(sc, ENETC_PCAPR0);
	*max_tx_nqueues = MIN(ENETC_PCAPR0_TXBDR(val), ENETC_MAX_QUEUES);
	*max_rx_nqueues = MIN(ENETC_PCAPR0_RXBDR(val), ENETC_MAX_QUEUES);
}

static int
enetc_setup_fixed(struct enetc_softc *sc, phandle_t node)
{
	ssize_t size;
	int speed;

	size = OF_getencprop(node, "speed", &speed, sizeof(speed));
	if (size <= 0) {
		device_printf(sc->dev,
		    "Device has fixed-link node without link speed specified\n");
		return (ENXIO);
	}
	switch (speed) {
	case 10:
		speed = IFM_10_T;
		break;
	case 100:
		speed = IFM_100_TX;
		break;
	case 1000:
		speed = IFM_1000_T;
		break;
	case 2500:
		speed = IFM_2500_T;
		break;
	default:
		device_printf(sc->dev, "Unsupported link speed value of %d\n",
		    speed);
		return (ENXIO);
	}
	speed |= IFM_ETHER;

	if (OF_hasprop(node, "full-duplex"))
		speed |= IFM_FDX;
	else
		speed |= IFM_HDX;

	sc->fixed_link = true;

	ifmedia_init(&sc->fixed_ifmedia, 0, enetc_fixed_media_change,
	    enetc_fixed_media_status);
	ifmedia_add(&sc->fixed_ifmedia, speed, 0, NULL);
	ifmedia_set(&sc->fixed_ifmedia, speed);
	sc->shared->isc_media = &sc->fixed_ifmedia;

	return (0);
}

static int
enetc_setup_phy(struct enetc_softc *sc)
{
	phandle_t node, fixed_link, phy_handle;
	struct mii_data *miid;
	int phy_addr, error;
	ssize_t size;

	node = ofw_bus_get_node(sc->dev);
	fixed_link = ofw_bus_find_child(node, "fixed-link");
	if (fixed_link != 0)
		return (enetc_setup_fixed(sc, fixed_link));

	size = OF_getencprop(node, "phy-handle", &phy_handle, sizeof(phy_handle));
	if (size <= 0) {
		device_printf(sc->dev,
		    "Failed to acquire PHY handle from FDT.\n");
		return (ENXIO);
	}
	phy_handle = OF_node_from_xref(phy_handle);
	size = OF_getencprop(phy_handle, "reg", &phy_addr, sizeof(phy_addr));
	if (size <= 0) {
		device_printf(sc->dev, "Failed to obtain PHY address\n");
		return (ENXIO);
	}
	error = mii_attach(sc->dev, &sc->miibus, iflib_get_ifp(sc->ctx),
	    enetc_media_change, enetc_media_status,
	    BMSR_DEFCAPMASK, phy_addr, MII_OFFSET_ANY, MIIF_DOPAUSE);
	if (error != 0) {
		device_printf(sc->dev, "mii_attach failed\n");
		return (error);
	}
	miid = device_get_softc(sc->miibus);
	sc->shared->isc_media = &miid->mii_media;

	return (0);
}

static int
enetc_attach_pre(if_ctx_t ctx)
{
	struct ifnet *ifp;
	if_softc_ctx_t scctx;
	struct enetc_softc *sc;
	int error, rid;

	sc = iflib_get_softc(ctx);
	scctx = iflib_get_softc_ctx(ctx);
	sc->ctx = ctx;
	sc->dev = iflib_get_dev(ctx);
	sc->shared = scctx;
	ifp = iflib_get_ifp(ctx);

	mtx_init(&sc->mii_lock, "enetc_mdio", NULL, MTX_DEF);

	pci_save_state(sc->dev);
	pcie_flr(sc->dev, 1000, false);
	pci_restore_state(sc->dev);

	rid = PCIR_BAR(ENETC_BAR_REGS);
	sc->regs = bus_alloc_resource_any(sc->dev, SYS_RES_MEMORY, &rid, RF_ACTIVE);
	if (sc->regs == NULL) {
		device_printf(sc->dev,
		    "Failed to allocate BAR %d\n", ENETC_BAR_REGS);
		return (ENXIO);
	}

	error = iflib_dma_alloc_align(ctx,
	    ENETC_MIN_DESC * sizeof(struct enetc_cbd),
	    ENETC_RING_ALIGN,
	    &sc->ctrl_queue.dma,
	    0);
	if (error != 0) {
		device_printf(sc->dev, "Failed to allocate control ring\n");
		goto fail;
	}
	sc->ctrl_queue.ring = (struct enetc_cbd*)sc->ctrl_queue.dma.idi_vaddr;

	scctx->isc_txrx = &enetc_txrx;
	scctx->isc_tx_nsegments = ENETC_MAX_SCATTER;
	enetc_max_nqueues(sc, &scctx->isc_nrxqsets_max, &scctx->isc_ntxqsets_max);

	if (scctx->isc_ntxd[0] % ENETC_DESC_ALIGN != 0) {
		device_printf(sc->dev,
		    "The number of TX descriptors has to be a multiple of %d\n",
		    ENETC_DESC_ALIGN);
		error = EINVAL;
		goto fail;
	}
	if (scctx->isc_nrxd[0] % ENETC_DESC_ALIGN != 0) {
		device_printf(sc->dev,
		    "The number of RX descriptors has to be a multiple of %d\n",
		    ENETC_DESC_ALIGN);
		error = EINVAL;
		goto fail;
	}
	scctx->isc_txqsizes[0] = scctx->isc_ntxd[0] * sizeof(union enetc_tx_bd);
	scctx->isc_rxqsizes[0] = scctx->isc_nrxd[0] * sizeof(union enetc_rx_bd);
	scctx->isc_txd_size[0] = sizeof(union enetc_tx_bd);
	scctx->isc_rxd_size[0] = sizeof(union enetc_rx_bd);
	scctx->isc_tx_csum_flags = 0;
	scctx->isc_capabilities = scctx->isc_capenable = ENETC_IFCAPS;

	error = enetc_mtu_set(ctx, ETHERMTU);
	if (error != 0)
		goto fail;

	scctx->isc_msix_bar = pci_msix_table_bar(sc->dev);

	error = enetc_setup_phy(sc);
	if (error != 0)
		goto fail;

	enetc_get_hwaddr(sc);

	return (0);
fail:
	enetc_detach(ctx);
	return (error);
}

static int
enetc_attach_post(if_ctx_t ctx)
{

	enetc_init_hw(iflib_get_softc(ctx));
	return (0);
}

static int
enetc_detach(if_ctx_t ctx)
{
	struct enetc_softc *sc;
	int error = 0, i;

	sc = iflib_get_softc(ctx);

	for (i = 0; i < sc->rx_num_queues; i++)
		iflib_irq_free(ctx, &sc->rx_queues[i].irq);

	if (sc->miibus != NULL)
		device_delete_child(sc->dev, sc->miibus);

	if (sc->regs != NULL)
		error = bus_release_resource(sc->dev, SYS_RES_MEMORY,
		    rman_get_rid(sc->regs), sc->regs);

	if (sc->ctrl_queue.dma.idi_size != 0)
		iflib_dma_free(&sc->ctrl_queue.dma);

	mtx_destroy(&sc->mii_lock);

	return (error);
}

static int
enetc_tx_queues_alloc(if_ctx_t ctx, caddr_t *vaddrs, uint64_t *paddrs,
    int ntxqs, int ntxqsets)
{
	struct enetc_softc *sc;
	struct enetc_tx_queue *queue;
	int i;

	sc = iflib_get_softc(ctx);

	MPASS(ntxqs == 1);

	sc->tx_queues = mallocarray(sc->tx_num_queues,
	    sizeof(struct enetc_tx_queue), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (sc->tx_queues == NULL) {
		device_printf(sc->dev,
		    "Failed to allocate memory for TX queues.\n");
		return (ENOMEM);
	}

	for (i = 0; i < sc->tx_num_queues; i++) {
		queue = &sc->tx_queues[i];
		queue->sc = sc;
		queue->ring = (union enetc_tx_bd*)(vaddrs[i]);
		queue->ring_paddr = paddrs[i];
		queue->next_to_clean = 0;
		queue->ring_full = false;
	}

	return (0);
}

static int
enetc_rx_queues_alloc(if_ctx_t ctx, caddr_t *vaddrs, uint64_t *paddrs,
    int nrxqs, int nrxqsets)
{
	struct enetc_softc *sc;
	struct enetc_rx_queue *queue;
	int i;

	sc = iflib_get_softc(ctx);
	MPASS(nrxqs == 1);

	sc->rx_queues = mallocarray(sc->rx_num_queues,
	    sizeof(struct enetc_rx_queue), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (sc->rx_queues == NULL) {
		device_printf(sc->dev,
		    "Failed to allocate memory for RX queues.\n");
		return (ENOMEM);
	}

	for (i = 0; i < sc->rx_num_queues; i++) {
		queue = &sc->rx_queues[i];
		queue->sc = sc;
		queue->qid = i;
		queue->ring = (union enetc_rx_bd*)(vaddrs[i]);
		queue->ring_paddr = paddrs[i];
	}

	return (0);
}

static void
enetc_queues_free(if_ctx_t ctx)
{
	struct enetc_softc *sc;

	sc = iflib_get_softc(ctx);

	if (sc->tx_queues != NULL) {
		free(sc->tx_queues, M_DEVBUF);
		sc->tx_queues = NULL;
	}
	if (sc->rx_queues != NULL) {
		free(sc->rx_queues, M_DEVBUF);
		sc->rx_queues = NULL;
	}
}

static void
enetc_get_hwaddr(struct enetc_softc *sc)
{
	struct ether_addr hwaddr;
	uint16_t high;
	uint32_t low;

	low = ENETC_PORT_RD4(sc, ENETC_PSIPMAR0(0));
	high = ENETC_PORT_RD2(sc, ENETC_PSIPMAR1(0));

	memcpy(&hwaddr.octet[0], &low, 4);
	memcpy(&hwaddr.octet[4], &high, 2);

	if (ETHER_IS_BROADCAST(hwaddr.octet) ||
	    ETHER_IS_MULTICAST(hwaddr.octet) ||
	    ETHER_IS_ZERO(hwaddr.octet)) {
		ether_gen_addr(iflib_get_ifp(sc->ctx), &hwaddr);
		device_printf(sc->dev,
		    "Failed to obtain MAC address, using a random one\n");
		memcpy(&low, &hwaddr.octet[0], 4);
		memcpy(&high, &hwaddr.octet[4], 2);
	}

	iflib_set_mac(sc->ctx, hwaddr.octet);
}

static void
enetc_set_hwaddr(struct enetc_softc *sc)
{
	struct ifnet *ifp;
	uint16_t high;
	uint32_t low;
	uint8_t *hwaddr;

	ifp = iflib_get_ifp(sc->ctx);
	hwaddr = (uint8_t*)if_getlladdr(ifp);
	low = *((uint32_t*)hwaddr);
	high = *((uint16_t*)(hwaddr+4));

	ENETC_PORT_WR4(sc, ENETC_PSIPMAR0(0), low);
	ENETC_PORT_WR2(sc, ENETC_PSIPMAR1(0), high);
}

static int
enetc_setup_rss(struct enetc_softc *sc)
{
	struct iflib_dma_info dma;
	int error, i, buckets_num = 0;
	uint8_t *rss_table;
	uint32_t reg;

	reg = ENETC_RD4(sc, ENETC_SIPCAPR0);
	if (reg & ENETC_SIPCAPR0_RSS) {
		reg = ENETC_RD4(sc, ENETC_SIRSSCAPR);
		buckets_num = ENETC_SIRSSCAPR_GET_NUM_RSS(reg);
        }
	if (buckets_num == 0)
		return (ENOTSUP);

	for (i = 0; i < ENETC_RSSHASH_KEY_SIZE / sizeof(uint32_t); i++) {
		arc4rand((uint8_t *)&reg, sizeof(reg), 0);
		ENETC_PORT_WR4(sc, ENETC_PRSSK(i), reg);
	}

	ENETC_WR4(sc, ENETC_SIRBGCR, sc->rx_num_queues);

	error = iflib_dma_alloc_align(sc->ctx,
	    buckets_num * sizeof(*rss_table),
	    ENETC_RING_ALIGN,
	    &dma,
	    0);
	if (error != 0) {
		device_printf(sc->dev, "Failed to allocate DMA buffer for RSS\n");
		return (error);
	}
	rss_table = (uint8_t *)dma.idi_vaddr;

	for (i = 0; i < buckets_num; i++)
		rss_table[i] = i % sc->rx_num_queues;

	error = enetc_ctrl_send(sc, (BDCR_CMD_RSS << 8) | BDCR_CMD_RSS_WRITE,
	    buckets_num * sizeof(*rss_table), &dma);
	if (error != 0)
		device_printf(sc->dev, "Failed to setup RSS table\n");

	iflib_dma_free(&dma);

	return (error);
}

static int
enetc_ctrl_send(struct enetc_softc *sc, uint16_t cmd, uint16_t size,
    iflib_dma_info_t dma)
{
	struct enetc_ctrl_queue *queue;
	struct enetc_cbd *desc;
	int timeout = 1000;

	queue = &sc->ctrl_queue;
	desc = &queue->ring[queue->pidx];

	if (++queue->pidx == ENETC_MIN_DESC)
		queue->pidx = 0;

	desc->addr[0] = (uint32_t)dma->idi_paddr;
	desc->addr[1] = (uint32_t)(dma->idi_paddr >> 32);
	desc->index = 0;
	desc->length = (uint16_t)size;
	desc->cmd = (uint8_t)cmd;
	desc->cls = (uint8_t)(cmd >> 8);
	desc->status_flags = 0;

	/* Sync command packet, */
	bus_dmamap_sync(dma->idi_tag, dma->idi_map, BUS_DMASYNC_PREWRITE);
	/* and the control ring. */
	bus_dmamap_sync(queue->dma.idi_tag, queue->dma.idi_map, BUS_DMASYNC_PREWRITE);
	ENETC_WR4(sc, ENETC_SICBDRPIR, queue->pidx);

	while (--timeout != 0) {
		DELAY(20);
		if (ENETC_RD4(sc, ENETC_SICBDRCIR) == queue->pidx)
			break;
	}

	if (timeout == 0)
		return (ETIMEDOUT);

	bus_dmamap_sync(dma->idi_tag, dma->idi_map, BUS_DMASYNC_POSTREAD);
	return (0);
}

static void
enetc_init_hw(struct enetc_softc *sc)
{
	uint32_t val;
	int error;

	ENETC_PORT_WR4(sc, ENETC_PM0_CMD_CFG,
	    ENETC_PM0_CMD_TXP | ENETC_PM0_PROMISC |
	    ENETC_PM0_TX_EN | ENETC_PM0_RX_EN);
	ENETC_PORT_WR4(sc, ENETC_PM0_RX_FIFO, ENETC_PM0_RX_FIFO_VAL);
	val = ENETC_PSICFGR0_SET_TXBDR(sc->tx_num_queues);
	val |= ENETC_PSICFGR0_SET_RXBDR(sc->rx_num_queues);
	val |= ENETC_PSICFGR0_SIVC(ENETC_VLAN_TYPE_C | ENETC_VLAN_TYPE_S);
	ENETC_PORT_WR4(sc, ENETC_PSICFGR0(0), val);
	ENETC_PORT_WR4(sc, ENETC_PSIPVMR, ENETC_PSIPVMR_SET_VUTA(1));
	ENETC_PORT_WR4(sc, ENETC_PVCLCTR,  ENETC_VLAN_TYPE_C | ENETC_VLAN_TYPE_S);
	ENETC_PORT_WR4(sc, ENETC_PSIVLANFMR, ENETC_PSIVLANFMR_VS);
	ENETC_PORT_WR4(sc, ENETC_PAR_PORT_CFG, ENETC_PAR_PORT_L4CD);
	ENETC_PORT_WR4(sc, ENETC_PMR, ENETC_PMR_SI0EN | ENETC_PMR_PSPEED_1000M);

	ENETC_WR4(sc, ENETC_SICAR0,
	    ENETC_SICAR_RD_COHERENT | ENETC_SICAR_WR_COHERENT);
	ENETC_WR4(sc, ENETC_SICAR1, ENETC_SICAR_MSI);
	ENETC_WR4(sc, ENETC_SICAR2,
	    ENETC_SICAR_RD_COHERENT | ENETC_SICAR_WR_COHERENT);

	enetc_init_ctrl(sc);
	error = enetc_setup_rss(sc);
	if (error != 0)
		ENETC_WR4(sc, ENETC_SIMR, ENETC_SIMR_EN);
	else
		ENETC_WR4(sc, ENETC_SIMR, ENETC_SIMR_EN | ENETC_SIMR_RSSE);

}

static void
enetc_init_ctrl(struct enetc_softc *sc)
{
	struct enetc_ctrl_queue *queue = &sc->ctrl_queue;

	ENETC_WR4(sc, ENETC_SICBDRBAR0,
	    (uint32_t)queue->dma.idi_paddr);
	ENETC_WR4(sc, ENETC_SICBDRBAR1,
	    (uint32_t)(queue->dma.idi_paddr >> 32));
	ENETC_WR4(sc, ENETC_SICBDRLENR,
	    queue->dma.idi_size / sizeof(struct enetc_cbd));

	queue->pidx = 0;
	ENETC_WR4(sc, ENETC_SICBDRPIR, queue->pidx);
	ENETC_WR4(sc, ENETC_SICBDRCIR, queue->pidx);
	ENETC_WR4(sc, ENETC_SICBDRMR, ENETC_SICBDRMR_EN);
}

static void
enetc_init_tx(struct enetc_softc *sc)
{
	struct enetc_tx_queue *queue;
	int i;

	for (i = 0; i < sc->tx_num_queues; i++) {
		queue = &sc->tx_queues[i];

		ENETC_TXQ_WR4(sc, i, ENETC_TBBAR0,
		    (uint32_t)queue->ring_paddr);
		ENETC_TXQ_WR4(sc, i, ENETC_TBBAR1,
		    (uint32_t)(queue->ring_paddr >> 32));
		ENETC_TXQ_WR4(sc, i, ENETC_TBLENR, sc->tx_queue_size);

		/*
		 * Even though it is undoccumented resetting the TX ring
		 * indices results in TX hang.
		 * Do the same as Linux and simply keep those unchanged
		 * for the drivers lifetime.
		 */
#if 0
		ENETC_TXQ_WR4(sc, i, ENETC_TBPIR, 0);
		ENETC_TXQ_WR4(sc, i, ENETC_TBCIR, 0);
#endif
		ENETC_TXQ_WR4(sc, i, ENETC_TBMR, ENETC_TBMR_EN);
	}

}

static void
enetc_init_rx(struct enetc_softc *sc)
{
	struct enetc_rx_queue *queue;
	uint32_t rx_buf_size;
	int i;

	rx_buf_size = iflib_get_rx_mbuf_sz(sc->ctx);

	for (i = 0; i < sc->rx_num_queues; i++) {
		queue = &sc->rx_queues[i];

		ENETC_RXQ_WR4(sc, i, ENETC_RBBAR0,
		    (uint32_t)queue->ring_paddr);
		ENETC_RXQ_WR4(sc, i, ENETC_RBBAR1,
		    (uint32_t)(queue->ring_paddr >> 32));
		ENETC_RXQ_WR4(sc, i, ENETC_RBLENR, sc->rx_queue_size);
		ENETC_RXQ_WR4(sc, i, ENETC_RBBSR, rx_buf_size);
		ENETC_RXQ_WR4(sc, i, ENETC_RBPIR, 0);
		ENETC_RXQ_WR4(sc, i, ENETC_RBCIR, 0);
		queue->enabled = false;
	}
}

static u_int
enetc_hash_mac(void *arg, struct sockaddr_dl *sdl, u_int cnt)
{
	uint64_t *bitmap = arg;
	uint64_t address = 0;
	uint8_t hash = 0;
	bool bit;
	int i, j;

	bcopy(LLADDR(sdl), &address, ETHER_ADDR_LEN);

	/*
	 * The six bit hash is calculated by xoring every
	 * 6th bit of the address.
	 * It is then used as an index in a bitmap that is
	 * written to the device.
	 */
	for (i = 0; i < 6; i++) {
		bit = 0;
		for (j = 0; j < 8; j++)
			bit ^= !!(address & BIT(i + j*6));

		hash |= bit << i;
	}

	*bitmap |= (1 << hash);
	return (1);
}

static void
enetc_setup_multicast(if_ctx_t ctx)
{
	struct enetc_softc *sc;
	struct ifnet *ifp;
	uint64_t bitmap = 0;
	uint8_t revid;

	sc = iflib_get_softc(ctx);
	ifp = iflib_get_ifp(ctx);
	revid = pci_get_revid(sc->dev);

	if_foreach_llmaddr(ifp, enetc_hash_mac, &bitmap);

	/*
	 * In revid 1 of this chip the positions multicast and unicast
	 * hash filter registers are flipped.
	 */
	ENETC_PORT_WR4(sc, ENETC_PSIMMHFR0(0, revid == 1), bitmap & UINT32_MAX);
	ENETC_PORT_WR4(sc, ENETC_PSIMMHFR1(0), bitmap >> 32);

}

static uint8_t
enetc_hash_vid(uint16_t vid)
{
	uint8_t hash = 0;
	bool bit;
	int i;

	for (i = 0;i < 6;i++) {
		bit = vid & BIT(i);
		bit ^= !!(vid & BIT(i + 6));
		hash |= bit << i;
	}

	return (hash);
}

static void
enetc_vlan_register(if_ctx_t ctx, uint16_t vid)
{
	struct enetc_softc *sc;
	uint8_t hash;
	uint64_t bitmap;

	sc = iflib_get_softc(ctx);
	hash = enetc_hash_vid(vid);

	/* Check if hash is alredy present in the bitmap. */
	if (++sc->vlan_bitmap[hash] != 1)
		return;

	bitmap = ENETC_PORT_RD4(sc, ENETC_PSIVHFR0(0));
	bitmap |= (uint64_t)ENETC_PORT_RD4(sc, ENETC_PSIVHFR1(0)) << 32;
	bitmap |= BIT(hash);
	ENETC_PORT_WR4(sc, ENETC_PSIVHFR0(0), bitmap & UINT32_MAX);
	ENETC_PORT_WR4(sc, ENETC_PSIVHFR1(0), bitmap >> 32);
}

static void
enetc_vlan_unregister(if_ctx_t ctx, uint16_t vid)
{
	struct enetc_softc *sc;
	uint8_t hash;
	uint64_t bitmap;

	sc = iflib_get_softc(ctx);
	hash = enetc_hash_vid(vid);

	MPASS(sc->vlan_bitmap[hash] > 0);
	if (--sc->vlan_bitmap[hash] != 0)
		return;

	bitmap = ENETC_PORT_RD4(sc, ENETC_PSIVHFR0(0));
	bitmap |= (uint64_t)ENETC_PORT_RD4(sc, ENETC_PSIVHFR1(0)) << 32;
	bitmap &= ~BIT(hash);
	ENETC_PORT_WR4(sc, ENETC_PSIVHFR0(0), bitmap & UINT32_MAX);
	ENETC_PORT_WR4(sc, ENETC_PSIVHFR1(0), bitmap >> 32);
}

static void
enetc_init(if_ctx_t ctx)
{
	struct enetc_softc *sc;
	struct mii_data *miid;
	struct ifnet *ifp;
	uint16_t max_frame_length;
	int baudrate;

	sc = iflib_get_softc(ctx);
	ifp = iflib_get_ifp(ctx);

	max_frame_length = sc->shared->isc_max_frame_size;
	MPASS(max_frame_length < ENETC_MAX_FRAME_LEN);

	/* Set max RX and TX frame lengths. */
	ENETC_PORT_WR4(sc, ENETC_PM0_MAXFRM, max_frame_length);
	ENETC_PORT_WR4(sc, ENETC_PTCMSDUR(0), max_frame_length);
	ENETC_PORT_WR4(sc, ENETC_PTXMBAR, 2 * max_frame_length);

	/* Set "VLAN promiscious" mode if filtering is disabled. */
	if ((if_getcapenable(ifp) & IFCAP_VLAN_HWFILTER) == 0)
		ENETC_PORT_WR4(sc, ENETC_PSIPVMR,
		    ENETC_PSIPVMR_SET_VUTA(1) | ENETC_PSIPVMR_SET_VP(1));
	else
		ENETC_PORT_WR4(sc, ENETC_PSIPVMR,
		    ENETC_PSIPVMR_SET_VUTA(1));

	sc->rbmr = ENETC_RBMR_EN | ENETC_RBMR_AL;

	if (if_getcapenable(ifp) & IFCAP_VLAN_HWTAGGING)
		sc->rbmr |= ENETC_RBMR_VTE;

	/* Write MAC address to hardware. */
	enetc_set_hwaddr(sc);

	enetc_init_tx(sc);
	enetc_init_rx(sc);

	if (sc->fixed_link) {
		baudrate = ifmedia_baudrate(sc->fixed_ifmedia.ifm_cur->ifm_media);
		iflib_link_state_change(sc->ctx, LINK_STATE_UP, baudrate);
	} else {
		/*
		 * Can't return an error from this function, there is not much
		 * we can do if this fails.
		 */
		miid = device_get_softc(sc->miibus);
		(void)mii_mediachg(miid);
	}

	enetc_promisc_set(ctx, if_getflags(ifp));
}

static void
enetc_stop(if_ctx_t ctx)
{
	struct enetc_softc *sc;
	int i;

	sc = iflib_get_softc(ctx);

	for (i = 0; i < sc->tx_num_queues; i++)
		ENETC_TXQ_WR4(sc, i, ENETC_TBMR, 0);

	for (i = 0; i < sc->rx_num_queues; i++)
		ENETC_RXQ_WR4(sc, i, ENETC_RBMR, 0);
}

static int
enetc_msix_intr_assign(if_ctx_t ctx, int msix)
{
	struct enetc_softc *sc;
	struct enetc_rx_queue *rx_queue;
	struct enetc_tx_queue *tx_queue;
	int vector = 0, i, error;
	char irq_name[16];

	sc = iflib_get_softc(ctx);

	MPASS(sc->rx_num_queues + 1 <= ENETC_MSIX_COUNT);
	MPASS(sc->rx_num_queues == sc->tx_num_queues);

	for (i = 0; i < sc->rx_num_queues; i++, vector++) {
		rx_queue = &sc->rx_queues[i];
		snprintf(irq_name, sizeof(irq_name), "rxtxq%d", i);
		error = iflib_irq_alloc_generic(ctx,
		    &rx_queue->irq, vector + 1, IFLIB_INTR_RXTX,
		    NULL, rx_queue, i, irq_name);
		if (error != 0)
			goto fail;

		ENETC_WR4(sc, ENETC_SIMSIRRV(i), vector);
		ENETC_RXQ_WR4(sc, i, ENETC_RBICR1, ENETC_RX_INTR_TIME_THR);
		ENETC_RXQ_WR4(sc, i, ENETC_RBICR0,
		    ENETC_RBICR0_ICEN | ENETC_RBICR0_SET_ICPT(ENETC_RX_INTR_PKT_THR));
	}
	vector = 0;
	for (i = 0;i < sc->tx_num_queues; i++, vector++) {
		tx_queue = &sc->tx_queues[i];
		snprintf(irq_name, sizeof(irq_name), "txq%d", i);
		iflib_softirq_alloc_generic(ctx, &tx_queue->irq,
		    IFLIB_INTR_TX, tx_queue, i, irq_name);

		ENETC_WR4(sc, ENETC_SIMSITRV(i), vector);
	}

	return (0);
fail:
	for (i = 0; i < sc->rx_num_queues; i++) {
		rx_queue = &sc->rx_queues[i];
		iflib_irq_free(ctx, &rx_queue->irq);
	}
	return (error);
}

static int
enetc_tx_queue_intr_enable(if_ctx_t ctx, uint16_t qid)
{
	struct enetc_softc *sc;

	sc = iflib_get_softc(ctx);
	ENETC_TXQ_RD4(sc, qid, ENETC_TBIDR);
	return (0);
}

static int
enetc_rx_queue_intr_enable(if_ctx_t ctx, uint16_t qid)
{
	struct enetc_softc *sc;

	sc = iflib_get_softc(ctx);
	ENETC_RXQ_RD4(sc, qid, ENETC_RBIDR);
	return (0);
}
static void
enetc_intr_enable(if_ctx_t ctx)
{
	struct enetc_softc *sc;
	int i;

	sc = iflib_get_softc(ctx);

	for (i = 0; i < sc->rx_num_queues; i++)
		ENETC_RXQ_WR4(sc, i, ENETC_RBIER, ENETC_RBIER_RXTIE);

	for (i = 0; i < sc->tx_num_queues; i++)
		ENETC_TXQ_WR4(sc, i, ENETC_TBIER, ENETC_TBIER_TXF);
}

static void
enetc_intr_disable(if_ctx_t ctx)
{
	struct enetc_softc *sc;
	int i;

	sc = iflib_get_softc(ctx);

	for (i = 0; i < sc->rx_num_queues; i++)
		ENETC_RXQ_WR4(sc, i, ENETC_RBIER, 0);

	for (i = 0; i < sc->tx_num_queues; i++)
		ENETC_TXQ_WR4(sc, i, ENETC_TBIER, 0);
}

static int
enetc_isc_txd_encap(void *data, if_pkt_info_t ipi)
{
	struct enetc_softc *sc = data;
	struct enetc_tx_queue *queue;
	union enetc_tx_bd *desc;
	bus_dma_segment_t *segs;
	qidx_t pidx, queue_len;
	qidx_t i = 0;

	queue = &sc->tx_queues[ipi->ipi_qsidx];
	segs = ipi->ipi_segs;
	pidx = ipi->ipi_pidx;
	queue_len = sc->tx_queue_size;

	/*
	 * First descriptor is special. We use it to set frame
	 * related information and offloads, e.g. VLAN tag.
	 */
	desc = &queue->ring[pidx];
	bzero(desc, sizeof(*desc));
	desc->frm_len = ipi->ipi_len;
	desc->addr = segs[i].ds_addr;
	desc->buf_len = segs[i].ds_len;
	if (ipi->ipi_flags & IPI_TX_INTR)
		desc->flags = ENETC_TXBD_FLAGS_FI;

	i++;
	if (++pidx == queue_len)
		pidx = 0;

	if (ipi->ipi_mflags & M_VLANTAG) {
		/* VLAN tag is inserted in a separate descriptor. */
		desc->flags |= ENETC_TXBD_FLAGS_EX;
		desc = &queue->ring[pidx];
		bzero(desc, sizeof(*desc));
		desc->ext.vid = ipi->ipi_vtag;
		desc->ext.e_flags = ENETC_TXBD_E_FLAGS_VLAN_INS;
		if (++pidx == queue_len)
			pidx = 0;
	}

	/* Now add remaining descriptors. */
	for (;i < ipi->ipi_nsegs; i++) {
		desc = &queue->ring[pidx];
		bzero(desc, sizeof(*desc));
		desc->addr = segs[i].ds_addr;
		desc->buf_len = segs[i].ds_len;

		if (++pidx == queue_len)
			pidx = 0;
	}

	desc->flags |= ENETC_TXBD_FLAGS_F;
	ipi->ipi_new_pidx = pidx;
	if (pidx == queue->next_to_clean)
		queue->ring_full = true;

	return (0);
}

static void
enetc_isc_txd_flush(void *data, uint16_t qid, qidx_t pidx)
{
	struct enetc_softc *sc = data;

	ENETC_TXQ_WR4(sc, qid, ENETC_TBPIR, pidx);
}

static int
enetc_isc_txd_credits_update(void *data, uint16_t qid, bool clear)
{
	struct enetc_softc *sc = data;
	struct enetc_tx_queue *queue;
	qidx_t next_to_clean, next_to_process;
	int clean_count;

	queue = &sc->tx_queues[qid];
	next_to_process =
	    ENETC_TXQ_RD4(sc, qid, ENETC_TBCIR) & ENETC_TBCIR_IDX_MASK;
	next_to_clean = queue->next_to_clean;

	if (next_to_clean == next_to_process && !queue->ring_full)
		return (0);

	if (!clear)
		return (1);

	clean_count = next_to_process - next_to_clean;
	if (clean_count <= 0)
		clean_count += sc->tx_queue_size;

	queue->next_to_clean = next_to_process;
	queue->ring_full = false;

	return (clean_count);
}

static int
enetc_isc_rxd_available(void *data, uint16_t qid, qidx_t pidx, qidx_t budget)
{
	struct enetc_softc *sc = data;
	struct enetc_rx_queue *queue;
	qidx_t hw_pidx, queue_len;
	union enetc_rx_bd *desc;
	int count = 0;

	queue = &sc->rx_queues[qid];
	desc = &queue->ring[pidx];
	queue_len = sc->rx_queue_size;

	if (desc->r.lstatus == 0)
		return (0);

	if (budget == 1)
		return (1);

	hw_pidx = ENETC_RXQ_RD4(sc, qid, ENETC_RBPIR);
	while (pidx != hw_pidx && count < budget) {
		desc = &queue->ring[pidx];
		if (desc->r.lstatus & ENETC_RXBD_LSTATUS_F)
			count++;

		if (++pidx == queue_len)
			pidx = 0;
	}

	return (count);
}

static int
enetc_isc_rxd_pkt_get(void *data, if_rxd_info_t ri)
{
	struct enetc_softc *sc = data;
	struct enetc_rx_queue *queue;
	union enetc_rx_bd *desc;
	uint16_t buf_len, pkt_size = 0;
	qidx_t cidx, queue_len;
	uint32_t status;
	int i;

	cidx = ri->iri_cidx;
	queue = &sc->rx_queues[ri->iri_qsidx];
	desc = &queue->ring[cidx];
	status = desc->r.lstatus;
	queue_len = sc->rx_queue_size;

	/*
	 * Ready bit will be set only when all descriptors
	 * in the chain have been processed.
	 */
	if ((status & ENETC_RXBD_LSTATUS_R) == 0)
		return (EAGAIN);

	/* Pass RSS hash. */
	if (status & ENETC_RXBD_FLAG_RSSV) {
		ri->iri_flowid = desc->r.rss_hash;
		ri->iri_rsstype = M_HASHTYPE_OPAQUE_HASH;
	}

	/* Pass IP checksum status. */
	ri->iri_csum_flags = CSUM_IP_CHECKED;
	if ((desc->r.parse_summary & ENETC_RXBD_PARSER_ERROR) == 0)
		ri->iri_csum_flags |= CSUM_IP_VALID;

	/* Pass extracted VLAN tag. */
	if (status & ENETC_RXBD_FLAG_VLAN) {
		ri->iri_vtag = desc->r.vlan_opt;
		ri->iri_flags = M_VLANTAG;
	}

	for (i = 0; i < ENETC_MAX_SCATTER; i++) {
		buf_len = desc->r.buf_len;
		ri->iri_frags[i].irf_idx = cidx;
		ri->iri_frags[i].irf_len = buf_len;
		pkt_size += buf_len;
		if (desc->r.lstatus & ENETC_RXBD_LSTATUS_F)
			break;

		if (++cidx == queue_len)
			cidx = 0;

		desc = &queue->ring[cidx];
	}
	ri->iri_nfrags = i + 1;
	ri->iri_len = pkt_size + ENETC_RX_IP_ALIGN;
	ri->iri_pad = ENETC_RX_IP_ALIGN;

	MPASS(desc->r.lstatus & ENETC_RXBD_LSTATUS_F);
	if (status & ENETC_RXBD_LSTATUS(ENETC_RXBD_ERR_MASK))
		return (EBADMSG);

	return (0);
}

static void
enetc_isc_rxd_refill(void *data, if_rxd_update_t iru)
{
	struct enetc_softc *sc = data;
	struct enetc_rx_queue *queue;
	union enetc_rx_bd *desc;
	qidx_t pidx, queue_len;
	uint64_t *paddrs;
	int i, count;

	queue = &sc->rx_queues[iru->iru_qsidx];
	paddrs = iru->iru_paddrs;
	pidx = iru->iru_pidx;
	count = iru->iru_count;
	queue_len = sc->rx_queue_size;

	for (i = 0; i < count; i++) {
		desc = &queue->ring[pidx];
		bzero(desc, sizeof(*desc));

		desc->w.addr = paddrs[i];
		if (++pidx == queue_len)
			pidx = 0;
	}
	/*
	 * After enabling the queue NIC will prefetch the first
	 * 8 descriptors. It probably assumes that the RX is fully
	 * refilled when cidx == pidx.
	 * Enable it only if we have enough descriptors ready on the ring.
	 */
	if (!queue->enabled && pidx >= 8) {
		ENETC_RXQ_WR4(sc, iru->iru_qsidx, ENETC_RBMR, sc->rbmr);
		queue->enabled = true;
	}
}

static void
enetc_isc_rxd_flush(void *data, uint16_t qid, uint8_t flid, qidx_t pidx)
{
	struct enetc_softc *sc = data;

	ENETC_RXQ_WR4(sc, qid, ENETC_RBCIR, pidx);
}

static uint64_t
enetc_get_counter(if_ctx_t ctx, ift_counter cnt)
{
	struct enetc_softc *sc;
	struct ifnet *ifp;

	sc = iflib_get_softc(ctx);
	ifp = iflib_get_ifp(ctx);

	switch (cnt) {
	case IFCOUNTER_IERRORS:
		return (ENETC_PORT_RD8(sc, ENETC_PM0_RERR));
	case IFCOUNTER_OERRORS:
		return (ENETC_PORT_RD8(sc, ENETC_PM0_TERR));
	default:
		return (if_get_counter_default(ifp, cnt));
	}
}

static int
enetc_mtu_set(if_ctx_t ctx, uint32_t mtu)
{
	struct enetc_softc *sc = iflib_get_softc(ctx);
	uint32_t max_frame_size;

	max_frame_size = mtu +
	    ETHER_HDR_LEN +
	    ETHER_CRC_LEN +
	    sizeof(struct ether_vlan_header);

	if (max_frame_size > ENETC_MAX_FRAME_LEN)
		return (EINVAL);

	sc->shared->isc_max_frame_size = max_frame_size;

	return (0);
}

static int
enetc_promisc_set(if_ctx_t ctx, int flags)
{
	struct enetc_softc *sc;
	uint32_t reg = 0;

	sc = iflib_get_softc(ctx);

	if (flags & IFF_PROMISC)
		reg = ENETC_PSIPMR_SET_UP(0) | ENETC_PSIPMR_SET_MP(0);
	else if (flags & IFF_ALLMULTI)
		reg = ENETC_PSIPMR_SET_MP(0);

	ENETC_PORT_WR4(sc, ENETC_PSIPMR, reg);

	return (0);
}

static void
enetc_timer(if_ctx_t ctx, uint16_t qid)
{
	/*
	 * Poll PHY status. Do this only for qid 0 to save
	 * some cycles.
	 */
	if (qid == 0)
		iflib_admin_intr_deferred(ctx);
}

static void
enetc_update_admin_status(if_ctx_t ctx)
{
	struct enetc_softc *sc;
	struct mii_data *miid;

	sc = iflib_get_softc(ctx);

	if (!sc->fixed_link) {
		miid = device_get_softc(sc->miibus);
		mii_tick(miid);
	}
}

static int
enetc_miibus_readreg(device_t dev, int phy, int reg)
{
	struct enetc_softc *sc;
	int val;

	sc = iflib_get_softc(device_get_softc(dev));

	mtx_lock(&sc->mii_lock);
	val = enetc_mdio_read(sc->regs, ENETC_PORT_BASE + ENETC_EMDIO_BASE,
	    phy, reg);
	mtx_unlock(&sc->mii_lock);

	return (val);
}

static int
enetc_miibus_writereg(device_t dev, int phy, int reg, int data)
{
	struct enetc_softc *sc;
	int ret;

	sc = iflib_get_softc(device_get_softc(dev));

	mtx_lock(&sc->mii_lock);
	ret = enetc_mdio_write(sc->regs, ENETC_PORT_BASE + ENETC_EMDIO_BASE,
	    phy, reg, data);
	mtx_unlock(&sc->mii_lock);

	return (ret);
}

static void
enetc_miibus_linkchg(device_t dev)
{

	enetc_miibus_statchg(dev);
}

static void
enetc_miibus_statchg(device_t dev)
{
	struct enetc_softc *sc;
	struct mii_data *miid;
	int link_state, baudrate;

	sc = iflib_get_softc(device_get_softc(dev));
	miid = device_get_softc(sc->miibus);

	baudrate = ifmedia_baudrate(miid->mii_media_active);
	if (miid->mii_media_status & IFM_AVALID) {
		if (miid->mii_media_status & IFM_ACTIVE)
			link_state = LINK_STATE_UP;
		else
			link_state = LINK_STATE_DOWN;
	} else {
		link_state = LINK_STATE_UNKNOWN;
	}

	iflib_link_state_change(sc->ctx, link_state, baudrate);

}

static int
enetc_media_change(if_t ifp)
{
	struct enetc_softc *sc;
	struct mii_data *miid;

	sc = iflib_get_softc(ifp->if_softc);
	miid = device_get_softc(sc->miibus);

	mii_mediachg(miid);
	return (0);
}

static void
enetc_media_status(if_t ifp, struct ifmediareq* ifmr)
{
	struct enetc_softc *sc;
	struct mii_data *miid;

	sc = iflib_get_softc(ifp->if_softc);
	miid = device_get_softc(sc->miibus);

	mii_pollstat(miid);

	ifmr->ifm_active = miid->mii_media_active;
	ifmr->ifm_status = miid->mii_media_status;
}

static int
enetc_fixed_media_change(if_t ifp)
{

	if_printf(ifp, "Can't change media in fixed-link mode.\n");
	return (0);
}
static void
enetc_fixed_media_status(if_t ifp, struct ifmediareq* ifmr)
{
	struct enetc_softc *sc;

	sc = iflib_get_softc(ifp->if_softc);

	ifmr->ifm_status = IFM_AVALID | IFM_ACTIVE;
	ifmr->ifm_active = sc->fixed_ifmedia.ifm_cur->ifm_media;
	return;
}
