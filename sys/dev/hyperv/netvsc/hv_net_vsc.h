/*-
 * Copyright (c) 2009-2012,2016 Microsoft Corp.
 * Copyright (c) 2010-2012 Citrix Inc.
 * Copyright (c) 2012 NetApp Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * HyperV vmbus (virtual machine bus) network VSC (virtual services client)
 * header file
 *
 * (Updated from unencumbered NvspProtocol.h)
 */

#ifndef __HV_NET_VSC_H__
#define __HV_NET_VSC_H__

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/taskqueue.h>
#include <sys/sema.h>
#include <sys/sx.h>

#include <machine/bus.h>
#include <sys/bus.h>
#include <sys/bus_dma.h>

#include <netinet/in.h>
#include <netinet/tcp_lro.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_media.h>

#include <dev/hyperv/include/hyperv.h>
#include <dev/hyperv/include/hyperv_busdma.h>
#include <dev/hyperv/include/vmbus.h>

#define HN_USE_TXDESC_BUFRING

MALLOC_DECLARE(M_NETVSC);

#define NVSP_INVALID_PROTOCOL_VERSION           (0xFFFFFFFF)

#define NVSP_PROTOCOL_VERSION_1                 2
#define NVSP_PROTOCOL_VERSION_2                 0x30002
#define NVSP_PROTOCOL_VERSION_4                 0x40000
#define NVSP_PROTOCOL_VERSION_5                 0x50000
#define NVSP_MIN_PROTOCOL_VERSION               (NVSP_PROTOCOL_VERSION_1)
#define NVSP_MAX_PROTOCOL_VERSION               (NVSP_PROTOCOL_VERSION_2)

#define NVSP_PROTOCOL_VERSION_CURRENT           NVSP_PROTOCOL_VERSION_2

#define VERSION_4_OFFLOAD_SIZE                  22

#define NVSP_OPERATIONAL_STATUS_OK              (0x00000000)
#define NVSP_OPERATIONAL_STATUS_DEGRADED        (0x00000001)
#define NVSP_OPERATIONAL_STATUS_NONRECOVERABLE  (0x00000002)
#define NVSP_OPERATIONAL_STATUS_NO_CONTACT      (0x00000003)
#define NVSP_OPERATIONAL_STATUS_LOST_COMMUNICATION (0x00000004)

/*
 * Maximun number of transfer pages (packets) the VSP will use on a receive
 */
#define NVSP_MAX_PACKETS_PER_RECEIVE            375

/* vRSS stuff */
#define RNDIS_OBJECT_TYPE_RSS_CAPABILITIES      0x88
#define RNDIS_OBJECT_TYPE_RSS_PARAMETERS        0x89

#define RNDIS_RECEIVE_SCALE_CAPABILITIES_REVISION_2     2
#define RNDIS_RECEIVE_SCALE_PARAMETERS_REVISION_2       2

struct rndis_obj_header {
        uint8_t type;
        uint8_t rev;
        uint16_t size;
} __packed;

/* rndis_recv_scale_cap/cap_flag */
#define RNDIS_RSS_CAPS_MESSAGE_SIGNALED_INTERRUPTS      0x01000000
#define RNDIS_RSS_CAPS_CLASSIFICATION_AT_ISR            0x02000000
#define RNDIS_RSS_CAPS_CLASSIFICATION_AT_DPC            0x04000000
#define RNDIS_RSS_CAPS_USING_MSI_X                      0x08000000
#define RNDIS_RSS_CAPS_RSS_AVAILABLE_ON_PORTS           0x10000000
#define RNDIS_RSS_CAPS_SUPPORTS_MSI_X                   0x20000000
#define RNDIS_RSS_CAPS_HASH_TYPE_TCP_IPV4               0x00000100
#define RNDIS_RSS_CAPS_HASH_TYPE_TCP_IPV6               0x00000200
#define RNDIS_RSS_CAPS_HASH_TYPE_TCP_IPV6_EX            0x00000400

/* RNDIS_RECEIVE_SCALE_CAPABILITIES */
struct rndis_recv_scale_cap {
        struct rndis_obj_header hdr;
        uint32_t cap_flag;
        uint32_t num_int_msg;
        uint32_t num_recv_que;
        uint16_t num_indirect_tabent;
} __packed;

/* rndis_recv_scale_param flags */
#define RNDIS_RSS_PARAM_FLAG_BASE_CPU_UNCHANGED         0x0001
#define RNDIS_RSS_PARAM_FLAG_HASH_INFO_UNCHANGED        0x0002
#define RNDIS_RSS_PARAM_FLAG_ITABLE_UNCHANGED           0x0004
#define RNDIS_RSS_PARAM_FLAG_HASH_KEY_UNCHANGED         0x0008
#define RNDIS_RSS_PARAM_FLAG_DISABLE_RSS                0x0010

