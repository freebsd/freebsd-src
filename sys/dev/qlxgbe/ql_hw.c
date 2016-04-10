/*
 * Copyright (c) 2013-2016 Qlogic Corporation
 * All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * File: ql_hw.c
 * Author : David C Somayajulu, Qlogic Corporation, Aliso Viejo, CA 92656.
 * Content: Contains Hardware dependant functions
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "ql_os.h"
#include "ql_hw.h"
#include "ql_def.h"
#include "ql_inline.h"
#include "ql_ver.h"
#include "ql_glbl.h"
#include "ql_dbg.h"

/*
 * Static Functions
 */

static void qla_del_rcv_cntxt(qla_host_t *ha);
static int qla_init_rcv_cntxt(qla_host_t *ha);
static void qla_del_xmt_cntxt(qla_host_t *ha);
static int qla_init_xmt_cntxt(qla_host_t *ha);
static void qla_hw_tx_done_locked(qla_host_t *ha, uint32_t txr_idx);
static int qla_mbx_cmd(qla_host_t *ha, uint32_t *h_mbox, uint32_t n_hmbox,
	uint32_t *fw_mbox, uint32_t n_fwmbox, uint32_t no_pause);
static int qla_config_intr_cntxt(qla_host_t *ha, uint32_t start_idx,
	uint32_t num_intrs, uint32_t create);
static int qla_config_rss(qla_host_t *ha, uint16_t cntxt_id);
static int qla_config_intr_coalesce(qla_host_t *ha, uint16_t cntxt_id,
	int tenable, int rcv);
static int qla_set_mac_rcv_mode(qla_host_t *ha, uint32_t mode);
static int qla_link_event_req(qla_host_t *ha, uint16_t cntxt_id);

static int qla_tx_tso(qla_host_t *ha, struct mbuf *mp, q80_tx_cmd_t *tx_cmd,
		uint8_t *hdr);
static int qla_hw_add_all_mcast(qla_host_t *ha);
static int qla_hw_del_all_mcast(qla_host_t *ha);
static int qla_add_rcv_rings(qla_host_t *ha, uint32_t sds_idx, uint32_t nsds);

static int qla_init_nic_func(qla_host_t *ha);
static int qla_stop_nic_func(qla_host_t *ha);
static int qla_query_fw_dcbx_caps(qla_host_t *ha);
static int qla_set_port_config(qla_host_t *ha, uint32_t cfg_bits);
static int qla_get_port_config(qla_host_t *ha, uint32_t *cfg_bits);
static void qla_get_quick_stats(qla_host_t *ha);

static int qla_minidump_init(qla_host_t *ha);
static void qla_minidump_free(qla_host_t *ha);


static int
qla_sysctl_get_drvr_stats(SYSCTL_HANDLER_ARGS)
{
        int err = 0, ret;
        qla_host_t *ha;
	uint32_t i;

        err = sysctl_handle_int(oidp, &ret, 0, req);

        if (err || !req->newptr)
                return (err);

        if (ret == 1) {

                ha = (qla_host_t *)arg1;

		for (i = 0; i < ha->hw.num_sds_rings; i++) 
			device_printf(ha->pci_dev,
				"%s: sds_ring[%d] = %p\n", __func__,i,
				(void *)ha->hw.sds[i].intr_count);

		for (i = 0; i < ha->hw.num_tx_rings; i++) 
			device_printf(ha->pci_dev,
				"%s: tx[%d] = %p\n", __func__,i,
				(void *)ha->tx_ring[i].count);

		for (i = 0; i < ha->hw.num_rds_rings; i++)
			device_printf(ha->pci_dev,
				"%s: rds_ring[%d] = %p\n", __func__,i,
				(void *)ha->hw.rds[i].count);

		device_printf(ha->pci_dev, "%s: lro_pkt_count = %p\n", __func__,
			(void *)ha->lro_pkt_count);

		device_printf(ha->pci_dev, "%s: lro_bytes = %p\n", __func__,
			(void *)ha->lro_bytes);

#ifdef QL_ENABLE_ISCSI_TLV
		device_printf(ha->pci_dev, "%s: iscsi_pkts = %p\n", __func__,
			(void *)ha->hw.iscsi_pkt_count);
#endif /* #ifdef QL_ENABLE_ISCSI_TLV */

	}
	return (err);
}

static int
qla_sysctl_get_quick_stats(SYSCTL_HANDLER_ARGS)
{
	int err, ret = 0;
	qla_host_t *ha;

	err = sysctl_handle_int(oidp, &ret, 0, req);

	if (err || !req->newptr)
		return (err);

	if (ret == 1) {
		ha = (qla_host_t *)arg1;
		qla_get_quick_stats(ha);
	}
	return (err);
}

#ifdef QL_DBG

static void
qla_stop_pegs(qla_host_t *ha)
{
        uint32_t val = 1;

        ql_rdwr_indreg32(ha, Q8_CRB_PEG_0, &val, 0);
        ql_rdwr_indreg32(ha, Q8_CRB_PEG_1, &val, 0);
        ql_rdwr_indreg32(ha, Q8_CRB_PEG_2, &val, 0);
        ql_rdwr_indreg32(ha, Q8_CRB_PEG_3, &val, 0);
        ql_rdwr_indreg32(ha, Q8_CRB_PEG_4, &val, 0);
        device_printf(ha->pci_dev, "%s PEGS HALTED!!!!!\n", __func__);
}

static int
qla_sysctl_stop_pegs(SYSCTL_HANDLER_ARGS)
{
	int err, ret = 0;
	qla_host_t *ha;
	
	err = sysctl_handle_int(oidp, &ret, 0, req);


	if (err || !req->newptr)
		return (err);

	if (ret == 1) {
		ha = (qla_host_t *)arg1;
		(void)QLA_LOCK(ha, __func__, 0);
		qla_stop_pegs(ha);	
		QLA_UNLOCK(ha, __func__);
	}

	return err;
}
#endif /* #ifdef QL_DBG */

static int
qla_validate_set_port_cfg_bit(uint32_t bits)
{
        if ((bits & 0xF) > 1)
                return (-1);

        if (((bits >> 4) & 0xF) > 2)
                return (-1);

        if (((bits >> 8) & 0xF) > 2)
                return (-1);

        return (0);
}

static int
qla_sysctl_port_cfg(SYSCTL_HANDLER_ARGS)
{
        int err, ret = 0;
        qla_host_t *ha;
        uint32_t cfg_bits;

        err = sysctl_handle_int(oidp, &ret, 0, req);

        if (err || !req->newptr)
                return (err);

        if ((qla_validate_set_port_cfg_bit((uint32_t)ret) == 0)) {

                ha = (qla_host_t *)arg1;

                err = qla_get_port_config(ha, &cfg_bits);

                if (err)
                        goto qla_sysctl_set_port_cfg_exit;

                if (ret & 0x1) {
                        cfg_bits |= Q8_PORT_CFG_BITS_DCBX_ENABLE;
                } else {
                        cfg_bits &= ~Q8_PORT_CFG_BITS_DCBX_ENABLE;
                }

                ret = ret >> 4;
                cfg_bits &= ~Q8_PORT_CFG_BITS_PAUSE_CFG_MASK;

                if ((ret & 0xF) == 0) {
                        cfg_bits |= Q8_PORT_CFG_BITS_PAUSE_DISABLED;
                } else if ((ret & 0xF) == 1){
                        cfg_bits |= Q8_PORT_CFG_BITS_PAUSE_STD;
                } else {
                        cfg_bits |= Q8_PORT_CFG_BITS_PAUSE_PPM;
                }

                ret = ret >> 4;
                cfg_bits &= ~Q8_PORT_CFG_BITS_STDPAUSE_DIR_MASK;

                if (ret == 0) {
                        cfg_bits |= Q8_PORT_CFG_BITS_STDPAUSE_XMT_RCV;
                } else if (ret == 1){
                        cfg_bits |= Q8_PORT_CFG_BITS_STDPAUSE_XMT;
                } else {
                        cfg_bits |= Q8_PORT_CFG_BITS_STDPAUSE_RCV;
                }

                err = qla_set_port_config(ha, cfg_bits);
        } else {
                ha = (qla_host_t *)arg1;

                err = qla_get_port_config(ha, &cfg_bits);
        }

qla_sysctl_set_port_cfg_exit:
        return err;
}

/*
 * Name: ql_hw_add_sysctls
 * Function: Add P3Plus specific sysctls
 */
void
ql_hw_add_sysctls(qla_host_t *ha)
{
        device_t	dev;

        dev = ha->pci_dev;

	ha->hw.num_sds_rings = MAX_SDS_RINGS;
	ha->hw.num_rds_rings = MAX_RDS_RINGS;
	ha->hw.num_tx_rings = NUM_TX_RINGS;

	SYSCTL_ADD_UINT(device_get_sysctl_ctx(dev),
		SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
		OID_AUTO, "num_rds_rings", CTLFLAG_RD, &ha->hw.num_rds_rings,
		ha->hw.num_rds_rings, "Number of Rcv Descriptor Rings");

        SYSCTL_ADD_UINT(device_get_sysctl_ctx(dev),
                SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
                OID_AUTO, "num_sds_rings", CTLFLAG_RD, &ha->hw.num_sds_rings,
		ha->hw.num_sds_rings, "Number of Status Descriptor Rings");

        SYSCTL_ADD_UINT(device_get_sysctl_ctx(dev),
                SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
                OID_AUTO, "num_tx_rings", CTLFLAG_RD, &ha->hw.num_tx_rings,
		ha->hw.num_tx_rings, "Number of Transmit Rings");

        SYSCTL_ADD_UINT(device_get_sysctl_ctx(dev),
                SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
                OID_AUTO, "tx_ring_index", CTLFLAG_RW, &ha->txr_idx,
		ha->txr_idx, "Tx Ring Used");

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
		SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
		OID_AUTO, "drvr_stats", CTLTYPE_INT | CTLFLAG_RW,
		(void *)ha, 0,
		qla_sysctl_get_drvr_stats, "I", "Driver Maintained Statistics");

        SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
                SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
                OID_AUTO, "quick_stats", CTLTYPE_INT | CTLFLAG_RW,
                (void *)ha, 0,
                qla_sysctl_get_quick_stats, "I", "Quick Statistics");

        SYSCTL_ADD_UINT(device_get_sysctl_ctx(dev),
                SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
                OID_AUTO, "max_tx_segs", CTLFLAG_RD, &ha->hw.max_tx_segs,
		ha->hw.max_tx_segs, "Max # of Segments in a non-TSO pkt");

	ha->hw.sds_cidx_thres = 32;
        SYSCTL_ADD_UINT(device_get_sysctl_ctx(dev),
                SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
                OID_AUTO, "sds_cidx_thres", CTLFLAG_RW, &ha->hw.sds_cidx_thres,
		ha->hw.sds_cidx_thres,
		"Number of SDS entries to process before updating"
		" SDS Ring Consumer Index");

	ha->hw.rds_pidx_thres = 32;
        SYSCTL_ADD_UINT(device_get_sysctl_ctx(dev),
                SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
                OID_AUTO, "rds_pidx_thres", CTLFLAG_RW, &ha->hw.rds_pidx_thres,
		ha->hw.rds_pidx_thres,
		"Number of Rcv Rings Entries to post before updating"
		" RDS Ring Producer Index");

        ha->hw.rcv_intr_coalesce = (3 << 16) | 256;
        SYSCTL_ADD_UINT(device_get_sysctl_ctx(dev),
                SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
                OID_AUTO, "rcv_intr_coalesce", CTLFLAG_RW,
                &ha->hw.rcv_intr_coalesce,
                ha->hw.rcv_intr_coalesce,
                "Rcv Intr Coalescing Parameters\n"
                "\tbits 15:0 max packets\n"
                "\tbits 31:16 max micro-seconds to wait\n"
                "\tplease run\n"
                "\tifconfig <if> down && ifconfig <if> up\n"
                "\tto take effect \n");

        ha->hw.xmt_intr_coalesce = (64 << 16) | 64;
        SYSCTL_ADD_UINT(device_get_sysctl_ctx(dev),
                SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
                OID_AUTO, "xmt_intr_coalesce", CTLFLAG_RW,
                &ha->hw.xmt_intr_coalesce,
                ha->hw.xmt_intr_coalesce,
                "Xmt Intr Coalescing Parameters\n"
                "\tbits 15:0 max packets\n"
                "\tbits 31:16 max micro-seconds to wait\n"
                "\tplease run\n"
                "\tifconfig <if> down && ifconfig <if> up\n"
                "\tto take effect \n");

        SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
                SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
                OID_AUTO, "port_cfg", CTLTYPE_INT | CTLFLAG_RW,
                (void *)ha, 0,
                qla_sysctl_port_cfg, "I",
                        "Set Port Configuration if values below "
                        "otherwise Get Port Configuration\n"
                        "\tBits 0-3 ; 1 = DCBX Enable; 0 = DCBX Disable\n"
                        "\tBits 4-7 : 0 = no pause; 1 = std ; 2 = ppm \n"
                        "\tBits 8-11: std pause cfg; 0 = xmt and rcv;"
                        " 1 = xmt only; 2 = rcv only;\n"
                );

        ha->hw.enable_9kb = 1;

        SYSCTL_ADD_UINT(device_get_sysctl_ctx(dev),
                SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
                OID_AUTO, "enable_9kb", CTLFLAG_RW, &ha->hw.enable_9kb,
                ha->hw.enable_9kb, "Enable 9Kbyte Buffers when MTU = 9000");

	ha->hw.mdump_active = 0;
        SYSCTL_ADD_UINT(device_get_sysctl_ctx(dev),
                SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
                OID_AUTO, "minidump_active", CTLFLAG_RW, &ha->hw.mdump_active,
		ha->hw.mdump_active,
		"Minidump Utility is Active \n"
		"\t 0 = Minidump Utility is not active\n"
		"\t 1 = Minidump Utility is retrieved on this port\n"
		"\t 2 = Minidump Utility is retrieved on the other port\n");

	ha->hw.mdump_start = 0;
        SYSCTL_ADD_UINT(device_get_sysctl_ctx(dev),
                SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
                OID_AUTO, "minidump_start", CTLFLAG_RW,
		&ha->hw.mdump_start, ha->hw.mdump_start,
		"Minidump Utility can start minidump process");
#ifdef QL_DBG

	ha->err_inject = 0;
        SYSCTL_ADD_UINT(device_get_sysctl_ctx(dev),
                SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
                OID_AUTO, "err_inject",
                CTLFLAG_RW, &ha->err_inject, ha->err_inject,
                "Error to be injected\n"
                "\t\t\t 0: No Errors\n"
                "\t\t\t 1: rcv: rxb struct invalid\n"
                "\t\t\t 2: rcv: mp == NULL\n"
                "\t\t\t 3: lro: rxb struct invalid\n"
                "\t\t\t 4: lro: mp == NULL\n"
                "\t\t\t 5: rcv: num handles invalid\n"
                "\t\t\t 6: reg: indirect reg rd_wr failure\n"
                "\t\t\t 7: ocm: offchip memory rd_wr failure\n"
                "\t\t\t 8: mbx: mailbox command failure\n"
                "\t\t\t 9: heartbeat failure\n"
                "\t\t\t A: temperature failure\n" );

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
                SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
                OID_AUTO, "peg_stop", CTLTYPE_INT | CTLFLAG_RW,
                (void *)ha, 0,
                qla_sysctl_stop_pegs, "I", "Peg Stop");

#endif /* #ifdef QL_DBG */

        ha->hw.user_pri_nic = 0;
        SYSCTL_ADD_UINT(device_get_sysctl_ctx(dev),
                SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
                OID_AUTO, "user_pri_nic", CTLFLAG_RW, &ha->hw.user_pri_nic,
                ha->hw.user_pri_nic,
                "VLAN Tag User Priority for Normal Ethernet Packets");

        ha->hw.user_pri_iscsi = 4;
        SYSCTL_ADD_UINT(device_get_sysctl_ctx(dev),
                SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
                OID_AUTO, "user_pri_iscsi", CTLFLAG_RW, &ha->hw.user_pri_iscsi,
                ha->hw.user_pri_iscsi,
                "VLAN Tag User Priority for iSCSI Packets");

}

