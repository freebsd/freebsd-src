/*-
 * Copyright (c) 1997 Semen Ustimenko (semenu@FreeBSD.org)
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

/*
 * EtherPower II 10/100 Fast Ethernet (SMC 9432 serie)
 *
 * These cards are based on SMC83c17x (EPIC) chip and one of the various
 * PHYs (QS6612, AC101 and LXT970 were seen). The media support depends on
 * card model. All cards support 10baseT/UTP and 100baseTX half- and full-
 * duplex (SMB9432TX). SMC9432BTX also supports 10baseT/BNC. SMC9432FTX also
 * supports fibre optics.
 *
 * Thanks are going to Steve Bauer and Jason Wright.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/queue.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <net/bpf.h>

#include <net/if_vlan_var.h>

#include <vm/vm.h>		/* for vtophys */
#include <vm/pmap.h>		/* for vtophys */
#include <machine/bus_memio.h>
#include <machine/bus_pio.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/clock.h>	/* for DELAY */
#include <sys/bus.h>
#include <sys/rman.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include "miidevs.h"

#include <dev/mii/lxtphyreg.h>

#include "miibus_if.h"

#include <dev/tx/if_txreg.h>
#include <dev/tx/if_txvar.h>

MODULE_DEPEND(tx, miibus, 1, 1, 1);

static int epic_ifioctl(register struct ifnet *, u_long, caddr_t);
static void epic_intr(void *);
static void epic_tx_underrun(epic_softc_t *);
static int epic_common_attach(epic_softc_t *);
static void epic_ifstart(struct ifnet *);
static void epic_ifwatchdog(struct ifnet *);
static void epic_stats_update(epic_softc_t *);
static int epic_init(epic_softc_t *);
static void epic_stop(epic_softc_t *);
static void epic_rx_done(epic_softc_t *);
static void epic_tx_done(epic_softc_t *);
static int epic_init_rings(epic_softc_t *);
static void epic_free_rings(epic_softc_t *);
static void epic_stop_activity(epic_softc_t *);
static int epic_queue_last_packet(epic_softc_t *);
static void epic_start_activity(epic_softc_t *);
static void epic_set_rx_mode(epic_softc_t *);
static void epic_set_tx_mode(epic_softc_t *);
static void epic_set_mc_table(epic_softc_t *);
static u_int8_t epic_calchash(caddr_t);
static int epic_read_eeprom(epic_softc_t *,u_int16_t);
static void epic_output_eepromw(epic_softc_t *, u_int16_t);
static u_int16_t epic_input_eepromw(epic_softc_t *);
static u_int8_t epic_eeprom_clock(epic_softc_t *,u_int8_t);
static void epic_write_eepromreg(epic_softc_t *,u_int8_t);
static u_int8_t epic_read_eepromreg(epic_softc_t *);

static int epic_read_phy_reg(epic_softc_t *, int, int);
static void epic_write_phy_reg(epic_softc_t *, int, int, int);

static int epic_miibus_readreg(device_t, int, int);
static int epic_miibus_writereg(device_t, int, int, int);
static void epic_miibus_statchg(device_t);
static void epic_miibus_mediainit(device_t);

static int epic_ifmedia_upd(struct ifnet *);
static void epic_ifmedia_sts(struct ifnet *, struct ifmediareq *);

static int epic_probe(device_t);
static int epic_attach(device_t);
static void epic_shutdown(device_t);
static int epic_detach(device_t);
static struct epic_type *epic_devtype(device_t);

static device_method_t epic_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		epic_probe),
	DEVMETHOD(device_attach,	epic_attach),
	DEVMETHOD(device_detach,	epic_detach),
	DEVMETHOD(device_shutdown,	epic_shutdown),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	epic_miibus_readreg),
	DEVMETHOD(miibus_writereg,	epic_miibus_writereg),
	DEVMETHOD(miibus_statchg,	epic_miibus_statchg),
	DEVMETHOD(miibus_mediainit,	epic_miibus_mediainit),

	{ 0, 0 }
};

static driver_t epic_driver = {
	"tx",
	epic_methods,
	sizeof(epic_softc_t)
};

static devclass_t epic_devclass;

DRIVER_MODULE(if_tx, pci, epic_driver, epic_devclass, 0, 0);
DRIVER_MODULE(miibus, tx, miibus_driver, miibus_devclass, 0, 0);

static struct epic_type epic_devs[] = {
	{ SMC_VENDORID, SMC_DEVICEID_83C170,
		"SMC EtherPower II 10/100" },
	{ 0, 0, NULL }
};

static int
epic_probe(dev)
	device_t dev;
{
	struct epic_type *t;

	t = epic_devtype(dev);

	if (t != NULL) {
		device_set_desc(dev, t->name);
		return(0);
	}

	return(ENXIO);
}

static struct epic_type *
epic_devtype(dev)
	device_t dev;
{
	struct epic_type *t;

	t = epic_devs;

	while(t->name != NULL) {
		if ((pci_get_vendor(dev) == t->ven_id) &&
		    (pci_get_device(dev) == t->dev_id)) {
			return(t);
		}
		t++;
	}
	return (NULL);
}

#if defined(EPIC_USEIOSPACE)
#define	EPIC_RES	SYS_RES_IOPORT
#define	EPIC_RID	PCIR_BASEIO
#else
#define	EPIC_RES	SYS_RES_MEMORY
#define	EPIC_RID	PCIR_BASEMEM
#endif

/*
 * Attach routine: map registers, allocate softc, rings and descriptors.
 * Reset to known state.
 */
