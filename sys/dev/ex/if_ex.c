/*
 * Copyright (c) 1996, Javier Martín Rueda (jmrueda@diatel.upm.es)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 * MAINTAINER: Matthew N. Dodd <winter@jurai.net>
 *                             <mdodd@FreeBSD.org>
 */

/*
 * Intel EtherExpress Pro/10, Pro/10+ Ethernet driver
 *
 * Revision history:
 *
 * 30-Oct-1996: first beta version. Inet and BPF supported, but no multicast.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/socket.h>

#include <sys/module.h>
#include <sys/bus.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_media.h> 
#include <net/ethernet.h>
#include <net/bpf.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>


#include <isa/isavar.h>
#include <isa/pnpvar.h>

#include <dev/ex/if_exreg.h>
#include <dev/ex/if_exvar.h>

#ifdef EXDEBUG
# define Start_End 1
# define Rcvd_Pkts 2
# define Sent_Pkts 4
# define Status    8
static int debug_mask = 0;
static int exintr_count = 0;
# define DODEBUG(level, action) if (level & debug_mask) action
#else
# define DODEBUG(level, action)
#endif

char irq2eemap[] =
	{ -1, -1, 0, 1, -1, 2, -1, -1, -1, 0, 3, 4, -1, -1, -1, -1 };
u_char ee2irqmap[] =
	{ 9, 3, 5, 10, 11, 0, 0, 0 };
                
char plus_irq2eemap[] =
	{ -1, -1, -1, 0, 1, 2, -1, 3, -1, 4, 5, 6, 7, -1, -1, -1 };
u_char plus_ee2irqmap[] =
	{ 3, 4, 5, 7, 9, 10, 11, 12 };

/* Network Interface Functions */
static void	ex_init		(void *);
static void	ex_start	(struct ifnet *);
static int	ex_ioctl	(struct ifnet *, u_long, caddr_t);
static void	ex_watchdog	(struct ifnet *);

/* ifmedia Functions	*/
static int	ex_ifmedia_upd	(struct ifnet *);
static void	ex_ifmedia_sts	(struct ifnet *, struct ifmediareq *);

static int	ex_get_media	(u_int32_t iobase);

static void	ex_reset	(struct ex_softc *);

static void	ex_tx_intr	(struct ex_softc *);
static void	ex_rx_intr	(struct ex_softc *);

int
look_for_card (u_int32_t iobase)
{
	int count1, count2;

	/*
	 * Check for the i82595 signature, and check that the round robin
	 * counter actually advances.
	 */
	if (((count1 = inb(iobase + ID_REG)) & Id_Mask) != Id_Sig)
		return(0);
	count2 = inb(iobase + ID_REG);
	count2 = inb(iobase + ID_REG);
	count2 = inb(iobase + ID_REG);

	return((count2 & Counter_bits) == ((count1 + 0xc0) & Counter_bits));
}

void
ex_get_address (u_int32_t iobase, u_char *enaddr)
{
	u_int16_t	eaddr_tmp;

	eaddr_tmp = eeprom_read(iobase, EE_Eth_Addr_Lo);
	enaddr[5] = eaddr_tmp & 0xff;
	enaddr[4] = eaddr_tmp >> 8;
	eaddr_tmp = eeprom_read(iobase, EE_Eth_Addr_Mid);
	enaddr[3] = eaddr_tmp & 0xff;
	enaddr[2] = eaddr_tmp >> 8;
	eaddr_tmp = eeprom_read(iobase, EE_Eth_Addr_Hi);
	enaddr[1] = eaddr_tmp & 0xff;
	enaddr[0] = eaddr_tmp >> 8;
	
	return;
}

int
ex_card_type (u_char *enaddr)
{
	if ((enaddr[0] == 0x00) && (enaddr[1] == 0xA0) && (enaddr[2] == 0xC9))
		return (CARD_TYPE_EX_10_PLUS);

	return (CARD_TYPE_EX_10);
}

/*
 * Caller is responsible for eventually calling
 * ex_release_resources() on failure.
 */
