/*
 * Copyright (c) 1997, 1998
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
 *	$Id: if_vr.c,v 1.14 1999/08/10 17:15:10 wpaul Exp $
 */

/*
 * VIA Rhine fast ethernet PCI NIC driver
 *
 * Supports various network adapters based on the VIA Rhine
 * and Rhine II PCI controllers, including the D-Link DFE530TX.
 * Datasheets are available at http://www.via.com.tw.
 *
 * Written by Bill Paul <wpaul@ctr.columbia.edu>
 * Electrical Engineering Department
 * Columbia University, New York City
 */

/*
 * The VIA Rhine controllers are similar in some respects to the
 * the DEC tulip chips, except less complicated. The controller
 * uses an MII bus and an external physical layer interface. The
 * receiver has a one entry perfect filter and a 64-bit hash table
 * multicast filter. Transmit and receive descriptors are similar
 * to the tulip.
 *
 * The Rhine has a serious flaw in its transmit DMA mechanism:
 * transmit buffers must be longword aligned. Unfortunately,
 * FreeBSD doesn't guarantee that mbufs will be filled in starting
 * at longword boundaries, so we have to do a buffer copy before
 * transmission.
 */

#include "bpf.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>

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

#define VR_USEIOSPACE

/* #define VR_BACKGROUND_AUTONEG */

#include <pci/if_vrreg.h>

#ifndef lint
static const char rcsid[] =
	"$Id: if_vr.c,v 1.14 1999/08/10 17:15:10 wpaul Exp $";
#endif

/*
 * Various supported device vendors/types and their names.
 */
static struct vr_type vr_devs[] = {
	{ VIA_VENDORID, VIA_DEVICEID_RHINE,
		"VIA VT3043 Rhine I 10/100BaseTX" },
	{ VIA_VENDORID, VIA_DEVICEID_RHINE_II,
		"VIA VT86C100A Rhine II 10/100BaseTX" },
	{ DELTA_VENDORID, DELTA_DEVICEID_RHINE_II,
		"Delta Electronics Rhine II 10/100BaseTX" },
	{ ADDTRON_VENDORID, ADDTRON_DEVICEID_RHINE_II,
		"Addtron Technology Rhine II 10/100BaseTX" },
	{ 0, 0, NULL }
};

/*
 * Various supported PHY vendors/types and their names. Note that
 * this driver will work with pretty much any MII-compliant PHY,
 * so failure to positively identify the chip is not a fatal error.
 */

static struct vr_type vr_phys[] = {
	{ TI_PHY_VENDORID, TI_PHY_10BT, "<TI ThunderLAN 10BT (internal)>" },
	{ TI_PHY_VENDORID, TI_PHY_100VGPMI, "<TI TNETE211 100VG Any-LAN>" },
	{ NS_PHY_VENDORID, NS_PHY_83840A, "<National Semiconductor DP83840A>"},
	{ LEVEL1_PHY_VENDORID, LEVEL1_PHY_LXT970, "<Level 1 LXT970>" }, 
	{ INTEL_PHY_VENDORID, INTEL_PHY_82555, "<Intel 82555>" },
	{ SEEQ_PHY_VENDORID, SEEQ_PHY_80220, "<SEEQ 80220>" },
	{ 0, 0, "<MII-compliant physical interface>" }
};

static int vr_probe		__P((device_t));
static int vr_attach		__P((device_t));
static int vr_detach		__P((device_t));

static int vr_newbuf		__P((struct vr_softc *,
					struct vr_chain_onefrag *,
					struct mbuf *));
static int vr_encap		__P((struct vr_softc *, struct vr_chain *,
						struct mbuf * ));

static void vr_rxeof		__P((struct vr_softc *));
static void vr_rxeoc		__P((struct vr_softc *));
static void vr_txeof		__P((struct vr_softc *));
static void vr_txeoc		__P((struct vr_softc *));
static void vr_intr		__P((void *));
static void vr_start		__P((struct ifnet *));
static int vr_ioctl		__P((struct ifnet *, u_long, caddr_t));
static void vr_init		__P((void *));
static void vr_stop		__P((struct vr_softc *));
static void vr_watchdog		__P((struct ifnet *));
static void vr_shutdown		__P((device_t));
static int vr_ifmedia_upd	__P((struct ifnet *));
static void vr_ifmedia_sts	__P((struct ifnet *, struct ifmediareq *));

static void vr_mii_sync		__P((struct vr_softc *));
static void vr_mii_send		__P((struct vr_softc *, u_int32_t, int));
static int vr_mii_readreg	__P((struct vr_softc *, struct vr_mii_frame *));
static int vr_mii_writereg	__P((struct vr_softc *, struct vr_mii_frame *));
static u_int16_t vr_phy_readreg	__P((struct vr_softc *, int));
static void vr_phy_writereg	__P((struct vr_softc *, u_int16_t, u_int16_t));

static void vr_autoneg_xmit	__P((struct vr_softc *));
static void vr_autoneg_mii	__P((struct vr_softc *, int, int));
static void vr_setmode_mii	__P((struct vr_softc *, int));
static void vr_getmode_mii	__P((struct vr_softc *));
static void vr_setcfg		__P((struct vr_softc *, u_int16_t));
static u_int8_t vr_calchash	__P((u_int8_t *));
static void vr_setmulti		__P((struct vr_softc *));
static void vr_reset		__P((struct vr_softc *));
static int vr_list_rx_init	__P((struct vr_softc *));
static int vr_list_tx_init	__P((struct vr_softc *));

#ifdef VR_USEIOSPACE
#define VR_RES			SYS_RES_IOPORT
#define VR_RID			VR_PCI_LOIO
#else
#define VR_RES			SYS_RES_MEMORY
#define VR_RID			VR_PCI_LOMEM
#endif

static device_method_t vr_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		vr_probe),
	DEVMETHOD(device_attach,	vr_attach),
	DEVMETHOD(device_detach, 	vr_detach),
	DEVMETHOD(device_shutdown,	vr_shutdown),
	{ 0, 0 }
};

static driver_t vr_driver = {
	"vr",
	vr_methods,
	sizeof(struct vr_softc)
};

static devclass_t vr_devclass;

DRIVER_MODULE(vr, pci, vr_driver, vr_devclass, 0, 0);

#define VR_SETBIT(sc, reg, x)				\
	CSR_WRITE_1(sc, reg,				\
		CSR_READ_1(sc, reg) | x)

#define VR_CLRBIT(sc, reg, x)				\
	CSR_WRITE_1(sc, reg,				\
		CSR_READ_1(sc, reg) & ~x)

#define VR_SETBIT16(sc, reg, x)				\
	CSR_WRITE_2(sc, reg,				\
		CSR_READ_2(sc, reg) | x)

#define VR_CLRBIT16(sc, reg, x)				\
	CSR_WRITE_2(sc, reg,				\
		CSR_READ_2(sc, reg) & ~x)

#define VR_SETBIT32(sc, reg, x)				\
	CSR_WRITE_4(sc, reg,				\
		CSR_READ_4(sc, reg) | x)

#define VR_CLRBIT32(sc, reg, x)				\
	CSR_WRITE_4(sc, reg,				\
		CSR_READ_4(sc, reg) & ~x)

#define SIO_SET(x)					\
	CSR_WRITE_1(sc, VR_MIICMD,			\
		CSR_READ_1(sc, VR_MIICMD) | x)

#define SIO_CLR(x)					\
	CSR_WRITE_1(sc, VR_MIICMD,			\
		CSR_READ_1(sc, VR_MIICMD) & ~x)

/*
 * Sync the PHYs by setting data bit and strobing the clock 32 times.
 */
static void vr_mii_sync(sc)
	struct vr_softc		*sc;
{
	register int		i;

	SIO_SET(VR_MIICMD_DIR|VR_MIICMD_DATAIN);

	for (i = 0; i < 32; i++) {
		SIO_SET(VR_MIICMD_CLK);
		DELAY(1);
		SIO_CLR(VR_MIICMD_CLK);
		DELAY(1);
	}

	return;
}

/*
 * Clock a series of bits through the MII.
 */
