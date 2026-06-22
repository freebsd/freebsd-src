/**
 * aQuantia Corporation Network Driver
 * Copyright (C) 2014-2017 aQuantia Corporation. All rights reserved
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
 *   (3) The name of the author may not be used to endorse or promote
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
 *
 * @file aq_fw2x.c
 * Firmware v2.x specific functions.
 * @date 2017.12.11  @author roman.agafonov@aquantia.com
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/errno.h>

#include "aq_common.h"


#include "aq_hw.h"
#include "aq_hw_llh.h"
#include "aq_hw_llh_internal.h"

#include "aq_fw.h"

#include "aq_dbg.h"

enum fw2x_caps_lo {
	CAPS_LO_10BASET_HD = 0x00,
	CAPS_LO_10BASET_FD,
	CAPS_LO_100BASETX_HD,
	CAPS_LO_100BASET4_HD,
	CAPS_LO_100BASET2_HD,
	CAPS_LO_100BASETX_FD,
	CAPS_LO_100BASET2_FD,
	CAPS_LO_1000BASET_HD,
	CAPS_LO_1000BASET_FD,
	CAPS_LO_2P5GBASET_FD,
	CAPS_LO_5GBASET_FD,
	CAPS_LO_10GBASET_FD,
};

enum fw2x_caps_hi {
	CAPS_HI_RESERVED1 = 0x00,
	CAPS_HI_10BASET_EEE,
	CAPS_HI_RESERVED2,
	CAPS_HI_PAUSE,
	CAPS_HI_ASYMMETRIC_PAUSE,
	CAPS_HI_100BASETX_EEE,
	CAPS_HI_RESERVED3,
	CAPS_HI_RESERVED4,
	CAPS_HI_1000BASET_FD_EEE,
	CAPS_HI_2P5GBASET_FD_EEE,
	CAPS_HI_5GBASET_FD_EEE,
	CAPS_HI_10GBASET_FD_EEE,
	CAPS_HI_RESERVED5,
	CAPS_HI_RESERVED6,
	CAPS_HI_RESERVED7,
	CAPS_HI_RESERVED8,
	CAPS_HI_RESERVED9,
	CAPS_HI_CABLE_DIAG,
	CAPS_HI_TEMPERATURE,
	CAPS_HI_DOWNSHIFT,
	CAPS_HI_PTP_AVB_EN,
	CAPS_HI_MEDIA_DETECT,
	CAPS_HI_LINK_DROP,
	CAPS_HI_SLEEP_PROXY,
	CAPS_HI_WOL,
	CAPS_HI_MAC_STOP,
	CAPS_HI_EXT_LOOPBACK,
	CAPS_HI_INT_LOOPBACK,
	CAPS_HI_EFUSE_AGENT,
	CAPS_HI_WOL_TIMER,
	CAPS_HI_STATISTICS,
	CAPS_HI_TRANSACTION_ID,
};

enum aq_fw2x_rate
{
	FW2X_RATE_100M = 0x20,
	FW2X_RATE_1G = 0x100,
	FW2X_RATE_2G5 = 0x200,
	FW2X_RATE_5G = 0x400,
	FW2X_RATE_10G = 0x800,
};


struct fw2x_msm_statistics
{
	uint32_t uprc;
	uint32_t mprc;
	uint32_t bprc;
	uint32_t erpt;
	uint32_t uptc;
	uint32_t mptc;
	uint32_t bptc;
	uint32_t erpr;
	uint32_t mbtc;
	uint32_t bbtc;
	uint32_t mbrc;
	uint32_t bbrc;
	uint32_t ubrc;
	uint32_t ubtc;
	uint32_t ptc;
	uint32_t prc;
};

struct fw2x_phy_cable_diag_data
{
	uint32_t lane_data[4];
};

struct fw2x_capabilities {
	uint32_t caps_lo;
	uint32_t caps_hi;
};

struct fw2x_mailbox // struct fwHostInterface
{
	uint32_t version;
	uint32_t transaction_id;
	int32_t error;
	struct fw2x_msm_statistics msm; // msmStatistics_t msm;
	uint16_t phy_h_bit;
	uint16_t phy_fault_code;
	int16_t phy_temperature;
	uint8_t cable_len;
	uint8_t reserved1;
	struct fw2x_phy_cable_diag_data diag_data;
	uint32_t reserved[8];

	struct fw2x_capabilities caps;

	/* ... */
};


