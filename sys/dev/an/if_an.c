/*
 * Copyright (c) 1997, 1998, 1999
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
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

/*
 * Aironet 4500/4800 802.11 PCMCIA/ISA/PCI driver for FreeBSD.
 *
 * Written by Bill Paul <wpaul@ctr.columbia.edu>
 * Electrical Engineering Department
 * Columbia University, New York City
 */

/*
 * The Aironet 4500/4800 series cards some in PCMCIA, ISA and PCI form.
 * This driver supports all three device types (PCI devices are supported
 * through an extra PCI shim: /sys/pci/if_an_p.c). ISA devices can be
 * supported either using hard-coded IO port/IRQ settings or via Plug
 * and Play. The 4500 series devices support 1Mbps and 2Mbps data rates.
 * The 4800 devices support 1, 2, 5.5 and 11Mbps rates.
 *
 * Like the WaveLAN/IEEE cards, the Aironet NICs are all essentially
 * PCMCIA devices. The ISA and PCI cards are a combination of a PCMCIA
 * device and a PCMCIA to ISA or PCMCIA to PCI adapter card. There are
 * a couple of important differences though:
 *
 * - Lucent doesn't currently offer a PCI card, however Aironet does
 * - Lucent ISA card looks to the host like a PCMCIA controller with
 *   a PCMCIA WaveLAN card inserted. This means that even desktop
 *   machines need to be configured with PCMCIA support in order to
 *   use WaveLAN/IEEE ISA cards. The Aironet cards on the other hand
 *   actually look like normal ISA and PCI devices to the host, so
 *   no PCMCIA controller support is needed
 *
 * The latter point results in a small gotcha. The Aironet PCMCIA
 * cards can be configured for one of two operating modes depending
 * on how the Vpp1 and Vpp2 programming voltages are set when the
 * card is activated. In order to put the card in proper PCMCIA
 * operation (where the CIS table is visible and the interface is
 * programmed for PCMCIA operation), both Vpp1 and Vpp2 have to be
 * set to 5 volts. FreeBSD by default doesn't set the Vpp voltages,
 * which leaves the card in ISA/PCI mode, which prevents it from
 * being activated as an PCMCIA device. Consequently, /sys/pccard/pccard.c
 * has to be patched slightly in order to enable the Vpp voltages in
 * order to make the Aironet PCMCIA cards work.
 *
 * Note that some PCMCIA controller software packages for Windows NT
 * fail to set the voltages as well.
 * 
 * The Aironet devices can operate in both station mode and access point
 * mode. Typically, when programmed for station mode, the card can be set
 * to automatically perform encapsulation/decapsulation of Ethernet II
 * and 802.3 frames within 802.11 frames so that the host doesn't have
 * to do it itself. This driver doesn't program the card that way: the
 * driver handles all of the encapsulation/decapsulation itself.
 */

#include "opt_inet.h"

#ifdef INET
#define ANCACHE			/* enable signal strength cache */
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/ucred.h>
#include <sys/socket.h>
#ifdef ANCACHE
#include <sys/syslog.h>
#include <sys/sysctl.h>
#endif

#include <sys/module.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <sys/mutex.h>
#include <machine/resource.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_types.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#endif

#include <net/bpf.h>

#include <machine/md_var.h>

#include <dev/an/if_aironet_ieee.h>
#include <dev/an/if_anreg.h>

#if !defined(lint)
static const char rcsid[] =
  "$FreeBSD$";
#endif

/* These are global because we need them in sys/pci/if_an_p.c. */
static void an_reset		__P((struct an_softc *));
static int an_ioctl		__P((struct ifnet *, u_long, caddr_t));
static void an_init		__P((void *));
static int an_init_tx_ring	__P((struct an_softc *));
static void an_start		__P((struct ifnet *));
static void an_watchdog		__P((struct ifnet *));
static void an_rxeof		__P((struct an_softc *));
static void an_txeof		__P((struct an_softc *, int));

static void an_promisc		__P((struct an_softc *, int));
static int an_cmd		__P((struct an_softc *, int, int));
static int an_read_record	__P((struct an_softc *, struct an_ltv_gen *));
static int an_write_record	__P((struct an_softc *, struct an_ltv_gen *));
static int an_read_data		__P((struct an_softc *, int,
					int, caddr_t, int));
static int an_write_data	__P((struct an_softc *, int,
					int, caddr_t, int));
static int an_seek		__P((struct an_softc *, int, int, int));
static int an_alloc_nicmem	__P((struct an_softc *, int, int *));
static void an_stats_update	__P((void *));
static void an_setdef		__P((struct an_softc *, struct an_req *));
#ifdef ANCACHE
static void an_cache_store	__P((struct an_softc *, struct ether_header *,
					struct mbuf *, unsigned short));
#endif

/* 
 * We probe for an Aironet 4500/4800 card by attempting to
 * read the default SSID list. On reset, the first entry in
 * the SSID list will contain the name "tsunami." If we don't
 * find this, then there's no card present.
 */
int an_probe(dev)
	device_t		dev;
{
        struct an_softc *sc = device_get_softc(dev);
	struct an_ltv_ssidlist	ssid;
	int	error;

	bzero((char *)&ssid, sizeof(ssid));

	error = an_alloc_port(dev, 0, AN_IOSIZ);
	if (error)
		return (0);

	/* can't do autoprobing */
	if (rman_get_start(sc->port_res) == -1)
		return(0);

	/*
	 * We need to fake up a softc structure long enough
	 * to be able to issue commands and call some of the
	 * other routines.
	 */
	sc->an_bhandle = rman_get_bushandle(sc->port_res);
	sc->an_btag = rman_get_bustag(sc->port_res);
	sc->an_unit = device_get_unit(dev);

	ssid.an_len = sizeof(ssid);
	ssid.an_type = AN_RID_SSIDLIST;

        /* Make sure interrupts are disabled. */
        CSR_WRITE_2(sc, AN_INT_EN, 0);
        CSR_WRITE_2(sc, AN_EVENT_ACK, 0xFFFF);

	an_reset(sc);

	if (an_cmd(sc, AN_CMD_READCFG, 0))
		return(0);

	if (an_read_record(sc, (struct an_ltv_gen *)&ssid))
		return(0);

	/* See if the ssid matches what we expect. */
	if (strcmp(ssid.an_ssid1, AN_DEF_SSID))
		return(0);
	
	return(AN_IOSIZ);
}

