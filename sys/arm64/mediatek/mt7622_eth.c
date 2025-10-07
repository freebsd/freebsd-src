/*-
* SPDX-License-Identifier: BSD-2-Clause
*
* Copyright (c) 2015-2016, Stanislav Galabov
* Copyright (c) 2014, Aleksandr A. Mityaev
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

#include "opt_platform.h"

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/bus.h>
#include <sys/kenv.h>
#include <sys/rman.h>


#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_param.h>

#include <machine/bus.h>
#include <machine/cpufunc.h>
#include <machine/pmap.h>
#include <machine/resource.h>

#include <net/bpf.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_var.h>
#include <net/if_vlan_var.h>

#include "mt7622_rtreg_eth.h"
#include "mt7622_rtvar.h"

#ifdef FDT
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#endif

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/mdio/mdio.h>
#include <dev/etherswitch/miiproxy.h>
#include "mdio_if.h"

/*
* Defines and macros
*/

#define	RT_TX_WATCHDOG_TIMEOUT		5
#define	MII_BUSY_RETRY			1000

#ifdef FDT
/* more specific and new models should go first */
static const struct ofw_compat_data rt_compat_data[] = {
       { "mediatek,mt7622-eth",	1 },
       { NULL,				0 }
};
#endif

/*
* Static function prototypes
*/
static int	rt_probe(device_t dev);
static int	rt_attach(device_t dev);
static int	rt_detach(device_t dev);
static void	rt_init_locked(void *priv);
static void	rt_init(void *priv);
static void	rt_stop_locked(void *priv);
static void	rt_start(if_t ifp);
static int	rt_ioctl(if_t ifp, u_long cmd, caddr_t data);
static void	rt_periodic(void *arg);
static void	rt_tx_watchdog(void *arg);
static void	rt_rt5350_intr(void *arg);
static void	rt_tx_coherent_intr(struct rt_softc *sc);
static void	rt_rx_coherent_intr(struct rt_softc *sc);
static void	rt_rx_delay_intr(struct rt_softc *sc);
static void	rt_tx_delay_intr(struct rt_softc *sc);
static void	rt_rx_intr(struct rt_softc *sc, int qid);
static void	rt_tx_intr(struct rt_softc *sc, int qid);
static void	rt_rx_done_task(void *context, int pending);
static void	rt_tx_done_task(void *context, int pending);
static void	rt_periodic_task(void *context, int pending);
static int	rt_rx_eof(struct rt_softc *sc,
   struct rt_softc_rx_ring *ring, int limit);
static void	rt_tx_eof(struct rt_softc *sc,
   struct rt_softc_tx_ring *ring);
static void	rt_update_stats(struct rt_softc *sc);
static void	rt_watchdog(struct rt_softc *sc);
static void	rt_update_raw_counters(struct rt_softc *sc);
static void	rt_intr_enable(struct rt_softc *sc, uint32_t intr_mask);
static void	rt_intr_disable(struct rt_softc *sc, uint32_t intr_mask);
static int	rt_txrx_enable(struct rt_softc *sc);
static int	rt_alloc_rx_ring(struct rt_softc *sc,
   struct rt_softc_rx_ring *ring, int qid);
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
static int	rt_ifmedia_upd(if_t );
static void	rt_ifmedia_sts(if_t , struct ifmediareq *);

static SYSCTL_NODE(_hw, OID_AUTO, rt, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
   "RT driver parameters");
#ifdef IF_RT_DEBUG
static int rt_debug = 0;
SYSCTL_INT(_hw_rt, OID_AUTO, debug, CTLFLAG_RWTUN, &rt_debug, 0,
   "RT debug level");
#endif

static int
rt_probe(device_t dev)
{
       char buf[80];
#ifdef FDT
       const struct ofw_compat_data * cd;

       cd = ofw_bus_search_compatible(dev, rt_compat_data);
       if (cd->ocd_data == 0)
	       return (ENXIO);
#endif
       snprintf(buf, sizeof(buf), "Mediatek MT7622 Ethernet driver");

       device_set_desc_copy(dev, buf);
       return (BUS_PROBE_GENERIC);
}

/* TODO Need to do path setup mac1 SGMII and mac2 RGMII */
/* TODO set ETHSYS_SYSCFG0 gmac right mode */

static void
rt_mac_change(struct rt_softc *sc, uint32_t media, int gmac)
{

       uint32_t reg;
       reg = (IPG_CFG_96BIT_WS_IFG << IPG_CFG_SHIFT) |
	   (MAC_RX_PKT_LEN_1536 << MAC_RX_PKT_LEN_SHIFT) |
	   MAC_MODE | FORCE_MODE |
	   MAC_TX_EN | MAC_RX_EN |
	   BKOFF_EN | BACKPR_EN |
	   FORCE_LINK;

       switch (IFM_SUBTYPE(media)) {
       case IFM_10_T:
	       reg |= (FORCE_SPD_10M << FORCE_SPD_SHIFT);
	       break;
       case IFM_100_TX:
	       reg |= (FORCE_SPD_100M << FORCE_SPD_SHIFT);
	       break;
       case IFM_1000_T:
       case IFM_1000_SX:
       case IFM_2500_T:
       case IFM_2500_SX:
	       reg |= (FORCE_SPD_1000M << FORCE_SPD_SHIFT);
	       break;
       default:
	       // sc->link_up = false;
	       return;
       }

       if ((IFM_OPTIONS(media) & IFM_FDX))
	       reg |= FORCE_DPX;
       else
	       reg &= ~FORCE_DPX;

       RT_WRITE(sc, MAC_P_MCR(gmac), reg);

       device_printf(sc->dev, "%s MAC_%iMCR  0x%x\n", __func__,
	   gmac, RT_READ(sc, MAC_P_MCR(gmac)));
}

/*
* ether_request_mac - try to find usable MAC address.
*/
static int
ether_request_mac(device_t dev, uint8_t *eaddr)
{
       uint32_t maclo, machi;

       maclo = 0xf2 | (arc4random() & 0xffff0000);
       machi = arc4random() & 0x0000ffff;

       eaddr[0] = maclo & 0xff;
       eaddr[1] = (maclo >> 8) & 0xff;
       eaddr[2] = (maclo >> 16) & 0xff;
       eaddr[3] = (maclo >> 24) & 0xff;
       eaddr[4] = machi & 0xff;
       eaddr[5] = (machi >> 8) & 0xff;

       return (0);
}

/*
* Set mac addr
*/
static void
rt_mac_addr(struct rt_softc *sc, int gmac)
{

       if_t ifp = sc->ifp;
       const uint8_t *eaddr;
       uint32_t val;

       /* Write our unicast address */
       eaddr = if_getlladdr(ifp);

       val = eaddr[1] | (eaddr[0] << 8);
       RT_WRITE(sc, RT_GDM_MAC_MSB(gmac), val);

       val = eaddr[5] | (eaddr[4] << 8) | (eaddr[3] << 16) |
	   (eaddr[2] << 24);
       RT_WRITE(sc, RT_GDM_MAC_LSB(gmac), val);
}

/*
* Reset hardware
*/
static void
reset_freng(struct rt_softc *sc)
{
       /* XXX hard reset kills everything so skip it ... */
       return;
}

