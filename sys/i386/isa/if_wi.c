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
 * $FreeBSD: src/sys/i386/isa/if_wi.c,v 1.18.2.5 2000/09/07 17:10:41 wpaul Exp $
 */

/*
 * Lucent WaveLAN/IEEE 802.11 PCMCIA driver for FreeBSD.
 *
 * Written by Bill Paul <wpaul@ctr.columbia.edu>
 * Electrical Engineering Department
 * Columbia University, New York City
 */

/*
 * The WaveLAN/IEEE adapter is the second generation of the WaveLAN
 * from Lucent. Unlike the older cards, the new ones are programmed
 * entirely via a firmware-driven controller called the Hermes.
 * Unfortunately, Lucent will not release the Hermes programming manual
 * without an NDA (if at all). What they do release is an API library
 * called the HCF (Hardware Control Functions) which is supposed to
 * do the device-specific operations of a device driver for you. The
 * publically available version of the HCF library (the 'HCF Light') is 
 * a) extremely gross, b) lacks certain features, particularly support
 * for 802.11 frames, and c) is contaminated by the GNU Public License.
 *
 * This driver does not use the HCF or HCF Light at all. Instead, it
 * programs the Hermes controller directly, using information gleaned
 * from the HCF Light code and corresponding documentation.
 *
 * This driver supports both the PCMCIA and ISA versions of the
 * WaveLAN/IEEE cards. Note however that the ISA card isn't really
 * anything of the sort: it's actually a PCMCIA bridge adapter
 * that fits into an ISA slot, into which a PCMCIA WaveLAN card is
 * inserted. Consequently, you need to use the pccard support for
 * both the ISA and PCMCIA adapters.
 */

#define WI_HERMES_AUTOINC_WAR	/* Work around data write autoinc bug. */
#define WI_HERMES_STATS_WAR	/* Work around stats counter bug. */
#define WICACHE			/* turn on signal strength cache code */  

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/ucred.h>
#include <sys/socket.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/syslog.h>
#include <sys/sysctl.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/clock.h>
#include <machine/md_var.h>
#include <machine/bus_pio.h>
#include <sys/rman.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>

#include <net/bpf.h>

#include <machine/if_wavelan_ieee.h>
#include <i386/isa/if_wireg.h>

#if !defined(lint)
static const char rcsid[] =
  "$FreeBSD: src/sys/i386/isa/if_wi.c,v 1.18.2.5 2000/09/07 17:10:41 wpaul Exp $";
#endif

#ifdef foo
static u_int8_t	wi_mcast_addr[6] = { 0x01, 0x60, 0x1D, 0x00, 0x01, 0x00 };
#endif

static void wi_intr		__P((void *));
static void wi_reset		__P((struct wi_softc *));
static int wi_ioctl		__P((struct ifnet *, u_long, caddr_t));
static void wi_init		__P((void *));
static void wi_start		__P((struct ifnet *));
static void wi_stop		__P((struct wi_softc *));
static void wi_watchdog		__P((struct ifnet *));
static void wi_rxeof		__P((struct wi_softc *));
static void wi_txeof		__P((struct wi_softc *, int));
static void wi_update_stats	__P((struct wi_softc *));
static void wi_setmulti		__P((struct wi_softc *));

static int wi_cmd		__P((struct wi_softc *, int, int));
static int wi_read_record	__P((struct wi_softc *, struct wi_ltv_gen *));
static int wi_write_record	__P((struct wi_softc *, struct wi_ltv_gen *));
static int wi_read_data		__P((struct wi_softc *, int,
					int, caddr_t, int));
static int wi_write_data	__P((struct wi_softc *, int,
					int, caddr_t, int));
static int wi_seek		__P((struct wi_softc *, int, int, int));
static int wi_alloc_nicmem	__P((struct wi_softc *, int, int *));
static void wi_inquire		__P((void *));
static void wi_setdef		__P((struct wi_softc *, struct wi_req *));
static int wi_mgmt_xmit		__P((struct wi_softc *, caddr_t, int));

#ifdef WICACHE
static
void wi_cache_store __P((struct wi_softc *, struct ether_header *,
	struct mbuf *, unsigned short));
#endif

static int wi_pccard_probe	__P((device_t));
static int wi_pccard_attach	__P((device_t));
static int wi_pccard_detach	__P((device_t));
static void wi_shutdown		__P((device_t));

static int wi_alloc		__P((device_t));
static void wi_free		__P((device_t));

static device_method_t wi_pccard_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		wi_pccard_probe),
	DEVMETHOD(device_attach,	wi_pccard_attach),
	DEVMETHOD(device_detach,	wi_pccard_detach),
	DEVMETHOD(device_shutdown,	wi_shutdown),

	{ 0, 0 }
};

static driver_t wi_pccard_driver = {
	"wi",
	wi_pccard_methods,
	sizeof(struct wi_softc)
};

static devclass_t wi_pccard_devclass;

DRIVER_MODULE(if_wi, pccard, wi_pccard_driver, wi_pccard_devclass, 0, 0);

static int wi_pccard_probe(dev)
	device_t	dev;
{
	struct wi_softc	*sc;
	int		error;

	sc = device_get_softc(dev);
	sc->wi_gone = 0;

	error = wi_alloc(dev);
	if (error)
		return (error);

	device_set_desc(dev, "WaveLAN/IEEE 802.11");
	wi_free(dev);

	/* Make sure interrupts are disabled. */
	CSR_WRITE_2(sc, WI_INT_EN, 0);
	CSR_WRITE_2(sc, WI_EVENT_ACK, 0xFFFF);

	return (0);
}

static int wi_pccard_detach(dev)
	device_t		dev;
{
	struct wi_softc		*sc;
	struct ifnet		*ifp;
	int			s;

	s = splimp();

	sc = device_get_softc(dev);
	ifp = &sc->arpcom.ac_if;

	if (sc->wi_gone) {
		device_printf(dev, "already unloaded\n");
		return(ENODEV);
	}

	wi_stop(sc);
	ether_ifdetach(ifp, ETHER_BPF_SUPPORTED);
	bus_teardown_intr(dev, sc->irq, sc->wi_intrhand);
	wi_free(dev);
	sc->wi_gone = 1;

	splx(s);
	device_printf(dev, "unload\n");

	return(0);
}

