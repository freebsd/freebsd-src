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
 * SysKonnect SK-NET gigabit ethernet driver for FreeBSD. Supports
 * the SK-984x series adapters, both single port and dual port.
 * References:
 * 	The XaQti XMAC II datasheet, http://www.xaqti.com
 *	The SysKonnect GEnesis manual, http://www.syskonnect.com
 *
 * Written by Bill Paul <wpaul@ee.columbia.edu>
 * Department of Electrical Engineering
 * Columbia University, New York City
 */

/*
 * The SysKonnect gigabit ethernet adapters consist of two main
 * components: the SysKonnect GEnesis controller chip and the XaQti Corp.
 * XMAC II gigabit ethernet MAC. The XMAC provides all of the MAC
 * components and a PHY while the GEnesis controller provides a PCI
 * interface with DMA support. Each card may have between 512K and
 * 2MB of SRAM on board depending on the configuration.
 *
 * The SysKonnect GEnesis controller can have either one or two XMAC
 * chips connected to it, allowing single or dual port NIC configurations.
 * SysKonnect has the distinction of being the only vendor on the market
 * with a dual port gigabit ethernet NIC. The GEnesis provides dual FIFOs,
 * dual DMA queues, packet/MAC/transmit arbiters and direct access to the
 * XMAC registers. This driver takes advantage of these features to allow
 * both XMACs to operate as independent interfaces.
 */
 
#include "bpf.h"

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

#if NBPF > 0
#include <net/bpf.h>
#endif

#include <vm/vm.h>              /* for vtophys */
#include <vm/pmap.h>            /* for vtophys */
#include <machine/clock.h>      /* for DELAY */
#include <machine/bus_pio.h>
#include <machine/bus_memio.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>

#define SK_USEIOSPACE

#include <pci/if_skreg.h>
#include <pci/xmaciireg.h>

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif

static struct sk_type sk_devs[] = {
	{ SK_VENDORID, SK_DEVICEID_GE, "SysKonnect Gigabit Ethernet" },
	{ 0, 0, NULL }
};

static int sk_probe		__P((device_t));
static int sk_attach		__P((device_t));
static int sk_detach		__P((device_t));
static int sk_attach_xmac	__P((struct sk_softc *, int));
static void sk_intr		__P((void *));
static void sk_intr_xmac	__P((struct sk_if_softc *));
static void sk_rxeof		__P((struct sk_if_softc *));
static void sk_txeof		__P((struct sk_if_softc *));
static int sk_encap		__P((struct sk_if_softc *, struct mbuf *,
					u_int32_t *));
static void sk_start		__P((struct ifnet *));
static int sk_ioctl		__P((struct ifnet *, u_long, caddr_t));
static void sk_init		__P((void *));
static void sk_init_xmac	__P((struct sk_if_softc *));
static void sk_stop		__P((struct sk_if_softc *));
static void sk_watchdog		__P((struct ifnet *));
static void sk_shutdown		__P((device_t));
static int sk_ifmedia_upd	__P((struct ifnet *));
static void sk_ifmedia_sts	__P((struct ifnet *, struct ifmediareq *));
static void sk_reset		__P((struct sk_softc *));
static int sk_newbuf		__P((struct sk_if_softc *,
					struct sk_chain *, struct mbuf *));
static int sk_alloc_jumbo_mem	__P((struct sk_if_softc *));
static void *sk_jalloc		__P((struct sk_if_softc *));
static void sk_jfree		__P((caddr_t, u_int));
static void sk_jref		__P((caddr_t, u_int));
static int sk_init_rx_ring	__P((struct sk_if_softc *));
static void sk_init_tx_ring	__P((struct sk_if_softc *));
#ifdef notdef
static u_int32_t sk_win_read_4	__P((struct sk_softc *, int));
#endif
static u_int16_t sk_win_read_2	__P((struct sk_softc *, int));
static u_int8_t sk_win_read_1	__P((struct sk_softc *, int));
static void sk_win_write_4	__P((struct sk_softc *, int, u_int32_t));
static void sk_win_write_2	__P((struct sk_softc *, int, u_int32_t));
static void sk_win_write_1	__P((struct sk_softc *, int, u_int32_t));
static u_int8_t sk_vpd_readbyte	__P((struct sk_softc *, int));
static void sk_vpd_read_res	__P((struct sk_softc *,
					struct vpd_res *, int));
static void sk_vpd_read		__P((struct sk_softc *));
static u_int16_t sk_phy_readreg	__P((struct sk_if_softc *, int));
static void sk_phy_writereg	__P((struct sk_if_softc *, int, u_int32_t));
static u_int32_t sk_calchash	__P((caddr_t));
static void sk_setfilt		__P((struct sk_if_softc *, caddr_t, int));
static void sk_setmulti		__P((struct sk_if_softc *));

#ifdef SK_USEIOSPACE
#define SK_RES		SYS_RES_IOPORT
#define SK_RID		SK_PCI_LOIO
#else
#define SK_RES		SYS_RES_MEMORY
#define SK_RID		SK_PCI_LOMEM
#endif

static device_method_t sk_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		sk_probe),
	DEVMETHOD(device_attach,	sk_attach),
	DEVMETHOD(device_detach,	sk_detach),
	DEVMETHOD(device_shutdown,	sk_shutdown),
	{ 0, 0 }
};

static driver_t sk_driver = {
	"skc",
	sk_methods,
	sizeof(struct sk_softc)
};

static devclass_t sk_devclass;

DRIVER_MODULE(if_sk, pci, sk_driver, sk_devclass, 0, 0);

#define SK_SETBIT(sc, reg, x)		\
	CSR_WRITE_4(sc, reg, CSR_READ_4(sc, reg) | x)

#define SK_CLRBIT(sc, reg, x)		\
	CSR_WRITE_4(sc, reg, CSR_READ_4(sc, reg) & ~x)

#define SK_WIN_SETBIT_4(sc, reg, x)	\
	sk_win_write_4(sc, reg, sk_win_read_4(sc, reg) | x)

#define SK_WIN_CLRBIT_4(sc, reg, x)	\
	sk_win_write_4(sc, reg, sk_win_read_4(sc, reg) & ~x)

#define SK_WIN_SETBIT_2(sc, reg, x)	\
	sk_win_write_2(sc, reg, sk_win_read_2(sc, reg) | x)

#define SK_WIN_CLRBIT_2(sc, reg, x)	\
	sk_win_write_2(sc, reg, sk_win_read_2(sc, reg) & ~x)

#ifdef notdef
static u_int32_t sk_win_read_4(sc, reg)
	struct sk_softc		*sc;
	int			reg;
{
	CSR_WRITE_4(sc, SK_RAP, SK_WIN(reg));
	return(CSR_READ_4(sc, SK_WIN_BASE + SK_REG(reg)));
}
#endif

static u_int16_t sk_win_read_2(sc, reg)
	struct sk_softc		*sc;
	int			reg;
{
	CSR_WRITE_4(sc, SK_RAP, SK_WIN(reg));
	return(CSR_READ_2(sc, SK_WIN_BASE + SK_REG(reg)));
}

static u_int8_t sk_win_read_1(sc, reg)
	struct sk_softc		*sc;
	int			reg;
{
	CSR_WRITE_4(sc, SK_RAP, SK_WIN(reg));
	return(CSR_READ_1(sc, SK_WIN_BASE + SK_REG(reg)));
}

static void sk_win_write_4(sc, reg, val)
	struct sk_softc		*sc;
	int			reg;
	u_int32_t		val;
{
	CSR_WRITE_4(sc, SK_RAP, SK_WIN(reg));
	CSR_WRITE_4(sc, SK_WIN_BASE + SK_REG(reg), val);
	return;
}

static void sk_win_write_2(sc, reg, val)
	struct sk_softc		*sc;
	int			reg;
	u_int32_t		val;
{
	CSR_WRITE_4(sc, SK_RAP, SK_WIN(reg));
	CSR_WRITE_2(sc, SK_WIN_BASE + SK_REG(reg), (u_int32_t)val);
	return;
}

static void sk_win_write_1(sc, reg, val)
	struct sk_softc		*sc;
	int			reg;
	u_int32_t		val;
{
	CSR_WRITE_4(sc, SK_RAP, SK_WIN(reg));
	CSR_WRITE_1(sc, SK_WIN_BASE + SK_REG(reg), val);
	return;
}

/*
 * The VPD EEPROM contains Vital Product Data, as suggested in
 * the PCI 2.1 specification. The VPD data is separared into areas
 * denoted by resource IDs. The SysKonnect VPD contains an ID string
 * resource (the name of the adapter), a read-only area resource
 * containing various key/data fields and a read/write area which
 * can be used to store asset management information or log messages.
 * We read the ID string and read-only into buffers attached to
 * the controller softc structure for later use. At the moment,
 * we only use the ID string during sk_attach().
 */
static u_int8_t sk_vpd_readbyte(sc, addr)
	struct sk_softc		*sc;
	int			addr;
{
	int			i;

	sk_win_write_2(sc, SK_PCI_REG(SK_PCI_VPD_ADDR), addr);
	for (i = 0; i < SK_TIMEOUT; i++) {
		DELAY(1);
		if (sk_win_read_2(sc,
		    SK_PCI_REG(SK_PCI_VPD_ADDR)) & SK_VPD_FLAG)
			break;
	}

	if (i == SK_TIMEOUT)
		return(0);

	return(sk_win_read_1(sc, SK_PCI_REG(SK_PCI_VPD_DATA)));
}

