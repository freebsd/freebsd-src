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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/limits.h>
#include <net/ethernet.h>
#include <net/if_dl.h>

#include "common/efx.h"

#include "sfxge.h"

static int
sfxge_mac_stat_update(struct sfxge_softc *sc)
{
	struct sfxge_port *port = &sc->port;
	efsys_mem_t *esmp = &(port->mac_stats.dma_buf);
	clock_t now;
	unsigned int count;
	int rc;

	mtx_lock(&port->lock);

	if (port->init_state != SFXGE_PORT_STARTED) {
		rc = 0;
		goto out;
	}

	now = ticks;
	if (now - port->mac_stats.update_time < hz) {
		rc = 0;
		goto out;
	}

	port->mac_stats.update_time = now;

	/* If we're unlucky enough to read statistics wduring the DMA, wait
	 * up to 10ms for it to finish (typically takes <500us) */
	for (count = 0; count < 100; ++count) {
		EFSYS_PROBE1(wait, unsigned int, count);

		/* Synchronize the DMA memory for reading */
		bus_dmamap_sync(esmp->esm_tag, esmp->esm_map,
		    BUS_DMASYNC_POSTREAD);

		/* Try to update the cached counters */
		if ((rc = efx_mac_stats_update(sc->enp, esmp,
		    port->mac_stats.decode_buf, NULL)) != EAGAIN)
			goto out;

		DELAY(100);
	}

	rc = ETIMEDOUT;
out:
	mtx_unlock(&port->lock);
	return (rc);
}

static int
sfxge_mac_stat_handler(SYSCTL_HANDLER_ARGS)
{
	struct sfxge_softc *sc = arg1;
	unsigned int id = arg2;
	int rc;

	if ((rc = sfxge_mac_stat_update(sc)) != 0)
		return (rc);

	return (SYSCTL_OUT(req,
			  (uint64_t *)sc->port.mac_stats.decode_buf + id,
			  sizeof(uint64_t)));
}

static void
sfxge_mac_stat_init(struct sfxge_softc *sc)
{
	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(sc->dev);
	struct sysctl_oid_list *stat_list;
	unsigned int id;
	const char *name;

	stat_list = SYSCTL_CHILDREN(sc->stats_node);

	/* Initialise the named stats */
	for (id = 0; id < EFX_MAC_NSTATS; id++) {
		name = efx_mac_stat_name(sc->enp, id);
		SYSCTL_ADD_PROC(
			ctx, stat_list,
			OID_AUTO, name, CTLTYPE_U64|CTLFLAG_RD,
			sc, id, sfxge_mac_stat_handler, "Q",
			"");
	}
}

#ifdef SFXGE_HAVE_PAUSE_MEDIAOPTS

static unsigned int
sfxge_port_wanted_fc(struct sfxge_softc *sc)
{
	struct ifmedia_entry *ifm = sc->media.ifm_cur;

	if (ifm->ifm_media == (IFM_ETHER | IFM_AUTO))
		return (EFX_FCNTL_RESPOND | EFX_FCNTL_GENERATE);
	return (((ifm->ifm_media & IFM_ETH_RXPAUSE) ? EFX_FCNTL_RESPOND : 0) |
		((ifm->ifm_media & IFM_ETH_TXPAUSE) ? EFX_FCNTL_GENERATE : 0));
}

static unsigned int
sfxge_port_link_fc_ifm(struct sfxge_softc *sc)
{
	unsigned int wanted_fc, link_fc;

	efx_mac_fcntl_get(sc->enp, &wanted_fc, &link_fc);
	return ((link_fc & EFX_FCNTL_RESPOND) ? IFM_ETH_RXPAUSE : 0) |
		((link_fc & EFX_FCNTL_GENERATE) ? IFM_ETH_TXPAUSE : 0);
}

#else /* !SFXGE_HAVE_PAUSE_MEDIAOPTS */

static unsigned int
sfxge_port_wanted_fc(struct sfxge_softc *sc)
{
	return (sc->port.wanted_fc);
}

