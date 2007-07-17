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

***************************************************************************/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus_dma.h>
#include <sys/rman.h>
#include <sys/queue.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>


#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/systm.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#ifdef CONFIG_DEFINED
#include <cxgb_include.h>
#else
#include <dev/cxgb/cxgb_include.h>
#endif

uint32_t collapse_free = 0;
uint32_t mb_free_vec_free = 0;
int      txq_fills = 0;
int      collapse_mbufs = 0;
static int recycle_enable = 1;
static int bogus_imm = 0;

/*
 * XXX GC
 */
#define NET_XMIT_CN 2
#define NET_XMIT_SUCCESS 0 

#define USE_GTS 0

#define SGE_RX_SM_BUF_SIZE	1536
#define SGE_RX_DROP_THRES	16
#define SGE_RX_COPY_THRES	128

/*
 * Period of the Tx buffer reclaim timer.  This timer does not need to run
 * frequently as Tx buffers are usually reclaimed by new Tx packets.
 */
#define TX_RECLAIM_PERIOD       (hz >> 1)

/* 
 * work request size in bytes
 */
#define WR_LEN (WR_FLITS * 8)

/* 
 * Values for sge_txq.flags
 */
enum {
	TXQ_RUNNING	= 1 << 0,  /* fetch engine is running */
	TXQ_LAST_PKT_DB = 1 << 1,  /* last packet rang the doorbell */
};

struct tx_desc {
	uint64_t	flit[TX_DESC_FLITS];
} __packed;

struct rx_desc {
	uint32_t	addr_lo;
	uint32_t	len_gen;
	uint32_t	gen2;
	uint32_t	addr_hi;
} __packed;;

struct rsp_desc {               /* response queue descriptor */
	struct rss_header	rss_hdr;
	uint32_t		flags;
	uint32_t		len_cq;
	uint8_t			imm_data[47];
	uint8_t			intr_gen;
} __packed;

#define RX_SW_DESC_MAP_CREATED	(1 << 0)
#define TX_SW_DESC_MAP_CREATED	(1 << 1)
#define RX_SW_DESC_INUSE        (1 << 3)
#define TX_SW_DESC_MAPPED       (1 << 4)

#define RSPQ_NSOP_NEOP           G_RSPD_SOP_EOP(0)
#define RSPQ_EOP                 G_RSPD_SOP_EOP(F_RSPD_EOP)
#define RSPQ_SOP                 G_RSPD_SOP_EOP(F_RSPD_SOP)
#define RSPQ_SOP_EOP             G_RSPD_SOP_EOP(F_RSPD_SOP|F_RSPD_EOP)

struct tx_sw_desc {                /* SW state per Tx descriptor */
	struct mbuf	*m;        
	bus_dmamap_t	map;
	int		flags;
};

struct rx_sw_desc {                /* SW state per Rx descriptor */
	void	        *cl;
	bus_dmamap_t	map;
	int		flags;
};

struct txq_state {
	unsigned int compl;
	unsigned int gen;
	unsigned int pidx;
};

struct refill_fl_cb_arg {
	int               error;
	bus_dma_segment_t seg;
	int               nseg;
};

/*
 * Maps a number of flits to the number of Tx descriptors that can hold them.
 * The formula is
 *
 * desc = 1 + (flits - 2) / (WR_FLITS - 1).
 *
 * HW allows up to 4 descriptors to be combined into a WR.
 */
static uint8_t flit_desc_map[] = {
	0,
#if SGE_NUM_GENBITS == 1
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4
#elif SGE_NUM_GENBITS == 2
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
#else
# error "SGE_NUM_GENBITS must be 1 or 2"
#endif
};


static int lro_default = 0;
int cxgb_debug = 0;

static void t3_free_qset(adapter_t *sc, struct sge_qset *q);
static void sge_timer_cb(void *arg);
static void sge_timer_reclaim(void *arg, int ncount);
static void sge_txq_reclaim_handler(void *arg, int ncount);
static int free_tx_desc(struct sge_txq *q, int n, struct mbuf **m_vec);

/**
 *	reclaim_completed_tx - reclaims completed Tx descriptors
 *	@adapter: the adapter
 *	@q: the Tx queue to reclaim completed descriptors from
 *
 *	Reclaims Tx descriptors that the SGE has indicated it has processed,
 *	and frees the associated buffers if possible.  Called with the Tx
 *	queue's lock held.
 */
static __inline int
reclaim_completed_tx(struct sge_txq *q, int nbufs, struct mbuf **mvec)
{
	int reclaimed, reclaim = desc_reclaimable(q);
	int n = 0;

	mtx_assert(&q->lock, MA_OWNED);
	if (reclaim > 0) {
		n = free_tx_desc(q, min(reclaim, nbufs), mvec);
		reclaimed = min(reclaim, nbufs);
		q->cleaned += reclaimed;
		q->in_use -= reclaimed;
	} 
	return (n);
}

/**
 *	should_restart_tx - are there enough resources to restart a Tx queue?
 *	@q: the Tx queue
 *
 *	Checks if there are enough descriptors to restart a suspended Tx queue.
 */
static __inline int
should_restart_tx(const struct sge_txq *q)
{
	unsigned int r = q->processed - q->cleaned;

	return q->in_use - r < (q->size >> 1);
}

/**
 *	t3_sge_init - initialize SGE
 *	@adap: the adapter
 *	@p: the SGE parameters
 *
 *	Performs SGE initialization needed every time after a chip reset.
 *	We do not initialize any of the queue sets here, instead the driver
 *	top-level must request those individually.  We also do not enable DMA
 *	here, that should be done after the queues have been set up.
 */
void
t3_sge_init(adapter_t *adap, struct sge_params *p)
{
	u_int ctrl, ups;

	ups = 0; /* = ffs(pci_resource_len(adap->pdev, 2) >> 12); */

	ctrl = F_DROPPKT | V_PKTSHIFT(2) | F_FLMODE | F_AVOIDCQOVFL |
	       F_CQCRDTCTRL |
	       V_HOSTPAGESIZE(PAGE_SHIFT - 11) | F_BIGENDIANINGRESS |
	       V_USERSPACESIZE(ups ? ups - 1 : 0) | F_ISCSICOALESCING;
#if SGE_NUM_GENBITS == 1
	ctrl |= F_EGRGENCTRL;
#endif
	if (adap->params.rev > 0) {
		if (!(adap->flags & (USING_MSIX | USING_MSI)))
			ctrl |= F_ONEINTMULTQ | F_OPTONEINTMULTQ;
		ctrl |= F_CQCRDTCTRL | F_AVOIDCQOVFL;
	}
	t3_write_reg(adap, A_SG_CONTROL, ctrl);
	t3_write_reg(adap, A_SG_EGR_RCQ_DRB_THRSH, V_HIRCQDRBTHRSH(512) |
		     V_LORCQDRBTHRSH(512));
	t3_write_reg(adap, A_SG_TIMER_TICK, core_ticks_per_usec(adap) / 10);
	t3_write_reg(adap, A_SG_CMDQ_CREDIT_TH, V_THRESHOLD(32) |
		     V_TIMEOUT(200 * core_ticks_per_usec(adap)));
	t3_write_reg(adap, A_SG_HI_DRB_HI_THRSH, 1000);
	t3_write_reg(adap, A_SG_HI_DRB_LO_THRSH, 256);
	t3_write_reg(adap, A_SG_LO_DRB_HI_THRSH, 1000);
	t3_write_reg(adap, A_SG_LO_DRB_LO_THRSH, 256);
	t3_write_reg(adap, A_SG_OCO_BASE, V_BASE1(0xfff));
	t3_write_reg(adap, A_SG_DRB_PRI_THRESH, 63 * 1024);
}


/**
 *	sgl_len - calculates the size of an SGL of the given capacity
 *	@n: the number of SGL entries
 *
 *	Calculates the number of flits needed for a scatter/gather list that
 *	can hold the given number of entries.
 */
static __inline unsigned int
sgl_len(unsigned int n)
{
	return ((3 * n) / 2 + (n & 1));
}

/**
 *	get_imm_packet - return the next ingress packet buffer from a response
 *	@resp: the response descriptor containing the packet data
 *
 *	Return a packet containing the immediate data of the given response.
 */
static int
get_imm_packet(adapter_t *sc, const struct rsp_desc *resp, struct mbuf *m, void *cl, uint32_t flags)
{
	int len, error;
	uint8_t sopeop = G_RSPD_SOP_EOP(flags);
	
	/*
	 * would be a firmware bug
	 */
	len = G_RSPD_LEN(ntohl(resp->len_cq));	
	if (sopeop == RSPQ_NSOP_NEOP || sopeop == RSPQ_SOP) {
		if (cxgb_debug)
			device_printf(sc->dev, "unexpected value sopeop=%d flags=0x%x len=%din get_imm_packet\n", sopeop, flags, len);
		bogus_imm++;
		return (EINVAL);
	}
	error = 0;
	switch (sopeop) {
	case RSPQ_SOP_EOP:
		m->m_len = m->m_pkthdr.len = len; 
		memcpy(mtod(m, uint8_t *), resp->imm_data, len); 
		break;
	case RSPQ_EOP:
		memcpy(cl, resp->imm_data, len); 
		m_iovappend(m, cl, MSIZE, len, 0); 
		break;
	default:
		bogus_imm++;
		error = EINVAL;
	}

	return (error);
}


static __inline u_int
flits_to_desc(u_int n)
{
	return (flit_desc_map[n]);
}

void
t3_sge_err_intr_handler(adapter_t *adapter)
{
	unsigned int v, status;

	
	status = t3_read_reg(adapter, A_SG_INT_CAUSE);
	
	if (status & F_RSPQCREDITOVERFOW)
		CH_ALERT(adapter, "SGE response queue credit overflow\n");

	if (status & F_RSPQDISABLED) {
		v = t3_read_reg(adapter, A_SG_RSPQ_FL_STATUS);

		CH_ALERT(adapter,
			 "packet delivered to disabled response queue (0x%x)\n",
			 (v >> S_RSPQ0DISABLED) & 0xff);
	}

	t3_write_reg(adapter, A_SG_INT_CAUSE, status);
	if (status & (F_RSPQCREDITOVERFOW | F_RSPQDISABLED))
		t3_fatal_err(adapter);
}

void
t3_sge_prep(adapter_t *adap, struct sge_params *p)
{
	int i;

	/* XXX Does ETHER_ALIGN need to be accounted for here? */
	p->max_pkt_size = MJUM16BYTES - sizeof(struct cpl_rx_data);

	for (i = 0; i < SGE_QSETS; ++i) {
		struct qset_params *q = p->qset + i;

		q->polling = adap->params.rev > 0;

		if (adap->params.nports > 2)
			q->coalesce_nsecs = 50000;
		else
			q->coalesce_nsecs = 5000;

		q->rspq_size = RSPQ_Q_SIZE;
		q->fl_size = FL_Q_SIZE;
		q->jumbo_size = JUMBO_Q_SIZE;
		q->txq_size[TXQ_ETH] = TX_ETH_Q_SIZE;
		q->txq_size[TXQ_OFLD] = 1024;
		q->txq_size[TXQ_CTRL] = 256;
		q->cong_thres = 0;
	}
}

