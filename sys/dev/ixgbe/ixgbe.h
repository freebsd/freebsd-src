/******************************************************************************

  Copyright (c) 2001-2008, Intel Corporation 
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
/*$FreeBSD: src/sys/dev/ixgbe/ixgbe.h,v 1.3.2.2.2.2 2008/11/30 04:44:46 jfv Exp $*/


#ifndef _IXGBE_H_
#define _IXGBE_H_


#include <sys/param.h>
#include <sys/systm.h>
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
#include <netinet/udp.h>

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

#include "ixgbe_api.h"
#include "tcp_lro.h"

/* Tunables */

/*
 * TxDescriptors Valid Range: 64-4096 Default Value: 256 This value is the
 * number of transmit descriptors allocated by the driver. Increasing this
 * value allows the driver to queue more transmits. Each descriptor is 16
 * bytes. Performance tests have show the 2K value to be optimal for top
 * performance.
 */
#define DEFAULT_TXD	256
#define PERFORM_TXD	2048
#define MAX_TXD		4096
#define MIN_TXD		64

/*
 * RxDescriptors Valid Range: 64-4096 Default Value: 256 This value is the
 * number of receive descriptors allocated for each RX queue. Increasing this
 * value allows the driver to buffer more incoming packets. Each descriptor
 * is 16 bytes.  A receive buffer is also allocated for each descriptor. 
 * 
 * Note: with 8 rings and a dual port card, it is possible to bump up 
 *	against the system mbuf pool limit, you can tune nmbclusters
 *	to adjust for this.
 */
#define DEFAULT_RXD	256
#define PERFORM_RXD	2048
#define MAX_RXD		4096
#define MIN_RXD		64

/* Alignment for rings */
#define DBA_ALIGN	128

/*
 * This parameter controls the maximum no of times the driver will loop in
 * the isr. Minimum Value = 1
 */
#define MAX_LOOP	10

/*
 * This parameter controls the duration of transmit watchdog timer.
 */
#define IXGBE_TX_TIMEOUT                   5	/* set to 5 seconds */

/*
 * This parameters control when the driver calls the routine to reclaim
 * transmit descriptors.
 */
#define IXGBE_TX_CLEANUP_THRESHOLD	(adapter->num_tx_desc / 8)
#define IXGBE_TX_OP_THRESHOLD		(adapter->num_tx_desc / 32)

#define IXGBE_MAX_FRAME_SIZE	0x3F00

/* Flow control constants */
#define IXGBE_FC_PAUSE		0x680
#define IXGBE_FC_HI		0x20000
#define IXGBE_FC_LO		0x10000

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

#define MAX_NUM_MULTICAST_ADDRESSES     128
#define IXGBE_MAX_SCATTER		100
#define MSIX_82598_BAR			3
#define MSIX_82599_BAR			4
#define IXGBE_TSO_SIZE			65535
#define IXGBE_TX_BUFFER_SIZE		((u32) 1514)
#define IXGBE_RX_HDR			128
#define CSUM_OFFLOAD			7	/* Bits in csum flags */

/* The number of MSIX messages the 82598 supports */
#define IXGBE_MSGS			18

/* For 6.X code compatibility */
#if __FreeBSD_version < 700000
#define ETHER_BPF_MTAP		BPF_MTAP
#define CSUM_TSO		0
#define IFCAP_TSO4		0
#define FILTER_STRAY
#define FILTER_HANDLED
#endif

/*
 * Interrupt Moderation parameters 
 */
#define IXGBE_LOW_LATENCY	128
#define IXGBE_AVE_LATENCY	400
#define IXGBE_BULK_LATENCY	1200
#define IXGBE_LINK_ITR		2000

/* Header split args for get_bug */
#define IXGBE_CLEAN_HDR		1
#define IXGBE_CLEAN_PKT		2
#define IXGBE_CLEAN_ALL		3

/* Used for auto RX queue configuration */
extern int mp_ncpus;

/*
 *****************************************************************************
 * vendor_info_array
 * 
 * This array contains the list of Subvendor/Subdevice IDs on which the driver
 * should load.
 * 
 *****************************************************************************
 */
typedef struct _ixgbe_vendor_info_t {
	unsigned int    vendor_id;
	unsigned int    device_id;
	unsigned int    subvendor_id;
	unsigned int    subdevice_id;
	unsigned int    index;
} ixgbe_vendor_info_t;


struct ixgbe_tx_buf {
	struct mbuf	*m_head;
	bus_dmamap_t	map;
};

struct ixgbe_rx_buf {
	struct mbuf	*m_head;
	struct mbuf	*m_pack;
	bus_dmamap_t	map;
};

/*
 * Bus dma allocation structure used by ixgbe_dma_malloc and ixgbe_dma_free.
 */
struct ixgbe_dma_alloc {
	bus_addr_t		dma_paddr;
	caddr_t			dma_vaddr;
	bus_dma_tag_t		dma_tag;
	bus_dmamap_t		dma_map;
	bus_dma_segment_t	dma_seg;
	bus_size_t		dma_size;
	int			dma_nseg;
};

/*
 * The transmit ring, one per tx queue
 */
