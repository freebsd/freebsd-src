/*-
 * Atlantic 2 (AQC113/114/115/116) firmware operations for aq(4).
 *
 * Adapted from the OpenBSD/NetBSD if_aq driver:
 *
 * Copyright (c) 2021 Jonathan Matthew <jonathan@d14n.org>
 * Copyright (c) 2021 Mike Larkin <mlarkin@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/endian.h>
#include <net/ethernet.h>

#include "aq_common.h"
#include "aq_hw.h"
#include "aq_hw_llh.h"
#include "aq2_hw.h"
#include "aq_fw.h"
#include "aq_dbg.h"

static int aq2_fw_reset(struct aq_hw *hw);
static int aq2_fw_set_mode(struct aq_hw *hw, enum aq_hw_fw_mpi_state mode,
    enum aq_fw_link_speed speed);
static int aq2_fw_get_mode(struct aq_hw *hw, enum aq_hw_fw_mpi_state *mode,
    enum aq_fw_link_speed *speed, enum aq_fw_link_fc *fc);
static int aq2_fw_get_mac_addr(struct aq_hw *hw, uint8_t *mac);
static int aq2_fw_get_stats(struct aq_hw *hw, struct aq_hw_stats *stats);
static int aq2_fw_get_temp(struct aq_hw *hw, int *temp_mc);
static int aq2_fw_get_thermal_limit(struct aq_hw *hw, int *limit_mc);

/* Coherent OUT-window read, bracketed by the transaction id. */
static int
aq2_fw_interface_buffer_read(struct aq_hw *hw, uint32_t reg0, uint32_t *data0,
    uint32_t size0)
{
	uint32_t tid0, tid1, reg, *data, size;
	int timo;

	for (timo = 10000; timo > 0; timo--) {
		tid0 = AQ_READ_REG(hw, AQ2_FW_INTERFACE_OUT_TRANSACTION_ID_REG);
		if (((tid0 & AQ2_FW_INTERFACE_OUT_TRANSACTION_ID_A) >>
		    AQ2_FW_INTERFACE_OUT_TRANSACTION_ID_A_S) !=
		    ((tid0 & AQ2_FW_INTERFACE_OUT_TRANSACTION_ID_B) >>
		    AQ2_FW_INTERFACE_OUT_TRANSACTION_ID_B_S)) {
			DELAY(10);
			continue;
		}

		/* size0 is a 4-byte multiple: full register-width reads. */
		for (reg = reg0, data = data0, size = size0;
		    size >= 4; reg += 4, data++, size -= 4)
			*data = AQ_READ_REG(hw, reg);

		tid1 = AQ_READ_REG(hw, AQ2_FW_INTERFACE_OUT_TRANSACTION_ID_REG);
		if (tid0 == tid1)
			break;
	}
	if (timo == 0) {
		device_printf(hw->dev, "A2 interface buffer read timeout\n");
		return (ETIMEDOUT);
	}
	return (0);
}

