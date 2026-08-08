/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2010-2016, Intel Corporation
 * Copyright (c) 2026 Kevin Bowling <kbowling@FreeBSD.org>
 */

#include "if_em.h"
#include "if_igb_iov.h"

#ifdef PCI_IOV

#include <sys/iov.h>
#include <sys/sdt.h>
#include <sys/time.h>

#define	IGB_IOV_RAH_POOLSEL_SHIFT	18
#define	IGB_IOV_RAH_POOLSEL_MASK	(0xffU << IGB_IOV_RAH_POOLSEL_SHIFT)
#define	IGB_IOV_MAX_MAC_FILTERS		3
#define	IGB_IOV_MAX_MC_HASHES		30
#define	IGB_IOV_MBX_RETRY_COUNT		6
/* Allow two complete 31-VID replays, then sustain eight additions/second. */
#define	IGB_IOV_VLAN_CHANGE_BURST	64
#define	IGB_IOV_VLAN_CHANGE_INTERVAL	(SBT_1S / 8)
/* 82576 Datasheet rev. 2.0, Section 8.14.16: VMOLR[31] must be one. */
#define	IGB_82576_VMOLR_RSV		(1U << 31)
#define	IGB_82576_LVMMC_BLOCK_MASK	0x1c
#define	IGB_82576_NUM_QUEUES		16
#define	IGB_82576_QUEUE_MASK		0xffff
#define	IGB_82576_STAGGERED_QUEUE_SHIFT	8
#define	IGB_82576_VF_QUEUE_STRIDE	8
#define	IGB_82576_VF_QUEUES		2
#define	IGB_I350_DTXCTL_ENABLE_SPOOF_QUEUE	(1U << 2)
#define	IGB_I350_LVMMC_MAC_VLAN_SPOOF	(1U << 25)
#define	IGB_I350_LVMMC_LAST_Q_SHIFT	29
#define	IGB_I350_LVMMC_LAST_Q_MASK	0x7
#define	IGB_I350_NUM_QUEUES		8
#define	IGB_I350_QUEUE_MASK		0xff
#define	IGB_I350_RESET_ACK_TIMEOUT	(100 * SBT_1MS)
#define	IGB_I350_VF_QUEUES		1
#define	IGB_IOV_QUEUE_DISABLE_BUSY_RETRIES	10
#define	IGB_IOV_QUEUE_DISABLE_DELAY_US	10
#define	IGB_IOV_QUEUE_DISABLE_PAUSE	(100 * SBT_1US)
#define	IGB_IOV_QUEUE_DISABLE_RETRIES	20
#define	IGB_IOV_VF_QUEUES_MAX		2

#define	IGB_VF_CTS			(1U << 0)
#define	IGB_VF_CAP_MAC			(1U << 1)
#define	IGB_VF_ACTIVE			(1U << 2)
#define	IGB_VF_MAC_ANTI_SPOOF		(1U << 3)
#define	IGB_VF_ALLOW_PROMISC		(1U << 4)
#define	IGB_VF_UCAST_PROMISC		(1U << 5)
#define	IGB_VF_MCAST_PROMISC		(1U << 6)
#define	IGB_VF_MCAST_OVERFLOW		(1U << 7)
#define	IGB_VF_MCAST_OVERFLOW_WARNED	(1U << 8)
#define	IGB_VF_MDD_BLOCKED		(1U << 9)
#define	IGB_VF_MBX_PENDING		(1U << 10)
/*
 * After bounded PFU retries, suppress future or overlapping VF requests until
 * RST/VFLR starts a new mailbox epoch.  Intel VF drivers assert CTRL.RST
 * before sending their mailbox reset request.
 */
#define	IGB_VF_MBX_GAVE_UP		(1U << 11)
#define	IGB_VF_MDD_NOTIFY_PENDING	(1U << 12)

struct igb_vf {
	u32	flags;
	struct timeval	last_nack;
	struct timeval	last_mbx_log;
	struct timeval	last_spoof_log;
	struct timeval	last_mdd_log;
	struct timeval	last_queue_log;
	sbintime_t	mbx_retry_at;
	sbintime_t	mdd_notify_at;
	sbintime_t	vlan_token_time;
	u16	pool;
	u16	rar_index;
	u16	max_frame_size;
	u16	mc_count;
	u16	vlan_count;
	u16	vlan_tokens;
	u16	default_vlan;
	u8	mbx_retry_count;
	u8	mac[ETHER_ADDR_LEN];
	u16	mc_hashes[IGB_IOV_MAX_MC_HASHES];
	u32	vlans[EM_VFTA_SIZE];
};

struct igb_vf_mac_filter {
	bool	active;
	u16	pool;
	u16	rar_index;
	u8	mac[ETHER_ADDR_LEN];
};

MALLOC_DEFINE(M_IGB_IOV, "igb_iov", "igb SR-IOV allocations");

/*
 * These logical-write probes let hardware tests verify the elision policy.
 * e1000_write_vfta_i350() expands one VFTA call into ten physical writes, so
 * the probes intentionally count calls made by the rebuild rather than MMIO
 * transactions.  The state probe exposes the final software images while the
 * stack arrays are still live.
 */
SDT_PROVIDER_DEFINE(igb_iov);
SDT_PROBE_DEFINE3(igb_iov, vlan, rebuild, vfta_clear,
    "struct e1000_softc *", "u_int", "uint32_t");
SDT_PROBE_DEFINE3(igb_iov, vlan, rebuild, vlvf_write,
    "struct e1000_softc *", "u_int", "uint32_t");
SDT_PROBE_DEFINE3(igb_iov, vlan, rebuild, vfta_set,
    "struct e1000_softc *", "u_int", "uint32_t");
SDT_PROBE_DEFINE3(igb_iov, vlan, rebuild, state,
    "struct e1000_softc *", "uint32_t *", "uint32_t *");
SDT_PROBE_DEFINE4(igb_iov, mdd, sample, wvbr,
    "struct e1000_softc *", "uint32_t", "uint32_t", "uint32_t");

static const struct timeval igb_iov_nack_interval = { 2, 0 };
static const struct timeval igb_iov_mbx_log_interval = { 2, 0 };
static const struct timeval igb_iov_spoof_log_interval = { 2, 0 };
static const struct timeval igb_iov_mdd_log_interval = { 2, 0 };
static const sbintime_t igb_iov_mdd_notify_retry = SBT_1S / 2;
static const sbintime_t igb_iov_mbx_retry_delay[IGB_IOV_MBX_RETRY_COUNT] = {
	SBT_1MS,
	2 * SBT_1MS,
	4 * SBT_1MS,
	8 * SBT_1MS,
	16 * SBT_1MS,
	32 * SBT_1MS,
};

static void	igb_iov_clear_mac_filters(struct e1000_softc *,
		    const struct igb_vf *);
static bool	igb_iov_mac_in_use(struct e1000_softc *, const u8 *,
		    const struct igb_vf *);
static bool	igb_iov_vlan_present(struct e1000_softc *, u16, bool);
static int	igb_iov_vlan_unique_count(struct e1000_softc *, bool);

static void
igb_iov_mbx_retry_callout(void *arg)
{
	struct e1000_softc *sc;

	sc = arg;
	/*
	 * Mailbox service is serialized by iflib's context lock.  The
	 * callout only re-enters through the ordinary admin task.
	 */
	iflib_admin_intr_deferred(sc->ctx);
}

static u_int
igb_iov_copy_maddr(void *arg, struct sockaddr_dl *sdl, u_int idx)
{
	u8 *mta;

	if (idx == MAX_NUM_MULTICAST_ADDRESSES)
		return (0);
	mta = arg;
	memcpy(&mta[idx * ETHER_ADDR_LEN], LLADDR(sdl), ETHER_ADDR_LEN);
	return (1);
}

static bool
igb_iov_pf_vlan_promisc(struct e1000_softc *sc)
{
	if_t ifp;

	ifp = iflib_get_ifp(sc->ctx);
	return (sc->iov_pf_vlan_promisc ||
	    (if_getflags(ifp) & IFF_PROMISC) != 0);
}

static bool
igb_iov_mac_valid(const u8 *mac)
{
	static const u8 zero[ETHER_ADDR_LEN];

	return (!ETHER_IS_MULTICAST(mac) &&
	    memcmp(mac, zero, ETHER_ADDR_LEN) != 0);
}

static bool
igb_iov_nack_allowed(struct igb_vf *vf)
{
	return (ratecheck(&vf->last_nack, &igb_iov_nack_interval) != 0);
}

static void
igb_iov_reset_vlan_rate(struct igb_vf *vf)
{

	vf->vlan_token_time = getsbinuptime();
	vf->vlan_tokens = IGB_IOV_VLAN_CHANGE_BURST;
}

static bool
igb_iov_vlan_add_allowed(struct igb_vf *vf)
{
	sbintime_t elapsed, now;
	uint64_t refill;

	now = getsbinuptime();
	elapsed = now - vf->vlan_token_time;
	if (elapsed >= IGB_IOV_VLAN_CHANGE_INTERVAL) {
		refill = elapsed / IGB_IOV_VLAN_CHANGE_INTERVAL;
		vf->vlan_tokens = min((uint64_t)IGB_IOV_VLAN_CHANGE_BURST,
		    vf->vlan_tokens + refill);
		vf->vlan_token_time = now;
	}
	if (vf->vlan_tokens == 0)
		return (false);
	vf->vlan_tokens--;
	return (true);
}

static u32
igb_iov_reply_header(u32 request, bool cts, bool ack)
{
	u32 reply, type;

	type = request & 0xffff;
	if (type == E1000_VF_SET_MAC_ADDR &&
	    (request & E1000_VT_MSGINFO_MASK) != 0)
		reply = request;
	else
		reply = type;
	reply &= ~(E1000_VT_MSGTYPE_ACK | E1000_VT_MSGTYPE_NACK |
	    E1000_VT_MSGTYPE_CTS);
	if (cts)
		reply |= E1000_VT_MSGTYPE_CTS;
	reply |= ack ? E1000_VT_MSGTYPE_ACK : E1000_VT_MSGTYPE_NACK;
	return (reply);
}

bool
igb_iov_supported(const struct e1000_softc *sc)
{
	switch (sc->hw.mac.type) {
	case e1000_82576:
	case e1000_i350:
		return (true);
	default:
		return (false);
	}
}

bool
igb_iov_enabled(const struct e1000_softc *sc)
{
	return (sc->num_vfs != 0);
}

int
igb_iov_attach(struct e1000_softc *sc)
{
	nvlist_t *pf_schema, *vf_schema;
	int error, iov_pos;

	if (!igb_iov_supported(sc))
		return (0);
	if (pci_find_extcap(sc->dev, PCIZ_SRIOV, &iov_pos) != 0)
		return (0);

	pf_schema = pci_iov_schema_alloc_node();
	vf_schema = pci_iov_schema_alloc_node();
	pci_iov_schema_add_unicast_mac(vf_schema, "mac-addr", 0, NULL);
	pci_iov_schema_add_bool(vf_schema, "mac-anti-spoof",
	    IOV_SCHEMA_HASDEFAULT, true);
	pci_iov_schema_add_bool(vf_schema, "allow-set-mac",
	    IOV_SCHEMA_HASDEFAULT, false);
	pci_iov_schema_add_bool(vf_schema, "allow-promisc",
	    IOV_SCHEMA_HASDEFAULT, false);
	pci_iov_schema_add_vlan(vf_schema, "vlan", IOV_SCHEMA_HASDEFAULT,
	    VF_VLAN_TRUNK);

	error = pci_iov_attach(sc->dev, pf_schema, vf_schema);
	if (error != 0)
		device_printf(sc->dev,
		    "failed to attach SR-IOV configuration interface: %d\n",
		    error);
	else {
		callout_init(&sc->iov_mbx_retry, 1);
		sc->iov_mbx_retry_initialized = true;
	}
	return (error);
}

void
igb_iov_detach(struct e1000_softc *sc)
{

	if (!sc->iov_mbx_retry_initialized)
		return;
	callout_drain(&sc->iov_mbx_retry);
	sc->iov_mbx_retry_initialized = false;
}

