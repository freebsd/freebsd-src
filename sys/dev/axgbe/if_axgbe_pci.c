/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2020 Advanced Micro Devices, Inc.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Contact Information :
 * Rajesh Kumar <rajesh1.kumar@amd.com>
 * Shreyank Amartya <Shreyank.Amartya@amd.com>
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_media.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include "xgbe.h"
#include "xgbe-common.h"

#include "miibus_if.h"
#include "ifdi_if.h"
#include "opt_inet.h"
#include "opt_inet6.h"

MALLOC_DEFINE(M_AXGBE, "axgbe", "axgbe data");

extern struct if_txrx axgbe_txrx;

/* Function prototypes */
static void *axgbe_register(device_t);
static int axgbe_if_attach_pre(if_ctx_t);
static int axgbe_if_attach_post(if_ctx_t);
static int axgbe_if_detach(if_ctx_t);
static void axgbe_if_stop(if_ctx_t);
static void axgbe_if_init(if_ctx_t);

/* Queue related routines */
static int axgbe_if_tx_queues_alloc(if_ctx_t, caddr_t *, uint64_t *, int, int);
static int axgbe_if_rx_queues_alloc(if_ctx_t, caddr_t *, uint64_t *, int, int);
static int axgbe_alloc_channels(if_ctx_t);
static void axgbe_if_queues_free(if_ctx_t);
static int axgbe_if_tx_queue_intr_enable(if_ctx_t, uint16_t);
static int axgbe_if_rx_queue_intr_enable(if_ctx_t, uint16_t);

/* Interrupt related routines */
static void axgbe_if_disable_intr(if_ctx_t);
static void axgbe_if_enable_intr(if_ctx_t);
static int axgbe_if_msix_intr_assign(if_ctx_t, int);
static void xgbe_free_intr(struct xgbe_prv_data *, struct resource *, void *, int);

/* Init and Iflib routines */
static void axgbe_pci_init(struct xgbe_prv_data *);
static void axgbe_pci_stop(if_ctx_t);
static void xgbe_disable_rx_tx_int(struct xgbe_prv_data *, struct xgbe_channel *);
static void xgbe_disable_rx_tx_ints(struct xgbe_prv_data *);
static int axgbe_if_mtu_set(if_ctx_t, uint32_t);
static void axgbe_if_update_admin_status(if_ctx_t);
static void axgbe_if_media_status(if_ctx_t, struct ifmediareq *);
static int axgbe_if_media_change(if_ctx_t);
static int axgbe_if_promisc_set(if_ctx_t, int);
static uint64_t axgbe_if_get_counter(if_ctx_t, ift_counter);
static void axgbe_if_vlan_register(if_ctx_t, uint16_t);
static void axgbe_if_vlan_unregister(if_ctx_t, uint16_t);
#if __FreeBSD_version >= 1300000
static bool axgbe_if_needs_restart(if_ctx_t, enum iflib_restart_event);
#endif
static void axgbe_set_counts(if_ctx_t);
static void axgbe_init_iflib_softc_ctx(struct axgbe_if_softc *);

/* MII interface registered functions */
static int axgbe_miibus_readreg(device_t, int, int);
static int axgbe_miibus_writereg(device_t, int, int, int);
static void axgbe_miibus_statchg(device_t);

/* ISR routines */
static int axgbe_dev_isr(void *);
static void axgbe_ecc_isr(void *);
static void axgbe_i2c_isr(void *);
static void axgbe_an_isr(void *);
static int axgbe_msix_que(void *);

/* Timer routines */
static void xgbe_service(void *, int);
static void xgbe_service_timer(void *);
static void xgbe_init_timers(struct xgbe_prv_data *);
static void xgbe_stop_timers(struct xgbe_prv_data *);

/* Dump routines */
static void xgbe_dump_prop_registers(struct xgbe_prv_data *);

/*
 * Allocate only for MAC (BAR0) and PCS (BAR1) registers, and just point the
 * MSI-X table bar  (BAR5) to iflib. iflib will do the allocation for MSI-X
 * table.
 */
static struct resource_spec axgbe_pci_mac_spec[] = {
	{ SYS_RES_MEMORY, PCIR_BAR(0), RF_ACTIVE }, /* MAC regs */
	{ SYS_RES_MEMORY, PCIR_BAR(1), RF_ACTIVE }, /* PCS regs */
	{ -1, 0 }
};

static pci_vendor_info_t axgbe_vendor_info_array[] =
{
	PVID(0x1022, 0x1458,  "AMD 10 Gigabit Ethernet Driver"),
	PVID(0x1022, 0x1459,  "AMD 10 Gigabit Ethernet Driver"),
	PVID_END
};

static struct xgbe_version_data xgbe_v2a = {
	.init_function_ptrs_phy_impl    = xgbe_init_function_ptrs_phy_v2,
	.xpcs_access                    = XGBE_XPCS_ACCESS_V2,
	.mmc_64bit                      = 1,
	.tx_max_fifo_size               = 229376,
	.rx_max_fifo_size               = 229376,
	.tx_tstamp_workaround           = 1,
	.ecc_support                    = 1,
	.i2c_support                    = 1,
	.irq_reissue_support            = 1,
	.tx_desc_prefetch               = 5,
	.rx_desc_prefetch               = 5,
	.an_cdr_workaround              = 1,
};

static struct xgbe_version_data xgbe_v2b = {
	.init_function_ptrs_phy_impl    = xgbe_init_function_ptrs_phy_v2,
	.xpcs_access                    = XGBE_XPCS_ACCESS_V2,
	.mmc_64bit                      = 1,
	.tx_max_fifo_size               = 65536,
	.rx_max_fifo_size               = 65536,
	.tx_tstamp_workaround           = 1,
	.ecc_support                    = 1,
	.i2c_support                    = 1,
	.irq_reissue_support            = 1,
	.tx_desc_prefetch               = 5,
	.rx_desc_prefetch               = 5,
	.an_cdr_workaround              = 1,
};

/* Device Interface */
static device_method_t ax_methods[] = {
	DEVMETHOD(device_register, axgbe_register),
	DEVMETHOD(device_probe, iflib_device_probe),
	DEVMETHOD(device_attach, iflib_device_attach),
	DEVMETHOD(device_detach, iflib_device_detach),

	/* MII interface */
	DEVMETHOD(miibus_readreg, axgbe_miibus_readreg),
	DEVMETHOD(miibus_writereg, axgbe_miibus_writereg),
	DEVMETHOD(miibus_statchg, axgbe_miibus_statchg),

	DEVMETHOD_END
};

static driver_t ax_driver = {
	"ax", ax_methods, sizeof(struct axgbe_if_softc),
};

devclass_t ax_devclass;
DRIVER_MODULE(axp, pci, ax_driver, ax_devclass, 0, 0);
DRIVER_MODULE(miibus, ax, miibus_driver, miibus_devclass, 0, 0);
IFLIB_PNP_INFO(pci, ax_driver, axgbe_vendor_info_array);

MODULE_DEPEND(ax, pci, 1, 1, 1);
MODULE_DEPEND(ax, ether, 1, 1, 1);
MODULE_DEPEND(ax, iflib, 1, 1, 1);
MODULE_DEPEND(ax, miibus, 1, 1, 1);

/* Iflib Interface */
static device_method_t axgbe_if_methods[] = {
	DEVMETHOD(ifdi_attach_pre, axgbe_if_attach_pre),
	DEVMETHOD(ifdi_attach_post, axgbe_if_attach_post),
	DEVMETHOD(ifdi_detach, axgbe_if_detach),
	DEVMETHOD(ifdi_init, axgbe_if_init),
	DEVMETHOD(ifdi_stop, axgbe_if_stop),
	DEVMETHOD(ifdi_msix_intr_assign, axgbe_if_msix_intr_assign),
	DEVMETHOD(ifdi_intr_enable, axgbe_if_enable_intr),
	DEVMETHOD(ifdi_intr_disable, axgbe_if_disable_intr),
	DEVMETHOD(ifdi_tx_queue_intr_enable, axgbe_if_tx_queue_intr_enable),
	DEVMETHOD(ifdi_rx_queue_intr_enable, axgbe_if_rx_queue_intr_enable),
	DEVMETHOD(ifdi_tx_queues_alloc, axgbe_if_tx_queues_alloc),
	DEVMETHOD(ifdi_rx_queues_alloc, axgbe_if_rx_queues_alloc),
	DEVMETHOD(ifdi_queues_free, axgbe_if_queues_free),
	DEVMETHOD(ifdi_update_admin_status, axgbe_if_update_admin_status),
	DEVMETHOD(ifdi_mtu_set, axgbe_if_mtu_set),
	DEVMETHOD(ifdi_media_status, axgbe_if_media_status),
	DEVMETHOD(ifdi_media_change, axgbe_if_media_change),
	DEVMETHOD(ifdi_promisc_set, axgbe_if_promisc_set),
	DEVMETHOD(ifdi_get_counter, axgbe_if_get_counter),
	DEVMETHOD(ifdi_vlan_register, axgbe_if_vlan_register),
	DEVMETHOD(ifdi_vlan_unregister, axgbe_if_vlan_unregister),
#if __FreeBSD_version >= 1300000
	DEVMETHOD(ifdi_needs_restart, axgbe_if_needs_restart),
#endif
	DEVMETHOD_END
};

static driver_t axgbe_if_driver = {
	"axgbe_if", axgbe_if_methods, sizeof(struct axgbe_if_softc)
};

/* Iflib Shared Context */
static struct if_shared_ctx axgbe_sctx_init = {
	.isc_magic = IFLIB_MAGIC,
	.isc_driver = &axgbe_if_driver,
	.isc_q_align = PAGE_SIZE,
	.isc_tx_maxsize = XGBE_TSO_MAX_SIZE + sizeof(struct ether_vlan_header),
	.isc_tx_maxsegsize = PAGE_SIZE,
	.isc_tso_maxsize = XGBE_TSO_MAX_SIZE + sizeof(struct ether_vlan_header),
	.isc_tso_maxsegsize = PAGE_SIZE,
	.isc_rx_maxsize = MJUM9BYTES,
	.isc_rx_maxsegsize = MJUM9BYTES,
	.isc_rx_nsegments = 1,
	.isc_admin_intrcnt = 4,

	.isc_vendor_info = axgbe_vendor_info_array,
	.isc_driver_version = XGBE_DRV_VERSION,

	.isc_nrxd_min = {XGBE_RX_DESC_CNT_MIN, XGBE_RX_DESC_CNT_MIN},
	.isc_nrxd_default = {XGBE_RX_DESC_CNT_DEFAULT, XGBE_RX_DESC_CNT_DEFAULT},
	.isc_nrxd_max = {XGBE_RX_DESC_CNT_MAX, XGBE_RX_DESC_CNT_MAX},
	.isc_ntxd_min = {XGBE_TX_DESC_CNT_MIN},
	.isc_ntxd_default = {XGBE_TX_DESC_CNT_DEFAULT}, 
	.isc_ntxd_max = {XGBE_TX_DESC_CNT_MAX},

	.isc_nfl = 2,
	.isc_ntxqs = 1,
	.isc_nrxqs = 2,
	.isc_flags = IFLIB_TSO_INIT_IP | IFLIB_NEED_SCRATCH |
	    IFLIB_NEED_ZERO_CSUM | IFLIB_NEED_ETHER_PAD,
};

static void *
axgbe_register(device_t dev)
{
	return (&axgbe_sctx_init);
}

/* MII Interface Functions */
static int
axgbe_miibus_readreg(device_t dev, int phy, int reg)
{
	struct axgbe_if_softc   *sc = iflib_get_softc(device_get_softc(dev));
	struct xgbe_prv_data    *pdata = &sc->pdata;
	int val;

	axgbe_printf(3, "%s: phy %d reg %d\n", __func__, phy, reg);

	val = xgbe_phy_mii_read(pdata, phy, reg);

	axgbe_printf(2, "%s: val 0x%x\n", __func__, val);
	return (val & 0xFFFF);
}

static int
axgbe_miibus_writereg(device_t dev, int phy, int reg, int val)
{
	struct axgbe_if_softc   *sc = iflib_get_softc(device_get_softc(dev));
	struct xgbe_prv_data    *pdata = &sc->pdata;

	axgbe_printf(3, "%s: phy %d reg %d val 0x%x\n", __func__, phy, reg, val);

	xgbe_phy_mii_write(pdata, phy, reg, val);

	return(0);
}

