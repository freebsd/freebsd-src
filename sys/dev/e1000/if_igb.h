/******************************************************************************

  Copyright (c) 2001-2010, Intel Corporation 
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

#ifndef _IGB_H_DEFINED_
#define _IGB_H_DEFINED_

/* Tunables */

/*
 * IGB_TXD: Maximum number of Transmit Descriptors
 *
 *   This value is the number of transmit descriptors allocated by the driver.
 *   Increasing this value allows the driver to queue more transmits. Each
 *   descriptor is 16 bytes.
 *   Since TDLEN should be multiple of 128bytes, the number of transmit
 *   desscriptors should meet the following condition.
 *      (num_tx_desc * sizeof(struct e1000_tx_desc)) % 128 == 0
 */
#define IGB_MIN_TXD		256
#define IGB_DEFAULT_TXD		1024
#define IGB_MAX_TXD		4096

/*
 * IGB_RXD: Maximum number of Transmit Descriptors
 *
 *   This value is the number of receive descriptors allocated by the driver.
 *   Increasing this value allows the driver to buffer more incoming packets.
 *   Each descriptor is 16 bytes.  A receive buffer is also allocated for each
 *   descriptor. The maximum MTU size is 16110.
 *   Since TDLEN should be multiple of 128bytes, the number of transmit
 *   desscriptors should meet the following condition.
 *      (num_tx_desc * sizeof(struct e1000_tx_desc)) % 128 == 0
 */
#define IGB_MIN_RXD		256
#define IGB_DEFAULT_RXD		1024
#define IGB_MAX_RXD		4096

/*
 * IGB_TIDV - Transmit Interrupt Delay Value
 * Valid Range: 0-65535 (0=off)
 * Default Value: 64
 *   This value delays the generation of transmit interrupts in units of
 *   1.024 microseconds. Transmit interrupt reduction can improve CPU
 *   efficiency if properly tuned for specific network traffic. If the
 *   system is reporting dropped transmits, this value may be set too high
 *   causing the driver to run out of available transmit descriptors.
 */
#define IGB_TIDV                         64

/*
 * IGB_TADV - Transmit Absolute Interrupt Delay Value
 * Valid Range: 0-65535 (0=off)
 * Default Value: 64
 *   This value, in units of 1.024 microseconds, limits the delay in which a
 *   transmit interrupt is generated. Useful only if IGB_TIDV is non-zero,
 *   this value ensures that an interrupt is generated after the initial
 *   packet is sent on the wire within the set amount of time.  Proper tuning,
 *   along with IGB_TIDV, may improve traffic throughput in specific
 *   network conditions.
 */
#define IGB_TADV                         64

/*
 * IGB_RDTR - Receive Interrupt Delay Timer (Packet Timer)
 * Valid Range: 0-65535 (0=off)
 * Default Value: 0
 *   This value delays the generation of receive interrupts in units of 1.024
 *   microseconds.  Receive interrupt reduction can improve CPU efficiency if
 *   properly tuned for specific network traffic. Increasing this value adds
 *   extra latency to frame reception and can end up decreasing the throughput
 *   of TCP traffic. If the system is reporting dropped receives, this value
 *   may be set too high, causing the driver to run out of available receive
 *   descriptors.
 *
 *   CAUTION: When setting IGB_RDTR to a value other than 0, adapters
 *            may hang (stop transmitting) under certain network conditions.
 *            If this occurs a WATCHDOG message is logged in the system
 *            event log. In addition, the controller is automatically reset,
 *            restoring the network connection. To eliminate the potential
 *            for the hang ensure that IGB_RDTR is set to 0.
 */
#define IGB_RDTR                         0

/*
 * Receive Interrupt Absolute Delay Timer (Not valid for 82542/82543/82544)
 * Valid Range: 0-65535 (0=off)
 * Default Value: 64
 *   This value, in units of 1.024 microseconds, limits the delay in which a
 *   receive interrupt is generated. Useful only if IGB_RDTR is non-zero,
 *   this value ensures that an interrupt is generated after the initial
 *   packet is received within the set amount of time.  Proper tuning,
 *   along with IGB_RDTR, may improve traffic throughput in specific network
 *   conditions.
 */
