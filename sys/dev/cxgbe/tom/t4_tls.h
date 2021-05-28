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

#define SALT_SIZE		4

/* Key Context Programming Operation type */
#define KEY_WRITE_RX			0x1
#define KEY_WRITE_TX			0x2
#define KEY_DELETE_RX			0x4
#define KEY_DELETE_TX			0x8

#define S_KEY_CLR_LOC		4
#define M_KEY_CLR_LOC		0xf
#define V_KEY_CLR_LOC(x)	((x) << S_KEY_CLR_LOC)
#define G_KEY_CLR_LOC(x)	(((x) >> S_KEY_CLR_LOC) & M_KEY_CLR_LOC)
#define F_KEY_CLR_LOC		V_KEY_CLR_LOC(1U)

#define S_KEY_GET_LOC           0
#define M_KEY_GET_LOC           0xf
#define V_KEY_GET_LOC(x)        ((x) << S_KEY_GET_LOC)
#define G_KEY_GET_LOC(x)        (((x) >> S_KEY_GET_LOC) & M_KEY_GET_LOC)

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

#define	TLS_KEY_CONTEXT_SZ	roundup2(sizeof(struct tls_keyctx), 32)

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
	CPL_TX_TLS_SFO_TYPE_CUSTOM,
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

struct tls_key_req {
	__be32 wr_hi;
	__be32 wr_mid;
        __be32 ftid;
        __u8   reneg_to_write_rx;
        __u8   protocol;
        __be16 mfs;
	/* master command */
	__be32 cmd;
	__be32 len16;             /* command length */
	__be32 dlen;              /* data length in 32-byte units */
	__be32 kaddr;
	/* sub-command */
	__be32 sc_more;
	__be32 sc_len;
}__packed;

struct tls_keyctx {
        union key_ctx {
                struct tx_keyctx_hdr {
                        __u8   ctxlen;
                        __u8   r2;
                        __be16 dualck_to_txvalid;
                        __u8   txsalt[4];
                        __be64 r5;
                } txhdr;
                struct rx_keyctx_hdr {
                        __u8   flitcnt_hmacctrl;
                        __u8   protover_ciphmode;
                        __u8   authmode_to_rxvalid;
                        __u8   ivpresent_to_rxmk_size;
                        __u8   rxsalt[4];
                        __be64 ivinsert_to_authinsrt;
                } rxhdr;
        } u;
        struct keys {
                __u8   edkey[32];
                __u8   ipad[64];
                __u8   opad[64];
        } keys;
};

#define S_TLS_KEYCTX_TX_WR_DUALCK    12
#define M_TLS_KEYCTX_TX_WR_DUALCK    0x1
#define V_TLS_KEYCTX_TX_WR_DUALCK(x) ((x) << S_TLS_KEYCTX_TX_WR_DUALCK)
#define G_TLS_KEYCTX_TX_WR_DUALCK(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_DUALCK) & M_TLS_KEYCTX_TX_WR_DUALCK)
#define F_TLS_KEYCTX_TX_WR_DUALCK    V_TLS_KEYCTX_TX_WR_DUALCK(1U)

#define S_TLS_KEYCTX_TX_WR_TXOPAD_PRESENT 11
#define M_TLS_KEYCTX_TX_WR_TXOPAD_PRESENT 0x1
#define V_TLS_KEYCTX_TX_WR_TXOPAD_PRESENT(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_TXOPAD_PRESENT)
#define G_TLS_KEYCTX_TX_WR_TXOPAD_PRESENT(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_TXOPAD_PRESENT) & \
     M_TLS_KEYCTX_TX_WR_TXOPAD_PRESENT)
#define F_TLS_KEYCTX_TX_WR_TXOPAD_PRESENT \
    V_TLS_KEYCTX_TX_WR_TXOPAD_PRESENT(1U)