static int
epic_attach(dev)
	device_t dev;
{
	struct ifnet *ifp;
	epic_softc_t *sc;
	u_int32_t command;
	int unit, error;
	int i, s, rid, tmp;

	s = splimp ();

	sc = device_get_softc(dev);
	unit = device_get_unit(dev);

	/* Preinitialize softc structure */
	bzero(sc, sizeof(epic_softc_t));		
	sc->unit = unit;
	sc->dev = dev;

	/* Fill ifnet structure */
	ifp = &sc->sc_if;
	ifp->if_unit = unit;
	ifp->if_name = "tx";
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST|IFF_SIMPLEX|IFF_MULTICAST;
	ifp->if_ioctl = epic_ifioctl;
	ifp->if_output = ether_output;
	ifp->if_start = epic_ifstart;
	ifp->if_watchdog = epic_ifwatchdog;
	ifp->if_init = (if_init_f_t*)epic_init;
	ifp->if_timer = 0;
	ifp->if_baudrate = 10000000;
	ifp->if_snd.ifq_maxlen = TX_RING_SIZE - 1;

	/* Enable ports, memory and busmastering */
	command = pci_read_config(dev, PCIR_COMMAND, 4);
	command |= PCIM_CMD_PORTEN | PCIM_CMD_MEMEN | PCIM_CMD_BUSMASTEREN;
	pci_write_config(dev, PCIR_COMMAND, command, 4);
	command = pci_read_config(dev, PCIR_COMMAND, 4);

#if defined(EPIC_USEIOSPACE)
	if ((command & PCIM_CMD_PORTEN) == 0) {
		device_printf(dev, "failed to enable I/O mapping!\n");
		error = ENXIO;
		goto fail;
	}
#else
	if ((command & PCIM_CMD_MEMEN) == 0) {
		device_printf(dev, "failed to enable memory mapping!\n");
		error = ENXIO;
		goto fail;
	}
#endif

	rid = EPIC_RID;
	sc->res = bus_alloc_resource(dev, EPIC_RES, &rid, 0, ~0, 1,
	    RF_ACTIVE);

	if (sc->res == NULL) {
		device_printf(dev, "couldn't map ports/memory\n");
		error = ENXIO;
		goto fail;
	}

	sc->sc_st = rman_get_bustag(sc->res);
	sc->sc_sh = rman_get_bushandle(sc->res);

	/* Allocate interrupt */
	rid = 0;
	sc->irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0, 1,
	    RF_SHAREABLE | RF_ACTIVE);

	if (sc->irq == NULL) {
		device_printf(dev, "couldn't map interrupt\n");
		bus_release_resource(dev, EPIC_RES, EPIC_RID, sc->res);
		error = ENXIO;
		goto fail;
	}

	error = bus_setup_intr(dev, sc->irq, INTR_TYPE_NET,
	    epic_intr, sc, &sc->sc_ih);

	if (error) {
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->irq);
		bus_release_resource(dev, EPIC_RES, EPIC_RID, sc->res);
		device_printf(dev, "couldn't set up irq\n");
		goto fail;
	}

	/* Do OS independent part, including chip wakeup and reset */
	error = epic_common_attach(sc);
	if (error) {
		bus_teardown_intr(dev, sc->irq, sc->sc_ih);
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->irq);
		bus_release_resource(dev, EPIC_RES, EPIC_RID, sc->res);
		error = ENXIO;
		goto fail;
	}

	/* Do ifmedia setup */
	if (mii_phy_probe(dev, &sc->miibus,
	    epic_ifmedia_upd, epic_ifmedia_sts)) {
		device_printf(dev, "ERROR! MII without any PHY!?\n");
		bus_teardown_intr(dev, sc->irq, sc->sc_ih);
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->irq);
		bus_release_resource(dev, EPIC_RES, EPIC_RID, sc->res);
		error = ENXIO;
		goto fail;
	}

	/* Display ethernet address ,... */
	device_printf(dev, "address %6D,", sc->sc_macaddr, ":");

	/* board type and ... */
	printf(" type ");
	for(i=0x2c;i<0x32;i++) {
		tmp = epic_read_eeprom(sc, i);
		if (' ' == (u_int8_t)tmp) break;
		printf("%c", (u_int8_t)tmp);
		tmp >>= 8;
		if (' ' == (u_int8_t)tmp) break;
		printf("%c", (u_int8_t)tmp);
	}
	printf("\n");

	/* Attach to OS's managers */
	ether_ifattach(ifp, sc->sc_macaddr);
	ifp->if_hdrlen = sizeof(struct ether_vlan_header);
	ifp->if_capabilities |= IFCAP_VLAN_MTU;
	callout_handle_init(&sc->stat_ch);

fail:
	splx(s);

	return(error);
}

/*
 * Detach driver and free resources
 */
static int
epic_detach(dev)
	device_t dev;
{
	struct ifnet *ifp;
	epic_softc_t *sc;
	int s;

	s = splimp();

	sc = device_get_softc(dev);
	ifp = &sc->arpcom.ac_if;

	ether_ifdetach(ifp);

	epic_stop(sc);

	bus_generic_detach(dev);
	device_delete_child(dev, sc->miibus);

	bus_teardown_intr(dev, sc->irq, sc->sc_ih);
	bus_release_resource(dev, SYS_RES_IRQ, 0, sc->irq);
	bus_release_resource(dev, EPIC_RES, EPIC_RID, sc->res);

	free(sc->tx_flist, M_DEVBUF);
	free(sc->tx_desc, M_DEVBUF);
	free(sc->rx_desc, M_DEVBUF);

	splx(s);

	return(0);
}

#undef	EPIC_RES
#undef	EPIC_RID

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
static void
epic_shutdown(dev)
	device_t dev;
{
	epic_softc_t *sc;

	sc = device_get_softc(dev);

	epic_stop(sc);

	return;
}

/*
 * This is if_ioctl handler.
 */
static int
epic_ifioctl(ifp, command, data)
	struct ifnet *ifp;
	u_long command;
	caddr_t data;
{
	epic_softc_t *sc = ifp->if_softc;
	struct mii_data	*mii;
	struct ifreq *ifr = (struct ifreq *) data;
	int x, error = 0;

	x = splimp();

	switch (command) {
	case SIOCSIFMTU:
		if (ifp->if_mtu == ifr->ifr_mtu)
			break;

		/* XXX Though the datasheet doesn't imply any
		 * limitations on RX and TX sizes beside max 64Kb
		 * DMA transfer, seems we can't send more then 1600
		 * data bytes per ethernet packet. (Transmitter hangs
		 * up if more data is sent)
		 */
		if (ifr->ifr_mtu + ifp->if_hdrlen <= EPIC_MAX_MTU) {
			ifp->if_mtu = ifr->ifr_mtu;
			epic_stop(sc);
			epic_init(sc);
		} else
			error = EINVAL;
		break;

	case SIOCSIFFLAGS:
		/*
		 * If the interface is marked up and stopped, then start it.
		 * If it is marked down and running, then stop it.
		 */
		if (ifp->if_flags & IFF_UP) {
			if ((ifp->if_flags & IFF_RUNNING) == 0) {
				epic_init(sc);
				break;
			}
		} else {
			if (ifp->if_flags & IFF_RUNNING) {
				epic_stop(sc);
				break;
			}
		}

		/* Handle IFF_PROMISC and IFF_ALLMULTI flags */
		epic_stop_activity(sc);	
		epic_set_mc_table(sc);
		epic_set_rx_mode(sc);
		epic_start_activity(sc);	
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		epic_set_mc_table(sc);
		error = 0;
		break;

	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		mii = device_get_softc(sc->miibus);
		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, command);
		break;

	default:
		error = ether_ioctl(ifp, command, data);
		break;
	}
	splx(x);

	return error;
}

/*
 * OS-independed part of attach process. allocate memory for descriptors
 * and frag lists, wake up chip, read MAC address and PHY identyfier.
 * Return -1 on failure.
 */