int
ex_alloc_resources (device_t dev)
{
	struct ex_softc *	sc = device_get_softc(dev);
	int			error = 0;

	sc->ioport = bus_alloc_resource(dev, SYS_RES_IOPORT, &sc->ioport_rid,
					0, ~0, 1, RF_ACTIVE);
	if (!sc->ioport) {
		device_printf(dev, "No I/O space?!\n");
		error = ENOMEM;
		goto bad;
	}

	sc->irq = bus_alloc_resource(dev, SYS_RES_IRQ, &sc->irq_rid,
					0, ~0, 1, RF_ACTIVE);

	if (!sc->irq) {
		device_printf(dev, "No IRQ?!\n");
		error = ENOMEM;
		goto bad;
	}

bad:
	return (error);
}

void
ex_release_resources (device_t dev)
{
	struct ex_softc *	sc = device_get_softc(dev);

	if (sc->ih) {
		bus_teardown_intr(dev, sc->irq, sc->ih);
		sc->ih = NULL;
	}

	if (sc->ioport) {
		bus_release_resource(dev, SYS_RES_IOPORT,
					sc->ioport_rid, sc->ioport);
		sc->ioport = NULL;
	}

	if (sc->irq) {
		bus_release_resource(dev, SYS_RES_IRQ,
					sc->irq_rid, sc->irq);
		sc->irq = NULL;
	}

	return;
}

int
ex_attach(device_t dev)
{
	struct ex_softc *	sc = device_get_softc(dev);
	struct ifnet *		ifp = &sc->arpcom.ac_if;
	struct ifmedia *	ifm;
	int			unit = device_get_unit(dev);
	u_int16_t		temp;

	/* work out which set of irq <-> internal tables to use */
	if (ex_card_type(sc->arpcom.ac_enaddr) == CARD_TYPE_EX_10_PLUS) {
		sc->irq2ee = plus_irq2eemap;
		sc->ee2irq = plus_ee2irqmap;
	} else {
		sc->irq2ee = irq2eemap;
		sc->ee2irq = ee2irqmap;
	}

	sc->mem_size = CARD_RAM_SIZE;	/* XXX This should be read from the card itself. */

	/*
	 * Initialize the ifnet structure.
	 */
	ifp->if_softc = sc;
	ifp->if_unit = unit;
	ifp->if_name = "ex";
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_SIMPLEX | IFF_BROADCAST /* XXX not done yet. | IFF_MULTICAST */;
	ifp->if_output = ether_output;
	ifp->if_start = ex_start;
	ifp->if_ioctl = ex_ioctl;
	ifp->if_watchdog = ex_watchdog;
	ifp->if_init = ex_init;
	ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;

	ifmedia_init(&sc->ifmedia, 0, ex_ifmedia_upd, ex_ifmedia_sts);

	temp = eeprom_read(sc->iobase, EE_W5);
	if (temp & EE_W5_PORT_TPE)
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_10_T, 0, NULL);
	if (temp & EE_W5_PORT_BNC)
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_10_2, 0, NULL);
	if (temp & EE_W5_PORT_AUI)
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_10_5, 0, NULL);

	ifmedia_set(&sc->ifmedia, ex_get_media(sc->iobase));

	ifm = &sc->ifmedia;
	ifm->ifm_media = ifm->ifm_cur->ifm_media;
	ex_ifmedia_upd(ifp);

	/*
	 * Attach the interface.
	 */
	ether_ifattach(ifp, ETHER_BPF_SUPPORTED);

	device_printf(sc->dev, "Ethernet address %6D\n",
			sc->arpcom.ac_enaddr, ":");

	return(0);
}