static unsigned int
sfxge_port_link_fc_ifm(struct sfxge_softc *sc)
{
	return (0);
}

static int
sfxge_port_wanted_fc_handler(SYSCTL_HANDLER_ARGS)
{
	struct sfxge_softc *sc;
	struct sfxge_port *port;
	unsigned int fcntl;
	int error;

	sc = arg1;
	port = &sc->port;

	mtx_lock(&port->lock);

	if (req->newptr != NULL) {
		if ((error = SYSCTL_IN(req, &fcntl, sizeof(fcntl))) != 0)
			goto out;

		if (port->wanted_fc == fcntl)
			goto out;

		port->wanted_fc = fcntl;

		if (port->init_state != SFXGE_PORT_STARTED)
			goto out;

		error = efx_mac_fcntl_set(sc->enp, port->wanted_fc, B_TRUE);
	} else {
		error = SYSCTL_OUT(req, &port->wanted_fc,
				   sizeof(port->wanted_fc));
	}

out:
	mtx_unlock(&port->lock);

	return (error);
}

static int
sfxge_port_link_fc_handler(SYSCTL_HANDLER_ARGS)
{
	struct sfxge_softc *sc;
	struct sfxge_port *port;
	unsigned int wanted_fc, link_fc;
	int error;

	sc = arg1;
	port = &sc->port;

	mtx_lock(&port->lock);
	if (port->init_state == SFXGE_PORT_STARTED && SFXGE_LINK_UP(sc))
		efx_mac_fcntl_get(sc->enp, &wanted_fc, &link_fc);
	else
		link_fc = 0;
	error = SYSCTL_OUT(req, &link_fc, sizeof(link_fc));
	mtx_unlock(&port->lock);

	return (error);
}

#endif /* SFXGE_HAVE_PAUSE_MEDIAOPTS */

static const uint64_t sfxge_link_baudrate[EFX_LINK_NMODES] = {
	[EFX_LINK_10HDX]	= IF_Mbps(10),
	[EFX_LINK_10FDX]	= IF_Mbps(10),
	[EFX_LINK_100HDX]	= IF_Mbps(100),
	[EFX_LINK_100FDX]	= IF_Mbps(100),
	[EFX_LINK_1000HDX]	= IF_Gbps(1),
	[EFX_LINK_1000FDX]	= IF_Gbps(1),
	[EFX_LINK_10000FDX]     = IF_Gbps(10),
};

void
sfxge_mac_link_update(struct sfxge_softc *sc, efx_link_mode_t mode)
{
	struct sfxge_port *port;
	int link_state;

	port = &sc->port;

	if (port->link_mode == mode)
		return;

	port->link_mode = mode;

	/* Push link state update to the OS */
	link_state = (port->link_mode != EFX_LINK_DOWN ?
		      LINK_STATE_UP : LINK_STATE_DOWN);
	sc->ifnet->if_baudrate = sfxge_link_baudrate[port->link_mode];
	if_link_state_change(sc->ifnet, link_state);
}

static void
sfxge_mac_poll_work(void *arg, int npending)
{
	struct sfxge_softc *sc;
	efx_nic_t *enp;
	struct sfxge_port *port;
	efx_link_mode_t mode;

	sc = (struct sfxge_softc *)arg;
	enp = sc->enp;
	port = &sc->port;

	mtx_lock(&port->lock);

	if (port->init_state != SFXGE_PORT_STARTED)
		goto done;

	/* This may sleep waiting for MCDI completion */
	(void)efx_port_poll(enp, &mode);
	sfxge_mac_link_update(sc, mode);

done:
	mtx_unlock(&port->lock);
}