#define IGB_RADV                         64

/*
 * This parameter controls the duration of transmit watchdog timer.
 */
#define IGB_WATCHDOG                   (10 * hz)

/*
 * This parameter controls when the driver calls the routine to reclaim
 * transmit descriptors.
 */
#define IGB_TX_CLEANUP_THRESHOLD	(adapter->num_tx_desc / 8)
#define IGB_TX_OP_THRESHOLD	(adapter->num_tx_desc / 32)

/*
 * This parameter controls whether or not autonegotation is enabled.
 *              0 - Disable autonegotiation
 *              1 - Enable  autonegotiation
 */
#define DO_AUTO_NEG                     1

/*
 * This parameter control whether or not the driver will wait for
 * autonegotiation to complete.
 *              1 - Wait for autonegotiation to complete
 *              0 - Don't wait for autonegotiation to complete
 */
#define WAIT_FOR_AUTO_NEG_DEFAULT       0

/* Tunables -- End */

#define AUTONEG_ADV_DEFAULT	(ADVERTISE_10_HALF | ADVERTISE_10_FULL | \
				ADVERTISE_100_HALF | ADVERTISE_100_FULL | \
				ADVERTISE_1000_FULL)

#define AUTO_ALL_MODES		0

/* PHY master/slave setting */
#define IGB_MASTER_SLAVE		e1000_ms_hw_default

/*
 * Micellaneous constants
 */
#define IGB_VENDOR_ID			0x8086

#define IGB_JUMBO_PBA			0x00000028
#define IGB_DEFAULT_PBA			0x00000030
#define IGB_SMARTSPEED_DOWNSHIFT	3
#define IGB_SMARTSPEED_MAX		15
#define IGB_MAX_LOOP			10

#define IGB_RX_PTHRESH			(hw->mac.type <= e1000_82576 ? 16 : 8)
#define IGB_RX_HTHRESH			8
#define IGB_RX_WTHRESH			1

#define IGB_TX_PTHRESH			8
#define IGB_TX_HTHRESH			1
#define IGB_TX_WTHRESH			((hw->mac.type == e1000_82576 && \
                                          adapter->msix_mem) ? 1 : 16)

#define MAX_NUM_MULTICAST_ADDRESSES     128
#define PCI_ANY_ID                      (~0U)
#define ETHER_ALIGN                     2
#define IGB_TX_BUFFER_SIZE		((uint32_t) 1514)
#define IGB_FC_PAUSE_TIME		0x0680
#define IGB_EEPROM_APME			0x400;

/*
 * TDBA/RDBA should be aligned on 16 byte boundary. But TDLEN/RDLEN should be
 * multiple of 128 bytes. So we align TDBA/RDBA on 128 byte boundary. This will
 * also optimize cache line size effect. H/W supports up to cache line size 128.
 */
#define IGB_DBA_ALIGN			128

#define SPEED_MODE_BIT (1<<21)		/* On PCI-E MACs only */

/* PCI Config defines */
#define IGB_MSIX_BAR		3

/*
** This is the total number of MSIX vectors you wish
** to use, it also controls the size of resources.
** The 82575 has a total of 10, 82576 has 25. Set this
** to the real amount you need to streamline data storage.
*/
#define IGB_MSIX_VEC		6	/* MSIX vectors configured */

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

#define IGB_MAX_SCATTER		64
#define IGB_VFTA_SIZE		128
#define IGB_BR_SIZE		4096	/* ring buf size */
#define IGB_TSO_SIZE		(65535 + sizeof(struct ether_vlan_header))
#define IGB_TSO_SEG_SIZE	4096	/* Max dma segment size */
#define IGB_HDR_BUF		128
#define IGB_PKTTYPE_MASK	0x0000FFF0
#define ETH_ZLEN		60
#define ETH_ADDR_LEN		6

/* Offload bits in mbuf flag */
#if __FreeBSD_version >= 800000
#define CSUM_OFFLOAD		(CSUM_IP|CSUM_TCP|CSUM_UDP|CSUM_SCTP)
#else
#define CSUM_OFFLOAD		(CSUM_IP|CSUM_TCP|CSUM_UDP)
#endif

