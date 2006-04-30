/**************************************************************************

Copyright (c) 2001-2005, Intel Corporation
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

***************************************************************************/

/*$FreeBSD$*/

#ifndef _EM_H_DEFINED_
#define _EM_H_DEFINED_


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>

#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <net/bpf.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <net/if_types.h>
#include <net/if_vlan_var.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include <dev/em/if_em_hw.h>

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
 *      (num_tx_desc * sizeof(struct em_tx_desc)) % 128 == 0
 */
#define EM_MIN_TXD		80
#define EM_MAX_TXD_82543	256
#define EM_MAX_TXD		4096
#define EM_DEFAULT_TXD		EM_MAX_TXD_82543

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
 *      (num_tx_desc * sizeof(struct em_tx_desc)) % 128 == 0
 */
#define EM_MIN_RXD		80
#define EM_MAX_RXD_82543	256
#define EM_MAX_RXD		4096
#define EM_DEFAULT_RXD		EM_MAX_RXD_82543

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
 * EM_TADV - Transmit Absolute Interrupt Delay Value (Not valid for 82542/82543/82544)
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
 *            If this occurs a WATCHDOG message is logged in the system event log.
 *            In addition, the controller is automatically reset, restoring the
 *            network connection. To eliminate the potential for the hang
 *            ensure that EM_RDTR is set to 0.
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
 * Inform the stack about transmit checksum offload capabilities.
 */
#define EM_CHECKSUM_FEATURES            (CSUM_TCP | CSUM_UDP)

/*
 * This parameter controls the duration of transmit watchdog timer.
 */
#define EM_TX_TIMEOUT                   5    /* set to 5 seconds */

/*
 * This parameter controls when the driver calls the routine to reclaim
 * transmit descriptors.
 */
#define EM_TX_CLEANUP_THRESHOLD		(adapter->num_tx_desc / 8)

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

/*
 * EM_MASTER_SLAVE is only defined to enable a workaround for a known compatibility issue
 * with 82541/82547 devices and some switches.  See the "Known Limitations" section of
 * the README file for a complete description and a list of affected switches.
 *
 *              0 = Hardware default
 *              1 = Master mode
 *              2 = Slave mode
 *              3 = Auto master/slave
 */
/* #define EM_MASTER_SLAVE      2 */

/* Tunables -- End */

#define AUTONEG_ADV_DEFAULT             (ADVERTISE_10_HALF | ADVERTISE_10_FULL | \
                                         ADVERTISE_100_HALF | ADVERTISE_100_FULL | \
                                         ADVERTISE_1000_FULL)

#define EM_VENDOR_ID                    0x8086

#define EM_JUMBO_PBA                    0x00000028
#define EM_DEFAULT_PBA                  0x00000030
#define EM_SMARTSPEED_DOWNSHIFT         3
#define EM_SMARTSPEED_MAX               15


#define MAX_NUM_MULTICAST_ADDRESSES     128
#define PCI_ANY_ID                      (~0U)
#define ETHER_ALIGN                     2

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


/* Supported RX Buffer Sizes */
#define EM_RXBUFFER_2048        2048
#define EM_RXBUFFER_4096        4096
#define EM_RXBUFFER_8192        8192
#define EM_RXBUFFER_16384      16384

#define EM_MAX_SCATTER            64

/* ******************************************************************************
 * vendor_info_array
 *
 * This array contains the list of Subvendor/Subdevice IDs on which the driver
 * should load.
 *
 * ******************************************************************************/
typedef struct _em_vendor_info_t {
	unsigned int vendor_id;
	unsigned int device_id;
	unsigned int subvendor_id;
	unsigned int subdevice_id;
	unsigned int index;
} em_vendor_info_t;


struct em_buffer {
        struct mbuf    *m_head;
        bus_dmamap_t    map;         /* bus_dma map for packet */
};

/*
 * Bus dma allocation structure used by
 * em_dma_malloc and em_dma_free.
 */
struct em_dma_alloc {
        bus_addr_t              dma_paddr;
        caddr_t                 dma_vaddr;
        bus_dma_tag_t           dma_tag;
        bus_dmamap_t            dma_map;
        bus_dma_segment_t       dma_seg;
        int                     dma_nseg;
};

typedef enum _XSUM_CONTEXT_T {
	OFFLOAD_NONE,
	OFFLOAD_TCP_IP,
	OFFLOAD_UDP_IP
} XSUM_CONTEXT_T;

struct adapter;
struct em_int_delay_info {
	struct adapter *adapter;	/* Back-pointer to the adapter struct */
	int offset;			/* Register offset to read/write */
	int value;			/* Current value in usecs */
};

/* For 82544 PCIX  Workaround */
typedef struct _ADDRESS_LENGTH_PAIR
{
    u_int64_t   address;
    u_int32_t   length;
} ADDRESS_LENGTH_PAIR, *PADDRESS_LENGTH_PAIR;

