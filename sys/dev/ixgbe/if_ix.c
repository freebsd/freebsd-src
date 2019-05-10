/******************************************************************************

  Copyright (c) 2001-2017, Intel Corporation
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   3. Neither the name of the Intel Corporation nor the names of its
      contributors may be used to endorse or promote products derived from
      this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

******************************************************************************/
/*$FreeBSD$*/


#ifndef IXGBE_STANDALONE_BUILD
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_rss.h"
#endif

#include "ixgbe.h"

/************************************************************************
 * Driver version
 ************************************************************************/
char ixgbe_driver_version[] = "3.2.12-k";


/************************************************************************
 * PCI Device ID Table
 *
 *   Used by probe to select devices to load on
 *   Last field stores an index into ixgbe_strings
 *   Last entry must be all 0s
 *
 *   { Vendor ID, Device ID, SubVendor ID, SubDevice ID, String Index }
 ************************************************************************/
static ixgbe_vendor_info_t ixgbe_vendor_info_array[] =
{
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82598AF_DUAL_PORT, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82598AF_SINGLE_PORT, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82598EB_CX4, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82598AT, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82598AT2, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82598, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82598_DA_DUAL_PORT, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82598_CX4_DUAL_PORT, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82598EB_XF_LR, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82598_SR_DUAL_PORT_EM, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82598EB_SFP_LOM, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82599_KX4, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82599_KX4_MEZZ, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82599_SFP, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82599_XAUI_LOM, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82599_CX4, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82599_T3_LOM, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82599_COMBO_BACKPLANE, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82599_BACKPLANE_FCOE, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82599_SFP_SF2, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82599_SFP_FCOE, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82599EN_SFP, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82599_SFP_SF_QP, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82599_QSFP_SF_QP, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_X540T, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_X540T1, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_X550T, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_X550T1, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_X550EM_X_KR, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_X550EM_X_KX4, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_X550EM_X_10G_T, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_X550EM_X_1G_T, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_X550EM_X_SFP, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_X550EM_A_KR, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_X550EM_A_KR_L, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_X550EM_A_SFP, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_X550EM_A_SFP_N, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_X550EM_A_SGMII, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_X550EM_A_SGMII_L, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_X550EM_A_10G_T, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_X550EM_A_1G_T, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_X550EM_A_1G_T_L, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_X540_BYPASS, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82599_BYPASS, 0, 0, 0},
	/* required last entry */
	{0, 0, 0, 0, 0}
};

/************************************************************************
 * Table of branding strings
 ************************************************************************/
static char    *ixgbe_strings[] = {
	"Intel(R) PRO/10GbE PCI-Express Network Driver"
};

/************************************************************************
 * Function prototypes
 ************************************************************************/
static int      ixgbe_probe(device_t);
static int      ixgbe_attach(device_t);
static int      ixgbe_detach(device_t);
static int      ixgbe_shutdown(device_t);
static int      ixgbe_suspend(device_t);
static int      ixgbe_resume(device_t);
static int      ixgbe_ioctl(struct ifnet *, u_long, caddr_t);
static void     ixgbe_init(void *);
static void     ixgbe_init_locked(struct adapter *);
static void     ixgbe_stop(void *);
#if __FreeBSD_version >= 1100036
static uint64_t ixgbe_get_counter(struct ifnet *, ift_counter);
#endif
static void     ixgbe_init_device_features(struct adapter *);
static void     ixgbe_check_fan_failure(struct adapter *, u32, bool);
static void     ixgbe_add_media_types(struct adapter *);
static void     ixgbe_media_status(struct ifnet *, struct ifmediareq *);
static int      ixgbe_media_change(struct ifnet *);
static int      ixgbe_allocate_pci_resources(struct adapter *);
static void     ixgbe_get_slot_info(struct adapter *);
static int      ixgbe_allocate_msix(struct adapter *);
static int      ixgbe_allocate_legacy(struct adapter *);
static int      ixgbe_configure_interrupts(struct adapter *);
static void     ixgbe_free_pci_resources(struct adapter *);
static void     ixgbe_local_timer(void *);
static int      ixgbe_setup_interface(device_t, struct adapter *);
static void     ixgbe_config_gpie(struct adapter *);
static void     ixgbe_config_dmac(struct adapter *);
static void     ixgbe_config_delay_values(struct adapter *);
static void     ixgbe_config_link(struct adapter *);
static void     ixgbe_check_wol_support(struct adapter *);
static int      ixgbe_setup_low_power_mode(struct adapter *);
static void     ixgbe_rearm_queues(struct adapter *, u64);

static void     ixgbe_initialize_transmit_units(struct adapter *);
static void     ixgbe_initialize_receive_units(struct adapter *);
static void     ixgbe_enable_rx_drop(struct adapter *);
static void     ixgbe_disable_rx_drop(struct adapter *);
static void     ixgbe_initialize_rss_mapping(struct adapter *);

static void     ixgbe_enable_intr(struct adapter *, bool);
static void     ixgbe_disable_intr(struct adapter *, bool);
static void     ixgbe_update_stats_counters(struct adapter *);
static void     ixgbe_set_promisc(struct adapter *);
static void     ixgbe_set_multi(struct adapter *);
static void     ixgbe_update_link_status(struct adapter *);
static void     ixgbe_set_ivar(struct adapter *, u8, u8, s8);
static void     ixgbe_configure_ivars(struct adapter *);
static u8       *ixgbe_mc_array_itr(struct ixgbe_hw *, u8 **, u32 *);

static void     ixgbe_setup_vlan_hw_support(struct adapter *);
static void     ixgbe_register_vlan(void *, struct ifnet *, u16);
static void     ixgbe_unregister_vlan(void *, struct ifnet *, u16);

static void     ixgbe_add_device_sysctls(struct adapter *);
static void     ixgbe_add_hw_stats(struct adapter *);
static int      ixgbe_set_flowcntl(struct adapter *, int);
static int      ixgbe_set_advertise(struct adapter *, int);
static int      ixgbe_get_advertise(struct adapter *);

/* Sysctl handlers */
static void     ixgbe_set_sysctl_value(struct adapter *, const char *,
                                       const char *, int *, int);
static int      ixgbe_sysctl_flowcntl(SYSCTL_HANDLER_ARGS);
static int      ixgbe_sysctl_advertise(SYSCTL_HANDLER_ARGS);
static int      ixgbe_sysctl_interrupt_rate_handler(SYSCTL_HANDLER_ARGS);
static int      ixgbe_sysctl_dmac(SYSCTL_HANDLER_ARGS);
static int      ixgbe_sysctl_phy_temp(SYSCTL_HANDLER_ARGS);
static int      ixgbe_sysctl_phy_overtemp_occurred(SYSCTL_HANDLER_ARGS);
#ifdef IXGBE_DEBUG
static int      ixgbe_sysctl_power_state(SYSCTL_HANDLER_ARGS);
static int      ixgbe_sysctl_print_rss_config(SYSCTL_HANDLER_ARGS);
#endif
static int      ixgbe_sysctl_rdh_handler(SYSCTL_HANDLER_ARGS);
static int      ixgbe_sysctl_rdt_handler(SYSCTL_HANDLER_ARGS);
static int      ixgbe_sysctl_tdt_handler(SYSCTL_HANDLER_ARGS);
static int      ixgbe_sysctl_tdh_handler(SYSCTL_HANDLER_ARGS);
static int      ixgbe_sysctl_eee_state(SYSCTL_HANDLER_ARGS);
static int      ixgbe_sysctl_wol_enable(SYSCTL_HANDLER_ARGS);
static int      ixgbe_sysctl_wufc(SYSCTL_HANDLER_ARGS);

/* Support for pluggable optic modules */
static bool     ixgbe_sfp_probe(struct adapter *);

/* Legacy (single vector) interrupt handler */
static void     ixgbe_legacy_irq(void *);

/* The MSI/MSI-X Interrupt handlers */
static void     ixgbe_msix_que(void *);
static void     ixgbe_msix_link(void *);

/* Deferred interrupt tasklets */
static void     ixgbe_handle_que(void *, int);
static void     ixgbe_handle_link(void *);
static void     ixgbe_handle_msf(void *);
static void     ixgbe_handle_mod(void *);
static void     ixgbe_handle_phy(void *);
static void     ixgbe_handle_admin_task(void *, int);


/************************************************************************
 *  FreeBSD Device Interface Entry Points
 ************************************************************************/
static device_method_t ix_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, ixgbe_probe),
	DEVMETHOD(device_attach, ixgbe_attach),
	DEVMETHOD(device_detach, ixgbe_detach),
	DEVMETHOD(device_shutdown, ixgbe_shutdown),
	DEVMETHOD(device_suspend, ixgbe_suspend),
	DEVMETHOD(device_resume, ixgbe_resume),
#ifdef PCI_IOV
	DEVMETHOD(pci_iov_init, ixgbe_init_iov),
	DEVMETHOD(pci_iov_uninit, ixgbe_uninit_iov),
	DEVMETHOD(pci_iov_add_vf, ixgbe_add_vf),
#endif /* PCI_IOV */
	DEVMETHOD_END
};

static driver_t ix_driver = {
	"ix", ix_methods, sizeof(struct adapter),
};

devclass_t ix_devclass;
DRIVER_MODULE(ix, pci, ix_driver, ix_devclass, 0, 0);

MODULE_DEPEND(ix, pci, 1, 1, 1);
MODULE_DEPEND(ix, ether, 1, 1, 1);
#ifdef DEV_NETMAP
MODULE_DEPEND(ix, netmap, 1, 1, 1);
#endif

/*
 * TUNEABLE PARAMETERS:
 */

static SYSCTL_NODE(_hw, OID_AUTO, ix, CTLFLAG_RD, 0, "IXGBE driver parameters");

/*
 * AIM: Adaptive Interrupt Moderation
 * which means that the interrupt rate
 * is varied over time based on the
 * traffic for that interrupt vector
 */
static int ixgbe_enable_aim = TRUE;
SYSCTL_INT(_hw_ix, OID_AUTO, enable_aim, CTLFLAG_RDTUN, &ixgbe_enable_aim, 0,
    "Enable adaptive interrupt moderation");

static int ixgbe_max_interrupt_rate = (4000000 / IXGBE_LOW_LATENCY);
SYSCTL_INT(_hw_ix, OID_AUTO, max_interrupt_rate, CTLFLAG_RDTUN,
    &ixgbe_max_interrupt_rate, 0, "Maximum interrupts per second");

/* How many packets rxeof tries to clean at a time */
static int ixgbe_rx_process_limit = 256;
SYSCTL_INT(_hw_ix, OID_AUTO, rx_process_limit, CTLFLAG_RDTUN,
    &ixgbe_rx_process_limit, 0, "Maximum number of received packets to process at a time, -1 means unlimited");

/* How many packets txeof tries to clean at a time */
static int ixgbe_tx_process_limit = 256;
SYSCTL_INT(_hw_ix, OID_AUTO, tx_process_limit, CTLFLAG_RDTUN,
    &ixgbe_tx_process_limit, 0,
    "Maximum number of sent packets to process at a time, -1 means unlimited");

/* Flow control setting, default to full */
static int ixgbe_flow_control = ixgbe_fc_full;
SYSCTL_INT(_hw_ix, OID_AUTO, flow_control, CTLFLAG_RDTUN,
    &ixgbe_flow_control, 0, "Default flow control used for all adapters");

/* Advertise Speed, default to 0 (auto) */
static int ixgbe_advertise_speed = 0;
SYSCTL_INT(_hw_ix, OID_AUTO, advertise_speed, CTLFLAG_RDTUN,
    &ixgbe_advertise_speed, 0, "Default advertised speed for all adapters");

/*
 * Smart speed setting, default to on
 * this only works as a compile option
 * right now as its during attach, set
 * this to 'ixgbe_smart_speed_off' to
 * disable.
 */
static int ixgbe_smart_speed = ixgbe_smart_speed_on;

/*
 * MSI-X should be the default for best performance,
 * but this allows it to be forced off for testing.
 */
static int ixgbe_enable_msix = 1;
SYSCTL_INT(_hw_ix, OID_AUTO, enable_msix, CTLFLAG_RDTUN, &ixgbe_enable_msix, 0,
    "Enable MSI-X interrupts");

/*
 * Number of Queues, can be set to 0,
 * it then autoconfigures based on the
 * number of cpus with a max of 8. This
 * can be overriden manually here.
 */
static int ixgbe_num_queues = 0;
SYSCTL_INT(_hw_ix, OID_AUTO, num_queues, CTLFLAG_RDTUN, &ixgbe_num_queues, 0,
    "Number of queues to configure, 0 indicates autoconfigure");

/*
 * Number of TX descriptors per ring,
 * setting higher than RX as this seems
 * the better performing choice.
 */
static int ixgbe_txd = PERFORM_TXD;
SYSCTL_INT(_hw_ix, OID_AUTO, txd, CTLFLAG_RDTUN, &ixgbe_txd, 0,
    "Number of transmit descriptors per queue");

/* Number of RX descriptors per ring */
static int ixgbe_rxd = PERFORM_RXD;
SYSCTL_INT(_hw_ix, OID_AUTO, rxd, CTLFLAG_RDTUN, &ixgbe_rxd, 0,
    "Number of receive descriptors per queue");

/*
 * Defining this on will allow the use
 * of unsupported SFP+ modules, note that
 * doing so you are on your own :)
 */
static int allow_unsupported_sfp = FALSE;
SYSCTL_INT(_hw_ix, OID_AUTO, unsupported_sfp, CTLFLAG_RDTUN,
    &allow_unsupported_sfp, 0,
    "Allow unsupported SFP modules...use at your own risk");

/*
 * Not sure if Flow Director is fully baked,
 * so we'll default to turning it off.
 */
static int ixgbe_enable_fdir = 0;
SYSCTL_INT(_hw_ix, OID_AUTO, enable_fdir, CTLFLAG_RDTUN, &ixgbe_enable_fdir, 0,
    "Enable Flow Director");

/* Legacy Transmit (single queue) */
static int ixgbe_enable_legacy_tx = 0;
SYSCTL_INT(_hw_ix, OID_AUTO, enable_legacy_tx, CTLFLAG_RDTUN,
    &ixgbe_enable_legacy_tx, 0, "Enable Legacy TX flow");

/* Receive-Side Scaling */
static int ixgbe_enable_rss = 1;
SYSCTL_INT(_hw_ix, OID_AUTO, enable_rss, CTLFLAG_RDTUN, &ixgbe_enable_rss, 0,
    "Enable Receive-Side Scaling (RSS)");

/* Keep running tab on them for sanity check */
static int ixgbe_total_ports;

static int (*ixgbe_start_locked)(struct ifnet *, struct tx_ring *);
static int (*ixgbe_ring_empty)(struct ifnet *, struct buf_ring *);

MALLOC_DEFINE(M_IXGBE, "ix", "ix driver allocations");

/************************************************************************
 * ixgbe_initialize_rss_mapping
 ************************************************************************/
static void
ixgbe_initialize_rss_mapping(struct adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;
	u32             reta = 0, mrqc, rss_key[10];
	int             queue_id, table_size, index_mult;
	int             i, j;
	u32             rss_hash_config;

	if (adapter->feat_en & IXGBE_FEATURE_RSS) {
		/* Fetch the configured RSS key */
		rss_getkey((uint8_t *)&rss_key);
	} else {
		/* set up random bits */
		arc4rand(&rss_key, sizeof(rss_key), 0);
	}

	/* Set multiplier for RETA setup and table size based on MAC */
	index_mult = 0x1;
	table_size = 128;
	switch (adapter->hw.mac.type) {
	case ixgbe_mac_82598EB:
		index_mult = 0x11;
		break;
	case ixgbe_mac_X550:
	case ixgbe_mac_X550EM_x:
	case ixgbe_mac_X550EM_a:
		table_size = 512;
		break;
	default:
		break;
	}

	/* Set up the redirection table */
	for (i = 0, j = 0; i < table_size; i++, j++) {
		if (j == adapter->num_queues)
			j = 0;

		if (adapter->feat_en & IXGBE_FEATURE_RSS) {
			/*
			 * Fetch the RSS bucket id for the given indirection
			 * entry. Cap it at the number of configured buckets
			 * (which is num_queues.)
			 */
			queue_id = rss_get_indirection_to_bucket(i);
			queue_id = queue_id % adapter->num_queues;
		} else
			queue_id = (j * index_mult);

		/*
		 * The low 8 bits are for hash value (n+0);
		 * The next 8 bits are for hash value (n+1), etc.
		 */
		reta = reta >> 8;
		reta = reta | (((uint32_t)queue_id) << 24);
		if ((i & 3) == 3) {
			if (i < 128)
				IXGBE_WRITE_REG(hw, IXGBE_RETA(i >> 2), reta);
			else
				IXGBE_WRITE_REG(hw, IXGBE_ERETA((i >> 2) - 32),
				    reta);
			reta = 0;
		}
	}

	/* Now fill our hash function seeds */
	for (i = 0; i < 10; i++)
		IXGBE_WRITE_REG(hw, IXGBE_RSSRK(i), rss_key[i]);

	/* Perform hash on these packet types */
	if (adapter->feat_en & IXGBE_FEATURE_RSS)
		rss_hash_config = rss_gethashconfig();
	else {
		/*
		 * Disable UDP - IP fragments aren't currently being handled
		 * and so we end up with a mix of 2-tuple and 4-tuple
		 * traffic.
		 */
		rss_hash_config = RSS_HASHTYPE_RSS_IPV4
		                | RSS_HASHTYPE_RSS_TCP_IPV4
		                | RSS_HASHTYPE_RSS_IPV6
		                | RSS_HASHTYPE_RSS_TCP_IPV6
		                | RSS_HASHTYPE_RSS_IPV6_EX
		                | RSS_HASHTYPE_RSS_TCP_IPV6_EX;
	}

	mrqc = IXGBE_MRQC_RSSEN;
	if (rss_hash_config & RSS_HASHTYPE_RSS_IPV4)
		mrqc |= IXGBE_MRQC_RSS_FIELD_IPV4;
	if (rss_hash_config & RSS_HASHTYPE_RSS_TCP_IPV4)
		mrqc |= IXGBE_MRQC_RSS_FIELD_IPV4_TCP;
	if (rss_hash_config & RSS_HASHTYPE_RSS_IPV6)
		mrqc |= IXGBE_MRQC_RSS_FIELD_IPV6;
	if (rss_hash_config & RSS_HASHTYPE_RSS_TCP_IPV6)
		mrqc |= IXGBE_MRQC_RSS_FIELD_IPV6_TCP;
	if (rss_hash_config & RSS_HASHTYPE_RSS_IPV6_EX)
		mrqc |= IXGBE_MRQC_RSS_FIELD_IPV6_EX;
	if (rss_hash_config & RSS_HASHTYPE_RSS_TCP_IPV6_EX)
		mrqc |= IXGBE_MRQC_RSS_FIELD_IPV6_EX_TCP;
	if (rss_hash_config & RSS_HASHTYPE_RSS_UDP_IPV4)
		mrqc |= IXGBE_MRQC_RSS_FIELD_IPV4_UDP;
	if (rss_hash_config & RSS_HASHTYPE_RSS_UDP_IPV4_EX)
		device_printf(adapter->dev, "%s: RSS_HASHTYPE_RSS_UDP_IPV4_EX defined, but not supported\n",
		    __func__);
	if (rss_hash_config & RSS_HASHTYPE_RSS_UDP_IPV6)
		mrqc |= IXGBE_MRQC_RSS_FIELD_IPV6_UDP;
	if (rss_hash_config & RSS_HASHTYPE_RSS_UDP_IPV6_EX)
		mrqc |= IXGBE_MRQC_RSS_FIELD_IPV6_EX_UDP;
	mrqc |= ixgbe_get_mrqc(adapter->iov_mode);
	IXGBE_WRITE_REG(hw, IXGBE_MRQC, mrqc);
} /* ixgbe_initialize_rss_mapping */

/************************************************************************
 * ixgbe_initialize_receive_units - Setup receive registers and features.
 ************************************************************************/
#define BSIZEPKT_ROUNDUP ((1<<IXGBE_SRRCTL_BSIZEPKT_SHIFT)-1)

static void
ixgbe_initialize_receive_units(struct adapter *adapter)
{
	struct rx_ring  *rxr = adapter->rx_rings;
	struct ixgbe_hw *hw = &adapter->hw;
	struct ifnet    *ifp = adapter->ifp;
	int             i, j;
	u32             bufsz, fctrl, srrctl, rxcsum;
	u32             hlreg;

	/*
	 * Make sure receives are disabled while
	 * setting up the descriptor ring
	 */
	ixgbe_disable_rx(hw);

	/* Enable broadcasts */
	fctrl = IXGBE_READ_REG(hw, IXGBE_FCTRL);
	fctrl |= IXGBE_FCTRL_BAM;
	if (adapter->hw.mac.type == ixgbe_mac_82598EB) {
		fctrl |= IXGBE_FCTRL_DPF;
		fctrl |= IXGBE_FCTRL_PMCF;
	}
	IXGBE_WRITE_REG(hw, IXGBE_FCTRL, fctrl);

	/* Set for Jumbo Frames? */
	hlreg = IXGBE_READ_REG(hw, IXGBE_HLREG0);
	if (ifp->if_mtu > ETHERMTU)
		hlreg |= IXGBE_HLREG0_JUMBOEN;
	else
		hlreg &= ~IXGBE_HLREG0_JUMBOEN;

#ifdef DEV_NETMAP
	/* CRC stripping is conditional in Netmap */
	if ((adapter->feat_en & IXGBE_FEATURE_NETMAP) &&
	    (ifp->if_capenable & IFCAP_NETMAP) &&
	    !ix_crcstrip)
		hlreg &= ~IXGBE_HLREG0_RXCRCSTRP;
	else
#endif /* DEV_NETMAP */
		hlreg |= IXGBE_HLREG0_RXCRCSTRP;

	IXGBE_WRITE_REG(hw, IXGBE_HLREG0, hlreg);

	bufsz = (adapter->rx_mbuf_sz + BSIZEPKT_ROUNDUP) >>
	    IXGBE_SRRCTL_BSIZEPKT_SHIFT;

	for (i = 0; i < adapter->num_queues; i++, rxr++) {
		u64 rdba = rxr->rxdma.dma_paddr;
		j = rxr->me;

		/* Setup the Base and Length of the Rx Descriptor Ring */
		IXGBE_WRITE_REG(hw, IXGBE_RDBAL(j),
		    (rdba & 0x00000000ffffffffULL));
		IXGBE_WRITE_REG(hw, IXGBE_RDBAH(j), (rdba >> 32));
		IXGBE_WRITE_REG(hw, IXGBE_RDLEN(j),
		    adapter->num_rx_desc * sizeof(union ixgbe_adv_rx_desc));

		/* Set up the SRRCTL register */
		srrctl = IXGBE_READ_REG(hw, IXGBE_SRRCTL(j));
		srrctl &= ~IXGBE_SRRCTL_BSIZEHDR_MASK;
		srrctl &= ~IXGBE_SRRCTL_BSIZEPKT_MASK;
		srrctl |= bufsz;
		srrctl |= IXGBE_SRRCTL_DESCTYPE_ADV_ONEBUF;

		/*
		 * Set DROP_EN iff we have no flow control and >1 queue.
		 * Note that srrctl was cleared shortly before during reset,
		 * so we do not need to clear the bit, but do it just in case
		 * this code is moved elsewhere.
		 */
		if (adapter->num_queues > 1 &&
		    adapter->hw.fc.requested_mode == ixgbe_fc_none) {
			srrctl |= IXGBE_SRRCTL_DROP_EN;
		} else {
			srrctl &= ~IXGBE_SRRCTL_DROP_EN;
		}

		IXGBE_WRITE_REG(hw, IXGBE_SRRCTL(j), srrctl);

		/* Setup the HW Rx Head and Tail Descriptor Pointers */
		IXGBE_WRITE_REG(hw, IXGBE_RDH(j), 0);
		IXGBE_WRITE_REG(hw, IXGBE_RDT(j), 0);

		/* Set the driver rx tail address */
		rxr->tail =  IXGBE_RDT(rxr->me);
	}

	if (adapter->hw.mac.type != ixgbe_mac_82598EB) {
		u32 psrtype = IXGBE_PSRTYPE_TCPHDR
		            | IXGBE_PSRTYPE_UDPHDR
		            | IXGBE_PSRTYPE_IPV4HDR
		            | IXGBE_PSRTYPE_IPV6HDR;
		IXGBE_WRITE_REG(hw, IXGBE_PSRTYPE(0), psrtype);
	}

	rxcsum = IXGBE_READ_REG(hw, IXGBE_RXCSUM);

	ixgbe_initialize_rss_mapping(adapter);

	if (adapter->num_queues > 1) {
		/* RSS and RX IPP Checksum are mutually exclusive */
		rxcsum |= IXGBE_RXCSUM_PCSD;
	}

	if (ifp->if_capenable & IFCAP_RXCSUM)
		rxcsum |= IXGBE_RXCSUM_PCSD;

	/* This is useful for calculating UDP/IP fragment checksums */
	if (!(rxcsum & IXGBE_RXCSUM_PCSD))
		rxcsum |= IXGBE_RXCSUM_IPPCSE;

	IXGBE_WRITE_REG(hw, IXGBE_RXCSUM, rxcsum);

	return;
} /* ixgbe_initialize_receive_units */

/************************************************************************
 * ixgbe_initialize_transmit_units - Enable transmit units.
 ************************************************************************/
