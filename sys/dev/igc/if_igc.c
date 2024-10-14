/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2001-2024, Intel Corporation
 * Copyright (c) 2016 Nicole Graziano <nicole@nextbsd.org>
 * Copyright (c) 2021-2024 Rubicon Communications, LLC (Netgate)
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include "if_igc.h"
#include <sys/sbuf.h>
#include <machine/_inttypes.h>

#ifdef RSS
#include <net/rss_config.h>
#include <netinet/in_rss.h>
#endif

/*********************************************************************
 *  PCI Device ID Table
 *
 *  Used by probe to select devices to load on
 *  Last entry must be all 0s
 *
 *  { Vendor ID, Device ID, String }
 *********************************************************************/

static const pci_vendor_info_t igc_vendor_info_array[] =
{
	/* Intel(R) PRO/1000 Network Connection - igc */
	PVID(0x8086, IGC_DEV_ID_I225_LM, "Intel(R) Ethernet Controller I225-LM"),
	PVID(0x8086, IGC_DEV_ID_I225_V, "Intel(R) Ethernet Controller I225-V"),
	PVID(0x8086, IGC_DEV_ID_I225_K, "Intel(R) Ethernet Controller I225-K"),
	PVID(0x8086, IGC_DEV_ID_I225_I, "Intel(R) Ethernet Controller I225-I"),
	PVID(0x8086, IGC_DEV_ID_I220_V, "Intel(R) Ethernet Controller I220-V"),
	PVID(0x8086, IGC_DEV_ID_I225_K2, "Intel(R) Ethernet Controller I225-K(2)"),
	PVID(0x8086, IGC_DEV_ID_I225_LMVP, "Intel(R) Ethernet Controller I225-LMvP(2)"),
	PVID(0x8086, IGC_DEV_ID_I226_K, "Intel(R) Ethernet Controller I226-K"),
	PVID(0x8086, IGC_DEV_ID_I226_LMVP, "Intel(R) Ethernet Controller I226-LMvP"),
	PVID(0x8086, IGC_DEV_ID_I225_IT, "Intel(R) Ethernet Controller I225-IT(2)"),
	PVID(0x8086, IGC_DEV_ID_I226_LM, "Intel(R) Ethernet Controller I226-LM"),
	PVID(0x8086, IGC_DEV_ID_I226_V, "Intel(R) Ethernet Controller I226-V"),
	PVID(0x8086, IGC_DEV_ID_I226_IT, "Intel(R) Ethernet Controller I226-IT"),
	PVID(0x8086, IGC_DEV_ID_I221_V, "Intel(R) Ethernet Controller I221-V"),
	PVID(0x8086, IGC_DEV_ID_I226_BLANK_NVM, "Intel(R) Ethernet Controller I226(blankNVM)"),
	PVID(0x8086, IGC_DEV_ID_I225_BLANK_NVM, "Intel(R) Ethernet Controller I225(blankNVM)"),
	/* required last entry */
	PVID_END
};

/*********************************************************************
 *  Function prototypes
 *********************************************************************/
static void	*igc_register(device_t);
static int	igc_if_attach_pre(if_ctx_t);
static int	igc_if_attach_post(if_ctx_t);
static int	igc_if_detach(if_ctx_t);
static int	igc_if_shutdown(if_ctx_t);
static int	igc_if_suspend(if_ctx_t);
static int	igc_if_resume(if_ctx_t);

static int	igc_if_tx_queues_alloc(if_ctx_t, caddr_t *, uint64_t *, int, int);
static int	igc_if_rx_queues_alloc(if_ctx_t, caddr_t *, uint64_t *, int, int);
static void	igc_if_queues_free(if_ctx_t);

static uint64_t	igc_if_get_counter(if_ctx_t, ift_counter);
static void	igc_if_init(if_ctx_t);
static void	igc_if_stop(if_ctx_t);
static void	igc_if_media_status(if_ctx_t, struct ifmediareq *);
static int	igc_if_media_change(if_ctx_t);
static int	igc_if_mtu_set(if_ctx_t, uint32_t);
static void	igc_if_timer(if_ctx_t, uint16_t);
static void	igc_if_watchdog_reset(if_ctx_t);
static bool	igc_if_needs_restart(if_ctx_t, enum iflib_restart_event);

static void	igc_identify_hardware(if_ctx_t);
static int	igc_allocate_pci_resources(if_ctx_t);
static void	igc_free_pci_resources(if_ctx_t);
static void	igc_reset(if_ctx_t);
static int	igc_setup_interface(if_ctx_t);
static int	igc_setup_msix(if_ctx_t);

static void	igc_initialize_transmit_unit(if_ctx_t);
static void	igc_initialize_receive_unit(if_ctx_t);

static void	igc_if_intr_enable(if_ctx_t);
static void	igc_if_intr_disable(if_ctx_t);
static int	igc_if_rx_queue_intr_enable(if_ctx_t, uint16_t);
static int	igc_if_tx_queue_intr_enable(if_ctx_t, uint16_t);
static void	igc_if_multi_set(if_ctx_t);
static void	igc_if_update_admin_status(if_ctx_t);
static void	igc_if_debug(if_ctx_t);
static void	igc_update_stats_counters(struct igc_softc *);
static void	igc_add_hw_stats(struct igc_softc *);
static int	igc_if_set_promisc(if_ctx_t, int);
static void	igc_setup_vlan_hw_support(if_ctx_t);
static void	igc_fw_version(struct igc_softc *);
static void	igc_sbuf_fw_version(struct igc_fw_version *, struct sbuf *);
static void	igc_print_fw_version(struct igc_softc *);
static int	igc_sysctl_print_fw_version(SYSCTL_HANDLER_ARGS);
static int	igc_sysctl_nvm_info(SYSCTL_HANDLER_ARGS);
static void	igc_print_nvm_info(struct igc_softc *);
static int	igc_sysctl_debug_info(SYSCTL_HANDLER_ARGS);
static int	igc_get_rs(SYSCTL_HANDLER_ARGS);
static void	igc_print_debug_info(struct igc_softc *);
static int 	igc_is_valid_ether_addr(u8 *);
static void	igc_neweitr(struct igc_softc *, struct igc_rx_queue *,
    struct tx_ring *, struct rx_ring *);
/* Management and WOL Support */
static void	igc_get_hw_control(struct igc_softc *);
static void	igc_release_hw_control(struct igc_softc *);
static void	igc_get_wakeup(if_ctx_t);
static void	igc_enable_wakeup(if_ctx_t);

int		igc_intr(void *);

/* MSI-X handlers */
static int	igc_if_msix_intr_assign(if_ctx_t, int);
static int	igc_msix_link(void *);
static void	igc_handle_link(void *context);

static int	igc_set_flowcntl(SYSCTL_HANDLER_ARGS);
static int	igc_sysctl_dmac(SYSCTL_HANDLER_ARGS);
static int	igc_sysctl_eee(SYSCTL_HANDLER_ARGS);

static int	igc_get_regs(SYSCTL_HANDLER_ARGS);

static void	igc_configure_queues(struct igc_softc *);


/*********************************************************************
 *  FreeBSD Device Interface Entry Points
 *********************************************************************/
static device_method_t igc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_register, igc_register),
	DEVMETHOD(device_probe, iflib_device_probe),
	DEVMETHOD(device_attach, iflib_device_attach),
	DEVMETHOD(device_detach, iflib_device_detach),
	DEVMETHOD(device_shutdown, iflib_device_shutdown),
	DEVMETHOD(device_suspend, iflib_device_suspend),
	DEVMETHOD(device_resume, iflib_device_resume),
	DEVMETHOD_END
};

static driver_t igc_driver = {
	"igc", igc_methods, sizeof(struct igc_softc),
};

DRIVER_MODULE(igc, pci, igc_driver, 0, 0);

MODULE_DEPEND(igc, pci, 1, 1, 1);
MODULE_DEPEND(igc, ether, 1, 1, 1);
MODULE_DEPEND(igc, iflib, 1, 1, 1);

IFLIB_PNP_INFO(pci, igc, igc_vendor_info_array);

static device_method_t igc_if_methods[] = {
	DEVMETHOD(ifdi_attach_pre, igc_if_attach_pre),
	DEVMETHOD(ifdi_attach_post, igc_if_attach_post),
	DEVMETHOD(ifdi_detach, igc_if_detach),
	DEVMETHOD(ifdi_shutdown, igc_if_shutdown),
	DEVMETHOD(ifdi_suspend, igc_if_suspend),
	DEVMETHOD(ifdi_resume, igc_if_resume),
	DEVMETHOD(ifdi_init, igc_if_init),
	DEVMETHOD(ifdi_stop, igc_if_stop),
	DEVMETHOD(ifdi_msix_intr_assign, igc_if_msix_intr_assign),
	DEVMETHOD(ifdi_intr_enable, igc_if_intr_enable),
	DEVMETHOD(ifdi_intr_disable, igc_if_intr_disable),
	DEVMETHOD(ifdi_tx_queues_alloc, igc_if_tx_queues_alloc),
	DEVMETHOD(ifdi_rx_queues_alloc, igc_if_rx_queues_alloc),
	DEVMETHOD(ifdi_queues_free, igc_if_queues_free),
	DEVMETHOD(ifdi_update_admin_status, igc_if_update_admin_status),
	DEVMETHOD(ifdi_multi_set, igc_if_multi_set),
	DEVMETHOD(ifdi_media_status, igc_if_media_status),
	DEVMETHOD(ifdi_media_change, igc_if_media_change),
	DEVMETHOD(ifdi_mtu_set, igc_if_mtu_set),
	DEVMETHOD(ifdi_promisc_set, igc_if_set_promisc),
	DEVMETHOD(ifdi_timer, igc_if_timer),
	DEVMETHOD(ifdi_watchdog_reset, igc_if_watchdog_reset),
	DEVMETHOD(ifdi_get_counter, igc_if_get_counter),
	DEVMETHOD(ifdi_rx_queue_intr_enable, igc_if_rx_queue_intr_enable),
	DEVMETHOD(ifdi_tx_queue_intr_enable, igc_if_tx_queue_intr_enable),
	DEVMETHOD(ifdi_debug, igc_if_debug),
	DEVMETHOD(ifdi_needs_restart, igc_if_needs_restart),
	DEVMETHOD_END
};

static driver_t igc_if_driver = {
	"igc_if", igc_if_methods, sizeof(struct igc_softc)
};

/*********************************************************************
 *  Tunable default values.
 *********************************************************************/

/* Allow common code without TSO */
#ifndef CSUM_TSO
#define CSUM_TSO	0
#endif

static SYSCTL_NODE(_hw, OID_AUTO, igc, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "igc driver parameters");

static int igc_disable_crc_stripping = 0;
SYSCTL_INT(_hw_igc, OID_AUTO, disable_crc_stripping, CTLFLAG_RDTUN,
    &igc_disable_crc_stripping, 0, "Disable CRC Stripping");

static int igc_smart_pwr_down = false;
SYSCTL_INT(_hw_igc, OID_AUTO, smart_pwr_down, CTLFLAG_RDTUN, &igc_smart_pwr_down,
    0, "Set to true to leave smart power down enabled on newer adapters");

/* Controls whether promiscuous also shows bad packets */
static int igc_debug_sbp = true;
SYSCTL_INT(_hw_igc, OID_AUTO, sbp, CTLFLAG_RDTUN, &igc_debug_sbp, 0,
    "Show bad packets in promiscuous mode");

/* Energy efficient ethernet - default to OFF */
static int igc_eee_setting = 1;
SYSCTL_INT(_hw_igc, OID_AUTO, eee_setting, CTLFLAG_RDTUN, &igc_eee_setting, 0,
    "Enable Energy Efficient Ethernet");

/*
 * AIM: Adaptive Interrupt Moderation
 * which means that the interrupt rate is varied over time based on the
 * traffic for that interrupt vector
 */
static int igc_enable_aim = 1;
SYSCTL_INT(_hw_igc, OID_AUTO, enable_aim, CTLFLAG_RWTUN, &igc_enable_aim,
    0, "Enable adaptive interrupt moderation (1=normal, 2=lowlatency)");

/*
** Tuneable Interrupt rate
*/
static int igc_max_interrupt_rate = IGC_INTS_DEFAULT;
SYSCTL_INT(_hw_igc, OID_AUTO, max_interrupt_rate, CTLFLAG_RDTUN,
    &igc_max_interrupt_rate, 0, "Maximum interrupts per second");

extern struct if_txrx igc_txrx;

static struct if_shared_ctx igc_sctx_init = {
	.isc_magic = IFLIB_MAGIC,
	.isc_q_align = PAGE_SIZE,
	.isc_tx_maxsize = IGC_TSO_SIZE + sizeof(struct ether_vlan_header),
	.isc_tx_maxsegsize = PAGE_SIZE,
	.isc_tso_maxsize = IGC_TSO_SIZE + sizeof(struct ether_vlan_header),
	.isc_tso_maxsegsize = IGC_TSO_SEG_SIZE,
	.isc_rx_maxsize = MAX_JUMBO_FRAME_SIZE,
	.isc_rx_nsegments = 1,
	.isc_rx_maxsegsize = MJUM9BYTES,
	.isc_nfl = 1,
	.isc_nrxqs = 1,
	.isc_ntxqs = 1,
	.isc_admin_intrcnt = 1,
	.isc_vendor_info = igc_vendor_info_array,
	.isc_driver_version = "1",
	.isc_driver = &igc_if_driver,
	.isc_flags = IFLIB_NEED_SCRATCH | IFLIB_TSO_INIT_IP | IFLIB_NEED_ZERO_CSUM,

	.isc_nrxd_min = {IGC_MIN_RXD},
	.isc_ntxd_min = {IGC_MIN_TXD},
	.isc_nrxd_max = {IGC_MAX_RXD},
	.isc_ntxd_max = {IGC_MAX_TXD},
	.isc_nrxd_default = {IGC_DEFAULT_RXD},
	.isc_ntxd_default = {IGC_DEFAULT_TXD},
};

/*****************************************************************
 *
 * Dump Registers
 *
 ****************************************************************/
#define IGC_REGS_LEN 739

