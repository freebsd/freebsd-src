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
 *	$Id: if_mx.c,v 1.20 1999/07/02 04:17:13 peter Exp $
 */

/*
 * Macronix PMAC fast ethernet PCI NIC driver
 *
 * Written by Bill Paul <wpaul@ctr.columbia.edu>
 * Electrical Engineering Department
 * Columbia University, New York City
 */

/*
 * The Macronix 98713, 98715 and 98725 chips are still more tulip clones.
 * The 98713 has an internal transceiver and an MII bus for external PHYs.
 * The other two chips have only the internal transceiver. All have
 * support for built-in autonegotiation. Additionally, there are 98713A
 * and 98715A chips which support power management. The 98725 chip
 * supports power management as well.
 *
 * Datasheets for the Macronix parts can be obtained from www.macronix.com.
 * Note however that the datasheets do not describe the TX and RX
 * descriptor structures or the setup frame format(s). For this, you should
 * obtain a DEC 21x4x datasheet from developer.intel.com. The Macronix
 * chips look to be fairly straightforward tulip clones, except for
 * the NWAY support.
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

#include <pci/pcireg.h>
#include <pci/pcivar.h>

#define MX_USEIOSPACE

/* #define MX_BACKGROUND_AUTONEG */

#include <pci/if_mxreg.h>

#ifndef lint
static const char rcsid[] =
	"$Id: if_mx.c,v 1.20 1999/07/02 04:17:13 peter Exp $";
#endif

/*
 * Various supported device vendors/types and their names.
 */
static struct mx_type mx_devs[] = {
	{ MX_VENDORID, MX_DEVICEID_98713,
		"Macronix 98713 10/100BaseTX" },
	{ MX_VENDORID, MX_DEVICEID_98713,
		"Macronix 98713A 10/100BaseTX" },
	{ CP_VENDORID, CP_DEVICEID_98713,
		"Compex RL100-TX 10/100BaseTX" },
	{ CP_VENDORID, CP_DEVICEID_98713,
		"Compex RL100-TX 10/100BaseTX" },
	{ MX_VENDORID, MX_DEVICEID_987x5,
		"Macronix 98715/98715A 10/100BaseTX" },
	{ MX_VENDORID, MX_DEVICEID_987x5,
		"Macronix 98725 10/100BaseTX" },
	{ PN_VENDORID, PN_DEVICEID_PNIC_II,
		"LC82C115 PNIC II 10/100BaseTX" },
	{ 0, 0, NULL }
};

/*
 * Various supported PHY vendors/types and their names. Note that
 * this driver will work with pretty much any MII-compliant PHY,
 * so failure to positively identify the chip is not a fatal error.
 */

static struct mx_type mx_phys[] = {
	{ TI_PHY_VENDORID, TI_PHY_10BT, "<TI ThunderLAN 10BT (internal)>" },
	{ TI_PHY_VENDORID, TI_PHY_100VGPMI, "<TI TNETE211 100VG Any-LAN>" },
	{ NS_PHY_VENDORID, NS_PHY_83840A, "<National Semiconductor DP83840A>"},
	{ LEVEL1_PHY_VENDORID, LEVEL1_PHY_LXT970, "<Level 1 LXT970>" }, 
	{ INTEL_PHY_VENDORID, INTEL_PHY_82555, "<Intel 82555>" },
	{ SEEQ_PHY_VENDORID, SEEQ_PHY_80220, "<SEEQ 80220>" },
	{ 0, 0, "<MII-compliant physical interface>" }
};

static unsigned long mx_count = 0;
static const char *mx_probe	__P((pcici_t, pcidi_t));
static void mx_attach		__P((pcici_t, int));
static struct mx_type *mx_devtype	__P((pcici_t, pcidi_t));
static int mx_newbuf		__P((struct mx_softc *,
						struct mx_chain_onefrag *));
static int mx_encap		__P((struct mx_softc *, struct mx_chain *,
						struct mbuf *));

static void mx_rxeof		__P((struct mx_softc *));
static void mx_rxeoc		__P((struct mx_softc *));
static void mx_txeof		__P((struct mx_softc *));
static void mx_txeoc		__P((struct mx_softc *));
static void mx_intr		__P((void *));
static void mx_start		__P((struct ifnet *));
static int mx_ioctl		__P((struct ifnet *, u_long, caddr_t));
static void mx_init		__P((void *));
static void mx_stop		__P((struct mx_softc *));
static void mx_watchdog		__P((struct ifnet *));
static void mx_shutdown		__P((int, void *));
static int mx_ifmedia_upd	__P((struct ifnet *));
static void mx_ifmedia_sts	__P((struct ifnet *, struct ifmediareq *));

static void mx_delay		__P((struct mx_softc *));
static void mx_eeprom_idle	__P((struct mx_softc *));
static void mx_eeprom_putbyte	__P((struct mx_softc *, int));
static void mx_eeprom_getword	__P((struct mx_softc *, int, u_int16_t *));
static void mx_read_eeprom	__P((struct mx_softc *, caddr_t, int,
							int, int));

static void mx_mii_writebit	__P((struct mx_softc *, int));
static int mx_mii_readbit	__P((struct mx_softc *));
static void mx_mii_sync		__P((struct mx_softc *));
static void mx_mii_send		__P((struct mx_softc *, u_int32_t, int));
static int mx_mii_readreg	__P((struct mx_softc *, struct mx_mii_frame *));
static int mx_mii_writereg	__P((struct mx_softc *, struct mx_mii_frame *));
static u_int16_t mx_phy_readreg	__P((struct mx_softc *, int));
static void mx_phy_writereg	__P((struct mx_softc *, int, int));

static void mx_autoneg_xmit	__P((struct mx_softc *));
static void mx_autoneg_mii	__P((struct mx_softc *, int, int));
static void mx_autoneg		__P((struct mx_softc *, int, int));
static void mx_setmode_mii	__P((struct mx_softc *, int));
static void mx_setmode		__P((struct mx_softc *, int, int));
static void mx_getmode_mii	__P((struct mx_softc *));
static void mx_setcfg		__P((struct mx_softc *, int));
static u_int32_t mx_calchash	__P((struct mx_softc *, caddr_t));
static void mx_setfilt		__P((struct mx_softc *));
static void mx_reset		__P((struct mx_softc *));
static int mx_list_rx_init	__P((struct mx_softc *));
static int mx_list_tx_init	__P((struct mx_softc *));

#define MX_SETBIT(sc, reg, x)				\
	CSR_WRITE_4(sc, reg,				\
		CSR_READ_4(sc, reg) | x)

#define MX_CLRBIT(sc, reg, x)				\
	CSR_WRITE_4(sc, reg,				\
		CSR_READ_4(sc, reg) & ~x)

#define SIO_SET(x)					\
	CSR_WRITE_4(sc, MX_SIO,				\
		CSR_READ_4(sc, MX_SIO) | x)

#define SIO_CLR(x)					\
	CSR_WRITE_4(sc, MX_SIO,				\
		CSR_READ_4(sc, MX_SIO) & ~x)

static void mx_delay(sc)
	struct mx_softc		*sc;
{
	int			idx;

	for (idx = (300 / 33) + 1; idx > 0; idx--)
		CSR_READ_4(sc, MX_BUSCTL);
}

static void mx_eeprom_idle(sc)
	struct mx_softc		*sc;
{
	register int		i;

	CSR_WRITE_4(sc, MX_SIO, MX_SIO_EESEL);
	mx_delay(sc);
	MX_SETBIT(sc, MX_SIO,  MX_SIO_ROMCTL_READ);
	mx_delay(sc);
	MX_SETBIT(sc, MX_SIO, MX_SIO_EE_CS);
	mx_delay(sc);
	MX_SETBIT(sc, MX_SIO, MX_SIO_EE_CLK);
	mx_delay(sc);

	for (i = 0; i < 25; i++) {
		MX_CLRBIT(sc, MX_SIO, MX_SIO_EE_CLK);
		mx_delay(sc);
		MX_SETBIT(sc, MX_SIO, MX_SIO_EE_CLK);
		mx_delay(sc);
	}

	MX_CLRBIT(sc, MX_SIO, MX_SIO_EE_CLK);
	mx_delay(sc);
	MX_CLRBIT(sc, MX_SIO, MX_SIO_EE_CS);
	mx_delay(sc);
	CSR_WRITE_4(sc, MX_SIO, 0x00000000);

	return;
}

/*
 * Send a read command and address to the EEPROM, check for ACK.
 */
static void mx_eeprom_putbyte(sc, addr)
	struct mx_softc		*sc;
	int			addr;
{
	register int		d, i;

	d = addr | MX_EECMD_READ;

	/*
	 * Feed in each bit and stobe the clock.
	 */
	for (i = 0x400; i; i >>= 1) {
		if (d & i) {
			SIO_SET(MX_SIO_EE_DATAIN);
		} else {
			SIO_CLR(MX_SIO_EE_DATAIN);
		}
		mx_delay(sc);
		SIO_SET(MX_SIO_EE_CLK);
		mx_delay(sc);
		SIO_CLR(MX_SIO_EE_CLK);
		mx_delay(sc);
	}

	return;
}

/*
 * Read a word of data stored in the EEPROM at address 'addr.'
 */
static void mx_eeprom_getword(sc, addr, dest)
	struct mx_softc		*sc;
	int			addr;
	u_int16_t		*dest;
{
	register int		i;
	u_int16_t		word = 0;

	/* Force EEPROM to idle state. */
	mx_eeprom_idle(sc);

	/* Enter EEPROM access mode. */
	CSR_WRITE_4(sc, MX_SIO, MX_SIO_EESEL);
	mx_delay(sc);
	MX_SETBIT(sc, MX_SIO,  MX_SIO_ROMCTL_READ);
	mx_delay(sc);
	MX_SETBIT(sc, MX_SIO, MX_SIO_EE_CS);
	mx_delay(sc);
	MX_SETBIT(sc, MX_SIO, MX_SIO_EE_CLK);
	mx_delay(sc);

	/*
	 * Send address of word we want to read.
	 */
	mx_eeprom_putbyte(sc, addr);

	/*
	 * Start reading bits from EEPROM.
	 */
	for (i = 0x8000; i; i >>= 1) {
		SIO_SET(MX_SIO_EE_CLK);
		mx_delay(sc);
		if (CSR_READ_4(sc, MX_SIO) & MX_SIO_EE_DATAOUT)
			word |= i;
		mx_delay(sc);
		SIO_CLR(MX_SIO_EE_CLK);
		mx_delay(sc);
	}

	/* Turn off EEPROM access mode. */
	mx_eeprom_idle(sc);

	*dest = word;

	return;
}