static void
ixgbe_initialize_transmit_units(struct adapter *adapter)
{
	struct tx_ring  *txr = adapter->tx_rings;
	struct ixgbe_hw *hw = &adapter->hw;

	/* Setup the Base and Length of the Tx Descriptor Ring */
	for (int i = 0; i < adapter->num_queues; i++, txr++) {
		u64 tdba = txr->txdma.dma_paddr;
		u32 txctrl = 0;
		int j = txr->me;

		IXGBE_WRITE_REG(hw, IXGBE_TDBAL(j),
		    (tdba & 0x00000000ffffffffULL));
		IXGBE_WRITE_REG(hw, IXGBE_TDBAH(j), (tdba >> 32));
		IXGBE_WRITE_REG(hw, IXGBE_TDLEN(j),
		    adapter->num_tx_desc * sizeof(union ixgbe_adv_tx_desc));

		/* Setup the HW Tx Head and Tail descriptor pointers */
		IXGBE_WRITE_REG(hw, IXGBE_TDH(j), 0);
		IXGBE_WRITE_REG(hw, IXGBE_TDT(j), 0);

		/* Cache the tail address */
		txr->tail = IXGBE_TDT(j);

		/* Disable Head Writeback */
		/*
		 * Note: for X550 series devices, these registers are actually
		 * prefixed with TPH_ isntead of DCA_, but the addresses and
		 * fields remain the same.
		 */
		switch (hw->mac.type) {
		case ixgbe_mac_82598EB:
			txctrl = IXGBE_READ_REG(hw, IXGBE_DCA_TXCTRL(j));
			break;
		default:
			txctrl = IXGBE_READ_REG(hw, IXGBE_DCA_TXCTRL_82599(j));
			break;
		}
		txctrl &= ~IXGBE_DCA_TXCTRL_DESC_WRO_EN;
		switch (hw->mac.type) {
		case ixgbe_mac_82598EB:
			IXGBE_WRITE_REG(hw, IXGBE_DCA_TXCTRL(j), txctrl);
			break;
		default:
			IXGBE_WRITE_REG(hw, IXGBE_DCA_TXCTRL_82599(j), txctrl);
			break;
		}

	}

	if (hw->mac.type != ixgbe_mac_82598EB) {
		u32 dmatxctl, rttdcs;

		dmatxctl = IXGBE_READ_REG(hw, IXGBE_DMATXCTL);
		dmatxctl |= IXGBE_DMATXCTL_TE;
		IXGBE_WRITE_REG(hw, IXGBE_DMATXCTL, dmatxctl);
		/* Disable arbiter to set MTQC */
		rttdcs = IXGBE_READ_REG(hw, IXGBE_RTTDCS);
		rttdcs |= IXGBE_RTTDCS_ARBDIS;
		IXGBE_WRITE_REG(hw, IXGBE_RTTDCS, rttdcs);
		IXGBE_WRITE_REG(hw, IXGBE_MTQC,
		    ixgbe_get_mtqc(adapter->iov_mode));
		rttdcs &= ~IXGBE_RTTDCS_ARBDIS;
		IXGBE_WRITE_REG(hw, IXGBE_RTTDCS, rttdcs);
	}

	return;
} /* ixgbe_initialize_transmit_units */

/************************************************************************
 * ixgbe_attach - Device initialization routine
 *
 *   Called when the driver is being loaded.
 *   Identifies the type of hardware, allocates all resources
 *   and initializes the hardware.
 *
 *   return 0 on success, positive on failure
 ************************************************************************/
static int
ixgbe_attach(device_t dev)
{
	struct adapter  *adapter;
	struct ixgbe_hw *hw;
	int             error = 0;
	u32             ctrl_ext;

	INIT_DEBUGOUT("ixgbe_attach: begin");

	/* Allocate, clear, and link in our adapter structure */
	adapter = device_get_softc(dev);
	adapter->hw.back = adapter;
	adapter->dev = dev;
	hw = &adapter->hw;

	/* Core Lock Init*/
	IXGBE_CORE_LOCK_INIT(adapter, device_get_nameunit(dev));

	/* Set up the timer callout */
	callout_init_mtx(&adapter->timer, &adapter->core_mtx, 0);

	/* Determine hardware revision */
	hw->vendor_id = pci_get_vendor(dev);
	hw->device_id = pci_get_device(dev);
	hw->revision_id = pci_get_revid(dev);
	hw->subsystem_vendor_id = pci_get_subvendor(dev);
	hw->subsystem_device_id = pci_get_subdevice(dev);

	/*
	 * Make sure BUSMASTER is set
	 */
	pci_enable_busmaster(dev);

	/* Do base PCI setup - map BAR0 */
	if (ixgbe_allocate_pci_resources(adapter)) {
		device_printf(dev, "Allocation of PCI resources failed\n");
		error = ENXIO;
		goto err_out;
	}

	/* let hardware know driver is loaded */
	ctrl_ext = IXGBE_READ_REG(hw, IXGBE_CTRL_EXT);
	ctrl_ext |= IXGBE_CTRL_EXT_DRV_LOAD;
	IXGBE_WRITE_REG(hw, IXGBE_CTRL_EXT, ctrl_ext);

	hw->allow_unsupported_sfp = allow_unsupported_sfp;

	/*
	 * Initialize the shared code
	 */
	if (ixgbe_init_shared_code(hw)) {
		device_printf(dev, "Unable to initialize the shared code\n");
		error = ENXIO;
		goto err_out;
	}

	if (hw->mbx.ops.init_params)
		hw->mbx.ops.init_params(hw);


	/* Pick up the 82599 settings */
	if (hw->mac.type != ixgbe_mac_82598EB) {
		hw->phy.smart_speed = ixgbe_smart_speed;
		adapter->num_segs = IXGBE_82599_SCATTER;
	} else
		adapter->num_segs = IXGBE_82598_SCATTER;

	ixgbe_init_device_features(adapter);

	if (ixgbe_configure_interrupts(adapter)) {
		error = ENXIO;
		goto err_out;
	}

	/* Allocate multicast array memory. */
	adapter->mta = malloc(sizeof(*adapter->mta) *
	    MAX_NUM_MULTICAST_ADDRESSES, M_IXGBE, M_NOWAIT);
	if (adapter->mta == NULL) {
		device_printf(dev, "Can not allocate multicast setup array\n");
		error = ENOMEM;
		goto err_out;
	}

	/* Enable WoL (if supported) */
	ixgbe_check_wol_support(adapter);

	/* Register for VLAN events */
	adapter->vlan_attach = EVENTHANDLER_REGISTER(vlan_config,
	    ixgbe_register_vlan, adapter, EVENTHANDLER_PRI_FIRST);
	adapter->vlan_detach = EVENTHANDLER_REGISTER(vlan_unconfig,
	    ixgbe_unregister_vlan, adapter, EVENTHANDLER_PRI_FIRST);

	/* Verify adapter fan is still functional (if applicable) */
	if (adapter->feat_en & IXGBE_FEATURE_FAN_FAIL) {
		u32 esdp = IXGBE_READ_REG(hw, IXGBE_ESDP);
		ixgbe_check_fan_failure(adapter, esdp, FALSE);
	}

	/* Ensure SW/FW semaphore is free */
	ixgbe_init_swfw_semaphore(hw);

	/* Enable EEE power saving */
	if (adapter->feat_en & IXGBE_FEATURE_EEE)
		hw->mac.ops.setup_eee(hw, TRUE);

	/* Set an initial default flow control value */
	hw->fc.requested_mode = ixgbe_flow_control;

	/* Sysctls for limiting the amount of work done in the taskqueues */
	ixgbe_set_sysctl_value(adapter, "rx_processing_limit",
	    "max number of rx packets to process",
	    &adapter->rx_process_limit, ixgbe_rx_process_limit);

	ixgbe_set_sysctl_value(adapter, "tx_processing_limit",
	    "max number of tx packets to process",
	    &adapter->tx_process_limit, ixgbe_tx_process_limit);

	/* Do descriptor calc and sanity checks */
	if (((ixgbe_txd * sizeof(union ixgbe_adv_tx_desc)) % DBA_ALIGN) != 0 ||
	    ixgbe_txd < MIN_TXD || ixgbe_txd > MAX_TXD) {
		device_printf(dev, "TXD config issue, using default!\n");
		adapter->num_tx_desc = DEFAULT_TXD;
	} else
		adapter->num_tx_desc = ixgbe_txd;

	/*
	 * With many RX rings it is easy to exceed the
	 * system mbuf allocation. Tuning nmbclusters
	 * can alleviate this.
	 */
	if (nmbclusters > 0) {
		int s;
		s = (ixgbe_rxd * adapter->num_queues) * ixgbe_total_ports;
		if (s > nmbclusters) {
			device_printf(dev, "RX Descriptors exceed system mbuf max, using default instead!\n");
			ixgbe_rxd = DEFAULT_RXD;
		}
	}

	if (((ixgbe_rxd * sizeof(union ixgbe_adv_rx_desc)) % DBA_ALIGN) != 0 ||
	    ixgbe_rxd < MIN_RXD || ixgbe_rxd > MAX_RXD) {
		device_printf(dev, "RXD config issue, using default!\n");
		adapter->num_rx_desc = DEFAULT_RXD;
	} else
		adapter->num_rx_desc = ixgbe_rxd;

	/* Allocate our TX/RX Queues */
	if (ixgbe_allocate_queues(adapter)) {
		error = ENOMEM;
		goto err_out;
	}

	hw->phy.reset_if_overtemp = TRUE;
	error = ixgbe_reset_hw(hw);
	hw->phy.reset_if_overtemp = FALSE;
	if (error == IXGBE_ERR_SFP_NOT_PRESENT) {
		/*
		 * No optics in this port, set up
		 * so the timer routine will probe
		 * for later insertion.
		 */
		adapter->sfp_probe = TRUE;
		error = IXGBE_SUCCESS;
	} else if (error == IXGBE_ERR_SFP_NOT_SUPPORTED) {
		device_printf(dev, "Unsupported SFP+ module detected!\n");
		error = EIO;
		goto err_late;
	} else if (error) {
		device_printf(dev, "Hardware initialization failed\n");
		error = EIO;
		goto err_late;
	}

	/* Make sure we have a good EEPROM before we read from it */
	if (ixgbe_validate_eeprom_checksum(&adapter->hw, NULL) < 0) {
		device_printf(dev, "The EEPROM Checksum Is Not Valid\n");
		error = EIO;
		goto err_late;
	}

	/* Setup OS specific network interface */
	if (ixgbe_setup_interface(dev, adapter) != 0)
		goto err_late;

	if (adapter->feat_en & IXGBE_FEATURE_MSIX)
		error = ixgbe_allocate_msix(adapter);
	else
		error = ixgbe_allocate_legacy(adapter);
	if (error)
		goto err_late;

	error = ixgbe_start_hw(hw);
	switch (error) {
	case IXGBE_ERR_EEPROM_VERSION:
		device_printf(dev, "This device is a pre-production adapter/LOM.  Please be aware there may be issues associated with your hardware.\nIf you are experiencing problems please contact your Intel or hardware representative who provided you with this hardware.\n");
		break;
	case IXGBE_ERR_SFP_NOT_SUPPORTED:
		device_printf(dev, "Unsupported SFP+ Module\n");
		error = EIO;
		goto err_late;
	case IXGBE_ERR_SFP_NOT_PRESENT:
		device_printf(dev, "No SFP+ Module found\n");
		/* falls thru */
	default:
		break;
	}

	/* Enable the optics for 82599 SFP+ fiber */
	ixgbe_enable_tx_laser(hw);

	/* Enable power to the phy. */
	ixgbe_set_phy_power(hw, TRUE);

	/* Initialize statistics */
	ixgbe_update_stats_counters(adapter);

	/* Check PCIE slot type/speed/width */
	ixgbe_get_slot_info(adapter);

	/*
	 * Do time init and sysctl init here, but
	 * only on the first port of a bypass adapter.
	 */
	ixgbe_bypass_init(adapter);

	/* Set an initial dmac value */
	adapter->dmac = 0;
	/* Set initial advertised speeds (if applicable) */
	adapter->advertise = ixgbe_get_advertise(adapter);

	if (adapter->feat_cap & IXGBE_FEATURE_SRIOV)
		ixgbe_define_iov_schemas(dev, &error);

	/* Add sysctls */
	ixgbe_add_device_sysctls(adapter);
	ixgbe_add_hw_stats(adapter);

	/* For Netmap */
	adapter->init_locked = ixgbe_init_locked;
	adapter->stop_locked = ixgbe_stop;

	if (adapter->feat_en & IXGBE_FEATURE_NETMAP)
		ixgbe_netmap_attach(adapter);

	/* Initialize Admin Task */
	TASK_INIT(&adapter->admin_task, 0, ixgbe_handle_admin_task, adapter);

	/* Initialize task queue */
	adapter->tq = taskqueue_create_fast("ixgbe_admin", M_NOWAIT,
	    taskqueue_thread_enqueue, &adapter->tq);
	taskqueue_start_threads(&adapter->tq, 1, PI_NET, "%s admintaskq",
	    device_get_nameunit(adapter->dev));

	INIT_DEBUGOUT("ixgbe_attach: end");

	return (0);

err_late:
	ixgbe_free_transmit_structures(adapter);
	ixgbe_free_receive_structures(adapter);
	free(adapter->queues, M_DEVBUF);
err_out:
	if (adapter->ifp != NULL)
		if_free(adapter->ifp);
	ctrl_ext = IXGBE_READ_REG(&adapter->hw, IXGBE_CTRL_EXT);
	ctrl_ext &= ~IXGBE_CTRL_EXT_DRV_LOAD;
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_CTRL_EXT, ctrl_ext);
	ixgbe_free_pci_resources(adapter);
	free(adapter->mta, M_IXGBE);
	IXGBE_CORE_LOCK_DESTROY(adapter);

	return (error);
} /* ixgbe_attach */

/************************************************************************
 * ixgbe_check_wol_support
 *
 *   Checks whether the adapter's ports are capable of
 *   Wake On LAN by reading the adapter's NVM.
 *
 *   Sets each port's hw->wol_enabled value depending
 *   on the value read here.
 ************************************************************************/
static void
ixgbe_check_wol_support(struct adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;
	u16             dev_caps = 0;

	/* Find out WoL support for port */
	adapter->wol_support = hw->wol_enabled = 0;
	ixgbe_get_device_caps(hw, &dev_caps);
	if ((dev_caps & IXGBE_DEVICE_CAPS_WOL_PORT0_1) ||
	    ((dev_caps & IXGBE_DEVICE_CAPS_WOL_PORT0) &&
	     hw->bus.func == 0))
		adapter->wol_support = hw->wol_enabled = 1;

	/* Save initial wake up filter configuration */
	adapter->wufc = IXGBE_READ_REG(hw, IXGBE_WUFC);

	return;
} /* ixgbe_check_wol_support */

/************************************************************************
 * ixgbe_setup_interface
 *
 *   Setup networking device structure and register an interface.
 ************************************************************************/
static int
ixgbe_setup_interface(device_t dev, struct adapter *adapter)
{
	struct ifnet *ifp;

	INIT_DEBUGOUT("ixgbe_setup_interface: begin");

	ifp = adapter->ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		device_printf(dev, "can not allocate ifnet structure\n");
		return (-1);
	}
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_baudrate = IF_Gbps(10);
	ifp->if_init = ixgbe_init;
	ifp->if_softc = adapter;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = ixgbe_ioctl;
#if __FreeBSD_version >= 1100036
	if_setgetcounterfn(ifp, ixgbe_get_counter);
#endif
#if __FreeBSD_version >= 1100045
	/* TSO parameters */
	ifp->if_hw_tsomax = 65518;
	ifp->if_hw_tsomaxsegcount = IXGBE_82599_SCATTER;
	ifp->if_hw_tsomaxsegsize = 2048;
#endif
	if (adapter->feat_en & IXGBE_FEATURE_LEGACY_TX) {
		ifp->if_start = ixgbe_legacy_start;
		IFQ_SET_MAXLEN(&ifp->if_snd, adapter->num_tx_desc - 2);
		ifp->if_snd.ifq_drv_maxlen = adapter->num_tx_desc - 2;
		IFQ_SET_READY(&ifp->if_snd);
		ixgbe_start_locked = ixgbe_legacy_start_locked;
		ixgbe_ring_empty = ixgbe_legacy_ring_empty;
	} else {
		ifp->if_transmit = ixgbe_mq_start;
		ifp->if_qflush = ixgbe_qflush;
		ixgbe_start_locked = ixgbe_mq_start_locked;
		ixgbe_ring_empty = drbr_empty;
	}

	ether_ifattach(ifp, adapter->hw.mac.addr);

	adapter->max_frame_size = ifp->if_mtu + ETHER_HDR_LEN + ETHER_CRC_LEN;

	/*
	 * Tell the upper layer(s) we support long frames.
	 */
	ifp->if_hdrlen = sizeof(struct ether_vlan_header);

	/* Set capability flags */
	ifp->if_capabilities |= IFCAP_HWCSUM
	                     |  IFCAP_HWCSUM_IPV6
	                     |  IFCAP_TSO
	                     |  IFCAP_LRO
	                     |  IFCAP_VLAN_HWTAGGING
	                     |  IFCAP_VLAN_HWTSO
	                     |  IFCAP_VLAN_HWCSUM
	                     |  IFCAP_JUMBO_MTU
	                     |  IFCAP_VLAN_MTU
	                     |  IFCAP_HWSTATS;

	/* Enable the above capabilities by default */
	ifp->if_capenable = ifp->if_capabilities;

	/*
	 * Don't turn this on by default, if vlans are
	 * created on another pseudo device (eg. lagg)
	 * then vlan events are not passed thru, breaking
	 * operation, but with HW FILTER off it works. If
	 * using vlans directly on the ixgbe driver you can
	 * enable this and get full hardware tag filtering.
	 */
	ifp->if_capabilities |= IFCAP_VLAN_HWFILTER;

	/*
	 * Specify the media types supported by this adapter and register
	 * callbacks to update media and link information
	 */
	ifmedia_init(&adapter->media, IFM_IMASK, ixgbe_media_change,
	    ixgbe_media_status);

	adapter->phy_layer = ixgbe_get_supported_physical_layer(&adapter->hw);
	ixgbe_add_media_types(adapter);

	/* Set autoselect media by default */
	ifmedia_set(&adapter->media, IFM_ETHER | IFM_AUTO);

	return (0);
} /* ixgbe_setup_interface */

#if __FreeBSD_version >= 1100036
/************************************************************************
 * ixgbe_get_counter
 ************************************************************************/
static uint64_t
ixgbe_get_counter(struct ifnet *ifp, ift_counter cnt)
{
	struct adapter *adapter;
	struct tx_ring *txr;
	uint64_t       rv;

	adapter = if_getsoftc(ifp);

	switch (cnt) {
	case IFCOUNTER_IPACKETS:
		return (adapter->ipackets);
	case IFCOUNTER_OPACKETS:
		return (adapter->opackets);
	case IFCOUNTER_IBYTES:
		return (adapter->ibytes);
	case IFCOUNTER_OBYTES:
		return (adapter->obytes);
	case IFCOUNTER_IMCASTS:
		return (adapter->imcasts);
	case IFCOUNTER_OMCASTS:
		return (adapter->omcasts);
	case IFCOUNTER_COLLISIONS:
		return (0);
	case IFCOUNTER_IQDROPS:
		return (adapter->iqdrops);
	case IFCOUNTER_OQDROPS:
		rv = 0;
		txr = adapter->tx_rings;
		for (int i = 0; i < adapter->num_queues; i++, txr++)
			rv += txr->br->br_drops;
		return (rv);
	case IFCOUNTER_IERRORS:
		return (adapter->ierrors);
	default:
		return (if_get_counter_default(ifp, cnt));
	}
} /* ixgbe_get_counter */
#endif

/************************************************************************
 * ixgbe_add_media_types
 ************************************************************************/
static void
ixgbe_add_media_types(struct adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;
	device_t        dev = adapter->dev;
	u64             layer;

	layer = adapter->phy_layer;

	/* Media types with matching FreeBSD media defines */
	if (layer & IXGBE_PHYSICAL_LAYER_10GBASE_T)
		ifmedia_add(&adapter->media, IFM_ETHER | IFM_10G_T, 0, NULL);
	if (layer & IXGBE_PHYSICAL_LAYER_1000BASE_T)
		ifmedia_add(&adapter->media, IFM_ETHER | IFM_1000_T, 0, NULL);
	if (layer & IXGBE_PHYSICAL_LAYER_100BASE_TX)
		ifmedia_add(&adapter->media, IFM_ETHER | IFM_100_TX, 0, NULL);
	if (layer & IXGBE_PHYSICAL_LAYER_10BASE_T)
		ifmedia_add(&adapter->media, IFM_ETHER | IFM_10_T, 0, NULL);

	if (layer & IXGBE_PHYSICAL_LAYER_SFP_PLUS_CU ||
	    layer & IXGBE_PHYSICAL_LAYER_SFP_ACTIVE_DA)
		ifmedia_add(&adapter->media, IFM_ETHER | IFM_10G_TWINAX, 0,
		    NULL);

	if (layer & IXGBE_PHYSICAL_LAYER_10GBASE_LR) {
		ifmedia_add(&adapter->media, IFM_ETHER | IFM_10G_LR, 0, NULL);
		if (hw->phy.multispeed_fiber)
			ifmedia_add(&adapter->media, IFM_ETHER | IFM_1000_LX, 0,
			    NULL);
	}
	if (layer & IXGBE_PHYSICAL_LAYER_10GBASE_SR) {
		ifmedia_add(&adapter->media, IFM_ETHER | IFM_10G_SR, 0, NULL);
		if (hw->phy.multispeed_fiber)
			ifmedia_add(&adapter->media, IFM_ETHER | IFM_1000_SX, 0,
			    NULL);
	} else if (layer & IXGBE_PHYSICAL_LAYER_1000BASE_SX)
		ifmedia_add(&adapter->media, IFM_ETHER | IFM_1000_SX, 0, NULL);
	if (layer & IXGBE_PHYSICAL_LAYER_10GBASE_CX4)
		ifmedia_add(&adapter->media, IFM_ETHER | IFM_10G_CX4, 0, NULL);

#ifdef IFM_ETH_XTYPE
	if (layer & IXGBE_PHYSICAL_LAYER_10GBASE_KR)
		ifmedia_add(&adapter->media, IFM_ETHER | IFM_10G_KR, 0, NULL);
	if (layer & IXGBE_PHYSICAL_LAYER_10GBASE_KX4)
		ifmedia_add(&adapter->media, IFM_ETHER | IFM_10G_KX4, 0, NULL);
	if (layer & IXGBE_PHYSICAL_LAYER_1000BASE_KX)
		ifmedia_add(&adapter->media, IFM_ETHER | IFM_1000_KX, 0, NULL);
	if (layer & IXGBE_PHYSICAL_LAYER_2500BASE_KX)
		ifmedia_add(&adapter->media, IFM_ETHER | IFM_2500_KX, 0, NULL);
#else
	if (layer & IXGBE_PHYSICAL_LAYER_10GBASE_KR) {
		device_printf(dev, "Media supported: 10GbaseKR\n");
		device_printf(dev, "10GbaseKR mapped to 10GbaseSR\n");
		ifmedia_add(&adapter->media, IFM_ETHER | IFM_10G_SR, 0, NULL);
	}
	if (layer & IXGBE_PHYSICAL_LAYER_10GBASE_KX4) {
		device_printf(dev, "Media supported: 10GbaseKX4\n");
		device_printf(dev, "10GbaseKX4 mapped to 10GbaseCX4\n");
		ifmedia_add(&adapter->media, IFM_ETHER | IFM_10G_CX4, 0, NULL);
	}
	if (layer & IXGBE_PHYSICAL_LAYER_1000BASE_KX) {
		device_printf(dev, "Media supported: 1000baseKX\n");
		device_printf(dev, "1000baseKX mapped to 1000baseCX\n");
		ifmedia_add(&adapter->media, IFM_ETHER | IFM_1000_CX, 0, NULL);
	}
	if (layer & IXGBE_PHYSICAL_LAYER_2500BASE_KX) {
		device_printf(dev, "Media supported: 2500baseKX\n");
		device_printf(dev, "2500baseKX mapped to 2500baseSX\n");
		ifmedia_add(&adapter->media, IFM_ETHER | IFM_2500_SX, 0, NULL);
	}
#endif
	if (layer & IXGBE_PHYSICAL_LAYER_1000BASE_BX)
		device_printf(dev, "Media supported: 1000baseBX\n");

	if (hw->device_id == IXGBE_DEV_ID_82598AT) {
		ifmedia_add(&adapter->media, IFM_ETHER | IFM_1000_T | IFM_FDX,
		    0, NULL);
		ifmedia_add(&adapter->media, IFM_ETHER | IFM_1000_T, 0, NULL);
	}

	ifmedia_add(&adapter->media, IFM_ETHER | IFM_AUTO, 0, NULL);
} /* ixgbe_add_media_types */

/************************************************************************
 * ixgbe_is_sfp
 ************************************************************************/
static inline bool
ixgbe_is_sfp(struct ixgbe_hw *hw)
{
	switch (hw->mac.type) {
	case ixgbe_mac_82598EB:
		if (hw->phy.type == ixgbe_phy_nl)
			return TRUE;
		return FALSE;
	case ixgbe_mac_82599EB:
		switch (hw->mac.ops.get_media_type(hw)) {
		case ixgbe_media_type_fiber:
		case ixgbe_media_type_fiber_qsfp:
			return TRUE;
		default:
			return FALSE;
		}
	case ixgbe_mac_X550EM_x:
	case ixgbe_mac_X550EM_a:
		if (hw->mac.ops.get_media_type(hw) == ixgbe_media_type_fiber)
			return TRUE;
		return FALSE;
	default:
		return FALSE;
	}
} /* ixgbe_is_sfp */

/************************************************************************
 * ixgbe_config_link
 ************************************************************************/
