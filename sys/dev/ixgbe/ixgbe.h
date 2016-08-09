/******************************************************************************

  Copyright (c) 2001-2015, Intel Corporation 
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


#ifndef _IXGBE_H_
#define _IXGBE_H_


#include <sys/param.h>
#include <sys/systm.h>
#ifndef IXGBE_LEGACY_TX
#include <sys/buf_ring.h>
#endif
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/sockio.h>
#include <sys/eventhandler.h>

#include <net/if.h>
#include <net/if_var.h>
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
#include <sys/sbuf.h>

#ifdef PCI_IOV
#include <sys/nv.h>
#include <sys/iov_schema.h>
#include <dev/pci/pci_iov.h>
#endif

#include "ixgbe_api.h"
#include "ixgbe_common.h"
#include "ixgbe_phy.h"
#include "ixgbe_vf.h"

#ifdef PCI_IOV
#include "ixgbe_common.h"
#include "ixgbe_mbx.h"
#endif

/* Tunables */

/*
 * TxDescriptors Valid Range: 64-4096 Default Value: 256 This value is the
 * number of transmit descriptors allocated by the driver. Increasing this
 * value allows the driver to queue more transmits. Each descriptor is 16
 * bytes. Performance tests have show the 2K value to be optimal for top
 * performance.
 */
#define DEFAULT_TXD	1024
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
#define DEFAULT_RXD	1024
#define PERFORM_RXD	2048
#define MAX_RXD		4096
#define MIN_RXD		64

/* Alignment for rings */
#define DBA_ALIGN	128

/*
 * This is the max watchdog interval, ie. the time that can
 * pass between any two TX clean operations, such only happening
 * when the TX hardware is functioning.
 */
#define IXGBE_WATCHDOG                   (10 * hz)

/*
 * This parameters control when the driver calls the routine to reclaim
 * transmit descriptors.
 */
#define IXGBE_TX_CLEANUP_THRESHOLD	(adapter->num_tx_desc / 8)
#define IXGBE_TX_OP_THRESHOLD		(adapter->num_tx_desc / 32)

/* These defines are used in MTU calculations */
#define IXGBE_MAX_FRAME_SIZE	9728
#define IXGBE_MTU_HDR		(ETHER_HDR_LEN + ETHER_CRC_LEN)
#define IXGBE_MTU_HDR_VLAN	(ETHER_HDR_LEN + ETHER_CRC_LEN + \
				 ETHER_VLAN_ENCAP_LEN)
#define IXGBE_MAX_MTU		(IXGBE_MAX_FRAME_SIZE - IXGBE_MTU_HDR)
#define IXGBE_MAX_MTU_VLAN	(IXGBE_MAX_FRAME_SIZE - IXGBE_MTU_HDR_VLAN)

/* Flow control constants */
#define IXGBE_FC_PAUSE		0xFFFF
#define IXGBE_FC_HI		0x20000
#define IXGBE_FC_LO		0x10000

/*
 * Used for optimizing small rx mbufs.  Effort is made to keep the copy
 * small and aligned for the CPU L1 cache.
 * 
 * MHLEN is typically 168 bytes, giving us 8-byte alignment.  Getting
 * 32 byte alignment needed for the fast bcopy results in 8 bytes being
 * wasted.  Getting 64 byte alignment, which _should_ be ideal for
 * modern Intel CPUs, results in 40 bytes wasted and a significant drop
 * in observed efficiency of the optimization, 97.9% -> 81.8%.
 */
#if __FreeBSD_version < 1002000
#define MPKTHSIZE			(sizeof(struct m_hdr) + sizeof(struct pkthdr))
#endif
#define IXGBE_RX_COPY_HDR_PADDED	((((MPKTHSIZE - 1) / 32) + 1) * 32)
#define IXGBE_RX_COPY_LEN		(MSIZE - IXGBE_RX_COPY_HDR_PADDED)
#define IXGBE_RX_COPY_ALIGN		(IXGBE_RX_COPY_HDR_PADDED - MPKTHSIZE)