/*
 * Read a sequence of words from the EEPROM.
 */
static void mx_read_eeprom(sc, dest, off, cnt, swap)
	struct mx_softc		*sc;
	caddr_t			dest;
	int			off;
	int			cnt;
	int			swap;
{
	int			i;
	u_int16_t		word = 0, *ptr;

	for (i = 0; i < cnt; i++) {
		mx_eeprom_getword(sc, off + i, &word);
		ptr = (u_int16_t *)(dest + (i * 2));
		if (swap)
			*ptr = ntohs(word);
		else
			*ptr = word;
	}

	return;
}

/*
 * The following two routines are taken from the Macronix 98713
 * Application Notes pp.19-21.
 */
/*
 * Write a bit to the MII bus.
 */
static void mx_mii_writebit(sc, bit)
	struct mx_softc		*sc;
	int			bit;
{
	if (bit)
		CSR_WRITE_4(sc, MX_SIO, MX_SIO_ROMCTL_WRITE|MX_SIO_MII_DATAOUT);
	else
		CSR_WRITE_4(sc, MX_SIO, MX_SIO_ROMCTL_WRITE);

	MX_SETBIT(sc, MX_SIO, MX_SIO_MII_CLK);
	MX_CLRBIT(sc, MX_SIO, MX_SIO_MII_CLK);

	return;
}

/*
 * Read a bit from the MII bus.
 */
static int mx_mii_readbit(sc)
	struct mx_softc		*sc;
{
	CSR_WRITE_4(sc, MX_SIO, MX_SIO_ROMCTL_READ|MX_SIO_MII_DIR);
	CSR_READ_4(sc, MX_SIO);
	MX_SETBIT(sc, MX_SIO, MX_SIO_MII_CLK);
	MX_CLRBIT(sc, MX_SIO, MX_SIO_MII_CLK);
	if (CSR_READ_4(sc, MX_SIO) & MX_SIO_MII_DATAIN)
		return(1);

	return(0);
}

/*
 * Sync the PHYs by setting data bit and strobing the clock 32 times.
 */
static void mx_mii_sync(sc)
	struct mx_softc		*sc;
{
	register int		i;

	CSR_WRITE_4(sc, MX_SIO, MX_SIO_ROMCTL_WRITE);

	for (i = 0; i < 32; i++)
		mx_mii_writebit(sc, 1);

	return;
}

/*
 * Clock a series of bits through the MII.
 */
static void mx_mii_send(sc, bits, cnt)
	struct mx_softc		*sc;
	u_int32_t		bits;
	int			cnt;
{
	int			i;

	for (i = (0x1 << (cnt - 1)); i; i >>= 1)
		mx_mii_writebit(sc, bits & i);
}

/*
 * Read an PHY register through the MII.
 */
static int mx_mii_readreg(sc, frame)
	struct mx_softc		*sc;
	struct mx_mii_frame	*frame;
	
{
	int			i, ack, s;

	s = splimp();

	/*
	 * Set up frame for RX.
	 */
	frame->mii_stdelim = MX_MII_STARTDELIM;
	frame->mii_opcode = MX_MII_READOP;
	frame->mii_turnaround = 0;
	frame->mii_data = 0;
	
	/*
	 * Sync the PHYs.
	 */
	mx_mii_sync(sc);

	/*
	 * Send command/address info.
	 */
	mx_mii_send(sc, frame->mii_stdelim, 2);
	mx_mii_send(sc, frame->mii_opcode, 2);
	mx_mii_send(sc, frame->mii_phyaddr, 5);
	mx_mii_send(sc, frame->mii_regaddr, 5);

#ifdef notdef
	/* Idle bit */
	mx_mii_writebit(sc, 1);
	mx_mii_writebit(sc, 0);
#endif

	/* Check for ack */
	ack = mx_mii_readbit(sc);

	/*
	 * Now try reading data bits. If the ack failed, we still
	 * need to clock through 16 cycles to keep the PHY(s) in sync.
	 */
	if (ack) {
		for(i = 0; i < 16; i++) {
			mx_mii_readbit(sc);
		}
		goto fail;
	}

	for (i = 0x8000; i; i >>= 1) {
		if (!ack) {
			if (mx_mii_readbit(sc))
				frame->mii_data |= i;
		}
	}

fail:

	mx_mii_writebit(sc, 0);
	mx_mii_writebit(sc, 0);

	splx(s);

	if (ack)
		return(1);
	return(0);
}

/*
 * Write to a PHY register through the MII.
 */
static int mx_mii_writereg(sc, frame)
	struct mx_softc		*sc;
	struct mx_mii_frame	*frame;
	
{
	int			s;

	s = splimp();
	/*
	 * Set up frame for TX.
	 */

	frame->mii_stdelim = MX_MII_STARTDELIM;
	frame->mii_opcode = MX_MII_WRITEOP;
	frame->mii_turnaround = MX_MII_TURNAROUND;

	/*
	 * Sync the PHYs.
	 */	
	mx_mii_sync(sc);

	mx_mii_send(sc, frame->mii_stdelim, 2);
	mx_mii_send(sc, frame->mii_opcode, 2);
	mx_mii_send(sc, frame->mii_phyaddr, 5);
	mx_mii_send(sc, frame->mii_regaddr, 5);
	mx_mii_send(sc, frame->mii_turnaround, 2);
	mx_mii_send(sc, frame->mii_data, 16);

	/* Idle bit. */
	mx_mii_writebit(sc, 0);
	mx_mii_writebit(sc, 0);

	splx(s);

	return(0);
}

static u_int16_t mx_phy_readreg(sc, reg)
	struct mx_softc		*sc;
	int			reg;
{
	struct mx_mii_frame	frame;
	u_int32_t		cfg;

	bzero((char *)&frame, sizeof(frame));

	frame.mii_phyaddr = sc->mx_phy_addr;
	frame.mii_regaddr = reg;
	cfg = CSR_READ_4(sc, MX_NETCFG);
	MX_CLRBIT(sc, MX_NETCFG, MX_NETCFG_PORTSEL);
	mx_mii_readreg(sc, &frame);
	CSR_WRITE_4(sc, MX_NETCFG, cfg);

	return(frame.mii_data);
}

static void mx_phy_writereg(sc, reg, data)
	struct mx_softc		*sc;
	int			reg;
	int			data;
{
	struct mx_mii_frame	frame;
	u_int32_t		cfg;

	bzero((char *)&frame, sizeof(frame));

	frame.mii_phyaddr = sc->mx_phy_addr;
	frame.mii_regaddr = reg;
	frame.mii_data = data;

	cfg = CSR_READ_4(sc, MX_NETCFG);
	MX_CLRBIT(sc, MX_NETCFG, MX_NETCFG_PORTSEL);
	mx_mii_writereg(sc, &frame);
	CSR_WRITE_4(sc, MX_NETCFG, cfg);

	return;
}

#define MX_POLY		0xEDB88320
#define MX_BITS		9
#define MX_BITS_PNIC_II	7

static u_int32_t mx_calchash(sc, addr)
	struct mx_softc		*sc;
	caddr_t			addr;
{
	u_int32_t		idx, bit, data, crc;

	/* Compute CRC for the address value. */
	crc = 0xFFFFFFFF; /* initial value */

	for (idx = 0; idx < 6; idx++) {
		for (data = *addr++, bit = 0; bit < 8; bit++, data >>= 1)
			crc = (crc >> 1) ^ (((crc ^ data) & 1) ? MX_POLY : 0);
	}

	/* The hash table on the PNIC II is only 128 bits wide. */
	if (sc->mx_info->mx_vid == PN_VENDORID)
		return (crc & ((1 << MX_BITS_PNIC_II) - 1));

	return (crc & ((1 << MX_BITS) - 1));
}

/*
 * Initiate an autonegotiation session.
 */
static void mx_autoneg_xmit(sc)
	struct mx_softc		*sc;
{
	u_int16_t		phy_sts;

	mx_phy_writereg(sc, PHY_BMCR, PHY_BMCR_RESET);
	DELAY(500);
	while(mx_phy_readreg(sc, PHY_BMCR)
			& PHY_BMCR_RESET);

	phy_sts = mx_phy_readreg(sc, PHY_BMCR);
	phy_sts |= PHY_BMCR_AUTONEGENBL|PHY_BMCR_AUTONEGRSTR;
	mx_phy_writereg(sc, PHY_BMCR, phy_sts);

	return;
}

/*
 * Invoke autonegotiation on a PHY.
 */
static void mx_autoneg_mii(sc, flag, verbose)
	struct mx_softc		*sc;
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
	phy_sts = mx_phy_readreg(sc, PHY_BMSR);
	if (!(phy_sts & PHY_BMSR_CANAUTONEG)) {
		if (verbose)
			printf("mx%d: autonegotiation not supported\n",
							sc->mx_unit);
		ifm->ifm_media = IFM_ETHER|IFM_10_T|IFM_HDX;	
		return;
	}
