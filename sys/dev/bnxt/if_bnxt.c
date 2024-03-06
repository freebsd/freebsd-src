/*-
 * Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2016 Broadcom, All Rights Reserved.
 * The term Broadcom refers to Broadcom Limited and/or its subsidiaries
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS'
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/endian.h>
#include <sys/sockio.h>
#include <sys/priv.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_var.h>
#include <net/ethernet.h>
#include <net/iflib.h>

#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_rss.h"

#include "ifdi_if.h"

#include "bnxt.h"
#include "bnxt_hwrm.h"
#include "bnxt_ioctl.h"
#include "bnxt_sysctl.h"
#include "hsi_struct_def.h"
#include "bnxt_mgmt.h"

/*
 * PCI Device ID Table
 */

static const pci_vendor_info_t bnxt_vendor_info_array[] =
{
    PVID(BROADCOM_VENDOR_ID, BCM57301,
	"Broadcom BCM57301 NetXtreme-C 10Gb Ethernet Controller"),
    PVID(BROADCOM_VENDOR_ID, BCM57302,
	"Broadcom BCM57302 NetXtreme-C 10Gb/25Gb Ethernet Controller"),
    PVID(BROADCOM_VENDOR_ID, BCM57304,
	"Broadcom BCM57304 NetXtreme-C 10Gb/25Gb/40Gb/50Gb Ethernet Controller"),
    PVID(BROADCOM_VENDOR_ID, BCM57311,
	"Broadcom BCM57311 NetXtreme-C 10Gb Ethernet"),
    PVID(BROADCOM_VENDOR_ID, BCM57312,
	"Broadcom BCM57312 NetXtreme-C 10Gb/25Gb Ethernet"),
    PVID(BROADCOM_VENDOR_ID, BCM57314,
	"Broadcom BCM57314 NetXtreme-C 10Gb/25Gb/40Gb/50Gb Ethernet"),
    PVID(BROADCOM_VENDOR_ID, BCM57402,
	"Broadcom BCM57402 NetXtreme-E 10Gb Ethernet Controller"),
    PVID(BROADCOM_VENDOR_ID, BCM57402_NPAR,
	"Broadcom BCM57402 NetXtreme-E Partition"),
    PVID(BROADCOM_VENDOR_ID, BCM57404,
	"Broadcom BCM57404 NetXtreme-E 10Gb/25Gb Ethernet Controller"),
    PVID(BROADCOM_VENDOR_ID, BCM57404_NPAR,
	"Broadcom BCM57404 NetXtreme-E Partition"),
    PVID(BROADCOM_VENDOR_ID, BCM57406,
	"Broadcom BCM57406 NetXtreme-E 10GBase-T Ethernet Controller"),
    PVID(BROADCOM_VENDOR_ID, BCM57406_NPAR,
	"Broadcom BCM57406 NetXtreme-E Partition"),
    PVID(BROADCOM_VENDOR_ID, BCM57407,
	"Broadcom BCM57407 NetXtreme-E 10GBase-T Ethernet Controller"),
    PVID(BROADCOM_VENDOR_ID, BCM57407_NPAR,
	"Broadcom BCM57407 NetXtreme-E Ethernet Partition"),
    PVID(BROADCOM_VENDOR_ID, BCM57407_SFP,
	"Broadcom BCM57407 NetXtreme-E 25Gb Ethernet Controller"),
    PVID(BROADCOM_VENDOR_ID, BCM57412,
	"Broadcom BCM57412 NetXtreme-E 10Gb Ethernet"),
    PVID(BROADCOM_VENDOR_ID, BCM57412_NPAR1,
	"Broadcom BCM57412 NetXtreme-E Ethernet Partition"),
    PVID(BROADCOM_VENDOR_ID, BCM57412_NPAR2,
	"Broadcom BCM57412 NetXtreme-E Ethernet Partition"),
    PVID(BROADCOM_VENDOR_ID, BCM57414,
	"Broadcom BCM57414 NetXtreme-E 10Gb/25Gb Ethernet"),
    PVID(BROADCOM_VENDOR_ID, BCM57414_NPAR1,
	"Broadcom BCM57414 NetXtreme-E Ethernet Partition"),
    PVID(BROADCOM_VENDOR_ID, BCM57414_NPAR2,
	"Broadcom BCM57414 NetXtreme-E Ethernet Partition"),
    PVID(BROADCOM_VENDOR_ID, BCM57416,
	"Broadcom BCM57416 NetXtreme-E 10GBase-T Ethernet"),
    PVID(BROADCOM_VENDOR_ID, BCM57416_NPAR1,
	"Broadcom BCM57416 NetXtreme-E Ethernet Partition"),
    PVID(BROADCOM_VENDOR_ID, BCM57416_NPAR2,
	"Broadcom BCM57416 NetXtreme-E Ethernet Partition"),
    PVID(BROADCOM_VENDOR_ID, BCM57416_SFP,
	"Broadcom BCM57416 NetXtreme-E 10Gb Ethernet"),
    PVID(BROADCOM_VENDOR_ID, BCM57417,
	"Broadcom BCM57417 NetXtreme-E 10GBase-T Ethernet"),
    PVID(BROADCOM_VENDOR_ID, BCM57417_NPAR1,
	"Broadcom BCM57417 NetXtreme-E Ethernet Partition"),
    PVID(BROADCOM_VENDOR_ID, BCM57417_NPAR2,
	"Broadcom BCM57417 NetXtreme-E Ethernet Partition"),
    PVID(BROADCOM_VENDOR_ID, BCM57417_SFP,
	"Broadcom BCM57417 NetXtreme-E 10Gb/25Gb Ethernet"),
    PVID(BROADCOM_VENDOR_ID, BCM57454,
	"Broadcom BCM57454 NetXtreme-E 10Gb/25Gb/40Gb/50Gb/100Gb Ethernet"),
    PVID(BROADCOM_VENDOR_ID, BCM58700,
	"Broadcom BCM58700 Nitro 1Gb/2.5Gb/10Gb Ethernet"),
    PVID(BROADCOM_VENDOR_ID, BCM57508,
	"Broadcom BCM57508 NetXtreme-E 10Gb/25Gb/50Gb/100Gb/200Gb Ethernet"),
    PVID(BROADCOM_VENDOR_ID, BCM57504,
	"Broadcom BCM57504 NetXtreme-E 10Gb/25Gb/50Gb/100Gb/200Gb Ethernet"),
    PVID(BROADCOM_VENDOR_ID, BCM57502,
	"Broadcom BCM57502 NetXtreme-E 10Gb/25Gb/50Gb/100Gb/200Gb Ethernet"),
    PVID(BROADCOM_VENDOR_ID, NETXTREME_C_VF1,
	"Broadcom NetXtreme-C Ethernet Virtual Function"),
    PVID(BROADCOM_VENDOR_ID, NETXTREME_C_VF2,
	"Broadcom NetXtreme-C Ethernet Virtual Function"),
    PVID(BROADCOM_VENDOR_ID, NETXTREME_C_VF3,
	"Broadcom NetXtreme-C Ethernet Virtual Function"),
    PVID(BROADCOM_VENDOR_ID, NETXTREME_E_VF1,
	"Broadcom NetXtreme-E Ethernet Virtual Function"),
    PVID(BROADCOM_VENDOR_ID, NETXTREME_E_VF2,
	"Broadcom NetXtreme-E Ethernet Virtual Function"),
    PVID(BROADCOM_VENDOR_ID, NETXTREME_E_VF3,
	"Broadcom NetXtreme-E Ethernet Virtual Function"),
    /* required last entry */

    PVID_END
};

/*
 * Function prototypes
 */

SLIST_HEAD(softc_list, bnxt_softc_list) pf_list;
int bnxt_num_pfs = 0;

static void *bnxt_register(device_t dev);

/* Soft queue setup and teardown */
static int bnxt_tx_queues_alloc(if_ctx_t ctx, caddr_t *vaddrs,
    uint64_t *paddrs, int ntxqs, int ntxqsets);
static int bnxt_rx_queues_alloc(if_ctx_t ctx, caddr_t *vaddrs,
    uint64_t *paddrs, int nrxqs, int nrxqsets);
static void bnxt_queues_free(if_ctx_t ctx);

/* Device setup and teardown */
static int bnxt_attach_pre(if_ctx_t ctx);
static int bnxt_attach_post(if_ctx_t ctx);
static int bnxt_detach(if_ctx_t ctx);

/* Device configuration */
static void bnxt_init(if_ctx_t ctx);
static void bnxt_stop(if_ctx_t ctx);
static void bnxt_multi_set(if_ctx_t ctx);
static int bnxt_mtu_set(if_ctx_t ctx, uint32_t mtu);
static void bnxt_media_status(if_ctx_t ctx, struct ifmediareq * ifmr);
static int bnxt_media_change(if_ctx_t ctx);
static int bnxt_promisc_set(if_ctx_t ctx, int flags);
static uint64_t	bnxt_get_counter(if_ctx_t, ift_counter);
static void bnxt_update_admin_status(if_ctx_t ctx);
static void bnxt_if_timer(if_ctx_t ctx, uint16_t qid);

/* Interrupt enable / disable */
static void bnxt_intr_enable(if_ctx_t ctx);
static int bnxt_rx_queue_intr_enable(if_ctx_t ctx, uint16_t qid);
static int bnxt_tx_queue_intr_enable(if_ctx_t ctx, uint16_t qid);
static void bnxt_disable_intr(if_ctx_t ctx);
static int bnxt_msix_intr_assign(if_ctx_t ctx, int msix);

/* vlan support */
static void bnxt_vlan_register(if_ctx_t ctx, uint16_t vtag);
static void bnxt_vlan_unregister(if_ctx_t ctx, uint16_t vtag);

/* ioctl */
static int bnxt_priv_ioctl(if_ctx_t ctx, u_long command, caddr_t data);

static int bnxt_shutdown(if_ctx_t ctx);
static int bnxt_suspend(if_ctx_t ctx);
static int bnxt_resume(if_ctx_t ctx);

/* Internal support functions */
static int bnxt_probe_phy(struct bnxt_softc *softc);
static void bnxt_add_media_types(struct bnxt_softc *softc);
static int bnxt_pci_mapping(struct bnxt_softc *softc);
static void bnxt_pci_mapping_free(struct bnxt_softc *softc);
static int bnxt_update_link(struct bnxt_softc *softc, bool chng_link_state);
static int bnxt_handle_def_cp(void *arg);
static int bnxt_handle_isr(void *arg);
static void bnxt_clear_ids(struct bnxt_softc *softc);
static void inline bnxt_do_enable_intr(struct bnxt_cp_ring *cpr);
static void inline bnxt_do_disable_intr(struct bnxt_cp_ring *cpr);
static void bnxt_mark_cpr_invalid(struct bnxt_cp_ring *cpr);
static void bnxt_def_cp_task(void *context);
static void bnxt_handle_async_event(struct bnxt_softc *softc,
    struct cmpl_base *cmpl);
static uint8_t get_phy_type(struct bnxt_softc *softc);
static uint64_t bnxt_get_baudrate(struct bnxt_link_info *link);
static void bnxt_get_wol_settings(struct bnxt_softc *softc);
static int bnxt_wol_config(if_ctx_t ctx);
static bool bnxt_if_needs_restart(if_ctx_t, enum iflib_restart_event);

/*
 * Device Interface Declaration
 */

static device_method_t bnxt_methods[] = {
	/* Device interface */
	DEVMETHOD(device_register, bnxt_register),
	DEVMETHOD(device_probe, iflib_device_probe),
	DEVMETHOD(device_attach, iflib_device_attach),
	DEVMETHOD(device_detach, iflib_device_detach),
	DEVMETHOD(device_shutdown, iflib_device_shutdown),
	DEVMETHOD(device_suspend, iflib_device_suspend),
	DEVMETHOD(device_resume, iflib_device_resume),
	DEVMETHOD_END
};

static driver_t bnxt_driver = {
	"bnxt", bnxt_methods, sizeof(struct bnxt_softc),
};

DRIVER_MODULE(bnxt, pci, bnxt_driver, 0, 0);

MODULE_DEPEND(bnxt, pci, 1, 1, 1);
MODULE_DEPEND(bnxt, ether, 1, 1, 1);
MODULE_DEPEND(bnxt, iflib, 1, 1, 1);

IFLIB_PNP_INFO(pci, bnxt, bnxt_vendor_info_array);

static device_method_t bnxt_iflib_methods[] = {
	DEVMETHOD(ifdi_tx_queues_alloc, bnxt_tx_queues_alloc),
	DEVMETHOD(ifdi_rx_queues_alloc, bnxt_rx_queues_alloc),
	DEVMETHOD(ifdi_queues_free, bnxt_queues_free),

	DEVMETHOD(ifdi_attach_pre, bnxt_attach_pre),
	DEVMETHOD(ifdi_attach_post, bnxt_attach_post),
	DEVMETHOD(ifdi_detach, bnxt_detach),

	DEVMETHOD(ifdi_init, bnxt_init),
	DEVMETHOD(ifdi_stop, bnxt_stop),
	DEVMETHOD(ifdi_multi_set, bnxt_multi_set),
	DEVMETHOD(ifdi_mtu_set, bnxt_mtu_set),
	DEVMETHOD(ifdi_media_status, bnxt_media_status),
	DEVMETHOD(ifdi_media_change, bnxt_media_change),
	DEVMETHOD(ifdi_promisc_set, bnxt_promisc_set),
	DEVMETHOD(ifdi_get_counter, bnxt_get_counter),
	DEVMETHOD(ifdi_update_admin_status, bnxt_update_admin_status),
	DEVMETHOD(ifdi_timer, bnxt_if_timer),

	DEVMETHOD(ifdi_intr_enable, bnxt_intr_enable),
	DEVMETHOD(ifdi_tx_queue_intr_enable, bnxt_tx_queue_intr_enable),
	DEVMETHOD(ifdi_rx_queue_intr_enable, bnxt_rx_queue_intr_enable),
	DEVMETHOD(ifdi_intr_disable, bnxt_disable_intr),
	DEVMETHOD(ifdi_msix_intr_assign, bnxt_msix_intr_assign),

	DEVMETHOD(ifdi_vlan_register, bnxt_vlan_register),
	DEVMETHOD(ifdi_vlan_unregister, bnxt_vlan_unregister),

	DEVMETHOD(ifdi_priv_ioctl, bnxt_priv_ioctl),

	DEVMETHOD(ifdi_suspend, bnxt_suspend),
	DEVMETHOD(ifdi_shutdown, bnxt_shutdown),
	DEVMETHOD(ifdi_resume, bnxt_resume),

	DEVMETHOD(ifdi_needs_restart, bnxt_if_needs_restart),

	DEVMETHOD_END
};

static driver_t bnxt_iflib_driver = {
	"bnxt", bnxt_iflib_methods, sizeof(struct bnxt_softc)
};

/*
 * iflib shared context
 */

#define BNXT_DRIVER_VERSION	"2.20.0.1"
const char bnxt_driver_version[] = BNXT_DRIVER_VERSION;
extern struct if_txrx bnxt_txrx;
static struct if_shared_ctx bnxt_sctx_init = {
	.isc_magic = IFLIB_MAGIC,
	.isc_driver = &bnxt_iflib_driver,
	.isc_nfl = 2,				// Number of Free Lists
	.isc_flags = IFLIB_HAS_RXCQ | IFLIB_HAS_TXCQ | IFLIB_NEED_ETHER_PAD,
	.isc_q_align = PAGE_SIZE,
	.isc_tx_maxsize = BNXT_TSO_SIZE + sizeof(struct ether_vlan_header),
	.isc_tx_maxsegsize = BNXT_TSO_SIZE + sizeof(struct ether_vlan_header),
	.isc_tso_maxsize = BNXT_TSO_SIZE + sizeof(struct ether_vlan_header),
	.isc_tso_maxsegsize = BNXT_TSO_SIZE + sizeof(struct ether_vlan_header),
	.isc_rx_maxsize = BNXT_TSO_SIZE + sizeof(struct ether_vlan_header),
	.isc_rx_maxsegsize = BNXT_TSO_SIZE + sizeof(struct ether_vlan_header),

	// Only use a single segment to avoid page size constraints
	.isc_rx_nsegments = 1,
	.isc_ntxqs = 3,
	.isc_nrxqs = 3,
	.isc_nrxd_min = {16, 16, 16},
	.isc_nrxd_default = {PAGE_SIZE / sizeof(struct cmpl_base) * 8,
	    PAGE_SIZE / sizeof(struct rx_prod_pkt_bd),
	    PAGE_SIZE / sizeof(struct rx_prod_pkt_bd)},
	.isc_nrxd_max = {BNXT_MAX_RXD, BNXT_MAX_RXD, BNXT_MAX_RXD},
	.isc_ntxd_min = {16, 16, 16},
	.isc_ntxd_default = {PAGE_SIZE / sizeof(struct cmpl_base) * 2,
	    PAGE_SIZE / sizeof(struct tx_bd_short),
	    PAGE_SIZE / sizeof(struct cmpl_base) * 2},
	.isc_ntxd_max = {BNXT_MAX_TXD, BNXT_MAX_TXD, BNXT_MAX_TXD},

	.isc_admin_intrcnt = 1,
	.isc_vendor_info = bnxt_vendor_info_array,
	.isc_driver_version = bnxt_driver_version,
};

/*
 * Device Methods
 */

static void *
bnxt_register(device_t dev)
{
	return (&bnxt_sctx_init);
}