static u32
igb_iov_active_mask(struct e1000_softc *sc)
{
	u32 mask;
	int i;

	mask = 0;
	for (i = 0; i < sc->num_vfs; i++)
		if (sc->vfs[i].flags & IGB_VF_ACTIVE)
			mask |= 1U << i;
	return (mask);
}

static void
igb_iov_map_rar(struct e1000_softc *sc, u16 rar, const u8 *mac, u16 pool)
{
	struct e1000_hw *hw;
	u32 rah;

	hw = &sc->hw;
	e1000_rar_set(hw, __DECONST(u8 *, mac), rar);
	rah = E1000_READ_REG(hw, E1000_RAH(rar));
	rah &= ~IGB_IOV_RAH_POOLSEL_MASK;
	rah |= 1U << (IGB_IOV_RAH_POOLSEL_SHIFT + pool);
	E1000_WRITE_REG(hw, E1000_RAH(rar), rah);
}

static void
igb_iov_clear_rar(struct e1000_softc *sc, u16 rar)
{
	u8 zero[ETHER_ADDR_LEN] = {};

	e1000_rar_set(&sc->hw, zero, rar);
}

static void
igb_iov_clear_mac_filters(struct e1000_softc *sc, const struct igb_vf *vf)
{
	struct igb_vf_mac_filter *filter;
	int i;

	for (i = 0; i < sc->num_vf_mac_filters; i++) {
		filter = &sc->vf_mac_filters[i];
		if (!filter->active || filter->pool != vf->pool)
			continue;
		igb_iov_clear_rar(sc, filter->rar_index);
		filter->active = false;
		memset(filter->mac, 0, sizeof(filter->mac));
	}
}

static u32
igb_iov_switch_reg(struct e1000_softc *sc)
{
	return (sc->hw.mac.type == e1000_82576 ?
	    E1000_DTXSWC : E1000_TXSWC);
}

static void
igb_iov_set_anti_spoof(struct e1000_softc *sc, struct igb_vf *vf)
{
	struct e1000_hw *hw;
	u32 reg, value;

	hw = &sc->hw;
	reg = igb_iov_switch_reg(sc);
	value = E1000_READ_REG(hw, reg);
	value &= ~((1U << vf->pool) |
	    (1U << (vf->pool + E1000_DTXSWC_VLAN_SPOOF_SHIFT)));
	if (vf->flags & IGB_VF_MAC_ANTI_SPOOF)
		value |= 1U << vf->pool;
	if (vf->flags & IGB_VF_ACTIVE)
		value |= 1U <<
		    (vf->pool + E1000_DTXSWC_VLAN_SPOOF_SHIFT);
	E1000_WRITE_REG(hw, reg, value);
}

static void
igb_iov_set_uta(struct e1000_softc *sc)
{
	struct e1000_hw *hw;
	bool enable;
	int i;

	if (!igb_iov_enabled(sc) || sc->hw.mac.type != e1000_82576)
		return;

	hw = &sc->hw;
	enable = (E1000_READ_REG(hw, E1000_VMOLR(sc->pool)) &
	    E1000_VMOLR_ROPE) != 0;
	for (i = 0; i < sc->num_vfs; i++)
		if ((sc->vfs[i].flags &
		    (IGB_VF_ACTIVE | IGB_VF_UCAST_PROMISC)) ==
		    (IGB_VF_ACTIVE | IGB_VF_UCAST_PROMISC)) {
			enable = true;
			break;
		}

	for (i = 0; i < MAX_MTA_REG; i++)
		E1000_WRITE_REG_ARRAY(hw, E1000_UTA, i,
		    enable ? 0xffffffffU : 0);
}

static void
igb_iov_configure_dvmolr(struct e1000_softc *sc, u16 pool,
    bool strip_vlan, bool hide_vlan, bool vf_pool)
{
	struct e1000_hw *hw;
	u32 dvmolr;

	hw = &sc->hw;
	if (hw->mac.type != e1000_i350)
		return;

	dvmolr = E1000_READ_REG(hw, E1000_DVMOLR(pool));
	dvmolr &= ~(E1000_DVMOLR_HIDVLAN | E1000_DVMOLR_STRVLAN |
	    E1000_DVMOLR_STRCRC);
	if (hide_vlan)
		dvmolr |= E1000_DVMOLR_HIDVLAN;
	if (strip_vlan)
		dvmolr |= E1000_DVMOLR_STRVLAN;
	if (vf_pool || strip_vlan ||
	    (E1000_READ_REG(hw, E1000_RCTL) & E1000_RCTL_SECRC) != 0)
		dvmolr |= E1000_DVMOLR_STRCRC;
	E1000_WRITE_REG(hw, E1000_DVMOLR(pool), dvmolr);
}

static void
igb_iov_configure_vmolr(struct e1000_softc *sc, struct igb_vf *vf)
{
	struct e1000_hw *hw;
	u32 max_frame_size, vmolr, vmvir;

	hw = &sc->hw;
	max_frame_size = vf->max_frame_size;
	if (vf->vlan_count != 0)
		max_frame_size = min(max_frame_size + VLAN_TAG_SIZE,
		    IGB_IOV_MAX_FRAME_SIZE);
	vmolr = E1000_READ_REG(hw, E1000_VMOLR(vf->pool));
	vmolr &= ~(E1000_VMOLR_RLPML_MASK | E1000_VMOLR_RSSE |
	    E1000_VMOLR_VPE | E1000_VMOLR_UPE | E1000_VMOLR_ROMPE |
	    E1000_VMOLR_ROPE | E1000_VMOLR_MPME | E1000_VMOLR_STRVLAN);
	vmolr |= E1000_VMOLR_BAM | E1000_VMOLR_LPE |
	    (max_frame_size & E1000_VMOLR_RLPML_MASK);
	if (vf->default_vlan == 0)
		vmolr |= E1000_VMOLR_AUPE;
	if (vf->mc_count != 0 &&
	    (vf->flags & (IGB_VF_MCAST_PROMISC |
	    IGB_VF_MCAST_OVERFLOW)) == 0)
		vmolr |= E1000_VMOLR_ROMPE;
	if (hw->mac.type == e1000_82576)
		vmolr |= IGB_82576_VMOLR_RSV;

	if (vf->flags & IGB_VF_UCAST_PROMISC) {
		if (hw->mac.type == e1000_82576)
			vmolr |= E1000_VMOLR_ROPE;
		else
			vmolr |= E1000_VMOLR_UPE;
	}
	/*
	 * The mailbox can describe only 30 hashes.  Fall back to receiving all
	 * multicast within the VF's VLAN membership when that list overflows.
	 */
	if ((vf->flags & (IGB_VF_MCAST_PROMISC |
	    IGB_VF_MCAST_OVERFLOW)) != 0)
		vmolr |= E1000_VMOLR_MPME;
	if (hw->mac.type == e1000_82576 && vf->vlan_count != 0)
		vmolr |= E1000_VMOLR_STRVLAN;
	/* A nonzero default VLAN makes this VF an untagged access port. */
	if (vf->default_vlan == 0)
		vmvir = 0;
	else
		vmvir = vf->default_vlan | E1000_VMVIR_VLANA_DEFAULT;

	E1000_WRITE_REG(hw, E1000_VMOLR(vf->pool), vmolr);
	E1000_WRITE_REG(hw, E1000_VMVIR(vf->pool), vmvir);
	igb_iov_configure_dvmolr(sc, vf->pool, vf->vlan_count != 0,
	    vf->default_vlan != 0, true);
}

static void
igb_iov_configure_pf_vmolr(struct e1000_softc *sc)
{
	struct e1000_hw *hw;
	if_t ifp;
	bool strip_vlan;
	u32 max_frame_size;
	u32 old_vmolr, vmolr;

	hw = &sc->hw;
	ifp = iflib_get_ifp(sc->ctx);
	max_frame_size = min(sc->shared->isc_max_frame_size + VLAN_TAG_SIZE,
	    IGB_IOV_MAX_FRAME_SIZE);
	strip_vlan = (E1000_READ_REG(hw, E1000_CTRL) & E1000_CTRL_VME) != 0;
	old_vmolr = E1000_READ_REG(hw, E1000_VMOLR(sc->pool));
	vmolr = E1000_VMOLR_BAM | E1000_VMOLR_AUPE |
	    E1000_VMOLR_LPE |
	    (max_frame_size & E1000_VMOLR_RLPML_MASK);
	if (hw->mac.type == e1000_82576) {
		vmolr |= IGB_82576_VMOLR_RSV;
		if (strip_vlan)
			vmolr |= E1000_VMOLR_STRVLAN;
	} else
		vmolr |= old_vmolr & E1000_VMOLR_VPE;

	if (if_getflags(ifp) & IFF_PROMISC) {
		if (hw->mac.type == e1000_82576)
			vmolr |= E1000_VMOLR_ROPE;
		else
			vmolr |= E1000_VMOLR_UPE | E1000_VMOLR_VPE;
		vmolr |= E1000_VMOLR_MPME;
	} else if ((if_getflags(ifp) & IFF_ALLMULTI) != 0 ||
	    if_llmaddr_count(ifp) >= MAX_NUM_MULTICAST_ADDRESSES)
		vmolr |= E1000_VMOLR_MPME;
	else if (if_llmaddr_count(ifp) != 0)
		vmolr |= E1000_VMOLR_ROMPE;

	E1000_WRITE_REG(hw, E1000_VMOLR(sc->pool), vmolr);
	igb_iov_configure_dvmolr(sc, sc->pool, strip_vlan, false, false);
}

void
igb_iov_update_pf_vmolr(struct e1000_softc *sc)
{
	if (!igb_iov_enabled(sc))
		return;

	igb_iov_configure_pf_vmolr(sc);
	igb_iov_set_uta(sc);
}

u32
igb_iov_intr_mask(const struct e1000_softc *sc)
{
	if (!sc->iov_hw_active)
		return (0);
	return (E1000_IMS_VMMB | E1000_IMS_MDDET);
}

void
igb_iov_intr_drain_stale(struct e1000_softc *sc)
{
	struct e1000_hw *hw;
	u32 icr;

	if (atomic_readandclear_32(&sc->iov_intr_drain_pending) == 0)
		return;
	hw = &sc->hw;
	/*
	 * Consume setup-time diagnostic state at the actual transition from
	 * masked to armed.  Read ICR last so an event arriving after the drain
	 * remains pending and is delivered when the caller enables MDDET.
	 */
	(void)E1000_READ_REG(hw, E1000_LVMMC);
	if (hw->mac.type == e1000_82576)
		(void)E1000_READ_REG(hw, E1000_WVBR);
	icr = E1000_READ_REG(hw, E1000_ICR);
	/*
	 * em_if_init() injects LSC after IOV setup to close the post-reset
	 * link race.  Preserve that cause across this MDDET-specific drain.
	 */
	if (__predict_true(icr != 0xffffffff) &&
	    (icr & E1000_ICR_LSC) != 0)
		E1000_WRITE_REG(hw, E1000_ICS, E1000_ICS_LSC);
}

static void
igb_iov_vfta_shadow_invalidate(struct e1000_softc *sc)
{

	/*
	 * I350 erratum 20 makes VFTA reads unreliable while VMDq loopback or
	 * anti-spoofing is active.  The shadow is therefore authoritative
	 * until a reset or another independent hardware writer invalidates
	 * it.  Readback cannot reliably audit a stale-but-valid shadow on
	 * this part, so keep all shadow mutation in these two helpers.
	 */
	memset(sc->iov_vfta, 0, sizeof(sc->iov_vfta));
	sc->iov_vfta_valid = false;
}

static void
igb_iov_vfta_shadow_store(struct e1000_softc *sc, const u32 *vfta)
{

	memcpy(sc->iov_vfta, vfta, sizeof(sc->iov_vfta));
	sc->iov_vfta_valid = true;
}

