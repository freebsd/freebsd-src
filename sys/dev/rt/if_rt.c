/*-
 * Copyright (c) 2011, Aleksandr Rybalko
 * based on hard work
 * by Alexander Egorenkov <egorenar@gmail.com>
 * and by Damien Bergamini <damien.bergamini@free.fr>
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

#include "if_rtvar.h"
#include "if_rtreg.h"

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_vlan_var.h>

#include <net/bpf.h>

#include <machine/bus.h>
#include <machine/cache.h>
#include <machine/cpufunc.h>
#include <machine/resource.h>
#include <vm/vm_param.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/pmap.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <mips/rt305x/rt305x_sysctlvar.h>
#include <mips/rt305x/rt305xreg.h>

#ifdef IF_RT_PHY_SUPPORT
#include "miibus_if.h"
#endif

/*
 * Defines and macros
 */
#define	RT_MAX_AGG_SIZE			3840

#define	RT_TX_DATA_SEG0_SIZE		MJUMPAGESIZE

#define	RT_MS(_v, _f)			(((_v) & _f) >> _f##_S)
#define	RT_SM(_v, _f)			(((_v) << _f##_S) & _f)

#define	RT_TX_WATCHDOG_TIMEOUT		5

/*
 * Static function prototypes
 */
static int	rt_probe(device_t dev);
static int	rt_attach(device_t dev);
static int	rt_detach(device_t dev);
static int	rt_shutdown(device_t dev);
static int	rt_suspend(device_t dev);
static int	rt_resume(device_t dev);
static void	rt_init_locked(void *priv);
static void	rt_init(void *priv);
static void	rt_stop_locked(void *priv);
static void	rt_stop(void *priv);
static void	rt_start(struct ifnet *ifp);
static int	rt_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data);
static void	rt_periodic(void *arg);
static void	rt_tx_watchdog(void *arg);
static void	rt_intr(void *arg);
static void	rt_tx_coherent_intr(struct rt_softc *sc);
static void	rt_rx_coherent_intr(struct rt_softc *sc);
static void	rt_rx_delay_intr(struct rt_softc *sc);
static void	rt_tx_delay_intr(struct rt_softc *sc);
static void	rt_rx_intr(struct rt_softc *sc);
static void	rt_tx_intr(struct rt_softc *sc, int qid);
static void	rt_rx_done_task(void *context, int pending);
static void	rt_tx_done_task(void *context, int pending);
static void	rt_periodic_task(void *context, int pending);
static int	rt_rx_eof(struct rt_softc *sc, int limit);
static void	rt_tx_eof(struct rt_softc *sc,
		    struct rt_softc_tx_ring *ring);
static void	rt_update_stats(struct rt_softc *sc);
static void	rt_watchdog(struct rt_softc *sc);
static void	rt_update_raw_counters(struct rt_softc *sc);
static void	rt_intr_enable(struct rt_softc *sc, uint32_t intr_mask);
static void	rt_intr_disable(struct rt_softc *sc, uint32_t intr_mask);
static int	rt_txrx_enable(struct rt_softc *sc);
static int	rt_alloc_rx_ring(struct rt_softc *sc,
		    struct rt_softc_rx_ring *ring);
static void	rt_reset_rx_ring(struct rt_softc *sc,
		    struct rt_softc_rx_ring *ring);
static void	rt_free_rx_ring(struct rt_softc *sc,
		    struct rt_softc_rx_ring *ring);
static int	rt_alloc_tx_ring(struct rt_softc *sc,
		    struct rt_softc_tx_ring *ring, int qid);
static void	rt_reset_tx_ring(struct rt_softc *sc,
		    struct rt_softc_tx_ring *ring);
static void	rt_free_tx_ring(struct rt_softc *sc,
		    struct rt_softc_tx_ring *ring);
static void	rt_dma_map_addr(void *arg, bus_dma_segment_t *segs,
		    int nseg, int error);
static void	rt_sysctl_attach(struct rt_softc *sc);
#ifdef IF_RT_PHY_SUPPORT
void		rt_miibus_statchg(device_t);
static int	rt_miibus_readreg(device_t, int, int);
static int	rt_miibus_writereg(device_t, int, int, int);
#endif
static int	rt_ifmedia_upd(struct ifnet *);
static void	rt_ifmedia_sts(struct ifnet *, struct ifmediareq *);

static SYSCTL_NODE(_hw, OID_AUTO, rt, CTLFLAG_RD, 0, "RT driver parameters");
#ifdef IF_RT_DEBUG
static int rt_debug = 0;
SYSCTL_INT(_hw_rt, OID_AUTO, debug, CTLFLAG_RWTUN, &rt_debug, 0,
    "RT debug level");
#endif

static int
rt_probe(device_t dev)
{
	device_set_desc(dev, "Ralink RT305XF onChip Ethernet MAC");
	return (BUS_PROBE_NOWILDCARD);
}

/*
 * macaddr_atoi - translate string MAC address to uint8_t array
 */
static int
macaddr_atoi(const char *str, uint8_t *mac)
{
	int count, i;
	unsigned int amac[ETHER_ADDR_LEN];	/* Aligned version */

	count = sscanf(str, "%x%*c%x%*c%x%*c%x%*c%x%*c%x",
	    &amac[0], &amac[1], &amac[2],
	    &amac[3], &amac[4], &amac[5]);
	if (count < ETHER_ADDR_LEN) {
		memset(mac, 0, ETHER_ADDR_LEN);
		return (1);
	}

	/* Copy aligned to result */
	for (i = 0; i < ETHER_ADDR_LEN; i ++)
		mac[i] = (amac[i] & 0xff);

	return (0);
}

#ifdef USE_GENERATED_MAC_ADDRESS
static char *
kernenv_next(char *cp)
{

	if (cp != NULL) {
		while (*cp != 0)
			cp++;
		cp++;
		if (*cp == 0)
			cp = NULL;
	}
	return (cp);
}

/*
 * generate_mac(uin8_t *mac)
 * This is MAC address generator for cases when real device MAC address
 * unknown or not yet accessible.
 * Use 'b','s','d' signature and 3 octets from CRC32 on kenv.
 * MAC = 'b', 's', 'd', CRC[3]^CRC[2], CRC[1], CRC[0]
 *
 * Output - MAC address, that do not change between reboots, if hints or
 * bootloader info unchange.
 */
static void
generate_mac(uint8_t *mac)
{
	unsigned char *cp;
	int i = 0;
	uint32_t crc = 0xffffffff;

	/* Generate CRC32 on kenv */
	if (dynamic_kenv) {
		for (cp = kenvp[0]; cp != NULL; cp = kenvp[++i]) {
			crc = calculate_crc32c(crc, cp, strlen(cp) + 1);
		}
	} else {
		for (cp = kern_envp; cp != NULL; cp = kernenv_next(cp)) {
			crc = calculate_crc32c(crc, cp, strlen(cp) + 1);
		}
	}
	crc = ~crc;

	mac[0] = 'b';
	mac[1] = 's';
	mac[2] = 'd';
	mac[3] = (crc >> 24) ^ ((crc >> 16) & 0xff);
	mac[4] = (crc >> 8) & 0xff;
	mac[5] = crc & 0xff;
}
#endif

/*
 * ether_request_mac - try to find usable MAC address.
 */
static int
ether_request_mac(device_t dev, uint8_t *mac)
{
	char *var;

	/*
	 * "ethaddr" is passed via envp on RedBoot platforms
	 * "kmac" is passed via argv on RouterBOOT platforms
	 */
#if defined(__U_BOOT__) ||  defined(__REDBOOT__) || defined(__ROUTERBOOT__)
	if ((var = getenv("ethaddr")) != NULL ||
	    (var = getenv("kmac")) != NULL ) {

		if(!macaddr_atoi(var, mac)) {
			printf("%s: use %s macaddr from KENV\n",
			    device_get_nameunit(dev), var);
			freeenv(var);
			return (0);
		}
		freeenv(var);
	}
#endif

	/*
	 * Try from hints
	 * hint.[dev].[unit].macaddr
	 */
	if (!resource_string_value(device_get_name(dev),
	    device_get_unit(dev), "macaddr", (const char **)&var)) {

		if(!macaddr_atoi(var, mac)) {
			printf("%s: use %s macaddr from hints\n",
			    device_get_nameunit(dev), var);
			return (0);
		}
	}

#ifdef USE_GENERATED_MAC_ADDRESS
	generate_mac(mac);

	device_printf(dev, "use generated %02x:%02x:%02x:%02x:%02x:%02x "
	    "macaddr\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
#else
	/* Hardcoded */
	mac[0] = 0x00;
	mac[1] = 0x18;
	mac[2] = 0xe7;
	mac[3] = 0xd5;
	mac[4] = 0x83;
	mac[5] = 0x90;

	device_printf(dev, "use hardcoded 00:18:e7:d5:83:90 macaddr\n");
#endif

	return (0);
}

static int
rt_attach(device_t dev)
{
	struct rt_softc *sc;
	struct ifnet *ifp;
	int error, i;

	sc = device_get_softc(dev);
	sc->dev = dev;

	mtx_init(&sc->lock, device_get_nameunit(dev), MTX_NETWORK_LOCK,
	    MTX_DEF | MTX_RECURSE);

	sc->mem_rid = 0;
	sc->mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &sc->mem_rid,
	    RF_ACTIVE);
	if (sc->mem == NULL) {
		device_printf(dev, "could not allocate memory resource\n");
		error = ENXIO;
		goto fail;
	}

	sc->bst = rman_get_bustag(sc->mem);
	sc->bsh = rman_get_bushandle(sc->mem);

	sc->irq_rid = 0;
	sc->irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &sc->irq_rid,
	    RF_ACTIVE);
	if (sc->irq == NULL) {
		device_printf(dev,
		    "could not allocate interrupt resource\n");
		error = ENXIO;
		goto fail;
	}

#ifdef IF_RT_DEBUG
	sc->debug = rt_debug;

	SYSCTL_ADD_INT(device_get_sysctl_ctx(dev),
		SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
		"debug", CTLFLAG_RW, &sc->debug, 0, "rt debug level");