void
ql_hw_link_status(qla_host_t *ha)
{
	device_printf(ha->pci_dev, "cable_oui\t\t 0x%08x\n", ha->hw.cable_oui);

	if (ha->hw.link_up) {
		device_printf(ha->pci_dev, "link Up\n");
	} else {
		device_printf(ha->pci_dev, "link Down\n");
	}

	if (ha->hw.flags.fduplex) {
		device_printf(ha->pci_dev, "Full Duplex\n");
	} else {
		device_printf(ha->pci_dev, "Half Duplex\n");
	}

	if (ha->hw.flags.autoneg) {
		device_printf(ha->pci_dev, "Auto Negotiation Enabled\n");
	} else {
		device_printf(ha->pci_dev, "Auto Negotiation Disabled\n");
	}

	switch (ha->hw.link_speed) {
	case 0x710:
		device_printf(ha->pci_dev, "link speed\t\t 10Gps\n");
		break;

	case 0x3E8:
		device_printf(ha->pci_dev, "link speed\t\t 1Gps\n");
		break;

	case 0x64:
		device_printf(ha->pci_dev, "link speed\t\t 100Mbps\n");
		break;

	default:
		device_printf(ha->pci_dev, "link speed\t\t Unknown\n");
		break;
	}

	switch (ha->hw.module_type) {

	case 0x01:
		device_printf(ha->pci_dev, "Module Type 10GBase-LRM\n");
		break;

	case 0x02:
		device_printf(ha->pci_dev, "Module Type 10GBase-LR\n");
		break;

	case 0x03:
		device_printf(ha->pci_dev, "Module Type 10GBase-SR\n");
		break;

	case 0x04:
		device_printf(ha->pci_dev,
			"Module Type 10GE Passive Copper(Compliant)[%d m]\n",
			ha->hw.cable_length);
		break;

	case 0x05:
		device_printf(ha->pci_dev, "Module Type 10GE Active"
			" Limiting Copper(Compliant)[%d m]\n",
			ha->hw.cable_length);
		break;

	case 0x06:
		device_printf(ha->pci_dev,
			"Module Type 10GE Passive Copper"
			" (Legacy, Best Effort)[%d m]\n",
			ha->hw.cable_length);
		break;

	case 0x07:
		device_printf(ha->pci_dev, "Module Type 1000Base-SX\n");
		break;

	case 0x08:
		device_printf(ha->pci_dev, "Module Type 1000Base-LX\n");
		break;

	case 0x09:
		device_printf(ha->pci_dev, "Module Type 1000Base-CX\n");
		break;

	case 0x0A:
		device_printf(ha->pci_dev, "Module Type 1000Base-T\n");
		break;

	case 0x0B:
		device_printf(ha->pci_dev, "Module Type 1GE Passive Copper"
			"(Legacy, Best Effort)\n");
		break;

	default:
		device_printf(ha->pci_dev, "Unknown Module Type 0x%x\n",
			ha->hw.module_type);
		break;
	}

	if (ha->hw.link_faults == 1)
		device_printf(ha->pci_dev, "SFP Power Fault\n");
}

/*
 * Name: ql_free_dma
 * Function: Frees the DMA'able memory allocated in ql_alloc_dma()
 */
void
ql_free_dma(qla_host_t *ha)
{
	uint32_t i;

        if (ha->hw.dma_buf.flags.sds_ring) {
		for (i = 0; i < ha->hw.num_sds_rings; i++) {
			ql_free_dmabuf(ha, &ha->hw.dma_buf.sds_ring[i]);
		}
        	ha->hw.dma_buf.flags.sds_ring = 0;
	}

        if (ha->hw.dma_buf.flags.rds_ring) {
		for (i = 0; i < ha->hw.num_rds_rings; i++) {
			ql_free_dmabuf(ha, &ha->hw.dma_buf.rds_ring[i]);
		}
        	ha->hw.dma_buf.flags.rds_ring = 0;
	}

        if (ha->hw.dma_buf.flags.tx_ring) {
		ql_free_dmabuf(ha, &ha->hw.dma_buf.tx_ring);
        	ha->hw.dma_buf.flags.tx_ring = 0;
	}
	qla_minidump_free(ha);
}

/*
 * Name: ql_alloc_dma
 * Function: Allocates DMA'able memory for Tx/Rx Rings, Tx/Rx Contexts.
 */
int
ql_alloc_dma(qla_host_t *ha)
{
        device_t                dev;
	uint32_t		i, j, size, tx_ring_size;
	qla_hw_t		*hw;
	qla_hw_tx_cntxt_t	*tx_cntxt;
	uint8_t			*vaddr;
	bus_addr_t		paddr;

        dev = ha->pci_dev;

        QL_DPRINT2(ha, (dev, "%s: enter\n", __func__));

	hw = &ha->hw;
	/*
	 * Allocate Transmit Ring
	 */
	tx_ring_size = (sizeof(q80_tx_cmd_t) * NUM_TX_DESCRIPTORS);
	size = (tx_ring_size * ha->hw.num_tx_rings);

	hw->dma_buf.tx_ring.alignment = 8;
	hw->dma_buf.tx_ring.size = size + PAGE_SIZE;
	
        if (ql_alloc_dmabuf(ha, &hw->dma_buf.tx_ring)) {
                device_printf(dev, "%s: tx ring alloc failed\n", __func__);
                goto ql_alloc_dma_exit;
        }

	vaddr = (uint8_t *)hw->dma_buf.tx_ring.dma_b;
	paddr = hw->dma_buf.tx_ring.dma_addr;
	
	for (i = 0; i < ha->hw.num_tx_rings; i++) {
		tx_cntxt = (qla_hw_tx_cntxt_t *)&hw->tx_cntxt[i];

		tx_cntxt->tx_ring_base = (q80_tx_cmd_t *)vaddr;
		tx_cntxt->tx_ring_paddr = paddr;

		vaddr += tx_ring_size;
		paddr += tx_ring_size;
	}

	for (i = 0; i < ha->hw.num_tx_rings; i++) {
		tx_cntxt = (qla_hw_tx_cntxt_t *)&hw->tx_cntxt[i];

		tx_cntxt->tx_cons = (uint32_t *)vaddr;
		tx_cntxt->tx_cons_paddr = paddr;

		vaddr += sizeof (uint32_t);
		paddr += sizeof (uint32_t);
	}

        ha->hw.dma_buf.flags.tx_ring = 1;

	QL_DPRINT2(ha, (dev, "%s: tx_ring phys %p virt %p\n",
		__func__, (void *)(hw->dma_buf.tx_ring.dma_addr),
		hw->dma_buf.tx_ring.dma_b));
	/*
	 * Allocate Receive Descriptor Rings
	 */

	for (i = 0; i < hw->num_rds_rings; i++) {

		hw->dma_buf.rds_ring[i].alignment = 8;
		hw->dma_buf.rds_ring[i].size =
			(sizeof(q80_recv_desc_t)) * NUM_RX_DESCRIPTORS;

		if (ql_alloc_dmabuf(ha, &hw->dma_buf.rds_ring[i])) {
			device_printf(dev, "%s: rds ring[%d] alloc failed\n",
				__func__, i);

			for (j = 0; j < i; j++)
				ql_free_dmabuf(ha, &hw->dma_buf.rds_ring[j]);

			goto ql_alloc_dma_exit;
		}
		QL_DPRINT4(ha, (dev, "%s: rx_ring[%d] phys %p virt %p\n",
			__func__, i, (void *)(hw->dma_buf.rds_ring[i].dma_addr),
			hw->dma_buf.rds_ring[i].dma_b));
	}

	hw->dma_buf.flags.rds_ring = 1;

	/*
	 * Allocate Status Descriptor Rings
	 */

	for (i = 0; i < hw->num_sds_rings; i++) {
		hw->dma_buf.sds_ring[i].alignment = 8;
		hw->dma_buf.sds_ring[i].size =
			(sizeof(q80_stat_desc_t)) * NUM_STATUS_DESCRIPTORS;

		if (ql_alloc_dmabuf(ha, &hw->dma_buf.sds_ring[i])) {
			device_printf(dev, "%s: sds ring alloc failed\n",
				__func__);

			for (j = 0; j < i; j++)
				ql_free_dmabuf(ha, &hw->dma_buf.sds_ring[j]);

			goto ql_alloc_dma_exit;
		}
		QL_DPRINT4(ha, (dev, "%s: sds_ring[%d] phys %p virt %p\n",
			__func__, i,
			(void *)(hw->dma_buf.sds_ring[i].dma_addr),
			hw->dma_buf.sds_ring[i].dma_b));
	}
	for (i = 0; i < hw->num_sds_rings; i++) {
		hw->sds[i].sds_ring_base =
			(q80_stat_desc_t *)hw->dma_buf.sds_ring[i].dma_b;
	}

	hw->dma_buf.flags.sds_ring = 1;

	return 0;

ql_alloc_dma_exit:
	ql_free_dma(ha);
	return -1;
}

#define Q8_MBX_MSEC_DELAY	5000

static int
qla_mbx_cmd(qla_host_t *ha, uint32_t *h_mbox, uint32_t n_hmbox,
	uint32_t *fw_mbox, uint32_t n_fwmbox, uint32_t no_pause)
{
	uint32_t i;
	uint32_t data;
	int ret = 0;

	if (QL_ERR_INJECT(ha, INJCT_MBX_CMD_FAILURE)) {
		ret = -3;
		ha->qla_initiate_recovery = 1;
		goto exit_qla_mbx_cmd;
	}

	if (no_pause)
		i = 1000;
	else
		i = Q8_MBX_MSEC_DELAY;

	while (i) {
		data = READ_REG32(ha, Q8_HOST_MBOX_CNTRL);
		if (data == 0)
			break;
		if (no_pause) {
			DELAY(1000);
		} else {
			qla_mdelay(__func__, 1);
		}
		i--;
	}

	if (i == 0) {
		device_printf(ha->pci_dev, "%s: host_mbx_cntrl 0x%08x\n",
			__func__, data);
		ret = -1;
		ha->qla_initiate_recovery = 1;
		goto exit_qla_mbx_cmd;
	}

	for (i = 0; i < n_hmbox; i++) {
		WRITE_REG32(ha, (Q8_HOST_MBOX0 + (i << 2)), *h_mbox);
		h_mbox++;
	}

	WRITE_REG32(ha, Q8_HOST_MBOX_CNTRL, 0x1);


	i = Q8_MBX_MSEC_DELAY;
	while (i) {
		data = READ_REG32(ha, Q8_FW_MBOX_CNTRL);

		if ((data & 0x3) == 1) {
			data = READ_REG32(ha, Q8_FW_MBOX0);
			if ((data & 0xF000) != 0x8000)
				break;
		}
		if (no_pause) {
			DELAY(1000);
		} else {
			qla_mdelay(__func__, 1);
		}
		i--;
	}
	if (i == 0) {
		device_printf(ha->pci_dev, "%s: fw_mbx_cntrl 0x%08x\n",
			__func__, data);
		ret = -2;
		ha->qla_initiate_recovery = 1;
		goto exit_qla_mbx_cmd;
	}

	for (i = 0; i < n_fwmbox; i++) {
		*fw_mbox++ = READ_REG32(ha, (Q8_FW_MBOX0 + (i << 2)));
	}

	WRITE_REG32(ha, Q8_FW_MBOX_CNTRL, 0x0);
	WRITE_REG32(ha, ha->hw.mbx_intr_mask_offset, 0x0);

exit_qla_mbx_cmd:
	return (ret);
}

int
qla_get_nic_partition(qla_host_t *ha, uint32_t *supports_9kb,
	uint32_t *num_rcvq)
{
	uint32_t *mbox, err;
	device_t dev = ha->pci_dev;

	bzero(ha->hw.mbox, (sizeof (uint32_t) * Q8_NUM_MBOX));

	mbox = ha->hw.mbox;

	mbox[0] = Q8_MBX_GET_NIC_PARTITION | (0x2 << 16) | (0x2 << 29);	

	if (qla_mbx_cmd(ha, mbox, 2, mbox, 19, 0)) {
		device_printf(dev, "%s: failed0\n", __func__);
		return (-1);
	}
	err = mbox[0] >> 25; 

	if (supports_9kb != NULL) {
		if (mbox[16] & 0x80) /* bit 7 of mbox 16 */
			*supports_9kb = 1;
		else
			*supports_9kb = 0;
	}

	if (num_rcvq != NULL)
		*num_rcvq =  ((mbox[6] >> 16) & 0xFFFF);

	if ((err != 1) && (err != 0)) {
		device_printf(dev, "%s: failed1 [0x%08x]\n", __func__, err);
		return (-1);
	}
	return 0;
}

static int
qla_config_intr_cntxt(qla_host_t *ha, uint32_t start_idx, uint32_t num_intrs,
	uint32_t create)
{
	uint32_t i, err;
	device_t dev = ha->pci_dev;
	q80_config_intr_t *c_intr;
	q80_config_intr_rsp_t *c_intr_rsp;

	c_intr = (q80_config_intr_t *)ha->hw.mbox;
	bzero(c_intr, (sizeof (q80_config_intr_t)));

	c_intr->opcode = Q8_MBX_CONFIG_INTR;

	c_intr->count_version = (sizeof (q80_config_intr_t) >> 2);
	c_intr->count_version |= Q8_MBX_CMD_VERSION;

	c_intr->nentries = num_intrs;

	for (i = 0; i < num_intrs; i++) {
		if (create) {
			c_intr->intr[i].cmd_type = Q8_MBX_CONFIG_INTR_CREATE;
			c_intr->intr[i].msix_index = start_idx + 1 + i;
		} else {
			c_intr->intr[i].cmd_type = Q8_MBX_CONFIG_INTR_DELETE;
			c_intr->intr[i].msix_index =
				ha->hw.intr_id[(start_idx + i)];
		}

		c_intr->intr[i].cmd_type |= Q8_MBX_CONFIG_INTR_TYPE_MSI_X;
	}

	if (qla_mbx_cmd(ha, (uint32_t *)c_intr,
		(sizeof (q80_config_intr_t) >> 2),
		ha->hw.mbox, (sizeof (q80_config_intr_rsp_t) >> 2), 0)) {
		device_printf(dev, "%s: failed0\n", __func__);
		return (-1);
	}

	c_intr_rsp = (q80_config_intr_rsp_t *)ha->hw.mbox;

	err = Q8_MBX_RSP_STATUS(c_intr_rsp->regcnt_status);

	if (err) {
		device_printf(dev, "%s: failed1 [0x%08x, %d]\n", __func__, err,
			c_intr_rsp->nentries);

		for (i = 0; i < c_intr_rsp->nentries; i++) {
			device_printf(dev, "%s: [%d]:[0x%x 0x%x 0x%x]\n",
				__func__, i, 
				c_intr_rsp->intr[i].status,
				c_intr_rsp->intr[i].intr_id,
				c_intr_rsp->intr[i].intr_src);
		}

		return (-1);
	}

	for (i = 0; ((i < num_intrs) && create); i++) {
		if (!c_intr_rsp->intr[i].status) {
			ha->hw.intr_id[(start_idx + i)] =
				c_intr_rsp->intr[i].intr_id;
			ha->hw.intr_src[(start_idx + i)] =
				c_intr_rsp->intr[i].intr_src;
		}
	}

	return (0);
}

/*
 * Name: qla_config_rss
 * Function: Configure RSS for the context/interface.
 */
static const uint64_t rss_key[] = { 0xbeac01fa6a42b73bULL,
			0x8030f20c77cb2da3ULL,
			0xae7b30b4d0ca2bcbULL, 0x43a38fb04167253dULL,
			0x255b0ec26d5a56daULL };

static int
qla_config_rss(qla_host_t *ha, uint16_t cntxt_id)
{
	q80_config_rss_t	*c_rss;
	q80_config_rss_rsp_t	*c_rss_rsp;
	uint32_t		err, i;
	device_t		dev = ha->pci_dev;

	c_rss = (q80_config_rss_t *)ha->hw.mbox;
	bzero(c_rss, (sizeof (q80_config_rss_t)));

	c_rss->opcode = Q8_MBX_CONFIG_RSS;

	c_rss->count_version = (sizeof (q80_config_rss_t) >> 2);
	c_rss->count_version |= Q8_MBX_CMD_VERSION;

	c_rss->hash_type = (Q8_MBX_RSS_HASH_TYPE_IPV4_TCP_IP |
				Q8_MBX_RSS_HASH_TYPE_IPV6_TCP_IP);
	//c_rss->hash_type = (Q8_MBX_RSS_HASH_TYPE_IPV4_TCP |
	//			Q8_MBX_RSS_HASH_TYPE_IPV6_TCP);

	c_rss->flags = Q8_MBX_RSS_FLAGS_ENABLE_RSS;
	c_rss->flags |= Q8_MBX_RSS_FLAGS_USE_IND_TABLE;

	c_rss->indtbl_mask = Q8_MBX_RSS_INDTBL_MASK;

	c_rss->indtbl_mask |= Q8_MBX_RSS_FLAGS_MULTI_RSS_VALID;
	c_rss->flags |= Q8_MBX_RSS_FLAGS_TYPE_CRSS;

	c_rss->cntxt_id = cntxt_id;

	for (i = 0; i < 5; i++) {
		c_rss->rss_key[i] = rss_key[i];
	}

	if (qla_mbx_cmd(ha, (uint32_t *)c_rss,
		(sizeof (q80_config_rss_t) >> 2),
		ha->hw.mbox, (sizeof(q80_config_rss_rsp_t) >> 2), 0)) {
		device_printf(dev, "%s: failed0\n", __func__);
		return (-1);
	}
	c_rss_rsp = (q80_config_rss_rsp_t *)ha->hw.mbox;

	err = Q8_MBX_RSP_STATUS(c_rss_rsp->regcnt_status);

	if (err) {
		device_printf(dev, "%s: failed1 [0x%08x]\n", __func__, err);
		return (-1);
	}
	return 0;
}