static void
ixgbe_config_link(struct adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;
	u32             autoneg, err = 0;
	bool            sfp, negotiate;

	sfp = ixgbe_is_sfp(hw);

	if (sfp) {
		if (hw->phy.multispeed_fiber) {
			hw->mac.ops.setup_sfp(hw);
			ixgbe_enable_tx_laser(hw);
			adapter->task_requests |= IXGBE_REQUEST_TASK_MSF;
			taskqueue_enqueue(adapter->tq, &adapter->admin_task);
		} else {
			adapter->task_requests |= IXGBE_REQUEST_TASK_MOD;
			taskqueue_enqueue(adapter->tq, &adapter->admin_task);
		}
	} else {
		if (hw->mac.ops.check_link)
			err = ixgbe_check_link(hw, &adapter->link_speed,
			    &adapter->link_up, FALSE);
		if (err)
			goto out;
		autoneg = hw->phy.autoneg_advertised;
		if ((!autoneg) && (hw->mac.ops.get_link_capabilities))
			err = hw->mac.ops.get_link_capabilities(hw, &autoneg,
			    &negotiate);
		if (err)
			goto out;
		if (hw->mac.ops.setup_link)
			err = hw->mac.ops.setup_link(hw, autoneg,
			    adapter->link_up);
	}
out:

	return;
} /* ixgbe_config_link */

/************************************************************************
 * ixgbe_update_stats_counters - Update board statistics counters.
 ************************************************************************/
static void
ixgbe_update_stats_counters(struct adapter *adapter)
{
	struct ixgbe_hw       *hw = &adapter->hw;
	struct ixgbe_hw_stats *stats = &adapter->stats.pf;
	u32                   missed_rx = 0, bprc, lxon, lxoff, total;
	u64                   total_missed_rx = 0;

	stats->crcerrs += IXGBE_READ_REG(hw, IXGBE_CRCERRS);
	stats->illerrc += IXGBE_READ_REG(hw, IXGBE_ILLERRC);
	stats->errbc += IXGBE_READ_REG(hw, IXGBE_ERRBC);
	stats->mspdc += IXGBE_READ_REG(hw, IXGBE_MSPDC);
	stats->mpc[0] += IXGBE_READ_REG(hw, IXGBE_MPC(0));

	for (int i = 0; i < 16; i++) {
		stats->qprc[i] += IXGBE_READ_REG(hw, IXGBE_QPRC(i));
		stats->qptc[i] += IXGBE_READ_REG(hw, IXGBE_QPTC(i));
		stats->qprdc[i] += IXGBE_READ_REG(hw, IXGBE_QPRDC(i));
	}
	stats->mlfc += IXGBE_READ_REG(hw, IXGBE_MLFC);
	stats->mrfc += IXGBE_READ_REG(hw, IXGBE_MRFC);
	stats->rlec += IXGBE_READ_REG(hw, IXGBE_RLEC);

	/* Hardware workaround, gprc counts missed packets */
	stats->gprc += IXGBE_READ_REG(hw, IXGBE_GPRC);
	stats->gprc -= missed_rx;

	if (hw->mac.type != ixgbe_mac_82598EB) {
		stats->gorc += IXGBE_READ_REG(hw, IXGBE_GORCL) +
		    ((u64)IXGBE_READ_REG(hw, IXGBE_GORCH) << 32);
		stats->gotc += IXGBE_READ_REG(hw, IXGBE_GOTCL) +
		    ((u64)IXGBE_READ_REG(hw, IXGBE_GOTCH) << 32);
		stats->tor += IXGBE_READ_REG(hw, IXGBE_TORL) +
		    ((u64)IXGBE_READ_REG(hw, IXGBE_TORH) << 32);
		stats->lxonrxc += IXGBE_READ_REG(hw, IXGBE_LXONRXCNT);
		stats->lxoffrxc += IXGBE_READ_REG(hw, IXGBE_LXOFFRXCNT);
	} else {
		stats->lxonrxc += IXGBE_READ_REG(hw, IXGBE_LXONRXC);
		stats->lxoffrxc += IXGBE_READ_REG(hw, IXGBE_LXOFFRXC);
		/* 82598 only has a counter in the high register */
		stats->gorc += IXGBE_READ_REG(hw, IXGBE_GORCH);
		stats->gotc += IXGBE_READ_REG(hw, IXGBE_GOTCH);
		stats->tor += IXGBE_READ_REG(hw, IXGBE_TORH);
	}

	/*
	 * Workaround: mprc hardware is incorrectly counting
	 * broadcasts, so for now we subtract those.
	 */
	bprc = IXGBE_READ_REG(hw, IXGBE_BPRC);
	stats->bprc += bprc;
	stats->mprc += IXGBE_READ_REG(hw, IXGBE_MPRC);
	if (hw->mac.type == ixgbe_mac_82598EB)
		stats->mprc -= bprc;

	stats->prc64 += IXGBE_READ_REG(hw, IXGBE_PRC64);
	stats->prc127 += IXGBE_READ_REG(hw, IXGBE_PRC127);
	stats->prc255 += IXGBE_READ_REG(hw, IXGBE_PRC255);
	stats->prc511 += IXGBE_READ_REG(hw, IXGBE_PRC511);
	stats->prc1023 += IXGBE_READ_REG(hw, IXGBE_PRC1023);
	stats->prc1522 += IXGBE_READ_REG(hw, IXGBE_PRC1522);

	lxon = IXGBE_READ_REG(hw, IXGBE_LXONTXC);
	stats->lxontxc += lxon;
	lxoff = IXGBE_READ_REG(hw, IXGBE_LXOFFTXC);
	stats->lxofftxc += lxoff;
	total = lxon + lxoff;

	stats->gptc += IXGBE_READ_REG(hw, IXGBE_GPTC);
	stats->mptc += IXGBE_READ_REG(hw, IXGBE_MPTC);
	stats->ptc64 += IXGBE_READ_REG(hw, IXGBE_PTC64);
	stats->gptc -= total;
	stats->mptc -= total;
	stats->ptc64 -= total;
	stats->gotc -= total * ETHER_MIN_LEN;

	stats->ruc += IXGBE_READ_REG(hw, IXGBE_RUC);
	stats->rfc += IXGBE_READ_REG(hw, IXGBE_RFC);
	stats->roc += IXGBE_READ_REG(hw, IXGBE_ROC);
	stats->rjc += IXGBE_READ_REG(hw, IXGBE_RJC);
	stats->mngprc += IXGBE_READ_REG(hw, IXGBE_MNGPRC);
	stats->mngpdc += IXGBE_READ_REG(hw, IXGBE_MNGPDC);
	stats->mngptc += IXGBE_READ_REG(hw, IXGBE_MNGPTC);
	stats->tpr += IXGBE_READ_REG(hw, IXGBE_TPR);
	stats->tpt += IXGBE_READ_REG(hw, IXGBE_TPT);
	stats->ptc127 += IXGBE_READ_REG(hw, IXGBE_PTC127);
	stats->ptc255 += IXGBE_READ_REG(hw, IXGBE_PTC255);
	stats->ptc511 += IXGBE_READ_REG(hw, IXGBE_PTC511);
	stats->ptc1023 += IXGBE_READ_REG(hw, IXGBE_PTC1023);
	stats->ptc1522 += IXGBE_READ_REG(hw, IXGBE_PTC1522);
	stats->bptc += IXGBE_READ_REG(hw, IXGBE_BPTC);
	stats->xec += IXGBE_READ_REG(hw, IXGBE_XEC);
	stats->fccrc += IXGBE_READ_REG(hw, IXGBE_FCCRC);
	stats->fclast += IXGBE_READ_REG(hw, IXGBE_FCLAST);
	/* Only read FCOE on 82599 */
	if (hw->mac.type != ixgbe_mac_82598EB) {
		stats->fcoerpdc += IXGBE_READ_REG(hw, IXGBE_FCOERPDC);
		stats->fcoeprc += IXGBE_READ_REG(hw, IXGBE_FCOEPRC);
		stats->fcoeptc += IXGBE_READ_REG(hw, IXGBE_FCOEPTC);
		stats->fcoedwrc += IXGBE_READ_REG(hw, IXGBE_FCOEDWRC);
		stats->fcoedwtc += IXGBE_READ_REG(hw, IXGBE_FCOEDWTC);
	}

	/* Fill out the OS statistics structure */
	IXGBE_SET_IPACKETS(adapter, stats->gprc);
	IXGBE_SET_OPACKETS(adapter, stats->gptc);
	IXGBE_SET_IBYTES(adapter, stats->gorc);
	IXGBE_SET_OBYTES(adapter, stats->gotc);
	IXGBE_SET_IMCASTS(adapter, stats->mprc);
	IXGBE_SET_OMCASTS(adapter, stats->mptc);
	IXGBE_SET_COLLISIONS(adapter, 0);
	IXGBE_SET_IQDROPS(adapter, total_missed_rx);
	IXGBE_SET_IERRORS(adapter, stats->crcerrs + stats->rlec);
} /* ixgbe_update_stats_counters */

/************************************************************************
 * ixgbe_add_hw_stats
 *
 *   Add sysctl variables, one per statistic, to the system.
 ************************************************************************/
static void
ixgbe_add_hw_stats(struct adapter *adapter)
{
	device_t               dev = adapter->dev;
	struct tx_ring         *txr = adapter->tx_rings;
	struct rx_ring         *rxr = adapter->rx_rings;
	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(dev);
	struct sysctl_oid      *tree = device_get_sysctl_tree(dev);
	struct sysctl_oid_list *child = SYSCTL_CHILDREN(tree);
	struct ixgbe_hw_stats  *stats = &adapter->stats.pf;
	struct sysctl_oid      *stat_node, *queue_node;
	struct sysctl_oid_list *stat_list, *queue_list;

#define QUEUE_NAME_LEN 32
	char                   namebuf[QUEUE_NAME_LEN];

	/* Driver Statistics */
	SYSCTL_ADD_ULONG(ctx, child, OID_AUTO, "dropped",
	    CTLFLAG_RD, &adapter->dropped_pkts, "Driver dropped packets");
	SYSCTL_ADD_ULONG(ctx, child, OID_AUTO, "mbuf_defrag_failed",
	    CTLFLAG_RD, &adapter->mbuf_defrag_failed, "m_defrag() failed");
	SYSCTL_ADD_ULONG(ctx, child, OID_AUTO, "watchdog_events",
	    CTLFLAG_RD, &adapter->watchdog_events, "Watchdog timeouts");
	SYSCTL_ADD_ULONG(ctx, child, OID_AUTO, "link_irq",
	    CTLFLAG_RD, &adapter->link_irq, "Link MSI-X IRQ Handled");

	for (int i = 0; i < adapter->num_queues; i++, txr++) {
		snprintf(namebuf, QUEUE_NAME_LEN, "queue%d", i);
		queue_node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, namebuf,
		    CTLFLAG_RD, NULL, "Queue Name");
		queue_list = SYSCTL_CHILDREN(queue_node);

		SYSCTL_ADD_PROC(ctx, queue_list, OID_AUTO, "interrupt_rate",
		    CTLTYPE_UINT | CTLFLAG_RW, &adapter->queues[i],
		    sizeof(&adapter->queues[i]),
		    ixgbe_sysctl_interrupt_rate_handler, "IU",
		    "Interrupt Rate");
		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "irqs",
		    CTLFLAG_RD, &(adapter->queues[i].irqs),
		    "irqs on this queue");
		SYSCTL_ADD_PROC(ctx, queue_list, OID_AUTO, "txd_head",
		    CTLTYPE_UINT | CTLFLAG_RD, txr, sizeof(txr),
		    ixgbe_sysctl_tdh_handler, "IU", "Transmit Descriptor Head");
		SYSCTL_ADD_PROC(ctx, queue_list, OID_AUTO, "txd_tail",
		    CTLTYPE_UINT | CTLFLAG_RD, txr, sizeof(txr),
		    ixgbe_sysctl_tdt_handler, "IU", "Transmit Descriptor Tail");
		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "tso_tx",
		    CTLFLAG_RD, &txr->tso_tx, "TSO");
		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "no_tx_dma_setup",
		    CTLFLAG_RD, &txr->no_tx_dma_setup,
		    "Driver tx dma failure in xmit");
		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "no_desc_avail",
		    CTLFLAG_RD, &txr->no_desc_avail,
		    "Queue No Descriptor Available");
		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "tx_packets",
		    CTLFLAG_RD, &txr->total_packets,
		    "Queue Packets Transmitted");
		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "br_drops",
		    CTLFLAG_RD, &txr->br->br_drops,
		    "Packets dropped in buf_ring");
	}

	for (int i = 0; i < adapter->num_queues; i++, rxr++) {
		struct lro_ctrl *lro = &rxr->lro;

		snprintf(namebuf, QUEUE_NAME_LEN, "queue%d", i);
		queue_node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, namebuf,
		    CTLFLAG_RD, NULL, "Queue Name");
		queue_list = SYSCTL_CHILDREN(queue_node);

		SYSCTL_ADD_PROC(ctx, queue_list, OID_AUTO, "rxd_head",
		    CTLTYPE_UINT | CTLFLAG_RD, rxr, sizeof(rxr),
		    ixgbe_sysctl_rdh_handler, "IU", "Receive Descriptor Head");
		SYSCTL_ADD_PROC(ctx, queue_list, OID_AUTO, "rxd_tail",
		    CTLTYPE_UINT | CTLFLAG_RD, rxr, sizeof(rxr),
		    ixgbe_sysctl_rdt_handler, "IU", "Receive Descriptor Tail");
		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "rx_packets",
		    CTLFLAG_RD, &rxr->rx_packets, "Queue Packets Received");
		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "rx_bytes",
		    CTLFLAG_RD, &rxr->rx_bytes, "Queue Bytes Received");
		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "rx_copies",
		    CTLFLAG_RD, &rxr->rx_copies, "Copied RX Frames");
		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "rx_discarded",
		    CTLFLAG_RD, &rxr->rx_discarded, "Discarded RX packets");
		SYSCTL_ADD_U64(ctx, queue_list, OID_AUTO, "lro_queued",
		    CTLFLAG_RD, &lro->lro_queued, 0, "LRO Queued");
		SYSCTL_ADD_U64(ctx, queue_list, OID_AUTO, "lro_flushed",
		    CTLFLAG_RD, &lro->lro_flushed, 0, "LRO Flushed");
	}

	/* MAC stats get their own sub node */

	stat_node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, "mac_stats",
	    CTLFLAG_RD, NULL, "MAC Statistics");
	stat_list = SYSCTL_CHILDREN(stat_node);

	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "crc_errs",
	    CTLFLAG_RD, &stats->crcerrs, "CRC Errors");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "ill_errs",
	    CTLFLAG_RD, &stats->illerrc, "Illegal Byte Errors");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "byte_errs",
	    CTLFLAG_RD, &stats->errbc, "Byte Errors");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "short_discards",
	    CTLFLAG_RD, &stats->mspdc, "MAC Short Packets Discarded");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "local_faults",
	    CTLFLAG_RD, &stats->mlfc, "MAC Local Faults");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "remote_faults",
	    CTLFLAG_RD, &stats->mrfc, "MAC Remote Faults");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "rec_len_errs",
	    CTLFLAG_RD, &stats->rlec, "Receive Length Errors");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "rx_missed_packets",
	    CTLFLAG_RD, &stats->mpc[0], "RX Missed Packet Count");

	/* Flow Control stats */
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "xon_txd",
	    CTLFLAG_RD, &stats->lxontxc, "Link XON Transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "xon_recvd",
	    CTLFLAG_RD, &stats->lxonrxc, "Link XON Received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "xoff_txd",
	    CTLFLAG_RD, &stats->lxofftxc, "Link XOFF Transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "xoff_recvd",
	    CTLFLAG_RD, &stats->lxoffrxc, "Link XOFF Received");

	/* Packet Reception Stats */
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "total_octets_rcvd",
	    CTLFLAG_RD, &stats->tor, "Total Octets Received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "good_octets_rcvd",
	    CTLFLAG_RD, &stats->gorc, "Good Octets Received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "total_pkts_rcvd",
	    CTLFLAG_RD, &stats->tpr, "Total Packets Received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "good_pkts_rcvd",
	    CTLFLAG_RD, &stats->gprc, "Good Packets Received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "mcast_pkts_rcvd",
	    CTLFLAG_RD, &stats->mprc, "Multicast Packets Received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "bcast_pkts_rcvd",
	    CTLFLAG_RD, &stats->bprc, "Broadcast Packets Received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "rx_frames_64",
	    CTLFLAG_RD, &stats->prc64, "64 byte frames received ");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "rx_frames_65_127",
	    CTLFLAG_RD, &stats->prc127, "65-127 byte frames received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "rx_frames_128_255",
	    CTLFLAG_RD, &stats->prc255, "128-255 byte frames received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "rx_frames_256_511",
	    CTLFLAG_RD, &stats->prc511, "256-511 byte frames received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "rx_frames_512_1023",
	    CTLFLAG_RD, &stats->prc1023, "512-1023 byte frames received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "rx_frames_1024_1522",
	    CTLFLAG_RD, &stats->prc1522, "1023-1522 byte frames received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "recv_undersized",
	    CTLFLAG_RD, &stats->ruc, "Receive Undersized");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "recv_fragmented",
	    CTLFLAG_RD, &stats->rfc, "Fragmented Packets Received ");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "recv_oversized",
	    CTLFLAG_RD, &stats->roc, "Oversized Packets Received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "recv_jabberd",
	    CTLFLAG_RD, &stats->rjc, "Received Jabber");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "management_pkts_rcvd",
	    CTLFLAG_RD, &stats->mngprc, "Management Packets Received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "management_pkts_drpd",
	    CTLFLAG_RD, &stats->mngptc, "Management Packets Dropped");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "checksum_errs",
	    CTLFLAG_RD, &stats->xec, "Checksum Errors");

	/* Packet Transmission Stats */
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "good_octets_txd",
	    CTLFLAG_RD, &stats->gotc, "Good Octets Transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "total_pkts_txd",
	    CTLFLAG_RD, &stats->tpt, "Total Packets Transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "good_pkts_txd",
	    CTLFLAG_RD, &stats->gptc, "Good Packets Transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "bcast_pkts_txd",
	    CTLFLAG_RD, &stats->bptc, "Broadcast Packets Transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "mcast_pkts_txd",
	    CTLFLAG_RD, &stats->mptc, "Multicast Packets Transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "management_pkts_txd",
	    CTLFLAG_RD, &stats->mngptc, "Management Packets Transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "tx_frames_64",
	    CTLFLAG_RD, &stats->ptc64, "64 byte frames transmitted ");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "tx_frames_65_127",
	    CTLFLAG_RD, &stats->ptc127, "65-127 byte frames transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "tx_frames_128_255",
	    CTLFLAG_RD, &stats->ptc255, "128-255 byte frames transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "tx_frames_256_511",
	    CTLFLAG_RD, &stats->ptc511, "256-511 byte frames transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "tx_frames_512_1023",
	    CTLFLAG_RD, &stats->ptc1023, "512-1023 byte frames transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "tx_frames_1024_1522",
	    CTLFLAG_RD, &stats->ptc1522, "1024-1522 byte frames transmitted");
} /* ixgbe_add_hw_stats */

/************************************************************************
 * ixgbe_sysctl_tdh_handler - Transmit Descriptor Head handler function
 *
 *   Retrieves the TDH value from the hardware
 ************************************************************************/
static int
ixgbe_sysctl_tdh_handler(SYSCTL_HANDLER_ARGS)
{
	struct tx_ring *txr = ((struct tx_ring *)oidp->oid_arg1);
	int            error;
	unsigned int   val;

	if (!txr)
		return (0);

	val = IXGBE_READ_REG(&txr->adapter->hw, IXGBE_TDH(txr->me));
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error || !req->newptr)
		return error;

	return (0);
} /* ixgbe_sysctl_tdh_handler */

/************************************************************************
 * ixgbe_sysctl_tdt_handler - Transmit Descriptor Tail handler function
 *
 *   Retrieves the TDT value from the hardware
 ************************************************************************/
static int
ixgbe_sysctl_tdt_handler(SYSCTL_HANDLER_ARGS)
{
	struct tx_ring *txr = ((struct tx_ring *)oidp->oid_arg1);
	int            error;
	unsigned int   val;

	if (!txr)
		return (0);

	val = IXGBE_READ_REG(&txr->adapter->hw, IXGBE_TDT(txr->me));
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error || !req->newptr)
		return error;

	return (0);
} /* ixgbe_sysctl_tdt_handler */

/************************************************************************
 * ixgbe_sysctl_rdh_handler - Receive Descriptor Head handler function
 *
 *   Retrieves the RDH value from the hardware
 ************************************************************************/
static int
ixgbe_sysctl_rdh_handler(SYSCTL_HANDLER_ARGS)
{
	struct rx_ring *rxr = ((struct rx_ring *)oidp->oid_arg1);
	int            error;
	unsigned int   val;

	if (!rxr)
		return (0);

	val = IXGBE_READ_REG(&rxr->adapter->hw, IXGBE_RDH(rxr->me));
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error || !req->newptr)
		return error;

	return (0);
} /* ixgbe_sysctl_rdh_handler */

/************************************************************************
 * ixgbe_sysctl_rdt_handler - Receive Descriptor Tail handler function
 *
 *   Retrieves the RDT value from the hardware
 ************************************************************************/
static int
ixgbe_sysctl_rdt_handler(SYSCTL_HANDLER_ARGS)
{
	struct rx_ring *rxr = ((struct rx_ring *)oidp->oid_arg1);
	int            error;
	unsigned int   val;

	if (!rxr)
		return (0);

	val = IXGBE_READ_REG(&rxr->adapter->hw, IXGBE_RDT(rxr->me));
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error || !req->newptr)
		return error;

	return (0);
} /* ixgbe_sysctl_rdt_handler */

/************************************************************************
 * ixgbe_register_vlan
 *
 *   Run via vlan config EVENT, it enables us to use the
 *   HW Filter table since we can get the vlan id. This
 *   just creates the entry in the soft version of the
 *   VFTA, init will repopulate the real table.
 ************************************************************************/
static void
ixgbe_register_vlan(void *arg, struct ifnet *ifp, u16 vtag)
{
	struct adapter *adapter = ifp->if_softc;
	u16            index, bit;

	if (ifp->if_softc != arg)   /* Not our event */
		return;

	if ((vtag == 0) || (vtag > 4095))  /* Invalid */
		return;

	IXGBE_CORE_LOCK(adapter);
	index = (vtag >> 5) & 0x7F;
	bit = vtag & 0x1F;
	adapter->shadow_vfta[index] |= (1 << bit);
	++adapter->num_vlans;
	ixgbe_setup_vlan_hw_support(adapter);
	IXGBE_CORE_UNLOCK(adapter);
} /* ixgbe_register_vlan */

/************************************************************************
 * ixgbe_unregister_vlan
 *
 *   Run via vlan unconfig EVENT, remove our entry in the soft vfta.
 ************************************************************************/
static void
ixgbe_unregister_vlan(void *arg, struct ifnet *ifp, u16 vtag)
{
	struct adapter *adapter = ifp->if_softc;
	u16            index, bit;

	if (ifp->if_softc != arg)
		return;

	if ((vtag == 0) || (vtag > 4095))  /* Invalid */
		return;

	IXGBE_CORE_LOCK(adapter);
	index = (vtag >> 5) & 0x7F;
	bit = vtag & 0x1F;
	adapter->shadow_vfta[index] &= ~(1 << bit);
	--adapter->num_vlans;
	/* Re-init to load the changes */
	ixgbe_setup_vlan_hw_support(adapter);
	IXGBE_CORE_UNLOCK(adapter);
} /* ixgbe_unregister_vlan */

/************************************************************************
 * ixgbe_setup_vlan_hw_support
 ************************************************************************/
static void
ixgbe_setup_vlan_hw_support(struct adapter *adapter)
{
	struct ifnet    *ifp = adapter->ifp;
	struct ixgbe_hw *hw = &adapter->hw;
	struct rx_ring  *rxr;
	int             i;
	u32             ctrl;


	/*
	 * We get here thru init_locked, meaning
	 * a soft reset, this has already cleared
	 * the VFTA and other state, so if there
	 * have been no vlan's registered do nothing.
	 */
	if (adapter->num_vlans == 0)
		return;

	/* Setup the queues for vlans */
	for (i = 0; i < adapter->num_queues; i++) {
		rxr = &adapter->rx_rings[i];
		/* On 82599 the VLAN enable is per/queue in RXDCTL */
		if (hw->mac.type != ixgbe_mac_82598EB) {
			ctrl = IXGBE_READ_REG(hw, IXGBE_RXDCTL(rxr->me));
			ctrl |= IXGBE_RXDCTL_VME;
			IXGBE_WRITE_REG(hw, IXGBE_RXDCTL(rxr->me), ctrl);
		}
		rxr->vtag_strip = TRUE;
	}

	if ((ifp->if_capenable & IFCAP_VLAN_HWFILTER) == 0)
		return;
	/*
	 * A soft reset zero's out the VFTA, so
	 * we need to repopulate it now.
	 */
	for (i = 0; i < IXGBE_VFTA_SIZE; i++)
		if (adapter->shadow_vfta[i] != 0)
			IXGBE_WRITE_REG(hw, IXGBE_VFTA(i),
			    adapter->shadow_vfta[i]);

	ctrl = IXGBE_READ_REG(hw, IXGBE_VLNCTRL);
	/* Enable the Filter Table if enabled */
	if (ifp->if_capenable & IFCAP_VLAN_HWFILTER) {
		ctrl &= ~IXGBE_VLNCTRL_CFIEN;
		ctrl |= IXGBE_VLNCTRL_VFE;
	}
	if (hw->mac.type == ixgbe_mac_82598EB)
		ctrl |= IXGBE_VLNCTRL_VME;
	IXGBE_WRITE_REG(hw, IXGBE_VLNCTRL, ctrl);
} /* ixgbe_setup_vlan_hw_support */

/************************************************************************
 * ixgbe_get_slot_info
 *
 *   Get the width and transaction speed of
 *   the slot this adapter is plugged into.
 ************************************************************************/