/* Keep older OS drivers building... */
#if !defined(SYSCTL_ADD_UQUAD)
#define SYSCTL_ADD_UQUAD SYSCTL_ADD_QUAD
#endif

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
#define IXGBE_82598_SCATTER		100
#define IXGBE_82599_SCATTER		32
#define MSIX_82598_BAR			3
#define MSIX_82599_BAR			4
#define IXGBE_TSO_SIZE			262140
#define IXGBE_RX_HDR			128
#define IXGBE_VFTA_SIZE			128
#define IXGBE_BR_SIZE			4096
#define IXGBE_QUEUE_MIN_FREE		32
#define IXGBE_MAX_TX_BUSY		10
#define IXGBE_QUEUE_HUNG		0x80000000

#define IXV_EITR_DEFAULT		128

/* Supported offload bits in mbuf flag */
#if __FreeBSD_version >= 1000000
#define CSUM_OFFLOAD		(CSUM_IP_TSO|CSUM_IP6_TSO|CSUM_IP| \
				 CSUM_IP_UDP|CSUM_IP_TCP|CSUM_IP_SCTP| \
				 CSUM_IP6_UDP|CSUM_IP6_TCP|CSUM_IP6_SCTP)
#elif __FreeBSD_version >= 800000
#define CSUM_OFFLOAD		(CSUM_IP|CSUM_TCP|CSUM_UDP|CSUM_SCTP)
#else
#define CSUM_OFFLOAD		(CSUM_IP|CSUM_TCP|CSUM_UDP)
#endif

/* Backward compatibility items for very old versions */
#ifndef pci_find_cap
#define pci_find_cap pci_find_extcap
#endif

#ifndef DEVMETHOD_END
#define DEVMETHOD_END { NULL, NULL }
#endif

/*
 * Interrupt Moderation parameters 
 */
#define IXGBE_LOW_LATENCY	128
#define IXGBE_AVE_LATENCY	400
#define IXGBE_BULK_LATENCY	1200

/* Using 1FF (the max value), the interval is ~1.05ms */
#define IXGBE_LINK_ITR_QUANTA	0x1FF
#define IXGBE_LINK_ITR		((IXGBE_LINK_ITR_QUANTA << 3) & \
				    IXGBE_EITR_ITR_INT_MASK)

/* MAC type macros */
#define IXGBE_IS_X550VF(_adapter) \
	((_adapter->hw.mac.type == ixgbe_mac_X550_vf) || \
	 (_adapter->hw.mac.type == ixgbe_mac_X550EM_x_vf))

#define IXGBE_IS_VF(_adapter) \
	(IXGBE_IS_X550VF(_adapter) || \
	 (_adapter->hw.mac.type == ixgbe_mac_X540_vf) || \
	 (_adapter->hw.mac.type == ixgbe_mac_82599_vf))

#ifdef PCI_IOV
#define IXGBE_VF_INDEX(vmdq)  ((vmdq) / 32)
#define IXGBE_VF_BIT(vmdq)    (1 << ((vmdq) % 32))

#define IXGBE_VT_MSG_MASK	0xFFFF

#define IXGBE_VT_MSGINFO(msg)	\
	(((msg) & IXGBE_VT_MSGINFO_MASK) >> IXGBE_VT_MSGINFO_SHIFT)

#define IXGBE_VF_GET_QUEUES_RESP_LEN	5

#define IXGBE_API_VER_1_0	0		
#define IXGBE_API_VER_2_0	1	/* Solaris API.  Not supported. */
#define IXGBE_API_VER_1_1	2
#define IXGBE_API_VER_UNKNOWN	UINT16_MAX

enum ixgbe_iov_mode {
	IXGBE_64_VM,
	IXGBE_32_VM,
	IXGBE_NO_VM
};
#endif /* PCI_IOV */


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
	union ixgbe_adv_tx_desc	*eop;
	struct mbuf	*m_head;
	bus_dmamap_t	map;
};

