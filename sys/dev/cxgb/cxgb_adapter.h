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



#ifndef _CXGB_ADAPTER_H_
#define _CXGB_ADAPTER_H_

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sx.h>
#include <sys/rman.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_media.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus_dma.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#ifdef CONFIG_DEFINED
#include <cxgb_osdep.h>
#include <ulp/toecore/toedev.h>
#include <sys/mbufq.h>
#else
#include <dev/cxgb/cxgb_osdep.h>
#include <dev/cxgb/sys/mbufq.h>
#include <dev/cxgb/ulp/toecore/toedev.h>
#endif

#define USE_SX

struct adapter;
struct sge_qset;
extern int cxgb_debug;

#ifdef DEBUG_LOCKING
#define MTX_INIT(lock, lockname, class, flags) \
	do { \
		printf("initializing %s at %s:%d\n", lockname, __FILE__, __LINE__); \
		mtx_init((lock), lockname, class, flags);		\
	} while (0)

#define MTX_DESTROY(lock) \
	do { \
		printf("destroying %s at %s:%d\n", (lock)->lock_object.lo_name, __FILE__, __LINE__); \
		mtx_destroy((lock));					\
	} while (0)

#define SX_INIT(lock, lockname) \
	do { \
		printf("initializing %s at %s:%d\n", lockname, __FILE__, __LINE__); \
		sx_init((lock), lockname);		\
	} while (0)

#define SX_DESTROY(lock) \
	do { \
		printf("destroying %s at %s:%d\n", (lock)->lock_object.lo_name, __FILE__, __LINE__); \
		sx_destroy((lock));					\
	} while (0)
#else
#define MTX_INIT mtx_init
#define MTX_DESTROY mtx_destroy
#define SX_INIT sx_init
#define SX_DESTROY sx_destroy
#endif

struct port_info {
	struct adapter	*adapter;
	struct ifnet	*ifp;
	int		if_flags;
	const struct port_type_info *port_type;
	struct cphy	phy;
	struct cmac	mac;
	struct link_config link_config;
	struct ifmedia	media;
#ifdef USE_SX	
	struct sx	lock;
#else	
	struct mtx	lock;
#endif	
	int		port_id;
	uint8_t		hw_addr[ETHER_ADDR_LEN];
	uint8_t		nqsets;
	uint8_t         first_qset;
	struct taskqueue *tq;
	struct task     start_task;
	struct task	timer_reclaim_task;
	struct cdev     *port_cdev;

#define PORT_NAME_LEN 32
#define TASKQ_NAME_LEN 32
	char            lockbuf[PORT_NAME_LEN];
	char            taskqbuf[TASKQ_NAME_LEN];
};

enum {				/* adapter flags */
	FULL_INIT_DONE	= (1 << 0),
	USING_MSI	= (1 << 1),
	USING_MSIX	= (1 << 2),
	QUEUES_BOUND	= (1 << 3),
	FW_UPTODATE     = (1 << 4),
	TPS_UPTODATE    = (1 << 5),
};


#define FL_Q_SIZE	4096
#define JUMBO_Q_SIZE	512
#define RSPQ_Q_SIZE	1024
#define TX_ETH_Q_SIZE	1024



/*
 * Types of Tx queues in each queue set.  Order here matters, do not change.
 * XXX TOE is not implemented yet, so the extra queues are just placeholders.
 */
enum { TXQ_ETH, TXQ_OFLD, TXQ_CTRL };


/* careful, the following are set on priv_flags and must not collide with
 * IFF_ flags!
 */
enum {
	LRO_ACTIVE = (1 << 8),
};

/* Max concurrent LRO sessions per queue set */
#define MAX_LRO_SES 8

struct t3_lro_session {
	struct mbuf *head;
	struct mbuf *tail;
	uint32_t seq;
	uint16_t ip_len;
	uint16_t vtag;
	uint8_t npkts;
};

struct lro_state {
	unsigned short enabled;
	unsigned short active_idx;
	unsigned int nactive;
	struct t3_lro_session sess[MAX_LRO_SES];
};

#define RX_BUNDLE_SIZE 8

struct rsp_desc;

struct sge_rspq {
	uint32_t	credits;
	uint32_t	size;
	uint32_t	cidx;
	uint32_t	gen;
	uint32_t	polling;
	uint32_t	holdoff_tmr;
	uint32_t	next_holdoff;
	uint32_t        imm_data;
	struct rsp_desc	*desc;
	uint32_t	cntxt_id;
	struct mtx      lock;
	struct mbuf     *rx_head;    /* offload packet receive queue head */
	struct mbuf     *rx_tail;    /* offload packet receive queue tail */