typedef struct _DESCRIPTOR_PAIR
{
    ADDRESS_LENGTH_PAIR descriptor[4];
    u_int32_t   elements;
} DESC_ARRAY, *PDESC_ARRAY;

/* Our adapter structure */
struct adapter {
	struct ifnet   *ifp;
	struct em_hw    hw;

	/* FreeBSD operating-system-specific structures */
	struct em_osdep osdep;
	struct device   *dev;
	struct resource *res_memory;
	struct resource *res_ioport;
	struct resource *res_interrupt;
	void            *int_handler_tag;
	struct ifmedia  media;
	struct callout	timer;
	struct callout	tx_fifo_timer;
	int             io_rid;
	u_int8_t        unit;
	struct mtx	mtx;
	int		em_insert_vlan_header;

	/* Info about the board itself */
	u_int32_t       part_num;
	u_int8_t        link_active;
	u_int16_t       link_speed;
	u_int16_t       link_duplex;
	u_int32_t       smartspeed;
	struct em_int_delay_info tx_int_delay;
	struct em_int_delay_info tx_abs_int_delay;
	struct em_int_delay_info rx_int_delay;
	struct em_int_delay_info rx_abs_int_delay;

	XSUM_CONTEXT_T  active_checksum_context;

	/*
         * Transmit definitions
         *
         * We have an array of num_tx_desc descriptors (handled
         * by the controller) paired with an array of tx_buffers
         * (at tx_buffer_area).
         * The index of the next available descriptor is next_avail_tx_desc.
         * The number of remaining tx_desc is num_tx_desc_avail.
         */
	struct em_dma_alloc txdma;              /* bus_dma glue for tx desc */
        struct em_tx_desc *tx_desc_base;
        u_int32_t          next_avail_tx_desc;
	u_int32_t          oldest_used_tx_desc;
        volatile u_int16_t num_tx_desc_avail;
        u_int16_t          num_tx_desc;
        u_int32_t          txd_cmd;
        struct em_buffer   *tx_buffer_area;
	bus_dma_tag_t      txtag;               /* dma tag for tx */

	/* 
	 * Receive definitions
         *
         * we have an array of num_rx_desc rx_desc (handled by the
         * controller), and paired with an array of rx_buffers
         * (at rx_buffer_area).
         * The next pair to check on receive is at offset next_rx_desc_to_check
         */
	struct em_dma_alloc rxdma;              /* bus_dma glue for rx desc */
        struct em_rx_desc *rx_desc_base;
        u_int32_t          next_rx_desc_to_check;
        u_int16_t          num_rx_desc;
        u_int32_t          rx_buffer_len;
        struct em_buffer   *rx_buffer_area;
	bus_dma_tag_t      rxtag;

	/* Jumbo frame */
	struct mbuf        *fmp;
	struct mbuf        *lmp;

	/* Misc stats maintained by the driver */
	unsigned long   dropped_pkts;
	unsigned long   mbuf_alloc_failed;
	unsigned long   mbuf_cluster_failed;
	unsigned long   no_tx_desc_avail1;
	unsigned long   no_tx_desc_avail2;
	unsigned long   no_tx_map_avail;
        unsigned long   no_tx_dma_setup;
	unsigned long	watchdog_events;
	unsigned long	rx_overruns;

	/* Used in for 82547 10Mb Half workaround */
	#define EM_PBA_BYTES_SHIFT	0xA
	#define EM_TX_HEAD_ADDR_SHIFT	7
	#define EM_PBA_TX_MASK		0xFFFF0000
	#define EM_FIFO_HDR              0x10

	#define EM_82547_PKT_THRESH      0x3e0

	u_int32_t       tx_fifo_size;
	u_int32_t       tx_fifo_head;
	u_int32_t       tx_fifo_head_addr;
	u_int64_t       tx_fifo_reset_cnt;
	u_int64_t       tx_fifo_wrk_cnt;
	u_int32_t       tx_head_addr;

        /* For 82544 PCIX Workaround */
        boolean_t       pcix_82544;
	boolean_t       in_detach;

	struct em_hw_stats stats;
};

#define	EM_LOCK_INIT(_sc, _name) \
	mtx_init(&(_sc)->mtx, _name, MTX_NETWORK_LOCK, MTX_DEF)
#define	EM_LOCK_DESTROY(_sc)	mtx_destroy(&(_sc)->mtx)
#define	EM_LOCK(_sc)		mtx_lock(&(_sc)->mtx)
#define	EM_UNLOCK(_sc)		mtx_unlock(&(_sc)->mtx)
#define	EM_LOCK_ASSERT(_sc)	mtx_assert(&(_sc)->mtx, MA_OWNED)

#endif                                                  /* _EM_H_DEFINED_ */
