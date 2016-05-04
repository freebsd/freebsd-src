/*-
 * Copyright (c) 2009 Yohanes Nugroho <yohanes@gmail.com>
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_var.h>
#include <net/if_vlan_var.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#endif

#include <net/bpf.h>
#include <net/bpfdesc.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <arm/cavium/cns11xx/if_ecereg.h>
#include <arm/cavium/cns11xx/if_ecevar.h>
#include <arm/cavium/cns11xx/econa_var.h>

#include <machine/bus.h>
#include <machine/intr.h>

/* "device miibus" required.  See GENERIC if you get errors here. */
#include "miibus_if.h"

static uint8_t
vlan0_mac[ETHER_ADDR_LEN] = {0x00, 0xaa, 0xbb, 0xcc, 0xdd, 0x19};

/*
 * Boot loader expects the hardware state to be the same when we
 * restart the device (warm boot), so we need to save the initial
 * config values.
 */
int initial_switch_config;
int initial_cpu_config;
int initial_port0_config;
int initial_port1_config;

static inline uint32_t
read_4(struct ece_softc *sc, bus_size_t off)
{

	return (bus_read_4(sc->mem_res, off));
}

static inline void
write_4(struct ece_softc *sc, bus_size_t off, uint32_t val)
{

	bus_write_4(sc->mem_res, off, val);
}

#define	ECE_LOCK(_sc)		mtx_lock(&(_sc)->sc_mtx)
#define	ECE_UNLOCK(_sc)		mtx_unlock(&(_sc)->sc_mtx)
#define	ECE_LOCK_INIT(_sc) \
	mtx_init(&_sc->sc_mtx, device_get_nameunit(_sc->dev),	\
		 MTX_NETWORK_LOCK, MTX_DEF)

#define	ECE_TXLOCK(_sc)		mtx_lock(&(_sc)->sc_mtx_tx)
#define	ECE_TXUNLOCK(_sc)		mtx_unlock(&(_sc)->sc_mtx_tx)
#define	ECE_TXLOCK_INIT(_sc) \
	mtx_init(&_sc->sc_mtx_tx, device_get_nameunit(_sc->dev),	\
		 "ECE TX Lock", MTX_DEF)

#define	ECE_CLEANUPLOCK(_sc)	mtx_lock(&(_sc)->sc_mtx_cleanup)
#define	ECE_CLEANUPUNLOCK(_sc)	mtx_unlock(&(_sc)->sc_mtx_cleanup)
#define	ECE_CLEANUPLOCK_INIT(_sc) \
	mtx_init(&_sc->sc_mtx_cleanup, device_get_nameunit(_sc->dev),	\
		 "ECE cleanup Lock", MTX_DEF)

#define	ECE_RXLOCK(_sc)		mtx_lock(&(_sc)->sc_mtx_rx)
#define	ECE_RXUNLOCK(_sc)		mtx_unlock(&(_sc)->sc_mtx_rx)
#define	ECE_RXLOCK_INIT(_sc) \
	mtx_init(&_sc->sc_mtx_rx, device_get_nameunit(_sc->dev),	\
		 "ECE RX Lock", MTX_DEF)

#define	ECE_LOCK_DESTROY(_sc)	mtx_destroy(&_sc->sc_mtx);
#define	ECE_TXLOCK_DESTROY(_sc)	mtx_destroy(&_sc->sc_mtx_tx);
#define	ECE_RXLOCK_DESTROY(_sc)	mtx_destroy(&_sc->sc_mtx_rx);
#define	ECE_CLEANUPLOCK_DESTROY(_sc)	\
	mtx_destroy(&_sc->sc_mtx_cleanup);

#define	ECE_ASSERT_LOCKED(_sc)	mtx_assert(&_sc->sc_mtx, MA_OWNED);
#define	ECE_ASSERT_UNLOCKED(_sc) mtx_assert(&_sc->sc_mtx, MA_NOTOWNED);

static devclass_t ece_devclass;

/* ifnet entry points */

static void	eceinit_locked(void *);
static void	ecestart_locked(struct ifnet *);

static void	eceinit(void *);
static void	ecestart(struct ifnet *);
static void	ecestop(struct ece_softc *);
static int	eceioctl(struct ifnet * ifp, u_long, caddr_t);

/* bus entry points */

static int	ece_probe(device_t dev);
static int	ece_attach(device_t dev);
static int	ece_detach(device_t dev);
static void	ece_intr(void *);
static void	ece_intr_qf(void *);
static void	ece_intr_status(void *xsc);

/* helper routines */
static int	ece_activate(device_t dev);
static void	ece_deactivate(device_t dev);
static int	ece_ifmedia_upd(struct ifnet *ifp);
static void	ece_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr);
static int	ece_get_mac(struct ece_softc *sc, u_char *eaddr);
static void	ece_set_mac(struct ece_softc *sc, u_char *eaddr);
static int	configure_cpu_port(struct ece_softc *sc);
static int	configure_lan_port(struct ece_softc *sc, int phy_type);
static void	set_pvid(struct ece_softc *sc, int port0, int port1, int cpu);
static void	set_vlan_vid(struct ece_softc *sc, int vlan);
static void	set_vlan_member(struct ece_softc *sc, int vlan);
static void	set_vlan_tag(struct ece_softc *sc, int vlan);
static int	hardware_init(struct ece_softc *sc);
static void	ece_intr_rx_locked(struct ece_softc *sc, int count);

static void	ece_free_desc_dma_tx(struct ece_softc *sc);
static void	ece_free_desc_dma_rx(struct ece_softc *sc);

static void	ece_intr_task(void *arg, int pending __unused);
static void	ece_tx_task(void *arg, int pending __unused);
static void	ece_cleanup_task(void *arg, int pending __unused);

static int	ece_allocate_dma(struct ece_softc *sc);

static void	ece_intr_tx(void *xsc);

static void	clear_mac_entries(struct ece_softc *ec, int include_this_mac);

static uint32_t read_mac_entry(struct ece_softc *ec,
	    uint8_t *mac_result,
	    int first);

/*PHY related functions*/
static inline int
phy_read(struct ece_softc *sc, int phy, int reg)
{
	int val;
	int ii;
	int status;

	write_4(sc, PHY_CONTROL, PHY_RW_OK);
	write_4(sc, PHY_CONTROL,
	    (PHY_ADDRESS(phy)|PHY_READ_COMMAND |
	    PHY_REGISTER(reg)));

	for (ii = 0; ii < 0x1000; ii++) {
		status = read_4(sc, PHY_CONTROL);
		if (status & PHY_RW_OK) {
			/* Clear the rw_ok status, and clear other
			 * bits value. */
			write_4(sc, PHY_CONTROL, PHY_RW_OK);
			val = PHY_GET_DATA(status);
			return (val);
		}
	}
	return (0);
}

static inline void
phy_write(struct ece_softc *sc, int phy, int reg, int data)
{
	int ii;

	write_4(sc, PHY_CONTROL, PHY_RW_OK);
	write_4(sc, PHY_CONTROL,
	    PHY_ADDRESS(phy) | PHY_REGISTER(reg) |
	    PHY_WRITE_COMMAND | PHY_DATA(data));
	for (ii = 0; ii < 0x1000; ii++) {
		if (read_4(sc, PHY_CONTROL) & PHY_RW_OK) {
			/* Clear the rw_ok status, and clear other
			 * bits value.
			 */
			write_4(sc, PHY_CONTROL, PHY_RW_OK);
			return;
		}
	}
}

static int get_phy_type(struct ece_softc *sc)
{
	uint16_t phy0_id = 0, phy1_id = 0;

	/*
	 * Use SMI (MDC/MDIO) to read Link Partner's PHY Identifier
	 * Register 1.
	 */
	phy0_id = phy_read(sc, 0, 0x2);
	phy1_id = phy_read(sc, 1, 0x2);

	if ((phy0_id == 0xFFFF) && (phy1_id == 0x000F))
		return (ASIX_GIGA_PHY);
	else if ((phy0_id == 0x0243) && (phy1_id == 0x0243))
		return (TWO_SINGLE_PHY);
	else if ((phy0_id == 0xFFFF) && (phy1_id == 0x0007))
		return (VSC8601_GIGA_PHY);
	else if ((phy0_id == 0x0243) && (phy1_id == 0xFFFF))
		return (IC_PLUS_PHY);

	return (NOT_FOUND_PHY);
}