#endif

	switch (flag) {
	case MX_FLAG_FORCEDELAY:
		/*
	 	 * XXX Never use this option anywhere but in the probe
	 	 * routine: making the kernel stop dead in its tracks
 		 * for three whole seconds after we've gone multi-user
		 * is really bad manners.
	 	 */
		mx_autoneg_xmit(sc);
		DELAY(5000000);
		break;
	case MX_FLAG_SCHEDDELAY:
		/*
		 * Wait for the transmitter to go idle before starting
		 * an autoneg session, otherwise mx_start() may clobber
	 	 * our timeout, and we don't want to allow transmission
		 * during an autoneg session since that can screw it up.
	 	 */
		if (sc->mx_cdata.mx_tx_head != NULL) {
			sc->mx_want_auto = 1;
			return;
		}
		mx_autoneg_xmit(sc);
		ifp->if_timer = 5;
		sc->mx_autoneg = 1;
		sc->mx_want_auto = 0;
		return;
		break;
	case MX_FLAG_DELAYTIMEO:
		ifp->if_timer = 0;
		sc->mx_autoneg = 0;
		break;
	default:
		printf("mx%d: invalid autoneg flag: %d\n", sc->mx_unit, flag);
		return;
	}

	if (mx_phy_readreg(sc, PHY_BMSR) & PHY_BMSR_AUTONEGCOMP) {
		if (verbose)
			printf("mx%d: autoneg complete, ", sc->mx_unit);
		phy_sts = mx_phy_readreg(sc, PHY_BMSR);
	} else {
		if (verbose)
			printf("mx%d: autoneg not complete, ", sc->mx_unit);
	}

	media = mx_phy_readreg(sc, PHY_BMCR);

	/* Link is good. Report modes and set duplex mode. */
	if (mx_phy_readreg(sc, PHY_BMSR) & PHY_BMSR_LINKSTAT) {
		if (verbose)
			printf("link status good ");
		advert = mx_phy_readreg(sc, PHY_ANAR);
		ability = mx_phy_readreg(sc, PHY_LPAR);

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
		} else if (advert & PHY_ANAR_10BTHALF &&
			ability & PHY_ANAR_10BTHALF) {
			ifm->ifm_media = IFM_ETHER|IFM_10_T|IFM_HDX;
			media &= ~PHY_BMCR_SPEEDSEL;
			media &= ~PHY_BMCR_DUPLEX;
			printf("(half-duplex, 10Mbps)\n");
		}

		media &= ~PHY_BMCR_AUTONEGENBL;

		/* Set ASIC's duplex mode to match the PHY. */
		mx_setcfg(sc, media);
		mx_phy_writereg(sc, PHY_BMCR, media);
	} else {
		if (verbose)
			printf("no carrier\n");
	}

	mx_init(sc);

	if (sc->mx_tx_pend) {
		sc->mx_autoneg = 0;
		sc->mx_tx_pend = 0;
		mx_start(ifp);
	}

	return;
}

/*
 * Invoke autoneg using internal NWAY.
 */
static void mx_autoneg(sc, flag, verbose)
	struct mx_softc		*sc;
	int			flag;
	int			verbose;
{
	u_int32_t		media, ability;
	struct ifnet		*ifp;
	struct ifmedia		*ifm;

	ifm = &sc->ifmedia;
	ifp = &sc->arpcom.ac_if;

	ifm->ifm_media = IFM_ETHER | IFM_AUTO;

	switch (flag) {
	case MX_FLAG_FORCEDELAY:
		/*
	 	 * XXX Never use this option anywhere but in the probe
	 	 * routine: making the kernel stop dead in its tracks
 		 * for three whole seconds after we've gone multi-user
		 * is really bad manners.
	 	 */
		MX_CLRBIT(sc, MX_NETCFG, MX_NETCFG_PORTSEL);
		MX_SETBIT(sc, MX_NETCFG, MX_NETCFG_FULLDUPLEX);
		MX_SETBIT(sc, MX_10BTCTRL, MX_TCTL_AUTONEGENBL);
		MX_SETBIT(sc, MX_10BTCTRL, MX_ASTAT_TXDISABLE);
		DELAY(5000000);
		break;
	case MX_FLAG_SCHEDDELAY:
		/*
		 * Wait for the transmitter to go idle before starting
		 * an autoneg session, otherwise mx_start() may clobber
	 	 * our timeout, and we don't want to allow transmission
		 * during an autoneg session since that can screw it up.
	 	 */
		if (sc->mx_cdata.mx_tx_head != NULL) {
			sc->mx_want_auto = 1;
			return;
		}
		MX_CLRBIT(sc, MX_NETCFG, MX_NETCFG_PORTSEL);
		MX_SETBIT(sc, MX_NETCFG, MX_NETCFG_FULLDUPLEX);
		MX_SETBIT(sc, MX_10BTCTRL, MX_TCTL_AUTONEGENBL);
		MX_SETBIT(sc, MX_10BTCTRL, MX_ASTAT_TXDISABLE);
		ifp->if_timer = 5;
		sc->mx_autoneg = 1;
		sc->mx_want_auto = 0;
		return;
		break;
	case MX_FLAG_DELAYTIMEO:
		ifp->if_timer = 0;
		sc->mx_autoneg = 0;
		break;
	default:
		printf("mx%d: invalid autoneg flag: %d\n", sc->mx_unit, flag);
		return;
	}

	if ((CSR_READ_4(sc, MX_10BTSTAT) & MX_TSTAT_ANEGSTAT) ==
						MX_ASTAT_AUTONEGCMP) {
		if (verbose)
			printf("mx%d: autoneg complete, ", sc->mx_unit);
	} else {
		if (verbose)
			printf("mx%d: autoneg not complete, ", sc->mx_unit);
	}

	media = CSR_READ_4(sc, MX_NETCFG);

	/* Link is good. Report modes and set duplex mode. */
	if (!(CSR_READ_4(sc, MX_10BTSTAT) & MX_TSTAT_LS10) ||
		!(CSR_READ_4(sc, MX_10BTSTAT) & MX_TSTAT_LS100)) {
		if (verbose)
			printf("link status good ");
		ability = CSR_READ_4(sc, MX_NWAYSTAT);
		if (ability & MX_NWAY_100BT4) {
			ifm->ifm_media = IFM_ETHER|IFM_100_T4;
			media |= MX_NETCFG_PORTSEL|MX_NETCFG_PCS|
					MX_NETCFG_SCRAMBLER;
			media &= ~(MX_NETCFG_FULLDUPLEX|MX_NETCFG_SPEEDSEL);
			printf("(100baseT4)\n");
		} else if (ability & MX_NWAY_100BTFULL) {
			ifm->ifm_media = IFM_ETHER|IFM_100_TX|IFM_FDX;
			media |= MX_NETCFG_PORTSEL|MX_NETCFG_PCS|
					MX_NETCFG_SCRAMBLER;
			media |= MX_NETCFG_FULLDUPLEX;
			media &= ~MX_NETCFG_SPEEDSEL;
			printf("(full-duplex, 100Mbps)\n");
		} else if (ability & MX_NWAY_100BTHALF) {
			ifm->ifm_media = IFM_ETHER|IFM_100_TX|IFM_HDX;
			media |= MX_NETCFG_PORTSEL|MX_NETCFG_PCS|
					MX_NETCFG_SCRAMBLER;
			media &= ~(MX_NETCFG_FULLDUPLEX|MX_NETCFG_SPEEDSEL);
			printf("(half-duplex, 100Mbps)\n");
		} else if (ability & MX_NWAY_10BTFULL) {
			ifm->ifm_media = IFM_ETHER|IFM_10_T|IFM_FDX;
			media &= ~MX_NETCFG_PORTSEL;
			media |= (MX_NETCFG_FULLDUPLEX|MX_NETCFG_SPEEDSEL);
			printf("(full-duplex, 10Mbps)\n");
		} else {
			ifm->ifm_media = IFM_ETHER|IFM_10_T|IFM_HDX;
			media &= ~MX_NETCFG_PORTSEL;
			media &= ~MX_NETCFG_FULLDUPLEX;
			media |= MX_NETCFG_SPEEDSEL;
			printf("(half-duplex, 10Mbps)\n");
		}

		CSR_WRITE_4(sc, MX_NETCFG, media);
		MX_CLRBIT(sc, MX_10BTCTRL, MX_TCTL_AUTONEGENBL);
	} else {
		if (verbose)
			printf("no carrier\n");
	}

	mx_init(sc);

	if (sc->mx_tx_pend) {
		sc->mx_autoneg = 0;
		sc->mx_tx_pend = 0;
		mx_start(ifp);
	}

	return;
}

static void mx_getmode_mii(sc)
	struct mx_softc		*sc;
{
	u_int16_t		bmsr;
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;

	bmsr = mx_phy_readreg(sc, PHY_BMSR);
	if (bootverbose)
		printf("mx%d: PHY status word: %x\n", sc->mx_unit, bmsr);

	/* fallback */
	sc->ifmedia.ifm_media = IFM_ETHER|IFM_10_T|IFM_HDX;

	if (bmsr & PHY_BMSR_10BTHALF) {
		if (bootverbose)
			printf("mx%d: 10Mbps half-duplex mode supported\n",
								sc->mx_unit);
		ifmedia_add(&sc->ifmedia,
			IFM_ETHER|IFM_10_T|IFM_HDX, 0, NULL);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_10_T, 0, NULL);
	}

	if (bmsr & PHY_BMSR_10BTFULL) {
		if (bootverbose)
			printf("mx%d: 10Mbps full-duplex mode supported\n",
								sc->mx_unit);
		ifmedia_add(&sc->ifmedia,
			IFM_ETHER|IFM_10_T|IFM_FDX, 0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_10_T|IFM_FDX;
	}

	if (bmsr & PHY_BMSR_100BTXHALF) {
		if (bootverbose)
			printf("mx%d: 100Mbps half-duplex mode supported\n",
								sc->mx_unit);
		ifp->if_baudrate = 100000000;
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_100_TX, 0, NULL);
		ifmedia_add(&sc->ifmedia,
			IFM_ETHER|IFM_100_TX|IFM_HDX, 0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_100_TX|IFM_HDX;
	}

	if (bmsr & PHY_BMSR_100BTXFULL) {
		if (bootverbose)
			printf("mx%d: 100Mbps full-duplex mode supported\n",
								sc->mx_unit);
		ifp->if_baudrate = 100000000;
		ifmedia_add(&sc->ifmedia,
			IFM_ETHER|IFM_100_TX|IFM_FDX, 0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_100_TX|IFM_FDX;
	}

	/* Some also support 100BaseT4. */
	if (bmsr & PHY_BMSR_100BT4) {
		if (bootverbose)
			printf("mx%d: 100baseT4 mode supported\n", sc->mx_unit);
		ifp->if_baudrate = 100000000;
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_100_T4, 0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_100_T4;
#ifdef FORCE_AUTONEG_TFOUR
		if (bootverbose)
			printf("mx%d: forcing on autoneg support for BT4\n",
							 sc->mx_unit);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_AUTO, 0 NULL):
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_AUTO;
#endif
	}

	if (bmsr & PHY_BMSR_CANAUTONEG) {
		if (bootverbose)
			printf("mx%d: autoneg supported\n", sc->mx_unit);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_AUTO, 0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_AUTO;
	}

	return;
}