static void sk_vpd_read_res(sc, res, addr)
	struct sk_softc		*sc;
	struct vpd_res		*res;
	int			addr;
{
	int			i;
	u_int8_t		*ptr;

	ptr = (u_int8_t *)res;
	for (i = 0; i < sizeof(struct vpd_res); i++)
		ptr[i] = sk_vpd_readbyte(sc, i + addr);

	return;
}

static void sk_vpd_read(sc)
	struct sk_softc		*sc;
{
	int			pos = 0, i;
	struct vpd_res		res;

	if (sc->sk_vpd_prodname != NULL)
		free(sc->sk_vpd_prodname, M_DEVBUF);
	if (sc->sk_vpd_readonly != NULL)
		free(sc->sk_vpd_readonly, M_DEVBUF);
	sc->sk_vpd_prodname = NULL;
	sc->sk_vpd_readonly = NULL;

	sk_vpd_read_res(sc, &res, pos);

	if (res.vr_id != VPD_RES_ID) {
		printf("skc%d: bad VPD resource id: expected %x got %x\n",
		    sc->sk_unit, VPD_RES_ID, res.vr_id);
		return;
	}

	pos += sizeof(res);
	sc->sk_vpd_prodname = malloc(res.vr_len + 1, M_DEVBUF, M_NOWAIT);
	for (i = 0; i < res.vr_len; i++)
		sc->sk_vpd_prodname[i] = sk_vpd_readbyte(sc, i + pos);
	sc->sk_vpd_prodname[i] = '\0';
	pos += i;

	sk_vpd_read_res(sc, &res, pos);

	if (res.vr_id != VPD_RES_READ) {
		printf("skc%d: bad VPD resource id: expected %x got %x\n",
		    sc->sk_unit, VPD_RES_READ, res.vr_id);
		return;
	}

	pos += sizeof(res);
	sc->sk_vpd_readonly = malloc(res.vr_len, M_DEVBUF, M_NOWAIT);
	for (i = 0; i < res.vr_len + 1; i++)
		sc->sk_vpd_readonly[i] = sk_vpd_readbyte(sc, i + pos);

	return;
}

static u_int16_t sk_phy_readreg(sc_if, reg)
	struct sk_if_softc	*sc_if;
	int			reg;
{
	int			i;

	SK_XM_WRITE_2(sc_if, XM_PHY_ADDR, reg);
	for (i = 0; i < SK_TIMEOUT; i++) {
		if (!(SK_XM_READ_2(sc_if, XM_MMUCMD) & XM_MMUCMD_PHYBUSY))
			break;
	}

	if (i == SK_TIMEOUT) {
		printf("sk%d: phy failed to come ready\n", sc_if->sk_unit);
		return(0);
	}

	return(SK_XM_READ_2(sc_if, XM_PHY_DATA));
}

static void sk_phy_writereg(sc_if, reg, val)
	struct sk_if_softc	*sc_if;
	int			reg;
	u_int32_t		val;
{
	int			i;

	SK_XM_WRITE_2(sc_if, XM_PHY_ADDR, reg);
	for (i = 0; i < SK_TIMEOUT; i++) {
		if (!(SK_XM_READ_2(sc_if, XM_MMUCMD) & XM_MMUCMD_PHYBUSY))
			break;
	}

	if (i == SK_TIMEOUT) {
		printf("sk%d: phy failed to come ready\n", sc_if->sk_unit);
		return;
	}

	SK_XM_WRITE_2(sc_if, XM_PHY_DATA, val);
	for (i = 0; i < SK_TIMEOUT; i++) {
		if (!(SK_XM_READ_2(sc_if, XM_MMUCMD) & XM_MMUCMD_PHYBUSY))
			break;
	}

	if (i == SK_TIMEOUT)
		printf("sk%d: phy write timed out\n", sc_if->sk_unit);

	return;
}

#define SK_POLY		0xEDB88320
#define SK_BITS		6

static u_int32_t sk_calchash(addr)
	caddr_t			addr;
{
	u_int32_t		idx, bit, data, crc;

	/* Compute CRC for the address value. */
	crc = 0xFFFFFFFF; /* initial value */

	for (idx = 0; idx < 6; idx++) {
		for (data = *addr++, bit = 0; bit < 8; bit++, data >>= 1)
			crc = (crc >> 1) ^ (((crc ^ data) & 1) ? SK_POLY : 0);
	}

	return (~crc & ((1 << SK_BITS) - 1));
}

static void sk_setfilt(sc_if, addr, slot)
	struct sk_if_softc	*sc_if;
	caddr_t			addr;
	int			slot;
{
	int			base;

	base = XM_RXFILT_ENTRY(slot);

	SK_XM_WRITE_2(sc_if, base, *(u_int16_t *)(&addr[0]));
	SK_XM_WRITE_2(sc_if, base + 2, *(u_int16_t *)(&addr[2]));
	SK_XM_WRITE_2(sc_if, base + 4, *(u_int16_t *)(&addr[4]));

	return;
}

static void sk_setmulti(sc_if)
	struct sk_if_softc	*sc_if;
{
	struct ifnet		*ifp;
	u_int32_t		hashes[2] = { 0, 0 };
	int			h, i;
	struct ifmultiaddr	*ifma;
	u_int8_t		dummy[] = { 0, 0, 0, 0, 0 ,0 };

	ifp = &sc_if->arpcom.ac_if;

	/* First, zot all the existing filters. */
	for (i = 1; i < XM_RXFILT_MAX; i++)
		sk_setfilt(sc_if, (caddr_t)&dummy, i);
	SK_XM_WRITE_4(sc_if, XM_MAR0, 0);
	SK_XM_WRITE_4(sc_if, XM_MAR2, 0);

	/* Now program new ones. */
	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
		hashes[0] = 0xFFFFFFFF;
		hashes[1] = 0xFFFFFFFF;
	} else {
		i = 1;
		/* First find the tail of the list. */
		for (ifma = ifp->if_multiaddrs.lh_first; ifma != NULL;
					ifma = ifma->ifma_link.le_next) {
			if (ifma->ifma_link.le_next == NULL)
				break;
		}
		/* Now traverse the list backwards. */
		for (; ifma != NULL && ifma != (void *)&ifp->if_multiaddrs;
			ifma = (struct ifmultiaddr *)ifma->ifma_link.le_prev) {
			if (ifma->ifma_addr->sa_family != AF_LINK)
				continue;
			/*
			 * Program the first XM_RXFILT_MAX multicast groups
			 * into the perfect filter. For all others,
			 * use the hash table.
			 */
			if (i < XM_RXFILT_MAX) {
				sk_setfilt(sc_if,
			LLADDR((struct sockaddr_dl *)ifma->ifma_addr), i);
				i++;
				continue;
			}

			h = sk_calchash(
				LLADDR((struct sockaddr_dl *)ifma->ifma_addr));
			if (h < 32)
				hashes[0] |= (1 << h);
			else
				hashes[1] |= (1 << (h - 32));
		}
	}

	SK_XM_SETBIT_4(sc_if, XM_MODE, XM_MODE_RX_USE_HASH|
	    XM_MODE_RX_USE_PERFECT);
	SK_XM_WRITE_4(sc_if, XM_MAR0, hashes[0]);
	SK_XM_WRITE_4(sc_if, XM_MAR2, hashes[1]);

	return;
}

static int sk_init_rx_ring(sc_if)
	struct sk_if_softc	*sc_if;
{
	struct sk_chain_data	*cd;
	struct sk_ring_data	*rd;
	int			i;

	cd = &sc_if->sk_cdata;
	rd = sc_if->sk_rdata;

	bzero((char *)rd->sk_rx_ring,
	    sizeof(struct sk_rx_desc) * SK_RX_RING_CNT);

	for (i = 0; i < SK_RX_RING_CNT; i++) {
		cd->sk_rx_chain[i].sk_desc = &rd->sk_rx_ring[i];
		if (sk_newbuf(sc_if, &cd->sk_rx_chain[i], NULL) == ENOBUFS)
			return(ENOBUFS);
		if (i == (SK_RX_RING_CNT - 1)) {
			cd->sk_rx_chain[i].sk_next =
			    &cd->sk_rx_chain[0];
			rd->sk_rx_ring[i].sk_next = 
			    vtophys(&rd->sk_rx_ring[0]);
		} else {
			cd->sk_rx_chain[i].sk_next =
			    &cd->sk_rx_chain[i + 1];
			rd->sk_rx_ring[i].sk_next = 
			    vtophys(&rd->sk_rx_ring[i + 1]);
		}
	}

	sc_if->sk_cdata.sk_rx_prod = 0;
	sc_if->sk_cdata.sk_rx_cons = 0;

	return(0);
}

static void sk_init_tx_ring(sc_if)
	struct sk_if_softc	*sc_if;
{
	struct sk_chain_data	*cd;
	struct sk_ring_data	*rd;
	int			i;

	cd = &sc_if->sk_cdata;
	rd = sc_if->sk_rdata;

	bzero((char *)sc_if->sk_rdata->sk_tx_ring,
	    sizeof(struct sk_tx_desc) * SK_TX_RING_CNT);

	for (i = 0; i < SK_TX_RING_CNT; i++) {
		cd->sk_tx_chain[i].sk_desc = &rd->sk_tx_ring[i];
		if (i == (SK_TX_RING_CNT - 1)) {
			cd->sk_tx_chain[i].sk_next =
			    &cd->sk_tx_chain[0];
			rd->sk_tx_ring[i].sk_next = 
			    vtophys(&rd->sk_tx_ring[0]);
		} else {
			cd->sk_tx_chain[i].sk_next =
			    &cd->sk_tx_chain[i + 1];
			rd->sk_tx_ring[i].sk_next = 
			    vtophys(&rd->sk_tx_ring[i + 1]);
		}
	}