static void
axgbe_miibus_statchg(device_t dev)
{
        struct axgbe_if_softc   *sc = iflib_get_softc(device_get_softc(dev));
        struct xgbe_prv_data    *pdata = &sc->pdata;
	struct mii_data		*mii = device_get_softc(pdata->axgbe_miibus);
	struct ifnet		*ifp = pdata->netdev;
	int bmsr;

	axgbe_printf(2, "%s: Link %d/%d\n", __func__, pdata->phy.link,
	    pdata->phy_link);

	if (mii == NULL || ifp == NULL ||
	    (ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
		return;

	if ((mii->mii_media_status & (IFM_ACTIVE | IFM_AVALID)) ==
	    (IFM_ACTIVE | IFM_AVALID)) {

		switch (IFM_SUBTYPE(mii->mii_media_active)) {
		case IFM_10_T:
		case IFM_100_TX:
			pdata->phy.link = 1;
			break;
		case IFM_1000_T:
		case IFM_1000_SX:
		case IFM_2500_SX:
			pdata->phy.link = 1;
			break;
		default:
			pdata->phy.link = 0;
			break;
		}
	} else
		pdata->phy_link = 0;

	bmsr = axgbe_miibus_readreg(pdata->dev, pdata->mdio_addr, MII_BMSR);
	if (bmsr & BMSR_ANEG) {

		axgbe_printf(2, "%s: Autoneg Done\n", __func__);

		/* Raise AN Interrupt */
		XMDIO_WRITE(pdata, MDIO_MMD_AN, MDIO_AN_INTMASK,
		    XGBE_AN_CL73_INT_MASK);
	}
}

static int
axgbe_if_attach_pre(if_ctx_t ctx)
{
	struct axgbe_if_softc	*sc;
	struct xgbe_prv_data	*pdata;
	struct resource		*mac_res[2];
	if_softc_ctx_t		scctx;
	if_shared_ctx_t		sctx;
	device_t		dev;
	unsigned int		ma_lo, ma_hi;
	unsigned int		reg;

	sc = iflib_get_softc(ctx);
	sc->pdata.dev = dev = iflib_get_dev(ctx);
	sc->sctx = sctx = iflib_get_sctx(ctx);
	sc->scctx = scctx = iflib_get_softc_ctx(ctx);
	sc->media = iflib_get_media(ctx);
	sc->ctx = ctx;
	sc->link_status = LINK_STATE_DOWN;
	pdata = &sc->pdata;
	pdata->netdev = iflib_get_ifp(ctx);

	spin_lock_init(&pdata->xpcs_lock);

	/* Initialize locks */
        mtx_init(&pdata->rss_mutex, "xgbe rss mutex lock", NULL, MTX_DEF);
	mtx_init(&pdata->mdio_mutex, "xgbe MDIO mutex lock", NULL, MTX_SPIN);

	/* Allocate VLAN bitmap */
	pdata->active_vlans = bit_alloc(VLAN_NVID, M_AXGBE, M_WAITOK|M_ZERO);
	pdata->num_active_vlans = 0;

	/* Get the version data */
	DBGPR("%s: Device ID: 0x%x\n", __func__, pci_get_device(dev));
	if (pci_get_device(dev) == 0x1458)
		sc->pdata.vdata = &xgbe_v2a;
	else if (pci_get_device(dev) == 0x1459)
		sc->pdata.vdata = &xgbe_v2b;

	/* PCI setup */
        if (bus_alloc_resources(dev, axgbe_pci_mac_spec, mac_res))
                return (ENXIO);

        sc->pdata.xgmac_res = mac_res[0];
        sc->pdata.xpcs_res = mac_res[1];

        /* Set the PCS indirect addressing definition registers*/
	pdata->xpcs_window_def_reg = PCS_V2_WINDOW_DEF;
	pdata->xpcs_window_sel_reg = PCS_V2_WINDOW_SELECT;

        /* Configure the PCS indirect addressing support */
	reg = XPCS32_IOREAD(pdata, pdata->xpcs_window_def_reg);
	pdata->xpcs_window = XPCS_GET_BITS(reg, PCS_V2_WINDOW_DEF, OFFSET);
	pdata->xpcs_window <<= 6;
	pdata->xpcs_window_size = XPCS_GET_BITS(reg, PCS_V2_WINDOW_DEF, SIZE);
	pdata->xpcs_window_size = 1 << (pdata->xpcs_window_size + 7);
	pdata->xpcs_window_mask = pdata->xpcs_window_size - 1;
	DBGPR("xpcs window def : %#010x\n",
	    pdata->xpcs_window_def_reg);
	DBGPR("xpcs window sel : %#010x\n",
	    pdata->xpcs_window_sel_reg);
        DBGPR("xpcs window : %#010x\n",
	    pdata->xpcs_window);
	DBGPR("xpcs window size : %#010x\n",
	    pdata->xpcs_window_size);
	DBGPR("xpcs window mask : %#010x\n",
	    pdata->xpcs_window_mask);

	/* Enable all interrupts in the hardware */
        XP_IOWRITE(pdata, XP_INT_EN, 0x1fffff);
	
	/* Retrieve the MAC address */
	ma_lo = XP_IOREAD(pdata, XP_MAC_ADDR_LO);
	ma_hi = XP_IOREAD(pdata, XP_MAC_ADDR_HI);
	pdata->mac_addr[0] = ma_lo & 0xff;
	pdata->mac_addr[1] = (ma_lo >> 8) & 0xff;
	pdata->mac_addr[2] = (ma_lo >>16) & 0xff;
	pdata->mac_addr[3] = (ma_lo >> 24) & 0xff;
	pdata->mac_addr[4] = ma_hi & 0xff;
	pdata->mac_addr[5] = (ma_hi >> 8) & 0xff;
	if (!XP_GET_BITS(ma_hi, XP_MAC_ADDR_HI, VALID)) {
		axgbe_error("Invalid mac address\n");
		return (EINVAL);
	}
	iflib_set_mac(ctx, pdata->mac_addr);

	/* Clock settings */
	pdata->sysclk_rate = XGBE_V2_DMA_CLOCK_FREQ;
	pdata->ptpclk_rate = XGBE_V2_PTP_CLOCK_FREQ;

	/* Set the DMA coherency values */
	pdata->coherent = 1;
	pdata->arcr = XGBE_DMA_PCI_ARCR;
	pdata->awcr = XGBE_DMA_PCI_AWCR;
	pdata->awarcr = XGBE_DMA_PCI_AWARCR;

	/* Read the port property registers */
	pdata->pp0 = XP_IOREAD(pdata, XP_PROP_0);
	pdata->pp1 = XP_IOREAD(pdata, XP_PROP_1);
	pdata->pp2 = XP_IOREAD(pdata, XP_PROP_2);
	pdata->pp3 = XP_IOREAD(pdata, XP_PROP_3);
	pdata->pp4 = XP_IOREAD(pdata, XP_PROP_4);
	DBGPR("port property 0 = %#010x\n", pdata->pp0);
	DBGPR("port property 1 = %#010x\n", pdata->pp1);
	DBGPR("port property 2 = %#010x\n", pdata->pp2);
	DBGPR("port property 3 = %#010x\n", pdata->pp3);
	DBGPR("port property 4 = %#010x\n", pdata->pp4);

	/* Set the maximum channels and queues */
	pdata->tx_max_channel_count = XP_GET_BITS(pdata->pp1, XP_PROP_1,
	    MAX_TX_DMA);
	pdata->rx_max_channel_count = XP_GET_BITS(pdata->pp1, XP_PROP_1,
	    MAX_RX_DMA);
	pdata->tx_max_q_count = XP_GET_BITS(pdata->pp1, XP_PROP_1,
	    MAX_TX_QUEUES);
	pdata->rx_max_q_count = XP_GET_BITS(pdata->pp1, XP_PROP_1,
	    MAX_RX_QUEUES);
	DBGPR("max tx/rx channel count = %u/%u\n",
	    pdata->tx_max_channel_count, pdata->rx_max_channel_count);
	DBGPR("max tx/rx hw queue count = %u/%u\n",
	    pdata->tx_max_q_count, pdata->rx_max_q_count);

	axgbe_set_counts(ctx);

	/* Set the maximum fifo amounts */
        pdata->tx_max_fifo_size = XP_GET_BITS(pdata->pp2, XP_PROP_2,
                                              TX_FIFO_SIZE);
        pdata->tx_max_fifo_size *= 16384;
        pdata->tx_max_fifo_size = min(pdata->tx_max_fifo_size,
                                      pdata->vdata->tx_max_fifo_size);
        pdata->rx_max_fifo_size = XP_GET_BITS(pdata->pp2, XP_PROP_2,
                                              RX_FIFO_SIZE);
        pdata->rx_max_fifo_size *= 16384;
        pdata->rx_max_fifo_size = min(pdata->rx_max_fifo_size,
                                      pdata->vdata->rx_max_fifo_size);
	DBGPR("max tx/rx max fifo size = %u/%u\n",
	    pdata->tx_max_fifo_size, pdata->rx_max_fifo_size);

	/* Initialize IFLIB if_softc_ctx_t */
	axgbe_init_iflib_softc_ctx(sc);

	/* Alloc channels */
	if (axgbe_alloc_channels(ctx)) {
		axgbe_error("Unable to allocate channel memory\n");
                return (ENOMEM);
        }

	TASK_INIT(&pdata->service_work, 0, xgbe_service, pdata);

	/* create the workqueue */
	pdata->dev_workqueue = taskqueue_create("axgbe", M_WAITOK,
	    taskqueue_thread_enqueue, &pdata->dev_workqueue);
	taskqueue_start_threads(&pdata->dev_workqueue, 1, PI_NET,
	    "axgbe dev taskq");

	/* Init timers */
	xgbe_init_timers(pdata);

        return (0);
} /* axgbe_if_attach_pre */

static void
xgbe_init_all_fptrs(struct xgbe_prv_data *pdata)
{
	xgbe_init_function_ptrs_dev(&pdata->hw_if);
	xgbe_init_function_ptrs_phy(&pdata->phy_if);
        xgbe_init_function_ptrs_i2c(&pdata->i2c_if);
	xgbe_init_function_ptrs_desc(&pdata->desc_if);

        pdata->vdata->init_function_ptrs_phy_impl(&pdata->phy_if);
}

static void
axgbe_set_counts(if_ctx_t ctx)
{
	struct axgbe_if_softc *sc = iflib_get_softc(ctx);;
	struct xgbe_prv_data *pdata = &sc->pdata;
	cpuset_t lcpus;
	int cpu_count, err;
	size_t len;

	/* Set all function pointers */
	xgbe_init_all_fptrs(pdata);

	/* Populate the hardware features */
	xgbe_get_all_hw_features(pdata);

	if (!pdata->tx_max_channel_count)
		pdata->tx_max_channel_count = pdata->hw_feat.tx_ch_cnt;
	if (!pdata->rx_max_channel_count)
		pdata->rx_max_channel_count = pdata->hw_feat.rx_ch_cnt;

	if (!pdata->tx_max_q_count)
		pdata->tx_max_q_count = pdata->hw_feat.tx_q_cnt;
	if (!pdata->rx_max_q_count)
		pdata->rx_max_q_count = pdata->hw_feat.rx_q_cnt;

	/*
	 * Calculate the number of Tx and Rx rings to be created
	 *  -Tx (DMA) Channels map 1-to-1 to Tx Queues so set
	 *   the number of Tx queues to the number of Tx channels
	 *   enabled
	 *  -Rx (DMA) Channels do not map 1-to-1 so use the actual
	 *   number of Rx queues or maximum allowed
	 */

	/* Get cpu count from sysctl */
	len = sizeof(cpu_count);
	err = kernel_sysctlbyname(curthread, "hw.ncpu", &cpu_count, &len, NULL,
	    0, NULL, 0);
	if (err) {
		axgbe_error("Unable to fetch number of cpus\n");
		cpu_count = 1;
	}

	if (bus_get_cpus(pdata->dev, INTR_CPUS, sizeof(lcpus), &lcpus) != 0) {
                axgbe_error("Unable to fetch CPU list\n");
                /* TODO - handle CPU_COPY(&all_cpus, &lcpus); */
        }

	DBGPR("ncpu %d intrcpu %d\n", cpu_count, CPU_COUNT(&lcpus));

	pdata->tx_ring_count = min(CPU_COUNT(&lcpus), pdata->hw_feat.tx_ch_cnt);
	pdata->tx_ring_count = min(pdata->tx_ring_count,
	    pdata->tx_max_channel_count);
	pdata->tx_ring_count = min(pdata->tx_ring_count, pdata->tx_max_q_count);

	pdata->tx_q_count = pdata->tx_ring_count;

	pdata->rx_ring_count = min(CPU_COUNT(&lcpus), pdata->hw_feat.rx_ch_cnt);
	pdata->rx_ring_count = min(pdata->rx_ring_count,
	    pdata->rx_max_channel_count);

	pdata->rx_q_count = min(pdata->hw_feat.rx_q_cnt, pdata->rx_max_q_count);

	DBGPR("TX/RX max channel count = %u/%u\n",
	    pdata->tx_max_channel_count, pdata->rx_max_channel_count);
	DBGPR("TX/RX max queue count = %u/%u\n",
	    pdata->tx_max_q_count, pdata->rx_max_q_count);
	DBGPR("TX/RX DMA ring count = %u/%u\n",
	    pdata->tx_ring_count, pdata->rx_ring_count);
	DBGPR("TX/RX hardware queue count = %u/%u\n",
	    pdata->tx_q_count, pdata->rx_q_count);
} /* axgbe_set_counts */

static void
axgbe_init_iflib_softc_ctx(struct axgbe_if_softc *sc)
{
	struct xgbe_prv_data *pdata = &sc->pdata;
	if_softc_ctx_t scctx = sc->scctx;
	if_shared_ctx_t sctx = sc->sctx;
	int i;

	scctx->isc_nrxqsets = pdata->rx_q_count;
	scctx->isc_ntxqsets = pdata->tx_q_count;
	scctx->isc_msix_bar = pci_msix_table_bar(pdata->dev);
	scctx->isc_tx_nsegments = 32;

	for (i = 0; i < sctx->isc_ntxqs; i++) {
		scctx->isc_txqsizes[i] = 
		    roundup2(scctx->isc_ntxd[i] * sizeof(struct xgbe_ring_desc),
		    128);
		scctx->isc_txd_size[i] = sizeof(struct xgbe_ring_desc);
	}

	for (i = 0; i < sctx->isc_nrxqs; i++) {
		scctx->isc_rxqsizes[i] =
		    roundup2(scctx->isc_nrxd[i] * sizeof(struct xgbe_ring_desc),
		    128);
		scctx->isc_rxd_size[i] = sizeof(struct xgbe_ring_desc);
	}

	scctx->isc_tx_tso_segments_max = 32;
	scctx->isc_tx_tso_size_max = XGBE_TSO_MAX_SIZE;
	scctx->isc_tx_tso_segsize_max = PAGE_SIZE;

	/*
	 * Set capabilities
	 * 1) IFLIB automatically adds IFCAP_HWSTATS, so need to set explicitly
	 * 2) isc_tx_csum_flags is mandatory if IFCAP_TXCSUM (included in
	 *    IFCAP_HWCSUM) is set
	 */
	scctx->isc_tx_csum_flags = (CSUM_IP | CSUM_TCP | CSUM_UDP | CSUM_SCTP |
	    CSUM_TCP_IPV6 | CSUM_UDP_IPV6 | CSUM_SCTP_IPV6 |
	    CSUM_TSO);
	scctx->isc_capenable = (IFCAP_HWCSUM | IFCAP_HWCSUM_IPV6 |
	    IFCAP_JUMBO_MTU |
	    IFCAP_VLAN_MTU | IFCAP_VLAN_HWTAGGING | IFCAP_VLAN_HWFILTER | 
	    IFCAP_VLAN_HWCSUM |
	    IFCAP_TSO | IFCAP_VLAN_HWTSO);
	scctx->isc_capabilities = scctx->isc_capenable;

	/*
	 * Set rss_table_size alone when adding RSS support. rss_table_mask
	 * will be set by IFLIB based on rss_table_size
	 */
	scctx->isc_rss_table_size = XGBE_RSS_MAX_TABLE_SIZE;

	scctx->isc_ntxqsets_max = XGBE_MAX_QUEUES;
	scctx->isc_nrxqsets_max = XGBE_MAX_QUEUES;

	scctx->isc_txrx = &axgbe_txrx;
}

static int
axgbe_alloc_channels(if_ctx_t ctx)
{
	struct axgbe_if_softc 	*sc = iflib_get_softc(ctx);
	struct xgbe_prv_data	*pdata = &sc->pdata;
	struct xgbe_channel	*channel;
	int i, j, count;

	DBGPR("%s: txqs %d rxqs %d\n", __func__, pdata->tx_ring_count,
	    pdata->rx_ring_count);

	/* Iflibe sets based on isc_ntxqsets/nrxqsets */
	count = max_t(unsigned int, pdata->tx_ring_count, pdata->rx_ring_count);

	/* Allocate channel memory */
	for (i = 0; i < count ; i++) {
		channel = (struct xgbe_channel*)malloc(sizeof(struct xgbe_channel),
		    M_AXGBE, M_NOWAIT | M_ZERO);

		if (channel == NULL) {	
			for (j = 0; j < i; j++) {
				free(pdata->channel[j], M_AXGBE);
				pdata->channel[j] = NULL;
			}
			return (ENOMEM);
		}

		pdata->channel[i] = channel;
	}

	pdata->total_channel_count = count;
	DBGPR("Channel count set to: %u\n", pdata->total_channel_count);

	for (i = 0; i < count; i++) {

		channel = pdata->channel[i];
		snprintf(channel->name, sizeof(channel->name), "channel-%d",i);

		channel->pdata = pdata;
		channel->queue_index = i;
		channel->dma_tag = rman_get_bustag(pdata->xgmac_res);
		bus_space_subregion(channel->dma_tag,
		    rman_get_bushandle(pdata->xgmac_res),
		    DMA_CH_BASE + (DMA_CH_INC * i), DMA_CH_INC,
		    &channel->dma_handle);
		channel->tx_ring = NULL;
		channel->rx_ring = NULL;
	}

	return (0);
} /* axgbe_alloc_channels */

static void
xgbe_service(void *ctx, int pending)
{
        struct xgbe_prv_data *pdata = ctx;
	struct axgbe_if_softc *sc = (struct axgbe_if_softc *)pdata;
	bool prev_state = false;

	/* Get previous link status */
	prev_state = pdata->phy.link;

        pdata->phy_if.phy_status(pdata);

	if (prev_state != pdata->phy.link) {
		pdata->phy_link = pdata->phy.link;
		axgbe_if_update_admin_status(sc->ctx);
	}

        callout_reset(&pdata->service_timer, 1*hz, xgbe_service_timer, pdata);
}

static void
xgbe_service_timer(void *data)
{
        struct xgbe_prv_data *pdata = data;

        taskqueue_enqueue(pdata->dev_workqueue, &pdata->service_work);
}

static void
xgbe_init_timers(struct xgbe_prv_data *pdata)
{
        callout_init(&pdata->service_timer, 1*hz);
}

static void
xgbe_start_timers(struct xgbe_prv_data *pdata)
{
	callout_reset(&pdata->service_timer, 1*hz, xgbe_service_timer, pdata);
}

static void
xgbe_stop_timers(struct xgbe_prv_data *pdata)
{
        callout_drain(&pdata->service_timer);
        callout_stop(&pdata->service_timer);
}

static void
xgbe_dump_phy_registers(struct xgbe_prv_data *pdata)
{
        axgbe_printf(1, "\n************* PHY Reg dump *********************\n");

        axgbe_printf(1, "PCS Control Reg (%#06x) = %#06x\n", MDIO_CTRL1,
            XMDIO_READ(pdata, MDIO_MMD_PCS, MDIO_CTRL1));
        axgbe_printf(1, "PCS Status Reg (%#06x) = %#06x\n", MDIO_STAT1,
            XMDIO_READ(pdata, MDIO_MMD_PCS, MDIO_STAT1));
        axgbe_printf(1, "Phy Id (PHYS ID 1 %#06x)= %#06x\n", MDIO_DEVID1,
            XMDIO_READ(pdata, MDIO_MMD_PCS, MDIO_DEVID1));
        axgbe_printf(1, "Phy Id (PHYS ID 2 %#06x)= %#06x\n", MDIO_DEVID2,
            XMDIO_READ(pdata, MDIO_MMD_PCS, MDIO_DEVID2));
        axgbe_printf(1, "Devices in Package (%#06x)= %#06x\n", MDIO_DEVS1,
            XMDIO_READ(pdata, MDIO_MMD_PCS, MDIO_DEVS1));
        axgbe_printf(1, "Devices in Package (%#06x)= %#06x\n", MDIO_DEVS2,
            XMDIO_READ(pdata, MDIO_MMD_PCS, MDIO_DEVS2));
        axgbe_printf(1, "Auto-Neg Control Reg (%#06x) = %#06x\n", MDIO_CTRL1,
            XMDIO_READ(pdata, MDIO_MMD_AN, MDIO_CTRL1));
        axgbe_printf(1, "Auto-Neg Status Reg (%#06x) = %#06x\n", MDIO_STAT1,
            XMDIO_READ(pdata, MDIO_MMD_AN, MDIO_STAT1));
        axgbe_printf(1, "Auto-Neg Ad Reg 1 (%#06x) = %#06x\n",
            MDIO_AN_ADVERTISE,
            XMDIO_READ(pdata, MDIO_MMD_AN, MDIO_AN_ADVERTISE));
        axgbe_printf(1, "Auto-Neg Ad Reg 2 (%#06x) = %#06x\n",
            MDIO_AN_ADVERTISE + 1,
            XMDIO_READ(pdata, MDIO_MMD_AN, MDIO_AN_ADVERTISE + 1));
        axgbe_printf(1, "Auto-Neg Ad Reg 3 (%#06x) = %#06x\n",
            MDIO_AN_ADVERTISE + 2,
            XMDIO_READ(pdata, MDIO_MMD_AN, MDIO_AN_ADVERTISE + 2));
        axgbe_printf(1, "Auto-Neg Completion Reg (%#06x) = %#06x\n",
            MDIO_AN_COMP_STAT,
            XMDIO_READ(pdata, MDIO_MMD_AN, MDIO_AN_COMP_STAT));

        axgbe_printf(1, "\n************************************************\n");
}

static void
xgbe_dump_prop_registers(struct xgbe_prv_data *pdata)
{
	int i;

        axgbe_printf(1, "\n************* PROP Reg dump ********************\n");

	for (i = 0 ; i < 38 ; i++) {
		axgbe_printf(1, "PROP Offset 0x%08x = %08x\n",
		    (XP_PROP_0 + (i * 4)), XP_IOREAD(pdata,
		    (XP_PROP_0 + (i * 4))));
	}
}

static void
xgbe_dump_dma_registers(struct xgbe_prv_data *pdata, int ch)
{
	struct xgbe_channel     *channel;
	int i;

        axgbe_printf(1, "\n************* DMA Reg dump *********************\n");

        axgbe_printf(1, "DMA MR Reg (%08x) = %08x\n", DMA_MR,
           XGMAC_IOREAD(pdata, DMA_MR));
        axgbe_printf(1, "DMA SBMR Reg (%08x) = %08x\n", DMA_SBMR,
           XGMAC_IOREAD(pdata, DMA_SBMR));
        axgbe_printf(1, "DMA ISR Reg (%08x) = %08x\n", DMA_ISR,
           XGMAC_IOREAD(pdata, DMA_ISR));
        axgbe_printf(1, "DMA AXIARCR Reg (%08x) = %08x\n", DMA_AXIARCR,
           XGMAC_IOREAD(pdata, DMA_AXIARCR));
        axgbe_printf(1, "DMA AXIAWCR Reg (%08x) = %08x\n", DMA_AXIAWCR,
           XGMAC_IOREAD(pdata, DMA_AXIAWCR));
        axgbe_printf(1, "DMA AXIAWARCR Reg (%08x) = %08x\n", DMA_AXIAWARCR,
           XGMAC_IOREAD(pdata, DMA_AXIAWARCR));
        axgbe_printf(1, "DMA DSR0 Reg (%08x) = %08x\n", DMA_DSR0,
           XGMAC_IOREAD(pdata, DMA_DSR0));
        axgbe_printf(1, "DMA DSR1 Reg (%08x) = %08x\n", DMA_DSR1,
           XGMAC_IOREAD(pdata, DMA_DSR1));
        axgbe_printf(1, "DMA DSR2 Reg (%08x) = %08x\n", DMA_DSR2,
           XGMAC_IOREAD(pdata, DMA_DSR2));
        axgbe_printf(1, "DMA DSR3 Reg (%08x) = %08x\n", DMA_DSR3,
           XGMAC_IOREAD(pdata, DMA_DSR3));
        axgbe_printf(1, "DMA DSR4 Reg (%08x) = %08x\n", DMA_DSR4,
           XGMAC_IOREAD(pdata, DMA_DSR4));
        axgbe_printf(1, "DMA TXEDMACR Reg (%08x) = %08x\n", DMA_TXEDMACR,
           XGMAC_IOREAD(pdata, DMA_TXEDMACR));
        axgbe_printf(1, "DMA RXEDMACR Reg (%08x) = %08x\n", DMA_RXEDMACR,
           XGMAC_IOREAD(pdata, DMA_RXEDMACR));

	for (i = 0 ; i < 8 ; i++ ) {

		if (ch >= 0) {
			if (i != ch)
				continue;
		}

		channel = pdata->channel[i];

        	axgbe_printf(1, "\n************* DMA CH %d dump ****************\n", i);

        	axgbe_printf(1, "DMA_CH_CR Reg (%08x) = %08x\n",
		    DMA_CH_CR, XGMAC_DMA_IOREAD(channel, DMA_CH_CR));
        	axgbe_printf(1, "DMA_CH_TCR Reg (%08x) = %08x\n",
		    DMA_CH_TCR, XGMAC_DMA_IOREAD(channel, DMA_CH_TCR));
        	axgbe_printf(1, "DMA_CH_RCR Reg (%08x) = %08x\n",
		    DMA_CH_RCR, XGMAC_DMA_IOREAD(channel, DMA_CH_RCR));
        	axgbe_printf(1, "DMA_CH_TDLR_HI Reg (%08x) = %08x\n",
		    DMA_CH_TDLR_HI, XGMAC_DMA_IOREAD(channel, DMA_CH_TDLR_HI));
        	axgbe_printf(1, "DMA_CH_TDLR_LO Reg (%08x) = %08x\n",
		    DMA_CH_TDLR_LO, XGMAC_DMA_IOREAD(channel, DMA_CH_TDLR_LO));
        	axgbe_printf(1, "DMA_CH_RDLR_HI Reg (%08x) = %08x\n",
		    DMA_CH_RDLR_HI, XGMAC_DMA_IOREAD(channel, DMA_CH_RDLR_HI));
        	axgbe_printf(1, "DMA_CH_RDLR_LO Reg (%08x) = %08x\n",
		    DMA_CH_RDLR_LO, XGMAC_DMA_IOREAD(channel, DMA_CH_RDLR_LO));
        	axgbe_printf(1, "DMA_CH_TDTR_LO Reg (%08x) = %08x\n",
		    DMA_CH_TDTR_LO, XGMAC_DMA_IOREAD(channel, DMA_CH_TDTR_LO));
        	axgbe_printf(1, "DMA_CH_RDTR_LO Reg (%08x) = %08x\n",
		    DMA_CH_RDTR_LO, XGMAC_DMA_IOREAD(channel, DMA_CH_RDTR_LO));	
        	axgbe_printf(1, "DMA_CH_TDRLR Reg (%08x) = %08x\n",
		    DMA_CH_TDRLR, XGMAC_DMA_IOREAD(channel, DMA_CH_TDRLR));
        	axgbe_printf(1, "DMA_CH_RDRLR Reg (%08x) = %08x\n",
		    DMA_CH_RDRLR, XGMAC_DMA_IOREAD(channel, DMA_CH_RDRLR));
        	axgbe_printf(1, "DMA_CH_IER Reg (%08x) = %08x\n",
		    DMA_CH_IER, XGMAC_DMA_IOREAD(channel, DMA_CH_IER));
        	axgbe_printf(1, "DMA_CH_RIWT Reg (%08x) = %08x\n",
		    DMA_CH_RIWT, XGMAC_DMA_IOREAD(channel, DMA_CH_RIWT));
        	axgbe_printf(1, "DMA_CH_CATDR_LO Reg (%08x) = %08x\n",
		    DMA_CH_CATDR_LO, XGMAC_DMA_IOREAD(channel, DMA_CH_CATDR_LO));
        	axgbe_printf(1, "DMA_CH_CARDR_LO Reg (%08x) = %08x\n",
		    DMA_CH_CARDR_LO, XGMAC_DMA_IOREAD(channel, DMA_CH_CARDR_LO));
        	axgbe_printf(1, "DMA_CH_CATBR_HI Reg (%08x) = %08x\n",
		    DMA_CH_CATBR_HI, XGMAC_DMA_IOREAD(channel, DMA_CH_CATBR_HI));
        	axgbe_printf(1, "DMA_CH_CATBR_LO Reg (%08x) = %08x\n",
		    DMA_CH_CATBR_LO, XGMAC_DMA_IOREAD(channel, DMA_CH_CATBR_LO));
        	axgbe_printf(1, "DMA_CH_CARBR_HI Reg (%08x) = %08x\n",
		    DMA_CH_CARBR_HI, XGMAC_DMA_IOREAD(channel, DMA_CH_CARBR_HI));
        	axgbe_printf(1, "DMA_CH_CARBR_LO Reg (%08x) = %08x\n",
		    DMA_CH_CARBR_LO, XGMAC_DMA_IOREAD(channel, DMA_CH_CARBR_LO));
        	axgbe_printf(1, "DMA_CH_SR Reg (%08x) = %08x\n",
		    DMA_CH_SR, XGMAC_DMA_IOREAD(channel, DMA_CH_SR));
        	axgbe_printf(1, "DMA_CH_DSR Reg (%08x) = %08x\n",
		    DMA_CH_DSR,	XGMAC_DMA_IOREAD(channel, DMA_CH_DSR));
        	axgbe_printf(1, "DMA_CH_DCFL Reg (%08x) = %08x\n",
		    DMA_CH_DCFL, XGMAC_DMA_IOREAD(channel, DMA_CH_DCFL));
        	axgbe_printf(1, "DMA_CH_MFC Reg (%08x) = %08x\n",
		    DMA_CH_MFC, XGMAC_DMA_IOREAD(channel, DMA_CH_MFC));
        	axgbe_printf(1, "DMA_CH_TDTRO Reg (%08x) = %08x\n",
		    DMA_CH_TDTRO, XGMAC_DMA_IOREAD(channel, DMA_CH_TDTRO));	
        	axgbe_printf(1, "DMA_CH_RDTRO Reg (%08x) = %08x\n",
		    DMA_CH_RDTRO, XGMAC_DMA_IOREAD(channel, DMA_CH_RDTRO));
        	axgbe_printf(1, "DMA_CH_TDWRO Reg (%08x) = %08x\n",
		    DMA_CH_TDWRO, XGMAC_DMA_IOREAD(channel, DMA_CH_TDWRO));	
        	axgbe_printf(1, "DMA_CH_RDWRO Reg (%08x) = %08x\n",
		    DMA_CH_RDWRO, XGMAC_DMA_IOREAD(channel, DMA_CH_RDWRO));
	}
}

static void
xgbe_dump_mtl_registers(struct xgbe_prv_data *pdata)
{
	int i;

        axgbe_printf(1, "\n************* MTL Reg dump *********************\n");

        axgbe_printf(1, "MTL OMR Reg (%08x) = %08x\n", MTL_OMR,
           XGMAC_IOREAD(pdata, MTL_OMR));
        axgbe_printf(1, "MTL FDCR Reg (%08x) = %08x\n", MTL_FDCR,
           XGMAC_IOREAD(pdata, MTL_FDCR));
        axgbe_printf(1, "MTL FDSR Reg (%08x) = %08x\n", MTL_FDSR,
           XGMAC_IOREAD(pdata, MTL_FDSR));
        axgbe_printf(1, "MTL FDDR Reg (%08x) = %08x\n", MTL_FDDR,
           XGMAC_IOREAD(pdata, MTL_FDDR));
        axgbe_printf(1, "MTL ISR Reg (%08x) = %08x\n", MTL_ISR,
           XGMAC_IOREAD(pdata, MTL_ISR));
        axgbe_printf(1, "MTL RQDCM0R Reg (%08x) = %08x\n", MTL_RQDCM0R,
           XGMAC_IOREAD(pdata, MTL_RQDCM0R));
        axgbe_printf(1, "MTL RQDCM1R Reg (%08x) = %08x\n", MTL_RQDCM1R,
           XGMAC_IOREAD(pdata, MTL_RQDCM1R));
        axgbe_printf(1, "MTL RQDCM2R Reg (%08x) = %08x\n", MTL_RQDCM2R,
           XGMAC_IOREAD(pdata, MTL_RQDCM2R));
        axgbe_printf(1, "MTL TCPM0R Reg (%08x) = %08x\n", MTL_TCPM0R,
           XGMAC_IOREAD(pdata, MTL_TCPM0R));
        axgbe_printf(1, "MTL TCPM1R Reg (%08x) = %08x\n", MTL_TCPM1R,
           XGMAC_IOREAD(pdata, MTL_TCPM1R));

	for (i = 0 ; i < 8 ; i++ ) {

        	axgbe_printf(1, "\n************* MTL CH %d dump ****************\n", i);

        	axgbe_printf(1, "MTL_Q_TQOMR Reg (%08x) = %08x\n",
		    MTL_Q_TQOMR, XGMAC_MTL_IOREAD(pdata, i, MTL_Q_TQOMR));
        	axgbe_printf(1, "MTL_Q_TQUR Reg (%08x) = %08x\n",
		    MTL_Q_TQUR, XGMAC_MTL_IOREAD(pdata, i, MTL_Q_TQUR));
        	axgbe_printf(1, "MTL_Q_TQDR Reg (%08x) = %08x\n",
		    MTL_Q_TQDR,	XGMAC_MTL_IOREAD(pdata, i, MTL_Q_TQDR));
        	axgbe_printf(1, "MTL_Q_TC0ETSCR Reg (%08x) = %08x\n",
		    MTL_Q_TC0ETSCR, XGMAC_MTL_IOREAD(pdata, i, MTL_Q_TC0ETSCR));
        	axgbe_printf(1, "MTL_Q_TC0ETSSR Reg (%08x) = %08x\n",
		    MTL_Q_TC0ETSSR, XGMAC_MTL_IOREAD(pdata, i, MTL_Q_TC0ETSSR));
        	axgbe_printf(1, "MTL_Q_TC0QWR Reg (%08x) = %08x\n",
		    MTL_Q_TC0QWR, XGMAC_MTL_IOREAD(pdata, i, MTL_Q_TC0QWR));

        	axgbe_printf(1, "MTL_Q_RQOMR Reg (%08x) = %08x\n",
		    MTL_Q_RQOMR, XGMAC_MTL_IOREAD(pdata, i, MTL_Q_RQOMR));
        	axgbe_printf(1, "MTL_Q_RQMPOCR Reg (%08x) = %08x\n",
		    MTL_Q_RQMPOCR, XGMAC_MTL_IOREAD(pdata, i, MTL_Q_RQMPOCR));
        	axgbe_printf(1, "MTL_Q_RQDR Reg (%08x) = %08x\n",
		    MTL_Q_RQDR,	XGMAC_MTL_IOREAD(pdata, i, MTL_Q_RQDR));
        	axgbe_printf(1, "MTL_Q_RQCR Reg (%08x) = %08x\n",
		    MTL_Q_RQCR,	XGMAC_MTL_IOREAD(pdata, i, MTL_Q_RQCR));
        	axgbe_printf(1, "MTL_Q_RQFCR Reg (%08x) = %08x\n",
		    MTL_Q_RQFCR, XGMAC_MTL_IOREAD(pdata, i, MTL_Q_RQFCR));
        	axgbe_printf(1, "MTL_Q_IER Reg (%08x) = %08x\n",
		    MTL_Q_IER, XGMAC_MTL_IOREAD(pdata, i, MTL_Q_IER));
        	axgbe_printf(1, "MTL_Q_ISR Reg (%08x) = %08x\n",
		    MTL_Q_ISR, XGMAC_MTL_IOREAD(pdata, i, MTL_Q_ISR));
	}
}

static void
xgbe_dump_mac_registers(struct xgbe_prv_data *pdata)
{
        axgbe_printf(1, "\n************* MAC Reg dump **********************\n");

        axgbe_printf(1, "MAC TCR Reg (%08x) = %08x\n", MAC_TCR,
           XGMAC_IOREAD(pdata, MAC_TCR));
        axgbe_printf(1, "MAC RCR Reg (%08x) = %08x\n", MAC_RCR,
           XGMAC_IOREAD(pdata, MAC_RCR));
        axgbe_printf(1, "MAC PFR Reg (%08x) = %08x\n", MAC_PFR,
           XGMAC_IOREAD(pdata, MAC_PFR));
        axgbe_printf(1, "MAC WTR Reg (%08x) = %08x\n", MAC_WTR,
           XGMAC_IOREAD(pdata, MAC_WTR));
        axgbe_printf(1, "MAC HTR0 Reg (%08x) = %08x\n", MAC_HTR0,
           XGMAC_IOREAD(pdata, MAC_HTR0));
        axgbe_printf(1, "MAC HTR1 Reg (%08x) = %08x\n", MAC_HTR1,
           XGMAC_IOREAD(pdata, MAC_HTR1));
        axgbe_printf(1, "MAC HTR2 Reg (%08x) = %08x\n", MAC_HTR2,
           XGMAC_IOREAD(pdata, MAC_HTR2));
        axgbe_printf(1, "MAC HTR3 Reg (%08x) = %08x\n", MAC_HTR3,
           XGMAC_IOREAD(pdata, MAC_HTR3));
        axgbe_printf(1, "MAC HTR4 Reg (%08x) = %08x\n", MAC_HTR4,
           XGMAC_IOREAD(pdata, MAC_HTR4));
        axgbe_printf(1, "MAC HTR5 Reg (%08x) = %08x\n", MAC_HTR5,
           XGMAC_IOREAD(pdata, MAC_HTR5));
        axgbe_printf(1, "MAC HTR6 Reg (%08x) = %08x\n", MAC_HTR6,
           XGMAC_IOREAD(pdata, MAC_HTR6));
        axgbe_printf(1, "MAC HTR7 Reg (%08x) = %08x\n", MAC_HTR7,
           XGMAC_IOREAD(pdata, MAC_HTR7));
        axgbe_printf(1, "MAC VLANTR Reg (%08x) = %08x\n", MAC_VLANTR,
           XGMAC_IOREAD(pdata, MAC_VLANTR));
        axgbe_printf(1, "MAC VLANHTR Reg (%08x) = %08x\n", MAC_VLANHTR,
           XGMAC_IOREAD(pdata, MAC_VLANHTR));
        axgbe_printf(1, "MAC VLANIR Reg (%08x) = %08x\n", MAC_VLANIR,
           XGMAC_IOREAD(pdata, MAC_VLANIR));
        axgbe_printf(1, "MAC IVLANIR Reg (%08x) = %08x\n", MAC_IVLANIR,
           XGMAC_IOREAD(pdata, MAC_IVLANIR));
        axgbe_printf(1, "MAC RETMR Reg (%08x) = %08x\n", MAC_RETMR,
           XGMAC_IOREAD(pdata, MAC_RETMR));
        axgbe_printf(1, "MAC Q0TFCR Reg (%08x) = %08x\n", MAC_Q0TFCR,
           XGMAC_IOREAD(pdata, MAC_Q0TFCR));
        axgbe_printf(1, "MAC Q1TFCR Reg (%08x) = %08x\n", MAC_Q1TFCR,
           XGMAC_IOREAD(pdata, MAC_Q1TFCR));
        axgbe_printf(1, "MAC Q2TFCR Reg (%08x) = %08x\n", MAC_Q2TFCR,
           XGMAC_IOREAD(pdata, MAC_Q2TFCR));
        axgbe_printf(1, "MAC Q3TFCR Reg (%08x) = %08x\n", MAC_Q3TFCR,
           XGMAC_IOREAD(pdata, MAC_Q3TFCR));
        axgbe_printf(1, "MAC Q4TFCR Reg (%08x) = %08x\n", MAC_Q4TFCR,
           XGMAC_IOREAD(pdata, MAC_Q4TFCR));
        axgbe_printf(1, "MAC Q5TFCR Reg (%08x) = %08x\n", MAC_Q5TFCR,
           XGMAC_IOREAD(pdata, MAC_Q5TFCR));
        axgbe_printf(1, "MAC Q6TFCR Reg (%08x) = %08x\n", MAC_Q6TFCR,
           XGMAC_IOREAD(pdata, MAC_Q6TFCR));
        axgbe_printf(1, "MAC Q7TFCR Reg (%08x) = %08x\n", MAC_Q7TFCR,
           XGMAC_IOREAD(pdata, MAC_Q7TFCR));
        axgbe_printf(1, "MAC RFCR Reg (%08x) = %08x\n", MAC_RFCR,
           XGMAC_IOREAD(pdata, MAC_RFCR));
        axgbe_printf(1, "MAC RQC0R Reg (%08x) = %08x\n", MAC_RQC0R,
           XGMAC_IOREAD(pdata, MAC_RQC0R));
        axgbe_printf(1, "MAC RQC1R Reg (%08x) = %08x\n", MAC_RQC1R,
           XGMAC_IOREAD(pdata, MAC_RQC1R));
        axgbe_printf(1, "MAC RQC2R Reg (%08x) = %08x\n", MAC_RQC2R,
           XGMAC_IOREAD(pdata, MAC_RQC2R));
        axgbe_printf(1, "MAC RQC3R Reg (%08x) = %08x\n", MAC_RQC3R,
           XGMAC_IOREAD(pdata, MAC_RQC3R));
        axgbe_printf(1, "MAC ISR Reg (%08x) = %08x\n", MAC_ISR,
           XGMAC_IOREAD(pdata, MAC_ISR));
        axgbe_printf(1, "MAC IER Reg (%08x) = %08x\n", MAC_IER,
           XGMAC_IOREAD(pdata, MAC_IER));
        axgbe_printf(1, "MAC RTSR Reg (%08x) = %08x\n", MAC_RTSR,
           XGMAC_IOREAD(pdata, MAC_RTSR));
        axgbe_printf(1, "MAC PMTCSR Reg (%08x) = %08x\n", MAC_PMTCSR,
           XGMAC_IOREAD(pdata, MAC_PMTCSR));
        axgbe_printf(1, "MAC RWKPFR Reg (%08x) = %08x\n", MAC_RWKPFR,
           XGMAC_IOREAD(pdata, MAC_RWKPFR));
        axgbe_printf(1, "MAC LPICSR Reg (%08x) = %08x\n", MAC_LPICSR,
           XGMAC_IOREAD(pdata, MAC_LPICSR));
        axgbe_printf(1, "MAC LPITCR Reg (%08x) = %08x\n", MAC_LPITCR,
           XGMAC_IOREAD(pdata, MAC_LPITCR));
        axgbe_printf(1, "MAC TIR Reg (%08x) = %08x\n", MAC_TIR,
           XGMAC_IOREAD(pdata, MAC_TIR));
        axgbe_printf(1, "MAC VR Reg (%08x) = %08x\n", MAC_VR,
           XGMAC_IOREAD(pdata, MAC_VR));
	axgbe_printf(1, "MAC DR Reg (%08x) = %08x\n", MAC_DR,
           XGMAC_IOREAD(pdata, MAC_DR));
        axgbe_printf(1, "MAC HWF0R Reg (%08x) = %08x\n", MAC_HWF0R,
           XGMAC_IOREAD(pdata, MAC_HWF0R));
        axgbe_printf(1, "MAC HWF1R Reg (%08x) = %08x\n", MAC_HWF1R,
           XGMAC_IOREAD(pdata, MAC_HWF1R));
        axgbe_printf(1, "MAC HWF2R Reg (%08x) = %08x\n", MAC_HWF2R,
           XGMAC_IOREAD(pdata, MAC_HWF2R));
        axgbe_printf(1, "MAC MDIOSCAR Reg (%08x) = %08x\n", MAC_MDIOSCAR,
           XGMAC_IOREAD(pdata, MAC_MDIOSCAR));
        axgbe_printf(1, "MAC MDIOSCCDR Reg (%08x) = %08x\n", MAC_MDIOSCCDR,
           XGMAC_IOREAD(pdata, MAC_MDIOSCCDR));
        axgbe_printf(1, "MAC MDIOISR Reg (%08x) = %08x\n", MAC_MDIOISR,
           XGMAC_IOREAD(pdata, MAC_MDIOISR));
        axgbe_printf(1, "MAC MDIOIER Reg (%08x) = %08x\n", MAC_MDIOIER,
           XGMAC_IOREAD(pdata, MAC_MDIOIER));
        axgbe_printf(1, "MAC MDIOCL22R Reg (%08x) = %08x\n", MAC_MDIOCL22R,
           XGMAC_IOREAD(pdata, MAC_MDIOCL22R));
        axgbe_printf(1, "MAC GPIOCR Reg (%08x) = %08x\n", MAC_GPIOCR,
           XGMAC_IOREAD(pdata, MAC_GPIOCR));
        axgbe_printf(1, "MAC GPIOSR Reg (%08x) = %08x\n", MAC_GPIOSR,
           XGMAC_IOREAD(pdata, MAC_GPIOSR));
        axgbe_printf(1, "MAC MACA0HR Reg (%08x) = %08x\n", MAC_MACA0HR,
           XGMAC_IOREAD(pdata, MAC_MACA0HR));
        axgbe_printf(1, "MAC MACA0LR Reg (%08x) = %08x\n", MAC_TCR,
           XGMAC_IOREAD(pdata, MAC_MACA0LR));
        axgbe_printf(1, "MAC MACA1HR Reg (%08x) = %08x\n", MAC_MACA1HR,
           XGMAC_IOREAD(pdata, MAC_MACA1HR));
        axgbe_printf(1, "MAC MACA1LR Reg (%08x) = %08x\n", MAC_MACA1LR,
           XGMAC_IOREAD(pdata, MAC_MACA1LR));
        axgbe_printf(1, "MAC RSSCR Reg (%08x) = %08x\n", MAC_RSSCR,
           XGMAC_IOREAD(pdata, MAC_RSSCR));
        axgbe_printf(1, "MAC RSSDR Reg (%08x) = %08x\n", MAC_RSSDR,
           XGMAC_IOREAD(pdata, MAC_RSSDR));
        axgbe_printf(1, "MAC RSSAR Reg (%08x) = %08x\n", MAC_RSSAR,
           XGMAC_IOREAD(pdata, MAC_RSSAR));
        axgbe_printf(1, "MAC TSCR Reg (%08x) = %08x\n", MAC_TSCR,
           XGMAC_IOREAD(pdata, MAC_TSCR));
        axgbe_printf(1, "MAC SSIR Reg (%08x) = %08x\n", MAC_SSIR,
           XGMAC_IOREAD(pdata, MAC_SSIR));
        axgbe_printf(1, "MAC STSR Reg (%08x) = %08x\n", MAC_STSR,
           XGMAC_IOREAD(pdata, MAC_STSR));
        axgbe_printf(1, "MAC STNR Reg (%08x) = %08x\n", MAC_STNR,
           XGMAC_IOREAD(pdata, MAC_STNR));
        axgbe_printf(1, "MAC STSUR Reg (%08x) = %08x\n", MAC_STSUR,
           XGMAC_IOREAD(pdata, MAC_STSUR));
        axgbe_printf(1, "MAC STNUR Reg (%08x) = %08x\n", MAC_STNUR,
           XGMAC_IOREAD(pdata, MAC_STNUR));
        axgbe_printf(1, "MAC TSAR Reg (%08x) = %08x\n", MAC_TSAR,
           XGMAC_IOREAD(pdata, MAC_TSAR));
        axgbe_printf(1, "MAC TSSR Reg (%08x) = %08x\n", MAC_TSSR,
           XGMAC_IOREAD(pdata, MAC_TSSR));
        axgbe_printf(1, "MAC TXSNR Reg (%08x) = %08x\n", MAC_TXSNR,
           XGMAC_IOREAD(pdata, MAC_TXSNR));
	 axgbe_printf(1, "MAC TXSSR Reg (%08x) = %08x\n", MAC_TXSSR,
           XGMAC_IOREAD(pdata, MAC_TXSSR));
}

static void
xgbe_dump_rmon_counters(struct xgbe_prv_data *pdata)
{
        struct xgbe_mmc_stats *stats = &pdata->mmc_stats;

        axgbe_printf(1, "\n************* RMON counters dump ***************\n");

        pdata->hw_if.read_mmc_stats(pdata);

        axgbe_printf(1, "rmon txoctetcount_gb (%08x) = %08lx\n",
	    MMC_TXOCTETCOUNT_GB_LO, stats->txoctetcount_gb);
        axgbe_printf(1, "rmon txframecount_gb (%08x) = %08lx\n",
	    MMC_TXFRAMECOUNT_GB_LO, stats->txframecount_gb);
        axgbe_printf(1, "rmon txbroadcastframes_g (%08x) = %08lx\n",
	    MMC_TXBROADCASTFRAMES_G_LO, stats->txbroadcastframes_g);
        axgbe_printf(1, "rmon txmulticastframes_g (%08x) = %08lx\n",
	    MMC_TXMULTICASTFRAMES_G_LO, stats->txmulticastframes_g);
        axgbe_printf(1, "rmon tx64octets_gb (%08x) = %08lx\n",
	    MMC_TX64OCTETS_GB_LO, stats->tx64octets_gb);
        axgbe_printf(1, "rmon tx65to127octets_gb (%08x) = %08lx\n",
	    MMC_TX65TO127OCTETS_GB_LO, stats->tx65to127octets_gb);
        axgbe_printf(1, "rmon tx128to255octets_gb (%08x) = %08lx\n",
	    MMC_TX128TO255OCTETS_GB_LO, stats->tx128to255octets_gb);
        axgbe_printf(1, "rmon tx256to511octets_gb (%08x) = %08lx\n",
	    MMC_TX256TO511OCTETS_GB_LO, stats->tx256to511octets_gb);
        axgbe_printf(1, "rmon tx512to1023octets_gb (%08x) = %08lx\n",
	    MMC_TX512TO1023OCTETS_GB_LO, stats->tx512to1023octets_gb);
	axgbe_printf(1, "rmon tx1024tomaxoctets_gb (%08x) = %08lx\n",
	    MMC_TX1024TOMAXOCTETS_GB_LO, stats->tx1024tomaxoctets_gb);
        axgbe_printf(1, "rmon txunicastframes_gb (%08x) = %08lx\n",
	    MMC_TXUNICASTFRAMES_GB_LO, stats->txunicastframes_gb);
        axgbe_printf(1, "rmon txmulticastframes_gb (%08x) = %08lx\n",
	    MMC_TXMULTICASTFRAMES_GB_LO, stats->txmulticastframes_gb);
        axgbe_printf(1, "rmon txbroadcastframes_gb (%08x) = %08lx\n",
	    MMC_TXBROADCASTFRAMES_GB_LO, stats->txbroadcastframes_gb);
        axgbe_printf(1, "rmon txunderflowerror (%08x) = %08lx\n",
	    MMC_TXUNDERFLOWERROR_LO, stats->txunderflowerror);
        axgbe_printf(1, "rmon txoctetcount_g (%08x) = %08lx\n",
	    MMC_TXOCTETCOUNT_G_LO, stats->txoctetcount_g);
        axgbe_printf(1, "rmon txframecount_g (%08x) = %08lx\n",
	    MMC_TXFRAMECOUNT_G_LO, stats->txframecount_g);
        axgbe_printf(1, "rmon txpauseframes (%08x) = %08lx\n",
	    MMC_TXPAUSEFRAMES_LO, stats->txpauseframes);
        axgbe_printf(1, "rmon txvlanframes_g (%08x) = %08lx\n",
	    MMC_TXVLANFRAMES_G_LO, stats->txvlanframes_g);
        axgbe_printf(1, "rmon rxframecount_gb (%08x) = %08lx\n",
	    MMC_RXFRAMECOUNT_GB_LO, stats->rxframecount_gb);
        axgbe_printf(1, "rmon rxoctetcount_gb (%08x) = %08lx\n",
	    MMC_RXOCTETCOUNT_GB_LO, stats->rxoctetcount_gb);
        axgbe_printf(1, "rmon rxoctetcount_g (%08x) = %08lx\n",
	    MMC_RXOCTETCOUNT_G_LO, stats->rxoctetcount_g);
        axgbe_printf(1, "rmon rxbroadcastframes_g (%08x) = %08lx\n",
	    MMC_RXBROADCASTFRAMES_G_LO, stats->rxbroadcastframes_g);
        axgbe_printf(1, "rmon rxmulticastframes_g (%08x) = %08lx\n",
	    MMC_RXMULTICASTFRAMES_G_LO, stats->rxmulticastframes_g);
        axgbe_printf(1, "rmon rxcrcerror (%08x) = %08lx\n",
	    MMC_RXCRCERROR_LO, stats->rxcrcerror);
	axgbe_printf(1, "rmon rxrunterror (%08x) = %08lx\n",
	    MMC_RXRUNTERROR, stats->rxrunterror);
        axgbe_printf(1, "rmon rxjabbererror (%08x) = %08lx\n",
	    MMC_RXJABBERERROR, stats->rxjabbererror);
        axgbe_printf(1, "rmon rxundersize_g (%08x) = %08lx\n",
	    MMC_RXUNDERSIZE_G, stats->rxundersize_g);
        axgbe_printf(1, "rmon rxoversize_g (%08x) = %08lx\n",
	    MMC_RXOVERSIZE_G, stats->rxoversize_g);
        axgbe_printf(1, "rmon rx64octets_gb (%08x) = %08lx\n",
	    MMC_RX64OCTETS_GB_LO, stats->rx64octets_gb);
        axgbe_printf(1, "rmon rx65to127octets_gb (%08x) = %08lx\n",
	    MMC_RX65TO127OCTETS_GB_LO, stats->rx65to127octets_gb);
        axgbe_printf(1, "rmon rx128to255octets_gb (%08x) = %08lx\n",
	    MMC_RX128TO255OCTETS_GB_LO, stats->rx128to255octets_gb);
        axgbe_printf(1, "rmon rx256to511octets_gb (%08x) = %08lx\n",
	    MMC_RX256TO511OCTETS_GB_LO, stats->rx256to511octets_gb);
        axgbe_printf(1, "rmon rx512to1023octets_gb (%08x) = %08lx\n",
	    MMC_RX512TO1023OCTETS_GB_LO, stats->rx512to1023octets_gb);
        axgbe_printf(1, "rmon rx1024tomaxoctets_gb (%08x) = %08lx\n",
	    MMC_RX1024TOMAXOCTETS_GB_LO, stats->rx1024tomaxoctets_gb);
        axgbe_printf(1, "rmon rxunicastframes_g (%08x) = %08lx\n",
	    MMC_RXUNICASTFRAMES_G_LO, stats->rxunicastframes_g);
        axgbe_printf(1, "rmon rxlengtherror (%08x) = %08lx\n",
	    MMC_RXLENGTHERROR_LO, stats->rxlengtherror);
        axgbe_printf(1, "rmon rxoutofrangetype (%08x) = %08lx\n",
	    MMC_RXOUTOFRANGETYPE_LO, stats->rxoutofrangetype);
        axgbe_printf(1, "rmon rxpauseframes (%08x) = %08lx\n",
	    MMC_RXPAUSEFRAMES_LO, stats->rxpauseframes);
        axgbe_printf(1, "rmon rxfifooverflow (%08x) = %08lx\n",
	    MMC_RXFIFOOVERFLOW_LO, stats->rxfifooverflow);
	axgbe_printf(1, "rmon rxvlanframes_gb (%08x) = %08lx\n",
	    MMC_RXVLANFRAMES_GB_LO, stats->rxvlanframes_gb);
        axgbe_printf(1, "rmon rxwatchdogerror (%08x) = %08lx\n",
	    MMC_RXWATCHDOGERROR, stats->rxwatchdogerror);
}

void
xgbe_dump_i2c_registers(struct xgbe_prv_data *pdata)
{
          axgbe_printf(1, "*************** I2C Registers **************\n");
          axgbe_printf(1, "  IC_CON             : %010x\n",
	      XI2C_IOREAD(pdata, 0x00));
          axgbe_printf(1, "  IC_TAR             : %010x\n",
	      XI2C_IOREAD(pdata, 0x04));
          axgbe_printf(1, "  IC_HS_MADDR        : %010x\n",
	      XI2C_IOREAD(pdata, 0x0c));
          axgbe_printf(1, "  IC_INTR_STAT       : %010x\n",
	      XI2C_IOREAD(pdata, 0x2c));
          axgbe_printf(1, "  IC_INTR_MASK       : %010x\n",
	      XI2C_IOREAD(pdata, 0x30));
          axgbe_printf(1, "  IC_RAW_INTR_STAT   : %010x\n",
	      XI2C_IOREAD(pdata, 0x34));
          axgbe_printf(1, "  IC_RX_TL           : %010x\n",
	      XI2C_IOREAD(pdata, 0x38));
          axgbe_printf(1, "  IC_TX_TL           : %010x\n",
	      XI2C_IOREAD(pdata, 0x3c));
          axgbe_printf(1, "  IC_ENABLE          : %010x\n",
	      XI2C_IOREAD(pdata, 0x6c));
          axgbe_printf(1, "  IC_STATUS          : %010x\n",
	      XI2C_IOREAD(pdata, 0x70));
          axgbe_printf(1, "  IC_TXFLR           : %010x\n",
	      XI2C_IOREAD(pdata, 0x74));
          axgbe_printf(1, "  IC_RXFLR           : %010x\n",
	      XI2C_IOREAD(pdata, 0x78));
          axgbe_printf(1, "  IC_ENABLE_STATUS   : %010x\n",
	      XI2C_IOREAD(pdata, 0x9c));
          axgbe_printf(1, "  IC_COMP_PARAM1     : %010x\n",
	      XI2C_IOREAD(pdata, 0xf4));
}

static void
xgbe_dump_active_vlans(struct xgbe_prv_data *pdata)
{
	int i;

	for(i=0 ; i<BITS_TO_LONGS(VLAN_NVID); i++) {
		if (i && (i%8 == 0))
			axgbe_printf(1, "\n");
                axgbe_printf(1, "vlans[%d]: 0x%08lx ", i, pdata->active_vlans[i]);
	}
	axgbe_printf(1, "\n");
}

static void
xgbe_default_config(struct xgbe_prv_data *pdata)
{
        pdata->blen = DMA_SBMR_BLEN_64;
        pdata->pbl = DMA_PBL_128;
        pdata->aal = 1;
        pdata->rd_osr_limit = 8;
        pdata->wr_osr_limit = 8;
        pdata->tx_sf_mode = MTL_TSF_ENABLE;
        pdata->tx_threshold = MTL_TX_THRESHOLD_64;
        pdata->tx_osp_mode = DMA_OSP_ENABLE;
        pdata->rx_sf_mode = MTL_RSF_DISABLE;
        pdata->rx_threshold = MTL_RX_THRESHOLD_64;
        pdata->pause_autoneg = 1;
        pdata->tx_pause = 1;
        pdata->rx_pause = 1;
        pdata->phy_speed = SPEED_UNKNOWN;
        pdata->power_down = 0;
        pdata->enable_rss = 1;
}

static void
axgbe_setup_sysctl(struct xgbe_prv_data *pdata)
{
	struct sysctl_ctx_list *clist;
	struct sysctl_oid *parent;
	struct sysctl_oid_list *top;
	
	clist = device_get_sysctl_ctx(pdata->dev);
	parent = device_get_sysctl_tree(pdata->dev);
	top = SYSCTL_CHILDREN(parent);
}

static int
axgbe_if_attach_post(if_ctx_t ctx)
{
	struct axgbe_if_softc	*sc = iflib_get_softc(ctx);
	struct xgbe_prv_data	*pdata = &sc->pdata;
	struct ifnet		*ifp = pdata->netdev;
        struct xgbe_phy_if	*phy_if = &pdata->phy_if;
	struct xgbe_hw_if 	*hw_if = &pdata->hw_if;
	if_softc_ctx_t		scctx = sc->scctx;
	int i, ret;

	/* Initialize ECC timestamps */
        pdata->tx_sec_period = ticks;
        pdata->tx_ded_period = ticks;
        pdata->rx_sec_period = ticks;
        pdata->rx_ded_period = ticks;
        pdata->desc_sec_period = ticks;
        pdata->desc_ded_period = ticks;

	/* Reset the hardware */
	ret = hw_if->exit(&sc->pdata);
	if (ret)
		axgbe_error("%s: exit error %d\n", __func__, ret);

	/* Configure the defaults */
	xgbe_default_config(pdata);

	/* Set default max values if not provided */
        if (!pdata->tx_max_fifo_size)
                pdata->tx_max_fifo_size = pdata->hw_feat.tx_fifo_size;
        if (!pdata->rx_max_fifo_size)
                pdata->rx_max_fifo_size = pdata->hw_feat.rx_fifo_size;

	DBGPR("%s: tx fifo 0x%x rx fifo 0x%x\n", __func__,
	    pdata->tx_max_fifo_size, pdata->rx_max_fifo_size);

        /* Set and validate the number of descriptors for a ring */
        MPASS(powerof2(XGBE_TX_DESC_CNT));
        pdata->tx_desc_count = XGBE_TX_DESC_CNT;
        MPASS(powerof2(XGBE_RX_DESC_CNT));
        pdata->rx_desc_count = XGBE_RX_DESC_CNT;

        /* Adjust the number of queues based on interrupts assigned */
        if (pdata->channel_irq_count) {
                pdata->tx_ring_count = min_t(unsigned int, pdata->tx_ring_count,
		    pdata->channel_irq_count);
                pdata->rx_ring_count = min_t(unsigned int, pdata->rx_ring_count,
		    pdata->channel_irq_count);

		DBGPR("adjusted TX %u/%u RX %u/%u\n",
		    pdata->tx_ring_count, pdata->tx_q_count,
		    pdata->rx_ring_count, pdata->rx_q_count);
        }

	/* Set channel count based on interrupts assigned */	
	pdata->channel_count = max_t(unsigned int, scctx->isc_ntxqsets,
	    scctx->isc_nrxqsets);
	DBGPR("Channel count set to: %u\n", pdata->channel_count);

	/* Get RSS key */
#ifdef	RSS
	rss_getkey((uint8_t *)pdata->rss_key);
#else
	arc4rand(&pdata->rss_key, ARRAY_SIZE(pdata->rss_key), 0);
#endif
	XGMAC_SET_BITS(pdata->rss_options, MAC_RSSCR, IP2TE, 1);
	XGMAC_SET_BITS(pdata->rss_options, MAC_RSSCR, TCP4TE, 1);
	XGMAC_SET_BITS(pdata->rss_options, MAC_RSSCR, UDP4TE, 1);

	/* Initialize the PHY device */
	pdata->sysctl_an_cdr_workaround = pdata->vdata->an_cdr_workaround;
	phy_if->phy_init(pdata);

	/* Set the coalescing */
        xgbe_init_rx_coalesce(&sc->pdata);
        xgbe_init_tx_coalesce(&sc->pdata);

	ifmedia_add(sc->media, IFM_ETHER | IFM_10G_KR, 0, NULL);
	ifmedia_add(sc->media, IFM_ETHER | IFM_10G_T, 0, NULL);
	ifmedia_add(sc->media, IFM_ETHER | IFM_10G_SFI, 0, NULL);
	ifmedia_add(sc->media, IFM_ETHER | IFM_1000_KX, 0, NULL);
	ifmedia_add(sc->media, IFM_ETHER | IFM_1000_CX, 0, NULL);
	ifmedia_add(sc->media, IFM_ETHER | IFM_1000_LX, 0, NULL);
	ifmedia_add(sc->media, IFM_ETHER | IFM_1000_SX, 0, NULL);
	ifmedia_add(sc->media, IFM_ETHER | IFM_1000_T, 0, NULL);
	ifmedia_add(sc->media, IFM_ETHER | IFM_1000_SGMII, 0, NULL);
	ifmedia_add(sc->media, IFM_ETHER | IFM_100_TX, 0, NULL);
	ifmedia_add(sc->media, IFM_ETHER | IFM_100_SGMII, 0, NULL);
	ifmedia_add(sc->media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(sc->media, IFM_ETHER | IFM_AUTO);

	/* Initialize the phy */
	pdata->phy_link = -1;
	pdata->phy_speed = SPEED_UNKNOWN;
	ret = phy_if->phy_reset(pdata);
	if (ret)
		return (ret);

	/* Calculate the Rx buffer size before allocating rings */
	ret = xgbe_calc_rx_buf_size(pdata->netdev, if_getmtu(pdata->netdev));
	pdata->rx_buf_size = ret;
	DBGPR("%s: rx_buf_size %d\n", __func__, ret);

	/* Setup RSS lookup table */
	for (i = 0; i < XGBE_RSS_MAX_TABLE_SIZE; i++)
		XGMAC_SET_BITS(pdata->rss_table[i], MAC_RSSDR, DMCH,
				i % pdata->rx_ring_count);

	/* 
	 * Mark the device down until it is initialized, which happens
	 * when the device is accessed first (for configuring the iface,
	 * eg: setting IP)
	 */
	set_bit(XGBE_DOWN, &pdata->dev_state);

	DBGPR("mtu %d\n", ifp->if_mtu);
	scctx->isc_max_frame_size = ifp->if_mtu + 18;
	scctx->isc_min_frame_size = XGMAC_MIN_PACKET;

	axgbe_setup_sysctl(pdata);

	axgbe_sysctl_init(pdata);

	return (0);
} /* axgbe_if_attach_post */

static void
xgbe_free_intr(struct xgbe_prv_data *pdata, struct resource *res, void *tag,
		int rid)
{
	if (tag)
		bus_teardown_intr(pdata->dev, res, tag);

	if (res)
		bus_release_resource(pdata->dev, SYS_RES_IRQ, rid, res);
}

static void
axgbe_interrupts_free(if_ctx_t ctx)
{
	struct axgbe_if_softc   *sc = iflib_get_softc(ctx);
        struct xgbe_prv_data	*pdata = &sc->pdata;
        if_softc_ctx_t          scctx = sc->scctx;
        struct xgbe_channel     *channel;
        struct if_irq   irq;
        int i;

	axgbe_printf(2, "%s: mode %d\n", __func__, scctx->isc_intr);
	
	/* Free dev_irq */	
	iflib_irq_free(ctx, &pdata->dev_irq);

	/* Free ecc_irq */
	xgbe_free_intr(pdata, pdata->ecc_irq_res, pdata->ecc_irq_tag,
	    pdata->ecc_rid);

	/* Free i2c_irq */	
	xgbe_free_intr(pdata, pdata->i2c_irq_res, pdata->i2c_irq_tag,
	    pdata->i2c_rid);

	/* Free an_irq */
	xgbe_free_intr(pdata, pdata->an_irq_res, pdata->an_irq_tag,
	    pdata->an_rid);

	for (i = 0; i < scctx->isc_nrxqsets; i++) {

		channel = pdata->channel[i];
		axgbe_printf(2, "%s: rid %d\n", __func__, channel->dma_irq_rid);
		irq.ii_res = channel->dma_irq_res;
		irq.ii_tag = channel->dma_irq_tag;
		iflib_irq_free(ctx, &irq);
	}
}

static int
axgbe_if_detach(if_ctx_t ctx)
{
	struct axgbe_if_softc	*sc = iflib_get_softc(ctx);
	struct xgbe_prv_data	*pdata = &sc->pdata;
        struct xgbe_phy_if	*phy_if = &pdata->phy_if;
        struct resource *mac_res[2];

	mac_res[0] = pdata->xgmac_res;
	mac_res[1] = pdata->xpcs_res;

	phy_if->phy_exit(pdata);

	/* Free Interrupts */
	axgbe_interrupts_free(ctx);

	/* Free workqueues */
	taskqueue_free(pdata->dev_workqueue);

	/* Release bus resources */
	bus_release_resources(iflib_get_dev(ctx), axgbe_pci_mac_spec, mac_res);

	/* Free VLAN bitmap */
	free(pdata->active_vlans, M_AXGBE);

	axgbe_sysctl_exit(pdata);

	return (0);
} /* axgbe_if_detach */

static void
axgbe_pci_init(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy_if	*phy_if = &pdata->phy_if;
	struct xgbe_hw_if       *hw_if = &pdata->hw_if;
	int ret = 0;
	
	hw_if->init(pdata);

        ret = phy_if->phy_start(pdata);
        if (ret) {
		axgbe_error("%s:  phy start %d\n", __func__, ret);
		ret = hw_if->exit(pdata);
		if (ret)
			axgbe_error("%s: exit error %d\n", __func__, ret);
		return;
	}

	hw_if->enable_tx(pdata);
	hw_if->enable_rx(pdata);

	xgbe_start_timers(pdata);

	clear_bit(XGBE_DOWN, &pdata->dev_state);

	xgbe_dump_phy_registers(pdata);
	xgbe_dump_prop_registers(pdata);
	xgbe_dump_dma_registers(pdata, -1);
	xgbe_dump_mtl_registers(pdata);
	xgbe_dump_mac_registers(pdata);
	xgbe_dump_rmon_counters(pdata);
}

static void
axgbe_if_init(if_ctx_t ctx)
{
	struct axgbe_if_softc   *sc = iflib_get_softc(ctx);
	struct xgbe_prv_data    *pdata = &sc->pdata;	

	axgbe_pci_init(pdata);
}

static void
axgbe_pci_stop(if_ctx_t ctx)
{
	struct axgbe_if_softc   *sc = iflib_get_softc(ctx);
        struct xgbe_prv_data    *pdata = &sc->pdata;
	struct xgbe_phy_if	*phy_if = &pdata->phy_if;
	struct xgbe_hw_if       *hw_if = &pdata->hw_if;
	int ret;

	if (__predict_false(test_bit(XGBE_DOWN, &pdata->dev_state))) {
		axgbe_printf(1, "%s: Stopping when XGBE_DOWN\n", __func__);
		return;
	}

	xgbe_stop_timers(pdata);
	taskqueue_drain_all(pdata->dev_workqueue);

	hw_if->disable_tx(pdata);
	hw_if->disable_rx(pdata);

	phy_if->phy_stop(pdata);

	ret = hw_if->exit(pdata);
	if (ret)
		axgbe_error("%s: exit error %d\n", __func__, ret);

	set_bit(XGBE_DOWN, &pdata->dev_state);
}

static void
axgbe_if_stop(if_ctx_t ctx)
{
	axgbe_pci_stop(ctx);
}

static void
axgbe_if_disable_intr(if_ctx_t ctx)
{
	/* TODO - implement */
}

static void
axgbe_if_enable_intr(if_ctx_t ctx)
{
	/* TODO - implement */
}

static int
axgbe_if_tx_queues_alloc(if_ctx_t ctx, caddr_t *va, uint64_t *pa, int ntxqs,
    int ntxqsets)
{
	struct axgbe_if_softc	*sc = iflib_get_softc(ctx);
	struct xgbe_prv_data 	*pdata = &sc->pdata;
	if_softc_ctx_t		scctx = sc->scctx;
	struct xgbe_channel	*channel;
	struct xgbe_ring	*tx_ring;
	int			i, j, k;

	MPASS(scctx->isc_ntxqsets > 0);
	MPASS(scctx->isc_ntxqsets == ntxqsets);
	MPASS(ntxqs == 1);

	axgbe_printf(1, "%s: txqsets %d/%d txqs %d\n", __func__,
	    scctx->isc_ntxqsets, ntxqsets, ntxqs);	

	for (i = 0 ; i < ntxqsets; i++) {

		channel = pdata->channel[i];

		tx_ring = (struct xgbe_ring*)malloc(ntxqs *
		    sizeof(struct xgbe_ring), M_AXGBE, M_NOWAIT | M_ZERO);

		if (tx_ring == NULL) {
			axgbe_error("Unable to allocate TX ring memory\n");
			goto tx_ring_fail;
		}

		channel->tx_ring = tx_ring;

		for (j = 0; j < ntxqs; j++, tx_ring++) {
			tx_ring->rdata =
			    (struct xgbe_ring_data*)malloc(scctx->isc_ntxd[j] *
			    sizeof(struct xgbe_ring_data), M_AXGBE, M_NOWAIT);

			/* Get the virtual & physical address of hw queues */
			tx_ring->rdesc = (struct xgbe_ring_desc *)va[i*ntxqs + j];
			tx_ring->rdesc_paddr = pa[i*ntxqs + j];
			tx_ring->rdesc_count = scctx->isc_ntxd[j];
			spin_lock_init(&tx_ring->lock);
		}
	}

	axgbe_printf(1, "allocated for %d tx queues\n", scctx->isc_ntxqsets);

	return (0);

tx_ring_fail:

	for (j = 0; j < i ; j++) {

		channel = pdata->channel[j];

		tx_ring = channel->tx_ring;
		for (k = 0; k < ntxqs ; k++, tx_ring++) {
			if (tx_ring && tx_ring->rdata)
				free(tx_ring->rdata, M_AXGBE);
		}
		free(channel->tx_ring, M_AXGBE);

		channel->tx_ring = NULL;
	}

	return (ENOMEM);

} /* axgbe_if_tx_queues_alloc */

static int
axgbe_if_rx_queues_alloc(if_ctx_t ctx, caddr_t *va, uint64_t *pa, int nrxqs,
    int nrxqsets)
{
	struct axgbe_if_softc	*sc = iflib_get_softc(ctx);
	struct xgbe_prv_data 	*pdata = &sc->pdata;
	if_softc_ctx_t		scctx = sc->scctx;
	struct xgbe_channel	*channel;
	struct xgbe_ring	*rx_ring;
	int			i, j, k;

	MPASS(scctx->isc_nrxqsets > 0);
	MPASS(scctx->isc_nrxqsets == nrxqsets);
	MPASS(nrxqs == 2);

	axgbe_printf(1, "%s: rxqsets %d/%d rxqs %d\n", __func__,
	    scctx->isc_nrxqsets, nrxqsets, nrxqs);	

	for (i = 0 ; i < nrxqsets; i++) {

		channel = pdata->channel[i];

		rx_ring = (struct xgbe_ring*)malloc(nrxqs *
		    sizeof(struct xgbe_ring), M_AXGBE, M_NOWAIT | M_ZERO);

		if (rx_ring == NULL) {
			axgbe_error("Unable to allocate RX ring memory\n");
			goto rx_ring_fail;
		}

		channel->rx_ring = rx_ring;

		for (j = 0; j < nrxqs; j++, rx_ring++) {
			rx_ring->rdata =
			    (struct xgbe_ring_data*)malloc(scctx->isc_nrxd[j] *
			    sizeof(struct xgbe_ring_data), M_AXGBE, M_NOWAIT);

			/* Get the virtual and physical address of the hw queues */
			rx_ring->rdesc = (struct xgbe_ring_desc *)va[i*nrxqs + j];
			rx_ring->rdesc_paddr = pa[i*nrxqs + j];
			rx_ring->rdesc_count = scctx->isc_nrxd[j];
			spin_lock_init(&rx_ring->lock);
		}
	}

	axgbe_printf(2, "allocated for %d rx queues\n", scctx->isc_nrxqsets);

	return (0);

rx_ring_fail:

	for (j = 0 ; j < i ; j++) {

		channel = pdata->channel[j];

		rx_ring = channel->rx_ring;
		for (k = 0; k < nrxqs ; k++, rx_ring++) {
			if (rx_ring && rx_ring->rdata)
				free(rx_ring->rdata, M_AXGBE);
		}
		free(channel->rx_ring, M_AXGBE);

		channel->rx_ring = NULL;
	}

	return (ENOMEM);

} /* axgbe_if_rx_queues_alloc */

static void
axgbe_if_queues_free(if_ctx_t ctx)
{
	struct axgbe_if_softc	*sc = iflib_get_softc(ctx);
	struct xgbe_prv_data 	*pdata = &sc->pdata;
	if_softc_ctx_t		scctx = sc->scctx;
	if_shared_ctx_t		sctx = sc->sctx;
	struct xgbe_channel	*channel;
	struct xgbe_ring        *tx_ring;
	struct xgbe_ring        *rx_ring;
	int i, j;

	for (i = 0 ; i < scctx->isc_ntxqsets; i++) {

		channel = pdata->channel[i];

		tx_ring = channel->tx_ring;
		for (j = 0; j < sctx->isc_ntxqs ; j++, tx_ring++) {
			if (tx_ring && tx_ring->rdata)
				free(tx_ring->rdata, M_AXGBE);
		}
		free(channel->tx_ring, M_AXGBE);
		channel->tx_ring = NULL;
	}

	for (i = 0 ; i < scctx->isc_nrxqsets; i++) {

		channel = pdata->channel[i];

		rx_ring = channel->rx_ring;
		for (j = 0; j < sctx->isc_nrxqs ; j++, rx_ring++) {
			if (rx_ring && rx_ring->rdata)
				free(rx_ring->rdata, M_AXGBE);
		}
		free(channel->rx_ring, M_AXGBE);
		channel->rx_ring = NULL;
	}

	/* Free Channels */
	for (i = 0; i < pdata->total_channel_count ; i++) {
		free(pdata->channel[i], M_AXGBE);
		pdata->channel[i] = NULL;
	}

	pdata->total_channel_count = 0;
	pdata->channel_count = 0;
} /* axgbe_if_queues_free */

static void
axgbe_if_vlan_register(if_ctx_t ctx, uint16_t vtag)
{
	struct axgbe_if_softc	*sc = iflib_get_softc(ctx);
	struct xgbe_prv_data 	*pdata = &sc->pdata;
	struct xgbe_hw_if 	*hw_if = &pdata->hw_if;

	if (!bit_test(pdata->active_vlans, vtag)) {
		axgbe_printf(0, "Registering VLAN %d\n", vtag);

		bit_set(pdata->active_vlans, vtag);
		hw_if->update_vlan_hash_table(pdata);
		pdata->num_active_vlans++;

		axgbe_printf(1, "Total active vlans: %d\n",
		    pdata->num_active_vlans);	
	} else
		axgbe_printf(0, "VLAN %d already registered\n", vtag);

	xgbe_dump_active_vlans(pdata);
}

static void
axgbe_if_vlan_unregister(if_ctx_t ctx, uint16_t vtag)
{
	struct axgbe_if_softc	*sc = iflib_get_softc(ctx);
	struct xgbe_prv_data 	*pdata = &sc->pdata;
	struct xgbe_hw_if 	*hw_if = &pdata->hw_if;

	if (pdata->num_active_vlans == 0) {
		axgbe_printf(1, "No active VLANs to unregister\n");
		return;
	}

	if (bit_test(pdata->active_vlans, vtag)){
		axgbe_printf(0, "Un-Registering VLAN %d\n", vtag);

		bit_clear(pdata->active_vlans, vtag);
		hw_if->update_vlan_hash_table(pdata);
		pdata->num_active_vlans--;

		axgbe_printf(1, "Total active vlans: %d\n",
		    pdata->num_active_vlans);	
	} else
		axgbe_printf(0, "VLAN %d already unregistered\n", vtag);

	xgbe_dump_active_vlans(pdata);
}

#if __FreeBSD_version >= 1300000
static bool
axgbe_if_needs_restart(if_ctx_t ctx __unused, enum iflib_restart_event event)
{
        switch (event) {
        case IFLIB_RESTART_VLAN_CONFIG:
        default:
                return (true);
        }
}
#endif

static int
axgbe_if_msix_intr_assign(if_ctx_t ctx, int msix)
{
	struct axgbe_if_softc	*sc = iflib_get_softc(ctx);
	struct xgbe_prv_data 	*pdata = &sc->pdata;
	if_softc_ctx_t		scctx = sc->scctx;
	struct xgbe_channel	*channel;
	struct if_irq		irq;
	int			i, error, rid = 0, flags;
	char			buf[16];

	MPASS(scctx->isc_intr != IFLIB_INTR_LEGACY);

	pdata->isr_as_tasklet = 1;

	if (scctx->isc_intr == IFLIB_INTR_MSI) {	
		pdata->irq_count = 1;
		pdata->channel_irq_count = 1;
		return (0);
	}

	axgbe_printf(1, "%s: msix %d txqsets %d rxqsets %d\n", __func__, msix,
	    scctx->isc_ntxqsets, scctx->isc_nrxqsets);

	flags = RF_ACTIVE;

	/* DEV INTR SETUP */
	rid++;
	error = iflib_irq_alloc_generic(ctx, &pdata->dev_irq, rid,
	    IFLIB_INTR_ADMIN, axgbe_dev_isr, sc, 0, "dev_irq");
	if (error) {
		axgbe_error("Failed to register device interrupt rid %d name %s\n",
		    rid, "dev_irq");
		return (error);
	}

	/* ECC INTR SETUP */
	rid++;
	pdata->ecc_rid = rid;
	pdata->ecc_irq_res = bus_alloc_resource_any(pdata->dev, SYS_RES_IRQ,
	    &rid, flags);
	if (!pdata->ecc_irq_res) {
		axgbe_error("failed to allocate IRQ for rid %d, name %s.\n",
		    rid, "ecc_irq");
		return (ENOMEM);
	}

	error = bus_setup_intr(pdata->dev, pdata->ecc_irq_res, INTR_MPSAFE |
	    INTR_TYPE_NET, NULL, axgbe_ecc_isr, sc, &pdata->ecc_irq_tag);
        if (error) {
                axgbe_error("failed to setup interrupt for rid %d, name %s: %d\n",
		    rid, "ecc_irq", error);
                return (error);
	}

	/* I2C INTR SETUP */
	rid++;
	pdata->i2c_rid = rid;
        pdata->i2c_irq_res = bus_alloc_resource_any(pdata->dev, SYS_RES_IRQ,
	    &rid, flags);
        if (!pdata->i2c_irq_res) {
                axgbe_error("failed to allocate IRQ for rid %d, name %s.\n",
		    rid, "i2c_irq");
                return (ENOMEM);
        }

        error = bus_setup_intr(pdata->dev, pdata->i2c_irq_res, INTR_MPSAFE |
	    INTR_TYPE_NET, NULL, axgbe_i2c_isr, sc, &pdata->i2c_irq_tag);
        if (error) {
                axgbe_error("failed to setup interrupt for rid %d, name %s: %d\n",
		    rid, "i2c_irq", error);
                return (error);
	}

	/* AN INTR SETUP */
	rid++;
	pdata->an_rid = rid;
        pdata->an_irq_res = bus_alloc_resource_any(pdata->dev, SYS_RES_IRQ,
	    &rid, flags);
        if (!pdata->an_irq_res) {
                axgbe_error("failed to allocate IRQ for rid %d, name %s.\n",
		    rid, "an_irq");
                return (ENOMEM);
        }

        error = bus_setup_intr(pdata->dev, pdata->an_irq_res, INTR_MPSAFE |
	    INTR_TYPE_NET, NULL, axgbe_an_isr, sc, &pdata->an_irq_tag);
        if (error) {
                axgbe_error("failed to setup interrupt for rid %d, name %s: %d\n",
		    rid, "an_irq", error);
                return (error);
	}

	pdata->per_channel_irq = 1;
	pdata->channel_irq_mode = XGBE_IRQ_MODE_LEVEL;
	rid++;
	for (i = 0; i < scctx->isc_nrxqsets; i++, rid++) {

		channel = pdata->channel[i];

		snprintf(buf, sizeof(buf), "rxq%d", i);
		error = iflib_irq_alloc_generic(ctx, &irq, rid, IFLIB_INTR_RX,
		    axgbe_msix_que, channel, channel->queue_index, buf);

		if (error) {
			axgbe_error("Failed to allocated que int %d err: %d\n",
			    i, error);
			return (error);
		}

		channel->dma_irq_rid = rid;
		channel->dma_irq_res = irq.ii_res;
		channel->dma_irq_tag = irq.ii_tag;
		axgbe_printf(1, "%s: channel count %d idx %d irq %d\n",
		    __func__, scctx->isc_nrxqsets, i, rid);
	}
	pdata->irq_count = msix;
	pdata->channel_irq_count = scctx->isc_nrxqsets;

	for (i = 0; i < scctx->isc_ntxqsets; i++) {

		channel = pdata->channel[i];

		snprintf(buf, sizeof(buf), "txq%d", i);
		irq.ii_res = channel->dma_irq_res;
		iflib_softirq_alloc_generic(ctx, &irq, IFLIB_INTR_TX, channel,
		    channel->queue_index, buf);
	}

	return (0);
} /* axgbe_if_msix_intr_assign */

static int
xgbe_enable_rx_tx_int(struct xgbe_prv_data *pdata, struct xgbe_channel *channel)
{
        struct xgbe_hw_if *hw_if = &pdata->hw_if;
        enum xgbe_int int_id;

	if (channel->tx_ring && channel->rx_ring)
		int_id = XGMAC_INT_DMA_CH_SR_TI_RI;
	else if (channel->tx_ring)
		int_id = XGMAC_INT_DMA_CH_SR_TI;
	else if (channel->rx_ring)
		int_id = XGMAC_INT_DMA_CH_SR_RI;
	else
		return (-1);

	axgbe_printf(1, "%s channel: %d rx_tx interrupt enabled %d\n",
	    __func__, channel->queue_index, int_id);
        return (hw_if->enable_int(channel, int_id));
}

static void
xgbe_disable_rx_tx_int(struct xgbe_prv_data *pdata, struct xgbe_channel *channel)
{
        struct xgbe_hw_if *hw_if = &pdata->hw_if;
        enum xgbe_int int_id;

        if (channel->tx_ring && channel->rx_ring)
                int_id = XGMAC_INT_DMA_CH_SR_TI_RI;
        else if (channel->tx_ring)
                int_id = XGMAC_INT_DMA_CH_SR_TI;
        else if (channel->rx_ring)
                int_id = XGMAC_INT_DMA_CH_SR_RI;
        else
                return;

	axgbe_printf(1, "%s channel: %d rx_tx interrupt disabled %d\n",
	    __func__, channel->queue_index, int_id);
        hw_if->disable_int(channel, int_id);
}

static void
xgbe_disable_rx_tx_ints(struct xgbe_prv_data *pdata)
{
        unsigned int i;

        for (i = 0; i < pdata->channel_count; i++)
                xgbe_disable_rx_tx_int(pdata, pdata->channel[i]);
}

static int
axgbe_msix_que(void *arg)
{
	struct xgbe_channel	*channel = (struct xgbe_channel *)arg;
	struct xgbe_prv_data	*pdata = channel->pdata;
	unsigned int 		dma_ch_isr, dma_status;

	axgbe_printf(1, "%s: Channel: %d SR 0x%04x DSR 0x%04x IER:0x%04x D_ISR:0x%04x M_ISR:0x%04x\n",
	    __func__, channel->queue_index,
	    XGMAC_DMA_IOREAD(channel, DMA_CH_SR),
	    XGMAC_DMA_IOREAD(channel, DMA_CH_DSR),
	    XGMAC_DMA_IOREAD(channel, DMA_CH_IER),
	    XGMAC_IOREAD(pdata, DMA_ISR),
	    XGMAC_IOREAD(pdata, MAC_ISR));

	dma_ch_isr = XGMAC_DMA_IOREAD(channel, DMA_CH_SR);

	/* Disable Tx and Rx channel interrupts */
	xgbe_disable_rx_tx_int(pdata, channel);

	/* Clear the interrupts */
	dma_status = 0;
	XGMAC_SET_BITS(dma_status, DMA_CH_SR, TI, 1);
	XGMAC_SET_BITS(dma_status, DMA_CH_SR, RI, 1);
	XGMAC_DMA_IOWRITE(channel, DMA_CH_SR, dma_status);

	return (FILTER_SCHEDULE_THREAD);
}

static int
axgbe_dev_isr(void *arg)
{
	struct axgbe_if_softc *sc = (struct axgbe_if_softc *)arg;
	struct xgbe_prv_data	*pdata = &sc->pdata;
	struct xgbe_channel	*channel;
	struct xgbe_hw_if	*hw_if = &pdata->hw_if;
	unsigned int		i, dma_isr, dma_ch_isr;
	unsigned int		mac_isr, mac_mdioisr;
	int ret = FILTER_HANDLED;

	dma_isr = XGMAC_IOREAD(pdata, DMA_ISR);
	axgbe_printf(2, "%s DMA ISR: 0x%x\n", __func__, dma_isr);

        if (!dma_isr)
                return (FILTER_HANDLED);

        for (i = 0; i < pdata->channel_count; i++) {

                if (!(dma_isr & (1 << i)))
                        continue;

                channel = pdata->channel[i];

                dma_ch_isr = XGMAC_DMA_IOREAD(channel, DMA_CH_SR);
		axgbe_printf(2, "%s: channel %d SR 0x%x DSR 0x%x\n", __func__,
		    channel->queue_index, dma_ch_isr, XGMAC_DMA_IOREAD(channel,
		    DMA_CH_DSR));

                /*
		 * The TI or RI interrupt bits may still be set even if using
                 * per channel DMA interrupts. Check to be sure those are not
                 * enabled before using the private data napi structure.
                 */
		if (!pdata->per_channel_irq &&
		    (XGMAC_GET_BITS(dma_ch_isr, DMA_CH_SR, TI) ||
		    XGMAC_GET_BITS(dma_ch_isr, DMA_CH_SR, RI))) {

			/* Disable Tx and Rx interrupts */
			xgbe_disable_rx_tx_ints(pdata);
                } else {

			/*
			 * Don't clear Rx/Tx status if doing per channel DMA
			 * interrupts, these will be cleared by the ISR for
		 	 * per channel DMA interrupts
		 	 */
                	XGMAC_SET_BITS(dma_ch_isr, DMA_CH_SR, TI, 0);
                	XGMAC_SET_BITS(dma_ch_isr, DMA_CH_SR, RI, 0);
		}

                if (XGMAC_GET_BITS(dma_ch_isr, DMA_CH_SR, RBU))
                        pdata->ext_stats.rx_buffer_unavailable++;

                /* Restart the device on a Fatal Bus Error */
                if (XGMAC_GET_BITS(dma_ch_isr, DMA_CH_SR, FBE))
			axgbe_error("%s: Fatal bus error reported 0x%x\n",
			    __func__, dma_ch_isr);

                /* Clear all interrupt signals */
                XGMAC_DMA_IOWRITE(channel, DMA_CH_SR, dma_ch_isr);

		ret = FILTER_SCHEDULE_THREAD;
        }

        if (XGMAC_GET_BITS(dma_isr, DMA_ISR, MACIS)) {

                mac_isr = XGMAC_IOREAD(pdata, MAC_ISR);
		axgbe_printf(2, "%s MAC ISR: 0x%x\n", __func__, mac_isr);

                if (XGMAC_GET_BITS(mac_isr, MAC_ISR, MMCTXIS))
                        hw_if->tx_mmc_int(pdata);

                if (XGMAC_GET_BITS(mac_isr, MAC_ISR, MMCRXIS))
                        hw_if->rx_mmc_int(pdata);

		if (XGMAC_GET_BITS(mac_isr, MAC_ISR, SMI)) {
			mac_mdioisr = XGMAC_IOREAD(pdata, MAC_MDIOISR);

			if (XGMAC_GET_BITS(mac_mdioisr, MAC_MDIOISR,
			    SNGLCOMPINT))
				wakeup_one(pdata);
		}

	}

	return (ret);
} /* axgbe_dev_isr */

static void
axgbe_i2c_isr(void *arg)
{
	struct axgbe_if_softc *sc = (struct axgbe_if_softc *)arg;

	sc->pdata.i2c_if.i2c_isr(&sc->pdata);	
}

static void
axgbe_ecc_isr(void *arg)
{
	/* TODO - implement */
}

static void
axgbe_an_isr(void *arg)
{
	struct axgbe_if_softc *sc = (struct axgbe_if_softc *)arg;

	sc->pdata.phy_if.an_isr(&sc->pdata);
}

static int
axgbe_if_tx_queue_intr_enable(if_ctx_t ctx, uint16_t qid)
{
	struct axgbe_if_softc	*sc = iflib_get_softc(ctx);
	struct xgbe_prv_data 	*pdata = &sc->pdata;
	int ret;

	if (qid < pdata->tx_q_count) {
		ret = xgbe_enable_rx_tx_int(pdata, pdata->channel[qid]);
		if (ret) {
			axgbe_error("Enable TX INT failed\n");
			return (ret);
		}
	} else
		axgbe_error("Queue ID exceed channel count\n");

	return (0);
}

static int
axgbe_if_rx_queue_intr_enable(if_ctx_t ctx, uint16_t qid)
{
	struct axgbe_if_softc	*sc = iflib_get_softc(ctx);
	struct xgbe_prv_data 	*pdata = &sc->pdata;
	int ret;

	if (qid < pdata->rx_q_count) {
		ret = xgbe_enable_rx_tx_int(pdata, pdata->channel[qid]);
		if (ret) {
			axgbe_error("Enable RX INT failed\n");
			return (ret);
		}
	} else
		axgbe_error("Queue ID exceed channel count\n");

	return (0);
}

static void
axgbe_if_update_admin_status(if_ctx_t ctx)
{
	struct axgbe_if_softc	*sc = iflib_get_softc(ctx);
	struct xgbe_prv_data 	*pdata = &sc->pdata;

	axgbe_printf(1, "%s: phy_link %d status %d speed %d\n", __func__,
	    pdata->phy_link, sc->link_status, pdata->phy.speed);

	if (pdata->phy_link < 0)
		return;

	if (pdata->phy_link) {
		if (sc->link_status == LINK_STATE_DOWN) {
			sc->link_status = LINK_STATE_UP;
			if (pdata->phy.speed & SPEED_10000)  
				iflib_link_state_change(ctx, LINK_STATE_UP,
				    IF_Gbps(10));
			else if (pdata->phy.speed & SPEED_2500)  
				iflib_link_state_change(ctx, LINK_STATE_UP,
				    IF_Gbps(2.5));
			else if (pdata->phy.speed & SPEED_1000)  
				iflib_link_state_change(ctx, LINK_STATE_UP,
				    IF_Gbps(1));
			else if (pdata->phy.speed & SPEED_100)  
				iflib_link_state_change(ctx, LINK_STATE_UP,
				    IF_Mbps(100));
			else if (pdata->phy.speed & SPEED_10)  
				iflib_link_state_change(ctx, LINK_STATE_UP,
				    IF_Mbps(10));
		}
	} else {
		if (sc->link_status == LINK_STATE_UP) {
			sc->link_status = LINK_STATE_DOWN;
			iflib_link_state_change(ctx, LINK_STATE_DOWN, 0);
		}
	}
}

static int
axgbe_if_media_change(if_ctx_t ctx)
{
        struct axgbe_if_softc   *sc = iflib_get_softc(ctx);
        struct ifmedia          *ifm = iflib_get_media(ctx);

        sx_xlock(&sc->pdata.an_mutex);
        if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
                return (EINVAL);

        switch (IFM_SUBTYPE(ifm->ifm_media)) {
        case IFM_10G_KR:
                sc->pdata.phy.speed = SPEED_10000;
                sc->pdata.phy.autoneg = AUTONEG_DISABLE;
                break;
        case IFM_2500_KX:
                sc->pdata.phy.speed = SPEED_2500;
                sc->pdata.phy.autoneg = AUTONEG_DISABLE;
                break;
        case IFM_1000_KX:
                sc->pdata.phy.speed = SPEED_1000;
                sc->pdata.phy.autoneg = AUTONEG_DISABLE;
                break;
        case IFM_100_TX:
                sc->pdata.phy.speed = SPEED_100;
                sc->pdata.phy.autoneg = AUTONEG_DISABLE;
                break;
        case IFM_AUTO:
                sc->pdata.phy.autoneg = AUTONEG_ENABLE;
                break;
        }
        sx_xunlock(&sc->pdata.an_mutex);

        return (-sc->pdata.phy_if.phy_config_aneg(&sc->pdata));
}

static int
axgbe_if_promisc_set(if_ctx_t ctx, int flags)
{
        struct axgbe_if_softc *sc = iflib_get_softc(ctx);

        if (XGMAC_IOREAD_BITS(&sc->pdata, MAC_PFR, PR) == 1)
                return (0);

        XGMAC_IOWRITE_BITS(&sc->pdata, MAC_PFR, PR, 1);
        XGMAC_IOWRITE_BITS(&sc->pdata, MAC_PFR, VTFE, 0);

        return (0);
}

static uint64_t
axgbe_if_get_counter(if_ctx_t ctx, ift_counter cnt)
{
	struct axgbe_if_softc	*sc = iflib_get_softc(ctx);
        struct ifnet		*ifp = iflib_get_ifp(ctx);
        struct xgbe_prv_data    *pdata = &sc->pdata;
        struct xgbe_mmc_stats	*pstats = &pdata->mmc_stats;

        pdata->hw_if.read_mmc_stats(pdata);

        switch(cnt) {
        case IFCOUNTER_IPACKETS:
                return (pstats->rxframecount_gb);
        case IFCOUNTER_IERRORS:
                return (pstats->rxframecount_gb - pstats->rxbroadcastframes_g -
                    pstats->rxmulticastframes_g - pstats->rxunicastframes_g);
        case IFCOUNTER_OPACKETS:
                return (pstats->txframecount_gb);
        case IFCOUNTER_OERRORS:
                return (pstats->txframecount_gb - pstats->txframecount_g);
        case IFCOUNTER_IBYTES:
                return (pstats->rxoctetcount_gb);
        case IFCOUNTER_OBYTES:
                return (pstats->txoctetcount_gb);
        default:
                return (if_get_counter_default(ifp, cnt));
        }
}

static int
axgbe_if_mtu_set(if_ctx_t ctx, uint32_t mtu)
{
        struct axgbe_if_softc	*sc = iflib_get_softc(ctx);
	struct xgbe_prv_data	*pdata = &sc->pdata;
	int ret;

        if (mtu > XGMAC_JUMBO_PACKET_MTU)
                return (EINVAL);

	ret = xgbe_calc_rx_buf_size(pdata->netdev, mtu);
        pdata->rx_buf_size = ret;
        axgbe_printf(1, "%s: rx_buf_size %d\n", __func__, ret);

        sc->scctx->isc_max_frame_size = mtu + ETHER_HDR_LEN + ETHER_CRC_LEN;
        return (0);
}

static void
axgbe_if_media_status(if_ctx_t ctx, struct ifmediareq * ifmr)
{
        struct axgbe_if_softc *sc = iflib_get_softc(ctx);
        struct xgbe_prv_data *pdata = &sc->pdata;

        ifmr->ifm_status = IFM_AVALID;
	if (!sc->pdata.phy.link)
		return;

	ifmr->ifm_active = IFM_ETHER;
	ifmr->ifm_status |= IFM_ACTIVE;

	axgbe_printf(1, "Speed 0x%x Mode %d\n", sc->pdata.phy.speed,
	    pdata->phy_if.phy_impl.cur_mode(pdata));
	pdata->phy_if.phy_impl.get_type(pdata, ifmr);

	ifmr->ifm_active |= IFM_FDX;
	ifmr->ifm_active |= IFM_ETH_TXPAUSE;
	ifmr->ifm_active |= IFM_ETH_RXPAUSE;
}
