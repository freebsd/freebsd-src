/*-
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_vlan_var.h>

#include <net/bpf.h>

#include <vm/vm.h>              /* for vtophys */
#include <vm/pmap.h>            /* for vtophys */
#include <machine/bus_memio.h>
#include <machine/bus_pio.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

/* "controller miibus0" required.  See GENERIC if you get errors here. */
#include "miibus_if.h"

#define STE_USEIOSPACE

#include <pci/if_stereg.h>

MODULE_DEPEND(ste, pci, 1, 1, 1);
MODULE_DEPEND(ste, ether, 1, 1, 1);
MODULE_DEPEND(ste, miibus, 1, 1, 1);

/*
 * Various supported device vendors/types and their names.
 */
static struct ste_type ste_devs[] = {
	{ ST_VENDORID, ST_DEVICEID_ST201, "Sundance ST201 10/100BaseTX" },
	{ DL_VENDORID, DL_DEVICEID_DL10050, "D-Link DL10050 10/100BaseTX" },
	{ 0, 0, NULL }
};

static int ste_probe(device_t);
static int ste_attach(device_t);
static int ste_detach(device_t);
static void ste_init(void *);
static void ste_intr(void *);
static void ste_rxeoc(struct ste_softc *);
static void ste_rxeof(struct ste_softc *);
static void ste_txeoc(struct ste_softc *);
static void ste_txeof(struct ste_softc *);
static void ste_stats_update(void *);
static void ste_stop(struct ste_softc *);
static void ste_reset(struct ste_softc *);
static int ste_ioctl(struct ifnet *, u_long, caddr_t);
static int ste_encap(struct ste_softc *, struct ste_chain *, struct mbuf *);
static void ste_start(struct ifnet *);
static void ste_watchdog(struct ifnet *);
static void ste_shutdown(device_t);
static int ste_newbuf(struct ste_softc *, struct ste_chain_onefrag *,
		struct mbuf *);
static int ste_ifmedia_upd(struct ifnet *);
static void ste_ifmedia_sts(struct ifnet *, struct ifmediareq *);

static void ste_mii_sync(struct ste_softc *);
static void ste_mii_send(struct ste_softc *, u_int32_t, int);
static int ste_mii_readreg(struct ste_softc *, struct ste_mii_frame *);
static int ste_mii_writereg(struct ste_softc *, struct ste_mii_frame *);
static int ste_miibus_readreg(device_t, int, int);
static int ste_miibus_writereg(device_t, int, int, int);
static void ste_miibus_statchg(device_t);

static int ste_eeprom_wait(struct ste_softc *);
static int ste_read_eeprom(struct ste_softc *, caddr_t, int, int, int);
static void ste_wait(struct ste_softc *);
static void ste_setmulti(struct ste_softc *);
static int ste_init_rx_list(struct ste_softc *);
static void ste_init_tx_list(struct ste_softc *);

#ifdef STE_USEIOSPACE
#define STE_RES			SYS_RES_IOPORT
#define STE_RID			STE_PCI_LOIO
#else
#define STE_RES			SYS_RES_MEMORY
#define STE_RID			STE_PCI_LOMEM
#endif

static device_method_t ste_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ste_probe),
	DEVMETHOD(device_attach,	ste_attach),
	DEVMETHOD(device_detach,	ste_detach),
	DEVMETHOD(device_shutdown,	ste_shutdown),

	/* bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	ste_miibus_readreg),
	DEVMETHOD(miibus_writereg,	ste_miibus_writereg),
	DEVMETHOD(miibus_statchg,	ste_miibus_statchg),

	{ 0, 0 }
};

static driver_t ste_driver = {
	"ste",
	ste_methods,
	sizeof(struct ste_softc)
};

static devclass_t ste_devclass;

DRIVER_MODULE(ste, pci, ste_driver, ste_devclass, 0, 0);
DRIVER_MODULE(miibus, ste, miibus_driver, miibus_devclass, 0, 0);

SYSCTL_NODE(_hw, OID_AUTO, ste, CTLFLAG_RD, 0, "if_ste parameters");

static int ste_rxsyncs;
SYSCTL_INT(_hw_ste, OID_AUTO, rxsyncs, CTLFLAG_RW, &ste_rxsyncs, 0, "");

#define STE_SETBIT4(sc, reg, x)				\
	CSR_WRITE_4(sc, reg, CSR_READ_4(sc, reg) | (x))

#define STE_CLRBIT4(sc, reg, x)				\
	CSR_WRITE_4(sc, reg, CSR_READ_4(sc, reg) & ~(x))

#define STE_SETBIT2(sc, reg, x)				\
	CSR_WRITE_2(sc, reg, CSR_READ_2(sc, reg) | (x))

#define STE_CLRBIT2(sc, reg, x)				\
	CSR_WRITE_2(sc, reg, CSR_READ_2(sc, reg) & ~(x))

#define STE_SETBIT1(sc, reg, x)				\
	CSR_WRITE_1(sc, reg, CSR_READ_1(sc, reg) | (x))

#define STE_CLRBIT1(sc, reg, x)				\
	CSR_WRITE_1(sc, reg, CSR_READ_1(sc, reg) & ~(x))


#define MII_SET(x)		STE_SETBIT1(sc, STE_PHYCTL, x)
#define MII_CLR(x)		STE_CLRBIT1(sc, STE_PHYCTL, x) 

/*
 * Sync the PHYs by setting data bit and strobing the clock 32 times.
 */
static void
ste_mii_sync(sc)
	struct ste_softc		*sc;
{
	register int		i;

	MII_SET(STE_PHYCTL_MDIR|STE_PHYCTL_MDATA);

	for (i = 0; i < 32; i++) {
		MII_SET(STE_PHYCTL_MCLK);
		DELAY(1);
		MII_CLR(STE_PHYCTL_MCLK);
		DELAY(1);
	}

	return;
}

/*
 * Clock a series of bits through the MII.
 */
static void
ste_mii_send(sc, bits, cnt)
	struct ste_softc		*sc;
	u_int32_t		bits;
	int			cnt;
{
	int			i;

	MII_CLR(STE_PHYCTL_MCLK);

	for (i = (0x1 << (cnt - 1)); i; i >>= 1) {
		if (bits & i) {
			MII_SET(STE_PHYCTL_MDATA);
                } else {
			MII_CLR(STE_PHYCTL_MDATA);
                }
		DELAY(1);
		MII_CLR(STE_PHYCTL_MCLK);
		DELAY(1);
		MII_SET(STE_PHYCTL_MCLK);
	}
}

