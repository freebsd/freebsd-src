/******************************************************************************

  Copyright (c) 2001-2011, Intel Corporation 
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


#ifndef _EM_H_DEFINED_
#define _EM_H_DEFINED_


/* Tunables */

/*
 * EM_TXD: Maximum number of Transmit Descriptors
 * Valid Range: 80-256 for 82542 and 82543-based adapters
 *              80-4096 for others
 * Default Value: 256
 *   This value is the number of transmit descriptors allocated by the driver.
 *   Increasing this value allows the driver to queue more transmits. Each
 *   descriptor is 16 bytes.
 *   Since TDLEN should be multiple of 128bytes, the number of transmit
 *   desscriptors should meet the following condition.
 *      (num_tx_desc * sizeof(struct e1000_tx_desc)) % 128 == 0
 */
#define EM_MIN_TXD		80
#define EM_MAX_TXD		4096
#define EM_DEFAULT_TXD		1024

/*
 * EM_RXD - Maximum number of receive Descriptors
 * Valid Range: 80-256 for 82542 and 82543-based adapters
 *              80-4096 for others
 * Default Value: 256
 *   This value is the number of receive descriptors allocated by the driver.
 *   Increasing this value allows the driver to buffer more incoming packets.
 *   Each descriptor is 16 bytes.  A receive buffer is also allocated for each
 *   descriptor. The maximum MTU size is 16110.
 *   Since TDLEN should be multiple of 128bytes, the number of transmit
 *   desscriptors should meet the following condition.
 *      (num_tx_desc * sizeof(struct e1000_tx_desc)) % 128 == 0
 */
#define EM_MIN_RXD		80
#define EM_MAX_RXD		4096
#define EM_DEFAULT_RXD		1024

/*
 * EM_TIDV - Transmit Interrupt Delay Value
 * Valid Range: 0-65535 (0=off)
 * Default Value: 64
 *   This value delays the generation of transmit interrupts in units of
 *   1.024 microseconds. Transmit interrupt reduction can improve CPU
 *   efficiency if properly tuned for specific network traffic. If the
 *   system is reporting dropped transmits, this value may be set too high
 *   causing the driver to run out of available transmit descriptors.
 */
#define EM_TIDV                         64

/*
 * EM_TADV - Transmit Absolute Interrupt Delay Value
 * (Not valid for 82542/82543/82544)
 * Valid Range: 0-65535 (0=off)
 * Default Value: 64
 *   This value, in units of 1.024 microseconds, limits the delay in which a
 *   transmit interrupt is generated. Useful only if EM_TIDV is non-zero,
 *   this value ensures that an interrupt is generated after the initial
 *   packet is sent on the wire within the set amount of time.  Proper tuning,
 *   along with EM_TIDV, may improve traffic throughput in specific
 *   network conditions.
 */
#define EM_TADV                         64

/*
 * EM_RDTR - Receive Interrupt Delay Timer (Packet Timer)
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
 *   CAUTION: When setting EM_RDTR to a value other than 0, adapters
 *            may hang (stop transmitting) under certain network conditions.
 *            If this occurs a WATCHDOG message is logged in the system
 *            event log. In addition, the controller is automatically reset,
 *            restoring the network connection. To eliminate the potential
 *            for the hang ensure that EM_RDTR is set to 0.
 */
#define EM_RDTR                         0

/*
 * Receive Interrupt Absolute Delay Timer (Not valid for 82542/82543/82544)
 * Valid Range: 0-65535 (0=off)
 * Default Value: 64
 *   This value, in units of 1.024 microseconds, limits the delay in which a
 *   receive interrupt is generated. Useful only if EM_RDTR is non-zero,
 *   this value ensures that an interrupt is generated after the initial
 *   packet is received within the set amount of time.  Proper tuning,
 *   along with EM_RDTR, may improve traffic throughput in specific network
 *   conditions.
 */
#define EM_RADV                         64