	sc_if->sk_cdata.sk_tx_prod = 0;
	sc_if->sk_cdata.sk_tx_cons = 0;
	sc_if->sk_cdata.sk_tx_cnt = 0;

	return;
}

static int sk_newbuf(sc_if, c, m)
	struct sk_if_softc	*sc_if;
	struct sk_chain		*c;
	struct mbuf		*m;
{
	struct mbuf		*m_new = NULL;
	struct sk_rx_desc	*r;

	if (m == NULL) {
		caddr_t			*buf = NULL;

		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL) {
			printf("sk%d: no memory for rx list -- "
			    "packet dropped!\n", sc_if->sk_unit);
			return(ENOBUFS);
		}

		/* Allocate the jumbo buffer */
		buf = sk_jalloc(sc_if);
		if (buf == NULL) {
			m_freem(m_new);
#ifdef SK_VERBOSE
			printf("sk%d: jumbo allocation failed "
			    "-- packet dropped!\n", sc_if->sk_unit);
#endif
			return(ENOBUFS);
		}

		/* Attach the buffer to the mbuf */
		m_new->m_data = m_new->m_ext.ext_buf = (void *)buf;
		m_new->m_flags |= M_EXT;
		m_new->m_ext.ext_size = m_new->m_pkthdr.len =
		    m_new->m_len = SK_MCLBYTES;
		m_new->m_ext.ext_free = sk_jfree;
		m_new->m_ext.ext_ref = sk_jref;
	} else {
		/*
	 	 * We're re-using a previously allocated mbuf;
		 * be sure to re-init pointers and lengths to
		 * default values.
		 */
		m_new = m;
		m_new->m_len = m_new->m_pkthdr.len = SK_MCLBYTES;
		m_new->m_data = m_new->m_ext.ext_buf;
	}

	/*
	 * Adjust alignment so packet payload begins on a
	 * longword boundary. Mandatory for Alpha, useful on
	 * x86 too.
	 */
	m_adj(m_new, ETHER_ALIGN);

	r = c->sk_desc;
	c->sk_mbuf = m_new;
	r->sk_data_lo = vtophys(mtod(m_new, caddr_t));
	r->sk_ctl = m_new->m_len | SK_RXSTAT;

	return(0);
}

/*
 * Allocate jumbo buffer storage. The SysKonnect adapters support
 * "jumbograms" (9K frames), although SysKonnect doesn't currently
 * use them in their drivers. In order for us to use them, we need
 * large 9K receive buffers, however standard mbuf clusters are only
 * 2048 bytes in size. Consequently, we need to allocate and manage
 * our own jumbo buffer pool. Fortunately, this does not require an
 * excessive amount of additional code.
 */
static int sk_alloc_jumbo_mem(sc_if)
	struct sk_if_softc	*sc_if;
{
	caddr_t			ptr;
	register int		i;
	struct sk_jpool_entry   *entry;

	/* Grab a big chunk o' storage. */
	sc_if->sk_cdata.sk_jumbo_buf = contigmalloc(SK_JMEM, M_DEVBUF,
	    M_NOWAIT, 0, 0xffffffff, PAGE_SIZE, 0);

	if (sc_if->sk_cdata.sk_jumbo_buf == NULL) {
		printf("sk%d: no memory for jumbo buffers!\n", sc_if->sk_unit);
		return(ENOBUFS);
	}

	SLIST_INIT(&sc_if->sk_jfree_listhead);
	SLIST_INIT(&sc_if->sk_jinuse_listhead);

	/*
	 * Now divide it up into 9K pieces and save the addresses
	 * in an array. Note that we play an evil trick here by using
	 * the first few bytes in the buffer to hold the the address
	 * of the softc structure for this interface. This is because
	 * sk_jfree() needs it, but it is called by the mbuf management
	 * code which will not pass it to us explicitly.
	 */
	ptr = sc_if->sk_cdata.sk_jumbo_buf;
	for (i = 0; i < SK_JSLOTS; i++) {
		u_int64_t		**aptr;
		aptr = (u_int64_t **)ptr;
		aptr[0] = (u_int64_t *)sc_if;
		ptr += sizeof(u_int64_t);
		sc_if->sk_cdata.sk_jslots[i].sk_buf = ptr;
		sc_if->sk_cdata.sk_jslots[i].sk_inuse = 0;
		ptr += SK_MCLBYTES;
		entry = malloc(sizeof(struct sk_jpool_entry), 
		    M_DEVBUF, M_NOWAIT);
		if (entry == NULL) {
			free(sc_if->sk_cdata.sk_jumbo_buf, M_DEVBUF);
			sc_if->sk_cdata.sk_jumbo_buf = NULL;
			printf("sk%d: no memory for jumbo "
			    "buffer queue!\n", sc_if->sk_unit);
			return(ENOBUFS);
		}
		entry->slot = i;
		SLIST_INSERT_HEAD(&sc_if->sk_jfree_listhead,
		    entry, jpool_entries);
	}

	return(0);
}

/*
 * Allocate a jumbo buffer.
 */
static void *sk_jalloc(sc_if)
	struct sk_if_softc	*sc_if;
{
	struct sk_jpool_entry   *entry;
	
	entry = SLIST_FIRST(&sc_if->sk_jfree_listhead);
	
	if (entry == NULL) {
#ifdef SK_VERBOSE
		printf("sk%d: no free jumbo buffers\n", sc_if->sk_unit);
#endif
		return(NULL);
	}

	SLIST_REMOVE_HEAD(&sc_if->sk_jfree_listhead, jpool_entries);
	SLIST_INSERT_HEAD(&sc_if->sk_jinuse_listhead, entry, jpool_entries);
	sc_if->sk_cdata.sk_jslots[entry->slot].sk_inuse = 1;
	return(sc_if->sk_cdata.sk_jslots[entry->slot].sk_buf);
}

/*
 * Adjust usage count on a jumbo buffer. In general this doesn't
 * get used much because our jumbo buffers don't get passed around
 * a lot, but it's implemented for correctness.
 */
static void sk_jref(buf, size)
	caddr_t			buf;
	u_int			size;
{
	struct sk_if_softc	*sc_if;
	u_int64_t		**aptr;
	register int		i;

	/* Extract the softc struct pointer. */
	aptr = (u_int64_t **)(buf - sizeof(u_int64_t));
	sc_if = (struct sk_if_softc *)(aptr[0]);

	if (sc_if == NULL)
		panic("sk_jref: can't find softc pointer!");

	if (size != SK_MCLBYTES)
		panic("sk_jref: adjusting refcount of buf of wrong size!");

	/* calculate the slot this buffer belongs to */

	i = ((vm_offset_t)aptr 
	     - (vm_offset_t)sc_if->sk_cdata.sk_jumbo_buf) / SK_JLEN;

	if ((i < 0) || (i >= SK_JSLOTS))
		panic("sk_jref: asked to reference buffer "
		    "that we don't manage!");
	else if (sc_if->sk_cdata.sk_jslots[i].sk_inuse == 0)
		panic("sk_jref: buffer already free!");
	else
		sc_if->sk_cdata.sk_jslots[i].sk_inuse++;

	return;
}

/*
 * Release a jumbo buffer.
 */
static void sk_jfree(buf, size)
	caddr_t			buf;
	u_int			size;
{
	struct sk_if_softc	*sc_if;
	u_int64_t		**aptr;
	int		        i;
	struct sk_jpool_entry   *entry;

	/* Extract the softc struct pointer. */
	aptr = (u_int64_t **)(buf - sizeof(u_int64_t));
	sc_if = (struct sk_if_softc *)(aptr[0]);

	if (sc_if == NULL)
		panic("sk_jfree: can't find softc pointer!");

	if (size != SK_MCLBYTES)
		panic("sk_jfree: freeing buffer of wrong size!");

	/* calculate the slot this buffer belongs to */

	i = ((vm_offset_t)aptr 
	     - (vm_offset_t)sc_if->sk_cdata.sk_jumbo_buf) / SK_JLEN;

	if ((i < 0) || (i >= SK_JSLOTS))
		panic("sk_jfree: asked to free buffer that we don't manage!");
	else if (sc_if->sk_cdata.sk_jslots[i].sk_inuse == 0)
		panic("sk_jfree: buffer already free!");
	else {
		sc_if->sk_cdata.sk_jslots[i].sk_inuse--;
		if(sc_if->sk_cdata.sk_jslots[i].sk_inuse == 0) {
			entry = SLIST_FIRST(&sc_if->sk_jinuse_listhead);
			if (entry == NULL)
				panic("sk_jfree: buffer not in use!");
			entry->slot = i;
			SLIST_REMOVE_HEAD(&sc_if->sk_jinuse_listhead, 
					  jpool_entries);
			SLIST_INSERT_HEAD(&sc_if->sk_jfree_listhead, 
					  entry, jpool_entries);
		}
	}

	return;
}

/*
 * Set media options.
 */
static int sk_ifmedia_upd(ifp)
	struct ifnet		*ifp;
{
	struct sk_if_softc	*sc_if;
	struct ifmedia		*ifm;

	sc_if = ifp->if_softc;
	ifm = &sc_if->ifmedia;

	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
		return(EINVAL);