/*
 * Set speed and duplex mode.
 */
static void mx_setmode_mii(sc, media)
	struct mx_softc		*sc;
	int			media;
{
	u_int16_t		bmcr;
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;

	/*
	 * If an autoneg session is in progress, stop it.
	 */
	if (sc->mx_autoneg) {
		printf("mx%d: canceling autoneg session\n", sc->mx_unit);
		ifp->if_timer = sc->mx_autoneg = sc->mx_want_auto = 0;
		bmcr = mx_phy_readreg(sc, PHY_BMCR);
		bmcr &= ~PHY_BMCR_AUTONEGENBL;
		mx_phy_writereg(sc, PHY_BMCR, bmcr);
	}

	printf("mx%d: selecting MII, ", sc->mx_unit);

	bmcr = mx_phy_readreg(sc, PHY_BMCR);

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

	mx_setcfg(sc, bmcr);
	mx_phy_writereg(sc, PHY_BMCR, bmcr);

	return;
}

/*
 * Set speed and duplex mode on internal transceiver.
 */
static void mx_setmode(sc, media, verbose)
	struct mx_softc		*sc;
	int			media;
	int			verbose;
{
	struct ifnet		*ifp;
	u_int32_t		mode;

	ifp = &sc->arpcom.ac_if;

	/*
	 * If an autoneg session is in progress, stop it.
	 */
	if (sc->mx_autoneg) {
		printf("mx%d: canceling autoneg session\n", sc->mx_unit);
		ifp->if_timer = sc->mx_autoneg = sc->mx_want_auto = 0;
		MX_CLRBIT(sc, MX_10BTCTRL, MX_TCTL_AUTONEGENBL);
	}

	if (verbose)
		printf("mx%d: selecting NWAY, ", sc->mx_unit);

	mode = CSR_READ_4(sc, MX_NETCFG);

	mode &= ~(MX_NETCFG_FULLDUPLEX|MX_NETCFG_PORTSEL|
		MX_NETCFG_PCS|MX_NETCFG_SCRAMBLER|MX_NETCFG_SPEEDSEL);

	if (IFM_SUBTYPE(media) == IFM_100_T4) {
		if (verbose)
			printf("100Mbps/T4, half-duplex\n");
		mode |= MX_NETCFG_PORTSEL|MX_NETCFG_PCS|MX_NETCFG_SCRAMBLER;
	}

	if (IFM_SUBTYPE(media) == IFM_100_TX) {
		if (verbose)
			printf("100Mbps, ");
		mode |= MX_NETCFG_PORTSEL|MX_NETCFG_PCS|MX_NETCFG_SCRAMBLER;
	}

	if (IFM_SUBTYPE(media) == IFM_10_T) {
		if (verbose)
			printf("10Mbps, ");
		mode &= ~MX_NETCFG_PORTSEL;
		mode |= MX_NETCFG_SPEEDSEL;
	}

	if ((media & IFM_GMASK) == IFM_FDX) {
		if (verbose)
			printf("full duplex\n");
		mode |= MX_NETCFG_FULLDUPLEX;
	} else {
		if (verbose)
			printf("half duplex\n");
		mode &= ~MX_NETCFG_FULLDUPLEX;
	}

	CSR_WRITE_4(sc, MX_NETCFG, mode);

	return;
}

/*
 * Programming the receiver filter on the tulip/PMAC is gross. You
 * have to construct a special setup frame and download it to the
 * chip via the transmit DMA engine. This routine is also somewhat
 * gross, as the setup frame is sent synchronously rather than putting
 * on the transmit queue. The transmitter has to be stopped, then we
 * can download the frame and wait for the 'owned' bit to clear.
 *
 * We always program the chip using 'hash perfect' mode, i.e. one perfect
 * address (our node address) and a 512-bit hash filter for multicast
 * frames. We also sneak the broadcast address into the hash filter since
 * we need that too.
 */
void mx_setfilt(sc)
	struct mx_softc		*sc;
{
	struct mx_desc		*sframe;
	u_int32_t		h, *sp;
	struct ifmultiaddr	*ifma;
	struct ifnet		*ifp;
	int			i;

	ifp = &sc->arpcom.ac_if;

	MX_CLRBIT(sc, MX_NETCFG, MX_NETCFG_TX_ON);
	MX_SETBIT(sc, MX_ISR, MX_ISR_TX_IDLE);

	sframe = &sc->mx_cdata.mx_sframe;
	sp = (u_int32_t *)&sc->mx_cdata.mx_sbuf;
	bzero((char *)sp, MX_SFRAME_LEN);

	sframe->mx_next = vtophys(&sc->mx_ldata->mx_tx_list[0]);
	sframe->mx_data = vtophys(&sc->mx_cdata.mx_sbuf);
	sframe->mx_ctl = MX_SFRAME_LEN | MX_TXCTL_TLINK |
			MX_TXCTL_SETUP | MX_FILTER_HASHPERF;

	/* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC)
		MX_SETBIT(sc, MX_NETCFG, MX_NETCFG_RX_PROMISC);
	else
		MX_CLRBIT(sc, MX_NETCFG, MX_NETCFG_RX_PROMISC);

	if (ifp->if_flags & IFF_ALLMULTI)
		MX_SETBIT(sc, MX_NETCFG, MX_NETCFG_RX_ALLMULTI);

	for (ifma = ifp->if_multiaddrs.lh_first; ifma != NULL;
				ifma = ifma->ifma_link.le_next) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		h = mx_calchash(sc,
		    LLADDR((struct sockaddr_dl *)ifma->ifma_addr));
		sp[h >> 4] |= 1 << (h & 0xF);
	}

	if (ifp->if_flags & IFF_BROADCAST) {
		h = mx_calchash(sc, (caddr_t)&etherbroadcastaddr);
		sp[h >> 4] |= 1 << (h & 0xF);
	}

	sp[39] = ((u_int16_t *)sc->arpcom.ac_enaddr)[0];
	sp[40] = ((u_int16_t *)sc->arpcom.ac_enaddr)[1];
	sp[41] = ((u_int16_t *)sc->arpcom.ac_enaddr)[2];

	CSR_WRITE_4(sc, MX_TXADDR, vtophys(sframe));
	MX_SETBIT(sc, MX_NETCFG, MX_NETCFG_TX_ON);
	sframe->mx_status = MX_TXSTAT_OWN;
	CSR_WRITE_4(sc, MX_TXSTART, 0xFFFFFFFF);

	/*
	 * Wait for chip to clear the 'own' bit.
	 */
	for (i = 0; i < MX_TIMEOUT; i++) {
		DELAY(10);
		if (sframe->mx_status != MX_TXSTAT_OWN)
			break;
	}

	if (i == MX_TIMEOUT)
		printf("mx%d: failed to send setup frame\n", sc->mx_unit);

	MX_SETBIT(sc, MX_ISR, MX_ISR_TX_NOBUF|MX_ISR_TX_IDLE);

	return;
}

/*
 * In order to fiddle with the
 * 'full-duplex' and '100Mbps' bits in the netconfig register, we
 * first have to put the transmit and/or receive logic in the idle state.
 */
static void mx_setcfg(sc, bmcr)
	struct mx_softc		*sc;
	int			bmcr;
{
	int			i, restart = 0;

	if (CSR_READ_4(sc, MX_NETCFG) & (MX_NETCFG_TX_ON|MX_NETCFG_RX_ON)) {
		restart = 1;
		MX_CLRBIT(sc, MX_NETCFG, (MX_NETCFG_TX_ON|MX_NETCFG_RX_ON));

		for (i = 0; i < MX_TIMEOUT; i++) {
			DELAY(10);
			if (CSR_READ_4(sc, MX_ISR) & MX_ISR_TX_IDLE)
				break;
		}

		if (i == MX_TIMEOUT)
			printf("mx%d: failed to force tx and "
				"rx to idle state\n", sc->mx_unit);

	}

	if (bmcr & PHY_BMCR_SPEEDSEL) {
		MX_CLRBIT(sc, MX_NETCFG, MX_NETCFG_SPEEDSEL);
		if (sc->mx_phy_addr == 0) {
			MX_SETBIT(sc, MX_NETCFG, MX_NETCFG_PORTSEL|
				MX_NETCFG_PCS|MX_NETCFG_SCRAMBLER);
		}
	} else
		MX_SETBIT(sc, MX_NETCFG, MX_NETCFG_SPEEDSEL);

	if (bmcr & PHY_BMCR_DUPLEX)
		MX_SETBIT(sc, MX_NETCFG, MX_NETCFG_FULLDUPLEX);
	else
		MX_CLRBIT(sc, MX_NETCFG, MX_NETCFG_FULLDUPLEX);

	if (restart)
		MX_SETBIT(sc, MX_NETCFG, MX_NETCFG_TX_ON|MX_NETCFG_RX_ON);

	return;
}

static void mx_reset(sc)
	struct mx_softc		*sc;
{
	register int		i;

	MX_SETBIT(sc, MX_BUSCTL, MX_BUSCTL_RESET);

	for (i = 0; i < MX_TIMEOUT; i++) {
		DELAY(10);
		if (!(CSR_READ_4(sc, MX_BUSCTL) & MX_BUSCTL_RESET))
			break;
	}
	if (i == MX_TIMEOUT)
		printf("mx%d: reset never completed!\n", sc->mx_unit);

	/* Wait a little while for the chip to get its brains in order. */
	DELAY(1000);
        return;
}

static struct mx_type *mx_devtype(config_id, device_id)
	pcici_t			config_id;
	pcidi_t			device_id;
{
	struct mx_type		*t;
	u_int32_t		rev;

	t = mx_devs;

	while(t->mx_name != NULL) {
		if ((device_id & 0xFFFF) == t->mx_vid &&
		    ((device_id >> 16) & 0xFFFF) == t->mx_did) {
			/* Check the PCI revision */
			rev = pci_conf_read(config_id, MX_PCI_REVID) & 0xFF;
			if (t->mx_did == MX_DEVICEID_98713 &&
						rev >= MX_REVISION_98713A)
				t++;
			if (t->mx_did == CP_DEVICEID_98713 &&
						rev >= MX_REVISION_98713A)
				t++;
			if (t->mx_did == MX_DEVICEID_987x5 &&
						rev >= MX_REVISION_98725)
				t++;
			return(t);
		}
		t++;
	}

	return(NULL);
}