int
t3_sge_alloc(adapter_t *sc)
{

	/* The parent tag. */
	if (bus_dma_tag_create( NULL,			/* parent */
				1, 0,			/* algnmnt, boundary */
				BUS_SPACE_MAXADDR,	/* lowaddr */
				BUS_SPACE_MAXADDR,	/* highaddr */
				NULL, NULL,		/* filter, filterarg */
				BUS_SPACE_MAXSIZE_32BIT,/* maxsize */
				BUS_SPACE_UNRESTRICTED, /* nsegments */
				BUS_SPACE_MAXSIZE_32BIT,/* maxsegsize */
				0,			/* flags */
				NULL, NULL,		/* lock, lockarg */
				&sc->parent_dmat)) {
		device_printf(sc->dev, "Cannot allocate parent DMA tag\n");
		return (ENOMEM);
	}

	/*
	 * DMA tag for normal sized RX frames
	 */
	if (bus_dma_tag_create(sc->parent_dmat, MCLBYTES, 0, BUS_SPACE_MAXADDR,
		BUS_SPACE_MAXADDR, NULL, NULL, MCLBYTES, 1,
		MCLBYTES, BUS_DMA_ALLOCNOW, NULL, NULL, &sc->rx_dmat)) {
		device_printf(sc->dev, "Cannot allocate RX DMA tag\n");
		return (ENOMEM);
	}

	/* 
	 * DMA tag for jumbo sized RX frames.
	 */
	if (bus_dma_tag_create(sc->parent_dmat, MJUMPAGESIZE, 0, BUS_SPACE_MAXADDR,
		BUS_SPACE_MAXADDR, NULL, NULL, MJUMPAGESIZE, 1, MJUMPAGESIZE,
		BUS_DMA_ALLOCNOW, NULL, NULL, &sc->rx_jumbo_dmat)) {
		device_printf(sc->dev, "Cannot allocate RX jumbo DMA tag\n");
		return (ENOMEM);
	}

	/* 
	 * DMA tag for TX frames.
	 */
	if (bus_dma_tag_create(sc->parent_dmat, 1, 0, BUS_SPACE_MAXADDR,
		BUS_SPACE_MAXADDR, NULL, NULL, TX_MAX_SIZE, TX_MAX_SEGS,
		TX_MAX_SIZE, BUS_DMA_ALLOCNOW,
		NULL, NULL, &sc->tx_dmat)) {
		device_printf(sc->dev, "Cannot allocate TX DMA tag\n");
		return (ENOMEM);
	}

	return (0);
}

int
t3_sge_free(struct adapter * sc)
{

	if (sc->tx_dmat != NULL)
		bus_dma_tag_destroy(sc->tx_dmat);

	if (sc->rx_jumbo_dmat != NULL)
		bus_dma_tag_destroy(sc->rx_jumbo_dmat);

	if (sc->rx_dmat != NULL)
		bus_dma_tag_destroy(sc->rx_dmat);

	if (sc->parent_dmat != NULL)
		bus_dma_tag_destroy(sc->parent_dmat);

	return (0);
}

void
t3_update_qset_coalesce(struct sge_qset *qs, const struct qset_params *p)
{

	qs->rspq.holdoff_tmr = max(p->coalesce_nsecs/100, 1U);
	qs->rspq.polling = 0 /* p->polling */;
}

static void
refill_fl_cb(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	struct refill_fl_cb_arg *cb_arg = arg;
	
	cb_arg->error = error;
	cb_arg->seg = segs[0];
	cb_arg->nseg = nseg;

}

/**
 *	refill_fl - refill an SGE free-buffer list
 *	@sc: the controller softc
 *	@q: the free-list to refill
 *	@n: the number of new buffers to allocate
 *
 *	(Re)populate an SGE free-buffer list with up to @n new packet buffers.
 *	The caller must assure that @n does not exceed the queue's capacity.
 */
static void
refill_fl(adapter_t *sc, struct sge_fl *q, int n)
{
	struct rx_sw_desc *sd = &q->sdesc[q->pidx];
	struct rx_desc *d = &q->desc[q->pidx];
	struct refill_fl_cb_arg cb_arg;
	void *cl;
	int err;

	cb_arg.error = 0;
	while (n--) {
		/*
		 * We only allocate a cluster, mbuf allocation happens after rx
		 */
		if ((cl = m_cljget(NULL, M_DONTWAIT, q->buf_size)) == NULL) {
			log(LOG_WARNING, "Failed to allocate cluster\n");
			goto done;
		}
		if ((sd->flags & RX_SW_DESC_MAP_CREATED) == 0) {
			if ((err = bus_dmamap_create(q->entry_tag, 0, &sd->map))) {
				log(LOG_WARNING, "bus_dmamap_create failed %d\n", err);
				uma_zfree(q->zone, cl);
				goto done;
			}
			sd->flags |= RX_SW_DESC_MAP_CREATED;
		}
		err = bus_dmamap_load(q->entry_tag, sd->map, cl, q->buf_size,
		    refill_fl_cb, &cb_arg, 0);
		
		if (err != 0 || cb_arg.error) {
			log(LOG_WARNING, "failure in refill_fl %d\n", cb_arg.error);
			/*
			 * XXX free cluster
			 */
			return;
		}
		
		sd->flags |= RX_SW_DESC_INUSE;
		sd->cl = cl;
		d->addr_lo = htobe32(cb_arg.seg.ds_addr & 0xffffffff);
		d->addr_hi = htobe32(((uint64_t)cb_arg.seg.ds_addr >>32) & 0xffffffff);
		d->len_gen = htobe32(V_FLD_GEN1(q->gen));
		d->gen2 = htobe32(V_FLD_GEN2(q->gen));

		d++;
		sd++;

		if (++q->pidx == q->size) {
			q->pidx = 0;
			q->gen ^= 1;
			sd = q->sdesc;
			d = q->desc;
		}
		q->credits++;
	}

done:
	t3_write_reg(sc, A_SG_KDOORBELL, V_EGRCNTX(q->cntxt_id));
}


/**
 *	free_rx_bufs - free the Rx buffers on an SGE free list
 *	@sc: the controle softc
 *	@q: the SGE free list to clean up
 *
 *	Release the buffers on an SGE free-buffer Rx queue.  HW fetching from
 *	this queue should be stopped before calling this function.
 */
static void
free_rx_bufs(adapter_t *sc, struct sge_fl *q)
{
	u_int cidx = q->cidx;

	while (q->credits--) {
		struct rx_sw_desc *d = &q->sdesc[cidx];

		if (d->flags & RX_SW_DESC_INUSE) {
			bus_dmamap_unload(q->entry_tag, d->map);
			bus_dmamap_destroy(q->entry_tag, d->map);
			uma_zfree(q->zone, d->cl);
		}
		d->cl = NULL;
		if (++cidx == q->size)
			cidx = 0;
	}
}

static __inline void
__refill_fl(adapter_t *adap, struct sge_fl *fl)
{
	refill_fl(adap, fl, min(16U, fl->size - fl->credits));
}

/**
 *	recycle_rx_buf - recycle a receive buffer
 *	@adapter: the adapter
 *	@q: the SGE free list
 *	@idx: index of buffer to recycle
 *
 *	Recycles the specified buffer on the given free list by adding it at
 *	the next available slot on the list.
 */
static void
recycle_rx_buf(adapter_t *adap, struct sge_fl *q, unsigned int idx)
{
	struct rx_desc *from = &q->desc[idx];
	struct rx_desc *to   = &q->desc[q->pidx];

	q->sdesc[q->pidx] = q->sdesc[idx];
	to->addr_lo = from->addr_lo;        // already big endian
	to->addr_hi = from->addr_hi;        // likewise
	wmb();
	to->len_gen = htobe32(V_FLD_GEN1(q->gen));
	to->gen2 = htobe32(V_FLD_GEN2(q->gen));
	q->credits++;

	if (++q->pidx == q->size) {
		q->pidx = 0;
		q->gen ^= 1;
	}
	t3_write_reg(adap, A_SG_KDOORBELL, V_EGRCNTX(q->cntxt_id));
}