// EEE caps
#define FW2X_FW_CAP_EEE_100M (1ULL << (32 + CAPS_HI_100BASETX_EEE))
#define FW2X_FW_CAP_EEE_1G   (1ULL << (32 + CAPS_HI_1000BASET_FD_EEE))
#define FW2X_FW_CAP_EEE_2G5  (1ULL << (32 + CAPS_HI_2P5GBASET_FD_EEE))
#define FW2X_FW_CAP_EEE_5G   (1ULL << (32 + CAPS_HI_5GBASET_FD_EEE))
#define FW2X_FW_CAP_EEE_10G  (1ULL << (32 + CAPS_HI_10GBASET_FD_EEE))

// Flow Control
#define FW2X_FW_CAP_PAUSE      (1ULL << (32 + CAPS_HI_PAUSE))
#define FW2X_FW_CAP_ASYM_PAUSE (1ULL << (32 + CAPS_HI_ASYMMETRIC_PAUSE))

// Link Drop
#define FW2X_CAP_LINK_DROP  (1ull << (32 + CAPS_HI_LINK_DROP))

// MSM Statistics
#define FW2X_CAP_STATISTICS (1ull << (32 + CAPS_HI_STATISTICS))


#define FW2X_RATE_MASK  (FW2X_RATE_100M | FW2X_RATE_1G | FW2X_RATE_2G5 | FW2X_RATE_5G | FW2X_RATE_10G)
#define FW2X_EEE_MASK  (FW2X_FW_CAP_EEE_100M | FW2X_FW_CAP_EEE_1G | FW2X_FW_CAP_EEE_2G5 | FW2X_FW_CAP_EEE_5G | FW2X_FW_CAP_EEE_10G)


#define FW2X_MPI_LED_ADDR           0x31c
#define FW2X_MPI_CONTROL_ADDR       0x368
#define FW2X_MPI_STATE_ADDR         0x370

#define FW2X_FW_MIN_VER_LED 0x03010026U

#define FW2X_LED_BLINK    0x2U
#define FW2X_LED_DEFAULT  0x0U

// Firmware v2-3.x specific functions.
static int fw2x_reset(struct aq_hw* hw);

static int fw2x_set_mode(struct aq_hw* hw, enum aq_hw_fw_mpi_state mode,
    enum aq_fw_link_speed speed);
static int fw2x_get_mode(struct aq_hw* hw, enum aq_hw_fw_mpi_state* mode,
    enum aq_fw_link_speed* speed, enum aq_fw_link_fc* fc);

static int fw2x_get_mac_addr(struct aq_hw* hw, uint8_t* mac);
static int fw2x_get_stats(struct aq_hw* hw, struct aq_hw_stats* stats);


static uint64_t
read64(struct aq_hw* hw, uint32_t addr)
{
	uint64_t lo, hi, hi2;

	hi = AQ_READ_REG(hw, addr + 4);
	do {
		hi2 = hi;
		lo = AQ_READ_REG(hw, addr);
		hi = AQ_READ_REG(hw, addr + 4);
	} while (hi != hi2);

	return (lo | (hi << 32));
}

static uint64_t
get_mpi_ctrl(struct aq_hw* hw)
{
	return read64(hw, FW2X_MPI_CONTROL_ADDR);
}

static uint64_t
get_mpi_state(struct aq_hw* hw)
{
	return read64(hw, FW2X_MPI_STATE_ADDR);
}

static void
set_mpi_ctrl(struct aq_hw* hw, uint64_t value)
{
	AQ_WRITE_REG(hw, FW2X_MPI_CONTROL_ADDR, (uint32_t)value);
	AQ_WRITE_REG(hw, FW2X_MPI_CONTROL_ADDR + 4, (uint32_t)(value >> 32));
}


