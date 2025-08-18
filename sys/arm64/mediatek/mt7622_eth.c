#include "if_mt7622var.h"
#include <sys/kenv.h>
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
#include <machine/cpufunc.h>
#include <machine/resource.h>
#include <vm/vm_param.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/pmap.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include "opt_platform.h"

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mdio/mdio.h>
#include <dev/etherswitch/miiproxy.h>
#include "mdio_if.h"
#include "miibus_if.h"

static struct ofw_compat_data compat_data[] = {
        {"mediatek,mt7622-eth", 1}
        {NULL,                  0}
};

static int
mt7622_eth_probe(device_t dev)
{
    if (!ofw_bus_search_compatible(dev, compat_data)->ocd_data)
        return (ENXIO);

    device_printf(dev, "%s\n", __func__ );
    device_set_desc(dev, "Mediatek 7622 ethernet driver");
    return (BUS_PROBE_DEFAULT);
}

static int
mt7622_eth_attach(device_t dev)
{
    struct rt_softc *sc;
    struct ifnet *ifp;
    int error, i;
    phandle_t node;
	char fdtval[32];

    device_printf(dev, "%s\n", __func__ );
    sc = device_get_softc(dev);
    sc->dev = dev;
    node = ofw_bus_get_node(sc->dev);

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

    /* Reset hardware */
    //reset_freng(sc);

    if (sc->rt_chipid == RT_CHIPID_MT7620) {
        sc->csum_fail_ip = MT7620_RXD_SRC_IP_CSUM_FAIL;
        sc->csum_fail_l4 = MT7620_RXD_SRC_L4_CSUM_FAIL;
    } else if (sc->rt_chipid == RT_CHIPID_MT7621) {
        sc->csum_fail_ip = MT7621_RXD_SRC_IP_CSUM_FAIL;
        sc->csum_fail_l4 = MT7621_RXD_SRC_L4_CSUM_FAIL;
    } else {
        sc->csum_fail_ip = RT305X_RXD_SRC_IP_CSUM_FAIL;
        sc->csum_fail_l4 = RT305X_RXD_SRC_L4_CSUM_FAIL;
    }

    /* Fill in soc-specific registers map */
    sc->gdma1_base = MT7620_GDMA1_BASE;

    if (sc->gdma1_base != 0)
        RT_WRITE(sc, sc->gdma1_base + GDMA_FWD_CFG,
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
                           NULL, (sc->rt_chipid == RT_CHIPID_RT5350 ||
                                  sc->rt_chipid == RT_CHIPID_MT7620 ||
                                  sc->rt_chipid == RT_CHIPID_MT7621) ? rt_rt5350_intr : rt_intr,
                           sc, &sc->irqh);
    if (error != 0) {
        printf("%s: could not set up interrupt\n",
               device_get_nameunit(dev));
        goto fail;
    }

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

static device_method_t mt7622_eth_dev_methods[] ={
        DEVMETHOD(device_probe, mt7622_eth_probe),
        DEVMETHOD(device_attach, mt7622_eth_attach),
      /**  DEVMETHOD(device_detach, mt7622_eth_detach),
        DEVMETHOD(device_shutdown, mt7622_eth_shutdown),
        DEVMETHOD(device_suspend, mt7622_eth_suspend),
        DEVMETHOD(device_resume, mt7622_eth_resume),*/

        /* MII interface */
	    /*DEVMETHOD(miibus_readreg,	rt_miibus_readreg),
	    DEVMETHOD(miibus_writereg,	rt_miibus_writereg),
	    DEVMETHOD(miibus_statchg,	rt_miibus_statchg),*/
        DEVMETHOD_END
};

static DEFINE_CLASS_0(mt7622_eth, mt7622_eth_driver, mt7622_eth_dev_methods,
sizeof(struct rt_softc));
MODULE_DEPEND(mt7622_eth, ether, 1, 1, 1);