static void
ex_init(void *xsc)
{
	struct ex_softc *	sc = (struct ex_softc *) xsc;
	struct ifnet *		ifp = &sc->arpcom.ac_if;
	int			s;
	int			i;
	register int		iobase = sc->iobase;
	unsigned short		temp_reg;

	DODEBUG(Start_End, printf("ex_init%d: start\n", ifp->if_unit););

	if (TAILQ_FIRST(&ifp->if_addrhead) == NULL) {
		return;
	}
	s = splimp();
	ifp->if_timer = 0;

	/*
	 * Load the ethernet address into the card.
	 */
	outb(iobase + CMD_REG, Bank2_Sel);
	temp_reg = inb(iobase + EEPROM_REG);
	if (temp_reg & Trnoff_Enable) {
		outb(iobase + EEPROM_REG, temp_reg & ~Trnoff_Enable);
	}
	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		outb(iobase + I_ADDR_REG0 + i, sc->arpcom.ac_enaddr[i]);
	}
	/*
	 * - Setup transmit chaining and discard bad received frames.
	 * - Match broadcast.
	 * - Clear test mode.
	 * - Set receiving mode.
	 * - Set IRQ number.
	 */
	outb(iobase + REG1, inb(iobase + REG1) | Tx_Chn_Int_Md | Tx_Chn_ErStp | Disc_Bad_Fr);
	outb(iobase + REG2, inb(iobase + REG2) | No_SA_Ins | RX_CRC_InMem);
	outb(iobase + REG3, inb(iobase + REG3) & 0x3f /* XXX constants. */ );
	outb(iobase + CMD_REG, Bank1_Sel);
	outb(iobase + INT_NO_REG, (inb(iobase + INT_NO_REG) & 0xf8) | sc->irq2ee[sc->irq_no]);

	/*
	 * Divide the available memory in the card into rcv and xmt buffers.
	 * By default, I use the first 3/4 of the memory for the rcv buffer,
	 * and the remaining 1/4 of the memory for the xmt buffer.
	 */
	sc->rx_mem_size = sc->mem_size * 3 / 4;
	sc->tx_mem_size = sc->mem_size - sc->rx_mem_size;
	sc->rx_lower_limit = 0x0000;
	sc->rx_upper_limit = sc->rx_mem_size - 2;
	sc->tx_lower_limit = sc->rx_mem_size;
	sc->tx_upper_limit = sc->mem_size - 2;
	outb(iobase + RCV_LOWER_LIMIT_REG, sc->rx_lower_limit >> 8);
	outb(iobase + RCV_UPPER_LIMIT_REG, sc->rx_upper_limit >> 8);
	outb(iobase + XMT_LOWER_LIMIT_REG, sc->tx_lower_limit >> 8);
	outb(iobase + XMT_UPPER_LIMIT_REG, sc->tx_upper_limit >> 8);
	
	/*
	 * Enable receive and transmit interrupts, and clear any pending int.
	 */
	outb(iobase + REG1, inb(iobase + REG1) | TriST_INT);
	outb(iobase + CMD_REG, Bank0_Sel);
	outb(iobase + MASK_REG, All_Int & ~(Rx_Int | Tx_Int));
	outb(iobase + STATUS_REG, All_Int);

	/*
	 * Initialize receive and transmit ring buffers.
	 */
	outw(iobase + RCV_BAR, sc->rx_lower_limit);
	sc->rx_head = sc->rx_lower_limit;
	outw(iobase + RCV_STOP_REG, sc->rx_upper_limit | 0xfe);
	outw(iobase + XMT_BAR, sc->tx_lower_limit);
	sc->tx_head = sc->tx_tail = sc->tx_lower_limit;

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;
	DODEBUG(Status, printf("OIDLE init\n"););
	
	/*
	 * Final reset of the board, and enable operation.
	 */
	outb(iobase + CMD_REG, Sel_Reset_CMD);
	DELAY(2);
	outb(iobase + CMD_REG, Rcv_Enable_CMD);

	ex_start(ifp);
	splx(s);

	DODEBUG(Start_End, printf("ex_init%d: finish\n", ifp->if_unit););
}