static int
qla_set_rss_ind_table(qla_host_t *ha, uint32_t start_idx, uint32_t count,
        uint16_t cntxt_id, uint8_t *ind_table)
{
        q80_config_rss_ind_table_t      *c_rss_ind;
        q80_config_rss_ind_table_rsp_t  *c_rss_ind_rsp;
        uint32_t                        err;
        device_t                        dev = ha->pci_dev;

	if ((count > Q8_RSS_IND_TBL_SIZE) ||
		((start_idx + count - 1) > Q8_RSS_IND_TBL_MAX_IDX)) {
		device_printf(dev, "%s: illegal count [%d, %d]\n", __func__,
			start_idx, count);
		return (-1);
	}

        c_rss_ind = (q80_config_rss_ind_table_t *)ha->hw.mbox;
        bzero(c_rss_ind, sizeof (q80_config_rss_ind_table_t));

        c_rss_ind->opcode = Q8_MBX_CONFIG_RSS_TABLE;
        c_rss_ind->count_version = (sizeof (q80_config_rss_ind_table_t) >> 2);
        c_rss_ind->count_version |= Q8_MBX_CMD_VERSION;

	c_rss_ind->start_idx = start_idx;
	c_rss_ind->end_idx = start_idx + count - 1;
	c_rss_ind->cntxt_id = cntxt_id;
	bcopy(ind_table, c_rss_ind->ind_table, count);

	if (qla_mbx_cmd(ha, (uint32_t *)c_rss_ind,
		(sizeof (q80_config_rss_ind_table_t) >> 2), ha->hw.mbox,
		(sizeof(q80_config_rss_ind_table_rsp_t) >> 2), 0)) {
		device_printf(dev, "%s: failed0\n", __func__);
		return (-1);
	}

	c_rss_ind_rsp = (q80_config_rss_ind_table_rsp_t *)ha->hw.mbox;
	err = Q8_MBX_RSP_STATUS(c_rss_ind_rsp->regcnt_status);

	if (err) {
		device_printf(dev, "%s: failed1 [0x%08x]\n", __func__, err);
		return (-1);
	}
	return 0;
}

/*
 * Name: qla_config_intr_coalesce
 * Function: Configure Interrupt Coalescing.
 */
static int
qla_config_intr_coalesce(qla_host_t *ha, uint16_t cntxt_id, int tenable,
	int rcv)
{
	q80_config_intr_coalesc_t	*intrc;
	q80_config_intr_coalesc_rsp_t	*intrc_rsp;
	uint32_t			err, i;
	device_t			dev = ha->pci_dev;
	
	intrc = (q80_config_intr_coalesc_t *)ha->hw.mbox;
	bzero(intrc, (sizeof (q80_config_intr_coalesc_t)));

	intrc->opcode = Q8_MBX_CONFIG_INTR_COALESCE;
	intrc->count_version = (sizeof (q80_config_intr_coalesc_t) >> 2);
	intrc->count_version |= Q8_MBX_CMD_VERSION;

	if (rcv) {
		intrc->flags = Q8_MBX_INTRC_FLAGS_RCV;
		intrc->max_pkts = ha->hw.rcv_intr_coalesce & 0xFFFF;
		intrc->max_mswait = (ha->hw.rcv_intr_coalesce >> 16) & 0xFFFF;
	} else {
		intrc->flags = Q8_MBX_INTRC_FLAGS_XMT;
		intrc->max_pkts = ha->hw.xmt_intr_coalesce & 0xFFFF;
		intrc->max_mswait = (ha->hw.xmt_intr_coalesce >> 16) & 0xFFFF;
	}

	intrc->cntxt_id = cntxt_id;

	if (tenable) {
		intrc->flags |= Q8_MBX_INTRC_FLAGS_PERIODIC;
		intrc->timer_type = Q8_MBX_INTRC_TIMER_PERIODIC;

		for (i = 0; i < ha->hw.num_sds_rings; i++) {
			intrc->sds_ring_mask |= (1 << i);
		}
		intrc->ms_timeout = 1000;
	}

	if (qla_mbx_cmd(ha, (uint32_t *)intrc,
		(sizeof (q80_config_intr_coalesc_t) >> 2),
		ha->hw.mbox, (sizeof(q80_config_intr_coalesc_rsp_t) >> 2), 0)) {
		device_printf(dev, "%s: failed0\n", __func__);
		return (-1);
	}
	intrc_rsp = (q80_config_intr_coalesc_rsp_t *)ha->hw.mbox;

	err = Q8_MBX_RSP_STATUS(intrc_rsp->regcnt_status);

	if (err) {
		device_printf(dev, "%s: failed1 [0x%08x]\n", __func__, err);
		return (-1);
	}
	
	return 0;
}


/*
 * Name: qla_config_mac_addr
 * Function: binds a MAC address to the context/interface.
 *	Can be unicast, multicast or broadcast.
 */
static int
qla_config_mac_addr(qla_host_t *ha, uint8_t *mac_addr, uint32_t add_mac)
{
	q80_config_mac_addr_t		*cmac;
	q80_config_mac_addr_rsp_t	*cmac_rsp;
	uint32_t			err;
	device_t			dev = ha->pci_dev;

	cmac = (q80_config_mac_addr_t *)ha->hw.mbox;
	bzero(cmac, (sizeof (q80_config_mac_addr_t)));

	cmac->opcode = Q8_MBX_CONFIG_MAC_ADDR;
	cmac->count_version = sizeof (q80_config_mac_addr_t) >> 2;
	cmac->count_version |= Q8_MBX_CMD_VERSION;

	if (add_mac) 
		cmac->cmd = Q8_MBX_CMAC_CMD_ADD_MAC_ADDR;
	else
		cmac->cmd = Q8_MBX_CMAC_CMD_DEL_MAC_ADDR;
		
	cmac->cmd |= Q8_MBX_CMAC_CMD_CAM_INGRESS;

	cmac->nmac_entries = 1;
	cmac->cntxt_id = ha->hw.rcv_cntxt_id;
	bcopy(mac_addr, cmac->mac_addr[0].addr, 6); 

	if (qla_mbx_cmd(ha, (uint32_t *)cmac,
		(sizeof (q80_config_mac_addr_t) >> 2),
		ha->hw.mbox, (sizeof(q80_config_mac_addr_rsp_t) >> 2), 1)) {
		device_printf(dev, "%s: %s failed0\n", __func__,
			(add_mac ? "Add" : "Del"));
		return (-1);
	}
	cmac_rsp = (q80_config_mac_addr_rsp_t *)ha->hw.mbox;

	err = Q8_MBX_RSP_STATUS(cmac_rsp->regcnt_status);

	if (err) {
		device_printf(dev, "%s: %s "
			"%02x:%02x:%02x:%02x:%02x:%02x failed1 [0x%08x]\n",
			__func__, (add_mac ? "Add" : "Del"),
			mac_addr[0], mac_addr[1], mac_addr[2],
			mac_addr[3], mac_addr[4], mac_addr[5], err);
		return (-1);
	}
	
	return 0;
}


/*
 * Name: qla_set_mac_rcv_mode
 * Function: Enable/Disable AllMulticast and Promiscous Modes.
 */
static int
qla_set_mac_rcv_mode(qla_host_t *ha, uint32_t mode)
{
	q80_config_mac_rcv_mode_t	*rcv_mode;
	uint32_t			err;
	q80_config_mac_rcv_mode_rsp_t	*rcv_mode_rsp;
	device_t			dev = ha->pci_dev;

	rcv_mode = (q80_config_mac_rcv_mode_t *)ha->hw.mbox;
	bzero(rcv_mode, (sizeof (q80_config_mac_rcv_mode_t)));

	rcv_mode->opcode = Q8_MBX_CONFIG_MAC_RX_MODE;
	rcv_mode->count_version = sizeof (q80_config_mac_rcv_mode_t) >> 2;
	rcv_mode->count_version |= Q8_MBX_CMD_VERSION;

	rcv_mode->mode = mode;

	rcv_mode->cntxt_id = ha->hw.rcv_cntxt_id;

	if (qla_mbx_cmd(ha, (uint32_t *)rcv_mode,
		(sizeof (q80_config_mac_rcv_mode_t) >> 2),
		ha->hw.mbox, (sizeof(q80_config_mac_rcv_mode_rsp_t) >> 2), 1)) {
		device_printf(dev, "%s: failed0\n", __func__);
		return (-1);
	}
	rcv_mode_rsp = (q80_config_mac_rcv_mode_rsp_t *)ha->hw.mbox;

	err = Q8_MBX_RSP_STATUS(rcv_mode_rsp->regcnt_status);

	if (err) {
		device_printf(dev, "%s: failed1 [0x%08x]\n", __func__, err);
		return (-1);
	}
	
	return 0;
}

int
ql_set_promisc(qla_host_t *ha)
{
	int ret;

	ha->hw.mac_rcv_mode |= Q8_MBX_MAC_RCV_PROMISC_ENABLE;
	ret = qla_set_mac_rcv_mode(ha, ha->hw.mac_rcv_mode);
	return (ret);
}

void
qla_reset_promisc(qla_host_t *ha)
{
	ha->hw.mac_rcv_mode &= ~Q8_MBX_MAC_RCV_PROMISC_ENABLE;
	(void)qla_set_mac_rcv_mode(ha, ha->hw.mac_rcv_mode);
}

int
ql_set_allmulti(qla_host_t *ha)
{
	int ret;

	ha->hw.mac_rcv_mode |= Q8_MBX_MAC_ALL_MULTI_ENABLE;
	ret = qla_set_mac_rcv_mode(ha, ha->hw.mac_rcv_mode);
	return (ret);
}

void
qla_reset_allmulti(qla_host_t *ha)
{
	ha->hw.mac_rcv_mode &= ~Q8_MBX_MAC_ALL_MULTI_ENABLE;
	(void)qla_set_mac_rcv_mode(ha, ha->hw.mac_rcv_mode);
}

/*
 * Name: ql_set_max_mtu
 * Function:
 *	Sets the maximum transfer unit size for the specified rcv context.
 */
int
ql_set_max_mtu(qla_host_t *ha, uint32_t mtu, uint16_t cntxt_id)
{
	device_t		dev;
	q80_set_max_mtu_t	*max_mtu;
	q80_set_max_mtu_rsp_t	*max_mtu_rsp;
	uint32_t		err;

	dev = ha->pci_dev;

	max_mtu = (q80_set_max_mtu_t *)ha->hw.mbox;
	bzero(max_mtu, (sizeof (q80_set_max_mtu_t)));

	max_mtu->opcode = Q8_MBX_SET_MAX_MTU;
	max_mtu->count_version = (sizeof (q80_set_max_mtu_t) >> 2);
	max_mtu->count_version |= Q8_MBX_CMD_VERSION;

	max_mtu->cntxt_id = cntxt_id;
	max_mtu->mtu = mtu;

        if (qla_mbx_cmd(ha, (uint32_t *)max_mtu,
		(sizeof (q80_set_max_mtu_t) >> 2),
                ha->hw.mbox, (sizeof (q80_set_max_mtu_rsp_t) >> 2), 1)) {
                device_printf(dev, "%s: failed\n", __func__);
                return -1;
        }

	max_mtu_rsp = (q80_set_max_mtu_rsp_t *)ha->hw.mbox;

        err = Q8_MBX_RSP_STATUS(max_mtu_rsp->regcnt_status);

        if (err) {
                device_printf(dev, "%s: failed [0x%08x]\n", __func__, err);
        }

	return 0;
}

static int
qla_link_event_req(qla_host_t *ha, uint16_t cntxt_id)
{
	device_t		dev;
	q80_link_event_t	*lnk;
	q80_link_event_rsp_t	*lnk_rsp;
	uint32_t		err;

	dev = ha->pci_dev;

	lnk = (q80_link_event_t *)ha->hw.mbox;
	bzero(lnk, (sizeof (q80_link_event_t)));

	lnk->opcode = Q8_MBX_LINK_EVENT_REQ;
	lnk->count_version = (sizeof (q80_link_event_t) >> 2);
	lnk->count_version |= Q8_MBX_CMD_VERSION;

	lnk->cntxt_id = cntxt_id;
	lnk->cmd = Q8_LINK_EVENT_CMD_ENABLE_ASYNC;

        if (qla_mbx_cmd(ha, (uint32_t *)lnk, (sizeof (q80_link_event_t) >> 2),
                ha->hw.mbox, (sizeof (q80_link_event_rsp_t) >> 2), 0)) {
                device_printf(dev, "%s: failed\n", __func__);
                return -1;
        }

	lnk_rsp = (q80_link_event_rsp_t *)ha->hw.mbox;

        err = Q8_MBX_RSP_STATUS(lnk_rsp->regcnt_status);

        if (err) {
                device_printf(dev, "%s: failed [0x%08x]\n", __func__, err);
        }

	return 0;
}

static int
qla_config_fw_lro(qla_host_t *ha, uint16_t cntxt_id)
{
	device_t		dev;
	q80_config_fw_lro_t	*fw_lro;
	q80_config_fw_lro_rsp_t	*fw_lro_rsp;
	uint32_t		err;

	dev = ha->pci_dev;

	fw_lro = (q80_config_fw_lro_t *)ha->hw.mbox;
	bzero(fw_lro, sizeof(q80_config_fw_lro_t));

	fw_lro->opcode = Q8_MBX_CONFIG_FW_LRO;
	fw_lro->count_version = (sizeof (q80_config_fw_lro_t) >> 2);
	fw_lro->count_version |= Q8_MBX_CMD_VERSION;

	fw_lro->flags |= Q8_MBX_FW_LRO_IPV4 | Q8_MBX_FW_LRO_IPV4_WO_DST_IP_CHK;
	fw_lro->flags |= Q8_MBX_FW_LRO_IPV6 | Q8_MBX_FW_LRO_IPV6_WO_DST_IP_CHK;

	fw_lro->cntxt_id = cntxt_id;

	if (qla_mbx_cmd(ha, (uint32_t *)fw_lro,
		(sizeof (q80_config_fw_lro_t) >> 2),
		ha->hw.mbox, (sizeof (q80_config_fw_lro_rsp_t) >> 2), 0)) {
		device_printf(dev, "%s: failed\n", __func__);
		return -1;
	}

	fw_lro_rsp = (q80_config_fw_lro_rsp_t *)ha->hw.mbox;

	err = Q8_MBX_RSP_STATUS(fw_lro_rsp->regcnt_status);

	if (err) {
		device_printf(dev, "%s: failed [0x%08x]\n", __func__, err);
	}

	return 0;
}

static void
qla_xmt_stats(qla_host_t *ha, q80_xmt_stats_t *xstat, int i)
{
	device_t dev = ha->pci_dev;

	if (i < ha->hw.num_tx_rings) {
		device_printf(dev, "%s[%d]: total_bytes\t\t%" PRIu64 "\n",
			__func__, i, xstat->total_bytes);
		device_printf(dev, "%s[%d]: total_pkts\t\t%" PRIu64 "\n",
			__func__, i, xstat->total_pkts);
		device_printf(dev, "%s[%d]: errors\t\t%" PRIu64 "\n",
			__func__, i, xstat->errors);
		device_printf(dev, "%s[%d]: pkts_dropped\t%" PRIu64 "\n",
			__func__, i, xstat->pkts_dropped);
		device_printf(dev, "%s[%d]: switch_pkts\t\t%" PRIu64 "\n",
			__func__, i, xstat->switch_pkts);
		device_printf(dev, "%s[%d]: num_buffers\t\t%" PRIu64 "\n",
			__func__, i, xstat->num_buffers);
	} else {
		device_printf(dev, "%s: total_bytes\t\t\t%" PRIu64 "\n",
			__func__, xstat->total_bytes);
		device_printf(dev, "%s: total_pkts\t\t\t%" PRIu64 "\n",
			__func__, xstat->total_pkts);
		device_printf(dev, "%s: errors\t\t\t%" PRIu64 "\n",
			__func__, xstat->errors);
		device_printf(dev, "%s: pkts_dropped\t\t\t%" PRIu64 "\n",
			__func__, xstat->pkts_dropped);
		device_printf(dev, "%s: switch_pkts\t\t\t%" PRIu64 "\n",
			__func__, xstat->switch_pkts);
		device_printf(dev, "%s: num_buffers\t\t\t%" PRIu64 "\n",
			__func__, xstat->num_buffers);
	}
}