static void vr_mii_send(sc, bits, cnt)
	struct vr_softc		*sc;
	u_int32_t		bits;
	int			cnt;
{
	int			i;

	SIO_CLR(VR_MIICMD_CLK);

	for (i = (0x1 << (cnt - 1)); i; i >>= 1) {
                if (bits & i) {
			SIO_SET(VR_MIICMD_DATAIN);
                } else {
			SIO_CLR(VR_MIICMD_DATAIN);
                }
		DELAY(1);
		SIO_CLR(VR_MIICMD_CLK);
		DELAY(1);
		SIO_SET(VR_MIICMD_CLK);
	}
}

/*
 * Read an PHY register through the MII.
 */
static int vr_mii_readreg(sc, frame)
	struct vr_softc		*sc;
	struct vr_mii_frame	*frame;
	
{
	int			i, ack, s;

	s = splimp();

	/*
	 * Set up frame for RX.
	 */
	frame->mii_stdelim = VR_MII_STARTDELIM;
	frame->mii_opcode = VR_MII_READOP;
	frame->mii_turnaround = 0;
	frame->mii_data = 0;
	
	CSR_WRITE_1(sc, VR_MIICMD, 0);
	VR_SETBIT(sc, VR_MIICMD, VR_MIICMD_DIRECTPGM);

	/*
 	 * Turn on data xmit.
	 */
	SIO_SET(VR_MIICMD_DIR);

	vr_mii_sync(sc);

	/*
	 * Send command/address info.
	 */
	vr_mii_send(sc, frame->mii_stdelim, 2);
	vr_mii_send(sc, frame->mii_opcode, 2);
	vr_mii_send(sc, frame->mii_phyaddr, 5);
	vr_mii_send(sc, frame->mii_regaddr, 5);

	/* Idle bit */
	SIO_CLR((VR_MIICMD_CLK|VR_MIICMD_DATAIN));
	DELAY(1);
	SIO_SET(VR_MIICMD_CLK);
	DELAY(1);

	/* Turn off xmit. */
	SIO_CLR(VR_MIICMD_DIR);

	/* Check for ack */
	SIO_CLR(VR_MIICMD_CLK);
	DELAY(1);
	SIO_SET(VR_MIICMD_CLK);
	DELAY(1);
	ack = CSR_READ_4(sc, VR_MIICMD) & VR_MIICMD_DATAOUT;

	/*
	 * Now try reading data bits. If the ack failed, we still
	 * need to clock through 16 cycles to keep the PHY(s) in sync.
	 */
	if (ack) {
		for(i = 0; i < 16; i++) {
			SIO_CLR(VR_MIICMD_CLK);
			DELAY(1);
			SIO_SET(VR_MIICMD_CLK);
			DELAY(1);
		}
		goto fail;
	}

	for (i = 0x8000; i; i >>= 1) {
		SIO_CLR(VR_MIICMD_CLK);
		DELAY(1);
		if (!ack) {
			if (CSR_READ_4(sc, VR_MIICMD) & VR_MIICMD_DATAOUT)
				frame->mii_data |= i;
			DELAY(1);
		}
		SIO_SET(VR_MIICMD_CLK);
		DELAY(1);
	}

fail:

	SIO_CLR(VR_MIICMD_CLK);
	DELAY(1);
	SIO_SET(VR_MIICMD_CLK);
	DELAY(1);

	splx(s);

	if (ack)
		return(1);
	return(0);
}

/*
 * Write to a PHY register through the MII.
 */
static int vr_mii_writereg(sc, frame)
	struct vr_softc		*sc;
	struct vr_mii_frame	*frame;
	
{
	int			s;

	s = splimp();

	CSR_WRITE_1(sc, VR_MIICMD, 0);
	VR_SETBIT(sc, VR_MIICMD, VR_MIICMD_DIRECTPGM);

	/*
	 * Set up frame for TX.
	 */

	frame->mii_stdelim = VR_MII_STARTDELIM;
	frame->mii_opcode = VR_MII_WRITEOP;
	frame->mii_turnaround = VR_MII_TURNAROUND;
	
	/*
 	 * Turn on data output.
	 */
	SIO_SET(VR_MIICMD_DIR);

	vr_mii_sync(sc);

	vr_mii_send(sc, frame->mii_stdelim, 2);
	vr_mii_send(sc, frame->mii_opcode, 2);
	vr_mii_send(sc, frame->mii_phyaddr, 5);
	vr_mii_send(sc, frame->mii_regaddr, 5);
	vr_mii_send(sc, frame->mii_turnaround, 2);
	vr_mii_send(sc, frame->mii_data, 16);

	/* Idle bit. */
	SIO_SET(VR_MIICMD_CLK);
	DELAY(1);
	SIO_CLR(VR_MIICMD_CLK);
	DELAY(1);

	/*
	 * Turn off xmit.
	 */
	SIO_CLR(VR_MIICMD_DIR);

	splx(s);

	return(0);
}

static u_int16_t vr_phy_readreg(sc, reg)
	struct vr_softc		*sc;
	int			reg;
{
	struct vr_mii_frame	frame;

	bzero((char *)&frame, sizeof(frame));

	frame.mii_phyaddr = sc->vr_phy_addr;
	frame.mii_regaddr = reg;
	vr_mii_readreg(sc, &frame);

	return(frame.mii_data);
}

static void vr_phy_writereg(sc, reg, data)
	struct vr_softc		*sc;
	u_int16_t		reg;
	u_int16_t		data;
{
	struct vr_mii_frame	frame;

	bzero((char *)&frame, sizeof(frame));

	frame.mii_phyaddr = sc->vr_phy_addr;
	frame.mii_regaddr = reg;
	frame.mii_data = data;

	vr_mii_writereg(sc, &frame);

	return;
}

/*
 * Calculate CRC of a multicast group address, return the lower 6 bits.
 */
static u_int8_t vr_calchash(addr)
	u_int8_t		*addr;
{
	u_int32_t		crc, carry;
	int			i, j;
	u_int8_t		c;

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

	/* return the filter bit position */
	return((crc >> 26) & 0x0000003F);
}

/*
 * Program the 64-bit multicast hash filter.
 */
static void vr_setmulti(sc)
	struct vr_softc		*sc;
{
	struct ifnet		*ifp;
	int			h = 0;
	u_int32_t		hashes[2] = { 0, 0 };
	struct ifmultiaddr	*ifma;
	u_int8_t		rxfilt;
	int			mcnt = 0;

	ifp = &sc->arpcom.ac_if;

	rxfilt = CSR_READ_1(sc, VR_RXCFG);

	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
		rxfilt |= VR_RXCFG_RX_MULTI;
		CSR_WRITE_1(sc, VR_RXCFG, rxfilt);
		CSR_WRITE_4(sc, VR_MAR0, 0xFFFFFFFF);
		CSR_WRITE_4(sc, VR_MAR1, 0xFFFFFFFF);
		return;
	}

	/* first, zot all the existing hash bits */
	CSR_WRITE_4(sc, VR_MAR0, 0);
	CSR_WRITE_4(sc, VR_MAR1, 0);

	/* now program new ones */
	for (ifma = ifp->if_multiaddrs.lh_first; ifma != NULL;
				ifma = ifma->ifma_link.le_next) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		h = vr_calchash(LLADDR((struct sockaddr_dl *)ifma->ifma_addr));
		if (h < 32)
			hashes[0] |= (1 << h);
		else
			hashes[1] |= (1 << (h - 32));
		mcnt++;
	}

	if (mcnt)
		rxfilt |= VR_RXCFG_RX_MULTI;
	else
		rxfilt &= ~VR_RXCFG_RX_MULTI;

	CSR_WRITE_4(sc, VR_MAR0, hashes[0]);
	CSR_WRITE_4(sc, VR_MAR1, hashes[1]);
	CSR_WRITE_1(sc, VR_RXCFG, rxfilt);

	return;
}

/*
 * Initiate an autonegotiation session.
 */
static void vr_autoneg_xmit(sc)
	struct vr_softc		*sc;
{
	u_int16_t		phy_sts;

	vr_phy_writereg(sc, PHY_BMCR, PHY_BMCR_RESET);
	DELAY(500);
	while(vr_phy_readreg(sc, PHY_BMCR)
			& PHY_BMCR_RESET);

