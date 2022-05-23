/*-
 * SPDX-License-Identifier: GPL-2.0 or Linux-OpenIB
 *
 * Copyright (c) 2015 - 2021 Intel Corporation
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenFabrics.org BSD license below:
 *
 *   Redistribution and use in source and binary forms, with or
 *   without modification, are permitted provided that the following
 *   conditions are met:
 *
 *    - Redistributions of source code must retain the above
 *	copyright notice, this list of conditions and the following
 *	disclaimer.
 *
 *    - Redistributions in binary form must reproduce the above
 *	copyright notice, this list of conditions and the following
 *	disclaimer in the documentation and/or other materials
 *	provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
/*$FreeBSD$*/

#ifndef IRDMA_DEFS_H
#define IRDMA_DEFS_H

#define IRDMA_BYTE_0		0
#define IRDMA_BYTE_8		8
#define IRDMA_BYTE_16		16
#define IRDMA_BYTE_24		24
#define IRDMA_BYTE_32		32
#define IRDMA_BYTE_40		40
#define IRDMA_BYTE_48		48
#define IRDMA_BYTE_56		56
#define IRDMA_BYTE_64		64
#define IRDMA_BYTE_72		72
#define IRDMA_BYTE_80		80
#define IRDMA_BYTE_88		88
#define IRDMA_BYTE_96		96
#define IRDMA_BYTE_104		104
#define IRDMA_BYTE_112		112
#define IRDMA_BYTE_120		120
#define IRDMA_BYTE_128		128
#define IRDMA_BYTE_136		136
#define IRDMA_BYTE_144		144
#define IRDMA_BYTE_152		152
#define IRDMA_BYTE_160		160
#define IRDMA_BYTE_168		168
#define IRDMA_BYTE_176		176
#define IRDMA_BYTE_184		184
#define IRDMA_BYTE_192		192
#define IRDMA_BYTE_200		200
#define IRDMA_BYTE_208		208
#define IRDMA_BYTE_216		216

#define IRDMA_QP_TYPE_IWARP	1
#define IRDMA_QP_TYPE_UDA	2
#define IRDMA_QP_TYPE_ROCE_RC	3
#define IRDMA_QP_TYPE_ROCE_UD	4

#define IRDMA_HW_PAGE_SIZE	4096
#define IRDMA_HW_PAGE_SHIFT	12
#define IRDMA_CQE_QTYPE_RQ	0
#define IRDMA_CQE_QTYPE_SQ	1

#define IRDMA_QP_SW_MIN_WQSIZE	8u /* in WRs*/
#define IRDMA_QP_WQE_MIN_SIZE	32
#define IRDMA_QP_WQE_MAX_SIZE	256
#define IRDMA_QP_WQE_MIN_QUANTA 1
#define IRDMA_MAX_RQ_WQE_SHIFT_GEN1 2
#define IRDMA_MAX_RQ_WQE_SHIFT_GEN2 3

#define IRDMA_SQ_RSVD	258
#define IRDMA_RQ_RSVD	1

#define IRDMA_FEATURE_RTS_AE			1ULL
#define IRDMA_FEATURE_CQ_RESIZE			2ULL
#define IRDMA_FEATURE_RELAX_RQ_ORDER		4ULL
#define IRDMAQP_OP_RDMA_WRITE			0x00
#define IRDMAQP_OP_RDMA_READ			0x01
#define IRDMAQP_OP_RDMA_SEND			0x03
#define IRDMAQP_OP_RDMA_SEND_INV		0x04
#define IRDMAQP_OP_RDMA_SEND_SOL_EVENT		0x05
#define IRDMAQP_OP_RDMA_SEND_SOL_EVENT_INV	0x06
#define IRDMAQP_OP_BIND_MW			0x08
#define IRDMAQP_OP_FAST_REGISTER		0x09
#define IRDMAQP_OP_LOCAL_INVALIDATE		0x0a
#define IRDMAQP_OP_RDMA_READ_LOC_INV		0x0b
#define IRDMAQP_OP_NOP				0x0c