/*
 * Probe for a Macronix PMAC chip. Check the PCI vendor and device
 * IDs against our list and return a device name if we find a match.
 * We do a little bit of extra work to identify the exact type of
 * chip. The MX98713 and MX98713A have the same PCI vendor/device ID,
 * but different revision IDs. The same is true for 98715/98715A
 * chips and the 98725. This doesn't affect a whole lot, but it
 * lets us tell the user exactly what type of device they have
 * in the probe output.
 */
static const char *
mx_probe(config_id, device_id)
	pcici_t			config_id;
	pcidi_t			device_id;
{
	struct mx_type		*t;

	t = mx_devtype(config_id, device_id);

	if (t != NULL)
		return(t->mx_name);

	return(NULL);
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
static void
mx_attach(config_id, unit)
	pcici_t			config_id;
	int			unit;
{
	int			s, i;
#ifndef MX_USEIOSPACE
	vm_offset_t		pbase, vbase;
#endif
	u_char			eaddr[ETHER_ADDR_LEN];
	u_int32_t		command;
	struct mx_softc		*sc;
	struct ifnet		*ifp;
	int			media = IFM_ETHER|IFM_100_TX|IFM_FDX;
	unsigned int		round;
	caddr_t			roundptr;
	struct mx_type		*p;
	u_int16_t		phy_vid, phy_did, phy_sts, mac_offset = 0;
	u_int32_t		revision, pci_id;

	s = splimp();

	sc = malloc(sizeof(struct mx_softc), M_DEVBUF, M_NOWAIT);
	if (sc == NULL) {
		printf("mx%d: no memory for softc struct!\n", unit);
		goto fail;
	}
	bzero(sc, sizeof(struct mx_softc));

	/*
	 * Handle power management nonsense.
	 */

	command = pci_conf_read(config_id, MX_PCI_CAPID) & 0x000000FF;
	if (command == 0x01) {

		command = pci_conf_read(config_id, MX_PCI_PWRMGMTCTRL);
		if (command & MX_PSTATE_MASK) {
			u_int32_t		iobase, membase, irq;

			/* Save important PCI config data. */
			iobase = pci_conf_read(config_id, MX_PCI_LOIO);
			membase = pci_conf_read(config_id, MX_PCI_LOMEM);
			irq = pci_conf_read(config_id, MX_PCI_INTLINE);

			/* Reset the power state. */
			printf("mx%d: chip is in D%d power mode "
			"-- setting to D0\n", unit, command & MX_PSTATE_MASK);
			command &= 0xFFFFFFFC;
			pci_conf_write(config_id, MX_PCI_PWRMGMTCTRL, command);

			/* Restore PCI config data. */
			pci_conf_write(config_id, MX_PCI_LOIO, iobase);
			pci_conf_write(config_id, MX_PCI_LOMEM, membase);
			pci_conf_write(config_id, MX_PCI_INTLINE, irq);
		}
	}

	/*
	 * Map control/status registers.
	 */
	command = pci_conf_read(config_id, PCI_COMMAND_STATUS_REG);
	command |= (PCIM_CMD_PORTEN|PCIM_CMD_MEMEN|PCIM_CMD_BUSMASTEREN);
	pci_conf_write(config_id, PCI_COMMAND_STATUS_REG, command);
	command = pci_conf_read(config_id, PCI_COMMAND_STATUS_REG);

#ifdef MX_USEIOSPACE
	if (!(command & PCIM_CMD_PORTEN)) {
		printf("mx%d: failed to enable I/O ports!\n", unit);
		free(sc, M_DEVBUF);
		goto fail;
	}

	if (!pci_map_port(config_id, MX_PCI_LOIO,
					(pci_port_t *)&(sc->mx_bhandle))) {
		printf ("mx%d: couldn't map ports\n", unit);
		goto fail;
        }
#ifdef __i386__
	sc->mx_btag = I386_BUS_SPACE_IO;
#endif
#ifdef __alpha__
	sc->mx_btag = ALPHA_BUS_SPACE_IO;
#endif
#else
	if (!(command & PCIM_CMD_MEMEN)) {
		printf("mx%d: failed to enable memory mapping!\n", unit);
		goto fail;
	}

	if (!pci_map_mem(config_id, MX_PCI_LOMEM, &vbase, &pbase)) {
		printf ("mx%d: couldn't map memory\n", unit);
		goto fail;
	}
#ifdef __i386__
	sc->mx_btag = I386_BUS_SPACE_MEM;
#endif
#ifdef __alpha__
	sc->mx_btag = ALPHA_BUS_SPACE_MEM;
#endif
	sc->mx_bhandle = vbase;
#endif

	/* Allocate interrupt */
	if (!pci_map_int(config_id, mx_intr, sc, &net_imask)) {
		printf("mx%d: couldn't map interrupt\n", unit);
		goto fail;
	}

	/* Need this info to decide on a chip type. */
	revision = pci_conf_read(config_id, MX_PCI_REVID) & 0x000000FF;
	pci_id = (pci_conf_read(config_id,MX_PCI_VENDOR_ID) >> 16) & 0x0000FFFF;

	if (pci_id == MX_DEVICEID_98713 && revision < MX_REVISION_98713A)
		sc->mx_type = MX_TYPE_98713;
	else if (pci_id == CP_DEVICEID_98713 && revision < MX_REVISION_98713A)
		sc->mx_type = MX_TYPE_98713;
	else if (pci_id == MX_DEVICEID_98713 && revision >= MX_REVISION_98713A)
		sc->mx_type = MX_TYPE_98713A;
	else
		sc->mx_type = MX_TYPE_987x5;

	/* Save the cache line size. */
	sc->mx_cachesize = pci_conf_read(config_id, MX_PCI_CACHELEN) & 0xFF;

	/* Save the device info; the PNIC II requires special handling. */
	pci_id = pci_conf_read(config_id,MX_PCI_VENDOR_ID);
	sc->mx_info = mx_devtype(config_id, pci_id);

	/* Reset the adapter. */
	mx_reset(sc);

	/*
	 * Get station address from the EEPROM.
	 */
	mx_read_eeprom(sc, (caddr_t)&mac_offset,
			(MX_EE_NODEADDR_OFFSET / 2), 1, 0);
	mx_read_eeprom(sc, (caddr_t)&eaddr, (mac_offset / 2), 3, 0);

	/*
	 * A PMAC chip was detected. Inform the world.
	 */
	printf("mx%d: Ethernet address: %6D\n", unit, eaddr, ":");

	sc->mx_unit = unit;
	bcopy(eaddr, (char *)&sc->arpcom.ac_enaddr, ETHER_ADDR_LEN);

	sc->mx_ldata_ptr = malloc(sizeof(struct mx_list_data) + 8,
				M_DEVBUF, M_NOWAIT);
	if (sc->mx_ldata_ptr == NULL) {
		free(sc, M_DEVBUF);
		printf("mx%d: no memory for list buffers!\n", unit);
		goto fail;
	}

	sc->mx_ldata = (struct mx_list_data *)sc->mx_ldata_ptr;
	round = (uintptr_t)sc->mx_ldata_ptr & 0xF;
	roundptr = sc->mx_ldata_ptr;
	for (i = 0; i < 8; i++) {
		if (round % 8) {
			round++;
			roundptr++;
		}
			break;
	}
	sc->mx_ldata = (struct mx_list_data *)roundptr;
	bzero(sc->mx_ldata, sizeof(struct mx_list_data));

	ifp = &sc->arpcom.ac_if;
	ifp->if_softc = sc;
	ifp->if_unit = unit;
	ifp->if_name = "mx";
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = mx_ioctl;
	ifp->if_output = ether_output;
	ifp->if_start = mx_start;
	ifp->if_watchdog = mx_watchdog;
	ifp->if_init = mx_init;
	ifp->if_baudrate = 10000000;
	ifp->if_snd.ifq_maxlen = MX_TX_LIST_CNT - 1;

	if (sc->mx_type == MX_TYPE_98713) {
		if (bootverbose)
			printf("mx%d: probing for a PHY\n", sc->mx_unit);
		for (i = MX_PHYADDR_MIN; i < MX_PHYADDR_MAX + 1; i++) {
			if (bootverbose)
				printf("mx%d: checking address: %d\n",
							sc->mx_unit, i);
			sc->mx_phy_addr = i;
			mx_phy_writereg(sc, PHY_BMCR, PHY_BMCR_RESET);
			DELAY(500);
			while(mx_phy_readreg(sc, PHY_BMCR)
					& PHY_BMCR_RESET);
			if ((phy_sts = mx_phy_readreg(sc, PHY_BMSR)))
				break;
		}
		if (phy_sts) {
			phy_vid = mx_phy_readreg(sc, PHY_VENID);
			phy_did = mx_phy_readreg(sc, PHY_DEVID);
			if (bootverbose)
				printf("mx%d: found PHY at address %d, ",
					sc->mx_unit, sc->mx_phy_addr);
			if (bootverbose)
				printf("vendor id: %x device id: %x\n",
				phy_vid, phy_did);
			p = mx_phys;
			while(p->mx_vid) {
				if (phy_vid == p->mx_vid &&
					(phy_did | 0x000F) == p->mx_did) {
					sc->mx_pinfo = p;
					break;
				}
				p++;
			}
			if (sc->mx_pinfo == NULL)
				sc->mx_pinfo = &mx_phys[PHY_UNKNOWN];
			if (bootverbose)
				printf("mx%d: PHY type: %s\n",
					sc->mx_unit, sc->mx_pinfo->mx_name);
		} else {
#ifdef DIAGNOSTIC
			printf("mx%d: MII without any phy!\n", sc->mx_unit);
#endif
		}
	}

	/*
	 * Do ifmedia setup.
	 */
	ifmedia_init(&sc->ifmedia, 0, mx_ifmedia_upd, mx_ifmedia_sts);

	if (sc->mx_type == MX_TYPE_98713 && sc->mx_pinfo != NULL) {
		mx_getmode_mii(sc);
		mx_autoneg_mii(sc, MX_FLAG_FORCEDELAY, 1);
	} else {
		ifmedia_add(&sc->ifmedia,
			IFM_ETHER|IFM_10_T|IFM_HDX, 0, NULL);
		ifmedia_add(&sc->ifmedia,
			IFM_ETHER|IFM_10_T|IFM_FDX, 0, NULL);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_10_T, 0, NULL);
		ifmedia_add(&sc->ifmedia,
			IFM_ETHER|IFM_100_TX|IFM_HDX, 0, NULL);
		ifmedia_add(&sc->ifmedia,
			IFM_ETHER|IFM_100_TX|IFM_FDX, 0, NULL);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_100_TX, 0, NULL);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_AUTO, 0, NULL);
		mx_autoneg(sc, MX_FLAG_FORCEDELAY, 1);
	}

	media = sc->ifmedia.ifm_media;
	mx_stop(sc);

	ifmedia_set(&sc->ifmedia, media);

	/*
	 * Call MI attach routines.
	 */
	if_attach(ifp);
	ether_ifattach(ifp);