	phy_sts = vr_phy_readreg(sc, PHY_BMCR);
	phy_sts |= PHY_BMCR_AUTONEGENBL|PHY_BMCR_AUTONEGRSTR;
	vr_phy_writereg(sc, PHY_BMCR, phy_sts);

	return;
}

/*
 * Invoke autonegotiation on a PHY.
 */
static void vr_autoneg_mii(sc, flag, verbose)
	struct vr_softc		*sc;
	int			flag;
	int			verbose;
{
	u_int16_t		phy_sts = 0, media, advert, ability;
	struct ifnet		*ifp;
	struct ifmedia		*ifm;

	ifm = &sc->ifmedia;
	ifp = &sc->arpcom.ac_if;

	ifm->ifm_media = IFM_ETHER | IFM_AUTO;

	/*
	 * The 100baseT4 PHY on the 3c905-T4 has the 'autoneg supported'
	 * bit cleared in the status register, but has the 'autoneg enabled'
	 * bit set in the control register. This is a contradiction, and
	 * I'm not sure how to handle it. If you want to force an attempt
	 * to autoneg for 100baseT4 PHYs, #define FORCE_AUTONEG_TFOUR
	 * and see what happens.
	 */
#ifndef FORCE_AUTONEG_TFOUR
	/*
	 * First, see if autoneg is supported. If not, there's
	 * no point in continuing.
	 */
	phy_sts = vr_phy_readreg(sc, PHY_BMSR);
	if (!(phy_sts & PHY_BMSR_CANAUTONEG)) {
		if (verbose)
			printf("vr%d: autonegotiation not supported\n",
							sc->vr_unit);
		ifm->ifm_media = IFM_ETHER|IFM_10_T|IFM_HDX;	
		return;
	}
#endif

	switch (flag) {
	case VR_FLAG_FORCEDELAY:
		/*
	 	 * XXX Never use this option anywhere but in the probe
	 	 * routine: making the kernel stop dead in its tracks
 		 * for three whole seconds after we've gone multi-user
		 * is really bad manners.
	 	 */
		vr_autoneg_xmit(sc);
		DELAY(5000000);
		break;
	case VR_FLAG_SCHEDDELAY:
		/*
		 * Wait for the transmitter to go idle before starting
		 * an autoneg session, otherwise vr_start() may clobber
	 	 * our timeout, and we don't want to allow transmission
		 * during an autoneg session since that can screw it up.
	 	 */
		if (sc->vr_cdata.vr_tx_head != NULL) {
			sc->vr_want_auto = 1;
			return;
		}
		vr_autoneg_xmit(sc);
		ifp->if_timer = 5;
		sc->vr_autoneg = 1;
		sc->vr_want_auto = 0;
		return;
		break;
	case VR_FLAG_DELAYTIMEO:
		ifp->if_timer = 0;
		sc->vr_autoneg = 0;
		break;
	default:
		printf("vr%d: invalid autoneg flag: %d\n", sc->vr_unit, flag);
		return;
	}

	if (vr_phy_readreg(sc, PHY_BMSR) & PHY_BMSR_AUTONEGCOMP) {
		if (verbose)
			printf("vr%d: autoneg complete, ", sc->vr_unit);
		phy_sts = vr_phy_readreg(sc, PHY_BMSR);
	} else {
		if (verbose)
			printf("vr%d: autoneg not complete, ", sc->vr_unit);
	}

	media = vr_phy_readreg(sc, PHY_BMCR);

	/* Link is good. Report modes and set duplex mode. */
	if (vr_phy_readreg(sc, PHY_BMSR) & PHY_BMSR_LINKSTAT) {
		if (verbose)
			printf("link status good ");
		advert = vr_phy_readreg(sc, PHY_ANAR);
		ability = vr_phy_readreg(sc, PHY_LPAR);

		if (advert & PHY_ANAR_100BT4 && ability & PHY_ANAR_100BT4) {
			ifm->ifm_media = IFM_ETHER|IFM_100_T4;
			media |= PHY_BMCR_SPEEDSEL;
			media &= ~PHY_BMCR_DUPLEX;
			printf("(100baseT4)\n");
		} else if (advert & PHY_ANAR_100BTXFULL &&
			ability & PHY_ANAR_100BTXFULL) {
			ifm->ifm_media = IFM_ETHER|IFM_100_TX|IFM_FDX;
			media |= PHY_BMCR_SPEEDSEL;
			media |= PHY_BMCR_DUPLEX;
			printf("(full-duplex, 100Mbps)\n");
		} else if (advert & PHY_ANAR_100BTXHALF &&
			ability & PHY_ANAR_100BTXHALF) {
			ifm->ifm_media = IFM_ETHER|IFM_100_TX|IFM_HDX;
			media |= PHY_BMCR_SPEEDSEL;
			media &= ~PHY_BMCR_DUPLEX;
			printf("(half-duplex, 100Mbps)\n");
		} else if (advert & PHY_ANAR_10BTFULL &&
			ability & PHY_ANAR_10BTFULL) {
			ifm->ifm_media = IFM_ETHER|IFM_10_T|IFM_FDX;
			media &= ~PHY_BMCR_SPEEDSEL;
			media |= PHY_BMCR_DUPLEX;
			printf("(full-duplex, 10Mbps)\n");
		} else {
			ifm->ifm_media = IFM_ETHER|IFM_10_T|IFM_HDX;
			media &= ~PHY_BMCR_SPEEDSEL;
			media &= ~PHY_BMCR_DUPLEX;
			printf("(half-duplex, 10Mbps)\n");
		}

		media &= ~PHY_BMCR_AUTONEGENBL;

		/* Set ASIC's duplex mode to match the PHY. */
		vr_setcfg(sc, media);
		vr_phy_writereg(sc, PHY_BMCR, media);
	} else {
		if (verbose)
			printf("no carrier\n");
	}

	vr_init(sc);

	if (sc->vr_tx_pend) {
		sc->vr_autoneg = 0;
		sc->vr_tx_pend = 0;
		vr_start(ifp);
	}

	return;
}

static void vr_getmode_mii(sc)
	struct vr_softc		*sc;
{
	u_int16_t		bmsr;
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;

	bmsr = vr_phy_readreg(sc, PHY_BMSR);
	if (bootverbose)
		printf("vr%d: PHY status word: %x\n", sc->vr_unit, bmsr);

	/* fallback */
	sc->ifmedia.ifm_media = IFM_ETHER|IFM_10_T|IFM_HDX;

	if (bmsr & PHY_BMSR_10BTHALF) {
		if (bootverbose)
			printf("vr%d: 10Mbps half-duplex mode supported\n",
								sc->vr_unit);
		ifmedia_add(&sc->ifmedia,
			IFM_ETHER|IFM_10_T|IFM_HDX, 0, NULL);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_10_T, 0, NULL);
	}

	if (bmsr & PHY_BMSR_10BTFULL) {
		if (bootverbose)
			printf("vr%d: 10Mbps full-duplex mode supported\n",
								sc->vr_unit);
		ifmedia_add(&sc->ifmedia,
			IFM_ETHER|IFM_10_T|IFM_FDX, 0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_10_T|IFM_FDX;
	}

	if (bmsr & PHY_BMSR_100BTXHALF) {
		if (bootverbose)
			printf("vr%d: 100Mbps half-duplex mode supported\n",
								sc->vr_unit);
		ifp->if_baudrate = 100000000;
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_100_TX, 0, NULL);
		ifmedia_add(&sc->ifmedia,
			IFM_ETHER|IFM_100_TX|IFM_HDX, 0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_100_TX|IFM_HDX;
	}

	if (bmsr & PHY_BMSR_100BTXFULL) {
		if (bootverbose)
			printf("vr%d: 100Mbps full-duplex mode supported\n",
								sc->vr_unit);
		ifp->if_baudrate = 100000000;
		ifmedia_add(&sc->ifmedia,
			IFM_ETHER|IFM_100_TX|IFM_FDX, 0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_100_TX|IFM_FDX;
	}

	/* Some also support 100BaseT4. */
	if (bmsr & PHY_BMSR_100BT4) {
		if (bootverbose)
			printf("vr%d: 100baseT4 mode supported\n", sc->vr_unit);
		ifp->if_baudrate = 100000000;
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_100_T4, 0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_100_T4;
#ifdef FORCE_AUTONEG_TFOUR
		if (bootverbose)
			printf("vr%d: forcing on autoneg support for BT4\n",
							 sc->vr_unit);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_AUTO, 0 NULL):
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_AUTO;
#endif
	}

	if (bmsr & PHY_BMSR_CANAUTONEG) {
		if (bootverbose)
			printf("vr%d: autoneg supported\n", sc->vr_unit);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_AUTO, 0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_AUTO;
	}

	return;
}

