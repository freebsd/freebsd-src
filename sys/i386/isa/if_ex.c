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
 *	$Id: if_ex.c,v 1.10 1998/01/08 23:40:52 eivind Exp $
 */

/*
 * Intel EtherExpress Pro/10, Pro/10+ Ethernet driver
 *
 * Revision history:
 *
 * 30-Oct-1996: first beta version. Inet and BPF supported, but no multicast.
 */

#include "ex.h"
#if NEX > 0
#include "bpfilter.h"
#include "opt_inet.h"
#include "opt_ipx.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/socket.h>

#include <net/if.h>

#ifdef INET
#include <netinet/in.h>
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
#endif

#include <machine/clock.h>

#include <i386/isa/isa_device.h>
#include <i386/isa/if_exreg.h>

#ifdef EXDEBUG
#define Start_End 1
#define Rcvd_Pkts 2
#define Sent_Pkts 4
#define Status    8
static int debug_mask = 0;
static int exintr_count = 0;
#define DODEBUG(level, action) if (level & debug_mask) action
#else
#define DODEBUG(level, action)
#endif

#define Conn_BNC 1
#define Conn_TPE 2
#define Conn_AUI 3

struct ex_softc {
  	struct arpcom arpcom;	/* Ethernet common data */
	u_int iobase;	/* I/O base address. */
	u_short connector;	/* Connector type. */
	u_short irq_no; /* IRQ number. */
	char *irq2ee; /* irq <-> internal representation conversion */
	u_char *ee2irq;
	u_int mem_size;	/* Total memory size, in bytes. */
	u_int rx_mem_size;	/* Rx memory size (by default, first 3/4 of total memory). */
  u_int rx_lower_limit, rx_upper_limit; /* Lower and upper limits of receive buffer. */
  u_int rx_head; /* Head of receive ring buffer. */
	u_int tx_mem_size;	/* Tx memory size (by default, last quarter of total memory). */
  u_int tx_lower_limit, tx_upper_limit; /* Lower and upper limits of transmit buffer. */
  u_int tx_head, tx_tail; /* Head and tail of transmit ring buffer. */
  u_int tx_last; /* Pointer to beginning of last frame in the chain. */
};

static struct ex_softc ex_sc[NEX]; /* XXX would it be better to malloc(3) the memory? */

static char irq2eemap[] = { -1, -1, 0, 1, -1, 2, -1, -1, -1, 0, 3, 4, -1, -1, -1, -1 };
static u_char ee2irqmap[] = { 9, 3, 5, 10, 11, 0, 0, 0 };
static char plus_irq2eemap[] = { -1, -1, -1, 0, 1, 2, -1, 3, -1, 4, 5, 6, 7, -1, -1, -1 };
static u_char plus_ee2irqmap[] = { 3, 4, 5, 7, 9, 10, 11, 12 };

static int ex_probe __P((struct isa_device *));
static int ex_attach __P((struct isa_device *));
static void ex_init __P((void *));
static void ex_start __P((struct ifnet *));
static void ex_stop __P((int));
static int ex_ioctl __P((struct ifnet *, int, caddr_t));
static void ex_reset __P((int));
static void ex_watchdog __P((struct ifnet *));

static u_short eeprom_read __P((int, int));
static int look_for_card __P((u_int));
static void ex_tx_intr __P((int));
static void ex_rx_intr __P((int));

struct isa_driver exdriver = {
	ex_probe,
	ex_attach,
	"ex",
	0
};

static int look_for_card(u_int iobase)
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