static void
igb_iov_notify_vfs_reset(struct e1000_softc *sc)
{
	struct igb_vf *vf;
	struct e1000_hw *hw;
	sbintime_t deadline;
	u32 msg, pending, undelivered;
	int i;

	hw = &sc->hw;
	/*
	 * Process VFLRs first and wait only for VFs that completed their
	 * mailbox handshake.  An unattached VF has nobody who can acknowledge.
	 */
	igb_iov_handle_mbx(sc);
	pending = 0;
	for (i = 0; i < sc->num_vfs; i++) {
		vf = &sc->vfs[i];
		if ((vf->flags & (IGB_VF_ACTIVE | IGB_VF_CTS)) ==
		    (IGB_VF_ACTIVE | IGB_VF_CTS))
			pending |= 1U << i;
	}
	if (pending == 0)
		return;

	/*
	 * I350 SDM section 4.6.11.2.3 requires each VF to acknowledge a
	 * mailbox warning before the PF asserts CTRL.RST.
	 *
	 * The mailbox pass above drained requests and stale acknowledgements.
	 * A VF read of the new notification sets its ACK bit.
	 */
	undelivered = 0;
	for (i = 0; i < sc->num_vfs; i++) {
		if ((pending & (1U << i)) == 0)
			continue;
		msg = E1000_PF_CONTROL_MSG;
		if (e1000_write_mbx(hw, &msg, 1, i) != 0) {
			undelivered |= 1U << i;
			pending &= ~(1U << i);
		}
	}
	if (undelivered != 0)
		device_printf(sc->dev,
		    "could not deliver reset warning to VF mask %#x\n",
		    undelivered);

	deadline = getsbinuptime() + IGB_I350_RESET_ACK_TIMEOUT;
	while (pending != 0 && getsbinuptime() < deadline) {
		for (i = 0; i < sc->num_vfs; i++) {
			if ((pending & (1U << i)) != 0 &&
			    e1000_check_for_ack(hw, i) == 0)
				pending &= ~(1U << i);
		}
		if (pending != 0)
			pause_sbt("igback", SBT_1MS, 0, C_HARDCLOCK);
	}
	if (pending != 0)
		device_printf(sc->dev,
		    "VF reset acknowledgement timed out for mask %#x\n",
		    pending);
}

void
igb_iov_reset_prepare(struct e1000_softc *sc)
{
	struct e1000_hw *hw;
	u32 mask;

	if (sc->iov_hw_active) {
		hw = &sc->hw;
		if (atomic_load_acq_32(&sc->iov_teardown) == 0) {
			if (hw->mac.type == e1000_i350)
				igb_iov_notify_vfs_reset(sc);
			else
				igb_iov_ping_all_vfs(sc);
		}

		/* Stop VF DMA before the PF asserts CTRL.RST. */
		mask = 1U << sc->pool;
		E1000_WRITE_REG(hw, E1000_VFRE, mask);
		E1000_WRITE_REG(hw, E1000_VFTE, mask);
		E1000_WRITE_FLUSH(hw);
	}
	sc->iov_hw_active = false;
	if (sc->iov_mbx_retry_initialized)
		callout_stop(&sc->iov_mbx_retry);
	sc->iov_mta_valid = false;
	igb_iov_vfta_shadow_invalidate(sc);
	atomic_readandclear_32(&sc->iov_mdd_cause);
	atomic_readandclear_32(&sc->iov_pending);
	atomic_readandclear_32(&sc->iov_spoof_pending);
	atomic_readandclear_32(&sc->iov_blocked_pending);
	/*
	 * Normal iflib initialization prepares the reset before
	 * igb_iov_initialize() requests this drain.  Preserve a still-pending
	 * I350 request across a later stop or repeated preparation so the next
	 * interrupt arm consumes it.  Other families retain the ordinary
	 * stop-time cleanup.
	 */
	if (sc->hw.mac.type != e1000_i350)
		atomic_readandclear_32(&sc->iov_intr_drain_pending);
}

void
igb_iov_rebuild_mta(struct e1000_softc *sc)
{
	struct e1000_hw *hw;
	struct igb_vf *vf;
	u32 hash_bit, hash_reg, hash_value;
	u32 mta[MAX_MTA_REG] = {};
	u16 hash;
	bool changed;
	int i, j, mcnt;

	if (!igb_iov_enabled(sc))
		return;

	hw = &sc->hw;
	memset(sc->mta, 0,
	    ETHER_ADDR_LEN * MAX_NUM_MULTICAST_ADDRESSES);
	mcnt = if_foreach_llmaddr(iflib_get_ifp(sc->ctx),
	    igb_iov_copy_maddr, sc->mta);
	mcnt = min(mcnt, MAX_NUM_MULTICAST_ADDRESSES);
	for (i = 0; i < mcnt; i++) {
		hash_value = e1000_hash_mc_addr(hw,
		    &sc->mta[i * ETHER_ADDR_LEN]);
		hash_reg = (hash_value >> 5) &
		    (hw->mac.mta_reg_count - 1);
		hash_bit = hash_value & 0x1f;
		mta[hash_reg] |= 1U << hash_bit;
	}
	for (i = 0; i < sc->num_vfs; i++) {
		vf = &sc->vfs[i];
		if (!(vf->flags & IGB_VF_ACTIVE))
			continue;
		for (j = 0; j < vf->mc_count; j++) {
			hash = vf->mc_hashes[j] & 0xfff;
			mta[(hash >> 5) & (hw->mac.mta_reg_count - 1)] |=
			    1U << (hash & 0x1f);
		}
	}

	changed = false;
	for (i = hw->mac.mta_reg_count - 1; i >= 0; i--) {
		if (sc->iov_mta_valid && hw->mac.mta_shadow[i] == mta[i])
			continue;
		hw->mac.mta_shadow[i] = mta[i];
		E1000_WRITE_REG_ARRAY(hw, E1000_MTA, i, mta[i]);
		changed = true;
	}
	if (changed)
		E1000_WRITE_FLUSH(hw);
	sc->iov_mta_valid = true;
}

static int
igb_iov_vlvf_add(u32 *vlvf, const u32 *old_vlvf, u16 vid, u16 pool,
    bool preserve_only)
{
	int free_slot, i;

	free_slot = -1;
	for (i = 0; i < E1000_VLVF_ARRAY_SIZE; i++) {
		if ((vlvf[i] & E1000_VLVF_VLANID_ENABLE) != 0 &&
		    (vlvf[i] & E1000_VLVF_VLANID_MASK) == vid) {
			vlvf[i] |= 1U << (E1000_VLVF_POOLSEL_SHIFT + pool);
			return (0);
		}
		if (free_slot == -1 &&
		    (vlvf[i] & E1000_VLVF_VLANID_ENABLE) == 0)
			free_slot = i;
	}
	for (i = 0; i < E1000_VLVF_ARRAY_SIZE; i++)
		if ((old_vlvf[i] & E1000_VLVF_VLANID_ENABLE) != 0 &&
		    (old_vlvf[i] & E1000_VLVF_VLANID_MASK) == vid &&
		    (vlvf[i] & E1000_VLVF_VLANID_ENABLE) == 0) {
			free_slot = i;
			break;
		}
	if (preserve_only && (i == E1000_VLVF_ARRAY_SIZE))
		return (ENOENT);
	if (free_slot == -1)
		return (ENOSPC);

	vlvf[free_slot] = E1000_VLVF_VLANID_ENABLE | vid |
	    (1U << (E1000_VLVF_POOLSEL_SHIFT + pool));
	return (0);
}

void
igb_iov_rebuild_vlan(struct e1000_softc *sc)
{
	struct e1000_hw *hw;
	struct igb_vf *vf;
	u32 old_vlvf[E1000_VLVF_ARRAY_SIZE];
	u32 effective_vfta[EM_VFTA_SIZE], vfta[EM_VFTA_SIZE];
	u32 vlvf[E1000_VLVF_ARRAY_SIZE];
	u32 old_vfta, rctl, vmolr;
	bool force_vfta, pf_overflow, pf_vlan_promisc, preserve_pf;
	bool vfta_changed, vlvf_changed;
	int i, vid;

	if (!igb_iov_enabled(sc))
		return;

	hw = &sc->hw;
	rctl = E1000_READ_REG(hw, E1000_RCTL);
	rctl &= ~E1000_RCTL_CFIEN;
	rctl |= E1000_RCTL_VFE;
	E1000_WRITE_REG(hw, E1000_RCTL, rctl);
	memcpy(vfta, sc->shadow_vfta, sizeof(vfta));
	memset(vlvf, 0, sizeof(vlvf));
	for (i = 0; i < E1000_VLVF_ARRAY_SIZE; i++)
		old_vlvf[i] = E1000_READ_REG(hw, E1000_VLVF(i));

	pf_vlan_promisc = igb_iov_pf_vlan_promisc(sc);
	pf_overflow = !pf_vlan_promisc && hw->mac.type == e1000_i350 &&
	    igb_iov_vlan_unique_count(sc, true) > E1000_VLVF_ARRAY_SIZE;
	preserve_pf = !pf_vlan_promisc && !pf_overflow;

	/* First keep every surviving VF mapping in its current slot. */
	for (i = 0; i < sc->num_vfs; i++) {
		vf = &sc->vfs[i];
		if (!(vf->flags & IGB_VF_ACTIVE))
			continue;
		for (vid = 0; vid < 4096; vid++) {
			if ((vf->vlans[vid >> 5] & (1U << (vid & 0x1f))) ==
			    0)
				continue;
			(void)igb_iov_vlvf_add(vlvf, old_vlvf, vid,
			    vf->pool, true);
		}
	}

	/*
	 * Preserve PF mappings unless I350 needs their slots for VFs.
	 * PF-only VLANs on 82576 intentionally have no VLVF mapping and
	 * reach the default PF pool after passing the global VFTA.
	 */
	if (preserve_pf)
		for (vid = 0; vid < 4096; vid++) {
			if ((sc->shadow_vfta[vid >> 5] &
			    (1U << (vid & 0x1f))) == 0)
				continue;
			if (hw->mac.type == e1000_82576 &&
			    !igb_iov_vlan_present(sc, vid, false))
				continue;
			(void)igb_iov_vlvf_add(vlvf, old_vlvf, vid,
			    sc->pool, true);
		}

	/* Allocate new VF mappings before PF mappings. */
	for (i = 0; i < sc->num_vfs; i++) {
		vf = &sc->vfs[i];
		if (!(vf->flags & IGB_VF_ACTIVE))
			continue;
		for (vid = 0; vid < 4096; vid++) {
			if ((vf->vlans[vid >> 5] & (1U << (vid & 0x1f))) ==
			    0)
				continue;
			if (igb_iov_vlvf_add(vlvf, old_vlvf, vid,
			    vf->pool, false) == 0)
				vfta[vid >> 5] |= 1U << (vid & 0x1f);
		}
		igb_iov_configure_vmolr(sc, vf);
	}
	if (!pf_vlan_promisc)
		for (vid = 0; vid < 4096; vid++) {
			if ((sc->shadow_vfta[vid >> 5] &
			    (1U << (vid & 0x1f))) == 0)
				continue;
			/*
			 * With no VLVF match, 82576 sends a globally admitted
			 * VLAN to the default PF pool.  A VLVF entry is needed
			 * only when this VLAN is also assigned to a VF.
			 */
			if (hw->mac.type == e1000_82576 &&
			    !igb_iov_vlan_present(sc, vid, false))
				continue;
			if (igb_iov_vlvf_add(vlvf, old_vlvf, vid,
			    sc->pool, false) != 0)
				pf_overflow = true;
		}

	if (pf_vlan_promisc) {
		memset(vfta, 0xff, sizeof(vfta));
		for (i = 0; i < E1000_VLVF_ARRAY_SIZE; i++)
			if ((vlvf[i] & E1000_VLVF_VLANID_ENABLE) != 0)
				vlvf[i] |= 1U <<
				    (E1000_VLVF_POOLSEL_SHIFT + sc->pool);
	}

	/*
	 * Establish the PF fallback before an overflowing I350 rebuild can
	 * displace one of its old VLVF mappings.
	 */
	vmolr = E1000_READ_REG(hw, E1000_VMOLR(sc->pool));
	vmolr &= ~E1000_VMOLR_VPE;
	if (hw->mac.type == e1000_i350 &&
	    (pf_overflow || pf_vlan_promisc))
		vmolr |= E1000_VMOLR_VPE;
	E1000_WRITE_REG(hw, E1000_VMOLR(sc->pool), vmolr);

	/*
	 * Remove global VFTA membership before removing a VLAN entirely, and
	 * add a VLVF mapping before globally admitting a new VF VLAN.  A
	 * transition to a PF-only VLAN deliberately retains VFTA membership
	 * and falls through to the default PF pool.
	 */
	force_vfta = hw->mac.type == e1000_i350 &&
	    !sc->iov_vfta_valid;
	vfta_changed = false;
	for (i = 0; i < EM_VFTA_SIZE; i++) {
		/*
		 * I350 erratum 20 makes VFTA reads unreliable while VMDq
		 * loopback or anti-spoofing is active.  Its ten-write
		 * workaround is already in e1000_write_vfta_i350().  Force a
		 * complete clear when the authoritative shadow is invalid;
		 * 82576 can safely diff against its live register contents.
		 */
		if (hw->mac.type == e1000_i350)
			old_vfta = force_vfta ? 0 : sc->iov_vfta[i];
		else
			old_vfta =
			    E1000_READ_REG_ARRAY(hw, E1000_VFTA, i);
		effective_vfta[i] = old_vfta & vfta[i];
		if (force_vfta || effective_vfta[i] != old_vfta) {
			SDT_PROBE3(igb_iov, vlan, rebuild, vfta_clear,
			    sc, i, effective_vfta[i]);
			e1000_write_vfta(hw, i, effective_vfta[i]);
			vfta_changed = true;
		}
	}
	if (vfta_changed)
		E1000_WRITE_FLUSH(hw);
	vlvf_changed = false;
	for (i = 0; i < E1000_VLVF_ARRAY_SIZE; i++)
		if (vlvf[i] != old_vlvf[i]) {
			SDT_PROBE3(igb_iov, vlan, rebuild, vlvf_write,
			    sc, i, vlvf[i]);
			E1000_WRITE_REG(hw, E1000_VLVF(i), vlvf[i]);
			vlvf_changed = true;
		}
	if (vlvf_changed)
		E1000_WRITE_FLUSH(hw);
	vfta_changed = false;
	for (i = 0; i < EM_VFTA_SIZE; i++)
		if (vfta[i] != effective_vfta[i]) {
			SDT_PROBE3(igb_iov, vlan, rebuild, vfta_set,
			    sc, i, vfta[i]);
			e1000_write_vfta(hw, i, vfta[i]);
			vfta_changed = true;
		}
	if (vfta_changed)
		E1000_WRITE_FLUSH(hw);
	SDT_PROBE3(igb_iov, vlan, rebuild, state, sc, vfta, vlvf);
	igb_iov_vfta_shadow_store(sc, vfta);
}

