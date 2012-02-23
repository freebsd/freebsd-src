/*-
 * Copyright (c) 2010-2011 Solarflare Communications, Inc.
 * All rights reserved.
 *
 * This software was developed in part by Philip Paeps under contract for
 * Solarflare Communications, Inc.
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
 *
 * $FreeBSD$
 */

#ifndef _SFXGE_H
#define _SFXGE_H

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/condvar.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/sx.h>
#include <vm/uma.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_media.h>
#include <net/if_types.h>

/*
 * Backward-compatibility
 */
#ifndef CACHE_LINE_SIZE
/* This should be right on most machines the driver will be used on, and
 * we needn't care too much about wasting a few KB per interface.
 */
#define CACHE_LINE_SIZE 128
#endif
#ifndef IFCAP_LINKSTATE
#define IFCAP_LINKSTATE 0
#endif
#ifndef IFCAP_VLAN_HWTSO
#define IFCAP_VLAN_HWTSO 0
#endif
#ifndef IFM_10G_T
#define IFM_10G_T IFM_UNKNOWN
#endif
#ifndef IFM_10G_KX4
#define IFM_10G_KX4 IFM_10G_CX4
#endif
#if __FreeBSD_version >= 800054
/* Networking core is multiqueue aware. We can manage our own TX
 * queues and use m_pkthdr.flowid.
 */
#define SFXGE_HAVE_MQ
#endif
#if (__FreeBSD_version >= 800501 && __FreeBSD_version < 900000) || \
	__FreeBSD_version >= 900003
#define SFXGE_HAVE_DESCRIBE_INTR
#endif
#ifdef IFM_ETH_RXPAUSE
#define SFXGE_HAVE_PAUSE_MEDIAOPTS
#endif
#ifndef CTLTYPE_U64
#define CTLTYPE_U64 CTLTYPE_QUAD
#endif

#include "sfxge_rx.h"
#include "sfxge_tx.h"

#define SFXGE_IP_ALIGN 2

#define SFXGE_ETHERTYPE_LOOPBACK        0x9000  /* Xerox loopback */

enum sfxge_evq_state {
	SFXGE_EVQ_UNINITIALIZED = 0,
	SFXGE_EVQ_INITIALIZED,
	SFXGE_EVQ_STARTING,
	SFXGE_EVQ_STARTED
};

#define	SFXGE_EV_BATCH	16384

struct sfxge_evq {
	struct sfxge_softc	*sc  __aligned(CACHE_LINE_SIZE);
	struct mtx		lock __aligned(CACHE_LINE_SIZE);

	enum sfxge_evq_state	init_state;
	unsigned int		index;
	efsys_mem_t		mem;
	unsigned int		buf_base_id;

	boolean_t		exception;

	efx_evq_t		*common;
	unsigned int		read_ptr;
	unsigned int		rx_done;
	unsigned int		tx_done;

	/* Linked list of TX queues with completions to process */
	struct sfxge_txq	*txq;
	struct sfxge_txq	**txqs;
};

#define	SFXGE_NEVS	4096
#define	SFXGE_NDESCS	1024
#define	SFXGE_MODERATION	30

enum sfxge_intr_state {
	SFXGE_INTR_UNINITIALIZED = 0,
	SFXGE_INTR_INITIALIZED,
	SFXGE_INTR_TESTING,
	SFXGE_INTR_STARTED
};

struct sfxge_intr_hdl {
	int                eih_rid;
	void               *eih_tag;
	struct resource    *eih_res;
};

struct sfxge_intr {
	enum sfxge_intr_state	state;
	struct resource		*msix_res;
	struct sfxge_intr_hdl	*table;
	int			n_alloc;
	int			type;
	efsys_mem_t		status;
	uint32_t		zero_count;
};

enum sfxge_mcdi_state {
	SFXGE_MCDI_UNINITIALIZED = 0,
	SFXGE_MCDI_INITIALIZED,
	SFXGE_MCDI_BUSY,
	SFXGE_MCDI_COMPLETED
};

struct sfxge_mcdi {
	struct mtx		lock;
	struct cv		cv;
	enum sfxge_mcdi_state	state;
	efx_mcdi_transport_t	transport;
};

struct sfxge_hw_stats {
	clock_t			update_time;
	efsys_mem_t		dma_buf;
	void			*decode_buf;
};

enum sfxge_port_state {
	SFXGE_PORT_UNINITIALIZED = 0,
	SFXGE_PORT_INITIALIZED,
	SFXGE_PORT_STARTED
};

struct sfxge_port {
	struct sfxge_softc	*sc;
	struct mtx		lock;
	enum sfxge_port_state	init_state;
#ifndef SFXGE_HAVE_PAUSE_MEDIAOPTS
	unsigned int		wanted_fc;
#endif
	struct sfxge_hw_stats	phy_stats;
	struct sfxge_hw_stats	mac_stats;
	efx_link_mode_t		link_mode;
};