/*
 * Read an PHY register through the MII.
 */
static int
ste_mii_readreg(sc, frame)
	struct ste_softc		*sc;
	struct ste_mii_frame	*frame;
	
{
	int			i, ack;

	STE_LOCK(sc);

	/*
	 * Set up frame for RX.
	 */
	frame->mii_stdelim = STE_MII_STARTDELIM;
	frame->mii_opcode = STE_MII_READOP;
	frame->mii_turnaround = 0;
	frame->mii_data = 0;
	
	CSR_WRITE_2(sc, STE_PHYCTL, 0);
	/*
 	 * Turn on data xmit.
	 */
	MII_SET(STE_PHYCTL_MDIR);

	ste_mii_sync(sc);

	/*
	 * Send command/address info.
	 */
	ste_mii_send(sc, frame->mii_stdelim, 2);
	ste_mii_send(sc, frame->mii_opcode, 2);
	ste_mii_send(sc, frame->mii_phyaddr, 5);
	ste_mii_send(sc, frame->mii_regaddr, 5);

	/* Turn off xmit. */
	MII_CLR(STE_PHYCTL_MDIR);

	/* Idle bit */
	MII_CLR((STE_PHYCTL_MCLK|STE_PHYCTL_MDATA));
	DELAY(1);
	MII_SET(STE_PHYCTL_MCLK);
	DELAY(1);

	/* Check for ack */
	MII_CLR(STE_PHYCTL_MCLK);
	DELAY(1);
	ack = CSR_READ_2(sc, STE_PHYCTL) & STE_PHYCTL_MDATA;
	MII_SET(STE_PHYCTL_MCLK);
	DELAY(1);

	/*
	 * Now try reading data bits. If the ack failed, we still
	 * need to clock through 16 cycles to keep the PHY(s) in sync.
	 */
	if (ack) {
		for(i = 0; i < 16; i++) {
			MII_CLR(STE_PHYCTL_MCLK);
			DELAY(1);
			MII_SET(STE_PHYCTL_MCLK);
			DELAY(1);
		}
		goto fail;
	}

	for (i = 0x8000; i; i >>= 1) {
		MII_CLR(STE_PHYCTL_MCLK);
		DELAY(1);
		if (!ack) {
			if (CSR_READ_2(sc, STE_PHYCTL) & STE_PHYCTL_MDATA)
				frame->mii_data |= i;
			DELAY(1);
		}
		MII_SET(STE_PHYCTL_MCLK);
		DELAY(1);
	}

fail:

	MII_CLR(STE_PHYCTL_MCLK);
	DELAY(1);
	MII_SET(STE_PHYCTL_MCLK);
	DELAY(1);

	STE_UNLOCK(sc);

	if (ack)
		return(1);
	return(0);
}

/*
 * Write to a PHY register through the MII.
 */
static int
ste_mii_writereg(sc, frame)
	struct ste_softc		*sc;
	struct ste_mii_frame	*frame;
	
{
	STE_LOCK(sc);

	/*
	 * Set up frame for TX.
	 */

	frame->mii_stdelim = STE_MII_STARTDELIM;
	frame->mii_opcode = STE_MII_WRITEOP;
	frame->mii_turnaround = STE_MII_TURNAROUND;
	
	/*
 	 * Turn on data output.
	 */
	MII_SET(STE_PHYCTL_MDIR);

	ste_mii_sync(sc);

	ste_mii_send(sc, frame->mii_stdelim, 2);
	ste_mii_send(sc, frame->mii_opcode, 2);
	ste_mii_send(sc, frame->mii_phyaddr, 5);
	ste_mii_send(sc, frame->mii_regaddr, 5);
	ste_mii_send(sc, frame->mii_turnaround, 2);
	ste_mii_send(sc, frame->mii_data, 16);

	/* Idle bit. */
	MII_SET(STE_PHYCTL_MCLK);
	DELAY(1);
	MII_CLR(STE_PHYCTL_MCLK);
	DELAY(1);

	/*
	 * Turn off xmit.
	 */
	MII_CLR(STE_PHYCTL_MDIR);

	STE_UNLOCK(sc);

	return(0);
}

static int
ste_miibus_readreg(dev, phy, reg)
	device_t		dev;
	int			phy, reg;
{
	struct ste_softc	*sc;
	struct ste_mii_frame	frame;

	sc = device_get_softc(dev);

	if ( sc->ste_one_phy && phy != 0 )
		return (0);

	bzero((char *)&frame, sizeof(frame));

	frame.mii_phyaddr = phy;
	frame.mii_regaddr = reg;
	ste_mii_readreg(sc, &frame);

	return(frame.mii_data);
}

static int
ste_miibus_writereg(dev, phy, reg, data)
	device_t		dev;
	int			phy, reg, data;
{
	struct ste_softc	*sc;
	struct ste_mii_frame	frame;

	sc = device_get_softc(dev);
	bzero((char *)&frame, sizeof(frame));

	frame.mii_phyaddr = phy;
	frame.mii_regaddr = reg;
	frame.mii_data = data;

	ste_mii_writereg(sc, &frame);

	return(0);
}

static void
ste_miibus_statchg(dev)
	device_t		dev;
{
	struct ste_softc	*sc;
	struct mii_data		*mii;

	sc = device_get_softc(dev);
	STE_LOCK(sc);
	mii = device_get_softc(sc->ste_miibus);

	if ((mii->mii_media_active & IFM_GMASK) == IFM_FDX) {
		STE_SETBIT2(sc, STE_MACCTL0, STE_MACCTL0_FULLDUPLEX);
	} else {
		STE_CLRBIT2(sc, STE_MACCTL0, STE_MACCTL0_FULLDUPLEX);
	}
	STE_UNLOCK(sc);

	return;
}
 
static int
ste_ifmedia_upd(ifp)
	struct ifnet		*ifp;
{
	struct ste_softc	*sc;
	struct mii_data		*mii;

	sc = ifp->if_softc;
	mii = device_get_softc(sc->ste_miibus);
	sc->ste_link = 0;
	if (mii->mii_instance) {
		struct mii_softc	*miisc;
		LIST_FOREACH(miisc, &mii->mii_phys, mii_list)
			mii_phy_reset(miisc);
	}
	mii_mediachg(mii);

	return(0);
}

static void
ste_ifmedia_sts(ifp, ifmr)
	struct ifnet		*ifp;
	struct ifmediareq	*ifmr;
{
	struct ste_softc	*sc;
	struct mii_data		*mii;

