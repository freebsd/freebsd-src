/* SPDX-License-Identifier: BSD-3-Clause */
/*  Copyright (c) 2023, Intel Corporation
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

#ifndef _ICE_SBQ_CMD_H_
#define _ICE_SBQ_CMD_H_

/* This header file defines the Sideband Queue commands, error codes and
 * descriptor format. It is shared between Firmware and Software.
 */

/* Sideband Queue command structure and opcodes */
enum ice_sbq_opc {
	/* Sideband Queue commands */
	ice_sbq_opc_neigh_dev_req			= 0x0C00,
	ice_sbq_opc_neigh_dev_ev			= 0x0C01
};

/* Sideband Queue descriptor. Indirect command
 * and non posted
 */
struct ice_sbq_cmd_desc {
	__le16 flags;
	__le16 opcode;
	__le16 datalen;
	__le16 cmd_retval;

	/* Opaque message data */
	__le32 cookie_high;
	__le32 cookie_low;

	union {
		__le16 cmd_len;
		__le16 cmpl_len;
	} param0;

	u8 reserved[6];
	__le32 addr_high;
	__le32 addr_low;
};

struct ice_sbq_evt_desc {
	__le16 flags;
	__le16 opcode;
	__le16 datalen;
	__le16 cmd_retval;
	u8 data[24];
};

enum ice_sbq_msg_dev {
	rmn_0	= 0x02,
	rmn_1	= 0x03,
	rmn_2	= 0x04,
	cgu	= 0x06
};

enum ice_sbq_msg_opcode {
	ice_sbq_msg_rd	= 0x00,
	ice_sbq_msg_wr	= 0x01
};

#define ICE_SBQ_MSG_FLAGS	0x40
#define ICE_SBQ_MSG_SBE_FBE	0x0F

struct ice_sbq_msg_req {
	u8 dest_dev;
	u8 src_dev;
	u8 opcode;
	u8 flags;
	u8 sbe_fbe;
	u8 func_id;
	__le16 msg_addr_low;
	__le32 msg_addr_high;
	__le32 data;
};

struct ice_sbq_msg_cmpl {
	u8 dest_dev;
	u8 src_dev;
	u8 opcode;
	u8 flags;
	__le32 data;
};

/* Internal struct */
struct ice_sbq_msg_input {
	u8 dest_dev;
	u8 opcode;
	u16 msg_addr_low;
	u32 msg_addr_high;
	u32 data;
};
#endif /* _ICE_SBQ_CMD_H_ */