	switch(IFM_SUBTYPE(ifm->ifm_media)) {
	case IFM_AUTO:
		sk_phy_writereg(sc_if, XM_PHY_BMCR,
		    XM_BMCR_RENEGOTIATE|XM_BMCR_AUTONEGENBL);
		break;
	case IFM_1000_LX:
	case IFM_1000_SX:
	case IFM_1000_CX:
	case IFM_1000_TX:
		if ((ifm->ifm_media & IFM_GMASK) == IFM_FDX)
			sk_phy_writereg(sc_if, XM_PHY_BMCR, XM_BMCR_DUPLEX);
		else
			sk_phy_writereg(sc_if, XM_PHY_BMCR, 0);
		break;
	default:
		printf("sk%d: invalid media selected\n", sc_if->sk_unit);
		return(EINVAL);
		break;
	}

	return(0);
}

/*
 * Report current media status.
 */
static void sk_ifmedia_sts(ifp, ifmr)
	struct ifnet		*ifp;
	struct ifmediareq	*ifmr;
{
	struct sk_softc		*sc;
	struct sk_if_softc	*sc_if;
	u_int16_t		bmsr, extsts;

	sc_if = ifp->if_softc;
	sc = sc_if->sk_softc;

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	bmsr = sk_phy_readreg(sc_if, XM_PHY_BMSR);
	extsts = sk_phy_readreg(sc_if, XM_PHY_EXTSTS);

	if (!(bmsr & XM_BMSR_LINKSTAT))
		return;

	ifmr->ifm_status |= IFM_ACTIVE;
	ifmr->ifm_active |= sc->sk_pmd;;
	if (extsts & XM_EXTSTS_FULLDUPLEX)
		ifmr->ifm_active |= IFM_FDX;
	else
		ifmr->ifm_active |= IFM_HDX;

	return;
}

static int sk_ioctl(ifp, command, data)
	struct ifnet		*ifp;
	u_long			command;
	caddr_t			data;
{
	struct sk_if_softc	*sc_if = ifp->if_softc;
	struct ifreq		*ifr = (struct ifreq *) data;
	int			s, error = 0;

	s = splimp();

	switch(command) {
	case SIOCSIFADDR:
	case SIOCGIFADDR:
		error = ether_ioctl(ifp, command, data);
		break;
	case SIOCSIFMTU:
		if (ifr->ifr_mtu > SK_JUMBO_MTU)
			error = EINVAL;
		else {
			ifp->if_mtu = ifr->ifr_mtu;
			sk_init(sc_if);
		}
		break;
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING &&
			    ifp->if_flags & IFF_PROMISC &&
			    !(sc_if->sk_if_flags & IFF_PROMISC)) {
				SK_XM_SETBIT_4(sc_if, XM_MODE,
				    XM_MODE_RX_PROMISC);
				sk_setmulti(sc_if);
			} else if (ifp->if_flags & IFF_RUNNING &&
			    !(ifp->if_flags & IFF_PROMISC) &&
			    sc_if->sk_if_flags & IFF_PROMISC) {
				SK_XM_CLRBIT_4(sc_if, XM_MODE,
				    XM_MODE_RX_PROMISC);
				sk_setmulti(sc_if);
			} else
				sk_init(sc_if);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				sk_stop(sc_if);
		}
		sc_if->sk_if_flags = ifp->if_flags;
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		sk_setmulti(sc_if);
		error = 0;
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc_if->ifmedia, command);
		break;
	default:
		error = EINVAL;
		break;
	}

	(void)splx(s);

	return(error);
}

/*
 * Probe for a SysKonnect GEnesis chip. Check the PCI vendor and device
 * IDs against our list and return a device name if we find a match.
 */
static int sk_probe(dev)
	device_t		dev;
{
	struct sk_type		*t;

	t = sk_devs;

	while(t->sk_name != NULL) {
		if ((pci_get_vendor(dev) == t->sk_vid) &&
		    (pci_get_device(dev) == t->sk_did)) {
			device_set_desc(dev, t->sk_name);
			return(0);
		}
		t++;
	}

	return(ENXIO);
}

/*
 * Force the GEnesis into reset, then bring it out of reset.
 */
static void sk_reset(sc)
	struct sk_softc		*sc;
{
	CSR_WRITE_4(sc, SK_CSR, SK_CSR_SW_RESET);
	CSR_WRITE_4(sc, SK_CSR, SK_CSR_MASTER_RESET);
	DELAY(1000);
	CSR_WRITE_4(sc, SK_CSR, SK_CSR_SW_UNRESET);
	CSR_WRITE_4(sc, SK_CSR, SK_CSR_MASTER_UNRESET);

	/* Configure packet arbiter */
	sk_win_write_2(sc, SK_PKTARB_CTL, SK_PKTARBCTL_UNRESET);
	sk_win_write_2(sc, SK_RXPA1_TINIT, SK_PKTARB_TIMEOUT);
	sk_win_write_2(sc, SK_TXPA1_TINIT, SK_PKTARB_TIMEOUT);
	sk_win_write_2(sc, SK_RXPA2_TINIT, SK_PKTARB_TIMEOUT);
	sk_win_write_2(sc, SK_TXPA2_TINIT, SK_PKTARB_TIMEOUT);

	/* Enable RAM interface */
	sk_win_write_4(sc, SK_RAMCTL, SK_RAMCTL_UNRESET);

	/*
         * Configure interrupt moderation. The moderation timer
	 * defers interrupts specified in the interrupt moderation
	 * timer mask based on the timeout specified in the interrupt
	 * moderation timer init register. Each bit in the timer
	 * register represents 18.825ns, so to specify a timeout in
	 * microseconds, we have to multiply by 54.
	 */
        sk_win_write_4(sc, SK_IMTIMERINIT, SK_IM_USECS(200));
        sk_win_write_4(sc, SK_IMMR, SK_ISR_TX1_S_EOF|SK_ISR_TX2_S_EOF|
	    SK_ISR_RX1_EOF|SK_ISR_RX2_EOF);
        sk_win_write_1(sc, SK_IMTIMERCTL, SK_IMCTL_START);

	return;
}

/*
 * Each XMAC chip is attached as a separate logical IP interface.
 * Single port cards will have only one logical interface of course.
 */