static int
ece_probe(device_t dev)
{

	device_set_desc(dev, "Econa Ethernet Controller");
	return (0);
}


static int
ece_attach(device_t dev)
{
	struct ece_softc *sc;
	struct ifnet *ifp = NULL;
	struct sysctl_ctx_list *sctx;
	struct sysctl_oid *soid;
	u_char eaddr[ETHER_ADDR_LEN];
	int err;
	int i, rid;
	uint32_t rnd;

	err = 0;

	sc = device_get_softc(dev);

	sc->dev = dev;

	rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
		    RF_ACTIVE);
	if (sc->mem_res == NULL)
		goto out;

	power_on_network_interface();

	rid = 0;
	sc->irq_res_status = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE);
	if (sc->irq_res_status == NULL)
		goto out;

	rid = 1;
	/*TSTC: Fm-Switch-Tx-Complete*/
	sc->irq_res_tx = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE);
	if (sc->irq_res_tx == NULL)
		goto out;

	rid = 2;
	/*FSRC: Fm-Switch-Rx-Complete*/
	sc->irq_res_rec = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE);
	if (sc->irq_res_rec == NULL)
		goto out;

	rid = 4;
	/*FSQF: Fm-Switch-Queue-Full*/
	sc->irq_res_qf = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE);
	if (sc->irq_res_qf == NULL)
		goto out;

	err = ece_activate(dev);
	if (err)
		goto out;

	/* Sysctls */
	sctx = device_get_sysctl_ctx(dev);
	soid = device_get_sysctl_tree(dev);

	ECE_LOCK_INIT(sc);

	callout_init_mtx(&sc->tick_ch, &sc->sc_mtx, 0);

	if ((err = ece_get_mac(sc, eaddr)) != 0) {
		/* No MAC address configured. Generate the random one. */
		if (bootverbose)
			device_printf(dev,
			    "Generating random ethernet address.\n");
		rnd = arc4random();

		/*from if_ae.c/if_ate.c*/
		/*
		 * Set OUI to convenient locally assigned address. 'b'
		 * is 0x62, which has the locally assigned bit set, and
		 * the broadcast/multicast bit clear.
		 */
		eaddr[0] = 'b';
		eaddr[1] = 's';
		eaddr[2] = 'd';
		eaddr[3] = (rnd >> 16) & 0xff;
		eaddr[4] = (rnd >> 8) & 0xff;
		eaddr[5] = rnd & 0xff;

		for (i = 0; i < ETHER_ADDR_LEN; i++)
			eaddr[i] = vlan0_mac[i];
	}
	ece_set_mac(sc, eaddr);
	sc->ifp = ifp = if_alloc(IFT_ETHER);
	/* Only one PHY at address 0 in this device. */
	err = mii_attach(dev, &sc->miibus, ifp, ece_ifmedia_upd,
	    ece_ifmedia_sts, BMSR_DEFCAPMASK, 0, MII_OFFSET_ANY, 0);
	if (err != 0) {
		device_printf(dev, "attaching PHYs failed\n");
		goto out;
	}
	ifp->if_softc = sc;
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;

	ifp->if_capabilities = IFCAP_HWCSUM;

	ifp->if_hwassist = (CSUM_IP | CSUM_TCP | CSUM_UDP);
	ifp->if_capenable = ifp->if_capabilities;
	ifp->if_start = ecestart;
	ifp->if_ioctl = eceioctl;
	ifp->if_init = eceinit;
	ifp->if_snd.ifq_drv_maxlen = ECE_MAX_TX_BUFFERS - 1;
	IFQ_SET_MAXLEN(&ifp->if_snd, ECE_MAX_TX_BUFFERS - 1);
	IFQ_SET_READY(&ifp->if_snd);

	/* Create local taskq. */

	TASK_INIT(&sc->sc_intr_task, 0, ece_intr_task, sc);
	TASK_INIT(&sc->sc_tx_task, 1, ece_tx_task, ifp);
	TASK_INIT(&sc->sc_cleanup_task, 2, ece_cleanup_task, sc);
	sc->sc_tq = taskqueue_create_fast("ece_taskq", M_WAITOK,
	    taskqueue_thread_enqueue,
	    &sc->sc_tq);
	if (sc->sc_tq == NULL) {
		device_printf(sc->dev, "could not create taskqueue\n");
		goto out;
	}

	ether_ifattach(ifp, eaddr);

	/*
	 * Activate interrupts
	 */
	err = bus_setup_intr(dev, sc->irq_res_rec, INTR_TYPE_NET | INTR_MPSAFE,
	    NULL, ece_intr, sc, &sc->intrhand);
	if (err) {
		ether_ifdetach(ifp);
		ECE_LOCK_DESTROY(sc);
		goto out;
	}

	err = bus_setup_intr(dev, sc->irq_res_status,
	    INTR_TYPE_NET | INTR_MPSAFE,
	    NULL, ece_intr_status, sc, &sc->intrhand_status);
	if (err) {
		ether_ifdetach(ifp);
		ECE_LOCK_DESTROY(sc);
		goto out;
	}

	err = bus_setup_intr(dev, sc->irq_res_qf, INTR_TYPE_NET | INTR_MPSAFE,
	    NULL,ece_intr_qf, sc, &sc->intrhand_qf);

	if (err) {
		ether_ifdetach(ifp);
		ECE_LOCK_DESTROY(sc);
		goto out;
	}

	err = bus_setup_intr(dev, sc->irq_res_tx, INTR_TYPE_NET | INTR_MPSAFE,
	    NULL, ece_intr_tx, sc, &sc->intrhand_tx);

	if (err) {
		ether_ifdetach(ifp);
		ECE_LOCK_DESTROY(sc);
		goto out;
	}

	ECE_TXLOCK_INIT(sc);
	ECE_RXLOCK_INIT(sc);
	ECE_CLEANUPLOCK_INIT(sc);

	/* Enable all interrupt sources. */
	write_4(sc, INTERRUPT_MASK, 0x00000000);

	/* Enable port 0. */
	write_4(sc, PORT_0_CONFIG, read_4(sc, PORT_0_CONFIG) & ~(PORT_DISABLE));

	taskqueue_start_threads(&sc->sc_tq, 1, PI_NET, "%s taskq",
	    device_get_nameunit(sc->dev));

out:
	if (err)
		ece_deactivate(dev);
	if (err && ifp)
		if_free(ifp);
	return (err);
}

static int
ece_detach(device_t dev)
{
	struct ece_softc *sc = device_get_softc(dev);
	struct ifnet *ifp = sc->ifp;

	ecestop(sc);
	if (ifp != NULL) {
		ether_ifdetach(ifp);
		if_free(ifp);
	}
	ece_deactivate(dev);
	return (0);
}