static bool
igb_iov_vlan_present(struct e1000_softc *sc, u16 vid, bool include_pf)
{
	int i;

	if (include_pf &&
	    (sc->shadow_vfta[vid >> 5] & (1U << (vid & 0x1f))) != 0)
		return (true);
	for (i = 0; i < sc->num_vfs; i++)
		if ((sc->vfs[i].flags & IGB_VF_ACTIVE) != 0 &&
		    (sc->vfs[i].vlans[vid >> 5] &
		    (1U << (vid & 0x1f))) != 0)
			return (true);
	return (false);
}

static int
igb_iov_vlan_unique_count(struct e1000_softc *sc, bool include_pf)
{
	u32 vlans;
	int count, i, word;

	count = 0;
	for (word = 0; word < EM_VFTA_SIZE; word++) {
		vlans = include_pf ? sc->shadow_vfta[word] : 0;
		for (i = 0; i < sc->num_vfs; i++)
			if ((sc->vfs[i].flags & IGB_VF_ACTIVE) != 0)
				vlans |= sc->vfs[i].vlans[word];
		count += bitcount32(vlans);
	}
	return (count);
}

static int
igb_iov_set_vlan(struct e1000_softc *sc, struct igb_vf *vf, u16 vid,
    bool add)
{
	u32 bit;
	bool present;

	bit = 1U << (vid & 0x1f);
	present = (vf->vlans[vid >> 5] & bit) != 0;
	if (vid == 0) {
		if (!present) {
			vf->vlans[0] |= 1U;
			igb_iov_rebuild_vlan(sc);
		}
		return (0);
	}
	if (add == present)
		return (0);

	/*
	 * Removals always reduce privilege and remain available.  Charge only
	 * additions, which a hostile VF must alternate with removals to force
	 * repeated global VLAN rebuilds.
	 */
	if (add && !igb_iov_vlan_present(sc, vid, false) &&
	    igb_iov_vlan_unique_count(sc, false) >=
	    E1000_VLVF_ARRAY_SIZE)
		return (ENOSPC);
	if (add && !igb_iov_vlan_add_allowed(vf))
		return (EBUSY);

	if (add) {
		vf->vlans[vid >> 5] |= bit;
		vf->vlan_count++;
	} else {
		vf->vlans[vid >> 5] &= ~bit;
		vf->vlan_count--;
	}
	igb_iov_rebuild_vlan(sc);
	return (0);
}

static void
igb_iov_reset_vf_state(struct e1000_softc *sc, struct igb_vf *vf)
{
	bool update_uta;

	update_uta = (vf->flags & IGB_VF_UCAST_PROMISC) != 0;
	vf->flags &= ~(IGB_VF_CTS | IGB_VF_UCAST_PROMISC |
	    IGB_VF_MCAST_PROMISC | IGB_VF_MCAST_OVERFLOW |
	    IGB_VF_MBX_PENDING | IGB_VF_MBX_GAVE_UP |
	    IGB_VF_MDD_NOTIFY_PENDING);
	vf->mbx_retry_at = 0;
	vf->mdd_notify_at = 0;
	vf->mbx_retry_count = 0;
	/*
	 * A reset starts a new mailbox epoch.  Permit one immediate NACK so a
	 * premature non-reset request does not wait for its posted-read
	 * timeout.
	 */
	memset(&vf->last_nack, 0, sizeof(vf->last_nack));
	vf->max_frame_size = ETHER_MAX_LEN;
	vf->mc_count = 0;
	vf->vlan_count = 0;
	memset(vf->mc_hashes, 0, sizeof(vf->mc_hashes));
	memset(vf->vlans, 0, sizeof(vf->vlans));
	/* Preserve the administrative access VLAN across VF and PF resets. */
	if (vf->default_vlan == 0)
		vf->vlans[0] = 1U;
	else {
		vf->vlans[vf->default_vlan >> 5] =
		    1U << (vf->default_vlan & 0x1f);
		vf->vlan_count = 1;
	}
	igb_iov_configure_vmolr(sc, vf);
	if (update_uta)
		igb_iov_set_uta(sc);
}

static bool
igb_iov_vf_vlan_is_default(const struct igb_vf *vf)
{
	u32 expected;
	int i;

	for (i = 0; i < EM_VFTA_SIZE; i++) {
		expected = 0;
		if (i == vf->default_vlan >> 5)
			expected = 1U << (vf->default_vlan & 0x1f);
		if (vf->vlans[i] != expected)
			return (false);
	}
	return (true);
}

static bool
igb_iov_sanitize_vf_queues(struct e1000_softc *sc,
    struct igb_vf *vf)
{
	struct e1000_hw *hw;
	u16 qid[IGB_IOV_VF_QUEUES_MAX];
	u32 rxdctl, txdctl;
	int i, nqueues, retry;

	hw = &sc->hw;
	switch (hw->mac.type) {
	case e1000_82576:
		nqueues = IGB_82576_VF_QUEUES;
		qid[0] = vf->pool;
		qid[1] = vf->pool + IGB_82576_VF_QUEUE_STRIDE;
		break;
	case e1000_i350:
		nqueues = IGB_I350_VF_QUEUES;
		qid[0] = vf->pool;
		break;
	default:
		return (true);
	}

	/*
	 * I350 maps pool n to queue n.  82576 gives VF n physical queues n
	 * and n + 8, so both retained queue configurations must be cleared.
	 */
	for (i = 0; i < nqueues; i++)
		KASSERT(qid[i] < (hw->mac.type == e1000_82576 ?
		    IGB_82576_NUM_QUEUES : IGB_I350_NUM_QUEUES),
		    ("%s: invalid VF queue %u", __func__, qid[i]));

	/*
	 * The 82576 and I350 specification updates, Software Clarification 3,
	 * note that VFLR does not reset the VF queue configuration.  Clear the
	 * PF-programmable state before acknowledging the reset so a new VF
	 * owner cannot inherit it, particularly a descriptor-head write-back
	 * DMA address.  The new VF driver initializes its active ring pointers
	 * during queue setup.
	 *
	 * Disable every queue first, then wait for outstanding DMA activity to
	 * stop before clearing TDWBAL/H and the remaining retained state.
	 * Spin only for the normal fast transition, then sleep so a VF that
	 * keeps asserting QUEUE_ENABLE cannot busy-wait the PF for 10 ms.
	 */
	for (i = 0; i < nqueues; i++) {
		E1000_WRITE_REG(hw, E1000_RXDCTL(qid[i]), 0);
		E1000_WRITE_REG(hw, E1000_TXDCTL(qid[i]), 0);
	}
	E1000_WRITE_FLUSH(hw);
	for (retry = 0; retry < IGB_IOV_QUEUE_DISABLE_RETRIES; retry++) {
		for (i = 0; i < nqueues; i++) {
			rxdctl = E1000_READ_REG(hw, E1000_RXDCTL(qid[i]));
			txdctl = E1000_READ_REG(hw, E1000_TXDCTL(qid[i]));
			if ((rxdctl & E1000_RXDCTL_QUEUE_ENABLE) != 0 ||
			    (txdctl & E1000_TXDCTL_QUEUE_ENABLE) != 0)
				break;
		}
		if (i == nqueues)
			break;
		if (retry + 1 < IGB_IOV_QUEUE_DISABLE_RETRIES) {
			if (retry < IGB_IOV_QUEUE_DISABLE_BUSY_RETRIES)
				DELAY(IGB_IOV_QUEUE_DISABLE_DELAY_US);
			else
				pause_sbt("igbqds",
				    IGB_IOV_QUEUE_DISABLE_PAUSE, 0,
				    C_PREL(1));
		}
	}
	if (retry == IGB_IOV_QUEUE_DISABLE_RETRIES) {
		if (ratecheck(&vf->last_queue_log,
		    &igb_iov_mbx_log_interval))
			device_printf(sc->dev,
			    "could not disable queues for VF %u; "
			    "reset deferred\n", vf->pool);
		return (false);
	}

	for (i = 0; i < nqueues; i++) {
		E1000_WRITE_REG(hw, E1000_SRRCTL(qid[i]), 0);
		E1000_WRITE_REG(hw, E1000_DCA_RXCTRL(qid[i]), 0);
		E1000_WRITE_REG(hw, E1000_TDWBAL(qid[i]), 0);
		E1000_WRITE_REG(hw, E1000_TDWBAH(qid[i]), 0);
		E1000_WRITE_REG(hw, E1000_DCA_TXCTRL(qid[i]), 0);
	}
	E1000_WRITE_REG(hw, E1000_PSRTYPE(vf->pool), 0);
	E1000_WRITE_FLUSH(hw);
	return (true);
}

