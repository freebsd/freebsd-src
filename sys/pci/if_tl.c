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
 *	$Id: if_tl.c,v 1.39 1999/07/22 16:54:10 wpaul Exp $
 */

/*
 * Texas Instruments ThunderLAN driver for FreeBSD 2.2.6 and 3.x.
 * Supports many Compaq PCI NICs based on the ThunderLAN ethernet controller,
 * the National Semiconductor DP83840A physical interface and the
 * Microchip Technology 24Cxx series serial EEPROM.
 *
 * Written using the following four documents:
 *
 * Texas Instruments ThunderLAN Programmer's Guide (www.ti.com)
 * National Semiconductor DP83840A data sheet (www.national.com)
 * Microchip Technology 24C02C data sheet (www.microchip.com)
 * Micro Linear ML6692 100BaseTX only PHY data sheet (www.microlinear.com)
 * 
 * Written by Bill Paul <wpaul@ctr.columbia.edu>
 * Electrical Engineering Department
 * Columbia University, New York City
 */

/*
 * Some notes about the ThunderLAN:
 *
 * The ThunderLAN controller is a single chip containing PCI controller
 * logic, approximately 3K of on-board SRAM, a LAN controller, and media
 * independent interface (MII) bus. The MII allows the ThunderLAN chip to
 * control up to 32 different physical interfaces (PHYs). The ThunderLAN
 * also has a built-in 10baseT PHY, allowing a single ThunderLAN controller
 * to act as a complete ethernet interface.
 *
 * Other PHYs may be attached to the ThunderLAN; the Compaq 10/100 cards
 * use a National Semiconductor DP83840A PHY that supports 10 or 100Mb/sec
 * in full or half duplex. Some of the Compaq Deskpro machines use a
 * Level 1 LXT970 PHY with the same capabilities. Certain Olicom adapters
 * use a Micro Linear ML6692 100BaseTX only PHY, which can be used in
 * concert with the ThunderLAN's internal PHY to provide full 10/100
 * support. This is cheaper than using a standalone external PHY for both
 * 10/100 modes and letting the ThunderLAN's internal PHY go to waste.
 * A serial EEPROM is also attached to the ThunderLAN chip to provide
 * power-up default register settings and for storing the adapter's
 * station address. Although not supported by this driver, the ThunderLAN
 * chip can also be connected to token ring PHYs.
 *
 * The ThunderLAN has a set of registers which can be used to issue
 * commands, acknowledge interrupts, and to manipulate other internal
 * registers on its DIO bus. The primary registers can be accessed
 * using either programmed I/O (inb/outb) or via PCI memory mapping,
 * depending on how the card is configured during the PCI probing
 * phase. It is even possible to have both PIO and memory mapped
 * access turned on at the same time.
 * 
 * Frame reception and transmission with the ThunderLAN chip is done
 * using frame 'lists.' A list structure looks more or less like this:
 *
 * struct tl_frag {
 *	u_int32_t		fragment_address;
 *	u_int32_t		fragment_size;
 * };
 * struct tl_list {
 *	u_int32_t		forward_pointer;
 *	u_int16_t		cstat;
 *	u_int16_t		frame_size;
 *	struct tl_frag		fragments[10];
 * };
 *
 * The forward pointer in the list header can be either a 0 or the address
 * of another list, which allows several lists to be linked together. Each
 * list contains up to 10 fragment descriptors. This means the chip allows
 * ethernet frames to be broken up into up to 10 chunks for transfer to
 * and from the SRAM. Note that the forward pointer and fragment buffer
 * addresses are physical memory addresses, not virtual. Note also that
 * a single ethernet frame can not span lists: if the host wants to
 * transmit a frame and the frame data is split up over more than 10
 * buffers, the frame has to collapsed before it can be transmitted.
 *
 * To receive frames, the driver sets up a number of lists and populates
 * the fragment descriptors, then it sends an RX GO command to the chip.
 * When a frame is received, the chip will DMA it into the memory regions
 * specified by the fragment descriptors and then trigger an RX 'end of
 * frame interrupt' when done. The driver may choose to use only one
 * fragment per list; this may result is slighltly less efficient use
 * of memory in exchange for improving performance.
 *
 * To transmit frames, the driver again sets up lists and fragment
 * descriptors, only this time the buffers contain frame data that
 * is to be DMA'ed into the chip instead of out of it. Once the chip
 * has transfered the data into its on-board SRAM, it will trigger a
 * TX 'end of frame' interrupt. It will also generate an 'end of channel'
 * interrupt when it reaches the end of the list.
 */

/*
 * Some notes about this driver:
 *
 * The ThunderLAN chip provides a couple of different ways to organize
 * reception, transmission and interrupt handling. The simplest approach
 * is to use one list each for transmission and reception. In this mode,
 * the ThunderLAN will generate two interrupts for every received frame
 * (one RX EOF and one RX EOC) and two for each transmitted frame (one
 * TX EOF and one TX EOC). This may make the driver simpler but it hurts
 * performance to have to handle so many interrupts.
 *
 * Initially I wanted to create a circular list of receive buffers so
 * that the ThunderLAN chip would think there was an infinitely long
 * receive channel and never deliver an RXEOC interrupt. However this
 * doesn't work correctly under heavy load: while the manual says the
 * chip will trigger an RXEOF interrupt each time a frame is copied into
 * memory, you can't count on the chip waiting around for you to acknowledge
 * the interrupt before it starts trying to DMA the next frame. The result
 * is that the chip might traverse the entire circular list and then wrap
 * around before you have a chance to do anything about it. Consequently,
 * the receive list is terminated (with a 0 in the forward pointer in the
 * last element). Each time an RXEOF interrupt arrives, the used list
 * is shifted to the end of the list. This gives the appearance of an
 * infinitely large RX chain so long as the driver doesn't fall behind
 * the chip and allow all of the lists to be filled up.
 *
 * If all the lists are filled, the adapter will deliver an RX 'end of
 * channel' interrupt when it hits the 0 forward pointer at the end of
 * the chain. The RXEOC handler then cleans out the RX chain and resets
 * the list head pointer in the ch_parm register and restarts the receiver.
 *
 * For frame transmission, it is possible to program the ThunderLAN's
 * transmit interrupt threshold so that the chip can acknowledge multiple
 * lists with only a single TX EOF interrupt. This allows the driver to
 * queue several frames in one shot, and only have to handle a total
 * two interrupts (one TX EOF and one TX EOC) no matter how many frames
 * are transmitted. Frame transmission is done directly out of the
 * mbufs passed to the tl_start() routine via the interface send queue.
 * The driver simply sets up the fragment descriptors in the transmit
 * lists to point to the mbuf data regions and sends a TX GO command.
 *
 * Note that since the RX and TX lists themselves are always used
 * only by the driver, the are malloc()ed once at driver initialization
 * time and never free()ed.
 *
 * Also, in order to remain as platform independent as possible, this
 * driver uses memory mapped register access to manipulate the card
 * as opposed to programmed I/O. This avoids the use of the inb/outb
 * (and related) instructions which are specific to the i386 platform.
 *
 * Using these techniques, this driver achieves very high performance
 * by minimizing the amount of interrupts generated during large
 * transfers and by completely avoiding buffer copies. Frame transfer
 * to and from the ThunderLAN chip is performed entirely by the chip
 * itself thereby reducing the load on the host CPU.
 */

#include "bpfilter.h"

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

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <vm/vm.h>              /* for vtophys */
#include <vm/pmap.h>            /* for vtophys */
#include <machine/clock.h>      /* for DELAY */
#include <machine/bus_memio.h>
#include <machine/bus_pio.h>
#include <machine/bus.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>

/*
 * Default to using PIO register access mode to pacify certain
 * laptop docking stations with built-in ThunderLAN chips that
 * don't seem to handle memory mapped mode properly.
 */
#define TL_USEIOSPACE

/* #define TL_BACKGROUND_AUTONEG */

#include <pci/if_tlreg.h>

#if !defined(lint)
static const char rcsid[] =
	"$Id: if_tl.c,v 1.39 1999/07/22 16:54:10 wpaul Exp $";
#endif

/*
 * Various supported device vendors/types and their names.
 */

static struct tl_type tl_devs[] = {
	{ TI_VENDORID,	TI_DEVICEID_THUNDERLAN,
		"Texas Instruments ThunderLAN" },
	{ COMPAQ_VENDORID, COMPAQ_DEVICEID_NETEL_10,
		"Compaq Netelligent 10" },
	{ COMPAQ_VENDORID, COMPAQ_DEVICEID_NETEL_10_100,
		"Compaq Netelligent 10/100" },
	{ COMPAQ_VENDORID, COMPAQ_DEVICEID_NETEL_10_100_PROLIANT,
		"Compaq Netelligent 10/100 Proliant" },
	{ COMPAQ_VENDORID, COMPAQ_DEVICEID_NETEL_10_100_DUAL,
		"Compaq Netelligent 10/100 Dual Port" },
	{ COMPAQ_VENDORID, COMPAQ_DEVICEID_NETFLEX_3P_INTEGRATED,
		"Compaq NetFlex-3/P Integrated" },
	{ COMPAQ_VENDORID, COMPAQ_DEVICEID_NETFLEX_3P,
		"Compaq NetFlex-3/P" },
	{ COMPAQ_VENDORID, COMPAQ_DEVICEID_NETFLEX_3P_BNC,
		"Compaq NetFlex 3/P w/ BNC" },
	{ COMPAQ_VENDORID, COMPAQ_DEVICEID_NETEL_10_100_EMBEDDED,
		"Compaq Netelligent 10/100 TX Embedded UTP" },
	{ COMPAQ_VENDORID, COMPAQ_DEVICEID_NETEL_10_T2_UTP_COAX,
		"Compaq Netelligent 10 T/2 PCI UTP/Coax" },
	{ COMPAQ_VENDORID, COMPAQ_DEVICEID_NETEL_10_100_TX_UTP,
		"Compaq Netelligent 10/100 TX UTP" },
	{ OLICOM_VENDORID, OLICOM_DEVICEID_OC2183,
		"Olicom OC-2183/2185" },
	{ OLICOM_VENDORID, OLICOM_DEVICEID_OC2325,
		"Olicom OC-2325" },
	{ OLICOM_VENDORID, OLICOM_DEVICEID_OC2326,
		"Olicom OC-2326 10/100 TX UTP" },
	{ 0, 0, NULL }
};

/*
 * Various supported PHY vendors/types and their names. Note that
 * this driver will work with pretty much any MII-compliant PHY,
 * so failure to positively identify the chip is not a fatal error.
 */

static struct tl_type tl_phys[] = {
	{ TI_PHY_VENDORID, TI_PHY_10BT, "<TI ThunderLAN 10BT (internal)>" },
	{ TI_PHY_VENDORID, TI_PHY_100VGPMI, "<TI TNETE211 100VG Any-LAN>" },
	{ NS_PHY_VENDORID, NS_PHY_83840A, "<National Semiconductor DP83840A>"},
	{ LEVEL1_PHY_VENDORID, LEVEL1_PHY_LXT970, "<Level 1 LXT970>" }, 
	{ INTEL_PHY_VENDORID, INTEL_PHY_82555, "<Intel 82555>" },
	{ SEEQ_PHY_VENDORID, SEEQ_PHY_80220, "<SEEQ 80220>" },
	{ 0, 0, "<MII-compliant physical interface>" }
};

static unsigned long		tl_count;

static const char *tl_probe	__P((pcici_t, pcidi_t));
static void tl_attach		__P((pcici_t, int));
static int tl_attach_phy	__P((struct tl_softc *));
static int tl_intvec_rxeoc	__P((void *, u_int32_t));
static int tl_intvec_txeoc	__P((void *, u_int32_t));
static int tl_intvec_txeof	__P((void *, u_int32_t));
static int tl_intvec_rxeof	__P((void *, u_int32_t));
static int tl_intvec_adchk	__P((void *, u_int32_t));
static int tl_intvec_netsts	__P((void *, u_int32_t));

static int tl_newbuf		__P((struct tl_softc *,
					struct tl_chain_onefrag *));
static void tl_stats_update	__P((void *));
static int tl_encap		__P((struct tl_softc *, struct tl_chain *,
						struct mbuf *));

static void tl_intr		__P((void *));
static void tl_start		__P((struct ifnet *));
static int tl_ioctl		__P((struct ifnet *, u_long, caddr_t));
static void tl_init		__P((void *));
static void tl_stop		__P((struct tl_softc *));
static void tl_watchdog		__P((struct ifnet *));
static void tl_shutdown		__P((int, void *));
static int tl_ifmedia_upd	__P((struct ifnet *));
static void tl_ifmedia_sts	__P((struct ifnet *, struct ifmediareq *));