static void
ece_getaddr(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{
	u_int32_t *paddr;
	KASSERT(nsegs == 1, ("wrong number of segments, should be 1"));
	paddr = arg;
	*paddr = segs->ds_addr;
}

static int
ece_alloc_desc_dma_tx(struct ece_softc *sc)
{
	int i;
	int error;

	/* Allocate a busdma tag and DMA safe memory for TX/RX descriptors. */
	error = bus_dma_tag_create(sc->sc_parent_tag,	/* parent */
	    16, 0, /* alignment, boundary */
	    BUS_SPACE_MAXADDR_32BIT,	/* lowaddr */
	    BUS_SPACE_MAXADDR,	/* highaddr */
	    NULL, NULL,	/* filtfunc, filtfuncarg */
	    sizeof(eth_tx_desc_t)*ECE_MAX_TX_BUFFERS, /* max size */
	    1, /*nsegments */
	    sizeof(eth_tx_desc_t)*ECE_MAX_TX_BUFFERS,
	    0, /* flags */
	    NULL, NULL,	/* lockfunc, lockfuncarg */
	    &sc->dmatag_data_tx); /* dmat */

	/* Allocate memory for TX ring. */
	error = bus_dmamem_alloc(sc->dmatag_data_tx,
	    (void**)&(sc->desc_tx),
	    BUS_DMA_NOWAIT | BUS_DMA_ZERO |
	    BUS_DMA_COHERENT,
	    &(sc->dmamap_ring_tx));

	if (error) {
		if_printf(sc->ifp, "failed to allocate DMA memory\n");
		bus_dma_tag_destroy(sc->dmatag_data_tx);
		sc->dmatag_data_tx = 0;
		return (ENXIO);
	}

	/* Load Ring DMA. */
	error = bus_dmamap_load(sc->dmatag_data_tx, sc->dmamap_ring_tx,
	    sc->desc_tx,
	    sizeof(eth_tx_desc_t)*ECE_MAX_TX_BUFFERS,
	    ece_getaddr,
	    &(sc->ring_paddr_tx), BUS_DMA_NOWAIT);

	if (error) {
		if_printf(sc->ifp, "can't load descriptor\n");
		bus_dmamem_free(sc->dmatag_data_tx, sc->desc_tx,
		    sc->dmamap_ring_tx);
		sc->desc_tx = NULL;
		bus_dma_tag_destroy(sc->dmatag_data_tx);
		sc->dmatag_data_tx = 0;
		return (ENXIO);
	}

	/* Allocate a busdma tag for mbufs. Alignment is 2 bytes */
	error = bus_dma_tag_create(sc->sc_parent_tag,	/* parent */
	    1, 0,			/* alignment, boundary */
	    BUS_SPACE_MAXADDR_32BIT,	/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,		/* filtfunc, filtfuncarg */
	   MCLBYTES*MAX_FRAGMENT,	/* maxsize */
	   MAX_FRAGMENT,		 /* nsegments */
	    MCLBYTES, 0,		/* maxsegsz, flags */
	    NULL, NULL,		/* lockfunc, lockfuncarg */
	    &sc->dmatag_ring_tx);	/* dmat */

	if (error) {
		if_printf(sc->ifp, "failed to create busdma tag for mbufs\n");
		return (ENXIO);
	}

	for (i = 0; i < ECE_MAX_TX_BUFFERS; i++) {
		/* Create dma map for each descriptor. */
		error = bus_dmamap_create(sc->dmatag_ring_tx, 0,
		    &(sc->tx_desc[i].dmamap));
		if (error) {
			if_printf(sc->ifp, "failed to create map for mbuf\n");
			return (ENXIO);
		}
	}
	return (0);
}

static void
ece_free_desc_dma_tx(struct ece_softc *sc)
{
	int i;

	for (i = 0; i < ECE_MAX_TX_BUFFERS; i++) {
		if (sc->tx_desc[i].buff) {
			m_freem(sc->tx_desc[i].buff);
			sc->tx_desc[i].buff= 0;
		}
	}

	if (sc->ring_paddr_tx) {
		bus_dmamap_unload(sc->dmatag_data_tx, sc->dmamap_ring_tx);
		sc->ring_paddr_tx = 0;
	}

	if (sc->desc_tx) {
		bus_dmamem_free(sc->dmatag_data_tx,
		    sc->desc_tx, sc->dmamap_ring_tx);
		sc->desc_tx = NULL;
	}

	if (sc->dmatag_data_tx) {
		bus_dma_tag_destroy(sc->dmatag_data_tx);
		sc->dmatag_data_tx = 0;
	}

	if (sc->dmatag_ring_tx) {
		for (i = 0; i<ECE_MAX_TX_BUFFERS; i++) {
			bus_dmamap_destroy(sc->dmatag_ring_tx,
			    sc->tx_desc[i].dmamap);
			sc->tx_desc[i].dmamap = 0;
		}
		bus_dma_tag_destroy(sc->dmatag_ring_tx);
		sc->dmatag_ring_tx = 0;
	}
}

static int
ece_alloc_desc_dma_rx(struct ece_softc *sc)
{
	int error;
	int i;

	/* Allocate a busdma tag and DMA safe memory for RX descriptors. */
	error = bus_dma_tag_create(sc->sc_parent_tag,	/* parent */
	    16, 0,			/* alignment, boundary */
	    BUS_SPACE_MAXADDR_32BIT,	/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,		/* filtfunc, filtfuncarg */
	    /* maxsize, nsegments */
	    sizeof(eth_rx_desc_t)*ECE_MAX_RX_BUFFERS, 1,
	    /* maxsegsz, flags */
	    sizeof(eth_rx_desc_t)*ECE_MAX_RX_BUFFERS, 0,
	    NULL, NULL,		/* lockfunc, lockfuncarg */
	    &sc->dmatag_data_rx);	/* dmat */

	/* Allocate RX ring. */
	error = bus_dmamem_alloc(sc->dmatag_data_rx,
	    (void**)&(sc->desc_rx),
	    BUS_DMA_NOWAIT | BUS_DMA_ZERO |
	    BUS_DMA_COHERENT,
	    &(sc->dmamap_ring_rx));

	if (error) {
		if_printf(sc->ifp, "failed to allocate DMA memory\n");
		return (ENXIO);
	}

	/* Load dmamap. */
	error = bus_dmamap_load(sc->dmatag_data_rx, sc->dmamap_ring_rx,
	    sc->desc_rx,
	    sizeof(eth_rx_desc_t)*ECE_MAX_RX_BUFFERS,
	    ece_getaddr,
	    &(sc->ring_paddr_rx), BUS_DMA_NOWAIT);

	if (error) {
		if_printf(sc->ifp, "can't load descriptor\n");
		bus_dmamem_free(sc->dmatag_data_rx, sc->desc_rx,
		    sc->dmamap_ring_rx);
		bus_dma_tag_destroy(sc->dmatag_data_rx);
		sc->desc_rx = NULL;
		return (ENXIO);
	}

	/* Allocate a busdma tag for mbufs. */
	error = bus_dma_tag_create(sc->sc_parent_tag,/* parent */
	    16, 0,			/* alignment, boundary */
	    BUS_SPACE_MAXADDR_32BIT,	/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,		/* filtfunc, filtfuncarg */
	    MCLBYTES, 1,		/* maxsize, nsegments */
	    MCLBYTES, 0,		/* maxsegsz, flags */
	    NULL, NULL,		/* lockfunc, lockfuncarg */
	    &sc->dmatag_ring_rx);	/* dmat */

	if (error) {
		if_printf(sc->ifp, "failed to create busdma tag for mbufs\n");
		return (ENXIO);
	}

	for (i = 0; i<ECE_MAX_RX_BUFFERS; i++) {
		error = bus_dmamap_create(sc->dmatag_ring_rx, 0,
		    &sc->rx_desc[i].dmamap);
		if (error) {
			if_printf(sc->ifp, "failed to create map for mbuf\n");
			return (ENXIO);
		}
	}

	error = bus_dmamap_create(sc->dmatag_ring_rx, 0, &sc->rx_sparemap);
	if (error) {
		if_printf(sc->ifp, "failed to create spare map\n");
		return (ENXIO);
	}

	return (0);
}

static void
ece_free_desc_dma_rx(struct ece_softc *sc)
{
	int i;

	for (i = 0; i < ECE_MAX_RX_BUFFERS; i++) {
		if (sc->rx_desc[i].buff) {
			m_freem(sc->rx_desc[i].buff);
			sc->rx_desc[i].buff = NULL;
		}
	}

	if (sc->ring_paddr_rx) {
		bus_dmamap_unload(sc->dmatag_data_rx, sc->dmamap_ring_rx);
		sc->ring_paddr_rx = 0;
	}

	if (sc->desc_rx) {
		bus_dmamem_free(sc->dmatag_data_rx, sc->desc_rx,
		    sc->dmamap_ring_rx);
		sc->desc_rx = NULL;
	}

	if (sc->dmatag_data_rx) {
		bus_dma_tag_destroy(sc->dmatag_data_rx);
		sc->dmatag_data_rx = NULL;
	}

	if (sc->dmatag_ring_rx) {
		for (i = 0; i < ECE_MAX_RX_BUFFERS; i++)
			bus_dmamap_destroy(sc->dmatag_ring_rx,
			    sc->rx_desc[i].dmamap);
		bus_dmamap_destroy(sc->dmatag_ring_rx, sc->rx_sparemap);
		bus_dma_tag_destroy(sc->dmatag_ring_rx);
		sc->dmatag_ring_rx = NULL;
	}
}

static int
ece_new_rxbuf(struct ece_softc *sc, struct rx_desc_info* descinfo)
{
	struct mbuf *new_mbuf;
	bus_dma_segment_t seg[1];
	bus_dmamap_t map;
	int error;
	int nsegs;
	bus_dma_tag_t tag;

	tag = sc->dmatag_ring_rx;

	new_mbuf = m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR);

	if (new_mbuf == NULL)
		return (ENOBUFS);

	new_mbuf->m_len = new_mbuf->m_pkthdr.len = MCLBYTES;

	error = bus_dmamap_load_mbuf_sg(tag, sc->rx_sparemap, new_mbuf,
	    seg, &nsegs, BUS_DMA_NOWAIT);

	KASSERT(nsegs == 1, ("Too many segments returned!"));

	if (nsegs != 1 || error) {
		m_free(new_mbuf);
		return (ENOBUFS);
	}

	if (descinfo->buff != NULL) {
		bus_dmamap_sync(tag, descinfo->dmamap, BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(tag, descinfo->dmamap);
	}

	map = descinfo->dmamap;
	descinfo->dmamap = sc->rx_sparemap;
	sc->rx_sparemap = map;

	bus_dmamap_sync(tag, descinfo->dmamap, BUS_DMASYNC_PREREAD);

	descinfo->buff = new_mbuf;
	descinfo->desc->data_ptr = seg->ds_addr;
	descinfo->desc->length = seg->ds_len - 2;

	return (0);
}

static int
ece_allocate_dma(struct ece_softc *sc)
{
	eth_tx_desc_t *desctx;
	eth_rx_desc_t *descrx;
	int i;
	int error;

	/* Create parent tag for tx and rx */
	error = bus_dma_tag_create(
	    bus_get_dma_tag(sc->dev),/* parent */
	    1, 0,		/* alignment, boundary */
	    BUS_SPACE_MAXADDR,	/* lowaddr */
	    BUS_SPACE_MAXADDR,	/* highaddr */
	    NULL, NULL,	/* filter, filterarg */
	    BUS_SPACE_MAXSIZE_32BIT, 0,/* maxsize, nsegments */
	    BUS_SPACE_MAXSIZE_32BIT,	/* maxsegsize */
	    0,			/* flags */
	    NULL, NULL,	/* lockfunc, lockarg */
	    &sc->sc_parent_tag);

	ece_alloc_desc_dma_tx(sc);

	for (i = 0; i < ECE_MAX_TX_BUFFERS; i++) {
		desctx = (eth_tx_desc_t *)(&sc->desc_tx[i]);
		memset(desctx, 0, sizeof(eth_tx_desc_t));
		desctx->length = MAX_PACKET_LEN;
		desctx->cown = 1;
		if (i == ECE_MAX_TX_BUFFERS - 1)
			desctx->eor = 1;
	}

	ece_alloc_desc_dma_rx(sc);

	for (i = 0; i < ECE_MAX_RX_BUFFERS; i++) {
		descrx = &(sc->desc_rx[i]);
		memset(descrx, 0, sizeof(eth_rx_desc_t));
		sc->rx_desc[i].desc = descrx;
		sc->rx_desc[i].buff = 0;
		ece_new_rxbuf(sc, &(sc->rx_desc[i]));

		if (i == ECE_MAX_RX_BUFFERS - 1)
			descrx->eor = 1;
	}
	sc->tx_prod = 0;
	sc->tx_cons = 0;
	sc->last_rx = 0;
	sc->desc_curr_tx = 0;

	return (0);
}

static int
ece_activate(device_t dev)
{
	struct ece_softc *sc;
	int err;
	uint32_t mac_port_config;
	struct ifnet *ifp;

	sc = device_get_softc(dev);
	ifp = sc->ifp;

	initial_switch_config = read_4(sc, SWITCH_CONFIG);
	initial_cpu_config = read_4(sc, CPU_PORT_CONFIG);
	initial_port0_config = read_4(sc, MAC_PORT_0_CONFIG);
	initial_port1_config = read_4(sc, MAC_PORT_1_CONFIG);

	/* Disable Port 0 */
	mac_port_config = read_4(sc, MAC_PORT_0_CONFIG);
	mac_port_config |= (PORT_DISABLE);
	write_4(sc, MAC_PORT_0_CONFIG, mac_port_config);

	/* Disable Port 1 */
	mac_port_config = read_4(sc, MAC_PORT_1_CONFIG);
	mac_port_config |= (PORT_DISABLE);
	write_4(sc, MAC_PORT_1_CONFIG, mac_port_config);

	err = ece_allocate_dma(sc);
	if (err) {
		if_printf(sc->ifp, "failed allocating dma\n");
		goto out;
	}

	write_4(sc, TS_DESCRIPTOR_POINTER, sc->ring_paddr_tx);
	write_4(sc, TS_DESCRIPTOR_BASE_ADDR, sc->ring_paddr_tx);

	write_4(sc, FS_DESCRIPTOR_POINTER, sc->ring_paddr_rx);
	write_4(sc, FS_DESCRIPTOR_BASE_ADDR, sc->ring_paddr_rx);

	write_4(sc, FS_DMA_CONTROL, 1);

	return (0);
out:
	return (ENXIO);

}

static void
ece_deactivate(device_t dev)
{
	struct ece_softc *sc;

	sc = device_get_softc(dev);

	if (sc->intrhand)
		bus_teardown_intr(dev, sc->irq_res_rec, sc->intrhand);

	sc->intrhand = 0;

	if (sc->intrhand_qf)
		bus_teardown_intr(dev, sc->irq_res_qf, sc->intrhand_qf);

	sc->intrhand_qf = 0;

	bus_generic_detach(sc->dev);
	if (sc->miibus)
		device_delete_child(sc->dev, sc->miibus);
	if (sc->mem_res)
		bus_release_resource(dev, SYS_RES_IOPORT,
		    rman_get_rid(sc->mem_res), sc->mem_res);
	sc->mem_res = 0;

	if (sc->irq_res_rec)
		bus_release_resource(dev, SYS_RES_IRQ,
		    rman_get_rid(sc->irq_res_rec), sc->irq_res_rec);

	if (sc->irq_res_qf)
		bus_release_resource(dev, SYS_RES_IRQ,
		    rman_get_rid(sc->irq_res_qf), sc->irq_res_qf);

	if (sc->irq_res_qf)
		bus_release_resource(dev, SYS_RES_IRQ,
		    rman_get_rid(sc->irq_res_status), sc->irq_res_status);

	sc->irq_res_rec = 0;
	sc->irq_res_qf = 0;
	sc->irq_res_status = 0;
	ECE_TXLOCK_DESTROY(sc);
	ECE_RXLOCK_DESTROY(sc);

	ece_free_desc_dma_tx(sc);
	ece_free_desc_dma_rx(sc);

	return;
}

/*
 * Change media according to request.
 */
static int
ece_ifmedia_upd(struct ifnet *ifp)
{
	struct ece_softc *sc = ifp->if_softc;
	struct mii_data *mii;
	int error;

	mii = device_get_softc(sc->miibus);
	ECE_LOCK(sc);
	error = mii_mediachg(mii);
	ECE_UNLOCK(sc);
	return (error);
}

/*
 * Notify the world which media we're using.
 */
static void
ece_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct ece_softc *sc = ifp->if_softc;
	struct mii_data *mii;

	mii = device_get_softc(sc->miibus);
	ECE_LOCK(sc);
	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
	ECE_UNLOCK(sc);
}

