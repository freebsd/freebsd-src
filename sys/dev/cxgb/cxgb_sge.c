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
#define DEBUG_BUFRING


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
#include <sys/sbuf.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/systm.h>
#include <sys/syslog.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#ifdef CONFIG_DEFINED
#include <cxgb_include.h>
#include <sys/mvec.h>
#else
#include <dev/cxgb/cxgb_include.h>
#include <dev/cxgb/sys/mvec.h>
#endif

int      txq_fills = 0;
/*
 * XXX don't re-enable this until TOE stops assuming
 * we have an m_ext
 */
static int recycle_enable = 0;
extern int cxgb_txq_buf_ring_size;
int cxgb_cached_allocations;
int cxgb_cached;
int cxgb_ext_freed = 0;
int cxgb_ext_inited = 0;
int fl_q_size = 0;
int jumbo_q_size = 0;

extern int cxgb_use_16k_clusters;
extern int cxgb_pcpu_cache_enable;
extern int nmbjumbo4;
extern int nmbjumbo9;
extern int nmbjumbo16;




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
	struct mbuf_iovec mi;
	bus_dmamap_t	map;
	int		flags;
};

struct rx_sw_desc {                /* SW state per Rx descriptor */
	caddr_t	         rxsd_cl;
	caddr_t	         data;
	bus_dmamap_t	  map;
	int		  flags;
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


int cxgb_debug = 0;

static void sge_timer_cb(void *arg);
static void sge_timer_reclaim(void *arg, int ncount);
static void sge_txq_reclaim_handler(void *arg, int ncount);

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
reclaim_completed_tx_(struct sge_txq *q, int reclaim_min)
{
	int reclaim = desc_reclaimable(q);

	if (reclaim < reclaim_min)
		return (0);
	
	mtx_assert(&q->lock, MA_OWNED);
	if (reclaim > 0) {
		t3_free_tx_desc(q, reclaim);
		q->cleaned += reclaim;
		q->in_use -= reclaim;
	} 
	return (reclaim);
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
	       F_CQCRDTCTRL | F_CONGMODE | F_TNLFLMODE | F_FATLPERREN |
	       V_HOSTPAGESIZE(PAGE_SHIFT - 11) | F_BIGENDIANINGRESS |
	       V_USERSPACESIZE(ups ? ups - 1 : 0) | F_ISCSICOALESCING;
#if SGE_NUM_GENBITS == 1
	ctrl |= F_EGRGENCTRL;
#endif
	if (adap->params.rev > 0) {
		if (!(adap->flags & (USING_MSIX | USING_MSI)))
			ctrl |= F_ONEINTMULTQ | F_OPTONEINTMULTQ;
	}
	t3_write_reg(adap, A_SG_CONTROL, ctrl);
	t3_write_reg(adap, A_SG_EGR_RCQ_DRB_THRSH, V_HIRCQDRBTHRSH(512) |
		     V_LORCQDRBTHRSH(512));
	t3_write_reg(adap, A_SG_TIMER_TICK, core_ticks_per_usec(adap) / 10);
	t3_write_reg(adap, A_SG_CMDQ_CREDIT_TH, V_THRESHOLD(32) |
		     V_TIMEOUT(200 * core_ticks_per_usec(adap)));
	t3_write_reg(adap, A_SG_HI_DRB_HI_THRSH,
		     adap->params.rev < T3_REV_C ? 1000 : 500);
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
get_imm_packet(adapter_t *sc, const struct rsp_desc *resp, struct mbuf *m)
{

	m->m_len = m->m_pkthdr.len = IMMED_PKT_SIZE;
	m->m_ext.ext_buf = NULL;
	m->m_ext.ext_type = 0;
	memcpy(mtod(m, uint8_t *), resp->imm_data, IMMED_PKT_SIZE); 
	return (0);	
}

static __inline u_int
flits_to_desc(u_int n)
{
	return (flit_desc_map[n]);
}

#define SGE_PARERR (F_CPPARITYERROR | F_OCPARITYERROR | F_RCPARITYERROR | \
		    F_IRPARITYERROR | V_ITPARITYERROR(M_ITPARITYERROR) | \
		    V_FLPARITYERROR(M_FLPARITYERROR) | F_LODRBPARITYERROR | \
		    F_HIDRBPARITYERROR | F_LORCQPARITYERROR | \
		    F_HIRCQPARITYERROR)
#define SGE_FRAMINGERR (F_UC_REQ_FRAMINGERROR | F_R_REQ_FRAMINGERROR)
#define SGE_FATALERR (SGE_PARERR | SGE_FRAMINGERR | F_RSPQCREDITOVERFOW | \
		      F_RSPQDISABLED)

/**
 *	t3_sge_err_intr_handler - SGE async event interrupt handler
 *	@adapter: the adapter
 *
 *	Interrupt handler for SGE asynchronous (non-data) events.
 */
void
t3_sge_err_intr_handler(adapter_t *adapter)
{
	unsigned int v, status;

	status = t3_read_reg(adapter, A_SG_INT_CAUSE);
	if (status & SGE_PARERR)
		CH_ALERT(adapter, "SGE parity error (0x%x)\n",
			 status & SGE_PARERR);
	if (status & SGE_FRAMINGERR)
		CH_ALERT(adapter, "SGE framing error (0x%x)\n",
			 status & SGE_FRAMINGERR);
	if (status & F_RSPQCREDITOVERFOW)
		CH_ALERT(adapter, "SGE response queue credit overflow\n");

	if (status & F_RSPQDISABLED) {
		v = t3_read_reg(adapter, A_SG_RSPQ_FL_STATUS);

		CH_ALERT(adapter,
			 "packet delivered to disabled response queue (0x%x)\n",
			 (v >> S_RSPQ0DISABLED) & 0xff);
	}

	t3_write_reg(adapter, A_SG_INT_CAUSE, status);
	if (status & SGE_FATALERR)
		t3_fatal_err(adapter);
}

