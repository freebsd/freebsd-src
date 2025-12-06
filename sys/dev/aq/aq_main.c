/*
 * aQuantia Corporation Network Driver
 * Copyright (C) 2019 aQuantia Corporation. All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   (1) Redistributions of source code must retain the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer.
 *
 *   (2) Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer in the documentation and/or other materials provided
 *   with the distribution.
 *
 *   (3)The name of the author may not be used to endorse or promote
 *   products derived from this software without specific prior
 *   written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/endian.h>
#include <sys/sockio.h>
#include <sys/priv.h>
#include <sys/sysctl.h>
#include <sys/sbuf.h>
#include <sys/bitstring.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <net/if.h>
#include <net/if_media.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/ethernet.h>
#include <net/iflib.h>
#include <net/rss_config.h>

#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_rss.h"

#include "ifdi_if.h"

#include "aq_device.h"
#include "aq_fw.h"
#include "aq_hw.h"
#include "aq_hw_llh.h"
#include "aq_ring.h"
#include "aq_dbg.h"


#define	AQ_XXX_UNIMPLEMENTED_FUNCTION	do {				\
	printf("atlantic: unimplemented function: %s@%s:%d\n", __func__, 	\
	    __FILE__, __LINE__);					\
} while (0)

MALLOC_DEFINE(M_AQ, "aq", "Aquantia");

char aq_driver_version[] = AQ_VER;

#define AQUANTIA_VENDOR_ID 0x1D6A

#define AQ_DEVICE_ID_0001	0x0001
#define AQ_DEVICE_ID_D100	0xD100
#define AQ_DEVICE_ID_D107	0xD107
#define AQ_DEVICE_ID_D108	0xD108
#define AQ_DEVICE_ID_D109	0xD109

#define AQ_DEVICE_ID_AQC100	0x00B1
#define AQ_DEVICE_ID_AQC107	0x07B1
#define AQ_DEVICE_ID_AQC108	0x08B1
#define AQ_DEVICE_ID_AQC109	0x09B1
#define AQ_DEVICE_ID_AQC111	0x11B1
#define AQ_DEVICE_ID_AQC112	0x12B1

#define AQ_DEVICE_ID_AQC100S	0x80B1
#define AQ_DEVICE_ID_AQC107S	0x87B1
#define AQ_DEVICE_ID_AQC108S	0x88B1
#define AQ_DEVICE_ID_AQC109S	0x89B1
#define AQ_DEVICE_ID_AQC111S	0x91B1
#define AQ_DEVICE_ID_AQC112S	0x92B1

static pci_vendor_info_t aq_vendor_info_array[] = {
	PVID(AQUANTIA_VENDOR_ID, AQ_DEVICE_ID_0001, "Aquantia AQtion 10Gbit Network Adapter"),
	PVID(AQUANTIA_VENDOR_ID, AQ_DEVICE_ID_D107, "Aquantia AQtion 10Gbit Network Adapter"),
	PVID(AQUANTIA_VENDOR_ID, AQ_DEVICE_ID_D108, "Aquantia AQtion 5Gbit Network Adapter"),
	PVID(AQUANTIA_VENDOR_ID, AQ_DEVICE_ID_D109, "Aquantia AQtion 2.5Gbit Network Adapter"),

	PVID(AQUANTIA_VENDOR_ID, AQ_DEVICE_ID_AQC107, "Aquantia AQtion 10Gbit Network Adapter"),
	PVID(AQUANTIA_VENDOR_ID, AQ_DEVICE_ID_AQC108, "Aquantia AQtion 5Gbit Network Adapter"),
	PVID(AQUANTIA_VENDOR_ID, AQ_DEVICE_ID_AQC109, "Aquantia AQtion 2.5Gbit Network Adapter"),
	PVID(AQUANTIA_VENDOR_ID, AQ_DEVICE_ID_AQC100, "Aquantia AQtion 10Gbit Network Adapter"),

	PVID(AQUANTIA_VENDOR_ID, AQ_DEVICE_ID_AQC107S, "Aquantia AQtion 10Gbit Network Adapter"),
	PVID(AQUANTIA_VENDOR_ID, AQ_DEVICE_ID_AQC108S, "Aquantia AQtion 5Gbit Network Adapter"),
	PVID(AQUANTIA_VENDOR_ID, AQ_DEVICE_ID_AQC109S, "Aquantia AQtion 2.5Gbit Network Adapter"),
	PVID(AQUANTIA_VENDOR_ID, AQ_DEVICE_ID_AQC100S, "Aquantia AQtion 10Gbit Network Adapter"),

	PVID(AQUANTIA_VENDOR_ID, AQ_DEVICE_ID_AQC111, "Aquantia AQtion 5Gbit Network Adapter"),
	PVID(AQUANTIA_VENDOR_ID, AQ_DEVICE_ID_AQC112, "Aquantia AQtion 2.5Gbit Network Adapter"),
	PVID(AQUANTIA_VENDOR_ID, AQ_DEVICE_ID_AQC111S, "Aquantia AQtion 5Gbit Network Adapter"),
	PVID(AQUANTIA_VENDOR_ID, AQ_DEVICE_ID_AQC112S, "Aquantia AQtion 2.5Gbit Network Adapter"),

	PVID_END
};


/* Device setup, teardown, etc */
static void *aq_register(device_t dev);
static int aq_if_attach_pre(if_ctx_t ctx);
static int aq_if_attach_post(if_ctx_t ctx);
static int aq_if_detach(if_ctx_t ctx);
static int aq_if_shutdown(if_ctx_t ctx);
static int aq_if_suspend(if_ctx_t ctx);
static int aq_if_resume(if_ctx_t ctx);

/* Soft queue setup and teardown */
static int aq_if_tx_queues_alloc(if_ctx_t ctx, caddr_t *vaddrs,
		    uint64_t *paddrs, int ntxqs, int ntxqsets);
static int aq_if_rx_queues_alloc(if_ctx_t ctx, caddr_t *vaddrs,
		    uint64_t *paddrs, int nrxqs, int nrxqsets);
static void aq_if_queues_free(if_ctx_t ctx);