#endif

	device_printf(dev, "RT305XF Ethernet MAC (rev 0x%08x)\n",
	    sc->mac_rev);

	/* Reset hardware */
	RT_WRITE(sc, GE_PORT_BASE + FE_RST_GLO, PSE_RESET);

	RT_WRITE(sc, GDMA1_BASE + GDMA_FWD_CFG,
	    (
	    GDM_ICS_EN | /* Enable IP Csum */
	    GDM_TCS_EN | /* Enable TCP Csum */
	    GDM_UCS_EN | /* Enable UDP Csum */
	    GDM_STRPCRC | /* Strip CRC from packet */
	    GDM_DST_PORT_CPU << GDM_UFRC_P_SHIFT | /* Forward UCast to CPU */
	    GDM_DST_PORT_CPU << GDM_BFRC_P_SHIFT | /* Forward BCast to CPU */
	    GDM_DST_PORT_CPU << GDM_MFRC_P_SHIFT | /* Forward MCast to CPU */
	    GDM_DST_PORT_CPU << GDM_OFRC_P_SHIFT   /* Forward Other to CPU */
	    ));

	/* allocate Tx and Rx rings */
	for (i = 0; i < RT_SOFTC_TX_RING_COUNT; i++) {
		error = rt_alloc_tx_ring(sc, &sc->tx_ring[i], i);
		if (error != 0) {
			device_printf(dev, "could not allocate Tx ring #%d\n",
			    i);
			goto fail;
		}
	}

	sc->tx_ring_mgtqid = 5;

	error = rt_alloc_rx_ring(sc, &sc->rx_ring);
	if (error != 0) {
		device_printf(dev, "could not allocate Rx ring\n");
		goto fail;
	}

	callout_init(&sc->periodic_ch, 0);
	callout_init_mtx(&sc->tx_watchdog_ch, &sc->lock, 0);

	ifp = sc->ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		device_printf(dev, "could not if_alloc()\n");
		error = ENOMEM;
		goto fail;
	}

	ifp->if_softc = sc;
	if_initname(ifp, device_get_name(sc->dev), device_get_unit(sc->dev));
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_init = rt_init;
	ifp->if_ioctl = rt_ioctl;
	ifp->if_start = rt_start;
#define	RT_TX_QLEN	256

	IFQ_SET_MAXLEN(&ifp->if_snd, RT_TX_QLEN);
	ifp->if_snd.ifq_drv_maxlen = RT_TX_QLEN;
	IFQ_SET_READY(&ifp->if_snd);

#ifdef IF_RT_PHY_SUPPORT
	error = mii_attach(dev, &sc->rt_miibus, ifp, rt_ifmedia_upd,
	    rt_ifmedia_sts, BMSR_DEFCAPMASK, MII_PHY_ANY, MII_OFFSET_ANY, 0);
	if (error != 0) {
		device_printf(dev, "attaching PHYs failed\n");
		error = ENXIO;
		goto fail;
	}
#else
	ifmedia_init(&sc->rt_ifmedia, 0, rt_ifmedia_upd, rt_ifmedia_sts);
	ifmedia_add(&sc->rt_ifmedia, IFM_ETHER | IFM_100_TX | IFM_FDX, 0,
	    NULL);
	ifmedia_set(&sc->rt_ifmedia, IFM_ETHER | IFM_100_TX | IFM_FDX);

#endif /* IF_RT_PHY_SUPPORT */

	ether_request_mac(dev, sc->mac_addr);
	ether_ifattach(ifp, sc->mac_addr);

	/*
	 * Tell the upper layer(s) we support long frames.
	 */
	ifp->if_hdrlen = sizeof(struct ether_vlan_header);
	ifp->if_capabilities |= IFCAP_VLAN_MTU;
	ifp->if_capenable |= IFCAP_VLAN_MTU;
	ifp->if_capabilities |= IFCAP_RXCSUM|IFCAP_TXCSUM;
	ifp->if_capenable |= IFCAP_RXCSUM|IFCAP_TXCSUM;

	/* init task queue */
	TASK_INIT(&sc->rx_done_task, 0, rt_rx_done_task, sc);
	TASK_INIT(&sc->tx_done_task, 0, rt_tx_done_task, sc);
	TASK_INIT(&sc->periodic_task, 0, rt_periodic_task, sc);

	sc->rx_process_limit = 100;

	sc->taskqueue = taskqueue_create("rt_taskq", M_NOWAIT,
	    taskqueue_thread_enqueue, &sc->taskqueue);

	taskqueue_start_threads(&sc->taskqueue, 1, PI_NET, "%s taskq",
	    device_get_nameunit(sc->dev));

	rt_sysctl_attach(sc);

	/* set up interrupt */
	error = bus_setup_intr(dev, sc->irq, INTR_TYPE_NET | INTR_MPSAFE,
	    NULL, rt_intr, sc, &sc->irqh);
	if (error != 0) {
		printf("%s: could not set up interrupt\n",
			device_get_nameunit(dev));
		goto fail;
	}
#ifdef IF_RT_DEBUG
	device_printf(dev, "debug var at %#08x\n", (u_int)&(sc->debug));
#endif

	return (0);

fail:
	/* free Tx and Rx rings */
	for (i = 0; i < RT_SOFTC_TX_RING_COUNT; i++)
		rt_free_tx_ring(sc, &sc->tx_ring[i]);

	rt_free_rx_ring(sc, &sc->rx_ring);

	mtx_destroy(&sc->lock);

	if (sc->mem != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, sc->mem_rid,
		    sc->mem);

	if (sc->irq != NULL)
		bus_release_resource(dev, SYS_RES_IRQ, sc->irq_rid,
		    sc->irq);

	return (error);
}

/*
 * Set media options.
 */
static int
rt_ifmedia_upd(struct ifnet *ifp)
{
	struct rt_softc *sc;
#ifdef IF_RT_PHY_SUPPORT
	struct mii_data *mii;
	struct mii_softc *miisc;
	int error = 0;

	sc = ifp->if_softc;
	RT_SOFTC_LOCK(sc);

	mii = device_get_softc(sc->rt_miibus);
	LIST_FOREACH(miisc, &mii->mii_phys, mii_list)
		PHY_RESET(miisc);
	error = mii_mediachg(mii);
	RT_SOFTC_UNLOCK(sc);

	return (error);

#else /* !IF_RT_PHY_SUPPORT */

	struct ifmedia *ifm;
	struct ifmedia_entry *ife;

	sc = ifp->if_softc;
	ifm = &sc->rt_ifmedia;
	ife = ifm->ifm_cur;

	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
		return (EINVAL);

	if (IFM_SUBTYPE(ife->ifm_media) == IFM_AUTO) {
		device_printf(sc->dev,
		    "AUTO is not supported for multiphy MAC");
		return (EINVAL);
	}

	/*
	 * Ignore everything
	 */
	return (0);
#endif /* IF_RT_PHY_SUPPORT */
}

/*
 * Report current media status.
 */
static void
rt_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
#ifdef IF_RT_PHY_SUPPORT
	struct rt_softc *sc;
	struct mii_data *mii;

	sc = ifp->if_softc;

	RT_SOFTC_LOCK(sc);
	mii = device_get_softc(sc->rt_miibus);
	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
	ifmr->ifm_active = IFM_ETHER | IFM_100_TX | IFM_FDX;
	ifmr->ifm_status = IFM_AVALID | IFM_ACTIVE;
	RT_SOFTC_UNLOCK(sc);
#else /* !IF_RT_PHY_SUPPORT */

	ifmr->ifm_status = IFM_AVALID | IFM_ACTIVE;
	ifmr->ifm_active = IFM_ETHER | IFM_100_TX | IFM_FDX;
#endif /* IF_RT_PHY_SUPPORT */
}

static int
rt_detach(device_t dev)
{
	struct rt_softc *sc;
	struct ifnet *ifp;
	int i;

	sc = device_get_softc(dev);
	ifp = sc->ifp;

	RT_DPRINTF(sc, RT_DEBUG_ANY, "detaching\n");

	RT_SOFTC_LOCK(sc);

	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

	callout_stop(&sc->periodic_ch);
	callout_stop(&sc->tx_watchdog_ch);

	taskqueue_drain(sc->taskqueue, &sc->rx_done_task);
	taskqueue_drain(sc->taskqueue, &sc->tx_done_task);
	taskqueue_drain(sc->taskqueue, &sc->periodic_task);

	/* free Tx and Rx rings */
	for (i = 0; i < RT_SOFTC_TX_RING_COUNT; i++)
		rt_free_tx_ring(sc, &sc->tx_ring[i]);

	rt_free_rx_ring(sc, &sc->rx_ring);

	RT_SOFTC_UNLOCK(sc);

#ifdef IF_RT_PHY_SUPPORT
	if (sc->rt_miibus != NULL)
		device_delete_child(dev, sc->rt_miibus);
#endif

	ether_ifdetach(ifp);
	if_free(ifp);

	taskqueue_free(sc->taskqueue);

	mtx_destroy(&sc->lock);

	bus_generic_detach(dev);
	bus_teardown_intr(dev, sc->irq, sc->irqh);
	bus_release_resource(dev, SYS_RES_IRQ, sc->irq_rid, sc->irq);
	bus_release_resource(dev, SYS_RES_MEMORY, sc->mem_rid, sc->mem);

	return (0);
}

static int
rt_shutdown(device_t dev)
{
	struct rt_softc *sc;

	sc = device_get_softc(dev);
	RT_DPRINTF(sc, RT_DEBUG_ANY, "shutting down\n");
	rt_stop(sc);

	return (0);
}

static int
rt_suspend(device_t dev)
{
	struct rt_softc *sc;

	sc = device_get_softc(dev);
	RT_DPRINTF(sc, RT_DEBUG_ANY, "suspending\n");
	rt_stop(sc);

	return (0);
}

static int
rt_resume(device_t dev)
{
	struct rt_softc *sc;
	struct ifnet *ifp;

	sc = device_get_softc(dev);
	ifp = sc->ifp;

	RT_DPRINTF(sc, RT_DEBUG_ANY, "resuming\n");

	if (ifp->if_flags & IFF_UP)
		rt_init(sc);

	return (0);
}

/*
 * rt_init_locked - Run initialization process having locked mtx.
 */