static void
ixgbe_get_slot_info(struct adapter *adapter)
{
	device_t              dev = adapter->dev;
	struct ixgbe_hw       *hw = &adapter->hw;
	u32                   offset;
	u16                   link;
	int                   bus_info_valid = TRUE;

	/* Some devices are behind an internal bridge */
	switch (hw->device_id) {
	case IXGBE_DEV_ID_82599_SFP_SF_QP:
	case IXGBE_DEV_ID_82599_QSFP_SF_QP:
		goto get_parent_info;
	default:
		break;
	}

	ixgbe_get_bus_info(hw);

	/*
	 * Some devices don't use PCI-E, but there is no need
	 * to display "Unknown" for bus speed and width.
	 */
	switch (hw->mac.type) {
	case ixgbe_mac_X550EM_x:
	case ixgbe_mac_X550EM_a:
		return;
	default:
		goto display;
	}

get_parent_info:
	/*
	 * For the Quad port adapter we need to parse back
	 * up the PCI tree to find the speed of the expansion
	 * slot into which this adapter is plugged. A bit more work.
	 */
	dev = device_get_parent(device_get_parent(dev));
#ifdef IXGBE_DEBUG
	device_printf(dev, "parent pcib = %x,%x,%x\n", pci_get_bus(dev),
	    pci_get_slot(dev), pci_get_function(dev));
#endif
	dev = device_get_parent(device_get_parent(dev));
#ifdef IXGBE_DEBUG
	device_printf(dev, "slot pcib = %x,%x,%x\n", pci_get_bus(dev),
	    pci_get_slot(dev), pci_get_function(dev));
#endif
	/* Now get the PCI Express Capabilities offset */
	if (pci_find_cap(dev, PCIY_EXPRESS, &offset)) {
		/*
		 * Hmm...can't get PCI-Express capabilities.
		 * Falling back to default method.
		 */
		bus_info_valid = FALSE;
		ixgbe_get_bus_info(hw);
		goto display;
	}
	/* ...and read the Link Status Register */
	link = pci_read_config(dev, offset + PCIER_LINK_STA, 2);
	ixgbe_set_pci_config_data_generic(hw, link);

display:
	device_printf(dev, "PCI Express Bus: Speed %s %s\n",
	    ((hw->bus.speed == ixgbe_bus_speed_8000)    ? "8.0GT/s"  :
	     (hw->bus.speed == ixgbe_bus_speed_5000)    ? "5.0GT/s"  :
	     (hw->bus.speed == ixgbe_bus_speed_2500)    ? "2.5GT/s"  :
	     "Unknown"),
	    ((hw->bus.width == ixgbe_bus_width_pcie_x8) ? "Width x8" :
	     (hw->bus.width == ixgbe_bus_width_pcie_x4) ? "Width x4" :
	     (hw->bus.width == ixgbe_bus_width_pcie_x1) ? "Width x1" :
	     "Unknown"));

	if (bus_info_valid) {
		if ((hw->device_id != IXGBE_DEV_ID_82599_SFP_SF_QP) &&
		    ((hw->bus.width <= ixgbe_bus_width_pcie_x4) &&
		    (hw->bus.speed == ixgbe_bus_speed_2500))) {
			device_printf(dev, "PCI-Express bandwidth available for this card\n     is not sufficient for optimal performance.\n");
			device_printf(dev, "For optimal performance a x8 PCIE, or x4 PCIE Gen2 slot is required.\n");
		}
		if ((hw->device_id == IXGBE_DEV_ID_82599_SFP_SF_QP) &&
		    ((hw->bus.width <= ixgbe_bus_width_pcie_x8) &&
		    (hw->bus.speed < ixgbe_bus_speed_8000))) {
			device_printf(dev, "PCI-Express bandwidth available for this card\n     is not sufficient for optimal performance.\n");
			device_printf(dev, "For optimal performance a x8 PCIE Gen3 slot is required.\n");
		}
	} else
		device_printf(dev, "Unable to determine slot speed/width. The speed/width reported are that of the internal switch.\n");

	return;
} /* ixgbe_get_slot_info */

/************************************************************************
 * ixgbe_enable_queue - MSI-X Interrupt Handlers and Tasklets
 ************************************************************************/
static inline void
ixgbe_enable_queue(struct adapter *adapter, u32 vector)
{
	struct ixgbe_hw *hw = &adapter->hw;
	u64             queue = (u64)(1 << vector);
	u32             mask;

	if (hw->mac.type == ixgbe_mac_82598EB) {
		mask = (IXGBE_EIMS_RTX_QUEUE & queue);
		IXGBE_WRITE_REG(hw, IXGBE_EIMS, mask);
	} else {
		mask = (queue & 0xFFFFFFFF);
		if (mask)
			IXGBE_WRITE_REG(hw, IXGBE_EIMS_EX(0), mask);
		mask = (queue >> 32);
		if (mask)
			IXGBE_WRITE_REG(hw, IXGBE_EIMS_EX(1), mask);
	}
} /* ixgbe_enable_queue */

/************************************************************************
 * ixgbe_disable_queue
 ************************************************************************/
static inline void
ixgbe_disable_queue(struct adapter *adapter, u32 vector)
{
	struct ixgbe_hw *hw = &adapter->hw;
	u64             queue = (u64)(1 << vector);
	u32             mask;

	if (hw->mac.type == ixgbe_mac_82598EB) {
		mask = (IXGBE_EIMS_RTX_QUEUE & queue);
		IXGBE_WRITE_REG(hw, IXGBE_EIMC, mask);
	} else {
		mask = (queue & 0xFFFFFFFF);
		if (mask)
			IXGBE_WRITE_REG(hw, IXGBE_EIMC_EX(0), mask);
		mask = (queue >> 32);
		if (mask)
			IXGBE_WRITE_REG(hw, IXGBE_EIMC_EX(1), mask);
	}
} /* ixgbe_disable_queue */

/************************************************************************
 * ixgbe_msix_que - MSI-X Queue Interrupt Service routine
 ************************************************************************/
void
ixgbe_msix_que(void *arg)
{
	struct ix_queue *que = arg;
	struct adapter  *adapter = que->adapter;
	struct ifnet    *ifp = adapter->ifp;
	struct tx_ring  *txr = que->txr;
	struct rx_ring  *rxr = que->rxr;
	bool            more;
	u32             newitr = 0;


	/* Protect against spurious interrupts */
	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
		return;

	ixgbe_disable_queue(adapter, que->msix);
	++que->irqs;

	more = ixgbe_rxeof(que);

	IXGBE_TX_LOCK(txr);
	ixgbe_txeof(txr);
	if (!ixgbe_ring_empty(ifp, txr->br))
		ixgbe_start_locked(ifp, txr);
	IXGBE_TX_UNLOCK(txr);

	/* Do AIM now? */

	if (adapter->enable_aim == FALSE)
		goto no_calc;
	/*
	 * Do Adaptive Interrupt Moderation:
	 *  - Write out last calculated setting
	 *  - Calculate based on average size over
	 *    the last interval.
	 */
	if (que->eitr_setting)
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_EITR(que->msix),
		    que->eitr_setting);

	que->eitr_setting = 0;

	/* Idle, do nothing */
	if ((txr->bytes == 0) && (rxr->bytes == 0))
		goto no_calc;

	if ((txr->bytes) && (txr->packets))
		newitr = txr->bytes/txr->packets;
	if ((rxr->bytes) && (rxr->packets))
		newitr = max(newitr, (rxr->bytes / rxr->packets));
	newitr += 24; /* account for hardware frame, crc */

	/* set an upper boundary */
	newitr = min(newitr, 3000);

	/* Be nice to the mid range */
	if ((newitr > 300) && (newitr < 1200))
		newitr = (newitr / 3);
	else
		newitr = (newitr / 2);

	if (adapter->hw.mac.type == ixgbe_mac_82598EB)
		newitr |= newitr << 16;
	else
		newitr |= IXGBE_EITR_CNT_WDIS;

	/* save for next interrupt */
	que->eitr_setting = newitr;

	/* Reset state */
	txr->bytes = 0;
	txr->packets = 0;
	rxr->bytes = 0;
	rxr->packets = 0;

no_calc:
	if (more)
		taskqueue_enqueue(que->tq, &que->que_task);
	else
		ixgbe_enable_queue(adapter, que->msix);

	return;
} /* ixgbe_msix_que */

/************************************************************************
 * ixgbe_media_status - Media Ioctl callback
 *
 *   Called whenever the user queries the status of
 *   the interface using ifconfig.
 ************************************************************************/
static void
ixgbe_media_status(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct adapter  *adapter = ifp->if_softc;
	struct ixgbe_hw *hw = &adapter->hw;
	int             layer;

	INIT_DEBUGOUT("ixgbe_media_status: begin");
	IXGBE_CORE_LOCK(adapter);
	ixgbe_update_link_status(adapter);

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	if (!adapter->link_active) {
		IXGBE_CORE_UNLOCK(adapter);
		return;
	}

	ifmr->ifm_status |= IFM_ACTIVE;
	layer = adapter->phy_layer;

	if (layer & IXGBE_PHYSICAL_LAYER_10GBASE_T ||
	    layer & IXGBE_PHYSICAL_LAYER_1000BASE_T ||
	    layer & IXGBE_PHYSICAL_LAYER_100BASE_TX ||
	    layer & IXGBE_PHYSICAL_LAYER_10BASE_T)
		switch (adapter->link_speed) {
		case IXGBE_LINK_SPEED_10GB_FULL:
			ifmr->ifm_active |= IFM_10G_T | IFM_FDX;
			break;
		case IXGBE_LINK_SPEED_1GB_FULL:
			ifmr->ifm_active |= IFM_1000_T | IFM_FDX;
			break;
		case IXGBE_LINK_SPEED_100_FULL:
			ifmr->ifm_active |= IFM_100_TX | IFM_FDX;
			break;
		case IXGBE_LINK_SPEED_10_FULL:
			ifmr->ifm_active |= IFM_10_T | IFM_FDX;
			break;
		}
	if (layer & IXGBE_PHYSICAL_LAYER_SFP_PLUS_CU ||
	    layer & IXGBE_PHYSICAL_LAYER_SFP_ACTIVE_DA)
		switch (adapter->link_speed) {
		case IXGBE_LINK_SPEED_10GB_FULL:
			ifmr->ifm_active |= IFM_10G_TWINAX | IFM_FDX;
			break;
		}
	if (layer & IXGBE_PHYSICAL_LAYER_10GBASE_LR)
		switch (adapter->link_speed) {
		case IXGBE_LINK_SPEED_10GB_FULL:
			ifmr->ifm_active |= IFM_10G_LR | IFM_FDX;
			break;
		case IXGBE_LINK_SPEED_1GB_FULL:
			ifmr->ifm_active |= IFM_1000_LX | IFM_FDX;
			break;
		}
	if (layer & IXGBE_PHYSICAL_LAYER_10GBASE_LRM)
		switch (adapter->link_speed) {
		case IXGBE_LINK_SPEED_10GB_FULL:
			ifmr->ifm_active |= IFM_10G_LRM | IFM_FDX;
			break;
		case IXGBE_LINK_SPEED_1GB_FULL:
			ifmr->ifm_active |= IFM_1000_LX | IFM_FDX;
			break;
		}
	if (layer & IXGBE_PHYSICAL_LAYER_10GBASE_SR ||
	    layer & IXGBE_PHYSICAL_LAYER_1000BASE_SX)
		switch (adapter->link_speed) {
		case IXGBE_LINK_SPEED_10GB_FULL:
			ifmr->ifm_active |= IFM_10G_SR | IFM_FDX;
			break;
		case IXGBE_LINK_SPEED_1GB_FULL:
			ifmr->ifm_active |= IFM_1000_SX | IFM_FDX;
			break;
		}
	if (layer & IXGBE_PHYSICAL_LAYER_10GBASE_CX4)
		switch (adapter->link_speed) {
		case IXGBE_LINK_SPEED_10GB_FULL:
			ifmr->ifm_active |= IFM_10G_CX4 | IFM_FDX;
			break;
		}
	/*
	 * XXX: These need to use the proper media types once
	 * they're added.
	 */
#ifndef IFM_ETH_XTYPE
	if (layer & IXGBE_PHYSICAL_LAYER_10GBASE_KR)
		switch (adapter->link_speed) {
		case IXGBE_LINK_SPEED_10GB_FULL:
			ifmr->ifm_active |= IFM_10G_SR | IFM_FDX;
			break;
		case IXGBE_LINK_SPEED_2_5GB_FULL:
			ifmr->ifm_active |= IFM_2500_SX | IFM_FDX;
			break;
		case IXGBE_LINK_SPEED_1GB_FULL:
			ifmr->ifm_active |= IFM_1000_CX | IFM_FDX;
			break;
		}
	else if (layer & IXGBE_PHYSICAL_LAYER_10GBASE_KX4 ||
	    layer & IXGBE_PHYSICAL_LAYER_2500BASE_KX ||
	    layer & IXGBE_PHYSICAL_LAYER_1000BASE_KX)
		switch (adapter->link_speed) {
		case IXGBE_LINK_SPEED_10GB_FULL:
			ifmr->ifm_active |= IFM_10G_CX4 | IFM_FDX;
			break;
		case IXGBE_LINK_SPEED_2_5GB_FULL:
			ifmr->ifm_active |= IFM_2500_SX | IFM_FDX;
			break;
		case IXGBE_LINK_SPEED_1GB_FULL:
			ifmr->ifm_active |= IFM_1000_CX | IFM_FDX;
			break;
		}
#else
	if (layer & IXGBE_PHYSICAL_LAYER_10GBASE_KR)
		switch (adapter->link_speed) {
		case IXGBE_LINK_SPEED_10GB_FULL:
			ifmr->ifm_active |= IFM_10G_KR | IFM_FDX;
			break;
		case IXGBE_LINK_SPEED_2_5GB_FULL:
			ifmr->ifm_active |= IFM_2500_KX | IFM_FDX;
			break;
		case IXGBE_LINK_SPEED_1GB_FULL:
			ifmr->ifm_active |= IFM_1000_KX | IFM_FDX;
			break;
		}
	else if (layer & IXGBE_PHYSICAL_LAYER_10GBASE_KX4 ||
	    layer & IXGBE_PHYSICAL_LAYER_2500BASE_KX ||
	    layer & IXGBE_PHYSICAL_LAYER_1000BASE_KX)
		switch (adapter->link_speed) {
		case IXGBE_LINK_SPEED_10GB_FULL:
			ifmr->ifm_active |= IFM_10G_KX4 | IFM_FDX;
			break;
		case IXGBE_LINK_SPEED_2_5GB_FULL:
			ifmr->ifm_active |= IFM_2500_KX | IFM_FDX;
			break;
		case IXGBE_LINK_SPEED_1GB_FULL:
			ifmr->ifm_active |= IFM_1000_KX | IFM_FDX;
			break;
		}
#endif

	/* If nothing is recognized... */
	if (IFM_SUBTYPE(ifmr->ifm_active) == 0)
		ifmr->ifm_active |= IFM_UNKNOWN;

#if __FreeBSD_version >= 900025
	/* Display current flow control setting used on link */
	if (hw->fc.current_mode == ixgbe_fc_rx_pause ||
	    hw->fc.current_mode == ixgbe_fc_full)
		ifmr->ifm_active |= IFM_ETH_RXPAUSE;
	if (hw->fc.current_mode == ixgbe_fc_tx_pause ||
	    hw->fc.current_mode == ixgbe_fc_full)
		ifmr->ifm_active |= IFM_ETH_TXPAUSE;
#endif

	IXGBE_CORE_UNLOCK(adapter);

	return;
} /* ixgbe_media_status */

/************************************************************************
 * ixgbe_media_change - Media Ioctl callback
 *
 *   Called when the user changes speed/duplex using
 *   media/mediopt option with ifconfig.
 ************************************************************************/
static int
ixgbe_media_change(struct ifnet *ifp)
{
	struct adapter   *adapter = ifp->if_softc;
	struct ifmedia   *ifm = &adapter->media;
	struct ixgbe_hw  *hw = &adapter->hw;
	ixgbe_link_speed speed = 0;

	INIT_DEBUGOUT("ixgbe_media_change: begin");

	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
		return (EINVAL);

	if (hw->phy.media_type == ixgbe_media_type_backplane)
		return (ENODEV);

	/*
	 * We don't actually need to check against the supported
	 * media types of the adapter; ifmedia will take care of
	 * that for us.
	 */
	switch (IFM_SUBTYPE(ifm->ifm_media)) {
		case IFM_AUTO:
		case IFM_10G_T:
			speed |= IXGBE_LINK_SPEED_100_FULL;
			speed |= IXGBE_LINK_SPEED_1GB_FULL;
			speed |= IXGBE_LINK_SPEED_10GB_FULL;
			break;
		case IFM_10G_LRM:
		case IFM_10G_LR:
#ifndef IFM_ETH_XTYPE
		case IFM_10G_SR: /* KR, too */
		case IFM_10G_CX4: /* KX4 */
#else
		case IFM_10G_KR:
		case IFM_10G_KX4:
#endif
			speed |= IXGBE_LINK_SPEED_1GB_FULL;
			speed |= IXGBE_LINK_SPEED_10GB_FULL;
			break;
#ifndef IFM_ETH_XTYPE
		case IFM_1000_CX: /* KX */
#else
		case IFM_1000_KX:
#endif
		case IFM_1000_LX:
		case IFM_1000_SX:
			speed |= IXGBE_LINK_SPEED_1GB_FULL;
			break;
		case IFM_1000_T:
			speed |= IXGBE_LINK_SPEED_100_FULL;
			speed |= IXGBE_LINK_SPEED_1GB_FULL;
			break;
		case IFM_10G_TWINAX:
			speed |= IXGBE_LINK_SPEED_10GB_FULL;
			break;
		case IFM_100_TX:
			speed |= IXGBE_LINK_SPEED_100_FULL;
			break;
		case IFM_10_T:
			speed |= IXGBE_LINK_SPEED_10_FULL;
			break;
		default:
			goto invalid;
	}

	hw->mac.autotry_restart = TRUE;
	hw->mac.ops.setup_link(hw, speed, TRUE);
	adapter->advertise =
	    ((speed & IXGBE_LINK_SPEED_10GB_FULL) ? 4 : 0) |
	    ((speed & IXGBE_LINK_SPEED_1GB_FULL)  ? 2 : 0) |
	    ((speed & IXGBE_LINK_SPEED_100_FULL)  ? 1 : 0) |
	    ((speed & IXGBE_LINK_SPEED_10_FULL)   ? 8 : 0);

	return (0);

invalid:
	device_printf(adapter->dev, "Invalid media type!\n");

	return (EINVAL);
} /* ixgbe_media_change */

/************************************************************************
 * ixgbe_set_promisc
 ************************************************************************/
static void
ixgbe_set_promisc(struct adapter *adapter)
{
	struct ifnet *ifp = adapter->ifp;
	int          mcnt = 0;
	u32          rctl;

	rctl = IXGBE_READ_REG(&adapter->hw, IXGBE_FCTRL);
	rctl &= (~IXGBE_FCTRL_UPE);
	if (ifp->if_flags & IFF_ALLMULTI)
		mcnt = MAX_NUM_MULTICAST_ADDRESSES;
	else {
		struct ifmultiaddr *ifma;
#if __FreeBSD_version < 800000
		IF_ADDR_LOCK(ifp);
#else
		if_maddr_rlock(ifp);
#endif
		TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
			if (ifma->ifma_addr->sa_family != AF_LINK)
				continue;
			if (mcnt == MAX_NUM_MULTICAST_ADDRESSES)
				break;
			mcnt++;
		}
#if __FreeBSD_version < 800000
		IF_ADDR_UNLOCK(ifp);
#else
		if_maddr_runlock(ifp);
#endif
	}
	if (mcnt < MAX_NUM_MULTICAST_ADDRESSES)
		rctl &= (~IXGBE_FCTRL_MPE);
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_FCTRL, rctl);

	if (ifp->if_flags & IFF_PROMISC) {
		rctl |= (IXGBE_FCTRL_UPE | IXGBE_FCTRL_MPE);
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_FCTRL, rctl);
	} else if (ifp->if_flags & IFF_ALLMULTI) {
		rctl |= IXGBE_FCTRL_MPE;
		rctl &= ~IXGBE_FCTRL_UPE;
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_FCTRL, rctl);
	}
} /* ixgbe_set_promisc */

/************************************************************************
 * ixgbe_msix_link - Link status change ISR (MSI/MSI-X)
 ************************************************************************/
static void
ixgbe_msix_link(void *arg)
{
	struct adapter  *adapter = arg;
	struct ixgbe_hw *hw = &adapter->hw;
	u32             eicr, eicr_mask;
	s32             retval;

	++adapter->link_irq;

	/* Pause other interrupts */
	IXGBE_WRITE_REG(hw, IXGBE_EIMC, IXGBE_EIMC_OTHER);

	/* First get the cause */
	eicr = IXGBE_READ_REG(hw, IXGBE_EICS);
	/* Be sure the queue bits are not cleared */
	eicr &= ~IXGBE_EICR_RTX_QUEUE;
	/* Clear interrupt with write */
	IXGBE_WRITE_REG(hw, IXGBE_EICR, eicr);

	/* Link status change */
	if (eicr & IXGBE_EICR_LSC) {
		IXGBE_WRITE_REG(hw, IXGBE_EIMC, IXGBE_EIMC_LSC);
		adapter->task_requests |= IXGBE_REQUEST_TASK_LINK;
		taskqueue_enqueue(adapter->tq, &adapter->admin_task);
	}

	if (adapter->hw.mac.type != ixgbe_mac_82598EB) {
		if ((adapter->feat_en & IXGBE_FEATURE_FDIR) &&
		    (eicr & IXGBE_EICR_FLOW_DIR)) {
			/* This is probably overkill :) */
			if (!atomic_cmpset_int(&adapter->fdir_reinit, 0, 1))
				return;
			/* Disable the interrupt */
			IXGBE_WRITE_REG(hw, IXGBE_EIMC, IXGBE_EIMC_FLOW_DIR);
			adapter->task_requests |= IXGBE_REQUEST_TASK_FDIR;
			taskqueue_enqueue(adapter->tq, &adapter->admin_task);
		}

		if (eicr & IXGBE_EICR_ECC) {
			device_printf(adapter->dev,
			    "CRITICAL: ECC ERROR!!  Please Reboot!!\n");
			IXGBE_WRITE_REG(hw, IXGBE_EICR, IXGBE_EICR_ECC);
		}

		/* Check for over temp condition */
		if (adapter->feat_en & IXGBE_FEATURE_TEMP_SENSOR) {
			switch (adapter->hw.mac.type) {
			case ixgbe_mac_X550EM_a:
				if (!(eicr & IXGBE_EICR_GPI_SDP0_X550EM_a))
					break;
				IXGBE_WRITE_REG(hw, IXGBE_EIMC,
				    IXGBE_EICR_GPI_SDP0_X550EM_a);
				IXGBE_WRITE_REG(hw, IXGBE_EICR,
				    IXGBE_EICR_GPI_SDP0_X550EM_a);
				retval = hw->phy.ops.check_overtemp(hw);
				if (retval != IXGBE_ERR_OVERTEMP)
					break;
				device_printf(adapter->dev, "CRITICAL: OVER TEMP!! PHY IS SHUT DOWN!!\n");
				device_printf(adapter->dev, "System shutdown required!\n");
				break;
			default:
				if (!(eicr & IXGBE_EICR_TS))
					break;
				retval = hw->phy.ops.check_overtemp(hw);
				if (retval != IXGBE_ERR_OVERTEMP)
					break;
				device_printf(adapter->dev, "CRITICAL: OVER TEMP!! PHY IS SHUT DOWN!!\n");
				device_printf(adapter->dev, "System shutdown required!\n");
				IXGBE_WRITE_REG(hw, IXGBE_EICR, IXGBE_EICR_TS);
				break;
			}
		}

		/* Check for VF message */
		if ((adapter->feat_en & IXGBE_FEATURE_SRIOV) &&
		    (eicr & IXGBE_EICR_MAILBOX)) {
			adapter->task_requests |= IXGBE_REQUEST_TASK_MBX;
			taskqueue_enqueue(adapter->tq, &adapter->admin_task);
		}
	}

	if (ixgbe_is_sfp(hw)) {
		/* Pluggable optics-related interrupt */
		if (hw->mac.type >= ixgbe_mac_X540)
			eicr_mask = IXGBE_EICR_GPI_SDP0_X540;
		else
			eicr_mask = IXGBE_EICR_GPI_SDP2_BY_MAC(hw);

		if (eicr & eicr_mask) {
			IXGBE_WRITE_REG(hw, IXGBE_EICR, eicr_mask);
			adapter->task_requests |= IXGBE_REQUEST_TASK_MOD;
			taskqueue_enqueue(adapter->tq, &adapter->admin_task);
		}

		if ((hw->mac.type == ixgbe_mac_82599EB) &&
		    (eicr & IXGBE_EICR_GPI_SDP1_BY_MAC(hw))) {
			IXGBE_WRITE_REG(hw, IXGBE_EICR,
			    IXGBE_EICR_GPI_SDP1_BY_MAC(hw));
			adapter->task_requests |= IXGBE_REQUEST_TASK_MSF;
			taskqueue_enqueue(adapter->tq, &adapter->admin_task);
		}
	}

	/* Check for fan failure */
	if (adapter->feat_en & IXGBE_FEATURE_FAN_FAIL) {
		ixgbe_check_fan_failure(adapter, eicr, TRUE);
		IXGBE_WRITE_REG(hw, IXGBE_EICR, IXGBE_EICR_GPI_SDP1_BY_MAC(hw));
	}

	/* External PHY interrupt */
	if ((hw->phy.type == ixgbe_phy_x550em_ext_t) &&
	    (eicr & IXGBE_EICR_GPI_SDP0_X540)) {
		IXGBE_WRITE_REG(hw, IXGBE_EICR, IXGBE_EICR_GPI_SDP0_X540);
		adapter->task_requests |= IXGBE_REQUEST_TASK_PHY;
		taskqueue_enqueue(adapter->tq, &adapter->admin_task);
	}
} /* ixgbe_msix_link */

/************************************************************************
 * ixgbe_sysctl_interrupt_rate_handler
 ************************************************************************/