/*
 * Allocate a port resource with the given resource id.
 */
int
an_alloc_port(dev, rid, size)
	device_t dev;
	int rid;
	int size;
{
	struct an_softc *sc = device_get_softc(dev);
	struct resource *res;

	res = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid,
				 0ul, ~0ul, size, RF_ACTIVE);
	if (res) {
		sc->port_rid = rid;
		sc->port_res = res;
		return (0);
	} else {
		return (ENOENT);
	}
}

/*
 * Allocate an irq resource with the given resource id.
 */
int
an_alloc_irq(dev, rid, flags)
	device_t dev;
	int rid;
	int flags;
{
	struct an_softc *sc = device_get_softc(dev);
	struct resource *res;

	res = bus_alloc_resource(dev, SYS_RES_IRQ, &rid,
				 0ul, ~0ul, 1, (RF_ACTIVE | flags));
	if (res) {
		sc->irq_rid = rid;
		sc->irq_res = res;
		return (0);
	} else {
		return (ENOENT);
	}
}

/*
 * Release all resources
 */
void
an_release_resources(dev)
	device_t dev;
{
	struct an_softc *sc = device_get_softc(dev);

	if (sc->port_res) {
		bus_release_resource(dev, SYS_RES_IOPORT,
				     sc->port_rid, sc->port_res);
		sc->port_res = 0;
	}
	if (sc->irq_res) {
		bus_release_resource(dev, SYS_RES_IRQ,
				     sc->irq_rid, sc->irq_res);
		sc->irq_res = 0;
	}
}

int an_attach(sc, unit, flags)
	struct an_softc *sc;
	int unit;
	int flags;
{
	struct ifnet		*ifp = &sc->arpcom.ac_if;

	mtx_init(&sc->an_mtx, device_get_nameunit(sc->an_dev), MTX_DEF);
	AN_LOCK(sc);

	sc->an_gone = 0;
	sc->an_associated = 0;

	/* Reset the NIC. */
	an_reset(sc);

	/* Load factory config */
	if (an_cmd(sc, AN_CMD_READCFG, 0)) {
		printf("an%d: failed to load config data\n", sc->an_unit);
		AN_UNLOCK(sc);
		mtx_destroy(&sc->an_mtx);
		return(EIO);
	}

	/* Read the current configuration */
	sc->an_config.an_type = AN_RID_GENCONFIG;
	sc->an_config.an_len = sizeof(struct an_ltv_genconfig);
	if (an_read_record(sc, (struct an_ltv_gen *)&sc->an_config)) {
		printf("an%d: read record failed\n", sc->an_unit);
		AN_UNLOCK(sc);
		mtx_destroy(&sc->an_mtx);
		return(EIO);
	}

	/* Read the card capabilities */
	sc->an_caps.an_type = AN_RID_CAPABILITIES;
	sc->an_caps.an_len = sizeof(struct an_ltv_caps);
	if (an_read_record(sc, (struct an_ltv_gen *)&sc->an_caps)) {
		printf("an%d: read record failed\n", sc->an_unit);
		AN_UNLOCK(sc);
		mtx_destroy(&sc->an_mtx);
		return(EIO);
	}

	/* Read ssid list */
	sc->an_ssidlist.an_type = AN_RID_SSIDLIST;
	sc->an_ssidlist.an_len = sizeof(struct an_ltv_ssidlist);
	if (an_read_record(sc, (struct an_ltv_gen *)&sc->an_ssidlist)) {
		printf("an%d: read record failed\n", sc->an_unit);
		AN_UNLOCK(sc);
		mtx_destroy(&sc->an_mtx);
		return(EIO);
	}

	/* Read AP list */
	sc->an_aplist.an_type = AN_RID_APLIST;
	sc->an_aplist.an_len = sizeof(struct an_ltv_aplist);
	if (an_read_record(sc, (struct an_ltv_gen *)&sc->an_aplist)) {
		printf("an%d: read record failed\n", sc->an_unit);
		AN_UNLOCK(sc);
		mtx_destroy(&sc->an_mtx);
		return(EIO);
	}

	bcopy((char *)&sc->an_caps.an_oemaddr,
	   (char *)&sc->arpcom.ac_enaddr, ETHER_ADDR_LEN);

	printf("an%d: Ethernet address: %6D\n", sc->an_unit,
	    sc->arpcom.ac_enaddr, ":");

	ifp->if_softc = sc;
	ifp->if_unit = sc->an_unit = unit;
	ifp->if_name = "an";
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = an_ioctl;
	ifp->if_output = ether_output;
	ifp->if_start = an_start;
	ifp->if_watchdog = an_watchdog;
	ifp->if_init = an_init;
	ifp->if_baudrate = 10000000;
	ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;

	bzero(sc->an_config.an_nodename, sizeof(sc->an_config.an_nodename));
	bcopy(AN_DEFAULT_NODENAME, sc->an_config.an_nodename,
	    sizeof(AN_DEFAULT_NODENAME) - 1);

	bzero(sc->an_ssidlist.an_ssid1, sizeof(sc->an_ssidlist.an_ssid1));
	bcopy(AN_DEFAULT_NETNAME, sc->an_ssidlist.an_ssid1,
	    sizeof(AN_DEFAULT_NETNAME) - 1);
	sc->an_ssidlist.an_ssid1_len = strlen(AN_DEFAULT_NETNAME);

	sc->an_config.an_opmode =
	    AN_OPMODE_IBSS_ADHOC;

	sc->an_tx_rate = 0;
	bzero((char *)&sc->an_stats, sizeof(sc->an_stats));

	/*
	 * Call MI attach routine.
	 */
	ether_ifattach(ifp, ETHER_BPF_SUPPORTED);
	callout_handle_init(&sc->an_stat_ch);
	AN_UNLOCK(sc);

	return(0);
}