static int
sfxge_mac_filter_set_locked(struct sfxge_softc *sc)
{
	unsigned int bucket[EFX_MAC_HASH_BITS];
	struct ifnet *ifp = sc->ifnet;
	struct ifmultiaddr *ifma;
	struct sockaddr_dl *sa;
	efx_nic_t *enp = sc->enp;
	unsigned int index;
	int rc;

	/* Set promisc-unicast and broadcast filter bits */
	if ((rc = efx_mac_filter_set(enp, !!(ifp->if_flags & IFF_PROMISC),
				     B_TRUE)) != 0)
		return (rc);

	/* Set multicast hash filter */
	if (ifp->if_flags & (IFF_PROMISC | IFF_ALLMULTI)) {
		for (index = 0; index < EFX_MAC_HASH_BITS; index++)
			bucket[index] = 1;
	} else {
		/* Broadcast frames also go through the multicast
		 * filter, and the broadcast address hashes to
		 * 0xff. */
		bucket[0xff] = 1;

		if_maddr_rlock(ifp);
		TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
			if (ifma->ifma_addr->sa_family == AF_LINK) {
				sa = (struct sockaddr_dl *)ifma->ifma_addr;
				index = ether_crc32_le(LLADDR(sa), 6) & 0xff;
				bucket[index] = 1;
			}
		}
		if_maddr_runlock(ifp);
	}
	return (efx_mac_hash_set(enp, bucket));
}

int
sfxge_mac_filter_set(struct sfxge_softc *sc)
{
	struct sfxge_port *port = &sc->port;
	int rc;

	mtx_lock(&port->lock);
	/*
	 * The function may be called without softc_lock held in the
	 * case of SIOCADDMULTI and SIOCDELMULTI ioctls. ioctl handler
	 * checks IFF_DRV_RUNNING flag which implies port started, but
	 * it is not guaranteed to remain. softc_lock shared lock can't
	 * be held in the case of these ioctls processing, since it
	 * results in failure where kernel complains that non-sleepable
	 * lock is held in sleeping thread. Both problems are repeatable
	 * on LAG with LACP proto bring up.
	 */
	if (port->init_state == SFXGE_PORT_STARTED)
		rc = sfxge_mac_filter_set_locked(sc);
	else
		rc = 0;
	mtx_unlock(&port->lock);
	return (rc);
}

void
sfxge_port_stop(struct sfxge_softc *sc)
{
	struct sfxge_port *port;
	efx_nic_t *enp;

	port = &sc->port;
	enp = sc->enp;

	mtx_lock(&port->lock);

	KASSERT(port->init_state == SFXGE_PORT_STARTED,
	    ("port not started"));

	port->init_state = SFXGE_PORT_INITIALIZED;

	port->mac_stats.update_time = 0;

	/* This may call MCDI */
	(void)efx_mac_drain(enp, B_TRUE);

	(void)efx_mac_stats_periodic(enp, &port->mac_stats.dma_buf, 0, B_FALSE);

	port->link_mode = EFX_LINK_UNKNOWN;

	/* Destroy the common code port object. */
	efx_port_fini(sc->enp);

	mtx_unlock(&port->lock);
}

