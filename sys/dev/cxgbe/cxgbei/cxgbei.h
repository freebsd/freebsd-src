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
#include "mbufq.h"

typedef struct iscsi_socket {
	/* iscsi private */
	unsigned char   s_flag;
	unsigned char   s_cpuno;        /* bind to cpuno */
	unsigned char   s_mode;         /* offload mode */
	unsigned char   s_txhold;

	unsigned char   s_ddp_pgidx;    /* ddp page selection */
	unsigned char   s_hcrc_len;
	unsigned char   s_dcrc_len;
	unsigned char   filler[1];

	unsigned int    s_tid;          /* for debug only */
	unsigned int    s_tmax;
	unsigned int    s_rmax;
	unsigned int    s_mss;
	void            *s_odev;        /* offload device, if any */
	void            *s_appdata;     /* upperlayer data pointer */
	void            *s_private;     /* underlying socket related info. */
	void            *s_conn;	/* ic_conn pointer */
	struct socket	*sock;
	struct mbuf_head iscsi_rcv_mbufq;/* rx - ULP mbufs */
	struct mbuf_head ulp2_writeq;	 /* tx - ULP mbufs */
	struct mbuf_head ulp2_wrq;	 /* tx wr- ULP mbufs */

	struct mbuf *mbuf_ulp_lhdr;
	struct mbuf *mbuf_ulp_ldata;
}iscsi_socket;

#define ISCSI_SG_SBUF_DMABLE            0x1
#define ISCSI_SG_SBUF_DMA_ONLY          0x2     /*private*/
#define ISCSI_SG_BUF_ALLOC              0x10
#define ISCSI_SG_PAGE_ALLOC             0x20
#define ISCSI_SG_SBUF_MAP_NEEDED        0x40
#define ISCSI_SG_SBUF_MAPPED            0x80

#define ISCSI_SG_SBUF_LISTHEAD          0x100
#define ISCSI_SG_SBUF_LISTTAIL          0x200
#define ISCSI_SG_SBUF_XFER_DONE         0x400

typedef struct cxgbei_sgl {
        int     sg_flag;
        void    *sg_addr;
        void    *sg_dma_addr;
        size_t  sg_offset;
        size_t  sg_length;
} cxgbei_sgl;

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
#define SBUF_ULP_FLAG_COALESCE_OFF      0x8
#define SBUF_ULP_FLAG_HCRC_ERROR        0x10
#define SBUF_ULP_FLAG_DCRC_ERROR        0x20
#define SBUF_ULP_FLAG_PAD_ERROR         0x40
#define SBUF_ULP_FLAG_DATA_DDPED        0x80

/* Flags for return value of CPL message handlers */
enum {
	CPL_RET_BUF_DONE = 1,	/* buffer processing done buffer may be freed */
	CPL_RET_BAD_MSG = 2,	/* bad CPL message (e.g., unknown opcode) */
	CPL_RET_UNKNOWN_TID = 4	/* unexpected unknown TID */
};


/*
 * Similar to tcp_skb_cb but with ULP elements added to support DDP, iSCSI,
 * etc.
 */
struct ulp_mbuf_cb {
	uint8_t ulp_mode;                    /* ULP mode/submode of sk_buff */
	uint8_t flags;                       /* TCP-like flags */
	uint32_t seq;                        /* TCP sequence number */
	union { /* ULP-specific fields */
		struct {
			uint32_t ddigest;    /* ULP rx_data_ddp selected field*/
			uint32_t pdulen;     /* ULP rx_data_ddp selected field*/
		} iscsi;
		struct {
			uint32_t offset;     /* ULP DDP offset notification */
			uint8_t flags;       /* ULP DDP flags ... */
		} ddp;
	} ulp;
	uint8_t ulp_data[16];                /* scratch area for ULP */
	void *pdu;                      /* pdu pointer */
};

/* private data for eack scsi task */
typedef struct cxgbei_task_data {
	cxgbei_sgl sgl[256];
	unsigned int	nsge;
	unsigned int	sc_ddp_tag;
}cxgbei_task_data;

static unsigned char t4tom_cpl_handler_register_flag;
enum {
	TOM_CPL_ISCSI_HDR_REGISTERED_BIT,
	TOM_CPL_SET_TCB_RPL_REGISTERED_BIT,
	TOM_CPL_RX_DATA_DDP_REGISTERED_BIT
};

#define ODEV_FLAG_ULP_CRC_ENABLED       0x1
#define ODEV_FLAG_ULP_DDP_ENABLED       0x2
#define ODEV_FLAG_ULP_TX_ALLOC_DIGEST   0x4
#define ODEV_FLAG_ULP_RX_PAD_INCLUDED   0x8

#define ODEV_FLAG_ULP_ENABLED   \
        (ODEV_FLAG_ULP_CRC_ENABLED | ODEV_FLAG_ULP_DDP_ENABLED)

struct ulp_mbuf_cb * get_ulp_mbuf_cb(struct mbuf *);
int cxgbei_conn_set_ulp_mode(struct socket *, void *);
int cxgbei_conn_close(struct socket *);
void cxgbei_conn_task_reserve_itt(void *, void **, void *, unsigned int *);
void cxgbei_conn_transfer_reserve_ttt(void *, void **, void *, unsigned int *);
void cxgbei_cleanup_task(void *, void *);
int cxgbei_conn_xmit_pdu(void *, void *);
#endif