/* Define the starting Interrupt rate per Queue */
#define IGB_INTS_PER_SEC        8000
#define IGB_DEFAULT_ITR          1000000000/(IGB_INTS_PER_SEC * 256)


/* Header split codes for get_buf */
#define IGB_CLEAN_HEADER		0x01
#define IGB_CLEAN_PAYLOAD		0x02
#define IGB_CLEAN_BOTH			(IGB_CLEAN_HEADER | IGB_CLEAN_PAYLOAD)

#define IGB_LINK_ITR            2000

/* Precision Time Sync (IEEE 1588) defines */
#define ETHERTYPE_IEEE1588	0x88F7
#define PICOSECS_PER_TICK	20833
#define TSYNC_PORT		319 /* UDP port for the protocol */

/*
 * Bus dma allocation structure used by
 * e1000_dma_malloc and e1000_dma_free.
 */
struct igb_dma_alloc {
        bus_addr_t              dma_paddr;
        caddr_t                 dma_vaddr;
        bus_dma_tag_t           dma_tag;
        bus_dmamap_t            dma_map;
        bus_dma_segment_t       dma_seg;
        int                     dma_nseg;
};


/*
** Driver queue struct: this is the interrupt container
**  for the associated tx and rx ring.
*/
struct igb_queue {
	struct adapter		*adapter;
	u32			msix;		/* This queue's MSIX vector */
	u32			eims;		/* This queue's EIMS bit */
	u32			eitr_setting;
	struct resource		*res;
	void			*tag;
	struct tx_ring		*txr;
	struct rx_ring		*rxr;
	struct task		que_task;
	struct taskqueue	*tq;
	u64			irqs;
};

/*
 * Transmit ring: one per queue
 */
struct tx_ring {
	struct adapter		*adapter;
	u32			me;
	struct mtx		tx_mtx;
	char			mtx_name[16];
	struct igb_dma_alloc	txdma;
	struct e1000_tx_desc	*tx_base;
	u32			next_avail_desc;
	u32			next_to_clean;
	volatile u16		tx_avail;
	struct igb_tx_buffer	*tx_buffers;
#if __FreeBSD_version >= 800000
	struct buf_ring		*br;
#endif
	bus_dma_tag_t		txtag;

	u32			bytes;
	u32			packets;

	bool			watchdog_check;
	int			watchdog_time;
	u64			no_desc_avail;
	u64			tx_packets;
};

/*
 * Receive ring: one per queue
 */
struct rx_ring {
	struct adapter		*adapter;
	u32			me;
	struct igb_dma_alloc	rxdma;
	union e1000_adv_rx_desc	*rx_base;
	struct lro_ctrl		lro;
	bool			lro_enabled;
	bool			hdr_split;
	bool			discard;
	struct mtx		rx_mtx;
	char			mtx_name[16];
	u32			next_to_refresh;
	u32			next_to_check;
	struct igb_rx_buf	*rx_buffers;
	bus_dma_tag_t		htag;		/* dma tag for rx head */
	bus_dma_tag_t		ptag;		/* dma tag for rx packet */
	/*
	 * First/last mbuf pointers, for
	 * collecting multisegment RX packets.
	 */
	struct mbuf	       *fmp;
	struct mbuf	       *lmp;

	u32			bytes;
	u32			packets;

	/* Soft stats */
	u64			rx_split_packets;
	u64			rx_discarded;
	u64			rx_packets;
	u64			rx_bytes;
};

struct adapter {
	struct ifnet	*ifp;
	struct e1000_hw	hw;

	struct e1000_osdep osdep;
	struct device	*dev;
	struct cdev	*led_dev;

	struct resource *pci_mem;
	struct resource *msix_mem;
	struct resource	*res;
	void		*tag;
	u32		eims_mask;

	int		linkvec;
	int		link_mask;
	struct task	link_task;
	int		link_irq;

	struct ifmedia	media;
	struct callout	timer;
	int		msix;	/* total vectors allocated */
	int		if_flags;
	int		max_frame_size;
	int		min_frame_size;
	struct mtx	core_mtx;
	int		igb_insert_vlan_header;
	struct taskqueue *tq;	/* adapter task queue */
        u16		num_queues;