static int wi_pccard_attach(device_t dev)
{
	struct wi_softc		*sc;
	struct wi_ltv_macaddr	mac;
	struct wi_ltv_gen	gen;
	struct ifnet		*ifp;
	int			error;

	sc = device_get_softc(dev);
	ifp = &sc->arpcom.ac_if;

	error = wi_alloc(dev);
	if (error) {
		device_printf(dev, "wi_alloc() failed! (%d)\n", error);
		return (error);
	}

	error = bus_setup_intr(dev, sc->irq, INTR_TYPE_NET,
			       wi_intr, sc, &sc->wi_intrhand);

	if (error) {
		device_printf(dev, "bus_setup_intr() failed! (%d)\n", error);
		wi_free(dev);
		return (error);
	}

	/* Reset the NIC. */
	wi_reset(sc);

	/* Read the station address. */
	mac.wi_type = WI_RID_MAC_NODE;
	mac.wi_len = 4;
	wi_read_record(sc, (struct wi_ltv_gen *)&mac);
	bcopy((char *)&mac.wi_mac_addr,
	   (char *)&sc->arpcom.ac_enaddr, ETHER_ADDR_LEN);

	device_printf(dev, "Ethernet address: %6D\n",
	    sc->arpcom.ac_enaddr, ":");

	ifp->if_softc = sc;
	ifp->if_unit = sc->wi_unit;
	ifp->if_name = "wi";
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = wi_ioctl;
	ifp->if_output = ether_output;
	ifp->if_start = wi_start;
	ifp->if_watchdog = wi_watchdog;
	ifp->if_init = wi_init;
	ifp->if_baudrate = 10000000;
	ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;

	bzero(sc->wi_node_name, sizeof(sc->wi_node_name));
	bcopy(WI_DEFAULT_NODENAME, sc->wi_node_name,
	    sizeof(WI_DEFAULT_NODENAME) - 1);

	bzero(sc->wi_net_name, sizeof(sc->wi_net_name));
	bcopy(WI_DEFAULT_NETNAME, sc->wi_net_name,
	    sizeof(WI_DEFAULT_NETNAME) - 1);

	bzero(sc->wi_ibss_name, sizeof(sc->wi_ibss_name));
	bcopy(WI_DEFAULT_IBSS, sc->wi_ibss_name,
	    sizeof(WI_DEFAULT_IBSS) - 1);

	sc->wi_portnum = WI_DEFAULT_PORT;
	sc->wi_ptype = WI_PORTTYPE_ADHOC;
	sc->wi_ap_density = WI_DEFAULT_AP_DENSITY;
	sc->wi_rts_thresh = WI_DEFAULT_RTS_THRESH;
	sc->wi_tx_rate = WI_DEFAULT_TX_RATE;
	sc->wi_max_data_len = WI_DEFAULT_DATALEN;
	sc->wi_create_ibss = WI_DEFAULT_CREATE_IBSS;
	sc->wi_pm_enabled = WI_DEFAULT_PM_ENABLED;
	sc->wi_max_sleep = WI_DEFAULT_MAX_SLEEP;

	/*
	 * Read the default channel from the NIC. This may vary
	 * depending on the country where the NIC was purchased, so
	 * we can't hard-code a default and expect it to work for
	 * everyone.
	 */
	gen.wi_type = WI_RID_OWN_CHNL;
	gen.wi_len = 2;
	wi_read_record(sc, &gen);
	sc->wi_channel = gen.wi_val;

	/*
	 * Find out if we support WEP on this card.
	 */
	gen.wi_type = WI_RID_WEP_AVAIL;
	gen.wi_len = 2;
	wi_read_record(sc, &gen);
	sc->wi_has_wep = gen.wi_val;

	bzero((char *)&sc->wi_stats, sizeof(sc->wi_stats));

	wi_init(sc);
	wi_stop(sc);

	/*
	 * Call MI attach routine.
	 */
	ether_ifattach(ifp, ETHER_BPF_SUPPORTED);
	callout_handle_init(&sc->wi_stat_ch);

	return(0);
}

static void wi_rxeof(sc)
	struct wi_softc		*sc;
{
	struct ifnet		*ifp;
	struct ether_header	*eh;
	struct wi_frame		rx_frame;
	struct mbuf		*m;
	int			id;

	ifp = &sc->arpcom.ac_if;

	id = CSR_READ_2(sc, WI_RX_FID);

	/* First read in the frame header */
	if (wi_read_data(sc, id, 0, (caddr_t)&rx_frame, sizeof(rx_frame))) {
		ifp->if_ierrors++;
		return;
	}

	if (rx_frame.wi_status & WI_STAT_ERRSTAT) {
		ifp->if_ierrors++;
		return;
	}

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

	eh = mtod(m, struct ether_header *);
	m->m_pkthdr.rcvif = ifp;

	if (rx_frame.wi_status == WI_STAT_1042 ||
	    rx_frame.wi_status == WI_STAT_TUNNEL ||
	    rx_frame.wi_status == WI_STAT_WMP_MSG) {
		if((rx_frame.wi_dat_len + WI_SNAPHDR_LEN) > MCLBYTES) {
			device_printf(sc->dev, "oversized packet received "
			    "(wi_dat_len=%d, wi_status=0x%x)\n",
			    rx_frame.wi_dat_len, rx_frame.wi_status);
			m_freem(m);
			ifp->if_ierrors++;
			return;
		}
		m->m_pkthdr.len = m->m_len =
		    rx_frame.wi_dat_len + WI_SNAPHDR_LEN;

		bcopy((char *)&rx_frame.wi_addr1,
		    (char *)&eh->ether_dhost, ETHER_ADDR_LEN);
		if (sc->wi_ptype == WI_PORTTYPE_ADHOC) {
			bcopy((char *)&rx_frame.wi_addr2,
			    (char *)&eh->ether_shost, ETHER_ADDR_LEN);
		} else {
			bcopy((char *)&rx_frame.wi_addr3,
			    (char *)&eh->ether_shost, ETHER_ADDR_LEN);
		}
		bcopy((char *)&rx_frame.wi_type,
		    (char *)&eh->ether_type, sizeof(u_int16_t));

		if (wi_read_data(sc, id, WI_802_11_OFFSET,
		    mtod(m, caddr_t) + sizeof(struct ether_header),
		    m->m_len + 2)) {
			m_freem(m);
			ifp->if_ierrors++;
			return;
		}
	} else {
		if((rx_frame.wi_dat_len +
		    sizeof(struct ether_header)) > MCLBYTES) {
			device_printf(sc->dev, "oversized packet received "
			    "(wi_dat_len=%d, wi_status=0x%x)\n",
			    rx_frame.wi_dat_len, rx_frame.wi_status);
			m_freem(m);
			ifp->if_ierrors++;
			return;
		}
		m->m_pkthdr.len = m->m_len =
		    rx_frame.wi_dat_len + sizeof(struct ether_header);

		if (wi_read_data(sc, id, WI_802_3_OFFSET,
		    mtod(m, caddr_t), m->m_len + 2)) {
			m_freem(m);
			ifp->if_ierrors++;
			return;
		}
	}