static bool
igb_iov_reset_event_common(struct e1000_softc *sc, struct igb_vf *vf,
    bool reset_intrs)
{
	struct e1000_hw *hw;
	bool rebuild_mta, rebuild_vlan, sanitized;
	u32 reg;

	hw = &sc->hw;
	rebuild_mta = vf->mc_count != 0;
	rebuild_vlan = !igb_iov_vf_vlan_is_default(vf);
	reg = E1000_READ_REG(hw, E1000_VFTE);
	E1000_WRITE_REG(hw, E1000_VFTE, reg & ~(1U << vf->pool));
	reg = E1000_READ_REG(hw, E1000_VFRE);
	E1000_WRITE_REG(hw, E1000_VFRE, reg & ~(1U << vf->pool));
	if (reset_intrs)
		E1000_WRITE_REG(hw, E1000_VTCTRL(vf->pool),
		    E1000_VTCTRL_RST);
	sanitized = igb_iov_sanitize_vf_queues(sc, vf);
	E1000_WRITE_REG(hw, E1000_VMVIR(vf->pool), 0);
	igb_iov_clear_mac_filters(sc, vf);
	igb_iov_clear_rar(sc, vf->rar_index);
	igb_iov_reset_vf_state(sc, vf);
	if (rebuild_mta)
		igb_iov_rebuild_mta(sc);
	if (rebuild_vlan)
		igb_iov_rebuild_vlan(sc);
	return (sanitized);
}

static bool
igb_iov_reset_event(struct e1000_softc *sc, struct igb_vf *vf)
{
	return (igb_iov_reset_event_common(sc, vf, true));
}

static void
igb_iov_mdd_reset_event(struct e1000_softc *sc, struct igb_vf *vf)
{
	/*
	 * VTCTRL.RST clears the VF's queue-enable and interrupt registers
	 * (I350 section 8.28.1).  It therefore also removes the admin-vector
	 * route needed to deliver the reset notification below.  MDD recovery
	 * explicitly permits toggling VFTE instead (section 7.8.3.8.3).
	 *
	 * Leave the interrupt registers intact, keep VFTE/VFRE disabled until
	 * the VF completes a new reset handshake, and use the no-CTS control
	 * message to make the guest reinitialize.  FreeBSD and DPDK consume
	 * that message directly; Linux ACKs it and the PF's non-CTS ACK path
	 * replies with the NACK that schedules igbvf's reset task.
	 *
	 * Sanitization failure leaves the pool disabled.  The VF reset
	 * handshake retries it and is NACKed while a queue remains active.
	 */
	(void)igb_iov_reset_event_common(sc, vf, false);
}

static void
igb_iov_reset_msg(struct e1000_softc *sc, struct igb_vf *vf)
{
	struct e1000_hw *hw;
	u32 msg[3], reg;

	hw = &sc->hw;
	if (!igb_iov_reset_event(sc, vf)) {
		msg[0] = E1000_VF_RESET | E1000_VT_MSGTYPE_NACK;
		e1000_write_mbx(hw, msg, 1, vf->pool);
		return;
	}
	igb_iov_map_rar(sc, vf->rar_index, vf->mac, vf->pool);
	igb_iov_set_anti_spoof(sc, vf);

	reg = E1000_READ_REG(hw, E1000_VFTE);
	E1000_WRITE_REG(hw, E1000_VFTE, reg | (1U << vf->pool));
	reg = E1000_READ_REG(hw, E1000_VFRE);
	E1000_WRITE_REG(hw, E1000_VFRE, reg | (1U << vf->pool));
	/*
	 * 82576's WVBR blocked bitmap is read-clear, so the reset handshake
	 * completes that event's lifetime.  I350 MDFB might be read-only;
	 * re-arm its edge latch only after a valid MDFB sample reads clear.
	 */
	if (hw->mac.type == e1000_82576)
		vf->flags &= ~IGB_VF_MDD_BLOCKED;
	vf->flags |= IGB_VF_CTS;

	memset(msg, 0, sizeof(msg));
	msg[0] = E1000_VF_RESET | E1000_VT_MSGTYPE_ACK;
	memcpy(&msg[1], vf->mac, ETHER_ADDR_LEN);
	e1000_write_mbx(hw, msg, 3, vf->pool);
}

static int
igb_iov_set_mac_filter(struct e1000_softc *sc, struct igb_vf *vf, u32 *msg)
{
	struct igb_vf_mac_filter *filter, *free_filter;
	const u8 *mac;
	u32 info;
	int count, i;

	info = msg[0] & E1000_VT_MSGINFO_MASK;
	if (info == E1000_VF_MAC_FILTER_CLR) {
		igb_iov_clear_mac_filters(sc, vf);
		return (0);
	}
	if (info != E1000_VF_MAC_FILTER_ADD)
		return (EINVAL);
	if ((vf->flags & IGB_VF_CAP_MAC) == 0)
		return (EPERM);

	mac = (const u8 *)&msg[1];
	if (!igb_iov_mac_valid(mac))
		return (EINVAL);
	if (memcmp(mac, vf->mac, ETHER_ADDR_LEN) == 0)
		return (0);

	count = 0;
	free_filter = NULL;
	for (i = 0; i < sc->num_vf_mac_filters; i++) {
		filter = &sc->vf_mac_filters[i];
		if (!filter->active) {
			if (free_filter == NULL)
				free_filter = filter;
			continue;
		}
		if (memcmp(filter->mac, mac, ETHER_ADDR_LEN) != 0)
			continue;
		return (filter->pool == vf->pool ? 0 : EADDRINUSE);
	}
	for (i = 0; i < sc->num_vf_mac_filters; i++)
		if (sc->vf_mac_filters[i].active &&
		    sc->vf_mac_filters[i].pool == vf->pool)
			count++;
	if (igb_iov_mac_in_use(sc, mac, vf))
		return (EADDRINUSE);
	if (count >= IGB_IOV_MAX_MAC_FILTERS)
		return (ENOSPC);
	if (free_filter == NULL)
		return (ENOSPC);

	free_filter->active = true;
	free_filter->pool = vf->pool;
	memcpy(free_filter->mac, mac, ETHER_ADDR_LEN);
	igb_iov_map_rar(sc, free_filter->rar_index, free_filter->mac, vf->pool);
	return (0);
}

static int
igb_iov_set_mac(struct e1000_softc *sc, struct igb_vf *vf, u32 *msg)
{
	u8 *mac;

	if ((msg[0] & E1000_VT_MSGINFO_MASK) != 0)
		return (igb_iov_set_mac_filter(sc, vf, msg));

	mac = (u8 *)&msg[1];
	if (!igb_iov_mac_valid(mac))
		return (EINVAL);
	if (memcmp(mac, vf->mac, ETHER_ADDR_LEN) != 0 &&
	    !(vf->flags & IGB_VF_CAP_MAC))
		return (EPERM);
	if (memcmp(mac, vf->mac, ETHER_ADDR_LEN) != 0 &&
	    igb_iov_mac_in_use(sc, mac, vf))
		return (EADDRINUSE);

	memcpy(vf->mac, mac, ETHER_ADDR_LEN);
	igb_iov_map_rar(sc, vf->rar_index, vf->mac, vf->pool);
	return (0);
}

static int
igb_iov_set_multicast(struct e1000_softc *sc, struct igb_vf *vf, u32 *msg)
{
	u16 hashes[IGB_IOV_MAX_MC_HASHES] = {};
	bool overflow;
	int count, i;

	count = (msg[0] & E1000_VF_SET_MULTICAST_COUNT_MASK) >>
	    E1000_VT_MSGINFO_SHIFT;
	overflow = count > IGB_IOV_MAX_MC_HASHES ||
	    (msg[0] & E1000_VF_SET_MULTICAST_OVERFLOW) != 0;
	count = min(count, IGB_IOV_MAX_MC_HASHES);
	for (i = 0; i < count; i++)
		hashes[i] =
		    (msg[1 + i / 2] >> ((i & 1) * 16)) & 0xffff;
	if (vf->mc_count == count &&
	    ((vf->flags & IGB_VF_MCAST_OVERFLOW) != 0) == overflow &&
	    memcmp(vf->mc_hashes, hashes, sizeof(hashes)) == 0)
		return (0);
	memcpy(vf->mc_hashes, hashes, sizeof(vf->mc_hashes));
	vf->mc_count = count;
	if (overflow)
		vf->flags |= IGB_VF_MCAST_OVERFLOW;
	else
		vf->flags &= ~IGB_VF_MCAST_OVERFLOW;
	if (overflow &&
	    (vf->flags & IGB_VF_MCAST_OVERFLOW_WARNED) == 0) {
		vf->flags |= IGB_VF_MCAST_OVERFLOW_WARNED;
		device_printf(sc->dev,
		    "VF %u multicast list exceeds 30 entries; "
		    "enabling all-multicast reception\n", vf->pool);
	}
	igb_iov_configure_vmolr(sc, vf);
	igb_iov_rebuild_mta(sc);
	return (0);
}

static int
igb_iov_set_lpe(struct e1000_softc *sc, struct igb_vf *vf, u32 *msg)
{
	u32 size;

	size = msg[1];
	if (size < ETHER_MIN_LEN)
		return (EINVAL);
	vf->max_frame_size = min(size, IGB_IOV_MAX_FRAME_SIZE);
	igb_iov_configure_vmolr(sc, vf);
	return (0);
}

static int
igb_iov_set_promisc(struct e1000_softc *sc, struct igb_vf *vf, u32 msg)
{
	u32 mode;

	mode = msg & E1000_VT_MSGINFO_MASK;
	if (mode & ~(E1000_VF_SET_PROMISC_UNICAST |
	    E1000_VF_SET_PROMISC_MULTICAST))
		return (EINVAL);
	if (mode != 0 && !(vf->flags & IGB_VF_ALLOW_PROMISC))
		return (EPERM);

	vf->flags &= ~(IGB_VF_UCAST_PROMISC | IGB_VF_MCAST_PROMISC);
	if (mode & E1000_VF_SET_PROMISC_UNICAST)
		vf->flags |= IGB_VF_UCAST_PROMISC;
	if (mode & E1000_VF_SET_PROMISC_MULTICAST)
		vf->flags |= IGB_VF_MCAST_PROMISC;
	igb_iov_configure_vmolr(sc, vf);
	igb_iov_set_uta(sc);
	return (0);
}

static bool
igb_iov_process_msg(struct e1000_softc *sc, struct igb_vf *vf)
{
	struct e1000_hw *hw;
	u32 msg[E1000_VFMAILBOX_SIZE], type;
	int error;

	hw = &sc->hw;
	memset(msg, 0, sizeof(msg));
	if (e1000_read_mbx(hw, msg, nitems(msg), vf->pool, false) != 0)
		return (false);
	vf->flags &= ~IGB_VF_MBX_PENDING;
	vf->mbx_retry_at = 0;
	vf->mbx_retry_count = 0;

	if (msg[0] & (E1000_VT_MSGTYPE_ACK | E1000_VT_MSGTYPE_NACK)) {
		e1000_unlock_mbx(hw, vf->pool);
		return (true);
	}
	if (msg[0] == E1000_VF_RESET) {
		igb_iov_reset_msg(sc, vf);
		return (true);
	}
	if (!(vf->flags & IGB_VF_CTS)) {
		if (igb_iov_nack_allowed(vf)) {
			msg[0] = igb_iov_reply_header(msg[0], false, false);
			e1000_write_mbx(hw, msg, 1, vf->pool);
		} else
			e1000_unlock_mbx(hw, vf->pool);
		return (true);
	}

	type = msg[0] & 0xffff;
	switch (type) {
	case E1000_VF_SET_MAC_ADDR:
		error = igb_iov_set_mac(sc, vf, msg);
		break;
	case E1000_VF_SET_MULTICAST:
		error = igb_iov_set_multicast(sc, vf, msg);
		break;
	case E1000_VF_SET_VLAN:
		if (vf->default_vlan != 0)
			error = EPERM;
		else if ((msg[1] & ~E1000_VLVF_VLANID_MASK) != 0)
			error = EINVAL;
		else
			error = igb_iov_set_vlan(sc, vf,
			    msg[1] & E1000_VLVF_VLANID_MASK,
			    (msg[0] & E1000_VF_SET_VLAN_ADD) != 0);
		break;
	case E1000_VF_SET_LPE:
		error = igb_iov_set_lpe(sc, vf, msg);
		break;
	case E1000_VF_SET_PROMISC:
		error = igb_iov_set_promisc(sc, vf, msg[0]);
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}

	msg[0] = igb_iov_reply_header(msg[0], true, error == 0);
	e1000_write_mbx(hw, msg, 1, vf->pool);
	return (true);
}