static int
rt_attach(device_t dev)
{
       struct rt_softc *sc;
       if_t ifp;
       int error, i;
       int gmac = 0;

#if 0
#ifdef FDT
       phandle_t node;
#endif
#endif
       sc = device_get_softc(dev);
       sc->dev = dev;

#if 0
#ifdef FDT
       node = ofw_bus_get_node(sc->dev);
#endif
#endif

       mtx_init(&sc->lock, device_get_nameunit(dev), MTX_NETWORK_LOCK,
	   MTX_DEF | MTX_RECURSE);

       sc->mem_rid = 0;
       sc->mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &sc->mem_rid,
	   RF_ACTIVE | RF_SHAREABLE);
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

       /* Reset hardware */
       reset_freng(sc);

       //	sc->csum_fail_ip = MT7620_RXD_SRC_IP_CSUM_FAIL;
       //	sc->csum_fail_l4 = MT7620_RXD_SRC_L4_CSUM_FAIL;
       sc->csum_fail_ip = MT7621_RXD_SRC_IP_CSUM_FAIL;
       sc->csum_fail_l4 = MT7621_RXD_SRC_L4_CSUM_FAIL;

       /* fallthrough */
       device_printf(dev, "MT7622 Ethernet MAC (rev 0x%08x)\n", sc->mac_rev);

       /* RT5350: No GDMA, PSE, CDMA, PPE */
       //	RT_WRITE(sc, GE_PORT_BASE + 0x0C00, // UDPCS, TCPCS, IPCS=1
       //		RT_READ(sc, GE_PORT_BASE + 0x0C00) | (0x7<<16));
       sc->pdma_delay_int_cfg=RT5350_DELAY_INT_CFG;
       sc->pdma_int_status=RT5350_PDMA_INT_STATUS;
       sc->pdma_int_enable=RT5350_PDMA_INT_ENABLE;
       sc->pdma_glo_cfg=RT5350_PDMA_GLO_CFG;
       sc->pdma_rst_idx=RT5350_PDMA_RST_IDX;
       for (i = 0; i < RT_SOFTC_TX_RING_COUNT; i++) {
	       sc->tx_base_ptr[i]=RT5350_TX_BASE_PTR(i);
	       sc->tx_max_cnt[i]=RT5350_TX_MAX_CNT(i);
	       sc->tx_ctx_idx[i]=RT5350_TX_CTX_IDX(i);
	       sc->tx_dtx_idx[i]=RT5350_TX_DTX_IDX(i);
       }
       sc->rx_ring_count=2;
       for (i = 0; i < sc->rx_ring_count; i++) {
	       sc->rx_base_ptr[i]=RT5350_RX_BASE_PTR(i);
	       sc->rx_max_cnt[i]=RT5350_RX_MAX_CNT(i);
	       sc->rx_calc_idx[i]=RT5350_RX_CALC_IDX(i);
	       sc->rx_drx_idx[i]=RT5350_RX_DRX_IDX(i);
       }
       sc->int_rx_done_mask=RT5350_INT_RXQ0_DONE;
       sc->int_tx_done_mask=RT5350_INT_TXQ0_DONE;

#ifdef notyet
       if (gmac != 0)
#endif
	       RT_WRITE(sc, RT_GDM_IG_CTRL(gmac),
		   (
		       GDM_ICS_EN | /* Enable IP Csum */
		       GDM_TCS_EN | /* Enable TCP Csum */
		       GDM_UCS_EN | /* Enable UDP Csum */
		       GDM_STRPCRC | /* Strip CRC from packet */
		       GDM_DST_PORT_CPU << GDM_UFRC_P_SHIFT | /* fwd UCast to CPU*/
		       GDM_DST_PORT_CPU << GDM_BFRC_P_SHIFT | /* fwd BCast to CPU*/
		       GDM_DST_PORT_CPU << GDM_MFRC_P_SHIFT | /* fwd MCast to CPU */
		       GDM_DST_PORT_CPU << GDM_OFRC_P_SHIFT   /* fwd Other to CPU */
		       ));

       rt_mac_change(sc, IFM_ETHER | IFM_1000_T | IFM_FDX, gmac);

       /* Create parent DMA tag. */
       error = bus_dma_tag_create(
	   bus_get_dma_tag(sc->dev),	/* parent */
	   1, 0,			/* alignment, boundary */
	   BUS_SPACE_MAXADDR_32BIT,	/* lowaddr */
	   BUS_SPACE_MAXADDR,		/* highaddr */
	   NULL, NULL,			/* filter, filterarg */
	   BUS_SPACE_MAXSIZE_32BIT,	/* maxsize */
	   0,				/* nsegments */
	   BUS_SPACE_MAXSIZE_32BIT,	/* maxsegsize */
	   0,				/* flags */
	   NULL, NULL,			/* lockfunc, lockarg */
	   &sc->rt_parent_tag);

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
       for (i = 0; i < sc->rx_ring_count; i++) {
	       error = rt_alloc_rx_ring(sc, &sc->rx_ring[i], i);
	       if (error != 0) {
		       device_printf(dev, "could not allocate Rx ring\n");
		       goto fail;
	       }
       }

       callout_init(&sc->periodic_ch, 0);
       callout_init_mtx(&sc->tx_watchdog_ch, &sc->lock, 0);

       ifp = sc->ifp = if_alloc(IFT_ETHER);
       if (ifp == NULL) {
	       device_printf(dev, "could not if_alloc()\n");
	       error = ENOMEM;
	       goto fail;
       }

       if_setsoftc(ifp, sc);
       if_initname(ifp, device_get_name(sc->dev), device_get_unit(sc->dev));
       if_setflags(ifp, IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST);
       if_setinitfn(ifp, rt_init);
       if_setioctlfn(ifp, rt_ioctl);
       if_setstartfn(ifp, rt_start);
       if_setsendqlen(ifp, ifqmaxlen);
       if_setsendqready(ifp);

       // device_printf(sc->dev, "IF_RT_ONLY_MAC\n");
       ifmedia_init(&sc->rt_ifmedia, 0, rt_ifmedia_upd, rt_ifmedia_sts);
       ifmedia_add(&sc->rt_ifmedia, IFM_ETHER | IFM_1000_T | IFM_FDX, 0,
	   NULL);
       ifmedia_set(&sc->rt_ifmedia, IFM_ETHER | IFM_1000_T | IFM_FDX);

       // if (rt_has_switch(dev)) {
       device_t child;
       child = device_add_child(dev, "mdio", DEVICE_UNIT_ANY);
       bus_attach_children(sc->dev);
       bus_attach_children(child);
       // device_printf(dev, "Switch attached.\n");
       //	sc->switch_attached = 1;
       //}

       ether_request_mac(dev, sc->mac_addr);
       if (bootverbose)
	       device_printf(dev, "Ethernet address %6D\n", sc->mac_addr, ":");

       /* Attach ethernet interface */
       ether_ifattach(ifp, sc->mac_addr);
       rt_mac_addr(sc, gmac);

       /*
	* Tell the upper layer(s) we support long frames.
	*/
       // if_sethdrlen(ifp, sizeof(struct ether_vlan_header));
       if_setifheaderlen(ifp, sizeof(struct ether_vlan_header));
       if_setcapabilitiesbit(ifp, IFCAP_VLAN_MTU, 0);
       if_setcapenablebit(ifp, IFCAP_VLAN_MTU, 0);
       if_setcapabilitiesbit(ifp, IFCAP_RXCSUM|IFCAP_TXCSUM, 0);
       if_setcapenablebit(ifp, IFCAP_RXCSUM|IFCAP_TXCSUM, 0);

       /* init task queue */
       NET_TASK_INIT(&sc->rx_done_task, 0, rt_rx_done_task, sc);
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
	   NULL,  rt_rt5350_intr, sc, &sc->irqh);
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

       for (i = 0; i < sc->rx_ring_count; i++)
	       rt_free_rx_ring(sc, &sc->rx_ring[i]);

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
rt_ifmedia_upd(if_t ifp)
{
       struct rt_softc *sc;
       struct ifmedia *ifm;
       struct ifmedia_entry *ife;

       sc = if_getsoftc(ifp);
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
}