static void
rt_init_locked(void *priv)
{
	struct rt_softc *sc;
	struct ifnet *ifp;
#ifdef IF_RT_PHY_SUPPORT
	struct mii_data *mii;
#endif
	int i, ntries;
	uint32_t tmp;

	sc = priv;
	ifp = sc->ifp;
#ifdef IF_RT_PHY_SUPPORT
	mii = device_get_softc(sc->rt_miibus);
#endif

	RT_DPRINTF(sc, RT_DEBUG_ANY, "initializing\n");

	RT_SOFTC_ASSERT_LOCKED(sc);

	/* hardware reset */
	RT_WRITE(sc, GE_PORT_BASE + FE_RST_GLO, PSE_RESET);
	rt305x_sysctl_set(SYSCTL_RSTCTRL, SYSCTL_RSTCTRL_FRENG);

	/* Fwd to CPU (uni|broad|multi)cast and Unknown */
	RT_WRITE(sc, GDMA1_BASE + GDMA_FWD_CFG,
	    (
	    GDM_ICS_EN | /* Enable IP Csum */
	    GDM_TCS_EN | /* Enable TCP Csum */
	    GDM_UCS_EN | /* Enable UDP Csum */
	    GDM_STRPCRC | /* Strip CRC from packet */
	    GDM_DST_PORT_CPU << GDM_UFRC_P_SHIFT | /* Forward UCast to CPU */
	    GDM_DST_PORT_CPU << GDM_BFRC_P_SHIFT | /* Forward BCast to CPU */
	    GDM_DST_PORT_CPU << GDM_MFRC_P_SHIFT | /* Forward MCast to CPU */
	    GDM_DST_PORT_CPU << GDM_OFRC_P_SHIFT   /* Forward Other to CPU */
	    ));

	/* disable DMA engine */
	RT_WRITE(sc, PDMA_BASE + PDMA_GLO_CFG, 0);
	RT_WRITE(sc, PDMA_BASE + PDMA_RST_IDX, 0xffffffff);

	/* wait while DMA engine is busy */
	for (ntries = 0; ntries < 100; ntries++) {
		tmp = RT_READ(sc, PDMA_BASE + PDMA_GLO_CFG);
		if (!(tmp & (FE_TX_DMA_BUSY | FE_RX_DMA_BUSY)))
			break;
		DELAY(1000);
	}

	if (ntries == 100) {
		device_printf(sc->dev, "timeout waiting for DMA engine\n");
		goto fail;
	}

	/* reset Rx and Tx rings */
	tmp = FE_RST_DRX_IDX0 |
		FE_RST_DTX_IDX3 |
		FE_RST_DTX_IDX2 |
		FE_RST_DTX_IDX1 |
		FE_RST_DTX_IDX0;

	RT_WRITE(sc, PDMA_BASE + PDMA_RST_IDX, tmp);

	/* XXX switch set mac address */
	for (i = 0; i < RT_SOFTC_TX_RING_COUNT; i++)
		rt_reset_tx_ring(sc, &sc->tx_ring[i]);

	for (i = 0; i < RT_SOFTC_TX_RING_COUNT; i++) {
		/* update TX_BASE_PTRx */
		RT_WRITE(sc, PDMA_BASE + TX_BASE_PTR(i),
			sc->tx_ring[i].desc_phys_addr);
		RT_WRITE(sc, PDMA_BASE + TX_MAX_CNT(i),
			RT_SOFTC_TX_RING_DESC_COUNT);
		RT_WRITE(sc, PDMA_BASE + TX_CTX_IDX(i), 0);
	}

	/* init Rx ring */
	rt_reset_rx_ring(sc, &sc->rx_ring);

	/* update RX_BASE_PTR0 */
	RT_WRITE(sc, PDMA_BASE + RX_BASE_PTR0,
		sc->rx_ring.desc_phys_addr);
	RT_WRITE(sc, PDMA_BASE + RX_MAX_CNT0,
		RT_SOFTC_RX_RING_DATA_COUNT);
	RT_WRITE(sc, PDMA_BASE + RX_CALC_IDX0,
		RT_SOFTC_RX_RING_DATA_COUNT - 1);

	/* write back DDONE, 16byte burst enable RX/TX DMA */
	RT_WRITE(sc, PDMA_BASE + PDMA_GLO_CFG,
	    FE_TX_WB_DDONE | FE_DMA_BT_SIZE16 | FE_RX_DMA_EN | FE_TX_DMA_EN);

	/* disable interrupts mitigation */
	RT_WRITE(sc, PDMA_BASE + DELAY_INT_CFG, 0);

	/* clear pending interrupts */
	RT_WRITE(sc, GE_PORT_BASE + FE_INT_STATUS, 0xffffffff);

	/* enable interrupts */
	tmp = 	CNT_PPE_AF |
		CNT_GDM_AF |
		PSE_P2_FC |
		GDM_CRC_DROP |
		PSE_BUF_DROP |
		GDM_OTHER_DROP |
		PSE_P1_FC |
		PSE_P0_FC |
		PSE_FQ_EMPTY |
		INT_TX_COHERENT |
		INT_RX_COHERENT |
		INT_TXQ3_DONE |
		INT_TXQ2_DONE |
		INT_TXQ1_DONE |
		INT_TXQ0_DONE |
		INT_RX_DONE;

	sc->intr_enable_mask = tmp;

	RT_WRITE(sc, GE_PORT_BASE + FE_INT_ENABLE, tmp);

	if (rt_txrx_enable(sc) != 0)
		goto fail;

#ifdef IF_RT_PHY_SUPPORT
	if (mii) mii_mediachg(mii);
#endif /* IF_RT_PHY_SUPPORT */

	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	ifp->if_drv_flags |= IFF_DRV_RUNNING;

	sc->periodic_round = 0;

	callout_reset(&sc->periodic_ch, hz / 10, rt_periodic, sc);

	return;

fail:
	rt_stop_locked(sc);
}

/*
 * rt_init - lock and initialize device.
 */
static void
rt_init(void *priv)
{
	struct rt_softc *sc;

	sc = priv;
	RT_SOFTC_LOCK(sc);
	rt_init_locked(sc);
	RT_SOFTC_UNLOCK(sc);
}

/*
 * rt_stop_locked - stop TX/RX w/ lock
 */
static void
rt_stop_locked(void *priv)
{
	struct rt_softc *sc;
	struct ifnet *ifp;

	sc = priv;
	ifp = sc->ifp;

	RT_DPRINTF(sc, RT_DEBUG_ANY, "stopping\n");

	RT_SOFTC_ASSERT_LOCKED(sc);
	sc->tx_timer = 0;
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);
	callout_stop(&sc->periodic_ch);
	callout_stop(&sc->tx_watchdog_ch);
	RT_SOFTC_UNLOCK(sc);
	taskqueue_block(sc->taskqueue);

	/*
	 * Sometime rt_stop_locked called from isr and we get panic
	 * When found, I fix it
	 */
#ifdef notyet
	taskqueue_drain(sc->taskqueue, &sc->rx_done_task);
	taskqueue_drain(sc->taskqueue, &sc->tx_done_task);
	taskqueue_drain(sc->taskqueue, &sc->periodic_task);
#endif
	RT_SOFTC_LOCK(sc);

	/* disable interrupts */
	RT_WRITE(sc, GE_PORT_BASE + FE_INT_ENABLE, 0);

	/* reset adapter */
	RT_WRITE(sc, GE_PORT_BASE + FE_RST_GLO, PSE_RESET);

	RT_WRITE(sc, GDMA1_BASE + GDMA_FWD_CFG,
	    (
	    GDM_ICS_EN | /* Enable IP Csum */
	    GDM_TCS_EN | /* Enable TCP Csum */
	    GDM_UCS_EN | /* Enable UDP Csum */
	    GDM_STRPCRC | /* Strip CRC from packet */
	    GDM_DST_PORT_CPU << GDM_UFRC_P_SHIFT | /* Forward UCast to CPU */
	    GDM_DST_PORT_CPU << GDM_BFRC_P_SHIFT | /* Forward BCast to CPU */
	    GDM_DST_PORT_CPU << GDM_MFRC_P_SHIFT | /* Forward MCast to CPU */
	    GDM_DST_PORT_CPU << GDM_OFRC_P_SHIFT   /* Forward Other to CPU */
	    ));
}

static void
rt_stop(void *priv)
{
	struct rt_softc *sc;

	sc = priv;
	RT_SOFTC_LOCK(sc);
	rt_stop_locked(sc);
	RT_SOFTC_UNLOCK(sc);
}

/*
 * rt_tx_data - transmit packet.
 */
static int
rt_tx_data(struct rt_softc *sc, struct mbuf *m, int qid)
{
	struct ifnet *ifp;
	struct rt_softc_tx_ring *ring;
	struct rt_softc_tx_data *data;
	struct rt_txdesc *desc;
	struct mbuf *m_d;
	bus_dma_segment_t dma_seg[RT_SOFTC_MAX_SCATTER];
	int error, ndmasegs, ndescs, i;

	KASSERT(qid >= 0 && qid < RT_SOFTC_TX_RING_COUNT,
		("%s: Tx data: invalid qid=%d\n",
		 device_get_nameunit(sc->dev), qid));

	RT_SOFTC_TX_RING_ASSERT_LOCKED(&sc->tx_ring[qid]);

	ifp = sc->ifp;
	ring = &sc->tx_ring[qid];
	desc = &ring->desc[ring->desc_cur];
	data = &ring->data[ring->data_cur];

	error = bus_dmamap_load_mbuf_sg(ring->data_dma_tag, data->dma_map, m,
	    dma_seg, &ndmasegs, 0);
	if (error != 0)	{
		/* too many fragments, linearize */

		RT_DPRINTF(sc, RT_DEBUG_TX,
			"could not load mbuf DMA map, trying to linearize "
			"mbuf: ndmasegs=%d, len=%d, error=%d\n",
			ndmasegs, m->m_pkthdr.len, error);

		m_d = m_collapse(m, M_NOWAIT, 16);
		if (m_d == NULL) {
			m_freem(m);
			m = NULL;
			return (ENOMEM);
		}
		m = m_d;

		sc->tx_defrag_packets++;

		error = bus_dmamap_load_mbuf_sg(ring->data_dma_tag,
		    data->dma_map, m, dma_seg, &ndmasegs, 0);
		if (error != 0)	{
			device_printf(sc->dev, "could not load mbuf DMA map: "
			    "ndmasegs=%d, len=%d, error=%d\n",
			    ndmasegs, m->m_pkthdr.len, error);
			m_freem(m);
			return (error);
		}
	}

	if (m->m_pkthdr.len == 0)
		ndmasegs = 0;

	/* determine how many Tx descs are required */
	ndescs = 1 + ndmasegs / 2;
	if ((ring->desc_queued + ndescs) >
	    (RT_SOFTC_TX_RING_DESC_COUNT - 2)) {
		RT_DPRINTF(sc, RT_DEBUG_TX,
		    "there are not enough Tx descs\n");

		sc->no_tx_desc_avail++;

		bus_dmamap_unload(ring->data_dma_tag, data->dma_map);
		m_freem(m);
		return (EFBIG);
	}

	data->m = m;

	/* set up Tx descs */
	for (i = 0; i < ndmasegs; i += 2) {
		/* Set destenation */
		desc->dst = (TXDSCR_DST_PORT_GDMA1);
		if ((ifp->if_capenable & IFCAP_TXCSUM) != 0)
			desc->dst |= (TXDSCR_IP_CSUM_GEN|TXDSCR_UDP_CSUM_GEN|
			    TXDSCR_TCP_CSUM_GEN);
		/* Set queue id */
		desc->qn = qid;
		/* No PPPoE */
		desc->pppoe = 0;
		/* No VLAN */
		desc->vid = 0;

		desc->sdp0 = htole32(dma_seg[i].ds_addr);
		desc->sdl0 = htole16(dma_seg[i].ds_len |
		    ( ((i+1) == ndmasegs )?RT_TXDESC_SDL0_LASTSEG:0 ));

		if ((i+1) < ndmasegs) {
			desc->sdp1 = htole32(dma_seg[i+1].ds_addr);
			desc->sdl1 = htole16(dma_seg[i+1].ds_len |
			    ( ((i+2) == ndmasegs )?RT_TXDESC_SDL1_LASTSEG:0 ));
		} else {
			desc->sdp1 = 0;
			desc->sdl1 = 0;
		}

		if ((i+2) < ndmasegs) {
			ring->desc_queued++;
			ring->desc_cur = (ring->desc_cur + 1) %
			    RT_SOFTC_TX_RING_DESC_COUNT;
		}
		desc = &ring->desc[ring->desc_cur];
	}

	RT_DPRINTF(sc, RT_DEBUG_TX, "sending data: len=%d, ndmasegs=%d, "
	    "DMA ds_len=%d/%d/%d/%d/%d\n",
	    m->m_pkthdr.len, ndmasegs,
	    (int) dma_seg[0].ds_len,
	    (int) dma_seg[1].ds_len,
	    (int) dma_seg[2].ds_len,
	    (int) dma_seg[3].ds_len,
	    (int) dma_seg[4].ds_len);

	bus_dmamap_sync(ring->seg0_dma_tag, ring->seg0_dma_map,
		BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(ring->data_dma_tag, data->dma_map,
		BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(ring->desc_dma_tag, ring->desc_dma_map,
		BUS_DMASYNC_PREWRITE);

	ring->desc_queued++;
	ring->desc_cur = (ring->desc_cur + 1) % RT_SOFTC_TX_RING_DESC_COUNT;

	ring->data_queued++;
	ring->data_cur = (ring->data_cur + 1) % RT_SOFTC_TX_RING_DATA_COUNT;

	/* kick Tx */
	RT_WRITE(sc, PDMA_BASE + TX_CTX_IDX(qid), ring->desc_cur);

	return (0);
}

/*
 * rt_start - start Transmit/Receive
 */
static void
rt_start(struct ifnet *ifp)
{
	struct rt_softc *sc;
	struct mbuf *m;
	int qid = 0 /* XXX must check QoS priority */;

	sc = ifp->if_softc;

	if (!(ifp->if_drv_flags & IFF_DRV_RUNNING))
		return;

	for (;;) {
		IFQ_DRV_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			break;

		m->m_pkthdr.rcvif = NULL;

		RT_SOFTC_TX_RING_LOCK(&sc->tx_ring[qid]);

		if (sc->tx_ring[qid].data_queued >=
		    RT_SOFTC_TX_RING_DATA_COUNT) {
			RT_SOFTC_TX_RING_UNLOCK(&sc->tx_ring[qid]);

			RT_DPRINTF(sc, RT_DEBUG_TX,
			    "if_start: Tx ring with qid=%d is full\n", qid);

			m_freem(m);

			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
			if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);

			sc->tx_data_queue_full[qid]++;

			break;
		}

		if (rt_tx_data(sc, m, qid) != 0) {
			RT_SOFTC_TX_RING_UNLOCK(&sc->tx_ring[qid]);

			if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);

			break;
		}

		RT_SOFTC_TX_RING_UNLOCK(&sc->tx_ring[qid]);
		sc->tx_timer = RT_TX_WATCHDOG_TIMEOUT;
		callout_reset(&sc->tx_watchdog_ch, hz, rt_tx_watchdog, sc);
	}
}