static sbintime_t
igb_iov_service_pending_mbx(struct e1000_softc *sc, struct igb_vf *vf,
    sbintime_t now)
{
	sbintime_t delay;

	if ((vf->flags & IGB_VF_MBX_PENDING) == 0)
		return (0);
	if (vf->mbx_retry_at != 0 && now < vf->mbx_retry_at)
		return (vf->mbx_retry_at);
	if (igb_iov_process_msg(sc, vf))
		return (0);

	now = getsbinuptime();
	if (vf->mbx_retry_count < IGB_IOV_MBX_RETRY_COUNT) {
		delay = igb_iov_mbx_retry_delay[vf->mbx_retry_count++];
		vf->mbx_retry_at = now + delay;
		return (vf->mbx_retry_at);
	}

	vf->flags &= ~(IGB_VF_CTS | IGB_VF_MBX_PENDING);
	vf->flags |= IGB_VF_MBX_GAVE_UP;
	vf->mbx_retry_at = 0;
	if (ratecheck(&vf->last_mbx_log, &igb_iov_mbx_log_interval))
		device_printf(sc->dev,
		    "mailbox remained busy for VF %u; CTS revoked\n",
		    vf->pool);
	return (0);
}

void
igb_iov_handle_mbx(struct e1000_softc *sc)
{
	struct e1000_hw *hw;
	struct igb_vf *vf;
	sbintime_t delay, next_retry_at, now, retry_at;
	u32 msg;
	int i;

	if (!sc->iov_hw_active)
		return;

	hw = &sc->hw;
	next_retry_at = 0;
	for (i = 0; i < sc->num_vfs; i++) {
		vf = &sc->vfs[i];
		if (!(vf->flags & IGB_VF_ACTIVE))
			continue;
		now = getsbinuptime();
		if (e1000_check_for_rst(hw, vf->pool) == 0) {
			/*
			 * The old VF is gone.  A new owner's reset handshake
			 * reruns sanitization before enabling its pool.
			 */
			(void)igb_iov_reset_event(sc, vf);
		}
		if ((vf->flags &
		    (IGB_VF_MBX_PENDING | IGB_VF_MBX_GAVE_UP)) == 0 &&
		    e1000_check_for_msg(hw, vf->pool) == 0) {
			vf->flags |= IGB_VF_MBX_PENDING;
			vf->mbx_retry_at = 0;
			vf->mbx_retry_count = 0;
		}
		retry_at = igb_iov_service_pending_mbx(sc, vf, now);
		if (retry_at != 0 &&
		    (next_retry_at == 0 || retry_at < next_retry_at))
			next_retry_at = retry_at;
		if (e1000_check_for_ack(hw, vf->pool) == 0 &&
		    !(vf->flags & IGB_VF_CTS) && igb_iov_nack_allowed(vf)) {
			msg = E1000_VT_MSGTYPE_NACK;
			e1000_write_mbx(hw, &msg, 1, vf->pool);
		}
	}
	if (next_retry_at != 0) {
		delay = next_retry_at - getsbinuptime();
		if (delay <= 0)
			delay = SBT_1MS;
		callout_reset_sbt(&sc->iov_mbx_retry, delay, 0,
		    igb_iov_mbx_retry_callout, sc, C_PREL(1));
	}
}

static bool
igb_iov_notify_vf_mdd_reset(struct e1000_softc *sc, struct igb_vf *vf)
{
	u32 msg;

	/*
	 * MDD recovery preserves the VF's admin-vector configuration.  Send
	 * the same no-CTS control message used for PF reset notification so
	 * the VF discards its state and completes a new reset handshake.
	 * A failed write is retried from the timer-driven admin pass; the VF's
	 * transmit watchdog remains the final fallback when traffic is still
	 * queued and notification never succeeds.
	 */
	msg = E1000_PF_CONTROL_MSG;
	if (e1000_write_mbx(&sc->hw, &msg, 1, vf->pool) != 0) {
		vf->mdd_notify_at =
		    getsbinuptime() + igb_iov_mdd_notify_retry;
		if (ratecheck(&vf->last_mbx_log,
		    &igb_iov_mbx_log_interval))
			device_printf(sc->dev,
			    "could not notify VF %u of malicious-driver "
			    "reset; will retry\n", vf->pool);
		return (false);
	}
	vf->flags &= ~IGB_VF_MDD_NOTIFY_PENDING;
	vf->mdd_notify_at = 0;
	return (true);
}

void
igb_iov_handle_mdd(struct e1000_softc *sc)
{
	struct igb_vf *vf;
	u32 blocked, cleared, handled, lvmmc;
	u32 readback, spoofed;
	bool mdfb_valid, pending;
	int i;

	pending = atomic_readandclear_32(&sc->iov_pending) != 0;
	lvmmc = pending ?
	    atomic_readandclear_32(&sc->iov_mdd_cause) : 0;
	if (!sc->iov_hw_active) {
		atomic_readandclear_32(&sc->iov_spoof_pending);
		atomic_readandclear_32(&sc->iov_blocked_pending);
		return;
	}

	blocked = 0;
	handled = 0;
	mdfb_valid = false;
	if (sc->hw.mac.type == e1000_i350) {
		u32 mdfb;

		spoofed = atomic_readandclear_32(&sc->iov_spoof_pending);
		/*
		 * I350 reports ordinary MAC/VLAN spoofing through the
		 * interrupt-time LVMMC snapshot rather than WVBR.  The
		 * filter accumulates Last_Q into iov_spoof_pending so events
		 * from different VFs coalesce safely until this timer-driven
		 * admin pass.
		 */
		spoofed &= IGB_I350_QUEUE_MASK;
		/*
		 * Sample MDFB on every admin pass so a blocked queue is not
		 * mislabeled as an ordinary spoof when no MDDET observation
		 * is pending.
		 */
		mdfb = E1000_READ_REG(&sc->hw, E1000_MDFB);
		if (__predict_false(mdfb == 0xffffffff))
			mdfb = 0;
		else {
			mdfb &= IGB_I350_QUEUE_MASK;
			mdfb_valid = true;
		}
		/*
		 * I350 SDM sections 8.14.10 and 8.14.11: WVBR reports
		 * spoof and malicious-driver events, while MDFB identifies
		 * the queues actually blocked for malicious behavior.
		 */
		spoofed &= ~mdfb;
		blocked = mdfb;
		if (blocked != 0 && lvmmc == 0)
			lvmmc = E1000_READ_REG(&sc->hw, E1000_LVMMC);
		/*
		 * A failed diagnostic read does not invalidate the
		 * blocked-queue bitmap that was read successfully above.
		 */
		if (__predict_false(lvmmc == 0xffffffff))
			lvmmc = 0;
		/*
		 * MDFB is authoritative for queues stopped by malicious-driver
		 * detection.  LVMMC reports causes such as VLAN IERR and
		 * Mal_PF, but its Last_Q field does not establish that a queue
		 * was blocked.  Do not manufacture a blocked bit when MDFB is
		 * clear.
		 */
	} else {
		if (!pending)
			return;
		/*
		 * WVBR is read-clear and does not preserve every queue across
		 * multiple MDDET interrupts.  The interrupt filter snapshots and
		 * accumulates its pool bitmaps before this deferred admin pass.
		 */
		spoofed = atomic_readandclear_32(&sc->iov_spoof_pending);
		blocked = atomic_readandclear_32(&sc->iov_blocked_pending);
		/* A blocked-queue classification dominates its WVBR low bit. */
		spoofed &= ~blocked;
	}

	for (i = 0; i < sc->num_vfs; i++) {
		vf = &sc->vfs[i];
		if (!(vf->flags & IGB_VF_ACTIVE))
			continue;
		if ((vf->flags & IGB_VF_MDD_NOTIFY_PENDING) != 0 &&
		    getsbinuptime() >= vf->mdd_notify_at)
			(void)igb_iov_notify_vf_mdd_reset(sc, vf);
		/*
		 * An invalid MDFB sample must neither report a new edge nor
		 * masquerade as evidence that an old edge has cleared.
		 */
		if (sc->hw.mac.type == e1000_i350 && mdfb_valid &&
		    (blocked & (1U << i)) == 0)
			vf->flags &= ~IGB_VF_MDD_BLOCKED;
		if ((spoofed & (1U << i)) != 0 &&
		    ratecheck(&vf->last_spoof_log,
		    &igb_iov_spoof_log_interval))
			device_printf(sc->dev,
			    "spoof event detected from VF %u; packet dropped\n",
			    vf->pool);
		if ((blocked & (1U << i)) == 0)
			continue;
		if ((vf->flags & IGB_VF_MDD_BLOCKED) != 0)
			continue;
		vf->flags |= IGB_VF_MDD_BLOCKED;
		if (ratecheck(&vf->last_mdd_log, &igb_iov_mdd_log_interval))
			device_printf(sc->dev,
			    "malicious-driver event 0x%08x from VF %u; "
			    "resetting VF\n", lvmmc, vf->pool);
		igb_iov_mdd_reset_event(sc, vf);
		vf->flags |= IGB_VF_MDD_NOTIFY_PENDING;
		(void)igb_iov_notify_vf_mdd_reset(sc, vf);
		handled |= 1U << i;
	}
	if (sc->hw.mac.type == e1000_i350 && mdfb_valid &&
	    (blocked & (1U << sc->pool)) == 0)
		sc->iov_pf_mdd_blocked = false;
	if ((blocked & (1U << sc->pool)) != 0 &&
	    (sc->hw.mac.type != e1000_i350 ||
	    !sc->iov_pf_mdd_blocked)) {
		if (sc->hw.mac.type == e1000_i350)
			sc->iov_pf_mdd_blocked = true;
		if (ratecheck(&sc->iov_last_mdd_log,
		    &igb_iov_mdd_log_interval))
			device_printf(sc->dev,
			    "malicious-driver event 0x%08x from PF queue; "
			    "resetting PF\n", lvmmc);
		iflib_request_reset(sc->ctx);
		iflib_admin_intr_deferred(sc->ctx);
		handled |= 1U << sc->pool;
	}
	if (sc->hw.mac.type == e1000_i350 && handled != 0) {
		/*
		 * I350 documentation conflicts: the register summary calls
		 * MDFB RWS while the detailed field table calls it RO.  I350
		 * silicon clears a blocked bit when software writes it back.
		 * Write only bits whose recovery was initiated.  If a revision
		 * instead implements MDFB as RO, the edge latch above prevents
		 * a reset loop and this one transition-time write is harmless.
		 */
		E1000_WRITE_REG(&sc->hw, E1000_MDFB, handled);
		E1000_WRITE_FLUSH(&sc->hw);
		/*
		 * Rearm from observed hardware state instead of waiting for
		 * the next admin pass.  The PF context lock prevents a reset
		 * handshake from re-enabling the VF before this readback.  A
		 * write-to-clear part reports zero; a read-only part retains
		 * the bit and therefore retains the one-shot edge latch.
		 */
		readback = E1000_READ_REG(&sc->hw, E1000_MDFB);
		if (__predict_false(readback == 0xffffffff))
			cleared = 0;
		else
			cleared = handled &
			    ~(readback & IGB_I350_QUEUE_MASK);
		for (i = 0; i < sc->num_vfs; i++)
			if ((cleared & (1U << i)) != 0)
				sc->vfs[i].flags &= ~IGB_VF_MDD_BLOCKED;
		if ((cleared & (1U << sc->pool)) != 0)
			sc->iov_pf_mdd_blocked = false;
	}
	if (sc->hw.mac.type == e1000_i350) {
		/*
		 * I350 can retain EICR.OTHER without delivering the admin MSI-X
		 * even though its EIMS and legacy IMS bits remain enabled.  Kick
		 * the already-enabled vector on each admin pass so its filter
		 * consumes any retained ICR/LVMMC cause.  A synthetic interrupt
		 * with no legacy cause is handled entirely by the filter.
		 */
		E1000_WRITE_REG(&sc->hw, E1000_EICS, sc->link_mask);
		E1000_WRITE_FLUSH(&sc->hw);
	}
}

