/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2008-2017 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 */

#ifndef _ENIC_H
#define _ENIC_H

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>

#include <machine/bus.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/iflib.h>

#define u8  uint8_t
#define u16 uint16_t
#define u32 uint32_t
#define u64 uint64_t

struct enic_bar_info {
	struct resource		*res;
	bus_space_tag_t		tag;
	bus_space_handle_t	handle;
	bus_size_t		size;
	int			rid;
	int			offset;
};

#define ENIC_BUS_WRITE_8(res, index, value) \
    bus_space_write_8(res->bar.tag, res->bar.handle, \
    res->bar.offset + (index), value)
#define ENIC_BUS_WRITE_4(res, index, value) \
    bus_space_write_4(res->bar.tag, res->bar.handle, \
    res->bar.offset + (index), value)
#define ENIC_BUS_WRITE_REGION_4(res, index, values, count) \
    bus_space_write_region_4(res->bar.tag, res->bar.handle, \
    res->bar.offset + (index), values, count);

#define ENIC_BUS_READ_8(res, index) \
    bus_space_read_8(res->bar.tag, res->bar.handle, \
    res->bar.offset + (index))
#define ENIC_BUS_READ_4(res, index) \
    bus_space_read_4(res->bar.tag, res->bar.handle, \
    res->bar.offset + (index))
#define ENIC_BUS_READ_REGION_4(res, type, index, values, count) \
    bus_space_read_region_4(res->type.tag, res->type.handle, \
    res->type.offset + (index), values, count);

struct vnic_res {
	unsigned int count;
	struct enic_bar_info bar;
};

#include "vnic_enet.h"
#include "vnic_dev.h"
#include "vnic_wq.h"
#include "vnic_rq.h"
#include "vnic_cq.h"
#include "vnic_intr.h"
#include "vnic_stats.h"
#include "vnic_nic.h"
#include "vnic_rss.h"
#include "enic_res.h"
#include "cq_enet_desc.h"

#define ENIC_LOCK(_softc)	mtx_lock(&(_softc)->enic_lock)
#define ENIC_UNLOCK(_softc)	mtx_unlock(&(_softc)->enic_lock)

#define DRV_NAME		"enic"
#define DRV_DESCRIPTION		"Cisco VIC Ethernet NIC"
#define DRV_COPYRIGHT		"Copyright 2008-2015 Cisco Systems, Inc"

#define ENIC_MAX_MAC_ADDR	64

#define VLAN_ETH_HLEN           18

#define ENICPMD_SETTING(enic, f) ((enic->config.flags & VENETF_##f) ? 1 : 0)

#define ENICPMD_BDF_LENGTH		13   /* 0000:00:00.0'\0' */
#define ENIC_CALC_IP_CKSUM		1
#define ENIC_CALC_TCP_UDP_CKSUM		2
#define ENIC_MAX_MTU			9000
#define ENIC_PAGE_SIZE			4096
#define PAGE_ROUND_UP(x) \
	((((unsigned long)(x)) + ENIC_PAGE_SIZE-1) & (~(ENIC_PAGE_SIZE-1)))

/* must be >= VNIC_COUNTER_DMA_MIN_PERIOD */
#define VNIC_FLOW_COUNTER_UPDATE_MSECS 500

/* PCI IDs */
#define CISCO_VENDOR_ID	0x1137

#define PCI_DEVICE_ID_CISCO_VIC_ENET	0x0043  /* ethernet vnic */
#define PCI_DEVICE_ID_CISCO_VIC_ENET_VF	0x0071  /* enet SRIOV VF */

/* Special Filter id for non-specific packet flagging. Don't change value */
#define ENIC_MAGIC_FILTER_ID 0xffff

#define ENICPMD_FDIR_MAX		64

/* HW default VXLAN port */
#define ENIC_DEFAULT_VXLAN_PORT		4789

/*
 * Interrupt 0: LSC and errors
 * Interrupt 1: rx queue 0
 * Interrupt 2: rx queue 1
 * ...
 */
#define ENICPMD_LSC_INTR_OFFSET 0
#define ENICPMD_RXQ_INTR_OFFSET 1

#include "vnic_devcmd.h"

enum vnic_proxy_type {
	PROXY_NONE,
	PROXY_BY_BDF,
	PROXY_BY_INDEX,
};

struct vnic_intr_coal_timer_info {
	u32 mul;
	u32 div;
	u32 max_usec;
};

