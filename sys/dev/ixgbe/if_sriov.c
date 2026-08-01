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

#include "ixgbe.h"
#include "ixgbe_sriov.h"

#ifdef PCI_IOV

#include <sys/iov.h>
#include <sys/ktr.h>

MALLOC_DEFINE(M_IXGBE_SRIOV, "ix_sriov", "ix SR-IOV allocations");

#define IXGBE_VF_MBX_CLEANUP_GRACE	(2 * SBT_1S)

static const struct timeval ixgbe_mdd_log_interval = { 2, 0 };

/************************************************************************
 * ixgbe_define_iov_schemas
 ************************************************************************/
void
ixgbe_define_iov_schemas(device_t dev, int *error)
{
	nvlist_t *pf_schema, *vf_schema;

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
	*error = pci_iov_attach(dev, pf_schema, vf_schema);
	if (*error != 0) {
		device_printf(dev,
		    "Error %d setting up SR-IOV\n", *error);
	}
} /* ixgbe_define_iov_schemas */

/************************************************************************
 * ixgbe_align_all_queue_indices
 ************************************************************************/
inline void
ixgbe_align_all_queue_indices(struct ixgbe_softc *sc)
{
	int i;
	int index;

	for (i = 0; i < sc->num_rx_queues; i++) {
		index = ixgbe_vf_que_index(sc->iov_mode, sc->pool, i);
		sc->rx_queues[i].rxr.me = index;
	}

	for (i = 0; i < sc->num_tx_queues; i++) {
		index = ixgbe_vf_que_index(sc->iov_mode, sc->pool, i);
		sc->tx_queues[i].txr.me = index;
	}
}

/* Support functions for SR-IOV/VF management */
static inline void
ixgbe_send_vf_msg(struct ixgbe_hw *hw, struct ixgbe_vf *vf, u32 msg)
{
	if (vf->flags & IXGBE_VF_CTS)
		msg |= IXGBE_VT_MSGTYPE_CTS;

	ixgbe_write_mbx(hw, &msg, 1, vf->pool);
}

static inline void
ixgbe_send_vf_success(struct ixgbe_softc *sc, struct ixgbe_vf *vf, u32 msg)
{
	msg &= IXGBE_VT_MSG_MASK;
	ixgbe_send_vf_msg(&sc->hw, vf, msg | IXGBE_VT_MSGTYPE_SUCCESS);
}

static inline void
ixgbe_send_vf_failure(struct ixgbe_softc *sc, struct ixgbe_vf *vf, u32 msg)
{
	msg &= IXGBE_VT_MSG_MASK;
	ixgbe_send_vf_msg(&sc->hw, vf, msg | IXGBE_VT_MSGTYPE_FAILURE);
}

static inline void
ixgbe_process_vf_ack(struct ixgbe_softc *sc, struct ixgbe_vf *vf)
{
	if (!(vf->flags & IXGBE_VF_CTS))
		ixgbe_send_vf_failure(sc, vf, 0);
}

static void
ixgbe_vf_set_anti_spoof(struct ixgbe_softc *sc, struct ixgbe_vf *vf)
{
	struct ixgbe_hw *hw;
	uint32_t reg;
	bool enable;

	hw = &sc->hw;
	enable = (vf->flags & IXGBE_VF_ANTI_SPOOF) != 0;
	if (hw->mac.ops.set_mac_anti_spoofing != NULL)
		hw->mac.ops.set_mac_anti_spoofing(hw, enable, vf->pool);
	if (hw->mac.ops.set_vlan_anti_spoofing != NULL)
		hw->mac.ops.set_vlan_anti_spoofing(hw, enable, vf->pool);
	if (hw->mac.ops.set_ethertype_anti_spoofing != NULL) {
		if (enable) {
			IXGBE_WRITE_REG(hw, IXGBE_ETQF(IXGBE_ETQF_FILTER_LLDP),
			    IXGBE_ETQF_FILTER_EN | IXGBE_ETQF_TX_ANTISPOOF |
			    ETHERTYPE_LLDP);
			IXGBE_WRITE_REG(hw, IXGBE_ETQF(IXGBE_ETQF_FILTER_FC),
			    IXGBE_ETQF_FILTER_EN | IXGBE_ETQF_TX_ANTISPOOF |
			    ETHERTYPE_FLOWCONTROL);
		}
		hw->mac.ops.set_ethertype_anti_spoofing(hw, enable,
		    vf->pool);
	}

	reg = IXGBE_READ_REG(hw, IXGBE_VMECM(IXGBE_VF_INDEX(vf->pool)));
	if (enable)
		reg |= IXGBE_VF_BIT(vf->pool);
	else
		reg &= ~IXGBE_VF_BIT(vf->pool);
	IXGBE_WRITE_REG(hw, IXGBE_VMECM(IXGBE_VF_INDEX(vf->pool)), reg);
}

static inline boolean_t
ixgbe_vf_mac_changed(struct ixgbe_vf *vf, const uint8_t *mac)
{
	return (bcmp(mac, vf->ether_addr, ETHER_ADDR_LEN) != 0);
}

static bool
ixgbe_rar_mac_in_use(struct ixgbe_softc *sc, const uint8_t *mac,
    int excluded_rar)
{
	struct ixgbe_hw *hw;
	uint32_t addr_high, addr_low, rah;
	int i;

	hw = &sc->hw;
	addr_low = (uint32_t)mac[0] | ((uint32_t)mac[1] << 8) |
	    ((uint32_t)mac[2] << 16) | ((uint32_t)mac[3] << 24);
	addr_high = (uint32_t)mac[4] | ((uint32_t)mac[5] << 8);
	for (i = 0; i < hw->mac.num_rar_entries; i++) {
		if (i == excluded_rar)
			continue;
		rah = IXGBE_READ_REG(hw, IXGBE_RAH(i));
		if ((rah & IXGBE_RAH_AV) != 0 &&
		    (rah & 0xffff) == addr_high &&
		    IXGBE_READ_REG(hw, IXGBE_RAL(i)) == addr_low)
			return (true);
	}
	return (ixgbe_validate_mac_addr(hw->mac.san_addr) == IXGBE_SUCCESS &&
	    bcmp(hw->mac.san_addr, mac, ETHER_ADDR_LEN) == 0);
}

static inline int
ixgbe_vf_queues(int mode)
{
	switch (mode) {
	case IXGBE_64_VM:
		return (2);
	case IXGBE_32_VM:
		return (4);
	case IXGBE_NO_VM:
	default:
		return (0);
	}
}

inline int
ixgbe_vf_que_index(int mode, int vfnum, int num)
{
	return ((vfnum * ixgbe_vf_queues(mode)) + num);
}

static inline void
ixgbe_update_max_frame(struct ixgbe_softc * sc, int max_frame)
{
	if (sc->max_frame_size < max_frame)
		sc->max_frame_size = max_frame;
}

inline u32
ixgbe_get_mrqc(int iov_mode)
{
	u32 mrqc;

	switch (iov_mode) {
	case IXGBE_64_VM:
		mrqc = IXGBE_MRQC_VMDQRSS64EN;
		break;
	case IXGBE_32_VM:
		mrqc = IXGBE_MRQC_VMDQRSS32EN;
		break;
	case IXGBE_NO_VM:
		mrqc = IXGBE_MRQC_RSSEN;
		break;
	default:
		panic("Unexpected SR-IOV mode %d", iov_mode);
	}

	return mrqc;
}


inline u32
ixgbe_get_mtqc(int iov_mode)
{
	uint32_t mtqc;

	switch (iov_mode) {
	case IXGBE_64_VM:
		mtqc = IXGBE_MTQC_64VF | IXGBE_MTQC_VT_ENA;
		break;
	case IXGBE_32_VM:
		mtqc = IXGBE_MTQC_32VF | IXGBE_MTQC_VT_ENA;
		break;
	case IXGBE_NO_VM:
		mtqc = IXGBE_MTQC_64Q_1PB;
		break;
	default:
		panic("Unexpected SR-IOV mode %d", iov_mode);
	}

	return mtqc;
}

void
ixgbe_ping_all_vfs(struct ixgbe_softc *sc)
{
	struct ixgbe_vf *vf;

	for (int i = 0; i < sc->num_vfs; i++) {
		vf = &sc->vfs[i];
		if ((vf->flags &
		    (IXGBE_VF_ACTIVE | IXGBE_VF_TRAFFIC_DISABLED)) ==
		    IXGBE_VF_ACTIVE)
			ixgbe_send_vf_msg(&sc->hw, vf, IXGBE_PF_CONTROL_MSG);
	}
} /* ixgbe_ping_all_vfs */


