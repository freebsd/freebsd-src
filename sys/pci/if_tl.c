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
 *	$Id: if_tl.c,v 1.2 1998/05/21 16:24:04 jkh Exp $
 */

/*
 * Texas Instruments ThunderLAN driver for FreeBSD 2.2.6 and 3.x.
 * Supports many Compaq PCI NICs based on the ThunderLAN ethernet controller,
 * the National Semiconductor DP83840A physical interface and the
 * Microchip Technology 24Cxx series serial EEPROM.
 *
 * Written using the following three documents:
 *
 * Texas Instruments ThunderLAN Programmer's Guide (www.ti.com)
 * National Semiconductor DP83840A data sheet (www.national.com)
 * Microchip Technology 24C02C data sheet (www.microchip.com)
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
 * independent interface (MII). The MII allows the ThunderLAN chip to
 * control up to 32 different physical interfaces (PHYs). The ThunderLAN
 * also has a built-in 10baseT PHY, allowing a single ThunderLAN controller
 * to act as a complete ethernet interface.
 *
 * Other PHYs may be attached to the ThunderLAN; the Compaq 10/100 cards
 * use a National Semiconductor DP83840A PHY that supports 10 or 100Mb/sec
 * in full or half duplex. Some of the Compaq Deskpro machines use a
 * Level 1 LXT970 PHY with the same capabilities. A serial EEPROM is also
 * attached to the ThunderLAN chip to provide power-up default register
 * settings and for storing the adapter's stattion address. Although not
 * supported by this driver, the ThunderLAN chip can also be connected
 * to token ring PHYs.
 *
 * It is important to note that while it is possible to have multiple
 * PHYs attached to the ThunderLAN's MII, only one PHY may be active at
 * any time. (This makes me wonder exactly how the dual port Compaq
 * adapter is supposed to work.) This driver attempts to compensate for
 * this in the following way:
 *
 * When the ThunderLAN chip is probed, the probe routine attempts to
 * locate all attached PHYs by checking all 32 possible PHY addresses
 * (0x00 to 0x1F). Each PHY is attached as a separate logical interface.
 * The driver allows any one interface to be brought up at any given
 * time: if an attempt is made to bring up a second PHY while another
 * PHY is already enabled, the driver will return an error.
 *
 * The ThunderLAN has a set of registers which can be used to issue
 * command, acknowledge interrupts, and to manipulate other internal
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
#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_mib.h>
#include <net/if_media.h>
#include <net/if_types.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif

#ifdef IPX
#include <netipx/ipx.h>
#include <netipx/ipx_if.h>
#endif

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#include <vm/vm.h>              /* for vtophys */
#include <vm/vm_param.h>        /* for vtophys */
#include <vm/pmap.h>            /* for vtophys */
#include <machine/clock.h>      /* for DELAY */

#include <pci/pcireg.h>
#include <pci/pcivar.h>

#include <pci/if_tlreg.h>

#ifndef lint
static char rcsid[] =
	"$Id: if_tl.c,v 1.2 1998/05/21 16:24:04 jkh Exp $";
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
	{ COMPAQ_VENDORID, COMPAQ_DEVICEID_DESKPRO_4000_5233MMX,
		"Compaq Deskpro 4000 5233MMX" },
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

static struct tl_iflist		*tl_iflist = NULL;
static unsigned long		tl_count;

static char *tl_probe		__P((pcici_t, pcidi_t));
static void tl_attach_ctlr	__P((pcici_t, int));
static int tl_attach_phy	__P((struct tl_csr *, int, char *,
					int, struct tl_iflist *));
static int tl_intvec_invalid	__P((void *, u_int32_t));
static int tl_intvec_dummy	__P((void *, u_int32_t));
static int tl_intvec_rxeoc	__P((void *, u_int32_t));
static int tl_intvec_txeoc	__P((void *, u_int32_t));
static int tl_intvec_txeof	__P((void *, u_int32_t));
static int tl_intvec_rxeof	__P((void *, u_int32_t));
static int tl_intvec_adchk	__P((void *, u_int32_t));
static int tl_intvec_netsts	__P((void *, u_int32_t));
static int tl_intvec_statoflow	__P((void *, u_int32_t));

static int tl_newbuf		__P((struct tl_softc *, struct tl_chain *));
static void tl_stats_update	__P((void *));
static int tl_encap		__P((struct tl_softc *, struct tl_chain *,
						struct mbuf *));

static void tl_intr		__P((void *));
static void tl_start		__P((struct ifnet *));
static int tl_ioctl		__P((struct ifnet *, int, caddr_t));
static void tl_init		__P((void *));
static void tl_stop		__P((struct tl_softc *));
static void tl_watchdog		__P((struct ifnet *));
static void tl_shutdown		__P((int, void *));
static int tl_ifmedia_upd	__P((struct ifnet *));
static void tl_ifmedia_sts	__P((struct ifnet *, struct ifmediareq *));

static u_int8_t tl_eeprom_putbyte	__P((struct tl_csr *, u_int8_t));
static u_int8_t	tl_eeprom_getbyte	__P((struct tl_csr *, u_int8_t ,
							u_int8_t * ));
static int tl_read_eeprom	__P((struct tl_csr *, caddr_t, int, int));

static void tl_mii_sync		__P((struct tl_csr *));
static void tl_mii_send		__P((struct tl_csr *, u_int32_t, int));
static int tl_mii_readreg	__P((struct tl_csr *, struct tl_mii_frame *));
static int tl_mii_writereg	__P((struct tl_csr *, struct tl_mii_frame *));
static u_int16_t tl_phy_readreg	__P((struct tl_softc *, int));
static void tl_phy_writereg	__P((struct tl_softc *, u_int16_t, u_int16_t));

static void tl_autoneg		__P((struct tl_softc *, int, int));
static void tl_setmode		__P((struct tl_softc *, int));
static int tl_calchash		__P((char *));
static void tl_setmulti		__P((struct tl_softc *));
static void tl_softreset	__P((struct tl_csr *, int));
static int tl_list_rx_init	__P((struct tl_softc *));
static int tl_list_tx_init	__P((struct tl_softc *));

/*
 * ThunderLAN adapters typically have a serial EEPROM containing
 * configuration information. The main reason we're interested in
 * it is because it also contains the adapters's station address.
 *
 * Access to the EEPROM is a bit goofy since it is a serial device:
 * you have to do reads and writes one bit at a time. The state of
 * the DATA bit can only change while the CLOCK line is held low.
 * Transactions work basically like this:
 *
 * 1) Send the EEPROM_START sequence to prepare the EEPROM for
 *    accepting commands. This pulls the clock high, sets
 *    the data bit to 0, enables transmission to the EEPROM,
 *    pulls the data bit up to 1, then pulls the clock low.
 *    The idea is to do a 0 to 1 transition of the data bit
 *    while the clock pin is held high.
 *
 * 2) To write a bit to the EEPROM, set the TXENABLE bit, then
 *    set the EDATA bit to send a 1 or clear it to send a 0.
 *    Finally, set and then clear ECLOK. Strobing the clock
 *    transmits the bit. After 8 bits have been written, the
 *    EEPROM should respond with an ACK, which should be read.
 *
 * 3) To read a bit from the EEPROM, clear the TXENABLE bit,
 *    then set ECLOK. The bit can then be read by reading EDATA.
 *    ECLOCK should then be cleared again. This can be repeated
 *    8 times to read a whole byte, after which the 
 *
 * 4) We need to send the address byte to the EEPROM. For this
 *    we have to send the write control byte to the EEPROM to
 *    tell it to accept data. The byte is 0xA0. The EEPROM should
 *    ack this. The address byte can be send after that.
 *
 * 5) Now we have to tell the EEPROM to send us data. For that we
 *    have to transmit the read control byte, which is 0xA1. This
 *    byte should also be acked. We can then read the data bits
 *    from the EEPROM.
 *
 * 6) When we're all finished, send the EEPROM_STOP sequence.
 *
 * Note that we use the ThunderLAN's NetSio register to access the
 * EEPROM, however there is an alternate method. There is a PCI NVRAM
 * register at PCI offset 0xB4 which can also be used with minor changes.
 * The difference is that access to PCI registers via pci_conf_read()
 * and pci_conf_write() is done using programmed I/O, which we want to
 * avoid.
 */