static int
epic_common_attach(sc)
	epic_softc_t *sc;
{
	int i;

	sc->tx_flist = malloc(sizeof(struct epic_frag_list)*TX_RING_SIZE,
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	sc->tx_desc = malloc(sizeof(struct epic_tx_desc)*TX_RING_SIZE,
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	sc->rx_desc = malloc(sizeof(struct epic_rx_desc)*RX_RING_SIZE,
	    M_DEVBUF, M_NOWAIT | M_ZERO);

	if (sc->tx_flist == NULL || sc->tx_desc == NULL || sc->rx_desc == NULL){
		device_printf(sc->dev, "failed to malloc memory\n");
		if (sc->tx_flist) free(sc->tx_flist, M_DEVBUF);
		if (sc->tx_desc) free(sc->tx_desc, M_DEVBUF);
		if (sc->rx_desc) free(sc->rx_desc, M_DEVBUF);
		return (ENOMEM);
	}

	/* Bring the chip out of low-power mode. */
	CSR_WRITE_4(sc, GENCTL, GENCTL_SOFT_RESET);
	DELAY(500);

	/* Workaround for Application Note 7-15 */
	for (i=0; i<16; i++) CSR_WRITE_4(sc, TEST1, TEST1_CLOCK_TEST);

	/* Read mac address from EEPROM */
	for (i = 0; i < ETHER_ADDR_LEN / sizeof(u_int16_t); i++)
		((u_int16_t *)sc->sc_macaddr)[i] = epic_read_eeprom(sc,i);

	/* Set Non-Volatile Control Register from EEPROM */
	CSR_WRITE_4(sc, NVCTL, epic_read_eeprom(sc, EEPROM_NVCTL) & 0x1F);

	/* Set defaults */
	sc->tx_threshold = TRANSMIT_THRESHOLD;
	sc->txcon = TXCON_DEFAULT;
	sc->miicfg = MIICFG_SMI_ENABLE;
	sc->phyid = EPIC_UNKN_PHY;
	sc->serinst = -1;

	/* Fetch card id */
	sc->cardvend = pci_read_config(sc->dev, PCIR_SUBVEND_0, 2);
	sc->cardid = pci_read_config(sc->dev, PCIR_SUBDEV_0, 2);

	if (sc->cardvend != SMC_VENDORID)
		device_printf(sc->dev, "unknown card vendor %04xh\n", sc->cardvend);

	return 0;
}

/*
 * This is if_start handler. It takes mbufs from if_snd queue
 * and queue them for transmit, one by one, until TX ring become full
 * or queue become empty.
 */
static void
epic_ifstart(ifp)
	struct ifnet * ifp;
{
	epic_softc_t *sc = ifp->if_softc;
	struct epic_tx_buffer *buf;
	struct epic_tx_desc *desc;
	struct epic_frag_list *flist;
	struct mbuf *m0;
	register struct mbuf *m;
	register int i;

	while (sc->pending_txs < TX_RING_SIZE) {
		buf = sc->tx_buffer + sc->cur_tx;
		desc = sc->tx_desc + sc->cur_tx;
		flist = sc->tx_flist + sc->cur_tx;

		/* Get next packet to send */
		IF_DEQUEUE(&ifp->if_snd, m0);

		/* If nothing to send, return */
		if (NULL == m0) return;

		/* Fill fragments list */
		for (m = m0, i = 0;
		    (NULL != m) && (i < EPIC_MAX_FRAGS);
		    m = m->m_next, i++) {
			flist->frag[i].fraglen = m->m_len;
			flist->frag[i].fragaddr = vtophys(mtod(m, caddr_t));
		}
		flist->numfrags = i;

		/* If packet was more than EPIC_MAX_FRAGS parts, */
		/* recopy packet to new allocated mbuf cluster */
		if( NULL != m ){
			m = m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR);
			if( NULL == m ){
				m_freem(m0);
				ifp->if_oerrors++;
				continue;
			}

			m_copydata(m0, 0, m0->m_pkthdr.len, mtod(m, caddr_t));
			flist->frag[0].fraglen =
			     m->m_pkthdr.len = m->m_len = m0->m_pkthdr.len;
			m->m_pkthdr.rcvif = ifp;

			flist->numfrags = 1;
			flist->frag[0].fragaddr = vtophys(mtod(m, caddr_t));
			m_freem(m0);
			m0 = m;
		}

		buf->mbuf = m0;
		sc->pending_txs++;
		sc->cur_tx = (sc->cur_tx + 1) & TX_RING_MASK;
		desc->control = 0x01;
		desc->txlength =
		    max(m0->m_pkthdr.len,ETHER_MIN_LEN-ETHER_CRC_LEN);
		desc->status = 0x8000;
		CSR_WRITE_4(sc, COMMAND, COMMAND_TXQUEUED);

		/* Set watchdog timer */
		ifp->if_timer = 8;

		BPF_MTAP(ifp, m0);
	}

	ifp->if_flags |= IFF_OACTIVE;

	return;
	
}

/*
 * Synopsis: Finish all received frames.
 */
static void
epic_rx_done(sc)
	epic_softc_t *sc;
{
	struct ifnet *ifp = &sc->sc_if;
	u_int16_t len;
	struct epic_rx_buffer *buf;
	struct epic_rx_desc *desc;
	struct mbuf *m;

	while ((sc->rx_desc[sc->cur_rx].status & 0x8000) == 0) {
		buf = sc->rx_buffer + sc->cur_rx;
		desc = sc->rx_desc + sc->cur_rx;

		/* Switch to next descriptor */
		sc->cur_rx = (sc->cur_rx+1) & RX_RING_MASK;

		/*
		 * Check for RX errors. This should only happen if
		 * SAVE_ERRORED_PACKETS is set. RX errors generate
		 * RXE interrupt usually.
		 */
		if ((desc->status & 1) == 0) {
			ifp->if_ierrors++;
			desc->status = 0x8000;
			continue;
		}

		/* Save packet length and mbuf contained packet */
		len = desc->rxlength - ETHER_CRC_LEN;
		m = buf->mbuf;

		/* Try to get mbuf cluster */
		buf->mbuf = m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR);
		if( NULL == buf->mbuf ) { 
			buf->mbuf = m;
			desc->status = 0x8000;
			ifp->if_ierrors++;
			continue;
		}
		buf->mbuf->m_len = buf->mbuf->m_pkthdr.len = MCLBYTES;
		m_adj(buf->mbuf, ETHER_ALIGN);

		/* Point to new mbuf, and give descriptor to chip */
		desc->bufaddr = vtophys(mtod(buf->mbuf, caddr_t));
		desc->status = 0x8000;
		
		/* First mbuf in packet holds the ethernet and packet headers */
		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = m->m_len = len;

		/* Give mbuf to OS */
		(*ifp->if_input)(ifp, m);

		/* Successfuly received frame */
		ifp->if_ipackets++;
        }

	return;
}