/*
 * rt_update_promisc - set/clear promiscuous mode. Unused yet, because
 * filtering done by attached Ethernet switch.
 */
static void
rt_update_promisc(struct ifnet *ifp)
{
	struct rt_softc *sc;

	sc = ifp->if_softc;
	printf("%s: %s promiscuous mode\n",
		device_get_nameunit(sc->dev),
		(ifp->if_flags & IFF_PROMISC) ? "entering" : "leaving");
}

/*
 * rt_ioctl - ioctl handler.
 */
static int
rt_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct rt_softc *sc;
	struct ifreq *ifr;
#ifdef IF_RT_PHY_SUPPORT
	struct mii_data *mii;
#endif /* IF_RT_PHY_SUPPORT */
	int error, startall;

	sc = ifp->if_softc;
	ifr = (struct ifreq *) data;

	error = 0;

	switch (cmd) {
	case SIOCSIFFLAGS:
		startall = 0;
		RT_SOFTC_LOCK(sc);
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				if ((ifp->if_flags ^ sc->if_flags) &
				    IFF_PROMISC)
					rt_update_promisc(ifp);
			} else {
				rt_init_locked(sc);
				startall = 1;
			}
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				rt_stop_locked(sc);
		}
		sc->if_flags = ifp->if_flags;
		RT_SOFTC_UNLOCK(sc);
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
#ifdef IF_RT_PHY_SUPPORT
		mii = device_get_softc(sc->rt_miibus);
		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, cmd);
#else
		error = ifmedia_ioctl(ifp, ifr, &sc->rt_ifmedia, cmd);
#endif /* IF_RT_PHY_SUPPORT */
		break;
	default:
		error = ether_ioctl(ifp, cmd, data);
		break;
	}
	return (error);
}

/*
 * rt_periodic - Handler of PERIODIC interrupt
 */
static void
rt_periodic(void *arg)
{
	struct rt_softc *sc;

	sc = arg;
	RT_DPRINTF(sc, RT_DEBUG_PERIODIC, "periodic\n");
	taskqueue_enqueue(sc->taskqueue, &sc->periodic_task);
}

/*
 * rt_tx_watchdog - Handler of TX Watchdog
 */
static void
rt_tx_watchdog(void *arg)
{
	struct rt_softc *sc;
	struct ifnet *ifp;

	sc = arg;
	ifp = sc->ifp;

	if (sc->tx_timer == 0)
		return;

	if (--sc->tx_timer == 0) {
		device_printf(sc->dev, "Tx watchdog timeout: resetting\n");
#ifdef notyet
		/*
		 * XXX: Commented out, because reset break input.
		 */
		rt_stop_locked(sc);
		rt_init_locked(sc);
#endif
		if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
		sc->tx_watchdog_timeouts++;
	}
	callout_reset(&sc->tx_watchdog_ch, hz, rt_tx_watchdog, sc);
}

/*
 * rt_cnt_ppe_af - Handler of PPE Counter Table Almost Full interrupt
 */
static void
rt_cnt_ppe_af(struct rt_softc *sc)
{

	RT_DPRINTF(sc, RT_DEBUG_INTR, "PPE Counter Table Almost Full\n");
}

/*
 * rt_cnt_gdm_af - Handler of GDMA 1 & 2 Counter Table Almost Full interrupt
 */
static void
rt_cnt_gdm_af(struct rt_softc *sc)
{

	RT_DPRINTF(sc, RT_DEBUG_INTR,
	    "GDMA 1 & 2 Counter Table Almost Full\n");
}

/*
 * rt_pse_p2_fc - Handler of PSE port2 (GDMA 2) flow control interrupt
 */
static void
rt_pse_p2_fc(struct rt_softc *sc)
{

	RT_DPRINTF(sc, RT_DEBUG_INTR,
	    "PSE port2 (GDMA 2) flow control asserted.\n");
}

/*
 * rt_gdm_crc_drop - Handler of GDMA 1/2 discard a packet due to CRC error
 * interrupt
 */
static void
rt_gdm_crc_drop(struct rt_softc *sc)
{

	RT_DPRINTF(sc, RT_DEBUG_INTR,
	    "GDMA 1 & 2 discard a packet due to CRC error\n");
}

/*
 * rt_pse_buf_drop - Handler of buffer sharing limitation interrupt
 */
static void
rt_pse_buf_drop(struct rt_softc *sc)
{

	RT_DPRINTF(sc, RT_DEBUG_INTR,
	    "PSE discards a packet due to buffer sharing limitation\n");
}

/*
 * rt_gdm_other_drop - Handler of discard on other reason interrupt
 */
static void
rt_gdm_other_drop(struct rt_softc *sc)
{

	RT_DPRINTF(sc, RT_DEBUG_INTR,
	    "GDMA 1 & 2 discard a packet due to other reason\n");
}

/*
 * rt_pse_p1_fc - Handler of PSE port1 (GDMA 1) flow control interrupt
 */
static void
rt_pse_p1_fc(struct rt_softc *sc)
{

	RT_DPRINTF(sc, RT_DEBUG_INTR,
	    "PSE port1 (GDMA 1) flow control asserted.\n");
}

/*
 * rt_pse_p0_fc - Handler of PSE port0 (CDMA) flow control interrupt
 */
static void
rt_pse_p0_fc(struct rt_softc *sc)
{

	RT_DPRINTF(sc, RT_DEBUG_INTR,
	    "PSE port0 (CDMA) flow control asserted.\n");
}

/*
 * rt_pse_fq_empty - Handler of PSE free Q empty threshold reached interrupt
 */
static void
rt_pse_fq_empty(struct rt_softc *sc)
{

	RT_DPRINTF(sc, RT_DEBUG_INTR,
	    "PSE free Q empty threshold reached & forced drop "
		    "condition occurred.\n");
}

/*
 * rt_intr - main ISR
 */
static void
rt_intr(void *arg)
{
	struct rt_softc *sc;
	struct ifnet *ifp;
	uint32_t status;

	sc = arg;
	ifp = sc->ifp;

	/* acknowledge interrupts */
	status = RT_READ(sc, GE_PORT_BASE + FE_INT_STATUS);
	RT_WRITE(sc, GE_PORT_BASE + FE_INT_STATUS, status);

	RT_DPRINTF(sc, RT_DEBUG_INTR, "interrupt: status=0x%08x\n", status);

	if (status == 0xffffffff ||	/* device likely went away */
		status == 0)		/* not for us */
		return;

	sc->interrupts++;

	if (!(ifp->if_drv_flags & IFF_DRV_RUNNING))
		return;

	if (status & CNT_PPE_AF)
		rt_cnt_ppe_af(sc);

	if (status & CNT_GDM_AF)
		rt_cnt_gdm_af(sc);

	if (status & PSE_P2_FC)
		rt_pse_p2_fc(sc);

	if (status & GDM_CRC_DROP)
		rt_gdm_crc_drop(sc);

	if (status & PSE_BUF_DROP)
		rt_pse_buf_drop(sc);

	if (status & GDM_OTHER_DROP)
		rt_gdm_other_drop(sc);

	if (status & PSE_P1_FC)
		rt_pse_p1_fc(sc);

	if (status & PSE_P0_FC)
		rt_pse_p0_fc(sc);

	if (status & PSE_FQ_EMPTY)
		rt_pse_fq_empty(sc);

	if (status & INT_TX_COHERENT)
		rt_tx_coherent_intr(sc);

	if (status & INT_RX_COHERENT)
		rt_rx_coherent_intr(sc);

	if (status & RX_DLY_INT)
		rt_rx_delay_intr(sc);

	if (status & TX_DLY_INT)
		rt_tx_delay_intr(sc);

	if (status & INT_RX_DONE)
		rt_rx_intr(sc);

	if (status & INT_TXQ3_DONE)
		rt_tx_intr(sc, 3);

	if (status & INT_TXQ2_DONE)
		rt_tx_intr(sc, 2);

	if (status & INT_TXQ1_DONE)
		rt_tx_intr(sc, 1);

	if (status & INT_TXQ0_DONE)
		rt_tx_intr(sc, 0);
}