	uint32_t        offload_pkts;
	uint32_t        offload_bundles;
	uint32_t        pure_rsps;
	uint32_t        unhandled_irqs;

	bus_addr_t	phys_addr;
	bus_dma_tag_t	desc_tag;
	bus_dmamap_t	desc_map;
	struct mbuf	*m;
#define RSPQ_NAME_LEN  32
	char            lockbuf[RSPQ_NAME_LEN];

};

struct rx_desc;
struct rx_sw_desc;

struct sge_fl {
	uint32_t	buf_size;
	uint32_t	credits;
	uint32_t	size;
	uint32_t	cidx;
	uint32_t	pidx;
	uint32_t	gen;
	struct rx_desc	*desc;
	struct rx_sw_desc *sdesc;
	bus_addr_t	phys_addr;
	uint32_t	cntxt_id;
	uint64_t	empty;
	bus_dma_tag_t	desc_tag;
	bus_dmamap_t	desc_map;
	bus_dma_tag_t   entry_tag;
	uma_zone_t      zone;
	int             type;
};

struct tx_desc;
struct tx_sw_desc;

#define TXQ_TRANSMITTING    0x1

struct sge_txq {
	uint64_t	flags;
	uint32_t	in_use;
	uint32_t	size;
	uint32_t	processed;
	uint32_t	cleaned;
	uint32_t	stop_thres;
	uint32_t	cidx;
	uint32_t	pidx;
	uint32_t	gen;
	uint32_t	unacked;
	struct tx_desc	*desc;
	struct tx_sw_desc *sdesc;
	uint32_t	token;
	bus_addr_t	phys_addr;
	struct task     qresume_task;
	struct task     qreclaim_task;
	struct port_info *port;
	uint32_t	cntxt_id;
	uint64_t	stops;
	uint64_t	restarts;
	bus_dma_tag_t	desc_tag;
	bus_dmamap_t	desc_map;
	bus_dma_tag_t   entry_tag;
	struct mbuf_head sendq;
	struct mtx      lock;
#define TXQ_NAME_LEN  32
	char            lockbuf[TXQ_NAME_LEN];
};
     	

enum {
	SGE_PSTAT_TSO,              /* # of TSO requests */
	SGE_PSTAT_RX_CSUM_GOOD,     /* # of successful RX csum offloads */
	SGE_PSTAT_TX_CSUM,          /* # of TX checksum offloads */
	SGE_PSTAT_VLANEX,           /* # of VLAN tag extractions */
	SGE_PSTAT_VLANINS,          /* # of VLAN tag insertions */
	SGE_PSTATS_LRO_QUEUED,	    /* # of LRO appended packets */
	SGE_PSTATS_LRO_FLUSHED,	    /* # of LRO flushed packets */
	SGE_PSTATS_LRO_X_STREAMS,   /* # of exceeded LRO contexts */
};

#define SGE_PSTAT_MAX (SGE_PSTATS_LRO_X_STREAMS+1)

struct sge_qset {
	struct sge_rspq		rspq;
	struct sge_fl		fl[SGE_RXQ_PER_SET];
	struct lro_state        lro;
	struct sge_txq		txq[SGE_TXQ_PER_SET];
	uint32_t                txq_stopped;       /* which Tx queues are stopped */
	uint64_t                port_stats[SGE_PSTAT_MAX];
	struct port_info        *port;
	int                     idx; /* qset # */
};

struct sge {
	struct sge_qset	        qs[SGE_QSETS];
	struct mtx              reg_lock;
};

struct filter_info;

struct adapter {
	device_t		dev;
	int			flags;
	TAILQ_ENTRY(adapter)    adapter_entry;
	
	/* PCI register resources */
	int			regs_rid;
	struct resource		*regs_res;
	bus_space_handle_t	bh;
	bus_space_tag_t		bt;
	bus_size_t              mmio_len;
	uint32_t                link_width;
	
	/* DMA resources */
	bus_dma_tag_t		parent_dmat;
	bus_dma_tag_t		rx_dmat;
	bus_dma_tag_t		rx_jumbo_dmat;
	bus_dma_tag_t		tx_dmat;

	/* Interrupt resources */
	struct resource		*irq_res;
	int			irq_rid;
	void			*intr_tag;

	uint32_t		msix_regs_rid;
	struct resource		*msix_regs_res;

	struct resource		*msix_irq_res[SGE_QSETS];
	int			msix_irq_rid[SGE_QSETS];
	void			*msix_intr_tag[SGE_QSETS];
	uint8_t                 rxpkt_map[8]; /* maps RX_PKT interface values to port ids */
	uint8_t                 rrss_map[SGE_QSETS]; /* revers RSS map table */