/*
 * Synopsis: Do last phase of transmission. I.e. if desc is
 * transmitted, decrease pending_txs counter, free mbuf contained
 * packet, switch to next descriptor and repeat until no packets
 * are pending or descriptor is not transmitted yet.
 */
static void
epic_tx_done(sc)
	epic_softc_t *sc;
{
	struct epic_tx_buffer *buf;
	struct epic_tx_desc *desc;
	u_int16_t status;

	while (sc->pending_txs > 0) {
		buf = sc->tx_buffer + sc->dirty_tx;
		desc = sc->tx_desc + sc->dirty_tx;
		status = desc->status;

		/* If packet is not transmitted, thou followed */
		/* packets are not transmitted too */
		if (status & 0x8000) break;

		/* Packet is transmitted. Switch to next and */
		/* free mbuf */
		sc->pending_txs--;
		sc->dirty_tx = (sc->dirty_tx + 1) & TX_RING_MASK;
		m_freem(buf->mbuf);
		buf->mbuf = NULL;

		/* Check for errors and collisions */
		if (status & 0x0001) sc->sc_if.if_opackets++;
		else sc->sc_if.if_oerrors++;
		sc->sc_if.if_collisions += (status >> 8) & 0x1F;
#if defined(EPIC_DIAG)
		if ((status & 0x1001) == 0x1001)
			device_printf(sc->dev,  "Tx ERROR: excessive coll. number\n");
#endif
	}

	if (sc->pending_txs < TX_RING_SIZE)
		sc->sc_if.if_flags &= ~IFF_OACTIVE;
}

/*
 * Interrupt function
 */
static void
epic_intr(arg)
    void *arg;
{
    epic_softc_t * sc = (epic_softc_t *) arg;
    int status, i = 4;

    while (i-- && ((status = CSR_READ_4(sc, INTSTAT)) & INTSTAT_INT_ACTV)) {
	CSR_WRITE_4(sc, INTSTAT, status);

	if (status & (INTSTAT_RQE|INTSTAT_RCC|INTSTAT_OVW)) {
	    epic_rx_done(sc);
	    if (status & (INTSTAT_RQE|INTSTAT_OVW)) {
#if defined(EPIC_DIAG)
		if (status & INTSTAT_OVW)
		    device_printf(sc->dev, "RX buffer overflow\n");
		if (status & INTSTAT_RQE)
		    device_printf(sc->dev, "RX FIFO overflow\n");
#endif
		if ((CSR_READ_4(sc, COMMAND) & COMMAND_RXQUEUED) == 0)
		    CSR_WRITE_4(sc, COMMAND, COMMAND_RXQUEUED);
		sc->sc_if.if_ierrors++;
	    }
	}

	if (status & (INTSTAT_TXC|INTSTAT_TCC|INTSTAT_TQE)) {
	    epic_tx_done(sc);
	    if (sc->sc_if.if_snd.ifq_head != NULL)
		    epic_ifstart(&sc->sc_if);
	}

	/* Check for rare errors */
	if (status & (INTSTAT_FATAL|INTSTAT_PMA|INTSTAT_PTA|
		      INTSTAT_APE|INTSTAT_DPE|INTSTAT_TXU|INTSTAT_RXE)) {
    	    if (status & (INTSTAT_FATAL|INTSTAT_PMA|INTSTAT_PTA|
			  INTSTAT_APE|INTSTAT_DPE)) {
		device_printf(sc->dev, "PCI fatal errors occured: %s%s%s%s\n",
		    (status&INTSTAT_PMA)?"PMA ":"",
		    (status&INTSTAT_PTA)?"PTA ":"",
		    (status&INTSTAT_APE)?"APE ":"",
		    (status&INTSTAT_DPE)?"DPE":""
		);

		epic_stop(sc);
		epic_init(sc);
		
	    	break;
	    }

	    if (status & INTSTAT_RXE) {
#if defined(EPIC_DIAG)
		device_printf(sc->dev, "CRC/Alignment error\n");
#endif
		sc->sc_if.if_ierrors++;
	    }

	    if (status & INTSTAT_TXU) {
		epic_tx_underrun(sc);
		sc->sc_if.if_oerrors++;
	    }
	}
    }

    /* If no packets are pending, then no timeouts */
    if (sc->pending_txs == 0) sc->sc_if.if_timer = 0;

    return;
}

/*
 * Handle the TX underrun error: increase the TX threshold
 * and restart the transmitter.
 */
static void
epic_tx_underrun(sc)
	epic_softc_t *sc;
{
	if (sc->tx_threshold > TRANSMIT_THRESHOLD_MAX) {
		sc->txcon &= ~TXCON_EARLY_TRANSMIT_ENABLE;
#if defined(EPIC_DIAG)
		device_printf(sc->dev, "Tx UNDERRUN: early TX disabled\n");
#endif
	} else {
		sc->tx_threshold += 0x40;
#if defined(EPIC_DIAG)
		device_printf(sc->dev, "Tx UNDERRUN: TX threshold increased to %d\n",
		    sc->tx_threshold);
#endif
	}

	/* We must set TXUGO to reset the stuck transmitter */
	CSR_WRITE_4(sc, COMMAND, COMMAND_TXUGO);

	/* Update the TX threshold */
	epic_stop_activity(sc);
	epic_set_tx_mode(sc);
	epic_start_activity(sc);

	return;
}

/*
 * Synopsis: This one is called if packets wasn't transmitted
 * during timeout. Try to deallocate transmitted packets, and
 * if success continue to work.
 */
static void
epic_ifwatchdog(ifp)
	struct ifnet *ifp;
{
	epic_softc_t *sc = ifp->if_softc;
	int x;

	x = splimp();

	device_printf(sc->dev, "device timeout %d packets\n", sc->pending_txs);

	/* Try to finish queued packets */
	epic_tx_done(sc);

	/* If not successful */
	if (sc->pending_txs > 0) {

		ifp->if_oerrors+=sc->pending_txs;

		/* Reinitialize board */
		device_printf(sc->dev, "reinitialization\n");
		epic_stop(sc);
		epic_init(sc);

	} else
		device_printf(sc->dev, "seems we can continue normaly\n");

	/* Start output */
	if (ifp->if_snd.ifq_head) epic_ifstart(ifp);

	splx(x);
}

/*
 * Despite the name of this function, it doesn't update statistics, it only
 * helps in autonegotiation process.
 */
