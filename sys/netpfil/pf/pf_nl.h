/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Alexander V. Chernikov <melifaro@FreeBSD.org>
 * Copyright (c) 2023 Rubicon Communications, LLC (Netgate)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef _NETPFIL_PF_PF_NL_H_
#define _NETPFIL_PF_PF_NL_H_

/* Genetlink family */
#define PFNL_FAMILY_NAME	"pfctl"

/* available commands */
enum {
	PFNL_CMD_UNSPEC = 0,
	PFNL_CMD_GETSTATES = 1,
	PFNL_CMD_GETCREATORS = 2,
	PFNL_CMD_START = 3,
	PFNL_CMD_STOP = 4,
	PFNL_CMD_ADDRULE = 5,
	PFNL_CMD_GETRULES = 6,
	PFNL_CMD_GETRULE = 7,
	PFNL_CMD_CLRSTATES = 8,
	PFNL_CMD_KILLSTATES = 9,
	PFNL_CMD_SET_STATUSIF = 10,
	__PFNL_CMD_MAX,
};
#define PFNL_CMD_MAX (__PFNL_CMD_MAX -1)

enum pfstate_key_type_t {
	PF_STK_UNSPEC,
	PF_STK_ADDR0		= 1, /* ip */
	PF_STK_ADDR1		= 2, /* ip */
	PF_STK_PORT0		= 3, /* u16 */
	PF_STK_PORT1		= 4, /* u16 */
};

enum pfstate_peer_type_t {
	PF_STP_UNSPEC,
	PF_STP_PFSS_FLAGS	= 1, /* u16 */
	PF_STP_PFSS_TTL		= 2, /* u8 */
	PF_STP_SCRUB_FLAG	= 3, /* u8 */
	PF_STP_PFSS_TS_MOD	= 4, /* u32 */
	PF_STP_SEQLO		= 5, /* u32 */
	PF_STP_SEQHI		= 6, /* u32 */
	PF_STP_SEQDIFF		= 7, /* u32 */
	PF_STP_MAX_WIN		= 8, /* u16 */
	PF_STP_MSS		= 9, /* u16 */
	PF_STP_STATE		= 10, /* u8 */
	PF_STP_WSCALE		= 11, /* u8 */
};

enum pfstate_type_t {
	PF_ST_UNSPEC,
	PF_ST_ID		= 1, /* u32, state id */
	PF_ST_CREATORID		= 2, /* u32, */
	PF_ST_IFNAME		= 3, /* string */
	PF_ST_ORIG_IFNAME	= 4, /* string */
	PF_ST_KEY_WIRE		= 5, /* nested, pfstate_key_type_t */
	PF_ST_KEY_STACK		= 6, /* nested, pfstate_key_type_t */
	PF_ST_PEER_SRC		= 7, /* nested, pfstate_peer_type_t*/
	PF_ST_PEER_DST		= 8, /* nested, pfstate_peer_type_t */
	PF_ST_RT_ADDR		= 9, /* ip */
	PF_ST_RULE		= 10, /* u32 */
	PF_ST_ANCHOR		= 11, /* u32 */
	PF_ST_NAT_RULE		= 12, /* u32 */
	PF_ST_CREATION		= 13, /* u32 */
	PF_ST_EXPIRE		= 14, /* u32 */
	PF_ST_PACKETS0		= 15, /* u64 */
	PF_ST_PACKETS1		= 16, /* u64 */
	PF_ST_BYTES0		= 17, /* u64 */
	PF_ST_BYTES1		= 18, /* u64 */
	PF_ST_AF		= 19, /* u8 */
	PF_ST_PROTO		= 21, /* u8 */
	PF_ST_DIRECTION		= 22, /* u8 */
	PF_ST_LOG		= 23, /* u8 */
	PF_ST_TIMEOUT		= 24, /* u8 */
	PF_ST_STATE_FLAGS	= 25, /* u8 */
	PF_ST_SYNC_FLAGS	= 26, /* u8 */
	PF_ST_UPDATES		= 27, /* u8 */
	PF_ST_VERSION		= 28, /* u64 */
	PF_ST_FILTER_ADDR	= 29, /* in6_addr */
	PF_ST_FILTER_MASK	= 30, /* in6_addr */
	PF_ST_RTABLEID		= 31, /* i32 */
	PF_ST_MIN_TTL		= 32, /* u8 */
	PF_ST_MAX_MSS		= 33, /* u16 */
	PF_ST_DNPIPE		= 34, /* u16 */
	PF_ST_DNRPIPE		= 35, /* u16 */
	PF_ST_RT		= 36, /* u8 */
	PF_ST_RT_IFNAME		= 37, /* string */
};