/* Boot the A2 firmware and select the A2 firmware ops. */
int
aq2_fw_reboot(struct aq_hw *hw)
{
	uint32_t v, filter_caps[3];
	int timo, err;
	const char *iface;

	hw->fw_ops = &aq2_fw_ops;

	AQ_WRITE_REG(hw, AQ2_MCP_HOST_REQ_INT_CLR_REG, 1);
	AQ_WRITE_REG(hw, AQ2_MIF_BOOT_REG, 1);	/* reboot request */
	for (timo = 20000; timo > 0; timo--) {
		v = AQ_READ_REG(hw, AQ2_MIF_BOOT_REG);
		if ((v & AQ2_MIF_BOOT_BOOT_STARTED) && v != 0xffffffff)
			break;
		DELAY(10);
	}
	if (timo <= 0) {
		device_printf(hw->dev, "A2 firmware reboot timeout\n");
		return (ETIMEDOUT);
	}
	trace(hw, dbg_init, "aq2> F/W boot started, %d ms", (20000 - timo) / 100);

	for (timo = 200000; timo > 0; timo--) {
		v = AQ_READ_REG(hw, AQ2_MIF_BOOT_REG);
		if (v & AQ2_MIF_BOOT_COMPLETE)
			break;
		v = AQ_READ_REG(hw, AQ2_MCP_HOST_REQ_INT_REG);
		if (v & AQ2_MCP_HOST_REQ_INT_READY)
			break;
		DELAY(10);
	}
	if (timo <= 0) {
		device_printf(hw->dev, "A2 firmware restart timeout\n");
		return (ETIMEDOUT);
	}
	trace(hw, dbg_init, "aq2> F/W boot complete, %d ms",
	    (200000 - timo) / 100);

	v = AQ_READ_REG(hw, AQ2_MIF_BOOT_REG);
	if (v & AQ2_MIF_BOOT_FAILED) {
		device_printf(hw->dev, "A2 firmware restart failed\n");
		return (EIO);
	}
	v = AQ_READ_REG(hw, AQ2_MCP_HOST_REQ_INT_REG);
	if (v & AQ2_MCP_HOST_REQ_INT_READY) {
		device_printf(hw->dev, "A2 firmware required but not present\n");
		return (ENXIO);
	}

	/* Repack into fw_version's major.minor.build layout. */
	err = aq2_fw_interface_buffer_read(hw,
	    AQ2_FW_INTERFACE_OUT_VERSION_BUNDLE_REG, &v, sizeof(v));
	if (err != 0)
		return (err);
	hw->fw_version.raw =
	    (((v & AQ2_FW_INTERFACE_OUT_VERSION_MAJOR) >>
		AQ2_FW_INTERFACE_OUT_VERSION_MAJOR_S) << 24) |
	    (((v & AQ2_FW_INTERFACE_OUT_VERSION_MINOR) >>
		AQ2_FW_INTERFACE_OUT_VERSION_MINOR_S) << 16) |
	    ((v & AQ2_FW_INTERFACE_OUT_VERSION_BUILD) >>
		AQ2_FW_INTERFACE_OUT_VERSION_BUILD_S);

	err = aq2_fw_interface_buffer_read(hw,
	    AQ2_FW_INTERFACE_OUT_VERSION_IFACE_REG, &v, sizeof(v));
	if (err != 0)
		return (err);
	hw->aq2_iface = v & AQ2_FW_INTERFACE_OUT_VERSION_IFACE_VER;
	switch (hw->aq2_iface) {
	case AQ2_FW_INTERFACE_OUT_VERSION_IFACE_VER_A0:
		iface = "A0";
		break;
	case AQ2_FW_INTERFACE_OUT_VERSION_IFACE_VER_B0:
		iface = "B0";
		break;
	default:
		iface = "unknown";
		break;
	}
	if (!hw->fw_announced || bootverbose) {
		device_printf(hw->dev, "Atlantic 2 %s, firmware %u.%u.%u\n",
		    iface, hw->fw_version.major_version,
		    hw->fw_version.minor_version, hw->fw_version.build_number);
		hw->fw_announced = true;
	}

	/* Base row added to every action-resolver-table index. */
	err = aq2_fw_interface_buffer_read(hw,
	    AQ2_FW_INTERFACE_OUT_FILTER_CAPS_REG, filter_caps,
	    sizeof(filter_caps));
	if (err != 0)
		return (err);
	hw->art_filter_base_index = ((filter_caps[2] &
	    AQ2_FW_INTERFACE_OUT_FILTER_CAPS3_RESOLVER_BASE_INDEX) >>
	    AQ2_FW_INTERFACE_OUT_FILTER_CAPS3_RESOLVER_BASE_INDEX_SHIFT) * 8;
	trace(hw, dbg_init, "aq2> ART filter base index = %u",
	    hw->art_filter_base_index);

	return (0);
}

/* Commit an IN-window write; wait for the MCP ack. */
static int
aq2_fw_wait_shared_ack(struct aq_hw *hw)
{
	int err;

	AQ_WRITE_REG(hw, AQ2_MIF_HOST_FINISHED_STATUS_WRITE_REG,
	    AQ2_MIF_HOST_FINISHED_STATUS_ACK);
	err = AQ_HW_WAIT_FOR((AQ_READ_REG(hw,
	    AQ2_MIF_HOST_FINISHED_STATUS_READ_REG) &
	    AQ2_MIF_HOST_FINISHED_STATUS_ACK) == 0, 100, 1000);
	if (err != 0)
		trace_error(hw, dbg_fw, "aq2> shared buffer ack timed out");

	return (err);
}