static void
epic_stats_update(epic_softc_t * sc)
{
	struct mii_data * mii;
	int s;

	s = splimp();

	mii = device_get_softc(sc->miibus);
	mii_tick(mii);

	sc->stat_ch = timeout((timeout_t *)epic_stats_update, sc, hz);

	splx(s);
}

/*
 * Set media options.
 */
static int
epic_ifmedia_upd(ifp)
	struct ifnet *ifp;
{
	epic_softc_t *sc;
	struct mii_data *mii;
	struct ifmedia *ifm;
	struct mii_softc *miisc;
	int cfg, media;

	sc = ifp->if_softc;
	mii = device_get_softc(sc->miibus);
	ifm = &mii->mii_media;
	media = ifm->ifm_cur->ifm_media;

	/* Do not do anything if interface is not up */
	if ((ifp->if_flags & IFF_UP) == 0)
		return (0);

	/*
	 * Lookup current selected PHY
	 */
	if (IFM_INST(media) == sc->serinst) {
		sc->phyid = EPIC_SERIAL;
		sc->physc = NULL;
	} else {
		/* If we're not selecting serial interface, select MII mode */
		sc->miicfg &= ~MIICFG_SERIAL_ENABLE;
		CSR_WRITE_4(sc, MIICFG, sc->miicfg);

		/* Default to unknown PHY */
		sc->phyid = EPIC_UNKN_PHY;

		/* Lookup selected PHY */
		for (miisc = LIST_FIRST(&mii->mii_phys); miisc != NULL;
		     miisc = LIST_NEXT(miisc, mii_list)) {
			if (IFM_INST(media) == miisc->mii_inst) {
				sc->physc = miisc;
				break;
			}
		}

		/* Identify selected PHY */
		if (sc->physc) {
			int id1, id2, model, oui;

			id1 = PHY_READ(sc->physc, MII_PHYIDR1);
			id2 = PHY_READ(sc->physc, MII_PHYIDR2);

			oui = MII_OUI(id1, id2);
			model = MII_MODEL(id2);
			switch (oui) {
			case MII_OUI_QUALSEMI:
				if (model == MII_MODEL_QUALSEMI_QS6612)
					sc->phyid = EPIC_QS6612_PHY;
				break;
			case MII_OUI_xxALTIMA:
				if (model == MII_MODEL_xxALTIMA_AC101)
					sc->phyid = EPIC_AC101_PHY;
				break;
			case MII_OUI_xxLEVEL1:
				if (model == MII_MODEL_xxLEVEL1_LXT970)
					sc->phyid = EPIC_LXT970_PHY;
				break;
			}
		}
	}

	/*
	 * Do PHY specific card setup
	 */

	/* Call this, to isolate all not selected PHYs and
	 * set up selected
	 */
	mii_mediachg(mii);

	/* Do our own setup */
	switch (sc->phyid) {
	case EPIC_QS6612_PHY:
		break;
	case EPIC_AC101_PHY:
		/* We have to powerup fiber tranceivers */
		if (IFM_SUBTYPE(media) == IFM_100_FX)
			sc->miicfg |= MIICFG_694_ENABLE;
		else
			sc->miicfg &= ~MIICFG_694_ENABLE;
		CSR_WRITE_4(sc, MIICFG, sc->miicfg);
	
		break;
	case EPIC_LXT970_PHY:
		/* We have to powerup fiber tranceivers */
		cfg = PHY_READ(sc->physc, MII_LXTPHY_CONFIG);
		if (IFM_SUBTYPE(media) == IFM_100_FX)
			cfg |= CONFIG_LEDC1 | CONFIG_LEDC0;
		else
			cfg &= ~(CONFIG_LEDC1 | CONFIG_LEDC0);
		PHY_WRITE(sc->physc, MII_LXTPHY_CONFIG, cfg);

		break;
	case EPIC_SERIAL:
		/* Select serial PHY, (10base2/BNC usually) */
		sc->miicfg |= MIICFG_694_ENABLE | MIICFG_SERIAL_ENABLE;
		CSR_WRITE_4(sc, MIICFG, sc->miicfg);

		/* There is no driver to fill this */
		mii->mii_media_active = media;
		mii->mii_media_status = 0;

		/* We need to call this manualy as i wasn't called
		 * in mii_mediachg()
		 */
		epic_miibus_statchg(sc->dev);

		break;
	default:
		device_printf(sc->dev, "ERROR! Unknown PHY selected\n");
		return (EINVAL);
	}

	return(0);
}

/*
 * Report current media status.
 */
static void
epic_ifmedia_sts(ifp, ifmr)
	struct ifnet *ifp;
	struct ifmediareq *ifmr;
{
	epic_softc_t *sc;
	struct mii_data *mii;
	struct ifmedia *ifm;

	sc = ifp->if_softc;
	mii = device_get_softc(sc->miibus);
	ifm = &mii->mii_media;

	/* Nothing should be selected if interface is down */
	if ((ifp->if_flags & IFF_UP) == 0) {
		ifmr->ifm_active = IFM_NONE;
		ifmr->ifm_status = 0;

		return;
	}

	/* Call underlying pollstat, if not serial PHY */
	if (sc->phyid != EPIC_SERIAL)
		mii_pollstat(mii);

	/* Simply copy media info */
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;

	return;
}

/*
 * Callback routine, called on media change.
 */
static void
epic_miibus_statchg(dev)
	device_t dev;
{
	epic_softc_t *sc;
	struct mii_data *mii;
	int media;

	sc = device_get_softc(dev);
	mii = device_get_softc(sc->miibus);
	media = mii->mii_media_active;

	sc->txcon &= ~(TXCON_LOOPBACK_MODE | TXCON_FULL_DUPLEX);

	/* If we are in full-duplex mode or loopback operation,
	 * we need to decouple receiver and transmitter.
	 */
	if (IFM_OPTIONS(media) & (IFM_FDX | IFM_LOOP))
 		sc->txcon |= TXCON_FULL_DUPLEX;

	/* On some cards we need manualy set fullduplex led */
	if (sc->cardid == SMC9432FTX ||
	    sc->cardid == SMC9432FTX_SC) {
		if (IFM_OPTIONS(media) & IFM_FDX)
			sc->miicfg |= MIICFG_694_ENABLE;
		else
			sc->miicfg &= ~MIICFG_694_ENABLE;

		CSR_WRITE_4(sc, MIICFG, sc->miicfg);
	}

	/* Update baudrate */
	if (IFM_SUBTYPE(media) == IFM_100_TX ||
	    IFM_SUBTYPE(media) == IFM_100_FX)
		sc->sc_if.if_baudrate = 100000000;
	else
		sc->sc_if.if_baudrate = 10000000;

	epic_stop_activity(sc);
	epic_set_tx_mode(sc);
	epic_start_activity(sc);

	return;
}