static void
alloc_ring_cb(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{
	uint32_t *addr;

	addr = arg;
	*addr = segs[0].ds_addr;
}

static int
alloc_ring(adapter_t *sc, size_t nelem, size_t elem_size, size_t sw_size,
    bus_addr_t *phys, void *desc, void *sdesc, bus_dma_tag_t *tag,
    bus_dmamap_t *map, bus_dma_tag_t parent_entry_tag, bus_dma_tag_t *entry_tag)
{
	size_t len = nelem * elem_size;
	void *s = NULL;
	void *p = NULL;
	int err;

	if ((err = bus_dma_tag_create(sc->parent_dmat, PAGE_SIZE, 0,
				      BUS_SPACE_MAXADDR_32BIT,
				      BUS_SPACE_MAXADDR, NULL, NULL, len, 1,
				      len, 0, NULL, NULL, tag)) != 0) {
		device_printf(sc->dev, "Cannot allocate descriptor tag\n");
		return (ENOMEM);
	}

	if ((err = bus_dmamem_alloc(*tag, (void **)&p, BUS_DMA_NOWAIT,
				    map)) != 0) {
		device_printf(sc->dev, "Cannot allocate descriptor memory\n");
		return (ENOMEM);
	}

	bus_dmamap_load(*tag, *map, p, len, alloc_ring_cb, phys, 0);
	bzero(p, len);
	*(void **)desc = p;

	if (sw_size) {
		len = nelem * sw_size;
		s = malloc(len, M_DEVBUF, M_WAITOK);
		bzero(s, len);
		*(void **)sdesc = s;
	}
	if (parent_entry_tag == NULL)
		return (0);
	    
	if ((err = bus_dma_tag_create(parent_entry_tag, 1, 0,
				      BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR,
		                      NULL, NULL, TX_MAX_SIZE, TX_MAX_SEGS,
				      TX_MAX_SIZE, BUS_DMA_ALLOCNOW,
		                      NULL, NULL, entry_tag)) != 0) {
		device_printf(sc->dev, "Cannot allocate descriptor entry tag\n");
		return (ENOMEM);
	}
	return (0);
}

static void
sge_slow_intr_handler(void *arg, int ncount)
{
	adapter_t *sc = arg;

	t3_slow_intr_handler(sc);
}

/**
 *	sge_timer_cb - perform periodic maintenance of an SGE qset
 *	@data: the SGE queue set to maintain
 *
 *	Runs periodically from a timer to perform maintenance of an SGE queue
 *	set.  It performs two tasks:
 *
 *	a) Cleans up any completed Tx descriptors that may still be pending.
 *	Normal descriptor cleanup happens when new packets are added to a Tx
 *	queue so this timer is relatively infrequent and does any cleanup only
 *	if the Tx queue has not seen any new packets in a while.  We make a
 *	best effort attempt to reclaim descriptors, in that we don't wait
 *	around if we cannot get a queue's lock (which most likely is because
 *	someone else is queueing new packets and so will also handle the clean
 *	up).  Since control queues use immediate data exclusively we don't
 *	bother cleaning them up here.
 *
 *	b) Replenishes Rx queues that have run out due to memory shortage.
 *	Normally new Rx buffers are added when existing ones are consumed but
 *	when out of memory a queue can become empty.  We try to add only a few
 *	buffers here, the queue will be replenished fully as these new buffers
 *	are used up if memory shortage has subsided.
 *	
 *	c) Return coalesced response queue credits in case a response queue is
 *	starved.
 *
 *	d) Ring doorbells for T304 tunnel queues since we have seen doorbell 
 *	fifo overflows and the FW doesn't implement any recovery scheme yet.
 */
static void
sge_timer_cb(void *arg)
{
	adapter_t *sc = arg;
	struct port_info *p;
	struct sge_qset *qs;
	struct sge_txq  *txq;
	int i, j;
	int reclaim_eth, reclaim_ofl, refill_rx;
	
	for (i = 0; i < sc->params.nports; i++) 
		for (j = 0; j < sc->port[i].nqsets; j++) {
			qs = &sc->sge.qs[i + j];
			txq = &qs->txq[0];
			reclaim_eth = txq[TXQ_ETH].processed - txq[TXQ_ETH].cleaned;
			reclaim_ofl = txq[TXQ_OFLD].processed - txq[TXQ_OFLD].cleaned;
			refill_rx = ((qs->fl[0].credits < qs->fl[0].size) || 
			    (qs->fl[1].credits < qs->fl[1].size));
			if (reclaim_eth || reclaim_ofl || refill_rx) {
				p = &sc->port[i];
				taskqueue_enqueue(p->tq, &p->timer_reclaim_task);
				break;
			}
		}
	if (sc->params.nports > 2) {
		int i;

		for_each_port(sc, i) {
			struct port_info *pi = &sc->port[i];

			t3_write_reg(sc, A_SG_KDOORBELL, 
				     F_SELEGRCNTX | 
				     (FW_TUNNEL_SGEEC_START + pi->first_qset));
		}
	}	
	if (sc->open_device_map != 0) 
		callout_reset(&sc->sge_timer_ch, TX_RECLAIM_PERIOD, sge_timer_cb, sc);
}

/*
 * This is meant to be a catch-all function to keep sge state private
 * to sge.c
 *
 */
int
t3_sge_init_adapter(adapter_t *sc)
{
	callout_init(&sc->sge_timer_ch, CALLOUT_MPSAFE);
	callout_reset(&sc->sge_timer_ch, TX_RECLAIM_PERIOD, sge_timer_cb, sc);
	TASK_INIT(&sc->slow_intr_task, 0, sge_slow_intr_handler, sc);
	return (0);
}

int
t3_sge_init_port(struct port_info *p)
{
	TASK_INIT(&p->timer_reclaim_task, 0, sge_timer_reclaim, p);
	return (0);
}

void
t3_sge_deinit_sw(adapter_t *sc)
{
	int i;
	
	callout_drain(&sc->sge_timer_ch);
	if (sc->tq) 
		taskqueue_drain(sc->tq, &sc->slow_intr_task);
	for (i = 0; i < sc->params.nports; i++) 
		if (sc->port[i].tq != NULL)
			taskqueue_drain(sc->port[i].tq, &sc->port[i].timer_reclaim_task);
}

/**
 *	refill_rspq - replenish an SGE response queue
 *	@adapter: the adapter
 *	@q: the response queue to replenish
 *	@credits: how many new responses to make available
 *
 *	Replenishes a response queue by making the supplied number of responses
 *	available to HW.
 */
static __inline void
refill_rspq(adapter_t *sc, const struct sge_rspq *q, u_int credits)
{

	/* mbufs are allocated on demand when a rspq entry is processed. */
	t3_write_reg(sc, A_SG_RSPQ_CREDIT_RETURN,
		     V_RSPQ(q->cntxt_id) | V_CREDITS(credits));
}

static __inline void
sge_txq_reclaim_(struct sge_txq *txq)
{
	int reclaimable, i, n;
	struct mbuf *m_vec[TX_CLEAN_MAX_DESC];
	struct port_info *p;

	p = txq->port;
reclaim_more:
	n = 0;
	reclaimable = desc_reclaimable(txq);
	if (reclaimable > 0 && mtx_trylock(&txq->lock)) {
		n = reclaim_completed_tx(txq, TX_CLEAN_MAX_DESC, m_vec);
		mtx_unlock(&txq->lock);
	}
	if (n == 0)
		return;
	
	for (i = 0; i < n; i++) {
		m_freem_vec(m_vec[i]);
	}
	if (p && p->ifp->if_drv_flags & IFF_DRV_OACTIVE &&
	    txq->size - txq->in_use >= TX_START_MAX_DESC) {
		txq_fills++;
		p->ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
		taskqueue_enqueue(p->tq, &p->start_task);
	}

	if (n)
		goto reclaim_more;
}

static void
sge_txq_reclaim_handler(void *arg, int ncount)
{
	struct sge_txq *q = arg;

	sge_txq_reclaim_(q);
}

static void
sge_timer_reclaim(void *arg, int ncount)
{
	struct port_info *p = arg;
	int i, nqsets = p->nqsets;
	adapter_t *sc = p->adapter;
	struct sge_qset *qs;
	struct sge_txq *txq;
	struct mtx *lock;

	for (i = 0; i < nqsets; i++) {
		qs = &sc->sge.qs[i];
		txq = &qs->txq[TXQ_ETH];
		sge_txq_reclaim_(txq);

		txq = &qs->txq[TXQ_OFLD];
		sge_txq_reclaim_(txq);
		
		lock = (sc->flags & USING_MSIX) ? &qs->rspq.lock :
			    &sc->sge.qs[0].rspq.lock;

		if (mtx_trylock(lock)) {
			/* XXX currently assume that we are *NOT* polling */
			uint32_t status = t3_read_reg(sc, A_SG_RSPQ_FL_STATUS);

			if (qs->fl[0].credits < qs->fl[0].size - 16)
				__refill_fl(sc, &qs->fl[0]);
			if (qs->fl[1].credits < qs->fl[1].size - 16)
				__refill_fl(sc, &qs->fl[1]);
			
			if (status & (1 << qs->rspq.cntxt_id)) {
				if (qs->rspq.credits) {
					refill_rspq(sc, &qs->rspq, 1);
					qs->rspq.credits--;
					t3_write_reg(sc, A_SG_RSPQ_FL_STATUS, 
					    1 << qs->rspq.cntxt_id);
				}
			}
			mtx_unlock(lock);
		}
	}
}

/**
 *	init_qset_cntxt - initialize an SGE queue set context info
 *	@qs: the queue set
 *	@id: the queue set id
 *
 *	Initializes the TIDs and context ids for the queues of a queue set.
 */
static void
init_qset_cntxt(struct sge_qset *qs, u_int id)
{

	qs->rspq.cntxt_id = id;
	qs->fl[0].cntxt_id = 2 * id;
	qs->fl[1].cntxt_id = 2 * id + 1;
	qs->txq[TXQ_ETH].cntxt_id = FW_TUNNEL_SGEEC_START + id;
	qs->txq[TXQ_ETH].token = FW_TUNNEL_TID_START + id;
	qs->txq[TXQ_OFLD].cntxt_id = FW_OFLD_SGEEC_START + id;
	qs->txq[TXQ_CTRL].cntxt_id = FW_CTRL_SGEEC_START + id;
	qs->txq[TXQ_CTRL].token = FW_CTRL_TID_START + id;
}


static void
txq_prod(struct sge_txq *txq, unsigned int ndesc, struct txq_state *txqs)
{
	txq->in_use += ndesc;
	/*
	 * XXX we don't handle stopping of queue
	 * presumably start handles this when we bump against the end
	 */
	txqs->gen = txq->gen;
	txq->unacked += ndesc;
	txqs->compl = (txq->unacked & 8) << (S_WR_COMPL - 3);
	txq->unacked &= 7;
	txqs->pidx = txq->pidx;
	txq->pidx += ndesc;
	
	if (txq->pidx >= txq->size) {
		txq->pidx -= txq->size;
		txq->gen ^= 1;
	}

}

/**
 *	calc_tx_descs - calculate the number of Tx descriptors for a packet
 *	@m: the packet mbufs
 *      @nsegs: the number of segments 
 *
 * 	Returns the number of Tx descriptors needed for the given Ethernet
 * 	packet.  Ethernet packets require addition of WR and CPL headers.
 */
static __inline unsigned int
calc_tx_descs(const struct mbuf *m, int nsegs)
{
	unsigned int flits;

	if (m->m_pkthdr.len <= WR_LEN - sizeof(struct cpl_tx_pkt))
		return 1;

	flits = sgl_len(nsegs) + 2;
#ifdef TSO_SUPPORTED
	if  (m->m_pkthdr.csum_flags & (CSUM_TSO))
		flits++;
#endif	
	return flits_to_desc(flits);
}

static unsigned int
busdma_map_mbufs(struct mbuf **m, struct sge_txq *txq,
    struct tx_sw_desc *stx, bus_dma_segment_t *segs, int *nsegs)
{
	struct mbuf *m0;
	int err, pktlen;
	
	m0 = *m;
	pktlen = m0->m_pkthdr.len;

	err = bus_dmamap_load_mvec_sg(txq->entry_tag, stx->map, m0, segs, nsegs, 0);
#ifdef DEBUG		
	if (err) {
		int n = 0;
		struct mbuf *mtmp = m0;
		while(mtmp) {
			n++;
			mtmp = mtmp->m_next;
		}
		printf("map_mbufs: bus_dmamap_load_mbuf_sg failed with %d - pkthdr.len==%d nmbufs=%d\n",
		    err, m0->m_pkthdr.len, n);
	}
#endif
	if (err == EFBIG) {
		/* Too many segments, try to defrag */
		m0 = m_defrag(m0, M_DONTWAIT);
		if (m0 == NULL) {
			m_freem(*m);
			*m = NULL;
			return (ENOBUFS);
		}
		*m = m0;
		err = bus_dmamap_load_mbuf_sg(txq->entry_tag, stx->map, m0, segs, nsegs, 0);
	}

	if (err == ENOMEM) {
		return (err);
	}

	if (err) {
		if (cxgb_debug)
			printf("map failure err=%d pktlen=%d\n", err, pktlen);
		m_freem_vec(m0);
		*m = NULL;
		return (err);
	}

	bus_dmamap_sync(txq->entry_tag, stx->map, BUS_DMASYNC_PREWRITE);
	stx->flags |= TX_SW_DESC_MAPPED;

	return (0);
}

/**
 *	make_sgl - populate a scatter/gather list for a packet
 *	@sgp: the SGL to populate
 *	@segs: the packet dma segments
 *	@nsegs: the number of segments
 *
 *	Generates a scatter/gather list for the buffers that make up a packet
 *	and returns the SGL size in 8-byte words.  The caller must size the SGL
 *	appropriately.
 */
static __inline void
make_sgl(struct sg_ent *sgp, bus_dma_segment_t *segs, int nsegs)
{
	int i, idx;
	
	for (idx = 0, i = 0; i < nsegs; i++, idx ^= 1) {
		if (i && idx == 0) 
			++sgp;

		sgp->len[idx] = htobe32(segs[i].ds_len);
		sgp->addr[idx] = htobe64(segs[i].ds_addr);
	}
	
	if (idx)
		sgp->len[idx] = 0;
}
	
/**
 *	check_ring_tx_db - check and potentially ring a Tx queue's doorbell
 *	@adap: the adapter
 *	@q: the Tx queue
 *
 *	Ring the doorbel if a Tx queue is asleep.  There is a natural race,
 *	where the HW is going to sleep just after we checked, however,
 *	then the interrupt handler will detect the outstanding TX packet
 *	and ring the doorbell for us.
 *
 *	When GTS is disabled we unconditionally ring the doorbell.
 */
static __inline void
check_ring_tx_db(adapter_t *adap, struct sge_txq *q)
{
#if USE_GTS
	clear_bit(TXQ_LAST_PKT_DB, &q->flags);
	if (test_and_set_bit(TXQ_RUNNING, &q->flags) == 0) {
		set_bit(TXQ_LAST_PKT_DB, &q->flags);
#ifdef T3_TRACE
		T3_TRACE1(adap->tb[q->cntxt_id & 7], "doorbell Tx, cntxt %d",
			  q->cntxt_id);
#endif
		t3_write_reg(adap, A_SG_KDOORBELL,
			     F_SELEGRCNTX | V_EGRCNTX(q->cntxt_id));
	}
#else
	wmb();            /* write descriptors before telling HW */
	t3_write_reg(adap, A_SG_KDOORBELL,
		     F_SELEGRCNTX | V_EGRCNTX(q->cntxt_id));
#endif
}

static __inline void
wr_gen2(struct tx_desc *d, unsigned int gen)
{
#if SGE_NUM_GENBITS == 2
	d->flit[TX_DESC_FLITS - 1] = htobe64(gen);
#endif
}



/**
 *	write_wr_hdr_sgl - write a WR header and, optionally, SGL
 *	@ndesc: number of Tx descriptors spanned by the SGL
 *	@txd: first Tx descriptor to be written
 *	@txqs: txq state (generation and producer index)
 *	@txq: the SGE Tx queue
 *	@sgl: the SGL
 *	@flits: number of flits to the start of the SGL in the first descriptor
 *	@sgl_flits: the SGL size in flits
 *	@wr_hi: top 32 bits of WR header based on WR type (big endian)
 *	@wr_lo: low 32 bits of WR header based on WR type (big endian)
 *
 *	Write a work request header and an associated SGL.  If the SGL is
 *	small enough to fit into one Tx descriptor it has already been written
 *	and we just need to write the WR header.  Otherwise we distribute the
 *	SGL across the number of descriptors it spans.
 */

static void
write_wr_hdr_sgl(unsigned int ndesc, struct tx_desc *txd, struct txq_state *txqs,
    const struct sge_txq *txq, const struct sg_ent *sgl, unsigned int flits,
    unsigned int sgl_flits, unsigned int wr_hi, unsigned int wr_lo)
{

	struct work_request_hdr *wrp = (struct work_request_hdr *)txd;
	struct tx_sw_desc *txsd = &txq->sdesc[txqs->pidx];
	
	if (__predict_true(ndesc == 1)) {
		wrp->wr_hi = htonl(F_WR_SOP | F_WR_EOP | V_WR_DATATYPE(1) |
		    V_WR_SGLSFLT(flits)) | wr_hi;
		wmb();
		wrp->wr_lo = htonl(V_WR_LEN(flits + sgl_flits) |
		    V_WR_GEN(txqs->gen)) | wr_lo;
		/* XXX gen? */
		wr_gen2(txd, txqs->gen);
	} else {
		unsigned int ogen = txqs->gen;
		const uint64_t *fp = (const uint64_t *)sgl;
		struct work_request_hdr *wp = wrp;
		
		wrp->wr_hi = htonl(F_WR_SOP | V_WR_DATATYPE(1) |
		    V_WR_SGLSFLT(flits)) | wr_hi;
		
		while (sgl_flits) {
			unsigned int avail = WR_FLITS - flits;

			if (avail > sgl_flits)
				avail = sgl_flits;
			memcpy(&txd->flit[flits], fp, avail * sizeof(*fp));
			sgl_flits -= avail;
			ndesc--;
			if (!sgl_flits)
				break;
			
			fp += avail;
			txd++;
			txsd++;
			if (++txqs->pidx == txq->size) {
				txqs->pidx = 0;
				txqs->gen ^= 1;
				txd = txq->desc;
				txsd = txq->sdesc;
			}
			
			/*
			 * when the head of the mbuf chain
			 * is freed all clusters will be freed
			 * with it
			 */
			txsd->m = NULL;
			wrp = (struct work_request_hdr *)txd;
			wrp->wr_hi = htonl(V_WR_DATATYPE(1) |
			    V_WR_SGLSFLT(1)) | wr_hi;
			wrp->wr_lo = htonl(V_WR_LEN(min(WR_FLITS,
				    sgl_flits + 1)) |
			    V_WR_GEN(txqs->gen)) | wr_lo;
			wr_gen2(txd, txqs->gen);
			flits = 1;
		}
		wrp->wr_hi |= htonl(F_WR_EOP);
		wmb();
		wp->wr_lo = htonl(V_WR_LEN(WR_FLITS) | V_WR_GEN(ogen)) | wr_lo;
		wr_gen2((struct tx_desc *)wp, ogen);
	}
}

	
/* sizeof(*eh) + sizeof(*vhdr) + sizeof(*ip) + sizeof(*tcp) */
#define TCPPKTHDRSIZE (ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN + 20 + 20)

int
t3_encap(struct port_info *p, struct mbuf **m)
{
	adapter_t *sc;
	struct mbuf *m0;
	struct sge_qset *qs;
	struct sge_txq *txq;
	struct tx_sw_desc *stx;
	struct txq_state txqs;
	unsigned int nsegs, ndesc, flits, cntrl, mlen;
	int err, tso_info = 0;

	struct work_request_hdr *wrp;
	struct tx_sw_desc *txsd;
	struct sg_ent *sgp, sgl[TX_MAX_SEGS / 2 + 1];
	bus_dma_segment_t segs[TX_MAX_SEGS];
	uint32_t wr_hi, wr_lo, sgl_flits; 

	struct tx_desc *txd;
	struct cpl_tx_pkt *cpl;
	
	DPRINTF("t3_encap ");
	m0 = *m;	
	sc = p->adapter;
	qs = &sc->sge.qs[p->first_qset];
	txq = &qs->txq[TXQ_ETH];
	stx = &txq->sdesc[txq->pidx];
	txd = &txq->desc[txq->pidx];
	cpl = (struct cpl_tx_pkt *)txd;
	mlen = m0->m_pkthdr.len;
	cpl->len = htonl(mlen | 0x80000000);
	
	DPRINTF("mlen=%d\n", mlen);
	/*
	 * XXX handle checksum, TSO, and VLAN here
	 *	 
	 */
	cntrl = V_TXPKT_INTF(p->port);

	/*
	 * XXX need to add VLAN support for 6.x
	 */
#ifdef VLAN_SUPPORTED
	if (m0->m_flags & M_VLANTAG) 
		cntrl |= F_TXPKT_VLAN_VLD | V_TXPKT_VLAN(m0->m_pkthdr.ether_vtag);
	if  (m0->m_pkthdr.csum_flags & (CSUM_TSO))
		tso_info = V_LSO_MSS(m0->m_pkthdr.tso_segsz);
#endif		
	if (tso_info) {
		int eth_type;
		struct cpl_tx_pkt_lso *hdr = (struct cpl_tx_pkt_lso *) cpl;
		struct ip *ip;
		struct tcphdr *tcp;
		uint8_t *pkthdr, tmp[TCPPKTHDRSIZE]; /* is this too large for the stack? */
		
		txd->flit[2] = 0;
		cntrl |= V_TXPKT_OPCODE(CPL_TX_PKT_LSO);
		hdr->cntrl = htonl(cntrl);
		
		if (__predict_false(m0->m_len < TCPPKTHDRSIZE)) {
			pkthdr = &tmp[0];
			m_copydata(m0, 0, TCPPKTHDRSIZE, pkthdr);
		} else {
			pkthdr = mtod(m0, uint8_t *);
		}

		if (__predict_false(m0->m_flags & M_VLANTAG)) {
			eth_type = CPL_ETH_II_VLAN;
			ip = (struct ip *)(pkthdr + ETHER_HDR_LEN +
			    ETHER_VLAN_ENCAP_LEN);
		} else {
			eth_type = CPL_ETH_II;
			ip = (struct ip *)(pkthdr + ETHER_HDR_LEN);
		}
		tcp = (struct tcphdr *)((uint8_t *)ip +
		    sizeof(*ip)); 

		tso_info |= V_LSO_ETH_TYPE(eth_type) |
			    V_LSO_IPHDR_WORDS(ip->ip_hl) |
			    V_LSO_TCPHDR_WORDS(tcp->th_off);
		hdr->lso_info = htonl(tso_info);
		flits = 3;	
	} else {
		cntrl |= V_TXPKT_OPCODE(CPL_TX_PKT);
		cpl->cntrl = htonl(cntrl);
		
		if (mlen <= WR_LEN - sizeof(*cpl)) {
			txq_prod(txq, 1, &txqs);
			txq->sdesc[txqs.pidx].m = m0;
			m_set_priority(m0, txqs.pidx);
			
			if (m0->m_len == m0->m_pkthdr.len)
				memcpy(&txd->flit[2], mtod(m0, uint8_t *), mlen);
			else
				m_copydata(m0, 0, mlen, (caddr_t)&txd->flit[2]);

			flits = (mlen + 7) / 8 + 2;
			cpl->wr.wr_hi = htonl(V_WR_BCNTLFLT(mlen & 7) |
					  V_WR_OP(FW_WROPCODE_TUNNEL_TX_PKT) |
					  F_WR_SOP | F_WR_EOP | txqs.compl);
			wmb();
			cpl->wr.wr_lo = htonl(V_WR_LEN(flits) |
			    V_WR_GEN(txqs.gen) | V_WR_TID(txq->token));

			wr_gen2(txd, txqs.gen);
			check_ring_tx_db(sc, txq);
			return (0);
		}
		flits = 2;
	}

	wrp = (struct work_request_hdr *)txd;
	
	if ((err = busdma_map_mbufs(m, txq, stx, segs, &nsegs)) != 0) {
		return (err);
	}
	m0 = *m;
	ndesc = calc_tx_descs(m0, nsegs);
	
	sgp = (ndesc == 1) ? (struct sg_ent *)&txd->flit[flits] : sgl;
	make_sgl(sgp, segs, nsegs);

	sgl_flits = sgl_len(nsegs);

	DPRINTF("make_sgl success nsegs==%d ndesc==%d\n", nsegs, ndesc);
	txq_prod(txq, ndesc, &txqs);
	txsd = &txq->sdesc[txqs.pidx];
	wr_hi = htonl(V_WR_OP(FW_WROPCODE_TUNNEL_TX_PKT) | txqs.compl);
	wr_lo = htonl(V_WR_TID(txq->token));
	txsd->m = m0;
	m_set_priority(m0, txqs.pidx); 

	write_wr_hdr_sgl(ndesc, txd, &txqs, txq, sgl, flits, sgl_flits, wr_hi, wr_lo);
	check_ring_tx_db(p->adapter, txq);

	return (0);
}


/**
 *	write_imm - write a packet into a Tx descriptor as immediate data
 *	@d: the Tx descriptor to write
 *	@m: the packet
 *	@len: the length of packet data to write as immediate data
 *	@gen: the generation bit value to write
 *
 *	Writes a packet as immediate data into a Tx descriptor.  The packet
 *	contains a work request at its beginning.  We must write the packet
 *	carefully so the SGE doesn't read accidentally before it's written in
 *	its entirety.
 */
static __inline void
write_imm(struct tx_desc *d, struct mbuf *m,
	  unsigned int len, unsigned int gen)
{
	struct work_request_hdr *from = mtod(m, struct work_request_hdr *);
	struct work_request_hdr *to = (struct work_request_hdr *)d;

	memcpy(&to[1], &from[1], len - sizeof(*from));
	to->wr_hi = from->wr_hi | htonl(F_WR_SOP | F_WR_EOP |
					V_WR_BCNTLFLT(len & 7));
	wmb();
	to->wr_lo = from->wr_lo | htonl(V_WR_GEN(gen) |
					V_WR_LEN((len + 7) / 8));
	wr_gen2(d, gen);
	m_freem(m);
}

/**
 *	check_desc_avail - check descriptor availability on a send queue
 *	@adap: the adapter
 *	@q: the TX queue
 *	@m: the packet needing the descriptors
 *	@ndesc: the number of Tx descriptors needed
 *	@qid: the Tx queue number in its queue set (TXQ_OFLD or TXQ_CTRL)
 *
 *	Checks if the requested number of Tx descriptors is available on an
 *	SGE send queue.  If the queue is already suspended or not enough
 *	descriptors are available the packet is queued for later transmission.
 *	Must be called with the Tx queue locked.
 *
 *	Returns 0 if enough descriptors are available, 1 if there aren't
 *	enough descriptors and the packet has been queued, and 2 if the caller
 *	needs to retry because there weren't enough descriptors at the
 *	beginning of the call but some freed up in the mean time.
 */
static __inline int
check_desc_avail(adapter_t *adap, struct sge_txq *q,
		 struct mbuf *m, unsigned int ndesc,
		 unsigned int qid)
{
	/* 
	 * XXX We currently only use this for checking the control queue
	 * the control queue is only used for binding qsets which happens
	 * at init time so we are guaranteed enough descriptors
	 */
	if (__predict_false(!mbufq_empty(&q->sendq))) {
addq_exit:	mbufq_tail(&q->sendq, m);
		return 1;
	}
	if (__predict_false(q->size - q->in_use < ndesc)) {

		struct sge_qset *qs = txq_to_qset(q, qid);

		setbit(&qs->txq_stopped, qid);
		smp_mb();

		if (should_restart_tx(q) &&
		    test_and_clear_bit(qid, &qs->txq_stopped))
			return 2;

		q->stops++;
		goto addq_exit;
	}
	return 0;
}


/**
 *	reclaim_completed_tx_imm - reclaim completed control-queue Tx descs
 *	@q: the SGE control Tx queue
 *
 *	This is a variant of reclaim_completed_tx() that is used for Tx queues
 *	that send only immediate data (presently just the control queues) and
 *	thus do not have any mbufs
 */
static __inline void
reclaim_completed_tx_imm(struct sge_txq *q)
{
	unsigned int reclaim = q->processed - q->cleaned;

	mtx_assert(&q->lock, MA_OWNED);
	
	q->in_use -= reclaim;
	q->cleaned += reclaim;
}

static __inline int
immediate(const struct mbuf *m)
{
	return m->m_len <= WR_LEN  && m->m_pkthdr.len <= WR_LEN ;
}

/**
 *	ctrl_xmit - send a packet through an SGE control Tx queue
 *	@adap: the adapter
 *	@q: the control queue
 *	@m: the packet
 *
 *	Send a packet through an SGE control Tx queue.  Packets sent through
 *	a control queue must fit entirely as immediate data in a single Tx
 *	descriptor and have no page fragments.
 */
static int
ctrl_xmit(adapter_t *adap, struct sge_txq *q, struct mbuf *m)
{
	int ret;
	struct work_request_hdr *wrp = mtod(m, struct work_request_hdr *);

	if (__predict_false(!immediate(m))) {
		m_freem(m);
		return 0;
	}

	wrp->wr_hi |= htonl(F_WR_SOP | F_WR_EOP);
	wrp->wr_lo = htonl(V_WR_TID(q->token));

	mtx_lock(&q->lock);
again:	reclaim_completed_tx_imm(q);

	ret = check_desc_avail(adap, q, m, 1, TXQ_CTRL);
	if (__predict_false(ret)) {
		if (ret == 1) {
			mtx_unlock(&q->lock);
			return (-1);
		}
		goto again;
	}

	write_imm(&q->desc[q->pidx], m, m->m_len, q->gen);

	q->in_use++;
	if (++q->pidx >= q->size) {
		q->pidx = 0;
		q->gen ^= 1;
	}
	mtx_unlock(&q->lock);
	wmb();
	t3_write_reg(adap, A_SG_KDOORBELL,
		     F_SELEGRCNTX | V_EGRCNTX(q->cntxt_id));
	return (0);
}


/**
 *	restart_ctrlq - restart a suspended control queue
 *	@qs: the queue set cotaining the control queue
 *
 *	Resumes transmission on a suspended Tx control queue.
 */
static void
restart_ctrlq(void *data, int npending)
{
	struct mbuf *m;
	struct sge_qset *qs = (struct sge_qset *)data;
	struct sge_txq *q = &qs->txq[TXQ_CTRL];
	adapter_t *adap = qs->port->adapter;

	mtx_lock(&q->lock);
again:	reclaim_completed_tx_imm(q);

	while (q->in_use < q->size &&
	       (m = mbufq_dequeue(&q->sendq)) != NULL) {

		write_imm(&q->desc[q->pidx], m, m->m_len, q->gen);

		if (++q->pidx >= q->size) {
			q->pidx = 0;
			q->gen ^= 1;
		}
		q->in_use++;
	}
	if (!mbufq_empty(&q->sendq)) {
		setbit(&qs->txq_stopped, TXQ_CTRL);
		smp_mb();

		if (should_restart_tx(q) &&
		    test_and_clear_bit(TXQ_CTRL, &qs->txq_stopped))
			goto again;
		q->stops++;
	}
	mtx_unlock(&q->lock);
	t3_write_reg(adap, A_SG_KDOORBELL,
		     F_SELEGRCNTX | V_EGRCNTX(q->cntxt_id));
}


/*
 * Send a management message through control queue 0
 */
int
t3_mgmt_tx(struct adapter *adap, struct mbuf *m)
{
	return ctrl_xmit(adap, &adap->sge.qs[0].txq[TXQ_CTRL], m);
}

/**
 *	free_qset - free the resources of an SGE queue set
 *	@sc: the controller owning the queue set
 *	@q: the queue set
 *
 *	Release the HW and SW resources associated with an SGE queue set, such
 *	as HW contexts, packet buffers, and descriptor rings.  Traffic to the
 *	queue set must be quiesced prior to calling this.
 */
static void
t3_free_qset(adapter_t *sc, struct sge_qset *q)
{
	int i;

	for (i = 0; i < SGE_RXQ_PER_SET; ++i) {
		if (q->fl[i].desc) {
			mtx_lock(&sc->sge.reg_lock);
			t3_sge_disable_fl(sc, q->fl[i].cntxt_id);
			mtx_unlock(&sc->sge.reg_lock);
			bus_dmamap_unload(q->fl[i].desc_tag, q->fl[i].desc_map);
			bus_dmamem_free(q->fl[i].desc_tag, q->fl[i].desc,
					q->fl[i].desc_map);
			bus_dma_tag_destroy(q->fl[i].desc_tag);
			bus_dma_tag_destroy(q->fl[i].entry_tag);
		}
		if (q->fl[i].sdesc) {
			free_rx_bufs(sc, &q->fl[i]);
			free(q->fl[i].sdesc, M_DEVBUF);
		}
	}

	for (i = 0; i < SGE_TXQ_PER_SET; i++) {
		if (q->txq[i].desc) {
			mtx_lock(&sc->sge.reg_lock);
			t3_sge_enable_ecntxt(sc, q->txq[i].cntxt_id, 0);
			mtx_unlock(&sc->sge.reg_lock);
			bus_dmamap_unload(q->txq[i].desc_tag,
					q->txq[i].desc_map);
			bus_dmamem_free(q->txq[i].desc_tag, q->txq[i].desc,
					q->txq[i].desc_map);
			bus_dma_tag_destroy(q->txq[i].desc_tag);
			bus_dma_tag_destroy(q->txq[i].entry_tag);
			MTX_DESTROY(&q->txq[i].lock);
		}
		if (q->txq[i].sdesc) {
			free(q->txq[i].sdesc, M_DEVBUF);
		}
	}

	if (q->rspq.desc) {
		mtx_lock(&sc->sge.reg_lock);
		t3_sge_disable_rspcntxt(sc, q->rspq.cntxt_id);
		mtx_unlock(&sc->sge.reg_lock);
		
		bus_dmamap_unload(q->rspq.desc_tag, q->rspq.desc_map);
		bus_dmamem_free(q->rspq.desc_tag, q->rspq.desc,
			        q->rspq.desc_map);
		bus_dma_tag_destroy(q->rspq.desc_tag);
		MTX_DESTROY(&q->rspq.lock);
	}

	bzero(q, sizeof(*q));
}

/**
 *	t3_free_sge_resources - free SGE resources
 *	@sc: the adapter softc
 *
 *	Frees resources used by the SGE queue sets.
 */
void
t3_free_sge_resources(adapter_t *sc)
{
	int i, nqsets;

	for (nqsets = i = 0; i < (sc)->params.nports; i++) 
		nqsets += sc->port[i].nqsets;
	
	for (i = 0; i < nqsets; ++i)
		t3_free_qset(sc, &sc->sge.qs[i]);
}

/**
 *	t3_sge_start - enable SGE
 *	@sc: the controller softc
 *
 *	Enables the SGE for DMAs.  This is the last step in starting packet
 *	transfers.
 */
void
t3_sge_start(adapter_t *sc)
{
	t3_set_reg_field(sc, A_SG_CONTROL, F_GLOBALENABLE, F_GLOBALENABLE);
}

/**
 *	t3_sge_stop - disable SGE operation
 *	@sc: the adapter
 *
 *	Disables the DMA engine.  This can be called in emeregencies (e.g.,
 *	from error interrupts) or from normal process context.  In the latter
 *	case it also disables any pending queue restart tasklets.  Note that
 *	if it is called in interrupt context it cannot disable the restart
 *	tasklets as it cannot wait, however the tasklets will have no effect
 *	since the doorbells are disabled and the driver will call this again
 *	later from process context, at which time the tasklets will be stopped
 *	if they are still running.
 */
void
t3_sge_stop(adapter_t *sc)
{
	int i, nqsets;
	
	t3_set_reg_field(sc, A_SG_CONTROL, F_GLOBALENABLE, 0);

	if (sc->tq == NULL)
		return;
	
	for (nqsets = i = 0; i < (sc)->params.nports; i++) 
		nqsets += sc->port[i].nqsets;
	
	for (i = 0; i < nqsets; ++i) {
		struct sge_qset *qs = &sc->sge.qs[i];
		
		taskqueue_drain(sc->tq, &qs->txq[TXQ_OFLD].qresume_task);
		taskqueue_drain(sc->tq, &qs->txq[TXQ_CTRL].qresume_task);
	}
}


/**
 *	free_tx_desc - reclaims Tx descriptors and their buffers
 *	@adapter: the adapter
 *	@q: the Tx queue to reclaim descriptors from
 *	@n: the number of descriptors to reclaim
 *
 *	Reclaims Tx descriptors from an SGE Tx queue and frees the associated
 *	Tx buffers.  Called with the Tx queue lock held.
 */
int
free_tx_desc(struct sge_txq *q, int n, struct mbuf **m_vec)
{
	struct tx_sw_desc *d;
	unsigned int cidx = q->cidx;
	int nbufs = 0;
	
#ifdef T3_TRACE
	T3_TRACE2(sc->tb[q->cntxt_id & 7],
		  "reclaiming %u Tx descriptors at cidx %u", n, cidx);
#endif
	d = &q->sdesc[cidx];
	
	while (n-- > 0) {
		DPRINTF("cidx=%d d=%p\n", cidx, d);
		if (d->m) {
			if (d->flags & TX_SW_DESC_MAPPED) {
				bus_dmamap_unload(q->entry_tag, d->map);
				bus_dmamap_destroy(q->entry_tag, d->map);
				d->flags &= ~TX_SW_DESC_MAPPED;
			}
			if (m_get_priority(d->m) == cidx) {
				m_vec[nbufs] = d->m;
				d->m = NULL;
				nbufs++;
			} else {
				printf("pri=%d cidx=%d\n", (int)m_get_priority(d->m), cidx);
			}
		}
		++d;
		if (++cidx == q->size) {
			cidx = 0;
			d = q->sdesc;
		}
	}
	q->cidx = cidx;

	return (nbufs);
}

/**
 *	is_new_response - check if a response is newly written
 *	@r: the response descriptor
 *	@q: the response queue
 *
 *	Returns true if a response descriptor contains a yet unprocessed
 *	response.
 */
static __inline int
is_new_response(const struct rsp_desc *r,
    const struct sge_rspq *q)
{
	return (r->intr_gen & F_RSPD_GEN2) == q->gen;
}

#define RSPD_GTS_MASK  (F_RSPD_TXQ0_GTS | F_RSPD_TXQ1_GTS)
#define RSPD_CTRL_MASK (RSPD_GTS_MASK | \
			V_RSPD_TXQ0_CR(M_RSPD_TXQ0_CR) | \
			V_RSPD_TXQ1_CR(M_RSPD_TXQ1_CR) | \
			V_RSPD_TXQ2_CR(M_RSPD_TXQ2_CR))

