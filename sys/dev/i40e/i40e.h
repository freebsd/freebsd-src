/******************************************************************************

  Copyright (c) 2013-2014, Intel Corporation 
  All rights reserved.
  
  Redistribution and use in source and binary forms, with or without 
  modification, are permitted provided that the following conditions are met:
  
   1. Redistributions of source code must retain the above copyright notice, 
      this list of conditions and the following disclaimer.
  
   2. Redistributions in binary form must reproduce the above copyright 
      notice, this list of conditions and the following disclaimer in the 
      documentation and/or other materials provided with the distribution.
  
   3. Neither the name of the Intel Corporation nor the names of its 
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

******************************************************************************/
/*$FreeBSD$*/


#ifndef _I40E_H_
#define _I40E_H_


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf_ring.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/sockio.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/bpf.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <net/bpf.h>
#include <net/if_types.h>
#include <net/if_vlan_var.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/tcp.h>
#include <netinet/tcp_lro.h>
#include <netinet/udp.h>
#include <netinet/sctp.h>

#include <machine/in_cksum.h>

#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/clock.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/endian.h>
#include <sys/taskqueue.h>
#include <sys/pcpu.h>
#include <sys/smp.h>
#include <machine/smp.h>

#include "i40e_type.h"
#include "i40e_prototype.h"

#ifdef I40E_DEBUG
#include <sys/sbuf.h>

#define MAC_FORMAT "%02x:%02x:%02x:%02x:%02x:%02x"
#define MAC_FORMAT_ARGS(mac_addr) \
	(mac_addr)[0], (mac_addr)[1], (mac_addr)[2], (mac_addr)[3], \
	(mac_addr)[4], (mac_addr)[5]
#define ON_OFF_STR(is_set) ((is_set) ? "On" : "Off")

#define DPRINTF(...)		printf(__VA_ARGS__)
#define DDPRINTF(dev, ...)	device_printf(dev, __VA_ARGS__)
#define IDPRINTF(ifp, ...)	if_printf(ifp, __VA_ARGS__)

// static void	i40e_dump_desc(void *, u8, u16);
#else
#define DPRINTF(...)
#define DDPRINTF(...)
#define IDPRINTF(...)
#endif

/* Tunables */

/*
 * Ring Descriptors Valid Range: 32-4096 Default Value: 1024 This value is the
 * number of tx/rx descriptors allocated by the driver. Increasing this
 * value allows the driver to queue more operations. Each descriptor is 16
 * or 32 bytes (configurable in FVL)
 */
#define DEFAULT_RING	1024
#define PERFORM_RING	2048
#define MAX_RING	4096
#define MIN_RING	32

/* Alignment for rings */
#define DBA_ALIGN	128

/*
 * This parameter controls the maximum no of times the driver will loop in
 * the isr. Minimum Value = 1
 */
#define MAX_LOOP	10

/*
 * This is the max watchdog interval, ie. the time that can
 * pass between any two TX clean operations, such only happening
 * when the TX hardware is functioning.
 */
#define I40E_WATCHDOG                   (10 * hz)

/*
 * This parameters control when the driver calls the routine to reclaim
 * transmit descriptors.
 */
#define I40E_TX_CLEANUP_THRESHOLD	(que->num_desc / 8)
#define I40E_TX_OP_THRESHOLD		(que->num_desc / 32)

/* Flow control constants */
#define I40E_FC_PAUSE		0xFFFF
#define I40E_FC_HI		0x20000
#define I40E_FC_LO		0x10000

/* Defines for printing debug information */
#define DEBUG_INIT  0
#define DEBUG_IOCTL 0
#define DEBUG_HW    0

#define INIT_DEBUGOUT(S)            if (DEBUG_INIT)  printf(S "\n")
#define INIT_DEBUGOUT1(S, A)        if (DEBUG_INIT)  printf(S "\n", A)
#define INIT_DEBUGOUT2(S, A, B)     if (DEBUG_INIT)  printf(S "\n", A, B)
#define IOCTL_DEBUGOUT(S)           if (DEBUG_IOCTL) printf(S "\n")
#define IOCTL_DEBUGOUT1(S, A)       if (DEBUG_IOCTL) printf(S "\n", A)
#define IOCTL_DEBUGOUT2(S, A, B)    if (DEBUG_IOCTL) printf(S "\n", A, B)
#define HW_DEBUGOUT(S)              if (DEBUG_HW) printf(S "\n")
#define HW_DEBUGOUT1(S, A)          if (DEBUG_HW) printf(S "\n", A)
#define HW_DEBUGOUT2(S, A, B)       if (DEBUG_HW) printf(S "\n", A, B)