static int
fw2x_reset(struct aq_hw* hw)
{
	struct fw2x_capabilities caps = {0};
	AQ_DBG_ENTER();
	int err = aq_hw_fw_downld_dwords(hw,
	    hw->mbox_addr + offsetof(struct fw2x_mailbox, caps),
	    (uint32_t*)&caps, sizeof caps/sizeof(uint32_t));
	if (err == 0) {
		hw->fw_caps = caps.caps_lo | ((uint64_t)caps.caps_hi << 32);
		trace(dbg_init,
		     "fw2x> F/W capabilities mask = %llx",
		     (unsigned long long)hw->fw_caps);
	} else {
		trace_error(dbg_init,
		     "fw2x> can't get F/W capabilities mask, error %d", err);
	}

	AQ_DBG_EXIT(err);
	return (err);
}


static enum aq_fw2x_rate
link_speed_mask_to_fw2x(uint32_t speed)
{
	uint32_t rate = 0;

	AQ_DBG_ENTER();
	if (speed & aq_fw_10G)
		rate |= FW2X_RATE_10G;

	if (speed & aq_fw_5G)
		rate |= FW2X_RATE_5G;

	if (speed & aq_fw_2G5)
		rate |= FW2X_RATE_2G5;

	if (speed & aq_fw_1G)
		rate |= FW2X_RATE_1G;

	if (speed & aq_fw_100M)
		rate |= FW2X_RATE_100M;

	AQ_DBG_EXIT(rate);
	return ((enum aq_fw2x_rate)rate);
}


static int
fw2x_set_mode(struct aq_hw* hw, enum aq_hw_fw_mpi_state mode,
    enum aq_fw_link_speed speed)
{
	uint64_t mpi_ctrl = get_mpi_ctrl(hw);

	AQ_DBG_ENTERA("speed=%d", speed);
	switch (mode) {
	case MPI_INIT:
		mpi_ctrl &= ~FW2X_RATE_MASK;
		mpi_ctrl |= link_speed_mask_to_fw2x(speed);
		mpi_ctrl &= ~FW2X_CAP_LINK_DROP;
#if 0 // #todo #flowcontrol #pause #eee
		if (pHal->pCfg->eee)
			mpi_ctrl |= FW2X_EEE_MASK;
#endif
		if (hw->fc.fc_rx)
		mpi_ctrl |= FW2X_FW_CAP_PAUSE;
		if (hw->fc.fc_tx)
			mpi_ctrl |= FW2X_FW_CAP_ASYM_PAUSE;
		break;

	case MPI_DEINIT:
		mpi_ctrl &= ~(FW2X_RATE_MASK | FW2X_EEE_MASK);
		mpi_ctrl &= ~(FW2X_FW_CAP_PAUSE | FW2X_FW_CAP_ASYM_PAUSE);
		break;

	default:
		trace_error(dbg_init, "fw2x> unknown MPI state %d", mode);
		return (EINVAL);
	}

	set_mpi_ctrl(hw, mpi_ctrl);
	AQ_DBG_EXIT(0);
	return (0);
}

static int
fw2x_get_mode(struct aq_hw* hw, enum aq_hw_fw_mpi_state* mode,
    enum aq_fw_link_speed* link_speed, enum aq_fw_link_fc* fc)
{
	uint64_t mpi_state = get_mpi_state(hw);
	uint32_t rates = mpi_state & FW2X_RATE_MASK;

 //   AQ_DBG_ENTER();

	if (mode) {
		uint64_t mpi_ctrl = get_mpi_ctrl(hw);
		if (mpi_ctrl & FW2X_RATE_MASK)
		*mode = MPI_INIT;
		else
		*mode = MPI_DEINIT;
	}

	enum aq_fw_link_speed speed = aq_fw_none;

	if (rates & FW2X_RATE_10G)
		speed = aq_fw_10G;
	else if (rates & FW2X_RATE_5G)
		speed = aq_fw_5G;
	else if (rates & FW2X_RATE_2G5)
		speed = aq_fw_2G5;
	else if (rates & FW2X_RATE_1G)
		speed = aq_fw_1G;
	else if (rates & FW2X_RATE_100M)
		speed = aq_fw_100M;

	if (link_speed)
		*link_speed = speed;

	*fc = (mpi_state & (FW2X_FW_CAP_PAUSE | FW2X_FW_CAP_ASYM_PAUSE)) >>
	    (32 + CAPS_HI_PAUSE);

//    AQ_DBG_EXIT(0);
	return (0);
}


