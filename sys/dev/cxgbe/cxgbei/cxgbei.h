/*-
 * Copyright (c) 2012 Chelsio Communications, Inc.
 * All rights reserved.
 *
 * Chelsio T5xx iSCSI driver
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
 */

#ifndef __CXGBEI_OFLD_H__
#define __CXGBEI_OFLD_H__

struct iscsi_socket {
	u_char  s_dcrc_len;
	void   *s_conn;	/* ic_conn pointer */
	struct toepcb *toep;

	/*
	 * XXXNP: locks on the same line.
	 * XXXNP: are the locks even needed?  Why not use so_snd/so_rcv mtx to
	 * guard the write and rcv queues?
	 */
	struct mbufq iscsi_rcvq;	/* rx - ULP mbufs */
	struct mtx iscsi_rcvq_lock;

	struct mbufq ulp2_writeq;	/* tx - ULP mbufs */
	struct mtx ulp2_writeq_lock;

	struct mbufq ulp2_wrq;		/* tx wr- ULP mbufs */
	struct mtx ulp2_wrq_lock;

	struct mbuf *mbuf_ulp_lhdr;
	struct mbuf *mbuf_ulp_ldata;
};

struct cxgbei_sgl {
        int     sg_flag;
        void    *sg_addr;
        void    *sg_dma_addr;
        size_t  sg_offset;
        size_t  sg_length;
};

#define cxgbei_scsi_for_each_sg(_sgl, _sgel, _n, _i)      \
        for (_i = 0, _sgel = (cxgbei_sgl*) (_sgl); _i < _n; _i++, \
                        _sgel++)
#define sg_dma_addr(_sgel)      _sgel->sg_dma_addr
#define sg_virt(_sgel)          _sgel->sg_addr
#define sg_len(_sgel)           _sgel->sg_length
#define sg_off(_sgel)           _sgel->sg_offset
#define sg_next(_sgel)          _sgel + 1

#define SBUF_ULP_FLAG_HDR_RCVD          0x1
#define SBUF_ULP_FLAG_DATA_RCVD         0x2
#define SBUF_ULP_FLAG_STATUS_RCVD       0x4
#define SBUF_ULP_FLAG_HCRC_ERROR        0x10
#define SBUF_ULP_FLAG_DCRC_ERROR        0x20
#define SBUF_ULP_FLAG_PAD_ERROR         0x40
#define SBUF_ULP_FLAG_DATA_DDPED        0x80

/*
 * Similar to tcp_skb_cb but with ULP elements added to support DDP, iSCSI,
 * etc.
 */
struct ulp_mbuf_cb {
	uint8_t ulp_mode;	/* ULP mode/submode of sk_buff */
	uint8_t flags;		/* TCP-like flags */
	uint32_t ddigest;	/* ULP rx_data_ddp selected field*/
	uint32_t pdulen;	/* ULP rx_data_ddp selected field*/
	void *pdu;		/* pdu pointer */
};

/* private data for each scsi task */
struct cxgbei_task_data {
	struct cxgbei_sgl sgl[256];
	u_int	nsge;
	u_int	sc_ddp_tag;
};

struct cxgbei_ulp2_tag_format {
	u_char sw_bits;
	u_char rsvd_bits;
	u_char rsvd_shift;
	u_char filler[1];
	uint32_t rsvd_mask;
};

struct cxgbei_data {
	u_int max_txsz;
	u_int max_rxsz;
	u_int llimit;
	u_int ulimit;
	u_int nppods;
	u_int idx_last;
	u_char idx_bits;
	uint32_t idx_mask;
	uint32_t rsvd_tag_mask;

	struct mtx map_lock;
	bus_dma_tag_t ulp_ddp_tag;
	unsigned char *colors;
	struct cxgbei_ulp2_gather_list **gl_map;

	struct cxgbei_ulp2_tag_format tag_format;
};

struct icl_conn;
struct icl_pdu;

struct ulp_mbuf_cb *get_ulp_mbuf_cb(struct mbuf *);
int cxgbei_conn_handoff(struct icl_conn *);
int cxgbei_conn_close(struct icl_conn *);
void cxgbei_conn_task_reserve_itt(void *, void **, void *, unsigned int *);
void cxgbei_conn_transfer_reserve_ttt(void *, void **, void *, unsigned int *);
void cxgbei_cleanup_task(void *, void *);
int cxgbei_conn_xmit_pdu(struct icl_conn *, struct icl_pdu *);

struct cxgbei_ulp2_pagepod_hdr;
int t4_ddp_set_map(struct cxgbei_data *, void *,
    struct cxgbei_ulp2_pagepod_hdr *, u_int, u_int,
    struct cxgbei_ulp2_gather_list *, int);
void t4_ddp_clear_map(struct cxgbei_data *, struct cxgbei_ulp2_gather_list *,
    u_int, u_int, u_int, struct iscsi_socket *);
#endif