#if NBPF > 0
	bpfattach(ifp, DLT_EN10MB, sizeof(struct ether_header));
#endif
	at_shutdown(mx_shutdown, sc, SHUTDOWN_POST_SYNC);

fail:
	splx(s);
	return;
}

/*
 * Initialize the transmit descriptors.
 */
static int mx_list_tx_init(sc)
	struct mx_softc		*sc;
{
	struct mx_chain_data	*cd;
	struct mx_list_data	*ld;
	int			i;

	cd = &sc->mx_cdata;
	ld = sc->mx_ldata;
	for (i = 0; i < MX_TX_LIST_CNT; i++) {
		cd->mx_tx_chain[i].mx_ptr = &ld->mx_tx_list[i];
		if (i == (MX_TX_LIST_CNT - 1))
			cd->mx_tx_chain[i].mx_nextdesc =
				&cd->mx_tx_chain[0];
		else
			cd->mx_tx_chain[i].mx_nextdesc =
				&cd->mx_tx_chain[i + 1];
	}

	cd->mx_tx_free = &cd->mx_tx_chain[0];
	cd->mx_tx_tail = cd->mx_tx_head = NULL;

	return(0);
}


/*
 * Initialize the RX descriptors and allocate mbufs for them. Note that
 * we arrange the descriptors in a closed ring, so that the last descriptor
 * points back to the first.
 */
static int mx_list_rx_init(sc)
	struct mx_softc		*sc;
{
	struct mx_chain_data	*cd;
	struct mx_list_data	*ld;
	int			i;

	cd = &sc->mx_cdata;
	ld = sc->mx_ldata;

	for (i = 0; i < MX_RX_LIST_CNT; i++) {
		cd->mx_rx_chain[i].mx_ptr =
			(struct mx_desc *)&ld->mx_rx_list[i];
		if (mx_newbuf(sc, &cd->mx_rx_chain[i]) == ENOBUFS)
			return(ENOBUFS);
		if (i == (MX_RX_LIST_CNT - 1)) {
			cd->mx_rx_chain[i].mx_nextdesc = &cd->mx_rx_chain[0];
			ld->mx_rx_list[i].mx_next =
					vtophys(&ld->mx_rx_list[0]);
		} else {
			cd->mx_rx_chain[i].mx_nextdesc = &cd->mx_rx_chain[i + 1];
			ld->mx_rx_list[i].mx_next =
					vtophys(&ld->mx_rx_list[i + 1]);
		}
	}

	cd->mx_rx_head = &cd->mx_rx_chain[0];

	return(0);
}

/*
 * Initialize an RX descriptor and attach an MBUF cluster.
 * Note: the length fields are only 11 bits wide, which means the
 * largest size we can specify is 2047. This is important because
 * MCLBYTES is 2048, so we have to subtract one otherwise we'll
 * overflow the field and make a mess.
 */
static int mx_newbuf(sc, c)
	struct mx_softc		*sc;
	struct mx_chain_onefrag	*c;
{
	struct mbuf		*m_new = NULL;

	MGETHDR(m_new, M_DONTWAIT, MT_DATA);
	if (m_new == NULL) {
		printf("mx%d: no memory for rx list -- packet dropped!\n",
								sc->mx_unit);
		return(ENOBUFS);
	}

	MCLGET(m_new, M_DONTWAIT);
	if (!(m_new->m_flags & M_EXT)) {
		printf("mx%d: no memory for rx list -- packet dropped!\n",
								sc->mx_unit);
		m_freem(m_new);
		return(ENOBUFS);
	}

	c->mx_mbuf = m_new;
	c->mx_ptr->mx_status = MX_RXSTAT;
	c->mx_ptr->mx_data = vtophys(mtod(m_new, caddr_t));
	c->mx_ptr->mx_ctl = MX_RXCTL_RLINK | (MCLBYTES - 1);

	return(0);
}

/*
 * A frame has been uploaded: pass the resulting mbuf chain up to
 * the higher level protocols.
 */
static void mx_rxeof(sc)
	struct mx_softc		*sc;
{
        struct ether_header	*eh;
        struct mbuf		*m;
        struct ifnet		*ifp;
	struct mx_chain_onefrag	*cur_rx;
	int			total_len = 0;
	u_int32_t		rxstat;

	ifp = &sc->arpcom.ac_if;