static bool
ixgbe_pf_owns_vlan(struct ixgbe_softc *sc, uint16_t tag)
{
	if_t ifp;

	ifp = iflib_get_ifp(sc->ctx);
	if ((if_getcapenable(ifp) & IFCAP_VLAN_HWFILTER) == 0)
		return (true);
	return ((sc->shadow_vfta[tag >> 5] &
	    (1U << (tag & 0x1f))) != 0);
}

static bool
ixgbe_vf_owns_vlan(const struct ixgbe_vf *vf, uint16_t tag)
{

	return ((vf->vlans[tag >> 5] & (1U << (tag & 0x1f))) != 0);
}

static void
ixgbe_vf_vlan_release_pf_only(struct ixgbe_softc *sc, uint16_t tag)
{
	struct ixgbe_hw *hw;
	uint32_t bits[2];
	s32 slot;

	hw = &sc->hw;
	slot = ixgbe_find_vlvf_slot(hw, tag, true);
	if (slot <= 0)
		return;
	bits[0] = IXGBE_READ_REG(hw, IXGBE_VLVFB(slot * 2));
	bits[1] = IXGBE_READ_REG(hw, IXGBE_VLVFB(slot * 2 + 1));
	bits[sc->pool / 32] &= ~(1U << (sc->pool % 32));
	if (bits[0] != 0 || bits[1] != 0)
		return;

	/* The VFTA bit still admits this VLAN to the PF's default pool. */
	IXGBE_WRITE_REG(hw, IXGBE_VLVF(slot), 0);
	IXGBE_WRITE_REG(hw, IXGBE_VLVFB(slot * 2), 0);
	IXGBE_WRITE_REG(hw, IXGBE_VLVFB(slot * 2 + 1), 0);
}

static s32
ixgbe_vf_vlan_hw_update(struct ixgbe_softc *sc, struct ixgbe_vf *vf,
    uint16_t tag, bool enable)
{
	struct ixgbe_hw *hw;
	s32 error;

	hw = &sc->hw;
	if (!enable &&
	    ixgbe_find_vlvf_slot(hw, tag, true) < IXGBE_SUCCESS)
		return (IXGBE_SUCCESS);
	/*
	 * Allocate the VLVF entry with the PF first when it also owns this
	 * VLAN.  This guarantees that adding the VF cannot hide the VLAN from
	 * the PF when the shared VFTA bit becomes pool-selective.
	 */
	if (enable && ixgbe_pf_owns_vlan(sc, tag)) {
		error = ixgbe_set_vfta(hw, tag, sc->pool, true, false);
		if (error != IXGBE_SUCCESS)
			return (error);
	}

	error = ixgbe_set_vfta(hw, tag, vf->pool, enable, false);
	if (error != IXGBE_SUCCESS)
		return (error);

	/* Free a PF-only VLVF slot without removing the PF's VFTA bit. */
	if (!enable && ixgbe_pf_owns_vlan(sc, tag))
		ixgbe_vf_vlan_release_pf_only(sc, tag);
	return (IXGBE_SUCCESS);
}

static void
ixgbe_vf_vlan_record(struct ixgbe_vf *vf, uint16_t tag, bool enable)
{
	u32 mask;

	mask = 1U << (tag & 0x1f);
	if (enable) {
		vf->vlans[tag >> 5] |= mask;
		vf->num_vlans++;
	} else {
		vf->vlans[tag >> 5] &= ~mask;
		vf->num_vlans--;
	}
}

static void
ixgbe_vf_configure_vmolr(struct ixgbe_softc *sc, struct ixgbe_vf *vf)
{
	struct ixgbe_hw *hw;
	uint32_t vmolr, vmvir;
	uint8_t xcast_mode;
	uint16_t tag;

	hw = &sc->hw;
	tag = vf->default_vlan;

	vmolr = IXGBE_READ_REG(hw, IXGBE_VMOLR(vf->pool));

	vmolr &= ~(IXGBE_VMOLR_UPE | IXGBE_VMOLR_ROMPE |
	    IXGBE_VMOLR_ROPE | IXGBE_VMOLR_MPE | IXGBE_VMOLR_VPE |
	    IXGBE_VMOLR_AUPE);

	/* Accept broadcasts. */
	vmolr |= IXGBE_VMOLR_BAM;

	if (tag == 0) {
		/* Accept non-vlan tagged traffic. */
		vmolr |= IXGBE_VMOLR_AUPE;

		/* Allow VM to tag outgoing traffic; no default tag. */
		vmvir = 0;
	} else {
		/* Require vlan-tagged traffic. */
		vmolr &= ~IXGBE_VMOLR_AUPE;

		/* Tag all traffic with provided vlan tag. */
		vmvir = (tag | IXGBE_VMVIR_VLANA_DEFAULT);
	}

	xcast_mode = vf->xcast_mode;
	if ((vf->api_ver == IXGBE_API_VER_UNKNOWN ||
	    vf->api_ver < IXGBE_API_VER_1_2) && vf->num_mc_hashes != 0)
		xcast_mode = IXGBEVF_XCAST_MODE_MULTI;
	switch (xcast_mode) {
	case IXGBEVF_XCAST_MODE_PROMISC:
		vmolr |= IXGBE_VMOLR_UPE;
		/* FALLTHROUGH */
	case IXGBEVF_XCAST_MODE_ALLMULTI:
		vmolr |= IXGBE_VMOLR_MPE;
		/* FALLTHROUGH */
	case IXGBEVF_XCAST_MODE_MULTI:
		vmolr |= IXGBE_VMOLR_ROMPE;
		break;
	case IXGBEVF_XCAST_MODE_NONE:
	default:
		break;
	}
	IXGBE_WRITE_REG(hw, IXGBE_VMOLR(vf->pool), vmolr);
	IXGBE_WRITE_REG(hw, IXGBE_VMVIR(vf->pool), vmvir);
} /* ixgbe_vf_configure_vmolr */

static void
ixgbe_vf_clear_vlans(struct ixgbe_softc *sc, struct ixgbe_vf *vf,
    bool clear_hw)
{
	uint32_t bits;
	int bit, index;

	for (index = 0; index < IXGBE_VFTA_SIZE; index++) {
		bits = vf->vlans[index];
		while (bits != 0) {
			bit = ffs(bits) - 1;
			if (clear_hw)
				(void)ixgbe_vf_vlan_hw_update(sc, vf,
				    index * 32 + bit, false);
			bits &= ~(1U << bit);
		}
	}
	bzero(vf->vlans, sizeof(vf->vlans));
	vf->num_vlans = 0;
}

static s32
ixgbe_vf_reset_vlan(struct ixgbe_softc *sc, struct ixgbe_vf *vf,
    bool clear_hw)
{
	s32 error;

	ixgbe_vf_clear_vlans(sc, vf, clear_hw);
	if (vf->default_vlan == 0) {
		/* VLAN 0 membership is implicit and not VF-removable. */
		error = ixgbe_vf_vlan_hw_update(sc, vf, 0, true);
	} else {
		error = ixgbe_vf_vlan_hw_update(sc, vf, vf->default_vlan, true);
		if (error == IXGBE_SUCCESS)
			ixgbe_vf_vlan_record(vf, vf->default_vlan, true);
	}
	ixgbe_vf_configure_vmolr(sc, vf);
	return (error);
}

static void
ixgbe_vf_clear_mac_filters(struct ixgbe_softc *sc, struct ixgbe_vf *vf,
    bool clear_hw)
{
	struct ixgbe_vf_mac_filter *filter;
	int i;

	for (i = 0; i < sc->num_vf_mac_filters; i++) {
		filter = &sc->vf_mac_filters[i];
		if (!filter->active || filter->pool != vf->pool)
			continue;
		if (clear_hw)
			(void)ixgbe_clear_rar(&sc->hw, filter->rar_index);
		filter->active = false;
		bzero(filter->mac, sizeof(filter->mac));
	}
	vf->num_mac_filters = 0;
}

