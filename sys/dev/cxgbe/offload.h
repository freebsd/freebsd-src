/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2010 Chelsio Communications, Inc.
 * All rights reserved.
 * Written by: Navdeep Parhar <np@FreeBSD.org>
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

#ifndef __T4_OFFLOAD_H__
#define __T4_OFFLOAD_H__
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/condvar.h>
#include <sys/bitstring.h>

#define INIT_ULPTX_WRH(w, wrlen, atomic, tid) do { \
	(w)->wr_hi = htonl(V_FW_WR_OP(FW_ULPTX_WR) | V_FW_WR_ATOMIC(atomic)); \
	(w)->wr_mid = htonl(V_FW_WR_LEN16(DIV_ROUND_UP(wrlen, 16)) | \
			       V_FW_WR_FLOWID(tid)); \
	(w)->wr_lo = cpu_to_be64(0); \
} while (0)

#define INIT_ULPTX_WR(w, wrlen, atomic, tid) \
    INIT_ULPTX_WRH(&((w)->wr), wrlen, atomic, tid)

#define INIT_TP_WR(w, tid) do { \
	(w)->wr.wr_hi = htonl(V_FW_WR_OP(FW_TP_WR) | \
                              V_FW_WR_IMMDLEN(sizeof(*w) - sizeof(w->wr))); \
	(w)->wr.wr_mid = htonl(V_FW_WR_LEN16(DIV_ROUND_UP(sizeof(*w), 16)) | \
                               V_FW_WR_FLOWID(tid)); \
	(w)->wr.wr_lo = cpu_to_be64(0); \
} while (0)

#define INIT_TP_WR_MIT_CPL(w, cpl, tid) do { \
	INIT_TP_WR(w, tid); \
	OPCODE_TID(w) = htonl(MK_OPCODE_TID(cpl, tid)); \
} while (0)

/*
 * Max # of ATIDs.  The absolute HW max is larger than this but we reserve a few
 * of the upper bits for use as a cookie to demux the reply.
 */
#define MAX_ATIDS (M_TID_TID + 1)

union aopen_entry {
	void *data;
	union aopen_entry *next;
};

/* cxgbe_rate_tag flags */
enum {
	EO_FLOWC_PENDING	= (1 << 0),	/* flowc needs to be sent */
	EO_FLOWC_RPL_PENDING	= (1 << 1),	/* flowc credits due back */
	EO_SND_TAG_REF		= (1 << 2),	/* kernel has a ref on us */
	EO_FLUSH_RPL_PENDING	= (1 << 3),	/* credit flush rpl due back */
};

struct cxgbe_rate_tag {
	struct m_snd_tag com;
	struct adapter *adapter;
	u_int flags;
	struct mtx lock;
	int port_id;
	int etid;
	struct mbufq pending_tx, pending_fwack;
	int plen;
	struct sge_ofld_txq *eo_txq;
	uint32_t ctrl0;
	uint16_t iqid;
	int8_t schedcl;
	uint64_t max_rate;      /* in bytes/s */
	uint8_t tx_total;	/* total tx WR credits (in 16B units) */
	uint8_t tx_credits;	/* tx WR credits (in 16B units) available */
	uint8_t tx_nocompl;	/* tx WR credits since last compl request */
	uint8_t ncompl;		/* # of completions outstanding. */
};

static inline struct cxgbe_rate_tag *
mst_to_crt(struct m_snd_tag *t)
{
	return (__containerof(t, struct cxgbe_rate_tag, com));
}

union etid_entry {
	struct cxgbe_rate_tag *cst;
	union etid_entry *next;
};

/*
 * Holds the size, base address, start, end, etc. of various types of TIDs.  The
 * tables themselves are allocated dynamically.
 */
struct tid_info {
	u_int nstids;
	u_int stid_base;

	u_int natids;

	u_int nftids;
	u_int ftid_base;
	u_int ftid_end;

	u_int nhpftids;
	u_int hpftid_base;
	u_int hpftid_end;

	u_int ntids;
	u_int tid_base;

	u_int netids;
	u_int etid_base;
	u_int etid_end;

	struct mtx stid_lock __aligned(CACHE_LINE_SIZE);
	struct listen_ctx **stid_tab;
	bitstr_t *stid_bitmap;
	u_int stids_in_use;
	bool stid_tab_stopped;

	struct mtx atid_lock __aligned(CACHE_LINE_SIZE);
	union aopen_entry *atid_tab;
	union aopen_entry *afree;
	u_int atids_in_use;
	bool atid_alloc_stopped;

	/* High priority filters and normal filters share the lock and cv. */
	struct mtx ftid_lock __aligned(CACHE_LINE_SIZE);
	struct cv ftid_cv;
	struct filter_entry *ftid_tab;
	struct filter_entry *hpftid_tab;
	u_int ftids_in_use;
	u_int hpftids_in_use;

	/*
	 * hashfilter and TOE are mutually exclusive and both use ntids and
	 * tids_in_use.  The lock and cv are used only by hashfilter.
	 */
	struct mtx hftid_lock __aligned(CACHE_LINE_SIZE);
	struct cv hftid_cv;
	void **tid_tab;
	u_int tids_in_use;

	void *hftid_hash_4t;	/* LIST_HEAD(, filter_entry) *hftid_hash_4t; */
	u_long hftid_4t_mask;
	void *hftid_hash_tid;	/* LIST_HEAD(, filter_entry) *hftid_hash_tid; */
	u_long hftid_tid_mask;

	struct mtx etid_lock __aligned(CACHE_LINE_SIZE);
	union etid_entry *etid_tab;
	union etid_entry *efree;
	u_int etids_in_use;
};

struct t4_range {
	u_int start;
	u_int size;
};

struct t4_virt_res {                      /* virtualized HW resources */
	struct t4_range ddp;
	struct t4_range iscsi;
	struct t4_range stag;
	struct t4_range rq;
	struct t4_range pbl;
	struct t4_range qp;
	struct t4_range cq;
	struct t4_range srq;
	struct t4_range ocq;
	struct t4_range l2t;
	struct t4_range key;
};

enum {
	ULD_TOM = 0,
	ULD_IWARP,
	ULD_ISCSI,
	ULD_MAX = ULD_ISCSI
};

struct adapter;
struct port_info;
struct uld_info {
	int (*uld_activate)(struct adapter *);
	int (*uld_deactivate)(struct adapter *);
	int (*uld_stop)(struct adapter *);
	int (*uld_restart)(struct adapter *);
};

struct tom_tunables {
	int cong_algorithm;
	int sndbuf;
	int ddp;
	int rx_coalesce;
	int tls;
	int tx_align;
	int tx_zcopy;
	int cop_managed_offloading;
	int autorcvbuf_inc;
	int update_hc_on_pmtu_change;
	int iso;
};

/* iWARP driver tunables */
struct iw_tunables {
	int wc_en;
};

struct tls_tunables {
	int inline_keys;
	int combo_wrs;
};

#ifdef TCP_OFFLOAD
int t4_register_uld(struct uld_info *, int);
int t4_unregister_uld(struct uld_info *, int);
int t4_activate_uld(struct adapter *, int);
int t4_deactivate_uld(struct adapter *, int);
int uld_active(struct adapter *, int);
#endif
#endif
