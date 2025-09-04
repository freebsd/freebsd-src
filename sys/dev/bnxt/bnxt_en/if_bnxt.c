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

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_var.h>
#include <net/ethernet.h>
#include <net/iflib.h>

#define	WANT_NATIVE_PCI_GET_SLOT
#include <linux/pci.h>
#include <linux/kmod.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/idr.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/rcupdate.h>
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
#include "bnxt_ulp.h"
#include "bnxt_auxbus_compat.h"

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
    PVID(BROADCOM_VENDOR_ID, BCM57504_NPAR,
	"Broadcom BCM57504 NetXtreme-E Ethernet Partition"),
    PVID(BROADCOM_VENDOR_ID, BCM57502,
	"Broadcom BCM57502 NetXtreme-E 10Gb/25Gb/50Gb/100Gb/200Gb Ethernet"),
    PVID(BROADCOM_VENDOR_ID, BCM57608,
	"Broadcom BCM57608 NetXtreme-E 25Gb/50Gb/100Gb/200Gb/400Gb Ethernet"),
    PVID(BROADCOM_VENDOR_ID, BCM57604,
	"Broadcom BCM57604 NetXtreme-E 25Gb/50Gb/100Gb/200Gb Ethernet"),
    PVID(BROADCOM_VENDOR_ID, BCM57602,
	"Broadcom BCM57602 NetXtreme-E 25Gb/50Gb Ethernet"),
    PVID(BROADCOM_VENDOR_ID, BCM57601,
	"Broadcom BCM57601 NetXtreme-E 25Gb/50Gb Ethernet"),
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

void
process_nq(struct bnxt_softc *softc, uint16_t nqid);
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
static void bnxt_def_cp_task(void *context, int pending);
static void bnxt_handle_async_event(struct bnxt_softc *softc,
    struct cmpl_base *cmpl);
static uint64_t bnxt_get_baudrate(struct bnxt_link_info *link);
static void bnxt_get_wol_settings(struct bnxt_softc *softc);
static int bnxt_wol_config(if_ctx_t ctx);
static bool bnxt_if_needs_restart(if_ctx_t, enum iflib_restart_event);
static int bnxt_i2c_req(if_ctx_t ctx, struct ifi2creq *i2c);
static void bnxt_get_port_module_status(struct bnxt_softc *softc);
static void bnxt_rdma_aux_device_init(struct bnxt_softc *softc);
static void bnxt_rdma_aux_device_uninit(struct bnxt_softc *softc);
static void bnxt_queue_fw_reset_work(struct bnxt_softc *bp, unsigned long delay);
void bnxt_queue_sp_work(struct bnxt_softc *bp);

void bnxt_fw_reset(struct bnxt_softc *bp);
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

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DEPEND(if_bnxt, pci, 1, 1, 1);
MODULE_DEPEND(if_bnxt, ether, 1, 1, 1);
MODULE_DEPEND(if_bnxt, iflib, 1, 1, 1);
MODULE_DEPEND(if_bnxt, linuxkpi, 1, 1, 1);
MODULE_VERSION(if_bnxt, 1);

IFLIB_PNP_INFO(pci, bnxt, bnxt_vendor_info_array);

void writel_fbsd(struct bnxt_softc *bp, u32, u8, u32);
u32 readl_fbsd(struct bnxt_softc *bp, u32, u8);

u32 readl_fbsd(struct bnxt_softc *bp, u32 reg_off, u8 bar_idx)
{

	if (!bar_idx)
		return bus_space_read_4(bp->doorbell_bar.tag, bp->doorbell_bar.handle, reg_off);
	else
		return bus_space_read_4(bp->hwrm_bar.tag, bp->hwrm_bar.handle, reg_off);
}

void writel_fbsd(struct bnxt_softc *bp, u32 reg_off, u8 bar_idx, u32 val)
{

	if (!bar_idx)
		bus_space_write_4(bp->doorbell_bar.tag, bp->doorbell_bar.handle, reg_off, htole32(val));
	else
		bus_space_write_4(bp->hwrm_bar.tag, bp->hwrm_bar.handle, reg_off, htole32(val));
}

static DEFINE_IDA(bnxt_aux_dev_ids);

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
	DEVMETHOD(ifdi_i2c_req, bnxt_i2c_req),

	DEVMETHOD(ifdi_needs_restart, bnxt_if_needs_restart),

	DEVMETHOD_END
};

static driver_t bnxt_iflib_driver = {
	"bnxt", bnxt_iflib_methods, sizeof(struct bnxt_softc)
};

/*
 * iflib shared context
 */

#define BNXT_DRIVER_VERSION	"230.0.133.0"
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
	    /* NQ depth 4096 */
	    PAGE_SIZE / sizeof(struct cmpl_base) * 16},
	.isc_ntxd_max = {BNXT_MAX_TXD, BNXT_MAX_TXD, BNXT_MAX_TXD},

	.isc_admin_intrcnt = BNXT_ROCE_IRQ_COUNT,
	.isc_vendor_info = bnxt_vendor_info_array,
	.isc_driver_version = bnxt_driver_version,
};

#define PCI_SUBSYSTEM_ID	0x2e
static struct workqueue_struct *bnxt_pf_wq;

extern void bnxt_destroy_irq(struct bnxt_softc *softc);

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


