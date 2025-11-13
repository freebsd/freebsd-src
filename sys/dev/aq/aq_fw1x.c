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


#define FW1X_MPI_CONTROL_ADR    0x368
#define FW1X_MPI_STATE_ADR      0x36C


typedef enum fw1x_mode {
    FW1X_MPI_DEINIT = 0,
    FW1X_MPI_RESERVED = 1,
    FW1X_MPI_INIT = 2,
    FW1X_MPI_POWER = 4,
} fw1x_mode;

typedef enum aq_fw1x_rate {
    FW1X_RATE_10G   = 1 << 0,
    FW1X_RATE_5G    = 1 << 1,
    FW1X_RATE_5GSR  = 1 << 2,
    FW1X_RATE_2G5   = 1 << 3,
    FW1X_RATE_1G    = 1 << 4,
    FW1X_RATE_100M  = 1 << 5,
    FW1X_RATE_INVALID = 1 << 6,
} aq_fw1x_rate;

typedef union fw1x_state_reg {
    u32 val;
    struct {
        u8 mode;
        u8 reserved1;
        u8 speed;
        u8 reserved2 : 1;
        u8 disableDirtyWake : 1;
        u8 reserved3 : 2;
        u8 downshift : 4;
    };
} fw1x_state_reg;

int fw1x_reset(struct aq_hw* hw);

int fw1x_set_mode(struct aq_hw* hw, enum aq_hw_fw_mpi_state_e mode, aq_fw_link_speed_t speed);
int fw1x_get_mode(struct aq_hw* hw, enum aq_hw_fw_mpi_state_e* mode, aq_fw_link_speed_t* speed, aq_fw_link_fc_t* fc);
int fw1x_get_mac_addr(struct aq_hw* hw, u8* mac_addr);
int fw1x_get_stats(struct aq_hw* hw, struct aq_hw_stats_s* stats);


static
fw1x_mode mpi_mode_to_fw1x_(enum aq_hw_fw_mpi_state_e mode)
{
    switch (mode) {
    case MPI_DEINIT:
        return (FW1X_MPI_DEINIT);

    case MPI_INIT:
        return (FW1X_MPI_INIT);

    case MPI_POWER:
        return (FW1X_MPI_POWER);

    case MPI_RESET:
        return (FW1X_MPI_RESERVED);
    }

    /*
     * We shouldn't get here.
     */

    return (FW1X_MPI_RESERVED);
}

static
aq_fw1x_rate link_speed_mask_to_fw1x_(u32 /*aq_fw_link_speed*/ speed)
{
    u32 rate = 0;
    if (speed & aq_fw_10G)
        rate |= FW1X_RATE_10G;

    if (speed & aq_fw_5G) {
        rate |= FW1X_RATE_5G;
        rate |= FW1X_RATE_5GSR;
    }

    if (speed & aq_fw_2G5)
        rate |= FW1X_RATE_2G5;

    if (speed & aq_fw_1G)
        rate |= FW1X_RATE_1G;

    if (speed & aq_fw_100M)
        rate |= FW1X_RATE_100M;

    return ((aq_fw1x_rate)rate);
}

static
aq_fw_link_speed_t fw1x_rate_to_link_speed_(aq_fw1x_rate rate)
{
    switch (rate) {
    case FW1X_RATE_10G:
        return (aq_fw_10G);
    case FW1X_RATE_5G:
    case FW1X_RATE_5GSR:
        return (aq_fw_5G);
    case FW1X_RATE_2G5:
        return (aq_fw_2G5);
    case FW1X_RATE_1G:
        return (aq_fw_1G);
    case FW1X_RATE_100M:
        return (aq_fw_100M);
    case FW1X_RATE_INVALID:
        return (aq_fw_none);
    }

    /*
     * We should never get here.
     */

    return (aq_fw_none);
}