int
sfxge_port_start(struct sfxge_softc *sc)
{
	uint8_t mac_addr[ETHER_ADDR_LEN];
	struct ifnet *ifp = sc->ifnet;
	struct sfxge_port *port;
	efx_nic_t *enp;
	size_t pdu;
	int rc;

	port = &sc->port;
	enp = sc->enp;

	mtx_lock(&port->lock);

	KASSERT(port->init_state == SFXGE_PORT_INITIALIZED,
	    ("port not initialized"));

	/* Initialize the port object in the common code. */
	if ((rc = efx_port_init(sc->enp)) != 0)
		goto fail;

	/* Set the SDU */
	pdu = EFX_MAC_PDU(ifp->if_mtu);
	if ((rc = efx_mac_pdu_set(enp, pdu)) != 0)
		goto fail2;

	if ((rc = efx_mac_fcntl_set(enp, sfxge_port_wanted_fc(sc), B_TRUE))
	    != 0)
		goto fail2;

	/* Set the unicast address */
	if_addr_rlock(ifp);
	bcopy(LLADDR((struct sockaddr_dl *)ifp->if_addr->ifa_addr),
	      mac_addr, sizeof(mac_addr));
	if_addr_runlock(ifp);
	if ((rc = efx_mac_addr_set(enp, mac_addr)) != 0)
		goto fail;

	sfxge_mac_filter_set_locked(sc);

	/* Update MAC stats by DMA every second */
	if ((rc = efx_mac_stats_periodic(enp, &port->mac_stats.dma_buf,
	    1000, B_FALSE)) != 0)
		goto fail2;

	if ((rc = efx_mac_drain(enp, B_FALSE)) != 0)
		goto fail3;

	if ((rc = efx_phy_adv_cap_set(sc->enp, sc->media.ifm_cur->ifm_data))
	    != 0)
		goto fail4;

	port->init_state = SFXGE_PORT_STARTED;

	/* Single poll in case there were missing initial events */
	mtx_unlock(&port->lock);
	sfxge_mac_poll_work(sc, 0);

	return (0);

fail4:
	(void)efx_mac_drain(enp, B_TRUE);
fail3:
	(void)efx_mac_stats_periodic(enp, &port->mac_stats.dma_buf,
	    0, B_FALSE);
fail2:
	efx_port_fini(sc->enp);
fail:
	mtx_unlock(&port->lock);

	return (rc);
}

static int
sfxge_phy_stat_update(struct sfxge_softc *sc)
{
	struct sfxge_port *port = &sc->port;
	efsys_mem_t *esmp = &port->phy_stats.dma_buf;
	clock_t now;
	unsigned int count;
	int rc;

	mtx_lock(&port->lock);

	if (port->init_state != SFXGE_PORT_STARTED) {
		rc = 0;
		goto out;
	}

	now = ticks;
	if (now - port->phy_stats.update_time < hz) {
		rc = 0;
		goto out;
	}

	port->phy_stats.update_time = now;

	/* If we're unlucky enough to read statistics wduring the DMA, wait
	 * up to 10ms for it to finish (typically takes <500us) */
	for (count = 0; count < 100; ++count) {
		EFSYS_PROBE1(wait, unsigned int, count);

		/* Synchronize the DMA memory for reading */
		bus_dmamap_sync(esmp->esm_tag, esmp->esm_map,
		    BUS_DMASYNC_POSTREAD);

		/* Try to update the cached counters */
		if ((rc = efx_phy_stats_update(sc->enp, esmp,
		    port->phy_stats.decode_buf)) != EAGAIN)
			goto out;

		DELAY(100);
	}

	rc = ETIMEDOUT;
out:
	mtx_unlock(&port->lock);
	return (rc);
}

static int
sfxge_phy_stat_handler(SYSCTL_HANDLER_ARGS)
{
	struct sfxge_softc *sc = arg1;
	unsigned int id = arg2;
	int rc;

	if ((rc = sfxge_phy_stat_update(sc)) != 0)
		return (rc);

	return (SYSCTL_OUT(req,
			  (uint32_t *)sc->port.phy_stats.decode_buf + id,
			  sizeof(uint32_t)));
}

static void
sfxge_phy_stat_init(struct sfxge_softc *sc)
{
	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(sc->dev);
	struct sysctl_oid_list *stat_list;
	unsigned int id;
	const char *name;
	uint64_t stat_mask = efx_nic_cfg_get(sc->enp)->enc_phy_stat_mask;

	stat_list = SYSCTL_CHILDREN(sc->stats_node);

	/* Initialise the named stats */
	for (id = 0; id < EFX_PHY_NSTATS; id++) {
		if (!(stat_mask & ((uint64_t)1 << id)))
			continue;
		name = efx_phy_stat_name(sc->enp, id);
		SYSCTL_ADD_PROC(
			ctx, stat_list,
			OID_AUTO, name, CTLTYPE_UINT|CTLFLAG_RD,
			sc, id, sfxge_phy_stat_handler,
			id == EFX_PHY_STAT_OUI ? "IX" : "IU",
			"");
	}
}

