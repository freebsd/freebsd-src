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


#ifndef _IXL_H_
#define _IXL_H_


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

#ifdef IXL_DEBUG
#include <sys/sbuf.h>

#define MAC_FORMAT "%02x:%02x:%02x:%02x:%02x:%02x"
#define MAC_FORMAT_ARGS(mac_addr) \
	(mac_addr)[0], (mac_addr)[1], (mac_addr)[2], (mac_addr)[3], \
	(mac_addr)[4], (mac_addr)[5]
#define ON_OFF_STR(is_set) ((is_set) ? "On" : "Off")


#define _DBG_PRINTF(S, ...)		printf("%s: " S "\n", __func__, ##__VA_ARGS__)
#define _DEV_DBG_PRINTF(dev, S, ...)	device_printf(dev, "%s: " S "\n", __func__, ##__VA_ARGS__)
#define _IF_DBG_PRINTF(ifp, S, ...)	if_printf(ifp, "%s: " S "\n", __func__, ##__VA_ARGS__)

/* Defines for printing generic debug information */
#define DPRINTF(...)			_DBG_PRINTF(__VA_ARGS__)
#define DDPRINTF(...)			_DEV_DBG_PRINTF(__VA_ARGS__)
#define IDPRINTF(...)			_IF_DBG_PRINTF(__VA_ARGS__)

/* Defines for printing specific debug information */
#define DEBUG_INIT  1
#define DEBUG_IOCTL 1
#define DEBUG_HW    1

#define INIT_DEBUGOUT(...)		if (DEBUG_INIT) _DBG_PRINTF(__VA_ARGS__)
#define INIT_DBG_DEV(...)		if (DEBUG_INIT) _DEV_DBG_PRINTF(__VA_ARGS__)
#define INIT_DBG_IF(...)		if (DEBUG_INIT) _IF_DBG_PRINTF(__VA_ARGS__)