/*
 * Set speed and duplex mode.
 */
static void vr_setmode_mii(sc, media)
	struct vr_softc		*sc;
	int			media;
{
	u_int16_t		bmcr;
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;

	/*
	 * If an autoneg session is in progress, stop it.
	 */
	if (sc->vr_autoneg) {
		printf("vr%d: canceling autoneg session\n", sc->vr_unit);
		ifp->if_timer = sc->vr_autoneg = sc->vr_want_auto = 0;
		bmcr = vr_phy_readreg(sc, PHY_BMCR);
		bmcr &= ~PHY_BMCR_AUTONEGENBL;
		vr_phy_writereg(sc, PHY_BMCR, bmcr);
	}

	printf("vr%d: selecting MII, ", sc->vr_unit);

	bmcr = vr_phy_readreg(sc, PHY_BMCR);

	bmcr &= ~(PHY_BMCR_AUTONEGENBL|PHY_BMCR_SPEEDSEL|
			PHY_BMCR_DUPLEX|PHY_BMCR_LOOPBK);

	if (IFM_SUBTYPE(media) == IFM_100_T4) {
		printf("100Mbps/T4, half-duplex\n");
		bmcr |= PHY_BMCR_SPEEDSEL;
		bmcr &= ~PHY_BMCR_DUPLEX;
	}

	if (IFM_SUBTYPE(media) == IFM_100_TX) {
		printf("100Mbps, ");
		bmcr |= PHY_BMCR_SPEEDSEL;
	}

	if (IFM_SUBTYPE(media) == IFM_10_T) {
		printf("10Mbps, ");
		bmcr &= ~PHY_BMCR_SPEEDSEL;
	}

	if ((media & IFM_GMASK) == IFM_FDX) {
		printf("full duplex\n");
		bmcr |= PHY_BMCR_DUPLEX;
	} else {
		printf("half duplex\n");
		bmcr &= ~PHY_BMCR_DUPLEX;
	}

	vr_setcfg(sc, bmcr);
	vr_phy_writereg(sc, PHY_BMCR, bmcr);

	return;
}

/*
 * In order to fiddle with the
 * 'full-duplex' and '100Mbps' bits in the netconfig register, we
 * first have to put the transmit and/or receive logic in the idle state.
 */
static void vr_setcfg(sc, bmcr)
	struct vr_softc		*sc;
	u_int16_t		bmcr;
{
	int			restart = 0;

	if (CSR_READ_2(sc, VR_COMMAND) & (VR_CMD_TX_ON|VR_CMD_RX_ON)) {
		restart = 1;
		VR_CLRBIT16(sc, VR_COMMAND, (VR_CMD_TX_ON|VR_CMD_RX_ON));
	}

	if (bmcr & PHY_BMCR_DUPLEX)
		VR_SETBIT16(sc, VR_COMMAND, VR_CMD_FULLDUPLEX);
	else
		VR_CLRBIT16(sc, VR_COMMAND, VR_CMD_FULLDUPLEX);

	if (restart)
		VR_SETBIT16(sc, VR_COMMAND, VR_CMD_TX_ON|VR_CMD_RX_ON);

	return;
}

static void vr_reset(sc)
	struct vr_softc		*sc;
{
	register int		i;

	VR_SETBIT16(sc, VR_COMMAND, VR_CMD_RESET);

	for (i = 0; i < VR_TIMEOUT; i++) {
		DELAY(10);
		if (!(CSR_READ_2(sc, VR_COMMAND) & VR_CMD_RESET))
			break;
	}
	if (i == VR_TIMEOUT)
		printf("vr%d: reset never completed!\n", sc->vr_unit);

	/* Wait a little while for the chip to get its brains in order. */
	DELAY(1000);

        return;
}

/*
 * Probe for a VIA Rhine chip. Check the PCI vendor and device
 * IDs against our list and return a device name if we find a match.
 */