static u_int8_t tl_eeprom_putbyte	__P((struct tl_softc *, int));
static u_int8_t	tl_eeprom_getbyte	__P((struct tl_softc *,
						int, u_int8_t *));
static int tl_read_eeprom	__P((struct tl_softc *, caddr_t, int, int));

static void tl_mii_sync		__P((struct tl_softc *));
static void tl_mii_send		__P((struct tl_softc *, u_int32_t, int));
static int tl_mii_readreg	__P((struct tl_softc *, struct tl_mii_frame *));
static int tl_mii_writereg	__P((struct tl_softc *, struct tl_mii_frame *));
static u_int16_t tl_phy_readreg	__P((struct tl_softc *, int));
static void tl_phy_writereg	__P((struct tl_softc *, int, int));

static void tl_autoneg		__P((struct tl_softc *, int, int));
static void tl_setmode		__P((struct tl_softc *, int));
static int tl_calchash		__P((caddr_t));
static void tl_setmulti		__P((struct tl_softc *));
static void tl_setfilt		__P((struct tl_softc *, caddr_t, int));
static void tl_softreset	__P((struct tl_softc *, int));
static void tl_hardreset	__P((struct tl_softc *));
static int tl_list_rx_init	__P((struct tl_softc *));
static int tl_list_tx_init	__P((struct tl_softc *));

static u_int8_t tl_dio_read8	__P((struct tl_softc *, int));
static u_int16_t tl_dio_read16	__P((struct tl_softc *, int));
static u_int32_t tl_dio_read32	__P((struct tl_softc *, int));
static void tl_dio_write8	__P((struct tl_softc *, int, int));
static void tl_dio_write16	__P((struct tl_softc *, int, int));
static void tl_dio_write32	__P((struct tl_softc *, int, int));
static void tl_dio_setbit	__P((struct tl_softc *, int, int));
static void tl_dio_clrbit	__P((struct tl_softc *, int, int));
static void tl_dio_setbit16	__P((struct tl_softc *, int, int));
static void tl_dio_clrbit16	__P((struct tl_softc *, int, int));

static u_int8_t tl_dio_read8(sc, reg)
	struct tl_softc		*sc;
	int			reg;
{
	CSR_WRITE_2(sc, TL_DIO_ADDR, reg);
	return(CSR_READ_1(sc, TL_DIO_DATA + (reg & 3)));
}

static u_int16_t tl_dio_read16(sc, reg)
	struct tl_softc		*sc;
	int			reg;
{
	CSR_WRITE_2(sc, TL_DIO_ADDR, reg);
	return(CSR_READ_2(sc, TL_DIO_DATA + (reg & 3)));
}

static u_int32_t tl_dio_read32(sc, reg)
	struct tl_softc		*sc;
	int			reg;
{
	CSR_WRITE_2(sc, TL_DIO_ADDR, reg);
	return(CSR_READ_4(sc, TL_DIO_DATA + (reg & 3)));
}

static void tl_dio_write8(sc, reg, val)
	struct tl_softc		*sc;
	int			reg;
	int			val;
{
	CSR_WRITE_2(sc, TL_DIO_ADDR, reg);
	CSR_WRITE_1(sc, TL_DIO_DATA + (reg & 3), val);
	return;
}

static void tl_dio_write16(sc, reg, val)
	struct tl_softc		*sc;
	int			reg;
	int			val;
{
	CSR_WRITE_2(sc, TL_DIO_ADDR, reg);
	CSR_WRITE_2(sc, TL_DIO_DATA + (reg & 3), val);
	return;
}

static void tl_dio_write32(sc, reg, val)
	struct tl_softc		*sc;
	int			reg;
	int			val;
{
	CSR_WRITE_2(sc, TL_DIO_ADDR, reg);
	CSR_WRITE_4(sc, TL_DIO_DATA + (reg & 3), val);
	return;
}

static void tl_dio_setbit(sc, reg, bit)
	struct tl_softc		*sc;
	int			reg;
	int			bit;
{
	u_int8_t			f;

	CSR_WRITE_2(sc, TL_DIO_ADDR, reg);
	f = CSR_READ_1(sc, TL_DIO_DATA + (reg & 3));
	f |= bit;
	CSR_WRITE_1(sc, TL_DIO_DATA + (reg & 3), f);

	return;
}

static void tl_dio_clrbit(sc, reg, bit)
	struct tl_softc		*sc;
	int			reg;
	int			bit;
{
	u_int8_t			f;

	CSR_WRITE_2(sc, TL_DIO_ADDR, reg);
	f = CSR_READ_1(sc, TL_DIO_DATA + (reg & 3));
	f &= ~bit;
	CSR_WRITE_1(sc, TL_DIO_DATA + (reg & 3), f);

	return;
}

static void tl_dio_setbit16(sc, reg, bit)
	struct tl_softc		*sc;
	int			reg;
	int			bit;
{
	u_int16_t			f;

	CSR_WRITE_2(sc, TL_DIO_ADDR, reg);
	f = CSR_READ_2(sc, TL_DIO_DATA + (reg & 3));
	f |= bit;
	CSR_WRITE_2(sc, TL_DIO_DATA + (reg & 3), f);

	return;
}

static void tl_dio_clrbit16(sc, reg, bit)
	struct tl_softc		*sc;
	int			reg;
	int			bit;
{
	u_int16_t			f;

	CSR_WRITE_2(sc, TL_DIO_ADDR, reg);
	f = CSR_READ_2(sc, TL_DIO_DATA + (reg & 3));
	f &= ~bit;
	CSR_WRITE_2(sc, TL_DIO_DATA + (reg & 3), f);

	return;
}

/*
 * Send an instruction or address to the EEPROM, check for ACK.
 */
static u_int8_t tl_eeprom_putbyte(sc, byte)
	struct tl_softc		*sc;
	int			byte;
{
	register int		i, ack = 0;

	/*
	 * Make sure we're in TX mode.
	 */
	tl_dio_setbit(sc, TL_NETSIO, TL_SIO_ETXEN);

	/*
	 * Feed in each bit and stobe the clock.
	 */
	for (i = 0x80; i; i >>= 1) {
		if (byte & i) {
			tl_dio_setbit(sc, TL_NETSIO, TL_SIO_EDATA);
		} else {
			tl_dio_clrbit(sc, TL_NETSIO, TL_SIO_EDATA);
		}
		DELAY(1);
		tl_dio_setbit(sc, TL_NETSIO, TL_SIO_ECLOK);
		DELAY(1);
		tl_dio_clrbit(sc, TL_NETSIO, TL_SIO_ECLOK);
	}

	/*
	 * Turn off TX mode.
	 */
	tl_dio_clrbit(sc, TL_NETSIO, TL_SIO_ETXEN);

	/*
	 * Check for ack.
	 */
	tl_dio_setbit(sc, TL_NETSIO, TL_SIO_ECLOK);
	ack = tl_dio_read8(sc, TL_NETSIO) & TL_SIO_EDATA;
	tl_dio_clrbit(sc, TL_NETSIO, TL_SIO_ECLOK);

	return(ack);
}

/*
 * Read a byte of data stored in the EEPROM at address 'addr.'
 */
static u_int8_t tl_eeprom_getbyte(sc, addr, dest)
	struct tl_softc		*sc;
	int			addr;
	u_int8_t		*dest;
{
	register int		i;
	u_int8_t		byte = 0;

	tl_dio_write8(sc, TL_NETSIO, 0);

	EEPROM_START;

	/*
	 * Send write control code to EEPROM.
	 */
	if (tl_eeprom_putbyte(sc, EEPROM_CTL_WRITE)) {
		printf("tl%d: failed to send write command, status: %x\n",
				sc->tl_unit, tl_dio_read8(sc, TL_NETSIO));
		return(1);
	}

	/*
	 * Send address of byte we want to read.
	 */
	if (tl_eeprom_putbyte(sc, addr)) {
		printf("tl%d: failed to send address, status: %x\n",
				sc->tl_unit, tl_dio_read8(sc, TL_NETSIO));
		return(1);
	}

	EEPROM_STOP;
	EEPROM_START;
	/*
	 * Send read control code to EEPROM.
	 */
	if (tl_eeprom_putbyte(sc, EEPROM_CTL_READ)) {
		printf("tl%d: failed to send write command, status: %x\n",
				sc->tl_unit, tl_dio_read8(sc, TL_NETSIO));
		return(1);
	}

	/*
	 * Start reading bits from EEPROM.
	 */
	tl_dio_clrbit(sc, TL_NETSIO, TL_SIO_ETXEN);
	for (i = 0x80; i; i >>= 1) {
		tl_dio_setbit(sc, TL_NETSIO, TL_SIO_ECLOK);
		DELAY(1);
		if (tl_dio_read8(sc, TL_NETSIO) & TL_SIO_EDATA)
			byte |= i;
		tl_dio_clrbit(sc, TL_NETSIO, TL_SIO_ECLOK);
		DELAY(1);
	}

	EEPROM_STOP;

	/*
	 * No ACK generated for read, so just return byte.
	 */

	*dest = byte;

	return(0);
}

/*
 * Read a sequence of bytes from the EEPROM.
 */
static int tl_read_eeprom(sc, dest, off, cnt)
	struct tl_softc		*sc;
	caddr_t			dest;
	int			off;
	int			cnt;
{
	int			err = 0, i;
	u_int8_t		byte = 0;

	for (i = 0; i < cnt; i++) {
		err = tl_eeprom_getbyte(sc, off + i, &byte);
		if (err)
			break;
		*(dest + i) = byte;
	}

	return(err ? 1 : 0);
}

static void tl_mii_sync(sc)
	struct tl_softc		*sc;
{
	register int		i;

	tl_dio_clrbit(sc, TL_NETSIO, TL_SIO_MTXEN);

	for (i = 0; i < 32; i++) {
		tl_dio_setbit(sc, TL_NETSIO, TL_SIO_MCLK);
		tl_dio_clrbit(sc, TL_NETSIO, TL_SIO_MCLK);
	}

	return;
}

static void tl_mii_send(sc, bits, cnt)
	struct tl_softc		*sc;
	u_int32_t		bits;
	int			cnt;
{
	int			i;

	for (i = (0x1 << (cnt - 1)); i; i >>= 1) {
		tl_dio_clrbit(sc, TL_NETSIO, TL_SIO_MCLK);
		if (bits & i) {
			tl_dio_setbit(sc, TL_NETSIO, TL_SIO_MDATA);
		} else {
			tl_dio_clrbit(sc, TL_NETSIO, TL_SIO_MDATA);
		}
		tl_dio_setbit(sc, TL_NETSIO, TL_SIO_MCLK);
	}
}

static int tl_mii_readreg(sc, frame)
	struct tl_softc		*sc;
	struct tl_mii_frame	*frame;
	
{
	int			i, ack, s;
	int			minten = 0;

	s = splimp();

	tl_mii_sync(sc);

	/*
	 * Set up frame for RX.
	 */
	frame->mii_stdelim = TL_MII_STARTDELIM;
	frame->mii_opcode = TL_MII_READOP;
	frame->mii_turnaround = 0;
	frame->mii_data = 0;
	
	/*
	 * Turn off MII interrupt by forcing MINTEN low.
	 */
	minten = tl_dio_read8(sc, TL_NETSIO) & TL_SIO_MINTEN;
	if (minten) {
		tl_dio_clrbit(sc, TL_NETSIO, TL_SIO_MINTEN);
	}

	/*
 	 * Turn on data xmit.
	 */
	tl_dio_setbit(sc, TL_NETSIO, TL_SIO_MTXEN);

	/*
	 * Send command/address info.
	 */
	tl_mii_send(sc, frame->mii_stdelim, 2);
	tl_mii_send(sc, frame->mii_opcode, 2);
	tl_mii_send(sc, frame->mii_phyaddr, 5);
	tl_mii_send(sc, frame->mii_regaddr, 5);

	/*
	 * Turn off xmit.
	 */
	tl_dio_clrbit(sc, TL_NETSIO, TL_SIO_MTXEN);

	/* Idle bit */
	tl_dio_clrbit(sc, TL_NETSIO, TL_SIO_MCLK);
	tl_dio_setbit(sc, TL_NETSIO, TL_SIO_MCLK);

	/* Check for ack */
	tl_dio_clrbit(sc, TL_NETSIO, TL_SIO_MCLK);
	ack = tl_dio_read8(sc, TL_NETSIO) & TL_SIO_MDATA;