	eventhandler_tag vlan_attach;
	eventhandler_tag vlan_detach;
	u32		num_vlans;

	/* Management and WOL features */
	int		wol;
	int		has_manage;

	/* Info about the board itself */
	u8		link_active;
	u16		link_speed;
	u16		link_duplex;
	u32		smartspeed;

	/* Interface queues */
	struct igb_queue	*queues;

	/*
	 * Transmit rings
	 */
	struct tx_ring		*tx_rings;
        u16			num_tx_desc;

	/* 
	 * Receive rings
	 */
	struct rx_ring		*rx_rings;
	bool			rx_hdr_split;
        u16			num_rx_desc;
	int			rx_process_limit;
	u32			rx_mbuf_sz;
	u32			rx_mask;

	/* Misc stats maintained by the driver */
	unsigned long	dropped_pkts;
	unsigned long	mbuf_defrag_failed;
	unsigned long	mbuf_header_failed;
	unsigned long	mbuf_packet_failed;
	unsigned long	no_tx_map_avail;
        unsigned long	no_tx_dma_setup;
	unsigned long	watchdog_events;
	unsigned long	rx_overruns;

	boolean_t       in_detach;

#ifdef IGB_IEEE1588
	/* IEEE 1588 precision time support */
	struct cyclecounter     cycles;
	struct nettimer         clock;
	struct nettime_compare  compare;
	struct hwtstamp_ctrl    hwtstamp;
#endif

	struct e1000_hw_stats stats;
};

/* ******************************************************************************
 * vendor_info_array
 *
 * This array contains the list of Subvendor/Subdevice IDs on which the driver
 * should load.
 *
 * ******************************************************************************/
typedef struct _igb_vendor_info_t {
	unsigned int vendor_id;
	unsigned int device_id;
	unsigned int subvendor_id;
	unsigned int subdevice_id;
	unsigned int index;
} igb_vendor_info_t;


struct igb_tx_buffer {
	int		next_eop;  /* Index of the desc to watch */
        struct mbuf    *m_head;
        bus_dmamap_t    map;         /* bus_dma map for packet */
};

struct igb_rx_buf {
        struct mbuf    *m_head;
        struct mbuf    *m_pack;
	bus_dmamap_t	hmap;	/* bus_dma map for header */
	bus_dmamap_t	pmap;	/* bus_dma map for packet */
};

#define	IGB_CORE_LOCK_INIT(_sc, _name) \
	mtx_init(&(_sc)->core_mtx, _name, "IGB Core Lock", MTX_DEF)
#define	IGB_CORE_LOCK_DESTROY(_sc)	mtx_destroy(&(_sc)->core_mtx)
#define	IGB_CORE_LOCK(_sc)		mtx_lock(&(_sc)->core_mtx)
#define	IGB_CORE_UNLOCK(_sc)		mtx_unlock(&(_sc)->core_mtx)
#define	IGB_CORE_LOCK_ASSERT(_sc)	mtx_assert(&(_sc)->core_mtx, MA_OWNED)

#define	IGB_TX_LOCK_DESTROY(_sc)	mtx_destroy(&(_sc)->tx_mtx)
#define	IGB_TX_LOCK(_sc)		mtx_lock(&(_sc)->tx_mtx)
#define	IGB_TX_UNLOCK(_sc)		mtx_unlock(&(_sc)->tx_mtx)
#define	IGB_TX_TRYLOCK(_sc)		mtx_trylock(&(_sc)->tx_mtx)
#define	IGB_TX_LOCK_ASSERT(_sc)		mtx_assert(&(_sc)->tx_mtx, MA_OWNED)

#define	IGB_RX_LOCK_DESTROY(_sc)	mtx_destroy(&(_sc)->rx_mtx)
#define	IGB_RX_LOCK(_sc)		mtx_lock(&(_sc)->rx_mtx)
#define	IGB_RX_UNLOCK(_sc)		mtx_unlock(&(_sc)->rx_mtx)
#define	IGB_TX_LOCK_ASSERT(_sc)		mtx_assert(&(_sc)->tx_mtx, MA_OWNED)

#endif /* _IGB_H_DEFINED_ */