static int igc_get_regs(SYSCTL_HANDLER_ARGS)
{
	struct igc_softc *sc = (struct igc_softc *)arg1;
	struct igc_hw *hw = &sc->hw;
	struct sbuf *sb;
	u32 *regs_buff;
	int rc;

	regs_buff = malloc(sizeof(u32) * IGC_REGS_LEN, M_DEVBUF, M_WAITOK);
	memset(regs_buff, 0, IGC_REGS_LEN * sizeof(u32));

	rc = sysctl_wire_old_buffer(req, 0);
	MPASS(rc == 0);
	if (rc != 0) {
		free(regs_buff, M_DEVBUF);
		return (rc);
	}

	sb = sbuf_new_for_sysctl(NULL, NULL, 32*400, req);
	MPASS(sb != NULL);
	if (sb == NULL) {
		free(regs_buff, M_DEVBUF);
		return (ENOMEM);
	}

	/* General Registers */
	regs_buff[0] = IGC_READ_REG(hw, IGC_CTRL);
	regs_buff[1] = IGC_READ_REG(hw, IGC_STATUS);
	regs_buff[2] = IGC_READ_REG(hw, IGC_CTRL_EXT);
	regs_buff[3] = IGC_READ_REG(hw, IGC_ICR);
	regs_buff[4] = IGC_READ_REG(hw, IGC_RCTL);
	regs_buff[5] = IGC_READ_REG(hw, IGC_RDLEN(0));
	regs_buff[6] = IGC_READ_REG(hw, IGC_RDH(0));
	regs_buff[7] = IGC_READ_REG(hw, IGC_RDT(0));
	regs_buff[8] = IGC_READ_REG(hw, IGC_RXDCTL(0));
	regs_buff[9] = IGC_READ_REG(hw, IGC_RDBAL(0));
	regs_buff[10] = IGC_READ_REG(hw, IGC_RDBAH(0));
	regs_buff[11] = IGC_READ_REG(hw, IGC_TCTL);
	regs_buff[12] = IGC_READ_REG(hw, IGC_TDBAL(0));
	regs_buff[13] = IGC_READ_REG(hw, IGC_TDBAH(0));
	regs_buff[14] = IGC_READ_REG(hw, IGC_TDLEN(0));
	regs_buff[15] = IGC_READ_REG(hw, IGC_TDH(0));
	regs_buff[16] = IGC_READ_REG(hw, IGC_TDT(0));
	regs_buff[17] = IGC_READ_REG(hw, IGC_TXDCTL(0));

	sbuf_printf(sb, "General Registers\n");
	sbuf_printf(sb, "\tCTRL\t %08x\n", regs_buff[0]);
	sbuf_printf(sb, "\tSTATUS\t %08x\n", regs_buff[1]);
	sbuf_printf(sb, "\tCTRL_EXIT\t %08x\n\n", regs_buff[2]);

	sbuf_printf(sb, "Interrupt Registers\n");
	sbuf_printf(sb, "\tICR\t %08x\n\n", regs_buff[3]);

	sbuf_printf(sb, "RX Registers\n");
	sbuf_printf(sb, "\tRCTL\t %08x\n", regs_buff[4]);
	sbuf_printf(sb, "\tRDLEN\t %08x\n", regs_buff[5]);
	sbuf_printf(sb, "\tRDH\t %08x\n", regs_buff[6]);
	sbuf_printf(sb, "\tRDT\t %08x\n", regs_buff[7]);
	sbuf_printf(sb, "\tRXDCTL\t %08x\n", regs_buff[8]);
	sbuf_printf(sb, "\tRDBAL\t %08x\n", regs_buff[9]);
	sbuf_printf(sb, "\tRDBAH\t %08x\n\n", regs_buff[10]);

	sbuf_printf(sb, "TX Registers\n");
	sbuf_printf(sb, "\tTCTL\t %08x\n", regs_buff[11]);
	sbuf_printf(sb, "\tTDBAL\t %08x\n", regs_buff[12]);
	sbuf_printf(sb, "\tTDBAH\t %08x\n", regs_buff[13]);
	sbuf_printf(sb, "\tTDLEN\t %08x\n", regs_buff[14]);
	sbuf_printf(sb, "\tTDH\t %08x\n", regs_buff[15]);
	sbuf_printf(sb, "\tTDT\t %08x\n", regs_buff[16]);
	sbuf_printf(sb, "\tTXDCTL\t %08x\n", regs_buff[17]);
	sbuf_printf(sb, "\tTDFH\t %08x\n", regs_buff[18]);
	sbuf_printf(sb, "\tTDFT\t %08x\n", regs_buff[19]);
	sbuf_printf(sb, "\tTDFHS\t %08x\n", regs_buff[20]);
	sbuf_printf(sb, "\tTDFPC\t %08x\n\n", regs_buff[21]);

	free(regs_buff, M_DEVBUF);

#ifdef DUMP_DESCS
	{
		if_softc_ctx_t scctx = sc->shared;
		struct rx_ring *rxr = &rx_que->rxr;
		struct tx_ring *txr = &tx_que->txr;
		int ntxd = scctx->isc_ntxd[0];
		int nrxd = scctx->isc_nrxd[0];
		int j;

	for (j = 0; j < nrxd; j++) {
		u32 staterr = le32toh(rxr->rx_base[j].wb.upper.status_error);
		u32 length =  le32toh(rxr->rx_base[j].wb.upper.length);
		sbuf_printf(sb, "\tReceive Descriptor Address %d: %08" PRIx64 "  Error:%d  Length:%d\n", j, rxr->rx_base[j].read.buffer_addr, staterr, length);
	}

	for (j = 0; j < min(ntxd, 256); j++) {
		unsigned int *ptr = (unsigned int *)&txr->tx_base[j];

		sbuf_printf(sb, "\tTXD[%03d] [0]: %08x [1]: %08x [2]: %08x [3]: %08x  eop: %d DD=%d\n",
			    j, ptr[0], ptr[1], ptr[2], ptr[3], buf->eop,
			    buf->eop != -1 ? txr->tx_base[buf->eop].upper.fields.status & IGC_TXD_STAT_DD : 0);

	}
	}
#endif

	rc = sbuf_finish(sb);
	sbuf_delete(sb);
	return(rc);
}

static void *
igc_register(device_t dev)
{
	return (&igc_sctx_init);
}

static int
igc_set_num_queues(if_ctx_t ctx)
{
	int maxqueues;

	maxqueues = 4;

	return (maxqueues);
}

#define	IGC_CAPS							\
    IFCAP_HWCSUM | IFCAP_VLAN_MTU | IFCAP_VLAN_HWTAGGING |		\
    IFCAP_VLAN_HWCSUM | IFCAP_WOL | IFCAP_TSO4 | IFCAP_LRO |		\
    IFCAP_VLAN_HWTSO | IFCAP_JUMBO_MTU | IFCAP_HWCSUM_IPV6 | IFCAP_TSO6

/*********************************************************************
 *  Device initialization routine
 *
 *  The attach entry point is called when the driver is being loaded.
 *  This routine identifies the type of hardware, allocates all resources
 *  and initializes the hardware.
 *
 *  return 0 on success, positive on failure
 *********************************************************************/
static int
igc_if_attach_pre(if_ctx_t ctx)
{
	struct igc_softc *sc;
	if_softc_ctx_t scctx;
	device_t dev;
	struct igc_hw *hw;
	int error = 0;

	INIT_DEBUGOUT("igc_if_attach_pre: begin");
	dev = iflib_get_dev(ctx);
	sc = iflib_get_softc(ctx);

	sc->ctx = sc->osdep.ctx = ctx;
	sc->dev = sc->osdep.dev = dev;
	scctx = sc->shared = iflib_get_softc_ctx(ctx);
	sc->media = iflib_get_media(ctx);
	hw = &sc->hw;

	/* SYSCTL stuff */
	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "nvm", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT,
	    sc, 0, igc_sysctl_nvm_info, "I", "NVM Information");

	sc->enable_aim = igc_enable_aim;
	SYSCTL_ADD_INT(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "enable_aim", CTLFLAG_RW,
	    &sc->enable_aim, 0,
	    "Interrupt Moderation (1=normal, 2=lowlatency)");

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "fw_version", CTLTYPE_STRING | CTLFLAG_RD,
	    sc, 0, igc_sysctl_print_fw_version, "A",
	    "Prints FW/NVM Versions");

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "debug", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT,
	    sc, 0, igc_sysctl_debug_info, "I", "Debug Information");

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "fc", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT,
	    sc, 0, igc_set_flowcntl, "I", "Flow Control");

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "reg_dump",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_NEEDGIANT, sc, 0,
	    igc_get_regs, "A", "Dump Registers");

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "rs_dump",
	    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT, sc, 0,
	    igc_get_rs, "I", "Dump RS indexes");

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "dmac",
	    CTLTYPE_INT | CTLFLAG_RW, sc, 0,
	    igc_sysctl_dmac, "I", "DMA Coalesce");

	/* Determine hardware and mac info */
	igc_identify_hardware(ctx);

	scctx->isc_tx_nsegments = IGC_MAX_SCATTER;
	scctx->isc_nrxqsets_max = scctx->isc_ntxqsets_max = igc_set_num_queues(ctx);
	if (bootverbose)
		device_printf(dev, "attach_pre capping queues at %d\n",
		    scctx->isc_ntxqsets_max);

	scctx->isc_txqsizes[0] = roundup2(scctx->isc_ntxd[0] * sizeof(union igc_adv_tx_desc), IGC_DBA_ALIGN);
	scctx->isc_rxqsizes[0] = roundup2(scctx->isc_nrxd[0] * sizeof(union igc_adv_rx_desc), IGC_DBA_ALIGN);
	scctx->isc_txd_size[0] = sizeof(union igc_adv_tx_desc);
	scctx->isc_rxd_size[0] = sizeof(union igc_adv_rx_desc);
	scctx->isc_txrx = &igc_txrx;
	scctx->isc_tx_tso_segments_max = IGC_MAX_SCATTER;
	scctx->isc_tx_tso_size_max = IGC_TSO_SIZE;
	scctx->isc_tx_tso_segsize_max = IGC_TSO_SEG_SIZE;
	scctx->isc_capabilities = scctx->isc_capenable = IGC_CAPS;
	scctx->isc_tx_csum_flags = CSUM_TCP | CSUM_UDP | CSUM_TSO |
		CSUM_IP6_TCP | CSUM_IP6_UDP | CSUM_SCTP | CSUM_IP6_SCTP;

	/*
	** Some new devices, as with ixgbe, now may
	** use a different BAR, so we need to keep
	** track of which is used.
	*/
	scctx->isc_msix_bar = PCIR_BAR(IGC_MSIX_BAR);
	if (pci_read_config(dev, scctx->isc_msix_bar, 4) == 0)
		scctx->isc_msix_bar += 4;

	/* Setup PCI resources */
	if (igc_allocate_pci_resources(ctx)) {
		device_printf(dev, "Allocation of PCI resources failed\n");
		error = ENXIO;
		goto err_pci;
	}

	/* Do Shared Code initialization */
	error = igc_setup_init_funcs(hw, true);
	if (error) {
		device_printf(dev, "Setup of Shared code failed, error %d\n",
		    error);
		error = ENXIO;
		goto err_pci;
	}

	igc_setup_msix(ctx);
	igc_get_bus_info(hw);

	hw->mac.autoneg = DO_AUTO_NEG;
	hw->phy.autoneg_wait_to_complete = false;
	hw->phy.autoneg_advertised = AUTONEG_ADV_DEFAULT;

	/* Copper options */
	if (hw->phy.media_type == igc_media_type_copper) {
		hw->phy.mdix = AUTO_ALL_MODES;
	}

	/*
	 * Set the frame limits assuming
	 * standard ethernet sized frames.
	 */
	scctx->isc_max_frame_size = sc->hw.mac.max_frame_size =
	    ETHERMTU + ETHER_HDR_LEN + ETHERNET_FCS_SIZE;

	/* Allocate multicast array memory. */
	sc->mta = malloc(sizeof(u8) * ETHER_ADDR_LEN *
	    MAX_NUM_MULTICAST_ADDRESSES, M_DEVBUF, M_NOWAIT);
	if (sc->mta == NULL) {
		device_printf(dev, "Can not allocate multicast setup array\n");
		error = ENOMEM;
		goto err_late;
	}

	/* Check SOL/IDER usage */
	if (igc_check_reset_block(hw))
		device_printf(dev, "PHY reset is blocked"
			      " due to SOL/IDER session.\n");

	/* Sysctl for setting Energy Efficient Ethernet */
	sc->hw.dev_spec._i225.eee_disable = igc_eee_setting;
	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "eee_control",
	    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT,
	    sc, 0, igc_sysctl_eee, "I",
	    "Disable Energy Efficient Ethernet");

	/*
	** Start from a known state, this is
	** important in reading the nvm and
	** mac from that.
	*/
	igc_reset_hw(hw);

	/* Make sure we have a good EEPROM before we read from it */
	if (igc_validate_nvm_checksum(hw) < 0) {
		/*
		** Some PCI-E parts fail the first check due to
		** the link being in sleep state, call it again,
		** if it fails a second time its a real issue.
		*/
		if (igc_validate_nvm_checksum(hw) < 0) {
			device_printf(dev,
			    "The EEPROM Checksum Is Not Valid\n");
			error = EIO;
			goto err_late;
		}
	}

	/* Copy the permanent MAC address out of the EEPROM */
	if (igc_read_mac_addr(hw) < 0) {
		device_printf(dev, "EEPROM read error while reading MAC"
			      " address\n");
		error = EIO;
		goto err_late;
	}

	if (!igc_is_valid_ether_addr(hw->mac.addr)) {
		device_printf(dev, "Invalid MAC address\n");
		error = EIO;
		goto err_late;
	}

	/* Save the EEPROM/NVM versions */
	igc_fw_version(sc);

	igc_print_fw_version(sc);

	/*
	 * Get Wake-on-Lan and Management info for later use
	 */
	igc_get_wakeup(ctx);

	/* Enable only WOL MAGIC by default */
	scctx->isc_capenable &= ~IFCAP_WOL;
	if (sc->wol != 0)
		scctx->isc_capenable |= IFCAP_WOL_MAGIC;

	iflib_set_mac(ctx, hw->mac.addr);

	return (0);

err_late:
	igc_release_hw_control(sc);
err_pci:
	igc_free_pci_resources(ctx);
	free(sc->mta, M_DEVBUF);

	return (error);
}

static int
igc_if_attach_post(if_ctx_t ctx)
{
	struct igc_softc *sc = iflib_get_softc(ctx);
	struct igc_hw *hw = &sc->hw;
	int error = 0;

	/* Setup OS specific network interface */
	error = igc_setup_interface(ctx);
	if (error != 0) {
		goto err_late;
	}

	igc_reset(ctx);

	/* Initialize statistics */
	igc_update_stats_counters(sc);
	hw->mac.get_link_status = true;
	igc_if_update_admin_status(ctx);
	igc_add_hw_stats(sc);

	/* the driver can now take control from firmware */
	igc_get_hw_control(sc);

	INIT_DEBUGOUT("igc_if_attach_post: end");

	return (error);

err_late:
	igc_release_hw_control(sc);
	igc_free_pci_resources(ctx);
	igc_if_queues_free(ctx);
	free(sc->mta, M_DEVBUF);

	return (error);
}

/*********************************************************************
 *  Device removal routine
 *
 *  The detach entry point is called when the driver is being removed.
 *  This routine stops the adapter and deallocates all the resources
 *  that were allocated for driver operation.
 *
 *  return 0 on success, positive on failure
 *********************************************************************/
static int
igc_if_detach(if_ctx_t ctx)
{
	struct igc_softc	*sc = iflib_get_softc(ctx);

	INIT_DEBUGOUT("igc_if_detach: begin");

	igc_phy_hw_reset(&sc->hw);

	igc_release_hw_control(sc);
	igc_free_pci_resources(ctx);

	return (0);
}

/*********************************************************************
 *
 *  Shutdown entry point
 *
 **********************************************************************/

static int
igc_if_shutdown(if_ctx_t ctx)
{
	return igc_if_suspend(ctx);
}

/*
 * Suspend/resume device methods.
 */