/* Device configuration */
static void aq_if_init(if_ctx_t ctx);
static void aq_if_stop(if_ctx_t ctx);
static void aq_if_multi_set(if_ctx_t ctx);
static int aq_if_mtu_set(if_ctx_t ctx, uint32_t mtu);
static void aq_if_media_status(if_ctx_t ctx, struct ifmediareq *ifmr);
static int aq_if_media_change(if_ctx_t ctx);
static int aq_if_promisc_set(if_ctx_t ctx, int flags);
static uint64_t aq_if_get_counter(if_ctx_t ctx, ift_counter cnt);
static void aq_if_timer(if_ctx_t ctx, uint16_t qid);
static int aq_hw_capabilities(struct aq_dev *softc);
static void aq_add_stats_sysctls(struct aq_dev *softc);

/* Interrupt enable / disable */
static void	aq_if_enable_intr(if_ctx_t ctx);
static void	aq_if_disable_intr(if_ctx_t ctx);
static int	aq_if_rx_queue_intr_enable(if_ctx_t ctx, uint16_t rxqid);
static int	aq_if_msix_intr_assign(if_ctx_t ctx, int msix);

/* VLAN support */
static bool aq_is_vlan_promisc_required(struct aq_dev *softc);
static void aq_update_vlan_filters(struct aq_dev *softc);
static void aq_if_vlan_register(if_ctx_t ctx, uint16_t vtag);
static void aq_if_vlan_unregister(if_ctx_t ctx, uint16_t vtag);

/* Informational/diagnostic */
static void	aq_if_led_func(if_ctx_t ctx, int onoff);

static device_method_t aq_methods[] = {
	DEVMETHOD(device_register, aq_register),
	DEVMETHOD(device_probe, iflib_device_probe),
	DEVMETHOD(device_attach, iflib_device_attach),
	DEVMETHOD(device_detach, iflib_device_detach),
	DEVMETHOD(device_shutdown, iflib_device_shutdown),
	DEVMETHOD(device_suspend, iflib_device_suspend),
	DEVMETHOD(device_resume, iflib_device_resume),

	DEVMETHOD_END
};

static driver_t aq_driver = {
	"aq", aq_methods, sizeof(struct aq_dev),
};

#if __FreeBSD_version >= 1400058
DRIVER_MODULE(atlantic, pci, aq_driver, 0, 0);
#else
static devclass_t aq_devclass;
DRIVER_MODULE(atlantic, pci, aq_driver, aq_devclass, 0, 0);
#endif

MODULE_DEPEND(atlantic, pci, 1, 1, 1);
MODULE_DEPEND(atlantic, ether, 1, 1, 1);
MODULE_DEPEND(atlantic, iflib, 1, 1, 1);

IFLIB_PNP_INFO(pci, atlantic, aq_vendor_info_array);

static device_method_t aq_if_methods[] = {
	/* Device setup, teardown, etc */
	DEVMETHOD(ifdi_attach_pre, aq_if_attach_pre),
	DEVMETHOD(ifdi_attach_post, aq_if_attach_post),
	DEVMETHOD(ifdi_detach, aq_if_detach),

	DEVMETHOD(ifdi_shutdown, aq_if_shutdown),
	DEVMETHOD(ifdi_suspend, aq_if_suspend),
	DEVMETHOD(ifdi_resume, aq_if_resume),

	/* Soft queue setup and teardown */
	DEVMETHOD(ifdi_tx_queues_alloc, aq_if_tx_queues_alloc),
	DEVMETHOD(ifdi_rx_queues_alloc, aq_if_rx_queues_alloc),
	DEVMETHOD(ifdi_queues_free, aq_if_queues_free),

	/* Device configuration */
	DEVMETHOD(ifdi_init, aq_if_init),
	DEVMETHOD(ifdi_stop, aq_if_stop),
	DEVMETHOD(ifdi_multi_set, aq_if_multi_set),
	DEVMETHOD(ifdi_mtu_set, aq_if_mtu_set),
	DEVMETHOD(ifdi_media_status, aq_if_media_status),
	DEVMETHOD(ifdi_media_change, aq_if_media_change),
	DEVMETHOD(ifdi_promisc_set, aq_if_promisc_set),
	DEVMETHOD(ifdi_get_counter, aq_if_get_counter),
	DEVMETHOD(ifdi_update_admin_status, aq_if_update_admin_status),
	DEVMETHOD(ifdi_timer, aq_if_timer),

	/* Interrupt enable / disable */
	DEVMETHOD(ifdi_intr_enable, aq_if_enable_intr),
	DEVMETHOD(ifdi_intr_disable, aq_if_disable_intr),
	DEVMETHOD(ifdi_rx_queue_intr_enable, aq_if_rx_queue_intr_enable),
	DEVMETHOD(ifdi_tx_queue_intr_enable, aq_if_rx_queue_intr_enable),
	DEVMETHOD(ifdi_msix_intr_assign, aq_if_msix_intr_assign),

	/* VLAN support */
	DEVMETHOD(ifdi_vlan_register, aq_if_vlan_register),
	DEVMETHOD(ifdi_vlan_unregister, aq_if_vlan_unregister),

	/* Informational/diagnostic */
	DEVMETHOD(ifdi_led_func, aq_if_led_func),

	DEVMETHOD_END
};

static driver_t aq_if_driver = {
	"aq_if", aq_if_methods, sizeof(struct aq_dev)
};

static struct if_shared_ctx aq_sctx_init = {
	.isc_magic = IFLIB_MAGIC,
	.isc_q_align = PAGE_SIZE,
	.isc_tx_maxsize = HW_ATL_B0_TSO_SIZE,
	.isc_tx_maxsegsize = HW_ATL_B0_MTU_JUMBO,
#if __FreeBSD__ >= 12
	.isc_tso_maxsize = HW_ATL_B0_TSO_SIZE,
	.isc_tso_maxsegsize = HW_ATL_B0_MTU_JUMBO,
#endif
	.isc_rx_maxsize = HW_ATL_B0_MTU_JUMBO,
	.isc_rx_nsegments = 16,
	.isc_rx_maxsegsize = PAGE_SIZE,
	.isc_nfl = 1,
	.isc_nrxqs = 1,
	.isc_ntxqs = 1,
	.isc_admin_intrcnt = 1,
	.isc_vendor_info = aq_vendor_info_array,
	.isc_driver_version = aq_driver_version,
	.isc_driver = &aq_if_driver,
	.isc_flags = IFLIB_NEED_SCRATCH | IFLIB_TSO_INIT_IP |
	    IFLIB_NEED_ZERO_CSUM,