	/* Complete the cycle */
	tl_dio_setbit(sc, TL_NETSIO, TL_SIO_MCLK);

	/*
	 * Now try reading data bits. If the ack failed, we still
	 * need to clock through 16 cycles to keep the PHYs in sync.
	 */
	if (ack) {
		for(i = 0; i < 16; i++) {
			tl_dio_clrbit(sc, TL_NETSIO, TL_SIO_MCLK);
			tl_dio_setbit(sc, TL_NETSIO, TL_SIO_MCLK);
		}
		goto fail;
	}

	for (i = 0x8000; i; i >>= 1) {
		tl_dio_clrbit(sc, TL_NETSIO, TL_SIO_MCLK);
		if (!ack) {
			if (tl_dio_read8(sc, TL_NETSIO) & TL_SIO_MDATA)
				frame->mii_data |= i;
		}
		tl_dio_setbit(sc, TL_NETSIO, TL_SIO_MCLK);
	}

fail:

	tl_dio_setbit(sc, TL_NETSIO, TL_SIO_MCLK);
	tl_dio_clrbit(sc, TL_NETSIO, TL_SIO_MCLK);

	/* Reenable interrupts */
	if (minten) {
		tl_dio_setbit(sc, TL_NETSIO, TL_SIO_MINTEN);
	}

	splx(s);

	if (ack)
		return(1);
	return(0);
}

static int tl_mii_writereg(sc, frame)
	struct tl_softc		*sc;
	struct tl_mii_frame	*frame;
	
{
	int			s;
	int			minten;

	tl_mii_sync(sc);

	s = splimp();
	/*
	 * Set up frame for TX.
	 */

	frame->mii_stdelim = TL_MII_STARTDELIM;
	frame->mii_opcode = TL_MII_WRITEOP;
	frame->mii_turnaround = TL_MII_TURNAROUND;
	
	/*
	 * Turn off MII interrupt by forcing MINTEN low.
	 */
	minten = tl_dio_read8(sc, TL_NETSIO) & TL_SIO_MINTEN;
	if (minten) {
		tl_dio_clrbit(sc, TL_NETSIO, TL_SIO_MINTEN);
	}

	/*
 	 * Turn on data output.
	 */
	tl_dio_setbit(sc, TL_NETSIO, TL_SIO_MTXEN);

	tl_mii_send(sc, frame->mii_stdelim, 2);
	tl_mii_send(sc, frame->mii_opcode, 2);
	tl_mii_send(sc, frame->mii_phyaddr, 5);
	tl_mii_send(sc, frame->mii_regaddr, 5);
	tl_mii_send(sc, frame->mii_turnaround, 2);
	tl_mii_send(sc, frame->mii_data, 16);

	tl_dio_setbit(sc, TL_NETSIO, TL_SIO_MCLK);
	tl_dio_clrbit(sc, TL_NETSIO, TL_SIO_MCLK);

	/*
	 * Turn off xmit.
	 */
	tl_dio_clrbit(sc, TL_NETSIO, TL_SIO_MTXEN);

	/* Reenable interrupts */
	if (minten)
		tl_dio_setbit(sc, TL_NETSIO, TL_SIO_MINTEN);

	splx(s);

	return(0);
}

static u_int16_t tl_phy_readreg(sc, reg)
	struct tl_softc		*sc;
	int			reg;
{
	struct tl_mii_frame	frame;

	bzero((char *)&frame, sizeof(frame));

	frame.mii_phyaddr = sc->tl_phy_addr;
	frame.mii_regaddr = reg;
	tl_mii_readreg(sc, &frame);

	/* Reenable MII interrupts, just in case. */
	tl_dio_setbit(sc, TL_NETSIO, TL_SIO_MINTEN);

	return(frame.mii_data);
}

static void tl_phy_writereg(sc, reg, data)
	struct tl_softc		*sc;
	int			reg;
	int			data;
{
	struct tl_mii_frame	frame;

	bzero((char *)&frame, sizeof(frame));

	frame.mii_phyaddr = sc->tl_phy_addr;
	frame.mii_regaddr = reg;
	frame.mii_data = data;

	tl_mii_writereg(sc, &frame);

	/* Reenable MII interrupts, just in case. */
	tl_dio_setbit(sc, TL_NETSIO, TL_SIO_MINTEN);

	return;
}

/*
 * Initiate autonegotiation with a link partner.
 *
 * Note that the Texas Instruments ThunderLAN programmer's guide
 * fails to mention one very important point about autonegotiation.
 * Autonegotiation is done largely by the PHY, independent of the
 * ThunderLAN chip itself: the PHY sets the flags in the BMCR
 * register to indicate what modes were selected and if link status
 * is good. In fact, the PHY does pretty much all of the work itself,
 * except for one small detail.
 *
 * The PHY may negotiate a full-duplex of half-duplex link, and set
 * the PHY_BMCR_DUPLEX bit accordingly, but the ThunderLAN's 'NetCommand'
 * register _also_ has a half-duplex/full-duplex bit, and you MUST ALSO
 * SET THIS BIT MANUALLY TO CORRESPOND TO THE MODE SELECTED FOR THE PHY!
 * In other words, both the ThunderLAN chip and the PHY have to be
 * programmed for full-duplex mode in order for full-duplex to actually
 * work. So in order for autonegotiation to really work right, we have
 * to wait for the link to come up, check the BMCR register, then set
 * the ThunderLAN for full or half-duplex as needed.
 *
 * I struggled for two days to figure this out, so I'm making a point
 * of drawing attention to this fact. I think it's very strange that
 * the ThunderLAN doesn't automagically track the duplex state of the
 * PHY, but there you have it.
 *
 * Also when, using a National Semiconductor DP83840A PHY, we have to
 * allow a full three seconds for autonegotiation to complete. So what
 * we do is flip the autonegotiation restart bit, then set a timeout
 * to wake us up in three seconds to check the link state.
 *
 * Note that there are some versions of the Olicom 2326 that use a
 * Micro Linear ML6692 100BaseTX PHY. This particular PHY is designed
 * to provide 100BaseTX support only, but can be used with a controller
 * that supports an internal 10Mbps PHY to provide a complete
 * 10/100Mbps solution. However, the ML6692 does not have vendor and
 * device ID registers, and hence always shows up with a vendor/device
 * ID of 0.
 *
 * We detect this configuration by checking the phy vendor ID in the
 * softc structure. If it's a zero, and we're negotiating a high-speed
 * mode, then we turn off the internal PHY. If it's a zero and we've
 * negotiated a high-speed mode, we turn on the internal PHY. Note
 * that to make things even more fun, we have to make extra sure that
 * the loopback bit in the internal PHY's control register is turned
 * off.
 */
static void tl_autoneg(sc, flag, verbose)
	struct tl_softc		*sc;
	int			flag;
	int			verbose;
{
	u_int16_t		phy_sts = 0, media = 0, advert, ability;
	struct ifnet		*ifp;
	struct ifmedia		*ifm;

	ifm = &sc->ifmedia;
	ifp = &sc->arpcom.ac_if;

	/*
	 * First, see if autoneg is supported. If not, there's
	 * no point in continuing.
	 */
	phy_sts = tl_phy_readreg(sc, PHY_BMSR);
	if (!(phy_sts & PHY_BMSR_CANAUTONEG)) {
		if (verbose)
			printf("tl%d: autonegotiation not supported\n",
							sc->tl_unit);
		return;
	}

	switch (flag) {
	case TL_FLAG_FORCEDELAY:
		/*
	 	 * XXX Never use this option anywhere but in the probe
	 	 * routine: making the kernel stop dead in its tracks
 		 * for three whole seconds after we've gone multi-user
		 * is really bad manners.
	 	 */
		tl_phy_writereg(sc, PHY_BMCR, PHY_BMCR_RESET);
		DELAY(500);
		phy_sts = tl_phy_readreg(sc, PHY_BMCR);
		phy_sts |= PHY_BMCR_AUTONEGENBL|PHY_BMCR_AUTONEGRSTR;
		tl_phy_writereg(sc, PHY_BMCR, phy_sts);
		DELAY(5000000);
		break;
	case TL_FLAG_SCHEDDELAY:
		/*
		 * Wait for the transmitter to go idle before starting
		 * an autoneg session, otherwise tl_start() may clobber
	 	 * our timeout, and we don't want to allow transmission
		 * during an autoneg session since that can screw it up.
	 	 */
		if (!sc->tl_txeoc) {
			sc->tl_want_auto = 1;
			return;
		}
		tl_phy_writereg(sc, PHY_BMCR, PHY_BMCR_RESET);
		DELAY(500);
		phy_sts = tl_phy_readreg(sc, PHY_BMCR);
		phy_sts |= PHY_BMCR_AUTONEGENBL|PHY_BMCR_AUTONEGRSTR;
		tl_phy_writereg(sc, PHY_BMCR, phy_sts);
		ifp->if_timer = 5;
		sc->tl_autoneg = 1;
		sc->tl_want_auto = 0;
		return;
	case TL_FLAG_DELAYTIMEO:
		ifp->if_timer = 0;
		sc->tl_autoneg = 0;
		break;
	default:
		printf("tl%d: invalid autoneg flag: %d\n", sc->tl_unit, flag);
		return;
	}

	/*
 	 * Read the BMSR register twice: the LINKSTAT bit is a
	 * latching bit.
	 */
	tl_phy_readreg(sc, PHY_BMSR);
	phy_sts = tl_phy_readreg(sc, PHY_BMSR);
	if (phy_sts & PHY_BMSR_AUTONEGCOMP) {
		if (verbose)
			printf("tl%d: autoneg complete, ", sc->tl_unit);
		phy_sts = tl_phy_readreg(sc, PHY_BMSR);
	} else {
		if (verbose)
			printf("tl%d: autoneg not complete, ", sc->tl_unit);
	}

	/* Link is good. Report modes and set duplex mode. */
	if (phy_sts & PHY_BMSR_LINKSTAT) {
		if (verbose)
			printf("link status good ");

		advert = tl_phy_readreg(sc, TL_PHY_ANAR);
		ability = tl_phy_readreg(sc, TL_PHY_LPAR);
		media = tl_phy_readreg(sc, PHY_BMCR);

		/*
	 	 * Be sure to turn off the ISOLATE and
		 * LOOPBACK bits in the control register,
		 * otherwise we may not be able to communicate.
		 */
		media &= ~(PHY_BMCR_LOOPBK|PHY_BMCR_ISOLATE);
		/* Set the DUPLEX bit in the NetCmd register accordingly. */
		if (advert & PHY_ANAR_100BT4 && ability & PHY_ANAR_100BT4) {
			ifm->ifm_media = IFM_ETHER|IFM_100_T4;
			media |= PHY_BMCR_SPEEDSEL;
			media &= ~PHY_BMCR_DUPLEX;
			if (verbose)
				printf("(100baseT4)\n");
		} else if (advert & PHY_ANAR_100BTXFULL &&
			ability & PHY_ANAR_100BTXFULL) {
			ifm->ifm_media = IFM_ETHER|IFM_100_TX|IFM_FDX;
			media |= PHY_BMCR_SPEEDSEL;
			media |= PHY_BMCR_DUPLEX;
			if (verbose)
				printf("(full-duplex, 100Mbps)\n");
		} else if (advert & PHY_ANAR_100BTXHALF &&
			ability & PHY_ANAR_100BTXHALF) {
			ifm->ifm_media = IFM_ETHER|IFM_100_TX|IFM_HDX;
			media |= PHY_BMCR_SPEEDSEL;
			media &= ~PHY_BMCR_DUPLEX;
			if (verbose)
				printf("(half-duplex, 100Mbps)\n");
		} else if (advert & PHY_ANAR_10BTFULL &&
			ability & PHY_ANAR_10BTFULL) {
			ifm->ifm_media = IFM_ETHER|IFM_10_T|IFM_FDX;
			media &= ~PHY_BMCR_SPEEDSEL;
			media |= PHY_BMCR_DUPLEX;
			if (verbose)
				printf("(full-duplex, 10Mbps)\n");
		} else {
			ifm->ifm_media = IFM_ETHER|IFM_10_T|IFM_HDX;
			media &= ~PHY_BMCR_SPEEDSEL;
			media &= ~PHY_BMCR_DUPLEX;
			if (verbose)
				printf("(half-duplex, 10Mbps)\n");
		}

		if (media & PHY_BMCR_DUPLEX)
			tl_dio_setbit(sc, TL_NETCMD, TL_CMD_DUPLEX);
		else
			tl_dio_clrbit(sc, TL_NETCMD, TL_CMD_DUPLEX);

		media &= ~PHY_BMCR_AUTONEGENBL;
		tl_phy_writereg(sc, PHY_BMCR, media);
	} else {
		if (verbose)
			printf("no carrier\n");
	}

	tl_init(sc);

	if (sc->tl_tx_pend) {
		sc->tl_autoneg = 0;
		sc->tl_tx_pend = 0;
		tl_start(ifp);
	}

	return;
}

