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

#ifndef _SFXGE_TX_H
#define _SFXGE_TX_H

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>

/* Maximum number of DMA segments needed to map an mbuf chain.  With
 * TSO, the mbuf length may be just over 64K, divided into 2K mbuf
 * clusters.  (The chain could be longer than this initially, but can
 * be shortened with m_collapse().)
 */
#define	SFXGE_TX_MAPPING_MAX_SEG (64 / 2 + 1)

/* Maximum number of DMA segments needed to map an output packet.  It
 * could overlap all mbufs in the chain and also require an extra
 * segment for a TSO header.
 */
#define SFXGE_TX_PACKET_MAX_SEG (SFXGE_TX_MAPPING_MAX_SEG + 1)

/*
 * Buffer mapping flags.
 *
 * Buffers and DMA mappings must be freed when the last descriptor
 * referring to them is completed.  Set the TX_BUF_UNMAP and
 * TX_BUF_MBUF flags on the last descriptor generated for an mbuf
 * chain.  Set only the TX_BUF_UNMAP flag on a descriptor referring to
 * a heap buffer.
 */
enum sfxge_tx_buf_flags {
	TX_BUF_UNMAP = 1,
	TX_BUF_MBUF = 2,
};

/*
 * Buffer mapping information for descriptors in flight.
 */
struct sfxge_tx_mapping {
	union {
		struct mbuf	*mbuf;
		caddr_t		heap_buf;
	}			u;
	bus_dmamap_t		map;
	enum sfxge_tx_buf_flags	flags;
};

#define	SFXGE_TX_DPL_GET_PKT_LIMIT_DEFAULT	64
#define	SFXGE_TX_DPL_PUT_PKT_LIMIT_DEFAULT	64

/*
 * Deferred packet list.
 */
struct sfxge_tx_dpl {
	uintptr_t		std_put;    /* Head of put list. */
	struct mbuf		*std_get;   /* Head of get list. */
	struct mbuf		**std_getp; /* Tail of get list. */
	unsigned int		std_count;  /* Count of packets. */
};


#define	SFXGE_TX_BUFFER_SIZE	0x400
#define	SFXGE_TX_HEADER_SIZE	0x100
#define	SFXGE_TX_COPY_THRESHOLD	0x200

enum sfxge_txq_state {
	SFXGE_TXQ_UNINITIALIZED = 0,
	SFXGE_TXQ_INITIALIZED,
	SFXGE_TXQ_STARTED
};

enum sfxge_txq_type {
	SFXGE_TXQ_NON_CKSUM = 0,
	SFXGE_TXQ_IP_CKSUM,
	SFXGE_TXQ_IP_TCP_UDP_CKSUM,
	SFXGE_TXQ_NTYPES
};

#define	SFXGE_TXQ_UNBLOCK_LEVEL		(EFX_TXQ_LIMIT(SFXGE_NDESCS) / 4)

#define	SFXGE_TX_BATCH	64

#ifdef SFXGE_HAVE_MQ
#define SFXGE_TXQ_LOCK(txq)		(&(txq)->lock)
#define SFXGE_TX_SCALE(sc)		((sc)->intr.n_alloc)
#else
#define SFXGE_TXQ_LOCK(txq)		(&(txq)->sc->tx_lock)
#define SFXGE_TX_SCALE(sc)		1
#endif

struct sfxge_txq {
	/* The following fields should be written very rarely */
	struct sfxge_softc		*sc;
	enum sfxge_txq_state		init_state;
	enum sfxge_flush_state		flush_state;
	enum sfxge_txq_type		type;
	unsigned int			txq_index;
	unsigned int			evq_index;
	efsys_mem_t			mem;
	unsigned int			buf_base_id;

	struct sfxge_tx_mapping		*stmp;	/* Packets in flight. */
	bus_dma_tag_t			packet_dma_tag;
	efx_buffer_t			*pend_desc;
	efx_txq_t			*common;
	struct sfxge_txq		*next;

	efsys_mem_t			*tsoh_buffer;

	/* This field changes more often and is read regularly on both
	 * the initiation and completion paths
	 */
	int				blocked __aligned(CACHE_LINE_SIZE);

	/* The following fields change more often, and are used mostly
	 * on the initiation path
	 */
#ifdef SFXGE_HAVE_MQ
	struct mtx			lock __aligned(CACHE_LINE_SIZE);
	struct sfxge_tx_dpl		dpl;	/* Deferred packet list. */
	unsigned int			n_pend_desc;
#else
	unsigned int			n_pend_desc __aligned(CACHE_LINE_SIZE);
#endif
	unsigned int			added;
	unsigned int			reaped;
	/* Statistics */
	unsigned long			tso_bursts;
	unsigned long			tso_packets;
	unsigned long			tso_long_headers;
	unsigned long			collapses;
	unsigned long			drops;
	unsigned long			early_drops;

	/* The following fields change more often, and are used mostly
	 * on the completion path
	 */
	unsigned int			pending __aligned(CACHE_LINE_SIZE);
	unsigned int			completed;
};

extern int sfxge_tx_packet_add(struct sfxge_txq *, struct mbuf *);

extern int sfxge_tx_init(struct sfxge_softc *sc);
extern void sfxge_tx_fini(struct sfxge_softc *sc);
extern int sfxge_tx_start(struct sfxge_softc *sc);
extern void sfxge_tx_stop(struct sfxge_softc *sc);
extern void sfxge_tx_qcomplete(struct sfxge_txq *txq);
extern void sfxge_tx_qflush_done(struct sfxge_txq *txq);
#ifdef SFXGE_HAVE_MQ
extern void sfxge_if_qflush(struct ifnet *ifp);
extern int sfxge_if_transmit(struct ifnet *ifp, struct mbuf *m);
#else
extern void sfxge_if_start(struct ifnet *ifp);
#endif

#endif