	.isc_nrxd_min = {HW_ATL_B0_MIN_RXD},
	.isc_ntxd_min = {HW_ATL_B0_MIN_TXD},
	.isc_nrxd_max = {HW_ATL_B0_MAX_RXD},
	.isc_ntxd_max = {HW_ATL_B0_MAX_TXD},
	.isc_nrxd_default = {PAGE_SIZE / sizeof(aq_txc_desc_t) * 4},
	.isc_ntxd_default = {PAGE_SIZE / sizeof(aq_txc_desc_t) * 4},
};

/*
 * TUNEABLE PARAMETERS:
 */

static SYSCTL_NODE(_hw, OID_AUTO, aq, CTLFLAG_RD, 0, "Atlantic driver parameters");
/* UDP Receive-Side Scaling */
static int aq_enable_rss_udp = 1;
SYSCTL_INT(_hw_aq, OID_AUTO, enable_rss_udp, CTLFLAG_RDTUN, &aq_enable_rss_udp, 0,
    "Enable Receive-Side Scaling (RSS) for UDP");


/*
 * Device Methods
 */
static void *aq_register(device_t dev)
{
	return (&aq_sctx_init);
}

static int aq_if_attach_pre(if_ctx_t ctx)
{
	struct aq_dev *softc;
	struct aq_hw *hw;
	if_softc_ctx_t scctx;
	int rc;

	AQ_DBG_ENTER();
	softc = iflib_get_softc(ctx);
	rc = 0;

	softc->ctx = ctx;
	softc->dev = iflib_get_dev(ctx);
	softc->media = iflib_get_media(ctx);
	softc->scctx = iflib_get_softc_ctx(ctx);
	softc->sctx = iflib_get_sctx(ctx);
	scctx = softc->scctx;

	softc->mmio_rid = PCIR_BAR(0);
	softc->mmio_res = bus_alloc_resource_any(softc->dev, SYS_RES_MEMORY,
	    &softc->mmio_rid, RF_ACTIVE|RF_SHAREABLE);
	if (softc->mmio_res == NULL) {
		device_printf(softc->dev,
		    "failed to allocate MMIO resources\n");
		rc = ENXIO;
		goto fail;
	}

	softc->mmio_tag = rman_get_bustag(softc->mmio_res);
	softc->mmio_handle = rman_get_bushandle(softc->mmio_res);
	softc->mmio_size = rman_get_size(softc->mmio_res);
	softc->hw.hw_addr = (u8*) softc->mmio_handle;
	hw = &softc->hw;
	hw->link_rate = aq_fw_speed_auto;
	hw->itr = -1;
	hw->fc.fc_rx = 1;
	hw->fc.fc_tx = 1;
	softc->linkup = 0U;

	/* Look up ops and caps. */
	rc = aq_hw_mpi_create(hw);
	if (rc < 0) {
		AQ_DBG_ERROR(" %s: aq_hw_mpi_create fail err=%d", __func__, rc);
		goto fail;
	}

	if (hw->fast_start_enabled) {
		if (hw->fw_ops && hw->fw_ops->reset)
			hw->fw_ops->reset(hw);
	} else
		aq_hw_reset(&softc->hw);
	aq_hw_capabilities(softc);

	if (aq_hw_get_mac_permanent(hw, hw->mac_addr) < 0) {
		AQ_DBG_ERROR("Unable to get mac addr from hw");
		goto fail;
	};

	softc->admin_ticks = 0;

	iflib_set_mac(ctx, hw->mac_addr);
#if __FreeBSD__ < 13
	/* since FreeBSD13 deadlock due to calling iflib_led_func() under CTX_LOCK() */
	iflib_led_create(ctx);
#endif
	scctx->isc_tx_csum_flags = CSUM_IP | CSUM_TCP | CSUM_UDP | CSUM_TSO;
#if __FreeBSD__ >= 12
	scctx->isc_capabilities = IFCAP_RXCSUM | IFCAP_TXCSUM | IFCAP_HWCSUM | IFCAP_TSO |
							  IFCAP_JUMBO_MTU | IFCAP_VLAN_HWFILTER |
							  IFCAP_VLAN_MTU | IFCAP_VLAN_HWTAGGING |
							  IFCAP_VLAN_HWCSUM;
	scctx->isc_capenable = scctx->isc_capabilities;
#else
	if_t ifp;
	ifp = iflib_get_ifp(ctx);
	if_setcapenable(ifp,  IFCAP_RXCSUM | IFCAP_TXCSUM | IFCAP_HWCSUM | IFCAP_TSO |
							  IFCAP_JUMBO_MTU | IFCAP_VLAN_HWFILTER |
							  IFCAP_VLAN_MTU | IFCAP_VLAN_HWTAGGING |
							  IFCAP_VLAN_HWCSUM;
#endif
	scctx->isc_tx_nsegments = 31,
	scctx->isc_tx_tso_segments_max = 31;
	scctx->isc_tx_tso_size_max = HW_ATL_B0_TSO_SIZE - sizeof(struct ether_vlan_header);
	scctx->isc_tx_tso_segsize_max = HW_ATL_B0_MTU_JUMBO;
	scctx->isc_min_frame_size = 52;
	scctx->isc_txrx = &aq_txrx;

	scctx->isc_txqsizes[0] = sizeof(aq_tx_desc_t) * scctx->isc_ntxd[0];
	scctx->isc_rxqsizes[0] = sizeof(aq_rx_desc_t) * scctx->isc_nrxd[0];

	scctx->isc_ntxqsets_max = HW_ATL_B0_RINGS_MAX;
	scctx->isc_nrxqsets_max = HW_ATL_B0_RINGS_MAX;

	/* iflib will map and release this bar */
	scctx->isc_msix_bar = pci_msix_table_bar(softc->dev);

	softc->vlan_tags  = bit_alloc(4096, M_AQ, M_NOWAIT);

	AQ_DBG_EXIT(rc);
	return (rc);

fail:
	if (softc->mmio_res != NULL)
		bus_release_resource(softc->dev, SYS_RES_MEMORY,
		    softc->mmio_rid, softc->mmio_res);

	AQ_DBG_EXIT(rc);
	return (ENXIO);
}


static int aq_if_attach_post(if_ctx_t ctx)
{
	struct aq_dev *softc;
	int rc;

	AQ_DBG_ENTER();

	softc = iflib_get_softc(ctx);
	rc = 0;

	aq_update_hw_stats(softc);

	aq_initmedia(softc);


	switch (softc->scctx->isc_intr) {
	case IFLIB_INTR_LEGACY:
		rc = EOPNOTSUPP;
		goto exit;
        goto exit;
		break;
	case IFLIB_INTR_MSI:
		break;
	case IFLIB_INTR_MSIX:
		break;
	default:
		device_printf(softc->dev, "unknown interrupt mode\n");
		rc = EOPNOTSUPP;
		goto exit;
	}

	aq_add_stats_sysctls(softc);
	/* RSS */
	arc4rand(softc->rss_key, HW_ATL_RSS_HASHKEY_SIZE, 0);
	for (int i = ARRAY_SIZE(softc->rss_table); i--;){
		softc->rss_table[i] = i & (softc->rx_rings_count - 1);
	}
exit:
	AQ_DBG_EXIT(rc);
	return (rc);
}


static int aq_if_detach(if_ctx_t ctx)
{
	struct aq_dev *softc;
	int i;

	AQ_DBG_ENTER();
	softc = iflib_get_softc(ctx);

	aq_hw_deinit(&softc->hw);

	for (i = 0; i < softc->scctx->isc_nrxqsets; i++)
		iflib_irq_free(ctx, &softc->rx_rings[i]->irq);
	iflib_irq_free(ctx, &softc->irq);


	if (softc->mmio_res != NULL)
		bus_release_resource(softc->dev, SYS_RES_MEMORY,
		    softc->mmio_rid, softc->mmio_res);

	free(softc->vlan_tags, M_AQ);

	AQ_DBG_EXIT(0);
	return (0);
}

static int aq_if_shutdown(if_ctx_t ctx)
{

	AQ_DBG_ENTER();

	AQ_XXX_UNIMPLEMENTED_FUNCTION;

	AQ_DBG_EXIT(0);
	return (0);
}

static int aq_if_suspend(if_ctx_t ctx)
{
	AQ_DBG_ENTER();

	AQ_XXX_UNIMPLEMENTED_FUNCTION;

	AQ_DBG_EXIT(0);
	return (0);
}

static int aq_if_resume(if_ctx_t ctx)
{
	AQ_DBG_ENTER();

	AQ_XXX_UNIMPLEMENTED_FUNCTION;

	AQ_DBG_EXIT(0);
	return (0);
}

/* Soft queue setup and teardown */
static int aq_if_tx_queues_alloc(if_ctx_t ctx, caddr_t *vaddrs,
    uint64_t *paddrs, int ntxqs, int ntxqsets)
{
	struct aq_dev *softc;
	struct aq_ring *ring;
	int rc = 0, i;

	AQ_DBG_ENTERA("ntxqs=%d, ntxqsets=%d", ntxqs, ntxqsets);
	softc = iflib_get_softc(ctx);
	AQ_DBG_PRINT("tx descriptors  number %d", softc->scctx->isc_ntxd[0]);

	for (i = 0; i < ntxqsets; i++) {
		ring = softc->tx_rings[i] = malloc(sizeof(struct aq_ring),
						   M_AQ, M_NOWAIT | M_ZERO);
		if (!ring){
			rc = ENOMEM;
			device_printf(softc->dev, "atlantic: tx_ring malloc fail\n");
			goto fail;
		}
		ring->tx_descs = (aq_tx_desc_t*)vaddrs[i];
		ring->tx_size = softc->scctx->isc_ntxd[0];
		ring->tx_descs_phys = paddrs[i];
		ring->tx_head = ring->tx_tail = 0;
		ring->index = i;
		ring->dev = softc;

		softc->tx_rings_count++;
	}

	AQ_DBG_EXIT(rc);
	return (rc);

fail:
	aq_if_queues_free(ctx);
	AQ_DBG_EXIT(rc);
	return (rc);
}

static int aq_if_rx_queues_alloc(if_ctx_t ctx, caddr_t *vaddrs,
    uint64_t *paddrs, int nrxqs, int nrxqsets)
{
	struct aq_dev *softc;
	struct aq_ring *ring;
	int rc = 0, i;

	AQ_DBG_ENTERA("nrxqs=%d, nrxqsets=%d", nrxqs, nrxqsets);
	softc = iflib_get_softc(ctx);

	for (i = 0; i < nrxqsets; i++) {
		ring = softc->rx_rings[i] = malloc(sizeof(struct aq_ring),
						   M_AQ, M_NOWAIT | M_ZERO);
		if (!ring){
			rc = ENOMEM;
			device_printf(softc->dev, "atlantic: rx_ring malloc fail\n");
			goto fail;
		}

		ring->rx_descs = (aq_rx_desc_t*)vaddrs[i];
		ring->rx_descs_phys = paddrs[i];
		ring->rx_size = softc->scctx->isc_nrxd[0];
		ring->index = i;
		ring->dev = softc;

		switch (MCLBYTES) {
			case    (4 * 1024):
			case    (8 * 1024):
			case    (16 * 1024):
				ring->rx_max_frame_size = MCLBYTES;
				break;
			default:
				ring->rx_max_frame_size = 2048;
				break;
		}

		softc->rx_rings_count++;
	}

	AQ_DBG_EXIT(rc);
	return (rc);

fail:
	aq_if_queues_free(ctx);
	AQ_DBG_EXIT(rc);
	return (rc);
}

static void aq_if_queues_free(if_ctx_t ctx)
{
	struct aq_dev *softc;
	int i;

	AQ_DBG_ENTER();
	softc = iflib_get_softc(ctx);

	for (i = 0; i < softc->tx_rings_count; i++) {
		if (softc->tx_rings[i]) {
			free(softc->tx_rings[i], M_AQ);
			softc->tx_rings[i] = NULL;
		}
	}
	softc->tx_rings_count = 0;
	for (i = 0; i < softc->rx_rings_count; i++) {
		if (softc->rx_rings[i]){
			free(softc->rx_rings[i], M_AQ);
			softc->rx_rings[i] = NULL;
		}
	}
	softc->rx_rings_count = 0;

	AQ_DBG_EXIT(0);
	return;
}

/* Device configuration */
static void aq_if_init(if_ctx_t ctx)
{
	struct aq_dev *softc;
	struct aq_hw *hw;
	struct ifmediareq ifmr;
	int i, err;

	AQ_DBG_ENTER();
	softc = iflib_get_softc(ctx);
	hw = &softc->hw;

	err = aq_hw_init(&softc->hw, softc->hw.mac_addr, softc->msix,
					softc->scctx->isc_intr == IFLIB_INTR_MSIX);
	if (err != EOK) {
		device_printf(softc->dev, "atlantic: aq_hw_init: %d", err);
	}

	aq_if_media_status(ctx, &ifmr);

	aq_update_vlan_filters(softc);

	for (i = 0; i < softc->tx_rings_count; i++) {
		struct aq_ring *ring = softc->tx_rings[i];
		err = aq_ring_tx_init(&softc->hw, ring);
		if (err) {
			device_printf(softc->dev, "atlantic: aq_ring_tx_init: %d", err);
		}
		err = aq_ring_tx_start(hw, ring);
		if (err != EOK) {
			device_printf(softc->dev, "atlantic: aq_ring_tx_start: %d", err);
		}
	}
	for (i = 0; i < softc->rx_rings_count; i++) {
		struct aq_ring *ring = softc->rx_rings[i];
		err = aq_ring_rx_init(&softc->hw, ring);
		if (err) {
			device_printf(softc->dev, "atlantic: aq_ring_rx_init: %d", err);
		}
		err = aq_ring_rx_start(hw, ring);
		if (err != EOK) {
			device_printf(softc->dev, "atlantic: aq_ring_rx_start: %d", err);
		}
		aq_if_rx_queue_intr_enable(ctx, i);
	}

	aq_hw_start(hw);
	aq_if_enable_intr(ctx);
	aq_hw_rss_hash_set(&softc->hw, softc->rss_key);
	aq_hw_rss_set(&softc->hw, softc->rss_table);
	aq_hw_udp_rss_enable(hw, aq_enable_rss_udp);
	aq_hw_set_link_speed(hw, hw->link_rate);

	AQ_DBG_EXIT(0);
}


static void aq_if_stop(if_ctx_t ctx)
{
	struct aq_dev *softc;
	struct aq_hw *hw;
	int i;

	AQ_DBG_ENTER();

	softc = iflib_get_softc(ctx);
	hw = &softc->hw;

	/* disable interrupt */
	aq_if_disable_intr(ctx);

	for (i = 0; i < softc->tx_rings_count; i++) {
		aq_ring_tx_stop(hw, softc->tx_rings[i]);
		softc->tx_rings[i]->tx_head = 0;
		softc->tx_rings[i]->tx_tail = 0;
	}
	for (i = 0; i < softc->rx_rings_count; i++) {
		aq_ring_rx_stop(hw, softc->rx_rings[i]);
	}

	aq_hw_reset(&softc->hw);
	memset(&softc->last_stats, 0, sizeof(softc->last_stats));
	softc->linkup = false;
	aq_if_update_admin_status(ctx);
	AQ_DBG_EXIT(0);
}

static uint64_t aq_if_get_counter(if_ctx_t ctx, ift_counter cnt)
{
	struct aq_dev *softc = iflib_get_softc(ctx);
	if_t ifp = iflib_get_ifp(ctx);

	switch (cnt) {
	case IFCOUNTER_IERRORS:
		return (softc->curr_stats.erpr);
	case IFCOUNTER_IQDROPS:
		return (softc->curr_stats.dpc);
	case IFCOUNTER_OERRORS:
		return (softc->curr_stats.erpt);
	default:
		return (if_get_counter_default(ifp, cnt));
	}
}

#if __FreeBSD_version >= 1300054
static u_int aq_mc_filter_apply(void *arg, struct sockaddr_dl *dl, u_int count)
{
	struct aq_dev *softc = arg;
	struct aq_hw *hw = &softc->hw;
	u8 *mac_addr = NULL;

	if (count == AQ_HW_MAC_MAX)
		return (0);

	mac_addr = LLADDR(dl);
	aq_hw_mac_addr_set(hw, mac_addr, count + 1);

	aq_log_detail("set %d mc address %6D", count + 1, mac_addr, ":");
	return (1);
}
#else
static int aq_mc_filter_apply(void *arg, struct ifmultiaddr *ifma, int count)
{
	struct aq_dev *softc = arg;
	struct aq_hw *hw = &softc->hw;
	u8 *mac_addr = NULL;

	if (ifma->ifma_addr->sa_family != AF_LINK)
		return (0);
	if (count == AQ_HW_MAC_MAX)
		return (0);

	mac_addr = LLADDR((struct sockaddr_dl *)ifma->ifma_addr);
	aq_hw_mac_addr_set(hw, mac_addr, count + 1);

	aq_log_detail("set %d mc address %6D", count + 1, mac_addr, ":");
	return (1);
}
#endif

static bool aq_is_mc_promisc_required(struct aq_dev *softc)
{
	return (softc->mcnt >= AQ_HW_MAC_MAX);
}

static void aq_if_multi_set(if_ctx_t ctx)
{
	struct aq_dev *softc = iflib_get_softc(ctx);
	if_t ifp = iflib_get_ifp(ctx);
	struct aq_hw  *hw = &softc->hw;
	AQ_DBG_ENTER();
#if __FreeBSD_version >= 1300054
	softc->mcnt = if_llmaddr_count(iflib_get_ifp(ctx));
#else
	softc->mcnt = if_multiaddr_count(iflib_get_ifp(ctx), AQ_HW_MAC_MAX);
#endif
	if (softc->mcnt >= AQ_HW_MAC_MAX)
	{
		aq_hw_set_promisc(hw, !!(if_getflags(ifp) & IFF_PROMISC),
				  aq_is_vlan_promisc_required(softc),
				  !!(if_getflags(ifp) & IFF_ALLMULTI) || aq_is_mc_promisc_required(softc));
	}else{
#if __FreeBSD_version >= 1300054
		if_foreach_llmaddr(iflib_get_ifp(ctx), &aq_mc_filter_apply, softc);
#else
		if_multi_apply(iflib_get_ifp(ctx), aq_mc_filter_apply, softc);
#endif
	}
	AQ_DBG_EXIT(0);
}

static int aq_if_mtu_set(if_ctx_t ctx, uint32_t mtu)
{
	int err = 0;
	AQ_DBG_ENTER();

	AQ_DBG_EXIT(err);
	return (err);
}

static void aq_if_media_status(if_ctx_t ctx, struct ifmediareq *ifmr)
{
	if_t ifp;

	AQ_DBG_ENTER();

	ifp = iflib_get_ifp(ctx);

	aq_mediastatus(ifp, ifmr);

	AQ_DBG_EXIT(0);
}

static int aq_if_media_change(if_ctx_t ctx)
{
	struct aq_dev *softc = iflib_get_softc(ctx);
	if_t ifp = iflib_get_ifp(ctx);
	int rc = 0;

	AQ_DBG_ENTER();

	/* Not allowd in UP state, since causes unsync of rings */
	if ((if_getflags(ifp) & IFF_UP)){
		rc = EPERM;
		goto exit;
	}

	ifp = iflib_get_ifp(softc->ctx);

	rc = aq_mediachange(ifp);

exit:
	AQ_DBG_EXIT(rc);
	return (rc);
}

static int aq_if_promisc_set(if_ctx_t ctx, int flags)
{
	struct aq_dev *softc;

	AQ_DBG_ENTER();

	softc = iflib_get_softc(ctx);

	aq_hw_set_promisc(&softc->hw, !!(flags & IFF_PROMISC),
			  aq_is_vlan_promisc_required(softc),
			  !!(flags & IFF_ALLMULTI) || aq_is_mc_promisc_required(softc));

	AQ_DBG_EXIT(0);
	return (0);
}

static void aq_if_timer(if_ctx_t ctx, uint16_t qid)
{
	struct aq_dev *softc;
	uint64_t ticks_now;

//	AQ_DBG_ENTER();

	softc = iflib_get_softc(ctx);
	ticks_now = ticks;

	/* Schedule aqc_if_update_admin_status() once per sec */
	if (ticks_now - softc->admin_ticks >= hz) {
		softc->admin_ticks = ticks_now;
		iflib_admin_intr_deferred(ctx);
	}

//	AQ_DBG_EXIT(0);
	return;

}

/* Interrupt enable / disable */
static void aq_if_enable_intr(if_ctx_t ctx)
{
	struct aq_dev *softc = iflib_get_softc(ctx);
	struct aq_hw  *hw = &softc->hw;

	AQ_DBG_ENTER();

	/* Enable interrupts */
	itr_irq_msk_setlsw_set(hw, BIT(softc->msix + 1) - 1);

	AQ_DBG_EXIT(0);
}

static void aq_if_disable_intr(if_ctx_t ctx)
{
	struct aq_dev *softc = iflib_get_softc(ctx);
	struct aq_hw  *hw = &softc->hw;

	AQ_DBG_ENTER();

	/* Disable interrupts */
	itr_irq_msk_clearlsw_set(hw, BIT(softc->msix + 1) - 1);

	AQ_DBG_EXIT(0);
}

static int aq_if_rx_queue_intr_enable(if_ctx_t ctx, uint16_t rxqid)
{
	struct aq_dev *softc = iflib_get_softc(ctx);
	struct aq_hw  *hw = &softc->hw;

	AQ_DBG_ENTER();

	itr_irq_msk_setlsw_set(hw, BIT(softc->rx_rings[rxqid]->msix));

	AQ_DBG_EXIT(0);
	return (0);
}

static int aq_if_msix_intr_assign(if_ctx_t ctx, int msix)
{
	struct aq_dev *softc;
	int i, vector = 0, rc;
	char irq_name[16];
	int rx_vectors;

	AQ_DBG_ENTER();
	softc = iflib_get_softc(ctx);

	for (i = 0; i < softc->rx_rings_count; i++, vector++) {
		snprintf(irq_name, sizeof(irq_name), "rxq%d", i);
		rc = iflib_irq_alloc_generic(ctx, &softc->rx_rings[i]->irq,
		    vector + 1, IFLIB_INTR_RX, aq_isr_rx, softc->rx_rings[i],
			softc->rx_rings[i]->index, irq_name);
		device_printf(softc->dev, "Assign IRQ %u to rx ring %u\n",
					  vector, softc->rx_rings[i]->index);

		if (rc) {
			device_printf(softc->dev, "failed to set up RX handler\n");
			i--;
			goto fail;
		}

		softc->rx_rings[i]->msix = vector;
	}

	rx_vectors = vector;

	for (i = 0; i < softc->tx_rings_count; i++, vector++) {
		snprintf(irq_name, sizeof(irq_name), "txq%d", i);
		iflib_softirq_alloc_generic(ctx, &softc->rx_rings[i]->irq, IFLIB_INTR_TX,
									softc->tx_rings[i], i, irq_name);

		softc->tx_rings[i]->msix = (vector % softc->rx_rings_count);
		device_printf(softc->dev, "Assign IRQ %u to tx ring %u\n",
					  softc->tx_rings[i]->msix, softc->tx_rings[i]->index);
	}

	rc = iflib_irq_alloc_generic(ctx, &softc->irq, rx_vectors + 1,
								 IFLIB_INTR_ADMIN, aq_linkstat_isr,
								 softc, 0, "aq");
	softc->msix = rx_vectors;
	device_printf(softc->dev, "Assign IRQ %u to admin proc \n",
				  rx_vectors);
	if (rc) {
		device_printf(iflib_get_dev(ctx), "Failed to register admin handler");
		i = softc->rx_rings_count;
		goto fail;
	}
	AQ_DBG_EXIT(0);
	return (0);

fail:
	for (; i >= 0; i--)
		iflib_irq_free(ctx, &softc->rx_rings[i]->irq);
	AQ_DBG_EXIT(rc);
	return (rc);
}

static bool aq_is_vlan_promisc_required(struct aq_dev *softc)
{
	int vlan_tag_count;

	bit_count(softc->vlan_tags, 0, 4096, &vlan_tag_count);

	if (vlan_tag_count <= AQ_HW_VLAN_MAX_FILTERS)
		return (false);
	else
		return (true);

}

static void aq_update_vlan_filters(struct aq_dev *softc)
{
	struct aq_rx_filter_vlan aq_vlans[AQ_HW_VLAN_MAX_FILTERS];
	struct aq_hw  *hw = &softc->hw;
	int bit_pos = 0;
	int vlan_tag = -1;
	int i;

	hw_atl_b0_hw_vlan_promisc_set(hw, true);
	for (i = 0; i < AQ_HW_VLAN_MAX_FILTERS; i++) {
		bit_ffs_at(softc->vlan_tags, bit_pos, 4096, &vlan_tag);
		if (vlan_tag != -1) {
			aq_vlans[i].enable = true;
			aq_vlans[i].location = i;
			aq_vlans[i].queue = 0xFF;
			aq_vlans[i].vlan_id = vlan_tag;
			bit_pos = vlan_tag;
		} else {
			aq_vlans[i].enable = false;
		}
	}

	hw_atl_b0_hw_vlan_set(hw, aq_vlans);
	hw_atl_b0_hw_vlan_promisc_set(hw, aq_is_vlan_promisc_required(softc));
}

/* VLAN support */
static void aq_if_vlan_register(if_ctx_t ctx, uint16_t vtag)
{
	struct aq_dev *softc = iflib_get_softc(ctx);

	AQ_DBG_ENTERA("%d", vtag);

	bit_set(softc->vlan_tags, vtag);

	aq_update_vlan_filters(softc);

	AQ_DBG_EXIT(0);
}

static void aq_if_vlan_unregister(if_ctx_t ctx, uint16_t vtag)
{
	struct aq_dev *softc = iflib_get_softc(ctx);

	AQ_DBG_ENTERA("%d", vtag);

	bit_clear(softc->vlan_tags, vtag);

	aq_update_vlan_filters(softc);

	AQ_DBG_EXIT(0);
}

static void aq_if_led_func(if_ctx_t ctx, int onoff)
{
	struct aq_dev *softc = iflib_get_softc(ctx);
	struct aq_hw  *hw = &softc->hw;

	AQ_DBG_ENTERA("%d", onoff);
	if (hw->fw_ops && hw->fw_ops->led_control)
		hw->fw_ops->led_control(hw, onoff);

	AQ_DBG_EXIT(0);
}

static int aq_hw_capabilities(struct aq_dev *softc)
{

	if (pci_get_vendor(softc->dev) != AQUANTIA_VENDOR_ID)
		return (ENXIO);

	switch (pci_get_device(softc->dev)) {
	case AQ_DEVICE_ID_D100:
	case AQ_DEVICE_ID_AQC100:
	case AQ_DEVICE_ID_AQC100S:
		softc->media_type = AQ_MEDIA_TYPE_FIBRE;
		softc->link_speeds = AQ_LINK_ALL & ~AQ_LINK_10G;
		break;

	case AQ_DEVICE_ID_0001:
	case AQ_DEVICE_ID_D107:
	case AQ_DEVICE_ID_AQC107:
	case AQ_DEVICE_ID_AQC107S:
		softc->media_type = AQ_MEDIA_TYPE_TP;
		softc->link_speeds = AQ_LINK_ALL;
		break;

	case AQ_DEVICE_ID_D108:
	case AQ_DEVICE_ID_AQC108:
	case AQ_DEVICE_ID_AQC108S:
	case AQ_DEVICE_ID_AQC111:
	case AQ_DEVICE_ID_AQC111S:
		softc->media_type = AQ_MEDIA_TYPE_TP;
		softc->link_speeds = AQ_LINK_ALL & ~AQ_LINK_10G;
		break;

	case AQ_DEVICE_ID_D109:
	case AQ_DEVICE_ID_AQC109:
	case AQ_DEVICE_ID_AQC109S:
	case AQ_DEVICE_ID_AQC112:
	case AQ_DEVICE_ID_AQC112S:
		softc->media_type = AQ_MEDIA_TYPE_TP;
		softc->link_speeds = AQ_LINK_ALL & ~(AQ_LINK_10G | AQ_LINK_5G);
		break;

	default:
		return (ENXIO);
	}

	return (0);
}

static int aq_sysctl_print_rss_config(SYSCTL_HANDLER_ARGS)
{
	struct aq_dev  *softc = (struct aq_dev *)arg1;
	device_t        dev = softc->dev;
	struct sbuf     *buf;
	int             error = 0;

	buf = sbuf_new_for_sysctl(NULL, NULL, 256, req);
	if (!buf) {
		device_printf(dev, "Could not allocate sbuf for output.\n");
		return (ENOMEM);
	}

	/* Print out the redirection table */
	sbuf_cat(buf, "\nRSS Indirection table:\n");
	for (int i = 0; i < HW_ATL_RSS_INDIRECTION_TABLE_MAX; i++) {
		sbuf_printf(buf, "%d ", softc->rss_table[i]);
		if ((i+1) % 10 == 0)
			sbuf_printf(buf, "\n");
	}

	sbuf_cat(buf, "\nRSS Key:\n");
	for (int i = 0; i < HW_ATL_RSS_HASHKEY_SIZE; i++) {
		sbuf_printf(buf, "0x%02x ", softc->rss_key[i]);
	}
	sbuf_printf(buf, "\n");

	error = sbuf_finish(buf);
	if (error)
		device_printf(dev, "Error finishing sbuf: %d\n", error);

	sbuf_delete(buf);

	return (0);
}

static int aq_sysctl_print_tx_head(SYSCTL_HANDLER_ARGS)
{
	struct aq_ring  *ring = arg1;
	int             error = 0;
	unsigned int   val;

	if (!ring)
		return (0);

	val = tdm_tx_desc_head_ptr_get(&ring->dev->hw, ring->index);

	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error || !req->newptr)
		return (error);

	return (0);
}