static void
qla_rcv_stats(qla_host_t *ha, q80_rcv_stats_t *rstat)
{
	device_t dev = ha->pci_dev;

	device_printf(dev, "%s: total_bytes\t\t\t%" PRIu64 "\n", __func__,
		rstat->total_bytes);
	device_printf(dev, "%s: total_pkts\t\t\t%" PRIu64 "\n", __func__,
		rstat->total_pkts);
	device_printf(dev, "%s: lro_pkt_count\t\t%" PRIu64 "\n", __func__,
		rstat->lro_pkt_count);
	device_printf(dev, "%s: sw_pkt_count\t\t\t%" PRIu64 "\n", __func__,
		rstat->sw_pkt_count);
	device_printf(dev, "%s: ip_chksum_err\t\t%" PRIu64 "\n", __func__,
		rstat->ip_chksum_err);
	device_printf(dev, "%s: pkts_wo_acntxts\t\t%" PRIu64 "\n", __func__,
		rstat->pkts_wo_acntxts);
	device_printf(dev, "%s: pkts_dropped_no_sds_card\t%" PRIu64 "\n",
		__func__, rstat->pkts_dropped_no_sds_card);
	device_printf(dev, "%s: pkts_dropped_no_sds_host\t%" PRIu64 "\n",
		__func__, rstat->pkts_dropped_no_sds_host);
	device_printf(dev, "%s: oversized_pkts\t\t%" PRIu64 "\n", __func__,
		rstat->oversized_pkts);
	device_printf(dev, "%s: pkts_dropped_no_rds\t\t%" PRIu64 "\n",
		__func__, rstat->pkts_dropped_no_rds);
	device_printf(dev, "%s: unxpctd_mcast_pkts\t\t%" PRIu64 "\n",
		__func__, rstat->unxpctd_mcast_pkts);
	device_printf(dev, "%s: re1_fbq_error\t\t%" PRIu64 "\n", __func__,
		rstat->re1_fbq_error);
	device_printf(dev, "%s: invalid_mac_addr\t\t%" PRIu64 "\n", __func__,
		rstat->invalid_mac_addr);
	device_printf(dev, "%s: rds_prime_trys\t\t%" PRIu64 "\n", __func__,
		rstat->rds_prime_trys);
	device_printf(dev, "%s: rds_prime_success\t\t%" PRIu64 "\n", __func__,
		rstat->rds_prime_success);
	device_printf(dev, "%s: lro_flows_added\t\t%" PRIu64 "\n", __func__,
		rstat->lro_flows_added);
	device_printf(dev, "%s: lro_flows_deleted\t\t%" PRIu64 "\n", __func__,
		rstat->lro_flows_deleted);
	device_printf(dev, "%s: lro_flows_active\t\t%" PRIu64 "\n", __func__,
		rstat->lro_flows_active);
	device_printf(dev, "%s: pkts_droped_unknown\t\t%" PRIu64 "\n",
		__func__, rstat->pkts_droped_unknown);
}

static void
qla_mac_stats(qla_host_t *ha, q80_mac_stats_t *mstat)
{
	device_t dev = ha->pci_dev;

	device_printf(dev, "%s: xmt_frames\t\t\t%" PRIu64 "\n", __func__,
		mstat->xmt_frames);
	device_printf(dev, "%s: xmt_bytes\t\t\t%" PRIu64 "\n", __func__,
		mstat->xmt_bytes);
	device_printf(dev, "%s: xmt_mcast_pkts\t\t%" PRIu64 "\n", __func__,
		mstat->xmt_mcast_pkts);
	device_printf(dev, "%s: xmt_bcast_pkts\t\t%" PRIu64 "\n", __func__,
		mstat->xmt_bcast_pkts);
	device_printf(dev, "%s: xmt_pause_frames\t\t%" PRIu64 "\n", __func__,
		mstat->xmt_pause_frames);
	device_printf(dev, "%s: xmt_cntrl_pkts\t\t%" PRIu64 "\n", __func__,
		mstat->xmt_cntrl_pkts);
	device_printf(dev, "%s: xmt_pkt_lt_64bytes\t\t%" PRIu64 "\n",
		__func__, mstat->xmt_pkt_lt_64bytes);
	device_printf(dev, "%s: xmt_pkt_lt_127bytes\t\t%" PRIu64 "\n",
		__func__, mstat->xmt_pkt_lt_127bytes);
	device_printf(dev, "%s: xmt_pkt_lt_255bytes\t\t%" PRIu64 "\n",
		__func__, mstat->xmt_pkt_lt_255bytes);
	device_printf(dev, "%s: xmt_pkt_lt_511bytes\t\t%" PRIu64 "\n",
		__func__, mstat->xmt_pkt_lt_511bytes);
	device_printf(dev, "%s: xmt_pkt_lt_1023bytes\t\t%" PRIu64 "\n",
		__func__, mstat->xmt_pkt_lt_1023bytes);
	device_printf(dev, "%s: xmt_pkt_lt_1518bytes\t\t%" PRIu64 "\n",
		__func__, mstat->xmt_pkt_lt_1518bytes);
	device_printf(dev, "%s: xmt_pkt_gt_1518bytes\t\t%" PRIu64 "\n",
		__func__, mstat->xmt_pkt_gt_1518bytes);

	device_printf(dev, "%s: rcv_frames\t\t\t%" PRIu64 "\n", __func__,
		mstat->rcv_frames);
	device_printf(dev, "%s: rcv_bytes\t\t\t%" PRIu64 "\n", __func__,
		mstat->rcv_bytes);
	device_printf(dev, "%s: rcv_mcast_pkts\t\t%" PRIu64 "\n", __func__,
		mstat->rcv_mcast_pkts);
	device_printf(dev, "%s: rcv_bcast_pkts\t\t%" PRIu64 "\n", __func__,
		mstat->rcv_bcast_pkts);
	device_printf(dev, "%s: rcv_pause_frames\t\t%" PRIu64 "\n", __func__,
		mstat->rcv_pause_frames);
	device_printf(dev, "%s: rcv_cntrl_pkts\t\t%" PRIu64 "\n", __func__,
		mstat->rcv_cntrl_pkts);
	device_printf(dev, "%s: rcv_pkt_lt_64bytes\t\t%" PRIu64 "\n",
		__func__, mstat->rcv_pkt_lt_64bytes);
	device_printf(dev, "%s: rcv_pkt_lt_127bytes\t\t%" PRIu64 "\n",
		__func__, mstat->rcv_pkt_lt_127bytes);
	device_printf(dev, "%s: rcv_pkt_lt_255bytes\t\t%" PRIu64 "\n",
		__func__, mstat->rcv_pkt_lt_255bytes);
	device_printf(dev, "%s: rcv_pkt_lt_511bytes\t\t%" PRIu64 "\n",
		__func__, mstat->rcv_pkt_lt_511bytes);
	device_printf(dev, "%s: rcv_pkt_lt_1023bytes\t\t%" PRIu64 "\n",
		__func__, mstat->rcv_pkt_lt_1023bytes);
	device_printf(dev, "%s: rcv_pkt_lt_1518bytes\t\t%" PRIu64 "\n",
		__func__, mstat->rcv_pkt_lt_1518bytes);
	device_printf(dev, "%s: rcv_pkt_gt_1518bytes\t\t%" PRIu64 "\n",
		__func__, mstat->rcv_pkt_gt_1518bytes);

	device_printf(dev, "%s: rcv_len_error\t\t%" PRIu64 "\n", __func__,
		mstat->rcv_len_error);
	device_printf(dev, "%s: rcv_len_small\t\t%" PRIu64 "\n", __func__,
		mstat->rcv_len_small);
	device_printf(dev, "%s: rcv_len_large\t\t%" PRIu64 "\n", __func__,
		mstat->rcv_len_large);
	device_printf(dev, "%s: rcv_jabber\t\t\t%" PRIu64 "\n", __func__,
		mstat->rcv_jabber);
	device_printf(dev, "%s: rcv_dropped\t\t\t%" PRIu64 "\n", __func__,
		mstat->rcv_dropped);
	device_printf(dev, "%s: fcs_error\t\t\t%" PRIu64 "\n", __func__,
		mstat->fcs_error);
	device_printf(dev, "%s: align_error\t\t\t%" PRIu64 "\n", __func__,
		mstat->align_error);
}


static int
qla_get_hw_stats(qla_host_t *ha, uint32_t cmd, uint32_t rsp_size)
{
	device_t		dev;
	q80_get_stats_t		*stat;
	q80_get_stats_rsp_t	*stat_rsp;
	uint32_t		err;

	dev = ha->pci_dev;

	stat = (q80_get_stats_t *)ha->hw.mbox;
	bzero(stat, (sizeof (q80_get_stats_t)));

	stat->opcode = Q8_MBX_GET_STATS;
	stat->count_version = 2;
	stat->count_version |= Q8_MBX_CMD_VERSION;

	stat->cmd = cmd;

        if (qla_mbx_cmd(ha, (uint32_t *)stat, 2,
                ha->hw.mbox, (rsp_size >> 2), 0)) {
                device_printf(dev, "%s: failed\n", __func__);
                return -1;
        }

	stat_rsp = (q80_get_stats_rsp_t *)ha->hw.mbox;

        err = Q8_MBX_RSP_STATUS(stat_rsp->regcnt_status);

        if (err) {
                return -1;
        }

	return 0;
}

void
ql_get_stats(qla_host_t *ha)
{
	q80_get_stats_rsp_t	*stat_rsp;
	q80_mac_stats_t		*mstat;
	q80_xmt_stats_t		*xstat;
	q80_rcv_stats_t		*rstat;
	uint32_t		cmd;
	int			i;

	stat_rsp = (q80_get_stats_rsp_t *)ha->hw.mbox;
	/*
	 * Get MAC Statistics
	 */
	cmd = Q8_GET_STATS_CMD_TYPE_MAC;
//	cmd |= Q8_GET_STATS_CMD_CLEAR;

	cmd |= ((ha->pci_func & 0x1) << 16);

	if (qla_get_hw_stats(ha, cmd, sizeof (q80_get_stats_rsp_t)) == 0) {
		mstat = (q80_mac_stats_t *)&stat_rsp->u.mac;
		qla_mac_stats(ha, mstat);
	} else {
                device_printf(ha->pci_dev, "%s: mac failed [0x%08x]\n",
			__func__, ha->hw.mbox[0]);
	}
	/*
	 * Get RCV Statistics
	 */
	cmd = Q8_GET_STATS_CMD_RCV | Q8_GET_STATS_CMD_TYPE_CNTXT;
//	cmd |= Q8_GET_STATS_CMD_CLEAR;
	cmd |= (ha->hw.rcv_cntxt_id << 16);

	if (qla_get_hw_stats(ha, cmd, sizeof (q80_get_stats_rsp_t)) == 0) {
		rstat = (q80_rcv_stats_t *)&stat_rsp->u.rcv;
		qla_rcv_stats(ha, rstat);
	} else {
                device_printf(ha->pci_dev, "%s: rcv failed [0x%08x]\n",
			__func__, ha->hw.mbox[0]);
	}
	/*
	 * Get XMT Statistics
	 */
	for (i = 0 ; i < ha->hw.num_tx_rings; i++) {
		cmd = Q8_GET_STATS_CMD_XMT | Q8_GET_STATS_CMD_TYPE_CNTXT;
//		cmd |= Q8_GET_STATS_CMD_CLEAR;
		cmd |= (ha->hw.tx_cntxt[i].tx_cntxt_id << 16);

		if (qla_get_hw_stats(ha, cmd, sizeof(q80_get_stats_rsp_t))
			== 0) {
			xstat = (q80_xmt_stats_t *)&stat_rsp->u.xmt;
			qla_xmt_stats(ha, xstat, i);
		} else {
			device_printf(ha->pci_dev, "%s: xmt failed [0x%08x]\n",
				__func__, ha->hw.mbox[0]);
		}
	}
	return;
}

static void
qla_get_quick_stats(qla_host_t *ha)
{
	q80_get_mac_rcv_xmt_stats_rsp_t *stat_rsp;
	q80_mac_stats_t         *mstat;
	q80_xmt_stats_t         *xstat;
	q80_rcv_stats_t         *rstat;
	uint32_t                cmd;

	stat_rsp = (q80_get_mac_rcv_xmt_stats_rsp_t *)ha->hw.mbox;

	cmd = Q8_GET_STATS_CMD_TYPE_ALL;
//      cmd |= Q8_GET_STATS_CMD_CLEAR;

//      cmd |= ((ha->pci_func & 0x3) << 16);
	cmd |= (0xFFFF << 16);

	if (qla_get_hw_stats(ha, cmd,
			sizeof (q80_get_mac_rcv_xmt_stats_rsp_t)) == 0) {

		mstat = (q80_mac_stats_t *)&stat_rsp->mac;
		rstat = (q80_rcv_stats_t *)&stat_rsp->rcv;
		xstat = (q80_xmt_stats_t *)&stat_rsp->xmt;
		qla_mac_stats(ha, mstat);
		qla_rcv_stats(ha, rstat);
		qla_xmt_stats(ha, xstat, ha->hw.num_tx_rings);
	} else {
		device_printf(ha->pci_dev, "%s: failed [0x%08x]\n",
			__func__, ha->hw.mbox[0]);
	}
	return;
}

/*
 * Name: qla_tx_tso
 * Function: Checks if the packet to be transmitted is a candidate for
 *	Large TCP Segment Offload. If yes, the appropriate fields in the Tx
 *	Ring Structure are plugged in.
 */
static int
qla_tx_tso(qla_host_t *ha, struct mbuf *mp, q80_tx_cmd_t *tx_cmd, uint8_t *hdr)
{
	struct ether_vlan_header *eh;
	struct ip *ip = NULL;
	struct ip6_hdr *ip6 = NULL;
	struct tcphdr *th = NULL;
	uint32_t ehdrlen,  hdrlen, ip_hlen, tcp_hlen, tcp_opt_off;
	uint16_t etype, opcode, offload = 1;
	device_t dev;

	dev = ha->pci_dev;


	eh = mtod(mp, struct ether_vlan_header *);

	if (eh->evl_encap_proto == htons(ETHERTYPE_VLAN)) {
		ehdrlen = ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN;
		etype = ntohs(eh->evl_proto);
	} else {
		ehdrlen = ETHER_HDR_LEN;
		etype = ntohs(eh->evl_encap_proto);
	}

	hdrlen = 0;

	switch (etype) {
		case ETHERTYPE_IP:

			tcp_opt_off = ehdrlen + sizeof(struct ip) +
					sizeof(struct tcphdr);

			if (mp->m_len < tcp_opt_off) {
				m_copydata(mp, 0, tcp_opt_off, hdr);
				ip = (struct ip *)(hdr + ehdrlen);
			} else {
				ip = (struct ip *)(mp->m_data + ehdrlen);
			}

			ip_hlen = ip->ip_hl << 2;
			opcode = Q8_TX_CMD_OP_XMT_TCP_LSO;

				
			if ((ip->ip_p != IPPROTO_TCP) ||
				(ip_hlen != sizeof (struct ip))){
				/* IP Options are not supported */

				offload = 0;
			} else
				th = (struct tcphdr *)((caddr_t)ip + ip_hlen);

		break;

		case ETHERTYPE_IPV6:

			tcp_opt_off = ehdrlen + sizeof(struct ip6_hdr) +
					sizeof (struct tcphdr);

			if (mp->m_len < tcp_opt_off) {
				m_copydata(mp, 0, tcp_opt_off, hdr);
				ip6 = (struct ip6_hdr *)(hdr + ehdrlen);
			} else {
				ip6 = (struct ip6_hdr *)(mp->m_data + ehdrlen);
			}

			ip_hlen = sizeof(struct ip6_hdr);
			opcode = Q8_TX_CMD_OP_XMT_TCP_LSO_IPV6;

			if (ip6->ip6_nxt != IPPROTO_TCP) {
				//device_printf(dev, "%s: ipv6\n", __func__);
				offload = 0;
			} else
				th = (struct tcphdr *)((caddr_t)ip6 + ip_hlen);
		break;

		default:
			QL_DPRINT8(ha, (dev, "%s: type!=ip\n", __func__));
			offload = 0;
		break;
	}

	if (!offload)
		return (-1);

	tcp_hlen = th->th_off << 2;
	hdrlen = ehdrlen + ip_hlen + tcp_hlen;

        if (mp->m_len < hdrlen) {
                if (mp->m_len < tcp_opt_off) {
                        if (tcp_hlen > sizeof(struct tcphdr)) {
                                m_copydata(mp, tcp_opt_off,
                                        (tcp_hlen - sizeof(struct tcphdr)),
                                        &hdr[tcp_opt_off]);
                        }
                } else {
                        m_copydata(mp, 0, hdrlen, hdr);
                }
        }

	tx_cmd->mss = mp->m_pkthdr.tso_segsz;

	tx_cmd->flags_opcode = opcode ;
	tx_cmd->tcp_hdr_off = ip_hlen + ehdrlen;
	tx_cmd->total_hdr_len = hdrlen;

	/* Check for Multicast least significant bit of MSB == 1 */
	if (eh->evl_dhost[0] & 0x01) {
		tx_cmd->flags_opcode |= Q8_TX_CMD_FLAGS_MULTICAST;
	}

	if (mp->m_len < hdrlen) {
		printf("%d\n", hdrlen);
		return (1);
	}

	return (0);
}

/*
 * Name: qla_tx_chksum
 * Function: Checks if the packet to be transmitted is a candidate for
 *	TCP/UDP Checksum offload. If yes, the appropriate fields in the Tx
 *	Ring Structure are plugged in.
 */