	sc = ifp->if_softc;
	mii = device_get_softc(sc->ste_miibus);

	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;

	return;
}

static void
ste_wait(sc)
	struct ste_softc		*sc;
{
	register int		i;

	for (i = 0; i < STE_TIMEOUT; i++) {
		if (!(CSR_READ_4(sc, STE_DMACTL) & STE_DMACTL_DMA_HALTINPROG))
			break;
	}

	if (i == STE_TIMEOUT)
		printf("ste%d: command never completed!\n", sc->ste_unit);

	return;
}

/*
 * The EEPROM is slow: give it time to come ready after issuing
 * it a command.
 */
static int
ste_eeprom_wait(sc)
	struct ste_softc		*sc;
{
	int			i;

	DELAY(1000);

	for (i = 0; i < 100; i++) {
		if (CSR_READ_2(sc, STE_EEPROM_CTL) & STE_EECTL_BUSY)
			DELAY(1000);
		else
			break;
	}

	if (i == 100) {
		printf("ste%d: eeprom failed to come ready\n", sc->ste_unit);
		return(1);
	}

	return(0);
}

/*
 * Read a sequence of words from the EEPROM. Note that ethernet address
 * data is stored in the EEPROM in network byte order.
 */
static int
ste_read_eeprom(sc, dest, off, cnt, swap)
	struct ste_softc		*sc;
	caddr_t			dest;
	int			off;
	int			cnt;
	int			swap;
{
	int			err = 0, i;
	u_int16_t		word = 0, *ptr;

	if (ste_eeprom_wait(sc))
		return(1);

	for (i = 0; i < cnt; i++) {
		CSR_WRITE_2(sc, STE_EEPROM_CTL, STE_EEOPCODE_READ | (off + i));
		err = ste_eeprom_wait(sc);
		if (err)
			break;
		word = CSR_READ_2(sc, STE_EEPROM_DATA);
		ptr = (u_int16_t *)(dest + (i * 2));
		if (swap)
			*ptr = ntohs(word);
		else
			*ptr = word;	
	}

	return(err ? 1 : 0);
}

static void
ste_setmulti(sc)
	struct ste_softc	*sc;
{
	struct ifnet		*ifp;
	int			h = 0;
	u_int32_t		hashes[2] = { 0, 0 };
	struct ifmultiaddr	*ifma;

	ifp = &sc->arpcom.ac_if;
	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
		STE_SETBIT1(sc, STE_RX_MODE, STE_RXMODE_ALLMULTI);
		STE_CLRBIT1(sc, STE_RX_MODE, STE_RXMODE_MULTIHASH);
		return;
	}

	/* first, zot all the existing hash bits */
	CSR_WRITE_2(sc, STE_MAR0, 0);
	CSR_WRITE_2(sc, STE_MAR1, 0);
	CSR_WRITE_2(sc, STE_MAR2, 0);
	CSR_WRITE_2(sc, STE_MAR3, 0);

	/* now program new ones */
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		h = ether_crc32_be(LLADDR((struct sockaddr_dl *)
		    ifma->ifma_addr), ETHER_ADDR_LEN) & 0x3F;
		if (h < 32)
			hashes[0] |= (1 << h);
		else
			hashes[1] |= (1 << (h - 32));
	}

	CSR_WRITE_2(sc, STE_MAR0, hashes[0] & 0xFFFF);
	CSR_WRITE_2(sc, STE_MAR1, (hashes[0] >> 16) & 0xFFFF);
	CSR_WRITE_2(sc, STE_MAR2, hashes[1] & 0xFFFF);
	CSR_WRITE_2(sc, STE_MAR3, (hashes[1] >> 16) & 0xFFFF);
	STE_CLRBIT1(sc, STE_RX_MODE, STE_RXMODE_ALLMULTI);
	STE_SETBIT1(sc, STE_RX_MODE, STE_RXMODE_MULTIHASH);

	return;
}

#ifdef DEVICE_POLLING
static poll_handler_t ste_poll;

static void
ste_poll(struct ifnet *ifp, enum poll_cmd cmd, int count)
{
	struct ste_softc *sc = ifp->if_softc;

	STE_LOCK(sc);
	if (!(ifp->if_capenable & IFCAP_POLLING)) {
		ether_poll_deregister(ifp);
		cmd = POLL_DEREGISTER;
	}
	if (cmd == POLL_DEREGISTER) { /* final call, enable interrupts */
		CSR_WRITE_2(sc, STE_IMR, STE_INTRS);
		goto done;
	}

	sc->rxcycles = count;
	if (cmd == POLL_AND_CHECK_STATUS)
		ste_rxeoc(sc);
	ste_rxeof(sc);
	ste_txeof(sc);
	if (ifp->if_snd.ifq_head != NULL)
		ste_start(ifp);

	if (cmd == POLL_AND_CHECK_STATUS) {
		u_int16_t status;

		status = CSR_READ_2(sc, STE_ISR_ACK);

		if (status & STE_ISR_TX_DONE)
			ste_txeoc(sc);

		if (status & STE_ISR_STATS_OFLOW) {
			untimeout(ste_stats_update, sc, sc->ste_stat_ch);
			ste_stats_update(sc);
		}

		if (status & STE_ISR_LINKEVENT)
			mii_pollstat(device_get_softc(sc->ste_miibus));

		if (status & STE_ISR_HOSTERR) {
			ste_reset(sc);
			ste_init(sc);
		}
	}
done:
	STE_UNLOCK(sc);
}
#endif /* DEVICE_POLLING */