int ex_probe(struct isa_device *dev)
{
	int unit = dev->id_unit;
	struct ex_softc *sc = &ex_sc[unit];
	u_int iobase;
	u_short eaddr_tmp;
	int tmp;

	DODEBUG(Start_End, printf("ex_probe%d: start\n", unit););

	/*
	 * If an I/O address was supplied in the configuration file, probe only
	 * that. Otherwise, cycle through the predefined set of possible addresses.
	 */
	if (dev->id_iobase != -1) {
		if (! look_for_card(iobase = dev->id_iobase))
			return(0);
	}
	else {
		for (iobase = 0x200; iobase < 0x3a0; iobase += 0x10)
			if (look_for_card(iobase))
				break;
		if (iobase >= 0x3a0)
			return(0);
		else
			dev->id_iobase = iobase;
	}

	/*
	 * Reset the card.
	 */
	outb(iobase + CMD_REG, Reset_CMD);
	DELAY(400);

	/*
	 * Fill in several fields of the softc structure:
	 *	- I/O base address.
	 *	- Hardware Ethernet address.
	 *	- IRQ number (if not supplied in config file, read it from EEPROM).
	 *	- Connector type.
	 */
	sc->iobase = iobase;
	eaddr_tmp = eeprom_read(iobase, EE_Eth_Addr_Lo);
	sc->arpcom.ac_enaddr[5] = eaddr_tmp & 0xff;
	sc->arpcom.ac_enaddr[4] = eaddr_tmp >> 8;
	eaddr_tmp = eeprom_read(iobase, EE_Eth_Addr_Mid);
	sc->arpcom.ac_enaddr[3] = eaddr_tmp & 0xff;
	sc->arpcom.ac_enaddr[2] = eaddr_tmp >> 8;
	eaddr_tmp = eeprom_read(iobase, EE_Eth_Addr_Hi);
	sc->arpcom.ac_enaddr[1] = eaddr_tmp & 0xff;
	sc->arpcom.ac_enaddr[0] = eaddr_tmp >> 8;
	tmp = eeprom_read(iobase, EE_IRQ_No) & IRQ_No_Mask;

	/* work out which set of irq <-> internal tables to use */
	if (sc->arpcom.ac_enaddr[0] == 0x00 &&
	    sc->arpcom.ac_enaddr[1] == 0xA0 &&
	    sc->arpcom.ac_enaddr[2] == 0xC9) {    /* it's a 10+ */
		sc->irq2ee = plus_irq2eemap;
		sc->ee2irq = plus_ee2irqmap;
	} else {                                  /* it's an ordinary 10 */
		sc->irq2ee = irq2eemap;
		sc->ee2irq = ee2irqmap;
	}

	if (dev->id_irq > 0) {
		if (sc->ee2irq[tmp] != ffs(dev->id_irq) - 1)
			printf("ex%d: WARNING: board's EEPROM is configured for IRQ %d, using %d\n", unit, sc->ee2irq[tmp], ffs(dev->id_irq) - 1);
		sc->irq_no = ffs(dev->id_irq) - 1;
	}
	else {
		sc->irq_no = sc->ee2irq[tmp];
		dev->id_irq = 1 << sc->irq_no;
	}
	if (sc->irq_no == 0) {
		printf("ex%d: invalid IRQ.\n", unit);
		return(0);
	}
	outb(iobase + CMD_REG, Bank2_Sel);
	tmp = inb(iobase + REG3);
	if (tmp & TPE_bit)
		sc->connector = Conn_TPE;
	else if (tmp & BNC_bit)
		sc->connector = Conn_BNC;
	else
		sc->connector = Conn_AUI;
	sc->mem_size = CARD_RAM_SIZE;	/* XXX This should be read from the card itself. */

	outb(iobase + CMD_REG, Bank0_Sel);

	DODEBUG(Start_End, printf("ex_probe%d: finish\n", unit););
	return(EX_IOSIZE);
}