static void
ece_tick(void *xsc)
{
	struct ece_softc *sc = xsc;
	struct mii_data *mii;
	int active;

	mii = device_get_softc(sc->miibus);
	active = mii->mii_media_active;
	mii_tick(mii);

	/*
	 * Schedule another timeout one second from now.
	 */
	callout_reset(&sc->tick_ch, hz, ece_tick, sc);
}

static uint32_t
read_mac_entry(struct ece_softc *ec,
    uint8_t *mac_result,
    int first)
{
	uint32_t ii;
	struct arl_table_entry_t entry;
	uint32_t *entry_val;
	write_4(ec, ARL_TABLE_ACCESS_CONTROL_0, 0);
	write_4(ec, ARL_TABLE_ACCESS_CONTROL_1, 0);
	write_4(ec, ARL_TABLE_ACCESS_CONTROL_2, 0);
	if (first)
		write_4(ec, ARL_TABLE_ACCESS_CONTROL_0, 0x1);
	else
		write_4(ec, ARL_TABLE_ACCESS_CONTROL_0, 0x2);

	for (ii = 0; ii < 0x1000; ii++)
		if (read_4(ec, ARL_TABLE_ACCESS_CONTROL_1) & (0x1))
			break;

	entry_val = (uint32_t*) (&entry);
	entry_val[0] = read_4(ec, ARL_TABLE_ACCESS_CONTROL_1);
	entry_val[1] = read_4(ec, ARL_TABLE_ACCESS_CONTROL_2);

	if (mac_result)
		memcpy(mac_result, entry.mac_addr, ETHER_ADDR_LEN);

	return (entry.table_end);
}