	ifp->if_ipackets++;

	/* Receive packet. */
	m_adj(m, sizeof(struct ether_header));
#ifdef WICACHE
	wi_cache_store(sc, eh, m, rx_frame.wi_q_info);
#endif  
	ether_input(ifp, eh, m);
}

static void wi_txeof(sc, status)
	struct wi_softc		*sc;
	int			status;
{
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;

	ifp->if_timer = 0;
	ifp->if_flags &= ~IFF_OACTIVE;

	if (status & WI_EV_TX_EXC)
		ifp->if_oerrors++;
	else
		ifp->if_opackets++;

	return;
}

void wi_inquire(xsc)
	void			*xsc;
{
	struct wi_softc		*sc;
	struct ifnet		*ifp;

	sc = xsc;
	ifp = &sc->arpcom.ac_if;

	sc->wi_stat_ch = timeout(wi_inquire, sc, hz * 60);

	/* Don't do this while we're transmitting */
	if (ifp->if_flags & IFF_OACTIVE)
		return;

	wi_cmd(sc, WI_CMD_INQUIRE, WI_INFO_COUNTERS);

	return;
}

void wi_update_stats(sc)
	struct wi_softc		*sc;
{
	struct wi_ltv_gen	gen;
	u_int16_t		id;
	struct ifnet		*ifp;
	u_int32_t		*ptr;
	int			i;
	u_int16_t		t;

	ifp = &sc->arpcom.ac_if;

	id = CSR_READ_2(sc, WI_INFO_FID);

	wi_read_data(sc, id, 0, (char *)&gen, 4);

	if (gen.wi_type != WI_INFO_COUNTERS ||
	    gen.wi_len > (sizeof(sc->wi_stats) / 4) + 1)
		return;

	ptr = (u_int32_t *)&sc->wi_stats;

	for (i = 0; i < gen.wi_len - 1; i++) {
		t = CSR_READ_2(sc, WI_DATA1);
#ifdef WI_HERMES_STATS_WAR
		if (t > 0xF000)
			t = ~t & 0xFFFF;
#endif
		ptr[i] += t;
	}

	ifp->if_collisions = sc->wi_stats.wi_tx_single_retries +
	    sc->wi_stats.wi_tx_multi_retries +
	    sc->wi_stats.wi_tx_retry_limit;

	return;
}

static void wi_intr(xsc)
	void		*xsc;
{
	struct wi_softc		*sc = xsc;
	struct ifnet		*ifp;
	u_int16_t		status;

	ifp = &sc->arpcom.ac_if;

	if (!(ifp->if_flags & IFF_UP)) {
		CSR_WRITE_2(sc, WI_EVENT_ACK, 0xFFFF);
		CSR_WRITE_2(sc, WI_INT_EN, 0);
		return;
	}

	/* Disable interrupts. */
	CSR_WRITE_2(sc, WI_INT_EN, 0);

	status = CSR_READ_2(sc, WI_EVENT_STAT);
	CSR_WRITE_2(sc, WI_EVENT_ACK, ~WI_INTRS);

	if (status & WI_EV_RX) {
		wi_rxeof(sc);
		CSR_WRITE_2(sc, WI_EVENT_ACK, WI_EV_RX);
	}

	if (status & WI_EV_TX) {
		wi_txeof(sc, status);
		CSR_WRITE_2(sc, WI_EVENT_ACK, WI_EV_TX);
	}

	if (status & WI_EV_ALLOC) {
		int			id;
		id = CSR_READ_2(sc, WI_ALLOC_FID);
		CSR_WRITE_2(sc, WI_EVENT_ACK, WI_EV_ALLOC);
		if (id == sc->wi_tx_data_id)
			wi_txeof(sc, status);
	}

	if (status & WI_EV_INFO) {
		wi_update_stats(sc);
		CSR_WRITE_2(sc, WI_EVENT_ACK, WI_EV_INFO);
	}

	if (status & WI_EV_TX_EXC) {
		wi_txeof(sc, status);
		CSR_WRITE_2(sc, WI_EVENT_ACK, WI_EV_TX_EXC);
	}

	if (status & WI_EV_INFO_DROP) {
		CSR_WRITE_2(sc, WI_EVENT_ACK, WI_EV_INFO_DROP);
	}

	/* Re-enable interrupts. */
	CSR_WRITE_2(sc, WI_INT_EN, WI_INTRS);

	if (ifp->if_snd.ifq_head != NULL)
		wi_start(ifp);

	return;
}

static int wi_cmd(sc, cmd, val)
	struct wi_softc		*sc;
	int			cmd;
	int			val;
{
	int			i, s = 0;

	CSR_WRITE_2(sc, WI_PARAM0, val);
	CSR_WRITE_2(sc, WI_COMMAND, cmd);

	for (i = 0; i < WI_TIMEOUT; i++) {
		/*
		 * Wait for 'command complete' bit to be
		 * set in the event status register.
		 */
		s = CSR_READ_2(sc, WI_EVENT_STAT) & WI_EV_CMD;
		if (s) {
			/* Ack the event and read result code. */
			s = CSR_READ_2(sc, WI_STATUS);
			CSR_WRITE_2(sc, WI_EVENT_ACK, WI_EV_CMD);
#ifdef foo
			if ((s & WI_CMD_CODE_MASK) != (cmd & WI_CMD_CODE_MASK))
				return(EIO);
#endif
			if (s & WI_STAT_CMD_RESULT)
				return(EIO);
			break;
		}
	}

	if (i == WI_TIMEOUT)
		return(ETIMEDOUT);

	return(0);
}