struct ixgbe_rx_buf {
	struct mbuf	*buf;
	struct mbuf	*fmp;
	bus_dmamap_t	pmap;
	u_int		flags;
#define IXGBE_RX_COPY	0x01
	uint64_t	addr;
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

struct ixgbe_mc_addr {
	u8 addr[IXGBE_ETH_LENGTH_OF_ADDRESS];
	u32 vmdq;
};

/*
** Driver queue struct: this is the interrupt container
**  for the associated tx and rx ring.
*/
struct ix_queue {
	struct adapter		*adapter;
	u32			msix;           /* This queue's MSIX vector */
	u32			eims;           /* This queue's EIMS bit */
	u32			eitr_setting;
	u32			me;
	struct resource		*res;
	void			*tag;
	int			busy;
	struct tx_ring		*txr;
	struct rx_ring		*rxr;
	struct task		que_task;
	struct taskqueue	*tq;
	u64			irqs;
};

/*
 * The transmit ring, one per queue
 */
struct tx_ring {
        struct adapter		*adapter;
	struct mtx		tx_mtx;
	u32			me;
	u32			tail;
	int			busy;
	union ixgbe_adv_tx_desc	*tx_base;
	struct ixgbe_tx_buf	*tx_buffers;
	struct ixgbe_dma_alloc	txdma;
	volatile u16		tx_avail;
	u16			next_avail_desc;
	u16			next_to_clean;
	u16			num_desc;
	u32			txd_cmd;
	bus_dma_tag_t		txtag;
	char			mtx_name[16];
#ifndef IXGBE_LEGACY_TX
	struct buf_ring		*br;
	struct task		txq_task;
#endif
#ifdef IXGBE_FDIR
	u16			atr_sample;
	u16			atr_count;
#endif
	u32			bytes;  /* used for AIM */
	u32			packets;
	/* Soft Stats */
	unsigned long   	tso_tx;
	unsigned long   	no_tx_map_avail;
	unsigned long   	no_tx_dma_setup;
	u64			no_desc_avail;
	u64			total_packets;
};


/*
 * The Receive ring, one per rx queue
 */
struct rx_ring {
        struct adapter		*adapter;
	struct mtx		rx_mtx;
	u32			me;
	u32			tail;
	union ixgbe_adv_rx_desc	*rx_base;
	struct ixgbe_dma_alloc	rxdma;
	struct lro_ctrl		lro;
	bool			lro_enabled;
	bool			hw_rsc;
	bool			vtag_strip;
        u16			next_to_refresh;
        u16 			next_to_check;
	u16			num_desc;
	u16			mbuf_sz;
	char			mtx_name[16];
	struct ixgbe_rx_buf	*rx_buffers;
	bus_dma_tag_t		ptag;

	u32			bytes; /* Used for AIM calc */
	u32			packets;

	/* Soft stats */
	u64			rx_irq;
	u64			rx_copies;
	u64			rx_packets;
	u64 			rx_bytes;
	u64 			rx_discarded;
	u64 			rsc_num;
#ifdef IXGBE_FDIR
	u64			flm;
#endif
};

#ifdef PCI_IOV
#define IXGBE_VF_CTS		(1 << 0) /* VF is clear to send. */
#define IXGBE_VF_CAP_MAC	(1 << 1) /* VF is permitted to change MAC. */
#define IXGBE_VF_CAP_VLAN	(1 << 2) /* VF is permitted to join vlans. */
#define IXGBE_VF_ACTIVE		(1 << 3) /* VF is active. */

#define IXGBE_MAX_VF_MC 30  /* Max number of multicast entries */

struct ixgbe_vf {
	u_int		pool;
	u_int		rar_index;
	u_int		max_frame_size;
	uint32_t	flags;
	uint8_t		ether_addr[ETHER_ADDR_LEN];
	uint16_t	mc_hash[IXGBE_MAX_VF_MC];
	uint16_t	num_mc_hashes;
	uint16_t	default_vlan;
	uint16_t	vlan_tag;
	uint16_t	api_ver;
};
#endif /* PCI_IOV */

/* Our adapter structure */
struct adapter {
	struct ixgbe_hw		hw;
	struct ixgbe_osdep	osdep;

	device_t		dev;
	struct ifnet		*ifp;

	struct resource		*pci_mem;
	struct resource		*msix_mem;