#ifndef LS_64_1
#define LS_64_1(val, bits)	((u64)(uintptr_t)(val) << (bits))
#define RS_64_1(val, bits)	((u64)(uintptr_t)(val) >> (bits))
#define LS_32_1(val, bits)	((u32)((val) << (bits)))
#define RS_32_1(val, bits)	((u32)((val) >> (bits)))
#endif
#define LS_64(val, field)	(((u64)(val) << field ## _S) & (field ## _M))
#define RS_64(val, field)	((u64)((val) & field ## _M) >> field ## _S)
#define LS_32(val, field)	(((val) << field ## _S) & (field ## _M))
#define RS_32(val, field)	(((val) & field ## _M) >> field ## _S)

#define IRDMA_CQPHC_QPCTX_S 0
#define IRDMA_CQPHC_QPCTX_M \
	(0xffffffffffffffffULL << IRDMA_CQPHC_QPCTX_S)

/* iWARP QP Doorbell shadow area */
#define IRDMA_QP_DBSA_HW_SQ_TAIL_S 0
#define IRDMA_QP_DBSA_HW_SQ_TAIL_M \
	(0x7fffULL << IRDMA_QP_DBSA_HW_SQ_TAIL_S)

/* Completion Queue Doorbell shadow area */
#define IRDMA_CQ_DBSA_CQEIDX_S 0
#define IRDMA_CQ_DBSA_CQEIDX_M (0xfffffULL << IRDMA_CQ_DBSA_CQEIDX_S)

#define IRDMA_CQ_DBSA_SW_CQ_SELECT_S 0
#define IRDMA_CQ_DBSA_SW_CQ_SELECT_M \
	(0x3fffULL << IRDMA_CQ_DBSA_SW_CQ_SELECT_S)

#define IRDMA_CQ_DBSA_ARM_NEXT_S 14
#define IRDMA_CQ_DBSA_ARM_NEXT_M BIT_ULL(IRDMA_CQ_DBSA_ARM_NEXT_S)

#define IRDMA_CQ_DBSA_ARM_NEXT_SE_S 15
#define IRDMA_CQ_DBSA_ARM_NEXT_SE_M BIT_ULL(IRDMA_CQ_DBSA_ARM_NEXT_SE_S)

#define IRDMA_CQ_DBSA_ARM_SEQ_NUM_S 16
#define IRDMA_CQ_DBSA_ARM_SEQ_NUM_M \
	(0x3ULL << IRDMA_CQ_DBSA_ARM_SEQ_NUM_S)

/* CQP and iWARP Completion Queue */
#define IRDMA_CQ_QPCTX_S IRDMA_CQPHC_QPCTX_S
#define IRDMA_CQ_QPCTX_M IRDMA_CQPHC_QPCTX_M

#define IRDMA_CQ_MINERR_S 0
#define IRDMA_CQ_MINERR_M (0xffffULL << IRDMA_CQ_MINERR_S)

#define IRDMA_CQ_MAJERR_S 16
#define IRDMA_CQ_MAJERR_M (0xffffULL << IRDMA_CQ_MAJERR_S)

#define IRDMA_CQ_WQEIDX_S 32
#define IRDMA_CQ_WQEIDX_M (0x7fffULL << IRDMA_CQ_WQEIDX_S)

#define IRDMA_CQ_EXTCQE_S 50
#define IRDMA_CQ_EXTCQE_M BIT_ULL(IRDMA_CQ_EXTCQE_S)

#define IRDMA_OOO_CMPL_S 54
#define IRDMA_OOO_CMPL_M BIT_ULL(IRDMA_OOO_CMPL_S)

#define IRDMA_CQ_ERROR_S 55
#define IRDMA_CQ_ERROR_M BIT_ULL(IRDMA_CQ_ERROR_S)

#define IRDMA_CQ_SQ_S 62
#define IRDMA_CQ_SQ_M BIT_ULL(IRDMA_CQ_SQ_S)