static void
rt_tx_coherent_intr(struct rt_softc *sc)
{
	uint32_t tmp;
	int i;

	RT_DPRINTF(sc, RT_DEBUG_INTR, "Tx coherent interrupt\n");

	sc->tx_coherent_interrupts++;

	/* restart DMA engine */
	tmp = RT_READ(sc, PDMA_BASE + PDMA_GLO_CFG);
	tmp &= ~(FE_TX_WB_DDONE | FE_TX_DMA_EN);
	RT_WRITE(sc, PDMA_BASE + PDMA_GLO_CFG, tmp);

	for (i = 0; i < RT_SOFTC_TX_RING_COUNT; i++)
		rt_reset_tx_ring(sc, &sc->tx_ring[i]);

	for (i = 0; i < RT_SOFTC_TX_RING_COUNT; i++) {
		RT_WRITE(sc, PDMA_BASE + TX_BASE_PTR(i),
			sc->tx_ring[i].desc_phys_addr);
		RT_WRITE(sc, PDMA_BASE + TX_MAX_CNT(i),
			RT_SOFTC_TX_RING_DESC_COUNT);
		RT_WRITE(sc, PDMA_BASE + TX_CTX_IDX(i), 0);
	}

	rt_txrx_enable(sc);
}

/*
 * rt_rx_coherent_intr
 */
static void
rt_rx_coherent_intr(struct rt_softc *sc)
{
	uint32_t tmp;

	RT_DPRINTF(sc, RT_DEBUG_INTR, "Rx coherent interrupt\n");

	sc->rx_coherent_interrupts++;

	/* restart DMA engine */
	tmp = RT_READ(sc, PDMA_BASE + PDMA_GLO_CFG);
	tmp &= ~(FE_RX_DMA_EN);
	RT_WRITE(sc, PDMA_BASE + PDMA_GLO_CFG, tmp);

	/* init Rx ring */
	rt_reset_rx_ring(sc, &sc->rx_ring);
	RT_WRITE(sc, PDMA_BASE + RX_BASE_PTR0,
		sc->rx_ring.desc_phys_addr);
	RT_WRITE(sc, PDMA_BASE + RX_MAX_CNT0,
		RT_SOFTC_RX_RING_DATA_COUNT);
	RT_WRITE(sc, PDMA_BASE + RX_CALC_IDX0,
		RT_SOFTC_RX_RING_DATA_COUNT - 1);

	rt_txrx_enable(sc);
}

/*
 * rt_rx_intr - a packet received
 */
static void
rt_rx_intr(struct rt_softc *sc)
{

	RT_DPRINTF(sc, RT_DEBUG_INTR, "Rx interrupt\n");
	sc->rx_interrupts++;
	RT_SOFTC_LOCK(sc);

	if (!(sc->intr_disable_mask & INT_RX_DONE)) {
		rt_intr_disable(sc, INT_RX_DONE);
		taskqueue_enqueue(sc->taskqueue, &sc->rx_done_task);
	}

	sc->intr_pending_mask |= INT_RX_DONE;
	RT_SOFTC_UNLOCK(sc);
}

static void
rt_rx_delay_intr(struct rt_softc *sc)
{

	RT_DPRINTF(sc, RT_DEBUG_INTR, "Rx delay interrupt\n");
	sc->rx_delay_interrupts++;
}

static void
rt_tx_delay_intr(struct rt_softc *sc)
{

	RT_DPRINTF(sc, RT_DEBUG_INTR, "Tx delay interrupt\n");
	sc->tx_delay_interrupts++;
}

/*
 * rt_tx_intr - Transsmition of packet done
 */
static void
rt_tx_intr(struct rt_softc *sc, int qid)
{

	KASSERT(qid >= 0 && qid < RT_SOFTC_TX_RING_COUNT,
		("%s: Tx interrupt: invalid qid=%d\n",
		 device_get_nameunit(sc->dev), qid));

	RT_DPRINTF(sc, RT_DEBUG_INTR, "Tx interrupt: qid=%d\n", qid);

	sc->tx_interrupts[qid]++;
	RT_SOFTC_LOCK(sc);

	if (!(sc->intr_disable_mask & (INT_TXQ0_DONE << qid))) {
		rt_intr_disable(sc, (INT_TXQ0_DONE << qid));
		taskqueue_enqueue(sc->taskqueue, &sc->tx_done_task);
	}

	sc->intr_pending_mask |= (INT_TXQ0_DONE << qid);
	RT_SOFTC_UNLOCK(sc);
}

/*
 * rt_rx_done_task - run RX task
 */
static void
rt_rx_done_task(void *context, int pending)
{
	struct rt_softc *sc;
	struct ifnet *ifp;
	int again;

	sc = context;
	ifp = sc->ifp;

	RT_DPRINTF(sc, RT_DEBUG_RX, "Rx done task\n");

	if (!(ifp->if_drv_flags & IFF_DRV_RUNNING))
		return;

	sc->intr_pending_mask &= ~INT_RX_DONE;

	again = rt_rx_eof(sc, sc->rx_process_limit);

	RT_SOFTC_LOCK(sc);

	if ((sc->intr_pending_mask & INT_RX_DONE) || again) {
		RT_DPRINTF(sc, RT_DEBUG_RX,
		    "Rx done task: scheduling again\n");
		taskqueue_enqueue(sc->taskqueue, &sc->rx_done_task);
	} else {
		rt_intr_enable(sc, INT_RX_DONE);
	}

	RT_SOFTC_UNLOCK(sc);
}

/*
 * rt_tx_done_task - check for pending TX task in all queues
 */
static void
rt_tx_done_task(void *context, int pending)
{
	struct rt_softc *sc;
	struct ifnet *ifp;
	uint32_t intr_mask;
	int i;

	sc = context;
	ifp = sc->ifp;

	RT_DPRINTF(sc, RT_DEBUG_TX, "Tx done task\n");

	if (!(ifp->if_drv_flags & IFF_DRV_RUNNING))
		return;

	for (i = RT_SOFTC_TX_RING_COUNT - 1; i >= 0; i--) {
		if (sc->intr_pending_mask & (INT_TXQ0_DONE << i)) {
			sc->intr_pending_mask &= ~(INT_TXQ0_DONE << i);
			rt_tx_eof(sc, &sc->tx_ring[i]);
		}
	}

	sc->tx_timer = 0;

	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	intr_mask = (
		INT_TXQ3_DONE |
		INT_TXQ2_DONE |
		INT_TXQ1_DONE |
		INT_TXQ0_DONE);

	RT_SOFTC_LOCK(sc);

	rt_intr_enable(sc, ~sc->intr_pending_mask &
	    (sc->intr_disable_mask & intr_mask));

	if (sc->intr_pending_mask & intr_mask) {
		RT_DPRINTF(sc, RT_DEBUG_TX,
		    "Tx done task: scheduling again\n");
		taskqueue_enqueue(sc->taskqueue, &sc->tx_done_task);
	}

	RT_SOFTC_UNLOCK(sc);

	if (!IFQ_IS_EMPTY(&ifp->if_snd))
		rt_start(ifp);
}

/*
 * rt_periodic_task - run periodic task
 */
static void
rt_periodic_task(void *context, int pending)
{
	struct rt_softc *sc;
	struct ifnet *ifp;

	sc = context;
	ifp = sc->ifp;

	RT_DPRINTF(sc, RT_DEBUG_PERIODIC, "periodic task: round=%lu\n",
	    sc->periodic_round);

	if (!(ifp->if_drv_flags & IFF_DRV_RUNNING))
		return;

	RT_SOFTC_LOCK(sc);
	sc->periodic_round++;
	rt_update_stats(sc);

	if ((sc->periodic_round % 10) == 0) {
		rt_update_raw_counters(sc);
		rt_watchdog(sc);
	}

	RT_SOFTC_UNLOCK(sc);
	callout_reset(&sc->periodic_ch, hz / 10, rt_periodic, sc);
}

/*
 * rt_rx_eof - check for frames that done by DMA engine and pass it into
 * network subsystem.
 */