	/*
	 * Interrupt resources: this set is
	 * either used for legacy, or for Link
	 * when doing MSIX
	 */
	void			*tag;
	struct resource 	*res;

	struct ifmedia		media;
	struct callout		timer;
	int			msix;
	int			if_flags;

	struct mtx		core_mtx;

	eventhandler_tag 	vlan_attach;
	eventhandler_tag 	vlan_detach;

	u16			num_vlans;
	u16			num_queues;

	/*
	** Shadow VFTA table, this is needed because
	** the real vlan filter table gets cleared during
	** a soft reset and the driver needs to be able
	** to repopulate it.
	*/
	u32			shadow_vfta[IXGBE_VFTA_SIZE];

	/* Info about the interface */
	u32			optics;
	u32			fc; /* local flow ctrl setting */
	int			advertise;  /* link speeds */
	bool			enable_aim; /* adaptive interrupt moderation */
	bool			link_active;
	u16			max_frame_size;
	u16			num_segs;
	u32			link_speed;
	bool			link_up;
	u32 			vector;
	u16			dmac;
	bool			eee_enabled;
	u32			phy_layer;

	/* Power management-related */
	bool			wol_support;
	u32			wufc;

	/* Mbuf cluster size */
	u32			rx_mbuf_sz;

	/* Support for pluggable optics */
	bool			sfp_probe;
	struct task     	link_task;  /* Link tasklet */
	struct task     	mod_task;   /* SFP tasklet */
	struct task     	msf_task;   /* Multispeed Fiber */
#ifdef PCI_IOV
	struct task		mbx_task;   /* VF -> PF mailbox interrupt */
#endif /* PCI_IOV */
#ifdef IXGBE_FDIR
	int			fdir_reinit;
	struct task     	fdir_task;
#endif
	struct task		phy_task;   /* PHY intr tasklet */
	struct taskqueue	*tq;

	/*
	** Queues: 
	**   This is the irq holder, it has
	**   and RX/TX pair or rings associated
	**   with it.
	*/
	struct ix_queue		*queues;

	/*
	 * Transmit rings:
	 *	Allocated at run time, an array of rings.
	 */
	struct tx_ring		*tx_rings;
	u32			num_tx_desc;
	u32			tx_process_limit;

	/*
	 * Receive rings:
	 *	Allocated at run time, an array of rings.
	 */
	struct rx_ring		*rx_rings;
	u64			active_queues;
	u32			num_rx_desc;
	u32			rx_process_limit;

	/* Multicast array memory */
	struct ixgbe_mc_addr	*mta;
	int			num_vfs;
	int			pool;
#ifdef PCI_IOV
	struct ixgbe_vf		*vfs;
#endif
#ifdef DEV_NETMAP
	void 			(*init_locked)(struct adapter *);
	void 			(*stop_locked)(void *);
#endif