static void
ste_intr(xsc)
	void			*xsc;
{
	struct ste_softc	*sc;
	struct ifnet		*ifp;
	u_int16_t		status;

	sc = xsc;
	STE_LOCK(sc);
	ifp = &sc->arpcom.ac_if;

#ifdef DEVICE_POLLING
	if (ifp->if_flags & IFF_POLLING)
		goto done;
	if ((ifp->if_capenable & IFCAP_POLLING) &&
	    ether_poll_register(ste_poll, ifp)) { /* ok, disable interrupts */
		CSR_WRITE_2(sc, STE_IMR, 0);
		ste_poll(ifp, 0, 1);
		goto done;
	}
#endif /* DEVICE_POLLING */

	/* See if this is really our interrupt. */
	if (!(CSR_READ_2(sc, STE_ISR) & STE_ISR_INTLATCH)) {
		STE_UNLOCK(sc);
		return;
	}

	for (;;) {
		status = CSR_READ_2(sc, STE_ISR_ACK);

		if (!(status & STE_INTRS))
			break;

		if (status & STE_ISR_RX_DMADONE) {
			ste_rxeoc(sc);
			ste_rxeof(sc);
		}

		if (status & STE_ISR_TX_DMADONE)
			ste_txeof(sc);

		if (status & STE_ISR_TX_DONE)
			ste_txeoc(sc);

		if (status & STE_ISR_STATS_OFLOW) {
			untimeout(ste_stats_update, sc, sc->ste_stat_ch);
			ste_stats_update(sc);
		}

		if (status & STE_ISR_LINKEVENT)
			mii_pollstat(device_get_softc(sc->ste_miibus));


		if (status & STE_ISR_HOSTERR) {
			ste_reset(sc);
			ste_init(sc);
		}
	}

	/* Re-enable interrupts */
	CSR_WRITE_2(sc, STE_IMR, STE_INTRS);

	if (ifp->if_snd.ifq_head != NULL)
		ste_start(ifp);

#ifdef DEVICE_POLLING
done:
#endif /* DEVICE_POLLING */
	STE_UNLOCK(sc);

	return;
}

static void
ste_rxeoc(struct ste_softc *sc)
{
	struct ste_chain_onefrag *cur_rx;

	STE_LOCK_ASSERT(sc);

	if (sc->ste_cdata.ste_rx_head->ste_ptr->ste_status == 0) {
		cur_rx = sc->ste_cdata.ste_rx_head;
		do {
			cur_rx = cur_rx->ste_next;
			/* If the ring is empty, just return. */
			if (cur_rx == sc->ste_cdata.ste_rx_head)
				return;
		} while (cur_rx->ste_ptr->ste_status == 0);
		if (sc->ste_cdata.ste_rx_head->ste_ptr->ste_status == 0) {
			/* We've fallen behind the chip: catch it. */
			sc->ste_cdata.ste_rx_head = cur_rx;
			++ste_rxsyncs;
		}
	}
}

/*
 * A frame has been uploaded: pass the resulting mbuf chain up to
 * the higher level protocols.
 */
static void
ste_rxeof(sc)
	struct ste_softc		*sc;
{
        struct mbuf		*m;
        struct ifnet		*ifp;
	struct ste_chain_onefrag	*cur_rx;
	int			total_len = 0, count=0;
	u_int32_t		rxstat;

	STE_LOCK_ASSERT(sc);

	ifp = &sc->arpcom.ac_if;

	while((rxstat = sc->ste_cdata.ste_rx_head->ste_ptr->ste_status)
	      & STE_RXSTAT_DMADONE) {
#ifdef DEVICE_POLLING
		if (ifp->if_flags & IFF_POLLING) {
			if (sc->rxcycles <= 0)
				break;
			sc->rxcycles--;
		}
#endif /* DEVICE_POLLING */
		if ((STE_RX_LIST_CNT - count) < 3) {
			break;
		}

		cur_rx = sc->ste_cdata.ste_rx_head;
		sc->ste_cdata.ste_rx_head = cur_rx->ste_next;

		/*
		 * If an error occurs, update stats, clear the
		 * status word and leave the mbuf cluster in place:
		 * it should simply get re-used next time this descriptor
	 	 * comes up in the ring.
		 */
		if (rxstat & STE_RXSTAT_FRAME_ERR) {
			ifp->if_ierrors++;
			cur_rx->ste_ptr->ste_status = 0;
			continue;
		}

		/*
		 * If there error bit was not set, the upload complete
		 * bit should be set which means we have a valid packet.
		 * If not, something truly strange has happened.
		 */
		if (!(rxstat & STE_RXSTAT_DMADONE)) {
			printf("ste%d: bad receive status -- packet dropped\n",
							sc->ste_unit);
			ifp->if_ierrors++;
			cur_rx->ste_ptr->ste_status = 0;
			continue;
		}

		/* No errors; receive the packet. */	
		m = cur_rx->ste_mbuf;
		total_len = cur_rx->ste_ptr->ste_status & STE_RXSTAT_FRAMELEN;

		/*
		 * Try to conjure up a new mbuf cluster. If that
		 * fails, it means we have an out of memory condition and
		 * should leave the buffer in place and continue. This will
		 * result in a lost packet, but there's little else we
		 * can do in this situation.
		 */
		if (ste_newbuf(sc, cur_rx, NULL) == ENOBUFS) {
			ifp->if_ierrors++;
			cur_rx->ste_ptr->ste_status = 0;
			continue;
		}

		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = m->m_len = total_len;

		ifp->if_ipackets++;
		STE_UNLOCK(sc);
		(*ifp->if_input)(ifp, m);
		STE_LOCK(sc);

		cur_rx->ste_ptr->ste_status = 0;
		count++;
	}

	return;
}

static void
ste_txeoc(sc)
	struct ste_softc	*sc;
{
	u_int8_t		txstat;
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;

	while ((txstat = CSR_READ_1(sc, STE_TX_STATUS)) &
	    STE_TXSTATUS_TXDONE) {
		if (txstat & STE_TXSTATUS_UNDERRUN ||
		    txstat & STE_TXSTATUS_EXCESSCOLLS ||
		    txstat & STE_TXSTATUS_RECLAIMERR) {
			ifp->if_oerrors++;
			printf("ste%d: transmission error: %x\n",
			    sc->ste_unit, txstat);

			ste_reset(sc);
			ste_init(sc);

			if (txstat & STE_TXSTATUS_UNDERRUN &&
			    sc->ste_tx_thresh < STE_PACKET_SIZE) {
				sc->ste_tx_thresh += STE_MIN_FRAMELEN;
				printf("ste%d: tx underrun, increasing tx"
				    " start threshold to %d bytes\n",
				    sc->ste_unit, sc->ste_tx_thresh);
			}
			CSR_WRITE_2(sc, STE_TX_STARTTHRESH, sc->ste_tx_thresh);
			CSR_WRITE_2(sc, STE_TX_RECLAIM_THRESH,
			    (STE_PACKET_SIZE >> 4));
		}
		ste_init(sc);
		CSR_WRITE_2(sc, STE_TX_STATUS, txstat);
	}

	return;
}

static void
ste_txeof(sc)
	struct ste_softc	*sc;
{
	struct ste_chain	*cur_tx;
	struct ifnet		*ifp;
	int			idx;

	ifp = &sc->arpcom.ac_if;

