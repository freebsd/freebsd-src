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

#include <dev/hyperv/netvsc/ndis.h>

#define HN_USE_TXDESC_BUFRING

MALLOC_DECLARE(M_NETVSC);

/*
 * The following arguably belongs in a separate header file
 */

/*
 * Defines
 */

#define NETVSC_SEND_BUFFER_SIZE			(1024*1024*15)   /* 15M */

#define NETVSC_RECEIVE_BUFFER_SIZE_LEGACY	(1024*1024*15) /* 15MB */
#define NETVSC_RECEIVE_BUFFER_SIZE		(1024*1024*16) /* 16MB */

/*
 * Maximum MTU we permit to be configured for a netvsc interface.
 * When the code was developed, a max MTU of 12232 was tested and
 * proven to work.  9K is a reasonable maximum for an Ethernet.
 */
#define NETVSC_MAX_CONFIGURABLE_MTU		(9 * 1024)

#define NETVSC_PACKET_SIZE			PAGE_SIZE

/*
 * Data types
 */

struct vmbus_channel;

#define NETVSC_DEVICE_RING_BUFFER_SIZE	(128 * PAGE_SIZE)
#define NETVSC_PACKET_MAXPAGE		32

typedef struct {
	uint8_t		mac_addr[ETHER_ADDR_LEN];
	uint32_t	link_state;
} netvsc_device_info;

#define HN_XACT_REQ_PGCNT		2
#define HN_XACT_RESP_PGCNT		2
#define HN_XACT_REQ_SIZE		(HN_XACT_REQ_PGCNT * PAGE_SIZE)
#define HN_XACT_RESP_SIZE		(HN_XACT_RESP_PGCNT * PAGE_SIZE)

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

	uint32_t		hn_rndis_rid;
	uint32_t		hn_ndis_ver;

	struct ndis_rssprm_toeplitz hn_rss;
} hn_softc_t;

#define HN_FLAG_RXBUF_CONNECTED		0x0001
#define HN_FLAG_CHIM_CONNECTED		0x0002

/*
 * Externs
 */
extern int hv_promisc_mode;
struct hn_send_ctx;

void netvsc_linkstatus_callback(struct hn_softc *sc, uint32_t status);
int hn_nvs_attach(struct hn_softc *sc, int mtu);
int hv_nv_on_device_remove(struct hn_softc *sc);
int hv_nv_on_send(struct vmbus_channel *chan, uint32_t rndis_mtype,
	struct hn_send_ctx *sndc, struct vmbus_gpa *gpa, int gpa_cnt);

#endif  /* __HV_NET_VSC_H__ */