static void
ex_start(struct ifnet *ifp)
{
	struct ex_softc *	sc = ifp->if_softc;
	int			iobase = sc->iobase;
	int			i, s, len, data_len, avail, dest, next;
	unsigned char		tmp16[2];
	struct mbuf *		opkt;
	struct mbuf *		m;

	DODEBUG(Start_End, printf("ex_start%d: start\n", unit););

	s = splimp();

	/*
	 * Main loop: send outgoing packets to network card until there are no
	 * more packets left, or the card cannot accept any more yet.
	 */
	while (((opkt = ifp->if_snd.ifq_head) != NULL) &&
	       !(ifp->if_flags & IFF_OACTIVE)) {

		/*
		 * Ensure there is enough free transmit buffer space for
		 * this packet, including its header. Note: the header
		 * cannot wrap around the end of the transmit buffer and
		 * must be kept together, so we allow space for twice the
		 * length of the header, just in case.
		 */

		for (len = 0, m = opkt; m != NULL; m = m->m_next) {
			len += m->m_len;
		}

		data_len = len;

		DODEBUG(Sent_Pkts, printf("1. Sending packet with %d data bytes. ", data_len););

		if (len & 1) {
			len += XMT_HEADER_LEN + 1;
		} else {
			len += XMT_HEADER_LEN;
		}

		if ((i = sc->tx_tail - sc->tx_head) >= 0) {
			avail = sc->tx_mem_size - i;
		} else {
			avail = -i;
		}

		DODEBUG(Sent_Pkts, printf("i=%d, avail=%d\n", i, avail););

		if (avail >= len + XMT_HEADER_LEN) {
			IF_DEQUEUE(&ifp->if_snd, opkt);

#ifdef EX_PSA_INTR      
			/*
			 * Disable rx and tx interrupts, to avoid corruption
			 * of the host address register by interrupt service
			 * routines.
			 * XXX Is this necessary with splimp() enabled?
			 */
			outb(iobase + MASK_REG, All_Int);
#endif

			/*
			 * Compute the start and end addresses of this
			 * frame in the tx buffer.
			 */
			dest = sc->tx_tail;
			next = dest + len;

			if (next > sc->tx_upper_limit) {
				if ((sc->tx_upper_limit + 2 - sc->tx_tail) <=
				    XMT_HEADER_LEN) {
					dest = sc->tx_lower_limit;
					next = dest + len;
				} else {
					next = sc->tx_lower_limit +
						next - sc->tx_upper_limit - 2;
				}
			}

			/*
			 * Build the packet frame in the card's ring buffer.
			 */
			DODEBUG(Sent_Pkts, printf("2. dest=%d, next=%d. ", dest, next););

			outw(iobase + HOST_ADDR_REG, dest);
			outw(iobase + IO_PORT_REG, Transmit_CMD);
			outw(iobase + IO_PORT_REG, 0);
			outw(iobase + IO_PORT_REG, next);
			outw(iobase + IO_PORT_REG, data_len);

			/*
			 * Output the packet data to the card. Ensure all
			 * transfers are 16-bit wide, even if individual
			 * mbufs have odd length.
			 */

			for (m = opkt, i = 0; m != NULL; m = m->m_next) {
				DODEBUG(Sent_Pkts, printf("[%d]", m->m_len););
				if (i) {
					tmp16[1] = *(mtod(m, caddr_t));
					outsw(iobase + IO_PORT_REG, tmp16, 1);
				}
				outsw(iobase + IO_PORT_REG,
				      mtod(m, caddr_t) + i, (m->m_len - i) / 2);

				if ((i = (m->m_len - i) & 1) != 0) {
					tmp16[0] = *(mtod(m, caddr_t) +
						   m->m_len - 1);
				}
			}
			if (i) {
				outsw(iobase + IO_PORT_REG, tmp16, 1);
			}
	
			/*
			 * If there were other frames chained, update the
			 * chain in the last one.
			 */
			if (sc->tx_head != sc->tx_tail) {
				if (sc->tx_tail != dest) {
					outw(iobase + HOST_ADDR_REG,
					     sc->tx_last + XMT_Chain_Point);
					outw(iobase + IO_PORT_REG, dest);
				}
				outw(iobase + HOST_ADDR_REG,
				     sc->tx_last + XMT_Byte_Count);
				i = inw(iobase + IO_PORT_REG);
				outw(iobase + HOST_ADDR_REG,
				     sc->tx_last + XMT_Byte_Count);
				outw(iobase + IO_PORT_REG, i | Ch_bit);
			}
	
			/*
			 * Resume normal operation of the card:
			 * - Make a dummy read to flush the DRAM write
			 *   pipeline.
			 * - Enable receive and transmit interrupts.
			 * - Send Transmit or Resume_XMT command, as
			 *   appropriate.
			 */
			inw(iobase + IO_PORT_REG);
#ifdef EX_PSA_INTR
			outb(iobase + MASK_REG, All_Int & ~(Rx_Int | Tx_Int));
#endif
			if (sc->tx_head == sc->tx_tail) {
				outw(iobase + XMT_BAR, dest);
				outb(iobase + CMD_REG, Transmit_CMD);
				sc->tx_head = dest;
				DODEBUG(Sent_Pkts, printf("Transmit\n"););
			} else {
				outb(iobase + CMD_REG, Resume_XMT_List_CMD);
				DODEBUG(Sent_Pkts, printf("Resume\n"););
			}
	
			sc->tx_last = dest;
			sc->tx_tail = next;
     	 
			if (ifp->if_bpf != NULL) {
				bpf_mtap(ifp, opkt);
			}

			ifp->if_timer = 2;
			ifp->if_opackets++;
			m_freem(opkt);
		} else {
			ifp->if_flags |= IFF_OACTIVE;
			DODEBUG(Status, printf("OACTIVE start\n"););
		}
	}

	splx(s);

	DODEBUG(Start_End, printf("ex_start%d: finish\n", unit););
}