/* How long to delay the next interrupt in case of memory shortage, in 0.1us. */
#define NOMEM_INTR_DELAY 2500

/**
 *	write_ofld_wr - write an offload work request
 *	@adap: the adapter
 *	@m: the packet to send
 *	@q: the Tx queue
 *	@pidx: index of the first Tx descriptor to write
 *	@gen: the generation value to use
 *	@ndesc: number of descriptors the packet will occupy
 *
 *	Write an offload work request to send the supplied packet.  The packet
 *	data already carry the work request with most fields populated.
 */
static void
write_ofld_wr(adapter_t *adap, struct mbuf *m,
    struct sge_txq *q, unsigned int pidx,
    unsigned int gen, unsigned int ndesc,
    bus_dma_segment_t *segs, unsigned int nsegs)
{
	unsigned int sgl_flits, flits;
	struct work_request_hdr *from;
	struct sg_ent *sgp, sgl[TX_MAX_SEGS / 2 + 1];
	struct tx_desc *d = &q->desc[pidx];
	struct txq_state txqs;
	
	if (immediate(m)) {
		q->sdesc[pidx].m = NULL;
		write_imm(d, m, m->m_len, gen);
		return;
	}

	/* Only TX_DATA builds SGLs */

	from = mtod(m, struct work_request_hdr *);
	memcpy(&d->flit[1], &from[1],
	    (uint8_t *)m->m_pkthdr.header - mtod(m, uint8_t *) - sizeof(*from));

	flits = ((uint8_t *)m->m_pkthdr.header - mtod(m, uint8_t *)) / 8;
	sgp = (ndesc == 1) ? (struct sg_ent *)&d->flit[flits] : sgl;

	make_sgl(sgp, segs, nsegs);
	sgl_flits = sgl_len(nsegs);

	txqs.gen = q->gen;
	txqs.pidx = q->pidx;
	txqs.compl = (q->unacked & 8) << (S_WR_COMPL - 3);
	write_wr_hdr_sgl(ndesc, d, &txqs, q, sgl, flits, sgl_flits,
	    from->wr_hi, from->wr_lo);
}