static int
qla_tx_chksum(qla_host_t *ha, struct mbuf *mp, uint32_t *op_code,
	uint32_t *tcp_hdr_off)
{
	struct ether_vlan_header *eh;
	struct ip *ip;
	struct ip6_hdr *ip6;
	uint32_t ehdrlen, ip_hlen;
	uint16_t etype, opcode, offload = 1;
	device_t dev;
	uint8_t buf[sizeof(struct ip6_hdr)];

	dev = ha->pci_dev;

	*op_code = 0;

	if ((mp->m_pkthdr.csum_flags & (CSUM_TCP|CSUM_UDP)) == 0)
		return (-1);

	eh = mtod(mp, struct ether_vlan_header *);

	if (eh->evl_encap_proto == htons(ETHERTYPE_VLAN)) {
		ehdrlen = ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN;
		etype = ntohs(eh->evl_proto);
	} else {
		ehdrlen = ETHER_HDR_LEN;
		etype = ntohs(eh->evl_encap_proto);
	}

		
	switch (etype) {
		case ETHERTYPE_IP:
			ip = (struct ip *)(mp->m_data + ehdrlen);

			ip_hlen = sizeof (struct ip);

			if (mp->m_len < (ehdrlen + ip_hlen)) {
				m_copydata(mp, ehdrlen, sizeof(struct ip), buf);
				ip = (struct ip *)buf;
			}

			if (ip->ip_p == IPPROTO_TCP)
				opcode = Q8_TX_CMD_OP_XMT_TCP_CHKSUM;
			else if (ip->ip_p == IPPROTO_UDP)
				opcode = Q8_TX_CMD_OP_XMT_UDP_CHKSUM;
			else {
				//device_printf(dev, "%s: ipv4\n", __func__);
				offload = 0;
			}
		break;

		case ETHERTYPE_IPV6:
			ip6 = (struct ip6_hdr *)(mp->m_data + ehdrlen);

			ip_hlen = sizeof(struct ip6_hdr);

			if (mp->m_len < (ehdrlen + ip_hlen)) {
				m_copydata(mp, ehdrlen, sizeof (struct ip6_hdr),
					buf);
				ip6 = (struct ip6_hdr *)buf;
			}

			if (ip6->ip6_nxt == IPPROTO_TCP)
				opcode = Q8_TX_CMD_OP_XMT_TCP_CHKSUM_IPV6;
			else if (ip6->ip6_nxt == IPPROTO_UDP)
				opcode = Q8_TX_CMD_OP_XMT_UDP_CHKSUM_IPV6;
			else {
				//device_printf(dev, "%s: ipv6\n", __func__);
				offload = 0;
			}
		break;

		default:
			offload = 0;
		break;
	}
	if (!offload)
		return (-1);

	*op_code = opcode;
	*tcp_hdr_off = (ip_hlen + ehdrlen);

	return (0);
}

#define QLA_TX_MIN_FREE 2
/*
 * Name: ql_hw_send
 * Function: Transmits a packet. It first checks if the packet is a
 *	candidate for Large TCP Segment Offload and then for UDP/TCP checksum
 *	offload. If either of these creteria are not met, it is transmitted
 *	as a regular ethernet frame.
 */
int
ql_hw_send(qla_host_t *ha, bus_dma_segment_t *segs, int nsegs,
	uint32_t tx_idx, struct mbuf *mp, uint32_t txr_idx, uint32_t iscsi_pdu)
{
	struct ether_vlan_header *eh;
	qla_hw_t *hw = &ha->hw;
	q80_tx_cmd_t *tx_cmd, tso_cmd;
	bus_dma_segment_t *c_seg;
	uint32_t num_tx_cmds, hdr_len = 0;
	uint32_t total_length = 0, bytes, tx_cmd_count = 0, txr_next;
	device_t dev;
	int i, ret;
	uint8_t *src = NULL, *dst = NULL;
	uint8_t frame_hdr[QL_FRAME_HDR_SIZE];
	uint32_t op_code = 0;
	uint32_t tcp_hdr_off = 0;

	dev = ha->pci_dev;

	/*
	 * Always make sure there is atleast one empty slot in the tx_ring
	 * tx_ring is considered full when there only one entry available
	 */
        num_tx_cmds = (nsegs + (Q8_TX_CMD_MAX_SEGMENTS - 1)) >> 2;

	total_length = mp->m_pkthdr.len;
	if (total_length > QLA_MAX_TSO_FRAME_SIZE) {
		device_printf(dev, "%s: total length exceeds maxlen(%d)\n",
			__func__, total_length);
		return (-1);
	}
	eh = mtod(mp, struct ether_vlan_header *);

	if (mp->m_pkthdr.csum_flags & CSUM_TSO) {

		bzero((void *)&tso_cmd, sizeof(q80_tx_cmd_t));

		src = frame_hdr;
		ret = qla_tx_tso(ha, mp, &tso_cmd, src);

		if (!(ret & ~1)) {
			/* find the additional tx_cmd descriptors required */

			if (mp->m_flags & M_VLANTAG)
				tso_cmd.total_hdr_len += ETHER_VLAN_ENCAP_LEN;

			hdr_len = tso_cmd.total_hdr_len;

			bytes = sizeof(q80_tx_cmd_t) - Q8_TX_CMD_TSO_ALIGN;
			bytes = QL_MIN(bytes, hdr_len);

			num_tx_cmds++;
			hdr_len -= bytes;

			while (hdr_len) {
				bytes = QL_MIN((sizeof(q80_tx_cmd_t)), hdr_len);
				hdr_len -= bytes;
				num_tx_cmds++;
			}
			hdr_len = tso_cmd.total_hdr_len;

			if (ret == 0)
				src = (uint8_t *)eh;
		} else 
			return (EINVAL);
	} else {
		(void)qla_tx_chksum(ha, mp, &op_code, &tcp_hdr_off);
	}

	if (iscsi_pdu)
		ha->hw.iscsi_pkt_count++;

	if (hw->tx_cntxt[txr_idx].txr_free <= (num_tx_cmds + QLA_TX_MIN_FREE)) {
		qla_hw_tx_done_locked(ha, txr_idx);
		if (hw->tx_cntxt[txr_idx].txr_free <=
				(num_tx_cmds + QLA_TX_MIN_FREE)) {
        		QL_DPRINT8(ha, (dev, "%s: (hw->txr_free <= "
				"(num_tx_cmds + QLA_TX_MIN_FREE))\n",
				__func__));
			return (-1);
		}
	}

	tx_cmd = &hw->tx_cntxt[txr_idx].tx_ring_base[tx_idx];

        if (!(mp->m_pkthdr.csum_flags & CSUM_TSO)) {

                if (nsegs > ha->hw.max_tx_segs)
                        ha->hw.max_tx_segs = nsegs;

                bzero((void *)tx_cmd, sizeof(q80_tx_cmd_t));

                if (op_code) {
                        tx_cmd->flags_opcode = op_code;
                        tx_cmd->tcp_hdr_off = tcp_hdr_off;

                } else {
                        tx_cmd->flags_opcode = Q8_TX_CMD_OP_XMT_ETHER;
                }
	} else {
		bcopy(&tso_cmd, tx_cmd, sizeof(q80_tx_cmd_t));
		ha->tx_tso_frames++;
	}

	if (eh->evl_encap_proto == htons(ETHERTYPE_VLAN)) {
        	tx_cmd->flags_opcode |= Q8_TX_CMD_FLAGS_VLAN_TAGGED;

		if (iscsi_pdu)
			eh->evl_tag |= ha->hw.user_pri_iscsi << 13;

	} else if (mp->m_flags & M_VLANTAG) {

		if (hdr_len) { /* TSO */
			tx_cmd->flags_opcode |= (Q8_TX_CMD_FLAGS_VLAN_TAGGED |
						Q8_TX_CMD_FLAGS_HW_VLAN_ID);
			tx_cmd->tcp_hdr_off += ETHER_VLAN_ENCAP_LEN;
		} else
			tx_cmd->flags_opcode |= Q8_TX_CMD_FLAGS_HW_VLAN_ID;

		ha->hw_vlan_tx_frames++;
		tx_cmd->vlan_tci = mp->m_pkthdr.ether_vtag;

		if (iscsi_pdu) {
			tx_cmd->vlan_tci |= ha->hw.user_pri_iscsi << 13;
			mp->m_pkthdr.ether_vtag = tx_cmd->vlan_tci;
		}
	}


        tx_cmd->n_bufs = (uint8_t)nsegs;
        tx_cmd->data_len_lo = (uint8_t)(total_length & 0xFF);
        tx_cmd->data_len_hi = qla_host_to_le16(((uint16_t)(total_length >> 8)));
	tx_cmd->cntxtid = Q8_TX_CMD_PORT_CNXTID(ha->pci_func);

	c_seg = segs;

	while (1) {
		for (i = 0; ((i < Q8_TX_CMD_MAX_SEGMENTS) && nsegs); i++) {

			switch (i) {
			case 0:
				tx_cmd->buf1_addr = c_seg->ds_addr;
				tx_cmd->buf1_len = c_seg->ds_len;
				break;

			case 1:
				tx_cmd->buf2_addr = c_seg->ds_addr;
				tx_cmd->buf2_len = c_seg->ds_len;
				break;

			case 2:
				tx_cmd->buf3_addr = c_seg->ds_addr;
				tx_cmd->buf3_len = c_seg->ds_len;
				break;

			case 3:
				tx_cmd->buf4_addr = c_seg->ds_addr;
				tx_cmd->buf4_len = c_seg->ds_len;
				break;
			}

			c_seg++;
			nsegs--;
		}

		txr_next = hw->tx_cntxt[txr_idx].txr_next =
			(hw->tx_cntxt[txr_idx].txr_next + 1) &
				(NUM_TX_DESCRIPTORS - 1);
		tx_cmd_count++;

		if (!nsegs)
			break;
		
		tx_cmd = &hw->tx_cntxt[txr_idx].tx_ring_base[txr_next];
		bzero((void *)tx_cmd, sizeof(q80_tx_cmd_t));
	}

	if (mp->m_pkthdr.csum_flags & CSUM_TSO) {

		/* TSO : Copy the header in the following tx cmd descriptors */

		txr_next = hw->tx_cntxt[txr_idx].txr_next;

		tx_cmd = &hw->tx_cntxt[txr_idx].tx_ring_base[txr_next];
		bzero((void *)tx_cmd, sizeof(q80_tx_cmd_t));

		bytes = sizeof(q80_tx_cmd_t) - Q8_TX_CMD_TSO_ALIGN;
		bytes = QL_MIN(bytes, hdr_len);

		dst = (uint8_t *)tx_cmd + Q8_TX_CMD_TSO_ALIGN;

		if (mp->m_flags & M_VLANTAG) {
			/* first copy the src/dst MAC addresses */
			bcopy(src, dst, (ETHER_ADDR_LEN * 2));
			dst += (ETHER_ADDR_LEN * 2);
			src += (ETHER_ADDR_LEN * 2);
			
			*((uint16_t *)dst) = htons(ETHERTYPE_VLAN);
			dst += 2;
			*((uint16_t *)dst) = htons(mp->m_pkthdr.ether_vtag);
			dst += 2;

			/* bytes left in src header */
			hdr_len -= ((ETHER_ADDR_LEN * 2) +
					ETHER_VLAN_ENCAP_LEN);

			/* bytes left in TxCmd Entry */
			bytes -= ((ETHER_ADDR_LEN * 2) + ETHER_VLAN_ENCAP_LEN);


			bcopy(src, dst, bytes);
			src += bytes;
			hdr_len -= bytes;
		} else {
			bcopy(src, dst, bytes);
			src += bytes;
			hdr_len -= bytes;
		}

		txr_next = hw->tx_cntxt[txr_idx].txr_next =
				(hw->tx_cntxt[txr_idx].txr_next + 1) &
					(NUM_TX_DESCRIPTORS - 1);
		tx_cmd_count++;
		
		while (hdr_len) {
			tx_cmd = &hw->tx_cntxt[txr_idx].tx_ring_base[txr_next];
			bzero((void *)tx_cmd, sizeof(q80_tx_cmd_t));

			bytes = QL_MIN((sizeof(q80_tx_cmd_t)), hdr_len);

			bcopy(src, tx_cmd, bytes);
			src += bytes;
			hdr_len -= bytes;

			txr_next = hw->tx_cntxt[txr_idx].txr_next =
				(hw->tx_cntxt[txr_idx].txr_next + 1) &
					(NUM_TX_DESCRIPTORS - 1);
			tx_cmd_count++;
		}
	}

	hw->tx_cntxt[txr_idx].txr_free =
		hw->tx_cntxt[txr_idx].txr_free - tx_cmd_count;

	QL_UPDATE_TX_PRODUCER_INDEX(ha, hw->tx_cntxt[txr_idx].txr_next,\
		txr_idx);
       	QL_DPRINT8(ha, (dev, "%s: return\n", __func__));

	return (0);
}



#define Q8_CONFIG_IND_TBL_SIZE	32 /* < Q8_RSS_IND_TBL_SIZE and power of 2 */
static int
qla_config_rss_ind_table(qla_host_t *ha)
{
	uint32_t i, count;
	uint8_t rss_ind_tbl[Q8_CONFIG_IND_TBL_SIZE];


	for (i = 0; i < Q8_CONFIG_IND_TBL_SIZE; i++) {
		rss_ind_tbl[i] = i % ha->hw.num_sds_rings;
	}

	for (i = 0; i <= Q8_RSS_IND_TBL_MAX_IDX ;
		i = i + Q8_CONFIG_IND_TBL_SIZE) {

		if ((i + Q8_CONFIG_IND_TBL_SIZE) > Q8_RSS_IND_TBL_MAX_IDX) {
			count = Q8_RSS_IND_TBL_MAX_IDX - i + 1;
		} else {
			count = Q8_CONFIG_IND_TBL_SIZE;
		}

		if (qla_set_rss_ind_table(ha, i, count, ha->hw.rcv_cntxt_id,
			rss_ind_tbl))
			return (-1);
	}

	return (0);
}

/*
 * Name: ql_del_hw_if
 * Function: Destroys the hardware specific entities corresponding to an
 *	Ethernet Interface
 */
void
ql_del_hw_if(qla_host_t *ha)
{
	uint32_t i;
	uint32_t num_msix;

	(void)qla_stop_nic_func(ha);

	qla_del_rcv_cntxt(ha);
	qla_del_xmt_cntxt(ha);

	if (ha->hw.flags.init_intr_cnxt) {
		for (i = 0; i < ha->hw.num_sds_rings; ) {

			if ((i + Q8_MAX_INTR_VECTORS) < ha->hw.num_sds_rings)
				num_msix = Q8_MAX_INTR_VECTORS;
			else
				num_msix = ha->hw.num_sds_rings - i;
			qla_config_intr_cntxt(ha, i, num_msix, 0);

			i += num_msix;
		}

		ha->hw.flags.init_intr_cnxt = 0;
	}
	return;
}

void
qla_confirm_9kb_enable(qla_host_t *ha)
{
	uint32_t supports_9kb = 0;

	ha->hw.mbx_intr_mask_offset = READ_REG32(ha, Q8_MBOX_INT_MASK_MSIX);

	/* Use MSI-X vector 0; Enable Firmware Mailbox Interrupt */
	WRITE_REG32(ha, Q8_MBOX_INT_ENABLE, BIT_2);
	WRITE_REG32(ha, ha->hw.mbx_intr_mask_offset, 0x0);

	qla_get_nic_partition(ha, &supports_9kb, NULL);

	if (!supports_9kb)
		ha->hw.enable_9kb = 0;

	return;
}


/*
 * Name: ql_init_hw_if
 * Function: Creates the hardware specific entities corresponding to an
 *	Ethernet Interface - Transmit and Receive Contexts. Sets the MAC Address
 *	corresponding to the interface. Enables LRO if allowed.
 */