void
ex_stop(struct ex_softc *sc)
{
	int iobase = sc->iobase;

	DODEBUG(Start_End, printf("ex_stop%d: start\n", unit););

	/*
	 * Disable card operation:
	 * - Disable the interrupt line.
	 * - Flush transmission and disable reception.
	 * - Mask and clear all interrupts.
	 * - Reset the 82595.
	 */
	outb(iobase + CMD_REG, Bank1_Sel);
	outb(iobase + REG1, inb(iobase + REG1) & ~TriST_INT);
	outb(iobase + CMD_REG, Bank0_Sel);
	outb(iobase + CMD_REG, Rcv_Stop);
	sc->tx_head = sc->tx_tail = sc->tx_lower_limit;
	sc->tx_last = 0; /* XXX I think these two lines are not necessary, because ex_init will always be called again to reinit the interface. */
	outb(iobase + MASK_REG, All_Int);
	outb(iobase + STATUS_REG, All_Int);
	outb(iobase + CMD_REG, Reset_CMD);
	DELAY(200);

	DODEBUG(Start_End, printf("ex_stop%d: finish\n", unit););

	return;
}

void
ex_intr(void *arg)
{
	struct ex_softc *	sc = (struct ex_softc *)arg;
	struct ifnet *	ifp = &sc->arpcom.ac_if;
	int			iobase = sc->iobase;
	int			int_status, send_pkts;

	DODEBUG(Start_End, printf("ex_intr%d: start\n", unit););

#ifdef EXDEBUG
	if (++exintr_count != 1)
		printf("WARNING: nested interrupt (%d). Mail the author.\n", exintr_count);
#endif

	send_pkts = 0;
	while ((int_status = inb(iobase + STATUS_REG)) & (Tx_Int | Rx_Int)) {
		if (int_status & Rx_Int) {
			outb(iobase + STATUS_REG, Rx_Int);

			ex_rx_intr(sc);
		} else if (int_status & Tx_Int) {
			outb(iobase + STATUS_REG, Tx_Int);

			ex_tx_intr(sc);
			send_pkts = 1;
		}
	}

	/*
	 * If any packet has been transmitted, and there are queued packets to
	 * be sent, attempt to send more packets to the network card.
	 */

	if (send_pkts && (ifp->if_snd.ifq_head != NULL)) {
		ex_start(ifp);
	}

#ifdef EXDEBUG
	exintr_count--;
#endif

	DODEBUG(Start_End, printf("ex_intr%d: finish\n", unit););

	return;
}

static void
ex_tx_intr(struct ex_softc *sc)
{
	struct ifnet *	ifp = &sc->arpcom.ac_if;
	int		iobase = sc->iobase;
	int		tx_status;

	DODEBUG(Start_End, printf("ex_tx_intr%d: start\n", unit););

	/*
	 * - Cancel the watchdog.
	 * For all packets transmitted since last transmit interrupt:
	 * - Advance chain pointer to next queued packet.
	 * - Update statistics.
	 */

	ifp->if_timer = 0;

	while (sc->tx_head != sc->tx_tail) {
		outw(iobase + HOST_ADDR_REG, sc->tx_head);

		if (! inw(iobase + IO_PORT_REG) & Done_bit)
			break;

		tx_status = inw(iobase + IO_PORT_REG);
		sc->tx_head = inw(iobase + IO_PORT_REG);

		if (tx_status & TX_OK_bit) {
			ifp->if_opackets++;
		} else {
			ifp->if_oerrors++;
		}

		ifp->if_collisions += tx_status & No_Collisions_bits;
	}

	/*
	 * The card should be ready to accept more packets now.
	 */

	ifp->if_flags &= ~IFF_OACTIVE;

	DODEBUG(Status, printf("OIDLE tx_intr\n"););
	DODEBUG(Start_End, printf("ex_tx_intr%d: finish\n", unit););

	return;
}