static int
fw2x_get_mac_addr(struct aq_hw* hw, uint8_t* mac)
{
	int err = EFAULT;
	uint32_t mac_addr[2];

	AQ_DBG_ENTER();

	uint32_t efuse_shadow_addr = AQ_READ_REG(hw, 0x364);
	if (efuse_shadow_addr == 0) {
		trace_error(dbg_init, "couldn't read eFUSE Shadow Address");
		AQ_DBG_EXIT(EFAULT);
		return (EFAULT);
	}

	err = aq_hw_fw_downld_dwords(hw, efuse_shadow_addr + (40 * 4), mac_addr,
	    nitems(mac_addr));
	if (err != 0) {
		mac_addr[0] = 0;
		mac_addr[1] = 0;
		AQ_DBG_EXIT(err);
		return (err);
	}

	mac_addr[0] = bswap32(mac_addr[0]);
	mac_addr[1] = bswap32(mac_addr[1]);

	memcpy(mac, (uint8_t*)mac_addr, ETHER_ADDR_LEN);

	AQ_DBG_EXIT(0);
	return (0);
}

static inline void
fw2x_stats_to_fw_stats(struct aq_hw_stats* dst,
    const struct fw2x_msm_statistics* src)
{
	dst->uprc = src->uprc;
	dst->mprc = src->mprc;
	dst->bprc = src->bprc;
	dst->erpt = src->erpt;
	dst->uptc = src->uptc;
	dst->mptc = src->mptc;
	dst->bptc = src->bptc;
	dst->erpr = src->erpr;
	dst->mbtc = src->mbtc;
	dst->bbtc = src->bbtc;
	dst->mbrc = src->mbrc;
	dst->bbrc = src->bbrc;
	dst->ubrc = src->ubrc;
	dst->ubtc = src->ubtc;
	dst->ptc = src->ptc;
	dst->prc = src->prc;
}


static int
fw2x_get_stats(struct aq_hw* hw, struct aq_hw_stats* stats)
{
	struct fw2x_msm_statistics fw2x_stats = {0};
	uint64_t mpi_ctrl;
	int err;

	if ((hw->fw_caps & FW2X_CAP_STATISTICS) == 0) {
		trace_warn(dbg_fw, "fw2x> statistics not supported by F/W");
		return (ENOTSUP);
	}

	/* Kick-and-read: take the F/W's previous snapshot, request the next. */
	err = aq_hw_fw_downld_dwords(hw,
	    hw->mbox_addr + offsetof(struct fw2x_mailbox, msm),
	    (uint32_t*)&fw2x_stats, sizeof fw2x_stats/sizeof(uint32_t));

	fw2x_stats_to_fw_stats(stats, &fw2x_stats);

	if (err != 0)
		trace_error(dbg_fw,
		    "fw2x> download statistics data FAILED, error %d", err);

	mpi_ctrl = get_mpi_ctrl(hw);
	mpi_ctrl ^= FW2X_CAP_STATISTICS;
	set_mpi_ctrl(hw, mpi_ctrl);

	return (err);
}

static int
fw2x_led_control(struct aq_hw* hw, uint32_t onoff)
{
	int err = 0;

	AQ_DBG_ENTER();

	struct aq_hw_fw_version ver_expected = { .raw = FW2X_FW_MIN_VER_LED};
	if (aq_hw_ver_match(&ver_expected, &hw->fw_version))
		AQ_WRITE_REG(hw, FW2X_MPI_LED_ADDR,
		    (onoff) ? ((FW2X_LED_BLINK) | (FW2X_LED_BLINK << 2) | (FW2X_LED_BLINK << 4)):
		    (FW2X_LED_DEFAULT));

	AQ_DBG_EXIT(err);
	return (err);
}

const struct aq_firmware_ops aq_fw2x_ops =
{
	.reset = fw2x_reset,

	.set_mode = fw2x_set_mode,
	.get_mode = fw2x_get_mode,

	.get_mac_addr = fw2x_get_mac_addr,
	.get_stats = fw2x_get_stats,

	.led_control = fw2x_led_control,
};