/*
 * This parameter controls the max duration of transmit watchdog.
 */
#define EM_WATCHDOG                   (10 * hz)

/*
 * This parameter controls when the driver calls the routine to reclaim
 * transmit descriptors.
 */
#define EM_TX_CLEANUP_THRESHOLD	(adapter->num_tx_desc / 8)

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
#define EM_MASTER_SLAVE		e1000_ms_hw_default

/*
 * Micellaneous constants
 */
#define EM_VENDOR_ID                    0x8086
#define EM_FLASH                        0x0014 

#define EM_JUMBO_PBA                    0x00000028
#define EM_DEFAULT_PBA                  0x00000030
#define EM_SMARTSPEED_DOWNSHIFT         3
#define EM_SMARTSPEED_MAX               15
#define EM_MAX_LOOP			10

#define MAX_NUM_MULTICAST_ADDRESSES     128
#define PCI_ANY_ID                      (~0U)
#define ETHER_ALIGN                     2
#define EM_FC_PAUSE_TIME		0x0680
#define EM_EEPROM_APME			0x400;
#define EM_82544_APME			0x0004;

#define EM_QUEUE_IDLE			0
#define EM_QUEUE_WORKING		1
#define EM_QUEUE_HUNG			2

/*
 * TDBA/RDBA should be aligned on 16 byte boundary. But TDLEN/RDLEN should be
 * multiple of 128 bytes. So we align TDBA/RDBA on 128 byte boundary. This will
 * also optimize cache line size effect. H/W supports up to cache line size 128.
 */
#define EM_DBA_ALIGN			128

#define SPEED_MODE_BIT (1<<21)		/* On PCI-E MACs only */

/* PCI Config defines */
#define EM_BAR_TYPE(v)		((v) & EM_BAR_TYPE_MASK)
#define EM_BAR_TYPE_MASK	0x00000001
#define EM_BAR_TYPE_MMEM	0x00000000
#define EM_BAR_TYPE_FLASH	0x0014 
#define EM_BAR_MEM_TYPE(v)	((v) & EM_BAR_MEM_TYPE_MASK)
#define EM_BAR_MEM_TYPE_MASK	0x00000006
#define EM_BAR_MEM_TYPE_32BIT	0x00000000
#define EM_BAR_MEM_TYPE_64BIT	0x00000004
#define EM_MSIX_BAR		3	/* On 82575 */

/* More backward compatibility */
#if __FreeBSD_version < 900000
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

#define EM_MAX_SCATTER		32
#define EM_VFTA_SIZE		128
#define EM_TSO_SIZE		(65535 + sizeof(struct ether_vlan_header))
#define EM_TSO_SEG_SIZE		4096	/* Max dma segment size */
#define EM_MSIX_MASK		0x01F00000 /* For 82574 use */
#define EM_MSIX_LINK		0x01000000 /* For 82574 use */
#define ETH_ZLEN		60
#define ETH_ADDR_LEN		6
#define CSUM_OFFLOAD		7	/* Offload bits in mbuf flag */

/*
 * 82574 has a nonstandard address for EIAC
 * and since its only used in MSIX, and in
 * the em driver only 82574 uses MSIX we can
 * solve it just using this define.
 */
#define EM_EIAC 0x000DC

/*
 * Bus dma allocation structure used by
 * e1000_dma_malloc and e1000_dma_free.
 */
struct em_dma_alloc {
        bus_addr_t              dma_paddr;
        caddr_t                 dma_vaddr;
        bus_dma_tag_t           dma_tag;
        bus_dmamap_t            dma_map;
        bus_dma_segment_t       dma_seg;
        int                     dma_nseg;
};

struct adapter;

struct em_int_delay_info {
	struct adapter *adapter;	/* Back-pointer to the adapter struct */
	int offset;			/* Register offset to read/write */
	int value;			/* Current value in usecs */
};

/*
 * The transmit ring, one per tx queue
 */