static boolean_t
ixgbe_vf_frame_size_compatible(struct ixgbe_softc *sc, struct ixgbe_vf *vf)
{
	/*
	 * Frame size compatibility between PF and VF is only a problem on
	 * 82599-based cards.  X540 and later support any combination of jumbo
	 * frames on PFs and VFs.
	 */
	if (sc->hw.mac.type != ixgbe_mac_82599EB)
		return (true);

	switch (vf->api_ver) {
	case IXGBE_API_VER_1_0:
	case IXGBE_API_VER_UNKNOWN:
		/*
		 * On legacy (1.0 and older) VF versions, we don't support
		 * jumbo frames on either the PF or the VF.
		 */
		if (sc->max_frame_size > ETHER_MAX_LEN ||
		    vf->maximum_frame_size > ETHER_MAX_LEN)
			return (false);

		return (true);

		break;
	case IXGBE_API_VER_1_1:
	default:
		/*
		 * 1.1 or later VF versions always work if they aren't using
		 * jumbo frames.
		 */
		if (vf->maximum_frame_size <= ETHER_MAX_LEN)
			return (true);

		/*
		 * Jumbo frames only work with VFs if the PF is also using
		 * jumbo frames.
		 */
		if (sc->max_frame_size <= ETHER_MAX_LEN)
			return (true);

		return (false);
	}
} /* ixgbe_vf_frame_size_compatible */


static void
ixgbe_process_vf_reset(struct ixgbe_softc *sc, struct ixgbe_vf *vf)
{
	struct ixgbe_hw *hw;
	bool rebuild_mta;
	s32 error;
	int i, queue_count;

	hw = &sc->hw;
	vf->flags &= ~IXGBE_VF_CTS;
	rebuild_mta = vf->num_mc_hashes != 0;
	vf->xcast_mode = IXGBEVF_XCAST_MODE_NONE;
	vf->num_mc_hashes = 0;
	bzero(vf->mc_hash, sizeof(vf->mc_hash));
	error = ixgbe_vf_reset_vlan(sc, vf, true);
	if (error != IXGBE_SUCCESS)
		device_printf(sc->dev,
		    "VF %u default VLAN restore failed: %d\n",
		    vf->pool, error);
	if (rebuild_mta)
		ixgbe_iov_rebuild_mta(sc);

	ixgbe_vf_clear_mac_filters(sc, vf, true);
	ixgbe_clear_rar(hw, vf->rar_index);
	ixgbe_vf_set_anti_spoof(sc, vf);
	ixgbe_toggle_txdctl(hw, vf->pool);

	/* VFLR does not clear transmit head write-back state. */
	queue_count = ixgbe_vf_queues(sc->iov_mode);
	for (i = 0; i < queue_count; i++) {
		IXGBE_WRITE_REG(hw,
		    IXGBE_PVFTDWBAHn(queue_count, vf->pool, i), 0);
		IXGBE_WRITE_REG(hw,
		    IXGBE_PVFTDWBALn(queue_count, vf->pool, i), 0);
	}

	/* VFLR also leaves malicious-driver queue blocks asserted. */
	ixgbe_restore_mdd_vf(hw, vf->pool);

	vf->flags &= ~(IXGBE_VF_INIT_DONE | IXGBE_VF_MDD_BLOCKED |
	    IXGBE_VF_MDD_NOTIFY_PENDING);
	vf->api_ver = IXGBE_API_VER_UNKNOWN;
} /* ixgbe_process_vf_reset */


static void
ixgbe_vf_enable_transmit(struct ixgbe_softc *sc, struct ixgbe_vf *vf)
{
	struct ixgbe_hw *hw;
	uint32_t vf_index, vfte;

	hw = &sc->hw;

	vf_index = IXGBE_VF_INDEX(vf->pool);
	vfte = IXGBE_READ_REG(hw, IXGBE_VFTE(vf_index));
	if (vf->flags & IXGBE_VF_TRAFFIC_DISABLED)
		vfte &= ~IXGBE_VF_BIT(vf->pool);
	else
		vfte |= IXGBE_VF_BIT(vf->pool);
	IXGBE_WRITE_REG(hw, IXGBE_VFTE(vf_index), vfte);
} /* ixgbe_vf_enable_transmit */


static void
ixgbe_vf_set_rx_drop(struct ixgbe_softc *sc, struct ixgbe_vf *vf,
    bool enable)
{
	struct ixgbe_hw *hw;
	u32 qde;
	int i, queue_count;

	hw = &sc->hw;
	queue_count = ixgbe_vf_queues(sc->iov_mode);
	for (i = 0; i < queue_count; i++) {
		qde = IXGBE_QDE_WRITE |
		    (ixgbe_vf_que_index(sc->iov_mode, vf->pool, i) <<
		    IXGBE_QDE_IDX_SHIFT);
		if (enable) {
			qde |= IXGBE_QDE_ENABLE;
			if (vf->default_vlan != 0 &&
			    hw->mac.type >= ixgbe_mac_X550)
				qde |= IXGBE_QDE_HIDE_VLAN;
		}
		IXGBE_WRITE_REG(hw, IXGBE_QDE, qde);
		IXGBE_WRITE_FLUSH(hw);
	}
}

static void
ixgbe_vf_enable_receive(struct ixgbe_softc *sc, struct ixgbe_vf *vf)
{
	struct ixgbe_hw *hw;
	uint32_t vf_index, vfre;

	hw = &sc->hw;
	/* Keep one VF without receive descriptors from blocking its peers. */
	ixgbe_vf_set_rx_drop(sc, vf, true);

	vf_index = IXGBE_VF_INDEX(vf->pool);
	vfre = IXGBE_READ_REG(hw, IXGBE_VFRE(vf_index));
	if (!(vf->flags & IXGBE_VF_TRAFFIC_DISABLED) &&
	    ixgbe_vf_frame_size_compatible(sc, vf))
		vfre |= IXGBE_VF_BIT(vf->pool);
	else
		vfre &= ~IXGBE_VF_BIT(vf->pool);
	IXGBE_WRITE_REG(hw, IXGBE_VFRE(vf_index), vfre);
} /* ixgbe_vf_enable_receive */


static void
ixgbe_vf_reset_msg(struct ixgbe_softc *sc, struct ixgbe_vf *vf, uint32_t *msg)
{
	struct ixgbe_hw *hw;
	uint32_t ack;
	uint32_t resp[IXGBE_VF_PERMADDR_MSG_LEN];

	hw = &sc->hw;

	ixgbe_process_vf_reset(sc, vf);

	if (ixgbe_validate_mac_addr(vf->ether_addr) == 0) {
		ixgbe_set_rar(&sc->hw, vf->rar_index, vf->ether_addr,
		    vf->pool, true);
		ack = IXGBE_VT_MSGTYPE_SUCCESS;
	} else
		ack = IXGBE_VT_MSGTYPE_FAILURE;

	ixgbe_vf_enable_transmit(sc, vf);
	ixgbe_vf_enable_receive(sc, vf);

	vf->flags |= IXGBE_VF_CTS | IXGBE_VF_INIT_DONE;

	resp[0] = IXGBE_VF_RESET | ack;
	bcopy(vf->ether_addr, &resp[1], ETHER_ADDR_LEN);
	resp[3] = hw->mac.mc_filter_type;
	ixgbe_write_mbx(hw, resp, IXGBE_VF_PERMADDR_MSG_LEN, vf->pool);
} /* ixgbe_vf_reset_msg */


static void
ixgbe_vf_set_mac(struct ixgbe_softc *sc, struct ixgbe_vf *vf, uint32_t *msg)
{
	uint8_t *mac;

	mac = (uint8_t*)&msg[1];

	/* Check that the VF has permission to change the MAC address. */
	if (!(vf->flags & IXGBE_VF_CAP_MAC) && ixgbe_vf_mac_changed(vf, mac)) {
		ixgbe_send_vf_failure(sc, vf, msg[0]);
		return;
	}

	if (ixgbe_validate_mac_addr(mac) != 0) {
		ixgbe_send_vf_failure(sc, vf, msg[0]);
		return;
	}
	if (ixgbe_vf_mac_changed(vf, mac) &&
	    ixgbe_rar_mac_in_use(sc, mac, vf->rar_index)) {
		ixgbe_send_vf_failure(sc, vf, msg[0]);
		return;
	}

	bcopy(mac, vf->ether_addr, ETHER_ADDR_LEN);

	ixgbe_set_rar(&sc->hw, vf->rar_index, vf->ether_addr, vf->pool,
	    true);

	ixgbe_send_vf_success(sc, vf, msg[0]);
} /* ixgbe_vf_set_mac */


/*
 * VF multicast addresses are set by using the appropriate bit in
 * 1 of 128 32 bit addresses (4096 possible).
 */