static void
ex_rx_intr(struct ex_softc *sc)
{
	struct ifnet *		ifp = &sc->arpcom.ac_if;
	int			iobase = sc->iobase;
	int			rx_status;
	int			pkt_len;
	int			QQQ;
	struct mbuf *		m;
	struct mbuf *		ipkt;
	struct ether_header *	eh;

	DODEBUG(Start_End, printf("ex_rx_intr%d: start\n", unit););

	/*
	 * For all packets received since last receive interrupt:
	 * - If packet ok, read it into a new mbuf and queue it to interface,
	 *   updating statistics.
	 * - If packet bad, just discard it, and update statistics.
	 * Finally, advance receive stop limit in card's memory to new location.
	 */

	outw(iobase + HOST_ADDR_REG, sc->rx_head);

	while (inw(iobase + IO_PORT_REG) == RCV_Done) {

		rx_status = inw(iobase + IO_PORT_REG);
		sc->rx_head = inw(iobase + IO_PORT_REG);
		QQQ = pkt_len = inw(iobase + IO_PORT_REG);

		if (rx_status & RCV_OK_bit) {
			MGETHDR(m, M_DONTWAIT, MT_DATA);
			ipkt = m;
			if (ipkt == NULL) {
				ifp->if_iqdrops++;
			} else {
				ipkt->m_pkthdr.rcvif = ifp;
				ipkt->m_pkthdr.len = pkt_len;
				ipkt->m_len = MHLEN;

				while (pkt_len > 0) {
					if (pkt_len > MINCLSIZE) {
						MCLGET(m, M_DONTWAIT);
						if (m->m_flags & M_EXT) {
							m->m_len = MCLBYTES;
						} else {
							m_freem(ipkt);
							ifp->if_iqdrops++;
							goto rx_another;
						}
					}
					m->m_len = min(m->m_len, pkt_len);

	  /*
	   * NOTE: I'm assuming that all mbufs allocated are of even length,
	   * except for the last one in an odd-length packet.
	   */

					insw(iobase + IO_PORT_REG,
					     mtod(m, caddr_t), m->m_len / 2);

					if (m->m_len & 1) {
						*(mtod(m, caddr_t) + m->m_len - 1) = inb(iobase + IO_PORT_REG);
					}
					pkt_len -= m->m_len;

					if (pkt_len > 0) {
						MGET(m->m_next, M_DONTWAIT, MT_DATA);
						if (m->m_next == NULL) {
							m_freem(ipkt);
							ifp->if_iqdrops++;
							goto rx_another;
						}
						m = m->m_next;
						m->m_len = MLEN;
					}
				}
				eh = mtod(ipkt, struct ether_header *);
#ifdef EXDEBUG
	if (debug_mask & Rcvd_Pkts) {
		if ((eh->ether_dhost[5] != 0xff) || (eh->ether_dhost[0] != 0xff)) {
			printf("Receive packet with %d data bytes: %6D -> ", QQQ, eh->ether_shost, ":");
			printf("%6D\n", eh->ether_dhost, ":");
		} /* QQQ */
	}
#endif
				m_adj(ipkt, sizeof(struct ether_header));
				ether_input(ifp, eh, ipkt);
				ifp->if_ipackets++;
			}
		} else {
			ifp->if_ierrors++;
		}
		outw(iobase + HOST_ADDR_REG, sc->rx_head);
rx_another: ;
	}

	if (sc->rx_head < sc->rx_lower_limit + 2)
		outw(iobase + RCV_STOP_REG, sc->rx_upper_limit);
	else
		outw(iobase + RCV_STOP_REG, sc->rx_head - 2);

	DODEBUG(Start_End, printf("ex_rx_intr%d: finish\n", unit););

	return;
}