/*
 * Note that EEPROM_START leaves transmission enabled.
 */
#define EEPROM_START							\
	DIO_SEL(TL_NETSIO);						\
	DIO_BYTE1_SET(TL_SIO_ECLOK); /* Pull clock pin high */		\
	DIO_BYTE1_SET(TL_SIO_EDATA); /* Set DATA bit to 1 */		\
	DIO_BYTE1_SET(TL_SIO_ETXEN); /* Enable xmit to write bit */	\
	DIO_BYTE1_CLR(TL_SIO_EDATA); /* Pull DATA bit to 0 again */	\
	DIO_BYTE1_CLR(TL_SIO_ECLOK); /* Pull clock low again */

/*
 * EEPROM_STOP ends access to the EEPROM and clears the ETXEN bit so
 * that no further data can be written to the EEPROM I/O pin.
 */
#define EEPROM_STOP							\
	DIO_SEL(TL_NETSIO);						\
	DIO_BYTE1_CLR(TL_SIO_ETXEN); /* Disable xmit */			\
	DIO_BYTE1_CLR(TL_SIO_EDATA); /* Pull DATA to 0 */		\
	DIO_BYTE1_SET(TL_SIO_ECLOK); /* Pull clock high */		\
	DIO_BYTE1_SET(TL_SIO_ETXEN); /* Enable xmit */			\
	DIO_BYTE1_SET(TL_SIO_EDATA); /* Toggle DATA to 1 */		\
	DIO_BYTE1_CLR(TL_SIO_ETXEN); /* Disable xmit. */		\
	DIO_BYTE1_CLR(TL_SIO_ECLOK); /* Pull clock low again */

/*
 * Send an instruction or address to the EEPROM, check for ACK.
 */
static u_int8_t tl_eeprom_putbyte(csr, byte)
	struct tl_csr		*csr;
	u_int8_t		byte;
{
	register int		i, ack = 0;

	/*
	 * Make sure we're in TX mode.
	 */
	DIO_SEL(TL_NETSIO);
	DIO_BYTE1_SET(TL_SIO_ETXEN);

	/*
	 * Feed in each bit and stobe the clock.
	 */
	for (i = 0x80; i; i >>= 1) {
		DIO_SEL(TL_NETSIO);
		if (byte & i) {
			DIO_BYTE1_SET(TL_SIO_EDATA);
		} else {
			DIO_BYTE1_CLR(TL_SIO_EDATA);
		}
		DIO_BYTE1_SET(TL_SIO_ECLOK);
		DIO_BYTE1_CLR(TL_SIO_ECLOK);
	}

	/*
	 * Turn off TX mode.
	 */
	DIO_BYTE1_CLR(TL_SIO_ETXEN);

	/*
	 * Check for ack.
	 */
	DIO_BYTE1_SET(TL_SIO_ECLOK);
	ack = DIO_BYTE1_GET(TL_SIO_EDATA);
	DIO_BYTE1_CLR(TL_SIO_ECLOK);

	return(ack);
}

/*
 * Read a byte of data stored in the EEPROM at address 'addr.'
 */
static u_int8_t tl_eeprom_getbyte(csr, addr, dest)
	struct tl_csr		*csr;
	u_int8_t		addr;
	u_int8_t		*dest;
{
	register int		i;
	u_int8_t		byte = 0;

	EEPROM_START;
	/*
	 * Send write control code to EEPROM.
	 */
	if (tl_eeprom_putbyte(csr, EEPROM_CTL_WRITE))
		return(1);

	/*
	 * Send address of byte we want to read.
	 */
	if (tl_eeprom_putbyte(csr, addr))
		return(1);

	EEPROM_STOP;
	EEPROM_START;
	/*
	 * Send read control code to EEPROM.
	 */
	if (tl_eeprom_putbyte(csr, EEPROM_CTL_READ))
		return(1);

	/*
	 * Start reading bits from EEPROM.
	 */
	DIO_SEL(TL_NETSIO);
	DIO_BYTE1_CLR(TL_SIO_ETXEN);
	for (i = 0x80; i; i >>= 1) {
		DIO_SEL(TL_NETSIO);
		DIO_BYTE1_SET(TL_SIO_ECLOK);
		if (DIO_BYTE1_GET(TL_SIO_EDATA))
			byte |= i;
		DIO_BYTE1_CLR(TL_SIO_ECLOK);
	}

	EEPROM_STOP;

	/*
	 * No ACK generated for read, so just return byte.
	 */

	*dest = byte;

	return(0);
}

static void tl_mii_sync(csr)
	struct tl_csr		*csr;
{
	register int		i;

	DIO_SEL(TL_NETSIO);
	DIO_BYTE1_CLR(TL_SIO_MTXEN);

	for (i = 0; i < 32; i++) {
		DIO_BYTE1_SET(TL_SIO_MCLK);
		DIO_BYTE1_CLR(TL_SIO_MCLK);
	}

	return;
}

static void tl_mii_send(csr, bits, cnt)
	struct tl_csr		*csr;
	u_int32_t		bits;
	int			cnt;
{
	int			i;

	for (i = (0x1 << (cnt - 1)); i; i >>= 1) {
		DIO_BYTE1_CLR(TL_SIO_MCLK);
		if (bits & i) {
			DIO_BYTE1_SET(TL_SIO_MDATA);
		} else {
			DIO_BYTE1_CLR(TL_SIO_MDATA);
		}
		DIO_BYTE1_SET(TL_SIO_MCLK);
	}
}

static int tl_mii_readreg(csr, frame)
	struct tl_csr		*csr;
	struct tl_mii_frame	*frame;
	
{
	int			i, ack, s;
	int			minten = 0;

	s = splimp();

	tl_mii_sync(csr);

	/*
	 * Set up frame for RX.
	 */
	frame->mii_stdelim = TL_MII_STARTDELIM;
	frame->mii_opcode = TL_MII_READOP;
	frame->mii_turnaround = 0;
	frame->mii_data = 0;
	
	/*
	 * Select the NETSIO register. We will be using it
 	 * to communicate indirectly with the MII.
	 */

	DIO_SEL(TL_NETSIO);

	/*
	 * Turn off MII interrupt by forcing MINTEN low.
	 */
	minten = DIO_BYTE1_GET(TL_SIO_MINTEN);
	if (minten) {
		DIO_BYTE1_CLR(TL_SIO_MINTEN);
	}

	/*
 	 * Turn on data xmit.
	 */
	DIO_BYTE1_SET(TL_SIO_MTXEN);

	/*
	 * Send command/address info.
	 */
	tl_mii_send(csr, frame->mii_stdelim, 2);
	tl_mii_send(csr, frame->mii_opcode, 2);
	tl_mii_send(csr, frame->mii_phyaddr, 5);
	tl_mii_send(csr, frame->mii_regaddr, 5);

	/*
	 * Turn off xmit.
	 */
	DIO_BYTE1_CLR(TL_SIO_MTXEN);

	/* Idle bit */
	DIO_BYTE1_CLR(TL_SIO_MCLK);
	DIO_BYTE1_SET(TL_SIO_MCLK);

	/* Check for ack */
	DIO_BYTE1_CLR(TL_SIO_MCLK);
	ack = DIO_BYTE1_GET(TL_SIO_MDATA);

	/* Complete the cycle */
	DIO_BYTE1_SET(TL_SIO_MCLK);

	/*
	 * Now try reading data bits. If the ack failed, we still
	 * need to clock through 16 cycles to keep the PHYs in sync.
	 */
	if (ack) {
		for(i = 0; i < 16; i++) {
			DIO_BYTE1_CLR(TL_SIO_MCLK);
			DIO_BYTE1_SET(TL_SIO_MCLK);
		}
		goto fail;
	}