static int
ixgbe_sysctl_interrupt_rate_handler(SYSCTL_HANDLER_ARGS)
{
	struct ix_queue *que = ((struct ix_queue *)oidp->oid_arg1);
	int             error;
	unsigned int    reg, usec, rate;

	reg = IXGBE_READ_REG(&que->adapter->hw, IXGBE_EITR(que->msix));
	usec = ((reg & 0x0FF8) >> 3);
	if (usec > 0)
		rate = 500000 / usec;
	else
		rate = 0;
	error = sysctl_handle_int(oidp, &rate, 0, req);
	if (error || !req->newptr)
		return error;
	reg &= ~0xfff; /* default, no limitation */
	ixgbe_max_interrupt_rate = 0;
	if (rate > 0 && rate < 500000) {
		if (rate < 1000)
			rate = 1000;
		ixgbe_max_interrupt_rate = rate;
		reg |= ((4000000/rate) & 0xff8);
	}
	IXGBE_WRITE_REG(&que->adapter->hw, IXGBE_EITR(que->msix), reg);

	return (0);
} /* ixgbe_sysctl_interrupt_rate_handler */

/************************************************************************
 * ixgbe_add_device_sysctls
 ************************************************************************/
static void
ixgbe_add_device_sysctls(struct adapter *adapter)
{
	device_t               dev = adapter->dev;
	struct ixgbe_hw        *hw = &adapter->hw;
	struct sysctl_oid_list *child;
	struct sysctl_ctx_list *ctx;

	ctx = device_get_sysctl_ctx(dev);
	child = SYSCTL_CHILDREN(device_get_sysctl_tree(dev));

	/* Sysctls for all devices */
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "fc", CTLTYPE_INT | CTLFLAG_RW,
	    adapter, 0, ixgbe_sysctl_flowcntl, "I", IXGBE_SYSCTL_DESC_SET_FC);

	adapter->enable_aim = ixgbe_enable_aim;
	SYSCTL_ADD_INT(ctx, child, OID_AUTO, "enable_aim", CTLFLAG_RW,
	    &adapter->enable_aim, 1, "Interrupt Moderation");

	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "advertise_speed",
	    CTLTYPE_INT | CTLFLAG_RW, adapter, 0, ixgbe_sysctl_advertise, "I",
	    IXGBE_SYSCTL_DESC_ADV_SPEED);

#ifdef IXGBE_DEBUG
	/* testing sysctls (for all devices) */
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "power_state",
	    CTLTYPE_INT | CTLFLAG_RW, adapter, 0, ixgbe_sysctl_power_state,
	    "I", "PCI Power State");

	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "print_rss_config",
	    CTLTYPE_STRING | CTLFLAG_RD, adapter, 0,
	    ixgbe_sysctl_print_rss_config, "A", "Prints RSS Configuration");
#endif
	/* for X550 series devices */
	if (hw->mac.type >= ixgbe_mac_X550)
		SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "dmac",
		    CTLTYPE_INT | CTLFLAG_RW, adapter, 0, ixgbe_sysctl_dmac,
		    "I", "DMA Coalesce");

	/* for WoL-capable devices */
	if (hw->device_id == IXGBE_DEV_ID_X550EM_X_10G_T) {
		SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "wol_enable",
		    CTLTYPE_INT | CTLFLAG_RW, adapter, 0,
		    ixgbe_sysctl_wol_enable, "I", "Enable/Disable Wake on LAN");

		SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "wufc",
		    CTLTYPE_INT | CTLFLAG_RW, adapter, 0, ixgbe_sysctl_wufc,
		    "I", "Enable/Disable Wake Up Filters");
	}

	/* for X552/X557-AT devices */
	if (hw->device_id == IXGBE_DEV_ID_X550EM_X_10G_T) {
		struct sysctl_oid *phy_node;
		struct sysctl_oid_list *phy_list;

		phy_node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, "phy",
		    CTLFLAG_RD, NULL, "External PHY sysctls");
		phy_list = SYSCTL_CHILDREN(phy_node);

		SYSCTL_ADD_PROC(ctx, phy_list, OID_AUTO, "temp",
		    CTLTYPE_INT | CTLFLAG_RD, adapter, 0, ixgbe_sysctl_phy_temp,
		    "I", "Current External PHY Temperature (Celsius)");

		SYSCTL_ADD_PROC(ctx, phy_list, OID_AUTO, "overtemp_occurred",
		    CTLTYPE_INT | CTLFLAG_RD, adapter, 0,
		    ixgbe_sysctl_phy_overtemp_occurred, "I",
		    "External PHY High Temperature Event Occurred");
	}

	if (adapter->feat_cap & IXGBE_FEATURE_EEE) {
		SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "eee_state",
		    CTLTYPE_INT | CTLFLAG_RW, adapter, 0,
		    ixgbe_sysctl_eee_state, "I", "EEE Power Save State");
	}
} /* ixgbe_add_device_sysctls */

/************************************************************************
 * ixgbe_allocate_pci_resources
 ************************************************************************/
static int
ixgbe_allocate_pci_resources(struct adapter *adapter)
{
	device_t dev = adapter->dev;
	int      rid;

	rid = PCIR_BAR(0);
	adapter->pci_mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);

	if (!(adapter->pci_mem)) {
		device_printf(dev, "Unable to allocate bus resource: memory\n");
		return (ENXIO);
	}

	/* Save bus_space values for READ/WRITE_REG macros */
	adapter->osdep.mem_bus_space_tag = rman_get_bustag(adapter->pci_mem);
	adapter->osdep.mem_bus_space_handle =
	    rman_get_bushandle(adapter->pci_mem);
	/* Set hw values for shared code */
	adapter->hw.hw_addr = (u8 *)&adapter->osdep.mem_bus_space_handle;

	return (0);
} /* ixgbe_allocate_pci_resources */

/************************************************************************
 * ixgbe_detach - Device removal routine
 *
 *   Called when the driver is being removed.
 *   Stops the adapter and deallocates all the resources
 *   that were allocated for driver operation.
 *
 *   return 0 on success, positive on failure
 ************************************************************************/
static int
ixgbe_detach(device_t dev)
{
	struct adapter  *adapter = device_get_softc(dev);
	struct ix_queue *que = adapter->queues;
	struct tx_ring  *txr = adapter->tx_rings;
	u32             ctrl_ext;

	INIT_DEBUGOUT("ixgbe_detach: begin");

	/* Make sure VLANS are not using driver */
	if (adapter->ifp->if_vlantrunk != NULL) {
		device_printf(dev, "Vlan in use, detach first\n");
		return (EBUSY);
	}

	if (ixgbe_pci_iov_detach(dev) != 0) {
		device_printf(dev, "SR-IOV in use; detach first.\n");
		return (EBUSY);
	}

	ether_ifdetach(adapter->ifp);
	/* Stop the adapter */
	IXGBE_CORE_LOCK(adapter);
	ixgbe_setup_low_power_mode(adapter);
	IXGBE_CORE_UNLOCK(adapter);

	for (int i = 0; i < adapter->num_queues; i++, que++, txr++) {
		if (que->tq) {
			if (!(adapter->feat_en & IXGBE_FEATURE_LEGACY_TX))
				taskqueue_drain(que->tq, &txr->txq_task);
			taskqueue_drain(que->tq, &que->que_task);
			taskqueue_free(que->tq);
		}
	}

	/* let hardware know driver is unloading */
	ctrl_ext = IXGBE_READ_REG(&adapter->hw, IXGBE_CTRL_EXT);
	ctrl_ext &= ~IXGBE_CTRL_EXT_DRV_LOAD;
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_CTRL_EXT, ctrl_ext);

	/* Unregister VLAN events */
	if (adapter->vlan_attach != NULL)
		EVENTHANDLER_DEREGISTER(vlan_config, adapter->vlan_attach);
	if (adapter->vlan_detach != NULL)
		EVENTHANDLER_DEREGISTER(vlan_unconfig, adapter->vlan_detach);

	callout_drain(&adapter->timer);

	if (adapter->feat_en & IXGBE_FEATURE_NETMAP)
		netmap_detach(adapter->ifp);

	/* Drain the Admin Task queue */
	if (adapter->tq) {
		taskqueue_drain(adapter->tq, &adapter->admin_task);
		taskqueue_free(adapter->tq);
	}

	ixgbe_free_pci_resources(adapter);
	bus_generic_detach(dev);
	if_free(adapter->ifp);

	ixgbe_free_transmit_structures(adapter);
	ixgbe_free_receive_structures(adapter);
	free(adapter->queues, M_DEVBUF);
	free(adapter->mta, M_IXGBE);

	IXGBE_CORE_LOCK_DESTROY(adapter);

	return (0);
} /* ixgbe_detach */

/************************************************************************
 * ixgbe_setup_low_power_mode - LPLU/WoL preparation
 *
 *   Prepare the adapter/port for LPLU and/or WoL
 ************************************************************************/
static int
ixgbe_setup_low_power_mode(struct adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;
	device_t        dev = adapter->dev;
	s32             error = 0;

	mtx_assert(&adapter->core_mtx, MA_OWNED);

	/* Limit power management flow to X550EM baseT */
	if (hw->device_id == IXGBE_DEV_ID_X550EM_X_10G_T &&
	    hw->phy.ops.enter_lplu) {
		/* Turn off support for APM wakeup. (Using ACPI instead) */
		IXGBE_WRITE_REG(hw, IXGBE_GRC,
		    IXGBE_READ_REG(hw, IXGBE_GRC) & ~(u32)2);

		/*
		 * Clear Wake Up Status register to prevent any previous wakeup
		 * events from waking us up immediately after we suspend.
		 */
		IXGBE_WRITE_REG(hw, IXGBE_WUS, 0xffffffff);

		/*
		 * Program the Wakeup Filter Control register with user filter
		 * settings
		 */
		IXGBE_WRITE_REG(hw, IXGBE_WUFC, adapter->wufc);

		/* Enable wakeups and power management in Wakeup Control */
		IXGBE_WRITE_REG(hw, IXGBE_WUC,
		    IXGBE_WUC_WKEN | IXGBE_WUC_PME_EN);

		/* X550EM baseT adapters need a special LPLU flow */
		hw->phy.reset_disable = true;
		ixgbe_stop(adapter);
		error = hw->phy.ops.enter_lplu(hw);
		if (error)
			device_printf(dev, "Error entering LPLU: %d\n", error);
		hw->phy.reset_disable = false;
	} else {
		/* Just stop for other adapters */
		ixgbe_stop(adapter);
	}

	return error;
} /* ixgbe_setup_low_power_mode */

/************************************************************************
 * ixgbe_shutdown - Shutdown entry point
 ************************************************************************/
static int
ixgbe_shutdown(device_t dev)
{
	struct adapter *adapter = device_get_softc(dev);
	int            error = 0;

	INIT_DEBUGOUT("ixgbe_shutdown: begin");

	IXGBE_CORE_LOCK(adapter);
	error = ixgbe_setup_low_power_mode(adapter);
	IXGBE_CORE_UNLOCK(adapter);

	return (error);
} /* ixgbe_shutdown */

/************************************************************************
 * ixgbe_suspend
 *
 *   From D0 to D3
 ************************************************************************/
static int
ixgbe_suspend(device_t dev)
{
	struct adapter *adapter = device_get_softc(dev);
	int            error = 0;

	INIT_DEBUGOUT("ixgbe_suspend: begin");

	IXGBE_CORE_LOCK(adapter);

	error = ixgbe_setup_low_power_mode(adapter);

	IXGBE_CORE_UNLOCK(adapter);

	return (error);
} /* ixgbe_suspend */

/************************************************************************
 * ixgbe_resume
 *
 *   From D3 to D0
 ************************************************************************/
static int
ixgbe_resume(device_t dev)
{
	struct adapter  *adapter = device_get_softc(dev);
	struct ifnet    *ifp = adapter->ifp;
	struct ixgbe_hw *hw = &adapter->hw;
	u32             wus;

	INIT_DEBUGOUT("ixgbe_resume: begin");

	IXGBE_CORE_LOCK(adapter);

	/* Read & clear WUS register */
	wus = IXGBE_READ_REG(hw, IXGBE_WUS);
	if (wus)
		device_printf(dev, "Woken up by (WUS): %#010x\n",
		    IXGBE_READ_REG(hw, IXGBE_WUS));
	IXGBE_WRITE_REG(hw, IXGBE_WUS, 0xffffffff);
	/* And clear WUFC until next low-power transition */
	IXGBE_WRITE_REG(hw, IXGBE_WUFC, 0);

	/*
	 * Required after D3->D0 transition;
	 * will re-advertise all previous advertised speeds
	 */
	if (ifp->if_flags & IFF_UP)
		ixgbe_init_locked(adapter);

	IXGBE_CORE_UNLOCK(adapter);

	return (0);
} /* ixgbe_resume */

/************************************************************************
 * ixgbe_set_if_hwassist - Set the various hardware offload abilities.
 *
 *   Takes the ifnet's if_capenable flags (e.g. set by the user using
 *   ifconfig) and indicates to the OS via the ifnet's if_hwassist
 *   field what mbuf offload flags the driver will understand.
 ************************************************************************/
static void
ixgbe_set_if_hwassist(struct adapter *adapter)
{
	struct ifnet *ifp = adapter->ifp;

	ifp->if_hwassist = 0;
#if __FreeBSD_version >= 1000000
	if (ifp->if_capenable & IFCAP_TSO4)
		ifp->if_hwassist |= CSUM_IP_TSO;
	if (ifp->if_capenable & IFCAP_TSO6)
		ifp->if_hwassist |= CSUM_IP6_TSO;
	if (ifp->if_capenable & IFCAP_TXCSUM) {
		ifp->if_hwassist |= (CSUM_IP | CSUM_IP_UDP | CSUM_IP_TCP);
		if (adapter->hw.mac.type != ixgbe_mac_82598EB)
			ifp->if_hwassist |= CSUM_IP_SCTP;
	}
	if (ifp->if_capenable & IFCAP_TXCSUM_IPV6) {
		ifp->if_hwassist |= (CSUM_IP6_UDP | CSUM_IP6_TCP);
		if (adapter->hw.mac.type != ixgbe_mac_82598EB)
			ifp->if_hwassist |= CSUM_IP6_SCTP;
	}
#else
	if (ifp->if_capenable & IFCAP_TSO)
		ifp->if_hwassist |= CSUM_TSO;
	if (ifp->if_capenable & IFCAP_TXCSUM) {
		ifp->if_hwassist |= (CSUM_TCP | CSUM_UDP);
		if (adapter->hw.mac.type != ixgbe_mac_82598EB)
			ifp->if_hwassist |= CSUM_SCTP;
	}
#endif
} /* ixgbe_set_if_hwassist */

/************************************************************************
 * ixgbe_init_locked - Init entry point
 *
 *   Used in two ways: It is used by the stack as an init
 *   entry point in network interface structure. It is also
 *   used by the driver as a hw/sw initialization routine to
 *   get to a consistent state.
 *
 *   return 0 on success, positive on failure
 ************************************************************************/
void
ixgbe_init_locked(struct adapter *adapter)
{
	struct ifnet    *ifp = adapter->ifp;
	device_t        dev = adapter->dev;
	struct ixgbe_hw *hw = &adapter->hw;
	struct tx_ring  *txr;
	struct rx_ring  *rxr;
	u32             txdctl, mhadd;
	u32             rxdctl, rxctrl;
	u32             ctrl_ext;
	int             err = 0;

	mtx_assert(&adapter->core_mtx, MA_OWNED);
	INIT_DEBUGOUT("ixgbe_init_locked: begin");

	hw->adapter_stopped = FALSE;
	ixgbe_stop_adapter(hw);
	callout_stop(&adapter->timer);

	/* Queue indices may change with IOV mode */
	ixgbe_align_all_queue_indices(adapter);

	/* reprogram the RAR[0] in case user changed it. */
	ixgbe_set_rar(hw, 0, hw->mac.addr, adapter->pool, IXGBE_RAH_AV);

	/* Get the latest mac address, User can use a LAA */
	bcopy(IF_LLADDR(ifp), hw->mac.addr, IXGBE_ETH_LENGTH_OF_ADDRESS);
	ixgbe_set_rar(hw, 0, hw->mac.addr, adapter->pool, 1);
	hw->addr_ctrl.rar_used_count = 1;

	/* Set hardware offload abilities from ifnet flags */
	ixgbe_set_if_hwassist(adapter);

	/* Prepare transmit descriptors and buffers */
	if (ixgbe_setup_transmit_structures(adapter)) {
		device_printf(dev, "Could not setup transmit structures\n");
		ixgbe_stop(adapter);
		return;
	}

	ixgbe_init_hw(hw);
	ixgbe_initialize_iov(adapter);
	ixgbe_initialize_transmit_units(adapter);

	/* Setup Multicast table */
	ixgbe_set_multi(adapter);

	/* Determine the correct mbuf pool, based on frame size */
	if (adapter->max_frame_size <= MCLBYTES)
		adapter->rx_mbuf_sz = MCLBYTES;
	else
		adapter->rx_mbuf_sz = MJUMPAGESIZE;

	/* Prepare receive descriptors and buffers */
	if (ixgbe_setup_receive_structures(adapter)) {
		device_printf(dev, "Could not setup receive structures\n");
		ixgbe_stop(adapter);
		return;
	}

	/* Configure RX settings */
	ixgbe_initialize_receive_units(adapter);

	/* Initialize variable holding task enqueue requests
	 * generated by interrupt handlers */
	adapter->task_requests = 0;

	/* Enable SDP & MSI-X interrupts based on adapter */
	ixgbe_config_gpie(adapter);

	/* Set MTU size */
	if (ifp->if_mtu > ETHERMTU) {
		/* aka IXGBE_MAXFRS on 82599 and newer */
		mhadd = IXGBE_READ_REG(hw, IXGBE_MHADD);
		mhadd &= ~IXGBE_MHADD_MFS_MASK;
		mhadd |= adapter->max_frame_size << IXGBE_MHADD_MFS_SHIFT;
		IXGBE_WRITE_REG(hw, IXGBE_MHADD, mhadd);
	}

	/* Now enable all the queues */
	for (int i = 0; i < adapter->num_queues; i++) {
		txr = &adapter->tx_rings[i];
		txdctl = IXGBE_READ_REG(hw, IXGBE_TXDCTL(txr->me));
		txdctl |= IXGBE_TXDCTL_ENABLE;
		/* Set WTHRESH to 8, burst writeback */
		txdctl |= (8 << 16);
		/*
		 * When the internal queue falls below PTHRESH (32),
		 * start prefetching as long as there are at least
		 * HTHRESH (1) buffers ready. The values are taken
		 * from the Intel linux driver 3.8.21.
		 * Prefetching enables tx line rate even with 1 queue.
		 */
		txdctl |= (32 << 0) | (1 << 8);
		IXGBE_WRITE_REG(hw, IXGBE_TXDCTL(txr->me), txdctl);
	}

	for (int i = 0, j = 0; i < adapter->num_queues; i++) {
		rxr = &adapter->rx_rings[i];
		rxdctl = IXGBE_READ_REG(hw, IXGBE_RXDCTL(rxr->me));
		if (hw->mac.type == ixgbe_mac_82598EB) {
			/*
			 * PTHRESH = 21
			 * HTHRESH = 4
			 * WTHRESH = 8
			 */
			rxdctl &= ~0x3FFFFF;
			rxdctl |= 0x080420;
		}
		rxdctl |= IXGBE_RXDCTL_ENABLE;
		IXGBE_WRITE_REG(hw, IXGBE_RXDCTL(rxr->me), rxdctl);
		for (; j < 10; j++) {
			if (IXGBE_READ_REG(hw, IXGBE_RXDCTL(rxr->me)) &
			    IXGBE_RXDCTL_ENABLE)
				break;
			else
				msec_delay(1);
		}
		wmb();

		/*
		 * In netmap mode, we must preserve the buffers made
		 * available to userspace before the if_init()
		 * (this is true by default on the TX side, because
		 * init makes all buffers available to userspace).
		 *
		 * netmap_reset() and the device specific routines
		 * (e.g. ixgbe_setup_receive_rings()) map these
		 * buffers at the end of the NIC ring, so here we
		 * must set the RDT (tail) register to make sure
		 * they are not overwritten.
		 *
		 * In this driver the NIC ring starts at RDH = 0,
		 * RDT points to the last slot available for reception (?),
		 * so RDT = num_rx_desc - 1 means the whole ring is available.
		 */
#ifdef DEV_NETMAP
		if ((adapter->feat_en & IXGBE_FEATURE_NETMAP) &&
		    (ifp->if_capenable & IFCAP_NETMAP)) {
			struct netmap_adapter *na = NA(adapter->ifp);
			struct netmap_kring *kring = na->rx_rings[i];
			int t = na->num_rx_desc - 1 - nm_kr_rxspace(kring);

			IXGBE_WRITE_REG(hw, IXGBE_RDT(rxr->me), t);
		} else
#endif /* DEV_NETMAP */
			IXGBE_WRITE_REG(hw, IXGBE_RDT(rxr->me),
			    adapter->num_rx_desc - 1);
	}

	/* Enable Receive engine */
	rxctrl = IXGBE_READ_REG(hw, IXGBE_RXCTRL);
	if (hw->mac.type == ixgbe_mac_82598EB)
		rxctrl |= IXGBE_RXCTRL_DMBYPS;
	rxctrl |= IXGBE_RXCTRL_RXEN;
	ixgbe_enable_rx_dma(hw, rxctrl);

	callout_reset(&adapter->timer, hz, ixgbe_local_timer, adapter);

	/* Set up MSI-X routing */
	if (adapter->feat_en & IXGBE_FEATURE_MSIX) {
		ixgbe_configure_ivars(adapter);
		/* Set up auto-mask */
		if (hw->mac.type == ixgbe_mac_82598EB)
			IXGBE_WRITE_REG(hw, IXGBE_EIAM, IXGBE_EICS_RTX_QUEUE);
		else {
			IXGBE_WRITE_REG(hw, IXGBE_EIAM_EX(0), 0xFFFFFFFF);
			IXGBE_WRITE_REG(hw, IXGBE_EIAM_EX(1), 0xFFFFFFFF);
		}
	} else {  /* Simple settings for Legacy/MSI */
		ixgbe_set_ivar(adapter, 0, 0, 0);
		ixgbe_set_ivar(adapter, 0, 0, 1);
		IXGBE_WRITE_REG(hw, IXGBE_EIAM, IXGBE_EICS_RTX_QUEUE);
	}

	ixgbe_init_fdir(adapter);

	/*
	 * Check on any SFP devices that
	 * need to be kick-started
	 */
	if (hw->phy.type == ixgbe_phy_none) {
		err = hw->phy.ops.identify(hw);
		if (err == IXGBE_ERR_SFP_NOT_SUPPORTED) {
			device_printf(dev,
			    "Unsupported SFP+ module type was detected.\n");
			return;
		}
	}

	/* Set moderation on the Link interrupt */
	IXGBE_WRITE_REG(hw, IXGBE_EITR(adapter->vector), IXGBE_LINK_ITR);

	/* Config/Enable Link */
	ixgbe_config_link(adapter);

	/* Hardware Packet Buffer & Flow Control setup */
	ixgbe_config_delay_values(adapter);

	/* Initialize the FC settings */
	ixgbe_start_hw(hw);

	/* Set up VLAN support and filter */
	ixgbe_setup_vlan_hw_support(adapter);

	/* Setup DMA Coalescing */
	ixgbe_config_dmac(adapter);

	/* And now turn on interrupts */
	ixgbe_enable_intr(adapter, false);

	/* Enable the use of the MBX by the VF's */
	if (adapter->feat_en & IXGBE_FEATURE_SRIOV) {
		ctrl_ext = IXGBE_READ_REG(hw, IXGBE_CTRL_EXT);
		ctrl_ext |= IXGBE_CTRL_EXT_PFRSTD;
		IXGBE_WRITE_REG(hw, IXGBE_CTRL_EXT, ctrl_ext);
	}

	/* Now inform the stack we're ready */
	ifp->if_drv_flags |= IFF_DRV_RUNNING;

	return;
} /* ixgbe_init_locked */

/************************************************************************
 * ixgbe_init
 ************************************************************************/
static void
ixgbe_init(void *arg)
{
	struct adapter *adapter = arg;

	IXGBE_CORE_LOCK(adapter);
	ixgbe_init_locked(adapter);
	IXGBE_CORE_UNLOCK(adapter);

	return;
} /* ixgbe_init */

/************************************************************************
 * ixgbe_set_ivar
 *
 *   Setup the correct IVAR register for a particular MSI-X interrupt
 *     (yes this is all very magic and confusing :)
 *    - entry is the register array entry
 *    - vector is the MSI-X vector for this queue
 *    - type is RX/TX/MISC
 ************************************************************************/
static void
ixgbe_set_ivar(struct adapter *adapter, u8 entry, u8 vector, s8 type)
{
	struct ixgbe_hw *hw = &adapter->hw;
	u32 ivar, index;

	vector |= IXGBE_IVAR_ALLOC_VAL;

	switch (hw->mac.type) {

	case ixgbe_mac_82598EB:
		if (type == -1)
			entry = IXGBE_IVAR_OTHER_CAUSES_INDEX;
		else
			entry += (type * 64);
		index = (entry >> 2) & 0x1F;
		ivar = IXGBE_READ_REG(hw, IXGBE_IVAR(index));
		ivar &= ~(0xFF << (8 * (entry & 0x3)));
		ivar |= (vector << (8 * (entry & 0x3)));
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_IVAR(index), ivar);
		break;

	case ixgbe_mac_82599EB:
	case ixgbe_mac_X540:
	case ixgbe_mac_X550:
	case ixgbe_mac_X550EM_x:
	case ixgbe_mac_X550EM_a:
		if (type == -1) { /* MISC IVAR */
			index = (entry & 1) * 8;
			ivar = IXGBE_READ_REG(hw, IXGBE_IVAR_MISC);
			ivar &= ~(0xFF << index);
			ivar |= (vector << index);
			IXGBE_WRITE_REG(hw, IXGBE_IVAR_MISC, ivar);
		} else {          /* RX/TX IVARS */
			index = (16 * (entry & 1)) + (8 * type);
			ivar = IXGBE_READ_REG(hw, IXGBE_IVAR(entry >> 1));
			ivar &= ~(0xFF << index);
			ivar |= (vector << index);
			IXGBE_WRITE_REG(hw, IXGBE_IVAR(entry >> 1), ivar);
		}

	default:
		break;
	}
} /* ixgbe_set_ivar */