/*
 * Set speed and duplex mode. Also program autoneg advertisements
 * accordingly.
 */
static void tl_setmode(sc, media)
	struct tl_softc		*sc;
	int			media;
{
	u_int16_t		bmcr;

	if (sc->tl_bitrate) {
		if (IFM_SUBTYPE(media) == IFM_10_5)
			tl_dio_setbit(sc, TL_ACOMMIT, TL_AC_MTXD1);
		if (IFM_SUBTYPE(media) == IFM_10_T) {
			tl_dio_clrbit(sc, TL_ACOMMIT, TL_AC_MTXD1);
			if ((media & IFM_GMASK) == IFM_FDX) {
				tl_dio_clrbit(sc, TL_ACOMMIT, TL_AC_MTXD3);
				tl_dio_setbit(sc, TL_NETCMD, TL_CMD_DUPLEX);
			} else {
				tl_dio_setbit(sc, TL_ACOMMIT, TL_AC_MTXD3);
				tl_dio_clrbit(sc, TL_NETCMD, TL_CMD_DUPLEX);
			}
		}
		return;
	}

	bmcr = tl_phy_readreg(sc, PHY_BMCR);

	bmcr &= ~(PHY_BMCR_SPEEDSEL|PHY_BMCR_DUPLEX|PHY_BMCR_AUTONEGENBL|
		  PHY_BMCR_LOOPBK|PHY_BMCR_ISOLATE);

	if (IFM_SUBTYPE(media) == IFM_LOOP)
		bmcr |= PHY_BMCR_LOOPBK;

	if (IFM_SUBTYPE(media) == IFM_AUTO)
		bmcr |= PHY_BMCR_AUTONEGENBL;

	/*
	 * The ThunderLAN's internal PHY has an AUI transceiver
	 * that can be selected. This is usually attached to a
	 * 10base2/BNC port. In order to activate this port, we
	 * have to set the AUISEL bit in the internal PHY's
	 * special control register.
	 */
	if (IFM_SUBTYPE(media) == IFM_10_5) {
		u_int16_t		addr, ctl;
		addr = sc->tl_phy_addr;
		sc->tl_phy_addr = TL_PHYADDR_MAX;
		ctl = tl_phy_readreg(sc, TL_PHY_CTL);
		ctl |= PHY_CTL_AUISEL;
		tl_phy_writereg(sc, TL_PHY_CTL, ctl);
		tl_phy_writereg(sc, PHY_BMCR, bmcr);
		sc->tl_phy_addr = addr;
		bmcr |= PHY_BMCR_ISOLATE;
	} else {
		u_int16_t		addr, ctl;
		addr = sc->tl_phy_addr;
		sc->tl_phy_addr = TL_PHYADDR_MAX;
		ctl = tl_phy_readreg(sc, TL_PHY_CTL);
		ctl &= ~PHY_CTL_AUISEL;
		tl_phy_writereg(sc, TL_PHY_CTL, ctl);
		tl_phy_writereg(sc, PHY_BMCR, PHY_BMCR_ISOLATE);
		sc->tl_phy_addr = addr;
		bmcr &= ~PHY_BMCR_ISOLATE;
	}

	if (IFM_SUBTYPE(media) == IFM_100_TX) {
		bmcr |= PHY_BMCR_SPEEDSEL;
		if ((media & IFM_GMASK) == IFM_FDX) {
			bmcr |= PHY_BMCR_DUPLEX;
			tl_dio_setbit(sc, TL_NETCMD, TL_CMD_DUPLEX);
		} else {
			bmcr &= ~PHY_BMCR_DUPLEX;
			tl_dio_clrbit(sc, TL_NETCMD, TL_CMD_DUPLEX);
		}
	}

	if (IFM_SUBTYPE(media) == IFM_10_T) {
		bmcr &= ~PHY_BMCR_SPEEDSEL;
		if ((media & IFM_GMASK) == IFM_FDX) {
			bmcr |= PHY_BMCR_DUPLEX;
			tl_dio_setbit(sc, TL_NETCMD, TL_CMD_DUPLEX);
		} else {
			bmcr &= ~PHY_BMCR_DUPLEX;
			tl_dio_clrbit(sc, TL_NETCMD, TL_CMD_DUPLEX);
		}
	}

	tl_phy_writereg(sc, PHY_BMCR, bmcr);

	tl_init(sc);

	return;
}

/*
 * Calculate the hash of a MAC address for programming the multicast hash
 * table.  This hash is simply the address split into 6-bit chunks
 * XOR'd, e.g.
 * byte: 000000|00 1111|1111 22|222222|333333|33 4444|4444 55|555555
 * bit:  765432|10 7654|3210 76|543210|765432|10 7654|3210 76|543210
 * Bytes 0-2 and 3-5 are symmetrical, so are folded together.  Then
 * the folded 24-bit value is split into 6-bit portions and XOR'd.
 */
static int tl_calchash(addr)
	caddr_t			addr;
{
	int			t;

	t = (addr[0] ^ addr[3]) << 16 | (addr[1] ^ addr[4]) << 8 |
		(addr[2] ^ addr[5]);
	return ((t >> 18) ^ (t >> 12) ^ (t >> 6) ^ t) & 0x3f;
}

/*
 * The ThunderLAN has a perfect MAC address filter in addition to
 * the multicast hash filter. The perfect filter can be programmed
 * with up to four MAC addresses. The first one is always used to
 * hold the station address, which leaves us free to use the other
 * three for multicast addresses.
 */
static void tl_setfilt(sc, addr, slot)
	struct tl_softc		*sc;
	caddr_t			addr;
	int			slot;
{
	int			i;
	u_int16_t		regaddr;

	regaddr = TL_AREG0_B5 + (slot * ETHER_ADDR_LEN);

	for (i = 0; i < ETHER_ADDR_LEN; i++)
		tl_dio_write8(sc, regaddr + i, *(addr + i));

	return;
}

/*
 * XXX In FreeBSD 3.0, multicast addresses are managed using a doubly
 * linked list. This is fine, except addresses are added from the head
 * end of the list. We want to arrange for 224.0.0.1 (the "all hosts")
 * group to always be in the perfect filter, but as more groups are added,
 * the 224.0.0.1 entry (which is always added first) gets pushed down
 * the list and ends up at the tail. So after 3 or 4 multicast groups
 * are added, the all-hosts entry gets pushed out of the perfect filter
 * and into the hash table.
 *
 * Because the multicast list is a doubly-linked list as opposed to a
 * circular queue, we don't have the ability to just grab the tail of
 * the list and traverse it backwards. Instead, we have to traverse
 * the list once to find the tail, then traverse it again backwards to
 * update the multicast filter.
 */
static void tl_setmulti(sc)
	struct tl_softc		*sc;
{
	struct ifnet		*ifp;
	u_int32_t		hashes[2] = { 0, 0 };
	int			h, i;
	struct ifmultiaddr	*ifma;
	u_int8_t		dummy[] = { 0, 0, 0, 0, 0 ,0 };
	ifp = &sc->arpcom.ac_if;

	/* First, zot all the existing filters. */
	for (i = 1; i < 4; i++)
		tl_setfilt(sc, (caddr_t)&dummy, i);
	tl_dio_write32(sc, TL_HASH1, 0);
	tl_dio_write32(sc, TL_HASH2, 0);

	/* Now program new ones. */
	if (ifp->if_flags & IFF_ALLMULTI) {
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
			 * Program the first three multicast groups
			 * into the perfect filter. For all others,
			 * use the hash table.
			 */
			if (i < 4) {
				tl_setfilt(sc,
			LLADDR((struct sockaddr_dl *)ifma->ifma_addr), i);
				i++;
				continue;
			}

			h = tl_calchash(
				LLADDR((struct sockaddr_dl *)ifma->ifma_addr));
			if (h < 32)
				hashes[0] |= (1 << h);
			else
				hashes[1] |= (1 << (h - 32));
		}
	}

	tl_dio_write32(sc, TL_HASH1, hashes[0]);
	tl_dio_write32(sc, TL_HASH2, hashes[1]);

	return;
}

/*
 * This routine is recommended by the ThunderLAN manual to insure that
 * the internal PHY is powered up correctly. It also recommends a one
 * second pause at the end to 'wait for the clocks to start' but in my
 * experience this isn't necessary.
 */
static void tl_hardreset(sc)
	struct tl_softc		*sc;
{
	int			i;
	u_int16_t		old_addr, flags;

	old_addr = sc->tl_phy_addr;

	for (i = 0; i < TL_PHYADDR_MAX + 1; i++) {
		sc->tl_phy_addr = i;
		tl_mii_sync(sc);
	}

	flags = PHY_BMCR_LOOPBK|PHY_BMCR_ISOLATE|PHY_BMCR_PWRDOWN;

	for (i = 0; i < TL_PHYADDR_MAX + 1; i++) {
		sc->tl_phy_addr = i;
		tl_phy_writereg(sc, PHY_BMCR, flags);
	}

	sc->tl_phy_addr = TL_PHYADDR_MAX;
	tl_phy_writereg(sc, PHY_BMCR, PHY_BMCR_ISOLATE);

	DELAY(50000);

	tl_phy_writereg(sc, PHY_BMCR, PHY_BMCR_LOOPBK|PHY_BMCR_ISOLATE);

	tl_mii_sync(sc);

	while(tl_phy_readreg(sc, PHY_BMCR) & PHY_BMCR_RESET);

	sc->tl_phy_addr = old_addr;

	return;
}

static void tl_softreset(sc, internal)
	struct tl_softc		*sc;
	int			internal;
{
        u_int32_t               cmd, dummy, i;

        /* Assert the adapter reset bit. */
	CMD_SET(sc, TL_CMD_ADRST);
        /* Turn off interrupts */
	CMD_SET(sc, TL_CMD_INTSOFF);

	/* First, clear the stats registers. */
	for (i = 0; i < 5; i++)
		dummy = tl_dio_read32(sc, TL_TXGOODFRAMES);

        /* Clear Areg and Hash registers */
	for (i = 0; i < 8; i++)
		tl_dio_write32(sc, TL_AREG0_B5, 0x00000000);

        /*
	 * Set up Netconfig register. Enable one channel and
	 * one fragment mode.
	 */
	tl_dio_setbit16(sc, TL_NETCONFIG, TL_CFG_ONECHAN|TL_CFG_ONEFRAG);
	if (internal && !sc->tl_bitrate) {
		tl_dio_setbit16(sc, TL_NETCONFIG, TL_CFG_PHYEN);
	} else {
		tl_dio_clrbit16(sc, TL_NETCONFIG, TL_CFG_PHYEN);
	}

	/* Handle cards with bitrate devices. */
	if (sc->tl_bitrate)
		tl_dio_setbit16(sc, TL_NETCONFIG, TL_CFG_BITRATE);

        /* Set PCI burst size */
	tl_dio_write8(sc, TL_BSIZEREG, 0x33);

	/*
	 * Load adapter irq pacing timer and tx threshold.
	 * We make the transmit threshold 1 initially but we may
	 * change that later.
	 */
	cmd = CSR_READ_4(sc, TL_HOSTCMD);
	cmd |= TL_CMD_NES;
	cmd &= ~(TL_CMD_RT|TL_CMD_EOC|TL_CMD_ACK_MASK|TL_CMD_CHSEL_MASK);
	CMD_PUT(sc, cmd | (TL_CMD_LDTHR | TX_THR));
	CMD_PUT(sc, cmd | (TL_CMD_LDTMR | 0x00000003));

        /* Unreset the MII */
	tl_dio_setbit(sc, TL_NETSIO, TL_SIO_NMRST);

	/* Clear status register */
        tl_dio_setbit16(sc, TL_NETSTS, TL_STS_MIRQ);
        tl_dio_setbit16(sc, TL_NETSTS, TL_STS_HBEAT);
        tl_dio_setbit16(sc, TL_NETSTS, TL_STS_TXSTOP);
        tl_dio_setbit16(sc, TL_NETSTS, TL_STS_RXSTOP);

	/* Enable network status interrupts for everything. */
	tl_dio_setbit(sc, TL_NETMASK, TL_MASK_MASK7|TL_MASK_MASK6|
			TL_MASK_MASK5|TL_MASK_MASK4);

	/* Take the adapter out of reset */
	tl_dio_setbit(sc, TL_NETCMD, TL_CMD_NRESET|TL_CMD_NWRAP);

	/* Wait for things to settle down a little. */
	DELAY(500);

        return;
}