static int
igc_if_suspend(if_ctx_t ctx)
{
	struct igc_softc *sc = iflib_get_softc(ctx);

	igc_release_hw_control(sc);
	igc_enable_wakeup(ctx);
	return (0);
}

static int
igc_if_resume(if_ctx_t ctx)
{
	igc_if_init(ctx);

	return(0);
}

static int
igc_if_mtu_set(if_ctx_t ctx, uint32_t mtu)
{
	int max_frame_size;
	struct igc_softc *sc = iflib_get_softc(ctx);
	if_softc_ctx_t scctx = iflib_get_softc_ctx(ctx);

	 IOCTL_DEBUGOUT("ioctl rcv'd: SIOCSIFMTU (Set Interface MTU)");

	 /* 9K Jumbo Frame size */
	 max_frame_size = 9234;

	if (mtu > max_frame_size - ETHER_HDR_LEN - ETHER_CRC_LEN) {
		return (EINVAL);
	}

	scctx->isc_max_frame_size = sc->hw.mac.max_frame_size =
	    mtu + ETHER_HDR_LEN + ETHER_CRC_LEN;
	return (0);
}

/*********************************************************************
 *  Init entry point
 *
 *  This routine is used in two ways. It is used by the stack as
 *  init entry point in network interface structure. It is also used
 *  by the driver as a hw/sw initialization routine to get to a
 *  consistent state.
 *
 **********************************************************************/
static void
igc_if_init(if_ctx_t ctx)
{
	struct igc_softc *sc = iflib_get_softc(ctx);
	if_softc_ctx_t scctx = sc->shared;
	if_t ifp = iflib_get_ifp(ctx);
	struct igc_tx_queue *tx_que;
	int i;

	INIT_DEBUGOUT("igc_if_init: begin");

	/* Get the latest mac address, User can use a LAA */
	bcopy(if_getlladdr(ifp), sc->hw.mac.addr,
	    ETHER_ADDR_LEN);

	/* Put the address into the Receive Address Array */
	igc_rar_set(&sc->hw, sc->hw.mac.addr, 0);

	/* Initialize the hardware */
	igc_reset(ctx);
	igc_if_update_admin_status(ctx);

	for (i = 0, tx_que = sc->tx_queues; i < sc->tx_num_queues; i++, tx_que++) {
		struct tx_ring *txr = &tx_que->txr;

		txr->tx_rs_cidx = txr->tx_rs_pidx;

		/* Initialize the last processed descriptor to be the end of
		 * the ring, rather than the start, so that we avoid an
		 * off-by-one error when calculating how many descriptors are
		 * done in the credits_update function.
		 */
		txr->tx_cidx_processed = scctx->isc_ntxd[0] - 1;
	}

	/* Setup VLAN support, basic and offload if available */
	IGC_WRITE_REG(&sc->hw, IGC_VET, ETHERTYPE_VLAN);

	/* Prepare transmit descriptors and buffers */
	igc_initialize_transmit_unit(ctx);

	/* Setup Multicast table */
	igc_if_multi_set(ctx);

	sc->rx_mbuf_sz = iflib_get_rx_mbuf_sz(ctx);
	igc_initialize_receive_unit(ctx);

	/* Set up VLAN support */
	igc_setup_vlan_hw_support(ctx);

	/* Don't lose promiscuous settings */
	igc_if_set_promisc(ctx, if_getflags(ifp));
	igc_clear_hw_cntrs_base_generic(&sc->hw);

	if (sc->intr_type == IFLIB_INTR_MSIX) /* Set up queue routing */
		igc_configure_queues(sc);

	/* this clears any pending interrupts */
	IGC_READ_REG(&sc->hw, IGC_ICR);
	IGC_WRITE_REG(&sc->hw, IGC_ICS, IGC_ICS_LSC);

	/* the driver can now take control from firmware */
	igc_get_hw_control(sc);

	/* Set Energy Efficient Ethernet */
	igc_set_eee_i225(&sc->hw, true, true, true);
}

enum eitr_latency_target {
	eitr_latency_disabled = 0,
	eitr_latency_lowest = 1,
	eitr_latency_low = 2,
	eitr_latency_bulk = 3
};
/*********************************************************************
 *
 *  Helper to calculate next EITR value for AIM
 *
 *********************************************************************/
static void
igc_neweitr(struct igc_softc *sc, struct igc_rx_queue *que,
    struct tx_ring *txr, struct rx_ring *rxr)
{
	struct igc_hw *hw = &sc->hw;
	u32 neweitr;
	u32 bytes;
	u32 bytes_packets;
	u32 packets;
	u8 nextlatency;

	/* Idle, do nothing */
	if ((txr->tx_bytes == 0) && (rxr->rx_bytes == 0))
		return;

	neweitr = 0;

	if (sc->enable_aim) {
		nextlatency = rxr->rx_nextlatency;

		/* Use half default (4K) ITR if sub-gig */
		if (sc->link_speed < 1000) {
			neweitr = IGC_INTS_4K;
			goto igc_set_next_eitr;
		}
		/* Want at least enough packet buffer for two frames to AIM */
		if (sc->shared->isc_max_frame_size * 2 > (sc->pba << 10)) {
			neweitr = igc_max_interrupt_rate;
			sc->enable_aim = 0;
			goto igc_set_next_eitr;
		}

		/* Get the largest values from the associated tx and rx ring */
		if (txr->tx_bytes && txr->tx_packets) {
			bytes = txr->tx_bytes;
			bytes_packets = txr->tx_bytes/txr->tx_packets;
			packets = txr->tx_packets;
		}
		if (rxr->rx_bytes && rxr->rx_packets) {
			bytes = max(bytes, rxr->rx_bytes);
			bytes_packets = max(bytes_packets, rxr->rx_bytes/rxr->rx_packets);
			packets = max(packets, rxr->rx_packets);
		}

		/* Latency state machine */
		switch (nextlatency) {
		case eitr_latency_disabled: /* Bootstrapping */
			nextlatency = eitr_latency_low;
			break;
		case eitr_latency_lowest: /* 70k ints/s */
			/* TSO and jumbo frames */
			if (bytes_packets > 8000)
				nextlatency = eitr_latency_bulk;
			else if ((packets < 5) && (bytes > 512))
				nextlatency = eitr_latency_low;
			break;
		case eitr_latency_low: /* 20k ints/s */
			if (bytes > 10000) {
				/* Handle TSO */
				if (bytes_packets > 8000)
					nextlatency = eitr_latency_bulk;
				else if ((packets < 10) || (bytes_packets > 1200))
					nextlatency = eitr_latency_bulk;
				else if (packets > 35)
					nextlatency = eitr_latency_lowest;
			} else if (bytes_packets > 2000) {
				nextlatency = eitr_latency_bulk;
			} else if (packets < 3 && bytes < 512) {
				nextlatency = eitr_latency_lowest;
			}
			break;
		case eitr_latency_bulk: /* 4k ints/s */
			if (bytes > 25000) {
				if (packets > 35)
					nextlatency = eitr_latency_low;
			} else if (bytes < 1500)
				nextlatency = eitr_latency_low;
			break;
		default:
			nextlatency = eitr_latency_low;
			device_printf(sc->dev, "Unexpected neweitr transition %d\n",
			    nextlatency);
			break;
		}

		/* Trim itr_latency_lowest for default AIM setting */
		if (sc->enable_aim == 1 && nextlatency == eitr_latency_lowest)
			nextlatency = eitr_latency_low;

		/* Request new latency */
		rxr->rx_nextlatency = nextlatency;
	} else {
		/* We may have toggled to AIM disabled */
		nextlatency = eitr_latency_disabled;
		rxr->rx_nextlatency = nextlatency;
	}

	/* ITR state machine */
	switch(nextlatency) {
	case eitr_latency_lowest:
		neweitr = IGC_INTS_70K;
		break;
	case eitr_latency_low:
		neweitr = IGC_INTS_20K;
		break;
	case eitr_latency_bulk:
		neweitr = IGC_INTS_4K;
		break;
	case eitr_latency_disabled:
	default:
		neweitr = igc_max_interrupt_rate;
		break;
	}

igc_set_next_eitr:
	neweitr = IGC_INTS_TO_EITR(neweitr);

	neweitr |= IGC_EITR_CNT_IGNR;

	if (neweitr != que->eitr_setting) {
		que->eitr_setting = neweitr;
		IGC_WRITE_REG(hw, IGC_EITR(que->msix), que->eitr_setting);
	}
}

/*********************************************************************
 *
 *  Fast Legacy/MSI Combined Interrupt Service routine
 *
 *********************************************************************/
int
igc_intr(void *arg)
{
	struct igc_softc *sc = arg;
	struct igc_hw *hw = &sc->hw;
	struct igc_rx_queue *que = &sc->rx_queues[0];
	struct tx_ring *txr = &sc->tx_queues[0].txr;
	struct rx_ring *rxr = &que->rxr;
	if_ctx_t ctx = sc->ctx;
	u32 reg_icr;

	reg_icr = IGC_READ_REG(hw, IGC_ICR);

	/* Hot eject? */
	if (reg_icr == 0xffffffff)
		return FILTER_STRAY;

	/* Definitely not our interrupt. */
	if (reg_icr == 0x0)
		return FILTER_STRAY;

	if ((reg_icr & IGC_ICR_INT_ASSERTED) == 0)
		return FILTER_STRAY;

	/*
	 * Only MSI-X interrupts have one-shot behavior by taking advantage
	 * of the EIAC register.  Thus, explicitly disable interrupts.  This
	 * also works around the MSI message reordering errata on certain
	 * systems.
	 */
	IFDI_INTR_DISABLE(ctx);

	/* Link status change */
	if (reg_icr & (IGC_ICR_RXSEQ | IGC_ICR_LSC))
		igc_handle_link(ctx);

	if (reg_icr & IGC_ICR_RXO)
		sc->rx_overruns++;

	igc_neweitr(sc, que, txr, rxr);

	/* Reset state */
	txr->tx_bytes = 0;
	txr->tx_packets = 0;
	rxr->rx_bytes = 0;
	rxr->rx_packets = 0;

	return (FILTER_SCHEDULE_THREAD);
}

static int
igc_if_rx_queue_intr_enable(if_ctx_t ctx, uint16_t rxqid)
{
	struct igc_softc *sc = iflib_get_softc(ctx);
	struct igc_rx_queue *rxq = &sc->rx_queues[rxqid];

	IGC_WRITE_REG(&sc->hw, IGC_EIMS, rxq->eims);
	return (0);
}

static int
igc_if_tx_queue_intr_enable(if_ctx_t ctx, uint16_t txqid)
{
	struct igc_softc *sc = iflib_get_softc(ctx);
	struct igc_tx_queue *txq = &sc->tx_queues[txqid];

	IGC_WRITE_REG(&sc->hw, IGC_EIMS, txq->eims);
	return (0);
}

/*********************************************************************
 *
 *  MSI-X RX Interrupt Service routine
 *
 **********************************************************************/
static int
igc_msix_que(void *arg)
{
	struct igc_rx_queue *que = arg;
	struct igc_softc *sc = que->sc;
	struct tx_ring *txr = &sc->tx_queues[que->msix].txr;
	struct rx_ring *rxr = &que->rxr;

	++que->irqs;

	igc_neweitr(sc, que, txr, rxr);

	/* Reset state */
	txr->tx_bytes = 0;
	txr->tx_packets = 0;
	rxr->rx_bytes = 0;
	rxr->rx_packets = 0;

	return (FILTER_SCHEDULE_THREAD);
}

/*********************************************************************
 *
 *  MSI-X Link Fast Interrupt Service routine
 *
 **********************************************************************/
static int
igc_msix_link(void *arg)
{
	struct igc_softc *sc = arg;
	u32 reg_icr;

	++sc->link_irq;
	MPASS(sc->hw.back != NULL);
	reg_icr = IGC_READ_REG(&sc->hw, IGC_ICR);

	if (reg_icr & IGC_ICR_RXO)
		sc->rx_overruns++;

	if (reg_icr & (IGC_ICR_RXSEQ | IGC_ICR_LSC)) {
		igc_handle_link(sc->ctx);
	}

	IGC_WRITE_REG(&sc->hw, IGC_IMS, IGC_IMS_LSC);
	IGC_WRITE_REG(&sc->hw, IGC_EIMS, sc->link_mask);

	return (FILTER_HANDLED);
}

static void
igc_handle_link(void *context)
{
	if_ctx_t ctx = context;
	struct igc_softc *sc = iflib_get_softc(ctx);

	sc->hw.mac.get_link_status = true;
	iflib_admin_intr_deferred(ctx);
}

/*********************************************************************
 *
 *  Media Ioctl callback
 *
 *  This routine is called whenever the user queries the status of
 *  the interface using ifconfig.
 *
 **********************************************************************/
static void
igc_if_media_status(if_ctx_t ctx, struct ifmediareq *ifmr)
{
	struct igc_softc *sc = iflib_get_softc(ctx);

	INIT_DEBUGOUT("igc_if_media_status: begin");

	iflib_admin_intr_deferred(ctx);

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	if (!sc->link_active) {
		return;
	}

	ifmr->ifm_status |= IFM_ACTIVE;

	switch (sc->link_speed) {
	case 10:
		ifmr->ifm_active |= IFM_10_T;
		break;
	case 100:
		ifmr->ifm_active |= IFM_100_TX;
                break;
	case 1000:
		ifmr->ifm_active |= IFM_1000_T;
		break;
	case 2500:
                ifmr->ifm_active |= IFM_2500_T;
                break;
	}

	if (sc->link_duplex == FULL_DUPLEX)
		ifmr->ifm_active |= IFM_FDX;
	else
		ifmr->ifm_active |= IFM_HDX;
}

/*********************************************************************
 *
 *  Media Ioctl callback
 *
 *  This routine is called when the user changes speed/duplex using
 *  media/mediopt option with ifconfig.
 *
 **********************************************************************/
static int
igc_if_media_change(if_ctx_t ctx)
{
	struct igc_softc *sc = iflib_get_softc(ctx);
	struct ifmedia *ifm = iflib_get_media(ctx);

	INIT_DEBUGOUT("igc_if_media_change: begin");

	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
		return (EINVAL);

	sc->hw.mac.autoneg = DO_AUTO_NEG;

	switch (IFM_SUBTYPE(ifm->ifm_media)) {
	case IFM_AUTO:
		sc->hw.phy.autoneg_advertised = AUTONEG_ADV_DEFAULT;
		break;
        case IFM_2500_T:
                sc->hw.phy.autoneg_advertised = ADVERTISE_2500_FULL;
                break;
	case IFM_1000_T:
		sc->hw.phy.autoneg_advertised = ADVERTISE_1000_FULL;
		break;
	case IFM_100_TX:
		if ((ifm->ifm_media & IFM_GMASK) == IFM_FDX)
			sc->hw.phy.autoneg_advertised = ADVERTISE_100_FULL;
		else
			sc->hw.phy.autoneg_advertised = ADVERTISE_100_HALF;
		break;
	case IFM_10_T:
		if ((ifm->ifm_media & IFM_GMASK) == IFM_FDX)
			sc->hw.phy.autoneg_advertised = ADVERTISE_10_FULL;
		else
			sc->hw.phy.autoneg_advertised = ADVERTISE_10_HALF;
		break;
	default:
		device_printf(sc->dev, "Unsupported media type\n");
	}

	igc_if_init(ctx);

	return (0);
}