/************************************************************************
 * ixgbe_configure_ivars
 ************************************************************************/
static void
ixgbe_configure_ivars(struct adapter *adapter)
{
	struct ix_queue *que = adapter->queues;
	u32             newitr;

	if (ixgbe_max_interrupt_rate > 0)
		newitr = (4000000 / ixgbe_max_interrupt_rate) & 0x0FF8;
	else {
		/*
		 * Disable DMA coalescing if interrupt moderation is
		 * disabled.
		 */
		adapter->dmac = 0;
		newitr = 0;
	}

	for (int i = 0; i < adapter->num_queues; i++, que++) {
		struct rx_ring *rxr = &adapter->rx_rings[i];
		struct tx_ring *txr = &adapter->tx_rings[i];
		/* First the RX queue entry */
		ixgbe_set_ivar(adapter, rxr->me, que->msix, 0);
		/* ... and the TX */
		ixgbe_set_ivar(adapter, txr->me, que->msix, 1);
		/* Set an Initial EITR value */
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_EITR(que->msix), newitr);
	}

	/* For the Link interrupt */
	ixgbe_set_ivar(adapter, 1, adapter->vector, -1);
} /* ixgbe_configure_ivars */

/************************************************************************
 * ixgbe_config_gpie
 ************************************************************************/
static void
ixgbe_config_gpie(struct adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;
	u32             gpie;

	gpie = IXGBE_READ_REG(hw, IXGBE_GPIE);

	if (adapter->feat_en & IXGBE_FEATURE_MSIX) {
		/* Enable Enhanced MSI-X mode */
		gpie |= IXGBE_GPIE_MSIX_MODE
		     |  IXGBE_GPIE_EIAME
		     |  IXGBE_GPIE_PBA_SUPPORT
		     |  IXGBE_GPIE_OCD;
	}

	/* Fan Failure Interrupt */
	if (adapter->feat_en & IXGBE_FEATURE_FAN_FAIL)
		gpie |= IXGBE_SDP1_GPIEN;

	/* Thermal Sensor Interrupt */
	if (adapter->feat_en & IXGBE_FEATURE_TEMP_SENSOR)
		gpie |= IXGBE_SDP0_GPIEN_X540;

	/* Link detection */
	switch (hw->mac.type) {
	case ixgbe_mac_82599EB:
		gpie |= IXGBE_SDP1_GPIEN | IXGBE_SDP2_GPIEN;
		break;
	case ixgbe_mac_X550EM_x:
	case ixgbe_mac_X550EM_a:
		gpie |= IXGBE_SDP0_GPIEN_X540;
		break;
	default:
		break;
	}

	IXGBE_WRITE_REG(hw, IXGBE_GPIE, gpie);

	return;
} /* ixgbe_config_gpie */

/************************************************************************
 * ixgbe_config_delay_values
 *
 *   Requires adapter->max_frame_size to be set.
 ************************************************************************/
static void
ixgbe_config_delay_values(struct adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;
	u32             rxpb, frame, size, tmp;

	frame = adapter->max_frame_size;

	/* Calculate High Water */
	switch (hw->mac.type) {
	case ixgbe_mac_X540:
	case ixgbe_mac_X550:
	case ixgbe_mac_X550EM_x:
	case ixgbe_mac_X550EM_a:
		tmp = IXGBE_DV_X540(frame, frame);
		break;
	default:
		tmp = IXGBE_DV(frame, frame);
		break;
	}
	size = IXGBE_BT2KB(tmp);
	rxpb = IXGBE_READ_REG(hw, IXGBE_RXPBSIZE(0)) >> 10;
	hw->fc.high_water[0] = rxpb - size;

	/* Now calculate Low Water */
	switch (hw->mac.type) {
	case ixgbe_mac_X540:
	case ixgbe_mac_X550:
	case ixgbe_mac_X550EM_x:
	case ixgbe_mac_X550EM_a:
		tmp = IXGBE_LOW_DV_X540(frame);
		break;
	default:
		tmp = IXGBE_LOW_DV(frame);
		break;
	}
	hw->fc.low_water[0] = IXGBE_BT2KB(tmp);

	hw->fc.pause_time = IXGBE_FC_PAUSE;
	hw->fc.send_xon = TRUE;
} /* ixgbe_config_delay_values */

/************************************************************************
 * ixgbe_set_multi - Multicast Update
 *
 *   Called whenever multicast address list is updated.
 ************************************************************************/
static void
ixgbe_set_multi(struct adapter *adapter)
{
	struct ifmultiaddr   *ifma;
	struct ixgbe_mc_addr *mta;
	struct ifnet         *ifp = adapter->ifp;
	u8                   *update_ptr;
	int                  mcnt = 0;
	u32                  fctrl;

	IOCTL_DEBUGOUT("ixgbe_set_multi: begin");

	mta = adapter->mta;
	bzero(mta, sizeof(*mta) * MAX_NUM_MULTICAST_ADDRESSES);

#if __FreeBSD_version < 800000
	IF_ADDR_LOCK(ifp);
#else
	if_maddr_rlock(ifp);
#endif
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		if (mcnt == MAX_NUM_MULTICAST_ADDRESSES)
			break;
		bcopy(LLADDR((struct sockaddr_dl *) ifma->ifma_addr),
		    mta[mcnt].addr, IXGBE_ETH_LENGTH_OF_ADDRESS);
		mta[mcnt].vmdq = adapter->pool;
		mcnt++;
	}
#if __FreeBSD_version < 800000
	IF_ADDR_UNLOCK(ifp);
#else
	if_maddr_runlock(ifp);
#endif

	fctrl = IXGBE_READ_REG(&adapter->hw, IXGBE_FCTRL);
	fctrl |= (IXGBE_FCTRL_UPE | IXGBE_FCTRL_MPE);
	if (ifp->if_flags & IFF_PROMISC)
		fctrl |= (IXGBE_FCTRL_UPE | IXGBE_FCTRL_MPE);
	else if (mcnt >= MAX_NUM_MULTICAST_ADDRESSES ||
	    ifp->if_flags & IFF_ALLMULTI) {
		fctrl |= IXGBE_FCTRL_MPE;
		fctrl &= ~IXGBE_FCTRL_UPE;
	} else
		fctrl &= ~(IXGBE_FCTRL_UPE | IXGBE_FCTRL_MPE);

	IXGBE_WRITE_REG(&adapter->hw, IXGBE_FCTRL, fctrl);

	if (mcnt < MAX_NUM_MULTICAST_ADDRESSES) {
		update_ptr = (u8 *)mta;
		ixgbe_update_mc_addr_list(&adapter->hw, update_ptr, mcnt,
		    ixgbe_mc_array_itr, TRUE);
	}

	return;
} /* ixgbe_set_multi */

/************************************************************************
 * ixgbe_mc_array_itr
 *
 *   An iterator function needed by the multicast shared code.
 *   It feeds the shared code routine the addresses in the
 *   array of ixgbe_set_multi() one by one.
 ************************************************************************/
static u8 *
ixgbe_mc_array_itr(struct ixgbe_hw *hw, u8 **update_ptr, u32 *vmdq)
{
	struct ixgbe_mc_addr *mta;

	mta = (struct ixgbe_mc_addr *)*update_ptr;
	*vmdq = mta->vmdq;

	*update_ptr = (u8*)(mta + 1);

	return (mta->addr);
} /* ixgbe_mc_array_itr */

/************************************************************************
 * ixgbe_local_timer - Timer routine
 *
 *   Checks for link status, updates statistics,
 *   and runs the watchdog check.
 ************************************************************************/
static void
ixgbe_local_timer(void *arg)
{
	struct adapter  *adapter = arg;
	device_t        dev = adapter->dev;
	struct ix_queue *que = adapter->queues;
	u64             queues = 0;
	int             hung = 0;

	mtx_assert(&adapter->core_mtx, MA_OWNED);

	/* Check for pluggable optics */
	if (adapter->sfp_probe)
		if (!ixgbe_sfp_probe(adapter))
			goto out; /* Nothing to do */

	ixgbe_update_link_status(adapter);
	ixgbe_update_stats_counters(adapter);

	/*
	 * Check the TX queues status
	 *      - mark hung queues so we don't schedule on them
	 *      - watchdog only if all queues show hung
	 */
	for (int i = 0; i < adapter->num_queues; i++, que++) {
		/* Keep track of queues with work for soft irq */
		if (que->txr->busy)
			queues |= ((u64)1 << que->me);
		/*
		 * Each time txeof runs without cleaning, but there
		 * are uncleaned descriptors it increments busy. If
		 * we get to the MAX we declare it hung.
		 */
		if (que->busy == IXGBE_QUEUE_HUNG) {
			++hung;
			/* Mark the queue as inactive */
			adapter->active_queues &= ~((u64)1 << que->me);
			continue;
		} else {
			/* Check if we've come back from hung */
			if ((adapter->active_queues & ((u64)1 << que->me)) == 0)
				adapter->active_queues |= ((u64)1 << que->me);
		}
		if (que->busy >= IXGBE_MAX_TX_BUSY) {
			device_printf(dev,
			    "Warning queue %d appears to be hung!\n", i);
			que->txr->busy = IXGBE_QUEUE_HUNG;
			++hung;
		}
	}

	/* Only truly watchdog if all queues show hung */
	if (hung == adapter->num_queues)
		goto watchdog;
	else if (queues != 0) { /* Force an IRQ on queues with work */
		ixgbe_rearm_queues(adapter, queues);
	}

out:
	callout_reset(&adapter->timer, hz, ixgbe_local_timer, adapter);
	return;

watchdog:
	device_printf(adapter->dev, "Watchdog timeout -- resetting\n");
	adapter->ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
	adapter->watchdog_events++;
	ixgbe_init_locked(adapter);
} /* ixgbe_local_timer */

/************************************************************************
 * ixgbe_sfp_probe
 *
 *   Determine if a port had optics inserted.
 ************************************************************************/
static bool
ixgbe_sfp_probe(struct adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;
	device_t        dev = adapter->dev;
	bool            result = FALSE;

	if ((hw->phy.type == ixgbe_phy_nl) &&
	    (hw->phy.sfp_type == ixgbe_sfp_type_not_present)) {
		s32 ret = hw->phy.ops.identify_sfp(hw);
		if (ret)
			goto out;
		ret = hw->phy.ops.reset(hw);
		adapter->sfp_probe = FALSE;
		if (ret == IXGBE_ERR_SFP_NOT_SUPPORTED) {
			device_printf(dev, "Unsupported SFP+ module detected!");
			device_printf(dev,
			    "Reload driver with supported module.\n");
			goto out;
		} else
			device_printf(dev, "SFP+ module detected!\n");
		/* We now have supported optics */
		result = TRUE;
	}
out:

	return (result);
} /* ixgbe_sfp_probe */

/************************************************************************
 * ixgbe_handle_mod - Tasklet for SFP module interrupts
 ************************************************************************/
static void
ixgbe_handle_mod(void *context)
{
	struct adapter  *adapter = context;
	struct ixgbe_hw *hw = &adapter->hw;
	device_t        dev = adapter->dev;
	u32             err, cage_full = 0;

	if (adapter->hw.need_crosstalk_fix) {
		switch (hw->mac.type) {
		case ixgbe_mac_82599EB:
			cage_full = IXGBE_READ_REG(hw, IXGBE_ESDP) &
			    IXGBE_ESDP_SDP2;
			break;
		case ixgbe_mac_X550EM_x:
		case ixgbe_mac_X550EM_a:
			cage_full = IXGBE_READ_REG(hw, IXGBE_ESDP) &
			    IXGBE_ESDP_SDP0;
			break;
		default:
			break;
		}

		if (!cage_full)
			return;
	}

	err = hw->phy.ops.identify_sfp(hw);
	if (err == IXGBE_ERR_SFP_NOT_SUPPORTED) {
		device_printf(dev,
		    "Unsupported SFP+ module type was detected.\n");
		goto handle_mod_out;
	}

	err = hw->mac.ops.setup_sfp(hw);
	if (err == IXGBE_ERR_SFP_NOT_SUPPORTED) {
		device_printf(dev,
		    "Setup failure - unsupported SFP+ module type.\n");
		goto handle_mod_out;
	}
	adapter->task_requests |= IXGBE_REQUEST_TASK_MSF;
	taskqueue_enqueue(adapter->tq, &adapter->admin_task);
	return;

handle_mod_out:
	adapter->task_requests &= ~(IXGBE_REQUEST_TASK_MSF);
} /* ixgbe_handle_mod */


/************************************************************************
 * ixgbe_handle_msf - Tasklet for MSF (multispeed fiber) interrupts
 ************************************************************************/
static void
ixgbe_handle_msf(void *context)
{
	struct adapter  *adapter = context;
	struct ixgbe_hw *hw = &adapter->hw;
	u32             autoneg;
	bool            negotiate;

	/* get_supported_phy_layer will call hw->phy.ops.identify_sfp() */
	adapter->phy_layer = ixgbe_get_supported_physical_layer(hw);

	autoneg = hw->phy.autoneg_advertised;
	if ((!autoneg) && (hw->mac.ops.get_link_capabilities))
		hw->mac.ops.get_link_capabilities(hw, &autoneg, &negotiate);
	if (hw->mac.ops.setup_link)
		hw->mac.ops.setup_link(hw, autoneg, TRUE);

	/* Adjust media types shown in ifconfig */
	ifmedia_removeall(&adapter->media);
	ixgbe_add_media_types(adapter);
	ifmedia_set(&adapter->media, IFM_ETHER | IFM_AUTO);
} /* ixgbe_handle_msf */

/************************************************************************
 * ixgbe_handle_phy - Tasklet for external PHY interrupts
 ************************************************************************/
static void
ixgbe_handle_phy(void *context)
{
	struct adapter  *adapter = context;
	struct ixgbe_hw *hw = &adapter->hw;
	int             error;

	error = hw->phy.ops.handle_lasi(hw);
	if (error == IXGBE_ERR_OVERTEMP)
		device_printf(adapter->dev, "CRITICAL: EXTERNAL PHY OVER TEMP!!  PHY will downshift to lower power state!\n");
	else if (error)
		device_printf(adapter->dev,
		    "Error handling LASI interrupt: %d\n", error);
} /* ixgbe_handle_phy */

/************************************************************************
 * ixgbe_handle_admin_task - Handler for interrupt tasklets meant to be
 *     called in separate task.
 ************************************************************************/
static void
ixgbe_handle_admin_task(void *context, int pending)
{
	struct adapter  *adapter = context;

	IXGBE_CORE_LOCK(adapter);
	ixgbe_disable_intr(adapter, true);

	if (adapter->task_requests & IXGBE_REQUEST_TASK_MOD)
		ixgbe_handle_mod(adapter);
	if (adapter->task_requests & IXGBE_REQUEST_TASK_MSF)
		ixgbe_handle_msf(adapter);
	if (adapter->task_requests & IXGBE_REQUEST_TASK_MBX)
		ixgbe_handle_mbx(adapter);
	if (adapter->task_requests & IXGBE_REQUEST_TASK_FDIR)
		ixgbe_reinit_fdir(adapter);
	if (adapter->task_requests & IXGBE_REQUEST_TASK_PHY)
		ixgbe_handle_phy(adapter);
	if (adapter->task_requests & IXGBE_REQUEST_TASK_LINK)
		ixgbe_handle_link(adapter);
	adapter->task_requests = 0;

	ixgbe_enable_intr(adapter, true);
	IXGBE_CORE_UNLOCK(adapter);
} /* ixgbe_handle_admin_task */

/************************************************************************
 * ixgbe_stop - Stop the hardware
 *
 *   Disables all traffic on the adapter by issuing a
 *   global reset on the MAC and deallocates TX/RX buffers.
 ************************************************************************/
static void
ixgbe_stop(void *arg)
{
	struct ifnet    *ifp;
	struct adapter  *adapter = arg;
	struct ixgbe_hw *hw = &adapter->hw;

	ifp = adapter->ifp;

	mtx_assert(&adapter->core_mtx, MA_OWNED);

	INIT_DEBUGOUT("ixgbe_stop: begin\n");
	ixgbe_disable_intr(adapter, false);
	callout_stop(&adapter->timer);

	/* Let the stack know...*/
	ifp->if_drv_flags &= ~IFF_DRV_RUNNING;

	ixgbe_reset_hw(hw);
	hw->adapter_stopped = FALSE;
	ixgbe_stop_adapter(hw);
	if (hw->mac.type == ixgbe_mac_82599EB)
		ixgbe_stop_mac_link_on_d3_82599(hw);
	/* Turn off the laser - noop with no optics */
	ixgbe_disable_tx_laser(hw);

	/* Update the stack */
	adapter->link_up = FALSE;
	ixgbe_update_link_status(adapter);

	/* reprogram the RAR[0] in case user changed it. */
	ixgbe_set_rar(&adapter->hw, 0, adapter->hw.mac.addr, 0, IXGBE_RAH_AV);

	return;
} /* ixgbe_stop */

/************************************************************************
 * ixgbe_update_link_status - Update OS on link state
 *
 * Note: Only updates the OS on the cached link state.
 *       The real check of the hardware only happens with
 *       a link interrupt.
 ************************************************************************/
static void
ixgbe_update_link_status(struct adapter *adapter)
{
	struct ifnet *ifp = adapter->ifp;
	device_t     dev = adapter->dev;

	if (adapter->link_up) {
		if (adapter->link_active == FALSE) {
			if (bootverbose)
				device_printf(dev, "Link is up %d Gbps %s \n",
				    ((adapter->link_speed == 128) ? 10 : 1),
				    "Full Duplex");
			adapter->link_active = TRUE;
			/* Update any Flow Control changes */
			ixgbe_fc_enable(&adapter->hw);
			/* Update DMA coalescing config */
			ixgbe_config_dmac(adapter);
			if_link_state_change(ifp, LINK_STATE_UP);
			if (adapter->feat_en & IXGBE_FEATURE_SRIOV)
				ixgbe_ping_all_vfs(adapter);
		}
	} else { /* Link down */
		if (adapter->link_active == TRUE) {
			if (bootverbose)
				device_printf(dev, "Link is Down\n");
			if_link_state_change(ifp, LINK_STATE_DOWN);
			adapter->link_active = FALSE;
			if (adapter->feat_en & IXGBE_FEATURE_SRIOV)
				ixgbe_ping_all_vfs(adapter);
		}
	}

	return;
} /* ixgbe_update_link_status */

/************************************************************************
 * ixgbe_config_dmac - Configure DMA Coalescing
 ************************************************************************/
static void
ixgbe_config_dmac(struct adapter *adapter)
{
	struct ixgbe_hw          *hw = &adapter->hw;
	struct ixgbe_dmac_config *dcfg = &hw->mac.dmac_config;

	if (hw->mac.type < ixgbe_mac_X550 || !hw->mac.ops.dmac_config)
		return;

	if (dcfg->watchdog_timer ^ adapter->dmac ||
	    dcfg->link_speed ^ adapter->link_speed) {
		dcfg->watchdog_timer = adapter->dmac;
		dcfg->fcoe_en = false;
		dcfg->link_speed = adapter->link_speed;
		dcfg->num_tcs = 1;

		INIT_DEBUGOUT2("dmac settings: watchdog %d, link speed %d\n",
		    dcfg->watchdog_timer, dcfg->link_speed);

		hw->mac.ops.dmac_config(hw);
	}
} /* ixgbe_config_dmac */

/************************************************************************
 * ixgbe_enable_intr
 *     If skip_traffic parameter is set, queues' irqs are not enabled.
 *     This is useful while reenabling interrupts after disabling them
 *     with ixgbe_disable_intr() 'keep_traffic' parameter set to true
 *     as queues' interrupts are already enabled.
 ************************************************************************/
static void
ixgbe_enable_intr(struct adapter *adapter, bool skip_traffic)
{
	struct ixgbe_hw *hw = &adapter->hw;
	struct ix_queue *que = adapter->queues;
	u32             mask, fwsm;

	mask = (IXGBE_EIMS_ENABLE_MASK & ~IXGBE_EIMS_RTX_QUEUE);

	switch (adapter->hw.mac.type) {
	case ixgbe_mac_82599EB:
		mask |= IXGBE_EIMS_ECC;
		/* Temperature sensor on some adapters */
		mask |= IXGBE_EIMS_GPI_SDP0;
		/* SFP+ (RX_LOS_N & MOD_ABS_N) */
		mask |= IXGBE_EIMS_GPI_SDP1;
		mask |= IXGBE_EIMS_GPI_SDP2;
		break;
	case ixgbe_mac_X540:
		/* Detect if Thermal Sensor is enabled */
		fwsm = IXGBE_READ_REG(hw, IXGBE_FWSM);
		if (fwsm & IXGBE_FWSM_TS_ENABLED)
			mask |= IXGBE_EIMS_TS;
		mask |= IXGBE_EIMS_ECC;
		break;
	case ixgbe_mac_X550:
		/* MAC thermal sensor is automatically enabled */
		mask |= IXGBE_EIMS_TS;
		mask |= IXGBE_EIMS_ECC;
		break;
	case ixgbe_mac_X550EM_x:
	case ixgbe_mac_X550EM_a:
		/* Some devices use SDP0 for important information */
		if (hw->device_id == IXGBE_DEV_ID_X550EM_X_SFP ||
		    hw->device_id == IXGBE_DEV_ID_X550EM_A_SFP ||
		    hw->device_id == IXGBE_DEV_ID_X550EM_A_SFP_N ||
		    hw->device_id == IXGBE_DEV_ID_X550EM_X_10G_T)
			mask |= IXGBE_EIMS_GPI_SDP0_BY_MAC(hw);
		if (hw->phy.type == ixgbe_phy_x550em_ext_t)
			mask |= IXGBE_EICR_GPI_SDP0_X540;
		mask |= IXGBE_EIMS_ECC;
		break;
	default:
		break;
	}

	/* Enable Fan Failure detection */
	if (adapter->feat_en & IXGBE_FEATURE_FAN_FAIL)
		mask |= IXGBE_EIMS_GPI_SDP1;
	/* Enable SR-IOV */
	if (adapter->feat_en & IXGBE_FEATURE_SRIOV)
		mask |= IXGBE_EIMS_MAILBOX;
	/* Enable Flow Director */
	if (adapter->feat_en & IXGBE_FEATURE_FDIR)
		mask |= IXGBE_EIMS_FLOW_DIR;

	IXGBE_WRITE_REG(hw, IXGBE_EIMS, mask);

	/* With MSI-X we use auto clear */
	if (adapter->msix_mem) {
		mask = IXGBE_EIMS_ENABLE_MASK;
		/* Don't autoclear Link */
		mask &= ~IXGBE_EIMS_OTHER;
		mask &= ~IXGBE_EIMS_LSC;
		if (adapter->feat_cap & IXGBE_FEATURE_SRIOV)
			mask &= ~IXGBE_EIMS_MAILBOX;
		IXGBE_WRITE_REG(hw, IXGBE_EIAC, mask);
	}

	if (!skip_traffic) {
		/*
		 * Now enable all queues, this is done separately to
		 * allow for handling the extended (beyond 32) MSI-X
		 * vectors that can be used by 82599
		 */
		for (int i = 0; i < adapter->num_queues; i++, que++)
			ixgbe_enable_queue(adapter, que->msix);
	}

	IXGBE_WRITE_FLUSH(hw);

	return;
} /* ixgbe_enable_intr */

/************************************************************************
 * ixgbe_disable_intr
 *     If keep_traffic parameter is set, queue interrupts are not disabled.
 *     This is needed by ixgbe_handle_admin_task() to handle link specific
 *     interrupt procedures without stopping the traffic.
 ************************************************************************/
static void
ixgbe_disable_intr(struct adapter *adapter, bool keep_traffic)
{
	struct ixgbe_hw *hw = &adapter->hw;
	u32 eiac_mask, eimc_mask, eimc_ext_mask;

	if (keep_traffic) {
		/* Autoclear only queue irqs */
		eiac_mask = IXGBE_EICR_RTX_QUEUE;

		/* Disable everything but queue irqs */
		eimc_mask = ~0;
		eimc_mask &= ~IXGBE_EIMC_RTX_QUEUE;
		eimc_ext_mask = 0;
	} else {
		eiac_mask = 0;
		eimc_mask = (hw->mac.type == ixgbe_mac_82598EB) ? ~0 : 0xFFFF0000;
		eimc_ext_mask = ~0;
	}

	if (adapter->msix_mem)
		IXGBE_WRITE_REG(hw, IXGBE_EIAC, eiac_mask);

	IXGBE_WRITE_REG(hw, IXGBE_EIMC, eimc_mask);
	IXGBE_WRITE_REG(hw, IXGBE_EIMC_EX(0), eimc_ext_mask);
	IXGBE_WRITE_REG(hw, IXGBE_EIMC_EX(1), eimc_ext_mask);

	IXGBE_WRITE_FLUSH(hw);

	return;
} /* ixgbe_disable_intr */

/************************************************************************
 * ixgbe_legacy_irq - Legacy Interrupt Service routine
 ************************************************************************/