/*
 * Probe for a ThunderLAN chip. Check the PCI vendor and device IDs
 * against our list and return its name if we find a match.
 */
static const char *
tl_probe(config_id, device_id)
	pcici_t			config_id;
	pcidi_t			device_id;
{
	struct tl_type		*t;

	t = tl_devs;

	while(t->tl_name != NULL) {
		if ((device_id & 0xFFFF) == t->tl_vid &&
		    ((device_id >> 16) & 0xFFFF) == t->tl_did)
			return(t->tl_name);
		t++;
	}

	return(NULL);
}

/*
 * Do the interface setup and attach for a PHY on a particular
 * ThunderLAN chip. Also also set up interrupt vectors.
 */ 
static int tl_attach_phy(sc)
	struct tl_softc		*sc;
{
	int			phy_ctl;
	struct tl_type		*p = tl_phys;
	int			media = IFM_ETHER|IFM_100_TX|IFM_FDX;
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;

	sc->tl_phy_did = tl_phy_readreg(sc, TL_PHY_DEVID);
	sc->tl_phy_vid = tl_phy_readreg(sc, TL_PHY_VENID);
	sc->tl_phy_sts = tl_phy_readreg(sc, TL_PHY_GENSTS);
	phy_ctl = tl_phy_readreg(sc, TL_PHY_GENCTL);

	/*
	 * PHY revision numbers tend to vary a bit. Our algorithm here
	 * is to check everything but the 8 least significant bits.
	 */
	while(p->tl_vid) {
		if (sc->tl_phy_vid  == p->tl_vid &&
			(sc->tl_phy_did | 0x000F) == p->tl_did) {
			sc->tl_pinfo = p;
			break;
		}
		p++;
	}
	if (sc->tl_pinfo == NULL) {
		sc->tl_pinfo = &tl_phys[PHY_UNKNOWN];
	}

	if (sc->tl_phy_sts & PHY_BMSR_100BT4 ||
		sc->tl_phy_sts & PHY_BMSR_100BTXFULL ||
		sc->tl_phy_sts & PHY_BMSR_100BTXHALF)
		ifp->if_baudrate = 100000000;
	else
		ifp->if_baudrate = 10000000;

	if (bootverbose) {
		printf("tl%d: phy at mii address %d\n", sc->tl_unit,
							sc->tl_phy_addr);

		printf("tl%d: %s ", sc->tl_unit, sc->tl_pinfo->tl_name);
	}

	if (sc->tl_phy_sts & PHY_BMSR_100BT4 ||
		sc->tl_phy_sts & PHY_BMSR_100BTXHALF ||
		sc->tl_phy_sts & PHY_BMSR_100BTXHALF)
		if (bootverbose)
			printf("10/100Mbps ");
	else {
		media &= ~IFM_100_TX;
		media |= IFM_10_T;
		if (bootverbose)
			printf("10Mbps ");
	}

	if (sc->tl_phy_sts & PHY_BMSR_100BTXFULL ||
		sc->tl_phy_sts & PHY_BMSR_10BTFULL)
		if (bootverbose)
			printf("full duplex ");
	else {
		if (bootverbose)
			printf("half duplex ");
		media &= ~IFM_FDX;
	}

	if (sc->tl_phy_sts & PHY_BMSR_CANAUTONEG) {
		media = IFM_ETHER|IFM_AUTO;
		if (bootverbose)
			printf("autonegotiating\n");
	} else
		if (bootverbose)
			printf("\n");

	/* If this isn't a known PHY, print the PHY indentifier info. */
	if (sc->tl_pinfo->tl_vid == 0 && bootverbose)
		printf("tl%d: vendor id: %04x product id: %04x\n",
			sc->tl_unit, sc->tl_phy_vid, sc->tl_phy_did);

	/* Set up ifmedia data and callbacks. */
	ifmedia_init(&sc->ifmedia, 0, tl_ifmedia_upd, tl_ifmedia_sts);

	/*
	 * All ThunderLANs support at least 10baseT half duplex.
	 * They also support AUI selection if used in 10Mb/s modes.
	 */
	ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_10_T|IFM_HDX, 0, NULL);
	ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_10_T, 0, NULL);
	ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_10_5, 0, NULL);

	/* Some ThunderLAN PHYs support autonegotiation. */
	if (sc->tl_phy_sts & PHY_BMSR_CANAUTONEG)
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_AUTO, 0, NULL);

	/* Some support 10baseT full duplex. */
	if (sc->tl_phy_sts & PHY_BMSR_10BTFULL)
		ifmedia_add(&sc->ifmedia,
			IFM_ETHER|IFM_10_T|IFM_FDX, 0, NULL);

	/* Some support 100BaseTX half duplex. */
	if (sc->tl_phy_sts & PHY_BMSR_100BTXHALF)
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_100_TX, 0, NULL);
	if (sc->tl_phy_sts & PHY_BMSR_100BTXHALF)
		ifmedia_add(&sc->ifmedia,
			IFM_ETHER|IFM_100_TX|IFM_HDX, 0, NULL);

	/* Some support 100BaseTX full duplex. */
	if (sc->tl_phy_sts & PHY_BMSR_100BTXFULL)
		ifmedia_add(&sc->ifmedia,
			IFM_ETHER|IFM_100_TX|IFM_FDX, 0, NULL);

	/* Some also support 100BaseT4. */
	if (sc->tl_phy_sts & PHY_BMSR_100BT4)
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_100_T4, 0, NULL);

	/* Set default media. */
	ifmedia_set(&sc->ifmedia, media);

	/*
	 * Kick off an autonegotiation session if this PHY supports it.
	 * This is necessary to make sure the chip's duplex mode matches
	 * the PHY's duplex mode. It may not: once enabled, the PHY may
	 * autonegotiate full-duplex mode with its link partner, but the
	 * ThunderLAN chip defaults to half-duplex and stays there unless
	 * told otherwise.
	 */
	if (sc->tl_phy_sts & PHY_BMSR_CANAUTONEG) {
		tl_init(sc);
#ifdef TL_BACKGROUND_AUTONEG
		tl_autoneg(sc, TL_FLAG_SCHEDDELAY, 1);
#else
		tl_autoneg(sc, TL_FLAG_FORCEDELAY, 1);
#endif
	}

	return(0);
}

static void
tl_attach(config_id, unit)
	pcici_t			config_id;
	int			unit;
{
	int			s, i, phys = 0;
#ifndef TL_USEIOSPACE
	vm_offset_t		pbase, vbase;
#endif
	u_int32_t		command;
	u_int16_t		did, vid;
	struct tl_type		*t;
	struct ifnet		*ifp;
	struct tl_softc		*sc;
	unsigned int		round;
	caddr_t			roundptr;

	s = splimp();

	vid = pci_cfgread(config_id, PCIR_VENDOR, 2);
	did = pci_cfgread(config_id, PCIR_DEVICE, 2);

	t = tl_devs;
	while(t->tl_name != NULL) {
		if (vid == t->tl_vid && did == t->tl_did)
			break;
		t++;
	}

	if (t->tl_name == NULL) {
		printf("tl%d: unknown device!?\n", unit);
		goto fail;
	}

	/* First, allocate memory for the softc struct. */
	sc = malloc(sizeof(struct tl_softc), M_DEVBUF, M_NOWAIT);
	if (sc == NULL) {
		printf("tl%d: no memory for softc struct!\n", unit);
		goto fail;
	}

	bzero(sc, sizeof(struct tl_softc));

	/*
	 * Map control/status registers.
	 */
	command = pci_conf_read(config_id, PCI_COMMAND_STATUS_REG);
	command |= (PCIM_CMD_PORTEN|PCIM_CMD_MEMEN|PCIM_CMD_BUSMASTEREN);
	pci_conf_write(config_id, PCI_COMMAND_STATUS_REG, command);
	command = pci_conf_read(config_id, PCI_COMMAND_STATUS_REG);

#ifdef TL_USEIOSPACE
	if (!(command & PCIM_CMD_PORTEN)) {
		printf("tl%d: failed to enable I/O ports!\n", unit);
		free(sc, M_DEVBUF);
		goto fail;
	}

	if (!pci_map_port(config_id, TL_PCI_LOIO,
	    (u_short *)&(sc->tl_bhandle))) {
		if (!pci_map_port(config_id, TL_PCI_LOMEM,
		    (u_short *)&(sc->tl_bhandle))) {
			printf ("tl%d: couldn't map ports\n", unit);
			goto fail;
		}
	}
#ifdef __alpha__
	sc->tl_btag = ALPHA_BUS_SPACE_IO;
#endif
#ifdef __i386__
	sc->tl_btag = I386_BUS_SPACE_IO;
#endif
#else
	if (!(command & PCIM_CMD_MEMEN)) {
		printf("tl%d: failed to enable memory mapping!\n", unit);
		goto fail;
	}

	if (!pci_map_mem(config_id, TL_PCI_LOMEM, &vbase, &pbase)) {
		if (!pci_map_mem(config_id, TL_PCI_LOIO, &vbase, &pbase)) {
			printf ("tl%d: couldn't map memory\n", unit);
			goto fail;
		}
	}

#ifdef __alpha__
	sc->tl_btag = ALPHA_BUS_SPACE_MEM;
#endif
#ifdef __i386__
	sc->tl_btag = I386_BUS_SPACE_MEM;
#endif
	sc->tl_bhandle = vbase;
#endif

#ifdef notdef
	/*
	 * The ThunderLAN manual suggests jacking the PCI latency
	 * timer all the way up to its maximum value. I'm not sure
	 * if this is really necessary, but what the manual wants,
	 * the manual gets.
	 */
	command = pci_conf_read(config_id, TL_PCI_LATENCY_TIMER);
	command |= 0x0000FF00;
	pci_conf_write(config_id, TL_PCI_LATENCY_TIMER, command);
#endif

	/* Allocate interrupt */
	if (!pci_map_int(config_id, tl_intr, sc, &net_imask)) {
		printf("tl%d: couldn't map interrupt\n", unit);
		goto fail;
	}

	/*
	 * Now allocate memory for the TX and RX lists. Note that
	 * we actually allocate 8 bytes more than we really need:
	 * this is because we need to adjust the final address to
	 * be aligned on a quadword (64-bit) boundary in order to
	 * make the chip happy. If the list structures aren't properly
	 * aligned, DMA fails and the chip generates an adapter check
	 * interrupt and has to be reset. If you set up the softc struct
	 * just right you can sort of obtain proper alignment 'by chance.'
	 * But I don't want to depend on this, so instead the alignment
	 * is forced here.
	 */
	sc->tl_ldata_ptr = malloc(sizeof(struct tl_list_data) + 8,
				M_DEVBUF, M_NOWAIT);

	if (sc->tl_ldata_ptr == NULL) {
		free(sc, M_DEVBUF);
		printf("tl%d: no memory for list buffers!\n", unit);
		goto fail;
	}

	/*
	 * Convoluted but satisfies my ANSI sensibilities. GCC lets
	 * you do casts on the LHS of an assignment, but ANSI doesn't
	 * allow that.
	 */
	sc->tl_ldata = (struct tl_list_data *)sc->tl_ldata_ptr;
	round = (unsigned int)sc->tl_ldata_ptr & 0xF;
	roundptr = sc->tl_ldata_ptr;
	for (i = 0; i < 8; i++) {
		if (round % 8) {
			round++;
			roundptr++;
		} else
			break;
	}
	sc->tl_ldata = (struct tl_list_data *)roundptr;

	bzero(sc->tl_ldata, sizeof(struct tl_list_data));

	sc->tl_unit = unit;
	sc->tl_dinfo = t;
	if (t->tl_vid == COMPAQ_VENDORID || t->tl_vid == TI_VENDORID)
		sc->tl_eeaddr = TL_EEPROM_EADDR;
	if (t->tl_vid == OLICOM_VENDORID)
		sc->tl_eeaddr = TL_EEPROM_EADDR_OC;

	/* Reset the adapter. */
	tl_softreset(sc, 1);
	tl_hardreset(sc);
	tl_softreset(sc, 1);

	/*
	 * Get station address from the EEPROM.
	 */
	if (tl_read_eeprom(sc, (caddr_t)&sc->arpcom.ac_enaddr,
				sc->tl_eeaddr, ETHER_ADDR_LEN)) {
		printf("tl%d: failed to read station address\n", unit);
		goto fail;
	}

        /*
         * XXX Olicom, in its desire to be different from the
         * rest of the world, has done strange things with the
         * encoding of the station address in the EEPROM. First
         * of all, they store the address at offset 0xF8 rather
         * than at 0x83 like the ThunderLAN manual suggests.
         * Second, they store the address in three 16-bit words in
         * network byte order, as opposed to storing it sequentially
         * like all the other ThunderLAN cards. In order to get
         * the station address in a form that matches what the Olicom
         * diagnostic utility specifies, we have to byte-swap each
         * word. To make things even more confusing, neither 00:00:28
         * nor 00:00:24 appear in the IEEE OUI database.
         */
        if (sc->tl_dinfo->tl_vid == OLICOM_VENDORID) {
                for (i = 0; i < ETHER_ADDR_LEN; i += 2) {
                        u_int16_t               *p;
                        p = (u_int16_t *)&sc->arpcom.ac_enaddr[i];
                        *p = ntohs(*p);
                }
        }

	/*
	 * A ThunderLAN chip was detected. Inform the world.
	 */
	printf("tl%d: Ethernet address: %6D\n", unit,
				sc->arpcom.ac_enaddr, ":");

	ifp = &sc->arpcom.ac_if;
	ifp->if_softc = sc;
	ifp->if_unit = sc->tl_unit;
	ifp->if_name = "tl";
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = tl_ioctl;
	ifp->if_output = ether_output;
	ifp->if_start = tl_start;
	ifp->if_watchdog = tl_watchdog;
	ifp->if_init = tl_init;
	ifp->if_mtu = ETHERMTU;
	ifp->if_snd.ifq_maxlen = TL_TX_LIST_CNT - 1;
	callout_handle_init(&sc->tl_stat_ch);

	/* Reset the adapter again. */
	tl_softreset(sc, 1);
	tl_hardreset(sc);
	tl_softreset(sc, 1);

	/*
	 * Now attach the ThunderLAN's PHYs. There will always
	 * be at least one PHY; if the PHY address is 0x1F, then
	 * it's the internal one.
	 */

	for (i = TL_PHYADDR_MIN; i < TL_PHYADDR_MAX + 1; i++) {
		sc->tl_phy_addr = i;
		if (bootverbose)
			printf("tl%d: looking for phy at addr %x\n", unit, i);
		tl_phy_writereg(sc, PHY_BMCR, PHY_BMCR_RESET);
		DELAY(500);
		while(tl_phy_readreg(sc, PHY_BMCR) & PHY_BMCR_RESET);
		sc->tl_phy_sts = tl_phy_readreg(sc, PHY_BMSR);
		if (bootverbose)
			printf("tl%d: status: %x\n", unit, sc->tl_phy_sts);
		if (!sc->tl_phy_sts)
			continue;
		if (tl_attach_phy(sc)) {
			printf("tl%d: failed to attach a phy %d\n", unit, i);
			goto fail;
		}
		phys++;
		if (phys && i != TL_PHYADDR_MAX)
			break;
	}

	/*
	 * If no MII-based PHYs were detected, then this is a
	 * TNETE110 device with a bit rate PHY. There's no autoneg
	 * support, so just default to 10baseT mode.
	 */
	if (!phys) {
		struct ifmedia		*ifm;
		sc->tl_bitrate = 1;
		ifmedia_init(&sc->ifmedia, 0, tl_ifmedia_upd, tl_ifmedia_sts);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_10_T, 0, NULL);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_10_T|IFM_HDX, 0, NULL);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_10_T|IFM_FDX, 0, NULL);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_10_5, 0, NULL);
		ifmedia_set(&sc->ifmedia, IFM_ETHER|IFM_10_T);
		/* Reset again, this time setting bitrate mode. */
		tl_softreset(sc, 1);
		ifm = &sc->ifmedia;
		ifm->ifm_media = ifm->ifm_cur->ifm_media;
		tl_ifmedia_upd(ifp);
	}

	tl_intvec_adchk((void *)sc, 0);
	tl_stop(sc);

	/*
	 * Attempt to clear any stray interrupts
	 * that may be lurking.
	 */
	tl_intr((void *)sc);

	/*
	 * Call MI attach routines.
	 */
	if_attach(ifp);
	ether_ifattach(ifp);