struct enic_softc;
struct vnic_dev {
	void *priv;
	struct rte_pci_device *pdev;
	struct vnic_res res[RES_TYPE_MAX];
	enum vnic_dev_intr_mode intr_mode;
	struct vnic_res __iomem *devcmd;
	struct vnic_devcmd_notify *notify;
	struct vnic_devcmd_notify notify_copy;
	bus_addr_t notify_pa;
	struct iflib_dma_info notify_res;
	u32 notify_sz;
	struct iflib_dma_info linkstatus_res;
	struct vnic_stats *stats;
	struct iflib_dma_info stats_res;
	struct vnic_devcmd_fw_info *fw_info;
	struct iflib_dma_info fw_info_res;
	enum vnic_proxy_type proxy;
	u32 proxy_index;
	u64 args[VNIC_DEVCMD_NARGS];
	int in_reset;
	struct vnic_intr_coal_timer_info intr_coal_timer_info;
	void *(*alloc_consistent)(void *priv, size_t size,
	    bus_addr_t *dma_handle, struct iflib_dma_info *res, u8 *name);
	void (*free_consistent)(void *priv, size_t size, void *vaddr,
	    bus_addr_t dma_handle, struct iflib_dma_info *res);
	struct vnic_counter_counts *flow_counters;
	struct iflib_dma_info flow_counters_res;
	u8 flow_counters_dma_active;
	struct enic_softc *softc;
};

struct enic_soft_stats {
	uint64_t rx_nombuf;
	uint64_t rx_packet_errors;
	uint64_t tx_oversized;
};

struct intr_queue {
	struct if_irq intr_irq;
	struct resource *res;
	int rid;
	struct enic_softc *softc;
};

struct enic {
	struct enic *next;
	struct rte_pci_device *pdev;
	struct vnic_enet_config config;
	struct vnic_dev_bar bar0;
	struct vnic_dev *vdev;

	/*
	 * mbuf_initializer contains 64 bits of mbuf rearm_data, used by
	 * the avx2 handler at this time.
	 */
	uint64_t mbuf_initializer;
	unsigned int port_id;
	bool overlay_offload;
	char bdf_name[ENICPMD_BDF_LENGTH];
	int dev_fd;
	int iommu_group_fd;
	int iommu_groupid;
	int eventfd;
	uint8_t mac_addr[ETH_ALEN];
	pthread_t err_intr_thread;
	u8 ig_vlan_strip_en;
	int link_status;
	u8 hw_ip_checksum;
	u16 max_mtu;
	u8 adv_filters;
	u32 flow_filter_mode;
	u8 filter_actions; /* HW supported actions */
	bool vxlan;
	bool disable_overlay; /* devargs disable_overlay=1 */
	uint8_t enable_avx2_rx;  /* devargs enable-avx2-rx=1 */
	bool nic_cfg_chk;     /* NIC_CFG_CHK available */
	bool udp_rss_weak;    /* Bodega style UDP RSS */
	uint8_t ig_vlan_rewrite_mode; /* devargs ig-vlan-rewrite */
	uint16_t vxlan_port;  /* current vxlan port pushed to NIC */

	unsigned int flags;
	unsigned int priv_flags;

	/* work queue (len = conf_wq_count) */
	struct vnic_wq *wq;
	unsigned int wq_count; /* equals eth_dev nb_tx_queues */

	/* receive queue (len = conf_rq_count) */
	struct vnic_rq *rq;
	unsigned int rq_count; /* equals eth_dev nb_rx_queues */

	/* completion queue (len = conf_cq_count) */
	struct vnic_cq *cq;
	unsigned int cq_count; /* equals rq_count + wq_count */

	/* interrupt vectors (len = conf_intr_count) */
	struct vnic_intr *intr;
	struct intr_queue *intr_queues;;
	unsigned int intr_count; /* equals enabled interrupts (lsc + rxqs) */


	/* software counters */
	struct enic_soft_stats soft_stats;

	/* configured resources on vic */
	unsigned int conf_rq_count;
	unsigned int conf_wq_count;
	unsigned int conf_cq_count;
	unsigned int conf_intr_count;

	/* linked list storing memory allocations */
	LIST_HEAD(enic_memzone_list, enic_memzone_entry) memzone_list;

	LIST_HEAD(enic_flows, rte_flow) flows;
	int max_flow_counter;

	/* RSS */
	uint16_t reta_size;
	uint8_t hash_key_size;
	uint64_t flow_type_rss_offloads; /* 0 indicates RSS not supported */
	/*
	 * Keep a copy of current RSS config for queries, as we cannot retrieve
	 * it from the NIC.
	 */
	uint8_t rss_hash_type; /* NIC_CFG_RSS_HASH_TYPE flags */
	uint8_t rss_enable;
	uint64_t rss_hf; /* ETH_RSS flags */
	union vnic_rss_key rss_key;
	union vnic_rss_cpu rss_cpu;