static int vr_probe(dev)
	device_t		dev;
{
	struct vr_type		*t;

	t = vr_devs;

	while(t->vr_name != NULL) {
		if ((pci_get_vendor(dev) == t->vr_vid) &&
		    (pci_get_device(dev) == t->vr_did)) {
			device_set_desc(dev, t->vr_name);
			return(0);
		}
		t++;
	}

	return(ENXIO);
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
static int vr_attach(dev)
	device_t		dev;
{
	int			s, i;
	u_char			eaddr[ETHER_ADDR_LEN];
	u_int32_t		command;
	struct vr_softc		*sc;
	struct ifnet		*ifp;
	int			media = IFM_ETHER|IFM_100_TX|IFM_FDX;
	unsigned int		round;
	caddr_t			roundptr;
	struct vr_type		*p;
	u_int16_t		phy_vid, phy_did, phy_sts;
	int			unit, error = 0, rid;

	s = splimp();

	sc = device_get_softc(dev);
	unit = device_get_unit(dev);
	bzero(sc, sizeof(struct vr_softc *));

	/*
	 * Handle power management nonsense.
	 */

	command = pci_read_config(dev, VR_PCI_CAPID, 4) & 0x000000FF;
	if (command == 0x01) {

		command = pci_read_config(dev, VR_PCI_PWRMGMTCTRL, 4);
		if (command & VR_PSTATE_MASK) {
			u_int32_t		iobase, membase, irq;

			/* Save important PCI config data. */
			iobase = pci_read_config(dev, VR_PCI_LOIO, 4);
			membase = pci_read_config(dev, VR_PCI_LOMEM, 4);
			irq = pci_read_config(dev, VR_PCI_INTLINE, 4);

			/* Reset the power state. */
			printf("vr%d: chip is in D%d power mode "
			"-- setting to D0\n", unit, command & VR_PSTATE_MASK);
			command &= 0xFFFFFFFC;
			pci_write_config(dev, VR_PCI_PWRMGMTCTRL, command, 4);

			/* Restore PCI config data. */
			pci_write_config(dev, VR_PCI_LOIO, iobase, 4);
			pci_write_config(dev, VR_PCI_LOMEM, membase, 4);
			pci_write_config(dev, VR_PCI_INTLINE, irq, 4);
		}
	}

	/*
	 * Map control/status registers.
	 */
	command = pci_read_config(dev, PCI_COMMAND_STATUS_REG, 4);
	command |= (PCIM_CMD_PORTEN|PCIM_CMD_MEMEN|PCIM_CMD_BUSMASTEREN);
	pci_write_config(dev, PCI_COMMAND_STATUS_REG, command, 4);
	command = pci_read_config(dev, PCI_COMMAND_STATUS_REG, 4);

#ifdef VR_USEIOSPACE
	if (!(command & PCIM_CMD_PORTEN)) {
		printf("vr%d: failed to enable I/O ports!\n", unit);
		free(sc, M_DEVBUF);
		goto fail;
	}
#else
	if (!(command & PCIM_CMD_MEMEN)) {
		printf("vr%d: failed to enable memory mapping!\n", unit);
		goto fail;
	}
#endif

	rid = VR_RID;
	sc->vr_res = bus_alloc_resource(dev, VR_RES, &rid,
	    0, ~0, 1, RF_ACTIVE);

	if (sc->vr_res == NULL) {
		printf("vr%d: couldn't map ports/memory\n", unit);
		error = ENXIO;
		goto fail;
	}

	sc->vr_btag = rman_get_bustag(sc->vr_res);
	sc->vr_bhandle = rman_get_bushandle(sc->vr_res);

	/* Allocate interrupt */
	rid = 0;
	sc->vr_irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0, 1,
	    RF_SHAREABLE | RF_ACTIVE);

	if (sc->vr_irq == NULL) {
		printf("vr%d: couldn't map interrupt\n", unit);
		bus_release_resource(dev, VR_RES, VR_RID, sc->vr_res);
		error = ENXIO;
		goto fail;
	}

	error = bus_setup_intr(dev, sc->vr_irq, INTR_TYPE_NET,
	    vr_intr, sc, &sc->vr_intrhand);

	if (error) {
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->vr_irq);
		bus_release_resource(dev, VR_RES, VR_RID, sc->vr_res);
		printf("vr%d: couldn't set up irq\n", unit);
		goto fail;
	}

	/* Reset the adapter. */
	vr_reset(sc);

	/*
	 * Get station address. The way the Rhine chips work,
	 * you're not allowed to directly access the EEPROM once
	 * they've been programmed a special way. Consequently,
	 * we need to read the node address from the PAR0 and PAR1
	 * registers.
	 */
	VR_SETBIT(sc, VR_EECSR, VR_EECSR_LOAD);
	DELAY(200);
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		eaddr[i] = CSR_READ_1(sc, VR_PAR0 + i);

	/*
	 * A Rhine chip was detected. Inform the world.
	 */
	printf("vr%d: Ethernet address: %6D\n", unit, eaddr, ":");

	sc->vr_unit = unit;
	bcopy(eaddr, (char *)&sc->arpcom.ac_enaddr, ETHER_ADDR_LEN);

	sc->vr_ldata_ptr = malloc(sizeof(struct vr_list_data) + 8,
				M_DEVBUF, M_NOWAIT);
	if (sc->vr_ldata_ptr == NULL) {
		printf("vr%d: no memory for list buffers!\n", unit);
		bus_teardown_intr(dev, sc->vr_irq, sc->vr_intrhand);
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->vr_irq);
		bus_release_resource(dev, VR_RES, VR_RID, sc->vr_res);
		error = ENXIO;
		goto fail;
	}

	sc->vr_ldata = (struct vr_list_data *)sc->vr_ldata_ptr;
	round = (unsigned int)sc->vr_ldata_ptr & 0xF;
	roundptr = sc->vr_ldata_ptr;
	for (i = 0; i < 8; i++) {
		if (round % 8) {
			round++;
			roundptr++;
		} else
			break;
	}
	sc->vr_ldata = (struct vr_list_data *)roundptr;
	bzero(sc->vr_ldata, sizeof(struct vr_list_data));

	ifp = &sc->arpcom.ac_if;
	ifp->if_softc = sc;
	ifp->if_unit = unit;
	ifp->if_name = "vr";
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = vr_ioctl;
	ifp->if_output = ether_output;
	ifp->if_start = vr_start;
	ifp->if_watchdog = vr_watchdog;
	ifp->if_init = vr_init;
	ifp->if_baudrate = 10000000;
	ifp->if_snd.ifq_maxlen = VR_TX_LIST_CNT - 1;

	if (bootverbose)
		printf("vr%d: probing for a PHY\n", sc->vr_unit);
	for (i = VR_PHYADDR_MIN; i < VR_PHYADDR_MAX + 1; i++) {
		if (bootverbose)
			printf("vr%d: checking address: %d\n",
						sc->vr_unit, i);
		sc->vr_phy_addr = i;
		vr_phy_writereg(sc, PHY_BMCR, PHY_BMCR_RESET);
		DELAY(500);
		while(vr_phy_readreg(sc, PHY_BMCR)
				& PHY_BMCR_RESET);
		if ((phy_sts = vr_phy_readreg(sc, PHY_BMSR)))
			break;
	}
	if (phy_sts) {
		phy_vid = vr_phy_readreg(sc, PHY_VENID);
		phy_did = vr_phy_readreg(sc, PHY_DEVID);
		if (bootverbose)
			printf("vr%d: found PHY at address %d, ",
					sc->vr_unit, sc->vr_phy_addr);
		if (bootverbose)
			printf("vendor id: %x device id: %x\n",
				phy_vid, phy_did);
		p = vr_phys;
		while(p->vr_vid) {
			if (phy_vid == p->vr_vid &&
				(phy_did | 0x000F) == p->vr_did) {
				sc->vr_pinfo = p;
				break;
			}
			p++;
		}
		if (sc->vr_pinfo == NULL)
			sc->vr_pinfo = &vr_phys[PHY_UNKNOWN];
		if (bootverbose)
			printf("vr%d: PHY type: %s\n",
				sc->vr_unit, sc->vr_pinfo->vr_name);
	} else {
		printf("vr%d: MII without any phy!\n", sc->vr_unit);
		bus_teardown_intr(dev, sc->vr_irq, sc->vr_intrhand);
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->vr_irq);
		bus_release_resource(dev, VR_RES, VR_RID, sc->vr_res);
		free(sc->vr_ldata_ptr, M_DEVBUF);
		error = ENXIO;
		goto fail;
	}

	/*
	 * Do ifmedia setup.
	 */
	ifmedia_init(&sc->ifmedia, 0, vr_ifmedia_upd, vr_ifmedia_sts);

	vr_getmode_mii(sc);
	if (cold) {
		vr_autoneg_mii(sc, VR_FLAG_FORCEDELAY, 1);
		vr_stop(sc);
	} else {
		vr_init(sc);
		vr_autoneg_mii(sc, VR_FLAG_SCHEDDELAY, 1);
	}

	media = sc->ifmedia.ifm_media;

	ifmedia_set(&sc->ifmedia, media);

	/*
	 * Call MI attach routines.
	 */
	if_attach(ifp);
	ether_ifattach(ifp);

#if NBPF > 0
	bpfattach(ifp, DLT_EN10MB, sizeof(struct ether_header));
#endif

fail:
	splx(s);
	return(error);
}

static int vr_detach(dev)
	device_t		dev;
{
	struct vr_softc		*sc;
	struct ifnet		*ifp;
	int			s;

	s = splimp();

	sc = device_get_softc(dev);
	ifp = &sc->arpcom.ac_if;

	vr_stop(sc);
	if_detach(ifp);

	bus_teardown_intr(dev, sc->vr_irq, sc->vr_intrhand);
	bus_release_resource(dev, SYS_RES_IRQ, 0, sc->vr_irq);
	bus_release_resource(dev, VR_RES, VR_RID, sc->vr_res);

	free(sc->vr_ldata_ptr, M_DEVBUF);
	ifmedia_removeall(&sc->ifmedia);

	splx(s);

	return(0);
}

/*
 * Initialize the transmit descriptors.
 */
static int vr_list_tx_init(sc)
	struct vr_softc		*sc;
{
	struct vr_chain_data	*cd;
	struct vr_list_data	*ld;
	int			i;

	cd = &sc->vr_cdata;
	ld = sc->vr_ldata;
	for (i = 0; i < VR_TX_LIST_CNT; i++) {
		cd->vr_tx_chain[i].vr_ptr = &ld->vr_tx_list[i];
		if (i == (VR_TX_LIST_CNT - 1))
			cd->vr_tx_chain[i].vr_nextdesc = 
				&cd->vr_tx_chain[0];
		else
			cd->vr_tx_chain[i].vr_nextdesc =
				&cd->vr_tx_chain[i + 1];
	}

	cd->vr_tx_free = &cd->vr_tx_chain[0];
	cd->vr_tx_tail = cd->vr_tx_head = NULL;

	return(0);
}


/*
 * Initialize the RX descriptors and allocate mbufs for them. Note that
 * we arrange the descriptors in a closed ring, so that the last descriptor
 * points back to the first.
 */
static int vr_list_rx_init(sc)
	struct vr_softc		*sc;
{
	struct vr_chain_data	*cd;
	struct vr_list_data	*ld;
	int			i;

	cd = &sc->vr_cdata;
	ld = sc->vr_ldata;