static uint32_t
write_arl_table_entry(struct ece_softc *ec,
    uint32_t filter,
    uint32_t vlan_mac,
    uint32_t vlan_gid,
    uint32_t age_field,
    uint32_t port_map,
    const uint8_t *mac_addr)
{
	uint32_t ii;
	uint32_t *entry_val;
	struct arl_table_entry_t entry;

	memset(&entry, 0, sizeof(entry));

	entry.filter = filter;
	entry.vlan_mac = vlan_mac;
	entry.vlan_gid = vlan_gid;
	entry.age_field = age_field;
	entry.port_map = port_map;
	memcpy(entry.mac_addr, mac_addr, ETHER_ADDR_LEN);

	entry_val = (uint32_t*) (&entry);

	write_4(ec, ARL_TABLE_ACCESS_CONTROL_0, 0);
	write_4(ec, ARL_TABLE_ACCESS_CONTROL_1, 0);
	write_4(ec, ARL_TABLE_ACCESS_CONTROL_2, 0);

	write_4(ec, ARL_TABLE_ACCESS_CONTROL_1, entry_val[0]);
	write_4(ec, ARL_TABLE_ACCESS_CONTROL_2, entry_val[1]);

	write_4(ec, ARL_TABLE_ACCESS_CONTROL_0, ARL_WRITE_COMMAND);

	for (ii = 0; ii < 0x1000; ii++)
		if (read_4(ec, ARL_TABLE_ACCESS_CONTROL_1) &
		    ARL_COMMAND_COMPLETE)
			return (1); /* Write OK. */

	/* Write failed. */
	return (0);
}

static void
remove_mac_entry(struct ece_softc *sc,
    uint8_t *mac)
{

	/* Invalid age_field mean erase this entry. */
	write_arl_table_entry(sc, 0, 1, VLAN0_GROUP_ID,
	    INVALID_ENTRY, VLAN0_GROUP,
	    mac);
}

static void
add_mac_entry(struct ece_softc *sc,
    uint8_t *mac)
{

	write_arl_table_entry(sc, 0, 1, VLAN0_GROUP_ID,
	    NEW_ENTRY, VLAN0_GROUP,
	    mac);
}

/**
 * The behavior of ARL table reading and deletion is not well defined
 * in the documentation. To be safe, all mac addresses are put to a
 * list, then deleted.
 *
 */
static void
clear_mac_entries(struct ece_softc *ec, int include_this_mac)
{
	int table_end;
	struct mac_list * temp;
	struct mac_list * mac_list_header;
	struct mac_list * current;
	char mac[ETHER_ADDR_LEN];

	current = NULL;
	mac_list_header = NULL;

	table_end = read_mac_entry(ec, mac, 1);
	while (!table_end) {
		if (!include_this_mac &&
		    memcmp(mac, vlan0_mac, ETHER_ADDR_LEN) == 0) {
			/* Read next entry. */
			table_end = read_mac_entry(ec, mac, 0);
			continue;
		}

		temp = (struct mac_list*)malloc(sizeof(struct mac_list),
		    M_DEVBUF,
		    M_NOWAIT | M_ZERO);
		memcpy(temp->mac_addr, mac, ETHER_ADDR_LEN);
		temp->next = 0;
		if (mac_list_header) {
			current->next = temp;
			current = temp;
		} else {
			mac_list_header = temp;
			current = temp;
		}
		/* Read next Entry */
		table_end = read_mac_entry(ec, mac, 0);
	}

	current = mac_list_header;

	while (current) {
		remove_mac_entry(ec, current->mac_addr);
		temp = current;
		current = current->next;
		free(temp, M_DEVBUF);
	}
}

static int
configure_lan_port(struct ece_softc *sc, int phy_type)
{
	uint32_t sw_config;
	uint32_t mac_port_config;

	/*
	 * Configure switch
	 */
	sw_config = read_4(sc, SWITCH_CONFIG);
	/* Enable fast aging. */
	sw_config |= FAST_AGING;
	/* Enable IVL learning. */
	sw_config |= IVL_LEARNING;
	/* Disable hardware NAT. */
	sw_config &= ~(HARDWARE_NAT);

	sw_config |= SKIP_L2_LOOKUP_PORT_0 | SKIP_L2_LOOKUP_PORT_1| NIC_MODE;

	write_4(sc, SWITCH_CONFIG, sw_config);

	sw_config = read_4(sc, SWITCH_CONFIG);

	mac_port_config = read_4(sc, MAC_PORT_0_CONFIG);

	if (!(mac_port_config & 0x1) || (mac_port_config & 0x2))
		if_printf(sc->ifp, "Link Down\n");
	else
		write_4(sc, MAC_PORT_0_CONFIG, mac_port_config);
	return (0);
}

static void
set_pvid(struct ece_softc *sc, int port0, int port1, int cpu)
{
	uint32_t val;
	val = read_4(sc, VLAN_PORT_PVID) & (~(0x7 << 0));
	write_4(sc, VLAN_PORT_PVID, val);
	val = read_4(sc, VLAN_PORT_PVID) | ((port0) & 0x07);
	write_4(sc, VLAN_PORT_PVID, val);
	val = read_4(sc, VLAN_PORT_PVID) & (~(0x7 << 4));
	write_4(sc, VLAN_PORT_PVID, val);
	val = read_4(sc, VLAN_PORT_PVID) | (((port1) & 0x07) << 4);
	write_4(sc, VLAN_PORT_PVID, val);

	val = read_4(sc, VLAN_PORT_PVID) & (~(0x7 << 8));
	write_4(sc, VLAN_PORT_PVID, val);
	val = read_4(sc, VLAN_PORT_PVID) | (((cpu) & 0x07) << 8);
	write_4(sc, VLAN_PORT_PVID, val);

}