static void
ixgbe_vf_set_mc_addr(struct ixgbe_softc *sc, struct ixgbe_vf *vf, u32 *msg)
{
	u16	*list = (u16*)&msg[1];
	int	entries;

	entries = (msg[0] & IXGBE_VT_MSGINFO_MASK) >>
	    IXGBE_VT_MSGINFO_SHIFT;
	entries = min(entries, IXGBE_MAX_VF_MC);

	bzero(vf->mc_hash, sizeof(vf->mc_hash));
	bcopy(list, vf->mc_hash, entries * sizeof(*list));
	vf->num_mc_hashes = entries;
	if (entries != 0 && vf->xcast_mode == IXGBEVF_XCAST_MODE_NONE)
		vf->xcast_mode = IXGBEVF_XCAST_MODE_MULTI;
	else if (entries == 0 &&
	    vf->xcast_mode == IXGBEVF_XCAST_MODE_MULTI)
		vf->xcast_mode = IXGBEVF_XCAST_MODE_NONE;
	ixgbe_iov_rebuild_mta(sc);
	ixgbe_vf_configure_vmolr(sc, vf);
	ixgbe_send_vf_success(sc, vf, msg[0]);
} /* ixgbe_vf_set_mc_addr */


static void
ixgbe_vf_set_vlan(struct ixgbe_softc *sc, struct ixgbe_vf *vf, uint32_t *msg)
{
	bool enable, present;
	s32 error;
	uint16_t tag;

	enable = IXGBE_VT_MSGINFO(msg[0]) != 0;
	tag = msg[1] & IXGBE_VLVF_VLANID_MASK;

	if (!(vf->flags & IXGBE_VF_CAP_VLAN) || vf->default_vlan != 0 ||
	    (msg[1] & ~IXGBE_VLVF_VLANID_MASK) != 0) {
		ixgbe_send_vf_failure(sc, vf, msg[0]);
		return;
	}

	/* It is illegal to enable vlan tag 0. */
	if (tag == 0 && enable) {
		ixgbe_send_vf_failure(sc, vf, msg[0]);
		return;
	}

	present = ixgbe_vf_owns_vlan(vf, tag);
	if (enable == present) {
		ixgbe_send_vf_success(sc, vf, msg[0]);
		return;
	}

	error = ixgbe_vf_vlan_hw_update(sc, vf, tag, enable);
	if (error != IXGBE_SUCCESS) {
		ixgbe_send_vf_failure(sc, vf, msg[0]);
		return;
	}
	ixgbe_vf_vlan_record(vf, tag, enable);
	ixgbe_send_vf_success(sc, vf, msg[0]);
} /* ixgbe_vf_set_vlan */


static void
ixgbe_vf_set_lpe(struct ixgbe_softc *sc, struct ixgbe_vf *vf, uint32_t *msg)
{
	struct ixgbe_hw *hw;
	uint32_t vf_max_size, pf_max_size, mhadd;

	hw = &sc->hw;
	vf_max_size = msg[1];

	if (vf_max_size < ETHER_CRC_LEN) {
		/* We intentionally ACK invalid LPE requests. */
		ixgbe_send_vf_success(sc, vf, msg[0]);
		return;
	}

	vf_max_size -= ETHER_CRC_LEN;

	if (vf_max_size > IXGBE_MAX_FRAME_SIZE) {
		/* We intentionally ACK invalid LPE requests. */
		ixgbe_send_vf_success(sc, vf, msg[0]);
		return;
	}

	vf->maximum_frame_size = vf_max_size;
	ixgbe_update_max_frame(sc, vf->maximum_frame_size);

	/*
	 * We might have to disable reception to this VF if the frame size is
	 * not compatible with the config on the PF.
	 */
	ixgbe_vf_enable_receive(sc, vf);

	mhadd = IXGBE_READ_REG(hw, IXGBE_MHADD);
	pf_max_size = (mhadd & IXGBE_MHADD_MFS_MASK) >> IXGBE_MHADD_MFS_SHIFT;

	if (pf_max_size < sc->max_frame_size) {
		mhadd &= ~IXGBE_MHADD_MFS_MASK;
		mhadd |= sc->max_frame_size << IXGBE_MHADD_MFS_SHIFT;
		IXGBE_WRITE_REG(hw, IXGBE_MHADD, mhadd);
	}

	ixgbe_send_vf_success(sc, vf, msg[0]);
} /* ixgbe_vf_set_lpe */


static void
ixgbe_vf_set_macvlan(struct ixgbe_softc *sc, struct ixgbe_vf *vf,
    uint32_t *msg)
{
	struct ixgbe_vf_mac_filter *filter, *free_filter;
	struct ixgbe_hw *hw;
	uint8_t *mac;
	int i, index;

	hw = &sc->hw;
	index = IXGBE_VT_MSGINFO(msg[0]);
	if (index == 0) {
		ixgbe_vf_clear_mac_filters(sc, vf, true);
		ixgbe_vf_set_anti_spoof(sc, vf);
		ixgbe_send_vf_success(sc, vf, msg[0]);
		return;
	}
	if (!(vf->flags & IXGBE_VF_CAP_MAC))
		goto failure;
	/* This hardware can anti-spoof only the VF's primary source MAC. */
	if ((vf->flags & IXGBE_VF_ANTI_SPOOF) != 0)
		goto failure;

	mac = (uint8_t *)&msg[1];
	if (ixgbe_validate_mac_addr(mac) != IXGBE_SUCCESS)
		goto failure;
	if (index == 1)
		ixgbe_vf_clear_mac_filters(sc, vf, true);
	if (bcmp(mac, vf->ether_addr, ETHER_ADDR_LEN) == 0) {
		ixgbe_vf_set_anti_spoof(sc, vf);
		ixgbe_send_vf_success(sc, vf, msg[0]);
		return;
	}

	free_filter = NULL;
	for (i = 0; i < sc->num_vf_mac_filters; i++) {
		filter = &sc->vf_mac_filters[i];
		if (!filter->active) {
			if (free_filter == NULL)
				free_filter = filter;
			continue;
		}
		if (bcmp(filter->mac, mac, ETHER_ADDR_LEN) != 0)
			continue;
		if (filter->pool != vf->pool)
			goto failure;
		ixgbe_send_vf_success(sc, vf, msg[0]);
		return;
	}
	if (vf->num_mac_filters >= IXGBE_MAX_VF_MAC_FILTERS ||
	    free_filter == NULL)
		goto failure;

	/* Reject collisions with PF, VF-primary, or other reserved RARs. */
	if (ixgbe_rar_mac_in_use(sc, mac, -1))
		goto failure;

	if (ixgbe_set_rar(hw, free_filter->rar_index, mac, vf->pool,
	    true) != IXGBE_SUCCESS)
		goto failure;
	free_filter->active = true;
	free_filter->pool = vf->pool;
	bcopy(mac, free_filter->mac, ETHER_ADDR_LEN);
	vf->num_mac_filters++;
	/* MAC anti-spoofing accepts only the primary address on this family. */
	ixgbe_vf_set_anti_spoof(sc, vf);
	ixgbe_send_vf_success(sc, vf, msg[0]);
	return;

failure:
	ixgbe_vf_set_anti_spoof(sc, vf);
	ixgbe_send_vf_failure(sc, vf, msg[0]);
} /* ixgbe_vf_set_macvlan */


static void
ixgbe_vf_api_negotiate(struct ixgbe_softc *sc, struct ixgbe_vf *vf,
    uint32_t *msg)
{
	switch (msg[1]) {
	case IXGBE_API_VER_1_0:
	case IXGBE_API_VER_1_1:
	case IXGBE_API_VER_1_2:
	case IXGBE_API_VER_1_3:
		vf->api_ver = msg[1];
		ixgbe_send_vf_success(sc, vf, msg[0]);
		break;
	default:
		vf->api_ver = IXGBE_API_VER_UNKNOWN;
		ixgbe_send_vf_failure(sc, vf, msg[0]);
		break;
	}
} /* ixgbe_vf_api_negotiate */