	for (i = 0x8000; i; i >>= 1) {
		DIO_BYTE1_CLR(TL_SIO_MCLK);
		if (!ack) {
			if (DIO_BYTE1_GET(TL_SIO_MDATA))
				frame->mii_data |= i;
		}
		DIO_BYTE1_SET(TL_SIO_MCLK);
	}

fail:

	DIO_BYTE1_CLR(TL_SIO_MCLK);
	DIO_BYTE1_SET(TL_SIO_MCLK);

	/* Reenable interrupts */
	if (minten) {
		DIO_BYTE1_SET(TL_SIO_MINTEN);
	}

	splx(s);

	if (ack)
		return(1);
	return(0);
}

static int tl_mii_writereg(csr, frame)
	struct tl_csr		*csr;
	struct tl_mii_frame	*frame;
	
{
	int			s;
	int			minten;

	tl_mii_sync(csr);

	s = splimp();
	/*
	 * Set up frame for TX.
	 */

	frame->mii_stdelim = TL_MII_STARTDELIM;
	frame->mii_opcode = TL_MII_WRITEOP;
	frame->mii_turnaround = TL_MII_TURNAROUND;
	
	/*
	 * Select the NETSIO register. We will be using it
 	 * to communicate indirectly with the MII.
	 */

	DIO_SEL(TL_NETSIO);

	/*
	 * Turn off MII interrupt by forcing MINTEN low.
	 */
	minten = DIO_BYTE1_GET(TL_SIO_MINTEN);
	if (minten) {
		DIO_BYTE1_CLR(TL_SIO_MINTEN);
	}

	/*
 	 * Turn on data output.
	 */
	DIO_BYTE1_SET(TL_SIO_MTXEN);

	tl_mii_send(csr, frame->mii_stdelim, 2);
	tl_mii_send(csr, frame->mii_opcode, 2);
	tl_mii_send(csr, frame->mii_phyaddr, 5);
	tl_mii_send(csr, frame->mii_regaddr, 5);
	tl_mii_send(csr, frame->mii_turnaround, 2);
	tl_mii_send(csr, frame->mii_data, 16);

	DIO_BYTE1_SET(TL_SIO_MCLK);
	DIO_BYTE1_CLR(TL_SIO_MCLK);

	/*
	 * Turn off xmit.
	 */
	DIO_BYTE1_CLR(TL_SIO_MTXEN);

	/* Reenable interrupts */
	if (minten)
		DIO_BYTE1_SET(TL_SIO_MINTEN);

	splx(s);

	return(0);
}

static u_int16_t tl_phy_readreg(sc, reg)
	struct tl_softc		*sc;
	int			reg;
{
	struct tl_mii_frame	frame;
	struct tl_csr		*csr;

	bzero((char *)&frame, sizeof(frame));

	csr = sc->csr;

	frame.mii_phyaddr = sc->tl_phy_addr;
	frame.mii_regaddr = reg;
	tl_mii_readreg(sc->csr, &frame);

	/* Reenable MII interrupts, just in case. */
	DIO_SEL(TL_NETSIO);
	DIO_BYTE1_SET(TL_SIO_MINTEN);

	return(frame.mii_data);
}

static void tl_phy_writereg(sc, reg, data)
	struct tl_softc		*sc;
	u_int16_t		reg;
	u_int16_t		data;
{
	struct tl_mii_frame	frame;
	struct tl_csr		*csr;

	bzero((char *)&frame, sizeof(frame));

	csr = sc->csr;
	frame.mii_phyaddr = sc->tl_phy_addr;
	frame.mii_regaddr = reg;
	frame.mii_data = data;

	tl_mii_writereg(sc->csr, &frame);

	/* Reenable MII interrupts, just in case. */
	DIO_SEL(TL_NETSIO);
	DIO_BYTE1_SET(TL_SIO_MINTEN);

	return;
}

/*
 * Read a sequence of bytes from the EEPROM.
 */