static int sk_attach_xmac(sc, port)
	struct sk_softc		*sc;
	int			port;
{
	struct sk_if_softc	*sc_if;
	struct ifnet		*ifp;
	int			i;
	char			ifname[64];

	if (sc == NULL)
		return(EINVAL);

	if (port != SK_PORT_A && port != SK_PORT_B)
		return(EINVAL);

	sc_if = malloc(sizeof(struct sk_if_softc), M_DEVBUF, M_NOWAIT);
	if (sc_if == NULL) {
		printf("skc%d: no memory for interface softc!\n", sc->sk_unit);
		return(ENOMEM);
	}
	bzero((char *)sc_if, sizeof(struct sk_if_softc));

	for (i = 0; i < SK_MAXUNIT; i++) {
		sprintf(ifname, "sk%d", i);
		if (ifunit(ifname) == NULL)
			break;
	}

	if (i == SK_MAXUNIT) {
		printf("skc%d: too many sk units\n", sc->sk_unit);
		free(sc_if, M_DEVBUF);
		return(ENODEV);
	}

	sc_if->sk_unit = i;
	sc_if->sk_port = port;
	sc_if->sk_softc = sc;
	sc->sk_if[port] = sc_if;
	if (port == SK_PORT_A)
		sc_if->sk_tx_bmu = SK_BMU_TXS_CSR0;
	if (port == SK_PORT_B)
		sc_if->sk_tx_bmu = SK_BMU_TXS_CSR1;
	
	/*
	 * Get station address for this interface. Note that
	 * dual port cards actually come with three station
	 * addresses: one for each port, plus an extra. The
	 * extra one is used by the SysKonnect driver software
	 * as a 'virtual' station address for when both ports
	 * are operating in failover mode. Currently we don't
	 * use this extra address.
	 */
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		sc_if->arpcom.ac_enaddr[i] =
		    sk_win_read_1(sc, SK_MAC0_0 + (port * 8) + i);

	printf("sk%d: <XaQti Corp. XMAC II> at skc%d port %d\n",
	    sc_if->sk_unit, sc->sk_unit, port);

	printf("sk%d: Ethernet address: %6D\n",
	    sc_if->sk_unit, sc_if->arpcom.ac_enaddr, ":");

	/*
	 * Set up RAM buffer addresses. The NIC will have a certain
	 * amount of SRAM on it, somewhere between 512K and 2MB. We
	 * need to divide this up a) between the transmitter and
 	 * receiver and b) between the two XMACs, if this is a
	 * dual port NIC. Our algotithm is to divide up the memory
	 * evenly so that everyone gets a fair share.
	 */
	if (sk_win_read_1(sc, SK_CONFIG) & SK_CONFIG_SINGLEMAC) {
		u_int32_t		chunk, val;

		chunk = sc->sk_ramsize / 2;
		val = sc->sk_rboff / sizeof(u_int64_t);
		sc_if->sk_rx_ramstart = val;
		val += (chunk / sizeof(u_int64_t));
		sc_if->sk_rx_ramend = val - 1;
		sc_if->sk_tx_ramstart = val;
		val += (chunk / sizeof(u_int64_t));
		sc_if->sk_tx_ramend = val - 1;
	} else {
		u_int32_t		chunk, val;

		chunk = sc->sk_ramsize / 4;
		val = (sc->sk_rboff + (chunk * 2 * sc_if->sk_port)) /
		    sizeof(u_int64_t);
		sc_if->sk_rx_ramstart = val;
		val += (chunk / sizeof(u_int64_t));
		sc_if->sk_rx_ramend = val - 1;
		sc_if->sk_tx_ramstart = val;
		val += (chunk / sizeof(u_int64_t));
		sc_if->sk_tx_ramend = val - 1;
	}

	/* Allocate the descriptor queues. */
	sc_if->sk_rdata = contigmalloc(sizeof(struct sk_ring_data), M_DEVBUF,
	    M_NOWAIT, 0, 0xffffffff, PAGE_SIZE, 0);

	if (sc_if->sk_rdata == NULL) {
		printf("sk%d: no memory for list buffers!\n", sc_if->sk_unit);
		free(sc_if, M_DEVBUF);
		sc->sk_if[port] = NULL;
		return(ENOMEM);
	}

	bzero(sc_if->sk_rdata, sizeof(struct sk_ring_data));

	/* Try to allocate memory for jumbo buffers. */
	if (sk_alloc_jumbo_mem(sc_if)) {
		printf("sk%d: jumbo buffer allocation failed\n",
		    sc_if->sk_unit);
		free(sc_if->sk_rdata, M_DEVBUF);
		free(sc_if, M_DEVBUF);
		sc->sk_if[port] = NULL;
		return(ENOMEM);
	}

	ifp = &sc_if->arpcom.ac_if;
	ifp->if_softc = sc_if;
	ifp->if_unit = sc_if->sk_unit; 
	ifp->if_name = "sk";
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = sk_ioctl;
	ifp->if_output = ether_output;
	ifp->if_start = sk_start;
	ifp->if_watchdog = sk_watchdog;
	ifp->if_init = sk_init;
	ifp->if_baudrate = 1000000000;
	ifp->if_snd.ifq_maxlen = SK_TX_RING_CNT - 1;

	/*
	 * Do ifmedia setup.
	 */
	ifmedia_init(&sc_if->ifmedia, 0, sk_ifmedia_upd, sk_ifmedia_sts);
	ifmedia_add(&sc_if->ifmedia, IFM_ETHER|sc->sk_pmd, 0, NULL);
	ifmedia_add(&sc_if->ifmedia, IFM_ETHER|sc->sk_pmd|IFM_FDX, 0, NULL);
	ifmedia_add(&sc_if->ifmedia, IFM_ETHER|sc->sk_pmd|IFM_HDX, 0, NULL);
	ifmedia_add(&sc_if->ifmedia, IFM_ETHER|IFM_AUTO, 0, NULL);
	ifmedia_set(&sc_if->ifmedia, IFM_ETHER|IFM_AUTO);

	/*
	 * Call MI attach routines.
	 */
	if_attach(ifp);
	ether_ifattach(ifp);

#if NBPF > 0
	bpfattach(ifp, DLT_EN10MB, sizeof(struct ether_header));
#endif

	return(0);
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
static int sk_attach(dev)
	device_t		dev;
{
	int			s;
	u_int32_t		command;
	struct sk_softc		*sc;
	int			unit, error = 0, rid;

	s = splimp();

	sc = device_get_softc(dev);
	unit = device_get_unit(dev);
	bzero(sc, sizeof(struct sk_softc));

	/*
	 * Handle power management nonsense.
	 */
	command = pci_read_config(dev, SK_PCI_CAPID, 4) & 0x000000FF;
	if (command == 0x01) {

		command = pci_read_config(dev, SK_PCI_PWRMGMTCTRL, 4);
		if (command & SK_PSTATE_MASK) {
			u_int32_t		iobase, membase, irq;

			/* Save important PCI config data. */
			iobase = pci_read_config(dev, SK_PCI_LOIO, 4);
			membase = pci_read_config(dev, SK_PCI_LOMEM, 4);
			irq = pci_read_config(dev, SK_PCI_INTLINE, 4);

			/* Reset the power state. */
			printf("skc%d: chip is in D%d power mode "
			"-- setting to D0\n", unit, command & SK_PSTATE_MASK);
			command &= 0xFFFFFFFC;
			pci_write_config(dev, SK_PCI_PWRMGMTCTRL, command, 4);

			/* Restore PCI config data. */
			pci_write_config(dev, SK_PCI_LOIO, iobase, 4);
			pci_write_config(dev, SK_PCI_LOMEM, membase, 4);
			pci_write_config(dev, SK_PCI_INTLINE, irq, 4);
		}
	}

	/*
	 * Map control/status registers.
	 */
	command = pci_read_config(dev, PCI_COMMAND_STATUS_REG, 4);
	command |= (PCIM_CMD_PORTEN|PCIM_CMD_MEMEN|PCIM_CMD_BUSMASTEREN);
	pci_write_config(dev, PCI_COMMAND_STATUS_REG, command, 4);
	command = pci_read_config(dev, PCI_COMMAND_STATUS_REG, 4);

#ifdef SK_USEIOSPACE
	if (!(command & PCIM_CMD_PORTEN)) {
		printf("skc%d: failed to enable I/O ports!\n", unit);
		error = ENXIO;
		goto fail;
	}
#else
	if (!(command & PCIM_CMD_MEMEN)) {
		printf("skc%d: failed to enable memory mapping!\n", unit);
		error = ENXIO;
		goto fail;
	}
#endif

	rid = SK_RID;
	sc->sk_res = bus_alloc_resource(dev, SK_RES, &rid,
	    0, ~0, 1, RF_ACTIVE);

	if (sc->sk_res == NULL) {
		printf("sk%d: couldn't map ports/memory\n", unit);
		error = ENXIO;
		goto fail;
	}

	sc->sk_btag = rman_get_bustag(sc->sk_res);
	sc->sk_bhandle = rman_get_bushandle(sc->sk_res);

	/* Allocate interrupt */
	rid = 0;
	sc->sk_irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0, 1,
	    RF_SHAREABLE | RF_ACTIVE);

	if (sc->sk_irq == NULL) {
		printf("skc%d: couldn't map interrupt\n", unit);
		bus_release_resource(dev, SK_RES, SK_RID, sc->sk_res);
		error = ENXIO;
		goto fail;
	}

	error = bus_setup_intr(dev, sc->sk_irq, INTR_TYPE_NET,
	    sk_intr, sc, &sc->sk_intrhand);

	if (error) {
		printf("skc%d: couldn't set up irq\n", unit);
		bus_release_resource(dev, SK_RES, SK_RID, sc->sk_res);
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->sk_res);
		goto fail;
	}

	/* Reset the adapter. */
	sk_reset(sc);

	sc->sk_unit = unit;

	/* Read and save vital product data from EEPROM. */
	sk_vpd_read(sc);

	/* Read and save RAM size and RAMbuffer offset */
	switch(sk_win_read_1(sc, SK_EPROM0)) {
	case SK_RAMSIZE_512K_64:
		sc->sk_ramsize = 0x80000;
		sc->sk_rboff = SK_RBOFF_0;
		break;
	case SK_RAMSIZE_1024K_64:
		sc->sk_ramsize = 0x100000;
		sc->sk_rboff = SK_RBOFF_80000;
		break;
	case SK_RAMSIZE_1024K_128:
		sc->sk_ramsize = 0x100000;
		sc->sk_rboff = SK_RBOFF_0;
		break;
	case SK_RAMSIZE_2048K_128:
		sc->sk_ramsize = 0x200000;
		sc->sk_rboff = SK_RBOFF_0;
		break;
	default:
		printf("skc%d: unknown ram size: %d\n",
		    sc->sk_unit, sk_win_read_1(sc, SK_EPROM0));
		bus_teardown_intr(dev, sc->sk_irq, sc->sk_intrhand);
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->sk_irq);
		bus_release_resource(dev, SK_RES, SK_RID, sc->sk_res);
		error = ENXIO;
		goto fail;
		break;
	}

	/* Read and save physical media type */
	switch(sk_win_read_1(sc, SK_PMDTYPE)) {
	case SK_PMD_1000BASESX:
		sc->sk_pmd = IFM_1000_SX;
		break;
	case SK_PMD_1000BASELX:
		sc->sk_pmd = IFM_1000_LX;
		break;
	case SK_PMD_1000BASECX:
		sc->sk_pmd = IFM_1000_CX;
		break;
	case SK_PMD_1000BASETX:
		sc->sk_pmd = IFM_1000_TX;
		break;
	default:
		printf("skc%d: unknown media type: 0x%x\n",
		    sc->sk_unit, sk_win_read_1(sc, SK_PMDTYPE));
		bus_teardown_intr(dev, sc->sk_irq, sc->sk_intrhand);
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->sk_irq);
		bus_release_resource(dev, SK_RES, SK_RID, sc->sk_res);
		error = ENXIO;
		goto fail;
	}

	/* Announce the product name. */
	printf("skc%d: %s\n", sc->sk_unit, sc->sk_vpd_prodname);

	sk_attach_xmac(sc, SK_PORT_A);
	if (!(sk_win_read_1(sc, SK_CONFIG) & SK_CONFIG_SINGLEMAC))
		sk_attach_xmac(sc, SK_PORT_B);

	/* Turn on the 'driver is loaded' LED. */
	CSR_WRITE_2(sc, SK_LED, SK_LED_GREEN_ON);

fail:
	splx(s);
	return(error);
}