static int
rt_rx_eof(struct rt_softc *sc, int limit)
{
	struct ifnet *ifp;
	struct rt_softc_rx_ring *ring;
	struct rt_rxdesc *desc;
	struct rt_softc_rx_data *data;
	struct mbuf *m, *mnew;
	bus_dma_segment_t segs[1];
	bus_dmamap_t dma_map;
	uint32_t index, desc_flags;
	int error, nsegs, len, nframes;

	ifp = sc->ifp;
	ring = &sc->rx_ring;

	nframes = 0;

	while (limit != 0) {
		index = RT_READ(sc, PDMA_BASE + RX_DRX_IDX0);
		if (ring->cur == index)
			break;

		desc = &ring->desc[ring->cur];
		data = &ring->data[ring->cur];

		bus_dmamap_sync(ring->desc_dma_tag, ring->desc_dma_map,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

#ifdef IF_RT_DEBUG
		if ( sc->debug & RT_DEBUG_RX ) {
			printf("\nRX Descriptor[%#08x] dump:\n", (u_int)desc);
		        hexdump(desc, 16, 0, 0);
			printf("-----------------------------------\n");
		}
#endif

		/* XXX Sometime device don`t set DDONE bit */
#ifdef DDONE_FIXED
		if (!(desc->sdl0 & htole16(RT_RXDESC_SDL0_DDONE))) {
			RT_DPRINTF(sc, RT_DEBUG_RX, "DDONE=0, try next\n");
			break;
		}
#endif

		len = le16toh(desc->sdl0) & 0x3fff;
		RT_DPRINTF(sc, RT_DEBUG_RX, "new frame len=%d\n", len);

		nframes++;

		mnew = m_getjcl(M_NOWAIT, MT_DATA, M_PKTHDR,
		    MJUMPAGESIZE);
		if (mnew == NULL) {
			sc->rx_mbuf_alloc_errors++;
			if_inc_counter(ifp, IFCOUNTER_IERRORS, 1);
			goto skip;
		}

		mnew->m_len = mnew->m_pkthdr.len = MJUMPAGESIZE;

		error = bus_dmamap_load_mbuf_sg(ring->data_dma_tag,
		    ring->spare_dma_map, mnew, segs, &nsegs, BUS_DMA_NOWAIT);
		if (error != 0) {
			RT_DPRINTF(sc, RT_DEBUG_RX,
			    "could not load Rx mbuf DMA map: "
			    "error=%d, nsegs=%d\n",
			    error, nsegs);

			m_freem(mnew);

			sc->rx_mbuf_dmamap_errors++;
			if_inc_counter(ifp, IFCOUNTER_IERRORS, 1);

			goto skip;
		}

		KASSERT(nsegs == 1, ("%s: too many DMA segments",
			device_get_nameunit(sc->dev)));

		bus_dmamap_sync(ring->data_dma_tag, data->dma_map,
			BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(ring->data_dma_tag, data->dma_map);

		dma_map = data->dma_map;
		data->dma_map = ring->spare_dma_map;
		ring->spare_dma_map = dma_map;

		bus_dmamap_sync(ring->data_dma_tag, data->dma_map,
			BUS_DMASYNC_PREREAD);

		m = data->m;
		desc_flags = desc->src;

		data->m = mnew;
		/* Add 2 for proper align of RX IP header */
		desc->sdp0 = htole32(segs[0].ds_addr+2);
		desc->sdl0 = htole32(segs[0].ds_len-2);
		desc->src = 0;
		desc->ai = 0;
		desc->foe = 0;

		RT_DPRINTF(sc, RT_DEBUG_RX,
		    "Rx frame: rxdesc flags=0x%08x\n", desc_flags);

		m->m_pkthdr.rcvif = ifp;
		/* Add 2 to fix data align, after sdp0 = addr + 2 */
		m->m_data += 2;
		m->m_pkthdr.len = m->m_len = len;

		/* check for crc errors */
		if ((ifp->if_capenable & IFCAP_RXCSUM) != 0) {
			/*check for valid checksum*/
			if (desc_flags & (RXDSXR_SRC_IP_CSUM_FAIL|
			    RXDSXR_SRC_L4_CSUM_FAIL)) {
				RT_DPRINTF(sc, RT_DEBUG_RX,
				    "rxdesc: crc error\n");

				if_inc_counter(ifp, IFCOUNTER_IERRORS, 1);

				if (!(ifp->if_flags & IFF_PROMISC)) {
				    m_freem(m);
				    goto skip;
				}
			}
			if ((desc_flags & RXDSXR_SRC_IP_CSUM_FAIL) != 0) {
				m->m_pkthdr.csum_flags |= CSUM_IP_CHECKED;
				m->m_pkthdr.csum_flags |= CSUM_IP_VALID;
				m->m_pkthdr.csum_data = 0xffff;
			}
			m->m_flags &= ~M_HASFCS;
		}

		(*ifp->if_input)(ifp, m);
skip:
		desc->sdl0 &= ~htole16(RT_RXDESC_SDL0_DDONE);

		bus_dmamap_sync(ring->desc_dma_tag, ring->desc_dma_map,
			BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

		ring->cur = (ring->cur + 1) % RT_SOFTC_RX_RING_DATA_COUNT;

		limit--;
	}

	if (ring->cur == 0)
		RT_WRITE(sc, PDMA_BASE + RX_CALC_IDX0,
			RT_SOFTC_RX_RING_DATA_COUNT - 1);
	else
		RT_WRITE(sc, PDMA_BASE + RX_CALC_IDX0,
			ring->cur - 1);

	RT_DPRINTF(sc, RT_DEBUG_RX, "Rx eof: nframes=%d\n", nframes);

	sc->rx_packets += nframes;

	return (limit == 0);
}

/*
 * rt_tx_eof - check for successful transmitted frames and mark their
 * descriptor as free.
 */
static void
rt_tx_eof(struct rt_softc *sc, struct rt_softc_tx_ring *ring)
{
	struct ifnet *ifp;
	struct rt_txdesc *desc;
	struct rt_softc_tx_data *data;
	uint32_t index;
	int ndescs, nframes;

	ifp = sc->ifp;

	ndescs = 0;
	nframes = 0;

	for (;;) {
		index = RT_READ(sc, PDMA_BASE + TX_DTX_IDX(ring->qid));
		if (ring->desc_next == index)
			break;

		ndescs++;

		desc = &ring->desc[ring->desc_next];

		bus_dmamap_sync(ring->desc_dma_tag, ring->desc_dma_map,
			BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

		if (desc->sdl0 & htole16(RT_TXDESC_SDL0_LASTSEG) ||
			desc->sdl1 & htole16(RT_TXDESC_SDL1_LASTSEG)) {
			nframes++;

			data = &ring->data[ring->data_next];

			bus_dmamap_sync(ring->data_dma_tag, data->dma_map,
				BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(ring->data_dma_tag, data->dma_map);

			m_freem(data->m);

			data->m = NULL;

			if_inc_counter(ifp, IFCOUNTER_OPACKETS, 1);

			RT_SOFTC_TX_RING_LOCK(ring);
			ring->data_queued--;
			ring->data_next = (ring->data_next + 1) %
			    RT_SOFTC_TX_RING_DATA_COUNT;
			RT_SOFTC_TX_RING_UNLOCK(ring);
		}

		desc->sdl0 &= ~htole16(RT_TXDESC_SDL0_DDONE);

		bus_dmamap_sync(ring->desc_dma_tag, ring->desc_dma_map,
			BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

		RT_SOFTC_TX_RING_LOCK(ring);
		ring->desc_queued--;
		ring->desc_next = (ring->desc_next + 1) %
		    RT_SOFTC_TX_RING_DESC_COUNT;
		RT_SOFTC_TX_RING_UNLOCK(ring);
	}

	RT_DPRINTF(sc, RT_DEBUG_TX,
	    "Tx eof: qid=%d, ndescs=%d, nframes=%d\n", ring->qid, ndescs,
	    nframes);
}

/*
 * rt_update_stats - query statistics counters and update related variables.
 */
static void
rt_update_stats(struct rt_softc *sc)
{
	struct ifnet *ifp;

	ifp = sc->ifp;
	RT_DPRINTF(sc, RT_DEBUG_STATS, "update statistic: \n");
	/* XXX do update stats here */
}

/*
 * rt_watchdog - reinit device on watchdog event.
 */
static void
rt_watchdog(struct rt_softc *sc)
{
	uint32_t tmp;
#ifdef notyet
	int ntries;
#endif

	tmp = RT_READ(sc, PSE_BASE + CDMA_OQ_STA);

	RT_DPRINTF(sc, RT_DEBUG_WATCHDOG, "watchdog: PSE_IQ_STA=0x%08x\n",
	    tmp);

	/* XXX: do not reset */
#ifdef notyet
	if (((tmp >> P0_IQ_PCNT_SHIFT) & 0xff) != 0) {
		sc->tx_queue_not_empty[0]++;

		for (ntries = 0; ntries < 10; ntries++) {
			tmp = RT_READ(sc, PSE_BASE + PSE_IQ_STA);
			if (((tmp >> P0_IQ_PCNT_SHIFT) & 0xff) == 0)
				break;

			DELAY(1);
		}
	}

	if (((tmp >> P1_IQ_PCNT_SHIFT) & 0xff) != 0) {
		sc->tx_queue_not_empty[1]++;

		for (ntries = 0; ntries < 10; ntries++) {
			tmp = RT_READ(sc, PSE_BASE + PSE_IQ_STA);
			if (((tmp >> P1_IQ_PCNT_SHIFT) & 0xff) == 0)
				break;

			DELAY(1);
		}
	}
#endif
}

/*
 * rt_update_raw_counters - update counters.
 */
static void
rt_update_raw_counters(struct rt_softc *sc)
{

	sc->tx_bytes	+= RT_READ(sc, CNTR_BASE + GDMA_TX_GBCNT0);
	sc->tx_packets	+= RT_READ(sc, CNTR_BASE + GDMA_TX_GPCNT0);
	sc->tx_skip	+= RT_READ(sc, CNTR_BASE + GDMA_TX_SKIPCNT0);
	sc->tx_collision+= RT_READ(sc, CNTR_BASE + GDMA_TX_COLCNT0);

	sc->rx_bytes	+= RT_READ(sc, CNTR_BASE + GDMA_RX_GBCNT0);
	sc->rx_packets	+= RT_READ(sc, CNTR_BASE + GDMA_RX_GPCNT0);
	sc->rx_crc_err	+= RT_READ(sc, CNTR_BASE + GDMA_RX_CSUM_ERCNT0);
	sc->rx_short_err+= RT_READ(sc, CNTR_BASE + GDMA_RX_SHORT_ERCNT0);
	sc->rx_long_err	+= RT_READ(sc, CNTR_BASE + GDMA_RX_LONG_ERCNT0);
	sc->rx_phy_err	+= RT_READ(sc, CNTR_BASE + GDMA_RX_FERCNT0);
	sc->rx_fifo_overflows+= RT_READ(sc, CNTR_BASE + GDMA_RX_OERCNT0);
}

static void
rt_intr_enable(struct rt_softc *sc, uint32_t intr_mask)
{
	uint32_t tmp;

	sc->intr_disable_mask &= ~intr_mask;
	tmp = sc->intr_enable_mask & ~sc->intr_disable_mask;
	RT_WRITE(sc, GE_PORT_BASE + FE_INT_ENABLE, tmp);
}

static void
rt_intr_disable(struct rt_softc *sc, uint32_t intr_mask)
{
	uint32_t tmp;

	sc->intr_disable_mask |= intr_mask;
	tmp = sc->intr_enable_mask & ~sc->intr_disable_mask;
	RT_WRITE(sc, GE_PORT_BASE + FE_INT_ENABLE, tmp);
}

/*
 * rt_txrx_enable - enable TX/RX DMA
 */
static int
rt_txrx_enable(struct rt_softc *sc)
{
	struct ifnet *ifp;
	uint32_t tmp;
	int ntries;

	ifp = sc->ifp;

	/* enable Tx/Rx DMA engine */
	for (ntries = 0; ntries < 200; ntries++) {
		tmp = RT_READ(sc, PDMA_BASE + PDMA_GLO_CFG);
		if (!(tmp & (FE_TX_DMA_BUSY | FE_RX_DMA_BUSY)))
			break;

		DELAY(1000);
	}

	if (ntries == 200) {
		device_printf(sc->dev, "timeout waiting for DMA engine\n");
		return (-1);
	}

	DELAY(50);

	tmp |= FE_TX_WB_DDONE |	FE_RX_DMA_EN | FE_TX_DMA_EN;
	RT_WRITE(sc, PDMA_BASE + PDMA_GLO_CFG, tmp);

	/* XXX set Rx filter */
	return (0);
}

/*
 * rt_alloc_rx_ring - allocate RX DMA ring buffer
 */
static int
rt_alloc_rx_ring(struct rt_softc *sc, struct rt_softc_rx_ring *ring)
{
	struct rt_rxdesc *desc;
	struct rt_softc_rx_data *data;
	bus_dma_segment_t segs[1];
	int i, nsegs, error;

	error = bus_dma_tag_create(bus_get_dma_tag(sc->dev), PAGE_SIZE, 0,
		BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
		RT_SOFTC_RX_RING_DATA_COUNT * sizeof(struct rt_rxdesc), 1,
		RT_SOFTC_RX_RING_DATA_COUNT * sizeof(struct rt_rxdesc),
		0, NULL, NULL, &ring->desc_dma_tag);
	if (error != 0)	{
		device_printf(sc->dev,
		    "could not create Rx desc DMA tag\n");
		goto fail;
	}

	error = bus_dmamem_alloc(ring->desc_dma_tag, (void **) &ring->desc,
	    BUS_DMA_NOWAIT | BUS_DMA_ZERO, &ring->desc_dma_map);
	if (error != 0) {
		device_printf(sc->dev,
		    "could not allocate Rx desc DMA memory\n");
		goto fail;
	}

	error = bus_dmamap_load(ring->desc_dma_tag, ring->desc_dma_map,
		ring->desc,
		RT_SOFTC_RX_RING_DATA_COUNT * sizeof(struct rt_rxdesc),
		rt_dma_map_addr, &ring->desc_phys_addr, 0);
	if (error != 0) {
		device_printf(sc->dev, "could not load Rx desc DMA map\n");
		goto fail;
	}

	error = bus_dma_tag_create(bus_get_dma_tag(sc->dev), PAGE_SIZE, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
		MJUMPAGESIZE, 1, MJUMPAGESIZE, 0, NULL, NULL,
		&ring->data_dma_tag);
	if (error != 0)	{
		device_printf(sc->dev,
		    "could not create Rx data DMA tag\n");
		goto fail;
	}

	for (i = 0; i < RT_SOFTC_RX_RING_DATA_COUNT; i++) {
		desc = &ring->desc[i];
		data = &ring->data[i];

		error = bus_dmamap_create(ring->data_dma_tag, 0,
		    &data->dma_map);
		if (error != 0)	{
			device_printf(sc->dev, "could not create Rx data DMA "
			    "map\n");
			goto fail;
		}

		data->m = m_getjcl(M_NOWAIT, MT_DATA, M_PKTHDR,
		    MJUMPAGESIZE);
		if (data->m == NULL) {
			device_printf(sc->dev, "could not allocate Rx mbuf\n");
			error = ENOMEM;
			goto fail;
		}

		data->m->m_len = data->m->m_pkthdr.len = MJUMPAGESIZE;

		error = bus_dmamap_load_mbuf_sg(ring->data_dma_tag,
		    data->dma_map, data->m, segs, &nsegs, BUS_DMA_NOWAIT);
		if (error != 0)	{
			device_printf(sc->dev,
			    "could not load Rx mbuf DMA map\n");
			goto fail;
		}

		KASSERT(nsegs == 1, ("%s: too many DMA segments",
			device_get_nameunit(sc->dev)));

		/* Add 2 for proper align of RX IP header */
		desc->sdp0 = htole32(segs[0].ds_addr+2);
		desc->sdl0 = htole32(segs[0].ds_len-2);
	}

	error = bus_dmamap_create(ring->data_dma_tag, 0,
	    &ring->spare_dma_map);
	if (error != 0) {
		device_printf(sc->dev,
		    "could not create Rx spare DMA map\n");
		goto fail;
	}

	bus_dmamap_sync(ring->desc_dma_tag, ring->desc_dma_map,
		BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	return (0);

fail:
	rt_free_rx_ring(sc, ring);
	return (error);
}

/*
 * rt_reset_rx_ring - reset RX ring buffer
 */
static void
rt_reset_rx_ring(struct rt_softc *sc, struct rt_softc_rx_ring *ring)
{
	struct rt_rxdesc *desc;
	int i;

	for (i = 0; i < RT_SOFTC_RX_RING_DATA_COUNT; i++) {
		desc = &ring->desc[i];
		desc->sdl0 &= ~htole16(RT_RXDESC_SDL0_DDONE);
	}

	bus_dmamap_sync(ring->desc_dma_tag, ring->desc_dma_map,
		BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	ring->cur = 0;
}

/*
 * rt_free_rx_ring - free memory used by RX ring buffer
 */
static void
rt_free_rx_ring(struct rt_softc *sc, struct rt_softc_rx_ring *ring)
{
	struct rt_softc_rx_data *data;
	int i;

	if (ring->desc != NULL) {
		bus_dmamap_sync(ring->desc_dma_tag, ring->desc_dma_map,
			BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(ring->desc_dma_tag, ring->desc_dma_map);
		bus_dmamem_free(ring->desc_dma_tag, ring->desc,
			ring->desc_dma_map);
	}

	if (ring->desc_dma_tag != NULL)
		bus_dma_tag_destroy(ring->desc_dma_tag);

	for (i = 0; i < RT_SOFTC_RX_RING_DATA_COUNT; i++) {
		data = &ring->data[i];

		if (data->m != NULL) {
			bus_dmamap_sync(ring->data_dma_tag, data->dma_map,
				BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(ring->data_dma_tag, data->dma_map);
			m_freem(data->m);
		}

		if (data->dma_map != NULL)
			bus_dmamap_destroy(ring->data_dma_tag, data->dma_map);
	}

	if (ring->spare_dma_map != NULL)
		bus_dmamap_destroy(ring->data_dma_tag, ring->spare_dma_map);

	if (ring->data_dma_tag != NULL)
		bus_dma_tag_destroy(ring->data_dma_tag);
}

/*
 * rt_alloc_tx_ring - allocate TX ring buffer
 */
static int
rt_alloc_tx_ring(struct rt_softc *sc, struct rt_softc_tx_ring *ring, int qid)
{
	struct rt_softc_tx_data *data;
	int error, i;

	mtx_init(&ring->lock, device_get_nameunit(sc->dev), NULL, MTX_DEF);

	error = bus_dma_tag_create(bus_get_dma_tag(sc->dev), PAGE_SIZE, 0,
		BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
		RT_SOFTC_TX_RING_DESC_COUNT * sizeof(struct rt_txdesc), 1,
		RT_SOFTC_TX_RING_DESC_COUNT * sizeof(struct rt_txdesc),
		0, NULL, NULL, &ring->desc_dma_tag);
	if (error != 0) {
		device_printf(sc->dev,
		    "could not create Tx desc DMA tag\n");
		goto fail;
	}

	error = bus_dmamem_alloc(ring->desc_dma_tag, (void **) &ring->desc,
	    BUS_DMA_NOWAIT | BUS_DMA_ZERO, &ring->desc_dma_map);
	if (error != 0)	{
		device_printf(sc->dev,
		    "could not allocate Tx desc DMA memory\n");
		goto fail;
	}

	error = bus_dmamap_load(ring->desc_dma_tag, ring->desc_dma_map,
	    ring->desc,	(RT_SOFTC_TX_RING_DESC_COUNT *
	    sizeof(struct rt_txdesc)), rt_dma_map_addr,
	    &ring->desc_phys_addr, 0);
	if (error != 0) {
		device_printf(sc->dev, "could not load Tx desc DMA map\n");
		goto fail;
	}

	ring->desc_queued = 0;
	ring->desc_cur = 0;
	ring->desc_next = 0;

	error = bus_dma_tag_create(bus_get_dma_tag(sc->dev), PAGE_SIZE, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    RT_SOFTC_TX_RING_DATA_COUNT * RT_TX_DATA_SEG0_SIZE, 1,
	    RT_SOFTC_TX_RING_DATA_COUNT * RT_TX_DATA_SEG0_SIZE,
	    0, NULL, NULL, &ring->seg0_dma_tag);
	if (error != 0) {
		device_printf(sc->dev,
		    "could not create Tx seg0 DMA tag\n");
		goto fail;
	}

	error = bus_dmamem_alloc(ring->seg0_dma_tag, (void **) &ring->seg0,
	    BUS_DMA_NOWAIT | BUS_DMA_ZERO, &ring->seg0_dma_map);
	if (error != 0) {
		device_printf(sc->dev,
		    "could not allocate Tx seg0 DMA memory\n");
		goto fail;
	}

	error = bus_dmamap_load(ring->seg0_dma_tag, ring->seg0_dma_map,
	    ring->seg0,
	    RT_SOFTC_TX_RING_DATA_COUNT * RT_TX_DATA_SEG0_SIZE,
	    rt_dma_map_addr, &ring->seg0_phys_addr, 0);
	if (error != 0) {
		device_printf(sc->dev, "could not load Tx seg0 DMA map\n");
		goto fail;
	}

	error = bus_dma_tag_create(bus_get_dma_tag(sc->dev), PAGE_SIZE, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    MJUMPAGESIZE, RT_SOFTC_MAX_SCATTER, MJUMPAGESIZE, 0, NULL, NULL,
	    &ring->data_dma_tag);
	if (error != 0) {
		device_printf(sc->dev,
		    "could not create Tx data DMA tag\n");
		goto fail;
	}

	for (i = 0; i < RT_SOFTC_TX_RING_DATA_COUNT; i++) {
		data = &ring->data[i];

		error = bus_dmamap_create(ring->data_dma_tag, 0,
		    &data->dma_map);
		if (error != 0) {
			device_printf(sc->dev, "could not create Tx data DMA "
			    "map\n");
			goto fail;
		}
	}

	ring->data_queued = 0;
	ring->data_cur = 0;
	ring->data_next = 0;

	ring->qid = qid;
	return (0);

fail:
	rt_free_tx_ring(sc, ring);
	return (error);
}

/*
 * rt_reset_tx_ring - reset TX ring buffer to empty state
 */
static void
rt_reset_tx_ring(struct rt_softc *sc, struct rt_softc_tx_ring *ring)
{
	struct rt_softc_tx_data *data;
	struct rt_txdesc *desc;
	int i;

	for (i = 0; i < RT_SOFTC_TX_RING_DESC_COUNT; i++) {
		desc = &ring->desc[i];

		desc->sdl0 = 0;
		desc->sdl1 = 0;
	}

	ring->desc_queued = 0;
	ring->desc_cur = 0;
	ring->desc_next = 0;

	bus_dmamap_sync(ring->desc_dma_tag, ring->desc_dma_map,
		BUS_DMASYNC_PREWRITE);

	bus_dmamap_sync(ring->seg0_dma_tag, ring->seg0_dma_map,
		BUS_DMASYNC_PREWRITE);

	for (i = 0; i < RT_SOFTC_TX_RING_DATA_COUNT; i++) {
		data = &ring->data[i];

		if (data->m != NULL) {
			bus_dmamap_sync(ring->data_dma_tag, data->dma_map,
				BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(ring->data_dma_tag, data->dma_map);
			m_freem(data->m);
			data->m = NULL;
		}
	}

	ring->data_queued = 0;
	ring->data_cur = 0;
	ring->data_next = 0;
}

/*
 * rt_free_tx_ring - free RX ring buffer
 */
static void
rt_free_tx_ring(struct rt_softc *sc, struct rt_softc_tx_ring *ring)
{
	struct rt_softc_tx_data *data;
	int i;

	if (ring->desc != NULL) {
		bus_dmamap_sync(ring->desc_dma_tag, ring->desc_dma_map,
			BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(ring->desc_dma_tag, ring->desc_dma_map);
		bus_dmamem_free(ring->desc_dma_tag, ring->desc,
			ring->desc_dma_map);
	}

	if (ring->desc_dma_tag != NULL)
		bus_dma_tag_destroy(ring->desc_dma_tag);

	if (ring->seg0 != NULL) {
		bus_dmamap_sync(ring->seg0_dma_tag, ring->seg0_dma_map,
			BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(ring->seg0_dma_tag, ring->seg0_dma_map);
		bus_dmamem_free(ring->seg0_dma_tag, ring->seg0,
			ring->seg0_dma_map);
	}

	if (ring->seg0_dma_tag != NULL)
		bus_dma_tag_destroy(ring->seg0_dma_tag);

	for (i = 0; i < RT_SOFTC_TX_RING_DATA_COUNT; i++) {
		data = &ring->data[i];

		if (data->m != NULL) {
			bus_dmamap_sync(ring->data_dma_tag, data->dma_map,
				BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(ring->data_dma_tag, data->dma_map);
			m_freem(data->m);
		}

		if (data->dma_map != NULL)
			bus_dmamap_destroy(ring->data_dma_tag, data->dma_map);
	}

	if (ring->data_dma_tag != NULL)
		bus_dma_tag_destroy(ring->data_dma_tag);

	mtx_destroy(&ring->lock);
}

/*
 * rt_dma_map_addr - get address of busdma segment
 */
static void
rt_dma_map_addr(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	if (error != 0)
		return;

	KASSERT(nseg == 1, ("too many DMA segments, %d should be 1", nseg));

	*(bus_addr_t *) arg = segs[0].ds_addr;
}

/*
 * rt_sysctl_attach - attach sysctl nodes for NIC counters.
 */
static void
rt_sysctl_attach(struct rt_softc *sc)
{
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *tree;
	struct sysctl_oid *stats;

	ctx = device_get_sysctl_ctx(sc->dev);
	tree = device_get_sysctl_tree(sc->dev);

	/* statistic counters */
	stats = SYSCTL_ADD_NODE(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "stats", CTLFLAG_RD, 0, "statistic");

	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	    "interrupts", CTLFLAG_RD, &sc->interrupts, 0,
	    "all interrupts");

	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	    "tx_coherent_interrupts", CTLFLAG_RD, &sc->tx_coherent_interrupts,
	    0, "Tx coherent interrupts");

	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	    "rx_coherent_interrupts", CTLFLAG_RD, &sc->rx_coherent_interrupts,
	    0, "Rx coherent interrupts");

	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	    "rx_interrupts", CTLFLAG_RD, &sc->rx_interrupts, 0,
	    "Rx interrupts");

	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	    "rx_delay_interrupts", CTLFLAG_RD, &sc->rx_delay_interrupts, 0,
	    "Rx delay interrupts");

	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	    "TXQ3_interrupts", CTLFLAG_RD, &sc->tx_interrupts[3], 0,
	    "Tx AC3 interrupts");

	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	    "TXQ2_interrupts", CTLFLAG_RD, &sc->tx_interrupts[2], 0,
	    "Tx AC2 interrupts");

	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	    "TXQ1_interrupts", CTLFLAG_RD, &sc->tx_interrupts[1], 0,
	    "Tx AC1 interrupts");

	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	    "TXQ0_interrupts", CTLFLAG_RD, &sc->tx_interrupts[0], 0,
	    "Tx AC0 interrupts");

	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	    "tx_delay_interrupts", CTLFLAG_RD, &sc->tx_delay_interrupts,
	    0, "Tx delay interrupts");

	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	    "TXQ3_desc_queued", CTLFLAG_RD, &sc->tx_ring[3].desc_queued,
	    0, "Tx AC3 descriptors queued");

	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	    "TXQ3_data_queued", CTLFLAG_RD, &sc->tx_ring[3].data_queued,
	    0, "Tx AC3 data queued");

	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	    "TXQ2_desc_queued", CTLFLAG_RD, &sc->tx_ring[2].desc_queued,
	    0, "Tx AC2 descriptors queued");

	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	    "TXQ2_data_queued", CTLFLAG_RD, &sc->tx_ring[2].data_queued,
	    0, "Tx AC2 data queued");

	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	    "TXQ1_desc_queued", CTLFLAG_RD, &sc->tx_ring[1].desc_queued,
	    0, "Tx AC1 descriptors queued");

	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	    "TXQ1_data_queued", CTLFLAG_RD, &sc->tx_ring[1].data_queued,
	    0, "Tx AC1 data queued");

	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	    "TXQ0_desc_queued", CTLFLAG_RD, &sc->tx_ring[0].desc_queued,
	    0, "Tx AC0 descriptors queued");

	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	    "TXQ0_data_queued", CTLFLAG_RD, &sc->tx_ring[0].data_queued,
	    0, "Tx AC0 data queued");

	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	    "TXQ3_data_queue_full", CTLFLAG_RD, &sc->tx_data_queue_full[3],
	    0, "Tx AC3 data queue full");

	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	    "TXQ2_data_queue_full", CTLFLAG_RD, &sc->tx_data_queue_full[2],
	    0, "Tx AC2 data queue full");

	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	    "TXQ1_data_queue_full", CTLFLAG_RD, &sc->tx_data_queue_full[1],
	    0, "Tx AC1 data queue full");

	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	    "TXQ0_data_queue_full", CTLFLAG_RD, &sc->tx_data_queue_full[0],
	    0, "Tx AC0 data queue full");

	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	    "tx_watchdog_timeouts", CTLFLAG_RD, &sc->tx_watchdog_timeouts,
	    0, "Tx watchdog timeouts");

	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	    "tx_defrag_packets", CTLFLAG_RD, &sc->tx_defrag_packets, 0,
	    "Tx defragmented packets");

	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	    "no_tx_desc_avail", CTLFLAG_RD, &sc->no_tx_desc_avail, 0,
	    "no Tx descriptors available");

	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	    "rx_mbuf_alloc_errors", CTLFLAG_RD, &sc->rx_mbuf_alloc_errors,
	    0, "Rx mbuf allocation errors");

	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	    "rx_mbuf_dmamap_errors", CTLFLAG_RD, &sc->rx_mbuf_dmamap_errors,
	    0, "Rx mbuf DMA mapping errors");

	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	    "tx_queue_0_not_empty", CTLFLAG_RD, &sc->tx_queue_not_empty[0],
	    0, "Tx queue 0 not empty");

	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	    "tx_queue_1_not_empty", CTLFLAG_RD, &sc->tx_queue_not_empty[1],
	    0, "Tx queue 1 not empty");

	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	    "rx_packets", CTLFLAG_RD, &sc->rx_packets, 0,
	    "Rx packets");

	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	    "rx_crc_errors", CTLFLAG_RD, &sc->rx_crc_err, 0,
	    "Rx CRC errors");

	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	    "rx_phy_errors", CTLFLAG_RD, &sc->rx_phy_err, 0,
	    "Rx PHY errors");

	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	    "rx_dup_packets", CTLFLAG_RD, &sc->rx_dup_packets, 0,
	    "Rx duplicate packets");

	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	    "rx_fifo_overflows", CTLFLAG_RD, &sc->rx_fifo_overflows, 0,
	    "Rx FIFO overflows");

	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	    "rx_bytes", CTLFLAG_RD, &sc->rx_bytes, 0,
	    "Rx bytes");

	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	    "rx_long_err", CTLFLAG_RD, &sc->rx_long_err, 0,
	    "Rx too long frame errors");

	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	    "rx_short_err", CTLFLAG_RD, &sc->rx_short_err, 0,
	    "Rx too short frame errors");

	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	    "tx_bytes", CTLFLAG_RD, &sc->tx_bytes, 0,
	    "Tx bytes");
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	    "tx_packets", CTLFLAG_RD, &sc->tx_packets, 0,
	    "Tx packets");
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	    "tx_skip", CTLFLAG_RD, &sc->tx_skip, 0,
	    "Tx skip count for GDMA ports");
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	    "tx_collision", CTLFLAG_RD, &sc->tx_collision, 0,
	    "Tx collision count for GDMA ports");
}