/**
 *	calc_tx_descs_ofld - calculate # of Tx descriptors for an offload packet
 *	@m: the packet
 *
 * 	Returns the number of Tx descriptors needed for the given offload
 * 	packet.  These packets are already fully constructed.
 */
static __inline unsigned int
calc_tx_descs_ofld(struct mbuf *m, unsigned int nsegs)
{
	unsigned int flits, cnt = 0;


	if (m->m_len <= WR_LEN)
		return 1;                 /* packet fits as immediate data */

	if (m->m_flags & M_IOVEC)
		cnt = mtomv(m)->mv_count;

	flits = ((uint8_t *)m->m_pkthdr.header - mtod(m, uint8_t *)) / 8;   /* headers */

	return flits_to_desc(flits + sgl_len(cnt));
}

/**
 *	ofld_xmit - send a packet through an offload queue
 *	@adap: the adapter
 *	@q: the Tx offload queue
 *	@m: the packet
 *
 *	Send an offload packet through an SGE offload queue.
 */
static int
ofld_xmit(adapter_t *adap, struct sge_txq *q, struct mbuf *m)
{
	int ret;
	unsigned int pidx, gen, nsegs;
	unsigned int ndesc;
	struct mbuf *m_vec[TX_CLEAN_MAX_DESC];
	bus_dma_segment_t segs[TX_MAX_SEGS];
	int i, cleaned;
	struct tx_sw_desc *stx = &q->sdesc[q->pidx];

	mtx_lock(&q->lock);
	if ((ret = busdma_map_mbufs(&m, q, stx, segs, &nsegs)) != 0) {
		mtx_unlock(&q->lock);
		return (ret);
	}
	ndesc = calc_tx_descs_ofld(m, nsegs);
again:	cleaned = reclaim_completed_tx(q, TX_CLEAN_MAX_DESC, m_vec);

	ret = check_desc_avail(adap, q, m, ndesc, TXQ_OFLD);
	if (__predict_false(ret)) {
		if (ret == 1) {
			m_set_priority(m, ndesc);     /* save for restart */
			mtx_unlock(&q->lock);
			return NET_XMIT_CN;
		}
		goto again;
	}

	gen = q->gen;
	q->in_use += ndesc;
	pidx = q->pidx;
	q->pidx += ndesc;
	if (q->pidx >= q->size) {
		q->pidx -= q->size;
		q->gen ^= 1;
	}
#ifdef T3_TRACE
	T3_TRACE5(adap->tb[q->cntxt_id & 7],
		  "ofld_xmit: ndesc %u, pidx %u, len %u, main %u, frags %u",
		  ndesc, pidx, skb->len, skb->len - skb->data_len,
		  skb_shinfo(skb)->nr_frags);
#endif
	mtx_unlock(&q->lock);

	write_ofld_wr(adap, m, q, pidx, gen, ndesc, segs, nsegs);
	check_ring_tx_db(adap, q);
	
	for (i = 0; i < cleaned; i++) {
		m_freem_vec(m_vec[i]);
	}
	return NET_XMIT_SUCCESS;
}