struct tx_ring {
        struct adapter          *adapter;
        struct mtx              tx_mtx;
        char                    mtx_name[16];
        u32                     me;
        u32                     msix;
	u32			ims;
        int			queue_status;
        int                     watchdog_time;
	struct em_dma_alloc	txdma;
	struct e1000_tx_desc	*tx_base;
        struct task             tx_task;
        struct taskqueue        *tq;
        u32                     next_avail_desc;
        u32                     next_to_clean;
        struct em_buffer	*tx_buffers;
        volatile u16            tx_avail;
	u32			tx_tso;		/* last tx was tso */
        u16			last_hw_offload;
	u8			last_hw_ipcso;
	u8			last_hw_ipcss;
	u8			last_hw_tucso;
	u8			last_hw_tucss;
#if __FreeBSD_version >= 800000
	struct buf_ring         *br;
#endif
	/* Interrupt resources */
        bus_dma_tag_t           txtag;
	void                    *tag;
	struct resource         *res;
        unsigned long		tx_irq;
        unsigned long		no_desc_avail;
};

/*
 * The Receive ring, one per rx queue
 */
struct rx_ring {
        struct adapter          *adapter;
        u32                     me;
        u32                     msix;
	u32			ims;
        struct mtx              rx_mtx;
        char                    mtx_name[16];
        u32                     payload;
        struct task             rx_task;
        struct taskqueue        *tq;
        struct e1000_rx_desc	*rx_base;
        struct em_dma_alloc	rxdma;
        u32			next_to_refresh;
        u32			next_to_check;
        struct em_buffer	*rx_buffers;
	struct mbuf		*fmp;
	struct mbuf		*lmp;

        /* Interrupt resources */
        void                    *tag;
        struct resource         *res;
        bus_dma_tag_t           rxtag;
	bool			discard;

        /* Soft stats */
        unsigned long		rx_irq;
        unsigned long		rx_discarded;
        unsigned long		rx_packets;
        unsigned long		rx_bytes;
};


/* Our adapter structure */
struct adapter {
	struct ifnet	*ifp;
	struct e1000_hw	hw;

	/* FreeBSD operating-system-specific structures. */
	struct e1000_osdep osdep;
	struct device	*dev;
	struct cdev	*led_dev;

	struct resource *memory;
	struct resource *flash;
	struct resource *msix_mem;

	struct resource	*res;
	void		*tag;
	u32		linkvec;
	u32		ivars;

	struct ifmedia	media;
	struct callout	timer;
	int		msix;
	int		if_flags;
	int		max_frame_size;
	int		min_frame_size;
	int		pause_frames;
	struct mtx	core_mtx;
	int		em_insert_vlan_header;
	u32		ims;
	bool		in_detach;

	/* Task for FAST handling */
	struct task     link_task;
	struct task     que_task;
	struct taskqueue *tq;           /* private task queue */

	eventhandler_tag vlan_attach;
	eventhandler_tag vlan_detach;

	u16	num_vlans;
	u16	num_queues;

        /*
         * Transmit rings:
         *      Allocated at run time, an array of rings.
         */
        struct tx_ring  *tx_rings;
        int             num_tx_desc;
        u32		txd_cmd;

        /*
         * Receive rings:
         *      Allocated at run time, an array of rings.
         */
        struct rx_ring  *rx_rings;
        int             num_rx_desc;
        u32             rx_process_limit;
	u32		rx_mbuf_sz;

	/* Management and WOL features */
	u32		wol;
	bool		has_manage;
	bool		has_amt;

	/* Multicast array memory */
	u8		*mta;

	/*
	** Shadow VFTA table, this is needed because
	** the real vlan filter table gets cleared during
	** a soft reset and the driver needs to be able
	** to repopulate it.
	*/
	u32		shadow_vfta[EM_VFTA_SIZE];

	/* Info about the interface */
	u16		link_active;
	u16		fc;
	u16		link_speed;
	u16		link_duplex;
	u32		smartspeed;

