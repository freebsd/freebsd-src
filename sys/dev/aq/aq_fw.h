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
#ifndef AQ_FW_H
#define AQ_FW_H

struct aq_hw;

typedef enum aq_fw_link_speed
{
    aq_fw_none  = 0,
    aq_fw_100M  = (1 << 0),
    aq_fw_1G    = (1 << 1),
    aq_fw_2G5   = (1 << 2),
    aq_fw_5G    = (1 << 3),
    aq_fw_10G   = (1 << 4),
} aq_fw_link_speed_t;

typedef enum aq_fw_link_fc
{
    aq_fw_fc_none  = 0,
    aq_fw_fc_ENABLE_RX = BIT(0),
    aq_fw_fc_ENABLE_TX = BIT(1),
    aq_fw_fc_ENABLE_ALL = aq_fw_fc_ENABLE_RX | aq_fw_fc_ENABLE_TX,
} aq_fw_link_fc_t;

#define aq_fw_speed_auto (aq_fw_100M | aq_fw_1G | aq_fw_2G5 | aq_fw_5G | aq_fw_10G)

struct aq_firmware_ops
{
    int (*reset)(struct aq_hw* hal);

    int (*set_mode)(struct aq_hw* hal, enum aq_hw_fw_mpi_state_e mode, aq_fw_link_speed_t speed);
    int (*get_mode)(struct aq_hw* hal, enum aq_hw_fw_mpi_state_e* mode, aq_fw_link_speed_t* speed, aq_fw_link_fc_t* fc);

    int (*get_mac_addr)(struct aq_hw* hal, u8* mac_addr);
    int (*get_stats)(struct aq_hw* hal, struct aq_hw_stats_s* stats);

    int (*led_control)(struct aq_hw* hal, u32 mode);
};


int aq_fw_reset(struct aq_hw* hw);
int aq_fw_ops_init(struct aq_hw* hw);

#endif // AQ_FW_H