	idx = sc->ste_cdata.ste_tx_cons;
	while(idx != sc->ste_cdata.ste_tx_prod) {
		cur_tx = &sc->ste_cdata.ste_tx_chain[idx];

		if (!(cur_tx->ste_ptr->ste_ctl & STE_TXCTL_DMADONE))
			break;

		m_freem(cur_tx->ste_mbuf);
		cur_tx->ste_mbuf = NULL;
		ifp->if_flags &= ~IFF_OACTIVE;
		ifp->if_opackets++;

		STE_INC(idx, STE_TX_LIST_CNT);
	}

	sc->ste_cdata.ste_tx_cons = idx;
	if (idx == sc->ste_cdata.ste_tx_prod)
		ifp->if_timer = 0;
}

static void
ste_stats_update(xsc)
	void			*xsc;
{
	struct ste_softc	*sc;
	struct ifnet		*ifp;
	struct mii_data		*mii;

	sc = xsc;
	STE_LOCK(sc);

	ifp = &sc->arpcom.ac_if;
	mii = device_get_softc(sc->ste_miibus);

	ifp->if_collisions += CSR_READ_1(sc, STE_LATE_COLLS)
	    + CSR_READ_1(sc, STE_MULTI_COLLS)
	    + CSR_READ_1(sc, STE_SINGLE_COLLS);

	if (!sc->ste_link) {
		mii_pollstat(mii);
		if (mii->mii_media_status & IFM_ACTIVE &&
		    IFM_SUBTYPE(mii->mii_media_active) != IFM_NONE) {
			sc->ste_link++;
			/*
			* we don't get a call-back on re-init so do it
			* otherwise we get stuck in the wrong link state
			*/
			ste_miibus_statchg(sc->ste_dev);
			if (ifp->if_snd.ifq_head != NULL)
				ste_start(ifp);
		}
	}

	sc->ste_stat_ch = timeout(ste_stats_update, sc, hz);
	STE_UNLOCK(sc);

	return;
}


/*
 * Probe for a Sundance ST201 chip. Check the PCI vendor and device
 * IDs against our list and return a device name if we find a match.
 */
static int
ste_probe(dev)
	device_t		dev;
{
	struct ste_type		*t;

	t = ste_devs;

	while(t->ste_name != NULL) {
		if ((pci_get_vendor(dev) == t->ste_vid) &&
		    (pci_get_device(dev) == t->ste_did)) {
			device_set_desc(dev, t->ste_name);
			return (BUS_PROBE_DEFAULT);
		}
		t++;
	}

	return(ENXIO);
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
static int
ste_attach(dev)
	device_t		dev;
{
	struct ste_softc	*sc;
	struct ifnet		*ifp;
	int			unit, error = 0, rid;

	sc = device_get_softc(dev);
	unit = device_get_unit(dev);
	sc->ste_dev = dev;

	/*
	 * Only use one PHY since this chip reports multiple
	 * Note on the DFE-550 the PHY is at 1 on the DFE-580
	 * it is at 0 & 1.  It is rev 0x12.
	 */
	if (pci_get_vendor(dev) == DL_VENDORID &&
	    pci_get_device(dev) == DL_DEVICEID_DL10050 &&
	    pci_get_revid(dev) == 0x12 )
		sc->ste_one_phy = 1;

	mtx_init(&sc->ste_mtx, device_get_nameunit(dev), MTX_NETWORK_LOCK,
	    MTX_DEF | MTX_RECURSE);
	/*
	 * Map control/status registers.
	 */
	pci_enable_busmaster(dev);

	rid = STE_RID;
	sc->ste_res = bus_alloc_resource_any(dev, STE_RES, &rid, RF_ACTIVE);

	if (sc->ste_res == NULL) {
		printf ("ste%d: couldn't map ports/memory\n", unit);
		error = ENXIO;
		goto fail;
	}

	sc->ste_btag = rman_get_bustag(sc->ste_res);
	sc->ste_bhandle = rman_get_bushandle(sc->ste_res);

	/* Allocate interrupt */
	rid = 0;
	sc->ste_irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_SHAREABLE | RF_ACTIVE);

	if (sc->ste_irq == NULL) {
		printf("ste%d: couldn't map interrupt\n", unit);
		error = ENXIO;
		goto fail;
	}

	callout_handle_init(&sc->ste_stat_ch);

	/* Reset the adapter. */
	ste_reset(sc);

	/*
	 * Get station address from the EEPROM.
	 */
	if (ste_read_eeprom(sc, (caddr_t)&sc->arpcom.ac_enaddr,
	    STE_EEADDR_NODE0, 3, 0)) {
		printf("ste%d: failed to read station address\n", unit);
		error = ENXIO;;
		goto fail;
	}

	sc->ste_unit = unit;

	/* Allocate the descriptor queues. */
	sc->ste_ldata = contigmalloc(sizeof(struct ste_list_data), M_DEVBUF,
	    M_NOWAIT, 0, 0xffffffff, PAGE_SIZE, 0);

	if (sc->ste_ldata == NULL) {
		printf("ste%d: no memory for list buffers!\n", unit);
		error = ENXIO;
		goto fail;
	}

	bzero(sc->ste_ldata, sizeof(struct ste_list_data));

	/* Do MII setup. */
	if (mii_phy_probe(dev, &sc->ste_miibus,
	    ste_ifmedia_upd, ste_ifmedia_sts)) {
		printf("ste%d: MII without any phy!\n", sc->ste_unit);
		error = ENXIO;
		goto fail;
	}

	ifp = &sc->arpcom.ac_if;
	ifp->if_softc = sc;
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST |
	    IFF_NEEDSGIANT;
	ifp->if_ioctl = ste_ioctl;
	ifp->if_start = ste_start;
	ifp->if_watchdog = ste_watchdog;
	ifp->if_init = ste_init;
	ifp->if_baudrate = 10000000;
	ifp->if_snd.ifq_maxlen = STE_TX_LIST_CNT - 1;

	sc->ste_tx_thresh = STE_TXSTART_THRESH;

	/*
	 * Call MI attach routine.
	 */
	ether_ifattach(ifp, sc->arpcom.ac_enaddr);

	/*
	 * Tell the upper layer(s) we support long frames.
	 */
	ifp->if_data.ifi_hdrlen = sizeof(struct ether_vlan_header);
	ifp->if_capabilities |= IFCAP_VLAN_MTU;
#ifdef DEVICE_POLLING
	ifp->if_capabilities |= IFCAP_POLLING;
#endif
	ifp->if_capenable = ifp->if_capabilities;

	/* Hook interrupt last to avoid having to lock softc */
	error = bus_setup_intr(dev, sc->ste_irq, INTR_TYPE_NET,
	    ste_intr, sc, &sc->ste_intrhand);

	if (error) {
		printf("ste%d: couldn't set up irq\n", unit);
		ether_ifdetach(ifp);
		goto fail;
	}

fail:
	if (error)
		ste_detach(dev);

	return(error);
}