void
igb_iov_mdd_event(struct e1000_softc *sc)
{
	u32 blocked, cause, queues, queue, spoofed, wvbr;

	/*
	 * LVMMC is clear-on-read.  Preserve it in the interrupt filter, as
	 * Intel's igb driver does, rather than deferring the only copy.
	 */
	cause = E1000_READ_REG(&sc->hw, E1000_LVMMC);
	if (__predict_false(cause == 0xffffffff))
		return;
	if (sc->hw.mac.type == e1000_82576) {
		/*
		 * Snapshot WVBR in the interrupt filter.  Waiting for the admin
		 * task loses all but the last of back-to-back VF MDD events on
		 * 82576.  Convert the staggered queue map into pool bits and OR
		 * each observation into software latches for deferred recovery.
		 */
		wvbr = E1000_READ_REG(&sc->hw, E1000_WVBR);
		if (__predict_false(wvbr == 0xffffffff)) {
			spoofed = 0;
			blocked = 0;
		} else {
			queues = wvbr & IGB_82576_QUEUE_MASK;
			spoofed = (queues & 0xff) |
			    (queues >> IGB_82576_STAGGERED_QUEUE_SHIFT);
			queues = (wvbr >> 16) & IGB_82576_QUEUE_MASK;
			blocked = (queues & 0xff) |
			    (queues >> IGB_82576_STAGGERED_QUEUE_SHIFT);
		}
		SDT_PROBE4(igb_iov, mdd, sample, wvbr, sc, wvbr, spoofed,
		    blocked);
		/*
		 * 82576 can report a coalesced block-class event with all affected
		 * queues in WVBR's low half and no high-half blocked bits.  If an
		 * ordinary spoof shares that snapshot, the register has no per-queue
		 * cause information.  Deliberately fail closed by recovering every
		 * low-half queue; this can reset a spoof-only sibling, but avoids
		 * stranding a blocked VF.  LVMMC.Last_Q identifies only the final
		 * event and lost simultaneous blocked VFs on tested silicon.
		 */
		if (blocked == 0 &&
		    (cause & IGB_82576_LVMMC_BLOCK_MASK) != 0) {
			blocked = spoofed;
			if (blocked == 0) {
				queue = (cause >> 16) & 0xf;
				blocked = 1U << (queue & 0x7);
			}
		}
		if (spoofed != 0)
			atomic_set_32(&sc->iov_spoof_pending, spoofed);
		if (blocked != 0)
			atomic_set_32(&sc->iov_blocked_pending, blocked);
	}
	if (sc->hw.mac.type == e1000_i350 &&
	    (cause & IGB_I350_LVMMC_MAC_VLAN_SPOOF) != 0) {
		queue = (cause >> IGB_I350_LVMMC_LAST_Q_SHIFT) &
		    IGB_I350_LVMMC_LAST_Q_MASK;
		/*
		 * FreeBSD assigns one queue to each VF pool, so Last_Q is
		 * also the VF number.  Preserve all VFs observed before the
		 * timer pass, and do not overwrite an unrelated blocked
		 * queue's diagnostic with this non-blocking spoof event.
		 */
		atomic_set_32(&sc->iov_spoof_pending, 1U << queue);
		return;
	}
	atomic_store_rel_32(&sc->iov_mdd_cause, cause);
	atomic_set_32(&sc->iov_pending, 1);
}

void
igb_iov_ping_all_vfs(struct e1000_softc *sc)
{
	struct igb_vf *vf;
	u32 msg;
	int i;

	if (!sc->iov_hw_active)
		return;

	for (i = 0; i < sc->num_vfs; i++) {
		vf = &sc->vfs[i];
		if (!(vf->flags & IGB_VF_ACTIVE))
			continue;
		msg = E1000_PF_CONTROL_MSG;
		if (vf->flags & IGB_VF_CTS)
			msg |= E1000_VT_MSGTYPE_CTS;
		e1000_write_mbx(&sc->hw, &msg, 1, vf->pool);
	}
}

void
igb_iov_initialize(struct e1000_softc *sc)
{
	struct e1000_hw *hw;
	struct igb_vf *vf;
	u32 ctrl_ext, dtxctl, mask, rctl, rplolr, vt_ctl;
	int i;

	if (sc->num_vfs == 0)
		return;

	hw = &sc->hw;
	atomic_readandclear_32(&sc->iov_mdd_cause);
	atomic_readandclear_32(&sc->iov_pending);
	atomic_readandclear_32(&sc->iov_spoof_pending);
	atomic_readandclear_32(&sc->iov_blocked_pending);
	/* Plain VMDq keeps every 82576 PF/VF pool on queue zero. */
	E1000_WRITE_REG(hw, E1000_MRQC, E1000_MRQC_ENABLE_VMDQ);

	vt_ctl = E1000_READ_REG(hw, E1000_VT_CTL);
	vt_ctl &= ~(E1000_VT_CTL_DEFAULT_POOL_MASK |
	    E1000_VT_CTL_DISABLE_DEF_POOL);
	vt_ctl |= sc->pool << E1000_VT_CTL_DEFAULT_POOL_SHIFT;
	vt_ctl |= E1000_VT_CTL_VM_REPL_EN;
	E1000_WRITE_REG(hw, E1000_VT_CTL, vt_ctl);

	mask = 1U << sc->pool;
	E1000_WRITE_REG(hw, E1000_VFRE, mask);
	E1000_WRITE_REG(hw, E1000_VFTE, mask);
	/* A VF without RX descriptors must not block any other pool. */
	E1000_WRITE_REG(hw, E1000_QDE,
	    hw->mac.type == e1000_i350 ? IGB_I350_QUEUE_MASK : ALL_QUEUES);
	e1000_vmdq_set_loopback_pf(hw, true);
	dtxctl = E1000_READ_REG(hw, E1000_DTXCTL);
	dtxctl |= E1000_DTXCTL_MDP_EN;
	if (hw->mac.type == e1000_82576) {
		dtxctl |= E1000_DTXCTL_VLAN_ADDED |
		    E1000_DTXCTL_SPOOF_INT;
		rplolr = E1000_READ_REG(hw, E1000_RPLOLR);
		rplolr |= E1000_RPLOLR_STRVLAN;
		E1000_WRITE_REG(hw, E1000_RPLOLR, rplolr);
	} else {
		/*
		 * I350 SDM section 8.12.5 defines this field with inverted
		 * polarity: setting it keeps an ordinary spoof from disabling
		 * the VF queue.  Enable its notification as well.  I350
		 * hardware reports the VF in LVMMC.Last_Q (WVBR remains zero);
		 * the moderated admin vector captures that value, while
		 * timer-driven administration and per-VF ratecheck bound the
		 * work and console output.
		 */
		dtxctl |= E1000_DTXCTL_SPOOF_INT |
		    IGB_I350_DTXCTL_ENABLE_SPOOF_QUEUE;
	}
	E1000_WRITE_REG(hw, E1000_DTXCTL, dtxctl);

	igb_iov_map_rar(sc, 0, hw->mac.addr, sc->pool);
	igb_iov_configure_pf_vmolr(sc);
	igb_iov_set_uta(sc);
	for (i = 0; i < sc->num_vfs; i++) {
		vf = &sc->vfs[i];
		if (!(vf->flags & IGB_VF_ACTIVE))
			continue;
		/*
		 * A PF-wide reset is trusted and can require a complete guest
		 * replay.  Guest-controlled RESET and VFLR do not refill this
		 * allowance.
		 */
		igb_iov_reset_vlan_rate(vf);
		igb_iov_clear_mac_filters(sc, vf);
		igb_iov_reset_vf_state(sc, vf);
		igb_iov_clear_rar(sc, vf->rar_index);
		igb_iov_set_anti_spoof(sc, vf);
	}
	igb_iov_rebuild_mta(sc);
	igb_iov_rebuild_vlan(sc);

	rctl = E1000_READ_REG(hw, E1000_RCTL);
	E1000_WRITE_REG(hw, E1000_RCTL, rctl | E1000_RCTL_VFE);
	E1000_WRITE_REG(hw, E1000_MBVFIMR, igb_iov_active_mask(sc));
	if (hw->mac.type == e1000_i350)
		E1000_WRITE_REG(hw, E1000_DMACR, 0);

	ctrl_ext = E1000_READ_REG(hw, E1000_CTRL_EXT);
	E1000_WRITE_REG(hw, E1000_CTRL_EXT,
	    ctrl_ext | E1000_CTRL_EXT_PFRSTD);
	E1000_WRITE_FLUSH(hw);
	/*
	 * MDDET remains masked until iov_hw_active is published and iflib
	 * rearms the admin vector.  Programming the per-pool policy above can
	 * leave a setup-time MDDET observation in the read-clear registers.
	 * If that stale cause is carried across the unmask, a later ordinary
	 * spoof can update LVMMC without generating a new interrupt edge.
	 *
	 * Drain only after all IOV policy is installed and before exposing the
	 * active state.  Mailbox requests are also serviced by the periodic
	 * admin pass, and ping_all_vfs() below supplies a fresh notification.
	 */
	/*
	 * Clear the setup-time interrupt latch before its diagnostic state.
	 * I350 does not reliably generate the next MDDET edge when LVMMC is
	 * consumed while ICR.MDDET remains latched.  This differs deliberately
	 * from the final arm-time drain, where ICR is read last so a later event
	 * remains pending for the unmask.
	 */
	if (hw->mac.type == e1000_i350)
		(void)E1000_READ_REG(hw, E1000_ICR);
	(void)E1000_READ_REG(hw, E1000_LVMMC);
	if (hw->mac.type == e1000_82576)
		(void)E1000_READ_REG(hw, E1000_WVBR);
	if (hw->mac.type != e1000_i350)
		(void)E1000_READ_REG(hw, E1000_ICR);
	atomic_readandclear_32(&sc->iov_mdd_cause);
	atomic_readandclear_32(&sc->iov_pending);
	atomic_readandclear_32(&sc->iov_spoof_pending);
	atomic_readandclear_32(&sc->iov_blocked_pending);
	atomic_store_rel_32(&sc->iov_intr_drain_pending, 1);
	sc->iov_hw_active = true;
	igb_iov_ping_all_vfs(sc);
}

int
igb_iov_validate(struct e1000_softc *sc, u16 num_vfs)
{
	if (!igb_iov_supported(sc))
		return (ENXIO);
	/* One of the eight hardware pools is reserved for the PF. */
	if (num_vfs == 0 || num_vfs > MAX_NUM_VFS)
		return (EINVAL);
	if (sc->vfs != NULL)
		return (EBUSY);
	if (sc->intr_type != IFLIB_INTR_MSIX) {
		device_printf(sc->dev, "SR-IOV requires MSI-X\n");
		return (ENOTSUP);
	}
	if (sc->tx_num_queues != 1 || sc->rx_num_queues != 1) {
		device_printf(sc->dev,
		    "SR-IOV requires one PF TX and RX queue; set "
		    "dev.igb.%d.iflib.override_ntxqs=1 and "
		    "dev.igb.%d.iflib.override_nrxqs=1 before attach\n",
		    device_get_unit(sc->dev), device_get_unit(sc->dev));
		return (EINVAL);
	}
	return (0);
}