	uint64_t rx_offload_capa; /* DEV_RX_OFFLOAD flags */
	uint64_t tx_offload_capa; /* DEV_TX_OFFLOAD flags */
	uint64_t tx_queue_offload_capa; /* DEV_TX_OFFLOAD flags */
	uint64_t tx_offload_mask; /* PKT_TX flags accepted */
	struct enic_softc *softc;
	int port_mtu;
};

struct enic_softc {
	device_t		dev;
	if_ctx_t		ctx;
	if_softc_ctx_t		scctx;
	if_shared_ctx_t		sctx;
	struct ifmedia		*media;
	if_t			ifp;

	struct mtx		enic_lock;

	struct enic_bar_info	mem;
	struct enic_bar_info	io;

	struct vnic_dev		vdev;
	struct enic		enic;

	int ntxqsets;
	int nrxqsets;

	struct if_irq		enic_event_intr_irq;
	struct if_irq		enic_err_intr_irq;
	uint8_t			lladdr[ETHER_ADDR_LEN];
	int			link_active;
	int			stopped;
	uint8_t			mac_addr[ETHER_ADDR_LEN];

	int			directed;
	int			multicast;
	int			broadcast;
	int			promisc;
	int 			allmulti;

	u_int			mc_count;
	uint8_t			*mta;
};

/* Per-instance private data structure */

static inline unsigned int enic_vnic_rq_count(struct enic *enic)
{
	return enic->rq_count;
}

static inline unsigned int enic_cq_rq(struct enic *enic, unsigned int rq)
{
	return rq;
}

static inline unsigned int enic_cq_wq(struct enic *enic, unsigned int wq)
{
	return enic->rq_count + wq;
}

static inline uint32_t
enic_ring_add(uint32_t n_descriptors, uint32_t i0, uint32_t i1)
{
	uint32_t d = i0 + i1;
	d -= (d >= n_descriptors) ? n_descriptors : 0;
	return d;
}

static inline uint32_t
enic_ring_sub(uint32_t n_descriptors, uint32_t i0, uint32_t i1)
{
	int32_t d = i1 - i0;
	return (uint32_t)((d < 0) ? ((int32_t)n_descriptors + d) : d);
}

static inline uint32_t
enic_ring_incr(uint32_t n_descriptors, uint32_t idx)
{
	idx++;
	if (unlikely(idx == n_descriptors))
		idx = 0;
	return idx;
}

void enic_free_wq(void *txq);
int enic_alloc_intr_resources(struct enic *enic);
int enic_setup_finish(struct enic *enic);
int enic_alloc_wq(struct enic *enic, uint16_t queue_idx,
		  unsigned int socket_id, uint16_t nb_desc);
void enic_start_wq(struct enic *enic, uint16_t queue_idx);
int enic_stop_wq(struct enic *enic, uint16_t queue_idx);
void enic_start_rq(struct enic *enic, uint16_t queue_idx);
void enic_free_rq(void *rxq);
int enic_set_vnic_res(struct enic *enic);
int enic_init_rss_nic_cfg(struct enic *enic);
int enic_set_rss_reta(struct enic *enic, union vnic_rss_cpu *rss_cpu);
int enic_set_vlan_strip(struct enic *enic);
int enic_enable(struct enic *enic);
int enic_disable(struct enic *enic);
void enic_remove(struct enic *enic);
int enic_get_link_status(struct enic *enic);
void enic_dev_stats_clear(struct enic *enic);
void enic_add_packet_filter(struct enic *enic);
int enic_set_mac_address(struct enic *enic, uint8_t *mac_addr);
int enic_del_mac_address(struct enic *enic, int mac_index);
unsigned int enic_cleanup_wq(struct enic *enic, struct vnic_wq *wq);

void enic_post_wq_index(struct vnic_wq *wq);
int enic_probe(struct enic *enic);
int enic_clsf_init(struct enic *enic);
void enic_clsf_destroy(struct enic *enic);
int enic_set_mtu(struct enic *enic, uint16_t new_mtu);
int enic_link_update(struct enic *enic);
bool enic_use_vector_rx_handler(struct enic *enic);
void enic_fdir_info(struct enic *enic);
void enic_prep_wq_for_simple_tx(struct enic *, uint16_t);

struct enic_ring {
	uint64_t		paddr;
	caddr_t			 vaddr;
	struct enic_softc	*softc;
	uint32_t		ring_size; /* Must be a power of two */
	uint16_t		id;	   /* Logical ID */
	uint16_t		phys_id;
};

struct enic_cp_ring {
	struct enic_ring	ring;
	struct if_irq		irq;
	uint32_t		cons;
	bool			v_bit;	  /* Value of valid bit */
	struct ctx_hw_stats	*stats;
	uint32_t		stats_ctx_id;
	uint32_t		last_idx; /* Used by RX rings only
					   * set to the last read pidx
					   */
};

#endif /* _ENIC_H_ */