/*
 * Shutdown hardware and free up resources. This can be called any
 * time after the mutex has been initialized. It is called in both
 * the error case in attach and the normal detach case so it needs
 * to be careful about only freeing resources that have actually been
 * allocated.
 */
static int
ste_detach(dev)
	device_t		dev;
{
	struct ste_softc	*sc;
	struct ifnet		*ifp;

	sc = device_get_softc(dev);
	KASSERT(mtx_initialized(&sc->ste_mtx), ("ste mutex not initialized"));
	STE_LOCK(sc);
	ifp = &sc->arpcom.ac_if;

	/* These should only be active if attach succeeded */
	if (device_is_attached(dev)) {
		ste_stop(sc);
		ether_ifdetach(ifp);
	}
	if (sc->ste_miibus)
		device_delete_child(dev, sc->ste_miibus);
	bus_generic_detach(dev);

	if (sc->ste_intrhand)
		bus_teardown_intr(dev, sc->ste_irq, sc->ste_intrhand);
	if (sc->ste_irq)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->ste_irq);
	if (sc->ste_res)
		bus_release_resource(dev, STE_RES, STE_RID, sc->ste_res);

	if (sc->ste_ldata) {
		contigfree(sc->ste_ldata, sizeof(struct ste_list_data),
		    M_DEVBUF);
	}

	STE_UNLOCK(sc);
	mtx_destroy(&sc->ste_mtx);

	return(0);
}

static int
ste_newbuf(sc, c, m)
	struct ste_softc	*sc;
	struct ste_chain_onefrag	*c;
	struct mbuf		*m;
{
	struct mbuf		*m_new = NULL;

	if (m == NULL) {
		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL)
			return(ENOBUFS);
		MCLGET(m_new, M_DONTWAIT);
		if (!(m_new->m_flags & M_EXT)) {
			m_freem(m_new);
			return(ENOBUFS);
		}
		m_new->m_len = m_new->m_pkthdr.len = MCLBYTES;
	} else {
		m_new = m;
		m_new->m_len = m_new->m_pkthdr.len = MCLBYTES;
		m_new->m_data = m_new->m_ext.ext_buf;
	}

	m_adj(m_new, ETHER_ALIGN);

	c->ste_mbuf = m_new;
	c->ste_ptr->ste_status = 0;
	c->ste_ptr->ste_frag.ste_addr = vtophys(mtod(m_new, caddr_t));
	c->ste_ptr->ste_frag.ste_len = (1536 + ETHER_VLAN_ENCAP_LEN) | STE_FRAG_LAST;

	return(0);
}

static int
ste_init_rx_list(sc)
	struct ste_softc	*sc;
{
	struct ste_chain_data	*cd;
	struct ste_list_data	*ld;
	int			i;

	cd = &sc->ste_cdata;
	ld = sc->ste_ldata;

	for (i = 0; i < STE_RX_LIST_CNT; i++) {
		cd->ste_rx_chain[i].ste_ptr = &ld->ste_rx_list[i];
		if (ste_newbuf(sc, &cd->ste_rx_chain[i], NULL) == ENOBUFS)
			return(ENOBUFS);
		if (i == (STE_RX_LIST_CNT - 1)) {
			cd->ste_rx_chain[i].ste_next =
			    &cd->ste_rx_chain[0];
			ld->ste_rx_list[i].ste_next =
			    vtophys(&ld->ste_rx_list[0]);
		} else {
			cd->ste_rx_chain[i].ste_next =
			    &cd->ste_rx_chain[i + 1];
			ld->ste_rx_list[i].ste_next =
			    vtophys(&ld->ste_rx_list[i + 1]);
		}
		ld->ste_rx_list[i].ste_status = 0;
	}

	cd->ste_rx_head = &cd->ste_rx_chain[0];

	return(0);
}

static void
ste_init_tx_list(sc)
	struct ste_softc	*sc;
{
	struct ste_chain_data	*cd;
	struct ste_list_data	*ld;
	int			i;

	cd = &sc->ste_cdata;
	ld = sc->ste_ldata;
	for (i = 0; i < STE_TX_LIST_CNT; i++) {
		cd->ste_tx_chain[i].ste_ptr = &ld->ste_tx_list[i];
		cd->ste_tx_chain[i].ste_ptr->ste_next = 0;
		cd->ste_tx_chain[i].ste_ptr->ste_ctl  = 0;
		cd->ste_tx_chain[i].ste_phys = vtophys(&ld->ste_tx_list[i]);
		if (i == (STE_TX_LIST_CNT - 1))
			cd->ste_tx_chain[i].ste_next =
			    &cd->ste_tx_chain[0];
		else
			cd->ste_tx_chain[i].ste_next =
			    &cd->ste_tx_chain[i + 1];
	}

	cd->ste_tx_prod = 0;
	cd->ste_tx_cons = 0;

	return;
}