struct tx_ring {
        struct adapter		*adapter;
	struct mtx		tx_mtx;
	u32			me;
	u32			msix;
	u32			eims;
	u32			watchdog_timer;
	union ixgbe_adv_tx_desc	*tx_base;
	volatile u32		tx_hwb;
	struct ixgbe_dma_alloc	txdma;
	struct task     	tx_task;
	struct taskqueue	*tq;
	u32			next_avail_tx_desc;
	u32			next_tx_to_clean;
	struct ixgbe_tx_buf	*tx_buffers;
	volatile u16		tx_avail;
	u32			txd_cmd;
	bus_dma_tag_t		txtag;
	char			mtx_name[16];
	/* Soft Stats */
	u32			no_tx_desc_avail;
	u32			no_tx_desc_late;
	u64			tx_irq;
	u64			total_packets;
};


/*
 * The Receive ring, one per rx queue
 */
struct rx_ring {
        struct adapter		*adapter;
	struct mtx		rx_mtx;
	u32			me;
	u32			msix;
	u32			eims;
	u32			payload;
	struct task     	rx_task;
	struct taskqueue	*tq;
	union ixgbe_adv_rx_desc	*rx_base;
	struct ixgbe_dma_alloc	rxdma;
	struct lro_ctrl		lro;
        unsigned int		last_cleaned;
        unsigned int		next_to_check;
	struct ixgbe_rx_buf	*rx_buffers;
	bus_dma_tag_t		rxtag;
	bus_dmamap_t		spare_map;
	struct mbuf		*fmp;
	struct mbuf		*lmp;
	char			mtx_name[16];

	u32			bytes; /* Used for AIM calc */
	u32			eitr_setting;
	bool			enable_lro;

	/* Soft stats */
	u64			rx_irq;
	u64			rx_split_packets;
	u64			rx_packets;
	u64 			rx_bytes;
};

/* Our adapter structure */
struct adapter {
	struct ifnet	*ifp;
	struct ixgbe_hw	hw;

	struct ixgbe_osdep	osdep;
	struct device	*dev;

	struct resource	*pci_mem;
	struct resource	*msix_mem;

	/*
	 * Interrupt resources:
	 *  Oplin has 20 MSIX messages
	 *  so allocate that for now.
	 */
	void		*tag[IXGBE_MSGS];
	struct resource *res[IXGBE_MSGS];
	int		rid[IXGBE_MSGS];

	struct ifmedia	media;
	struct callout	timer;
	int		msix;
	int		if_flags;

	struct mtx	core_mtx;

	/* Info about the board itself */
	u32		part_num;
	u32		optics;
	bool		link_active;
	u16		max_frame_size;
	u32		link_speed;
	u32 		linkvec;
	u32		tx_int_delay;
	u32		tx_abs_int_delay;
	u32		rx_int_delay;
	u32		rx_abs_int_delay;

#ifdef IXGBE_HW_VLAN_SUPPORT
	eventhandler_tag vlan_attach;
	eventhandler_tag vlan_detach;
#endif

	/* Mbuf cluster size */
	u32		rx_mbuf_sz;

	/* Check for missing optics */
	bool		sfp_probe;

	/*
	 * Transmit rings:
	 *	Allocated at run time, an array of rings.
	 */
	struct tx_ring	*tx_rings;
	int		num_tx_desc;
	int		num_tx_queues;

	/*
	 * Receive rings:
	 *	Allocated at run time, an array of rings.
	 */
	struct rx_ring	*rx_rings;
	int		num_rx_desc;
	int		num_rx_queues;
	u32		rx_mask;
	u32		rx_process_limit;

	/* Misc stats maintained by the driver */
	unsigned long   dropped_pkts;
	unsigned long   mbuf_defrag_failed;
	unsigned long   mbuf_header_failed;
	unsigned long   mbuf_packet_failed;
	unsigned long   no_tx_map_avail;
	unsigned long   no_tx_dma_setup;
	unsigned long   watchdog_events;
	unsigned long   tso_tx;
	unsigned long	link_irq;

	struct ixgbe_hw_stats stats;
};

#define IXGBE_CORE_LOCK_INIT(_sc, _name) \
        mtx_init(&(_sc)->core_mtx, _name, "IXGBE Core Lock", MTX_DEF)
#define IXGBE_CORE_LOCK_DESTROY(_sc)      mtx_destroy(&(_sc)->core_mtx)
#define IXGBE_TX_LOCK_DESTROY(_sc)                mtx_destroy(&(_sc)->tx_mtx)
#define IXGBE_RX_LOCK_DESTROY(_sc)                mtx_destroy(&(_sc)->rx_mtx)
#define IXGBE_CORE_LOCK(_sc)              mtx_lock(&(_sc)->core_mtx)
#define IXGBE_TX_LOCK(_sc)                        mtx_lock(&(_sc)->tx_mtx)
#define IXGBE_RX_LOCK(_sc)                        mtx_lock(&(_sc)->rx_mtx)
#define IXGBE_CORE_UNLOCK(_sc)            mtx_unlock(&(_sc)->core_mtx)
#define IXGBE_TX_UNLOCK(_sc)              mtx_unlock(&(_sc)->tx_mtx)
#define IXGBE_RX_UNLOCK(_sc)              mtx_unlock(&(_sc)->rx_mtx)
#define IXGBE_CORE_LOCK_ASSERT(_sc)       mtx_assert(&(_sc)->core_mtx, MA_OWNED)
#define IXGBE_TX_LOCK_ASSERT(_sc)         mtx_assert(&(_sc)->tx_mtx, MA_OWNED)


#endif /* _IXGBE_H_ */