static void an_rxeof(sc)
	struct an_softc		*sc;
{
	struct ifnet		*ifp;
	struct ether_header	*eh;
#ifdef ANCACHE
	struct an_rxframe	rx_frame;
#endif
	struct an_rxframe_802_3	rx_frame_802_3;
	struct mbuf		*m;
	int			id, error = 0;

	ifp = &sc->arpcom.ac_if;

	id = CSR_READ_2(sc, AN_RX_FID);

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL) {
		ifp->if_ierrors++;
		return;
	}
	MCLGET(m, M_DONTWAIT);
	if (!(m->m_flags & M_EXT)) {
		m_freem(m);
		ifp->if_ierrors++;
		return;
	}

	m->m_pkthdr.rcvif = ifp;

	eh = mtod(m, struct ether_header *);

#ifdef ANCACHE
	/* Read NIC frame header */
	if (an_read_data(sc, id, 0, (caddr_t)&rx_frame, sizeof(rx_frame))) {
		ifp->if_ierrors++;
		return;
	}
#endif
	/* Read in the 802_3 frame header */
	if (an_read_data(sc, id, 0x34, (caddr_t)&rx_frame_802_3, 
			 sizeof(rx_frame_802_3))) {
		ifp->if_ierrors++;
		return;
	}

	if (rx_frame_802_3.an_rx_802_3_status != 0) {
		ifp->if_ierrors++;
		return;
	}

	/* Check for insane frame length */
	if (rx_frame_802_3.an_rx_802_3_payload_len > MCLBYTES) {
		ifp->if_ierrors++;
		return;
	}

	m->m_pkthdr.len = m->m_len =
	    rx_frame_802_3.an_rx_802_3_payload_len + 12;

	
	bcopy((char *)&rx_frame_802_3.an_rx_dst_addr,
	    (char *)&eh->ether_dhost, ETHER_ADDR_LEN);
	bcopy((char *)&rx_frame_802_3.an_rx_src_addr,
	    (char *)&eh->ether_shost, ETHER_ADDR_LEN);

	/* in mbuf header type is just before payload */
	error = an_read_data(sc, id, 0x44, (caddr_t)&(eh->ether_type), 
			     rx_frame_802_3.an_rx_802_3_payload_len);

	if (error) {
		m_freem(m);
		ifp->if_ierrors++;
		return;
	}

	ifp->if_ipackets++;

	/* Receive packet. */
	m_adj(m, sizeof(struct ether_header));
#ifdef ANCACHE
	an_cache_store(sc, eh, m, rx_frame.an_rx_signal_strength);
#endif
	ether_input(ifp, eh, m);
}

static void an_txeof(sc, status)
	struct an_softc		*sc;
	int			status;
{
	struct ifnet		*ifp;
	int			id;

	ifp = &sc->arpcom.ac_if;

	ifp->if_timer = 0;
	ifp->if_flags &= ~IFF_OACTIVE;

	id = CSR_READ_2(sc, AN_TX_CMP_FID);

	if (status & AN_EV_TX_EXC) {
		ifp->if_oerrors++;
	} else
		ifp->if_opackets++;

	if (id != sc->an_rdata.an_tx_ring[sc->an_rdata.an_tx_cons])
		printf("an%d: id mismatch: expected %x, got %x\n",
		    sc->an_unit,
		    sc->an_rdata.an_tx_ring[sc->an_rdata.an_tx_cons], id);

	sc->an_rdata.an_tx_ring[sc->an_rdata.an_tx_cons] = 0;
	AN_INC(sc->an_rdata.an_tx_cons, AN_TX_RING_CNT);

	return;
}

/*
 * We abuse the stats updater to check the current NIC status. This
 * is important because we don't want to allow transmissions until
 * the NIC has synchronized to the current cell (either as the master
 * in an ad-hoc group, or as a station connected to an access point).
 */
void an_stats_update(xsc)
	void			*xsc;
{
	struct an_softc		*sc;
	struct ifnet		*ifp;

	sc = xsc;
	AN_LOCK(sc);
	ifp = &sc->arpcom.ac_if;

	sc->an_status.an_type = AN_RID_STATUS;
	sc->an_status.an_len = sizeof(struct an_ltv_status);
	an_read_record(sc, (struct an_ltv_gen *)&sc->an_status);

	if (sc->an_status.an_opmode & AN_STATUS_OPMODE_IN_SYNC)
		sc->an_associated = 1;
	else
		sc->an_associated = 0;

	/* Don't do this while we're transmitting */
	if (ifp->if_flags & IFF_OACTIVE) {
		sc->an_stat_ch = timeout(an_stats_update, sc, hz);
		AN_UNLOCK(sc);
		return;
	}

	sc->an_stats.an_len = sizeof(struct an_ltv_stats);
	sc->an_stats.an_type = AN_RID_32BITS_CUM;
	an_read_record(sc, (struct an_ltv_gen *)&sc->an_stats.an_len);

	sc->an_stat_ch = timeout(an_stats_update, sc, hz);
	AN_UNLOCK(sc);

	return;
}