#define MAX_MULTICAST_ADDR	128

#define I40E_BAR		3
#define I40E_ADM_LIMIT		2
#define I40E_TSO_SIZE		65535
#define I40E_TX_BUF_SZ		((u32) 1514)
#define I40E_AQ_BUF_SZ		((u32) 4096)
#define I40E_RX_HDR		128
#define I40E_AQ_LEN		32
#define I40E_AQ_BUFSZ		4096
#define I40E_RX_LIMIT		512
#define I40E_RX_ITR		0
#define I40E_TX_ITR		1
#define I40E_ITR_NONE		3
#define I40E_QUEUE_EOL		0x7FF
#define I40E_MAX_FRAME		0x2600
#define I40E_MAX_TX_SEGS	8 
#define I40E_MAX_TSO_SEGS	66 
#define I40E_SPARSE_CHAIN	6
#define I40E_QUEUE_HUNG		0x80000000

/* ERJ: hardware can support ~1.5k filters between all functions */
#define I40E_MAX_FILTERS	256
#define I40E_MAX_TX_BUSY	10

#define I40E_NVM_VERSION_LO_SHIFT	0
#define I40E_NVM_VERSION_LO_MASK	(0xff << I40E_NVM_VERSION_LO_SHIFT)
#define I40E_NVM_VERSION_HI_SHIFT	12
#define I40E_NVM_VERSION_HI_MASK	(0xf << I40E_NVM_VERSION_HI_SHIFT)


/*
 * Interrupt Moderation parameters 
 */
#define I40E_MAX_ITR		0x07FF
#define I40E_ITR_100K		0x0005
#define I40E_ITR_20K		0x0019
#define I40E_ITR_8K		0x003E
#define I40E_ITR_4K		0x007A
#define I40E_ITR_DYNAMIC	0x8000
#define I40E_LOW_LATENCY	0
#define I40E_AVE_LATENCY	1
#define I40E_BULK_LATENCY	2

/* MacVlan Flags */
#define I40E_FILTER_USED	(u16)(1 << 0)
#define I40E_FILTER_VLAN	(u16)(1 << 1)
#define I40E_FILTER_ADD		(u16)(1 << 2)
#define I40E_FILTER_DEL		(u16)(1 << 3)
#define I40E_FILTER_MC		(u16)(1 << 4)

/* used in the vlan field of the filter when not a vlan */
#define I40E_VLAN_ANY		-1

#define CSUM_OFFLOAD_IPV4	(CSUM_IP|CSUM_TCP|CSUM_UDP|CSUM_SCTP)
#define CSUM_OFFLOAD_IPV6	(CSUM_TCP_IPV6|CSUM_UDP_IPV6|CSUM_SCTP_IPV6)
#define CSUM_OFFLOAD		(CSUM_OFFLOAD_IPV4|CSUM_OFFLOAD_IPV6|CSUM_TSO)

/* Misc flags for i40e_vsi.flags */
#define I40E_FLAGS_KEEP_TSO4	(1 << 0)
#define I40E_FLAGS_KEEP_TSO6	(1 << 1)

#define I40E_TX_LOCK(_sc)                mtx_lock(&(_sc)->mtx)
#define I40E_TX_UNLOCK(_sc)              mtx_unlock(&(_sc)->mtx)
#define I40E_TX_LOCK_DESTROY(_sc)        mtx_destroy(&(_sc)->mtx)
#define I40E_TX_TRYLOCK(_sc)             mtx_trylock(&(_sc)->mtx)
#define I40E_TX_LOCK_ASSERT(_sc)         mtx_assert(&(_sc)->mtx, MA_OWNED)

#define I40E_RX_LOCK(_sc)                mtx_lock(&(_sc)->mtx)
#define I40E_RX_UNLOCK(_sc)              mtx_unlock(&(_sc)->mtx)
#define I40E_RX_LOCK_DESTROY(_sc)        mtx_destroy(&(_sc)->mtx)