static void
epic_miibus_mediainit(dev)
	device_t dev;
{
	epic_softc_t *sc;
	struct mii_data *mii;
	struct ifmedia *ifm;
	int media;

	sc = device_get_softc(dev);
	mii = device_get_softc(sc->miibus);
	ifm = &mii->mii_media;

	/* Add Serial Media Interface if present, this applies to
	 * SMC9432BTX serie
	 */
	if (CSR_READ_4(sc, MIICFG) & MIICFG_PHY_PRESENT) {
		/* Store its instance */
		sc->serinst = mii->mii_instance++;

		/* Add as 10base2/BNC media */
		media = IFM_MAKEWORD(IFM_ETHER, IFM_10_2, 0, sc->serinst);
		ifmedia_add(ifm, media, 0, NULL);

		/* Report to user */
		device_printf(sc->dev, "serial PHY detected (10Base2/BNC)\n");
	}

	return;
}

/*
 * Reset chip, allocate rings, and update media.
 */
static int
epic_init(sc)
	epic_softc_t *sc;
{
	struct ifnet *ifp = &sc->sc_if;
	int s,i;

	s = splimp();

	/* If interface is already running, then we need not do anything */
	if (ifp->if_flags & IFF_RUNNING) {
		splx(s);
		return 0;
	}

	/* Soft reset the chip (we have to power up card before) */
	CSR_WRITE_4(sc, GENCTL, 0);
	CSR_WRITE_4(sc, GENCTL, GENCTL_SOFT_RESET);

	/*
	 * Reset takes 15 pci ticks which depends on PCI bus speed.
	 * Assuming it >= 33000000 hz, we have wait at least 495e-6 sec.
	 */
	DELAY(500);

	/* Wake up */
	CSR_WRITE_4(sc, GENCTL, 0);

	/* Workaround for Application Note 7-15 */
	for (i=0; i<16; i++) CSR_WRITE_4(sc, TEST1, TEST1_CLOCK_TEST);

	/* Initialize rings */
	if (epic_init_rings(sc)) {
		device_printf(sc->dev, "failed to init rings\n");
		splx(s);
		return -1;
	}	

	/* Give rings to EPIC */
	CSR_WRITE_4(sc, PRCDAR, vtophys(sc->rx_desc));
	CSR_WRITE_4(sc, PTCDAR, vtophys(sc->tx_desc));

	/* Put node address to EPIC */
	CSR_WRITE_4(sc, LAN0, ((u_int16_t *)sc->sc_macaddr)[0]);
	CSR_WRITE_4(sc, LAN1, ((u_int16_t *)sc->sc_macaddr)[1]);
	CSR_WRITE_4(sc, LAN2, ((u_int16_t *)sc->sc_macaddr)[2]);

	/* Set tx mode, includeing transmit threshold */
	epic_set_tx_mode(sc);

	/* Compute and set RXCON. */
	epic_set_rx_mode(sc);

	/* Set multicast table */
	epic_set_mc_table(sc);

	/* Enable interrupts by setting the interrupt mask. */
	CSR_WRITE_4(sc, INTMASK,
		INTSTAT_RCC  | /* INTSTAT_RQE | INTSTAT_OVW | INTSTAT_RXE | */
		/* INTSTAT_TXC | */ INTSTAT_TCC | INTSTAT_TQE | INTSTAT_TXU |
		INTSTAT_FATAL);

	/* Acknowledge all pending interrupts */
	CSR_WRITE_4(sc, INTSTAT, CSR_READ_4(sc, INTSTAT));

	/* Enable interrupts,  set for PCI read multiple and etc */
	CSR_WRITE_4(sc, GENCTL,
		GENCTL_ENABLE_INTERRUPT | GENCTL_MEMORY_READ_MULTIPLE |
		GENCTL_ONECOPY | GENCTL_RECEIVE_FIFO_THRESHOLD64);

	/* Mark interface running ... */
	if (ifp->if_flags & IFF_UP) ifp->if_flags |= IFF_RUNNING;
	else ifp->if_flags &= ~IFF_RUNNING;

	/* ... and free */
	ifp->if_flags &= ~IFF_OACTIVE;

	/* Start Rx process */
	epic_start_activity(sc);

	/* Set appropriate media */
	epic_ifmedia_upd(ifp);

	sc->stat_ch = timeout((timeout_t *)epic_stats_update, sc, hz);

	splx(s);

	return 0;
}

/*
 * Synopsis: calculate and set Rx mode. Chip must be in idle state to
 * access RXCON.
 */
static void
epic_set_rx_mode(sc)
	epic_softc_t *sc;
{
	u_int32_t 		flags = sc->sc_if.if_flags;
	u_int32_t 		rxcon = RXCON_DEFAULT;

#if defined(EPIC_EARLY_RX)
	rxcon |= RXCON_EARLY_RX;
#endif

	rxcon |= (flags & IFF_PROMISC) ? RXCON_PROMISCUOUS_MODE : 0;

	CSR_WRITE_4(sc, RXCON, rxcon);

	return;
}

/*
 * Synopsis: Set transmit control register. Chip must be in idle state to
 * access TXCON.
 */
static void
epic_set_tx_mode(sc)
	epic_softc_t *sc;
{
	if (sc->txcon & TXCON_EARLY_TRANSMIT_ENABLE)
		CSR_WRITE_4(sc, ETXTHR, sc->tx_threshold);

	CSR_WRITE_4(sc, TXCON, sc->txcon);
}

/*
 * Synopsis: Program multicast filter honoring IFF_ALLMULTI and IFF_PROMISC
 * flags. (Note, that setting PROMISC bit in EPIC's RXCON will only touch
 * individual frames, multicast filter must be manually programmed)
 *
 * Note: EPIC must be in idle state.
 */
static void
epic_set_mc_table(sc)
	epic_softc_t *sc;
{
	struct ifnet *ifp = &sc->sc_if;
	struct ifmultiaddr *ifma;
	u_int16_t filter[4];
	u_int8_t h;

	if (ifp->if_flags & (IFF_ALLMULTI | IFF_PROMISC)) {
		CSR_WRITE_4(sc, MC0, 0xFFFF);
		CSR_WRITE_4(sc, MC1, 0xFFFF);
		CSR_WRITE_4(sc, MC2, 0xFFFF);
		CSR_WRITE_4(sc, MC3, 0xFFFF);

		return;
	}

	filter[0] = 0;
	filter[1] = 0;
	filter[2] = 0;
	filter[3] = 0;

#if __FreeBSD_version < 500000
	LIST_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
#else
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
#endif
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		h = epic_calchash(LLADDR((struct sockaddr_dl *)ifma->ifma_addr));
		filter[h >> 4] |= 1 << (h & 0xF);
	}

	CSR_WRITE_4(sc, MC0, filter[0]);
	CSR_WRITE_4(sc, MC1, filter[1]);
	CSR_WRITE_4(sc, MC2, filter[2]);
	CSR_WRITE_4(sc, MC3, filter[3]);

	return;
}