	for (i = 0; i < VR_RX_LIST_CNT; i++) {
		cd->vr_rx_chain[i].vr_ptr =
			(struct vr_desc *)&ld->vr_rx_list[i];
		if (vr_newbuf(sc, &cd->vr_rx_chain[i], NULL) == ENOBUFS)
			return(ENOBUFS);
		if (i == (VR_RX_LIST_CNT - 1)) {
			cd->vr_rx_chain[i].vr_nextdesc =
					&cd->vr_rx_chain[0];
			ld->vr_rx_list[i].vr_next =
					vtophys(&ld->vr_rx_list[0]);
		} else {
			cd->vr_rx_chain[i].vr_nextdesc =
					&cd->vr_rx_chain[i + 1];
			ld->vr_rx_list[i].vr_next =
					vtophys(&ld->vr_rx_list[i + 1]);
		}
	}

	cd->vr_rx_head = &cd->vr_rx_chain[0];

	return(0);
}

/*
 * Initialize an RX descriptor and attach an MBUF cluster.
 * Note: the length fields are only 11 bits wide, which means the
 * largest size we can specify is 2047. This is important because
 * MCLBYTES is 2048, so we have to subtract one otherwise we'll
 * overflow the field and make a mess.
 */
static int vr_newbuf(sc, c, m)
	struct vr_softc		*sc;
	struct vr_chain_onefrag	*c;
	struct mbuf		*m;
{
	struct mbuf		*m_new = NULL;

	if (m == NULL) {
		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL) {
			printf("vr%d: no memory for rx list "
			    "-- packet dropped!\n", sc->vr_unit);
			return(ENOBUFS);
		}

		MCLGET(m_new, M_DONTWAIT);
		if (!(m_new->m_flags & M_EXT)) {
			printf("vr%d: no memory for rx list "
			    "-- packet dropped!\n", sc->vr_unit);
			m_freem(m_new);
			return(ENOBUFS);
		}
		m_new->m_len = m_new->m_pkthdr.len = MCLBYTES;
	} else {
		m_new = m;
		m_new->m_len = m_new->m_pkthdr.len = MCLBYTES;
		m_new->m_data = m_new->m_ext.ext_buf;
	}

	m_adj(m_new, sizeof(u_int64_t));

	c->vr_mbuf = m_new;
	c->vr_ptr->vr_status = VR_RXSTAT;
	c->vr_ptr->vr_data = vtophys(mtod(m_new, caddr_t));
	c->vr_ptr->vr_ctl = VR_RXCTL | VR_RXLEN;

	return(0);
}

/*
 * A frame has been uploaded: pass the resulting mbuf chain up to
 * the higher level protocols.
 */
static void vr_rxeof(sc)
	struct vr_softc		*sc;
{
        struct ether_header	*eh;
        struct mbuf		*m;
        struct ifnet		*ifp;
	struct vr_chain_onefrag	*cur_rx;
	int			total_len = 0;
	u_int32_t		rxstat;

	ifp = &sc->arpcom.ac_if;

	while(!((rxstat = sc->vr_cdata.vr_rx_head->vr_ptr->vr_status) &
							VR_RXSTAT_OWN)) {
		struct mbuf		*m0 = NULL;

		cur_rx = sc->vr_cdata.vr_rx_head;
		sc->vr_cdata.vr_rx_head = cur_rx->vr_nextdesc;
		m = cur_rx->vr_mbuf;

		/*
		 * If an error occurs, update stats, clear the
		 * status word and leave the mbuf cluster in place:
		 * it should simply get re-used next time this descriptor
	 	 * comes up in the ring.
		 */
		if (rxstat & VR_RXSTAT_RXERR) {
			ifp->if_ierrors++;
			printf("vr%d: rx error: ", sc->vr_unit);
			switch(rxstat & 0x000000FF) {
			case VR_RXSTAT_CRCERR:
				printf("crc error\n");
				break;
			case VR_RXSTAT_FRAMEALIGNERR:
				printf("frame alignment error\n");
				break;
			case VR_RXSTAT_FIFOOFLOW:
				printf("FIFO overflow\n");
				break;
			case VR_RXSTAT_GIANT:
				printf("received giant packet\n");
				break;
			case VR_RXSTAT_RUNT:
				printf("received runt packet\n");
				break;
			case VR_RXSTAT_BUSERR:
				printf("system bus error\n");
				break;
			case VR_RXSTAT_BUFFERR:
				printf("rx buffer error\n");
				break;
			default:
				printf("unknown rx error\n");
				break;
			}
			vr_newbuf(sc, cur_rx, m);
			continue;
		}

		/* No errors; receive the packet. */	
		total_len = VR_RXBYTES(cur_rx->vr_ptr->vr_status);

		/*
		 * XXX The VIA Rhine chip includes the CRC with every
		 * received frame, and there's no way to turn this
		 * behavior off (at least, I can't find anything in
	 	 * the manual that explains how to do it) so we have
		 * to trim off the CRC manually.
		 */
		total_len -= ETHER_CRC_LEN;

		m0 = m_devget(mtod(m, char *) - ETHER_ALIGN,
		    total_len + ETHER_ALIGN, 0, ifp, NULL);
		vr_newbuf(sc, cur_rx, m);
		if (m0 == NULL) {
			ifp->if_ierrors++;
			continue;
		}
		m_adj(m0, ETHER_ALIGN);
		m = m0;

		ifp->if_ipackets++;
		eh = mtod(m, struct ether_header *);

#if NBPF > 0
		/*
		 * Handle BPF listeners. Let the BPF user see the packet, but
		 * don't pass it up to the ether_input() layer unless it's
		 * a broadcast packet, multicast packet, matches our ethernet
		 * address or the interface is in promiscuous mode.
		 */
		if (ifp->if_bpf) {
			bpf_mtap(ifp, m);
			if (ifp->if_flags & IFF_PROMISC &&
				(bcmp(eh->ether_dhost, sc->arpcom.ac_enaddr,
						ETHER_ADDR_LEN) &&
					(eh->ether_dhost[0] & 1) == 0)) {
				m_freem(m);
				continue;
			}
		}
#endif
		/* Remove header from mbuf and pass it on. */
		m_adj(m, sizeof(struct ether_header));
		ether_input(ifp, eh, m);
	}

	return;
}

void vr_rxeoc(sc)
	struct vr_softc		*sc;
{

	vr_rxeof(sc);
	VR_CLRBIT16(sc, VR_COMMAND, VR_CMD_RX_ON);
	CSR_WRITE_4(sc, VR_RXADDR, vtophys(sc->vr_cdata.vr_rx_head->vr_ptr));
	VR_SETBIT16(sc, VR_COMMAND, VR_CMD_RX_ON);
	VR_SETBIT16(sc, VR_COMMAND, VR_CMD_RX_GO);

	return;
}

/*
 * A frame was downloaded to the chip. It's safe for us to clean up
 * the list buffers.
 */

static void vr_txeof(sc)
	struct vr_softc		*sc;
{
	struct vr_chain		*cur_tx;
	struct ifnet		*ifp;
	register struct mbuf	*n;

	ifp = &sc->arpcom.ac_if;

	/* Clear the timeout timer. */
	ifp->if_timer = 0;

	/* Sanity check. */
	if (sc->vr_cdata.vr_tx_head == NULL)
		return;

	/*
	 * Go through our tx list and free mbufs for those
	 * frames that have been transmitted.
	 */
	while(sc->vr_cdata.vr_tx_head->vr_mbuf != NULL) {
		u_int32_t		txstat;

		cur_tx = sc->vr_cdata.vr_tx_head;
		txstat = cur_tx->vr_ptr->vr_status;

		if (txstat & VR_TXSTAT_OWN)
			break;

		if (txstat & VR_TXSTAT_ERRSUM) {
			ifp->if_oerrors++;
			if (txstat & VR_TXSTAT_DEFER)
				ifp->if_collisions++;
			if (txstat & VR_TXSTAT_LATECOLL)
				ifp->if_collisions++;
		}

		ifp->if_collisions +=(txstat & VR_TXSTAT_COLLCNT) >> 3;

		ifp->if_opackets++;
        	MFREE(cur_tx->vr_mbuf, n);
		cur_tx->vr_mbuf = NULL;

		if (sc->vr_cdata.vr_tx_head == sc->vr_cdata.vr_tx_tail) {
			sc->vr_cdata.vr_tx_head = NULL;
			sc->vr_cdata.vr_tx_tail = NULL;
			break;
		}

		sc->vr_cdata.vr_tx_head = cur_tx->vr_nextdesc;
	}