int
ql_init_hw_if(qla_host_t *ha)
{
	device_t	dev;
	uint32_t	i;
	uint8_t		bcast_mac[6];
	qla_rdesc_t	*rdesc;
	uint32_t	num_msix;

	dev = ha->pci_dev;

	for (i = 0; i < ha->hw.num_sds_rings; i++) {
		bzero(ha->hw.dma_buf.sds_ring[i].dma_b,
			ha->hw.dma_buf.sds_ring[i].size);
	}

	for (i = 0; i < ha->hw.num_sds_rings; ) {

		if ((i + Q8_MAX_INTR_VECTORS) < ha->hw.num_sds_rings)
			num_msix = Q8_MAX_INTR_VECTORS;
		else
			num_msix = ha->hw.num_sds_rings - i;

		if (qla_config_intr_cntxt(ha, i, num_msix, 1)) {

			if (i > 0) {

				num_msix = i;

				for (i = 0; i < num_msix; ) {
					qla_config_intr_cntxt(ha, i,
						Q8_MAX_INTR_VECTORS, 0);
					i += Q8_MAX_INTR_VECTORS;
				}
			}
			return (-1);
		}

		i = i + num_msix;
	}

        ha->hw.flags.init_intr_cnxt = 1;

	if (ha->hw.mdump_init == 0) {
		qla_minidump_init(ha);
	}

	/*
	 * Create Receive Context
	 */
	if (qla_init_rcv_cntxt(ha)) {
		return (-1);
	}

	for (i = 0; i < ha->hw.num_rds_rings; i++) {
		rdesc = &ha->hw.rds[i];
		rdesc->rx_next = NUM_RX_DESCRIPTORS - 2;
		rdesc->rx_in = 0;
		/* Update the RDS Producer Indices */
		QL_UPDATE_RDS_PRODUCER_INDEX(ha, rdesc->prod_std,\
			rdesc->rx_next);
	}


	/*
	 * Create Transmit Context
	 */
	if (qla_init_xmt_cntxt(ha)) {
		qla_del_rcv_cntxt(ha);
		return (-1);
	}
	ha->hw.max_tx_segs = 0;

	if (qla_config_mac_addr(ha, ha->hw.mac_addr, 1))
		return(-1);

	ha->hw.flags.unicast_mac = 1;

	bcast_mac[0] = 0xFF; bcast_mac[1] = 0xFF; bcast_mac[2] = 0xFF;
	bcast_mac[3] = 0xFF; bcast_mac[4] = 0xFF; bcast_mac[5] = 0xFF;

	if (qla_config_mac_addr(ha, bcast_mac, 1))
		return (-1);

	ha->hw.flags.bcast_mac = 1;

	/*
	 * program any cached multicast addresses
	 */
	if (qla_hw_add_all_mcast(ha))
		return (-1);

	if (qla_config_rss(ha, ha->hw.rcv_cntxt_id))
		return (-1);

	if (qla_config_rss_ind_table(ha))
		return (-1);

	if (qla_config_intr_coalesce(ha, ha->hw.rcv_cntxt_id, 0, 1))
		return (-1);

	if (qla_link_event_req(ha, ha->hw.rcv_cntxt_id))
		return (-1);

	if (qla_config_fw_lro(ha, ha->hw.rcv_cntxt_id))
		return (-1);

        if (qla_init_nic_func(ha))
                return (-1);

        if (qla_query_fw_dcbx_caps(ha))
                return (-1);

	for (i = 0; i < ha->hw.num_sds_rings; i++)
		QL_ENABLE_INTERRUPTS(ha, i);

	return (0);
}

static int
qla_map_sds_to_rds(qla_host_t *ha, uint32_t start_idx, uint32_t num_idx)
{
        device_t                dev = ha->pci_dev;
        q80_rq_map_sds_to_rds_t *map_rings;
	q80_rsp_map_sds_to_rds_t *map_rings_rsp;
        uint32_t                i, err;
        qla_hw_t                *hw = &ha->hw;

        map_rings = (q80_rq_map_sds_to_rds_t *)ha->hw.mbox;
        bzero(map_rings, sizeof(q80_rq_map_sds_to_rds_t));

        map_rings->opcode = Q8_MBX_MAP_SDS_TO_RDS;
        map_rings->count_version = (sizeof (q80_rq_map_sds_to_rds_t) >> 2);
        map_rings->count_version |= Q8_MBX_CMD_VERSION;

        map_rings->cntxt_id = hw->rcv_cntxt_id;
        map_rings->num_rings = num_idx;

	for (i = 0; i < num_idx; i++) {
		map_rings->sds_rds[i].sds_ring = i + start_idx;
		map_rings->sds_rds[i].rds_ring = i + start_idx;
	}

        if (qla_mbx_cmd(ha, (uint32_t *)map_rings,
                (sizeof (q80_rq_map_sds_to_rds_t) >> 2),
                ha->hw.mbox, (sizeof(q80_rsp_add_rcv_rings_t) >> 2), 0)) {
                device_printf(dev, "%s: failed0\n", __func__);
                return (-1);
        }

        map_rings_rsp = (q80_rsp_map_sds_to_rds_t *)ha->hw.mbox;

        err = Q8_MBX_RSP_STATUS(map_rings_rsp->regcnt_status);

        if (err) {
                device_printf(dev, "%s: failed1 [0x%08x]\n", __func__, err);
                return (-1);
        }

        return (0);
}

/*
 * Name: qla_init_rcv_cntxt
 * Function: Creates the Receive Context.
 */
static int
qla_init_rcv_cntxt(qla_host_t *ha)
{
	q80_rq_rcv_cntxt_t	*rcntxt;
	q80_rsp_rcv_cntxt_t	*rcntxt_rsp;
	q80_stat_desc_t		*sdesc;
	int			i, j;
        qla_hw_t		*hw = &ha->hw;
	device_t		dev;
	uint32_t		err;
	uint32_t		rcntxt_sds_rings;
	uint32_t		rcntxt_rds_rings;
	uint32_t		max_idx;

	dev = ha->pci_dev;

	/*
	 * Create Receive Context
	 */

	for (i = 0; i < hw->num_sds_rings; i++) {
		sdesc = (q80_stat_desc_t *)&hw->sds[i].sds_ring_base[0];

		for (j = 0; j < NUM_STATUS_DESCRIPTORS; j++) {
			sdesc->data[0] = 1ULL;
			sdesc->data[1] = 1ULL;
		}
	}

	rcntxt_sds_rings = hw->num_sds_rings;
	if (hw->num_sds_rings > MAX_RCNTXT_SDS_RINGS)
		rcntxt_sds_rings = MAX_RCNTXT_SDS_RINGS;

	rcntxt_rds_rings = hw->num_rds_rings;

	if (hw->num_rds_rings > MAX_RDS_RING_SETS)
		rcntxt_rds_rings = MAX_RDS_RING_SETS;

	rcntxt = (q80_rq_rcv_cntxt_t *)ha->hw.mbox;
	bzero(rcntxt, (sizeof (q80_rq_rcv_cntxt_t)));

	rcntxt->opcode = Q8_MBX_CREATE_RX_CNTXT;
	rcntxt->count_version = (sizeof (q80_rq_rcv_cntxt_t) >> 2);
	rcntxt->count_version |= Q8_MBX_CMD_VERSION;

	rcntxt->cap0 = Q8_RCV_CNTXT_CAP0_BASEFW |
			Q8_RCV_CNTXT_CAP0_LRO |
			Q8_RCV_CNTXT_CAP0_HW_LRO |
			Q8_RCV_CNTXT_CAP0_RSS |
			Q8_RCV_CNTXT_CAP0_SGL_LRO;

	if (ha->hw.enable_9kb)
		rcntxt->cap0 |= Q8_RCV_CNTXT_CAP0_SINGLE_JUMBO;
	else
		rcntxt->cap0 |= Q8_RCV_CNTXT_CAP0_SGL_JUMBO;

	if (ha->hw.num_rds_rings > 1) {
		rcntxt->nrds_sets_rings = rcntxt_rds_rings | (1 << 5);
		rcntxt->cap0 |= Q8_RCV_CNTXT_CAP0_MULTI_RDS;
	} else
		rcntxt->nrds_sets_rings = 0x1 | (1 << 5);

	rcntxt->nsds_rings = rcntxt_sds_rings;

	rcntxt->rds_producer_mode = Q8_RCV_CNTXT_RDS_PROD_MODE_UNIQUE;

	rcntxt->rcv_vpid = 0;

	for (i = 0; i <  rcntxt_sds_rings; i++) {
		rcntxt->sds[i].paddr =
			qla_host_to_le64(hw->dma_buf.sds_ring[i].dma_addr);
		rcntxt->sds[i].size =
			qla_host_to_le32(NUM_STATUS_DESCRIPTORS);
		if (ha->msix_count == 2) {
			rcntxt->sds[i].intr_id =
				qla_host_to_le16(hw->intr_id[0]);
			rcntxt->sds[i].intr_src_bit = qla_host_to_le16((i));
		} else {
			rcntxt->sds[i].intr_id =
				qla_host_to_le16(hw->intr_id[i]);
			rcntxt->sds[i].intr_src_bit = qla_host_to_le16(0);
		}
	}

	for (i = 0; i <  rcntxt_rds_rings; i++) {
		rcntxt->rds[i].paddr_std =
			qla_host_to_le64(hw->dma_buf.rds_ring[i].dma_addr);

		if (ha->hw.enable_9kb)
			rcntxt->rds[i].std_bsize =
				qla_host_to_le64(MJUM9BYTES);
		else
			rcntxt->rds[i].std_bsize = qla_host_to_le64(MCLBYTES);

		rcntxt->rds[i].std_nentries =
			qla_host_to_le32(NUM_RX_DESCRIPTORS);
	}

        if (qla_mbx_cmd(ha, (uint32_t *)rcntxt,
		(sizeof (q80_rq_rcv_cntxt_t) >> 2),
                ha->hw.mbox, (sizeof(q80_rsp_rcv_cntxt_t) >> 2), 0)) {
                device_printf(dev, "%s: failed0\n", __func__);
                return (-1);
        }

        rcntxt_rsp = (q80_rsp_rcv_cntxt_t *)ha->hw.mbox;

        err = Q8_MBX_RSP_STATUS(rcntxt_rsp->regcnt_status);

        if (err) {
                device_printf(dev, "%s: failed1 [0x%08x]\n", __func__, err);
                return (-1);
        }

	for (i = 0; i <  rcntxt_sds_rings; i++) {
		hw->sds[i].sds_consumer = rcntxt_rsp->sds_cons[i];
	}

	for (i = 0; i <  rcntxt_rds_rings; i++) {
		hw->rds[i].prod_std = rcntxt_rsp->rds[i].prod_std;
	}

	hw->rcv_cntxt_id = rcntxt_rsp->cntxt_id;

	ha->hw.flags.init_rx_cnxt = 1;

	if (hw->num_sds_rings > MAX_RCNTXT_SDS_RINGS) {

		for (i = MAX_RCNTXT_SDS_RINGS; i < hw->num_sds_rings;) {

			if ((i + MAX_RCNTXT_SDS_RINGS) < hw->num_sds_rings)
				max_idx = MAX_RCNTXT_SDS_RINGS;
			else
				max_idx = hw->num_sds_rings - i;

			err = qla_add_rcv_rings(ha, i, max_idx);
			if (err)
				return -1;

			i += max_idx;
		}
	}

	if (hw->num_rds_rings > 1) {

		for (i = 0; i < hw->num_rds_rings; ) {

			if ((i + MAX_SDS_TO_RDS_MAP) < hw->num_rds_rings)
				max_idx = MAX_SDS_TO_RDS_MAP;
			else
				max_idx = hw->num_rds_rings - i;

			err = qla_map_sds_to_rds(ha, i, max_idx);
			if (err)
				return -1;

			i += max_idx;
		}
	}

	return (0);
}

static int
qla_add_rcv_rings(qla_host_t *ha, uint32_t sds_idx, uint32_t nsds)
{
	device_t		dev = ha->pci_dev;
	q80_rq_add_rcv_rings_t	*add_rcv;
	q80_rsp_add_rcv_rings_t	*add_rcv_rsp;
	uint32_t		i,j, err;
        qla_hw_t		*hw = &ha->hw;

	add_rcv = (q80_rq_add_rcv_rings_t *)ha->hw.mbox;
	bzero(add_rcv, sizeof (q80_rq_add_rcv_rings_t));

	add_rcv->opcode = Q8_MBX_ADD_RX_RINGS;
	add_rcv->count_version = (sizeof (q80_rq_add_rcv_rings_t) >> 2);
	add_rcv->count_version |= Q8_MBX_CMD_VERSION;

	add_rcv->nrds_sets_rings = nsds | (1 << 5);
	add_rcv->nsds_rings = nsds;
	add_rcv->cntxt_id = hw->rcv_cntxt_id;

        for (i = 0; i <  nsds; i++) {

		j = i + sds_idx;

                add_rcv->sds[i].paddr =
                        qla_host_to_le64(hw->dma_buf.sds_ring[j].dma_addr);

                add_rcv->sds[i].size =
                        qla_host_to_le32(NUM_STATUS_DESCRIPTORS);

                if (ha->msix_count == 2) {
                        add_rcv->sds[i].intr_id =
                                qla_host_to_le16(hw->intr_id[0]);
                        add_rcv->sds[i].intr_src_bit = qla_host_to_le16(j);
                } else {
                        add_rcv->sds[i].intr_id =
                                qla_host_to_le16(hw->intr_id[j]);
                        add_rcv->sds[i].intr_src_bit = qla_host_to_le16(0);
                }

        }
        for (i = 0; (i <  nsds); i++) {
                j = i + sds_idx;

                add_rcv->rds[i].paddr_std =
                        qla_host_to_le64(hw->dma_buf.rds_ring[j].dma_addr);

		if (ha->hw.enable_9kb)
			add_rcv->rds[i].std_bsize =
				qla_host_to_le64(MJUM9BYTES);
		else
                	add_rcv->rds[i].std_bsize = qla_host_to_le64(MCLBYTES);

                add_rcv->rds[i].std_nentries =
                        qla_host_to_le32(NUM_RX_DESCRIPTORS);
        }


        if (qla_mbx_cmd(ha, (uint32_t *)add_rcv,
		(sizeof (q80_rq_add_rcv_rings_t) >> 2),
                ha->hw.mbox, (sizeof(q80_rsp_add_rcv_rings_t) >> 2), 0)) {
                device_printf(dev, "%s: failed0\n", __func__);
                return (-1);
        }

        add_rcv_rsp = (q80_rsp_add_rcv_rings_t *)ha->hw.mbox;

        err = Q8_MBX_RSP_STATUS(add_rcv_rsp->regcnt_status);

        if (err) {
                device_printf(dev, "%s: failed1 [0x%08x]\n", __func__, err);
                return (-1);
        }

	for (i = 0; i < nsds; i++) {
		hw->sds[(i + sds_idx)].sds_consumer = add_rcv_rsp->sds_cons[i];
	}

	for (i = 0; i < nsds; i++) {
		hw->rds[(i + sds_idx)].prod_std = add_rcv_rsp->rds[i].prod_std;
	}

	return (0);
}

/*
 * Name: qla_del_rcv_cntxt
 * Function: Destroys the Receive Context.
 */
static void
qla_del_rcv_cntxt(qla_host_t *ha)
{
	device_t			dev = ha->pci_dev;
	q80_rcv_cntxt_destroy_t		*rcntxt;
	q80_rcv_cntxt_destroy_rsp_t	*rcntxt_rsp;
	uint32_t			err;
	uint8_t				bcast_mac[6];

	if (!ha->hw.flags.init_rx_cnxt)
		return;

	if (qla_hw_del_all_mcast(ha))
		return;

	if (ha->hw.flags.bcast_mac) {

		bcast_mac[0] = 0xFF; bcast_mac[1] = 0xFF; bcast_mac[2] = 0xFF;
		bcast_mac[3] = 0xFF; bcast_mac[4] = 0xFF; bcast_mac[5] = 0xFF;

		if (qla_config_mac_addr(ha, bcast_mac, 0))
			return;
		ha->hw.flags.bcast_mac = 0;

	}

	if (ha->hw.flags.unicast_mac) {
		if (qla_config_mac_addr(ha, ha->hw.mac_addr, 0))
			return;
		ha->hw.flags.unicast_mac = 0;
	}

	rcntxt = (q80_rcv_cntxt_destroy_t *)ha->hw.mbox;
	bzero(rcntxt, (sizeof (q80_rcv_cntxt_destroy_t)));

	rcntxt->opcode = Q8_MBX_DESTROY_RX_CNTXT;
	rcntxt->count_version = (sizeof (q80_rcv_cntxt_destroy_t) >> 2);
	rcntxt->count_version |= Q8_MBX_CMD_VERSION;

	rcntxt->cntxt_id = ha->hw.rcv_cntxt_id;

        if (qla_mbx_cmd(ha, (uint32_t *)rcntxt,
		(sizeof (q80_rcv_cntxt_destroy_t) >> 2),
                ha->hw.mbox, (sizeof(q80_rcv_cntxt_destroy_rsp_t) >> 2), 0)) {
                device_printf(dev, "%s: failed0\n", __func__);
                return;
        }
        rcntxt_rsp = (q80_rcv_cntxt_destroy_rsp_t *)ha->hw.mbox;

        err = Q8_MBX_RSP_STATUS(rcntxt_rsp->regcnt_status);

        if (err) {
                device_printf(dev, "%s: failed1 [0x%08x]\n", __func__, err);
        }

	ha->hw.flags.init_rx_cnxt = 0;
	return;
}

/*
 * Name: qla_init_xmt_cntxt
 * Function: Creates the Transmit Context.
 */