static void
bnxt_set_db_mask(struct bnxt_softc *bp, struct bnxt_ring *db,
		 u32 ring_type)
{
	if (BNXT_CHIP_P7(bp)) {
		db->db_epoch_mask = db->db_ring_mask + 1;
		db->db_epoch_shift = DBR_EPOCH_SFT - ilog2(db->db_epoch_mask);

	}
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

	if (BNXT_CHIP_P5_PLUS(softc)) {
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
		softc->tx_cp_rings[i].ring.doorbell = (BNXT_CHIP_P5_PLUS(softc)) ?
			softc->legacy_db_size: softc->tx_cp_rings[i].ring.id * 0x80;
		softc->tx_cp_rings[i].ring.ring_size =
		    softc->scctx->isc_ntxd[0];
		softc->tx_cp_rings[i].ring.db_ring_mask =
		    softc->tx_cp_rings[i].ring.ring_size - 1;
		softc->tx_cp_rings[i].ring.vaddr = vaddrs[i * ntxqs];
		softc->tx_cp_rings[i].ring.paddr = paddrs[i * ntxqs];


		/* Set up the TX ring */
		softc->tx_rings[i].phys_id = (uint16_t)HWRM_NA_SIGNATURE;
		softc->tx_rings[i].softc = softc;
		softc->tx_rings[i].idx = i;
		softc->tx_rings[i].id =
		    (softc->scctx->isc_nrxqsets * 2) + 1 + i;
		softc->tx_rings[i].doorbell = (BNXT_CHIP_P5_PLUS(softc)) ?
			softc->legacy_db_size : softc->tx_rings[i].id * 0x80;
		softc->tx_rings[i].ring_size = softc->scctx->isc_ntxd[1];
		softc->tx_rings[i].db_ring_mask = softc->tx_rings[i].ring_size - 1;
		softc->tx_rings[i].vaddr = vaddrs[i * ntxqs + 1];
		softc->tx_rings[i].paddr = paddrs[i * ntxqs + 1];

		bnxt_create_tx_sysctls(softc, i);

		if (BNXT_CHIP_P5_PLUS(softc)) {
			/* Set up the Notification ring (NQ) */
			softc->nq_rings[i].stats_ctx_id = HWRM_NA_SIGNATURE;
			softc->nq_rings[i].ring.phys_id =
				(uint16_t)HWRM_NA_SIGNATURE;
			softc->nq_rings[i].ring.softc = softc;
			softc->nq_rings[i].ring.idx = i;
			softc->nq_rings[i].ring.id = i;
			softc->nq_rings[i].ring.doorbell = (BNXT_CHIP_P5_PLUS(softc)) ?
				softc->legacy_db_size : softc->nq_rings[i].ring.id * 0x80;
			softc->nq_rings[i].ring.ring_size = softc->scctx->isc_ntxd[2];
			softc->nq_rings[i].ring.db_ring_mask = softc->nq_rings[i].ring.ring_size - 1;
			softc->nq_rings[i].ring.vaddr = vaddrs[i * ntxqs + 2];
			softc->nq_rings[i].ring.paddr = paddrs[i * ntxqs + 2];
			softc->nq_rings[i].type = Q_TYPE_TX;
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
	iflib_dma_free(&softc->hw_tx_port_stats_ext);
	iflib_dma_free(&softc->hw_rx_port_stats_ext);
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


	rc = iflib_dma_alloc(ctx, sizeof(struct rx_port_stats_ext),
		&softc->hw_rx_port_stats_ext, 0);
	if (rc)
		goto hw_port_rx_stats_ext_alloc_fail;

	bus_dmamap_sync(softc->hw_rx_port_stats_ext.idi_tag,
	    softc->hw_rx_port_stats_ext.idi_map, BUS_DMASYNC_PREREAD);

	rc = iflib_dma_alloc(ctx, sizeof(struct tx_port_stats_ext),
		&softc->hw_tx_port_stats_ext, 0);
	if (rc)
		goto hw_port_tx_stats_ext_alloc_fail;

	bus_dmamap_sync(softc->hw_tx_port_stats_ext.idi_tag,
	    softc->hw_tx_port_stats_ext.idi_map, BUS_DMASYNC_PREREAD);

	softc->rx_port_stats_ext = (void *) softc->hw_rx_port_stats_ext.idi_vaddr;
	softc->tx_port_stats_ext = (void *) softc->hw_tx_port_stats_ext.idi_vaddr;

	for (i = 0; i < nrxqsets; i++) {
		/* Allocation the completion ring */
		softc->rx_cp_rings[i].stats_ctx_id = HWRM_NA_SIGNATURE;
		softc->rx_cp_rings[i].ring.phys_id =
		    (uint16_t)HWRM_NA_SIGNATURE;
		softc->rx_cp_rings[i].ring.softc = softc;
		softc->rx_cp_rings[i].ring.idx = i;
		softc->rx_cp_rings[i].ring.id = i + 1;
		softc->rx_cp_rings[i].ring.doorbell = (BNXT_CHIP_P5_PLUS(softc)) ?
			softc->legacy_db_size : softc->rx_cp_rings[i].ring.id * 0x80;
		/*
		 * If this ring overflows, RX stops working.
		 */
		softc->rx_cp_rings[i].ring.ring_size =
		    softc->scctx->isc_nrxd[0];
		softc->rx_cp_rings[i].ring.db_ring_mask =
		    softc->rx_cp_rings[i].ring.ring_size - 1;

		softc->rx_cp_rings[i].ring.vaddr = vaddrs[i * nrxqs];
		softc->rx_cp_rings[i].ring.paddr = paddrs[i * nrxqs];

		/* Allocate the RX ring */
		softc->rx_rings[i].phys_id = (uint16_t)HWRM_NA_SIGNATURE;
		softc->rx_rings[i].softc = softc;
		softc->rx_rings[i].idx = i;
		softc->rx_rings[i].id = i + 1;
		softc->rx_rings[i].doorbell = (BNXT_CHIP_P5_PLUS(softc)) ?
			softc->legacy_db_size : softc->rx_rings[i].id * 0x80;
		softc->rx_rings[i].ring_size = softc->scctx->isc_nrxd[1];
		softc->rx_rings[i].db_ring_mask =
			softc->rx_rings[i].ring_size -1;
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
		softc->ag_rings[i].doorbell = (BNXT_CHIP_P5_PLUS(softc)) ?
			softc->legacy_db_size : softc->ag_rings[i].id * 0x80;
		softc->ag_rings[i].ring_size = softc->scctx->isc_nrxd[2];
		softc->ag_rings[i].db_ring_mask = softc->ag_rings[i].ring_size - 1;
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
mc_list_alloc_fail:
	for (i = i - 1; i >= 0; i--) {
		if (softc->rx_rings[i].tpa_start)
			free(softc->rx_rings[i].tpa_start, M_DEVBUF);
	}
tpa_alloc_fail:
	iflib_dma_free(&softc->hw_tx_port_stats_ext);
hw_port_tx_stats_ext_alloc_fail:
	iflib_dma_free(&softc->hw_rx_port_stats_ext);
hw_port_rx_stats_ext_alloc_fail:
	iflib_dma_free(&softc->hw_tx_port_stats);
hw_port_tx_stats_alloc_fail:
	iflib_dma_free(&softc->hw_rx_port_stats);
hw_port_rx_stats_alloc_fail:
	for (i=0; i < nrxqsets; i++) {
		if (softc->rx_stats[i].idi_vaddr)
			iflib_dma_free(&softc->rx_stats[i]);
	}
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

static void bnxt_free_ring(struct bnxt_softc *softc, struct bnxt_ring_mem_info *rmem)
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

static void bnxt_init_ctx_mem(struct bnxt_ctx_mem_type *ctxm, void *p, int len)
{
	u8 init_val = ctxm->init_value;
	u16 offset = ctxm->init_offset;
	u8 *p2 = p;
	int i;

	if (!init_val)
		return;
	if (offset == BNXT_CTX_INIT_INVALID_OFFSET) {
		memset(p, init_val, len);
		return;
	}
	for (i = 0; i < len; i += ctxm->entry_size)
		*(p2 + i + offset) = init_val;
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

		if (rmem->ctx_mem)
			bnxt_init_ctx_mem(rmem->ctx_mem, rmem->pg_arr[i].idi_vaddr,
					rmem->page_size);

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


#define HWRM_FUNC_BACKING_STORE_CFG_INPUT_DFLT_ENABLES		\
	(HWRM_FUNC_BACKING_STORE_CFG_INPUT_ENABLES_QP |		\
	 HWRM_FUNC_BACKING_STORE_CFG_INPUT_ENABLES_SRQ |	\
	 HWRM_FUNC_BACKING_STORE_CFG_INPUT_ENABLES_CQ |		\
	 HWRM_FUNC_BACKING_STORE_CFG_INPUT_ENABLES_VNIC |	\
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
				  struct bnxt_ctx_pg_info *ctx_pg, u32 mem_size,
				  u8 depth, struct bnxt_ctx_mem_type *ctxm)
{
	struct bnxt_ring_mem_info *rmem = &ctx_pg->ring_mem;
	int rc;

	if (!mem_size)
		return -EINVAL;

	ctx_pg->nr_pages = DIV_ROUND_UP(mem_size, BNXT_PAGE_SIZE);
	if (ctx_pg->nr_pages > MAX_CTX_TOTAL_PAGES) {
		ctx_pg->nr_pages = 0;
		return -EINVAL;
	}
	if (ctx_pg->nr_pages > MAX_CTX_PAGES || depth > 1) {
		int nr_tbls, i;

		rmem->depth = 2;
		ctx_pg->ctx_pg_tbl = kzalloc(MAX_CTX_PAGES * sizeof(ctx_pg),
					      GFP_KERNEL);
		if (!ctx_pg->ctx_pg_tbl)
			return -ENOMEM;
		nr_tbls = DIV_ROUND_UP(ctx_pg->nr_pages, MAX_CTX_PAGES);
		rmem->nr_pages = nr_tbls;
		rc = bnxt_alloc_ctx_mem_blk(softc, ctx_pg);
		if (rc)
			return rc;
		for (i = 0; i < nr_tbls; i++) {
			struct bnxt_ctx_pg_info *pg_tbl;

			pg_tbl = kzalloc(sizeof(*pg_tbl), GFP_KERNEL);
			if (!pg_tbl)
				return -ENOMEM;
			ctx_pg->ctx_pg_tbl[i] = pg_tbl;
			rmem = &pg_tbl->ring_mem;
			memcpy(&rmem->pg_tbl, &ctx_pg->ctx_arr[i], sizeof(struct iflib_dma_info));
			rmem->depth = 1;
			rmem->nr_pages = MAX_CTX_PAGES;
			rmem->ctx_mem = ctxm;
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
		rmem->ctx_mem = ctxm;
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
		kfree(ctx_pg->ctx_pg_tbl);
		ctx_pg->ctx_pg_tbl = NULL;
	}
	bnxt_free_ring(softc, rmem);
	ctx_pg->nr_pages = 0;
}

static int bnxt_setup_ctxm_pg_tbls(struct bnxt_softc *softc,
				   struct bnxt_ctx_mem_type *ctxm, u32 entries,
				   u8 pg_lvl)
{
	struct bnxt_ctx_pg_info *ctx_pg = ctxm->pg_info;
	int i, rc = 0, n = 1;
	u32 mem_size;

	if (!ctxm->entry_size || !ctx_pg)
		return -EINVAL;
	if (ctxm->instance_bmap)
		n = hweight32(ctxm->instance_bmap);
	if (ctxm->entry_multiple)
		entries = roundup(entries, ctxm->entry_multiple);
	entries = clamp_t(u32, entries, ctxm->min_entries, ctxm->max_entries);
	mem_size = entries * ctxm->entry_size;
	for (i = 0; i < n && !rc; i++) {
		ctx_pg[i].entries = entries;
		rc = bnxt_alloc_ctx_pg_tbls(softc, &ctx_pg[i], mem_size, pg_lvl,
					    ctxm->init_value ? ctxm : NULL);
	}
	if (!rc)
		ctxm->mem_valid = 1;
	return rc;
}

static void bnxt_free_ctx_mem(struct bnxt_softc *softc)
{
	struct bnxt_ctx_mem_info *ctx = softc->ctx_mem;
	u16 type;

	if (!ctx)
		return;

	for (type = 0; type < BNXT_CTX_MAX; type++) {
		struct bnxt_ctx_mem_type *ctxm = &ctx->ctx_arr[type];
		struct bnxt_ctx_pg_info *ctx_pg = ctxm->pg_info;
		int i, n = 1;

		if (!ctx_pg)
			continue;
		if (ctxm->instance_bmap)
			n = hweight32(ctxm->instance_bmap);
		for (i = 0; i < n; i++)
			bnxt_free_ctx_pg_tbls(softc, &ctx_pg[i]);

		kfree(ctx_pg);
		ctxm->pg_info = NULL;
	}

	ctx->flags &= ~BNXT_CTX_FLAG_INITED;
	kfree(ctx);
	softc->ctx_mem = NULL;
}

static int
bnxt_backing_store_cfg_v2(struct bnxt_softc *softc, u32 ena)
{
	struct bnxt_ctx_mem_info *ctx = softc->ctx_mem;
	struct bnxt_ctx_mem_type *ctxm;
	u16 last_type = BNXT_CTX_INV;
	int rc = 0;
	u16 type;

	if (BNXT_PF(softc)) {
		for (type = BNXT_CTX_SRT_TRACE; type <= BNXT_CTX_ROCE_HWRM_TRACE; type++) {
			ctxm = &ctx->ctx_arr[type];
			if (!(ctxm->flags & BNXT_CTX_MEM_TYPE_VALID))
				continue;
			rc = bnxt_setup_ctxm_pg_tbls(softc, ctxm, ctxm->max_entries, 1);
			if (rc) {
				device_printf(softc->dev, "Unable to setup ctx page for type:0x%x.\n", type);
				rc = 0;
				continue;
			}
			/* ckp TODO: this is trace buffer related stuff, so keeping it diabled now. needs revisit */
			//bnxt_bs_trace_init(bp, ctxm, type - BNXT_CTX_SRT_TRACE);
			last_type = type;
		}
	}

	if (last_type == BNXT_CTX_INV) {
		if (!ena)
			return 0;
		else if (ena & HWRM_FUNC_BACKING_STORE_CFG_INPUT_ENABLES_TIM)
			last_type = BNXT_CTX_MAX - 1;
		else
			last_type = BNXT_CTX_L2_MAX - 1;
	}
	ctx->ctx_arr[last_type].last = 1;

	for (type = 0 ; type < BNXT_CTX_V2_MAX; type++) {
		ctxm = &ctx->ctx_arr[type];

		if (!ctxm->mem_valid)
			continue;
		rc = bnxt_hwrm_func_backing_store_cfg_v2(softc, ctxm, ctxm->last);
		if (rc)
			return rc;
	}
	return 0;
}

static int bnxt_alloc_ctx_mem(struct bnxt_softc *softc)
{
	struct bnxt_ctx_pg_info *ctx_pg;
	struct bnxt_ctx_mem_type *ctxm;
	struct bnxt_ctx_mem_info *ctx;
	u32 l2_qps, qp1_qps, max_qps;
	u32 ena, entries_sp, entries;
	u32 srqs, max_srqs, min;
	u32 num_mr, num_ah;
	u32 extra_srqs = 0;
	u32 extra_qps = 0;
	u8 pg_lvl = 1;
	int i, rc;

	if (!BNXT_CHIP_P5_PLUS(softc))
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

	ena = 0;
	if (BNXT_VF(softc))
		goto skip_legacy;

	ctxm = &ctx->ctx_arr[BNXT_CTX_QP];
	l2_qps = ctxm->qp_l2_entries;
	qp1_qps = ctxm->qp_qp1_entries;
	max_qps = ctxm->max_entries;
	ctxm = &ctx->ctx_arr[BNXT_CTX_SRQ];
	srqs = ctxm->srq_l2_entries;
	max_srqs = ctxm->max_entries;
	if (softc->flags & BNXT_FLAG_ROCE_CAP) {
		pg_lvl = 2;
		extra_qps = min_t(u32, 65536, max_qps - l2_qps - qp1_qps);
		extra_srqs = min_t(u32, 8192, max_srqs - srqs);
	}

	ctxm = &ctx->ctx_arr[BNXT_CTX_QP];
	rc = bnxt_setup_ctxm_pg_tbls(softc, ctxm, l2_qps + qp1_qps + extra_qps,
				     pg_lvl);
	if (rc)
		return rc;

	ctxm = &ctx->ctx_arr[BNXT_CTX_SRQ];
	rc = bnxt_setup_ctxm_pg_tbls(softc, ctxm, srqs + extra_srqs, pg_lvl);
	if (rc)
		return rc;

	ctxm = &ctx->ctx_arr[BNXT_CTX_CQ];
	rc = bnxt_setup_ctxm_pg_tbls(softc, ctxm, ctxm->cq_l2_entries +
				     extra_qps * 2, pg_lvl);
	if (rc)
		return rc;

	ctxm = &ctx->ctx_arr[BNXT_CTX_VNIC];
	rc = bnxt_setup_ctxm_pg_tbls(softc, ctxm, ctxm->max_entries, 1);
	if (rc)
		return rc;

	ctxm = &ctx->ctx_arr[BNXT_CTX_STAT];
	rc = bnxt_setup_ctxm_pg_tbls(softc, ctxm, ctxm->max_entries, 1);
	if (rc)
		return rc;

	if (!(softc->flags & BNXT_FLAG_ROCE_CAP))
		goto skip_rdma;

	ctxm = &ctx->ctx_arr[BNXT_CTX_MRAV];
	ctx_pg = ctxm->pg_info;
	/* 128K extra is needed to accomodate static AH context
	 * allocation by f/w.
	 */
	num_mr = min_t(u32, ctxm->max_entries / 2, 1024 * 256);
	num_ah = min_t(u32, num_mr, 1024 * 128);
	rc = bnxt_setup_ctxm_pg_tbls(softc, ctxm, num_mr + num_ah, 2);
	if (rc)
		return rc;
	ctx_pg->entries = num_mr + num_ah;
	ena = HWRM_FUNC_BACKING_STORE_CFG_INPUT_ENABLES_MRAV;
	if (ctxm->mrav_num_entries_units)
		ctx_pg->entries =
			((num_mr / ctxm->mrav_num_entries_units) << 16) |
			 (num_ah / ctxm->mrav_num_entries_units);

	ctxm = &ctx->ctx_arr[BNXT_CTX_TIM];
	rc = bnxt_setup_ctxm_pg_tbls(softc, ctxm, l2_qps + qp1_qps + extra_qps, 1);
	if (rc)
		return rc;
	ena |= HWRM_FUNC_BACKING_STORE_CFG_INPUT_ENABLES_TIM;

skip_rdma:
	ctxm = &ctx->ctx_arr[BNXT_CTX_STQM];
	min = ctxm->min_entries;
	entries_sp = ctx->ctx_arr[BNXT_CTX_VNIC].vnic_entries + l2_qps +
		     2 * (extra_qps + qp1_qps) + min;
	rc = bnxt_setup_ctxm_pg_tbls(softc, ctxm, entries_sp, 2);
		if (rc)
			return rc;

	ctxm = &ctx->ctx_arr[BNXT_CTX_FTQM];
	entries = l2_qps + 2 * (extra_qps + qp1_qps);
	rc = bnxt_setup_ctxm_pg_tbls(softc, ctxm, entries, 2);
	if (rc)
		return rc;
	for (i = 0; i < ctx->tqm_fp_rings_count + 1; i++) {
		if (i < BNXT_MAX_TQM_LEGACY_RINGS)
			ena |= HWRM_FUNC_BACKING_STORE_CFG_INPUT_ENABLES_TQM_SP << i;
		else
			ena |= HWRM_FUNC_BACKING_STORE_CFG_INPUT_ENABLES_TQM_RING8;
	}
	ena |= HWRM_FUNC_BACKING_STORE_CFG_INPUT_DFLT_ENABLES;

skip_legacy:
	if (BNXT_CHIP_P7(softc)) {
		if (softc->fw_cap & BNXT_FW_CAP_BACKING_STORE_V2)
			rc = bnxt_backing_store_cfg_v2(softc, ena);
	} else {
		rc = bnxt_hwrm_func_backing_store_cfg(softc, ena);
	}
	if (rc) {
		device_printf(softc->dev, "Failed configuring context mem, rc = %d.\n",
			      rc);
		return rc;
	}
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

static void
bnxt_thor2_db_rx(void *db_ptr, uint16_t idx)
{
	struct bnxt_ring *ring = (struct bnxt_ring *) db_ptr;
	struct bnxt_bar_info *db_bar = &ring->softc->doorbell_bar;
	uint64_t db_val;

	if (idx >= ring->ring_size) {
		device_printf(ring->softc->dev, "%s: BRCM DBG: idx: %d crossed boundary\n", __func__, idx);
		return;
	}

	db_val = ((DBR_PATH_L2 | DBR_TYPE_SRQ | DBR_VALID | idx) |
				((uint64_t)ring->phys_id << DBR_XID_SFT));

	/* Add the PI index */
	db_val |= DB_RING_IDX(ring, idx, ring->epoch_arr[idx]);

	bus_space_barrier(db_bar->tag, db_bar->handle, ring->doorbell, 8,
			BUS_SPACE_BARRIER_WRITE);
	bus_space_write_8(db_bar->tag, db_bar->handle, ring->doorbell,
			htole64(db_val));
}

static void
bnxt_thor2_db_tx(void *db_ptr, uint16_t idx)
{
	struct bnxt_ring *ring = (struct bnxt_ring *) db_ptr;
	struct bnxt_bar_info *db_bar = &ring->softc->doorbell_bar;
	uint64_t db_val;

	if (idx >= ring->ring_size) {
		device_printf(ring->softc->dev, "%s: BRCM DBG: idx: %d crossed boundary\n", __func__, idx);
		return;
	}

	db_val = ((DBR_PATH_L2 | DBR_TYPE_SQ | DBR_VALID | idx) |
				((uint64_t)ring->phys_id << DBR_XID_SFT));

	/* Add the PI index */
	db_val |= DB_RING_IDX(ring, idx, ring->epoch_arr[idx]);

	bus_space_barrier(db_bar->tag, db_bar->handle, ring->doorbell, 8,
			BUS_SPACE_BARRIER_WRITE);
	bus_space_write_8(db_bar->tag, db_bar->handle, ring->doorbell,
			htole64(db_val));
}

static void
bnxt_thor2_db_rx_cq(void *db_ptr, bool enable_irq)
{
	struct bnxt_cp_ring *cpr = (struct bnxt_cp_ring *) db_ptr;
	struct bnxt_bar_info *db_bar = &cpr->ring.softc->doorbell_bar;
	u64 db_msg = { 0 };
	uint32_t cons = cpr->raw_cons;
	uint32_t toggle = 0;

	if (cons == UINT32_MAX)
		cons = 0;

	if (enable_irq == true)
		toggle = cpr->toggle;

	db_msg = DBR_PATH_L2 | ((u64)cpr->ring.phys_id << DBR_XID_SFT) | DBR_VALID |
			DB_RING_IDX_CMP(&cpr->ring, cons) | DB_TOGGLE(toggle);

	if (enable_irq)
		db_msg |= DBR_TYPE_CQ_ARMALL;
	else
		db_msg |= DBR_TYPE_CQ;

	bus_space_barrier(db_bar->tag, db_bar->handle, cpr->ring.doorbell, 8,
			BUS_SPACE_BARRIER_WRITE);
	bus_space_write_8(db_bar->tag, db_bar->handle, cpr->ring.doorbell,
			htole64(*(uint64_t *)&db_msg));
	bus_space_barrier(db_bar->tag, db_bar->handle, 0, db_bar->size,
			BUS_SPACE_BARRIER_WRITE);
}

static void
bnxt_thor2_db_tx_cq(void *db_ptr, bool enable_irq)
{
	struct bnxt_cp_ring *cpr = (struct bnxt_cp_ring *) db_ptr;
	struct bnxt_bar_info *db_bar = &cpr->ring.softc->doorbell_bar;
	u64 db_msg = { 0 };
	uint32_t cons = cpr->raw_cons;
	uint32_t toggle = 0;

	if (enable_irq == true)
		toggle = cpr->toggle;

	db_msg = DBR_PATH_L2 | ((u64)cpr->ring.phys_id << DBR_XID_SFT) | DBR_VALID |
			DB_RING_IDX_CMP(&cpr->ring, cons) | DB_TOGGLE(toggle);

	if (enable_irq)
		db_msg |= DBR_TYPE_CQ_ARMALL;
	else
		db_msg |= DBR_TYPE_CQ;

	bus_space_barrier(db_bar->tag, db_bar->handle, cpr->ring.doorbell, 8,
			BUS_SPACE_BARRIER_WRITE);
	bus_space_write_8(db_bar->tag, db_bar->handle, cpr->ring.doorbell,
			htole64(*(uint64_t *)&db_msg));
	bus_space_barrier(db_bar->tag, db_bar->handle, 0, db_bar->size,
			BUS_SPACE_BARRIER_WRITE);
}

static void
bnxt_thor2_db_nq(void *db_ptr, bool enable_irq)
{
	struct bnxt_cp_ring *cpr = (struct bnxt_cp_ring *) db_ptr;
	struct bnxt_bar_info *db_bar = &cpr->ring.softc->doorbell_bar;
	u64 db_msg = { 0 };
	uint32_t cons = cpr->raw_cons;
	uint32_t toggle = 0;

	if (enable_irq == true)
		toggle = cpr->toggle;

	db_msg = DBR_PATH_L2 | ((u64)cpr->ring.phys_id << DBR_XID_SFT) | DBR_VALID |
			DB_RING_IDX_CMP(&cpr->ring, cons) | DB_TOGGLE(toggle);

	if (enable_irq)
		db_msg |= DBR_TYPE_NQ_ARM;
	else
		db_msg |= DBR_TYPE_NQ_MASK;

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


static void bnxt_verify_asym_queues(struct bnxt_softc *softc)
{
	uint8_t i, lltc = 0;

	if (!softc->max_lltc)
		return;

	/* Verify that lossless TX and RX queues are in the same index */
	for (i = 0; i < softc->max_tc; i++) {
		if (BNXT_LLQ(softc->tx_q_info[i].queue_profile) &&
		    BNXT_LLQ(softc->rx_q_info[i].queue_profile))
			lltc++;
	}
	softc->max_lltc = min(softc->max_lltc, lltc);
}

static int bnxt_hwrm_poll(struct bnxt_softc *bp)
{
	struct hwrm_ver_get_output	*resp =
	    (void *)bp->hwrm_cmd_resp.idi_vaddr;
	struct hwrm_ver_get_input req = {0};
	int rc;

	bnxt_hwrm_cmd_hdr_init(bp, &req, HWRM_VER_GET);

	req.hwrm_intf_maj = HWRM_VERSION_MAJOR;
	req.hwrm_intf_min = HWRM_VERSION_MINOR;
	req.hwrm_intf_upd = HWRM_VERSION_UPDATE;

	rc = _hwrm_send_message(bp, &req, sizeof(req));
	if (rc)
		return rc;

	if (resp->flags & HWRM_VER_GET_OUTPUT_FLAGS_DEV_NOT_RDY)
		rc = -EAGAIN;

	return rc;
}

static void bnxt_rtnl_lock_sp(struct bnxt_softc *bp)
{
	/* We are called from bnxt_sp_task which has BNXT_STATE_IN_SP_TASK
	 * set.  If the device is being closed, bnxt_close() may be holding
	 * rtnl() and waiting for BNXT_STATE_IN_SP_TASK to clear.  So we
	 * must clear BNXT_STATE_IN_SP_TASK before holding rtnl().
	 */
	clear_bit(BNXT_STATE_IN_SP_TASK, &bp->state);
	rtnl_lock();
}

static void bnxt_rtnl_unlock_sp(struct bnxt_softc *bp)
{
	set_bit(BNXT_STATE_IN_SP_TASK, &bp->state);
	rtnl_unlock();
}

static void bnxt_fw_fatal_close(struct bnxt_softc *softc)
{
	bnxt_disable_intr(softc->ctx);
	if (pci_is_enabled(softc->pdev))
		pci_disable_device(softc->pdev);
}

static u32 bnxt_fw_health_readl(struct bnxt_softc *bp, int reg_idx)
{
	struct bnxt_fw_health *fw_health = bp->fw_health;
	u32 reg = fw_health->regs[reg_idx];
	u32 reg_type, reg_off, val = 0;

	reg_type = BNXT_FW_HEALTH_REG_TYPE(reg);
	reg_off = BNXT_FW_HEALTH_REG_OFF(reg);
	switch (reg_type) {
	case BNXT_FW_HEALTH_REG_TYPE_CFG:
		pci_read_config_dword(bp->pdev, reg_off, &val);
		break;
	case BNXT_FW_HEALTH_REG_TYPE_GRC:
		reg_off = fw_health->mapped_regs[reg_idx];
		fallthrough;
	case BNXT_FW_HEALTH_REG_TYPE_BAR0:
		val = readl_fbsd(bp, reg_off, 0);
		break;
	case BNXT_FW_HEALTH_REG_TYPE_BAR1:
		val = readl_fbsd(bp, reg_off, 2);
		break;
	}
	if (reg_idx == BNXT_FW_RESET_INPROG_REG)
		val &= fw_health->fw_reset_inprog_reg_mask;
	return val;
}

static void bnxt_fw_reset_close(struct bnxt_softc *bp)
{
	int i;
	bnxt_ulp_stop(bp);
	/* When firmware is in fatal state, quiesce device and disable
	 * bus master to prevent any potential bad DMAs before freeing
	 * kernel memory.
	 */
	if (test_bit(BNXT_STATE_FW_FATAL_COND, &bp->state)) {
		u16 val = 0;

		val = pci_read_config(bp->dev, PCI_SUBSYSTEM_ID, 2);
		if (val == 0xffff) {
			bp->fw_reset_min_dsecs = 0;
		}
		bnxt_fw_fatal_close(bp);
	}

	iflib_request_reset(bp->ctx);
	bnxt_stop(bp->ctx);
	bnxt_hwrm_func_drv_unrgtr(bp, false);

	for (i = bp->nrxqsets-1; i>=0; i--) {
		if (BNXT_CHIP_P5_PLUS(bp))
			iflib_irq_free(bp->ctx, &bp->nq_rings[i].irq);
		else
			iflib_irq_free(bp->ctx, &bp->rx_cp_rings[i].irq);

	}
	if (pci_is_enabled(bp->pdev))
		pci_disable_device(bp->pdev);
	pci_disable_busmaster(bp->dev);
	bnxt_free_ctx_mem(bp);
}

static bool is_bnxt_fw_ok(struct bnxt_softc *bp)
{
	struct bnxt_fw_health *fw_health = bp->fw_health;
	bool no_heartbeat = false, has_reset = false;
	u32 val;

	val = bnxt_fw_health_readl(bp, BNXT_FW_HEARTBEAT_REG);
	if (val == fw_health->last_fw_heartbeat)
		no_heartbeat = true;

	val = bnxt_fw_health_readl(bp, BNXT_FW_RESET_CNT_REG);
	if (val != fw_health->last_fw_reset_cnt)
		has_reset = true;

	if (!no_heartbeat && has_reset)
		return true;

	return false;
}

void bnxt_fw_reset(struct bnxt_softc *bp)
{
	bnxt_rtnl_lock_sp(bp);
	if (test_bit(BNXT_STATE_OPEN, &bp->state) &&
	    !test_bit(BNXT_STATE_IN_FW_RESET, &bp->state)) {
		int tmo;
		set_bit(BNXT_STATE_IN_FW_RESET, &bp->state);
		bnxt_fw_reset_close(bp);

		if ((bp->fw_cap & BNXT_FW_CAP_ERR_RECOVER_RELOAD)) {
			bp->fw_reset_state = BNXT_FW_RESET_STATE_POLL_FW_DOWN;
			tmo = HZ / 10;
		} else {
			bp->fw_reset_state = BNXT_FW_RESET_STATE_ENABLE_DEV;
			tmo = bp->fw_reset_min_dsecs * HZ /10;
		}
		bnxt_queue_fw_reset_work(bp, tmo);
	}
	bnxt_rtnl_unlock_sp(bp);
}

static void bnxt_queue_fw_reset_work(struct bnxt_softc *bp, unsigned long delay)
{
	if (!(test_bit(BNXT_STATE_IN_FW_RESET, &bp->state)))
		return;

	if (BNXT_PF(bp))
		queue_delayed_work(bnxt_pf_wq, &bp->fw_reset_task, delay);
	else
		schedule_delayed_work(&bp->fw_reset_task, delay);
}

void bnxt_queue_sp_work(struct bnxt_softc *bp)
{
	if (BNXT_PF(bp))
		queue_work(bnxt_pf_wq, &bp->sp_task);
	else
		schedule_work(&bp->sp_task);
}

static void bnxt_fw_reset_writel(struct bnxt_softc *bp, int reg_idx)
{
	struct bnxt_fw_health *fw_health = bp->fw_health;
	u32 reg = fw_health->fw_reset_seq_regs[reg_idx];
	u32 val = fw_health->fw_reset_seq_vals[reg_idx];
	u32 reg_type, reg_off, delay_msecs;

	delay_msecs = fw_health->fw_reset_seq_delay_msec[reg_idx];
	reg_type = BNXT_FW_HEALTH_REG_TYPE(reg);
	reg_off = BNXT_FW_HEALTH_REG_OFF(reg);
	switch (reg_type) {
	case BNXT_FW_HEALTH_REG_TYPE_CFG:
		pci_write_config_dword(bp->pdev, reg_off, val);
		break;
	case BNXT_FW_HEALTH_REG_TYPE_GRC:
		writel_fbsd(bp, BNXT_GRCPF_REG_WINDOW_BASE_OUT + 4, 0, reg_off & BNXT_GRC_BASE_MASK);
		reg_off = (reg_off & BNXT_GRC_OFFSET_MASK) + 0x2000;
		fallthrough;
	case BNXT_FW_HEALTH_REG_TYPE_BAR0:
		writel_fbsd(bp, reg_off, 0, val);
		break;
	case BNXT_FW_HEALTH_REG_TYPE_BAR1:
		writel_fbsd(bp, reg_off, 2, val);
		break;
	}
	if (delay_msecs) {
		pci_read_config_dword(bp->pdev, 0, &val);
		msleep(delay_msecs);
	}
}

static void bnxt_reset_all(struct bnxt_softc *bp)
{
	struct bnxt_fw_health *fw_health = bp->fw_health;
	int i, rc;

	if (bp->fw_cap & BNXT_FW_CAP_ERR_RECOVER_RELOAD) {
		bp->fw_reset_timestamp = jiffies;
		return;
	}

	if (fw_health->flags & HWRM_ERROR_RECOVERY_QCFG_OUTPUT_FLAGS_HOST) {
		for (i = 0; i < fw_health->fw_reset_seq_cnt; i++)
			bnxt_fw_reset_writel(bp, i);
	} else if (fw_health->flags & HWRM_ERROR_RECOVERY_QCFG_OUTPUT_FLAGS_CO_CPU) {
		struct hwrm_fw_reset_input req = {0};

		bnxt_hwrm_cmd_hdr_init(bp, &req, HWRM_FW_RESET);
		req.target_id = htole16(HWRM_TARGET_ID_KONG);
		req.embedded_proc_type = HWRM_FW_RESET_INPUT_EMBEDDED_PROC_TYPE_CHIP;
		req.selfrst_status = HWRM_FW_RESET_INPUT_SELFRST_STATUS_SELFRSTASAP;
		req.flags = HWRM_FW_RESET_INPUT_FLAGS_RESET_GRACEFUL;
		rc = hwrm_send_message(bp, &req, sizeof(req));

		if (rc != -ENODEV)
			device_printf(bp->dev, "Unable to reset FW rc=%d\n", rc);
	}
	bp->fw_reset_timestamp = jiffies;
}

static int __bnxt_alloc_fw_health(struct bnxt_softc *bp)
{
	if (bp->fw_health)
		return 0;

	bp->fw_health = kzalloc(sizeof(*bp->fw_health), GFP_KERNEL);
	if (!bp->fw_health)
		return -ENOMEM;

	mutex_init(&bp->fw_health->lock);
	return 0;
}

static int bnxt_alloc_fw_health(struct bnxt_softc *bp)
{
	int rc;

	if (!(bp->fw_cap & BNXT_FW_CAP_HOT_RESET) &&
	    !(bp->fw_cap & BNXT_FW_CAP_ERROR_RECOVERY))
		return 0;

	rc = __bnxt_alloc_fw_health(bp);
	if (rc) {
		bp->fw_cap &= ~BNXT_FW_CAP_HOT_RESET;
		bp->fw_cap &= ~BNXT_FW_CAP_ERROR_RECOVERY;
		return rc;
	}

	return 0;
}

static inline void __bnxt_map_fw_health_reg(struct bnxt_softc *bp, u32 reg)
{
	writel_fbsd(bp, BNXT_GRCPF_REG_WINDOW_BASE_OUT + BNXT_FW_HEALTH_WIN_MAP_OFF, 0, reg & BNXT_GRC_BASE_MASK);
}

static int bnxt_map_fw_health_regs(struct bnxt_softc *bp)
{
	struct bnxt_fw_health *fw_health = bp->fw_health;
	u32 reg_base = 0xffffffff;
	int i;

	bp->fw_health->status_reliable = false;
	bp->fw_health->resets_reliable = false;
	/* Only pre-map the monitoring GRC registers using window 3 */
	for (i = 0; i < 4; i++) {
		u32 reg = fw_health->regs[i];

		if (BNXT_FW_HEALTH_REG_TYPE(reg) != BNXT_FW_HEALTH_REG_TYPE_GRC)
			continue;
		if (reg_base == 0xffffffff)
			reg_base = reg & BNXT_GRC_BASE_MASK;
		if ((reg & BNXT_GRC_BASE_MASK) != reg_base)
			return -ERANGE;
		fw_health->mapped_regs[i] = BNXT_FW_HEALTH_WIN_OFF(reg);
	}
	bp->fw_health->status_reliable = true;
	bp->fw_health->resets_reliable = true;
	if (reg_base == 0xffffffff)
		return 0;

	__bnxt_map_fw_health_reg(bp, reg_base);
	return 0;
}

static void bnxt_inv_fw_health_reg(struct bnxt_softc *bp)
{
	struct bnxt_fw_health *fw_health = bp->fw_health;
	u32 reg_type;

	if (!fw_health)
		return;

	reg_type = BNXT_FW_HEALTH_REG_TYPE(fw_health->regs[BNXT_FW_HEALTH_REG]);
	if (reg_type == BNXT_FW_HEALTH_REG_TYPE_GRC)
		fw_health->status_reliable = false;

	reg_type = BNXT_FW_HEALTH_REG_TYPE(fw_health->regs[BNXT_FW_RESET_CNT_REG]);
	if (reg_type == BNXT_FW_HEALTH_REG_TYPE_GRC)
		fw_health->resets_reliable = false;
}

static int bnxt_hwrm_error_recovery_qcfg(struct bnxt_softc *bp)
{
	struct bnxt_fw_health *fw_health = bp->fw_health;
	struct hwrm_error_recovery_qcfg_output *resp =
	    (void *)bp->hwrm_cmd_resp.idi_vaddr;
	struct hwrm_error_recovery_qcfg_input req = {0};
	int rc, i;

	if (!(bp->fw_cap & BNXT_FW_CAP_ERROR_RECOVERY))
		return 0;

	bnxt_hwrm_cmd_hdr_init(bp, &req, HWRM_ERROR_RECOVERY_QCFG);
	rc = _hwrm_send_message(bp, &req, sizeof(req));

	if (rc)
		goto err_recovery_out;
	fw_health->flags = le32toh(resp->flags);
	if ((fw_health->flags & HWRM_ERROR_RECOVERY_QCFG_OUTPUT_FLAGS_CO_CPU) &&
	    !(bp->fw_cap & BNXT_FW_CAP_KONG_MB_CHNL)) {
		rc = -EINVAL;
		goto err_recovery_out;
	}
	fw_health->polling_dsecs = le32toh(resp->driver_polling_freq);
	fw_health->master_func_wait_dsecs =
		le32toh(resp->master_func_wait_period);
	fw_health->normal_func_wait_dsecs =
		le32toh(resp->normal_func_wait_period);
	fw_health->post_reset_wait_dsecs =
		le32toh(resp->master_func_wait_period_after_reset);
	fw_health->post_reset_max_wait_dsecs =
		le32toh(resp->max_bailout_time_after_reset);
	fw_health->regs[BNXT_FW_HEALTH_REG] =
		le32toh(resp->fw_health_status_reg);
	fw_health->regs[BNXT_FW_HEARTBEAT_REG] =
		le32toh(resp->fw_heartbeat_reg);
	fw_health->regs[BNXT_FW_RESET_CNT_REG] =
		le32toh(resp->fw_reset_cnt_reg);
	fw_health->regs[BNXT_FW_RESET_INPROG_REG] =
		le32toh(resp->reset_inprogress_reg);
	fw_health->fw_reset_inprog_reg_mask =
		le32toh(resp->reset_inprogress_reg_mask);
	fw_health->fw_reset_seq_cnt = resp->reg_array_cnt;
	if (fw_health->fw_reset_seq_cnt >= 16) {
		rc = -EINVAL;
		goto err_recovery_out;
	}
	for (i = 0; i < fw_health->fw_reset_seq_cnt; i++) {
		fw_health->fw_reset_seq_regs[i] =
			le32toh(resp->reset_reg[i]);
		fw_health->fw_reset_seq_vals[i] =
			le32toh(resp->reset_reg_val[i]);
		fw_health->fw_reset_seq_delay_msec[i] =
			le32toh(resp->delay_after_reset[i]);
	}
err_recovery_out:
	if (!rc)
		rc = bnxt_map_fw_health_regs(bp);
	if (rc)
		bp->fw_cap &= ~BNXT_FW_CAP_ERROR_RECOVERY;
	return rc;
}

static int bnxt_drv_rgtr(struct bnxt_softc *bp)
{
	int rc;

	/* determine whether we can support error recovery before
	 * registering with FW
	 */
	if (bnxt_alloc_fw_health(bp)) {
		device_printf(bp->dev, "no memory for firmware error recovery\n");
	} else {
		rc = bnxt_hwrm_error_recovery_qcfg(bp);
		if (rc)
			device_printf(bp->dev, "hwrm query error recovery failure rc: %d\n",
				    rc);
	}
	rc = bnxt_hwrm_func_drv_rgtr(bp, NULL, 0, false);  //sumit dbg: revisit the params
	if (rc)
		return -ENODEV;
	return 0;
}

static bool bnxt_fw_reset_timeout(struct bnxt_softc *bp)
{
	return time_after(jiffies, bp->fw_reset_timestamp +
			  (bp->fw_reset_max_dsecs * HZ / 10));
}

static int bnxt_open(struct bnxt_softc *bp)
{
	int rc = 0;
	if (BNXT_PF(bp))
		rc = bnxt_hwrm_nvm_get_dev_info(bp, &bp->nvm_info->mfg_id,
			&bp->nvm_info->device_id, &bp->nvm_info->sector_size,
			&bp->nvm_info->size, &bp->nvm_info->reserved_size,
			&bp->nvm_info->available_size);

	/* Get the queue config */
	rc = bnxt_hwrm_queue_qportcfg(bp, HWRM_QUEUE_QPORTCFG_INPUT_FLAGS_PATH_TX);
	if (rc) {
		device_printf(bp->dev, "reinit: hwrm qportcfg (tx) failed\n");
		return rc;
	}
	if (bp->is_asym_q) {
		rc = bnxt_hwrm_queue_qportcfg(bp,
					      HWRM_QUEUE_QPORTCFG_INPUT_FLAGS_PATH_RX);
		if (rc) {
			device_printf(bp->dev, "re-init: hwrm qportcfg (rx)  failed\n");
			return rc;
		}
		bnxt_verify_asym_queues(bp);
	} else {
		bp->rx_max_q = bp->tx_max_q;
		memcpy(bp->rx_q_info, bp->tx_q_info, sizeof(bp->rx_q_info));
		memcpy(bp->rx_q_ids, bp->tx_q_ids, sizeof(bp->rx_q_ids));
	}
	/* Get the HW capabilities */
	rc = bnxt_hwrm_func_qcaps(bp);
	if (rc)
		return rc;

	/* Register the driver with the FW */
	rc = bnxt_drv_rgtr(bp);
	if (rc)
		return rc;
	if (bp->hwrm_spec_code >= 0x10803) {
		rc = bnxt_alloc_ctx_mem(bp);
		if (rc) {
			device_printf(bp->dev, "attach: alloc_ctx_mem failed\n");
			return rc;
		}
		rc = bnxt_hwrm_func_resc_qcaps(bp, true);
		if (!rc)
			bp->flags |= BNXT_FLAG_FW_CAP_NEW_RM;
	}

	if (BNXT_CHIP_P5_PLUS(bp))
		bnxt_hwrm_reserve_pf_rings(bp);
	/* Get the current configuration of this function */
	rc = bnxt_hwrm_func_qcfg(bp);
	if (rc) {
		device_printf(bp->dev, "re-init: hwrm func qcfg failed\n");
		return rc;
	}

	bnxt_msix_intr_assign(bp->ctx, 0);
	bnxt_init(bp->ctx);
	bnxt_intr_enable(bp->ctx);

	if (test_and_clear_bit(BNXT_STATE_FW_RESET_DET, &bp->state)) {
		if (!test_bit(BNXT_STATE_IN_FW_RESET, &bp->state)) {
			bnxt_ulp_start(bp, 0);
		}
	}

	device_printf(bp->dev, "Network interface is UP and operational\n");

	return rc;
}
static void bnxt_fw_reset_abort(struct bnxt_softc *bp, int rc)
{
	clear_bit(BNXT_STATE_IN_FW_RESET, &bp->state);
	if (bp->fw_reset_state != BNXT_FW_RESET_STATE_POLL_VF) {
		bnxt_ulp_start(bp, rc);
	}
	bp->fw_reset_state = 0;
}

static void bnxt_fw_reset_task(struct work_struct *work)
{
	struct bnxt_softc *bp = container_of(work, struct bnxt_softc, fw_reset_task.work);
	int rc = 0;

	if (!test_bit(BNXT_STATE_IN_FW_RESET, &bp->state)) {
		device_printf(bp->dev, "bnxt_fw_reset_task() called when not in fw reset mode!\n");
		return;
	}

	switch (bp->fw_reset_state) {
	case BNXT_FW_RESET_STATE_POLL_FW_DOWN: {
		u32 val;

		val = bnxt_fw_health_readl(bp, BNXT_FW_HEALTH_REG);
		if (!(val & BNXT_FW_STATUS_SHUTDOWN) &&
		    !bnxt_fw_reset_timeout(bp)) {
			bnxt_queue_fw_reset_work(bp, HZ / 5);
			return;
		}

		if (!bp->fw_health->primary) {
			u32 wait_dsecs = bp->fw_health->normal_func_wait_dsecs;

			bp->fw_reset_state = BNXT_FW_RESET_STATE_ENABLE_DEV;
			bnxt_queue_fw_reset_work(bp, wait_dsecs * HZ / 10);
			return;
		}
		bp->fw_reset_state = BNXT_FW_RESET_STATE_RESET_FW;
	}
		fallthrough;
	case BNXT_FW_RESET_STATE_RESET_FW:
		bnxt_reset_all(bp);
		bp->fw_reset_state = BNXT_FW_RESET_STATE_ENABLE_DEV;
		bnxt_queue_fw_reset_work(bp, bp->fw_reset_min_dsecs * HZ / 10);
		return;
	case BNXT_FW_RESET_STATE_ENABLE_DEV:
		bnxt_inv_fw_health_reg(bp);
		if (test_bit(BNXT_STATE_FW_FATAL_COND, &bp->state) &&
		    !bp->fw_reset_min_dsecs) {
			u16 val;

			val = pci_read_config(bp->dev, PCI_SUBSYSTEM_ID, 2);
			if (val == 0xffff) {
				if (bnxt_fw_reset_timeout(bp)) {
					device_printf(bp->dev, "Firmware reset aborted, PCI config space invalid\n");
					rc = -ETIMEDOUT;
					goto fw_reset_abort;
				}
				bnxt_queue_fw_reset_work(bp, HZ / 1000);
				return;
			}
		}
		clear_bit(BNXT_STATE_FW_FATAL_COND, &bp->state);
		clear_bit(BNXT_STATE_FW_NON_FATAL_COND, &bp->state);
		if (!pci_is_enabled(bp->pdev)) {
			if (pci_enable_device(bp->pdev)) {
				device_printf(bp->dev, "Cannot re-enable PCI device\n");
				rc = -ENODEV;
				goto fw_reset_abort;
			}
		}
		pci_set_master(bp->pdev);
		bp->fw_reset_state = BNXT_FW_RESET_STATE_POLL_FW;
		fallthrough;
	case BNXT_FW_RESET_STATE_POLL_FW:
		bp->hwrm_cmd_timeo = SHORT_HWRM_CMD_TIMEOUT;
		rc = bnxt_hwrm_poll(bp);
		if (rc) {
			if (bnxt_fw_reset_timeout(bp)) {
				device_printf(bp->dev, "Firmware reset aborted\n");
				goto fw_reset_abort_status;
			}
			bnxt_queue_fw_reset_work(bp, HZ / 5);
			return;
		}
		bp->hwrm_cmd_timeo = DFLT_HWRM_CMD_TIMEOUT;
		bp->fw_reset_state = BNXT_FW_RESET_STATE_OPENING;
		fallthrough;
	case BNXT_FW_RESET_STATE_OPENING:
		rc = bnxt_open(bp);
		if (rc) {
			device_printf(bp->dev, "bnxt_open() failed during FW reset\n");
			bnxt_fw_reset_abort(bp, rc);
			rtnl_unlock();
			return;
		}

		if ((bp->fw_cap & BNXT_FW_CAP_ERROR_RECOVERY) &&
		    bp->fw_health->enabled) {
			bp->fw_health->last_fw_reset_cnt =
				bnxt_fw_health_readl(bp, BNXT_FW_RESET_CNT_REG);
		}
		bp->fw_reset_state = 0;
		smp_mb__before_atomic();
		clear_bit(BNXT_STATE_IN_FW_RESET, &bp->state);
		bnxt_ulp_start(bp, 0);
		clear_bit(BNXT_STATE_FW_ACTIVATE, &bp->state);
		set_bit(BNXT_STATE_OPEN, &bp->state);
		rtnl_unlock();
	}
	return;

fw_reset_abort_status:
	if (bp->fw_health->status_reliable ||
	    (bp->fw_cap & BNXT_FW_CAP_ERROR_RECOVERY)) {
		u32 sts = bnxt_fw_health_readl(bp, BNXT_FW_HEALTH_REG);

		device_printf(bp->dev, "fw_health_status 0x%x\n", sts);
	}
fw_reset_abort:
	rtnl_lock();
	bnxt_fw_reset_abort(bp, rc);
	rtnl_unlock();
}

static void bnxt_force_fw_reset(struct bnxt_softc *bp)
{
	struct bnxt_fw_health *fw_health = bp->fw_health;
	u32 wait_dsecs;

	if (!test_bit(BNXT_STATE_OPEN, &bp->state) ||
	    test_bit(BNXT_STATE_IN_FW_RESET, &bp->state))
		return;
	bnxt_fw_reset_close(bp);
	wait_dsecs = fw_health->master_func_wait_dsecs;
	if (fw_health->primary) {
		if (fw_health->flags & HWRM_ERROR_RECOVERY_QCFG_OUTPUT_FLAGS_CO_CPU)
			wait_dsecs = 0;
		bp->fw_reset_state = BNXT_FW_RESET_STATE_RESET_FW;
	} else {
		bp->fw_reset_timestamp = jiffies + wait_dsecs * HZ / 10;
		wait_dsecs = fw_health->normal_func_wait_dsecs;
		bp->fw_reset_state = BNXT_FW_RESET_STATE_ENABLE_DEV;
	}

	bp->fw_reset_min_dsecs = fw_health->post_reset_wait_dsecs;
	bp->fw_reset_max_dsecs = fw_health->post_reset_max_wait_dsecs;
	bnxt_queue_fw_reset_work(bp, wait_dsecs * HZ / 10);
}

static void bnxt_fw_exception(struct bnxt_softc *bp)
{
	device_printf(bp->dev, "Detected firmware fatal condition, initiating reset\n");
	set_bit(BNXT_STATE_FW_FATAL_COND, &bp->state);
	bnxt_rtnl_lock_sp(bp);
	bnxt_force_fw_reset(bp);
	bnxt_rtnl_unlock_sp(bp);
}

static void __bnxt_fw_recover(struct bnxt_softc *bp)
{
	if (test_bit(BNXT_STATE_FW_FATAL_COND, &bp->state) ||
	    test_bit(BNXT_STATE_FW_NON_FATAL_COND, &bp->state))
		bnxt_fw_reset(bp);
	else
		bnxt_fw_exception(bp);
}

static void bnxt_devlink_health_fw_report(struct bnxt_softc *bp)
{
	struct bnxt_fw_health *fw_health = bp->fw_health;

	if (!fw_health)
		return;

	if (!fw_health->fw_reporter) {
		__bnxt_fw_recover(bp);
		return;
	}
}

static void bnxt_sp_task(struct work_struct *work)
{
	struct bnxt_softc *bp = container_of(work, struct bnxt_softc, sp_task);

	set_bit(BNXT_STATE_IN_SP_TASK, &bp->state);
	smp_mb__after_atomic();
	if (!test_bit(BNXT_STATE_OPEN, &bp->state)) {
		clear_bit(BNXT_STATE_IN_SP_TASK, &bp->state);
		return;
	}

	if (test_and_clear_bit(BNXT_FW_RESET_NOTIFY_SP_EVENT, &bp->sp_event)) {
		if (test_bit(BNXT_STATE_FW_FATAL_COND, &bp->state) ||
		    test_bit(BNXT_STATE_FW_NON_FATAL_COND, &bp->state))
			bnxt_devlink_health_fw_report(bp);
		else
			bnxt_fw_reset(bp);
	}

	if (test_and_clear_bit(BNXT_FW_EXCEPTION_SP_EVENT, &bp->sp_event)) {
		if (!is_bnxt_fw_ok(bp))
			bnxt_devlink_health_fw_report(bp);
	}
	smp_mb__before_atomic();
	clear_bit(BNXT_STATE_IN_SP_TASK, &bp->state);
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
	case BCM57504_NPAR:
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

	if (bnxt_pci_mapping(softc)) {
		device_printf(softc->dev, "PCI mapping failed\n");
		rc = ENXIO;
		goto pci_map_fail;
	}

	softc->pdev = kzalloc(sizeof(*softc->pdev), GFP_KERNEL);
	if (!softc->pdev) {
		device_printf(softc->dev, "pdev alloc failed\n");
		rc = -ENOMEM;
		goto free_pci_map;
	}

	rc = linux_pci_attach_device(softc->dev, NULL, NULL, softc->pdev);
	if (rc) {
		device_printf(softc->dev, "Failed to attach Linux PCI device 0x%x\n", rc);
		goto pci_attach_fail;
	}

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
	    (softc->ver_info->chip_num == BCM57504_NPAR) ||
	    (softc->ver_info->chip_num == BCM57502) ||
	    (softc->ver_info->chip_num == BCM57601) ||
	    (softc->ver_info->chip_num == BCM57602) ||
	    (softc->ver_info->chip_num == BCM57604))
		softc->flags |= BNXT_FLAG_CHIP_P5;

	if (softc->ver_info->chip_num == BCM57608)
		softc->flags |= BNXT_FLAG_CHIP_P7;

	softc->flags |= BNXT_FLAG_TPA;

	if (BNXT_CHIP_P5_PLUS(softc) && (!softc->ver_info->chip_rev) &&
			(!softc->ver_info->chip_metal))
		softc->flags &= ~BNXT_FLAG_TPA;

	if (BNXT_CHIP_P5_PLUS(softc))
		softc->flags &= ~BNXT_FLAG_TPA;

	/* Get NVRAM info */
	if (BNXT_PF(softc)) {
		if (!bnxt_pf_wq) {
			bnxt_pf_wq =
				create_singlethread_workqueue("bnxt_pf_wq");
			if (!bnxt_pf_wq) {
				device_printf(softc->dev, "Unable to create workqueue.\n");
				rc = -ENOMEM;
				goto nvm_alloc_fail;
			}
		}

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
	} else if (BNXT_CHIP_P7(softc)) {
		softc->db_ops.bnxt_db_tx = bnxt_thor2_db_tx;
		softc->db_ops.bnxt_db_rx = bnxt_thor2_db_rx;
		softc->db_ops.bnxt_db_rx_cq = bnxt_thor2_db_rx_cq;
		softc->db_ops.bnxt_db_tx_cq = bnxt_thor2_db_tx_cq;
		softc->db_ops.bnxt_db_nq = bnxt_thor2_db_nq;
	} else {
		softc->db_ops.bnxt_db_tx = bnxt_cuw_db_tx;
		softc->db_ops.bnxt_db_rx = bnxt_cuw_db_rx;
		softc->db_ops.bnxt_db_rx_cq = bnxt_cuw_db_cq;
		softc->db_ops.bnxt_db_tx_cq = bnxt_cuw_db_cq;
	}


	/* Get the queue config */
	rc = bnxt_hwrm_queue_qportcfg(softc, HWRM_QUEUE_QPORTCFG_INPUT_FLAGS_PATH_TX);
	if (rc) {
		device_printf(softc->dev, "attach: hwrm qportcfg (tx) failed\n");
		goto failed;
	}
	if (softc->is_asym_q) {
		rc = bnxt_hwrm_queue_qportcfg(softc,
					      HWRM_QUEUE_QPORTCFG_INPUT_FLAGS_PATH_RX);
		if (rc) {
			device_printf(softc->dev, "attach: hwrm qportcfg (rx)  failed\n");
			return rc;
		}
		bnxt_verify_asym_queues(softc);
	} else {
		softc->rx_max_q = softc->tx_max_q;
		memcpy(softc->rx_q_info, softc->tx_q_info, sizeof(softc->rx_q_info));
		memcpy(softc->rx_q_ids, softc->tx_q_ids, sizeof(softc->rx_q_ids));
	}

	/* Get the HW capabilities */
	rc = bnxt_hwrm_func_qcaps(softc);
	if (rc)
		goto failed;

	/*
	 * Register the driver with the FW
	 * Register the async events with the FW
	 */
	rc = bnxt_drv_rgtr(softc);
	if (rc)
		goto failed;

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
	if (BNXT_CHIP_P5_PLUS(softc))
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
	softc->def_cp_ring.ring.doorbell = (BNXT_CHIP_P5_PLUS(softc)) ?
		softc->legacy_db_size : softc->def_cp_ring.ring.id * 0x80;
	softc->def_cp_ring.ring.ring_size = PAGE_SIZE /
	    sizeof(struct cmpl_base);
	softc->def_cp_ring.ring.db_ring_mask = softc->def_cp_ring.ring.ring_size -1 ;
	rc = iflib_dma_alloc(ctx,
	    sizeof(struct cmpl_base) * softc->def_cp_ring.ring.ring_size,
	    &softc->def_cp_ring_mem, 0);
	softc->def_cp_ring.ring.vaddr = softc->def_cp_ring_mem.idi_vaddr;
	softc->def_cp_ring.ring.paddr = softc->def_cp_ring_mem.idi_paddr;
	iflib_config_task_init(ctx, &softc->def_cp_task, bnxt_def_cp_task);

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

	rc = bnxt_create_dcb_sysctls(softc);
	if (rc)
		goto failed;

	set_bit(BNXT_STATE_OPEN, &softc->state);
	INIT_WORK(&softc->sp_task, bnxt_sp_task);
	INIT_DELAYED_WORK(&softc->fw_reset_task, bnxt_fw_reset_task);

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
	if (softc->pdev)
		linux_pci_detach_device(softc->pdev);
pci_attach_fail:
	kfree(softc->pdev);
	softc->pdev = NULL;
free_pci_map:
	bnxt_pci_mapping_free(softc);
pci_map_fail:
	pci_disable_busmaster(softc->dev);
	return (rc);
}

static int
bnxt_attach_post(if_ctx_t ctx)
{
	struct bnxt_softc *softc = iflib_get_softc(ctx);
	if_t ifp = iflib_get_ifp(ctx);
	int rc;

	softc->ifp = ifp;
	bnxt_create_config_sysctls_post(softc);

	/* Update link state etc... */
	rc = bnxt_probe_phy(softc);
	if (rc)
		goto failed;

	/* Needs to be done after probing the phy */
	bnxt_create_ver_sysctls(softc);
	ifmedia_removeall(softc->media);
	bnxt_add_media_types(softc);
	ifmedia_set(softc->media, IFM_ETHER | IFM_AUTO);

	softc->scctx->isc_max_frame_size = if_getmtu(ifp) + ETHER_HDR_LEN +
	    ETHER_CRC_LEN;

	softc->rx_buf_size = min(softc->scctx->isc_max_frame_size, BNXT_PAGE_SIZE);
	bnxt_dcb_init(softc);
	bnxt_rdma_aux_device_init(softc);

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

	bnxt_rdma_aux_device_uninit(softc);
	cancel_delayed_work_sync(&softc->fw_reset_task);
	cancel_work_sync(&softc->sp_task);
	bnxt_dcb_free(softc);
	SLIST_REMOVE(&pf_list, &softc->list, bnxt_softc_list, next);
	bnxt_num_pfs--;
	bnxt_wol_config(ctx);
	bnxt_do_disable_intr(&softc->def_cp_ring);
	bnxt_free_sysctl_ctx(softc);
	bnxt_hwrm_func_reset(softc);
	bnxt_free_ctx_mem(softc);
	bnxt_clear_ids(softc);
	iflib_irq_free(ctx, &softc->def_cp_ring.irq);
	/* We need to free() these here... */
	for (i = softc->nrxqsets-1; i>=0; i--) {
		if (BNXT_CHIP_P5_PLUS(softc))
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

	if (!bnxt_num_pfs && bnxt_pf_wq)
		destroy_workqueue(bnxt_pf_wq);

	if (softc->pdev)
		linux_pci_detach_device(softc->pdev);
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

		if (BNXT_CHIP_P5_PLUS(softc)) {
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

	if (!BNXT_CHIP_P5_PLUS(softc)) {
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
		if (BNXT_CHIP_P5_PLUS(softc)) {
			rgt[i++] = htole16(softc->rx_rings[j].phys_id);
			rgt[i] = htole16(softc->rx_cp_rings[j].ring.phys_id);
		} else {
			rgt[i] = htole16(softc->grp_info[j].grp_id);
		}
		if (++j == softc->nrxqsets)
			j = 0;
	}
}

static void bnxt_get_port_module_status(struct bnxt_softc *softc)
{
	struct bnxt_link_info *link_info = &softc->link_info;
	struct hwrm_port_phy_qcfg_output *resp = &link_info->phy_qcfg_resp;
	uint8_t module_status;

	if (bnxt_update_link(softc, false))
		return;

	module_status = link_info->module_status;
	switch (module_status) {
	case HWRM_PORT_PHY_QCFG_OUTPUT_MODULE_STATUS_DISABLETX:
	case HWRM_PORT_PHY_QCFG_OUTPUT_MODULE_STATUS_PWRDOWN:
	case HWRM_PORT_PHY_QCFG_OUTPUT_MODULE_STATUS_WARNINGMSG:
		device_printf(softc->dev, "Unqualified SFP+ module detected on port %d\n",
			    softc->pf.port_id);
		if (softc->hwrm_spec_code >= 0x10201) {
			device_printf(softc->dev, "Module part number %s\n",
				    resp->phy_vendor_partnumber);
		}
		if (module_status == HWRM_PORT_PHY_QCFG_OUTPUT_MODULE_STATUS_DISABLETX)
			device_printf(softc->dev, "TX is disabled\n");
		if (module_status == HWRM_PORT_PHY_QCFG_OUTPUT_MODULE_STATUS_PWRDOWN)
			device_printf(softc->dev, "SFP+ module is shutdown\n");
	}
}

static void bnxt_aux_dev_free(struct bnxt_softc *softc)
{
	kfree(softc->aux_dev);
	softc->aux_dev = NULL;
}

static struct bnxt_aux_dev *bnxt_aux_dev_init(struct bnxt_softc *softc)
{
	struct bnxt_aux_dev *bnxt_adev;

	msleep(1000 * 2);
	bnxt_adev = kzalloc(sizeof(*bnxt_adev), GFP_KERNEL);
	if (!bnxt_adev)
		return ERR_PTR(-ENOMEM);

	return bnxt_adev;
}

static void bnxt_rdma_aux_device_uninit(struct bnxt_softc *softc)
{
	struct bnxt_aux_dev *bnxt_adev = softc->aux_dev;

	/* Skip if no auxiliary device init was done. */
	if (!(softc->flags & BNXT_FLAG_ROCE_CAP))
		return;

	if (IS_ERR_OR_NULL(bnxt_adev))
		return;

	bnxt_rdma_aux_device_del(softc);

	if (bnxt_adev->id >= 0)
		ida_free(&bnxt_aux_dev_ids, bnxt_adev->id);

	bnxt_aux_dev_free(softc);
}

static void bnxt_rdma_aux_device_init(struct bnxt_softc *softc)
{
	int rc;

	if (!(softc->flags & BNXT_FLAG_ROCE_CAP))
		return;

	softc->aux_dev = bnxt_aux_dev_init(softc);
	if (IS_ERR_OR_NULL(softc->aux_dev)) {
		device_printf(softc->dev, "Failed to init auxiliary device for ROCE\n");
		goto skip_aux_init;
	}

	softc->aux_dev->id = ida_alloc(&bnxt_aux_dev_ids, GFP_KERNEL);
	if (softc->aux_dev->id < 0) {
		device_printf(softc->dev, "ida alloc failed for ROCE auxiliary device\n");
		bnxt_aux_dev_free(softc);
		goto skip_aux_init;
	}

	msleep(1000 * 2);
	/* If aux bus init fails, continue with netdev init. */
	rc = bnxt_rdma_aux_device_add(softc);
	if (rc) {
		device_printf(softc->dev, "Failed to add auxiliary device for ROCE\n");
		msleep(1000 * 2);
		ida_free(&bnxt_aux_dev_ids, softc->aux_dev->id);
	}
	device_printf(softc->dev, "%s:%d Added auxiliary device (id %d) for ROCE \n",
		      __func__, __LINE__, softc->aux_dev->id);
skip_aux_init:
	return;
}

/* Device configuration */
static void
bnxt_init(if_ctx_t ctx)
{
	struct bnxt_softc *softc = iflib_get_softc(ctx);
	struct ifmediareq ifmr;
	int i;
	int rc;

	if (!BNXT_CHIP_P5_PLUS(softc)) {
		rc = bnxt_hwrm_func_reset(softc);
		if (rc)
			return;
	} else if (softc->is_dev_init) {
		bnxt_stop(ctx);
	}

	softc->is_dev_init = true;
	bnxt_clear_ids(softc);

	if (BNXT_CHIP_P5_PLUS(softc))
		goto skip_def_cp_ring;
	/* Allocate the default completion ring */
	softc->def_cp_ring.cons = UINT32_MAX;
	softc->def_cp_ring.v_bit = 1;
	bnxt_mark_cpr_invalid(&softc->def_cp_ring);
	rc = bnxt_hwrm_ring_alloc(softc,
			HWRM_RING_ALLOC_INPUT_RING_TYPE_L2_CMPL,
			&softc->def_cp_ring.ring);
	bnxt_set_db_mask(softc, &softc->def_cp_ring.ring,
			HWRM_RING_ALLOC_INPUT_RING_TYPE_L2_CMPL);
	if (rc)
		goto fail;
skip_def_cp_ring:
	for (i = 0; i < softc->nrxqsets; i++) {
		/* Allocate the statistics context */
		rc = bnxt_hwrm_stat_ctx_alloc(softc, &softc->rx_cp_rings[i],
		    softc->rx_stats[i].idi_paddr);
		if (rc)
			goto fail;

		if (BNXT_CHIP_P5_PLUS(softc)) {
			/* Allocate the NQ */
			softc->nq_rings[i].cons = 0;
			softc->nq_rings[i].raw_cons = 0;
			softc->nq_rings[i].v_bit = 1;
			softc->nq_rings[i].last_idx = UINT32_MAX;
			bnxt_mark_cpr_invalid(&softc->nq_rings[i]);
			rc = bnxt_hwrm_ring_alloc(softc,
					HWRM_RING_ALLOC_INPUT_RING_TYPE_NQ,
					&softc->nq_rings[i].ring);
			bnxt_set_db_mask(softc, &softc->nq_rings[i].ring,
					HWRM_RING_ALLOC_INPUT_RING_TYPE_NQ);
			if (rc)
				goto fail;

			softc->db_ops.bnxt_db_nq(&softc->nq_rings[i], 1);
		}
		/* Allocate the completion ring */
		softc->rx_cp_rings[i].cons = UINT32_MAX;
		softc->rx_cp_rings[i].raw_cons = UINT32_MAX;
		softc->rx_cp_rings[i].v_bit = 1;
		softc->rx_cp_rings[i].last_idx = UINT32_MAX;
		softc->rx_cp_rings[i].toggle = 0;
		bnxt_mark_cpr_invalid(&softc->rx_cp_rings[i]);
		rc = bnxt_hwrm_ring_alloc(softc,
				HWRM_RING_ALLOC_INPUT_RING_TYPE_L2_CMPL,
				&softc->rx_cp_rings[i].ring);
		bnxt_set_db_mask(softc, &softc->rx_cp_rings[i].ring,
				HWRM_RING_ALLOC_INPUT_RING_TYPE_L2_CMPL);
		if (rc)
			goto fail;

		if (BNXT_CHIP_P5_PLUS(softc))
			softc->db_ops.bnxt_db_rx_cq(&softc->rx_cp_rings[i], 1);

		/* Allocate the RX ring */
		rc = bnxt_hwrm_ring_alloc(softc,
		    HWRM_RING_ALLOC_INPUT_RING_TYPE_RX, &softc->rx_rings[i]);
		bnxt_set_db_mask(softc, &softc->rx_rings[i],
				HWRM_RING_ALLOC_INPUT_RING_TYPE_RX);
		if (rc)
			goto fail;
		softc->db_ops.bnxt_db_rx(&softc->rx_rings[i], 0);

		/* Allocate the AG ring */
		rc = bnxt_hwrm_ring_alloc(softc,
				HWRM_RING_ALLOC_INPUT_RING_TYPE_RX_AGG,
				&softc->ag_rings[i]);
		bnxt_set_db_mask(softc, &softc->ag_rings[i],
				HWRM_RING_ALLOC_INPUT_RING_TYPE_RX_AGG);
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
		softc->tx_cp_rings[i].raw_cons = UINT32_MAX;
		softc->tx_cp_rings[i].v_bit = 1;
		softc->tx_cp_rings[i].toggle = 0;
		bnxt_mark_cpr_invalid(&softc->tx_cp_rings[i]);
		rc = bnxt_hwrm_ring_alloc(softc,
				HWRM_RING_ALLOC_INPUT_RING_TYPE_L2_CMPL,
				&softc->tx_cp_rings[i].ring);
		bnxt_set_db_mask(softc, &softc->tx_cp_rings[i].ring,
				HWRM_RING_ALLOC_INPUT_RING_TYPE_L2_CMPL);
		if (rc)
			goto fail;

		if (BNXT_CHIP_P5_PLUS(softc))
			softc->db_ops.bnxt_db_tx_cq(&softc->tx_cp_rings[i], 1);

		/* Allocate the TX ring */
		rc = bnxt_hwrm_ring_alloc(softc,
				HWRM_RING_ALLOC_INPUT_RING_TYPE_TX,
				&softc->tx_rings[i]);
		bnxt_set_db_mask(softc, &softc->tx_rings[i],
				HWRM_RING_ALLOC_INPUT_RING_TYPE_TX);
		if (rc)
			goto fail;
		softc->db_ops.bnxt_db_tx(&softc->tx_rings[i], 0);
	}

	bnxt_do_enable_intr(&softc->def_cp_ring);
	bnxt_get_port_module_status(softc);
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
	struct bnxt_link_info *link_info = &softc->link_info;

	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
		return EINVAL;

	switch (IFM_SUBTYPE(ifm->ifm_media)) {
	case IFM_100_T:
		link_info->autoneg &= ~BNXT_AUTONEG_SPEED;
		link_info->req_link_speed =
		    HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_100MB;
		break;
	case IFM_1000_KX:
	case IFM_1000_SGMII:
	case IFM_1000_CX:
	case IFM_1000_SX:
	case IFM_1000_LX:

		link_info->autoneg &= ~BNXT_AUTONEG_SPEED;

		if (link_info->support_speeds & HWRM_PORT_PHY_QCFG_OUTPUT_SUPPORT_SPEEDS_1GB) {
			link_info->req_link_speed = HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_1GB;

		} else if (link_info->support_speeds2 & HWRM_PORT_PHY_QCFG_OUTPUT_SUPPORT_SPEEDS2_1GB) {
			link_info->req_link_speed = HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEEDS2_1GB;
			link_info->force_speed2_nrz = true;
		}

		break;

	case IFM_2500_KX:
	case IFM_2500_T:
		link_info->autoneg &= ~BNXT_AUTONEG_SPEED;
		link_info->req_link_speed =
		    HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_2_5GB;
		break;
	case IFM_10G_CR1:
	case IFM_10G_KR:
	case IFM_10G_LR:
	case IFM_10G_SR:

		link_info->autoneg &= ~BNXT_AUTONEG_SPEED;

		if (link_info->support_speeds & HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_10GB) {
			link_info->req_link_speed = HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_10GB;

		} else if (link_info->support_speeds2 & HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEEDS2_10GB) {
			link_info->req_link_speed = HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEEDS2_10GB;
			link_info->force_speed2_nrz = true;
		}

		break;
	case IFM_20G_KR2:
		link_info->autoneg &= ~BNXT_AUTONEG_SPEED;
		link_info->req_link_speed =
		    HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_20GB;
		break;
	case IFM_25G_CR:
	case IFM_25G_KR:
	case IFM_25G_SR:
	case IFM_25G_LR:

		link_info->autoneg &= ~BNXT_AUTONEG_SPEED;

		if (link_info->support_speeds & HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_25GB) {
			link_info->req_link_speed = HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_25GB;

		} else if (link_info->support_speeds2 & HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEEDS2_25GB) {
			link_info->req_link_speed = HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEEDS2_25GB;
			link_info->force_speed2_nrz = true;
		}

		break;

	case IFM_40G_CR4:
	case IFM_40G_KR4:
	case IFM_40G_LR4:
	case IFM_40G_SR4:
	case IFM_40G_XLAUI:
	case IFM_40G_XLAUI_AC:

		link_info->autoneg &= ~BNXT_AUTONEG_SPEED;

		if (link_info->support_speeds & HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_40GB) {
			link_info->req_link_speed = HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_40GB;

		} else if (link_info->support_speeds2 & HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEEDS2_40GB) {
			link_info->req_link_speed = HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEEDS2_40GB;
			link_info->force_speed2_nrz = true;
		}

		break;

	case IFM_50G_CR2:
	case IFM_50G_KR2:
	case IFM_50G_KR4:
	case IFM_50G_SR2:
	case IFM_50G_LR2:

		link_info->autoneg &= ~BNXT_AUTONEG_SPEED;

		if (link_info->support_speeds & HWRM_PORT_PHY_QCFG_OUTPUT_SUPPORT_SPEEDS_50GB) {
			link_info->req_link_speed = HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_50GB;

		} else if (link_info->support_speeds2 & HWRM_PORT_PHY_QCFG_OUTPUT_SUPPORT_SPEEDS2_50GB) {
			link_info->req_link_speed = HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEEDS2_50GB;
			link_info->force_speed2_nrz = true;
		}

		break;

	case IFM_50G_CP:
	case IFM_50G_LR:
	case IFM_50G_SR:
	case IFM_50G_KR_PAM4:

		link_info->autoneg &= ~BNXT_AUTONEG_SPEED;

		if (link_info->support_pam4_speeds & HWRM_PORT_PHY_QCFG_OUTPUT_SUPPORT_PAM4_SPEEDS_50G) {
			link_info->req_link_speed = HWRM_PORT_PHY_CFG_INPUT_FORCE_PAM4_LINK_SPEED_50GB;
			link_info->force_pam4_speed = true;

		} else if (link_info->support_speeds2 & HWRM_PORT_PHY_QCFG_OUTPUT_SUPPORT_SPEEDS2_50GB_PAM4_56) {
			link_info->req_link_speed = HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEEDS2_50GB_PAM4_56;
			link_info->force_pam4_56_speed2 = true;
		}

		break;

	case IFM_100G_CR4:
	case IFM_100G_KR4:
	case IFM_100G_LR4:
	case IFM_100G_SR4:
	case IFM_100G_AUI4:

		link_info->autoneg &= ~BNXT_AUTONEG_SPEED;

		if (link_info->support_speeds & HWRM_PORT_PHY_QCFG_OUTPUT_SUPPORT_SPEEDS_100GB) {
			link_info->req_link_speed = HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_100GB;

		} else if (link_info->support_speeds2 & HWRM_PORT_PHY_QCFG_OUTPUT_SUPPORT_SPEEDS2_100GB) {
			link_info->req_link_speed = HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEEDS2_100GB;
			link_info->force_speed2_nrz = true;
		}

		break;

	case IFM_100G_CP2:
	case IFM_100G_SR2:
	case IFM_100G_KR2_PAM4:
	case IFM_100G_AUI2:

		link_info->autoneg &= ~BNXT_AUTONEG_SPEED;

		if (link_info->support_pam4_speeds & HWRM_PORT_PHY_QCFG_OUTPUT_SUPPORT_PAM4_SPEEDS_100G) {
			link_info->req_link_speed = HWRM_PORT_PHY_CFG_INPUT_FORCE_PAM4_LINK_SPEED_100GB;
			link_info->force_pam4_speed = true;

		} else if (link_info->support_speeds2 & HWRM_PORT_PHY_QCFG_OUTPUT_SUPPORT_SPEEDS2_100GB_PAM4_56) {
			link_info->req_link_speed = HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEEDS2_100GB_PAM4_56;
			link_info->force_pam4_56_speed2 = true;
		}

		break;

	case IFM_100G_KR_PAM4:
	case IFM_100G_CR_PAM4:
	case IFM_100G_DR:
	case IFM_100G_AUI2_AC:

		link_info->autoneg &= ~BNXT_AUTONEG_SPEED;

		if (link_info->support_speeds2 & HWRM_PORT_PHY_QCFG_OUTPUT_SUPPORT_SPEEDS2_100GB_PAM4_112) {
			link_info->req_link_speed = HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEEDS2_100GB_PAM4_112;
			link_info->force_pam4_112_speed2 = true;
		}

		break;

	case IFM_200G_SR4:
	case IFM_200G_FR4:
	case IFM_200G_LR4:
	case IFM_200G_DR4:
	case IFM_200G_CR4_PAM4:
	case IFM_200G_KR4_PAM4:

		link_info->autoneg &= ~BNXT_AUTONEG_SPEED;

		if (link_info->support_pam4_speeds & HWRM_PORT_PHY_QCFG_OUTPUT_SUPPORT_PAM4_SPEEDS_200G) {
			link_info->req_link_speed = HWRM_PORT_PHY_CFG_INPUT_FORCE_PAM4_LINK_SPEED_200GB;
			link_info->force_pam4_speed = true;

		} else if (link_info->support_speeds2 & HWRM_PORT_PHY_QCFG_OUTPUT_SUPPORT_SPEEDS2_200GB_PAM4_56) {
			link_info->req_link_speed = HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEEDS2_200GB_PAM4_56;
			link_info->force_pam4_56_speed2 = true;
		}

		break;

	case IFM_200G_AUI4:

		link_info->autoneg &= ~BNXT_AUTONEG_SPEED;

		if (link_info->support_speeds2 & HWRM_PORT_PHY_QCFG_OUTPUT_SUPPORT_SPEEDS2_200GB_PAM4_112) {
			link_info->req_link_speed = HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEEDS2_200GB_PAM4_112;
			link_info->force_pam4_112_speed2 = true;
		}

		break;

	case IFM_400G_FR8:
	case IFM_400G_LR8:
	case IFM_400G_AUI8:
		link_info->autoneg &= ~BNXT_AUTONEG_SPEED;

		if (link_info->support_speeds2 & HWRM_PORT_PHY_QCFG_OUTPUT_SUPPORT_SPEEDS2_400GB_PAM4_56) {
			link_info->req_link_speed = HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEEDS2_400GB_PAM4_56;
			link_info->force_pam4_56_speed2 = true;
		}

		break;

	case IFM_400G_AUI8_AC:
	case IFM_400G_DR4:
		link_info->autoneg &= ~BNXT_AUTONEG_SPEED;

		if (link_info->support_speeds2 & HWRM_PORT_PHY_QCFG_OUTPUT_SUPPORT_SPEEDS2_400GB_PAM4_112) {
			link_info->req_link_speed = HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEEDS2_400GB_PAM4_112;
			link_info->force_pam4_112_speed2 = true;
		}

		break;

	case IFM_1000_T:
		link_info->advertising = HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_1GB;
		link_info->autoneg |= BNXT_AUTONEG_SPEED;
		break;
	case IFM_10G_T:
		link_info->advertising = HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_10GB;
		link_info->autoneg |= BNXT_AUTONEG_SPEED;
		break;
	default:
		device_printf(softc->dev,
		    "Unsupported media type!  Using auto\n");
		/* Fall-through */
	case IFM_AUTO:
		// Auto
		link_info->autoneg |= BNXT_AUTONEG_SPEED;
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

	if (BNXT_CHIP_P5_PLUS(softc) &&
	    (softc->flags & BNXT_FLAG_FW_CAP_EXT_STATS))
		bnxt_hwrm_port_qstats_ext(softc);

	if (BNXT_CHIP_P5_PLUS(softc)) {
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

	if (BNXT_CHIP_P5_PLUS(softc))
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

	if (BNXT_CHIP_P5_PLUS(softc))
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
		if (BNXT_CHIP_P5_PLUS(softc))
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

	if (BNXT_CHIP_P5_PLUS(softc))
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

void
process_nq(struct bnxt_softc *softc, uint16_t nqid)
{
	struct bnxt_cp_ring *cpr = &softc->nq_rings[nqid];
	nq_cn_t *cmp = (nq_cn_t *) cpr->ring.vaddr;
	struct bnxt_cp_ring *tx_cpr = &softc->tx_cp_rings[nqid];
	struct bnxt_cp_ring *rx_cpr = &softc->rx_cp_rings[nqid];
	bool v_bit = cpr->v_bit;
	uint32_t cons = cpr->cons;
	uint32_t raw_cons = cpr->raw_cons;
	uint16_t nq_type, nqe_cnt = 0;

	while (1) {
		if (!NQ_VALID(&cmp[cons], v_bit)) {
			goto done;
		}

		nq_type = NQ_CN_TYPE_MASK & cmp[cons].type;

		if (NQE_CN_TYPE(nq_type) != NQ_CN_TYPE_CQ_NOTIFICATION) {
			 bnxt_process_async_msg(cpr, (tx_cmpl_t *)&cmp[cons]);
		} else {
			tx_cpr->toggle = NQE_CN_TOGGLE(cmp[cons].type);
			rx_cpr->toggle = NQE_CN_TOGGLE(cmp[cons].type);
		}

		NEXT_CP_CONS_V(&cpr->ring, cons, v_bit);
		raw_cons++;
		nqe_cnt++;
	}
done:
	if (nqe_cnt) {
		cpr->cons = cons;
		cpr->raw_cons = raw_cons;
		cpr->v_bit = v_bit;
	}
}

static int
bnxt_rx_queue_intr_enable(if_ctx_t ctx, uint16_t qid)
{
	struct bnxt_softc *softc = iflib_get_softc(ctx);

	if (BNXT_CHIP_P5_PLUS(softc)) {
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
		if (BNXT_CHIP_P5_PLUS(softc))
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

	if (BNXT_CHIP_P5_PLUS(softc))
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
		if (BNXT_CHIP_P5_PLUS(softc)) {
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

static int
bnxt_i2c_req(if_ctx_t ctx, struct ifi2creq *i2c)
{
	struct bnxt_softc *softc = iflib_get_softc(ctx);
	uint8_t *data = i2c->data;
	int rc;

	/* No point in going further if phy status indicates
	 * module is not inserted or if it is powered down or
	 * if it is of type 10GBase-T
	 */
	if (softc->link_info.module_status >
		HWRM_PORT_PHY_QCFG_OUTPUT_MODULE_STATUS_WARNINGMSG)
		return -EOPNOTSUPP;

	/* This feature is not supported in older firmware versions */
	if (!BNXT_CHIP_P5_PLUS(softc) ||
	    (softc->hwrm_spec_code < 0x10202))
		return -EOPNOTSUPP;


	rc = bnxt_read_sfp_module_eeprom_info(softc, I2C_DEV_ADDR_A0, 0, 0, 0,
		i2c->offset, i2c->len, data);

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

	softc->phy_flags = 0;
	rc = bnxt_hwrm_phy_qcaps(softc);
	if (rc) {
		device_printf(softc->dev,
			      "Probe phy can't get phy capabilities (rc: %x)\n", rc);
		return rc;
	}

	rc = bnxt_update_link(softc, false);
	if (rc) {
		device_printf(softc->dev,
		    "Probe phy can't update link (rc: %x)\n", rc);
		return (rc);
	}

	bnxt_get_port_module_status(softc);

	/*initialize the ethool setting copy with NVM settings */
	if (link_info->auto_mode != HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_MODE_NONE)
		link_info->autoneg |= BNXT_AUTONEG_SPEED;

	link_info->req_duplex = link_info->duplex_setting;

	/* NRZ link speed */
	if (link_info->autoneg & BNXT_AUTONEG_SPEED)
		link_info->req_link_speed = link_info->auto_link_speeds;
	else
		link_info->req_link_speed = link_info->force_link_speed;

	/* PAM4 link speed */
	if (link_info->auto_pam4_link_speeds)
		link_info->req_link_speed = link_info->auto_pam4_link_speeds;
	if (link_info->force_pam4_link_speed)
		link_info->req_link_speed = link_info->force_pam4_link_speed;

	return (rc);
}

static void
add_media(struct bnxt_softc *softc, u8 media_type, u16 supported_NRZ_speeds,
	  u16 supported_pam4_speeds, u16 supported_speeds2)
{

	switch (media_type) {
		case BNXT_MEDIA_CR:

			BNXT_IFMEDIA_ADD(supported_pam4_speeds, PAM4_SPEEDS_50G, IFM_50G_CP);
			BNXT_IFMEDIA_ADD(supported_pam4_speeds, PAM4_SPEEDS_100G, IFM_100G_CP2);
			BNXT_IFMEDIA_ADD(supported_pam4_speeds, PAM4_SPEEDS_200G, IFM_200G_CR4_PAM4);

			BNXT_IFMEDIA_ADD(supported_NRZ_speeds, SPEEDS_100GB, IFM_100G_CR4);
			BNXT_IFMEDIA_ADD(supported_NRZ_speeds, SPEEDS_50GB, IFM_50G_CR2);
			BNXT_IFMEDIA_ADD(supported_NRZ_speeds, SPEEDS_40GB, IFM_40G_CR4);
			BNXT_IFMEDIA_ADD(supported_NRZ_speeds, SPEEDS_25GB, IFM_25G_CR);
			BNXT_IFMEDIA_ADD(supported_NRZ_speeds, SPEEDS_10GB, IFM_10G_CR1);
			BNXT_IFMEDIA_ADD(supported_NRZ_speeds, SPEEDS_1GB, IFM_1000_CX);
			/* thor2 nrz*/
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_100GB, IFM_100G_CR4);
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_50GB, IFM_50G_CR2);
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_40GB, IFM_40G_CR4);
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_25GB, IFM_25G_CR);
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_10GB, IFM_10G_CR1);
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_1GB, IFM_1000_CX);
			/* thor2 PAM56 */
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_50GB_PAM4_56, IFM_50G_CP);
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_100GB_PAM4_56, IFM_100G_CP2);
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_200GB_PAM4_56, IFM_200G_CR4_PAM4);
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_400GB_PAM4_56, IFM_400G_AUI8);
			/* thor2 PAM112 */
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_100GB_PAM4_112, IFM_100G_CR_PAM4);
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_200GB_PAM4_112, IFM_200G_AUI4);
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_400GB_PAM4_112, IFM_400G_AUI8_AC);

			break;

		case BNXT_MEDIA_LR:
			BNXT_IFMEDIA_ADD(supported_pam4_speeds, PAM4_SPEEDS_50G, IFM_50G_LR);
			BNXT_IFMEDIA_ADD(supported_pam4_speeds, PAM4_SPEEDS_200G, IFM_200G_LR4);
			BNXT_IFMEDIA_ADD(supported_NRZ_speeds, SPEEDS_100GB, IFM_100G_LR4);
			BNXT_IFMEDIA_ADD(supported_NRZ_speeds, SPEEDS_50GB, IFM_50G_LR2);
			BNXT_IFMEDIA_ADD(supported_NRZ_speeds, SPEEDS_40GB, IFM_40G_LR4);
			BNXT_IFMEDIA_ADD(supported_NRZ_speeds, SPEEDS_25GB, IFM_25G_LR);
			BNXT_IFMEDIA_ADD(supported_NRZ_speeds, SPEEDS_10GB, IFM_10G_LR);
			/* thor2 nrz*/
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_100GB, IFM_100G_LR4);
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_50GB, IFM_50G_LR2);
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_40GB, IFM_40G_LR4);
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_25GB, IFM_25G_LR);
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_10GB, IFM_10G_LR);
			/* thor2 PAM56 */
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_50GB_PAM4_56, IFM_50G_LR);
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_100GB_PAM4_56, IFM_100G_AUI2);
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_200GB_PAM4_56, IFM_200G_LR4);
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_400GB_PAM4_56, IFM_400G_LR8);
			/* thor2 PAM112 */
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_100GB_PAM4_112, IFM_100G_AUI2_AC);
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_200GB_PAM4_112, IFM_200G_AUI4);
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_400GB_PAM4_112, IFM_400G_AUI8_AC);

			break;

		case BNXT_MEDIA_SR:
			BNXT_IFMEDIA_ADD(supported_pam4_speeds, PAM4_SPEEDS_50G, IFM_50G_SR);
			BNXT_IFMEDIA_ADD(supported_pam4_speeds, PAM4_SPEEDS_100G, IFM_100G_SR2);
			BNXT_IFMEDIA_ADD(supported_pam4_speeds, PAM4_SPEEDS_200G, IFM_200G_SR4);
			BNXT_IFMEDIA_ADD(supported_NRZ_speeds, SPEEDS_100GB, IFM_100G_SR4);
			BNXT_IFMEDIA_ADD(supported_NRZ_speeds, SPEEDS_50GB, IFM_50G_SR2);
			BNXT_IFMEDIA_ADD(supported_NRZ_speeds, SPEEDS_40GB, IFM_40G_SR4);
			BNXT_IFMEDIA_ADD(supported_NRZ_speeds, SPEEDS_25GB, IFM_25G_SR);
			BNXT_IFMEDIA_ADD(supported_NRZ_speeds, SPEEDS_10GB, IFM_10G_SR);
			BNXT_IFMEDIA_ADD(supported_NRZ_speeds, SPEEDS_1GB, IFM_1000_SX);
			/* thor2 nrz*/
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_100GB, IFM_100G_SR4);
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_50GB, IFM_50G_SR2);
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_40GB, IFM_40G_SR4);
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_25GB, IFM_25G_SR);
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_10GB, IFM_10G_SR);
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_1GB, IFM_1000_SX);
			/* thor2 PAM56 */
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_50GB_PAM4_56, IFM_50G_SR);
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_100GB_PAM4_56, IFM_100G_SR2);
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_200GB_PAM4_56, IFM_200G_SR4);
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_400GB_PAM4_56, IFM_400G_AUI8);
			/* thor2 PAM112 */
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_100GB_PAM4_112, IFM_100G_AUI2_AC);
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_200GB_PAM4_112, IFM_200G_AUI4);
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_400GB_PAM4_112, IFM_400G_DR4);
			break;

		case BNXT_MEDIA_ER:
			BNXT_IFMEDIA_ADD(supported_NRZ_speeds, SPEEDS_40GB, IFM_40G_ER4);
			BNXT_IFMEDIA_ADD(supported_NRZ_speeds, SPEEDS_100GB, IFM_100G_AUI4);
			/* thor2 PAM56 */
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_50GB_PAM4_56, IFM_50G_LR);
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_100GB_PAM4_56, IFM_100G_AUI2);
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_200GB_PAM4_56, IFM_200G_LR4);
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_400GB_PAM4_56, IFM_400G_FR8);
			/* thor2 PAM112 */
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_100GB_PAM4_112, IFM_100G_AUI2_AC);
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_200GB_PAM4_112, IFM_200G_AUI4_AC);
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_400GB_PAM4_112, IFM_400G_AUI8_AC);
			break;

		case BNXT_MEDIA_KR:
			BNXT_IFMEDIA_ADD(supported_pam4_speeds, PAM4_SPEEDS_50G, IFM_50G_KR_PAM4);
			BNXT_IFMEDIA_ADD(supported_pam4_speeds, PAM4_SPEEDS_100G, IFM_100G_KR2_PAM4);
			BNXT_IFMEDIA_ADD(supported_pam4_speeds, PAM4_SPEEDS_200G, IFM_200G_KR4_PAM4);
			BNXT_IFMEDIA_ADD(supported_NRZ_speeds, SPEEDS_100GB, IFM_100G_KR4);
			BNXT_IFMEDIA_ADD(supported_NRZ_speeds, SPEEDS_50GB, IFM_50G_KR2);
			BNXT_IFMEDIA_ADD(supported_NRZ_speeds, SPEEDS_50GB, IFM_50G_KR4);
			BNXT_IFMEDIA_ADD(supported_NRZ_speeds, SPEEDS_40GB, IFM_40G_KR4);
			BNXT_IFMEDIA_ADD(supported_NRZ_speeds, SPEEDS_25GB, IFM_25G_KR);
			BNXT_IFMEDIA_ADD(supported_NRZ_speeds, SPEEDS_20GB, IFM_20G_KR2);
			BNXT_IFMEDIA_ADD(supported_NRZ_speeds, SPEEDS_10GB, IFM_10G_KR);
			BNXT_IFMEDIA_ADD(supported_NRZ_speeds, SPEEDS_1GB, IFM_1000_KX);
			break;

		case BNXT_MEDIA_AC:
			BNXT_IFMEDIA_ADD(supported_NRZ_speeds, SPEEDS_25GB, IFM_25G_ACC);
			BNXT_IFMEDIA_ADD(supported_NRZ_speeds, SPEEDS_10GB, IFM_10G_AOC);
			BNXT_IFMEDIA_ADD(supported_NRZ_speeds, SPEEDS_40GB, IFM_40G_XLAUI);
			BNXT_IFMEDIA_ADD(supported_NRZ_speeds, SPEEDS_40GB, IFM_40G_XLAUI_AC);
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_25GB, IFM_25G_ACC);
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_10GB, IFM_10G_AOC);
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_40GB, IFM_40G_XLAUI);
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_40GB, IFM_40G_XLAUI_AC);
			break;

		case BNXT_MEDIA_BASECX:
			BNXT_IFMEDIA_ADD(supported_NRZ_speeds, SPEEDS_1GB, IFM_1000_CX);
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_1GB, IFM_1000_CX);
			break;

		case BNXT_MEDIA_BASET:
			BNXT_IFMEDIA_ADD(supported_NRZ_speeds, SPEEDS_10GB, IFM_10G_T);
			BNXT_IFMEDIA_ADD(supported_NRZ_speeds, SPEEDS_2_5GB, IFM_2500_T);
			BNXT_IFMEDIA_ADD(supported_NRZ_speeds, SPEEDS_1GB, IFM_1000_T);
			BNXT_IFMEDIA_ADD(supported_NRZ_speeds, SPEEDS_100MB, IFM_100_T);
			BNXT_IFMEDIA_ADD(supported_NRZ_speeds, SPEEDS_10MB, IFM_10_T);
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_10GB, IFM_10G_T);
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_1GB, IFM_1000_T);
			break;

		case BNXT_MEDIA_BASEKX:
			BNXT_IFMEDIA_ADD(supported_NRZ_speeds, SPEEDS_10GB, IFM_10G_KR);
			BNXT_IFMEDIA_ADD(supported_NRZ_speeds, SPEEDS_2_5GB, IFM_2500_KX);
			BNXT_IFMEDIA_ADD(supported_NRZ_speeds, SPEEDS_1GB, IFM_1000_KX);
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_10GB, IFM_10G_KR);
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_1GB, IFM_1000_KX);
			break;

		case BNXT_MEDIA_BASESGMII:
			BNXT_IFMEDIA_ADD(supported_NRZ_speeds, SPEEDS_1GB, IFM_1000_SGMII);
			BNXT_IFMEDIA_ADD(supported_speeds2, SPEEDS2_1GB, IFM_1000_SGMII);
			break;

		default:
			break;

	}
	return;

}