#define IRDMA_CQ_VALID_S 63
#define IRDMA_CQ_VALID_M BIT_ULL(IRDMA_CQ_VALID_S)

#define IRDMA_CQ_IMMVALID_S 62
#define IRDMA_CQ_IMMVALID_M BIT_ULL(IRDMA_CQ_IMMVALID_S)

#define IRDMA_CQ_UDSMACVALID_S 61
#define IRDMA_CQ_UDSMACVALID_M BIT_ULL(IRDMA_CQ_UDSMACVALID_S)

#define IRDMA_CQ_UDVLANVALID_S 60
#define IRDMA_CQ_UDVLANVALID_M BIT_ULL(IRDMA_CQ_UDVLANVALID_S)

#define IRDMA_CQ_UDSMAC_S 0
#define IRDMA_CQ_UDSMAC_M (0xffffffffffffULL << IRDMA_CQ_UDSMAC_S)

#define IRDMA_CQ_UDVLAN_S 48
#define IRDMA_CQ_UDVLAN_M (0xffffULL << IRDMA_CQ_UDVLAN_S)

#define IRDMA_CQ_IMMDATA_S 0
#define IRDMA_CQ_IMMDATA_M (0xffffffffffffffffULL << IRDMA_CQ_IMMVALID_S)

#define IRDMA_CQ_IMMDATALOW32_S 0
#define IRDMA_CQ_IMMDATALOW32_M (0xffffffffULL << IRDMA_CQ_IMMDATALOW32_S)

#define IRDMA_CQ_IMMDATAUP32_S 32
#define IRDMA_CQ_IMMDATAUP32_M (0xffffffffULL << IRDMA_CQ_IMMDATAUP32_S)

#define IRDMACQ_PAYLDLEN_S 0
#define IRDMACQ_PAYLDLEN_M (0xffffffffULL << IRDMACQ_PAYLDLEN_S)

#define IRDMACQ_TCPSEQNUMRTT_S 32
#define IRDMACQ_TCPSEQNUMRTT_M (0xffffffffULL << IRDMACQ_TCPSEQNUMRTT_S)

#define IRDMACQ_INVSTAG_S 0
#define IRDMACQ_INVSTAG_M (0xffffffffULL << IRDMACQ_INVSTAG_S)

#define IRDMACQ_QPID_S 32
#define IRDMACQ_QPID_M (0xffffffULL << IRDMACQ_QPID_S)

#define IRDMACQ_UDSRCQPN_S 0
#define IRDMACQ_UDSRCQPN_M (0xffffffffULL << IRDMACQ_UDSRCQPN_S)

#define IRDMACQ_PSHDROP_S 51
#define IRDMACQ_PSHDROP_M BIT_ULL(IRDMACQ_PSHDROP_S)

#define IRDMACQ_STAG_S 53
#define IRDMACQ_STAG_M BIT_ULL(IRDMACQ_STAG_S)

#define IRDMACQ_IPV4_S 53
#define IRDMACQ_IPV4_M BIT_ULL(IRDMACQ_IPV4_S)

#define IRDMACQ_SOEVENT_S 54
#define IRDMACQ_SOEVENT_M BIT_ULL(IRDMACQ_SOEVENT_S)

#define IRDMACQ_OP_S 56
#define IRDMACQ_OP_M (0x3fULL << IRDMACQ_OP_S)

/* Manage Push Page - MPP */
#define IRDMA_INVALID_PUSH_PAGE_INDEX_GEN_1 0xffff
#define IRDMA_INVALID_PUSH_PAGE_INDEX 0xffffffff

/* iwarp QP SQ WQE common fields */
#define IRDMAQPSQ_OPCODE_S 32
#define IRDMAQPSQ_OPCODE_M (0x3fULL << IRDMAQPSQ_OPCODE_S)

#define IRDMAQPSQ_COPY_HOST_PBL_S 43
#define IRDMAQPSQ_COPY_HOST_PBL_M BIT_ULL(IRDMAQPSQ_COPY_HOST_PBL_S)