static void wi_reset(sc)
	struct wi_softc		*sc;
{
	wi_cmd(sc, WI_CMD_INI, 0);
	DELAY(100000);
	wi_cmd(sc, WI_CMD_INI, 0);
	DELAY(100000);
#ifdef foo
	if (wi_cmd(sc, WI_CMD_INI, 0))
		device_printf(sc->dev, "init failed\n");
	CSR_WRITE_2(sc, WI_INT_EN, 0);
	CSR_WRITE_2(sc, WI_EVENT_ACK, 0xFFFF);

	/* Calibrate timer. */
	WI_SETVAL(WI_RID_TICK_TIME, 8);
#endif
	return;
}

/*
 * Read an LTV record from the NIC.
 */
static int wi_read_record(sc, ltv)
	struct wi_softc		*sc;
	struct wi_ltv_gen	*ltv;
{
	u_int16_t		*ptr;
	int			i, len, code;

	/* Tell the NIC to enter record read mode. */
	if (wi_cmd(sc, WI_CMD_ACCESS|WI_ACCESS_READ, ltv->wi_type))
		return(EIO);

	/* Seek to the record. */
	if (wi_seek(sc, ltv->wi_type, 0, WI_BAP1))
		return(EIO);

	/*
	 * Read the length and record type and make sure they
	 * match what we expect (this verifies that we have enough
	 * room to hold all of the returned data).
	 */
	len = CSR_READ_2(sc, WI_DATA1);
	if (len > ltv->wi_len)
		return(ENOSPC);
	code = CSR_READ_2(sc, WI_DATA1);
	if (code != ltv->wi_type)
		return(EIO);

	ltv->wi_len = len;
	ltv->wi_type = code;

	/* Now read the data. */
	ptr = &ltv->wi_val;
	for (i = 0; i < ltv->wi_len - 1; i++)
		ptr[i] = CSR_READ_2(sc, WI_DATA1);

	return(0);
}

/*
 * Same as read, except we inject data instead of reading it.
 */
static int wi_write_record(sc, ltv)
	struct wi_softc		*sc;
	struct wi_ltv_gen	*ltv;
{
	u_int16_t		*ptr;
	int			i;

	if (wi_seek(sc, ltv->wi_type, 0, WI_BAP1))
		return(EIO);

	CSR_WRITE_2(sc, WI_DATA1, ltv->wi_len);
	CSR_WRITE_2(sc, WI_DATA1, ltv->wi_type);

	ptr = &ltv->wi_val;
	for (i = 0; i < ltv->wi_len - 1; i++)
		CSR_WRITE_2(sc, WI_DATA1, ptr[i]);

	if (wi_cmd(sc, WI_CMD_ACCESS|WI_ACCESS_WRITE, ltv->wi_type))
		return(EIO);

	return(0);
}

static int wi_seek(sc, id, off, chan)
	struct wi_softc		*sc;
	int			id, off, chan;
{
	int			i;
	int			selreg, offreg;

	switch (chan) {
	case WI_BAP0:
		selreg = WI_SEL0;
		offreg = WI_OFF0;
		break;
	case WI_BAP1:
		selreg = WI_SEL1;
		offreg = WI_OFF1;
		break;
	default:
		device_printf(sc->dev, "invalid data path: %x\n", chan);
		return(EIO);
	}

	CSR_WRITE_2(sc, selreg, id);
	CSR_WRITE_2(sc, offreg, off);

	for (i = 0; i < WI_TIMEOUT; i++) {
		if (!(CSR_READ_2(sc, offreg) & (WI_OFF_BUSY|WI_OFF_ERR)))
			break;
	}

	if (i == WI_TIMEOUT)
		return(ETIMEDOUT);

	return(0);
}

static int wi_read_data(sc, id, off, buf, len)
	struct wi_softc		*sc;
	int			id, off;
	caddr_t			buf;
	int			len;
{
	int			i;
	u_int16_t		*ptr;

	if (wi_seek(sc, id, off, WI_BAP1))
		return(EIO);

	ptr = (u_int16_t *)buf;
	for (i = 0; i < len / 2; i++)
		ptr[i] = CSR_READ_2(sc, WI_DATA1);

	return(0);
}

/*
 * According to the comments in the HCF Light code, there is a bug in
 * the Hermes (or possibly in certain Hermes firmware revisions) where
 * the chip's internal autoincrement counter gets thrown off during
 * data writes: the autoincrement is missed, causing one data word to
 * be overwritten and subsequent words to be written to the wrong memory
 * locations. The end result is that we could end up transmitting bogus
 * frames without realizing it. The workaround for this is to write a
 * couple of extra guard words after the end of the transfer, then
 * attempt to read then back. If we fail to locate the guard words where
 * we expect them, we preform the transfer over again.
 */
static int wi_write_data(sc, id, off, buf, len)
	struct wi_softc		*sc;
	int			id, off;
	caddr_t			buf;
	int			len;
{
	int			i;
	u_int16_t		*ptr;

#ifdef WI_HERMES_AUTOINC_WAR
again:
#endif

	if (wi_seek(sc, id, off, WI_BAP0))
		return(EIO);

	ptr = (u_int16_t *)buf;
	for (i = 0; i < (len / 2); i++)
		CSR_WRITE_2(sc, WI_DATA0, ptr[i]);

#ifdef WI_HERMES_AUTOINC_WAR
	CSR_WRITE_2(sc, WI_DATA0, 0x1234);
	CSR_WRITE_2(sc, WI_DATA0, 0x5678);

	if (wi_seek(sc, id, off + len, WI_BAP0))
		return(EIO);

	if (CSR_READ_2(sc, WI_DATA0) != 0x1234 ||
	    CSR_READ_2(sc, WI_DATA0) != 0x5678)
		goto again;
#endif

	return(0);
}

/*
 * Allocate a region of memory inside the NIC and zero
 * it out.
 */