	struct filter_info      *filters;
	
	/* Tasks */
	struct task		ext_intr_task;
	struct task		slow_intr_task;
	struct task		tick_task;
	struct task		process_responses_task;
	struct taskqueue	*tq;
	struct callout		cxgb_tick_ch;
	struct callout		sge_timer_ch;

	/* Register lock for use by the hardware layer */
	struct mtx		mdio_lock;
	struct mtx		elmer_lock;

	/* Bookkeeping for the hardware layer */
	struct adapter_params  params;
	unsigned int slow_intr_mask;
	unsigned long irq_stats[IRQ_NUM_STATS];

	struct sge              sge;
	struct mc7              pmrx;
	struct mc7              pmtx;
	struct mc7              cm;
	struct mc5              mc5;

	struct port_info	port[MAX_NPORTS];
	device_t		portdev[MAX_NPORTS];
	struct toedev           tdev;
	char                    fw_version[64];
	uint32_t                open_device_map;
	uint32_t                registered_device_map;
#ifdef USE_SX
	struct sx               lock;
#else	
	struct mtx              lock;
#endif	
	driver_intr_t           *cxgb_intr;
	int                     msi_count;

#define ADAPTER_LOCK_NAME_LEN	32
	char                    lockbuf[ADAPTER_LOCK_NAME_LEN];
	char                    reglockbuf[ADAPTER_LOCK_NAME_LEN];
	char                    mdiolockbuf[ADAPTER_LOCK_NAME_LEN];
	char                    elmerlockbuf[ADAPTER_LOCK_NAME_LEN];
};

struct t3_rx_mode {
	
	uint32_t                idx;
	struct port_info        *port;
};


#define MDIO_LOCK(adapter)	mtx_lock(&(adapter)->mdio_lock)
#define MDIO_UNLOCK(adapter)	mtx_unlock(&(adapter)->mdio_lock)
#define ELMR_LOCK(adapter)	mtx_lock(&(adapter)->elmer_lock)
#define ELMR_UNLOCK(adapter)	mtx_unlock(&(adapter)->elmer_lock)


#ifdef USE_SX
#define PORT_LOCK(port)		     sx_xlock(&(port)->lock);
#define PORT_UNLOCK(port)	     sx_xunlock(&(port)->lock);
#define PORT_LOCK_INIT(port, name)   SX_INIT(&(port)->lock, name)
#define PORT_LOCK_DEINIT(port)       SX_DESTROY(&(port)->lock)
#define PORT_LOCK_ASSERT_OWNED(port) sx_assert(&(port)->lock, SA_LOCKED)

#define ADAPTER_LOCK(adap)	           sx_xlock(&(adap)->lock);
#define ADAPTER_UNLOCK(adap)	           sx_xunlock(&(adap)->lock);
#define ADAPTER_LOCK_INIT(adap, name)      SX_INIT(&(adap)->lock, name)
#define ADAPTER_LOCK_DEINIT(adap)          SX_DESTROY(&(adap)->lock)
#define ADAPTER_LOCK_ASSERT_NOTOWNED(adap) sx_assert(&(adap)->lock, SA_UNLOCKED)
#else
#define PORT_LOCK(port)		     mtx_lock(&(port)->lock);
#define PORT_UNLOCK(port)	     mtx_unlock(&(port)->lock);
#define PORT_LOCK_INIT(port, name)   mtx_init(&(port)->lock, name, 0, MTX_DEF)
#define PORT_LOCK_DEINIT(port)       mtx_destroy(&(port)->lock)
#define PORT_LOCK_ASSERT_OWNED(port) mtx_assert(&(port)->lock, MA_OWNED)

#define ADAPTER_LOCK(adap)	mtx_lock(&(adap)->lock);
#define ADAPTER_UNLOCK(adap)	mtx_unlock(&(adap)->lock);
#define ADAPTER_LOCK_INIT(adap, name) mtx_init(&(adap)->lock, name, 0, MTX_DEF)
#define ADAPTER_LOCK_DEINIT(adap) mtx_destroy(&(adap)->lock)
#define ADAPTER_LOCK_ASSERT_NOTOWNED(adap) mtx_assert(&(adap)->lock, MO_NOTOWNED)
#endif


static __inline uint32_t
t3_read_reg(adapter_t *adapter, uint32_t reg_addr)
{
	return (bus_space_read_4(adapter->bt, adapter->bh, reg_addr));
}