void an_intr(xsc)
	void			*xsc;
{
	struct an_softc		*sc;
	struct ifnet		*ifp;
	u_int16_t		status;

	sc = (struct an_softc*)xsc;

	AN_LOCK(sc);

	if (sc->an_gone) {
		AN_UNLOCK(sc);
		return;
	}

	ifp = &sc->arpcom.ac_if;

	if (!(ifp->if_flags & IFF_UP)) {
		CSR_WRITE_2(sc, AN_EVENT_ACK, 0xFFFF);
		CSR_WRITE_2(sc, AN_INT_EN, 0);
		AN_UNLOCK(sc);
		return;
	}

	/* Disable interrupts. */
	CSR_WRITE_2(sc, AN_INT_EN, 0);

	status = CSR_READ_2(sc, AN_EVENT_STAT);
	CSR_WRITE_2(sc, AN_EVENT_ACK, ~AN_INTRS);

	if (status & AN_EV_AWAKE) {
		CSR_WRITE_2(sc, AN_EVENT_ACK, AN_EV_AWAKE);
	}

	if (status & AN_EV_LINKSTAT) {
		if (CSR_READ_2(sc, AN_LINKSTAT) == AN_LINKSTAT_ASSOCIATED)
			sc->an_associated = 1;
		else
			sc->an_associated = 0;
		CSR_WRITE_2(sc, AN_EVENT_ACK, AN_EV_LINKSTAT);
	}

	if (status & AN_EV_RX) {
		an_rxeof(sc);
		CSR_WRITE_2(sc, AN_EVENT_ACK, AN_EV_RX);
	}

	if (status & AN_EV_TX) {
		an_txeof(sc, status);
		CSR_WRITE_2(sc, AN_EVENT_ACK, AN_EV_TX);
	}

	if (status & AN_EV_TX_EXC) {
		an_txeof(sc, status);
		CSR_WRITE_2(sc, AN_EVENT_ACK, AN_EV_TX_EXC);
	}

	if (status & AN_EV_ALLOC)
		CSR_WRITE_2(sc, AN_EVENT_ACK, AN_EV_ALLOC);

	/* Re-enable interrupts. */
	CSR_WRITE_2(sc, AN_INT_EN, AN_INTRS);

	if (ifp->if_snd.ifq_head != NULL)
		an_start(ifp);

	AN_UNLOCK(sc);

	return;
}

static int an_cmd(sc, cmd, val)
	struct an_softc		*sc;
	int			cmd;
	int			val;
{
	int			i, s = 0;

	CSR_WRITE_2(sc, AN_PARAM0, val);
	CSR_WRITE_2(sc, AN_PARAM1, 0);
	CSR_WRITE_2(sc, AN_PARAM2, 0);
	CSR_WRITE_2(sc, AN_COMMAND, cmd);

	for (i = 0; i < AN_TIMEOUT; i++) {
		if (CSR_READ_2(sc, AN_EVENT_STAT) & AN_EV_CMD)
			break;
		else {
			if (CSR_READ_2(sc, AN_COMMAND) == cmd)
				CSR_WRITE_2(sc, AN_COMMAND, cmd);
		}
	}

	for (i = 0; i < AN_TIMEOUT; i++) {
		CSR_READ_2(sc, AN_RESP0);
		CSR_READ_2(sc, AN_RESP1);
		CSR_READ_2(sc, AN_RESP2);
		s = CSR_READ_2(sc, AN_STATUS);
		if ((s & AN_STAT_CMD_CODE) == (cmd & AN_STAT_CMD_CODE))
			break;
	}

	/* Ack the command */
	CSR_WRITE_2(sc, AN_EVENT_ACK, AN_EV_CMD);

	if (CSR_READ_2(sc, AN_COMMAND) & AN_CMD_BUSY)
		CSR_WRITE_2(sc, AN_EVENT_ACK, AN_EV_CLR_STUCK_BUSY);

	if (i == AN_TIMEOUT)
		return(ETIMEDOUT);

	return(0);
}

/*
 * This reset sequence may look a little strange, but this is the
 * most reliable method I've found to really kick the NIC in the
 * head and force it to reboot correctly.
 */
static void an_reset(sc)
	struct an_softc		*sc;
{
	if (sc->an_gone)
		return;
        
	an_cmd(sc, AN_CMD_ENABLE, 0);
	an_cmd(sc, AN_CMD_FW_RESTART, 0);
	an_cmd(sc, AN_CMD_NOOP2, 0);

	if (an_cmd(sc, AN_CMD_FORCE_SYNCLOSS, 0) == ETIMEDOUT)
		printf("an%d: reset failed\n", sc->an_unit);

	an_cmd(sc, AN_CMD_DISABLE, 0);

	return;
}

/*
 * Read an LTV record from the NIC.
 */
static int an_read_record(sc, ltv)
	struct an_softc		*sc;
	struct an_ltv_gen	*ltv;
{
	u_int16_t		*ptr;
	int			i, len;

	if (ltv->an_len == 0 || ltv->an_type == 0)
		return(EINVAL);

	/* Tell the NIC to enter record read mode. */
	if (an_cmd(sc, AN_CMD_ACCESS|AN_ACCESS_READ, ltv->an_type)) {
		printf("an%d: RID access failed\n", sc->an_unit);
		return(EIO);
	}

	/* Seek to the record. */
	if (an_seek(sc, ltv->an_type, 0, AN_BAP1)) {
		printf("an%d: seek to record failed\n", sc->an_unit);
		return(EIO);
	}

	/*
	 * Read the length and record type and make sure they
	 * match what we expect (this verifies that we have enough
	 * room to hold all of the returned data).
	 */
	len = CSR_READ_2(sc, AN_DATA1);
	if (len > ltv->an_len) {
		printf("an%d: record length mismatch -- expected %d, "
		    "got %d\n", sc->an_unit, ltv->an_len, len);
		return(ENOSPC);
	}

	ltv->an_len = len;

	/* Now read the data. */
	ptr = &ltv->an_val;
	for (i = 0; i < (ltv->an_len - 1) >> 1; i++)
		ptr[i] = CSR_READ_2(sc, AN_DATA1);

	return(0);
}

/*
 * Same as read, except we inject data instead of reading it.
 */
static int an_write_record(sc, ltv)
	struct an_softc		*sc;
	struct an_ltv_gen	*ltv;
{
	u_int16_t		*ptr;
	int			i;

	if (an_cmd(sc, AN_CMD_ACCESS|AN_ACCESS_READ, ltv->an_type))
		return(EIO);