static int
ex_ioctl(register struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct ex_softc *	sc = ifp->if_softc;
	struct ifreq *		ifr = (struct ifreq *)data;
	int			s;
	int			error = 0;

	DODEBUG(Start_End, printf("ex_ioctl%d: start ", ifp->if_unit););

	s = splimp();

	switch(cmd) {
		case SIOCSIFADDR:
		case SIOCGIFADDR:
		case SIOCSIFMTU:
			error = ether_ioctl(ifp, cmd, data);
			break;

		case SIOCSIFFLAGS:
			DODEBUG(Start_End, printf("SIOCSIFFLAGS"););
			if ((ifp->if_flags & IFF_UP) == 0 &&
			    (ifp->if_flags & IFF_RUNNING)) {

				ifp->if_flags &= ~IFF_RUNNING;
				ex_stop(sc);
			} else {
      				ex_init(sc);
			}
			break;
#ifdef NODEF
		case SIOCGHWADDR:
			DODEBUG(Start_End, printf("SIOCGHWADDR"););
			bcopy((caddr_t)sc->sc_addr, (caddr_t)&ifr->ifr_data,
			      sizeof(sc->sc_addr));
			break;
#endif
		case SIOCADDMULTI:
			DODEBUG(Start_End, printf("SIOCADDMULTI"););
		case SIOCDELMULTI:
			DODEBUG(Start_End, printf("SIOCDELMULTI"););
			/* XXX Support not done yet. */
			error = EINVAL;
			break;
		case SIOCSIFMEDIA:
		case SIOCGIFMEDIA:
			error = ifmedia_ioctl(ifp, ifr, &sc->ifmedia, cmd);
			break;
		default:
			DODEBUG(Start_End, printf("unknown"););
			error = EINVAL;
	}

	splx(s);

	DODEBUG(Start_End, printf("\nex_ioctl%d: finish\n", ifp->if_unit););

	return(error);
}


static void
ex_reset(struct ex_softc *sc)
{
	int s;

	DODEBUG(Start_End, printf("ex_reset%d: start\n", unit););
  
	s = splimp();

	ex_stop(sc);
	ex_init(sc);

	splx(s);

	DODEBUG(Start_End, printf("ex_reset%d: finish\n", unit););

	return;
}

static void
ex_watchdog(struct ifnet *ifp)
{
	struct ex_softc *	sc = ifp->if_softc;

	DODEBUG(Start_End, printf("ex_watchdog%d: start\n", ifp->if_unit););

	ifp->if_flags &= ~IFF_OACTIVE;

	DODEBUG(Status, printf("OIDLE watchdog\n"););

	ifp->if_oerrors++;
	ex_reset(sc);
	ex_start(ifp);

	DODEBUG(Start_End, printf("ex_watchdog%d: finish\n", ifp->if_unit););

	return;
}

static int
ex_get_media (u_int32_t iobase)
{
	int	tmp;

	outb(iobase + CMD_REG, Bank2_Sel);
	tmp = inb(iobase + REG3);
	outb(iobase + CMD_REG, Bank0_Sel);

	if (tmp & TPE_bit)
		return(IFM_ETHER|IFM_10_T);
	if (tmp & BNC_bit)
		return(IFM_ETHER|IFM_10_2);

	return (IFM_ETHER|IFM_10_5);
}

static int
ex_ifmedia_upd (ifp)
	struct ifnet *		ifp;
{

	return (0);
}

static void
ex_ifmedia_sts(ifp, ifmr)
	struct ifnet *          ifp;
	struct ifmediareq *     ifmr;
{
	struct ex_softc *       sc = ifp->if_softc;

	ifmr->ifm_active = ex_get_media(sc->iobase);

	return;
}

u_short
eeprom_read(u_int32_t iobase, int location)
{
	int i;
	u_short data = 0;
	int ee_addr;
	int read_cmd = location | EE_READ_CMD;
	short ctrl_val = EECS;

	ee_addr = iobase + EEPROM_REG;
	outb(iobase + CMD_REG, Bank2_Sel);
	outb(ee_addr, EECS);
	for (i = 8; i >= 0; i--) {
		short outval = (read_cmd & (1 << i)) ? ctrl_val | EEDI : ctrl_val;
		outb(ee_addr, outval);
		outb(ee_addr, outval | EESK);
		DELAY(3);
		outb(ee_addr, outval);
		DELAY(2);
	}
	outb(ee_addr, ctrl_val);

	for (i = 16; i > 0; i--) {
		outb(ee_addr, ctrl_val | EESK);
		DELAY(3);
		data = (data << 1) | ((inb(ee_addr) & EEDO) ? 1 : 0);
		outb(ee_addr, ctrl_val);
		DELAY(2);
	}

	ctrl_val &= ~EECS;
	outb(ee_addr, ctrl_val | EESK);
	DELAY(3);
	outb(ee_addr, ctrl_val);
	DELAY(2);
	outb(iobase + CMD_REG, Bank0_Sel);
	return(data);
}