static int
qla_init_xmt_cntxt_i(qla_host_t *ha, uint32_t txr_idx)
{
	device_t		dev;
        qla_hw_t		*hw = &ha->hw;
	q80_rq_tx_cntxt_t	*tcntxt;
	q80_rsp_tx_cntxt_t	*tcntxt_rsp;
	uint32_t		err;
	qla_hw_tx_cntxt_t       *hw_tx_cntxt;

	hw_tx_cntxt = &hw->tx_cntxt[txr_idx];

	dev = ha->pci_dev;

	/*
	 * Create Transmit Context
	 */
	tcntxt = (q80_rq_tx_cntxt_t *)ha->hw.mbox;
	bzero(tcntxt, (sizeof (q80_rq_tx_cntxt_t)));

	tcntxt->opcode = Q8_MBX_CREATE_TX_CNTXT;
	tcntxt->count_version = (sizeof (q80_rq_tx_cntxt_t) >> 2);
	tcntxt->count_version |= Q8_MBX_CMD_VERSION;

#ifdef QL_ENABLE_ISCSI_TLV

	tcntxt->cap0 = Q8_TX_CNTXT_CAP0_BASEFW | Q8_TX_CNTXT_CAP0_LSO |
				Q8_TX_CNTXT_CAP0_TC;

	if (txr_idx >= (ha->hw.num_tx_rings >> 1)) {
		tcntxt->traffic_class = 1;
	}

#else

	tcntxt->cap0 = Q8_TX_CNTXT_CAP0_BASEFW | Q8_TX_CNTXT_CAP0_LSO;

#endif /* #ifdef QL_ENABLE_ISCSI_TLV */

	tcntxt->ntx_rings = 1;

	tcntxt->tx_ring[0].paddr =
		qla_host_to_le64(hw_tx_cntxt->tx_ring_paddr);
	tcntxt->tx_ring[0].tx_consumer =
		qla_host_to_le64(hw_tx_cntxt->tx_cons_paddr);
	tcntxt->tx_ring[0].nentries = qla_host_to_le16(NUM_TX_DESCRIPTORS);

	tcntxt->tx_ring[0].intr_id = qla_host_to_le16(hw->intr_id[0]);
	tcntxt->tx_ring[0].intr_src_bit = qla_host_to_le16(0);


	hw_tx_cntxt->txr_free = NUM_TX_DESCRIPTORS;
	hw_tx_cntxt->txr_next = hw_tx_cntxt->txr_comp = 0;

        if (qla_mbx_cmd(ha, (uint32_t *)tcntxt,
		(sizeof (q80_rq_tx_cntxt_t) >> 2),
                ha->hw.mbox,
		(sizeof(q80_rsp_tx_cntxt_t) >> 2), 0)) {
                device_printf(dev, "%s: failed0\n", __func__);
                return (-1);
        }
        tcntxt_rsp = (q80_rsp_tx_cntxt_t *)ha->hw.mbox;

        err = Q8_MBX_RSP_STATUS(tcntxt_rsp->regcnt_status);

        if (err) {
                device_printf(dev, "%s: failed1 [0x%08x]\n", __func__, err);
		return -1;
        }

	hw_tx_cntxt->tx_prod_reg = tcntxt_rsp->tx_ring[0].prod_index;
	hw_tx_cntxt->tx_cntxt_id = tcntxt_rsp->tx_ring[0].cntxt_id;

	if (qla_config_intr_coalesce(ha, hw_tx_cntxt->tx_cntxt_id, 0, 0))
		return (-1);

	return (0);
}


/*
 * Name: qla_del_xmt_cntxt
 * Function: Destroys the Transmit Context.
 */
static int
qla_del_xmt_cntxt_i(qla_host_t *ha, uint32_t txr_idx)
{
	device_t			dev = ha->pci_dev;
	q80_tx_cntxt_destroy_t		*tcntxt;
	q80_tx_cntxt_destroy_rsp_t	*tcntxt_rsp;
	uint32_t			err;

	tcntxt = (q80_tx_cntxt_destroy_t *)ha->hw.mbox;
	bzero(tcntxt, (sizeof (q80_tx_cntxt_destroy_t)));

	tcntxt->opcode = Q8_MBX_DESTROY_TX_CNTXT;
	tcntxt->count_version = (sizeof (q80_tx_cntxt_destroy_t) >> 2);
	tcntxt->count_version |= Q8_MBX_CMD_VERSION;

	tcntxt->cntxt_id = ha->hw.tx_cntxt[txr_idx].tx_cntxt_id;

        if (qla_mbx_cmd(ha, (uint32_t *)tcntxt,
		(sizeof (q80_tx_cntxt_destroy_t) >> 2),
                ha->hw.mbox, (sizeof (q80_tx_cntxt_destroy_rsp_t) >> 2), 0)) {
                device_printf(dev, "%s: failed0\n", __func__);
                return (-1);
        }
        tcntxt_rsp = (q80_tx_cntxt_destroy_rsp_t *)ha->hw.mbox;

        err = Q8_MBX_RSP_STATUS(tcntxt_rsp->regcnt_status);

        if (err) {
                device_printf(dev, "%s: failed1 [0x%08x]\n", __func__, err);
		return (-1);
        }

	return (0);
}
static void
qla_del_xmt_cntxt(qla_host_t *ha)
{
	uint32_t i;

	if (!ha->hw.flags.init_tx_cnxt)
		return;

	for (i = 0; i < ha->hw.num_tx_rings; i++) {
		if (qla_del_xmt_cntxt_i(ha, i))
			break;
	}
	ha->hw.flags.init_tx_cnxt = 0;
}

static int
qla_init_xmt_cntxt(qla_host_t *ha)
{
	uint32_t i, j;

	for (i = 0; i < ha->hw.num_tx_rings; i++) {
		if (qla_init_xmt_cntxt_i(ha, i) != 0) {
			for (j = 0; j < i; j++)
				qla_del_xmt_cntxt_i(ha, j);
			return (-1);
		}
	}
	ha->hw.flags.init_tx_cnxt = 1;
	return (0);
}

static int
qla_hw_add_all_mcast(qla_host_t *ha)
{
	int i, nmcast;

	nmcast = ha->hw.nmcast;

	for (i = 0 ; ((i < Q8_MAX_NUM_MULTICAST_ADDRS) && nmcast); i++) {
		if ((ha->hw.mcast[i].addr[0] != 0) || 
			(ha->hw.mcast[i].addr[1] != 0) ||
			(ha->hw.mcast[i].addr[2] != 0) ||
			(ha->hw.mcast[i].addr[3] != 0) ||
			(ha->hw.mcast[i].addr[4] != 0) ||
			(ha->hw.mcast[i].addr[5] != 0)) {

			if (qla_config_mac_addr(ha, ha->hw.mcast[i].addr, 1)) {
                		device_printf(ha->pci_dev, "%s: failed\n",
					__func__);
				return (-1);
			}

			nmcast--;
		}
	}
	return 0;
}

static int
qla_hw_del_all_mcast(qla_host_t *ha)
{
	int i, nmcast;

	nmcast = ha->hw.nmcast;

	for (i = 0 ; ((i < Q8_MAX_NUM_MULTICAST_ADDRS) && nmcast); i++) {
		if ((ha->hw.mcast[i].addr[0] != 0) || 
			(ha->hw.mcast[i].addr[1] != 0) ||
			(ha->hw.mcast[i].addr[2] != 0) ||
			(ha->hw.mcast[i].addr[3] != 0) ||
			(ha->hw.mcast[i].addr[4] != 0) ||
			(ha->hw.mcast[i].addr[5] != 0)) {

			if (qla_config_mac_addr(ha, ha->hw.mcast[i].addr, 0))
				return (-1);

			nmcast--;
		}
	}
	return 0;
}

static int
qla_hw_add_mcast(qla_host_t *ha, uint8_t *mta)
{
	int i;

	for (i = 0; i < Q8_MAX_NUM_MULTICAST_ADDRS; i++) {

		if (QL_MAC_CMP(ha->hw.mcast[i].addr, mta) == 0)
			return 0; /* its been already added */
	}

	for (i = 0; i < Q8_MAX_NUM_MULTICAST_ADDRS; i++) {

		if ((ha->hw.mcast[i].addr[0] == 0) && 
			(ha->hw.mcast[i].addr[1] == 0) &&
			(ha->hw.mcast[i].addr[2] == 0) &&
			(ha->hw.mcast[i].addr[3] == 0) &&
			(ha->hw.mcast[i].addr[4] == 0) &&
			(ha->hw.mcast[i].addr[5] == 0)) {

			if (qla_config_mac_addr(ha, mta, 1))
				return (-1);

			bcopy(mta, ha->hw.mcast[i].addr, Q8_MAC_ADDR_LEN);
			ha->hw.nmcast++;	

			return 0;
		}
	}
	return 0;
}

static int
qla_hw_del_mcast(qla_host_t *ha, uint8_t *mta)
{
	int i;

	for (i = 0; i < Q8_MAX_NUM_MULTICAST_ADDRS; i++) {
		if (QL_MAC_CMP(ha->hw.mcast[i].addr, mta) == 0) {

			if (qla_config_mac_addr(ha, mta, 0))
				return (-1);

			ha->hw.mcast[i].addr[0] = 0;
			ha->hw.mcast[i].addr[1] = 0;
			ha->hw.mcast[i].addr[2] = 0;
			ha->hw.mcast[i].addr[3] = 0;
			ha->hw.mcast[i].addr[4] = 0;
			ha->hw.mcast[i].addr[5] = 0;

			ha->hw.nmcast--;	

			return 0;
		}
	}
	return 0;
}

/*
 * Name: ql_hw_set_multi
 * Function: Sets the Multicast Addresses provided the host O.S into the
 *	hardware (for the given interface)
 */
int
ql_hw_set_multi(qla_host_t *ha, uint8_t *mcast, uint32_t mcnt,
	uint32_t add_mac)
{
	int i;
	uint8_t *mta = mcast;
	int ret = 0;

	for (i = 0; i < mcnt; i++) {
		if (add_mac) {
			ret = qla_hw_add_mcast(ha, mta);
			if (ret)
				break;
		} else {
			ret = qla_hw_del_mcast(ha, mta);
			if (ret)
				break;
		}
			
		mta += Q8_MAC_ADDR_LEN;
	}
	return (ret);
}

/*
 * Name: qla_hw_tx_done_locked
 * Function: Handle Transmit Completions
 */