#define S_TLS_KEYCTX_TX_WR_SALT_PRESENT 10
#define M_TLS_KEYCTX_TX_WR_SALT_PRESENT 0x1
#define V_TLS_KEYCTX_TX_WR_SALT_PRESENT(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_SALT_PRESENT)
#define G_TLS_KEYCTX_TX_WR_SALT_PRESENT(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_SALT_PRESENT) & \
     M_TLS_KEYCTX_TX_WR_SALT_PRESENT)
#define F_TLS_KEYCTX_TX_WR_SALT_PRESENT \
    V_TLS_KEYCTX_TX_WR_SALT_PRESENT(1U)

#define S_TLS_KEYCTX_TX_WR_TXCK_SIZE 6
#define M_TLS_KEYCTX_TX_WR_TXCK_SIZE 0xf
#define V_TLS_KEYCTX_TX_WR_TXCK_SIZE(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_TXCK_SIZE)
#define G_TLS_KEYCTX_TX_WR_TXCK_SIZE(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_TXCK_SIZE) & \
     M_TLS_KEYCTX_TX_WR_TXCK_SIZE)

#define S_TLS_KEYCTX_TX_WR_TXMK_SIZE 2
#define M_TLS_KEYCTX_TX_WR_TXMK_SIZE 0xf
#define V_TLS_KEYCTX_TX_WR_TXMK_SIZE(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_TXMK_SIZE)
#define G_TLS_KEYCTX_TX_WR_TXMK_SIZE(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_TXMK_SIZE) & \
     M_TLS_KEYCTX_TX_WR_TXMK_SIZE)

#define S_TLS_KEYCTX_TX_WR_TXVALID   0
#define M_TLS_KEYCTX_TX_WR_TXVALID   0x1
#define V_TLS_KEYCTX_TX_WR_TXVALID(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_TXVALID)
#define G_TLS_KEYCTX_TX_WR_TXVALID(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_TXVALID) & M_TLS_KEYCTX_TX_WR_TXVALID)
#define F_TLS_KEYCTX_TX_WR_TXVALID   V_TLS_KEYCTX_TX_WR_TXVALID(1U)

#define S_TLS_KEYCTX_TX_WR_FLITCNT   3
#define M_TLS_KEYCTX_TX_WR_FLITCNT   0x1f
#define V_TLS_KEYCTX_TX_WR_FLITCNT(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_FLITCNT)
#define G_TLS_KEYCTX_TX_WR_FLITCNT(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_FLITCNT) & M_TLS_KEYCTX_TX_WR_FLITCNT)

#define S_TLS_KEYCTX_TX_WR_HMACCTRL  0
#define M_TLS_KEYCTX_TX_WR_HMACCTRL  0x7
#define V_TLS_KEYCTX_TX_WR_HMACCTRL(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_HMACCTRL)
#define G_TLS_KEYCTX_TX_WR_HMACCTRL(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_HMACCTRL) & M_TLS_KEYCTX_TX_WR_HMACCTRL)

#define S_TLS_KEYCTX_TX_WR_PROTOVER  4
#define M_TLS_KEYCTX_TX_WR_PROTOVER  0xf
#define V_TLS_KEYCTX_TX_WR_PROTOVER(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_PROTOVER)
#define G_TLS_KEYCTX_TX_WR_PROTOVER(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_PROTOVER) & M_TLS_KEYCTX_TX_WR_PROTOVER)

#define S_TLS_KEYCTX_TX_WR_CIPHMODE  0
#define M_TLS_KEYCTX_TX_WR_CIPHMODE  0xf
#define V_TLS_KEYCTX_TX_WR_CIPHMODE(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_CIPHMODE)
#define G_TLS_KEYCTX_TX_WR_CIPHMODE(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_CIPHMODE) & M_TLS_KEYCTX_TX_WR_CIPHMODE)

#define S_TLS_KEYCTX_TX_WR_AUTHMODE  4
#define M_TLS_KEYCTX_TX_WR_AUTHMODE  0xf
#define V_TLS_KEYCTX_TX_WR_AUTHMODE(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_AUTHMODE)
#define G_TLS_KEYCTX_TX_WR_AUTHMODE(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_AUTHMODE) & M_TLS_KEYCTX_TX_WR_AUTHMODE)