/* VLAN related functions */
static void
set_vlan_vid(struct ece_softc *sc, int vlan)
{
	const uint32_t regs[] = {
	    VLAN_VID_0_1,
	    VLAN_VID_0_1,
	    VLAN_VID_2_3,
	    VLAN_VID_2_3,
	    VLAN_VID_4_5,
	    VLAN_VID_4_5,
	    VLAN_VID_6_7,
	    VLAN_VID_6_7
	};

	const int vids[] = {
	    VLAN0_VID,
	    VLAN1_VID,
	    VLAN2_VID,
	    VLAN3_VID,
	    VLAN4_VID,
	    VLAN5_VID,
	    VLAN6_VID,
	    VLAN7_VID
	};

	uint32_t val;
	uint32_t reg;
	int vid;

	reg = regs[vlan];
	vid = vids[vlan];

	if (vlan & 1) {
		val = read_4(sc, reg);
		write_4(sc, reg, val & (~(0xFFF << 0)));
		val = read_4(sc, reg);
		write_4(sc, reg, val|((vid & 0xFFF) << 0));
	} else {
		val = read_4(sc, reg);
		write_4(sc, reg, val & (~(0xFFF << 12)));
		val = read_4(sc, reg);
		write_4(sc, reg, val|((vid & 0xFFF) << 12));
	}
}

static void
set_vlan_member(struct ece_softc *sc, int vlan)
{
	unsigned char shift;
	uint32_t val;
	int group;
	const int groups[] = {
	    VLAN0_GROUP,
	    VLAN1_GROUP,
	    VLAN2_GROUP,
	    VLAN3_GROUP,
	    VLAN4_GROUP,
	    VLAN5_GROUP,
	    VLAN6_GROUP,
	    VLAN7_GROUP
	};

	group = groups[vlan];

	shift = vlan*3;
	val = read_4(sc, VLAN_MEMBER_PORT_MAP) & (~(0x7 << shift));
	write_4(sc, VLAN_MEMBER_PORT_MAP, val);
	val = read_4(sc, VLAN_MEMBER_PORT_MAP);
	write_4(sc, VLAN_MEMBER_PORT_MAP, val | ((group & 0x7) << shift));
}

static void
set_vlan_tag(struct ece_softc *sc, int vlan)
{
	unsigned char shift;
	uint32_t val;

	int tag = 0;

	shift = vlan*3;
	val = read_4(sc, VLAN_TAG_PORT_MAP) & (~(0x7 << shift));
	write_4(sc, VLAN_TAG_PORT_MAP, val);
	val = read_4(sc, VLAN_TAG_PORT_MAP);
	write_4(sc, VLAN_TAG_PORT_MAP, val | ((tag & 0x7) << shift));
}

static int
configure_cpu_port(struct ece_softc *sc)
{
	uint32_t cpu_port_config;
	int i;

	cpu_port_config = read_4(sc, CPU_PORT_CONFIG);
	/* SA learning Disable */
	cpu_port_config |= (SA_LEARNING_DISABLE);
	/* set data offset + 2 */
	cpu_port_config &= ~(1U << 31);

	write_4(sc, CPU_PORT_CONFIG, cpu_port_config);

	if (!write_arl_table_entry(sc, 0, 1, VLAN0_GROUP_ID,
	    STATIC_ENTRY, VLAN0_GROUP,
	    vlan0_mac))
		return (1);

	set_pvid(sc, PORT0_PVID, PORT1_PVID, CPU_PORT_PVID);

	for (i = 0; i < 8; i++) {
		set_vlan_vid(sc, i);
		set_vlan_member(sc, i);
		set_vlan_tag(sc, i);
	}

	/* disable all interrupt status sources */
	write_4(sc, INTERRUPT_MASK, 0xffff1fff);

	/* clear previous interrupt sources */
	write_4(sc, INTERRUPT_STATUS, 0x00001FFF);

	write_4(sc, TS_DMA_CONTROL, 0);
	write_4(sc, FS_DMA_CONTROL, 0);
	return (0);
}

static int
hardware_init(struct ece_softc *sc)
{
	int status = 0;
	static int gw_phy_type;

	gw_phy_type = get_phy_type(sc);
	/* Currently only ic_plus phy is supported. */
	if (gw_phy_type != IC_PLUS_PHY) {
		device_printf(sc->dev, "PHY type is not supported (%d)\n",
		    gw_phy_type);
		return (-1);
	}
	status = configure_lan_port(sc, gw_phy_type);
	configure_cpu_port(sc);
	return (0);
}

static void
set_mac_address(struct ece_softc *sc, const char *mac, int mac_len)
{

	/* Invalid age_field mean erase this entry. */
	write_arl_table_entry(sc, 0, 1, VLAN0_GROUP_ID,
	    INVALID_ENTRY, VLAN0_GROUP,
	    mac);
	memcpy(vlan0_mac, mac, ETHER_ADDR_LEN);

	write_arl_table_entry(sc, 0, 1, VLAN0_GROUP_ID,
	    STATIC_ENTRY, VLAN0_GROUP,
	    mac);
}

static void
ece_set_mac(struct ece_softc *sc, u_char *eaddr)
{
	memcpy(vlan0_mac, eaddr, ETHER_ADDR_LEN);
	set_mac_address(sc, eaddr, ETHER_ADDR_LEN);
}

/*
 * TODO: the device doesn't have MAC stored, we should read the
 * configuration stored in FLASH, but the format depends on the
 * bootloader used.*
 */
static int
ece_get_mac(struct ece_softc *sc, u_char *eaddr)
{
	return (ENXIO);
}

static void
ece_intr_rx_locked(struct ece_softc *sc, int count)
{
	struct ifnet *ifp = sc->ifp;
	struct mbuf *mb;
	struct rx_desc_info *rxdesc;
	eth_rx_desc_t *desc;

	int fssd_curr;
	int fssd;
	int i;
	int idx;
	int rxcount;
	uint32_t status;

	fssd_curr = read_4(sc, FS_DESCRIPTOR_POINTER);

	fssd = (fssd_curr - (uint32_t)sc->ring_paddr_rx)>>4;

	desc = sc->rx_desc[sc->last_rx].desc;

	/* Prepare to read the data in the ring. */
	bus_dmamap_sync(sc->dmatag_ring_rx,
	    sc->dmamap_ring_rx,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	if (fssd > sc->last_rx)
		rxcount = fssd - sc->last_rx;
	else if (fssd < sc->last_rx)
		rxcount = (ECE_MAX_RX_BUFFERS - sc->last_rx) + fssd;
	else {
		if (desc->cown == 0)
			return;
		else
			rxcount = ECE_MAX_RX_BUFFERS;
	}

	for (i= 0; i < rxcount; i++) {
		status = desc->cown;
		if (!status)
			break;

		idx = sc->last_rx;
		rxdesc = &sc->rx_desc[idx];
		mb = rxdesc->buff;

		if (desc->length < ETHER_MIN_LEN - ETHER_CRC_LEN ||
		    desc->length > ETHER_MAX_LEN - ETHER_CRC_LEN +
		    ETHER_VLAN_ENCAP_LEN) {
			if_inc_counter(ifp, IFCOUNTER_IERRORS, 1);
			desc->cown = 0;
			desc->length = MCLBYTES - 2;
			/* Invalid packet, skip and process next
			 * packet.
			 */
			continue;
		}

		if (ece_new_rxbuf(sc, rxdesc) != 0) {
			if_inc_counter(ifp, IFCOUNTER_IQDROPS, 1);
			desc->cown = 0;
			desc->length = MCLBYTES - 2;
			break;
		}

		/**
		 * The device will write to addrress + 2 So we need to adjust
		 * the address after the packet is received.
		 */
		mb->m_data += 2;
		mb->m_len = mb->m_pkthdr.len = desc->length;

		mb->m_flags |= M_PKTHDR;
		mb->m_pkthdr.rcvif = ifp;
		if ((ifp->if_capenable & IFCAP_RXCSUM) != 0) {
			/*check for valid checksum*/
			if ( (!desc->l4f)  && (desc->prot != 3)) {
				mb->m_pkthdr.csum_flags |= CSUM_IP_CHECKED;
				mb->m_pkthdr.csum_flags |= CSUM_IP_VALID;
				mb->m_pkthdr.csum_data = 0xffff;
			}
		}
		ECE_RXUNLOCK(sc);
		(*ifp->if_input)(ifp, mb);
		ECE_RXLOCK(sc);

		desc->cown = 0;
		desc->length = MCLBYTES - 2;

		bus_dmamap_sync(sc->dmatag_ring_rx,
		    sc->dmamap_ring_rx,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

		if (sc->last_rx == ECE_MAX_RX_BUFFERS - 1)
			sc->last_rx = 0;
		else
			sc->last_rx++;

		desc = sc->rx_desc[sc->last_rx].desc;
	}

	/* Sync updated flags. */
	bus_dmamap_sync(sc->dmatag_ring_rx,
	    sc->dmamap_ring_rx,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	return;
}

static void
ece_intr_task(void *arg, int pending __unused)
{
	struct ece_softc *sc = arg;
	ECE_RXLOCK(sc);
	ece_intr_rx_locked(sc, -1);
	ECE_RXUNLOCK(sc);
}

static void
ece_intr(void *xsc)
{
	struct ece_softc *sc = xsc;
	struct ifnet *ifp = sc->ifp;

	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0) {
		write_4(sc, FS_DMA_CONTROL, 0);
		return;
	}

	taskqueue_enqueue(sc->sc_tq, &sc->sc_intr_task);

	if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
		taskqueue_enqueue(sc->sc_tq, &sc->sc_tx_task);
}

static void
ece_intr_status(void *xsc)
{
	struct ece_softc *sc = xsc;
	struct ifnet *ifp = sc->ifp;
	int stat;

	stat = read_4(sc, INTERRUPT_STATUS);

	write_4(sc, INTERRUPT_STATUS, stat);

	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) != 0) {
		if ((stat & ERROR_MASK) != 0)
			if_inc_counter(ifp, IFCOUNTER_IQDROPS, 1);
	}
}