/**
 *	restart_offloadq - restart a suspended offload queue
 *	@qs: the queue set cotaining the offload queue
 *
 *	Resumes transmission on a suspended Tx offload queue.
 */
static void
restart_offloadq(void *data, int npending)
{

	struct mbuf *m;
	struct sge_qset *qs = data;
	struct sge_txq *q = &qs->txq[TXQ_OFLD];
	adapter_t *adap = qs->port->adapter;
	struct mbuf *m_vec[TX_CLEAN_MAX_DESC];
	bus_dma_segment_t segs[TX_MAX_SEGS];
	int nsegs, i, cleaned;
	struct tx_sw_desc *stx = &q->sdesc[q->pidx];
		
	mtx_lock(&q->lock);
again:	cleaned = reclaim_completed_tx(q, TX_CLEAN_MAX_DESC, m_vec);

	while ((m = mbufq_peek(&q->sendq)) != NULL) {
		unsigned int gen, pidx;
		unsigned int ndesc = m_get_priority(m);

		if (__predict_false(q->size - q->in_use < ndesc)) {
			setbit(&qs->txq_stopped, TXQ_OFLD);
			smp_mb();

			if (should_restart_tx(q) &&
			    test_and_clear_bit(TXQ_OFLD, &qs->txq_stopped))
				goto again;
			q->stops++;
			break;
		}

		gen = q->gen;
		q->in_use += ndesc;
		pidx = q->pidx;
		q->pidx += ndesc;
		if (q->pidx >= q->size) {
			q->pidx -= q->size;
			q->gen ^= 1;
		}
		
		(void)mbufq_dequeue(&q->sendq);
		busdma_map_mbufs(&m, q, stx, segs, &nsegs);
		mtx_unlock(&q->lock);
		write_ofld_wr(adap, m, q, pidx, gen, ndesc, segs, nsegs);
		mtx_lock(&q->lock);
	}
	mtx_unlock(&q->lock);
	
#if USE_GTS
	set_bit(TXQ_RUNNING, &q->flags);
	set_bit(TXQ_LAST_PKT_DB, &q->flags);
#endif
	t3_write_reg(adap, A_SG_KDOORBELL,
		     F_SELEGRCNTX | V_EGRCNTX(q->cntxt_id));
	
	for (i = 0; i < cleaned; i++) {
		m_freem_vec(m_vec[i]);
	}
}

/**
 *	queue_set - return the queue set a packet should use
 *	@m: the packet
 *
 *	Maps a packet to the SGE queue set it should use.  The desired queue
 *	set is carried in bits 1-3 in the packet's priority.
 */
static __inline int
queue_set(const struct mbuf *m)
{
	return m_get_priority(m) >> 1;
}

/**
 *	is_ctrl_pkt - return whether an offload packet is a control packet
 *	@m: the packet
 *
 *	Determines whether an offload packet should use an OFLD or a CTRL
 *	Tx queue.  This is indicated by bit 0 in the packet's priority.
 */
static __inline int
is_ctrl_pkt(const struct mbuf *m)
{
	return m_get_priority(m) & 1;
}

/**
 *	t3_offload_tx - send an offload packet
 *	@tdev: the offload device to send to
 *	@m: the packet
 *
 *	Sends an offload packet.  We use the packet priority to select the
 *	appropriate Tx queue as follows: bit 0 indicates whether the packet
 *	should be sent as regular or control, bits 1-3 select the queue set.
 */
int
t3_offload_tx(struct toedev *tdev, struct mbuf *m)
{
	adapter_t *adap = tdev2adap(tdev);
	struct sge_qset *qs = &adap->sge.qs[queue_set(m)];

	if (__predict_false(is_ctrl_pkt(m)))
		return ctrl_xmit(adap, &qs->txq[TXQ_CTRL], m);

	return ofld_xmit(adap, &qs->txq[TXQ_OFLD], m);
}

/**
 *	deliver_partial_bundle - deliver a (partial) bundle of Rx offload pkts
 *	@tdev: the offload device that will be receiving the packets
 *	@q: the SGE response queue that assembled the bundle
 *	@m: the partial bundle
 *	@n: the number of packets in the bundle
 *
 *	Delivers a (partial) bundle of Rx offload packets to an offload device.
 */
static __inline void
deliver_partial_bundle(struct toedev *tdev,
			struct sge_rspq *q,
			struct mbuf *mbufs[], int n)
{
	if (n) {
		q->offload_bundles++;
		cxgb_ofld_recv(tdev, mbufs, n);
	}
}

static __inline int
rx_offload(struct toedev *tdev, struct sge_rspq *rq,
    struct mbuf *m, struct mbuf *rx_gather[],
    unsigned int gather_idx)
{
	rq->offload_pkts++;
	m->m_pkthdr.header = mtod(m, void *);
	    
	rx_gather[gather_idx++] = m;
	if (gather_idx == RX_BUNDLE_SIZE) {
		cxgb_ofld_recv(tdev, rx_gather, RX_BUNDLE_SIZE);
		gather_idx = 0;
		rq->offload_bundles++;
	}
	return (gather_idx);
}

static void
restart_tx(struct sge_qset *qs)
{
	struct adapter *sc = qs->port->adapter;
	
	if (isset(&qs->txq_stopped, TXQ_OFLD) &&
	    should_restart_tx(&qs->txq[TXQ_OFLD]) &&
	    test_and_clear_bit(TXQ_OFLD, &qs->txq_stopped)) {
		qs->txq[TXQ_OFLD].restarts++;
		taskqueue_enqueue(sc->tq, &qs->txq[TXQ_OFLD].qresume_task);
	}
	if (isset(&qs->txq_stopped, TXQ_CTRL) &&
	    should_restart_tx(&qs->txq[TXQ_CTRL]) &&
	    test_and_clear_bit(TXQ_CTRL, &qs->txq_stopped)) {
		qs->txq[TXQ_CTRL].restarts++;
		taskqueue_enqueue(sc->tq, &qs->txq[TXQ_CTRL].qresume_task);
	}
}

/**
 *	t3_sge_alloc_qset - initialize an SGE queue set
 *	@sc: the controller softc
 *	@id: the queue set id
 *	@nports: how many Ethernet ports will be using this queue set
 *	@irq_vec_idx: the IRQ vector index for response queue interrupts
 *	@p: configuration parameters for this queue set
 *	@ntxq: number of Tx queues for the queue set
 *	@pi: port info for queue set
 *
 *	Allocate resources and initialize an SGE queue set.  A queue set
 *	comprises a response queue, two Rx free-buffer queues, and up to 3
 *	Tx queues.  The Tx queues are assigned roles in the order Ethernet
 *	queue, offload queue, and control queue.
 */
