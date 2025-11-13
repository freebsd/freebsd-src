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

#include <errno.h>

#include "aq_common.h"


#include "aq_hw.h"
#include "aq_hw_llh.h"
#include "aq_hw_llh_internal.h"

#include "aq_fw.h"

#include "aq_dbg.h"

typedef enum {
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
} fw2x_caps_lo;

typedef enum {
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
} fw2x_caps_hi;

typedef enum aq_fw2x_rate
{
    FW2X_RATE_100M = 0x20,
    FW2X_RATE_1G = 0x100,
    FW2X_RATE_2G5 = 0x200,
    FW2X_RATE_5G = 0x400,
    FW2X_RATE_10G = 0x800,
} aq_fw2x_rate;


typedef struct fw2x_msm_statistics
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
} fw2x_msm_statistics;

typedef struct fw2x_phy_cable_diag_data
{
    u32 lane_data[4];
} fw2x_phy_cable_diag_data;

typedef struct fw2x_capabilities {
    u32 caps_lo;
    u32 caps_hi;
} fw2x_capabilities;

typedef struct fw2x_mailbox // struct fwHostInterface
{
    u32 version;
    u32 transaction_id;
    s32 error;
    fw2x_msm_statistics msm; // msmStatistics_t msm;
    u16 phy_h_bit;
    u16 phy_fault_code;
    s16 phy_temperature;
    u8 cable_len;
    u8 reserved1;
    fw2x_phy_cable_diag_data diag_data;
    u32 reserved[8];

    fw2x_capabilities caps;

    /* ... */
} fw2x_mailbox;


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
//@{
int fw2x_reset(struct aq_hw* hw);

int fw2x_set_mode(struct aq_hw* hw, enum aq_hw_fw_mpi_state_e mode, aq_fw_link_speed_t speed);
int fw2x_get_mode(struct aq_hw* hw, enum aq_hw_fw_mpi_state_e* mode, aq_fw_link_speed_t* speed, aq_fw_link_fc_t* fc);

int fw2x_get_mac_addr(struct aq_hw* hw, u8* mac);
int fw2x_get_stats(struct aq_hw* hw, struct aq_hw_stats_s* stats);
//@}



static u64 read64_(struct aq_hw* hw, u32 addr)
{
    u64 lo = AQ_READ_REG(hw, addr);
    u64 hi = AQ_READ_REG(hw, addr + 4);
    return (lo | (hi << 32));
}

static uint64_t get_mpi_ctrl_(struct aq_hw* hw)
{
    return read64_(hw, FW2X_MPI_CONTROL_ADDR);
}

static uint64_t get_mpi_state_(struct aq_hw* hw)
{
    return read64_(hw, FW2X_MPI_STATE_ADDR);
}

static void set_mpi_ctrl_(struct aq_hw* hw, u64 value)
{
    AQ_WRITE_REG(hw, FW2X_MPI_CONTROL_ADDR, (u32)value);
    AQ_WRITE_REG(hw, FW2X_MPI_CONTROL_ADDR + 4, (u32)(value >> 32));
}


int fw2x_reset(struct aq_hw* hw)
{
    fw2x_capabilities caps = {0};
    AQ_DBG_ENTER();
    int err = aq_hw_fw_downld_dwords(hw, hw->mbox_addr + offsetof(fw2x_mailbox, caps), (u32*)&caps, sizeof caps/sizeof(u32));
    if (err == EOK) {
        hw->fw_caps = caps.caps_lo | ((u64)caps.caps_hi << 32);
        trace(dbg_init, "fw2x> F/W capabilities mask = %llx", (unsigned long long)hw->fw_caps);
    } else {
        trace_error(dbg_init, "fw2x> can't get F/W capabilities mask, error %d", err);
    }

	AQ_DBG_EXIT(EOK);
	return (EOK);
}


static
aq_fw2x_rate link_speed_mask_to_fw2x_(u32 speed)
{
    u32 rate = 0;

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
    return ((aq_fw2x_rate)rate);
}


int fw2x_set_mode(struct aq_hw* hw, enum aq_hw_fw_mpi_state_e mode, aq_fw_link_speed_t speed)
{
    u64 mpi_ctrl = get_mpi_ctrl_(hw);
    
    AQ_DBG_ENTERA("speed=%d", speed);
    switch (mode) {
    case MPI_INIT:
        mpi_ctrl &= ~FW2X_RATE_MASK;
        mpi_ctrl |= link_speed_mask_to_fw2x_(speed);
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
        return (-EINVAL);
    }

    set_mpi_ctrl_(hw, mpi_ctrl);
    AQ_DBG_EXIT(EOK);
    return (EOK);
}

int fw2x_get_mode(struct aq_hw* hw, enum aq_hw_fw_mpi_state_e* mode, aq_fw_link_speed_t* link_speed, aq_fw_link_fc_t* fc)
{
    u64 mpi_state = get_mpi_state_(hw);
    u32 rates = mpi_state & FW2X_RATE_MASK;

 //   AQ_DBG_ENTER();

    if (mode) {
        u64 mpi_ctrl = get_mpi_ctrl_(hw);
        if (mpi_ctrl & FW2X_RATE_MASK)
            *mode = MPI_INIT;
        else
            *mode = MPI_DEINIT;
    }

    aq_fw_link_speed_t speed = aq_fw_none;
    
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

    *fc = (mpi_state & (FW2X_FW_CAP_PAUSE | FW2X_FW_CAP_ASYM_PAUSE)) >> (32 + CAPS_HI_PAUSE);


//    AQ_DBG_EXIT(0);
    return (EOK);
}