static void
bnxt_nq_alloc(struct bnxt_softc *softc, int nqsets)
{

	if (softc->nq_rings)
		return;

	softc->nq_rings = malloc(sizeof(struct bnxt_cp_ring) * nqsets,
	    M_DEVBUF, M_NOWAIT | M_ZERO);
}

static void
bnxt_nq_free(struct bnxt_softc *softc)
{

	if (softc->nq_rings)
		free(softc->nq_rings, M_DEVBUF);
	softc->nq_rings = NULL;
}

/*
 * Device Dependent Configuration Functions
*/

/* Soft queue setup and teardown */
static int
bnxt_tx_queues_alloc(if_ctx_t ctx, caddr_t *vaddrs,
    uint64_t *paddrs, int ntxqs, int ntxqsets)
{
	struct bnxt_softc *softc;
	int i;
	int rc;

	softc = iflib_get_softc(ctx);

	if (BNXT_CHIP_P5(softc)) {
		bnxt_nq_alloc(softc, ntxqsets);
		if (!softc->nq_rings) {
			device_printf(iflib_get_dev(ctx),
					"unable to allocate NQ rings\n");
			rc = ENOMEM;
			goto nq_alloc_fail;
		}
	}

	softc->tx_cp_rings = malloc(sizeof(struct bnxt_cp_ring) * ntxqsets,
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (!softc->tx_cp_rings) {
		device_printf(iflib_get_dev(ctx),
		    "unable to allocate TX completion rings\n");
		rc = ENOMEM;
		goto cp_alloc_fail;
	}
	softc->tx_rings = malloc(sizeof(struct bnxt_ring) * ntxqsets,
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (!softc->tx_rings) {
		device_printf(iflib_get_dev(ctx),
		    "unable to allocate TX rings\n");
		rc = ENOMEM;
		goto ring_alloc_fail;
	}

	for (i=0; i < ntxqsets; i++) {
		rc = iflib_dma_alloc(ctx, sizeof(struct ctx_hw_stats),
				&softc->tx_stats[i], 0);
		if (rc)
			goto dma_alloc_fail;
		bus_dmamap_sync(softc->tx_stats[i].idi_tag, softc->tx_stats[i].idi_map,
				BUS_DMASYNC_PREREAD);
	}

	for (i = 0; i < ntxqsets; i++) {
		/* Set up the completion ring */
		softc->tx_cp_rings[i].stats_ctx_id = HWRM_NA_SIGNATURE;
		softc->tx_cp_rings[i].ring.phys_id =
		    (uint16_t)HWRM_NA_SIGNATURE;
		softc->tx_cp_rings[i].ring.softc = softc;
		softc->tx_cp_rings[i].ring.idx = i;
		softc->tx_cp_rings[i].ring.id =
		    (softc->scctx->isc_nrxqsets * 2) + 1 + i;
		softc->tx_cp_rings[i].ring.doorbell = (BNXT_CHIP_P5(softc)) ?
			DB_PF_OFFSET_P5: softc->tx_cp_rings[i].ring.id * 0x80;
		softc->tx_cp_rings[i].ring.ring_size =
		    softc->scctx->isc_ntxd[0];
		softc->tx_cp_rings[i].ring.vaddr = vaddrs[i * ntxqs];
		softc->tx_cp_rings[i].ring.paddr = paddrs[i * ntxqs];

		/* Set up the TX ring */
		softc->tx_rings[i].phys_id = (uint16_t)HWRM_NA_SIGNATURE;
		softc->tx_rings[i].softc = softc;
		softc->tx_rings[i].idx = i;
		softc->tx_rings[i].id =
		    (softc->scctx->isc_nrxqsets * 2) + 1 + i;
		softc->tx_rings[i].doorbell = (BNXT_CHIP_P5(softc)) ?
			DB_PF_OFFSET_P5 : softc->tx_rings[i].id * 0x80;
		softc->tx_rings[i].ring_size = softc->scctx->isc_ntxd[1];
		softc->tx_rings[i].vaddr = vaddrs[i * ntxqs + 1];
		softc->tx_rings[i].paddr = paddrs[i * ntxqs + 1];

		bnxt_create_tx_sysctls(softc, i);

		if (BNXT_CHIP_P5(softc)) {
			/* Set up the Notification ring (NQ) */
			softc->nq_rings[i].stats_ctx_id = HWRM_NA_SIGNATURE;
			softc->nq_rings[i].ring.phys_id =
				(uint16_t)HWRM_NA_SIGNATURE;
			softc->nq_rings[i].ring.softc = softc;
			softc->nq_rings[i].ring.idx = i;
			softc->nq_rings[i].ring.id = i;
			softc->nq_rings[i].ring.doorbell = (BNXT_CHIP_P5(softc)) ?
				DB_PF_OFFSET_P5 : softc->nq_rings[i].ring.id * 0x80;
			softc->nq_rings[i].ring.ring_size = softc->scctx->isc_ntxd[2];
			softc->nq_rings[i].ring.vaddr = vaddrs[i * ntxqs + 2];
			softc->nq_rings[i].ring.paddr = paddrs[i * ntxqs + 2];
		}
	}

	softc->ntxqsets = ntxqsets;
	return rc;

dma_alloc_fail:
	for (i = i - 1; i >= 0; i--)
		iflib_dma_free(&softc->tx_stats[i]);
	free(softc->tx_rings, M_DEVBUF);
ring_alloc_fail:
	free(softc->tx_cp_rings, M_DEVBUF);
cp_alloc_fail:
	bnxt_nq_free(softc);
nq_alloc_fail:
	return rc;
}

static void
bnxt_queues_free(if_ctx_t ctx)
{
	struct bnxt_softc *softc = iflib_get_softc(ctx);
	int i;

	// Free TX queues
	for (i=0; i<softc->ntxqsets; i++)
		iflib_dma_free(&softc->tx_stats[i]);
	free(softc->tx_rings, M_DEVBUF);
	softc->tx_rings = NULL;
	free(softc->tx_cp_rings, M_DEVBUF);
	softc->tx_cp_rings = NULL;
	softc->ntxqsets = 0;

	// Free RX queues
	for (i=0; i<softc->nrxqsets; i++)
		iflib_dma_free(&softc->rx_stats[i]);
	iflib_dma_free(&softc->hw_tx_port_stats);
	iflib_dma_free(&softc->hw_rx_port_stats);
	free(softc->grp_info, M_DEVBUF);
	free(softc->ag_rings, M_DEVBUF);
	free(softc->rx_rings, M_DEVBUF);
	free(softc->rx_cp_rings, M_DEVBUF);
	bnxt_nq_free(softc);
}

static int
bnxt_rx_queues_alloc(if_ctx_t ctx, caddr_t *vaddrs,
    uint64_t *paddrs, int nrxqs, int nrxqsets)
{
	struct bnxt_softc *softc;
	int i;
	int rc;

	softc = iflib_get_softc(ctx);

	softc->rx_cp_rings = malloc(sizeof(struct bnxt_cp_ring) * nrxqsets,
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (!softc->rx_cp_rings) {
		device_printf(iflib_get_dev(ctx),
		    "unable to allocate RX completion rings\n");
		rc = ENOMEM;
		goto cp_alloc_fail;
	}
	softc->rx_rings = malloc(sizeof(struct bnxt_ring) * nrxqsets,
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (!softc->rx_rings) {
		device_printf(iflib_get_dev(ctx),
		    "unable to allocate RX rings\n");
		rc = ENOMEM;
		goto ring_alloc_fail;
	}
	softc->ag_rings = malloc(sizeof(struct bnxt_ring) * nrxqsets,
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (!softc->ag_rings) {
		device_printf(iflib_get_dev(ctx),
		    "unable to allocate aggregation rings\n");
		rc = ENOMEM;
		goto ag_alloc_fail;
	}
	softc->grp_info = malloc(sizeof(struct bnxt_grp_info) * nrxqsets,
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (!softc->grp_info) {
		device_printf(iflib_get_dev(ctx),
		    "unable to allocate ring groups\n");
		rc = ENOMEM;
		goto grp_alloc_fail;
	}

	for (i=0; i < nrxqsets; i++) {
		rc = iflib_dma_alloc(ctx, sizeof(struct ctx_hw_stats),
				&softc->rx_stats[i], 0);
		if (rc)
			goto hw_stats_alloc_fail;
		bus_dmamap_sync(softc->rx_stats[i].idi_tag, softc->rx_stats[i].idi_map,
				BUS_DMASYNC_PREREAD);
	}

/*
 * Additional 512 bytes for future expansion.
 * To prevent corruption when loaded with newer firmwares with added counters.
 * This can be deleted when there will be no further additions of counters.
 */
#define BNXT_PORT_STAT_PADDING  512

	rc = iflib_dma_alloc(ctx, sizeof(struct rx_port_stats) + BNXT_PORT_STAT_PADDING,
	    &softc->hw_rx_port_stats, 0);
	if (rc)
		goto hw_port_rx_stats_alloc_fail;

	bus_dmamap_sync(softc->hw_rx_port_stats.idi_tag,
            softc->hw_rx_port_stats.idi_map, BUS_DMASYNC_PREREAD);

	rc = iflib_dma_alloc(ctx, sizeof(struct tx_port_stats) + BNXT_PORT_STAT_PADDING,
	    &softc->hw_tx_port_stats, 0);

	if (rc)
		goto hw_port_tx_stats_alloc_fail;

	bus_dmamap_sync(softc->hw_tx_port_stats.idi_tag,
            softc->hw_tx_port_stats.idi_map, BUS_DMASYNC_PREREAD);

	softc->rx_port_stats = (void *) softc->hw_rx_port_stats.idi_vaddr;
	softc->tx_port_stats = (void *) softc->hw_tx_port_stats.idi_vaddr;

	for (i = 0; i < nrxqsets; i++) {
		/* Allocation the completion ring */
		softc->rx_cp_rings[i].stats_ctx_id = HWRM_NA_SIGNATURE;
		softc->rx_cp_rings[i].ring.phys_id =
		    (uint16_t)HWRM_NA_SIGNATURE;
		softc->rx_cp_rings[i].ring.softc = softc;
		softc->rx_cp_rings[i].ring.idx = i;
		softc->rx_cp_rings[i].ring.id = i + 1;
		softc->rx_cp_rings[i].ring.doorbell = (BNXT_CHIP_P5(softc)) ?
			DB_PF_OFFSET_P5 : softc->rx_cp_rings[i].ring.id * 0x80;
		/*
		 * If this ring overflows, RX stops working.
		 */
		softc->rx_cp_rings[i].ring.ring_size =
		    softc->scctx->isc_nrxd[0];
		softc->rx_cp_rings[i].ring.vaddr = vaddrs[i * nrxqs];
		softc->rx_cp_rings[i].ring.paddr = paddrs[i * nrxqs];

		/* Allocate the RX ring */
		softc->rx_rings[i].phys_id = (uint16_t)HWRM_NA_SIGNATURE;
		softc->rx_rings[i].softc = softc;
		softc->rx_rings[i].idx = i;
		softc->rx_rings[i].id = i + 1;
		softc->rx_rings[i].doorbell = (BNXT_CHIP_P5(softc)) ?
			DB_PF_OFFSET_P5 : softc->rx_rings[i].id * 0x80;
		softc->rx_rings[i].ring_size = softc->scctx->isc_nrxd[1];
		softc->rx_rings[i].vaddr = vaddrs[i * nrxqs + 1];
		softc->rx_rings[i].paddr = paddrs[i * nrxqs + 1];

		/* Allocate the TPA start buffer */
		softc->rx_rings[i].tpa_start = malloc(sizeof(struct bnxt_full_tpa_start) *
	    		(RX_TPA_START_CMPL_AGG_ID_MASK >> RX_TPA_START_CMPL_AGG_ID_SFT),
	    		M_DEVBUF, M_NOWAIT | M_ZERO);
		if (softc->rx_rings[i].tpa_start == NULL) {
			rc = -ENOMEM;
			device_printf(softc->dev,
					"Unable to allocate space for TPA\n");
			goto tpa_alloc_fail;
		}

		/* Allocate the AG ring */
		softc->ag_rings[i].phys_id = (uint16_t)HWRM_NA_SIGNATURE;
		softc->ag_rings[i].softc = softc;
		softc->ag_rings[i].idx = i;
		softc->ag_rings[i].id = nrxqsets + i + 1;
		softc->ag_rings[i].doorbell = (BNXT_CHIP_P5(softc)) ?
			DB_PF_OFFSET_P5 : softc->ag_rings[i].id * 0x80;
		softc->ag_rings[i].ring_size = softc->scctx->isc_nrxd[2];
		softc->ag_rings[i].vaddr = vaddrs[i * nrxqs + 2];
		softc->ag_rings[i].paddr = paddrs[i * nrxqs + 2];

		/* Allocate the ring group */
		softc->grp_info[i].grp_id = (uint16_t)HWRM_NA_SIGNATURE;
		softc->grp_info[i].stats_ctx =
		    softc->rx_cp_rings[i].stats_ctx_id;
		softc->grp_info[i].rx_ring_id = softc->rx_rings[i].phys_id;
		softc->grp_info[i].ag_ring_id = softc->ag_rings[i].phys_id;
		softc->grp_info[i].cp_ring_id =
		    softc->rx_cp_rings[i].ring.phys_id;

		bnxt_create_rx_sysctls(softc, i);
	}

	/*
	 * When SR-IOV is enabled, avoid each VF sending PORT_QSTATS
         * HWRM every sec with which firmware timeouts can happen
         */
	if (BNXT_PF(softc))
        	bnxt_create_port_stats_sysctls(softc);

	/* And finally, the VNIC */
	softc->vnic_info.id = (uint16_t)HWRM_NA_SIGNATURE;
	softc->vnic_info.filter_id = -1;
	softc->vnic_info.def_ring_grp = (uint16_t)HWRM_NA_SIGNATURE;
	softc->vnic_info.cos_rule = (uint16_t)HWRM_NA_SIGNATURE;
	softc->vnic_info.lb_rule = (uint16_t)HWRM_NA_SIGNATURE;
	softc->vnic_info.rx_mask = HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_BCAST |
		HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_ANYVLAN_NONVLAN;
	softc->vnic_info.mc_list_count = 0;
	softc->vnic_info.flags = BNXT_VNIC_FLAG_DEFAULT;
	rc = iflib_dma_alloc(ctx, BNXT_MAX_MC_ADDRS * ETHER_ADDR_LEN,
	    &softc->vnic_info.mc_list, 0);
	if (rc)
		goto mc_list_alloc_fail;

	/* The VNIC RSS Hash Key */
	rc = iflib_dma_alloc(ctx, HW_HASH_KEY_SIZE,
	    &softc->vnic_info.rss_hash_key_tbl, 0);
	if (rc)
		goto rss_hash_alloc_fail;
	bus_dmamap_sync(softc->vnic_info.rss_hash_key_tbl.idi_tag,
	    softc->vnic_info.rss_hash_key_tbl.idi_map,
	    BUS_DMASYNC_PREWRITE);
	memcpy(softc->vnic_info.rss_hash_key_tbl.idi_vaddr,
	    softc->vnic_info.rss_hash_key, HW_HASH_KEY_SIZE);

	/* Allocate the RSS tables */
	rc = iflib_dma_alloc(ctx, HW_HASH_INDEX_SIZE * sizeof(uint16_t),
	    &softc->vnic_info.rss_grp_tbl, 0);
	if (rc)
		goto rss_grp_alloc_fail;
	bus_dmamap_sync(softc->vnic_info.rss_grp_tbl.idi_tag,
	    softc->vnic_info.rss_grp_tbl.idi_map,
	    BUS_DMASYNC_PREWRITE);
	memset(softc->vnic_info.rss_grp_tbl.idi_vaddr, 0xff,
	    softc->vnic_info.rss_grp_tbl.idi_size);

	softc->nrxqsets = nrxqsets;
	return rc;

rss_grp_alloc_fail:
	iflib_dma_free(&softc->vnic_info.rss_hash_key_tbl);
rss_hash_alloc_fail:
	iflib_dma_free(&softc->vnic_info.mc_list);
tpa_alloc_fail:
mc_list_alloc_fail:
	for (i = i - 1; i >= 0; i--)
		free(softc->rx_rings[i].tpa_start, M_DEVBUF);
	iflib_dma_free(&softc->hw_tx_port_stats);
hw_port_tx_stats_alloc_fail:
	iflib_dma_free(&softc->hw_rx_port_stats);
hw_port_rx_stats_alloc_fail:
	for (i = i - 1; i >= 0; i--)
		iflib_dma_free(&softc->rx_stats[i]);
hw_stats_alloc_fail:
	free(softc->grp_info, M_DEVBUF);
grp_alloc_fail:
	free(softc->ag_rings, M_DEVBUF);
ag_alloc_fail:
	free(softc->rx_rings, M_DEVBUF);
ring_alloc_fail:
	free(softc->rx_cp_rings, M_DEVBUF);
cp_alloc_fail:
	return rc;
}

static void bnxt_free_hwrm_short_cmd_req(struct bnxt_softc *softc)
{
	if (softc->hwrm_short_cmd_req_addr.idi_vaddr)
		iflib_dma_free(&softc->hwrm_short_cmd_req_addr);
	softc->hwrm_short_cmd_req_addr.idi_vaddr = NULL;
}

static int bnxt_alloc_hwrm_short_cmd_req(struct bnxt_softc *softc)
{
	int rc;

	rc = iflib_dma_alloc(softc->ctx, softc->hwrm_max_req_len,
	    &softc->hwrm_short_cmd_req_addr, BUS_DMA_NOWAIT);

	return rc;
}

static void bnxt_free_ring(struct bnxt_softc *bp, struct bnxt_ring_mem_info *rmem)
{
        int i;

        for (i = 0; i < rmem->nr_pages; i++) {
                if (!rmem->pg_arr[i].idi_vaddr)
                        continue;

		iflib_dma_free(&rmem->pg_arr[i]);
                rmem->pg_arr[i].idi_vaddr = NULL;
        }
        if (rmem->pg_tbl.idi_vaddr) {
		iflib_dma_free(&rmem->pg_tbl);
                rmem->pg_tbl.idi_vaddr = NULL;

        }
        if (rmem->vmem_size && *rmem->vmem) {
                free(*rmem->vmem, M_DEVBUF);
                *rmem->vmem = NULL;
        }
}

static int bnxt_alloc_ring(struct bnxt_softc *softc, struct bnxt_ring_mem_info *rmem)
{
	uint64_t valid_bit = 0;
	int i;
	int rc;

	if (rmem->flags & (BNXT_RMEM_VALID_PTE_FLAG | BNXT_RMEM_RING_PTE_FLAG))
		valid_bit = PTU_PTE_VALID;

	if ((rmem->nr_pages > 1 || rmem->depth > 0) && !rmem->pg_tbl.idi_vaddr) {
		size_t pg_tbl_size = rmem->nr_pages * 8;

		if (rmem->flags & BNXT_RMEM_USE_FULL_PAGE_FLAG)
			pg_tbl_size = rmem->page_size;

		rc = iflib_dma_alloc(softc->ctx, pg_tbl_size, &rmem->pg_tbl, 0);
		if (rc)
			return -ENOMEM;
	}

	for (i = 0; i < rmem->nr_pages; i++) {
		uint64_t extra_bits = valid_bit;
		uint64_t *ptr;

		rc = iflib_dma_alloc(softc->ctx, rmem->page_size, &rmem->pg_arr[i], 0);
		if (rc)
			return -ENOMEM;

		if (rmem->init_val)
                        memset(rmem->pg_arr[i].idi_vaddr, rmem->init_val, rmem->page_size);

		if (rmem->nr_pages > 1 || rmem->depth > 0) {
			if (i == rmem->nr_pages - 2 &&
					(rmem->flags & BNXT_RMEM_RING_PTE_FLAG))
				extra_bits |= PTU_PTE_NEXT_TO_LAST;
			else if (i == rmem->nr_pages - 1 &&
					(rmem->flags & BNXT_RMEM_RING_PTE_FLAG))
				extra_bits |= PTU_PTE_LAST;

			ptr = (void *) rmem->pg_tbl.idi_vaddr;
			ptr[i]  = htole64(rmem->pg_arr[i].idi_paddr | extra_bits);
		}
	}

	if (rmem->vmem_size) {
		*rmem->vmem = malloc(rmem->vmem_size, M_DEVBUF, M_NOWAIT | M_ZERO);
		if (!(*rmem->vmem))
			return -ENOMEM;
	}
	return 0;
}

#define HWRM_FUNC_BACKING_STORE_CFG_INPUT_DFLT_ENABLES			\
	(HWRM_FUNC_BACKING_STORE_CFG_INPUT_ENABLES_QP |		\
	 HWRM_FUNC_BACKING_STORE_CFG_INPUT_ENABLES_SRQ |		\
	 HWRM_FUNC_BACKING_STORE_CFG_INPUT_ENABLES_CQ |		\
	 HWRM_FUNC_BACKING_STORE_CFG_INPUT_ENABLES_VNIC |		\
	 HWRM_FUNC_BACKING_STORE_CFG_INPUT_ENABLES_STAT)

static int bnxt_alloc_ctx_mem_blk(struct bnxt_softc *softc,
				  struct bnxt_ctx_pg_info *ctx_pg)
{
	struct bnxt_ring_mem_info *rmem = &ctx_pg->ring_mem;

	rmem->page_size = BNXT_PAGE_SIZE;
	rmem->pg_arr = ctx_pg->ctx_arr;
	rmem->flags = BNXT_RMEM_VALID_PTE_FLAG;
	if (rmem->depth >= 1)
		rmem->flags |= BNXT_RMEM_USE_FULL_PAGE_FLAG;

	return bnxt_alloc_ring(softc, rmem);
}

static int bnxt_alloc_ctx_pg_tbls(struct bnxt_softc *softc,
				  struct bnxt_ctx_pg_info *ctx_pg, uint32_t mem_size,
				  uint8_t depth, bool use_init_val)
{
	struct bnxt_ring_mem_info *rmem = &ctx_pg->ring_mem;
	int rc;

	if (!mem_size)
		return 0;

	ctx_pg->nr_pages = DIV_ROUND_UP(mem_size, BNXT_PAGE_SIZE);
	if (ctx_pg->nr_pages > MAX_CTX_TOTAL_PAGES) {
		ctx_pg->nr_pages = 0;
		return -EINVAL;
	}
	if (ctx_pg->nr_pages > MAX_CTX_PAGES || depth > 1) {
		int nr_tbls, i;

		rmem->depth = 2;
		ctx_pg->ctx_pg_tbl = malloc(MAX_CTX_PAGES * sizeof(ctx_pg),
				M_DEVBUF, M_NOWAIT | M_ZERO);
		if (!ctx_pg->ctx_pg_tbl)
			return -ENOMEM;
		nr_tbls = DIV_ROUND_UP(ctx_pg->nr_pages, MAX_CTX_PAGES);
		rmem->nr_pages = nr_tbls;
		rc = bnxt_alloc_ctx_mem_blk(softc, ctx_pg);
		if (rc)
			return rc;
		for (i = 0; i < nr_tbls; i++) {
			struct bnxt_ctx_pg_info *pg_tbl;

			pg_tbl = malloc(sizeof(*pg_tbl), M_DEVBUF, M_NOWAIT | M_ZERO);
			if (!pg_tbl)
				return -ENOMEM;
			ctx_pg->ctx_pg_tbl[i] = pg_tbl;
			rmem = &pg_tbl->ring_mem;
			memcpy(&rmem->pg_tbl, &ctx_pg->ctx_arr[i], sizeof(struct iflib_dma_info));
			rmem->depth = 1;
			rmem->nr_pages = MAX_CTX_PAGES;
			if (use_init_val)
                                rmem->init_val = softc->ctx_mem->ctx_kind_initializer;
			if (i == (nr_tbls - 1)) {
				int rem = ctx_pg->nr_pages % MAX_CTX_PAGES;

				if (rem)
					rmem->nr_pages = rem;
			}
			rc = bnxt_alloc_ctx_mem_blk(softc, pg_tbl);
			if (rc)
				break;
		}
	} else {
		rmem->nr_pages = DIV_ROUND_UP(mem_size, BNXT_PAGE_SIZE);
		if (rmem->nr_pages > 1 || depth)
			rmem->depth = 1;
		if (use_init_val)
			rmem->init_val = softc->ctx_mem->ctx_kind_initializer;
		rc = bnxt_alloc_ctx_mem_blk(softc, ctx_pg);
	}
	return rc;
}

static void bnxt_free_ctx_pg_tbls(struct bnxt_softc *softc,
				  struct bnxt_ctx_pg_info *ctx_pg)
{
	struct bnxt_ring_mem_info *rmem = &ctx_pg->ring_mem;

	if (rmem->depth > 1 || ctx_pg->nr_pages > MAX_CTX_PAGES ||
	    ctx_pg->ctx_pg_tbl) {
		int i, nr_tbls = rmem->nr_pages;

		for (i = 0; i < nr_tbls; i++) {
			struct bnxt_ctx_pg_info *pg_tbl;
			struct bnxt_ring_mem_info *rmem2;

			pg_tbl = ctx_pg->ctx_pg_tbl[i];
			if (!pg_tbl)
				continue;
			rmem2 = &pg_tbl->ring_mem;
			bnxt_free_ring(softc, rmem2);
			ctx_pg->ctx_arr[i].idi_vaddr = NULL;
			free(pg_tbl , M_DEVBUF);
			ctx_pg->ctx_pg_tbl[i] = NULL;
		}
		free(ctx_pg->ctx_pg_tbl , M_DEVBUF);
		ctx_pg->ctx_pg_tbl = NULL;
	}
	bnxt_free_ring(softc, rmem);
	ctx_pg->nr_pages = 0;
}

static void bnxt_free_ctx_mem(struct bnxt_softc *softc)
{
	struct bnxt_ctx_mem_info *ctx = softc->ctx_mem;
	int i;

	if (!ctx)
		return;

	if (ctx->tqm_mem[0]) {
		for (i = 0; i < softc->max_q + 1; i++) {
			if (!ctx->tqm_mem[i])
				continue;
			bnxt_free_ctx_pg_tbls(softc, ctx->tqm_mem[i]);
		}
		free(ctx->tqm_mem[0] , M_DEVBUF);
		ctx->tqm_mem[0] = NULL;
	}

	bnxt_free_ctx_pg_tbls(softc, &ctx->tim_mem);
	bnxt_free_ctx_pg_tbls(softc, &ctx->mrav_mem);
	bnxt_free_ctx_pg_tbls(softc, &ctx->stat_mem);
	bnxt_free_ctx_pg_tbls(softc, &ctx->vnic_mem);
	bnxt_free_ctx_pg_tbls(softc, &ctx->cq_mem);
	bnxt_free_ctx_pg_tbls(softc, &ctx->srq_mem);
	bnxt_free_ctx_pg_tbls(softc, &ctx->qp_mem);
	ctx->flags &= ~BNXT_CTX_FLAG_INITED;
	free(softc->ctx_mem, M_DEVBUF);
	softc->ctx_mem = NULL;
}

static int bnxt_alloc_ctx_mem(struct bnxt_softc *softc)
{
	struct bnxt_ctx_pg_info *ctx_pg;
	struct bnxt_ctx_mem_info *ctx;
	uint32_t mem_size, ena, entries;
	int i, rc;

	if (!BNXT_CHIP_P5(softc))
		return 0;

	rc = bnxt_hwrm_func_backing_store_qcaps(softc);
	if (rc) {
		device_printf(softc->dev, "Failed querying context mem capability, rc = %d.\n",
			   rc);
		return rc;
	}
	ctx = softc->ctx_mem;
	if (!ctx || (ctx->flags & BNXT_CTX_FLAG_INITED))
		return 0;

	ctx_pg = &ctx->qp_mem;
	ctx_pg->entries = ctx->qp_min_qp1_entries + ctx->qp_max_l2_entries +
			  (1024 * 64); /* FIXME: Enable 64K QPs */
	mem_size = ctx->qp_entry_size * ctx_pg->entries;
	rc = bnxt_alloc_ctx_pg_tbls(softc, ctx_pg, mem_size, 2, true);
	if (rc)
		return rc;

	ctx_pg = &ctx->srq_mem;
	/* FIXME: Temporarily enable 8K RoCE SRQs */
	ctx_pg->entries = ctx->srq_max_l2_entries + (1024 * 8);
	mem_size = ctx->srq_entry_size * ctx_pg->entries;
	rc = bnxt_alloc_ctx_pg_tbls(softc, ctx_pg, mem_size, 2, true);
	if (rc)
		return rc;

	ctx_pg = &ctx->cq_mem;
	/* FIXME: Temporarily enable 64K RoCE CQ */
	ctx_pg->entries = ctx->cq_max_l2_entries + (1024 * 64 * 2);
	mem_size = ctx->cq_entry_size * ctx_pg->entries;
	rc = bnxt_alloc_ctx_pg_tbls(softc, ctx_pg, mem_size, 2, true);
	if (rc)
		return rc;

	ctx_pg = &ctx->vnic_mem;
	ctx_pg->entries = ctx->vnic_max_vnic_entries +
			  ctx->vnic_max_ring_table_entries;
	mem_size = ctx->vnic_entry_size * ctx_pg->entries;
	rc = bnxt_alloc_ctx_pg_tbls(softc, ctx_pg, mem_size, 1, true);
	if (rc)
		return rc;

	ctx_pg = &ctx->stat_mem;
	ctx_pg->entries = ctx->stat_max_entries;
	mem_size = ctx->stat_entry_size * ctx_pg->entries;
	rc = bnxt_alloc_ctx_pg_tbls(softc, ctx_pg, mem_size, 1, true);
	if (rc)
		return rc;

	ctx_pg = &ctx->mrav_mem;
	/* FIXME: Temporarily enable 256K RoCE MRs */
	ctx_pg->entries = 1024 * 256;
	mem_size = ctx->mrav_entry_size * ctx_pg->entries;
	rc = bnxt_alloc_ctx_pg_tbls(softc, ctx_pg, mem_size, 2, true);
	if (rc)
		return rc;
	ena = HWRM_FUNC_BACKING_STORE_CFG_INPUT_ENABLES_MRAV;

	ctx_pg = &ctx->tim_mem;
	/* Firmware needs number of TIM entries equal to
	 * number of Total QP contexts enabled, including
	 * L2 QPs.
	 */
	ctx_pg->entries = ctx->qp_min_qp1_entries +
			  ctx->qp_max_l2_entries + 1024 * 64;
	/* FIXME: L2 driver is not able to create queue depth
	 *  worth of 1M 32bit timers. Need a fix when l2-roce
	 *  interface is well designed.
	 */
	mem_size = ctx->tim_entry_size * ctx_pg->entries;
	rc = bnxt_alloc_ctx_pg_tbls(softc, ctx_pg, mem_size, 2, false);
	if (rc)
		return rc;
	ena |= HWRM_FUNC_BACKING_STORE_CFG_INPUT_ENABLES_TIM;

	/* FIXME: Temporarily increase the TQM queue depth
	 * by 1K for 1K RoCE QPs.
	 */
	entries = ctx->qp_max_l2_entries + 1024 * 64;
	entries = roundup(entries, ctx->tqm_entries_multiple);
	entries = clamp_t(uint32_t, entries, ctx->tqm_min_entries_per_ring,
			  ctx->tqm_max_entries_per_ring);
	for (i = 0; i < softc->max_q + 1; i++) {
		ctx_pg = ctx->tqm_mem[i];
		ctx_pg->entries = entries;
		mem_size = ctx->tqm_entry_size * entries;
		rc = bnxt_alloc_ctx_pg_tbls(softc, ctx_pg, mem_size, 2, false);
		if (rc)
			return rc;
		ena |= HWRM_FUNC_BACKING_STORE_CFG_INPUT_ENABLES_TQM_SP << i;
	}
	ena |= HWRM_FUNC_BACKING_STORE_CFG_INPUT_DFLT_ENABLES;
	rc = bnxt_hwrm_func_backing_store_cfg(softc, ena);
	if (rc)
		device_printf(softc->dev, "Failed configuring context mem, rc = %d.\n",
			   rc);
	else
		ctx->flags |= BNXT_CTX_FLAG_INITED;

	return 0;
}
/*
 * If we update the index, a write barrier is needed after the write to ensure
 * the completion ring has space before the RX/TX ring does.  Since we can't
 * make the RX and AG doorbells covered by the same barrier without remapping
 * MSI-X vectors, we create the barrier over the enture doorbell bar.
 * TODO: Remap the MSI-X vectors to allow a barrier to only cover the doorbells
 *       for a single ring group.
 *
 * A barrier of just the size of the write is used to ensure the ordering
 * remains correct and no writes are lost.
 */

static void bnxt_cuw_db_rx(void *db_ptr, uint16_t idx)
{
	struct bnxt_ring *ring = (struct bnxt_ring *) db_ptr;
	struct bnxt_bar_info *db_bar = &ring->softc->doorbell_bar;

	bus_space_barrier(db_bar->tag, db_bar->handle, ring->doorbell, 4,
			BUS_SPACE_BARRIER_WRITE);
	bus_space_write_4(db_bar->tag, db_bar->handle, ring->doorbell,
			htole32(RX_DOORBELL_KEY_RX | idx));
}

static void bnxt_cuw_db_tx(void *db_ptr, uint16_t idx)
{
	struct bnxt_ring *ring = (struct bnxt_ring *) db_ptr;
	struct bnxt_bar_info *db_bar = &ring->softc->doorbell_bar;

	bus_space_barrier(db_bar->tag, db_bar->handle, ring->doorbell, 4,
			BUS_SPACE_BARRIER_WRITE);
	bus_space_write_4(db_bar->tag, db_bar->handle, ring->doorbell,
			htole32(TX_DOORBELL_KEY_TX | idx));
}

static void bnxt_cuw_db_cq(void *db_ptr, bool enable_irq)
{
	struct bnxt_cp_ring *cpr = (struct bnxt_cp_ring *) db_ptr;
	struct bnxt_bar_info *db_bar = &cpr->ring.softc->doorbell_bar;

	bus_space_barrier(db_bar->tag, db_bar->handle, cpr->ring.doorbell, 4,
			BUS_SPACE_BARRIER_WRITE);
	bus_space_write_4(db_bar->tag, db_bar->handle, cpr->ring.doorbell,
			htole32(CMPL_DOORBELL_KEY_CMPL |
				((cpr->cons == UINT32_MAX) ? 0 :
				 (cpr->cons | CMPL_DOORBELL_IDX_VALID)) |
				((enable_irq) ? 0 : CMPL_DOORBELL_MASK)));
	bus_space_barrier(db_bar->tag, db_bar->handle, 0, db_bar->size,
			BUS_SPACE_BARRIER_WRITE);
}

static void bnxt_thor_db_rx(void *db_ptr, uint16_t idx)
{
	struct bnxt_ring *ring = (struct bnxt_ring *) db_ptr;
	struct bnxt_bar_info *db_bar = &ring->softc->doorbell_bar;

	bus_space_barrier(db_bar->tag, db_bar->handle, ring->doorbell, 8,
			BUS_SPACE_BARRIER_WRITE);
	bus_space_write_8(db_bar->tag, db_bar->handle, ring->doorbell,
			htole64((DBR_PATH_L2 | DBR_TYPE_SRQ | idx) |
				((uint64_t)ring->phys_id << DBR_XID_SFT)));
}

static void bnxt_thor_db_tx(void *db_ptr, uint16_t idx)
{
	struct bnxt_ring *ring = (struct bnxt_ring *) db_ptr;
	struct bnxt_bar_info *db_bar = &ring->softc->doorbell_bar;

	bus_space_barrier(db_bar->tag, db_bar->handle, ring->doorbell, 8,
			BUS_SPACE_BARRIER_WRITE);
	bus_space_write_8(db_bar->tag, db_bar->handle, ring->doorbell,
			htole64((DBR_PATH_L2 | DBR_TYPE_SQ | idx) |
				((uint64_t)ring->phys_id << DBR_XID_SFT)));
}

static void bnxt_thor_db_rx_cq(void *db_ptr, bool enable_irq)
{
	struct bnxt_cp_ring *cpr = (struct bnxt_cp_ring *) db_ptr;
	struct bnxt_bar_info *db_bar = &cpr->ring.softc->doorbell_bar;
	dbc_dbc_t db_msg = { 0 };
	uint32_t cons = cpr->cons;

	if (cons == UINT32_MAX)
		cons = 0;
	else
		cons = RING_NEXT(&cpr->ring, cons);

	db_msg.index = ((cons << DBC_DBC_INDEX_SFT) & DBC_DBC_INDEX_MASK);

	db_msg.type_path_xid = ((cpr->ring.phys_id << DBC_DBC_XID_SFT) &
			DBC_DBC_XID_MASK) | DBC_DBC_PATH_L2 |
		((enable_irq) ? DBC_DBC_TYPE_CQ_ARMALL: DBC_DBC_TYPE_CQ);

	bus_space_barrier(db_bar->tag, db_bar->handle, cpr->ring.doorbell, 8,
			BUS_SPACE_BARRIER_WRITE);
	bus_space_write_8(db_bar->tag, db_bar->handle, cpr->ring.doorbell,
			htole64(*(uint64_t *)&db_msg));
	bus_space_barrier(db_bar->tag, db_bar->handle, 0, db_bar->size,
			BUS_SPACE_BARRIER_WRITE);
}

static void bnxt_thor_db_tx_cq(void *db_ptr, bool enable_irq)
{
	struct bnxt_cp_ring *cpr = (struct bnxt_cp_ring *) db_ptr;
	struct bnxt_bar_info *db_bar = &cpr->ring.softc->doorbell_bar;
	dbc_dbc_t db_msg = { 0 };
	uint32_t cons = cpr->cons;

	db_msg.index = ((cons << DBC_DBC_INDEX_SFT) & DBC_DBC_INDEX_MASK);

	db_msg.type_path_xid = ((cpr->ring.phys_id << DBC_DBC_XID_SFT) &
			DBC_DBC_XID_MASK) | DBC_DBC_PATH_L2 |
		((enable_irq) ? DBC_DBC_TYPE_CQ_ARMALL: DBC_DBC_TYPE_CQ);

	bus_space_barrier(db_bar->tag, db_bar->handle, cpr->ring.doorbell, 8,
			BUS_SPACE_BARRIER_WRITE);
	bus_space_write_8(db_bar->tag, db_bar->handle, cpr->ring.doorbell,
			htole64(*(uint64_t *)&db_msg));
	bus_space_barrier(db_bar->tag, db_bar->handle, 0, db_bar->size,
			BUS_SPACE_BARRIER_WRITE);
}

static void bnxt_thor_db_nq(void *db_ptr, bool enable_irq)
{
	struct bnxt_cp_ring *cpr = (struct bnxt_cp_ring *) db_ptr;
	struct bnxt_bar_info *db_bar = &cpr->ring.softc->doorbell_bar;
	dbc_dbc_t db_msg = { 0 };
	uint32_t cons = cpr->cons;

	db_msg.index = ((cons << DBC_DBC_INDEX_SFT) & DBC_DBC_INDEX_MASK);

	db_msg.type_path_xid = ((cpr->ring.phys_id << DBC_DBC_XID_SFT) &
			DBC_DBC_XID_MASK) | DBC_DBC_PATH_L2 |
		((enable_irq) ? DBC_DBC_TYPE_NQ_ARM: DBC_DBC_TYPE_NQ);

	bus_space_barrier(db_bar->tag, db_bar->handle, cpr->ring.doorbell, 8,
			BUS_SPACE_BARRIER_WRITE);
	bus_space_write_8(db_bar->tag, db_bar->handle, cpr->ring.doorbell,
			htole64(*(uint64_t *)&db_msg));
	bus_space_barrier(db_bar->tag, db_bar->handle, 0, db_bar->size,
			BUS_SPACE_BARRIER_WRITE);
}

struct bnxt_softc *bnxt_find_dev(uint32_t domain, uint32_t bus, uint32_t dev_fn, char *dev_name)
{
	struct bnxt_softc_list *sc = NULL;

	SLIST_FOREACH(sc, &pf_list, next) {
		/* get the softc reference based on device name */
		if (dev_name && !strncmp(dev_name, if_name(iflib_get_ifp(sc->softc->ctx)), BNXT_MAX_STR)) {
			return sc->softc;
		}
		/* get the softc reference based on domain,bus,device,function */
		if (!dev_name &&
		    (domain == sc->softc->domain) &&
		    (bus == sc->softc->bus) &&
		    (dev_fn == sc->softc->dev_fn)) {
			return sc->softc;

		}
	}

	return NULL;
}

/* Device setup and teardown */
static int
bnxt_attach_pre(if_ctx_t ctx)
{
	struct bnxt_softc *softc = iflib_get_softc(ctx);
	if_softc_ctx_t scctx;
	int rc = 0;

	softc->ctx = ctx;
	softc->dev = iflib_get_dev(ctx);
	softc->media = iflib_get_media(ctx);
	softc->scctx = iflib_get_softc_ctx(ctx);
	softc->sctx = iflib_get_sctx(ctx);
	scctx = softc->scctx;

	/* TODO: Better way of detecting NPAR/VF is needed */
	switch (pci_get_device(softc->dev)) {
	case BCM57402_NPAR:
	case BCM57404_NPAR:
	case BCM57406_NPAR:
	case BCM57407_NPAR:
	case BCM57412_NPAR1:
	case BCM57412_NPAR2:
	case BCM57414_NPAR1:
	case BCM57414_NPAR2:
	case BCM57416_NPAR1:
	case BCM57416_NPAR2:
		softc->flags |= BNXT_FLAG_NPAR;
		break;
	case NETXTREME_C_VF1:
	case NETXTREME_C_VF2:
	case NETXTREME_C_VF3:
	case NETXTREME_E_VF1:
	case NETXTREME_E_VF2:
	case NETXTREME_E_VF3:
		softc->flags |= BNXT_FLAG_VF;
		break;
	}

#define PCI_DEVFN(device, func) ((((device) & 0x1f) << 3) | ((func) & 0x07))
	softc->domain = pci_get_domain(softc->dev);
	softc->bus = pci_get_bus(softc->dev);
	softc->slot = pci_get_slot(softc->dev);
	softc->function = pci_get_function(softc->dev);
	softc->dev_fn = PCI_DEVFN(softc->slot, softc->function);

	if (bnxt_num_pfs == 0)
		  SLIST_INIT(&pf_list);
	bnxt_num_pfs++;
	softc->list.softc = softc;
	SLIST_INSERT_HEAD(&pf_list, &softc->list, next);

	pci_enable_busmaster(softc->dev);

	if (bnxt_pci_mapping(softc))
		return (ENXIO);

	/* HWRM setup/init */
	BNXT_HWRM_LOCK_INIT(softc, device_get_nameunit(softc->dev));
	rc = bnxt_alloc_hwrm_dma_mem(softc);
	if (rc)
		goto dma_fail;

	/* Get firmware version and compare with driver */
	softc->ver_info = malloc(sizeof(struct bnxt_ver_info),
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (softc->ver_info == NULL) {
		rc = ENOMEM;
		device_printf(softc->dev,
		    "Unable to allocate space for version info\n");
		goto ver_alloc_fail;
	}
	/* Default minimum required HWRM version */
	softc->ver_info->hwrm_min_major = HWRM_VERSION_MAJOR;
	softc->ver_info->hwrm_min_minor = HWRM_VERSION_MINOR;
	softc->ver_info->hwrm_min_update = HWRM_VERSION_UPDATE;

	rc = bnxt_hwrm_ver_get(softc);
	if (rc) {
		device_printf(softc->dev, "attach: hwrm ver get failed\n");
		goto ver_fail;
	}

	/* Now perform a function reset */
	rc = bnxt_hwrm_func_reset(softc);

	if ((softc->flags & BNXT_FLAG_SHORT_CMD) ||
	    softc->hwrm_max_ext_req_len > BNXT_HWRM_MAX_REQ_LEN) {
		rc = bnxt_alloc_hwrm_short_cmd_req(softc);
		if (rc)
			goto hwrm_short_cmd_alloc_fail;
	}

	if ((softc->ver_info->chip_num == BCM57508) ||
	    (softc->ver_info->chip_num == BCM57504) ||
	    (softc->ver_info->chip_num == BCM57502))
		softc->flags |= BNXT_FLAG_CHIP_P5;

	softc->flags |= BNXT_FLAG_TPA;

	/* No TPA for Thor A0 */
	if (BNXT_CHIP_P5(softc) && (!softc->ver_info->chip_rev) &&
			(!softc->ver_info->chip_metal))
		softc->flags &= ~BNXT_FLAG_TPA;

	/* TBD ++ Add TPA support from Thor B1 */
	if (BNXT_CHIP_P5(softc))
		softc->flags &= ~BNXT_FLAG_TPA;

	/* Get NVRAM info */
	if (BNXT_PF(softc)) {
		softc->nvm_info = malloc(sizeof(struct bnxt_nvram_info),
		    M_DEVBUF, M_NOWAIT | M_ZERO);
		if (softc->nvm_info == NULL) {
			rc = ENOMEM;
			device_printf(softc->dev,
			    "Unable to allocate space for NVRAM info\n");
			goto nvm_alloc_fail;
		}

		rc = bnxt_hwrm_nvm_get_dev_info(softc, &softc->nvm_info->mfg_id,
		    &softc->nvm_info->device_id, &softc->nvm_info->sector_size,
		    &softc->nvm_info->size, &softc->nvm_info->reserved_size,
		    &softc->nvm_info->available_size);
	}

	if (BNXT_CHIP_P5(softc)) {
		softc->db_ops.bnxt_db_tx = bnxt_thor_db_tx;
		softc->db_ops.bnxt_db_rx = bnxt_thor_db_rx;
		softc->db_ops.bnxt_db_rx_cq = bnxt_thor_db_rx_cq;
		softc->db_ops.bnxt_db_tx_cq = bnxt_thor_db_tx_cq;
		softc->db_ops.bnxt_db_nq = bnxt_thor_db_nq;
	} else {
		softc->db_ops.bnxt_db_tx = bnxt_cuw_db_tx;
		softc->db_ops.bnxt_db_rx = bnxt_cuw_db_rx;
		softc->db_ops.bnxt_db_rx_cq = bnxt_cuw_db_cq;
		softc->db_ops.bnxt_db_tx_cq = bnxt_cuw_db_cq;
	}

	/* Register the driver with the FW */
	rc = bnxt_hwrm_func_drv_rgtr(softc);
	if (rc) {
		device_printf(softc->dev, "attach: hwrm drv rgtr failed\n");
		goto drv_rgtr_fail;
	}

        rc = bnxt_hwrm_func_rgtr_async_events(softc, NULL, 0);
	if (rc) {
		device_printf(softc->dev, "attach: hwrm rgtr async evts failed\n");
		goto drv_rgtr_fail;
	}

	/* Get the queue config */
	rc = bnxt_hwrm_queue_qportcfg(softc);
	if (rc) {
		device_printf(softc->dev, "attach: hwrm qportcfg failed\n");
		goto failed;
	}

	if (softc->hwrm_spec_code >= 0x10803) {
		rc = bnxt_alloc_ctx_mem(softc);
		if (rc) {
			device_printf(softc->dev, "attach: alloc_ctx_mem failed\n");
			return rc;
		}
		rc = bnxt_hwrm_func_resc_qcaps(softc, true);
		if (!rc)
			softc->flags |= BNXT_FLAG_FW_CAP_NEW_RM;
	}

	/* Get the HW capabilities */
	rc = bnxt_hwrm_func_qcaps(softc);
	if (rc)
		goto failed;

	/* Get the current configuration of this function */
	rc = bnxt_hwrm_func_qcfg(softc);
	if (rc) {
		device_printf(softc->dev, "attach: hwrm func qcfg failed\n");
		goto failed;
	}

	iflib_set_mac(ctx, softc->func.mac_addr);

	scctx->isc_txrx = &bnxt_txrx;
	scctx->isc_tx_csum_flags = (CSUM_IP | CSUM_TCP | CSUM_UDP |
	    CSUM_TCP_IPV6 | CSUM_UDP_IPV6 | CSUM_TSO);
	scctx->isc_capabilities = scctx->isc_capenable =
	    /* These are translated to hwassit bits */
	    IFCAP_TXCSUM | IFCAP_TXCSUM_IPV6 | IFCAP_TSO4 | IFCAP_TSO6 |
	    /* These are checked by iflib */
	    IFCAP_LRO | IFCAP_VLAN_HWFILTER |
	    /* These are part of the iflib mask */
	    IFCAP_RXCSUM | IFCAP_RXCSUM_IPV6 | IFCAP_VLAN_MTU |
	    IFCAP_VLAN_HWTAGGING | IFCAP_VLAN_HWTSO |
	    /* These likely get lost... */
	    IFCAP_VLAN_HWCSUM | IFCAP_JUMBO_MTU;

	if (bnxt_wol_supported(softc))
		scctx->isc_capabilities |= IFCAP_WOL_MAGIC;
	bnxt_get_wol_settings(softc);
	if (softc->wol)
		scctx->isc_capenable |= IFCAP_WOL_MAGIC;

	/* Get the queue config */
	bnxt_get_wol_settings(softc);
	if (BNXT_CHIP_P5(softc))
		bnxt_hwrm_reserve_pf_rings(softc);
	rc = bnxt_hwrm_func_qcfg(softc);
	if (rc) {
		device_printf(softc->dev, "attach: hwrm func qcfg failed\n");
		goto failed;
	}

	bnxt_clear_ids(softc);
	if (rc)
		goto failed;

	/* Now set up iflib sc */
	scctx->isc_tx_nsegments = 31,
	scctx->isc_tx_tso_segments_max = 31;
	scctx->isc_tx_tso_size_max = BNXT_TSO_SIZE;
	scctx->isc_tx_tso_segsize_max = BNXT_TSO_SIZE;
	scctx->isc_vectors = softc->func.max_cp_rings;
	scctx->isc_min_frame_size = BNXT_MIN_FRAME_SIZE;
	scctx->isc_txrx = &bnxt_txrx;

	if (scctx->isc_nrxd[0] <
	    ((scctx->isc_nrxd[1] * 4) + scctx->isc_nrxd[2]))
		device_printf(softc->dev,
		    "WARNING: nrxd0 (%d) should be at least 4 * nrxd1 (%d) + nrxd2 (%d).  Driver may be unstable\n",
		    scctx->isc_nrxd[0], scctx->isc_nrxd[1], scctx->isc_nrxd[2]);
	if (scctx->isc_ntxd[0] < scctx->isc_ntxd[1] * 2)
		device_printf(softc->dev,
		    "WARNING: ntxd0 (%d) should be at least 2 * ntxd1 (%d).  Driver may be unstable\n",
		    scctx->isc_ntxd[0], scctx->isc_ntxd[1]);
	scctx->isc_txqsizes[0] = sizeof(struct cmpl_base) * scctx->isc_ntxd[0];
	scctx->isc_txqsizes[1] = sizeof(struct tx_bd_short) *
	    scctx->isc_ntxd[1];
	scctx->isc_txqsizes[2] = sizeof(struct cmpl_base) * scctx->isc_ntxd[2];
	scctx->isc_rxqsizes[0] = sizeof(struct cmpl_base) * scctx->isc_nrxd[0];
	scctx->isc_rxqsizes[1] = sizeof(struct rx_prod_pkt_bd) *
	    scctx->isc_nrxd[1];
	scctx->isc_rxqsizes[2] = sizeof(struct rx_prod_pkt_bd) *
	    scctx->isc_nrxd[2];

	scctx->isc_nrxqsets_max = min(pci_msix_count(softc->dev)-1,
	    softc->fn_qcfg.alloc_completion_rings - 1);
	scctx->isc_nrxqsets_max = min(scctx->isc_nrxqsets_max,
	    softc->fn_qcfg.alloc_rx_rings);
	scctx->isc_nrxqsets_max = min(scctx->isc_nrxqsets_max,
	    softc->fn_qcfg.alloc_vnics);
	scctx->isc_ntxqsets_max = min(softc->fn_qcfg.alloc_tx_rings,
	    softc->fn_qcfg.alloc_completion_rings - scctx->isc_nrxqsets_max - 1);

	scctx->isc_rss_table_size = HW_HASH_INDEX_SIZE;
	scctx->isc_rss_table_mask = scctx->isc_rss_table_size - 1;

	/* iflib will map and release this bar */
	scctx->isc_msix_bar = pci_msix_table_bar(softc->dev);

        /*
         * Default settings for HW LRO (TPA):
         *  Disable HW LRO by default
         *  Can be enabled after taking care of 'packet forwarding'
         */
	if (softc->flags & BNXT_FLAG_TPA) {
		softc->hw_lro.enable = 0;
		softc->hw_lro.is_mode_gro = 0;
		softc->hw_lro.max_agg_segs = 5; /* 2^5 = 32 segs */
		softc->hw_lro.max_aggs = HWRM_VNIC_TPA_CFG_INPUT_MAX_AGGS_MAX;
		softc->hw_lro.min_agg_len = 512;
	}

	/* Allocate the default completion ring */
	softc->def_cp_ring.stats_ctx_id = HWRM_NA_SIGNATURE;
	softc->def_cp_ring.ring.phys_id = (uint16_t)HWRM_NA_SIGNATURE;
	softc->def_cp_ring.ring.softc = softc;
	softc->def_cp_ring.ring.id = 0;
	softc->def_cp_ring.ring.doorbell = (BNXT_CHIP_P5(softc)) ?
		DB_PF_OFFSET_P5 : softc->def_cp_ring.ring.id * 0x80;
	softc->def_cp_ring.ring.ring_size = PAGE_SIZE /
	    sizeof(struct cmpl_base);
	rc = iflib_dma_alloc(ctx,
	    sizeof(struct cmpl_base) * softc->def_cp_ring.ring.ring_size,
	    &softc->def_cp_ring_mem, 0);
	softc->def_cp_ring.ring.vaddr = softc->def_cp_ring_mem.idi_vaddr;
	softc->def_cp_ring.ring.paddr = softc->def_cp_ring_mem.idi_paddr;
	iflib_config_gtask_init(ctx, &softc->def_cp_task, bnxt_def_cp_task,
	    "dflt_cp");

	rc = bnxt_init_sysctl_ctx(softc);
	if (rc)
		goto init_sysctl_failed;
	if (BNXT_PF(softc)) {
		rc = bnxt_create_nvram_sysctls(softc->nvm_info);
		if (rc)
			goto failed;
	}

	arc4rand(softc->vnic_info.rss_hash_key, HW_HASH_KEY_SIZE, 0);
	softc->vnic_info.rss_hash_type =
	    HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_IPV4 |
	    HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_TCP_IPV4 |
	    HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_UDP_IPV4 |
	    HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_IPV6 |
	    HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_TCP_IPV6 |
	    HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_UDP_IPV6;
	rc = bnxt_create_config_sysctls_pre(softc);
	if (rc)
		goto failed;

	rc = bnxt_create_hw_lro_sysctls(softc);
	if (rc)
		goto failed;

	rc = bnxt_create_pause_fc_sysctls(softc);
	if (rc)
		goto failed;

	/* Initialize the vlan list */
	SLIST_INIT(&softc->vnic_info.vlan_tags);
	softc->vnic_info.vlan_tag_list.idi_vaddr = NULL;
	softc->state_bv = bit_alloc(BNXT_STATE_MAX, M_DEVBUF,
			M_WAITOK|M_ZERO);

	return (rc);

failed:
	bnxt_free_sysctl_ctx(softc);
init_sysctl_failed:
	bnxt_hwrm_func_drv_unrgtr(softc, false);
drv_rgtr_fail:
	if (BNXT_PF(softc))
		free(softc->nvm_info, M_DEVBUF);
nvm_alloc_fail:
	bnxt_free_hwrm_short_cmd_req(softc);
hwrm_short_cmd_alloc_fail:
ver_fail:
	free(softc->ver_info, M_DEVBUF);
ver_alloc_fail:
	bnxt_free_hwrm_dma_mem(softc);
dma_fail:
	BNXT_HWRM_LOCK_DESTROY(softc);
	bnxt_pci_mapping_free(softc);
	pci_disable_busmaster(softc->dev);
	return (rc);
}

static int
bnxt_attach_post(if_ctx_t ctx)
{
	struct bnxt_softc *softc = iflib_get_softc(ctx);
	if_t ifp = iflib_get_ifp(ctx);
	int rc;

	bnxt_create_config_sysctls_post(softc);

	/* Update link state etc... */
	rc = bnxt_probe_phy(softc);
	if (rc)
		goto failed;

	/* Needs to be done after probing the phy */
	bnxt_create_ver_sysctls(softc);
	bnxt_add_media_types(softc);
	ifmedia_set(softc->media, IFM_ETHER | IFM_AUTO);

	softc->scctx->isc_max_frame_size = if_getmtu(ifp) + ETHER_HDR_LEN +
	    ETHER_CRC_LEN;

	softc->rx_buf_size = min(softc->scctx->isc_max_frame_size, BNXT_PAGE_SIZE);

failed:
	return rc;
}

static int
bnxt_detach(if_ctx_t ctx)
{
	struct bnxt_softc *softc = iflib_get_softc(ctx);
	struct bnxt_vlan_tag *tag;
	struct bnxt_vlan_tag *tmp;
	int i;

	SLIST_REMOVE(&pf_list, &softc->list, bnxt_softc_list, next);
	bnxt_num_pfs--;
	bnxt_wol_config(ctx);
	bnxt_do_disable_intr(&softc->def_cp_ring);
	bnxt_free_sysctl_ctx(softc);
	bnxt_hwrm_func_reset(softc);
	bnxt_free_ctx_mem(softc);
	bnxt_clear_ids(softc);
	iflib_irq_free(ctx, &softc->def_cp_ring.irq);
	iflib_config_gtask_deinit(&softc->def_cp_task);
	/* We need to free() these here... */
	for (i = softc->nrxqsets-1; i>=0; i--) {
		if (BNXT_CHIP_P5(softc))
			iflib_irq_free(ctx, &softc->nq_rings[i].irq);
		else
			iflib_irq_free(ctx, &softc->rx_cp_rings[i].irq);

	}
	iflib_dma_free(&softc->vnic_info.mc_list);
	iflib_dma_free(&softc->vnic_info.rss_hash_key_tbl);
	iflib_dma_free(&softc->vnic_info.rss_grp_tbl);
	if (softc->vnic_info.vlan_tag_list.idi_vaddr)
		iflib_dma_free(&softc->vnic_info.vlan_tag_list);
	SLIST_FOREACH_SAFE(tag, &softc->vnic_info.vlan_tags, next, tmp)
		free(tag, M_DEVBUF);
	iflib_dma_free(&softc->def_cp_ring_mem);
	for (i = 0; i < softc->nrxqsets; i++)
		free(softc->rx_rings[i].tpa_start, M_DEVBUF);
	free(softc->ver_info, M_DEVBUF);
	if (BNXT_PF(softc))
		free(softc->nvm_info, M_DEVBUF);

	bnxt_hwrm_func_drv_unrgtr(softc, false);
	bnxt_free_hwrm_dma_mem(softc);
	bnxt_free_hwrm_short_cmd_req(softc);
	BNXT_HWRM_LOCK_DESTROY(softc);

	free(softc->state_bv, M_DEVBUF);
	pci_disable_busmaster(softc->dev);
	bnxt_pci_mapping_free(softc);

	return 0;
}

static void
bnxt_hwrm_resource_free(struct bnxt_softc *softc)
{
	int i, rc = 0;

	rc = bnxt_hwrm_ring_free(softc,
			HWRM_RING_ALLOC_INPUT_RING_TYPE_L2_CMPL,
			&softc->def_cp_ring.ring,
			(uint16_t)HWRM_NA_SIGNATURE);
	if (rc)
		goto fail;

	for (i = 0; i < softc->ntxqsets; i++) {
		rc = bnxt_hwrm_ring_free(softc,
				HWRM_RING_ALLOC_INPUT_RING_TYPE_TX,
				&softc->tx_rings[i],
				softc->tx_cp_rings[i].ring.phys_id);
		if (rc)
			goto fail;

		rc = bnxt_hwrm_ring_free(softc,
				HWRM_RING_ALLOC_INPUT_RING_TYPE_L2_CMPL,
				&softc->tx_cp_rings[i].ring,
				(uint16_t)HWRM_NA_SIGNATURE);
		if (rc)
			goto fail;

		rc = bnxt_hwrm_stat_ctx_free(softc, &softc->tx_cp_rings[i]);
		if (rc)
			goto fail;
	}
	rc = bnxt_hwrm_free_filter(softc);
	if (rc)
		goto fail;

	rc = bnxt_hwrm_vnic_free(softc, &softc->vnic_info);
	if (rc)
		goto fail;

	rc = bnxt_hwrm_vnic_ctx_free(softc, softc->vnic_info.rss_id);
	if (rc)
		goto fail;

	for (i = 0; i < softc->nrxqsets; i++) {
		rc = bnxt_hwrm_ring_grp_free(softc, &softc->grp_info[i]);
		if (rc)
			goto fail;

		rc = bnxt_hwrm_ring_free(softc,
				HWRM_RING_ALLOC_INPUT_RING_TYPE_RX_AGG,
				&softc->ag_rings[i],
				(uint16_t)HWRM_NA_SIGNATURE);
		if (rc)
			goto fail;

		rc = bnxt_hwrm_ring_free(softc,
				HWRM_RING_ALLOC_INPUT_RING_TYPE_RX,
				&softc->rx_rings[i],
				softc->rx_cp_rings[i].ring.phys_id);
		if (rc)
			goto fail;

		rc = bnxt_hwrm_ring_free(softc,
				HWRM_RING_ALLOC_INPUT_RING_TYPE_L2_CMPL,
				&softc->rx_cp_rings[i].ring,
				(uint16_t)HWRM_NA_SIGNATURE);
		if (rc)
			goto fail;

		if (BNXT_CHIP_P5(softc)) {
			rc = bnxt_hwrm_ring_free(softc,
					HWRM_RING_ALLOC_INPUT_RING_TYPE_NQ,
					&softc->nq_rings[i].ring,
					(uint16_t)HWRM_NA_SIGNATURE);
			if (rc)
				goto fail;
		}

		rc = bnxt_hwrm_stat_ctx_free(softc, &softc->rx_cp_rings[i]);
		if (rc)
			goto fail;
	}

fail:
	return;
}


static void
bnxt_func_reset(struct bnxt_softc *softc)
{

	if (!BNXT_CHIP_P5(softc)) {
		bnxt_hwrm_func_reset(softc);
		return;
	}

	bnxt_hwrm_resource_free(softc);
	return;
}

static void
bnxt_rss_grp_tbl_init(struct bnxt_softc *softc)
{
	uint16_t *rgt = (uint16_t *) softc->vnic_info.rss_grp_tbl.idi_vaddr;
	int i, j;

	for (i = 0, j = 0; i < HW_HASH_INDEX_SIZE; i++) {
		if (BNXT_CHIP_P5(softc)) {
			rgt[i++] = htole16(softc->rx_rings[j].phys_id);
			rgt[i] = htole16(softc->rx_cp_rings[j].ring.phys_id);
		} else {
			rgt[i] = htole16(softc->grp_info[j].grp_id);
		}
		if (++j == softc->nrxqsets)
			j = 0;
	}
}

/* Device configuration */
static void
bnxt_init(if_ctx_t ctx)
{
	struct bnxt_softc *softc = iflib_get_softc(ctx);
	struct ifmediareq ifmr;
	int i;
	int rc;

	if (!BNXT_CHIP_P5(softc)) {
		rc = bnxt_hwrm_func_reset(softc);
		if (rc)
			return;
	} else if (softc->is_dev_init) {
		bnxt_stop(ctx);
	}

	softc->is_dev_init = true;
	bnxt_clear_ids(softc);

	// TBD -- Check if it is needed for Thor as well
	if (BNXT_CHIP_P5(softc))
		goto skip_def_cp_ring;
	/* Allocate the default completion ring */
	softc->def_cp_ring.cons = UINT32_MAX;
	softc->def_cp_ring.v_bit = 1;
	bnxt_mark_cpr_invalid(&softc->def_cp_ring);
	rc = bnxt_hwrm_ring_alloc(softc,
			HWRM_RING_ALLOC_INPUT_RING_TYPE_L2_CMPL,
			&softc->def_cp_ring.ring);
	if (rc)
		goto fail;
skip_def_cp_ring:
	for (i = 0; i < softc->nrxqsets; i++) {
		/* Allocate the statistics context */
		rc = bnxt_hwrm_stat_ctx_alloc(softc, &softc->rx_cp_rings[i],
		    softc->rx_stats[i].idi_paddr);
		if (rc)
			goto fail;

		if (BNXT_CHIP_P5(softc)) {
			/* Allocate the NQ */
			softc->nq_rings[i].cons = 0;
			softc->nq_rings[i].v_bit = 1;
			softc->nq_rings[i].last_idx = UINT32_MAX;
			bnxt_mark_cpr_invalid(&softc->nq_rings[i]);
			rc = bnxt_hwrm_ring_alloc(softc,
					HWRM_RING_ALLOC_INPUT_RING_TYPE_NQ,
					&softc->nq_rings[i].ring);
			if (rc)
				goto fail;

			softc->db_ops.bnxt_db_nq(&softc->nq_rings[i], 1);
		}
		/* Allocate the completion ring */
		softc->rx_cp_rings[i].cons = UINT32_MAX;
		softc->rx_cp_rings[i].v_bit = 1;
		softc->rx_cp_rings[i].last_idx = UINT32_MAX;
		bnxt_mark_cpr_invalid(&softc->rx_cp_rings[i]);
		rc = bnxt_hwrm_ring_alloc(softc,
				HWRM_RING_ALLOC_INPUT_RING_TYPE_L2_CMPL,
				&softc->rx_cp_rings[i].ring);
		if (rc)
			goto fail;

		if (BNXT_CHIP_P5(softc))
			softc->db_ops.bnxt_db_rx_cq(&softc->rx_cp_rings[i], 1);

		/* Allocate the RX ring */
		rc = bnxt_hwrm_ring_alloc(softc,
		    HWRM_RING_ALLOC_INPUT_RING_TYPE_RX, &softc->rx_rings[i]);
		if (rc)
			goto fail;
		softc->db_ops.bnxt_db_rx(&softc->rx_rings[i], 0);

		/* Allocate the AG ring */
		rc = bnxt_hwrm_ring_alloc(softc,
				HWRM_RING_ALLOC_INPUT_RING_TYPE_RX_AGG,
				&softc->ag_rings[i]);
		if (rc)
			goto fail;
		softc->db_ops.bnxt_db_rx(&softc->ag_rings[i], 0);

		/* Allocate the ring group */
		softc->grp_info[i].stats_ctx =
		    softc->rx_cp_rings[i].stats_ctx_id;
		softc->grp_info[i].rx_ring_id = softc->rx_rings[i].phys_id;
		softc->grp_info[i].ag_ring_id = softc->ag_rings[i].phys_id;
		softc->grp_info[i].cp_ring_id =
		    softc->rx_cp_rings[i].ring.phys_id;
		rc = bnxt_hwrm_ring_grp_alloc(softc, &softc->grp_info[i]);
		if (rc)
			goto fail;
	}

	/* And now set the default CP / NQ ring for the async */
	rc = bnxt_cfg_async_cr(softc);
	if (rc)
		goto fail;

	/* Allocate the VNIC RSS context */
	rc = bnxt_hwrm_vnic_ctx_alloc(softc, &softc->vnic_info.rss_id);
	if (rc)
		goto fail;

	/* Allocate the vnic */
	softc->vnic_info.def_ring_grp = softc->grp_info[0].grp_id;
	softc->vnic_info.mru = softc->scctx->isc_max_frame_size;
	rc = bnxt_hwrm_vnic_alloc(softc, &softc->vnic_info);
	if (rc)
		goto fail;
	rc = bnxt_hwrm_vnic_cfg(softc, &softc->vnic_info);
	if (rc)
		goto fail;
	rc = bnxt_hwrm_vnic_set_hds(softc, &softc->vnic_info);
	if (rc)
		goto fail;
	rc = bnxt_hwrm_set_filter(softc);
	if (rc)
		goto fail;

	bnxt_rss_grp_tbl_init(softc);

	rc = bnxt_hwrm_rss_cfg(softc, &softc->vnic_info,
	    softc->vnic_info.rss_hash_type);
	if (rc)
		goto fail;

	rc = bnxt_hwrm_vnic_tpa_cfg(softc);
	if (rc)
		goto fail;

	for (i = 0; i < softc->ntxqsets; i++) {
		/* Allocate the statistics context */
		rc = bnxt_hwrm_stat_ctx_alloc(softc, &softc->tx_cp_rings[i],
		    softc->tx_stats[i].idi_paddr);
		if (rc)
			goto fail;

		/* Allocate the completion ring */
		softc->tx_cp_rings[i].cons = UINT32_MAX;
		softc->tx_cp_rings[i].v_bit = 1;
		bnxt_mark_cpr_invalid(&softc->tx_cp_rings[i]);
		rc = bnxt_hwrm_ring_alloc(softc,
				HWRM_RING_ALLOC_INPUT_RING_TYPE_L2_CMPL,
				&softc->tx_cp_rings[i].ring);
		if (rc)
			goto fail;

		if (BNXT_CHIP_P5(softc))
			softc->db_ops.bnxt_db_tx_cq(&softc->tx_cp_rings[i], 1);

		/* Allocate the TX ring */
		rc = bnxt_hwrm_ring_alloc(softc,
				HWRM_RING_ALLOC_INPUT_RING_TYPE_TX,
				&softc->tx_rings[i]);
		if (rc)
			goto fail;
		softc->db_ops.bnxt_db_tx(&softc->tx_rings[i], 0);
	}

	bnxt_do_enable_intr(&softc->def_cp_ring);
	bnxt_media_status(softc->ctx, &ifmr);
	bnxt_hwrm_cfa_l2_set_rx_mask(softc, &softc->vnic_info);
	return;

fail:
	bnxt_func_reset(softc);
	bnxt_clear_ids(softc);
	return;
}

static void
bnxt_stop(if_ctx_t ctx)
{
	struct bnxt_softc *softc = iflib_get_softc(ctx);

	softc->is_dev_init = false;
	bnxt_do_disable_intr(&softc->def_cp_ring);
	bnxt_func_reset(softc);
	bnxt_clear_ids(softc);
	return;
}

static u_int
bnxt_copy_maddr(void *arg, struct sockaddr_dl *sdl, u_int cnt)
{
	uint8_t *mta = arg;

	if (cnt == BNXT_MAX_MC_ADDRS)
		return (1);

	bcopy(LLADDR(sdl), &mta[cnt * ETHER_ADDR_LEN], ETHER_ADDR_LEN);

	return (1);
}

static void
bnxt_multi_set(if_ctx_t ctx)
{
	struct bnxt_softc *softc = iflib_get_softc(ctx);
	if_t ifp = iflib_get_ifp(ctx);
	uint8_t *mta;
	int mcnt;

	mta = softc->vnic_info.mc_list.idi_vaddr;
	bzero(mta, softc->vnic_info.mc_list.idi_size);
	mcnt = if_foreach_llmaddr(ifp, bnxt_copy_maddr, mta);

	if (mcnt > BNXT_MAX_MC_ADDRS) {
		softc->vnic_info.rx_mask |=
		    HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_ALL_MCAST;
		bnxt_hwrm_cfa_l2_set_rx_mask(softc, &softc->vnic_info);
	} else {
		softc->vnic_info.rx_mask &=
		    ~HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_ALL_MCAST;
		bus_dmamap_sync(softc->vnic_info.mc_list.idi_tag,
		    softc->vnic_info.mc_list.idi_map, BUS_DMASYNC_PREWRITE);
		softc->vnic_info.mc_list_count = mcnt;
		softc->vnic_info.rx_mask |=
		    HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_MCAST;
		if (bnxt_hwrm_cfa_l2_set_rx_mask(softc, &softc->vnic_info))
			device_printf(softc->dev,
			    "set_multi: rx_mask set failed\n");
	}
}

static int
bnxt_mtu_set(if_ctx_t ctx, uint32_t mtu)
{
	struct bnxt_softc *softc = iflib_get_softc(ctx);

	if (mtu > BNXT_MAX_MTU)
		return EINVAL;

	softc->scctx->isc_max_frame_size = mtu + ETHER_HDR_LEN + ETHER_CRC_LEN;
	softc->rx_buf_size = min(softc->scctx->isc_max_frame_size, BNXT_PAGE_SIZE);
	return 0;
}

static void
bnxt_media_status(if_ctx_t ctx, struct ifmediareq * ifmr)
{
	struct bnxt_softc *softc = iflib_get_softc(ctx);
	struct bnxt_link_info *link_info = &softc->link_info;
	struct ifmedia_entry *next;
	uint64_t target_baudrate = bnxt_get_baudrate(link_info);
	int active_media = IFM_UNKNOWN;

	bnxt_update_link(softc, true);

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	if (link_info->link_up)
		ifmr->ifm_status |= IFM_ACTIVE;
	else
		ifmr->ifm_status &= ~IFM_ACTIVE;

	if (link_info->duplex == HWRM_PORT_PHY_QCFG_OUTPUT_DUPLEX_CFG_FULL)
		ifmr->ifm_active |= IFM_FDX;
	else
		ifmr->ifm_active |= IFM_HDX;

        /*
         * Go through the list of supported media which got prepared
         * as part of bnxt_add_media_types() using api ifmedia_add().
         */
	LIST_FOREACH(next, &(iflib_get_media(ctx)->ifm_list), ifm_list) {
		if (ifmedia_baudrate(next->ifm_media) == target_baudrate) {
			active_media = next->ifm_media;
			break;
		}
	}
	ifmr->ifm_active |= active_media;

	if (link_info->flow_ctrl.rx)
		ifmr->ifm_active |= IFM_ETH_RXPAUSE;
	if (link_info->flow_ctrl.tx)
		ifmr->ifm_active |= IFM_ETH_TXPAUSE;

	bnxt_report_link(softc);
	return;
}

static int
bnxt_media_change(if_ctx_t ctx)
{
	struct bnxt_softc *softc = iflib_get_softc(ctx);
	struct ifmedia *ifm = iflib_get_media(ctx);
	struct ifmediareq ifmr;
	int rc;

	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
		return EINVAL;

	switch (IFM_SUBTYPE(ifm->ifm_media)) {
	case IFM_100_T:
		softc->link_info.autoneg &= ~BNXT_AUTONEG_SPEED;
		softc->link_info.req_link_speed =
		    HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_100MB;
		break;
	case IFM_1000_KX:
	case IFM_1000_T:
	case IFM_1000_SGMII:
	case IFM_1000_CX:
	case IFM_1000_SX:
	case IFM_1000_LX:
		softc->link_info.autoneg &= ~BNXT_AUTONEG_SPEED;
		softc->link_info.req_link_speed =
		    HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_1GB;
		break;
	case IFM_2500_KX:
	case IFM_2500_T:
		softc->link_info.autoneg &= ~BNXT_AUTONEG_SPEED;
		softc->link_info.req_link_speed =
		    HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_2_5GB;
		break;
	case IFM_10G_CR1:
	case IFM_10G_KR:
	case IFM_10G_LR:
	case IFM_10G_SR:
	case IFM_10G_T:
		softc->link_info.autoneg &= ~BNXT_AUTONEG_SPEED;
		softc->link_info.req_link_speed =
		    HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_10GB;
		break;
	case IFM_20G_KR2:
		softc->link_info.autoneg &= ~BNXT_AUTONEG_SPEED;
		softc->link_info.req_link_speed =
		    HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_20GB;
		break;
	case IFM_25G_CR:
	case IFM_25G_KR:
	case IFM_25G_SR:
		softc->link_info.autoneg &= ~BNXT_AUTONEG_SPEED;
		softc->link_info.req_link_speed =
		    HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_25GB;
		break;
	case IFM_40G_CR4:
	case IFM_40G_KR4:
	case IFM_40G_LR4:
	case IFM_40G_SR4:
	case IFM_40G_XLAUI:
	case IFM_40G_XLAUI_AC:
		softc->link_info.autoneg &= ~BNXT_AUTONEG_SPEED;
		softc->link_info.req_link_speed =
		    HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_40GB;
		break;
	case IFM_50G_CR2:
	case IFM_50G_KR2:
		softc->link_info.autoneg &= ~BNXT_AUTONEG_SPEED;
		softc->link_info.req_link_speed =
		    HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_50GB;
		break;
	case IFM_100G_CR4:
	case IFM_100G_KR4:
	case IFM_100G_LR4:
	case IFM_100G_SR4:
		softc->link_info.autoneg &= ~BNXT_AUTONEG_SPEED;
		softc->link_info.req_link_speed =
			HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_100GB;
		break;
	default:
		device_printf(softc->dev,
		    "Unsupported media type!  Using auto\n");
		/* Fall-through */
	case IFM_AUTO:
		// Auto
		softc->link_info.autoneg |= BNXT_AUTONEG_SPEED;
		break;
	}
	rc = bnxt_hwrm_set_link_setting(softc, true, true, true);
	bnxt_media_status(softc->ctx, &ifmr);
	return rc;
}

static int
bnxt_promisc_set(if_ctx_t ctx, int flags)
{
	struct bnxt_softc *softc = iflib_get_softc(ctx);
	if_t ifp = iflib_get_ifp(ctx);
	int rc;

	if (if_getflags(ifp) & IFF_ALLMULTI ||
	    if_llmaddr_count(ifp) > BNXT_MAX_MC_ADDRS)
		softc->vnic_info.rx_mask |=
		    HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_ALL_MCAST;
	else
		softc->vnic_info.rx_mask &=
		    ~HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_ALL_MCAST;

	if (if_getflags(ifp) & IFF_PROMISC)
		softc->vnic_info.rx_mask |=
		    HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_PROMISCUOUS |
		    HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_ANYVLAN_NONVLAN;
	else
		softc->vnic_info.rx_mask &=
		    ~(HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_PROMISCUOUS);

	rc = bnxt_hwrm_cfa_l2_set_rx_mask(softc, &softc->vnic_info);

	return rc;
}

static uint64_t
bnxt_get_counter(if_ctx_t ctx, ift_counter cnt)
{
	if_t ifp = iflib_get_ifp(ctx);

	if (cnt < IFCOUNTERS)
		return if_get_counter_default(ifp, cnt);

	return 0;
}

static void
bnxt_update_admin_status(if_ctx_t ctx)
{
	struct bnxt_softc *softc = iflib_get_softc(ctx);

	/*
	 * When SR-IOV is enabled, avoid each VF sending this HWRM
	 * request every sec with which firmware timeouts can happen
	 */
	if (!BNXT_PF(softc))
		return;

	bnxt_hwrm_port_qstats(softc);

	if (BNXT_CHIP_P5(softc)) {
		struct ifmediareq ifmr;

		if (bit_test(softc->state_bv, BNXT_STATE_LINK_CHANGE)) {
			bit_clear(softc->state_bv, BNXT_STATE_LINK_CHANGE);
			bnxt_media_status(softc->ctx, &ifmr);
		}
	}

	return;
}

static void
bnxt_if_timer(if_ctx_t ctx, uint16_t qid)
{

	struct bnxt_softc *softc = iflib_get_softc(ctx);
	uint64_t ticks_now = ticks;

        /* Schedule bnxt_update_admin_status() once per sec */
	if (ticks_now - softc->admin_ticks >= hz) {
		softc->admin_ticks = ticks_now;
		iflib_admin_intr_deferred(ctx);
	}

	return;
}

static void inline
bnxt_do_enable_intr(struct bnxt_cp_ring *cpr)
{
	struct bnxt_softc *softc = cpr->ring.softc;

	if (cpr->ring.phys_id == (uint16_t)HWRM_NA_SIGNATURE)
		return;

	if (BNXT_CHIP_P5(softc))
		softc->db_ops.bnxt_db_nq(cpr, 1);
	else
		softc->db_ops.bnxt_db_rx_cq(cpr, 1);
}

static void inline
bnxt_do_disable_intr(struct bnxt_cp_ring *cpr)
{
	struct bnxt_softc *softc = cpr->ring.softc;

	if (cpr->ring.phys_id == (uint16_t)HWRM_NA_SIGNATURE)
		return;

	if (BNXT_CHIP_P5(softc))
		softc->db_ops.bnxt_db_nq(cpr, 0);
	else
		softc->db_ops.bnxt_db_rx_cq(cpr, 0);
}

/* Enable all interrupts */
static void
bnxt_intr_enable(if_ctx_t ctx)
{
	struct bnxt_softc *softc = iflib_get_softc(ctx);
	int i;

	bnxt_do_enable_intr(&softc->def_cp_ring);
	for (i = 0; i < softc->nrxqsets; i++)
		if (BNXT_CHIP_P5(softc))
			softc->db_ops.bnxt_db_nq(&softc->nq_rings[i], 1);
		else
			softc->db_ops.bnxt_db_rx_cq(&softc->rx_cp_rings[i], 1);

	return;
}

/* Enable interrupt for a single queue */
static int
bnxt_tx_queue_intr_enable(if_ctx_t ctx, uint16_t qid)
{
	struct bnxt_softc *softc = iflib_get_softc(ctx);

	if (BNXT_CHIP_P5(softc))
		softc->db_ops.bnxt_db_nq(&softc->nq_rings[qid], 1);
	else
		softc->db_ops.bnxt_db_rx_cq(&softc->tx_cp_rings[qid], 1);

	return 0;
}

static void
bnxt_process_cmd_cmpl(struct bnxt_softc *softc, hwrm_cmpl_t *cmd_cmpl)
{
	device_printf(softc->dev, "cmd sequence number %d\n",
			cmd_cmpl->sequence_id);
	return;
}

static void
bnxt_process_async_msg(struct bnxt_cp_ring *cpr, tx_cmpl_t *cmpl)
{
	struct bnxt_softc *softc = cpr->ring.softc;
	uint16_t type = cmpl->flags_type & TX_CMPL_TYPE_MASK;

	switch (type) {
	case HWRM_CMPL_TYPE_HWRM_DONE:
		bnxt_process_cmd_cmpl(softc, (hwrm_cmpl_t *)cmpl);
		break;
	case HWRM_ASYNC_EVENT_CMPL_TYPE_HWRM_ASYNC_EVENT:
		bnxt_handle_async_event(softc, (cmpl_base_t *) cmpl);
		break;
	default:
		device_printf(softc->dev, "%s:%d Unhandled async message %x\n",
				__FUNCTION__, __LINE__, type);
		break;
	}
}

static void
process_nq(struct bnxt_softc *softc, uint16_t nqid)
{
	struct bnxt_cp_ring *cpr = &softc->nq_rings[nqid];
	nq_cn_t *cmp = (nq_cn_t *) cpr->ring.vaddr;
	bool v_bit = cpr->v_bit;
	uint32_t cons = cpr->cons;
	uint16_t nq_type, nqe_cnt = 0;

	while (1) {
		if (!NQ_VALID(&cmp[cons], v_bit))
			goto done;

		nq_type = NQ_CN_TYPE_MASK & cmp[cons].type;

		if (nq_type != NQ_CN_TYPE_CQ_NOTIFICATION)
			 bnxt_process_async_msg(cpr, (tx_cmpl_t *)&cmp[cons]);

		NEXT_CP_CONS_V(&cpr->ring, cons, v_bit);
		nqe_cnt++;
	}
done:
	if (nqe_cnt) {
		cpr->cons = cons;
		cpr->v_bit = v_bit;
	}
}

static int
bnxt_rx_queue_intr_enable(if_ctx_t ctx, uint16_t qid)
{
	struct bnxt_softc *softc = iflib_get_softc(ctx);

	if (BNXT_CHIP_P5(softc)) {
		process_nq(softc, qid);
		softc->db_ops.bnxt_db_nq(&softc->nq_rings[qid], 1);
	}
	softc->db_ops.bnxt_db_rx_cq(&softc->rx_cp_rings[qid], 1);
        return 0;
}

/* Disable all interrupts */
static void
bnxt_disable_intr(if_ctx_t ctx)
{
	struct bnxt_softc *softc = iflib_get_softc(ctx);
	int i;

	/*
	 * NOTE: These TX interrupts should never get enabled, so don't
	 * update the index
	 */
	for (i = 0; i < softc->nrxqsets; i++)
		if (BNXT_CHIP_P5(softc))
			softc->db_ops.bnxt_db_nq(&softc->nq_rings[i], 0);
		else
			softc->db_ops.bnxt_db_rx_cq(&softc->rx_cp_rings[i], 0);


	return;
}

static int
bnxt_msix_intr_assign(if_ctx_t ctx, int msix)
{
	struct bnxt_softc *softc = iflib_get_softc(ctx);
	struct bnxt_cp_ring *ring;
	struct if_irq *irq;
	uint16_t id;
	int rc;
	int i;
	char irq_name[16];

	if (BNXT_CHIP_P5(softc))
		goto skip_default_cp;

	rc = iflib_irq_alloc_generic(ctx, &softc->def_cp_ring.irq,
	    softc->def_cp_ring.ring.id + 1, IFLIB_INTR_ADMIN,
	    bnxt_handle_def_cp, softc, 0, "def_cp");
	if (rc) {
		device_printf(iflib_get_dev(ctx),
		    "Failed to register default completion ring handler\n");
		return rc;
	}

skip_default_cp:
	for (i=0; i<softc->scctx->isc_nrxqsets; i++) {
		if (BNXT_CHIP_P5(softc)) {
			irq = &softc->nq_rings[i].irq;
			id = softc->nq_rings[i].ring.id;
			ring = &softc->nq_rings[i];
		} else {
			irq = &softc->rx_cp_rings[i].irq;
			id = softc->rx_cp_rings[i].ring.id ;
			ring = &softc->rx_cp_rings[i];
		}
		snprintf(irq_name, sizeof(irq_name), "rxq%d", i);
		rc = iflib_irq_alloc_generic(ctx, irq, id + 1, IFLIB_INTR_RX,
				bnxt_handle_isr, ring, i, irq_name);
		if (rc) {
			device_printf(iflib_get_dev(ctx),
			    "Failed to register RX completion ring handler\n");
			i--;
			goto fail;
		}
	}

	for (i=0; i<softc->scctx->isc_ntxqsets; i++)
		iflib_softirq_alloc_generic(ctx, NULL, IFLIB_INTR_TX, NULL, i, "tx_cp");

	return rc;

fail:
	for (; i>=0; i--)
		iflib_irq_free(ctx, &softc->rx_cp_rings[i].irq);
	iflib_irq_free(ctx, &softc->def_cp_ring.irq);
	return rc;
}

/*
 * We're explicitly allowing duplicates here.  They will need to be
 * removed as many times as they are added.
 */
static void
bnxt_vlan_register(if_ctx_t ctx, uint16_t vtag)
{
	struct bnxt_softc *softc = iflib_get_softc(ctx);
	struct bnxt_vlan_tag *new_tag;

	new_tag = malloc(sizeof(struct bnxt_vlan_tag), M_DEVBUF, M_NOWAIT);
	if (new_tag == NULL)
		return;
	new_tag->tag = vtag;
	new_tag->filter_id = -1;
	SLIST_INSERT_HEAD(&softc->vnic_info.vlan_tags, new_tag, next);
};

static void
bnxt_vlan_unregister(if_ctx_t ctx, uint16_t vtag)
{
	struct bnxt_softc *softc = iflib_get_softc(ctx);
	struct bnxt_vlan_tag *vlan_tag;

	SLIST_FOREACH(vlan_tag, &softc->vnic_info.vlan_tags, next) {
		if (vlan_tag->tag == vtag) {
			SLIST_REMOVE(&softc->vnic_info.vlan_tags, vlan_tag,
			    bnxt_vlan_tag, next);
			free(vlan_tag, M_DEVBUF);
			break;
		}
	}
}

static int
bnxt_wol_config(if_ctx_t ctx)
{
	struct bnxt_softc *softc = iflib_get_softc(ctx);
	if_t ifp = iflib_get_ifp(ctx);

	if (!softc)
		return -EBUSY;

	if (!bnxt_wol_supported(softc))
		return -ENOTSUP;

	if (if_getcapenable(ifp) & IFCAP_WOL_MAGIC) {
		if (!softc->wol) {
			if (bnxt_hwrm_alloc_wol_fltr(softc))
				return -EBUSY;
			softc->wol = 1;
		}
	} else {
		if (softc->wol) {
			if (bnxt_hwrm_free_wol_fltr(softc))
				return -EBUSY;
			softc->wol = 0;
		}
	}

	return 0;
}

static bool
bnxt_if_needs_restart(if_ctx_t ctx __unused, enum iflib_restart_event event)
{
	switch (event) {
	case IFLIB_RESTART_VLAN_CONFIG:
	default:
		return (false);
	}
}

static int
bnxt_shutdown(if_ctx_t ctx)
{
	bnxt_wol_config(ctx);
	return 0;
}

static int
bnxt_suspend(if_ctx_t ctx)
{
	bnxt_wol_config(ctx);
	return 0;
}

static int
bnxt_resume(if_ctx_t ctx)
{
	struct bnxt_softc *softc = iflib_get_softc(ctx);

	bnxt_get_wol_settings(softc);
	return 0;
}

static int
bnxt_priv_ioctl(if_ctx_t ctx, u_long command, caddr_t data)
{
	struct bnxt_softc *softc = iflib_get_softc(ctx);
	struct ifreq *ifr = (struct ifreq *)data;
	struct bnxt_ioctl_header *ioh;
	size_t iol;
	int rc = ENOTSUP;
	struct bnxt_ioctl_data iod_storage, *iod = &iod_storage;

	switch (command) {
	case SIOCGPRIVATE_0:
		if ((rc = priv_check(curthread, PRIV_DRIVER)) != 0)
			goto exit;

		ioh = ifr_buffer_get_buffer(ifr);
		iol = ifr_buffer_get_length(ifr);
		if (iol > sizeof(iod_storage))
			return (EINVAL);

		if ((rc = copyin(ioh, iod, iol)) != 0)
			goto exit;

		switch (iod->hdr.type) {
		case BNXT_HWRM_NVM_FIND_DIR_ENTRY:
		{
			struct bnxt_ioctl_hwrm_nvm_find_dir_entry *find =
			    &iod->find;

			rc = bnxt_hwrm_nvm_find_dir_entry(softc, find->type,
			    &find->ordinal, find->ext, &find->index,
			    find->use_index, find->search_opt,
			    &find->data_length, &find->item_length,
			    &find->fw_ver);
			if (rc) {
				iod->hdr.rc = rc;
				rc = copyout(&iod->hdr.rc, &ioh->rc,
				    sizeof(ioh->rc));
			} else {
				iod->hdr.rc = 0;
				rc = copyout(iod, ioh, iol);
			}

			goto exit;
		}
		case BNXT_HWRM_NVM_READ:
		{
			struct bnxt_ioctl_hwrm_nvm_read *rd = &iod->read;
			struct iflib_dma_info dma_data;
			size_t offset;
			size_t remain;
			size_t csize;

			/*
			 * Some HWRM versions can't read more than 0x8000 bytes
			 */
			rc = iflib_dma_alloc(softc->ctx,
			    min(rd->length, 0x8000), &dma_data, BUS_DMA_NOWAIT);
			if (rc)
				break;
			for (remain = rd->length, offset = 0;
			    remain && offset < rd->length; offset += 0x8000) {
				csize = min(remain, 0x8000);
				rc = bnxt_hwrm_nvm_read(softc, rd->index,
				    rd->offset + offset, csize, &dma_data);
				if (rc) {
					iod->hdr.rc = rc;
					rc = copyout(&iod->hdr.rc, &ioh->rc,
					    sizeof(ioh->rc));
					break;
				} else {
					rc = copyout(dma_data.idi_vaddr,
					    rd->data + offset, csize);
					iod->hdr.rc = rc;
				}
				remain -= csize;
			}
			if (rc == 0)
				rc = copyout(iod, ioh, iol);

			iflib_dma_free(&dma_data);
			goto exit;
		}
		case BNXT_HWRM_FW_RESET:
		{
			struct bnxt_ioctl_hwrm_fw_reset *rst =
			    &iod->reset;

			rc = bnxt_hwrm_fw_reset(softc, rst->processor,
			    &rst->selfreset);
			if (rc) {
				iod->hdr.rc = rc;
				rc = copyout(&iod->hdr.rc, &ioh->rc,
				    sizeof(ioh->rc));
			} else {
				iod->hdr.rc = 0;
				rc = copyout(iod, ioh, iol);
			}

			goto exit;
		}
		case BNXT_HWRM_FW_QSTATUS:
		{
			struct bnxt_ioctl_hwrm_fw_qstatus *qstat =
			    &iod->status;

			rc = bnxt_hwrm_fw_qstatus(softc, qstat->processor,
			    &qstat->selfreset);
			if (rc) {
				iod->hdr.rc = rc;
				rc = copyout(&iod->hdr.rc, &ioh->rc,
				    sizeof(ioh->rc));
			} else {
				iod->hdr.rc = 0;
				rc = copyout(iod, ioh, iol);
			}

			goto exit;
		}
		case BNXT_HWRM_NVM_WRITE:
		{
			struct bnxt_ioctl_hwrm_nvm_write *wr =
			    &iod->write;

			rc = bnxt_hwrm_nvm_write(softc, wr->data, true,
			    wr->type, wr->ordinal, wr->ext, wr->attr,
			    wr->option, wr->data_length, wr->keep,
			    &wr->item_length, &wr->index);
			if (rc) {
				iod->hdr.rc = rc;
				rc = copyout(&iod->hdr.rc, &ioh->rc,
				    sizeof(ioh->rc));
			}
			else {
				iod->hdr.rc = 0;
				rc = copyout(iod, ioh, iol);
			}

			goto exit;
		}
		case BNXT_HWRM_NVM_ERASE_DIR_ENTRY:
		{
			struct bnxt_ioctl_hwrm_nvm_erase_dir_entry *erase =
			    &iod->erase;

			rc = bnxt_hwrm_nvm_erase_dir_entry(softc, erase->index);
			if (rc) {
				iod->hdr.rc = rc;
				rc = copyout(&iod->hdr.rc, &ioh->rc,
				    sizeof(ioh->rc));
			} else {
				iod->hdr.rc = 0;
				rc = copyout(iod, ioh, iol);
			}

			goto exit;
		}
		case BNXT_HWRM_NVM_GET_DIR_INFO:
		{
			struct bnxt_ioctl_hwrm_nvm_get_dir_info *info =
			    &iod->dir_info;

			rc = bnxt_hwrm_nvm_get_dir_info(softc, &info->entries,
			    &info->entry_length);
			if (rc) {
				iod->hdr.rc = rc;
				rc = copyout(&iod->hdr.rc, &ioh->rc,
				    sizeof(ioh->rc));
			} else {
				iod->hdr.rc = 0;
				rc = copyout(iod, ioh, iol);
			}

			goto exit;
		}
		case BNXT_HWRM_NVM_GET_DIR_ENTRIES:
		{
			struct bnxt_ioctl_hwrm_nvm_get_dir_entries *get =
			    &iod->dir_entries;
			struct iflib_dma_info dma_data;

			rc = iflib_dma_alloc(softc->ctx, get->max_size,
			    &dma_data, BUS_DMA_NOWAIT);
			if (rc)
				break;
			rc = bnxt_hwrm_nvm_get_dir_entries(softc, &get->entries,
			    &get->entry_length, &dma_data);
			if (rc) {
				iod->hdr.rc = rc;
				rc = copyout(&iod->hdr.rc, &ioh->rc,
				    sizeof(ioh->rc));
			} else {
				rc = copyout(dma_data.idi_vaddr, get->data,
				    get->entry_length * get->entries);
				iod->hdr.rc = rc;
				if (rc == 0)
					rc = copyout(iod, ioh, iol);
			}
			iflib_dma_free(&dma_data);

			goto exit;
		}
		case BNXT_HWRM_NVM_VERIFY_UPDATE:
		{
			struct bnxt_ioctl_hwrm_nvm_verify_update *vrfy =
			    &iod->verify;

			rc = bnxt_hwrm_nvm_verify_update(softc, vrfy->type,
			    vrfy->ordinal, vrfy->ext);
			if (rc) {
				iod->hdr.rc = rc;
				rc = copyout(&iod->hdr.rc, &ioh->rc,
				    sizeof(ioh->rc));
			} else {
				iod->hdr.rc = 0;
				rc = copyout(iod, ioh, iol);
			}

			goto exit;
		}
		case BNXT_HWRM_NVM_INSTALL_UPDATE:
		{
			struct bnxt_ioctl_hwrm_nvm_install_update *inst =
			    &iod->install;

			rc = bnxt_hwrm_nvm_install_update(softc,
			    inst->install_type, &inst->installed_items,
			    &inst->result, &inst->problem_item,
			    &inst->reset_required);
			if (rc) {
				iod->hdr.rc = rc;
				rc = copyout(&iod->hdr.rc, &ioh->rc,
				    sizeof(ioh->rc));
			} else {
				iod->hdr.rc = 0;
				rc = copyout(iod, ioh, iol);
			}

			goto exit;
		}
		case BNXT_HWRM_NVM_MODIFY:
		{
			struct bnxt_ioctl_hwrm_nvm_modify *mod = &iod->modify;

			rc = bnxt_hwrm_nvm_modify(softc, mod->index,
			    mod->offset, mod->data, true, mod->length);
			if (rc) {
				iod->hdr.rc = rc;
				rc = copyout(&iod->hdr.rc, &ioh->rc,
				    sizeof(ioh->rc));
			} else {
				iod->hdr.rc = 0;
				rc = copyout(iod, ioh, iol);
			}

			goto exit;
		}
		case BNXT_HWRM_FW_GET_TIME:
		{
			struct bnxt_ioctl_hwrm_fw_get_time *gtm =
			    &iod->get_time;

			rc = bnxt_hwrm_fw_get_time(softc, &gtm->year,
			    &gtm->month, &gtm->day, &gtm->hour, &gtm->minute,
			    &gtm->second, &gtm->millisecond, &gtm->zone);
			if (rc) {
				iod->hdr.rc = rc;
				rc = copyout(&iod->hdr.rc, &ioh->rc,
				    sizeof(ioh->rc));
			} else {
				iod->hdr.rc = 0;
				rc = copyout(iod, ioh, iol);
			}

			goto exit;
		}
		case BNXT_HWRM_FW_SET_TIME:
		{
			struct bnxt_ioctl_hwrm_fw_set_time *stm =
			    &iod->set_time;

			rc = bnxt_hwrm_fw_set_time(softc, stm->year,
			    stm->month, stm->day, stm->hour, stm->minute,
			    stm->second, stm->millisecond, stm->zone);
			if (rc) {
				iod->hdr.rc = rc;
				rc = copyout(&iod->hdr.rc, &ioh->rc,
				    sizeof(ioh->rc));
			} else {
				iod->hdr.rc = 0;
				rc = copyout(iod, ioh, iol);
			}

			goto exit;
		}
		}
		break;
	}

exit:
	return rc;
}

/*
 * Support functions
 */
static int
bnxt_probe_phy(struct bnxt_softc *softc)
{
	struct bnxt_link_info *link_info = &softc->link_info;
	int rc = 0;

	rc = bnxt_update_link(softc, false);
	if (rc) {
		device_printf(softc->dev,
		    "Probe phy can't update link (rc: %x)\n", rc);
		return (rc);
	}

	/*initialize the ethool setting copy with NVM settings */
	if (link_info->auto_mode != HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_MODE_NONE)
		link_info->autoneg |= BNXT_AUTONEG_SPEED;

	link_info->req_duplex = link_info->duplex_setting;
	if (link_info->autoneg & BNXT_AUTONEG_SPEED)
		link_info->req_link_speed = link_info->auto_link_speed;
	else
		link_info->req_link_speed = link_info->force_link_speed;
	return (rc);
}

static void
bnxt_add_media_types(struct bnxt_softc *softc)
{
	struct bnxt_link_info *link_info = &softc->link_info;
	uint16_t supported;
	uint8_t phy_type = get_phy_type(softc);

	supported = link_info->support_speeds;

	/* Auto is always supported */
	ifmedia_add(softc->media, IFM_ETHER | IFM_AUTO, 0, NULL);

	if (softc->flags & BNXT_FLAG_NPAR)
		return;

	switch (phy_type) {
	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_100G_BASECR4:
	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_40G_BASECR4:
	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_25G_BASECR_CA_L:
	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_25G_BASECR_CA_S:
	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_25G_BASECR_CA_N:
	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_BASECR:
		BNXT_IFMEDIA_ADD(supported, SPEEDS_100GB, IFM_100G_CR4);
		BNXT_IFMEDIA_ADD(supported, SPEEDS_50GB, IFM_50G_CR2);
		BNXT_IFMEDIA_ADD(supported, SPEEDS_40GB, IFM_40G_CR4);
		BNXT_IFMEDIA_ADD(supported, SPEEDS_25GB, IFM_25G_CR);
		BNXT_IFMEDIA_ADD(supported, SPEEDS_10GB, IFM_10G_CR1);
		BNXT_IFMEDIA_ADD(supported, SPEEDS_1GB, IFM_1000_CX);
		break;

	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_100G_BASELR4:
	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_40G_BASELR4:
	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_BASELR:
		BNXT_IFMEDIA_ADD(supported, SPEEDS_100GB, IFM_100G_LR4);
		BNXT_IFMEDIA_ADD(supported, SPEEDS_40GB, IFM_40G_LR4);
		BNXT_IFMEDIA_ADD(supported, SPEEDS_25GB, IFM_25G_LR);
		BNXT_IFMEDIA_ADD(supported, SPEEDS_10GB, IFM_10G_LR);
		BNXT_IFMEDIA_ADD(supported, SPEEDS_1GB, IFM_1000_LX);
		break;

	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_100G_BASESR10:
	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_100G_BASESR4:
	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_40G_BASESR4:
	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_BASESR:
	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_40G_BASEER4:
	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_100G_BASEER4:
	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_25G_BASESR:
	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_1G_BASESX:
		BNXT_IFMEDIA_ADD(supported, SPEEDS_100GB, IFM_100G_SR4);
		BNXT_IFMEDIA_ADD(supported, SPEEDS_40GB, IFM_40G_SR4);
		BNXT_IFMEDIA_ADD(supported, SPEEDS_25GB, IFM_25G_SR);
		BNXT_IFMEDIA_ADD(supported, SPEEDS_10GB, IFM_10G_SR);
		BNXT_IFMEDIA_ADD(supported, SPEEDS_1GB, IFM_1000_SX);
		break;

	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_BASEKR4:
	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_BASEKR2:
	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_BASEKR:
		BNXT_IFMEDIA_ADD(supported, SPEEDS_100GB, IFM_100G_KR4);
		BNXT_IFMEDIA_ADD(supported, SPEEDS_50GB, IFM_50G_KR2);
		BNXT_IFMEDIA_ADD(supported, SPEEDS_40GB, IFM_40G_KR4);
		BNXT_IFMEDIA_ADD(supported, SPEEDS_25GB, IFM_25G_KR);
		BNXT_IFMEDIA_ADD(supported, SPEEDS_20GB, IFM_20G_KR2);
		BNXT_IFMEDIA_ADD(supported, SPEEDS_10GB, IFM_10G_KR);
		BNXT_IFMEDIA_ADD(supported, SPEEDS_1GB, IFM_1000_KX);
		break;

	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_40G_ACTIVE_CABLE:
		BNXT_IFMEDIA_ADD(supported, SPEEDS_25GB, IFM_25G_ACC);
		BNXT_IFMEDIA_ADD(supported, SPEEDS_10GB, IFM_10G_AOC);
		BNXT_IFMEDIA_ADD(supported, SPEEDS_40GB, IFM_40G_XLAUI);
		BNXT_IFMEDIA_ADD(supported, SPEEDS_40GB, IFM_40G_XLAUI_AC);
		break;

	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_1G_BASECX:
		BNXT_IFMEDIA_ADD(supported, SPEEDS_1GBHD, IFM_1000_CX);
		break;

	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_1G_BASET:
	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_BASET:
	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_BASETE:
		BNXT_IFMEDIA_ADD(supported, SPEEDS_10GB, IFM_10G_T);
		BNXT_IFMEDIA_ADD(supported, SPEEDS_2_5GB, IFM_2500_T);
		BNXT_IFMEDIA_ADD(supported, SPEEDS_1GB, IFM_1000_T);
		BNXT_IFMEDIA_ADD(supported, SPEEDS_100MB, IFM_100_T);
		BNXT_IFMEDIA_ADD(supported, SPEEDS_10MB, IFM_10_T);
		break;

	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_BASEKX:
		BNXT_IFMEDIA_ADD(supported, SPEEDS_10GB, IFM_10G_KR);
		BNXT_IFMEDIA_ADD(supported, SPEEDS_2_5GB, IFM_2500_KX);
		BNXT_IFMEDIA_ADD(supported, SPEEDS_1GB, IFM_1000_KX);
		break;

	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_SGMIIEXTPHY:
		BNXT_IFMEDIA_ADD(supported, SPEEDS_1GB, IFM_1000_SGMII);
		break;

	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_UNKNOWN:
		/* Only Autoneg is supported for TYPE_UNKNOWN */
		device_printf(softc->dev, "Unknown phy type\n");
		break;

        default:
		/* Only Autoneg is supported for new phy type values */
		device_printf(softc->dev, "phy type %d not supported by driver\n", phy_type);
		break;
	}

	return;
}

static int
bnxt_map_bar(struct bnxt_softc *softc, struct bnxt_bar_info *bar, int bar_num, bool shareable)
{
	uint32_t	flag;

	if (bar->res != NULL) {
		device_printf(softc->dev, "Bar %d already mapped\n", bar_num);
		return EDOOFUS;
	}

	bar->rid = PCIR_BAR(bar_num);
	flag = RF_ACTIVE;
	if (shareable)
		flag |= RF_SHAREABLE;

	if ((bar->res =
		bus_alloc_resource_any(softc->dev,
			   SYS_RES_MEMORY,
			   &bar->rid,
			   flag)) == NULL) {
		device_printf(softc->dev,
		    "PCI BAR%d mapping failure\n", bar_num);
		return (ENXIO);
	}
	bar->tag = rman_get_bustag(bar->res);
	bar->handle = rman_get_bushandle(bar->res);
	bar->size = rman_get_size(bar->res);

	return 0;
}

static int
bnxt_pci_mapping(struct bnxt_softc *softc)
{
	int rc;

	rc = bnxt_map_bar(softc, &softc->hwrm_bar, 0, true);
	if (rc)
		return rc;

	rc = bnxt_map_bar(softc, &softc->doorbell_bar, 2, false);

	return rc;
}

static void
bnxt_pci_mapping_free(struct bnxt_softc *softc)
{
	if (softc->hwrm_bar.res != NULL)
		bus_release_resource(softc->dev, SYS_RES_MEMORY,
		    softc->hwrm_bar.rid, softc->hwrm_bar.res);
	softc->hwrm_bar.res = NULL;

	if (softc->doorbell_bar.res != NULL)
		bus_release_resource(softc->dev, SYS_RES_MEMORY,
		    softc->doorbell_bar.rid, softc->doorbell_bar.res);
	softc->doorbell_bar.res = NULL;
}

static int
bnxt_update_link(struct bnxt_softc *softc, bool chng_link_state)
{
	struct bnxt_link_info *link_info = &softc->link_info;
	uint8_t link_up = link_info->link_up;
	int rc = 0;

	rc = bnxt_hwrm_port_phy_qcfg(softc);
	if (rc)
		goto exit;

	/* TODO: need to add more logic to report VF link */
	if (chng_link_state) {
		if (link_info->phy_link_status ==
		    HWRM_PORT_PHY_QCFG_OUTPUT_LINK_LINK)
			link_info->link_up = 1;
		else
			link_info->link_up = 0;
		if (link_up != link_info->link_up)
			bnxt_report_link(softc);
	} else {
		/* always link down if not require to update link state */
		link_info->link_up = 0;
	}

exit:
	return rc;
}

void
bnxt_report_link(struct bnxt_softc *softc)
{
	struct bnxt_link_info *link_info = &softc->link_info;
	const char *duplex = NULL, *flow_ctrl = NULL;

	if (link_info->link_up == link_info->last_link_up) {
		if (!link_info->link_up)
			return;
		if ((link_info->duplex == link_info->last_duplex) &&
		    (link_info->phy_type == link_info->last_phy_type) &&
                    (!(BNXT_IS_FLOW_CTRL_CHANGED(link_info))))
			return;
	}

	if (link_info->link_up) {
		if (link_info->duplex ==
		    HWRM_PORT_PHY_QCFG_OUTPUT_DUPLEX_CFG_FULL)
			duplex = "full duplex";
		else
			duplex = "half duplex";
		if (link_info->flow_ctrl.tx & link_info->flow_ctrl.rx)
			flow_ctrl = "FC - receive & transmit";
		else if (link_info->flow_ctrl.tx)
			flow_ctrl = "FC - transmit";
		else if (link_info->flow_ctrl.rx)
			flow_ctrl = "FC - receive";
		else
			flow_ctrl = "FC - none";
		iflib_link_state_change(softc->ctx, LINK_STATE_UP,
		    IF_Gbps(100));
		device_printf(softc->dev, "Link is UP %s, %s - %d Mbps \n", duplex,
		    flow_ctrl, (link_info->link_speed * 100));
	} else {
		iflib_link_state_change(softc->ctx, LINK_STATE_DOWN,
		    bnxt_get_baudrate(&softc->link_info));
		device_printf(softc->dev, "Link is Down\n");
	}

	link_info->last_link_up = link_info->link_up;
	link_info->last_duplex = link_info->duplex;
	link_info->last_phy_type = link_info->phy_type;
	link_info->last_flow_ctrl.tx = link_info->flow_ctrl.tx;
	link_info->last_flow_ctrl.rx = link_info->flow_ctrl.rx;
	link_info->last_flow_ctrl.autoneg = link_info->flow_ctrl.autoneg;
	/* update media types */
	ifmedia_removeall(softc->media);
	bnxt_add_media_types(softc);
	ifmedia_set(softc->media, IFM_ETHER | IFM_AUTO);
}

static int
bnxt_handle_isr(void *arg)
{
	struct bnxt_cp_ring *cpr = arg;
	struct bnxt_softc *softc = cpr->ring.softc;

	cpr->int_count++;
	/* Disable further interrupts for this queue */
	if (!BNXT_CHIP_P5(softc))
		softc->db_ops.bnxt_db_rx_cq(cpr, 0);

	return FILTER_SCHEDULE_THREAD;
}

static int
bnxt_handle_def_cp(void *arg)
{
	struct bnxt_softc *softc = arg;

	softc->db_ops.bnxt_db_rx_cq(&softc->def_cp_ring, 0);
	GROUPTASK_ENQUEUE(&softc->def_cp_task);
	return FILTER_HANDLED;
}

static void
bnxt_clear_ids(struct bnxt_softc *softc)
{
	int i;

	softc->def_cp_ring.stats_ctx_id = HWRM_NA_SIGNATURE;
	softc->def_cp_ring.ring.phys_id = (uint16_t)HWRM_NA_SIGNATURE;
	softc->def_nq_ring.stats_ctx_id = HWRM_NA_SIGNATURE;
	softc->def_nq_ring.ring.phys_id = (uint16_t)HWRM_NA_SIGNATURE;
	for (i = 0; i < softc->ntxqsets; i++) {
		softc->tx_cp_rings[i].stats_ctx_id = HWRM_NA_SIGNATURE;
		softc->tx_cp_rings[i].ring.phys_id =
		    (uint16_t)HWRM_NA_SIGNATURE;
		softc->tx_rings[i].phys_id = (uint16_t)HWRM_NA_SIGNATURE;

		if (!softc->nq_rings)
			continue;
		softc->nq_rings[i].stats_ctx_id = HWRM_NA_SIGNATURE;
		softc->nq_rings[i].ring.phys_id = (uint16_t)HWRM_NA_SIGNATURE;
	}
	for (i = 0; i < softc->nrxqsets; i++) {
		softc->rx_cp_rings[i].stats_ctx_id = HWRM_NA_SIGNATURE;
		softc->rx_cp_rings[i].ring.phys_id =
		    (uint16_t)HWRM_NA_SIGNATURE;
		softc->rx_rings[i].phys_id = (uint16_t)HWRM_NA_SIGNATURE;
		softc->ag_rings[i].phys_id = (uint16_t)HWRM_NA_SIGNATURE;
		softc->grp_info[i].grp_id = (uint16_t)HWRM_NA_SIGNATURE;
	}
	softc->vnic_info.filter_id = -1;
	softc->vnic_info.id = (uint16_t)HWRM_NA_SIGNATURE;
	softc->vnic_info.rss_id = (uint16_t)HWRM_NA_SIGNATURE;
	memset(softc->vnic_info.rss_grp_tbl.idi_vaddr, 0xff,
	    softc->vnic_info.rss_grp_tbl.idi_size);
}

static void
bnxt_mark_cpr_invalid(struct bnxt_cp_ring *cpr)
{
	struct cmpl_base *cmp = (void *)cpr->ring.vaddr;
	int i;

	for (i = 0; i < cpr->ring.ring_size; i++)
		cmp[i].info3_v = !cpr->v_bit;
}

static void
bnxt_handle_async_event(struct bnxt_softc *softc, struct cmpl_base *cmpl)
{
	struct hwrm_async_event_cmpl *ae = (void *)cmpl;
	uint16_t async_id = le16toh(ae->event_id);
	struct ifmediareq ifmr;

	switch (async_id) {
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_LINK_STATUS_CHANGE:
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_LINK_SPEED_CHANGE:
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_LINK_SPEED_CFG_CHANGE:
		if (BNXT_CHIP_P5(softc))
			bit_set(softc->state_bv, BNXT_STATE_LINK_CHANGE);
		else
			bnxt_media_status(softc->ctx, &ifmr);
		break;
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_LINK_MTU_CHANGE:
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_DCB_CONFIG_CHANGE:
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_PORT_CONN_NOT_ALLOWED:
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_LINK_SPEED_CFG_NOT_ALLOWED:
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_FUNC_DRVR_UNLOAD:
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_FUNC_DRVR_LOAD:
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_PF_DRVR_UNLOAD:
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_PF_DRVR_LOAD:
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_VF_FLR:
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_VF_MAC_ADDR_CHANGE:
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_PF_VF_COMM_STATUS_CHANGE:
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_VF_CFG_CHANGE:
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_HWRM_ERROR:
		device_printf(softc->dev,
		    "Unhandled async completion type %u\n", async_id);
		break;
	default:
		device_printf(softc->dev,
		    "Unknown async completion type %u\n", async_id);
		break;
	}
}

static void
bnxt_def_cp_task(void *context)
{
	if_ctx_t ctx = context;
	struct bnxt_softc *softc = iflib_get_softc(ctx);
	struct bnxt_cp_ring *cpr = &softc->def_cp_ring;

	/* Handle completions on the default completion ring */
	struct cmpl_base *cmpl;
	uint32_t cons = cpr->cons;
	bool v_bit = cpr->v_bit;
	bool last_v_bit;
	uint32_t last_cons;
	uint16_t type;

	for (;;) {
		last_cons = cons;
		last_v_bit = v_bit;
		NEXT_CP_CONS_V(&cpr->ring, cons, v_bit);
		cmpl = &((struct cmpl_base *)cpr->ring.vaddr)[cons];

		if (!CMP_VALID(cmpl, v_bit))
			break;

		type = le16toh(cmpl->type) & CMPL_BASE_TYPE_MASK;
		switch (type) {
		case CMPL_BASE_TYPE_HWRM_ASYNC_EVENT:
			bnxt_handle_async_event(softc, cmpl);
			break;
		case CMPL_BASE_TYPE_TX_L2:
		case CMPL_BASE_TYPE_RX_L2:
		case CMPL_BASE_TYPE_RX_AGG:
		case CMPL_BASE_TYPE_RX_TPA_START:
		case CMPL_BASE_TYPE_RX_TPA_END:
		case CMPL_BASE_TYPE_STAT_EJECT:
		case CMPL_BASE_TYPE_HWRM_DONE:
		case CMPL_BASE_TYPE_HWRM_FWD_REQ:
		case CMPL_BASE_TYPE_HWRM_FWD_RESP:
		case CMPL_BASE_TYPE_CQ_NOTIFICATION:
		case CMPL_BASE_TYPE_SRQ_EVENT:
		case CMPL_BASE_TYPE_DBQ_EVENT:
		case CMPL_BASE_TYPE_QP_EVENT:
		case CMPL_BASE_TYPE_FUNC_EVENT:
			device_printf(softc->dev,
			    "Unhandled completion type %u\n", type);
			break;
		default:
			device_printf(softc->dev,
			    "Unknown completion type %u\n", type);
			break;
		}
	}

	cpr->cons = last_cons;
	cpr->v_bit = last_v_bit;
	softc->db_ops.bnxt_db_rx_cq(cpr, 1);
}

static uint8_t
get_phy_type(struct bnxt_softc *softc)
{
	struct bnxt_link_info *link_info = &softc->link_info;
	uint8_t phy_type = link_info->phy_type;
	uint16_t supported;

	if (phy_type != HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_UNKNOWN)
		return phy_type;

	/* Deduce the phy type from the media type and supported speeds */
	supported = link_info->support_speeds;

	if (link_info->media_type ==
	    HWRM_PORT_PHY_QCFG_OUTPUT_MEDIA_TYPE_TP)
		return HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_BASET;
	if (link_info->media_type ==
	    HWRM_PORT_PHY_QCFG_OUTPUT_MEDIA_TYPE_DAC) {
		if (supported & HWRM_PORT_PHY_QCFG_OUTPUT_SUPPORT_SPEEDS_2_5GB)
			return HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_BASEKX;
		if (supported & HWRM_PORT_PHY_QCFG_OUTPUT_SUPPORT_SPEEDS_20GB)
			return HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_BASEKR;
		return HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_BASECR;
	}
	if (link_info->media_type ==
	    HWRM_PORT_PHY_QCFG_OUTPUT_MEDIA_TYPE_FIBRE)
		return HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_BASESR;

	return phy_type;
}

bool
bnxt_check_hwrm_version(struct bnxt_softc *softc)
{
	char buf[16];

	sprintf(buf, "%hhu.%hhu.%hhu", softc->ver_info->hwrm_min_major,
	    softc->ver_info->hwrm_min_minor, softc->ver_info->hwrm_min_update);
	if (softc->ver_info->hwrm_min_major > softc->ver_info->hwrm_if_major) {
		device_printf(softc->dev,
		    "WARNING: HWRM version %s is too old (older than %s)\n",
		    softc->ver_info->hwrm_if_ver, buf);
		return false;
	}
	else if(softc->ver_info->hwrm_min_major ==
	    softc->ver_info->hwrm_if_major) {
		if (softc->ver_info->hwrm_min_minor >
		    softc->ver_info->hwrm_if_minor) {
			device_printf(softc->dev,
			    "WARNING: HWRM version %s is too old (older than %s)\n",
			    softc->ver_info->hwrm_if_ver, buf);
			return false;
		}
		else if (softc->ver_info->hwrm_min_minor ==
		    softc->ver_info->hwrm_if_minor) {
			if (softc->ver_info->hwrm_min_update >
			    softc->ver_info->hwrm_if_update) {
				device_printf(softc->dev,
				    "WARNING: HWRM version %s is too old (older than %s)\n",
				    softc->ver_info->hwrm_if_ver, buf);
				return false;
			}
		}
	}
	return true;
}

static uint64_t
bnxt_get_baudrate(struct bnxt_link_info *link)
{
	switch (link->link_speed) {
	case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_100MB:
		return IF_Mbps(100);
	case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_1GB:
		return IF_Gbps(1);
	case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_2GB:
		return IF_Gbps(2);
	case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_2_5GB:
		return IF_Mbps(2500);
	case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_10GB:
		return IF_Gbps(10);
	case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_20GB:
		return IF_Gbps(20);
	case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_25GB:
		return IF_Gbps(25);
	case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_40GB:
		return IF_Gbps(40);
	case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_50GB:
		return IF_Gbps(50);
	case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_100GB:
		return IF_Gbps(100);
	case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_10MB:
		return IF_Mbps(10);
	}
	return IF_Gbps(100);
}

static void
bnxt_get_wol_settings(struct bnxt_softc *softc)
{
	uint16_t wol_handle = 0;

	if (!bnxt_wol_supported(softc))
		return;

	do {
		wol_handle = bnxt_hwrm_get_wol_fltrs(softc, wol_handle);
	} while (wol_handle && wol_handle != BNXT_NO_MORE_WOL_FILTERS);
}