#define IRDMAQPSQ_ADDFRAGCNT_S 38
#define IRDMAQPSQ_ADDFRAGCNT_M (0xfULL << IRDMAQPSQ_ADDFRAGCNT_S)

#define IRDMAQPSQ_PUSHWQE_S 56
#define IRDMAQPSQ_PUSHWQE_M BIT_ULL(IRDMAQPSQ_PUSHWQE_S)

#define IRDMAQPSQ_STREAMMODE_S 58
#define IRDMAQPSQ_STREAMMODE_M BIT_ULL(IRDMAQPSQ_STREAMMODE_S)

#define IRDMAQPSQ_WAITFORRCVPDU_S 59
#define IRDMAQPSQ_WAITFORRCVPDU_M BIT_ULL(IRDMAQPSQ_WAITFORRCVPDU_S)

#define IRDMAQPSQ_READFENCE_S 60
#define IRDMAQPSQ_READFENCE_M BIT_ULL(IRDMAQPSQ_READFENCE_S)

#define IRDMAQPSQ_LOCALFENCE_S 61
#define IRDMAQPSQ_LOCALFENCE_M BIT_ULL(IRDMAQPSQ_LOCALFENCE_S)

#define IRDMAQPSQ_UDPHEADER_S 61
#define IRDMAQPSQ_UDPHEADER_M BIT_ULL(IRDMAQPSQ_UDPHEADER_S)

#define IRDMAQPSQ_L4LEN_S 42
#define IRDMAQPSQ_L4LEN_M ((u64)0xF << IRDMAQPSQ_L4LEN_S)

#define IRDMAQPSQ_SIGCOMPL_S 62
#define IRDMAQPSQ_SIGCOMPL_M BIT_ULL(IRDMAQPSQ_SIGCOMPL_S)

#define IRDMAQPSQ_VALID_S 63
#define IRDMAQPSQ_VALID_M BIT_ULL(IRDMAQPSQ_VALID_S)

#define IRDMAQPSQ_FRAG_TO_S IRDMA_CQPHC_QPCTX_S
#define IRDMAQPSQ_FRAG_TO_M IRDMA_CQPHC_QPCTX_M

#define IRDMAQPSQ_FRAG_VALID_S 63
#define IRDMAQPSQ_FRAG_VALID_M BIT_ULL(IRDMAQPSQ_FRAG_VALID_S)

#define IRDMAQPSQ_FRAG_LEN_S 32
#define IRDMAQPSQ_FRAG_LEN_M (0x7fffffffULL << IRDMAQPSQ_FRAG_LEN_S)

#define IRDMAQPSQ_FRAG_STAG_S 0
#define IRDMAQPSQ_FRAG_STAG_M (0xffffffffULL << IRDMAQPSQ_FRAG_STAG_S)

#define IRDMAQPSQ_GEN1_FRAG_LEN_S 0
#define IRDMAQPSQ_GEN1_FRAG_LEN_M (0xffffffffULL << IRDMAQPSQ_GEN1_FRAG_LEN_S)

#define IRDMAQPSQ_GEN1_FRAG_STAG_S 32
#define IRDMAQPSQ_GEN1_FRAG_STAG_M (0xffffffffULL << IRDMAQPSQ_GEN1_FRAG_STAG_S)

#define IRDMAQPSQ_REMSTAGINV_S 0
#define IRDMAQPSQ_REMSTAGINV_M (0xffffffffULL << IRDMAQPSQ_REMSTAGINV_S)

#define IRDMAQPSQ_DESTQKEY_S 0
#define IRDMAQPSQ_DESTQKEY_M (0xffffffffULL << IRDMAQPSQ_DESTQKEY_S)

#define IRDMAQPSQ_DESTQPN_S 32
#define IRDMAQPSQ_DESTQPN_M (0x00ffffffULL << IRDMAQPSQ_DESTQPN_S)

#define IRDMAQPSQ_AHID_S 0
#define IRDMAQPSQ_AHID_M (0x0001ffffULL << IRDMAQPSQ_AHID_S)