void
sfxge_port_fini(struct sfxge_softc *sc)
{
	struct sfxge_port *port;
	efsys_mem_t *esmp;

	port = &sc->port;
	esmp = &port->mac_stats.dma_buf;

	KASSERT(port->init_state == SFXGE_PORT_INITIALIZED,
	    ("Port not initialized"));

	port->init_state = SFXGE_PORT_UNINITIALIZED;

	port->link_mode = EFX_LINK_UNKNOWN;

	/* Finish with PHY DMA memory */
	sfxge_dma_free(&port->phy_stats.dma_buf);
	free(port->phy_stats.decode_buf, M_SFXGE);

	sfxge_dma_free(esmp);
	free(port->mac_stats.decode_buf, M_SFXGE);

	mtx_destroy(&port->lock);

	port->sc = NULL;
}

int
sfxge_port_init(struct sfxge_softc *sc)
{
	struct sfxge_port *port;
	struct sysctl_ctx_list *sysctl_ctx;
	struct sysctl_oid *sysctl_tree;
	efsys_mem_t *mac_stats_buf, *phy_stats_buf;
	int rc;

	port = &sc->port;
	mac_stats_buf = &port->mac_stats.dma_buf;
	phy_stats_buf = &port->phy_stats.dma_buf;

	KASSERT(port->init_state == SFXGE_PORT_UNINITIALIZED,
	    ("Port already initialized"));

	port->sc = sc;

	mtx_init(&port->lock, "sfxge_port", NULL, MTX_DEF);

	port->phy_stats.decode_buf = malloc(EFX_PHY_NSTATS * sizeof(uint32_t),
					    M_SFXGE, M_WAITOK | M_ZERO);
	if ((rc = sfxge_dma_alloc(sc, EFX_PHY_STATS_SIZE, phy_stats_buf)) != 0)
		goto fail;
	bzero(phy_stats_buf->esm_base, phy_stats_buf->esm_size);
	sfxge_phy_stat_init(sc);

	sysctl_ctx = device_get_sysctl_ctx(sc->dev);
	sysctl_tree = device_get_sysctl_tree(sc->dev);

#ifndef SFXGE_HAVE_PAUSE_MEDIAOPTS
	/* If flow control cannot be configured or reported through
	 * ifmedia, provide sysctls for it. */
	port->wanted_fc = EFX_FCNTL_RESPOND | EFX_FCNTL_GENERATE;
	SYSCTL_ADD_PROC(sysctl_ctx, SYSCTL_CHILDREN(sysctl_tree), OID_AUTO,
	    "wanted_fc", CTLTYPE_UINT|CTLFLAG_RW, sc, 0,
	    sfxge_port_wanted_fc_handler, "IU", "wanted flow control mode");
	SYSCTL_ADD_PROC(sysctl_ctx, SYSCTL_CHILDREN(sysctl_tree), OID_AUTO,
	    "link_fc", CTLTYPE_UINT|CTLFLAG_RD, sc, 0,
	    sfxge_port_link_fc_handler, "IU", "link flow control mode");
#endif

	port->mac_stats.decode_buf = malloc(EFX_MAC_NSTATS * sizeof(uint64_t),
					    M_SFXGE, M_WAITOK | M_ZERO);
	if ((rc = sfxge_dma_alloc(sc, EFX_MAC_STATS_SIZE, mac_stats_buf)) != 0)
		goto fail2;
	bzero(mac_stats_buf->esm_base, mac_stats_buf->esm_size);
	sfxge_mac_stat_init(sc);

	port->init_state = SFXGE_PORT_INITIALIZED;

	return (0);

fail2:
	free(port->mac_stats.decode_buf, M_SFXGE);
	sfxge_dma_free(phy_stats_buf);
fail:
	free(port->phy_stats.decode_buf, M_SFXGE);
	(void)mtx_destroy(&port->lock);
	port->sc = NULL;
	return (rc);
}