static int
igc_if_set_promisc(if_ctx_t ctx, int flags)
{
	struct igc_softc *sc = iflib_get_softc(ctx);
	if_t ifp = iflib_get_ifp(ctx);
	u32 reg_rctl;
	int mcnt = 0;

	reg_rctl = IGC_READ_REG(&sc->hw, IGC_RCTL);
	reg_rctl &= ~(IGC_RCTL_SBP | IGC_RCTL_UPE);
	if (flags & IFF_ALLMULTI)
		mcnt = MAX_NUM_MULTICAST_ADDRESSES;
	else
		mcnt = min(if_llmaddr_count(ifp), MAX_NUM_MULTICAST_ADDRESSES);

	/* Don't disable if in MAX groups */
	if (mcnt < MAX_NUM_MULTICAST_ADDRESSES)
		reg_rctl &=  (~IGC_RCTL_MPE);
	IGC_WRITE_REG(&sc->hw, IGC_RCTL, reg_rctl);

	if (flags & IFF_PROMISC) {
		reg_rctl |= (IGC_RCTL_UPE | IGC_RCTL_MPE);
		/* Turn this on if you want to see bad packets */
		if (igc_debug_sbp)
			reg_rctl |= IGC_RCTL_SBP;
		IGC_WRITE_REG(&sc->hw, IGC_RCTL, reg_rctl);
	} else if (flags & IFF_ALLMULTI) {
		reg_rctl |= IGC_RCTL_MPE;
		reg_rctl &= ~IGC_RCTL_UPE;
		IGC_WRITE_REG(&sc->hw, IGC_RCTL, reg_rctl);
	}
	return (0);
}

static u_int
igc_copy_maddr(void *arg, struct sockaddr_dl *sdl, u_int idx)
{
	u8 *mta = arg;

	if (idx == MAX_NUM_MULTICAST_ADDRESSES)
		return (0);

	bcopy(LLADDR(sdl), &mta[idx * ETHER_ADDR_LEN], ETHER_ADDR_LEN);

	return (1);
}

/*********************************************************************
 *  Multicast Update
 *
 *  This routine is called whenever multicast address list is updated.
 *
 **********************************************************************/

static void
igc_if_multi_set(if_ctx_t ctx)
{
	struct igc_softc *sc = iflib_get_softc(ctx);
	if_t ifp = iflib_get_ifp(ctx);
	u8  *mta; /* Multicast array memory */
	u32 reg_rctl = 0;
	int mcnt = 0;

	IOCTL_DEBUGOUT("igc_set_multi: begin");

	mta = sc->mta;
	bzero(mta, sizeof(u8) * ETHER_ADDR_LEN * MAX_NUM_MULTICAST_ADDRESSES);

	mcnt = if_foreach_llmaddr(ifp, igc_copy_maddr, mta);

	reg_rctl = IGC_READ_REG(&sc->hw, IGC_RCTL);

	if (if_getflags(ifp) & IFF_PROMISC) {
		reg_rctl |= (IGC_RCTL_UPE | IGC_RCTL_MPE);
		/* Turn this on if you want to see bad packets */
		if (igc_debug_sbp)
			reg_rctl |= IGC_RCTL_SBP;
	} else if (mcnt >= MAX_NUM_MULTICAST_ADDRESSES ||
	      if_getflags(ifp) & IFF_ALLMULTI) {
                reg_rctl |= IGC_RCTL_MPE;
		reg_rctl &= ~IGC_RCTL_UPE;
        } else
		reg_rctl &= ~(IGC_RCTL_UPE | IGC_RCTL_MPE);

	if (mcnt < MAX_NUM_MULTICAST_ADDRESSES)
		igc_update_mc_addr_list(&sc->hw, mta, mcnt);

	IGC_WRITE_REG(&sc->hw, IGC_RCTL, reg_rctl);
}

/*********************************************************************
 *  Timer routine
 *
 *  This routine schedules igc_if_update_admin_status() to check for
 *  link status and to gather statistics as well as to perform some
 *  controller-specific hardware patting.
 *
 **********************************************************************/
static void
igc_if_timer(if_ctx_t ctx, uint16_t qid)
{

	if (qid != 0)
		return;

	iflib_admin_intr_deferred(ctx);
}

static void
igc_if_update_admin_status(if_ctx_t ctx)
{
	struct igc_softc *sc = iflib_get_softc(ctx);
	struct igc_hw *hw = &sc->hw;
	device_t dev = iflib_get_dev(ctx);
	u32 link_check, thstat, ctrl;

	link_check = thstat = ctrl = 0;
	/* Get the cached link value or read phy for real */
	switch (hw->phy.media_type) {
	case igc_media_type_copper:
		if (hw->mac.get_link_status == true) {
			/* Do the work to read phy */
			igc_check_for_link(hw);
			link_check = !hw->mac.get_link_status;
		} else
			link_check = true;
		break;
	case igc_media_type_unknown:
		igc_check_for_link(hw);
		link_check = !hw->mac.get_link_status;
		/* FALLTHROUGH */
	default:
		break;
	}

	/* Now check for a transition */
	if (link_check && (sc->link_active == 0)) {
		igc_get_speed_and_duplex(hw, &sc->link_speed,
		    &sc->link_duplex);
		if (bootverbose)
			device_printf(dev, "Link is up %d Mbps %s\n",
			    sc->link_speed,
			    ((sc->link_duplex == FULL_DUPLEX) ?
			    "Full Duplex" : "Half Duplex"));
		sc->link_active = 1;
		iflib_link_state_change(ctx, LINK_STATE_UP,
		    IF_Mbps(sc->link_speed));
	} else if (!link_check && (sc->link_active == 1)) {
		sc->link_speed = 0;
		sc->link_duplex = 0;
		sc->link_active = 0;
		iflib_link_state_change(ctx, LINK_STATE_DOWN, 0);
	}
	igc_update_stats_counters(sc);
}

static void
igc_if_watchdog_reset(if_ctx_t ctx)
{
	struct igc_softc *sc = iflib_get_softc(ctx);

	/*
	 * Just count the event; iflib(4) will already trigger a
	 * sufficient reset of the controller.
	 */
	sc->watchdog_events++;
}

/*********************************************************************
 *
 *  This routine disables all traffic on the adapter by issuing a
 *  global reset on the MAC.
 *
 **********************************************************************/
static void
igc_if_stop(if_ctx_t ctx)
{
	struct igc_softc *sc = iflib_get_softc(ctx);

	INIT_DEBUGOUT("igc_if_stop: begin");

	igc_reset_hw(&sc->hw);
	IGC_WRITE_REG(&sc->hw, IGC_WUC, 0);
}

/*********************************************************************
 *
 *  Determine hardware revision.
 *
 **********************************************************************/
static void
igc_identify_hardware(if_ctx_t ctx)
{
	device_t dev = iflib_get_dev(ctx);
	struct igc_softc *sc = iflib_get_softc(ctx);

	/* Make sure our PCI config space has the necessary stuff set */
	sc->hw.bus.pci_cmd_word = pci_read_config(dev, PCIR_COMMAND, 2);

	/* Save off the information about this board */
	sc->hw.vendor_id = pci_get_vendor(dev);
	sc->hw.device_id = pci_get_device(dev);
	sc->hw.revision_id = pci_read_config(dev, PCIR_REVID, 1);
	sc->hw.subsystem_vendor_id =
	    pci_read_config(dev, PCIR_SUBVEND_0, 2);
	sc->hw.subsystem_device_id =
	    pci_read_config(dev, PCIR_SUBDEV_0, 2);

	/* Do Shared Code Init and Setup */
	if (igc_set_mac_type(&sc->hw)) {
		device_printf(dev, "Setup init failure\n");
		return;
	}
}

static int
igc_allocate_pci_resources(if_ctx_t ctx)
{
	struct igc_softc *sc = iflib_get_softc(ctx);
	device_t dev = iflib_get_dev(ctx);
	int rid;

	rid = PCIR_BAR(0);
	sc->memory = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &rid, RF_ACTIVE);
	if (sc->memory == NULL) {
		device_printf(dev, "Unable to allocate bus resource: memory\n");
		return (ENXIO);
	}
	sc->osdep.mem_bus_space_tag = rman_get_bustag(sc->memory);
	sc->osdep.mem_bus_space_handle =
	    rman_get_bushandle(sc->memory);
	sc->hw.hw_addr = (u8 *)&sc->osdep.mem_bus_space_handle;

	sc->hw.back = &sc->osdep;

	return (0);
}

/*********************************************************************
 *
 *  Set up the MSI-X Interrupt handlers
 *
 **********************************************************************/
static int
igc_if_msix_intr_assign(if_ctx_t ctx, int msix)
{
	struct igc_softc *sc = iflib_get_softc(ctx);
	struct igc_rx_queue *rx_que = sc->rx_queues;
	struct igc_tx_queue *tx_que = sc->tx_queues;
	int error, rid, i, vector = 0, rx_vectors;
	char buf[16];

	/* First set up ring resources */
	for (i = 0; i < sc->rx_num_queues; i++, rx_que++, vector++) {
		rid = vector + 1;
		snprintf(buf, sizeof(buf), "rxq%d", i);
		error = iflib_irq_alloc_generic(ctx, &rx_que->que_irq, rid, IFLIB_INTR_RXTX, igc_msix_que, rx_que, rx_que->me, buf);
		if (error) {
			device_printf(iflib_get_dev(ctx), "Failed to allocate que int %d err: %d", i, error);
			sc->rx_num_queues = i + 1;
			goto fail;
		}

		rx_que->msix =  vector;

		/*
		 * Set the bit to enable interrupt
		 * in IGC_IMS -- bits 20 and 21
		 * are for RX0 and RX1, note this has
		 * NOTHING to do with the MSI-X vector
		 */
		rx_que->eims = 1 << vector;
	}
	rx_vectors = vector;

	vector = 0;
	for (i = 0; i < sc->tx_num_queues; i++, tx_que++, vector++) {
		snprintf(buf, sizeof(buf), "txq%d", i);
		tx_que = &sc->tx_queues[i];
		iflib_softirq_alloc_generic(ctx,
		    &sc->rx_queues[i % sc->rx_num_queues].que_irq,
		    IFLIB_INTR_TX, tx_que, tx_que->me, buf);

		tx_que->msix = (vector % sc->rx_num_queues);

		/*
		 * Set the bit to enable interrupt
		 * in IGC_IMS -- bits 22 and 23
		 * are for TX0 and TX1, note this has
		 * NOTHING to do with the MSI-X vector
		 */
		tx_que->eims = 1 << i;
	}

	/* Link interrupt */
	rid = rx_vectors + 1;
	error = iflib_irq_alloc_generic(ctx, &sc->irq, rid, IFLIB_INTR_ADMIN, igc_msix_link, sc, 0, "aq");

	if (error) {
		device_printf(iflib_get_dev(ctx), "Failed to register admin handler");
		goto fail;
	}
	sc->linkvec = rx_vectors;
	return (0);
fail:
	iflib_irq_free(ctx, &sc->irq);
	rx_que = sc->rx_queues;
	for (int i = 0; i < sc->rx_num_queues; i++, rx_que++)
		iflib_irq_free(ctx, &rx_que->que_irq);
	return (error);
}

static void
igc_configure_queues(struct igc_softc *sc)
{
	struct igc_hw *hw = &sc->hw;
	struct igc_rx_queue *rx_que;
	struct igc_tx_queue *tx_que;
	u32 ivar = 0, newitr = 0;

	/* First turn on RSS capability */
	IGC_WRITE_REG(hw, IGC_GPIE,
	    IGC_GPIE_MSIX_MODE | IGC_GPIE_EIAME | IGC_GPIE_PBA |
	    IGC_GPIE_NSICR);

	/* Turn on MSI-X */
	/* RX entries */
	for (int i = 0; i < sc->rx_num_queues; i++) {
		u32 index = i >> 1;
		ivar = IGC_READ_REG_ARRAY(hw, IGC_IVAR0, index);
		rx_que = &sc->rx_queues[i];
		if (i & 1) {
			ivar &= 0xFF00FFFF;
			ivar |= (rx_que->msix | IGC_IVAR_VALID) << 16;
		} else {
			ivar &= 0xFFFFFF00;
			ivar |= rx_que->msix | IGC_IVAR_VALID;
		}
		IGC_WRITE_REG_ARRAY(hw, IGC_IVAR0, index, ivar);
	}
	/* TX entries */
	for (int i = 0; i < sc->tx_num_queues; i++) {
		u32 index = i >> 1;
		ivar = IGC_READ_REG_ARRAY(hw, IGC_IVAR0, index);
		tx_que = &sc->tx_queues[i];
		if (i & 1) {
			ivar &= 0x00FFFFFF;
			ivar |= (tx_que->msix | IGC_IVAR_VALID) << 24;
		} else {
			ivar &= 0xFFFF00FF;
			ivar |= (tx_que->msix | IGC_IVAR_VALID) << 8;
		}
		IGC_WRITE_REG_ARRAY(hw, IGC_IVAR0, index, ivar);
		sc->que_mask |= tx_que->eims;
	}

	/* And for the link interrupt */
	ivar = (sc->linkvec | IGC_IVAR_VALID) << 8;
	sc->link_mask = 1 << sc->linkvec;
	IGC_WRITE_REG(hw, IGC_IVAR_MISC, ivar);

	/* Set the starting interrupt rate */
	if (igc_max_interrupt_rate > 0)
		newitr = IGC_INTS_TO_EITR(igc_max_interrupt_rate);

	newitr |= IGC_EITR_CNT_IGNR;

	for (int i = 0; i < sc->rx_num_queues; i++) {
		rx_que = &sc->rx_queues[i];
		IGC_WRITE_REG(hw, IGC_EITR(rx_que->msix), newitr);
	}

	return;
}

static void
igc_free_pci_resources(if_ctx_t ctx)
{
	struct igc_softc *sc = iflib_get_softc(ctx);
	struct igc_rx_queue *que = sc->rx_queues;
	device_t dev = iflib_get_dev(ctx);

	/* Release all MSI-X queue resources */
	if (sc->intr_type == IFLIB_INTR_MSIX)
		iflib_irq_free(ctx, &sc->irq);

	for (int i = 0; i < sc->rx_num_queues; i++, que++) {
		iflib_irq_free(ctx, &que->que_irq);
	}

	if (sc->memory != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY,
		    rman_get_rid(sc->memory), sc->memory);
		sc->memory = NULL;
	}

	if (sc->flash != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY,
		    rman_get_rid(sc->flash), sc->flash);
		sc->flash = NULL;
	}

	if (sc->ioport != NULL) {
		bus_release_resource(dev, SYS_RES_IOPORT,
		    rman_get_rid(sc->ioport), sc->ioport);
		sc->ioport = NULL;
	}
}

/* Set up MSI or MSI-X */
static int
igc_setup_msix(if_ctx_t ctx)
{
	return (0);
}