static void
ste_init(xsc)
	void			*xsc;
{
	struct ste_softc	*sc;
	int			i;
	struct ifnet		*ifp;

	sc = xsc;
	STE_LOCK(sc);
	ifp = &sc->arpcom.ac_if;

	ste_stop(sc);

	/* Init our MAC address */
	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		CSR_WRITE_1(sc, STE_PAR0 + i, sc->arpcom.ac_enaddr[i]);
	}

	/* Init RX list */
	if (ste_init_rx_list(sc) == ENOBUFS) {
		printf("ste%d: initialization failed: no "
		    "memory for RX buffers\n", sc->ste_unit);
		ste_stop(sc);
		STE_UNLOCK(sc);
		return;
	}

	/* Set RX polling interval */
	CSR_WRITE_1(sc, STE_RX_DMAPOLL_PERIOD, 64);

	/* Init TX descriptors */
	ste_init_tx_list(sc);

	/* Set the TX freethresh value */
	CSR_WRITE_1(sc, STE_TX_DMABURST_THRESH, STE_PACKET_SIZE >> 8);

	/* Set the TX start threshold for best performance. */
	CSR_WRITE_2(sc, STE_TX_STARTTHRESH, sc->ste_tx_thresh);

	/* Set the TX reclaim threshold. */
	CSR_WRITE_1(sc, STE_TX_RECLAIM_THRESH, (STE_PACKET_SIZE >> 4));

	/* Set up the RX filter. */
	CSR_WRITE_1(sc, STE_RX_MODE, STE_RXMODE_UNICAST);

	/* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC) {
		STE_SETBIT1(sc, STE_RX_MODE, STE_RXMODE_PROMISC);
	} else {
		STE_CLRBIT1(sc, STE_RX_MODE, STE_RXMODE_PROMISC);
	}

	/* Set capture broadcast bit to accept broadcast frames. */
	if (ifp->if_flags & IFF_BROADCAST) {
		STE_SETBIT1(sc, STE_RX_MODE, STE_RXMODE_BROADCAST);
	} else {
		STE_CLRBIT1(sc, STE_RX_MODE, STE_RXMODE_BROADCAST);
	}

	ste_setmulti(sc);

	/* Load the address of the RX list. */
	STE_SETBIT4(sc, STE_DMACTL, STE_DMACTL_RXDMA_STALL);
	ste_wait(sc);
	CSR_WRITE_4(sc, STE_RX_DMALIST_PTR,
	    vtophys(&sc->ste_ldata->ste_rx_list[0]));
	STE_SETBIT4(sc, STE_DMACTL, STE_DMACTL_RXDMA_UNSTALL);
	STE_SETBIT4(sc, STE_DMACTL, STE_DMACTL_RXDMA_UNSTALL);

	/* Set TX polling interval (defer until we TX first packet */
	CSR_WRITE_1(sc, STE_TX_DMAPOLL_PERIOD, 0);

	/* Load address of the TX list */
	STE_SETBIT4(sc, STE_DMACTL, STE_DMACTL_TXDMA_STALL);
	ste_wait(sc);
	CSR_WRITE_4(sc, STE_TX_DMALIST_PTR, 0);
	STE_SETBIT4(sc, STE_DMACTL, STE_DMACTL_TXDMA_UNSTALL);
	STE_SETBIT4(sc, STE_DMACTL, STE_DMACTL_TXDMA_UNSTALL);
	ste_wait(sc);
	sc->ste_tx_prev = NULL;

	/* Enable receiver and transmitter */
	CSR_WRITE_2(sc, STE_MACCTL0, 0);
	CSR_WRITE_2(sc, STE_MACCTL1, 0);
	STE_SETBIT2(sc, STE_MACCTL1, STE_MACCTL1_TX_ENABLE);
	STE_SETBIT2(sc, STE_MACCTL1, STE_MACCTL1_RX_ENABLE);

	/* Enable stats counters. */
	STE_SETBIT2(sc, STE_MACCTL1, STE_MACCTL1_STATS_ENABLE);

	CSR_WRITE_2(sc, STE_ISR, 0xFFFF);
#ifdef DEVICE_POLLING
	/* Disable interrupts if we are polling. */
	if (ifp->if_flags & IFF_POLLING)
		CSR_WRITE_2(sc, STE_IMR, 0);
	else   
#endif /* DEVICE_POLLING */
	/* Enable interrupts. */
	CSR_WRITE_2(sc, STE_IMR, STE_INTRS);

	/* Accept VLAN length packets */
	CSR_WRITE_2(sc, STE_MAX_FRAMELEN, ETHER_MAX_LEN + ETHER_VLAN_ENCAP_LEN);

	ste_ifmedia_upd(ifp);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	sc->ste_stat_ch = timeout(ste_stats_update, sc, hz);
	STE_UNLOCK(sc);

	return;
}

static void
ste_stop(sc)
	struct ste_softc	*sc;
{
	int			i;
	struct ifnet		*ifp;

	STE_LOCK(sc);
	ifp = &sc->arpcom.ac_if;

	untimeout(ste_stats_update, sc, sc->ste_stat_ch);
	ifp->if_flags &= ~(IFF_RUNNING|IFF_OACTIVE);
#ifdef DEVICE_POLLING
	ether_poll_deregister(ifp);
#endif /* DEVICE_POLLING */

	CSR_WRITE_2(sc, STE_IMR, 0);
	STE_SETBIT2(sc, STE_MACCTL1, STE_MACCTL1_TX_DISABLE);
	STE_SETBIT2(sc, STE_MACCTL1, STE_MACCTL1_RX_DISABLE);
	STE_SETBIT2(sc, STE_MACCTL1, STE_MACCTL1_STATS_DISABLE);
	STE_SETBIT2(sc, STE_DMACTL, STE_DMACTL_TXDMA_STALL);
	STE_SETBIT2(sc, STE_DMACTL, STE_DMACTL_RXDMA_STALL);
	ste_wait(sc);
	/* 
	 * Try really hard to stop the RX engine or under heavy RX 
	 * data chip will write into de-allocated memory.
	 */
	ste_reset(sc);

	sc->ste_link = 0;

	for (i = 0; i < STE_RX_LIST_CNT; i++) {
		if (sc->ste_cdata.ste_rx_chain[i].ste_mbuf != NULL) {
			m_freem(sc->ste_cdata.ste_rx_chain[i].ste_mbuf);
			sc->ste_cdata.ste_rx_chain[i].ste_mbuf = NULL;
		}
	}

	for (i = 0; i < STE_TX_LIST_CNT; i++) {
		if (sc->ste_cdata.ste_tx_chain[i].ste_mbuf != NULL) {
			m_freem(sc->ste_cdata.ste_tx_chain[i].ste_mbuf);
			sc->ste_cdata.ste_tx_chain[i].ste_mbuf = NULL;
		}
	}

	bzero(sc->ste_ldata, sizeof(struct ste_list_data));
	STE_UNLOCK(sc);

	return;
}

static void
ste_reset(sc)
	struct ste_softc	*sc;
{
	int			i;

	STE_SETBIT4(sc, STE_ASICCTL,
	    STE_ASICCTL_GLOBAL_RESET|STE_ASICCTL_RX_RESET|
	    STE_ASICCTL_TX_RESET|STE_ASICCTL_DMA_RESET|
	    STE_ASICCTL_FIFO_RESET|STE_ASICCTL_NETWORK_RESET|
	    STE_ASICCTL_AUTOINIT_RESET|STE_ASICCTL_HOST_RESET|
	    STE_ASICCTL_EXTRESET_RESET);

	DELAY(100000);

	for (i = 0; i < STE_TIMEOUT; i++) {
		if (!(CSR_READ_4(sc, STE_ASICCTL) & STE_ASICCTL_RESET_BUSY))
			break;
	}

	if (i == STE_TIMEOUT)
		printf("ste%d: global reset never completed\n", sc->ste_unit);