static int sfxge_link_mode[EFX_PHY_MEDIA_NTYPES][EFX_LINK_NMODES] = {
	[EFX_PHY_MEDIA_CX4] = {
		[EFX_LINK_10000FDX]	= IFM_ETHER | IFM_FDX | IFM_10G_CX4,
	},
	[EFX_PHY_MEDIA_KX4] = {
		[EFX_LINK_10000FDX]	= IFM_ETHER | IFM_FDX | IFM_10G_KX4,
	},
	[EFX_PHY_MEDIA_XFP] = {
		/* Don't know the module type, but assume SR for now. */
		[EFX_LINK_10000FDX]	= IFM_ETHER | IFM_FDX | IFM_10G_SR,
	},
	[EFX_PHY_MEDIA_SFP_PLUS] = {
		/* Don't know the module type, but assume SX/SR for now. */
		[EFX_LINK_1000FDX]	= IFM_ETHER | IFM_FDX | IFM_1000_SX,
		[EFX_LINK_10000FDX]	= IFM_ETHER | IFM_FDX | IFM_10G_SR,
	},
	[EFX_PHY_MEDIA_BASE_T] = {
		[EFX_LINK_10HDX]	= IFM_ETHER | IFM_HDX | IFM_10_T,
		[EFX_LINK_10FDX]	= IFM_ETHER | IFM_FDX | IFM_10_T,
		[EFX_LINK_100HDX]	= IFM_ETHER | IFM_HDX | IFM_100_TX,
		[EFX_LINK_100FDX]	= IFM_ETHER | IFM_FDX | IFM_100_TX,
		[EFX_LINK_1000HDX]	= IFM_ETHER | IFM_HDX | IFM_1000_T,
		[EFX_LINK_1000FDX]	= IFM_ETHER | IFM_FDX | IFM_1000_T,
		[EFX_LINK_10000FDX]	= IFM_ETHER | IFM_FDX | IFM_10G_T,
	},
};

static void
sfxge_media_status(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct sfxge_softc *sc;
	efx_phy_media_type_t medium_type;
	efx_link_mode_t mode;

	sc = ifp->if_softc;
	sx_xlock(&sc->softc_lock);

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	if (SFXGE_RUNNING(sc) && SFXGE_LINK_UP(sc)) {
		ifmr->ifm_status |= IFM_ACTIVE;

		efx_phy_media_type_get(sc->enp, &medium_type);
		mode = sc->port.link_mode;
		ifmr->ifm_active |= sfxge_link_mode[medium_type][mode];
		ifmr->ifm_active |= sfxge_port_link_fc_ifm(sc);
	}

	sx_xunlock(&sc->softc_lock);
}

static int
sfxge_media_change(struct ifnet *ifp)
{
	struct sfxge_softc *sc;
	struct ifmedia_entry *ifm;
	int rc;

	sc = ifp->if_softc;
	ifm = sc->media.ifm_cur;

	sx_xlock(&sc->softc_lock);

	if (!SFXGE_RUNNING(sc)) {
		rc = 0;
		goto out;
	}

	rc = efx_mac_fcntl_set(sc->enp, sfxge_port_wanted_fc(sc), B_TRUE);
	if (rc != 0)
		goto out;

	rc = efx_phy_adv_cap_set(sc->enp, ifm->ifm_data);
out:
	sx_xunlock(&sc->softc_lock);

	return (rc);
}