/*
* Report current media status.
*/
static void
rt_ifmedia_sts(if_t ifp, struct ifmediareq *ifmr)
{
       /* TODO Uuri MAC_MSR */
       ifmr->ifm_status = IFM_AVALID | IFM_ACTIVE;
       ifmr->ifm_active = IFM_ETHER | IFM_1000_T | IFM_FDX;
}

static int
rt_detach(device_t dev)
{
       struct rt_softc *sc;
       if_t ifp;
       int i;

       sc = device_get_softc(dev);
       ifp = sc->ifp;

       RT_DPRINTF(sc, RT_DEBUG_ANY, "detaching\n");

       RT_SOFTC_LOCK(sc);

       if_setdrvflagbits(ifp, 0, (IFF_DRV_RUNNING | IFF_DRV_OACTIVE));

       callout_stop(&sc->periodic_ch);
       callout_stop(&sc->tx_watchdog_ch);

       taskqueue_drain(sc->taskqueue, &sc->rx_done_task);
       taskqueue_drain(sc->taskqueue, &sc->tx_done_task);
       taskqueue_drain(sc->taskqueue, &sc->periodic_task);

       /* free Tx and Rx rings */
       for (i = 0; i < RT_SOFTC_TX_RING_COUNT; i++)
	       rt_free_tx_ring(sc, &sc->tx_ring[i]);
       for (i = 0; i < sc->rx_ring_count; i++)
	       rt_free_rx_ring(sc, &sc->rx_ring[i]);

       RT_SOFTC_UNLOCK(sc);

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

/*
* rt_init_locked - Run initialization process having locked mtx.
*/
static void
rt_init_locked(void *priv)
{
       struct rt_softc *sc;
       if_t ifp;
       int i, ntries;
       uint32_t tmp;
       int gmac = 0;

       sc = priv;
       ifp = sc->ifp;

       RT_DPRINTF(sc, RT_DEBUG_ANY, "initializing\n");

       RT_SOFTC_ASSERT_LOCKED(sc);

       /* hardware reset */
       //RT_WRITE(sc, GE_PORT_BASE + FE_RST_GLO, PSE_RESET);
       //rt305x_sysctl_set(SYSCTL_RSTCTRL, SYSCTL_RSTCTRL_FRENG);

       /* Fwd to CPU (uni|broad|multi)cast and Unknown */
#ifdef notyet
       if (gmac != 0)
#endif
	       RT_WRITE(sc, RT_GDM_IG_CTRL(gmac),
		   (
		       GDM_ICS_EN | /* Enable IP Csum */
		       GDM_TCS_EN | /* Enable TCP Csum */
		       GDM_UCS_EN | /* Enable UDP Csum */
		       GDM_STRPCRC | /* Strip CRC from packet */
		       GDM_DST_PORT_CPU << GDM_UFRC_P_SHIFT | /* fwd UCast to CPU */
		       GDM_DST_PORT_CPU << GDM_BFRC_P_SHIFT | /* fwd BCast to CPU */
		       GDM_DST_PORT_CPU << GDM_MFRC_P_SHIFT | /* fwd MCast to CPU */
		       GDM_DST_PORT_CPU << GDM_OFRC_P_SHIFT   /* fwd Other to CPU */
		       ));

       /* disable DMA engine */
       RT_WRITE(sc, sc->pdma_glo_cfg, 0);
       RT_WRITE(sc, sc->pdma_rst_idx, 0xffffffff);

       /* wait while DMA engine is busy */
       for (ntries = 0; ntries < 100; ntries++) {
	       tmp = RT_READ(sc, sc->pdma_glo_cfg);
	       if (!(tmp & (FE_TX_DMA_BUSY | FE_RX_DMA_BUSY)))
		       break;
	       DELAY(1000);
       }

       if (ntries == 100) {
	       device_printf(sc->dev, "timeout waiting for DMA engine\n");
	       goto fail;
       }

       /* reset Rx and Tx rings */
       tmp = FE_RST_DRX_IDX1 |
	   FE_RST_DRX_IDX0 |
	   FE_RST_DTX_IDX3 |
	   FE_RST_DTX_IDX2 |
	   FE_RST_DTX_IDX1 |
	   FE_RST_DTX_IDX0;

       RT_WRITE(sc, sc->pdma_rst_idx, tmp);

       /* XXX switch set mac address */
       for (i = 0; i < RT_SOFTC_TX_RING_COUNT; i++)
	       rt_reset_tx_ring(sc, &sc->tx_ring[i]);

       for (i = 0; i < RT_SOFTC_TX_RING_COUNT; i++) {
	       /* update TX_BASE_PTRx */
	       RT_WRITE(sc, sc->tx_base_ptr[i],
		   sc->tx_ring[i].desc_phys_addr);
	       RT_WRITE(sc, sc->tx_max_cnt[i],
		   RT_SOFTC_TX_RING_DESC_COUNT);
	       RT_WRITE(sc, sc->tx_ctx_idx[i], 0);
       }

       /* init Rx ring */
       for (i = 0; i < sc->rx_ring_count; i++)
	       rt_reset_rx_ring(sc, &sc->rx_ring[i]);

       /* update RX_BASE_PTRx */
       for (i = 0; i < sc->rx_ring_count; i++) {
	       RT_WRITE(sc, sc->rx_base_ptr[i],
		   sc->rx_ring[i].desc_phys_addr);
	       RT_WRITE(sc, sc->rx_max_cnt[i],
		   RT_SOFTC_RX_RING_DATA_COUNT);
	       RT_WRITE(sc, sc->rx_calc_idx[i],
		   RT_SOFTC_RX_RING_DATA_COUNT - 1);
       }

       /* write back DDONE, 16byte burst enable RX/TX DMA */
       tmp = FE_TX_WB_DDONE | FE_DMA_BT_SIZE16 | FE_RX_DMA_EN | FE_TX_DMA_EN;
       tmp |= FE_RX_2B_OFFSET;
       RT_WRITE(sc, sc->pdma_glo_cfg, tmp);

       /* disable interrupts mitigation */
       RT_WRITE(sc, sc->pdma_delay_int_cfg, 0);

       /* clear pending interrupts */
       RT_WRITE(sc, sc->pdma_int_status, 0xffffffff);

       /* enable interrupts */
       tmp = RT5350_INT_TX_COHERENT |
	   RT5350_INT_RX_COHERENT |
	   RT5350_INT_TXQ3_DONE |
	   RT5350_INT_TXQ2_DONE |
	   RT5350_INT_TXQ1_DONE |
	   RT5350_INT_TXQ0_DONE |
	   RT5350_INT_RXQ1_DONE |
	   RT5350_INT_RXQ0_DONE;

       sc->intr_enable_mask = tmp;

       RT_WRITE(sc, sc->pdma_int_enable, tmp);

       if (rt_txrx_enable(sc) != 0)
	       goto fail;

       if_setdrvflagbits(ifp, 0, IFF_DRV_OACTIVE);
       if_setdrvflagbits(ifp, IFF_DRV_RUNNING, 0);

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
       if_t ifp;

       sc = priv;
       ifp = sc->ifp;
       int gmac = 0;

       RT_DPRINTF(sc, RT_DEBUG_ANY, "stopping\n");

       RT_SOFTC_ASSERT_LOCKED(sc);
       sc->tx_timer = 0;
       if_setdrvflagbits(ifp, 0, (IFF_DRV_RUNNING | IFF_DRV_OACTIVE));
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
       RT_WRITE(sc, sc->pdma_int_enable, 0);

       /* reset adapter */
       //#  RT_WRITE(sc, GE_PORT_BASE + FE_RST_GLO, PSE_RESET);
#ifdef notyet
       if (gmac != 0)
#endif
	       RT_WRITE(sc, RT_GDM_IG_CTRL(gmac),
		   (
		       GDM_ICS_EN | /* Enable IP Csum */
		       GDM_TCS_EN | /* Enable TCP Csum */
		       GDM_UCS_EN | /* Enable UDP Csum */
		       GDM_STRPCRC | /* Strip CRC from packet */
		       GDM_DST_PORT_CPU << GDM_UFRC_P_SHIFT | /* fwd UCast to CPU */
		       GDM_DST_PORT_CPU << GDM_BFRC_P_SHIFT | /* fwd BCast to CPU */
		       GDM_DST_PORT_CPU << GDM_MFRC_P_SHIFT | /* fwd MCast to CPU */
		       GDM_DST_PORT_CPU << GDM_OFRC_P_SHIFT   /* fwd Other to CPU */
		       ));
}

/*
* rt_tx_data - transmit packet.
*/
static int
rt_tx_data(struct rt_softc *sc, struct mbuf *m, int qid)
{
       // device_printf(sc->dev, "%s\n", __func__);
       if_t ifp;
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
	   dma_seg, &ndmasegs, BUS_DMA_WAITOK);
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
		   data->dma_map, m, dma_seg, &ndmasegs, BUS_DMA_WAITOK);
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
	       /* TODO: this needs to be refined as MT7620 for example has
		* a different word3 layout than RT305x and RT5350 (the last
		* one doesn't use word3 at all). And so does MT7621...
		*/

	       /* Set destination */
	       desc->dst = (TXDSCR_DST_PORT_GDMA1 << 1); /* start at bit one */

	       if ((if_getcapenable(ifp) & IFCAP_TXCSUM) != 0)
		       desc->dst |= (TXDSCR_IP_CSUM_GEN |
			   TXDSCR_UDP_CSUM_GEN | TXDSCR_TCP_CSUM_GEN);
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

       bus_dmamap_sync(ring->data_dma_tag, data->dma_map,
	   BUS_DMASYNC_PREWRITE);
       bus_dmamap_sync(ring->desc_dma_tag, ring->desc_dma_map,
	   BUS_DMASYNC_PREWRITE);

       ring->desc_queued++;
       ring->desc_cur = (ring->desc_cur + 1) % RT_SOFTC_TX_RING_DESC_COUNT;

       ring->data_queued++;
       ring->data_cur = (ring->data_cur + 1) % RT_SOFTC_TX_RING_DATA_COUNT;

       /* kick Tx */
       RT_WRITE(sc, sc->tx_ctx_idx[qid], ring->desc_cur);

       return (0);
}