static int
aq2_fw_reset(struct aq_hw *hw)
{
	uint32_t v;
	int err;

	AQ_WRITE_REG_BIT(hw, AQ2_FW_INTERFACE_IN_LINK_CONTROL_REG,
	    AQ2_FW_INTERFACE_IN_LINK_CONTROL_MODE, 0,
	    AQ2_FW_INTERFACE_IN_LINK_CONTROL_MODE_ACTIVE);

	AQ_WRITE_REG(hw, AQ2_FW_INTERFACE_IN_MTU_REG, HW_ATL_B0_MTU_JUMBO);

	v = AQ_READ_REG(hw, AQ2_FW_INTERFACE_IN_REQUEST_POLICY_REG);
	v |= AQ2_FW_INTERFACE_IN_REQUEST_POLICY_MCAST_QUEUE_OR_TC;
	v &= ~AQ2_FW_INTERFACE_IN_REQUEST_POLICY_MCAST_RX_QUEUE_TC_INDEX;
	v |= AQ2_FW_INTERFACE_IN_REQUEST_POLICY_MCAST_ACCEPT;
	v |= AQ2_FW_INTERFACE_IN_REQUEST_POLICY_BCAST_QUEUE_OR_TC;
	v &= ~AQ2_FW_INTERFACE_IN_REQUEST_POLICY_BCAST_RX_QUEUE_TC_INDEX;
	v |= AQ2_FW_INTERFACE_IN_REQUEST_POLICY_BCAST_ACCEPT;
	v |= AQ2_FW_INTERFACE_IN_REQUEST_POLICY_PROMISC_QUEUE_OR_TC;
	v &= ~AQ2_FW_INTERFACE_IN_REQUEST_POLICY_PROMISC_RX_QUEUE_TX_INDEX;
	AQ_WRITE_REG(hw, AQ2_FW_INTERFACE_IN_REQUEST_POLICY_REG, v);

	trace(hw, dbg_init, "aq2> reset: mtu %u, request policy %#x",
	    HW_ATL_B0_MTU_JUMBO, v);

	err = aq2_fw_wait_shared_ack(hw);
	if (err != 0)
		device_printf(hw->dev, "A2 firmware reset timed out\n");
	return (err);
}

static int
aq2_fw_get_mac_addr(struct aq_hw *hw, uint8_t *mac)
{
	uint32_t mac_addr[2];

	mac_addr[0] = AQ_READ_REG(hw, AQ2_FW_INTERFACE_IN_MAC_ADDRESS_REG);
	mac_addr[1] = AQ_READ_REG(hw, AQ2_FW_INTERFACE_IN_MAC_ADDRESS_REG + 4);

	if (mac_addr[0] == 0 && mac_addr[1] == 0) {
		device_printf(hw->dev, "A2 mac address not found\n");
		return (ENXIO);
	}

	mac_addr[0] = htole32(mac_addr[0]);
	mac_addr[1] = htole32(mac_addr[1]);
	memcpy(mac, (uint8_t *)mac_addr, ETHER_ADDR_LEN);
	trace(hw, dbg_init, "aq2> MAC addr %6D", mac, ":");
	return (0);
}

/* No fw_mtx: the IN window is written only from iflib-serialised paths. */
static int
aq2_fw_set_mode(struct aq_hw *hw, enum aq_hw_fw_mpi_state mode,
    enum aq_fw_link_speed speed)
{
	uint32_t v;
	int err;

	v = AQ_READ_REG(hw, AQ2_FW_INTERFACE_IN_LINK_OPTIONS_REG);
	v &= ~(AQ2_FW_INTERFACE_IN_LINK_OPTIONS_RATE_10G |
	    AQ2_FW_INTERFACE_IN_LINK_OPTIONS_RATE_N5G |
	    AQ2_FW_INTERFACE_IN_LINK_OPTIONS_RATE_5G |
	    AQ2_FW_INTERFACE_IN_LINK_OPTIONS_RATE_N2G5 |
	    AQ2_FW_INTERFACE_IN_LINK_OPTIONS_RATE_2G5 |
	    AQ2_FW_INTERFACE_IN_LINK_OPTIONS_RATE_1G |
	    AQ2_FW_INTERFACE_IN_LINK_OPTIONS_RATE_100M |
	    AQ2_FW_INTERFACE_IN_LINK_OPTIONS_RATE_10M |
	    AQ2_FW_INTERFACE_IN_LINK_OPTIONS_RATE_1G_HD |
	    AQ2_FW_INTERFACE_IN_LINK_OPTIONS_RATE_100M_HD |
	    AQ2_FW_INTERFACE_IN_LINK_OPTIONS_RATE_10M_HD);
	v &= ~AQ2_FW_INTERFACE_IN_LINK_OPTIONS_LINK_UP;