int fw2x_get_mac_addr(struct aq_hw* hw, u8* mac)
{
    int err = -EFAULT;
    u32 mac_addr[2];

    AQ_DBG_ENTER();

    u32 efuse_shadow_addr = AQ_READ_REG(hw, 0x364);
    if (efuse_shadow_addr == 0) {
        trace_error(dbg_init, "couldn't read eFUSE Shadow Address");
        AQ_DBG_EXIT(-EFAULT);
        return (-EFAULT);
    }

    err = aq_hw_fw_downld_dwords(hw, efuse_shadow_addr + (40 * 4),
        mac_addr, ARRAY_SIZE(mac_addr));
    if (err < 0) {
        mac_addr[0] = 0;
        mac_addr[1] = 0;
        AQ_DBG_EXIT(err);
        return (err);
    }

    mac_addr[0] = bswap32(mac_addr[0]);
    mac_addr[1] = bswap32(mac_addr[1]);

    memcpy(mac, (u8*)mac_addr, ETH_MAC_LEN);

    AQ_DBG_EXIT(EOK);
    return (EOK);
}

static inline
void fw2x_stats_to_fw_stats_(struct aq_hw_stats_s* dst, const fw2x_msm_statistics* src)
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


static bool toggle_mpi_ctrl_and_wait_(struct aq_hw* hw, u64 mask, u32 timeout_ms, u32 try_count)
{
    u64 ctrl = get_mpi_ctrl_(hw);
    u64 state = get_mpi_state_(hw);

 //   AQ_DBG_ENTER();
    // First, check that control and state values are consistent
    if ((ctrl & mask) != (state & mask)) {
        trace_warn(dbg_fw, "fw2x> MPI control (%#llx) and state (%#llx) are not consistent for mask %#llx!",
            (unsigned long long)ctrl, (unsigned long long)state, (unsigned long long)mask);
		AQ_DBG_EXIT(false);
        return (false);
    }

    // Invert bits (toggle) in control register
    ctrl ^= mask;
    set_mpi_ctrl_(hw, ctrl);

    // Clear all bits except masked
    ctrl &= mask;

    // Wait for FW reflecting change in state register
    while (try_count-- != 0) {
        if ((get_mpi_state_(hw) & mask) == ctrl)
		{
//			AQ_DBG_EXIT(true);
            return (true);
		}
        msec_delay(timeout_ms);
    }

    trace_detail(dbg_fw, "f/w2x> timeout while waiting for response in state register for bit %#llx!", (unsigned long long)mask);
 //   AQ_DBG_EXIT(false);
    return (false);
}


int fw2x_get_stats(struct aq_hw* hw, struct aq_hw_stats_s* stats)
{
    int err = 0;
    fw2x_msm_statistics fw2x_stats = {0};

//    AQ_DBG_ENTER();

    if ((hw->fw_caps & FW2X_CAP_STATISTICS) == 0) {
        trace_warn(dbg_fw, "fw2x> statistics not supported by F/W");
        return (-ENOTSUP);
    }

    // Say to F/W to update the statistics
    if (!toggle_mpi_ctrl_and_wait_(hw, FW2X_CAP_STATISTICS, 1, 25)) {
        trace_error(dbg_fw, "fw2x> statistics update timeout");
		AQ_DBG_EXIT(-ETIME);
        return (-ETIME);
    }

    err = aq_hw_fw_downld_dwords(hw, hw->mbox_addr + offsetof(fw2x_mailbox, msm),
        (u32*)&fw2x_stats, sizeof fw2x_stats/sizeof(u32));

    fw2x_stats_to_fw_stats_(stats, &fw2x_stats);

    if (err != EOK)
        trace_error(dbg_fw, "fw2x> download statistics data FAILED, error %d", err);

//    AQ_DBG_EXIT(err);
    return (err);
}

static int fw2x_led_control(struct aq_hw* hw, u32 onoff)
{
    int err = 0;

    AQ_DBG_ENTER();

    aq_hw_fw_version ver_expected = { .raw = FW2X_FW_MIN_VER_LED};
    if (aq_hw_ver_match(&ver_expected, &hw->fw_version))
        AQ_WRITE_REG(hw, FW2X_MPI_LED_ADDR, (onoff)?
					    ((FW2X_LED_BLINK) | (FW2X_LED_BLINK << 2) | (FW2X_LED_BLINK << 4)):
					    (FW2X_LED_DEFAULT));

    AQ_DBG_EXIT(err);
    return (err);
}

struct aq_firmware_ops aq_fw2x_ops =
{
    .reset = fw2x_reset,

    .set_mode = fw2x_set_mode,
    .get_mode = fw2x_get_mode,

    .get_mac_addr = fw2x_get_mac_addr,
    .get_stats = fw2x_get_stats,

    .led_control = fw2x_led_control,
};