/* Hash info bits */
#define RNDIS_HASH_FUNC_TOEPLITZ                0x00000001
#define RNDIS_HASH_IPV4                         0x00000100
#define RNDIS_HASH_TCP_IPV4                     0x00000200
#define RNDIS_HASH_IPV6                         0x00000400
#define RNDIS_HASH_IPV6_EX                      0x00000800
#define RNDIS_HASH_TCP_IPV6                     0x00001000
#define RNDIS_HASH_TCP_IPV6_EX                  0x00002000

#define RNDIS_RSS_INDIRECTION_TABLE_MAX_SIZE_REVISION_2 (128 * 4)
#define RNDIS_RSS_HASH_SECRET_KEY_MAX_SIZE_REVISION_2   40

#define ITAB_NUM                                        128
#define HASH_KEYLEN RNDIS_RSS_HASH_SECRET_KEY_MAX_SIZE_REVISION_2

/* RNDIS_RECEIVE_SCALE_PARAMETERS */
typedef struct rndis_recv_scale_param_ {
        struct rndis_obj_header hdr;

        /* Qualifies the rest of the information */
        uint16_t flag;

        /* The base CPU number to do receive processing. not used */
        uint16_t base_cpu_number;

        /* This describes the hash function and type being enabled */
        uint32_t hashinfo;

        /* The size of indirection table array */
        uint16_t indirect_tabsize;

        /* The offset of the indirection table from the beginning of this
         * structure
         */
        uint32_t indirect_taboffset;

        /* The size of the hash secret key */
        uint16_t hashkey_size;

        /* The offset of the secret key from the beginning of this structure */
        uint32_t hashkey_offset;

        uint32_t processor_masks_offset;
        uint32_t num_processor_masks;
        uint32_t processor_masks_entry_size;
} rndis_recv_scale_param;

/*
 * The following arguably belongs in a separate header file
 */

/*
 * Defines
 */

#define NETVSC_SEND_BUFFER_SIZE			(1024*1024*15)   /* 15M */
#define NETVSC_SEND_BUFFER_ID			0xface

#define NETVSC_RECEIVE_BUFFER_SIZE_LEGACY	(1024*1024*15) /* 15MB */
#define NETVSC_RECEIVE_BUFFER_SIZE		(1024*1024*16) /* 16MB */

#define NETVSC_RECEIVE_BUFFER_ID		0xcafe

#define NETVSC_RECEIVE_SG_COUNT			1

/* Preallocated receive packets */
#define NETVSC_RECEIVE_PACKETLIST_COUNT		256

/*
 * Maximum MTU we permit to be configured for a netvsc interface.
 * When the code was developed, a max MTU of 12232 was tested and
 * proven to work.  9K is a reasonable maximum for an Ethernet.
 */
#define NETVSC_MAX_CONFIGURABLE_MTU		(9 * 1024)

#define NETVSC_PACKET_SIZE			PAGE_SIZE
#define VRSS_SEND_TABLE_SIZE			16

/*
 * Data types
 */

struct vmbus_channel;

typedef void (*pfn_on_send_rx_completion)(struct vmbus_channel *, void *);

#define NETVSC_DEVICE_RING_BUFFER_SIZE	(128 * PAGE_SIZE)
#define NETVSC_PACKET_MAXPAGE		32

#define NETVSC_VLAN_PRIO_MASK		0xe000
#define NETVSC_VLAN_PRIO_SHIFT		13
#define NETVSC_VLAN_VID_MASK		0x0fff

#define TYPE_IPV4			2
#define TYPE_IPV6			4
#define TYPE_TCP			2
#define TYPE_UDP			4

#define TRANSPORT_TYPE_NOT_IP		0
#define TRANSPORT_TYPE_IPV4_TCP		((TYPE_IPV4 << 16) | TYPE_TCP)
#define TRANSPORT_TYPE_IPV4_UDP		((TYPE_IPV4 << 16) | TYPE_UDP)
#define TRANSPORT_TYPE_IPV6_TCP		((TYPE_IPV6 << 16) | TYPE_TCP)
#define TRANSPORT_TYPE_IPV6_UDP		((TYPE_IPV6 << 16) | TYPE_UDP)

typedef struct {
	uint8_t		mac_addr[6];  /* Assumption unsigned long */
	uint8_t		link_state;
} netvsc_device_info;

#ifndef HN_USE_TXDESC_BUFRING
struct hn_txdesc;
SLIST_HEAD(hn_txdesc_list, hn_txdesc);
#else
struct buf_ring;
#endif

struct hn_tx_ring;

struct hn_rx_ring {
	struct ifnet	*hn_ifp;
	struct hn_tx_ring *hn_txr;
	void		*hn_rdbuf;
	uint8_t		*hn_rxbuf;	/* shadow sc->hn_rxbuf */
	int		hn_rx_idx;

	/* Trust csum verification on host side */
	int		hn_trust_hcsum;	/* HN_TRUST_HCSUM_ */
	struct lro_ctrl	hn_lro;