static int wi_alloc_nicmem(sc, len, id)
	struct wi_softc		*sc;
	int			len;
	int			*id;
{
	int			i;

	if (wi_cmd(sc, WI_CMD_ALLOC_MEM, len)) {
		device_printf(sc->dev, "failed to allocate %d bytes on NIC\n", len);
		return(ENOMEM);
	}

	for (i = 0; i < WI_TIMEOUT; i++) {
		if (CSR_READ_2(sc, WI_EVENT_STAT) & WI_EV_ALLOC)
			break;
	}

	if (i == WI_TIMEOUT)
		return(ETIMEDOUT);

	CSR_WRITE_2(sc, WI_EVENT_ACK, WI_EV_ALLOC);
	*id = CSR_READ_2(sc, WI_ALLOC_FID);

	if (wi_seek(sc, *id, 0, WI_BAP0))
		return(EIO);

	for (i = 0; i < len / 2; i++)
		CSR_WRITE_2(sc, WI_DATA0, 0);

	return(0);
}

static void wi_setmulti(sc)
	struct wi_softc		*sc;
{
	struct ifnet		*ifp;
	int			i = 0;
	struct ifmultiaddr	*ifma;
	struct wi_ltv_mcast	mcast;

	ifp = &sc->arpcom.ac_if;

	bzero((char *)&mcast, sizeof(mcast));

	mcast.wi_type = WI_RID_MCAST;
	mcast.wi_len = (3 * 16) + 1;

	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
		wi_write_record(sc, (struct wi_ltv_gen *)&mcast);
		return;
	}

	for (ifma = ifp->if_multiaddrs.lh_first; ifma != NULL;
				ifma = ifma->ifma_link.le_next) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		if (i < 16) {
			bcopy(LLADDR((struct sockaddr_dl *)ifma->ifma_addr),
			    (char *)&mcast.wi_mcast[i], ETHER_ADDR_LEN);
			i++;
		} else {
			bzero((char *)&mcast, sizeof(mcast));
			break;
		}
	}

	mcast.wi_len = (i * 3) + 1;
	wi_write_record(sc, (struct wi_ltv_gen *)&mcast);

	return;
}

static void wi_setdef(sc, wreq)
	struct wi_softc		*sc;
	struct wi_req		*wreq;
{
	struct sockaddr_dl	*sdl;
	struct ifaddr		*ifa;
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;

	switch(wreq->wi_type) {
	case WI_RID_MAC_NODE:
		ifa = ifnet_addrs[ifp->if_index - 1];
		sdl = (struct sockaddr_dl *)ifa->ifa_addr;
		bcopy((char *)&wreq->wi_val, (char *)&sc->arpcom.ac_enaddr,
		   ETHER_ADDR_LEN);
		bcopy((char *)&wreq->wi_val, LLADDR(sdl), ETHER_ADDR_LEN);
		break;
	case WI_RID_PORTTYPE:
		sc->wi_ptype = wreq->wi_val[0];
		break;
	case WI_RID_TX_RATE:
		sc->wi_tx_rate = wreq->wi_val[0];
		break;
	case WI_RID_MAX_DATALEN:
		sc->wi_max_data_len = wreq->wi_val[0];
		break;
	case WI_RID_RTS_THRESH:
		sc->wi_rts_thresh = wreq->wi_val[0];
		break;
	case WI_RID_SYSTEM_SCALE:
		sc->wi_ap_density = wreq->wi_val[0];
		break;
	case WI_RID_CREATE_IBSS:
		sc->wi_create_ibss = wreq->wi_val[0];
		break;
	case WI_RID_OWN_CHNL:
		sc->wi_channel = wreq->wi_val[0];
		break;
	case WI_RID_NODENAME:
		bzero(sc->wi_node_name, sizeof(sc->wi_node_name));
		bcopy((char *)&wreq->wi_val[1], sc->wi_node_name, 30);
		break;
	case WI_RID_DESIRED_SSID:
		bzero(sc->wi_net_name, sizeof(sc->wi_net_name));
		bcopy((char *)&wreq->wi_val[1], sc->wi_net_name, 30);
		break;
	case WI_RID_OWN_SSID:
		bzero(sc->wi_ibss_name, sizeof(sc->wi_ibss_name));
		bcopy((char *)&wreq->wi_val[1], sc->wi_ibss_name, 30);
		break;
	case WI_RID_PM_ENABLED:
		sc->wi_pm_enabled = wreq->wi_val[0];
		break;
	case WI_RID_MAX_SLEEP:
		sc->wi_max_sleep = wreq->wi_val[0];
		break;
	case WI_RID_ENCRYPTION:
		sc->wi_use_wep = wreq->wi_val[0];
		break;
	case WI_RID_TX_CRYPT_KEY:
		sc->wi_tx_key = wreq->wi_val[0];
		break;
	case WI_RID_DEFLT_CRYPT_KEYS:
		bcopy((char *)wreq, (char *)&sc->wi_keys,
		    sizeof(struct wi_ltv_keys));
		break;
	default:
		break;
	}

	/* Reinitialize WaveLAN. */
	wi_init(sc);

	return;
}

static int wi_ioctl(ifp, command, data)
	struct ifnet		*ifp;
	u_long			command;
	caddr_t			data;
{
	int			s, error = 0;
	struct wi_softc		*sc;
	struct wi_req		wreq;
	struct ifreq		*ifr;
	struct proc		*p = curproc;

	s = splimp();

	sc = ifp->if_softc;
	ifr = (struct ifreq *)data;

	if (sc->wi_gone) {
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
			    !(sc->wi_if_flags & IFF_PROMISC)) {
				WI_SETVAL(WI_RID_PROMISC, 1);
			} else if (ifp->if_flags & IFF_RUNNING &&
			    !(ifp->if_flags & IFF_PROMISC) &&
			    sc->wi_if_flags & IFF_PROMISC) {
				WI_SETVAL(WI_RID_PROMISC, 0);
			} else
				wi_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING) {
				wi_stop(sc);
			}
		}
		sc->wi_if_flags = ifp->if_flags;
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		wi_setmulti(sc);
		error = 0;
		break;
	case SIOCGWAVELAN:
		error = copyin(ifr->ifr_data, &wreq, sizeof(wreq));
		if (error)
			break;
		/* Don't show WEP keys to non-root users. */
		if (wreq.wi_type == WI_RID_DEFLT_CRYPT_KEYS && suser(p))
			break;
		if (wreq.wi_type == WI_RID_IFACE_STATS) {
			bcopy((char *)&sc->wi_stats, (char *)&wreq.wi_val,
			    sizeof(sc->wi_stats));
			wreq.wi_len = (sizeof(sc->wi_stats) / 2) + 1;
		} else if (wreq.wi_type == WI_RID_DEFLT_CRYPT_KEYS) {
			bcopy((char *)&sc->wi_keys, (char *)&wreq,
			    sizeof(struct wi_ltv_keys));
		}
