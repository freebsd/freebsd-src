/*-
 * Copyright (c) 2007-2011 Broadcom Corporation. All rights reserved.
 *
 *    Gary Zambrano <zambrano@broadcom.com>
 *    David Christensen <davidch@broadcom.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Broadcom Corporation nor the name of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written consent.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * The following controllers are supported by this driver:
 *   BCM57710  A1+
 *   BCM57711  A0+
 *   BCM57711E A0+
 *
 * The following controllers are not supported by this driver:
 *   BCM57710  A0 (pre-production)
 *
 * External PHY References:
 * ------------------------
 * BCM8073  - Dual Port 10GBase-KR Ethernet PHY
 * BCM8705  - 10Gb Ethernet Serial Transceiver
 * BCM8706  - 10Gb Ethernet LRM PHY
 * BCM8726  - Dual Port 10Gb Ethernet LRM PHY
 * BCM8727  - Dual Port 10Gb Ethernet LRM PHY
 * BCM8481  - Single Port 10GBase-T Ethernet PHY
 * BCM84823 - Dual Port 10GBase-T Ethernet PHY
 * SFX7101  - Solarflare 10GBase-T Ethernet PHY
 *
 */

#include "opt_bxe.h"
#include "bxe_include.h"
#include "if_bxe.h"
#include "bxe_init.h"

#include "hw_dump_reg_st.h"
#include "dump_e1.h"
#include "dump_e1h.h"

#include "bxe_self_test.h"

/* BXE Debug Options */
#ifdef BXE_DEBUG
uint32_t bxe_debug = BXE_WARN;


/*          0 = Never              */
/*          1 = 1 in 2,147,483,648 */
/*        256 = 1 in     8,388,608 */
/*       2048 = 1 in     1,048,576 */
/*      65536 = 1 in        32,768 */
/*    1048576 = 1 in         2,048 */
/*  268435456 =	1 in             8 */
/*  536870912 = 1 in             4 */
/* 1073741824 = 1 in             2 */

/* Controls how often to simulate an mbuf allocation failure. */
int bxe_debug_mbuf_allocation_failure = 0;

/* Controls how often to simulate a DMA mapping failure. */
int bxe_debug_dma_map_addr_failure = 0;

/* Controls how often to received frame error. */
int bxe_debug_received_frame_error = 0;

/* Controls how often to simulate a bootcode failure. */
int bxe_debug_bootcode_running_failure = 0;
#endif

#define	MDIO_INDIRECT_REG_ADDR	0x1f
#define	MDIO_SET_REG_BANK(sc, reg_bank)         \
	bxe_mdio22_write(sc, MDIO_INDIRECT_REG_ADDR, reg_bank)

#define	MDIO_ACCESS_TIMEOUT	1000
#define	BMAC_CONTROL_RX_ENABLE	2

/* BXE Build Time Options */
/* #define BXE_NVRAM_WRITE 1 */
#define	USE_DMAE 1

/*
 * PCI Device ID Table
 * Used by bxe_probe() to identify the devices supported by this driver.
 */
#define	BXE_DEVDESC_MAX		64

static struct bxe_type bxe_devs[] = {
	/* BCM57710 Controllers and OEM boards. */
	{ BRCM_VENDORID, BRCM_DEVICEID_BCM57710, PCI_ANY_ID,  PCI_ANY_ID,
	    "Broadcom NetXtreme II BCM57710 10GbE" },
	/* BCM57711 Controllers and OEM boards. */
	{ BRCM_VENDORID, BRCM_DEVICEID_BCM57711, PCI_ANY_ID,  PCI_ANY_ID,
	    "Broadcom NetXtreme II BCM57711 10GbE" },
	/* BCM57711E Controllers and OEM boards. */
	{ BRCM_VENDORID, BRCM_DEVICEID_BCM57711E, PCI_ANY_ID,  PCI_ANY_ID,
	    "Broadcom NetXtreme II BCM57711E 10GbE" },
	{0, 0, 0, 0, NULL}
};

/*
 * FreeBSD device entry points.
 */
static int  bxe_probe(device_t);
static int  bxe_attach(device_t);
static int  bxe_detach(device_t);
static int  bxe_shutdown(device_t);

static void bxe_set_tunables(struct bxe_softc *);
static void bxe_print_adapter_info(struct bxe_softc *);
static void bxe_probe_pci_caps(struct bxe_softc *);
static void bxe_link_settings_supported(struct bxe_softc *, uint32_t);
static void bxe_link_settings_requested(struct bxe_softc *);
static int  bxe_get_function_hwinfo(struct bxe_softc *);
static void bxe_get_port_hwinfo(struct bxe_softc *);
static void bxe_get_common_hwinfo(struct bxe_softc *);
static void bxe_undi_unload(struct bxe_softc *);
static int  bxe_setup_leading(struct bxe_softc *);
static int  bxe_stop_leading(struct bxe_softc *);
static int  bxe_setup_multi(struct bxe_softc *, int);
static int  bxe_stop_multi(struct bxe_softc *, int);
static int  bxe_stop_locked(struct bxe_softc *, int);
static int  bxe_alloc_buf_rings(struct bxe_softc *);
static void bxe_free_buf_rings(struct bxe_softc *);
static void bxe_init_locked(struct bxe_softc *, int);
static int  bxe_wait_ramrod(struct bxe_softc *, int, int, int *, int);
static void bxe_init_str_wr(struct bxe_softc *, uint32_t, const uint32_t *,
	    uint32_t);
static void bxe_init_ind_wr(struct bxe_softc *, uint32_t, const uint32_t *,
	    uint16_t);
static void bxe_init_wr_64(struct bxe_softc *, uint32_t, const uint32_t *,
	    uint32_t);
static void bxe_write_big_buf(struct bxe_softc *, uint32_t, uint32_t);
static void bxe_init_fill(struct bxe_softc *, uint32_t, int, uint32_t);
static void bxe_init_block(struct bxe_softc *, uint32_t, uint32_t);
static void bxe_init(void *);
static void bxe_release_resources(struct bxe_softc *);
static void bxe_reg_wr_ind(struct bxe_softc *, uint32_t, uint32_t);
static uint32_t bxe_reg_rd_ind(struct bxe_softc *, uint32_t);
static void bxe_post_dmae(struct bxe_softc *, struct dmae_command *, int);
static void bxe_wb_wr(struct bxe_softc *, int, uint32_t, uint32_t);
static __inline uint32_t bxe_reg_poll(struct bxe_softc *, uint32_t,
	    uint32_t, int, int);
static int  bxe_mc_assert(struct bxe_softc *);
static void bxe_panic_dump(struct bxe_softc *);
static void bxe_int_enable(struct bxe_softc *);
static void bxe_int_disable(struct bxe_softc *);

static int  bxe_nvram_acquire_lock(struct bxe_softc *);
static int  bxe_nvram_release_lock(struct bxe_softc *);
static void bxe_nvram_enable_access(struct bxe_softc *);
static void bxe_nvram_disable_access(struct bxe_softc *);
static int  bxe_nvram_read_dword	(struct bxe_softc *, uint32_t, uint32_t *,
	    uint32_t);
static int  bxe_nvram_read(struct bxe_softc *, uint32_t, uint8_t *, int);

#ifdef BXE_NVRAM_WRITE_SUPPORT
static int  bxe_nvram_write_dword(struct bxe_softc *, uint32_t, uint32_t,
	    uint32_t);
static int  bxe_nvram_write1(struct bxe_softc *, uint32_t, uint8_t *, int);
static int  bxe_nvram_write(struct bxe_softc *, uint32_t, uint8_t *, int);
#endif

static int bxe_nvram_test(struct bxe_softc *);

static __inline void bxe_ack_sb(struct bxe_softc *, uint8_t, uint8_t, uint16_t,
	    uint8_t, uint8_t);
static __inline uint16_t bxe_update_fpsb_idx(struct bxe_fastpath *);
static uint16_t bxe_ack_int(struct bxe_softc *);
static void bxe_sp_event(struct bxe_fastpath *, union eth_rx_cqe *);
static int  bxe_acquire_hw_lock(struct bxe_softc *, uint32_t);
static int  bxe_release_hw_lock(struct bxe_softc *, uint32_t);
static void bxe_acquire_phy_lock(struct bxe_softc *);
static void bxe_release_phy_lock(struct bxe_softc *);
static void bxe_pmf_update(struct bxe_softc *);
static void bxe_init_port_minmax(struct bxe_softc *);
static void bxe_link_attn(struct bxe_softc *);

static int  bxe_sp_post(struct bxe_softc *, int, int, uint32_t, uint32_t, int);
static int  bxe_acquire_alr(struct bxe_softc *);
static void bxe_release_alr(struct bxe_softc *);
static uint16_t  bxe_update_dsb_idx(struct bxe_softc *);
static void bxe_attn_int_asserted(struct bxe_softc *, uint32_t);
static __inline void bxe_attn_int_deasserted0(struct bxe_softc *, uint32_t);
static __inline void bxe_attn_int_deasserted1(struct bxe_softc *, uint32_t);
static __inline void bxe_attn_int_deasserted2(struct bxe_softc *, uint32_t);
static __inline void bxe_attn_int_deasserted3(struct bxe_softc *, uint32_t);
static void bxe_attn_int_deasserted(struct bxe_softc *, uint32_t);
static void bxe_attn_int(struct bxe_softc *);

static void bxe_stats_storm_post(struct bxe_softc *);
static void bxe_stats_init(struct bxe_softc *);
static void bxe_stats_hw_post(struct bxe_softc *);
static int  bxe_stats_comp(struct bxe_softc *);
static void bxe_stats_pmf_update(struct bxe_softc *);
static void bxe_stats_port_base_init(struct bxe_softc *);
static void bxe_stats_port_init(struct bxe_softc *);
static void bxe_stats_func_base_init(struct bxe_softc *);
static void bxe_stats_func_init(struct bxe_softc *);
static void bxe_stats_start(struct bxe_softc *);
static void bxe_stats_pmf_start(struct bxe_softc *);
static void bxe_stats_restart(struct bxe_softc *);
static void bxe_stats_bmac_update(struct bxe_softc *);
static void bxe_stats_emac_update(struct bxe_softc *);
static int  bxe_stats_hw_update(struct bxe_softc *);
static int  bxe_stats_storm_update(struct bxe_softc *);
static void bxe_stats_func_base_update(struct bxe_softc *);
static void bxe_stats_update(struct bxe_softc *);
static void bxe_stats_port_stop(struct bxe_softc *);
static void bxe_stats_stop(struct bxe_softc *);
static void bxe_stats_do_nothing(struct bxe_softc *);
static void bxe_stats_handle(struct bxe_softc *, enum bxe_stats_event);

static int  bxe_tx_encap(struct bxe_fastpath *, struct mbuf **);
static void bxe_tx_start(struct ifnet *);
static void bxe_tx_start_locked(struct ifnet *, struct bxe_fastpath *);
static int  bxe_tx_mq_start(struct ifnet *, struct mbuf *);
static int  bxe_tx_mq_start_locked(struct ifnet *, struct bxe_fastpath *,
    struct mbuf *);
static void bxe_mq_flush(struct ifnet *ifp);
static int  bxe_ioctl(struct ifnet *, u_long, caddr_t);
static __inline int bxe_has_rx_work(struct bxe_fastpath *);
static __inline int bxe_has_tx_work(struct bxe_fastpath *);

static void bxe_intr_legacy(void *);
static void bxe_task_sp(void *, int);
static void bxe_intr_sp(void *);
static void bxe_task_fp(void *, int);
static void bxe_intr_fp(void *);
static void bxe_zero_sb(struct bxe_softc *, int);
static void bxe_init_sb(struct bxe_softc *, struct host_status_block *,
	    bus_addr_t, int);
static void bxe_zero_def_sb(struct bxe_softc *);
static void bxe_init_def_sb(struct bxe_softc *, struct host_def_status_block *,
	    bus_addr_t, int);
static void bxe_update_coalesce(struct bxe_softc *);
static __inline void bxe_update_rx_prod(struct bxe_softc *,
	    struct bxe_fastpath *, uint16_t, uint16_t, uint16_t);
static void bxe_clear_sge_mask_next_elems(struct bxe_fastpath *);
static __inline void bxe_init_sge_ring_bit_mask(struct bxe_fastpath *);
static __inline void bxe_free_tpa_pool(struct bxe_fastpath *, int);
static __inline void bxe_free_rx_sge(struct bxe_softc *, struct bxe_fastpath *,
	    uint16_t);
static __inline void bxe_free_rx_sge_range(struct bxe_softc *,
	    struct bxe_fastpath *, int);
static struct mbuf *bxe_alloc_mbuf(struct bxe_fastpath *, int);
static int  bxe_map_mbuf(struct bxe_fastpath *, struct mbuf *, bus_dma_tag_t,
	    bus_dmamap_t, bus_dma_segment_t *);
static struct mbuf *bxe_alloc_tpa_mbuf(struct bxe_fastpath *, int, int);
static void bxe_alloc_mutexes(struct bxe_softc *);
static void bxe_free_mutexes(struct bxe_softc *);
static int  bxe_alloc_rx_sge(struct bxe_softc *, struct bxe_fastpath *,
	    uint16_t);
static void bxe_init_rx_chains(struct bxe_softc *);
static void bxe_init_tx_chains(struct bxe_softc *);
static void bxe_free_rx_chains(struct bxe_softc *);
static void bxe_free_tx_chains(struct bxe_softc *);
static void bxe_init_sp_ring(struct bxe_softc *);
static void bxe_init_context(struct bxe_softc *);
static void bxe_init_ind_table(struct bxe_softc *);
static void bxe_set_client_config(struct bxe_softc *);
static void bxe_set_storm_rx_mode(struct bxe_softc *);
static void bxe_init_internal_common(struct bxe_softc *);
static void bxe_init_internal_port(struct bxe_softc *);

static void bxe_init_internal_func(struct bxe_softc *);
static void bxe_init_internal(struct bxe_softc *, uint32_t);
static void bxe_init_nic(struct bxe_softc *, uint32_t);
static int  bxe_gunzip_init(struct bxe_softc *);
static void bxe_lb_pckt(struct bxe_softc *);
static int  bxe_int_mem_test(struct bxe_softc *);
static void bxe_enable_blocks_attention (struct bxe_softc *);

static void bxe_init_pxp(struct bxe_softc *);
static int  bxe_init_common(struct bxe_softc *);
static int  bxe_init_port(struct bxe_softc *);
static void bxe_ilt_wr(struct bxe_softc *, uint32_t, bus_addr_t);
static int  bxe_init_func(struct bxe_softc *);
static int  bxe_init_hw(struct bxe_softc *, uint32_t);
static int  bxe_fw_command(struct bxe_softc *, uint32_t);
static void bxe_dma_free(struct bxe_softc *);
static void bxe_dmamem_free(struct bxe_softc *, bus_dma_tag_t, caddr_t,
	    bus_dmamap_t);
static void bxe_dma_map_addr(void *, bus_dma_segment_t *, int, int);
static int  bxe_dma_alloc(device_t);
static int  bxe_dmamem_alloc(struct bxe_softc *, bus_dma_tag_t, bus_dmamap_t,
	    void *, uint32_t, bus_addr_t *);
static void bxe_set_mac_addr_e1(struct bxe_softc *, int);
static void bxe_set_mac_addr_e1h(struct bxe_softc *, int);
static void bxe_set_rx_mode(struct bxe_softc *);
static void bxe_reset_func(struct bxe_softc *);
static void bxe_reset_port(struct bxe_softc *);
static void bxe_reset_common(struct bxe_softc *);
static void bxe_reset_chip(struct bxe_softc *, uint32_t);
static int  bxe_ifmedia_upd(struct ifnet *);
static void bxe_ifmedia_status(struct ifnet *, struct ifmediareq *);
static __inline void bxe_update_last_max_sge(struct bxe_fastpath *, uint16_t);
static void bxe_update_sge_prod(struct bxe_fastpath *,
	    struct eth_fast_path_rx_cqe *);
static void bxe_tpa_start(struct bxe_fastpath *, uint16_t, uint16_t, uint16_t);
static int  bxe_fill_frag_mbuf(struct bxe_softc *, struct bxe_fastpath *,
	    struct mbuf *, struct eth_fast_path_rx_cqe *, uint16_t);
static void bxe_tpa_stop(struct bxe_softc *, struct bxe_fastpath *, uint16_t,
	    int, int, union eth_rx_cqe *, uint16_t);
static void bxe_rxeof(struct bxe_fastpath *);
static void bxe_txeof(struct bxe_fastpath *);
static int  bxe_get_buf(struct bxe_fastpath *, struct mbuf *, uint16_t);
static int  bxe_watchdog(struct bxe_fastpath *fp);
static int  bxe_change_mtu(struct bxe_softc *, int);
static void bxe_tick(void *);
static void bxe_add_sysctls(struct bxe_softc *);
static void bxe_gunzip_end(struct bxe_softc *);

static void bxe_write_dmae_phys_len(struct bxe_softc *, bus_addr_t, uint32_t,
	    uint32_t);

void bxe_write_dmae(struct bxe_softc *, bus_addr_t, uint32_t, uint32_t);
void bxe_read_dmae(struct bxe_softc *, uint32_t, uint32_t);
int  bxe_set_gpio(struct bxe_softc *, int, uint32_t, uint8_t);
int  bxe_get_gpio(struct bxe_softc *, int, uint8_t);
int  bxe_set_spio(struct bxe_softc *, int, uint32_t);
int  bxe_set_gpio_int(struct bxe_softc *, int, uint32_t, uint8_t);

/*
 * BXE Debug Data Structure Dump Routines
 */

#ifdef BXE_DEBUG
static int bxe_sysctl_driver_state(SYSCTL_HANDLER_ARGS);
static int bxe_sysctl_hw_state(SYSCTL_HANDLER_ARGS);
static int bxe_sysctl_dump_fw(SYSCTL_HANDLER_ARGS);
static int bxe_sysctl_dump_rx_cq_chain(SYSCTL_HANDLER_ARGS);
static int bxe_sysctl_dump_rx_bd_chain(SYSCTL_HANDLER_ARGS);
static int bxe_sysctl_dump_tx_chain(SYSCTL_HANDLER_ARGS);
static int bxe_sysctl_reg_read(SYSCTL_HANDLER_ARGS);
static int bxe_sysctl_breakpoint(SYSCTL_HANDLER_ARGS);
static void bxe_validate_rx_packet(struct bxe_fastpath *, uint16_t,
	    union eth_rx_cqe *, struct mbuf *);
static void bxe_grcdump(struct bxe_softc *, int);
static void bxe_dump_enet(struct bxe_softc *,struct mbuf *);
static void bxe_dump_mbuf (struct bxe_softc *, struct mbuf *);
static void bxe_dump_tx_mbuf_chain(struct bxe_softc *, int, int);
static void bxe_dump_rx_mbuf_chain(struct bxe_softc *, int, int);
static void bxe_dump_tx_parsing_bd(struct bxe_fastpath *,int,
	    struct eth_tx_parse_bd *);
static void bxe_dump_txbd(struct bxe_fastpath *, int,
	    union eth_tx_bd_types *);
static void bxe_dump_rxbd(struct bxe_fastpath *, int,
	    struct eth_rx_bd *);
static void bxe_dump_cqe(struct bxe_fastpath *, int, union eth_rx_cqe *);
static void bxe_dump_tx_chain(struct bxe_fastpath *, int, int);
static void bxe_dump_rx_cq_chain(struct bxe_fastpath *, int, int);
static void bxe_dump_rx_bd_chain(struct bxe_fastpath *, int, int);
static void bxe_dump_status_block(struct bxe_softc *);
static void bxe_dump_stats_block(struct bxe_softc *);
static void bxe_dump_fp_state(struct bxe_fastpath *);
static void bxe_dump_port_state_locked(struct bxe_softc *);
static void bxe_dump_link_vars_state_locked(struct bxe_softc *);
static void bxe_dump_link_params_state_locked(struct bxe_softc *);
static void bxe_dump_driver_state(struct bxe_softc *);
static void bxe_dump_hw_state(struct bxe_softc *);
static void bxe_dump_fw(struct bxe_softc *);
static void bxe_decode_mb_msgs(struct bxe_softc *, uint32_t, uint32_t);
static void bxe_decode_ramrod_cmd(struct bxe_softc *, int);
static void bxe_breakpoint(struct bxe_softc *);
#endif


#define	BXE_DRIVER_VERSION	"1.5.52"

static void bxe_init_e1_firmware(struct bxe_softc *sc);
static void bxe_init_e1h_firmware(struct bxe_softc *sc);

/*
 * FreeBSD device dispatch table.
 */
static device_method_t bxe_methods[] = {
	/* Device interface (device_if.h) */
	DEVMETHOD(device_probe,		bxe_probe),
	DEVMETHOD(device_attach,	bxe_attach),
	DEVMETHOD(device_detach,	bxe_detach),
	DEVMETHOD(device_shutdown,	bxe_shutdown),

	/* Bus interface (bus_if.h) */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),

	KOBJMETHOD_END
};


static driver_t bxe_driver = {
	"bxe",
	bxe_methods,
	sizeof(struct bxe_softc)
};

static devclass_t bxe_devclass;

MODULE_DEPEND(bxe, pci, 1, 1, 1);
MODULE_DEPEND(bxe, ether, 1, 1, 1);
DRIVER_MODULE(bxe, pci, bxe_driver, bxe_devclass, 0, 0);

/*
 * Tunable device values
 */
SYSCTL_NODE(_hw, OID_AUTO, bxe, CTLFLAG_RD, 0, "bxe driver parameters");
/* Allowable values are TRUE (1) or FALSE (0). */

static int bxe_stats_enable = FALSE;
TUNABLE_INT("hw.bxe.stats_enable", &bxe_stats_enable);
SYSCTL_UINT(_hw_bxe, OID_AUTO, stats_enable, CTLFLAG_RDTUN, &bxe_stats_enable,
    0, "stats Enable/Disable");

static int bxe_dcc_enable = FALSE;
TUNABLE_INT("hw.bxe.dcc_enable", &bxe_dcc_enable);
SYSCTL_UINT(_hw_bxe, OID_AUTO, dcc_enable, CTLFLAG_RDTUN, &bxe_dcc_enable,
    0, "dcc Enable/Disable");

/* Allowable values are TRUE (1) or FALSE (0). */
static int bxe_tso_enable = TRUE;
TUNABLE_INT("hw.bxe.tso_enable", &bxe_tso_enable);
SYSCTL_UINT(_hw_bxe, OID_AUTO, tso_enable, CTLFLAG_RDTUN, &bxe_tso_enable,
    0, "TSO Enable/Disable");

/* Allowable values are 0 (IRQ), 1 (MSI/IRQ), and 2 (MSI-X/MSI/IRQ). */
static int bxe_int_mode = 2;
TUNABLE_INT("hw.bxe.int_mode", &bxe_int_mode);
SYSCTL_UINT(_hw_bxe, OID_AUTO, int_mode, CTLFLAG_RDTUN, &bxe_int_mode,
    0, "Interrupt (MSI-X|MSI|INTx) mode");

/*
 * Specifies whether the driver should disable Transparent Packet
 * Aggregation (TPA, also known as LRO).  By default TPA is enabled.
 *
 * Allowable values are TRUE (1) or FALSE (0).
 */
static int bxe_tpa_enable = FALSE;
TUNABLE_INT("hw.bxe.tpa_enable", &bxe_tpa_enable);
SYSCTL_UINT(_hw_bxe, OID_AUTO, tpa_enable, CTLFLAG_RDTUN, &bxe_tpa_enable,
    0, "TPA Enable/Disable");


/*
 * Specifies the number of queues that will be used when a multi-queue
 * RSS mode is selected  using bxe_multi_mode below.
 *
 * Allowable values are 0 (Auto) or 1 to MAX_CONTEXT (fixed queue number).
 */
static int bxe_queue_count = 0;
TUNABLE_INT("hw.bxe.queue_count", &bxe_queue_count);
SYSCTL_UINT(_hw_bxe, OID_AUTO, queue_count, CTLFLAG_RDTUN, &bxe_queue_count,
    0, "Multi-Queue queue count");

/*
 * ETH_RSS_MODE_DISABLED (0)
 * Disables all multi-queue/packet sorting algorithms.  Each
 * received frame is routed to the same receive queue.
 *
 * ETH_RSS_MODE_REGULAR (1)
 * The default mode which assigns incoming frames to receive
 * queues according to RSS (i.e a 2-tuple match on the source/
 * destination IP address or a 4-tuple match on the source/
 * destination IP address and the source/destination TCP port).
 *
 */
static int bxe_multi_mode = ETH_RSS_MODE_REGULAR;
TUNABLE_INT("hw.bxe.multi_mode", &bxe_multi_mode);
SYSCTL_UINT(_hw_bxe, OID_AUTO, multi_mode, CTLFLAG_RDTUN, &bxe_multi_mode,
    0, "Multi-Queue Mode");

/*
 * Host interrupt coalescing is controller by these values.
 * The first frame always causes an interrupt but subsequent
 * frames are coalesced until the RX/TX ticks timer value
 * expires and another interrupt occurs.  (Ticks are measured
 * in microseconds.)
 */
static uint32_t bxe_rx_ticks = 25;
TUNABLE_INT("hw.bxe.rx_ticks", &bxe_rx_ticks);
SYSCTL_UINT(_hw_bxe, OID_AUTO, rx_ticks, CTLFLAG_RDTUN, &bxe_rx_ticks,
    0, "Receive ticks");

static uint32_t bxe_tx_ticks = 50;
TUNABLE_INT("hw.bxe.tx_ticks", &bxe_tx_ticks);
SYSCTL_UINT(_hw_bxe, OID_AUTO, tx_ticks, CTLFLAG_RDTUN, &bxe_tx_ticks,
    0, "Transmit ticks");

/*
 * Allows the PCIe maximum read request size value to be manually
 * set during initialization rather than automatically determined
 * by the driver.
 *
 * Allowable values are:
 * -1 (Auto), 0 (128B), 1 (256B), 2 (512B), 3 (1KB)
 */
static int bxe_mrrs = -1;
TUNABLE_INT("hw.bxe.mrrs", &bxe_mrrs);
SYSCTL_UINT(_hw_bxe, OID_AUTO, mrrs, CTLFLAG_RDTUN, &bxe_mrrs,
    0, "PCIe maximum read request size.");

#if 0
/*
 * Allows setting the maximum number of received frames to process
 * during an interrupt.
 *
 * Allowable values are:
 * -1 (Unlimited), 0 (None), otherwise specifies the number of RX frames.
 */
static int bxe_rx_limit = -1;
TUNABLE_INT("hw.bxe.rx_limit", &bxe_rx_limit);
SYSCTL_UINT(_hw_bxe, OID_AUTO, rx_limit, CTLFLAG_RDTUN, &bxe_rx_limit,
    0, "Maximum received frames processed during an interrupt.");

/*
 * Allows setting the maximum number of transmit frames to process
 * during an interrupt.
 *
 * Allowable values are:
 * -1 (Unlimited), 0 (None), otherwise specifies the number of TX frames.
 */
static int bxe_tx_limit = -1;
TUNABLE_INT("hw.bxe.tx_limit", &bxe_tx_limit);
SYSCTL_UINT(_hw_bxe, OID_AUTO, tx_limit, CTLFLAG_RDTUN, &bxe_tx_limit,
	0, "Maximum transmit frames processed during an interrupt.");
#endif

/*
 * Global variables
 */

/* 0 is common, 1 is port 0, 2 is port 1. */
static int load_count[3];

/* Tracks whether MCP firmware is running. */
static int nomcp;

#ifdef BXE_DEBUG
/*
 * A debug version of the 32 bit OS register write function to
 * capture/display values written to the controller.
 *
 * Returns:
 *   None.
 */
void
bxe_reg_write32(struct bxe_softc *sc, bus_size_t offset, uint32_t val)
{

	if ((offset % 4) != 0) {
		DBPRINT(sc, BXE_WARN,
		    "%s(): Warning! Unaligned write to 0x%jX!\n", __FUNCTION__,
		    (uintmax_t)offset);
	}

	DBPRINT(sc, BXE_INSANE, "%s(): offset = 0x%jX, val = 0x%08X\n",
	    __FUNCTION__, (uintmax_t)offset, val);

	bus_space_write_4(sc->bxe_btag, sc->bxe_bhandle, offset, val);
}

/*
 * A debug version of the 16 bit OS register write function to
 * capture/display values written to the controller.
 *
 * Returns:
 *   None.
 */
static void
bxe_reg_write16(struct bxe_softc *sc, bus_size_t offset, uint16_t val)
{

	if ((offset % 2) != 0) {
		DBPRINT(sc, BXE_WARN,
		    "%s(): Warning! Unaligned write to 0x%jX!\n", __FUNCTION__,
		    (uintmax_t)offset);
	}

	DBPRINT(sc, BXE_INSANE, "%s(): offset = 0x%jX, val = 0x%04X\n",
	    __FUNCTION__, (uintmax_t)offset, val);

	bus_space_write_2(sc->bxe_btag, sc->bxe_bhandle, offset, val);
}

/*
 * A debug version of the 8 bit OS register write function to
 * capture/display values written to the controller.
 *
 * Returns:
 *   None.
 */
static void
bxe_reg_write8(struct bxe_softc *sc, bus_size_t offset, uint8_t val)
{

	DBPRINT(sc, BXE_INSANE, "%s(): offset = 0x%jX, val = 0x%02X\n",
	    __FUNCTION__, (uintmax_t)offset, val);

	bus_space_write_1(sc->bxe_btag, sc->bxe_bhandle, offset, val);
}

/*
 * A debug version of the 32 bit OS register read function to
 * capture/display values read from the controller.
 *
 * Returns:
 *   32bit value read.
 */
uint32_t
bxe_reg_read32(struct bxe_softc *sc, bus_size_t offset)
{
	uint32_t val;

	if ((offset % 4) != 0) {
		DBPRINT(sc, BXE_WARN,
		    "%s(): Warning! Unaligned read from 0x%jX!\n",
		    __FUNCTION__, (uintmax_t)offset);
	}

	val = bus_space_read_4(sc->bxe_btag, sc->bxe_bhandle, offset);

	DBPRINT(sc, BXE_INSANE, "%s(): offset = 0x%jX, val = 0x%08X\n",
		__FUNCTION__, (uintmax_t)offset, val);

	return (val);
}

/*
 * A debug version of the 16 bit OS register read function to
 * capture/display values read from the controller.
 *
 * Returns:
 *   16bit value read.
 */
static uint16_t
bxe_reg_read16(struct bxe_softc *sc, bus_size_t offset)
{
	uint16_t val;

	if ((offset % 2) != 0) {
		DBPRINT(sc, BXE_WARN,
		    "%s(): Warning! Unaligned read from 0x%jX!\n",
		    __FUNCTION__, (uintmax_t)offset);
	}

	val = bus_space_read_2(sc->bxe_btag, sc->bxe_bhandle, offset);

	DBPRINT(sc, BXE_INSANE, "%s(): offset = 0x%jX, val = 0x%08X\n",
	    __FUNCTION__, (uintmax_t)offset, val);

	return (val);
}


/*
 * A debug version of the 8 bit OS register write function to
 * capture/display values written to the controller.
 *
 * Returns:
 *   8bit value read.
 */
static uint8_t
bxe_reg_read8(struct bxe_softc *sc, bus_size_t offset)
{
	uint8_t val = bus_space_read_1(sc->bxe_btag, sc->bxe_bhandle, offset);

	DBPRINT(sc, BXE_INSANE, "%s(): offset = 0x%jX, val = 0x%02X\n",
		__FUNCTION__, (uintmax_t)offset, val);

	return(val);
}
#endif

static void
bxe_read_mf_cfg(struct bxe_softc *sc)
{
	int func, vn;

	for (vn = VN_0; vn < E1HVN_MAX; vn++) {
		func = 2 * vn + BP_PORT(sc);
		sc->mf_config[vn] =
		    SHMEM_RD(sc,mf_cfg.func_mf_config[func].config);
	}
}


static void
bxe_e1h_disable(struct bxe_softc *sc)
{
	int port;

	port = BP_PORT(sc);
	REG_WR(sc, NIG_REG_LLH0_FUNC_EN + port * 8, 0);
	sc->bxe_ifp->if_drv_flags = 0;
}

static void
bxe_e1h_enable(struct bxe_softc *sc)
{
	int port;

	port = BP_PORT(sc);
	REG_WR(sc, NIG_REG_LLH0_FUNC_EN + port * 8, 1);
	sc->bxe_ifp->if_drv_flags = IFF_DRV_RUNNING;
}

/*
 * Calculates the sum of vn_min_rates.
 * It's needed for further normalizing of the min_rates.
 * Returns:
 *   sum of vn_min_rates.
 *     or
 *   0 - if all the min_rates are 0. In the later case fainess
 * 	 algorithm should be deactivated. If not all min_rates are
 * 	 zero then those that are zeroes will be set to 1.
 */
static void
bxe_calc_vn_wsum(struct bxe_softc *sc)
{
	uint32_t vn_cfg, vn_min_rate;
	int all_zero, vn;

	DBENTER(BXE_VERBOSE_LOAD);

	all_zero = 1;
	sc->vn_wsum = 0;
	for (vn = VN_0; vn < E1HVN_MAX; vn++) {
		vn_cfg = sc->mf_config[vn];
		vn_min_rate = ((vn_cfg & FUNC_MF_CFG_MIN_BW_MASK) >>
		    FUNC_MF_CFG_MIN_BW_SHIFT) * 100;
		/* Skip hidden vns */
		if (vn_cfg & FUNC_MF_CFG_FUNC_HIDE)
			continue;
		/* If min rate is zero - set it to 1. */
		if (!vn_min_rate)
			vn_min_rate = DEF_MIN_RATE;
		else
			all_zero = 0;

		sc->vn_wsum += vn_min_rate;
	}

	/* ... only if all min rates are zeros - disable fairness */
	if (all_zero)
		sc->cmng.flags.cmng_enables &= ~CMNG_FLAGS_PER_PORT_FAIRNESS_VN;
	else
		sc->cmng.flags.cmng_enables |= CMNG_FLAGS_PER_PORT_FAIRNESS_VN;

	DBEXIT(BXE_VERBOSE_LOAD);
}

/*
 *
 * Returns:
 *   None.
 */
static void
bxe_init_vn_minmax(struct bxe_softc *sc, int vn)
{
	struct rate_shaping_vars_per_vn m_rs_vn;
	struct fairness_vars_per_vn m_fair_vn;
	uint32_t vn_cfg;
	uint16_t vn_min_rate, vn_max_rate;
	int func, i;

	vn_cfg = sc->mf_config[vn];
	func = 2 * vn + BP_PORT(sc);

	DBENTER(BXE_VERBOSE_LOAD);

	/* If function is hidden - set min and max to zeroes. */
	if (vn_cfg & FUNC_MF_CFG_FUNC_HIDE) {
		vn_min_rate = 0;
		vn_max_rate = 0;
	} else {
		vn_min_rate = ((vn_cfg & FUNC_MF_CFG_MIN_BW_MASK) >>
		    FUNC_MF_CFG_MIN_BW_SHIFT) * 100;
		/*
		 * If fairness is enabled (i.e. not all min rates are zero),
		 * and if the current min rate is zero, set it to 1.
		 * This is a requirement of the algorithm.
		 */
		if (sc->vn_wsum && (vn_min_rate == 0))
			vn_min_rate = DEF_MIN_RATE;

		vn_max_rate = ((vn_cfg & FUNC_MF_CFG_MAX_BW_MASK) >>
		    FUNC_MF_CFG_MAX_BW_SHIFT) * 100;

		if (vn_max_rate == 0)
			return;
	}
	DBPRINT(sc, BXE_INFO_LOAD,
	    "%s(): func %d: vn_min_rate = %d, vn_max_rate = %d, wsum = %d.\n",
	    __FUNCTION__, func, vn_min_rate, vn_max_rate, sc->vn_wsum);

	memset(&m_rs_vn, 0, sizeof(struct rate_shaping_vars_per_vn));
	memset(&m_fair_vn, 0, sizeof(struct fairness_vars_per_vn));

	/* Global VNIC counter - maximal Mbps for this VNIC. */
	m_rs_vn.vn_counter.rate = vn_max_rate;

	/* Quota - number of bytes transmitted in this period. */
	m_rs_vn.vn_counter.quota =
	    (vn_max_rate * RS_PERIODIC_TIMEOUT_USEC) / 8;

	if (sc->vn_wsum) {
		/*
		 * Credit for each period of the fairness algorithm.  The
		 * number of bytes in T_FAIR (the VNIC shares the port rate).
		 * vn_wsum should not be larger than 10000, thus
		 * T_FAIR_COEF / (8 * vn_wsum) will always be grater than zero.
		 */
		m_fair_vn.vn_credit_delta =
		    max((uint32_t)(vn_min_rate * (T_FAIR_COEF /
		    (8 * sc->vn_wsum))),
		    (uint32_t)(sc->cmng.fair_vars.fair_threshold * 2));
	}

	func = BP_FUNC(sc);

	/* Store it to internal memory */
	for (i = 0; i < sizeof(struct rate_shaping_vars_per_vn) / 4; i++)
		REG_WR(sc, BAR_XSTORM_INTMEM +
		    XSTORM_RATE_SHAPING_PER_VN_VARS_OFFSET(func) + (i * 4),
		    ((uint32_t *)(&m_rs_vn))[i]);

	for (i = 0; i < sizeof(struct fairness_vars_per_vn) / 4; i++)
		REG_WR(sc, BAR_XSTORM_INTMEM +
		    XSTORM_FAIRNESS_PER_VN_VARS_OFFSET(func) + (i * 4),
		    ((uint32_t *)(&m_fair_vn))[i]);

	DBEXIT(BXE_VERBOSE_LOAD);
}

static void
bxe_congestionmgmt(struct bxe_softc *sc, uint8_t readshm)
{
	int vn;

	DBENTER(BXE_VERBOSE_LOAD);

	/* Read mf conf from shmem. */
	if (readshm)
		bxe_read_mf_cfg(sc);

	/* Init rate shaping and fairness contexts */
	bxe_init_port_minmax(sc);

	/* vn_weight_sum and enable fairness if not 0 */
	bxe_calc_vn_wsum(sc);

	/* calculate and set min-max rate for each vn */
	for (vn = 0; vn < E1HVN_MAX; vn++)
		bxe_init_vn_minmax(sc, vn);

	/* Always enable rate shaping and fairness. */
	sc->cmng.flags.cmng_enables |= CMNG_FLAGS_PER_PORT_RATE_SHAPING_VN;

	DBPRINT(sc, BXE_VERBOSE_LOAD,
	    "%s(): Rate shaping set\n", __FUNCTION__);

	if (!sc->vn_wsum)
		DBPRINT(sc, BXE_INFO_LOAD, "%s(): All MIN values "
		    "are zeroes, fairness is disabled\n", __FUNCTION__);

	DBEXIT(BXE_VERBOSE_LOAD);
}

static void
bxe_dcc_event(struct bxe_softc *sc, uint32_t dcc_event)
{
	int i, port;

	DBENTER(BXE_VERBOSE_LOAD);

	if (dcc_event & DRV_STATUS_DCC_DISABLE_ENABLE_PF) {
		if (sc->mf_config[BP_E1HVN(sc)] & FUNC_MF_CFG_FUNC_DISABLED) {
			DBPRINT(sc, BXE_INFO_LOAD, "%s(): mf_cfg function "
			    "disabled\n", __FUNCTION__);
			sc->state = BXE_STATE_DISABLED;
			bxe_e1h_disable(sc);
		} else {
			DBPRINT(sc, BXE_INFO_LOAD, "%s(): mf_cfg function "
			    "enabled\n", __FUNCTION__);
			sc->state = BXE_STATE_OPEN;
			bxe_e1h_enable(sc);
		}
		dcc_event &= ~DRV_STATUS_DCC_DISABLE_ENABLE_PF;
	}
	if (dcc_event & DRV_STATUS_DCC_BANDWIDTH_ALLOCATION) {
		port = BP_PORT(sc);
		bxe_congestionmgmt(sc, TRUE);
		for (i = 0; i < sizeof(struct cmng_struct_per_port) / 4; i++)
			REG_WR(sc, BAR_XSTORM_INTMEM +
			       XSTORM_CMNG_PER_PORT_VARS_OFFSET(port) + i*4,
			       ((uint32_t *)(&sc->cmng))[i]);
		dcc_event &= ~DRV_STATUS_DCC_BANDWIDTH_ALLOCATION;
	}

	/* Report results to MCP */
	if (dcc_event)
		bxe_fw_command(sc, DRV_MSG_CODE_DCC_FAILURE);
	else
		bxe_fw_command(sc, DRV_MSG_CODE_DCC_OK);

	DBEXIT(BXE_VERBOSE_LOAD);
}

/*
 * Device probe function.
 *
 * Compares the device to the driver's list of supported devices and
 * reports back to the OS whether this is the right driver for the device.
 *
 * Returns:
 *   BUS_PROBE_DEFAULT on success, positive value on failure.
 */
static int
bxe_probe(device_t dev)
{
	struct bxe_softc *sc;
	struct bxe_type *t;
	char *descbuf;
	uint16_t did, sdid, svid, vid;

	sc = device_get_softc(dev);
	sc->dev = dev;
	t = bxe_devs;

	/* Get the data for the device to be probed. */
	vid  = pci_get_vendor(dev);
	did  = pci_get_device(dev);
	svid = pci_get_subvendor(dev);
	sdid = pci_get_subdevice(dev);

	DBPRINT(sc, BXE_VERBOSE_LOAD,
	    "%s(); VID = 0x%04X, DID = 0x%04X, SVID = 0x%04X, "
	    "SDID = 0x%04X\n", __FUNCTION__, vid, did, svid, sdid);

	/* Look through the list of known devices for a match. */
	while (t->bxe_name != NULL) {
		if ((vid == t->bxe_vid) && (did == t->bxe_did) &&
		    ((svid == t->bxe_svid) || (t->bxe_svid == PCI_ANY_ID)) &&
		    ((sdid == t->bxe_sdid) || (t->bxe_sdid == PCI_ANY_ID))) {
			descbuf = malloc(BXE_DEVDESC_MAX, M_TEMP, M_NOWAIT);
			if (descbuf == NULL)
				return (ENOMEM);

			/* Print out the device identity. */
			snprintf(descbuf, BXE_DEVDESC_MAX,
			    "%s (%c%d) BXE v:%s\n", t->bxe_name,
			    (((pci_read_config(dev, PCIR_REVID, 4) &
			    0xf0) >> 4) + 'A'),
			    (pci_read_config(dev, PCIR_REVID, 4) & 0xf),
			    BXE_DRIVER_VERSION);

			device_set_desc_copy(dev, descbuf);
			free(descbuf, M_TEMP);
			return (BUS_PROBE_DEFAULT);
		}
		t++;
	}

	return (ENXIO);
}

/*
 * Prints useful adapter info.
 *
 * Returns:
 *   None.
 */
static void
bxe_print_adapter_info(struct bxe_softc *sc)
{
	int i = 0;

	DBENTER(BXE_EXTREME_LOAD);

	/* Hardware chip info. */
	BXE_PRINTF("ASIC (0x%08X); ", sc->common.chip_id);
	printf("Rev (%c%d); ", (CHIP_REV(sc) >> 12) + 'A',
	       (CHIP_METAL(sc) >> 4));

	/* Bus info. */
	printf("Bus (PCIe x%d, ", sc->pcie_link_width);
	switch (sc->pcie_link_speed) {
	case 1:
		printf("2.5Gbps");
		break;
	case 2:
		printf("5Gbps");
		break;
	default:
		printf("Unknown link speed");
	}

	/* Device features. */
	printf("); Flags (");

	/* Miscellaneous flags. */
	if (sc->bxe_flags & BXE_USING_MSI_FLAG)
		printf("MSI");

	if (sc->bxe_flags & BXE_USING_MSIX_FLAG) {
		if (i > 0) printf("|");
		printf("MSI-X"); i++;
	}

	if (sc->bxe_flags & BXE_SAFC_TX_FLAG) {
		if (i > 0) printf("|");
		printf("SAFC"); i++;
	}

	if (TPA_ENABLED(sc)) {
		if (i > 0) printf("|");
		printf("TPA"); i++;
	}

	printf("); Queues (");
	switch (sc->multi_mode) {
	case ETH_RSS_MODE_DISABLED:
		printf("None");
		break;
	case ETH_RSS_MODE_REGULAR:
		printf("RSS:%d", sc->num_queues);
		break;
	default:
		printf("Unknown");
		break;
	}

	/* Firmware versions and device features. */
	printf("); Firmware (%d.%d.%d); Bootcode (%d.%d.%d)\n",
	    BCM_5710_FW_MAJOR_VERSION,
	    BCM_5710_FW_MINOR_VERSION,
	    BCM_5710_FW_REVISION_VERSION,
	    (int)((sc->common.bc_ver & 0xff0000) >> 16),
	    (int)((sc->common.bc_ver & 0x00ff00) >> 8),
	    (int)((sc->common.bc_ver & 0x0000ff)));

	DBEXIT(BXE_EXTREME_LOAD);
}

/*
 * This function determines and allocates the appropriate
 * interrupt based on system capabilites and user request.
 *
 * The user may force a particular interrupt mode, specify
 * the number of receive queues, specify the method for
 * distribuitng received frames to receive queues, or use
 * the default settings which will automatically select the
 * best supported combination.  In addition, the OS may or
 * may not support certain combinations of these settings.
 * This routine attempts to reconcile the settings requested
 * by the user with the capabilites available from the system
 * to select the optimal combination of features.
 *
 * Returns:
 *   0 = Success, !0 = Failure.
 */
static int
bxe_interrupt_allocate(struct bxe_softc *sc)
{
	device_t dev;
	int i, rid, rc;
	int msi_count, msi_required, msi_allocated;
	int msix_count, msix_required, msix_allocated;

	rc = 0;
	dev = sc->dev;
	msi_count = 0;
	msi_required = 0;
	msi_allocated = 0;
	msix_count = 0;
	msix_required = 0;
	msix_allocated = 0;

	DBENTER(BXE_VERBOSE_LOAD | BXE_VERBOSE_INTR);

	/* Assume SAFC not enabled for TX. */
	sc->bxe_flags &= ~BXE_SAFC_TX_FLAG;

	/* Clear any previous priority queue mappings. */
	for (i = 0; i < BXE_MAX_PRIORITY; i++)
		sc->pri_map[i] = 0;

	/* Get the number of available MSI/MSI-X interrupts from the OS. */
	if (sc->int_mode > 0) {
		if (sc->bxe_cap_flags & BXE_MSIX_CAPABLE_FLAG)
			msix_count = pci_msix_count(dev);

		if (sc->bxe_cap_flags & BXE_MSI_CAPABLE_FLAG)
			msi_count = pci_msi_count(dev);

		DBPRINT(sc, (BXE_VERBOSE_LOAD | BXE_VERBOSE_INTR),
		    "%s(): %d MSI and %d MSI-X vectors available.\n",
		    __FUNCTION__, msi_count, msix_count);
	}

	/* Try allocating MSI-X interrupt resources. */
	if ((sc->bxe_cap_flags & BXE_MSIX_CAPABLE_FLAG) &&
	    (sc->int_mode > 1) && (msix_count > 0) &&
	    (msix_count >= sc->num_queues)) {
		/* Ask for the necessary number	of MSI-X vectors. */
		if (sc->num_queues == 1)
			msix_allocated = msix_required = 2;
		else
			msix_allocated = msix_required = sc->num_queues + 1;

		DBPRINT(sc, (BXE_VERBOSE_LOAD | BXE_VERBOSE_INTR),
		    "%s(): Requesting %d MSI-X vectors.\n",
		    __FUNCTION__, msix_required);

		/* BSD resource identifier */
		rid = 1;
		if (pci_alloc_msix(dev, &msix_allocated) == 0) {
			DBPRINT(sc, (BXE_VERBOSE_LOAD | BXE_VERBOSE_INTR),
		"%s(): Required/Allocated (%d/%d) MSI-X vector(s).\n",
			    __FUNCTION__, msix_required, msix_allocated);

			/* Make sure we got all the interrupts we asked for. */
			if (msix_allocated >= msix_required) {
				sc->msix_count = msix_required;
				sc->bxe_flags |= BXE_USING_MSIX_FLAG;
				msi_count = 0;

				/* Allocate the MSI-X vectors. */
				for (i = 0; i < msix_required; i++) {
					sc->bxe_msix_rid[i] = rid + i +
					    BP_L_ID(sc);
					sc->bxe_msix_res[i] =
					    bus_alloc_resource_any(dev,
					    SYS_RES_IRQ, &sc->bxe_msix_rid[i],
					    RF_ACTIVE);
					/* Report any IRQ allocation errors. */
					if (sc->bxe_msix_res[i] == NULL) {
						BXE_PRINTF(
				"%s(%d): Failed to map MSI-X[%d] vector!\n",
						    __FILE__, __LINE__, (3));
						rc = ENXIO;
						goto bxe_interrupt_allocate_exit;
					}
				}
			} else {

				DBPRINT(sc, BXE_WARN,
				    "%s(): MSI-X allocation failed!\n",
				    __FUNCTION__);

				/* Release any resources acquired. */
				pci_release_msi(dev);
				sc->bxe_flags &= ~BXE_USING_MSIX_FLAG;
				sc->msix_count = msix_count = 0;

				/* We'll try MSI next. */
				sc->int_mode = 1;
			}
		}
	}

	/* Try allocating MSI vector resources. */
	if ((sc->bxe_cap_flags & BXE_MSI_CAPABLE_FLAG) &&
	    (sc->int_mode > 0) && (msi_count > 0) &&
	    (msi_count >= sc->num_queues)) {
		/* Ask for the necessary number	of MSI vectors. */
		if (sc->num_queues == 1)
			msi_required = msi_allocated = 1;
		else
			msi_required = msi_allocated = BXE_MSI_VECTOR_COUNT;

		DBPRINT(sc, (BXE_VERBOSE_LOAD | BXE_VERBOSE_INTR),
		    "%s(): Requesting %d MSI vectors.\n", __FUNCTION__,
		    msi_required);

		rid = 1;
		if (pci_alloc_msi(dev, &msi_allocated) == 0) {
			DBPRINT(sc, (BXE_VERBOSE_LOAD | BXE_VERBOSE_INTR),
			    "%s(): Required/Allocated (%d/%d) MSI vector(s).\n",
			    __FUNCTION__, msi_required, msi_allocated);

			/*
			 * Make sure we got all the vectors we asked for.
			 * XXX
			 * FreeBSD always gives 8 even if we ask for less.
			 */
			if (msi_required >= msi_allocated) {
				sc->msi_count = msi_required;
				sc->bxe_flags |= BXE_USING_MSI_FLAG;
				/* Allocate the MSI vectors. */
				for (i = 0; i < msi_required; i++) {
					sc->bxe_msi_rid[i] = i + rid;
					sc->bxe_msi_res[i] =
					    bus_alloc_resource_any(dev,
					    SYS_RES_IRQ, &sc->bxe_msi_rid[i],
					    RF_ACTIVE);
					/* Report any IRQ allocation errors. */
					if (sc->bxe_msi_res[i] == NULL) {
						BXE_PRINTF(
				"%s(%d): Failed to map MSI vector (%d)!\n",
						    __FILE__, __LINE__, (i));
						rc = ENXIO;
						goto bxe_interrupt_allocate_exit;
					}
				}
			}
		} else {

			DBPRINT(sc, BXE_WARN, "%s(): MSI allocation failed!\n",
			    __FUNCTION__);

			/* Release any resources acquired. */
			pci_release_msi(dev);
			sc->bxe_flags &= ~BXE_USING_MSI_FLAG;
			sc->msi_count = msi_count = 0;

			/* We'll try INTx next. */
			sc->int_mode = 0;
		}
	}

	/* Try allocating INTx resources. */
	if (sc->int_mode == 0) {
		sc->num_queues = 1;
		sc->multi_mode = ETH_RSS_MODE_DISABLED;

		DBPRINT(sc, (BXE_VERBOSE_LOAD | BXE_VERBOSE_INTR),
		    "%s(): Requesting legacy INTx interrupt.\n",
		    __FUNCTION__);

		rid = 0;
		sc->bxe_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
		    RF_SHAREABLE | RF_ACTIVE);
		/* Report any IRQ allocation errors. */
		if (sc->bxe_irq_res == NULL) {
			BXE_PRINTF("%s(%d): PCI map interrupt failed!\n",
			    __FILE__, __LINE__);
			rc = ENXIO;
			goto bxe_interrupt_allocate_exit;
		}
		sc->bxe_irq_rid = rid;
	}

	DBPRINT(sc, (BXE_VERBOSE_LOAD | BXE_VERBOSE_INTR),
	    "%s(): Actual: int_mode = %d, multi_mode = %d, num_queues = %d\n",
	    __FUNCTION__, sc->int_mode, sc->multi_mode, sc->num_queues);

bxe_interrupt_allocate_exit:
	DBEXIT(BXE_VERBOSE_LOAD | BXE_VERBOSE_INTR);
	return (rc);
}

static void
bxe_interrupt_detach(struct bxe_softc *sc)
{
	device_t dev;
	int i;

	dev = sc->dev;
	DBENTER(BXE_VERBOSE_RESET | BXE_VERBOSE_UNLOAD);
	/* Release interrupt resources. */
	if ((sc->bxe_flags & BXE_USING_MSIX_FLAG) && sc->msix_count) {
		for (i = 0; i < sc->msix_count; i++) {
			if (sc->bxe_msix_tag[i] && sc->bxe_msix_res[i])
				bus_teardown_intr(dev, sc->bxe_msix_res[i],
				    sc->bxe_msix_tag[i]);
		}
	} else if ((sc->bxe_flags & BXE_USING_MSI_FLAG) && sc->msi_count) {
		for (i = 0; i < sc->msi_count; i++) {
			if (sc->bxe_msi_tag[i] && sc->bxe_msi_res[i])
				bus_teardown_intr(dev, sc->bxe_msi_res[i],
				    sc->bxe_msi_tag[i]);
		}
	} else {
		if (sc->bxe_irq_tag != NULL)
			bus_teardown_intr(dev, sc->bxe_irq_res,
			    sc->bxe_irq_tag);
	}
}

/*
 * This function enables interrupts and attachs to the ISR.
 *
 * When using multiple MSI/MSI-X vectors the first vector
 * is used for slowpath operations while all remaining
 * vectors are used for fastpath operations.  If only a
 * single MSI/MSI-X vector is used (SINGLE_ISR) then the
 * ISR must look for both slowpath and fastpath completions.
 *
 * Returns:
 *   0 = Success, !0 = Failure.
 */
static int
bxe_interrupt_attach(struct bxe_softc *sc)
{
	struct bxe_fastpath *fp;
	int i, rc;

	DBENTER(BXE_VERBOSE_LOAD | BXE_VERBOSE_INTR);

	rc = 0;

#ifdef BXE_TASK
	/* Setup the slowpath deferred task queue. */
	TASK_INIT(&sc->task, 0, bxe_task_sp, sc);
	sc->tq = taskqueue_create_fast("bxe_spq", M_NOWAIT,
		taskqueue_thread_enqueue, &sc->tq);
	taskqueue_start_threads(&sc->tq, 1, PI_NET, "%s spq",
		device_get_nameunit(sc->dev));
#endif

	/* Setup interrupt handlers. */
	if (sc->bxe_flags & BXE_USING_MSIX_FLAG) {
		DBPRINT(sc, (BXE_VERBOSE_LOAD | BXE_VERBOSE_INTR),
		    "%s(): Enabling slowpath MSI-X[0] vector.\n",__FUNCTION__);
		/*
		 * Setup the interrupt handler.  Note that we pass the
		 * driver instance to the interrupt handler for the
		 * slowpath.
		 */
		rc = bus_setup_intr(sc->dev,
				    sc->bxe_msix_res[0],
				    INTR_TYPE_NET | INTR_MPSAFE,
				    NULL,
				    bxe_intr_sp,
				    sc,
				    &sc->bxe_msix_tag[0]);

		if (rc) {
			BXE_PRINTF(
			    "%s(%d): Failed to allocate MSI-X[0] vector!\n",
			    __FILE__, __LINE__);
			goto bxe_interrupt_attach_exit;
		}

		/* Now initialize the fastpath vectors. */
		for (i = 0; i < (sc->num_queues); i++) {
			fp = &sc->fp[i];
			DBPRINT(sc,
				(BXE_VERBOSE_LOAD | BXE_VERBOSE_INTR),
				"%s(): Enabling MSI-X[%d] vector.\n",
				__FUNCTION__, i + 1);
			/*
			 * Setup the interrupt handler. Note that we pass the
			 * fastpath context to the interrupt handler in this
			 * case. Also the first msix_res was used by the sp.
			 */
			rc = bus_setup_intr(sc->dev,
					    sc->bxe_msix_res[i + 1],
					    INTR_TYPE_NET | INTR_MPSAFE,
					    NULL,
					    bxe_intr_fp,
					    fp,
					    &sc->bxe_msix_tag[i + 1]
					    );

			if (rc) {
			    BXE_PRINTF(
			    "%s(%d): Failed to allocate MSI-X[%d] vector!\n",
			    __FILE__, __LINE__, (i + 1));
			    goto bxe_interrupt_attach_exit;
			}
#ifdef BXE_TASK
			TASK_INIT(&fp->task, 0, bxe_task_fp, fp);
			fp->tq = taskqueue_create_fast("bxe_fpq", M_NOWAIT,
				taskqueue_thread_enqueue, &fp->tq);
			taskqueue_start_threads(&fp->tq, 1, PI_NET, "%s fpq",
				device_get_nameunit(sc->dev));
#endif
			fp->state = BXE_FP_STATE_IRQ;
		}
	} else if (sc->bxe_flags & BXE_USING_MSI_FLAG) {
		DBPRINT(sc, (BXE_VERBOSE_LOAD | BXE_VERBOSE_INTR),
			"%s(): Enabling slowpath MSI[0] vector.\n",
			__FUNCTION__);
		/*
		 * Setup the interrupt handler. Note that we pass the driver
		 * instance to the interrupt handler for the slowpath.
		 */
		rc = bus_setup_intr(sc->dev,sc->bxe_msi_res[0],
				    INTR_TYPE_NET | INTR_MPSAFE,
				    NULL,
				    bxe_intr_sp,
				    sc,
				    &sc->bxe_msi_tag[0]
				    );

		if (rc) {
			BXE_PRINTF(
			    "%s(%d): Failed to allocate MSI[0] vector!\n",
			    __FILE__, __LINE__);
			goto bxe_interrupt_attach_exit;
		}

		/* Now initialize the fastpath vectors. */
		for (i = 0; i < (sc->num_queues); i++) {
			fp = &sc->fp[i];
			DBPRINT(sc,
				(BXE_VERBOSE_LOAD | BXE_VERBOSE_INTR),
				"%s(): Enabling MSI[%d] vector.\n",
				__FUNCTION__, i + 1);
			/*
			 * Setup the interrupt handler. Note that we pass the
			 * fastpath context to the interrupt handler in this
			 * case.
			 */
			rc = bus_setup_intr(sc->dev,
					    sc->bxe_msi_res[i + 1],
					    INTR_TYPE_NET | INTR_MPSAFE,
					    NULL,
					    bxe_intr_fp,
					    fp,
					    &sc->bxe_msi_tag[i + 1]
					    );

			if (rc) {
				BXE_PRINTF(
				"%s(%d): Failed to allocate MSI[%d] vector!\n",
				__FILE__, __LINE__, (i + 1));
				goto bxe_interrupt_attach_exit;
			}
#ifdef BXE_TASK
			TASK_INIT(&fp->task, 0, bxe_task_fp, fp);
			fp->tq = taskqueue_create_fast("bxe_fpq", M_NOWAIT,
					taskqueue_thread_enqueue, &fp->tq);
			taskqueue_start_threads(&fp->tq, 1, PI_NET, "%s fpq",
				device_get_nameunit(sc->dev));
#endif
		}

	} else {
#ifdef BXE_TASK
		fp = &sc->fp[0];
#endif
		DBPRINT(sc, (BXE_VERBOSE_LOAD | BXE_VERBOSE_INTR),
			"%s(): Enabling INTx interrupts.\n", __FUNCTION__);

		/*
		 * Setup the interrupt handler.  Note that we pass the
		 * driver instance to the interrupt handler which
		 * will handle both the slowpath and fastpath.
		 */
		rc = bus_setup_intr(sc->dev,sc->bxe_irq_res,
		    INTR_TYPE_NET | INTR_MPSAFE,
		    NULL,
		    bxe_intr_legacy,
		    sc,
		    &sc->bxe_irq_tag);

		if (rc) {
			BXE_PRINTF("%s(%d): Failed to allocate interrupt!\n",
				   __FILE__, __LINE__);
			goto bxe_interrupt_attach_exit;
		}
#ifdef BXE_TASK
		TASK_INIT(&fp->task, 0, bxe_task_fp, fp);
		fp->tq = taskqueue_create_fast("bxe_fpq",
		    M_NOWAIT, taskqueue_thread_enqueue,	&fp->tq);
		taskqueue_start_threads(&fp->tq, 1,
		    PI_NET, "%s fpq", device_get_nameunit(sc->dev));
#endif
	}

bxe_interrupt_attach_exit:
	DBEXIT(BXE_VERBOSE_LOAD | BXE_VERBOSE_INTR);
	return (rc);
}


/*
 * PCI Capabilities Probe Function.
 *
 * Walks the PCI capabiites list for the device to find what features are
 * supported.  These capabilites may be enabled/disabled by firmware so it's
 * best to walk the list rather than hard code any values.
 *
 * Returns:
 *   None.
 */
static void
bxe_probe_pci_caps(struct bxe_softc *sc)
{
	device_t dev;
	uint32_t reg;
	uint16_t link_status;

	dev = sc->dev;
	DBENTER(BXE_EXTREME_LOAD);

	/* Check if PCI Power Management capability is enabled. */
	if (pci_find_cap(dev, PCIY_PMG, &reg) == 0) {
		if (reg != 0) {
			DBPRINT(sc, BXE_EXTREME_LOAD,
				"%s(): Found PM capability at 0x%04X\n",
				__FUNCTION__, reg);
			sc->pm_cap = reg;
		}
	}

	/* Check if PCIe capability is enabled. */
	if (pci_find_cap(dev, PCIY_EXPRESS, &reg) == 0) {
		if (reg != 0) {
			link_status = pci_read_config(dev, reg + 0x12, 2);

			DBPRINT(sc, BXE_EXTREME_LOAD,
				"%s(): Found PCIe capability at 0x%04X\n",
				__FUNCTION__, reg);

			/* Handle PCIe 2.0 workarounds for the 57710. */
			if (CHIP_IS_E1(sc)) {
				/* Workaround for 57710 errata E4_57710_27462. */
				sc->pcie_link_speed =
					(REG_RD(sc, 0x3d04) & (1 << 24)) ? 2 : 1;

				/* Workaround for 57710 errata E4_57710_27488. */
				sc->pcie_link_width = (link_status >> 4) & 0x3f;
				if (sc->pcie_link_speed > 1)
					sc->pcie_link_width =
						((link_status >> 4) & 0x3f) >> 1;

			} else {

				sc->pcie_link_speed = link_status & 0xf;
				sc->pcie_link_width = (link_status >> 4) & 0x3f;

			}

			sc->bxe_cap_flags |= BXE_PCIE_CAPABLE_FLAG;
			sc->pcie_cap = reg;
		}
	}


	/* Check if MSI capability is enabled. */
	if (pci_find_cap(dev, PCIY_MSI, &reg) == 0) {
		if (reg != 0) {
			DBPRINT(sc, BXE_EXTREME_LOAD,
				"%s(): Found MSI capability at 0x%04X\n",
				__FUNCTION__, reg);
			sc->bxe_cap_flags |= BXE_MSI_CAPABLE_FLAG;
		}
	}

	/* Check if MSI-X capability is enabled. */
	if (pci_find_cap(dev, PCIY_MSIX, &reg) == 0) {
		if (reg != 0) {
			DBPRINT(sc, BXE_EXTREME_LOAD,
				"%s(): Found MSI-X capability at 0x%04X\n",
				__FUNCTION__, reg);
			sc->bxe_cap_flags |= BXE_MSIX_CAPABLE_FLAG;
		}
	}

	DBEXIT(BXE_EXTREME_LOAD);
}

static void
bxe_init_e1_firmware(struct bxe_softc *sc)
{
	INIT_OPS(sc)                  = (struct raw_op *)init_ops_e1;
	INIT_DATA(sc)                 = (const uint32_t *)init_data_e1;
	INIT_OPS_OFFSETS(sc)          = (const uint16_t *)init_ops_offsets_e1;
	INIT_TSEM_INT_TABLE_DATA(sc)  = tsem_int_table_data_e1;
	INIT_TSEM_PRAM_DATA(sc)       = tsem_pram_data_e1;
	INIT_USEM_INT_TABLE_DATA(sc)  = usem_int_table_data_e1;
	INIT_USEM_PRAM_DATA(sc)       = usem_pram_data_e1;
	INIT_XSEM_INT_TABLE_DATA(sc)  = xsem_int_table_data_e1;
	INIT_XSEM_PRAM_DATA(sc)       = xsem_pram_data_e1;
	INIT_CSEM_INT_TABLE_DATA(sc)  = csem_int_table_data_e1;
	INIT_CSEM_PRAM_DATA(sc)       = csem_pram_data_e1;
}

static void
bxe_init_e1h_firmware(struct bxe_softc *sc)
{
	INIT_OPS(sc)                  = (struct raw_op *)init_ops_e1h;
	INIT_DATA(sc)                 = (const uint32_t *)init_data_e1h;
	INIT_OPS_OFFSETS(sc)          = (const uint16_t *)init_ops_offsets_e1h;
	INIT_TSEM_INT_TABLE_DATA(sc)  = tsem_int_table_data_e1h;
	INIT_TSEM_PRAM_DATA(sc)       = tsem_pram_data_e1h;
	INIT_USEM_INT_TABLE_DATA(sc)  = usem_int_table_data_e1h;
	INIT_USEM_PRAM_DATA(sc)       = usem_pram_data_e1h;
	INIT_XSEM_INT_TABLE_DATA(sc)  = xsem_int_table_data_e1h;
	INIT_XSEM_PRAM_DATA(sc)       = xsem_pram_data_e1h;
	INIT_CSEM_INT_TABLE_DATA(sc)  = csem_int_table_data_e1h;
	INIT_CSEM_PRAM_DATA(sc)       = csem_pram_data_e1h;
}

static int
bxe_init_firmware(struct bxe_softc *sc)
{
	if (CHIP_IS_E1(sc))
		bxe_init_e1_firmware(sc);
	else if (CHIP_IS_E1H(sc))
		bxe_init_e1h_firmware(sc);
	else {
		BXE_PRINTF("%s(%d): Unsupported chip revision\n",
		    __FILE__, __LINE__);
		return (ENXIO);
	}
	return (0);
}


static void
bxe_set_tunables(struct bxe_softc *sc)
{
	/*
	 * Get our starting point for interrupt mode/number of queues.
	 * We will progressively step down from MSI-X to MSI to INTx
	 * and reduce the number of receive queues as necessary to
	 * match the system capabilities.
	 */
	sc->multi_mode	= bxe_multi_mode;
	sc->int_mode	= bxe_int_mode;
	sc->tso_enable	= bxe_tso_enable;

	/*
	 * Verify the Priority -> Receive Queue mappings.
	 */
	if (sc->int_mode > 0) {
		/* Multi-queue modes require MSI/MSI-X. */
		switch (sc->multi_mode) {
		case ETH_RSS_MODE_DISABLED:
			/* No multi-queue mode requested. */
			sc->num_queues = 1;
			break;
		case ETH_RSS_MODE_REGULAR:
			if (sc->int_mode > 1) {
				/*
				 * Assume we can use MSI-X
				 * (max of 16 receive queues).
				 */
				sc->num_queues = min((bxe_queue_count ?
				    bxe_queue_count : mp_ncpus), MAX_CONTEXT);
			} else {
				/*
				 * Assume we can use MSI
				 * (max of 7 receive queues).
				 */
				sc->num_queues = min((bxe_queue_count ?
				    bxe_queue_count : mp_ncpus),
				    BXE_MSI_VECTOR_COUNT - 1);
			}
			break;
		default:
			BXE_PRINTF(
			    "%s(%d): Unsupported multi_mode parameter (%d), "
			    "disabling multi-queue support!\n", __FILE__,
			    __LINE__, sc->multi_mode);
			sc->multi_mode = ETH_RSS_MODE_DISABLED;
			sc->num_queues = 1;
			break;
		}
	} else {
		/* User	has forced INTx mode. */
		sc->multi_mode = ETH_RSS_MODE_DISABLED;
		sc->num_queues = 1;
	}

	DBPRINT(sc, (BXE_VERBOSE_LOAD | BXE_VERBOSE_INTR),
	    "%s(): Requested: int_mode = %d, multi_mode = %d num_queues = %d\n",
	    __FUNCTION__, sc->int_mode, sc->multi_mode, sc->num_queues);

	/* Set transparent packet aggregation (TPA), aka LRO, flag. */
	if (bxe_tpa_enable!= FALSE)
		sc->bxe_flags |= BXE_TPA_ENABLE_FLAG;

	/* Capture the stats enable/disable setting. */
	if (bxe_stats_enable == FALSE)
		sc->stats_enable = FALSE;
	else
		sc->stats_enable = TRUE;

	/* Select the host coalescing tick count values (limit values). */
	if (bxe_tx_ticks > 100) {
		BXE_PRINTF("%s(%d): bxe_tx_ticks too large "
		    "(%d), setting default value of 50.\n",
		    __FILE__, __LINE__, bxe_tx_ticks);
		sc->tx_ticks = 50;
	} else
		sc->tx_ticks = bxe_tx_ticks;

	if (bxe_rx_ticks > 100) {
		BXE_PRINTF("%s(%d): bxe_rx_ticks too large "
		    "(%d), setting default value of 25.\n",
		    __FILE__, __LINE__, bxe_rx_ticks);
		sc->rx_ticks = 25;
	} else
		sc->rx_ticks = bxe_rx_ticks;

	/* Select the PCIe maximum read request size (MRRS). */
	if (bxe_mrrs > 3)
		sc->mrrs = 3;
	else
		sc->mrrs = bxe_mrrs;

	/* Check for DCC support. */
	if (bxe_dcc_enable == FALSE)
		sc->dcc_enable = FALSE;
	else
		sc->dcc_enable = TRUE;
}


/*
 * Returns:
 *   0 = Success, !0 = Failure
 */
static int
bxe_alloc_pci_resources(struct bxe_softc *sc)
{
	int rid, rc = 0;

	DBENTER(BXE_VERBOSE_LOAD);

	/*
	 * Allocate PCI memory resources for BAR0.
	 * This includes device registers and internal
	 * processor memory.
	 */
	rid = PCIR_BAR(0);
	sc->bxe_res = bus_alloc_resource_any(
	    sc->dev, SYS_RES_MEMORY, &rid, RF_ACTIVE);
	if (sc->bxe_res == NULL) {
		BXE_PRINTF("%s(%d):PCI BAR0 memory allocation failed\n",
		    __FILE__, __LINE__);
		rc = ENXIO;
		goto bxe_alloc_pci_resources_exit;
	}

	/* Get OS resource handles for BAR0 memory. */
	sc->bxe_btag	= rman_get_bustag(sc->bxe_res);
	sc->bxe_bhandle	= rman_get_bushandle(sc->bxe_res);
	sc->bxe_vhandle	= (vm_offset_t) rman_get_virtual(sc->bxe_res);

	/*
	 * Allocate PCI memory resources for BAR2.
	 * Doorbell (DB) memory.
	 */
	rid = PCIR_BAR(2);
	sc->bxe_db_res = bus_alloc_resource_any(
	    sc->dev, SYS_RES_MEMORY, &rid, RF_ACTIVE);
	if (sc->bxe_db_res == NULL) {
		BXE_PRINTF("%s(%d): PCI BAR2 memory allocation failed\n",
		    __FILE__, __LINE__);
		rc = ENXIO;
		goto bxe_alloc_pci_resources_exit;
	}

	/* Get OS resource handles for BAR2 memory. */
	sc->bxe_db_btag    = rman_get_bustag(sc->bxe_db_res);
	sc->bxe_db_bhandle = rman_get_bushandle(sc->bxe_db_res);
	sc->bxe_db_vhandle = (vm_offset_t) rman_get_virtual(sc->bxe_db_res);

bxe_alloc_pci_resources_exit:
	DBEXIT(BXE_VERBOSE_LOAD);
	return(rc);
}


/*
 * Returns:
 *   None
 */
static void
bxe_release_pci_resources(struct bxe_softc *sc)
{
	/* Release the PCIe BAR0 mapped memory. */
	if (sc->bxe_res != NULL) {
		DBPRINT(sc, (BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET),
		    "%s(): Releasing PCI BAR0 memory.\n", __FUNCTION__);
		bus_release_resource(sc->dev,
		    SYS_RES_MEMORY, PCIR_BAR(0), sc->bxe_res);
	}

	/* Release the PCIe BAR2 (doorbell) mapped memory. */
	if (sc->bxe_db_res != NULL) {
		DBPRINT(sc, (BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET),
		    "%s(): Releasing PCI BAR2 memory.\n", __FUNCTION__);
		bus_release_resource(sc->dev,
		    SYS_RES_MEMORY, PCIR_BAR(2), sc->bxe_db_res);
	}
}


/*
 * Returns:
 *   0 = Success, !0 = Failure
 */
static int
bxe_media_detect(struct bxe_softc *sc)
{
	int rc = 0;

	/* Identify supported media based on the PHY type. */
	switch (XGXS_EXT_PHY_TYPE(sc->link_params.ext_phy_config)) {
	case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_DIRECT:
		DBPRINT(sc, BXE_INFO_LOAD,
		    "%s(): Found 10GBase-CX4 media.\n", __FUNCTION__);
		sc->media = IFM_10G_CX4;
		break;
	case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8073:
		/* Technically 10GBase-KR but report as 10GBase-SR*/
	case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8726:
	case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727:
	case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727_NOC:
		DBPRINT(sc, BXE_INFO_LOAD,
		    "%s(): Found 10GBase-SR media.\n", __FUNCTION__);
		sc->media = IFM_10G_SR;
		break;
	case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8705:
	case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8706:
		DBPRINT(sc, BXE_INFO_LOAD,
		    "%s(): Found 10Gb twinax media.\n", __FUNCTION__);
		sc->media = IFM_10G_TWINAX;
		break;
	case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8481:
	case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_SFX7101:
	case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM84823:
		DBPRINT(sc, BXE_INFO_LOAD,
		    "%s(): Found 10GBase-T media.\n", __FUNCTION__);
		sc->media = IFM_10G_T;
		break;
	case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_FAILURE:
	case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_NOT_CONN:
	default:
		BXE_PRINTF("%s(%d): PHY not supported by driver!\n",
		    __FILE__, __LINE__);
		sc->media = 0;
		rc = ENODEV;
	}

	return (rc);
}


/*
 * Device attach function.
 *
 * Allocates device resources, performs secondary chip identification,
 * resets and initializes the hardware, and initializes driver instance
 * variables.
 *
 * Returns:
 *   0 = Success, Positive value on failure.
 */
static int
bxe_attach(device_t dev)
{
	struct bxe_softc *sc;
	struct ifnet *ifp;
	int rc;

	sc = device_get_softc(dev);
	DBENTER(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);

	sc->dev = dev;
	sc->bxe_unit = device_get_unit(dev);
	sc->bxe_func = pci_get_function(dev);
	sc->bxe_flags = 0;
	sc->state = BXE_STATE_CLOSED;
	rc = 0;
	bxe_set_tunables(sc);

	bxe_alloc_mutexes(sc);

	/* Prepare the tick routine. */
	callout_init(&sc->bxe_tick_callout, CALLOUT_MPSAFE);

	/* Enable bus master capability */
	pci_enable_busmaster(dev);

	if ((rc = bxe_alloc_pci_resources(sc)) != 0)
		goto bxe_attach_fail;

	/* Put indirect address registers into a sane state. */
	pci_write_config(sc->dev, PCICFG_GRC_ADDRESS,
	    PCICFG_VENDOR_ID_OFFSET, 4);
	REG_WR(sc, PXP2_REG_PGL_ADDR_88_F0 + BP_PORT(sc) * 16, 0);
	REG_WR(sc, PXP2_REG_PGL_ADDR_8C_F0 + BP_PORT(sc) * 16, 0);
	REG_WR(sc, PXP2_REG_PGL_ADDR_90_F0 + BP_PORT(sc) * 16, 0);
	REG_WR(sc, PXP2_REG_PGL_ADDR_94_F0 + BP_PORT(sc) * 16, 0);

	/* Get hardware info from shared memory and validate data. */
	if (bxe_get_function_hwinfo(sc)) {
		DBPRINT(sc, BXE_WARN,
		    "%s(): Failed to get hardware info!\n", __FUNCTION__);
		rc = ENODEV;
		goto bxe_attach_fail;
	}

	/* Setup supported media options. */
	if ((rc = bxe_media_detect(sc)) != 0)
		goto bxe_attach_fail;

	ifmedia_init(&sc->bxe_ifmedia,
	    IFM_IMASK, bxe_ifmedia_upd,	bxe_ifmedia_status);
	ifmedia_add(&sc->bxe_ifmedia,
	    IFM_ETHER | sc->media | IFM_FDX, 0,	NULL);
	ifmedia_add(&sc->bxe_ifmedia,
	    IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&sc->bxe_ifmedia,
	    IFM_ETHER | IFM_AUTO);
	sc->bxe_ifmedia.ifm_media =
	    sc->bxe_ifmedia.ifm_cur->ifm_media;

	/* Set init arrays */
	rc = bxe_init_firmware(sc);
	if (rc) {
		BXE_PRINTF("%s(%d): Error loading firmware\n",
		    __FILE__, __LINE__);
		goto bxe_attach_fail;
	}


#ifdef BXE_DEBUG
	/* Allocate a memory buffer for grcdump output.*/
	sc->grcdump_buffer = malloc(BXE_GRCDUMP_BUF_SIZE, M_TEMP, M_NOWAIT);
	if (sc->grcdump_buffer == NULL) {
		/* Failure is OK, just print a message and continue attach. */
		BXE_PRINTF("%s(%d): Failed to allocate grcdump memory "
		    "buffer!\n", __FILE__, __LINE__);
	}
#endif

	/* Check that NVRAM contents are valid.*/
	if (bxe_nvram_test(sc)) {
		BXE_PRINTF("%s(%d): Failed NVRAM test!\n",
		    __FILE__, __LINE__);
		rc = ENODEV;
		goto bxe_attach_fail;
	}

	/* Allocate the appropriate interrupts.*/
	if (bxe_interrupt_allocate(sc)) {
		BXE_PRINTF("%s(%d): Interrupt allocation failed!\n",
		    __FILE__, __LINE__);
		rc = ENODEV;
		goto bxe_attach_fail;
	}

	/* Useful for accessing unconfigured devices (i.e. factory diags).*/
	if (nomcp)
		sc->bxe_flags |= BXE_NO_MCP_FLAG;

	/* If bootcode is not running only initialize port 0. */
	if (nomcp && BP_PORT(sc)) {
		BXE_PRINTF(
		    "%s(%d): Second device disabled (no bootcode), "
		    "exiting...\n", __FILE__, __LINE__);
		rc = ENODEV;
		goto bxe_attach_fail;
	}

	/* Check if PXE/UNDI is still active and unload it. */
	if (!BP_NOMCP(sc))
		bxe_undi_unload(sc);

	/*
	 * Select the RX and TX ring sizes.  The actual
	 * ring size for TX is complicated by the fact
	 * that a single TX frame may be broken up into
	 * many buffer descriptors (tx_start_bd,
	 * tx_parse_bd, tx_data_bd).  In the best case,
	 * there are always at least two BD's required
	 * so we'll assume the best case here.
	 */
	sc->tx_ring_size = (USABLE_TX_BD >> 1);
	sc->rx_ring_size = USABLE_RX_BD;

	/* Assume receive IP/TCP/UDP checksum is enabled. */
	sc->rx_csum = 1;

	/* Disable WoL. */
	sc->wol = 0;

	/* Assume a standard 1500 byte MTU size for mbuf allocations. */
	sc->mbuf_alloc_size  = MCLBYTES;

	/* Allocate DMA memory resources. */
	if (bxe_dma_alloc(sc->dev)) {
		BXE_PRINTF("%s(%d): DMA memory allocation failed!\n",
		    __FILE__, __LINE__);
		rc = ENOMEM;
		goto bxe_attach_fail;
	}

	/* Allocate a FreeBSD ifnet structure. */
	ifp = sc->bxe_ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		BXE_PRINTF("%s(%d): Interface allocation failed!\n",
		    __FILE__, __LINE__);
		rc = ENXIO;
		goto bxe_attach_fail;
	}

	/* Initialize the FreeBSD ifnet interface. */
	ifp->if_softc = sc;
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = bxe_ioctl;
	ifp->if_start = bxe_tx_start;

#if __FreeBSD_version >= 800000
	ifp->if_transmit = bxe_tx_mq_start;
	ifp->if_qflush   = bxe_mq_flush;
#endif

#ifdef FreeBSD8_0
	ifp->if_timer = 0;
#endif

	ifp->if_init = bxe_init;
	ifp->if_mtu = ETHERMTU;
	ifp->if_hwassist = BXE_IF_HWASSIST;
	ifp->if_capabilities = BXE_IF_CAPABILITIES;
	if (TPA_ENABLED(sc)) {
		ifp->if_capabilities |= IFCAP_LRO;
	}
	ifp->if_capenable = ifp->if_capabilities;
	ifp->if_baudrate = IF_Gbps(10UL);

	ifp->if_snd.ifq_drv_maxlen = sc->tx_ring_size;

	IFQ_SET_MAXLEN(&ifp->if_snd, ifp->if_snd.ifq_drv_maxlen);
	IFQ_SET_READY(&ifp->if_snd);

	/* Attach to the Ethernet interface list. */
	ether_ifattach(ifp, sc->link_params.mac_addr);

	/* Attach the interrupts to the interrupt handlers. */
	if (bxe_interrupt_attach(sc)) {
		BXE_PRINTF("%s(%d): Interrupt allocation failed!\n",
		    __FILE__, __LINE__);
		goto bxe_attach_fail;
	}

	/* Print important adapter info for the user. */
	bxe_print_adapter_info(sc);

	/* Add the supported sysctls to the kernel. */
	bxe_add_sysctls(sc);

bxe_attach_fail:
	if (rc != 0)
		bxe_detach(dev);

	DBEXIT(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);
	return(rc);
}


/*
 * Supported link settings.
 *
 * Examines hardware configuration present in NVRAM and
 * determines the link settings that are supported between
 * the external PHY and the switch.
 *
 * Returns:
 *   None.
 *
 * Side effects:
 * 	 Sets sc->port.supported
 *	 Sets sc->link_params.phy_addr
 */
static void
bxe_link_settings_supported(struct bxe_softc *sc, uint32_t switch_cfg)
{
	uint32_t ext_phy_type;
	int port;

	DBENTER(BXE_VERBOSE_PHY);
	DBPRINT(sc, BXE_VERBOSE_PHY, "%s(): switch_cfg = 0x%08X\n",
		__FUNCTION__, switch_cfg);

	port = BP_PORT(sc);
	/* Get the link	settings supported by the external PHY. */
	switch (switch_cfg) {
	case SWITCH_CFG_1G:
		ext_phy_type =
		    SERDES_EXT_PHY_TYPE(sc->link_params.ext_phy_config);

		DBPRINT(sc, BXE_VERBOSE_PHY,
		    "%s(): 1G switch w/ ext_phy_type = "
		    "0x%08X\n", __FUNCTION__, ext_phy_type);

		switch (ext_phy_type) {
		case PORT_HW_CFG_SERDES_EXT_PHY_TYPE_DIRECT:
			DBPRINT(sc, BXE_VERBOSE_PHY, "%s(): 1G Direct.\n",
			    __FUNCTION__);

			sc->port.supported |=
			    (SUPPORTED_10baseT_Half |
			     SUPPORTED_10baseT_Full |
			     SUPPORTED_100baseT_Half |
			     SUPPORTED_100baseT_Full |
			     SUPPORTED_1000baseT_Full |
			     SUPPORTED_2500baseX_Full |
			     SUPPORTED_TP |
			     SUPPORTED_FIBRE |
			     SUPPORTED_Autoneg |
			     SUPPORTED_Pause |
			     SUPPORTED_Asym_Pause);
			break;

		case PORT_HW_CFG_SERDES_EXT_PHY_TYPE_BCM5482:
			DBPRINT(sc, BXE_VERBOSE_PHY, "%s(): 1G 5482\n",
				__FUNCTION__);

			sc->port.supported |=
			    (SUPPORTED_10baseT_Half |
			     SUPPORTED_10baseT_Full |
			     SUPPORTED_100baseT_Half |
			     SUPPORTED_100baseT_Full |
			     SUPPORTED_1000baseT_Full |
			     SUPPORTED_TP |
			     SUPPORTED_FIBRE |
			     SUPPORTED_Autoneg |
			     SUPPORTED_Pause |
			     SUPPORTED_Asym_Pause);
			break;

		default:
			BXE_PRINTF(
			    "%s(%d): Bad NVRAM 1Gb PHY configuration data "
			    "(ext_phy_config=0x%08X).\n",
			    __FILE__, __LINE__,
			    sc->link_params.ext_phy_config);
			goto bxe_link_settings_supported_exit;
		}

		sc->port.phy_addr =
		    REG_RD(sc, NIG_REG_SERDES0_CTRL_PHY_ADDR + (port * 0x10));

		DBPRINT(sc, BXE_VERBOSE_PHY, "%s(): phy_addr = 0x%08X\n",
		    __FUNCTION__, sc->port.phy_addr);
		break;

	case SWITCH_CFG_10G:
		ext_phy_type =
		    XGXS_EXT_PHY_TYPE(sc->link_params.ext_phy_config);

		DBPRINT(
		    sc, BXE_VERBOSE_PHY,
		    "%s(): 10G switch w/ ext_phy_type = 0x%08X\n",
		    __FUNCTION__, ext_phy_type);

		switch (ext_phy_type) {
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_DIRECT:
			DBPRINT(sc, BXE_VERBOSE_PHY,
			    "%s(): 10G switch w/ direct connect.\n",
			    __FUNCTION__);

			sc->port.supported |=
			    (SUPPORTED_10baseT_Half |
			     SUPPORTED_10baseT_Full |
			     SUPPORTED_100baseT_Half |
			     SUPPORTED_100baseT_Full |
			     SUPPORTED_1000baseT_Full |
			     SUPPORTED_2500baseX_Full |
			     SUPPORTED_10000baseT_Full |
			     SUPPORTED_TP |
			     SUPPORTED_FIBRE |
			     SUPPORTED_Autoneg |
			     SUPPORTED_Pause |
			     SUPPORTED_Asym_Pause);
			break;

		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8072:
			DBPRINT(sc, BXE_VERBOSE_PHY,
			    "ext_phy_type 0x%x (8072)\n",ext_phy_type);

			sc->port.supported |=
			    (SUPPORTED_10000baseT_Full |
			     SUPPORTED_1000baseT_Full |
			     SUPPORTED_FIBRE |
			     SUPPORTED_Autoneg |
			     SUPPORTED_Pause |
			     SUPPORTED_Asym_Pause);
			break;

		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8073:
			DBPRINT(sc,
			    BXE_VERBOSE_PHY,"ext_phy_type 0x%x (8073)\n",
			    ext_phy_type);

			sc->port.supported |=
			    (SUPPORTED_10000baseT_Full |
			     SUPPORTED_2500baseX_Full |
			     SUPPORTED_1000baseT_Full |
			     SUPPORTED_FIBRE |
			     SUPPORTED_Autoneg |
			     SUPPORTED_Pause |
			     SUPPORTED_Asym_Pause);
			break;

		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8705:
			DBPRINT(sc, BXE_VERBOSE_PHY,
			    "%s(): 10G switch w/ 8705.\n",__FUNCTION__);

			sc->port.supported |=
			    (SUPPORTED_10000baseT_Full |
			     SUPPORTED_FIBRE |
			     SUPPORTED_Pause |
			     SUPPORTED_Asym_Pause);
			break;

		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8706:
			DBPRINT(sc, BXE_VERBOSE_PHY,
			    "%s(): 10G switch w/ 8706.\n",
			    __FUNCTION__);

			sc->port.supported |=
			    (SUPPORTED_10000baseT_Full |
			     SUPPORTED_1000baseT_Full |
			     SUPPORTED_FIBRE |
			     SUPPORTED_Pause |
			     SUPPORTED_Asym_Pause);
			break;

		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8726:
			DBPRINT(sc, BXE_VERBOSE_PHY,
			    "%s(): 10G switch w/ 8726.\n",
			    __FUNCTION__);

			sc->port.supported |=
			    (SUPPORTED_10000baseT_Full |
			     SUPPORTED_FIBRE |
			     SUPPORTED_Pause |
			     SUPPORTED_Asym_Pause);
			break;

		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727:
			DBPRINT(sc, BXE_VERBOSE_PHY,"ext_phy_type 0x%x (8727)\n",
			    ext_phy_type);

			sc->port.supported |=
			    (SUPPORTED_10000baseT_Full |
			     SUPPORTED_1000baseT_Full |
			     SUPPORTED_Autoneg |
			     SUPPORTED_FIBRE |
			     SUPPORTED_Pause |
			     SUPPORTED_Asym_Pause);
			break;

		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_SFX7101:
			 DBPRINT(sc, BXE_VERBOSE_PHY,
			    "%s(): 10G switch w/ SFX7101.\n",
			    __FUNCTION__);

			sc->port.supported |=
			    (SUPPORTED_10000baseT_Full |
			     SUPPORTED_TP |
			     SUPPORTED_Autoneg |
			     SUPPORTED_Pause |
			     SUPPORTED_Asym_Pause);
			break;

		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8481:
			 DBPRINT(sc, BXE_VERBOSE_PHY,
			    "ext_phy_type 0x%x (BCM8481)\n",
			    ext_phy_type);

			sc->port.supported |=
			    (SUPPORTED_10baseT_Half |
			     SUPPORTED_10baseT_Full |
			     SUPPORTED_100baseT_Half |
			     SUPPORTED_100baseT_Full |
			     SUPPORTED_1000baseT_Full |
			     SUPPORTED_10000baseT_Full |
			     SUPPORTED_TP |
			     SUPPORTED_Autoneg |
			     SUPPORTED_Pause |
			     SUPPORTED_Asym_Pause);
			break;

		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_FAILURE:
			DBPRINT(sc, BXE_WARN,
			    "%s(): 10G XGXS PHY failure detected.\n",
			    __FUNCTION__);
			break;

			BXE_PRINTF(
			    "%s(%d): Bad NVRAM 10Gb PHY configuration data "
			    "(ext_phy_config=0x%08X).\n",
			    __FILE__, __LINE__,
			    sc->link_params.ext_phy_config);
			goto bxe_link_settings_supported_exit;
		}

		sc->port.phy_addr =
		    REG_RD(sc, NIG_REG_XGXS0_CTRL_PHY_ADDR +(port * 0x18));
		break;

	default:
		DBPRINT(sc, BXE_WARN, "%s(): BAD switch configuration "
		    "(link_config = 0x%08X)\n", __FUNCTION__,
		    sc->port.link_config);
		goto bxe_link_settings_supported_exit;
	}

	sc->link_params.phy_addr = sc->port.phy_addr;

	/* Mask out unsupported speeds according to NVRAM. */
	if ((sc->link_params.speed_cap_mask &
	    PORT_HW_CFG_SPEED_CAPABILITY_D0_10M_HALF) == 0)
		sc->port.supported &= ~SUPPORTED_10baseT_Half;

	if ((sc->link_params.speed_cap_mask &
	    PORT_HW_CFG_SPEED_CAPABILITY_D0_10M_FULL) == 0)
		sc->port.supported &= ~SUPPORTED_10baseT_Full;

	if ((sc->link_params.speed_cap_mask &
	    PORT_HW_CFG_SPEED_CAPABILITY_D0_100M_HALF) == 0)
		sc->port.supported &= ~SUPPORTED_100baseT_Half;

	if ((sc->link_params.speed_cap_mask &
	    PORT_HW_CFG_SPEED_CAPABILITY_D0_100M_FULL) == 0)
		sc->port.supported &= ~SUPPORTED_100baseT_Full;

	if ((sc->link_params.speed_cap_mask &
	    PORT_HW_CFG_SPEED_CAPABILITY_D0_1G) == 0)
		sc->port.supported &= ~(SUPPORTED_1000baseT_Half |
			SUPPORTED_1000baseT_Full);

	if ((sc->link_params.speed_cap_mask &
	    PORT_HW_CFG_SPEED_CAPABILITY_D0_2_5G) == 0)
		sc->port.supported &= ~SUPPORTED_2500baseX_Full;

	if ((sc->link_params.speed_cap_mask &
	    PORT_HW_CFG_SPEED_CAPABILITY_D0_10G) == 0)
		sc->port.supported &= ~SUPPORTED_10000baseT_Full;

	DBPRINT(sc, BXE_VERBOSE_PHY,
	    "%s(): Supported link settings = 0x%b\n", __FUNCTION__,
	    sc->port.supported, BXE_SUPPORTED_PRINTFB);

bxe_link_settings_supported_exit:

	DBEXIT(BXE_VERBOSE_PHY);
}

/*
 * Requested link settings.
 *
 * Returns:
 *   None.
 */
static void
bxe_link_settings_requested(struct bxe_softc *sc)
{
	uint32_t ext_phy_type;
	DBENTER(BXE_VERBOSE_PHY);

	sc->link_params.req_duplex = MEDIUM_FULL_DUPLEX;

	switch (sc->port.link_config & PORT_FEATURE_LINK_SPEED_MASK) {

	case PORT_FEATURE_LINK_SPEED_AUTO:
		if (sc->port.supported & SUPPORTED_Autoneg) {
			sc->link_params.req_line_speed |= SPEED_AUTO_NEG;
			sc->port.advertising = sc->port.supported;
		} else {
			ext_phy_type = XGXS_EXT_PHY_TYPE(
			    sc->link_params.ext_phy_config);

			if ((ext_phy_type ==
			     PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8705) ||
			    (ext_phy_type ==
			     PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8706)) {
				/* Force 10G, no autonegotiation. */
				sc->link_params.req_line_speed = SPEED_10000;
				sc->port.advertising =
				    ADVERTISED_10000baseT_Full |
				    ADVERTISED_FIBRE;
				break;
			}

			DBPRINT(sc, BXE_FATAL,
			    "%s(): NVRAM config error. Invalid "
			    "link_config (0x%08X) - Autoneg not supported!\n",
			    __FUNCTION__, sc->port.link_config);
			goto bxe_link_settings_requested_exit;
		}
		break;
	case PORT_FEATURE_LINK_SPEED_10M_FULL:
		if (sc->port.supported & SUPPORTED_10baseT_Full) {
			sc->link_params.req_line_speed = SPEED_10;
			sc->port.advertising = ADVERTISED_10baseT_Full |
				ADVERTISED_TP;
		} else {
			DBPRINT(sc, BXE_FATAL,
			    "%s(): NVRAM config error. Invalid "
			    "link_config (0x%08X) - speed_cap_mask 0x%08X\n",
			    __FUNCTION__, sc->port.link_config,
			    sc->link_params.speed_cap_mask);
			goto bxe_link_settings_requested_exit;
		}
		break;
	case PORT_FEATURE_LINK_SPEED_10M_HALF:
		if (sc->port.supported & SUPPORTED_10baseT_Half) {
			sc->link_params.req_line_speed = SPEED_10;
			sc->link_params.req_duplex = MEDIUM_HALF_DUPLEX;
			sc->port.advertising = ADVERTISED_10baseT_Half |
				ADVERTISED_TP;
		} else {
			DBPRINT(sc, BXE_FATAL,
			    "%s(): NVRAM config error. Invalid "
			    "link_config (0x%08X) - speed_cap_mask = 0x%08X\n",
			    __FUNCTION__, sc->port.link_config,
			    sc->link_params.speed_cap_mask);
			goto bxe_link_settings_requested_exit;
		}
		break;
	case PORT_FEATURE_LINK_SPEED_100M_FULL:
		if (sc->port.supported & SUPPORTED_100baseT_Full) {
			sc->link_params.req_line_speed = SPEED_100;
			sc->port.advertising = ADVERTISED_100baseT_Full |
				ADVERTISED_TP;
		} else {
			DBPRINT(sc, BXE_FATAL,
			    "%s(): NVRAM config error. Invalid "
			    "link_config (0x%08X) - speed_cap_mask = 0x%08X\n",
			    __FUNCTION__, sc->port.link_config,
			    sc->link_params.speed_cap_mask);
			goto bxe_link_settings_requested_exit;
		}
		break;
	case PORT_FEATURE_LINK_SPEED_100M_HALF:
		if (sc->port.supported & SUPPORTED_100baseT_Half) {
			sc->link_params.req_line_speed = SPEED_100;
			sc->link_params.req_duplex = MEDIUM_HALF_DUPLEX;
			sc->port.advertising = ADVERTISED_100baseT_Half |
				ADVERTISED_TP;
		} else {
			DBPRINT(sc, BXE_FATAL,
			    "%s(): NVRAM config error. Invalid "
			    "link_config (0x%08X) - speed_cap_mask = 0x%08X\n",
			    __FUNCTION__, sc->port.link_config,
			    sc->link_params.speed_cap_mask);
			goto bxe_link_settings_requested_exit;
		}
		break;
	case PORT_FEATURE_LINK_SPEED_1G:
		if (sc->port.supported & SUPPORTED_1000baseT_Full) {
			sc->link_params.req_line_speed = SPEED_1000;
			sc->port.advertising = ADVERTISED_1000baseT_Full |
				ADVERTISED_TP;
		} else {
			DBPRINT(sc, BXE_FATAL,
			    "%s(): NVRAM config error. Invalid "
			    "link_config (0x%08X) - speed_cap_mask = 0x%08X\n",
			    __FUNCTION__, sc->port.link_config,
			    sc->link_params.speed_cap_mask);
			goto bxe_link_settings_requested_exit;
		}
		break;
	case PORT_FEATURE_LINK_SPEED_2_5G:
		if (sc->port.supported & SUPPORTED_2500baseX_Full) {
			sc->link_params.req_line_speed = SPEED_2500;
			sc->port.advertising = ADVERTISED_2500baseX_Full |
				ADVERTISED_TP;
		} else {
			DBPRINT(sc, BXE_FATAL,
			    "%s(): NVRAM config error. Invalid "
			    "link_config (0x%08X) - speed_cap_mask = 0x%08X\n",
			    __FUNCTION__, sc->port.link_config,
			    sc->link_params.speed_cap_mask);
			goto bxe_link_settings_requested_exit;
		}
		break;
	case PORT_FEATURE_LINK_SPEED_10G_CX4:
	case PORT_FEATURE_LINK_SPEED_10G_KX4:
	case PORT_FEATURE_LINK_SPEED_10G_KR:
		if (sc->port.supported & SUPPORTED_10000baseT_Full) {
			sc->link_params.req_line_speed = SPEED_10000;
			sc->port.advertising = ADVERTISED_10000baseT_Full |
			    ADVERTISED_FIBRE;
		} else {
			DBPRINT(sc, BXE_FATAL,
			    "%s(): NVRAM config error. Invalid "
			    "link_config (0x%08X) - speed_cap_mask = 0x%08X\n",
			    __FUNCTION__, sc->port.link_config,
			    sc->link_params.speed_cap_mask);
			goto bxe_link_settings_requested_exit;
		}
		break;
	default:
		DBPRINT(sc, BXE_FATAL, "%s(): NVRAM config error. BAD link "
		    "speed - link_config = 0x%08X\n", __FUNCTION__,
		    sc->port.link_config);
		sc->link_params.req_line_speed = 0;
		sc->port.advertising = sc->port.supported;
		break;
	}

	DBPRINT(sc, BXE_VERBOSE_PHY,
	    "%s(): req_line_speed = %d, req_duplex = %d\n",
	    __FUNCTION__, sc->link_params.req_line_speed,
	    sc->link_params.req_duplex);

	sc->link_params.req_flow_ctrl =
	    sc->port.link_config & PORT_FEATURE_FLOW_CONTROL_MASK;

	if ((sc->link_params.req_flow_ctrl == FLOW_CTRL_AUTO) &&
	    !(sc->port.supported & SUPPORTED_Autoneg))
		sc->link_params.req_flow_ctrl = FLOW_CTRL_NONE;

	DBPRINT(sc, BXE_VERBOSE_PHY,
		"%s(): req_flow_ctrl = 0x%08X, advertising = 0x%08X\n",
		__FUNCTION__, sc->link_params.req_flow_ctrl,
		sc->port.advertising);

bxe_link_settings_requested_exit:

	DBEXIT(BXE_VERBOSE_PHY);
}


/*
 * Get function specific hardware configuration.
 *
 * Multiple function devices such as the BCM57711E have configuration
 * information that is specific to each PCIe function of the controller.
 * The number of PCIe functions is not necessarily the same as the number
 * of Ethernet ports supported by the device.
 *
 * Returns:
 *   0 = Success, !0 = Failure
 */
static int
bxe_get_function_hwinfo(struct bxe_softc *sc)
{
	uint32_t mac_hi, mac_lo, val;
	int func, rc;

	DBENTER(BXE_VERBOSE_LOAD);

	rc = 0;
	func = BP_FUNC(sc);

	/* Get the common hardware configuration first. */
	bxe_get_common_hwinfo(sc);

	/* Assume no outer VLAN/multi-function support. */
	sc->e1hov = sc->e1hmf = 0;

	/* Get config info for mf enabled devices. */
	if (CHIP_IS_E1H(sc)) {
		sc->mf_config[BP_E1HVN(sc)] =
		    SHMEM_RD(sc, mf_cfg.func_mf_config[func].config);
		val = (SHMEM_RD(sc, mf_cfg.func_mf_config[func].e1hov_tag) &
		    FUNC_MF_CFG_E1HOV_TAG_MASK);
		if (val != FUNC_MF_CFG_E1HOV_TAG_DEFAULT) {
			sc->e1hov = (uint16_t) val;
			sc->e1hmf = 1;
		} else {
			if (BP_E1HVN(sc)) {
				rc = EPERM;
				goto bxe_get_function_hwinfo_exit;
			}
		}
	}

	if (!BP_NOMCP(sc)) {
		bxe_get_port_hwinfo(sc);
		sc->fw_seq = SHMEM_RD(sc, func_mb[func].drv_mb_header) &
		    DRV_MSG_SEQ_NUMBER_MASK;
	}


	/*
	 * Fetch the factory configured MAC address for multi function
	 * devices. If this is not a multi-function device then the MAC
	 * address was already read in the bxe_get_port_hwinfo() routine.
	 * The MAC addresses used by the port are not the same as the MAC
	 * addressed used by the function.
	 */
	if (IS_E1HMF(sc)) {
		mac_hi = SHMEM_RD(sc, mf_cfg.func_mf_config[func].mac_upper);
		mac_lo = SHMEM_RD(sc, mf_cfg.func_mf_config[func].mac_lower);

		if ((mac_lo == 0) && (mac_hi == 0)) {
			BXE_PRINTF("%s(%d): Invalid Ethernet address!\n",
			    __FILE__, __LINE__);
		} else {
			sc->link_params.mac_addr[0] = (u_char)(mac_hi >> 8);
			sc->link_params.mac_addr[1] = (u_char)(mac_hi);
			sc->link_params.mac_addr[2] = (u_char)(mac_lo >> 24);
			sc->link_params.mac_addr[3] = (u_char)(mac_lo >> 16);
			sc->link_params.mac_addr[4] = (u_char)(mac_lo >> 8);
			sc->link_params.mac_addr[5] = (u_char)(mac_lo);
		}
	}


bxe_get_function_hwinfo_exit:
	DBEXIT(BXE_VERBOSE_LOAD);
	return(rc);
}


/*
 * Get port specific hardware configuration.
 *
 * Multiple port devices such as the BCM57710 have configuration
 * information that is specific to each Ethernet port of the
 * controller.  This function reads that configuration
 * information from the bootcode's shared memory and saves it
 * for future use.
 *
 * Returns:
 *   None
 */
static void
bxe_get_port_hwinfo(struct bxe_softc *sc)
{
	int i, port;
	uint32_t val, mac_hi, mac_lo;

	DBENTER(BXE_VERBOSE_LOAD);

	port = BP_PORT(sc);
	sc->link_params.sc = sc;
	sc->link_params.port = port;

	/* Fetch several configuration values from bootcode shared memory. */
	sc->link_params.lane_config =
	    SHMEM_RD(sc, dev_info.port_hw_config[port].lane_config);
	sc->link_params.ext_phy_config =
	    SHMEM_RD(sc, dev_info.port_hw_config[port].external_phy_config);

	if (XGXS_EXT_PHY_TYPE(sc->link_params.ext_phy_config) ==
	    PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727_NOC) {
		sc->link_params.ext_phy_config &=
		    ~PORT_HW_CFG_XGXS_EXT_PHY_TYPE_MASK;
		sc->link_params.ext_phy_config |=
		    PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727;
		sc->link_params.feature_config_flags |=
		    FEATURE_CONFIG_BCM8727_NOC;
	}

	sc->link_params.speed_cap_mask =
	    SHMEM_RD(sc, dev_info.port_hw_config[port].speed_capability_mask);
	sc->port.link_config =
	    SHMEM_RD(sc, dev_info.port_feature_config[port].link_config);


	/* Read the XGXS RX/TX preemphasis values. */
	for (i = 0; i < 2; i++) {
		val = SHMEM_RD(sc,
		    dev_info.port_hw_config[port].xgxs_config_rx[i<<1]);
		sc->link_params.xgxs_config_rx[i << 1] = ((val >> 16) & 0xffff);
		sc->link_params.xgxs_config_rx[(i << 1) + 1] = (val & 0xffff);

		val = SHMEM_RD(sc,
		    dev_info.port_hw_config[port].xgxs_config_tx[i<<1]);
		sc->link_params.xgxs_config_tx[i << 1] = ((val >> 16) & 0xffff);
		sc->link_params.xgxs_config_tx[(i << 1) + 1] = (val & 0xffff);
	}

	/* Fetch the device configured link settings. */
	sc->link_params.switch_cfg = sc->port.link_config &
	    PORT_FEATURE_CONNECTED_SWITCH_MASK;

	bxe_link_settings_supported(sc, sc->link_params.switch_cfg);
	bxe_link_settings_requested(sc);

	mac_hi = SHMEM_RD(sc, dev_info.port_hw_config[port].mac_upper);
	mac_lo = SHMEM_RD(sc, dev_info.port_hw_config[port].mac_lower);

	if (mac_lo == 0 && mac_hi == 0) {
		BXE_PRINTF("%s(%d): No Ethernet address programmed on the "
		    "controller!\n", __FILE__, __LINE__);
	} else {
		sc->link_params.mac_addr[0] = (u_char)(mac_hi >> 8);
		sc->link_params.mac_addr[1] = (u_char)(mac_hi);
		sc->link_params.mac_addr[2] = (u_char)(mac_lo >> 24);
		sc->link_params.mac_addr[3] = (u_char)(mac_lo >> 16);
		sc->link_params.mac_addr[4] = (u_char)(mac_lo >> 8);
		sc->link_params.mac_addr[5] = (u_char)(mac_lo);
	}

	DBEXIT(BXE_VERBOSE_LOAD);
}


/*
 * Get common hardware configuration.
 *
 * Multiple port devices such as the BCM57710 have configuration
 * information that is specific to each Ethernet port of the controller.
 *
 * Returns:
 *   None
 */
static void
bxe_get_common_hwinfo(struct bxe_softc *sc)
{
	uint32_t val;

	DBENTER(BXE_VERBOSE_LOAD);

	/* Get the chip revision. */
	sc->common.chip_id = sc->link_params.chip_id =
	    ((REG_RD(sc, MISC_REG_CHIP_NUM) & 0xffff) << 16) |
	    ((REG_RD(sc, MISC_REG_CHIP_REV) & 0x000f) << 12) |
	    ((REG_RD(sc, MISC_REG_CHIP_METAL) & 0xff) << 4) |
	    ((REG_RD(sc, MISC_REG_BOND_ID) & 0xf));

	DBPRINT(sc, BXE_VERBOSE_LOAD, "%s(): chip_id = 0x%08X.\n",
	    __FUNCTION__, sc->common.chip_id);

	val = (REG_RD(sc, 0x2874) & 0x55);
	if ((sc->common.chip_id & 0x1) ||
	    (CHIP_IS_E1(sc) && val) || (CHIP_IS_E1H(sc) && (val == 0x55))) {
		sc->bxe_flags |= BXE_ONE_PORT_FLAG;
		DBPRINT(sc, BXE_VERBOSE_LOAD, "%s(): Single port device.\n",
		    __FUNCTION__);
	}

	/* Identify enabled PCI capabilites (PCIe, MSI-X, etc.). */
	bxe_probe_pci_caps(sc);

	/* Get the NVRAM size. */
	val = REG_RD(sc, MCP_REG_MCPR_NVM_CFG4);
	sc->common.flash_size = (NVRAM_1MB_SIZE <<
	    (val & MCPR_NVM_CFG4_FLASH_SIZE));

	DBPRINT(sc, BXE_VERBOSE_LOAD, "%s(): flash_size = 0x%08x (%dKB)\n",
	    __FUNCTION__, sc->common.flash_size,(sc->common.flash_size >> 10));

	/* Find the shared memory base address. */
	sc->common.shmem_base = sc->link_params.shmem_base =
	    REG_RD(sc, MISC_REG_SHARED_MEM_ADDR);
	sc->common.shmem2_base = REG_RD(sc, MISC_REG_GENERIC_CR_0);
	DBPRINT(sc, BXE_VERBOSE_LOAD, "%s(): shmem_base = 0x%08X\n",
	    __FUNCTION__, sc->common.shmem_base);

	/* Make sure the shared memory address is valid. */
	if (!sc->common.shmem_base ||
	    (sc->common.shmem_base < 0xA0000) ||
	    (sc->common.shmem_base > 0xC0000)) {

		DBPRINT(sc, BXE_FATAL, "%s(): MCP is not active!\n",
		    __FUNCTION__);
		sc->bxe_flags |= BXE_NO_MCP_FLAG;
		goto bxe_get_common_hwinfo_exit;
	}

	/* Make sure the shared memory contents are valid. */
	val = SHMEM_RD(sc, validity_map[BP_PORT(sc)]);
	if ((val & (SHR_MEM_VALIDITY_DEV_INFO | SHR_MEM_VALIDITY_MB)) !=
	    (SHR_MEM_VALIDITY_DEV_INFO | SHR_MEM_VALIDITY_MB)) {
		BXE_PRINTF("%s(%d): Invalid NVRAM! Bad validity "
		    "signature.\n", __FILE__, __LINE__);
		goto bxe_get_common_hwinfo_exit;
	}

	/* Read the device configuration from shared memory. */
	sc->common.hw_config =
	    SHMEM_RD(sc, dev_info.shared_hw_config.config);
	sc->link_params.hw_led_mode = ((sc->common.hw_config &
	    SHARED_HW_CFG_LED_MODE_MASK) >> SHARED_HW_CFG_LED_MODE_SHIFT);

	/* Check if we need to override the preemphasis values. */
	sc->link_params.feature_config_flags = 0;
	val = SHMEM_RD(sc, dev_info.shared_feature_config.config);
	if (val & SHARED_FEAT_CFG_OVERRIDE_PREEMPHASIS_CFG_ENABLED)
		sc->link_params.feature_config_flags |=
		    FEATURE_CONFIG_OVERRIDE_PREEMPHASIS_ENABLED;
	else
		sc->link_params.feature_config_flags &=
		    ~FEATURE_CONFIG_OVERRIDE_PREEMPHASIS_ENABLED;

	/* In multifunction mode, we can't support WoL on a VN. */
	if (BP_E1HVN(sc) == 0) {
		val = REG_RD(sc, PCICFG_OFFSET + PCICFG_PM_CAPABILITY);
		sc->bxe_flags |= (val & PCICFG_PM_CAPABILITY_PME_IN_D3_COLD) ?
			0 : BXE_NO_WOL_FLAG;
	} else
		sc->bxe_flags |= BXE_NO_WOL_FLAG;

	DBPRINT(sc, BXE_VERBOSE_LOAD, "%s(): %sWoL capable\n", __FUNCTION__,
		(sc->bxe_flags & BXE_NO_WOL_FLAG) ? "Not " : "");

	/* Check bootcode version */
	sc->common.bc_ver = ((SHMEM_RD(sc, dev_info.bc_rev)) >> 8);
	if (sc->common.bc_ver < MIN_BXE_BC_VER) {
		BXE_PRINTF("%s(%d): Warning: This driver needs bootcode "
		    "0x%08X but found 0x%08X, please upgrade!\n",
		    __FILE__, __LINE__,	MIN_BXE_BC_VER, sc->common.bc_ver);
		goto bxe_get_common_hwinfo_exit;
	}

bxe_get_common_hwinfo_exit:
	DBEXIT(BXE_VERBOSE_LOAD);
}


/*
 * Remove traces of PXE boot by forcing UNDI driver unload.
 *
 * Returns:
 *   None.
 */
static void
bxe_undi_unload(struct bxe_softc *sc)
{
	uint32_t reset_code, swap_en, swap_val, val;
	int func;

	DBENTER(BXE_VERBOSE_LOAD);

	/* Check if there is any driver already loaded */
	val = REG_RD(sc, MISC_REG_UNPREPARED);
	if (val == 0x1) {

		/* Check if it is the UNDI driver. */
		bxe_acquire_hw_lock(sc, HW_LOCK_RESOURCE_UNDI);
		val = REG_RD(sc, DORQ_REG_NORM_CID_OFST);
		if (val == 0x7) {
			reset_code = DRV_MSG_CODE_UNLOAD_REQ_WOL_DIS;
			func = BP_FUNC(sc);

			DBPRINT(sc, BXE_WARN,
			    "%s(): UNDI is active! Resetting the device.\n",
			    __FUNCTION__);

			/* Clear the UNDI indication. */
			REG_WR(sc, DORQ_REG_NORM_CID_OFST, 0);

			/* Try to unload UNDI on port 0. */
			sc->bxe_func = 0;
			sc->fw_seq = (SHMEM_RD(sc,
			    func_mb[sc->bxe_func].drv_mb_header) &
			    DRV_MSG_SEQ_NUMBER_MASK);
			reset_code = bxe_fw_command(sc, reset_code);

			/* Check if UNDI is active on port 1. */
			if (reset_code != FW_MSG_CODE_DRV_UNLOAD_COMMON) {

				/* Send "done" for previous unload. */
				bxe_fw_command(sc, DRV_MSG_CODE_UNLOAD_DONE);

				/* Now unload on port 1. */
				sc->bxe_func = 1;
				sc->fw_seq = (SHMEM_RD(sc,
				    func_mb[sc->bxe_func].drv_mb_header) &
				    DRV_MSG_SEQ_NUMBER_MASK);

				reset_code = DRV_MSG_CODE_UNLOAD_REQ_WOL_DIS;
				bxe_fw_command(sc, reset_code);
			}

			/* It's	now safe to release the lock. */
			bxe_release_hw_lock(sc, HW_LOCK_RESOURCE_UNDI);

			REG_WR(sc, (BP_PORT(sc) ? HC_REG_CONFIG_1 :
				HC_REG_CONFIG_0), 0x1000);

			REG_WR(sc, (BP_PORT(sc) ?
			    NIG_REG_LLH1_BRB1_DRV_MASK :
			    NIG_REG_LLH0_BRB1_DRV_MASK), 0x0);

			REG_WR(sc, (BP_PORT(sc) ?
			    NIG_REG_LLH1_BRB1_NOT_MCP :
			    NIG_REG_LLH0_BRB1_NOT_MCP), 0x0);

			/* Clear AEU. */
			REG_WR(sc, (BP_PORT(sc) ?
			    MISC_REG_AEU_MASK_ATTN_FUNC_1 :
			    MISC_REG_AEU_MASK_ATTN_FUNC_0), 0);

			DELAY(10000);

			/* Save NIG port swap information. */
			swap_val = REG_RD(sc, NIG_REG_PORT_SWAP);
			swap_en = REG_RD(sc, NIG_REG_STRAP_OVERRIDE);

			/* Reset the controller. */
			REG_WR(sc, GRCBASE_MISC +
			    MISC_REGISTERS_RESET_REG_1_CLEAR, 0xd3ffffff);
			REG_WR(sc, GRCBASE_MISC +
			    MISC_REGISTERS_RESET_REG_2_CLEAR, 0x00001403);

			/* Take the NIG out of reset and restore swap values.*/
			REG_WR(sc, GRCBASE_MISC +
			    MISC_REGISTERS_RESET_REG_1_SET,
			    MISC_REGISTERS_RESET_REG_1_RST_NIG);
			REG_WR(sc, NIG_REG_PORT_SWAP, swap_val);
			REG_WR(sc, NIG_REG_STRAP_OVERRIDE, swap_en);

			/* Send completion message to the MCP. */
			bxe_fw_command(sc, DRV_MSG_CODE_UNLOAD_DONE);

			/*
			 * Restore our function and firmware sequence counter.
			 */
			sc->bxe_func = func;
			sc->fw_seq = (SHMEM_RD(sc,
			    func_mb[sc->bxe_func].drv_mb_header) &
			    DRV_MSG_SEQ_NUMBER_MASK);
		} else
			bxe_release_hw_lock(sc, HW_LOCK_RESOURCE_UNDI);
	}

	DBEXIT(BXE_VERBOSE_LOAD);
}


/*
 * Device detach function.
 *
 * Stops the controller, resets the controller, and releases resources.
 *
 * Returns:
 *   0 on success, positive value on failure.
 */
static int
bxe_detach(device_t dev)
{
	struct bxe_softc *sc;
	struct ifnet *ifp;
#ifdef BXE_TASK
	struct bxe_fastpath *fp;
	int i;
#endif

	sc = device_get_softc(dev);
	DBENTER(BXE_VERBOSE_RESET);

	ifp = sc->bxe_ifp;
	if (ifp != NULL && ifp->if_vlantrunk != NULL) {
		BXE_PRINTF("%s(%d): Cannot detach while VLANs are in use.\n",
		    __FILE__, __LINE__);
		return(EBUSY);
	}

	/* Stop and reset the controller if it was open. */
	if (sc->state != BXE_STATE_CLOSED) {
		BXE_CORE_LOCK(sc);
		bxe_stop_locked(sc, UNLOAD_CLOSE);
		BXE_CORE_UNLOCK(sc);
	}

#ifdef BXE_TASK
	/* Free the OS taskqueue resources. */
	for (i = 0; i < sc->num_queues; i++) {
		fp = &sc->fp[i];

		if (fp->tq) {
			taskqueue_drain(fp->tq, &fp->task);
			taskqueue_free(fp->tq);
		}
	}

	if (sc->tq) {
		taskqueue_drain(sc->tq, &sc->task);
		taskqueue_free(sc->tq);
	}
#endif
	/* Release the network interface. */
	if (ifp != NULL)
		ether_ifdetach(ifp);
	ifmedia_removeall(&sc->bxe_ifmedia);

	/* Release all remaining resources. */
	bxe_release_resources(sc);
	pci_disable_busmaster(dev);

	return(0);
}


/*
 * Setup a leading connection for the controller.
 *
 * Returns:
 *   0 = Success, !0 = Failure.
 */
static int
bxe_setup_leading(struct bxe_softc *sc)
{
	int rc;

	DBENTER(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET | BXE_VERBOSE_RAMROD);

	DBPRINT(sc, BXE_VERBOSE_LOAD, "%s(): Setup leading connection "
	    "on fp[00].\n", __FUNCTION__);

	/* Reset IGU state for the leading connection. */
	bxe_ack_sb(sc, sc->fp[0].sb_id, CSTORM_ID, 0, IGU_INT_ENABLE, 0);

	/* Post a PORT_SETUP ramrod and wait for completion. */
	bxe_sp_post(sc, RAMROD_CMD_ID_ETH_PORT_SETUP, 0, 0, 0, 0);

	/* Wait for the ramrod to complete on the leading connection. */
	rc = bxe_wait_ramrod(sc, BXE_STATE_OPEN, 0, &(sc->state), 1);

	DBEXIT(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET | BXE_VERBOSE_RAMROD);
	return (rc);
}


/*
 * Stop the leading connection on the controller.
 *
 * Returns:
 *   None.
 */
static int
bxe_stop_leading(struct bxe_softc *sc)
{
	uint16_t dsb_sp_prod_idx;
	int rc, timeout;

	DBENTER(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET | BXE_VERBOSE_RAMROD);

	DBPRINT(sc, BXE_VERBOSE_LOAD, "%s(): Stop client connection "
	    "on fp[00].\n", __FUNCTION__);

	/* Send the ETH_HALT ramrod. */
	sc->fp[0].state = BXE_FP_STATE_HALTING;
	bxe_sp_post(sc,RAMROD_CMD_ID_ETH_HALT, 0, 0, sc->fp[0].cl_id, 0);

	/* Poll for the ETH_HALT ramrod on the leading connection. */
	rc = bxe_wait_ramrod(sc, BXE_FP_STATE_HALTED, 0, &(sc->fp[0].state), 1);
	if (rc)
		goto bxe_stop_leading_exit;

	dsb_sp_prod_idx = *sc->dsb_sp_prod;

	/*
	 * Now that the connection is in the
	 * HALTED state send PORT_DELETE ramrod.
	 */
	bxe_sp_post(sc, RAMROD_CMD_ID_ETH_PORT_DEL, 0, 0, 0, 1);

	/*
	 * Wait for completion.  This can take a * long time if the other port
	 * is busy. Give the command some time to complete but don't wait for a
	 * completion since there's nothing we can do.
	 */
	timeout = 500;
	while (dsb_sp_prod_idx == *sc->dsb_sp_prod) {
		if (!timeout) {
			DBPRINT(sc, BXE_FATAL, "%s(): Timeout waiting for "
			    "PORT_DEL ramrod completion!\n", __FUNCTION__);
			rc = EBUSY;
			break;
		}
		timeout--;
		DELAY(1000);
		rmb();
	}

	/* Update the adapter and connection states. */
	sc->state = BXE_STATE_CLOSING_WAIT4_UNLOAD;
	sc->fp[0].state = BXE_FP_STATE_CLOSED;

bxe_stop_leading_exit:
	DBEXIT(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET | BXE_VERBOSE_RAMROD);
	return(rc);
}

/*
 * Setup a client connection when using multi-queue/RSS.
 *
 * Returns:
 *   Nothing.
 */
static int
bxe_setup_multi(struct bxe_softc *sc, int index)
{
	struct bxe_fastpath *fp;
	int rc;

	DBENTER(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET | BXE_VERBOSE_RAMROD);

	DBPRINT(sc, BXE_VERBOSE_LOAD, "%s(): Setup client connection "
	    "on fp[%02d].\n", __FUNCTION__, index);

	fp = &sc->fp[index];
	/* Reset IGU state. */
	bxe_ack_sb(sc, fp->sb_id, CSTORM_ID, 0, IGU_INT_ENABLE, 0);

	/* Post a CLIENT_SETUP ramrod. */
	fp->state = BXE_FP_STATE_OPENING;
	bxe_sp_post(sc, RAMROD_CMD_ID_ETH_CLIENT_SETUP, index, 0, fp->cl_id, 0);

	/* Wait for the ramrod to complete. */
	rc = bxe_wait_ramrod(sc, BXE_FP_STATE_OPEN, index, &(fp->state), 1);

	DBEXIT(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET | BXE_VERBOSE_RAMROD);
	return(rc);
}

/*
 * Stop a client connection.
 *
 * Stops an individual client connection on the device.  Use
 * bxe_stop_leading() for the first/default connection.
 *
 * Returns:
 *   0 = Success, !0 = Failure.
 */
static int
bxe_stop_multi(struct bxe_softc *sc, int index)
{
	struct bxe_fastpath *fp;
	int rc;

	DBENTER(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET | BXE_VERBOSE_RAMROD);

	DBPRINT(sc, BXE_VERBOSE_LOAD, "%s(): Stop client connection "
	    "on fp[%02d].\n", __FUNCTION__, index);

	fp = &sc->fp[index];

	/* Halt the client connection. */
	fp->state = BXE_FP_STATE_HALTING;
	bxe_sp_post(sc, RAMROD_CMD_ID_ETH_HALT, index, 0, fp->cl_id, 0);

	/* Wait for the ramrod completion. */
	rc = bxe_wait_ramrod(sc, BXE_FP_STATE_HALTED, index, &(fp->state), 1);
	if (rc){
		BXE_PRINTF("%s(%d): fp[%02d] client ramrod halt failed!\n",
		    __FILE__, __LINE__, index);
		goto bxe_stop_multi_exit;
	}
	/* Delete the CFC entry. */
	bxe_sp_post(sc, RAMROD_CMD_ID_ETH_CFC_DEL, index, 0, 0, 1);

	/* Poll for the ramrod completion. */
	rc = bxe_wait_ramrod(sc, BXE_FP_STATE_CLOSED, index, &(fp->state), 1);

bxe_stop_multi_exit:
	DBEXIT(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET | BXE_VERBOSE_RAMROD);
	return(rc);
}

/*
 * Hardware lock for shared, dual-port PHYs.
 *
 * Returns:
 *   None.
 */
static void
bxe_acquire_phy_lock(struct bxe_softc *sc)
{
	uint32_t ext_phy_type;

	DBENTER(BXE_VERBOSE_PHY);

	ext_phy_type = XGXS_EXT_PHY_TYPE(sc->link_params.ext_phy_config);
	switch(ext_phy_type){
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8072:
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8073:
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8726:
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727:
			bxe_acquire_hw_lock(sc, HW_LOCK_RESOURCE_MDIO);
			break;
		default:
			break;
	}
	DBEXIT(BXE_VERBOSE_PHY);
}

/*
 * Hardware unlock for shared, dual-port PHYs.
 *
 * Returns:
 *   None.
 */
static void
bxe_release_phy_lock(struct bxe_softc *sc)
{
	uint32_t ext_phy_type;

	DBENTER(BXE_VERBOSE_PHY);
	ext_phy_type = XGXS_EXT_PHY_TYPE(sc->link_params.ext_phy_config);
	switch(ext_phy_type){
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8072:
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8073:
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8726:
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727:
			bxe_release_hw_lock(sc, HW_LOCK_RESOURCE_MDIO);
			break;
		default:
			break;
	}

	DBEXIT(BXE_VERBOSE_PHY);
}

/*
 *
 * Returns:
 *   None.
 */
static void
bxe__link_reset(struct bxe_softc *sc)
{
	DBENTER(BXE_VERBOSE_PHY);

	if (!BP_NOMCP(sc)) {
		bxe_acquire_phy_lock(sc);
		bxe_link_reset(&sc->link_params, &sc->link_vars, 1);
		bxe_release_phy_lock(sc);
	} else {
		DBPRINT(sc, BXE_WARN,
		    "%s(): Bootcode is not running, not resetting link!\n",
		    __FUNCTION__);
	}

	DBEXIT(BXE_VERBOSE_PHY);
}

/*
 * Stop the controller.
 *
 * Returns:
 *   Nothing.
 */
static int
bxe_stop_locked(struct bxe_softc *sc, int unload_mode)
{
	struct ifnet *ifp;
	struct mac_configuration_cmd *config;
	struct bxe_fastpath *fp;
	uint32_t reset_code;
	uint32_t emac_base, val;
	uint8_t entry, *mac_addr;
	int count, i, port, rc;

	DBENTER(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET | BXE_VERBOSE_UNLOAD);
	ifp = sc->bxe_ifp;
	port = BP_PORT(sc),
	reset_code = 0;
	rc = 0;

	/* Stop the periodic tick. */
	callout_stop(&sc->bxe_tick_callout);

	sc->state = BXE_STATE_CLOSING_WAIT4_HALT;

	/* Stop receiving all types of Ethernet traffic. */
	sc->rx_mode = BXE_RX_MODE_NONE;
	bxe_set_storm_rx_mode(sc);

	/* Tell the stack the driver is stopped and TX queue is full. */
	if (ifp != NULL)
		ifp->if_drv_flags = 0;

	/* Tell the bootcode to stop watching for a heartbeat. */
	SHMEM_WR(sc, func_mb[BP_FUNC(sc)].drv_pulse_mb,
	    (DRV_PULSE_ALWAYS_ALIVE | sc->fw_drv_pulse_wr_seq));
	/* Stop the statistics updates. */
	bxe_stats_handle(sc, STATS_EVENT_STOP);

	/* Wait until all TX fastpath tasks have completed. */
	for (i = 0; i < sc->num_queues; i++) {
		fp = &sc->fp[i];

		count = 1000;
		while (bxe_has_tx_work(fp)) {

			bxe_txeof(fp);

			if (count == 0) {
				BXE_PRINTF(
		"%s(%d): Timeout wating for fp[%d] transmits to complete!\n",
				    __FILE__, __LINE__, i);
				break;
			}
			count--;
			DELAY(1000);
			rmb();
		}
	}

	/* Wait until all slowpath tasks have completed. */
	count = 1000;
	while ((sc->spq_left != MAX_SPQ_PENDING) && count--)
		DELAY(1000);

	/* Disable Interrupts */
	bxe_int_disable(sc);

	DELAY(1000);
	/* Clear the MAC addresses. */
	if (CHIP_IS_E1(sc)) {
		config = BXE_SP(sc, mcast_config);
		bxe_set_mac_addr_e1(sc, 0);

		for (i = 0; i < config->hdr.length; i++)
			CAM_INVALIDATE(&config->config_table[i]);

		config->hdr.length = i;
		config->hdr.offset = BXE_MAX_MULTICAST * (1 + port);
		config->hdr.client_id = BP_CL_ID(sc);
		config->hdr.reserved1 = 0;

		bxe_sp_post(sc, RAMROD_CMD_ID_ETH_SET_MAC, 0,
		    U64_HI(BXE_SP_MAPPING(sc, mcast_config)),
		    U64_LO(BXE_SP_MAPPING(sc, mcast_config)), 0);
	} else {
		REG_WR(sc, NIG_REG_LLH0_FUNC_EN + port * 8, 0);
		bxe_set_mac_addr_e1h(sc, 0);
		for (i = 0; i < MC_HASH_SIZE; i++)
			REG_WR(sc, MC_HASH_OFFSET(sc, i), 0);
		REG_WR(sc, MISC_REG_E1HMF_MODE, 0);
	}
	/* Determine if any WoL settings needed. */
	if (unload_mode == UNLOAD_NORMAL)
		reset_code = DRV_MSG_CODE_UNLOAD_REQ_WOL_DIS;
	else if (sc->bxe_flags & BXE_NO_WOL_FLAG) {
		/* Driver initiated WoL is disabled, use OOB WoL settings. */
		reset_code = DRV_MSG_CODE_UNLOAD_REQ_WOL_MCP;
		if (CHIP_IS_E1H(sc))
			REG_WR(sc, MISC_REG_E1HMF_MODE, 0);
	} else if (sc->wol) {
		emac_base = BP_PORT(sc) ?  GRCBASE_EMAC0 : GRCBASE_EMAC1;
		mac_addr = sc->link_params.mac_addr;
		entry = (BP_E1HVN(sc) + 1) * 8;
		val = (mac_addr[0] << 8) | mac_addr[1];
		EMAC_WR(sc, EMAC_REG_EMAC_MAC_MATCH + entry, val);
		val = (mac_addr[2] << 24) | (mac_addr[3] << 16) |
		    (mac_addr[4] << 8) | mac_addr[5];
		EMAC_WR(sc, EMAC_REG_EMAC_MAC_MATCH + entry + 4, val);
		reset_code = DRV_MSG_CODE_UNLOAD_REQ_WOL_EN;
	} else {
		/* Prevent WoL. */
		reset_code = DRV_MSG_CODE_UNLOAD_REQ_WOL_DIS;
	}
	/* Stop all non-leading client connections. */
	for (i = 1; i < sc->num_queues; i++) {
		if (bxe_stop_multi(sc, i)){
			goto bxe_stop_locked_exit;
		}
	}
	/* Stop the leading client connection. */
	rc = bxe_stop_leading(sc);
	if (rc) {
#ifdef BXE_DEBUG
		if ((sc->state != BXE_STATE_CLOSING_WAIT4_UNLOAD) ||
		    (sc->fp[0].state != BXE_FP_STATE_CLOSED)) {
			BXE_PRINTF("%s(%d): Failed to close leading "
			    "client connection!\n", __FILE__, __LINE__);
		}
#endif
	}

	DELAY(10000);

bxe_stop_locked_exit:

	if (BP_NOMCP(sc)) {
		DBPRINT(sc, BXE_INFO,
		    "%s(): Old No MCP load counts:  %d, %d, %d\n", __FUNCTION__,
		    load_count[0], load_count[1], load_count[2]);

		load_count[0]--;
		load_count[1 + port]--;
		DBPRINT(sc, BXE_INFO,
		    "%s(): New No MCP load counts:  %d, %d, %d\n", __FUNCTION__,
		    load_count[0], load_count[1], load_count[2]);

		if (load_count[0] == 0)
			reset_code = FW_MSG_CODE_DRV_UNLOAD_COMMON;
		else if (load_count[1 + BP_PORT(sc)] == 0)
			reset_code = FW_MSG_CODE_DRV_UNLOAD_PORT;
		else
			reset_code = FW_MSG_CODE_DRV_UNLOAD_FUNCTION;
	} else {
		/* Tell MCP driver unload is complete. */
		reset_code = bxe_fw_command(sc, reset_code);
	}

	if ((reset_code == FW_MSG_CODE_DRV_UNLOAD_COMMON) ||
	    (reset_code == FW_MSG_CODE_DRV_UNLOAD_PORT))
		bxe__link_reset(sc);

	DELAY(10000);

	/* Reset the chip */
	bxe_reset_chip(sc, reset_code);

	DELAY(10000);

	/* Report UNLOAD_DONE to MCP */
	if (!BP_NOMCP(sc))
		bxe_fw_command(sc, DRV_MSG_CODE_UNLOAD_DONE);
	sc->port.pmf = 0;

	/* Free RX chains and buffers. */
	bxe_free_rx_chains(sc);

	/* Free TX chains and buffers. */
	bxe_free_tx_chains(sc);

	sc->state = BXE_STATE_CLOSED;

	bxe_ack_int(sc);

	DBEXIT(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET |BXE_VERBOSE_UNLOAD);
	return(rc);
}


/*
 * Device shutdown function.
 *
 * Stops and resets the controller.
 *
 * Returns:
 *   Nothing
 */
static int
bxe_shutdown(device_t dev)
{
	struct bxe_softc *sc;

	sc = device_get_softc(dev);
	DBENTER(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET | BXE_VERBOSE_UNLOAD);

	BXE_CORE_LOCK(sc);
	bxe_stop_locked(sc, UNLOAD_NORMAL);
	BXE_CORE_UNLOCK(sc);

	DBEXIT(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET | BXE_VERBOSE_UNLOAD);
	return (0);
}

/*
 * Prints out link speed and duplex setting to console.
 *
 * Returns:
 *   None.
 */
static void
bxe_link_report(struct bxe_softc *sc)
{
	uint32_t line_speed;
	uint16_t vn_max_rate;

	DBENTER(BXE_VERBOSE_PHY);

	if (sc->link_vars.link_up) {
		/* Report the link status change to OS. */
		if (sc->state == BXE_STATE_OPEN)
			if_link_state_change(sc->bxe_ifp, LINK_STATE_UP);

		line_speed = sc->link_vars.line_speed;

		if (IS_E1HMF(sc)){
			vn_max_rate = ((sc->mf_config[BP_E1HVN(sc)] &
			    FUNC_MF_CFG_MAX_BW_MASK) >>
			    FUNC_MF_CFG_MAX_BW_SHIFT) * 100;
			if (vn_max_rate < line_speed)
				line_speed = vn_max_rate;
		}

		BXE_PRINTF("Link is up, %d Mbps, ", line_speed);

		if (sc->link_vars.duplex == MEDIUM_FULL_DUPLEX)
			printf("full duplex");
		else
			printf("half duplex");

		if (sc->link_vars.flow_ctrl) {
			if (sc->link_vars.flow_ctrl & FLOW_CTRL_RX) {
				printf(", receive ");
				if (sc->link_vars.flow_ctrl & FLOW_CTRL_TX)
					printf("& transmit ");
			} else
				printf(", transmit ");
			printf("flow control ON");
		}
		printf("\n");
	} else {
		/* Report the link down */
		BXE_PRINTF("Link is down\n");
		if_link_state_change(sc->bxe_ifp, LINK_STATE_DOWN);
	}

	DBEXIT(BXE_VERBOSE_PHY);
}

/*
 *
 * Returns:
 *   None.
 */
static void
bxe__link_status_update(struct bxe_softc *sc)
{
	DBENTER(BXE_VERBOSE_PHY);

	if (sc->stats_enable == FALSE || sc->state != BXE_STATE_OPEN)
		return;

	bxe_link_status_update(&sc->link_params, &sc->link_vars);

	if (sc->link_vars.link_up)
		bxe_stats_handle(sc, STATS_EVENT_LINK_UP);
	else
		bxe_stats_handle(sc, STATS_EVENT_STOP);
	bxe_read_mf_cfg(sc);
	/* Indicate link status. */
	bxe_link_report(sc);

	DBEXIT(BXE_VERBOSE_PHY);
}

/*
 * Calculate flow control to advertise during autonegotiation.
 *
 * Returns:
 *   None.
 */
static void
bxe_calc_fc_adv(struct bxe_softc *sc)
{
	DBENTER(BXE_EXTREME_PHY);

	switch (sc->link_vars.ieee_fc &
	    MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_MASK) {

	case MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_NONE:
		sc->port.advertising &= ~(ADVERTISED_Asym_Pause |
		    ADVERTISED_Pause);
		break;

	case MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_BOTH:
		sc->port.advertising |= (ADVERTISED_Asym_Pause |
		    ADVERTISED_Pause);
		break;

	case MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_ASYMMETRIC:
		sc->port.advertising |= ADVERTISED_Asym_Pause;
		break;

	default:
		sc->port.advertising &= ~(ADVERTISED_Asym_Pause |
		    ADVERTISED_Pause);
		break;
	}

	DBEXIT(BXE_EXTREME_PHY);
}



/*
 *
 * Returns:
 *
 */
static uint8_t
bxe_initial_phy_init(struct bxe_softc *sc)
{
	uint8_t rc;

	DBENTER(BXE_VERBOSE_PHY);

	rc = 0;
	if (!BP_NOMCP(sc)) {

		/*
		 * It is recommended to turn off RX flow control for 5771x
		 * when using jumbo frames for better performance.
		 */
		if (!IS_E1HMF(sc) && (sc->mbuf_alloc_size > 5000))
			sc->link_params.req_fc_auto_adv = FLOW_CTRL_TX;
		else
			sc->link_params.req_fc_auto_adv = FLOW_CTRL_BOTH;

		bxe_acquire_phy_lock(sc);
		rc = bxe_phy_init(&sc->link_params, &sc->link_vars);
		bxe_release_phy_lock(sc);

		bxe_calc_fc_adv(sc);
		if (sc->link_vars.link_up) {
		    bxe_stats_handle(sc,STATS_EVENT_LINK_UP);
		    bxe_link_report(sc);
		}

	} else {
		DBPRINT(sc, BXE_FATAL, "%s(): Bootcode is not running, "
		    "not initializing link!\n",	__FUNCTION__);
		rc = EINVAL;
	}

	DBEXIT(BXE_VERBOSE_PHY);
	return (rc);
}


#if __FreeBSD_version >= 800000
/*
 * Allocate buffer rings used for multiqueue.
 *
 * Returns:
 *   0 = Success, !0 = Failure.
 */
static int
bxe_alloc_buf_rings(struct bxe_softc *sc)
{
	struct bxe_fastpath *fp;
	int i, rc = 0;

	DBENTER(BXE_VERBOSE_LOAD);

	for (i = 0; i < sc->num_queues; i++) {
		fp = &sc->fp[i];

		if (fp != NULL) {
			fp->br = buf_ring_alloc(BXE_BR_SIZE,
			    M_DEVBUF, M_WAITOK,	&fp->mtx);
			if (fp->br == NULL) {
				rc = ENOMEM;
				return(rc);
			}
		} else
			BXE_PRINTF("%s(%d): Bug!\n", __FILE__, __LINE__);
	}

	DBEXIT(BXE_VERBOSE_LOAD);
	return(rc);
}

/*
 * Releases buffer rings used for multiqueue.
 *
 * Returns:
 *   None
 */
static void
bxe_free_buf_rings(struct bxe_softc *sc)
{
	struct bxe_fastpath *fp;
	int i;

	DBENTER(BXE_VERBOSE_UNLOAD);

	for (i = 0; i < sc->num_queues; i++) {
		fp = &sc->fp[i];
		if (fp != NULL) {
			if (fp->br != NULL)
				buf_ring_free(fp->br, M_DEVBUF);
		}
	}

	DBEXIT(BXE_VERBOSE_UNLOAD);
}
#endif


/*
 * Handles controller initialization.
 *
 * Must be called from a locked routine.  Since this code
 * may be called from the OS it does not provide a return
 * error value and must clean-up it's own mess.
 *
 * Returns:
 *   Nothing.
 */
static void
bxe_init_locked(struct bxe_softc *sc, int load_mode)
{
	struct ifnet *ifp;
	uint32_t load_code;
	int i, port;

	DBENTER(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);

	BXE_CORE_LOCK_ASSERT(sc);

	ifp = sc->bxe_ifp;
	/* Skip if we're in panic mode. */
	if (sc->panic) {
		DBPRINT(sc, BXE_WARN, "%s(): Panic mode enabled, exiting!\n",
		    __FUNCTION__);
		goto bxe_init_locked_exit;
	}

	/* Check if the driver is still running and bail out if it is. */
	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		DBPRINT(sc, BXE_INFO,
		    "%s(): Init called while driver is running!\n",
		    __FUNCTION__);
		goto bxe_init_locked_exit;
	}

	/*
	 * Send LOAD_REQUEST command to MCP.
	 * The MCP will return the type of LOAD
	 * the driver should perform.
	 * - If it is the first port to be initialized
	 *   then all common blocks should be initialized.
	 * - If it is not the first port to be initialized
	 *   then don't do the common block initialization.
	 */
	sc->state = BXE_STATE_OPENING_WAIT4_LOAD;

	if (BP_NOMCP(sc)) {
		port = BP_PORT(sc);

		DBPRINT(sc, BXE_INFO,
		    "%s(): Old No MCP load counts:  %d, %d, %d\n",
		    __FUNCTION__,
		    load_count[0], load_count[1], load_count[2]);

		load_count[0]++;
		load_count[1 + port]++;

		DBPRINT(sc, BXE_INFO,
		    "%s(): New No MCP load counts:  %d, %d, %d\n",
		    __FUNCTION__,
		    load_count[0], load_count[1], load_count[2]);

		/* No MCP to tell us what to do. */
		if (load_count[0] == 1)
			load_code = FW_MSG_CODE_DRV_LOAD_COMMON;
		else if (load_count[1 + port] == 1)
			load_code = FW_MSG_CODE_DRV_LOAD_PORT;
		else
			load_code = FW_MSG_CODE_DRV_LOAD_FUNCTION;

	} else {
		/* Ask the MCP what type of initialization we need to do. */
		load_code = bxe_fw_command(sc, DRV_MSG_CODE_LOAD_REQ);

		if ((load_code == 0) ||
		    (load_code == FW_MSG_CODE_DRV_LOAD_REFUSED)) {
			BXE_PRINTF("%s(%d): Bootcode refused load request.!\n",
			    __FILE__, __LINE__);
			goto bxe_init_locked_failed1;
		}
	}

	/* Keep track of whether we are controlling the port. */
	if ((load_code == FW_MSG_CODE_DRV_LOAD_COMMON) ||
	    (load_code == FW_MSG_CODE_DRV_LOAD_PORT))
		sc->port.pmf = 1;
	else
		sc->port.pmf = 0;

	/* Block any interrupts until we're ready. */
	sc->intr_sem = 1;

	/* Initialize hardware. */
	if (bxe_init_hw(sc, load_code)){
		BXE_PRINTF("%s(%d): Hardware initialization failed, "
		    "aborting!\n", __FILE__, __LINE__);
		goto bxe_init_locked_failed1;
	}

	/* Calculate and save the Ethernet MTU size. */
	sc->port.ether_mtu = ifp->if_mtu + ETHER_HDR_LEN +
	    (ETHER_VLAN_ENCAP_LEN * 2) + ETHER_CRC_LEN + 4;
	DBPRINT(sc, BXE_INFO, "%s(): Setting MTU = %d\n",
	    __FUNCTION__, sc->port.ether_mtu);

	/* Setup the mbuf allocation size for RX frames. */
	if (sc->port.ether_mtu <= MCLBYTES)
		sc->mbuf_alloc_size = MCLBYTES;
	else if (sc->port.ether_mtu <= PAGE_SIZE)
		sc->mbuf_alloc_size = PAGE_SIZE;
	else
		sc->mbuf_alloc_size = MJUM9BYTES;
	DBPRINT(sc, BXE_INFO, "%s(): mbuf_alloc_size = %d, "
	    "max_frame_size = %d\n", __FUNCTION__,
	    sc->mbuf_alloc_size, sc->port.ether_mtu);

	/* Setup NIC internals and enable interrupts. */
	bxe_init_nic(sc, load_code);

	if ((load_code == FW_MSG_CODE_DRV_LOAD_COMMON) &&
	    (sc->common.shmem2_base)){
		if (sc->dcc_enable == TRUE) {
			BXE_PRINTF("Enabing DCC support\n");
			SHMEM2_WR(sc, dcc_support,
			    (SHMEM_DCC_SUPPORT_DISABLE_ENABLE_PF_TLV |
			     SHMEM_DCC_SUPPORT_BANDWIDTH_ALLOCATION_TLV));
		}
	}

#if __FreeBSD_version >= 800000
	/* Allocate buffer rings for multiqueue operation. */
	if (bxe_alloc_buf_rings(sc)) {
		BXE_PRINTF("%s(%d): Buffer ring initialization failed, "
		    "aborting!\n", __FILE__, __LINE__);
		goto bxe_init_locked_failed1;
	}
#endif

	/* Tell MCP that driver load is done. */
	if (!BP_NOMCP(sc)) {
		load_code = bxe_fw_command(sc, DRV_MSG_CODE_LOAD_DONE);
		if (!load_code) {
			BXE_PRINTF("%s(%d): Driver load failed! No MCP "
			    "response to LOAD_DONE!\n", __FILE__, __LINE__);
			goto bxe_init_locked_failed2;
		}
	}

	sc->state = BXE_STATE_OPENING_WAIT4_PORT;

	/* Enable ISR for PORT_SETUP ramrod. */
	sc->intr_sem = 0;

	/* Setup the leading connection for the controller. */
	if (bxe_setup_leading(sc))
		DBPRINT(sc, BXE_FATAL, "%s(): Initial PORT_SETUP ramrod "
		    "failed. State is not OPEN!\n", __FUNCTION__);


	if (CHIP_IS_E1H(sc)) {
		if (sc->mf_config[BP_E1HVN(sc)] & FUNC_MF_CFG_FUNC_DISABLED) {
			BXE_PRINTF("Multi-function mode is disabled\n");
			/* sc->state = BXE_STATE_DISABLED; */
		}

		/* Setup additional client connections for RSS/multi-queue */
		if (sc->state == BXE_STATE_OPEN) {
			for (i = 1; i < sc->num_queues; i++) {
				if (bxe_setup_multi(sc, i)) {
					DBPRINT(sc, BXE_FATAL,
		"%s(): fp[%02d] CLIENT_SETUP ramrod failed! State not OPEN!\n",
					    __FUNCTION__, i);
					goto bxe_init_locked_failed4;
				}
			}
		}
	}

	DELAY(5000);
	bxe_int_enable(sc);
	DELAY(5000);
	/* Initialize statistics. */
	bxe_stats_init(sc);
	DELAY(1000);

	/* Load our MAC address. */
	bcopy(IF_LLADDR(sc->bxe_ifp), sc->link_params.mac_addr, ETHER_ADDR_LEN);

	if (CHIP_IS_E1(sc))
		bxe_set_mac_addr_e1(sc, 1);
	else
		bxe_set_mac_addr_e1h(sc, 1);


	DELAY(1000);

	/* Perform PHY initialization for the primary port. */
	if (sc->port.pmf)
		bxe_initial_phy_init(sc);

	DELAY(1000);

	/* Start fastpath. */
	switch (load_mode) {
	case LOAD_NORMAL:
	case LOAD_OPEN:
		/* Initialize the receive filters. */
		bxe_set_rx_mode(sc);
		break;

	case LOAD_DIAG:
		/* Initialize the receive filters. */
		bxe_set_rx_mode(sc);
		sc->state = BXE_STATE_DIAG;
		break;

	default:
		DBPRINT(sc, BXE_WARN, "%s(): Unknown load mode (%d)!\n",
		    __FUNCTION__, load_mode);
		break;
	}

	if (!sc->port.pmf)
		bxe__link_status_update(sc);

	DELAY(1000);
	/* Tell the stack the driver is running and the TX queue is open. */
	ifp->if_drv_flags = IFF_DRV_RUNNING;

	/* Schedule our periodic timer tick. */
	callout_reset(&sc->bxe_tick_callout, hz, bxe_tick, sc);
	/* Everything went OK, go ahead and exit. */
	goto bxe_init_locked_exit;

	/* Try and gracefully shutdown the device because of a failure. */
bxe_init_locked_failed4:

	for (i = 1; i < sc->num_queues; i++)
		bxe_stop_multi(sc, i);

	bxe_stop_leading(sc);

	bxe_stats_handle(sc, STATS_EVENT_STOP);

bxe_init_locked_failed2:

	bxe_int_disable(sc);

bxe_init_locked_failed1:

	if (!BP_NOMCP(sc)) {
		bxe_fw_command(sc, DRV_MSG_CODE_LOAD_DONE);
		bxe_fw_command(sc, DRV_MSG_CODE_UNLOAD_REQ_WOL_MCP);
		bxe_fw_command(sc, DRV_MSG_CODE_UNLOAD_DONE);
	}
	sc->port.pmf = 0;

#if __FreeBSD_version >= 800000
	bxe_free_buf_rings(sc);
#endif

	DBPRINT(sc, BXE_INFO, "%s(): Initialization failed!\n", __FUNCTION__);

bxe_init_locked_exit:

	DBEXIT(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);
}

/*
 * Ramrod wait function.
 *
 * Waits for a ramrod command to complete.
 *
 * Returns:
 *   0 = Success, !0 = Failure
 */
static int
bxe_wait_ramrod(struct bxe_softc *sc, int state, int idx, int *state_p,
    int poll)
{
	int rc, timeout;

	DBENTER(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET | BXE_VERBOSE_RAMROD);

	DBPRINT(sc, BXE_VERBOSE_RAMROD, "%s(): %s for state 0x%08X on "
	    "fp[%02d], currently 0x%08X.\n", __FUNCTION__,
	    poll ? "Polling" : "Waiting", state, idx, *state_p);

	rc = 0;
	timeout = 5000;
	while (timeout) {

		/* Manually check for the completion. */
		if (poll) {
			bxe_rxeof(sc->fp);
			/*
			 * Some commands don't use the leading client
			 * connection.
			 */
			if (idx)
				bxe_rxeof(&sc->fp[idx]);
		}

		/* State may be changed by bxe_sp_event(). */
		mb();
		if (*state_p == state)
			goto bxe_wait_ramrod_exit;

		timeout--;

		/* Pause 1ms before checking again. */
		DELAY(1000);
	}

	/* We timed out polling for a completion. */
	DBPRINT(sc, BXE_FATAL, "%s(): Timeout %s for state 0x%08X on fp[%d]. "
	    "Got 0x%x instead\n", __FUNCTION__, poll ? "polling" : "waiting",
	    state, idx, *state_p);

	rc = EBUSY;

bxe_wait_ramrod_exit:

	DBEXIT(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET | BXE_VERBOSE_RAMROD);
	return (rc);
}

/*
 *
 *
 */
static void
bxe_write_dmae_phys_len(struct bxe_softc *sc, bus_addr_t phys_addr,
    uint32_t addr, uint32_t len)
{
	int dmae_wr_max, offset;
	DBENTER(BXE_VERBOSE_LOAD);

	dmae_wr_max = DMAE_LEN32_WR_MAX(sc);
	offset = 0;
	while (len > dmae_wr_max) {
		bxe_write_dmae(sc, phys_addr + offset, addr + offset,
		    dmae_wr_max);
		offset += dmae_wr_max * 4;
		len -= dmae_wr_max;
	}
	bxe_write_dmae(sc, phys_addr + offset, addr + offset, len);
	DBEXIT(BXE_VERBOSE_LOAD);

}



#define	INIT_MEM_WR(block, reg, part, hw, data, reg_off, len) \
	bxe_init_str_wr(sc, GRCBASE_##block + reg + reg_off * 4, data, len)


/*
 * Write a block of data to a range of registers.
 *
 * Returns:
 *   None.
 */
static void
bxe_init_str_wr(struct bxe_softc *sc, uint32_t addr, const uint32_t *data,
    uint32_t len)
{
	uint32_t i;
	for (i = 0; i < len; i++)
		REG_WR(sc, addr + i * 4, data[i]);
}

/*
 * Write a block of data to a range of registers using indirect access.
 *
 * Returns:
 *   None.
 */
static void
bxe_init_ind_wr(struct bxe_softc *sc, uint32_t addr, const uint32_t *data,
    uint16_t len)
{
	uint32_t i;
	for (i = 0; i < len; i++)
		REG_WR_IND(sc, addr + i * 4, data[i]);
}

/*
 *
 * Returns:
 *   None.
 */
static void
bxe_write_big_buf(struct bxe_softc *sc, uint32_t addr, uint32_t len)
{
	DBENTER(BXE_VERBOSE_LOAD);
#ifdef USE_DMAE
	if (sc->dmae_ready)
		bxe_write_dmae_phys_len(sc, sc->gunzip_mapping,	addr, len);
	else
		bxe_init_str_wr(sc, addr, sc->gunzip_buf, len);
#else
	bxe_init_str_wr(sc, addr, sc->gunzip_buf, len);
#endif

	DBEXIT(BXE_VERBOSE_LOAD);
}

/*
 * Fill areas of device memory with the specified value.
 *
 * Generally used to clear a small area of device memory prior to writing
 * firmware to STORM memory or writing STORM firmware to device memory.
 *
 * Returns:
 *   None.
 */
static void
bxe_init_fill(struct bxe_softc *sc, uint32_t addr, int fill, uint32_t len)
{
	uint32_t cur_len, i, leftovers, length;

	DBENTER(BXE_VERBOSE_LOAD);

	length = (((len * 4) > FW_BUF_SIZE) ? FW_BUF_SIZE : (len * 4));
	leftovers = length / 4;
	memset(sc->gunzip_buf, fill, length);

	for (i = 0; i < len; i += leftovers) {
		cur_len = min(leftovers, len - i);
		bxe_write_big_buf(sc, addr + i * 4, cur_len);
	}

	DBEXIT(BXE_VERBOSE_LOAD);
}

/*
 *
 * Returns:
 *   None.
 */
static void
bxe_init_wr_64(struct bxe_softc *sc, uint32_t addr, const uint32_t *data,
    uint32_t len64)
{
	uint64_t data64, *pdata;
	uint32_t buf_len32, cur_len, len;
	int i;

	buf_len32 = FW_BUF_SIZE / 4;
	len = len64 * 2;
	/* 64 bit value is in a blob: first low DWORD, then high DWORD. */
	data64 = HILO_U64((*(data + 1)), (*data));
	len64 = min((uint32_t)(FW_BUF_SIZE / 8), len64);
	for (i = 0; i < len64; i++) {
		pdata = ((uint64_t *)(sc->gunzip_buf)) + i;
		*pdata = data64;
	}

	for (i = 0; i < len; i += buf_len32) {
		cur_len = min(buf_len32, len - i);
		bxe_write_big_buf(sc, addr + i*4, cur_len);
	}
}


/*
 * There are different blobs for each PRAM section. In addition, each
 * blob write operation is divided into multiple, smaller write
 * operations in order to decrease the amount of physically contiguous
 * buffer memory needed. Thus, when we select a blob, the address may
 * be with some offset from the beginning of PRAM section. The same
 * holds for the INT_TABLE sections.
 */

#define	IF_IS_INT_TABLE_ADDR(base, addr) \
	if (((base) <= (addr)) && ((base) + 0x400 >= (addr)))

#define	IF_IS_PRAM_ADDR(base, addr) \
	if (((base) <= (addr)) && ((base) + 0x40000 >= (addr)))

/*
 *
 * Returns:
 *   None.
 */

static const uint8_t *
bxe_sel_blob(struct bxe_softc *sc, uint32_t addr, const uint8_t *data)
{

	IF_IS_INT_TABLE_ADDR(TSEM_REG_INT_TABLE, addr)
		data = INIT_TSEM_INT_TABLE_DATA(sc);
	else
		IF_IS_INT_TABLE_ADDR(CSEM_REG_INT_TABLE, addr)
			data = INIT_CSEM_INT_TABLE_DATA(sc);
	else
		IF_IS_INT_TABLE_ADDR(USEM_REG_INT_TABLE, addr)
			data = INIT_USEM_INT_TABLE_DATA(sc);
	else
		IF_IS_INT_TABLE_ADDR(XSEM_REG_INT_TABLE, addr)
			data = INIT_XSEM_INT_TABLE_DATA(sc);
	else
		IF_IS_PRAM_ADDR(TSEM_REG_PRAM, addr)
			data = INIT_TSEM_PRAM_DATA(sc);
	else
		IF_IS_PRAM_ADDR(CSEM_REG_PRAM, addr)
			data = INIT_CSEM_PRAM_DATA(sc);
	else
		IF_IS_PRAM_ADDR(USEM_REG_PRAM, addr)
			data = INIT_USEM_PRAM_DATA(sc);
	else
		IF_IS_PRAM_ADDR(XSEM_REG_PRAM, addr)
			data = INIT_XSEM_PRAM_DATA(sc);

	return (data);

}

static void
bxe_write_big_buf_wb(struct bxe_softc *sc, uint32_t addr, uint32_t len)
{
	if (sc->dmae_ready)
		bxe_write_dmae_phys_len(sc, sc->gunzip_mapping, addr, len);
	else
		bxe_init_ind_wr(sc, addr, sc->gunzip_buf, len);
}


#define VIRT_WR_DMAE_LEN(sc, data, addr, len32, le32_swap) \
	do { \
		memcpy(sc->gunzip_buf, data, (len32)*4); \
		bxe_write_big_buf_wb(sc, addr, len32); \
	} while (0)


/*
 *
 * Returns:
 *   None.
 */
static void
bxe_init_wr_wb(struct bxe_softc *sc, uint32_t addr, const uint32_t *data,
    uint32_t len)
{
	const uint32_t *old_data;

	DBENTER(BXE_VERBOSE_LOAD);
	old_data = data;
	data = (const uint32_t *)bxe_sel_blob(sc, addr, (const uint8_t *)data);
	if (sc->dmae_ready) {
		if (old_data != data)
			VIRT_WR_DMAE_LEN(sc, data, addr, len, 1);
		else
			VIRT_WR_DMAE_LEN(sc, data, addr, len, 0);
	} else
		bxe_init_ind_wr(sc, addr, data, len);

	DBEXIT(BXE_VERBOSE_LOAD);
}

static void
bxe_init_wr_zp(struct bxe_softc *sc, uint32_t addr, uint32_t len,
    uint32_t blob_off)
{
	BXE_PRINTF("%s(%d): Compressed FW is not supported yet. "
	    "ERROR: address:0x%x len:0x%x blob_offset:0x%x\n",
	    __FILE__, __LINE__,	addr, len, blob_off);
}

/*
 * Initialize blocks of the device.
 *
 * This routine basically performs bulk register programming for different
 * blocks within the controller.  The file bxe_init_values.h contains a
 * series of register access operations (read, write, fill, etc.) as well
 * as a BLOB of data to initialize multiple blocks within the controller.
 * Block initialization may be supported by all controllers or by specific
 * models only.
 *
 * Returns:
 *   None.
 */
static void
bxe_init_block(struct bxe_softc *sc, uint32_t block, uint32_t stage)
{
	union init_op *op;
	const uint32_t *data, *data_base;
	uint32_t i, op_type, addr, len;
	uint16_t op_end, op_start;
	int hw_wr;

	op_start = INIT_OPS_OFFSETS(sc)[BLOCK_OPS_IDX(block, stage,
	    STAGE_START)];
	op_end = INIT_OPS_OFFSETS(sc)[BLOCK_OPS_IDX(block, stage, STAGE_END)];
	/* If empty block */
	if (op_start == op_end)
		return;

	hw_wr = OP_WR_ASIC;

	data_base = INIT_DATA(sc);

	for (i = op_start; i < op_end; i++) {

		op = (union init_op *)&(INIT_OPS(sc)[i]);

		op_type = op->str_wr.op;
		addr = op->str_wr.offset;
		len = op->str_wr.data_len;
		data = data_base + op->str_wr.data_off;

		/* HW/EMUL specific */
		if ((op_type > OP_WB) && (op_type == hw_wr))
			op_type = OP_WR;

		switch (op_type) {
		case OP_RD:
			REG_RD(sc, addr);
			break;
		case OP_WR:
			REG_WR(sc, addr, op->write.val);
			break;
		case OP_SW:
			bxe_init_str_wr(sc, addr, data, len);
			break;
		case OP_WB:
			bxe_init_wr_wb(sc, addr, data, len);
			break;
		case OP_SI:
			bxe_init_ind_wr(sc, addr, data, len);
			break;
		case OP_ZR:
			bxe_init_fill(sc, addr, 0, op->zero.len);
			break;
		case OP_ZP:
			bxe_init_wr_zp(sc, addr, len, op->str_wr.data_off);
			break;
		case OP_WR_64:
			bxe_init_wr_64(sc, addr, data, len);
			break;
		default:
			/* happens whenever an op is of a diff HW */
			break;
		}
	}
}

/*
 * Handles controller initialization when called from an unlocked routine.
 * ifconfig calls this function.
 * Returns:
 *   None.
 */
static void
bxe_init(void *xsc)
{
	struct bxe_softc *sc;

	sc = xsc;
	DBENTER(BXE_VERBOSE_RESET | BXE_VERBOSE_UNLOAD);

	BXE_CORE_LOCK(sc);
	bxe_init_locked(sc, LOAD_NORMAL);
	BXE_CORE_UNLOCK(sc);

	DBEXIT(BXE_VERBOSE_RESET | BXE_VERBOSE_UNLOAD);
}


/*
 * Release all resources used by the driver.
 *
 * Releases all resources acquired by the driver including interrupts,
 * interrupt handler, interfaces, mutexes, and DMA memory.
 *
 * Returns:
 *   None.
 */
static void
bxe_release_resources(struct bxe_softc *sc)
{
	device_t dev;
	int i;

	DBENTER(BXE_VERBOSE_RESET | BXE_VERBOSE_UNLOAD);

	dev = sc->dev;

	/* Release the FreeBSD interface. */
	if (sc->bxe_ifp != NULL)
		if_free(sc->bxe_ifp);

	/* Release interrupt resources. */
	bxe_interrupt_detach(sc);

	if ((sc->bxe_flags & BXE_USING_MSIX_FLAG) && sc->msix_count) {

		for (i = 0; i < sc->msix_count; i++) {
			DBPRINT(sc, (BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET |
			    BXE_VERBOSE_INTR), "%s(): Releasing MSI-X[%d] "
			    "vector.\n", __FUNCTION__, i);
			if (sc->bxe_msix_res[i] && sc->bxe_msix_rid[i])
				bus_release_resource(dev, SYS_RES_IRQ,
				    sc->bxe_msix_rid[i], sc->bxe_msix_res[i]);
		}

		pci_release_msi(dev);

	} else if ((sc->bxe_flags & BXE_USING_MSI_FLAG) && sc->msi_count) {

		for (i = 0; i < sc->msi_count; i++) {
			DBPRINT(sc, (BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET |
			    BXE_VERBOSE_INTR), "%s(): Releasing MSI[%d] "
			    "vector.\n", __FUNCTION__, i);
			if (sc->bxe_msi_res[i] && sc->bxe_msi_rid[i])
				bus_release_resource(dev, SYS_RES_IRQ,
				    sc->bxe_msi_rid[i], sc->bxe_msi_res[i]);
		}

		pci_release_msi(dev);

	} else {

		DBPRINT(sc, (BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET |
		    BXE_VERBOSE_INTR), "%s(): Releasing legacy interrupt.\n",
		    __FUNCTION__);
		if (sc->bxe_irq_res != NULL)
			bus_release_resource(dev, SYS_RES_IRQ,
			    sc->bxe_irq_rid, sc->bxe_irq_res);
	}

	/* Free the DMA resources. */
	bxe_dma_free(sc);

	bxe_release_pci_resources(sc);

#if __FreeBSD_version >= 800000
	/* Free multiqueue buffer rings. */
	bxe_free_buf_rings(sc);
#endif

	/* Free remaining fastpath resources. */
	bxe_free_mutexes(sc);
}


/*
 * Indirect register write.
 *
 * Writes NetXtreme II registers using an index/data register pair in PCI
 * configuration space.  Using this mechanism avoids issues with posted
 * writes but is much slower than memory-mapped I/O.
 *
 * Returns:
 *   None.
 */
static void
bxe_reg_wr_ind(struct bxe_softc *sc, uint32_t offset, uint32_t val)
{
	DBPRINT(sc, BXE_INSANE, "%s(); offset = 0x%08X, val = 0x%08X\n",
		__FUNCTION__, offset, val);

	pci_write_config(sc->dev, PCICFG_GRC_ADDRESS, offset, 4);
	pci_write_config(sc->dev, PCICFG_GRC_DATA, val, 4);

	/* Return to a safe address. */
	pci_write_config(sc->dev, PCICFG_GRC_ADDRESS,
	    PCICFG_VENDOR_ID_OFFSET, 4);
}


/*
 * Indirect register read.
 *
 * Reads NetXtreme II registers using an index/data register pair in PCI
 * configuration space.  Using this mechanism avoids issues with posted
 * reads but is much slower than memory-mapped I/O.
 *
 * Returns:
 *   The value of the register.
 */
static uint32_t
bxe_reg_rd_ind(struct bxe_softc *sc, uint32_t offset)
{
	uint32_t val;

	pci_write_config(sc->dev, PCICFG_GRC_ADDRESS, offset, 4);
	val = pci_read_config(sc->dev, PCICFG_GRC_DATA, 4);

	/* Return to a safe address. */
	pci_write_config(sc->dev, PCICFG_GRC_ADDRESS,
	    PCICFG_VENDOR_ID_OFFSET, 4);

	DBPRINT(sc, BXE_INSANE, "%s(); offset = 0x%08X, val = 0x%08X\n",
	    __FUNCTION__, offset, val);
	return (val);
}



static uint32_t dmae_reg_go_c[] = {
	DMAE_REG_GO_C0, DMAE_REG_GO_C1, DMAE_REG_GO_C2, DMAE_REG_GO_C3,
	DMAE_REG_GO_C4, DMAE_REG_GO_C5, DMAE_REG_GO_C6, DMAE_REG_GO_C7,
	DMAE_REG_GO_C8, DMAE_REG_GO_C9, DMAE_REG_GO_C10, DMAE_REG_GO_C11,
	DMAE_REG_GO_C12, DMAE_REG_GO_C13, DMAE_REG_GO_C14, DMAE_REG_GO_C15
};


/*
 * Copy DMAE command into memory and start the command.
 *
 * Returns:
 *   None.
 */
static void
bxe_post_dmae(struct bxe_softc *sc, struct dmae_command *dmae, int idx)
{
	uint32_t cmd_offset;
	int i;
	cmd_offset = (DMAE_REG_CMD_MEM + sizeof(struct dmae_command) * idx);

	for (i = 0; i < (sizeof(struct dmae_command) / 4); i++) {
		REG_WR(sc, cmd_offset + i * 4, *(((uint32_t *)dmae) + i));
		DBPRINT(sc, BXE_INSANE, "%s(): DMAE cmd[%d].%d : 0x%08X\n",
		    __FUNCTION__, idx, i, cmd_offset + i * 4);
	}

	/* Kick off the command. */
	REG_WR(sc, dmae_reg_go_c[idx], 1);
}


/*
 * Perform a DMAE write to device memory.
 *
 * Some of the registers on the 577XX controller are 128bits wide.  It is
 * required that when accessing those registers that they be written
 * atomically and that no intervening bus acceses to the device occur.
 * This could be handled by a lock held across all driver instances for
 * the device or it can be handled by performing a DMA operation when
 * writing to the device.  This code implements the latter.
 *
 * Returns:
 *   None.
 */
void
bxe_write_dmae(struct bxe_softc *sc, bus_addr_t dma_addr, uint32_t dst_addr,
    uint32_t len32)
{
	struct dmae_command dmae;
	uint32_t *data, *wb_comp;
	int timeout;

	DBENTER(BXE_INSANE_REGS);

	DBPRINT(sc, BXE_EXTREME_REGS,
	    "%s(): host addr = 0x%jX, device addr = 0x%08X, length = %d.\n",
	    __FUNCTION__, (uintmax_t)dma_addr, dst_addr, (int)len32);

	wb_comp = BXE_SP(sc, wb_comp);
	/* Fall back to indirect access if DMAE is not ready. */
	if (!sc->dmae_ready) {
		data = BXE_SP(sc, wb_data[0]);

		DBPRINT(sc, BXE_WARN, "%s(): DMAE not ready, "
		    "using indirect.\n", __FUNCTION__);

		bxe_init_ind_wr(sc, dst_addr, data, len32);
		goto bxe_write_dmae_exit;
	}

	memset(&dmae, 0, sizeof(struct dmae_command));

	dmae.opcode = (DMAE_CMD_SRC_PCI | DMAE_CMD_DST_GRC |
	    DMAE_CMD_C_DST_PCI | DMAE_CMD_C_ENABLE |
	    DMAE_CMD_SRC_RESET | DMAE_CMD_DST_RESET |
#ifdef __BIG_ENDIAN
	    DMAE_CMD_ENDIANITY_B_DW_SWAP |
#else
	    DMAE_CMD_ENDIANITY_DW_SWAP |
#endif
	    (BP_PORT(sc) ? DMAE_CMD_PORT_1 : DMAE_CMD_PORT_0) |
	    (BP_E1HVN(sc) << DMAE_CMD_E1HVN_SHIFT));
	dmae.src_addr_lo = U64_LO(dma_addr);
	dmae.src_addr_hi = U64_HI(dma_addr);
	dmae.dst_addr_lo = dst_addr >> 2;
	dmae.dst_addr_hi = 0;
	dmae.len = len32;
	dmae.comp_addr_lo = U64_LO(BXE_SP_MAPPING(sc, wb_comp));
	dmae.comp_addr_hi = U64_HI(BXE_SP_MAPPING(sc, wb_comp));
	dmae.comp_val = BXE_WB_COMP_VAL;

	BXE_DMAE_LOCK(sc);

	*wb_comp = 0;

	bxe_post_dmae(sc, &dmae, INIT_DMAE_C(sc));

	DELAY(50);

	/* Wait up to 200ms. */
	timeout = 4000;
	while (*wb_comp != BXE_WB_COMP_VAL) {
		if (!timeout) {
			DBPRINT(sc, BXE_FATAL,
			"%s(): DMAE timeout (dst_addr = 0x%08X, len = %d)!\n",
			    __FUNCTION__, dst_addr, len32);
			break;
		}
		timeout--;
		DELAY(50);
	}

	BXE_DMAE_UNLOCK(sc);

bxe_write_dmae_exit:
	DBEXIT(BXE_INSANE_REGS);
}


/*
 * Perform a DMAE read from to device memory.
 *
 * Some of the registers on the 577XX controller are 128bits wide.  It is
 * required that when accessing those registers that they be read
 * atomically and that no intervening bus acceses to the device occur.
 * This could be handled by a lock held across all driver instances for
 * the device or it can be handled by performing a DMA operation when
 * reading from the device.  This code implements the latter.
 *
 * Returns:
 *   None.
 */
void
bxe_read_dmae(struct bxe_softc *sc, uint32_t src_addr,
    uint32_t len32)
{
	struct dmae_command dmae;
	uint32_t *data, *wb_comp;
	int i, timeout;

	DBENTER(BXE_INSANE);

	wb_comp = BXE_SP(sc, wb_comp);
	/* Fall back to indirect access if DMAE is not ready. */
	if (!sc->dmae_ready) {
		data = BXE_SP(sc, wb_data[0]);

		DBPRINT(sc, BXE_WARN, "%s(): DMAE not ready, "
		    "using indirect.\n", __FUNCTION__);

		for (i = 0; i < len32; i++)
			data[i] = bxe_reg_rd_ind(sc, src_addr + i * 4);

		goto bxe_read_dmae_exit;
	}

	memset(&dmae, 0, sizeof(struct dmae_command));

	dmae.opcode = (DMAE_CMD_SRC_GRC | DMAE_CMD_DST_PCI |
	    DMAE_CMD_C_DST_PCI | DMAE_CMD_C_ENABLE |
	    DMAE_CMD_SRC_RESET | DMAE_CMD_DST_RESET |
#ifdef __BIG_ENDIAN
	    DMAE_CMD_ENDIANITY_B_DW_SWAP |
#else
	    DMAE_CMD_ENDIANITY_DW_SWAP |
#endif
	    (BP_PORT(sc) ? DMAE_CMD_PORT_1 : DMAE_CMD_PORT_0) |
	    (BP_E1HVN(sc) << DMAE_CMD_E1HVN_SHIFT));

	dmae.src_addr_lo = src_addr >> 2;
	dmae.src_addr_hi = 0;
	dmae.dst_addr_lo = U64_LO(BXE_SP_MAPPING(sc, wb_data));
	dmae.dst_addr_hi = U64_HI(BXE_SP_MAPPING(sc, wb_data));
	dmae.len = len32;
	dmae.comp_addr_lo = U64_LO(BXE_SP_MAPPING(sc, wb_comp));
	dmae.comp_addr_hi = U64_HI(BXE_SP_MAPPING(sc, wb_comp));
	dmae.comp_val = BXE_WB_COMP_VAL;

	BXE_DMAE_LOCK(sc);

	memset(BXE_SP(sc, wb_data[0]), 0, sizeof(uint32_t) * 4);
	*wb_comp = 0;

	bxe_post_dmae(sc, &dmae, INIT_DMAE_C(sc));

	DELAY(50);

	timeout = 4000;
	while (*wb_comp != BXE_WB_COMP_VAL) {
		if (!timeout) {
			DBPRINT(sc, BXE_FATAL,
			"%s(): DMAE timeout (src_addr = 0x%08X, len = %d)!\n",
			    __FUNCTION__, src_addr, len32);
			break;
		}
		timeout--;
		DELAY(50);
	}

	BXE_DMAE_UNLOCK(sc);

bxe_read_dmae_exit:
	DBEXIT(BXE_INSANE);
}

/*
 * DMAE write wrapper.
 *
 * Returns:
 *   None.
 */
static void
bxe_wb_wr(struct bxe_softc *sc, int reg, uint32_t val_hi, uint32_t val_lo)
{
	uint32_t wb_write[2];

	wb_write[0] = val_hi;
	wb_write[1] = val_lo;
	REG_WR_DMAE(sc, reg, wb_write, 2);
}



/*
 * Poll a register waiting for a value.
 *
 * Returns:
 *   The last read register value.
 */
static __inline
uint32_t bxe_reg_poll(struct bxe_softc *sc, uint32_t reg, uint32_t expected,
    int ms, int wait)
{
	uint32_t val;

	do {
		val = REG_RD(sc, reg);
		if (val == expected)
			break;
		ms -= wait;
		DELAY(wait * 1000);

	} while (ms > 0);

	return (val);
}


/*
 * Microcode assert display.
 *
 * This function walks through each STORM processor and prints out a
 * listing of all asserts currently in effect.  Useful for post-mortem
 * debugging.
 *
 * Returns:
 *   The number of asserts detected.
 */
static int
bxe_mc_assert(struct bxe_softc *sc)
{
	uint32_t row0, row1, row2, row3;
	char last_idx;
	int i, rc;

	DBENTER(BXE_VERBOSE_INTR);

	rc = 0;
	/* XSTORM */
	last_idx = REG_RD8(sc, BAR_XSTORM_INTMEM +
	    XSTORM_ASSERT_LIST_INDEX_OFFSET);

	if (last_idx)
		DBPRINT(sc, BXE_FATAL, "DATA XSTORM_ASSERT_LIST_INDEX 0x%x\n",
		    last_idx);

	/* Print the asserts */
	for (i = 0; i < STORM_ASSERT_ARRAY_SIZE; i++) {

		row0 = REG_RD(sc, BAR_XSTORM_INTMEM +
		    XSTORM_ASSERT_LIST_OFFSET(i));
		row1 = REG_RD(sc, BAR_XSTORM_INTMEM +
		    XSTORM_ASSERT_LIST_OFFSET(i) + 4);
		row2 = REG_RD(sc, BAR_XSTORM_INTMEM +
		    XSTORM_ASSERT_LIST_OFFSET(i) + 8);
		row3 = REG_RD(sc, BAR_XSTORM_INTMEM +
		    XSTORM_ASSERT_LIST_OFFSET(i) + 12);

		if (row0 != COMMON_ASM_INVALID_ASSERT_OPCODE) {
			DBPRINT(sc, BXE_FATAL, "DATA XSTORM_ASSERT_INDEX %d = "
			    "0x%08x 0x%08x 0x%08x 0x%08x\n", i, row3, row2,
			    row1, row0);
			rc++;
		} else
			break;
	}

	/* TSTORM */
	last_idx = REG_RD8(sc, BAR_TSTORM_INTMEM +
	    TSTORM_ASSERT_LIST_INDEX_OFFSET);

	if (last_idx)
		DBPRINT(sc, BXE_FATAL, "DATA TSTORM_ASSERT_LIST_INDEX 0x%x\n",
			last_idx);

	/* Print the asserts */
	for (i = 0; i < STORM_ASSERT_ARRAY_SIZE; i++) {

		row0 = REG_RD(sc, BAR_TSTORM_INTMEM +
		    TSTORM_ASSERT_LIST_OFFSET(i));
		row1 = REG_RD(sc, BAR_TSTORM_INTMEM +
		    TSTORM_ASSERT_LIST_OFFSET(i) + 4);
		row2 = REG_RD(sc, BAR_TSTORM_INTMEM +
		    TSTORM_ASSERT_LIST_OFFSET(i) + 8);
		row3 = REG_RD(sc, BAR_TSTORM_INTMEM +
		    TSTORM_ASSERT_LIST_OFFSET(i) + 12);

		if (row0 != COMMON_ASM_INVALID_ASSERT_OPCODE) {
			DBPRINT(sc, BXE_FATAL, "DATA TSTORM_ASSERT_INDEX %d = "
			    "0x%08x 0x%08x 0x%08x 0x%08x\n", i, row3, row2,
			    row1, row0);
			rc++;
		} else
			break;
	}

	/* CSTORM */
	last_idx = REG_RD8(sc, BAR_CSTORM_INTMEM +
	    CSTORM_ASSERT_LIST_INDEX_OFFSET);

	if (last_idx)
		DBPRINT(sc, BXE_FATAL, "DATA CSTORM_ASSERT_LIST_INDEX 0x%x\n",
		    last_idx);

	/* Print the asserts */
	for (i = 0; i < STORM_ASSERT_ARRAY_SIZE; i++) {

		row0 = REG_RD(sc, BAR_CSTORM_INTMEM +
		    CSTORM_ASSERT_LIST_OFFSET(i));
		row1 = REG_RD(sc, BAR_CSTORM_INTMEM +
		    CSTORM_ASSERT_LIST_OFFSET(i) + 4);
		row2 = REG_RD(sc, BAR_CSTORM_INTMEM +
		    CSTORM_ASSERT_LIST_OFFSET(i) + 8);
		row3 = REG_RD(sc, BAR_CSTORM_INTMEM +
		    CSTORM_ASSERT_LIST_OFFSET(i) + 12);

		if (row0 != COMMON_ASM_INVALID_ASSERT_OPCODE) {
			DBPRINT(sc, BXE_FATAL, "DATA CSTORM_ASSERT_INDEX %d = "
			    "0x%08x 0x%08x 0x%08x 0x%08x\n", i, row3, row2,
			    row1, row0);
			rc++;
		} else
			break;
	}

	/* USTORM */
	last_idx = REG_RD8(sc, BAR_USTORM_INTMEM +
	    USTORM_ASSERT_LIST_INDEX_OFFSET);

	if (last_idx)
		DBPRINT(sc, BXE_FATAL, "DATA USTORM_ASSERT_LIST_INDEX 0x%x\n",
		    last_idx);

	/* Print the asserts */
	for (i = 0; i < STORM_ASSERT_ARRAY_SIZE; i++) {

		row0 = REG_RD(sc, BAR_USTORM_INTMEM +
		    USTORM_ASSERT_LIST_OFFSET(i));
		row1 = REG_RD(sc, BAR_USTORM_INTMEM +
		    USTORM_ASSERT_LIST_OFFSET(i) + 4);
		row2 = REG_RD(sc, BAR_USTORM_INTMEM +
		    USTORM_ASSERT_LIST_OFFSET(i) + 8);
		row3 = REG_RD(sc, BAR_USTORM_INTMEM +
		    USTORM_ASSERT_LIST_OFFSET(i) + 12);

		if (row0 != COMMON_ASM_INVALID_ASSERT_OPCODE) {
			DBPRINT(sc, BXE_FATAL, "DATA USTORM_ASSERT_INDEX %d = "
			    "0x%08x 0x%08x 0x%08x 0x%08x\n", i, row3, row2,
			    row1, row0);
			rc++;
		} else
			break;
	}

	DBEXIT(BXE_VERBOSE_INTR);
	return (rc);
}


/*
 * Perform a panic dump.
 *
 * Returns:
 *   None
 */
static void
bxe_panic_dump(struct bxe_softc *sc)
{
	DBENTER(BXE_FATAL);

	sc->stats_state = STATS_STATE_DISABLED;

	BXE_PRINTF("---------- Begin crash dump ----------\n");

	/* Idle	check is run twice to verify the controller has	stopped. */
	bxe_idle_chk(sc);
	bxe_idle_chk(sc);
	bxe_mc_assert(sc);

#ifdef BXE_DEBUG
	bxe_breakpoint(sc);
#endif

	BXE_PRINTF("----------  End crash dump  ----------\n");

	DBEXIT(BXE_FATAL);
}


/*
 * Enables interrupt generation.
 *
 * Returns:
 *   None.
 */
static void
bxe_int_enable(struct bxe_softc *sc)
{
	uint32_t hc_addr, val;
	int port;

	DBENTER(BXE_VERBOSE_INTR);

	port = BP_PORT(sc);
	hc_addr = port ? HC_REG_CONFIG_1 : HC_REG_CONFIG_0;
	val = REG_RD(sc, hc_addr);
	if (sc->bxe_flags & BXE_USING_MSIX_FLAG) {
		if (sc->msix_count == 1) {

			/* Single interrupt, multiple queues.*/
			DBPRINT(sc, BXE_VERBOSE_INTR,
		"%s(): Setting host coalescing registers for MSI-X (SIMQ).\n",
			    __FUNCTION__);

			/* Clear INTx. */
			val &= ~HC_CONFIG_0_REG_INT_LINE_EN_0;

			/* Enable single ISR mode, MSI/MSI-X, and attention messages. */
			val |= (HC_CONFIG_0_REG_SINGLE_ISR_EN_0 |
			    HC_CONFIG_0_REG_MSI_MSIX_INT_EN_0 |
			    HC_CONFIG_0_REG_ATTN_BIT_EN_0);
		} else {

			/* Multiple interrupts, multiple queues.*/
			DBPRINT(sc, BXE_VERBOSE_INTR,
		"%s(): Setting host coalescing registers for MSI-X (MIMQ).\n",
			    __FUNCTION__);

			/* Clear single ISR mode and INTx. */
			val &= ~(HC_CONFIG_0_REG_SINGLE_ISR_EN_0 |
			    HC_CONFIG_0_REG_INT_LINE_EN_0);

			/* Enable MSI/MSI-X and attention messages. */
			val |= (HC_CONFIG_0_REG_MSI_MSIX_INT_EN_0 |
    			    HC_CONFIG_0_REG_ATTN_BIT_EN_0);
		}

	} else if (sc->bxe_flags & BXE_USING_MSI_FLAG) {

		if (sc->msi_count == 1) {

			/* Single interrupt, multiple queues.*/
			DBPRINT(sc, BXE_VERBOSE_INTR,
		"%s(): Setting host coalescing registers for MSI (SIMQ).\n",
			    __FUNCTION__);

			/* Clear INTx. */
			val &= ~HC_CONFIG_0_REG_INT_LINE_EN_0;

			/* Enable single ISR mode, MSI/MSI-X, and attention
			 * messages.
			 */
			val |= (HC_CONFIG_0_REG_SINGLE_ISR_EN_0 |
			    HC_CONFIG_0_REG_MSI_MSIX_INT_EN_0 |
			    HC_CONFIG_0_REG_ATTN_BIT_EN_0);
		} else {
			/* Multiple interrupts, multiple queues.*/
			DBPRINT(sc, BXE_VERBOSE_INTR,
			    "%s(): Setting host coalescing registers for"
			    "MSI (MIMQ).\n",
			    __FUNCTION__);

			/* Clear single ISR mode and INTx. */
			val &= ~(HC_CONFIG_0_REG_SINGLE_ISR_EN_0 |
			    HC_CONFIG_0_REG_INT_LINE_EN_0);

			/* Enable MSI/MSI-X and attention messages. */
			val |= (HC_CONFIG_0_REG_MSI_MSIX_INT_EN_0 |
			    HC_CONFIG_0_REG_ATTN_BIT_EN_0);
		}
	} else {
		/* Single interrupt, single queue. */
		DBPRINT(sc, BXE_VERBOSE_INTR,
		    "%s(): Setting host coalescing registers for INTA#.\n",
		    __FUNCTION__);

		val |= (HC_CONFIG_0_REG_SINGLE_ISR_EN_0   |
		    HC_CONFIG_0_REG_MSI_MSIX_INT_EN_0 |
		    HC_CONFIG_0_REG_INT_LINE_EN_0     |
		    HC_CONFIG_0_REG_ATTN_BIT_EN_0);
		REG_WR(sc, hc_addr, val);

		val &= ~HC_CONFIG_0_REG_MSI_MSIX_INT_EN_0;
	}

	/* Write the interrupt mode to the host coalescing block. */
	REG_WR(sc, hc_addr, val);

	if (CHIP_IS_E1H(sc)) {

		/* Init leading/trailing edge attention generation. */
		if (IS_E1HMF(sc)) {
			val = (0xee0f | (1 << (BP_E1HVN(sc) + 4)));

			/*
			 * Check if this driver instance is the port
			 * master function.
			 */
			if (sc->port.pmf)
				/* Enable nig & GPIO3 attentions. */
				val |= 0x1100;
		} else
			val = 0xffff;

		REG_WR(sc, HC_REG_TRAILING_EDGE_0 + port * 8, val);
		REG_WR(sc, HC_REG_LEADING_EDGE_0 + port * 8, val);
	}

	DBEXIT(BXE_VERBOSE_INTR);
}


/*
 * Disables interrupt generation.
 *
 * Returns:
 *   None.
 */
static void
bxe_int_disable(struct bxe_softc *sc)
{
	uint32_t hc_addr, val;
	int port;

	DBENTER(BXE_VERBOSE_INTR);

	port = BP_PORT(sc);
	hc_addr = port ? HC_REG_CONFIG_1 : HC_REG_CONFIG_0;
	val = REG_RD(sc, hc_addr);

	val &= ~(HC_CONFIG_0_REG_MSI_MSIX_INT_EN_0 |
	    HC_CONFIG_0_REG_INT_LINE_EN_0 | HC_CONFIG_0_REG_ATTN_BIT_EN_0);

	REG_WR(sc, hc_addr, val);

	if (REG_RD(sc, hc_addr)!= val) {
		DBPRINT(sc, BXE_WARN, "%s(): BUG! Returned value from IGU "
		    "doesn't match value written (0x%08X).\n",
		    __FUNCTION__, val);
	}

	DBEXIT(BXE_VERBOSE_INTR);
}

#define	BXE_CRC32_RESIDUAL	0xdebb20e3

/*
 * Returns:
 *   0 = Success, !0 = Failure.
 */
static int
bxe_nvram_acquire_lock(struct bxe_softc *sc)
{
	uint32_t val;
	int i, port, rc;

	DBENTER(BXE_VERBOSE_NVRAM);

	port = BP_PORT(sc);
	rc = 0;
	val = 0;

	/* Acquire the NVRAM lock. */
	REG_WR(sc, MCP_REG_MCPR_NVM_SW_ARB,
	    (MCPR_NVM_SW_ARB_ARB_REQ_SET1 << port));

	for (i = 0; i < NVRAM_TIMEOUT_COUNT * 10; i++) {
		val = REG_RD(sc, MCP_REG_MCPR_NVM_SW_ARB);
		if (val & (MCPR_NVM_SW_ARB_ARB_ARB1 << port))
			break;

		DELAY(5);
	}

	if (!(val & (MCPR_NVM_SW_ARB_ARB_ARB1 << port))) {
		DBPRINT(sc, BXE_WARN, "%s(): Cannot acquire NVRAM lock!\n",
		    __FUNCTION__);
		rc = EBUSY;
	}

	DBEXIT(BXE_VERBOSE_NVRAM);
	return (rc);
}

/*
 * Returns:
 *   0 = Success, !0 = Failure.
 */
static int
bxe_nvram_release_lock(struct bxe_softc *sc)
{
	uint32_t val;
	int i, port, rc;

	DBENTER(BXE_VERBOSE_NVRAM);

	port = BP_PORT(sc);
	rc = 0;
	val = 0;

	/* Release the NVRAM lock. */
	REG_WR(sc, MCP_REG_MCPR_NVM_SW_ARB,
	    (MCPR_NVM_SW_ARB_ARB_REQ_CLR1 << port));

	for (i = 0; i < NVRAM_TIMEOUT_COUNT * 10; i++) {
		val = REG_RD(sc, MCP_REG_MCPR_NVM_SW_ARB);
		if (!(val & (MCPR_NVM_SW_ARB_ARB_ARB1 << port)))
			break;

		DELAY(5);
	}

	if (val & (MCPR_NVM_SW_ARB_ARB_ARB1 << port)) {
		DBPRINT(sc, BXE_WARN, "%s(): Cannot release NVRAM lock!\n",
		    __FUNCTION__);
		rc = EBUSY;
	}

	DBEXIT(BXE_VERBOSE_NVRAM);
	return (rc);
}

/*
 * Returns:
 *   None.
 */
static void
bxe_nvram_enable_access(struct bxe_softc *sc)
{
	uint32_t val;

	DBENTER(BXE_VERBOSE_NVRAM);

	val = REG_RD(sc, MCP_REG_MCPR_NVM_ACCESS_ENABLE);

	/* Enable both bits, even on read */
	REG_WR(sc, MCP_REG_MCPR_NVM_ACCESS_ENABLE,
	    (val | MCPR_NVM_ACCESS_ENABLE_EN |
	     MCPR_NVM_ACCESS_ENABLE_WR_EN));

	DBEXIT(BXE_VERBOSE_NVRAM);
}

/*
 * Returns:
 *   None.
 */
static void
bxe_nvram_disable_access(struct bxe_softc *sc)
{
	uint32_t val;

	DBENTER(BXE_VERBOSE_NVRAM);

	val = REG_RD(sc, MCP_REG_MCPR_NVM_ACCESS_ENABLE);

	/* Disable both bits, even after read. */
	REG_WR(sc, MCP_REG_MCPR_NVM_ACCESS_ENABLE,
	    (val & ~(MCPR_NVM_ACCESS_ENABLE_EN |
	    MCPR_NVM_ACCESS_ENABLE_WR_EN)));

	DBEXIT(BXE_VERBOSE_NVRAM);
}

/*
 * Returns:
 *   0 = Success, !0 = Failure.
 */
static int
bxe_nvram_read_dword(struct bxe_softc *sc, uint32_t offset, uint32_t *ret_val,
    uint32_t cmd_flags)
{
	uint32_t val;
	int i, rc;

	DBENTER(BXE_INSANE_NVRAM);

	/* Build the command word. */
	cmd_flags |= MCPR_NVM_COMMAND_DOIT;

	/* Need to clear DONE bit separately. */
	REG_WR(sc, MCP_REG_MCPR_NVM_COMMAND, MCPR_NVM_COMMAND_DONE);

	/* Address within the NVRAM to read. */
	REG_WR(sc, MCP_REG_MCPR_NVM_ADDR,
		(offset & MCPR_NVM_ADDR_NVM_ADDR_VALUE));

	/* Issue a read command. */
	REG_WR(sc, MCP_REG_MCPR_NVM_COMMAND, cmd_flags);

	/* Wait for completion. */
	*ret_val = 0;
	rc = EBUSY;
	for (i = 0; i < NVRAM_TIMEOUT_COUNT; i++) {
		DELAY(5);
		val = REG_RD(sc, MCP_REG_MCPR_NVM_COMMAND);

		if (val & MCPR_NVM_COMMAND_DONE) {
			val = REG_RD(sc, MCP_REG_MCPR_NVM_READ);
			val = htobe32(val);
			*ret_val = val;
			rc = 0;
			break;
		}
	}

	DBPRINT(sc, BXE_INSANE_NVRAM, "%s(): Read 0x%08X from offset 0x%08X.\n",
	    __FUNCTION__, *ret_val, offset);
	DBEXIT(BXE_INSANE_NVRAM);
	return (rc);
}

/*
 * Returns:
 *   0 = Success, !0 = Failure.
 */
static int
bxe_nvram_read(struct bxe_softc *sc, uint32_t offset, uint8_t *ret_buf,
    int buf_size)
{
	uint32_t cmd_flags, val;
	int rc;

	DBENTER(BXE_EXTREME_NVRAM);

	if ((offset & 0x03) || (buf_size & 0x03) || (buf_size == 0)) {
		DBPRINT(sc, BXE_WARN, "%s(): Unaligned address or invalid "
		    "buffer for NVRAM read (offset = 0x%08X, buf_size = %d)!\n",
		    __FUNCTION__, offset, buf_size);
		rc = EINVAL;
		goto bxe_nvram_read_exit;
	}

	if (offset + buf_size > sc->common.flash_size) {
		DBPRINT(sc, BXE_WARN, "%s(): Read extends beyond the end of "
		    "the NVRAM (offset (0x%08X) + buf_size (%d) > flash_size "
		    "(0x%08X))!\n", __FUNCTION__, offset, buf_size,
		    sc->common.flash_size);
		rc = EINVAL;
		goto bxe_nvram_read_exit;
	}

	rc = bxe_nvram_acquire_lock(sc);
	if (rc)
		goto bxe_nvram_read_exit;

	bxe_nvram_enable_access(sc);

	/* Read the first word(s). */
	cmd_flags = MCPR_NVM_COMMAND_FIRST;
	while ((buf_size > sizeof(uint32_t)) && (rc == 0)) {
		rc = bxe_nvram_read_dword(sc, offset, &val, cmd_flags);
		memcpy(ret_buf, &val, 4);

		/* Advance to the next DWORD. */
		offset += sizeof(uint32_t);
		ret_buf += sizeof(uint32_t);
		buf_size -= sizeof(uint32_t);
		cmd_flags = 0;
	}

	/* Read the final word. */
	if (rc == 0) {
		cmd_flags |= MCPR_NVM_COMMAND_LAST;
		rc = bxe_nvram_read_dword(sc, offset, &val, cmd_flags);
		memcpy(ret_buf, &val, 4);
	}

	/* Disable access to NVRAM interface. */
	bxe_nvram_disable_access(sc);
	bxe_nvram_release_lock(sc);

bxe_nvram_read_exit:
	DBEXIT(BXE_EXTREME_NVRAM);
	return (rc);
}

#ifdef BXE_NVRAM_WRITE_SUPPORT
/*
 * Returns:
 *   0 = Success, !0 = Failure.
 */
static int
bxe_nvram_write_dword(struct bxe_softc *sc, uint32_t offset, uint32_t val,
uint32_t cmd_flags)
{
	int i, rc;

	DBENTER(BXE_VERBOSE_NVRAM);

	/* Build the command word. */
	cmd_flags |= MCPR_NVM_COMMAND_DOIT | MCPR_NVM_COMMAND_WR;

	/* Need to clear DONE bit separately. */
	REG_WR(sc, MCP_REG_MCPR_NVM_COMMAND, MCPR_NVM_COMMAND_DONE);

	/* Write the data. */
	REG_WR(sc, MCP_REG_MCPR_NVM_WRITE, val);

	/* Address to write within the NVRAM. */
	REG_WR(sc, MCP_REG_MCPR_NVM_ADDR,
		(offset & MCPR_NVM_ADDR_NVM_ADDR_VALUE));

	/* Issue the write command. */
	REG_WR(sc, MCP_REG_MCPR_NVM_COMMAND, cmd_flags);

	/* Wait for completion. */
	rc = EBUSY;
	for (i = 0; i < NVRAM_TIMEOUT_COUNT; i++) {
		DELAY(5);
		val = REG_RD(sc, MCP_REG_MCPR_NVM_COMMAND);
		if (val & MCPR_NVM_COMMAND_DONE) {
			rc = 0;
			break;
		}
	}

	DBEXIT(BXE_VERBOSE_NVRAM);
	return (rc);
}

#define	BYTE_OFFSET(offset)		(8 * (offset & 0x03))

/*
 * Returns:
 *
 */
static int
bxe_nvram_write1(struct bxe_softc *sc, uint32_t offset, uint8_t *data_buf,
    int buf_size)
{
	uint32_t align_offset, cmd_flags, val;
	int rc;

	DBENTER(BXE_VERBOSE_NVRAM);

	if (offset + buf_size > sc->common.flash_size) {
		DBPRINT(sc, BXE_WARN, "%s(): Write extends beyond the end of "
		    "the NVRAM (offset (0x%08X) + buf_size (%d) > flash_size "
		    "(0x%08X))!\n", __FUNCTION__, offset, buf_size,
		    sc->common.flash_size);
		rc = EINVAL;
		goto bxe_nvram_write1_exit;
	}

	/* request access to nvram interface */
	rc = bxe_nvram_acquire_lock(sc);
	if (rc)
		goto bxe_nvram_write1_exit;

	/* Enable access to the NVRAM interface. */
	bxe_nvram_enable_access(sc);

	cmd_flags = (MCPR_NVM_COMMAND_FIRST | MCPR_NVM_COMMAND_LAST);
	align_offset = (offset & ~0x03);
	rc = bxe_nvram_read_dword(sc, align_offset, &val, cmd_flags);

	if (rc == 0) {
		val &= ~(0xff << BYTE_OFFSET(offset));
		val |= (*data_buf << BYTE_OFFSET(offset));

		val = be32toh(val);
		rc = bxe_nvram_write_dword(sc, align_offset, val, cmd_flags);
	}

	/* Disable access to the NVRAM interface. */
	bxe_nvram_disable_access(sc);
	bxe_nvram_release_lock(sc);

bxe_nvram_write1_exit:
	DBEXIT(BXE_VERBOSE_NVRAM);
	return (rc);
}

/*
 * Returns:
 *   0 = Success, !0 = Failure.
 */
static int
bxe_nvram_write(struct bxe_softc *sc, uint32_t offset, uint8_t *data_buf,
    int buf_size)
{
	uint32_t cmd_flags, val, written_so_far;
	int rc;

	rc = 0;

	if (buf_size == 1)
		return (bxe_nvram_write1(sc, offset, data_buf, buf_size));

	if ((offset & 0x03) || (buf_size & 0x03) || (buf_size == 0)) {
		DBPRINT(sc, BXE_WARN, "%s(): Unaligned address or invalid "
		    "buffer for NVRAM write "
		    "(offset = 0x%08X, buf_size = %d)!\n", __FUNCTION__,
		    offset, buf_size);
		rc = EINVAL;
		goto bxe_nvram_write_exit;
	}

	if (offset + buf_size > sc->common.flash_size) {
		DBPRINT(sc, BXE_WARN, "%s(): Write extends beyond the end of "
		    "the NVRAM (offset (0x%08X) + buf_size (%d) > flash_size "
		    "(0x%08X))!\n", __FUNCTION__, offset, buf_size,
		    sc->common.flash_size);
		rc = EINVAL;
		goto bxe_nvram_write_exit;
	}

	/* Request access to NVRAM interface. */
	rc = bxe_nvram_acquire_lock(sc);
	if (rc)
		goto bxe_nvram_write_exit;

	/* Enable access to the NVRAM interface. */
	bxe_nvram_enable_access(sc);

	written_so_far = 0;
	cmd_flags = MCPR_NVM_COMMAND_FIRST;
	while ((written_so_far < buf_size) && (rc == 0)) {
		if (written_so_far == (buf_size - sizeof(uint32_t)))
			cmd_flags |= MCPR_NVM_COMMAND_LAST;
		else if (((offset + 4) % NVRAM_PAGE_SIZE) == 0)
			cmd_flags |= MCPR_NVM_COMMAND_LAST;
		else if ((offset % NVRAM_PAGE_SIZE) == 0)
			cmd_flags |= MCPR_NVM_COMMAND_FIRST;

		memcpy(&val, data_buf, 4);

		rc = bxe_nvram_write_dword(sc, offset, val, cmd_flags);

		/* Advance to the next DWORD. */
		offset += sizeof(uint32_t);
		data_buf += sizeof(uint32_t);
		written_so_far += sizeof(uint32_t);
		cmd_flags = 0;
	}

	/* Disable access to the NVRAM interface. */
	bxe_nvram_disable_access(sc);
	bxe_nvram_release_lock(sc);

bxe_nvram_write_exit:
	DBEXIT(BXE_VERBOSE_NVRAM);
	return (rc);
}
#endif

/*
 * This function validates NVRAM content by reading spcific
 * regions and validating that the NVRAM checksum matches the
 * actual content.
 *
 * Returns:
 *   0 = Success, !0 = Failure.
 */
static int
bxe_nvram_test(struct bxe_softc *sc)
{
	static const struct {
		int offset;
		int size;
	} nvram_tbl[] = {
		{     0,  0x14 }, /* bootstrap area*/
		{  0x14,  0xec }, /* directory area */
		{ 0x100, 0x350 }, /* manuf_info */
		{ 0x450,  0xf0 }, /* feature_info */
		{ 0x640,  0x64 }, /* upgrade_key_info */
		{ 0x708,  0x70 }, /* manuf_key_info */
		{     0,     0 }
	};
	uint32_t magic, csum, buf[0x350 / 4];
	uint8_t *data;
	int i, rc;

	DBENTER(BXE_VERBOSE_NVRAM);

	data = (uint8_t *) buf;

	/* Read the DWORD at offset 0 in NVRAM. */
	rc = bxe_nvram_read(sc, 0, data, 4);
	if (rc) {
		BXE_PRINTF("%s(%d): Error (%d) returned reading NVRAM!\n",
		    __FILE__, __LINE__, rc);
		goto bxe_nvram_test_exit;
	}

	/* Make sure we found our magic value. */
	magic = be32toh(buf[0]);
	if (magic != 0x669955aa) {
		BXE_PRINTF("%s(%d): Invalid magic value (0x%08x) found!\n",
		    __FILE__, __LINE__, magic);
		rc = ENODEV;
		goto bxe_nvram_test_exit;
	}

	/* Read through each region in NVRAM and validate the checksum. */
	for (i = 0; nvram_tbl[i].size; i++) {
		DBPRINT(sc, BXE_VERBOSE_NVRAM, "%s(): Testing NVRAM region %d, "
		    "starting offset = %d, length = %d\n", __FUNCTION__, i,
		    nvram_tbl[i].offset, nvram_tbl[i].size);

		rc = bxe_nvram_read(sc, nvram_tbl[i].offset, data,
			nvram_tbl[i].size);
		if (rc) {
			BXE_PRINTF("%s(%d): Error (%d) returned reading NVRAM "
			    "region %d!\n", __FILE__, __LINE__, rc, i);
			goto bxe_nvram_test_exit;
		}

		csum = ether_crc32_le(data, nvram_tbl[i].size);
		if (csum != BXE_CRC32_RESIDUAL) {
			BXE_PRINTF("%s(%d): Checksum error (0x%08X) for NVRAM "
			    "region %d!\n", __FILE__, __LINE__, csum, i);
			rc = ENODEV;
			goto bxe_nvram_test_exit;
		}
	}

bxe_nvram_test_exit:
	DBEXIT(BXE_VERBOSE_NVRAM);
	return (rc);
}

/*
 * Acknowledge status block and modify interrupt mode.
 *
 * Returns:
 *   None.
 */
static __inline void
bxe_ack_sb(struct bxe_softc *sc, uint8_t sb_id, uint8_t storm, uint16_t index,
    uint8_t int_mode, uint8_t update)
{
	struct igu_ack_register igu_ack;
	uint32_t hc_addr;

	DBPRINT(sc, BXE_VERBOSE_INTR, "%s(): sb_id = %d, storm = %d, "
	    "index = %d, int_mode = %d, update = %d.\n", __FUNCTION__, sb_id,
	    storm, index, int_mode, update);

	hc_addr = (HC_REG_COMMAND_REG + BP_PORT(sc) * 32 + COMMAND_REG_INT_ACK);
	igu_ack.status_block_index = index;
	igu_ack.sb_id_and_flags =
	    ((sb_id << IGU_ACK_REGISTER_STATUS_BLOCK_ID_SHIFT) |
	    (storm << IGU_ACK_REGISTER_STORM_ID_SHIFT) |
	    (update << IGU_ACK_REGISTER_UPDATE_INDEX_SHIFT) |
	    (int_mode << IGU_ACK_REGISTER_INTERRUPT_MODE_SHIFT));

	rmb();

	DBPRINT(sc, BXE_VERBOSE_INTR,
	    "%s(): Writing 0x%08X to HC addr 0x%08X\n", __FUNCTION__,
	    (*(uint32_t *) &igu_ack), hc_addr);

	REG_WR(sc, hc_addr, (*(uint32_t *) &igu_ack));
	wmb();
}

/*
 * Update fastpath status block index.
 *
 * Returns:
 *   0
 */
static __inline uint16_t
bxe_update_fpsb_idx(struct bxe_fastpath *fp)
{
	struct host_status_block *fpsb;
	uint16_t rc;

	fpsb = fp->status_block;
	rc = 0;

	rmb();

	/* Check for any CSTORM transmit completions. */
	if (fp->fp_c_idx != le16toh(fpsb->c_status_block.status_block_index)) {
		fp->fp_c_idx = le16toh(fpsb->c_status_block.status_block_index);
		rc |= 0x1;
	}

	/* Check for any USTORM receive completions. */
	if (fp->fp_u_idx != le16toh(fpsb->u_status_block.status_block_index)) {
		fp->fp_u_idx = le16toh(fpsb->u_status_block.status_block_index);
		rc |= 0x2;
	}

	return (rc);
}

/*
 * Acknowledge interrupt.
 *
 * Returns:
 *   Interrupt value read from IGU.
 */
static uint16_t
bxe_ack_int(struct bxe_softc *sc)
{
	uint32_t hc_addr, result;

	hc_addr = HC_REG_COMMAND_REG + BP_PORT(sc) * 32 + COMMAND_REG_SIMD_MASK;
	result = REG_RD(sc, hc_addr);
	DBPRINT(sc, BXE_INSANE_INTR, "%s(): Read 0x%08X from HC addr 0x%08X\n",
	    __FUNCTION__, result, hc_addr);

	return (result);
}

/*
 * Slowpath event handler.
 *
 * Checks that a ramrod completion occurs while	the
 * controller is in the proper state.
 *
 * Returns:
 *   None.
 */
static void
bxe_sp_event(struct bxe_fastpath *fp, union eth_rx_cqe *rr_cqe)
{
	struct bxe_softc *sc;
	int cid, command;

	sc = fp->sc;
	DBENTER(BXE_VERBOSE_RAMROD);

	cid = SW_CID(rr_cqe->ramrod_cqe.conn_and_cmd_data);
	command = CQE_CMD(rr_cqe->ramrod_cqe.conn_and_cmd_data);
	DBPRINT(sc, BXE_VERBOSE_RAMROD, "%s(): CID = %d, ramrod command = %d, "
	    "device state = 0x%08X, fp[%d].state = 0x%08X, type = %d\n",
	    __FUNCTION__, cid, command, sc->state, fp->index, fp->state,
	    rr_cqe->ramrod_cqe.ramrod_type);

	/* Free up an entry on the slowpath queue. */
	sc->spq_left++;

	/* Handle ramrod commands that completed on a client connection. */
	if (fp->index) {
		/* Check for a completion for the current state. */
		switch (command | fp->state) {
		case (RAMROD_CMD_ID_ETH_CLIENT_SETUP | BXE_FP_STATE_OPENING):
			DBPRINT(sc, BXE_VERBOSE_RAMROD,
			    "%s(): Completed fp[%d] CLIENT_SETUP Ramrod.\n",
			    __FUNCTION__, cid);
			fp->state = BXE_FP_STATE_OPEN;
			break;
		case (RAMROD_CMD_ID_ETH_HALT | BXE_FP_STATE_HALTING):
			DBPRINT(sc, BXE_VERBOSE_RAMROD,
			    "%s(): Completed fp[%d] ETH_HALT ramrod\n",
			    __FUNCTION__, cid);
			fp->state = BXE_FP_STATE_HALTED;
			break;
		default:
			DBPRINT(sc, BXE_VERBOSE_RAMROD,
			    "%s(): Unexpected microcode reply (%d) while "
			    "in state 0x%04X!\n", __FUNCTION__, command,
			    fp->state);
		}

		goto bxe_sp_event_exit;
	}

	/* Handle ramrod commands that completed on the leading connection. */
	switch (command | sc->state) {
	case (RAMROD_CMD_ID_ETH_PORT_SETUP | BXE_STATE_OPENING_WAIT4_PORT):
		DBPRINT(sc, BXE_VERBOSE_RAMROD,
		    "%s(): Completed PORT_SETUP ramrod.\n", __FUNCTION__);
		sc->state = BXE_STATE_OPEN;
		break;
	case (RAMROD_CMD_ID_ETH_HALT | BXE_STATE_CLOSING_WAIT4_HALT):
		DBPRINT(sc, BXE_VERBOSE_RAMROD,
		    "%s(): Completed ETH_HALT ramrod.\n", __FUNCTION__);
		sc->state = BXE_STATE_CLOSING_WAIT4_DELETE;
		fp->state = BXE_FP_STATE_HALTED;
		break;
	case (RAMROD_CMD_ID_ETH_CFC_DEL | BXE_STATE_CLOSING_WAIT4_HALT):
		DBPRINT(sc, BXE_VERBOSE_RAMROD,
		    "%s(): Completed fp[%d] ETH_CFC_DEL ramrod.\n",
		    __FUNCTION__, cid);
		sc->fp[cid].state = BXE_FP_STATE_CLOSED;
		break;
	case (RAMROD_CMD_ID_ETH_SET_MAC | BXE_STATE_OPEN):
		DBPRINT(sc, BXE_VERBOSE_RAMROD,
		    "%s(): Completed ETH_SET_MAC ramrod in STATE_OPEN state.\n",
		    __FUNCTION__);
		break;
	case (RAMROD_CMD_ID_ETH_SET_MAC | BXE_STATE_CLOSING_WAIT4_HALT):
		DBPRINT(sc, BXE_VERBOSE_RAMROD,
		    "%s(): Completed ETH_SET_MAC ramrod in "
		    "CLOSING_WAIT4_HALT state.\n", __FUNCTION__);
		break;
	default:
		DBPRINT(sc, BXE_FATAL, "%s(): Unexpected microcode reply (%d)! "
		    "State is 0x%08X\n", __FUNCTION__, command, sc->state);
	}

bxe_sp_event_exit:
	/* Force bxe_wait_ramrod() to see the change. */
	mb();
	DBEXIT(BXE_VERBOSE_RAMROD);
}

/*
 * Lock access to a hardware resource using controller arbitration
 * register.
 *
 * Returns:
 *   0 = Success, !0 = Failure.
 */
static int
bxe_acquire_hw_lock(struct bxe_softc *sc, uint32_t resource)
{
	uint32_t hw_lock_control_reg, lock_status, resource_bit;
	uint8_t func;
	int cnt, rc;

	DBENTER(BXE_VERBOSE_MISC);
	DBPRINT(sc, BXE_VERBOSE_MISC, "%s(): Locking resource 0x%08X\n",
	    __FUNCTION__, resource);

	func = BP_FUNC(sc);
	resource_bit = 1 << resource;
	rc = 0;

	hw_lock_control_reg = ((func <= 5) ?
	    (MISC_REG_DRIVER_CONTROL_1 + func * 8) :
	    (MISC_REG_DRIVER_CONTROL_7 + (func - 6) * 8));

	/* Validating that the resource is within range. */
	if (resource > HW_LOCK_MAX_RESOURCE_VALUE) {
		DBPRINT(sc, BXE_INFO, "%s(): Resource is out of range! "
		    "resource(0x%08X) > HW_LOCK_MAX_RESOURCE_VALUE(0x%08X)\n",
		    __FUNCTION__, resource, HW_LOCK_MAX_RESOURCE_VALUE);
		rc = EINVAL;
		goto bxe_acquire_hw_lock_exit;
	}

	/* Validating that the resource is not already taken. */
	lock_status = REG_RD(sc, hw_lock_control_reg);
	if (lock_status & resource_bit) {
		DBPRINT(sc, BXE_INFO, "%s(): Failed to acquire lock! "
		    "lock_status = 0x%08X, resource_bit = 0x%08X\n",
		    __FUNCTION__, lock_status, resource_bit);
		rc = EEXIST;
		goto bxe_acquire_hw_lock_exit;
	}

	/* Try for 5 seconds every 5ms. */
	for (cnt = 0; cnt < 1000; cnt++) {
		/* Try to acquire the lock. */
		REG_WR(sc, hw_lock_control_reg + 4, resource_bit);
		lock_status = REG_RD(sc, hw_lock_control_reg);

		if (lock_status & resource_bit)
			goto bxe_acquire_hw_lock_exit;
		DELAY(5000);
	}

	DBPRINT(sc, BXE_INFO, "%s(): Timeout!\n", __FUNCTION__);
	rc = EAGAIN;

bxe_acquire_hw_lock_exit:
	DBEXIT(BXE_VERBOSE_MISC);
	return (rc);
}

/*
 * Unlock access to a hardware resource using controller arbitration
 * register.
 *
 * Returns:
 *   0 = Success, !0 = Failure.
 */
static int
bxe_release_hw_lock(struct bxe_softc *sc, uint32_t resource)
{
	uint32_t hw_lock_control_reg, lock_status, resource_bit;
	uint8_t func;
	int rc;

	DBENTER(BXE_VERBOSE_MISC);
	DBPRINT(sc, BXE_VERBOSE_MISC, "%s(): Unlocking resource 0x%08X\n",
		__FUNCTION__, resource);

	resource_bit = 1 << resource;
	func = BP_FUNC(sc);
	rc = 0;
	/* Validating that the resource is within range */
	if (resource > HW_LOCK_MAX_RESOURCE_VALUE) {
		DBPRINT(sc, BXE_INFO, "%s(): Resource is out of range! "
		    "resource(0x%08X) > HW_LOCK_MAX_RESOURCE_VALUE(0x%08X)\n",
		    __FUNCTION__, resource, HW_LOCK_MAX_RESOURCE_VALUE);
		rc = EINVAL;
		goto bxe_release_hw_lock_exit;
	}

	/* Find the register for the resource lock. */
	hw_lock_control_reg = ((func <= 5) ?
	    (MISC_REG_DRIVER_CONTROL_1 + func * 8) :
	    (MISC_REG_DRIVER_CONTROL_7 + (func - 6) * 8));

	/* Validating that the resource is currently taken */
	lock_status = REG_RD(sc, hw_lock_control_reg);
	if (!(lock_status & resource_bit)) {
		DBPRINT(sc, BXE_INFO, "%s(): The resource is not currently "
		    "locked! lock_status = 0x%08X, resource_bit = 0x%08X\n",
		    __FUNCTION__, lock_status, resource_bit);
		rc = EFAULT;
		goto bxe_release_hw_lock_exit;
	}

	/* Free the hardware lock. */
	REG_WR(sc, hw_lock_control_reg, resource_bit);

bxe_release_hw_lock_exit:
	DBEXIT(BXE_VERBOSE_MISC);
	return (rc);
}

int
bxe_get_gpio(struct bxe_softc *sc, int gpio_num, uint8_t port)
{
	uint32_t gpio_mask, gpio_reg;
	int gpio_port, gpio_shift, value;

	/* The GPIO should be swapped if swap register is set and active */
	gpio_port = (REG_RD(sc, NIG_REG_PORT_SWAP) && REG_RD(sc,
	    NIG_REG_STRAP_OVERRIDE)) ^ port;
	gpio_shift = gpio_num +
	    (gpio_port ? MISC_REGISTERS_GPIO_PORT_SHIFT : 0);
	gpio_mask = 1 << gpio_shift;

	if (gpio_num > MISC_REGISTERS_GPIO_3) {
		DBPRINT(sc, BXE_WARN, "%s(): Invalid GPIO %d\n",
		    __FUNCTION__, gpio_num);
		return (-EINVAL);
	}

	/* read GPIO value */
	gpio_reg = REG_RD(sc, MISC_REG_GPIO);

	/* get the requested pin value */
	if ((gpio_reg & gpio_mask) == gpio_mask)
		value = 1;
	else
		value = 0;

	DBPRINT(sc, BXE_VERBOSE_PHY, "pin %d  value 0x%x\n", gpio_num, value);

	return (value);
}

/*
 * Sets the state of a General Purpose I/O (GPIO).
 *
 * Returns:
 *   None.
 */
int
bxe_set_gpio(struct bxe_softc *sc, int gpio_num, uint32_t mode, uint8_t port)
{
	uint32_t gpio_reg, gpio_mask;
	int gpio_port, gpio_shift, rc;

	DBENTER(BXE_VERBOSE_MISC);

	/* The GPIO should be swapped if swap register is set and active. */
	gpio_port = (REG_RD(sc, NIG_REG_PORT_SWAP) && REG_RD(sc,
	    NIG_REG_STRAP_OVERRIDE)) ^ port;
	gpio_shift = gpio_num +
	    (gpio_port ? MISC_REGISTERS_GPIO_PORT_SHIFT : 0);
	gpio_mask = (1 << gpio_shift);
	rc = 0;

	if (gpio_num > MISC_REGISTERS_GPIO_3) {
		DBPRINT(sc, BXE_FATAL, "%s(): Invalid GPIO (%d)!\n",
		    __FUNCTION__, gpio_num);
		rc = EINVAL;
		goto bxe_set_gpio_exit;
	}

	/* Make sure no one else is trying to use the GPIO. */
	rc = bxe_acquire_hw_lock(sc, HW_LOCK_RESOURCE_GPIO);
	if (rc) {
		DBPRINT(sc, BXE_WARN, "%s(): Can't acquire GPIO lock!\n",
		    __FUNCTION__);
		goto bxe_set_gpio_exit;
	}

	/* Read GPIO and mask all but the float bits. */
	gpio_reg = (REG_RD(sc, MISC_REG_GPIO) & MISC_REGISTERS_GPIO_FLOAT);

	switch (mode) {
	case MISC_REGISTERS_GPIO_OUTPUT_LOW:
		DBPRINT(sc, BXE_VERBOSE, "%s(): Set GPIO %d (shift %d) -> "
		    "output low\n", __FUNCTION__, gpio_num, gpio_shift);
		gpio_reg &= ~(gpio_mask << MISC_REGISTERS_GPIO_FLOAT_POS);
		gpio_reg |=  (gpio_mask << MISC_REGISTERS_GPIO_CLR_POS);
		break;
	case MISC_REGISTERS_GPIO_OUTPUT_HIGH:
		DBPRINT(sc, BXE_VERBOSE, "%s(): Set GPIO %d (shift %d) -> "
		    "output high\n", __FUNCTION__, gpio_num, gpio_shift);
		gpio_reg &= ~(gpio_mask << MISC_REGISTERS_GPIO_FLOAT_POS);
		gpio_reg |=  (gpio_mask << MISC_REGISTERS_GPIO_SET_POS);
		break;
	case MISC_REGISTERS_GPIO_INPUT_HI_Z:
		DBPRINT(sc, BXE_VERBOSE, "%s(): Set GPIO %d (shift %d) -> "
		    "input\n", __FUNCTION__, gpio_num, gpio_shift);
		gpio_reg |= (gpio_mask << MISC_REGISTERS_GPIO_FLOAT_POS);
		break;
	default:
		DBPRINT(sc, BXE_FATAL, "%s(): Unknown GPIO mode (0x%08X)!\n",
		    __FUNCTION__, mode);
		break;
	}

	REG_WR(sc, MISC_REG_GPIO, gpio_reg);
	rc = bxe_release_hw_lock(sc, HW_LOCK_RESOURCE_GPIO);
	if (rc) {
		DBPRINT(sc, BXE_WARN, "%s(): Can't release GPIO lock!\n",
		    __FUNCTION__);
	}

bxe_set_gpio_exit:
	DBEXIT(BXE_VERBOSE_MISC);
	return (rc);
}

int
bxe_set_gpio_int(struct bxe_softc *sc, int gpio_num, uint32_t mode,
    uint8_t port)
{
	uint32_t gpio_mask, gpio_reg;
	int gpio_port, gpio_shift;

	/* The GPIO should be swapped if swap register is set and active */
	gpio_port = (REG_RD(sc, NIG_REG_PORT_SWAP) && REG_RD(sc,
	    NIG_REG_STRAP_OVERRIDE)) ^ port;
	gpio_shift = gpio_num +
	    (gpio_port ? MISC_REGISTERS_GPIO_PORT_SHIFT : 0);
	gpio_mask = (1 << gpio_shift);
	if (gpio_num > MISC_REGISTERS_GPIO_3) {
		DBPRINT(sc, BXE_WARN, "%s(): Invalid GPIO %d\n",
		    __FUNCTION__, gpio_num);
		return (-EINVAL);
	}

	bxe_acquire_hw_lock(sc, HW_LOCK_RESOURCE_GPIO);
	/* read GPIO int */
	gpio_reg = REG_RD(sc, MISC_REG_GPIO_INT);

	switch (mode) {
	case MISC_REGISTERS_GPIO_INT_OUTPUT_CLR:
		DBPRINT(sc, BXE_VERBOSE_PHY, "Clear GPIO INT %d (shift %d) -> "
		    "output low\n", gpio_num, gpio_shift);
		/* clear SET and set CLR */
		gpio_reg &= ~(gpio_mask << MISC_REGISTERS_GPIO_INT_SET_POS);
		gpio_reg |=  (gpio_mask << MISC_REGISTERS_GPIO_INT_CLR_POS);
		break;
	case MISC_REGISTERS_GPIO_INT_OUTPUT_SET:
		DBPRINT(sc, BXE_VERBOSE_PHY, "Set GPIO INT %d (shift %d) -> "
		    "output high\n", gpio_num, gpio_shift);
		/* clear CLR and set SET */
		gpio_reg &= ~(gpio_mask << MISC_REGISTERS_GPIO_INT_CLR_POS);
		gpio_reg |=  (gpio_mask << MISC_REGISTERS_GPIO_INT_SET_POS);
		break;
	default:
		break;
	}

	REG_WR(sc, MISC_REG_GPIO_INT, gpio_reg);
	bxe_release_hw_lock(sc, HW_LOCK_RESOURCE_GPIO);

	return (0);
}

/*
 * Sets the state of a Shared Purpose I/O (SPIO).
 *
 * Returns:
 *   0 = Success, !0 = Failure.
 */
int
bxe_set_spio(struct bxe_softc *sc, int spio_num, uint32_t mode)
{
	uint32_t spio_reg, spio_mask;
	int rc;

	DBENTER(BXE_VERBOSE_MISC);

	rc = 0;
	spio_mask = 1 << spio_num;

	/* Validate the SPIO. */
	if ((spio_num < MISC_REGISTERS_SPIO_4) ||
	    (spio_num > MISC_REGISTERS_SPIO_7)) {
		DBPRINT(sc, BXE_FATAL, "%s(): Invalid SPIO (%d)!\n",
		    __FUNCTION__, spio_num);
		rc = EINVAL;
		goto bxe_set_spio_exit;
	}

	rc = bxe_acquire_hw_lock(sc, HW_LOCK_RESOURCE_SPIO);
	if (rc) {
		DBPRINT(sc, BXE_WARN, "%s(): Can't acquire SPIO lock!\n",
		    __FUNCTION__);
		goto bxe_set_spio_exit;
	}

	/* Read SPIO and mask all but the float bits. */
	spio_reg = (REG_RD(sc, MISC_REG_SPIO) & MISC_REGISTERS_SPIO_FLOAT);

	switch (mode) {
	case MISC_REGISTERS_SPIO_OUTPUT_LOW :
		DBPRINT(sc, BXE_INFO, "%s(): Set SPIO %d -> output low\n",
		    __FUNCTION__, spio_num);
		spio_reg &= ~(spio_mask << MISC_REGISTERS_SPIO_FLOAT_POS);
		spio_reg |=  (spio_mask << MISC_REGISTERS_SPIO_CLR_POS);
		break;
	case MISC_REGISTERS_SPIO_OUTPUT_HIGH :
		DBPRINT(sc, BXE_INFO,  "%s(): Set SPIO %d -> output high\n",
		    __FUNCTION__, spio_num);
		spio_reg &= ~(spio_mask << MISC_REGISTERS_SPIO_FLOAT_POS);
		spio_reg |=  (spio_mask << MISC_REGISTERS_SPIO_SET_POS);
		break;
	case MISC_REGISTERS_SPIO_INPUT_HI_Z:
		DBPRINT(sc, BXE_INFO, "%s(): Set SPIO %d -> input\n",
		    __FUNCTION__, spio_num);
		spio_reg |= (spio_mask << MISC_REGISTERS_SPIO_FLOAT_POS);
		break;
	default:
		DBPRINT(sc, BXE_FATAL, "%s(): Unknown SPIO mode (0x%08X)!\n",
		    __FUNCTION__, mode);
		break;
	}

	REG_WR(sc, MISC_REG_SPIO, spio_reg);
	rc = bxe_release_hw_lock(sc, HW_LOCK_RESOURCE_SPIO);
	if (rc) {
		DBPRINT(sc, BXE_WARN, "%s(): Can't release SPIO lock!\n",
		    __FUNCTION__);
	}

bxe_set_spio_exit:
	DBEXIT(BXE_VERBOSE_MISC);
	return (rc);
}

/*
 * When the 57711E is operating in multi-function mode, the controller
 * must be configured to arbitrate TX between multiple VNICs.
 *
 * Returns:
 *   None.
 */
static void
bxe_init_port_minmax(struct bxe_softc *sc)
{
	uint32_t fair_periodic_timeout_usec, r_param, t_fair;

	DBENTER(BXE_VERBOSE_MISC);

	r_param = sc->link_vars.line_speed / 8;

	memset(&(sc->cmng.rs_vars), 0,
	    sizeof(struct rate_shaping_vars_per_port));
	memset(&(sc->cmng.fair_vars), 0, sizeof(struct fairness_vars_per_port));

	/* 100 usec in SDM ticks = 25 since each tick is 4 usec. */
	sc->cmng.rs_vars.rs_periodic_timeout = RS_PERIODIC_TIMEOUT_USEC / 4;
	/*
	 * This is the threshold below which no timer arming will occur.
	 * We use a coefficient of 1, 25 so that the threshold is a
	 * little bigger that real time to compensate for timer
	 * in-accuracy.
	 */
	sc->cmng.rs_vars.rs_threshold = (RS_PERIODIC_TIMEOUT_USEC *
	    r_param * 5) / 4;
	/* Resolution of fairness timer. */
	fair_periodic_timeout_usec = QM_ARB_BYTES / r_param;

	/* For 10G it is 1000us, for 1G it is 10000us. */
	t_fair = T_FAIR_COEF / sc->link_vars.line_speed;
	/* This is the threshold where we won't arm the timer
	   anymore. */
	sc->cmng.fair_vars.fair_threshold = QM_ARB_BYTES;
	/*
	 * Multiply by 1e3/8 to get bytes/msec. We don't want the
	 * credits to pass a credit of the T_FAIR*FAIR_MEM (algorithm
	 * resolution)
	 */
	sc->cmng.fair_vars.upper_bound = r_param * t_fair * FAIR_MEM;
	/* Since each tick is 4 us. */
	sc->cmng.fair_vars.fairness_timeout = fair_periodic_timeout_usec / 4;

	DBEXIT(BXE_VERBOSE_MISC);
}


/*
 * This function is called when a link interrupt is generated
 * and configures the controller for the new link state.
 *
 * Returns:
 *   None.
 */
static void
bxe_link_attn(struct bxe_softc *sc)
{
	struct host_port_stats *pstats;
	uint32_t pause_enabled;
	int func, i, port, vn;

	DBENTER(BXE_VERBOSE_PHY);

	/* Make sure that we are synced with the current statistics. */
	bxe_stats_handle(sc, STATS_EVENT_STOP);

	bxe_link_update(&sc->link_params, &sc->link_vars);

	if (sc->link_vars.link_up) {
		if (CHIP_IS_E1H(sc)) {
			port = BP_PORT(sc);
			pause_enabled = 0;

			if (sc->link_vars.flow_ctrl & FLOW_CTRL_TX)
				pause_enabled = 1;

			REG_WR(sc, BAR_USTORM_INTMEM +
			    USTORM_ETH_PAUSE_ENABLED_OFFSET(port),
			    pause_enabled);
		}

		if (sc->link_vars.mac_type == MAC_TYPE_BMAC) {
			pstats = BXE_SP(sc, port_stats);
			/* Reset old BMAC statistics. */
			memset(&(pstats->mac_stx[0]), 0,
			    sizeof(struct mac_stx));
		}

		if ((sc->state == BXE_STATE_OPEN) ||
		    (sc->state == BXE_STATE_DISABLED))
			bxe_stats_handle(sc, STATS_EVENT_LINK_UP);
	}

	/* Report the new link status. */
	bxe_link_report(sc);

	/* Need additional handling for multi-function devices. */
	if (IS_E1HMF(sc)) {
		port = BP_PORT(sc);
		if (sc->link_vars.link_up) {
			if (sc->dcc_enable == TRUE) {
				bxe_congestionmgmt(sc, TRUE);
				/* Store in internal memory. */
				for (i = 0; i <
				    sizeof(struct cmng_struct_per_port) / 4;
				    i++) {
					REG_WR(sc, BAR_XSTORM_INTMEM +
				XSTORM_CMNG_PER_PORT_VARS_OFFSET(port) + (i*4),
					    ((uint32_t *)(&sc->cmng))[i]);
				}
			}
		}
		for (vn = VN_0; vn < E1HVN_MAX; vn++) {
			/* Don't send an attention to ourselves. */
			if (vn == BP_E1HVN(sc))
				continue;
			func = ((vn << 1) | port);
			/*
			 * Send an attention to other drivers on the same port.
			 */
			REG_WR(sc, MISC_REG_AEU_GENERAL_ATTN_0 +
			    (LINK_SYNC_ATTENTION_BIT_FUNC_0 + func) * 4, 1);
		}
	}

	DBEXIT(BXE_VERBOSE_PHY);
}

/*
 * Sets the driver instance as the port management function (PMF).
 *
 * This is only used on "multi-function" capable devices such as the
 * 57711E and initializes the controller so that the PMF driver instance
 * can interact with other driver instances that may be operating on
 * the same Ethernet port.
 *
 * Returns:
 *   None.
 */
static void
bxe_pmf_update(struct bxe_softc *sc)
{
	uint32_t val;
	int port;

	DBENTER(BXE_VERBOSE_INTR);

	/* Record that this driver instance is managing the port. */
	sc->port.pmf = 1;
	DBPRINT(sc, BXE_INFO, "%s(): Enabling port management function.\n",
	    __FUNCTION__);

	/* Enable NIG attention. */
	port = BP_PORT(sc);
	val = (0xff0f | (1 << (BP_E1HVN(sc) + 4)));
	REG_WR(sc, HC_REG_TRAILING_EDGE_0 + port * 8, val);
	REG_WR(sc, HC_REG_LEADING_EDGE_0 + port * 8, val);

	bxe_stats_handle(sc, STATS_EVENT_PMF);

	DBEXIT(BXE_VERBOSE_INTR);
}

/* 8073 Download definitions */
/* spi Parameters.*/
#define	SPI_CTRL_1_L	0xC000
#define	SPI_CTRL_1_H	0xC002
#define	SPI_CTRL_2_L	0xC400
#define	SPI_CTRL_2_H	0xC402
#define	SPI_TXFIFO	0xD000
#define	SPI_RXFIFO	0xD400

/* Input Command Messages.*/
/*
 * Write CPU/SPI Control Regs, followed by Count And CPU/SPI Controller
 * Reg add/data pairs.
 */
#define	WR_CPU_CTRL_REGS	0x11
/*
 * Read CPU/SPI Control Regs, followed by Count and CPU/SPI Controller
 * Register Add.
 */
#define	RD_CPU_CTRL_REGS	0xEE
/*
 * Write CPU/SPI Control Regs Continously, followed by Count and
 * CPU/SPI Controller Reg addr and data's.
 */
#define	WR_CPU_CTRL_FIFO	0x66
/* Output Command Messages.*/
#define	DONE			0x4321

/* SPI Controller Commands (known As messages).*/
#define	MSGTYPE_HWR	0x40
#define	MSGTYPE_HRD	0x80
#define	WRSR_OPCODE	0x01
#define	WR_OPCODE	0x02
#define	RD_OPCODE	0x03
#define	WRDI_OPCODE	0x04
#define	RDSR_OPCODE	0x05
#define	WREN_OPCODE	0x06
#define	WR_BLOCK_SIZE	0x40	/* Maximum 64 Bytes Writes.*/

/*
 * Post a slowpath command.
 *
 * A slowpath command is used to propogate a configuration change through
 * the controller in a controlled manner, allowing each STORM processor and
 * other H/W blocks to phase in the change.  The commands sent on the
 * slowpath are referred to as ramrods.  Depending on the ramrod used the
 * completion of the ramrod will occur in different ways.  Here's a
 * breakdown of ramrods and how they complete:
 *
 * RAMROD_CMD_ID_ETH_PORT_SETUP
 *   Used to setup the leading connection on a port.  Completes on the
 *   Receive Completion Queue (RCQ) of that port (typically fp[0]).
 *
 * RAMROD_CMD_ID_ETH_CLIENT_SETUP
 *   Used to setup an additional connection on a port.  Completes on the
 *   RCQ of the multi-queue/RSS connection being initialized.
 *
 * RAMROD_CMD_ID_ETH_STAT_QUERY
 *   Used to force the storm processors to update the statistics database
 *   in host memory.  This ramrod is send on the leading connection CID and
 *   completes as an index increment of the CSTORM on the default status
 *   block.
 *
 * RAMROD_CMD_ID_ETH_UPDATE
 *   Used to update the state of the leading connection, usually to udpate
 *   the RSS indirection table.  Completes on the RCQ of the leading
 *   connection. (Not currently used under FreeBSD until OS support becomes
 *   available.)
 *
 * RAMROD_CMD_ID_ETH_HALT
 *   Used when tearing down a connection prior to driver unload.  Completes
 *   on the RCQ of the multi-queue/RSS connection being torn down.  Don't
 *   use this on the leading connection.
 *
 * RAMROD_CMD_ID_ETH_SET_MAC
 *   Sets the Unicast/Broadcast/Multicast used by the port.  Completes on
 *   the RCQ of the leading connection.
 *
 * RAMROD_CMD_ID_ETH_CFC_DEL
 *   Used when tearing down a conneciton prior to driver unload.  Completes
 *   on the RCQ of the leading connection (since the current connection
 *   has been completely removed from controller memory).
 *
 * RAMROD_CMD_ID_ETH_PORT_DEL
 *   Used to tear down the leading connection prior to driver unload,
 *   typically fp[0].  Completes as an index increment of the CSTORM on the
 *   default status block.
 *
 * RAMROD_CMD_ID_ETH_FORWARD_SETUP
 *   Used for connection offload.  Completes on the RCQ of the multi-queue
 *   RSS connection that is being offloaded.  (Not currently used under
 *   FreeBSD.)
 *
 * There can only be one command pending per function.
 *
 * Returns:
 *   0 = Success, !0 = Failure.
 */
static int
bxe_sp_post(struct bxe_softc *sc, int command, int cid, uint32_t data_hi,
    uint32_t data_lo, int common)
{
	int func, rc;

	DBENTER(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET | BXE_VERBOSE_RAMROD);

	DBRUNMSG(BXE_VERBOSE_RAMROD, bxe_decode_ramrod_cmd(sc, command));

	DBPRINT(sc, BXE_VERBOSE_RAMROD, "%s(): cid = %d, data_hi = 0x%08X, "
	    "data_low = 0x%08X, remaining spq entries = %d\n", __FUNCTION__,
	    cid, data_hi, data_lo, sc->spq_left);

	rc = 0;
	/* Skip all slowpath commands if the driver has panic'd. */
	if (sc->panic) {
		rc = EIO;
		goto bxe_sp_post_exit;
	}

	BXE_SP_LOCK(sc);

	/* We are limited to 8 slowpath commands. */
	if (!sc->spq_left) {
		BXE_PRINTF("%s(%d): Slowpath queue is full!\n",
		    __FILE__, __LINE__);
		bxe_panic_dump(sc);
		rc = EBUSY;
		goto bxe_sp_post_exit;
	}

	/* Encode the CID with the command. */
	sc->spq_prod_bd->hdr.conn_and_cmd_data =
	    htole32(((command << SPE_HDR_CMD_ID_SHIFT) | HW_CID(sc, cid)));
	sc->spq_prod_bd->hdr.type = htole16(ETH_CONNECTION_TYPE);

	if (common)
		sc->spq_prod_bd->hdr.type |=
		    htole16((1 << SPE_HDR_COMMON_RAMROD_SHIFT));

	/* Point the hardware at the new configuration data. */
	sc->spq_prod_bd->data.mac_config_addr.hi = htole32(data_hi);
	sc->spq_prod_bd->data.mac_config_addr.lo = htole32(data_lo);

	/* Reduce the number of available slots for slowpath commands. */
	sc->spq_left--;

	/* Manage the end of the ring. */
	if (sc->spq_prod_bd == sc->spq_last_bd) {
		sc->spq_prod_bd = sc->spq;
		sc->spq_prod_idx = 0;
		DBPRINT(sc, BXE_VERBOSE, "%s(): End of slowpath queue.\n",
		    __FUNCTION__);
	} else {
		sc->spq_prod_bd++;
		sc->spq_prod_idx++;
	}

	func = BP_FUNC(sc);
	/* Kick off the slowpath command. */
	REG_WR(sc, BAR_XSTORM_INTMEM + XSTORM_SPQ_PROD_OFFSET(func),
	    sc->spq_prod_idx);

bxe_sp_post_exit:
	BXE_SP_UNLOCK(sc);

	DBEXIT(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET | BXE_VERBOSE_RAMROD);

	return (rc);
}

/*
 * Acquire the MCP access lock.
 *
 * Returns:
 *   0 = Success, !0 = Failure.
 */
static int
bxe_acquire_alr(struct bxe_softc *sc)
{
	uint32_t val;
	int i, rc, retries;

	DBENTER(BXE_VERBOSE_MISC);

	rc = 0;
	retries = 100;
	/* Acquire lock using mcpr_access_lock SPLIT register. */
	for (i = 0; i < retries * 10; i++) {
		val = 1UL << 31;
		REG_WR(sc, GRCBASE_MCP + 0x9c, val);
		val = REG_RD(sc, GRCBASE_MCP + 0x9c);

		if (val & (1L << 31))
			break;

		DELAY(5000);
	}

	if (!(val & (1L << 31))) {
		DBPRINT(sc, BXE_WARN,
		    "%s(): Cannot acquire MCP split access lock.\n",
		    __FUNCTION__);
		rc = EBUSY;
	}

	DBEXIT(BXE_VERBOSE_MISC);
	return (rc);
}


/*
 * Release the MCP access lock.
 *
 * Returns:
 *   None.
 */
static void
bxe_release_alr(struct bxe_softc* sc)
{

	DBENTER(BXE_VERBOSE_MISC);

	REG_WR(sc, GRCBASE_MCP + 0x9c, 0);

	DBEXIT(BXE_VERBOSE_MISC);
}

/*
 * Update driver's copies of the values in the host default status block.
 *
 * Returns:
 *   Bitmap indicating changes to the block.
 */
static __inline uint16_t
bxe_update_dsb_idx(struct bxe_softc *sc)
{
	struct host_def_status_block *dsb;
	uint16_t rc;

	rc = 0;
	dsb = sc->def_status_block;
	/* Read memory barrier since block is written by hardware. */
	rmb();

	if (sc->def_att_idx !=
	    le16toh(dsb->atten_status_block.attn_bits_index)) {
		sc->def_att_idx =
		    le16toh(dsb->atten_status_block.attn_bits_index);
		rc |= 0x1;
	}

	if (sc->def_c_idx !=
	    le16toh(dsb->c_def_status_block.status_block_index)) {
		sc->def_c_idx =
		    le16toh(dsb->c_def_status_block.status_block_index);
		rc |= 0x2;
	}

	if (sc->def_u_idx !=
	    le16toh(dsb->u_def_status_block.status_block_index)) {
		sc->def_u_idx =
		    le16toh(dsb->u_def_status_block.status_block_index);
		rc |= 0x4;
	}

	if (sc->def_x_idx !=
	    le16toh(dsb->x_def_status_block.status_block_index)) {
		sc->def_x_idx =
		    le16toh(dsb->x_def_status_block.status_block_index);
		rc |= 0x8;
	}

	if (sc->def_t_idx !=
	    le16toh(dsb->t_def_status_block.status_block_index)) {
		sc->def_t_idx =
		    le16toh(dsb->t_def_status_block.status_block_index);
		rc |= 0x10;
	}

	return (rc);
}

/*
 * Handle any attentions that have been newly asserted.
 *
 * Returns:
 *   None
 */
static void
bxe_attn_int_asserted(struct bxe_softc *sc, uint32_t asserted)
{
	uint32_t aeu_addr, hc_addr, nig_int_mask_addr;
	uint32_t aeu_mask, nig_mask;
	int port, rc;

	DBENTER(BXE_VERBOSE_INTR);

	port = BP_PORT(sc);
	hc_addr = (HC_REG_COMMAND_REG + port * 32 + COMMAND_REG_ATTN_BITS_SET);
	aeu_addr = port ? MISC_REG_AEU_MASK_ATTN_FUNC_1 :
	    MISC_REG_AEU_MASK_ATTN_FUNC_0;
	nig_int_mask_addr = port ? NIG_REG_MASK_INTERRUPT_PORT1 :
	    NIG_REG_MASK_INTERRUPT_PORT0;
	nig_mask = 0;

	if (sc->attn_state & asserted)
		BXE_PRINTF("%s(%d): IGU attention ERROR!\n",
		    __FILE__, __LINE__);

	rc = bxe_acquire_hw_lock(sc,
		HW_LOCK_RESOURCE_PORT0_ATT_MASK + port);
	if (rc) {
		DBPRINT(sc, BXE_WARN,
		    "%s(): Failed to acquire attention lock for port %d!\n",
		    __FUNCTION__, port);
		goto bxe_attn_int_asserted_exit;
	}

	aeu_mask = REG_RD(sc, aeu_addr);
	DBPRINT(sc, BXE_VERBOSE_INTR,
	    "%s(): aeu_mask = 0x%08X, newly asserted = 0x%08X\n", __FUNCTION__,
	    aeu_mask, asserted);

	aeu_mask &= ~(asserted & 0xff);
	DBPRINT(sc, BXE_VERBOSE_INTR, "%s(): new mask = 0x%08X\n", __FUNCTION__,
	    aeu_mask);
	REG_WR(sc, aeu_addr, aeu_mask);

	rc = bxe_release_hw_lock(sc, HW_LOCK_RESOURCE_PORT0_ATT_MASK + port);
	if (rc) {
		DBPRINT(sc, BXE_WARN,
		    "%s(): Failed to release attention lock!\n", __FUNCTION__);
		goto bxe_attn_int_asserted_exit;
	}

	DBPRINT(sc, BXE_VERBOSE_INTR, "%s(): attn_state = 0x%08X\n",
	    __FUNCTION__, sc->attn_state);

	sc->attn_state |= asserted;
	DBPRINT(sc, BXE_VERBOSE_INTR, "%s(): new attn_state = 0x%08X\n",
	    __FUNCTION__, sc->attn_state);

	if (asserted & ATTN_HARD_WIRED_MASK) {
		if (asserted & ATTN_NIG_FOR_FUNC) {
			bxe_acquire_phy_lock(sc);

			/* Save NIG interrupt mask. */
			nig_mask = REG_RD(sc, nig_int_mask_addr);
			REG_WR(sc, nig_int_mask_addr, 0);

			bxe_link_attn(sc);
		}

		if (asserted & ATTN_SW_TIMER_4_FUNC)
			DBPRINT(sc, BXE_WARN, "%s(): ATTN_SW_TIMER_4_FUNC!\n",
			    __FUNCTION__);

		if (asserted & GPIO_2_FUNC)
			DBPRINT(sc, BXE_WARN, "%s(): GPIO_2_FUNC!\n",
			    __FUNCTION__);

		if (asserted & GPIO_3_FUNC)
			DBPRINT(sc, BXE_WARN, "%s(): GPIO_3_FUNC!\n",
			    __FUNCTION__);

		if (asserted & GPIO_4_FUNC)
			DBPRINT(sc, BXE_WARN, "%s(): GPIO_4_FUNC!\n",
			    __FUNCTION__);

		if (port == 0) {
			if (asserted & ATTN_GENERAL_ATTN_1) {
				DBPRINT(sc, BXE_WARN,
				    "%s(): ATTN_GENERAL_ATTN_1!\n",
				    __FUNCTION__);
				REG_WR(sc, MISC_REG_AEU_GENERAL_ATTN_1, 0x0);
			}

			if (asserted & ATTN_GENERAL_ATTN_2) {
				DBPRINT(sc, BXE_WARN,
				    "%s(): ATTN_GENERAL_ATTN_2!\n",
				    __FUNCTION__);
				REG_WR(sc, MISC_REG_AEU_GENERAL_ATTN_2, 0x0);
			}

			if (asserted & ATTN_GENERAL_ATTN_3) {
				DBPRINT(sc, BXE_WARN,
				    "%s(): ATTN_GENERAL_ATTN_3!\n",
				    __FUNCTION__);
				REG_WR(sc, MISC_REG_AEU_GENERAL_ATTN_3, 0x0);
			}
		} else {
			if (asserted & ATTN_GENERAL_ATTN_4) {
				DBPRINT(sc, BXE_WARN,
				    "%s(): ATTN_GENERAL_ATTN_4!\n",
				    __FUNCTION__);
				REG_WR(sc, MISC_REG_AEU_GENERAL_ATTN_4, 0x0);
			}

			if (asserted & ATTN_GENERAL_ATTN_5) {
				DBPRINT(sc, BXE_WARN,
				    "%s(): ATTN_GENERAL_ATTN_5!\n",
				    __FUNCTION__);
				REG_WR(sc, MISC_REG_AEU_GENERAL_ATTN_5, 0x0);
			}
			if (asserted & ATTN_GENERAL_ATTN_6) {
				DBPRINT(sc, BXE_WARN,
				    "%s(): ATTN_GENERAL_ATTN_6!\n",
				    __FUNCTION__);
				REG_WR(sc, MISC_REG_AEU_GENERAL_ATTN_6, 0x0);
			}
		}
	}

	DBPRINT(sc, BXE_VERBOSE_INTR,
	    "%s(): Writing 0x%08X to HC addr 0x%08X\n", __FUNCTION__,
	    asserted, hc_addr);
	REG_WR(sc, hc_addr, asserted);

	/* Now set back the NIG mask. */
	if (asserted & ATTN_NIG_FOR_FUNC) {
		REG_WR(sc, nig_int_mask_addr, nig_mask);
		bxe_release_phy_lock(sc);
	}

bxe_attn_int_asserted_exit:
	DBEXIT(BXE_VERBOSE_INTR);
}

/*
 * Handle any attentions that have been newly deasserted.
 *
 * Returns:
 *   None
 */
static __inline void
bxe_attn_int_deasserted0(struct bxe_softc *sc, uint32_t attn)
{
	uint32_t val, swap_val, swap_override;
	int port, reg_offset;

	DBENTER(BXE_VERBOSE_INTR);

	port = BP_PORT(sc);
	reg_offset = port ? MISC_REG_AEU_ENABLE1_FUNC_1_OUT_0 :
	    MISC_REG_AEU_ENABLE1_FUNC_0_OUT_0;

	/* Handle SPIO5 attention. */
	if (attn & AEU_INPUTS_ATTN_BITS_SPIO5) {
		val = REG_RD(sc, reg_offset);
		val &= ~AEU_INPUTS_ATTN_BITS_SPIO5;
		REG_WR(sc, reg_offset, val);

		DBPRINT(sc, BXE_FATAL, "%s(): SPIO5 H/W attention!\n",
		    __FUNCTION__);
		/* Fan failure attention */
		switch (XGXS_EXT_PHY_TYPE(sc->link_params.ext_phy_config)) {
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_SFX7101:
			/*
			 * SPIO5 is used on A1022G boards to indicate
			 * fan failure.  Shutdown the controller and
			 * associated PHY to avoid damage.
			 */

			/* Low power mode is controled by GPIO 2. */
			bxe_set_gpio(sc, MISC_REGISTERS_GPIO_2,
				MISC_REGISTERS_GPIO_OUTPUT_LOW, port);
			/* PHY reset is controled by GPIO 1. */
			bxe_set_gpio(sc, MISC_REGISTERS_GPIO_1,
				MISC_REGISTERS_GPIO_OUTPUT_LOW, port);
			break;
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727:
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8481:
			/*
			 * The PHY reset is controlled by GPIO 1.
			 * Fake the port number to cancel the swap done in
			 * set_gpio().
			 */
			swap_val = REG_RD(sc, NIG_REG_PORT_SWAP);
			swap_override = REG_RD(sc, NIG_REG_STRAP_OVERRIDE);
			port = (swap_val && swap_override) ^ 1;
			bxe_set_gpio(sc, MISC_REGISTERS_GPIO_1,
			    MISC_REGISTERS_GPIO_OUTPUT_LOW, port);
			break;
		default:
			break;
		}

		/* Mark the failure. */
		sc->link_params.ext_phy_config &=
		    ~PORT_HW_CFG_XGXS_EXT_PHY_TYPE_MASK;
		sc->link_params.ext_phy_config |=
		    PORT_HW_CFG_XGXS_EXT_PHY_TYPE_FAILURE;
		SHMEM_WR(sc, dev_info.port_hw_config[port].external_phy_config,
		    sc->link_params.ext_phy_config);
		/* Log the failure */
		BXE_PRINTF("A fan failure has caused the driver to "
		    "shutdown the device to prevent permanent damage.\n");
	}

	if (attn & (AEU_INPUTS_ATTN_BITS_GPIO3_FUNCTION_0 |
	    AEU_INPUTS_ATTN_BITS_GPIO3_FUNCTION_1)) {
		bxe_acquire_phy_lock(sc);
		bxe_handle_module_detect_int(&sc->link_params);
		bxe_release_phy_lock(sc);
	}

	/* Checking for an assert on the zero block */
	if (attn & HW_INTERRUT_ASSERT_SET_0) {
		val = REG_RD(sc, reg_offset);
		val &= ~(attn & HW_INTERRUT_ASSERT_SET_0);
		REG_WR(sc, reg_offset, val);

		BXE_PRINTF("%s(%d): FATAL hardware block attention "
		    "(set0 = 0x%08X)!\n", __FILE__, __LINE__,
		    (attn & (uint32_t)HW_INTERRUT_ASSERT_SET_0));

		bxe_panic_dump(sc);
	}

	DBEXIT(BXE_VERBOSE_INTR);
}

/*
 * Handle any attentions that have been newly deasserted.
 *
 * Returns:
 *   None
 */
static __inline void
bxe_attn_int_deasserted1(struct bxe_softc *sc, uint32_t attn)
{
	uint32_t val;
	int port, reg_offset;

	DBENTER(BXE_VERBOSE_INTR);

	if (attn & AEU_INPUTS_ATTN_BITS_DOORBELLQ_HW_INTERRUPT) {
		val = REG_RD(sc, DORQ_REG_DORQ_INT_STS_CLR);

		DBPRINT(sc, BXE_FATAL,
		    "%s(): Doorbell hardware attention (0x%08X).\n",
		    __FUNCTION__, val);

		/* DORQ discard attention */
		if (val & 0x2)
			DBPRINT(sc, BXE_FATAL,
			    "%s(): FATAL doorbell queue error!\n",
			    __FUNCTION__);
	}

	if (attn & HW_INTERRUT_ASSERT_SET_1) {
		port = BP_PORT(sc);
		reg_offset = port ? MISC_REG_AEU_ENABLE1_FUNC_1_OUT_1 :
		    MISC_REG_AEU_ENABLE1_FUNC_0_OUT_1;

		val = REG_RD(sc, reg_offset);
		val &= ~(attn & HW_INTERRUT_ASSERT_SET_1);
		REG_WR(sc, reg_offset, val);

		BXE_PRINTF("%s(%d): FATAL hardware block attention "
		    "(set1 = 0x%08X)!\n", __FILE__, __LINE__,
		    (attn & (uint32_t)HW_INTERRUT_ASSERT_SET_1));

		bxe_panic_dump(sc);
	}

	DBEXIT(BXE_VERBOSE_INTR);
}

/*
 * Handle any attentions that have been newly deasserted.
 *
 * Returns:
 *   None
 */
static __inline void
bxe_attn_int_deasserted2(struct bxe_softc *sc, uint32_t attn)
{
	uint32_t val;
	int port, reg_offset;

	DBENTER(BXE_VERBOSE_INTR);

	if (attn & AEU_INPUTS_ATTN_BITS_CFC_HW_INTERRUPT) {
		val = REG_RD(sc, CFC_REG_CFC_INT_STS_CLR);

		DBPRINT(sc, BXE_FATAL,
		    "%s(): CFC hardware attention (0x%08X).\n", __FUNCTION__,
		    val);

		/* CFC error attention. */
		if (val & 0x2)
			DBPRINT(sc, BXE_FATAL, "%s(): FATAL CFC error!\n",
			    __FUNCTION__);
	}

	if (attn & AEU_INPUTS_ATTN_BITS_PXP_HW_INTERRUPT) {
		val = REG_RD(sc, PXP_REG_PXP_INT_STS_CLR_0);

		DBPRINT(sc, BXE_FATAL,
		    "%s(): PXP hardware attention (0x%08X).\n", __FUNCTION__,
		    val);

		/* RQ_USDMDP_FIFO_OVERFLOW */
		if (val & 0x18000)
			DBPRINT(sc, BXE_FATAL, "%s(): FATAL PXP error!\n",
			    __FUNCTION__);
	}

	if (attn & HW_INTERRUT_ASSERT_SET_2) {
		port = BP_PORT(sc);
		reg_offset = port ? MISC_REG_AEU_ENABLE1_FUNC_1_OUT_2 :
		    MISC_REG_AEU_ENABLE1_FUNC_0_OUT_2;

		val = REG_RD(sc, reg_offset);
		val &= ~(attn & HW_INTERRUT_ASSERT_SET_2);
		REG_WR(sc, reg_offset, val);

		BXE_PRINTF("%s(%d): FATAL hardware block attention (set2 = "
		    "0x%08X)! port=%d, val written=0x%x attn=0x%x\n", __FILE__,
		    __LINE__, (attn & (uint32_t)HW_INTERRUT_ASSERT_SET_2),
		    port, val, attn);

		bxe_panic_dump(sc);
	}

	DBEXIT(BXE_VERBOSE_INTR);
}

/*
 * Handle any attentions that have been newly deasserted.
 *
 * Returns:
 *   None
 */
static __inline void
bxe_attn_int_deasserted3(struct bxe_softc *sc, uint32_t attn)
{
	uint32_t val;
	int func;

	DBENTER(BXE_VERBOSE_INTR);

	if (attn & EVEREST_GEN_ATTN_IN_USE_MASK) {
		/* Look for any port assertions. */
		if (attn & BXE_PMF_LINK_ASSERT) {
			/*
			 * We received a message from the driver instance
			 * that is managing the Ethernet port (link up/down).
			 * Go ahead and handle it.
			 */
			func = BP_FUNC(sc);

			DBPRINT(sc, BXE_INFO,
			    "%s(): Received link attention from PMF.\n",
			    __FUNCTION__);

			/* Clear the attention. */
			REG_WR(sc, MISC_REG_AEU_GENERAL_ATTN_12 + func * 4, 0);
			sc->mf_config[BP_E1HVN(sc)] =
			    SHMEM_RD(sc,
			    mf_cfg.func_mf_config[(sc->bxe_func & 1)].config);
			val = SHMEM_RD(sc, func_mb[func].drv_status);
			if (sc->dcc_enable == TRUE) {
				if (val & DRV_STATUS_DCC_EVENT_MASK)
					bxe_dcc_event(sc,
					    val & DRV_STATUS_DCC_EVENT_MASK);
			}
			bxe__link_status_update(sc);

			if ((sc->port.pmf == 0) && (val & DRV_STATUS_PMF))
				bxe_pmf_update(sc);
		/* Look for any microcode assertions. */
		} else if (attn & BXE_MC_ASSERT_BITS) {
			DBPRINT(sc, BXE_FATAL, "%s(): Microcode assert!\n",
			    __FUNCTION__);

			REG_WR(sc, MISC_REG_AEU_GENERAL_ATTN_10, 0);
			REG_WR(sc, MISC_REG_AEU_GENERAL_ATTN_9, 0);
			REG_WR(sc, MISC_REG_AEU_GENERAL_ATTN_8, 0);
			REG_WR(sc, MISC_REG_AEU_GENERAL_ATTN_7, 0);

			bxe_panic_dump(sc);

		/* Look for any bootcode assertions. */
		} else if (attn & BXE_MCP_ASSERT) {
			DBPRINT(sc, BXE_FATAL, "%s(): Bootcode assert!\n",
				__FUNCTION__);

			REG_WR(sc, MISC_REG_AEU_GENERAL_ATTN_11, 0);

			DBRUN(bxe_dump_fw(sc));
		} else
			DBPRINT(sc, BXE_FATAL,
			    "%s(): Unknown hardware assertion "
			    "(attn = 0x%08X)!\n", __FUNCTION__, attn);
	}

	/* Look for any hardware latched attentions. */
	if (attn & EVEREST_LATCHED_ATTN_IN_USE_MASK) {
		DBPRINT(sc, BXE_FATAL,
		    "%s(): Latched attention 0x%08X (masked)!\n", __FUNCTION__,
		    attn);

		/* Check if a GRC register access timeout occurred. */
		if (attn & BXE_GRC_TIMEOUT) {
			val = CHIP_IS_E1H(sc) ? REG_RD(sc,
			    MISC_REG_GRC_TIMEOUT_ATTN) : 0;

			DBPRINT(sc, BXE_WARN,
			    "%s(): GRC timeout for register 0x%08X!\n",
			    __FUNCTION__, val);
		}

		/* Check if a GRC reserved register was accessed. */
		if (attn & BXE_GRC_RSV) {
			val = CHIP_IS_E1H(sc) ? REG_RD(sc,
			    MISC_REG_GRC_RSV_ATTN) : 0;

			DBPRINT(sc, BXE_WARN,
			    "%s(): GRC register 0x%08X is reserved!\n",
			    __FUNCTION__, val);
		}

		REG_WR(sc, MISC_REG_AEU_CLR_LATCH_SIGNAL, 0x7ff);
	}

	DBEXIT(BXE_VERBOSE_INTR);
}

/*
 * Handle any attentions that have been newly deasserted.
 *
 * Returns:
 *   None
 */
static void
bxe_attn_int_deasserted(struct bxe_softc *sc, uint32_t deasserted)
{
	struct attn_route attn;
	struct attn_route group_mask;
	uint32_t val, reg_addr, aeu_mask;
	int index, port;

	DBENTER(BXE_VERBOSE_INTR);

	/*
	 * Need to take HW lock because MCP or other port might also try
	 * to handle this event.
	 */
	bxe_acquire_alr(sc);

	port = BP_PORT(sc);
	/* Get the current attention signal bits. */
	attn.sig[0] = REG_RD(sc,
	    MISC_REG_AEU_AFTER_INVERT_1_FUNC_0 + port * 4);
	attn.sig[1] = REG_RD(sc,
	    MISC_REG_AEU_AFTER_INVERT_2_FUNC_0 + port * 4);
	attn.sig[2] = REG_RD(sc,
	    MISC_REG_AEU_AFTER_INVERT_3_FUNC_0 + port * 4);
	attn.sig[3] = REG_RD(sc,
	    MISC_REG_AEU_AFTER_INVERT_4_FUNC_0 + port * 4);

	DBPRINT(sc, BXE_EXTREME_INTR,
	    "%s(): attention = 0x%08X 0x%08X 0x%08X 0x%08X\n", __FUNCTION__,
	    attn.sig[0], attn.sig[1], attn.sig[2], attn.sig[3]);

	/*
	 * Compare the current attention bits to each attention group
	 * to see if anyone has registered this attention.
	 */
	for (index = 0; index < MAX_DYNAMIC_ATTN_GRPS; index++) {
		if (deasserted & (1 << index)) {
			group_mask = sc->attn_group[index];

			DBPRINT(sc, BXE_EXTREME_INTR,
			    "%s(): group[%02d] = 0x%08X 0x%08X 0x%08x 0X%08x\n",
			    __FUNCTION__, index, group_mask.sig[0],
			    group_mask.sig[1], group_mask.sig[2],
			    group_mask.sig[3]);

			/* Handle any registered attentions. */
			bxe_attn_int_deasserted3(sc,
			    attn.sig[3] & group_mask.sig[3]);
			bxe_attn_int_deasserted1(sc,
			    attn.sig[1] & group_mask.sig[1]);
			bxe_attn_int_deasserted2(sc,
			    attn.sig[2] & group_mask.sig[2]);
			bxe_attn_int_deasserted0(sc,
			    attn.sig[0] & group_mask.sig[0]);

			if ((attn.sig[0] & group_mask.sig[0] &
			    HW_PRTY_ASSERT_SET_0) ||
			    (attn.sig[1] & group_mask.sig[1] &
			    HW_PRTY_ASSERT_SET_1) ||
			    (attn.sig[2] & group_mask.sig[2] &
			    HW_PRTY_ASSERT_SET_2))
				BXE_PRINTF("%s(%d): FATAL hardware block "
				    "parity attention!\n", __FILE__, __LINE__);
		}
	}

	bxe_release_alr(sc);

	reg_addr = (HC_REG_COMMAND_REG +
	    port * 32 + COMMAND_REG_ATTN_BITS_CLR);

	val = ~deasserted;
	DBPRINT(sc, BXE_EXTREME_INTR,
	    "%s(): About to mask 0x%08X at HC addr 0x%08X\n", __FUNCTION__,
	    deasserted, reg_addr);
	REG_WR(sc, reg_addr, val);

	if (~sc->attn_state & deasserted)
		DBPRINT(sc, BXE_FATAL, "%s(): IGU Bug!\n", __FUNCTION__);

	reg_addr = port ? MISC_REG_AEU_MASK_ATTN_FUNC_1 :
	    MISC_REG_AEU_MASK_ATTN_FUNC_0;

	bxe_acquire_hw_lock(sc, HW_LOCK_RESOURCE_PORT0_ATT_MASK + port);
	aeu_mask = REG_RD(sc, reg_addr);

	DBPRINT(sc, BXE_EXTREME_INTR,
	    "%s(): Current aeu_mask = 0x%08X, newly deasserted = 0x%08X\n",
	    __FUNCTION__, aeu_mask, deasserted);
	aeu_mask |= (deasserted & 0xff);

	DBPRINT(sc, BXE_EXTREME_INTR, "%s(): New aeu_mask = 0x%08X\n",
	    __FUNCTION__, aeu_mask);

	REG_WR(sc, reg_addr, aeu_mask);
	bxe_release_hw_lock(sc, HW_LOCK_RESOURCE_PORT0_ATT_MASK + port);

	DBPRINT(sc, BXE_EXTREME_INTR, "%s(): Current attn_state = 0x%08X\n",
	    __FUNCTION__, sc->attn_state);

	sc->attn_state &= ~deasserted;
	DBPRINT(sc, BXE_EXTREME_INTR, "%s(): New attn_state = 0x%08X\n",
	    __FUNCTION__, sc->attn_state);

	DBEXIT(BXE_VERBOSE_INTR);
}

/*
 * Handle interrupts caused by internal attentions (everything else other
 * than RX, TX, and link state changes).
 *
 * Returns:
 *   None
 */
static void
bxe_attn_int(struct bxe_softc* sc)
{
	uint32_t attn_ack, attn_bits, attn_state;
	uint32_t asserted, deasserted;

	DBENTER(BXE_VERBOSE_INTR);

	attn_bits = le32toh(sc->def_status_block->atten_status_block.attn_bits);
	attn_ack =
	    le32toh(sc->def_status_block->atten_status_block.attn_bits_ack);
	attn_state = sc->attn_state;
	asserted = attn_bits & ~attn_ack & ~attn_state;
	deasserted = ~attn_bits &  attn_ack &  attn_state;

	/* Make sure we're in a sane state. */
	if (~(attn_bits ^ attn_ack) & (attn_bits ^ attn_state))
	    BXE_PRINTF("%s(%d): Bad attention state!\n",
	    	__FILE__, __LINE__);

	/* Handle any attentions that are newly asserted. */
	if (asserted) {
		DBPRINT(sc, BXE_VERBOSE_INTR,
		    "%s(): attn_state = 0x%08X, attn_bits = 0x%08X, "
		    "attn_ack = 0x%08X, asserted = 0x%08X\n", __FUNCTION__,
		    attn_state, attn_bits, attn_ack, asserted);
		bxe_attn_int_asserted(sc, asserted);
	}

	/* Handle any attentions that are newly deasserted. */
	if (deasserted) {
		DBPRINT(sc, BXE_VERBOSE_INTR,
		    "%s(): attn_state = 0x%08X, attn_bits = 0x%08X, "
		    "attn_ack = 0x%08X, deasserted = 0x%08X\n", __FUNCTION__,
		    attn_state, attn_bits, attn_ack, deasserted);
		bxe_attn_int_deasserted(sc, deasserted);
	}

	DBEXIT(BXE_VERBOSE_INTR);
}

/* sum[hi:lo] += add[hi:lo] */
#define	ADD_64(s_hi, a_hi, s_lo, a_lo) do {			\
	s_lo += a_lo;						\
	s_hi += a_hi + (s_lo < a_lo) ? 1 : 0;			\
} while (0)

/* Subtraction = minuend -= subtrahend */
#define	SUB_64(m_hi, s_hi, m_lo, s_lo)				\
	do {							\
		DIFF_64(m_hi, m_hi, s_hi, m_lo, m_lo, s_lo);	\
	} while (0)


/* difference = minuend - subtrahend */
#define	DIFF_64(d_hi, m_hi, s_hi, d_lo, m_lo, s_lo) do {	\
	if (m_lo < s_lo) {					\
		/* underflow */					\
		d_hi = m_hi - s_hi;				\
		if (d_hi > 0) {					\
			/* we can 'loan' 1 */			\
			d_hi--;					\
			d_lo = m_lo + (UINT_MAX - s_lo) + 1;	\
		} else {					\
			/* m_hi <= s_hi */			\
			d_hi = 0;				\
			d_lo = 0;				\
		}						\
	} else {						\
		/* m_lo >= s_lo */				\
		if (m_hi < s_hi) {				\
			d_hi = 0;				\
			d_lo = 0;				\
		} else {					\
			/* m_hi >= s_hi */			\
			d_hi = m_hi - s_hi;			\
			d_lo = m_lo - s_lo;			\
		}						\
	}							\
} while (0)

#define	UPDATE_STAT64(s, t) do {				\
	DIFF_64(diff.hi, new->s##_hi, pstats->mac_stx[0].t##_hi,\
	    diff.lo, new->s##_lo, pstats->mac_stx[0].t##_lo);	\
	pstats->mac_stx[0].t##_hi = new->s##_hi;		\
	pstats->mac_stx[0].t##_lo = new->s##_lo;		\
	ADD_64(pstats->mac_stx[1].t##_hi, diff.hi,		\
	    pstats->mac_stx[1].t##_lo, diff.lo);		\
} while (0)

#define	UPDATE_STAT64_NIG(s, t) do {				\
	DIFF_64(diff.hi, new->s##_hi, old->s##_hi,		\
	    diff.lo, new->s##_lo, old->s##_lo);			\
	ADD_64(estats->t##_hi, diff.hi,				\
	    estats->t##_lo, diff.lo);				\
} while (0)

/* sum[hi:lo] += add */
#define	ADD_EXTEND_64(s_hi, s_lo, a) do {			\
	s_lo += a;						\
	s_hi += (s_lo < a) ? 1 : 0;				\
} while (0)

#define	UPDATE_EXTEND_STAT(s) do {				\
	ADD_EXTEND_64(pstats->mac_stx[1].s##_hi,		\
	    pstats->mac_stx[1].s##_lo, new->s);			\
} while (0)

#define	UPDATE_EXTEND_TSTAT(s, t) do {				\
	diff = (tclient->s) - (old_tclient->s);	\
	old_tclient->s = (tclient->s);				\
	ADD_EXTEND_64(qstats->t##_hi, qstats->t##_lo, diff);	\
} while (0)

#define	UPDATE_EXTEND_XSTAT(s, t) do {				\
	diff = xclient->s - old_xclient->s;	\
	old_xclient->s = xclient->s;				\
	ADD_EXTEND_64(qstats->t##_hi, qstats->t##_lo, diff);	\
} while (0)

#define	UPDATE_EXTEND_USTAT(s, t) do {				\
	diff = uclient->s - old_uclient->s;	\
	old_uclient->s = uclient->s;				\
	ADD_EXTEND_64(qstats->t##_hi, qstats->t##_lo, diff);	\
} while (0)

#define	SUB_EXTEND_64(m_hi, m_lo, s)do {			\
	SUB_64(m_hi, 0, m_lo, s);				\
} while (0)

#define	SUB_EXTEND_USTAT(s, t)do {				\
	diff = (uclient->s) - (old_uclient->s);	\
	SUB_EXTEND_64(qstats->t##_hi, qstats->t##_lo, diff);	\
} while (0)




#ifdef __i386__
#define	BITS_PER_LONG	32
#else /*Only support x86_64(AMD64 and EM64T)*/
#define	BITS_PER_LONG	64
#endif

static __inline long
bxe_hilo(uint32_t *hiref)
{
	uint32_t lo;

	lo = *(hiref + 1);
#if (BITS_PER_LONG == 64)
	uint32_t hi = *hiref;
	return (HILO_U64(hi, lo));
#else
	return (lo);
#endif
}

/*
 * Request the STORM statistics by posting a slowpath ramrod.
 *
 * Returns:
 *   None.
 */
static void
bxe_stats_storm_post(struct bxe_softc *sc)
{
	struct eth_query_ramrod_data ramrod_data = {0};
	int rc;

	DBENTER(BXE_INSANE_STATS);

	if (!sc->stats_pending) {
		ramrod_data.drv_counter = sc->stats_counter++;
		ramrod_data.collect_port = sc->port.pmf ? 1 : 0;
		ramrod_data.ctr_id_vector = (1 << BP_CL_ID(sc));

		rc = bxe_sp_post(sc, RAMROD_CMD_ID_ETH_STAT_QUERY, 0,
		    ((uint32_t *)&ramrod_data)[1],
		    ((uint32_t *)&ramrod_data)[0], 0);

		if (rc == 0) {
			/* Stats ramrod has it's own slot on the SPQ. */
			sc->spq_left++;
			sc->stats_pending = 1;
		}
	}

	DBEXIT(BXE_INSANE_STATS);
}

static void
bxe_stats_port_base_init(struct bxe_softc *sc)
{
	uint32_t *stats_comp;
	struct dmae_command *dmae;

	if (!sc->port.pmf || !sc->port.port_stx) {
		BXE_PRINTF("%s(%d): Invalid statistcs port setup!\n",
		    __FILE__, __LINE__);
		return;
	}

	stats_comp = BXE_SP(sc, stats_comp);

	sc->executer_idx = 0;  /* dmae clients */

	dmae = BXE_SP(sc, dmae[sc->executer_idx++]);
	dmae->opcode = (DMAE_CMD_SRC_PCI | DMAE_CMD_DST_GRC |
			DMAE_CMD_C_DST_PCI | DMAE_CMD_C_ENABLE |
			DMAE_CMD_SRC_RESET | DMAE_CMD_DST_RESET |
#ifdef __BIG_ENDIAN
			DMAE_CMD_ENDIANITY_B_DW_SWAP |
#else
			DMAE_CMD_ENDIANITY_DW_SWAP |
#endif
			(BP_PORT(sc) ? DMAE_CMD_PORT_1 : DMAE_CMD_PORT_0) |
			(BP_E1HVN(sc) << DMAE_CMD_E1HVN_SHIFT));
	dmae->src_addr_lo = U64_LO(BXE_SP_MAPPING(sc, port_stats));
	dmae->src_addr_hi = U64_HI(BXE_SP_MAPPING(sc, port_stats));
	dmae->dst_addr_lo = sc->port.port_stx >> 2;
	dmae->dst_addr_hi = 0;
	dmae->len = sizeof(struct host_port_stats) >> 2;
	dmae->comp_addr_lo = U64_LO(BXE_SP_MAPPING(sc, stats_comp));
	dmae->comp_addr_hi = U64_HI(BXE_SP_MAPPING(sc, stats_comp));
	dmae->comp_val = DMAE_COMP_VAL;

	*stats_comp = 0;
	bxe_stats_hw_post(sc);
	bxe_stats_comp(sc);
}

static void
bxe_stats_func_base_init(struct bxe_softc *sc)
{
	int port, func;
	int vn, vn_max;
	uint32_t func_stx;

	port = BP_PORT(sc);
	func_stx = sc->func_stx;
	vn_max = IS_E1HMF(sc) ? E1HVN_MAX : E1VN_MAX;

	for (vn = VN_0; vn < vn_max; vn++) {
		func = 2*vn + port;
		sc->func_stx = SHMEM_RD(sc, func_mb[func].fw_mb_param);
		bxe_stats_func_init(sc);
		bxe_stats_hw_post(sc);
		bxe_stats_comp(sc);
	}

	sc->func_stx = func_stx;
}

static void
bxe_stats_func_base_update(struct bxe_softc *sc)
{
	uint32_t *stats_comp;
	struct dmae_command *dmae;

	dmae = &sc->stats_dmae;
	stats_comp = BXE_SP(sc, stats_comp);

	sc->executer_idx = 0;
	memset(dmae, 0, sizeof(struct dmae_command));

	dmae->opcode = (DMAE_CMD_SRC_GRC | DMAE_CMD_DST_PCI |
			DMAE_CMD_C_DST_PCI | DMAE_CMD_C_ENABLE |
			DMAE_CMD_SRC_RESET | DMAE_CMD_DST_RESET |
#ifdef __BIG_ENDIAN
			DMAE_CMD_ENDIANITY_B_DW_SWAP |
#else
			DMAE_CMD_ENDIANITY_DW_SWAP |
#endif
			(BP_PORT(sc) ? DMAE_CMD_PORT_1 : DMAE_CMD_PORT_0) |
			(BP_E1HVN(sc) << DMAE_CMD_E1HVN_SHIFT));
	dmae->src_addr_lo = sc->func_stx >> 2;
	dmae->src_addr_hi = 0;
	dmae->dst_addr_lo = U64_LO(BXE_SP_MAPPING(sc, func_stats_base));
	dmae->dst_addr_hi = U64_HI(BXE_SP_MAPPING(sc, func_stats_base));
	dmae->len = sizeof(struct host_func_stats) >> 2;
	dmae->comp_addr_lo = U64_LO(BXE_SP_MAPPING(sc, stats_comp));
	dmae->comp_addr_hi = U64_HI(BXE_SP_MAPPING(sc, stats_comp));
	dmae->comp_val = DMAE_COMP_VAL;

	*stats_comp = 0;
	bxe_stats_hw_post(sc);
	bxe_stats_comp(sc);
}


/*
 * Initialize statistics.
 *
 * Returns:
 *   Nothing.
 */
static void
bxe_stats_init(struct bxe_softc *sc)
{
	struct bxe_fastpath *fp;
	int func, i, port;

	DBENTER(BXE_VERBOSE_STATS);

	if (sc->stats_enable == FALSE)
	    return;

	port = BP_PORT(sc);
	func = BP_FUNC(sc);
	sc->executer_idx  = 0;
	sc->stats_counter = 0;
	sc->stats_pending = 0;

	/* Fetch the offset of port statistics in shared memory. */
	if (BP_NOMCP(sc)){
		sc->port.port_stx = 0;
		sc->func_stx = 0;
	} else{
		sc->port.port_stx = SHMEM_RD(sc, port_mb[port].port_stx);
		sc->func_stx = SHMEM_RD(sc, func_mb[func].fw_mb_param);
	}
	/* If this is still 0 then no management firmware running. */
	DBPRINT(sc, BXE_VERBOSE_STATS, "%s(): sc->port.port_stx = 0x%08X\n",
	    __FUNCTION__, sc->port.port_stx);

	/* port stats */
	memset(&(sc->port.old_nig_stats), 0, sizeof(struct nig_stats));
	sc->port.old_nig_stats.brb_discard = REG_RD(sc,
	    NIG_REG_STAT0_BRB_DISCARD + port * 0x38);
	sc->port.old_nig_stats.brb_truncate = REG_RD(sc,
	    NIG_REG_STAT0_BRB_TRUNCATE + port * 0x38);
	REG_RD_DMAE(sc, NIG_REG_STAT0_EGRESS_MAC_PKT0 + port * 0x50,
	    &(sc->port.old_nig_stats.egress_mac_pkt0_lo), 2);
	REG_RD_DMAE(sc, NIG_REG_STAT0_EGRESS_MAC_PKT1 + port * 0x50,
	    &(sc->port.old_nig_stats.egress_mac_pkt1_lo), 2);

	/* function stats */
	for (i = 0; i < sc->num_queues; i++) {
		fp = &sc->fp[i];
		/* Clear function statistics memory. */
		memset(&fp->old_tclient, 0,
		    sizeof(struct tstorm_per_client_stats));
		memset(&fp->old_uclient, 0,
		    sizeof(struct ustorm_per_client_stats));
		memset(&fp->old_xclient, 0,
		    sizeof(struct xstorm_per_client_stats));
		memset(&fp->eth_q_stats, 0,
		    sizeof(struct bxe_q_stats));
	}

	sc->stats_state = STATS_STATE_DISABLED;

	/* Init port statistics if we're the port management function. */
	if (sc->port.pmf) {
		/* Port_stx are in 57710 when ncsi presnt & always in 57711.*/
		if (sc->port.port_stx)
			bxe_stats_port_base_init(sc);
		if (sc->func_stx)
			bxe_stats_func_base_init(sc);
	} else if (sc->func_stx)
		bxe_stats_func_base_update(sc);

	DBEXIT(BXE_VERBOSE_STATS);
}

/*
 *
 * Returns:
 *   None.
 */
static void
bxe_stats_hw_post(struct bxe_softc *sc)
{
	struct dmae_command *dmae;
	uint32_t *stats_comp;
	int loader_idx;

	DBENTER(BXE_INSANE_STATS);

	dmae = &sc->stats_dmae;
	stats_comp = BXE_SP(sc, stats_comp);
	*stats_comp = DMAE_COMP_VAL;

	if (sc->executer_idx) {
		loader_idx = PMF_DMAE_C(sc);

		memset(dmae, 0, sizeof(struct dmae_command));

		dmae->opcode = (DMAE_CMD_SRC_PCI | DMAE_CMD_DST_GRC |
		    DMAE_CMD_C_DST_GRC | DMAE_CMD_C_ENABLE |
		    DMAE_CMD_DST_RESET |
#ifdef __BIG_ENDIAN
		    DMAE_CMD_ENDIANITY_B_DW_SWAP |
#else
		    DMAE_CMD_ENDIANITY_DW_SWAP |
#endif
		    (BP_PORT(sc) ? DMAE_CMD_PORT_1 : DMAE_CMD_PORT_0) |
		    (BP_E1HVN(sc) << DMAE_CMD_E1HVN_SHIFT));

		dmae->src_addr_lo = U64_LO(BXE_SP_MAPPING(sc, dmae[0]));
		dmae->src_addr_hi = U64_HI(BXE_SP_MAPPING(sc, dmae[0]));
		dmae->dst_addr_lo = (DMAE_REG_CMD_MEM +
		    sizeof(struct dmae_command) * (loader_idx + 1)) >> 2;
		dmae->dst_addr_hi = 0;
		dmae->len = sizeof(struct dmae_command) >> 2;

		if (CHIP_IS_E1(sc))
			dmae->len--;

		dmae->comp_addr_lo = dmae_reg_go_c[loader_idx + 1] >> 2;
		dmae->comp_addr_hi = 0;
		dmae->comp_val = 1;

		*stats_comp = 0;
		bxe_post_dmae(sc, dmae, loader_idx);

	} else if (sc->func_stx) {
		*stats_comp = 0;
		bxe_post_dmae(sc, dmae, INIT_DMAE_C(sc));
	}

	DBEXIT(BXE_INSANE_STATS);
}

/*
 *
 * Returns:
 *   1
 */
static int
bxe_stats_comp(struct bxe_softc *sc)
{
	uint32_t *stats_comp;
	int cnt;

	DBENTER(BXE_VERBOSE_STATS);

	stats_comp = BXE_SP(sc, stats_comp);
	cnt = 10;
	while (*stats_comp != DMAE_COMP_VAL) {
		if (!cnt) {
			BXE_PRINTF("%s(%d): Timeout waiting for statistics "
			    "completions.\n", __FILE__, __LINE__);
			break;
		}
		cnt--;
		DELAY(1000);
	}

	DBEXIT(BXE_VERBOSE_STATS);
	return (1);
}

/*
 * Initialize port statistics.
 *
 * Returns:
 *   None.
 */
static void
bxe_stats_pmf_update(struct bxe_softc *sc)
{
	struct dmae_command *dmae;
	uint32_t opcode, *stats_comp;
	int loader_idx;

	DBENTER(BXE_VERBOSE_STATS);

	stats_comp = BXE_SP(sc, stats_comp);
	loader_idx = PMF_DMAE_C(sc);

	/* We shouldn't be here if any of the following are false. */
	if (!IS_E1HMF(sc) || !sc->port.pmf || !sc->port.port_stx) {
		DBPRINT(sc, BXE_WARN, "%s(): Bug!\n", __FUNCTION__);
		goto bxe_stats_pmf_update_exit;
	}

	sc->executer_idx = 0;

	opcode = (DMAE_CMD_SRC_GRC | DMAE_CMD_DST_PCI |
	    DMAE_CMD_C_DST_PCI | DMAE_CMD_C_ENABLE |
	    DMAE_CMD_SRC_RESET | DMAE_CMD_DST_RESET |
#ifdef __BIG_ENDIAN
	    DMAE_CMD_ENDIANITY_B_DW_SWAP |
#else
	    DMAE_CMD_ENDIANITY_DW_SWAP |
#endif
	    (BP_PORT(sc) ? DMAE_CMD_PORT_1 : DMAE_CMD_PORT_0) |
	    (BP_E1HVN(sc) << DMAE_CMD_E1HVN_SHIFT));

	dmae = BXE_SP(sc, dmae[sc->executer_idx++]);
	dmae->opcode = (opcode | DMAE_CMD_C_DST_GRC);
	dmae->src_addr_lo = sc->port.port_stx >> 2;
	dmae->src_addr_hi = 0;
	dmae->dst_addr_lo = U64_LO(BXE_SP_MAPPING(sc, port_stats));
	dmae->dst_addr_hi = U64_HI(BXE_SP_MAPPING(sc, port_stats));
	dmae->len = DMAE_LEN32_RD_MAX;
	dmae->comp_addr_lo = dmae_reg_go_c[loader_idx] >> 2;
	dmae->comp_addr_hi = 0;
	dmae->comp_val = 1;

	dmae = BXE_SP(sc, dmae[sc->executer_idx++]);
	dmae->opcode = (opcode | DMAE_CMD_C_DST_PCI);
	dmae->src_addr_lo = (sc->port.port_stx >> 2) + DMAE_LEN32_RD_MAX;
	dmae->src_addr_hi = 0;
	dmae->dst_addr_lo = U64_LO(BXE_SP_MAPPING(sc, port_stats) +
	    DMAE_LEN32_RD_MAX * 4);
	dmae->dst_addr_hi = U64_HI(BXE_SP_MAPPING(sc, port_stats) +
	    DMAE_LEN32_RD_MAX * 4);
	dmae->len = (sizeof(struct host_port_stats) >> 2) -
	    DMAE_LEN32_RD_MAX;
	dmae->comp_addr_lo = U64_LO(BXE_SP_MAPPING(sc, stats_comp));
	dmae->comp_addr_hi = U64_HI(BXE_SP_MAPPING(sc, stats_comp));
	dmae->comp_val = DMAE_COMP_VAL;

	*stats_comp = 0;
	bxe_stats_hw_post(sc);
	bxe_stats_comp(sc);

bxe_stats_pmf_update_exit:
	DBEXIT(BXE_VERBOSE_STATS);
}

/*
 * Prepare the DMAE parameters required for port statistics.
 *
 * Returns:
 *   None.
 */
static void
bxe_stats_port_init(struct bxe_softc *sc)
{
	struct dmae_command *dmae;
	uint32_t mac_addr, opcode, *stats_comp;
	int loader_idx, port, vn;

	DBENTER(BXE_VERBOSE_STATS);

	port = BP_PORT(sc);
	vn = BP_E1HVN(sc);
	loader_idx = PMF_DMAE_C(sc);
	stats_comp = BXE_SP(sc, stats_comp);

	/* Sanity check. */
	if (!sc->link_vars.link_up || !sc->port.pmf) {
		BXE_PRINTF("%s(%d): Invalid statistics port setup!\n",
		    __FILE__, __LINE__);
		goto bxe_stats_port_init_exit;
	}

	sc->executer_idx = 0;

	/* Setup statistics reporting to MCP. */
	opcode = (DMAE_CMD_SRC_PCI | DMAE_CMD_DST_GRC |
	    DMAE_CMD_C_DST_GRC | DMAE_CMD_C_ENABLE |
	    DMAE_CMD_SRC_RESET | DMAE_CMD_DST_RESET |
#ifdef __BIG_ENDIAN
	    DMAE_CMD_ENDIANITY_B_DW_SWAP |
#else
	    DMAE_CMD_ENDIANITY_DW_SWAP |
#endif
	    (port ? DMAE_CMD_PORT_1 : DMAE_CMD_PORT_0) |
	    (vn << DMAE_CMD_E1HVN_SHIFT));

	/* Setup the DMA for port statistics. */
	if (sc->port.port_stx) {
		dmae = BXE_SP(sc, dmae[sc->executer_idx++]);
		dmae->opcode = opcode;
		dmae->src_addr_lo = U64_LO(BXE_SP_MAPPING(sc, port_stats));
		dmae->src_addr_hi = U64_HI(BXE_SP_MAPPING(sc, port_stats));
		dmae->dst_addr_lo = sc->port.port_stx >> 2;
		dmae->dst_addr_hi = 0;
		dmae->len = sizeof(struct host_port_stats) >> 2;
		dmae->comp_addr_lo = dmae_reg_go_c[loader_idx] >> 2;
		dmae->comp_addr_hi = 0;
		dmae->comp_val = 1;
	}

	/* Setup the DMA for function statistics. */
	if (sc->func_stx) {
		dmae = BXE_SP(sc, dmae[sc->executer_idx++]);
		dmae->opcode = opcode;
		dmae->src_addr_lo = U64_LO(BXE_SP_MAPPING(sc, func_stats));
		dmae->src_addr_hi = U64_HI(BXE_SP_MAPPING(sc, func_stats));
		dmae->dst_addr_lo = sc->func_stx >> 2;
		dmae->dst_addr_hi = 0;
		dmae->len = sizeof(struct host_func_stats) >> 2;
		dmae->comp_addr_lo = dmae_reg_go_c[loader_idx] >> 2;
		dmae->comp_addr_hi = 0;
		dmae->comp_val = 1;
	}

	/* Setup statistics reporting for the MAC. */
	opcode = (DMAE_CMD_SRC_GRC | DMAE_CMD_DST_PCI |
	    DMAE_CMD_C_DST_GRC | DMAE_CMD_C_ENABLE |
	    DMAE_CMD_SRC_RESET | DMAE_CMD_DST_RESET |
#ifdef __BIG_ENDIAN
	    DMAE_CMD_ENDIANITY_B_DW_SWAP |
#else
	    DMAE_CMD_ENDIANITY_DW_SWAP |
#endif
	    (port ? DMAE_CMD_PORT_1 : DMAE_CMD_PORT_0) |
	    (vn << DMAE_CMD_E1HVN_SHIFT));

	if (sc->link_vars.mac_type == MAC_TYPE_BMAC) {
		/* Enable statistics for the BMAC. */

		mac_addr = (port ? NIG_REG_INGRESS_BMAC1_MEM :
			NIG_REG_INGRESS_BMAC0_MEM);

		/* Setup BMAC TX statistics (TX_STAT_GTPKT .. TX_STAT_GTBYT). */
		dmae = BXE_SP(sc, dmae[sc->executer_idx++]);
		dmae->opcode = opcode;
		dmae->src_addr_lo = (mac_addr +
		    BIGMAC_REGISTER_TX_STAT_GTPKT) >> 2;
		dmae->src_addr_hi = 0;
		dmae->dst_addr_lo = U64_LO(BXE_SP_MAPPING(sc, mac_stats));
		dmae->dst_addr_hi = U64_HI(BXE_SP_MAPPING(sc, mac_stats));
		dmae->len = (8 + BIGMAC_REGISTER_TX_STAT_GTBYT -
		    BIGMAC_REGISTER_TX_STAT_GTPKT) >> 2;
		dmae->comp_addr_lo = dmae_reg_go_c[loader_idx] >> 2;
		dmae->comp_addr_hi = 0;
		dmae->comp_val = 1;

		/* Setup BMAC RX statistcs (RX_STAT_GR64 .. RX_STAT_GRIPJ). */
		dmae = BXE_SP(sc, dmae[sc->executer_idx++]);
		dmae->opcode = opcode;
		dmae->src_addr_lo = (mac_addr +
		    BIGMAC_REGISTER_RX_STAT_GR64) >> 2;
		dmae->src_addr_hi = 0;
		dmae->dst_addr_lo = U64_LO(BXE_SP_MAPPING(sc, mac_stats) +
		    offsetof(struct bmac_stats, rx_stat_gr64_lo));
		dmae->dst_addr_hi = U64_HI(BXE_SP_MAPPING(sc, mac_stats) +
		    offsetof(struct bmac_stats, rx_stat_gr64_lo));
		dmae->len = (8 + BIGMAC_REGISTER_RX_STAT_GRIPJ -
		    BIGMAC_REGISTER_RX_STAT_GR64) >> 2;
		dmae->comp_addr_lo = dmae_reg_go_c[loader_idx] >> 2;
		dmae->comp_addr_hi = 0;
		dmae->comp_val = 1;

	} else if (sc->link_vars.mac_type == MAC_TYPE_EMAC) {
		/* Enable statistics for the EMAC. */

		mac_addr = (port ? GRCBASE_EMAC1 : GRCBASE_EMAC0);

		/* Setup EMAC RX statistics. */
		dmae = BXE_SP(sc, dmae[sc->executer_idx++]);
		dmae->opcode = opcode;
		dmae->src_addr_lo = (mac_addr + EMAC_REG_EMAC_RX_STAT_AC) >> 2;
		dmae->src_addr_hi = 0;
		dmae->dst_addr_lo = U64_LO(BXE_SP_MAPPING(sc, mac_stats));
		dmae->dst_addr_hi = U64_HI(BXE_SP_MAPPING(sc, mac_stats));
		dmae->len = EMAC_REG_EMAC_RX_STAT_AC_COUNT;
		dmae->comp_addr_lo = dmae_reg_go_c[loader_idx] >> 2;
		dmae->comp_addr_hi = 0;
		dmae->comp_val = 1;

		/* Setup additional EMAC RX statistics. */
		dmae = BXE_SP(sc, dmae[sc->executer_idx++]);
		dmae->opcode = opcode;
		dmae->src_addr_lo = (mac_addr +
		    EMAC_REG_EMAC_RX_STAT_AC_28) >> 2;
		dmae->src_addr_hi = 0;
		dmae->dst_addr_lo = U64_LO(BXE_SP_MAPPING(sc, mac_stats) +
		    offsetof(struct emac_stats, rx_stat_falsecarriererrors));
		dmae->dst_addr_hi = U64_HI(BXE_SP_MAPPING(sc, mac_stats) +
		    offsetof(struct emac_stats, rx_stat_falsecarriererrors));
		dmae->len = 1;
		dmae->comp_addr_lo = dmae_reg_go_c[loader_idx] >> 2;
		dmae->comp_addr_hi = 0;
		dmae->comp_val = 1;

		/* Setup EMAC TX statistics. */
		dmae = BXE_SP(sc, dmae[sc->executer_idx++]);
		dmae->opcode = opcode;
		dmae->src_addr_lo = (mac_addr + EMAC_REG_EMAC_TX_STAT_AC) >> 2;
		dmae->src_addr_hi = 0;
		dmae->dst_addr_lo = U64_LO(BXE_SP_MAPPING(sc, mac_stats) +
		    offsetof(struct emac_stats, tx_stat_ifhcoutoctets));
		dmae->dst_addr_hi = U64_HI(BXE_SP_MAPPING(sc, mac_stats) +
		    offsetof(struct emac_stats, tx_stat_ifhcoutoctets));
		dmae->len = EMAC_REG_EMAC_TX_STAT_AC_COUNT;
		dmae->comp_addr_lo = dmae_reg_go_c[loader_idx] >> 2;
		dmae->comp_addr_hi = 0;
		dmae->comp_val = 1;
	} else {
		DBPRINT(sc, BXE_WARN, "%s(): Undefined MAC type.\n",
		    __FUNCTION__);
	}

	/* Enable NIG statistics. */
	dmae = BXE_SP(sc, dmae[sc->executer_idx++]);
	dmae->opcode = opcode;
	dmae->src_addr_lo = (port ? NIG_REG_STAT1_BRB_DISCARD :
	    NIG_REG_STAT0_BRB_DISCARD) >> 2;
	dmae->src_addr_hi = 0;
	dmae->dst_addr_lo = U64_LO(BXE_SP_MAPPING(sc, nig_stats));
	dmae->dst_addr_hi = U64_HI(BXE_SP_MAPPING(sc, nig_stats));
	dmae->len = (sizeof(struct nig_stats) - 4 * sizeof(uint32_t)) >> 2;
	dmae->comp_addr_lo = dmae_reg_go_c[loader_idx] >> 2;
	dmae->comp_addr_hi = 0;
	dmae->comp_val = 1;

	dmae = BXE_SP(sc, dmae[sc->executer_idx++]);
	dmae->opcode = opcode;
	dmae->src_addr_lo = (port ? NIG_REG_STAT1_EGRESS_MAC_PKT0 :
	    NIG_REG_STAT0_EGRESS_MAC_PKT0) >> 2;
	dmae->src_addr_hi = 0;
	dmae->dst_addr_lo = U64_LO(BXE_SP_MAPPING(sc, nig_stats) +
	    offsetof(struct nig_stats, egress_mac_pkt0_lo));
	dmae->dst_addr_hi = U64_HI(BXE_SP_MAPPING(sc, nig_stats) +
	    offsetof(struct nig_stats, egress_mac_pkt0_lo));
	dmae->len = (2 * sizeof(uint32_t)) >> 2;
	dmae->comp_addr_lo = dmae_reg_go_c[loader_idx] >> 2;
	dmae->comp_addr_hi = 0;
	dmae->comp_val = 1;

	dmae = BXE_SP(sc, dmae[sc->executer_idx++]);
	dmae->opcode = (DMAE_CMD_SRC_GRC | DMAE_CMD_DST_PCI |
	    DMAE_CMD_C_DST_PCI | DMAE_CMD_C_ENABLE |
	    DMAE_CMD_SRC_RESET | DMAE_CMD_DST_RESET |
#ifdef __BIG_ENDIAN
	    DMAE_CMD_ENDIANITY_B_DW_SWAP |
#else
	    DMAE_CMD_ENDIANITY_DW_SWAP |
#endif
	    (port ? DMAE_CMD_PORT_1 : DMAE_CMD_PORT_0) |
	    (vn << DMAE_CMD_E1HVN_SHIFT));
	dmae->src_addr_lo = (port ? NIG_REG_STAT1_EGRESS_MAC_PKT1 :
	    NIG_REG_STAT0_EGRESS_MAC_PKT1) >> 2;
	dmae->src_addr_hi = 0;
	dmae->dst_addr_lo = U64_LO(BXE_SP_MAPPING(sc, nig_stats) +
	    offsetof(struct nig_stats, egress_mac_pkt1_lo));
	dmae->dst_addr_hi = U64_HI(BXE_SP_MAPPING(sc, nig_stats) +
	    offsetof(struct nig_stats, egress_mac_pkt1_lo));
	dmae->len = (2 * sizeof(uint32_t)) >> 2;
	dmae->comp_addr_lo = U64_LO(BXE_SP_MAPPING(sc, stats_comp));
	dmae->comp_addr_hi = U64_HI(BXE_SP_MAPPING(sc, stats_comp));
	dmae->comp_val = DMAE_COMP_VAL;

	/* Clear the statistics completion value. */
	*stats_comp = 0;

bxe_stats_port_init_exit:
	DBEXIT(BXE_VERBOSE_STATS);
}

/*
 * Prepare the DMAE parameters required for function statistics.
 *
 * Returns:
 *   None.
 */
static void
bxe_stats_func_init(struct bxe_softc *sc)
{
	struct dmae_command *dmae;
	uint32_t *stats_comp;

	DBENTER(BXE_VERBOSE_STATS);

	dmae = &sc->stats_dmae;
	stats_comp = BXE_SP(sc, stats_comp);

	if (!sc->func_stx) {
		BXE_PRINTF("%s(%d): Invalid statistics function setup!\n",
		     __FILE__, __LINE__);
		goto bxe_stats_func_init_exit;
	}

	sc->executer_idx = 0;
	memset(dmae, 0, sizeof(struct dmae_command));

	/* Setup the DMA for function statistics. */
	dmae->opcode = (DMAE_CMD_SRC_PCI | DMAE_CMD_DST_GRC |
	    DMAE_CMD_C_DST_PCI | DMAE_CMD_C_ENABLE |
	    DMAE_CMD_SRC_RESET | DMAE_CMD_DST_RESET |
#ifdef __BIG_ENDIAN
	    DMAE_CMD_ENDIANITY_B_DW_SWAP |
#else
	    DMAE_CMD_ENDIANITY_DW_SWAP |
#endif
	    (BP_PORT(sc) ? DMAE_CMD_PORT_1 : DMAE_CMD_PORT_0) |
	    (BP_E1HVN(sc) << DMAE_CMD_E1HVN_SHIFT));

	dmae->src_addr_lo = U64_LO(BXE_SP_MAPPING(sc, func_stats));
	dmae->src_addr_hi = U64_HI(BXE_SP_MAPPING(sc, func_stats));
	dmae->dst_addr_lo = sc->func_stx >> 2;
	dmae->dst_addr_hi = 0;
	dmae->len = sizeof(struct host_func_stats) >> 2;
	dmae->comp_addr_lo = U64_LO(BXE_SP_MAPPING(sc, stats_comp));
	dmae->comp_addr_hi = U64_HI(BXE_SP_MAPPING(sc, stats_comp));
	dmae->comp_val = DMAE_COMP_VAL;

	*stats_comp = 0;

bxe_stats_func_init_exit:
	DBEXIT(BXE_VERBOSE_STATS);
}

/*
 * Returns:
 *   None.
 */
static void
bxe_stats_start(struct bxe_softc *sc)
{

	DBENTER(BXE_VERBOSE_STATS);

	if (sc->port.pmf)
		bxe_stats_port_init(sc);

	else if (sc->func_stx)
		bxe_stats_func_init(sc);

	bxe_stats_hw_post(sc);
	bxe_stats_storm_post(sc);

	DBEXIT(BXE_VERBOSE_STATS);
}

/*
 * Returns:
 *   None.
 */
static void
bxe_stats_pmf_start(struct bxe_softc *sc)
{
	DBENTER(BXE_VERBOSE_STATS);

	bxe_stats_comp(sc);
	bxe_stats_pmf_update(sc);
	bxe_stats_start(sc);

	DBEXIT(BXE_VERBOSE_STATS);
}

/*
 * Returns:
 *   None.
 */
static void
bxe_stats_restart(struct bxe_softc *sc)
{

	DBENTER(BXE_VERBOSE_STATS);

	bxe_stats_comp(sc);
	bxe_stats_start(sc);

	DBEXIT(BXE_VERBOSE_STATS);
}

/*
 *
 * Returns:
 *   None.
 */
static void
bxe_stats_bmac_update(struct bxe_softc *sc)
{
	struct bmac_stats *new;
	struct host_port_stats *pstats;
	struct bxe_eth_stats *estats;
	struct regpair diff;

	DBENTER(BXE_INSANE_STATS);

	new = BXE_SP(sc, mac_stats.bmac_stats);
	pstats = BXE_SP(sc, port_stats);
	estats = &sc->eth_stats;

	UPDATE_STAT64(rx_stat_grerb, rx_stat_ifhcinbadoctets);
	UPDATE_STAT64(rx_stat_grfcs, rx_stat_dot3statsfcserrors);
	UPDATE_STAT64(rx_stat_grund, rx_stat_etherstatsundersizepkts);
	UPDATE_STAT64(rx_stat_grovr, rx_stat_dot3statsframestoolong);
	UPDATE_STAT64(rx_stat_grfrg, rx_stat_etherstatsfragments);
	UPDATE_STAT64(rx_stat_grjbr, rx_stat_etherstatsjabbers);
	UPDATE_STAT64(rx_stat_grxcf, rx_stat_maccontrolframesreceived);
	UPDATE_STAT64(rx_stat_grxpf, rx_stat_xoffstateentered);
	UPDATE_STAT64(rx_stat_grxpf, rx_stat_bmac_xpf);
	UPDATE_STAT64(tx_stat_gtxpf, tx_stat_outxoffsent);
	UPDATE_STAT64(tx_stat_gtxpf, tx_stat_flowcontroldone);
	UPDATE_STAT64(tx_stat_gt64, tx_stat_etherstatspkts64octets);
	UPDATE_STAT64(tx_stat_gt127, tx_stat_etherstatspkts65octetsto127octets);
	UPDATE_STAT64(tx_stat_gt255,
	    tx_stat_etherstatspkts128octetsto255octets);
	UPDATE_STAT64(tx_stat_gt511,
	    tx_stat_etherstatspkts256octetsto511octets);
	UPDATE_STAT64(tx_stat_gt1023,
	    tx_stat_etherstatspkts512octetsto1023octets);
	UPDATE_STAT64(tx_stat_gt1518,
	    tx_stat_etherstatspkts1024octetsto1522octets);
	UPDATE_STAT64(tx_stat_gt2047, tx_stat_bmac_2047);
	UPDATE_STAT64(tx_stat_gt4095, tx_stat_bmac_4095);
	UPDATE_STAT64(tx_stat_gt9216, tx_stat_bmac_9216);
	UPDATE_STAT64(tx_stat_gt16383, tx_stat_bmac_16383);
	UPDATE_STAT64(tx_stat_gterr,
	    tx_stat_dot3statsinternalmactransmiterrors);
	UPDATE_STAT64(tx_stat_gtufl, tx_stat_bmac_ufl);

	estats->pause_frames_received_hi =
	    pstats->mac_stx[1].rx_stat_bmac_xpf_hi;
	estats->pause_frames_received_lo =
	    pstats->mac_stx[1].rx_stat_bmac_xpf_lo;

	estats->pause_frames_sent_hi =
	    pstats->mac_stx[1].tx_stat_outxoffsent_hi;
	estats->pause_frames_sent_lo =
	    pstats->mac_stx[1].tx_stat_outxoffsent_lo;

	DBEXIT(BXE_INSANE_STATS);
}

/*
 * Returns:
 *   None.
 */
static void
bxe_stats_emac_update(struct bxe_softc *sc)
{
	struct emac_stats *new;
	struct host_port_stats *pstats;
	struct bxe_eth_stats *estats;

	DBENTER(BXE_INSANE_STATS);

	new = BXE_SP(sc, mac_stats.emac_stats);
	pstats = BXE_SP(sc, port_stats);
	estats = &sc->eth_stats;

	UPDATE_EXTEND_STAT(rx_stat_ifhcinbadoctets);
	UPDATE_EXTEND_STAT(tx_stat_ifhcoutbadoctets);
	UPDATE_EXTEND_STAT(rx_stat_dot3statsfcserrors);
	UPDATE_EXTEND_STAT(rx_stat_dot3statsalignmenterrors);
	UPDATE_EXTEND_STAT(rx_stat_dot3statscarriersenseerrors);
	UPDATE_EXTEND_STAT(rx_stat_falsecarriererrors);
	UPDATE_EXTEND_STAT(rx_stat_etherstatsundersizepkts);
	UPDATE_EXTEND_STAT(rx_stat_dot3statsframestoolong);
	UPDATE_EXTEND_STAT(rx_stat_etherstatsfragments);
	UPDATE_EXTEND_STAT(rx_stat_etherstatsjabbers);
	UPDATE_EXTEND_STAT(rx_stat_maccontrolframesreceived);
	UPDATE_EXTEND_STAT(rx_stat_xoffstateentered);
	UPDATE_EXTEND_STAT(rx_stat_xonpauseframesreceived);
	UPDATE_EXTEND_STAT(rx_stat_xoffpauseframesreceived);
	UPDATE_EXTEND_STAT(tx_stat_outxonsent);
	UPDATE_EXTEND_STAT(tx_stat_outxoffsent);
	UPDATE_EXTEND_STAT(tx_stat_flowcontroldone);
	UPDATE_EXTEND_STAT(tx_stat_etherstatscollisions);
	UPDATE_EXTEND_STAT(tx_stat_dot3statssinglecollisionframes);
	UPDATE_EXTEND_STAT(tx_stat_dot3statsmultiplecollisionframes);
	UPDATE_EXTEND_STAT(tx_stat_dot3statsdeferredtransmissions);
	UPDATE_EXTEND_STAT(tx_stat_dot3statsexcessivecollisions);
	UPDATE_EXTEND_STAT(tx_stat_dot3statslatecollisions);
	UPDATE_EXTEND_STAT(tx_stat_etherstatspkts64octets);
	UPDATE_EXTEND_STAT(tx_stat_etherstatspkts65octetsto127octets);
	UPDATE_EXTEND_STAT(tx_stat_etherstatspkts128octetsto255octets);
	UPDATE_EXTEND_STAT(tx_stat_etherstatspkts256octetsto511octets);
	UPDATE_EXTEND_STAT(tx_stat_etherstatspkts512octetsto1023octets);
	UPDATE_EXTEND_STAT(tx_stat_etherstatspkts1024octetsto1522octets);
	UPDATE_EXTEND_STAT(tx_stat_etherstatspktsover1522octets);
	UPDATE_EXTEND_STAT(tx_stat_dot3statsinternalmactransmiterrors);

	estats->pause_frames_received_hi =
	    pstats->mac_stx[1].rx_stat_xonpauseframesreceived_hi;
	estats->pause_frames_received_lo =
	    pstats->mac_stx[1].rx_stat_xonpauseframesreceived_lo;
	ADD_64(estats->pause_frames_received_hi,
       pstats->mac_stx[1].rx_stat_xoffpauseframesreceived_hi,
       estats->pause_frames_received_lo,
       pstats->mac_stx[1].rx_stat_xoffpauseframesreceived_lo);

	estats->pause_frames_sent_hi =
	    pstats->mac_stx[1].tx_stat_outxonsent_hi;
	estats->pause_frames_sent_lo =
	    pstats->mac_stx[1].tx_stat_outxonsent_lo;
	ADD_64(estats->pause_frames_sent_hi,
	    pstats->mac_stx[1].tx_stat_outxoffsent_hi,
	    estats->pause_frames_sent_lo,
	    pstats->mac_stx[1].tx_stat_outxoffsent_lo);

	DBEXIT(BXE_INSANE_STATS);
}

/*
 * Returns:
 *   0 = Success, !0 = Failure.
 */
static int
bxe_stats_hw_update(struct bxe_softc *sc)
{
	struct nig_stats *new, *old;
	struct host_port_stats *pstats;
	struct bxe_eth_stats *estats;
	struct regpair diff;
	uint32_t nig_timer_max;
	int rc;

	DBENTER(BXE_INSANE_STATS);

	rc = 0;
	new = BXE_SP(sc, nig_stats);
	old = &(sc->port.old_nig_stats);
	pstats = BXE_SP(sc, port_stats);
	estats = &sc->eth_stats;

	/* Update statistics for the active MAC. */
	if (sc->link_vars.mac_type == MAC_TYPE_BMAC)
		bxe_stats_bmac_update(sc);
	else if (sc->link_vars.mac_type == MAC_TYPE_EMAC)
		bxe_stats_emac_update(sc);
	else {
		DBPRINT(sc, BXE_WARN,
		    "%s(): Statistics updated by DMAE but no MAC is active!\n",
		    __FUNCTION__);
		rc = EINVAL;
		goto bxe_stats_hw_update_exit;
	}

	/* Now update the hardware (NIG) statistics. */
	ADD_EXTEND_64(pstats->brb_drop_hi, pstats->brb_drop_lo,
	    new->brb_discard - old->brb_discard);
	ADD_EXTEND_64(estats->brb_truncate_hi, estats->brb_truncate_lo,
	    new->brb_truncate - old->brb_truncate);

	UPDATE_STAT64_NIG(egress_mac_pkt0,
	    etherstatspkts1024octetsto1522octets);
	UPDATE_STAT64_NIG(egress_mac_pkt1, etherstatspktsover1522octets);

	memcpy(old, new, sizeof(struct nig_stats));

	memcpy(&(estats->rx_stat_ifhcinbadoctets_hi), &(pstats->mac_stx[1]),
	    sizeof(struct mac_stx));
	estats->brb_drop_hi = pstats->brb_drop_hi;
	estats->brb_drop_lo = pstats->brb_drop_lo;

	pstats->host_port_stats_start = ++pstats->host_port_stats_end;

	nig_timer_max = SHMEM_RD(sc, port_mb[BP_PORT(sc)].stat_nig_timer);
	if (nig_timer_max != estats->nig_timer_max) {
		estats->nig_timer_max = nig_timer_max;
		DBPRINT(sc, BXE_WARN,
		    "%s(): NIG timer reached max value (%u)!\n", __FUNCTION__,
		    estats->nig_timer_max);
	}

bxe_stats_hw_update_exit:
	DBEXIT(BXE_INSANE_STATS);
	return (rc);
}

/*
 * Returns:
 *   0 = Success, !0 = Failure.
 */
static int
bxe_stats_storm_update(struct bxe_softc *sc)
{
	int rc, i, cl_id;
	struct eth_stats_query *stats;
	struct host_func_stats *fstats;
	struct tstorm_per_port_stats *tport;
	struct tstorm_per_client_stats *tclient;
	struct ustorm_per_client_stats *uclient;
	struct xstorm_per_client_stats *xclient;
	struct tstorm_per_client_stats *old_tclient;
	struct ustorm_per_client_stats *old_uclient;
	struct xstorm_per_client_stats *old_xclient;
	struct bxe_eth_stats *estats;
	struct bxe_q_stats *qstats;
	struct bxe_fastpath * fp;
	uint32_t diff;

	DBENTER(BXE_INSANE_STATS);

	rc = 0;
	stats = BXE_SP(sc, fw_stats);
	tport = &stats->tstorm_common.port_statistics;

	fstats = BXE_SP(sc, func_stats);
	memcpy(&(fstats->total_bytes_received_hi),
	    &(BXE_SP(sc, func_stats_base)->total_bytes_received_hi),
	    sizeof(struct host_func_stats) - 2*sizeof(uint32_t));

	diff = 0;
	estats = &sc->eth_stats;
	estats->no_buff_discard_hi = 0;
	estats->no_buff_discard_lo = 0;
	estats->error_bytes_received_hi = 0;
	estats->error_bytes_received_lo = 0;
/*	estats->etherstatsoverrsizepkts_hi = 0;
	estats->etherstatsoverrsizepkts_lo = 0;
*/
	for (i = 0; i < sc->num_queues; i++) {
		fp = &sc->fp[i];
		cl_id = fp->cl_id;
		tclient = &stats->tstorm_common.client_statistics[cl_id];
		uclient = &stats->ustorm_common.client_statistics[cl_id];
		xclient = &stats->xstorm_common.client_statistics[cl_id];
		old_tclient = &fp->old_tclient;
		old_uclient = &fp->old_uclient;
		old_xclient = &fp->old_xclient;
		qstats = &fp->eth_q_stats;

		/* Are STORM statistics valid? */
		if ((uint16_t)(le16toh(tclient->stats_counter) + 1) !=
		    sc->stats_counter) {
#if 0
			DBPRINT(sc, BXE_WARN, "%s(): Stats not updated by TSTORM "
			    "(tstorm counter (%d) != stats_counter (%d))!\n",
			    __FUNCTION__, tclient->stats_counter, sc->stats_counter);
#endif
			rc = 1;
			goto bxe_stats_storm_update_exit;
		}

		if ((uint16_t)(le16toh(uclient->stats_counter) + 1) !=
		    sc->stats_counter) {
#if 0
			DBPRINT(sc, BXE_WARN, "%s(): Stats not updated by USTORM "
			    "(ustorm counter (%d) != stats_counter (%d))!\n",
			    __FUNCTION__, uclient->stats_counter, sc->stats_counter);
#endif
			rc = 2;
			goto bxe_stats_storm_update_exit;
		}

		if ((uint16_t)(le16toh(xclient->stats_counter) + 1) !=
			sc->stats_counter) {
#if 0
			DBPRINT(sc, BXE_WARN, "%s(): Stats not updated by XSTORM "
			    "(xstorm counter (%d) != stats_counter (%d))!\n",
			    __FUNCTION__, xclient->stats_counter, sc->stats_counter);
#endif
			rc = 3;
			goto bxe_stats_storm_update_exit;
		}

		qstats->total_bytes_received_hi =
		    (tclient->rcv_broadcast_bytes.hi);
		qstats->total_bytes_received_lo =
		    le32toh(tclient->rcv_broadcast_bytes.lo);

		ADD_64(qstats->total_bytes_received_hi,
		    le32toh(tclient->rcv_multicast_bytes.hi),
		    qstats->total_bytes_received_lo,
		    le32toh(tclient->rcv_multicast_bytes.lo));

		ADD_64(qstats->total_bytes_received_hi,
		    le32toh(tclient->rcv_unicast_bytes.hi),
		    qstats->total_bytes_received_lo,
		    le32toh(tclient->rcv_unicast_bytes.lo));

		SUB_64(qstats->total_bytes_received_hi,
		    le32toh(uclient->bcast_no_buff_bytes.hi),
		    qstats->total_bytes_received_lo,
		    le32toh(uclient->bcast_no_buff_bytes.lo));

		SUB_64(qstats->total_bytes_received_hi,
		    le32toh(uclient->mcast_no_buff_bytes.hi),
		    qstats->total_bytes_received_lo,
		    le32toh(uclient->mcast_no_buff_bytes.lo));

		SUB_64(qstats->total_bytes_received_hi,
		    le32toh(uclient->ucast_no_buff_bytes.hi),
		    qstats->total_bytes_received_lo,
		    le32toh(uclient->ucast_no_buff_bytes.lo));

		qstats->valid_bytes_received_hi =
		    qstats->total_bytes_received_hi;
		qstats->valid_bytes_received_lo =
		    qstats->total_bytes_received_lo;

		qstats->error_bytes_received_hi =
		    le32toh(tclient->rcv_error_bytes.hi);
		qstats->error_bytes_received_lo =
		    le32toh(tclient->rcv_error_bytes.lo);

		ADD_64(qstats->total_bytes_received_hi,
		    qstats->error_bytes_received_hi,
		    qstats->total_bytes_received_lo,
		    qstats->error_bytes_received_lo);

		UPDATE_EXTEND_TSTAT(rcv_unicast_pkts,
		    total_unicast_packets_received);
		UPDATE_EXTEND_TSTAT(rcv_multicast_pkts,
		    total_multicast_packets_received);
		UPDATE_EXTEND_TSTAT(rcv_broadcast_pkts,
		    total_broadcast_packets_received);
/*		UPDATE_EXTEND_TSTAT(packets_too_big_discard,
        	    etherstatsoverrsizepkts);
*/
		UPDATE_EXTEND_TSTAT(no_buff_discard, no_buff_discard);

		SUB_EXTEND_USTAT(ucast_no_buff_pkts,
		    total_unicast_packets_received);
		SUB_EXTEND_USTAT(mcast_no_buff_pkts,
		    total_multicast_packets_received);
		SUB_EXTEND_USTAT(bcast_no_buff_pkts,
		    total_broadcast_packets_received);
		UPDATE_EXTEND_USTAT(ucast_no_buff_pkts, no_buff_discard);
		UPDATE_EXTEND_USTAT(mcast_no_buff_pkts, no_buff_discard);
		UPDATE_EXTEND_USTAT(bcast_no_buff_pkts, no_buff_discard);

		qstats->total_bytes_transmitted_hi =
		    (xclient->unicast_bytes_sent.hi);
		qstats->total_bytes_transmitted_lo =
		    (xclient->unicast_bytes_sent.lo);

		ADD_64(qstats->total_bytes_transmitted_hi,
		    (xclient->multicast_bytes_sent.hi),
		    qstats->total_bytes_transmitted_lo,
		    (xclient->multicast_bytes_sent.lo));

		ADD_64(qstats->total_bytes_transmitted_hi,
		    (xclient->broadcast_bytes_sent.hi),
		    qstats->total_bytes_transmitted_lo,
		    (xclient->broadcast_bytes_sent.lo));

		UPDATE_EXTEND_XSTAT(unicast_pkts_sent,
		    total_unicast_packets_transmitted);

		UPDATE_EXTEND_XSTAT(multicast_pkts_sent,
		    total_multicast_packets_transmitted);

		UPDATE_EXTEND_XSTAT(broadcast_pkts_sent,
		    total_broadcast_packets_transmitted);

		old_tclient->checksum_discard = tclient->checksum_discard;
		old_tclient->ttl0_discard = tclient->ttl0_discard;

		ADD_64(fstats->total_bytes_received_hi,
		    qstats->total_bytes_received_hi,
		    fstats->total_bytes_received_lo,
		    qstats->total_bytes_received_lo);
		ADD_64(fstats->total_bytes_transmitted_hi,
		    qstats->total_bytes_transmitted_hi,
		    fstats->total_bytes_transmitted_lo,
		    qstats->total_bytes_transmitted_lo);
		ADD_64(fstats->total_unicast_packets_received_hi,
		    qstats->total_unicast_packets_received_hi,
		    fstats->total_unicast_packets_received_lo,
		    qstats->total_unicast_packets_received_lo);
		ADD_64(fstats->total_multicast_packets_received_hi,
		    qstats->total_multicast_packets_received_hi,
		    fstats->total_multicast_packets_received_lo,
		    qstats->total_multicast_packets_received_lo);
		ADD_64(fstats->total_broadcast_packets_received_hi,
		    qstats->total_broadcast_packets_received_hi,
		    fstats->total_broadcast_packets_received_lo,
		    qstats->total_broadcast_packets_received_lo);
		ADD_64(fstats->total_unicast_packets_transmitted_hi,
		    qstats->total_unicast_packets_transmitted_hi,
		    fstats->total_unicast_packets_transmitted_lo,
		    qstats->total_unicast_packets_transmitted_lo);
		ADD_64(fstats->total_multicast_packets_transmitted_hi,
		    qstats->total_multicast_packets_transmitted_hi,
		    fstats->total_multicast_packets_transmitted_lo,
		    qstats->total_multicast_packets_transmitted_lo);
		ADD_64(fstats->total_broadcast_packets_transmitted_hi,
		    qstats->total_broadcast_packets_transmitted_hi,
		    fstats->total_broadcast_packets_transmitted_lo,
		    qstats->total_broadcast_packets_transmitted_lo);
		ADD_64(fstats->valid_bytes_received_hi,
		    qstats->valid_bytes_received_hi,
		    fstats->valid_bytes_received_lo,
		    qstats->valid_bytes_received_lo);

		ADD_64(estats->error_bytes_received_hi,
		    qstats->error_bytes_received_hi,
		    estats->error_bytes_received_lo,
		    qstats->error_bytes_received_lo);

		ADD_64(estats->no_buff_discard_hi, qstats->no_buff_discard_hi,
		    estats->no_buff_discard_lo, qstats->no_buff_discard_lo);
	}

	ADD_64(fstats->total_bytes_received_hi,
	    estats->rx_stat_ifhcinbadoctets_hi,
	    fstats->total_bytes_received_lo,
	    estats->rx_stat_ifhcinbadoctets_lo);

	memcpy(estats, &(fstats->total_bytes_received_hi),
	    sizeof(struct host_func_stats) - 2*sizeof(uint32_t));

	ADD_64(estats->error_bytes_received_hi,
	    estats->rx_stat_ifhcinbadoctets_hi,
	    estats->error_bytes_received_lo,
	    estats->rx_stat_ifhcinbadoctets_lo);

	if (sc->port.pmf) {
		estats->mac_filter_discard =
		    le32toh(tport->mac_filter_discard);
		estats->xxoverflow_discard =
		    le32toh(tport->xxoverflow_discard);
		estats->brb_truncate_discard =
		    le32toh(tport->brb_truncate_discard);
		estats->mac_discard = le32toh(tport->mac_discard);
	}

	fstats->host_func_stats_start = ++fstats->host_func_stats_end;

	sc->stats_pending = 0;

bxe_stats_storm_update_exit:

	DBEXIT(BXE_INSANE_STATS);
	return(rc);
}

/*
 * Copy the controller maintained statistics over to the OS.
 *
 * Returns:
 *   None.
 */
static void
bxe_stats_net_update(struct bxe_softc *sc)
{
	struct tstorm_per_client_stats *old_tclient;
	struct bxe_eth_stats *estats;
	struct ifnet *ifp;

	DBENTER(BXE_INSANE_STATS);

	old_tclient = &sc->fp[0].old_tclient;
	estats = &sc->eth_stats;
	ifp = sc->bxe_ifp;

	/*
	 * Update the OS interface statistics from
	 * the hardware statistics.
	 */

	ifp->if_collisions =
	    (u_long) estats->tx_stat_dot3statssinglecollisionframes_lo +
	    (u_long) estats->tx_stat_dot3statsmultiplecollisionframes_lo +
	    (u_long) estats->tx_stat_dot3statslatecollisions_lo +
	    (u_long) estats->tx_stat_dot3statsexcessivecollisions_lo;

	ifp->if_ierrors =
	    (u_long) old_tclient->checksum_discard +
	    (u_long) estats->no_buff_discard_lo +
	    (u_long) estats->mac_discard +
	    (u_long) estats->rx_stat_etherstatsundersizepkts_lo +
	    (u_long) estats->jabber_packets_received +
	    (u_long) estats->brb_drop_lo +
	    (u_long) estats->brb_truncate_discard +
	    (u_long) estats->rx_stat_dot3statsfcserrors_lo +
	    (u_long) estats->rx_stat_dot3statsalignmenterrors_lo +
	    (u_long) estats->xxoverflow_discard;

	ifp->if_oerrors =
	    (u_long) estats->tx_stat_dot3statslatecollisions_lo +
	    (u_long) estats->tx_stat_dot3statsexcessivecollisions_lo +
	    (u_long) estats->tx_stat_dot3statsinternalmactransmiterrors_lo;

	ifp->if_ipackets =
	    bxe_hilo(&estats->total_unicast_packets_received_hi) +
	    bxe_hilo(&estats->total_multicast_packets_received_hi) +
	    bxe_hilo(&estats->total_broadcast_packets_received_hi);

	ifp->if_opackets =
	    bxe_hilo(&estats->total_unicast_packets_transmitted_hi) +
	    bxe_hilo(&estats->total_multicast_packets_transmitted_hi) +
	    bxe_hilo(&estats->total_broadcast_packets_transmitted_hi);

	DBEXIT(BXE_INSANE_STATS);
}

/*
 *
 * Returns:
 *   None.
 */
static void
bxe_stats_update(struct bxe_softc *sc)
{
	uint32_t *stats_comp;
	int update;

	DBENTER(BXE_INSANE_STATS);

	stats_comp = BXE_SP(sc, stats_comp);
	update = 0;

	/* Make sure the statistics DMAE update has completed. */
	if (*stats_comp != DMAE_COMP_VAL)
		goto bxe_stats_update_exit;

	/* Check for any hardware statistics updates. */
	if (sc->port.pmf)
		update = (bxe_stats_hw_update(sc) == 0);

	/* Check for any STORM statistics updates. */
	update |= (bxe_stats_storm_update(sc) == 0);

	/* If we got updated hardware statistics then update the OS. */
	if (update)
		bxe_stats_net_update(sc);
	else {
		/* Check if any statistics updates are pending. */
		if (sc->stats_pending) {
			/* The update hasn't completed, keep waiting. */
			sc->stats_pending++;

			/* Have we been waiting for too long? */
			if (sc->stats_pending >= 3) {
				BXE_PRINTF(
				    "%s(%d): Failed to get statistics after "
				    "3 tries!\n", __FILE__, __LINE__);
				bxe_panic_dump(sc);
				goto bxe_stats_update_exit;
			}
		}
	}

	/* Kickoff the next statistics request. */
	bxe_stats_hw_post(sc);
	bxe_stats_storm_post(sc);

bxe_stats_update_exit:
	DBEXIT(BXE_INSANE_STATS);
}

/*
 *
 * Returns:
 *   None.
 */
static void
bxe_stats_port_stop(struct bxe_softc *sc)
{
	struct dmae_command *dmae;
	uint32_t opcode, *stats_comp;
	int loader_idx;

	DBENTER(BXE_VERBOSE_STATS);

	stats_comp = BXE_SP(sc, stats_comp);
	loader_idx = PMF_DMAE_C(sc);
	sc->executer_idx = 0;

	opcode = (DMAE_CMD_SRC_PCI | DMAE_CMD_DST_GRC |
	    DMAE_CMD_C_ENABLE |
	    DMAE_CMD_SRC_RESET | DMAE_CMD_DST_RESET |
#ifdef __BIG_ENDIAN
	    DMAE_CMD_ENDIANITY_B_DW_SWAP |
#else
	    DMAE_CMD_ENDIANITY_DW_SWAP |
#endif
	    (BP_PORT(sc) ? DMAE_CMD_PORT_1 : DMAE_CMD_PORT_0) |
	    (BP_E1HVN(sc) << DMAE_CMD_E1HVN_SHIFT));

	if (sc->port.port_stx) {
		dmae = BXE_SP(sc, dmae[sc->executer_idx++]);

		if (sc->func_stx)
			dmae->opcode = (opcode | DMAE_CMD_C_DST_GRC);
		else
			dmae->opcode = (opcode | DMAE_CMD_C_DST_PCI);

		dmae->src_addr_lo = U64_LO(BXE_SP_MAPPING(sc, port_stats));
		dmae->src_addr_hi = U64_HI(BXE_SP_MAPPING(sc, port_stats));
		dmae->dst_addr_lo = sc->port.port_stx >> 2;
		dmae->dst_addr_hi = 0;
		dmae->len = sizeof(struct host_port_stats) >> 2;

		if (sc->func_stx) {
			dmae->comp_addr_lo = dmae_reg_go_c[loader_idx] >> 2;
			dmae->comp_addr_hi = 0;
			dmae->comp_val = 1;
		} else {
			dmae->comp_addr_lo = U64_LO(BXE_SP_MAPPING(sc,
			    stats_comp));
			dmae->comp_addr_hi = U64_HI(BXE_SP_MAPPING(sc,
			    stats_comp));
			dmae->comp_val = DMAE_COMP_VAL;

			*stats_comp = 0;
		}
	}

	if (sc->func_stx) {
		dmae = BXE_SP(sc, dmae[sc->executer_idx++]);
		dmae->opcode = (opcode | DMAE_CMD_C_DST_PCI);
		dmae->src_addr_lo = U64_LO(BXE_SP_MAPPING(sc, func_stats));
		dmae->src_addr_hi = U64_HI(BXE_SP_MAPPING(sc, func_stats));
		dmae->dst_addr_lo = sc->func_stx >> 2;
		dmae->dst_addr_hi = 0;
		dmae->len = sizeof(struct host_func_stats) >> 2;
		dmae->comp_addr_lo = U64_LO(BXE_SP_MAPPING(sc, stats_comp));
		dmae->comp_addr_hi = U64_HI(BXE_SP_MAPPING(sc, stats_comp));
		dmae->comp_val = DMAE_COMP_VAL;

		*stats_comp = 0;
	}

	DBEXIT(BXE_VERBOSE_STATS);
}

/*
 * Returns:
 *   None.
 */
static void
bxe_stats_stop(struct bxe_softc *sc)
{
	int update;

	DBENTER(BXE_VERBOSE_STATS);

	update = 0;
	/* Wait for any pending completions. */
	bxe_stats_comp(sc);

	if (sc->port.pmf)
		update = (bxe_stats_hw_update(sc) == 0);

	update |= (bxe_stats_storm_update(sc) == 0);

	if (update) {
		bxe_stats_net_update(sc);

		if (sc->port.pmf)
			bxe_stats_port_stop(sc);

		bxe_stats_hw_post(sc);
		bxe_stats_comp(sc);
	}

	DBEXIT(BXE_VERBOSE_STATS);
}

/*
 * A dummy function to fill in the statistics state transition table.
 *
 * Returns:
 *   None.
 */
static void
bxe_stats_do_nothing(struct bxe_softc *sc)
{

}

static const struct {
	void (*action)(struct bxe_softc *sc);
	enum bxe_stats_state next_state;
} bxe_stats_stm[STATS_STATE_MAX][STATS_EVENT_MAX] = {
    /* State   Event  */
    {
    /* DISABLED	PMF	*/ {bxe_stats_pmf_update,	STATS_STATE_DISABLED},
    /*		LINK_UP	*/ {bxe_stats_start,		STATS_STATE_ENABLED},
    /*		UPDATE	*/ {bxe_stats_do_nothing,	STATS_STATE_DISABLED},
    /*		STOP	*/ {bxe_stats_do_nothing,	STATS_STATE_DISABLED}
    },

    {
    /* ENABLED	PMF	*/ {bxe_stats_pmf_start,	STATS_STATE_ENABLED},
    /*		LINK_UP	*/ {bxe_stats_restart,		STATS_STATE_ENABLED},
    /*		UPDATE	*/ {bxe_stats_update,		STATS_STATE_ENABLED},
    /*		STOP	*/ {bxe_stats_stop,		STATS_STATE_DISABLED}
    }
};

/*
 * Move to the next state of the statistics state machine.
 *
 * Returns:
 *   None.
 */
static void
bxe_stats_handle(struct bxe_softc *sc, enum bxe_stats_event event)
{
	enum bxe_stats_state state;

	DBENTER(BXE_INSANE_STATS);

	state = sc->stats_state;
#ifdef BXE_DEBUG
	if (event != STATS_EVENT_UPDATE)
		DBPRINT(sc, BXE_VERBOSE_STATS,
		    "%s(): Current state = %d, event = %d.\n", __FUNCTION__,
		    state, event);
#endif

	bxe_stats_stm[state][event].action(sc);
	sc->stats_state = bxe_stats_stm[state][event].next_state;

#ifdef BXE_DEBUG
	if (event != STATS_EVENT_UPDATE)
		DBPRINT(sc, BXE_VERBOSE_STATS, "%s(): New state = %d.\n",
		    __FUNCTION__, sc->stats_state);
#endif

	DBEXIT(BXE_INSANE_STATS);
}

/*
 * bxe_chktso_window()
 * Checks to ensure the 13 bd sliding window is >= MSS for TSO.
 * Check that (13 total bds - 3bds) = 10 bd window >= MSS.
 * The window: 3 bds are = 1 (for headers BD) + 2 (for PBD and last BD)
 * The headers comes in a seperate bd in FreeBSD. So 13-3=10.
 *
 * Returns:
 *   0 if OK to send, 1 if packet needs further defragmentation.
 */
static int
bxe_chktso_window(struct bxe_softc* sc, int nsegs, bus_dma_segment_t *segs,
    struct mbuf *m0)
{
	uint32_t num_wnds, wnd_size, wnd_sum;
	int32_t frag_idx, wnd_idx;
	unsigned short lso_mss;
	int defrag;

	defrag = 0;
	wnd_sum = 0;
	wnd_size = 10;
	num_wnds = nsegs - wnd_size;
	lso_mss = htole16(m0->m_pkthdr.tso_segsz);

	/*
	 * Total Header lengths Eth+IP+TCP in 1st FreeBSD mbuf so
	 * calculate the first window sum of data skip the first
	 * assuming it is the header in FreeBSD.
	 */
	for (frag_idx = 1; (frag_idx <= wnd_size); frag_idx++)
		wnd_sum += htole16(segs[frag_idx].ds_len);

	/* Chk the first 10 bd window size */
	if (wnd_sum < lso_mss)
		return (defrag = 1);

	/* Run through the windows */
	for (wnd_idx = 0; wnd_idx < num_wnds; wnd_idx++, frag_idx++) {
		/* Subtract the 1st mbuf->m_len of the last wndw(-header). */
		wnd_sum	-= htole16(segs[wnd_idx+1].ds_len);
		/* Add the next mbuf len to the len of our new window. */
		wnd_sum +=  htole16(segs[frag_idx].ds_len);
		if (wnd_sum < lso_mss) {
			defrag = 1;
			break;
		}
	}

	return (defrag);
}


/*
 * Encapsultes an mbuf cluster into the tx_bd chain structure and
 * makes the memory visible to the controller.
 *
 * If an mbuf is submitted to this routine and cannot be given to the
 * controller (e.g. it has too many fragments) then the function may free
 * the mbuf and return to the caller.
 *
 * Returns:
 *   0 = Success, !0 = Failure
 *   Note the side effect that an mbuf may be freed if it causes a problem.
 */
static int
bxe_tx_encap(struct bxe_fastpath *fp, struct mbuf **m_head)
{
	bus_dma_segment_t segs[32];
	bus_dmamap_t map;
	struct mbuf *m0;
	struct eth_tx_parse_bd *tx_parse_bd;
	struct eth_tx_bd *tx_data_bd;
	struct eth_tx_bd *tx_total_pkt_size_bd;
	struct eth_tx_start_bd *tx_start_bd;
	uint16_t etype, bd_prod, pkt_prod, total_pkt_size;
	uint8_t mac_type;
	int i, e_hlen, error, nsegs, rc, nbds, vlan_off, ovlan;
	struct bxe_softc *sc;

	sc = fp->sc;
	DBENTER(BXE_VERBOSE_SEND);

	rc = nbds = ovlan = vlan_off = total_pkt_size = 0;

	m0 = *m_head;

	tx_total_pkt_size_bd = NULL;
	tx_start_bd = NULL;
	tx_data_bd = NULL;
	tx_parse_bd = NULL;

	pkt_prod = fp->tx_pkt_prod;
	bd_prod = TX_BD(fp->tx_bd_prod);

	mac_type = UNICAST_ADDRESS;

#ifdef BXE_DEBUG
	int debug_prod;
	DBRUN(debug_prod = bd_prod);
#endif

	/* Map the mbuf into the next open DMAable memory. */
	map = fp->tx_mbuf_map[TX_BD(pkt_prod)];
	error = bus_dmamap_load_mbuf_sg(fp->tx_mbuf_tag, map, m0,
	    segs, &nsegs, BUS_DMA_NOWAIT);

	do{
		/* Handle any mapping errors. */
		if(__predict_false(error)){
			fp->tx_dma_mapping_failure++;
			if (error == ENOMEM) {
				/* Resource issue, try again later. */
				rc = ENOMEM;
			}else if (error == EFBIG) {
				/* Possibly recoverable. */
				fp->mbuf_defrag_attempts++;
				m0 = m_defrag(*m_head, M_DONTWAIT);
				if (m0 == NULL) {
					fp->mbuf_defrag_failures++;
					rc = ENOBUFS;
				} else {
				/* Defrag was successful, try mapping again.*/
					fp->mbuf_defrag_successes++;
					*m_head = m0;
					error =
					    bus_dmamap_load_mbuf_sg(
						fp->tx_mbuf_tag, map, m0,
						segs, &nsegs, BUS_DMA_NOWAIT);
					if (error) {
						fp->tx_dma_mapping_failure++;
						rc = error;
					}
				}
			}else {
				/* Unrecoverable. */
				DBPRINT(sc, BXE_WARN_SEND,
				    "%s(): Unknown TX mapping error! "
				    "rc = %d.\n", __FUNCTION__, error);
				DBRUN(bxe_dump_mbuf(sc, m0));
				rc = error;
			}

			break;
		}

		/* Make sure this enough room in the send queue. */
		if (__predict_false((nsegs + 2) >
		    (USABLE_TX_BD - fp->used_tx_bd))) {
			fp->tx_queue_too_full++;
			bus_dmamap_unload(fp->tx_mbuf_tag, map);
			rc = ENOBUFS;
			break;
		}

		/* Now make sure it fits in the pkt window */
		if (__predict_false(nsegs > 12)) {

			/*
			 * The mbuf may be to big for the controller
			 * to handle.  If the frame is a TSO frame
			 * we'll need to do an additional check.
			 */
			if(m0->m_pkthdr.csum_flags & CSUM_TSO){
				if (bxe_chktso_window(sc,nsegs,segs,m0) == 0)
					/* OK to send. */
					break;
				else
					fp->window_violation_tso++;
			} else
				fp->window_violation_std++;

			/*
			 * If this is a standard frame then defrag is
			 * required.  Unmap the mbuf, defrag it, then
			 * try mapping it again.
			 */
			fp->mbuf_defrag_attempts++;
			bus_dmamap_unload(fp->tx_mbuf_tag, map);
			m0 = m_defrag(*m_head, M_DONTWAIT);
			if (m0 == NULL) {
				fp->mbuf_defrag_failures++;
				rc = ENOBUFS;
				break;
			}

			/* Defrag was successful, try mapping again. */
			fp->mbuf_defrag_successes++;
			*m_head = m0;
			error =
			    bus_dmamap_load_mbuf_sg(
				fp->tx_mbuf_tag, map, m0,
				segs, &nsegs, BUS_DMA_NOWAIT);

			/* Handle any mapping errors. */
			if (__predict_false(error)) {
				fp->tx_dma_mapping_failure++;
				rc = error;
				break;
			}

			/* Last try */
			if (m0->m_pkthdr.csum_flags & CSUM_TSO){
				if (bxe_chktso_window(sc,nsegs,segs,m0) == 1)
					rc = ENOBUFS;
			} else if (nsegs > 12 ){
				rc = ENOBUFS;
			} else
				rc = 0;
		}
	}while (0);

	/* Check for errors */
	if (rc){
		if(rc == ENOMEM){
			/* Recoverable try again later  */
		}else{
			fp->soft_tx_errors++;
			DBRUN(fp->tx_mbuf_alloc--);
			m_freem(*m_head);
			*m_head = NULL;
		}
		return (rc);
	}

	/* We're committed to sending the frame, update the counter. */
	fp->tx_pkt_prod++;

	/* set flag according to packet type (UNICAST_ADDRESS is default)*/
	if (m0->m_flags & M_BCAST)
		mac_type = BROADCAST_ADDRESS;
	else if (m0->m_flags & M_MCAST)
		mac_type = MULTICAST_ADDRESS;

	/* Prepare the first transmit BD for the mbuf(Get a link from the chain). */
	tx_start_bd = &fp->tx_bd_chain[TX_PAGE(bd_prod)][TX_IDX(bd_prod)].start_bd;

	tx_start_bd->addr_lo = htole32(U64_LO(segs[0].ds_addr));
	tx_start_bd->addr_hi = htole32(U64_HI(segs[0].ds_addr));
	tx_start_bd->nbytes  = htole16(segs[0].ds_len);
	total_pkt_size += tx_start_bd->nbytes;
	tx_start_bd->bd_flags.as_bitfield = ETH_TX_BD_FLAGS_START_BD;
	tx_start_bd->general_data =
		(mac_type << ETH_TX_START_BD_ETH_ADDR_TYPE_SHIFT);

	tx_start_bd->general_data |= (1 << ETH_TX_START_BD_HDR_NBDS_SHIFT);

	nbds = nsegs + 1;  /* Add 1 for parsing bd. Assuming nseg > 0  */
	tx_start_bd->nbd = htole16(nbds);

	if (m0->m_flags & M_VLANTAG) {
//		vlan_off += ETHER_VLAN_ENCAP_LEN;
		tx_start_bd->bd_flags.as_bitfield |= ETH_TX_BD_FLAGS_VLAN_TAG;
		tx_start_bd->vlan = htole16(m0->m_pkthdr.ether_vtag);
		DBPRINT(sc, BXE_VERBOSE_SEND, "%s(): Inserting VLAN tag %d\n",
			__FUNCTION__, m0->m_pkthdr.ether_vtag);
	}
	else
		/*
		 * In cases where the VLAN tag is not used the firmware
		 * expects to see a packet counter in the VLAN tag field
		 * Failure to do so will cause an assertion which will
		 * stop the controller.
		 */
		tx_start_bd->vlan = htole16(pkt_prod);

	/*
	 * Add a parsing BD from the chain. The parsing bd is always added,
	 * however, it is only used for tso & chksum.
	 */
	bd_prod = TX_BD(NEXT_TX_BD(bd_prod));
	tx_parse_bd = (struct eth_tx_parse_bd *)
		   &fp->tx_bd_chain[TX_PAGE(bd_prod)][TX_IDX(bd_prod)].parse_bd;
	memset(tx_parse_bd, 0, sizeof(struct eth_tx_parse_bd));

	/* Gather all info about the packet and add to tx_parse_bd */
	if (m0->m_pkthdr.csum_flags) {
		struct ether_vlan_header *eh;
		struct ip *ip = NULL;
		struct tcphdr *th = NULL;
		uint16_t flags = 0;
		struct udphdr *uh = NULL;

		/* Map the Ethernet header to find the type & header length. */
		eh = mtod(m0, struct ether_vlan_header *);

		/* Handle VLAN encapsulation if present. */
		if (eh->evl_encap_proto == htons(ETHERTYPE_VLAN)) {
			etype = ntohs(eh->evl_proto);
			e_hlen = ETHER_HDR_LEN + vlan_off;
		} else {
			etype = ntohs(eh->evl_encap_proto);
			e_hlen = ETHER_HDR_LEN;
		}

		/* Set the Ethernet header length in 16 bit words. */
		tx_parse_bd->global_data = (e_hlen + ovlan) >> 1;
		tx_parse_bd->global_data |= ((m0->m_flags & M_VLANTAG) <<
		    ETH_TX_PARSE_BD_LLC_SNAP_EN_SHIFT);

		switch (etype) {
		case ETHERTYPE_IP:{
			/* if mbuf's len < 20bytes, the ip_hdr is in next mbuf*/
			if (m0->m_len < sizeof(struct ip))
				ip = (struct ip *)m0->m_next->m_data;
			else
				ip = (struct ip *)(m0->m_data + e_hlen);

			/* Calculate IP header length (16 bit words). */
			tx_parse_bd->ip_hlen = (ip->ip_hl << 1);

			/* Calculate enet + IP header length (16 bit words). */
			tx_parse_bd->total_hlen = tx_parse_bd->ip_hlen + (e_hlen >> 1);

			if (m0->m_pkthdr.csum_flags & CSUM_IP) {
				DBPRINT(sc, BXE_EXTREME_SEND, "%s(): IP checksum "
					"enabled.\n", __FUNCTION__);
				fp->offload_frames_csum_ip++;
				flags |= ETH_TX_BD_FLAGS_IP_CSUM;
			}

			/* Handle any checksums requested by the stack. */
			if ((m0->m_pkthdr.csum_flags & CSUM_TCP)||
			    (m0->m_pkthdr.csum_flags & CSUM_TSO)){

				/* Perform TCP checksum offload. */
				DBPRINT(sc, BXE_EXTREME_SEND, "%s(): TCP checksum "
					"enabled.\n", __FUNCTION__);

				/* Get the TCP header. */
				th = (struct tcphdr *)((caddr_t)ip + (ip->ip_hl << 2));

				/* Add the TCP checksum offload flag. */
				flags |= ETH_TX_BD_FLAGS_L4_CSUM;
				fp->offload_frames_csum_tcp++;

				/* Update the enet + IP + TCP header length. */
				tx_parse_bd->total_hlen += (uint16_t)(th->th_off << 1);

				/* Get the pseudo header checksum. */
				tx_parse_bd->tcp_pseudo_csum = ntohs(th->th_sum);
			} else if (m0->m_pkthdr.csum_flags & CSUM_UDP) {
				/*
				 * The hardware doesn't actually support UDP checksum
				 * offload but we can fake it by doing TCP checksum
				 * offload and factoring out the extra bytes that are
				 * different between the TCP header and the UDP header.
				 * calculation will begin 10 bytes before the actual
				 * start of the UDP header.  To work around this we
				 * need to calculate the checksum of the 10 bytes
				 * before the UDP header and factor that out of the
				 * UDP pseudo header checksum before asking the H/W
				 * to calculate the full UDP checksum.
				 */
				uint16_t tmp_csum;
				uint32_t *tmp_uh;

				/* This value is 10. */
				uint8_t fix = (uint8_t) (offsetof(struct tcphdr, th_sum) -
					(int) offsetof(struct udphdr, uh_sum));

				/* Perform UDP checksum offload. */
				DBPRINT(sc, BXE_EXTREME_SEND, "%s(): UDP checksum "
					"enabled.\n", __FUNCTION__);

				/* Add the TCP checksum offload flag for UDP frames too. */
				flags |= ETH_TX_BD_FLAGS_L4_CSUM;
				fp->offload_frames_csum_udp++;
				tx_parse_bd->global_data |= ETH_TX_PARSE_BD_UDP_CS_FLG;

				/* Get a pointer to the UDP header. */
				uh = (struct udphdr *)((caddr_t)ip + (ip->ip_hl << 2));

				/* Set a pointer 10 bytes before the actual UDP header. */
 					tmp_uh = (uint32_t *)((uint8_t *)uh - fix);

				/*
				 * Calculate a pseudo header checksum over the 10 bytes
				 * before the UDP header.
				 */
				tmp_csum = in_pseudo(ntohl(*tmp_uh),
					ntohl(*(tmp_uh + 1)),
					ntohl((*(tmp_uh + 2)) & 0x0000FFFF));

				/* Update the enet + IP + UDP header length. */
				tx_parse_bd->total_hlen += (sizeof(struct udphdr) >> 1);
				tx_parse_bd->tcp_pseudo_csum = ~in_addword(uh->uh_sum, ~tmp_csum);
			}

			/* Update the flags settings for VLAN/Offload. */
			tx_start_bd->bd_flags.as_bitfield |= flags;

			break;
		}
		case ETHERTYPE_IPV6:
			fp->unsupported_tso_request_ipv6++;
			/* DRC - How to handle this error? */
			break;

		default:
			fp->unsupported_tso_request_not_tcp++;
			/* DRC - How to handle this error? */
		}

		/* Setup the Parsing BD with TSO specific info */
		if (m0->m_pkthdr.csum_flags & CSUM_TSO) {

			uint16_t hdr_len = tx_parse_bd->total_hlen << 1;

			DBPRINT(sc, BXE_EXTREME_SEND, "%s(): TSO is enabled.\n",
			    __FUNCTION__);

			tx_start_bd->bd_flags.as_bitfield |= ETH_TX_BD_FLAGS_SW_LSO;

			fp->offload_frames_tso++;
 			if (__predict_false(tx_start_bd->nbytes > hdr_len)) {
				/*
				 * Split the first BD into 2 BDs to make the
				 * FW job easy...
				 */
				tx_start_bd->nbd++;
				DBPRINT(sc, BXE_EXTREME_SEND,
			"%s(): TSO split headr size is %d (%x:%x) nbds %d\n",
			__FUNCTION__, tx_start_bd->nbytes, tx_start_bd->addr_hi,
				    tx_start_bd->addr_lo, nbds);

				bd_prod = TX_BD(NEXT_TX_BD(bd_prod));

				/* Get a new transmit BD (after the tx_parse_bd) and fill it. */
				tx_data_bd = &fp->tx_bd_chain[TX_PAGE(bd_prod)][TX_IDX(bd_prod)].reg_bd;
				tx_data_bd->addr_hi = htole32(U64_HI(segs[0].ds_addr + hdr_len));
				tx_data_bd->addr_lo = htole32(U64_LO(segs[0].ds_addr + hdr_len));
				tx_data_bd->nbytes = htole16(segs[0].ds_len) - hdr_len;
				if (tx_total_pkt_size_bd == NULL)
					tx_total_pkt_size_bd = tx_data_bd;

				/*
				 * This indicates that the transmit BD
				 * has no individual mapping and the
				 * FW ignores this flag in a BD that is
				 * not marked with the start flag.
				 */

				DBPRINT(sc, BXE_EXTREME_SEND,
				"%s(): TSO split data size is %d (%x:%x)\n",
				    __FUNCTION__, tx_data_bd->nbytes,
				    tx_data_bd->addr_hi, tx_data_bd->addr_lo);
			}

			/*
			 * For TSO the controller needs the following info:
			 * MSS, tcp_send_seq, ip_id, and tcp_pseudo_csum.
			 */
			tx_parse_bd->lso_mss = htole16(m0->m_pkthdr.tso_segsz);
			tx_parse_bd->tcp_send_seq = ntohl(th->th_seq);
			tx_parse_bd->tcp_flags = th->th_flags;
			tx_parse_bd->ip_id = ntohs(ip->ip_id);

			tx_parse_bd->tcp_pseudo_csum =
			    ntohs(in_pseudo(ip->ip_src.s_addr,
			    ip->ip_dst.s_addr, htons(IPPROTO_TCP)));

			tx_parse_bd->global_data |=
			    ETH_TX_PARSE_BD_PSEUDO_CS_WITHOUT_LEN;
		}
	}

	/* Prepare Remaining BDs. Start_tx_bd contains first seg(frag). */
	for (i = 1; i < nsegs ; i++) {
		bd_prod = TX_BD(NEXT_TX_BD(bd_prod));
		tx_data_bd = &fp->tx_bd_chain[TX_PAGE(bd_prod)][TX_IDX(bd_prod)].reg_bd;
		tx_data_bd->addr_lo = htole32(U64_LO(segs[i].ds_addr));
		tx_data_bd->addr_hi = htole32(U64_HI(segs[i].ds_addr));
		tx_data_bd->nbytes = htole16(segs[i].ds_len);
		if (tx_total_pkt_size_bd == NULL)
			tx_total_pkt_size_bd = tx_data_bd;
		total_pkt_size += tx_data_bd->nbytes;
	}

	if(tx_total_pkt_size_bd != NULL)
		tx_total_pkt_size_bd->total_pkt_bytes = total_pkt_size;

	/* Update bd producer index value for next tx */
	bd_prod = TX_BD(NEXT_TX_BD(bd_prod));
	DBRUNMSG(BXE_EXTREME_SEND, bxe_dump_tx_chain(fp, debug_prod, nbds));

	/*
	 * Ensure that the mbuf pointer for this
	 * transmission is placed at the array index
	 * of the last descriptor in this chain.
	 * This is done because a single map is used
	 * for all segments of the mbuf and we don't
	 * want to unload the map before all of the
	 * segments have been freed.
	 */
	fp->tx_mbuf_ptr[TX_BD(pkt_prod)] = m0;

	fp->used_tx_bd += nbds;

	/*
	 * Ring the tx doorbell, counting the next
	 * bd if the packet contains or ends with it.
	 */
	if(TX_IDX(bd_prod) < nbds)
		nbds++;

//BXE_PRINTF("nsegs:%d, tpktsz:0x%x\n",nsegs, total_pkt_size) ;

	/*
	 * Update the buffer descriptor producer count and the packet
	 * producer count in doorbell data memory (eth_tx_db_data) then
	 * ring the doorbell.
	 */
/*	fp->hw_tx_prods->bds_prod =
		htole16(le16toh(fp->hw_tx_prods->bds_prod) + nbds);
*/


	/* Don't allow reordering of writes for nbd and packets. */
	mb();
/*
	fp->hw_tx_prods->packets_prod =
		htole32(le32toh(fp->hw_tx_prods->packets_prod) + 1);
*/
//	DOORBELL(sc, fp->index, 0);

//	BXE_PRINTF("doorbell: nbd %d  bd %u  index %d\n", nbds, bd_prod, fp->index);

	fp->tx_db.data.prod += nbds;

	/* Producer points to the next free tx_bd at this point. */
	fp->tx_bd_prod = bd_prod;

	DOORBELL(sc, fp->index, fp->tx_db.raw);

	fp->tx_pkts++;

	/* Prevent speculative reads from getting ahead of the status block. */
	bus_space_barrier(sc->bxe_btag, sc->bxe_bhandle,
	    0, 0, BUS_SPACE_BARRIER_READ);

	/* Prevent speculative reads from getting ahead of the doorbell. */
	bus_space_barrier(sc->bxe_db_btag, sc->bxe_db_bhandle,
	    0, 0, BUS_SPACE_BARRIER_READ);

	DBEXIT(BXE_VERBOSE_SEND);
	return(rc);
}


/*
 * Legacy (non-RSS) dispatch routine.
 *
 * Returns:
 *   Nothing.
 */
static void
bxe_tx_start(struct ifnet *ifp)
{
	struct bxe_softc *sc;
	struct bxe_fastpath *fp;

	sc = ifp->if_softc;
	DBENTER(BXE_EXTREME_SEND);

	/* Exit if the transmit queue is full or link down. */
	if (((ifp->if_drv_flags & (IFF_DRV_RUNNING | IFF_DRV_OACTIVE)) !=
	    IFF_DRV_RUNNING) || !sc->link_vars.link_up) {
		DBPRINT(sc, BXE_VERBOSE_SEND,
		    "%s(): No link or TX queue full, ignoring "
		    "transmit request.\n", __FUNCTION__);
		goto bxe_tx_start_exit;
	}

	/* Set the TX queue for the frame. */
	fp = &sc->fp[0];

	BXE_FP_LOCK(fp);
	bxe_tx_start_locked(ifp, fp);
	BXE_FP_UNLOCK(fp);

bxe_tx_start_exit:
	DBEXIT(BXE_EXTREME_SEND);
}


/*
 * Legacy (non-RSS) transmit routine.
 *
 * Returns:
 *   Nothing.
 */
static void
bxe_tx_start_locked(struct ifnet *ifp, struct bxe_fastpath *fp)
{
	struct bxe_softc *sc;
	struct mbuf *m = NULL;
	int tx_count = 0;

	sc = fp->sc;
	DBENTER(BXE_EXTREME_SEND);

	BXE_FP_LOCK_ASSERT(fp);

 	/* Keep adding entries while there are frames to send. */
	while (!IFQ_DRV_IS_EMPTY(&ifp->if_snd)) {

		/* Check for any frames to send. */
		IFQ_DRV_DEQUEUE(&ifp->if_snd, m);
		if (__predict_false(m == NULL))
			break;

		/* The transmit mbuf now belongs to us, keep track of it. */
		DBRUN(fp->tx_mbuf_alloc++);

		/*
		 * Pack the data into the transmit ring. If we
		 * don't have room, place the mbuf back at the
		 * head of the TX queue, set the OACTIVE flag,
		 * and wait for the NIC to drain the chain.
		 */
		if (__predict_false(bxe_tx_encap(fp, &m))) {
			fp->tx_encap_failures++;
			/* Very Bad Frames(tm) may have been dropped. */
			if (m != NULL) {
				/*
				 * Mark the TX queue as full and return
				 * the frame.
				 */
				ifp->if_drv_flags |= IFF_DRV_OACTIVE;
				IFQ_DRV_PREPEND(&ifp->if_snd, m);
				DBRUN(fp->tx_mbuf_alloc--);
				sc->eth_stats.driver_xoff++;
			} else {

			}

			/* Stop looking for more work. */
			break;
		}

		/* The transmit frame was enqueued successfully. */
		tx_count++;

		/* Send a copy of the frame to any BPF listeners. */
		BPF_MTAP(ifp, m);
	}

	/* No TX packets were dequeued. */
	if (tx_count > 0)
		/* Reset the TX watchdog timeout timer. */
		fp->watchdog_timer = BXE_TX_TIMEOUT;
	else
		fp->tx_start_called_on_empty_queue++;

	DBEXIT(BXE_EXTREME_SEND);
}

#if __FreeBSD_version >= 800000
/*
 * Multiqueue (RSS) dispatch routine.
 *
 * Returns:
 *   0 if transmit succeeds, !0 otherwise.
 */
static int
bxe_tx_mq_start(struct ifnet *ifp, struct mbuf *m)
{
	struct	bxe_softc *sc;
	struct	bxe_fastpath *fp;
	int	fp_index, rc;

	sc = ifp->if_softc;
	fp_index = 0;

	DBENTER(BXE_EXTREME_SEND);

	/* Map the flow ID to a queue number. */
	if ((m->m_flags & M_FLOWID) != 0) {
		fp_index = m->m_pkthdr.flowid % sc->num_queues;
		DBPRINT(sc, BXE_EXTREME_SEND,
		    "%s(): Found flowid %d\n",
		    __FUNCTION__, fp_index);
	} else {
		DBPRINT(sc, BXE_EXTREME_SEND,
		    "%s(): No flowid found, using %d\n",
		    __FUNCTION__, fp_index);
	}


	/* Select the fastpath TX queue for the frame. */
	fp = &sc->fp[fp_index];

	/* Exit if the transmit queue is full or link down. */
	if (((ifp->if_drv_flags & (IFF_DRV_RUNNING | IFF_DRV_OACTIVE)) !=
	    IFF_DRV_RUNNING) || !sc->link_vars.link_up) {
		/* We're stuck with the mbuf.  Stash it for now. */
		DBPRINT(sc, BXE_EXTREME_SEND,
		    "%s(): TX queue full/link down, "
		    "parking mbuf...\n", __FUNCTION__);
		rc = drbr_enqueue(ifp, fp->br, m);
		/* DRC - Setup a task to try again. */
		/* taskqueue_enqueue(tq, task); */
		goto bxe_tx_mq_start_exit;
	}

	BXE_FP_LOCK(fp);
	rc = bxe_tx_mq_start_locked(ifp, fp, m);
	BXE_FP_UNLOCK(fp);

bxe_tx_mq_start_exit:
	DBEXIT(BXE_EXTREME_SEND);
	return(rc);
}


/*
 * Multiqueue (RSS) transmit routine.
 *
 * Returns:
 *   0 if transmit succeeds, !0 otherwise.
 */
static int
bxe_tx_mq_start_locked(struct ifnet *ifp,
    struct bxe_fastpath *fp, struct mbuf *m)
{
	struct bxe_softc *sc;
	struct mbuf *next;
	int rc = 0, tx_count = 0;

	sc = fp->sc;

	DBENTER(BXE_EXTREME_SEND);
	DBPRINT(sc, BXE_EXTREME_SEND,
	    "%s(): fp[%02d], drbr queue depth=%d\n",
	    __FUNCTION__, fp->index, drbr_inuse(ifp, fp->br));

	BXE_FP_LOCK_ASSERT(fp);

	if (m == NULL) {
		/* Check for any other work. */
		DBPRINT(sc, BXE_EXTREME_SEND,
		    "%s(): No initial work, dequeue mbuf...\n",
		    __FUNCTION__);
		next = drbr_dequeue(ifp, fp->br);
	} else if (drbr_needs_enqueue(ifp, fp->br)) {
		/* Work pending, queue mbuf to maintain packet order. */
		DBPRINT(sc, BXE_EXTREME_SEND,
		    "%s(): Found queued data pending...\n",
		    __FUNCTION__);
		if ((rc = drbr_enqueue(ifp, fp->br, m)) != 0) {
			DBPRINT(sc, BXE_EXTREME_SEND,
			    "%s(): Enqueue failed...\n",
			    __FUNCTION__);
			goto bxe_tx_mq_start_locked_exit;
		}
		DBPRINT(sc, BXE_EXTREME_SEND,
		    "%s(): Dequeueing old mbuf...\n",
		    __FUNCTION__);
		next = drbr_dequeue(ifp, fp->br);
	} else {
		/* Work with the mbuf we have. */
		DBPRINT(sc, BXE_EXTREME_SEND,
		    "%s(): Start with current mbuf...\n",
		    __FUNCTION__);
		next = m;
	}

 	/* Keep adding entries while there are frames to send. */
	while (next != NULL) {

		/* The transmit mbuf now belongs to us, keep track of it. */
		DBRUN(fp->tx_mbuf_alloc++);

		/*
		 * Pack the data into the transmit ring. If we
		 * don't have room, place the mbuf back at the
		 * head of the TX queue, set the OACTIVE flag,
		 * and wait for the NIC to drain the chain.
		 */
		if (__predict_false(bxe_tx_encap(fp, &next))) {
			DBPRINT(sc, BXE_WARN, "%s(): TX encap failure...\n",
			    __FUNCTION__);
			fp->tx_encap_failures++;
			/* Very Bad Frames(tm) may have been dropped. */
			if (next != NULL) {
				/*
				 * Mark the TX queue as full and save
				 * the frame.
				 */
				ifp->if_drv_flags |= IFF_DRV_OACTIVE;
				DBPRINT(sc, BXE_EXTREME_SEND,
				    "%s(): Save mbuf for another time...\n",
				    __FUNCTION__);
				rc = drbr_enqueue(ifp, fp->br, next);
				DBRUN(fp->tx_mbuf_alloc--);
				sc->eth_stats.driver_xoff++;
			}

			/* Stop looking for more work. */
			break;
		}

		/* The transmit frame was enqueued successfully. */
		tx_count++;

		/* Send a copy of the frame to any BPF listeners. */
		BPF_MTAP(ifp, next);

		DBPRINT(sc, BXE_EXTREME_SEND,
		    "%s(): Check for queued mbufs...\n",
		    __FUNCTION__);
		next = drbr_dequeue(ifp, fp->br);
	}

	DBPRINT(sc, BXE_EXTREME_SEND,
	    "%s(): Enqueued %d mbufs...\n",
	    __FUNCTION__, tx_count);

	/* No TX packets were dequeued. */
	if (tx_count > 0) {
		/* Reset the TX watchdog timeout timer. */
		fp->watchdog_timer = BXE_TX_TIMEOUT;
	} else {
		fp->tx_start_called_on_empty_queue++;
	}

bxe_tx_mq_start_locked_exit:
	DBEXIT(BXE_EXTREME_SEND);
	return(rc);
}


static void
bxe_mq_flush(struct ifnet *ifp)
{
	struct bxe_softc *sc;
	struct bxe_fastpath *fp;
	struct mbuf *m;
	int i;

	sc = ifp->if_softc;

	DBENTER(BXE_VERBOSE_UNLOAD);

	for (i = 0; i < sc->num_queues; i++) {
		fp = &sc->fp[i];

		DBPRINT(sc, BXE_VERBOSE_UNLOAD, "%s(): Clearing fp[%02d]...\n",
		    __FUNCTION__, fp->index);

		if (fp->br != NULL) {
			BXE_FP_LOCK(fp);
			while ((m = buf_ring_dequeue_sc(fp->br)) != NULL)
				m_freem(m);
			BXE_FP_UNLOCK(fp);
		}
	}

	if_qflush(ifp);

	DBEXIT(BXE_VERBOSE_UNLOAD);
}
#endif /* FreeBSD_version >= 800000 */


/*
 * Handles any IOCTL calls from the operating system.
 *
 * Returns:
 *   0 for success, positive value for failure.
 */
static int
bxe_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct bxe_softc *sc;
	struct ifreq *ifr;
	int error, mask, reinit;

	sc = ifp->if_softc;
	DBENTER(BXE_EXTREME_MISC);

	ifr = (struct ifreq *)data;
	error = 0;
	reinit = 0;

	switch (command) {
	case SIOCSIFMTU:
		/* Set the MTU. */
		DBPRINT(sc, BXE_EXTREME_MISC, "%s(): Received SIOCSIFMTU\n",
		    __FUNCTION__);

		/* Check that the MTU setting is supported. */
		if ((ifr->ifr_mtu < BXE_MIN_MTU) ||
			(ifr->ifr_mtu > BXE_JUMBO_MTU)) {
			DBPRINT(sc, BXE_WARN, "%s(): Unsupported MTU "
			    "(%d < %d < %d)!\n", __FUNCTION__, BXE_MIN_MTU,
			    ifr->ifr_mtu, BXE_JUMBO_MTU);
			error = EINVAL;
			break;
		}

		BXE_CORE_LOCK(sc);
		ifp->if_mtu = ifr->ifr_mtu;
		bxe_change_mtu(sc, ifp->if_drv_flags & IFF_DRV_RUNNING);
		BXE_CORE_UNLOCK(sc);
		break;
	case SIOCSIFFLAGS:
		/* Toggle the interface state up or down. */
		DBPRINT(sc, BXE_EXTREME_MISC, "%s(): Received SIOCSIFFLAGS\n",
		    __FUNCTION__);

		BXE_CORE_LOCK(sc);

		/* Check if the interface is up. */
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				/*
				 * Change the promiscuous/multicast flags as
				 * necessary.
				 */
				bxe_set_rx_mode(sc);
			} else {
				/* Start the HW */
				bxe_init_locked(sc, LOAD_NORMAL);
			}
		} else {
			/*
			 * The interface is down.  Check if the driver is
			 * running.
			 */
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				bxe_stop_locked(sc, UNLOAD_NORMAL);
		}
		BXE_CORE_UNLOCK(sc);
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		/* Add/Delete multicast addresses. */
		DBPRINT(sc, BXE_EXTREME_MISC,
		    "%s(): Received SIOCADDMULTI/SIOCDELMULTI\n", __FUNCTION__);

		BXE_CORE_LOCK(sc);

		/* Don't bother unless the driver's running. */
		if (ifp->if_drv_flags & IFF_DRV_RUNNING)
			bxe_set_rx_mode(sc);

		BXE_CORE_UNLOCK(sc);
		break;
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		/* Set/Get Interface media */
		DBPRINT(sc, BXE_EXTREME_MISC,
		    "%s(): Received SIOCSIFMEDIA/SIOCGIFMEDIA\n", __FUNCTION__);
		error = ifmedia_ioctl(ifp, ifr, &sc->bxe_ifmedia, command);
		break;
	case SIOCSIFCAP:
		/* Set interface capability */

		/* Find out which capabilities have changed. */
		mask = ifr->ifr_reqcap ^ ifp->if_capenable;
		DBPRINT(sc, BXE_VERBOSE_MISC,
		    "%s(): Received SIOCSIFCAP (mask = 0x%08X)\n", __FUNCTION__,
		    (uint32_t)mask);

		BXE_CORE_LOCK(sc);

		/* Toggle the LRO capabilites enable flag. */
		if (mask & IFCAP_LRO) {
			if (TPA_ENABLED(sc)) {
				ifp->if_capenable ^= IFCAP_LRO;
				sc->bxe_flags ^= BXE_TPA_ENABLE_FLAG;
				DBPRINT(sc, BXE_INFO_MISC,
				    "%s(): Toggling LRO (bxe_flags = "
				    "0x%08X).\n", __FUNCTION__, sc->bxe_flags);
			}
			reinit = 1;
		}

		/* Toggle the TX checksum capabilites enable flag. */
		if (mask & IFCAP_TXCSUM) {
			DBPRINT(sc, BXE_VERBOSE_MISC,
			    "%s(): Toggling IFCAP_TXCSUM.\n", __FUNCTION__);

			ifp->if_capenable ^= IFCAP_TXCSUM;

			if (IFCAP_TXCSUM & ifp->if_capenable)
				ifp->if_hwassist = BXE_IF_HWASSIST;
			else
				ifp->if_hwassist = 0;
		}

		/* Toggle the RX checksum capabilities enable flag. */
		if (mask & IFCAP_RXCSUM) {
			DBPRINT(sc, BXE_VERBOSE_MISC,
			    "%s(): Toggling IFCAP_RXCSUM.\n", __FUNCTION__);

			ifp->if_capenable ^= IFCAP_RXCSUM;

			if (IFCAP_RXCSUM & ifp->if_capenable)
				ifp->if_hwassist = BXE_IF_HWASSIST;
			else
				ifp->if_hwassist = 0;
		}

		/* Toggle VLAN_MTU capabilities enable flag. */
		if (mask & IFCAP_VLAN_MTU) {
			BXE_PRINTF("%s(%d): Changing VLAN_MTU not supported.\n",
			    __FILE__, __LINE__);
			error = EINVAL;
		}

		/* Toggle VLANHWTAG capabilities enabled flag. */
		if (mask & IFCAP_VLAN_HWTAGGING) {
			BXE_PRINTF(
			    "%s(%d): Changing VLAN_HWTAGGING not supported!\n",
			    __FILE__, __LINE__);
			error = EINVAL;
		}

		/* Toggle TSO4 capabilities enabled flag. */
		if (mask & IFCAP_TSO4) {
			DBPRINT(sc, BXE_VERBOSE_MISC,
			    "%s(): Toggling IFCAP_TSO4.\n", __FUNCTION__);

			ifp->if_capenable ^= IFCAP_TSO4;
		}

		/* Toggle TSO6 capabilities enabled flag. */
		if (mask & IFCAP_TSO6) {
			DBPRINT(sc, BXE_VERBOSE_MISC,
			    "%s(): Toggling IFCAP_TSO6.\n", __FUNCTION__);

			ifp->if_capenable ^= IFCAP_TSO6;
		}

		/* Handle any other capabilities. */
		if (mask & ~(IFCAP_VLAN_HWTAGGING | IFCAP_VLAN_MTU |
		    IFCAP_RXCSUM | IFCAP_TXCSUM)) {
			BXE_PRINTF("%s(%d): Unsupported capability!\n",
			    __FILE__, __LINE__);
			error = EINVAL;
		}

		/* Restart the controller with the new capabilities. */
		if (reinit) {
			bxe_stop_locked(sc, UNLOAD_NORMAL);
			bxe_init_locked(sc, LOAD_NORMAL);
		}

		BXE_CORE_UNLOCK(sc);
		break;
	default:
		/* We don't know how to handle the IOCTL, pass it on. */
		error = ether_ioctl(ifp, command, data);
		break;
	}

	DBEXIT(BXE_EXTREME_MISC);

	return (error);
}

/*
 * Gets the current value of the RX Completion Consumer index
 * from the fastpath status block, updates it as necessary if
 * it is pointing to a "Next Page" entry, and returns it to the
 * caller.
 *
 * Returns:
 * 	 The adjusted value of *fp->rx_cons_sb.
 */
static __inline uint16_t
bxe_rx_cq_cons(struct bxe_fastpath *fp)
{
	volatile uint16_t rx_cq_cons_sb = 0;

	rmb();
	rx_cq_cons_sb = (volatile uint16_t) le16toh(*fp->rx_cq_cons_sb);

	/*
	 * It is valid for the hardware's copy of the completion
	 * consumer index to be pointing at a "Next Page" entry in
	 * the completion chain but the driver prefers to assume
	 * that it is pointing at the next available CQE so we
	 * need to adjust the value accordingly.
	 */
	if ((rx_cq_cons_sb & USABLE_RCQ_ENTRIES_PER_PAGE) ==
		USABLE_RCQ_ENTRIES_PER_PAGE)
		rx_cq_cons_sb++;

	return (rx_cq_cons_sb);
}

static __inline int
bxe_has_tx_work(struct bxe_fastpath *fp)
{

	rmb();
	return (((fp->tx_pkt_prod != le16toh(*fp->tx_cons_sb)) || \
	    (fp->tx_pkt_prod != fp->tx_pkt_cons)));
}

/*
 * Checks if there are any received frames to process on the
 * completion queue.
 *
 * Returns:
 * 	 0 = No received frames pending, !0 = Received frames
 * 	 pending
 */
static __inline int
bxe_has_rx_work(struct bxe_fastpath *fp)
{

	rmb();
	return (bxe_rx_cq_cons(fp) != fp->rx_cq_cons);
}

/*
 * Slowpath task entry point.
 *
 * Returns:
 *   None
 */
static void
bxe_task_sp(void *xsc, int pending)
{
	struct bxe_softc *sc;
	uint32_t sp_status;

	sc = xsc;
	DBENTER(BXE_EXTREME_INTR);

	DBPRINT(sc, BXE_EXTREME_INTR, "%s(): pending = %d.\n", __FUNCTION__,
	    pending);

	/* Check for the source of the interrupt. */
	sp_status = bxe_update_dsb_idx(sc);

	/* Handle any hardware attentions. */
	if (sp_status & 0x1) {
		bxe_attn_int(sc);
		sp_status &= ~0x1;
	}

	/* CSTORM event asserted (query_stats, port delete ramrod, etc.). */
	if (sp_status & 0x2) {
		sc->stats_pending = 0;
		sp_status &= ~0x2;
	}

	/* Check for other weirdness. */
	if (sp_status != 0) {
		DBPRINT(sc, BXE_WARN, "%s(): Unexpected slowpath interrupt "
		    "(sp_status = 0x%04X)!\n", __FUNCTION__, sp_status);
	}

	/* Acknowledge the xSTORM tags and enable slowpath interrupts. */
	bxe_ack_sb(sc, DEF_SB_ID, ATTENTION_ID, le16toh(sc->def_att_idx),
	    IGU_INT_NOP, 1);
	bxe_ack_sb(sc, DEF_SB_ID, USTORM_ID, le16toh(sc->def_u_idx),
	    IGU_INT_NOP, 1);
	bxe_ack_sb(sc, DEF_SB_ID, CSTORM_ID, le16toh(sc->def_c_idx),
	    IGU_INT_NOP, 1);
	bxe_ack_sb(sc, DEF_SB_ID, XSTORM_ID, le16toh(sc->def_x_idx),
	    IGU_INT_NOP, 1);
	bxe_ack_sb(sc, DEF_SB_ID, TSTORM_ID, le16toh(sc->def_t_idx),
	    IGU_INT_ENABLE, 1);

	DBEXIT(BXE_EXTREME_INTR);
}


/*
 * Legacy interrupt entry point.
 *
 * Verifies that the controller generated the interrupt and
 * then calls a separate routine to handle the various
 * interrupt causes: link, RX, and TX.
 *
 * Returns:
 *   None
 */
static void
bxe_intr_legacy(void *xsc)
{
	struct bxe_softc *sc;
	struct bxe_fastpath *fp;
	uint32_t mask, fp_status;

	sc = xsc;
	fp = &sc->fp[0];

	/* Don't handle any interrupts if we're not ready. */
	if (__predict_false(sc->intr_sem != 0))
		goto bxe_intr_legacy_exit;

	/* Bail out if the interrupt wasn't generated by our hardware. */
	fp_status = bxe_ack_int(sc);
	if (fp_status == 0)
		goto bxe_intr_legacy_exit;

	/* Need to weed out calls due to shared interrupts. */
	DBENTER(BXE_EXTREME_INTR);

	/* Handle the fastpath interrupt. */
	/*
	 * sb_id = 0 for ustorm, 1 for cstorm.
	 * The bits returned from ack_int() are 0-15,
	 * bit 0=attention status block
	 * bit 1=fast path status block
	 * A mask of 0x2 or more = tx/rx event
	 * A mask of 1 = slow path event
	 */

	mask = (0x2 << fp->sb_id);
	DBPRINT(sc, BXE_EXTREME_INTR,
	    "%s(): fp_status = 0x%08X, mask = 0x%08X\n", __FUNCTION__,
	    fp_status, mask);

	/* CSTORM event means fastpath completion. */
	if (fp_status & mask) {
		/* This interrupt must be ours, disable further interrupts. */
		bxe_ack_sb(sc, fp->sb_id, USTORM_ID, 0, IGU_INT_DISABLE, 0);
#ifdef BXE_TASK
		taskqueue_enqueue(fp->tq, &fp->task);
#else
		bxe_task_fp((void *)fp, 0);
#endif
		/* Clear this event from the status flags. */
		fp_status &= ~mask;
	}

	/* Handle all slow path interrupts and attentions */
	if (fp_status & 0x1) {
		/* Acknowledge and disable further slowpath interrupts. */
		bxe_ack_sb(sc, DEF_SB_ID, TSTORM_ID, 0, IGU_INT_DISABLE, 0);
#ifdef BXE_TASK
		/* Schedule the slowpath task. */
		taskqueue_enqueue(sc->tq, &sc->task);
#else
		bxe_task_sp(xsc, 0);
#endif
		/* Clear this event from the status flags. */
		fp_status &= ~0x1;
	}

#ifdef BXE_DEBUG
	if (fp_status) {
		DBPRINT(sc, BXE_WARN,
		    "%s(): Unexpected fastpath status (fp_status = 0x%08X)!\n",
		    __FUNCTION__, fp_status);
	}
#endif

	DBEXIT(BXE_EXTREME_INTR);

bxe_intr_legacy_exit:
	return;
}

/*
 * Slowpath interrupt entry point.
 *
 * Acknowledge the interrupt and schedule a slowpath task.
 *
 * Returns:
 *   None
 */
static void
bxe_intr_sp(void *xsc)
{
	struct bxe_softc *sc;

	sc = xsc;
	DBENTER(BXE_EXTREME_INTR);

	/* Don't handle any interrupts if we're not ready. */
	if (__predict_false(sc->intr_sem != 0))
		goto bxe_intr_sp_exit;

	/* Acknowledge and disable further slowpath interrupts. */
	bxe_ack_sb(sc, DEF_SB_ID, TSTORM_ID, 0, IGU_INT_DISABLE, 0);

#ifdef BXE_TASK
	/* Schedule the slowpath task. */
	taskqueue_enqueue(sc->tq, &sc->task);
#else
	bxe_task_sp(xsc, 0);
#endif

bxe_intr_sp_exit:
	DBEXIT(BXE_EXTREME_INTR);
}

/*
 * Fastpath interrupt entry point.
 *
 * Acknowledge the interrupt and schedule a fastpath task.
 *
 * Returns:
 *   None
 */
static void
bxe_intr_fp (void *xfp)
{
	struct bxe_fastpath *fp;
	struct bxe_softc *sc;

	fp = xfp;
	sc = fp->sc;

	DBENTER(BXE_EXTREME_INTR);

	DBPRINT(sc, BXE_VERBOSE_INTR,
	    "%s(%d): MSI-X vector on fp[%d].sb_id = %d\n",
	    __FUNCTION__, curcpu, fp->index, fp->sb_id);

	/* Don't handle any interrupts if we're not ready. */
	if (__predict_false(sc->intr_sem != 0))
		goto bxe_intr_fp_exit;

	/* Disable further interrupts. */
	bxe_ack_sb(sc, fp->sb_id, USTORM_ID, 0, IGU_INT_DISABLE, 0);
#ifdef BXE_TASK
	taskqueue_enqueue(fp->tq, &fp->task);
#else
	bxe_task_fp (xfp, 0);
#endif

bxe_intr_fp_exit:
	DBEXIT(BXE_EXTREME_INTR);
}

/*
 * Fastpath task entry point.
 *
 * Handle any pending transmit or receive events.
 *
 * Returns:
 *   None
 */
static void
bxe_task_fp (void *xfp, int pending)
{
	struct bxe_fastpath *fp;
	struct bxe_softc *sc;

	fp = xfp;
	sc = fp->sc;

	DBENTER(BXE_EXTREME_INTR);

	DBPRINT(sc, BXE_EXTREME_INTR, "%s(): pending = %d.\n", __FUNCTION__,
	    pending);

	DBPRINT(sc, BXE_EXTREME_INTR, "%s(%d): Fastpath task on fp[%d]"
	    ".sb_id = %d\n", __FUNCTION__, curcpu, fp->index, fp->sb_id);

	/* Update the fast path indices */
	bxe_update_fpsb_idx(fp);

	/* Service any completed TX frames. */
	if (bxe_has_tx_work(fp)) {
		BXE_FP_LOCK(fp);
		bxe_txeof(fp);
		BXE_FP_UNLOCK(fp);
	}

	/* Service any completed RX frames. */
	rmb();
	bxe_rxeof(fp);

	/* Acknowledge the fastpath status block indices. */
	bxe_ack_sb(sc, fp->sb_id, USTORM_ID, fp->fp_u_idx, IGU_INT_NOP, 1);
	bxe_ack_sb(sc, fp->sb_id, CSTORM_ID, fp->fp_c_idx, IGU_INT_ENABLE, 1);

	DBEXIT(BXE_EXTREME_INTR);
}

/*
 * Clears the fastpath (per-queue) status block.
 *
 * Returns:
 *   None
 */
static void
bxe_zero_sb(struct bxe_softc *sc, int sb_id)
{
	int port;

	port = BP_PORT(sc);

	DBENTER(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET | BXE_VERBOSE_INTR);
	DBPRINT(sc, (BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET),
	    "%s(): Clearing sb_id = %d on port %d.\n", __FUNCTION__, sb_id,
	    port);

	/* "CSTORM" */
	bxe_init_fill(sc, CSEM_REG_FAST_MEMORY +
	    CSTORM_SB_HOST_STATUS_BLOCK_U_OFFSET(port, sb_id), 0,
	    CSTORM_SB_STATUS_BLOCK_U_SIZE / 4);
	bxe_init_fill(sc, CSEM_REG_FAST_MEMORY +
	    CSTORM_SB_HOST_STATUS_BLOCK_C_OFFSET(port, sb_id), 0,
	    CSTORM_SB_STATUS_BLOCK_C_SIZE / 4);

	DBEXIT(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET | BXE_VERBOSE_INTR);
}

/*
 * Initialize the fastpath (per queue) status block.
 *
 * Returns:
 *   None
 */
static void
bxe_init_sb(struct bxe_softc *sc, struct host_status_block *sb,
    bus_addr_t mapping, int sb_id)
{
	uint64_t section;
	int func, index, port;

	port = BP_PORT(sc);
	func = BP_FUNC(sc);

	DBENTER(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET | BXE_VERBOSE_INTR);
	DBPRINT(sc, (BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET | BXE_VERBOSE_INTR),
		 "%s():	Initializing sb_id = %d on port %d, function %d.\n",
		__FUNCTION__, sb_id, port, func);

	/* Setup the USTORM status block. */
	section = ((uint64_t)mapping) + offsetof(struct host_status_block,
	    u_status_block);
	sb->u_status_block.status_block_id = sb_id;

	REG_WR(sc, BAR_CSTORM_INTMEM +
	    CSTORM_SB_HOST_SB_ADDR_U_OFFSET(port, sb_id), U64_LO(section));
	REG_WR(sc, BAR_CSTORM_INTMEM +
	    ((CSTORM_SB_HOST_SB_ADDR_U_OFFSET(port, sb_id)) + 4),
	    U64_HI(section));
	REG_WR8(sc, BAR_CSTORM_INTMEM + FP_USB_FUNC_OFF +
	    CSTORM_SB_HOST_STATUS_BLOCK_U_OFFSET(port, sb_id), func);

	for (index = 0; index < HC_USTORM_SB_NUM_INDICES; index++)
		REG_WR16(sc, BAR_CSTORM_INTMEM +
		    CSTORM_SB_HC_DISABLE_U_OFFSET(port, sb_id, index), 0x1);

	/* Setup the CSTORM status block. */
	section = ((uint64_t)mapping) + offsetof(struct host_status_block,
	    c_status_block);
	sb->c_status_block.status_block_id = sb_id;

	/* Write the status block address to CSTORM. Order is important! */
	REG_WR(sc, BAR_CSTORM_INTMEM +
	    CSTORM_SB_HOST_SB_ADDR_C_OFFSET(port, sb_id), U64_LO(section));
	REG_WR(sc, BAR_CSTORM_INTMEM +
	    ((CSTORM_SB_HOST_SB_ADDR_C_OFFSET(port, sb_id)) + 4),
	    U64_HI(section));
	REG_WR8(sc, BAR_CSTORM_INTMEM + FP_CSB_FUNC_OFF +
	    CSTORM_SB_HOST_STATUS_BLOCK_C_OFFSET(port, sb_id), func);

	for (index = 0; index < HC_CSTORM_SB_NUM_INDICES; index++)
		REG_WR16(sc, BAR_CSTORM_INTMEM +
		    CSTORM_SB_HC_DISABLE_C_OFFSET(port, sb_id, index), 0x1);

	/* Enable interrupts. */
	bxe_ack_sb(sc, sb_id, CSTORM_ID, 0, IGU_INT_ENABLE, 0);

	DBEXIT(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET | BXE_VERBOSE_INTR);
}

/*
 * Clears the default status block.
 *
 * Returns:
 *   None
 */
static void
bxe_zero_def_sb(struct bxe_softc *sc)
{
	int func;

	func = BP_FUNC(sc);

	DBENTER(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET | BXE_VERBOSE_INTR);
	DBPRINT(sc, (BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET | BXE_VERBOSE_INTR),
	    "%s(): Clearing default status block on function %d.\n",
	    __FUNCTION__, func);

	/* Fill the STORM's copy of the default status block with 0. */
	bxe_init_fill(sc, TSEM_REG_FAST_MEMORY +
	    TSTORM_DEF_SB_HOST_STATUS_BLOCK_OFFSET(func), 0,
	    sizeof(struct tstorm_def_status_block) / 4);
	bxe_init_fill(sc, CSEM_REG_FAST_MEMORY +
	    CSTORM_DEF_SB_HOST_STATUS_BLOCK_U_OFFSET(func), 0,
	    sizeof(struct cstorm_def_status_block_u) / 4);
	bxe_init_fill(sc, CSEM_REG_FAST_MEMORY +
	    CSTORM_DEF_SB_HOST_STATUS_BLOCK_C_OFFSET(func), 0,
	    sizeof(struct cstorm_def_status_block_c) / 4);
	bxe_init_fill(sc, XSEM_REG_FAST_MEMORY +
	    XSTORM_DEF_SB_HOST_STATUS_BLOCK_OFFSET(func), 0,
	    sizeof(struct xstorm_def_status_block) / 4);

	DBEXIT(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET | BXE_VERBOSE_INTR);
}

/*
 * Initialize default status block.
 *
 * Returns:
 *   None
 */
static void
bxe_init_def_sb(struct bxe_softc *sc, struct host_def_status_block *def_sb,
    bus_addr_t mapping, int sb_id)
{
	uint64_t section;
	int func, index, port, reg_offset, val;

	port = BP_PORT(sc);
	func = BP_FUNC(sc);

	DBENTER(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET | BXE_VERBOSE_INTR);
	DBPRINT(sc, (BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET | BXE_VERBOSE_INTR),
	   "%s(): Initializing default status block on port %d, function %d.\n",
	   __FUNCTION__, port, func);

	/* Setup the default status block (DSB). */
	section = ((uint64_t)mapping) + offsetof(struct host_def_status_block,
	    atten_status_block);
	def_sb->atten_status_block.status_block_id = sb_id;
	sc->attn_state = 0;
	sc->def_att_idx = 0;

	/*
	 * Read routing configuration for attn signal
	 * output of groups. Currently, only groups
	 * 0 through 3 are wired.
	 */
	reg_offset = port ? MISC_REG_AEU_ENABLE1_FUNC_1_OUT_0 :
	    MISC_REG_AEU_ENABLE1_FUNC_0_OUT_0;

	for (index = 0; index < MAX_DYNAMIC_ATTN_GRPS; index++) {
		sc->attn_group[index].sig[0] = REG_RD(sc, reg_offset +
		    0x10 * index);
		sc->attn_group[index].sig[1] = REG_RD(sc, reg_offset +
		    0x10 * index + 0x4);
		sc->attn_group[index].sig[2] = REG_RD(sc, reg_offset +
		    0x10 * index + 0x8);
		sc->attn_group[index].sig[3] = REG_RD(sc, reg_offset +
		    0x10 * index + 0xc);

		DBPRINT(sc, (BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET |
		    BXE_VERBOSE_INTR),
		    "%s(): attn_group[%d] = 0x%08X 0x%08X 0x%08x 0X%08x\n",
		    __FUNCTION__, index, sc->attn_group[index].sig[0],
		    sc->attn_group[index].sig[1], sc->attn_group[index].sig[2],
		    sc->attn_group[index].sig[3]);
	}

	reg_offset = port ? HC_REG_ATTN_MSG1_ADDR_L : HC_REG_ATTN_MSG0_ADDR_L;

	REG_WR(sc, reg_offset, U64_LO(section));
	REG_WR(sc, reg_offset + 4, U64_HI(section));

	reg_offset = port ? HC_REG_ATTN_NUM_P1 : HC_REG_ATTN_NUM_P0;

	val = REG_RD(sc, reg_offset);
	val |= sb_id;
	REG_WR(sc, reg_offset, val);

	/* USTORM */
	section = ((uint64_t)mapping) + offsetof(struct host_def_status_block,
	    u_def_status_block);
	def_sb->u_def_status_block.status_block_id = sb_id;
	sc->def_u_idx = 0;

	REG_WR(sc, BAR_CSTORM_INTMEM +
	    CSTORM_DEF_SB_HOST_SB_ADDR_U_OFFSET(func), U64_LO(section));
	REG_WR(sc, BAR_CSTORM_INTMEM +
	    ((CSTORM_DEF_SB_HOST_SB_ADDR_U_OFFSET(func)) + 4), U64_HI(section));
	REG_WR8(sc, BAR_CSTORM_INTMEM + DEF_USB_FUNC_OFF +
	    CSTORM_DEF_SB_HOST_STATUS_BLOCK_U_OFFSET(func), func);

	for (index = 0; index < HC_USTORM_DEF_SB_NUM_INDICES; index++)
		REG_WR16(sc, BAR_CSTORM_INTMEM +
		    CSTORM_DEF_SB_HC_DISABLE_U_OFFSET(func, index), 1);

	/* CSTORM */
	section = ((uint64_t)mapping) + offsetof(struct host_def_status_block,
	    c_def_status_block);
	def_sb->c_def_status_block.status_block_id = sb_id;
	sc->def_c_idx = 0;

	REG_WR(sc, BAR_CSTORM_INTMEM +
	    CSTORM_DEF_SB_HOST_SB_ADDR_C_OFFSET(func), U64_LO(section));
	REG_WR(sc, BAR_CSTORM_INTMEM +
	    ((CSTORM_DEF_SB_HOST_SB_ADDR_C_OFFSET(func)) + 4), U64_HI(section));
	REG_WR8(sc, BAR_CSTORM_INTMEM + DEF_CSB_FUNC_OFF +
	    CSTORM_DEF_SB_HOST_STATUS_BLOCK_C_OFFSET(func), func);

	for (index = 0; index < HC_CSTORM_DEF_SB_NUM_INDICES; index++)
		REG_WR16(sc, BAR_CSTORM_INTMEM +
		    CSTORM_DEF_SB_HC_DISABLE_C_OFFSET(func, index), 1);

	/* TSTORM */
	section = ((uint64_t)mapping) + offsetof(struct host_def_status_block,
	    t_def_status_block);
	def_sb->t_def_status_block.status_block_id = sb_id;
	sc->def_t_idx = 0;

	REG_WR(sc, BAR_TSTORM_INTMEM +
	    TSTORM_DEF_SB_HOST_SB_ADDR_OFFSET(func), U64_LO(section));
	REG_WR(sc, BAR_TSTORM_INTMEM +
	    ((TSTORM_DEF_SB_HOST_SB_ADDR_OFFSET(func)) + 4), U64_HI(section));
	REG_WR8(sc, BAR_TSTORM_INTMEM + DEF_TSB_FUNC_OFF +
	    TSTORM_DEF_SB_HOST_STATUS_BLOCK_OFFSET(func), func);

	for (index = 0; index < HC_TSTORM_DEF_SB_NUM_INDICES; index++)
		REG_WR16(sc, BAR_TSTORM_INTMEM +
		    TSTORM_DEF_SB_HC_DISABLE_OFFSET(func, index), 1);

	/* XSTORM */
	section = ((uint64_t)mapping) + offsetof(struct host_def_status_block,
	    x_def_status_block);
	def_sb->x_def_status_block.status_block_id = sb_id;
	sc->def_x_idx = 0;

	REG_WR(sc, BAR_XSTORM_INTMEM +
	    XSTORM_DEF_SB_HOST_SB_ADDR_OFFSET(func), U64_LO(section));
	REG_WR(sc, BAR_XSTORM_INTMEM +
	    ((XSTORM_DEF_SB_HOST_SB_ADDR_OFFSET(func)) + 4), U64_HI(section));
	REG_WR8(sc, BAR_XSTORM_INTMEM + DEF_XSB_FUNC_OFF +
	    XSTORM_DEF_SB_HOST_STATUS_BLOCK_OFFSET(func), func);

	for (index = 0; index < HC_XSTORM_DEF_SB_NUM_INDICES; index++)
		REG_WR16(sc, BAR_XSTORM_INTMEM +
		    XSTORM_DEF_SB_HC_DISABLE_OFFSET(func, index), 1);

	sc->stats_pending = 0;
	sc->set_mac_pending = 0;

	bxe_ack_sb(sc, sb_id, CSTORM_ID, 0, IGU_INT_ENABLE, 0);

	DBEXIT(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET | BXE_VERBOSE_INTR);
}

/*
 * Update interrupt coalescing parameters.
 *
 * Returns:
 *   None
 */
static void
bxe_update_coalesce(struct bxe_softc *sc)
{
	int i, port, sb_id;

	DBENTER(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);

	port = BP_PORT(sc);
	/* Cycle through each fastpath queue and set the coalescing values. */
	for (i = 0; i < sc->num_queues; i++) {
		sb_id = sc->fp[i].sb_id;

		/* Receive interrupt coalescing is done on USTORM. */
		REG_WR8(sc, BAR_CSTORM_INTMEM +
		    CSTORM_SB_HC_TIMEOUT_U_OFFSET(port, sb_id,
		    U_SB_ETH_RX_CQ_INDEX), sc->rx_ticks / (BXE_BTR * 4));

		REG_WR16(sc, BAR_CSTORM_INTMEM +
		    CSTORM_SB_HC_DISABLE_U_OFFSET(port, sb_id,
		    U_SB_ETH_RX_CQ_INDEX),
		    (sc->rx_ticks / (BXE_BTR * 4)) ? 0 : 1);

		/* Transmit interrupt coalescing is done on CSTORM. */
		REG_WR8(sc, BAR_CSTORM_INTMEM +
		    CSTORM_SB_HC_TIMEOUT_C_OFFSET(port, sb_id,
		    C_SB_ETH_TX_CQ_INDEX), sc->tx_ticks / (BXE_BTR * 4));
		REG_WR16(sc, BAR_CSTORM_INTMEM +
		    CSTORM_SB_HC_DISABLE_C_OFFSET(port, sb_id,
		    C_SB_ETH_TX_CQ_INDEX),
		    (sc->tx_ticks / (BXE_BTR * 4)) ? 0 : 1);
	}

	DBEXIT(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);
}

/*
 * Free memory buffers from the TPA pool.
 *
 * Returns:
 *   None
 */
static __inline void
bxe_free_tpa_pool(struct bxe_fastpath *fp, int last)
{
	struct bxe_softc *sc;
	int j;

	sc = fp->sc;
#ifdef BXE_DEBUG
	int tpa_pool_max;

	tpa_pool_max = CHIP_IS_E1H(sc) ? ETH_MAX_AGGREGATION_QUEUES_E1H :
	    ETH_MAX_AGGREGATION_QUEUES_E1;
	DBRUNIF((last > tpa_pool_max), DBPRINT(sc, BXE_FATAL,
	    "%s(): Index value out of range (%d > %d)!\n", __FUNCTION__, last,
	    tpa_pool_max));
#endif

	if (!(TPA_ENABLED(sc)))
		return;

	for (j = 0; j < last; j++) {
		if (fp->rx_mbuf_tag) {
			if (fp->tpa_mbuf_map[j] != NULL) {
				bus_dmamap_sync(fp->rx_mbuf_tag,
				    fp->tpa_mbuf_map[j], BUS_DMASYNC_POSTREAD);
				bus_dmamap_unload(fp->rx_mbuf_tag,
				    fp->tpa_mbuf_map[j]);
			}

			if (fp->tpa_mbuf_ptr[j] != NULL) {
				m_freem(fp->tpa_mbuf_ptr[j]);
				DBRUN(fp->tpa_mbuf_alloc--);
				fp->tpa_mbuf_ptr[j] = NULL;
			} else {
				DBPRINT(sc, BXE_FATAL,
				    "%s(): TPA bin %d empty on free!\n",
				    __FUNCTION__, j);
			}
		}
	}
}

/*
 * Free an entry in the receive scatter gather list.
 *
 * Returns:
 *   None
 */
static __inline void
bxe_free_rx_sge(struct bxe_softc *sc, struct bxe_fastpath *fp, uint16_t index)
{
	struct eth_rx_sge *sge;

	sge = &fp->rx_sge_chain[RX_SGE_PAGE(index)][RX_SGE_IDX(index)];
	/* Skip "next page" elements */
	if (!sge)
		return;

	if (fp->rx_sge_buf_tag) {
		if (fp->rx_sge_buf_map[index]) {
			bus_dmamap_sync(fp->rx_sge_buf_tag,
			    fp->rx_sge_buf_map[index], BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(fp->rx_sge_buf_tag,
			    fp->rx_sge_buf_map[index]);
		}

		if (fp->rx_sge_buf_ptr[index]) {
			DBRUN(fp->sge_mbuf_alloc--);
			m_freem(fp->rx_sge_buf_ptr[index]);
			fp->rx_sge_buf_ptr[index] = NULL;
		}

		sge->addr_hi = sge->addr_lo = 0;
	}
}

/*
 * Free a range of scatter gather elements from the ring.
 *
 * Returns:
 *   None
 */
static __inline void
bxe_free_rx_sge_range(struct bxe_softc *sc, struct bxe_fastpath *fp, int last)
{
	int i;

	for (i = 0; i < last; i++)
		bxe_free_rx_sge(sc, fp, i);
}

/*
 * Allocate an mbuf of the specified size for the caller.
 *
 * Returns:
 *   NULL on failure or an mbuf pointer on success.
 */
static struct mbuf*
bxe_alloc_mbuf(struct bxe_fastpath *fp, int size)
{
	struct bxe_softc *sc;
	struct mbuf *m_new;

	sc = fp->sc;
	DBENTER(BXE_INSANE);

#ifdef BXE_DEBUG
	/* Simulate an mbuf allocation failure. */
	if (DB_RANDOMTRUE(bxe_debug_mbuf_allocation_failure)) {
		DBPRINT(sc, BXE_WARN,
		    "%s(): Simulated mbuf allocation failure!\n", __FUNCTION__);
		fp->mbuf_alloc_failed++;
		sc->debug_mbuf_sim_alloc_failed++;
		m_new = NULL;
		goto bxe_alloc_mbuf_exit;
	}
#endif

	/* Allocate a new mbuf with memory attached. */
	if (size <= MCLBYTES)
		m_new = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
	else
		m_new = m_getjcl(M_DONTWAIT, MT_DATA, M_PKTHDR, size);

	/* Check whether the allocation succeeded and handle a failure. */
   	if (__predict_false(m_new == NULL)) {
		DBPRINT(sc, BXE_WARN, "%s(): mbuf allocation failure!\n",
		    __FUNCTION__);
		fp->mbuf_alloc_failed++;
	    goto bxe_alloc_mbuf_exit;
   	}

	/* Do a little extra error checking when debugging. */
	DBRUN(M_ASSERTPKTHDR(m_new));

	/* Initialize the mbuf buffer length. */
	m_new->m_pkthdr.len = m_new->m_len = size;
	DBRUN(sc->debug_memory_allocated += size);

bxe_alloc_mbuf_exit:
	return (m_new);
}

/*
 * Map an mbuf into non-paged memory for the caller.
 *
 * Returns:
 *   0 = Success, !0 = Failure.
 *
 * Side-effects:
 *   The mbuf passed will be released if a mapping failure occurs.
 *   The segment mapping will be udpated if the mapping is successful.
 */
static int
bxe_map_mbuf(struct bxe_fastpath *fp, struct mbuf *m, bus_dma_tag_t tag,
    bus_dmamap_t map, bus_dma_segment_t *seg)
{
	struct bxe_softc *sc;
	bus_dma_segment_t segs[4];
	int nsegs, rc;

	sc = fp->sc;
	rc = 0;

	DBENTER(BXE_INSANE);

#ifdef BXE_DEBUG
	/* Simulate an mbuf mapping failure. */
	if (DB_RANDOMTRUE(bxe_debug_dma_map_addr_failure)) {
		DBPRINT(sc, BXE_WARN, "%s(): Simulated mbuf mapping failure!\n",
		    __FUNCTION__);
		sc->debug_mbuf_sim_map_failed++;
		fp->mbuf_alloc_failed++;
		DBRUN(sc->debug_memory_allocated -= m->m_len);
		m_freem(m);
		rc = EINVAL;
		goto bxe_map_mbuf_exit;
	}
#endif

	/* Map the buffer memory into non-paged memory. */
	rc = bus_dmamap_load_mbuf_sg(tag, map, m, segs, &nsegs, BUS_DMA_NOWAIT);

	/* Handle any mapping errors. */
	if (__predict_false(rc)) {
		DBPRINT(sc, BXE_WARN, "%s(): mbuf mapping failure (%d)!\n",
		    __FUNCTION__, rc);
		m_freem(m);
		fp->mbuf_alloc_failed++;
		goto bxe_map_mbuf_exit;
	}

	/* All mubfs must map to a single segment. */
	KASSERT(nsegs == 1, ("%s(): Too many segments (%d) returned!",
	    __FUNCTION__, nsegs));

	/* Save the DMA mapping tag for this memory buffer. */
	*seg = segs[0];

bxe_map_mbuf_exit:
	return (rc);
}

/*
 * Allocate an mbuf for the TPA pool.
 *
 * Returns:
 *   NULL on failure or an mbuf pointer on success.
 */
static struct mbuf *
bxe_alloc_tpa_mbuf(struct bxe_fastpath *fp, int index, int size)
{
	bus_dma_segment_t seg;
	struct mbuf *m;
	int rc;

	/* Allocate the new mbuf. */
	if ((m = bxe_alloc_mbuf(fp, size)) == NULL)
		goto bxe_alloc_tpa_mbuf_exit;

	/* Map the mbuf into non-paged pool. */
	rc = bxe_map_mbuf(fp, m, fp->rx_mbuf_tag, fp->tpa_mbuf_map[index],
	    &seg);

	if (rc) {
		m = NULL;
		goto bxe_alloc_tpa_mbuf_exit;
	}

	DBRUN(fp->tpa_mbuf_alloc++);

	/* Save the mapping info for the mbuf. */
	fp->tpa_mbuf_segs[index] = seg;

bxe_alloc_tpa_mbuf_exit:
	return (m);
}

/*
 * Allocate a receive scatter gather entry
 *
 * Returns:
 *   0 = Success, != Failure.
 */
static int
bxe_alloc_rx_sge(struct bxe_softc *sc, struct bxe_fastpath *fp,
    uint16_t ring_prod)
{
	struct eth_rx_sge *sge;
	bus_dma_segment_t seg;
	struct mbuf *m;
	int rc;

	sge = &fp->rx_sge_chain[RX_SGE_PAGE(ring_prod)][RX_SGE_IDX(ring_prod)];
	rc = 0;

	/* Allocate a new mbuf. */
	if ((m = bxe_alloc_mbuf(fp, PAGE_SIZE)) == NULL) {
		rc = ENOMEM;
		goto bxe_alloc_rx_sge_exit;
	}

	/* Map the mbuf into non-paged pool. */
	rc = bxe_map_mbuf(fp, m, fp->rx_sge_buf_tag,
		fp->rx_sge_buf_map[ring_prod], &seg);

	if (rc)
		goto bxe_alloc_rx_sge_exit;

	DBRUN(fp->sge_mbuf_alloc++);

	/* Add the SGE buffer to the SGE ring. */
	sge->addr_hi = htole32(U64_HI(seg.ds_addr));
	sge->addr_lo = htole32(U64_LO(seg.ds_addr));
	fp->rx_sge_buf_ptr[ring_prod] = m;

bxe_alloc_rx_sge_exit:
	return (rc);
}


/*
 * Returns:
 *   None.
 */
static void
bxe_alloc_mutexes(struct bxe_softc *sc)
{
	struct bxe_fastpath *fp;
	int i;

	DBENTER(BXE_VERBOSE_LOAD);

	BXE_CORE_LOCK_INIT(sc, device_get_nameunit(sc->dev));
	BXE_SP_LOCK_INIT(sc, "bxe_sp_lock");
	BXE_DMAE_LOCK_INIT(sc, "bxe_dmae_lock");
	BXE_PHY_LOCK_INIT(sc, "bxe_phy_lock");
	BXE_FWMB_LOCK_INIT(sc, "bxe_fwmb_lock");
	BXE_PRINT_LOCK_INIT(sc, "bxe_print_lock");

	/* Allocate one mutex for each fastpath structure. */
	for (i=0; i < sc->num_queues; i++ ) {
		fp = &sc->fp[i];

		/* Allocate per fastpath mutexes. */
		snprintf(fp->mtx_name, sizeof(fp->mtx_name), "%s:fp[%02d]",
		    device_get_nameunit(sc->dev), fp->index);
		mtx_init(&fp->mtx, fp->mtx_name, NULL, MTX_DEF);
	}

	DBEXIT(BXE_VERBOSE_LOAD);
}

/*
 * Returns:
 *   None.
 */
static void
bxe_free_mutexes(struct bxe_softc *sc)
{
	struct bxe_fastpath *fp;
	int i;

	DBENTER(BXE_VERBOSE_UNLOAD);

	for (i=0; i < sc->num_queues; i++ ) {
		fp = &sc->fp[i];

		/* Release per fastpath mutexes. */
		if (mtx_initialized(&(fp->mtx)))
			mtx_destroy(&(fp->mtx));
	}

	BXE_PRINT_LOCK_DESTROY(sc);
	BXE_FWMB_LOCK_DESTROY(sc);
	BXE_PHY_LOCK_DESTROY(sc);
	BXE_DMAE_LOCK_DESTROY(sc);
	BXE_SP_LOCK_DESTROY(sc);
	BXE_CORE_LOCK_DESTROY(sc);

	DBEXIT(BXE_VERBOSE_UNLOAD);

}



/*
 * Initialize the receive rings.
 *
 * Returns:
 *   None.
 */
static void
bxe_init_rx_chains(struct bxe_softc *sc)
{
	struct bxe_fastpath *fp;
	struct eth_rx_sge *sge;
	struct eth_rx_bd *rx_bd;
	struct eth_rx_cqe_next_page *nextpg;
	uint16_t rx_bd_prod, rx_sge_prod;
	int func, i, j, rcq_idx, rx_idx, rx_sge_idx, max_agg_queues;

	DBENTER(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);

	func = BP_FUNC(sc);
	max_agg_queues = CHIP_IS_E1(sc) ? ETH_MAX_AGGREGATION_QUEUES_E1 :
	    ETH_MAX_AGGREGATION_QUEUES_E1H;

	sc->rx_buf_size = sc->mbuf_alloc_size;

	/* Allocate memory for the TPA pool. */
	if (TPA_ENABLED(sc)) {
		DBPRINT(sc, (BXE_INFO_LOAD | BXE_INFO_RESET),
		    "%s(): mtu = %d, rx_buf_size = %d\n", __FUNCTION__,
		    (int)sc->bxe_ifp->if_mtu, sc->rx_buf_size);

		for (i = 0; i < sc->num_queues; i++) {
			fp = &sc->fp[i];
			DBPRINT(sc, (BXE_INSANE_LOAD | BXE_INSANE_RESET),
			    "%s(): Initializing fp[%02d] TPA pool.\n",
			    __FUNCTION__, i);

			for (j = 0; j < max_agg_queues; j++) {
				DBPRINT(sc,
				    (BXE_INSANE_LOAD | BXE_INSANE_RESET),
				    "%s(): Initializing fp[%02d] TPA "
				    "pool[%d].\n", __FUNCTION__, i, j);

				fp->disable_tpa = 0;
				fp->tpa_mbuf_ptr[j] = bxe_alloc_tpa_mbuf(fp, j,
				    sc->mbuf_alloc_size);

				if (fp->tpa_mbuf_ptr[j] == NULL) {
					fp->tpa_mbuf_alloc_failed++;
					BXE_PRINTF("TPA disabled on "
					    "fp[%02d]!\n", i);
					bxe_free_tpa_pool(fp, j);
					fp->disable_tpa = 1;
					break;
				}
				fp->tpa_state[j] = BXE_TPA_STATE_STOP;
			}
		}
	}

	/* Allocate memory for RX and CQ chains. */
	for (i = 0; i < sc->num_queues; i++) {
		fp = &sc->fp[i];
		DBPRINT(sc, (BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET),
		    "%s(): Initializing fp[%d] RX chain.\n", __FUNCTION__, i);

		fp->rx_bd_cons = fp->rx_bd_prod = 0;
		fp->rx_cq_cons = fp->rx_cq_prod = 0;

		/* Status block's completion queue consumer index. */
		fp->rx_cq_cons_sb = &fp->status_block->
		    u_status_block.index_values[HC_INDEX_U_ETH_RX_CQ_CONS];

		/* Pointer to status block's receive consumer index. */
		fp->rx_bd_cons_sb = &fp->status_block->
		    u_status_block.index_values[HC_INDEX_U_ETH_RX_BD_CONS];

		if (TPA_ENABLED(sc)) {
			DBPRINT(sc, (BXE_INSANE_LOAD | BXE_INSANE_RESET),
			    "%s(): Linking fp[%d] SGE rings.\n", __FUNCTION__,
			    i);

			/* Link the SGE Ring Pages to form SGE chain */
			for (j = 0; j < NUM_RX_SGE_PAGES; j++) {
				rx_sge_idx = ((j + 1) % NUM_RX_SGE_PAGES);
				sge = &fp->rx_sge_chain[j][MAX_RX_SGE_CNT];

				DBPRINT(sc, (BXE_EXTREME_LOAD | BXE_EXTREME_RESET),
				    "%s(): fp[%02d].rx_sge_chain[%02d][0x%04X]=0x%jX\n",
				     __FUNCTION__, i, j,
				    (uint16_t) MAX_RX_SGE_CNT,
				    (uintmax_t) fp->rx_sge_chain_paddr[rx_sge_idx]);

				sge->addr_hi =
				    htole32(U64_HI(fp->rx_sge_chain_paddr[rx_sge_idx]));
				sge->addr_lo =
				    htole32(U64_LO(fp->rx_sge_chain_paddr[rx_sge_idx]));
			}

			bxe_init_sge_ring_bit_mask(fp);
		}

		DBPRINT(sc, (BXE_INSANE_LOAD | BXE_INSANE_RESET),
		    "%s(): Linking fp[%d] RX chain pages.\n", __FUNCTION__, i);

		/* Link the pages to form the RX BD Chain. */
		for (j = 0; j < NUM_RX_PAGES; j++) {
			rx_idx = ((j + 1) % NUM_RX_PAGES);
			rx_bd = &fp->rx_bd_chain[j][USABLE_RX_BD_PER_PAGE];

			DBPRINT(sc, (BXE_EXTREME_LOAD),
			    "%s(): fp[%02d].rx_bd_chain[%02d][0x%04X]=0x%jX\n",
			     __FUNCTION__, i, j,
			    (uint16_t) USABLE_RX_BD_PER_PAGE,
			    (uintmax_t) fp->rx_bd_chain_paddr[rx_idx]);

			rx_bd->addr_hi =
			    htole32(U64_HI(fp->rx_bd_chain_paddr[rx_idx]));
			rx_bd->addr_lo =
			    htole32(U64_LO(fp->rx_bd_chain_paddr[rx_idx]));
		}

		DBPRINT(sc, (BXE_INSANE_LOAD | BXE_INSANE_RESET),
		    "%s(): Linking fp[%d] RX completion chain pages.\n",
		    __FUNCTION__, i);

		/* Link the pages to form the RX Completion Queue.*/
		for (j = 0; j < NUM_RCQ_PAGES; j++) {
			rcq_idx = ((j + 1) % NUM_RCQ_PAGES);
			nextpg = (struct eth_rx_cqe_next_page *)
			    &fp->rx_cq_chain[j][USABLE_RCQ_ENTRIES_PER_PAGE];

			DBPRINT(sc, (BXE_EXTREME_LOAD),
			    "%s(): fp[%02d].rx_cq_chain[%02d][0x%04X]=0x%jX\n",
			     __FUNCTION__, i, j,
			    (uint16_t) USABLE_RCQ_ENTRIES_PER_PAGE,
			    (uintmax_t) fp->rx_cq_chain_paddr[rcq_idx]);

			nextpg->addr_hi =
			    htole32(U64_HI(fp->rx_cq_chain_paddr[rcq_idx]));
			nextpg->addr_lo =
			    htole32(U64_LO(fp->rx_cq_chain_paddr[rcq_idx]));
		}

		if (TPA_ENABLED(sc)) {
			/* Allocate SGEs and initialize the ring elements. */
			rx_sge_prod = 0;

			while (rx_sge_prod < sc->rx_ring_size) {
				if (bxe_alloc_rx_sge(sc, fp, rx_sge_prod) != 0) {
					fp->tpa_mbuf_alloc_failed++;
					BXE_PRINTF(
					    "%s(%d): Memory allocation failure! "
					    "Disabling TPA for fp[%02d].\n",
					    __FILE__, __LINE__, i);

					/* Cleanup already allocated elements */
					bxe_free_rx_sge_range(sc, fp,
					    rx_sge_prod);
					fp->disable_tpa = 1;
					rx_sge_prod = 0;
					break;
				}
				rx_sge_prod = NEXT_SGE_IDX(rx_sge_prod);
			}

			fp->rx_sge_prod = rx_sge_prod;
		}

		/*
		 * Allocate buffers for all the RX BDs in RX BD Chain.
		 */
		rx_bd_prod = 0;
		DBRUN(fp->free_rx_bd = sc->rx_ring_size);

		for (j = 0; j < sc->rx_ring_size; j++) {
			if (bxe_get_buf(fp, NULL, rx_bd_prod)) {
				BXE_PRINTF(
	"%s(%d): Memory allocation failure! Cannot fill fp[%d] RX chain.\n",
				    __FILE__, __LINE__, i);
				break;
			}
			rx_bd_prod = NEXT_RX_BD(rx_bd_prod);
		}

		/* Update the driver's copy of the producer indices. */
		fp->rx_bd_prod = rx_bd_prod;
		fp->rx_cq_prod = TOTAL_RCQ_ENTRIES;
		fp->rx_pkts = fp->rx_calls = 0;

		DBPRINT(sc, (BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET),
		    "%s(): USABLE_RX_BD=0x%04X, USABLE_RCQ_ENTRIES=0x%04X\n",
		    __FUNCTION__, (uint16_t) USABLE_RX_BD,
		    (uint16_t) USABLE_RCQ_ENTRIES);
		DBPRINT(sc, (BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET),
		    "%s(): fp[%02d]->rx_bd_prod=0x%04X, rx_cq_prod=0x%04X\n",
		    __FUNCTION__, i, fp->rx_bd_prod, fp->rx_cq_prod);


		/* Prepare the recevie BD and CQ buffers for DMA access. */
		for (j = 0; j < NUM_RX_PAGES; j++)
			bus_dmamap_sync(fp->rx_bd_chain_tag,
			    fp->rx_bd_chain_map[j], BUS_DMASYNC_PREREAD |
			    BUS_DMASYNC_PREWRITE);

		for (j = 0; j < NUM_RCQ_PAGES; j++)
			bus_dmamap_sync(fp->rx_cq_chain_tag,
			    fp->rx_cq_chain_map[j], BUS_DMASYNC_PREREAD |
			    BUS_DMASYNC_PREWRITE);

		/*
		 * Tell the controller that we have rx_bd's and CQE's
		 * available.  Warning! this will generate an interrupt
		 * (to the TSTORM).  This must only be done when the
		 * controller is initialized.
		 */
		bxe_update_rx_prod(sc, fp, fp->rx_bd_prod,
		    fp->rx_cq_prod, fp->rx_sge_prod);

		/*
		 * Tell controller where the receive CQ
		 * chains start in physical memory.
		 */
		if (i == 0) {
			REG_WR(sc, BAR_USTORM_INTMEM +
			    USTORM_MEM_WORKAROUND_ADDRESS_OFFSET(func),
			    U64_LO(fp->rx_cq_chain_paddr[0]));
			REG_WR(sc, BAR_USTORM_INTMEM +
			    USTORM_MEM_WORKAROUND_ADDRESS_OFFSET(func) + 4,
			    U64_HI(fp->rx_cq_chain_paddr[0]));
		}
	}

	DBEXIT(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);
}

/*
 * Initialize the transmit chain.
 *
 * Returns:
 *   None.
 */
static void
bxe_init_tx_chains(struct bxe_softc *sc)
{
	struct bxe_fastpath *fp;
	struct eth_tx_next_bd *tx_n_bd;
	int i, j;

	DBENTER(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);

	for (i = 0; i < sc->num_queues; i++) {
		fp = &sc->fp[i];

		DBPRINT(sc, (BXE_INSANE_LOAD | BXE_INSANE_RESET),
		    "%s(): Linking fp[%d] TX chain pages.\n", __FUNCTION__, i);

		for (j = 0; j < NUM_TX_PAGES; j++) {
			tx_n_bd =
			    &fp->tx_bd_chain[j][USABLE_TX_BD_PER_PAGE].next_bd;

			DBPRINT(sc, (BXE_INSANE_LOAD | BXE_INSANE_RESET),
			    "%s(): Linking fp[%d] TX BD chain page[%d].\n",
			    __FUNCTION__, i, j);

			tx_n_bd->addr_hi =
			    htole32(U64_HI(fp->tx_bd_chain_paddr[(j + 1) %
			    NUM_TX_PAGES]));
			tx_n_bd->addr_lo =
			    htole32(U64_LO(fp->tx_bd_chain_paddr[(j + 1) %
			    NUM_TX_PAGES]));
		}

		fp->tx_db.data.header.header = DOORBELL_HDR_DB_TYPE;
		fp->tx_db.data.zero_fill1 = 0;
		fp->tx_db.data.prod = 0;

		fp->tx_pkt_prod = 0;
		fp->tx_pkt_cons = 0;
		fp->tx_bd_prod  = 0;
		fp->tx_bd_cons  = 0;
		fp->used_tx_bd  = 0;

		/*
		 * Copy of TX BD Chain completion queue Consumer Index
		 * from the Status Block.
		 */
		fp->tx_cons_sb =
		    &fp->status_block->c_status_block.index_values[C_SB_ETH_TX_CQ_INDEX];

		fp->tx_pkts = 0;
	}

	DBEXIT(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);
}

/*
 * Free memory and clear the RX data structures.
 *
 * Returns:
 *   Nothing.
 */
static void
bxe_free_rx_chains(struct bxe_softc *sc)
{
	struct bxe_fastpath *fp;
	int i, j, max_agg_queues;

	DBENTER(BXE_VERBOSE_RESET);

	for (i = 0; i < sc->num_queues; i++) {
		fp = &sc->fp[i];
		if (fp->rx_mbuf_tag) {
			/* Free any mbufs still in the RX mbuf chain. */
			for (j = 0; j < TOTAL_RX_BD; j++) {
				if (fp->rx_mbuf_ptr[j] != NULL) {
					if (fp->rx_mbuf_map[j] != NULL)
						bus_dmamap_sync(fp->rx_mbuf_tag,
						    fp->rx_mbuf_map[j],
						    BUS_DMASYNC_POSTREAD);
					DBRUN(fp->rx_mbuf_alloc--);
					m_freem(fp->rx_mbuf_ptr[j]);
					fp->rx_mbuf_ptr[j] = NULL;
				}
			}

			/* Clear each RX chain page. */
			for (j = 0; j < NUM_RX_PAGES; j++) {
				if (fp->rx_bd_chain[j] != NULL)
					bzero((char *)fp->rx_bd_chain[j],
					    BXE_RX_CHAIN_PAGE_SZ);
			}

			/* Clear each RX completion queue page. */
			for (j = 0; j < NUM_RCQ_PAGES; j++) {
				if (fp->rx_cq_chain[j] != NULL)
					bzero((char *)fp->rx_cq_chain[j],
					    BXE_RX_CHAIN_PAGE_SZ);
			}

			if (TPA_ENABLED(sc)) {
				max_agg_queues = CHIP_IS_E1H(sc) ?
				    ETH_MAX_AGGREGATION_QUEUES_E1H :
				    ETH_MAX_AGGREGATION_QUEUES_E1;

				/* Free the TPA Pool mbufs. */
				bxe_free_tpa_pool(fp, max_agg_queues);

				/*
				 * Free any mbufs still in the RX SGE
				 * buf chain.
				 */
				bxe_free_rx_sge_range(fp->sc, fp, MAX_RX_SGE);

				/* Clear each RX SGE page. */
				for (j = 0; j < NUM_RX_SGE_PAGES; j++) {
					if (fp->rx_sge_chain[j] != NULL)
						bzero(
						    (char *)fp->rx_sge_chain[j],
						    BXE_RX_CHAIN_PAGE_SZ);
				}
			}
		}

		/* Check if we lost any mbufs in the process. */
		DBRUNIF((fp->rx_mbuf_alloc), DBPRINT(sc, BXE_FATAL,
		    "%s(): Memory leak! Lost %d mbufs from fp[%d] RX chain!\n",
		    __FUNCTION__, fp->rx_mbuf_alloc, fp->index));
	}

	DBEXIT(BXE_VERBOSE_RESET);
}

/*
 * Free memory and clear the TX data structures.
 *
 * Returns:
 *   Nothing.
 */
static void
bxe_free_tx_chains(struct bxe_softc *sc)
{
	struct bxe_fastpath *fp;
	int i, j;

	DBENTER(BXE_VERBOSE_RESET);

	for (i = 0; i < sc->num_queues; i++) {
		fp = &sc->fp[i];
		if (fp->tx_mbuf_tag) {
			/*
			 * Unmap, unload, and free any mbufs in the
			 * TX mbuf chain.
			 */
			for (j = 0; j < TOTAL_TX_BD; j++) {
				if (fp->tx_mbuf_ptr[j] != NULL) {
					if (fp->tx_mbuf_map[j] != NULL)
						bus_dmamap_sync(fp->tx_mbuf_tag,
						    fp->tx_mbuf_map[j],
						    BUS_DMASYNC_POSTWRITE);
					DBRUN(fp->tx_mbuf_alloc--);
					m_freem(fp->tx_mbuf_ptr[j]);
					fp->tx_mbuf_ptr[j] = NULL;
				}
			}

			/* Clear each TX chain page. */
			for (j = 0; j < NUM_TX_PAGES; j++) {
				if (fp->tx_bd_chain[j] != NULL)
					bzero((char *)fp->tx_bd_chain[j],
					    BXE_TX_CHAIN_PAGE_SZ);
			}

			/* Check if we lost any mbufs in the process. */
			DBRUNIF((fp->tx_mbuf_alloc), DBPRINT(sc, BXE_FATAL,
		"%s(): Memory leak! Lost %d mbufs from fp[%d] TX chain!\n",
			    __FUNCTION__, fp->tx_mbuf_alloc, fp->index));
		}
	}

	DBEXIT(BXE_VERBOSE_RESET);
}

/*
 * Initialize the slowpath ring.
 *
 * Returns:
 *   None.
 */
static void
bxe_init_sp_ring(struct bxe_softc *sc)
{
	int func;

	func = BP_FUNC(sc);

	DBENTER(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);

	bzero((char *)sc->slowpath, BXE_SLOWPATH_SZ);

	/* When the producer equals the consumer the chain is empty. */
	sc->spq_left = MAX_SPQ_PENDING;
	sc->spq_prod_idx = 0;
	sc->dsb_sp_prod = BXE_SP_DSB_INDEX;
	sc->spq_prod_bd = sc->spq;
	sc->spq_last_bd = sc->spq_prod_bd + MAX_SP_DESC_CNT;

	/* Tell the controller the address of the slowpath ring. */
	REG_WR(sc, XSEM_REG_FAST_MEMORY + XSTORM_SPQ_PAGE_BASE_OFFSET(func),
	    U64_LO(sc->spq_paddr));
	REG_WR(sc, XSEM_REG_FAST_MEMORY + XSTORM_SPQ_PAGE_BASE_OFFSET(func) + 4,
	    U64_HI(sc->spq_paddr));
	REG_WR(sc, XSEM_REG_FAST_MEMORY + XSTORM_SPQ_PROD_OFFSET(func),
	    sc->spq_prod_idx);

	DBEXIT(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);
}

/*
 * Initialize STORM processor context.
 *
 * Returns:
 *   None.
 */
static void
bxe_init_context(struct bxe_softc *sc)
{
	struct eth_context *context;
	struct bxe_fastpath *fp;
	uint8_t sb_id;
	uint8_t cl_id;
	int i;

	DBENTER(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);

	for (i = 0; i < sc->num_queues; i++) {
		context = BXE_SP(sc, context[i].eth);
		fp = &sc->fp[i];
		sb_id = fp->sb_id;
		cl_id = fp->cl_id;

		/* Update the USTORM context. */
		context->ustorm_st_context.common.sb_index_numbers =
		    BXE_RX_SB_INDEX_NUM;
		context->ustorm_st_context.common.clientId = cl_id;
		context->ustorm_st_context.common.status_block_id = sb_id;
		/* Enable packet alignment/pad and statistics. */
		context->ustorm_st_context.common.flags =
		    USTORM_ETH_ST_CONTEXT_CONFIG_ENABLE_MC_ALIGNMENT;
		if (sc->stats_enable == TRUE)
			context->ustorm_st_context.common.flags |=
		    USTORM_ETH_ST_CONTEXT_CONFIG_ENABLE_STATISTICS;
		context->ustorm_st_context.common.statistics_counter_id=cl_id;
		/*
		 * Set packet alignment boundary.
		 * (Must be >= 4 (i.e. 16 bytes).)
		 */
		context->ustorm_st_context.common.mc_alignment_log_size = 8;
		/* Set the size of the receive buffers. */
		context->ustorm_st_context.common.bd_buff_size =
		    sc->rx_buf_size;

		/* Set the address of the receive chain base page. */
		context->ustorm_st_context.common.bd_page_base_hi =
		    U64_HI(fp->rx_bd_chain_paddr[0]);
		context->ustorm_st_context.common.bd_page_base_lo =
		    U64_LO(fp->rx_bd_chain_paddr[0]);

		if (TPA_ENABLED(sc) && !(fp->disable_tpa)) {
			/* Enable TPA and SGE chain support. */
			context->ustorm_st_context.common.flags |=
			    USTORM_ETH_ST_CONTEXT_CONFIG_ENABLE_TPA;
			/* Set the size of the SGE buffer. */
			context->ustorm_st_context.common.sge_buff_size =
			    (uint16_t) (PAGES_PER_SGE * BCM_PAGE_SIZE);
			/* Set the address of the SGE chain base page. */
			context->ustorm_st_context.common.sge_page_base_hi =
			    U64_HI(fp->rx_sge_chain_paddr[0]);
			context->ustorm_st_context.common.sge_page_base_lo =
			    U64_LO(fp->rx_sge_chain_paddr[0]);

			context->ustorm_st_context.common.max_sges_for_packet =
			    SGE_PAGE_ALIGN(sc->bxe_ifp->if_mtu) >>
			    SGE_PAGE_SHIFT;
			context->ustorm_st_context.common.max_sges_for_packet =
			    ((context->ustorm_st_context.common.
			    max_sges_for_packet + PAGES_PER_SGE - 1) &
			    (~(PAGES_PER_SGE - 1))) >> PAGES_PER_SGE_SHIFT;
		}

		/* Update USTORM context. */
		context->ustorm_ag_context.cdu_usage =
		    CDU_RSRVD_VALUE_TYPE_A(HW_CID(sc, i),
		    CDU_REGION_NUMBER_UCM_AG, ETH_CONNECTION_TYPE);

		/* Update XSTORM context. */
		context->xstorm_ag_context.cdu_reserved =
		    CDU_RSRVD_VALUE_TYPE_A(HW_CID(sc, i),
		    CDU_REGION_NUMBER_XCM_AG, ETH_CONNECTION_TYPE);

		/* Set the address of the transmit chain base page. */
		context->xstorm_st_context.tx_bd_page_base_hi =
		    U64_HI(fp->tx_bd_chain_paddr[0]);
		context->xstorm_st_context.tx_bd_page_base_lo =
		    U64_LO(fp->tx_bd_chain_paddr[0]);

		/* Enable XSTORM statistics. */
		context->xstorm_st_context.statistics_data = (cl_id |
		    XSTORM_ETH_ST_CONTEXT_STATISTICS_ENABLE);

		/* Update CSTORM status block configuration. */
		context->cstorm_st_context.sb_index_number =
		    C_SB_ETH_TX_CQ_INDEX;
		context->cstorm_st_context.status_block_id = sb_id;
	}

	DBEXIT(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);
}

/*
 * Initialize indirection table.
 *
 * Returns:
 *   None.
 */
static void
bxe_init_ind_table(struct bxe_softc *sc)
{
	int func, i;

	func = BP_FUNC(sc);

	DBENTER(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);

	if (sc->multi_mode == ETH_RSS_MODE_DISABLED)
		return;

	/* Initialize the indirection table. */
	for (i = 0; i < TSTORM_INDIRECTION_TABLE_SIZE; i++)
		REG_WR8(sc, BAR_TSTORM_INTMEM +
		    TSTORM_INDIRECTION_TABLE_OFFSET(func) + i,
		    sc->fp->cl_id + (i % sc->num_queues));

	DBEXIT(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);
}

/*
 * Set client configuration.
 *
 * Returns:
 *   None.
 */
static void
bxe_set_client_config(struct bxe_softc *sc)
{
	struct tstorm_eth_client_config tstorm_client = {0};
	int i, port;

	port = BP_PORT(sc);

	DBENTER(BXE_VERBOSE_MISC);

	tstorm_client.mtu = sc->bxe_ifp->if_mtu; /* ETHERMTU */
	tstorm_client.config_flags =
	    (TSTORM_ETH_CLIENT_CONFIG_STATSITICS_ENABLE |
	    TSTORM_ETH_CLIENT_CONFIG_E1HOV_REM_ENABLE);

	/* Unconditionally enable VLAN tag stripping. */
	if (sc->rx_mode) {
		tstorm_client.config_flags |=
		    TSTORM_ETH_CLIENT_CONFIG_VLAN_REM_ENABLE;
		DBPRINT(sc, BXE_VERBOSE, "%s(): VLAN tag stripping enabled.\n",
		    __FUNCTION__);
	}

	/* Initialize the receive mode for each receive queue. */
	for (i = 0; i < sc->num_queues; i++) {
		tstorm_client.statistics_counter_id = sc->fp[i].cl_id;

		REG_WR(sc, BAR_TSTORM_INTMEM +
		    TSTORM_CLIENT_CONFIG_OFFSET(port, sc->fp[i].cl_id),
		    ((uint32_t *) &tstorm_client)[0]);
		REG_WR(sc, BAR_TSTORM_INTMEM +
		    TSTORM_CLIENT_CONFIG_OFFSET(port, sc->fp[i].cl_id) + 4,
		    ((uint32_t *) &tstorm_client)[1]);
	}

	DBEXIT(BXE_VERBOSE_MISC);
}

/*
 * Set receive mode.
 *
 * Programs the MAC according to the type of unicast/broadcast/multicast
 * packets it should receive.
 *
 * Returns:
 *   None.
 */
static void
bxe_set_storm_rx_mode(struct bxe_softc *sc)
{
	struct tstorm_eth_mac_filter_config tstorm_mac_filter = {0};
	uint32_t llh_mask;
	int mode, mask;
	int func, i , port;

	DBENTER(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);

	mode = sc->rx_mode;
	mask = 1 << BP_L_ID(sc);
	func = BP_FUNC(sc);
	port = BP_PORT(sc);

	/* All but management unicast packets should pass to the host as well */
	llh_mask = NIG_LLH0_BRB1_DRV_MASK_REG_LLH0_BRB1_DRV_MASK_BRCST |
	    NIG_LLH0_BRB1_DRV_MASK_REG_LLH0_BRB1_DRV_MASK_MLCST |
	    NIG_LLH0_BRB1_DRV_MASK_REG_LLH0_BRB1_DRV_MASK_VLAN |
	    NIG_LLH0_BRB1_DRV_MASK_REG_LLH0_BRB1_DRV_MASK_NO_VLAN;

	/* Set the individual accept/drop flags based on the receive mode. */
	switch (mode) {
	case BXE_RX_MODE_NONE:
		/* Drop everything. */
		DBPRINT(sc, BXE_VERBOSE,
		    "%s(): Setting RX_MODE_NONE for function %d.\n",
		    __FUNCTION__, func);
		tstorm_mac_filter.ucast_drop_all = mask;
		tstorm_mac_filter.mcast_drop_all = mask;
		tstorm_mac_filter.bcast_drop_all = mask;
		break;
	case BXE_RX_MODE_NORMAL:
		/* Accept all broadcast frames. */
		DBPRINT(sc, BXE_VERBOSE,
		    "%s(): Setting RX_MODE_NORMAL for function %d.\n",
		    __FUNCTION__, func);
		tstorm_mac_filter.bcast_accept_all = mask;
		break;
	case BXE_RX_MODE_ALLMULTI:
		/* Accept all broadcast and multicast frames. */
		DBPRINT(sc, BXE_VERBOSE,
		    "%s(): Setting RX_MODE_ALLMULTI for function %d.\n",
		    __FUNCTION__, func);
		tstorm_mac_filter.mcast_accept_all = mask;
		tstorm_mac_filter.bcast_accept_all = mask;
		break;
	case BXE_RX_MODE_PROMISC:
		/* Accept all frames (promiscuous mode). */
		DBPRINT(sc, BXE_VERBOSE,
		    "%s(): Setting RX_MODE_PROMISC for function %d.\n",
		    __FUNCTION__, func);
		tstorm_mac_filter.ucast_accept_all = mask;
		tstorm_mac_filter.mcast_accept_all = mask;
		tstorm_mac_filter.bcast_accept_all = mask;
		llh_mask |= NIG_LLH0_BRB1_DRV_MASK_REG_LLH0_BRB1_DRV_MASK_UNCST;
		break;

	default:
		BXE_PRINTF(
		    "%s(%d): Tried to set unknown receive mode (0x%08X)!\n",
		    __FILE__, __LINE__, mode);
	}

	REG_WR(sc, port ? NIG_REG_LLH1_BRB1_DRV_MASK :
	    NIG_REG_LLH0_BRB1_DRV_MASK, llh_mask);

	/* Write the RX mode filter to the TSTORM. */
	for (i = 0; i < sizeof(struct tstorm_eth_mac_filter_config) / 4; i++)
		REG_WR(sc, BAR_TSTORM_INTMEM +
		    TSTORM_MAC_FILTER_CONFIG_OFFSET(func) + (i * 4),
		    ((uint32_t *) &tstorm_mac_filter)[i]);

	if (mode != BXE_RX_MODE_NONE)
		bxe_set_client_config(sc);

	DBEXIT(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);
}

/*
 * Initialize common internal resources.  (Applies to both ports and
 * functions.)
 *
 * Returns:
 *   Nothing.
 */
static void
bxe_init_internal_common(struct bxe_softc *sc)
{
	int i;

	DBENTER(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);

	/*
	 * Zero this manually as its initialization is currently not
	 * handled through block initialization.
	 */
	for (i = 0; i < (USTORM_AGG_DATA_SIZE >> 2); i++)
		REG_WR(sc, BAR_USTORM_INTMEM + USTORM_AGG_DATA_OFFSET + i * 4,
		    0);

	DBEXIT(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);
}

/*
 * Initialize port specific internal resources.
 *
 * Returns:
 *   Nothing.
 */
static void
bxe_init_internal_port(struct bxe_softc *sc)
{
	int port = BP_PORT(sc);

	port = BP_PORT(sc);

	DBENTER(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);
	DBPRINT(sc, (BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET),
	    "%s(): Port %d internal initialization.\n", __FUNCTION__, port);

	/*
	 * Each SDM timer tick is 4us. Configure host coalescing
	 * basic timer resolution (BTR) to 12us (3 * 4us).
	 */
	REG_WR(sc, BAR_CSTORM_INTMEM + CSTORM_HC_BTR_U_OFFSET(port), BXE_BTR);
	REG_WR(sc, BAR_CSTORM_INTMEM + CSTORM_HC_BTR_C_OFFSET(port), BXE_BTR);
	REG_WR(sc, BAR_TSTORM_INTMEM + TSTORM_HC_BTR_OFFSET(port), BXE_BTR);
	REG_WR(sc, BAR_XSTORM_INTMEM + XSTORM_HC_BTR_OFFSET(port), BXE_BTR);

	DBEXIT(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);
}

/*
 * Initialize function specific internal resources.
 *
 * Returns:
 *   Nothing.
 */
static void
bxe_init_internal_func(struct bxe_softc *sc)
{
	struct tstorm_eth_function_common_config tstorm_config = {0};
	struct stats_indication_flags stats_flags = {0};
	struct ustorm_eth_rx_pause_data_e1h rx_pause = {0};
	struct bxe_fastpath *fp;
	struct eth_rx_cqe_next_page *nextpg;
	uint32_t offset, size;
	uint16_t max_agg_size;
	uint8_t cl_id;
	int func, i, j, port;

	port = BP_PORT(sc);
	func = BP_FUNC(sc);

	DBENTER(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);
	DBPRINT(sc, (BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET),
	    "%s(): Port %d, function %d internal initialization.\n",
	    __FUNCTION__, port, func);

	/*
	 * Configure which fields the controller looks at when
	 * distributing incoming frames for RSS/multi-queue operation.
	 */
	if (sc->num_queues > 1) {
		tstorm_config.config_flags = MULTI_FLAGS(sc);
		tstorm_config.rss_result_mask = MULTI_MASK;
	}

	/* Enable TPA if needed */
	if (sc->bxe_flags & BXE_TPA_ENABLE_FLAG)
		tstorm_config.config_flags |=
		    TSTORM_ETH_FUNCTION_COMMON_CONFIG_ENABLE_TPA;

	if (IS_E1HMF(sc))
		tstorm_config.config_flags |=
		    TSTORM_ETH_FUNCTION_COMMON_CONFIG_E1HOV_IN_CAM;

	tstorm_config.leading_client_id = BP_L_ID(sc);

	REG_WR(sc, BAR_TSTORM_INTMEM +
	    TSTORM_FUNCTION_COMMON_CONFIG_OFFSET(func),
	    (*(uint32_t *)&tstorm_config));

	/* Don't receive anything until the link is up. */
	sc->rx_mode = BXE_RX_MODE_NONE;
	sc->rx_mode_cl_mask = (1 << BP_L_ID(sc));
	bxe_set_storm_rx_mode(sc);

	for (i = 0; i < sc->num_queues; i++) {
		cl_id = sc->fp[i].cl_id;
		/* Reset XSTORM per client statistics. */
		size = sizeof(struct xstorm_per_client_stats) / 4;
		offset = BAR_XSTORM_INTMEM +
		    XSTORM_PER_COUNTER_ID_STATS_OFFSET(port, cl_id);
		for (j = 0; j < size; j++)
			REG_WR(sc, offset +(j * 4), 0);

		/* Reset TSTORM per client statistics. */
		size = sizeof(struct tstorm_per_client_stats) / 4;
		offset = BAR_TSTORM_INTMEM +
		    TSTORM_PER_COUNTER_ID_STATS_OFFSET(port, cl_id);
		for (j = 0; j < size; j++)
			REG_WR(sc, offset + (j * 4), 0);

		/* Reset USTORM per client statistics. */
		size = sizeof(struct ustorm_per_client_stats) / 4;
		offset = BAR_USTORM_INTMEM +
		    USTORM_PER_COUNTER_ID_STATS_OFFSET(port, cl_id);
		for (j = 0; j < size; j++)
			REG_WR(sc, offset + (j * 4), 0);
	}

	/* Initialize statistics related context. */
	stats_flags.collect_eth = 1;

	REG_WR(sc, BAR_XSTORM_INTMEM + XSTORM_STATS_FLAGS_OFFSET(func),
	    ((uint32_t *)&stats_flags)[0]);
	REG_WR(sc, BAR_XSTORM_INTMEM + XSTORM_STATS_FLAGS_OFFSET(func) + 4,
	    ((uint32_t *)&stats_flags)[1]);

	REG_WR(sc, BAR_TSTORM_INTMEM + TSTORM_STATS_FLAGS_OFFSET(func),
	    ((uint32_t *)&stats_flags)[0]);
	REG_WR(sc, BAR_TSTORM_INTMEM + TSTORM_STATS_FLAGS_OFFSET(func) + 4,
	    ((uint32_t *)&stats_flags)[1]);

	REG_WR(sc, BAR_USTORM_INTMEM + USTORM_STATS_FLAGS_OFFSET(func),
	    ((uint32_t *)&stats_flags)[0]);
	REG_WR(sc, BAR_USTORM_INTMEM + USTORM_STATS_FLAGS_OFFSET(func) + 4,
	    ((uint32_t *)&stats_flags)[1]);

	REG_WR(sc, BAR_CSTORM_INTMEM + CSTORM_STATS_FLAGS_OFFSET(func),
	    ((uint32_t *)&stats_flags)[0]);
	REG_WR(sc, BAR_CSTORM_INTMEM + CSTORM_STATS_FLAGS_OFFSET(func) + 4,
	    ((uint32_t *)&stats_flags)[1]);

	REG_WR(sc, BAR_XSTORM_INTMEM + XSTORM_ETH_STATS_QUERY_ADDR_OFFSET(func),
	    U64_LO(BXE_SP_MAPPING(sc, fw_stats)));
	REG_WR(sc, BAR_XSTORM_INTMEM +
	    XSTORM_ETH_STATS_QUERY_ADDR_OFFSET(func) + 4,
	    U64_HI(BXE_SP_MAPPING(sc, fw_stats)));

	REG_WR(sc, BAR_TSTORM_INTMEM + TSTORM_ETH_STATS_QUERY_ADDR_OFFSET(func),
	    U64_LO(BXE_SP_MAPPING(sc, fw_stats)));
	REG_WR(sc, BAR_TSTORM_INTMEM +
	    TSTORM_ETH_STATS_QUERY_ADDR_OFFSET(func) + 4,
	    U64_HI(BXE_SP_MAPPING(sc, fw_stats)));

	REG_WR(sc, BAR_USTORM_INTMEM + USTORM_ETH_STATS_QUERY_ADDR_OFFSET(func),
	    U64_LO(BXE_SP_MAPPING(sc, fw_stats)));
	REG_WR(sc, BAR_USTORM_INTMEM +
	    USTORM_ETH_STATS_QUERY_ADDR_OFFSET(func) + 4,
	    U64_HI(BXE_SP_MAPPING(sc, fw_stats)));

	/* Additional initialization for 57711/57711E. */
	if (CHIP_IS_E1H(sc)) {
		REG_WR8(sc, BAR_XSTORM_INTMEM + XSTORM_FUNCTION_MODE_OFFSET,
		    IS_E1HMF(sc));
		REG_WR8(sc, BAR_TSTORM_INTMEM + TSTORM_FUNCTION_MODE_OFFSET,
		    IS_E1HMF(sc));
		REG_WR8(sc, BAR_CSTORM_INTMEM + CSTORM_FUNCTION_MODE_OFFSET,
		    IS_E1HMF(sc));
		REG_WR8(sc, BAR_USTORM_INTMEM + USTORM_FUNCTION_MODE_OFFSET,
		    IS_E1HMF(sc));

		/* Set the outer VLAN tag. */
		REG_WR16(sc, BAR_XSTORM_INTMEM + XSTORM_E1HOV_OFFSET(func),
		    sc->e1hov);
	}

	/* Init completion queue mapping and TPA aggregation size. */
	max_agg_size = min((uint32_t)(sc->rx_buf_size + 8 * BCM_PAGE_SIZE *
	    PAGES_PER_SGE), (uint32_t)0xffff);

	for (i = 0; i < sc->num_queues; i++) {
		fp = &sc->fp[i];
		nextpg = (struct eth_rx_cqe_next_page *)
		    &fp->rx_cq_chain[0][USABLE_RCQ_ENTRIES_PER_PAGE];

		/* Program the completion queue address. */
		REG_WR(sc, BAR_USTORM_INTMEM +
		    USTORM_CQE_PAGE_BASE_OFFSET(port, fp->cl_id),
		    U64_LO(fp->rx_cq_chain_paddr[0]));
		REG_WR(sc, BAR_USTORM_INTMEM +
		    USTORM_CQE_PAGE_BASE_OFFSET(port, fp->cl_id) + 4,
		    U64_HI(fp->rx_cq_chain_paddr[0]));

		/* Program the first CQ next page address. */
		REG_WR(sc, BAR_USTORM_INTMEM +
		    USTORM_CQE_PAGE_NEXT_OFFSET(port, fp->cl_id),
		    nextpg->addr_lo);
		REG_WR(sc, BAR_USTORM_INTMEM +
		    USTORM_CQE_PAGE_NEXT_OFFSET(port, fp->cl_id) + 4,
		    nextpg->addr_hi);

		/* Set the maximum TPA aggregation size. */
		REG_WR16(sc, BAR_USTORM_INTMEM +
		    USTORM_MAX_AGG_SIZE_OFFSET(port, fp->cl_id),
		    max_agg_size);
	}

	/* Configure lossless flow control. */
	if (CHIP_IS_E1H(sc)) {
		rx_pause.bd_thr_low = 250;
		rx_pause.cqe_thr_low = 250;
		rx_pause.cos = 1;
		rx_pause.sge_thr_low = 0;
		rx_pause.bd_thr_high = 350;
		rx_pause.cqe_thr_high = 350;
		rx_pause.sge_thr_high = 0;

		for (i = 0; i < sc->num_queues; i++) {
			fp = &sc->fp[i];
			if (!fp->disable_tpa) {
				rx_pause.sge_thr_low = 150;
				rx_pause.sge_thr_high = 250;
			}

			offset = BAR_USTORM_INTMEM +
			    USTORM_ETH_RING_PAUSE_DATA_OFFSET(port, fp->cl_id);

			for (j = 0; j <
			    sizeof(struct ustorm_eth_rx_pause_data_e1h) / 4;
			    j++)
				REG_WR(sc, offset + (j * 4),
				    ((uint32_t *)&rx_pause)[j]);
		}
	}

	memset(&(sc->cmng), 0, sizeof(struct cmng_struct_per_port));
	if (IS_E1HMF(sc)) {
		/*
		 * During init there is no active link.
		 * Until link is up, assume link rate @ 10Gbps
		 */
		bxe_read_mf_cfg(sc);

		if (!sc->vn_wsum)
			DBPRINT(sc, BXE_VERBOSE_MISC,
			    "%s(): All MIN values are zeroes, "
			    "fairness will be disabled.\n", __FUNCTION__);
	}

	/* Store it to internal memory */
	if (sc->port.pmf) {
		for (i = 0; i < sizeof(struct cmng_struct_per_port) / 4; i++)
			REG_WR(sc, BAR_XSTORM_INTMEM +
			    XSTORM_CMNG_PER_PORT_VARS_OFFSET(port) + i * 4,
			    ((uint32_t *)(&sc->cmng))[i]);
	}

	DBEXIT(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);
}

/*
 * Initialize internal resources.
 *
 * Returns:
 *   Nothing.
 */
static void
bxe_init_internal(struct bxe_softc *sc, uint32_t load_code)
{

	DBENTER(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);

	switch (load_code) {
	case FW_MSG_CODE_DRV_LOAD_COMMON:
		bxe_init_internal_common(sc);
		/* FALLTHROUGH */

	case FW_MSG_CODE_DRV_LOAD_PORT:
		bxe_init_internal_port(sc);
		/* FALLTHROUGH */

	case FW_MSG_CODE_DRV_LOAD_FUNCTION:
		bxe_init_internal_func(sc);
		break;

	default:
		BXE_PRINTF(
		    "%s(%d): Unknown load_code (0x%08X) from MCP!\n",
		    __FILE__, __LINE__, load_code);
		break;
	}

	DBEXIT(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);
}


/*
 * Perform driver instance specific initialization.
 *
 * Returns:
 *   None
 */
static void
bxe_init_nic(struct bxe_softc *sc, uint32_t load_code)
{
	struct bxe_fastpath *fp;
	int i;

	DBENTER(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);

	/* Intialize fastpath structures and the status block. */
	for (i = 0; i < sc->num_queues; i++) {
		fp = &sc->fp[i];
		fp->disable_tpa = 1;

		bzero((char *)fp->status_block, BXE_STATUS_BLK_SZ);
		fp->fp_u_idx = 0;
		fp->fp_c_idx = 0;

		/* Set a pointer back to the driver instance. */
		fp->sc = sc;

		/* Set the fastpath starting state as closed. */
		fp->state = BXE_FP_STATE_CLOSED;

		/* Self-reference to this fastpath's instance. */
		fp->index = i;

		/* Set the client ID beginning with the leading id. */
		fp->cl_id = BP_L_ID(sc) + i;

		/* Set the status block ID for this fastpath instance. */
		fp->sb_id = fp->cl_id;

		DBPRINT(sc, (BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET),
		    "%s(): fp[%d]: cl_id = %d, sb_id = %d\n",
		    __FUNCTION__, fp->index, fp->cl_id, fp->sb_id);

		/* Initialize the fastpath status block. */
		bxe_init_sb(sc, fp->status_block, fp->status_block_paddr,
		    fp->sb_id);
		bxe_update_fpsb_idx(fp);
	}

	rmb();

	bzero((char *)sc->def_status_block, BXE_DEF_STATUS_BLK_SZ);

	/* Initialize the Default Status Block. */
	bxe_init_def_sb(sc, sc->def_status_block, sc->def_status_block_paddr,
	    DEF_SB_ID);
	bxe_update_dsb_idx(sc);

	/* Initialize the coalescence parameters. */
	bxe_update_coalesce(sc);

	/* Intiialize the Receive BD Chain and Receive Completion Chain. */
	bxe_init_rx_chains(sc);

	/* Initialize the Transmit BD Chain. */
	bxe_init_tx_chains(sc);

	/* Initialize the Slow Path Chain. */
	bxe_init_sp_ring(sc);

	/* Initialize STORM processor context/configuration. */
	bxe_init_context(sc);

	/* Initialize the Context. */
	bxe_init_internal(sc, load_code);

	/* Enable indirection table for multi-queue operation. */
	bxe_init_ind_table(sc);

	mb();

	/* Disable the interrupts from device until init is complete.*/
	bxe_int_disable(sc);

	DBEXIT(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);
}

/*
*
* Returns:
*   0 = Success, !0 = Failure
*/
static int
bxe_gunzip_init(struct bxe_softc *sc)
{
	int rc;

	rc = 0;

	DBENTER(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);

	bxe_dmamem_alloc(sc, sc->gunzip_tag, sc->gunzip_map, sc->gunzip_buf,
	    FW_BUF_SIZE, &sc->gunzip_mapping);

	if (sc->gunzip_buf == NULL)
		goto bxe_gunzip_init_nomem1;

	sc->strm = malloc(sizeof(*sc->strm), M_DEVBUF, M_NOWAIT);
	if (sc->strm  == NULL)
		goto bxe_gunzip_init_nomem2;

	goto bxe_gunzip_init_exit;

bxe_gunzip_init_nomem2:
	bxe_dmamem_free(sc, sc->gunzip_tag, sc->gunzip_buf, sc->gunzip_map);
	sc->gunzip_buf = NULL;

bxe_gunzip_init_nomem1:
	BXE_PRINTF(
	    "%s(%d): Cannot allocate firmware buffer for decompression!\n",
	    __FILE__, __LINE__);
	rc = ENOMEM;

bxe_gunzip_init_exit:
	DBEXIT(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);
	return (rc);
}

/*
 * Send a loopback packet through the Network Interface Glue (NIG) block.
 *
 * Returns:
 *   None.
 */
static void
bxe_lb_pckt(struct bxe_softc *sc)
{
#ifdef USE_DMAE
	uint32_t wb_write[3];
#endif

	DBENTER(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);

	/* Ethernet source and destination addresses. */
#ifdef USE_DMAE
	wb_write[0] = 0x55555555;
	wb_write[1] = 0x55555555;
	wb_write[2] = 0x20;	/* SOP */
	REG_WR_DMAE(sc, NIG_REG_DEBUG_PACKET_LB, wb_write, 3);
#else
	REG_WR_IND(sc, NIG_REG_DEBUG_PACKET_LB, 0x55555555);
	REG_WR_IND(sc, NIG_REG_DEBUG_PACKET_LB + 4, 0x55555555);
	REG_WR_IND(sc, NIG_REG_DEBUG_PACKET_LB + 8, 0x20);
#endif

	/* NON-IP protocol. */
#ifdef USE_DMAE
	wb_write[0] = 0x09000000;
	wb_write[1] = 0x55555555;
	wb_write[2] = 0x10;	/* EOP */
	REG_WR_DMAE(sc, NIG_REG_DEBUG_PACKET_LB, wb_write, 3);
#else
	REG_WR_IND(sc, NIG_REG_DEBUG_PACKET_LB, 0x09000000);
	REG_WR_IND(sc, NIG_REG_DEBUG_PACKET_LB + 4, 0x55555555);
	REG_WR_IND(sc, NIG_REG_DEBUG_PACKET_LB + 8, 0x10);
#endif

	DBEXIT(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);
}

/*
 * Perform an internal memory test.
 *
 * Some internal memories are not accessible through the PCIe interface so
 * we send some debug packets for the test.
 *
 * Returns:
 *   0 = Success, !0 = Failure.
 */
static int
bxe_int_mem_test(struct bxe_softc *sc)
{
	uint32_t val;
	int count, i, rc;

	rc = 0;
	val = 0;

	DBENTER(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);

	/* Perform a single debug packet test. */

	/* Disable inputs of parser neighbor blocks. */
	REG_WR(sc, TSDM_REG_ENABLE_IN1, 0x0);
	REG_WR(sc, TCM_REG_PRS_IFEN, 0x0);
	REG_WR(sc, CFC_REG_DEBUG0, 0x1);
	REG_WR(sc, NIG_REG_PRS_REQ_IN_EN, 0x0);

	/*  Write 0 to parser credits for CFC search request. */
	REG_WR(sc, PRS_REG_CFC_SEARCH_INITIAL_CREDIT, 0x0);

	/* Send an Ethernet packet. */
	bxe_lb_pckt(sc);

	/* Wait until NIG register shows 1 packet of size 0x10. */
	count = 1000;
	while (count) {
		bxe_read_dmae(sc, NIG_REG_STAT2_BRB_OCTET, 2);
		val = *BXE_SP(sc, wb_data[0]);
		if (val == 0x10)
			break;

		DELAY(10000);
		count--;
	}

	if (val != 0x10) {
		DBPRINT(sc, BXE_FATAL,
		    "%s(): NIG loopback test 1 timeout (val = 0x%08X)!\n",
		    __FUNCTION__, val);
		rc = 1;
		goto bxe_int_mem_test_exit;
	}

	/* Wait until PRS register shows 1 packet */
	count = 1000;
	while (count) {
		val = REG_RD(sc, PRS_REG_NUM_OF_PACKETS);

		if (val == 1)
			break;

		DELAY(10000);
		count--;
	}

	if (val != 0x1) {
		DBPRINT(sc, BXE_FATAL,
		    "%s(): PRS loopback test 1 timeout (val = 0x%08X)!\n",
		    __FUNCTION__, val);
		rc = 2;
		goto bxe_int_mem_test_exit;
	}

	/* Reset and init BRB, PRS. */
	REG_WR(sc, GRCBASE_MISC + MISC_REGISTERS_RESET_REG_1_CLEAR, 0x3);
	DELAY(50000);
	REG_WR(sc, GRCBASE_MISC + MISC_REGISTERS_RESET_REG_1_SET, 0x3);
	DELAY(50000);
	bxe_init_block(sc, BRB1_BLOCK, COMMON_STAGE);
	bxe_init_block(sc, PRS_BLOCK, COMMON_STAGE);

	/* Perform the test again, this time with 10 packets. */

	/* Disable inputs of parser neighbor blocks. */
	REG_WR(sc, TSDM_REG_ENABLE_IN1, 0x0);
	REG_WR(sc, TCM_REG_PRS_IFEN, 0x0);
	REG_WR(sc, CFC_REG_DEBUG0, 0x1);
	REG_WR(sc, NIG_REG_PRS_REQ_IN_EN, 0x0);

	/* Write 0 to parser credits for CFC search request. */
	REG_WR(sc, PRS_REG_CFC_SEARCH_INITIAL_CREDIT, 0x0);

	/* Send 10 Ethernet packets. */
	for (i = 0; i < 10; i++)
		bxe_lb_pckt(sc);

	/* Wait until NIG shows 10 + 1 packets of size 11 * 0x10 = 0xb0. */
	count = 1000;
	while (count) {
		bxe_read_dmae(sc, NIG_REG_STAT2_BRB_OCTET, 2);
		val = *BXE_SP(sc, wb_data[0]);
		if (val == 0xb0)
			break;

		DELAY(10000);
		count--;
	}

	if (val != 0xb0) {
		DBPRINT(sc, BXE_FATAL,
		    "%s(): NIG loopback test 2 timeout (val = 0x%08X)!\n",
		    __FUNCTION__, val);
		rc = 3;
		goto bxe_int_mem_test_exit;
	}

	/* Wait until PRS register shows 2 packets. */
	val = REG_RD(sc, PRS_REG_NUM_OF_PACKETS);
	if (val != 2) {
		DBPRINT(sc, BXE_FATAL,
		    "%s(): PRS loopback test 2 timeout (val = 0x%x)!\n",
		    __FUNCTION__, val);
		rc = 4;
		goto bxe_int_mem_test_exit;
	}

	/* Write 1 to parser credits for CFC search request. */
	REG_WR(sc, PRS_REG_CFC_SEARCH_INITIAL_CREDIT, 0x1);

	/* Wait until PRS register shows 3 packets. */
	DELAY(10000);

	/* Wait until NIG register shows 1 packet of size 0x10. */
	val = REG_RD(sc, PRS_REG_NUM_OF_PACKETS);
	if (val != 3) {
		DBPRINT(sc, BXE_FATAL,
		    "%s(): PRS loopback test 3 timeout (val = 0x%08X)!\n",
		    __FUNCTION__, val);
		rc = 5;
		goto bxe_int_mem_test_exit;
	}

	/* Clear NIG end-of-packet FIFO. */
	for (i = 0; i < 11; i++)
		REG_RD(sc, NIG_REG_INGRESS_EOP_LB_FIFO);

	val = REG_RD(sc, NIG_REG_INGRESS_EOP_LB_EMPTY);
	if (val != 1) {
		DBPRINT(sc, BXE_INFO, "clear of NIG failed\n");
		rc = 6;
		goto bxe_int_mem_test_exit;
	}

	/* Reset and init BRB, PRS, NIG. */
	REG_WR(sc, GRCBASE_MISC + MISC_REGISTERS_RESET_REG_1_CLEAR, 0x03);
	DELAY(50000);
	REG_WR(sc, GRCBASE_MISC + MISC_REGISTERS_RESET_REG_1_SET, 0x03);
	DELAY(50000);
	bxe_init_block(sc, BRB1_BLOCK, COMMON_STAGE);
	bxe_init_block(sc, PRS_BLOCK, COMMON_STAGE);

	/* Set NIC mode. */
	REG_WR(sc, PRS_REG_NIC_MODE, 1);

	/* Enable inputs of parser neighbor blocks. */
	REG_WR(sc, TSDM_REG_ENABLE_IN1, 0x7fffffff);
	REG_WR(sc, TCM_REG_PRS_IFEN, 0x1);
	REG_WR(sc, CFC_REG_DEBUG0, 0x0);
	REG_WR(sc, NIG_REG_PRS_REQ_IN_EN, 0x1);

bxe_int_mem_test_exit:
	DBEXIT(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);
	return (rc);
}

/*
 * Enable attentions from various blocks.
 *
 * Returns:
 *   None.
 */
static void
bxe_enable_blocks_attention(struct bxe_softc *sc)
{

	DBENTER(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);

	REG_WR(sc, PXP_REG_PXP_INT_MASK_0, 0);
	REG_WR(sc, PXP_REG_PXP_INT_MASK_1, 0);
	REG_WR(sc, DORQ_REG_DORQ_INT_MASK, 0);
	REG_WR(sc, CFC_REG_CFC_INT_MASK, 0);
	REG_WR(sc, QM_REG_QM_INT_MASK, 0);
	REG_WR(sc, TM_REG_TM_INT_MASK, 0);
	REG_WR(sc, XSDM_REG_XSDM_INT_MASK_0, 0);
	REG_WR(sc, XSDM_REG_XSDM_INT_MASK_1, 0);
	REG_WR(sc, XCM_REG_XCM_INT_MASK, 0);

	REG_WR(sc, USDM_REG_USDM_INT_MASK_0, 0);
	REG_WR(sc, USDM_REG_USDM_INT_MASK_1, 0);
	REG_WR(sc, UCM_REG_UCM_INT_MASK, 0);

	REG_WR(sc, GRCBASE_UPB + PB_REG_PB_INT_MASK, 0);
	REG_WR(sc, CSDM_REG_CSDM_INT_MASK_0, 0);
	REG_WR(sc, CSDM_REG_CSDM_INT_MASK_1, 0);
	REG_WR(sc, CCM_REG_CCM_INT_MASK, 0);

	REG_WR(sc, PXP2_REG_PXP2_INT_MASK_0, 0x480000);

	REG_WR(sc, TSDM_REG_TSDM_INT_MASK_0, 0);
	REG_WR(sc, TSDM_REG_TSDM_INT_MASK_1, 0);
	REG_WR(sc, TCM_REG_TCM_INT_MASK, 0);

	REG_WR(sc, CDU_REG_CDU_INT_MASK, 0);
	REG_WR(sc, DMAE_REG_DMAE_INT_MASK, 0);
	REG_WR(sc, PBF_REG_PBF_INT_MASK, 0X18);

	DBEXIT(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);
}

/*
 * PXP Arbiter
 */

/*
 * This code configures the PCI read/write arbiter
 * which implements a weighted round robin
 * between the virtual queues in the chip.
 *
 * The values were derived for each PCI max payload and max request size.
 * since max payload and max request size are only known at run time,
 * this is done as a separate init stage.
 */

#define	NUM_WR_Q			13
#define	NUM_RD_Q			29
#define	MAX_RD_ORD			3
#define	MAX_WR_ORD			2

/* Configuration for one arbiter queue. */
struct arb_line {
	int l;
	int add;
	int ubound;
};

/* Derived configuration for each read queue for each max request size. */
static const struct arb_line read_arb_data[NUM_RD_Q][MAX_RD_ORD + 1] = {
/* 1 */	{ {8, 64, 25}, {16, 64, 25}, {32, 64, 25}, {64, 64, 41} },
	{ {4, 8,  4},  {4,  8,  4},  {4,  8,  4},  {4,  8,  4}  },
	{ {4, 3,  3},  {4,  3,  3},  {4,  3,  3},  {4,  3,  3}  },
	{ {8, 3,  6},  {16, 3,  11}, {16, 3,  11}, {16, 3,  11} },
	{ {8, 64, 25}, {16, 64, 25}, {32, 64, 25}, {64, 64, 41} },
	{ {8, 3,  6},  {16, 3,  11}, {32, 3,  21}, {64, 3,  41} },
	{ {8, 3,  6},  {16, 3,  11}, {32, 3,  21}, {64, 3,  41} },
	{ {8, 3,  6},  {16, 3,  11}, {32, 3,  21}, {64, 3,  41} },
	{ {8, 3,  6},  {16, 3,  11}, {32, 3,  21}, {64, 3,  41} },
/* 10 */{ {8, 3,  6},  {16, 3,  11}, {32, 3,  21}, {32, 3,  21} },
	{ {8, 3,  6},  {16, 3,  11}, {32, 3,  21}, {32, 3,  21} },
	{ {8, 3,  6},  {16, 3,  11}, {32, 3,  21}, {32, 3,  21} },
	{ {8, 3,  6},  {16, 3,  11}, {32, 3,  21}, {32, 3,  21} },
	{ {8, 3,  6},  {16, 3,  11}, {32, 3,  21}, {32, 3,  21} },
	{ {8, 3,  6},  {16, 3,  11}, {32, 3,  21}, {32, 3,  21} },
	{ {8, 3,  6},  {16, 3,  11}, {32, 3,  21}, {32, 3,  21} },
	{ {8, 64, 6},  {16, 64, 11}, {32, 64, 21}, {32, 64, 21} },
	{ {8, 3,  6},  {16, 3,  11}, {32, 3,  21}, {32, 3,  21} },
	{ {8, 3,  6},  {16, 3,  11}, {32, 3,  21}, {32, 3,  21} },
/* 20 */{ {8, 3,  6},  {16, 3,  11}, {32, 3,  21}, {32, 3,  21} },
	{ {8, 3,  6},  {16, 3,  11}, {32, 3,  21}, {32, 3,  21} },
	{ {8, 3,  6},  {16, 3,  11}, {32, 3,  21}, {32, 3,  21} },
	{ {8, 3,  6},  {16, 3,  11}, {32, 3,  21}, {32, 3,  21} },
	{ {8, 3,  6},  {16, 3,  11}, {32, 3,  21}, {32, 3,  21} },
	{ {8, 3,  6},  {16, 3,  11}, {32, 3,  21}, {32, 3,  21} },
	{ {8, 3,  6},  {16, 3,  11}, {32, 3,  21}, {32, 3,  21} },
	{ {8, 3,  6},  {16, 3,  11}, {32, 3,  21}, {32, 3,  21} },
	{ {8, 3,  6},  {16, 3,  11}, {32, 3,  21}, {32, 3,  21} },
	{ {8, 64, 25}, {16, 64, 41}, {32, 64, 81}, {64, 64, 120} }
};

/* Derived configuration for each write queue for each max request size. */
static const struct arb_line write_arb_data[NUM_WR_Q][MAX_WR_ORD + 1] = {
/* 1 */	{ {4, 6,  3},  {4,  6,  3},  {4,  6,  3} },
	{ {4, 2,  3},  {4,  2,  3},  {4,  2,  3} },
	{ {8, 2,  6},  {16, 2,  11}, {16, 2,  11} },
	{ {8, 2,  6},  {16, 2,  11}, {32, 2,  21} },
	{ {8, 2,  6},  {16, 2,  11}, {32, 2,  21} },
	{ {8, 2,  6},  {16, 2,  11}, {32, 2,  21} },
	{ {8, 64, 25}, {16, 64, 25}, {32, 64, 25} },
	{ {8, 2,  6},  {16, 2,  11}, {16, 2,  11} },
	{ {8, 2,  6},  {16, 2,  11}, {16, 2,  11} },
/* 10 */{ {8, 9,  6},  {16, 9,  11}, {32, 9,  21} },
	{ {8, 47, 19}, {16, 47, 19}, {32, 47, 21} },
	{ {8, 9,  6},  {16, 9,  11}, {16, 9,  11} },
	{ {8, 64, 25}, {16, 64, 41}, {32, 64, 81} }
};

/* Register addresses for read queues. */
static const struct arb_line read_arb_addr[NUM_RD_Q-1] = {
/* 1 */	{PXP2_REG_RQ_BW_RD_L0, PXP2_REG_RQ_BW_RD_ADD0,
	    PXP2_REG_RQ_BW_RD_UBOUND0},
	{PXP2_REG_PSWRQ_BW_L1, PXP2_REG_PSWRQ_BW_ADD1,
	    PXP2_REG_PSWRQ_BW_UB1},
	{PXP2_REG_PSWRQ_BW_L2, PXP2_REG_PSWRQ_BW_ADD2,
	    PXP2_REG_PSWRQ_BW_UB2},
	{PXP2_REG_PSWRQ_BW_L3, PXP2_REG_PSWRQ_BW_ADD3,
	    PXP2_REG_PSWRQ_BW_UB3},
	{PXP2_REG_RQ_BW_RD_L4, PXP2_REG_RQ_BW_RD_ADD4,
	    PXP2_REG_RQ_BW_RD_UBOUND4},
	{PXP2_REG_RQ_BW_RD_L5, PXP2_REG_RQ_BW_RD_ADD5,
	    PXP2_REG_RQ_BW_RD_UBOUND5},
	{PXP2_REG_PSWRQ_BW_L6, PXP2_REG_PSWRQ_BW_ADD6,
	    PXP2_REG_PSWRQ_BW_UB6},
	{PXP2_REG_PSWRQ_BW_L7, PXP2_REG_PSWRQ_BW_ADD7,
	    PXP2_REG_PSWRQ_BW_UB7},
	{PXP2_REG_PSWRQ_BW_L8, PXP2_REG_PSWRQ_BW_ADD8,
	    PXP2_REG_PSWRQ_BW_UB8},
/* 10 */{PXP2_REG_PSWRQ_BW_L9, PXP2_REG_PSWRQ_BW_ADD9,
	    PXP2_REG_PSWRQ_BW_UB9},
	{PXP2_REG_PSWRQ_BW_L10, PXP2_REG_PSWRQ_BW_ADD10,
	    PXP2_REG_PSWRQ_BW_UB10},
	{PXP2_REG_PSWRQ_BW_L11, PXP2_REG_PSWRQ_BW_ADD11,
	    PXP2_REG_PSWRQ_BW_UB11},
	{PXP2_REG_RQ_BW_RD_L12, PXP2_REG_RQ_BW_RD_ADD12,
	    PXP2_REG_RQ_BW_RD_UBOUND12},
	{PXP2_REG_RQ_BW_RD_L13, PXP2_REG_RQ_BW_RD_ADD13,
	    PXP2_REG_RQ_BW_RD_UBOUND13},
	{PXP2_REG_RQ_BW_RD_L14, PXP2_REG_RQ_BW_RD_ADD14,
	    PXP2_REG_RQ_BW_RD_UBOUND14},
	{PXP2_REG_RQ_BW_RD_L15, PXP2_REG_RQ_BW_RD_ADD15,
	    PXP2_REG_RQ_BW_RD_UBOUND15},
	{PXP2_REG_RQ_BW_RD_L16, PXP2_REG_RQ_BW_RD_ADD16,
	    PXP2_REG_RQ_BW_RD_UBOUND16},
	{PXP2_REG_RQ_BW_RD_L17, PXP2_REG_RQ_BW_RD_ADD17,
	    PXP2_REG_RQ_BW_RD_UBOUND17},
	{PXP2_REG_RQ_BW_RD_L18, PXP2_REG_RQ_BW_RD_ADD18,
	    PXP2_REG_RQ_BW_RD_UBOUND18},
/* 20 */{PXP2_REG_RQ_BW_RD_L19, PXP2_REG_RQ_BW_RD_ADD19,
	    PXP2_REG_RQ_BW_RD_UBOUND19},
	{PXP2_REG_RQ_BW_RD_L20, PXP2_REG_RQ_BW_RD_ADD20,
	    PXP2_REG_RQ_BW_RD_UBOUND20},
	{PXP2_REG_RQ_BW_RD_L22, PXP2_REG_RQ_BW_RD_ADD22,
	    PXP2_REG_RQ_BW_RD_UBOUND22},
	{PXP2_REG_RQ_BW_RD_L23, PXP2_REG_RQ_BW_RD_ADD23,
	    PXP2_REG_RQ_BW_RD_UBOUND23},
	{PXP2_REG_RQ_BW_RD_L24, PXP2_REG_RQ_BW_RD_ADD24,
	    PXP2_REG_RQ_BW_RD_UBOUND24},
	{PXP2_REG_RQ_BW_RD_L25, PXP2_REG_RQ_BW_RD_ADD25,
	    PXP2_REG_RQ_BW_RD_UBOUND25},
	{PXP2_REG_RQ_BW_RD_L26, PXP2_REG_RQ_BW_RD_ADD26,
	    PXP2_REG_RQ_BW_RD_UBOUND26},
	{PXP2_REG_RQ_BW_RD_L27, PXP2_REG_RQ_BW_RD_ADD27,
	    PXP2_REG_RQ_BW_RD_UBOUND27},
	{PXP2_REG_PSWRQ_BW_L28, PXP2_REG_PSWRQ_BW_ADD28,
	    PXP2_REG_PSWRQ_BW_UB28}
};

/* Register addresses for write queues. */
static const struct arb_line write_arb_addr[NUM_WR_Q-1] = {
/* 1 */	{PXP2_REG_PSWRQ_BW_L1, PXP2_REG_PSWRQ_BW_ADD1,
	    PXP2_REG_PSWRQ_BW_UB1},
	{PXP2_REG_PSWRQ_BW_L2, PXP2_REG_PSWRQ_BW_ADD2,
	    PXP2_REG_PSWRQ_BW_UB2},
	{PXP2_REG_PSWRQ_BW_L3, PXP2_REG_PSWRQ_BW_ADD3,
	    PXP2_REG_PSWRQ_BW_UB3},
	{PXP2_REG_PSWRQ_BW_L6, PXP2_REG_PSWRQ_BW_ADD6,
	    PXP2_REG_PSWRQ_BW_UB6},
	{PXP2_REG_PSWRQ_BW_L7, PXP2_REG_PSWRQ_BW_ADD7,
	    PXP2_REG_PSWRQ_BW_UB7},
	{PXP2_REG_PSWRQ_BW_L8, PXP2_REG_PSWRQ_BW_ADD8,
	    PXP2_REG_PSWRQ_BW_UB8},
	{PXP2_REG_PSWRQ_BW_L9, PXP2_REG_PSWRQ_BW_ADD9,
	    PXP2_REG_PSWRQ_BW_UB9},
	{PXP2_REG_PSWRQ_BW_L10, PXP2_REG_PSWRQ_BW_ADD10,
	    PXP2_REG_PSWRQ_BW_UB10},
	{PXP2_REG_PSWRQ_BW_L11, PXP2_REG_PSWRQ_BW_ADD11,
	    PXP2_REG_PSWRQ_BW_UB11},
/* 10 */{PXP2_REG_PSWRQ_BW_L28, PXP2_REG_PSWRQ_BW_ADD28,
	    PXP2_REG_PSWRQ_BW_UB28},
	{PXP2_REG_RQ_BW_WR_L29, PXP2_REG_RQ_BW_WR_ADD29,
	    PXP2_REG_RQ_BW_WR_UBOUND29},
	{PXP2_REG_RQ_BW_WR_L30, PXP2_REG_RQ_BW_WR_ADD30,
	    PXP2_REG_RQ_BW_WR_UBOUND30}
};

static void
bxe_init_pxp_arb(struct bxe_softc *sc, int r_order, int w_order)
{
	uint32_t val, i;

	if (r_order > MAX_RD_ORD) {
		DBPRINT(sc, (BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET),
		    "%s(): Read order of %d order adjusted to %d\n",
		    __FUNCTION__,  r_order, MAX_RD_ORD);
		r_order = MAX_RD_ORD;
	}
	if (w_order > MAX_WR_ORD) {
		DBPRINT(sc, (BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET),
		    "%s(): Write order of %d order adjusted to %d\n",
		    __FUNCTION__, w_order, MAX_WR_ORD);
		w_order = MAX_WR_ORD;
	}

	DBPRINT(sc, (BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET),
	    "%s(): Read order %d, write order %d\n",
	    __FUNCTION__, r_order, w_order);

	for (i = 0; i < NUM_RD_Q - 1; i++) {
		REG_WR(sc, read_arb_addr[i].l,
		    read_arb_data[i][r_order].l);
		REG_WR(sc, read_arb_addr[i].add,
		    read_arb_data[i][r_order].add);
		REG_WR(sc, read_arb_addr[i].ubound,
		    read_arb_data[i][r_order].ubound);
	}

	for (i = 0; i < NUM_WR_Q - 1; i++) {
		if ((write_arb_addr[i].l == PXP2_REG_RQ_BW_WR_L29) ||
		    (write_arb_addr[i].l == PXP2_REG_RQ_BW_WR_L30)) {

			REG_WR(sc, write_arb_addr[i].l,
			    write_arb_data[i][w_order].l);

			REG_WR(sc, write_arb_addr[i].add,
			    write_arb_data[i][w_order].add);

			REG_WR(sc, write_arb_addr[i].ubound,
			    write_arb_data[i][w_order].ubound);
		} else {

			val = REG_RD(sc, write_arb_addr[i].l);
			REG_WR(sc, write_arb_addr[i].l, val |
			    (write_arb_data[i][w_order].l << 10));

			val = REG_RD(sc, write_arb_addr[i].add);
			REG_WR(sc, write_arb_addr[i].add, val |
			    (write_arb_data[i][w_order].add << 10));

			val = REG_RD(sc, write_arb_addr[i].ubound);
			REG_WR(sc, write_arb_addr[i].ubound, val |
			    (write_arb_data[i][w_order].ubound << 7));
		}
	}

	val =  write_arb_data[NUM_WR_Q - 1][w_order].add;
	val += write_arb_data[NUM_WR_Q - 1][w_order].ubound << 10;
	val += write_arb_data[NUM_WR_Q - 1][w_order].l << 17;
	REG_WR(sc, PXP2_REG_PSWRQ_BW_RD, val);

	val =  read_arb_data[NUM_RD_Q - 1][r_order].add;
	val += read_arb_data[NUM_RD_Q - 1][r_order].ubound << 10;
	val += read_arb_data[NUM_RD_Q - 1][r_order].l << 17;
	REG_WR(sc, PXP2_REG_PSWRQ_BW_WR, val);

	REG_WR(sc, PXP2_REG_RQ_WR_MBS0, w_order);
	REG_WR(sc, PXP2_REG_RQ_WR_MBS1, w_order);
	REG_WR(sc, PXP2_REG_RQ_RD_MBS0, r_order);
	REG_WR(sc, PXP2_REG_RQ_RD_MBS1, r_order);

	if (r_order == MAX_RD_ORD)
		REG_WR(sc, PXP2_REG_RQ_PDR_LIMIT, 0xe00);

	REG_WR(sc, PXP2_REG_WR_USDMDP_TH, (0x18 << w_order));

	if (CHIP_IS_E1H(sc)) {
		/*    MPS      w_order     optimal TH      presently TH
		 *    128         0             0               2
		 *    256         1             1               3
		 *    >=512       2             2               3
		 */
		val = ((w_order == 0) ? 2 : 3);
		REG_WR(sc, PXP2_REG_WR_HC_MPS, val);
		REG_WR(sc, PXP2_REG_WR_USDM_MPS, val);
		REG_WR(sc, PXP2_REG_WR_CSDM_MPS, val);
		REG_WR(sc, PXP2_REG_WR_TSDM_MPS, val);
		REG_WR(sc, PXP2_REG_WR_XSDM_MPS, val);
		REG_WR(sc, PXP2_REG_WR_QM_MPS, val);
		REG_WR(sc, PXP2_REG_WR_TM_MPS, val);
		REG_WR(sc, PXP2_REG_WR_SRC_MPS, val);
		REG_WR(sc, PXP2_REG_WR_DBG_MPS, val);
		REG_WR(sc, PXP2_REG_WR_DMAE_MPS, 2); /* DMAE is special */
		REG_WR(sc, PXP2_REG_WR_CDU_MPS, val);
	}
}

static void
bxe_init_pxp(struct bxe_softc *sc)
{
	uint16_t devctl;
	int r_order, w_order;

	devctl = pci_read_config(sc->dev,
	    sc->pcie_cap + PCI_EXP_DEVCTL, 2);
	DBPRINT(sc, (BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET),
	    "%s(): Read 0x%x from devctl\n", __FUNCTION__, devctl);
	w_order = ((devctl & PCI_EXP_DEVCTL_PAYLOAD) >> 5);
	if (sc->mrrs == -1)
		r_order = ((devctl & PCI_EXP_DEVCTL_READRQ) >> 12);
	else {
		DBPRINT(sc, (BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET),
		    "%s(): Force MRRS read order to %d\n",
		    __FUNCTION__, sc->mrrs);
		r_order = sc->mrrs;
	}

	bxe_init_pxp_arb(sc, r_order, w_order);
}

static void
bxe_setup_fan_failure_detection(struct bxe_softc *sc)
{
	uint32_t phy_type, val;
	int is_required, port;

	is_required = 0;
	if (BP_NOMCP(sc))
		return;

	val = SHMEM_RD(sc, dev_info.shared_hw_config.config2) &
	    SHARED_HW_CFG_FAN_FAILURE_MASK;

	if (val == SHARED_HW_CFG_FAN_FAILURE_ENABLED)
		is_required = 1;

	/*
	 * The fan failure mechanism is usually related to the PHY type since
	 * the power consumption of the board is affected by the PHY. Currently,
	 * fan is required for most designs with SFX7101, BCM8727 and BCM8481.
	 */
	else if (val == SHARED_HW_CFG_FAN_FAILURE_PHY_TYPE)
		for (port = PORT_0; port < PORT_MAX; port++) {
			phy_type = SHMEM_RD(sc,
			    dev_info.port_hw_config[port].external_phy_config) &
			    PORT_HW_CFG_XGXS_EXT_PHY_TYPE_MASK;
			is_required |=
			((phy_type == PORT_HW_CFG_XGXS_EXT_PHY_TYPE_SFX7101) ||
			 (phy_type == PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727) ||
			 (phy_type == PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8481));
		}

	if (is_required == 0)
		return;

	/* Fan failure is indicated by SPIO 5. */
	bxe_set_spio(sc, MISC_REGISTERS_SPIO_5, MISC_REGISTERS_SPIO_INPUT_HI_Z);

	/* Set to active low mode. */
	val = REG_RD(sc, MISC_REG_SPIO_INT);
	val |= ((1 << MISC_REGISTERS_SPIO_5) <<
					MISC_REGISTERS_SPIO_INT_OLD_SET_POS);
	REG_WR(sc, MISC_REG_SPIO_INT, val);

	/* Enable interrupt to signal the IGU. */
	val = REG_RD(sc, MISC_REG_SPIO_EVENT_EN);
	val |= (1 << MISC_REGISTERS_SPIO_5);
	REG_WR(sc, MISC_REG_SPIO_EVENT_EN, val);
}

/*
 * Common initialization.
 *
 * Returns:
 *   0 = Success, !0 = Failure.
 */
static int
bxe_init_common(struct bxe_softc *sc)
{
	uint32_t val;
	int i, rc;

	rc = 0;
	DBENTER(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);

	/* Reset all blocks within the chip except the BMAC. */
	bxe_reset_common(sc);
	DELAY(30000);
	REG_WR(sc, GRCBASE_MISC + MISC_REGISTERS_RESET_REG_1_SET, 0xffffffff);
	REG_WR(sc, GRCBASE_MISC + MISC_REGISTERS_RESET_REG_2_SET, 0xfffc);
	DELAY(30000);

	bxe_init_block(sc, MISC_BLOCK, COMMON_STAGE);
	if (CHIP_IS_E1H(sc))
		REG_WR(sc, MISC_REG_E1HMF_MODE, IS_E1HMF(sc));

	REG_WR(sc, MISC_REG_LCPLL_CTRL_REG_2, 0x100);
	DELAY(30000);
	REG_WR(sc, MISC_REG_LCPLL_CTRL_REG_2, 0x0);

	bxe_init_block(sc, PXP_BLOCK, COMMON_STAGE);
	if (CHIP_IS_E1(sc)) {
		/*
		 * Enable HW interrupt from PXP on USDM overflow
		 * bit 16 on INT_MASK_0.
		 */
		REG_WR(sc, PXP_REG_PXP_INT_MASK_0, 0);
	}

	bxe_init_block(sc, PXP2_BLOCK, COMMON_STAGE);
	bxe_init_pxp(sc);

#ifdef __BIG_ENDIAN
	REG_WR(sc, PXP2_REG_RQ_QM_ENDIAN_M, 1);
	REG_WR(sc, PXP2_REG_RQ_TM_ENDIAN_M, 1);
	REG_WR(sc, PXP2_REG_RQ_SRC_ENDIAN_M, 1);
	REG_WR(sc, PXP2_REG_RQ_CDU_ENDIAN_M, 1);
	REG_WR(sc, PXP2_REG_RQ_DBG_ENDIAN_M, 1);
	/* Make sure this value is 0. */
	REG_WR(sc, PXP2_REG_RQ_HC_ENDIAN_M, 0);

	REG_WR(sc, PXP2_REG_RD_QM_SWAP_MODE, 1);
	REG_WR(sc, PXP2_REG_RD_TM_SWAP_MODE, 1);
	REG_WR(sc, PXP2_REG_RD_SRC_SWAP_MODE, 1);
	REG_WR(sc, PXP2_REG_RD_CDURD_SWAP_MODE, 1);
#endif

	REG_WR(sc, PXP2_REG_RQ_CDU_P_SIZE, 2);

	/* Let the HW do it's magic ... */
	DELAY(100000);
	/* Finish the PXP initialization. */
	val = REG_RD(sc, PXP2_REG_RQ_CFG_DONE);
	if (val != 1) {
		BXE_PRINTF("%s(%d): PXP2 CFG failed!\n", __FILE__, __LINE__);
		rc = EBUSY;
		goto bxe_init_common_exit;
	}

	val = REG_RD(sc, PXP2_REG_RD_INIT_DONE);
	if (val != 1) {
		BXE_PRINTF("%s(%d): PXP2 RD_INIT failed!\n", __FILE__,
		    __LINE__);
		rc = EBUSY;
		goto bxe_init_common_exit;
	}

	REG_WR(sc, PXP2_REG_RQ_DISABLE_INPUTS, 0);
	REG_WR(sc, PXP2_REG_RD_DISABLE_INPUTS, 0);

	bxe_init_block(sc, DMAE_BLOCK, COMMON_STAGE);

	sc->dmae_ready = 1;
	bxe_init_fill(sc, TSEM_REG_PRAM, 0, 8);

	bxe_init_block(sc, TCM_BLOCK, COMMON_STAGE);
	bxe_init_block(sc, UCM_BLOCK, COMMON_STAGE);
	bxe_init_block(sc, CCM_BLOCK, COMMON_STAGE);
	bxe_init_block(sc, XCM_BLOCK, COMMON_STAGE);

	bxe_read_dmae(sc, XSEM_REG_PASSIVE_BUFFER, 3);
	bxe_read_dmae(sc, CSEM_REG_PASSIVE_BUFFER, 3);
	bxe_read_dmae(sc, TSEM_REG_PASSIVE_BUFFER, 3);
	bxe_read_dmae(sc, USEM_REG_PASSIVE_BUFFER, 3);

	bxe_init_block(sc, QM_BLOCK, COMMON_STAGE);

	/* Soft reset pulse. */
	REG_WR(sc, QM_REG_SOFT_RESET, 1);
	REG_WR(sc, QM_REG_SOFT_RESET, 0);

	bxe_init_block(sc, DQ_BLOCK, COMMON_STAGE);
	REG_WR(sc, DORQ_REG_DPM_CID_OFST, BCM_PAGE_SHIFT);

	REG_WR(sc, DORQ_REG_DORQ_INT_MASK, 0);

	bxe_init_block(sc, BRB1_BLOCK, COMMON_STAGE);
	bxe_init_block(sc, PRS_BLOCK, COMMON_STAGE);
	REG_WR(sc, PRS_REG_A_PRSU_20, 0xf);

	if (CHIP_IS_E1H(sc))
		REG_WR(sc, PRS_REG_E1HOV_MODE, IS_E1HMF(sc));

	bxe_init_block(sc, TSDM_BLOCK, COMMON_STAGE);
	bxe_init_block(sc, CSDM_BLOCK, COMMON_STAGE);
	bxe_init_block(sc, USDM_BLOCK, COMMON_STAGE);
	bxe_init_block(sc, XSDM_BLOCK, COMMON_STAGE);
	/* Clear STORM processor memory. */
	bxe_init_fill(sc, TSEM_REG_FAST_MEMORY, 0, STORM_INTMEM_SIZE(sc));
	bxe_init_fill(sc, USEM_REG_FAST_MEMORY, 0, STORM_INTMEM_SIZE(sc));
	bxe_init_fill(sc, CSEM_REG_FAST_MEMORY, 0, STORM_INTMEM_SIZE(sc));
	bxe_init_fill(sc, XSEM_REG_FAST_MEMORY, 0, STORM_INTMEM_SIZE(sc));

	bxe_init_block(sc, TSEM_BLOCK, COMMON_STAGE);
	bxe_init_block(sc, USEM_BLOCK, COMMON_STAGE);
	bxe_init_block(sc, CSEM_BLOCK, COMMON_STAGE);
	bxe_init_block(sc, XSEM_BLOCK, COMMON_STAGE);

	/* Sync semi rtc. */
	REG_WR(sc, GRCBASE_MISC + MISC_REGISTERS_RESET_REG_1_CLEAR, 0x80000000);
	REG_WR(sc, GRCBASE_MISC + MISC_REGISTERS_RESET_REG_1_SET, 0x80000000);

	bxe_init_block(sc, UPB_BLOCK, COMMON_STAGE);
	bxe_init_block(sc, XPB_BLOCK, COMMON_STAGE);
	bxe_init_block(sc, PBF_BLOCK, COMMON_STAGE);

	REG_WR(sc, SRC_REG_SOFT_RST, 1);
	/* Setup RSS/multi-queue hasking keys. */
	for (i = SRC_REG_KEYRSS0_0; i <= SRC_REG_KEYRSS1_9; i += 4)
		REG_WR(sc, i, 0xc0cac01a);

	bxe_init_block(sc, SRCH_BLOCK, COMMON_STAGE);

	REG_WR(sc, SRC_REG_SOFT_RST, 0);

	/* Make sure the cdu_context structure has the right size. */
	if (sizeof(union cdu_context) != 1024) {
		BXE_PRINTF("%s(%d): Invalid size for context (%ld != 1024)!\n",
		    __FILE__, __LINE__, (long)sizeof(union cdu_context));
		rc = EBUSY;
		goto bxe_init_common_exit;
	}

	bxe_init_block(sc, CDU_BLOCK, COMMON_STAGE);

	/*
	 * val = (num_context_in_page << 24) +
	 * (context_waste_size << 12) +
	 * context_line_size.
	 */

	val = (4 << 24) + (0 << 12) + 1024;
	REG_WR(sc, CDU_REG_CDU_GLOBAL_PARAMS, val);

	bxe_init_block(sc, CFC_BLOCK, COMMON_STAGE);
	REG_WR(sc, CFC_REG_INIT_REG, 0x7FF);
	/* Enable context validation interrupt from CFC. */
	REG_WR(sc, CFC_REG_CFC_INT_MASK, 0);

	/* Set the thresholds to prevent CFC/CDU race. */
	REG_WR(sc, CFC_REG_DEBUG0, 0x20020000);

	bxe_init_block(sc, HC_BLOCK, COMMON_STAGE);
	bxe_init_block(sc, MISC_AEU_BLOCK, COMMON_STAGE);

	bxe_init_block(sc, PXPCS_BLOCK, COMMON_STAGE);
	/* Clear PCIe block debug status bits. */
	REG_WR(sc, 0x2814, 0xffffffff);
	REG_WR(sc, 0x3820, 0xffffffff);

	bxe_init_block(sc, EMAC0_BLOCK, COMMON_STAGE);
	bxe_init_block(sc, EMAC1_BLOCK, COMMON_STAGE);
	bxe_init_block(sc, DBU_BLOCK, COMMON_STAGE);
	bxe_init_block(sc, DBG_BLOCK, COMMON_STAGE);

	bxe_init_block(sc, NIG_BLOCK, COMMON_STAGE);
	if (CHIP_IS_E1H(sc)) {
		REG_WR(sc, NIG_REG_LLH_MF_MODE, IS_E1HMF(sc));
		REG_WR(sc, NIG_REG_LLH_E1HOV_MODE, IS_E1HOV(sc));
	}

	/* Finish CFC initialization. */
	val = bxe_reg_poll(sc, CFC_REG_LL_INIT_DONE, 1, 100, 10);
	if (val != 1) {
		BXE_PRINTF("%s(%d): CFC LL_INIT failed!\n",
		    __FILE__, __LINE__);
		rc = EBUSY;
		goto bxe_init_common_exit;
	}

	val = bxe_reg_poll(sc, CFC_REG_AC_INIT_DONE, 1, 100, 10);
	if (val != 1) {
		BXE_PRINTF("%s(%d): CFC AC_INIT failed!\n",
		     __FILE__, __LINE__);
		rc = EBUSY;
		goto bxe_init_common_exit;
	}

	val = bxe_reg_poll(sc, CFC_REG_CAM_INIT_DONE, 1, 100, 10);
	if (val != 1) {
		BXE_PRINTF("%s(%d): CFC CAM_INIT failed!\n",
		    __FILE__, __LINE__);
		rc = EBUSY;
		goto bxe_init_common_exit;
	}

	REG_WR(sc, CFC_REG_DEBUG0, 0);

	/* Read NIG statistic and check for first load since powerup. */
 	bxe_read_dmae(sc, NIG_REG_STAT2_BRB_OCTET, 2);
 	val = *BXE_SP(sc, wb_data[0]);

 	/* Do internal memory self test only after a full power cycle. */
 	if ((CHIP_IS_E1(sc)) && (val == 0) && bxe_int_mem_test(sc)) {
		BXE_PRINTF("%s(%d): Internal memory self-test failed!\n",
		    __FILE__, __LINE__);
		rc = EBUSY;
		goto bxe_init_common_exit;
	}

	/* Handle any board specific initialization. */
	switch (XGXS_EXT_PHY_TYPE(sc->link_params.ext_phy_config)) {
	case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8072:
	case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8073:
	case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8726:
	case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727:
		break;

	default:
		break;
	}

	bxe_setup_fan_failure_detection(sc);

	/* Clear PXP2 attentions. */
	REG_RD(sc, PXP2_REG_PXP2_INT_STS_CLR_0);

	bxe_enable_blocks_attention(sc);

	if (!BP_NOMCP(sc)) {
		bxe_acquire_phy_lock(sc);
		bxe_common_init_phy(sc, sc->common.shmem_base);
		bxe_release_phy_lock(sc);
	} else
		BXE_PRINTF(
		    "%s(%d): Bootcode is missing - cannot initialize PHY!\n",
		    __FILE__, __LINE__);

bxe_init_common_exit:
	DBEXIT(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);
	return (rc);
}

/*
 * Port initialization.
 *
 * Returns:
 *   0 = Success, !0 = Failure.
 */
static int
bxe_init_port(struct bxe_softc *sc)
{
	uint32_t val, low, high;
	uint32_t swap_val, swap_override, aeu_gpio_mask, offset;
	uint32_t reg_addr;
	int i, init_stage, port;

	port = BP_PORT(sc);
	init_stage = port ? PORT1_STAGE : PORT0_STAGE;

	DBENTER(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);

	DBPRINT(sc, (BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET),
	    "%s(): Initializing port %d.\n", __FUNCTION__, port);

	REG_WR(sc, NIG_REG_MASK_INTERRUPT_PORT0 + port * 4, 0);

	bxe_init_block(sc, PXP_BLOCK, init_stage);
	bxe_init_block(sc, PXP2_BLOCK, init_stage);

	bxe_init_block(sc, TCM_BLOCK, init_stage);
	bxe_init_block(sc, UCM_BLOCK, init_stage);
	bxe_init_block(sc, CCM_BLOCK, init_stage);
	bxe_init_block(sc, XCM_BLOCK, init_stage);

	bxe_init_block(sc, DQ_BLOCK, init_stage);

	bxe_init_block(sc, BRB1_BLOCK, init_stage);

	/* Determine the pause threshold for the BRB */
	if (IS_E1HMF(sc))
		low = (sc->bxe_flags & BXE_ONE_PORT_FLAG) ? 160 : 246;
	else if (sc->bxe_ifp->if_mtu > 4096) {
		if (sc->bxe_flags & BXE_ONE_PORT_FLAG)
			low = 160;
		else {
			val = sc->bxe_ifp->if_mtu;
			/* (24*1024 + val*4)/256 */
			low = 96 + (val/64) + ((val % 64) ? 1 : 0);
		}
	} else
		low = (sc->bxe_flags & BXE_ONE_PORT_FLAG) ? 80 : 160;
	high = low + 56;	/* 14 * 1024 / 256 */

	REG_WR(sc, BRB1_REG_PAUSE_LOW_THRESHOLD_0 + port * 4, low);
	REG_WR(sc, BRB1_REG_PAUSE_HIGH_THRESHOLD_0 + port * 4, high);

	if (sc->bxe_flags & BXE_SAFC_TX_FLAG) {
		REG_WR(sc, BRB1_REG_HIGH_LLFC_LOW_THRESHOLD_0 + port * 4, 0xa0);
		REG_WR(sc, BRB1_REG_HIGH_LLFC_HIGH_THRESHOLD_0 + port * 4,
		    0xd8);
		REG_WR(sc, BRB1_REG_LOW_LLFC_LOW_THRESHOLD_0 + port *4, 0xa0);
		REG_WR(sc, BRB1_REG_LOW_LLFC_HIGH_THRESHOLD_0 + port * 4, 0xd8);
	}

	/* Port PRS comes here. */
	bxe_init_block(sc, PRS_BLOCK, init_stage);

	bxe_init_block(sc, TSDM_BLOCK, init_stage);
	bxe_init_block(sc, CSDM_BLOCK, init_stage);
	bxe_init_block(sc, USDM_BLOCK, init_stage);
	bxe_init_block(sc, XSDM_BLOCK, init_stage);

	bxe_init_block(sc, TSEM_BLOCK, init_stage);
	bxe_init_block(sc, USEM_BLOCK, init_stage);
	bxe_init_block(sc, CSEM_BLOCK, init_stage);
	bxe_init_block(sc, XSEM_BLOCK, init_stage);

	bxe_init_block(sc, UPB_BLOCK, init_stage);
	bxe_init_block(sc, XPB_BLOCK, init_stage);

	bxe_init_block(sc, PBF_BLOCK, init_stage);

	/* Configure PBF to work without pause for MTU = 9000. */
	REG_WR(sc, PBF_REG_P0_PAUSE_ENABLE + port * 4, 0);

	/* Update threshold. */
	REG_WR(sc, PBF_REG_P0_ARB_THRSH + port * 4, (9040/16));
	/* Update initial credit. */
	REG_WR(sc, PBF_REG_P0_INIT_CRD + port * 4, (9040/16) + 553 - 22);

	/* Probe changes. */
	REG_WR(sc, PBF_REG_INIT_P0 + port * 4, 1);
	DELAY(5000);
	REG_WR(sc, PBF_REG_INIT_P0 + port * 4, 0);

	bxe_init_block(sc, CDU_BLOCK, init_stage);
	bxe_init_block(sc, CFC_BLOCK, init_stage);

	if (CHIP_IS_E1(sc)) {
		REG_WR(sc, HC_REG_LEADING_EDGE_0 + port * 8, 0);
		REG_WR(sc, HC_REG_TRAILING_EDGE_0 + port * 8, 0);
	}
	bxe_init_block(sc, HC_BLOCK, init_stage);

	bxe_init_block(sc, MISC_AEU_BLOCK, init_stage);
	/*
	 * init aeu_mask_attn_func_0/1:
	 *  - SF mode: bits 3-7 are masked. only bits 0-2 are in use
	 *  - MF mode: bit 3 is masked. bits 0-2 are in use as in SF
	 *             bits 4-7 are used for "per vn group attention"
	 */
	REG_WR(sc, MISC_REG_AEU_MASK_ATTN_FUNC_0 + port * 4,
	    (IS_E1HMF(sc) ? 0xF7 : 0x7));

	bxe_init_block(sc, PXPCS_BLOCK, init_stage);
	bxe_init_block(sc, EMAC0_BLOCK, init_stage);
	bxe_init_block(sc, EMAC1_BLOCK, init_stage);
	bxe_init_block(sc, DBU_BLOCK, init_stage);
	bxe_init_block(sc, DBG_BLOCK, init_stage);

	bxe_init_block(sc, NIG_BLOCK, init_stage);

	REG_WR(sc, NIG_REG_XGXS_SERDES0_MODE_SEL + port * 4, 1);

	if (CHIP_IS_E1H(sc)) {
		/* Enable outer VLAN support if required. */
		REG_WR(sc, NIG_REG_LLH0_BRB1_DRV_MASK_MF + port * 4,
		    (IS_E1HOV(sc) ? 0x1 : 0x2));

		if (sc->bxe_flags & BXE_SAFC_TX_FLAG){
			high = 0;
			for (i = 0; i < BXE_MAX_PRIORITY; i++) {
				if (sc->pri_map[i] == 1)
					high |= (1 << i);
			}
			REG_WR(sc, NIG_REG_LLFC_HIGH_PRIORITY_CLASSES_0 +
			    port * 4, high);
			low = 0;
			for (i = 0; i < BXE_MAX_PRIORITY; i++) {
				if (sc->pri_map[i] == 0)
					low |= (1 << i);
			}
			REG_WR(sc, NIG_REG_LLFC_LOW_PRIORITY_CLASSES_0 +
			    port * 4, low);

			REG_WR(sc, NIG_REG_PAUSE_ENABLE_0 + port * 4, 0);
			REG_WR(sc, NIG_REG_LLFC_ENABLE_0 + port * 4, 1);
			REG_WR(sc, NIG_REG_LLFC_OUT_EN_0 + port * 4, 1);
		} else {
			REG_WR(sc, NIG_REG_LLFC_ENABLE_0 + port * 4, 0);
			REG_WR(sc, NIG_REG_LLFC_OUT_EN_0 + port * 4, 0);
			REG_WR(sc, NIG_REG_PAUSE_ENABLE_0 + port * 4, 1);
		}
	}

	bxe_init_block(sc, MCP_BLOCK, init_stage);
	bxe_init_block(sc, DMAE_BLOCK, init_stage);

	switch (XGXS_EXT_PHY_TYPE(sc->link_params.ext_phy_config)) {
	case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8726:
		bxe_set_gpio(sc, MISC_REGISTERS_GPIO_3,
		    MISC_REGISTERS_GPIO_INPUT_HI_Z, port);

		/*
		 * The GPIO should be swapped if the swap register is
		 * set and active.
		 */
		swap_val = REG_RD(sc, NIG_REG_PORT_SWAP);
		swap_override = REG_RD(sc, NIG_REG_STRAP_OVERRIDE);

		/* Select function upon port-swap configuration. */
		if (port == 0) {
			offset = MISC_REG_AEU_ENABLE1_FUNC_0_OUT_0;
			aeu_gpio_mask = (swap_val && swap_override) ?
			    AEU_INPUTS_ATTN_BITS_GPIO3_FUNCTION_1 :
			    AEU_INPUTS_ATTN_BITS_GPIO3_FUNCTION_0;
		} else {
			offset = MISC_REG_AEU_ENABLE1_FUNC_1_OUT_0;
			aeu_gpio_mask = (swap_val && swap_override) ?
			    AEU_INPUTS_ATTN_BITS_GPIO3_FUNCTION_0 :
			    AEU_INPUTS_ATTN_BITS_GPIO3_FUNCTION_1;
		}
		val = REG_RD(sc, offset);
		/* Add GPIO3 to group. */
		val |= aeu_gpio_mask;
		REG_WR(sc, offset, val);
		break;
	case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_SFX7101:
	case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727:
		/* Add SPIO 5 to group 0. */
		reg_addr = port ? MISC_REG_AEU_ENABLE1_FUNC_1_OUT_0 :
		    MISC_REG_AEU_ENABLE1_FUNC_0_OUT_0;
		val = REG_RD(sc, reg_addr);
		val |= AEU_INPUTS_ATTN_BITS_SPIO5;
		REG_WR(sc, reg_addr, val);
		break;
	default:
		break;
	}

	bxe__link_reset(sc);

	DBEXIT(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);

	return (0);
}

#define	ILT_PER_FUNC		(768/2)
#define	FUNC_ILT_BASE(func)	(func * ILT_PER_FUNC)
/*
 * The phys address is shifted right 12 bits and has an added 1=valid
 * bit added to the 53rd bit (bit 52) then since this is a wide
 * register(TM) we split it into two 32 bit writes.
 */
#define	ONCHIP_ADDR1(x)		((uint32_t)(((uint64_t)x >> 12) & 0xFFFFFFFF))
#define	ONCHIP_ADDR2(x)		((uint32_t)((1 << 20) | ((uint64_t)x >> 44)))
#define	PXP_ONE_ILT(x)		(((x) << 10) | x)
#define	PXP_ILT_RANGE(f, l)	(((l) << 10) | f)
#define	CNIC_ILT_LINES		0

/*
 * ILT write.
 *
 * Returns:
 *   None.
 */
static void
bxe_ilt_wr(struct bxe_softc *sc, uint32_t index, bus_addr_t addr)
{
	int reg;

	DBENTER(BXE_INSANE_LOAD | BXE_INSANE_RESET);

	if (CHIP_IS_E1H(sc))
		reg = PXP2_REG_RQ_ONCHIP_AT_B0 + index * 8;
	else
		reg = PXP2_REG_RQ_ONCHIP_AT + index * 8;

	bxe_wb_wr(sc, reg, ONCHIP_ADDR1(addr), ONCHIP_ADDR2(addr));

	DBEXIT(BXE_INSANE_LOAD | BXE_INSANE_RESET);
}

/*
 * Initialize a function.
 *
 * Returns:
 *   0 = Success, !0 = Failure.
 */
static int
bxe_init_func(struct bxe_softc *sc)
{
	uint32_t addr, val;
	int func, i, port;

	port = BP_PORT(sc);
	func = BP_FUNC(sc);

	DBENTER(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);

	DBPRINT(sc, (BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET),
	    "%s(): Initializing port %d, function %d.\n", __FUNCTION__, port,
	    func);

	/* Set MSI reconfigure capability. */
	addr = (port ? HC_REG_CONFIG_1 : HC_REG_CONFIG_0);
	val = REG_RD(sc, addr);
	val |= HC_CONFIG_0_REG_MSI_ATTN_EN_0;
	REG_WR(sc, addr, val);

	i = FUNC_ILT_BASE(func);

	bxe_ilt_wr(sc, i, BXE_SP_MAPPING(sc, context));

	if (CHIP_IS_E1H(sc)) {
		REG_WR(sc, PXP2_REG_RQ_CDU_FIRST_ILT, i);
		REG_WR(sc, PXP2_REG_RQ_CDU_LAST_ILT, i + CNIC_ILT_LINES);
	} else /* E1 */
		REG_WR(sc, PXP2_REG_PSWRQ_CDU0_L2P + func * 4,
		    PXP_ILT_RANGE(i, i + CNIC_ILT_LINES));

	if (CHIP_IS_E1H(sc)) {
		bxe_init_block(sc, MISC_BLOCK, FUNC0_STAGE + func);
		bxe_init_block(sc, TCM_BLOCK, FUNC0_STAGE + func);
		bxe_init_block(sc, UCM_BLOCK, FUNC0_STAGE + func);
		bxe_init_block(sc, CCM_BLOCK, FUNC0_STAGE + func);
		bxe_init_block(sc, XCM_BLOCK, FUNC0_STAGE + func);
		bxe_init_block(sc, TSEM_BLOCK, FUNC0_STAGE + func);
		bxe_init_block(sc, USEM_BLOCK, FUNC0_STAGE + func);
		bxe_init_block(sc, CSEM_BLOCK, FUNC0_STAGE + func);
		bxe_init_block(sc, XSEM_BLOCK, FUNC0_STAGE + func);

		REG_WR(sc, NIG_REG_LLH0_FUNC_EN + port * 8, 1);
		REG_WR(sc, NIG_REG_LLH0_FUNC_VLAN_ID + port * 8, sc->e1hov);
	}

	/* Host Coalescing initialization per function. */
	if (CHIP_IS_E1H(sc)) {
		REG_WR(sc, MISC_REG_AEU_GENERAL_ATTN_12 + func * 4, 0);
		REG_WR(sc, HC_REG_LEADING_EDGE_0 + port * 8, 0);
		REG_WR(sc, HC_REG_TRAILING_EDGE_0 + port * 8, 0);
	}

	bxe_init_block(sc, HC_BLOCK, FUNC0_STAGE + func);

	/* Reset PCIe block debug values. */
	REG_WR(sc, 0x2114, 0xffffffff);
	REG_WR(sc, 0x2120, 0xffffffff);

	DBEXIT(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);

	return (0);
}

/*
 *
 * Returns:
 *   0 = Failure, !0 = Failure.
 */
static int
bxe_init_hw(struct bxe_softc *sc, uint32_t load_code)
{
	int func, i, rc;

	rc = 0;
	DBENTER(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);

	sc->dmae_ready = 0;
	bxe_gunzip_init(sc);
	switch (load_code) {
	case FW_MSG_CODE_DRV_LOAD_COMMON:
		rc = bxe_init_common(sc);
		if (rc)
			goto bxe_init_hw_exit;
		/* FALLTHROUGH */
	case FW_MSG_CODE_DRV_LOAD_PORT:
		sc->dmae_ready = 1;
		rc = bxe_init_port(sc);
		if (rc)
			goto bxe_init_hw_exit;
		/* FALLTHROUGH */
	case FW_MSG_CODE_DRV_LOAD_FUNCTION:
		sc->dmae_ready = 1;
		rc = bxe_init_func(sc);
		if (rc)
			goto bxe_init_hw_exit;
		break;
	default:
		DBPRINT(sc, BXE_WARN,
		    "%s(): Unknown load_code (0x%08X) from MCP!\n",
		    __FUNCTION__, load_code);
		break;
	}

	/* Fetch additional config data if the bootcode is running. */
	if (!BP_NOMCP(sc)) {
		func = BP_FUNC(sc);
		/* Fetch the pulse sequence number. */
		sc->fw_drv_pulse_wr_seq = (SHMEM_RD(sc,
		    func_mb[func].drv_pulse_mb) & DRV_PULSE_SEQ_MASK);
	}

	/* This needs to be done before gunzip end. */
	bxe_zero_def_sb(sc);
	for (i = 0; i < sc->num_queues; i++)
		bxe_zero_sb(sc, BP_L_ID(sc) + i);

bxe_init_hw_exit:
	bxe_gunzip_end(sc);

	DBEXIT(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);

	return (rc);
}

/*
 * Send a firmware command and wait for the response.
 *
 * Post a command to shared memory for the bootcode running on the MCP and
 * stall until the bootcode responds or a timeout occurs.
 *
 * Returns:
 *   0 = Failure, otherwise firmware response code (FW_MSG_CODE_*).
 */
static int
bxe_fw_command(struct bxe_softc *sc, uint32_t command)
{
	uint32_t cnt, rc, seq;
	int func;

	func = BP_FUNC(sc);
	seq = ++sc->fw_seq;
	rc = 0;
	cnt = 1;

	DBENTER(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);

	DBRUNMSG(BXE_VERBOSE, bxe_decode_mb_msgs(sc, (command | seq), 0));

	BXE_FWMB_LOCK(sc);

	/* Write the command to the shared memory mailbox. */
	SHMEM_WR(sc, func_mb[func].drv_mb_header, (command | seq));

	/* Wait up to 2 seconds for a response. */
	do {
		/* Wait 10ms for a response. */
		DELAY(10000);

		/* Pickup the response. */
		rc = SHMEM_RD(sc, func_mb[func].fw_mb_header);
	} while ((seq != (rc & FW_MSG_SEQ_NUMBER_MASK)) && (cnt++ < 400));

	DBRUNMSG(BXE_VERBOSE, bxe_decode_mb_msgs(sc, 0, rc));

	/* Make sure we read the right response. */
	if (seq == (rc & FW_MSG_SEQ_NUMBER_MASK ))
		rc &= FW_MSG_CODE_MASK;
	else {
		BXE_PRINTF("%s(%d): Bootcode failed to respond!\n",
		    __FILE__, __LINE__);
		DBRUN(bxe_dump_fw(sc));
		rc = 0;
	}

	BXE_FWMB_UNLOCK(sc);

	DBEXIT(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);
	return (rc);
}

/*
 * Free any DMA memory owned by the driver.
 *
 * Scans through each data structre that requires DMA memory and frees
 * the memory if allocated.
 *
 * Returns:
 *   Nothing.
 */
static void
bxe_dma_free(struct bxe_softc *sc)
{
	struct bxe_fastpath *fp;
	int i, j;

	DBENTER(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);

	if (sc->parent_tag != NULL) {

		for (i = 0; i < sc->num_queues; i++) {
			fp = &sc->fp[i];
			/* Trust no one! */
			if (fp) {
				/* Free, unmap, and destroy the status block. */
				if (fp->status_block_tag != NULL) {
					if (fp->status_block_map != NULL) {
						if (fp->status_block != NULL)
							bus_dmamem_free(
							  fp->status_block_tag,
							  fp->status_block,
							  fp->status_block_map);

						bus_dmamap_unload(
						    fp->status_block_tag,
						    fp->status_block_map);
						bus_dmamap_destroy(
						    fp->status_block_tag,
						    fp->status_block_map);
					}

					bus_dma_tag_destroy(
					    fp->status_block_tag);
				}

				/*
				 * Free, unmap and destroy all TX BD
				 * chain pages.
				 */
				if (fp->tx_bd_chain_tag != NULL) {
					for (j = 0; j < NUM_TX_PAGES; j++ ) {
						if (fp->tx_bd_chain_map[j] != NULL) {
							if (fp->tx_bd_chain[j] != NULL)
								bus_dmamem_free(fp->tx_bd_chain_tag,
									fp->tx_bd_chain[j],
									fp->tx_bd_chain_map[j]);

							bus_dmamap_unload(fp->tx_bd_chain_tag,
								fp->tx_bd_chain_map[j]);
							bus_dmamap_destroy(fp->tx_bd_chain_tag,
								fp->tx_bd_chain_map[j]);
						}
					}

					bus_dma_tag_destroy(fp->tx_bd_chain_tag);
				}

				/* Free, unmap and destroy all RX BD chain pages. */
				if (fp->rx_bd_chain_tag != NULL) {

					for (j = 0; j < NUM_RX_PAGES; j++ ) {
						if (fp->rx_bd_chain_map[j] != NULL) {
							if (fp->rx_bd_chain[j] != NULL)
								bus_dmamem_free(fp->rx_bd_chain_tag,
									fp->rx_bd_chain[j],
									fp->rx_bd_chain_map[j]);

							bus_dmamap_unload(fp->rx_bd_chain_tag,
								fp->rx_bd_chain_map[j]);
							bus_dmamap_destroy(fp->rx_bd_chain_tag,
								fp->rx_bd_chain_map[j]);
						}
					}

					bus_dma_tag_destroy(fp->rx_bd_chain_tag);
				}

				/*
				 * Free, unmap and destroy all RX CQ
				 * chain pages.
				 */
				if (fp->rx_cq_chain_tag != NULL) {
					for (j = 0; j < NUM_RCQ_PAGES; j++ ) {
						if (fp->rx_cq_chain_map[j] != NULL) {
							if (fp->rx_cq_chain[j] != NULL)
								bus_dmamem_free(fp->rx_cq_chain_tag,
									fp->rx_cq_chain[j],
									fp->rx_cq_chain_map[j]);

							bus_dmamap_unload(fp->rx_cq_chain_tag,
								fp->rx_cq_chain_map[j]);
							bus_dmamap_destroy(fp->rx_cq_chain_tag,
								fp->rx_cq_chain_map[j]);
						}
					}

					bus_dma_tag_destroy(fp->rx_cq_chain_tag);
				}

				/* Unload and destroy the TX mbuf maps. */
				if (fp->tx_mbuf_tag != NULL) {
					for (j = 0; j < TOTAL_TX_BD; j++) {
						if (fp->tx_mbuf_map[j] != NULL) {
							bus_dmamap_unload(fp->tx_mbuf_tag,
								fp->tx_mbuf_map[j]);
							bus_dmamap_destroy(fp->tx_mbuf_tag,
								fp->tx_mbuf_map[j]);
						}
					}

					bus_dma_tag_destroy(fp->tx_mbuf_tag);
				}


				if (TPA_ENABLED(sc)) {
					int tpa_pool_max = CHIP_IS_E1H(sc) ?
						ETH_MAX_AGGREGATION_QUEUES_E1H :
						ETH_MAX_AGGREGATION_QUEUES_E1;

					/* Unload and destroy the TPA pool mbuf maps. */
					if (fp->rx_mbuf_tag != NULL) {

						for (j = 0; j < tpa_pool_max; j++) {

							if (fp->tpa_mbuf_map[j] != NULL) {
								bus_dmamap_unload(fp->rx_mbuf_tag,
									fp->tpa_mbuf_map[j]);
								bus_dmamap_destroy(fp->rx_mbuf_tag,
									fp->tpa_mbuf_map[j]);
							}
						}
					}

					/* Free, unmap and destroy all RX SGE chain pages. */
					if (fp->rx_sge_chain_tag != NULL) {
						for (j = 0; j < NUM_RX_SGE_PAGES; j++ ) {
							if (fp->rx_sge_chain_map[j] != NULL) {
								if (fp->rx_sge_chain[j] != NULL)
									bus_dmamem_free(fp->rx_sge_chain_tag,
										fp->rx_sge_chain[j],
										fp->rx_sge_chain_map[j]);

								bus_dmamap_unload(fp->rx_sge_chain_tag,
									fp->rx_sge_chain_map[j]);
								bus_dmamap_destroy(fp->rx_sge_chain_tag,
									fp->rx_sge_chain_map[j]);
							}
						}

						bus_dma_tag_destroy(fp->rx_sge_chain_tag);
					}

					/* Unload and destroy the SGE Buf maps. */
					if (fp->rx_sge_buf_tag != NULL) {

						for (j = 0; j < TOTAL_RX_SGE; j++) {
							if (fp->rx_sge_buf_map[j] != NULL) {
								bus_dmamap_unload(fp->rx_sge_buf_tag,
									fp->rx_sge_buf_map[j]);
								bus_dmamap_destroy(fp->rx_sge_buf_tag,
									fp->rx_sge_buf_map[j]);
							}
						}

						bus_dma_tag_destroy(fp->rx_sge_buf_tag);
					}
				}

				/* Unload and destroy the RX mbuf maps. */
				if (fp->rx_mbuf_tag != NULL) {
					for (j = 0; j < TOTAL_RX_BD; j++) {
						if (fp->rx_mbuf_map[j] != NULL) {
							bus_dmamap_unload(fp->rx_mbuf_tag,
								fp->rx_mbuf_map[j]);
							bus_dmamap_destroy(fp->rx_mbuf_tag,
								fp->rx_mbuf_map[j]);
						}
					}

					bus_dma_tag_destroy(fp->rx_mbuf_tag);
				}

			}
		}

		/* Destroy the def_status block. */
		if (sc->def_status_block_tag != NULL) {
			if (sc->def_status_block_map != NULL) {
				if (sc->def_status_block != NULL)
					bus_dmamem_free(
					    sc->def_status_block_tag,
					    sc->def_status_block,
					    sc->def_status_block_map);

				bus_dmamap_unload(sc->def_status_block_tag,
				    sc->def_status_block_map);
				bus_dmamap_destroy(sc->def_status_block_tag,
				    sc->def_status_block_map);
			}

			bus_dma_tag_destroy(sc->def_status_block_tag);
		}

		/* Destroy the statistics block. */
		if (sc->stats_tag != NULL) {
			if (sc->stats_map != NULL) {
				if (sc->stats_block != NULL)
					bus_dmamem_free(sc->stats_tag,
					    sc->stats_block, sc->stats_map);
				bus_dmamap_unload(sc->stats_tag, sc->stats_map);
				bus_dmamap_destroy(sc->stats_tag,
				    sc->stats_map);
			}

			bus_dma_tag_destroy(sc->stats_tag);
		}

		/* Destroy the Slow Path block. */
		if (sc->slowpath_tag != NULL) {
			if (sc->slowpath_map != NULL) {
				if (sc->slowpath != NULL)
					bus_dmamem_free(sc->slowpath_tag,
					    sc->slowpath, sc->slowpath_map);

				bus_dmamap_unload(sc->slowpath_tag,
				    sc->slowpath_map);
				bus_dmamap_destroy(sc->slowpath_tag,
				    sc->slowpath_map);
			}

			bus_dma_tag_destroy(sc->slowpath_tag);
		}

		/* Destroy the Slow Path Ring. */
		if (sc->spq_tag != NULL) {
			if (sc->spq_map != NULL) {
				if (sc->spq != NULL)
					bus_dmamem_free(sc->spq_tag, sc->spq,
					    sc->spq_map);

				bus_dmamap_unload(sc->spq_tag, sc->spq_map);
				bus_dmamap_destroy(sc->spq_tag, sc->spq_map);
			}

			bus_dma_tag_destroy(sc->spq_tag);
		}


		free(sc->strm, M_DEVBUF);
		sc->strm = NULL;

		if (sc->gunzip_tag != NULL) {
			if (sc->gunzip_map != NULL) {
				if (sc->gunzip_buf != NULL)
					bus_dmamem_free(sc->gunzip_tag,
					    sc->gunzip_buf, sc->gunzip_map);

				bus_dmamap_unload(sc->gunzip_tag,
				    sc->gunzip_map);
				bus_dmamap_destroy(sc->gunzip_tag,
				    sc->gunzip_map);
			}

			bus_dma_tag_destroy(sc->gunzip_tag);
		}

		bus_dma_tag_destroy(sc->parent_tag);
	}

	DBENTER(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);
}

/*
 * Free paged pool memory maps and tags.
 *
 * Returns:
 *   Nothing.
 */

static void
bxe_dmamem_free(struct bxe_softc *sc, bus_dma_tag_t tag, caddr_t buf,
    bus_dmamap_t map)
{

	DBENTER(BXE_VERBOSE_RESET | BXE_VERBOSE_UNLOAD);

	if (tag) {
		if (sc->gunzip_buf != NULL)
			bus_dmamem_free(tag, buf, map);

		if (map != NULL) {
			bus_dmamap_unload(tag, map);
			bus_dmamap_destroy(tag, map);
		}

		if (tag != NULL)
			bus_dma_tag_destroy(tag);
	}


	DBEXIT(BXE_VERBOSE_RESET | BXE_VERBOSE_UNLOAD);
}

/*
 * Get DMA memory from the OS.
 *
 * Validates that the OS has provided DMA buffers in response to a
 * bus_dmamap_load call and saves the physical address of those buffers.
 * When the callback is used the OS will return 0 for the mapping function
 * (bus_dmamap_load) so we use the value of map_arg->maxsegs to pass any
 * failures back to the caller.
 *
 * Returns:
 *   Nothing.
 */
static void
bxe_dma_map_addr(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	bus_addr_t *busaddr;

	busaddr = arg;
	/* Check for an error and signal the caller that an error occurred. */
	if (error) {
		printf(
		    "bxe %s(%d): DMA mapping error (error = %d, nseg = %d)!\n",
		    __FILE__, __LINE__, error, nseg);
		*busaddr = 0;
		return;
	}

	*busaddr = segs->ds_addr;
}

/*
 * Allocate any non-paged DMA memory needed by the driver.
 *
 * Allocates DMA memory needed for the various global structures which are
 * read or written by the hardware.
 *
 * Returns:
 *   0 = Success, !0 = Failure.
 */
static int
bxe_dma_alloc(device_t dev)
{
	struct bxe_softc *sc;
	struct bxe_fastpath *fp;
	int error, rc;
	bus_addr_t busaddr;
	bus_size_t max_size, max_seg_size;
	int i, j, max_segments;

	sc = device_get_softc(dev);
	rc = 0;

	DBENTER(BXE_VERBOSE_RESET);

	/*
	 * Allocate the parent bus DMA tag appropriate for PCI.
	 */
	if (bus_dma_tag_create(NULL,	/* parent tag */
	    1,				/* alignment for segs */
	    BXE_DMA_BOUNDARY,		/* cannot cross */
	    BUS_SPACE_MAXADDR,		/* restricted low */
	    BUS_SPACE_MAXADDR,		/* restricted hi */
	    NULL,			/* filter f() */
	    NULL,			/* filter f() arg */
	    MAXBSIZE,			/* max map for this tag */
	    BUS_SPACE_UNRESTRICTED,	/* # of discontinuities */
	    BUS_SPACE_MAXSIZE_32BIT,	/* max seg size */
	    0,				/* flags */
	    NULL,			/* lock f() */
	    NULL,			/* lock f() arg */
	    &sc->parent_tag)		/* dma tag */
	    ) {
		BXE_PRINTF("%s(%d): Could not allocate parent DMA tag!\n",
		    __FILE__, __LINE__);
		rc = ENOMEM;
		goto bxe_dma_alloc_exit;
	}

	/* Allocate DMA memory for each fastpath structure. */
	for (i = 0; i < sc->num_queues; i++) {
		fp = &sc->fp[i];
		DBPRINT(sc, (BXE_EXTREME_LOAD | BXE_EXTREME_RESET),
		    "%s(): fp[%d] virtual address = %p, size = %lu\n",
		    __FUNCTION__,  i, fp,
		    (long unsigned int)sizeof(struct bxe_fastpath));

		/*
		 * Create a DMA tag for the status block, allocate and
		 * clear the memory, map the memory into DMA space, and
		 * fetch the physical address of the block.
		 */

		if (bus_dma_tag_create(sc->parent_tag,
		    BCM_PAGE_SIZE,	/* alignment for segs */
		    BXE_DMA_BOUNDARY, 	/* cannot cross */
		    BUS_SPACE_MAXADDR,	/* restricted low */
		    BUS_SPACE_MAXADDR,	/* restricted hi */
		    NULL,		/* filter f() */
		    NULL,		/* filter f() arg */
		    BXE_STATUS_BLK_SZ,	/* max map for this tag */
		    1,			/* # of discontinuities */
		    BXE_STATUS_BLK_SZ,	/* max seg size */
		    0,			/* flags */
		    NULL,		/* lock f() */
		    NULL,		/* lock f() arg */
		    &fp->status_block_tag)) {
			BXE_PRINTF(
	"%s(%d): Could not allocate fp[%d] status block DMA tag!\n",
			    __FILE__, __LINE__, i);
			rc = ENOMEM;
			goto bxe_dma_alloc_exit;
		}

		if (bus_dmamem_alloc(fp->status_block_tag,
		    (void **)&fp->status_block, BUS_DMA_NOWAIT,
		    &fp->status_block_map)) {
			BXE_PRINTF(
	"%s(%d): Could not allocate fp[%d] status block	DMA memory!\n",
			    __FILE__, __LINE__, i);
			rc = ENOMEM;
			goto bxe_dma_alloc_exit;
		}

		bzero((char *)fp->status_block, BXE_STATUS_BLK_SZ);

		error = bus_dmamap_load(fp->status_block_tag,
		    fp->status_block_map, fp->status_block, BXE_STATUS_BLK_SZ,
		    bxe_dma_map_addr, &busaddr, BUS_DMA_NOWAIT);

		if (error) {
			BXE_PRINTF(
		"%s(%d): Could not map fp[%d] status block DMA memory!\n",
			    __FILE__, __LINE__, i);
			rc = ENOMEM;
			goto bxe_dma_alloc_exit;
		}

		/* Physical address of Status Block */
		fp->status_block_paddr = busaddr;
		DBPRINT(sc, (BXE_EXTREME_LOAD | BXE_EXTREME_RESET),
		    "%s(): fp[%d] status block physical address = 0x%jX\n",
		    __FUNCTION__, i, (uintmax_t) fp->status_block_paddr);

		/*
		 * Create a DMA tag for the TX buffer descriptor chain,
		 * allocate and clear the  memory, and fetch the
		 * physical address of the block.
		 */
		if (bus_dma_tag_create(sc->parent_tag,
		    BCM_PAGE_SIZE,	/* alignment for segs */
		    BXE_DMA_BOUNDARY,	/* cannot cross */
		    BUS_SPACE_MAXADDR,	/* restricted low */
		    BUS_SPACE_MAXADDR,	/* restricted hi */
		    NULL,		/* filter f() */
		    NULL,		/* filter f() arg */
		    BXE_TX_CHAIN_PAGE_SZ,/* max map for this tag */
		    1,			/* # of discontinuities */
		    BXE_TX_CHAIN_PAGE_SZ,/* max seg size */
		    0,			/* flags */
		    NULL,		/* lock f() */
		    NULL,		/* lock f() arg */
		    &fp->tx_bd_chain_tag)) {
			BXE_PRINTF(
	"%s(%d): Could not allocate fp[%d] TX descriptor chain DMA tag!\n",
			    __FILE__, __LINE__, i);
			rc = ENOMEM;
			goto bxe_dma_alloc_exit;
		}

		for (j = 0; j < NUM_TX_PAGES; j++) {
			if (bus_dmamem_alloc(fp->tx_bd_chain_tag,
			    (void **)&fp->tx_bd_chain[j], BUS_DMA_NOWAIT,
			    &fp->tx_bd_chain_map[j])) {
				BXE_PRINTF(
	"%s(%d): Could not allocate fp[%d] TX descriptor chain DMA memory!\n",
				    __FILE__, __LINE__, i);
				rc = ENOMEM;
				goto bxe_dma_alloc_exit;
			}

			bzero((char *)fp->tx_bd_chain[j], BXE_TX_CHAIN_PAGE_SZ);

			error = bus_dmamap_load(fp->tx_bd_chain_tag,
			    fp->tx_bd_chain_map[j], fp->tx_bd_chain[j],
			    BXE_TX_CHAIN_PAGE_SZ, bxe_dma_map_addr,
			    &busaddr, BUS_DMA_NOWAIT);

			if (error) {
				BXE_PRINTF(
	"%s(%d): Could not map fp[%d] TX descriptor chain DMA memory!\n",
				    __FILE__, __LINE__, i);
				rc = ENOMEM;
				goto bxe_dma_alloc_exit;
			}

			/* Physical Address of each page in the Tx BD Chain. */
			fp->tx_bd_chain_paddr[j] = busaddr;
			DBPRINT(sc, (BXE_EXTREME_LOAD | BXE_EXTREME_RESET),
			    "%s(): fp[%d]->tx_bd_chain_paddr[%d] = 0x%jX\n",
			    __FUNCTION__, i, j, (uintmax_t)busaddr);
		}

		/*
		 * Check required size before mapping to conserve resources.
		 */
		if (sc->tso_enable == TRUE) {
			max_size     = BXE_TSO_MAX_SIZE;
			max_segments = BXE_TSO_MAX_SEGMENTS;
			max_seg_size = BXE_TSO_MAX_SEG_SIZE;
		} else {
			max_size     = MCLBYTES * BXE_MAX_SEGMENTS;
			max_segments = BXE_MAX_SEGMENTS;
			max_seg_size = MCLBYTES;
		}

		/* Create a DMA tag for TX mbufs. */
		if (bus_dma_tag_create(sc->parent_tag,
		    1, 			/* alignment for segs */
		    BXE_DMA_BOUNDARY,	/* cannot cross */
		    BUS_SPACE_MAXADDR,	/* restricted low */
		    BUS_SPACE_MAXADDR,	/* restricted hi */
		    NULL,		/* filter f() */
		    NULL,		/* filter f() arg */
		    max_size,		/* max map for this tag */
		    max_segments,	/* # of discontinuities */
		    max_seg_size,	/* max seg size */
		    0,			/* flags */
		    NULL,		/* lock f() */
		    NULL,		/* lock f() arg */
		    &fp->tx_mbuf_tag)) {
			BXE_PRINTF(
		"%s(%d): Could not allocate fp[%d] TX mbuf DMA tag!\n",
			    __FILE__, __LINE__, i);
			rc = ENOMEM;
			goto bxe_dma_alloc_exit;
		}

		/* Create DMA maps for each the TX mbuf cluster(ext buf). */
		for (j = 0; j < TOTAL_TX_BD; j++) {
			if (bus_dmamap_create(fp->tx_mbuf_tag,
			    BUS_DMA_NOWAIT,
			    &(fp->tx_mbuf_map[j]))) {
				BXE_PRINTF(
		"%s(%d): Unable to create fp[%d] TX mbuf DMA map!\n",
				    __FILE__, __LINE__, i);
				rc = ENOMEM;
				goto bxe_dma_alloc_exit;
			}
		}

		/*
		 * Create a DMA tag for the RX buffer
		 * descriptor chain, allocate and clear
		 * the  memory, and fetch the physical
		 * address of the blocks.
		 */
		if (bus_dma_tag_create(sc->parent_tag,
		    BCM_PAGE_SIZE,	/* alignment for segs */
		    BXE_DMA_BOUNDARY,	/* cannot cross */
		    BUS_SPACE_MAXADDR,	/* restricted low */
		    BUS_SPACE_MAXADDR,	/* restricted hi */
		    NULL,		/* filter f() */
		    NULL,		/* filter f() arg */
		    BXE_RX_CHAIN_PAGE_SZ,/* max map for this tag */
		    1,			/* # of discontinuities */
		    BXE_RX_CHAIN_PAGE_SZ,/* max seg size */
		    0,			/* flags */
		    NULL,		/* lock f() */
		    NULL,		/* lock f() arg */
		    &fp->rx_bd_chain_tag)) {
			BXE_PRINTF(
	"%s(%d): Could not allocate fp[%d] RX BD chain DMA tag!\n",
			    __FILE__, __LINE__, i);
			rc = ENOMEM;
			goto bxe_dma_alloc_exit;
		}

		for (j = 0; j < NUM_RX_PAGES; j++) {
			if (bus_dmamem_alloc(fp->rx_bd_chain_tag,
			    (void **)&fp->rx_bd_chain[j], BUS_DMA_NOWAIT,
			    &fp->rx_bd_chain_map[j])) {
				BXE_PRINTF(
	"%s(%d): Could not allocate fp[%d] RX BD chain[%d] DMA memory!\n",
				    __FILE__, __LINE__, i, j);
				rc = ENOMEM;
				goto bxe_dma_alloc_exit;
			}

			bzero((char *)fp->rx_bd_chain[j], BXE_RX_CHAIN_PAGE_SZ);

			error = bus_dmamap_load(fp->rx_bd_chain_tag,
			    fp->rx_bd_chain_map[j], fp->rx_bd_chain[j],
			    BXE_RX_CHAIN_PAGE_SZ, bxe_dma_map_addr, &busaddr,
			    BUS_DMA_NOWAIT);

			if (error) {
				BXE_PRINTF(
		"%s(%d): Could not map fp[%d] RX BD chain[%d] DMA memory!\n",
				    __FILE__, __LINE__, i, j);
				rc = ENOMEM;
				goto bxe_dma_alloc_exit;
			}

			/* Physical address of each page in the RX BD chain */
			fp->rx_bd_chain_paddr[j] = busaddr;

			DBPRINT(sc, (BXE_EXTREME_LOAD | BXE_EXTREME_RESET),
			    "%s(): fp[%d]->rx_bd_chain_paddr[%d] = 0x%jX\n",
			    __FUNCTION__, i, j, (uintmax_t)busaddr);
		}

		/*
		 * Create a DMA tag for RX mbufs.
		 */
		if (bus_dma_tag_create(sc->parent_tag,
		    1,			/* alignment for segs */
		    BXE_DMA_BOUNDARY,	/* cannot cross */
		    BUS_SPACE_MAXADDR,	/* restricted low */
		    BUS_SPACE_MAXADDR,	/* restricted hi */
		    NULL,		/* filter f() */
		    NULL,		/* filter f() arg */
		    MJUM9BYTES,		/* max map for this tag */
		    1,			/* # of discontinuities */
		    MJUM9BYTES,		/* max seg size */
		    0,			/* flags */
		    NULL,		/* lock f() */
		    NULL,		/* lock f() arg */
		    &fp->rx_mbuf_tag)) {
			BXE_PRINTF(
		"%s(%d): Could not allocate fp[%d] RX mbuf DMA tag!\n",
			    __FILE__, __LINE__, i);
			rc = ENOMEM;
			goto bxe_dma_alloc_exit;
		}

		/* Create DMA maps for the RX mbuf clusters. */
		for (j = 0; j < TOTAL_RX_BD; j++) {
			if (bus_dmamap_create(fp->rx_mbuf_tag,
			    BUS_DMA_NOWAIT, &(fp->rx_mbuf_map[j]))) {
				BXE_PRINTF(
		"%s(%d): Unable to create fp[%d] RX mbuf DMA map!\n",
				    __FILE__, __LINE__, i);
				rc = ENOMEM;
				goto bxe_dma_alloc_exit;
			}
		}

		/*
		 * Create a DMA tag for the RX Completion
		 * Queue, allocate and clear the  memory,
		 * map the memory into DMA space, and fetch
		 * the physical address of the block.
		 */
		if (bus_dma_tag_create(sc->parent_tag,
		    BCM_PAGE_SIZE,	/* alignment for segs */
		    BXE_DMA_BOUNDARY,	/* cannot cross */
		    BUS_SPACE_MAXADDR,	/* restricted low */
		    BUS_SPACE_MAXADDR,	/* restricted hi */
		    NULL,		/* filter f() */
		    NULL,		/* filter f() arg */
		    BXE_RX_CHAIN_PAGE_SZ,/* max map for this tag */
		    1,			/* # of discontinuities */
		    BXE_RX_CHAIN_PAGE_SZ,/* max seg size */
		    0,			/* flags */
		    NULL,		/* lock f() */
		    NULL,		/* lock f() arg */
		    &fp->rx_cq_chain_tag)) {
			BXE_PRINTF(
	"%s(%d): Could not allocate fp[%d] RX Completion Queue DMA tag!\n",
			    __FILE__, __LINE__, i);
			rc = ENOMEM;
			goto bxe_dma_alloc_exit;
		}

		for (j = 0; j < NUM_RCQ_PAGES; j++) {
			if (bus_dmamem_alloc(fp->rx_cq_chain_tag,
			    (void **)&fp->rx_cq_chain[j], BUS_DMA_NOWAIT,
			    &fp->rx_cq_chain_map[j])) {
				BXE_PRINTF(
	"%s(%d): Could not allocate fp[%d] RX Completion Queue DMA memory!\n",
				    __FILE__, __LINE__, i);
				rc = ENOMEM;
				goto bxe_dma_alloc_exit;
			}

			bzero((char *)fp->rx_cq_chain[j],
			    BXE_RX_CHAIN_PAGE_SZ);

			error = bus_dmamap_load(fp->rx_cq_chain_tag,
			    fp->rx_cq_chain_map[j], fp->rx_cq_chain[j],
			    BXE_RX_CHAIN_PAGE_SZ, bxe_dma_map_addr, &busaddr,
			    BUS_DMA_NOWAIT);

			if (error) {
				BXE_PRINTF(
	"%s(%d): Could not map fp[%d] RX Completion Queue DMA memory!\n",
				    __FILE__, __LINE__, i);
				rc = ENOMEM;
				goto bxe_dma_alloc_exit;
			}

			/*
			 * Physical address of each page in the RX
			 * Completion Chain.
			 */
			fp->rx_cq_chain_paddr[j] = busaddr;

			DBPRINT(sc, (BXE_EXTREME_LOAD | BXE_EXTREME_RESET),
			    "%s(): fp[%d]->rx_cq_chain_paddr[%d] = 0x%jX\n",
			    __FUNCTION__, i, j, (uintmax_t)busaddr);
		}

		if (TPA_ENABLED(sc)) {
			int tpa_pool_max = CHIP_IS_E1H(sc) ?
			    ETH_MAX_AGGREGATION_QUEUES_E1H :
			    ETH_MAX_AGGREGATION_QUEUES_E1;

			/*
			 * Create a DMA tag for the RX SGE Ring,
			 * allocate and clear the memory, map the
			 * memory into DMA space, and fetch the
			 * physical address of the block.
			 */
			if (bus_dma_tag_create(sc->parent_tag,
			    BCM_PAGE_SIZE,	/* alignment for segs */
			    BXE_DMA_BOUNDARY,	/* cannot cross */
			    BUS_SPACE_MAXADDR,	/* restricted low */
			    BUS_SPACE_MAXADDR,	/* restricted hi */
			    NULL,		/* filter f() */
			    NULL,		/* filter f() arg */
			    BXE_RX_CHAIN_PAGE_SZ,/* max map for this tag */
			    1,			/* # of discontinuities */
			    BXE_RX_CHAIN_PAGE_SZ,/* max seg size */
			    0,			/* flags */
			    NULL,		/* lock f() */
			    NULL,		/* lock f() arg */
			    &fp->rx_sge_chain_tag)) {
				BXE_PRINTF(
	"%s(%d): Could not allocate fp[%d] RX SGE descriptor chain DMA tag!\n",
				    __FILE__, __LINE__, i);
				rc = ENOMEM;
				goto bxe_dma_alloc_exit;
			}

			for (j = 0; j < NUM_RX_SGE_PAGES; j++) {
				if (bus_dmamem_alloc(fp->rx_sge_chain_tag,
				    (void **)&fp->rx_sge_chain[j],
				    BUS_DMA_NOWAIT, &fp->rx_sge_chain_map[j])) {
					BXE_PRINTF(
	"%s(%d): Could not allocate fp[%d] RX SGE chain[%d] DMA memory!\n",
					    __FILE__, __LINE__, i, j);
					rc = ENOMEM;
					goto bxe_dma_alloc_exit;
				}

				bzero((char *)fp->rx_sge_chain[j],
				    BXE_RX_CHAIN_PAGE_SZ);

				error = bus_dmamap_load(fp->rx_sge_chain_tag,
				    fp->rx_sge_chain_map[j],
				    fp->rx_sge_chain[j], BXE_RX_CHAIN_PAGE_SZ,
				    bxe_dma_map_addr, &busaddr, BUS_DMA_NOWAIT);

				if (error) {
					BXE_PRINTF(
	"%s(%d): Could not map fp[%d] RX SGE chain[%d] DMA memory!\n",
					    __FILE__, __LINE__, i, j);
					rc = ENOMEM;
					goto bxe_dma_alloc_exit;
				}

				/*
				 * Physical address of each page in the RX
				 * SGE chain.
				 */
				DBPRINT(sc,
				    (BXE_EXTREME_LOAD | BXE_EXTREME_RESET),
			"%s(): fp[%d]->rx_sge_chain_paddr[%d] = 0x%jX\n",
				    __FUNCTION__, i, j, (uintmax_t)busaddr);
				fp->rx_sge_chain_paddr[j] = busaddr;
			}

			/*
			 * Create a DMA tag for RX SGE bufs.
			 */
			if (bus_dma_tag_create(sc->parent_tag, 1,
			    BXE_DMA_BOUNDARY, BUS_SPACE_MAXADDR,
			    BUS_SPACE_MAXADDR, NULL, NULL, PAGE_SIZE, 1,
			    PAGE_SIZE, 0, NULL, NULL, &fp->rx_sge_buf_tag)) {
				BXE_PRINTF(
		"%s(%d): Could not allocate fp[%d] RX SGE mbuf DMA tag!\n",
				    __FILE__, __LINE__, i);
				rc = ENOMEM;
				goto bxe_dma_alloc_exit;
			}

			/* Create DMA maps for the SGE mbuf clusters. */
			for (j = 0; j < TOTAL_RX_SGE; j++) {
				if (bus_dmamap_create(fp->rx_sge_buf_tag,
				    BUS_DMA_NOWAIT, &(fp->rx_sge_buf_map[j]))) {
					BXE_PRINTF(
		"%s(%d): Unable to create fp[%d] RX SGE mbuf DMA map!\n",
					    __FILE__, __LINE__, i);
					rc = ENOMEM;
					goto bxe_dma_alloc_exit;
				}
			}

			/* Create DMA maps for the TPA pool mbufs. */
			for (j = 0; j < tpa_pool_max; j++) {
				if (bus_dmamap_create(fp->rx_mbuf_tag,
				    BUS_DMA_NOWAIT, &(fp->tpa_mbuf_map[j]))) {
					BXE_PRINTF(
			"%s(%d): Unable to create fp[%d] TPA DMA map!\n",
					    __FILE__, __LINE__, i);
					rc = ENOMEM;
					goto bxe_dma_alloc_exit;
				}
			}
		}
	}

	/*
	 * Create a DMA tag for the def_status block, allocate and clear the
	 * memory, map the memory into DMA space, and fetch the physical
	 * address of the block.
	 */
	if (bus_dma_tag_create(sc->parent_tag, BCM_PAGE_SIZE, BXE_DMA_BOUNDARY,
	    BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL, NULL,
	    BXE_DEF_STATUS_BLK_SZ, 1, BXE_DEF_STATUS_BLK_SZ, 0, NULL, NULL,
	    &sc->def_status_block_tag)) {
		BXE_PRINTF(
		    "%s(%d): Could not allocate def_status block DMA tag!\n",
		    __FILE__, __LINE__);
		rc = ENOMEM;
		goto bxe_dma_alloc_exit;
	}

	if (bus_dmamem_alloc(sc->def_status_block_tag,
	    (void **)&sc->def_status_block, BUS_DMA_NOWAIT,
	    &sc->def_status_block_map)) {
		BXE_PRINTF(
		    "%s(%d): Could not allocate def_status block DMA memory!\n",
		    __FILE__, __LINE__);
		rc = ENOMEM;
		goto bxe_dma_alloc_exit;
	}

	bzero((char *)sc->def_status_block, BXE_DEF_STATUS_BLK_SZ);

	error = bus_dmamap_load(sc->def_status_block_tag,
	    sc->def_status_block_map, sc->def_status_block,
	    BXE_DEF_STATUS_BLK_SZ, bxe_dma_map_addr, &busaddr, BUS_DMA_NOWAIT);

	if (error) {
		BXE_PRINTF(
		    "%s(%d): Could not map def_status block DMA memory!\n",
		    __FILE__, __LINE__);
		rc = ENOMEM;
		goto bxe_dma_alloc_exit;
	}

	/* Physical Address of Default Status Block. */
	sc->def_status_block_paddr = busaddr;
	DBPRINT(sc, (BXE_EXTREME_LOAD | BXE_EXTREME_RESET),
	    "%s(): Default status block physical address = 0x%08X\n",
	    __FUNCTION__, (uint32_t)sc->def_status_block_paddr);

	/*
	 * Create a DMA tag for the statistics block, allocate and clear the
	 * memory, map the memory into DMA space, and fetch the physical
	 * address of the block.
	 */
	if (bus_dma_tag_create(sc->parent_tag, BXE_DMA_ALIGN, BXE_DMA_BOUNDARY,
	    BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL, NULL, BXE_STATS_BLK_SZ,
	    1, BXE_STATS_BLK_SZ, 0, NULL, NULL, &sc->stats_tag)) {
		BXE_PRINTF(
		    "%s(%d): Could not allocate statistics block DMA tag!\n",
		    __FILE__, __LINE__);
		rc = ENOMEM;
		goto bxe_dma_alloc_exit;
	}

	if (bus_dmamem_alloc(sc->stats_tag, (void **)&sc->stats_block,
	    BUS_DMA_NOWAIT, &sc->stats_map)) {
		BXE_PRINTF(
		    "%s(%d): Could not allocate statistics block DMA memory!\n",
		    __FILE__, __LINE__);
		rc = ENOMEM;
		goto bxe_dma_alloc_exit;
	}

	bzero((char *)sc->stats_block, BXE_STATS_BLK_SZ);

	error = bus_dmamap_load(sc->stats_tag, sc->stats_map, sc->stats_block,
	    BXE_STATS_BLK_SZ, bxe_dma_map_addr, &busaddr, BUS_DMA_NOWAIT);

	if (error) {
		BXE_PRINTF(
		    "%s(%d): Could not map statistics block DMA memory!\n",
		    __FILE__, __LINE__);
		rc = ENOMEM;
		goto bxe_dma_alloc_exit;
	}

	/* Physical Address of Statistics Block. */
	sc->stats_block_paddr = busaddr;
	DBPRINT(sc, (BXE_EXTREME_LOAD | BXE_EXTREME_RESET),
	    "%s(): Statistics block physical address = 0x%08X\n",
	    __FUNCTION__, (uint32_t)sc->stats_block_paddr);

	/*
	 * Create a DMA tag for slowpath memory, allocate and clear the
	 * memory, map the memory into DMA space, and fetch the physical
	 * address of the block.
	 */
	if (bus_dma_tag_create(sc->parent_tag, BCM_PAGE_SIZE, BXE_DMA_BOUNDARY,
	    BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL, NULL, BXE_SLOWPATH_SZ,
	    1, BXE_SLOWPATH_SZ, 0, NULL, NULL, &sc->slowpath_tag)) {
		BXE_PRINTF(
		    "%s(%d): Could not allocate slowpath DMA tag!\n",
		    __FILE__, __LINE__);
		rc = ENOMEM;
		goto bxe_dma_alloc_exit;
	}

	if (bus_dmamem_alloc(sc->slowpath_tag, (void **)&sc->slowpath,
	    BUS_DMA_NOWAIT, &sc->slowpath_map)) {
		BXE_PRINTF(
		    "%s(%d): Could not allocate slowpath DMA memory!\n",
		    __FILE__, __LINE__);
		rc = ENOMEM;
		goto bxe_dma_alloc_exit;
	}

	bzero((char *)sc->slowpath, BXE_SLOWPATH_SZ);

	error = bus_dmamap_load(sc->slowpath_tag, sc->slowpath_map,
	    sc->slowpath, BXE_SLOWPATH_SZ, bxe_dma_map_addr, &busaddr,
	    BUS_DMA_NOWAIT);

	if (error) {
		BXE_PRINTF("%s(%d): Could not map slowpath DMA memory!\n",
		    __FILE__, __LINE__);
		rc = ENOMEM;
		goto bxe_dma_alloc_exit;
	}

	/* Physical Address For Slow Path Context. */
	sc->slowpath_paddr = busaddr;
	DBPRINT(sc, (BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET),
	    "%s(): Slowpath context physical address = 0x%08X\n",
		__FUNCTION__, (uint32_t)sc->slowpath_paddr);

	/*
	 * Create a DMA tag for the Slow Path Queue, allocate and clear the
	 * memory, map the memory into DMA space, and fetch the physical
	 * address of the block.
	 */
	if (bus_dma_tag_create(sc->parent_tag, BCM_PAGE_SIZE, BXE_DMA_BOUNDARY,
	    BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL, NULL, BXE_SPQ_SZ, 1,
	    BXE_SPQ_SZ, 0, NULL, NULL, &sc->spq_tag)) {
		BXE_PRINTF("%s(%d): Could not allocate SPQ DMA tag!\n",
		    __FILE__, __LINE__);
		rc = ENOMEM;
		goto bxe_dma_alloc_exit;
	}

	if (bus_dmamem_alloc(sc->spq_tag, (void **)&sc->spq, BUS_DMA_NOWAIT,
	    &sc->spq_map)) {
		BXE_PRINTF("%s(%d): Could not allocate SPQ DMA memory!\n",
		    __FILE__, __LINE__);
		rc = ENOMEM;
		goto bxe_dma_alloc_exit;
	}

	bzero((char *)sc->spq, BXE_SPQ_SZ);

	error = bus_dmamap_load(sc->spq_tag, sc->spq_map, sc->spq, BXE_SPQ_SZ,
	    bxe_dma_map_addr, &busaddr, BUS_DMA_NOWAIT);

	if (error) {
		BXE_PRINTF("%s(%d): Could not map SPQ DMA memory!\n",
			__FILE__, __LINE__);
		rc = ENOMEM;
		goto bxe_dma_alloc_exit;
	}

	/* Physical address of slow path queue. */
	sc->spq_paddr = busaddr;
	DBPRINT(sc, (BXE_EXTREME_LOAD | BXE_EXTREME_RESET),
	    "%s(): Slowpath queue physical address = 0x%08X\n",
	    __FUNCTION__, (uint32_t)sc->spq_paddr);

	if (bxe_gunzip_init(sc)) {
		rc = ENOMEM;
		goto bxe_dma_alloc_exit;
	}

bxe_dma_alloc_exit:
	DBEXIT(BXE_VERBOSE_RESET);
	return (rc);
}

/*
 * Allocate DMA memory used for the firmware gunzip memory.
 *
 * Returns:
 *   0 for success, !0 = Failure.
 */

static int
bxe_dmamem_alloc(struct bxe_softc *sc, bus_dma_tag_t tag, bus_dmamap_t map,
    void *buf, uint32_t buflen, bus_addr_t *busaddr)
{
	int rc;

	rc = 0;

	DBENTER(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);

	/*
	 * Create a DMA tag for the block, allocate and clear the
	 * memory, map the memory into DMA space, and fetch the physical
	 * address of the block.
	 */
	if (bus_dma_tag_create(sc->parent_tag, BXE_DMA_ALIGN, BXE_DMA_BOUNDARY,
	    BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL, NULL, buflen, 1, buflen,
	    0, NULL, NULL, &sc->gunzip_tag)) {
		BXE_PRINTF("%s(%d): Could not allocate DMA tag!\n",
		    __FILE__, __LINE__);
		rc = ENOMEM;
		goto bxe_dmamem_alloc_exit;
	}

	if (bus_dmamem_alloc(sc->gunzip_tag, (void **)&sc->gunzip_buf,
	    BUS_DMA_NOWAIT, &sc->gunzip_map)) {
		BXE_PRINTF("%s(%d): Could not allocate DMA memory!\n",
		    __FILE__, __LINE__);
		rc = ENOMEM;
		goto bxe_dmamem_alloc_exit;
	}

	bzero((char *)sc->gunzip_buf, buflen);

	if (bus_dmamap_load(sc->gunzip_tag, sc->gunzip_map, sc->gunzip_buf,
	    buflen, bxe_dma_map_addr, busaddr, BUS_DMA_NOWAIT)) {
		BXE_PRINTF("%s(%d): Could not map DMA memory!\n",
		    __FILE__, __LINE__);
		rc = ENOMEM;
	}

bxe_dmamem_alloc_exit:
	DBEXIT(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);
	return (rc);
}

/*
 * Program the MAC address for 57710 controllers.
 *
 * Returns:
 *   Nothing.
 */
static void
bxe_set_mac_addr_e1(struct bxe_softc *sc, int set)
{
	struct mac_configuration_cmd *config;
	struct mac_configuration_entry *config_table;
	uint8_t *eaddr;
	int port;

	DBENTER(BXE_VERBOSE_MISC);

	config = BXE_SP(sc, mac_config);
	port = BP_PORT(sc);
	/*
	 * CAM allocation:
	 * Port 0 Unicast Addresses: 32 Perfect Match Filters (31-0)
	 * Port 1 Unicast Addresses: 32 Perfect Match Filters (63-32)
	 * Port 0 Multicast Addresses: 128 Hashes (127-64)
	 * Port 1 Multicast Addresses: 128 Hashes (191-128)
	 */

	config->hdr.length = 2;
	config->hdr.offset = port ? 32 : 0;
	config->hdr.client_id = BP_CL_ID(sc);
	config->hdr.reserved1 = 0;

	/* Program the primary MAC address. */
	config_table = &config->config_table[0];
	eaddr = sc->link_params.mac_addr;
	config_table->cam_entry.msb_mac_addr = eaddr[0] << 8 | eaddr[1];
	config_table->cam_entry.middle_mac_addr = eaddr[2] << 8 | eaddr[3];
	config_table->cam_entry.lsb_mac_addr = eaddr[4] << 8 | eaddr[5];
	config_table->cam_entry.flags = htole16(port);

	if (set)
		config_table->target_table_entry.flags = 0;
	else
		CAM_INVALIDATE(config_table);

	config_table->target_table_entry.vlan_id = 0;

	DBPRINT(sc, BXE_VERBOSE, "%s(): %s MAC (%04x:%04x:%04x)\n",
	   __FUNCTION__, (set ? "Setting" : "Clearing"),
	   config_table->cam_entry.msb_mac_addr,
	   config_table->cam_entry.middle_mac_addr,
	   config_table->cam_entry.lsb_mac_addr);

	/* Program the broadcast MAC address. */
	config_table = &config->config_table[1];
	config_table->cam_entry.msb_mac_addr = 0xffff;
	config_table->cam_entry.middle_mac_addr = 0xffff;
	config_table->cam_entry.lsb_mac_addr = 0xffff;
	config_table->cam_entry.flags = htole16(port);

	if (set)
		config_table->target_table_entry.flags =
		    TSTORM_CAM_TARGET_TABLE_ENTRY_BROADCAST;
	else
		CAM_INVALIDATE(config_table);

	config_table->target_table_entry.vlan_id = 0;

	/* Post the command to slow path queue. */
	bxe_sp_post(sc, RAMROD_CMD_ID_ETH_SET_MAC, 0,
	    U64_HI(BXE_SP_MAPPING(sc, mac_config)),
	    U64_LO(BXE_SP_MAPPING(sc, mac_config)), 0);

	DBEXIT(BXE_VERBOSE_MISC);
}

/*
 * Program the MAC address for 57711/57711E controllers.
 *
 * Returns:
 *   Nothing.
 */
static void
bxe_set_mac_addr_e1h(struct bxe_softc *sc, int set)
{
	struct mac_configuration_cmd_e1h *config;
	struct mac_configuration_entry_e1h *config_table;
	uint8_t *eaddr;
	int func, port;

	DBENTER(BXE_VERBOSE_MISC);

	config = (struct mac_configuration_cmd_e1h *)BXE_SP(sc, mac_config);
	port = BP_PORT(sc);
	func = BP_FUNC(sc);

	if (set && (sc->state != BXE_STATE_OPEN)) {
		DBPRINT(sc, BXE_VERBOSE,
		    "%s(): Can't set E1H MAC in state 0x%08X!\n", __FUNCTION__,
		    sc->state);
		goto bxe_set_mac_addr_e1h_exit;
	}

	/*
	 * CAM allocation:
	 * Function 0-7 Unicast Addresses: 8 Perfect Match Filters
	 * Multicast Addresses: 20 + FUNC * 20, 20 each (???)
	 */
	config->hdr.length = 1;
	config->hdr.offset = func;
	config->hdr.client_id = 0xff;
	config->hdr.reserved1 = 0;

	/* Program the primary MAC address. */
	config_table = &config->config_table[0];
	eaddr = sc->link_params.mac_addr;
	config_table->msb_mac_addr = eaddr[0] << 8 | eaddr[1];
	config_table->middle_mac_addr = eaddr[2] << 8 | eaddr[3];
	config_table->lsb_mac_addr = eaddr[4] << 8 | eaddr[5];
	config_table->clients_bit_vector = htole32(1 << sc->fp->cl_id);

	config_table->vlan_id = 0;
	config_table->e1hov_id = htole16(sc->e1hov);

	if (set)
		config_table->flags = port;
	else
		config_table->flags =
			MAC_CONFIGURATION_ENTRY_E1H_ACTION_TYPE;

	DBPRINT(sc, BXE_VERBOSE_MISC,
	    "%s(): %s MAC (%04x:%04x:%04x), E1HOV = %d, CLID = %d\n",
	    __FUNCTION__, (set ? "Setting" : "Clearing"),
	    config_table->msb_mac_addr, config_table->middle_mac_addr,
	    config_table->lsb_mac_addr, sc->e1hov, BP_L_ID(sc));

	bxe_sp_post(sc, RAMROD_CMD_ID_ETH_SET_MAC, 0,
	    U64_HI(BXE_SP_MAPPING(sc, mac_config)),
	    U64_LO(BXE_SP_MAPPING(sc, mac_config)), 0);

bxe_set_mac_addr_e1h_exit:
	DBEXIT(BXE_VERBOSE_MISC);
}

/*
 * Programs the various packet receive modes (broadcast and multicast).
 *
 * Returns:
 *   Nothing.
 */

static void
bxe_set_rx_mode(struct bxe_softc *sc)
{
	struct ifnet *ifp;
	struct ifmultiaddr *ifma;
	struct mac_configuration_cmd *config;
	struct mac_configuration_entry *config_table;
	uint32_t mc_filter[MC_HASH_SIZE];
	uint8_t *maddr;
	uint32_t crc, bit, regidx, rx_mode;
	int i, old, offset, port;

	BXE_CORE_LOCK_ASSERT(sc);

	rx_mode = BXE_RX_MODE_NORMAL;
	port = BP_PORT(sc);

	DBENTER(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);

	if (sc->state != BXE_STATE_OPEN) {
		DBPRINT(sc, BXE_WARN, "%s(): State (0x%08X) is not open!\n",
		    __FUNCTION__, sc->state);
		goto bxe_set_rx_mode_exit;
	}

	ifp = sc->bxe_ifp;

	/*
	 * Check for promiscuous, all multicast, or selected
	 * multicast address filtering.
	 */
	if (ifp->if_flags & IFF_PROMISC) {
		DBPRINT(sc, BXE_VERBOSE_MISC,
		    "%s(): Enabling promiscuous mode.\n", __FUNCTION__);

		/* Enable promiscuous mode. */
		rx_mode = BXE_RX_MODE_PROMISC;
	} else if (ifp->if_flags & IFF_ALLMULTI ||
	    ifp->if_amcount > BXE_MAX_MULTICAST) {
		DBPRINT(sc, BXE_VERBOSE_MISC,
		    "%s(): Enabling all multicast mode.\n", __FUNCTION__);

		/* Enable all multicast addresses. */
		rx_mode = BXE_RX_MODE_ALLMULTI;
	} else {
		/* Enable selective multicast mode. */
		DBPRINT(sc, BXE_VERBOSE_MISC,
		    "%s(): Enabling selective multicast mode.\n",
		    __FUNCTION__);

		if (CHIP_IS_E1(sc)) {
			i = 0;
			config = BXE_SP(sc, mcast_config);

			IF_ADDR_LOCK(ifp);

			TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
				if (ifma->ifma_addr->sa_family != AF_LINK)
					continue;
				maddr = (uint8_t *)LLADDR(
				    (struct sockaddr_dl *)ifma->ifma_addr);
				config_table = &config->config_table[i];
				config_table->cam_entry.msb_mac_addr =
				    maddr[0] << 8 | maddr[1];
				config_table->cam_entry.middle_mac_addr =
				    maddr[2] << 8 | maddr[3];
				config_table->cam_entry.lsb_mac_addr =
				    maddr[4] << 8 | maddr[5];
				config_table->cam_entry.flags = htole16(port);
				config_table->target_table_entry.flags = 0;
				config_table->target_table_entry.
				    clients_bit_vector =
				    htole32(1 << BP_L_ID(sc));
				config_table->target_table_entry.vlan_id = 0;
				i++;
				DBPRINT(sc, BXE_INFO,
			"%s(): Setting MCAST[%d] (%04X:%04X:%04X)\n",
				    __FUNCTION__, i,
				    config_table->cam_entry.msb_mac_addr,
				    config_table->cam_entry.middle_mac_addr,
				    config_table->cam_entry.lsb_mac_addr);
			}

			IF_ADDR_UNLOCK(ifp);

			old = config->hdr.length;

			/* Invalidate any extra MC entries in the CAM. */
			if (old > i) {
				for (; i < old; i++) {
					config_table = &config->config_table[i];
					if (CAM_IS_INVALID(config_table))
						break;
					/* Invalidate */
					CAM_INVALIDATE(config_table);
				}
			}

			offset = BXE_MAX_MULTICAST * (1 + port);
			config->hdr.length = i;
			config->hdr.offset = offset;
			config->hdr.client_id = sc->fp->cl_id;
			config->hdr.reserved1 = 0;
			wmb();
			bxe_sp_post(sc, RAMROD_CMD_ID_ETH_SET_MAC, 0,
			    U64_HI(BXE_SP_MAPPING(sc, mcast_config)),
			    U64_LO(BXE_SP_MAPPING(sc, mcast_config)), 0);
		} else { /* E1H */
			/* Accept one or more multicasts */
			memset(mc_filter, 0, 4 * MC_HASH_SIZE);

			IF_ADDR_LOCK(ifp);

			TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
				if (ifma->ifma_addr->sa_family != AF_LINK)
					continue;
				crc = ether_crc32_le(ifma->ifma_addr->sa_data,
				    ETHER_ADDR_LEN);
				bit = (crc >> 24) & 0xff;
				regidx = bit >> 5;
				bit &= 0x1f;
				mc_filter[regidx] |= (1 << bit);
			}
			IF_ADDR_UNLOCK(ifp);

			for (i = 0; i < MC_HASH_SIZE; i++)
				REG_WR(sc, MC_HASH_OFFSET(sc, i), mc_filter[i]);
		}
	}

	DBPRINT(sc, BXE_VERBOSE, "%s(): Enabling new receive mode: 0x%08X\n",
	    __FUNCTION__, rx_mode);

	sc->rx_mode = rx_mode;
	bxe_set_storm_rx_mode(sc);

bxe_set_rx_mode_exit:
	DBEXIT(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET);
}

/*
 * Function specific controller reset.
 *
 * Returns:
 *   Nothing.
 */
static void
bxe_reset_func(struct bxe_softc *sc)
{
	int base, func, i, port;

	DBENTER(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET | BXE_VERBOSE_UNLOAD);

	port = BP_PORT(sc);
	func = BP_FUNC(sc);

	/* Configure IGU. */
	REG_WR(sc, HC_REG_LEADING_EDGE_0 + port * 8, 0);
	REG_WR(sc, HC_REG_TRAILING_EDGE_0 + port * 8, 0);

	REG_WR(sc, HC_REG_CONFIG_0 + (port * 4), 0x1000);

	/* Clear ILT. */
	base = FUNC_ILT_BASE(func);
	for (i = base; i < base + ILT_PER_FUNC; i++)
		bxe_ilt_wr(sc, i, 0);

	DBEXIT(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET | BXE_VERBOSE_UNLOAD);
}

/*
 * Port specific controller reset.
 *
 * Returns:
 *   Nothing.
 */
static void
bxe_reset_port(struct bxe_softc *sc)
{
	uint32_t val;
	int port;

	DBENTER(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET | BXE_VERBOSE_UNLOAD);

	port = BP_PORT(sc);
	REG_WR(sc, NIG_REG_MASK_INTERRUPT_PORT0 + port * 4, 0);

	/* Do not receive packets to BRB. */
	REG_WR(sc, NIG_REG_LLH0_BRB1_DRV_MASK + port * 4, 0x0);

	/* Do not direct receive packets that are not for MCP to the BRB. */
	REG_WR(sc, port ? NIG_REG_LLH1_BRB1_NOT_MCP :
	    NIG_REG_LLH0_BRB1_NOT_MCP, 0x0);

	/* Configure AEU. */
	REG_WR(sc, MISC_REG_AEU_MASK_ATTN_FUNC_0 + port * 4, 0);

	DELAY(100000);

	/* Check for BRB port occupancy. */
	val = REG_RD(sc, BRB1_REG_PORT_NUM_OCC_BLOCKS_0 + port * 4);
	if (val)
		DBPRINT(sc, BXE_VERBOSE,
		    "%s(): BRB1 is not empty (%d blocks are occupied)!\n",
		    __FUNCTION__, val);

	DBEXIT(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET | BXE_VERBOSE_UNLOAD);
}

/*
 * Common controller reset.
 *
 * Returns:
 *   Nothing.
 */
static void
bxe_reset_common(struct bxe_softc *sc)
{

	DBENTER(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET | BXE_VERBOSE_UNLOAD);

	REG_WR(sc, GRCBASE_MISC + MISC_REGISTERS_RESET_REG_1_CLEAR, 0xd3ffff7f);
	REG_WR(sc, GRCBASE_MISC + MISC_REGISTERS_RESET_REG_2_CLEAR, 0x1403);

	DBEXIT(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET | BXE_VERBOSE_UNLOAD);
}

/*
 * Reset the controller.
 *
 * Returns:
 *   Nothing.
 */
static void
bxe_reset_chip(struct bxe_softc *sc, uint32_t reset_code)
{

	DBENTER(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET | BXE_VERBOSE_UNLOAD);
	DBRUNLV(BXE_INFO, bxe_decode_mb_msgs(sc, 0, reset_code));

	switch (reset_code) {
	case FW_MSG_CODE_DRV_UNLOAD_COMMON:
		bxe_reset_port(sc);
		bxe_reset_func(sc);
		bxe_reset_common(sc);
		break;
	case FW_MSG_CODE_DRV_UNLOAD_PORT:
		bxe_reset_port(sc);
		bxe_reset_func(sc);
		break;
	case FW_MSG_CODE_DRV_UNLOAD_FUNCTION:
		bxe_reset_func(sc);
		break;
	default:
		BXE_PRINTF("%s(%d): Unknown reset code (0x%08X) from MCP!\n",
		    __FILE__, __LINE__, reset_code);
		break;
	}

	DBEXIT(BXE_VERBOSE_LOAD | BXE_VERBOSE_RESET | BXE_VERBOSE_UNLOAD);
}

/*
 * Called by the OS to set media options (link, speed, etc.).
 *
 * Returns:
 *   0 = Success, positive value for failure.
 */
static int
bxe_ifmedia_upd(struct ifnet *ifp)
{
	struct bxe_softc *sc;
	struct ifmedia *ifm;
	int rc;

	sc = ifp->if_softc;
	DBENTER(BXE_VERBOSE_PHY);

	ifm = &sc->bxe_ifmedia;
	rc = 0;

	/* This is an Ethernet controller. */
	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER) {
		rc = EINVAL;
		goto bxe_ifmedia_upd_exit;
	}

	BXE_CORE_LOCK(sc);

	switch (IFM_SUBTYPE(ifm->ifm_media)) {
	case IFM_AUTO:
		 DBPRINT(sc, BXE_VERBOSE_PHY,
	    "%s(): Media set to IFM_AUTO, restarting autonegotiation.\n",
		    __FUNCTION__);
		 break;
	case IFM_10G_CX4:
		DBPRINT(sc, BXE_VERBOSE_PHY,
	    "%s(): Media set to IFM_10G_CX4, forced mode.\n", __FUNCTION__);
		break;
	case IFM_10G_SR:
		DBPRINT(sc, BXE_VERBOSE_PHY,
	    "%s(): Media set to IFM_10G_SR, forced mode.\n", __FUNCTION__);
		break;
	case IFM_10G_T:
		DBPRINT(sc, BXE_VERBOSE_PHY,
	    "%s(): Media set to IFM_10G_T, forced mode.\n", __FUNCTION__);
		break;
	case IFM_10G_TWINAX:
		DBPRINT(sc, BXE_VERBOSE_PHY,
	    "%s(): Media set to IFM_10G_TWINAX, forced mode.\n", __FUNCTION__);
		break;
	default:
		DBPRINT(sc, BXE_WARN, "%s(): Invalid media type!\n",
		    __FUNCTION__);
		rc = EINVAL;
	}

	BXE_CORE_UNLOCK(sc);

bxe_ifmedia_upd_exit:
	DBENTER(BXE_VERBOSE_PHY);
	return (rc);
}

/*
 * Called by the OS to report current media status
 * (link, speed, etc.).
 *
 * Returns:
 *   Nothing.
 */
static void
bxe_ifmedia_status(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct bxe_softc *sc;

	sc = ifp->if_softc;
	DBENTER(BXE_EXTREME_LOAD | BXE_EXTREME_RESET);

	/* Report link down if the driver isn't running. */
	if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
		ifmr->ifm_active |= IFM_NONE;
		goto bxe_ifmedia_status_exit;
	}

	/* Setup the default interface info. */
	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	if (sc->link_vars.link_up)
		ifmr->ifm_status |= IFM_ACTIVE;
	else {
		ifmr->ifm_active |= IFM_NONE;
		goto bxe_ifmedia_status_exit;
	}

	ifmr->ifm_active |= sc->media;

	if (sc->link_vars.duplex == MEDIUM_FULL_DUPLEX)
		ifmr->ifm_active |= IFM_FDX;
	else
		ifmr->ifm_active |= IFM_HDX;

bxe_ifmedia_status_exit:
	DBEXIT(BXE_EXTREME_LOAD | BXE_EXTREME_RESET);
}


/*
 * Update last maximum scatter gather entry.
 *
 * Returns:
 *   None.
 */
static __inline void
bxe_update_last_max_sge(struct bxe_fastpath *fp, uint16_t idx)
{
	uint16_t last_max;

	last_max = fp->last_max_sge;
	if (SUB_S16(idx, last_max) > 0)
		fp->last_max_sge = idx;
}

/*
 * Clear scatter gather mask next elements.
 *
 * Returns:
 *   None
 */
static void
bxe_clear_sge_mask_next_elems(struct bxe_fastpath *fp)
{
	int i, idx, j;

	for (i = 1; i <= NUM_RX_SGE_PAGES; i++) {
		idx = RX_SGE_CNT * i - 1;
		for (j = 0; j < 2; j++) {
			SGE_MASK_CLEAR_BIT(fp, idx);
			idx--;
		}
	}
}

/*
 * Update SGE producer.
 *
 * Returns:
 *   None.
 */
static void
bxe_update_sge_prod(struct bxe_fastpath *fp,
    struct eth_fast_path_rx_cqe *fp_cqe)
{
	struct bxe_softc *sc;
	uint16_t delta, last_max, last_elem, first_elem, sge_len;
	int i;

	sc = fp->sc;
	DBENTER(BXE_EXTREME_RECV);

	delta = 0;
	sge_len = SGE_PAGE_ALIGN(le16toh(fp_cqe->pkt_len) -
	    le16toh(fp_cqe->len_on_bd)) >> SGE_PAGE_SHIFT;
	if (!sge_len)
		return;

	/* First mark all used pages. */
	for (i = 0; i < sge_len; i++)
		SGE_MASK_CLEAR_BIT(fp, RX_SGE(le16toh(fp_cqe->sgl[i])));

	/* Assume that the last SGE index is the biggest. */
	bxe_update_last_max_sge(fp, le16toh(fp_cqe->sgl[sge_len - 1]));

	last_max = RX_SGE(fp->last_max_sge);
	last_elem = last_max >> RX_SGE_MASK_ELEM_SHIFT;
	first_elem = RX_SGE(fp->rx_sge_prod) >> RX_SGE_MASK_ELEM_SHIFT;

	/* If ring is not full. */
	if (last_elem + 1 != first_elem)
		last_elem++;

	/* Now update the producer index. */
	for (i = first_elem; i != last_elem; i = NEXT_SGE_MASK_ELEM(i)) {
		if (fp->sge_mask[i])
			break;

		fp->sge_mask[i] = RX_SGE_MASK_ELEM_ONE_MASK;
		delta += RX_SGE_MASK_ELEM_SZ;
	}

	if (delta > 0) {
		fp->rx_sge_prod += delta;
		/* clear page-end entries */
		bxe_clear_sge_mask_next_elems(fp);
	}

	DBEXIT(BXE_EXTREME_RECV);
}

/*
 * Initialize scatter gather ring bitmask.
 *
 * Elements may be taken from the scatter gather ring out of order since
 * TCP frames may be out of order or intermingled among multiple TCP
 * flows on the wire.  The SGE bitmask tracks which elements are used
 * or available.
 *
 * Returns:
 *   None
 */
static __inline void
bxe_init_sge_ring_bit_mask(struct bxe_fastpath *fp)
{

	/* Set the mask to all 1s, it's faster to compare to 0 than to 0xf. */
	memset(fp->sge_mask, 0xff,
	    (TOTAL_RX_SGE >> RX_SGE_MASK_ELEM_SHIFT) * sizeof(uint64_t));

	/*
	 * Clear the two last indices in the page to 1.  These are the
	 * indices that correspond to the "next" element which will
	 * never be indicated and should be removed from calculations.
	 */
	bxe_clear_sge_mask_next_elems(fp);
}

/*
 * The current mbuf is part of an aggregation.  Swap the mbuf into the TPA
 * aggregation queue, swap an empty mbuf back onto the receive chain, and
 * mark the current aggregation queue as in-progress.
 *
 * Returns:
 *   None.
 */
static void
bxe_tpa_start(struct bxe_fastpath *fp, uint16_t queue, uint16_t cons,
    uint16_t prod)
{
	struct bxe_softc *sc = fp->sc;
	struct mbuf *m_temp;
	struct eth_rx_bd *rx_bd;
	bus_dmamap_t map_temp;

	sc = fp->sc;
	DBENTER(BXE_EXTREME_RECV);

	/* Move the empty mbuf and mapping from the TPA pool. */
	m_temp = fp->tpa_mbuf_ptr[queue];
	map_temp = fp->tpa_mbuf_map[queue];

	/* Move received mbuf and mapping to TPA pool. */
	fp->tpa_mbuf_ptr[queue] = fp->rx_mbuf_ptr[cons];
	fp->tpa_mbuf_map[queue] = fp->rx_mbuf_map[cons];

	DBRUNIF((fp->tpa_state[queue] != BXE_TPA_STATE_STOP),
	    DBPRINT(sc, BXE_FATAL, "%s(): Starting bin[%d] even though queue "
	    "is not in the TPA_STOP state!\n", __FUNCTION__, queue));

	/* Place the TPA bin into the START state. */
	fp->tpa_state[queue] = BXE_TPA_STATE_START;
	DBRUN(fp->tpa_queue_used |= (1 << queue));

	/* Get the rx_bd for the next open entry on the receive chain. */
	rx_bd = &fp->rx_bd_chain[RX_PAGE(prod)][RX_IDX(prod)];

	/* Update the rx_bd with the empty mbuf from the TPA pool. */
	rx_bd->addr_hi = htole32(U64_HI(fp->tpa_mbuf_segs[queue].ds_addr));
	rx_bd->addr_lo = htole32(U64_LO(fp->tpa_mbuf_segs[queue].ds_addr));
	fp->rx_mbuf_ptr[prod] = m_temp;
	fp->rx_mbuf_map[prod] = map_temp;

	DBEXIT(BXE_EXTREME_RECV);
}

/*
 * When a TPA aggregation is completed, loop through the individual mbufs
 * of the aggregation, combining them into a single mbuf which will be sent
 * up the stack.  Refill all mbufs freed as we go along.
 *
 * Returns:
 *   0 = Success, !0 = Failure.
 */
static int
bxe_fill_frag_mbuf(struct bxe_softc *sc, struct bxe_fastpath *fp,
    struct mbuf *m, struct eth_fast_path_rx_cqe *fp_cqe, uint16_t cqe_idx)
{
	uint32_t frag_len, frag_size, pages, i;
	uint16_t sge_idx, len_on_bd;
	int rc, j;

	DBENTER(BXE_EXTREME_RECV);

	rc = 0;
	len_on_bd = le16toh(fp_cqe->len_on_bd);
	frag_size = le16toh(fp_cqe->pkt_len) - len_on_bd;
	pages = SGE_PAGE_ALIGN(frag_size) >> SGE_PAGE_SHIFT;

	/* Make sure the aggregated frame is not too big to handle. */
	if (pages > 8 * PAGES_PER_SGE) {
		DBPRINT(sc, BXE_FATAL,
		    "%s(): SGL length (%d) is too long! CQE index is %d\n",
		    __FUNCTION__, pages, cqe_idx);
		DBPRINT(sc, BXE_FATAL,
		    "%s(): fp_cqe->pkt_len = %d fp_cqe->len_on_bd = %d\n",
		    __FUNCTION__, le16toh(fp_cqe->pkt_len), len_on_bd);
		bxe_panic_dump(sc);
		rc = EINVAL;
		goto bxe_fill_frag_mbuf_exit;
	}

	/*
	 * Run through the scatter gather list, pulling the individual
	 * mbufs into a single mbuf for the host stack.
	 */
	for (i = 0, j = 0; i < pages; i += PAGES_PER_SGE, j++) {
		sge_idx = RX_SGE(le16toh(fp_cqe->sgl[j]));

		/*
		 * Firmware gives the indices of the SGE as if the ring is an
		 * array (meaning that the "next" element will consume 2
		 * indices).
		 */
		frag_len = min(frag_size, (uint32_t)(BCM_PAGE_SIZE *
		    PAGES_PER_SGE));

		/* Update the mbuf with the fragment length. */
		fp->rx_sge_buf_ptr[sge_idx]->m_len = frag_len;

		/* Unmap the mbuf from DMA space. */
		bus_dmamap_sync(fp->rx_sge_buf_tag, fp->rx_sge_buf_map[sge_idx],
		    BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(fp->rx_sge_buf_tag,
		    fp->rx_sge_buf_map[sge_idx]);

		/* Concatenate the current fragment to the aggregated mbuf. */
		m_cat(m, fp->rx_sge_buf_ptr[sge_idx]);

		/* The SGE mbuf was freed in the call to m_cat(). */
		DBRUN(fp->sge_mbuf_alloc--);
		fp->rx_sge_buf_ptr[sge_idx] = NULL;

		/*
		 * Try an allocate a new mbuf for the SGE that was just
		 * released. If an allocation error occurs stop where we
		 * are and drop the whole frame.
		 */
		rc = bxe_alloc_rx_sge(sc, fp, sge_idx);
		if (rc)
			goto bxe_fill_frag_mbuf_exit;

		m->m_pkthdr.len += frag_len;

		frag_size -= frag_len;
	}

bxe_fill_frag_mbuf_exit:
	DBEXIT(BXE_EXTREME_RECV);
	return (rc);
}

/*
 * The aggregation on the current TPA queue has completed.  Pull the
 * individual mbuf fragments together into a single mbuf, perform all
 * necessary checksum calculations, and send the resuting mbuf to the stack.
 *
 * Returns:
 *   None.
 */
static void
bxe_tpa_stop(struct bxe_softc *sc, struct bxe_fastpath *fp, uint16_t queue,
    int pad, int len, union eth_rx_cqe *cqe, uint16_t cqe_idx)
{
	struct mbuf *m_old, *m_new;
	struct ip *ip;
	struct ifnet *ifp;
	struct ether_vlan_header *eh;
	bus_dma_segment_t seg;
	int rc, e_hlen;

	DBENTER(BXE_EXTREME_RECV);
	DBPRINT(sc, BXE_VERBOSE_RECV,
	    "%s(): fp[%d], tpa queue = %d, len = %d, pad = %d\n", __FUNCTION__,
	    fp->index, queue, len, pad);

	rc = 0;
	ifp = sc->bxe_ifp;
	/* Unmap m_old from DMA space. */
	m_old = fp->tpa_mbuf_ptr[queue];
	bus_dmamap_sync(fp->rx_mbuf_tag, fp->tpa_mbuf_map[queue],
	    BUS_DMASYNC_POSTREAD);
	bus_dmamap_unload(fp->rx_mbuf_tag, fp->tpa_mbuf_map[queue]);

	/* Skip over the pad when passing the data up the stack. */
	m_adj(m_old, pad);

	/* Adjust the packet length to match the received data. */
	m_old->m_pkthdr.len = m_old->m_len = len;

	/* Validate the checksum if offload enabled. */
	m_old->m_pkthdr.csum_flags |= CSUM_IP_CHECKED | CSUM_IP_VALID |
	    CSUM_DATA_VALID | CSUM_PSEUDO_HDR;
	m_old->m_pkthdr.csum_data = 0xffff;

	/* Map the header and find the Ethernet type & header length. */
	eh = mtod(m_old, struct ether_vlan_header *);
	if (eh->evl_encap_proto == htons(ETHERTYPE_VLAN))
		e_hlen = ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN;
	else
		e_hlen = ETHER_HDR_LEN;

	/* Get the IP header pointer. */
	ip = (struct ip *)(m_old->m_data + e_hlen);

	ip->ip_sum = 0;
	ip->ip_sum = in_cksum_hdr(ip);

	/* Try and aggregate all of the receive mbufs into a single mbuf. */
	if (!bxe_fill_frag_mbuf(sc, fp, m_old, &cqe->fast_path_cqe, cqe_idx)) {
		/*
		 * We have an aggregated frame.  If the frame has a vlan tag
		 * attach that information to the mbuf.
		 */
		if ((le16toh(cqe->fast_path_cqe.pars_flags.flags) &
		    PARSING_FLAGS_VLAN)) {
			m_old->m_pkthdr.ether_vtag =
			    cqe->fast_path_cqe.vlan_tag;
			m_old->m_flags |= M_VLANTAG;
		}

		/* Send the packet to the appropriate interface. */
		m_old->m_pkthdr.rcvif = ifp;

		/* Pass the packet up to the stack. */
		fp->ipackets++;
		DBRUN(fp->tpa_pkts++);
		(*ifp->if_input)(ifp, m_old);
	} else 	{
		DBPRINT(sc, BXE_WARN,
		    "%s(): Failed to allocate new SGE page, dropping frame!\n",
		    __FUNCTION__);
		fp->soft_rx_errors++;
		m_freem(m_old);
	}

	/* We passed m_old up the stack or dropped the frame. */
	DBRUN(fp->tpa_mbuf_alloc--);

	/* Allocate a replacement mbuf. */
	if (__predict_false((m_new = bxe_alloc_mbuf(fp,
	    sc->mbuf_alloc_size)) == NULL))
		goto bxe_tpa_stop_exit;

	/* Map the new mbuf and place it in the pool. */
	rc = bxe_map_mbuf(fp, m_new, fp->rx_mbuf_tag,
		fp->tpa_mbuf_map[queue], &seg);
	if (rc)
		goto bxe_tpa_stop_exit;

	DBRUN(fp->tpa_mbuf_alloc++);

	fp->tpa_mbuf_ptr[queue] = m_new;
	fp->tpa_mbuf_segs[queue] = seg;

bxe_tpa_stop_exit:
	fp->tpa_state[queue] = BXE_TPA_STATE_STOP;
	DBRUN(fp->tpa_queue_used &= ~(1 << queue));

	DBEXIT(BXE_EXTREME_RECV);
}

/*
 * Notify the controller that the RX producer indices have been updated for
 * a fastpath connection by writing them to the controller.
 *
 * Returns:
 *   None
 */
static __inline void
bxe_update_rx_prod(struct bxe_softc *sc, struct bxe_fastpath *fp,
    uint16_t bd_prod, uint16_t cqe_prod, uint16_t sge_prod)
{
	volatile struct ustorm_eth_rx_producers rx_prods = {0};
	int i;

	/* Update producers. */
	rx_prods.bd_prod =  bd_prod;
	rx_prods.cqe_prod = cqe_prod;
	rx_prods.sge_prod = sge_prod;

	wmb();

	for (i = 0; i < sizeof(struct ustorm_eth_rx_producers) / 4; i++){
		REG_WR(sc, BAR_USTORM_INTMEM +
		    USTORM_RX_PRODS_OFFSET(BP_PORT(sc), fp->cl_id) + i * 4,
		    ((volatile uint32_t *) &rx_prods)[i]);
	}

	DBPRINT(sc, BXE_EXTREME_RECV, "%s(%d): Wrote fp[%02d] bd_prod = 0x%04X, "
	    "cqe_prod = 0x%04X, sge_prod = 0x%04X\n", __FUNCTION__, curcpu,
	    fp->index, bd_prod, cqe_prod, sge_prod);
}

/*
 * Handles received frame interrupt events.
 *
 * Returns:
 *   Nothing.
 */
static void
bxe_rxeof(struct bxe_fastpath *fp)
{
	struct bxe_softc *sc;
	struct ifnet *ifp;
	uint16_t rx_bd_cons, rx_bd_cons_idx;
	uint16_t rx_bd_prod, rx_bd_prod_idx;
	uint16_t rx_cq_cons, rx_cq_cons_idx;
	uint16_t rx_cq_prod, rx_cq_cons_sb;
	unsigned long rx_pkts = 0;

	sc = fp->sc;
	ifp = sc->bxe_ifp;

	DBENTER(BXE_EXTREME_RECV);

	/* Get the status block's view of the RX completion consumer index. */
	rx_cq_cons_sb = bxe_rx_cq_cons(fp);

	/*
	 * Get working copies of the driver's view of the
	 * RX indices. These are 16 bit values that are
	 * expected to increment from from 0 to	65535
	 * and then wrap-around to 0 again.
	 */
	rx_bd_cons = fp->rx_bd_cons;
	rx_bd_prod = fp->rx_bd_prod;
	rx_cq_cons = fp->rx_cq_cons;
	rx_cq_prod = fp->rx_cq_prod;

	DBPRINT(sc, (BXE_EXTREME_RECV),
	    "%s(%d): BEFORE: fp[%d], rx_bd_cons = 0x%04X, rx_bd_prod = 0x%04X, "
	    "rx_cq_cons_sw = 0x%04X, rx_cq_prod_sw = 0x%04X\n", __FUNCTION__,
	    curcpu, fp->index, rx_bd_cons, rx_bd_prod, rx_cq_cons, rx_cq_prod);

	/*
	 * Memory barrier to prevent speculative reads of the RX buffer
	 * from getting ahead of the index in the status block.
	 */
	rmb();

	/*
	 * Scan through the receive chain as long
	 * as there is work to do.
	 */
	while (rx_cq_cons != rx_cq_cons_sb) {
		struct mbuf *m;
		union eth_rx_cqe *cqe;
		uint8_t cqe_fp_flags;
		uint16_t len, pad;

		/*
		 * Convert the 16 bit indices used by hardware
		 * into values that map to the arrays used by
		 * the driver (i.e. an index).
		 */
		rx_cq_cons_idx   = RCQ_ENTRY(rx_cq_cons);
		rx_bd_prod_idx = RX_BD(rx_bd_prod);
		rx_bd_cons_idx = RX_BD(rx_bd_cons);
		wmb();

		/* Fetch the cookie. */
		cqe = (union eth_rx_cqe *)
		    &fp->rx_cq_chain[RCQ_PAGE(rx_cq_cons_idx)][RCQ_IDX(rx_cq_cons_idx)];
		cqe_fp_flags = cqe->fast_path_cqe.type_error_flags;

#ifdef BXE_DEBUG
		/* Simulate an error on the received frame. */
		if (DB_RANDOMTRUE(bxe_debug_received_frame_error)) {
			DBPRINT(sc, BXE_WARN,
			    "%s(): Simulated CQE error flags!\n", __FUNCTION__);
			cqe_fp_flags |= ETH_RX_ERROR_FLAGS;
			sc->debug_received_frame_error++;
		}
#endif

		DBRUNIF((cqe_fp_flags == 0),
		    fp->null_cqe_flags++;
		    bxe_dump_cqe(fp, rx_cq_cons_idx, cqe));
		/* DRC - ANything else to do here? */

		/* Check the CQE type for slowpath or fastpath completion. */
		if (__predict_false(CQE_TYPE(cqe_fp_flags) ==
		    RX_ETH_CQE_TYPE_ETH_RAMROD)) {
			/* This is a slowpath completion. */
			bxe_sp_event(fp, cqe);
			goto bxe_rxeof_next_cqe;

		} else {
			/* This is a fastpath completion. */

			/* Get the length and pad information from the CQE. */
			len = le16toh(cqe->fast_path_cqe.pkt_len);
			pad = cqe->fast_path_cqe.placement_offset;

			/* Check if the completion is for TPA. */
			if ((!fp->disable_tpa) && (TPA_TYPE(cqe_fp_flags) !=
			    (TPA_TYPE_START | TPA_TYPE_END))) {
				uint16_t queue = cqe->fast_path_cqe.queue_index;

				/*
				 * No need to worry about error flags in
				 * the frame as the firmware has already
				 * managed that for us when aggregating
				 * the frames.
				 */

				/*
				 * Check if a TPA aggregation has been started.
				 */
				if (TPA_TYPE(cqe_fp_flags) == TPA_TYPE_START) {
					bxe_tpa_start(fp, queue,
					    rx_bd_cons_idx, rx_bd_prod_idx);
					goto bxe_rxeof_next_rx;
				}

				/* Check if a TPA aggregation has completed. */
				if (TPA_TYPE(cqe_fp_flags) == TPA_TYPE_END) {
					if (!BXE_RX_SUM_FIX(cqe))
						DBPRINT(sc, BXE_FATAL,
						"%s(): STOP on non-TCP data.\n",
						    __FUNCTION__);

					/*
					 * This is the size of the linear
					 * data on this mbuf.
					 */
					len = le16toh(cqe->fast_path_cqe.len_on_bd);

					/*
					 * Stop the aggregation and pass
					 * the frame up.
					 */
					bxe_tpa_stop(sc, fp, queue, pad, len,
					    cqe, rx_cq_cons_idx);
					bxe_update_sge_prod(fp,
					    &cqe->fast_path_cqe);
					goto bxe_rxeof_next_cqe;
				}
			}

			/* Remove the mbuf from the RX chain. */
			m = fp->rx_mbuf_ptr[rx_bd_cons_idx];
			fp->rx_mbuf_ptr[rx_bd_cons_idx] = NULL;

			DBRUN(fp->free_rx_bd++);
			DBRUNIF((fp->free_rx_bd > USABLE_RX_BD),
				DBPRINT(sc, BXE_FATAL,
			"%s(): fp[%d] - Too many free rx_bd's (0x%04X)!\n",
				    __FUNCTION__, fp->index, fp->free_rx_bd));

			/* Unmap the mbuf from DMA space. */
			bus_dmamap_sync(fp->rx_mbuf_tag,
			    fp->rx_mbuf_map[rx_bd_cons_idx],
			    BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(fp->rx_mbuf_tag,
			    fp->rx_mbuf_map[rx_bd_cons_idx]);

			/* Check if the received frame has any errors. */
			if (__predict_false(cqe_fp_flags &
			    ETH_RX_ERROR_FLAGS)) {
				DBPRINT(sc, BXE_WARN ,
				    "%s(): Found error flags (0x%08X) "
				    "set in received frame on fp[%d]!\n",
				    __FUNCTION__, cqe_fp_flags, fp->index);

				fp->soft_rx_errors++;

				/* Reuse the mbuf for a new frame. */
				if (bxe_get_buf(fp, m, rx_bd_prod_idx)) {
					DBPRINT(sc, BXE_FATAL,
					    "%s(): Can't reuse RX mbuf!\n",
					    __FUNCTION__);
					DBRUN(bxe_breakpoint(sc));

					/* ToDo: Find alterntive to panic(). */
					panic("bxe%d: Can't reuse RX mbuf!\n",
					    sc->bxe_unit);
				}

				/* Go handle any additional received frames. */
				goto bxe_rxeof_next_rx;
			}

			/*
			 * The high level logic used here is to
			 * immediatley replace each receive buffer
			 * as it is used so that the receive chain
			 * is full at all times.  First we try to
			 * allocate a new receive buffer, but if
			 * that fails then we will reuse the
			 * existing mbuf and log an error for the
			 * lost packet.
			 */

			/* Allocate a new mbuf for the receive chain. */
			if (__predict_false(bxe_get_buf(fp,
			    NULL, rx_bd_prod_idx))) {
				/*
				 * Drop the current frame if we can't get
				 * a new mbuf.
				 */
				fp->soft_rx_errors++;

				/*
				 * Place the current mbuf back in the
				 * receive chain.
				 */
				if (__predict_false(bxe_get_buf(fp, m,
				    rx_bd_prod_idx))) {
					/* This is really bad! */
					DBPRINT(sc, BXE_FATAL,
					    "%s(): Can't reuse RX mbuf!\n",
					    __FUNCTION__);
					DBRUN(bxe_breakpoint(sc));

					/* ToDo: Find alterntive to panic(). */
					panic(
				"bxe%d: Double mbuf allocation failure!\n",
					    sc->bxe_unit);
				}

				/* Go handle any additional received frames. */
				goto bxe_rxeof_next_rx;
			}

			/*
			 * Skip over the pad when passing the data up the stack.
			 */
			m_adj(m, pad);

			/*
			 * Adjust the packet length to match the received data.
			 */
			m->m_pkthdr.len = m->m_len = len;

			/* Send the packet to the appropriate interface. */
			m->m_pkthdr.rcvif = ifp;

			/* Assume no hardware checksum. */
			m->m_pkthdr.csum_flags = 0;

			/* Validate the checksum if offload enabled. */
			if (ifp->if_capenable & IFCAP_RXCSUM) {
				/* Check whether IP checksummed or not. */
				if (sc->rx_csum &&
				    !(cqe->fast_path_cqe.status_flags &
				    ETH_FAST_PATH_RX_CQE_IP_XSUM_NO_VALIDATION_FLG)) {
					m->m_pkthdr.csum_flags |=
					    CSUM_IP_CHECKED;
					if (__predict_false(cqe_fp_flags &
					    ETH_FAST_PATH_RX_CQE_IP_BAD_XSUM_FLG)) {
						DBPRINT(sc, BXE_WARN_SEND,
					"%s(): Invalid IP checksum!\n",
						    __FUNCTION__);
					} else
						m->m_pkthdr.csum_flags |=
						    CSUM_IP_VALID;
				}

				/* Check for a valid TCP/UDP frame. */
				if (sc->rx_csum &&
				    !(cqe->fast_path_cqe.status_flags &
				    ETH_FAST_PATH_RX_CQE_L4_XSUM_NO_VALIDATION_FLG)) {
					/* Check for a good TCP/UDP checksum. */
					if (__predict_false(cqe_fp_flags &
					    ETH_FAST_PATH_RX_CQE_L4_BAD_XSUM_FLG)) {
						DBPRINT(sc, BXE_VERBOSE_RECV,
					"%s(): Invalid TCP/UDP checksum!\n",
						    __FUNCTION__);
					} else {
						m->m_pkthdr.csum_data = 0xFFFF;
						m->m_pkthdr.csum_flags |=
						    (CSUM_DATA_VALID |
						    CSUM_PSEUDO_HDR);
					}
				}
			}

			/*
			 * If we received a packet with a vlan tag,
			 * attach that information to the packet.
			 */
			if (cqe->fast_path_cqe.pars_flags.flags &
			    PARSING_FLAGS_VLAN) {
				m->m_pkthdr.ether_vtag =
				    cqe->fast_path_cqe.vlan_tag;
				m->m_flags |= M_VLANTAG;
			}

#if __FreeBSD_version >= 800000
			/* Tell OS what RSS queue was used for this flow. */
			m->m_pkthdr.flowid = fp->index;
			m->m_flags |= M_FLOWID;
#endif

			/* Last chance to check for problems. */
			DBRUN(bxe_validate_rx_packet(fp, rx_cq_cons, cqe, m));

			/* Pass the mbuf off to the upper layers. */
			ifp->if_ipackets++;

			/* ToDo: Any potential locking issues here? */
			/* Pass the frame to the stack. */
			(*ifp->if_input)(ifp, m);

			DBRUN(fp->rx_mbuf_alloc--);
		}

bxe_rxeof_next_rx:
		rx_bd_prod = NEXT_RX_BD(rx_bd_prod);
		rx_bd_cons = NEXT_RX_BD(rx_bd_cons);
		rx_pkts++;

bxe_rxeof_next_cqe:
		rx_cq_prod = NEXT_RCQ_IDX(rx_cq_prod);
		rx_cq_cons = NEXT_RCQ_IDX(rx_cq_cons);

		/*
		 * Memory barrier to prevent speculative reads of the RX buffer
		 * from getting ahead of the index in the status block.
		 */
		rmb();
	}

	/* Update the driver copy of the fastpath indices. */
	fp->rx_bd_cons = rx_bd_cons;
	fp->rx_bd_prod = rx_bd_prod;
	fp->rx_cq_cons = rx_cq_cons;
	fp->rx_cq_prod = rx_cq_prod;

	DBPRINT(sc, (BXE_EXTREME_RECV),
	    "%s(%d):  AFTER: fp[%d], rx_bd_cons = 0x%04X, rx_bd_prod = 0x%04X, "
	    "rx_cq_cons_sw = 0x%04X, rx_cq_prod_sw = 0x%04X\n", __FUNCTION__,
	    curcpu, fp->index, rx_bd_cons, rx_bd_prod, rx_cq_cons, rx_cq_prod);

	/* Update producers */
	bxe_update_rx_prod(sc, fp, fp->rx_bd_prod,
	    fp->rx_cq_prod, fp->rx_sge_prod);
	bus_space_barrier(sc->bxe_btag, sc->bxe_bhandle, 0, 0,
	    BUS_SPACE_BARRIER_READ);

	fp->rx_pkts += rx_pkts;
	fp->rx_calls++;
	DBEXIT(BXE_EXTREME_RECV);
}

/*
 * Handles transmit completion interrupt events.
 *
 * Returns:
 *   Nothing.
 */
static void
bxe_txeof(struct bxe_fastpath *fp)
{
	struct bxe_softc *sc;
	struct ifnet *ifp;
	struct eth_tx_start_bd *txbd;
	uint16_t hw_pkt_cons, sw_pkt_cons, sw_tx_bd_cons, sw_tx_chain_cons;
	uint16_t pkt_cons, nbds;
	int i;

	sc = fp->sc;
	ifp = sc->bxe_ifp;

	DBENTER(BXE_EXTREME_SEND);
	DBPRINT(sc, BXE_EXTREME_SEND, "%s(): Servicing fp[%d]\n",
	    __FUNCTION__, fp->index);

	/* Get the hardware's view of the TX packet consumer index. */
	hw_pkt_cons = le16toh(*fp->tx_cons_sb);
	sw_pkt_cons = fp->tx_pkt_cons;
	sw_tx_bd_cons = fp->tx_bd_cons;

	/* Cycle through any completed TX chain page entries. */
	while (sw_pkt_cons != hw_pkt_cons) {
		txbd = NULL;
		sw_tx_chain_cons = TX_BD(sw_tx_bd_cons);
		pkt_cons = TX_BD(sw_pkt_cons);

#ifdef BXE_DEBUG
		if (sw_tx_chain_cons > MAX_TX_BD) {
			BXE_PRINTF(
		"%s(): TX chain consumer out of range! 0x%04X > 0x%04X\n",
			    __FUNCTION__, sw_tx_chain_cons, (int)MAX_TX_BD);
			bxe_breakpoint(sc);
		}
#endif

		txbd =
		    &fp->tx_bd_chain[TX_PAGE(sw_tx_chain_cons)][TX_IDX(sw_tx_chain_cons)].start_bd;

#ifdef BXE_DEBUG
		if (txbd == NULL) {
			BXE_PRINTF("%s(): Unexpected NULL tx_bd[0x%04X]!\n",
			    __FUNCTION__, sw_tx_chain_cons);
			bxe_breakpoint(sc);
		}
#endif

		/*
		 * Find the number of BD's that were used in the completed pkt.
		 */
		nbds = txbd->nbd;

		/*
		 * Free the ext mbuf cluster from the mbuf of the completed
		 * frame.
		 */
		if (__predict_true(fp->tx_mbuf_ptr[pkt_cons] != NULL)) {
			/* Unmap it from the mbuf. */
			bus_dmamap_unload(fp->tx_mbuf_tag,
			    fp->tx_mbuf_map[pkt_cons]);

			/* Return the mbuf to the stack. */
			DBRUN(fp->tx_mbuf_alloc--);
			m_freem(fp->tx_mbuf_ptr[pkt_cons]);
			fp->tx_mbuf_ptr[pkt_cons] = NULL;
			fp->opackets++;
		} else {
			fp->tx_chain_lost_mbuf++;
		}

		/* Skip over the remaining used buffer descriptors. */
		fp->used_tx_bd -= nbds;

		for (i = 0; i < nbds; i++)
			sw_tx_bd_cons = NEXT_TX_BD(sw_tx_bd_cons);

		/* Increment the software copy of packet consumer index */
		sw_pkt_cons++;

		/*
		 * Refresh the hw packet consumer index to see if there's
		 * new work.
		 */
		hw_pkt_cons = le16toh(*fp->tx_cons_sb);
		rmb();
	}

	/* Enable new transmits if we've made enough room. */
	if (fp->used_tx_bd < BXE_TX_CLEANUP_THRESHOLD) {
		ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
		if (fp->used_tx_bd == 0) {
			/*
			 * Clear the watchdog timer if we've emptied
			 * the TX chain.
			 */
			fp->watchdog_timer = 0;
		} else {
			/*
			 * Reset the watchdog timer if we still have
			 * transmits pending.
			 */
			fp->watchdog_timer = BXE_TX_TIMEOUT;
		}
	}

	/* Save our indices. */
	fp->tx_pkt_cons = sw_pkt_cons;
	fp->tx_bd_cons = sw_tx_bd_cons;
	DBEXIT(BXE_EXTREME_SEND);
}

/*
 * Encapsulate an mbuf cluster into the rx_bd.
 *
 * This routine will map an mbuf cluster into 1 rx_bd
 *
 * Returns:
 *   0 for success, positive value for failure.
 */
static int
bxe_get_buf(struct bxe_fastpath *fp, struct mbuf *m, uint16_t prod)
{
	struct bxe_softc *sc;
	bus_dma_segment_t seg;
	struct mbuf *m_new;
	struct eth_rx_bd *rx_bd;
	int rc;

	sc = fp->sc;
	m_new = NULL;
	rc = 0;

	DBENTER(BXE_INSANE_LOAD | BXE_INSANE_RESET | BXE_INSANE_RECV);

	/* Make sure the inputs are valid. */
	DBRUNIF((prod > MAX_RX_BD),
	    BXE_PRINTF("%s(): RX producer out of range: 0x%04X > 0x%04X\n",
	    __FUNCTION__, prod, (uint16_t) MAX_RX_BD));

	/* Check whether this is a new mbuf allocation. */
	if (m == NULL) {
		if ((m_new = bxe_alloc_mbuf(fp, sc->mbuf_alloc_size)) == NULL) {
			rc = ENOBUFS;
			goto bxe_get_buf_exit;
		}

		DBRUN(fp->rx_mbuf_alloc++);
	} else {
		/* Reuse the existing mbuf. */
		m_new = m;
		m_new->m_pkthdr.len = m_new->m_len = sc->mbuf_alloc_size;
	}

	/* Do some additional sanity checks on the mbuf. */
	DBRUN(m_sanity(m_new, FALSE));

	rc = bxe_map_mbuf(fp, m_new, fp->rx_mbuf_tag,
	    fp->rx_mbuf_map[prod], &seg);

	if (__predict_false(rc)) {
		DBRUN(fp->rx_mbuf_alloc--);
		rc = ENOBUFS;
		goto bxe_get_buf_exit;
	}

	/* Setup the rx_bd for the first segment. */
	rx_bd = &fp->rx_bd_chain[RX_PAGE(prod)][RX_IDX(prod)];
	rx_bd->addr_lo  = htole32(U64_LO(seg.ds_addr));
	rx_bd->addr_hi  = htole32(U64_HI(seg.ds_addr));

	/* Save the mbuf and update our counter. */
	fp->rx_mbuf_ptr[prod] = m_new;

	DBRUN(fp->free_rx_bd--);
	DBRUNIF((fp->free_rx_bd > USABLE_RX_BD),
	    DBPRINT(sc, BXE_FATAL, "%s(): fp[%d] - Too many free rx_bd's "
	    "(0x%04X)!\n", __FUNCTION__, fp->index, fp->free_rx_bd));

bxe_get_buf_exit:
	DBEXIT(BXE_INSANE_LOAD | BXE_INSANE_RESET | BXE_INSANE_RECV);
	return (rc);
}

/*
 * Transmit timeout handler.
 *
 * Returns:
 *   0 = No timeout, !0 = timeout occurred.
 */
static int
bxe_watchdog(struct bxe_fastpath *fp)
{
	struct bxe_softc *sc = fp->sc;
	int rc = 0;

	DBENTER(BXE_INSANE_SEND);

	BXE_FP_LOCK(fp);
	if (fp->watchdog_timer == 0 || --fp->watchdog_timer) {
		rc = EINVAL;
		BXE_FP_UNLOCK(fp);
		goto bxe_watchdog_exit;
	}
	BXE_FP_UNLOCK(fp);

	BXE_PRINTF("TX watchdog timeout occurred on fp[%02d], "
	    "resetting!\n", fp->index);

	/* DBRUNLV(BXE_FATAL, bxe_breakpoint(sc)); */

	BXE_CORE_LOCK(sc);

	/* Mark the interface as down. */
	sc->bxe_ifp->if_drv_flags &= ~IFF_DRV_RUNNING;

	bxe_stop_locked(sc, UNLOAD_NORMAL);
	DELAY(10000);
	bxe_init_locked(sc, LOAD_OPEN);

	BXE_CORE_UNLOCK(sc);

bxe_watchdog_exit:
	DBEXIT(BXE_INSANE_SEND);
	return(rc);
}


/*
 * Change the MTU size for the port.  The MTU should be validated before
 * calling this routine.
 *
 * Returns:
 *   0 = Success, !0 = Failure.
 */
static int
bxe_change_mtu(struct bxe_softc *sc, int if_drv_running)
{
	struct ifnet *ifp;
	int rc;

	BXE_CORE_LOCK_ASSERT(sc);

	rc = 0;
	ifp = sc->bxe_ifp;
	sc->bxe_ifp->if_mtu = ifp->if_mtu;
	if (if_drv_running) {
		DBPRINT(sc, BXE_INFO_IOCTL, "%s(): Changing the MTU to %d.\n",
		    __FUNCTION__, sc->port.ether_mtu);

		bxe_stop_locked(sc, UNLOAD_NORMAL);
		bxe_init_locked(sc, LOAD_NORMAL);
	}

	return (rc);
}

/*
 * The periodic timer tick routine.
 *
 * This code only runs when the interface is up.
 *
 * Returns:
 *   None
 */
static void
bxe_tick(void *xsc)
{
	struct bxe_softc *sc;
	struct bxe_fastpath *fp;
#if 0
	/* Re-enable at a later time. */
	uint32_t drv_pulse, mcp_pulse;
#endif
	int i, func;

	sc = xsc;
	DBENTER(BXE_INSANE_MISC);

	/* Check for TX timeouts on any fastpath. */
	for (i = 0; i < sc->num_queues; i++) {
		fp = &sc->fp[i];
		if (bxe_watchdog(fp) != 0)
			break;
	}

	BXE_CORE_LOCK(sc);
	func = BP_FUNC(sc);

	/* Schedule the next tick. */
	callout_reset(&sc->bxe_tick_callout, hz, bxe_tick, sc);

#if 0
	if (!BP_NOMCP(sc)) {
		func = BP_FUNC(sc);

		++sc->fw_drv_pulse_wr_seq;
		sc->fw_drv_pulse_wr_seq &= DRV_PULSE_SEQ_MASK;

		/* Let the MCP know we're alive. */
		drv_pulse = sc->fw_drv_pulse_wr_seq;
		SHMEM_WR(sc, func_mb[func].drv_pulse_mb, drv_pulse);

		/* Check if the MCP is still alive. */
		mcp_pulse = (SHMEM_RD(sc, func_mb[func].mcp_pulse_mb) &
		    MCP_PULSE_SEQ_MASK);

		/*
		 * The delta between driver pulse and MCP response should be 1
		 * (before MCP response) or 0 (after MCP response).
		 */
		if ((drv_pulse != mcp_pulse) && (drv_pulse != ((mcp_pulse + 1) &
		    MCP_PULSE_SEQ_MASK))) {
			/* Someone's in cardiac arrest. */
			DBPRINT(sc, BXE_WARN,
			    "%s(): drv_pulse (0x%x) != mcp_pulse (0x%x)\n",
			    __FUNCTION__, drv_pulse, mcp_pulse);
		}
	}
#endif

	if ((sc->state == BXE_STATE_OPEN) || (sc->state == BXE_STATE_DISABLED))
		bxe_stats_handle(sc, STATS_EVENT_UPDATE);

	BXE_CORE_UNLOCK(sc);
}

#ifdef BXE_DEBUG
/*
 * Allows the driver state to be dumped through the sysctl interface.
 *
 * Returns:
 *   0 for success, positive value for failure.
 */
static int
bxe_sysctl_driver_state(SYSCTL_HANDLER_ARGS)
{
	struct bxe_softc *sc;
	struct bxe_fastpath *fp;
	int error, i, result;

	sc = (struct bxe_softc *)arg1;
	result = -1;
	error = sysctl_handle_int(oidp, &result, 0, req);
	if (error || !req->newptr)
		return (error);

	if (result == 1) {
		bxe_dump_driver_state(sc);
		for (i = 0; i < sc->num_queues; i++) {
			fp = &sc->fp[i];
			bxe_dump_fp_state(fp);
		}
		bxe_dump_status_block(sc);
	}

	return (error);
}

/*
 * Allows the hardware state to be dumped through the sysctl interface.
 *
 * Returns:
 *   0 for success, positive value for failure.
 */
static int
bxe_sysctl_hw_state(SYSCTL_HANDLER_ARGS)
{
	struct bxe_softc *sc;
	int error, result;

	sc = (struct bxe_softc *)arg1;
	result = -1;
	error = sysctl_handle_int(oidp, &result, 0, req);
	if (error || !req->newptr)
		return (error);

	if (result == 1)
		bxe_dump_hw_state(sc);

	return (error);
}

/*
 * Allows the MCP firmware to be dumped through the sysctl interface.
 *
 * Returns:
 *   0 for success, positive value for failure.
 */
static int
bxe_sysctl_dump_fw(SYSCTL_HANDLER_ARGS)
{
	struct bxe_softc *sc;
	int error, result;

	sc = (struct bxe_softc *)arg1;
	result = -1;
	error = sysctl_handle_int(oidp, &result, 0, req);
	if (error || !req->newptr)
		return (error);

	if (result == 1)
		bxe_dump_fw(sc);

	return (error);
}

/*
 * Provides a sysctl interface to allow dumping the RX completion chain.
 *
 * Returns:
 *   0 for success, positive value for failure.
 */
static int
bxe_sysctl_dump_rx_cq_chain(SYSCTL_HANDLER_ARGS)
{
	struct bxe_softc *sc;
	struct bxe_fastpath *fp;
	int error, result;

	sc = (struct bxe_softc *)arg1;
	result = -1;
	error = sysctl_handle_int(oidp, &result, 0, req);
	if (error || !req->newptr)
		return (error);

	if ((result >= 0) && (result < sc->num_queues)) {
		fp = &sc->fp[result];
		bxe_dump_rx_cq_chain(fp, 0, TOTAL_RCQ_ENTRIES);
	}

	return (error);
}


/*
 * Provides a sysctl interface to allow dumping the RX chain.
 *
 * Returns:
 *   0 for success, positive value for failure.
 */
static int
bxe_sysctl_dump_rx_bd_chain(SYSCTL_HANDLER_ARGS)
{
	struct bxe_softc *sc;
	struct bxe_fastpath *fp;
	int error, result;

	sc = (struct bxe_softc *)arg1;
	result = -1;
	error = sysctl_handle_int(oidp, &result, 0, req);
	if (error || !req->newptr)
		return (error);

	if ((result >= 0) && (result < sc->num_queues)) {
		fp = &sc->fp[result];
		bxe_dump_rx_bd_chain(fp, 0, TOTAL_RX_BD);
	}

	return (error);
}

/*
* Provides a sysctl interface to allow dumping the TX chain.
*
* Returns:
*   0 for success, positive value for failure.
*/
static int
bxe_sysctl_dump_tx_chain(SYSCTL_HANDLER_ARGS)
{
	struct bxe_softc *sc;
	struct bxe_fastpath *fp;
	int error, result;

	sc = (struct bxe_softc *)arg1;
	result = -1;
	error = sysctl_handle_int(oidp, &result, 0, req);
	if (error || !req->newptr)
		return (error);

	if ((result >= 0) && (result < sc->num_queues)) {
		fp = &sc->fp[result];
		bxe_dump_tx_chain(fp, 0, TOTAL_TX_BD);
	}

	return (error);
}

/*
 * Provides a sysctl interface to allow reading arbitrary registers in the
 * device.  DO NOT ENABLE ON PRODUCTION SYSTEMS!
 *
 * Returns:
 *   0 for success, positive value for failure.
 */
static int
bxe_sysctl_reg_read(SYSCTL_HANDLER_ARGS)
{
	struct bxe_softc *sc;
	uint32_t result, val;
	int error;

	sc = (struct bxe_softc *)arg1;
	result = -1;
	error = sysctl_handle_int(oidp, &result, 0, req);
	if (error || (req->newptr == NULL))
		return (error);

	val = REG_RD(sc, result);
	BXE_PRINTF("reg 0x%08X = 0x%08X\n", result, val);

	return (error);
}

/*
* Provides a sysctl interface to allow generating a grcdump.
*
* Returns:
*   0 for success, positive value for failure.
*/
static int
bxe_sysctl_grcdump(SYSCTL_HANDLER_ARGS)
{
	struct bxe_softc *sc;
	int error, result;

	sc = (struct bxe_softc *)arg1;
	result = -1;
	error = sysctl_handle_int(oidp, &result, 0, req);
	if (error || !req->newptr)
		return (error);

	if (result == 1) {
		/* Generate a grcdump and log the contents.*/
		bxe_grcdump(sc, 1);
	} else {
		/* Generate a grcdump and don't log the contents. */
		bxe_grcdump(sc, 0);
	}

	return (error);
}

/*
 * Provides a sysctl interface to forcing the driver to dump state and
 * enter the debugger.  DO NOT ENABLE ON PRODUCTION SYSTEMS!
 *
 * Returns:
 *   0 for success, positive value for failure.
 */
static int
bxe_sysctl_breakpoint(SYSCTL_HANDLER_ARGS)
{
	struct bxe_softc *sc;
	int error, result;

	result = -1;
	error = sysctl_handle_int(oidp, &result, 0, req);
	if (error || !req->newptr)
		return (error);

	if (result == 1) {
		sc = (struct bxe_softc *)arg1;
		bxe_breakpoint(sc);
	}

	return (error);
}
#endif

/*
 * Adds any sysctl parameters for tuning or debugging purposes.
 *
 * Returns:
 *   None.
 */
static void
bxe_add_sysctls(struct bxe_softc *sc)
{
	struct sysctl_ctx_list *ctx =
	    device_get_sysctl_ctx(sc->dev);
	struct sysctl_oid_list *children =
	    SYSCTL_CHILDREN(device_get_sysctl_tree(sc->dev));
	struct bxe_eth_stats *estats = &sc->eth_stats;

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO,
	    "estats_total_bytes_received_hi",
	    CTLFLAG_RD, &estats->total_bytes_received_hi,
	    0, "Total bytes received (hi)");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO,
	    "estats_total_bytes_received_lo",
	    CTLFLAG_RD, &estats->total_bytes_received_lo,
	    0, "Total bytes received (lo)");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO,
	   "estats_valid_bytes_received_hi",
	   CTLFLAG_RD, &estats->valid_bytes_received_hi,
	   0, "Valid bytes received (hi)");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO,
	    "estats_valid_bytes_received_lo",
	    CTLFLAG_RD, &estats->valid_bytes_received_lo,
	    0, "Valid bytes received (lo)");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO,
	    "estats_total_unicast_packets_received_hi",
	    CTLFLAG_RD, &estats->total_unicast_packets_received_hi,
	    0, "Total unicast packets received (hi)");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO,
	    "estats_total_unicast_packets_received_lo",
	    CTLFLAG_RD, &estats->total_unicast_packets_received_lo,
	    0, "Total unicast packets received (lo)");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO,
	    "estats_total_bytes_transmitted_hi",
	    CTLFLAG_RD, &estats->total_bytes_transmitted_hi,
	    0, "Total bytes transmitted (hi)");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO,
	    "estats_total_bytes_transmitted_lo",
	    CTLFLAG_RD, &estats->total_bytes_transmitted_lo,
	    0, "Total bytes transmitted (lo)");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO,
	    "estats_total_unicast_packets_transmitted_hi",
	    CTLFLAG_RD, &estats->total_unicast_packets_transmitted_hi,
	    0, "Total unicast packets transmitted (hi)");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO,
	    "estats_total_unicast_packets_transmitted_lo",
	    CTLFLAG_RD, &estats->total_unicast_packets_transmitted_lo,
	    0, "Total unicast packets transmitted (lo)");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO,
	    "estats_total_broadcast_packets_received_lo",
	    CTLFLAG_RD, &estats->total_broadcast_packets_received_lo,
	    0, "Total broadcast packets received (lo)");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO,
	    "estats_total_broadcast_packets_transmitted_lo",
	    CTLFLAG_RD, &estats->total_broadcast_packets_transmitted_lo,
	    0, "Total broadcast packets transmitted (lo)");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO,
	    "estats_total_multicast_packets_received_lo",
	    CTLFLAG_RD, &estats->total_multicast_packets_received_lo,
	    0, "Total multicast packets received (lo)");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO,
	    "estats_total_multicast_packets_transmitted_lo",
	    CTLFLAG_RD, &estats->total_multicast_packets_transmitted_lo,
	    0, "Total multicast packets transmitted (lo)");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO,
	    "tx_stat_etherstatspkts64octets_hi",
	    CTLFLAG_RD, &estats->tx_stat_etherstatspkts64octets_hi,
	    0, "Total 64 byte packets transmitted (hi)");

	/* ToDo: Fix for 64 bit access. */
	SYSCTL_ADD_UINT(ctx, children, OID_AUTO,
	    "tx_stat_etherstatspkts64octets_lo",
	    CTLFLAG_RD, &estats->tx_stat_etherstatspkts64octets_lo,
	    0, "Total 64 byte packets transmitted (lo)");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO,
	    "driver_xoff",
	    CTLFLAG_RD, &estats->driver_xoff,
	    0, "Driver transmit queue full count");

	SYSCTL_ADD_ULONG(ctx, children, OID_AUTO,
	    "tx_start_called_with_link_down",
	    CTLFLAG_RD, &sc->tx_start_called_with_link_down,
	    "TX start routine called while link down count");

	SYSCTL_ADD_ULONG(ctx, children, OID_AUTO,
	    "tx_start_called_with_queue_full",
	    CTLFLAG_RD, &sc->tx_start_called_with_queue_full,
	    "TX start routine called with queue full count");

	/* ToDo: Add more statistics here. */

#ifdef BXE_DEBUG
	SYSCTL_ADD_INT(ctx, children, OID_AUTO, "bxe_debug",
	    CTLFLAG_RW, &bxe_debug, 0,
	    "Debug message level flag");
#endif

	do {
#define QUEUE_NAME_LEN 32
		char namebuf[QUEUE_NAME_LEN];
		struct sysctl_oid *queue_node;
		struct sysctl_oid_list *queue_list;

		for (int i = 0; i < sc->num_queues; i++) {
			struct bxe_fastpath *fp	= &sc->fp[i];
			snprintf(namebuf, QUEUE_NAME_LEN, "fp[%02d]", i);

			queue_node = SYSCTL_ADD_NODE(ctx, children, OID_AUTO,
			    namebuf, CTLFLAG_RD, NULL, "Queue Name");
			queue_list = SYSCTL_CHILDREN(queue_node);

			SYSCTL_ADD_ULONG(ctx, queue_list, OID_AUTO,
			    "rx_pkts",
			    CTLFLAG_RD, &fp->rx_pkts,
			    "Received packets");

			SYSCTL_ADD_ULONG(ctx, queue_list, OID_AUTO,
			    "tx_pkts",
			    CTLFLAG_RD, &fp->tx_pkts,
			    "Transmitted packets");

			SYSCTL_ADD_ULONG(ctx, queue_list, OID_AUTO,
			    "mbuf_alloc_failed",
			    CTLFLAG_RD, &fp->mbuf_alloc_failed,
			    "Mbuf allocation failure count");

			SYSCTL_ADD_ULONG(ctx, queue_list, OID_AUTO,
			    "tpa_mbuf_alloc_failed",
			    CTLFLAG_RD, &fp->tpa_mbuf_alloc_failed,
			    "TPA mbuf allocation failure count");

			SYSCTL_ADD_ULONG(ctx, queue_list, OID_AUTO,
			    "mbuf_defrag_attempts",
			    CTLFLAG_RD, &fp->mbuf_defrag_attempts,
			    "Mbuf defrag attempt count");

			SYSCTL_ADD_ULONG(ctx, queue_list, OID_AUTO,
			    "mbuf_defrag_failures",
			    CTLFLAG_RD, &fp->mbuf_defrag_failures,
			    "Mbuf defrag failure count");

			SYSCTL_ADD_ULONG(ctx, queue_list, OID_AUTO,
			    "mbuf_defrag_successes",
			    CTLFLAG_RD, &fp->mbuf_defrag_successes,
			    "Mbuf defrag success count");

			SYSCTL_ADD_ULONG(ctx, queue_list, OID_AUTO,
			    "offload_frames_csum_ip",
			    CTLFLAG_RD, &fp->offload_frames_csum_ip,
			    "IP checksum offload frame count");

			SYSCTL_ADD_ULONG(ctx, queue_list, OID_AUTO,
			    "offload_frames_csum_tcp",
			    CTLFLAG_RD, &fp->offload_frames_csum_tcp,
			    "TCP checksum offload frame count");

			SYSCTL_ADD_ULONG(ctx, queue_list, OID_AUTO,
			    "offload_frames_csum_udp",
			    CTLFLAG_RD, &fp->offload_frames_csum_udp,
			    "UDP checksum offload frame count");

			SYSCTL_ADD_ULONG(ctx, queue_list, OID_AUTO,
			    "offload_frames_tso",
			    CTLFLAG_RD, &fp->offload_frames_tso,
			    "TSO offload frame count");

			SYSCTL_ADD_ULONG(ctx, queue_list, OID_AUTO,
			    "tx_encap_failures",
			    CTLFLAG_RD, &fp->tx_encap_failures,
			    "TX encapsulation failure count");

			SYSCTL_ADD_ULONG(ctx, queue_list, OID_AUTO,
			    "tx_start_called_on_empty_queue",
			    CTLFLAG_RD, &fp->tx_start_called_on_empty_queue,
			    "TX start function called on empty "
			    "TX queue count");

			SYSCTL_ADD_ULONG(ctx, queue_list, OID_AUTO,
			    "tx_queue_too_full",
			    CTLFLAG_RD, &fp->tx_queue_too_full,
			    "TX queue too full to add a TX frame count");

			SYSCTL_ADD_ULONG(ctx, queue_list, OID_AUTO,
			    "window_violation_std",
			    CTLFLAG_RD, &fp->window_violation_std,
			    "Standard frame TX BD window violation count");

			SYSCTL_ADD_ULONG(ctx, queue_list, OID_AUTO,
			    "window_violation_tso",
			    CTLFLAG_RD, &fp->window_violation_tso,
			    "TSO frame TX BD window violation count");

			SYSCTL_ADD_ULONG(ctx, queue_list, OID_AUTO,
			    "unsupported_tso_request_ipv6",
			    CTLFLAG_RD, &fp->unsupported_tso_request_ipv6,
			    "TSO frames with unsupported IPv6 protocol count");

			SYSCTL_ADD_ULONG(ctx, queue_list, OID_AUTO,
			    "unsupported_tso_request_not_tcp",
			    CTLFLAG_RD, &fp->unsupported_tso_request_not_tcp,
			    "TSO frames with unsupported protocol count");

			SYSCTL_ADD_ULONG(ctx, queue_list, OID_AUTO,
			    "tx_chain_lost_mbuf",
			    CTLFLAG_RD, &fp->tx_chain_lost_mbuf,
			    "Mbufs lost on TX chain count");
#ifdef BXE_DEBUG
			SYSCTL_ADD_ULONG(ctx, queue_list, OID_AUTO,
			    "null_cqe_flags",
			    CTLFLAG_RD, &fp->null_cqe_flags,
			    "CQEs with NULL flags count");
#endif
		}
	} while (0);


#ifdef BXE_DEBUG
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "driver_state",
	    CTLTYPE_INT | CTLFLAG_RW, (void *)sc, 0,
	    bxe_sysctl_driver_state, "I", "Drive state information");

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "hw_state",
	    CTLTYPE_INT | CTLFLAG_RW, (void *)sc, 0,
	    bxe_sysctl_hw_state, "I", "Hardware state information");

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "dump_fw",
	    CTLTYPE_INT | CTLFLAG_RW, (void *)sc, 0,
	    bxe_sysctl_dump_fw,	"I", "Dump MCP firmware");

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "dump_rx_bd_chain",
	    CTLTYPE_INT | CTLFLAG_RW, (void *)sc, 0,
	    bxe_sysctl_dump_rx_bd_chain, "I", "Dump rx_bd chain");

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "dump_rx_cq_chain",
	    CTLTYPE_INT | CTLFLAG_RW, (void *)sc, 0,
	    bxe_sysctl_dump_rx_cq_chain, "I", "Dump cqe chain");

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "dump_tx_chain",
	    CTLTYPE_INT | CTLFLAG_RW, (void *)sc, 0,
	    bxe_sysctl_dump_tx_chain, "I", "Dump tx_bd chain");

	/*
	 * Generates a GRCdump (run sysctl dev.bxe.0.grcdump=0
	 * before accessing buffer below).
	 */
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "grcdump",
	    CTLTYPE_INT | CTLFLAG_RW, (void *)sc, 0, bxe_sysctl_grcdump,
	    "I", "Initiate a grcdump operation");

	/*
	 * Hidden sysctl.
	 *  Use "sysctl -b dev.bxe.0.grcdump_buffer > buf.bin".
	 */
	SYSCTL_ADD_OPAQUE(ctx, children, OID_AUTO, "grcdump_buffer",
	    CTLFLAG_RD | CTLFLAG_SKIP, sc->grcdump_buffer,
	    BXE_GRCDUMP_BUF_SIZE, "IU", "Access grcdump buffer");

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "breakpoint",
	    CTLTYPE_INT | CTLFLAG_RW, (void *)sc, 0,
	    bxe_sysctl_breakpoint, "I", "Driver breakpoint");

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "reg_read",
	    CTLTYPE_INT | CTLFLAG_RW, (void *)sc, 0,
	    bxe_sysctl_reg_read, "I", "Register read");

#endif /* BXE_DEBUG */
}

/*
 * BXE Debug Routines
 */
#ifdef BXE_DEBUG
/*
 * Writes out the header for the debug dump buffer.
 *
 * Returns:
 * 	 None.
 *
 * Modifies:
 *   index
 */
static void
bxe_dump_debug_header(struct bxe_softc *sc, uint32_t *index)
{
	struct hd_param hd_param_cu = {0};
	uint32_t *buf;

	buf = sc->grcdump_buffer;
	if (CHIP_IS_E1H(sc))
		hd_param_cu = hd_param_e1h;
	else
		hd_param_cu = hd_param_e1;

	buf[(*index)++] = hd_param_cu.time_stamp;
	buf[(*index)++] = hd_param_cu.diag_ver;
	buf[(*index)++] = hd_param_cu.grc_dump_ver;

	buf[(*index)++] = REG_RD_IND(sc, XSTORM_WAITP_ADDRESS);
	buf[(*index)++] = REG_RD_IND(sc, TSTORM_WAITP_ADDRESS);
	buf[(*index)++] = REG_RD_IND(sc, USTORM_WAITP_ADDRESS);
	buf[(*index)++] = REG_RD_IND(sc, CSTORM_WAITP_ADDRESS);

	/* The size of the header is stored at the first DWORD. */
	buf[0] = (*index) - 1;
}


/*
 * Writes to the controller to prepare it for a dump.
 *
 * Returns:
 * 	 None.
 *
 * Modifies:
 *   None.
 */
static void
bxe_dump_debug_writes(struct bxe_softc *sc)
{
	uint32_t write_val;

	write_val = 1;
	/* Halt the STORMs to get a consistent device state. */
	REG_WR_IND(sc, XSTORM_WAITP_ADDRESS, write_val);
	REG_WR_IND(sc, TSTORM_WAITP_ADDRESS, write_val);
	REG_WR_IND(sc, USTORM_WAITP_ADDRESS, write_val);
	REG_WR_IND(sc, CSTORM_WAITP_ADDRESS, write_val);

	if (CHIP_IS_E1H(sc))
		REG_WR_IND(sc, TSTORM_CAM_MODE, write_val);
}


/*
 * Cycles through the required register reads and dumps them
 * to the debug buffer.
 *
 * Returns:
 * 	 None.
 *
 * Modifies:
 *   index
 */
static void
bxe_dump_debug_reg_read(struct bxe_softc *sc, uint32_t *index)
{
	preg_addr preg_addrs;
	uint32_t regs_count, *buf;
	uint32_t i, reg_addrs_index;

	buf = sc->grcdump_buffer;
	preg_addrs = NULL;

	/* Read different registers for different controllers. */
	if (CHIP_IS_E1H(sc)) {
		regs_count = regs_count_e1h;
		preg_addrs = &reg_addrs_e1h[0];
	} else {
		regs_count = regs_count_e1;
		preg_addrs = &reg_addrs_e1[0];
	}

	/* ToDo: Add a buffer size check. */
	for (reg_addrs_index = 0; reg_addrs_index < regs_count;
	    reg_addrs_index++) {
		for (i = 0; i < preg_addrs[reg_addrs_index].size; i++) {
			buf[(*index)++] = REG_RD_IND(sc,
			    preg_addrs[reg_addrs_index].addr + (i * 4));
		}
	}
}

/*
 * Cycles through the required wide register reads and dumps them
 * to the debug buffer.
 *
 * Returns:
 *   None.
 */
static void
bxe_dump_debug_reg_wread(struct bxe_softc *sc, uint32_t *index)
{
	pwreg_addr pwreg_addrs;
	uint32_t reg_addrs_index, reg_add_read,	reg_add_count;
	uint32_t *buf, cam_index, wregs_count;

	buf = sc->grcdump_buffer;
	pwreg_addrs = NULL;

	/* Read different registers for different controllers. */
    if (CHIP_IS_E1H(sc)) {
        wregs_count = wregs_count_e1h;
        pwreg_addrs = &wreg_addrs_e1h[0];
    } else {
        wregs_count = wregs_count_e1;
        pwreg_addrs = &wreg_addrs_e1[0];
    }

	for (reg_addrs_index = 0; reg_addrs_index < wregs_count;
	    reg_addrs_index++) {
		reg_add_read = pwreg_addrs[reg_addrs_index].addr;
		for (reg_add_count = 0; reg_add_count <
		    pwreg_addrs[reg_addrs_index].size; reg_add_count++) {
			buf[(*index)++] = REG_RD_IND(sc, reg_add_read);
			reg_add_read += sizeof(uint32_t);

			for (cam_index = 0; cam_index <
			    pwreg_addrs[reg_addrs_index].const_regs_count;
			    cam_index++)
				buf[(*index)++] = REG_RD_IND(sc,
				    pwreg_addrs[reg_addrs_index].const_regs[cam_index]);
		}
	}
}

/*
 * Performs a debug dump for offline diagnostics.
 *
 * Note that when this routine is called the STORM
 * processors will be stopped in order to create a
 * cohesive dump.  The controller will need to be
 * reset before the device can begin passing traffic
 * again.
 *
 * Returns:
 *   None.
 */
static void
bxe_grcdump(struct bxe_softc *sc, int log)
{
	uint32_t *buf, i, index;

	index = 1;
	buf = sc->grcdump_buffer;
	if (buf != NULL) {

		/* Write the header and regsiters contents to the dump buffer. */
		bxe_dump_debug_header(sc, &index);
		bxe_dump_debug_writes(sc);
		bxe_dump_debug_reg_read(sc,&index);
		bxe_dump_debug_reg_wread(sc, &index);

		/* Print the results to the system log is necessary. */
		if (log) {
			BXE_PRINTF(
			    "-----------------------------"
			    "    grcdump   "
			    "-----------------------------\n");
			BXE_PRINTF("Buffer length = 0x%08X bytes\n", index * 4);

			for (i = 0; i < index; i += 8) {
				BXE_PRINTF(
				    "0x%08X - 0x%08X 0x%08X 0x%08X 0x%08X "
				    "0x%08X 0x%08X 0x%08X 0x%08X\n", i * 4,
				    buf[i + 0], buf[i + 1], buf[i + 2],
				    buf[i + 3], buf[i + 4], buf[i + 5],
				    buf[i + 6], buf[i + 7]);
			}

			BXE_PRINTF(
			    "-----------------------------"
			    "--------------"
			    "-----------------------------\n");
		}
	} else {
		BXE_PRINTF("No grcdump buffer allocated!\n");
	}
}

/*
 * Check that an Etherent frame is valid and prints out debug info if it's
 * not.
 *
 * Returns:
 *   Nothing.
 */
static __attribute__ ((noinline))
void bxe_validate_rx_packet(struct bxe_fastpath *fp, uint16_t comp_cons,
    union eth_rx_cqe *cqe, struct mbuf *m)
{
	struct bxe_softc *sc;

	sc = fp->sc;
	/* Check that the mbuf is sane. */
	m_sanity(m, FALSE);

	/* Make sure the packet has a valid length. */
	if ((m->m_len < ETHER_HDR_LEN) |
		(m->m_len > ETH_MAX_JUMBO_PACKET_SIZE + ETH_OVREHEAD)) {
		m_print(m, 128);
		bxe_dump_enet(sc, m);
		bxe_dump_cqe(fp, comp_cons, cqe);
	}
}

/*
 * Prints out Ethernet frame information from an mbuf.
 *
 * Partially decode an Ethernet frame to look at some important headers.
 *
 * Returns:
 *   Nothing.
 */
static __attribute__ ((noinline))
void bxe_dump_enet(struct bxe_softc *sc, struct mbuf *m)
{
	struct ether_vlan_header *eh;
	uint16_t etype;
	int e_hlen;
	struct ip *ip;
	struct tcphdr *th;
	struct udphdr *uh;
	struct arphdr *ah;

	BXE_PRINTF(
	    "-----------------------------"
	    " Frame Decode "
	    "-----------------------------\n");

	eh = mtod(m, struct ether_vlan_header *);

	/* Handle VLAN encapsulation if present. */
	if (eh->evl_encap_proto == htons(ETHERTYPE_VLAN)) {
		etype = ntohs(eh->evl_proto);
		e_hlen = ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN;
	} else {
		etype = ntohs(eh->evl_encap_proto);
		e_hlen = ETHER_HDR_LEN;
	}

	BXE_PRINTF("enet: dest = %6D, src = %6D, type = 0x%04X, e_hlen = %d\n",
	    eh->evl_dhost, ":", eh->evl_shost, ":", etype, e_hlen);

	switch (etype) {
	case ETHERTYPE_IP:
		ip = (struct ip *)(m->m_data + e_hlen);
		BXE_PRINTF(
		    "--ip: dest = 0x%08X , src = 0x%08X, "
		    "ip_hlen = %d bytes, len = %d bytes, protocol = 0x%02X, "
		    "ip_id = 0x%04X, csum = 0x%04X\n",
		    ntohl(ip->ip_dst.s_addr), ntohl(ip->ip_src.s_addr),
		    (ip->ip_hl << 2), ntohs(ip->ip_len), ip->ip_p,
		    ntohs(ip->ip_id), ntohs(ip->ip_sum));

		switch (ip->ip_p) {
		case IPPROTO_TCP:
			th = (struct tcphdr *)((caddr_t)ip + (ip->ip_hl << 2));
			BXE_PRINTF(
			    "-tcp: dest = %d, src = %d, tcp_hlen = %d "
			    "bytes, flags = 0x%b, csum = 0x%04X\n",
			    ntohs(th->th_dport), ntohs(th->th_sport),
			    (th->th_off << 2), th->th_flags,
			    "\20\10CWR\07ECE\06URG\05ACK\04PSH\03RST\02SYN\01FIN",
			    ntohs(th->th_sum));
			break;
		case IPPROTO_UDP:
			uh = (struct udphdr *)((caddr_t)ip + (ip->ip_hl << 2));
			BXE_PRINTF(
			    "-udp: dest = %d, src = %d, udp_hlen = %d "
			    "bytes, len = %d bytes, csum = 0x%04X\n",
			    ntohs(uh->uh_dport), ntohs(uh->uh_sport),
			    (int)sizeof(struct udphdr), ntohs(uh->uh_ulen),
			    ntohs(uh->uh_sum));
			break;
		case IPPROTO_ICMP:
			BXE_PRINTF("icmp:\n");
			break;
		default:
			BXE_PRINTF("----: Other IP protocol.\n");
		}
		break;
	case ETHERTYPE_IPV6:
		/* ToDo: Add IPv6 support. */
		BXE_PRINTF("IPv6 not supported!.\n");
		break;
	case ETHERTYPE_ARP:
		BXE_PRINTF("-arp: ");
		ah = (struct arphdr *) (m->m_data + e_hlen);
		switch (ntohs(ah->ar_op)) {
		case ARPOP_REVREQUEST:
			printf("reverse ARP request\n");
			break;
		case ARPOP_REVREPLY:
			printf("reverse ARP reply\n");
			break;
		case ARPOP_REQUEST:
			printf("ARP request\n");
			break;
		case ARPOP_REPLY:
			printf("ARP reply\n");
			break;
		default:
			printf("other ARP operation\n");
		}
		break;
	default:
		BXE_PRINTF("----: Other protocol.\n");
	}

	BXE_PRINTF(
	   "-----------------------------"
	   "--------------"
	   "-----------------------------\n");
}

#if 0
static void
bxe_dump_mbuf_data(struct mbuf *m, int len)
{
	uint8_t *ptr;
	int i;

	ptr = mtod(m, uint8_t *);
	printf("\nmbuf->m_data:");
	printf("\n0x");
	for (i = 0; i < len; i++){
		if (i != 0 && i % 40 == 0)
			printf("\n0x");
		else if (i != 0 && i % 6 == 0)
			printf(" 0x");
		printf("%02x", *ptr++);
	}
	printf("\n\n");
}
#endif


/*
 * Prints out information about an mbuf.
 *
 * Returns:
 *   Nothing.
 */
static __attribute__ ((noinline))
void bxe_dump_mbuf(struct bxe_softc *sc, struct mbuf *m)
{
	if (m == NULL) {
		BXE_PRINTF("mbuf: null pointer\n");
		return;
	}

	while (m) {
		BXE_PRINTF("mbuf: %p, m_len = %d, m_flags = 0x%b, "
		    "m_data = %p\n", m, m->m_len, m->m_flags,
		    "\20\1M_EXT\2M_PKTHDR\3M_EOR\4M_RDONLY", m->m_data);

		if (m->m_flags & M_PKTHDR) {
			 BXE_PRINTF("- m_pkthdr: len = %d, flags = 0x%b, "
			    "csum_flags = %b\n", m->m_pkthdr.len,
			    m->m_flags, "\20\12M_BCAST\13M_MCAST\14M_FRAG"
			    "\15M_FIRSTFRAG\16M_LASTFRAG\21M_VLANTAG"
			    "\22M_PROMISC\23M_NOFREE",
			    m->m_pkthdr.csum_flags,
			    "\20\1CSUM_IP\2CSUM_TCP\3CSUM_UDP\4CSUM_IP_FRAGS"
			    "\5CSUM_FRAGMENT\6CSUM_TSO\11CSUM_IP_CHECKED"
			    "\12CSUM_IP_VALID\13CSUM_DATA_VALID"
			    "\14CSUM_PSEUDO_HDR");
		}

		if (m->m_flags & M_EXT) {
			BXE_PRINTF("- m_ext: %p, ext_size = %d, type = ",
			    m->m_ext.ext_buf, m->m_ext.ext_size);
			switch (m->m_ext.ext_type) {
			case EXT_CLUSTER:
				printf("EXT_CLUSTER\n"); break;
			case EXT_SFBUF:
				printf("EXT_SFBUF\n"); break;
			case EXT_JUMBO9:
				printf("EXT_JUMBO9\n"); break;
			case EXT_JUMBO16:
				printf("EXT_JUMBO16\n"); break;
			case EXT_PACKET:
				printf("EXT_PACKET\n"); break;
			case EXT_MBUF:
				printf("EXT_MBUF\n"); break;
			case EXT_NET_DRV:
				printf("EXT_NET_DRV\n"); break;
			case EXT_MOD_TYPE:
				printf("EXT_MOD_TYPE\n"); break;
			case EXT_DISPOSABLE:
				printf("EXT_DISPOSABLE\n"); break;
			case EXT_EXTREF:
				printf("EXT_EXTREF\n"); break;
			default:
				printf("UNKNOWN\n");
			}
		}

		m = m->m_next;
	}
}

/*
 * Prints out information about an rx_bd.
 *
 * Returns:
 *   Nothing.
 */
static __attribute__ ((noinline))
void bxe_dump_rxbd(struct bxe_fastpath *fp, int idx,
    struct eth_rx_bd *rx_bd)
{
	struct bxe_softc *sc = fp->sc;

	/* Check if index out of range. */
	if (idx > MAX_RX_BD) {
		BXE_PRINTF("fp[%02d].rx_bd[0x%04X] XX: Invalid rx_bd index!\n",
		    fp->index, idx);
	} else if ((idx & RX_DESC_MASK) >= USABLE_RX_BD_PER_PAGE) {
		/* RX Chain page pointer. */
		BXE_PRINTF("fp[%02d].rx_bd[0x%04X] NP: haddr=0x%08X:%08X\n",
		    fp->index, idx, rx_bd->addr_hi, rx_bd->addr_lo);
	} else {
		BXE_PRINTF("fp[%02d].rx_bd[0x%04X] RX: haddr=0x%08X:%08X\n",
		    fp->index, idx, rx_bd->addr_hi, rx_bd->addr_lo);
	}
}

/*
 * Prints out a completion queue entry.
 *
 * Returns:
 *   Nothing.
 */
static __attribute__ ((noinline))
void bxe_dump_cqe(struct bxe_fastpath *fp, int idx,
    union eth_rx_cqe *cqe)
{
	struct bxe_softc *sc = fp->sc;

	if (idx > MAX_RCQ_ENTRIES) {
		/* Index out of range. */
		BXE_PRINTF("fp[%02d].rx_cqe[0x%04X]: Invalid rx_cqe index!\n",
		    fp->index, idx);
	} else if ((idx & USABLE_RCQ_ENTRIES_PER_PAGE) ==
	    USABLE_RCQ_ENTRIES_PER_PAGE) {
		/* CQE next page pointer. */
		BXE_PRINTF("fp[%02d].rx_cqe[0x%04X] NP: haddr=0x%08X:%08X\n",
		    fp->index, idx,
		    le32toh(cqe->next_page_cqe.addr_hi),
		    le32toh(cqe->next_page_cqe.addr_lo));
	} else {
		/* Normal CQE. */
		BXE_PRINTF("fp[%02d].rx_cqe[0x%04X] CQ: error_flags=0x%b, "
		    "pkt_len=0x%04X, status_flags=0x%02X, vlan=0x%04X "
		    "rss_hash=0x%08X\n", fp->index, idx,
		    cqe->fast_path_cqe.type_error_flags,
		    BXE_ETH_FAST_PATH_RX_CQE_ERROR_FLAGS_PRINTFB,
		    le16toh(cqe->fast_path_cqe.pkt_len),
		    cqe->fast_path_cqe.status_flags,
		    le16toh(cqe->fast_path_cqe.vlan_tag),
		    le32toh(cqe->fast_path_cqe.rss_hash_result));
	}
}

/*
 * Prints out information about a TX parsing BD.
 *
 * Returns:
 *   Nothing.
 */
static __attribute__ ((noinline))
void bxe_dump_tx_parsing_bd(struct bxe_fastpath *fp, int idx,
    struct eth_tx_parse_bd *p_bd)
{
    struct bxe_softc *sc = fp->sc;

    if (idx > MAX_TX_BD){
	/* Index out of range. */
	BXE_PRINTF("fp[%02d].tx_bd[0x%04X] XX: Invalid tx_bd index!\n",
	    fp->index, idx);
    } else {
	BXE_PRINTF("fp[%02d]:tx_bd[0x%04X] PB: global_data=0x%b, "
		"tcp_flags=0x%b, ip_hlen=%04d, total_hlen=%04d, "
		"tcp_pseudo_csum=0x%04X, lso_mss=0x%04X, ip_id=0x%04X, "
		"tcp_send_seq=0x%08X\n", fp->index, idx,
		p_bd->global_data, BXE_ETH_TX_PARSE_BD_GLOBAL_DATA_PRINTFB,
		p_bd->tcp_flags, BXE_ETH_TX_PARSE_BD_TCP_FLAGS_PRINTFB,
		p_bd->ip_hlen, p_bd->total_hlen, p_bd->tcp_pseudo_csum,
		p_bd->lso_mss, p_bd->ip_id, p_bd->tcp_send_seq);
    }
}

/*
 * Prints out information about a tx_bd.
 *
 * Returns:
 *   Nothing.
 */
static __attribute__ ((noinline))
void bxe_dump_txbd(struct bxe_fastpath *fp, int idx,
    union eth_tx_bd_types *tx_bd)
{
	struct bxe_softc *sc = fp->sc;

	if (idx > MAX_TX_BD){
		/* Index out of range. */
		BXE_PRINTF("fp[%02d]:tx_bd[0x%04X] XX: Invalid tx_bd index!\n",
		    fp->index, idx);
	} else if ((idx & USABLE_TX_BD_PER_PAGE) == USABLE_TX_BD_PER_PAGE) {
		/* TX next page BD. */
		BXE_PRINTF("fp[%02d]:tx_bd[0x%04X] NP: haddr=0x%08X:%08X\n",
		    fp->index, idx,	tx_bd->next_bd.addr_hi,
		    tx_bd->next_bd.addr_lo);
	} else if ((tx_bd->start_bd.bd_flags.as_bitfield &
	    ETH_TX_BD_FLAGS_START_BD) != 0) {
		/* TX start BD. */
		BXE_PRINTF("fp[%02d]:tx_bd[0x%04X] ST: haddr=0x%08X:%08X, "
		    "nbd=%02d, nbytes=%05d, vlan/idx=0x%04X, flags=0x%b, "
		    "gendata=0x%02X\n",
		    fp->index, idx, tx_bd->start_bd.addr_hi,
		    tx_bd->start_bd.addr_lo, tx_bd->start_bd.nbd,
		    tx_bd->start_bd.nbytes, tx_bd->start_bd.vlan,
		    tx_bd->start_bd.bd_flags.as_bitfield,
		    BXE_ETH_TX_BD_FLAGS_PRINTFB,
		    tx_bd->start_bd.general_data);
	} else {
		/* Regular TX BD. */
		BXE_PRINTF("fp[%02d]:tx_bd[0x%04X] TX: haddr=0x%08X:%08X, "
		    "total_pkt_bytes=%05d, nbytes=%05d\n", fp->index, idx,
		    tx_bd->reg_bd.addr_hi, tx_bd->reg_bd.addr_lo,
		    tx_bd->reg_bd.total_pkt_bytes, tx_bd->reg_bd.nbytes);
	}
}


/*
 * Prints out the transmit chain.
 *
 * Returns:
 *   Nothing.
 */
static __attribute__ ((noinline))
void bxe_dump_tx_chain(struct bxe_fastpath * fp, int tx_bd_prod, int count)
{
	struct bxe_softc *sc = fp->sc;
	union eth_tx_bd_types *tx_bd;
	uint32_t val_hi, val_lo;
	int i, parsing_bd = 0;

	/* First some info about the tx_bd chain structure. */
	BXE_PRINTF(
	    "----------------------------"
	    "  tx_bd chain "
	    "----------------------------\n");

	val_hi = U64_HI(fp->tx_bd_chain_paddr);
	val_lo = U64_LO(fp->tx_bd_chain_paddr);
	BXE_PRINTF(
	    "0x%08X:%08X - (fp[%02d]->tx_bd_chain_paddr) TX Chain physical address\n",
	    val_hi, val_lo, fp->index);
	BXE_PRINTF(
	    "page size      = 0x%08X, tx chain pages        = 0x%08X\n",
	    (uint32_t)BCM_PAGE_SIZE, (uint32_t)NUM_TX_PAGES);
	BXE_PRINTF(
	    "tx_bd per page = 0x%08X, usable tx_bd per page = 0x%08X\n",
	    (uint32_t)TOTAL_TX_BD_PER_PAGE, (uint32_t)USABLE_TX_BD_PER_PAGE);
	BXE_PRINTF(
	    "total tx_bd    = 0x%08X\n", (uint32_t)TOTAL_TX_BD);

	BXE_PRINTF(
	    "-----------------------------"
	    "  tx_bd data  "
	    "-----------------------------\n");

	/* Now print out the tx_bd's themselves. */
	for (i = 0; i < count; i++) {
		tx_bd =
		    &fp->tx_bd_chain[TX_PAGE(tx_bd_prod)][TX_IDX(tx_bd_prod)];
		if (parsing_bd) {
			struct eth_tx_parse_bd *p_bd;
			p_bd = (struct eth_tx_parse_bd *)
			    &fp->tx_bd_chain[TX_PAGE(tx_bd_prod)][TX_IDX(tx_bd_prod)].parse_bd;
			bxe_dump_tx_parsing_bd(fp, tx_bd_prod, p_bd);
			parsing_bd = 0;
		} else {
			bxe_dump_txbd(fp, tx_bd_prod, tx_bd);
			if ((tx_bd->start_bd.bd_flags.as_bitfield &
			    ETH_TX_BD_FLAGS_START_BD) != 0)
				/*
				 * There is always a parsing BD following the
				 * tx_bd with the start bit set.
				 */
				parsing_bd = 1;
		}
		/* Don't skip next page pointers. */
		   tx_bd_prod = ((tx_bd_prod + 1) & MAX_TX_BD);
	}

	BXE_PRINTF(
	    "-----------------------------"
	    "--------------"
	    "-----------------------------\n");
}

/*
 * Prints out the receive completion queue chain.
 *
 * Returns:
 *   Nothing.
 */
static __attribute__ ((noinline))
void bxe_dump_rx_cq_chain(struct bxe_fastpath *fp, int rx_cq_prod, int count)
{
	struct bxe_softc *sc = fp->sc;
	union eth_rx_cqe *cqe;
	int i;

	/* First some info about the tx_bd chain structure. */
	BXE_PRINTF(
	   "----------------------------"
	   "   CQE  Chain   "
	   "----------------------------\n");

	for (i=0; i< NUM_RCQ_PAGES; i++) {
		BXE_PRINTF("fp[%02d]->rx_cq_chain_paddr[%d] = 0x%jX\n",
		    fp->index, i, (uintmax_t) fp->rx_cq_chain_paddr[i]);
	}

	BXE_PRINTF("page size       = 0x%08X, cq chain pages    "
	    "     = 0x%08X\n",
	    (uint32_t)BCM_PAGE_SIZE, (uint32_t) NUM_RCQ_PAGES);

	BXE_PRINTF("cqe_bd per page = 0x%08X, usable cqe_bd per "
	    "page = 0x%08X\n",
	    (uint32_t) TOTAL_RCQ_ENTRIES_PER_PAGE,
	    (uint32_t) USABLE_RCQ_ENTRIES_PER_PAGE);

	BXE_PRINTF("total cqe_bd    = 0x%08X\n",(uint32_t) TOTAL_RCQ_ENTRIES);

	/* Now the CQE entries themselves. */
	BXE_PRINTF(
	    "----------------------------"
	    "    CQE Data    "
	    "----------------------------\n");

	for (i = 0; i < count; i++) {
		cqe = (union eth_rx_cqe *)&fp->rx_cq_chain
		    [RCQ_PAGE(rx_cq_prod)][RCQ_IDX(rx_cq_prod)];
		bxe_dump_cqe(fp, rx_cq_prod, cqe);
		/* Don't skip next page pointers. */
		rx_cq_prod = ((rx_cq_prod + 1) & MAX_RCQ_ENTRIES);
	}

	BXE_PRINTF(
	    "----------------------------"
	    "--------------"
	    "----------------------------\n");
}

/*
 * Prints out the receive chain.
 *
 * Returns:
 *   Nothing.
 */
static __attribute__ ((noinline))
void bxe_dump_rx_bd_chain(struct bxe_fastpath *fp, int rx_prod, int count)
{
	struct bxe_softc *sc;
	struct eth_rx_bd *rx_bd;
	struct mbuf *m;
	int i;

	sc = fp->sc;
	/* First some info about the tx_bd chain structure. */
	BXE_PRINTF(
	    "----------------------------"
	    "  rx_bd  chain  "
	    "----------------------------\n");

	BXE_PRINTF(
	    "----- RX_BD Chain -----\n");

	BXE_PRINTF("fp[%02d]->rx_cq_chain_paddr[0] = 0x%jX\n",
	    fp->index, (uintmax_t) fp->rx_cq_chain_paddr[0]);

	BXE_PRINTF(
	    "page size = 0x%08X, rx chain pages = 0x%08X\n",
	    (uint32_t)BCM_PAGE_SIZE, (uint32_t)NUM_RX_PAGES);

	BXE_PRINTF(
	    "rx_bd per page = 0x%08X, usable rx_bd per page = 0x%08X\n",
	    (uint32_t)TOTAL_RX_BD_PER_PAGE, (uint32_t)USABLE_RX_BD_PER_PAGE);

	BXE_PRINTF(
	    "total rx_bd = 0x%08X\n", (uint32_t)TOTAL_RX_BD);

	/* Now the rx_bd entries themselves. */
	BXE_PRINTF(
	    "----------------------------"
	    "   rx_bd data   "
	    "----------------------------\n");

	/* Now print out the rx_bd's themselves. */
	for (i = 0; i < count; i++) {
		rx_bd = (struct eth_rx_bd *)
		    (&fp->rx_bd_chain[RX_PAGE(rx_prod)][RX_IDX(rx_prod)]);
		m = sc->fp->rx_mbuf_ptr[rx_prod];

		bxe_dump_rxbd(fp, rx_prod, rx_bd);
		bxe_dump_mbuf(sc, m);

		/* Don't skip next page pointers. */
		rx_prod = ((rx_prod + 1) & MAX_RX_BD);
	}

	BXE_PRINTF(
	    "----------------------------"
	    "--------------"
	    "----------------------------\n");
}

/*
 * Prints out a register dump.
 *
 * Returns:
 *   Nothing.
 */
static __attribute__ ((noinline))
void bxe_dump_hw_state(struct bxe_softc *sc)
{
	int i;

	BXE_PRINTF(
		"----------------------------"
		" Hardware State "
		"----------------------------\n");

	for (i = 0x2000; i < 0x10000; i += 0x10)
		BXE_PRINTF("0x%04X: 0x%08X 0x%08X 0x%08X 0x%08X\n", i,
		    REG_RD(sc, 0 + i), REG_RD(sc, 0 + i + 0x4),
		    REG_RD(sc, 0 + i + 0x8), REG_RD(sc, 0 + i + 0xC));

	BXE_PRINTF(
	    "----------------------------"
	    "----------------"
	    "----------------------------\n");
}

/*
 * Prints out the RX mbuf chain.
 *
 * Returns:
 *   Nothing.
 */
static __attribute__ ((noinline))
void bxe_dump_rx_mbuf_chain(struct bxe_softc *sc, int chain_prod, int count)
{
	struct mbuf *m;
	int i;

	BXE_PRINTF(
	    "----------------------------"
	    "  rx mbuf data  "
	    "----------------------------\n");

	for (i = 0; i < count; i++) {
	 	m = sc->fp->rx_mbuf_ptr[chain_prod];
		BXE_PRINTF("rxmbuf[0x%04X]\n", chain_prod);
		bxe_dump_mbuf(sc, m);
		chain_prod = RX_BD(NEXT_RX_BD(chain_prod));
	}

	BXE_PRINTF(
	    "----------------------------"
	    "----------------"
	    "----------------------------\n");
}

/*
 * Prints out the mbufs in the TX mbuf chain.
 *
 * Returns:
 *   Nothing.
 */
static __attribute__ ((noinline))
void bxe_dump_tx_mbuf_chain(struct bxe_softc *sc, int chain_prod, int count)
{
	struct mbuf *m;
	int i;

	BXE_PRINTF(
	    "----------------------------"
	    "  tx mbuf data  "
	    "----------------------------\n");

	for (i = 0; i < count; i++) {
	 	m = sc->fp->tx_mbuf_ptr[chain_prod];
		BXE_PRINTF("txmbuf[%d]\n", chain_prod);
		bxe_dump_mbuf(sc, m);
		chain_prod = TX_BD(NEXT_TX_BD(chain_prod));
	}

	BXE_PRINTF(
	    "----------------------------"
	    "----------------"
	    "----------------------------\n");
}

/*
 * Prints out the status block from host memory.
 *
 * Returns:
 *   Nothing.
 */
static __attribute__ ((noinline))
void bxe_dump_status_block(struct bxe_softc *sc)
{
	struct bxe_fastpath *fp;
	struct host_def_status_block *dsb;
	struct host_status_block *fpsb;
	int i;

	dsb = sc->def_status_block;
	BXE_PRINTF(
	    "----------------------------"
	    "  Status Block  "
	    "----------------------------\n");

	for (i = 0; i < sc->num_queues; i++) {
		fp = &sc->fp[i];
		fpsb = fp->status_block;
		BXE_PRINTF(
		    "----------------------------"
		    "     fp[%02d]     "
		    "----------------------------\n", fp->index);

		/* Print the USTORM fields (HC_USTORM_SB_NUM_INDICES). */
		BXE_PRINTF(
		    "0x%08X - USTORM Flags (F/W RESERVED)\n",
		    fpsb->u_status_block.__flags);
		BXE_PRINTF(
		    "      0x%02X - USTORM PCIe Function\n",
		    fpsb->u_status_block.func);
		BXE_PRINTF(
		    "      0x%02X - USTORM Status Block ID\n",
		    fpsb->u_status_block.status_block_id);
		BXE_PRINTF(
		    "    0x%04X - USTORM Status Block Index (Tag)\n",
		    fpsb->u_status_block.status_block_index);
		BXE_PRINTF(
		    "    0x%04X - USTORM [TOE_RX_CQ_CONS]\n",
		    fpsb->u_status_block.index_values[HC_INDEX_U_TOE_RX_CQ_CONS]);
		BXE_PRINTF(
		    "    0x%04X - USTORM [ETH_RX_CQ_CONS]\n",
		    fpsb->u_status_block.index_values[HC_INDEX_U_ETH_RX_CQ_CONS]);
		BXE_PRINTF(
		    "    0x%04X - USTORM [ETH_RX_BD_CONS]\n",
		    fpsb->u_status_block.index_values[HC_INDEX_U_ETH_RX_BD_CONS]);
		BXE_PRINTF(
		    "    0x%04X - USTORM [RESERVED]\n",
		    fpsb->u_status_block.index_values[3]);

		/* Print the CSTORM fields (HC_CSTORM_SB_NUM_INDICES). */
		BXE_PRINTF(
		    "0x%08X - CSTORM Flags (F/W RESERVED)\n",
		    fpsb->c_status_block.__flags);
		BXE_PRINTF(
		    "      0x%02X - CSTORM PCIe Function\n",
		    fpsb->c_status_block.func);
		BXE_PRINTF(
		    "      0x%02X - CSTORM Status Block ID\n",
		    fpsb->c_status_block.status_block_id);
		BXE_PRINTF(
		    "    0x%04X - CSTORM Status Block Index (Tag)\n",
		    fpsb->c_status_block.status_block_index);
		BXE_PRINTF(
		    "    0x%04X - CSTORM [TOE_TX_CQ_CONS]\n",
		    fpsb->c_status_block.index_values[HC_INDEX_C_TOE_TX_CQ_CONS]);
		BXE_PRINTF(
		    "    0x%04X - CSTORM [ETH_TX_CQ_CONS]\n",
		    fpsb->c_status_block.index_values[HC_INDEX_C_ETH_TX_CQ_CONS]);
		BXE_PRINTF(
		    "    0x%04X - CSTORM [ISCSI_EQ_CONS]\n",
		    fpsb->c_status_block.index_values[HC_INDEX_C_ISCSI_EQ_CONS]);
		BXE_PRINTF(
		    "    0x%04X - CSTORM [RESERVED]\n",
		    fpsb->c_status_block.index_values[3]);
	}

	BXE_PRINTF(
	    "--------------------------"
	    "  Def Status Block  "
	    "--------------------------\n");

	/* Print attention information. */
	BXE_PRINTF(
	    "      0x%02X - Status Block ID\n",
	    dsb->atten_status_block.status_block_id);
	BXE_PRINTF(
	    "0x%08X - Attn Bits\n",
	    dsb->atten_status_block.attn_bits);
	BXE_PRINTF(
	    "0x%08X - Attn Bits Ack\n",
	    dsb->atten_status_block.attn_bits_ack);
	BXE_PRINTF(
	    "    0x%04X - Attn Block Index\n",
	    le16toh(dsb->atten_status_block.attn_bits_index));

	/* Print the USTORM fields (HC_USTORM_DEF_SB_NUM_INDICES). */
	BXE_PRINTF(
	    "      0x%02X - USTORM Status Block ID\n",
	    dsb->u_def_status_block.status_block_id);
	BXE_PRINTF(
	    "    0x%04X - USTORM Status Block Index\n",
	    le16toh(dsb->u_def_status_block.status_block_index));
	BXE_PRINTF(
	    "    0x%04X - USTORM [ETH_RDMA_RX_CQ_CONS]\n",
	    le16toh(dsb->u_def_status_block.index_values[HC_INDEX_DEF_U_ETH_RDMA_RX_CQ_CONS]));
	BXE_PRINTF(
	    "    0x%04X - USTORM [ETH_ISCSI_RX_CQ_CONS]\n",
	    le16toh(dsb->u_def_status_block.index_values[HC_INDEX_DEF_U_ETH_ISCSI_RX_CQ_CONS]));
	BXE_PRINTF(
	    "    0x%04X - USTORM [ETH_RDMA_RX_BD_CONS]\n",
	    le16toh(dsb->u_def_status_block.index_values[HC_INDEX_DEF_U_ETH_RDMA_RX_BD_CONS]));
	BXE_PRINTF(
	    "    0x%04X - USTORM [ETH_ISCSI_RX_BD_CONS]\n",
	    le16toh(dsb->u_def_status_block.index_values[HC_INDEX_DEF_U_ETH_ISCSI_RX_BD_CONS]));

	/* Print the CSTORM fields (HC_CSTORM_DEF_SB_NUM_INDICES). */
	BXE_PRINTF(
	    "      0x%02X - CSTORM Status Block ID\n",
	    dsb->c_def_status_block.status_block_id);
	BXE_PRINTF(
	    "    0x%04X - CSTORM Status Block Index\n",
	    le16toh(dsb->c_def_status_block.status_block_index));
	BXE_PRINTF(
	    "    0x%04X - CSTORM [RDMA_EQ_CONS]\n",
	    le16toh(dsb->c_def_status_block.index_values[HC_INDEX_DEF_C_RDMA_EQ_CONS]));
	BXE_PRINTF(
	    "    0x%04X - CSTORM [RDMA_NAL_PROD]\n",
	    le16toh(dsb->c_def_status_block.index_values[HC_INDEX_DEF_C_RDMA_NAL_PROD]));
	BXE_PRINTF(
	    "    0x%04X - CSTORM [ETH_FW_TX_CQ_CONS]\n",
	    le16toh(dsb->c_def_status_block.index_values[HC_INDEX_DEF_C_ETH_FW_TX_CQ_CONS]));
	BXE_PRINTF(
	    "    0x%04X - CSTORM [ETH_SLOW_PATH]\n",
	    le16toh(dsb->c_def_status_block.index_values[HC_INDEX_DEF_C_ETH_SLOW_PATH]));
	BXE_PRINTF(
	    "    0x%04X - CSTORM [ETH_RDMA_CQ_CONS]\n",
	    le16toh(dsb->c_def_status_block.index_values[HC_INDEX_DEF_C_ETH_RDMA_CQ_CONS]));
	BXE_PRINTF(
	    "    0x%04X - CSTORM [ETH_ISCSI_CQ_CONS]\n",
	    le16toh(dsb->c_def_status_block.index_values[HC_INDEX_DEF_C_ETH_ISCSI_CQ_CONS]));
	BXE_PRINTF(
	    "    0x%04X - CSTORM [UNUSED]\n",
	    le16toh(dsb->c_def_status_block.index_values[6]));
	BXE_PRINTF(
	    "    0x%04X - CSTORM [UNUSED]\n",
	    le16toh(dsb->c_def_status_block.index_values[7]));

	/* Print the TSTORM fields (HC_TSTORM_DEF_SB_NUM_INDICES). */
	BXE_PRINTF(
	    "      0x%02X - TSTORM Status Block ID\n",
	    dsb->t_def_status_block.status_block_id);
	BXE_PRINTF(
	     "    0x%04X - TSTORM Status Block Index\n",
	    le16toh(dsb->t_def_status_block.status_block_index));
	for (i = 0; i < HC_TSTORM_DEF_SB_NUM_INDICES; i++)
		BXE_PRINTF(
		    "    0x%04X - TSTORM [UNUSED]\n",
		    le16toh(dsb->t_def_status_block.index_values[i]));

	/* Print the XSTORM fields (HC_XSTORM_DEF_SB_NUM_INDICES). */
	BXE_PRINTF(
	    "      0x%02X - XSTORM Status Block ID\n",
	    dsb->x_def_status_block.status_block_id);
	BXE_PRINTF(
	    "    0x%04X - XSTORM Status Block Index\n",
	    le16toh(dsb->x_def_status_block.status_block_index));
	for (i = 0; i < HC_XSTORM_DEF_SB_NUM_INDICES; i++)
		BXE_PRINTF(
		    "    0x%04X - XSTORM [UNUSED]\n",
		    le16toh(dsb->x_def_status_block.index_values[i]));

	BXE_PRINTF(
	    "----------------------------"
	    "----------------"
	    "----------------------------\n");
}


/*
 * Prints out the statistics block from host memory.
 *
 * Returns:
 *   Nothing.
 */
static __attribute__ ((noinline))
void bxe_dump_stats_block(struct bxe_softc *sc)
{

}

/*
 * Prints out a summary of the fastpath state.
 *
 * Returns:
 *   Nothing.
 */
static __attribute__ ((noinline))
void bxe_dump_fp_state(struct bxe_fastpath *fp)
{
	struct bxe_softc *sc;
	uint32_t val_hi, val_lo;
	int i;

	sc = fp->sc;
	BXE_PRINTF(
	    "----------------------------"
	    " Fastpath State "
	    "----------------------------\n");

	val_hi = U64_HI(fp);
	val_lo = U64_LO(fp);
	BXE_PRINTF(
	    "0x%08X:%08X - (fp[%02d]) fastpath virtual address\n",
	    val_hi, val_lo, fp->index);
	BXE_PRINTF(
	    "                %3d - (fp[%02d]->sb_id)\n",
	    fp->sb_id, fp->index);
	BXE_PRINTF(
	    "                %3d - (fp[%02d]->cl_id)\n",
	    fp->cl_id, fp->index);
	BXE_PRINTF(
	    "         0x%08X - (fp[%02d]->state)\n",
	    (uint32_t)fp->state, fp->index);

	/* Receive state. */
	BXE_PRINTF(
	    "             0x%04X - (fp[%02d]->free_rx_bd)\n",
	    fp->free_rx_bd, fp->index);
	BXE_PRINTF(
	    "             0x%04X - (fp[%02d]->rx_bd_prod)\n",
	    fp->rx_bd_prod, fp->index);
	BXE_PRINTF(
	    "             0x%04X - (fp[%02d]->rx_bd_cons)\n",
	    fp->rx_bd_cons, fp->index);
	BXE_PRINTF(
	    "             0x%04X - (fp[%02d]->rx_cq_prod)\n",
	    fp->rx_cq_prod, fp->index);
	BXE_PRINTF(
	    "             0x%04X - (fp[%02d]->rx_cq_cons)\n",
	    fp->rx_cq_cons, fp->index);
	BXE_PRINTF(
	    "   %16lu - (fp[%02d]->rx_pkts)\n",
	    fp->rx_pkts, fp->index);
	BXE_PRINTF(
	    "         0x%08X - (fp[%02d]->rx_mbuf_alloc)\n",
	    fp->rx_mbuf_alloc, fp->index);
	BXE_PRINTF(
	    "   %16lu - (fp[%02d]->ipackets)\n",
	    fp->ipackets, fp->index);
	BXE_PRINTF(
	    "   %16lu - (fp[%02d]->soft_rx_errors)\n",
	    fp->soft_rx_errors, fp->index);

	/* Transmit state. */
	BXE_PRINTF(
	    "             0x%04X - (fp[%02d]->used_tx_bd)\n",
	    fp->used_tx_bd, fp->index);
	BXE_PRINTF(
	    "             0x%04X - (fp[%02d]->tx_bd_prod)\n",
	    fp->tx_bd_prod, fp->index);
	BXE_PRINTF(
	    "             0x%04X - (fp[%02d]->tx_bd_cons)\n",
	    fp->tx_bd_cons, fp->index);
	BXE_PRINTF(
	    "             0x%04X - (fp[%02d]->tx_pkt_prod)\n",
	    fp->tx_pkt_prod, fp->index);
	BXE_PRINTF(
	    "             0x%04X - (fp[%02d]->tx_pkt_cons)\n",
	    fp->tx_pkt_cons, fp->index);
	BXE_PRINTF(
	    "   %16lu - (fp[%02d]->tx_pkts)\n",
	    fp->tx_pkts, fp->index);
	BXE_PRINTF(
	    "         0x%08X - (fp[%02d]->tx_mbuf_alloc)\n",
	    fp->tx_mbuf_alloc, fp->index);
	BXE_PRINTF(
	    "   %16lu - (fp[%02d]->opackets)\n",
	    fp->opackets, fp->index);
	BXE_PRINTF(
	    "   %16lu - (fp[%02d]->soft_tx_errors)\n",
	    fp->soft_tx_errors, fp->index);

	/* TPA state. */
	if (TPA_ENABLED(sc)) {
		BXE_PRINTF(
		    "   %16lu - (fp[%02d]->tpa_pkts)\n",
		    fp->tpa_pkts, fp->index);
		BXE_PRINTF(
		    "         0x%08X - (fp[%02d]->tpa_mbuf_alloc)\n",
		    fp->tpa_mbuf_alloc, fp->index);
		BXE_PRINTF(
		    "         0x%08X - (fp[%02d]->sge_mbuf_alloc)\n",
		    fp->sge_mbuf_alloc, fp->index);

		if (CHIP_IS_E1(sc)) {
			for (i = 0; i < ETH_MAX_AGGREGATION_QUEUES_E1; i++)
				BXE_PRINTF(
			"         0x%08X - (fp[%02d]->tpa_state[%02d])\n",
				    (uint32_t)fp->tpa_state[i], fp->index, i);
		} else {
			for (i = 0; i < ETH_MAX_AGGREGATION_QUEUES_E1; i++)
				BXE_PRINTF(
			"         0x%08X - (fp[%02d]->tpa_state[%02d])\n",
				    (uint32_t)fp->tpa_state[i], fp->index, i);
		}
	}

	BXE_PRINTF(
	    "----------------------------"
	    "----------------"
	    "----------------------------\n");
}

/*
 * Returns:
 *   Nothing.
 */
static __attribute__ ((noinline))
void bxe_dump_port_state_locked(struct bxe_softc *sc)
{

	BXE_PRINTF(
	    "------------------------------"
	    " Port State "
	    "------------------------------\n");

	BXE_PRINTF(
	    "        %2d - (port) pmf\n", sc->port.pmf);
	BXE_PRINTF(
	    "0x%08X - (port) link_config\n", sc->port.link_config);
	BXE_PRINTF(
	    "0x%08X - (port) supported\n", sc->port.supported);
	BXE_PRINTF(
	    "0x%08X - (port) advertising\n", sc->port.advertising);
	BXE_PRINTF(
	    "0x%08X - (port) port_stx\n", sc->port.port_stx);

	BXE_PRINTF(
	    "----------------------------"
	    "----------------"
	    "----------------------------\n");
}

/*
 * Returns:
 *   Nothing.
 */
static __attribute__ ((noinline))
void bxe_dump_link_vars_state_locked(struct bxe_softc *sc)
{
	BXE_PRINTF(
	    "---------------------------"
	    " Link Vars State "
	    "----------------------------\n");

	switch (sc->link_vars.mac_type) {
	case MAC_TYPE_NONE:
		BXE_PRINTF("      NONE");
		break;
	case MAC_TYPE_EMAC:
		BXE_PRINTF("      EMAC");
		break;
	case MAC_TYPE_BMAC:
		BXE_PRINTF("      BMAC");
		break;
	default:
		BXE_PRINTF("      UNKN");
	}
	printf(" - (link_vars->mac_type)\n");

	BXE_PRINTF(
	    "        %2d - (link_vars->phy_link_up)\n",
	    sc->link_vars.phy_link_up);
	BXE_PRINTF(
	    "        %2d - (link_vars->link_up)\n",
	    sc->link_vars.link_up);
	BXE_PRINTF(
	    "        %2d - (link_vars->duplex)\n",
	    sc->link_vars.duplex);
	BXE_PRINTF(
	    "    0x%04X - (link_vars->flow_ctrl)\n",
	    sc->link_vars.flow_ctrl);
	BXE_PRINTF(
	    "    0x%04X - (link_vars->line_speed)\n",
	    sc->link_vars.line_speed);
	BXE_PRINTF(
	    "0x%08X - (link_vars->ieee_fc)\n",
	    sc->link_vars.ieee_fc);
	BXE_PRINTF(
	    "0x%08X - (link_vars->autoneg)\n",
	    sc->link_vars.autoneg);
	BXE_PRINTF(
	    "0x%08X - (link_vars->phy_flags)\n",
	    sc->link_vars.phy_flags);
	BXE_PRINTF(
	    "0x%08X - (link_vars->link_status)\n",
	    sc->link_vars.link_status);

	BXE_PRINTF(
	    "----------------------------"
	    "----------------"
	    "----------------------------\n");
}


/*
 *
 * Returns:
 *   Nothing.
 */
static __attribute__ ((noinline))
void bxe_dump_link_params_state_locked(struct bxe_softc *sc)
{
	BXE_PRINTF(
	    "--------------------------"
	    " Link Params State "
	    "---------------------------\n");

	BXE_PRINTF(
	    "        %2d - (link_params->port)\n",
	    sc->link_params.port);
	BXE_PRINTF(
	    "        %2d - (link_params->loopback_mode)\n",
	    sc->link_params.loopback_mode);
	BXE_PRINTF(
	    "       %3d - (link_params->phy_addr)\n",
	    sc->link_params.phy_addr);
	BXE_PRINTF(
	    "    0x%04X - (link_params->req_duplex)\n",
	    sc->link_params.req_duplex);
	BXE_PRINTF(
	    "    0x%04X - (link_params->req_flow_ctrl)\n",
	    sc->link_params.req_flow_ctrl);
	BXE_PRINTF(
	    "    0x%04X - (link_params->req_line_speed)\n",
	    sc->link_params.req_line_speed);
	BXE_PRINTF(
	    "     %5d - (link_params->ether_mtu)\n",
	    sc->port.ether_mtu);
	BXE_PRINTF(
	    "0x%08X - (link_params->shmem_base) shared memory base address\n",
	    sc->link_params.shmem_base);
	BXE_PRINTF(
	    "0x%08X - (link_params->speed_cap_mask)\n",
	    sc->link_params.speed_cap_mask);
	BXE_PRINTF(
	    "0x%08X - (link_params->ext_phy_config)\n",
	    sc->link_params.ext_phy_config);
	BXE_PRINTF(
	    "0x%08X - (link_params->switch_cfg)\n",
	    sc->link_params.switch_cfg);

	BXE_PRINTF(
	    "----------------------------"
		"----------------"
		"----------------------------\n");
}

/*
 * Prints out a summary of the driver state.
 *
 * Returns:
 *   Nothing.
 */
static __attribute__ ((noinline))
void bxe_dump_driver_state(struct bxe_softc *sc)
{
	uint32_t val_hi, val_lo;

	BXE_PRINTF(
	    "-----------------------------"
	    " Driver State "
	    "-----------------------------\n");

	val_hi = U64_HI(sc);
	val_lo = U64_LO(sc);
	BXE_PRINTF(
	    "0x%08X:%08X - (sc) driver softc structure virtual address\n",
	    val_hi, val_lo);

	val_hi = U64_HI(sc->bxe_vhandle);
	val_lo = U64_LO(sc->bxe_vhandle);
	BXE_PRINTF(
	    "0x%08X:%08X - (sc->bxe_vhandle) PCI BAR0 virtual address\n",
	    val_hi, val_lo);

	val_hi = U64_HI(sc->bxe_db_vhandle);
	val_lo = U64_LO(sc->bxe_db_vhandle);
	BXE_PRINTF(
	    "0x%08X:%08X - (sc->bxe_db_vhandle) PCI BAR2 virtual address\n",
	    val_hi, val_lo);

	BXE_PRINTF("         0x%08X - (sc->num_queues) Fastpath queues\n",
	    sc->num_queues);
	BXE_PRINTF("         0x%08X - (sc->rx_lane_swap) RX XAUI lane swap\n",
	    sc->rx_lane_swap);
	BXE_PRINTF("         0x%08X - (sc->tx_lane_swap) TX XAUI lane swap\n",
	    sc->tx_lane_swap);
	BXE_PRINTF("   %16lu - (sc->debug_mbuf_sim_alloc_failed)\n",
	    sc->debug_mbuf_sim_alloc_failed);
	BXE_PRINTF("   %16lu - (sc->debug_mbuf_sim_map_failed)\n",
	    sc->debug_mbuf_sim_map_failed);
	BXE_PRINTF("   %16lu - (sc->debug_memory_allocated)\n",
	    sc->debug_memory_allocated);

	BXE_PRINTF(
	    "----------------------------"
	    "----------------"
	    "----------------------------\n");

	bxe_dump_port_state_locked(sc);
	bxe_dump_link_params_state_locked(sc);
	bxe_dump_link_vars_state_locked(sc);
}

/*
 * Dump bootcode debug buffer to the console.
 *
 * Returns:
 *   None
 */
static __attribute__ ((noinline))
void bxe_dump_fw(struct bxe_softc *sc)
{
	uint32_t data[9], mark, offset;
	int word;

	mark = REG_RD(sc, MCP_REG_MCPR_SCRATCH + 0xf104);
	mark = ((mark + 0x3) & ~0x3);

	BXE_PRINTF(
	    "----------------------------"
	    " Bootcode State "
	    "----------------------------\n");
	BXE_PRINTF("Begin MCP bootcode dump (mark = 0x%08X)\n", mark);
	BXE_PRINTF(
	    "----------------------------"
	    "----------------"
	    "----------------------------\n");

	for (offset = mark - 0x08000000; offset <= 0xF900;
	    offset += (0x8 * 4)) {
		for (word = 0; word < 8; word++)
			data[word] = htonl(REG_RD(sc, MCP_REG_MCPR_SCRATCH +
			    offset + 4 * word));
		data[8] = 0x0;
		printf("%s", (char *) data);
	}

	for (offset = 0xF108; offset <= mark - 0x08000000;
		offset += (0x8 * 4)) {
		for (word = 0; word < 8; word++)
			data[word] = htonl(REG_RD(sc, MCP_REG_MCPR_SCRATCH +
			    offset + 4 * word));
		data[8] = 0x0;
		printf("%s", (char *) data);
	}

	BXE_PRINTF(
	    "----------------------------"
	    "----------------"
	    "----------------------------\n");
}

/*
 * Decode firmware messages.
 *
 * Returns:
 *   None
 */
static void
bxe_decode_mb_msgs(struct bxe_softc *sc, uint32_t drv_mb_header,
    uint32_t fw_mb_header)
{

	if (drv_mb_header) {
		BXE_PRINTF("Driver message is ");
		switch (drv_mb_header & DRV_MSG_CODE_MASK) {
		case DRV_MSG_CODE_LOAD_REQ:
			printf(
			    "LOAD_REQ (0x%08X)",
			    (uint32_t)DRV_MSG_CODE_LOAD_REQ);
			break;
		case DRV_MSG_CODE_LOAD_DONE:
			printf(
			    "LOAD_DONE (0x%08X)",
			    (uint32_t)DRV_MSG_CODE_LOAD_DONE);
			break;
		case DRV_MSG_CODE_UNLOAD_REQ_WOL_EN:
			printf(
			    "UNLOAD_REQ_WOL_EN (0x%08X)",
			    (uint32_t)DRV_MSG_CODE_UNLOAD_REQ_WOL_EN);
			break;
		case DRV_MSG_CODE_UNLOAD_REQ_WOL_DIS:
			printf(
			    "UNLOAD_REQ_WOL_DIS (0x%08X)",
			    (uint32_t)DRV_MSG_CODE_UNLOAD_REQ_WOL_DIS);
			break;
		case DRV_MSG_CODE_UNLOAD_REQ_WOL_MCP:
			printf(
			    "UNLOADREQ_WOL_MCP (0x%08X)",
			    (uint32_t)DRV_MSG_CODE_UNLOAD_REQ_WOL_MCP);
			break;
		case DRV_MSG_CODE_UNLOAD_DONE:
			printf(
			    "UNLOAD_DONE (0x%08X)",
			    (uint32_t)DRV_MSG_CODE_UNLOAD_DONE);
			break;
		case DRV_MSG_CODE_DIAG_ENTER_REQ:
			printf(
			    "DIAG_ENTER_REQ (0x%08X)",
			    (uint32_t)DRV_MSG_CODE_DIAG_ENTER_REQ);
			break;
		case DRV_MSG_CODE_DIAG_EXIT_REQ:
			printf(
			    "DIAG_EXIT_REQ (0x%08X)",
			    (uint32_t)DRV_MSG_CODE_DIAG_EXIT_REQ);
			break;
		case DRV_MSG_CODE_VALIDATE_KEY:
			printf(
			    "CODE_VALIDITY_KEY (0x%08X)",
			    (uint32_t)DRV_MSG_CODE_VALIDATE_KEY);
			break;
		case DRV_MSG_CODE_GET_CURR_KEY:
			printf(
			    "GET_CURR_KEY (0x%08X)",
			    (uint32_t) DRV_MSG_CODE_GET_CURR_KEY);
			break;
		case DRV_MSG_CODE_GET_UPGRADE_KEY:
			printf(
			    "GET_UPGRADE_KEY (0x%08X)",
			    (uint32_t)DRV_MSG_CODE_GET_UPGRADE_KEY);
			break;
		case DRV_MSG_CODE_GET_MANUF_KEY:
			printf(
			    "GET_MANUF_KEY (0x%08X)",
			    (uint32_t)DRV_MSG_CODE_GET_MANUF_KEY);
			break;
		case DRV_MSG_CODE_LOAD_L2B_PRAM:
			printf(
			    "LOAD_L2B_PRAM (0x%08X)",
			    (uint32_t)DRV_MSG_CODE_LOAD_L2B_PRAM);
				break;
		case BIOS_MSG_CODE_LIC_CHALLENGE:
			printf(
			    "LIC_CHALLENGE (0x%08X)",
			    (uint32_t)BIOS_MSG_CODE_LIC_CHALLENGE);
			break;
		case BIOS_MSG_CODE_LIC_RESPONSE:
			printf(
			    "LIC_RESPONSE (0x%08X)",
			    (uint32_t)BIOS_MSG_CODE_LIC_RESPONSE);
			break;
		case BIOS_MSG_CODE_VIRT_MAC_PRIM:
			printf(
			    "VIRT_MAC_PRIM (0x%08X)",
			    (uint32_t)BIOS_MSG_CODE_VIRT_MAC_PRIM);
			break;
		case BIOS_MSG_CODE_VIRT_MAC_ISCSI:
			printf(
			    "VIRT_MAC_ISCSI (0x%08X)",
			    (uint32_t)BIOS_MSG_CODE_VIRT_MAC_ISCSI);
			break;
		default:
			printf(
			    "Unknown command (0x%08X)!",
			    (drv_mb_header & DRV_MSG_CODE_MASK));
		}

		printf(" (seq = 0x%04X)\n", (drv_mb_header &
		    DRV_MSG_SEQ_NUMBER_MASK));
	}

	if (fw_mb_header) {
		BXE_PRINTF("Firmware response is ");
		switch (fw_mb_header & FW_MSG_CODE_MASK) {
		case FW_MSG_CODE_DRV_LOAD_COMMON:
			printf(
			    "DRV_LOAD_COMMON (0x%08X)",
			    (uint32_t)FW_MSG_CODE_DRV_LOAD_COMMON);
			break;
		case FW_MSG_CODE_DRV_LOAD_PORT:
			printf(
			    "DRV_LOAD_PORT (0x%08X)",
			    (uint32_t)FW_MSG_CODE_DRV_LOAD_PORT);
			break;
		case FW_MSG_CODE_DRV_LOAD_FUNCTION:
			printf(
			    "DRV_LOAD_FUNCTION (0x%08X)",
			    (uint32_t)FW_MSG_CODE_DRV_LOAD_FUNCTION);
			break;
		case FW_MSG_CODE_DRV_LOAD_REFUSED:
			printf(
			    "DRV_LOAD_REFUSED (0x%08X)",
			    (uint32_t)FW_MSG_CODE_DRV_LOAD_REFUSED);
			break;
		case FW_MSG_CODE_DRV_LOAD_DONE:
			printf(
			    "DRV_LOAD_DONE (0x%08X)",
			    (uint32_t)FW_MSG_CODE_DRV_LOAD_DONE);
			break;
		case FW_MSG_CODE_DRV_UNLOAD_COMMON:
			printf(
			    "DRV_UNLOAD_COMMON (0x%08X)",
			    (uint32_t)FW_MSG_CODE_DRV_UNLOAD_COMMON);
			break;
		case FW_MSG_CODE_DRV_UNLOAD_PORT:
			printf(
			    "DRV_UNLOAD_PORT (0x%08X)",
			    (uint32_t)FW_MSG_CODE_DRV_UNLOAD_PORT);
			break;
		case FW_MSG_CODE_DRV_UNLOAD_FUNCTION:
			printf(
			    "DRV_UNLOAD_FUNCTION (0x%08X)",
			    (uint32_t)FW_MSG_CODE_DRV_UNLOAD_FUNCTION);
			break;
		case FW_MSG_CODE_DRV_UNLOAD_DONE:
			printf(
			    "DRV_UNLOAD_DONE (0x%08X)",
			    (uint32_t)FW_MSG_CODE_DRV_UNLOAD_DONE);
			break;
		case FW_MSG_CODE_DIAG_ENTER_DONE:
			printf(
			    "DIAG_ENTER_DONE (0x%08X)",
			    (uint32_t)FW_MSG_CODE_DIAG_ENTER_DONE);
			break;
		case FW_MSG_CODE_DIAG_REFUSE:
			printf(
			    "DIAG_REFUSE (0x%08X)",
			    (uint32_t)FW_MSG_CODE_DIAG_REFUSE);
			break;
		case FW_MSG_CODE_DIAG_EXIT_DONE:
			printf(
			    "DIAG_EXIT_DONE (0x%08X)",
			    (uint32_t)FW_MSG_CODE_DIAG_EXIT_DONE);
			break;
		case FW_MSG_CODE_VALIDATE_KEY_SUCCESS:
			printf(
			    "VALIDATE_KEY_SUCCESS (0x%08X)",
			    (uint32_t)FW_MSG_CODE_VALIDATE_KEY_SUCCESS);
			break;
		case FW_MSG_CODE_VALIDATE_KEY_FAILURE:
			printf(
			    "VALIDATE_KEY_FAILURE (0x%08X)",
			    (uint32_t)FW_MSG_CODE_VALIDATE_KEY_FAILURE);
			break;
		case FW_MSG_CODE_GET_KEY_DONE:
			printf(
			    "GET_KEY_DONE (0x%08X)",
			    (uint32_t)FW_MSG_CODE_GET_KEY_DONE);
			break;
		case FW_MSG_CODE_NO_KEY:
			printf(
			    "NO_KEY (0x%08X)",
			    (uint32_t)FW_MSG_CODE_NO_KEY);
			break;
		default:
			printf(
			    "unknown value (0x%08X)!",
			    (fw_mb_header & FW_MSG_CODE_MASK));
		}

		printf(" (seq = 0x%04X)\n", (fw_mb_header &
		    FW_MSG_SEQ_NUMBER_MASK));
	}
}

/*
 * Prints a text string for the ramrod command.
 *
 * Returns:
 *   None
 */
static void
bxe_decode_ramrod_cmd(struct bxe_softc *sc, int command)
{
	BXE_PRINTF("Ramrod command = ");

	switch (command) {
	case RAMROD_CMD_ID_ETH_PORT_SETUP:
		printf("ETH_PORT_SETUP\n");
		break;
	case RAMROD_CMD_ID_ETH_CLIENT_SETUP:
		printf("ETH_CLIENT_SETUP\n");
		break;
	case RAMROD_CMD_ID_ETH_STAT_QUERY:
		printf("ETH_STAT_QUERY\n");
		break;
	case RAMROD_CMD_ID_ETH_UPDATE:
		printf("ETH_UPDATE\n");
		break;
	case RAMROD_CMD_ID_ETH_HALT:
		printf("ETH_HALT\n");
		break;
	case RAMROD_CMD_ID_ETH_SET_MAC:
		printf("ETH_SET_MAC\n");
		break;
	case RAMROD_CMD_ID_ETH_CFC_DEL:
		printf("ETH_CFC_DEL\n");
		break;
	case RAMROD_CMD_ID_ETH_PORT_DEL:
		printf("ETH_PORT_DEL\n");
		break;
	case RAMROD_CMD_ID_ETH_FORWARD_SETUP:
		printf("ETH_FORWARD_SETUP\n");
		break;
	default:
		printf("Unknown ramrod command!\n");
	}
}


/*
 * Prints out driver information and forces a kernel breakpoint.
 *
 * Returns:
 *   Nothing.
 */
static void
bxe_breakpoint(struct bxe_softc *sc)
{
	struct bxe_fastpath *fp;
	int i;

	fp = &sc->fp[0];
	/* Unreachable code to silence the compiler about unused functions. */
	if (0) {
		bxe_reg_read16(sc, PCICFG_OFFSET);
		bxe_dump_tx_mbuf_chain(sc, 0, USABLE_TX_BD);
		bxe_dump_rx_mbuf_chain(sc, 0, USABLE_RX_BD);
		bxe_dump_tx_chain(fp, 0, USABLE_TX_BD);
		bxe_dump_rx_cq_chain(fp, 0, USABLE_RCQ_ENTRIES);
		bxe_dump_rx_bd_chain(fp, 0, USABLE_RX_BD);
		bxe_dump_status_block(sc);
		bxe_dump_stats_block(sc);
		bxe_dump_fp_state(fp);
		bxe_dump_driver_state(sc);
		bxe_dump_hw_state(sc);
		bxe_dump_fw(sc);
	}

	/*
	 * Do some device sanity checking.  Run it twice in case
	 * the hardware is still running so we can identify any
	 * transient conditions.
	 */
	bxe_idle_chk(sc); bxe_idle_chk(sc);

	bxe_dump_driver_state(sc);

	for (i = 0; i < sc->num_queues; i++)
		bxe_dump_fp_state(&sc->fp[i]);

	bxe_dump_status_block(sc);

	/* Call the OS debugger. */
	breakpoint();
}
#endif

/*
 *
 * Returns:
 *   Nothing.
 */
static void
bxe_gunzip_end(struct bxe_softc *sc)
{
	free(sc->strm, M_DEVBUF);
	sc->strm = NULL;

	if (sc->gunzip_buf) {
		bxe_dmamem_free(sc, sc->gunzip_tag, sc->gunzip_buf,
		    sc->gunzip_map);
		sc->gunzip_buf = NULL;
	}
}