#define IRDMAQPSQ_INLINEDATAFLAG_S 57
#define IRDMAQPSQ_INLINEDATAFLAG_M BIT_ULL(IRDMAQPSQ_INLINEDATAFLAG_S)

#define IRDMA_INLINE_VALID_S 7

#define IRDMAQPSQ_INLINEDATALEN_S 48
#define IRDMAQPSQ_INLINEDATALEN_M \
	(0xffULL << IRDMAQPSQ_INLINEDATALEN_S)
#define IRDMAQPSQ_IMMDATAFLAG_S 47
#define IRDMAQPSQ_IMMDATAFLAG_M \
	BIT_ULL(IRDMAQPSQ_IMMDATAFLAG_S)
#define IRDMAQPSQ_REPORTRTT_S 46
#define IRDMAQPSQ_REPORTRTT_M \
	BIT_ULL(IRDMAQPSQ_REPORTRTT_S)

#define IRDMAQPSQ_IMMDATA_S 0
#define IRDMAQPSQ_IMMDATA_M \
	(0xffffffffffffffffULL << IRDMAQPSQ_IMMDATA_S)

/* rdma write */
#define IRDMAQPSQ_REMSTAG_S 0
#define IRDMAQPSQ_REMSTAG_M (0xffffffffULL << IRDMAQPSQ_REMSTAG_S)

#define IRDMAQPSQ_REMTO_S IRDMA_CQPHC_QPCTX_S
#define IRDMAQPSQ_REMTO_M IRDMA_CQPHC_QPCTX_M

/* memory window */
#define IRDMAQPSQ_STAGRIGHTS_S 48
#define IRDMAQPSQ_STAGRIGHTS_M (0x1fULL << IRDMAQPSQ_STAGRIGHTS_S)

#define IRDMAQPSQ_VABASEDTO_S 53
#define IRDMAQPSQ_VABASEDTO_M BIT_ULL(IRDMAQPSQ_VABASEDTO_S)

#define IRDMAQPSQ_MEMWINDOWTYPE_S 54
#define IRDMAQPSQ_MEMWINDOWTYPE_M BIT_ULL(IRDMAQPSQ_MEMWINDOWTYPE_S)

#define IRDMAQPSQ_MWLEN_S IRDMA_CQPHC_QPCTX_S
#define IRDMAQPSQ_MWLEN_M IRDMA_CQPHC_QPCTX_M

#define IRDMAQPSQ_PARENTMRSTAG_S 32
#define IRDMAQPSQ_PARENTMRSTAG_M \
	(0xffffffffULL << IRDMAQPSQ_PARENTMRSTAG_S)

#define IRDMAQPSQ_MWSTAG_S 0
#define IRDMAQPSQ_MWSTAG_M (0xffffffffULL << IRDMAQPSQ_MWSTAG_S)

#define IRDMAQPSQ_BASEVA_TO_FBO_S IRDMA_CQPHC_QPCTX_S
#define IRDMAQPSQ_BASEVA_TO_FBO_M IRDMA_CQPHC_QPCTX_M

/* Local Invalidate */
#define IRDMAQPSQ_LOCSTAG_S 0
#define IRDMAQPSQ_LOCSTAG_M (0xffffffffULL << IRDMAQPSQ_LOCSTAG_S)

/* iwarp QP RQ WQE common fields */
#define IRDMAQPRQ_ADDFRAGCNT_S IRDMAQPSQ_ADDFRAGCNT_S
#define IRDMAQPRQ_ADDFRAGCNT_M IRDMAQPSQ_ADDFRAGCNT_M

#define IRDMAQPRQ_VALID_S IRDMAQPSQ_VALID_S
#define IRDMAQPRQ_VALID_M IRDMAQPSQ_VALID_M

#define IRDMAQPRQ_COMPLCTX_S IRDMA_CQPHC_QPCTX_S
#define IRDMAQPRQ_COMPLCTX_M IRDMA_CQPHC_QPCTX_M