static void
ixgbe_legacy_irq(void *arg)
{
	struct ix_queue *que = arg;
	struct adapter  *adapter = que->adapter;
	struct ixgbe_hw *hw = &adapter->hw;
	struct ifnet    *ifp = adapter->ifp;
	struct tx_ring  *txr = adapter->tx_rings;
	bool            more = false;
	u32             eicr, eicr_mask;

	/* Silicon errata #26 on 82598 */
	IXGBE_WRITE_REG(hw, IXGBE_EIMC, IXGBE_IRQ_CLEAR_MASK);

	eicr = IXGBE_READ_REG(hw, IXGBE_EICR);

	++que->irqs;
	if (eicr == 0) {
		ixgbe_enable_intr(adapter, false);
		return;
	}

	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		more = ixgbe_rxeof(que);

		IXGBE_TX_LOCK(txr);
		ixgbe_txeof(txr);
		if (!ixgbe_ring_empty(ifp, txr->br))
			ixgbe_start_locked(ifp, txr);
		IXGBE_TX_UNLOCK(txr);
	}

	/* Check for fan failure */
	if (adapter->feat_en & IXGBE_FEATURE_FAN_FAIL) {
		ixgbe_check_fan_failure(adapter, eicr, true);
		IXGBE_WRITE_REG(hw, IXGBE_EIMS, IXGBE_EICR_GPI_SDP1_BY_MAC(hw));
	}

	/* Link status change */
	if (eicr & IXGBE_EICR_LSC){
		adapter->task_requests |= IXGBE_REQUEST_TASK_LINK;
		taskqueue_enqueue(adapter->tq, &adapter->admin_task);
	}

	if (ixgbe_is_sfp(hw)) {
		/* Pluggable optics-related interrupt */
		if (hw->mac.type >= ixgbe_mac_X540)
			eicr_mask = IXGBE_EICR_GPI_SDP0_X540;
		else
			eicr_mask = IXGBE_EICR_GPI_SDP2_BY_MAC(hw);

		if (eicr & eicr_mask) {
			IXGBE_WRITE_REG(hw, IXGBE_EICR, eicr_mask);
			adapter->task_requests |= IXGBE_REQUEST_TASK_MOD;
			taskqueue_enqueue(adapter->tq, &adapter->admin_task);
		}

		if ((hw->mac.type == ixgbe_mac_82599EB) &&
		    (eicr & IXGBE_EICR_GPI_SDP1_BY_MAC(hw))) {
			IXGBE_WRITE_REG(hw, IXGBE_EICR,
			    IXGBE_EICR_GPI_SDP1_BY_MAC(hw));
			adapter->task_requests |= IXGBE_REQUEST_TASK_MSF;
			taskqueue_enqueue(adapter->tq, &adapter->admin_task);
		}
	}

	/* External PHY interrupt */
	if ((hw->phy.type == ixgbe_phy_x550em_ext_t) &&
	    (eicr & IXGBE_EICR_GPI_SDP0_X540)) {
		adapter->task_requests |= IXGBE_REQUEST_TASK_PHY;
		taskqueue_enqueue(adapter->tq, &adapter->admin_task);
	}

	if (more)
		taskqueue_enqueue(que->tq, &que->que_task);
	else
		ixgbe_enable_intr(adapter, false);

	return;
} /* ixgbe_legacy_irq */

/************************************************************************
 * ixgbe_free_pci_resources
 ************************************************************************/
static void
ixgbe_free_pci_resources(struct adapter *adapter)
{
	struct ix_queue *que = adapter->queues;
	device_t        dev = adapter->dev;
	int             rid, memrid;

	if (adapter->hw.mac.type == ixgbe_mac_82598EB)
		memrid = PCIR_BAR(MSIX_82598_BAR);
	else
		memrid = PCIR_BAR(MSIX_82599_BAR);

	/*
	 * There is a slight possibility of a failure mode
	 * in attach that will result in entering this function
	 * before interrupt resources have been initialized, and
	 * in that case we do not want to execute the loops below
	 * We can detect this reliably by the state of the adapter
	 * res pointer.
	 */
	if (adapter->res == NULL)
		goto mem;

	/*
	 * Release all msix queue resources:
	 */
	for (int i = 0; i < adapter->num_queues; i++, que++) {
		rid = que->msix + 1;
		if (que->tag != NULL) {
			bus_teardown_intr(dev, que->res, que->tag);
			que->tag = NULL;
		}
		if (que->res != NULL)
			bus_release_resource(dev, SYS_RES_IRQ, rid, que->res);
	}


	if (adapter->tag != NULL) {
		bus_teardown_intr(dev, adapter->res, adapter->tag);
		adapter->tag = NULL;
	}

	/* Clean the Legacy or Link interrupt last */
	if (adapter->res != NULL)
		bus_release_resource(dev, SYS_RES_IRQ, adapter->link_rid,
		    adapter->res);

mem:
	if ((adapter->feat_en & IXGBE_FEATURE_MSI) ||
	    (adapter->feat_en & IXGBE_FEATURE_MSIX))
		pci_release_msi(dev);

	if (adapter->msix_mem != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, memrid,
		    adapter->msix_mem);

	if (adapter->pci_mem != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, PCIR_BAR(0),
		    adapter->pci_mem);

	return;
} /* ixgbe_free_pci_resources */

/************************************************************************
 * ixgbe_set_sysctl_value
 ************************************************************************/
static void
ixgbe_set_sysctl_value(struct adapter *adapter, const char *name,
    const char *description, int *limit, int value)
{
	*limit = value;
	SYSCTL_ADD_INT(device_get_sysctl_ctx(adapter->dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(adapter->dev)),
	    OID_AUTO, name, CTLFLAG_RW, limit, value, description);
} /* ixgbe_set_sysctl_value */

/************************************************************************
 * ixgbe_sysctl_flowcntl
 *
 *   SYSCTL wrapper around setting Flow Control
 ************************************************************************/
static int
ixgbe_sysctl_flowcntl(SYSCTL_HANDLER_ARGS)
{
	struct adapter *adapter;
	int            error, fc;

	adapter = (struct adapter *)arg1;
	fc = adapter->hw.fc.current_mode;

	error = sysctl_handle_int(oidp, &fc, 0, req);
	if ((error) || (req->newptr == NULL))
		return (error);

	/* Don't bother if it's not changed */
	if (fc == adapter->hw.fc.current_mode)
		return (0);

	return ixgbe_set_flowcntl(adapter, fc);
} /* ixgbe_sysctl_flowcntl */

/************************************************************************
 * ixgbe_set_flowcntl - Set flow control
 *
 *   Flow control values:
 *     0 - off
 *     1 - rx pause
 *     2 - tx pause
 *     3 - full
 ************************************************************************/
static int
ixgbe_set_flowcntl(struct adapter *adapter, int fc)
{
	switch (fc) {
	case ixgbe_fc_rx_pause:
	case ixgbe_fc_tx_pause:
	case ixgbe_fc_full:
		adapter->hw.fc.requested_mode = fc;
		if (adapter->num_queues > 1)
			ixgbe_disable_rx_drop(adapter);
		break;
	case ixgbe_fc_none:
		adapter->hw.fc.requested_mode = ixgbe_fc_none;
		if (adapter->num_queues > 1)
			ixgbe_enable_rx_drop(adapter);
		break;
	default:
		return (EINVAL);
	}

	/* Don't autoneg if forcing a value */
	adapter->hw.fc.disable_fc_autoneg = TRUE;
	ixgbe_fc_enable(&adapter->hw);

	return (0);
} /* ixgbe_set_flowcntl */

/************************************************************************
 * ixgbe_enable_rx_drop
 *
 *   Enable the hardware to drop packets when the buffer is
 *   full. This is useful with multiqueue, so that no single
 *   queue being full stalls the entire RX engine. We only
 *   enable this when Multiqueue is enabled AND Flow Control
 *   is disabled.
 ************************************************************************/
static void
ixgbe_enable_rx_drop(struct adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;
	struct rx_ring  *rxr;
	u32             srrctl;

	for (int i = 0; i < adapter->num_queues; i++) {
		rxr = &adapter->rx_rings[i];
		srrctl = IXGBE_READ_REG(hw, IXGBE_SRRCTL(rxr->me));
		srrctl |= IXGBE_SRRCTL_DROP_EN;
		IXGBE_WRITE_REG(hw, IXGBE_SRRCTL(rxr->me), srrctl);
	}

	/* enable drop for each vf */
	for (int i = 0; i < adapter->num_vfs; i++) {
		IXGBE_WRITE_REG(hw, IXGBE_QDE,
		    (IXGBE_QDE_WRITE | (i << IXGBE_QDE_IDX_SHIFT) |
		    IXGBE_QDE_ENABLE));
	}
} /* ixgbe_enable_rx_drop */

/************************************************************************
 * ixgbe_disable_rx_drop
 ************************************************************************/
static void
ixgbe_disable_rx_drop(struct adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;
	struct rx_ring  *rxr;
	u32             srrctl;

	for (int i = 0; i < adapter->num_queues; i++) {
		rxr = &adapter->rx_rings[i];
		srrctl = IXGBE_READ_REG(hw, IXGBE_SRRCTL(rxr->me));
		srrctl &= ~IXGBE_SRRCTL_DROP_EN;
		IXGBE_WRITE_REG(hw, IXGBE_SRRCTL(rxr->me), srrctl);
	}

	/* disable drop for each vf */
	for (int i = 0; i < adapter->num_vfs; i++) {
		IXGBE_WRITE_REG(hw, IXGBE_QDE,
		    (IXGBE_QDE_WRITE | (i << IXGBE_QDE_IDX_SHIFT)));
	}
} /* ixgbe_disable_rx_drop */

/************************************************************************
 * ixgbe_sysctl_advertise
 *
 *   SYSCTL wrapper around setting advertised speed
 ************************************************************************/
static int
ixgbe_sysctl_advertise(SYSCTL_HANDLER_ARGS)
{
	struct adapter *adapter;
	int            error, advertise;

	adapter = (struct adapter *)arg1;
	advertise = adapter->advertise;

	error = sysctl_handle_int(oidp, &advertise, 0, req);
	if ((error) || (req->newptr == NULL))
		return (error);

	return ixgbe_set_advertise(adapter, advertise);
} /* ixgbe_sysctl_advertise */

/************************************************************************
 * ixgbe_set_advertise - Control advertised link speed
 *
 *   Flags:
 *     0x1 - advertise 100 Mb
 *     0x2 - advertise 1G
 *     0x4 - advertise 10G
 *     0x8 - advertise 10 Mb (yes, Mb)
 ************************************************************************/
static int
ixgbe_set_advertise(struct adapter *adapter, int advertise)
{
	device_t         dev;
	struct ixgbe_hw  *hw;
	ixgbe_link_speed speed = 0;
	ixgbe_link_speed link_caps = 0;
	s32              err = IXGBE_NOT_IMPLEMENTED;
	bool             negotiate = FALSE;

	/* Checks to validate new value */
	if (adapter->advertise == advertise) /* no change */
		return (0);

	dev = adapter->dev;
	hw = &adapter->hw;

	/* No speed changes for backplane media */
	if (hw->phy.media_type == ixgbe_media_type_backplane)
		return (ENODEV);

	if (!((hw->phy.media_type == ixgbe_media_type_copper) ||
	      (hw->phy.multispeed_fiber))) {
		device_printf(dev, "Advertised speed can only be set on copper or multispeed fiber media types.\n");
		return (EINVAL);
	}

	if (advertise < 0x1 || advertise > 0xF) {
		device_printf(dev, "Invalid advertised speed; valid modes are 0x1 through 0xF\n");
		return (EINVAL);
	}

	if (hw->mac.ops.get_link_capabilities) {
		err = hw->mac.ops.get_link_capabilities(hw, &link_caps,
		    &negotiate);
		if (err != IXGBE_SUCCESS) {
			device_printf(dev, "Unable to determine supported advertise speeds\n");
			return (ENODEV);
		}
	}

	/* Set new value and report new advertised mode */
	if (advertise & 0x1) {
		if (!(link_caps & IXGBE_LINK_SPEED_100_FULL)) {
			device_printf(dev, "Interface does not support 100Mb advertised speed\n");
			return (EINVAL);
		}
		speed |= IXGBE_LINK_SPEED_100_FULL;
	}
	if (advertise & 0x2) {
		if (!(link_caps & IXGBE_LINK_SPEED_1GB_FULL)) {
			device_printf(dev, "Interface does not support 1Gb advertised speed\n");
			return (EINVAL);
		}
		speed |= IXGBE_LINK_SPEED_1GB_FULL;
	}
	if (advertise & 0x4) {
		if (!(link_caps & IXGBE_LINK_SPEED_10GB_FULL)) {
			device_printf(dev, "Interface does not support 10Gb advertised speed\n");
			return (EINVAL);
		}
		speed |= IXGBE_LINK_SPEED_10GB_FULL;
	}
	if (advertise & 0x8) {
		if (!(link_caps & IXGBE_LINK_SPEED_10_FULL)) {
			device_printf(dev, "Interface does not support 10Mb advertised speed\n");
			return (EINVAL);
		}
		speed |= IXGBE_LINK_SPEED_10_FULL;
	}

	hw->mac.autotry_restart = TRUE;
	hw->mac.ops.setup_link(hw, speed, TRUE);
	adapter->advertise = advertise;

	return (0);
} /* ixgbe_set_advertise */

/************************************************************************
 * ixgbe_get_advertise - Get current advertised speed settings
 *
 *   Formatted for sysctl usage.
 *   Flags:
 *     0x1 - advertise 100 Mb
 *     0x2 - advertise 1G
 *     0x4 - advertise 10G
 *     0x8 - advertise 10 Mb (yes, Mb)
 ************************************************************************/
static int
ixgbe_get_advertise(struct adapter *adapter)
{
	struct ixgbe_hw  *hw = &adapter->hw;
	int              speed;
	ixgbe_link_speed link_caps = 0;
	s32              err;
	bool             negotiate = FALSE;

	/*
	 * Advertised speed means nothing unless it's copper or
	 * multi-speed fiber
	 */
	if (!(hw->phy.media_type == ixgbe_media_type_copper) &&
	    !(hw->phy.multispeed_fiber))
		return (0);

	err = hw->mac.ops.get_link_capabilities(hw, &link_caps, &negotiate);
	if (err != IXGBE_SUCCESS)
		return (0);

	speed =
	    ((link_caps & IXGBE_LINK_SPEED_10GB_FULL) ? 4 : 0) |
	    ((link_caps & IXGBE_LINK_SPEED_1GB_FULL)  ? 2 : 0) |
	    ((link_caps & IXGBE_LINK_SPEED_100_FULL)  ? 1 : 0) |
	    ((link_caps & IXGBE_LINK_SPEED_10_FULL)   ? 8 : 0);

	return speed;
} /* ixgbe_get_advertise */

/************************************************************************
 * ixgbe_sysctl_dmac - Manage DMA Coalescing
 *
 *   Control values:
 *     0/1 - off / on (use default value of 1000)
 *
 *     Legal timer values are:
 *     50,100,250,500,1000,2000,5000,10000
 *
 *     Turning off interrupt moderation will also turn this off.
 ************************************************************************/
static int
ixgbe_sysctl_dmac(SYSCTL_HANDLER_ARGS)
{
	struct adapter *adapter = (struct adapter *)arg1;
	struct ifnet   *ifp = adapter->ifp;
	int            error;
	u32            newval;

	newval = adapter->dmac;
	error = sysctl_handle_int(oidp, &newval, 0, req);
	if ((error) || (req->newptr == NULL))
		return (error);

	switch (newval) {
	case 0:
		/* Disabled */
		adapter->dmac = 0;
		break;
	case 1:
		/* Enable and use default */
		adapter->dmac = 1000;
		break;
	case 50:
	case 100:
	case 250:
	case 500:
	case 1000:
	case 2000:
	case 5000:
	case 10000:
		/* Legal values - allow */
		adapter->dmac = newval;
		break;
	default:
		/* Do nothing, illegal value */
		return (EINVAL);
	}

	/* Re-initialize hardware if it's already running */
	if (ifp->if_drv_flags & IFF_DRV_RUNNING)
		ixgbe_init(adapter);

	return (0);
} /* ixgbe_sysctl_dmac */

#ifdef IXGBE_DEBUG
/************************************************************************
 * ixgbe_sysctl_power_state
 *
 *   Sysctl to test power states
 *   Values:
 *     0      - set device to D0
 *     3      - set device to D3
 *     (none) - get current device power state
 ************************************************************************/
static int
ixgbe_sysctl_power_state(SYSCTL_HANDLER_ARGS)
{
	struct adapter *adapter = (struct adapter *)arg1;
	device_t       dev = adapter->dev;
	int            curr_ps, new_ps, error = 0;

	curr_ps = new_ps = pci_get_powerstate(dev);

	error = sysctl_handle_int(oidp, &new_ps, 0, req);
	if ((error) || (req->newptr == NULL))
		return (error);

	if (new_ps == curr_ps)
		return (0);

	if (new_ps == 3 && curr_ps == 0)
		error = DEVICE_SUSPEND(dev);
	else if (new_ps == 0 && curr_ps == 3)
		error = DEVICE_RESUME(dev);
	else
		return (EINVAL);

	device_printf(dev, "New state: %d\n", pci_get_powerstate(dev));

	return (error);
} /* ixgbe_sysctl_power_state */
#endif

/************************************************************************
 * ixgbe_sysctl_wol_enable
 *
 *   Sysctl to enable/disable the WoL capability,
 *   if supported by the adapter.
 *
 *   Values:
 *     0 - disabled
 *     1 - enabled
 ************************************************************************/
static int
ixgbe_sysctl_wol_enable(SYSCTL_HANDLER_ARGS)
{
	struct adapter  *adapter = (struct adapter *)arg1;
	struct ixgbe_hw *hw = &adapter->hw;
	int             new_wol_enabled;
	int             error = 0;

	new_wol_enabled = hw->wol_enabled;
	error = sysctl_handle_int(oidp, &new_wol_enabled, 0, req);
	if ((error) || (req->newptr == NULL))
		return (error);
	new_wol_enabled = !!(new_wol_enabled);
	if (new_wol_enabled == hw->wol_enabled)
		return (0);

	if (new_wol_enabled > 0 && !adapter->wol_support)
		return (ENODEV);
	else
		hw->wol_enabled = new_wol_enabled;

	return (0);
} /* ixgbe_sysctl_wol_enable */

/************************************************************************
 * ixgbe_sysctl_wufc - Wake Up Filter Control
 *
 *   Sysctl to enable/disable the types of packets that the
 *   adapter will wake up on upon receipt.
 *   Flags:
 *     0x1  - Link Status Change
 *     0x2  - Magic Packet
 *     0x4  - Direct Exact
 *     0x8  - Directed Multicast
 *     0x10 - Broadcast
 *     0x20 - ARP/IPv4 Request Packet
 *     0x40 - Direct IPv4 Packet
 *     0x80 - Direct IPv6 Packet
 *
 *   Settings not listed above will cause the sysctl to return an error.
 ************************************************************************/
static int
ixgbe_sysctl_wufc(SYSCTL_HANDLER_ARGS)
{
	struct adapter *adapter = (struct adapter *)arg1;
	int            error = 0;
	u32            new_wufc;

	new_wufc = adapter->wufc;

	error = sysctl_handle_int(oidp, &new_wufc, 0, req);
	if ((error) || (req->newptr == NULL))
		return (error);
	if (new_wufc == adapter->wufc)
		return (0);

	if (new_wufc & 0xffffff00)
		return (EINVAL);

	new_wufc &= 0xff;
	new_wufc |= (0xffffff & adapter->wufc);
	adapter->wufc = new_wufc;

	return (0);
} /* ixgbe_sysctl_wufc */

#ifdef IXGBE_DEBUG
/************************************************************************
 * ixgbe_sysctl_print_rss_config
 ************************************************************************/
static int
ixgbe_sysctl_print_rss_config(SYSCTL_HANDLER_ARGS)
{
	struct adapter  *adapter = (struct adapter *)arg1;
	struct ixgbe_hw *hw = &adapter->hw;
	device_t        dev = adapter->dev;
	struct sbuf     *buf;
	int             error = 0, reta_size;
	u32             reg;

	buf = sbuf_new_for_sysctl(NULL, NULL, 128, req);
	if (!buf) {
		device_printf(dev, "Could not allocate sbuf for output.\n");
		return (ENOMEM);
	}

	// TODO: use sbufs to make a string to print out
	/* Set multiplier for RETA setup and table size based on MAC */
	switch (adapter->hw.mac.type) {
	case ixgbe_mac_X550:
	case ixgbe_mac_X550EM_x:
	case ixgbe_mac_X550EM_a:
		reta_size = 128;
		break;
	default:
		reta_size = 32;
		break;
	}

	/* Print out the redirection table */
	sbuf_cat(buf, "\n");
	for (int i = 0; i < reta_size; i++) {
		if (i < 32) {
			reg = IXGBE_READ_REG(hw, IXGBE_RETA(i));
			sbuf_printf(buf, "RETA(%2d): 0x%08x\n", i, reg);
		} else {
			reg = IXGBE_READ_REG(hw, IXGBE_ERETA(i - 32));
			sbuf_printf(buf, "ERETA(%2d): 0x%08x\n", i - 32, reg);
		}
	}

	// TODO: print more config

	error = sbuf_finish(buf);
	if (error)
		device_printf(dev, "Error finishing sbuf: %d\n", error);

	sbuf_delete(buf);

	return (0);
} /* ixgbe_sysctl_print_rss_config */
#endif /* IXGBE_DEBUG */

/************************************************************************
 * ixgbe_sysctl_phy_temp - Retrieve temperature of PHY
 *
 *   For X552/X557-AT devices using an external PHY
 ************************************************************************/
static int
ixgbe_sysctl_phy_temp(SYSCTL_HANDLER_ARGS)
{
	struct adapter  *adapter = (struct adapter *)arg1;
	struct ixgbe_hw *hw = &adapter->hw;
	u16             reg;

	if (hw->device_id != IXGBE_DEV_ID_X550EM_X_10G_T) {
		device_printf(adapter->dev,
		    "Device has no supported external thermal sensor.\n");
		return (ENODEV);
	}

	if (hw->phy.ops.read_reg(hw, IXGBE_PHY_CURRENT_TEMP,
	    IXGBE_MDIO_VENDOR_SPECIFIC_1_DEV_TYPE, &reg)) {
		device_printf(adapter->dev,
		    "Error reading from PHY's current temperature register\n");
		return (EAGAIN);
	}

	/* Shift temp for output */
	reg = reg >> 8;

	return (sysctl_handle_int(oidp, NULL, reg, req));
} /* ixgbe_sysctl_phy_temp */

/************************************************************************
 * ixgbe_sysctl_phy_overtemp_occurred
 *
 *   Reports (directly from the PHY) whether the current PHY
 *   temperature is over the overtemp threshold.
 ************************************************************************/
static int
ixgbe_sysctl_phy_overtemp_occurred(SYSCTL_HANDLER_ARGS)
{
	struct adapter  *adapter = (struct adapter *)arg1;
	struct ixgbe_hw *hw = &adapter->hw;
	u16             reg;

	if (hw->device_id != IXGBE_DEV_ID_X550EM_X_10G_T) {
		device_printf(adapter->dev,
		    "Device has no supported external thermal sensor.\n");
		return (ENODEV);
	}

	if (hw->phy.ops.read_reg(hw, IXGBE_PHY_OVERTEMP_STATUS,
	    IXGBE_MDIO_VENDOR_SPECIFIC_1_DEV_TYPE, &reg)) {
		device_printf(adapter->dev,
		    "Error reading from PHY's temperature status register\n");
		return (EAGAIN);
	}

	/* Get occurrence bit */
	reg = !!(reg & 0x4000);

	return (sysctl_handle_int(oidp, 0, reg, req));
} /* ixgbe_sysctl_phy_overtemp_occurred */

/************************************************************************
 * ixgbe_sysctl_eee_state
 *
 *   Sysctl to set EEE power saving feature
 *   Values:
 *     0      - disable EEE
 *     1      - enable EEE
 *     (none) - get current device EEE state
 ************************************************************************/
static int
ixgbe_sysctl_eee_state(SYSCTL_HANDLER_ARGS)
{
	struct adapter *adapter = (struct adapter *)arg1;
	device_t       dev = adapter->dev;
	int            curr_eee, new_eee, error = 0;
	s32            retval;

	curr_eee = new_eee = !!(adapter->feat_en & IXGBE_FEATURE_EEE);

	error = sysctl_handle_int(oidp, &new_eee, 0, req);
	if ((error) || (req->newptr == NULL))
		return (error);

	/* Nothing to do */
	if (new_eee == curr_eee)
		return (0);

	/* Not supported */
	if (!(adapter->feat_cap & IXGBE_FEATURE_EEE))
		return (EINVAL);

	/* Bounds checking */
	if ((new_eee < 0) || (new_eee > 1))
		return (EINVAL);

	retval = adapter->hw.mac.ops.setup_eee(&adapter->hw, new_eee);
	if (retval) {
		device_printf(dev, "Error in EEE setup: 0x%08X\n", retval);
		return (EINVAL);
	}

	/* Restart auto-neg */
	ixgbe_init(adapter);

	device_printf(dev, "New EEE state: %d\n", new_eee);

	/* Cache new value */
	if (new_eee)
		adapter->feat_en |= IXGBE_FEATURE_EEE;
	else
		adapter->feat_en &= ~IXGBE_FEATURE_EEE;

	return (error);
} /* ixgbe_sysctl_eee_state */

/************************************************************************
 * ixgbe_init_device_features
 ************************************************************************/