#ifdef IF_RT_PHY_SUPPORT
static int
rt_miibus_readreg(device_t dev, int phy, int reg)
{
	struct rt_softc *sc = device_get_softc(dev);

	/*
	 * PSEUDO_PHYAD is a special value for indicate switch attached.
	 * No one PHY use PSEUDO_PHYAD (0x1e) address.
	 */
	if (phy == 31) {
		/* Fake PHY ID for bfeswitch attach */
		switch (reg) {
		case MII_BMSR:
			return (BMSR_EXTSTAT|BMSR_MEDIAMASK);
		case MII_PHYIDR1:
			return (0x40);		/* As result of faking */
		case MII_PHYIDR2:		/* PHY will detect as */
			return (0x6250);		/* bfeswitch */
		}
	}

	/* Wait prev command done if any */
	while (RT_READ(sc, MDIO_ACCESS) & MDIO_CMD_ONGO);
	RT_WRITE(sc, MDIO_ACCESS,
	    MDIO_CMD_ONGO ||
	    ((phy << MDIO_PHY_ADDR_SHIFT) & MDIO_PHY_ADDR_MASK) ||
	    ((reg << MDIO_PHYREG_ADDR_SHIFT) & MDIO_PHYREG_ADDR_MASK));
	while (RT_READ(sc, MDIO_ACCESS) & MDIO_CMD_ONGO);

	return (RT_READ(sc, MDIO_ACCESS) & MDIO_PHY_DATA_MASK);
}