	if (mode == MPI_INIT) {
		if (speed & aq_fw_10G)
			v |= AQ2_FW_INTERFACE_IN_LINK_OPTIONS_RATE_10G;
		if (speed & aq_fw_5G)
			v |= AQ2_FW_INTERFACE_IN_LINK_OPTIONS_RATE_N5G |
			    AQ2_FW_INTERFACE_IN_LINK_OPTIONS_RATE_5G;
		if (speed & aq_fw_2G5)
			v |= AQ2_FW_INTERFACE_IN_LINK_OPTIONS_RATE_N2G5 |
			    AQ2_FW_INTERFACE_IN_LINK_OPTIONS_RATE_2G5;
		if (speed & aq_fw_1G)
			v |= AQ2_FW_INTERFACE_IN_LINK_OPTIONS_RATE_1G;
		if (speed & aq_fw_100M)
			v |= AQ2_FW_INTERFACE_IN_LINK_OPTIONS_RATE_100M;
		if (speed & aq_fw_10M)
			v |= AQ2_FW_INTERFACE_IN_LINK_OPTIONS_RATE_10M;

		v &= ~(AQ2_FW_INTERFACE_IN_LINK_OPTIONS_PAUSE_TX |
		    AQ2_FW_INTERFACE_IN_LINK_OPTIONS_PAUSE_RX);
		if (hw->fc.fc_tx)
			v |= AQ2_FW_INTERFACE_IN_LINK_OPTIONS_PAUSE_TX;
		if (hw->fc.fc_rx)
			v |= AQ2_FW_INTERFACE_IN_LINK_OPTIONS_PAUSE_RX;

		v |= AQ2_FW_INTERFACE_IN_LINK_OPTIONS_LINK_UP;
	} else {
		AQ_WRITE_REG_BIT(hw, AQ2_FW_INTERFACE_IN_LINK_CONTROL_REG,
		    AQ2_FW_INTERFACE_IN_LINK_CONTROL_MODE, 0,
		    AQ2_FW_INTERFACE_IN_LINK_CONTROL_MODE_SHUTDOWN);
	}

	trace(hw, dbg_init, "aq2> set mode %d, speed mask %#x, link options %#x",
	    mode, speed, v);

	/* Options acked before ACTIVE so bring-up negotiates the new mask. */
	AQ_WRITE_REG(hw, AQ2_FW_INTERFACE_IN_LINK_OPTIONS_REG, v);
	if (mode == MPI_INIT) {
		err = aq2_fw_wait_shared_ack(hw);
		if (err != 0)
			return (err);
		AQ_WRITE_REG_BIT(hw, AQ2_FW_INTERFACE_IN_LINK_CONTROL_REG,
		    AQ2_FW_INTERFACE_IN_LINK_CONTROL_MODE, 0,
		    AQ2_FW_INTERFACE_IN_LINK_CONTROL_MODE_ACTIVE);
	}
	return (aq2_fw_wait_shared_ack(hw));
}

static int
aq2_fw_get_mode(struct aq_hw *hw, enum aq_hw_fw_mpi_state *modep,
    enum aq_fw_link_speed *speedp, enum aq_fw_link_fc *fcp)
{
	uint32_t v;
	enum aq_fw_link_speed speed;
	enum aq_fw_link_fc fc = aq_fw_fc_none;

	if (modep != NULL)
		*modep = MPI_INIT;

	v = AQ_READ_REG(hw, AQ2_FW_INTERFACE_OUT_LINK_STATUS_REG);
	switch ((v & AQ2_FW_INTERFACE_OUT_LINK_STATUS_RATE) >>
	    AQ2_FW_INTERFACE_OUT_LINK_STATUS_RATE_S) {
	case AQ2_FW_INTERFACE_OUT_LINK_STATUS_RATE_10G:
		speed = aq_fw_10G;
		break;
	case AQ2_FW_INTERFACE_OUT_LINK_STATUS_RATE_5G:
		speed = aq_fw_5G;
		break;
	case AQ2_FW_INTERFACE_OUT_LINK_STATUS_RATE_2G5:
		speed = aq_fw_2G5;
		break;
	case AQ2_FW_INTERFACE_OUT_LINK_STATUS_RATE_1G:
		speed = aq_fw_1G;
		break;
	case AQ2_FW_INTERFACE_OUT_LINK_STATUS_RATE_100M:
		speed = aq_fw_100M;
		break;
	case AQ2_FW_INTERFACE_OUT_LINK_STATUS_RATE_10M:
		speed = aq_fw_10M;
		break;
	default:
		speed = aq_fw_none;
		break;
	}
	if (speedp != NULL)
		*speedp = speed;