int
t3_sge_alloc_qset(adapter_t *sc, u_int id, int nports, int irq_vec_idx,
		  const struct qset_params *p, int ntxq, struct port_info *pi)
{
	struct sge_qset *q = &sc->sge.qs[id];
	int i, ret = 0;

	init_qset_cntxt(q, id);
	
	if ((ret = alloc_ring(sc, p->fl_size, sizeof(struct rx_desc),
		    sizeof(struct rx_sw_desc), &q->fl[0].phys_addr,
		    &q->fl[0].desc, &q->fl[0].sdesc,
		    &q->fl[0].desc_tag, &q->fl[0].desc_map,
		    sc->rx_dmat, &q->fl[0].entry_tag)) != 0) {
		printf("error %d from alloc ring fl0\n", ret);
		goto err;
	}

	if ((ret = alloc_ring(sc, p->jumbo_size, sizeof(struct rx_desc),
		    sizeof(struct rx_sw_desc), &q->fl[1].phys_addr,
		    &q->fl[1].desc, &q->fl[1].sdesc,
		    &q->fl[1].desc_tag, &q->fl[1].desc_map,
		    sc->rx_jumbo_dmat, &q->fl[1].entry_tag)) != 0) {
		printf("error %d from alloc ring fl1\n", ret);
		goto err;
	}

	if ((ret = alloc_ring(sc, p->rspq_size, sizeof(struct rsp_desc), 0,
		    &q->rspq.phys_addr, &q->rspq.desc, NULL,
		    &q->rspq.desc_tag, &q->rspq.desc_map,
		    NULL, NULL)) != 0) {
		printf("error %d from alloc ring rspq\n", ret);
		goto err;
	}

	for (i = 0; i < ntxq; ++i) {
		/*
		 * The control queue always uses immediate data so does not
		 * need to keep track of any mbufs.
		 * XXX Placeholder for future TOE support.
		 */
		size_t sz = i == TXQ_CTRL ? 0 : sizeof(struct tx_sw_desc);

		if ((ret = alloc_ring(sc, p->txq_size[i],
			    sizeof(struct tx_desc), sz,
			    &q->txq[i].phys_addr, &q->txq[i].desc,
			    &q->txq[i].sdesc, &q->txq[i].desc_tag,
			    &q->txq[i].desc_map,
			    sc->tx_dmat, &q->txq[i].entry_tag)) != 0) {
			printf("error %d from alloc ring tx %i\n", ret, i);
			goto err;
		}
		mbufq_init(&q->txq[i].sendq);
		q->txq[i].gen = 1;
		q->txq[i].size = p->txq_size[i];
		snprintf(q->txq[i].lockbuf, TXQ_NAME_LEN, "t3 txq lock %d:%d:%d",
		    device_get_unit(sc->dev), irq_vec_idx, i);
		MTX_INIT(&q->txq[i].lock, q->txq[i].lockbuf, NULL, MTX_DEF);
	}

	q->txq[TXQ_ETH].port = pi;
	
	TASK_INIT(&q->txq[TXQ_OFLD].qresume_task, 0, restart_offloadq, q);
	TASK_INIT(&q->txq[TXQ_CTRL].qresume_task, 0, restart_ctrlq, q);
	TASK_INIT(&q->txq[TXQ_ETH].qreclaim_task, 0, sge_txq_reclaim_handler, &q->txq[TXQ_ETH]);
	TASK_INIT(&q->txq[TXQ_OFLD].qreclaim_task, 0, sge_txq_reclaim_handler, &q->txq[TXQ_OFLD]);

	

	
	q->fl[0].gen = q->fl[1].gen = 1;
	q->fl[0].size = p->fl_size;
	q->fl[1].size = p->jumbo_size;

	q->rspq.gen = 1;
	q->rspq.cidx = 0;
	q->rspq.size = p->rspq_size;

	q->txq[TXQ_ETH].stop_thres = nports *
	    flits_to_desc(sgl_len(TX_MAX_SEGS + 1) + 3);

	q->fl[0].buf_size = MCLBYTES;
	q->fl[0].zone = zone_clust;
	q->fl[0].type = EXT_CLUSTER;
	q->fl[1].buf_size = MJUMPAGESIZE;
	q->fl[1].zone = zone_jumbop;
	q->fl[1].type = EXT_JUMBOP;
	
	q->lro.enabled = lro_default;
	
	mtx_lock(&sc->sge.reg_lock);
	ret = -t3_sge_init_rspcntxt(sc, q->rspq.cntxt_id, irq_vec_idx,
				   q->rspq.phys_addr, q->rspq.size,
				   q->fl[0].buf_size, 1, 0);
	if (ret) {
		printf("error %d from t3_sge_init_rspcntxt\n", ret);
		goto err_unlock;
	}

	for (i = 0; i < SGE_RXQ_PER_SET; ++i) {
		ret = -t3_sge_init_flcntxt(sc, q->fl[i].cntxt_id, 0,
					  q->fl[i].phys_addr, q->fl[i].size,
					  q->fl[i].buf_size, p->cong_thres, 1,
					  0);
		if (ret) {
			printf("error %d from t3_sge_init_flcntxt for index i=%d\n", ret, i);
			goto err_unlock;
		}
	}

	ret = -t3_sge_init_ecntxt(sc, q->txq[TXQ_ETH].cntxt_id, USE_GTS,
				 SGE_CNTXT_ETH, id, q->txq[TXQ_ETH].phys_addr,
				 q->txq[TXQ_ETH].size, q->txq[TXQ_ETH].token,
				 1, 0);
	if (ret) {
		printf("error %d from t3_sge_init_ecntxt\n", ret);
		goto err_unlock;
	}

	if (ntxq > 1) {
		ret = -t3_sge_init_ecntxt(sc, q->txq[TXQ_OFLD].cntxt_id,
					 USE_GTS, SGE_CNTXT_OFLD, id,
					 q->txq[TXQ_OFLD].phys_addr,
					 q->txq[TXQ_OFLD].size, 0, 1, 0);
		if (ret) {
			printf("error %d from t3_sge_init_ecntxt\n", ret);
			goto err_unlock;
		}
	}

	if (ntxq > 2) {
		ret = -t3_sge_init_ecntxt(sc, q->txq[TXQ_CTRL].cntxt_id, 0,
					 SGE_CNTXT_CTRL, id,
					 q->txq[TXQ_CTRL].phys_addr,
					 q->txq[TXQ_CTRL].size,
					 q->txq[TXQ_CTRL].token, 1, 0);
		if (ret) {
			printf("error %d from t3_sge_init_ecntxt\n", ret);
			goto err_unlock;
		}
	}
	
	snprintf(q->rspq.lockbuf, RSPQ_NAME_LEN, "t3 rspq lock %d:%d",
	    device_get_unit(sc->dev), irq_vec_idx);
	MTX_INIT(&q->rspq.lock, q->rspq.lockbuf, NULL, MTX_DEF);
	
	mtx_unlock(&sc->sge.reg_lock);
	t3_update_qset_coalesce(q, p);
	q->port = pi;
	
	refill_fl(sc, &q->fl[0], q->fl[0].size);
	refill_fl(sc, &q->fl[1], q->fl[1].size);
	refill_rspq(sc, &q->rspq, q->rspq.size - 1);

	t3_write_reg(sc, A_SG_GTS, V_RSPQ(q->rspq.cntxt_id) |
		     V_NEWTIMER(q->rspq.holdoff_tmr));

	return (0);

err_unlock:
	mtx_unlock(&sc->sge.reg_lock);
err:	
	t3_free_qset(sc, q);

	return (ret);
}

void
t3_rx_eth(struct port_info *pi, struct sge_rspq *rq, struct mbuf *m, int ethpad)
{
	struct cpl_rx_pkt *cpl = (struct cpl_rx_pkt *)(mtod(m, uint8_t *) + ethpad);
	struct ifnet *ifp = pi->ifp;
	
	DPRINTF("rx_eth m=%p m->m_data=%p p->iff=%d\n", m, mtod(m, uint8_t *), cpl->iff);
	if (&pi->adapter->port[cpl->iff] != pi)
		panic("bad port index %d m->m_data=%p\n", cpl->iff, mtod(m, uint8_t *));

	if ((ifp->if_capenable & IFCAP_RXCSUM) && !cpl->fragment &&
	    cpl->csum_valid && cpl->csum == 0xffff) {
		m->m_pkthdr.csum_flags = (CSUM_IP_CHECKED|CSUM_IP_VALID);
		rspq_to_qset(rq)->port_stats[SGE_PSTAT_RX_CSUM_GOOD]++;
		m->m_pkthdr.csum_flags = (CSUM_IP_CHECKED|CSUM_IP_VALID|CSUM_DATA_VALID|CSUM_PSEUDO_HDR);
		m->m_pkthdr.csum_data = 0xffff;
	}
	/* 
	 * XXX need to add VLAN support for 6.x
	 */
#ifdef VLAN_SUPPORTED
	if (__predict_false(cpl->vlan_valid)) {
		m->m_pkthdr.ether_vtag = ntohs(cpl->vlan);
		m->m_flags |= M_VLANTAG;
	} 
#endif
	
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.header = mtod(m, uint8_t *) + sizeof(*cpl) + ethpad;
	m_explode(m);
	/*
	 * adjust after conversion to mbuf chain
	 */
	m_adj(m, sizeof(*cpl) + ethpad);

	(*ifp->if_input)(ifp, m);
}

/**
 *	get_packet - return the next ingress packet buffer from a free list
 *	@adap: the adapter that received the packet
 *	@drop_thres: # of remaining buffers before we start dropping packets
 *	@qs: the qset that the SGE free list holding the packet belongs to
 *      @mh: the mbuf header, contains a pointer to the head and tail of the mbuf chain
 *      @r: response descriptor 
 *
 *	Get the next packet from a free list and complete setup of the
 *	sk_buff.  If the packet is small we make a copy and recycle the
 *	original buffer, otherwise we use the original buffer itself.  If a
 *	positive drop threshold is supplied packets are dropped and their
 *	buffers recycled if (a) the number of remaining buffers is under the
 *	threshold and the packet is too big to copy, or (b) the packet should
 *	be copied but there is no memory for the copy.
 */
static int
get_packet(adapter_t *adap, unsigned int drop_thres, struct sge_qset *qs,
    struct mbuf *m, struct rsp_desc *r)
{
	
	unsigned int len_cq =  ntohl(r->len_cq);
	struct sge_fl *fl = (len_cq & F_RSPD_FLQ) ? &qs->fl[1] : &qs->fl[0];
	struct rx_sw_desc *sd = &fl->sdesc[fl->cidx];
	uint32_t len = G_RSPD_LEN(len_cq);
	uint32_t flags = ntohl(r->flags);
	uint8_t sopeop = G_RSPD_SOP_EOP(flags);
	void *cl;
	int ret = 0;
	
	prefetch(sd->cl);

	fl->credits--;
	bus_dmamap_sync(fl->entry_tag, sd->map, BUS_DMASYNC_POSTREAD);

	if (recycle_enable && len <= SGE_RX_COPY_THRES && sopeop == RSPQ_SOP_EOP) {
		cl = mtod(m, void *);
		memcpy(cl, sd->cl, len);
		recycle_rx_buf(adap, fl, fl->cidx);
	} else {
		cl = sd->cl;
		bus_dmamap_unload(fl->entry_tag, sd->map);
	}
	switch(sopeop) {
	case RSPQ_SOP_EOP:
		DBG(DBG_RX, ("get_packet: SOP-EOP m %p\n", m));
		if (cl == sd->cl)
			m_cljset(m, cl, fl->type);
		m->m_len = m->m_pkthdr.len = len;
		ret = 1;
		goto done;
		break;
	case RSPQ_NSOP_NEOP:
		DBG(DBG_RX, ("get_packet: NO_SOP-NO_EOP m %p\n", m));
		ret = 0;
		break;
	case RSPQ_SOP:
		DBG(DBG_RX, ("get_packet: SOP m %p\n", m));
		m_iovinit(m);
		ret = 0;
		break;
	case RSPQ_EOP:
		DBG(DBG_RX, ("get_packet: EOP m %p\n", m));
		ret = 1;
		break;
	}
	m_iovappend(m, cl, fl->buf_size, len, 0);

done:	
	if (++fl->cidx == fl->size)
		fl->cidx = 0;

	return (ret);
}

/**
 *	handle_rsp_cntrl_info - handles control information in a response
 *	@qs: the queue set corresponding to the response
 *	@flags: the response control flags
 *
 *	Handles the control information of an SGE response, such as GTS
 *	indications and completion credits for the queue set's Tx queues.
 *	HW coalesces credits, we don't do any extra SW coalescing.
 */
static __inline void
handle_rsp_cntrl_info(struct sge_qset *qs, uint32_t flags)
{
	unsigned int credits;

#if USE_GTS
	if (flags & F_RSPD_TXQ0_GTS)
		clear_bit(TXQ_RUNNING, &qs->txq[TXQ_ETH].flags);
#endif
	credits = G_RSPD_TXQ0_CR(flags);
	if (credits) {
		qs->txq[TXQ_ETH].processed += credits;
		if (desc_reclaimable(&qs->txq[TXQ_ETH]) > TX_START_MAX_DESC)
			taskqueue_enqueue(qs->port->adapter->tq,
			    &qs->port->timer_reclaim_task);
	}
	
	credits = G_RSPD_TXQ2_CR(flags);
	if (credits) 
		qs->txq[TXQ_CTRL].processed += credits;

# if USE_GTS
	if (flags & F_RSPD_TXQ1_GTS)
		clear_bit(TXQ_RUNNING, &qs->txq[TXQ_OFLD].flags);
# endif
	credits = G_RSPD_TXQ1_CR(flags);
	if (credits)
		qs->txq[TXQ_OFLD].processed += credits;
}

static void
check_ring_db(adapter_t *adap, struct sge_qset *qs,
    unsigned int sleeping)
{
	;
}

/**
 *	process_responses - process responses from an SGE response queue
 *	@adap: the adapter
 *	@qs: the queue set to which the response queue belongs
 *	@budget: how many responses can be processed in this round
 *
 *	Process responses from an SGE response queue up to the supplied budget.
 *	Responses include received packets as well as credits and other events
 *	for the queues that belong to the response queue's queue set.
 *	A negative budget is effectively unlimited.
 *
 *	Additionally choose the interrupt holdoff time for the next interrupt
 *	on this queue.  If the system is under memory shortage use a fairly
 *	long delay to help recovery.
 */
