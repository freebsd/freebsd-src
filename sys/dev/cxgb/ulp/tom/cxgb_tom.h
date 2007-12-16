
/**************************************************************************

Copyright (c) 2007, Chelsio Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

 2. Neither the name of the Chelsio Corporation nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.


$FreeBSD$

***************************************************************************/
#ifndef CXGB_TOM_H_
#define CXGB_TOM_H_
#include <sys/protosw.h>

#define LISTEN_INFO_HASH_SIZE 32 

struct listen_info {
	struct listen_info *next;  /* Link to next entry */
	struct socket *so;         /* The listening socket */
	unsigned int stid;         /* The server TID */
};


/*
 * TOM tunable parameters.  They can be manipulated through sysctl(2) or /proc.
 */
struct tom_tunables {
        int max_host_sndbuf;    // max host RAM consumed by a sndbuf
        int tx_hold_thres;      // push/pull threshold for non-full TX sk_buffs
        int max_wrs;            // max # of outstanding WRs per connection
        int rx_credit_thres;    // min # of RX credits needed for RX_DATA_ACK
        int cong_alg;           // Congestion control algorithm
        int mss;                // max TX_DATA WR payload size
        int delack;             // delayed ACK control
        int max_conn;           // maximum number of offloaded connections
        int soft_backlog_limit; // whether the listen backlog limit is soft
        int ddp;                // whether to put new connections in DDP mode
        int ddp_thres;          // min recvmsg size before activating DDP
        int ddp_copy_limit;     // capacity of kernel DDP buffer
        int ddp_push_wait;      // whether blocking DDP waits for PSH flag
        int ddp_rcvcoalesce;    // whether receive coalescing is enabled
        int zcopy_sosend_enabled; // < is never zcopied
        int zcopy_sosend_partial_thres; // < is never zcopied
        int zcopy_sosend_partial_copy; // bytes copied in partial zcopy
        int zcopy_sosend_thres;// >= are mostly zcopied
        int zcopy_sosend_copy; // bytes coped in zcopied
        int zcopy_sosend_ret_pending_dma;// pot. return while pending DMA
        int activated;          // TOE engine activation state
};

struct tom_data {
        TAILQ_ENTRY(tom_data) entry;
			      
        struct t3cdev *cdev;
        struct pci_dev *pdev;
        struct toedev tdev;

        struct cxgb_client *client;
        struct tom_tunables conf;
        struct tom_sysctl_table *sysctl;

        /*
         * The next three locks listen_lock, deferq.lock, and tid_release_lock
         * are used rarely so we let them potentially share a cacheline.
         */

        struct listen_info *listen_hash_tab[LISTEN_INFO_HASH_SIZE];
        struct mtx listen_lock;

        struct mbuf_head deferq;
        struct task deferq_task;

        struct socket **tid_release_list;
        struct mtx tid_release_lock;
        struct task tid_release_task;

        volatile int tx_dma_pending;
	
        unsigned int ddp_llimit;
        unsigned int ddp_ulimit;

        unsigned int rx_page_size;

        u8 *ppod_map;
        unsigned int nppods;
        struct mtx ppod_map_lock;
	
        struct adap_ports *ports;
	struct taskqueue *tq;
};


struct listen_ctx {
	struct socket *lso;
	struct tom_data *tom_data;
	int ulp_mode;
	LIST_HEAD(, toepcb) synq_head;
	
};

#define TOM_DATA(dev) (*(struct tom_data **)&(dev)->tod_l4opt)
#define T3C_DEV(sk) ((TOM_DATA(TOE_DEV(sk)))->cdev)
#define TOEP_T3C_DEV(toep) (TOM_DATA(toep->tp_toedev)->cdev)
#define TOM_TUNABLE(dev, param) (TOM_DATA(dev)->conf.param)

#define TP_DATASENT         	(1 << 0)
#define TP_TX_WAIT_IDLE      	(1 << 1)
#define TP_FIN_SENT          	(1 << 2)
#define TP_ABORT_RPL_PENDING 	(1 << 3)
#define TP_ABORT_SHUTDOWN    	(1 << 4)
#define TP_ABORT_RPL_RCVD    	(1 << 5)
#define TP_ABORT_REQ_RCVD    	(1 << 6)
#define TP_CLOSE_CON_REQUESTED	(1 << 7)
#define TP_SYN_RCVD		(1 << 8)
#define TP_ESTABLISHED		(1 << 9)

void t3_init_tunables(struct tom_data *t);

static __inline struct mbuf *
m_gethdr_nofail(int len)
{
	struct mbuf *m;
	
	m = m_gethdr(M_NOWAIT, MT_DATA);
	if (m == NULL) {
		panic("implement lowmem cache\n");
	}
	
	KASSERT(len < MHLEN, ("requested header size too large for mbuf"));	
	m->m_pkthdr.len = m->m_len = len;
	return (m);
}


#endif