	if (v & AQ2_FW_INTERFACE_OUT_LINK_STATUS_PAUSE_TX)
		fc |= aq_fw_fc_ENABLE_TX;
	if (v & AQ2_FW_INTERFACE_OUT_LINK_STATUS_PAUSE_RX)
		fc |= aq_fw_fc_ENABLE_RX;
	if (fcp != NULL)
		*fcp = fc;

	trace_detail(hw, dbg_init,
	    "aq2> get mode: link status %#x, speed %d, fc %d", v, speed, fc);

	return (0);
}

/* A2 firmware stats; layout depends on interface version. */
struct aq2_fw_statistics_a0 {
	uint32_t link_up;
	uint32_t link_down;
	uint64_t tx_unicast_octets;
	uint64_t tx_multicast_octets;
	uint64_t tx_broadcast_octets;
	uint64_t rx_unicast_octets;
	uint64_t rx_multicast_octets;
	uint64_t rx_broadcast_octets;
	uint32_t tx_unicast_frames;
	uint32_t tx_multicast_frames;
	uint32_t tx_broadcast_frames;
	uint32_t tx_errors;
	uint32_t rx_unicast_frames;
	uint32_t rx_multicast_frames;
	uint32_t rx_broadcast_frames;
	uint32_t rx_dropped_frames;
	uint32_t rx_errors;
	uint32_t tx_good_frames;
	uint32_t rx_good_frames;
	uint32_t reserved1;
	uint32_t main_loop_cycles;
	uint32_t reserved2;
};

struct aq2_fw_statistics_b0 {
	uint64_t rx_good_octets;
	uint64_t rx_pause_frames;
	uint64_t rx_good_frames;
	uint64_t rx_errors;
	uint64_t rx_unicast_frames;
	uint64_t rx_multicast_frames;
	uint64_t rx_broadcast_frames;
	uint64_t tx_good_octets;
	uint64_t tx_pause_frames;
	uint64_t tx_good_frames;
	uint64_t tx_errors;
	uint64_t tx_unicast_frames;
	uint64_t tx_multicast_frames;
	uint64_t tx_broadcast_frames;
	uint32_t main_loop_cycles;
} __packed;

union aq2_fw_statistics {
	struct aq2_fw_statistics_a0 a0;
	struct aq2_fw_statistics_b0 b0;
};

static int
aq2_fw_get_stats(struct aq_hw *hw, struct aq_hw_stats *stats)
{
	union aq2_fw_statistics u;
	int err;

	memset(stats, 0, sizeof(*stats));

	err = aq2_fw_interface_buffer_read(hw, AQ2_FW_INTERFACE_OUT_STATS_REG,
	    (uint32_t *)&u, sizeof(u));
	if (err != 0) {
		trace_error(hw, dbg_fw,
		    "aq2> statistics read FAILED, error %d", err);
		return (err);
	}

	if (hw->aq2_iface == AQ2_FW_INTERFACE_OUT_VERSION_IFACE_VER_A0) {
		stats->uprc = u.a0.rx_unicast_frames;
		stats->mprc = u.a0.rx_multicast_frames;
		stats->bprc = u.a0.rx_broadcast_frames;
		stats->erpr = u.a0.rx_errors;
		stats->ubrc = u.a0.rx_unicast_octets;
		stats->mbrc = u.a0.rx_multicast_octets;
		stats->bbrc = u.a0.rx_broadcast_octets;
		stats->prc = u.a0.rx_good_frames;
		stats->uptc = u.a0.tx_unicast_frames;
		stats->mptc = u.a0.tx_multicast_frames;
		stats->bptc = u.a0.tx_broadcast_frames;
		stats->erpt = u.a0.tx_errors;
		stats->ubtc = u.a0.tx_unicast_octets;
		stats->mbtc = u.a0.tx_multicast_octets;
		stats->bbtc = u.a0.tx_broadcast_octets;
		stats->ptc = u.a0.tx_good_frames;
	} else if (hw->aq2_iface == AQ2_FW_INTERFACE_OUT_VERSION_IFACE_VER_B0) {
		/* B0 reports aggregate octets only; per-cast stays zero. */
		stats->brc = u.b0.rx_good_octets;
		stats->btc = u.b0.tx_good_octets;
		stats->uprc = u.b0.rx_unicast_frames;
		stats->mprc = u.b0.rx_multicast_frames;
		stats->bprc = u.b0.rx_broadcast_frames;
		stats->erpr = u.b0.rx_errors;
		stats->prc = u.b0.rx_good_frames;
		stats->uptc = u.b0.tx_unicast_frames;
		stats->mptc = u.b0.tx_multicast_frames;
		stats->bptc = u.b0.tx_broadcast_frames;
		stats->erpt = u.b0.tx_errors;
		stats->ptc = u.b0.tx_good_frames;
	} else {
		trace_warn(hw, dbg_fw,
		    "aq2> unknown F/W interface version %u, no statistics",
		    hw->aq2_iface);
	}

	return (0);
}