static int sk_detach(dev)
	device_t		dev;
{
	struct sk_softc		*sc;
	struct sk_if_softc	*sc_if0 = NULL, *sc_if1 = NULL;
	struct ifnet		*ifp0 = NULL, *ifp1 = NULL;
	int			s;

	s = splimp();

	sc = device_get_softc(dev);
	sc_if0 = sc->sk_if[SK_PORT_A];
	ifp0 = &sc_if0->arpcom.ac_if;
	sk_stop(sc_if0);
	if_detach(ifp0);
	contigfree(sc_if0->sk_cdata.sk_jumbo_buf, SK_JMEM, M_DEVBUF);
	ifmedia_removeall(&sc_if0->ifmedia);
	free(sc->sk_if[SK_PORT_A], M_DEVBUF);
	if (sc->sk_if[SK_PORT_B] != NULL) {
		sc_if1 = sc->sk_if[SK_PORT_B];
		ifp1 = &sc_if1->arpcom.ac_if;
		sk_stop(sc_if1);
		if_detach(ifp1);
		contigfree(sc_if1->sk_cdata.sk_jumbo_buf, SK_JMEM, M_DEVBUF);
		ifmedia_removeall(&sc_if1->ifmedia);
		free(sc->sk_if[SK_PORT_B], M_DEVBUF);
	}

	bus_teardown_intr(dev, sc->sk_irq, sc->sk_intrhand);
	bus_release_resource(dev, SYS_RES_IRQ, 0, sc->sk_irq);
	bus_release_resource(dev, SK_RES, SK_RID, sc->sk_res);

	splx(s);

	return(0);
}

static int sk_encap(sc_if, m_head, txidx)
        struct sk_if_softc	*sc_if;
        struct mbuf		*m_head;
        u_int32_t		*txidx;
{
	struct sk_tx_desc	*f = NULL;
	struct mbuf		*m;
	u_int32_t		frag, cur, cnt = 0;

	m = m_head;
	cur = frag = *txidx;

	/*
	 * Start packing the mbufs in this chain into
	 * the fragment pointers. Stop when we run out
	 * of fragments or hit the end of the mbuf chain.
	 */
	for (m = m_head; m != NULL; m = m->m_next) {
		if (m->m_len != 0) {
			if ((SK_TX_RING_CNT -
			    (sc_if->sk_cdata.sk_tx_cnt + cnt)) < 2)
				return(ENOBUFS);
			f = &sc_if->sk_rdata->sk_tx_ring[frag];
			f->sk_data_lo = vtophys(mtod(m, vm_offset_t));
			f->sk_ctl = m->m_len | SK_OPCODE_DEFAULT;
			if (cnt == 0)
				f->sk_ctl |= SK_TXCTL_FIRSTFRAG;
			else
				f->sk_ctl |= SK_TXCTL_OWN;
			cur = frag;
			SK_INC(frag, SK_TX_RING_CNT);
			cnt++;
		}
	}

	if (m != NULL)
		return(ENOBUFS);

	sc_if->sk_rdata->sk_tx_ring[cur].sk_ctl |=
		SK_TXCTL_LASTFRAG|SK_TXCTL_EOF_INTR;
	sc_if->sk_cdata.sk_tx_chain[cur].sk_mbuf = m_head;
	sc_if->sk_rdata->sk_tx_ring[*txidx].sk_ctl |= SK_TXCTL_OWN;
	sc_if->sk_cdata.sk_tx_cnt += cnt;

	*txidx = frag;

	return(0);
}

static void sk_start(ifp)
	struct ifnet		*ifp;
{
        struct sk_softc		*sc;
        struct sk_if_softc	*sc_if;
        struct mbuf		*m_head = NULL;
        u_int32_t		idx;

	sc_if = ifp->if_softc;
	sc = sc_if->sk_softc;

	idx = sc_if->sk_cdata.sk_tx_prod;

	while(sc_if->sk_cdata.sk_tx_chain[idx].sk_mbuf == NULL) {
		IF_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		/*
		 * Pack the data into the transmit ring. If we
		 * don't have room, set the OACTIVE flag and wait
		 * for the NIC to drain the ring.
		 */
		if (sk_encap(sc_if, m_head, &idx)) {
			IF_PREPEND(&ifp->if_snd, m_head);
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
#if NBPF > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp, m_head);
#endif
	}

	/* Transmit */
	sc_if->sk_cdata.sk_tx_prod = idx;
	CSR_WRITE_4(sc, sc_if->sk_tx_bmu, SK_TXBMU_TX_START);

	/* Set a timeout in case the chip goes out to lunch. */
	ifp->if_timer = 5;

	return;
}


static void sk_watchdog(ifp)
	struct ifnet		*ifp;
{
	struct sk_if_softc	*sc_if;

	sc_if = ifp->if_softc;

	printf("sk%d: watchdog timeout\n", sc_if->sk_unit);
	sk_init(sc_if);

	return;
}

static void sk_shutdown(dev)
	device_t		dev;
{
	struct sk_softc		*sc;

	sc = device_get_softc(dev);

	/* Turn off the 'driver is loaded' LED. */
	CSR_WRITE_2(sc, SK_LED, SK_LED_GREEN_OFF);

	/*
	 * Reset the GEnesis controller. Doing this should also
	 * assert the resets on the attached XMAC(s).
	 */
	sk_reset(sc);

	return;
}

static void sk_rxeof(sc_if)
	struct sk_if_softc	*sc_if;
{
	struct ether_header	*eh;
	struct mbuf		*m;
	struct ifnet		*ifp;
	struct sk_chain		*cur_rx;
	int			total_len = 0;
	int			i;
	u_int32_t		rxstat;

	ifp = &sc_if->arpcom.ac_if;
	i = sc_if->sk_cdata.sk_rx_prod;
	cur_rx = &sc_if->sk_cdata.sk_rx_chain[i];

	while(!(sc_if->sk_rdata->sk_rx_ring[i].sk_ctl & SK_RXCTL_OWN)) {

		cur_rx = &sc_if->sk_cdata.sk_rx_chain[i];
		rxstat = sc_if->sk_rdata->sk_rx_ring[i].sk_xmac_rxstat;
		m = cur_rx->sk_mbuf;
		cur_rx->sk_mbuf = NULL;
		total_len = SK_RXBYTES(sc_if->sk_rdata->sk_rx_ring[i].sk_ctl);
		SK_INC(i, SK_RX_RING_CNT);

		if (rxstat & XM_RXSTAT_ERRFRAME) {
			ifp->if_ierrors++;
			sk_newbuf(sc_if, cur_rx, m);
			continue;
		}

		/*
		 * Try to allocate a new jumbo buffer. If that
		 * fails, copy the packet to mbufs and put the
		 * jumbo buffer back in the ring so it can be
		 * re-used. If allocating mbufs fails, then we
		 * have to drop the packet.
		 */
		if (sk_newbuf(sc_if, cur_rx, NULL) == ENOBUFS) {
			struct mbuf		*m0;
			m0 = m_devget(mtod(m, char *) - ETHER_ALIGN,
			    total_len + ETHER_ALIGN, 0, ifp, NULL);
			sk_newbuf(sc_if, cur_rx, m);
			if (m0 == NULL) {
				printf("sk%d: no receive buffers "
				    "available -- packet dropped!\n",
				    sc_if->sk_unit);
				ifp->if_ierrors++;
				continue;
			}
			m_adj(m0, ETHER_ALIGN);
			m = m0;
		} else {
			m->m_pkthdr.rcvif = ifp;
			m->m_pkthdr.len = m->m_len = total_len;
		}

		ifp->if_ipackets++;
		eh = mtod(m, struct ether_header *);

#if NBPF > 0
		if (ifp->if_bpf) {
			bpf_mtap(ifp, m);
			if (ifp->if_flags & IFF_PROMISC &&
			    (bcmp(eh->ether_dhost, sc_if->arpcom.ac_enaddr,
			    ETHER_ADDR_LEN) && !(eh->ether_dhost[0] & 1))) {
				m_freem(m);
				continue;
			}
		}
#endif
		/* Remove header from mbuf and pass it on. */
		m_adj(m, sizeof(struct ether_header));
		ether_input(ifp, eh, m);
	}

	sc_if->sk_cdata.sk_rx_prod = i;

	return;
}

static void sk_txeof(sc_if)
	struct sk_if_softc	*sc_if;
{
	struct sk_tx_desc	*cur_tx = NULL;
	struct ifnet		*ifp;
	u_int32_t		idx;

	ifp = &sc_if->arpcom.ac_if;

	/*
	 * Go through our tx ring and free mbufs for those
	 * frames that have been sent.
	 */
	idx = sc_if->sk_cdata.sk_tx_cons;
	while(idx != sc_if->sk_cdata.sk_tx_prod) {
		cur_tx = &sc_if->sk_rdata->sk_tx_ring[idx];
		if (cur_tx->sk_ctl & SK_TXCTL_OWN)
			break;
		if (cur_tx->sk_ctl & SK_TXCTL_LASTFRAG)
			ifp->if_opackets++;
		if (sc_if->sk_cdata.sk_tx_chain[idx].sk_mbuf != NULL) {
			m_freem(sc_if->sk_cdata.sk_tx_chain[idx].sk_mbuf);
			sc_if->sk_cdata.sk_tx_chain[idx].sk_mbuf = NULL;
		}
		sc_if->sk_cdata.sk_tx_cnt--;
		SK_INC(idx, SK_TX_RING_CNT);
		ifp->if_timer = 0;
	}

	sc_if->sk_cdata.sk_tx_cons = idx;

	if (cur_tx != NULL)
		ifp->if_flags &= ~IFF_OACTIVE;