#define S_TLS_KEYCTX_TX_WR_CIPHAUTHSEQCTRL 3
#define M_TLS_KEYCTX_TX_WR_CIPHAUTHSEQCTRL 0x1
#define V_TLS_KEYCTX_TX_WR_CIPHAUTHSEQCTRL(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_CIPHAUTHSEQCTRL)
#define G_TLS_KEYCTX_TX_WR_CIPHAUTHSEQCTRL(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_CIPHAUTHSEQCTRL) & \
     M_TLS_KEYCTX_TX_WR_CIPHAUTHSEQCTRL)
#define F_TLS_KEYCTX_TX_WR_CIPHAUTHSEQCTRL \
    V_TLS_KEYCTX_TX_WR_CIPHAUTHSEQCTRL(1U)

#define S_TLS_KEYCTX_TX_WR_SEQNUMCTRL 1
#define M_TLS_KEYCTX_TX_WR_SEQNUMCTRL 0x3
#define V_TLS_KEYCTX_TX_WR_SEQNUMCTRL(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_SEQNUMCTRL)
#define G_TLS_KEYCTX_TX_WR_SEQNUMCTRL(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_SEQNUMCTRL) & \
     M_TLS_KEYCTX_TX_WR_SEQNUMCTRL)

#define S_TLS_KEYCTX_TX_WR_RXVALID   0
#define M_TLS_KEYCTX_TX_WR_RXVALID   0x1
#define V_TLS_KEYCTX_TX_WR_RXVALID(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_RXVALID)
#define G_TLS_KEYCTX_TX_WR_RXVALID(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_RXVALID) & M_TLS_KEYCTX_TX_WR_RXVALID)
#define F_TLS_KEYCTX_TX_WR_RXVALID   V_TLS_KEYCTX_TX_WR_RXVALID(1U)

#define S_TLS_KEYCTX_TX_WR_IVPRESENT 7
#define M_TLS_KEYCTX_TX_WR_IVPRESENT 0x1
#define V_TLS_KEYCTX_TX_WR_IVPRESENT(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_IVPRESENT)
#define G_TLS_KEYCTX_TX_WR_IVPRESENT(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_IVPRESENT) & \
     M_TLS_KEYCTX_TX_WR_IVPRESENT)
#define F_TLS_KEYCTX_TX_WR_IVPRESENT V_TLS_KEYCTX_TX_WR_IVPRESENT(1U)

#define S_TLS_KEYCTX_TX_WR_RXOPAD_PRESENT 6
#define M_TLS_KEYCTX_TX_WR_RXOPAD_PRESENT 0x1
#define V_TLS_KEYCTX_TX_WR_RXOPAD_PRESENT(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_RXOPAD_PRESENT)
#define G_TLS_KEYCTX_TX_WR_RXOPAD_PRESENT(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_RXOPAD_PRESENT) & \
     M_TLS_KEYCTX_TX_WR_RXOPAD_PRESENT)
#define F_TLS_KEYCTX_TX_WR_RXOPAD_PRESENT \
    V_TLS_KEYCTX_TX_WR_RXOPAD_PRESENT(1U)

#define S_TLS_KEYCTX_TX_WR_RXCK_SIZE 3
#define M_TLS_KEYCTX_TX_WR_RXCK_SIZE 0x7
#define V_TLS_KEYCTX_TX_WR_RXCK_SIZE(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_RXCK_SIZE)
#define G_TLS_KEYCTX_TX_WR_RXCK_SIZE(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_RXCK_SIZE) & \
     M_TLS_KEYCTX_TX_WR_RXCK_SIZE)

#define S_TLS_KEYCTX_TX_WR_RXMK_SIZE 0
#define M_TLS_KEYCTX_TX_WR_RXMK_SIZE 0x7
#define V_TLS_KEYCTX_TX_WR_RXMK_SIZE(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_RXMK_SIZE)
#define G_TLS_KEYCTX_TX_WR_RXMK_SIZE(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_RXMK_SIZE) & \
     M_TLS_KEYCTX_TX_WR_RXMK_SIZE)