static int
aq2_fw_get_temp(struct aq_hw *hw, int *temp_mc)
{
	uint32_t raw;
	int err;

	err = aq2_fw_interface_buffer_read(hw,
	    AQ2_FW_INTERFACE_OUT_PHY_HEALTH_MONITOR_REG, &raw, sizeof(raw));
	if (err != 0)
		return (err);

	/* The F/W zeroes the block until the PHY reports in. */
	if ((raw & AQ2_FW_INTERFACE_OUT_PHY_HEALTH_MONITOR_READY) == 0) {
		trace_detail(hw, dbg_fw, "aq2> PHY health monitor not ready");
		return (ENXIO);
	}

	/* F/W reports whole degrees Celsius, signed. */
	*temp_mc = (int)(int8_t)((raw &
	    AQ2_FW_INTERFACE_OUT_PHY_TEMPERATURE) >>
	    AQ2_FW_INTERFACE_OUT_PHY_TEMPERATURE_S) * 1000;

	return (0);
}

/* interface-in thermal_shutdown.shutdown_temperature, whole degC. */
static int
aq2_fw_get_thermal_limit(struct aq_hw *hw, int *limit_mc)
{
	uint32_t v;
	int temp_c;

	v = AQ_READ_REG(hw, AQ2_FW_INTERFACE_IN_THERMAL_SHUTDOWN_REG);
	temp_c = (v & AQ2_FW_INTERFACE_IN_THERMAL_SHUTDOWN_TEMP) >>
	    AQ2_FW_INTERFACE_IN_THERMAL_SHUTDOWN_TEMP_S;
	if (temp_c == 0 || temp_c == 0xff)
		return (ENXIO);
	*limit_mc = temp_c * 1000;

	return (0);
}

static int
aq2_fw_get_phy_fault(struct aq_hw *hw, uint16_t *fault)
{
	uint32_t health, code;
	int err;

	err = aq2_fw_interface_buffer_read(hw,
	    AQ2_FW_INTERFACE_OUT_PHY_HEALTH_MONITOR_REG, &health,
	    sizeof(health));
	if (err != 0)
		return (err);

	if ((health & AQ2_FW_INTERFACE_OUT_PHY_HEALTH_MONITOR_FAULT) == 0) {
		*fault = 0;
		return (0);
	}

	err = aq2_fw_interface_buffer_read(hw,
	    AQ2_FW_INTERFACE_OUT_PHY_FAULT_CODE_REG, &code, sizeof(code));
	if (err != 0)
		return (err);

	*fault = (uint16_t)(code & AQ2_FW_INTERFACE_OUT_PHY_FAULT_CODE);

	return (0);
}

const struct aq_firmware_ops aq2_fw_ops = {
	.reset = aq2_fw_reset,
	.set_mode = aq2_fw_set_mode,
	.get_mode = aq2_fw_get_mode,
	.get_mac_addr = aq2_fw_get_mac_addr,
	.get_stats = aq2_fw_get_stats,
	.get_temp = aq2_fw_get_temp,
	.get_phy_fault = aq2_fw_get_phy_fault,
	.phy_reset = NULL,	/* A2 clears thermal shutdown on its own reset */
	.thermal_arm = NULL,	/* A2 firmware ships thermal shutdown armed */
	.get_thermal_limit = aq2_fw_get_thermal_limit,
	.led_control = NULL,
};