/*********************************************************************
 *
 *  Initialize the DMA Coalescing feature
 *
 **********************************************************************/
static void
igc_init_dmac(struct igc_softc *sc, u32 pba)
{
	device_t	dev = sc->dev;
	struct igc_hw *hw = &sc->hw;
	u32 		dmac, reg = ~IGC_DMACR_DMAC_EN;
	u16		hwm;
	u16		max_frame_size;
	int		status;

	max_frame_size = sc->shared->isc_max_frame_size;

	if (sc->dmac == 0) { /* Disabling it */
		IGC_WRITE_REG(hw, IGC_DMACR, reg);
		return;
	} else
		device_printf(dev, "DMA Coalescing enabled\n");

	/* Set starting threshold */
	IGC_WRITE_REG(hw, IGC_DMCTXTH, 0);

	hwm = 64 * pba - max_frame_size / 16;
	if (hwm < 64 * (pba - 6))
		hwm = 64 * (pba - 6);
	reg = IGC_READ_REG(hw, IGC_FCRTC);
	reg &= ~IGC_FCRTC_RTH_COAL_MASK;
	reg |= ((hwm << IGC_FCRTC_RTH_COAL_SHIFT)
		& IGC_FCRTC_RTH_COAL_MASK);
	IGC_WRITE_REG(hw, IGC_FCRTC, reg);

	dmac = pba - max_frame_size / 512;
	if (dmac < pba - 10)
		dmac = pba - 10;
	reg = IGC_READ_REG(hw, IGC_DMACR);
	reg &= ~IGC_DMACR_DMACTHR_MASK;
	reg |= ((dmac << IGC_DMACR_DMACTHR_SHIFT)
		& IGC_DMACR_DMACTHR_MASK);

	/* transition to L0x or L1 if available..*/
	reg |= (IGC_DMACR_DMAC_EN | IGC_DMACR_DMAC_LX_MASK);

	/* Check if status is 2.5Gb backplane connection
	 * before configuration of watchdog timer, which is
	 * in msec values in 12.8usec intervals
	 * watchdog timer= msec values in 32usec intervals
	 * for non 2.5Gb connection
	 */
	status = IGC_READ_REG(hw, IGC_STATUS);
	if ((status & IGC_STATUS_2P5_SKU) &&
	    (!(status & IGC_STATUS_2P5_SKU_OVER)))
		reg |= ((sc->dmac * 5) >> 6);
	else
		reg |= (sc->dmac >> 5);

	IGC_WRITE_REG(hw, IGC_DMACR, reg);

	IGC_WRITE_REG(hw, IGC_DMCRTRH, 0);

	/* Set the interval before transition */
	reg = IGC_READ_REG(hw, IGC_DMCTLX);
	reg |= IGC_DMCTLX_DCFLUSH_DIS;

	/*
	** in 2.5Gb connection, TTLX unit is 0.4 usec
	** which is 0x4*2 = 0xA. But delay is still 4 usec
	*/
	status = IGC_READ_REG(hw, IGC_STATUS);
	if ((status & IGC_STATUS_2P5_SKU) &&
	    (!(status & IGC_STATUS_2P5_SKU_OVER)))
		reg |= 0xA;
	else
		reg |= 0x4;

	IGC_WRITE_REG(hw, IGC_DMCTLX, reg);

	/* free space in tx packet buffer to wake from DMA coal */
	IGC_WRITE_REG(hw, IGC_DMCTXTH, (IGC_TXPBSIZE -
	    (2 * max_frame_size)) >> 6);

	/* make low power state decision controlled by DMA coal */
	reg = IGC_READ_REG(hw, IGC_PCIEMISC);
	reg &= ~IGC_PCIEMISC_LX_DECISION;
	IGC_WRITE_REG(hw, IGC_PCIEMISC, reg);
}

/*********************************************************************
 *
 *  Initialize the hardware to a configuration as specified by the
 *  softc structure.
 *
 **********************************************************************/
static void
igc_reset(if_ctx_t ctx)
{
	device_t dev = iflib_get_dev(ctx);
	struct igc_softc *sc = iflib_get_softc(ctx);
	struct igc_hw *hw = &sc->hw;
	u32 rx_buffer_size;
	u32 pba;

	INIT_DEBUGOUT("igc_reset: begin");
	/* Let the firmware know the OS is in control */
	igc_get_hw_control(sc);

	/*
	 * Packet Buffer Allocation (PBA)
	 * Writing PBA sets the receive portion of the buffer
	 * the remainder is used for the transmit buffer.
	 */
	pba = IGC_PBA_34K;

	INIT_DEBUGOUT1("igc_reset: pba=%dK",pba);

	/*
	 * These parameters control the automatic generation (Tx) and
	 * response (Rx) to Ethernet PAUSE frames.
	 * - High water mark should allow for at least two frames to be
	 *   received after sending an XOFF.
	 * - Low water mark works best when it is very near the high water mark.
	 *   This allows the receiver to restart by sending XON when it has
	 *   drained a bit. Here we use an arbitrary value of 1500 which will
	 *   restart after one full frame is pulled from the buffer. There
	 *   could be several smaller frames in the buffer and if so they will
	 *   not trigger the XON until their total number reduces the buffer
	 *   by 1500.
	 * - The pause time is fairly large at 1000 x 512ns = 512 usec.
	 */
	rx_buffer_size = (pba & 0xffff) << 10;
	hw->fc.high_water = rx_buffer_size -
	    roundup2(sc->hw.mac.max_frame_size, 1024);
	/* 16-byte granularity */
	hw->fc.low_water = hw->fc.high_water - 16;

	if (sc->fc) /* locally set flow control value? */
		hw->fc.requested_mode = sc->fc;
	else
		hw->fc.requested_mode = igc_fc_full;

	hw->fc.pause_time = IGC_FC_PAUSE_TIME;

	hw->fc.send_xon = true;

	/* Issue a global reset */
	igc_reset_hw(hw);
	IGC_WRITE_REG(hw, IGC_WUC, 0);

	/* and a re-init */
	if (igc_init_hw(hw) < 0) {
		device_printf(dev, "Hardware Initialization Failed\n");
		return;
	}

	/* Setup DMA Coalescing */
	igc_init_dmac(sc, pba);

	/* Save the final PBA off if it needs to be used elsewhere i.e. AIM */
	sc->pba = pba;

	IGC_WRITE_REG(hw, IGC_VET, ETHERTYPE_VLAN);
	igc_get_phy_info(hw);
	igc_check_for_link(hw);
}

/*
 * Initialise the RSS mapping for NICs that support multiple transmit/
 * receive rings.
 */

#define RSSKEYLEN 10
static void
igc_initialize_rss_mapping(struct igc_softc *sc)
{
	struct igc_hw *hw = &sc->hw;
	int i;
	int queue_id;
	u32 reta;
	u32 rss_key[RSSKEYLEN], mrqc, shift = 0;

	/*
	 * The redirection table controls which destination
	 * queue each bucket redirects traffic to.
	 * Each DWORD represents four queues, with the LSB
	 * being the first queue in the DWORD.
	 *
	 * This just allocates buckets to queues using round-robin
	 * allocation.
	 *
	 * NOTE: It Just Happens to line up with the default
	 * RSS allocation method.
	 */

	/* Warning FM follows */
	reta = 0;
	for (i = 0; i < 128; i++) {
#ifdef RSS
		queue_id = rss_get_indirection_to_bucket(i);
		/*
		 * If we have more queues than buckets, we'll
		 * end up mapping buckets to a subset of the
		 * queues.
		 *
		 * If we have more buckets than queues, we'll
		 * end up instead assigning multiple buckets
		 * to queues.
		 *
		 * Both are suboptimal, but we need to handle
		 * the case so we don't go out of bounds
		 * indexing arrays and such.
		 */
		queue_id = queue_id % sc->rx_num_queues;
#else
		queue_id = (i % sc->rx_num_queues);
#endif
		/* Adjust if required */
		queue_id = queue_id << shift;

		/*
		 * The low 8 bits are for hash value (n+0);
		 * The next 8 bits are for hash value (n+1), etc.
		 */
		reta = reta >> 8;
		reta = reta | ( ((uint32_t) queue_id) << 24);
		if ((i & 3) == 3) {
			IGC_WRITE_REG(hw, IGC_RETA(i >> 2), reta);
			reta = 0;
		}
	}

	/* Now fill in hash table */

	/*
	 * MRQC: Multiple Receive Queues Command
	 * Set queuing to RSS control, number depends on the device.
	 */
	mrqc = IGC_MRQC_ENABLE_RSS_4Q;

#ifdef RSS
	/* XXX ew typecasting */
	rss_getkey((uint8_t *) &rss_key);
#else
	arc4rand(&rss_key, sizeof(rss_key), 0);
#endif
	for (i = 0; i < RSSKEYLEN; i++)
		IGC_WRITE_REG_ARRAY(hw, IGC_RSSRK(0), i, rss_key[i]);

	/*
	 * Configure the RSS fields to hash upon.
	 */
	mrqc |= (IGC_MRQC_RSS_FIELD_IPV4 |
	    IGC_MRQC_RSS_FIELD_IPV4_TCP);
	mrqc |= (IGC_MRQC_RSS_FIELD_IPV6 |
	    IGC_MRQC_RSS_FIELD_IPV6_TCP);
	mrqc |=( IGC_MRQC_RSS_FIELD_IPV4_UDP |
	    IGC_MRQC_RSS_FIELD_IPV6_UDP);
	mrqc |=( IGC_MRQC_RSS_FIELD_IPV6_UDP_EX |
	    IGC_MRQC_RSS_FIELD_IPV6_TCP_EX);

	IGC_WRITE_REG(hw, IGC_MRQC, mrqc);
}

/*********************************************************************
 *
 *  Setup networking device structure and register interface media.
 *
 **********************************************************************/