static void
ixgbe_init_device_features(struct adapter *adapter)
{
	adapter->feat_cap = IXGBE_FEATURE_NETMAP
	                  | IXGBE_FEATURE_RSS
	                  | IXGBE_FEATURE_MSI
	                  | IXGBE_FEATURE_MSIX
	                  | IXGBE_FEATURE_LEGACY_IRQ
	                  | IXGBE_FEATURE_LEGACY_TX;

	/* Set capabilities first... */
	switch (adapter->hw.mac.type) {
	case ixgbe_mac_82598EB:
		if (adapter->hw.device_id == IXGBE_DEV_ID_82598AT)
			adapter->feat_cap |= IXGBE_FEATURE_FAN_FAIL;
		break;
	case ixgbe_mac_X540:
		adapter->feat_cap |= IXGBE_FEATURE_SRIOV;
		adapter->feat_cap |= IXGBE_FEATURE_FDIR;
		if ((adapter->hw.device_id == IXGBE_DEV_ID_X540_BYPASS) &&
		    (adapter->hw.bus.func == 0))
			adapter->feat_cap |= IXGBE_FEATURE_BYPASS;
		break;
	case ixgbe_mac_X550:
		adapter->feat_cap |= IXGBE_FEATURE_TEMP_SENSOR;
		adapter->feat_cap |= IXGBE_FEATURE_SRIOV;
		adapter->feat_cap |= IXGBE_FEATURE_FDIR;
		break;
	case ixgbe_mac_X550EM_x:
		adapter->feat_cap |= IXGBE_FEATURE_SRIOV;
		adapter->feat_cap |= IXGBE_FEATURE_FDIR;
		if (adapter->hw.device_id == IXGBE_DEV_ID_X550EM_X_KR)
			adapter->feat_cap |= IXGBE_FEATURE_EEE;
		break;
	case ixgbe_mac_X550EM_a:
		adapter->feat_cap |= IXGBE_FEATURE_SRIOV;
		adapter->feat_cap |= IXGBE_FEATURE_FDIR;
		adapter->feat_cap &= ~IXGBE_FEATURE_LEGACY_IRQ;
		if ((adapter->hw.device_id == IXGBE_DEV_ID_X550EM_A_1G_T) ||
		    (adapter->hw.device_id == IXGBE_DEV_ID_X550EM_A_1G_T_L)) {
			adapter->feat_cap |= IXGBE_FEATURE_TEMP_SENSOR;
			adapter->feat_cap |= IXGBE_FEATURE_EEE;
		}
		break;
	case ixgbe_mac_82599EB:
		adapter->feat_cap |= IXGBE_FEATURE_SRIOV;
		adapter->feat_cap |= IXGBE_FEATURE_FDIR;
		if ((adapter->hw.device_id == IXGBE_DEV_ID_82599_BYPASS) &&
		    (adapter->hw.bus.func == 0))
			adapter->feat_cap |= IXGBE_FEATURE_BYPASS;
		if (adapter->hw.device_id == IXGBE_DEV_ID_82599_QSFP_SF_QP)
			adapter->feat_cap &= ~IXGBE_FEATURE_LEGACY_IRQ;
		break;
	default:
		break;
	}

	/* Enabled by default... */
	/* Fan failure detection */
	if (adapter->feat_cap & IXGBE_FEATURE_FAN_FAIL)
		adapter->feat_en |= IXGBE_FEATURE_FAN_FAIL;
	/* Netmap */
	if (adapter->feat_cap & IXGBE_FEATURE_NETMAP)
		adapter->feat_en |= IXGBE_FEATURE_NETMAP;
	/* EEE */
	if (adapter->feat_cap & IXGBE_FEATURE_EEE)
		adapter->feat_en |= IXGBE_FEATURE_EEE;
	/* Thermal Sensor */
	if (adapter->feat_cap & IXGBE_FEATURE_TEMP_SENSOR)
		adapter->feat_en |= IXGBE_FEATURE_TEMP_SENSOR;

	/* Enabled via global sysctl... */
	/* Flow Director */
	if (ixgbe_enable_fdir) {
		if (adapter->feat_cap & IXGBE_FEATURE_FDIR)
			adapter->feat_en |= IXGBE_FEATURE_FDIR;
		else
			device_printf(adapter->dev, "Device does not support Flow Director. Leaving disabled.");
	}
	/* Legacy (single queue) transmit */
	if ((adapter->feat_cap & IXGBE_FEATURE_LEGACY_TX) &&
	    ixgbe_enable_legacy_tx)
		adapter->feat_en |= IXGBE_FEATURE_LEGACY_TX;
	/*
	 * Message Signal Interrupts - Extended (MSI-X)
	 * Normal MSI is only enabled if MSI-X calls fail.
	 */
	if (!ixgbe_enable_msix)
		adapter->feat_cap &= ~IXGBE_FEATURE_MSIX;
	/* Receive-Side Scaling (RSS) */
	if ((adapter->feat_cap & IXGBE_FEATURE_RSS) && ixgbe_enable_rss)
		adapter->feat_en |= IXGBE_FEATURE_RSS;

	/* Disable features with unmet dependencies... */
	/* No MSI-X */
	if (!(adapter->feat_cap & IXGBE_FEATURE_MSIX)) {
		adapter->feat_cap &= ~IXGBE_FEATURE_RSS;
		adapter->feat_cap &= ~IXGBE_FEATURE_SRIOV;
		adapter->feat_en &= ~IXGBE_FEATURE_RSS;
		adapter->feat_en &= ~IXGBE_FEATURE_SRIOV;
	}
} /* ixgbe_init_device_features */

/************************************************************************
 * ixgbe_probe - Device identification routine
 *
 *   Determines if the driver should be loaded on
 *   adapter based on its PCI vendor/device ID.
 *
 *   return BUS_PROBE_DEFAULT on success, positive on failure
 ************************************************************************/
static int
ixgbe_probe(device_t dev)
{
	ixgbe_vendor_info_t *ent;

	u16  pci_vendor_id = 0;
	u16  pci_device_id = 0;
	u16  pci_subvendor_id = 0;
	u16  pci_subdevice_id = 0;
	char adapter_name[256];

	INIT_DEBUGOUT("ixgbe_probe: begin");

	pci_vendor_id = pci_get_vendor(dev);
	if (pci_vendor_id != IXGBE_INTEL_VENDOR_ID)
		return (ENXIO);

	pci_device_id = pci_get_device(dev);
	pci_subvendor_id = pci_get_subvendor(dev);
	pci_subdevice_id = pci_get_subdevice(dev);

	ent = ixgbe_vendor_info_array;
	while (ent->vendor_id != 0) {
		if ((pci_vendor_id == ent->vendor_id) &&
		    (pci_device_id == ent->device_id) &&
		    ((pci_subvendor_id == ent->subvendor_id) ||
		     (ent->subvendor_id == 0)) &&
		    ((pci_subdevice_id == ent->subdevice_id) ||
		     (ent->subdevice_id == 0))) {
			sprintf(adapter_name, "%s, Version - %s",
				ixgbe_strings[ent->index],
				ixgbe_driver_version);
			device_set_desc_copy(dev, adapter_name);
			++ixgbe_total_ports;
			return (BUS_PROBE_DEFAULT);
		}
		ent++;
	}

	return (ENXIO);
} /* ixgbe_probe */


/************************************************************************
 * ixgbe_ioctl - Ioctl entry point
 *
 *   Called when the user wants to configure the interface.
 *
 *   return 0 on success, positive on failure
 ************************************************************************/
static int
ixgbe_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct adapter *adapter = ifp->if_softc;
	struct ifreq   *ifr = (struct ifreq *) data;
#if defined(INET) || defined(INET6)
	struct ifaddr  *ifa = (struct ifaddr *)data;
#endif
	int            error = 0;
	bool           avoid_reset = FALSE;

	switch (command) {
	case SIOCSIFADDR:
#ifdef INET
		if (ifa->ifa_addr->sa_family == AF_INET)
			avoid_reset = TRUE;
#endif
#ifdef INET6
		if (ifa->ifa_addr->sa_family == AF_INET6)
			avoid_reset = TRUE;
#endif
		/*
		 * Calling init results in link renegotiation,
		 * so we avoid doing it when possible.
		 */
		if (avoid_reset) {
			ifp->if_flags |= IFF_UP;
			if (!(ifp->if_drv_flags & IFF_DRV_RUNNING))
				ixgbe_init(adapter);
#ifdef INET
			if (!(ifp->if_flags & IFF_NOARP))
				arp_ifinit(ifp, ifa);
#endif
		} else
			error = ether_ioctl(ifp, command, data);
		break;
	case SIOCSIFMTU:
		IOCTL_DEBUGOUT("ioctl: SIOCSIFMTU (Set Interface MTU)");
		if (ifr->ifr_mtu > IXGBE_MAX_MTU) {
			error = EINVAL;
		} else {
			IXGBE_CORE_LOCK(adapter);
			ifp->if_mtu = ifr->ifr_mtu;
			adapter->max_frame_size = ifp->if_mtu + IXGBE_MTU_HDR;
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				ixgbe_init_locked(adapter);
			ixgbe_recalculate_max_frame(adapter);
			IXGBE_CORE_UNLOCK(adapter);
		}
		break;
	case SIOCSIFFLAGS:
		IOCTL_DEBUGOUT("ioctl: SIOCSIFFLAGS (Set Interface Flags)");
		IXGBE_CORE_LOCK(adapter);
		if (ifp->if_flags & IFF_UP) {
			if ((ifp->if_drv_flags & IFF_DRV_RUNNING)) {
				if ((ifp->if_flags ^ adapter->if_flags) &
				    (IFF_PROMISC | IFF_ALLMULTI)) {
					ixgbe_set_promisc(adapter);
				}
			} else
				ixgbe_init_locked(adapter);
		} else
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				ixgbe_stop(adapter);
		adapter->if_flags = ifp->if_flags;
		IXGBE_CORE_UNLOCK(adapter);
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		IOCTL_DEBUGOUT("ioctl: SIOC(ADD|DEL)MULTI");
		if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
			IXGBE_CORE_LOCK(adapter);
			ixgbe_disable_intr(adapter, false);
			ixgbe_set_multi(adapter);
			ixgbe_enable_intr(adapter, false);
			IXGBE_CORE_UNLOCK(adapter);
		}
		break;
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		IOCTL_DEBUGOUT("ioctl: SIOCxIFMEDIA (Get/Set Interface Media)");
		error = ifmedia_ioctl(ifp, ifr, &adapter->media, command);
		break;
	case SIOCSIFCAP:
	{
		IOCTL_DEBUGOUT("ioctl: SIOCSIFCAP (Set Capabilities)");

		int mask = ifr->ifr_reqcap ^ ifp->if_capenable;

		if (!mask)
			break;

		/* HW cannot turn these on/off separately */
		if (mask & (IFCAP_RXCSUM | IFCAP_RXCSUM_IPV6)) {
			ifp->if_capenable ^= IFCAP_RXCSUM;
			ifp->if_capenable ^= IFCAP_RXCSUM_IPV6;
		}
		if (mask & IFCAP_TXCSUM)
			ifp->if_capenable ^= IFCAP_TXCSUM;
		if (mask & IFCAP_TXCSUM_IPV6)
			ifp->if_capenable ^= IFCAP_TXCSUM_IPV6;
		if (mask & IFCAP_TSO4)
			ifp->if_capenable ^= IFCAP_TSO4;
		if (mask & IFCAP_TSO6)
			ifp->if_capenable ^= IFCAP_TSO6;
		if (mask & IFCAP_LRO)
			ifp->if_capenable ^= IFCAP_LRO;
		if (mask & IFCAP_VLAN_HWTAGGING)
			ifp->if_capenable ^= IFCAP_VLAN_HWTAGGING;
		if (mask & IFCAP_VLAN_HWFILTER)
			ifp->if_capenable ^= IFCAP_VLAN_HWFILTER;
		if (mask & IFCAP_VLAN_HWTSO)
			ifp->if_capenable ^= IFCAP_VLAN_HWTSO;

		if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
			IXGBE_CORE_LOCK(adapter);
			ixgbe_init_locked(adapter);
			IXGBE_CORE_UNLOCK(adapter);
		}
		VLAN_CAPABILITIES(ifp);
		break;
	}
#if __FreeBSD_version >= 1100036
	case SIOCGI2C:
	{
		struct ixgbe_hw *hw = &adapter->hw;
		struct ifi2creq i2c;
		int i;

		IOCTL_DEBUGOUT("ioctl: SIOCGI2C (Get I2C Data)");
		error = copyin(ifr_data_get_ptr(ifr), &i2c, sizeof(i2c));
		if (error != 0)
			break;
		if (i2c.dev_addr != 0xA0 && i2c.dev_addr != 0xA2) {
			error = EINVAL;
			break;
		}
		if (i2c.len > sizeof(i2c.data)) {
			error = EINVAL;
			break;
		}

		for (i = 0; i < i2c.len; i++)
			hw->phy.ops.read_i2c_byte(hw, i2c.offset + i,
			    i2c.dev_addr, &i2c.data[i]);
		error = copyout(&i2c, ifr_data_get_ptr(ifr), sizeof(i2c));
		break;
	}
#endif
	default:
		IOCTL_DEBUGOUT1("ioctl: UNKNOWN (0x%X)\n", (int)command);
		error = ether_ioctl(ifp, command, data);
		break;
	}

	return (error);
} /* ixgbe_ioctl */

/************************************************************************
 * ixgbe_check_fan_failure
 ************************************************************************/
static void
ixgbe_check_fan_failure(struct adapter *adapter, u32 reg, bool in_interrupt)
{
	u32 mask;

	mask = (in_interrupt) ? IXGBE_EICR_GPI_SDP1_BY_MAC(&adapter->hw) :
	    IXGBE_ESDP_SDP1;

	if (reg & mask)
		device_printf(adapter->dev, "\nCRITICAL: FAN FAILURE!! REPLACE IMMEDIATELY!!\n");
} /* ixgbe_check_fan_failure */

/************************************************************************
 * ixgbe_handle_que
 ************************************************************************/
static void
ixgbe_handle_que(void *context, int pending)
{
	struct ix_queue *que = context;
	struct adapter  *adapter = que->adapter;
	struct tx_ring  *txr = que->txr;
	struct ifnet    *ifp = adapter->ifp;

	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		ixgbe_rxeof(que);
		IXGBE_TX_LOCK(txr);
		ixgbe_txeof(txr);
		if (!ixgbe_ring_empty(ifp, txr->br))
			ixgbe_start_locked(ifp, txr);
		IXGBE_TX_UNLOCK(txr);
	}

	/* Re-enable this interrupt */
	if (que->res != NULL)
		ixgbe_enable_queue(adapter, que->msix);
	else
		ixgbe_enable_intr(adapter, false);

	return;
} /* ixgbe_handle_que */



/************************************************************************
 * ixgbe_allocate_legacy - Setup the Legacy or MSI Interrupt handler
 ************************************************************************/
static int
ixgbe_allocate_legacy(struct adapter *adapter)
{
	device_t        dev = adapter->dev;
	struct ix_queue *que = adapter->queues;
	struct tx_ring  *txr = adapter->tx_rings;
	int             error;

	/* We allocate a single interrupt resource */
	adapter->res = bus_alloc_resource_any(dev, SYS_RES_IRQ,
	    &adapter->link_rid, RF_SHAREABLE | RF_ACTIVE);
	if (adapter->res == NULL) {
		device_printf(dev,
		    "Unable to allocate bus resource: interrupt\n");
		return (ENXIO);
	}

	/*
	 * Try allocating a fast interrupt and the associated deferred
	 * processing contexts.
	 */
	if (!(adapter->feat_en & IXGBE_FEATURE_LEGACY_TX))
		TASK_INIT(&txr->txq_task, 0, ixgbe_deferred_mq_start, txr);
	TASK_INIT(&que->que_task, 0, ixgbe_handle_que, que);
	que->tq = taskqueue_create_fast("ixgbe_que", M_NOWAIT,
	    taskqueue_thread_enqueue, &que->tq);
	taskqueue_start_threads(&que->tq, 1, PI_NET, "%s ixq",
	    device_get_nameunit(adapter->dev));

	if ((error = bus_setup_intr(dev, adapter->res,
	    INTR_TYPE_NET | INTR_MPSAFE, NULL, ixgbe_legacy_irq, que,
	    &adapter->tag)) != 0) {
		device_printf(dev,
		    "Failed to register fast interrupt handler: %d\n", error);
		taskqueue_free(que->tq);
		que->tq = NULL;

		return (error);
	}
	/* For simplicity in the handlers */
	adapter->active_queues = IXGBE_EIMS_ENABLE_MASK;

	return (0);
} /* ixgbe_allocate_legacy */


/************************************************************************
 * ixgbe_allocate_msix - Setup MSI-X Interrupt resources and handlers
 ************************************************************************/
static int
ixgbe_allocate_msix(struct adapter *adapter)
{
	device_t        dev = adapter->dev;
	struct ix_queue *que = adapter->queues;
	struct tx_ring  *txr = adapter->tx_rings;
	int             error, rid, vector = 0;
	int             cpu_id = 0;
	unsigned int    rss_buckets = 0;
	cpuset_t        cpu_mask;

	/*
	 * If we're doing RSS, the number of queues needs to
	 * match the number of RSS buckets that are configured.
	 *
	 * + If there's more queues than RSS buckets, we'll end
	 *   up with queues that get no traffic.
	 *
	 * + If there's more RSS buckets than queues, we'll end
	 *   up having multiple RSS buckets map to the same queue,
	 *   so there'll be some contention.
	 */
	rss_buckets = rss_getnumbuckets();
	if ((adapter->feat_en & IXGBE_FEATURE_RSS) &&
	    (adapter->num_queues != rss_buckets)) {
		device_printf(dev, "%s: number of queues (%d) != number of RSS buckets (%d); performance will be impacted.\n",
		    __func__, adapter->num_queues, rss_buckets);
	}

	for (int i = 0; i < adapter->num_queues; i++, vector++, que++, txr++) {
		rid = vector + 1;
		que->res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
		    RF_SHAREABLE | RF_ACTIVE);
		if (que->res == NULL) {
			device_printf(dev, "Unable to allocate bus resource: que interrupt [%d]\n",
			    vector);
			return (ENXIO);
		}
		/* Set the handler function */
		error = bus_setup_intr(dev, que->res,
		    INTR_TYPE_NET | INTR_MPSAFE, NULL, ixgbe_msix_que, que,
		    &que->tag);
		if (error) {
			que->res = NULL;
			device_printf(dev, "Failed to register QUE handler");
			return (error);
		}
#if __FreeBSD_version >= 800504
		bus_describe_intr(dev, que->res, que->tag, "q%d", i);
#endif
		que->msix = vector;
		adapter->active_queues |= (u64)(1 << que->msix);

		if (adapter->feat_en & IXGBE_FEATURE_RSS) {
			/*
			 * The queue ID is used as the RSS layer bucket ID.
			 * We look up the queue ID -> RSS CPU ID and select
			 * that.
			 */
			cpu_id = rss_getcpu(i % rss_buckets);
			CPU_SETOF(cpu_id, &cpu_mask);
		} else {
			/*
			 * Bind the MSI-X vector, and thus the
			 * rings to the corresponding CPU.
			 *
			 * This just happens to match the default RSS
			 * round-robin bucket -> queue -> CPU allocation.
			 */
			if (adapter->num_queues > 1)
				cpu_id = i;
		}
		if (adapter->num_queues > 1)
			bus_bind_intr(dev, que->res, cpu_id);
#ifdef IXGBE_DEBUG
		if (adapter->feat_en & IXGBE_FEATURE_RSS)
			device_printf(dev, "Bound RSS bucket %d to CPU %d\n", i,
			    cpu_id);
		else
			device_printf(dev, "Bound queue %d to cpu %d\n", i,
			    cpu_id);
#endif /* IXGBE_DEBUG */


		if (!(adapter->feat_en & IXGBE_FEATURE_LEGACY_TX))
			TASK_INIT(&txr->txq_task, 0, ixgbe_deferred_mq_start,
			    txr);
		TASK_INIT(&que->que_task, 0, ixgbe_handle_que, que);
		que->tq = taskqueue_create_fast("ixgbe_que", M_NOWAIT,
		    taskqueue_thread_enqueue, &que->tq);
#if __FreeBSD_version < 1100000
		taskqueue_start_threads(&que->tq, 1, PI_NET, "%s:q%d",
		    device_get_nameunit(adapter->dev), i);
#else
		if (adapter->feat_en & IXGBE_FEATURE_RSS)
			taskqueue_start_threads_cpuset(&que->tq, 1, PI_NET,
			    &cpu_mask, "%s (bucket %d)",
			    device_get_nameunit(adapter->dev), cpu_id);
		else
			taskqueue_start_threads_cpuset(&que->tq, 1, PI_NET,
			    NULL, "%s:q%d", device_get_nameunit(adapter->dev),
			    i);
#endif
	}

	/* and Link */
	adapter->link_rid = vector + 1;
	adapter->res = bus_alloc_resource_any(dev, SYS_RES_IRQ,
	    &adapter->link_rid, RF_SHAREABLE | RF_ACTIVE);
	if (!adapter->res) {
		device_printf(dev,
		    "Unable to allocate bus resource: Link interrupt [%d]\n",
		    adapter->link_rid);
		return (ENXIO);
	}
	/* Set the link handler function */
	error = bus_setup_intr(dev, adapter->res, INTR_TYPE_NET | INTR_MPSAFE,
	    NULL, ixgbe_msix_link, adapter, &adapter->tag);
	if (error) {
		adapter->res = NULL;
		device_printf(dev, "Failed to register LINK handler");
		return (error);
	}
#if __FreeBSD_version >= 800504
	bus_describe_intr(dev, adapter->res, adapter->tag, "link");
#endif
	adapter->vector = vector;
	return (0);
} /* ixgbe_allocate_msix */

/************************************************************************
 * ixgbe_configure_interrupts
 *
 *   Setup MSI-X, MSI, or legacy interrupts (in that order).
 *   This will also depend on user settings.
 ************************************************************************/
static int
ixgbe_configure_interrupts(struct adapter *adapter)
{
	device_t dev = adapter->dev;
	int      rid, want, queues, msgs;

	/* Default to 1 queue if MSI-X setup fails */
	adapter->num_queues = 1;

	/* Override by tuneable */
	if (!(adapter->feat_cap & IXGBE_FEATURE_MSIX))
		goto msi;

	/* First try MSI-X */
	msgs = pci_msix_count(dev);
	if (msgs == 0)
		goto msi;
	rid = PCIR_BAR(MSIX_82598_BAR);
	adapter->msix_mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (adapter->msix_mem == NULL) {
		rid += 4;  /* 82599 maps in higher BAR */
		adapter->msix_mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
		    &rid, RF_ACTIVE);
	}
	if (adapter->msix_mem == NULL) {
		/* May not be enabled */
		device_printf(adapter->dev, "Unable to map MSI-X table.\n");
		goto msi;
	}

	/* Figure out a reasonable auto config value */
	queues = min(mp_ncpus, msgs - 1);
	/* If we're doing RSS, clamp at the number of RSS buckets */
	if (adapter->feat_en & IXGBE_FEATURE_RSS)
		queues = min(queues, rss_getnumbuckets());
	if (ixgbe_num_queues > queues) {
		device_printf(adapter->dev, "ixgbe_num_queues (%d) is too large, using reduced amount (%d).\n", ixgbe_num_queues, queues);
		ixgbe_num_queues = queues;
	}

	if (ixgbe_num_queues != 0)
		queues = ixgbe_num_queues;
	/* Set max queues to 8 when autoconfiguring */
	else
		queues = min(queues, 8);

	/* reflect correct sysctl value */
	ixgbe_num_queues = queues;

	/*
	 * Want one vector (RX/TX pair) per queue
	 * plus an additional for Link.
	 */
	want = queues + 1;
	if (msgs >= want)
		msgs = want;
	else {
		device_printf(adapter->dev, "MSI-X Configuration Problem, %d vectors but %d queues wanted!\n",
		    msgs, want);
		goto msi;
	}
	if ((pci_alloc_msix(dev, &msgs) == 0) && (msgs == want)) {
		device_printf(adapter->dev,
		    "Using MSI-X interrupts with %d vectors\n", msgs);
		adapter->num_queues = queues;
		adapter->feat_en |= IXGBE_FEATURE_MSIX;
		return (0);
	}
	/*
	 * MSI-X allocation failed or provided us with
	 * less vectors than needed. Free MSI-X resources
	 * and we'll try enabling MSI.
	 */
	pci_release_msi(dev);

msi:
	/* Without MSI-X, some features are no longer supported */
	adapter->feat_cap &= ~IXGBE_FEATURE_RSS;
	adapter->feat_en  &= ~IXGBE_FEATURE_RSS;
	adapter->feat_cap &= ~IXGBE_FEATURE_SRIOV;
	adapter->feat_en  &= ~IXGBE_FEATURE_SRIOV;

	if (adapter->msix_mem != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY, rid,
		    adapter->msix_mem);
		adapter->msix_mem = NULL;
	}
	msgs = 1;
	if (pci_alloc_msi(dev, &msgs) == 0) {
		adapter->feat_en |= IXGBE_FEATURE_MSI;
		adapter->link_rid = 1;
		device_printf(adapter->dev, "Using an MSI interrupt\n");
		return (0);
	}

	if (!(adapter->feat_cap & IXGBE_FEATURE_LEGACY_IRQ)) {
		device_printf(adapter->dev,
		    "Device does not support legacy interrupts.\n");
		return 1;
	}

	adapter->feat_en |= IXGBE_FEATURE_LEGACY_IRQ;
	adapter->link_rid = 0;
	device_printf(adapter->dev, "Using a Legacy interrupt\n");

	return (0);
} /* ixgbe_configure_interrupts */


/************************************************************************
 * ixgbe_handle_link - Tasklet for MSI-X Link interrupts
 *
 *   Done outside of interrupt context since the driver might sleep
 ************************************************************************/
static void
ixgbe_handle_link(void *context)
{
	struct adapter  *adapter = context;
	struct ixgbe_hw *hw = &adapter->hw;

	ixgbe_check_link(hw, &adapter->link_speed, &adapter->link_up, 0);

	/* Re-enable link interrupts */
	IXGBE_WRITE_REG(hw, IXGBE_EIMS, IXGBE_EIMS_LSC);
} /* ixgbe_handle_link */

/************************************************************************
 * ixgbe_rearm_queues
 ************************************************************************/
static void
ixgbe_rearm_queues(struct adapter *adapter, u64 queues)
{
	u32 mask;

	switch (adapter->hw.mac.type) {
	case ixgbe_mac_82598EB:
		mask = (IXGBE_EIMS_RTX_QUEUE & queues);
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_EICS, mask);
		break;
	case ixgbe_mac_82599EB:
	case ixgbe_mac_X540:
	case ixgbe_mac_X550:
	case ixgbe_mac_X550EM_x:
	case ixgbe_mac_X550EM_a:
		mask = (queues & 0xFFFFFFFF);
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_EICS_EX(0), mask);
		mask = (queues >> 32);
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_EICS_EX(1), mask);
		break;
	default:
		break;
	}
} /* ixgbe_rearm_queues */