	return;
}

static void sk_intr_xmac(sc_if)
	struct sk_if_softc	*sc_if;
{
	struct sk_softc		*sc;
	u_int16_t		status;
	u_int16_t		bmsr;

	sc = sc_if->sk_softc;
	status = SK_XM_READ_2(sc_if, XM_ISR);

	if (status & XM_ISR_LINKEVENT) {
		SK_XM_SETBIT_2(sc_if, XM_IMR, XM_IMR_LINKEVENT);
		if (sc_if->sk_link == 1) {
			printf("sk%d: gigabit link down\n", sc_if->sk_unit);
			sc_if->sk_link = 0;
		}
	}

	if (status & XM_ISR_AUTONEG_DONE) {
		bmsr = sk_phy_readreg(sc_if, XM_PHY_BMSR);
		if (bmsr & XM_BMSR_LINKSTAT) {
			sc_if->sk_link = 1;
			SK_XM_CLRBIT_2(sc_if, XM_IMR, XM_IMR_LINKEVENT);
			printf("sk%d: gigabit link up\n", sc_if->sk_unit);
		}
	}

	if (status & XM_IMR_TX_UNDERRUN)
		SK_XM_SETBIT_4(sc_if, XM_MODE, XM_MODE_FLUSH_TXFIFO);

	if (status & XM_IMR_RX_OVERRUN)
		SK_XM_SETBIT_4(sc_if, XM_MODE, XM_MODE_FLUSH_RXFIFO);

	return;
}

static void sk_intr(xsc)
	void			*xsc;
{
	struct sk_softc		*sc = xsc;
	struct sk_if_softc	*sc_if0 = NULL, *sc_if1 = NULL;
	struct ifnet		*ifp0 = NULL, *ifp1 = NULL;
	u_int32_t		status;

	sc_if0 = sc->sk_if[SK_PORT_A];
	sc_if1 = sc->sk_if[SK_PORT_B];

	if (sc_if0 != NULL)
		ifp0 = &sc_if0->arpcom.ac_if;
	if (sc_if1 != NULL)
		ifp1 = &sc_if0->arpcom.ac_if;

	for (;;) {
		status = CSR_READ_4(sc, SK_ISSR);
		if (!(status & sc->sk_intrmask))
			break;

		/* Handle receive interrupts first. */
		if (status & SK_ISR_RX1_EOF) {
			sk_rxeof(sc_if0);
			CSR_WRITE_4(sc, SK_BMU_RX_CSR0,
			    SK_RXBMU_CLR_IRQ_EOF|SK_RXBMU_RX_START);
		}
		if (status & SK_ISR_RX2_EOF) {
			sk_rxeof(sc_if1);
			CSR_WRITE_4(sc, SK_BMU_RX_CSR1,
			    SK_RXBMU_CLR_IRQ_EOF|SK_RXBMU_RX_START);
		}

		/* Then transmit interrupts. */
		if (status & SK_ISR_TX1_S_EOF) {
			sk_txeof(sc_if0);
			CSR_WRITE_4(sc, SK_BMU_TXS_CSR0,
			    SK_TXBMU_CLR_IRQ_EOF);
		}
		if (status & SK_ISR_TX2_S_EOF) {
			sk_txeof(sc_if1);
			CSR_WRITE_4(sc, SK_BMU_TXS_CSR1,
			    SK_TXBMU_CLR_IRQ_EOF);
		}

		/* Then MAC interrupts. */
		if (status & SK_ISR_MAC1)
			sk_intr_xmac(sc_if0);

		if (status & SK_ISR_MAC2)
			sk_intr_xmac(sc_if1);
	}

	CSR_WRITE_4(sc, SK_IMR, sc->sk_intrmask);

	return;
}

static void sk_init_xmac(sc_if)
	struct sk_if_softc	*sc_if;
{
	struct sk_softc		*sc;
	struct ifnet		*ifp;

	sc = sc_if->sk_softc;
	ifp = &sc_if->arpcom.ac_if;

	/* Unreset the XMAC. */
	SK_IF_WRITE_2(sc_if, 0, SK_TXF1_MACCTL, SK_TXMACCTL_XMAC_UNRESET);
	DELAY(1000);

	/* Save the XMAC II revision */
	sc_if->sk_xmac_rev = XM_XMAC_REV(SK_XM_READ_4(sc_if, XM_DEVID));

	/* Set station address */
	SK_XM_WRITE_2(sc_if, XM_PAR0,
	    *(u_int16_t *)(&sc_if->arpcom.ac_enaddr[0]));
	SK_XM_WRITE_2(sc_if, XM_PAR1,
	    *(u_int16_t *)(&sc_if->arpcom.ac_enaddr[2]));
	SK_XM_WRITE_2(sc_if, XM_PAR2,
	    *(u_int16_t *)(&sc_if->arpcom.ac_enaddr[4]));
	SK_XM_SETBIT_4(sc_if, XM_MODE, XM_MODE_RX_USE_STATION);

	if (ifp->if_flags & IFF_PROMISC) {
		SK_XM_SETBIT_4(sc_if, XM_MODE, XM_MODE_RX_PROMISC);
	} else {
		SK_XM_CLRBIT_4(sc_if, XM_MODE, XM_MODE_RX_PROMISC);
	}

	if (ifp->if_flags & IFF_BROADCAST) {
		SK_XM_CLRBIT_4(sc_if, XM_MODE, XM_MODE_RX_NOBROAD);
	} else {
		SK_XM_SETBIT_4(sc_if, XM_MODE, XM_MODE_RX_NOBROAD);
	}

	/* We don't need the FCS appended to the packet. */
	SK_XM_SETBIT_2(sc_if, XM_RXCMD, XM_RXCMD_STRIPFCS);

	/* We want short frames padded to 60 bytes. */
	SK_XM_SETBIT_2(sc_if, XM_TXCMD, XM_TXCMD_AUTOPAD);

	/*
	 * Enable the reception of all error frames. This is is
	 * a necessary evil due to the design of the XMAC. The
	 * XMAC's receive FIFO is only 8K in size, however jumbo
	 * frames can be up to 9000 bytes in length. When bad
	 * frame filtering is enabled, the XMAC's RX FIFO operates
	 * in 'store and forward' mode. For this to work, the
	 * entire frame has to fit into the FIFO, but that means
	 * that jumbo frames larger than 8192 bytes will be
	 * truncated. Disabling all bad frame filtering causes
	 * the RX FIFO to operate in streaming mode, in which
	 * case the XMAC will start transfering frames out of the
	 * RX FIFO as soon as the FIFO threshold is reached.
	 */
	SK_XM_SETBIT_4(sc_if, XM_MODE, XM_MODE_RX_BADFRAMES|
	    XM_MODE_RX_GIANTS|XM_MODE_RX_RUNTS|XM_MODE_RX_CRCERRS|
	    XM_MODE_RX_INRANGELEN);

	if (ifp->if_mtu > (ETHERMTU + ETHER_HDR_LEN + ETHER_CRC_LEN))
		SK_XM_SETBIT_2(sc_if, XM_RXCMD, XM_RXCMD_BIGPKTOK);
	else
		SK_XM_CLRBIT_2(sc_if, XM_RXCMD, XM_RXCMD_BIGPKTOK);

	/*
	 * Bump up the transmit threshold. This helps hold off transmit
	 * underruns when we're blasting traffic from both ports at once.
	 */
	SK_XM_WRITE_2(sc_if, XM_TX_REQTHRESH, SK_XM_TX_FIFOTHRESH);

	/* Set multicast filter */
	sk_setmulti(sc_if);

	/* Clear and enable interrupts */
	SK_XM_READ_2(sc_if, XM_ISR);
	SK_XM_WRITE_2(sc_if, XM_IMR, XM_INTRS);

	sc_if->sk_link = 0;

	/* Configure MAC arbiter */
	switch(sc_if->sk_xmac_rev) {
	case XM_XMAC_REV_B2:
		sk_win_write_1(sc, SK_RCINIT_RX1, SK_RCINIT_XMAC_B2);
		sk_win_write_1(sc, SK_RCINIT_TX1, SK_RCINIT_XMAC_B2);
		sk_win_write_1(sc, SK_RCINIT_RX2, SK_RCINIT_XMAC_B2);
		sk_win_write_1(sc, SK_RCINIT_TX2, SK_RCINIT_XMAC_B2);
		sk_win_write_1(sc, SK_MINIT_RX1, SK_MINIT_XMAC_B2);
		sk_win_write_1(sc, SK_MINIT_TX1, SK_MINIT_XMAC_B2);
		sk_win_write_1(sc, SK_MINIT_RX2, SK_MINIT_XMAC_B2);
		sk_win_write_1(sc, SK_MINIT_TX2, SK_MINIT_XMAC_B2);
		sk_win_write_1(sc, SK_RECOVERY_CTL, SK_RECOVERY_XMAC_B2);
		break;
	case XM_XMAC_REV_C1:
		sk_win_write_1(sc, SK_RCINIT_RX1, SK_RCINIT_XMAC_C1);
		sk_win_write_1(sc, SK_RCINIT_TX1, SK_RCINIT_XMAC_C1);
		sk_win_write_1(sc, SK_RCINIT_RX2, SK_RCINIT_XMAC_C1);
		sk_win_write_1(sc, SK_RCINIT_TX2, SK_RCINIT_XMAC_C1);
		sk_win_write_1(sc, SK_MINIT_RX1, SK_MINIT_XMAC_C1);
		sk_win_write_1(sc, SK_MINIT_TX1, SK_MINIT_XMAC_C1);
		sk_win_write_1(sc, SK_MINIT_RX2, SK_MINIT_XMAC_C1);
		sk_win_write_1(sc, SK_MINIT_TX2, SK_MINIT_XMAC_C1);
		sk_win_write_1(sc, SK_RECOVERY_CTL, SK_RECOVERY_XMAC_B2);
		break;
	default:
		break;
	}
	sk_win_write_2(sc, SK_MACARB_CTL,
	    SK_MACARBCTL_UNRESET|SK_MACARBCTL_FASTOE_OFF);