	if (an_seek(sc, ltv->an_type, 0, AN_BAP1))
		return(EIO);

	CSR_WRITE_2(sc, AN_DATA1, ltv->an_len);

	ptr = &ltv->an_val;
	for (i = 0; i < (ltv->an_len - 1) >> 1; i++)
		CSR_WRITE_2(sc, AN_DATA1, ptr[i]);

	if (an_cmd(sc, AN_CMD_ACCESS|AN_ACCESS_WRITE, ltv->an_type))
		return(EIO);

	return(0);
}

static int an_seek(sc, id, off, chan)
	struct an_softc		*sc;
	int			id, off, chan;
{
	int			i;
	int			selreg, offreg;

	switch (chan) {
	case AN_BAP0:
		selreg = AN_SEL0;
		offreg = AN_OFF0;
		break;
	case AN_BAP1:
		selreg = AN_SEL1;
		offreg = AN_OFF1;
		break;
	default:
		printf("an%d: invalid data path: %x\n", sc->an_unit, chan);
		return(EIO);
	}

	CSR_WRITE_2(sc, selreg, id);
	CSR_WRITE_2(sc, offreg, off);

	for (i = 0; i < AN_TIMEOUT; i++) {
		if (!(CSR_READ_2(sc, offreg) & (AN_OFF_BUSY|AN_OFF_ERR)))
			break;
	}

	if (i == AN_TIMEOUT)
		return(ETIMEDOUT);

	return(0);
}

static int an_read_data(sc, id, off, buf, len)
	struct an_softc		*sc;
	int			id, off;
	caddr_t			buf;
	int			len;
{
	int			i;
	u_int16_t		*ptr;
	u_int8_t		*ptr2;

	if (off != -1) {
		if (an_seek(sc, id, off, AN_BAP1))
			return(EIO);
	}

	ptr = (u_int16_t *)buf;
	for (i = 0; i < len / 2; i++)
		ptr[i] = CSR_READ_2(sc, AN_DATA1);
	i*=2;
	if (i<len){
	        ptr2 = (u_int8_t *)buf;
	        ptr2[i] = CSR_READ_1(sc, AN_DATA1);
	}

	return(0);
}

static int an_write_data(sc, id, off, buf, len)
	struct an_softc		*sc;
	int			id, off;
	caddr_t			buf;
	int			len;
{
	int			i;
	u_int16_t		*ptr;
	u_int8_t		*ptr2;

	if (off != -1) {
		if (an_seek(sc, id, off, AN_BAP0))
			return(EIO);
	}

	ptr = (u_int16_t *)buf;
	for (i = 0; i < (len / 2); i++)
		CSR_WRITE_2(sc, AN_DATA0, ptr[i]);
	i*=2;
	if (i<len){
	        ptr2 = (u_int8_t *)buf;
	        CSR_WRITE_1(sc, AN_DATA0, ptr2[i]);
	}

	return(0);
}

/*
 * Allocate a region of memory inside the NIC and zero
 * it out.
 */
static int an_alloc_nicmem(sc, len, id)
	struct an_softc		*sc;
	int			len;
	int			*id;
{
	int			i;

	if (an_cmd(sc, AN_CMD_ALLOC_MEM, len)) {
		printf("an%d: failed to allocate %d bytes on NIC\n",
		    sc->an_unit, len);
		return(ENOMEM);
	}

	for (i = 0; i < AN_TIMEOUT; i++) {
		if (CSR_READ_2(sc, AN_EVENT_STAT) & AN_EV_ALLOC)
			break;
	}

	if (i == AN_TIMEOUT)
		return(ETIMEDOUT);

	CSR_WRITE_2(sc, AN_EVENT_ACK, AN_EV_ALLOC);
	*id = CSR_READ_2(sc, AN_ALLOC_FID);

	if (an_seek(sc, *id, 0, AN_BAP0))
		return(EIO);

	for (i = 0; i < len / 2; i++)
		CSR_WRITE_2(sc, AN_DATA0, 0);

	return(0);
}

static void an_setdef(sc, areq)
	struct an_softc		*sc;
	struct an_req		*areq;
{
	struct sockaddr_dl	*sdl;
	struct ifaddr		*ifa;
	struct ifnet		*ifp;
	struct an_ltv_genconfig	*cfg;
	struct an_ltv_ssidlist	*ssid;
	struct an_ltv_aplist	*ap;
	struct an_ltv_gen	*sp;

	ifp = &sc->arpcom.ac_if;

	switch (areq->an_type) {
	case AN_RID_GENCONFIG:
		cfg = (struct an_ltv_genconfig *)areq;

		ifa = ifnet_addrs[ifp->if_index - 1];
		sdl = (struct sockaddr_dl *)ifa->ifa_addr;
		bcopy((char *)&cfg->an_macaddr, (char *)&sc->arpcom.ac_enaddr,
		    ETHER_ADDR_LEN);
		bcopy((char *)&cfg->an_macaddr, LLADDR(sdl), ETHER_ADDR_LEN);

		bcopy((char *)cfg, (char *)&sc->an_config,
			sizeof(struct an_ltv_genconfig));
		break;
	case AN_RID_SSIDLIST:
		ssid = (struct an_ltv_ssidlist *)areq;
		bcopy((char *)ssid, (char *)&sc->an_ssidlist,
			sizeof(struct an_ltv_ssidlist));
		break;
	case AN_RID_APLIST:
		ap = (struct an_ltv_aplist *)areq;
		bcopy((char *)ap, (char *)&sc->an_aplist,
			sizeof(struct an_ltv_aplist));
		break;
	case AN_RID_TX_SPEED:
		sp = (struct an_ltv_gen *)areq;
		sc->an_tx_rate = sp->an_val;
		break;
	default:
		printf("an%d: unknown RID: %x\n", sc->an_unit, areq->an_type);
		return;
		break;
	}


	/* Reinitialize the card. */
	if (ifp->if_flags & IFF_UP)
		an_init(sc);

	return;
}