#if NBPFILTER > 0
	bpfattach(ifp, DLT_EN10MB, sizeof(struct ether_header));
#endif

	at_shutdown(tl_shutdown, sc, SHUTDOWN_POST_SYNC);

fail:
	splx(s);
	return;
}

/*
 * Initialize the transmit lists.
 */
static int tl_list_tx_init(sc)
	struct tl_softc		*sc;
{
	struct tl_chain_data	*cd;
	struct tl_list_data	*ld;
	int			i;

	cd = &sc->tl_cdata;
	ld = sc->tl_ldata;
	for (i = 0; i < TL_TX_LIST_CNT; i++) {
		cd->tl_tx_chain[i].tl_ptr = &ld->tl_tx_list[i];
		if (i == (TL_TX_LIST_CNT - 1))
			cd->tl_tx_chain[i].tl_next = NULL;
		else
			cd->tl_tx_chain[i].tl_next = &cd->tl_tx_chain[i + 1];
	}

	cd->tl_tx_free = &cd->tl_tx_chain[0];
	cd->tl_tx_tail = cd->tl_tx_head = NULL;
	sc->tl_txeoc = 1;

	return(0);
}

/*
 * Initialize the RX lists and allocate mbufs for them.
 */
static int tl_list_rx_init(sc)
	struct tl_softc		*sc;
{
	struct tl_chain_data	*cd;
	struct tl_list_data	*ld;
	int			i;

	cd = &sc->tl_cdata;
	ld = sc->tl_ldata;

	for (i = 0; i < TL_RX_LIST_CNT; i++) {
		cd->tl_rx_chain[i].tl_ptr =
			(struct tl_list_onefrag *)&ld->tl_rx_list[i];
		if (tl_newbuf(sc, &cd->tl_rx_chain[i]) == ENOBUFS)
			return(ENOBUFS);
		if (i == (TL_RX_LIST_CNT - 1)) {
			cd->tl_rx_chain[i].tl_next = NULL;
			ld->tl_rx_list[i].tlist_fptr = 0;
		} else {
			cd->tl_rx_chain[i].tl_next = &cd->tl_rx_chain[i + 1];
			ld->tl_rx_list[i].tlist_fptr =
					vtophys(&ld->tl_rx_list[i + 1]);
		}
	}

	cd->tl_rx_head = &cd->tl_rx_chain[0];
	cd->tl_rx_tail = &cd->tl_rx_chain[TL_RX_LIST_CNT - 1];

	return(0);
}

static int tl_newbuf(sc, c)
	struct tl_softc		*sc;
	struct tl_chain_onefrag	*c;
{
	struct mbuf		*m_new = NULL;

	MGETHDR(m_new, M_DONTWAIT, MT_DATA);
	if (m_new == NULL) {
		printf("tl%d: no memory for rx list -- packet dropped!",
				sc->tl_unit);
		return(ENOBUFS);
	}

	MCLGET(m_new, M_DONTWAIT);
	if (!(m_new->m_flags & M_EXT)) {
		printf("tl%d: no memory for rx list -- packet dropped!",
				 sc->tl_unit);
		m_freem(m_new);
		return(ENOBUFS);
	}

#ifdef __alpha__
	m_new->m_data += 2;
#endif

	c->tl_mbuf = m_new;
	c->tl_next = NULL;
	c->tl_ptr->tlist_frsize = MCLBYTES;
	c->tl_ptr->tlist_cstat = TL_CSTAT_READY;
	c->tl_ptr->tlist_fptr = 0;
	c->tl_ptr->tl_frag.tlist_dadr = vtophys(mtod(m_new, caddr_t));
	c->tl_ptr->tl_frag.tlist_dcnt = MCLBYTES;

	return(0);
}
/*
 * Interrupt handler for RX 'end of frame' condition (EOF). This
 * tells us that a full ethernet frame has been captured and we need
 * to handle it.
 *
 * Reception is done using 'lists' which consist of a header and a
 * series of 10 data count/data address pairs that point to buffers.
 * Initially you're supposed to create a list, populate it with pointers
 * to buffers, then load the physical address of the list into the
 * ch_parm register. The adapter is then supposed to DMA the received
 * frame into the buffers for you.
 *
 * To make things as fast as possible, we have the chip DMA directly
 * into mbufs. This saves us from having to do a buffer copy: we can
 * just hand the mbufs directly to ether_input(). Once the frame has
 * been sent on its way, the 'list' structure is assigned a new buffer
 * and moved to the end of the RX chain. As long we we stay ahead of
 * the chip, it will always think it has an endless receive channel.
 *
 * If we happen to fall behind and the chip manages to fill up all of
 * the buffers, it will generate an end of channel interrupt and wait
 * for us to empty the chain and restart the receiver.
 */