	return;
}

/*
 * TX 'end of channel' interrupt handler.
 */
static void vr_txeoc(sc)
	struct vr_softc		*sc;
{
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;

	ifp->if_timer = 0;

	if (sc->vr_cdata.vr_tx_head == NULL) {
		ifp->if_flags &= ~IFF_OACTIVE;
		sc->vr_cdata.vr_tx_tail = NULL;
		if (sc->vr_want_auto)
			vr_autoneg_mii(sc, VR_FLAG_SCHEDDELAY, 1);
	}

	return;
}

static void vr_intr(arg)
	void			*arg;
{
	struct vr_softc		*sc;
	struct ifnet		*ifp;
	u_int16_t		status;

	sc = arg;
	ifp = &sc->arpcom.ac_if;

	/* Supress unwanted interrupts. */
	if (!(ifp->if_flags & IFF_UP)) {
		vr_stop(sc);
		return;
	}

	/* Disable interrupts. */
	CSR_WRITE_2(sc, VR_IMR, 0x0000);

	for (;;) {

		status = CSR_READ_2(sc, VR_ISR);
		if (status)
			CSR_WRITE_2(sc, VR_ISR, status);

		if ((status & VR_INTRS) == 0)
			break;

		if (status & VR_ISR_RX_OK)
			vr_rxeof(sc);

		if ((status & VR_ISR_RX_ERR) || (status & VR_ISR_RX_NOBUF) ||
		    (status & VR_ISR_RX_NOBUF) || (status & VR_ISR_RX_OFLOW) ||
		    (status & VR_ISR_RX_DROPPED)) {
			vr_rxeof(sc);
			vr_rxeoc(sc);
		}

		if (status & VR_ISR_TX_OK) {
			vr_txeof(sc);
			vr_txeoc(sc);
		}

		if ((status & VR_ISR_TX_UNDERRUN)||(status & VR_ISR_TX_ABRT)){ 
			ifp->if_oerrors++;
			vr_txeof(sc);
			if (sc->vr_cdata.vr_tx_head != NULL) {
				VR_SETBIT16(sc, VR_COMMAND, VR_CMD_TX_ON);
				VR_SETBIT16(sc, VR_COMMAND, VR_CMD_TX_GO);
			}
		}

		if (status & VR_ISR_BUSERR) {
			vr_reset(sc);
			vr_init(sc);
		}
	}

	/* Re-enable interrupts. */
	CSR_WRITE_2(sc, VR_IMR, VR_INTRS);

	if (ifp->if_snd.ifq_head != NULL) {
		vr_start(ifp);
	}

	return;
}

/*
 * Encapsulate an mbuf chain in a descriptor by coupling the mbuf data
 * pointers to the fragment pointers.
 */
static int vr_encap(sc, c, m_head)
	struct vr_softc		*sc;
	struct vr_chain		*c;
	struct mbuf		*m_head;
{
	int			frag = 0;
	struct vr_desc		*f = NULL;
	int			total_len;
	struct mbuf		*m;

	m = m_head;
	total_len = 0;

	/*
	 * The VIA Rhine wants packet buffers to be longword
	 * aligned, but very often our mbufs aren't. Rather than
	 * waste time trying to decide when to copy and when not
	 * to copy, just do it all the time.
	 */
	if (m != NULL) {
		struct mbuf		*m_new = NULL;

		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL) {
			printf("vr%d: no memory for tx list", sc->vr_unit);
			return(1);
		}
		if (m_head->m_pkthdr.len > MHLEN) {
			MCLGET(m_new, M_DONTWAIT);
			if (!(m_new->m_flags & M_EXT)) {
				m_freem(m_new);
				printf("vr%d: no memory for tx list",
						sc->vr_unit);
				return(1);
			}
		}
		m_copydata(m_head, 0, m_head->m_pkthdr.len,	
					mtod(m_new, caddr_t));
		m_new->m_pkthdr.len = m_new->m_len = m_head->m_pkthdr.len;
		m_freem(m_head);
		m_head = m_new;
		/*
		 * The Rhine chip doesn't auto-pad, so we have to make
		 * sure to pad short frames out to the minimum frame length
		 * ourselves.
		 */
		if (m_head->m_len < VR_MIN_FRAMELEN) {
			m_new->m_pkthdr.len += VR_MIN_FRAMELEN - m_new->m_len;
			m_new->m_len = m_new->m_pkthdr.len;
		}
		f = c->vr_ptr;
		f->vr_data = vtophys(mtod(m_new, caddr_t));
		f->vr_ctl = total_len = m_new->m_len;
		f->vr_ctl |= VR_TXCTL_TLINK|VR_TXCTL_FIRSTFRAG;
		f->vr_status = 0;
		frag = 1;
	}

	c->vr_mbuf = m_head;
	c->vr_ptr->vr_ctl |= VR_TXCTL_LASTFRAG|VR_TXCTL_FINT;
	c->vr_ptr->vr_next = vtophys(c->vr_nextdesc->vr_ptr);

	return(0);
}

/*
 * Main transmit routine. To avoid having to do mbuf copies, we put pointers
 * to the mbuf data regions directly in the transmit lists. We also save a
 * copy of the pointers since the transmit list fragment pointers are
 * physical addresses.
 */

static void vr_start(ifp)
	struct ifnet		*ifp;
{
	struct vr_softc		*sc;
	struct mbuf		*m_head = NULL;
	struct vr_chain		*cur_tx = NULL, *start_tx;

	sc = ifp->if_softc;

	if (sc->vr_autoneg) {
		sc->vr_tx_pend = 1;
		return;
	}

	/*
	 * Check for an available queue slot. If there are none,
	 * punt.
	 */
	if (sc->vr_cdata.vr_tx_free->vr_mbuf != NULL) {
		ifp->if_flags |= IFF_OACTIVE;
		return;
	}

	start_tx = sc->vr_cdata.vr_tx_free;

	while(sc->vr_cdata.vr_tx_free->vr_mbuf == NULL) {
		IF_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		/* Pick a descriptor off the free list. */
		cur_tx = sc->vr_cdata.vr_tx_free;
		sc->vr_cdata.vr_tx_free = cur_tx->vr_nextdesc;

		/* Pack the data into the descriptor. */
		vr_encap(sc, cur_tx, m_head);

		if (cur_tx != start_tx)
			VR_TXOWN(cur_tx) = VR_TXSTAT_OWN;

#if NBPF > 0
		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		if (ifp->if_bpf)
			bpf_mtap(ifp, cur_tx->vr_mbuf);
#endif
		VR_TXOWN(cur_tx) = VR_TXSTAT_OWN;
		VR_SETBIT16(sc, VR_COMMAND, VR_CMD_TX_ON|VR_CMD_TX_GO);
	}

	/*
	 * If there are no frames queued, bail.
	 */
	if (cur_tx == NULL)
		return;

	sc->vr_cdata.vr_tx_tail = cur_tx;

	if (sc->vr_cdata.vr_tx_head == NULL)
		sc->vr_cdata.vr_tx_head = start_tx;

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;

	return;
}