	return;
}

/*
 * Note that to properly initialize any part of the GEnesis chip,
 * you first have to take it out of reset mode.
 */
static void sk_init(xsc)
	void			*xsc;
{
	struct sk_if_softc	*sc_if = xsc;
	struct sk_softc		*sc;
	struct ifnet		*ifp;
	int			s;

	s = splimp();

	ifp = &sc_if->arpcom.ac_if;
	sc = sc_if->sk_softc;

	/* Cancel pending I/O and free all RX/TX buffers. */
	sk_stop(sc_if);

	/* Configure LINK_SYNC LED */
	SK_IF_WRITE_1(sc_if, 0, SK_LINKLED1_CTL, SK_LINKLED_ON);
	SK_IF_WRITE_1(sc_if, 0, SK_LINKLED1_CTL, SK_LINKLED_LINKSYNC_ON);

	/* Configure RX LED */
	SK_IF_WRITE_1(sc_if, 0, SK_RXLED1_CTL, SK_RXLEDCTL_COUNTER_START);

	/* Configure TX LED */
	SK_IF_WRITE_1(sc_if, 0, SK_TXLED1_CTL, SK_TXLEDCTL_COUNTER_START);

	/* Configure I2C registers */

	/* Configure XMAC(s) */
	sk_init_xmac(sc_if);

	/* Configure MAC FIFOs */
	SK_IF_WRITE_4(sc_if, 0, SK_RXF1_CTL, SK_FIFO_UNRESET);
	SK_IF_WRITE_4(sc_if, 0, SK_RXF1_END, SK_FIFO_END);
	SK_IF_WRITE_4(sc_if, 0, SK_RXF1_CTL, SK_FIFO_ON);

	SK_IF_WRITE_4(sc_if, 0, SK_TXF1_CTL, SK_FIFO_UNRESET);
	SK_IF_WRITE_4(sc_if, 0, SK_TXF1_END, SK_FIFO_END);
	SK_IF_WRITE_4(sc_if, 0, SK_TXF1_CTL, SK_FIFO_ON);

	/* Configure transmit arbiter(s) */
	SK_IF_WRITE_1(sc_if, 0, SK_TXAR1_COUNTERCTL,
	    SK_TXARCTL_ON|SK_TXARCTL_FSYNC_ON);

	/* Configure RAMbuffers */
	SK_IF_WRITE_4(sc_if, 0, SK_RXRB1_CTLTST, SK_RBCTL_UNRESET);
	SK_IF_WRITE_4(sc_if, 0, SK_RXRB1_START, sc_if->sk_rx_ramstart);
	SK_IF_WRITE_4(sc_if, 0, SK_RXRB1_WR_PTR, sc_if->sk_rx_ramstart);
	SK_IF_WRITE_4(sc_if, 0, SK_RXRB1_RD_PTR, sc_if->sk_rx_ramstart);
	SK_IF_WRITE_4(sc_if, 0, SK_RXRB1_END, sc_if->sk_rx_ramend);
	SK_IF_WRITE_4(sc_if, 0, SK_RXRB1_CTLTST, SK_RBCTL_ON);

	SK_IF_WRITE_4(sc_if, 1, SK_TXRBS1_CTLTST, SK_RBCTL_UNRESET);
	SK_IF_WRITE_4(sc_if, 1, SK_TXRBS1_CTLTST, SK_RBCTL_STORENFWD_ON);
	SK_IF_WRITE_4(sc_if, 1, SK_TXRBS1_START, sc_if->sk_tx_ramstart);
	SK_IF_WRITE_4(sc_if, 1, SK_TXRBS1_WR_PTR, sc_if->sk_tx_ramstart);
	SK_IF_WRITE_4(sc_if, 1, SK_TXRBS1_RD_PTR, sc_if->sk_tx_ramstart);
	SK_IF_WRITE_4(sc_if, 1, SK_TXRBS1_END, sc_if->sk_tx_ramend);
	SK_IF_WRITE_4(sc_if, 1, SK_TXRBS1_CTLTST, SK_RBCTL_ON);

	/* Configure BMUs */
	SK_IF_WRITE_4(sc_if, 0, SK_RXQ1_BMU_CSR, SK_RXBMU_ONLINE);
	SK_IF_WRITE_4(sc_if, 0, SK_RXQ1_CURADDR_LO,
	    vtophys(&sc_if->sk_rdata->sk_rx_ring[0]));
	SK_IF_WRITE_4(sc_if, 0, SK_RXQ1_CURADDR_HI, 0);

	SK_IF_WRITE_4(sc_if, 1, SK_TXQS1_BMU_CSR, SK_TXBMU_ONLINE);
	SK_IF_WRITE_4(sc_if, 1, SK_TXQS1_CURADDR_LO,
	    vtophys(&sc_if->sk_rdata->sk_tx_ring[0]));
	SK_IF_WRITE_4(sc_if, 1, SK_TXQS1_CURADDR_HI, 0);

	/* Init descriptors */
	if (sk_init_rx_ring(sc_if) == ENOBUFS) {
		printf("sk%d: initialization failed: no "
		    "memory for rx buffers\n", sc_if->sk_unit);
		sk_stop(sc_if);
		(void)splx(s);
		return;
	}
	sk_init_tx_ring(sc_if);

	/* Configure interrupt handling */
	CSR_READ_4(sc, SK_ISSR);
	if (sc_if->sk_port == SK_PORT_A)
		sc->sk_intrmask |= SK_INTRS1;
	else
		sc->sk_intrmask |= SK_INTRS2;
	CSR_WRITE_4(sc, SK_IMR, sc->sk_intrmask);

	/* Start BMUs. */
	SK_IF_WRITE_4(sc_if, 0, SK_RXQ1_BMU_CSR, SK_RXBMU_RX_START);

	/* Enable XMACs TX and RX state machines */
	SK_XM_SETBIT_2(sc_if, XM_MMUCMD, XM_MMUCMD_TX_ENB|XM_MMUCMD_RX_ENB);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	splx(s);

	return;
}

static void sk_stop(sc_if)
	struct sk_if_softc	*sc_if;
{
	int			i;
	struct sk_softc		*sc;
	struct ifnet		*ifp;

	sc = sc_if->sk_softc;
	ifp = &sc_if->arpcom.ac_if;

	/* Turn off various components of this interface. */
	SK_IF_WRITE_2(sc_if, 0, SK_TXF1_MACCTL, SK_TXMACCTL_XMAC_RESET);
	SK_IF_WRITE_4(sc_if, 0, SK_RXF1_CTL, SK_FIFO_RESET);
	SK_IF_WRITE_4(sc_if, 0, SK_RXQ1_BMU_CSR, SK_RXBMU_OFFLINE);
	SK_IF_WRITE_4(sc_if, 0, SK_RXRB1_CTLTST, SK_RBCTL_RESET|SK_RBCTL_OFF);
	SK_IF_WRITE_4(sc_if, 1, SK_TXQS1_BMU_CSR, SK_TXBMU_OFFLINE);
	SK_IF_WRITE_4(sc_if, 1, SK_TXRBS1_CTLTST, SK_RBCTL_RESET|SK_RBCTL_OFF);
	SK_IF_WRITE_1(sc_if, 0, SK_TXAR1_COUNTERCTL, SK_TXARCTL_OFF);
	SK_IF_WRITE_1(sc_if, 0, SK_RXLED1_CTL, SK_RXLEDCTL_COUNTER_STOP);
	SK_IF_WRITE_1(sc_if, 0, SK_TXLED1_CTL, SK_RXLEDCTL_COUNTER_STOP);
	SK_IF_WRITE_1(sc_if, 0, SK_LINKLED1_CTL, SK_LINKLED_OFF);
	SK_IF_WRITE_1(sc_if, 0, SK_LINKLED1_CTL, SK_LINKLED_LINKSYNC_OFF);

	/* Disable interrupts */
	if (sc_if->sk_port == SK_PORT_A)
		sc->sk_intrmask &= ~SK_INTRS1;
	else
		sc->sk_intrmask &= ~SK_INTRS2;
	CSR_WRITE_4(sc, SK_IMR, sc->sk_intrmask);

	/* Free RX and TX mbufs still in the queues. */
	for (i = 0; i < SK_RX_RING_CNT; i++) {
		if (sc_if->sk_cdata.sk_rx_chain[i].sk_mbuf != NULL) {
			m_freem(sc_if->sk_cdata.sk_rx_chain[i].sk_mbuf);
			sc_if->sk_cdata.sk_rx_chain[i].sk_mbuf = NULL;
		}
	}

	for (i = 0; i < SK_TX_RING_CNT; i++) {
		if (sc_if->sk_cdata.sk_tx_chain[i].sk_mbuf != NULL) {
			m_freem(sc_if->sk_cdata.sk_tx_chain[i].sk_mbuf);
			sc_if->sk_cdata.sk_tx_chain[i].sk_mbuf = NULL;
		}
	}

	ifp->if_flags &= ~(IFF_RUNNING|IFF_OACTIVE);

	return;
}
