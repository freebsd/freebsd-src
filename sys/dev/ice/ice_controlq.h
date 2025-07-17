/* SPDX-License-Identifier: BSD-3-Clause */
/*  Copyright (c) 2024, Intel Corporation
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *   3. Neither the name of the Intel Corporation nor the names of its
 *      contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _ICE_CONTROLQ_H_
#define _ICE_CONTROLQ_H_

#include "ice_adminq_cmd.h"

/* Maximum buffer lengths for all control queue types */
#define ICE_AQ_MAX_BUF_LEN 4096
#define ICE_MBXQ_MAX_BUF_LEN 4096
#define ICE_SBQ_MAX_BUF_LEN 512

#define ICE_CTL_Q_DESC(R, i) \
	(&(((struct ice_aq_desc *)((R).desc_buf.va))[i]))

#define ICE_CTL_Q_DESC_UNUSED(R) \
	((u16)((((R)->next_to_clean > (R)->next_to_use) ? 0 : (R)->count) + \
	       (R)->next_to_clean - (R)->next_to_use - 1))

/* Defines that help manage the driver vs FW API checks.
 * Take a look at ice_aq_ver_check in ice_controlq.c for actual usage.
 */
#define EXP_FW_API_VER_BRANCH_E830	0x00
#define EXP_FW_API_VER_MAJOR_E830	0x01
#define EXP_FW_API_VER_MINOR_E830	0x07

#define EXP_FW_API_VER_BRANCH_E810	0x00
#define EXP_FW_API_VER_MAJOR_E810	0x01
#define EXP_FW_API_VER_MINOR_E810	0x05

#define EXP_FW_API_VER_BRANCH_BY_MAC(hw) ((hw)->mac_type == ICE_MAC_E830 ? \
			EXP_FW_API_VER_BRANCH_E830 : \
			EXP_FW_API_VER_BRANCH_E810)

#define EXP_FW_API_VER_MAJOR_BY_MAC(hw) ((hw)->mac_type == ICE_MAC_E830 ? \
			EXP_FW_API_VER_MAJOR_E830 : \
			EXP_FW_API_VER_MAJOR_E810)

#define EXP_FW_API_VER_MINOR_BY_MAC(hw) ((hw)->mac_type == ICE_MAC_E830 ? \
			EXP_FW_API_VER_MINOR_E830 : \
			EXP_FW_API_VER_MINOR_E810)

/* Different control queue types: These are mainly for SW consumption. */
enum ice_ctl_q {
	ICE_CTL_Q_UNKNOWN = 0,
	ICE_CTL_Q_ADMIN,
	ICE_CTL_Q_MAILBOX,
	ICE_CTL_Q_SB,
};

/* Control Queue timeout settings - max delay 1s */
#define ICE_CTL_Q_SQ_CMD_TIMEOUT	100000	/* Count 100000 times */
#define ICE_CTL_Q_ADMIN_INIT_TIMEOUT	10    /* Count 10 times */
#define ICE_CTL_Q_ADMIN_INIT_MSEC	100   /* Check every 100msec */

struct ice_ctl_q_ring {
	void *dma_head;			/* Virtual address to DMA head */
	struct ice_dma_mem desc_buf;	/* descriptor ring memory */

	union {
		struct ice_dma_mem *sq_bi;
		struct ice_dma_mem *rq_bi;
	} r;

	u16 count;		/* Number of descriptors */

	/* used for interrupt processing */
	u16 next_to_use;
	u16 next_to_clean;

	/* used for queue tracking */
	u32 head;
	u32 tail;
	u32 len;
	u32 bah;
	u32 bal;
	u32 len_mask;
	u32 len_ena_mask;
	u32 len_crit_mask;
	u32 head_mask;
};

/* sq transaction details */
struct ice_sq_cd {
	struct ice_aq_desc *wb_desc;
};

/* rq event information */
struct ice_rq_event_info {
	struct ice_aq_desc desc;
	u16 msg_len;
	u16 buf_len;
	u8 *msg_buf;
};

/* Control Queue information */
struct ice_ctl_q_info {
	enum ice_ctl_q qtype;
	struct ice_ctl_q_ring rq;	/* receive queue */
	struct ice_ctl_q_ring sq;	/* send queue */
	u32 sq_cmd_timeout;		/* send queue cmd write back timeout */
	u16 num_rq_entries;		/* receive queue depth */
	u16 num_sq_entries;		/* send queue depth */
	u16 rq_buf_size;		/* receive queue buffer size */
	u16 sq_buf_size;		/* send queue buffer size */
	enum ice_aq_err sq_last_status;	/* last status on send queue */
	struct ice_lock sq_lock;		/* Send queue lock */
	struct ice_lock rq_lock;		/* Receive queue lock */
};

#endif /* _ICE_CONTROLQ_H_ */