static void vr_init(xsc)
	void			*xsc;
{
	struct vr_softc		*sc = xsc;
	struct ifnet		*ifp = &sc->arpcom.ac_if;
	u_int16_t		phy_bmcr = 0;
	int			s;

	if (sc->vr_autoneg)
		return;

	s = splimp();

	if (sc->vr_pinfo != NULL)
		phy_bmcr = vr_phy_readreg(sc, PHY_BMCR);

	/*
	 * Cancel pending I/O and free all RX/TX buffers.
	 */
	vr_stop(sc);
	vr_reset(sc);

	VR_CLRBIT(sc, VR_RXCFG, VR_RXCFG_RX_THRESH);
	VR_SETBIT(sc, VR_RXCFG, VR_RXTHRESH_STORENFWD);

	VR_CLRBIT(sc, VR_TXCFG, VR_TXCFG_TX_THRESH);
	VR_SETBIT(sc, VR_TXCFG, VR_TXTHRESH_STORENFWD);

	/* Init circular RX list. */
	if (vr_list_rx_init(sc) == ENOBUFS) {
		printf("vr%d: initialization failed: no "
			"memory for rx buffers\n", sc->vr_unit);
		vr_stop(sc);
		(void)splx(s);
		return;
	}

	/*
	 * Init tx descriptors.
	 */
	vr_list_tx_init(sc);

	/* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC)
		VR_SETBIT(sc, VR_RXCFG, VR_RXCFG_RX_PROMISC);
	else
		VR_CLRBIT(sc, VR_RXCFG, VR_RXCFG_RX_PROMISC);

	/* Set capture broadcast bit to capture broadcast frames. */
	if (ifp->if_flags & IFF_BROADCAST)
		VR_SETBIT(sc, VR_RXCFG, VR_RXCFG_RX_BROAD);
	else
		VR_CLRBIT(sc, VR_RXCFG, VR_RXCFG_RX_BROAD);

	/*
	 * Program the multicast filter, if necessary.
	 */
	vr_setmulti(sc);

	/*
	 * Load the address of the RX list.
	 */
	CSR_WRITE_4(sc, VR_RXADDR, vtophys(sc->vr_cdata.vr_rx_head->vr_ptr));

	/* Enable receiver and transmitter. */
	CSR_WRITE_2(sc, VR_COMMAND, VR_CMD_TX_NOPOLL|VR_CMD_START|
				    VR_CMD_TX_ON|VR_CMD_RX_ON|
				    VR_CMD_RX_GO);

	vr_setcfg(sc, vr_phy_readreg(sc, PHY_BMCR));

	CSR_WRITE_4(sc, VR_TXADDR, vtophys(&sc->vr_ldata->vr_tx_list[0]));

	/*
	 * Enable interrupts.
	 */
	CSR_WRITE_2(sc, VR_ISR, 0xFFFF);
	CSR_WRITE_2(sc, VR_IMR, VR_INTRS);

	/* Restore state of BMCR */
	if (sc->vr_pinfo != NULL)
		vr_phy_writereg(sc, PHY_BMCR, phy_bmcr);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	(void)splx(s);

	return;
}

/*
 * Set media options.
 */
static int vr_ifmedia_upd(ifp)
	struct ifnet		*ifp;
{
	struct vr_softc		*sc;
	struct ifmedia		*ifm;

	sc = ifp->if_softc;
	ifm = &sc->ifmedia;

	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
		return(EINVAL);

	if (IFM_SUBTYPE(ifm->ifm_media) == IFM_AUTO)
		vr_autoneg_mii(sc, VR_FLAG_SCHEDDELAY, 1);
	else
		vr_setmode_mii(sc, ifm->ifm_media);

	return(0);
}

/*
 * Report current media status.
 */
static void vr_ifmedia_sts(ifp, ifmr)
	struct ifnet		*ifp;
	struct ifmediareq	*ifmr;
{
	struct vr_softc		*sc;
	u_int16_t		advert = 0, ability = 0;

	sc = ifp->if_softc;

	ifmr->ifm_active = IFM_ETHER;

	if (!(vr_phy_readreg(sc, PHY_BMCR) & PHY_BMCR_AUTONEGENBL)) {
		if (vr_phy_readreg(sc, PHY_BMCR) & PHY_BMCR_SPEEDSEL)
			ifmr->ifm_active = IFM_ETHER|IFM_100_TX;
		else
			ifmr->ifm_active = IFM_ETHER|IFM_10_T;
		if (vr_phy_readreg(sc, PHY_BMCR) & PHY_BMCR_DUPLEX)
			ifmr->ifm_active |= IFM_FDX;
		else
			ifmr->ifm_active |= IFM_HDX;
		return;
	}

	ability = vr_phy_readreg(sc, PHY_LPAR);
	advert = vr_phy_readreg(sc, PHY_ANAR);
	if (advert & PHY_ANAR_100BT4 &&
		ability & PHY_ANAR_100BT4) {
		ifmr->ifm_active = IFM_ETHER|IFM_100_T4;
	} else if (advert & PHY_ANAR_100BTXFULL &&
		ability & PHY_ANAR_100BTXFULL) {
		ifmr->ifm_active = IFM_ETHER|IFM_100_TX|IFM_FDX;
	} else if (advert & PHY_ANAR_100BTXHALF &&
		ability & PHY_ANAR_100BTXHALF) {
		ifmr->ifm_active = IFM_ETHER|IFM_100_TX|IFM_HDX;
	} else if (advert & PHY_ANAR_10BTFULL &&
		ability & PHY_ANAR_10BTFULL) {
		ifmr->ifm_active = IFM_ETHER|IFM_10_T|IFM_FDX;
	} else if (advert & PHY_ANAR_10BTHALF &&
		ability & PHY_ANAR_10BTHALF) {
		ifmr->ifm_active = IFM_ETHER|IFM_10_T|IFM_HDX;
	}

	return;
}

static int vr_ioctl(ifp, command, data)
	struct ifnet		*ifp;
	u_long			command;
	caddr_t			data;
{
	struct vr_softc		*sc = ifp->if_softc;
	struct ifreq		*ifr = (struct ifreq *) data;
	int			s, error = 0;

	s = splimp();

	switch(command) {
	case SIOCSIFADDR:
	case SIOCGIFADDR:
	case SIOCSIFMTU:
		error = ether_ioctl(ifp, command, data);
		break;
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			vr_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				vr_stop(sc);
		}
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		vr_setmulti(sc);
		error = 0;
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->ifmedia, command);
		break;
	default:
		error = EINVAL;
		break;
	}

	(void)splx(s);

	return(error);
}

static void vr_watchdog(ifp)
	struct ifnet		*ifp;
{
	struct vr_softc		*sc;

	sc = ifp->if_softc;

	if (sc->vr_autoneg) {
		vr_autoneg_mii(sc, VR_FLAG_DELAYTIMEO, 1);
		if (!(ifp->if_flags & IFF_UP))
			vr_stop(sc);
		return;
	}

	ifp->if_oerrors++;
	printf("vr%d: watchdog timeout\n", sc->vr_unit);

	if (!(vr_phy_readreg(sc, PHY_BMSR) & PHY_BMSR_LINKSTAT))
		printf("vr%d: no carrier - transceiver cable problem?\n",
								sc->vr_unit);

	vr_stop(sc);
	vr_reset(sc);
	vr_init(sc);

	if (ifp->if_snd.ifq_head != NULL)
		vr_start(ifp);

	return;
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
static void vr_stop(sc)
	struct vr_softc		*sc;
{
	register int		i;
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;
	ifp->if_timer = 0;

	VR_SETBIT16(sc, VR_COMMAND, VR_CMD_STOP);
	VR_CLRBIT16(sc, VR_COMMAND, (VR_CMD_RX_ON|VR_CMD_TX_ON));
	CSR_WRITE_2(sc, VR_IMR, 0x0000);
	CSR_WRITE_4(sc, VR_TXADDR, 0x00000000);
	CSR_WRITE_4(sc, VR_RXADDR, 0x00000000);

	/*
	 * Free data in the RX lists.
	 */
	for (i = 0; i < VR_RX_LIST_CNT; i++) {
		if (sc->vr_cdata.vr_rx_chain[i].vr_mbuf != NULL) {
			m_freem(sc->vr_cdata.vr_rx_chain[i].vr_mbuf);
			sc->vr_cdata.vr_rx_chain[i].vr_mbuf = NULL;
		}
	}
	bzero((char *)&sc->vr_ldata->vr_rx_list,
		sizeof(sc->vr_ldata->vr_rx_list));

	/*
	 * Free the TX list buffers.
	 */
	for (i = 0; i < VR_TX_LIST_CNT; i++) {
		if (sc->vr_cdata.vr_tx_chain[i].vr_mbuf != NULL) {
			m_freem(sc->vr_cdata.vr_tx_chain[i].vr_mbuf);
			sc->vr_cdata.vr_tx_chain[i].vr_mbuf = NULL;
		}
	}

	bzero((char *)&sc->vr_ldata->vr_tx_list,
		sizeof(sc->vr_ldata->vr_tx_list));

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	return;
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
static void vr_shutdown(dev)
	device_t		dev;
{
	struct vr_softc		*sc;

	sc = device_get_softc(dev);

	vr_stop(sc);

	return;
}