enum sfxge_softc_state {
	SFXGE_UNINITIALIZED = 0,
	SFXGE_INITIALIZED,
	SFXGE_REGISTERED,
	SFXGE_STARTED
};

struct sfxge_softc {
	device_t			dev;
	struct sx			softc_lock;
	enum sfxge_softc_state		init_state;
	struct ifnet                    *ifnet;
	unsigned int			if_flags;
	struct sysctl_oid		*stats_node;

	struct task			task_reset;

	efx_family_t			family;
	caddr_t				vpd_data;
	size_t				vpd_size;
	efx_nic_t			*enp;
	struct mtx			enp_lock;

	bus_dma_tag_t                   parent_dma_tag;
	efsys_bar_t			bar;

	struct sfxge_intr		intr;
	struct sfxge_mcdi		mcdi;
	struct sfxge_port		port;
	uint32_t			buffer_table_next;

	struct sfxge_evq		*evq[SFXGE_RX_SCALE_MAX];
	unsigned int			ev_moderation;
	clock_t				ev_stats_update_time;
	uint64_t			ev_stats[EV_NQSTATS];

	uma_zone_t			rxq_cache;
	struct sfxge_rxq		*rxq[SFXGE_RX_SCALE_MAX];
	unsigned int			rx_indir_table[SFXGE_RX_SCALE_MAX];

#ifdef SFXGE_HAVE_MQ
	struct sfxge_txq		*txq[SFXGE_TXQ_NTYPES + SFXGE_RX_SCALE_MAX];
#else
	struct sfxge_txq		*txq[SFXGE_TXQ_NTYPES];
#endif

	struct ifmedia			media;

	size_t				rx_prefix_size;
	size_t				rx_buffer_size;
	uma_zone_t			rx_buffer_zone;

#ifndef SFXGE_HAVE_MQ
	struct mtx			tx_lock __aligned(CACHE_LINE_SIZE);
#endif
};

#define SFXGE_LINK_UP(sc) ((sc)->port.link_mode != EFX_LINK_DOWN)
#define SFXGE_RUNNING(sc) ((sc)->ifnet->if_drv_flags & IFF_DRV_RUNNING)

/*
 * From sfxge.c.
 */
extern void sfxge_schedule_reset(struct sfxge_softc *sc);
extern void sfxge_sram_buf_tbl_alloc(struct sfxge_softc *sc, size_t n,
				     uint32_t *idp);

/*
 * From sfxge_dma.c.
 */
extern int sfxge_dma_init(struct sfxge_softc *sc);
extern void sfxge_dma_fini(struct sfxge_softc *sc);
extern int sfxge_dma_alloc(struct sfxge_softc *sc, bus_size_t len,
    efsys_mem_t *esmp);
extern void sfxge_dma_free(efsys_mem_t *esmp);
extern int sfxge_dma_map_sg_collapse(bus_dma_tag_t tag, bus_dmamap_t map,
    struct mbuf **mp, bus_dma_segment_t *segs, int *nsegs, int maxsegs);

/*
 * From sfxge_ev.c.
 */
extern int sfxge_ev_init(struct sfxge_softc *sc);
extern void sfxge_ev_fini(struct sfxge_softc *sc);
extern int sfxge_ev_start(struct sfxge_softc *sc);
extern void sfxge_ev_stop(struct sfxge_softc *sc);
extern int sfxge_ev_qpoll(struct sfxge_softc *sc, unsigned int index);

/*
 * From sfxge_intr.c.
 */
extern int sfxge_intr_init(struct sfxge_softc *sc);
extern void sfxge_intr_fini(struct sfxge_softc *sc);
extern int sfxge_intr_start(struct sfxge_softc *sc);
extern void sfxge_intr_stop(struct sfxge_softc *sc);

/*
 * From sfxge_mcdi.c.
 */
extern int sfxge_mcdi_init(struct sfxge_softc *sc);
extern void sfxge_mcdi_fini(struct sfxge_softc *sc);

/*
 * From sfxge_port.c.
 */
extern int sfxge_port_init(struct sfxge_softc *sc);
extern void sfxge_port_fini(struct sfxge_softc *sc);
extern int sfxge_port_start(struct sfxge_softc *sc);
extern void sfxge_port_stop(struct sfxge_softc *sc);
extern void sfxge_mac_link_update(struct sfxge_softc *sc,
    efx_link_mode_t mode);
extern int sfxge_mac_filter_set(struct sfxge_softc *sc);
extern int sfxge_port_ifmedia_init(struct sfxge_softc *sc);

#define SFXGE_MAX_MTU (9 * 1024)

#endif /* _SFXGE_H */