static void
ixgbe_vf_update_xcast_mode(struct ixgbe_softc *sc, struct ixgbe_vf *vf,
    uint32_t *msg)
{
	struct ixgbe_hw *hw;
	uint32_t mode;

	hw = &sc->hw;
	mode = msg[1];
	switch (vf->api_ver) {
	case IXGBE_API_VER_1_2:
		if (mode == IXGBEVF_XCAST_MODE_PROMISC)
			goto failure;
		break;
	case IXGBE_API_VER_1_3:
		break;
	default:
		goto failure;
	}
	if (mode > IXGBEVF_XCAST_MODE_PROMISC)
		goto failure;
	if (mode > IXGBEVF_XCAST_MODE_MULTI &&
	    !(vf->flags & IXGBE_VF_ALLOW_PROMISC))
		goto failure;
	if (mode == IXGBEVF_XCAST_MODE_PROMISC &&
	    (hw->mac.type <= ixgbe_mac_82599EB ||
	    !(IXGBE_READ_REG(hw, IXGBE_FCTRL) & IXGBE_FCTRL_UPE)))
		goto failure;

	vf->xcast_mode = mode;
	ixgbe_vf_configure_vmolr(sc, vf);
	msg[0] &= IXGBE_VT_MSG_MASK;
	msg[0] |= IXGBE_VT_MSGTYPE_SUCCESS | IXGBE_VT_MSGTYPE_CTS;
	msg[1] = mode;
	ixgbe_write_mbx(hw, msg, 2, vf->pool);
	return;

failure:
	ixgbe_send_vf_failure(sc, vf, msg[0]);
} /* ixgbe_vf_update_xcast_mode */


static void
ixgbe_vf_get_queues(struct ixgbe_softc *sc, struct ixgbe_vf *vf,
    uint32_t *msg)
{
	struct ixgbe_hw *hw;
	uint32_t resp[IXGBE_VF_GET_QUEUES_RESP_LEN];
	int num_queues;

	hw = &sc->hw;

	/* GET_QUEUES is not supported on pre-1.1 APIs. */
	switch (vf->api_ver) {
	case IXGBE_API_VER_1_0:
	case IXGBE_API_VER_UNKNOWN:
		ixgbe_send_vf_failure(sc, vf, msg[0]);
		return;
	}

	resp[0] = IXGBE_VF_GET_QUEUES | IXGBE_VT_MSGTYPE_SUCCESS |
	    IXGBE_VT_MSGTYPE_CTS;

	num_queues = ixgbe_vf_queues(sc->iov_mode);
	resp[IXGBE_VF_TX_QUEUES] = num_queues;
	resp[IXGBE_VF_RX_QUEUES] = num_queues;
	resp[IXGBE_VF_TRANS_VLAN] = (vf->default_vlan != 0);
	resp[IXGBE_VF_DEF_QUEUE] = 0;

	ixgbe_write_mbx(hw, resp, IXGBE_VF_GET_QUEUES_RESP_LEN, vf->pool);
} /* ixgbe_vf_get_queues */


static bool
ixgbe_process_vf_msg(if_ctx_t ctx, struct ixgbe_vf *vf, bool reset_pending)
{
	struct ixgbe_softc *sc = iflib_get_softc(ctx);
#ifdef KTR
	if_t ifp = iflib_get_ifp(ctx);
#endif
	struct ixgbe_hw *hw;
	uint32_t msg[IXGBE_VFMAILBOX_SIZE];
	bool cleanup_complete;
	int error;

	hw = &sc->hw;
	cleanup_complete = true;

	error = ixgbe_read_mbx(hw, msg, IXGBE_VFMAILBOX_SIZE, vf->pool);

	if (error != 0)
		return (false);
	/*
	 * Some devices do not clear VFMBMEM on VFLR.  Copy a pending request
	 * first because the VF posts its mailbox reset request after raising
	 * the reset event.
	 */
	if (reset_pending || msg[0] == IXGBE_VF_RESET)
		cleanup_complete =
		    ixgbe_clear_mbx(hw, vf->pool) == IXGBE_SUCCESS;
	/* The successful read ACKed the request; dispatch it even if not clear. */

	CTR3(KTR_MALLOC, "%s: received msg %x from %d", if_name(ifp),
	    msg[0], vf->pool);
	if (msg[0] == IXGBE_VF_RESET) {
		ixgbe_vf_reset_msg(sc, vf, msg);
		return (cleanup_complete);
	}
	/* Discard requests from the mailbox session invalidated by VFLR. */
	if (reset_pending)
		return (cleanup_complete);

	if (!(vf->flags & IXGBE_VF_CTS)) {
		ixgbe_send_vf_failure(sc, vf, msg[0]);
		return (true);
	}

	switch (msg[0] & IXGBE_VT_MSG_MASK) {
	case IXGBE_VF_SET_MAC_ADDR:
		ixgbe_vf_set_mac(sc, vf, msg);
		break;
	case IXGBE_VF_SET_MULTICAST:
		ixgbe_vf_set_mc_addr(sc, vf, msg);
		break;
	case IXGBE_VF_SET_VLAN:
		ixgbe_vf_set_vlan(sc, vf, msg);
		break;
	case IXGBE_VF_SET_LPE:
		ixgbe_vf_set_lpe(sc, vf, msg);
		break;
	case IXGBE_VF_SET_MACVLAN:
		ixgbe_vf_set_macvlan(sc, vf, msg);
		break;
	case IXGBE_VF_API_NEGOTIATE:
		ixgbe_vf_api_negotiate(sc, vf, msg);
		break;
	case IXGBE_VF_GET_QUEUES:
		ixgbe_vf_get_queues(sc, vf, msg);
		break;
	case IXGBE_VF_UPDATE_XCAST_MODE:
		ixgbe_vf_update_xcast_mode(sc, vf, msg);
		break;
	default:
		ixgbe_send_vf_failure(sc, vf, msg[0]);
	}
	return (true);
} /* ixgbe_process_vf_msg */

static void
ixgbe_cleanup_vf_mbx(struct ixgbe_softc *sc, struct ixgbe_vf *vf)
{
	struct ixgbe_hw *hw;
	sbintime_t now;

	hw = &sc->hw;
	if (ixgbe_clear_mbx(hw, vf->pool) == IXGBE_SUCCESS) {
		vf->flags &= ~IXGBE_VF_MBX_CLEANUP;
		return;
	}

	now = getsbinuptime();
	if (now < vf->mbx_cleanup_deadline)
		return;

	/*
	 * A functioning VF posts VFREQ within the grace interval.  Ownership
	 * still held after that interval is residue from the old reset epoch.
	 * The force-clear helper rechecks VFREQ before asserting RVFU.
	 */
	if (ixgbe_force_clear_mbx_pf(hw, vf->pool) == IXGBE_SUCCESS) {
		vf->flags &= ~IXGBE_VF_MBX_CLEANUP;
		return;
	}

	/* A request raced the force-clear attempt; give it another interval. */
	vf->mbx_cleanup_deadline = now + IXGBE_VF_MBX_CLEANUP_GRACE;
}

static void
ixgbe_notify_vf_mdd_reset(struct ixgbe_softc *sc, struct ixgbe_vf *vf)
{
	u32 msg;
	int error;

	msg = IXGBE_PF_CONTROL_MSG | IXGBE_VT_MSGTYPE_FAILURE;
	error = ixgbe_write_mbx(&sc->hw, &msg, 1, vf->pool);
	if (error == IXGBE_SUCCESS) {
		vf->flags &= ~IXGBE_VF_MDD_NOTIFY_PENDING;
		return;
	}

	/* The periodic admin pass retries after any mailbox collision. */
	if (ratecheck(&vf->last_mdd_log, &ixgbe_mdd_log_interval))
		device_printf(sc->dev,
		    "could not notify VF %u of malicious-driver reset: %d; "
		    "will retry\n", vf->pool, error);
}