/*
 * We can't change the NIC configuration while the MAC is enabled,
 * so in order to turn on RX monitor mode, we have to turn the MAC
 * off first.
 */
static void an_promisc(sc, promisc)
	struct an_softc		*sc;
	int			promisc;
{
	/* Disable the MAC. */
	an_cmd(sc, AN_CMD_DISABLE, 0);

	/* Set RX mode. */
	if (promisc && 
	    !(sc->an_config.an_rxmode & AN_RXMODE_LAN_MONITOR_CURBSS)
	    ) {
		sc->an_rxmode = sc->an_config.an_rxmode;
		sc->an_config.an_rxmode |=
		    AN_RXMODE_LAN_MONITOR_CURBSS;
	} else {
		sc->an_config.an_rxmode = sc->an_rxmode;
	}
 
	/* Transfer the configuration to the NIC */
	sc->an_config.an_len = sizeof(struct an_ltv_genconfig);
	sc->an_config.an_type = AN_RID_GENCONFIG;
	if (an_write_record(sc, (struct an_ltv_gen *)&sc->an_config)) {
		printf("an%d: failed to set configuration\n", sc->an_unit);
		return;
	}
	/* Turn the MAC back on. */
	an_cmd(sc, AN_CMD_ENABLE, 0);

	return;
}

static int an_ioctl(ifp, command, data)
	struct ifnet		*ifp;
	u_long			command;
	caddr_t			data;
{
	int			error = 0;
	struct an_softc		*sc;
	struct an_req		areq;
	struct ifreq		*ifr;
	struct proc		*p = curproc;

	sc = ifp->if_softc;
	AN_LOCK(sc);
	ifr = (struct ifreq *)data;

	if (sc->an_gone) {
		error = ENODEV;
		goto out;
	}

	switch(command) {
	case SIOCSIFADDR:
	case SIOCGIFADDR:
	case SIOCSIFMTU:
		error = ether_ioctl(ifp, command, data);
		break;
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING &&
			    ifp->if_flags & IFF_PROMISC &&
			    !(sc->an_if_flags & IFF_PROMISC)) {
				an_promisc(sc, 1);
			} else if (ifp->if_flags & IFF_RUNNING &&
			    !(ifp->if_flags & IFF_PROMISC) &&
			    sc->an_if_flags & IFF_PROMISC) {
				an_promisc(sc, 0);
			} else
				an_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				an_stop(sc);
		}
		sc->an_if_flags = ifp->if_flags;
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		/* The Aironet has no multicast filter. */
		error = 0;
		break;
	case SIOCGAIRONET:
		error = copyin(ifr->ifr_data, &areq, sizeof(areq));
		if (error)
			break;
#ifdef ANCACHE
		if (areq.an_type == AN_RID_ZERO_CACHE) {
			sc->an_sigitems = sc->an_nextitem = 0;
			break;
		} else if (areq.an_type == AN_RID_READ_CACHE) {
			char *pt = (char *)&areq.an_val;
			bcopy((char *)&sc->an_sigitems, (char *)pt,
			    sizeof(int));
			pt += sizeof(int);
			areq.an_len = sizeof(int) / 2;
			bcopy((char *)&sc->an_sigcache, (char *)pt,
			    sizeof(struct an_sigcache) * sc->an_sigitems);
			areq.an_len += ((sizeof(struct an_sigcache) *
			    sc->an_sigitems) / 2) + 1;
		} else
#endif
		if (an_read_record(sc, (struct an_ltv_gen *)&areq)) {
			error = EINVAL;
			break;
		}
		error = copyout(&areq, ifr->ifr_data, sizeof(areq));
		break;
	case SIOCSAIRONET:
		if ((error = suser(p)))
			goto out;
		error = copyin(ifr->ifr_data, &areq, sizeof(areq));
		if (error)
			break;
		an_setdef(sc, &areq);
		break;
	default:
		error = EINVAL;
		break;
	}
out:
	AN_UNLOCK(sc);

	return(error);
}

static int an_init_tx_ring(sc)
	struct an_softc		*sc;
{
	int			i;
	int			id;

	if (sc->an_gone)
		return (0);

	for (i = 0; i < AN_TX_RING_CNT; i++) {
		if (an_alloc_nicmem(sc, 1518 +
		    0x44, &id))
			return(ENOMEM);
		sc->an_rdata.an_tx_fids[i] = id;
		sc->an_rdata.an_tx_ring[i] = 0;
	}

	sc->an_rdata.an_tx_prod = 0;
	sc->an_rdata.an_tx_cons = 0;

	return(0);
}