int sfxge_port_ifmedia_init(struct sfxge_softc *sc)
{
	efx_phy_media_type_t medium_type;
	uint32_t cap_mask, mode_cap_mask;
	efx_link_mode_t mode;
	int mode_ifm, best_mode_ifm = 0;
	int rc;

	/* We need port state to initialise the ifmedia list. */
	if ((rc = efx_nic_init(sc->enp)) != 0)
		goto out;
	if ((rc = efx_port_init(sc->enp)) != 0)
		goto out2;

	/*
	 * Register ifconfig callbacks for querying and setting the
	 * link mode and link status.
	 */
	ifmedia_init(&sc->media, IFM_IMASK, sfxge_media_change,
	    sfxge_media_status);

	/*
	 * Map firmware medium type and capabilities to ifmedia types.
	 * ifmedia does not distinguish between forcing the link mode
	 * and disabling auto-negotiation.  1000BASE-T and 10GBASE-T
	 * require AN even if only one link mode is enabled, and for
	 * 100BASE-TX it is useful even if the link mode is forced.
	 * Therefore we never disable auto-negotiation.
	 *
	 * Also enable and advertise flow control by default.
	 */

	efx_phy_media_type_get(sc->enp, &medium_type);
	efx_phy_adv_cap_get(sc->enp, EFX_PHY_CAP_PERM, &cap_mask);

	EFX_STATIC_ASSERT(EFX_LINK_10HDX == EFX_PHY_CAP_10HDX + 1);
	EFX_STATIC_ASSERT(EFX_LINK_10FDX == EFX_PHY_CAP_10FDX + 1);
	EFX_STATIC_ASSERT(EFX_LINK_100HDX == EFX_PHY_CAP_100HDX + 1);
	EFX_STATIC_ASSERT(EFX_LINK_100FDX == EFX_PHY_CAP_100FDX + 1);
	EFX_STATIC_ASSERT(EFX_LINK_1000HDX == EFX_PHY_CAP_1000HDX + 1);
	EFX_STATIC_ASSERT(EFX_LINK_1000FDX == EFX_PHY_CAP_1000FDX + 1);
	EFX_STATIC_ASSERT(EFX_LINK_10000FDX == EFX_PHY_CAP_10000FDX + 1);

	for (mode = EFX_LINK_10HDX; mode <= EFX_LINK_10000FDX; mode++) {
		mode_cap_mask = 1 << (mode - 1);
		mode_ifm = sfxge_link_mode[medium_type][mode];

		if ((cap_mask & mode_cap_mask) && mode_ifm) {
			mode_cap_mask |= cap_mask & (1 << EFX_PHY_CAP_AN);

#ifdef SFXGE_HAVE_PAUSE_MEDIAOPTS
			/* No flow-control */
			ifmedia_add(&sc->media, mode_ifm, mode_cap_mask, NULL);

			/* Respond-only.  If using AN, we implicitly
			 * offer symmetric as well, but that doesn't
			 * mean we *have* to generate pause frames.
			 */
			mode_cap_mask |= cap_mask & ((1 << EFX_PHY_CAP_PAUSE) |
						     (1 << EFX_PHY_CAP_ASYM));
			mode_ifm |= IFM_ETH_RXPAUSE;
			ifmedia_add(&sc->media, mode_ifm, mode_cap_mask, NULL);

			/* Symmetric */
			mode_cap_mask &= ~(1 << EFX_PHY_CAP_ASYM);
			mode_ifm |= IFM_ETH_TXPAUSE;
#else /* !SFXGE_HAVE_PAUSE_MEDIAOPTS */
			mode_cap_mask |= cap_mask & (1 << EFX_PHY_CAP_PAUSE);
#endif
			ifmedia_add(&sc->media, mode_ifm, mode_cap_mask, NULL);

			/* Link modes are numbered in order of speed,
			 * so assume the last one available is the best.
			 */
			best_mode_ifm = mode_ifm;
		}
	}

	if (cap_mask & (1 << EFX_PHY_CAP_AN)) {
		/* Add autoselect mode. */
		mode_ifm = IFM_ETHER | IFM_AUTO;
		ifmedia_add(&sc->media, mode_ifm,
			    cap_mask & ~(1 << EFX_PHY_CAP_ASYM), NULL);
		best_mode_ifm = mode_ifm;
	}

	if (best_mode_ifm != 0)
		ifmedia_set(&sc->media, best_mode_ifm);

	/* Now discard port state until interface is started. */
	efx_port_fini(sc->enp);
out2:
	efx_nic_fini(sc->enp);
out:
	return (rc);
}