static int
igc_setup_interface(if_ctx_t ctx)
{
	if_t ifp = iflib_get_ifp(ctx);
	struct igc_softc *sc = iflib_get_softc(ctx);
	if_softc_ctx_t scctx = sc->shared;

	INIT_DEBUGOUT("igc_setup_interface: begin");

	/* Single Queue */
	if (sc->tx_num_queues == 1) {
		if_setsendqlen(ifp, scctx->isc_ntxd[0] - 1);
		if_setsendqready(ifp);
	}

	/*
	 * Specify the media types supported by this adapter and register
	 * callbacks to update media and link information
	 */
	ifmedia_add(sc->media, IFM_ETHER | IFM_10_T, 0, NULL);
	ifmedia_add(sc->media, IFM_ETHER | IFM_10_T | IFM_FDX, 0, NULL);
	ifmedia_add(sc->media, IFM_ETHER | IFM_100_TX, 0, NULL);
	ifmedia_add(sc->media, IFM_ETHER | IFM_100_TX | IFM_FDX, 0, NULL);
	ifmedia_add(sc->media, IFM_ETHER | IFM_1000_T | IFM_FDX, 0, NULL);
	ifmedia_add(sc->media, IFM_ETHER | IFM_1000_T, 0, NULL);
	ifmedia_add(sc->media, IFM_ETHER | IFM_2500_T, 0, NULL);

	ifmedia_add(sc->media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(sc->media, IFM_ETHER | IFM_AUTO);
	return (0);
}

static int
igc_if_tx_queues_alloc(if_ctx_t ctx, caddr_t *vaddrs, uint64_t *paddrs, int ntxqs, int ntxqsets)
{
	struct igc_softc *sc = iflib_get_softc(ctx);
	if_softc_ctx_t scctx = sc->shared;
	int error = IGC_SUCCESS;
	struct igc_tx_queue *que;
	int i, j;

	MPASS(sc->tx_num_queues > 0);
	MPASS(sc->tx_num_queues == ntxqsets);

	/* First allocate the top level queue structs */
	if (!(sc->tx_queues =
	    (struct igc_tx_queue *) malloc(sizeof(struct igc_tx_queue) *
	    sc->tx_num_queues, M_DEVBUF, M_NOWAIT | M_ZERO))) {
		device_printf(iflib_get_dev(ctx), "Unable to allocate queue memory\n");
		return(ENOMEM);
	}

	for (i = 0, que = sc->tx_queues; i < sc->tx_num_queues; i++, que++) {
		/* Set up some basics */

		struct tx_ring *txr = &que->txr;
		txr->sc = que->sc = sc;
		que->me = txr->me =  i;

		/* Allocate report status array */
		if (!(txr->tx_rsq = (qidx_t *) malloc(sizeof(qidx_t) * scctx->isc_ntxd[0], M_DEVBUF, M_NOWAIT | M_ZERO))) {
			device_printf(iflib_get_dev(ctx), "failed to allocate rs_idxs memory\n");
			error = ENOMEM;
			goto fail;
		}
		for (j = 0; j < scctx->isc_ntxd[0]; j++)
			txr->tx_rsq[j] = QIDX_INVALID;
		/* get the virtual and physical address of the hardware queues */
		txr->tx_base = (struct igc_tx_desc *)vaddrs[i*ntxqs];
		txr->tx_paddr = paddrs[i*ntxqs];
	}

	if (bootverbose)
		device_printf(iflib_get_dev(ctx),
		    "allocated for %d tx_queues\n", sc->tx_num_queues);
	return (0);
fail:
	igc_if_queues_free(ctx);
	return (error);
}

static int
igc_if_rx_queues_alloc(if_ctx_t ctx, caddr_t *vaddrs, uint64_t *paddrs, int nrxqs, int nrxqsets)
{
	struct igc_softc *sc = iflib_get_softc(ctx);
	int error = IGC_SUCCESS;
	struct igc_rx_queue *que;
	int i;

	MPASS(sc->rx_num_queues > 0);
	MPASS(sc->rx_num_queues == nrxqsets);

	/* First allocate the top level queue structs */
	if (!(sc->rx_queues =
	    (struct igc_rx_queue *) malloc(sizeof(struct igc_rx_queue) *
	    sc->rx_num_queues, M_DEVBUF, M_NOWAIT | M_ZERO))) {
		device_printf(iflib_get_dev(ctx), "Unable to allocate queue memory\n");
		error = ENOMEM;
		goto fail;
	}

	for (i = 0, que = sc->rx_queues; i < nrxqsets; i++, que++) {
		/* Set up some basics */
		struct rx_ring *rxr = &que->rxr;
		rxr->sc = que->sc = sc;
		rxr->que = que;
		que->me = rxr->me =  i;

		/* get the virtual and physical address of the hardware queues */
		rxr->rx_base = (union igc_rx_desc_extended *)vaddrs[i*nrxqs];
		rxr->rx_paddr = paddrs[i*nrxqs];
	}
 
	if (bootverbose)
		device_printf(iflib_get_dev(ctx),
		    "allocated for %d rx_queues\n", sc->rx_num_queues);

	return (0);
fail:
	igc_if_queues_free(ctx);
	return (error);
}

static void
igc_if_queues_free(if_ctx_t ctx)
{
	struct igc_softc *sc = iflib_get_softc(ctx);
	struct igc_tx_queue *tx_que = sc->tx_queues;
	struct igc_rx_queue *rx_que = sc->rx_queues;

	if (tx_que != NULL) {
		for (int i = 0; i < sc->tx_num_queues; i++, tx_que++) {
			struct tx_ring *txr = &tx_que->txr;
			if (txr->tx_rsq == NULL)
				break;

			free(txr->tx_rsq, M_DEVBUF);
			txr->tx_rsq = NULL;
		}
		free(sc->tx_queues, M_DEVBUF);
		sc->tx_queues = NULL;
	}

	if (rx_que != NULL) {
		free(sc->rx_queues, M_DEVBUF);
		sc->rx_queues = NULL;
	}

	igc_release_hw_control(sc);

	if (sc->mta != NULL) {
		free(sc->mta, M_DEVBUF);
	}
}

/*********************************************************************
 *
 *  Enable transmit unit.
 *
 **********************************************************************/
static void
igc_initialize_transmit_unit(if_ctx_t ctx)
{
	struct igc_softc *sc = iflib_get_softc(ctx);
	if_softc_ctx_t scctx = sc->shared;
	struct igc_tx_queue *que;
	struct tx_ring	*txr;
	struct igc_hw	*hw = &sc->hw;
	u32 tctl, txdctl = 0;

	INIT_DEBUGOUT("igc_initialize_transmit_unit: begin");

	for (int i = 0; i < sc->tx_num_queues; i++, txr++) {
		u64 bus_addr;
		caddr_t offp, endp;

		que = &sc->tx_queues[i];
		txr = &que->txr;
		bus_addr = txr->tx_paddr;

		/* Clear checksum offload context. */
		offp = (caddr_t)&txr->csum_flags;
		endp = (caddr_t)(txr + 1);
		bzero(offp, endp - offp);

		/* Base and Len of TX Ring */
		IGC_WRITE_REG(hw, IGC_TDLEN(i),
		    scctx->isc_ntxd[0] * sizeof(struct igc_tx_desc));
		IGC_WRITE_REG(hw, IGC_TDBAH(i),
		    (u32)(bus_addr >> 32));
		IGC_WRITE_REG(hw, IGC_TDBAL(i),
		    (u32)bus_addr);
		/* Init the HEAD/TAIL indices */
		IGC_WRITE_REG(hw, IGC_TDT(i), 0);
		IGC_WRITE_REG(hw, IGC_TDH(i), 0);

		HW_DEBUGOUT2("Base = %x, Length = %x\n",
		    IGC_READ_REG(&sc->hw, IGC_TDBAL(i)),
		    IGC_READ_REG(&sc->hw, IGC_TDLEN(i)));

		txdctl = 0; /* clear txdctl */
		txdctl |= 0x1f; /* PTHRESH */
		txdctl |= 1 << 8; /* HTHRESH */
		txdctl |= 1 << 16;/* WTHRESH */
		txdctl |= 1 << 22; /* Reserved bit 22 must always be 1 */
		txdctl |= IGC_TXDCTL_GRAN;
		txdctl |= 1 << 25; /* LWTHRESH */

		IGC_WRITE_REG(hw, IGC_TXDCTL(i), txdctl);
	}

	/* Program the Transmit Control Register */
	tctl = IGC_READ_REG(&sc->hw, IGC_TCTL);
	tctl &= ~IGC_TCTL_CT;
	tctl |= (IGC_TCTL_PSP | IGC_TCTL_RTLC | IGC_TCTL_EN |
		   (IGC_COLLISION_THRESHOLD << IGC_CT_SHIFT));

	/* This write will effectively turn on the transmit unit. */
	IGC_WRITE_REG(&sc->hw, IGC_TCTL, tctl);
}

/*********************************************************************
 *
 *  Enable receive unit.
 *
 **********************************************************************/
#define BSIZEPKT_ROUNDUP	((1<<IGC_SRRCTL_BSIZEPKT_SHIFT)-1)

static void
igc_initialize_receive_unit(if_ctx_t ctx)
{
	struct igc_softc *sc = iflib_get_softc(ctx);
	if_softc_ctx_t scctx = sc->shared;
	if_t ifp = iflib_get_ifp(ctx);
	struct igc_hw	*hw = &sc->hw;
	struct igc_rx_queue *que;
	int i;
	u32 psize, rctl, rxcsum, srrctl = 0;

	INIT_DEBUGOUT("igc_initialize_receive_units: begin");

	/*
	 * Make sure receives are disabled while setting
	 * up the descriptor ring
	 */
	rctl = IGC_READ_REG(hw, IGC_RCTL);
	IGC_WRITE_REG(hw, IGC_RCTL, rctl & ~IGC_RCTL_EN);

	/* Setup the Receive Control Register */
	rctl &= ~(3 << IGC_RCTL_MO_SHIFT);
	rctl |= IGC_RCTL_EN | IGC_RCTL_BAM |
	    IGC_RCTL_LBM_NO | IGC_RCTL_RDMTS_HALF |
	    (hw->mac.mc_filter_type << IGC_RCTL_MO_SHIFT);

	/* Do not store bad packets */
	rctl &= ~IGC_RCTL_SBP;

	/* Enable Long Packet receive */
	if (if_getmtu(ifp) > ETHERMTU)
		rctl |= IGC_RCTL_LPE;
	else
		rctl &= ~IGC_RCTL_LPE;

	/* Strip the CRC */
	if (!igc_disable_crc_stripping)
		rctl |= IGC_RCTL_SECRC;

	rxcsum = IGC_READ_REG(hw, IGC_RXCSUM);
	if (if_getcapenable(ifp) & IFCAP_RXCSUM) {
		rxcsum |= IGC_RXCSUM_CRCOFL;
		if (sc->tx_num_queues > 1)
			rxcsum |= IGC_RXCSUM_PCSD;
		else
			rxcsum |= IGC_RXCSUM_IPPCSE;
	} else {
		if (sc->tx_num_queues > 1)
			rxcsum |= IGC_RXCSUM_PCSD;
		else
			rxcsum &= ~IGC_RXCSUM_TUOFL;
	}
	IGC_WRITE_REG(hw, IGC_RXCSUM, rxcsum);

	if (sc->rx_num_queues > 1)
		igc_initialize_rss_mapping(sc);

	if (if_getmtu(ifp) > ETHERMTU) {
		psize = scctx->isc_max_frame_size;
		/* are we on a vlan? */
		if (if_vlantrunkinuse(ifp))
			psize += VLAN_TAG_SIZE;
		IGC_WRITE_REG(&sc->hw, IGC_RLPML, psize);
	}

	/* Set maximum packet buffer len */
	srrctl |= (sc->rx_mbuf_sz + BSIZEPKT_ROUNDUP) >>
	    IGC_SRRCTL_BSIZEPKT_SHIFT;
	/* srrctl above overrides this but set the register to a sane value */
	rctl |= IGC_RCTL_SZ_2048;

	/*
	 * If TX flow control is disabled and there's >1 queue defined,
	 * enable DROP.
	 *
	 * This drops frames rather than hanging the RX MAC for all queues.
	 */
	if ((sc->rx_num_queues > 1) &&
	    (sc->fc == igc_fc_none ||
	     sc->fc == igc_fc_rx_pause)) {
		srrctl |= IGC_SRRCTL_DROP_EN;
	}

	/* Setup the Base and Length of the Rx Descriptor Rings */
	for (i = 0, que = sc->rx_queues; i < sc->rx_num_queues; i++, que++) {
		struct rx_ring *rxr = &que->rxr;
		u64 bus_addr = rxr->rx_paddr;
		u32 rxdctl;

#ifdef notyet
		/* Configure for header split? -- ignore for now */
		rxr->hdr_split = igc_header_split;
#else
		srrctl |= IGC_SRRCTL_DESCTYPE_ADV_ONEBUF;
#endif

		IGC_WRITE_REG(hw, IGC_RDLEN(i),
			      scctx->isc_nrxd[0] * sizeof(struct igc_rx_desc));
		IGC_WRITE_REG(hw, IGC_RDBAH(i),
			      (uint32_t)(bus_addr >> 32));
		IGC_WRITE_REG(hw, IGC_RDBAL(i),
			      (uint32_t)bus_addr);
		IGC_WRITE_REG(hw, IGC_SRRCTL(i), srrctl);
		/* Setup the Head and Tail Descriptor Pointers */
		IGC_WRITE_REG(hw, IGC_RDH(i), 0);
		IGC_WRITE_REG(hw, IGC_RDT(i), 0);
		/* Enable this Queue */
		rxdctl = IGC_READ_REG(hw, IGC_RXDCTL(i));
		rxdctl |= IGC_RXDCTL_QUEUE_ENABLE;
		rxdctl &= 0xFFF00000;
		rxdctl |= IGC_RX_PTHRESH;
		rxdctl |= IGC_RX_HTHRESH << 8;
		rxdctl |= IGC_RX_WTHRESH << 16;
		IGC_WRITE_REG(hw, IGC_RXDCTL(i), rxdctl);
	}

	/* Make sure VLAN Filters are off */
	rctl &= ~IGC_RCTL_VFE;

	/* Write out the settings */
	IGC_WRITE_REG(hw, IGC_RCTL, rctl);

	return;
}

static void
igc_setup_vlan_hw_support(if_ctx_t ctx)
{
	struct igc_softc *sc = iflib_get_softc(ctx);
	struct igc_hw *hw = &sc->hw;
	struct ifnet *ifp = iflib_get_ifp(ctx);
	u32 reg;

	/* igc hardware doesn't seem to implement VFTA for HWFILTER */

	if (if_getcapenable(ifp) & IFCAP_VLAN_HWTAGGING &&
	    !igc_disable_crc_stripping) {
		reg = IGC_READ_REG(hw, IGC_CTRL);
		reg |= IGC_CTRL_VME;
		IGC_WRITE_REG(hw, IGC_CTRL, reg);
	} else {
		reg = IGC_READ_REG(hw, IGC_CTRL);
		reg &= ~IGC_CTRL_VME;
		IGC_WRITE_REG(hw, IGC_CTRL, reg);
	}
}

static void
igc_if_intr_enable(if_ctx_t ctx)
{
	struct igc_softc *sc = iflib_get_softc(ctx);
	struct igc_hw *hw = &sc->hw;
	u32 mask;

	if (__predict_true(sc->intr_type == IFLIB_INTR_MSIX)) {
		mask = (sc->que_mask | sc->link_mask);
		IGC_WRITE_REG(hw, IGC_EIAC, mask);
		IGC_WRITE_REG(hw, IGC_EIAM, mask);
		IGC_WRITE_REG(hw, IGC_EIMS, mask);
		IGC_WRITE_REG(hw, IGC_IMS, IGC_IMS_LSC);
	} else
		IGC_WRITE_REG(hw, IGC_IMS, IMS_ENABLE_MASK);
	IGC_WRITE_FLUSH(hw);
}

static void
igc_if_intr_disable(if_ctx_t ctx)
{
	struct igc_softc *sc = iflib_get_softc(ctx);
	struct igc_hw *hw = &sc->hw;

	if (__predict_true(sc->intr_type == IFLIB_INTR_MSIX)) {
		IGC_WRITE_REG(hw, IGC_EIMC, 0xffffffff);
		IGC_WRITE_REG(hw, IGC_EIAC, 0);
	}
	IGC_WRITE_REG(hw, IGC_IMC, 0xffffffff);
	IGC_WRITE_FLUSH(hw);
}

/*
 * igc_get_hw_control sets the {CTRL_EXT|FWSM}:DRV_LOAD bit.
 * For ASF and Pass Through versions of f/w this means
 * that the driver is loaded. For AMT version type f/w
 * this means that the network i/f is open.
 */
static void
igc_get_hw_control(struct igc_softc *sc)
{
	u32 ctrl_ext;

	if (sc->vf_ifp)
		return;

	ctrl_ext = IGC_READ_REG(&sc->hw, IGC_CTRL_EXT);
	IGC_WRITE_REG(&sc->hw, IGC_CTRL_EXT,
	    ctrl_ext | IGC_CTRL_EXT_DRV_LOAD);
}

/*
 * igc_release_hw_control resets {CTRL_EXT|FWSM}:DRV_LOAD bit.
 * For ASF and Pass Through versions of f/w this means that
 * the driver is no longer loaded. For AMT versions of the
 * f/w this means that the network i/f is closed.
 */
static void
igc_release_hw_control(struct igc_softc *sc)
{
	u32 ctrl_ext;

	ctrl_ext = IGC_READ_REG(&sc->hw, IGC_CTRL_EXT);
	IGC_WRITE_REG(&sc->hw, IGC_CTRL_EXT,
	    ctrl_ext & ~IGC_CTRL_EXT_DRV_LOAD);
	return;
}

static int
igc_is_valid_ether_addr(u8 *addr)
{
	char zero_addr[6] = { 0, 0, 0, 0, 0, 0 };

	if ((addr[0] & 1) || (!bcmp(addr, zero_addr, ETHER_ADDR_LEN))) {
		return (false);
	}

	return (true);
}

/*
** Parse the interface capabilities with regard
** to both system management and wake-on-lan for
** later use.
*/
static void
igc_get_wakeup(if_ctx_t ctx)
{
	struct igc_softc *sc = iflib_get_softc(ctx);
	u16 eeprom_data = 0, apme_mask;

	apme_mask = IGC_WUC_APME;
	eeprom_data = IGC_READ_REG(&sc->hw, IGC_WUC);

	if (eeprom_data & apme_mask)
		sc->wol = IGC_WUFC_LNKC;
}


/*
 * Enable PCI Wake On Lan capability
 */
static void
igc_enable_wakeup(if_ctx_t ctx)
{
	struct igc_softc *sc = iflib_get_softc(ctx);
	device_t dev = iflib_get_dev(ctx);
	if_t ifp = iflib_get_ifp(ctx);
	int error = 0;
	u32 pmc, ctrl, rctl;
	u16 status;

	if (pci_find_cap(dev, PCIY_PMG, &pmc) != 0)
		return;

	/*
	 * Determine type of Wakeup: note that wol
	 * is set with all bits on by default.
	 */
	if ((if_getcapenable(ifp) & IFCAP_WOL_MAGIC) == 0)
		sc->wol &= ~IGC_WUFC_MAG;

	if ((if_getcapenable(ifp) & IFCAP_WOL_UCAST) == 0)
		sc->wol &= ~IGC_WUFC_EX;

	if ((if_getcapenable(ifp) & IFCAP_WOL_MCAST) == 0)
		sc->wol &= ~IGC_WUFC_MC;
	else {
		rctl = IGC_READ_REG(&sc->hw, IGC_RCTL);
		rctl |= IGC_RCTL_MPE;
		IGC_WRITE_REG(&sc->hw, IGC_RCTL, rctl);
	}

	if (!(sc->wol & (IGC_WUFC_EX | IGC_WUFC_MAG | IGC_WUFC_MC)))
		goto pme;

	/* Advertise the wakeup capability */
	ctrl = IGC_READ_REG(&sc->hw, IGC_CTRL);
	ctrl |= IGC_CTRL_ADVD3WUC;
	IGC_WRITE_REG(&sc->hw, IGC_CTRL, ctrl);

	/* Enable wakeup by the MAC */
	IGC_WRITE_REG(&sc->hw, IGC_WUC, IGC_WUC_PME_EN);
	IGC_WRITE_REG(&sc->hw, IGC_WUFC, sc->wol);

pme:
	status = pci_read_config(dev, pmc + PCIR_POWER_STATUS, 2);
	status &= ~(PCIM_PSTAT_PME | PCIM_PSTAT_PMEENABLE);
	if (!error && (if_getcapenable(ifp) & IFCAP_WOL))
		status |= PCIM_PSTAT_PME | PCIM_PSTAT_PMEENABLE;
	pci_write_config(dev, pmc + PCIR_POWER_STATUS, status, 2);

	return;
}

/**********************************************************************
 *
 *  Update the board statistics counters.
 *
 **********************************************************************/
static void
igc_update_stats_counters(struct igc_softc *sc)
{
	u64 prev_xoffrxc = sc->stats.xoffrxc;

	sc->stats.crcerrs += IGC_READ_REG(&sc->hw, IGC_CRCERRS);
	sc->stats.mpc += IGC_READ_REG(&sc->hw, IGC_MPC);
	sc->stats.scc += IGC_READ_REG(&sc->hw, IGC_SCC);
	sc->stats.ecol += IGC_READ_REG(&sc->hw, IGC_ECOL);

	sc->stats.mcc += IGC_READ_REG(&sc->hw, IGC_MCC);
	sc->stats.latecol += IGC_READ_REG(&sc->hw, IGC_LATECOL);
	sc->stats.colc += IGC_READ_REG(&sc->hw, IGC_COLC);
	sc->stats.colc += IGC_READ_REG(&sc->hw, IGC_RERC);
	sc->stats.dc += IGC_READ_REG(&sc->hw, IGC_DC);
	sc->stats.rlec += IGC_READ_REG(&sc->hw, IGC_RLEC);
	sc->stats.xonrxc += IGC_READ_REG(&sc->hw, IGC_XONRXC);
	sc->stats.xontxc += IGC_READ_REG(&sc->hw, IGC_XONTXC);
	sc->stats.xoffrxc += IGC_READ_REG(&sc->hw, IGC_XOFFRXC);
	/*
	 * For watchdog management we need to know if we have been
	 * paused during the last interval, so capture that here.
	 */
	if (sc->stats.xoffrxc != prev_xoffrxc)
		sc->shared->isc_pause_frames = 1;
	sc->stats.xofftxc += IGC_READ_REG(&sc->hw, IGC_XOFFTXC);
	sc->stats.fcruc += IGC_READ_REG(&sc->hw, IGC_FCRUC);
	sc->stats.prc64 += IGC_READ_REG(&sc->hw, IGC_PRC64);
	sc->stats.prc127 += IGC_READ_REG(&sc->hw, IGC_PRC127);
	sc->stats.prc255 += IGC_READ_REG(&sc->hw, IGC_PRC255);
	sc->stats.prc511 += IGC_READ_REG(&sc->hw, IGC_PRC511);
	sc->stats.prc1023 += IGC_READ_REG(&sc->hw, IGC_PRC1023);
	sc->stats.prc1522 += IGC_READ_REG(&sc->hw, IGC_PRC1522);
	sc->stats.tlpic += IGC_READ_REG(&sc->hw, IGC_TLPIC);
	sc->stats.rlpic += IGC_READ_REG(&sc->hw, IGC_RLPIC);
	sc->stats.gprc += IGC_READ_REG(&sc->hw, IGC_GPRC);
	sc->stats.bprc += IGC_READ_REG(&sc->hw, IGC_BPRC);
	sc->stats.mprc += IGC_READ_REG(&sc->hw, IGC_MPRC);
	sc->stats.gptc += IGC_READ_REG(&sc->hw, IGC_GPTC);

	/* For the 64-bit byte counters the low dword must be read first. */
	/* Both registers clear on the read of the high dword */

	sc->stats.gorc += IGC_READ_REG(&sc->hw, IGC_GORCL) +
	    ((u64)IGC_READ_REG(&sc->hw, IGC_GORCH) << 32);
	sc->stats.gotc += IGC_READ_REG(&sc->hw, IGC_GOTCL) +
	    ((u64)IGC_READ_REG(&sc->hw, IGC_GOTCH) << 32);

	sc->stats.rnbc += IGC_READ_REG(&sc->hw, IGC_RNBC);
	sc->stats.ruc += IGC_READ_REG(&sc->hw, IGC_RUC);
	sc->stats.rfc += IGC_READ_REG(&sc->hw, IGC_RFC);
	sc->stats.roc += IGC_READ_REG(&sc->hw, IGC_ROC);
	sc->stats.rjc += IGC_READ_REG(&sc->hw, IGC_RJC);

	sc->stats.mgprc += IGC_READ_REG(&sc->hw, IGC_MGTPRC);
	sc->stats.mgpdc += IGC_READ_REG(&sc->hw, IGC_MGTPDC);
	sc->stats.mgptc += IGC_READ_REG(&sc->hw, IGC_MGTPTC);

	sc->stats.tor += IGC_READ_REG(&sc->hw, IGC_TORH);
	sc->stats.tot += IGC_READ_REG(&sc->hw, IGC_TOTH);

	sc->stats.tpr += IGC_READ_REG(&sc->hw, IGC_TPR);
	sc->stats.tpt += IGC_READ_REG(&sc->hw, IGC_TPT);
	sc->stats.ptc64 += IGC_READ_REG(&sc->hw, IGC_PTC64);
	sc->stats.ptc127 += IGC_READ_REG(&sc->hw, IGC_PTC127);
	sc->stats.ptc255 += IGC_READ_REG(&sc->hw, IGC_PTC255);
	sc->stats.ptc511 += IGC_READ_REG(&sc->hw, IGC_PTC511);
	sc->stats.ptc1023 += IGC_READ_REG(&sc->hw, IGC_PTC1023);
	sc->stats.ptc1522 += IGC_READ_REG(&sc->hw, IGC_PTC1522);
	sc->stats.mptc += IGC_READ_REG(&sc->hw, IGC_MPTC);
	sc->stats.bptc += IGC_READ_REG(&sc->hw, IGC_BPTC);

	/* Interrupt Counts */
	sc->stats.iac += IGC_READ_REG(&sc->hw, IGC_IAC);
	sc->stats.rxdmtc += IGC_READ_REG(&sc->hw, IGC_RXDMTC);

	sc->stats.algnerrc += IGC_READ_REG(&sc->hw, IGC_ALGNERRC);
	sc->stats.tncrs += IGC_READ_REG(&sc->hw, IGC_TNCRS);
	sc->stats.htdpmc += IGC_READ_REG(&sc->hw, IGC_HTDPMC);
	sc->stats.tsctc += IGC_READ_REG(&sc->hw, IGC_TSCTC);
}

static uint64_t
igc_if_get_counter(if_ctx_t ctx, ift_counter cnt)
{
	struct igc_softc *sc = iflib_get_softc(ctx);
	if_t ifp = iflib_get_ifp(ctx);

	switch (cnt) {
	case IFCOUNTER_COLLISIONS:
		return (sc->stats.colc);
	case IFCOUNTER_IERRORS:
		return (sc->dropped_pkts + sc->stats.rxerrc +
		    sc->stats.crcerrs + sc->stats.algnerrc +
		    sc->stats.ruc + sc->stats.roc +
		    sc->stats.mpc + sc->stats.htdpmc);
	case IFCOUNTER_OERRORS:
		return (sc->stats.ecol + sc->stats.latecol +
		    sc->watchdog_events);
	default:
		return (if_get_counter_default(ifp, cnt));
	}
}

/* igc_if_needs_restart - Tell iflib when the driver needs to be reinitialized
 * @ctx: iflib context
 * @event: event code to check
 *
 * Defaults to returning false for unknown events.
 *
 * @returns true if iflib needs to reinit the interface
 */
static bool
igc_if_needs_restart(if_ctx_t ctx __unused, enum iflib_restart_event event)
{
	switch (event) {
	case IFLIB_RESTART_VLAN_CONFIG:
	default:
		return (false);
	}
}

/* Export a single 32-bit register via a read-only sysctl. */
static int
igc_sysctl_reg_handler(SYSCTL_HANDLER_ARGS)
{
	struct igc_softc *sc;
	u_int val;

	sc = oidp->oid_arg1;
	val = IGC_READ_REG(&sc->hw, oidp->oid_arg2);
	return (sysctl_handle_int(oidp, &val, 0, req));
}

/* Per queue holdoff interrupt rate handler */
static int
igc_sysctl_interrupt_rate_handler(SYSCTL_HANDLER_ARGS)
{
	struct igc_rx_queue *rque;
	struct igc_tx_queue *tque;
	struct igc_hw *hw;
	int error;
	u32 reg, usec, rate;

	bool tx = oidp->oid_arg2;

	if (tx) {
		tque = oidp->oid_arg1;
		hw = &tque->sc->hw;
		reg = IGC_READ_REG(hw, IGC_EITR(tque->me));
	} else {
		rque = oidp->oid_arg1;
		hw = &rque->sc->hw;
		reg = IGC_READ_REG(hw, IGC_EITR(rque->msix));
	}

	usec = (reg & IGC_QVECTOR_MASK);
	if (usec > 0)
		rate = IGC_INTS_TO_EITR(usec);
	else
		rate = 0;

	error = sysctl_handle_int(oidp, &rate, 0, req);
	if (error || !req->newptr)
		return error;
	return 0;
}

/*
 * Add sysctl variables, one per statistic, to the system.
 */
static void
igc_add_hw_stats(struct igc_softc *sc)
{
	device_t dev = iflib_get_dev(sc->ctx);
	struct igc_tx_queue *tx_que = sc->tx_queues;
	struct igc_rx_queue *rx_que = sc->rx_queues;

	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(dev);
	struct sysctl_oid *tree = device_get_sysctl_tree(dev);
	struct sysctl_oid_list *child = SYSCTL_CHILDREN(tree);
	struct igc_hw_stats *stats = &sc->stats;

	struct sysctl_oid *stat_node, *queue_node, *int_node;
	struct sysctl_oid_list *stat_list, *queue_list, *int_list;

#define QUEUE_NAME_LEN 32
	char namebuf[QUEUE_NAME_LEN];

	/* Driver Statistics */
	SYSCTL_ADD_ULONG(ctx, child, OID_AUTO, "dropped",
			CTLFLAG_RD, &sc->dropped_pkts,
			"Driver dropped packets");
	SYSCTL_ADD_ULONG(ctx, child, OID_AUTO, "link_irq",
			CTLFLAG_RD, &sc->link_irq,
			"Link MSI-X IRQ Handled");
	SYSCTL_ADD_ULONG(ctx, child, OID_AUTO, "rx_overruns",
			CTLFLAG_RD, &sc->rx_overruns,
			"RX overruns");
	SYSCTL_ADD_ULONG(ctx, child, OID_AUTO, "watchdog_timeouts",
			CTLFLAG_RD, &sc->watchdog_events,
			"Watchdog timeouts");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "device_control",
	    CTLTYPE_UINT | CTLFLAG_RD | CTLFLAG_NEEDGIANT,
	    sc, IGC_CTRL, igc_sysctl_reg_handler, "IU",
	    "Device Control Register");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "rx_control",
	    CTLTYPE_UINT | CTLFLAG_RD | CTLFLAG_NEEDGIANT,
	    sc, IGC_RCTL, igc_sysctl_reg_handler, "IU",
	    "Receiver Control Register");
	SYSCTL_ADD_UINT(ctx, child, OID_AUTO, "fc_high_water",
			CTLFLAG_RD, &sc->hw.fc.high_water, 0,
			"Flow Control High Watermark");
	SYSCTL_ADD_UINT(ctx, child, OID_AUTO, "fc_low_water",
			CTLFLAG_RD, &sc->hw.fc.low_water, 0,
			"Flow Control Low Watermark");

	for (int i = 0; i < sc->tx_num_queues; i++, tx_que++) {
		struct tx_ring *txr = &tx_que->txr;
		snprintf(namebuf, QUEUE_NAME_LEN, "queue_tx_%d", i);
		queue_node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, namebuf,
		    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "TX Queue Name");
		queue_list = SYSCTL_CHILDREN(queue_node);

		SYSCTL_ADD_PROC(ctx, queue_list, OID_AUTO, "interrupt_rate",
		    CTLTYPE_UINT | CTLFLAG_RD, tx_que,
		    true, igc_sysctl_interrupt_rate_handler, "IU",
		    "Interrupt Rate");
		SYSCTL_ADD_PROC(ctx, queue_list, OID_AUTO, "txd_head",
		    CTLTYPE_UINT | CTLFLAG_RD | CTLFLAG_NEEDGIANT, sc,
		    IGC_TDH(txr->me), igc_sysctl_reg_handler, "IU",
		    "Transmit Descriptor Head");
		SYSCTL_ADD_PROC(ctx, queue_list, OID_AUTO, "txd_tail",
		    CTLTYPE_UINT | CTLFLAG_RD | CTLFLAG_NEEDGIANT, sc,
		    IGC_TDT(txr->me), igc_sysctl_reg_handler, "IU",
		    "Transmit Descriptor Tail");
		SYSCTL_ADD_ULONG(ctx, queue_list, OID_AUTO, "tx_irq",
				CTLFLAG_RD, &txr->tx_irq,
				"Queue MSI-X Transmit Interrupts");
	}

	for (int j = 0; j < sc->rx_num_queues; j++, rx_que++) {
		struct rx_ring *rxr = &rx_que->rxr;
		snprintf(namebuf, QUEUE_NAME_LEN, "queue_rx_%d", j);
		queue_node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, namebuf,
		    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "RX Queue Name");
		queue_list = SYSCTL_CHILDREN(queue_node);

		SYSCTL_ADD_PROC(ctx, queue_list, OID_AUTO, "interrupt_rate",
		    CTLTYPE_UINT | CTLFLAG_RD, rx_que,
			false, igc_sysctl_interrupt_rate_handler, "IU",
			"Interrupt Rate");
		SYSCTL_ADD_PROC(ctx, queue_list, OID_AUTO, "rxd_head",
		    CTLTYPE_UINT | CTLFLAG_RD | CTLFLAG_NEEDGIANT, sc,
		    IGC_RDH(rxr->me), igc_sysctl_reg_handler, "IU",
		    "Receive Descriptor Head");
		SYSCTL_ADD_PROC(ctx, queue_list, OID_AUTO, "rxd_tail",
		    CTLTYPE_UINT | CTLFLAG_RD | CTLFLAG_NEEDGIANT, sc,
		    IGC_RDT(rxr->me), igc_sysctl_reg_handler, "IU",
		    "Receive Descriptor Tail");
		SYSCTL_ADD_ULONG(ctx, queue_list, OID_AUTO, "rx_irq",
				CTLFLAG_RD, &rxr->rx_irq,
				"Queue MSI-X Receive Interrupts");
	}

	/* MAC stats get their own sub node */

	stat_node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, "mac_stats",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "Statistics");
	stat_list = SYSCTL_CHILDREN(stat_node);

	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "excess_coll",
			CTLFLAG_RD, &stats->ecol,
			"Excessive collisions");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "single_coll",
			CTLFLAG_RD, &stats->scc,
			"Single collisions");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "multiple_coll",
			CTLFLAG_RD, &stats->mcc,
			"Multiple collisions");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "late_coll",
			CTLFLAG_RD, &stats->latecol,
			"Late collisions");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "collision_count",
			CTLFLAG_RD, &stats->colc,
			"Collision Count");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "symbol_errors",
			CTLFLAG_RD, &sc->stats.symerrs,
			"Symbol Errors");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "sequence_errors",
			CTLFLAG_RD, &sc->stats.sec,
			"Sequence Errors");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "defer_count",
			CTLFLAG_RD, &sc->stats.dc,
			"Defer Count");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "missed_packets",
			CTLFLAG_RD, &sc->stats.mpc,
			"Missed Packets");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "recv_length_errors",
			CTLFLAG_RD, &sc->stats.rlec,
			"Receive Length Errors");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "recv_no_buff",
			CTLFLAG_RD, &sc->stats.rnbc,
			"Receive No Buffers");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "recv_undersize",
			CTLFLAG_RD, &sc->stats.ruc,
			"Receive Undersize");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "recv_fragmented",
			CTLFLAG_RD, &sc->stats.rfc,
			"Fragmented Packets Received ");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "recv_oversize",
			CTLFLAG_RD, &sc->stats.roc,
			"Oversized Packets Received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "recv_jabber",
			CTLFLAG_RD, &sc->stats.rjc,
			"Recevied Jabber");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "recv_errs",
			CTLFLAG_RD, &sc->stats.rxerrc,
			"Receive Errors");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "crc_errs",
			CTLFLAG_RD, &sc->stats.crcerrs,
			"CRC errors");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "alignment_errs",
			CTLFLAG_RD, &sc->stats.algnerrc,
			"Alignment Errors");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "xon_recvd",
			CTLFLAG_RD, &sc->stats.xonrxc,
			"XON Received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "xon_txd",
			CTLFLAG_RD, &sc->stats.xontxc,
			"XON Transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "xoff_recvd",
			CTLFLAG_RD, &sc->stats.xoffrxc,
			"XOFF Received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "xoff_txd",
			CTLFLAG_RD, &sc->stats.xofftxc,
			"XOFF Transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "unsupported_fc_recvd",
			CTLFLAG_RD, &sc->stats.fcruc,
			"Unsupported Flow Control Received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "mgmt_pkts_recvd",
			CTLFLAG_RD, &sc->stats.mgprc,
			"Management Packets Received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "mgmt_pkts_drop",
			CTLFLAG_RD, &sc->stats.mgpdc,
			"Management Packets Dropped");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "mgmt_pkts_txd",
			CTLFLAG_RD, &sc->stats.mgptc,
			"Management Packets Transmitted");

	/* Packet Reception Stats */
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "total_pkts_recvd",
			CTLFLAG_RD, &sc->stats.tpr,
			"Total Packets Received ");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "good_pkts_recvd",
			CTLFLAG_RD, &sc->stats.gprc,
			"Good Packets Received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "bcast_pkts_recvd",
			CTLFLAG_RD, &sc->stats.bprc,
			"Broadcast Packets Received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "mcast_pkts_recvd",
			CTLFLAG_RD, &sc->stats.mprc,
			"Multicast Packets Received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "rx_frames_64",
			CTLFLAG_RD, &sc->stats.prc64,
			"64 byte frames received ");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "rx_frames_65_127",
			CTLFLAG_RD, &sc->stats.prc127,
			"65-127 byte frames received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "rx_frames_128_255",
			CTLFLAG_RD, &sc->stats.prc255,
			"128-255 byte frames received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "rx_frames_256_511",
			CTLFLAG_RD, &sc->stats.prc511,
			"256-511 byte frames received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "rx_frames_512_1023",
			CTLFLAG_RD, &sc->stats.prc1023,
			"512-1023 byte frames received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "rx_frames_1024_1522",
			CTLFLAG_RD, &sc->stats.prc1522,
			"1023-1522 byte frames received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "good_octets_recvd",
			CTLFLAG_RD, &sc->stats.gorc,
			"Good Octets Received");

	/* Packet Transmission Stats */
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "good_octets_txd",
			CTLFLAG_RD, &sc->stats.gotc,
			"Good Octets Transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "total_pkts_txd",
			CTLFLAG_RD, &sc->stats.tpt,
			"Total Packets Transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "good_pkts_txd",
			CTLFLAG_RD, &sc->stats.gptc,
			"Good Packets Transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "bcast_pkts_txd",
			CTLFLAG_RD, &sc->stats.bptc,
			"Broadcast Packets Transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "mcast_pkts_txd",
			CTLFLAG_RD, &sc->stats.mptc,
			"Multicast Packets Transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "tx_frames_64",
			CTLFLAG_RD, &sc->stats.ptc64,
			"64 byte frames transmitted ");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "tx_frames_65_127",
			CTLFLAG_RD, &sc->stats.ptc127,
			"65-127 byte frames transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "tx_frames_128_255",
			CTLFLAG_RD, &sc->stats.ptc255,
			"128-255 byte frames transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "tx_frames_256_511",
			CTLFLAG_RD, &sc->stats.ptc511,
			"256-511 byte frames transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "tx_frames_512_1023",
			CTLFLAG_RD, &sc->stats.ptc1023,
			"512-1023 byte frames transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "tx_frames_1024_1522",
			CTLFLAG_RD, &sc->stats.ptc1522,
			"1024-1522 byte frames transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "tso_txd",
			CTLFLAG_RD, &sc->stats.tsctc,
			"TSO Contexts Transmitted");

	/* Interrupt Stats */

	int_node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, "interrupts",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "Interrupt Statistics");
	int_list = SYSCTL_CHILDREN(int_node);

	SYSCTL_ADD_UQUAD(ctx, int_list, OID_AUTO, "asserts",
			CTLFLAG_RD, &sc->stats.iac,
			"Interrupt Assertion Count");

	SYSCTL_ADD_UQUAD(ctx, int_list, OID_AUTO, "rx_desc_min_thresh",
			CTLFLAG_RD, &sc->stats.rxdmtc,
			"Rx Desc Min Thresh Count");
}