static void an_init(xsc)
	void			*xsc;
{
	struct an_softc		*sc = xsc;
	struct ifnet		*ifp = &sc->arpcom.ac_if;

	AN_LOCK(sc);

	if (sc->an_gone) {
		AN_UNLOCK(sc);
		return;
	}

	if (ifp->if_flags & IFF_RUNNING)
		an_stop(sc);

	sc->an_associated = 0;

	/* Allocate the TX buffers */
	if (an_init_tx_ring(sc)) {
		an_reset(sc);
		if (an_init_tx_ring(sc)) {
			printf("an%d: tx buffer allocation "
			    "failed\n", sc->an_unit);
			AN_UNLOCK(sc);
			return;
		}
	}

	/* Set our MAC address. */
	bcopy((char *)&sc->arpcom.ac_enaddr,
	    (char *)&sc->an_config.an_macaddr, ETHER_ADDR_LEN);

	if (ifp->if_flags & IFF_BROADCAST)
		sc->an_config.an_rxmode = AN_RXMODE_BC_ADDR;
	else
		sc->an_config.an_rxmode = AN_RXMODE_ADDR;

	if (ifp->if_flags & IFF_MULTICAST)
		sc->an_config.an_rxmode = AN_RXMODE_BC_MC_ADDR;

	/* Initialize promisc mode. */
	if (ifp->if_flags & IFF_PROMISC)
		sc->an_config.an_rxmode |= AN_RXMODE_LAN_MONITOR_CURBSS;

	sc->an_rxmode = sc->an_config.an_rxmode;

	/* Set the ssid list */
	sc->an_ssidlist.an_type = AN_RID_SSIDLIST;
	sc->an_ssidlist.an_len = sizeof(struct an_ltv_ssidlist);
	if (an_write_record(sc, (struct an_ltv_gen *)&sc->an_ssidlist)) {
		printf("an%d: failed to set ssid list\n", sc->an_unit);
		AN_UNLOCK(sc);
		return;
	}

	/* Set the AP list */
	sc->an_aplist.an_type = AN_RID_APLIST;
	sc->an_aplist.an_len = sizeof(struct an_ltv_aplist);
	if (an_write_record(sc, (struct an_ltv_gen *)&sc->an_aplist)) {
		printf("an%d: failed to set AP list\n", sc->an_unit);
		AN_UNLOCK(sc);
		return;
	}

	/* Set the configuration in the NIC */
	sc->an_config.an_len = sizeof(struct an_ltv_genconfig);
	sc->an_config.an_type = AN_RID_GENCONFIG;
	if (an_write_record(sc, (struct an_ltv_gen *)&sc->an_config)) {
		printf("an%d: failed to set configuration\n", sc->an_unit);
		AN_UNLOCK(sc);
		return;
	}

	/* Enable the MAC */
	if (an_cmd(sc, AN_CMD_ENABLE, 0)) {
		printf("an%d: failed to enable MAC\n", sc->an_unit);
		AN_UNLOCK(sc);
		return;
	}

	/* enable interrupts */
	CSR_WRITE_2(sc, AN_INT_EN, AN_INTRS);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	sc->an_stat_ch = timeout(an_stats_update, sc, hz);
	AN_UNLOCK(sc);

	return;
}

static void an_start(ifp)
	struct ifnet		*ifp;
{
	struct an_softc		*sc;
	struct mbuf		*m0 = NULL;
	struct an_txframe_802_3	tx_frame_802_3;
	struct ether_header	*eh;
	int			id;
	int			idx;
	unsigned char           txcontrol;

	sc = ifp->if_softc;

	if (sc->an_gone)
		return;

	if (ifp->if_flags & IFF_OACTIVE)
		return;

	if (!sc->an_associated)
		return;

	idx = sc->an_rdata.an_tx_prod;
	bzero((char *)&tx_frame_802_3, sizeof(tx_frame_802_3));

	while(sc->an_rdata.an_tx_ring[idx] == 0) {
		IF_DEQUEUE(&ifp->if_snd, m0);
		if (m0 == NULL)
			break;

		id = sc->an_rdata.an_tx_fids[idx];
		eh = mtod(m0, struct ether_header *);

		bcopy((char *)&eh->ether_dhost,
		    (char *)&tx_frame_802_3.an_tx_dst_addr, ETHER_ADDR_LEN);
		bcopy((char *)&eh->ether_shost,
		    (char *)&tx_frame_802_3.an_tx_src_addr, ETHER_ADDR_LEN);
 
		tx_frame_802_3.an_tx_802_3_payload_len =
		  m0->m_pkthdr.len - 12;  /* minus src/dest mac & type */

                m_copydata(m0, sizeof(struct ether_header) - 2 ,
                    tx_frame_802_3.an_tx_802_3_payload_len,
                    (caddr_t)&sc->an_txbuf);

		txcontrol=AN_TXCTL_8023;
		/* write the txcontrol only */
		an_write_data(sc, id, 0x08, (caddr_t)&txcontrol,
			      sizeof(txcontrol));
		
		/* 802_3 header */
		an_write_data(sc, id, 0x34, (caddr_t)&tx_frame_802_3,
			      sizeof(struct an_txframe_802_3));
			
		/* in mbuf header type is just before payload */
		an_write_data(sc, id, 0x44, (caddr_t)&sc->an_txbuf,
			    tx_frame_802_3.an_tx_802_3_payload_len);
		
		/*
		 * If there's a BPF listner, bounce a copy of
		 * this frame to him.
		 */
		if (ifp->if_bpf)
			bpf_mtap(ifp, m0);

		m_freem(m0);
		m0 = NULL;

		sc->an_rdata.an_tx_ring[idx] = id;
		if (an_cmd(sc, AN_CMD_TX, id))
			printf("an%d: xmit failed\n", sc->an_unit);

		AN_INC(idx, AN_TX_RING_CNT);
	}

	if (m0 != NULL)
		ifp->if_flags |= IFF_OACTIVE;

	sc->an_rdata.an_tx_prod = idx;

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;

	return;
}

void an_stop(sc)
	struct an_softc		*sc;
{
	struct ifnet		*ifp;
	int			i;

	AN_LOCK(sc);

	if (sc->an_gone) {
		AN_UNLOCK(sc);
		return;
	}

	ifp = &sc->arpcom.ac_if;

	an_cmd(sc, AN_CMD_FORCE_SYNCLOSS, 0);
	CSR_WRITE_2(sc, AN_INT_EN, 0);
	an_cmd(sc, AN_CMD_DISABLE, 0);

	for (i = 0; i < AN_TX_RING_CNT; i++)
		an_cmd(sc, AN_CMD_DEALLOC_MEM, sc->an_rdata.an_tx_fids[i]);

	untimeout(an_stats_update, sc, sc->an_stat_ch);

	ifp->if_flags &= ~(IFF_RUNNING|IFF_OACTIVE);

	AN_UNLOCK(sc);

	return;
}

static void an_watchdog(ifp)
	struct ifnet		*ifp;
{
	struct an_softc		*sc;

	sc = ifp->if_softc;
	AN_LOCK(sc);

	if (sc->an_gone) {
		AN_UNLOCK(sc);
		return;
	}

	printf("an%d: device timeout\n", sc->an_unit);

	an_reset(sc);
	an_init(sc);

	ifp->if_oerrors++;
	AN_UNLOCK(sc);

	return;
}