	while(!((rxstat = sc->mx_cdata.mx_rx_head->mx_ptr->mx_status) &
							MX_RXSTAT_OWN)) {
#ifdef __alpha__
		struct mbuf		*m0 = NULL;
#endif
		cur_rx = sc->mx_cdata.mx_rx_head;
		sc->mx_cdata.mx_rx_head = cur_rx->mx_nextdesc;

		/*
		 * If an error occurs, update stats, clear the
		 * status word and leave the mbuf cluster in place:
		 * it should simply get re-used next time this descriptor
	 	 * comes up in the ring.
		 */
		if (rxstat & MX_RXSTAT_RXERR) {
			ifp->if_ierrors++;
			if (rxstat & MX_RXSTAT_COLLSEEN)
				ifp->if_collisions++;
			cur_rx->mx_ptr->mx_status = MX_RXSTAT;
			cur_rx->mx_ptr->mx_ctl =
				MX_RXCTL_RLINK | (MCLBYTES - 1);
			continue;
		}

		/* No errors; receive the packet. */	
		m = cur_rx->mx_mbuf;
		total_len = MX_RXBYTES(cur_rx->mx_ptr->mx_status);

		/*
		 * XXX The Macronix chips includes the CRC with every
		 * received frame, and there's no way to turn this
		 * behavior off (at least, I can't find anything in
	 	 * the manual that explains how to do it) so we have
		 * to trim off the CRC manually.
		 */
		total_len -= ETHER_CRC_LEN;

		/*
		 * Try to conjure up a new mbuf cluster. If that
		 * fails, it means we have an out of memory condition and
		 * should leave the buffer in place and continue. This will
		 * result in a lost packet, but there's little else we
		 * can do in this situation.
		 */
		if (mx_newbuf(sc, cur_rx) == ENOBUFS) {
			ifp->if_ierrors++;
			cur_rx->mx_ptr->mx_status = MX_RXSTAT;
			cur_rx->mx_ptr->mx_ctl =
					MX_RXCTL_RLINK | (MCLBYTES - 1);
			continue;
		}

#ifdef __alpha__
		/*
		 * Deal with alignment on alpha.
		 */
		MGETHDR(m0, M_DONTWAIT, MT_DATA);
		if (m0 == NULL) {
			ifp->if_ierrors++;
			cur_rx->mx_ptr->mx_status = MX_RXSTAT;
			cur_rx->mx_ptr->mx_ctl =
					MX_RXCTL_RLINK | (MCLBYTES - 1);
			bzero((char *)mtod(cur_rx->mx_mbuf, char *), MCLBYTES);
			continue;
		}

		m0->m_data += 2;
		if (total_len <= (MHLEN - 2)) {
			bcopy(mtod(m, caddr_t), mtod(m0, caddr_t), total_len);				m_freem(m);
			m = m0;
			m->m_pkthdr.len = m->m_len = total_len;
		} else {
			bcopy(mtod(m, caddr_t), mtod(m0, caddr_t), (MHLEN - 2));
			m->m_len = total_len - (MHLEN - 2);
			m->m_data += (MHLEN - 2);
			m0->m_next = m;
			m0->m_len = (MHLEN - 2);
			m = m0;
			m->m_pkthdr.len = total_len;
		}
#else
		m->m_pkthdr.len = m->m_len = total_len;
#endif
		ifp->if_ipackets++;
		eh = mtod(m, struct ether_header *);
		m->m_pkthdr.rcvif = ifp;

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

void mx_rxeoc(sc)
	struct mx_softc		*sc;
{

	mx_rxeof(sc);
	MX_CLRBIT(sc, MX_NETCFG, MX_NETCFG_RX_ON);
	CSR_WRITE_4(sc, MX_RXADDR, vtophys(sc->mx_cdata.mx_rx_head->mx_ptr));
	MX_SETBIT(sc, MX_NETCFG, MX_NETCFG_RX_ON);
	CSR_WRITE_4(sc, MX_RXSTART, 0xFFFFFFFF);

	return;
}

/*
 * A frame was downloaded to the chip. It's safe for us to clean up
 * the list buffers.
 */

static void mx_txeof(sc)
	struct mx_softc		*sc;
{
	struct mx_chain		*cur_tx;
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;

	/* Clear the timeout timer. */
	ifp->if_timer = 0;

	if (sc->mx_cdata.mx_tx_head == NULL)
		return;

	/*
	 * Go through our tx list and free mbufs for those
	 * frames that have been transmitted.
	 */
	while(sc->mx_cdata.mx_tx_head->mx_mbuf != NULL) {
		u_int32_t		txstat;

		cur_tx = sc->mx_cdata.mx_tx_head;
		txstat = MX_TXSTATUS(cur_tx);

		if (txstat & MX_TXSTAT_OWN)
			break;

		if (txstat & MX_TXSTAT_ERRSUM) {
			ifp->if_oerrors++;
			if (txstat & MX_TXSTAT_EXCESSCOLL)
				ifp->if_collisions++;
			if (txstat & MX_TXSTAT_LATECOLL)
				ifp->if_collisions++;
		}

		ifp->if_collisions += (txstat & MX_TXSTAT_COLLCNT) >> 3;

		ifp->if_opackets++;
		m_freem(cur_tx->mx_mbuf);
		cur_tx->mx_mbuf = NULL;

		if (sc->mx_cdata.mx_tx_head == sc->mx_cdata.mx_tx_tail) {
			sc->mx_cdata.mx_tx_head = NULL;
			sc->mx_cdata.mx_tx_tail = NULL;
			break;
		}

		sc->mx_cdata.mx_tx_head = cur_tx->mx_nextdesc;
	}

	return;
}

/*
 * TX 'end of channel' interrupt handler.
 */
static void mx_txeoc(sc)
	struct mx_softc		*sc;
{
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;

	ifp->if_timer = 0;

	if (sc->mx_cdata.mx_tx_head == NULL) {
		ifp->if_flags &= ~IFF_OACTIVE;
		sc->mx_cdata.mx_tx_tail = NULL;
		if (sc->mx_want_auto) {
			if (sc->mx_type == MX_TYPE_98713 &&
						sc->mx_pinfo != NULL)
				mx_autoneg_mii(sc, MX_FLAG_DELAYTIMEO, 1);
			else
				mx_autoneg(sc, MX_FLAG_DELAYTIMEO, 1);
		}
	}

	return;
}

static void mx_intr(arg)
	void			*arg;
{
	struct mx_softc		*sc;
	struct ifnet		*ifp;
	u_int32_t		status;

	sc = arg;
	ifp = &sc->arpcom.ac_if;

	/* Supress unwanted interrupts */
	if (!(ifp->if_flags & IFF_UP)) {
		mx_stop(sc);
		return;
	}

	/* Disable interrupts. */
	CSR_WRITE_4(sc, MX_IMR, 0x00000000);

	for (;;) {
		status = CSR_READ_4(sc, MX_ISR);
		if (status)
			CSR_WRITE_4(sc, MX_ISR, status);

		if ((status & MX_INTRS) == 0)
			break;

		if (status & MX_ISR_TX_OK)
			mx_txeof(sc);

		if (status & MX_ISR_TX_NOBUF)
			mx_txeoc(sc);

		if (status & MX_ISR_TX_IDLE) {
			mx_txeof(sc);
			if (sc->mx_cdata.mx_tx_head != NULL) {
				MX_SETBIT(sc, MX_NETCFG, MX_NETCFG_TX_ON);
				CSR_WRITE_4(sc, MX_TXSTART, 0xFFFFFFFF);
			}
		}

		if (status & MX_ISR_TX_UNDERRUN) {
			u_int32_t		cfg;
			cfg = CSR_READ_4(sc, MX_NETCFG);
			if ((cfg & MX_NETCFG_TX_THRESH) == MX_TXTHRESH_160BYTES)
				MX_SETBIT(sc, MX_NETCFG, MX_NETCFG_STORENFWD);
			else
				CSR_WRITE_4(sc, MX_NETCFG, cfg + 0x4000);
		}

		if (status & MX_ISR_RX_OK)
			mx_rxeof(sc);

		if ((status & MX_ISR_RX_WATDOGTIMEO)
					|| (status & MX_ISR_RX_NOBUF))
			mx_rxeoc(sc);

		if (status & MX_ISR_BUS_ERR) {
			mx_reset(sc);
			mx_init(sc);
		}
	}

	/* Re-enable interrupts. */
	CSR_WRITE_4(sc, MX_IMR, MX_INTRS);

	if (ifp->if_snd.ifq_head != NULL) {
		mx_start(ifp);
	}

	return;
}

/*
 * Encapsulate an mbuf chain in a descriptor by coupling the mbuf data
 * pointers to the fragment pointers.
 */
static int mx_encap(sc, c, m_head)
	struct mx_softc		*sc;
	struct mx_chain		*c;
	struct mbuf		*m_head;
{
	int			frag = 0;
	struct mx_desc		*f = NULL;
	int			total_len;
	struct mbuf		*m;

	/*
 	 * Start packing the mbufs in this chain into
	 * the fragment pointers. Stop when we run out
 	 * of fragments or hit the end of the mbuf chain.
	 */
	m = m_head;
	total_len = 0;

	for (m = m_head, frag = 0; m != NULL; m = m->m_next) {
		if (m->m_len != 0) {
			if (frag == MX_MAXFRAGS)
				break;
			total_len += m->m_len;
			f = &c->mx_ptr->mx_frag[frag];
			f->mx_ctl = MX_TXCTL_TLINK | m->m_len;
			if (frag == 0) {
				f->mx_status = 0;
				f->mx_ctl |= MX_TXCTL_FIRSTFRAG;
			} else
				f->mx_status = MX_TXSTAT_OWN;
			f->mx_next = vtophys(&c->mx_ptr->mx_frag[frag + 1]);
			f->mx_data = vtophys(mtod(m, vm_offset_t));
			frag++;
		}
	}

	/*
	 * Handle special case: we ran out of fragments,
	 * but we have more mbufs left in the chain. Copy the
	 * data into an mbuf cluster. Note that we don't
	 * bother clearing the values in the other fragment
	 * pointers/counters; it wouldn't gain us anything,
	 * and would waste cycles.
	 */
	if (m != NULL) {
		struct mbuf		*m_new = NULL;

		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL) {
			printf("mx%d: no memory for tx list", sc->mx_unit);
			return(1);
		}
		if (m_head->m_pkthdr.len > MHLEN) {
			MCLGET(m_new, M_DONTWAIT);
			if (!(m_new->m_flags & M_EXT)) {
				m_freem(m_new);
				printf("mx%d: no memory for tx list",
						sc->mx_unit);
				return(1);
			}
		}
		m_copydata(m_head, 0, m_head->m_pkthdr.len,	
					mtod(m_new, caddr_t));
		m_new->m_pkthdr.len = m_new->m_len = m_head->m_pkthdr.len;
		m_freem(m_head);
		m_head = m_new;
		f = &c->mx_ptr->mx_frag[0];
		f->mx_status = 0;
		f->mx_data = vtophys(mtod(m_new, caddr_t));
		f->mx_ctl = total_len = m_new->m_len;
		f->mx_ctl |= MX_TXCTL_TLINK|MX_TXCTL_FIRSTFRAG;
		frag = 1;
	}


	if (total_len < MX_MIN_FRAMELEN) {
		f = &c->mx_ptr->mx_frag[frag];
		f->mx_ctl = MX_MIN_FRAMELEN - total_len;
		f->mx_data = vtophys(&sc->mx_cdata.mx_pad);
		f->mx_ctl |= MX_TXCTL_TLINK;
		f->mx_status = MX_TXSTAT_OWN;
		frag++;
	}

	c->mx_mbuf = m_head;
	c->mx_lastdesc = frag - 1;
	MX_TXCTL(c) |= MX_TXCTL_LASTFRAG|MX_TXCTL_FINT;
	MX_TXNEXT(c) = vtophys(&c->mx_nextdesc->mx_ptr->mx_frag[0]);
	return(0);
}

/*
 * Main transmit routine. To avoid having to do mbuf copies, we put pointers
 * to the mbuf data regions directly in the transmit lists. We also save a
 * copy of the pointers since the transmit list fragment pointers are
 * physical addresses.
 */

static void mx_start(ifp)
	struct ifnet		*ifp;
{
	struct mx_softc		*sc;
	struct mbuf		*m_head = NULL;
	struct mx_chain		*cur_tx = NULL, *start_tx;

	sc = ifp->if_softc;

	if (sc->mx_autoneg) {
		sc->mx_tx_pend = 1;
		return;
	}

	if (ifp->if_flags & IFF_OACTIVE)
		return;

	/*
	 * Check for an available queue slot. If there are none,
	 * punt.
	 */
	if (sc->mx_cdata.mx_tx_free->mx_mbuf != NULL) {
		ifp->if_flags |= IFF_OACTIVE;
		return;
	}

	start_tx = sc->mx_cdata.mx_tx_free;

	while(sc->mx_cdata.mx_tx_free->mx_mbuf == NULL) {
		IF_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		/* Pick a descriptor off the free list. */
		cur_tx = sc->mx_cdata.mx_tx_free;
		sc->mx_cdata.mx_tx_free = cur_tx->mx_nextdesc;

		/* Pack the data into the descriptor. */
		mx_encap(sc, cur_tx, m_head);
		if (cur_tx != start_tx)
			MX_TXOWN(cur_tx) = MX_TXSTAT_OWN;

#if NBPF > 0
		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		if (ifp->if_bpf)
			bpf_mtap(ifp, cur_tx->mx_mbuf);
#endif
		MX_TXOWN(cur_tx) = MX_TXSTAT_OWN;
		CSR_WRITE_4(sc, MX_TXSTART, 0xFFFFFFFF);

	}

	/*
	 * If there are no frames queued, bail.
	 */
	if (cur_tx == NULL)
		return;

	sc->mx_cdata.mx_tx_tail = cur_tx;

	if (sc->mx_cdata.mx_tx_head == NULL)
		sc->mx_cdata.mx_tx_head = start_tx;

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;