	return;
}

static int
ste_ioctl(ifp, command, data)
	struct ifnet		*ifp;
	u_long			command;
	caddr_t			data;
{
	struct ste_softc	*sc;
	struct ifreq		*ifr;
	struct mii_data		*mii;
	int			error = 0;

	sc = ifp->if_softc;
	STE_LOCK(sc);
	ifr = (struct ifreq *)data;

	switch(command) {
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING &&
			    ifp->if_flags & IFF_PROMISC &&
			    !(sc->ste_if_flags & IFF_PROMISC)) {
				STE_SETBIT1(sc, STE_RX_MODE,
				    STE_RXMODE_PROMISC);
			} else if (ifp->if_flags & IFF_RUNNING &&
			    !(ifp->if_flags & IFF_PROMISC) &&
			    sc->ste_if_flags & IFF_PROMISC) {
				STE_CLRBIT1(sc, STE_RX_MODE,
				    STE_RXMODE_PROMISC);
			} 
			if (ifp->if_flags & IFF_RUNNING &&
			    (ifp->if_flags ^ sc->ste_if_flags) & IFF_ALLMULTI)
				ste_setmulti(sc);
			if (!(ifp->if_flags & IFF_RUNNING)) {
				sc->ste_tx_thresh = STE_TXSTART_THRESH;
				ste_init(sc);
			}
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				ste_stop(sc);
		}
		sc->ste_if_flags = ifp->if_flags;
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		ste_setmulti(sc);
		error = 0;
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		mii = device_get_softc(sc->ste_miibus);
		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, command);
		break;
	case SIOCSIFCAP:
		ifp->if_capenable &= ~IFCAP_POLLING;
		ifp->if_capenable |= ifr->ifr_reqcap & IFCAP_POLLING;
		break;
	default:
		error = ether_ioctl(ifp, command, data);
		break;
	}

	STE_UNLOCK(sc);

	return(error);
}

static int
ste_encap(sc, c, m_head)
	struct ste_softc	*sc;
	struct ste_chain	*c;
	struct mbuf		*m_head;
{
	int			frag = 0;
	struct ste_frag		*f = NULL;
	struct mbuf		*m;
	struct ste_desc		*d;

	d = c->ste_ptr;
	d->ste_ctl = 0;

encap_retry:
	for (m = m_head, frag = 0; m != NULL; m = m->m_next) {
		if (m->m_len != 0) {
			if (frag == STE_MAXFRAGS)
				break;
			f = &d->ste_frags[frag];
			f->ste_addr = vtophys(mtod(m, vm_offset_t));
			f->ste_len = m->m_len;
			frag++;
		}
	}

	if (m != NULL) {
		struct mbuf *mn;

		/*
		 * We ran out of segments. We have to recopy this
		 * mbuf chain first. Bail out if we can't get the
		 * new buffers.
		 */
		mn = m_defrag(m_head, M_DONTWAIT);
		if (mn == NULL) {
			m_freem(m_head);
			return ENOMEM;
		}
		m_head = mn;
		goto encap_retry;
	}

	c->ste_mbuf = m_head;
	d->ste_frags[frag - 1].ste_len |= STE_FRAG_LAST;
	d->ste_ctl = 1;

	return(0);
}

static void
ste_start(ifp)
	struct ifnet		*ifp;
{
	struct ste_softc	*sc;
	struct mbuf		*m_head = NULL;
	struct ste_chain	*cur_tx;
	int			idx;

	sc = ifp->if_softc;
	STE_LOCK(sc);

	if (!sc->ste_link) {
		STE_UNLOCK(sc);
		return;
	}

	if (ifp->if_flags & IFF_OACTIVE) {
		STE_UNLOCK(sc);
		return;
	}

	idx = sc->ste_cdata.ste_tx_prod;

	while(sc->ste_cdata.ste_tx_chain[idx].ste_mbuf == NULL) {
		/*
		 * We cannot re-use the last (free) descriptor;
		 * the chip may not have read its ste_next yet.
		 */
		if (STE_NEXT(idx, STE_TX_LIST_CNT) ==
		    sc->ste_cdata.ste_tx_cons) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

		IF_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		cur_tx = &sc->ste_cdata.ste_tx_chain[idx];

		if (ste_encap(sc, cur_tx, m_head) != 0)
			break;

		cur_tx->ste_ptr->ste_next = 0;

		if (sc->ste_tx_prev == NULL) {
			cur_tx->ste_ptr->ste_ctl = STE_TXCTL_DMAINTR | 1;
			/* Load address of the TX list */
			STE_SETBIT4(sc, STE_DMACTL, STE_DMACTL_TXDMA_STALL);
			ste_wait(sc);

			CSR_WRITE_4(sc, STE_TX_DMALIST_PTR,
			    vtophys(&sc->ste_ldata->ste_tx_list[0]));

			/* Set TX polling interval to start TX engine */
			CSR_WRITE_1(sc, STE_TX_DMAPOLL_PERIOD, 64);
		  
			STE_SETBIT4(sc, STE_DMACTL, STE_DMACTL_TXDMA_UNSTALL);
			ste_wait(sc);
		}else{
			cur_tx->ste_ptr->ste_ctl = STE_TXCTL_DMAINTR | 1;
			sc->ste_tx_prev->ste_ptr->ste_next
				= cur_tx->ste_phys;
		}

		sc->ste_tx_prev = cur_tx;

		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
	 	 */
		BPF_MTAP(ifp, cur_tx->ste_mbuf);

		STE_INC(idx, STE_TX_LIST_CNT);
		ifp->if_timer = 5;
	}
	sc->ste_cdata.ste_tx_prod = idx;

	STE_UNLOCK(sc);

	return;
}

static void
ste_watchdog(ifp)
	struct ifnet		*ifp;
{
	struct ste_softc	*sc;

	sc = ifp->if_softc;
	STE_LOCK(sc);

	ifp->if_oerrors++;
	printf("ste%d: watchdog timeout\n", sc->ste_unit);

	ste_txeoc(sc);
	ste_txeof(sc);
	ste_rxeoc(sc);
	ste_rxeof(sc);
	ste_reset(sc);
	ste_init(sc);

	if (ifp->if_snd.ifq_head != NULL)
		ste_start(ifp);
	STE_UNLOCK(sc);

	return;
}

static void
ste_shutdown(dev)
	device_t		dev;
{
	struct ste_softc	*sc;

	sc = device_get_softc(dev);

	ste_stop(sc);

	return;
}
