/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2016 Nicole Graziano <nicole@nextbsd.org>
 * All rights reserved.
 * Copyright (c) 2021 Rubicon Communications, LLC (Netgate)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "opt_ddb.h"
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_rss.h"

#ifdef HAVE_KERNEL_OPTION_HEADERS
#include "opt_device_polling.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#ifdef DDB
#include <sys/types.h>
#include <ddb/ddb.h>
#endif
#include <sys/buf_ring.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/smp.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>
#include <sys/eventhandler.h>
#include <machine/bus.h>
#include <machine/resource.h>

#include <net/bpf.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/iflib.h>

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
#include <dev/led/led.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include "igc_api.h"
#include "igc_i225.h"
#include "ifdi_if.h"


#ifndef _IGC_H_DEFINED_
#define _IGC_H_DEFINED_


/* Tunables */

/*
 * IGC_MAX_TXD: Maximum number of Transmit Descriptors
 * Valid Range: 128-4096
 * Default Value: 1024
 *   This value is the number of transmit descriptors allocated by the driver.
 *   Increasing this value allows the driver to queue more transmits. Each
 *   descriptor is 16 bytes.
 *   Since TDLEN should be multiple of 128bytes, the number of transmit
 *   desscriptors should meet the following condition.
 *      (num_tx_desc * sizeof(struct igc_tx_desc)) % 128 == 0
 */
#define IGC_MIN_TXD		128
#define IGC_MAX_TXD		4096
#define IGC_DEFAULT_TXD          1024
#define IGC_DEFAULT_MULTI_TXD	4096
#define IGC_MAX_TXD		4096

/*
 * IGC_MAX_RXD - Maximum number of receive Descriptors
 * Valid Range: 128-4096
 * Default Value: 1024
 *   This value is the number of receive descriptors allocated by the driver.
 *   Increasing this value allows the driver to buffer more incoming packets.
 *   Each descriptor is 16 bytes.  A receive buffer is also allocated for each
 *   descriptor. The maximum MTU size is 16110.
 *   Since TDLEN should be multiple of 128bytes, the number of transmit
 *   desscriptors should meet the following condition.
 *      (num_tx_desc * sizeof(struct igc_tx_desc)) % 128 == 0
 */
#define IGC_MIN_RXD		128
#define IGC_MAX_RXD		4096
#define IGC_DEFAULT_RXD		1024
#define IGC_DEFAULT_MULTI_RXD	4096
#define IGC_MAX_RXD		4096

/*
 * This parameter controls whether or not autonegotation is enabled.
 *              0 - Disable autonegotiation
 *              1 - Enable  autonegotiation
 */
#define DO_AUTO_NEG		true

/* Tunables -- End */

#define AUTONEG_ADV_DEFAULT	(ADVERTISE_10_HALF | ADVERTISE_10_FULL | \
				ADVERTISE_100_HALF | ADVERTISE_100_FULL | \
				ADVERTISE_1000_FULL | ADVERTISE_2500_FULL)

#define AUTO_ALL_MODES		0

/*
 * Micellaneous constants
 */
#define MAX_NUM_MULTICAST_ADDRESSES     128
#define IGC_FC_PAUSE_TIME		0x0680

#define IGC_TXPBSIZE		20408
#define IGC_PKTTYPE_MASK	0x0000FFF0
#define IGC_DMCTLX_DCFLUSH_DIS	0x80000000  /* Disable DMA Coalesce Flush */

#define IGC_RX_PTHRESH			8
#define IGC_RX_HTHRESH			8
#define IGC_RX_WTHRESH			4

#define IGC_TX_PTHRESH			8
#define IGC_TX_HTHRESH			1

/*
 * TDBA/RDBA should be aligned on 16 byte boundary. But TDLEN/RDLEN should be
 * multiple of 128 bytes. So we align TDBA/RDBA on 128 byte boundary. This will
 * also optimize cache line size effect. H/W supports up to cache line size 128.
 */
#define IGC_DBA_ALIGN			128

#define IGC_MSIX_BAR			3

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

#define IGC_MAX_SCATTER			40
#define IGC_VFTA_SIZE			128
#define IGC_TSO_SIZE			65535
#define IGC_TSO_SEG_SIZE		4096	/* Max dma segment size */
#define IGC_CSUM_OFFLOAD	(CSUM_IP | CSUM_IP_UDP | CSUM_IP_TCP | \
				 CSUM_IP_SCTP | CSUM_IP6_UDP | CSUM_IP6_TCP | \
				 CSUM_IP6_SCTP)	/* Offload bits in mbuf flag */