	u_long		hn_csum_ip;
	u_long		hn_csum_tcp;
	u_long		hn_csum_udp;
	u_long		hn_csum_trusted;
	u_long		hn_lro_tried;
	u_long		hn_small_pkts;
	u_long		hn_pkts;
	u_long		hn_rss_pkts;

	/* Rarely used stuffs */
	struct sysctl_oid *hn_rx_sysctl_tree;
	int		hn_rx_flags;
} __aligned(CACHE_LINE_SIZE);

#define HN_TRUST_HCSUM_IP	0x0001
#define HN_TRUST_HCSUM_TCP	0x0002
#define HN_TRUST_HCSUM_UDP	0x0004

#define HN_RX_FLAG_ATTACHED	0x1

struct hn_tx_ring {
#ifndef HN_USE_TXDESC_BUFRING
	struct mtx	hn_txlist_spin;
	struct hn_txdesc_list hn_txlist;
#else
	struct buf_ring	*hn_txdesc_br;
#endif
	int		hn_txdesc_cnt;
	int		hn_txdesc_avail;
	u_short		hn_has_txeof;
	u_short		hn_txdone_cnt;

	int		hn_sched_tx;
	void		(*hn_txeof)(struct hn_tx_ring *);
	struct taskqueue *hn_tx_taskq;
	struct task	hn_tx_task;
	struct task	hn_txeof_task;

	struct buf_ring	*hn_mbuf_br;
	int		hn_oactive;
	int		hn_tx_idx;

	struct mtx	hn_tx_lock;
	struct hn_softc	*hn_sc;
	struct vmbus_channel *hn_chan;

	int		hn_direct_tx_size;
	int		hn_chim_size;
	bus_dma_tag_t	hn_tx_data_dtag;
	uint64_t	hn_csum_assist;

	int		hn_gpa_cnt;
	struct vmbus_gpa hn_gpa[NETVSC_PACKET_MAXPAGE];

	u_long		hn_no_txdescs;
	u_long		hn_send_failed;
	u_long		hn_txdma_failed;
	u_long		hn_tx_collapsed;
	u_long		hn_tx_chimney_tried;
	u_long		hn_tx_chimney;
	u_long		hn_pkts;

	/* Rarely used stuffs */
	struct hn_txdesc *hn_txdesc;
	bus_dma_tag_t	hn_tx_rndis_dtag;
	struct sysctl_oid *hn_tx_sysctl_tree;
	int		hn_tx_flags;
} __aligned(CACHE_LINE_SIZE);

#define HN_TX_FLAG_ATTACHED	0x1

/*
 * Device-specific softc structure
 */
typedef struct hn_softc {
	struct ifnet    *hn_ifp;
	struct ifmedia	hn_media;
	device_t        hn_dev;
	uint8_t         hn_unit;
	int             hn_carrier;
	int             hn_if_flags;
	struct mtx      hn_lock;
	int             hn_initdone;
	/* See hv_netvsc_drv_freebsd.c for rules on how to use */
	int             temp_unusable;
	struct rndis_device_ *rndis_dev;
	struct vmbus_channel *hn_prichan;

	int		hn_rx_ring_cnt;
	int		hn_rx_ring_inuse;
	struct hn_rx_ring *hn_rx_ring;

	int		hn_tx_ring_cnt;
	int		hn_tx_ring_inuse;
	struct hn_tx_ring *hn_tx_ring;

	uint8_t		*hn_chim;
	u_long		*hn_chim_bmap;
	int		hn_chim_bmap_cnt;
	int		hn_chim_cnt;
	int		hn_chim_szmax;

	int		hn_cpu;
	struct taskqueue *hn_tx_taskq;
	struct sysctl_oid *hn_tx_sysctl_tree;
	struct sysctl_oid *hn_rx_sysctl_tree;
	struct vmbus_xact_ctx *hn_xact;
	uint32_t	hn_nvs_ver;

	uint32_t		hn_flags;
	void			*hn_rxbuf;
	uint32_t		hn_rxbuf_gpadl;
	struct hyperv_dma	hn_rxbuf_dma;

	uint32_t		hn_chim_gpadl;
	struct hyperv_dma	hn_chim_dma;
} hn_softc_t;

#define HN_FLAG_RXBUF_CONNECTED		0x0001
#define HN_FLAG_CHIM_CONNECTED		0x0002

/*
 * Externs
 */
extern int hv_promisc_mode;
struct hn_send_ctx;

void netvsc_linkstatus_callback(struct hn_softc *sc, uint32_t status);
int hv_nv_on_device_add(struct hn_softc *sc, struct hn_rx_ring *rxr);
int hv_nv_on_device_remove(struct hn_softc *sc,
    boolean_t destroy_channel);
int hv_nv_on_send(struct vmbus_channel *chan, uint32_t rndis_mtype,
	struct hn_send_ctx *sndc, struct vmbus_gpa *gpa, int gpa_cnt);
void hv_nv_subchan_attach(struct vmbus_channel *chan,
    struct hn_rx_ring *rxr);

#endif  /* __HV_NET_VSC_H__ */

