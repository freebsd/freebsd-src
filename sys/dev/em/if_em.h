/**************************************************************************

Copyright (c) 2001-2003, Intel Corporation
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
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/sockio.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <net/bpf.h>
#include <net/if_types.h>
#include <net/if_vlan_var.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/clock.h>
#include <pci/pcivar.h>
#include <pci/pcireg.h>
#include "opt_bdg.h"

#include <dev/em/if_em_hw.h>

/* Tunables */

/*
 * FlowControl
 * Valid Range: 0-3 (0=none, 1=Rx only, 2=Tx only, 3=Rx&Tx)
 * Default: Read flow control settings from the EEPROM
 *   This parameter controls the automatic generation(Tx) and response(Rx) to
 *   Ethernet PAUSE frames.
 */


/*
 * TxDescriptors
 * Valid Range: 80-256 for 82542 and 82543-based adapters
 *            80-4096 for 82540, 82544, 82545, and 82546-based adapters
 * Default Value: 256
 *   This value is the number of transmit descriptors allocated by the driver.
 *   Increasing this value allows the driver to queue more transmits. Each
 *   descriptor is 16 bytes.
 */
#define EM_MAX_TXD                      256

/*
 * RxDescriptors
 * Valid Range: 80-256 for 82542 and 82543-based adapters
 *            80-4096 for 82540, 82544, 82545, and 82546-based adapters
 * Default Value: 256
 *   This value is the number of receive descriptors allocated by the driver.
 *   Increasing this value allows the driver to buffer more incoming packets.
 *   Each descriptor is 16 bytes.  A receive buffer is also allocated for each
 *   descriptor. The maximum MTU size is 16110.
 *
 */
#define EM_MAX_RXD                      256

/*
 * TxIntDelay
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
 * TxAbsIntDelay (82540, 82545, and 82546-based adapters only)
 * Valid Range: 0-65535 (0=off)
 * Default Value: 64
 *   This value, in units of 1.024 microseconds, limits the delay in which a
 *   transmit interrupt is generated. Useful only if TxIntDelay is non-zero,
 *   this value ensures that an interrupt is generated after the initial
 *   packet is sent on the wire within the set amount of time.  Proper tuning,
 *   along with TxIntDelay, may improve traffic throughput in specific
 *   network conditions.
 */
#define EM_TADV                         64

/*
 * RxIntDelay
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
 *   CAUTION: When setting RxIntDelay to a value other than 0, adapters
 *            may hang (stop transmitting) under certain network conditions.
 *            If this occurs a WATCHDOG message is logged in the system event log.
 *            In addition, the controller is automatically reset, restoring the
 *            network connection. To eliminate the potential for the hang
 *            ensure that RxIntDelay is set to 0.
 */
#define EM_RDTR                         0

/*
 * RxAbsIntDelay (82540, 82545, and 82546-based adapters only)
 * Valid Range: 0-65535 (0=off)
 * Default Value: 64
 *   This value, in units of 1.024 microseconds, limits the delay in which a
 *   receive interrupt is generated. Useful only if RxIntDelay is non-zero,
 *   this value ensures that an interrupt is generated after the initial
 *   packet is received within the set amount of time.  Proper tuning,
 *   along with RxIntDelay, may improve traffic throughput in specific network
 *   conditions.
 */
#define EM_RADV                         64


/*
 * This parameter controls the maximum no of times the driver will loop
 * in the isr.
 *           Minimum Value = 1
 */
#define EM_MAX_INTR                     3


/*
 * This parameter determines when the hardware will report that it is
 * done with the packet.
 *           0 - "Done" is reported when the packet has been sent on the wire
 *           1 - "Done" is reported when the packet has been DMA'ed and is on chip.
 *           2 -  Determine the best method.
 */
#define EM_REPORT_TX_EARLY              2

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
#define EM_TX_CLEANUP_THRESHOLD         EM_MAX_TXD / 8

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
#define WAIT_FOR_AUTO_NEG_DEFAULT       1


/* Tunables -- End */

#define AUTONEG_ADV_DEFAULT             (ADVERTISE_10_HALF | ADVERTISE_10_FULL | \
                                         ADVERTISE_100_HALF | ADVERTISE_100_FULL | \
                                         ADVERTISE_1000_FULL)

#define EM_VENDOR_ID                    0x8086
#define EM_MMBA                         0x0010 /* Mem base address */
#define EM_ROUNDUP(size, unit) (((size) + (unit) - 1) & ~((unit) - 1))
#define EM_JUMBO_PBA                    0x00000028
#define EM_DEFAULT_PBA                  0x00000030

#define IOCTL_CMD_TYPE                  u_long
#define MAX_NUM_MULTICAST_ADDRESSES     128
#define PCI_ANY_ID                      (~0U)

#ifndef ETHER_ALIGN
#define ETHER_ALIGN                     2
#endif

#define QTAG_TYPE                       0x8100

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

#ifdef __alpha__
	#undef vtophys
	#define vtophys(va)     alpha_XXX_dmamap((vm_offset_t)(va))
#endif /* __alpha__ */

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
};


typedef enum _XSUM_CONTEXT_T {
	OFFLOAD_NONE,
	OFFLOAD_TCP_IP,
	OFFLOAD_UDP_IP
} XSUM_CONTEXT_T;

/* Our adapter structure */
struct adapter {
	struct arpcom   interface_data;
	struct adapter *next;
	struct adapter *prev;
	struct em_hw    hw;

	/* FreeBSD operating-system-specific structures */
	struct em_osdep osdep;
	struct device   *dev;
	struct resource *res_memory;
	struct resource *res_ioport;
	struct resource *res_interrupt;
	void            *int_handler_tag;
	struct ifmedia  media;
	struct callout_handle timer_handle;
	int             io_rid;
	u_int8_t        unit;

	/* Info about the board itself */
	u_int32_t       part_num;
	u_int8_t        link_active;
	u_int16_t       link_speed;
	u_int16_t       link_duplex;
	u_int32_t       tx_int_delay;
	u_int32_t       tx_abs_int_delay;
	u_int32_t       rx_int_delay;
	u_int32_t       rx_abs_int_delay;

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
        struct em_tx_desc *tx_desc_base;
        u_int32_t          next_avail_tx_desc;
	u_int32_t          oldest_used_tx_desc;
        volatile u_int16_t num_tx_desc_avail;
        u_int16_t          num_tx_desc;
        u_int32_t          txd_cmd;
        struct em_buffer   *tx_buffer_area;

	/* 
	 * Receive definitions
         *
         * we have an array of num_rx_desc rx_desc (handled by the
         * controller), and paired with an array of rx_buffers
         * (at rx_buffer_area).
         * The next pair to check on receive is at offset next_rx_desc_to_check
         */
        struct em_rx_desc *rx_desc_base;
        u_int32_t          next_rx_desc_to_check;
        u_int16_t          num_rx_desc;
        u_int32_t          rx_buffer_len;
        struct em_buffer   *rx_buffer_area;


	/* Jumbo frame */
	struct mbuf     *fmp;
	struct mbuf     *lmp;


	/* Misc stats maintained by the driver */
	unsigned long   dropped_pkts;
	unsigned long   mbuf_alloc_failed;
	unsigned long   mbuf_cluster_failed;
	unsigned long   no_tx_desc_avail1;
	unsigned long   no_tx_desc_avail2;
#ifdef DBG_STATS
	unsigned long   no_pkts_avail;
	unsigned long   clean_tx_interrupts;

#endif

	struct em_hw_stats stats;
};

#endif                                                  /* _EM_H_DEFINED_ */
