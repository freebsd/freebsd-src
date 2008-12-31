/*      $FreeBSD: src/sys/dev/ipw/if_ipwvar.h,v 1.6.2.1.4.1 2008/11/25 02:59:29 kensmith Exp $	*/

/*-
 * Copyright (c) 2004-2006
 *      Damien Bergamini <damien.bergamini@free.fr>. All rights reserved.
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

#define IPW_MAX_NSEG	1

struct ipw_soft_bd {
	struct ipw_bd	*bd;
	int		type;
#define IPW_SBD_TYPE_NOASSOC	0
#define IPW_SBD_TYPE_COMMAND	1
#define IPW_SBD_TYPE_HEADER	2
#define IPW_SBD_TYPE_DATA	3
	void		*priv;
};

struct ipw_soft_hdr {
	struct ipw_hdr			hdr;
	bus_dmamap_t			map;
	SLIST_ENTRY(ipw_soft_hdr)	next;
};

struct ipw_soft_buf {
	struct mbuf			*m;
	struct ieee80211_node		*ni;
	bus_dmamap_t			map;
	SLIST_ENTRY(ipw_soft_buf)	next;
};

struct ipw_rx_radiotap_header {
	struct ieee80211_radiotap_header wr_ihdr;
	uint8_t		wr_flags;
	uint16_t	wr_chan_freq;
	uint16_t	wr_chan_flags;
	uint8_t		wr_antsignal;
};

#define IPW_RX_RADIOTAP_PRESENT						\
	((1 << IEEE80211_RADIOTAP_FLAGS) |				\
	 (1 << IEEE80211_RADIOTAP_CHANNEL) |				\
	 (1 << IEEE80211_RADIOTAP_DB_ANTSIGNAL))

struct ipw_tx_radiotap_header {
	struct ieee80211_radiotap_header wt_ihdr;
	uint8_t		wt_flags;
	uint16_t	wt_chan_freq;
	uint16_t	wt_chan_flags;
};

#define IPW_TX_RADIOTAP_PRESENT						\
	((1 << IEEE80211_RADIOTAP_FLAGS) |				\
	 (1 << IEEE80211_RADIOTAP_CHANNEL))

struct ipw_softc {
	struct ifnet			*sc_ifp;
	struct ieee80211com		sc_ic;
	int				(*sc_newstate)(struct ieee80211com *,
					    enum ieee80211_state, int);
	device_t			sc_dev;

	struct mtx			sc_mtx;
	struct task			sc_init_task;
	struct task			sc_scan_task;
	struct task			sc_chan_task;
	struct task			sc_assoc_task;
	struct task			sc_disassoc_task;
	struct callout			sc_wdtimer;	/* watchdog timer */

	uint32_t			flags;
#define IPW_FLAG_FW_INITED		(1 << 0)
#define IPW_FLAG_INIT_LOCKED		(1 << 1)
#define IPW_FLAG_HAS_RADIO_SWITCH	(1 << 2)
#define	IPW_FLAG_HACK			(1 << 3)
#define	IPW_FLAG_SCANNING		(1 << 4)
#define	IPW_FLAG_ENABLED		(1 << 5)
#define	IPW_FLAG_BUSY			(1 << 6)
#define	IPW_FLAG_ASSOCIATED		(1 << 7)

	int				irq_rid;
	int				mem_rid;
	struct resource			*irq;
	struct resource			*mem;
	bus_space_tag_t			sc_st;
	bus_space_handle_t		sc_sh;
	void 				*sc_ih;
	const struct firmware		*sc_firmware;

	int				sc_tx_timer;
	int				sc_scan_timer;

	bus_dma_tag_t			tbd_dmat;
	bus_dma_tag_t			rbd_dmat;
	bus_dma_tag_t			status_dmat;
	bus_dma_tag_t			cmd_dmat;
	bus_dma_tag_t			hdr_dmat;
	bus_dma_tag_t			txbuf_dmat;
	bus_dma_tag_t			rxbuf_dmat;

	bus_dmamap_t			tbd_map;
	bus_dmamap_t			rbd_map;
	bus_dmamap_t			status_map;
	bus_dmamap_t			cmd_map;

	bus_addr_t			tbd_phys;
	bus_addr_t			rbd_phys;
	bus_addr_t			status_phys;

	struct ipw_bd			*tbd_list;
	struct ipw_bd			*rbd_list;
	struct ipw_status		*status_list;

	struct ipw_cmd			cmd;
	struct ipw_soft_bd		stbd_list[IPW_NTBD];
	struct ipw_soft_buf		tx_sbuf_list[IPW_NDATA];
	struct ipw_soft_hdr		shdr_list[IPW_NDATA];
	struct ipw_soft_bd		srbd_list[IPW_NRBD];
	struct ipw_soft_buf		rx_sbuf_list[IPW_NRBD];

	SLIST_HEAD(, ipw_soft_hdr)	free_shdr;
	SLIST_HEAD(, ipw_soft_buf)	free_sbuf;

	uint32_t			table1_base;
	uint32_t			table2_base;

	uint32_t			txcur;
	uint32_t			txold;
	uint32_t			rxcur;
	int				txfree;

	int				dwelltime;

	struct bpf_if			*sc_drvbpf;

	union {
		struct ipw_rx_radiotap_header th;
		uint8_t	pad[64];
	} sc_rxtapu;
#define sc_rxtap	sc_rxtapu.th
	int				sc_rxtap_len;

	union {
		struct ipw_tx_radiotap_header th;
		uint8_t	pad[64];
	} sc_txtapu;
#define sc_txtap	sc_txtapu.th
	int				sc_txtap_len;
};

/*
 * NB.: This models the only instance of async locking in ipw_init_locked
 *	and must be kept in sync.
 */
#define	IPW_LOCK_DECL	int	__waslocked = 0
#define IPW_LOCK(sc)	do {				\
	if (!(__waslocked = mtx_owned(&(sc)->sc_mtx)))	\
		mtx_lock(&sc->sc_mtx);			\
} while (0)
#define IPW_UNLOCK(sc)	do {				\
	if (!__waslocked)				\
		mtx_unlock(&sc->sc_mtx);		\
} while (0)
#define IPW_LOCK_ASSERT(sc)	mtx_assert(&(sc)->sc_mtx, MA_OWNED)