static void
ixgbe_handle_mdd(struct ixgbe_softc *sc)
{
	struct ixgbe_hw *hw;
	struct ixgbe_vf *vf;
	bool pf_reset;
	u32 rx_cause, tx_cause;
	u32 vf_bitmap[2] = {};
	int pool;

	hw = &sc->hw;
	ixgbe_mdd_event(hw, vf_bitmap);
	if (vf_bitmap[0] != 0 || vf_bitmap[1] != 0) {
		tx_cause = IXGBE_READ_REG(hw, IXGBE_LVMMC_TX);
		rx_cause = IXGBE_READ_REG(hw, IXGBE_LVMMC_RX);
	} else {
		tx_cause = 0;
		rx_cause = 0;
	}
	pf_reset = false;
	for (pool = 0; pool < 64; pool++) {
		if ((vf_bitmap[pool / 32] & (1U << (pool % 32))) == 0)
			continue;

		if (pool == sc->pool) {
			if (sc->iov_pf_mdd_reset_pending)
				continue;
			sc->iov_pf_mdd_reset_pending = true;
			pf_reset = true;
			if (ratecheck(&sc->iov_last_mdd_log,
			    &ixgbe_mdd_log_interval))
				device_printf(sc->dev,
				    "malicious-driver event on PF pool "
				    "(last tx cause %#x, last rx cause %#x); "
				    "resetting PF\n",
				    tx_cause, rx_cause);
			continue;
		}
		if (pool >= sc->num_vfs) {
			ixgbe_restore_mdd_vf(hw, pool);
			continue;
		}
		vf = &sc->vfs[pool];
		if (!(vf->flags & IXGBE_VF_ACTIVE)) {
			ixgbe_restore_mdd_vf(hw, pool);
			continue;
		}

		if ((vf->flags & IXGBE_VF_MDD_BLOCKED) != 0)
			continue;
		if (ratecheck(&vf->last_mdd_log, &ixgbe_mdd_log_interval))
			device_printf(sc->dev,
			    "malicious-driver event from VF %u "
			    "(last tx cause %#x, last rx cause %#x); "
			    "requesting VF reset\n",
			    vf->pool, tx_cause, rx_cause);

		/*
		 * Keep both the pool gate and the per-queue WQBR block asserted.
		 * PFVFTE stops packet-data fetches but can still allow descriptor
		 * fetches into the internal queue, so releasing WQBR here would
		 * leave a hostile VF able to retrigger MDD before it resets.
		 * ixgbe_process_vf_reset() releases WQBR in the new reset epoch.
		 */
		vf->flags |= IXGBE_VF_MDD_BLOCKED;
		vf->flags &= ~IXGBE_VF_CTS;
		ixgbe_vf_enable_transmit(sc, vf);
		ixgbe_vf_enable_receive(sc, vf);
		IXGBE_WRITE_FLUSH(hw);
		vf->flags |= IXGBE_VF_MDD_NOTIFY_PENDING;
	}

	if (pf_reset) {
		iflib_request_reset(sc->ctx);
		iflib_admin_intr_deferred(sc->ctx);
	}
}

/* Tasklet for handling VF -> PF mailbox messages */
void
ixgbe_handle_mbx(void *context)
{
	if_ctx_t ctx = context;
	struct ixgbe_softc *sc = iflib_get_softc(ctx);
	struct ixgbe_hw *hw;
	struct ixgbe_vf *vf;
	bool cleanup_pending, mbx_activity, reset_pending, reset_seen;
	int i;

	hw = &sc->hw;
	cleanup_pending = false;
	ixgbe_handle_mdd(sc);

	for (i = 0; i < sc->num_vfs; i++) {
		vf = &sc->vfs[i];
		mbx_activity = false;

		if (vf->flags & IXGBE_VF_ACTIVE) {
			reset_seen = hw->mbx.ops[vf->pool].check_for_rst(hw,
			    vf->pool) == 0;
			if (reset_seen) {
				vf->flags |= IXGBE_VF_MBX_CLEANUP;
				vf->mbx_cleanup_deadline = getsbinuptime() +
				    IXGBE_VF_MBX_CLEANUP_GRACE;
				ixgbe_process_vf_reset(sc, vf);
			}
			reset_pending =
			    (vf->flags & IXGBE_VF_MBX_CLEANUP) != 0;

			if (hw->mbx.ops[vf->pool].check_for_msg(hw,
			    vf->pool) == 0) {
				mbx_activity = true;
				if (ixgbe_process_vf_msg(ctx, vf, reset_pending))
					vf->flags &= ~IXGBE_VF_MBX_CLEANUP;
			}
			if (reset_pending &&
			    (vf->flags & IXGBE_VF_MBX_CLEANUP) != 0)
				ixgbe_cleanup_vf_mbx(sc, vf);

			if (hw->mbx.ops[vf->pool].check_for_ack(hw,
			    vf->pool) == 0) {
				mbx_activity = true;
				ixgbe_process_vf_ack(sc, vf);
			}

			/* Do not overwrite a response from this mailbox pass. */
			if (!mbx_activity &&
			    (vf->flags & IXGBE_VF_MDD_NOTIFY_PENDING) != 0)
				ixgbe_notify_vf_mdd_reset(sc, vf);

			if (vf->flags & IXGBE_VF_MBX_CLEANUP)
				cleanup_pending = true;
		}
	}
	sc->iov_mbx_cleanup_pending = cleanup_pending;
} /* ixgbe_handle_mbx */

/*
 * VFREQ, VFACK, a bare VFLR, and WQBR can remain latched without a usable
 * shared mailbox interrupt across a PF stop/restart or an MDD mask interval.
 * Sample their aggregate registers from the periodic admin pass so work does
 * not depend on another edge.  Pending asynchronous MDD notifications use the
 * same pass to retry after the VF wins a mailbox collision.
 */
bool
ixgbe_mbx_pending(struct ixgbe_softc *sc)
{
	struct ixgbe_hw *hw;
	uint32_t active_mbx[4], active_rst[2], events, vf_mdd[2];
	int i, index;

	if (sc->num_vfs == 0)
		return (false);

	bzero(active_mbx, sizeof(active_mbx));
	bzero(active_rst, sizeof(active_rst));
	bzero(vf_mdd, sizeof(vf_mdd));
	for (i = 0; i < sc->num_vfs; i++) {
		if ((sc->vfs[i].flags & IXGBE_VF_ACTIVE) == 0)
			continue;
		if ((sc->vfs[i].flags & IXGBE_VF_MDD_NOTIFY_PENDING) != 0)
			return (true);
		index = IXGBE_PFMBICR_INDEX(sc->vfs[i].pool);
		active_mbx[index] |=
		    IXGBE_PFMBICR_VFREQ_VF1 <<
		    IXGBE_PFMBICR_SHIFT(sc->vfs[i].pool);
		active_mbx[index] |=
		    IXGBE_PFMBICR_VFACK_VF1 <<
		    IXGBE_PFMBICR_SHIFT(sc->vfs[i].pool);
		index = IXGBE_PFVFLRE_INDEX(sc->vfs[i].pool);
		active_rst[index] |=
		    1U << IXGBE_PFVFLRE_SHIFT(sc->vfs[i].pool);
	}

	hw = &sc->hw;
	for (index = 0; index < nitems(active_mbx); index++) {
		if (active_mbx[index] != 0 &&
		    (IXGBE_READ_REG(hw, IXGBE_PFMBICR(index)) &
		    active_mbx[index]) != 0)
			return (true);
	}
	for (index = 0; index < nitems(active_rst); index++) {
		if (active_rst[index] == 0)
			continue;
		switch (hw->mac.type) {
		case ixgbe_mac_82599EB:
			events = IXGBE_READ_REG(hw, IXGBE_PFVFLRE(index));
			break;
		case ixgbe_mac_X540:
		case ixgbe_mac_X550:
		case ixgbe_mac_X550EM_x:
		case ixgbe_mac_X550EM_a:
			events = IXGBE_READ_REG(hw, IXGBE_PFVFLREC(index));
			break;
		default:
			return (false);
		}
		if ((events & active_rst[index]) != 0)
			return (true);
	}
	ixgbe_mdd_event(hw, vf_mdd);
	/* A fenced VF retains WQBR until reset; do not reschedule for it. */
	for (i = 0; i < sc->num_vfs; i++) {
		if ((sc->vfs[i].flags & IXGBE_VF_MDD_BLOCKED) != 0)
			vf_mdd[i / 32] &= ~(1U << (i % 32));
	}
	if (sc->iov_pf_mdd_reset_pending)
		vf_mdd[sc->pool / 32] &= ~(1U << (sc->pool % 32));
	if (vf_mdd[0] != 0 || vf_mdd[1] != 0)
		return (true);
	return (false);
}

int
ixgbe_iov_validate(struct ixgbe_softc *sc, u16 num_vfs)
{
	int mode, pool, queue_count;

	if (!(sc->feat_cap & IXGBE_FEATURE_SRIOV))
		return (ENXIO);
	if (num_vfs == 0)
		return (EINVAL);
	if (sc->vfs != NULL || sc->vf_mac_filters != NULL ||
	    (sc->feat_en & IXGBE_FEATURE_SRIOV))
		return (EBUSY);
	if (sc->intr_type != IFLIB_INTR_MSIX) {
		device_printf(sc->dev, "SR-IOV requires MSI-X\n");
		return (ENOTSUP);
	}

	/*
	 * We've got to reserve a VM's worth of queues for the PF,
	 * thus we go into "64 VF mode" if 32+ VFs are requested.
	 * With 64 VFs, you can only have two queues per VF.
	 * With 32 VFs, you can have up to four queues per VF.
	 */
	if (num_vfs >= IXGBE_32_VM)
		mode = IXGBE_64_VM;
	else
		mode = IXGBE_32_VM;
	queue_count = ixgbe_vf_queues(mode);
	if (sc->num_rx_queues > queue_count ||
	    sc->num_tx_queues > queue_count) {
		device_printf(sc->dev,
		    "SR-IOV mode supports %d PF queues, but %d RX and %d TX "
		    "queues are allocated\n", queue_count, sc->num_rx_queues,
		    sc->num_tx_queues);
		return (ENOSPC);
	}

	/* Again, reserving 1 VM's worth of queues for the PF */
	pool = mode - 1;
	if (num_vfs > pool || num_vfs >= IXGBE_64_VM)
		return (ENOSPC);
	return (0);
}

