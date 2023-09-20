/* SPDX-License-Identifier: BSD-3-Clause */
/*  Copyright (c) 2021, Intel Corporation
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *   3. Neither the name of the Intel Corporation nor the names of its
 *      contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file iavf_iflib.h
 * @brief main header for the iflib driver
 *
 * Contains definitions for various driver structures used throughout the
 * driver code. This header is used by the iflib implementation.
 */
#ifndef _IAVF_IFLIB_H_
#define _IAVF_IFLIB_H_

#include "iavf_opts.h"

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
#include <sys/eventhandler.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_arp.h>
#include <net/bpf.h>
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
#include <sys/pciio.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/clock.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <sys/proc.h>
#include <sys/endian.h>
#include <sys/taskqueue.h>
#include <sys/pcpu.h>
#include <sys/smp.h>
#include <sys/sbuf.h>
#include <machine/smp.h>
#include <machine/stdarg.h>
#include <net/ethernet.h>
#include <net/iflib.h>
#include "ifdi_if.h"

#include "iavf_lib.h"

#define IAVF_CSUM_TCP \
	(CSUM_IP_TCP|CSUM_IP_TSO|CSUM_IP6_TSO|CSUM_IP6_TCP)
#define IAVF_CSUM_UDP \
	(CSUM_IP_UDP|CSUM_IP6_UDP)
#define IAVF_CSUM_SCTP \
	(CSUM_IP_SCTP|CSUM_IP6_SCTP)
#define IAVF_CSUM_IPV4 \
	(CSUM_IP|CSUM_IP_TSO)

#define IAVF_CAPS \
	(IFCAP_TSO4 | IFCAP_TSO6 | \
	 IFCAP_TXCSUM | IFCAP_TXCSUM_IPV6 | \
	 IFCAP_RXCSUM | IFCAP_RXCSUM_IPV6 | \
	 IFCAP_VLAN_HWFILTER | IFCAP_VLAN_HWTSO | \
	 IFCAP_VLAN_HWTAGGING | IFCAP_VLAN_HWCSUM | \
	 IFCAP_VLAN_MTU | IFCAP_JUMBO_MTU | IFCAP_LRO)

#define iavf_sc_from_ctx(_ctx) \
    ((struct iavf_sc *)iflib_get_softc(_ctx))

/* Use the correct assert function for each lock type */
#define IAVF_VC_LOCK(_sc)                mtx_lock(&(_sc)->vc_mtx)
#define IAVF_VC_UNLOCK(_sc)              mtx_unlock(&(_sc)->vc_mtx)
#define IAVF_VC_LOCK_DESTROY(_sc)        mtx_destroy(&(_sc)->vc_mtx)
#define IAVF_VC_TRYLOCK(_sc)             mtx_trylock(&(_sc)->vc_mtx)
#define IAVF_VC_LOCK_ASSERT(_sc)         mtx_assert(&(_sc)->vc_mtx, MA_OWNED)

/**
 * @struct tx_ring
 * @brief Transmit ring control struct
 *
 * Structure used to track the hardware Tx ring data.
 */
struct tx_ring {
        struct iavf_tx_queue	*que;
	u32			tail;
	struct iavf_tx_desc	*tx_base;
	u64			tx_paddr;
	u32			packets;
	u32			me;

	/*
	 * For reporting completed packet status
	 * in descriptor writeback mdoe
	 */
	qidx_t			*tx_rsq;
	qidx_t			tx_rs_cidx;
	qidx_t			tx_rs_pidx;
	qidx_t			tx_cidx_processed;

	/* Used for Dynamic ITR calculation */
	u32			bytes;
	u32			itr;
	u32			latency;

	/* Soft Stats */
	u64			tx_bytes;
	u64			tx_packets;
	u64			mss_too_small;
};

/**
 * @struct rx_ring
 * @brief Receive ring control struct
 *
 * Structure used to track the hardware Rx ring data.
 */
struct rx_ring {
        struct iavf_rx_queue	*que;
	union iavf_rx_desc	*rx_base;
	uint64_t		rx_paddr;
	bool			discard;
	u32			itr;
	u32			latency;
	u32			mbuf_sz;
	u32			tail;
	u32			me;

	/* Used for Dynamic ITR calculation */
	u32			packets;
	u32			bytes;

	/* Soft stats */
	u64			rx_packets;
	u64			rx_bytes;
	u64			desc_errs;
};

/**
 * @struct iavf_tx_queue
 * @brief Driver Tx queue structure
 *
 * Structure to track the Tx ring, IRQ, MSI-X vector, and some software stats
 * for a Tx queue.
 */
struct iavf_tx_queue {
	struct iavf_vsi		*vsi;
	struct tx_ring		txr;
	struct if_irq		que_irq;
	u32			msix;

	/* Stats */
	u64			irqs;
	u64			tso;
	u32			pkt_too_small;
};

/**
 * @struct iavf_rx_queue
 * @brief Driver Rx queue structure
 *
 * Structure to track the Rx ring, IRQ, MSI-X vector, and some software stats
 * for an Rx queue.
 */
struct iavf_rx_queue {
	struct iavf_vsi		*vsi;
	struct rx_ring		rxr;
	struct if_irq		que_irq;
	u32			msix;

	/* Stats */
	u64			irqs;
};