static int tl_read_eeprom(csr, dest, off, cnt)
	struct tl_csr		*csr;
	caddr_t			dest;
	int			off;
	int			cnt;
{
	int			err = 0, i;
	u_int8_t		byte = 0;

	for (i = 0; i < cnt; i++) {
		err = tl_eeprom_getbyte(csr, off + i, &byte);
		if (err)
			break;
		*(dest + i) = byte;
	}

	return(err ? 1 : 0);
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
 */
static void tl_autoneg(sc, flag, verbose)
	struct tl_softc		*sc;
	int			flag;
	int			verbose;
{
	u_int16_t		phy_sts = 0, media = 0;
	struct ifnet		*ifp;
	struct ifmedia		*ifm;
	struct tl_csr		*csr;

	ifm = &sc->ifmedia;
	ifp = &sc->arpcom.ac_if;
	csr = sc->csr;

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
		phy_sts = tl_phy_readreg(sc, PHY_BMCR);
		phy_sts |= PHY_BMCR_AUTONEGENBL|PHY_BMCR_AUTONEGRSTR;
		tl_phy_writereg(sc, PHY_BMCR, phy_sts);
		DELAY(3000000);
		break;
	case TL_FLAG_SCHEDDELAY:
		phy_sts = tl_phy_readreg(sc, PHY_BMCR);
		phy_sts |= PHY_BMCR_AUTONEGENBL|PHY_BMCR_AUTONEGRSTR;
		tl_phy_writereg(sc, PHY_BMCR, phy_sts);
		ifp->if_timer = 3;
		sc->tl_autoneg = 1;
		return;
	case TL_FLAG_DELAYTIMEO:
		ifp->if_timer = 0;
		sc->tl_autoneg = 0;
		break;
	default:
		printf("tl%d: invalid autoneg flag: %d\n", flag, sc->tl_unit);
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
		media = tl_phy_readreg(sc, PHY_BMCR);

		/* Set the DUPLEX bit in the NetCmd register accordingly. */
		if (media & PHY_BMCR_DUPLEX) {
			if (verbose)
				printf("(full-duplex, ");
			ifm->ifm_media |= IFM_FDX;
			ifm->ifm_media &= ~IFM_HDX;
			DIO_SEL(TL_NETCMD);
			DIO_BYTE0_SET(TL_CMD_DUPLEX);
		} else {
			if (verbose)
				printf("(half-duplex, ");
			ifm->ifm_media &= ~IFM_FDX;
			ifm->ifm_media |= IFM_HDX;
			DIO_SEL(TL_NETCMD);
			DIO_BYTE0_CLR(TL_CMD_DUPLEX);
		}

		if (media & PHY_BMCR_SPEEDSEL) {
			if (verbose)
				printf("100Mb/s)\n");
			ifm->ifm_media |= IFM_100_TX;
			ifm->ifm_media &= ~IFM_10_T;
		} else {
			if (verbose)
				printf("10Mb/s)\n");
			ifm->ifm_media &= ~IFM_100_TX;
			ifm->ifm_media |= IFM_10_T;
		}

		/* Turn off autoneg */
		media &= ~PHY_BMCR_AUTONEGENBL;
		tl_phy_writereg(sc, PHY_BMCR, media);
	} else {
		if (verbose)
			printf("no carrier\n");
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
	u_int16_t		bmcr, anar, ctl;
	struct tl_csr		*csr;

	csr = sc->csr;
	bmcr = tl_phy_readreg(sc, PHY_BMCR);
	anar = tl_phy_readreg(sc, PHY_ANAR);
	ctl = tl_phy_readreg(sc, TL_PHY_CTL);
	DIO_SEL(TL_NETCMD);

	bmcr &= ~(PHY_BMCR_SPEEDSEL|PHY_BMCR_DUPLEX|PHY_BMCR_AUTONEGENBL|
		  PHY_BMCR_LOOPBK);
	anar &= ~(PHY_ANAR_100BT4|PHY_ANAR_100BTXFULL|PHY_ANAR_100BTXHALF|
		  PHY_ANAR_10BTFULL|PHY_ANAR_10BTHALF);

	ctl &= ~PHY_CTL_AUISEL;

	if (IFM_SUBTYPE(media) == IFM_LOOP)
		bmcr |= PHY_BMCR_LOOPBK;

	if (IFM_SUBTYPE(media) == IFM_AUTO)
		bmcr |= PHY_BMCR_AUTONEGENBL;

	if (IFM_SUBTYPE(media) == IFM_10_5)
		ctl |= PHY_CTL_AUISEL;

	if (IFM_SUBTYPE(media) == IFM_100_TX) {
		bmcr |= PHY_BMCR_SPEEDSEL;
		if ((media & IFM_GMASK) == IFM_FDX) {
			bmcr |= PHY_BMCR_DUPLEX;
			anar |= PHY_ANAR_100BTXFULL;
			DIO_BYTE0_SET(TL_CMD_DUPLEX);
		} else if ((media & IFM_GMASK) == IFM_HDX) {
			bmcr &= ~PHY_BMCR_DUPLEX;
			anar |= PHY_ANAR_100BTXHALF;
			DIO_BYTE0_CLR(TL_CMD_DUPLEX);
		} else {
			bmcr &= ~PHY_BMCR_DUPLEX;
			anar |= PHY_ANAR_100BTXHALF;
			DIO_BYTE0_CLR(TL_CMD_DUPLEX);
		}
	}

	if (IFM_SUBTYPE(media) == IFM_10_T) {
		bmcr &= ~PHY_BMCR_SPEEDSEL;
		if ((media & IFM_GMASK) == IFM_FDX) {
			bmcr |= PHY_BMCR_DUPLEX;
			anar |= PHY_ANAR_10BTFULL;
			DIO_BYTE0_SET(TL_CMD_DUPLEX);
		} else if ((media & IFM_GMASK) == IFM_HDX) {
			bmcr &= ~PHY_BMCR_DUPLEX;
			anar |= PHY_ANAR_10BTHALF;
			DIO_BYTE0_CLR(TL_CMD_DUPLEX);
		} else {
			bmcr &= ~PHY_BMCR_DUPLEX;
			anar |= PHY_ANAR_10BTHALF;
			DIO_BYTE0_CLR(TL_CMD_DUPLEX);
		}
	}

	tl_phy_writereg(sc, PHY_BMCR, bmcr);
	tl_phy_writereg(sc, PHY_ANAR, anar);
	tl_phy_writereg(sc, TL_PHY_CTL, ctl);

	return;
}

#define XOR(a, b)		((a && !b) || (!a && b))
#define DA(addr, offset)	(addr[offset / 8] & (1 << (offset % 8)))

static int tl_calchash(addr)
	char			*addr;
{
	int			h;

	h = XOR(DA(addr, 0), XOR(DA(addr, 6), XOR(DA(addr, 12),
	    XOR(DA(addr, 18), XOR(DA(addr, 24), XOR(DA(addr, 30),
	    XOR(DA(addr, 36), DA(addr, 42))))))));

	h |= XOR(DA(addr, 1), XOR(DA(addr, 7), XOR(DA(addr, 13),
	     XOR(DA(addr, 19), XOR(DA(addr, 25), XOR(DA(addr, 31),
	     XOR(DA(addr, 37), DA(addr, 43)))))))) << 1;

	h |= XOR(DA(addr, 2), XOR(DA(addr, 8), XOR(DA(addr, 14),
	     XOR(DA(addr, 20), XOR(DA(addr, 26), XOR(DA(addr, 32),
	     XOR(DA(addr, 38), DA(addr, 44)))))))) << 2;

	h |= XOR(DA(addr, 3), XOR(DA(addr, 9), XOR(DA(addr, 15),
	     XOR(DA(addr, 21), XOR(DA(addr, 27), XOR(DA(addr, 33),
	     XOR(DA(addr, 39), DA(addr, 45)))))))) << 3;

	h |= XOR(DA(addr, 4), XOR(DA(addr, 10), XOR(DA(addr, 16),
	     XOR(DA(addr, 22), XOR(DA(addr, 28), XOR(DA(addr, 34),
	     XOR(DA(addr, 40), DA(addr, 46)))))))) << 4;

	h |= XOR(DA(addr, 5), XOR(DA(addr, 11), XOR(DA(addr, 17),
	     XOR(DA(addr, 23), XOR(DA(addr, 29), XOR(DA(addr, 35),
	     XOR(DA(addr, 41), DA(addr, 47)))))))) << 5;

	return(h);
}

static void tl_setmulti(sc)
	struct tl_softc		*sc;
{
	struct ifnet		*ifp;
	struct tl_csr		*csr;
	u_int32_t		hashes[2] = { 0, 0 };
	int			h;
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
	struct ifmultiaddr	*ifma;
#else
	struct ether_multi	*enm;
	struct ether_multistep	step;
#endif

	csr = sc->csr;
	ifp = &sc->arpcom.ac_if;

	if (sc->arpcom.ac_multicnt > 64 || ifp->if_flags & IFF_ALLMULTI) {
		hashes[0] = 0xFFFFFFFF;
		hashes[1] = 0xFFFFFFFF;
	} else {
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
		for (ifma = ifp->if_multiaddrs.lh_first; ifma != NULL;
					ifma = ifma->ifma_link.le_next) {
			if (ifma->ifma_addr->sa_family != AF_LINK)
				continue;
			h = tl_calchash(
				LLADDR((struct sockaddr_dl *)ifma->ifma_addr));
			if (h < 32)
				hashes[0] |= (1 << h);
			else
				hashes[1] |= (1 << (h - 31));
		}
#else
		ETHER_FIRST_MULTI(step, &sc->arpcom, enm);
		while(enm != NULL) {
			if (bcmp(enm->enm_addrlo, enm->enm_addrhi,
						ETHER_ADDR_LEN)) {
				hashes[0] = 0xFFFFFFFF;
				hashes[1] = 0xFFFFFFFF;
				break;
			} else {
				h = tl_calchash(enm->enm_addrlo);
				if (h < 32)
					hashes[0] |= (1 << h);
				else
					hashes[1] |= (1 << (h - 31));
			}
			ETHER_NEXT_MULTI(step, enm);
		}
#endif
	}

	DIO_SEL(TL_HASH1);
	DIO_LONG_PUT(hashes[0]);
	DIO_SEL(TL_HASH2);
	DIO_LONG_PUT(hashes[1]);

	return;
}

static void tl_softreset(csr, internal)
        struct tl_csr           *csr;
	int			internal;
{
        u_int32_t               cmd, dummy;

        /* Assert the adapter reset bit. */
        csr->tl_host_cmd |= TL_CMD_ADRST;
        /* Turn off interrupts */
        csr->tl_host_cmd |= TL_CMD_INTSOFF;

	/* First, clear the stats registers. */
	DIO_SEL(TL_TXGOODFRAMES|TL_DIO_ADDR_INC);
	DIO_LONG_GET(dummy);
	DIO_LONG_GET(dummy);
	DIO_LONG_GET(dummy);
	DIO_LONG_GET(dummy);
	DIO_LONG_GET(dummy);

        /* Clear Areg and Hash registers */
	DIO_SEL(TL_AREG0_B5|TL_DIO_ADDR_INC);
	DIO_LONG_PUT(0x00000000);
	DIO_LONG_PUT(0x00000000);
	DIO_LONG_PUT(0x00000000);
	DIO_LONG_PUT(0x00000000);
	DIO_LONG_PUT(0x00000000);
	DIO_LONG_PUT(0x00000000);
	DIO_LONG_PUT(0x00000000);
	DIO_LONG_PUT(0x00000000);

        /*
	 * Set up Netconfig register. Enable one channel and
	 * one fragment mode.
	 */
	DIO_SEL(TL_NETCONFIG);
	DIO_WORD0_SET(TL_CFG_ONECHAN|TL_CFG_ONEFRAG);
	if (internal) {
		DIO_SEL(TL_NETCONFIG);
		DIO_WORD0_SET(TL_CFG_PHYEN);
	} else {
		DIO_SEL(TL_NETCONFIG);
		DIO_WORD0_CLR(TL_CFG_PHYEN);
	}

        /* Set PCI burst size */
        DIO_SEL(TL_BSIZEREG);
        DIO_BYTE1_SET(0x33);

	/*
	 * Load adapter irq pacing timer and tx threshold.
	 * We make the transmit threshold 1 initially but we may
	 * change that later.
	 */
	cmd = csr->tl_host_cmd;
	cmd |= TL_CMD_NES;
	cmd &= ~(TL_CMD_RT|TL_CMD_EOC|TL_CMD_ACK_MASK|TL_CMD_CHSEL_MASK);
	csr->tl_host_cmd = cmd | (TL_CMD_LDTHR | TX_THR);
	csr->tl_host_cmd = cmd | (TL_CMD_LDTMR | 0x00000003);

        /* Unreset the MII */
        DIO_SEL(TL_NETSIO);
        DIO_BYTE1_SET(TL_SIO_NMRST);

	/* Clear status register */
        DIO_SEL(TL_NETSTS);
        DIO_BYTE2_SET(TL_STS_MIRQ);
        DIO_BYTE2_SET(TL_STS_HBEAT);
        DIO_BYTE2_SET(TL_STS_TXSTOP);
        DIO_BYTE2_SET(TL_STS_RXSTOP);

	/* Enable network status interrupts for everything. */
	DIO_SEL(TL_NETMASK);
	DIO_BYTE3_SET(TL_MASK_MASK7|TL_MASK_MASK6|
			TL_MASK_MASK5|TL_MASK_MASK4);

	/* Take the adapter out of reset */
	DIO_SEL(TL_NETCMD);
	DIO_BYTE0_SET(TL_CMD_NRESET|TL_CMD_NWRAP);

	/* Wait for things to settle down a little. */
	DELAY(500);

        return;
}

/*
 * Probe for a ThunderLAN chip. Check the PCI vendor and device IDs
 * against our list and return its name if we find a match. Note that
 * we also save a pointer to the tl_type struct for this card since we
 * will need it for the softc struct and attach routine later.
 */
static char *
tl_probe(config_id, device_id)
	pcici_t			config_id;
	pcidi_t			device_id;
{
	struct tl_type		*t;
	struct tl_iflist	*new;

	t = tl_devs;

	while(t->tl_name != NULL) {
		if ((device_id & 0xFFFF) == t->tl_vid &&
		    ((device_id >> 16) & 0xFFFF) == t->tl_did) {
			new = malloc(sizeof(struct tl_iflist),
					M_DEVBUF, M_NOWAIT);
			if (new == NULL) {
				printf("no memory for controller struct!\n");
				break;
			}
			bzero(new, sizeof(struct tl_iflist));
			new->tl_config_id = config_id;
			new->tl_dinfo = t;
			new->tl_next = tl_iflist;
			tl_iflist = new;
			return(t->tl_name);
		}
		t++;
	}

	return(NULL);
}

/*
 * The ThunderLAN controller can support multiple PHYs. Logically,
 * this means we have to be able to deal with each PHY as a separate
 * interface. We therefore consider ThunderLAN devices as follows:
 *
 * o Each ThunderLAN controller device is assigned the name tlcX where
 *   X is the controller's unit number. Each ThunderLAN device found
 *   is assigned a different number.
 *
 * o Each PHY on each controller is assigned the name tlX. X starts at
 *   0 and is incremented each time an additional PHY is found.
 *
 * So, if you had two dual-channel ThunderLAN cards, you'd have
 * tlc0 and tlc1 (the controllers) and tl0, tl1, tl2, tl3 (the logical
 * interfaces). I think. I'm still not sure how dual chanel controllers
 * work as I've yet to see one.
 */

/*
 * Do the interface setup and attach for a PHY on a particular
 * ThunderLAN chip. Also also set up interrupt vectors.
 */ 
static int tl_attach_phy(csr, tl_unit, eaddr, tl_phy, ilist)
	struct tl_csr		*csr;
	int			tl_unit;
	char			*eaddr;
	int			tl_phy;
	struct tl_iflist	*ilist;
{
	struct tl_softc		*sc;
	struct ifnet		*ifp;
	int			phy_ctl;
	struct tl_type		*p = tl_phys;
	struct tl_mii_frame	frame;
	int			i, media = IFM_ETHER|IFM_100_TX|IFM_FDX;
	unsigned int		round;
	caddr_t			roundptr;

	if (tl_phy != TL_PHYADDR_MAX)
		tl_softreset(csr, 0);

	/* Reset the PHY again, just in case. */
	bzero((char *)&frame, sizeof(frame));
	frame.mii_phyaddr = tl_phy;
	frame.mii_regaddr = TL_PHY_GENCTL;
	frame.mii_data = PHY_BMCR_RESET;
	tl_mii_writereg(csr, &frame);
	DELAY(500);
	frame.mii_data = 0;

	/* First, allocate memory for the softc struct. */
	sc = malloc(sizeof(struct tl_softc), M_DEVBUF, M_NOWAIT);
	if (sc == NULL) {
		printf("tlc%d: no memory for softc struct!\n", ilist->tlc_unit);
		return(1);
	}

	bzero(sc, sizeof(struct tl_softc));

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
		printf("tlc%d: no memory for list buffers!\n", ilist->tlc_unit);
		return(1);
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

	sc->csr = csr;
	sc->tl_dinfo = ilist->tl_dinfo;
	sc->tl_ctlr = ilist->tlc_unit;
	sc->tl_unit = tl_unit;
	sc->tl_phy_addr = tl_phy;
	sc->tl_iflist = ilist;
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
	callout_handle_init(&sc->tl_stat_ch);
#endif

	frame.mii_regaddr = TL_PHY_VENID;
	tl_mii_readreg(csr, &frame);
	sc->tl_phy_vid = frame.mii_data;

	frame.mii_regaddr = TL_PHY_DEVID;
	tl_mii_readreg(csr, &frame);
	sc->tl_phy_did = frame.mii_data;

	frame.mii_regaddr = TL_PHY_GENSTS;
	tl_mii_readreg(csr, &frame);
	sc->tl_phy_sts = frame.mii_data;

	frame.mii_regaddr = TL_PHY_GENCTL;
	tl_mii_readreg(csr, &frame);
	phy_ctl = frame.mii_data;

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

	bcopy(eaddr, (char *)&sc->arpcom.ac_enaddr, ETHER_ADDR_LEN);
	ifp = &sc->arpcom.ac_if;
	ifp->if_softc = sc;
	ifp->if_unit = tl_unit;
	ifp->if_name = "tl";
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = tl_ioctl;
	ifp->if_output = ether_output;
	ifp->if_start = tl_start;
	ifp->if_watchdog = tl_watchdog;
	ifp->if_init = tl_init;

	if (sc->tl_phy_sts & PHY_BMSR_100BT4 ||
		sc->tl_phy_sts & PHY_BMSR_100BTXFULL ||
		sc->tl_phy_sts & PHY_BMSR_100BTXHALF)
		ifp->if_baudrate = 100000000;
	else
		ifp->if_baudrate = 10000000;

	ilist->tl_sc[tl_phy] = sc;

	printf("tl%d at tlc%d physical interface %d\n", ifp->if_unit,
						sc->tl_ctlr,
						sc->tl_phy_addr);

	printf("tl%d: %s ", ifp->if_unit, sc->tl_pinfo->tl_name);

	if (sc->tl_phy_sts & PHY_BMSR_100BT4 ||
		sc->tl_phy_sts & PHY_BMSR_100BTXHALF ||
		sc->tl_phy_sts & PHY_BMSR_100BTXHALF)
		printf("10/100Mbps ");
	else {
		media &= ~IFM_100_TX;
		media |= IFM_10_T;
		printf("10Mbps ");
	}

	if (sc->tl_phy_sts & PHY_BMSR_100BTXFULL ||
		sc->tl_phy_sts & PHY_BMSR_10BTFULL)
		printf("full duplex ");
	else {
		printf("half duplex ");
		media &= ~IFM_FDX;
	}

	if (sc->tl_phy_sts & PHY_BMSR_CANAUTONEG) {
		media = IFM_ETHER|IFM_AUTO;
		printf("autonegotiating\n");
	} else
		printf("\n");

	/* If this isn't a known PHY, print the PHY indentifier info. */
	if (sc->tl_pinfo->tl_vid == 0)
		printf("tl%d: vendor id: %04x product id: %04x\n",
			sc->tl_unit, sc->tl_phy_vid, sc->tl_phy_did);

	/* Set up ifmedia data and callbacks. */
	ifmedia_init(&sc->ifmedia, 0, tl_ifmedia_upd, tl_ifmedia_sts);

	/*
	 * All ThunderLANs support at least 10baseT half duplex.
	 * They also support AUI selection if used in 10Mb/s modes.
	 * They all also support a loopback mode.
	 */
	ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_10_T|IFM_HDX, 0, NULL);
	ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_10_T, 0, NULL);
	ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_10_5, 0, NULL);
	ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_LOOP, 0, NULL);

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
	if (sc->tl_phy_sts & PHY_BMSR_CANAUTONEG)
		tl_autoneg(sc, TL_FLAG_FORCEDELAY, 0);

	/*
	 * Call MI attach routines.
	 */
	if_attach(ifp);
	ether_ifattach(ifp);