static int tl_intvec_rxeof(xsc, type)
	void			*xsc;
	u_int32_t		type;
{
	struct tl_softc		*sc;
	int			r = 0, total_len = 0;
	struct ether_header	*eh;
	struct mbuf		*m;
	struct ifnet		*ifp;
	struct tl_chain_onefrag	*cur_rx;

	sc = xsc;
	ifp = &sc->arpcom.ac_if;

	while(sc->tl_cdata.tl_rx_head->tl_ptr->tlist_cstat & TL_CSTAT_FRAMECMP){
		r++;
		cur_rx = sc->tl_cdata.tl_rx_head;
		sc->tl_cdata.tl_rx_head = cur_rx->tl_next;
		m = cur_rx->tl_mbuf;
		total_len = cur_rx->tl_ptr->tlist_frsize;

		if (tl_newbuf(sc, cur_rx) == ENOBUFS) {
			ifp->if_ierrors++;
			cur_rx->tl_ptr->tlist_frsize = MCLBYTES;
			cur_rx->tl_ptr->tlist_cstat = TL_CSTAT_READY;
			cur_rx->tl_ptr->tl_frag.tlist_dcnt = MCLBYTES;
			continue;
		}

		sc->tl_cdata.tl_rx_tail->tl_ptr->tlist_fptr =
						vtophys(cur_rx->tl_ptr);
		sc->tl_cdata.tl_rx_tail->tl_next = cur_rx;
		sc->tl_cdata.tl_rx_tail = cur_rx;

		eh = mtod(m, struct ether_header *);
		m->m_pkthdr.rcvif = ifp;

		/*
		 * Note: when the ThunderLAN chip is in 'capture all
		 * frames' mode, it will receive its own transmissions.
		 * We drop don't need to process our own transmissions,
		 * so we drop them here and continue.
		 */
		/*if (ifp->if_flags & IFF_PROMISC && */
		if (!bcmp(eh->ether_shost, sc->arpcom.ac_enaddr,
		 					ETHER_ADDR_LEN)) {
				m_freem(m);
				continue;
		}

#if NBPFILTER > 0
		/*
	 	 * Handle BPF listeners. Let the BPF user see the packet, but
	 	 * don't pass it up to the ether_input() layer unless it's
	 	 * a broadcast packet, multicast packet, matches our ethernet
	 	 * address or the interface is in promiscuous mode. If we don't
	 	 * want the packet, just forget it. We leave the mbuf in place
	 	 * since it can be used again later.
	 	 */
		if (ifp->if_bpf) {
			m->m_pkthdr.len = m->m_len = total_len;
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
		m->m_pkthdr.len = m->m_len =
				total_len - sizeof(struct ether_header);
		m->m_data += sizeof(struct ether_header);
		ether_input(ifp, eh, m);
	}

	return(r);
}

/*
 * The RX-EOC condition hits when the ch_parm address hasn't been
 * initialized or the adapter reached a list with a forward pointer
 * of 0 (which indicates the end of the chain). In our case, this means
 * the card has hit the end of the receive buffer chain and we need to
 * empty out the buffers and shift the pointer back to the beginning again.
 */
static int tl_intvec_rxeoc(xsc, type)
	void			*xsc;
	u_int32_t		type;
{
	struct tl_softc		*sc;
	int			r;

	sc = xsc;

	/* Flush out the receive queue and ack RXEOF interrupts. */
	r = tl_intvec_rxeof(xsc, type);
	CMD_PUT(sc, TL_CMD_ACK | r | (type & ~(0x00100000)));
	r = 1;
	CSR_WRITE_4(sc, TL_CH_PARM, vtophys(sc->tl_cdata.tl_rx_head->tl_ptr));
	r |= (TL_CMD_GO|TL_CMD_RT);
	return(r);
}

static int tl_intvec_txeof(xsc, type)
	void			*xsc;
	u_int32_t		type;
{
	struct tl_softc		*sc;
	int			r = 0;
	struct tl_chain		*cur_tx;

	sc = xsc;

	/*
	 * Go through our tx list and free mbufs for those
	 * frames that have been sent.
	 */
	while (sc->tl_cdata.tl_tx_head != NULL) {
		cur_tx = sc->tl_cdata.tl_tx_head;
		if (!(cur_tx->tl_ptr->tlist_cstat & TL_CSTAT_FRAMECMP))
			break;
		sc->tl_cdata.tl_tx_head = cur_tx->tl_next;

		r++;
		m_freem(cur_tx->tl_mbuf);
		cur_tx->tl_mbuf = NULL;

		cur_tx->tl_next = sc->tl_cdata.tl_tx_free;
		sc->tl_cdata.tl_tx_free = cur_tx;
		if (!cur_tx->tl_ptr->tlist_fptr)
			break;
	}

	return(r);
}

/*
 * The transmit end of channel interrupt. The adapter triggers this
 * interrupt to tell us it hit the end of the current transmit list.
 *
 * A note about this: it's possible for a condition to arise where
 * tl_start() may try to send frames between TXEOF and TXEOC interrupts.
 * You have to avoid this since the chip expects things to go in a
 * particular order: transmit, acknowledge TXEOF, acknowledge TXEOC.
 * When the TXEOF handler is called, it will free all of the transmitted
 * frames and reset the tx_head pointer to NULL. However, a TXEOC
 * interrupt should be received and acknowledged before any more frames
 * are queued for transmission. If tl_statrt() is called after TXEOF
 * resets the tx_head pointer but _before_ the TXEOC interrupt arrives,
 * it could attempt to issue a transmit command prematurely.
 *
 * To guard against this, tl_start() will only issue transmit commands
 * if the tl_txeoc flag is set, and only the TXEOC interrupt handler
 * can set this flag once tl_start() has cleared it.
 */
static int tl_intvec_txeoc(xsc, type)
	void			*xsc;
	u_int32_t		type;
{
	struct tl_softc		*sc;
	struct ifnet		*ifp;
	u_int32_t		cmd;

	sc = xsc;
	ifp = &sc->arpcom.ac_if;

	/* Clear the timeout timer. */
	ifp->if_timer = 0;

	if (sc->tl_cdata.tl_tx_head == NULL) {
		ifp->if_flags &= ~IFF_OACTIVE;
		sc->tl_cdata.tl_tx_tail = NULL;
		sc->tl_txeoc = 1;
		/*
		 * If we just drained the TX queue and
		 * there's an autoneg request waiting, set
		 * it in motion. This will block the transmitter
		 * until the autoneg session completes which will
		 * no doubt piss off any processes waiting to
		 * transmit, but that's the way the ball bounces.
		 */
		if (sc->tl_want_auto)
			tl_autoneg(sc, TL_FLAG_SCHEDDELAY, 1);
	} else {
		sc->tl_txeoc = 0;
		/* First we have to ack the EOC interrupt. */
		CMD_PUT(sc, TL_CMD_ACK | 0x00000001 | type);
		/* Then load the address of the next TX list. */
		CSR_WRITE_4(sc, TL_CH_PARM,
				vtophys(sc->tl_cdata.tl_tx_head->tl_ptr));
		/* Restart TX channel. */
		cmd = CSR_READ_4(sc, TL_HOSTCMD);
		cmd &= ~TL_CMD_RT;
		cmd |= TL_CMD_GO|TL_CMD_INTSON;
		CMD_PUT(sc, cmd);
		return(0);
	}

	return(1);
}

static int tl_intvec_adchk(xsc, type)
	void			*xsc;
	u_int32_t		type;
{
	struct tl_softc		*sc;
	u_int16_t		bmcr, ctl;

	sc = xsc;

	if (type)
		printf("tl%d: adapter check: %x\n", sc->tl_unit,
			(unsigned int)CSR_READ_4(sc, TL_CH_PARM));

	/*
	 * Before resetting the adapter, try reading the PHY
	 * settings so we can put them back later. This is
	 * necessary to keep the chip operating at the same
	 * speed and duplex settings after the reset completes.
	 */
	if (!sc->tl_bitrate) {
	bmcr = tl_phy_readreg(sc, PHY_BMCR);
	ctl = tl_phy_readreg(sc, TL_PHY_CTL);
	tl_softreset(sc, 1);
	tl_phy_writereg(sc, PHY_BMCR, bmcr);
	tl_phy_writereg(sc, TL_PHY_CTL, ctl);
	if (bmcr & PHY_BMCR_DUPLEX) {
		tl_dio_setbit(sc, TL_NETCMD, TL_CMD_DUPLEX);
	} else {
		tl_dio_clrbit(sc, TL_NETCMD, TL_CMD_DUPLEX);
	}
	}
	tl_stop(sc);
	tl_init(sc);
	CMD_SET(sc, TL_CMD_INTSON);

	return(0);
}

static int tl_intvec_netsts(xsc, type)
	void			*xsc;
	u_int32_t		type;
{
	struct tl_softc		*sc;
	u_int16_t		netsts;

	sc = xsc;

	netsts = tl_dio_read16(sc, TL_NETSTS);
	tl_dio_write16(sc, TL_NETSTS, netsts);

	printf("tl%d: network status: %x\n", sc->tl_unit, netsts);

	return(1);
}

static void tl_intr(xsc)
	void			*xsc;
{
	struct tl_softc		*sc;
	struct ifnet		*ifp;
	int			r = 0;
	u_int32_t		type = 0;
	u_int16_t		ints = 0;
	u_int8_t		ivec = 0;

	sc = xsc;

	/* Disable interrupts */
	ints = CSR_READ_2(sc, TL_HOST_INT);
	CSR_WRITE_2(sc, TL_HOST_INT, ints);
	type = (ints << 16) & 0xFFFF0000;
	ivec = (ints & TL_VEC_MASK) >> 5;
	ints = (ints & TL_INT_MASK) >> 2;

	ifp = &sc->arpcom.ac_if;

	switch(ints) {
	case (TL_INTR_INVALID):
#ifdef DIAGNOSTIC
		printf("tl%d: got an invalid interrupt!\n", sc->tl_unit);
#endif
		/* Re-enable interrupts but don't ack this one. */
		CMD_PUT(sc, type);
		r = 0;
		break;
	case (TL_INTR_TXEOF):
		r = tl_intvec_txeof((void *)sc, type);
		break;
	case (TL_INTR_TXEOC):
		r = tl_intvec_txeoc((void *)sc, type);
		break;
	case (TL_INTR_STATOFLOW):
		tl_stats_update(sc);
		r = 1;
		break;
	case (TL_INTR_RXEOF):
		r = tl_intvec_rxeof((void *)sc, type);
		break;
	case (TL_INTR_DUMMY):
		printf("tl%d: got a dummy interrupt\n", sc->tl_unit);
		r = 1;
		break;
	case (TL_INTR_ADCHK):
		if (ivec)
			r = tl_intvec_adchk((void *)sc, type);
		else
			r = tl_intvec_netsts((void *)sc, type);
		break;
	case (TL_INTR_RXEOC):
		r = tl_intvec_rxeoc((void *)sc, type);
		break;
	default:
		printf("tl%d: bogus interrupt type\n", ifp->if_unit);
		break;
	}

	/* Re-enable interrupts */
	if (r) {
		CMD_PUT(sc, TL_CMD_ACK | r | type);
	}

	if (ifp->if_snd.ifq_head != NULL)
		tl_start(ifp);

	return;
}

static void tl_stats_update(xsc)
	void			*xsc;
{
	struct tl_softc		*sc;
	struct ifnet		*ifp;
	struct tl_stats		tl_stats;
	u_int32_t		*p;
	int			s;

	s = splimp();

	bzero((char *)&tl_stats, sizeof(struct tl_stats));

	sc = xsc;
	ifp = &sc->arpcom.ac_if;

	p = (u_int32_t *)&tl_stats;

	CSR_WRITE_2(sc, TL_DIO_ADDR, TL_TXGOODFRAMES|TL_DIO_ADDR_INC);
	*p++ = CSR_READ_4(sc, TL_DIO_DATA);
	*p++ = CSR_READ_4(sc, TL_DIO_DATA);
	*p++ = CSR_READ_4(sc, TL_DIO_DATA);
	*p++ = CSR_READ_4(sc, TL_DIO_DATA);
	*p++ = CSR_READ_4(sc, TL_DIO_DATA);

	ifp->if_opackets += tl_tx_goodframes(tl_stats);
	ifp->if_collisions += tl_stats.tl_tx_single_collision +
				tl_stats.tl_tx_multi_collision;
	ifp->if_ipackets += tl_rx_goodframes(tl_stats);
	ifp->if_ierrors += tl_stats.tl_crc_errors + tl_stats.tl_code_errors +
			    tl_rx_overrun(tl_stats);
	ifp->if_oerrors += tl_tx_underrun(tl_stats);

	sc->tl_stat_ch = timeout(tl_stats_update, sc, hz);

	splx(s);

	return;
}

/*
 * Encapsulate an mbuf chain in a list by coupling the mbuf data
 * pointers to the fragment pointers.
 */
static int tl_encap(sc, c, m_head)
	struct tl_softc		*sc;
	struct tl_chain		*c;
	struct mbuf		*m_head;
{
	int			frag = 0;
	struct tl_frag		*f = NULL;
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
			if (frag == TL_MAXFRAGS)
				break;
			total_len+= m->m_len;
			c->tl_ptr->tl_frag[frag].tlist_dadr =
				vtophys(mtod(m, vm_offset_t));
			c->tl_ptr->tl_frag[frag].tlist_dcnt = m->m_len;
			frag++;
		}
	}

	/*
	 * Handle special cases.
	 * Special case #1: we used up all 10 fragments, but
	 * we have more mbufs left in the chain. Copy the
	 * data into an mbuf cluster. Note that we don't
	 * bother clearing the values in the other fragment
	 * pointers/counters; it wouldn't gain us anything,
	 * and would waste cycles.
	 */
	if (m != NULL) {
		struct mbuf		*m_new = NULL;

		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL) {
			printf("tl%d: no memory for tx list", sc->tl_unit);
			return(1);
		}
		if (m_head->m_pkthdr.len > MHLEN) {
			MCLGET(m_new, M_DONTWAIT);
			if (!(m_new->m_flags & M_EXT)) {
				m_freem(m_new);
				printf("tl%d: no memory for tx list",
				sc->tl_unit);
				return(1);
			}
		}
		m_copydata(m_head, 0, m_head->m_pkthdr.len,	
					mtod(m_new, caddr_t));
		m_new->m_pkthdr.len = m_new->m_len = m_head->m_pkthdr.len;
		m_freem(m_head);
		m_head = m_new;
		f = &c->tl_ptr->tl_frag[0];
		f->tlist_dadr = vtophys(mtod(m_new, caddr_t));
		f->tlist_dcnt = total_len = m_new->m_len;
		frag = 1;
	}

	/*
	 * Special case #2: the frame is smaller than the minimum
	 * frame size. We have to pad it to make the chip happy.
	 */
	if (total_len < TL_MIN_FRAMELEN) {
		if (frag == TL_MAXFRAGS)
			printf("tl%d: all frags filled but "
				"frame still to small!\n", sc->tl_unit);
		f = &c->tl_ptr->tl_frag[frag];
		f->tlist_dcnt = TL_MIN_FRAMELEN - total_len;
		f->tlist_dadr = vtophys(&sc->tl_ldata->tl_pad);
		total_len += f->tlist_dcnt;
		frag++;
	}

	c->tl_mbuf = m_head;
	c->tl_ptr->tl_frag[frag - 1].tlist_dcnt |= TL_LAST_FRAG;
	c->tl_ptr->tlist_frsize = total_len;
	c->tl_ptr->tlist_cstat = TL_CSTAT_READY;
	c->tl_ptr->tlist_fptr = 0;

	return(0);
}

/*
 * Main transmit routine. To avoid having to do mbuf copies, we put pointers
 * to the mbuf data regions directly in the transmit lists. We also save a
 * copy of the pointers since the transmit list fragment pointers are
 * physical addresses.
 */