static void
igc_fw_version(struct igc_softc *sc)
{
	struct igc_hw *hw = &sc->hw;
	struct igc_fw_version *fw_ver = &sc->fw_ver;

	*fw_ver = (struct igc_fw_version){0};

	igc_get_fw_version(hw, fw_ver);
}

static void
igc_sbuf_fw_version(struct igc_fw_version *fw_ver, struct sbuf *buf)
{
	const char *space = "";

	if (fw_ver->eep_major || fw_ver->eep_minor || fw_ver->eep_build) {
		sbuf_printf(buf, "EEPROM V%d.%d-%d", fw_ver->eep_major,
			    fw_ver->eep_minor, fw_ver->eep_build);
		space = " ";
	}

	if (fw_ver->invm_major || fw_ver->invm_minor || fw_ver->invm_img_type) {
		sbuf_printf(buf, "%sNVM V%d.%d imgtype%d",
			    space, fw_ver->invm_major, fw_ver->invm_minor,
			    fw_ver->invm_img_type);
		space = " ";
	}

	if (fw_ver->or_valid) {
		sbuf_printf(buf, "%sOption ROM V%d-b%d-p%d",
			    space, fw_ver->or_major, fw_ver->or_build,
			    fw_ver->or_patch);
		space = " ";
	}

	if (fw_ver->etrack_id)
		sbuf_printf(buf, "%seTrack 0x%08x", space, fw_ver->etrack_id);
}