	struct em_int_delay_info tx_int_delay;
	struct em_int_delay_info tx_abs_int_delay;
	struct em_int_delay_info rx_int_delay;
	struct em_int_delay_info rx_abs_int_delay;
	struct em_int_delay_info tx_itr;

	/* Misc stats maintained by the driver */
	unsigned long	dropped_pkts;
	unsigned long	mbuf_alloc_failed;
	unsigned long	mbuf_cluster_failed;
	unsigned long	no_tx_map_avail;
        unsigned long	no_tx_dma_setup;
	unsigned long	rx_overruns;
	unsigned long	watchdog_events;
	unsigned long	link_irq;

	struct e1000_hw_stats stats;
};

/********************************************************************************
 * vendor_info_array
 *
 * This array contains the list of Subvendor/Subdevice IDs on which the driver
 * should load.
 *
 ********************************************************************************/
typedef struct _em_vendor_info_t {
	unsigned int vendor_id;
	unsigned int device_id;
	unsigned int subvendor_id;
	unsigned int subdevice_id;
	unsigned int index;
} em_vendor_info_t;

struct em_buffer {
	int		next_eop;  /* Index of the desc to watch */
        struct mbuf    *m_head;
        bus_dmamap_t    map;         /* bus_dma map for packet */
};


/*
** Find the number of unrefreshed RX descriptors
*/
static inline u16
e1000_rx_unrefreshed(struct rx_ring *rxr)
{
	struct adapter	*adapter = rxr->adapter;

	if (rxr->next_to_check > rxr->next_to_refresh)
		return (rxr->next_to_check - rxr->next_to_refresh - 1);
	else
		return ((adapter->num_rx_desc + rxr->next_to_check) -
		    rxr->next_to_refresh - 1); 
}

#define	EM_CORE_LOCK_INIT(_sc, _name) \
	mtx_init(&(_sc)->core_mtx, _name, "EM Core Lock", MTX_DEF)
#define	EM_TX_LOCK_INIT(_sc, _name) \
	mtx_init(&(_sc)->tx_mtx, _name, "EM TX Lock", MTX_DEF)
#define	EM_RX_LOCK_INIT(_sc, _name) \
	mtx_init(&(_sc)->rx_mtx, _name, "EM RX Lock", MTX_DEF)
#define	EM_CORE_LOCK_DESTROY(_sc)	mtx_destroy(&(_sc)->core_mtx)
#define	EM_TX_LOCK_DESTROY(_sc)		mtx_destroy(&(_sc)->tx_mtx)
#define	EM_RX_LOCK_DESTROY(_sc)		mtx_destroy(&(_sc)->rx_mtx)
#define	EM_CORE_LOCK(_sc)		mtx_lock(&(_sc)->core_mtx)
#define	EM_TX_LOCK(_sc)			mtx_lock(&(_sc)->tx_mtx)
#define	EM_TX_TRYLOCK(_sc)		mtx_trylock(&(_sc)->tx_mtx)
#define	EM_RX_LOCK(_sc)			mtx_lock(&(_sc)->rx_mtx)
#define	EM_CORE_UNLOCK(_sc)		mtx_unlock(&(_sc)->core_mtx)
#define	EM_TX_UNLOCK(_sc)		mtx_unlock(&(_sc)->tx_mtx)
#define	EM_RX_UNLOCK(_sc)		mtx_unlock(&(_sc)->rx_mtx)
#define	EM_CORE_LOCK_ASSERT(_sc)	mtx_assert(&(_sc)->core_mtx, MA_OWNED)
#define	EM_TX_LOCK_ASSERT(_sc)		mtx_assert(&(_sc)->tx_mtx, MA_OWNED)
#define	EM_RX_LOCK_ASSERT(_sc)		mtx_assert(&(_sc)->rx_mtx, MA_OWNED)

#endif /* _EM_H_DEFINED_ */