#define IOCTL_DEBUGOUT(...)		if (DEBUG_IOCTL) _DBG_PRINTF(__VA_ARGS__)
#define IOCTL_DBG_IF2(ifp, S, ...)	if (DEBUG_IOCTL) \
					    if_printf(ifp, S "\n", ##__VA_ARGS__)
#define IOCTL_DBG_IF(...)		if (DEBUG_IOCTL) _IF_DBG_PRINTF(__VA_ARGS__)

#define HW_DEBUGOUT(...)		if (DEBUG_HW) _DBG_PRINTF(__VA_ARGS__)

#else
#define DEBUG_INIT  0
#define DEBUG_IOCTL 0
#define DEBUG_HW    0

#define DPRINTF(...)
#define DDPRINTF(...)
#define IDPRINTF(...)

#define INIT_DEBUGOUT(...)
#define INIT_DBG_DEV(...)
#define INIT_DBG_IF(...)
#define IOCTL_DEBUGOUT(...)
#define IOCTL_DBG_IF2(...)
#define IOCTL_DBG_IF(...)
#define HW_DEBUGOUT(...)
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

/*
** Default number of entries in Tx queue buf_ring.
*/
#define DEFAULT_TXBRSZ	(4096 * 4096)

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
#define IXL_WATCHDOG                   (10 * hz)

/*
 * This parameters control when the driver calls the routine to reclaim
 * transmit descriptors.
 */
#define IXL_TX_CLEANUP_THRESHOLD	(que->num_desc / 8)
#define IXL_TX_OP_THRESHOLD		(que->num_desc / 32)

/* Flow control constants */
#define IXL_FC_PAUSE		0xFFFF
#define IXL_FC_HI		0x20000
#define IXL_FC_LO		0x10000

#define MAX_MULTICAST_ADDR	128

#define IXL_BAR		3
#define IXL_ADM_LIMIT		2
#define IXL_TSO_SIZE		65535
#define IXL_TX_BUF_SZ		((u32) 1514)
#define IXL_AQ_BUF_SZ		((u32) 4096)
#define IXL_RX_HDR		128
#define IXL_AQ_LEN		256
#define IXL_AQ_BUFSZ		4096
#define IXL_RX_LIMIT		512
#define IXL_RX_ITR		0
#define IXL_TX_ITR		1
#define IXL_ITR_NONE		3
#define IXL_QUEUE_EOL		0x7FF
#define IXL_MAX_FRAME		0x2600
#define IXL_MAX_TX_SEGS	8 
#define IXL_MAX_TSO_SEGS	66 
#define IXL_SPARSE_CHAIN	6
#define IXL_QUEUE_HUNG		0x80000000

/* ERJ: hardware can support ~1.5k filters between all functions */
#define IXL_MAX_FILTERS	256
#define IXL_MAX_TX_BUSY	10

#define IXL_NVM_VERSION_LO_SHIFT	0
#define IXL_NVM_VERSION_LO_MASK		(0xff << IXL_NVM_VERSION_LO_SHIFT)
#define IXL_NVM_VERSION_HI_SHIFT	12
#define IXL_NVM_VERSION_HI_MASK		(0xf << IXL_NVM_VERSION_HI_SHIFT)


/*
 * Interrupt Moderation parameters 
 */
#define IXL_MAX_ITR		0x07FF
#define IXL_ITR_100K		0x0005
#define IXL_ITR_20K		0x0019
#define IXL_ITR_8K		0x003E
#define IXL_ITR_4K		0x007A
#define IXL_ITR_DYNAMIC		0x8000
#define IXL_LOW_LATENCY		0
#define IXL_AVE_LATENCY		1
#define IXL_BULK_LATENCY	2

/* MacVlan Flags */
#define IXL_FILTER_USED		(u16)(1 << 0)
#define IXL_FILTER_VLAN		(u16)(1 << 1)
#define IXL_FILTER_ADD		(u16)(1 << 2)
#define IXL_FILTER_DEL		(u16)(1 << 3)
#define IXL_FILTER_MC		(u16)(1 << 4)

/* used in the vlan field of the filter when not a vlan */
#define IXL_VLAN_ANY		-1

#define CSUM_OFFLOAD_IPV4	(CSUM_IP|CSUM_TCP|CSUM_UDP|CSUM_SCTP)
#define CSUM_OFFLOAD_IPV6	(CSUM_TCP_IPV6|CSUM_UDP_IPV6|CSUM_SCTP_IPV6)
#define CSUM_OFFLOAD		(CSUM_OFFLOAD_IPV4|CSUM_OFFLOAD_IPV6|CSUM_TSO)

/* Misc flags for ixl_vsi.flags */
#define IXL_FLAGS_KEEP_TSO4	(1 << 0)
#define IXL_FLAGS_KEEP_TSO6	(1 << 1)

#define IXL_TX_LOCK(_sc)                mtx_lock(&(_sc)->mtx)
#define IXL_TX_UNLOCK(_sc)              mtx_unlock(&(_sc)->mtx)
#define IXL_TX_LOCK_DESTROY(_sc)        mtx_destroy(&(_sc)->mtx)
#define IXL_TX_TRYLOCK(_sc)             mtx_trylock(&(_sc)->mtx)
#define IXL_TX_LOCK_ASSERT(_sc)         mtx_assert(&(_sc)->mtx, MA_OWNED)

#define IXL_RX_LOCK(_sc)                mtx_lock(&(_sc)->mtx)
#define IXL_RX_UNLOCK(_sc)              mtx_unlock(&(_sc)->mtx)
#define IXL_RX_LOCK_DESTROY(_sc)        mtx_destroy(&(_sc)->mtx)

/*
 *****************************************************************************
 * vendor_info_array
 * 
 * This array contains the list of Subvendor/Subdevice IDs on which the driver
 * should load.
 * 
 *****************************************************************************
 */
typedef struct _ixl_vendor_info_t {
	unsigned int    vendor_id;
	unsigned int    device_id;
	unsigned int    subvendor_id;
	unsigned int    subdevice_id;
	unsigned int    index;
} ixl_vendor_info_t;


struct ixl_tx_buf {
	u32		eop_index;
	struct mbuf	*m_head;
	bus_dmamap_t	map;
	bus_dma_tag_t	tag;
};

struct ixl_rx_buf {
	struct mbuf	*m_head;
	struct mbuf	*m_pack;
	struct mbuf	*fmp;
	bus_dmamap_t	hmap;
	bus_dmamap_t	pmap;
#ifdef DEV_NETMAP
	u64		addr;
#endif
};

/*
** This struct has multiple uses, multicast
** addresses, vlans, and mac filters all use it.
*/
struct ixl_mac_filter {
	SLIST_ENTRY(ixl_mac_filter) next;
	u8	macaddr[ETHER_ADDR_LEN];
	s16	vlan;
	u16	flags;
};


/*
 * The Transmit ring control struct
 */
struct tx_ring {
        struct ixl_queue	*que;
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
	struct ixl_tx_buf	*buffers;
	volatile u16		avail;
	u32			cmd;
	bus_dma_tag_t		tx_tag;
	bus_dma_tag_t		tso_tag;
	char			mtx_name[16];
	struct buf_ring		*br;

	/* Used for Dynamic ITR calculation */
	u32			packets;
	u32 			bytes;

	/* Soft Stats */
	u64			tx_bytes;
	u64			no_desc;
	u64			total_packets;
};


/*
 * The Receive ring control struct
 */
struct rx_ring {
        struct ixl_queue	*que;
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
	struct ixl_rx_buf	*buffers;
	u32			mbuf_sz;
	u32			tail;
	bus_dma_tag_t		htag;
	bus_dma_tag_t		ptag;

	/* Used for Dynamic ITR calculation */
	u32			packets;
	u32 			bytes;

	/* Soft stats */
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
struct ixl_queue {
	struct ixl_vsi		*vsi;
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
SLIST_HEAD(ixl_ftl_head, ixl_mac_filter);
struct ixl_vsi {
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
	struct ixl_queue	*queues;	/* head of queues */
	bool			link_active;
	u16			seid;
	u16			max_frame_size;
	u32			link_speed;
	bool			link_up;
	u32			fc; /* local flow ctrl setting */

	/* MAC/VLAN Filter list */
	struct ixl_ftl_head ftl;

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
ixl_rx_unrefreshed(struct ixl_queue *que)
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
static inline struct ixl_mac_filter *
ixl_get_filter(struct ixl_vsi *vsi)
{
	struct ixl_mac_filter  *f;

	/* create a new empty filter */
	f = malloc(sizeof(struct ixl_mac_filter),
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
struct ixl_sysctl_info {
	u64	*stat;
	char	*name;
	char	*description;
};

extern int ixl_atr_rate;

/*
** ixl_fw_version_str - format the FW and NVM version strings
*/
static inline char *
ixl_fw_version_str(struct i40e_hw *hw)
{
	static char buf[32];

	snprintf(buf, sizeof(buf),
	    "f%d.%d a%d.%d n%02x.%02x e%08x",
	    hw->aq.fw_maj_ver, hw->aq.fw_min_ver,
	    hw->aq.api_maj_ver, hw->aq.api_min_ver,
	    (hw->nvm.version & IXL_NVM_VERSION_HI_MASK) >>
	    IXL_NVM_VERSION_HI_SHIFT,
	    (hw->nvm.version & IXL_NVM_VERSION_LO_MASK) >>
	    IXL_NVM_VERSION_LO_SHIFT,
	    hw->nvm.eetrack);
	return buf;
}

/*********************************************************************
 *  TXRX Function prototypes
 *********************************************************************/
int	ixl_allocate_tx_data(struct ixl_queue *);
int	ixl_allocate_rx_data(struct ixl_queue *);
void	ixl_init_tx_ring(struct ixl_queue *);
int	ixl_init_rx_ring(struct ixl_queue *);
bool	ixl_rxeof(struct ixl_queue *, int);
bool	ixl_txeof(struct ixl_queue *);
int	ixl_mq_start(struct ifnet *, struct mbuf *);
int	ixl_mq_start_locked(struct ifnet *, struct tx_ring *);
void	ixl_deferred_mq_start(void *, int);
void	ixl_qflush(struct ifnet *);
void	ixl_free_vsi(struct ixl_vsi *);
void	ixl_free_que_tx(struct ixl_queue *);
void	ixl_free_que_rx(struct ixl_queue *);
#ifdef IXL_FDIR
void	ixl_atr(struct ixl_queue *, struct tcphdr *, int);
#endif

#endif /* _IXL_H_ */