static int
rt_miibus_writereg(device_t dev, int phy, int reg, int val)
{
	struct rt_softc *sc = device_get_softc(dev);

	/* Wait prev command done if any */
	while (RT_READ(sc, MDIO_ACCESS) & MDIO_CMD_ONGO);
	RT_WRITE(sc, MDIO_ACCESS,
	    MDIO_CMD_ONGO || MDIO_CMD_WR ||
	    ((phy << MDIO_PHY_ADDR_SHIFT) & MDIO_PHY_ADDR_MASK) ||
	    ((reg << MDIO_PHYREG_ADDR_SHIFT) & MDIO_PHYREG_ADDR_MASK) ||
	    (val & MDIO_PHY_DATA_MASK));
	while (RT_READ(sc, MDIO_ACCESS) & MDIO_CMD_ONGO);

	return (0);
}

void
rt_miibus_statchg(device_t dev)
{
	struct rt_softc *sc = device_get_softc(dev);
	struct mii_data *mii;

	mii = device_get_softc(sc->rt_miibus);

	if ((mii->mii_media_status & (IFM_ACTIVE | IFM_AVALID)) ==
	    (IFM_ACTIVE | IFM_AVALID)) {
		switch (IFM_SUBTYPE(mii->mii_media_active)) {
		case IFM_10_T:
		case IFM_100_TX:
			/* XXX check link here */
			sc->flags |= 1;
			break;
		default:
			break;
		}
	}
}
#endif /* IF_RT_PHY_SUPPORT */

static device_method_t rt_dev_methods[] =
{
	DEVMETHOD(device_probe, rt_probe),
	DEVMETHOD(device_attach, rt_attach),
	DEVMETHOD(device_detach, rt_detach),
	DEVMETHOD(device_shutdown, rt_shutdown),
	DEVMETHOD(device_suspend, rt_suspend),
	DEVMETHOD(device_resume, rt_resume),

#ifdef IF_RT_PHY_SUPPORT
	/* MII interface */
	DEVMETHOD(miibus_readreg,	rt_miibus_readreg),
	DEVMETHOD(miibus_writereg,	rt_miibus_writereg),
	DEVMETHOD(miibus_statchg,	rt_miibus_statchg),
#endif

	DEVMETHOD_END
};

static driver_t rt_driver =
{
	"rt",
	rt_dev_methods,
	sizeof(struct rt_softc)
};

static devclass_t rt_dev_class;

DRIVER_MODULE(rt, nexus, rt_driver, rt_dev_class, 0, 0);
MODULE_DEPEND(rt, ether, 1, 1, 1);
MODULE_DEPEND(rt, miibus, 1, 1, 1);