int fw1x_reset(struct aq_hw* hal)
{
    u32 tid0 = ~0u; /*< Initial value of MBOX transactionId. */
    struct aq_hw_fw_mbox mbox;
    const int retryCount = 1000;

    for (int i = 0; i < retryCount; ++i) {
        // Read the beginning of Statistics structure to capture the Transaction ID.
        aq_hw_fw_downld_dwords(hal, hal->mbox_addr, (u32*)&mbox,
            (u32)((char*)&mbox.stats - (char*)&mbox) / sizeof(u32));

        // Successfully read the stats.
        if (tid0 == ~0U) {
            // We have read the initial value.
            tid0 = mbox.transaction_id;
            continue;
        } else if (mbox.transaction_id != tid0) {
            /*
             * Compare transaction ID to initial value.
             * If it's different means f/w is alive. We're done.
             */

            return (EOK);
        }

        /*
         * Transaction ID value haven't changed since last time.
         * Try reading the stats again.
         */
        usec_delay(10);
    }

    trace_error(dbg_init, "F/W 1.x reset finalize timeout");
    return (-EBUSY);
}

int fw1x_set_mode(struct aq_hw* hw, enum aq_hw_fw_mpi_state_e mode, aq_fw_link_speed_t speed)
{
    union fw1x_state_reg state = {0};
    state.mode = mpi_mode_to_fw1x_(mode);
    state.speed = link_speed_mask_to_fw1x_(speed);

    trace(dbg_init, "fw1x> set mode %d, rate mask = %#x; raw = %#x", state.mode, state.speed, state.val);

    AQ_WRITE_REG(hw, FW1X_MPI_CONTROL_ADR, state.val);

    return (EOK);
}

int fw1x_get_mode(struct aq_hw* hw, enum aq_hw_fw_mpi_state_e* mode, aq_fw_link_speed_t* speed, aq_fw_link_fc_t* fc)
{
    union fw1x_state_reg state = { .val = AQ_READ_REG(hw, AQ_HW_MPI_STATE_ADR) };

    trace(dbg_init, "fw1x> get_mode(): 0x36c -> %x, 0x368 -> %x", state.val, AQ_READ_REG(hw, AQ_HW_MPI_CONTROL_ADR));

    enum aq_hw_fw_mpi_state_e md = MPI_DEINIT;

    switch (state.mode) {
    case FW1X_MPI_DEINIT:
        md = MPI_DEINIT;
        break;
    case FW1X_MPI_RESERVED:
        md = MPI_RESET;
        break;
    case FW1X_MPI_INIT:
        md = MPI_INIT;
        break;
    case FW1X_MPI_POWER:
        md = MPI_POWER;
        break;
    }

    if (mode)
        *mode = md;

    if (speed)
        *speed = fw1x_rate_to_link_speed_(state.speed);

    *fc = aq_fw_fc_none;

    AQ_DBG_EXIT(EOK);
    return (EOK);
}


int fw1x_get_mac_addr(struct aq_hw* hw, u8* mac)
{
    int err = -EFAULT;
    u32 mac_addr[2];

    AQ_DBG_ENTER();

    u32 efuse_shadow_addr = AQ_READ_REG(hw, 0x374);
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

    trace(dbg_init, "fw1x> eFUSE MAC addr -> %02x-%02x-%02x-%02x-%02x-%02x",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    AQ_DBG_EXIT(EOK);
    return (EOK);
}

int fw1x_get_stats(struct aq_hw* hw, struct aq_hw_stats_s* stats)
{
    int err = 0;

    AQ_DBG_ENTER();
    err = aq_hw_fw_downld_dwords(hw, hw->mbox_addr, (u32*)(void*)&hw->mbox,
        sizeof hw->mbox / sizeof(u32));

    if (err >= 0) {
        if (stats != &hw->mbox.stats)
            memcpy(stats, &hw->mbox.stats, sizeof *stats);

        stats->dpc = reg_rx_dma_stat_counter7get(hw);
    }

    AQ_DBG_EXIT(err);
    return (err);
}

struct aq_firmware_ops aq_fw1x_ops =
{
    .reset = fw1x_reset,

    .set_mode = fw1x_set_mode,
    .get_mode = fw1x_get_mode,

    .get_mac_addr = fw1x_get_mac_addr,
    .get_stats = fw1x_get_stats,
};