#define S_TLS_KEYCTX_TX_WR_IVINSERT  55
#define M_TLS_KEYCTX_TX_WR_IVINSERT  0x1ffULL
#define V_TLS_KEYCTX_TX_WR_IVINSERT(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_IVINSERT)
#define G_TLS_KEYCTX_TX_WR_IVINSERT(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_IVINSERT) & M_TLS_KEYCTX_TX_WR_IVINSERT)

#define S_TLS_KEYCTX_TX_WR_AADSTRTOFST 47
#define M_TLS_KEYCTX_TX_WR_AADSTRTOFST 0xffULL
#define V_TLS_KEYCTX_TX_WR_AADSTRTOFST(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_AADSTRTOFST)
#define G_TLS_KEYCTX_TX_WR_AADSTRTOFST(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_AADSTRTOFST) & \
     M_TLS_KEYCTX_TX_WR_AADSTRTOFST)

#define S_TLS_KEYCTX_TX_WR_AADSTOPOFST 39
#define M_TLS_KEYCTX_TX_WR_AADSTOPOFST 0xffULL
#define V_TLS_KEYCTX_TX_WR_AADSTOPOFST(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_AADSTOPOFST)
#define G_TLS_KEYCTX_TX_WR_AADSTOPOFST(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_AADSTOPOFST) & \
     M_TLS_KEYCTX_TX_WR_AADSTOPOFST)

#define S_TLS_KEYCTX_TX_WR_CIPHERSRTOFST 30
#define M_TLS_KEYCTX_TX_WR_CIPHERSRTOFST 0x1ffULL
#define V_TLS_KEYCTX_TX_WR_CIPHERSRTOFST(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_CIPHERSRTOFST)
#define G_TLS_KEYCTX_TX_WR_CIPHERSRTOFST(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_CIPHERSRTOFST) & \
     M_TLS_KEYCTX_TX_WR_CIPHERSRTOFST)

#define S_TLS_KEYCTX_TX_WR_CIPHERSTOPOFST 23
#define M_TLS_KEYCTX_TX_WR_CIPHERSTOPOFST 0x7f
#define V_TLS_KEYCTX_TX_WR_CIPHERSTOPOFST(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_CIPHERSTOPOFST)
#define G_TLS_KEYCTX_TX_WR_CIPHERSTOPOFST(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_CIPHERSTOPOFST) & \
     M_TLS_KEYCTX_TX_WR_CIPHERSTOPOFST)

#define S_TLS_KEYCTX_TX_WR_AUTHSRTOFST 14
#define M_TLS_KEYCTX_TX_WR_AUTHSRTOFST 0x1ff
#define V_TLS_KEYCTX_TX_WR_AUTHSRTOFST(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_AUTHSRTOFST)
#define G_TLS_KEYCTX_TX_WR_AUTHSRTOFST(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_AUTHSRTOFST) & \
     M_TLS_KEYCTX_TX_WR_AUTHSRTOFST)

#define S_TLS_KEYCTX_TX_WR_AUTHSTOPOFST 7
#define M_TLS_KEYCTX_TX_WR_AUTHSTOPOFST 0x7f
#define V_TLS_KEYCTX_TX_WR_AUTHSTOPOFST(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_AUTHSTOPOFST)
#define G_TLS_KEYCTX_TX_WR_AUTHSTOPOFST(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_AUTHSTOPOFST) & \
     M_TLS_KEYCTX_TX_WR_AUTHSTOPOFST)

#define S_TLS_KEYCTX_TX_WR_AUTHINSRT 0
#define M_TLS_KEYCTX_TX_WR_AUTHINSRT 0x7f
#define V_TLS_KEYCTX_TX_WR_AUTHINSRT(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_AUTHINSRT)
#define G_TLS_KEYCTX_TX_WR_AUTHINSRT(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_AUTHINSRT) & \
     M_TLS_KEYCTX_TX_WR_AUTHINSRT)

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