int ex_attach(struct isa_device *dev)
{
	int unit = dev->id_unit;
	struct ex_softc *sc = &ex_sc[unit];
	struct ifnet *ifp = &sc->arpcom.ac_if;

	DODEBUG(Start_End, printf("ex_attach%d: start\n", unit););

	/*
	 * Initialize the ifnet structure.
	 */
	ifp->if_softc = sc;
	ifp->if_unit = unit;
	ifp->if_name = "ex";
	ifp->if_init = ex_init;
	ifp->if_output = ether_output;
	ifp->if_start = ex_start;
	ifp->if_ioctl = ex_ioctl;
	ifp->if_watchdog = ex_watchdog;
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_SIMPLEX | IFF_BROADCAST /* XXX not done yet. | IFF_MULTICAST */;

	/*
	 * Attach the interface.
	 */
	if_attach(ifp);
	ether_ifattach(ifp);

	if (sc->arpcom.ac_enaddr[0] == 0x00 &&
	    sc->arpcom.ac_enaddr[1] == 0xA0 &&
	    sc->arpcom.ac_enaddr[2] == 0xC9) {
		printf("ex%d: Intel EtherExpress Pro/10+, address %6D, connector ", dev->id_unit, sc->arpcom.ac_enaddr, ":");
	} else {
		printf("ex%d: Intel EtherExpress Pro/10, address %6D, connector ", dev->id_unit, sc->arpcom.ac_enaddr, ":");
	}
	switch(sc->connector) {
		case Conn_TPE: printf("TPE\n"); break;
		case Conn_BNC: printf("BNC\n"); break;
		case Conn_AUI: printf("AUI\n"); break;
		default: printf("???\n");
	}

	/*
	 * If BPF is in the kernel, call the attach for it
	 */
#if NBPFILTER > 0
	bpfattach(ifp, DLT_EN10MB, sizeof(struct ether_header));
#endif
	DODEBUG(Start_End, printf("ex_attach%d: finish\n", unit););
	return(1);
}