#define IRDMAQPRQ_FRAG_LEN_S IRDMAQPSQ_FRAG_LEN_S
#define IRDMAQPRQ_FRAG_LEN_M IRDMAQPSQ_FRAG_LEN_M

#define IRDMAQPRQ_STAG_S IRDMAQPSQ_FRAG_STAG_S
#define IRDMAQPRQ_STAG_M IRDMAQPSQ_FRAG_STAG_M

#define IRDMAQPRQ_TO_S IRDMAQPSQ_FRAG_TO_S
#define IRDMAQPRQ_TO_M IRDMAQPSQ_FRAG_TO_M

#define IRDMAPFINT_OICR_HMC_ERR_M BIT(26)
#define IRDMAPFINT_OICR_PE_PUSH_M BIT(27)
#define IRDMAPFINT_OICR_PE_CRITERR_M BIT(28)

#define IRDMA_GET_CURRENT_CQ_ELEM(_cq) \
	( \
		(_cq)->cq_base[IRDMA_RING_CURRENT_HEAD((_cq)->cq_ring)].buf  \
	)
#define IRDMA_GET_CURRENT_EXTENDED_CQ_ELEM(_cq) \
	( \
		((struct irdma_extended_cqe *) \
		((_cq)->cq_base))[IRDMA_RING_CURRENT_HEAD((_cq)->cq_ring)].buf \
	)

#define IRDMA_RING_INIT(_ring, _size) \
	{ \
		(_ring).head = 0; \
		(_ring).tail = 0; \
		(_ring).size = (_size); \
	}
#define IRDMA_RING_SIZE(_ring) ((_ring).size)
#define IRDMA_RING_CURRENT_HEAD(_ring) ((_ring).head)
#define IRDMA_RING_CURRENT_TAIL(_ring) ((_ring).tail)

#define IRDMA_RING_MOVE_HEAD(_ring, _retcode) \
	{ \
		register u32 size; \
		size = (_ring).size;  \
		if (!IRDMA_RING_FULL_ERR(_ring)) { \
			(_ring).head = ((_ring).head + 1) % size; \
			(_retcode) = 0; \
		} else { \
			(_retcode) = ENOSPC; \
		} \
	}
#define IRDMA_RING_MOVE_HEAD_BY_COUNT(_ring, _count, _retcode) \
	{ \
		register u32 size; \
		size = (_ring).size; \
		if ((IRDMA_RING_USED_QUANTA(_ring) + (_count)) < size) { \
			(_ring).head = ((_ring).head + (_count)) % size; \
			(_retcode) = 0; \
		} else { \
			(_retcode) = ENOSPC; \
		} \
	}
#define IRDMA_SQ_RING_MOVE_HEAD(_ring, _retcode) \
	{ \
		register u32 size; \
		size = (_ring).size;  \
		if (!IRDMA_SQ_RING_FULL_ERR(_ring)) { \
			(_ring).head = ((_ring).head + 1) % size; \
			(_retcode) = 0; \
		} else { \
			(_retcode) = ENOSPC; \
		} \
	}
#define IRDMA_SQ_RING_MOVE_HEAD_BY_COUNT(_ring, _count, _retcode) \
	{ \
		register u32 size; \
		size = (_ring).size; \
		if ((IRDMA_RING_USED_QUANTA(_ring) + (_count)) < (size - 256)) { \
			(_ring).head = ((_ring).head + (_count)) % size; \
			(_retcode) = 0; \
		} else { \
			(_retcode) = ENOSPC; \
		} \
	}
#define IRDMA_RING_MOVE_HEAD_BY_COUNT_NOCHECK(_ring, _count) \
	(_ring).head = ((_ring).head + (_count)) % (_ring).size

#define IRDMA_RING_MOVE_TAIL(_ring) \
	(_ring).tail = ((_ring).tail + 1) % (_ring).size

#define IRDMA_RING_MOVE_HEAD_NOCHECK(_ring) \
	(_ring).head = ((_ring).head + 1) % (_ring).size

#define IRDMA_RING_MOVE_TAIL_BY_COUNT(_ring, _count) \
	(_ring).tail = ((_ring).tail + (_count)) % (_ring).size