	/* Misc stats maintained by the driver */
	unsigned long   	dropped_pkts;
	unsigned long   	mbuf_defrag_failed;
	unsigned long   	mbuf_header_failed;
	unsigned long   	mbuf_packet_failed;
	unsigned long   	watchdog_events;
	unsigned long		link_irq;
	union {
		struct ixgbe_hw_stats pf;
		struct ixgbevf_hw_stats vf;
	} stats;
#if __FreeBSD_version >= 1100036
	/* counter(9) stats */
	u64			ipackets;
	u64			ierrors;
	u64			opackets;
	u64			oerrors;
	u64			ibytes;
	u64			obytes;
	u64			imcasts;
	u64			omcasts;
	u64			iqdrops;
	u64			noproto;
#endif
};


/* Precision Time Sync (IEEE 1588) defines */
#define ETHERTYPE_IEEE1588      0x88F7
#define PICOSECS_PER_TICK       20833
#define TSYNC_UDP_PORT          319 /* UDP port for the protocol */
#define IXGBE_ADVTXD_TSTAMP	0x00080000


#define IXGBE_CORE_LOCK_INIT(_sc, _name) \
        mtx_init(&(_sc)->core_mtx, _name, "IXGBE Core Lock", MTX_DEF)
#define IXGBE_CORE_LOCK_DESTROY(_sc)      mtx_destroy(&(_sc)->core_mtx)
#define IXGBE_TX_LOCK_DESTROY(_sc)        mtx_destroy(&(_sc)->tx_mtx)
#define IXGBE_RX_LOCK_DESTROY(_sc)        mtx_destroy(&(_sc)->rx_mtx)
#define IXGBE_CORE_LOCK(_sc)              mtx_lock(&(_sc)->core_mtx)
#define IXGBE_TX_LOCK(_sc)                mtx_lock(&(_sc)->tx_mtx)
#define IXGBE_TX_TRYLOCK(_sc)             mtx_trylock(&(_sc)->tx_mtx)
#define IXGBE_RX_LOCK(_sc)                mtx_lock(&(_sc)->rx_mtx)
#define IXGBE_CORE_UNLOCK(_sc)            mtx_unlock(&(_sc)->core_mtx)
#define IXGBE_TX_UNLOCK(_sc)              mtx_unlock(&(_sc)->tx_mtx)
#define IXGBE_RX_UNLOCK(_sc)              mtx_unlock(&(_sc)->rx_mtx)
#define IXGBE_CORE_LOCK_ASSERT(_sc)       mtx_assert(&(_sc)->core_mtx, MA_OWNED)
#define IXGBE_TX_LOCK_ASSERT(_sc)         mtx_assert(&(_sc)->tx_mtx, MA_OWNED)

/* For backward compatibility */
#if !defined(PCIER_LINK_STA)
#define PCIER_LINK_STA PCIR_EXPRESS_LINK_STA
#endif

/* Stats macros */
#if __FreeBSD_version >= 1100036
#define IXGBE_SET_IPACKETS(sc, count)    (sc)->ipackets = (count)
#define IXGBE_SET_IERRORS(sc, count)     (sc)->ierrors = (count)
#define IXGBE_SET_OPACKETS(sc, count)    (sc)->opackets = (count)
#define IXGBE_SET_OERRORS(sc, count)     (sc)->oerrors = (count)
#define IXGBE_SET_COLLISIONS(sc, count)
#define IXGBE_SET_IBYTES(sc, count)      (sc)->ibytes = (count)
#define IXGBE_SET_OBYTES(sc, count)      (sc)->obytes = (count)
#define IXGBE_SET_IMCASTS(sc, count)     (sc)->imcasts = (count)
#define IXGBE_SET_OMCASTS(sc, count)     (sc)->omcasts = (count)
#define IXGBE_SET_IQDROPS(sc, count)     (sc)->iqdrops = (count)
#else
#define IXGBE_SET_IPACKETS(sc, count)    (sc)->ifp->if_ipackets = (count)
#define IXGBE_SET_IERRORS(sc, count)     (sc)->ifp->if_ierrors = (count)
#define IXGBE_SET_OPACKETS(sc, count)    (sc)->ifp->if_opackets = (count)
#define IXGBE_SET_OERRORS(sc, count)     (sc)->ifp->if_oerrors = (count)
#define IXGBE_SET_COLLISIONS(sc, count)  (sc)->ifp->if_collisions = (count)
#define IXGBE_SET_IBYTES(sc, count)      (sc)->ifp->if_ibytes = (count)
#define IXGBE_SET_OBYTES(sc, count)      (sc)->ifp->if_obytes = (count)
#define IXGBE_SET_IMCASTS(sc, count)     (sc)->ifp->if_imcasts = (count)
#define IXGBE_SET_OMCASTS(sc, count)     (sc)->ifp->if_omcasts = (count)
#define IXGBE_SET_IQDROPS(sc, count)     (sc)->ifp->if_iqdrops = (count)
#endif

/* External PHY register addresses */
#define IXGBE_PHY_CURRENT_TEMP		0xC820
#define IXGBE_PHY_OVERTEMP_STATUS	0xC830

/* Sysctl help messages; displayed with sysctl -d */
#define IXGBE_SYSCTL_DESC_ADV_SPEED \
	"\nControl advertised link speed using these flags:\n" \
	"\t0x1 - advertise 100M\n" \
	"\t0x2 - advertise 1G\n" \
	"\t0x4 - advertise 10G\n\n" \
	"\t100M is only supported on certain 10GBaseT adapters.\n"

#define IXGBE_SYSCTL_DESC_SET_FC \
	"\nSet flow control mode using these values:\n" \
	"\t0 - off\n" \
	"\t1 - rx pause\n" \
	"\t2 - tx pause\n" \
	"\t3 - tx and rx pause"

static inline bool
ixgbe_is_sfp(struct ixgbe_hw *hw)
{
	switch (hw->phy.type) {
	case ixgbe_phy_sfp_avago:
	case ixgbe_phy_sfp_ftl:
	case ixgbe_phy_sfp_intel:
	case ixgbe_phy_sfp_unknown:
	case ixgbe_phy_sfp_passive_tyco:
	case ixgbe_phy_sfp_passive_unknown:
	case ixgbe_phy_qsfp_passive_unknown:
	case ixgbe_phy_qsfp_active_unknown:
	case ixgbe_phy_qsfp_intel:
	case ixgbe_phy_qsfp_unknown:
		return TRUE;
	default:
		return FALSE;
	}
}

/* Workaround to make 8.0 buildable */
#if __FreeBSD_version >= 800000 && __FreeBSD_version < 800504
static __inline int
drbr_needs_enqueue(struct ifnet *ifp, struct buf_ring *br)
{
#ifdef ALTQ
        if (ALTQ_IS_ENABLED(&ifp->if_snd))
                return (1);
#endif
        return (!buf_ring_empty(br));
}
#endif

/*
** Find the number of unrefreshed RX descriptors
*/
static inline u16
ixgbe_rx_unrefreshed(struct rx_ring *rxr)
{       
	if (rxr->next_to_check > rxr->next_to_refresh)
		return (rxr->next_to_check - rxr->next_to_refresh - 1);
	else
		return ((rxr->num_desc + rxr->next_to_check) -
		    rxr->next_to_refresh - 1);
}       

/*
** This checks for a zero mac addr, something that will be likely
** unless the Admin on the Host has created one.
*/
static inline bool
ixv_check_ether_addr(u8 *addr)
{
	bool status = TRUE;

	if ((addr[0] == 0 && addr[1]== 0 && addr[2] == 0 &&
	    addr[3] == 0 && addr[4]== 0 && addr[5] == 0))
		status = FALSE;
	return (status);
}

/* Shared Prototypes */

#ifdef IXGBE_LEGACY_TX
void     ixgbe_start(struct ifnet *);
void     ixgbe_start_locked(struct tx_ring *, struct ifnet *);
#else /* ! IXGBE_LEGACY_TX */
int	ixgbe_mq_start(struct ifnet *, struct mbuf *);
int	ixgbe_mq_start_locked(struct ifnet *, struct tx_ring *);
void	ixgbe_qflush(struct ifnet *);
void	ixgbe_deferred_mq_start(void *, int);
#endif /* IXGBE_LEGACY_TX */

int	ixgbe_allocate_queues(struct adapter *);
int	ixgbe_allocate_transmit_buffers(struct tx_ring *);
int	ixgbe_setup_transmit_structures(struct adapter *);
void	ixgbe_free_transmit_structures(struct adapter *);
int	ixgbe_allocate_receive_buffers(struct rx_ring *);
int	ixgbe_setup_receive_structures(struct adapter *);
void	ixgbe_free_receive_structures(struct adapter *);
void	ixgbe_txeof(struct tx_ring *);
bool	ixgbe_rxeof(struct ix_queue *);

int	ixgbe_dma_malloc(struct adapter *,
	    bus_size_t, struct ixgbe_dma_alloc *, int);
void	ixgbe_dma_free(struct adapter *, struct ixgbe_dma_alloc *);

#ifdef PCI_IOV

static inline boolean_t
ixgbe_vf_mac_changed(struct ixgbe_vf *vf, const uint8_t *mac)
{
	return (bcmp(mac, vf->ether_addr, ETHER_ADDR_LEN) != 0);
}

static inline void
ixgbe_send_vf_msg(struct adapter *adapter, struct ixgbe_vf *vf, u32 msg)
{

	if (vf->flags & IXGBE_VF_CTS)
		msg |= IXGBE_VT_MSGTYPE_CTS;
	
	ixgbe_write_mbx(&adapter->hw, &msg, 1, vf->pool);
}

static inline void
ixgbe_send_vf_ack(struct adapter *adapter, struct ixgbe_vf *vf, u32 msg)
{
	msg &= IXGBE_VT_MSG_MASK;
	ixgbe_send_vf_msg(adapter, vf, msg | IXGBE_VT_MSGTYPE_ACK);
}

static inline void
ixgbe_send_vf_nack(struct adapter *adapter, struct ixgbe_vf *vf, u32 msg)
{
	msg &= IXGBE_VT_MSG_MASK;
	ixgbe_send_vf_msg(adapter, vf, msg | IXGBE_VT_MSGTYPE_NACK);
}

static inline void
ixgbe_process_vf_ack(struct adapter *adapter, struct ixgbe_vf *vf)
{
	if (!(vf->flags & IXGBE_VF_CTS))
		ixgbe_send_vf_nack(adapter, vf, 0);
}

static inline enum ixgbe_iov_mode
ixgbe_get_iov_mode(struct adapter *adapter)
{
	if (adapter->num_vfs == 0)
		return (IXGBE_NO_VM);
	if (adapter->num_queues <= 2)
		return (IXGBE_64_VM);
	else if (adapter->num_queues <= 4)
		return (IXGBE_32_VM);
	else
		return (IXGBE_NO_VM);
}

static inline u16
ixgbe_max_vfs(enum ixgbe_iov_mode mode)
{
	/*
	 * We return odd numbers below because we
	 * reserve 1 VM's worth of queues for the PF.
	 */
	switch (mode) {
	case IXGBE_64_VM:
		return (63);
	case IXGBE_32_VM:
		return (31);
	case IXGBE_NO_VM:
	default:
		return (0);
	}
}

static inline int
ixgbe_vf_queues(enum ixgbe_iov_mode mode)
{
	switch (mode) {
	case IXGBE_64_VM:
		return (2);
	case IXGBE_32_VM:
		return (4);
	case IXGBE_NO_VM:
	default:
		return (0);
	}
}

static inline int
ixgbe_vf_que_index(enum ixgbe_iov_mode mode, u32 vfnum, int num)
{
	return ((vfnum * ixgbe_vf_queues(mode)) + num);
}

static inline int
ixgbe_pf_que_index(enum ixgbe_iov_mode mode, int num)
{
	return (ixgbe_vf_que_index(mode, ixgbe_max_vfs(mode), num));
}

static inline void
ixgbe_update_max_frame(struct adapter * adapter, int max_frame)
{
	if (adapter->max_frame_size < max_frame)
		adapter->max_frame_size = max_frame;
}

static inline u32
ixgbe_get_mrqc(enum ixgbe_iov_mode mode)
{
       u32 mrqc = 0;
       switch (mode) {
       case IXGBE_64_VM:
               mrqc = IXGBE_MRQC_VMDQRSS64EN;
               break;
       case IXGBE_32_VM:
               mrqc = IXGBE_MRQC_VMDQRSS32EN;
               break;
        case IXGBE_NO_VM:
                mrqc = 0;
                break;
       default:
            panic("Unexpected SR-IOV mode %d", mode);
       }
        return(mrqc);
}


static inline u32
ixgbe_get_mtqc(enum ixgbe_iov_mode mode)
{
       uint32_t mtqc = 0;
        switch (mode) {
        case IXGBE_64_VM:
               mtqc |= IXGBE_MTQC_64VF | IXGBE_MTQC_VT_ENA;
                break;
        case IXGBE_32_VM:
               mtqc |= IXGBE_MTQC_32VF | IXGBE_MTQC_VT_ENA;
                break;
        case IXGBE_NO_VM:
                mtqc = IXGBE_MTQC_64Q_1PB;
                break;
        default:
                panic("Unexpected SR-IOV mode %d", mode);
        }
        return(mtqc);
}
#endif /* PCI_IOV */

#endif /* _IXGBE_H_ */