enum pf_addr_type_t {
	PF_AT_UNSPEC,
	PF_AT_ADDR		= 1, /* in6_addr */
	PF_AT_MASK		= 2, /* in6_addr */
	PF_AT_IFNAME		= 3, /* string */
	PF_AT_TABLENAME		= 4, /* string */
	PF_AT_TYPE		= 5, /* u8 */
	PF_AT_IFLAGS		= 6, /* u8 */
	PF_AT_TBLCNT		= 7, /* u32 */
	PF_AT_DYNCNT		= 8, /* u32 */
};

enum pfrule_addr_type_t {
	PF_RAT_UNSPEC,
	PF_RAT_ADDR		= 1, /* nested, pf_addr_type_t */
	PF_RAT_SRC_PORT		= 2, /* u16 */
	PF_RAT_DST_PORT		= 3, /* u16 */
	PF_RAT_NEG		= 4, /* u8 */
	PF_RAT_OP		= 5, /* u8 */
};

enum pf_labels_type_t {
	PF_LT_UNSPEC,
	PF_LT_LABEL		= 1, /* string */
};

enum pf_mape_portset_type_t
{
	PF_MET_UNSPEC,
	PF_MET_OFFSET		= 1, /* u8 */
	PF_MET_PSID_LEN		= 2, /* u8 */
	PF_MET_PSID		= 3, /* u16 */
};

enum pf_rpool_type_t
{
	PF_PT_UNSPEC,
	PF_PT_KEY		= 1, /* bytes, sizeof(struct pf_poolhashkey) */
	PF_PT_COUNTER		= 2, /* in6_addr */
	PF_PT_TBLIDX		= 3, /* u32 */
	PF_PT_PROXY_SRC_PORT	= 4, /* u16 */
	PF_PT_PROXY_DST_PORT	= 5, /* u16 */
	PF_PT_OPTS		= 6, /* u8 */
	PF_PT_MAPE		= 7, /* nested, pf_mape_portset_type_t */
};

enum pf_timeout_type_t {
	PF_TT_UNSPEC,
	PF_TT_TIMEOUT		= 1, /* u32 */
};

enum pf_rule_uid_type_t {
	PF_RUT_UNSPEC,
	PF_RUT_UID_LOW		= 1, /* u32 */
	PF_RUT_UID_HIGH		= 2, /* u32 */
	PF_RUT_OP		= 3, /* u8 */
};