#define IRDMA_RING_SET_TAIL(_ring, _pos) \
	(_ring).tail = (_pos) % (_ring).size

#define IRDMA_RING_FULL_ERR(_ring) \
	( \
		(IRDMA_RING_USED_QUANTA(_ring) == ((_ring).size - 1))  \
	)

#define IRDMA_ERR_RING_FULL2(_ring) \
	( \
		(IRDMA_RING_USED_QUANTA(_ring) == ((_ring).size - 2))  \
	)

#define IRDMA_ERR_RING_FULL3(_ring) \
	( \
		(IRDMA_RING_USED_QUANTA(_ring) == ((_ring).size - 3))  \
	)

#define IRDMA_SQ_RING_FULL_ERR(_ring) \
	( \
		(IRDMA_RING_USED_QUANTA(_ring) == ((_ring).size - 257))  \
	)

#define IRDMA_ERR_SQ_RING_FULL2(_ring) \
	( \
		(IRDMA_RING_USED_QUANTA(_ring) == ((_ring).size - 258))  \
	)
#define IRDMA_ERR_SQ_RING_FULL3(_ring) \
	( \
		(IRDMA_RING_USED_QUANTA(_ring) == ((_ring).size - 259))  \
	)
#define IRDMA_RING_MORE_WORK(_ring) \
	( \
		(IRDMA_RING_USED_QUANTA(_ring) != 0) \
	)

#define IRDMA_RING_USED_QUANTA(_ring) \
	( \
		(((_ring).head + (_ring).size - (_ring).tail) % (_ring).size) \
	)

#define IRDMA_RING_FREE_QUANTA(_ring) \
	( \
		((_ring).size - IRDMA_RING_USED_QUANTA(_ring) - 1) \
	)

#define IRDMA_SQ_RING_FREE_QUANTA(_ring) \
	( \
		((_ring).size - IRDMA_RING_USED_QUANTA(_ring) - 257) \
	)

#define IRDMA_ATOMIC_RING_MOVE_HEAD(_ring, index, _retcode) \
	{ \
		index = IRDMA_RING_CURRENT_HEAD(_ring); \
		IRDMA_RING_MOVE_HEAD(_ring, _retcode); \
	}

enum irdma_qp_wqe_size {
	IRDMA_WQE_SIZE_32  = 32,
	IRDMA_WQE_SIZE_64  = 64,
	IRDMA_WQE_SIZE_96  = 96,
	IRDMA_WQE_SIZE_128 = 128,
	IRDMA_WQE_SIZE_256 = 256,
};

/**
 * set_64bit_val - set 64 bit value to hw wqe
 * @wqe_words: wqe addr to write
 * @byte_index: index in wqe
 * @val: value to write
 **/
static inline void set_64bit_val(__le64 *wqe_words, u32 byte_index, u64 val)
{
	wqe_words[byte_index >> 3] = htole64(val);
}

/**
 * set_32bit_val - set 32 bit value to hw wqe
 * @wqe_words: wqe addr to write
 * @byte_index: index in wqe
 * @val: value to write
 **/
static inline void set_32bit_val(__le32 *wqe_words, u32 byte_index, u32 val)
{
	wqe_words[byte_index >> 2] = htole32(val);
}

/**
 * get_64bit_val - read 64 bit value from wqe
 * @wqe_words: wqe addr
 * @byte_index: index to read from
 * @val: read value
 **/
static inline void get_64bit_val(__le64 *wqe_words, u32 byte_index, u64 *val)
{
	*val = le64toh(wqe_words[byte_index >> 3]);
}

/**
 * get_32bit_val - read 32 bit value from wqe
 * @wqe_words: wqe addr
 * @byte_index: index to reaad from
 * @val: return 32 bit value
 **/
static inline void get_32bit_val(__le32 *wqe_words, u32 byte_index, u32 *val)
{
	*val = le32toh(wqe_words[byte_index >> 2]);
}
#endif /* IRDMA_DEFS_H */