static void
qla_hw_tx_done_locked(qla_host_t *ha, uint32_t txr_idx)
{
	qla_tx_buf_t *txb;
        qla_hw_t *hw = &ha->hw;
	uint32_t comp_idx, comp_count = 0;
	qla_hw_tx_cntxt_t *hw_tx_cntxt;

	hw_tx_cntxt = &hw->tx_cntxt[txr_idx];

	/* retrieve index of last entry in tx ring completed */
	comp_idx = qla_le32_to_host(*(hw_tx_cntxt->tx_cons));

	while (comp_idx != hw_tx_cntxt->txr_comp) {

		txb = &ha->tx_ring[txr_idx].tx_buf[hw_tx_cntxt->txr_comp];

		hw_tx_cntxt->txr_comp++;
		if (hw_tx_cntxt->txr_comp == NUM_TX_DESCRIPTORS)
			hw_tx_cntxt->txr_comp = 0;

		comp_count++;

		if (txb->m_head) {
			if_inc_counter(ha->ifp, IFCOUNTER_OPACKETS, 1);

			bus_dmamap_sync(ha->tx_tag, txb->map,
				BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(ha->tx_tag, txb->map);
			m_freem(txb->m_head);

			txb->m_head = NULL;
		}
	}

	hw_tx_cntxt->txr_free += comp_count;
	return;
}

/*
 * Name: ql_hw_tx_done
 * Function: Handle Transmit Completions
 */
void
ql_hw_tx_done(qla_host_t *ha)
{
	int i;
	uint32_t flag = 0;

	if (!mtx_trylock(&ha->tx_lock)) {
       		QL_DPRINT8(ha, (ha->pci_dev,
			"%s: !mtx_trylock(&ha->tx_lock)\n", __func__));
		return;
	}
	for (i = 0; i < ha->hw.num_tx_rings; i++) {
		qla_hw_tx_done_locked(ha, i);
		if (ha->hw.tx_cntxt[i].txr_free <= (NUM_TX_DESCRIPTORS >> 1))
			flag = 1;
	}

	if (!flag)
		ha->ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	QLA_TX_UNLOCK(ha);
	return;
}

void
ql_update_link_state(qla_host_t *ha)
{
	uint32_t link_state;
	uint32_t prev_link_state;

	if (!(ha->ifp->if_drv_flags & IFF_DRV_RUNNING)) {
		ha->hw.link_up = 0;
		return;
	}
	link_state = READ_REG32(ha, Q8_LINK_STATE);

	prev_link_state =  ha->hw.link_up;

	if (ha->pci_func == 0) 
		ha->hw.link_up = (((link_state & 0xF) == 1)? 1 : 0);
	else
		ha->hw.link_up = ((((link_state >> 4)& 0xF) == 1)? 1 : 0);

	if (prev_link_state !=  ha->hw.link_up) {
		if (ha->hw.link_up) {
			if_link_state_change(ha->ifp, LINK_STATE_UP);
		} else {
			if_link_state_change(ha->ifp, LINK_STATE_DOWN);
		}
	}
	return;
}

void
ql_hw_stop_rcv(qla_host_t *ha)
{
	int i, done, count = 100;

	while (count) {
		done = 1;
		for (i = 0; i < ha->hw.num_sds_rings; i++) {
			if (ha->hw.sds[i].rcv_active)
				done = 0;
		}
		if (done)
			break;
		else 
			qla_mdelay(__func__, 10);
		count--;
	}
	if (!count)
		device_printf(ha->pci_dev, "%s: Counter expired.\n", __func__);

	return;
}

int
ql_hw_check_health(qla_host_t *ha)
{
	uint32_t val;

	ha->hw.health_count++;

	if (ha->hw.health_count < 1000)
		return 0;

	ha->hw.health_count = 0;

	val = READ_REG32(ha, Q8_ASIC_TEMPERATURE);

	if (((val & 0xFFFF) == 2) || ((val & 0xFFFF) == 3) ||
		(QL_ERR_INJECT(ha, INJCT_TEMPERATURE_FAILURE))) {
		device_printf(ha->pci_dev, "%s: Temperature Alert [0x%08x]\n",
			__func__, val);
		return -1;
	}

	val = READ_REG32(ha, Q8_FIRMWARE_HEARTBEAT);

	if ((val != ha->hw.hbeat_value) &&
		(!(QL_ERR_INJECT(ha, INJCT_HEARTBEAT_FAILURE)))) {
		ha->hw.hbeat_value = val;
		return 0;
	}
	device_printf(ha->pci_dev, "%s: Heartbeat Failue [0x%08x]\n",
		__func__, val);

	return -1;
}

static int
qla_init_nic_func(qla_host_t *ha)
{
        device_t                dev;
        q80_init_nic_func_t     *init_nic;
        q80_init_nic_func_rsp_t *init_nic_rsp;
        uint32_t                err;

        dev = ha->pci_dev;

        init_nic = (q80_init_nic_func_t *)ha->hw.mbox;
        bzero(init_nic, sizeof(q80_init_nic_func_t));

        init_nic->opcode = Q8_MBX_INIT_NIC_FUNC;
        init_nic->count_version = (sizeof (q80_init_nic_func_t) >> 2);
        init_nic->count_version |= Q8_MBX_CMD_VERSION;

        init_nic->options = Q8_INIT_NIC_REG_DCBX_CHNG_AEN;
        init_nic->options |= Q8_INIT_NIC_REG_SFP_CHNG_AEN;
        init_nic->options |= Q8_INIT_NIC_REG_IDC_AEN;

//qla_dump_buf8(ha, __func__, init_nic, sizeof (q80_init_nic_func_t));
        if (qla_mbx_cmd(ha, (uint32_t *)init_nic,
                (sizeof (q80_init_nic_func_t) >> 2),
                ha->hw.mbox, (sizeof (q80_init_nic_func_rsp_t) >> 2), 0)) {
                device_printf(dev, "%s: failed\n", __func__);
                return -1;
        }

        init_nic_rsp = (q80_init_nic_func_rsp_t *)ha->hw.mbox;
// qla_dump_buf8(ha, __func__, init_nic_rsp, sizeof (q80_init_nic_func_rsp_t));

        err = Q8_MBX_RSP_STATUS(init_nic_rsp->regcnt_status);

        if (err) {
                device_printf(dev, "%s: failed [0x%08x]\n", __func__, err);
        }

        return 0;
}

static int
qla_stop_nic_func(qla_host_t *ha)
{
        device_t                dev;
        q80_stop_nic_func_t     *stop_nic;
        q80_stop_nic_func_rsp_t *stop_nic_rsp;
        uint32_t                err;

        dev = ha->pci_dev;

        stop_nic = (q80_stop_nic_func_t *)ha->hw.mbox;
        bzero(stop_nic, sizeof(q80_stop_nic_func_t));

        stop_nic->opcode = Q8_MBX_STOP_NIC_FUNC;
        stop_nic->count_version = (sizeof (q80_stop_nic_func_t) >> 2);
        stop_nic->count_version |= Q8_MBX_CMD_VERSION;

        stop_nic->options = Q8_STOP_NIC_DEREG_DCBX_CHNG_AEN;
        stop_nic->options |= Q8_STOP_NIC_DEREG_SFP_CHNG_AEN;

//qla_dump_buf8(ha, __func__, stop_nic, sizeof (q80_stop_nic_func_t));
        if (qla_mbx_cmd(ha, (uint32_t *)stop_nic,
                (sizeof (q80_stop_nic_func_t) >> 2),
                ha->hw.mbox, (sizeof (q80_stop_nic_func_rsp_t) >> 2), 0)) {
                device_printf(dev, "%s: failed\n", __func__);
                return -1;
        }

        stop_nic_rsp = (q80_stop_nic_func_rsp_t *)ha->hw.mbox;
//qla_dump_buf8(ha, __func__, stop_nic_rsp, sizeof (q80_stop_nic_func_rsp_ t));

        err = Q8_MBX_RSP_STATUS(stop_nic_rsp->regcnt_status);

        if (err) {
                device_printf(dev, "%s: failed [0x%08x]\n", __func__, err);
        }

        return 0;
}

static int
qla_query_fw_dcbx_caps(qla_host_t *ha)
{
        device_t                        dev;
        q80_query_fw_dcbx_caps_t        *fw_dcbx;
        q80_query_fw_dcbx_caps_rsp_t    *fw_dcbx_rsp;
        uint32_t                        err;

        dev = ha->pci_dev;

        fw_dcbx = (q80_query_fw_dcbx_caps_t *)ha->hw.mbox;
        bzero(fw_dcbx, sizeof(q80_query_fw_dcbx_caps_t));

        fw_dcbx->opcode = Q8_MBX_GET_FW_DCBX_CAPS;
        fw_dcbx->count_version = (sizeof (q80_query_fw_dcbx_caps_t) >> 2);
        fw_dcbx->count_version |= Q8_MBX_CMD_VERSION;

        ql_dump_buf8(ha, __func__, fw_dcbx, sizeof (q80_query_fw_dcbx_caps_t));
        if (qla_mbx_cmd(ha, (uint32_t *)fw_dcbx,
                (sizeof (q80_query_fw_dcbx_caps_t) >> 2),
                ha->hw.mbox, (sizeof (q80_query_fw_dcbx_caps_rsp_t) >> 2), 0)) {
                device_printf(dev, "%s: failed\n", __func__);
                return -1;
        }

        fw_dcbx_rsp = (q80_query_fw_dcbx_caps_rsp_t *)ha->hw.mbox;
        ql_dump_buf8(ha, __func__, fw_dcbx_rsp,
                sizeof (q80_query_fw_dcbx_caps_rsp_t));

        err = Q8_MBX_RSP_STATUS(fw_dcbx_rsp->regcnt_status);

        if (err) {
                device_printf(dev, "%s: failed [0x%08x]\n", __func__, err);
        }

        return 0;
}

static int
qla_idc_ack(qla_host_t *ha, uint32_t aen_mb1, uint32_t aen_mb2,
        uint32_t aen_mb3, uint32_t aen_mb4)
{
        device_t                dev;
        q80_idc_ack_t           *idc_ack;
        q80_idc_ack_rsp_t       *idc_ack_rsp;
        uint32_t                err;
        int                     count = 300;

        dev = ha->pci_dev;

        idc_ack = (q80_idc_ack_t *)ha->hw.mbox;
        bzero(idc_ack, sizeof(q80_idc_ack_t));

        idc_ack->opcode = Q8_MBX_IDC_ACK;
        idc_ack->count_version = (sizeof (q80_idc_ack_t) >> 2);
        idc_ack->count_version |= Q8_MBX_CMD_VERSION;

        idc_ack->aen_mb1 = aen_mb1;
        idc_ack->aen_mb2 = aen_mb2;
        idc_ack->aen_mb3 = aen_mb3;
        idc_ack->aen_mb4 = aen_mb4;

        ha->hw.imd_compl= 0;

        if (qla_mbx_cmd(ha, (uint32_t *)idc_ack,
                (sizeof (q80_idc_ack_t) >> 2),
                ha->hw.mbox, (sizeof (q80_idc_ack_rsp_t) >> 2), 0)) {
                device_printf(dev, "%s: failed\n", __func__);
                return -1;
        }

        idc_ack_rsp = (q80_idc_ack_rsp_t *)ha->hw.mbox;

        err = Q8_MBX_RSP_STATUS(idc_ack_rsp->regcnt_status);

        if (err) {
                device_printf(dev, "%s: failed [0x%08x]\n", __func__, err);
                return(-1);
        }

        while (count && !ha->hw.imd_compl) {
                qla_mdelay(__func__, 100);
                count--;
        }

        if (!count)
                return -1;
        else
                device_printf(dev, "%s: count %d\n", __func__, count);

        return (0);
}

static int
qla_set_port_config(qla_host_t *ha, uint32_t cfg_bits)
{
        device_t                dev;
        q80_set_port_cfg_t      *pcfg;
        q80_set_port_cfg_rsp_t  *pfg_rsp;
        uint32_t                err;
        int                     count = 300;

        dev = ha->pci_dev;

        pcfg = (q80_set_port_cfg_t *)ha->hw.mbox;
        bzero(pcfg, sizeof(q80_set_port_cfg_t));

        pcfg->opcode = Q8_MBX_SET_PORT_CONFIG;
        pcfg->count_version = (sizeof (q80_set_port_cfg_t) >> 2);
        pcfg->count_version |= Q8_MBX_CMD_VERSION;

        pcfg->cfg_bits = cfg_bits;

        device_printf(dev, "%s: cfg_bits"
                " [STD_PAUSE_DIR, PAUSE_TYPE, DCBX]"
                " [0x%x, 0x%x, 0x%x]\n", __func__,
                ((cfg_bits & Q8_PORT_CFG_BITS_STDPAUSE_DIR_MASK)>>20),
                ((cfg_bits & Q8_PORT_CFG_BITS_PAUSE_CFG_MASK) >> 5),
                ((cfg_bits & Q8_PORT_CFG_BITS_DCBX_ENABLE) ? 1: 0));

        ha->hw.imd_compl= 0;

        if (qla_mbx_cmd(ha, (uint32_t *)pcfg,
                (sizeof (q80_set_port_cfg_t) >> 2),
                ha->hw.mbox, (sizeof (q80_set_port_cfg_rsp_t) >> 2), 0)) {
                device_printf(dev, "%s: failed\n", __func__);
                return -1;
        }

        pfg_rsp = (q80_set_port_cfg_rsp_t *)ha->hw.mbox;

        err = Q8_MBX_RSP_STATUS(pfg_rsp->regcnt_status);

        if (err == Q8_MBX_RSP_IDC_INTRMD_RSP) {
                while (count && !ha->hw.imd_compl) {
                        qla_mdelay(__func__, 100);
                        count--;
                }
                if (count) {
                        device_printf(dev, "%s: count %d\n", __func__, count);

                        err = 0;
                }
        }

        if (err) {
                device_printf(dev, "%s: failed [0x%08x]\n", __func__, err);
                return(-1);
        }

        return (0);
}


static int
qla_get_minidump_tmplt_size(qla_host_t *ha, uint32_t *size)
{
	uint32_t			err;
	device_t			dev = ha->pci_dev;
	q80_config_md_templ_size_t	*md_size;
	q80_config_md_templ_size_rsp_t	*md_size_rsp;

#ifdef QL_LDFLASH_FW

	*size = ql83xx_minidump_len;
	return (0);

#endif /* #ifdef QL_LDFLASH_FW */

	md_size = (q80_config_md_templ_size_t *) ha->hw.mbox;
	bzero(md_size, sizeof(q80_config_md_templ_size_t));

	md_size->opcode = Q8_MBX_GET_MINIDUMP_TMPLT_SIZE;
	md_size->count_version = (sizeof (q80_config_md_templ_size_t) >> 2);
	md_size->count_version |= Q8_MBX_CMD_VERSION;

	if (qla_mbx_cmd(ha, (uint32_t *) md_size,
		(sizeof(q80_config_md_templ_size_t) >> 2), ha->hw.mbox,
		(sizeof(q80_config_md_templ_size_rsp_t) >> 2), 0)) {

		device_printf(dev, "%s: failed\n", __func__);

		return (-1);
	}

	md_size_rsp = (q80_config_md_templ_size_rsp_t *) ha->hw.mbox;

	err = Q8_MBX_RSP_STATUS(md_size_rsp->regcnt_status);

        if (err) {
		device_printf(dev, "%s: failed [0x%08x]\n", __func__, err);
		return(-1);
        }

	*size = md_size_rsp->templ_size;

	return (0);
}

static int
qla_get_port_config(qla_host_t *ha, uint32_t *cfg_bits)
{
        device_t                dev;
        q80_get_port_cfg_t      *pcfg;
        q80_get_port_cfg_rsp_t  *pcfg_rsp;
        uint32_t                err;

        dev = ha->pci_dev;

        pcfg = (q80_get_port_cfg_t *)ha->hw.mbox;
        bzero(pcfg, sizeof(q80_get_port_cfg_t));

        pcfg->opcode = Q8_MBX_GET_PORT_CONFIG;
        pcfg->count_version = (sizeof (q80_get_port_cfg_t) >> 2);
        pcfg->count_version |= Q8_MBX_CMD_VERSION;

        if (qla_mbx_cmd(ha, (uint32_t *)pcfg,
                (sizeof (q80_get_port_cfg_t) >> 2),
                ha->hw.mbox, (sizeof (q80_get_port_cfg_rsp_t) >> 2), 0)) {
                device_printf(dev, "%s: failed\n", __func__);
                return -1;
        }

        pcfg_rsp = (q80_get_port_cfg_rsp_t *)ha->hw.mbox;

        err = Q8_MBX_RSP_STATUS(pcfg_rsp->regcnt_status);

        if (err) {
                device_printf(dev, "%s: failed [0x%08x]\n", __func__, err);
                return(-1);
        }

        device_printf(dev, "%s: [cfg_bits, port type]"
                " [0x%08x, 0x%02x] [STD_PAUSE_DIR, PAUSE_TYPE, DCBX]"
                " [0x%x, 0x%x, 0x%x]\n", __func__,
                pcfg_rsp->cfg_bits, pcfg_rsp->phys_port_type,
                ((pcfg_rsp->cfg_bits & Q8_PORT_CFG_BITS_STDPAUSE_DIR_MASK)>>20),
                ((pcfg_rsp->cfg_bits & Q8_PORT_CFG_BITS_PAUSE_CFG_MASK) >> 5),
                ((pcfg_rsp->cfg_bits & Q8_PORT_CFG_BITS_DCBX_ENABLE) ? 1: 0)
                );

        *cfg_bits = pcfg_rsp->cfg_bits;

        return (0);
}

int
qla_iscsi_pdu(qla_host_t *ha, struct mbuf *mp)
{
        struct ether_vlan_header        *eh;
        uint16_t                        etype;
        struct ip                       *ip = NULL;
        struct ip6_hdr                  *ip6 = NULL;
        struct tcphdr                   *th = NULL;
        uint32_t                        hdrlen;
        uint32_t                        offset;
        uint8_t                         buf[sizeof(struct ip6_hdr)];

        eh = mtod(mp, struct ether_vlan_header *);

        if (eh->evl_encap_proto == htons(ETHERTYPE_VLAN)) {
                hdrlen = ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN;
                etype = ntohs(eh->evl_proto);
        } else {
                hdrlen = ETHER_HDR_LEN;
                etype = ntohs(eh->evl_encap_proto);
        }

	if (etype == ETHERTYPE_IP) {

		offset = (hdrlen + sizeof (struct ip));

		if (mp->m_len >= offset) {
                        ip = (struct ip *)(mp->m_data + hdrlen);
		} else {
			m_copydata(mp, hdrlen, sizeof (struct ip), buf);
                        ip = (struct ip *)buf;
		}

                if (ip->ip_p == IPPROTO_TCP) {

			hdrlen += ip->ip_hl << 2;
			offset = hdrlen + 4;
	
			if (mp->m_len >= offset) {
				th = (struct tcphdr *)(mp->m_data + hdrlen);
			} else {
                                m_copydata(mp, hdrlen, 4, buf);
				th = (struct tcphdr *)buf;
			}
                }

	} else if (etype == ETHERTYPE_IPV6) {

		offset = (hdrlen + sizeof (struct ip6_hdr));

		if (mp->m_len >= offset) {
                        ip6 = (struct ip6_hdr *)(mp->m_data + hdrlen);
		} else {
                        m_copydata(mp, hdrlen, sizeof (struct ip6_hdr), buf);
                        ip6 = (struct ip6_hdr *)buf;
		}

                if (ip6->ip6_nxt == IPPROTO_TCP) {

			hdrlen += sizeof(struct ip6_hdr);
			offset = hdrlen + 4;

			if (mp->m_len >= offset) {
				th = (struct tcphdr *)(mp->m_data + hdrlen);
			} else {
				m_copydata(mp, hdrlen, 4, buf);
				th = (struct tcphdr *)buf;
			}
                }
	}

        if (th != NULL) {
                if ((th->th_sport == htons(3260)) ||
                        (th->th_dport == htons(3260)))
                        return 0;
        }
        return (-1);
}

void
qla_hw_async_event(qla_host_t *ha)
{
        switch (ha->hw.aen_mb0) {
        case 0x8101:
                (void)qla_idc_ack(ha, ha->hw.aen_mb1, ha->hw.aen_mb2,
                        ha->hw.aen_mb3, ha->hw.aen_mb4);

                break;

        default:
                break;
        }

        return;
}

#ifdef QL_LDFLASH_FW
static int
qla_get_minidump_template(qla_host_t *ha)
{
	uint32_t			err;
	device_t			dev = ha->pci_dev;
	q80_config_md_templ_cmd_t	*md_templ;
	q80_config_md_templ_cmd_rsp_t	*md_templ_rsp;

	md_templ = (q80_config_md_templ_cmd_t *) ha->hw.mbox;
	bzero(md_templ, (sizeof (q80_config_md_templ_cmd_t)));

	md_templ->opcode = Q8_MBX_GET_MINIDUMP_TMPLT;
	md_templ->count_version = ( sizeof(q80_config_md_templ_cmd_t) >> 2);
	md_templ->count_version |= Q8_MBX_CMD_VERSION;

	md_templ->buf_addr = ha->hw.dma_buf.minidump.dma_addr;
	md_templ->buff_size = ha->hw.dma_buf.minidump.size;

	if (qla_mbx_cmd(ha, (uint32_t *) md_templ,
		(sizeof(q80_config_md_templ_cmd_t) >> 2),
		 ha->hw.mbox,
		(sizeof(q80_config_md_templ_cmd_rsp_t) >> 2), 0)) {

		device_printf(dev, "%s: failed\n", __func__);

		return (-1);
	}

	md_templ_rsp = (q80_config_md_templ_cmd_rsp_t *) ha->hw.mbox;

	err = Q8_MBX_RSP_STATUS(md_templ_rsp->regcnt_status);

	if (err) {
		device_printf(dev, "%s: failed [0x%08x]\n", __func__, err);
		return (-1);
	}

	return (0);

}
#endif /* #ifdef QL_LDFLASH_FW */

static int
qla_minidump_init(qla_host_t *ha)
{
	int		ret = 0;
	uint32_t	template_size = 0;
	device_t	dev = ha->pci_dev;

	/*
	 * Get Minidump Template Size
 	 */
	ret = qla_get_minidump_tmplt_size(ha, &template_size);

	if (ret || (template_size == 0)) {
		device_printf(dev, "%s: failed [%d, %d]\n", __func__, ret,
			template_size);
		return (-1);
	}

	/*
	 * Allocate Memory for Minidump Template
	 */

	ha->hw.dma_buf.minidump.alignment = 8;
	ha->hw.dma_buf.minidump.size = template_size;

#ifdef QL_LDFLASH_FW
	if (ql_alloc_dmabuf(ha, &ha->hw.dma_buf.minidump)) {

		device_printf(dev, "%s: minidump dma alloc failed\n", __func__);

		return (-1);
	}
	ha->hw.dma_buf.flags.minidump = 1;

	/*
	 * Retrieve Minidump Template
	 */
	ret = qla_get_minidump_template(ha);
#else
	ha->hw.dma_buf.minidump.dma_b = ql83xx_minidump;
#endif /* #ifdef QL_LDFLASH_FW */

	if (ret) {
		qla_minidump_free(ha);
	} else {
		ha->hw.mdump_init = 1;
	}

	return (ret);
}


static void
qla_minidump_free(qla_host_t *ha)
{
	ha->hw.mdump_init = 0;
	if (ha->hw.dma_buf.flags.minidump) {
		ha->hw.dma_buf.flags.minidump = 0;
		ql_free_dmabuf(ha, &ha->hw.dma_buf.minidump);
	}
	return;
}

void
ql_minidump(qla_host_t *ha)
{
	uint32_t delay = 6000;

	if (!ha->hw.mdump_init)
		return;

	if (!ha->hw.mdump_active)
		return;

	if (ha->hw.mdump_active == 1) {
		ha->hw.mdump_start_seq_index = ql_stop_sequence(ha);
		ha->hw.mdump_start = 1;
	}

	while (delay-- && ha->hw.mdump_active) {
		qla_mdelay(__func__, 100);
	}
	ha->hw.mdump_start = 0;
	ql_start_sequence(ha, ha->hw.mdump_start_seq_index);

	return;
}