static void tl_start(ifp)
	struct ifnet		*ifp;
{
	struct tl_softc		*sc;
	struct mbuf		*m_head = NULL;
	u_int32_t		cmd;
	struct tl_chain		*prev = NULL, *cur_tx = NULL, *start_tx;

	sc = ifp->if_softc;

	if (sc->tl_autoneg) {
		sc->tl_tx_pend = 1;
		return;
	}

	/*
	 * Check for an available queue slot. If there are none,
	 * punt.
	 */
	if (sc->tl_cdata.tl_tx_free == NULL) {
		ifp->if_flags |= IFF_OACTIVE;
		return;
	}

	start_tx = sc->tl_cdata.tl_tx_free;

	while(sc->tl_cdata.tl_tx_free != NULL) {
		IF_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		/* Pick a chain member off the free list. */
		cur_tx = sc->tl_cdata.tl_tx_free;
		sc->tl_cdata.tl_tx_free = cur_tx->tl_next;

		cur_tx->tl_next = NULL;

		/* Pack the data into the list. */
		tl_encap(sc, cur_tx, m_head);

		/* Chain it together */
		if (prev != NULL) {
			prev->tl_next = cur_tx;
			prev->tl_ptr->tlist_fptr = vtophys(cur_tx->tl_ptr);
		}
		prev = cur_tx;

		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp, cur_tx->tl_mbuf);
#endif
	}

	/*
	 * If there are no packets queued, bail.
	 */
	if (cur_tx == NULL)
		return;

	/*
	 * That's all we can stands, we can't stands no more.
	 * If there are no other transfers pending, then issue the
	 * TX GO command to the adapter to start things moving.
	 * Otherwise, just leave the data in the queue and let
	 * the EOF/EOC interrupt handler send.
	 */
	if (sc->tl_cdata.tl_tx_head == NULL) {
		sc->tl_cdata.tl_tx_head = start_tx;
		sc->tl_cdata.tl_tx_tail = cur_tx;

		if (sc->tl_txeoc) {
			sc->tl_txeoc = 0;
			CSR_WRITE_4(sc, TL_CH_PARM, vtophys(start_tx->tl_ptr));
			cmd = CSR_READ_4(sc, TL_HOSTCMD);
			cmd &= ~TL_CMD_RT;
			cmd |= TL_CMD_GO|TL_CMD_INTSON;
			CMD_PUT(sc, cmd);
		}
	} else {
		sc->tl_cdata.tl_tx_tail->tl_next = start_tx;
		sc->tl_cdata.tl_tx_tail = cur_tx;
	}

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;

	return;
}

static void tl_init(xsc)
	void			*xsc;
{
	struct tl_softc		*sc = xsc;
	struct ifnet		*ifp = &sc->arpcom.ac_if;
        int			s;
	u_int16_t		phy_sts;

	if (sc->tl_autoneg)
		return;

	s = splimp();

	ifp = &sc->arpcom.ac_if;

	/*
	 * Cancel pending I/O.
	 */
	tl_stop(sc);

	/*
	 * Set 'capture all frames' bit for promiscuous mode.
	 */
	if (ifp->if_flags & IFF_PROMISC)
		tl_dio_setbit(sc, TL_NETCMD, TL_CMD_CAF);
	else
		tl_dio_clrbit(sc, TL_NETCMD, TL_CMD_CAF);

	/*
	 * Set capture broadcast bit to capture broadcast frames.
	 */
	if (ifp->if_flags & IFF_BROADCAST)
		tl_dio_clrbit(sc, TL_NETCMD, TL_CMD_NOBRX);
	else
		tl_dio_setbit(sc, TL_NETCMD, TL_CMD_NOBRX);

	/* Init our MAC address */
	tl_setfilt(sc, (caddr_t)&sc->arpcom.ac_enaddr, 0);

	/* Init multicast filter, if needed. */
	tl_setmulti(sc);

	/* Init circular RX list. */
	if (tl_list_rx_init(sc) == ENOBUFS) {
		printf("tl%d: initialization failed: no "
			"memory for rx buffers\n", sc->tl_unit);
		tl_stop(sc);
		return;
	}

	/* Init TX pointers. */
	tl_list_tx_init(sc);

	/*
	 * Enable PHY interrupts.
	 */
	phy_sts = tl_phy_readreg(sc, TL_PHY_CTL);
	phy_sts |= PHY_CTL_INTEN;
	tl_phy_writereg(sc, TL_PHY_CTL, phy_sts);

	/* Enable MII interrupts. */
	tl_dio_setbit(sc, TL_NETSIO, TL_SIO_MINTEN);

	/* Enable PCI interrupts. */
	CMD_SET(sc, TL_CMD_INTSON);

	/* Load the address of the rx list */
	CMD_SET(sc, TL_CMD_RT);
	CSR_WRITE_4(sc, TL_CH_PARM, vtophys(&sc->tl_ldata->tl_rx_list[0]));

	/*
	 * XXX This is a kludge to handle adapters with the Micro Linear
	 * ML6692 100BaseTX PHY, which only supports 100Mbps modes and
	 * relies on the controller's internal 10Mbps PHY to provide
	 * 10Mbps modes. The ML6692 always shows up with a vendor/device ID
	 * of 0 (it doesn't actually have vendor/device ID registers)
	 * so we use that property to detect it. In theory there ought to
	 * be a better way to 'spot the looney' but I can't find one.
         */
        if (!sc->tl_phy_vid) {
                u_int8_t                        addr = 0;
                u_int16_t                       bmcr;

                bmcr = tl_phy_readreg(sc, PHY_BMCR);
                addr = sc->tl_phy_addr;
                sc->tl_phy_addr = TL_PHYADDR_MAX;
                tl_phy_writereg(sc, PHY_BMCR, PHY_BMCR_RESET);
                if (bmcr & PHY_BMCR_SPEEDSEL)
                        tl_phy_writereg(sc, PHY_BMCR, PHY_BMCR_ISOLATE);
                else
                        tl_phy_writereg(sc, PHY_BMCR, bmcr);
                sc->tl_phy_addr = addr;
        }

	/* Send the RX go command */
	CMD_SET(sc, TL_CMD_GO|TL_CMD_RT);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	(void)splx(s);

	/* Start the stats update counter */
	sc->tl_stat_ch = timeout(tl_stats_update, sc, hz);

	return;
}

/*
 * Set media options.
 */
static int tl_ifmedia_upd(ifp)
	struct ifnet		*ifp;
{
	struct tl_softc		*sc;
	struct ifmedia		*ifm;

	sc = ifp->if_softc;
	ifm = &sc->ifmedia;

	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
		return(EINVAL);

	if (IFM_SUBTYPE(ifm->ifm_media) == IFM_AUTO)
		tl_autoneg(sc, TL_FLAG_SCHEDDELAY, 1);
	else
		tl_setmode(sc, ifm->ifm_media);

	return(0);
}

/*
 * Report current media status.
 */
static void tl_ifmedia_sts(ifp, ifmr)
	struct ifnet		*ifp;
	struct ifmediareq	*ifmr;
{
	u_int16_t		phy_ctl;
	u_int16_t		phy_sts;
	struct tl_softc		*sc;

	sc = ifp->if_softc;

	ifmr->ifm_active = IFM_ETHER;

	if (sc->tl_bitrate) {
		if (tl_dio_read8(sc, TL_ACOMMIT) & TL_AC_MTXD1)
			ifmr->ifm_active = IFM_ETHER|IFM_10_5;
		else
			ifmr->ifm_active = IFM_ETHER|IFM_10_T;
		if (tl_dio_read8(sc, TL_ACOMMIT) & TL_AC_MTXD3)
			ifmr->ifm_active |= IFM_HDX;
		else
			ifmr->ifm_active |= IFM_FDX;
		return;
	}

	phy_ctl = tl_phy_readreg(sc, PHY_BMCR);
	phy_sts = tl_phy_readreg(sc, TL_PHY_CTL);

	if (phy_sts & PHY_CTL_AUISEL)
		ifmr->ifm_active = IFM_ETHER|IFM_10_5;

	if (phy_ctl & PHY_BMCR_LOOPBK)
		ifmr->ifm_active = IFM_ETHER|IFM_LOOP;

	if (phy_ctl & PHY_BMCR_SPEEDSEL)
		ifmr->ifm_active = IFM_ETHER|IFM_100_TX;
	else
		ifmr->ifm_active = IFM_ETHER|IFM_10_T;

	if (phy_ctl & PHY_BMCR_DUPLEX) {
		ifmr->ifm_active |= IFM_FDX;
		ifmr->ifm_active &= ~IFM_HDX;
	} else {
		ifmr->ifm_active &= ~IFM_FDX;
		ifmr->ifm_active |= IFM_HDX;
	}

	return;
}

static int tl_ioctl(ifp, command, data)
	struct ifnet		*ifp;
	u_long			command;
	caddr_t			data;
{
	struct tl_softc		*sc = ifp->if_softc;
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
			tl_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING) {
				tl_stop(sc);
			}
		}
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		tl_setmulti(sc);
		error = 0;
		break;
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->ifmedia, command);
		break;
	default:
		error = EINVAL;
		break;
	}

	(void)splx(s);

	return(error);
}

static void tl_watchdog(ifp)
	struct ifnet		*ifp;
{
	struct tl_softc		*sc;
	u_int16_t		bmsr;

	sc = ifp->if_softc;

	if (sc->tl_autoneg) {
		tl_autoneg(sc, TL_FLAG_DELAYTIMEO, 1);
		return;
	}

	/* Check that we're still connected. */
	tl_phy_readreg(sc, PHY_BMSR);
	bmsr = tl_phy_readreg(sc, PHY_BMSR);
	if (!(bmsr & PHY_BMSR_LINKSTAT)) {
		printf("tl%d: no carrier\n", sc->tl_unit);
		tl_autoneg(sc, TL_FLAG_SCHEDDELAY, 1);
	} else
		printf("tl%d: device timeout\n", sc->tl_unit);

	ifp->if_oerrors++;

	tl_init(sc);

	return;
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
static void tl_stop(sc)
	struct tl_softc		*sc;
{
	register int		i;
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;

	/* Stop the stats updater. */
	untimeout(tl_stats_update, sc, sc->tl_stat_ch);

	/* Stop the transmitter */
	CMD_CLR(sc, TL_CMD_RT);
	CMD_SET(sc, TL_CMD_STOP);
	CSR_WRITE_4(sc, TL_CH_PARM, 0);

	/* Stop the receiver */
	CMD_SET(sc, TL_CMD_RT);
	CMD_SET(sc, TL_CMD_STOP);
	CSR_WRITE_4(sc, TL_CH_PARM, 0);

	/*
	 * Disable host interrupts.
	 */
	CMD_SET(sc, TL_CMD_INTSOFF);

	/*
	 * Disable MII interrupts.
	 */
	tl_dio_clrbit(sc, TL_NETSIO, TL_SIO_MINTEN);

	/*
	 * Clear list pointer.
	 */
	CSR_WRITE_4(sc, TL_CH_PARM, 0);

	/*
	 * Free the RX lists.
	 */
	for (i = 0; i < TL_RX_LIST_CNT; i++) {
		if (sc->tl_cdata.tl_rx_chain[i].tl_mbuf != NULL) {
			m_freem(sc->tl_cdata.tl_rx_chain[i].tl_mbuf);
			sc->tl_cdata.tl_rx_chain[i].tl_mbuf = NULL;
		}
	}
	bzero((char *)&sc->tl_ldata->tl_rx_list,
		sizeof(sc->tl_ldata->tl_rx_list));

	/*
	 * Free the TX list buffers.
	 */
	for (i = 0; i < TL_TX_LIST_CNT; i++) {
		if (sc->tl_cdata.tl_tx_chain[i].tl_mbuf != NULL) {
			m_freem(sc->tl_cdata.tl_tx_chain[i].tl_mbuf);
			sc->tl_cdata.tl_tx_chain[i].tl_mbuf = NULL;
		}
	}
	bzero((char *)&sc->tl_ldata->tl_tx_list,
		sizeof(sc->tl_ldata->tl_tx_list));

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	return;
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
static void tl_shutdown(howto, xsc)
	int			howto;
	void			*xsc;
{
	struct tl_softc		*sc;

	sc = xsc;

	tl_stop(sc);

	return;
}


static struct pci_device tl_device = {
	"tl",
	tl_probe,
	tl_attach,
	&tl_count,
	NULL
};
DATA_SET(pcidevice_set, tl_device);