static void
bnxt_add_media_types(struct bnxt_softc *softc)
{
	struct bnxt_link_info *link_info = &softc->link_info;
	uint16_t supported_NRZ_speeds = 0, supported_pam4_speeds = 0, supported_speeds2 = 0;
	uint8_t phy_type = get_phy_type(softc), media_type;

	supported_NRZ_speeds = link_info->support_speeds;
	supported_speeds2 = link_info->support_speeds2;
	supported_pam4_speeds = link_info->support_pam4_speeds;

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

	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_50G_BASECR:
	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_100G_BASECR2:
	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_200G_BASECR4:
	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_400G_BASECR8:

	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_100G_BASECR:
	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_200G_BASECR2:
	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_400G_BASECR4:

		media_type = BNXT_MEDIA_CR;
		break;

	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_100G_BASELR4:
	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_40G_BASELR4:
	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_BASELR:

	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_50G_BASELR:
	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_100G_BASELR2:
	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_200G_BASELR4:
	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_400G_BASELR8:

	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_100G_BASELR:
	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_200G_BASELR2:
	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_400G_BASELR4:

		media_type = BNXT_MEDIA_LR;
		break;

	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_100G_BASESR10:
	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_100G_BASESR4:
	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_40G_BASESR4:
	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_BASESR:
	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_25G_BASESR:
	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_1G_BASESX:

	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_50G_BASESR:
	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_100G_BASESR2:
	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_200G_BASESR4:
	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_400G_BASESR8:

	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_100G_BASESR:
	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_200G_BASESR2:
	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_400G_BASESR4:

		media_type = BNXT_MEDIA_SR;
		break;

	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_40G_BASEER4:
	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_100G_BASEER4:

	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_50G_BASEER:
	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_100G_BASEER2:
	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_200G_BASEER4:
	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_400G_BASEER8:

	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_100G_BASEER:
	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_200G_BASEER2:
	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_400G_BASEER4:

		media_type = BNXT_MEDIA_ER;
		break;

	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_BASEKR4:
	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_BASEKR2:
	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_BASEKR:
		media_type = BNXT_MEDIA_KR;
		break;

	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_40G_ACTIVE_CABLE:
		media_type = BNXT_MEDIA_AC;
		break;

	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_1G_BASECX:
		media_type = BNXT_MEDIA_BASECX;
		break;

	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_1G_BASET:
	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_BASET:
	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_BASETE:
		media_type = BNXT_MEDIA_BASET;
		break;

	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_BASEKX:
		media_type = BNXT_MEDIA_BASEKX;
		break;

	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_SGMIIEXTPHY:
		media_type = BNXT_MEDIA_BASESGMII;
		break;

	case HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_UNKNOWN:
		/* Only Autoneg is supported for TYPE_UNKNOWN */
		break;

        default:
		/* Only Autoneg is supported for new phy type values */
		device_printf(softc->dev, "phy type %d not supported by driver\n", phy_type);
		break;
	}

	switch (link_info->sig_mode) {
	case BNXT_SIG_MODE_NRZ:
		if (supported_NRZ_speeds != 0)
			add_media(softc, media_type, supported_NRZ_speeds, 0, 0);
		else
			add_media(softc, media_type, 0, 0, supported_speeds2);
		break;
	case BNXT_SIG_MODE_PAM4:
		if (supported_pam4_speeds != 0)
			add_media(softc, media_type, 0, supported_pam4_speeds, 0);
		else
			add_media(softc, media_type, 0, 0, supported_speeds2);
		break;
	case BNXT_SIG_MODE_PAM4_112:
		add_media(softc, media_type, 0, 0, supported_speeds2);
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

#define ETHTOOL_SPEED_1000		1000
#define ETHTOOL_SPEED_10000		10000
#define ETHTOOL_SPEED_20000		20000
#define ETHTOOL_SPEED_25000		25000
#define ETHTOOL_SPEED_40000		40000
#define ETHTOOL_SPEED_50000		50000
#define ETHTOOL_SPEED_100000		100000
#define ETHTOOL_SPEED_200000		200000
#define ETHTOOL_SPEED_UNKNOWN		-1

static u32
bnxt_fw_to_ethtool_speed(u16 fw_link_speed)
{
	switch (fw_link_speed) {
	case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_1GB:
		return ETHTOOL_SPEED_1000;
	case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_10GB:
		return ETHTOOL_SPEED_10000;
	case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_20GB:
		return ETHTOOL_SPEED_20000;
	case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_25GB:
		return ETHTOOL_SPEED_25000;
	case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_40GB:
		return ETHTOOL_SPEED_40000;
	case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_50GB:
		return ETHTOOL_SPEED_50000;
	case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_100GB:
		return ETHTOOL_SPEED_100000;
	case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_200GB:
		return ETHTOOL_SPEED_200000;
	default:
		return ETHTOOL_SPEED_UNKNOWN;
	}
}

void
bnxt_report_link(struct bnxt_softc *softc)
{
	struct bnxt_link_info *link_info = &softc->link_info;
	const char *duplex = NULL, *flow_ctrl = NULL;
	const char *signal_mode = "";

	if(softc->edev)
		softc->edev->espeed =
		    bnxt_fw_to_ethtool_speed(link_info->link_speed);

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

		if (softc->link_info.phy_qcfg_resp.option_flags &
		    HWRM_PORT_PHY_QCFG_OUTPUT_OPTION_FLAGS_SIGNAL_MODE_KNOWN) {
			uint8_t sig_mode = softc->link_info.active_fec_sig_mode &
				      HWRM_PORT_PHY_QCFG_OUTPUT_SIGNAL_MODE_MASK;
			switch (sig_mode) {
			case BNXT_SIG_MODE_NRZ:
				signal_mode = "(NRZ) ";
				break;
			case BNXT_SIG_MODE_PAM4:
				signal_mode = "(PAM4 56Gbps) ";
				break;
			case BNXT_SIG_MODE_PAM4_112:
				signal_mode = "(PAM4 112Gbps) ";
				break;
			default:
				break;
			}
		link_info->sig_mode = sig_mode;
		}

		iflib_link_state_change(softc->ctx, LINK_STATE_UP,
		    IF_Gbps(100));
		device_printf(softc->dev, "Link is UP %s %s, %s - %d Mbps \n", duplex, signal_mode,
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
	if (!BNXT_CHIP_P5_PLUS(softc))
		softc->db_ops.bnxt_db_rx_cq(cpr, 0);

	return FILTER_SCHEDULE_THREAD;
}

static int
bnxt_handle_def_cp(void *arg)
{
	struct bnxt_softc *softc = arg;

	softc->db_ops.bnxt_db_rx_cq(&softc->def_cp_ring, 0);
	iflib_config_task_enqueue(softc->ctx, &softc->def_cp_task);
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

static void bnxt_event_error_report(struct bnxt_softc *softc, u32 data1, u32 data2)
{
	u32 err_type = BNXT_EVENT_ERROR_REPORT_TYPE(data1);

	switch (err_type) {
	case HWRM_ASYNC_EVENT_CMPL_ERROR_REPORT_BASE_EVENT_DATA1_ERROR_TYPE_INVALID_SIGNAL:
		device_printf(softc->dev,
			      "1PPS: Received invalid signal on pin%u from the external source. Please fix the signal and reconfigure the pin\n",
			      BNXT_EVENT_INVALID_SIGNAL_DATA(data2));
		break;
	case HWRM_ASYNC_EVENT_CMPL_ERROR_REPORT_BASE_EVENT_DATA1_ERROR_TYPE_PAUSE_STORM:
		device_printf(softc->dev,
			      "Pause Storm detected!\n");
		break;
	case HWRM_ASYNC_EVENT_CMPL_ERROR_REPORT_BASE_EVENT_DATA1_ERROR_TYPE_DOORBELL_DROP_THRESHOLD:
		device_printf(softc->dev,
			      "One or more MMIO doorbells dropped by the device! epoch: 0x%x\n",
			      BNXT_EVENT_DBR_EPOCH(data1));
		break;
	case HWRM_ASYNC_EVENT_CMPL_ERROR_REPORT_BASE_EVENT_DATA1_ERROR_TYPE_NVM: {
		const char *nvm_err_str;

		if (EVENT_DATA1_NVM_ERR_TYPE_WRITE(data1))
			nvm_err_str = "nvm write error";
		else if (EVENT_DATA1_NVM_ERR_TYPE_ERASE(data1))
			nvm_err_str = "nvm erase error";
		else
			nvm_err_str = "unrecognized nvm error";

		device_printf(softc->dev,
			      "%s reported at address 0x%x\n", nvm_err_str,
			      (u32)EVENT_DATA2_NVM_ERR_ADDR(data2));
		break;
	}
	case HWRM_ASYNC_EVENT_CMPL_ERROR_REPORT_BASE_EVENT_DATA1_ERROR_TYPE_THERMAL_THRESHOLD: {
		char *threshold_type;
		char *dir_str;

		switch (EVENT_DATA1_THERMAL_THRESHOLD_TYPE(data1)) {
		case HWRM_ASYNC_EVENT_CMPL_ERROR_REPORT_THERMAL_EVENT_DATA1_THRESHOLD_TYPE_WARN:
			threshold_type = "warning";
			break;
		case HWRM_ASYNC_EVENT_CMPL_ERROR_REPORT_THERMAL_EVENT_DATA1_THRESHOLD_TYPE_CRITICAL:
			threshold_type = "critical";
			break;
		case HWRM_ASYNC_EVENT_CMPL_ERROR_REPORT_THERMAL_EVENT_DATA1_THRESHOLD_TYPE_FATAL:
			threshold_type = "fatal";
			break;
		case HWRM_ASYNC_EVENT_CMPL_ERROR_REPORT_THERMAL_EVENT_DATA1_THRESHOLD_TYPE_SHUTDOWN:
			threshold_type = "shutdown";
			break;
		default:
			device_printf(softc->dev,
				      "Unknown Thermal threshold type event\n");
			return;
		}
		if (EVENT_DATA1_THERMAL_THRESHOLD_DIR_INCREASING(data1))
			dir_str = "above";
		else
			dir_str = "below";
		device_printf(softc->dev,
			      "Chip temperature has gone %s the %s thermal threshold!\n",
			      dir_str, threshold_type);
		device_printf(softc->dev,
			      "Temperature (In Celsius), Current: %u, threshold: %u\n",
			      BNXT_EVENT_THERMAL_CURRENT_TEMP(data2),
			      BNXT_EVENT_THERMAL_THRESHOLD_TEMP(data2));
		break;
	}
	case HWRM_ASYNC_EVENT_CMPL_ERROR_REPORT_BASE_EVENT_DATA1_ERROR_TYPE_DUAL_DATA_RATE_NOT_SUPPORTED:
		device_printf(softc->dev,
			      "Speed change is not supported with dual rate transceivers on this board\n");
		break;

	default:
	device_printf(softc->dev,
		      "FW reported unknown error type: %u, data1: 0x%x data2: 0x%x\n",
		      err_type, data1, data2);
		break;
	}
}

static void
bnxt_handle_async_event(struct bnxt_softc *softc, struct cmpl_base *cmpl)
{
	struct hwrm_async_event_cmpl *ae = (void *)cmpl;
	uint16_t async_id = le16toh(ae->event_id);
	struct ifmediareq ifmr;
	char *type_str;
	char *status_desc;
	struct bnxt_fw_health *fw_health;
	u32 data1 = le32toh(ae->event_data1);
	u32 data2 = le32toh(ae->event_data2);

	switch (async_id) {
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_LINK_STATUS_CHANGE:
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_LINK_SPEED_CHANGE:
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_LINK_SPEED_CFG_CHANGE:
		if (BNXT_CHIP_P5_PLUS(softc))
			bit_set(softc->state_bv, BNXT_STATE_LINK_CHANGE);
		else
			bnxt_media_status(softc->ctx, &ifmr);
		break;
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_ERROR_REPORT: {
		bnxt_event_error_report(softc, data1, data2);
		goto async_event_process_exit;
	}
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_DOORBELL_PACING_THRESHOLD:
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_DOORBELL_PACING_NQ_UPDATE:
		break;
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_RESET_NOTIFY: {
		type_str = "Solicited";

		if (!softc->fw_health)
			goto async_event_process_exit;

		softc->fw_reset_timestamp = jiffies;
		softc->fw_reset_min_dsecs = ae->timestamp_lo;
		if (!softc->fw_reset_min_dsecs)
			softc->fw_reset_min_dsecs = BNXT_DFLT_FW_RST_MIN_DSECS;
		softc->fw_reset_max_dsecs = le16toh(ae->timestamp_hi);
		if (!softc->fw_reset_max_dsecs)
			softc->fw_reset_max_dsecs = BNXT_DFLT_FW_RST_MAX_DSECS;
		if (EVENT_DATA1_RESET_NOTIFY_FW_ACTIVATION(data1)) {
			set_bit(BNXT_STATE_FW_ACTIVATE_RESET, &softc->state);
		} else if (EVENT_DATA1_RESET_NOTIFY_FATAL(data1)) {
			type_str = "Fatal";
			softc->fw_health->fatalities++;
			set_bit(BNXT_STATE_FW_FATAL_COND, &softc->state);
		} else if (data2 && BNXT_FW_STATUS_HEALTHY !=
			   EVENT_DATA2_RESET_NOTIFY_FW_STATUS_CODE(data2)) {
			type_str = "Non-fatal";
			softc->fw_health->survivals++;
			set_bit(BNXT_STATE_FW_NON_FATAL_COND, &softc->state);
		}
		device_printf(softc->dev,
			   "%s firmware reset event, data1: 0x%x, data2: 0x%x, min wait %u ms, max wait %u ms\n",
			   type_str, data1, data2,
			   softc->fw_reset_min_dsecs * 100,
			   softc->fw_reset_max_dsecs * 100);
		set_bit(BNXT_FW_RESET_NOTIFY_SP_EVENT, &softc->sp_event);
		break;
	}
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_ERROR_RECOVERY: {
		fw_health = softc->fw_health;
		status_desc = "healthy";
		u32 status;

		if (!fw_health)
			goto async_event_process_exit;

		if (!EVENT_DATA1_RECOVERY_ENABLED(data1)) {
			fw_health->enabled = false;
			device_printf(softc->dev, "Driver recovery watchdog is disabled\n");
			break;
		}
		fw_health->primary = EVENT_DATA1_RECOVERY_MASTER_FUNC(data1);
		fw_health->tmr_multiplier =
			DIV_ROUND_UP(fw_health->polling_dsecs * HZ,
				     HZ * 10);
		fw_health->tmr_counter = fw_health->tmr_multiplier;
		if (!fw_health->enabled)
			fw_health->last_fw_heartbeat =
				bnxt_fw_health_readl(softc, BNXT_FW_HEARTBEAT_REG);
		fw_health->last_fw_reset_cnt =
			bnxt_fw_health_readl(softc, BNXT_FW_RESET_CNT_REG);
		status = bnxt_fw_health_readl(softc, BNXT_FW_HEALTH_REG);
		if (status != BNXT_FW_STATUS_HEALTHY)
			status_desc = "unhealthy";
		device_printf(softc->dev,
			   "Driver recovery watchdog, role: %s, firmware status: 0x%x (%s), resets: %u\n",
			   fw_health->primary ? "primary" : "backup", status,
			   status_desc, fw_health->last_fw_reset_cnt);
		if (!fw_health->enabled) {
			/* Make sure tmr_counter is set and seen by
			 * bnxt_health_check() before setting enabled
			 */
			smp_mb();
			fw_health->enabled = true;
		}
		goto async_event_process_exit;
	}

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
		dev_dbg(softc->dev, "Unknown Async event completion type %u\n",
			async_id);
		break;
	}
	bnxt_queue_sp_work(softc);

async_event_process_exit:
	bnxt_ulp_async_events(softc, ae);
}

static void
bnxt_def_cp_task(void *context, int pending)
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
		case CMPL_BASE_TYPE_RX_L2_V3:
		case CMPL_BASE_TYPE_RX_AGG:
		case CMPL_BASE_TYPE_RX_TPA_START:
		case CMPL_BASE_TYPE_RX_TPA_START_V3:
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
			dev_dbg(softc->dev, "Unhandled Async event completion type %u\n",
				type);
			break;
		default:
			dev_dbg(softc->dev, "Unknown Async event completion type %u\n",
				type);
			break;
		}
	}

	cpr->cons = last_cons;
	cpr->v_bit = last_v_bit;
	softc->db_ops.bnxt_db_rx_cq(cpr, 1);
}

uint8_t
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
	case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_200GB:
		return IF_Gbps(200);
	case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_400GB:
		return IF_Gbps(400);
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