int
igb_if_vf_status(if_ctx_t ctx, nvlist_t *status)
{
	struct e1000_softc *sc;
	struct igb_vf *vf;
	nvlist_t **vfs;
	u_int num_queues;
	int error, i;

	sc = iflib_get_softc(ctx);
	if (sc->num_vfs == 0)
		return (ENXIO);
	num_queues = sc->hw.mac.type == e1000_82576 ?
	    IGB_82576_VF_QUEUES : IGB_I350_VF_QUEUES;
	vfs = mallocarray(sc->num_vfs, sizeof(*vfs), M_IGB_IOV,
	    M_WAITOK | M_ZERO);
	for (i = 0; i < sc->num_vfs; i++) {
		vf = &sc->vfs[i];
		vfs[i] = nvlist_create(0);
		nvlist_add_number(vfs[i], IFVF_STATUS_INDEX, i);
		nvlist_add_bool(vfs[i], IFVF_STATUS_CONFIGURED,
		    (vf->flags & IGB_VF_ACTIVE) != 0);
		nvlist_add_bool(vfs[i], IFVF_STATUS_INITIALIZED,
		    (vf->flags & IGB_VF_CTS) != 0);
		nvlist_add_binary(vfs[i], IFVF_STATUS_MAC, vf->mac,
		    ETHER_ADDR_LEN);
		if (vf->default_vlan == 0)
			nvlist_add_string(vfs[i], IFVF_STATUS_VLAN_MODE,
			    IFVF_VLAN_MODE_TRUNK);
		else {
			nvlist_add_string(vfs[i], IFVF_STATUS_VLAN_MODE,
			    IFVF_VLAN_MODE_ACCESS);
			nvlist_add_number(vfs[i], IFVF_STATUS_VLAN,
			    vf->default_vlan);
		}
		nvlist_add_number(vfs[i], IFVF_STATUS_VLAN_COUNT,
		    vf->vlan_count);
		nvlist_add_number(vfs[i], IFVF_STATUS_NUM_QUEUES, num_queues);
		nvlist_add_bool(vfs[i], IFVF_STATUS_ALLOW_SET_MAC,
		    (vf->flags & IGB_VF_CAP_MAC) != 0);
		nvlist_add_bool(vfs[i], IFVF_STATUS_ALLOW_SET_VLAN,
		    vf->default_vlan == 0);
		nvlist_add_bool(vfs[i], IFVF_STATUS_MAC_ANTI_SPOOF,
		    (vf->flags & IGB_VF_MAC_ANTI_SPOOF) != 0);
		nvlist_add_bool(vfs[i], IFVF_STATUS_ALLOW_PROMISC,
		    (vf->flags & IGB_VF_ALLOW_PROMISC) != 0);
		nvlist_add_bool(vfs[i], IFVF_STATUS_MDD_BLOCKED,
		    (vf->flags & IGB_VF_MDD_BLOCKED) != 0);
	}
	nvlist_add_nvlist_array(status, IFVF_STATUS_VFS,
	    (const nvlist_t * const *)vfs, sc->num_vfs);
	error = nvlist_error(status);
	for (i = 0; i < sc->num_vfs; i++)
		nvlist_destroy(vfs[i]);
	free(vfs, M_IGB_IOV);
	return (error);
}

int
igb_if_iov_init(if_ctx_t ctx, u16 num_vfs, const nvlist_t *config)
{
	struct e1000_softc *sc;
	int error, i;

	sc = iflib_get_softc(ctx);
	(void)config;
	/*
	 * This callback may run while the PF is down.  Record the software
	 * layout here; igb_iov_initialize() programs it during interface init.
	 */
	atomic_store_rel_32(&sc->iov_teardown, 0);
	error = igb_iov_validate(sc, num_vfs);
	if (error != 0)
		return (error);

	sc->vfs = mallocarray(num_vfs, sizeof(*sc->vfs), M_IGB_IOV,
	    M_WAITOK | M_ZERO);
	sc->num_vf_mac_filters =
	    sc->hw.mac.rar_entry_count - num_vfs - 1;
	sc->vf_mac_filters = mallocarray(sc->num_vf_mac_filters,
	    sizeof(*sc->vf_mac_filters), M_IGB_IOV, M_WAITOK | M_ZERO);
	for (i = 0; i < sc->num_vf_mac_filters; i++)
		sc->vf_mac_filters[i].rar_index = i + 1;
	sc->pool = num_vfs;
	sc->iov_mta_valid = false;
	sc->iov_pf_mdd_blocked = false;
	sc->tx_queues[0].txr.me = sc->pool;
	sc->rx_queues[0].rxr.me = sc->pool;
	e1000_init_mbx_params_pf(&sc->hw);
	sc->num_vfs = num_vfs;
	return (0);
}

void
igb_if_iov_uninit(if_ctx_t ctx)
{
	struct e1000_softc *sc;
	struct e1000_hw *hw;
	u32 mask, rah;
	int error, i, iov_pos;
	u16 iov_ctl;

	sc = iflib_get_softc(ctx);
	if (sc->vfs == NULL)
		return;
	hw = &sc->hw;
	sc->iov_hw_active = false;
	if (sc->iov_mbx_retry_initialized)
		callout_drain(&sc->iov_mbx_retry);

	E1000_WRITE_REG(hw, E1000_MBVFIMR, 0);
	mask = 1U << sc->pool;
	E1000_WRITE_REG(hw, E1000_VFRE, mask);
	E1000_WRITE_REG(hw, E1000_VFTE, mask);

	/*
	 * pci_iov(4) invokes the driver before it clears VF Enable.  Quiesce
	 * the VFs and clear it here so that 82576's queue-reuse interval is
	 * measured from the actual IOV-disable event.
	 */
	error = pci_find_extcap(sc->dev, PCIZ_SRIOV, &iov_pos);
	if (error == 0) {
		iov_ctl = pci_read_config(sc->dev,
		    iov_pos + PCIR_SRIOV_CTL, 2);
		iov_ctl &= ~(PCIM_SRIOV_VF_EN | PCIM_SRIOV_VF_MSE);
		pci_write_config(sc->dev, iov_pos + PCIR_SRIOV_CTL,
		    iov_ctl, 2);
		if (hw->mac.type == e1000_82576) {
			pause("igbiov", MAX(1, howmany(hz, 10)));
			E1000_WRITE_REG(hw, E1000_IOVCTL,
			    E1000_IOVCTL_REUSE_VFQ);
			E1000_WRITE_FLUSH(hw);
			pause("igbiov", MAX(1, howmany(hz, 10)));
		}
	} else
		device_printf(sc->dev,
		    "could not disable PCI SR-IOV before queue reuse: %d\n",
		    error);

	E1000_WRITE_REG(hw, E1000_VT_CTL, 0);
	e1000_vmdq_set_loopback_pf(hw, false);
	e1000_vmdq_set_anti_spoofing_pf(hw, false, 0);
	for (i = 0; i < E1000_VLVF_ARRAY_SIZE; i++)
		E1000_WRITE_REG(hw, E1000_VLVF(i), 0);
	for (i = 0; i < sc->num_vfs; i++)
		if (sc->vfs[i].flags & IGB_VF_ACTIVE)
			igb_iov_clear_rar(sc, sc->vfs[i].rar_index);
	for (i = 0; i < sc->num_vf_mac_filters; i++)
		if (sc->vf_mac_filters[i].active)
			igb_iov_clear_rar(sc, sc->vf_mac_filters[i].rar_index);
	rah = E1000_READ_REG(hw, E1000_RAH(0));
	rah &= ~IGB_IOV_RAH_POOLSEL_MASK;
	E1000_WRITE_REG(hw, E1000_RAH(0), rah);

	free(sc->vfs, M_IGB_IOV);
	free(sc->vf_mac_filters, M_IGB_IOV);
	sc->vfs = NULL;
	sc->vf_mac_filters = NULL;
	sc->num_vfs = 0;
	sc->num_vf_mac_filters = 0;
	sc->pool = 0;
	sc->iov_mta_valid = false;
	sc->iov_pf_mdd_blocked = false;
	sc->iov_pf_vlan_promisc = false;
	igb_iov_vfta_shadow_invalidate(sc);
	sc->tx_queues[0].txr.me = 0;
	sc->rx_queues[0].rxr.me = 0;
	atomic_readandclear_32(&sc->iov_mdd_cause);
	atomic_readandclear_32(&sc->iov_pending);
	atomic_readandclear_32(&sc->iov_spoof_pending);
	atomic_readandclear_32(&sc->iov_blocked_pending);
	atomic_readandclear_32(&sc->iov_intr_drain_pending);
	atomic_store_rel_32(&sc->iov_teardown, 0);
}

static bool
igb_iov_mac_in_use(struct e1000_softc *sc, const u8 *mac,
    const struct igb_vf *skip)
{
	int i;

	if (memcmp(sc->hw.mac.addr, mac, ETHER_ADDR_LEN) == 0)
		return (true);
	for (i = 0; i < sc->num_vfs; i++)
		if (&sc->vfs[i] != skip &&
		    (sc->vfs[i].flags & IGB_VF_ACTIVE) != 0 &&
		    memcmp(sc->vfs[i].mac, mac, ETHER_ADDR_LEN) == 0)
			return (true);
	for (i = 0; i < sc->num_vf_mac_filters; i++)
		if (sc->vf_mac_filters[i].active &&
		    memcmp(sc->vf_mac_filters[i].mac, mac,
		    ETHER_ADDR_LEN) == 0)
			return (true);
	return (false);
}

int
igb_if_iov_vf_add(if_ctx_t ctx, u16 vfnum, const nvlist_t *config)
{
	struct e1000_softc *sc;
	struct igb_vf *vf;
	struct ether_addr generated;
	const void *mac;
	char nameunit[IFNAMSIZ + sizeof("-vf65535")];
	size_t mac_size;
	uint64_t configured_vlan;
	u16 vlan;

	sc = iflib_get_softc(ctx);
	if (vfnum >= sc->num_vfs)
		return (EINVAL);
	vf = &sc->vfs[vfnum];
	if (vf->flags & IGB_VF_ACTIVE)
		return (EBUSY);

	configured_vlan = nvlist_get_number(config, "vlan");
	if (configured_vlan > VF_VLAN_TRUNK)
		return (EINVAL);
	vlan = configured_vlan;
	if (vlan == 0)
		return (ENOTSUP);
	if (vlan == VF_VLAN_TRUNK)
		vlan = 0;
	if (!igb_iov_vlan_present(sc, vlan, false) &&
	    igb_iov_vlan_unique_count(sc, false) >=
	    E1000_VLVF_ARRAY_SIZE)
		return (ENOSPC);

	vf->pool = vfnum;
	vf->rar_index = sc->hw.mac.rar_entry_count - (vfnum + 1);
	vf->max_frame_size = ETHER_MAX_LEN;
	vf->default_vlan = vlan;
	igb_iov_reset_vlan_rate(vf);
	if (nvlist_exists_binary(config, "mac-addr")) {
		mac = nvlist_get_binary(config, "mac-addr", &mac_size);
		if (mac_size != ETHER_ADDR_LEN || !igb_iov_mac_valid(mac))
			return (EINVAL);
		if (igb_iov_mac_in_use(sc, mac, vf))
			return (EADDRINUSE);
		memcpy(vf->mac, mac, ETHER_ADDR_LEN);
	} else {
		snprintf(nameunit, sizeof(nameunit), "%s-vf%u",
		    device_get_nameunit(sc->dev), vfnum);
		ether_gen_addr_byname(nameunit, &generated);
		memcpy(vf->mac, generated.octet, ETHER_ADDR_LEN);
		if (igb_iov_mac_in_use(sc, vf->mac, vf))
			return (EADDRINUSE);
	}
	if (nvlist_get_bool(config, "allow-set-mac"))
		vf->flags |= IGB_VF_CAP_MAC;
	if (nvlist_get_bool(config, "mac-anti-spoof"))
		vf->flags |= IGB_VF_MAC_ANTI_SPOOF;
	if (nvlist_get_bool(config, "allow-promisc"))
		vf->flags |= IGB_VF_ALLOW_PROMISC;
	vf->flags |= IGB_VF_ACTIVE;

	igb_iov_reset_vf_state(sc, vf);
	igb_iov_set_anti_spoof(sc, vf);
	igb_iov_rebuild_vlan(sc);
	E1000_WRITE_REG(&sc->hw, E1000_MBVFIMR, igb_iov_active_mask(sc));
	return (0);
}

#endif /* PCI_IOV */