void an_shutdown(dev)
	device_t		dev;
{
	struct an_softc		*sc;

	sc = device_get_softc(dev);
	an_stop(sc);

	return;
}

#ifdef ANCACHE
/* Aironet signal strength cache code.
 * store signal/noise/quality on per MAC src basis in
 * a small fixed cache.  The cache wraps if > MAX slots
 * used.  The cache may be zeroed out to start over.
 * Two simple filters exist to reduce computation:
 * 1. ip only (literally 0x800) which may be used
 * to ignore some packets.  It defaults to ip only.
 * it could be used to focus on broadcast, non-IP 802.11 beacons.
 * 2. multicast/broadcast only.  This may be used to
 * ignore unicast packets and only cache signal strength
 * for multicast/broadcast packets (beacons); e.g., Mobile-IP
 * beacons and not unicast traffic.
 *
 * The cache stores (MAC src(index), IP src (major clue), signal,
 *	quality, noise)
 *
 * No apologies for storing IP src here.  It's easy and saves much
 * trouble elsewhere.  The cache is assumed to be INET dependent, 
 * although it need not be.
 *
 * Note: the Aironet only has a single byte of signal strength value
 * in the rx frame header, and it's not scaled to anything sensible.
 * This is kind of lame, but it's all we've got.
 */

#ifdef documentation

int an_sigitems;                                /* number of cached entries */
struct an_sigcache an_sigcache[MAXANCACHE];  /*  array of cache entries */
int an_nextitem;                                /*  index/# of entries */


#endif

/* control variables for cache filtering.  Basic idea is
 * to reduce cost (e.g., to only Mobile-IP agent beacons
 * which are broadcast or multicast).  Still you might
 * want to measure signal strength anth unicast ping packets
 * on a pt. to pt. ant. setup.
 */
/* set true if you want to limit cache items to broadcast/mcast 
 * only packets (not unicast).  Useful for mobile-ip beacons which
 * are broadcast/multicast at network layer.  Default is all packets
 * so ping/unicast anll work say anth pt. to pt. antennae setup.
 */
static int an_cache_mcastonly = 0;
SYSCTL_INT(_machdep, OID_AUTO, an_cache_mcastonly, CTLFLAG_RW, 
	&an_cache_mcastonly, 0, "");

/* set true if you want to limit cache items to IP packets only
*/
static int an_cache_iponly = 1;
SYSCTL_INT(_machdep, OID_AUTO, an_cache_iponly, CTLFLAG_RW, 
	&an_cache_iponly, 0, "");

/*
 * an_cache_store, per rx packet store signal
 * strength in MAC (src) indexed cache.
 */
static  
void an_cache_store (sc, eh, m, rx_quality)
	struct an_softc *sc;
	struct ether_header *eh;
	struct mbuf *m;
	unsigned short rx_quality;
{
	struct ip *ip = 0; 
	int i;
	static int cache_slot = 0; 	/* use this cache entry */
	static int wrapindex = 0;       /* next "free" cache entry */
	int saanp=0;

	/* filters:
	 * 1. ip only
	 * 2. configurable filter to throw out unicast packets,
	 * keep multicast only.
	 */
 
	if ((ntohs(eh->ether_type) == 0x800)) {
		saanp = 1;
	}

	/* filter for ip packets only 
	*/
	if ( an_cache_iponly && !saanp) {
		return;
	}

	/* filter for broadcast/multicast only
	 */
	if (an_cache_mcastonly && ((eh->ether_dhost[0] & 1) == 0)) {
		return;
	}

#ifdef SIGDEBUG
	printf("an: q value %x (MSB=0x%x, LSB=0x%x) \n",
	    rx_quality & 0xffff, rx_quality >> 8, rx_quality & 0xff);
#endif

	/* find the ip header.  we want to store the ip_src
	 * address.  
	 */
	if (saanp) {
		ip = mtod(m, struct ip *);
	}
        
	/* do a linear search for a matching MAC address 
	 * in the cache table
	 * . MAC address is 6 bytes,
	 * . var w_nextitem holds total number of entries already cached
	 */
	for(i = 0; i < sc->an_nextitem; i++) {
		if (! bcmp(eh->ether_shost , sc->an_sigcache[i].macsrc,  6 )) {
			/* Match!,
			 * so we already have this entry,
			 * update the data
			 */
			break;	
		}
	}

	/* did we find a matching mac address?
	 * if yes, then overwrite a previously existing cache entry
	 */
	if (i < sc->an_nextitem )   {
		cache_slot = i; 
	}
	/* else, have a new address entry,so
	 * add this new entry,
	 * if table full, then we need to replace LRU entry
	 */
	else    {                          

		/* check for space in cache table 
		 * note: an_nextitem also holds number of entries
		 * added in the cache table 
		 */
		if ( sc->an_nextitem < MAXANCACHE ) {
			cache_slot = sc->an_nextitem;
			sc->an_nextitem++;                 
			sc->an_sigitems = sc->an_nextitem;
		}
        	/* no space found, so simply wrap anth wrap index
		 * and "zap" the next entry
		 */
		else {
			if (wrapindex == MAXANCACHE) {
				wrapindex = 0;
			}
			cache_slot = wrapindex++;
		}
	}

	/* invariant: cache_slot now points at some slot
	 * in cache.
	 */
	if (cache_slot < 0 || cache_slot >= MAXANCACHE) {
		log(LOG_ERR, "an_cache_store, bad index: %d of "
		    "[0..%d], gross cache error\n",
		    cache_slot, MAXANCACHE);
		return;
	}

	/*  store items in cache
	 *  .ip source address
	 *  .mac src
	 *  .signal, etc.
	 */
	if (saanp) {
		sc->an_sigcache[cache_slot].ipsrc = ip->ip_src.s_addr;
	}
	bcopy( eh->ether_shost, sc->an_sigcache[cache_slot].macsrc,  6);

	sc->an_sigcache[cache_slot].signal = rx_quality;

	return;
}
#endif