struct igc_adapter;

struct igc_int_delay_info {
	struct igc_adapter *adapter;	/* Back-pointer to the adapter struct */
	int offset;			/* Register offset to read/write */
	int value;			/* Current value in usecs */
};

/*
 * The transmit ring, one per tx queue
 */
struct tx_ring {
        struct igc_adapter	*adapter;
	struct igc_tx_desc	*tx_base;
	uint64_t                tx_paddr;
	qidx_t			*tx_rsq;
	uint8_t			me;
	qidx_t			tx_rs_cidx;
	qidx_t			tx_rs_pidx;
	qidx_t			tx_cidx_processed;
	/* Interrupt resources */
	void                    *tag;
	struct resource         *res;
        unsigned long		tx_irq;

	/* Saved csum offloading context information */
	int			csum_flags;
	int			csum_lhlen;
	int			csum_iphlen;

	int			csum_thlen;
	int			csum_mss;
	int			csum_pktlen;

	uint32_t		csum_txd_upper;
	uint32_t		csum_txd_lower; /* last field */
};

/*
 * The Receive ring, one per rx queue
 */
struct rx_ring {
        struct igc_adapter      *adapter;
        struct igc_rx_queue     *que;
        u32                     me;
        u32                     payload;
        union igc_rx_desc_extended	*rx_base;
        uint64_t                rx_paddr;

        /* Interrupt resources */
        void                    *tag;
        struct resource         *res;

        /* Soft stats */
        unsigned long		rx_irq;
        unsigned long		rx_discarded;
        unsigned long		rx_packets;
        unsigned long		rx_bytes;
};

struct igc_tx_queue {
	struct igc_adapter      *adapter;
        u32                     msix;
	u32			eims;		/* This queue's EIMS bit */
	u32                     me;
	struct tx_ring          txr;
};

struct igc_rx_queue {
	struct igc_adapter     *adapter;
	u32                    me;
	u32                    msix;
	u32                    eims;
	struct rx_ring         rxr;
	u64                    irqs;
	struct if_irq          que_irq;
};

/* Our adapter structure */
struct igc_adapter {
	if_t		ifp;
	struct igc_hw	hw;

        if_softc_ctx_t shared;
        if_ctx_t ctx;
#define tx_num_queues shared->isc_ntxqsets
#define rx_num_queues shared->isc_nrxqsets
#define intr_type shared->isc_intr
	/* FreeBSD operating-system-specific structures. */
	struct igc_osdep osdep;
	device_t	dev;
	struct cdev	*led_dev;

        struct igc_tx_queue *tx_queues;
        struct igc_rx_queue *rx_queues;
        struct if_irq   irq;

	struct resource *memory;
	struct resource *flash;
	struct resource	*ioport;

	struct resource	*res;
	void		*tag;
	u32		linkvec;
	u32		ivars;

	struct ifmedia	*media;
	int		msix;
	int		if_flags;
	int		igc_insert_vlan_header;
	u32		ims;

	u32		flags;
	/* Task for FAST handling */
	struct grouptask link_task;

        u32		txd_cmd;

	u32		rx_mbuf_sz;

	/* Management and WOL features */
	u32		wol;

	/* Multicast array memory */
	u8		*mta;

	/* Info about the interface */
	u16		link_active;
	u16		fc;
	u16		link_speed;
	u16		link_duplex;
	u32		smartspeed;
	u32		dmac;
	int		link_mask;

	u64		que_mask;

	struct igc_int_delay_info tx_int_delay;
	struct igc_int_delay_info tx_abs_int_delay;
	struct igc_int_delay_info rx_int_delay;
	struct igc_int_delay_info rx_abs_int_delay;
	struct igc_int_delay_info tx_itr;

	/* Misc stats maintained by the driver */
	unsigned long	dropped_pkts;
	unsigned long	link_irq;
	unsigned long	rx_overruns;
	unsigned long	watchdog_events;

	struct igc_hw_stats stats;
	u16		vf_ifp;
};

void igc_dump_rs(struct igc_adapter *);

#define IGC_RSSRK_SIZE	4
#define IGC_RSSRK_VAL(key, i)		(key[(i) * IGC_RSSRK_SIZE] | \
					 key[(i) * IGC_RSSRK_SIZE + 1] << 8 | \
					 key[(i) * IGC_RSSRK_SIZE + 2] << 16 | \
					 key[(i) * IGC_RSSRK_SIZE + 3] << 24)
#endif /* _IGC_H_DEFINED_ */
