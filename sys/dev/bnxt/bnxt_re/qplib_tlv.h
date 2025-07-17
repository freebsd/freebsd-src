/*
 * Copyright (c) 2017 - 2024, Broadcom. All rights reserved.  The term
 * Broadcom refers to Broadcom Limited and/or its subsidiaries.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef __QPLIB_TLV_H__
#define __QPLIB_TLV_H__

struct roce_tlv {
	struct tlv tlv;
	u8 total_size;
	u8 unused[7];
};

#define CHUNK_SIZE 16
#define CHUNKS(x) (((x) + CHUNK_SIZE - 1) / CHUNK_SIZE)

#define ROCE_1ST_TLV_PREP(rtlv, tot_chunks, content_bytes, more)     \
    do {                                                             \
	(rtlv)->tlv.cmd_discr = CMD_DISCR_TLV_ENCAP;                 \
	(rtlv)->tlv.tlv_type = TLV_TYPE_ROCE_SP_COMMAND;             \
	(rtlv)->tlv.length = (content_bytes);                        \
	(rtlv)->tlv.flags = TLV_FLAGS_REQUIRED;                      \
	(rtlv)->tlv.flags |= (more) ? TLV_FLAGS_MORE : 0;            \
	(rtlv)->total_size = (tot_chunks);                           \
    } while (0)

#define ROCE_EXT_TLV_PREP(rtlv, ext_type, content_bytes, more, reqd) \
    do {                                                             \
	(rtlv)->tlv.cmd_discr = CMD_DISCR_TLV_ENCAP;                 \
	(rtlv)->tlv.tlv_type = (ext_type);                           \
	(rtlv)->tlv.length = (content_bytes);                        \
	(rtlv)->tlv.flags |= (more) ? TLV_FLAGS_MORE : 0;            \
	(rtlv)->tlv.flags |= (reqd) ? TLV_FLAGS_REQUIRED : 0;        \
    } while (0)

/*
 * TLV size in units of 16 byte chunks
 */
#define TLV_SIZE ((sizeof(struct roce_tlv) + 15) / 16)
/*
 * TLV length in bytes
 */
#define TLV_BYTES (TLV_SIZE * 16)

#define HAS_TLV_HEADER(msg) (((struct tlv *)(msg))->cmd_discr == CMD_DISCR_TLV_ENCAP)
#define GET_TLV_DATA(tlv)   ((void *)&((uint8_t *)(tlv))[TLV_BYTES])

static inline u8 __get_cmdq_base_opcode(struct cmdq_base *req, u32 size)
{
	if (HAS_TLV_HEADER(req) && size > TLV_BYTES)
		return ((struct cmdq_base *)GET_TLV_DATA(req))->opcode;
	else
		return req->opcode;
}

static inline void __set_cmdq_base_opcode(struct cmdq_base *req,
					  u32 size, u8 val)
{
	if (HAS_TLV_HEADER(req) && size > TLV_BYTES)
		((struct cmdq_base *)GET_TLV_DATA(req))->opcode = val;
	else
		req->opcode = val;
}

static inline __le16 __get_cmdq_base_cookie(struct cmdq_base *req, u32 size)
{
	if (HAS_TLV_HEADER(req) && size > TLV_BYTES)
		return ((struct cmdq_base *)GET_TLV_DATA(req))->cookie;
	else
		return req->cookie;
}

static inline void __set_cmdq_base_cookie(struct cmdq_base *req,
					  u32 size, __le16 val)
{
	if (HAS_TLV_HEADER(req) && size > TLV_BYTES)
		((struct cmdq_base *)GET_TLV_DATA(req))->cookie = val;
	else
		req->cookie = val;
}

static inline __le64 __get_cmdq_base_resp_addr(struct cmdq_base *req, u32 size)
{
	if (HAS_TLV_HEADER(req) && size > TLV_BYTES)
		return ((struct cmdq_base *)GET_TLV_DATA(req))->resp_addr;
	else
		return req->resp_addr;
}

static inline void __set_cmdq_base_resp_addr(struct cmdq_base *req,
					     u32 size, __le64 val)
{
	if (HAS_TLV_HEADER(req) && size > TLV_BYTES)
		((struct cmdq_base *)GET_TLV_DATA(req))->resp_addr = val;
	else
		req->resp_addr = val;
}

static inline u8 __get_cmdq_base_resp_size(struct cmdq_base *req, u32 size)
{
	if (HAS_TLV_HEADER(req) && size > TLV_BYTES)
		return ((struct cmdq_base *)GET_TLV_DATA(req))->resp_size;
	else
		return req->resp_size;
}

static inline void __set_cmdq_base_resp_size(struct cmdq_base *req,
					     u32 size, u8 val)
{
	if (HAS_TLV_HEADER(req) && size > TLV_BYTES)
		((struct cmdq_base *)GET_TLV_DATA(req))->resp_size = val;
	else
		req->resp_size = val;
}

static inline u8 __get_cmdq_base_cmd_size(struct cmdq_base *req, u32 size)
{
	if (HAS_TLV_HEADER(req) && size > TLV_BYTES)
		return ((struct roce_tlv *)(req))->total_size;
	else
		return req->cmd_size;
}

static inline void __set_cmdq_base_cmd_size(struct cmdq_base *req,
					    u32 size, u8 val)
{
	if (HAS_TLV_HEADER(req) && size > TLV_BYTES)
		((struct cmdq_base *)GET_TLV_DATA(req))->cmd_size = val;
	else
		req->cmd_size = val;
}

static inline __le16 __get_cmdq_base_flags(struct cmdq_base *req, u32 size)
{
	if (HAS_TLV_HEADER(req) && size > TLV_BYTES)
		return ((struct cmdq_base *)GET_TLV_DATA(req))->flags;
	else
		return req->flags;
}

static inline void __set_cmdq_base_flags(struct cmdq_base *req,
					 u32 size, __le16 val)
{
	if (HAS_TLV_HEADER(req) && size > TLV_BYTES)
		((struct cmdq_base *)GET_TLV_DATA(req))->flags = val;
	else
		req->flags = val;
}

struct bnxt_qplib_tlv_modify_cc_req {
	struct roce_tlv				tlv_hdr;
	struct cmdq_modify_roce_cc		base_req;
	__le64					tlvpad;
	struct cmdq_modify_roce_cc_gen1_tlv	ext_req;
};

struct bnxt_qplib_tlv_query_rcc_sb {
	struct roce_tlv					tlv_hdr;
	struct creq_query_roce_cc_resp_sb		base_sb;
	struct creq_query_roce_cc_gen1_resp_sb_tlv	gen1_sb;
};
#endif