static int aq_sysctl_print_tx_tail(SYSCTL_HANDLER_ARGS)
{
	struct aq_ring  *ring = arg1;
	int             error = 0;
	unsigned int   val;

	if (!ring)
		return (0);

	val = reg_tx_dma_desc_tail_ptr_get(&ring->dev->hw, ring->index);

	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error || !req->newptr)
		return (error);

	return (0);
}

static int aq_sysctl_print_rx_head(SYSCTL_HANDLER_ARGS)
{
	struct aq_ring  *ring = arg1;
	int             error = 0;
	unsigned int   val;

	if (!ring)
		return (0);

	val = rdm_rx_desc_head_ptr_get(&ring->dev->hw, ring->index);

	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error || !req->newptr)
		return (error);

	return (0);
}

static int aq_sysctl_print_rx_tail(SYSCTL_HANDLER_ARGS)
{
	struct aq_ring  *ring = arg1;
	int             error = 0;
	unsigned int   val;

	if (!ring)
		return (0);

	val = reg_rx_dma_desc_tail_ptr_get(&ring->dev->hw, ring->index);

	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error || !req->newptr)
		return (error);

	return (0);
}

static void aq_add_stats_sysctls(struct aq_dev *softc)
{
    device_t                dev = softc->dev;
    struct sysctl_ctx_list  *ctx = device_get_sysctl_ctx(dev);
    struct sysctl_oid       *tree = device_get_sysctl_tree(dev);
    struct sysctl_oid_list  *child = SYSCTL_CHILDREN(tree);
    struct aq_stats_s *stats = &softc->curr_stats;
    struct sysctl_oid       *stat_node, *queue_node;
    struct sysctl_oid_list  *stat_list, *queue_list;

#define QUEUE_NAME_LEN 32
    char                    namebuf[QUEUE_NAME_LEN];
	/* RSS configuration */
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "print_rss_config",
		CTLTYPE_STRING | CTLFLAG_RD, softc, 0,
		aq_sysctl_print_rss_config, "A", "Prints RSS Configuration");

    /* Driver Statistics */
     for (int i = 0; i < softc->tx_rings_count; i++) {
        struct aq_ring *ring = softc->tx_rings[i];
        snprintf(namebuf, QUEUE_NAME_LEN, "tx_queue%d", i);
        queue_node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, namebuf,
            CTLFLAG_RD, NULL, "Queue Name");
        queue_list = SYSCTL_CHILDREN(queue_node);

        SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "tx_pkts",
            CTLFLAG_RD, &(ring->stats.tx_pkts), "TX Packets");
        SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "tx_bytes",
            CTLFLAG_RD, &(ring->stats.tx_bytes), "TX Octets");
        SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "tx_drops",
            CTLFLAG_RD, &(ring->stats.tx_drops), "TX Drops");
        SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "tx_queue_full",
            CTLFLAG_RD, &(ring->stats.tx_queue_full), "TX Queue Full");
	SYSCTL_ADD_PROC(ctx, queue_list, OID_AUTO, "tx_head",
		CTLTYPE_UINT | CTLFLAG_RD, ring, 0,
		aq_sysctl_print_tx_head, "IU", "ring head pointer");
	SYSCTL_ADD_PROC(ctx, queue_list, OID_AUTO, "tx_tail",
		CTLTYPE_UINT | CTLFLAG_RD, ring, 0,
		aq_sysctl_print_tx_tail, "IU", "ring tail pointer");
    }

     for (int i = 0; i < softc->rx_rings_count; i++) {
        struct aq_ring *ring = softc->rx_rings[i];
        snprintf(namebuf, QUEUE_NAME_LEN, "rx_queue%d", i);
        queue_node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, namebuf,
            CTLFLAG_RD, NULL, "Queue Name");
        queue_list = SYSCTL_CHILDREN(queue_node);

        SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "rx_pkts",
            CTLFLAG_RD, &(ring->stats.rx_pkts), "RX Packets");
        SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "rx_bytes",
            CTLFLAG_RD, &(ring->stats.rx_bytes), "TX Octets");
        SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "jumbo_pkts",
            CTLFLAG_RD, &(ring->stats.jumbo_pkts), "Jumbo Packets");
        SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "rx_err",
            CTLFLAG_RD, &(ring->stats.rx_err), "RX Errors");
        SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "irq",
            CTLFLAG_RD, &(ring->stats.irq), "RX interrupts");
	SYSCTL_ADD_PROC(ctx, queue_list, OID_AUTO, "rx_head",
		CTLTYPE_UINT | CTLFLAG_RD, ring, 0,
		aq_sysctl_print_rx_head, "IU", "ring head pointer");
	SYSCTL_ADD_PROC(ctx, queue_list, OID_AUTO, "rx_tail",
		CTLTYPE_UINT | CTLFLAG_RD, ring, 0,
		aq_sysctl_print_rx_tail, "IU", " ring tail pointer");
    }

    stat_node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, "mac",
        CTLFLAG_RD, NULL, "Statistics (read from HW registers)");
    stat_list = SYSCTL_CHILDREN(stat_node);

    SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "good_pkts_rcvd",
        CTLFLAG_RD, &stats->prc, "Good Packets Received");
    SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "ucast_pkts_rcvd",
        CTLFLAG_RD, &stats->uprc, "Unicast Packets Received");
    SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "mcast_pkts_rcvd",
        CTLFLAG_RD, &stats->mprc, "Multicast Packets Received");
    SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "bcast_pkts_rcvd",
        CTLFLAG_RD, &stats->bprc, "Broadcast Packets Received");
    SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "rsc_pkts_rcvd",
        CTLFLAG_RD, &stats->cprc, "Coalesced Packets Received");
    SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "err_pkts_rcvd",
        CTLFLAG_RD, &stats->erpr, "Errors of Packet Receive");
    SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "drop_pkts_dma",
        CTLFLAG_RD, &stats->dpc, "Dropped Packets in DMA");
    SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "good_octets_rcvd",
        CTLFLAG_RD, &stats->brc, "Good Octets Received");
    SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "ucast_octets_rcvd",
        CTLFLAG_RD, &stats->ubrc, "Unicast Octets Received");
    SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "mcast_octets_rcvd",
        CTLFLAG_RD, &stats->mbrc, "Multicast Octets Received");
    SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "bcast_octets_rcvd",
        CTLFLAG_RD, &stats->bbrc, "Broadcast Octets Received");

    SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "good_pkts_txd",
        CTLFLAG_RD, &stats->ptc, "Good Packets Transmitted");
    SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "ucast_pkts_txd",
        CTLFLAG_RD, &stats->uptc, "Unicast Packets Transmitted");
    SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "mcast_pkts_txd",
        CTLFLAG_RD, &stats->mptc, "Multicast Packets Transmitted");
    SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "bcast_pkts_txd",
        CTLFLAG_RD, &stats->bptc, "Broadcast Packets Transmitted");

    SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "err_pkts_txd",
        CTLFLAG_RD, &stats->erpt, "Errors of Packet Transmit");
    SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "good_octets_txd",
        CTLFLAG_RD, &stats->btc, "Good Octets Transmitted");
    SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "ucast_octets_txd",
        CTLFLAG_RD, &stats->ubtc, "Unicast Octets Transmitted");
    SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "mcast_octets_txd",
        CTLFLAG_RD, &stats->mbtc, "Multicast Octets Transmitted");
    SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "bcast_octets_txd",
        CTLFLAG_RD, &stats->bbtc, "Broadcast Octets Transmitted");
}