static void
ece_cleanup_locked(struct ece_softc *sc)
{
	eth_tx_desc_t *desc;

	if (sc->tx_cons == sc->tx_prod) return;

	/* Prepare to read the ring (owner bit). */
	bus_dmamap_sync(sc->dmatag_ring_tx,
	    sc->dmamap_ring_tx,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	while (sc->tx_cons != sc->tx_prod) {
		desc = sc->tx_desc[sc->tx_cons].desc;
		if (desc->cown != 0) {
			struct tx_desc_info *td = &(sc->tx_desc[sc->tx_cons]);
			/* We are finished with this descriptor ... */
			bus_dmamap_sync(sc->dmatag_data_tx, td->dmamap,
			    BUS_DMASYNC_POSTWRITE);
			/* ... and unload, so we can reuse. */
			bus_dmamap_unload(sc->dmatag_data_tx, td->dmamap);
			m_freem(td->buff);
			td->buff = 0;
			sc->tx_cons = (sc->tx_cons + 1) % ECE_MAX_TX_BUFFERS;
		} else {
			break;
		}
	}

}

static void
ece_cleanup_task(void *arg, int pending __unused)
{
	struct ece_softc *sc = arg;
	ECE_CLEANUPLOCK(sc);
	ece_cleanup_locked(sc);
	ECE_CLEANUPUNLOCK(sc);
}

static void
ece_intr_tx(void *xsc)
{
	struct ece_softc *sc = xsc;
	struct ifnet *ifp = sc->ifp;
	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0) {
		/* This should not happen, stop DMA. */
		write_4(sc, FS_DMA_CONTROL, 0);
		return;
	}
	taskqueue_enqueue(sc->sc_tq, &sc->sc_cleanup_task);
}

static void
ece_intr_qf(void *xsc)
{
	struct ece_softc *sc = xsc;
	struct ifnet *ifp = sc->ifp;
	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0) {
		/* This should not happen, stop DMA. */
		write_4(sc, FS_DMA_CONTROL, 0);
		return;
	}
	taskqueue_enqueue(sc->sc_tq, &sc->sc_intr_task);
	write_4(sc, FS_DMA_CONTROL, 1);
}

/*
 * Reset and initialize the chip
 */
static void
eceinit_locked(void *xsc)
{
	struct ece_softc *sc = xsc;
	struct ifnet *ifp = sc->ifp;
	struct mii_data *mii;
	uint32_t cfg_reg;
	uint32_t cpu_port_config;
	uint32_t mac_port_config;

	while (1) {
		cfg_reg = read_4(sc, BIST_RESULT_TEST_0);
		if ((cfg_reg & (1<<17)))
			break;
		DELAY(100);
	}
	/* Set to default values. */
	write_4(sc, SWITCH_CONFIG, 0x007AA7A1);
	write_4(sc, MAC_PORT_0_CONFIG, 0x00423D00);
	write_4(sc, MAC_PORT_1_CONFIG, 0x00423D80);
	write_4(sc, CPU_PORT_CONFIG, 0x004C0000);

	hardware_init(sc);

	mac_port_config = read_4(sc, MAC_PORT_0_CONFIG);

	 /* Enable Port 0 */
	mac_port_config &= (~(PORT_DISABLE));
	write_4(sc, MAC_PORT_0_CONFIG, mac_port_config);

	cpu_port_config = read_4(sc, CPU_PORT_CONFIG);
	/* Enable CPU. */
	cpu_port_config &= ~(PORT_DISABLE);
	write_4(sc, CPU_PORT_CONFIG, cpu_port_config);

	/*
	 * Set 'running' flag, and clear output active flag
	 * and attempt to start the output
	 */
	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	mii = device_get_softc(sc->miibus);
	mii_pollstat(mii);
	/* Enable DMA. */
	write_4(sc, FS_DMA_CONTROL, 1);

	callout_reset(&sc->tick_ch, hz, ece_tick, sc);
}

static inline int
ece_encap(struct ece_softc *sc, struct mbuf *m0)
{
	struct ifnet *ifp;
	bus_dma_segment_t segs[MAX_FRAGMENT];
	bus_dmamap_t mapp;
	eth_tx_desc_t *desc = NULL;
	int csum_flags;
	int desc_no;
	int error;
	int nsegs;
	int seg;

	ifp = sc->ifp;

	/* Fetch unused map */
	mapp = sc->tx_desc[sc->tx_prod].dmamap;

	error = bus_dmamap_load_mbuf_sg(sc->dmatag_ring_tx, mapp,
	    m0, segs, &nsegs,
	    BUS_DMA_NOWAIT);

	if (error != 0) {
		bus_dmamap_unload(sc->dmatag_ring_tx, mapp);
		return ((error != 0) ? error : -1);
	}

	desc = &(sc->desc_tx[sc->desc_curr_tx]);
	sc->tx_desc[sc->tx_prod].desc = desc;
	sc->tx_desc[sc->tx_prod].buff = m0;
	desc_no = sc->desc_curr_tx;

	for (seg = 0; seg < nsegs; seg++) {
		if (desc->cown == 0 ) {
			if_printf(ifp, "ERROR: descriptor is still used\n");
			return (-1);
		}

		desc->length = segs[seg].ds_len;
		desc->data_ptr = segs[seg].ds_addr;

		if (seg == 0) {
			desc->fs = 1;
		} else {
			desc->fs = 0;
		}
		if (seg == nsegs - 1) {
			desc->ls = 1;
		} else {
			desc->ls = 0;
		}

		csum_flags = m0->m_pkthdr.csum_flags;

		desc->fr =  1;
		desc->pmap =  1;
		desc->insv =  0;
		desc->ico = 0;
		desc->tco = 0;
		desc->uco = 0;
		desc->interrupt = 1;

		if (csum_flags & CSUM_IP) {
			desc->ico = 1;
			if (csum_flags & CSUM_TCP)
				desc->tco = 1;
			if (csum_flags & CSUM_UDP)
				desc->uco = 1;
		}

		desc++;
		sc->desc_curr_tx = (sc->desc_curr_tx + 1) % ECE_MAX_TX_BUFFERS;
		if (sc->desc_curr_tx == 0) {
			desc = (eth_tx_desc_t *)&(sc->desc_tx[0]);
		}
	}

	desc = sc->tx_desc[sc->tx_prod].desc;

	sc->tx_prod = (sc->tx_prod + 1) % ECE_MAX_TX_BUFFERS;

	/*
	 * After all descriptors are set, we set the flags to start the
	 * sending process.
	 */
	for (seg = 0; seg < nsegs; seg++) {
		desc->cown = 0;
		desc++;
		desc_no = (desc_no + 1) % ECE_MAX_TX_BUFFERS;
		if (desc_no == 0)
			desc = (eth_tx_desc_t *)&(sc->desc_tx[0]);
	}

	bus_dmamap_sync(sc->dmatag_data_tx, mapp, BUS_DMASYNC_PREWRITE);
	return (0);
}