void
t3_sge_prep(adapter_t *adap, struct sge_params *p)
{
	int i, nqsets;

	nqsets = min(SGE_QSETS, mp_ncpus*4);

	fl_q_size = min(nmbclusters/(3*nqsets), FL_Q_SIZE);

	while (!powerof2(fl_q_size))
		fl_q_size--;
#if __FreeBSD_version > 800000
	if (cxgb_use_16k_clusters) 
		jumbo_q_size = min(nmbjumbo16/(3*nqsets), JUMBO_Q_SIZE);
	else
		jumbo_q_size = min(nmbjumbo9/(3*nqsets), JUMBO_Q_SIZE);
#else
	jumbo_q_size = min(nmbjumbo4/(3*nqsets), JUMBO_Q_SIZE);
#endif
	while (!powerof2(jumbo_q_size))
		jumbo_q_size--;		
	
	/* XXX Does ETHER_ALIGN need to be accounted for here? */
	p->max_pkt_size = adap->sge.qs[0].fl[1].buf_size - sizeof(struct cpl_rx_data);

	for (i = 0; i < SGE_QSETS; ++i) {
		struct qset_params *q = p->qset + i;

		if (adap->params.nports > 2) {
			q->coalesce_usecs = 50;
		} else {
#ifdef INVARIANTS			
			q->coalesce_usecs = 10;
#else
			q->coalesce_usecs = 5;
#endif			
		}
		q->polling = adap->params.rev > 0;
		q->rspq_size = RSPQ_Q_SIZE;
		q->fl_size = fl_q_size;
		q->jumbo_size = jumbo_q_size;
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
	if (bus_dma_tag_create(sc->parent_dmat, MJUM16BYTES, 0, BUS_SPACE_MAXADDR,
		BUS_SPACE_MAXADDR, NULL, NULL, MJUM16BYTES, 1, MJUM16BYTES,
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

	qs->rspq.holdoff_tmr = max(p->coalesce_usecs * 10, 1U);
	qs->rspq.polling = 0 /* p->polling */;
}

#if !defined(__i386__) && !defined(__amd64__)
static void
refill_fl_cb(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	struct refill_fl_cb_arg *cb_arg = arg;
	
	cb_arg->error = error;
	cb_arg->seg = segs[0];
	cb_arg->nseg = nseg;

}
#endif
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
	caddr_t cl;
	int err, count = 0;
	int header_size = sizeof(struct m_hdr) + sizeof(struct pkthdr) + sizeof(struct m_ext_) + sizeof(uint32_t);
	
	cb_arg.error = 0;
	while (n--) {
		/*
		 * We only allocate a cluster, mbuf allocation happens after rx
		 */
		if ((cl = cxgb_cache_get(q->zone)) == NULL) {
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
#if !defined(__i386__) && !defined(__amd64__)
		err = bus_dmamap_load(q->entry_tag, sd->map,
		    cl + header_size, q->buf_size,
		    refill_fl_cb, &cb_arg, 0);
		
		if (err != 0 || cb_arg.error) {
			log(LOG_WARNING, "failure in refill_fl %d\n", cb_arg.error);
			/*
			 * XXX free cluster
			 */
			return;
		}
#else
		cb_arg.seg.ds_addr = pmap_kextract((vm_offset_t)(cl + header_size));
#endif		
		sd->flags |= RX_SW_DESC_INUSE;
		sd->rxsd_cl = cl;
		sd->data = cl + header_size;
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
		count++;
	}

done:
	if (count)
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
			uma_zfree(q->zone, d->rxsd_cl);
		}
		d->rxsd_cl = NULL;
		if (++cidx == q->size)
			cidx = 0;
	}
}

static __inline void
__refill_fl(adapter_t *adap, struct sge_fl *fl)
{
	refill_fl(adap, fl, min(16U, fl->size - fl->credits));
}

static __inline void
__refill_fl_lt(adapter_t *adap, struct sge_fl *fl, int max)
{
	if ((fl->size - fl->credits) < max)
		refill_fl(adap, fl, min(max, fl->size - fl->credits));
}

void
refill_fl_service(adapter_t *adap, struct sge_fl *fl)
{
	__refill_fl_lt(adap, fl, 512);
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
		s = malloc(len, M_DEVBUF, M_WAITOK|M_ZERO);
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
#ifndef IFNET_MULTIQUEUE	
	struct port_info *pi;
	struct sge_qset *qs;
	struct sge_txq  *txq;
	int i, j;
	int reclaim_ofl, refill_rx;

	for (i = 0; i < sc->params.nports; i++) 
		for (j = 0; j < sc->port[i].nqsets; j++) {
			qs = &sc->sge.qs[i + j];
			txq = &qs->txq[0];
			reclaim_ofl = txq[TXQ_OFLD].processed - txq[TXQ_OFLD].cleaned;
			refill_rx = ((qs->fl[0].credits < qs->fl[0].size) || 
			    (qs->fl[1].credits < qs->fl[1].size));
			if (reclaim_ofl || refill_rx) {
				pi = &sc->port[i];
				taskqueue_enqueue(pi->tq, &pi->timer_reclaim_task);
				break;
			}
		}
#endif
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
	mi_init();
	cxgb_cache_init();
	return (0);
}

int
t3_sge_reset_adapter(adapter_t *sc)
{
	callout_reset(&sc->sge_timer_ch, TX_RECLAIM_PERIOD, sge_timer_cb, sc);
	return (0);
}

int
t3_sge_init_port(struct port_info *pi)
{
	TASK_INIT(&pi->timer_reclaim_task, 0, sge_timer_reclaim, pi);
	return (0);
}

void
t3_sge_deinit_sw(adapter_t *sc)
{

	mi_deinit();
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
sge_txq_reclaim_(struct sge_txq *txq, int force)
{

	if (desc_reclaimable(txq) < 16)
		return;
	if (mtx_trylock(&txq->lock) == 0) 
		return;
	reclaim_completed_tx_(txq, 16);
	mtx_unlock(&txq->lock);

}

static void
sge_txq_reclaim_handler(void *arg, int ncount)
{
	struct sge_txq *q = arg;

	sge_txq_reclaim_(q, TRUE);
}



static void
sge_timer_reclaim(void *arg, int ncount)
{
	struct port_info *pi = arg;
	int i, nqsets = pi->nqsets;
	adapter_t *sc = pi->adapter;
	struct sge_qset *qs;
	struct sge_txq *txq;
	struct mtx *lock;

#ifdef IFNET_MULTIQUEUE
	panic("%s should not be called with multiqueue support\n", __FUNCTION__);
#endif 
	for (i = 0; i < nqsets; i++) {
		qs = &sc->sge.qs[i];

		txq = &qs->txq[TXQ_OFLD];
		sge_txq_reclaim_(txq, FALSE);
		
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

	mbufq_init(&qs->txq[TXQ_ETH].sendq);
	mbufq_init(&qs->txq[TXQ_OFLD].sendq);
	mbufq_init(&qs->txq[TXQ_CTRL].sendq);
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
	txqs->compl = (txq->unacked & 32) << (S_WR_COMPL - 5);
	txq->unacked &= 31;
	txqs->pidx = txq->pidx;
	txq->pidx += ndesc;
#ifdef INVARIANTS
	if (((txqs->pidx > txq->cidx) &&
		(txq->pidx < txqs->pidx) &&
		(txq->pidx >= txq->cidx)) ||
	    ((txqs->pidx < txq->cidx) &&
		(txq->pidx >= txq-> cidx)) ||
	    ((txqs->pidx < txq->cidx) &&
		(txq->cidx < txqs->pidx)))
		panic("txqs->pidx=%d txq->pidx=%d txq->cidx=%d",
		    txqs->pidx, txq->pidx, txq->cidx);
#endif
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
	if (m->m_pkthdr.csum_flags & CSUM_TSO)
		flits++;
#endif	
	return flits_to_desc(flits);
}

static unsigned int
busdma_map_mbufs(struct mbuf **m, struct sge_txq *txq,
    struct tx_sw_desc *txsd, bus_dma_segment_t *segs, int *nsegs)
{
	struct mbuf *m0;
	int err, pktlen, pass = 0;
	
retry:
	err = 0;
	m0 = *m;
	pktlen = m0->m_pkthdr.len;
#if defined(__i386__) || defined(__amd64__)
	if (busdma_map_sg_collapse(m, segs, nsegs) == 0) {
		goto done;
	} else
#endif
		err = bus_dmamap_load_mbuf_sg(txq->entry_tag, txsd->map, m0, segs, nsegs, 0);

	if (err == 0) {
		goto done;
	}
	if (err == EFBIG && pass == 0) {
		pass = 1;
		/* Too many segments, try to defrag */
		m0 = m_defrag(m0, M_DONTWAIT);
		if (m0 == NULL) {
			m_freem(*m);
			*m = NULL;
			return (ENOBUFS);
		}
		*m = m0;
		goto retry;
	} else if (err == ENOMEM) {
		return (err);
	} if (err) {
		if (cxgb_debug)
			printf("map failure err=%d pktlen=%d\n", err, pktlen);
		m_freem(m0);
		*m = NULL;
		return (err);
	}
done:
#if !defined(__i386__) && !defined(__amd64__)
	bus_dmamap_sync(txq->entry_tag, txsd->map, BUS_DMASYNC_PREWRITE);
#endif	
	txsd->flags |= TX_SW_DESC_MAPPED;

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
	
	for (idx = 0, i = 0; i < nsegs; i++) {
		/*
		 * firmware doesn't like empty segments
		 */
		if (segs[i].ds_len == 0)
			continue;
		if (i && idx == 0) 
			++sgp;
		
		sgp->len[idx] = htobe32(segs[i].ds_len);
		sgp->addr[idx] = htobe64(segs[i].ds_addr);
		idx ^= 1;
	}
	
	if (idx) {
		sgp->len[idx] = 0;
		sgp->addr[idx] = 0;
	}
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
			KASSERT(txsd->mi.mi_base == NULL,
			    ("overwriting valid entry mi_base==%p", txsd->mi.mi_base));
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

#ifdef VLAN_SUPPORTED
#define GET_VTAG(cntrl, m) \
do { \
	if ((m)->m_flags & M_VLANTAG)					            \
		cntrl |= F_TXPKT_VLAN_VLD | V_TXPKT_VLAN((m)->m_pkthdr.ether_vtag); \
} while (0)

#define GET_VTAG_MI(cntrl, mi) \
do { \
	if ((mi)->mi_flags & M_VLANTAG)					\
		cntrl |= F_TXPKT_VLAN_VLD | V_TXPKT_VLAN((mi)->mi_ether_vtag); \
} while (0)
#else
#define GET_VTAG(cntrl, m)
#define GET_VTAG_MI(cntrl, m)
#endif

int
t3_encap(struct sge_qset *qs, struct mbuf **m, int count)
{
	adapter_t *sc;
	struct mbuf *m0;
	struct sge_txq *txq;
	struct txq_state txqs;
	struct port_info *pi;
	unsigned int ndesc, flits, cntrl, mlen;
	int err, nsegs, tso_info = 0;

	struct work_request_hdr *wrp;
	struct tx_sw_desc *txsd;
	struct sg_ent *sgp, *sgl;
	uint32_t wr_hi, wr_lo, sgl_flits; 
	bus_dma_segment_t segs[TX_MAX_SEGS];

	struct tx_desc *txd;
	struct mbuf_vec *mv;
	struct mbuf_iovec *mi;
		
	DPRINTF("t3_encap cpu=%d ", curcpu);

	mi = NULL;
	pi = qs->port;
	sc = pi->adapter;
	txq = &qs->txq[TXQ_ETH];
	txd = &txq->desc[txq->pidx];
	txsd = &txq->sdesc[txq->pidx];
	sgl = txq->txq_sgl;
	m0 = *m;
	
	DPRINTF("t3_encap port_id=%d qsidx=%d ", pi->port_id, pi->first_qset);
	DPRINTF("mlen=%d txpkt_intf=%d tx_chan=%d\n", m[0]->m_pkthdr.len, pi->txpkt_intf, pi->tx_chan);
	if (cxgb_debug)
		printf("mi_base=%p cidx=%d pidx=%d\n\n", txsd->mi.mi_base, txq->cidx, txq->pidx);
	
	mtx_assert(&txq->lock, MA_OWNED);
	cntrl = V_TXPKT_INTF(pi->txpkt_intf);
/*
 * XXX need to add VLAN support for 6.x
 */
#ifdef VLAN_SUPPORTED
	if  (m0->m_pkthdr.csum_flags & (CSUM_TSO))
		tso_info = V_LSO_MSS(m0->m_pkthdr.tso_segsz);
#endif
	KASSERT(txsd->mi.mi_base == NULL,
	    ("overwriting valid entry mi_base==%p", txsd->mi.mi_base));
	if (count > 1) {
		panic("count > 1 not support in CVS\n");
		if ((err = busdma_map_sg_vec(m, &m0, segs, count)))
			return (err);
		nsegs = count;
	} else if ((err = busdma_map_sg_collapse(&m0, segs, &nsegs))) {
		if (cxgb_debug)
			printf("failed ... err=%d\n", err);
		return (err);
	} 
	KASSERT(m0->m_pkthdr.len, ("empty packet nsegs=%d count=%d", nsegs, count));

	if (!(m0->m_pkthdr.len <= PIO_LEN)) {
		mi_collapse_mbuf(&txsd->mi, m0);
		mi = &txsd->mi;
	}
	if (count > 1) {
		struct cpl_tx_pkt_batch *cpl_batch = (struct cpl_tx_pkt_batch *)txd;
		int i, fidx;
		struct mbuf_iovec *batchmi;

		mv = mtomv(m0);
		batchmi = mv->mv_vec;
		
		wrp = (struct work_request_hdr *)txd;

		flits = count*2 + 1;
		txq_prod(txq, 1, &txqs);

		for (fidx = 1, i = 0; i < count; i++, batchmi++, fidx += 2) {
			struct cpl_tx_pkt_batch_entry *cbe = &cpl_batch->pkt_entry[i];

			cntrl = V_TXPKT_INTF(pi->txpkt_intf);
			GET_VTAG_MI(cntrl, batchmi);
			cntrl |= V_TXPKT_OPCODE(CPL_TX_PKT);
			if (__predict_false(!(m0->m_pkthdr.csum_flags & CSUM_IP)))
				cntrl |= F_TXPKT_IPCSUM_DIS;
			if (__predict_false(!(m0->m_pkthdr.csum_flags & (CSUM_TCP | CSUM_UDP))))
				cntrl |= F_TXPKT_L4CSUM_DIS;
			cbe->cntrl = htonl(cntrl);
			cbe->len = htonl(batchmi->mi_len | 0x80000000);
			cbe->addr = htobe64(segs[i].ds_addr);
			txd->flit[fidx] |= htobe64(1 << 24);
		}

		wrp->wr_hi = htonl(F_WR_SOP | F_WR_EOP | V_WR_DATATYPE(1) |
		    V_WR_SGLSFLT(flits)) | htonl(V_WR_OP(FW_WROPCODE_TUNNEL_TX_PKT) | txqs.compl);
		wmb();
		wrp->wr_lo = htonl(V_WR_LEN(flits) |
		    V_WR_GEN(txqs.gen)) | htonl(V_WR_TID(txq->token));
		/* XXX gen? */
		wr_gen2(txd, txqs.gen);
		check_ring_tx_db(sc, txq);
		
		return (0);		
	} else if (tso_info) {
		int min_size = TCPPKTHDRSIZE, eth_type, tagged;
		struct cpl_tx_pkt_lso *hdr = (struct cpl_tx_pkt_lso *)txd;
		struct ip *ip;
		struct tcphdr *tcp;
		char *pkthdr;

		txd->flit[2] = 0;
		GET_VTAG(cntrl, m0);
		cntrl |= V_TXPKT_OPCODE(CPL_TX_PKT_LSO);
		hdr->cntrl = htonl(cntrl);
		mlen = m0->m_pkthdr.len;
		hdr->len = htonl(mlen | 0x80000000);

		DPRINTF("tso buf len=%d\n", mlen);

		tagged = m0->m_flags & M_VLANTAG;
		if (!tagged)
			min_size -= ETHER_VLAN_ENCAP_LEN;

		if (__predict_false(mlen < min_size)) {
			printf("mbuf=%p,len=%d,tso_segsz=%d,csum_flags=%#x,flags=%#x",
			    m0, mlen, m0->m_pkthdr.tso_segsz,
			    m0->m_pkthdr.csum_flags, m0->m_flags);
			panic("tx tso packet too small");
		}

		/* Make sure that ether, ip, tcp headers are all in m0 */
		if (__predict_false(m0->m_len < min_size)) {
			m0 = m_pullup(m0, min_size);
			if (__predict_false(m0 == NULL)) {
				/* XXX panic probably an overreaction */
				panic("couldn't fit header into mbuf");
			}
		}
		pkthdr = m0->m_data;

		if (tagged) {
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

		if (__predict_false(mlen <= PIO_LEN)) {
			/* pkt not undersized but fits in PIO_LEN */
			printf("**5592 Fix** mbuf=%p,len=%d,tso_segsz=%d,csum_flags=%#x,flags=%#x",
			    m0, mlen, m0->m_pkthdr.tso_segsz, m0->m_pkthdr.csum_flags, m0->m_flags);
			txq_prod(txq, 1, &txqs);
			m_copydata(m0, 0, mlen, (caddr_t)&txd->flit[3]);
			m_freem(m0);
			m0 = NULL;
			flits = (mlen + 7) / 8 + 3;
			hdr->wr.wr_hi = htonl(V_WR_BCNTLFLT(mlen & 7) |
					  V_WR_OP(FW_WROPCODE_TUNNEL_TX_PKT) |
					  F_WR_SOP | F_WR_EOP | txqs.compl);
			wmb();
			hdr->wr.wr_lo = htonl(V_WR_LEN(flits) |
			    V_WR_GEN(txqs.gen) | V_WR_TID(txq->token));

			wr_gen2(txd, txqs.gen);
			check_ring_tx_db(sc, txq);
			return (0);
		}
		flits = 3;	
	} else {
		struct cpl_tx_pkt *cpl = (struct cpl_tx_pkt *)txd;

		GET_VTAG(cntrl, m0);
		cntrl |= V_TXPKT_OPCODE(CPL_TX_PKT);
		if (__predict_false(!(m0->m_pkthdr.csum_flags & CSUM_IP)))
			cntrl |= F_TXPKT_IPCSUM_DIS;
		if (__predict_false(!(m0->m_pkthdr.csum_flags & (CSUM_TCP | CSUM_UDP))))
			cntrl |= F_TXPKT_L4CSUM_DIS;
		cpl->cntrl = htonl(cntrl);
		mlen = m0->m_pkthdr.len;
		cpl->len = htonl(mlen | 0x80000000);

		if (mlen <= PIO_LEN) {
			txq_prod(txq, 1, &txqs);
			m_copydata(m0, 0, mlen, (caddr_t)&txd->flit[2]);
			m_freem(m0);
			m0 = NULL;
			flits = (mlen + 7) / 8 + 2;
			cpl->wr.wr_hi = htonl(V_WR_BCNTLFLT(mlen & 7) |
					  V_WR_OP(FW_WROPCODE_TUNNEL_TX_PKT) |
					  F_WR_SOP | F_WR_EOP | txqs.compl);
			wmb();
			cpl->wr.wr_lo = htonl(V_WR_LEN(flits) |
			    V_WR_GEN(txqs.gen) | V_WR_TID(txq->token));

			wr_gen2(txd, txqs.gen);
			check_ring_tx_db(sc, txq);
			DPRINTF("pio buf\n");
			return (0);
		}
		DPRINTF("regular buf\n");
		flits = 2;
	}
	wrp = (struct work_request_hdr *)txd;

#ifdef	nomore
	/*
	 * XXX need to move into one of the helper routines above
	 *
	 */
	if ((err = busdma_map_mbufs(m, txq, txsd, segs, &nsegs)) != 0) 
		return (err);
	m0 = *m;
#endif
	ndesc = calc_tx_descs(m0, nsegs);
	
	sgp = (ndesc == 1) ? (struct sg_ent *)&txd->flit[flits] : sgl;
	make_sgl(sgp, segs, nsegs);

	sgl_flits = sgl_len(nsegs);

	DPRINTF("make_sgl success nsegs==%d ndesc==%d\n", nsegs, ndesc);
	txq_prod(txq, ndesc, &txqs);
	wr_hi = htonl(V_WR_OP(FW_WROPCODE_TUNNEL_TX_PKT) | txqs.compl);
	wr_lo = htonl(V_WR_TID(txq->token));
	write_wr_hdr_sgl(ndesc, txd, &txqs, txq, sgl, flits, sgl_flits, wr_hi, wr_lo);
	check_ring_tx_db(pi->adapter, txq);

	if ((m0->m_type == MT_DATA) &&
	    ((m0->m_flags & (M_EXT|M_NOFREE)) == M_EXT) &&
	    (m0->m_ext.ext_type != EXT_PACKET)) {
		m0->m_flags &= ~M_EXT ;
		cxgb_mbufs_outstanding--;
		m_free(m0);
	}
	
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

	if (len > WR_LEN)
		panic("len too big %d\n", len);
	if (len < sizeof(*from))
		panic("len too small %d", len);
	
	memcpy(&to[1], &from[1], len - sizeof(*from));
	to->wr_hi = from->wr_hi | htonl(F_WR_SOP | F_WR_EOP |
					V_WR_BCNTLFLT(len & 7));
	wmb();
	to->wr_lo = from->wr_lo | htonl(V_WR_GEN(gen) |
					V_WR_LEN((len + 7) / 8));
	wr_gen2(d, gen);

	/*
	 * This check is a hack we should really fix the logic so
	 * that this can't happen
	 */
	if (m->m_type != MT_DONTFREE)
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

		printf("stopping q\n");
		
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
			log(LOG_ERR, "no desc available\n");
			return (ENOSPC);
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

	log(LOG_WARNING, "Restart_ctrlq in_use=%d\n", q->in_use);
	
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
	wmb();
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
void
t3_free_qset(adapter_t *sc, struct sge_qset *q)
{
	int i;
	
	t3_free_tx_desc_all(&q->txq[TXQ_ETH]);
	
	for (i = 0; i < SGE_TXQ_PER_SET; i++) 
		if (q->txq[i].txq_mr.br_ring != NULL) {
			free(q->txq[i].txq_mr.br_ring, M_DEVBUF);
			mtx_destroy(&q->txq[i].txq_mr.br_lock);
		}
	for (i = 0; i < SGE_RXQ_PER_SET; ++i) {
		if (q->fl[i].desc) {
			mtx_lock_spin(&sc->sge.reg_lock);
			t3_sge_disable_fl(sc, q->fl[i].cntxt_id);
			mtx_unlock_spin(&sc->sge.reg_lock);
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
			mtx_lock_spin(&sc->sge.reg_lock);
			t3_sge_enable_ecntxt(sc, q->txq[i].cntxt_id, 0);
			mtx_unlock_spin(&sc->sge.reg_lock);
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
		mtx_lock_spin(&sc->sge.reg_lock);
		t3_sge_disable_rspcntxt(sc, q->rspq.cntxt_id);
		mtx_unlock_spin(&sc->sge.reg_lock);
		
		bus_dmamap_unload(q->rspq.desc_tag, q->rspq.desc_map);
		bus_dmamem_free(q->rspq.desc_tag, q->rspq.desc,
			        q->rspq.desc_map);
		bus_dma_tag_destroy(q->rspq.desc_tag);
		MTX_DESTROY(&q->rspq.lock);
	}

	tcp_lro_free(&q->lro.ctrl);

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
	
#ifdef IFNET_MULTIQUEUE
	panic("%s should not be called when IFNET_MULTIQUEUE is defined", __FUNCTION__);
#endif		
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
#ifdef notyet
	/*
	 * 
	 * XXX
	 */
	for (i = 0; i < nqsets; ++i) {
		struct sge_qset *qs = &sc->sge.qs[i];
		
		taskqueue_drain(sc->tq, &qs->txq[TXQ_OFLD].qresume_task);
		taskqueue_drain(sc->tq, &qs->txq[TXQ_CTRL].qresume_task);
	}
#endif
}

/**
 *	t3_free_tx_desc - reclaims Tx descriptors and their buffers
 *	@adapter: the adapter
 *	@q: the Tx queue to reclaim descriptors from
 *	@reclaimable: the number of descriptors to reclaim
 *      @m_vec_size: maximum number of buffers to reclaim
 *      @desc_reclaimed: returns the number of descriptors reclaimed
 *
 *	Reclaims Tx descriptors from an SGE Tx queue and frees the associated
 *	Tx buffers.  Called with the Tx queue lock held.
 *
 *      Returns number of buffers of reclaimed   
 */
void
t3_free_tx_desc(struct sge_txq *q, int reclaimable)
{
	struct tx_sw_desc *txsd;
	unsigned int cidx;
	
#ifdef T3_TRACE
	T3_TRACE2(sc->tb[q->cntxt_id & 7],
		  "reclaiming %u Tx descriptors at cidx %u", reclaimable, cidx);
#endif
	cidx = q->cidx;
	txsd = &q->sdesc[cidx];
	DPRINTF("reclaiming %d WR\n", reclaimable);
	mtx_assert(&q->lock, MA_OWNED);
	while (reclaimable--) {
		DPRINTF("cidx=%d d=%p\n", cidx, txsd);
		if (txsd->mi.mi_base != NULL) {
			if (txsd->flags & TX_SW_DESC_MAPPED) {
				bus_dmamap_unload(q->entry_tag, txsd->map);
				txsd->flags &= ~TX_SW_DESC_MAPPED;
			}
			m_freem_iovec(&txsd->mi);	
			buf_ring_scan(&q->txq_mr, txsd->mi.mi_base, __FILE__, __LINE__);
			txsd->mi.mi_base = NULL;

#if defined(DIAGNOSTIC) && 0
			if (m_get_priority(txsd->m[0]) != cidx) 
				printf("pri=%d cidx=%d\n",
				    (int)m_get_priority(txsd->m[0]), cidx);
#endif			

		} else
			q->txq_skipped++;
		
		++txsd;
		if (++cidx == q->size) {
			cidx = 0;
			txsd = q->sdesc;
		}
	}
	q->cidx = cidx;

}

void
t3_free_tx_desc_all(struct sge_txq *q)
{
	int i;
	struct tx_sw_desc *txsd;
	
	for (i = 0; i < q->size; i++) {
		txsd = &q->sdesc[i];
		if (txsd->mi.mi_base != NULL) {
			if (txsd->flags & TX_SW_DESC_MAPPED) {
				bus_dmamap_unload(q->entry_tag, txsd->map);
				txsd->flags &= ~TX_SW_DESC_MAPPED;
			}
			m_freem_iovec(&txsd->mi);
			bzero(&txsd->mi, sizeof(txsd->mi));
		}
	}
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
	
	if (immediate(m) && nsegs == 0) {
		write_imm(d, m, m->m_len, gen);
		return;
	}

	/* Only TX_DATA builds SGLs */
	from = mtod(m, struct work_request_hdr *);
	memcpy(&d->flit[1], &from[1], m->m_len - sizeof(*from));

	flits = m->m_len / 8;
	sgp = (ndesc == 1) ? (struct sg_ent *)&d->flit[flits] : sgl;

	make_sgl(sgp, segs, nsegs);
	sgl_flits = sgl_len(nsegs);

	txqs.gen = gen;
	txqs.pidx = pidx;
	txqs.compl = 0;

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
	int ndescs;

	if (m->m_len <= WR_LEN && nsegs == 0)
		return (1);                 /* packet fits as immediate data */

	if (m->m_flags & M_IOVEC)
		cnt = mtomv(m)->mv_count;
	else
		cnt = nsegs;

	/* headers */
	flits = m->m_len / 8;

	ndescs = flits_to_desc(flits + sgl_len(cnt));

	CTR4(KTR_CXGB, "flits=%d sgl_len=%d nsegs=%d ndescs=%d",
	    flits, sgl_len(cnt), nsegs, ndescs);

	return (ndescs);
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
	int ret, nsegs;
	unsigned int ndesc;
	unsigned int pidx, gen;
	bus_dma_segment_t segs[TX_MAX_SEGS], *vsegs;
	struct tx_sw_desc *stx;

	nsegs = m_get_sgllen(m);
	vsegs = m_get_sgl(m);
	ndesc = calc_tx_descs_ofld(m, nsegs);
	busdma_map_sgl(vsegs, segs, nsegs);

	stx = &q->sdesc[q->pidx];
	KASSERT(stx->mi.mi_base == NULL, ("mi_base set"));
	
	mtx_lock(&q->lock);
again:	reclaim_completed_tx_(q, 16);
	ret = check_desc_avail(adap, q, m, ndesc, TXQ_OFLD);
	if (__predict_false(ret)) {
		if (ret == 1) {
			printf("no ofld desc avail\n");
			
			m_set_priority(m, ndesc);     /* save for restart */
			mtx_unlock(&q->lock);
			return (EINTR);
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
	return (0);
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
	bus_dma_segment_t segs[TX_MAX_SEGS];
	struct tx_sw_desc *stx = &q->sdesc[q->pidx];
	int nsegs, cleaned;
		
	mtx_lock(&q->lock);
again:	cleaned = reclaim_completed_tx_(q, 16);

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
	wmb();
	t3_write_reg(adap, A_SG_KDOORBELL,
		     F_SELEGRCNTX | V_EGRCNTX(q->cntxt_id));
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
t3_offload_tx(struct t3cdev *tdev, struct mbuf *m)
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
deliver_partial_bundle(struct t3cdev *tdev,
			struct sge_rspq *q,
			struct mbuf *mbufs[], int n)
{
	if (n) {
		q->offload_bundles++;
		cxgb_ofld_recv(tdev, mbufs, n);
	}
}

static __inline int
rx_offload(struct t3cdev *tdev, struct sge_rspq *rq,
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
		DPRINTF("restarting TXQ_OFLD\n");
		taskqueue_enqueue(sc->tq, &qs->txq[TXQ_OFLD].qresume_task);
	}
	DPRINTF("stopped=0x%x restart=%d processed=%d cleaned=%d in_use=%d\n",
	    qs->txq_stopped, should_restart_tx(&qs->txq[TXQ_CTRL]),
	    qs->txq[TXQ_CTRL].processed, qs->txq[TXQ_CTRL].cleaned,
	    qs->txq[TXQ_CTRL].in_use);
	
	if (isset(&qs->txq_stopped, TXQ_CTRL) &&
	    should_restart_tx(&qs->txq[TXQ_CTRL]) &&
	    test_and_clear_bit(TXQ_CTRL, &qs->txq_stopped)) {
		qs->txq[TXQ_CTRL].restarts++;
		DPRINTF("restarting TXQ_CTRL\n");
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
	int i, header_size, ret = 0;

	for (i = 0; i < SGE_TXQ_PER_SET; i++) {
		if ((q->txq[i].txq_mr.br_ring = malloc(cxgb_txq_buf_ring_size*sizeof(struct mbuf *),
			    M_DEVBUF, M_WAITOK|M_ZERO)) == NULL) {
			device_printf(sc->dev, "failed to allocate mbuf ring\n");
			goto err;
		}
		q->txq[i].txq_mr.br_prod = q->txq[i].txq_mr.br_cons = 0;
		q->txq[i].txq_mr.br_size = cxgb_txq_buf_ring_size;
		mtx_init(&q->txq[i].txq_mr.br_lock, "txq mbuf ring", NULL, MTX_DEF);
	}

	init_qset_cntxt(q, id);
	q->idx = id;
	
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


	header_size = sizeof(struct m_hdr) + sizeof(struct pkthdr) + sizeof(struct m_ext_) + sizeof(uint32_t);
	q->txq[TXQ_ETH].stop_thres = nports *
	    flits_to_desc(sgl_len(TX_MAX_SEGS + 1) + 3);

	q->fl[0].buf_size = (MCLBYTES - header_size);
	q->fl[0].zone = zone_clust;
	q->fl[0].type = EXT_CLUSTER;
#if __FreeBSD_version > 800000
	if (cxgb_use_16k_clusters) {		
		q->fl[1].buf_size = MJUM16BYTES - header_size;
		q->fl[1].zone = zone_jumbo16;
		q->fl[1].type = EXT_JUMBO16;
	} else {
		q->fl[1].buf_size = MJUM9BYTES - header_size;
		q->fl[1].zone = zone_jumbo9;
		q->fl[1].type = EXT_JUMBO9;		
	}
#else
	q->fl[1].buf_size = MJUMPAGESIZE - header_size;
	q->fl[1].zone = zone_jumbop;
	q->fl[1].type = EXT_JUMBOP;
#endif

	/*
	 * We allocate and setup the lro_ctrl structure irrespective of whether
	 * lro is available and/or enabled.
	 */
	q->lro.enabled = !!(pi->ifp->if_capenable & IFCAP_LRO);
	ret = tcp_lro_init(&q->lro.ctrl);
	if (ret) {
		printf("error %d from tcp_lro_init\n", ret);
		goto err;
	}
	q->lro.ctrl.ifp = pi->ifp;

	mtx_lock_spin(&sc->sge.reg_lock);
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
	
	mtx_unlock_spin(&sc->sge.reg_lock);
	t3_update_qset_coalesce(q, p);
	q->port = pi;
	
	refill_fl(sc, &q->fl[0], q->fl[0].size);
	refill_fl(sc, &q->fl[1], q->fl[1].size);
	refill_rspq(sc, &q->rspq, q->rspq.size - 1);

	t3_write_reg(sc, A_SG_GTS, V_RSPQ(q->rspq.cntxt_id) |
		     V_NEWTIMER(q->rspq.holdoff_tmr));

	return (0);

err_unlock:
	mtx_unlock_spin(&sc->sge.reg_lock);
err:	
	t3_free_qset(sc, q);

	return (ret);
}

/*
 * Remove CPL_RX_PKT headers from the mbuf and reduce it to a regular mbuf with
 * ethernet data.  Hardware assistance with various checksums and any vlan tag
 * will also be taken into account here.
 */
void
t3_rx_eth(struct adapter *adap, struct sge_rspq *rq, struct mbuf *m, int ethpad)
{
	struct cpl_rx_pkt *cpl = (struct cpl_rx_pkt *)(mtod(m, uint8_t *) + ethpad);
	struct port_info *pi = &adap->port[adap->rxpkt_map[cpl->iff]];
	struct ifnet *ifp = pi->ifp;
	
	DPRINTF("rx_eth m=%p m->m_data=%p p->iff=%d\n", m, mtod(m, uint8_t *), cpl->iff);

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
#ifndef DISABLE_MBUF_IOVEC
	m_explode(m);
#endif	
	/*
	 * adjust after conversion to mbuf chain
	 */
	m->m_pkthdr.len -= (sizeof(*cpl) + ethpad);
	m->m_len -= (sizeof(*cpl) + ethpad);
	m->m_data += (sizeof(*cpl) + ethpad);
}

static void
ext_free_handler(void *arg1, void * arg2)
{
	uintptr_t type = (uintptr_t)arg2;
	uma_zone_t zone;
	struct mbuf *m;

	m = arg1;
	zone = m_getzonefromtype(type);
	m->m_ext.ext_type = (int)type;
	cxgb_ext_freed++;
	cxgb_cache_put(zone, m);
}

static void
init_cluster_mbuf(caddr_t cl, int flags, int type, uma_zone_t zone)
{
	struct mbuf *m;
	int header_size;
	
	header_size = sizeof(struct m_hdr) + sizeof(struct pkthdr) +
	    sizeof(struct m_ext_) + sizeof(uint32_t);
	
	bzero(cl, header_size);
	m = (struct mbuf *)cl;
	
	cxgb_ext_inited++;
	SLIST_INIT(&m->m_pkthdr.tags);
	m->m_type = MT_DATA;
	m->m_flags = flags | M_NOFREE | M_EXT;
	m->m_data = cl + header_size;
	m->m_ext.ext_buf = cl;
	m->m_ext.ref_cnt = (uint32_t *)(cl + header_size - sizeof(uint32_t));
	m->m_ext.ext_size = m_getsizefromtype(type);
	m->m_ext.ext_free = ext_free_handler;
	m->m_ext.ext_arg1 = cl;
	m->m_ext.ext_arg2 = (void *)(uintptr_t)type;
	m->m_ext.ext_type = EXT_EXTREF;
	*(m->m_ext.ref_cnt) = 1;
	DPRINTF("data=%p ref_cnt=%p\n", m->m_data, m->m_ext.ref_cnt); 
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
#ifdef DISABLE_MBUF_IOVEC

static int
get_packet(adapter_t *adap, unsigned int drop_thres, struct sge_qset *qs,
    struct t3_mbuf_hdr *mh, struct rsp_desc *r)
{

	unsigned int len_cq =  ntohl(r->len_cq);
	struct sge_fl *fl = (len_cq & F_RSPD_FLQ) ? &qs->fl[1] : &qs->fl[0];
	struct rx_sw_desc *sd = &fl->sdesc[fl->cidx];
	uint32_t len = G_RSPD_LEN(len_cq);
	uint32_t flags = ntohl(r->flags);
	uint8_t sopeop = G_RSPD_SOP_EOP(flags);
	caddr_t cl;
	struct mbuf *m, *m0;
	int ret = 0;
	
	prefetch(sd->rxsd_cl);

	fl->credits--;
	bus_dmamap_sync(fl->entry_tag, sd->map, BUS_DMASYNC_POSTREAD);
	
	if (recycle_enable && len <= SGE_RX_COPY_THRES && sopeop == RSPQ_SOP_EOP) {
		if ((m0 = m_gethdr(M_DONTWAIT, MT_DATA)) == NULL)
			goto skip_recycle;
		cl = mtod(m0, void *);
		memcpy(cl, sd->data, len);
		recycle_rx_buf(adap, fl, fl->cidx);
		m = m0;
		m0->m_len = len;
	} else {
	skip_recycle:

		bus_dmamap_unload(fl->entry_tag, sd->map);
		cl = sd->rxsd_cl;
		m = m0 = (struct mbuf *)cl;

		if ((sopeop == RSPQ_SOP_EOP) ||
		    (sopeop == RSPQ_SOP))
			flags = M_PKTHDR;
		init_cluster_mbuf(cl, flags, fl->type, fl->zone);
		m0->m_len = len;
	}		
	switch(sopeop) {
	case RSPQ_SOP_EOP:
		DBG(DBG_RX, ("get_packet: SOP-EOP m %p\n", m));
		mh->mh_head = mh->mh_tail = m;
		m->m_pkthdr.len = len;
		ret = 1;
		break;
	case RSPQ_NSOP_NEOP:
		DBG(DBG_RX, ("get_packet: NO_SOP-NO_EOP m %p\n", m));
		if (mh->mh_tail == NULL) {
			log(LOG_ERR, "discarding intermediate descriptor entry\n");
			m_freem(m);
			break;
		}
		mh->mh_tail->m_next = m;
		mh->mh_tail = m;
		mh->mh_head->m_pkthdr.len += len;
		ret = 0;
		break;
	case RSPQ_SOP:
		DBG(DBG_RX, ("get_packet: SOP m %p\n", m));
		m->m_pkthdr.len = len;
		mh->mh_head = mh->mh_tail = m;
		ret = 0;
		break;
	case RSPQ_EOP:
		DBG(DBG_RX, ("get_packet: EOP m %p\n", m));
		mh->mh_head->m_pkthdr.len += len;
		mh->mh_tail->m_next = m;
		mh->mh_tail = m;
		ret = 1;
		break;
	}
	if (++fl->cidx == fl->size)
		fl->cidx = 0;

	return (ret);
}

#else

static int
get_packet(adapter_t *adap, unsigned int drop_thres, struct sge_qset *qs,
    struct mbuf **m, struct rsp_desc *r)
{
	
	unsigned int len_cq =  ntohl(r->len_cq);
	struct sge_fl *fl = (len_cq & F_RSPD_FLQ) ? &qs->fl[1] : &qs->fl[0];
	struct rx_sw_desc *sd = &fl->sdesc[fl->cidx];
	uint32_t len = G_RSPD_LEN(len_cq);
	uint32_t flags = ntohl(r->flags);
	uint8_t sopeop = G_RSPD_SOP_EOP(flags);
	void *cl;
	int ret = 0;
	struct mbuf *m0;
#if 0
	if ((sd + 1 )->rxsd_cl)
		prefetch((sd + 1)->rxsd_cl);
	if ((sd + 2)->rxsd_cl)
		prefetch((sd + 2)->rxsd_cl);
#endif
	DPRINTF("rx cpu=%d\n", curcpu);
	fl->credits--;
	bus_dmamap_sync(fl->entry_tag, sd->map, BUS_DMASYNC_POSTREAD);

	if (recycle_enable && len <= SGE_RX_COPY_THRES && sopeop == RSPQ_SOP_EOP) {
		if ((m0 = m_gethdr(M_DONTWAIT, MT_DATA)) == NULL)
			goto skip_recycle;
		cl = mtod(m0, void *);
		memcpy(cl, sd->data, len);
		recycle_rx_buf(adap, fl, fl->cidx);
		*m = m0;
	} else {
	skip_recycle:
		bus_dmamap_unload(fl->entry_tag, sd->map);
		cl = sd->rxsd_cl;
		*m = m0 = (struct mbuf *)cl;
	}

	switch(sopeop) {
	case RSPQ_SOP_EOP:
		DBG(DBG_RX, ("get_packet: SOP-EOP m %p\n", m));
		if (cl == sd->rxsd_cl)
			init_cluster_mbuf(cl, M_PKTHDR, fl->type, fl->zone);
		m0->m_len = m0->m_pkthdr.len = len;
		ret = 1;
		goto done;
		break;
	case RSPQ_NSOP_NEOP:
		DBG(DBG_RX, ("get_packet: NO_SOP-NO_EOP m %p\n", m));
		panic("chaining unsupported");
		ret = 0;
		break;
	case RSPQ_SOP:
		DBG(DBG_RX, ("get_packet: SOP m %p\n", m));
		panic("chaining unsupported");
		m_iovinit(m0);
		ret = 0;
		break;
	case RSPQ_EOP:
		DBG(DBG_RX, ("get_packet: EOP m %p\n", m));
		panic("chaining unsupported");
		ret = 1;
		break;
	}
	panic("append not supported");
#if 0	
	m_iovappend(m0, cl, fl->buf_size, len, sizeof(uint32_t), sd->rxsd_ref);
#endif	
done:	
	if (++fl->cidx == fl->size)
		fl->cidx = 0;

	return (ret);
}
#endif
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
	if (credits) 
		qs->txq[TXQ_ETH].processed += credits;
	
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
int
process_responses(adapter_t *adap, struct sge_qset *qs, int budget)
{
	struct sge_rspq *rspq = &qs->rspq;
	struct rsp_desc *r = &rspq->desc[rspq->cidx];
	int budget_left = budget;
	unsigned int sleeping = 0;
	int lro_enabled = qs->lro.enabled;
	struct lro_ctrl *lro_ctrl = &qs->lro.ctrl;
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
		uint32_t rss_hash = be32toh(r->rss_hdr.rss_hash_val);
		
		eth = (r->rss_hdr.opcode == CPL_RX_PKT);
		
		if (__predict_false(flags & F_RSPD_ASYNC_NOTIF)) {
			struct mbuf *m;

			if (cxgb_debug)
				printf("async notification\n");

			if (rspq->rspq_mh.mh_head == NULL) {
				rspq->rspq_mh.mh_head = m_gethdr(M_DONTWAIT, MT_DATA);
				m = rspq->rspq_mh.mh_head;
			} else {
				m = m_gethdr(M_DONTWAIT, MT_DATA);
			}

			/* XXX m is lost here if rspq->rspq_mbuf is not NULL */

			if (m == NULL)
				goto no_mem;

                        memcpy(mtod(m, char *), r, AN_PKT_SIZE);
			m->m_len = m->m_pkthdr.len = AN_PKT_SIZE;
                        *mtod(m, char *) = CPL_ASYNC_NOTIF;
			rss_csum = htonl(CPL_ASYNC_NOTIF << 24);
			eop = 1;
                        rspq->async_notif++;
			goto skip;
		} else if  (flags & F_RSPD_IMM_DATA_VALID) {
			struct mbuf *m = NULL;

			DPRINTF("IMM DATA VALID opcode=0x%x rspq->cidx=%d\n",
			    r->rss_hdr.opcode, rspq->cidx);
			if (rspq->rspq_mh.mh_head == NULL)
				rspq->rspq_mh.mh_head = m_gethdr(M_DONTWAIT, MT_DATA);
                        else 
				m = m_gethdr(M_DONTWAIT, MT_DATA);

			if (rspq->rspq_mh.mh_head == NULL &&  m == NULL) {	
		no_mem:
				rspq->next_holdoff = NOMEM_INTR_DELAY;
				budget_left--;
				break;
			}
			get_imm_packet(adap, r, rspq->rspq_mh.mh_head);
			eop = 1;
			rspq->imm_data++;
		} else if (r->len_cq) {
			int drop_thresh = eth ? SGE_RX_DROP_THRES : 0;
			
#ifdef DISABLE_MBUF_IOVEC
			eop = get_packet(adap, drop_thresh, qs, &rspq->rspq_mh, r);
#else
			eop = get_packet(adap, drop_thresh, qs, &rspq->rspq_mbuf, r);
#endif
#ifdef IFNET_MULTIQUEUE
			rspq->rspq_mh.mh_head->m_pkthdr.rss_hash = rss_hash;
#endif			
			ethpad = 2;
		} else {
			DPRINTF("pure response\n");
			rspq->pure_rsps++;
		}
	skip:
		if (flags & RSPD_CTRL_MASK) {
			sleeping |= flags & RSPD_GTS_MASK;
			handle_rsp_cntrl_info(qs, flags);
		}

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
		DPRINTF("eth=%d eop=%d flags=0x%x\n", eth, eop, flags);

		if (!eth && eop) {
			rspq->rspq_mh.mh_head->m_pkthdr.csum_data = rss_csum;
			/*
			 * XXX size mismatch
			 */
			m_set_priority(rspq->rspq_mh.mh_head, rss_hash);

			
			ngathered = rx_offload(&adap->tdev, rspq,
			    rspq->rspq_mh.mh_head, offload_mbufs, ngathered);
			rspq->rspq_mh.mh_head = NULL;
			DPRINTF("received offload packet\n");
			
		} else if (eth && eop) {
			struct mbuf *m = rspq->rspq_mh.mh_head;
			prefetch(mtod(m, uint8_t *)); 
			prefetch(mtod(m, uint8_t *) + L1_CACHE_BYTES);

			t3_rx_eth(adap, rspq, m, ethpad);
			if (lro_enabled && lro_ctrl->lro_cnt &&
			    (tcp_lro_rx(lro_ctrl, m, 0) == 0)) {
				/* successfully queue'd for LRO */
			} else {
				/*
				 * LRO not enabled, packet unsuitable for LRO,
				 * or unable to queue.  Pass it up right now in
				 * either case.
				 */
				struct ifnet *ifp = m->m_pkthdr.rcvif;
				(*ifp->if_input)(ifp, m);
			}
			DPRINTF("received tunnel packet\n");
			rspq->rspq_mh.mh_head = NULL;

		}
		__refill_fl_lt(adap, &qs->fl[0], 32);
		__refill_fl_lt(adap, &qs->fl[1], 32);
		--budget_left;
	}

	deliver_partial_bundle(&adap->tdev, rspq, offload_mbufs, ngathered);

	/* Flush LRO */
	while (!SLIST_EMPTY(&lro_ctrl->lro_active)) {
		struct lro_entry *queued = SLIST_FIRST(&lro_ctrl->lro_active);
		SLIST_REMOVE_HEAD(&lro_ctrl->lro_active, next);
		tcp_lro_flush(lro_ctrl, queued);
	}

	if (sleeping)
		check_ring_db(adap, qs, sleeping);

	smp_mb();  /* commit Tx queue processed updates */
	if (__predict_false(qs->txq_stopped > 1)) {
		printf("restarting tx on %p\n", qs);
		
		restart_tx(qs);
	}
	
	__refill_fl_lt(adap, &qs->fl[0], 512);
	__refill_fl_lt(adap, &qs->fl[1], 512);
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
	
	return (work);
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
	uint32_t i, map;
	adapter_t *adap = data;
	struct sge_rspq *q0 = &adap->sge.qs[0].rspq;
	
	t3_write_reg(adap, A_PL_CLI, 0);
	map = t3_read_reg(adap, A_SG_DATA_INTR);

	if (!map) 
		return;

	if (__predict_false(map & F_ERRINTR))
		taskqueue_enqueue(adap->tq, &adap->slow_intr_task);

	mtx_lock(&q0->lock);
	for_each_port(adap, i)
	    if (map & (1 << i))
			process_responses_gts(adap, &adap->sge.qs[i].rspq);
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
	int i, new_packets = 0;

	mtx_lock(&q0->lock);

	for_each_port(adap, i)
	    if (process_responses_gts(adap, &adap->sge.qs[i].rspq)) 
		    new_packets = 1;
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
#ifndef IFNET_MULTIQUEUE
	mtx_lock(&rspq->lock);
#else	
	if (mtx_trylock(&rspq->lock)) 
#endif
	{
		
		if (process_responses_gts(adap, rspq) == 0)
			rspq->unhandled_irqs++;
		mtx_unlock(&rspq->lock);
	}
}

#define QDUMP_SBUF_SIZE		32 * 400
static int
t3_dump_rspq(SYSCTL_HANDLER_ARGS)
{
	struct sge_rspq *rspq;
	struct sge_qset *qs;
	int i, err, dump_end, idx;
	static int multiplier = 1;
	struct sbuf *sb;
	struct rsp_desc *rspd;
	uint32_t data[4];
	
	rspq = arg1;
	qs = rspq_to_qset(rspq);
	if (rspq->rspq_dump_count == 0) 
		return (0);
	if (rspq->rspq_dump_count > RSPQ_Q_SIZE) {
		log(LOG_WARNING,
		    "dump count is too large %d\n", rspq->rspq_dump_count);
		rspq->rspq_dump_count = 0;
		return (EINVAL);
	}
	if (rspq->rspq_dump_start > (RSPQ_Q_SIZE-1)) {
		log(LOG_WARNING,
		    "dump start of %d is greater than queue size\n",
		    rspq->rspq_dump_start);
		rspq->rspq_dump_start = 0;
		return (EINVAL);
	}
	err = t3_sge_read_rspq(qs->port->adapter, rspq->cntxt_id, data);
	if (err)
		return (err);
retry_sbufops:
	sb = sbuf_new(NULL, NULL, QDUMP_SBUF_SIZE*multiplier, SBUF_FIXEDLEN);

	sbuf_printf(sb, " \n index=%u size=%u MSI-X/RspQ=%u intr enable=%u intr armed=%u\n",
	    (data[0] & 0xffff), data[0] >> 16, ((data[2] >> 20) & 0x3f),
	    ((data[2] >> 26) & 1), ((data[2] >> 27) & 1));
	sbuf_printf(sb, " generation=%u CQ mode=%u FL threshold=%u\n",
	    ((data[2] >> 28) & 1), ((data[2] >> 31) & 1), data[3]);
	
	sbuf_printf(sb, " start=%d -> end=%d\n", rspq->rspq_dump_start,
	    (rspq->rspq_dump_start + rspq->rspq_dump_count) & (RSPQ_Q_SIZE-1));
	
	dump_end = rspq->rspq_dump_start + rspq->rspq_dump_count;
	for (i = rspq->rspq_dump_start; i < dump_end; i++) {
		idx = i & (RSPQ_Q_SIZE-1);
		
		rspd = &rspq->desc[idx];
		sbuf_printf(sb, "\tidx=%04d opcode=%02x cpu_idx=%x hash_type=%x cq_idx=%x\n",
		    idx, rspd->rss_hdr.opcode, rspd->rss_hdr.cpu_idx,
		    rspd->rss_hdr.hash_type, be16toh(rspd->rss_hdr.cq_idx));
		sbuf_printf(sb, "\trss_hash_val=%x flags=%08x len_cq=%x intr_gen=%x\n",
		    rspd->rss_hdr.rss_hash_val, be32toh(rspd->flags),
		    be32toh(rspd->len_cq), rspd->intr_gen);
	}
	if (sbuf_overflowed(sb)) {
		sbuf_delete(sb);
		multiplier++;
		goto retry_sbufops;
	}
	sbuf_finish(sb);
	err = SYSCTL_OUT(req, sbuf_data(sb), sbuf_len(sb) + 1);
	sbuf_delete(sb);
	return (err);
}	

static int
t3_dump_txq_eth(SYSCTL_HANDLER_ARGS)
{
	struct sge_txq *txq;
	struct sge_qset *qs;
	int i, j, err, dump_end;
	static int multiplier = 1;
	struct sbuf *sb;
	struct tx_desc *txd;
	uint32_t *WR, wr_hi, wr_lo, gen;
	uint32_t data[4];
	
	txq = arg1;
	qs = txq_to_qset(txq, TXQ_ETH);
	if (txq->txq_dump_count == 0) {
		return (0);
	}
	if (txq->txq_dump_count > TX_ETH_Q_SIZE) {
		log(LOG_WARNING,
		    "dump count is too large %d\n", txq->txq_dump_count);
		txq->txq_dump_count = 1;
		return (EINVAL);
	}
	if (txq->txq_dump_start > (TX_ETH_Q_SIZE-1)) {
		log(LOG_WARNING,
		    "dump start of %d is greater than queue size\n",
		    txq->txq_dump_start);
		txq->txq_dump_start = 0;
		return (EINVAL);
	}
	err = t3_sge_read_ecntxt(qs->port->adapter, qs->rspq.cntxt_id, data);
	if (err)
		return (err);
	
	    
retry_sbufops:
	sb = sbuf_new(NULL, NULL, QDUMP_SBUF_SIZE*multiplier, SBUF_FIXEDLEN);

	sbuf_printf(sb, " \n credits=%u GTS=%u index=%u size=%u rspq#=%u cmdq#=%u\n",
	    (data[0] & 0x7fff), ((data[0] >> 15) & 1), (data[0] >> 16), 
	    (data[1] & 0xffff), ((data[3] >> 4) & 7), ((data[3] >> 7) & 1));
	sbuf_printf(sb, " TUN=%u TOE=%u generation%u uP token=%u valid=%u\n",
	    ((data[3] >> 8) & 1), ((data[3] >> 9) & 1), ((data[3] >> 10) & 1),
	    ((data[3] >> 11) & 0xfffff), ((data[3] >> 31) & 1));
	sbuf_printf(sb, " qid=%d start=%d -> end=%d\n", qs->idx,
	    txq->txq_dump_start,
	    (txq->txq_dump_start + txq->txq_dump_count) & (TX_ETH_Q_SIZE-1));

	dump_end = txq->txq_dump_start + txq->txq_dump_count;
	for (i = txq->txq_dump_start; i < dump_end; i++) {
		txd = &txq->desc[i & (TX_ETH_Q_SIZE-1)];
		WR = (uint32_t *)txd->flit;
		wr_hi = ntohl(WR[0]);
		wr_lo = ntohl(WR[1]);		
		gen = G_WR_GEN(wr_lo);
		
		sbuf_printf(sb," wr_hi %08x wr_lo %08x gen %d\n",
		    wr_hi, wr_lo, gen);
		for (j = 2; j < 30; j += 4) 
			sbuf_printf(sb, "\t%08x %08x %08x %08x \n",
			    WR[j], WR[j + 1], WR[j + 2], WR[j + 3]);

	}
	if (sbuf_overflowed(sb)) {
		sbuf_delete(sb);
		multiplier++;
		goto retry_sbufops;
	}
	sbuf_finish(sb);
	err = SYSCTL_OUT(req, sbuf_data(sb), sbuf_len(sb) + 1);
	sbuf_delete(sb);
	return (err);
}

static int
t3_dump_txq_ctrl(SYSCTL_HANDLER_ARGS)
{
	struct sge_txq *txq;
	struct sge_qset *qs;
	int i, j, err, dump_end;
	static int multiplier = 1;
	struct sbuf *sb;
	struct tx_desc *txd;
	uint32_t *WR, wr_hi, wr_lo, gen;
	
	txq = arg1;
	qs = txq_to_qset(txq, TXQ_CTRL);
	if (txq->txq_dump_count == 0) {
		return (0);
	}
	if (txq->txq_dump_count > 256) {
		log(LOG_WARNING,
		    "dump count is too large %d\n", txq->txq_dump_count);
		txq->txq_dump_count = 1;
		return (EINVAL);
	}
	if (txq->txq_dump_start > 255) {
		log(LOG_WARNING,
		    "dump start of %d is greater than queue size\n",
		    txq->txq_dump_start);
		txq->txq_dump_start = 0;
		return (EINVAL);
	}

retry_sbufops:
	sb = sbuf_new(NULL, NULL, QDUMP_SBUF_SIZE*multiplier, SBUF_FIXEDLEN);
	sbuf_printf(sb, " qid=%d start=%d -> end=%d\n", qs->idx,
	    txq->txq_dump_start,
	    (txq->txq_dump_start + txq->txq_dump_count) & 255);

	dump_end = txq->txq_dump_start + txq->txq_dump_count;
	for (i = txq->txq_dump_start; i < dump_end; i++) {
		txd = &txq->desc[i & (255)];
		WR = (uint32_t *)txd->flit;
		wr_hi = ntohl(WR[0]);
		wr_lo = ntohl(WR[1]);		
		gen = G_WR_GEN(wr_lo);
		
		sbuf_printf(sb," wr_hi %08x wr_lo %08x gen %d\n",
		    wr_hi, wr_lo, gen);
		for (j = 2; j < 30; j += 4) 
			sbuf_printf(sb, "\t%08x %08x %08x %08x \n",
			    WR[j], WR[j + 1], WR[j + 2], WR[j + 3]);

	}
	if (sbuf_overflowed(sb)) {
		sbuf_delete(sb);
		multiplier++;
		goto retry_sbufops;
	}
	sbuf_finish(sb);
	err = SYSCTL_OUT(req, sbuf_data(sb), sbuf_len(sb) + 1);
	sbuf_delete(sb);
	return (err);
}

static int
t3_set_coalesce_usecs(SYSCTL_HANDLER_ARGS)
{
	adapter_t *sc = arg1;
	struct qset_params *qsp = &sc->params.sge.qset[0]; 
	int coalesce_usecs;	
	struct sge_qset *qs;
	int i, j, err, nqsets = 0;
	struct mtx *lock;

	if ((sc->flags & FULL_INIT_DONE) == 0)
		return (ENXIO);
		
	coalesce_usecs = qsp->coalesce_usecs;
        err = sysctl_handle_int(oidp, &coalesce_usecs, arg2, req);

	if (err != 0) {
		return (err);
	}
	if (coalesce_usecs == qsp->coalesce_usecs)
		return (0);

	for (i = 0; i < sc->params.nports; i++) 
		for (j = 0; j < sc->port[i].nqsets; j++)
			nqsets++;

	coalesce_usecs = max(1, coalesce_usecs);

	for (i = 0; i < nqsets; i++) {
		qs = &sc->sge.qs[i];
		qsp = &sc->params.sge.qset[i];
		qsp->coalesce_usecs = coalesce_usecs;
		
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
t3_add_attach_sysctls(adapter_t *sc)
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
	SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
	    "hw_revision",
	    CTLFLAG_RD, &sc->params.rev,
	    0, "chip model");
	SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
	    "enable_debug",
	    CTLFLAG_RW, &cxgb_debug,
	    0, "enable verbose debugging output");
	SYSCTL_ADD_ULONG(ctx, children, OID_AUTO, "tunq_coalesce",
	    CTLFLAG_RD, &sc->tunq_coalesce,
	    "#tunneled packets freed");
	SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
	    "txq_overrun",
	    CTLFLAG_RD, &txq_fills,
	    0, "#times txq overrun");
	SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
	    "pcpu_cache_enable",
	    CTLFLAG_RW, &cxgb_pcpu_cache_enable,
	    0, "#enable driver local pcpu caches");
	SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
	    "cache_alloc",
	    CTLFLAG_RD, &cxgb_cached_allocations,
	    0, "#times a cluster was allocated from cache");
	SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
	    "cached",
	    CTLFLAG_RD, &cxgb_cached,
	    0, "#times a cluster was cached");
	SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
	    "ext_freed",
	    CTLFLAG_RD, &cxgb_ext_freed,
	    0, "#times a cluster was freed through ext_free");
	SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
	    "ext_inited",
	    CTLFLAG_RD, &cxgb_ext_inited,
	    0, "#times a cluster was initialized for ext_free");
	SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
	    "mbufs_outstanding",
	    CTLFLAG_RD, &cxgb_mbufs_outstanding,
	    0, "#mbufs in flight in the driver");
	SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
	    "pack_outstanding",
	    CTLFLAG_RD, &cxgb_pack_outstanding,
	    0, "#packet in flight in the driver"); 	
}


static const char *rspq_name = "rspq";
static const char *txq_names[] =
{
	"txq_eth",
	"txq_ofld",
	"txq_ctrl"	
};

static int
sysctl_handle_macstat(SYSCTL_HANDLER_ARGS)
{
	struct port_info *p = arg1;
	uint64_t *parg;

	if (!p)
		return (EINVAL);

	parg = (uint64_t *) ((uint8_t *)&p->mac.stats + arg2);

	PORT_LOCK(p);
	t3_mac_update_stats(&p->mac);
	PORT_UNLOCK(p);

	return (sysctl_handle_quad(oidp, parg, 0, req));
}

void
t3_add_configured_sysctls(adapter_t *sc)
{
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid_list *children;
	int i, j;
	
	ctx = device_get_sysctl_ctx(sc->dev);
	children = SYSCTL_CHILDREN(device_get_sysctl_tree(sc->dev));

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, 
	    "intr_coal",
	    CTLTYPE_INT|CTLFLAG_RW, sc,
	    0, t3_set_coalesce_usecs,
	    "I", "interrupt coalescing timer (us)");

	for (i = 0; i < sc->params.nports; i++) {
		struct port_info *pi = &sc->port[i];
		struct sysctl_oid *poid;
		struct sysctl_oid_list *poidlist;
		struct mac_stats *mstats = &pi->mac.stats;
		
		snprintf(pi->namebuf, PORT_NAME_LEN, "port%d", i);
		poid = SYSCTL_ADD_NODE(ctx, children, OID_AUTO, 
		    pi->namebuf, CTLFLAG_RD, NULL, "port statistics");
		poidlist = SYSCTL_CHILDREN(poid);
		SYSCTL_ADD_INT(ctx, poidlist, OID_AUTO, 
		    "nqsets", CTLFLAG_RD, &pi->nqsets,
		    0, "#queue sets");

		for (j = 0; j < pi->nqsets; j++) {
			struct sge_qset *qs = &sc->sge.qs[pi->first_qset + j];
			struct sysctl_oid *qspoid, *rspqpoid, *txqpoid, *ctrlqpoid, *lropoid;
			struct sysctl_oid_list *qspoidlist, *rspqpoidlist, *txqpoidlist, *ctrlqpoidlist, *lropoidlist;
			struct sge_txq *txq = &qs->txq[TXQ_ETH];
			
			snprintf(qs->namebuf, QS_NAME_LEN, "qs%d", j);
			
			qspoid = SYSCTL_ADD_NODE(ctx, poidlist, OID_AUTO, 
			    qs->namebuf, CTLFLAG_RD, NULL, "qset statistics");
			qspoidlist = SYSCTL_CHILDREN(qspoid);
			
			rspqpoid = SYSCTL_ADD_NODE(ctx, qspoidlist, OID_AUTO, 
			    rspq_name, CTLFLAG_RD, NULL, "rspq statistics");
			rspqpoidlist = SYSCTL_CHILDREN(rspqpoid);

			txqpoid = SYSCTL_ADD_NODE(ctx, qspoidlist, OID_AUTO, 
			    txq_names[0], CTLFLAG_RD, NULL, "txq statistics");
			txqpoidlist = SYSCTL_CHILDREN(txqpoid);

			ctrlqpoid = SYSCTL_ADD_NODE(ctx, qspoidlist, OID_AUTO, 
			    txq_names[2], CTLFLAG_RD, NULL, "ctrlq statistics");
			ctrlqpoidlist = SYSCTL_CHILDREN(ctrlqpoid);

			lropoid = SYSCTL_ADD_NODE(ctx, qspoidlist, OID_AUTO, 
			    "lro_stats", CTLFLAG_RD, NULL, "LRO statistics");
			lropoidlist = SYSCTL_CHILDREN(lropoid);

			SYSCTL_ADD_UINT(ctx, rspqpoidlist, OID_AUTO, "size",
			    CTLFLAG_RD, &qs->rspq.size,
			    0, "#entries in response queue");
			SYSCTL_ADD_UINT(ctx, rspqpoidlist, OID_AUTO, "cidx",
			    CTLFLAG_RD, &qs->rspq.cidx,
			    0, "consumer index");
			SYSCTL_ADD_UINT(ctx, rspqpoidlist, OID_AUTO, "credits",
			    CTLFLAG_RD, &qs->rspq.credits,
			    0, "#credits");
			SYSCTL_ADD_XLONG(ctx, rspqpoidlist, OID_AUTO, "phys_addr",
			    CTLFLAG_RD, &qs->rspq.phys_addr,
			    "physical_address_of the queue");
			SYSCTL_ADD_UINT(ctx, rspqpoidlist, OID_AUTO, "dump_start",
			    CTLFLAG_RW, &qs->rspq.rspq_dump_start,
			    0, "start rspq dump entry");
			SYSCTL_ADD_UINT(ctx, rspqpoidlist, OID_AUTO, "dump_count",
			    CTLFLAG_RW, &qs->rspq.rspq_dump_count,
			    0, "#rspq entries to dump");
			SYSCTL_ADD_PROC(ctx, rspqpoidlist, OID_AUTO, "qdump",
			    CTLTYPE_STRING | CTLFLAG_RD, &qs->rspq,
			    0, t3_dump_rspq, "A", "dump of the response queue");


			SYSCTL_ADD_INT(ctx, txqpoidlist, OID_AUTO, "dropped",
			    CTLFLAG_RD, &qs->txq[TXQ_ETH].txq_drops,
			    0, "#tunneled packets dropped");
			SYSCTL_ADD_INT(ctx, txqpoidlist, OID_AUTO, "sendqlen",
			    CTLFLAG_RD, &qs->txq[TXQ_ETH].sendq.qlen,
			    0, "#tunneled packets waiting to be sent");
			SYSCTL_ADD_UINT(ctx, txqpoidlist, OID_AUTO, "queue_pidx",
			    CTLFLAG_RD, (uint32_t *)(uintptr_t)&qs->txq[TXQ_ETH].txq_mr.br_prod,
			    0, "#tunneled packets queue producer index");
			SYSCTL_ADD_UINT(ctx, txqpoidlist, OID_AUTO, "queue_cidx",
			    CTLFLAG_RD, (uint32_t *)(uintptr_t)&qs->txq[TXQ_ETH].txq_mr.br_cons,
			    0, "#tunneled packets queue consumer index");
			SYSCTL_ADD_INT(ctx, txqpoidlist, OID_AUTO, "processed",
			    CTLFLAG_RD, &qs->txq[TXQ_ETH].processed,
			    0, "#tunneled packets processed by the card");
			SYSCTL_ADD_UINT(ctx, txqpoidlist, OID_AUTO, "cleaned",
			    CTLFLAG_RD, &txq->cleaned,
			    0, "#tunneled packets cleaned");
			SYSCTL_ADD_UINT(ctx, txqpoidlist, OID_AUTO, "in_use",
			    CTLFLAG_RD, &txq->in_use,
			    0, "#tunneled packet slots in use");
			SYSCTL_ADD_ULONG(ctx, txqpoidlist, OID_AUTO, "frees",
			    CTLFLAG_RD, &txq->txq_frees,
			    "#tunneled packets freed");
			SYSCTL_ADD_UINT(ctx, txqpoidlist, OID_AUTO, "skipped",
			    CTLFLAG_RD, &txq->txq_skipped,
			    0, "#tunneled packet descriptors skipped");
			SYSCTL_ADD_UINT(ctx, txqpoidlist, OID_AUTO, "coalesced",
			    CTLFLAG_RD, &txq->txq_coalesced,
			    0, "#tunneled packets coalesced");
			SYSCTL_ADD_UINT(ctx, txqpoidlist, OID_AUTO, "enqueued",
			    CTLFLAG_RD, &txq->txq_enqueued,
			    0, "#tunneled packets enqueued to hardware");
			SYSCTL_ADD_UINT(ctx, txqpoidlist, OID_AUTO, "stopped_flags",
			    CTLFLAG_RD, &qs->txq_stopped,
			    0, "tx queues stopped");
			SYSCTL_ADD_XLONG(ctx, txqpoidlist, OID_AUTO, "phys_addr",
			    CTLFLAG_RD, &txq->phys_addr,
			    "physical_address_of the queue");
			SYSCTL_ADD_UINT(ctx, txqpoidlist, OID_AUTO, "qgen",
			    CTLFLAG_RW, &qs->txq[TXQ_ETH].gen,
			    0, "txq generation");
			SYSCTL_ADD_UINT(ctx, txqpoidlist, OID_AUTO, "hw_cidx",
			    CTLFLAG_RD, &txq->cidx,
			    0, "hardware queue cidx");			
			SYSCTL_ADD_UINT(ctx, txqpoidlist, OID_AUTO, "hw_pidx",
			    CTLFLAG_RD, &txq->pidx,
			    0, "hardware queue pidx");
			SYSCTL_ADD_UINT(ctx, txqpoidlist, OID_AUTO, "dump_start",
			    CTLFLAG_RW, &qs->txq[TXQ_ETH].txq_dump_start,
			    0, "txq start idx for dump");
			SYSCTL_ADD_UINT(ctx, txqpoidlist, OID_AUTO, "dump_count",
			    CTLFLAG_RW, &qs->txq[TXQ_ETH].txq_dump_count,
			    0, "txq #entries to dump");			
			SYSCTL_ADD_PROC(ctx, txqpoidlist, OID_AUTO, "qdump",
			    CTLTYPE_STRING | CTLFLAG_RD, &qs->txq[TXQ_ETH],
			    0, t3_dump_txq_eth, "A", "dump of the transmit queue");

			SYSCTL_ADD_UINT(ctx, ctrlqpoidlist, OID_AUTO, "dump_start",
			    CTLFLAG_RW, &qs->txq[TXQ_CTRL].txq_dump_start,
			    0, "ctrlq start idx for dump");
			SYSCTL_ADD_UINT(ctx, ctrlqpoidlist, OID_AUTO, "dump_count",
			    CTLFLAG_RW, &qs->txq[TXQ_CTRL].txq_dump_count,
			    0, "ctrl #entries to dump");			
			SYSCTL_ADD_PROC(ctx, ctrlqpoidlist, OID_AUTO, "qdump",
			    CTLTYPE_STRING | CTLFLAG_RD, &qs->txq[TXQ_CTRL],
			    0, t3_dump_txq_ctrl, "A", "dump of the transmit queue");

			SYSCTL_ADD_INT(ctx, lropoidlist, OID_AUTO, "lro_queued",
			    CTLFLAG_RD, &qs->lro.ctrl.lro_queued, 0, NULL);
			SYSCTL_ADD_INT(ctx, lropoidlist, OID_AUTO, "lro_flushed",
			    CTLFLAG_RD, &qs->lro.ctrl.lro_flushed, 0, NULL);
			SYSCTL_ADD_INT(ctx, lropoidlist, OID_AUTO, "lro_bad_csum",
			    CTLFLAG_RD, &qs->lro.ctrl.lro_bad_csum, 0, NULL);
			SYSCTL_ADD_INT(ctx, lropoidlist, OID_AUTO, "lro_cnt",
			    CTLFLAG_RD, &qs->lro.ctrl.lro_cnt, 0, NULL);
		}

		/* Now add a node for mac stats. */
		poid = SYSCTL_ADD_NODE(ctx, poidlist, OID_AUTO, "mac_stats",
		    CTLFLAG_RD, NULL, "MAC statistics");
		poidlist = SYSCTL_CHILDREN(poid);

		/*
		 * We (ab)use the length argument (arg2) to pass on the offset
		 * of the data that we are interested in.  This is only required
		 * for the quad counters that are updated from the hardware (we
		 * make sure that we return the latest value).
		 * sysctl_handle_macstat first updates *all* the counters from
		 * the hardware, and then returns the latest value of the
		 * requested counter.  Best would be to update only the
		 * requested counter from hardware, but t3_mac_update_stats()
		 * hides all the register details and we don't want to dive into
		 * all that here.
		 */
#define CXGB_SYSCTL_ADD_QUAD(a)	SYSCTL_ADD_OID(ctx, poidlist, OID_AUTO, #a, \
    (CTLTYPE_QUAD | CTLFLAG_RD), pi, offsetof(struct mac_stats, a), \
    sysctl_handle_macstat, "QU", 0)
		CXGB_SYSCTL_ADD_QUAD(tx_octets);
		CXGB_SYSCTL_ADD_QUAD(tx_octets_bad);
		CXGB_SYSCTL_ADD_QUAD(tx_frames);
		CXGB_SYSCTL_ADD_QUAD(tx_mcast_frames);
		CXGB_SYSCTL_ADD_QUAD(tx_bcast_frames);
		CXGB_SYSCTL_ADD_QUAD(tx_pause);
		CXGB_SYSCTL_ADD_QUAD(tx_deferred);
		CXGB_SYSCTL_ADD_QUAD(tx_late_collisions);
		CXGB_SYSCTL_ADD_QUAD(tx_total_collisions);
		CXGB_SYSCTL_ADD_QUAD(tx_excess_collisions);
		CXGB_SYSCTL_ADD_QUAD(tx_underrun);
		CXGB_SYSCTL_ADD_QUAD(tx_len_errs);
		CXGB_SYSCTL_ADD_QUAD(tx_mac_internal_errs);
		CXGB_SYSCTL_ADD_QUAD(tx_excess_deferral);
		CXGB_SYSCTL_ADD_QUAD(tx_fcs_errs);
		CXGB_SYSCTL_ADD_QUAD(tx_frames_64);
		CXGB_SYSCTL_ADD_QUAD(tx_frames_65_127);
		CXGB_SYSCTL_ADD_QUAD(tx_frames_128_255);
		CXGB_SYSCTL_ADD_QUAD(tx_frames_256_511);
		CXGB_SYSCTL_ADD_QUAD(tx_frames_512_1023);
		CXGB_SYSCTL_ADD_QUAD(tx_frames_1024_1518);
		CXGB_SYSCTL_ADD_QUAD(tx_frames_1519_max);
		CXGB_SYSCTL_ADD_QUAD(rx_octets);
		CXGB_SYSCTL_ADD_QUAD(rx_octets_bad);
		CXGB_SYSCTL_ADD_QUAD(rx_frames);
		CXGB_SYSCTL_ADD_QUAD(rx_mcast_frames);
		CXGB_SYSCTL_ADD_QUAD(rx_bcast_frames);
		CXGB_SYSCTL_ADD_QUAD(rx_pause);
		CXGB_SYSCTL_ADD_QUAD(rx_align_errs);
		CXGB_SYSCTL_ADD_QUAD(rx_symbol_errs);
		CXGB_SYSCTL_ADD_QUAD(rx_data_errs);
		CXGB_SYSCTL_ADD_QUAD(rx_sequence_errs);
		CXGB_SYSCTL_ADD_QUAD(rx_runt);
		CXGB_SYSCTL_ADD_QUAD(rx_jabber);
		CXGB_SYSCTL_ADD_QUAD(rx_short);
		CXGB_SYSCTL_ADD_QUAD(rx_too_long);
		CXGB_SYSCTL_ADD_QUAD(rx_mac_internal_errs);
		CXGB_SYSCTL_ADD_QUAD(rx_cong_drops);
		CXGB_SYSCTL_ADD_QUAD(rx_frames_64);
		CXGB_SYSCTL_ADD_QUAD(rx_frames_65_127);
		CXGB_SYSCTL_ADD_QUAD(rx_frames_128_255);
		CXGB_SYSCTL_ADD_QUAD(rx_frames_256_511);
		CXGB_SYSCTL_ADD_QUAD(rx_frames_512_1023);
		CXGB_SYSCTL_ADD_QUAD(rx_frames_1024_1518);
		CXGB_SYSCTL_ADD_QUAD(rx_frames_1519_max);
#undef CXGB_SYSCTL_ADD_QUAD

#define CXGB_SYSCTL_ADD_ULONG(a) SYSCTL_ADD_ULONG(ctx, poidlist, OID_AUTO, #a, \
    CTLFLAG_RD, &mstats->a, 0)
		CXGB_SYSCTL_ADD_ULONG(tx_fifo_parity_err);
		CXGB_SYSCTL_ADD_ULONG(rx_fifo_parity_err);
		CXGB_SYSCTL_ADD_ULONG(tx_fifo_urun);
		CXGB_SYSCTL_ADD_ULONG(rx_fifo_ovfl);
		CXGB_SYSCTL_ADD_ULONG(serdes_signal_loss);
		CXGB_SYSCTL_ADD_ULONG(xaui_pcs_ctc_err);
		CXGB_SYSCTL_ADD_ULONG(xaui_pcs_align_change);
		CXGB_SYSCTL_ADD_ULONG(num_toggled);
		CXGB_SYSCTL_ADD_ULONG(num_resets);
#undef CXGB_SYSCTL_ADD_ULONG
	}
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