#ifdef WICACHE
		else if (wreq.wi_type == WI_RID_ZERO_CACHE) {
			sc->wi_sigitems = sc->wi_nextitem = 0;
		} else if (wreq.wi_type == WI_RID_READ_CACHE) {
			char *pt = (char *)&wreq.wi_val;
			bcopy((char *)&sc->wi_sigitems,
			    (char *)pt, sizeof(int));
			pt += (sizeof (int));
			wreq.wi_len = sizeof(int) / 2;
			bcopy((char *)&sc->wi_sigcache, (char *)pt,
			    sizeof(struct wi_sigcache) * sc->wi_sigitems);
			wreq.wi_len += ((sizeof(struct wi_sigcache) *
			    sc->wi_sigitems) / 2) + 1;
		}
#endif
		else {
			if (wi_read_record(sc, (struct wi_ltv_gen *)&wreq)) {
				error = EINVAL;
				break;
			}
		}
		error = copyout(&wreq, ifr->ifr_data, sizeof(wreq));
		break;
	case SIOCSWAVELAN:
		if ((error = suser(p)))
			goto out;
		error = copyin(ifr->ifr_data, &wreq, sizeof(wreq));
		if (error)
			break;
		if (wreq.wi_type == WI_RID_IFACE_STATS) {
			error = EINVAL;
			break;
		} else if (wreq.wi_type == WI_RID_MGMT_XMIT) {
			error = wi_mgmt_xmit(sc, (caddr_t)&wreq.wi_val,
			    wreq.wi_len);
		} else {
			error = wi_write_record(sc, (struct wi_ltv_gen *)&wreq);
			if (!error)
				wi_setdef(sc, &wreq);
		}
		break;
	default:
		error = EINVAL;
		break;
	}
out:
	splx(s);

	return(error);
}

static void wi_init(xsc)
	void			*xsc;
{
	struct wi_softc		*sc = xsc;
	struct ifnet		*ifp = &sc->arpcom.ac_if;
	int			s;
	struct wi_ltv_macaddr	mac;
	int			id = 0;

	if (sc->wi_gone)
		return;

	s = splimp();

	if (ifp->if_flags & IFF_RUNNING)
		wi_stop(sc);

	wi_reset(sc);

	/* Program max data length. */
	WI_SETVAL(WI_RID_MAX_DATALEN, sc->wi_max_data_len);

	/* Enable/disable IBSS creation. */
	WI_SETVAL(WI_RID_CREATE_IBSS, sc->wi_create_ibss);

	/* Set the port type. */
	WI_SETVAL(WI_RID_PORTTYPE, sc->wi_ptype);

	/* Program the RTS/CTS threshold. */
	WI_SETVAL(WI_RID_RTS_THRESH, sc->wi_rts_thresh);

	/* Program the TX rate */
	WI_SETVAL(WI_RID_TX_RATE, sc->wi_tx_rate);

	/* Access point density */
	WI_SETVAL(WI_RID_SYSTEM_SCALE, sc->wi_ap_density);

	/* Power Management Enabled */
	WI_SETVAL(WI_RID_PM_ENABLED, sc->wi_pm_enabled);

	/* Power Managment Max Sleep */
	WI_SETVAL(WI_RID_MAX_SLEEP, sc->wi_max_sleep);

	/* Specify the IBSS name */
	WI_SETSTR(WI_RID_OWN_SSID, sc->wi_ibss_name);

	/* Specify the network name */
	WI_SETSTR(WI_RID_DESIRED_SSID, sc->wi_net_name);

	/* Specify the frequency to use */
	WI_SETVAL(WI_RID_OWN_CHNL, sc->wi_channel);

	/* Program the nodename. */
	WI_SETSTR(WI_RID_NODENAME, sc->wi_node_name);

	/* Set our MAC address. */
	mac.wi_len = 4;
	mac.wi_type = WI_RID_MAC_NODE;
	bcopy((char *)&sc->arpcom.ac_enaddr,
	   (char *)&mac.wi_mac_addr, ETHER_ADDR_LEN);
	wi_write_record(sc, (struct wi_ltv_gen *)&mac);

	/* Configure WEP. */
	if (sc->wi_has_wep) {
		WI_SETVAL(WI_RID_ENCRYPTION, sc->wi_use_wep);
		WI_SETVAL(WI_RID_TX_CRYPT_KEY, sc->wi_tx_key);
		sc->wi_keys.wi_len = (sizeof(struct wi_ltv_keys) / 2) + 1;
		sc->wi_keys.wi_type = WI_RID_DEFLT_CRYPT_KEYS;
		wi_write_record(sc, (struct wi_ltv_gen *)&sc->wi_keys);
	}

	/* Initialize promisc mode. */
	if (ifp->if_flags & IFF_PROMISC) {
		WI_SETVAL(WI_RID_PROMISC, 1);
	} else {
		WI_SETVAL(WI_RID_PROMISC, 0);
	}

	/* Set multicast filter. */
	wi_setmulti(sc);

	/* Enable desired port */
	wi_cmd(sc, WI_CMD_ENABLE|sc->wi_portnum, 0);

	if (wi_alloc_nicmem(sc, 1518 + sizeof(struct wi_frame) + 8, &id))
		device_printf(sc->dev, "tx buffer allocation failed\n");
	sc->wi_tx_data_id = id;

	if (wi_alloc_nicmem(sc, 1518 + sizeof(struct wi_frame) + 8, &id))
		device_printf(sc->dev, "mgmt. buffer allocation failed\n");
	sc->wi_tx_mgmt_id = id;

	/* enable interrupts */
	CSR_WRITE_2(sc, WI_INT_EN, WI_INTRS);

	splx(s);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	sc->wi_stat_ch = timeout(wi_inquire, sc, hz * 60);

	return;
}

static void wi_start(ifp)
	struct ifnet		*ifp;
{
	struct wi_softc		*sc;
	struct mbuf		*m0;
	struct wi_frame		tx_frame;
	struct ether_header	*eh;
	int			id;

	sc = ifp->if_softc;

	if (sc->wi_gone)
		return;

	if (ifp->if_flags & IFF_OACTIVE)
		return;