static __inline void
t3_write_reg(adapter_t *adapter, uint32_t reg_addr, uint32_t val)
{
	bus_space_write_4(adapter->bt, adapter->bh, reg_addr, val);
}

static __inline void
t3_os_pci_read_config_4(adapter_t *adapter, int reg, uint32_t *val)
{
	*val = pci_read_config(adapter->dev, reg, 4);
}

static __inline void
t3_os_pci_write_config_4(adapter_t *adapter, int reg, uint32_t val)
{
	pci_write_config(adapter->dev, reg, val, 4);
}

static __inline void
t3_os_pci_read_config_2(adapter_t *adapter, int reg, uint16_t *val)
{
	*val = pci_read_config(adapter->dev, reg, 2);
}

static __inline void
t3_os_pci_write_config_2(adapter_t *adapter, int reg, uint16_t val)
{
	pci_write_config(adapter->dev, reg, val, 2);
}

static __inline uint8_t *
t3_get_next_mcaddr(struct t3_rx_mode *rm)
{
	uint8_t *macaddr = NULL;
	
	if (rm->idx == 0)
		macaddr = rm->port->hw_addr;

	rm->idx++;
	return (macaddr);
}

static __inline void
t3_init_rx_mode(struct t3_rx_mode *rm, struct port_info *port)
{
	rm->idx = 0;
	rm->port = port;
}

static __inline struct port_info *
adap2pinfo(struct adapter *adap, int idx)
{
	return &adap->port[idx];
}

int t3_os_find_pci_capability(adapter_t *adapter, int cap);
int t3_os_pci_save_state(struct adapter *adapter);
int t3_os_pci_restore_state(struct adapter *adapter);
void t3_os_link_changed(adapter_t *adapter, int port_id, int link_status,
			int speed, int duplex, int fc);
void t3_sge_err_intr_handler(adapter_t *adapter);
int t3_offload_tx(struct toedev *, struct mbuf *);
void t3_os_ext_intr_handler(adapter_t *adapter);
void t3_os_set_hw_addr(adapter_t *adapter, int port_idx, u8 hw_addr[]);
int t3_mgmt_tx(adapter_t *adap, struct mbuf *m);


int t3_sge_alloc(struct adapter *);
int t3_sge_free(struct adapter *);
int t3_sge_alloc_qset(adapter_t *, uint32_t, int, int, const struct qset_params *,
    int, struct port_info *);
void t3_free_sge_resources(adapter_t *);
void t3_sge_start(adapter_t *);
void t3_sge_stop(adapter_t *);
void t3b_intr(void *data);
void t3_intr_msi(void *data);
void t3_intr_msix(void *data);
int t3_encap(struct port_info *, struct mbuf **);

int t3_sge_init_adapter(adapter_t *);
int t3_sge_init_port(struct port_info *);
void t3_sge_deinit_sw(adapter_t *);

void t3_rx_eth_lro(adapter_t *adap, struct sge_rspq *rq, struct mbuf *m,
    int ethpad, uint32_t rss_hash, uint32_t rss_csum, int lro);
void t3_rx_eth(struct port_info *p, struct sge_rspq *rq, struct mbuf *m, int ethpad);
void t3_lro_flush(adapter_t *adap, struct sge_qset *qs, struct lro_state *state);

void t3_add_sysctls(adapter_t *sc);
int t3_get_desc(const struct sge_qset *qs, unsigned int qnum, unsigned int idx,
    unsigned char *data);
void t3_update_qset_coalesce(struct sge_qset *qs, const struct qset_params *p);
/*
 * XXX figure out how we can return this to being private to sge
 */
#define desc_reclaimable(q) ((int)((q)->processed - (q)->cleaned - TX_MAX_DESC))

#define container_of(p, stype, field) ((stype *)(((uint8_t *)(p)) - offsetof(stype, field))) 

static __inline struct sge_qset *
fl_to_qset(struct sge_fl *q, int qidx)
{
	return container_of(q, struct sge_qset, fl[qidx]);
}

static __inline struct sge_qset *
rspq_to_qset(struct sge_rspq *q)
{
	return container_of(q, struct sge_qset, rspq);
}

static __inline struct sge_qset *
txq_to_qset(struct sge_txq *q, int qidx)
{
	return container_of(q, struct sge_qset, txq[qidx]);
}

static __inline struct adapter *
tdev2adap(struct toedev *d)
{
	return container_of(d, struct adapter, tdev);
}

#undef container_of

#define OFFLOAD_DEVMAP_BIT 15
static inline int offload_running(adapter_t *adapter)
{
        return isset(&adapter->open_device_map, OFFLOAD_DEVMAP_BIT);
}


#endif