/*
 * Synopsis: calculate EPIC's hash of multicast address.
 */
static u_int8_t
epic_calchash(addr)
	caddr_t addr;
{
	u_int32_t crc, carry;
	int i, j;
	u_int8_t c;

	/* Compute CRC for the address value. */
	crc = 0xFFFFFFFF; /* initial value */

	for (i = 0; i < 6; i++) {
		c = *(addr + i);
		for (j = 0; j < 8; j++) {
			carry = ((crc & 0x80000000) ? 1 : 0) ^ (c & 0x01);
			crc <<= 1;
			c >>= 1;
			if (carry)
				crc = (crc ^ 0x04c11db6) | carry;
		}
	}

	return ((crc >> 26) & 0x3F);
}


/*
 * Synopsis: Start receive process and transmit one, if they need.
 */
static void
epic_start_activity(sc)
	epic_softc_t *sc;
{
	/* Start rx process */
	CSR_WRITE_4(sc, COMMAND,
		COMMAND_RXQUEUED | COMMAND_START_RX |
		(sc->pending_txs?COMMAND_TXQUEUED:0));
}

/*
 * Synopsis: Completely stop Rx and Tx processes. If TQE is set additional
 * packet needs to be queued to stop Tx DMA.
 */
static void
epic_stop_activity(sc)
	epic_softc_t *sc;
{
	int status, i;

	/* Stop Tx and Rx DMA */
	CSR_WRITE_4(sc, COMMAND,
	    COMMAND_STOP_RX | COMMAND_STOP_RDMA | COMMAND_STOP_TDMA);

	/* Wait Rx and Tx DMA to stop (why 1 ms ??? XXX) */
	for (i=0; i<0x1000; i++) {
		status = CSR_READ_4(sc, INTSTAT) & (INTSTAT_TXIDLE | INTSTAT_RXIDLE);
		if (status == (INTSTAT_TXIDLE | INTSTAT_RXIDLE))
			break;
		DELAY(1);
	}

	/* Catch all finished packets */
	epic_rx_done(sc);
	epic_tx_done(sc);

	status = CSR_READ_4(sc, INTSTAT);

	if ((status & INTSTAT_RXIDLE) == 0)
		device_printf(sc->dev, "ERROR! Can't stop Rx DMA\n");

	if ((status & INTSTAT_TXIDLE) == 0)
		device_printf(sc->dev, "ERROR! Can't stop Tx DMA\n");

	/*
	 * May need to queue one more packet if TQE, this is rare
	 * but existing case.
	 */
	if ((status & INTSTAT_TQE) && !(status & INTSTAT_TXIDLE))
		(void) epic_queue_last_packet(sc);

}

/*
 * The EPIC transmitter may stuck in TQE state. It will not go IDLE until
 * a packet from current descriptor will be copied to internal RAM. We
 * compose a dummy packet here and queue it for transmission.
 *
 * XXX the packet will then be actually sent over network...
 */
static int
epic_queue_last_packet(sc)
	epic_softc_t *sc;
{
	struct epic_tx_desc *desc;
	struct epic_frag_list *flist;
	struct epic_tx_buffer *buf;
	struct mbuf *m0;
	int i;

	device_printf(sc->dev, "queue last packet\n");

	desc = sc->tx_desc + sc->cur_tx;
	flist = sc->tx_flist + sc->cur_tx;
	buf = sc->tx_buffer + sc->cur_tx;

	if ((desc->status & 0x8000) || (buf->mbuf != NULL))
		return (EBUSY);

	MGETHDR(m0, M_NOWAIT, MT_DATA);
	if (NULL == m0)
		return (ENOBUFS);

	/* Prepare mbuf */
	m0->m_len = min(MHLEN, ETHER_MIN_LEN-ETHER_CRC_LEN);
	flist->frag[0].fraglen = m0->m_len;
	m0->m_pkthdr.len = m0->m_len;
	m0->m_pkthdr.rcvif = &sc->sc_if;
	bzero(mtod(m0,caddr_t), m0->m_len);

	/* Fill fragments list */
	flist->frag[0].fraglen = m0->m_len;
	flist->frag[0].fragaddr = vtophys(mtod(m0, caddr_t));
	flist->numfrags = 1;

	/* Fill in descriptor */
	buf->mbuf = m0;
	sc->pending_txs++;
	sc->cur_tx = (sc->cur_tx + 1) & TX_RING_MASK;
	desc->control = 0x01;
	desc->txlength = max(m0->m_pkthdr.len,ETHER_MIN_LEN-ETHER_CRC_LEN);
	desc->status = 0x8000;

	/* Launch transmition */
	CSR_WRITE_4(sc, COMMAND, COMMAND_STOP_TDMA | COMMAND_TXQUEUED);

	/* Wait Tx DMA to stop (for how long??? XXX) */
	for (i=0; i<1000; i++) {
		if (CSR_READ_4(sc, INTSTAT) & INTSTAT_TXIDLE)
			break;
		DELAY(1);
	}

	if ((CSR_READ_4(sc, INTSTAT) & INTSTAT_TXIDLE) == 0)
		device_printf(sc->dev, "ERROR! can't stop Tx DMA (2)\n");
	else
		epic_tx_done(sc);

	return 0;
}

/*
 *  Synopsis: Shut down board and deallocates rings.
 */
static void
epic_stop(sc)
	epic_softc_t *sc;
{
	int s;

	s = splimp();

	sc->sc_if.if_timer = 0;

	untimeout((timeout_t *)epic_stats_update, sc, sc->stat_ch);

	/* Disable interrupts */
	CSR_WRITE_4(sc, INTMASK, 0);
	CSR_WRITE_4(sc, GENCTL, 0);

	/* Try to stop Rx and TX processes */
	epic_stop_activity(sc);

	/* Reset chip */
	CSR_WRITE_4(sc, GENCTL, GENCTL_SOFT_RESET);
	DELAY(1000);

	/* Make chip go to bed */
	CSR_WRITE_4(sc, GENCTL, GENCTL_POWER_DOWN);

	/* Free memory allocated for rings */
	epic_free_rings(sc);

	/* Mark as stoped */
	sc->sc_if.if_flags &= ~IFF_RUNNING;

	splx(s);
	return;
}

/*
 * Synopsis: This function should free all memory allocated for rings.
 */