/*
 *****************************************************************************
 * vendor_info_array
 * 
 * This array contains the list of Subvendor/Subdevice IDs on which the driver
 * should load.
 * 
 *****************************************************************************
 */
typedef struct _i40e_vendor_info_t {
	unsigned int    vendor_id;
	unsigned int    device_id;
	unsigned int    subvendor_id;
	unsigned int    subdevice_id;
	unsigned int    index;
} i40e_vendor_info_t;


struct i40e_tx_buf {
	u32		eop_index;
	struct mbuf	*m_head;
	bus_dmamap_t	map;
	bus_dma_tag_t	tag;
};

struct i40e_rx_buf {
	struct mbuf	*m_head;
	struct mbuf	*m_pack;
	struct mbuf	*fmp;
	bus_dmamap_t	hmap;
	bus_dmamap_t	pmap;
};

/*
** This struct has multiple uses, multicast
** addresses, vlans, and mac filters all use it.
*/
struct i40e_mac_filter {
	SLIST_ENTRY(i40e_mac_filter) next;
	u8	macaddr[ETHER_ADDR_LEN];
	s16	vlan;
	u16	flags;
};


/*
 * The Transmit ring control struct
 */
struct tx_ring {
        struct i40e_queue	*que;
	struct mtx		mtx;
	u32			tail;
	struct i40e_tx_desc	*base;
	struct i40e_dma_mem	dma;
	u16			next_avail;
	u16			next_to_clean;
	u16			atr_rate;
	u16			atr_count;
	u16			itr;
	u16			latency;
	struct i40e_tx_buf	*buffers;
	volatile u16		avail;
	u32			cmd;
	bus_dma_tag_t		tx_tag;
	bus_dma_tag_t		tso_tag;
	char			mtx_name[16];
	struct buf_ring		*br;

	/* Soft Stats */
	u32			packets;
	u32 			bytes;
	u64			no_desc;
	u64			total_packets;
};


/*
 * The Receive ring control struct
 */
struct rx_ring {
        struct i40e_queue	*que;
	struct mtx		mtx;
	union i40e_rx_desc	*base;
	struct i40e_dma_mem	dma;
	struct lro_ctrl		lro;
	bool			lro_enabled;
	bool			hdr_split;
	bool			discard;
        u16			next_refresh;
        u16 			next_check;
	u16			itr;
	u16			latency;
	char			mtx_name[16];
	struct i40e_rx_buf	*buffers;
	u32			mbuf_sz;
	u32			tail;
	bus_dma_tag_t		htag;
	bus_dma_tag_t		ptag;

	/* Soft stats */
	u32			packets;
	u32 			bytes;

	u64			split;
	u64			rx_packets;
	u64 			rx_bytes;
	u64 			discarded;
	u64 			not_done;
};

/*
** Driver queue struct: this is the interrupt container
**  for the associated tx and rx ring pair.
*/
struct i40e_queue {
	struct i40e_vsi		*vsi;
	u32			me;
	u32			msix;           /* This queue's MSIX vector */
	u32			eims;           /* This queue's EIMS bit */
	struct resource		*res;
	void			*tag;
	int			num_desc;	/* both tx and rx */
	int			busy;
	struct tx_ring		txr;
	struct rx_ring		rxr;
	struct task		task;
	struct task		tx_task;
	struct taskqueue	*tq;

	/* Queue stats */
	u64			irqs;
	u64			tso;
	u64			mbuf_defrag_failed;
	u64			mbuf_hdr_failed;
	u64			mbuf_pkt_failed;
	u64			tx_map_avail;
	u64			tx_dma_setup;
	u64			dropped_pkts;
};

/*
** Virtual Station interface: 
**	there would be one of these per traffic class/type
**	for now just one, and its embedded in the pf
*/
SLIST_HEAD(i40e_ftl_head, i40e_mac_filter);
struct i40e_vsi {
	void 			*back;
	struct ifnet		*ifp;
	struct device		*dev;
	struct i40e_hw		*hw;
	struct ifmedia		media;
	u64			que_mask;
	int			id;
	u16			msix_base;	/* station base MSIX vector */
	u16			num_queues;
	u16			rx_itr_setting;
	u16			tx_itr_setting;
	struct i40e_queue	*queues;	/* head of queues */
	bool			link_active;
	u16			seid;
	u16			max_frame_size;
	u32			link_speed;
	bool			link_up;
	u32			fc; /* local flow ctrl setting */