void ex_init(void *xsc)
{
	register struct ex_softc *sc = (struct ex_softc *) xsc;
	struct ifnet *ifp = &sc->arpcom.ac_if;
	int s, i;
	register int iobase = sc->iobase;
	unsigned short temp_reg;

	DODEBUG(Start_End, printf("ex_init%d: start\n", ifp->if_unit););

	if (ifp->if_addrhead.tqh_first == NULL)
	  return;
	s = splimp();
	sc->arpcom.ac_if.if_timer = 0;

	/*
	 * Load the ethernet address into the card.
	 */
	outb(iobase + CMD_REG, Bank2_Sel);
	temp_reg = inb(iobase + EEPROM_REG);
	if (temp_reg & Trnoff_Enable)
	  outb(iobase + EEPROM_REG, temp_reg & ~Trnoff_Enable);
	for (i = 0; i < ETHER_ADDR_LEN; i++)
	  outb(iobase + I_ADDR_REG0 + i, sc->arpcom.ac_enaddr[i]);
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


void ex_start(struct ifnet *ifp)
{
  int unit = ifp->if_unit;
  register struct ex_softc *sc = &ex_sc[unit];
  register int iobase = sc->iobase;
  int i, s, len, data_len, avail, dest, next;
  unsigned char tmp16[2];
  struct mbuf *opkt;
  register struct mbuf *m;

  DODEBUG(Start_End, printf("ex_start%d: start\n", unit););

  s = splimp();

  /*
   * Main loop: send outgoing packets to network card until there are no
   * more packets left, or the card cannot accept any more yet.
   */
  while (((opkt = ifp->if_snd.ifq_head) != NULL) && ! (ifp->if_flags & IFF_OACTIVE)) {

    /*
     * Ensure there is enough free transmit buffer space for this packet,
     * including its header. Note: the header cannot wrap around the end of
     * the transmit buffer and must be kept together, so we allow space for
     * twice the length of the header, just in case.
     */
    for (len = 0, m = opkt; m != NULL; m = m->m_next)
      len += m->m_len;
    data_len = len;
    DODEBUG(Sent_Pkts, printf("1. Sending packet with %d data bytes. ", data_len););
    if (len & 1)
      len += XMT_HEADER_LEN + 1;
    else
      len += XMT_HEADER_LEN;
    if ((i = sc->tx_tail - sc->tx_head) >= 0)
      avail = sc->tx_mem_size - i;
    else
      avail = -i;
	DODEBUG(Sent_Pkts, printf("i=%d, avail=%d\n", i, avail););
    if (avail >= len + XMT_HEADER_LEN) {
      IF_DEQUEUE(&ifp->if_snd, opkt);

#ifdef EX_PSA_INTR      
      /*
       * Disable rx and tx interrupts, to avoid corruption of the host
       * address register by interrupt service routines. XXX Is this necessary with splimp() enabled?
       */
      outb(iobase + MASK_REG, All_Int);
#endif

      /*
       * Compute the start and end addresses of this frame in the tx buffer.
       */
      dest = sc->tx_tail;
      next = dest + len;
      if (next > sc->tx_upper_limit) {
	if ((sc->tx_upper_limit + 2 - sc->tx_tail) <= XMT_HEADER_LEN) {
	  dest = sc->tx_lower_limit;
	  next = dest + len;
	}
	else
	  next = sc->tx_lower_limit + next - sc->tx_upper_limit - 2;
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
	 * Output the packet data to the card. Ensure all transfers are
	 * 16-bit wide, even if individual mbufs have odd length.
	 */

     for (m = opkt, i = 0; m != NULL; m = m->m_next) {
		DODEBUG(Sent_Pkts, printf("[%d]", m->m_len););
		if (i) {
			tmp16[1] = *(mtod(m, caddr_t));
			outsw(iobase + IO_PORT_REG, tmp16, 1);
		}
		outsw(iobase + IO_PORT_REG, mtod(m, caddr_t) + i, (m->m_len - i) / 2);
		if (i = (m->m_len - i) & 1)
			tmp16[0] = *(mtod(m, caddr_t) + m->m_len - 1);
	}
	if (i)
		outsw(iobase + IO_PORT_REG, tmp16, 1);

      /*
       * If there were other frames chained, update the chain in the last one.
       */
      if (sc->tx_head != sc->tx_tail) {
	if (sc->tx_tail != dest) {
	  outw(iobase + HOST_ADDR_REG, sc->tx_last + XMT_Chain_Point);
	  outw(iobase + IO_PORT_REG, dest);
	}
	outw(iobase + HOST_ADDR_REG, sc->tx_last + XMT_Byte_Count);
	i = inw(iobase + IO_PORT_REG);
	outw(iobase + HOST_ADDR_REG, sc->tx_last + XMT_Byte_Count);
	outw(iobase + IO_PORT_REG, i | Ch_bit);
      }

      /*
       * Resume normal operation of the card:
       * - Make a dummy read to flush the DRAM write pipeline.
       * - Enable receive and transmit interrupts.
       * - Send Transmit or Resume_XMT command, as appropriate.
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
      }
      else {
	outb(iobase + CMD_REG, Resume_XMT_List_CMD);
	DODEBUG(Sent_Pkts, printf("Resume\n"););
	}
      sc->tx_last = dest;
      sc->tx_tail = next;
      
#if NBPFILTER > 0
      if (ifp->if_bpf != NULL)
	bpf_mtap(ifp, opkt);
#endif
      ifp->if_timer = 2;
      ifp->if_opackets++;
      m_freem(opkt);
    }
    else {
      ifp->if_flags |= IFF_OACTIVE;
	  DODEBUG(Status, printf("OACTIVE start\n"););
	}
  }

  splx(s);

  DODEBUG(Start_End, printf("ex_start%d: finish\n", unit););
}


void ex_stop(int unit)
{
  struct ex_softc *sc = &ex_sc[unit];
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
}


void exintr(int unit)
{
  struct ex_softc *sc = &ex_sc[unit];
  struct ifnet *ifp = &sc->arpcom.ac_if;
  int iobase = sc->iobase;
  int int_status, send_pkts;

  DODEBUG(Start_End, printf("exintr%d: start\n", unit););

#ifdef EXDEBUG
	if (++exintr_count != 1)
		printf("WARNING: nested interrupt (%d). Mail the author.\n", exintr_count);
#endif

  send_pkts = 0;
  while ((int_status = inb(iobase + STATUS_REG)) & (Tx_Int | Rx_Int)) {
    if (int_status & Rx_Int) {
      outb(iobase + STATUS_REG, Rx_Int);
      ex_rx_intr(unit);
    }
	else if (int_status & Tx_Int) {
      outb(iobase + STATUS_REG, Tx_Int);
      ex_tx_intr(unit);
      send_pkts = 1;
    }
  }

  /*
   * If any packet has been transmitted, and there are queued packets to
   * be sent, attempt to send more packets to the network card.
   */

  if (send_pkts && (ifp->if_snd.ifq_head != NULL))
    ex_start(ifp);

#ifdef EXDEBUG
	exintr_count--;
#endif

  DODEBUG(Start_End, printf("exintr%d: finish\n", unit););
}


void ex_tx_intr(int unit)
{
  register struct ex_softc *sc = &ex_sc[unit];
  register struct ifnet *ifp = &sc->arpcom.ac_if;
  register int iobase = sc->iobase;
  int tx_status;

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
    if (tx_status & TX_OK_bit)
      ifp->if_opackets++;
    else
      ifp->if_oerrors++;
    ifp->if_collisions += tx_status & No_Collisions_bits;
  }

  /*
   * The card should be ready to accept more packets now.
   */

  ifp->if_flags &= ~IFF_OACTIVE;
  DODEBUG(Status, printf("OIDLE tx_intr\n"););

  DODEBUG(Start_End, printf("ex_tx_intr%d: finish\n", unit););
}


void ex_rx_intr(int unit)
{
  register struct ex_softc *sc = &ex_sc[unit];
  register struct ifnet *ifp = &sc->arpcom.ac_if;
  register int iobase = sc->iobase;
  int rx_status, pkt_len, QQQ;
  register struct mbuf *m, *ipkt;
  struct ether_header *eh;

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
      if (ipkt == NULL)
	ifp->if_iqdrops++;
      else {
	ipkt->m_pkthdr.rcvif = ifp;
	ipkt->m_pkthdr.len = pkt_len;
	ipkt->m_len = MHLEN;
	while (pkt_len > 0) {
	  if (pkt_len > MINCLSIZE) {
	    MCLGET(m, M_DONTWAIT);
	    if (m->m_flags & M_EXT)
	      m->m_len = MCLBYTES;
	    else {
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
	  insw(iobase + IO_PORT_REG, mtod(m, caddr_t), m->m_len / 2);
	  if (m->m_len & 1)
		*(mtod(m, caddr_t) + m->m_len - 1) = inb(iobase + IO_PORT_REG);
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
#if NBPFILTER > 0
	if (ifp->if_bpf != NULL) {
		bpf_mtap(ifp, ipkt);

		/*
		 * Note that the interface cannot be in promiscuous mode if there are
		 * no BPF listeners. And if we are in promiscuous mode, we have to
		 * check if this packet is really ours.
		 */
		if ((ifp->if_flags & IFF_PROMISC) &&
		    (eh->ether_dhost[0] & 1) == 0 &&
		    bcmp(eh->ether_dhost, sc->arpcom.ac_enaddr, sizeof(eh->ether_dhost)) != 0 &&
		    bcmp(eh->ether_dhost, etherbroadcastaddr, sizeof(eh->ether_dhost)) != 0) {
			m_freem(ipkt);
			goto rx_another;
		}
	}
#endif
	m_adj(ipkt, sizeof(struct ether_header));
	ether_input(ifp, eh, ipkt);
	ifp->if_ipackets++;
      }
    }
    else
      ifp->if_ierrors++;
    outw(iobase + HOST_ADDR_REG, sc->rx_head);
  rx_another: ;
  }
  if (sc->rx_head < sc->rx_lower_limit + 2)
    outw(iobase + RCV_STOP_REG, sc->rx_upper_limit);
  else
    outw(iobase + RCV_STOP_REG, sc->rx_head - 2);

  DODEBUG(Start_End, printf("ex_rx_intr%d: finish\n", unit););
}


int ex_ioctl(register struct ifnet *ifp, int cmd, caddr_t data)
{
  register struct ifaddr *ifa = (struct ifaddr *) data;
  struct ex_softc *sc = &ex_sc[ifp->if_unit];
  struct ifreq *ifr = (struct ifreq *) data;
  int s, error = 0;

  DODEBUG(Start_End, printf("ex_ioctl%d: start ", ifp->if_unit););

  s = splimp();

  switch(cmd) {
  case SIOCSIFADDR:
    DODEBUG(Start_End, printf("SIOCSIFADDR"););
    ifp->if_flags |= IFF_UP;
    
    switch(ifa->ifa_addr->sa_family) {
#ifdef INET
    case AF_INET:
      ex_init(sc);
      arp_ifinit((struct arpcom *) ifp, ifa);
      break;
#endif
#ifdef IPX_NOTYET
    case AF_IPX:
      {
	register struct ipx_addr *ina = &(IA_SIPX(ifa)->sipx_addr);

	if (ipx_nullhost(*ina))
	  ina->x_host = *(union ipx_host *) (sc->arpcom.ac_enaddr);
	else {
	  ifp->if_flags &= ~IFF_RUNNING;
	  bcopy((caddr_t) ina->x_host.c_host, (caddr_t) sc->arpcom.ac_enaddr, sizeof(sc->arpcom.ac_enaddr));
	}
	ex_init(sc);
	break;
      }
#endif
#ifdef NS
    case AF_NS:
      {
	register struct ns_addr *ina = &(IA_SNS(ifa)->sns_addr);

	if (ns_nullhost(*ina))
	  ina->x_host = *(union ns_host *) (sc->arpcom.ac_enaddr);
	else {
	  ifp->if_flags &= ~IFF_RUNNING;
	  bcopy((caddr_t) ina->x_host.c_host, (caddr_t) sc->arpcom.ac_enaddr, sizeof(sc->arpcom.ac_enaddr));
	}
	ex_init(sc);
	break;
      }
#endif
    default:
      ex_init(sc);
      break;
    }
    break;
  case SIOCGIFADDR:
    {
      struct sockaddr *sa;

      DODEBUG(Start_End, printf("SIOCGIFADDR"););
      sa = (struct sockaddr *) &ifr->ifr_data;
      bcopy((caddr_t) sc->arpcom.ac_enaddr, (caddr_t) sa->sa_data, ETHER_ADDR_LEN);
    }
  break;
  case SIOCSIFFLAGS:
    DODEBUG(Start_End, printf("SIOCSIFFLAGS"););
    if ((ifp->if_flags & IFF_UP) == 0 && ifp->if_flags & IFF_RUNNING) {
      ifp->if_flags &= ~IFF_RUNNING;
      ex_stop(ifp->if_unit);
    }
    else
      ex_init(sc);
    break;
#ifdef NODEF
  case SIOCGHWADDR:
    DODEBUG(Start_End, printf("SIOCGHWADDR"););
    bcopy((caddr_t) sc->sc_addr, (caddr_t) &ifr->ifr_data, sizeof(sc->sc_addr));
    break;
#endif
  case SIOCSIFMTU:
    DODEBUG(Start_End, printf("SIOCSIFMTU"););
    if (ifr->ifr_mtu > ETHERMTU)
      error = EINVAL;
    else
      ifp->if_mtu = ifr->ifr_mtu;
    break;
  case SIOCADDMULTI:
    DODEBUG(Start_End, printf("SIOCADDMULTI"););
  case SIOCDELMULTI:
    DODEBUG(Start_End, printf("SIOCDELMULTI"););
    /* XXX Support not done yet. */
    error = EINVAL;
    break;
  default:
    DODEBUG(Start_End, printf("unknown"););
    error = EINVAL;
  }

  splx(s);

  DODEBUG(Start_End, printf("\nex_ioctl%d: finish\n", ifp->if_unit););
  return(error);
}


void ex_reset(int unit)
{
  struct ex_softc *sc = &ex_sc[unit];
  int s;

  DODEBUG(Start_End, printf("ex_reset%d: start\n", unit););
  
  s = splimp();

  ex_stop(unit);
  ex_init(sc);

  splx(s);

  DODEBUG(Start_End, printf("ex_reset%d: finish\n", unit););
}


void ex_watchdog(struct ifnet *ifp)
{

  DODEBUG(Start_End, printf("ex_watchdog%d: start\n", ifp->if_unit););

  ifp->if_flags &= ~IFF_OACTIVE;
  DODEBUG(Status, printf("OIDLE watchdog\n"););
  ifp->if_oerrors++;
  ex_reset(ifp->if_unit);
  ex_start(ifp);

  DODEBUG(Start_End, printf("ex_watchdog%d: finish\n", ifp->if_unit););
}


static u_short eeprom_read(int iobase, int location)
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

#endif /* NEX > 0 */