static void
epic_free_rings(sc)
	epic_softc_t *sc;
{
	int i;

	for (i=0; i<RX_RING_SIZE; i++) {
		struct epic_rx_buffer *buf = sc->rx_buffer + i;
		struct epic_rx_desc *desc = sc->rx_desc + i;
		
		desc->status = 0;
		desc->buflength = 0;
		desc->bufaddr = 0;

		if (buf->mbuf) m_freem(buf->mbuf);
		buf->mbuf = NULL;
	}

	for (i=0; i<TX_RING_SIZE; i++) {
		struct epic_tx_buffer *buf = sc->tx_buffer + i;
		struct epic_tx_desc *desc = sc->tx_desc + i;

		desc->status = 0;
		desc->buflength = 0;
		desc->bufaddr = 0;

		if (buf->mbuf) m_freem(buf->mbuf);
		buf->mbuf = NULL;
	}
}

/*
 * Synopsis:  Allocates mbufs for Rx ring and point Rx descs to them.
 * Point Tx descs to fragment lists. Check that all descs and fraglists
 * are bounded and aligned properly.
 */
static int
epic_init_rings(sc)
	epic_softc_t *sc;
{
	int i;

	sc->cur_rx = sc->cur_tx = sc->dirty_tx = sc->pending_txs = 0;

	for (i = 0; i < RX_RING_SIZE; i++) {
		struct epic_rx_buffer *buf = sc->rx_buffer + i;
		struct epic_rx_desc *desc = sc->rx_desc + i;

		desc->status = 0;		/* Owned by driver */
		desc->next = vtophys(sc->rx_desc + ((i+1) & RX_RING_MASK));

		if ((desc->next & 3) ||
		    ((desc->next & PAGE_MASK) + sizeof *desc) > PAGE_SIZE) {
			epic_free_rings(sc);
			return EFAULT;
		}

		buf->mbuf = m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR);
		if( NULL == buf->mbuf ) {
			epic_free_rings(sc);
			return ENOBUFS;
		}
		buf->mbuf->m_len = buf->mbuf->m_pkthdr.len = MCLBYTES;
		m_adj(buf->mbuf, ETHER_ALIGN);

		desc->bufaddr = vtophys( mtod(buf->mbuf,caddr_t) );
		desc->buflength = buf->mbuf->m_len;/* Max RX buffer length */
		desc->status = 0x8000;		/* Set owner bit to NIC */
	}

	for (i = 0; i < TX_RING_SIZE; i++) {
		struct epic_tx_buffer *buf = sc->tx_buffer + i;
		struct epic_tx_desc *desc = sc->tx_desc + i;

		desc->status = 0;
		desc->next = vtophys(sc->tx_desc + ((i+1) & TX_RING_MASK));

		if ((desc->next & 3) ||
		    ((desc->next & PAGE_MASK) + sizeof *desc) > PAGE_SIZE) {
			epic_free_rings(sc);
			return EFAULT;
		}

		buf->mbuf = NULL;
		desc->bufaddr = vtophys(sc->tx_flist + i);

		if ((desc->bufaddr & 3) ||
		    ((desc->bufaddr & PAGE_MASK) + sizeof(struct epic_frag_list)) > PAGE_SIZE) {
			epic_free_rings(sc);
			return EFAULT;
		}
	}

	return 0;
}

/*
 * EEPROM operation functions
 */
static void
epic_write_eepromreg(sc, val)
	epic_softc_t *sc;
	u_int8_t val;
{
	u_int16_t i;

	CSR_WRITE_1(sc, EECTL, val);

	for (i=0; i<0xFF; i++)
		if ((CSR_READ_1(sc, EECTL) & 0x20) == 0) break;

	return;
}

static u_int8_t
epic_read_eepromreg(sc)
	epic_softc_t *sc;
{
	return CSR_READ_1(sc, EECTL);
}

static u_int8_t
epic_eeprom_clock(sc, val)
	epic_softc_t *sc;
	u_int8_t val;
{
	epic_write_eepromreg(sc, val);
	epic_write_eepromreg(sc, (val | 0x4));
	epic_write_eepromreg(sc, val);
	
	return epic_read_eepromreg(sc);
}

static void
epic_output_eepromw(sc, val)
	epic_softc_t *sc;
	u_int16_t val;
{
	int i;

	for (i = 0xF; i >= 0; i--) {
		if (val & (1 << i))
			epic_eeprom_clock(sc, 0x0B);
		else
			epic_eeprom_clock(sc, 0x03);
	}
}

static u_int16_t
epic_input_eepromw(sc)
	epic_softc_t *sc;
{
	u_int16_t retval = 0;
	int i;

	for (i = 0xF; i >= 0; i--) {	
		if (epic_eeprom_clock(sc, 0x3) & 0x10)
			retval |= (1 << i);
	}

	return retval;
}

static int
epic_read_eeprom(sc, loc)
	epic_softc_t *sc;
	u_int16_t loc;
{
	u_int16_t dataval;
	u_int16_t read_cmd;

	epic_write_eepromreg(sc, 3);

	if (epic_read_eepromreg(sc) & 0x40)
		read_cmd = (loc & 0x3F) | 0x180;
	else
		read_cmd = (loc & 0xFF) | 0x600;

	epic_output_eepromw(sc, read_cmd);

	dataval = epic_input_eepromw(sc);

	epic_write_eepromreg(sc, 1);
	
	return dataval;
}

/*
 * Here goes MII read/write routines
 */
static int
epic_read_phy_reg(sc, phy, reg)
	epic_softc_t *sc;
	int phy, reg;
{
	int i;

	CSR_WRITE_4(sc, MIICTL, ((reg << 4) | (phy << 9) | 0x01));

	for (i = 0; i < 0x100; i++) {
		if ((CSR_READ_4(sc, MIICTL) & 0x01) == 0) break;
		DELAY(1);
	}

	return (CSR_READ_4(sc, MIIDATA));
}

static void
epic_write_phy_reg(sc, phy, reg, val)
	epic_softc_t *sc;
	int phy, reg, val;
{
	int i;

	CSR_WRITE_4(sc, MIIDATA, val);
	CSR_WRITE_4(sc, MIICTL, ((reg << 4) | (phy << 9) | 0x02));

	for(i=0;i<0x100;i++) {
		if ((CSR_READ_4(sc, MIICTL) & 0x02) == 0) break;
		DELAY(1);
	}

	return;
}

static int
epic_miibus_readreg(dev, phy, reg)
	device_t dev;
	int phy, reg;
{
	epic_softc_t *sc;

	sc = device_get_softc(dev);

	return (PHY_READ_2(sc, phy, reg));
}

static int
epic_miibus_writereg(dev, phy, reg, data)
	device_t dev;
	int phy, reg, data;
{
	epic_softc_t *sc;

	sc = device_get_softc(dev);

	PHY_WRITE_2(sc, phy, reg, data);

	return (0);
}
