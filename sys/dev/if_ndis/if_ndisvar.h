/*-
 * Copyright (c) 2003
 *	Bill Paul <wpaul@windriver.com>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#define NDIS_DEFAULT_NODENAME	"FreeBSD NDIS node"
#define NDIS_NODENAME_LEN	32

struct ndis_pci_type {
	uint16_t		ndis_vid;
	uint16_t		ndis_did;
	uint32_t		ndis_subsys;
	char			*ndis_name;
};

struct ndis_pccard_type {
	const char		*ndis_vid;
	const char		*ndis_did;
	char			*ndis_name;
};

struct ndis_shmem {
	bus_dma_tag_t		ndis_stag;
	bus_dmamap_t		ndis_smap;
	void			*ndis_saddr;
	struct ndis_shmem	*ndis_next;
};

struct ndis_cfglist {
	ndis_cfg		ndis_cfg;
        TAILQ_ENTRY(ndis_cfglist)	link;
};

TAILQ_HEAD(nch, ndis_cfglist);

#define NDIS_INITIALIZED(sc)	(sc->ndis_block.nmb_miniportadapterctx != NULL)

#define NDIS_INC(x)		\
	(x)->ndis_txidx = ((x)->ndis_txidx + 1) % (x)->ndis_maxpkts 

#define arpcom ic.ic_ac

struct ndis_softc {
	struct ieee80211com	ic;		/* interface info */
#ifdef notdef
	struct ieee80211com	arpcom;		/* interface info */
#endif
	struct ifmedia		ifmedia;	/* media info */
	u_long			ndis_hwassist;
	uint32_t		ndis_v4tx;
	uint32_t		ndis_v4rx;
	bus_space_handle_t	ndis_bhandle;
	bus_space_tag_t		ndis_btag;
	void			*ndis_intrhand;
	struct resource		*ndis_irq;
	struct resource		*ndis_res;
	struct resource		*ndis_res_io;
	int			ndis_io_rid;
	struct resource		*ndis_res_mem;
	int			ndis_mem_rid;
	struct resource		*ndis_res_altmem;
	int			ndis_altmem_rid;
	struct resource		*ndis_res_am;	/* attribute mem (pccard) */
	int			ndis_am_rid;
	struct resource		*ndis_res_cm;	/* common mem (pccard) */
	struct resource_list	ndis_rl;
	int			ndis_rescnt;
	struct mtx		ndis_mtx;
	struct mtx		ndis_intrmtx;
	device_t		ndis_dev;
	int			ndis_unit;
	ndis_miniport_block	ndis_block;
	ndis_miniport_characteristics	ndis_chars;
	interface_type		ndis_type;
	struct callout_handle	ndis_stat_ch;
	int			ndis_maxpkts;
	ndis_oid		*ndis_oids;
	int			ndis_oidcnt;
	int			ndis_txidx;
	int			ndis_txpending;
	ndis_packet		**ndis_txarray;
	int			ndis_sc;
	ndis_cfg		*ndis_regvals;
	struct nch		ndis_cfglist_head;
	int			ndis_80211;
	int			ndis_link;
	uint32_t		ndis_filter;
	int			ndis_if_flags;
	int			ndis_skip;

#if __FreeBSD_version < 502113
	struct sysctl_ctx_list	ndis_ctx;
	struct sysctl_oid	*ndis_tree;
#endif
	int			ndis_devidx;
	interface_type		ndis_iftype;

	bus_dma_tag_t		ndis_parent_tag;
	struct ndis_shmem	*ndis_shlist;
	bus_dma_tag_t		ndis_mtag;
	bus_dma_tag_t		ndis_ttag;
	bus_dmamap_t		*ndis_mmaps;
	bus_dmamap_t		*ndis_tmaps;
	int			ndis_mmapcnt;
};

#define NDIS_LOCK(_sc)		mtx_lock(&(_sc)->ndis_mtx)
#define NDIS_UNLOCK(_sc)	mtx_unlock(&(_sc)->ndis_mtx)