	/* MAC/VLAN Filter list */
	struct i40e_ftl_head ftl;

	struct i40e_aqc_vsi_properties_data info;

	eventhandler_tag 	vlan_attach;
	eventhandler_tag 	vlan_detach;
	u16			num_vlans;

	/* Per-VSI stats from hardware */
	struct i40e_eth_stats	eth_stats;
	struct i40e_eth_stats	eth_stats_offsets;
	bool 			stat_offsets_loaded;

	/* Driver statistics */
	u64			hw_filters_del;
	u64			hw_filters_add;

	/* Misc. */
	u64 			active_queues;
	u64 			flags;
};

/*
** Find the number of unrefreshed RX descriptors
*/
static inline u16
i40e_rx_unrefreshed(struct i40e_queue *que)
{       
        struct rx_ring	*rxr = &que->rxr;
        
	if (rxr->next_check > rxr->next_refresh)
		return (rxr->next_check - rxr->next_refresh - 1);
	else
		return ((que->num_desc + rxr->next_check) -
		    rxr->next_refresh - 1);
}       

/*
** Find the next available unused filter
*/
static inline struct i40e_mac_filter *
i40e_get_filter(struct i40e_vsi *vsi)
{
	struct i40e_mac_filter  *f;

	/* create a new empty filter */
	f = malloc(sizeof(struct i40e_mac_filter),
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	SLIST_INSERT_HEAD(&vsi->ftl, f, next);

	return (f);
}

/*
** Compare two ethernet addresses
*/
static inline bool
cmp_etheraddr(u8 *ea1, u8 *ea2)
{       
	bool cmp = FALSE;

	if ((ea1[0] == ea2[0]) && (ea1[1] == ea2[1]) &&
	    (ea1[2] == ea2[2]) && (ea1[3] == ea2[3]) &&
	    (ea1[4] == ea2[4]) && (ea1[5] == ea2[5])) 
		cmp = TRUE;

	return (cmp);
}       

/*
 * Info for stats sysctls
 */
struct i40e_sysctl_info {
	u64	*stat;
	char	*name;
	char	*description;
};

extern int i40e_atr_rate;

/*
** i40e_fw_version_str - format the FW and NVM version strings
*/
static inline char *
i40e_fw_version_str(struct i40e_hw *hw)
{
	static char buf[32];

	snprintf(buf, sizeof(buf),
	    "f%d.%d a%d.%d n%02x.%02x e%08x",
	    hw->aq.fw_maj_ver, hw->aq.fw_min_ver,
	    hw->aq.api_maj_ver, hw->aq.api_min_ver,
	    (hw->nvm.version & I40E_NVM_VERSION_HI_MASK) >>
	    I40E_NVM_VERSION_HI_SHIFT,
	    (hw->nvm.version & I40E_NVM_VERSION_LO_MASK) >>
	    I40E_NVM_VERSION_LO_SHIFT,
	    hw->nvm.eetrack);
	return buf;
}

/*********************************************************************
 *  TXRX Function prototypes
 *********************************************************************/
int	i40e_allocate_tx_data(struct i40e_queue *);
int	i40e_allocate_rx_data(struct i40e_queue *);
void	i40e_init_tx_ring(struct i40e_queue *);
int	i40e_init_rx_ring(struct i40e_queue *);
bool	i40e_rxeof(struct i40e_queue *, int);
bool	i40e_txeof(struct i40e_queue *);
int	i40e_mq_start(struct ifnet *, struct mbuf *);
int	i40e_mq_start_locked(struct ifnet *, struct tx_ring *);
void	i40e_deferred_mq_start(void *, int);
void	i40e_qflush(struct ifnet *);
void	i40e_free_vsi(struct i40e_vsi *);
void	i40e_free_que_tx(struct i40e_queue *);
void	i40e_free_que_rx(struct i40e_queue *);
#ifdef I40E_FDIR
void	i40e_atr(struct i40e_queue *, struct tcphdr *, int);
#endif

#endif /* _I40E_H_ */
