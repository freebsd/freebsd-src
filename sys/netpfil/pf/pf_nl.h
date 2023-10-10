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
};

#ifdef _KERNEL

void	pf_nl_register(void);
void	pf_nl_unregister(void);

#endif

#endif