int
ixgbe_if_iov_init(if_ctx_t ctx, u16 num_vfs, const nvlist_t *config)
{
	struct ixgbe_softc *sc;
	int i, num_filters, retval;

	(void)config;
	sc = iflib_get_softc(ctx);
	retval = ixgbe_iov_validate(sc, num_vfs);
	if (retval != 0)
		return (retval);

	if (num_vfs >= IXGBE_32_VM)
		sc->iov_mode = IXGBE_64_VM;
	else
		sc->iov_mode = IXGBE_32_VM;
	sc->pool = sc->iov_mode - 1;

	sc->vfs = malloc(sizeof(*sc->vfs) * num_vfs, M_IXGBE_SRIOV,
	    M_NOWAIT | M_ZERO);

	if (sc->vfs == NULL) {
		retval = ENOMEM;
		goto err_init_iov;
	}
	num_filters = sc->hw.mac.num_rar_entries - num_vfs - 1 -
	    IXGBE_MAX_PF_MAC_FILTERS;
	if (num_filters > 0) {
		sc->vf_mac_filters = mallocarray(num_filters,
		    sizeof(*sc->vf_mac_filters), M_IXGBE_SRIOV,
		    M_NOWAIT | M_ZERO);
		if (sc->vf_mac_filters != NULL) {
			sc->num_vf_mac_filters = num_filters;
			for (i = 0; i < num_filters; i++)
				sc->vf_mac_filters[i].rar_index =
				    IXGBE_MAX_PF_MAC_FILTERS + 1 + i;
		} else
			device_printf(sc->dev,
			    "could not allocate VF secondary MAC filters; "
			    "SET_MACVLAN will be unavailable\n");
	}

	sc->num_vfs = num_vfs;
	sc->iov_mbx_cleanup_pending = false;
	sc->iov_pf_mdd_reset_pending = false;
	ixgbe_init_mbx_params_pf(&sc->hw);

	sc->feat_en |= IXGBE_FEATURE_SRIOV;

	return (retval);

err_init_iov:
	free(sc->vf_mac_filters, M_IXGBE_SRIOV);
	sc->vf_mac_filters = NULL;
	sc->num_vf_mac_filters = 0;
	free(sc->vfs, M_IXGBE_SRIOV);
	sc->vfs = NULL;
	sc->num_vfs = 0;
	sc->pool = 0;
	sc->iov_mode = IXGBE_NO_VM;
	sc->iov_mbx_cleanup_pending = false;
	sc->iov_pf_mdd_reset_pending = false;

	return (retval);
} /* ixgbe_if_iov_init */

void
ixgbe_if_iov_uninit(if_ctx_t ctx)
{
	struct ixgbe_hw *hw;
	struct ixgbe_softc *sc;
	uint32_t pf_reg, vf_reg;
	int error, i, iov_pos;
	u16 iov_ctl;

	sc = iflib_get_softc(ctx);
	hw = &sc->hw;
	ixgbe_disable_mdd(hw);

	/* Enable rx/tx for the PF and disable it for all VFs. */
	pf_reg = IXGBE_VF_INDEX(sc->pool);
	IXGBE_WRITE_REG(hw, IXGBE_VFRE(pf_reg), IXGBE_VF_BIT(sc->pool));
	IXGBE_WRITE_REG(hw, IXGBE_VFTE(pf_reg), IXGBE_VF_BIT(sc->pool));

	if (pf_reg == 0)
		vf_reg = 1;
	else
		vf_reg = 0;
	IXGBE_WRITE_REG(hw, IXGBE_VFRE(vf_reg), 0);
	IXGBE_WRITE_REG(hw, IXGBE_VFTE(vf_reg), 0);
	IXGBE_WRITE_FLUSH(hw);

	/*
	 * pci_iov(4) normally clears VF Enable after this callback returns,
	 * but iflib's restart transaction reuses the PF queues first.  Disable
	 * the VFs here and allow outstanding transactions to drain before the
	 * queue layout changes.
	 */
	error = pci_find_extcap(sc->dev, PCIZ_SRIOV, &iov_pos);
	if (error == 0) {
		iov_ctl = pci_read_config(sc->dev,
		    iov_pos + PCIR_SRIOV_CTL, 2);
		iov_ctl &= ~(PCIM_SRIOV_VF_EN | PCIM_SRIOV_VF_MSE);
		pci_write_config(sc->dev, iov_pos + PCIR_SRIOV_CTL,
		    iov_ctl, 2);
		pause("ixiov", MAX(1, howmany(hz, 10)));
	} else
		device_printf(sc->dev,
		    "could not disable PCI SR-IOV before queue reuse: %d\n",
		    error);

	for (i = 0; i < sc->num_vfs; i++) {
		if (!(sc->vfs[i].flags & IXGBE_VF_ACTIVE))
			continue;
		ixgbe_vf_set_rx_drop(sc, &sc->vfs[i], false);
		ixgbe_vf_clear_mac_filters(sc, &sc->vfs[i], true);
		sc->vfs[i].flags &= ~IXGBE_VF_ANTI_SPOOF;
		ixgbe_vf_set_anti_spoof(sc, &sc->vfs[i]);
	}
	if (hw->mac.ops.set_ethertype_anti_spoofing != NULL) {
		IXGBE_WRITE_REG(hw, IXGBE_ETQF(IXGBE_ETQF_FILTER_LLDP), 0);
		IXGBE_WRITE_REG(hw, IXGBE_ETQF(IXGBE_ETQF_FILTER_FC), 0);
	}

	IXGBE_WRITE_REG(hw, IXGBE_VT_CTL, 0);

	sc->num_vfs = 0;
	ixgbe_iov_rebuild_mta(sc);
	free(sc->vf_mac_filters, M_IXGBE_SRIOV);
	sc->vf_mac_filters = NULL;
	sc->num_vf_mac_filters = 0;
	free(sc->vfs, M_IXGBE_SRIOV);
	sc->vfs = NULL;
	sc->feat_en &= ~IXGBE_FEATURE_SRIOV;
	sc->pool = 0;
	sc->iov_mode = IXGBE_NO_VM;
	ixgbe_align_all_queue_indices(sc);
	sc->iov_vfta_valid = false;
	sc->iov_vlan_promisc = false;
	sc->iov_mbx_cleanup_pending = false;
	sc->iov_pf_mdd_reset_pending = false;
	(void)ixgbe_clear_vfta(hw);
	ixgbe_setup_vlan_hw_support(ctx);
} /* ixgbe_if_iov_uninit */

static s32
ixgbe_init_vf(struct ixgbe_softc *sc, struct ixgbe_vf *vf)
{
	struct ixgbe_hw *hw;
	uint32_t vf_index, pfmbimr;
	s32 error;

	hw = &sc->hw;
	vf->flags &= ~(IXGBE_VF_INIT_DONE | IXGBE_VF_MDD_BLOCKED |
	    IXGBE_VF_MDD_NOTIFY_PENDING);

	if (!(vf->flags & IXGBE_VF_ACTIVE))
		return (IXGBE_SUCCESS);

	vf_index = IXGBE_VF_INDEX(vf->pool);
	pfmbimr = IXGBE_READ_REG(hw, IXGBE_PFMBIMR(vf_index));
	pfmbimr |= IXGBE_VF_BIT(vf->pool);
	IXGBE_WRITE_REG(hw, IXGBE_PFMBIMR(vf_index), pfmbimr);

	vf->xcast_mode = IXGBEVF_XCAST_MODE_NONE;
	vf->api_ver = IXGBE_API_VER_UNKNOWN;
	vf->num_mc_hashes = 0;
	bzero(vf->mc_hash, sizeof(vf->mc_hash));
	ixgbe_vf_clear_mac_filters(sc, vf, false);
	error = ixgbe_vf_reset_vlan(sc, vf, false);
	if (error != IXGBE_SUCCESS)
		return (error);

	if (ixgbe_validate_mac_addr(vf->ether_addr) == 0) {
		ixgbe_set_rar(&sc->hw, vf->rar_index,
		    vf->ether_addr, vf->pool, true);
	}
	ixgbe_vf_set_anti_spoof(sc, vf);

	vf->flags |= IXGBE_VF_INIT_DONE;
	return (IXGBE_SUCCESS);
} /* ixgbe_init_vf */