	IF_DEQUEUE(&ifp->if_snd, m0);
	if (m0 == NULL)
		return;

	bzero((char *)&tx_frame, sizeof(tx_frame));
	id = sc->wi_tx_data_id;
	eh = mtod(m0, struct ether_header *);

	/*
	 * Use RFC1042 encoding for IP and ARP datagrams,
	 * 802.3 for anything else.
	 */
	if (ntohs(eh->ether_type) > 1518) {
		bcopy((char *)&eh->ether_dhost,
		    (char *)&tx_frame.wi_addr1, ETHER_ADDR_LEN);
		bcopy((char *)&eh->ether_shost,
		    (char *)&tx_frame.wi_addr2, ETHER_ADDR_LEN);
		bcopy((char *)&eh->ether_dhost,
		    (char *)&tx_frame.wi_dst_addr, ETHER_ADDR_LEN);
		bcopy((char *)&eh->ether_shost,
		    (char *)&tx_frame.wi_src_addr, ETHER_ADDR_LEN);

		tx_frame.wi_dat_len = m0->m_pkthdr.len - WI_SNAPHDR_LEN;
		tx_frame.wi_frame_ctl = WI_FTYPE_DATA;
		tx_frame.wi_dat[0] = htons(WI_SNAP_WORD0);
		tx_frame.wi_dat[1] = htons(WI_SNAP_WORD1);
		tx_frame.wi_len = htons(m0->m_pkthdr.len - WI_SNAPHDR_LEN);
		tx_frame.wi_type = eh->ether_type;

		m_copydata(m0, sizeof(struct ether_header),
		    m0->m_pkthdr.len - sizeof(struct ether_header),
		    (caddr_t)&sc->wi_txbuf);

		wi_write_data(sc, id, 0, (caddr_t)&tx_frame,
		    sizeof(struct wi_frame));
		wi_write_data(sc, id, WI_802_11_OFFSET, (caddr_t)&sc->wi_txbuf,
		    (m0->m_pkthdr.len - sizeof(struct ether_header)) + 2);
	} else {
		tx_frame.wi_dat_len = m0->m_pkthdr.len;

		eh->ether_type = htons(m0->m_pkthdr.len - WI_SNAPHDR_LEN);
		m_copydata(m0, 0, m0->m_pkthdr.len, (caddr_t)&sc->wi_txbuf);

		wi_write_data(sc, id, 0, (caddr_t)&tx_frame,
		    sizeof(struct wi_frame));
		wi_write_data(sc, id, WI_802_3_OFFSET, (caddr_t)&sc->wi_txbuf,
		    m0->m_pkthdr.len + 2);
	}

	/*
	 * If there's a BPF listner, bounce a copy of
	 * this frame to him.
	 */
	if (ifp->if_bpf)
		bpf_mtap(ifp, m0);

	m_freem(m0);

	if (wi_cmd(sc, WI_CMD_TX|WI_RECLAIM, id))
		device_printf(sc->dev, "xmit failed\n");

	ifp->if_flags |= IFF_OACTIVE;

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;

	return;
}

static int wi_mgmt_xmit(sc, data, len)
	struct wi_softc		*sc;
	caddr_t			data;
	int			len;
{
	struct wi_frame		tx_frame;
	int			id;
	struct wi_80211_hdr	*hdr;
	caddr_t			dptr;

	if (sc->wi_gone)
		return(ENODEV);

	hdr = (struct wi_80211_hdr *)data;
	dptr = data + sizeof(struct wi_80211_hdr);

	bzero((char *)&tx_frame, sizeof(tx_frame));
	id = sc->wi_tx_mgmt_id;

	bcopy((char *)hdr, (char *)&tx_frame.wi_frame_ctl,
	   sizeof(struct wi_80211_hdr));

	tx_frame.wi_dat_len = len - WI_SNAPHDR_LEN;
	tx_frame.wi_len = htons(len - WI_SNAPHDR_LEN);

	wi_write_data(sc, id, 0, (caddr_t)&tx_frame, sizeof(struct wi_frame));
	wi_write_data(sc, id, WI_802_11_OFFSET_RAW, dptr,
	    (len - sizeof(struct wi_80211_hdr)) + 2);

	if (wi_cmd(sc, WI_CMD_TX|WI_RECLAIM, id)) {
		device_printf(sc->dev, "xmit failed\n");
		return(EIO);
	}

	return(0);
}

static void wi_stop(sc)
	struct wi_softc		*sc;
{
	struct ifnet		*ifp;

	if (sc->wi_gone)
		return;

	ifp = &sc->arpcom.ac_if;

	CSR_WRITE_2(sc, WI_INT_EN, 0);
	wi_cmd(sc, WI_CMD_DISABLE|sc->wi_portnum, 0);

	untimeout(wi_inquire, sc, sc->wi_stat_ch);

	ifp->if_flags &= ~(IFF_RUNNING|IFF_OACTIVE);

	return;
}

static void wi_watchdog(ifp)
	struct ifnet		*ifp;
{
	struct wi_softc		*sc;

	sc = ifp->if_softc;

	device_printf(sc->dev,"device timeout\n");

	wi_init(sc);

	ifp->if_oerrors++;

	return;
}

static int wi_alloc(dev)
	device_t		dev;
{
	struct wi_softc		*sc = device_get_softc(dev);
	int			rid;

	rid = 0;
	sc->iobase = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid,
					0, ~0, 1, RF_ACTIVE);
	if (!sc->iobase) {
		device_printf(dev, "No I/O space?!\n");
		return (ENXIO);
	}

	rid = 0;
	sc->irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid,
				     0, ~0, 1, RF_ACTIVE);
	if (!sc->irq) {
		device_printf(dev, "No irq?!\n");
		return (ENXIO);
	}

	sc->dev = dev;
	sc->wi_unit = device_get_unit(dev);
	sc->wi_io_addr = rman_get_start(sc->iobase);
	sc->wi_btag = rman_get_bustag(sc->iobase);
	sc->wi_bhandle = rman_get_bushandle(sc->iobase);

	return (0);
}

static void wi_free(dev)
	device_t		dev;
{
	struct wi_softc		*sc = device_get_softc(dev);

	if (sc->iobase != NULL)
		bus_release_resource(dev, SYS_RES_IOPORT, 0, sc->iobase);
	if (sc->irq != NULL)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->irq);

	return;
}