static void
igc_print_fw_version(struct igc_softc *sc )
{
	device_t dev = sc->dev;
	struct sbuf *buf;
	int error = 0;

	buf = sbuf_new_auto();
	if (!buf) {
		device_printf(dev, "Could not allocate sbuf for output.\n");
		return;
	}

	igc_sbuf_fw_version(&sc->fw_ver, buf);

	error = sbuf_finish(buf);
	if (error)
		device_printf(dev, "Error finishing sbuf: %d\n", error);
	else if (sbuf_len(buf))
		device_printf(dev, "%s\n", sbuf_data(buf));

	sbuf_delete(buf);
}

static int
igc_sysctl_print_fw_version(SYSCTL_HANDLER_ARGS)
{
	struct igc_softc *sc = (struct igc_softc *)arg1;
	device_t dev = sc->dev;
	struct sbuf *buf;
	int error = 0;

	buf = sbuf_new_for_sysctl(NULL, NULL, 128, req);
	if (!buf) {
		device_printf(dev, "Could not allocate sbuf for output.\n");
		return (ENOMEM);
	}

	igc_sbuf_fw_version(&sc->fw_ver, buf);

	error = sbuf_finish(buf);
	if (error)
		device_printf(dev, "Error finishing sbuf: %d\n", error);

	sbuf_delete(buf);

	return (0);
}

/**********************************************************************
 *
 *  This routine provides a way to dump out the adapter eeprom,
 *  often a useful debug/service tool. This only dumps the first
 *  32 words, stuff that matters is in that extent.
 *
 **********************************************************************/
static int
igc_sysctl_nvm_info(SYSCTL_HANDLER_ARGS)
{
	struct igc_softc *sc = (struct igc_softc *)arg1;
	int error;
	int result;

	result = -1;
	error = sysctl_handle_int(oidp, &result, 0, req);

	if (error || !req->newptr)
		return (error);

	/*
	 * This value will cause a hex dump of the
	 * first 32 16-bit words of the EEPROM to
	 * the screen.
	 */
	if (result == 1)
		igc_print_nvm_info(sc);

	return (error);
}

static void
igc_print_nvm_info(struct igc_softc *sc)
{
	u16 eeprom_data;
	int i, j, row = 0;

	/* Its a bit crude, but it gets the job done */
	printf("\nInterface EEPROM Dump:\n");
	printf("Offset\n0x0000  ");
	for (i = 0, j = 0; i < 32; i++, j++) {
		if (j == 8) { /* Make the offset block */
			j = 0; ++row;
			printf("\n0x00%x0  ",row);
		}
		igc_read_nvm(&sc->hw, i, 1, &eeprom_data);
		printf("%04x ", eeprom_data);
	}
	printf("\n");
}

/*
 * Set flow control using sysctl:
 * Flow control values:
 *      0 - off
 *      1 - rx pause
 *      2 - tx pause
 *      3 - full
 */
static int
igc_set_flowcntl(SYSCTL_HANDLER_ARGS)
{
	int error;
	static int input = 3; /* default is full */
	struct igc_softc	*sc = (struct igc_softc *) arg1;

	error = sysctl_handle_int(oidp, &input, 0, req);

	if ((error) || (req->newptr == NULL))
		return (error);

	if (input == sc->fc) /* no change? */
		return (error);

	switch (input) {
	case igc_fc_rx_pause:
	case igc_fc_tx_pause:
	case igc_fc_full:
	case igc_fc_none:
		sc->hw.fc.requested_mode = input;
		sc->fc = input;
		break;
	default:
		/* Do nothing */
		return (error);
	}

	sc->hw.fc.current_mode = sc->hw.fc.requested_mode;
	igc_force_mac_fc(&sc->hw);
	return (error);
}

/*
 * Manage DMA Coalesce:
 * Control values:
 * 	0/1 - off/on
 *	Legal timer values are:
 *	250,500,1000-10000 in thousands
 */
static int
igc_sysctl_dmac(SYSCTL_HANDLER_ARGS)
{
	struct igc_softc *sc = (struct igc_softc *) arg1;
	int error;

	error = sysctl_handle_int(oidp, &sc->dmac, 0, req);

	if ((error) || (req->newptr == NULL))
		return (error);

	switch (sc->dmac) {
		case 0:
			/* Disabling */
			break;
		case 1: /* Just enable and use default */
			sc->dmac = 1000;
			break;
		case 250:
		case 500:
		case 1000:
		case 2000:
		case 3000:
		case 4000:
		case 5000:
		case 6000:
		case 7000:
		case 8000:
		case 9000:
		case 10000:
			/* Legal values - allow */
			break;
		default:
			/* Do nothing, illegal value */
			sc->dmac = 0;
			return (EINVAL);
	}
	/* Reinit the interface */
	igc_if_init(sc->ctx);
	return (error);
}

/*
 * Manage Energy Efficient Ethernet:
 * Control values:
 *     0/1 - enabled/disabled
 */
static int
igc_sysctl_eee(SYSCTL_HANDLER_ARGS)
{
	struct igc_softc *sc = (struct igc_softc *) arg1;
	int error, value;

	value = sc->hw.dev_spec._i225.eee_disable;
	error = sysctl_handle_int(oidp, &value, 0, req);
	if (error || req->newptr == NULL)
		return (error);

	sc->hw.dev_spec._i225.eee_disable = (value != 0);
	igc_if_init(sc->ctx);

	return (0);
}

static int
igc_sysctl_debug_info(SYSCTL_HANDLER_ARGS)
{
	struct igc_softc *sc;
	int error;
	int result;

	result = -1;
	error = sysctl_handle_int(oidp, &result, 0, req);

	if (error || !req->newptr)
		return (error);

	if (result == 1) {
		sc = (struct igc_softc *) arg1;
		igc_print_debug_info(sc);
	}

	return (error);
}

static int
igc_get_rs(SYSCTL_HANDLER_ARGS)
{
	struct igc_softc *sc = (struct igc_softc *) arg1;
	int error;
	int result;

	result = 0;
	error = sysctl_handle_int(oidp, &result, 0, req);

	if (error || !req->newptr || result != 1)
		return (error);
	igc_dump_rs(sc);

	return (error);
}

static void
igc_if_debug(if_ctx_t ctx)
{
	igc_dump_rs(iflib_get_softc(ctx));
}

/*
 * This routine is meant to be fluid, add whatever is
 * needed for debugging a problem.  -jfv
 */
static void
igc_print_debug_info(struct igc_softc *sc)
{
	device_t dev = iflib_get_dev(sc->ctx);
	if_t ifp = iflib_get_ifp(sc->ctx);
	struct tx_ring *txr = &sc->tx_queues->txr;
	struct rx_ring *rxr = &sc->rx_queues->rxr;

	if (if_getdrvflags(ifp) & IFF_DRV_RUNNING)
		printf("Interface is RUNNING ");
	else
		printf("Interface is NOT RUNNING\n");

	if (if_getdrvflags(ifp) & IFF_DRV_OACTIVE)
		printf("and INACTIVE\n");
	else
		printf("and ACTIVE\n");

	for (int i = 0; i < sc->tx_num_queues; i++, txr++) {
		device_printf(dev, "TX Queue %d ------\n", i);
		device_printf(dev, "hw tdh = %d, hw tdt = %d\n",
			IGC_READ_REG(&sc->hw, IGC_TDH(i)),
			IGC_READ_REG(&sc->hw, IGC_TDT(i)));

	}
	for (int j=0; j < sc->rx_num_queues; j++, rxr++) {
		device_printf(dev, "RX Queue %d ------\n", j);
		device_printf(dev, "hw rdh = %d, hw rdt = %d\n",
			IGC_READ_REG(&sc->hw, IGC_RDH(j)),
			IGC_READ_REG(&sc->hw, IGC_RDT(j)));
	}
}
