/*-
 * Copyright (c) 2012, 2015 Chelsio Communications, Inc.
 * All rights reserved.
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

#ifndef __CXGBEI_OFLD_H__
#define __CXGBEI_OFLD_H__

#include <dev/iscsi/icl.h>

#define CXGBEI_CONN_SIGNATURE 0x56788765

struct cxgbei_cmp {
	LIST_ENTRY(cxgbei_cmp) link;

	uint32_t tt;		/* Transfer tag. */

	uint32_t next_buffer_offset;
	uint32_t last_datasn;
};
LIST_HEAD(cxgbei_cmp_head, cxgbei_cmp);

struct icl_cxgbei_conn {
	struct icl_conn ic;

	/* cxgbei specific stuff goes here. */
	uint32_t icc_signature;
	int ulp_submode;
	struct adapter *sc;
	struct toepcb *toep;

	/* Receive related. */
	bool rx_active;				/* protected by so_rcv lock */
	bool rx_exiting;			/* protected by so_rcv lock */
	STAILQ_HEAD(, icl_pdu) rcvd_pdus;	/* protected by so_rcv lock */
	struct thread *rx_thread;

	struct cxgbei_cmp_head *cmp_table;	/* protected by cmp_lock */
	struct mtx cmp_lock;
	unsigned long cmp_hash_mask;

	/* Transmit related. */
	bool tx_active;				/* protected by ic lock */
	STAILQ_HEAD(, icl_pdu) sent_pdus;	/* protected by ic lock */
	struct thread *tx_thread;
};

static inline struct icl_cxgbei_conn *
ic_to_icc(struct icl_conn *ic)
{

	return (__containerof(ic, struct icl_cxgbei_conn, ic));
}

/* PDU flags and signature. */
enum {
	ICPF_RX_HDR	= 1 << 0, /* PDU header received. */
	ICPF_RX_FLBUF	= 1 << 1, /* PDU payload received in a freelist. */
	ICPF_RX_DDP	= 1 << 2, /* PDU payload DDP'd. */
	ICPF_RX_STATUS	= 1 << 3, /* Rx status received. */

	CXGBEI_PDU_SIGNATURE = 0x12344321
};

struct icl_cxgbei_pdu {
	struct icl_pdu ip;

	/* cxgbei specific stuff goes here. */
	uint32_t icp_signature;
	uint32_t icp_seq;	/* For debug only */
	u_int icp_flags;

	u_int ref_cnt;
	icl_pdu_cb cb;
	int error;
};

static inline struct icl_cxgbei_pdu *
ip_to_icp(struct icl_pdu *ip)
{

	return (__containerof(ip, struct icl_cxgbei_pdu, ip));
}

struct cxgbei_data {
	u_int max_tx_data_len;
	u_int max_rx_data_len;

	u_int ddp_threshold;
	struct ppod_region pr;

	struct sysctl_ctx_list ctx;	/* from uld_activate to deactivate */
};

#define CXGBEI_MAX_ISO_PAYLOAD	65535

/* cxgbei.c */
u_int cxgbei_select_worker_thread(struct icl_cxgbei_conn *);
void cwt_queue_for_tx(struct icl_cxgbei_conn *);
void parse_pdus(struct icl_cxgbei_conn *, struct sockbuf *);

/* icl_cxgbei.c */
void cwt_tx_main(void *);
int icl_cxgbei_mod_load(void);
int icl_cxgbei_mod_unload(void);
struct icl_pdu *icl_cxgbei_new_pdu(int);
void icl_cxgbei_new_pdu_set_conn(struct icl_pdu *, struct icl_conn *);
void icl_cxgbei_conn_pdu_free(struct icl_conn *, struct icl_pdu *);
struct cxgbei_cmp *cxgbei_find_cmp(struct icl_cxgbei_conn *, uint32_t);

#endif
