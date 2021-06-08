/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2017-2018 Chelsio Communications, Inc.
 * All rights reserved.
 * Written by: John Baldwin <jhb@FreeBSD.org>, Atul Gupta
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
 * $FreeBSD$
 *
 */

#ifndef __T4_TLS_H__
#define __T4_TLS_H__

#ifdef _KERNEL

/* Timeouts for handshake timer in seconds. */
#define TLS_SRV_HELLO_DONE		9
#define TLS_SRV_HELLO_RD_TM		5
#define TLS_SRV_HELLO_BKOFF_TM		15

#define CONTENT_TYPE_CCS		20
#define CONTENT_TYPE_ALERT		21
#define CONTENT_TYPE_HANDSHAKE		22
#define CONTENT_TYPE_APP_DATA		23
#define CONTENT_TYPE_HEARTBEAT		24
#define CONTENT_TYPE_KEY_CONTEXT	32
#define CONTENT_TYPE_ERROR		127

#define TLS_HEADER_LENGTH		5
#define TP_TX_PG_SZ			65536
#define FC_TP_PLEN_MAX			17408

enum {
	TLS_SFO_WR_CONTEXTLOC_DSGL,
	TLS_SFO_WR_CONTEXTLOC_IMMEDIATE,
	TLS_SFO_WR_CONTEXTLOC_DDR,
};

enum {
	CPL_TX_TLS_SFO_TYPE_CCS,
	CPL_TX_TLS_SFO_TYPE_ALERT,
	CPL_TX_TLS_SFO_TYPE_HANDSHAKE,
	CPL_TX_TLS_SFO_TYPE_DATA,
	CPL_TX_TLS_SFO_TYPE_HEARTBEAT,	/* XXX: Shouldn't this be "CUSTOM"? */
};

struct tls_scmd {
	__be32 seqno_numivs;
	__be32 ivgen_hdrlen;
};

struct tls_ofld_info {
	unsigned int frag_size;
	int key_location;
	int rx_key_addr;
	int tx_key_addr;
	uint64_t tx_seq_no;
	uint16_t rx_version;
	unsigned short fcplenmax;
	unsigned short adjusted_plen;
	unsigned short expn_per_ulp;
	unsigned short pdus_per_ulp;
	struct tls_scmd scmd0;
	u_int iv_len;
	unsigned int tx_key_info_size;
	struct callout handshake_timer;
};

struct tls_hdr {
	__u8   type;
	__be16 version;
	__be16 length;
} __packed;

struct tlsrx_hdr_pkt {
	__u8   type;
	__be16 version;
	__be16 length;

	__be64 tls_seq;
	__be16 reserved1;
	__u8   res_to_mac_error;
} __packed;

/* res_to_mac_error fields */
#define S_TLSRX_HDR_PKT_INTERNAL_ERROR   4
#define M_TLSRX_HDR_PKT_INTERNAL_ERROR   0x1
#define V_TLSRX_HDR_PKT_INTERNAL_ERROR(x) \
	((x) << S_TLSRX_HDR_PKT_INTERNAL_ERROR)
#define G_TLSRX_HDR_PKT_INTERNAL_ERROR(x) \
(((x) >> S_TLSRX_HDR_PKT_INTERNAL_ERROR) & M_TLSRX_HDR_PKT_INTERNAL_ERROR)
#define F_TLSRX_HDR_PKT_INTERNAL_ERROR   V_TLSRX_HDR_PKT_INTERNAL_ERROR(1U)

#define S_TLSRX_HDR_PKT_SPP_ERROR        3
#define M_TLSRX_HDR_PKT_SPP_ERROR        0x1
#define V_TLSRX_HDR_PKT_SPP_ERROR(x)     ((x) << S_TLSRX_HDR_PKT_SPP_ERROR)
#define G_TLSRX_HDR_PKT_SPP_ERROR(x)     \
(((x) >> S_TLSRX_HDR_PKT_SPP_ERROR) & M_TLSRX_HDR_PKT_SPP_ERROR)
#define F_TLSRX_HDR_PKT_SPP_ERROR        V_TLSRX_HDR_PKT_SPP_ERROR(1U)

#define S_TLSRX_HDR_PKT_CCDX_ERROR       2
#define M_TLSRX_HDR_PKT_CCDX_ERROR       0x1
#define V_TLSRX_HDR_PKT_CCDX_ERROR(x)    ((x) << S_TLSRX_HDR_PKT_CCDX_ERROR)
#define G_TLSRX_HDR_PKT_CCDX_ERROR(x)    \
(((x) >> S_TLSRX_HDR_PKT_CCDX_ERROR) & M_TLSRX_HDR_PKT_CCDX_ERROR)
#define F_TLSRX_HDR_PKT_CCDX_ERROR       V_TLSRX_HDR_PKT_CCDX_ERROR(1U)

#define S_TLSRX_HDR_PKT_PAD_ERROR        1
#define M_TLSRX_HDR_PKT_PAD_ERROR        0x1
#define V_TLSRX_HDR_PKT_PAD_ERROR(x)     ((x) << S_TLSRX_HDR_PKT_PAD_ERROR)
#define G_TLSRX_HDR_PKT_PAD_ERROR(x)     \
(((x) >> S_TLSRX_HDR_PKT_PAD_ERROR) & M_TLSRX_HDR_PKT_PAD_ERROR)
#define F_TLSRX_HDR_PKT_PAD_ERROR        V_TLSRX_HDR_PKT_PAD_ERROR(1U)

#define S_TLSRX_HDR_PKT_MAC_ERROR        0
#define M_TLSRX_HDR_PKT_MAC_ERROR        0x1
#define V_TLSRX_HDR_PKT_MAC_ERROR(x)     ((x) << S_TLSRX_HDR_PKT_MAC_ERROR)
#define G_TLSRX_HDR_PKT_MAC_ERROR(x)     \
(((x) >> S_TLSRX_HDR_PKT_MAC_ERROR) & M_TLSRX_HDR_PKT_MAC_ERROR)
#define F_TLSRX_HDR_PKT_MAC_ERROR        V_TLSRX_HDR_PKT_MAC_ERROR(1U)

#define M_TLSRX_HDR_PKT_ERROR		0x1F

#endif /* _KERNEL */

#endif /* !__T4_TLS_H__ */