static void
ixgbe_activate_vf(struct ixgbe_softc *sc, struct ixgbe_vf *vf)
{
	if ((vf->flags & (IXGBE_VF_ACTIVE | IXGBE_VF_INIT_DONE)) !=
	    (IXGBE_VF_ACTIVE | IXGBE_VF_INIT_DONE) ||
	    (vf->flags & IXGBE_VF_TRAFFIC_DISABLED))
		return;

	ixgbe_vf_enable_transmit(sc, vf);
	ixgbe_vf_enable_receive(sc, vf);
	ixgbe_send_vf_msg(&sc->hw, vf, IXGBE_PF_CONTROL_MSG);
} /* ixgbe_activate_vf */

void
ixgbe_initialize_iov(struct ixgbe_softc *sc)
{
	struct ixgbe_hw *hw = &sc->hw;
	uint32_t mrqc, mtqc, vt_ctl, vf_reg, gcr_ext, gpie;
	int i;

	if (sc->iov_mode == IXGBE_NO_VM)
		return;
	sc->iov_pf_mdd_reset_pending = false;

	/* RMW appropriate registers based on IOV mode */
	/* Read... */
	mrqc = IXGBE_READ_REG(hw, IXGBE_MRQC);
	gcr_ext = IXGBE_READ_REG(hw, IXGBE_GCR_EXT);
	gpie = IXGBE_READ_REG(hw, IXGBE_GPIE);
	/* Modify... */
	mrqc &= ~IXGBE_MRQC_MRQE_MASK;
	mtqc = IXGBE_MTQC_VT_ENA;      /* No initial MTQC read needed */
	gcr_ext |= IXGBE_GCR_EXT_MSIX_EN;
	gcr_ext &= ~IXGBE_GCR_EXT_VT_MODE_MASK;
	gpie &= ~IXGBE_GPIE_VTMODE_MASK;
	switch (sc->iov_mode) {
	case IXGBE_64_VM:
		mrqc |= IXGBE_MRQC_VMDQRSS64EN;
		mtqc |= IXGBE_MTQC_64VF;
		gcr_ext |= IXGBE_GCR_EXT_VT_MODE_64;
		gpie |= IXGBE_GPIE_VTMODE_64;
		break;
	case IXGBE_32_VM:
		mrqc |= IXGBE_MRQC_VMDQRSS32EN;
		mtqc |= IXGBE_MTQC_32VF;
		gcr_ext |= IXGBE_GCR_EXT_VT_MODE_32;
		gpie |= IXGBE_GPIE_VTMODE_32;
		break;
	default:
		panic("Unexpected SR-IOV mode %d", sc->iov_mode);
	}
	/* Write... */
	IXGBE_WRITE_REG(hw, IXGBE_MRQC, mrqc);
	IXGBE_WRITE_REG(hw, IXGBE_MTQC, mtqc);
	IXGBE_WRITE_REG(hw, IXGBE_GCR_EXT, gcr_ext);
	IXGBE_WRITE_REG(hw, IXGBE_GPIE, gpie);

	/* Enable rx/tx for the PF. */
	vf_reg = IXGBE_VF_INDEX(sc->pool);
	IXGBE_WRITE_REG(hw, IXGBE_VFRE(vf_reg), IXGBE_VF_BIT(sc->pool));
	IXGBE_WRITE_REG(hw, IXGBE_VFTE(vf_reg), IXGBE_VF_BIT(sc->pool));

	/* Allow VM-to-VM communication. */
	IXGBE_WRITE_REG(hw, IXGBE_PFDTXGSWC, IXGBE_PFDTXGSWC_VT_LBEN);

	vt_ctl = IXGBE_VT_CTL_VT_ENABLE | IXGBE_VT_CTL_REPLEN;
	vt_ctl |= (sc->pool << IXGBE_VT_CTL_POOL_SHIFT);
	IXGBE_WRITE_REG(hw, IXGBE_VT_CTL, vt_ctl);

	for (i = 0; i < sc->num_vfs; i++) {
		if (ixgbe_init_vf(sc, &sc->vfs[i]) != IXGBE_SUCCESS)
			device_printf(sc->dev,
			    "VF %d default VLAN restore failed\n", i);
	}
} /* ixgbe_initialize_iov */

void
ixgbe_activate_vfs(struct ixgbe_softc *sc)
{
	int i;

	for (i = 0; i < sc->num_vfs; i++)
		ixgbe_activate_vf(sc, &sc->vfs[i]);
} /* ixgbe_activate_vfs */


/* Check the max frame setting of all active VF's */
void
ixgbe_recalculate_max_frame(struct ixgbe_softc *sc)
{
	struct ixgbe_vf *vf;

	for (int i = 0; i < sc->num_vfs; i++) {
		vf = &sc->vfs[i];
		if (vf->flags & IXGBE_VF_ACTIVE)
			ixgbe_update_max_frame(sc, vf->maximum_frame_size);
	}
} /* ixgbe_recalculate_max_frame */

int
ixgbe_if_iov_vf_add(if_ctx_t ctx, u16 vfnum, const nvlist_t *config)
{
	struct ixgbe_softc *sc;
	struct ixgbe_vf *vf;
	const void *mac;
	uint8_t mac_addr[ETHER_ADDR_LEN];
	uint64_t configured_vlan;
	uint16_t vlan;
	s32 error;

	sc = iflib_get_softc(ctx);

	KASSERT(vfnum < sc->num_vfs, ("VF index %d is out of range %d",
	    vfnum, sc->num_vfs));

	vf = &sc->vfs[vfnum];
	if (vf->flags & IXGBE_VF_ACTIVE)
		return (EBUSY);
	bzero(vf, sizeof(*vf));

	configured_vlan = nvlist_get_number(config, "vlan");
	if (configured_vlan > VF_VLAN_TRUNK)
		return (EINVAL);
	vlan = configured_vlan;
	if (vlan == 0)
		return (ENOTSUP);
	if (vlan == VF_VLAN_TRUNK)
		vlan = 0;

	vf->pool = vfnum;

	/* Allocate VF-primary RARs from the top, away from PF filters. */
	vf->rar_index = sc->hw.mac.num_rar_entries - (vfnum + 1);
	vf->default_vlan = vlan;
	vf->maximum_frame_size = ETHER_MAX_LEN;
	ixgbe_update_max_frame(sc, vf->maximum_frame_size);
	if (nvlist_get_bool(config, "mac-anti-spoof"))
		vf->flags |= IXGBE_VF_ANTI_SPOOF;
	if (nvlist_get_bool(config, "allow-promisc"))
		vf->flags |= IXGBE_VF_ALLOW_PROMISC;

	if (nvlist_exists_binary(config, "mac-addr")) {
		mac = nvlist_get_binary(config, "mac-addr", NULL);
		bcopy(mac, mac_addr, sizeof(mac_addr));
		if (ixgbe_validate_mac_addr(mac_addr) != IXGBE_SUCCESS)
			return (EINVAL);
		if (ixgbe_rar_mac_in_use(sc, mac_addr, vf->rar_index))
			return (EADDRINUSE);
		bcopy(mac_addr, vf->ether_addr, ETHER_ADDR_LEN);
		if (nvlist_get_bool(config, "allow-set-mac"))
			vf->flags |= IXGBE_VF_CAP_MAC;
	} else
		/*
		 * If the administrator has not specified a MAC address then
		 * we must allow the VF to choose one.
		 */
		vf->flags |= IXGBE_VF_CAP_MAC;
	if (vf->default_vlan == 0)
		vf->flags |= IXGBE_VF_CAP_VLAN;

	vf->flags |= IXGBE_VF_ACTIVE;

	error = ixgbe_init_vf(sc, vf);
	if (error != IXGBE_SUCCESS) {
		vf->flags &= ~IXGBE_VF_ACTIVE;
		vf->default_vlan = 0;
		ixgbe_vf_clear_vlans(sc, vf, true);
		return (ENOSPC);
	}
	ixgbe_activate_vf(sc, vf);

	return (0);
} /* ixgbe_if_iov_vf_add */

#else

void
ixgbe_handle_mbx(void *context)
{
	UNREFERENCED_PARAMETER(context);
} /* ixgbe_handle_mbx */

bool
ixgbe_mbx_pending(struct ixgbe_softc *sc)
{
	UNREFERENCED_PARAMETER(sc);
	return (false);
}

#endif