enum pf_rule_type_t {
	PF_RT_UNSPEC,
	PF_RT_SRC		= 1, /* nested, pf_rule_addr_type_t */
	PF_RT_DST		= 2, /* nested, pf_rule_addr_type_t */
	PF_RT_RIDENTIFIER	= 3, /* u32 */
	PF_RT_LABELS		= 4, /* nested, pf_labels_type_t */
	PF_RT_IFNAME		= 5, /* string */
	PF_RT_QNAME		= 6, /* string */
	PF_RT_PQNAME		= 7, /* string */
	PF_RT_TAGNAME		= 8, /* string */
	PF_RT_MATCH_TAGNAME	= 9, /* string */
	PF_RT_OVERLOAD_TBLNAME	= 10, /* string */
	PF_RT_RPOOL		= 11, /* nested, pf_rpool_type_t */
	PF_RT_OS_FINGERPRINT	= 12, /* u32 */
	PF_RT_RTABLEID		= 13, /* u32 */
	PF_RT_TIMEOUT		= 14, /* nested, pf_timeout_type_t */
	PF_RT_MAX_STATES	= 15, /* u32 */
	PF_RT_MAX_SRC_NODES	= 16, /* u32 */
	PF_RT_MAX_SRC_STATES	= 17, /* u32 */
	PF_RT_MAX_SRC_CONN_RATE_LIMIT	= 18, /* u32 */
	PF_RT_MAX_SRC_CONN_RATE_SECS	= 19, /* u32 */
	PF_RT_DNPIPE		= 20, /* u16 */
	PF_RT_DNRPIPE		= 21, /* u16 */
	PF_RT_DNFLAGS		= 22, /* u32 */
	PF_RT_NR		= 23, /* u32 */
	PF_RT_PROB		= 24, /* u32 */
	PF_RT_CUID		= 25, /* u32 */
	PF_RT_CPID		= 26, /* u32 */
	PF_RT_RETURN_ICMP	= 27, /* u16 */
	PF_RT_RETURN_ICMP6	= 28, /* u16 */
	PF_RT_MAX_MSS		= 29, /* u16 */
	PF_RT_SCRUB_FLAGS	= 30, /* u16 */
	PF_RT_UID		= 31, /* nested, pf_rule_uid_type_t */
	PF_RT_GID		= 32, /* nested, pf_rule_uid_type_t */
	PF_RT_RULE_FLAG		= 33, /* u32 */
	PF_RT_ACTION		= 34, /* u8 */
	PF_RT_DIRECTION		= 35, /* u8 */
	PF_RT_LOG		= 36, /* u8 */
	PF_RT_LOGIF		= 37, /* u8 */
	PF_RT_QUICK		= 38, /* u8 */
	PF_RT_IF_NOT		= 39, /* u8 */
	PF_RT_MATCH_TAG_NOT	= 40, /* u8 */
	PF_RT_NATPASS		= 41, /* u8 */
	PF_RT_KEEP_STATE	= 42, /* u8 */
	PF_RT_AF		= 43, /* u8 */
	PF_RT_PROTO		= 44, /* u8 */
	PF_RT_TYPE		= 45, /* u8 */
	PF_RT_CODE		= 46, /* u8 */
	PF_RT_FLAGS		= 47, /* u8 */
	PF_RT_FLAGSET		= 48, /* u8 */
	PF_RT_MIN_TTL		= 49, /* u8 */
	PF_RT_ALLOW_OPTS	= 50, /* u8 */
	PF_RT_RT		= 51, /* u8 */
	PF_RT_RETURN_TTL	= 52, /* u8 */
	PF_RT_TOS		= 53, /* u8 */
	PF_RT_SET_TOS		= 54, /* u8 */
	PF_RT_ANCHOR_RELATIVE	= 55, /* u8 */
	PF_RT_ANCHOR_WILDCARD	= 56, /* u8 */
	PF_RT_FLUSH		= 57, /* u8 */
	PF_RT_PRIO		= 58, /* u8 */
	PF_RT_SET_PRIO		= 59, /* u8 */
	PF_RT_SET_PRIO_REPLY	= 60, /* u8 */
	PF_RT_DIVERT_ADDRESS	= 61, /* in6_addr */
	PF_RT_DIVERT_PORT	= 62, /* u16 */
	PF_RT_PACKETS_IN	= 63, /* u64 */
	PF_RT_PACKETS_OUT	= 64, /* u64 */
	PF_RT_BYTES_IN		= 65, /* u64 */
	PF_RT_BYTES_OUT		= 66, /* u64 */
	PF_RT_EVALUATIONS	= 67, /* u64 */
	PF_RT_TIMESTAMP		= 68, /* u64 */
	PF_RT_STATES_CUR	= 69, /* u64 */
	PF_RT_STATES_TOTAL	= 70, /* u64 */
	PF_RT_SRC_NODES		= 71, /* u64 */
	PF_RT_ANCHOR_CALL	= 72, /* string */
};

enum pf_addrule_type_t {
	PF_ART_UNSPEC,
	PF_ART_TICKET		= 1, /* u32 */
	PF_ART_POOL_TICKET	= 2, /* u32 */
	PF_ART_ANCHOR		= 3, /* string */
	PF_ART_ANCHOR_CALL	= 4, /* string */
	PF_ART_RULE		= 5, /* nested, pfrule_type_t */
};

enum pf_getrules_type_t {
	PF_GR_UNSPEC,
	PF_GR_ANCHOR		= 1, /* string */
	PF_GR_ACTION		= 2, /* u8 */
	PF_GR_NR		= 3, /* u32 */
	PF_GR_TICKET		= 4, /* u32 */
	PF_GR_CLEAR		= 5, /* u8 */
};

enum pf_clear_states_type_t {
	PF_CS_UNSPEC,
	PF_CS_CMP_ID		= 1, /* u64 */
	PF_CS_CMP_CREATORID	= 2, /* u32 */
	PF_CS_CMP_DIR		= 3, /* u8 */
	PF_CS_AF		= 4, /* u8 */
	PF_CS_PROTO		= 5, /* u8 */
	PF_CS_SRC		= 6, /* nested, pf_addr_wrap */
	PF_CS_DST		= 7, /* nested, pf_addr_wrap */
	PF_CS_RT_ADDR		= 8, /* nested, pf_addr_wrap */
	PF_CS_IFNAME		= 9, /* string */
	PF_CS_LABEL		= 10, /* string */
	PF_CS_KILL_MATCH	= 11, /* bool */
	PF_CS_NAT		= 12, /* bool */
	PF_CS_KILLED		= 13, /* u32 */
};

enum pf_set_statusif_types_t {
	PF_SS_UNSPEC,
	PF_SS_IFNAME		= 1, /* string */
};
#ifdef _KERNEL

void	pf_nl_register(void);
void	pf_nl_unregister(void);

#endif

#endif