/*
 * dequeu packets and transmit
 */
static void
ecestart_locked(struct ifnet *ifp)
{
	struct ece_softc *sc;
	struct mbuf *m0;
	uint32_t queued = 0;

	sc = ifp->if_softc;
	if ((ifp->if_drv_flags & (IFF_DRV_RUNNING | IFF_DRV_OACTIVE)) !=
	    IFF_DRV_RUNNING)
		return;

	bus_dmamap_sync(sc->dmatag_ring_tx,
	    sc->dmamap_ring_tx,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	for (;;) {
		/* Get packet from the queue */
		IF_DEQUEUE(&ifp->if_snd, m0);
		if (m0 == NULL)
			break;
		if (ece_encap(sc, m0)) {
			IF_PREPEND(&ifp->if_snd, m0);
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
			break;
		}
		queued++;
		BPF_MTAP(ifp, m0);
	}
	if (queued) {
		bus_dmamap_sync(sc->dmatag_ring_tx, sc->dmamap_ring_tx,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
		write_4(sc, TS_DMA_CONTROL, 1);
	}
}

static void
eceinit(void *xsc)
{
	struct ece_softc *sc = xsc;
	ECE_LOCK(sc);
	eceinit_locked(sc);
	ECE_UNLOCK(sc);
}

static void
ece_tx_task(void *arg, int pending __unused)
{
	struct ifnet *ifp;
	ifp = (struct ifnet *)arg;
	ecestart(ifp);
}

static void
ecestart(struct ifnet *ifp)
{
	struct ece_softc *sc = ifp->if_softc;
	ECE_TXLOCK(sc);
	ecestart_locked(ifp);
	ECE_TXUNLOCK(sc);
}

/*
 * Turn off interrupts, and stop the nic.  Can be called with sc->ifp
 * NULL so be careful.
 */
static void
ecestop(struct ece_softc *sc)
{
	struct ifnet *ifp = sc->ifp;
	uint32_t mac_port_config;

	write_4(sc, TS_DMA_CONTROL, 0);
	write_4(sc, FS_DMA_CONTROL, 0);

	if (ifp)
		ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

	callout_stop(&sc->tick_ch);

	/*Disable Port 0 */
	mac_port_config = read_4(sc, MAC_PORT_0_CONFIG);
	mac_port_config |= (PORT_DISABLE);
	write_4(sc, MAC_PORT_0_CONFIG, mac_port_config);

	/*Disable Port 1 */
	mac_port_config = read_4(sc, MAC_PORT_1_CONFIG);
	mac_port_config |= (PORT_DISABLE);
	write_4(sc, MAC_PORT_1_CONFIG, mac_port_config);

	/* Disable all interrupt status sources. */
	write_4(sc, INTERRUPT_MASK, 0x00001FFF);

	/* Clear previous interrupt sources. */
	write_4(sc, INTERRUPT_STATUS, 0x00001FFF);

	write_4(sc, SWITCH_CONFIG, initial_switch_config);
	write_4(sc, CPU_PORT_CONFIG, initial_cpu_config);
	write_4(sc, MAC_PORT_0_CONFIG, initial_port0_config);
	write_4(sc, MAC_PORT_1_CONFIG, initial_port1_config);

	clear_mac_entries(sc, 1);
}

static void
ece_restart(struct ece_softc *sc)
{
	struct ifnet *ifp = sc->ifp;

	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	/* Enable port 0. */
	write_4(sc, PORT_0_CONFIG,
	    read_4(sc, PORT_0_CONFIG) & ~(PORT_DISABLE));
	write_4(sc, INTERRUPT_MASK, 0x00000000);
	write_4(sc, FS_DMA_CONTROL, 1);
	callout_reset(&sc->tick_ch, hz, ece_tick, sc);
}

static void
set_filter(struct ece_softc *sc)
{
	struct ifnet		*ifp;
	struct ifmultiaddr	*ifma;
	uint32_t mac_port_config;

	ifp = sc->ifp;

	clear_mac_entries(sc, 0);
	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
		mac_port_config = read_4(sc, MAC_PORT_0_CONFIG);
		mac_port_config &= ~(DISABLE_BROADCAST_PACKET);
		mac_port_config &= ~(DISABLE_MULTICAST_PACKET);
		write_4(sc, MAC_PORT_0_CONFIG, mac_port_config);
		return;
	}
	if_maddr_rlock(ifp);
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		add_mac_entry(sc,
		    LLADDR((struct sockaddr_dl *)ifma->ifma_addr));
	}
	if_maddr_runlock(ifp);
}

static int
eceioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct ece_softc *sc = ifp->if_softc;
	struct mii_data *mii;
	struct ifreq *ifr = (struct ifreq *)data;
	int mask, error = 0;

	switch (cmd) {
	case SIOCSIFFLAGS:
		ECE_LOCK(sc);
		if ((ifp->if_flags & IFF_UP) == 0 &&
		    ifp->if_drv_flags & IFF_DRV_RUNNING) {
			ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
			ecestop(sc);
		} else {
			/* Reinitialize card on any parameter change. */
			if ((ifp->if_flags & IFF_UP) &&
			    !(ifp->if_drv_flags & IFF_DRV_RUNNING))
				ece_restart(sc);
		}
		ECE_UNLOCK(sc);
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		ECE_LOCK(sc);
		set_filter(sc);
		ECE_UNLOCK(sc);
		break;

	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		mii = device_get_softc(sc->miibus);
		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, cmd);
		break;
	case SIOCSIFCAP:
		mask = ifp->if_capenable ^ ifr->ifr_reqcap;
		if (mask & IFCAP_VLAN_MTU) {
			ECE_LOCK(sc);
			ECE_UNLOCK(sc);
		}
	default:
		error = ether_ioctl(ifp, cmd, data);
		break;
	}
	return (error);
}

static void
ece_child_detached(device_t dev, device_t child)
{
	struct ece_softc *sc;

	sc = device_get_softc(dev);
	if (child == sc->miibus)
		sc->miibus = NULL;
}

/*
 * MII bus support routines.
 */
static int
ece_miibus_readreg(device_t dev, int phy, int reg)
{
	struct ece_softc *sc;
	sc = device_get_softc(dev);
	return (phy_read(sc, phy, reg));
}

static int
ece_miibus_writereg(device_t dev, int phy, int reg, int data)
{
	struct ece_softc *sc;
	sc = device_get_softc(dev);
	phy_write(sc, phy, reg, data);
	return (0);
}

static device_method_t ece_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,	ece_probe),
	DEVMETHOD(device_attach,	ece_attach),
	DEVMETHOD(device_detach,	ece_detach),

	/* Bus interface */
	DEVMETHOD(bus_child_detached,	ece_child_detached),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	ece_miibus_readreg),
	DEVMETHOD(miibus_writereg,	ece_miibus_writereg),

	{ 0, 0 }
};

static driver_t ece_driver = {
	"ece",
	ece_methods,
	sizeof(struct ece_softc),
};

DRIVER_MODULE(ece, econaarm, ece_driver, ece_devclass, 0, 0);
DRIVER_MODULE(miibus, ece, miibus_driver, miibus_devclass, 0, 0);
MODULE_DEPEND(ece, miibus, 1, 1, 1);
MODULE_DEPEND(ece, ether, 1, 1, 1);