#if NBPFILTER > 0
	bpfattach(ifp, DLT_EN10MB, sizeof(struct ether_header));
#endif

	return(0);
}

static void
tl_attach_ctlr(config_id, unit)
	pcici_t			config_id;
	int			unit;
{
	int			s, i, phys = 0;
	vm_offset_t		pbase, vbase;
	struct tl_csr		*csr;
	char			eaddr[ETHER_ADDR_LEN];
	struct tl_mii_frame	frame;
	u_int32_t		command;
	struct tl_iflist	*ilist;

	s = splimp();

	for (ilist = tl_iflist; ilist != NULL; ilist = ilist->tl_next)
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
		if (ilist->tl_config_id == config_id)
#else
		if (sametag(ilist->tl_config_id, config_id))
#endif
			break;

	if (ilist == NULL) {
		printf("couldn't match config id with controller struct\n");
		goto fail;
	}

	/*
	 * Map control/status registers.
	 */
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
	pci_conf_write(config_id, PCI_COMMAND_STATUS_REG,
			PCIM_CMD_MEMEN|PCIM_CMD_BUSMASTEREN);

	command = pci_conf_read(config_id, PCI_COMMAND_STATUS_REG);

	if (!(command & PCIM_CMD_MEMEN)) {
		printf("tlc%d: failed to enable memory mapping!\n", unit);
		goto fail;
	}
#else
	pci_conf_write(config_id, PCI_COMMAND_STATUS_REG,
			PCI_COMMAND_MEM_ENABLE|
			PCI_COMMAND_MASTER_ENABLE);

	command = pci_conf_read(config_id, PCI_COMMAND_STATUS_REG);

	if (!(command & PCI_COMMAND_MEM_ENABLE)) {
		printf("tlc%d: failed to enable memory mapping!\n", unit);
		goto fail;
	}
#endif

	if (!pci_map_mem(config_id, TL_PCI_LOMEM, &vbase, &pbase)) {
		printf ("tlc%d: couldn't map memory\n", unit);
		goto fail;
	}

	csr = (struct tl_csr *)vbase;

	ilist->csr = csr;
	ilist->tl_active_phy = TL_PHYS_IDLE;
	ilist->tlc_unit = unit;

	/* Allocate interrupt */
	if (!pci_map_int(config_id, tl_intr, ilist, &net_imask)) {
		printf("tlc%d: couldn't map interrupt\n", unit);
		goto fail;
	}

	/* Reset the adapter. */
	tl_softreset(csr, 1);

	/*
	 * Get station address from the EEPROM.
	 */
	if (tl_read_eeprom(csr, (caddr_t)&eaddr,
				TL_EEPROM_EADDR, ETHER_ADDR_LEN)) {
		printf("tlc%d: failed to read station address\n", unit);
		goto fail;
	}

	/*
	 * A ThunderLAN chip was detected. Inform the world.
	 */
	printf("tlc%d: Ethernet address: %6D\n", unit, eaddr, ":");

	/*
	 * Now attach the ThunderLAN's PHYs. There will always
	 * be at least one PHY; if the PHY address is 0x1F, then
	 * it's the internal one. If we encounter a lower numbered
	 * PHY, we ignore the internal once since enabling the
	 * internal PHY disables the external one.
	 */

	bzero((char *)&frame, sizeof(frame));

	for (i = TL_PHYADDR_MIN; i < TL_PHYADDR_MAX + 1; i++) {
		frame.mii_phyaddr = i;
		frame.mii_regaddr = TL_PHY_GENCTL;
		frame.mii_data = PHY_BMCR_RESET;
		tl_mii_writereg(csr, &frame);
		DELAY(500);
		while(frame.mii_data & PHY_BMCR_RESET)
			tl_mii_readreg(csr, &frame);
		frame.mii_regaddr = TL_PHY_VENID;
		frame.mii_data = 0;
		tl_mii_readreg(csr, &frame);
		if (!frame.mii_data)
			continue;
		if (tl_attach_phy(csr, phys, eaddr, i, ilist)) {
			printf("tlc%d: failed to attach interface %d\n",
						unit, i);
			goto fail;
		}
		phys++;
		if (phys && i != TL_PHYADDR_MAX)
			break;
	}

	if (!phys) {
		printf("tlc%d: no physical interfaces attached!\n", unit);
		goto fail;
	}

	at_shutdown(tl_shutdown, ilist, SHUTDOWN_POST_SYNC);

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

	for (i = 0; i < TL_TX_LIST_CNT; i++) {
		cd->tl_rx_chain[i].tl_ptr =
			(struct tl_list *)&ld->tl_rx_list[i];
		tl_newbuf(sc, &cd->tl_rx_chain[i]);
		if (i == (TL_TX_LIST_CNT - 1)) {
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
	struct tl_chain		*c;
{
	struct mbuf		*m_new = NULL;

	MGETHDR(m_new, M_DONTWAIT, MT_DATA);
	if (m_new == NULL) {
		printf("tl%d: no memory for rx list",
				sc->tl_unit);
		return(ENOBUFS);
	}

	MCLGET(m_new, M_DONTWAIT);
	if (!(m_new->m_flags & M_EXT)) {
		printf("tl%d: no memory for rx list", sc->tl_unit);
		m_freem(m_new);
		return(ENOBUFS);
	}

	c->tl_mbuf = m_new;
	c->tl_next = NULL;
	c->tl_ptr->tlist_frsize = MCLBYTES;
	c->tl_ptr->tlist_cstat = TL_CSTAT_READY;
	c->tl_ptr->tlist_fptr = 0;
	c->tl_ptr->tl_frag[0].tlist_dadr = vtophys(mtod(m_new, caddr_t));
	c->tl_ptr->tl_frag[0].tlist_dcnt = MCLBYTES;

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
	struct tl_chain		*cur_rx;

	sc = xsc;
	ifp = &sc->arpcom.ac_if;

	while(sc->tl_cdata.tl_rx_head->tl_ptr->tlist_cstat & TL_CSTAT_FRAMECMP){
		r++;
		cur_rx = sc->tl_cdata.tl_rx_head;
		sc->tl_cdata.tl_rx_head = cur_rx->tl_next;
		m = cur_rx->tl_mbuf;
		total_len = cur_rx->tl_ptr->tlist_frsize;

		tl_newbuf(sc, cur_rx);

		sc->tl_cdata.tl_rx_tail->tl_ptr->tlist_fptr =
						vtophys(cur_rx->tl_ptr);
		sc->tl_cdata.tl_rx_tail->tl_next = cur_rx;
		sc->tl_cdata.tl_rx_tail = cur_rx;

		eh = mtod(m, struct ether_header *);
		m->m_pkthdr.rcvif = ifp;

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
	sc->csr->tl_host_cmd = TL_CMD_ACK | r | (type & ~(0x00100000));
	r = 1;
	sc->csr->tl_ch_parm = vtophys(sc->tl_cdata.tl_rx_head->tl_ptr);
	r |= (TL_CMD_GO|TL_CMD_RT);
	return(r);
}

/*
 * Invalid interrupt handler. The manual says invalid interrupts
 * are caused by a hardware error in other hardware and that they
 * should just be ignored.
 */
static int tl_intvec_invalid(xsc, type)
	void			*xsc;
	u_int32_t		type;
{
	struct tl_softc		*sc;

	sc = xsc;

#ifdef DIAGNOSTIC
	printf("tl%d: got an invalid interrupt!\n", sc->tl_unit);
#endif
	/* Re-enable interrupts but don't ack this one. */
	sc->csr->tl_host_cmd |= type;

	return(0);
}

/*
 * Dummy interrupt handler. Dummy interrupts are generated by setting
 * the ReqInt bit in the host command register. They should only occur
 * if we ask for them, and we never do, so if one magically appears,
 * we should make some noise about it.
 */
static int tl_intvec_dummy(xsc, type)
	void			*xsc;
	u_int32_t		type;
{
	struct tl_softc		*sc;

	sc = xsc;
	printf("tl%d: got a dummy interrupt\n", sc->tl_unit);

	return(1);
}

/*
 * Stats counter overflow interrupt. The chip delivers one of these
 * if we don't poll the stats counters often enough.
 */
static int tl_intvec_statoflow(xsc, type)
	void			*xsc;
	u_int32_t		type;
{
	struct tl_softc		*sc;

	sc = xsc;

	tl_stats_update(sc);

	return(1);
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
	} else {
		sc->tl_txeoc = 0;
		/* First we have to ack the EOC interrupt. */
		sc->csr->tl_host_cmd = TL_CMD_ACK | 0x00000001 | type;
		/* Then load the address of the next TX list. */
		sc->csr->tl_ch_parm = vtophys(sc->tl_cdata.tl_tx_head->tl_ptr);
		/* Restart TX channel. */
		cmd = sc->csr->tl_host_cmd;
		cmd &= ~TL_CMD_RT;
		cmd |= TL_CMD_GO|TL_CMD_INTSON;
		sc->csr->tl_host_cmd = cmd;
		return(0);
	}

	return(1);
}

static int tl_intvec_adchk(xsc, type)
	void			*xsc;
	u_int32_t		type;
{
	struct tl_softc		*sc;

	sc = xsc;

	printf("tl%d: adapter check: %x\n", sc->tl_unit, sc->csr->tl_ch_parm);

	tl_softreset(sc->csr, sc->tl_phy_addr == TL_PHYADDR_MAX ? 1 : 0);
	tl_init(sc);
	sc->csr->tl_host_cmd |= TL_CMD_INTSON;

	return(0);
}

static int tl_intvec_netsts(xsc, type)
	void			*xsc;
	u_int32_t		type;
{
	struct tl_softc		*sc;
	u_int16_t		netsts;
	struct tl_csr		*csr;

	sc = xsc;
	csr = sc->csr;

	DIO_SEL(TL_NETSTS);
	netsts = DIO_BYTE2_GET(0xFF);
	DIO_BYTE2_SET(netsts);

	printf("tl%d: network status: %x\n", sc->tl_unit, netsts);

	return(1);
}

static void tl_intr(xilist)
	void			*xilist;
{
	struct tl_iflist	*ilist;
	struct tl_softc		*sc;
	struct tl_csr		*csr;
	struct ifnet		*ifp;
	int			r = 0;
	u_int32_t		type = 0;
	u_int16_t		ints = 0;
	u_int8_t		ivec = 0;

	ilist = xilist;
	csr = ilist->csr;

	/* Disable interrupts */
	ints = csr->tl_host_int;
	csr->tl_host_int = ints;
	type = (ints << 16) & 0xFFFF0000;
	ivec = (ints & TL_VEC_MASK) >> 5;
	ints = (ints & TL_INT_MASK) >> 2;
	/*
 	 * An interrupt has been posted by the ThunderLAN, but we
	 * have to figure out which PHY generated it before we can
	 * do anything with it. If we receive an interrupt when we
	 * know none of the PHYs are turned on, then either there's
	 * a bug in the driver or we we handed an interrupt that
	 * doesn't actually belong to us.
	 */
	if (ilist->tl_active_phy == TL_PHYS_IDLE) {
		printf("tlc%d: interrupt type %x with all phys idle\n",
			ilist->tlc_unit, ints);
		return;
	}

	sc = ilist->tl_sc[ilist->tl_active_phy];
	csr = sc->csr;
	ifp = &sc->arpcom.ac_if;

	switch(ints) {
	case (TL_INTR_INVALID):
		r = tl_intvec_invalid((void *)sc, type);
		break;
	case (TL_INTR_TXEOF):
		r = tl_intvec_txeof((void *)sc, type);
		break;
	case (TL_INTR_TXEOC):
		r = tl_intvec_txeoc((void *)sc, type);
		break;
	case (TL_INTR_STATOFLOW):
		r = tl_intvec_statoflow((void *)sc, type);
		break;
	case (TL_INTR_RXEOF):
		r = tl_intvec_rxeof((void *)sc, type);
		break;
	case (TL_INTR_DUMMY):
		r = tl_intvec_dummy((void *)sc, type);
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
	if (r)
		csr->tl_host_cmd = TL_CMD_ACK | r | type;

	return;
}

static void tl_stats_update(xsc)
	void			*xsc;
{
	struct tl_softc		*sc;
	struct ifnet		*ifp;
	struct tl_csr		*csr;
	struct tl_stats		tl_stats;
	u_int32_t		*p;

	bzero((char *)&tl_stats, sizeof(struct tl_stats));

	sc = xsc;
	csr = sc->csr;
	ifp = &sc->arpcom.ac_if;

	p = (u_int32_t *)&tl_stats;

	DIO_SEL(TL_TXGOODFRAMES|TL_DIO_ADDR_INC);
	DIO_LONG_GET(*p++);
	DIO_LONG_GET(*p++);
	DIO_LONG_GET(*p++);
	DIO_LONG_GET(*p++);
	DIO_LONG_GET(*p++);

	ifp->if_opackets += tl_tx_goodframes(tl_stats);
	ifp->if_collisions += tl_stats.tl_tx_single_collision +
				tl_stats.tl_tx_multi_collision;
	ifp->if_ipackets += tl_rx_goodframes(tl_stats);
	ifp->if_ierrors += tl_stats.tl_crc_errors + tl_stats.tl_code_errors +
			    tl_rx_overrun(tl_stats);
	ifp->if_oerrors += tl_tx_underrun(tl_stats);

#if defined(__FreeBSD__) && __FreeBSD__ >= 3
	sc->tl_stat_ch = timeout(tl_stats_update, sc, hz);
#else
	timeout(tl_stats_update, sc, hz);
#endif
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
			printf("all frags filled but frame still to small!\n");
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
	struct tl_csr		*csr;
	struct mbuf		*m_head = NULL;
	u_int32_t		cmd;
	struct tl_chain		*prev = NULL, *cur_tx = NULL, *start_tx;

	sc = ifp->if_softc;
	csr = sc->csr;

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
			sc->csr->tl_ch_parm = vtophys(start_tx->tl_ptr);
			cmd = sc->csr->tl_host_cmd;
			cmd &= ~TL_CMD_RT;
			cmd |= TL_CMD_GO|TL_CMD_INTSON;
			sc->csr->tl_host_cmd = cmd;
		}
	} else {
		sc->tl_cdata.tl_tx_tail->tl_next = start_tx;
		sc->tl_cdata.tl_tx_tail->tl_ptr->tlist_fptr =
					vtophys(start_tx->tl_ptr);
		sc->tl_cdata.tl_tx_tail = start_tx;
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
	struct tl_csr		*csr = sc->csr;
        int			s;
	u_int16_t		phy_sts;

	s = splimp();

	ifp = &sc->arpcom.ac_if;

	/*
	 * Cancel pending I/O.
	 */
	tl_stop(sc);

	/*
	 * Set 'capture all frames' bit for promiscuous mode.
	 */
	if (ifp->if_flags & IFF_PROMISC) {
		DIO_SEL(TL_NETCMD);
		DIO_BYTE0_SET(TL_CMD_CAF);
	} else {
		DIO_SEL(TL_NETCMD);
		DIO_BYTE0_CLR(TL_CMD_CAF);
	}

	/*
	 * Set capture broadcast bit to capture broadcast frames.
	 */
	if (ifp->if_flags & IFF_BROADCAST) {
		DIO_SEL(TL_NETCMD);
		DIO_BYTE0_CLR(TL_CMD_NOBRX);
	} else {
		DIO_SEL(TL_NETCMD);
		DIO_BYTE0_SET(TL_CMD_NOBRX);
	}

	/* Init our MAC address */
	DIO_SEL(TL_AREG0_B5);
	csr->u.tl_dio_bytes.byte0 = sc->arpcom.ac_enaddr[0];
	csr->u.tl_dio_bytes.byte1 = sc->arpcom.ac_enaddr[1];
	csr->u.tl_dio_bytes.byte2 = sc->arpcom.ac_enaddr[2];
	csr->u.tl_dio_bytes.byte3 = sc->arpcom.ac_enaddr[3];
	DIO_SEL(TL_AREG0_B1);
	csr->u.tl_dio_bytes.byte0 = sc->arpcom.ac_enaddr[4];
	csr->u.tl_dio_bytes.byte1 = sc->arpcom.ac_enaddr[5];

	/* Init circular RX list. */
	if (tl_list_rx_init(sc)) {
		printf("tl%d: failed to set up rx lists\n", sc->tl_unit);
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
	DIO_SEL(TL_NETSIO);
	DIO_BYTE1_SET(TL_SIO_MINTEN);

	/* Enable PCI interrupts. */	
        csr->tl_host_cmd |= TL_CMD_INTSON;

	/* Load the address of the rx list */
	sc->csr->tl_host_cmd |= TL_CMD_RT;
	sc->csr->tl_ch_parm = vtophys(&sc->tl_ldata->tl_rx_list[0]);

	/* Send the RX go command */
	sc->csr->tl_host_cmd |= (TL_CMD_GO|TL_CMD_RT);
	sc->tl_iflist->tl_active_phy = sc->tl_phy_addr;

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	(void)splx(s);

	/* Start the stats update counter */
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
	sc->tl_stat_ch = timeout(tl_stats_update, sc, hz);
#else
	timeout(tl_stats_update, sc, hz);
#endif

	return;
}

/*
 * Set media options.
 */
static int tl_ifmedia_upd(ifp)
	struct ifnet		*ifp;
{
	struct tl_softc		*sc;
	struct tl_csr		*csr;
	struct ifmedia		*ifm;

	sc = ifp->if_softc;
	csr = sc->csr;
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
	struct tl_csr		*csr;

	sc = ifp->if_softc;
	csr = sc->csr;

	ifmr->ifm_active = IFM_ETHER;

	phy_ctl = tl_phy_readreg(sc, PHY_BMCR);
	phy_sts = tl_phy_readreg(sc, TL_PHY_CTL);

	if (phy_sts & PHY_CTL_AUISEL)
		ifmr->ifm_active |= IFM_10_5;

	if (phy_ctl & PHY_BMCR_LOOPBK)
		ifmr->ifm_active |= IFM_LOOP;

	if (phy_ctl & PHY_BMCR_SPEEDSEL)
		ifmr->ifm_active |= IFM_100_TX;
	else
		ifmr->ifm_active |= IFM_10_T;

	if (phy_ctl & PHY_BMCR_DUPLEX) {
		ifmr->ifm_active |= IFM_FDX;
		ifmr->ifm_active &= ~IFM_HDX;
	} else {
		ifmr->ifm_active &= ~IFM_FDX;
		ifmr->ifm_active |= IFM_HDX;
	}

	if (phy_ctl & PHY_BMCR_AUTONEGENBL)
		ifmr->ifm_active |= IFM_AUTO;

	return;
}

static int tl_ioctl(ifp, command, data)
	struct ifnet		*ifp;
	int			command;
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
		/*
		 * Make sure no more than one PHY is active
		 * at any one time.
		 */
		if (ifp->if_flags & IFF_UP) {
			if (sc->tl_iflist->tl_active_phy != TL_PHYS_IDLE &&
			    sc->tl_iflist->tl_active_phy != sc->tl_phy_addr) {
				error = EINVAL;
				break;
			}
			sc->tl_iflist->tl_active_phy = sc->tl_phy_addr;
			tl_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING) {
				sc->tl_iflist->tl_active_phy = TL_PHYS_IDLE;
				tl_stop(sc);
			}
		}
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
#if defined(__FreeBSD__) && __FreeBSD__ < 3
		if (command == SIOCADDMULTI)
			error = ether_addmulti(ifr, &sc->arpcom);
		else
			error = ether_delmulti(ifr, &sc->arpcom);
		if (error == ENETRESET) {
			tl_setmulti(sc);
			error = 0;
		}
#else
		tl_setmulti(sc);
		error = 0;
#endif
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
	struct tl_csr		*csr;
	struct tl_mii_frame	frame;

	csr = sc->csr;
	ifp = &sc->arpcom.ac_if;

	/* Stop the stats updater. */
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
	untimeout(tl_stats_update, sc, sc->tl_stat_ch);
#else
	untimeout(tl_stats_update, sc);
#endif

	/* Stop the transmitter */
	sc->csr->tl_host_cmd &= TL_CMD_RT;
	sc->csr->tl_host_cmd |= TL_CMD_STOP;

	/* Stop the receiver */
	sc->csr->tl_host_cmd |= TL_CMD_RT;
	sc->csr->tl_host_cmd |= TL_CMD_STOP;

	/*
	 * Disable host interrupts.
	 */
	sc->csr->tl_host_cmd |= TL_CMD_INTSOFF;

	/*
	 * Disable PHY interrupts.
	 */
	bzero((char *)&frame, sizeof(frame));

	frame.mii_phyaddr = sc->tl_phy_addr;
	frame.mii_regaddr = TL_PHY_CTL;
	tl_mii_readreg(csr, &frame);
	frame.mii_data |= PHY_CTL_INTEN;
	tl_mii_writereg(csr, &frame);

	/*
	 * Disable MII interrupts.
	 */
	DIO_SEL(TL_NETSIO);
	DIO_BYTE1_CLR(TL_SIO_MINTEN);

	/*
	 * Clear list pointer.
	 */
	sc->csr->tl_ch_parm = 0;

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

	sc->tl_iflist->tl_active_phy = TL_PHYS_IDLE;
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	return;
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
static void tl_shutdown(howto, xilist)
	int			howto;
	void			*xilist;
{
	struct tl_iflist	*ilist = (struct tl_iflist *)xilist;
	struct tl_csr		*csr = ilist->csr;
	struct tl_mii_frame	frame;
	int			i;

	/* Stop the transmitter */
	csr->tl_host_cmd &= TL_CMD_RT;
	csr->tl_host_cmd |= TL_CMD_STOP;

	/* Stop the receiver */
	csr->tl_host_cmd |= TL_CMD_RT;
	csr->tl_host_cmd |= TL_CMD_STOP;

	/*
	 * Disable host interrupts.
	 */
	csr->tl_host_cmd |= TL_CMD_INTSOFF;

	/*
	 * Disable PHY interrupts.
	 */
	bzero((char *)&frame, sizeof(frame));

	for (i = TL_PHYADDR_MIN; i < TL_PHYADDR_MAX + 1; i++) {
		frame.mii_phyaddr = i;
		frame.mii_regaddr = TL_PHY_CTL;
		tl_mii_readreg(csr, &frame);
		frame.mii_data |= PHY_CTL_INTEN;
		tl_mii_writereg(csr, &frame);
	};

	/*
	 * Disable MII interrupts.
	 */
	DIO_SEL(TL_NETSIO);
	DIO_BYTE1_CLR(TL_SIO_MINTEN);

	return;
}


static struct pci_device tlc_device = {
	"tlc",
	tl_probe,
	tl_attach_ctlr,
	&tl_count,
	NULL
};
DATA_SET(pcidevice_set, tlc_device);