/*
* rt_start - start Transmit/Receive
*/
static void
rt_start(if_t ifp)
{
       struct rt_softc *sc;
       struct mbuf *m;
       int qid = 0 /* XXX must check QoS priority */;

       sc = if_getsoftc(ifp);

       if (!(if_getdrvflags(ifp) & IFF_DRV_RUNNING))
	       return;

       for (;;) {
	       m = if_dequeue(ifp);
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

		       if_setdrvflagbits(ifp, IFF_DRV_OACTIVE, 0);
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

	       ETHER_BPF_MTAP(ifp, m);
       }
}

/*
* rt_update_promisc - set/clear promiscuous mode. Unused yet, because
* filtering done by attached Ethernet switch.
*/
static void
rt_update_promisc(if_t ifp)
{
       struct rt_softc *sc;

       sc = if_getsoftc(ifp);
       printf("%s: %s promiscuous mode\n",
	   device_get_nameunit(sc->dev),
	   (if_getflags(ifp) & IFF_PROMISC) ? "entering" : "leaving");
}

/*
* rt_ioctl - ioctl handler.
*/
static int
rt_ioctl(if_t ifp, u_long cmd, caddr_t data)
{
       struct rt_softc *sc;
       struct ifreq *ifr;
       int error;

       sc = if_getsoftc(ifp);
       ifr = (struct ifreq *) data;

       error = 0;

       switch (cmd) {
       case SIOCSIFFLAGS:
	       RT_SOFTC_LOCK(sc);
	       if (if_getflags(ifp) & IFF_UP) {
		       if (if_getdrvflags(ifp) & IFF_DRV_RUNNING) {
			       if ((if_getflags(ifp) ^ sc->if_flags) &
				   IFF_PROMISC)
				       rt_update_promisc(ifp);
		       } else {
			       rt_init_locked(sc);
		       }
	       } else {
		       if (if_getdrvflags(ifp) & IFF_DRV_RUNNING)
			       rt_stop_locked(sc);
	       }
	       sc->if_flags = if_getflags(ifp);
	       RT_SOFTC_UNLOCK(sc);
	       break;
       case SIOCGIFMEDIA:
       case SIOCSIFMEDIA:
	       error = ifmedia_ioctl(ifp, ifr, &sc->rt_ifmedia, cmd);
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
       if_t ifp;

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
* rt_rt5350_intr - main ISR for Ralink 5350 SoC
*/
static void
rt_rt5350_intr(void *arg)
{
       struct rt_softc *sc;
       if_t ifp;
       uint32_t status;

       sc = arg;
       ifp = sc->ifp;

       /* acknowledge interrupts */
       status = RT_READ(sc, sc->pdma_int_status);
       RT_WRITE(sc, sc->pdma_int_status, status);

       RT_DPRINTF(sc, RT_DEBUG_INTR, "interrupt: status=0x%08x\n", status);

       if (status == 0xffffffff ||     /* device likely went away */
	   status == 0)            /* not for us */
	       return;

       sc->interrupts++;

       if (!(if_getdrvflags(ifp) & IFF_DRV_RUNNING))
	       return;

       if (status & RT5350_INT_TX_COHERENT)
	       rt_tx_coherent_intr(sc);
       if (status & RT5350_INT_RX_COHERENT)
	       rt_rx_coherent_intr(sc);
       if (status & RT5350_RX_DLY_INT)
	       rt_rx_delay_intr(sc);
       if (status & RT5350_TX_DLY_INT)
	       rt_tx_delay_intr(sc);
       if (status & RT5350_INT_RXQ1_DONE)
	       rt_rx_intr(sc, 1);
       if (status & RT5350_INT_RXQ0_DONE)
	       rt_rx_intr(sc, 0);
       if (status & RT5350_INT_TXQ3_DONE)
	       rt_tx_intr(sc, 3);
       if (status & RT5350_INT_TXQ2_DONE)
	       rt_tx_intr(sc, 2);
       if (status & RT5350_INT_TXQ1_DONE)
	       rt_tx_intr(sc, 1);
       if (status & RT5350_INT_TXQ0_DONE)
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
       tmp = RT_READ(sc, sc->pdma_glo_cfg);
       tmp &= ~(FE_TX_WB_DDONE | FE_TX_DMA_EN);
       RT_WRITE(sc, sc->pdma_glo_cfg, tmp);

       for (i = 0; i < RT_SOFTC_TX_RING_COUNT; i++)
	       rt_reset_tx_ring(sc, &sc->tx_ring[i]);

       for (i = 0; i < RT_SOFTC_TX_RING_COUNT; i++) {
	       RT_WRITE(sc, sc->tx_base_ptr[i],
		   sc->tx_ring[i].desc_phys_addr);
	       RT_WRITE(sc, sc->tx_max_cnt[i],
		   RT_SOFTC_TX_RING_DESC_COUNT);
	       RT_WRITE(sc, sc->tx_ctx_idx[i], 0);
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
       int i;

       RT_DPRINTF(sc, RT_DEBUG_INTR, "Rx coherent interrupt\n");

       sc->rx_coherent_interrupts++;

       /* restart DMA engine */
       tmp = RT_READ(sc, sc->pdma_glo_cfg);
       tmp &= ~(FE_RX_DMA_EN);
       RT_WRITE(sc, sc->pdma_glo_cfg, tmp);

       /* init Rx ring */
       for (i = 0; i < sc->rx_ring_count; i++)
	       rt_reset_rx_ring(sc, &sc->rx_ring[i]);

       for (i = 0; i < sc->rx_ring_count; i++) {
	       RT_WRITE(sc, sc->rx_base_ptr[i],
		   sc->rx_ring[i].desc_phys_addr);
	       RT_WRITE(sc, sc->rx_max_cnt[i],
		   RT_SOFTC_RX_RING_DATA_COUNT);
	       RT_WRITE(sc, sc->rx_calc_idx[i],
		   RT_SOFTC_RX_RING_DATA_COUNT - 1);
       }

       rt_txrx_enable(sc);
}

/*
* rt_rx_intr - a packet received
*/
static void
rt_rx_intr(struct rt_softc *sc, int qid)
{
       KASSERT(qid >= 0 && qid < sc->rx_ring_count,
	   ("%s: Rx interrupt: invalid qid=%d\n",
	       device_get_nameunit(sc->dev), qid));

       RT_DPRINTF(sc, RT_DEBUG_INTR, "Rx interrupt\n");
       sc->rx_interrupts[qid]++;
       RT_SOFTC_LOCK(sc);

       if (!(sc->intr_disable_mask & (sc->int_rx_done_mask << qid))) {
	       rt_intr_disable(sc, (sc->int_rx_done_mask << qid));
	       taskqueue_enqueue(sc->taskqueue, &sc->rx_done_task);
       }

       sc->intr_pending_mask |= (sc->int_rx_done_mask << qid);
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

       if (!(sc->intr_disable_mask & (sc->int_tx_done_mask << qid))) {
	       rt_intr_disable(sc, (sc->int_tx_done_mask << qid));
	       taskqueue_enqueue(sc->taskqueue, &sc->tx_done_task);
       }

       sc->intr_pending_mask |= (sc->int_tx_done_mask << qid);
       RT_SOFTC_UNLOCK(sc);
}

/*
* rt_rx_done_task - run RX task
*/
static void
rt_rx_done_task(void *context, int pending)
{
       struct rt_softc *sc;
       if_t ifp;
       int again;

       sc = context;
       ifp = sc->ifp;

       RT_DPRINTF(sc, RT_DEBUG_RX, "Rx done task\n");

       if (!(if_getdrvflags(ifp) & IFF_DRV_RUNNING))
	       return;

       sc->intr_pending_mask &= ~sc->int_rx_done_mask;

       again = rt_rx_eof(sc, &sc->rx_ring[0], sc->rx_process_limit);

       RT_SOFTC_LOCK(sc);

       if ((sc->intr_pending_mask & sc->int_rx_done_mask) || again) {
	       RT_DPRINTF(sc, RT_DEBUG_RX,
		   "Rx done task: scheduling again\n");
	       taskqueue_enqueue(sc->taskqueue, &sc->rx_done_task);
       } else {
	       rt_intr_enable(sc, sc->int_rx_done_mask);
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
       if_t ifp;
       uint32_t intr_mask;
       int i;

       sc = context;
       ifp = sc->ifp;

       RT_DPRINTF(sc, RT_DEBUG_TX, "Tx done task\n");

       if (!(if_getdrvflags(ifp) & IFF_DRV_RUNNING))
	       return;

       for (i = RT_SOFTC_TX_RING_COUNT - 1; i >= 0; i--) {
	       if (sc->intr_pending_mask & (sc->int_tx_done_mask << i)) {
		       sc->intr_pending_mask &= ~(sc->int_tx_done_mask << i);
		       rt_tx_eof(sc, &sc->tx_ring[i]);
	       }
       }

       sc->tx_timer = 0;

       if_setdrvflagbits(ifp, 0, IFF_DRV_OACTIVE);

       intr_mask = (
	   RT5350_INT_TXQ3_DONE |
	   RT5350_INT_TXQ2_DONE |
	   RT5350_INT_TXQ1_DONE |
	   RT5350_INT_TXQ0_DONE);

       RT_SOFTC_LOCK(sc);

       rt_intr_enable(sc, ~sc->intr_pending_mask &
	       (sc->intr_disable_mask & intr_mask));

       if (sc->intr_pending_mask & intr_mask) {
	       RT_DPRINTF(sc, RT_DEBUG_TX,
		   "Tx done task: scheduling again\n");
	       taskqueue_enqueue(sc->taskqueue, &sc->tx_done_task);
       }

       RT_SOFTC_UNLOCK(sc);

       if (!(if_sendq_empty(ifp)))
	       rt_start(ifp);
}

/*
* rt_periodic_task - run periodic task
*/
static void
rt_periodic_task(void *context, int pending)
{
       struct rt_softc *sc;
       if_t ifp;

       sc = context;
       ifp = sc->ifp;

       RT_DPRINTF(sc, RT_DEBUG_PERIODIC, "periodic task: round=%lu\n",
	   sc->periodic_round);

       if (!(if_getdrvflags(ifp) & IFF_DRV_RUNNING))
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
rt_rx_eof(struct rt_softc *sc, struct rt_softc_rx_ring *ring, int limit)
{
       if_t ifp;
       /*	struct rt_softc_rx_ring *ring; */
       struct rt_rxdesc *desc;
       struct rt_softc_rx_data *data;
       struct mbuf *m, *mnew;
       bus_dma_segment_t segs[1];
       bus_dmamap_t dma_map;
       uint32_t index, desc_flags;
       int error, nsegs, len, nframes;

       ifp = sc->ifp;
       /*	ring = &sc->rx_ring[0]; */

       nframes = 0;

       while (limit != 0) {
	       index = RT_READ(sc, sc->rx_drx_idx[0]);
	       if (ring->cur == index)
		       break;

	       desc = &ring->desc[ring->cur];
	       data = &ring->data[ring->cur];

	       bus_dmamap_sync(ring->desc_dma_tag, ring->desc_dma_map,
		   BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

#ifdef IF_RT_DEBUG
	       if ( sc->debug & RT_DEBUG_RX ) {
		       printf("\nRX Descriptor[%#08lx] dump:\n", (uint64_t)desc);
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

	       mnew = m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR);
	       if (mnew == NULL) {
		       sc->rx_mbuf_alloc_errors++;
		       if_inc_counter(ifp, IFCOUNTER_IERRORS, 1);
		       goto skip;
	       }

	       mnew->m_len = mnew->m_pkthdr.len = MCLBYTES;

	       error = bus_dmamap_load_mbuf_sg(ring->data_dma_tag,
		   ring->spare_dma_map, mnew, segs, &nsegs, BUS_DMA_NOWAIT);
	       if (error != 0) {
		       device_printf(sc->dev, "%s could not load Rx mbuf DMA map:error=%d, nsegs=%d\n",
			   __func__, error, nsegs);
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
	       desc_flags = desc->word3;

	       data->m = mnew;
	       /* Add 2 for proper align of RX IP header */
	       desc->sdp0 = htole32(segs[0].ds_addr+2);
	       desc->sdl0 = htole32(segs[0].ds_len-2);
	       desc->word3 = 0;

	       RT_DPRINTF(sc, RT_DEBUG_RX,
		   "Rx frame: rxdesc flags=0x%08x\n", desc_flags);

	       m->m_pkthdr.rcvif = ifp;
	       /* Add 2 to fix data align, after sdp0 = addr + 2 */
	       m->m_data += 2;
	       m->m_pkthdr.len = m->m_len = len;

	       /* check for crc errors */
	       if ((if_getcapenable(ifp) & IFCAP_RXCSUM) != 0) {
		       /*check for valid checksum*/
		       if (desc_flags & (sc->csum_fail_ip|sc->csum_fail_l4)) {
			       RT_DPRINTF(sc, RT_DEBUG_RX,
				   "rxdesc: crc error\n");

			       if_inc_counter(ifp, IFCOUNTER_IERRORS, 1);

			       if (!(if_getflags(ifp) & IFF_PROMISC)) {
				       m_freem(m);
				       goto skip;
			       }
		       }
		       if ((desc_flags & sc->csum_fail_ip) == 0) {
			       m->m_pkthdr.csum_flags |= CSUM_IP_CHECKED;
			       m->m_pkthdr.csum_flags |= CSUM_IP_VALID;
			       m->m_pkthdr.csum_data = 0xffff;
		       }
	       }

	       if_input(ifp, m);
       skip:
	       desc->sdl0 &= ~htole16(RT_RXDESC_SDL0_DDONE);

	       bus_dmamap_sync(ring->desc_dma_tag, ring->desc_dma_map,
		   BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	       ring->cur = (ring->cur + 1) % RT_SOFTC_RX_RING_DATA_COUNT;

	       limit--;
       }

       if (ring->cur == 0)
	       RT_WRITE(sc, sc->rx_calc_idx[0],
		   RT_SOFTC_RX_RING_DATA_COUNT - 1);
       else
	       RT_WRITE(sc, sc->rx_calc_idx[0],
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
       if_t ifp;
       struct rt_txdesc *desc;
       struct rt_softc_tx_data *data;
       uint32_t index;
       int ndescs; //, nframes;

       ifp = sc->ifp;

       ndescs = 0;
       //nframes = 0;

       bus_dmamap_sync(ring->desc_dma_tag, ring->desc_dma_map,
	   BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

       for (;;) {
	       index = RT_READ(sc, sc->tx_dtx_idx[ring->qid]);
	       if (ring->desc_next == index)
		       break;

	       ndescs++;

	       desc = &ring->desc[ring->desc_next];

	       if (desc->sdl0 & htole16(RT_TXDESC_SDL0_LASTSEG) ||
		   desc->sdl1 & htole16(RT_TXDESC_SDL1_LASTSEG)) {
		       //		nframes++;

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
		       ring->desc_queued--;
		       ring->desc_next = (ring->desc_next + 1) %
			   RT_SOFTC_TX_RING_DESC_COUNT;
		       RT_SOFTC_TX_RING_UNLOCK(ring);
	       } else {
		       RT_SOFTC_TX_RING_LOCK(ring);
		       ring->desc_queued--;
		       ring->desc_next = (ring->desc_next + 1) %
			   RT_SOFTC_TX_RING_DESC_COUNT;
		       RT_SOFTC_TX_RING_UNLOCK(ring);
	       }

	       desc->sdl0 &= ~htole16(RT_TXDESC_SDL0_DDONE);

       }

       if(ndescs)
	       bus_dmamap_sync(ring->desc_dma_tag, ring->desc_dma_map,
		   BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

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
       // if_t ifp;

       // ifp = sc->ifp;
       RT_DPRINTF(sc, RT_DEBUG_STATS, "update statistic: \n");
       /* XXX do update stats here */
}

/*
* rt_watchdog - reinit device on watchdog event.
*/
static void
rt_watchdog(struct rt_softc *sc)
{
       // uint32_t tmp;
       // tmp = RT_READ(sc, PSE_BASE + CDMA_OQ_STA);

       //RT_DPRINTF(sc, RT_DEBUG_WATCHDOG,
       //	   "watchdog: PSE_IQ_STA=0x%08x\n", tmp);
}

/*
* rt_update_raw_counters - update counters.
*/
static void
rt_update_raw_counters(struct rt_softc *sc)
{
       int gmac = 0;
       uint32_t tmp;

       sc->tx_bytes	+= RT_READ(sc, GDM_TX_GBCNT_LSB(gmac));
       tmp = RT_READ(sc,GDM_TX_GBCNT_MSB(gmac));
       if (tmp)
	       sc->tx_bytes    += ((uint64_t) tmp << 32);
       sc->tx_packets	+= RT_READ(sc, GDM_TX_GPCNT(gmac));
       sc->tx_skip	+= RT_READ(sc, GDM_TX_SKIPCNT(gmac));
       sc->tx_collision+= RT_READ(sc, GDM_TX_COLCNT(gmac));

       sc->rx_bytes	+= RT_READ(sc, GDM_RX_GBCNT_LSB(gmac));
       tmp = RT_READ(sc,GDM_RX_GBCNT_MSB(gmac));
       if (tmp)
	       sc->rx_bytes    += ((uint64_t)tmp << 32);
       sc->rx_packets	+= RT_READ(sc, GDM_RX_GPCNT(gmac));
       sc->rx_crc_err	+= RT_READ(sc, GDM_RX_CSUM_ERCNT(gmac));
       sc->rx_short_err+= RT_READ(sc, GDM_RX_SHORT_ERCNT(gmac));
       sc->rx_long_err	+= RT_READ(sc, GDM_RX_LONG_ERCNT(gmac));
       sc->rx_phy_err	+= RT_READ(sc, GDM_RX_FERCNT(gmac));
       sc->rx_fifo_overflows+= RT_READ(sc, GDM_RX_OERCNT(gmac));
}

static void
rt_intr_enable(struct rt_softc *sc, uint32_t intr_mask)
{
       uint32_t tmp;

       sc->intr_disable_mask &= ~intr_mask;
       tmp = sc->intr_enable_mask & ~sc->intr_disable_mask;
       RT_WRITE(sc, sc->pdma_int_enable, tmp);
}

static void
rt_intr_disable(struct rt_softc *sc, uint32_t intr_mask)
{
       uint32_t tmp;

       sc->intr_disable_mask |= intr_mask;
       tmp = sc->intr_enable_mask & ~sc->intr_disable_mask;
       RT_WRITE(sc, sc->pdma_int_enable, tmp);
}

/*
* rt_txrx_enable - enable TX/RX DMA
*/
static int
rt_txrx_enable(struct rt_softc *sc)
{
       struct ifnet; //  *ifp;
       uint32_t tmp;
       int ntries;

       // ifp = sc->ifp;

       /* enable Tx/Rx DMA engine */
       for (ntries = 0; ntries < 200; ntries++) {
	       tmp = RT_READ(sc, sc->pdma_glo_cfg);
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
       RT_WRITE(sc, sc->pdma_glo_cfg, tmp);

       /* XXX set Rx filter */
       return (0);
}

/*
* rt_alloc_rx_ring - allocate RX DMA ring buffer
*/
static int
rt_alloc_rx_ring(struct rt_softc *sc, struct rt_softc_rx_ring *ring, int qid)
{
       // device_printf(sc->dev, "%s\n", __func__);
       struct rt_rxdesc *desc;
       struct rt_softc_rx_data *data;
       bus_dma_segment_t segs[1];
       int i, nsegs, error;

       error = bus_dma_tag_create(sc->rt_parent_tag,
	   sizeof(struct rt_rxdesc), 0,
	   BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL, NULL,
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

       error = bus_dma_tag_create(sc->rt_parent_tag, 1, 0,
	   BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL, NULL,
	   MCLBYTES, RT_SOFTC_MAX_SCATTER, MCLBYTES, 0, NULL, NULL,
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

	       data->m = m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR);
	       if (data->m == NULL) {
		       device_printf(sc->dev, "could not allocate Rx mbuf\n");
		       error = ENOMEM;
		       goto fail;
	       }

	       data->m->m_len = data->m->m_pkthdr.len = MCLBYTES;

	       error = bus_dmamap_load_mbuf_sg(ring->data_dma_tag,
		   data->dma_map, data->m, segs, &nsegs, BUS_DMA_WAITOK);
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
       ring->qid = qid;
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
       device_printf(sc->dev, "%s\n", __func__);
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
       // device_printf(sc->dev, "%s\n", __func__);
       struct rt_softc_tx_data *data;
       int error, i;

       mtx_init(&ring->lock, device_get_nameunit(sc->dev), NULL, MTX_DEF);

       error = bus_dma_tag_create(sc->rt_parent_tag,
	   sizeof(struct rt_txdesc), 0,
	   BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL, NULL,
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

       error = bus_dma_tag_create(sc->rt_parent_tag, 1, 0,
	   BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL, NULL,
	   MCLBYTES * RT_SOFTC_MAX_SCATTER, RT_SOFTC_MAX_SCATTER,
	   MCLBYTES, 0, NULL, NULL,
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
       device_printf(sc->dev, "%s\n", __func__);
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
	   "stats", CTLFLAG_RD | CTLFLAG_MPSAFE, 0, "statistic");

       SYSCTL_ADD_ULONG(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	   "interrupts", CTLFLAG_RD, &sc->interrupts,
	   "all interrupts");

       SYSCTL_ADD_ULONG(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	   "tx_coherent_interrupts", CTLFLAG_RD, &sc->tx_coherent_interrupts,
	   "Tx coherent interrupts");

       SYSCTL_ADD_ULONG(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	   "rx_coherent_interrupts", CTLFLAG_RD, &sc->rx_coherent_interrupts,
	   "Rx coherent interrupts");

       SYSCTL_ADD_ULONG(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	   "rx_interrupts", CTLFLAG_RD, &sc->rx_interrupts[0],
	   "Rx interrupts");

       SYSCTL_ADD_ULONG(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	   "rx_delay_interrupts", CTLFLAG_RD, &sc->rx_delay_interrupts,
	   "Rx delay interrupts");

#if RT_SOFTC_TX_RING_COUNT > 3
       SYSCTL_ADD_ULONG(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	   "TXQ3_interrupts", CTLFLAG_RD, &sc->tx_interrupts[3],
	   "Tx AC3 interrupts");
#endif

#if RT_SOFTC_TX_RING_COUNT > 2
       SYSCTL_ADD_ULONG(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	   "TXQ2_interrupts", CTLFLAG_RD, &sc->tx_interrupts[2],
	   "Tx AC2 interrupts");
#endif

#if RT_SOFTC_TX_RING_COUNT > 1
       SYSCTL_ADD_ULONG(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	   "TXQ1_interrupts", CTLFLAG_RD, &sc->tx_interrupts[1],
	   "Tx AC1 interrupts");
#endif

       SYSCTL_ADD_ULONG(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	   "TXQ0_interrupts", CTLFLAG_RD, &sc->tx_interrupts[0],
	   "Tx AC0 interrupts");

       SYSCTL_ADD_ULONG(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	   "tx_delay_interrupts", CTLFLAG_RD, &sc->tx_delay_interrupts,
	   "Tx delay interrupts");

#if RT_SOFTC_TX_RING_COUNT > 3
       SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	   "TXQ3_desc_queued", CTLFLAG_RD, &sc->tx_ring[3].desc_queued,
	   0, "Tx AC3 descriptors queued");

       SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	   "TXQ3_data_queued", CTLFLAG_RD, &sc->tx_ring[3].data_queued,
	   0, "Tx AC3 data queued");
#endif

#if RT_SOFTC_TX_RING_COUNT > 2
       SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	   "TXQ2_desc_queued", CTLFLAG_RD, &sc->tx_ring[2].desc_queued,
	   0, "Tx AC2 descriptors queued");

       SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	   "TXQ2_data_queued", CTLFLAG_RD, &sc->tx_ring[2].data_queued,
	   0, "Tx AC2 data queued");
#endif

#if RT_SOFTC_TX_RING_COUNT > 1
       SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	   "TXQ1_desc_queued", CTLFLAG_RD, &sc->tx_ring[1].desc_queued,
	   0, "Tx AC1 descriptors queued");

       SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	   "TXQ1_data_queued", CTLFLAG_RD, &sc->tx_ring[1].data_queued,
	   0, "Tx AC1 data queued");
#endif

       SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	   "TXQ0_desc_queued", CTLFLAG_RD, &sc->tx_ring[0].desc_queued,
	   0, "Tx AC0 descriptors queued");

       SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	   "TXQ0_data_queued", CTLFLAG_RD, &sc->tx_ring[0].data_queued,
	   0, "Tx AC0 data queued");

#if RT_SOFTC_TX_RING_COUNT > 3
       SYSCTL_ADD_ULONG(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	   "TXQ3_data_queue_full", CTLFLAG_RD, &sc->tx_data_queue_full[3],
	   "Tx AC3 data queue full");
#endif

#if RT_SOFTC_TX_RING_COUNT > 2
       SYSCTL_ADD_ULONG(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	   "TXQ2_data_queue_full", CTLFLAG_RD, &sc->tx_data_queue_full[2],
	   "Tx AC2 data queue full");
#endif

#if RT_SOFTC_TX_RING_COUNT > 1
       SYSCTL_ADD_ULONG(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	   "TXQ1_data_queue_full", CTLFLAG_RD, &sc->tx_data_queue_full[1],
	   "Tx AC1 data queue full");
#endif

       SYSCTL_ADD_ULONG(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	   "TXQ0_data_queue_full", CTLFLAG_RD, &sc->tx_data_queue_full[0],
	   "Tx AC0 data queue full");

       SYSCTL_ADD_ULONG(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	   "tx_watchdog_timeouts", CTLFLAG_RD, &sc->tx_watchdog_timeouts,
	   "Tx watchdog timeouts");

       SYSCTL_ADD_ULONG(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	   "tx_defrag_packets", CTLFLAG_RD, &sc->tx_defrag_packets,
	   "Tx defragmented packets");

       SYSCTL_ADD_ULONG(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	   "no_tx_desc_avail", CTLFLAG_RD, &sc->no_tx_desc_avail,
	   "no Tx descriptors available");

       SYSCTL_ADD_ULONG(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	   "rx_mbuf_alloc_errors", CTLFLAG_RD, &sc->rx_mbuf_alloc_errors,
	   "Rx mbuf allocation errors");

       SYSCTL_ADD_ULONG(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	   "rx_mbuf_dmamap_errors", CTLFLAG_RD, &sc->rx_mbuf_dmamap_errors,
	   "Rx mbuf DMA mapping errors");

       SYSCTL_ADD_ULONG(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	   "tx_queue_0_not_empty", CTLFLAG_RD, &sc->tx_queue_not_empty[0],
	   "Tx queue 0 not empty");

       SYSCTL_ADD_ULONG(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	   "tx_queue_1_not_empty", CTLFLAG_RD, &sc->tx_queue_not_empty[1],
	   "Tx queue 1 not empty");

       SYSCTL_ADD_ULONG(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	   "rx_packets", CTLFLAG_RD, &sc->rx_packets,
	   "Rx packets");

       SYSCTL_ADD_ULONG(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	   "rx_crc_errors", CTLFLAG_RD, &sc->rx_crc_err,
	   "Rx CRC errors");

       SYSCTL_ADD_ULONG(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	   "rx_phy_errors", CTLFLAG_RD, &sc->rx_phy_err,
	   "Rx PHY errors");

       SYSCTL_ADD_ULONG(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	   "rx_dup_packets", CTLFLAG_RD, &sc->rx_dup_packets,
	   "Rx duplicate packets");

       SYSCTL_ADD_ULONG(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	   "rx_fifo_overflows", CTLFLAG_RD, &sc->rx_fifo_overflows,
	   "Rx FIFO overflows");

       SYSCTL_ADD_QUAD(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	   "rx_bytes", CTLFLAG_RD, &sc->rx_bytes,
	   "Rx bytes");

       SYSCTL_ADD_ULONG(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	   "rx_long_err", CTLFLAG_RD, &sc->rx_long_err,
	   "Rx too long frame errors");

       SYSCTL_ADD_ULONG(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	   "rx_short_err", CTLFLAG_RD, &sc->rx_short_err,
	   "Rx too short frame errors");

       SYSCTL_ADD_ULONG(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	   "tx_bytes", CTLFLAG_RD, &sc->tx_bytes,
	   "Tx bytes");

       SYSCTL_ADD_QUAD(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	   "tx_packets", CTLFLAG_RD, &sc->tx_packets,
	   "Tx packets");

       SYSCTL_ADD_ULONG(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	   "tx_skip", CTLFLAG_RD, &sc->tx_skip,
	   "Tx skip count for GDMA ports");

       SYSCTL_ADD_ULONG(ctx, SYSCTL_CHILDREN(stats), OID_AUTO,
	   "tx_collision", CTLFLAG_RD, &sc->tx_collision,
	   "Tx collision count for GDMA ports");
}

static int
rt_miibus_wait_idle(struct rt_softc *sc)
{
       uint32_t dat;
       int retry;

       for (retry = MII_BUSY_RETRY; retry > 0; retry--) {
	       dat = RT_READ(sc, MDIO_ACCESS);
	       if (!(dat & MDIO_CMD_ONGO))
		       break;
	       DELAY(10);
       }

       return (retry);
}

/*
* PSEUDO_PHYAD is a special value for indicate switch attached.
* No one PHY use PSEUDO_PHYAD (0x1e) address.
*/

static int
rt_mdio_writereg(device_t dev, int phy, int reg, int val)
{
       struct rt_softc *sc = device_get_softc(dev);
       int dat;
       int retry;
       int st = MDIO_ST_C22;
       int cmd = MDIO_CMD_WRITE;

       /* Wait prev command done if any */
       retry = rt_miibus_wait_idle(sc);
       if (!retry) {
	       device_printf(dev, "phy write timeout, phy=%d reg=%d\n",
		   phy, reg);
	       return (ETIMEDOUT);
       }

       dat = (st << MDIO_ST_SHIFT) |
	   ((cmd << MDIO_CMD_SHIFT) & MDIO_CMD_MASK) |
	   ((phy << MDIO_PHY_ADDR_SHIFT) & MDIO_PHY_ADDR_MASK) |
	   ((reg << MDIO_PHYREG_ADDR_SHIFT) & MDIO_PHYREG_ADDR_MASK) |
	   (val & MDIO_PHY_DATA_MASK);

       RT_WRITE(sc, MDIO_ACCESS, dat);
       RT_WRITE(sc, MDIO_ACCESS, dat | MDIO_CMD_ONGO);

       retry = rt_miibus_wait_idle(sc);
       if (!retry) {
	       device_printf(dev, "phy write timeout, phy=%d reg=%d\n",
		   phy, reg);
	       return (ETIMEDOUT);
       }

       return (0);
}

static int
rt_mdio_readreg(device_t dev, int phy, int reg)
{
       struct rt_softc *sc = device_get_softc(dev);
       int dat;
       int retry;
       int st = MDIO_ST_C22;
       int cmd = MDIO_CMD_READ;

       /* Wait prev command done if any */
       retry = rt_miibus_wait_idle(sc);
       if (!retry) {
	       device_printf(dev, "phy write timeout, phy=%d reg=%d\n",
		   phy, reg);
	       return (ETIMEDOUT);
       }

       dat = (st << MDIO_ST_SHIFT) |
	   ((cmd << MDIO_CMD_SHIFT) & MDIO_CMD_MASK) |
	   ((phy << MDIO_PHY_ADDR_SHIFT) & MDIO_PHY_ADDR_MASK) |
	   ((reg << MDIO_PHYREG_ADDR_SHIFT) & MDIO_PHYREG_ADDR_MASK);

       RT_WRITE(sc, MDIO_ACCESS, dat);
       RT_WRITE(sc, MDIO_ACCESS, dat | MDIO_CMD_ONGO);

       retry = rt_miibus_wait_idle(sc);
       if (!retry) {
	       device_printf(dev, "phy write timeout, phy=%d reg=%d\n",
		   phy, reg);
	       return (ETIMEDOUT);
       }

       return (RT_READ(sc, MDIO_ACCESS) & MDIO_PHY_DATA_MASK);
}

#if 0
static boolean_t
rt_has_switch(device_t dev)
{
#ifdef FDT
       phandle_t node;
       node = ofw_bus_get_node(dev);
       return (fdt_find_ethernet_prop_switch(node, OF_finddevice("/")));
#endif
       return (false);
}
#endif

static device_method_t rt_dev_methods[] =
   {
	   DEVMETHOD(device_probe, rt_probe),
	   DEVMETHOD(device_attach, rt_attach),
	   DEVMETHOD(device_detach, rt_detach),

	   /* MDIO interface */
	   DEVMETHOD(mdio_readreg,		rt_mdio_readreg),
	   DEVMETHOD(mdio_writereg,	rt_mdio_writereg),

	   DEVMETHOD_END
   };

static driver_t rt_driver =
   {
	   "rt",
	   rt_dev_methods,
	   sizeof(struct rt_softc)
   };

//DRIVER_MODULE(rt, nexus, rt_driver, 0, 0);
DRIVER_MODULE(miibus, rt, miibus_driver, 0, 0);
DRIVER_MODULE(mdio, rt, mdio_driver, 0, 0);

#ifdef FDT
DRIVER_MODULE(rt, simplebus, rt_driver, 0, 0);
#endif

MODULE_DEPEND(rt, ether, 1, 1, 1);
MODULE_DEPEND(rt, miibus, 1, 1, 1);
MODULE_DEPEND(rt, mdio, 1, 1, 1);