/**
 * @struct iavf_vsi
 * @brief Virtual Station Interface
 *
 * Data tracking a VSI for an iavf device.
 */
struct iavf_vsi {
	if_ctx_t		ctx;
	if_softc_ctx_t		shared;
	if_t			ifp;
	struct iavf_sc		*back;
	device_t		dev;
	struct iavf_hw		*hw;

	int			id;
	u16			num_rx_queues;
	u16			num_tx_queues;
	u32			rx_itr_setting;
	u32			tx_itr_setting;
	u16			max_frame_size;
	bool			enable_head_writeback;

	bool			link_active;

	struct iavf_tx_queue	*tx_queues;
	struct iavf_rx_queue	*rx_queues;
	struct if_irq		irq;

	u16			num_vlans;
	u16			num_macs;

	/* Per-VSI stats from hardware */
	struct iavf_eth_stats	eth_stats;
	struct iavf_eth_stats	eth_stats_offsets;
	bool			stat_offsets_loaded;
	/* VSI stat counters */
	u64			ipackets;
	u64			ierrors;
	u64			opackets;
	u64			oerrors;
	u64			ibytes;
	u64			obytes;
	u64			imcasts;
	u64			omcasts;
	u64			iqdrops;
	u64			oqdrops;
	u64			noproto;

	/* Misc. */
	u64			flags;
	struct sysctl_oid	*vsi_node;
	struct sysctl_ctx_list  sysctl_ctx;
};

/**
 * @struct iavf_mac_filter
 * @brief MAC Address filter data
 *
 * Entry in the MAC filter list describing a MAC address filter used to
 * program hardware to filter a specific MAC address.
 */
struct iavf_mac_filter {
	SLIST_ENTRY(iavf_mac_filter)  next;
	u8      macaddr[ETHER_ADDR_LEN];
	u16     flags;
};

/**
 * @struct mac_list
 * @brief MAC filter list head
 *
 * List head type for a singly-linked list of MAC address filters.
 */
SLIST_HEAD(mac_list, iavf_mac_filter);

/**
 * @struct iavf_vlan_filter
 * @brief VLAN filter data
 *
 * Entry in the VLAN filter list describing a VLAN filter used to
 * program hardware to filter traffic on a specific VLAN.
 */
struct iavf_vlan_filter {
	SLIST_ENTRY(iavf_vlan_filter)  next;
	u16     vlan;
	u16     flags;
};

/**
 * @struct vlan_list
 * @brief VLAN filter list head
 *
 * List head type for a singly-linked list of VLAN filters.
 */
SLIST_HEAD(vlan_list, iavf_vlan_filter);

/**
 * @struct iavf_sc
 * @brief Main context structure for the iavf driver
 *
 * Software context structure used to store information about a single device
 * that is loaded by the iavf driver.
 */
struct iavf_sc {
	struct iavf_vsi		vsi;

	struct iavf_hw		hw;
	struct iavf_osdep	osdep;
	device_t		dev;

	struct resource		*pci_mem;

	/* driver state flags, only access using atomic functions */
	u32			state;

	struct ifmedia		*media;
	struct virtchnl_version_info version;
	enum iavf_dbg_mask	dbg_mask;
	u16			promisc_flags;

	bool			link_up;
	union {
		enum virtchnl_link_speed link_speed;
		u32		link_speed_adv;
	};

	/* Tunable settings */
	int			tx_itr;
	int			rx_itr;
	int			dynamic_tx_itr;
	int			dynamic_rx_itr;

	/* Filter lists */
	struct mac_list		*mac_filters;
	struct vlan_list	*vlan_filters;

	/* Virtual comm channel */
	struct virtchnl_vf_resource *vf_res;
	struct virtchnl_vsi_resource *vsi_res;

	/* Misc stats maintained by the driver */
	u64			admin_irq;

	/* Buffer used for reading AQ responses */
	u8			aq_buffer[IAVF_AQ_BUF_SZ];

	/* State flag used in init/stop */
	u32			queues_enabled;
	u8			enable_queues_chan;
	u8			disable_queues_chan;

	/* For virtchnl message processing task */
	struct task		vc_task;
	struct taskqueue	*vc_tq;
	char			vc_mtx_name[16];
	struct mtx		vc_mtx;
};

/* Function prototypes */
void		 iavf_init_tx_ring(struct iavf_vsi *vsi, struct iavf_tx_queue *que);
void		 iavf_get_default_rss_key(u32 *);
const char *	iavf_vc_stat_str(struct iavf_hw *hw,
    enum virtchnl_status_code stat_err);
void		iavf_init_tx_rsqs(struct iavf_vsi *vsi);
void		iavf_init_tx_cidx(struct iavf_vsi *vsi);
u64		iavf_max_vc_speed_to_value(u8 link_speeds);
void		iavf_add_vsi_sysctls(device_t dev, struct iavf_vsi *vsi,
		    struct sysctl_ctx_list *ctx, const char *sysctl_name);
void		iavf_add_sysctls_eth_stats(struct sysctl_ctx_list *ctx,
		    struct sysctl_oid_list *child,
		    struct iavf_eth_stats *eth_stats);
void		iavf_add_queues_sysctls(device_t dev, struct iavf_vsi *vsi);

void	iavf_enable_intr(struct iavf_vsi *);
void	iavf_disable_intr(struct iavf_vsi *);
#endif /* _IAVF_IFLIB_H_ */