static int
process_responses(adapter_t *adap, struct sge_qset *qs, int budget)
{
	struct sge_rspq *rspq = &qs->rspq;
	struct rsp_desc *r = &rspq->desc[rspq->cidx];
	int budget_left = budget;
	unsigned int sleeping = 0;
	int lro = qs->lro.enabled;
	struct mbuf *offload_mbufs[RX_BUNDLE_SIZE];
	int ngathered = 0;
#ifdef DEBUG	
	static int last_holdoff = 0;
	if (cxgb_debug && rspq->holdoff_tmr != last_holdoff) {
		printf("next_holdoff=%d\n", rspq->holdoff_tmr);
		last_holdoff = rspq->holdoff_tmr;
	}
#endif
	rspq->next_holdoff = rspq->holdoff_tmr;

	while (__predict_true(budget_left && is_new_response(r, rspq))) {
		int eth, eop = 0, ethpad = 0;
		uint32_t flags = ntohl(r->flags);
		uint32_t rss_csum = *(const uint32_t *)r;
		uint32_t rss_hash = r->rss_hdr.rss_hash_val;
		
		eth = (r->rss_hdr.opcode == CPL_RX_PKT);
		
		if (__predict_false(flags & F_RSPD_ASYNC_NOTIF)) {
			/* XXX */
			printf("async notification\n");

		} else if  (flags & F_RSPD_IMM_DATA_VALID) {
			struct mbuf *m = NULL;

			if (cxgb_debug)
				printf("IMM DATA VALID opcode=0x%x rspq->cidx=%d\n", r->rss_hdr.opcode, rspq->cidx);
			if (rspq->m == NULL)
				rspq->m = m_gethdr(M_DONTWAIT, MT_DATA);
                        else
				m = m_gethdr(M_DONTWAIT, MT_DATA);

			/*
			 * XXX revisit me
			 */
			if (rspq->m == NULL &&  m == NULL) {
				rspq->next_holdoff = NOMEM_INTR_DELAY;
				budget_left--;
				break;
			}
			if (get_imm_packet(adap, r, rspq->m, m, flags))
				goto skip;
			eop = 1;
			rspq->imm_data++;
		} else if (r->len_cq) {			
			int drop_thresh = eth ? SGE_RX_DROP_THRES : 0;

                        if (rspq->m == NULL)  
				rspq->m = m_gethdr(M_DONTWAIT, MT_DATA);
			if (rspq->m == NULL) { 
				log(LOG_WARNING, "failed to get mbuf for packet\n"); 
				break; 
			}

			ethpad = 2;
			eop = get_packet(adap, drop_thresh, qs, rspq->m, r);
		} else {
			DPRINTF("pure response\n");
			rspq->pure_rsps++;
		}

		if (flags & RSPD_CTRL_MASK) {
			sleeping |= flags & RSPD_GTS_MASK;
			handle_rsp_cntrl_info(qs, flags);
		}
	skip:
		r++;
		if (__predict_false(++rspq->cidx == rspq->size)) {
			rspq->cidx = 0;
			rspq->gen ^= 1;
			r = rspq->desc;
		}
		
		prefetch(r);
		if (++rspq->credits >= (rspq->size / 4)) {
			refill_rspq(adap, rspq, rspq->credits);
			rspq->credits = 0;
		}
		
		if (eop) {
			prefetch(mtod(rspq->m, uint8_t *)); 
			prefetch(mtod(rspq->m, uint8_t *) + L1_CACHE_BYTES); 

			if (eth) {				
				t3_rx_eth_lro(adap, rspq, rspq->m, ethpad,
				    rss_hash, rss_csum, lro);

				rspq->m = NULL;
			} else {
				rspq->m->m_pkthdr.csum_data = rss_csum;
				/*
				 * XXX size mismatch
				 */
				m_set_priority(rspq->m, rss_hash);
				
				ngathered = rx_offload(&adap->tdev, rspq, rspq->m,
				    offload_mbufs, ngathered);
			}
			__refill_fl(adap, &qs->fl[0]);
			__refill_fl(adap, &qs->fl[1]);

		}
		--budget_left;
	}

	deliver_partial_bundle(&adap->tdev, rspq, offload_mbufs, ngathered);
	t3_lro_flush(adap, qs, &qs->lro);
	
	if (sleeping)
		check_ring_db(adap, qs, sleeping);

	smp_mb();  /* commit Tx queue processed updates */
	if (__predict_false(qs->txq_stopped != 0))
		restart_tx(qs);

	budget -= budget_left;
	return (budget);
}

/*
 * A helper function that processes responses and issues GTS.
 */
static __inline int
process_responses_gts(adapter_t *adap, struct sge_rspq *rq)
{
	int work;
	static int last_holdoff = 0;
	
	work = process_responses(adap, rspq_to_qset(rq), -1);

	if (cxgb_debug && (rq->next_holdoff != last_holdoff)) {
		printf("next_holdoff=%d\n", rq->next_holdoff);
		last_holdoff = rq->next_holdoff;
	}

	t3_write_reg(adap, A_SG_GTS, V_RSPQ(rq->cntxt_id) |
		     V_NEWTIMER(rq->next_holdoff) | V_NEWINDEX(rq->cidx));
	return work;
}


/*
 * Interrupt handler for legacy INTx interrupts for T3B-based cards.
 * Handles data events from SGE response queues as well as error and other
 * async events as they all use the same interrupt pin.  We use one SGE
 * response queue per port in this mode and protect all response queues with
 * queue 0's lock.
 */
void
t3b_intr(void *data)
{
	uint32_t map;
	adapter_t *adap = data;
	struct sge_rspq *q0 = &adap->sge.qs[0].rspq;
	struct sge_rspq *q1 = &adap->sge.qs[1].rspq;
	
	t3_write_reg(adap, A_PL_CLI, 0);
	map = t3_read_reg(adap, A_SG_DATA_INTR);

	if (!map) 
		return;

	if (__predict_false(map & F_ERRINTR))
		taskqueue_enqueue(adap->tq, &adap->slow_intr_task);
	
	mtx_lock(&q0->lock);
	
	if (__predict_true(map & 1))
		process_responses_gts(adap, q0);
	
	if (map & 2)
		process_responses_gts(adap, q1);

	mtx_unlock(&q0->lock);
}

/*
 * The MSI interrupt handler.  This needs to handle data events from SGE
 * response queues as well as error and other async events as they all use
 * the same MSI vector.  We use one SGE response queue per port in this mode
 * and protect all response queues with queue 0's lock.
 */
void
t3_intr_msi(void *data)
{
	adapter_t *adap = data;
	struct sge_rspq *q0 = &adap->sge.qs[0].rspq;
	struct sge_rspq *q1 = &adap->sge.qs[1].rspq;
	int new_packets = 0;

	mtx_lock(&q0->lock);
	if (process_responses_gts(adap, q0)) {
		new_packets = 1;
	}

	if (adap->params.nports == 2 &&
	    process_responses_gts(adap, q1)) {
		new_packets = 1;
	}
	
	mtx_unlock(&q0->lock);
	if (new_packets == 0)
		taskqueue_enqueue(adap->tq, &adap->slow_intr_task);
}

void
t3_intr_msix(void *data)
{
	struct sge_qset *qs = data;
	adapter_t *adap = qs->port->adapter;
	struct sge_rspq *rspq = &qs->rspq;

	mtx_lock(&rspq->lock);
	if (process_responses_gts(adap, rspq) == 0)
		rspq->unhandled_irqs++;
	mtx_unlock(&rspq->lock);
}

/* 
 * broken by recent mbuf changes 
 */ 
static int
t3_lro_enable(SYSCTL_HANDLER_ARGS)
{
	adapter_t *sc;
	int i, j, enabled, err, nqsets = 0;

#ifndef LRO_WORKING
	return (0);
#endif	
	
	sc = arg1;
	enabled = sc->sge.qs[0].lro.enabled;
        err = sysctl_handle_int(oidp, &enabled, arg2, req);

	if (err != 0) 
		return (err);
	if (enabled == sc->sge.qs[0].lro.enabled)
		return (0);

	for (i = 0; i < sc->params.nports; i++) 
		for (j = 0; j < sc->port[i].nqsets; j++)
			nqsets++;
	
	for (i = 0; i < nqsets; i++) 
		sc->sge.qs[i].lro.enabled = enabled;
	
	return (0);
}

static int
t3_set_coalesce_nsecs(SYSCTL_HANDLER_ARGS)
{
	adapter_t *sc = arg1;
	struct qset_params *qsp = &sc->params.sge.qset[0]; 
	int coalesce_nsecs;	
	struct sge_qset *qs;
	int i, j, err, nqsets = 0;
	struct mtx *lock;
	
	coalesce_nsecs = qsp->coalesce_nsecs;
        err = sysctl_handle_int(oidp, &coalesce_nsecs, arg2, req);

	if (err != 0) {
		return (err);
	}
	if (coalesce_nsecs == qsp->coalesce_nsecs)
		return (0);

	for (i = 0; i < sc->params.nports; i++) 
		for (j = 0; j < sc->port[i].nqsets; j++)
			nqsets++;

	coalesce_nsecs = max(100, coalesce_nsecs);

	for (i = 0; i < nqsets; i++) {
		qs = &sc->sge.qs[i];
		qsp = &sc->params.sge.qset[i];
		qsp->coalesce_nsecs = coalesce_nsecs;
		
		lock = (sc->flags & USING_MSIX) ? &qs->rspq.lock :
			    &sc->sge.qs[0].rspq.lock;

		mtx_lock(lock);
		t3_update_qset_coalesce(qs, qsp);
		t3_write_reg(sc, A_SG_GTS, V_RSPQ(qs->rspq.cntxt_id) |
		    V_NEWTIMER(qs->rspq.holdoff_tmr));
		mtx_unlock(lock);
	}

	return (0);
}


void
t3_add_sysctls(adapter_t *sc)
{
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid_list *children;
	
	ctx = device_get_sysctl_ctx(sc->dev);
	children = SYSCTL_CHILDREN(device_get_sysctl_tree(sc->dev));

	/* random information */
	SYSCTL_ADD_STRING(ctx, children, OID_AUTO, 
	    "firmware_version",
	    CTLFLAG_RD, &sc->fw_version,
	    0, "firmware version");

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, 
	    "enable_lro",
	    CTLTYPE_INT|CTLFLAG_RW, sc,
	    0, t3_lro_enable,
	    "I", "enable large receive offload");

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, 
	    "intr_coal",
	    CTLTYPE_INT|CTLFLAG_RW, sc,
	    0, t3_set_coalesce_nsecs,
	    "I", "interrupt coalescing timer (ns)");
	SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
	    "enable_debug",
	    CTLFLAG_RW, &cxgb_debug,
	    0, "enable verbose debugging output");

	SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
	    "collapse_free",
	    CTLFLAG_RD, &collapse_free,
	    0, "frees during collapse");
	SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
	    "mb_free_vec_free",
	    CTLFLAG_RD, &mb_free_vec_free,
	    0, "frees during mb_free_vec");
	SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
	    "collapse_mbufs",
	    CTLFLAG_RW, &collapse_mbufs,
	    0, "collapse mbuf chains into iovecs");
	SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
	    "txq_overrun",
	    CTLFLAG_RD, &txq_fills,
	    0, "#times txq overrun");
	SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
	    "bogus_imm",
	    CTLFLAG_RD, &bogus_imm,
	    0, "#times a bogus immediate response was seen");	
}

/**
 *	t3_get_desc - dump an SGE descriptor for debugging purposes
 *	@qs: the queue set
 *	@qnum: identifies the specific queue (0..2: Tx, 3:response, 4..5: Rx)
 *	@idx: the descriptor index in the queue
 *	@data: where to dump the descriptor contents
 *
 *	Dumps the contents of a HW descriptor of an SGE queue.  Returns the
 *	size of the descriptor.
 */
int
t3_get_desc(const struct sge_qset *qs, unsigned int qnum, unsigned int idx,
		unsigned char *data)
{
	if (qnum >= 6)
		return (EINVAL);

	if (qnum < 3) {
		if (!qs->txq[qnum].desc || idx >= qs->txq[qnum].size)
			return -EINVAL;
		memcpy(data, &qs->txq[qnum].desc[idx], sizeof(struct tx_desc));
		return sizeof(struct tx_desc);
	}

	if (qnum == 3) {
		if (!qs->rspq.desc || idx >= qs->rspq.size)
			return (EINVAL);
		memcpy(data, &qs->rspq.desc[idx], sizeof(struct rsp_desc));
		return sizeof(struct rsp_desc);
	}

	qnum -= 4;
	if (!qs->fl[qnum].desc || idx >= qs->fl[qnum].size)
		return (EINVAL);
	memcpy(data, &qs->fl[qnum].desc[idx], sizeof(struct rx_desc));
	return sizeof(struct rx_desc);
}