	return;
}

static void mx_init(xsc)
	void			*xsc;
{
	struct mx_softc		*sc = xsc;
	struct ifnet		*ifp = &sc->arpcom.ac_if;
	u_int16_t		phy_bmcr = 0;
	int			s;

	if (sc->mx_autoneg)
		return;

	s = splimp();

	if (sc->mx_pinfo != NULL)
		phy_bmcr = mx_phy_readreg(sc, PHY_BMCR);

	/*
	 * Cancel pending I/O and free all RX/TX buffers.
	 */
	mx_stop(sc);
	mx_reset(sc);

	/*
	 * Set cache alignment and burst length.
	 */
	CSR_WRITE_4(sc, MX_BUSCTL, MX_BUSCTL_MUSTBEONE|MX_BUSCTL_ARBITRATION);
	MX_SETBIT(sc, MX_BUSCTL, MX_BURSTLEN_16LONG);
	switch(sc->mx_cachesize) {
	case 32:
		MX_SETBIT(sc, MX_BUSCTL, MX_CACHEALIGN_32LONG);
		break;
	case 16:
		MX_SETBIT(sc, MX_BUSCTL, MX_CACHEALIGN_16LONG);
		break; 
	case 8:
		MX_SETBIT(sc, MX_BUSCTL, MX_CACHEALIGN_8LONG);
		break;  
	case 0:
	default:
		MX_SETBIT(sc, MX_BUSCTL, MX_CACHEALIGN_NONE);
		break;
	}

	MX_SETBIT(sc, MX_NETCFG, MX_NETCFG_NO_RXCRC);
	MX_CLRBIT(sc, MX_NETCFG, MX_NETCFG_HEARTBEAT);
	MX_SETBIT(sc, MX_NETCFG, MX_NETCFG_STORENFWD);
	MX_CLRBIT(sc, MX_NETCFG, MX_NETCFG_TX_BACKOFF);

	/*
	 * The app notes for the 98713 and 98715A say that
	 * in order to have the chips operate properly, a magic
	 * number must be written to CSR16. Macronix does not
	 * document the meaning of these bits so there's no way
	 * to know exactly what they mean. The 98713 has a magic
	 * number all its own; the rest all use a different one.
	 */
	MX_CLRBIT(sc, MX_MAGICPACKET, 0xFFFF0000);
	if (sc->mx_type == MX_TYPE_98713)
		MX_SETBIT(sc, MX_MAGICPACKET, MX_MAGIC_98713);
	else
		MX_SETBIT(sc, MX_MAGICPACKET, MX_MAGIC_98715);

	if (sc->mx_pinfo != NULL) {
		MX_SETBIT(sc, MX_WATCHDOG, MX_WDOG_JABBERDIS);
		mx_setcfg(sc, mx_phy_readreg(sc, PHY_BMCR));
	} else
		mx_setmode(sc, sc->ifmedia.ifm_media, 0);

	MX_CLRBIT(sc, MX_NETCFG, MX_NETCFG_TX_THRESH);
	/*MX_CLRBIT(sc, MX_NETCFG, MX_NETCFG_SPEEDSEL);*/

	if (IFM_SUBTYPE(sc->ifmedia.ifm_media) == IFM_10_T)
		MX_SETBIT(sc, MX_NETCFG, MX_TXTHRESH_160BYTES);
	else
		MX_SETBIT(sc, MX_NETCFG, MX_TXTHRESH_72BYTES);

	/* Init circular RX list. */
	if (mx_list_rx_init(sc) == ENOBUFS) {
		printf("mx%d: initialization failed: no "
			"memory for rx buffers\n", sc->mx_unit);
		mx_stop(sc);
		(void)splx(s);
		return;
	}

	/*
	 * Init tx descriptors.
	 */
	mx_list_tx_init(sc);

	/*
	 * Load the address of the RX list.
	 */
	CSR_WRITE_4(sc, MX_RXADDR, vtophys(sc->mx_cdata.mx_rx_head->mx_ptr));

	/*
	 * Load the RX/multicast filter.
	 */
	mx_setfilt(sc);

	/*
	 * Enable interrupts.
	 */
	CSR_WRITE_4(sc, MX_IMR, MX_INTRS);
	CSR_WRITE_4(sc, MX_ISR, 0xFFFFFFFF);

	/* Enable receiver and transmitter. */
	MX_SETBIT(sc, MX_NETCFG, MX_NETCFG_TX_ON|MX_NETCFG_RX_ON);
	CSR_WRITE_4(sc, MX_RXSTART, 0xFFFFFFFF);

	/* Restore state of BMCR */
	if (sc->mx_pinfo != NULL)
		mx_phy_writereg(sc, PHY_BMCR, phy_bmcr);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	(void)splx(s);

	return;
}

/*
 * Set media options.
 */
static int mx_ifmedia_upd(ifp)
	struct ifnet		*ifp;
{
	struct mx_softc		*sc;
	struct ifmedia		*ifm;

	sc = ifp->if_softc;
	ifm = &sc->ifmedia;

	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
		return(EINVAL);

	if (sc->mx_type == MX_TYPE_98713 && sc->mx_pinfo != NULL) {
		if (IFM_SUBTYPE(ifm->ifm_media) == IFM_AUTO)
			mx_autoneg_mii(sc, MX_FLAG_SCHEDDELAY, 1);
		else
			mx_setmode_mii(sc, ifm->ifm_media);
	} else {
		if (IFM_SUBTYPE(ifm->ifm_media) == IFM_AUTO)
			mx_autoneg(sc, MX_FLAG_SCHEDDELAY, 1);
		else
			mx_setmode(sc, ifm->ifm_media, 1);
	}

	return(0);
}

/*
 * Report current media status.
 */
static void mx_ifmedia_sts(ifp, ifmr)
	struct ifnet		*ifp;
	struct ifmediareq	*ifmr;
{
	struct mx_softc		*sc;
	u_int16_t		advert = 0, ability = 0;
	u_int32_t		media = 0;

	sc = ifp->if_softc;

	ifmr->ifm_active = IFM_ETHER;

	if (sc->mx_type != MX_TYPE_98713 || sc->mx_pinfo == NULL) {
		media = CSR_READ_4(sc, MX_NETCFG);
		if (media & MX_NETCFG_PORTSEL)
			ifmr->ifm_active = IFM_ETHER|IFM_100_TX;
		else
			ifmr->ifm_active = IFM_ETHER|IFM_10_T;
		if (media & MX_NETCFG_FULLDUPLEX)
			ifmr->ifm_active |= IFM_FDX;
		else
			ifmr->ifm_active |= IFM_HDX;
		return;
	}

	if (!(mx_phy_readreg(sc, PHY_BMCR) & PHY_BMCR_AUTONEGENBL)) {
		if (mx_phy_readreg(sc, PHY_BMCR) & PHY_BMCR_SPEEDSEL)
			ifmr->ifm_active = IFM_ETHER|IFM_100_TX;
		else
			ifmr->ifm_active = IFM_ETHER|IFM_10_T;
		if (mx_phy_readreg(sc, PHY_BMCR) & PHY_BMCR_DUPLEX)
			ifmr->ifm_active |= IFM_FDX;
		else
			ifmr->ifm_active |= IFM_HDX;
		return;
	}

	ability = mx_phy_readreg(sc, PHY_LPAR);
	advert = mx_phy_readreg(sc, PHY_ANAR);
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

static int mx_ioctl(ifp, command, data)
	struct ifnet		*ifp;
	u_long			command;
	caddr_t			data;
{
	struct mx_softc		*sc = ifp->if_softc;
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
			mx_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				mx_stop(sc);
		}
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		mx_init(sc);
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

static void mx_watchdog(ifp)
	struct ifnet		*ifp;
{
	struct mx_softc		*sc;

	sc = ifp->if_softc;

	if (sc->mx_autoneg) {
		if (sc->mx_type == MX_TYPE_98713 && sc->mx_pinfo != NULL)
			mx_autoneg_mii(sc, MX_FLAG_DELAYTIMEO, 1);
		else
			mx_autoneg(sc, MX_FLAG_DELAYTIMEO, 1);
		return;
	}

	ifp->if_oerrors++;
	printf("mx%d: watchdog timeout\n", sc->mx_unit);

	if (sc->mx_pinfo == NULL) {
		if (!(CSR_READ_4(sc, MX_10BTSTAT) & MX_TSTAT_LS10) ||
			!(CSR_READ_4(sc, MX_10BTSTAT) & MX_TSTAT_LS100))
			printf("mx%d: no carrier - transceiver "
				"cable problem?\n", sc->mx_unit);
	} else {
		if (!(mx_phy_readreg(sc, PHY_BMSR) & PHY_BMSR_LINKSTAT))
			printf("mx%d: no carrier - transceiver "
				"cable problem?\n", sc->mx_unit);
	}

	mx_stop(sc);
	mx_reset(sc);
	mx_init(sc);

	if (ifp->if_snd.ifq_head != NULL)
		mx_start(ifp);

	return;
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
static void mx_stop(sc)
	struct mx_softc		*sc;
{
	register int		i;
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;
	ifp->if_timer = 0;

	MX_CLRBIT(sc, MX_NETCFG, (MX_NETCFG_RX_ON|MX_NETCFG_TX_ON));
	CSR_WRITE_4(sc, MX_IMR, 0x00000000);
	CSR_WRITE_4(sc, MX_TXADDR, 0x00000000);
	CSR_WRITE_4(sc, MX_RXADDR, 0x00000000);

	/*
	 * Free data in the RX lists.
	 */
	for (i = 0; i < MX_RX_LIST_CNT; i++) {
		if (sc->mx_cdata.mx_rx_chain[i].mx_mbuf != NULL) {
			m_freem(sc->mx_cdata.mx_rx_chain[i].mx_mbuf);
			sc->mx_cdata.mx_rx_chain[i].mx_mbuf = NULL;
		}
	}
	bzero((char *)&sc->mx_ldata->mx_rx_list,
		sizeof(sc->mx_ldata->mx_rx_list));

	/*
	 * Free the TX list buffers.
	 */
	for (i = 0; i < MX_TX_LIST_CNT; i++) {
		if (sc->mx_cdata.mx_tx_chain[i].mx_mbuf != NULL) {
			m_freem(sc->mx_cdata.mx_tx_chain[i].mx_mbuf);
			sc->mx_cdata.mx_tx_chain[i].mx_mbuf = NULL;
		}
	}

	bzero((char *)&sc->mx_ldata->mx_tx_list,
		sizeof(sc->mx_ldata->mx_tx_list));

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	return;
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
static void mx_shutdown(howto, arg)
	int			howto;
	void			*arg;
{
	struct mx_softc		*sc = (struct mx_softc *)arg;

	mx_stop(sc);

	return;
}

static struct pci_device mx_device = {
	"mx",
	mx_probe,
	mx_attach,
	&mx_count,
	NULL
};
COMPAT_PCI_DRIVER(mx, mx_device);