static void wi_shutdown(dev)
	device_t		dev;
{
	struct wi_softc		*sc;

	sc = device_get_softc(dev);
	wi_stop(sc);

	return;
}

#ifdef WICACHE
/* wavelan signal strength cache code.
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
 */

#ifdef documentation

int wi_sigitems;                                /* number of cached entries */
struct wi_sigcache wi_sigcache[MAXWICACHE];  /*  array of cache entries */
int wi_nextitem;                                /*  index/# of entries */


#endif

/* control variables for cache filtering.  Basic idea is
 * to reduce cost (e.g., to only Mobile-IP agent beacons
 * which are broadcast or multicast).  Still you might
 * want to measure signal strength with unicast ping packets
 * on a pt. to pt. ant. setup.
 */
/* set true if you want to limit cache items to broadcast/mcast 
 * only packets (not unicast).  Useful for mobile-ip beacons which
 * are broadcast/multicast at network layer.  Default is all packets
 * so ping/unicast will work say with pt. to pt. antennae setup.
 */
static int wi_cache_mcastonly = 0;
SYSCTL_INT(_machdep, OID_AUTO, wi_cache_mcastonly, CTLFLAG_RW, 
	&wi_cache_mcastonly, 0, "");

/* set true if you want to limit cache items to IP packets only
*/
static int wi_cache_iponly = 1;
SYSCTL_INT(_machdep, OID_AUTO, wi_cache_iponly, CTLFLAG_RW, 
	&wi_cache_iponly, 0, "");

/*
 * Original comments:
 * -----------------
 * wi_cache_store, per rx packet store signal
 * strength in MAC (src) indexed cache.
 *
 * follows linux driver in how signal strength is computed.
 * In ad hoc mode, we use the rx_quality field. 
 * signal and noise are trimmed to fit in the range from 47..138.
 * rx_quality field MSB is signal strength.
 * rx_quality field LSB is noise.
 * "quality" is (signal - noise) as is log value.
 * note: quality CAN be negative.
 * 
 * In BSS mode, we use the RID for communication quality.
 * TBD:  BSS mode is currently untested.
 *
 * Bill's comments:
 * ---------------
 * Actually, we use the rx_quality field all the time for both "ad-hoc"
 * and BSS modes. Why? Because reading an RID is really, really expensive:
 * there's a bunch of PIO operations that have to be done to read a record
 * from the NIC, and reading the comms quality RID each time a packet is
 * received can really hurt performance. We don't have to do this anyway:
 * the comms quality field only reflects the values in the rx_quality field
 * anyway. The comms quality RID is only meaningful in infrastructure mode,
 * but the values it contains are updated based on the rx_quality from
 * frames received from the access point.
 *
 * Also, according to Lucent, the signal strength and noise level values
 * can be converted to dBms by subtracting 149, so I've modified the code
 * to do that instead of the scaling it did originally.
 */
static  
void wi_cache_store (struct wi_softc *sc, struct ether_header *eh,
                     struct mbuf *m, unsigned short rx_quality)
{
	struct ip *ip = 0; 
	int i;
	static int cache_slot = 0; 	/* use this cache entry */
	static int wrapindex = 0;       /* next "free" cache entry */
	int sig, noise;
	int sawip=0;

	/* filters:
	 * 1. ip only
	 * 2. configurable filter to throw out unicast packets,
	 * keep multicast only.
	 */
 
	if ((ntohs(eh->ether_type) == 0x800)) {
		sawip = 1;
	}

	/* filter for ip packets only 
	*/
	if (wi_cache_iponly && !sawip) {
		return;
	}

	/* filter for broadcast/multicast only
	 */
	if (wi_cache_mcastonly && ((eh->ether_dhost[0] & 1) == 0)) {
		return;
	}

#ifdef SIGDEBUG
	printf("wi%d: q value %x (MSB=0x%x, LSB=0x%x) \n", sc->wi_unit,
	    rx_quality & 0xffff, rx_quality >> 8, rx_quality & 0xff);
#endif

	/* find the ip header.  we want to store the ip_src
	 * address.  
	 */
	if (sawip) {
		ip = mtod(m, struct ip *);
	}
        
	/* do a linear search for a matching MAC address 
	 * in the cache table
	 * . MAC address is 6 bytes,
	 * . var w_nextitem holds total number of entries already cached
	 */
	for(i = 0; i < sc->wi_nextitem; i++) {
		if (! bcmp(eh->ether_shost , sc->wi_sigcache[i].macsrc,  6 )) {
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
	if (i < sc->wi_nextitem )   {
		cache_slot = i; 
	}
	/* else, have a new address entry,so
	 * add this new entry,
	 * if table full, then we need to replace LRU entry
	 */
	else    {                          

		/* check for space in cache table 
		 * note: wi_nextitem also holds number of entries
		 * added in the cache table 
		 */
		if ( sc->wi_nextitem < MAXWICACHE ) {
			cache_slot = sc->wi_nextitem;
			sc->wi_nextitem++;                 
			sc->wi_sigitems = sc->wi_nextitem;
		}
        	/* no space found, so simply wrap with wrap index
		 * and "zap" the next entry
		 */
		else {
			if (wrapindex == MAXWICACHE) {
				wrapindex = 0;
			}
			cache_slot = wrapindex++;
		}
	}

	/* invariant: cache_slot now points at some slot
	 * in cache.
	 */
	if (cache_slot < 0 || cache_slot >= MAXWICACHE) {
		log(LOG_ERR, "wi_cache_store, bad index: %d of "
		    "[0..%d], gross cache error\n",
		    cache_slot, MAXWICACHE);
		return;
	}

	/*  store items in cache
	 *  .ip source address
	 *  .mac src
	 *  .signal, etc.
	 */
	if (sawip) {
		sc->wi_sigcache[cache_slot].ipsrc = ip->ip_src.s_addr;
	}
	bcopy( eh->ether_shost, sc->wi_sigcache[cache_slot].macsrc,  6);

	sig = (rx_quality >> 8) & 0xFF;
	noise = rx_quality & 0xFF;
	sc->wi_sigcache[cache_slot].signal = sig - 149;
	sc->wi_sigcache[cache_slot].noise = noise - 149;
	sc->wi_sigcache[cache_slot].quality = sig - noise;

	return;
}
#endif
