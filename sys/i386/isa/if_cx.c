/*
 * Cronyx-Sigma adapter driver for FreeBSD.
 * Supports PPP/HDLC and Cisco/HDLC protocol in synchronous mode,
 * and asyncronous channels with full modem control.
 * Keepalive protocol implemented in both Cisco and PPP modes.
 *
 * Copyright (C) 1994 Cronyx Ltd.
 * Author: Serge Vakulenko, <vak@zebub.msk.su>
 *
 * This software is distributed with NO WARRANTIES, not even the implied
 * warranties for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Authors grant any other persons or organisations permission to use
 * or modify this software as long as this message is kept with the software,
 * all derivative works or modified versions.
 *
 * Version 1.2, Tue Nov 22 18:57:27 MSK 1994
 */
#undef DEBUG

#include "cx.h"
#if NCX > 0
#include <bpfilter.h>

#include <sys/param.h>
#include <systm.h>
#include <kernel.h>
#include <sys/mbuf.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/conf.h>
#include <sys/errno.h>

#include <net/if.h>
#include <net/if_types.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#ifdef __FreeBSD__
#   include <i386/isa/isa_device.h>
#   if __FreeBSD__ < 2
#      include <i386/include/pio.h>
#   else
#      include <i386/include/cpufunc.h>
#      include <sys/devconf.h>
#   endif
#   define init_func_t     void(*)(int)
#   define watchdog_func_t void(*)(int)
#   define start_func_t    void(*)(struct ifnet*)
#endif

#ifdef __bsdi__
#   if INET
#      include <netinet/in.h>
#      include <netinet/in_systm.h>
#      include <netinet/ip.h>
#   endif
#   include <sys/device.h>
#   include <i386/isa/isavar.h>
#   include <i386/isa/icu.h>
#   include <i386/include/inline.h>
#   include <net/if_slvar.h>
#   include <net/if_p2p.h>
#   define timeout_func_t  void(*)()
#   define init_func_t     int(*)()
#   define watchdog_func_t int(*)()
#   define start_func_t    int(*)()
struct cxsoftc {
	struct device dev;      /* base device */
	struct isadev isadev;   /* ISA device */
	struct intrhand intr;   /* interrupt vectoring */
};
#endif

#include <net/if_sppp.h>
#include <sys/cronyx.h>
#include <i386/isa/cxreg.h>

#ifdef DEBUG
#   define print(s)     printf s
#else
#   define print(s)     /*void*/
#endif

#define TXTIMEOUT       2               /* transmit timeout in seconds */
#define DMABUFSZ        (6*256)         /* buffer size */
#define PPP_HEADER_LEN  4               /* size of PPP header */

/*
 * Under BSDI it's possible to use general p2p protocol scheme,
 * as well as our own one.  Switching is done via IFF_ALTPHYS flag.
 * Our ifnet pointer holds the buffer large enough to contain
 * any of sppp and p2p structures.
 */
#ifdef __bsdi__
#   define SPPPSZ       (sizeof (struct sppp))
#   define P2PSZ        (sizeof (struct p2pcom))
#   define IFSTRUCTSZ   (SPPPSZ>P2PSZ ? SPPPSZ : P2PSZ)
#else
#   define IFSTRUCTSZ   (sizeof (struct sppp))
#endif
#define IFNETSZ         (sizeof (struct ifnet))

int cxoutput (struct ifnet *ifp, struct mbuf *m, struct sockaddr *dst, struct rtentry *rt);
int cxsioctl (struct ifnet *ifp, int cmd, caddr_t data);
void cxinit (int unit);
void cxstart (struct ifnet *ifp);
void cxwatchdog (int unit);
void cxinput (cx_chan_t *c, void *buf, unsigned len);
int cxrinta (cx_chan_t *c);
void cxtinta (cx_chan_t *c);
void cxmint (cx_chan_t *c);
void cxtimeout (caddr_t a);

cx_board_t cxboard [NCX];               /* adapter state structures */
cx_chan_t *cxchan [NCX*NCHAN];          /* unit to channel struct pointer */

static unsigned short irq_valid_values [] = { 3, 5, 7, 10, 11, 12, 15, 0 };
static unsigned short drq_valid_values [] = { 5, 6, 7, 0 };
static unsigned short port_valid_values [] = {
	0x240, 0x260, 0x280, 0x300, 0x320, 0x380, 0x3a0, 0,
};

#if __FreeBSD__ >= 2
static char cxdescription [80];
struct kern_devconf kdc_cx [NCX] = { {
	0, 0, 0, "cx", 0, { MDDT_ISA, 0, "net" },
	isa_generic_externalize, 0, 0, ISA_EXTERNALLEN, &kdc_isa0, 0,
	DC_IDLE, cxdescription,
} };
#endif

/*
 * Check that the value is contained in the list of correct values.
 */
static int valid (unsigned short value, unsigned short *list)
{
	while (*list)
		if (value == *list++)
			return (1);
	return (0);
}

/*
 * Print the mbuf chain, for debug purposes only.
 */
static void printmbuf (struct mbuf *m)
{
	printf ("mbuf:");
	for (; m; m=m->m_next) {
		if (m->m_flags & M_PKTHDR)
			printf (" HDR %d:", m->m_pkthdr.len);
		if (m->m_flags & M_EXT)
			printf (" EXT:");
		printf (" %d", m->m_len);
	}
	printf ("\n");
}

/*
 * Make an mbuf from data.
 */
static struct mbuf *makembuf (void *buf, unsigned len)
{
	struct mbuf *m, *o, *p;

	MGETHDR (m, M_DONTWAIT, MT_DATA);
	if (! m)
		return (0);
	if (len >= MINCLSIZE)
		MCLGET (m, M_DONTWAIT);
	m->m_pkthdr.len = len;
	m->m_len = 0;

	p = m;
	while (len) {
		unsigned n = M_TRAILINGSPACE (p);
		if (n > len)
			n = len;

		if (! n) {
			/* Allocate new mbuf. */
			o = p;
			MGET (p, M_DONTWAIT, MT_DATA);
			if (! p) {
				m_freem (m);
				return (0);
			}
			if (len >= MINCLSIZE)
				MCLGET (p, M_DONTWAIT);
			p->m_len = 0;
			o->m_next = p;

			n = M_TRAILINGSPACE (p);
			if (n > len)
				n = len;
		}

		bcopy (buf, mtod (p, caddr_t) + p->m_len, n);

		p->m_len += n;
		buf += n;
		len -= n;
	}
	return (m);
}

/*
 * Test the presence of the adapter on the given i/o port.
 */
#ifdef __FreeBSD__
int cxprobe (struct isa_device *id)
{
	int unit = id->id_unit;
	int iobase = id->id_iobase;
	int irq = id->id_irq;
	int drq = id->id_drq;
	int irqnum;
#endif
#ifdef __bsdi__
int cxprobe (struct device *parent, struct cfdata *cf, void *aux)
{
	int unit = cf->cf_unit;
	int iobase = ((struct isa_attach_args*)aux)->ia_iobase;
	int irq = ((struct isa_attach_args*)aux)->ia_irq;
	int drq = ((struct isa_attach_args*)aux)->ia_drq;
	int irqnum, i;

	for (i=0; i<NCX; ++i)
		if (i != unit && cxboard[i].port == iobase)
			return (0);
	if (irq == IRQUNK) {
		irq = isa_irqalloc (IRQ3|IRQ5|IRQ7|IRQ10|IRQ11|IRQ12|IRQ15);
		if (! irq)
			return (0);
		((struct isa_attach_args*)aux)->ia_irq = irq;
	}
#endif
	irqnum = ffs (irq) - 1;

	print (("cx%d: probe iobase=0x%x irq=%d drq=%d\n",
		unit, iobase, irqnum, drq));
	if (! valid (irqnum, irq_valid_values)) {
		printf ("cx%d: Incorrect IRQ: %d\n", unit, irqnum);
		return (0);
	}
	if (! valid (iobase, port_valid_values)) {
		printf ("cx%d: Incorrect port address: 0x%x\n", unit, iobase);
		return (0);
	}
	if (! valid (drq, drq_valid_values)) {
		printf ("cx%d: Incorrect DMA channel: %d\n", unit, drq);
		return (0);
	}
	if (! cx_probe_board (iobase))
		return (0);
	return (1);
}

/*
 * The adapter is present, initialize the driver structures.
 */
#ifdef __FreeBSD__
int cxattach (struct isa_device *id)
{
	int unit = id->id_unit;
	int iobase = id->id_iobase;
	int irq = id->id_irq;
	int drq = id->id_drq;
#endif
#ifdef __bsdi__
void cxattach (struct device *parent, struct device *self, void *aux)
{
	int unit = self->dv_unit;
	int iobase = ((struct isa_attach_args*)aux)->ia_iobase;
	int irq = ((struct isa_attach_args*)aux)->ia_irq;
	int drq = ((struct isa_attach_args*)aux)->ia_drq;
	struct cxsoftc *sc = (struct cxsoftc*) self;
	void cxintr (cx_board_t *b);
#endif
	cx_board_t *b = cxboard + unit;
	int i;

	/* Initialize the board structure. */
	cx_init (b, unit, iobase, ffs(irq)-1, drq);

	for (i=0; i<NCHAN; ++i) {
		cx_chan_t *c = b->chan + i;
		int u = b->num*NCHAN + i;
		cxchan[u] = c;

		if (c->type == T_NONE)
			continue;

		/* Allocate the buffer memory. */
		c->arbuf = malloc (DMABUFSZ, M_DEVBUF, M_NOWAIT);
		c->brbuf = malloc (DMABUFSZ, M_DEVBUF, M_NOWAIT);
		c->atbuf = malloc (DMABUFSZ, M_DEVBUF, M_NOWAIT);
		c->btbuf = malloc (DMABUFSZ, M_DEVBUF, M_NOWAIT);

		/* All buffers should be located in lower 16M of memory! */
		if (!c->arbuf || !c->brbuf || !c->atbuf || !c->btbuf) {
			printf ("cx%d.%d: No memory for channel buffers\n",
				c->board->num, c->num);
			c->type = T_NONE;
		}

		switch (c->type) {
		case T_SYNC_RS232:
		case T_SYNC_V35:
		case T_SYNC_RS449:
		case T_UNIV_RS232:
		case T_UNIV_RS449:
		case T_UNIV_V35:
			c->ifp = malloc (IFSTRUCTSZ, M_DEVBUF, M_NOWAIT);
			if (! c->ifp) {
				printf ("cx%d.%d: No memory for ifnet buffer\n",
					c->board->num, c->num);
				c->type = T_NONE;
				continue;
			}
			c->ifp->if_unit = u;
			c->ifp->if_name = "cx";
			c->ifp->if_mtu = PP_MTU;
			c->ifp->if_flags = IFF_POINTOPOINT|IFF_MULTICAST;
			c->ifp->if_ioctl = cxsioctl;
			c->ifp->if_start = (start_func_t) cxstart;
			c->ifp->if_watchdog = (watchdog_func_t) cxwatchdog;
			/* Init routine is never called by upper level? */
			c->ifp->if_init = (init_func_t) cxinit;
			sppp_attach (c->ifp);
			if_attach (c->ifp);
#if NBPFILTER > 0
			/* If BPF is in the kernel, call the attach for it. */
			bpfattach (&c->bpf, c->ifp, DLT_PPP, PPP_HEADER_LEN);
#endif
		}
	}

	/* Reset the adapter. */
	cx_setup_board (b);

	/* Activate the timeout routine. */
	if (unit == 0)
		timeout ((timeout_func_t) cxtimeout, 0, hz*5);

#if __FreeBSD__ >= 2
	if (unit != 0)
		kdc_cx[unit] = kdc_cx[0];
	kdc_cx[unit].kdc_unit = unit;
	kdc_cx[unit].kdc_isa = id;
	sprintf (cxdescription, "Cronyx-Sigma-%s sync/async serial adapter",
		b->name);
	dev_attach (&kdc_cx[unit]);
#endif
#ifdef __FreeBSD__
	printf ("cx%d: <Cronyx-%s>\n", unit, b->name);
	return (1);
#endif
#ifdef __bsdi__
	printf (": <Cronyx-%s>\n", b->name);
	isa_establish (&sc->isadev, &sc->dev);
	sc->intr.ih_fun = (int(*)()) cxintr;
	sc->intr.ih_arg = (void*) b;
	intr_establish (irq, &sc->intr, DV_NET);
#endif
}

#ifdef __FreeBSD__
struct isa_driver cxdriver = { cxprobe, cxattach, "cx" };
#endif
#ifdef __bsdi__
struct cfdriver cxcd = { 0, "cx", cxprobe, cxattach, sizeof (struct cxsoftc) };
#endif

/*
 * Process an ioctl request.
 */
int cxsioctl (struct ifnet *ifp, int cmd, caddr_t data)
{
	cx_chan_t *c = cxchan[ifp->if_unit];
	int error, s;

	if (c->type==T_NONE || c->mode==M_ASYNC)
		return (EINVAL);
#ifdef __bsdi__
	if (c->sopt.ext) {
		/* Save RUNNING flag. */
		int running = (ifp->if_flags & IFF_RUNNING);
		error = p2p_ioctl (ifp, cmd, data);
		ifp->if_flags &= ~IFF_RUNNING;
		ifp->if_flags |= running;
	} else
#endif
	error = sppp_ioctl (ifp, cmd, data);
	if (error)
		return (error);
	if (cmd != SIOCSIFFLAGS)
		return (0);

	s = splimp ();
	if (! (ifp->if_flags & IFF_UP) && (ifp->if_flags & IFF_RUNNING))
		/* Interface is down and running -- stop it. */
		cxinit (ifp->if_unit);
	else if ((ifp->if_flags & IFF_UP) && ! (ifp->if_flags & IFF_RUNNING))
		/* Interface is up and not running -- start it. */
		cxinit (ifp->if_unit);
	splx (s);
	return (0);
}

/*
 * Initialization of interface.
 */
void cxinit (int unit)
{
	cx_chan_t *c = cxchan[unit];
	unsigned short port = c->chip->port;
	int s;

	print (("cx%d.%d: cxinit\n", c->board->num, c->num));

	/* Disable interrupts */
	s = splimp();

	/* Reset the channel (for sync modes only) */
	if (c->ifp->if_flags & IFF_OACTIVE) {
		outb (CAR(port), c->num & 3);
		outb (STCR(port), STC_ABORTTX | STC_SNDSPC);
	}
	cx_setup_chan (c);
	c->ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	if (c->ifp->if_flags & IFF_UP) {
		/* The interface is up, start it */
		c->ifp->if_flags |= IFF_RUNNING;

#if __FreeBSD__ >= 2
		/* Mark the board busy on the first startup.
		 * Never goes idle. */
		kdc_cx[c->board->num].kdc_state = DC_BUSY;
#endif
		/* Initialize channel, enable receiver and transmitter */
		cx_cmd (port, CCR_INITCH | CCR_ENRX | CCR_ENTX);
		/* Repeat the command, to avoid the rev.H bug */
		cx_cmd (port, CCR_INITCH | CCR_ENRX | CCR_ENTX);

		/* Start receiver */
		outw (ARBCNT(port), DMABUFSZ);
		outb (ARBSTS(port), BSTS_OWN24);
		outw (BRBCNT(port), DMABUFSZ);
		outb (BRBSTS(port), BSTS_OWN24);

		/* Raise DTR and RTS */
		cx_chan_dtr (c, 1);
		cx_chan_rts (c, 1);

		/* Enable interrupts */
		outb (IER(port), IER_RXD | IER_TXD);

		cxstart (c->ifp);

	} else if (! c->sopt.ext)
		/* Flush the interface output queue, if it is down */
		sppp_flush (c->ifp);

	splx (s);
}

/*
 * Fill transmitter buffer with data.
 */
void cxput (cx_chan_t *c, char b)
{
	struct mbuf *m;
	unsigned char *buf;
	unsigned short port = c->chip->port, len, cnt_port, sts_port;

	/* Choose the buffer. */
	if (b == 'A') {
		buf      = c->atbuf;
		cnt_port = ATBCNT(port);
		sts_port = ATBSTS(port);
	} else {
		buf      = c->btbuf;
		cnt_port = BTBCNT(port);
		sts_port = BTBSTS(port);
	}

	/* Is it busy? */
	if (inb (sts_port) & BSTS_OWN24) {
		c->ifp->if_flags |= IFF_OACTIVE;
		return;
	}

	/* Get the packet to send. */
#ifdef __bsdi__
	if (c->sopt.ext) {
		struct p2pcom *p = (struct p2pcom*) c->ifp;
		int s = splimp ();

		IF_DEQUEUE (&p->p2p_isnd, m)
		if (! m)
			IF_DEQUEUE (&c->ifp->if_snd, m)
		splx (s);
	} else
#endif
	m = sppp_dequeue (c->ifp);
	if (! m)
		return;
	len = m->m_pkthdr.len;
	if (len >= DMABUFSZ) {
		printf ("cx%d.%d: too long packet: %d bytes\n",
			c->board->num, c->num, len);
		printmbuf (m);
		m_freem (m);
		return;
	}
	m_copydata (m, 0, len, buf);
#if NBPFILTER > 0
	if (c->bpf)
		bpf_mtap (c->bpf, m);
#endif
	m_freem (m);

	/* Start transmitter. */
	outw (cnt_port, len);
	outb (sts_port, BSTS_EOFR | BSTS_INTR | BSTS_OWN24);
#ifdef DEBUG
	if (c->ifp->if_flags & IFF_DEBUG)
		printf ("cx%d.%d: enqueue %d bytes to %c\n",
			c->board->num, c->num, len, buf==c->atbuf ? 'A' : 'B');
#endif
	c->ifp->if_flags |= IFF_OACTIVE;
}

/*
 * Start output on interface.  Get another datagram to send
 * off of the interface queue, and copy it to the interface
 * before starting the output.
 */
void cxstart (struct ifnet *ifp)
{
	cx_chan_t *c = cxchan[ifp->if_unit];
	unsigned short port = c->chip->port;

	/* No output if the interface is down. */
	if (! (ifp->if_flags & IFF_RUNNING))
		return;

	/* Set the current channel number. */
	outb (CAR(port), c->num & 3);

	/* Determine the buffer order. */
	if (inb (DMABSTS(port)) & DMABSTS_NTBUF) {
		cxput (c, 'B');
		cxput (c, 'A');
	} else {
		cxput (c, 'A');
		cxput (c, 'B');
	}

	/* Set up transmit timeout. */
	if (c->ifp->if_flags & IFF_OACTIVE)
		c->ifp->if_timer = TXTIMEOUT;

	/*
	 * Enable TXMPTY interrupt,
	 * to catch the case when the second buffer is empty.
	 */
	if ((inb (ATBSTS(port)) & BSTS_OWN24) &&
	    (inb (BTBSTS(port)) & BSTS_OWN24)) {
		outb (IER(port), IER_RXD | IER_TXD | IER_TXMPTY);
	} else
		outb (IER(port), IER_RXD | IER_TXD);
}

/*
 * Handle transmit timeouts.
 * Recover after lost transmit interrupts.
 */
void cxwatchdog (int unit)
{
	cx_chan_t *c = cxchan[unit];

	if (c->ifp->if_flags & IFF_OACTIVE) {
		c->ifp->if_flags &= ~IFF_OACTIVE;
		cxstart (c->ifp);
	}
}

/*
 * Handle receive interrupts, including receive errors and
 * receive timeout interrupt.
 */
void cxrinth (cx_chan_t *c)
{
	unsigned short port = c->chip->port;
	unsigned short len, risr = inw (RISR(port));

	print (("cx%d.%d: hdlc receive interrupt, risr=%b, arbsts=%b, brbsts=%b\n",
		c->board->num, c->num, risr, RISH_BITS,
		inb (ARBSTS(port)), BSTS_BITS, inb (BRBSTS(port)), BSTS_BITS));

	/* Receive errors. */
	if (risr & (RIS_BUSERR | RIS_OVERRUN | RISH_CRCERR | RISH_RXABORT)) {
		if (c->ifp->if_flags & IFF_DEBUG)
			printf ("cx%d.%d: receive error, risr=%b\n",
				c->board->num, c->num, risr, RISH_BITS);
		++c->ifp->if_ierrors;
		if (risr & RIS_OVERRUN)
			++c->ifp->if_collisions;
	} else if (risr & RIS_EOBUF) {
		/* Handle received data. */
		len = (risr & RIS_BB) ? inw(BRBCNT(port)) : inw(ARBCNT(port));
		if (len > DMABUFSZ) {
			/* Fatal error: actual DMA transfer size
			 * exceeds our buffer size.  It could be caused
			 * by incorrectly programmed DMA register or
			 * hardware fault.  Possibly, should panic here. */
			printf ("cx%d.%d: panic! DMA buffer overflow: %d bytes\n",
			       c->board->num, c->num, len);
			++c->ifp->if_ierrors;
		} else if (! (risr & RIS_EOFR)) {
			/* The received frame does not fit in the DMA buffer.
			 * It could be caused by serial lie noise,
			 * or if the peer has too big MTU. */
			if (c->ifp->if_flags & IFF_DEBUG)
				printf ("cx%d.%d: received frame length exceeds MTU, risr=%b\n",
					c->board->num, c->num, risr, RISH_BITS);
			++c->ifp->if_ierrors;
		} else {
			/* Valid frame received. */
			print (("cx%d.%d: HDLC: %d bytes received\n",
				c->board->num, c->num, len));
			cxinput (c, (risr & RIS_BB) ? c->brbuf : c->arbuf, len);
			++c->ifp->if_ipackets;
		}
	}

	/* Restart receiver. */
	if (! (inb (ARBSTS(port)) & BSTS_OWN24)) {
		outw (ARBCNT(port), DMABUFSZ);
		outb (ARBSTS(port), BSTS_OWN24);
	}
	if (! (inb (BRBSTS(port)) & BSTS_OWN24)) {
		outw (BRBCNT(port), DMABUFSZ);
		outb (BRBSTS(port), BSTS_OWN24);
	}
}

/*
 * Handle transmit interrupt.
 */
int cxtinth (cx_chan_t *c)
{
	unsigned short port = c->chip->port;
	unsigned char tisr = inb (TISR(port));
	unsigned char teoir = 0;

	print (("cx%d.%d: hdlc transmit interrupt, tisr=%b, atbsts=%b, btbsts=%b\n",
		c->board->num, c->num, tisr, TIS_BITS,
		inb (ATBSTS(port)), BSTS_BITS, inb (BTBSTS(port)), BSTS_BITS));

	if (tisr & (TIS_BUSERR | TIS_UNDERRUN)) {
		if (c->ifp->if_flags & IFF_DEBUG)
			printf ("cx%d.%d: transmit error, tisr=%b\n",
				c->board->num, c->num, tisr, TIS_BITS);
		++c->ifp->if_oerrors;
		/* Terminate the failed buffer. */
		teoir |= TEOI_TERMBUFF;
	} else if (tisr & (TIS_EOFR))
		++c->ifp->if_opackets;

	c->ifp->if_flags &= ~IFF_OACTIVE;
	cxstart (c->ifp);
	return (teoir);
}

#ifdef __FreeBSD__
void cxintr (int bnum)
{
	cx_board_t *b = cxboard + bnum;
#endif
#ifdef __bsdi__
void cxintr (cx_board_t *b)
{
#endif
	while (! (inw (BSR(b->port)) & BSR_NOINTR)) {
		/* Acknowledge the interrupt to enter the interrupt context. */
		/* Read the local interrupt vector register. */
		unsigned char livr = inb (IACK(b->port, BRD_INTR_LEVEL));
		cx_chan_t *c = b->chan + (livr>>2 & 0xf);
		unsigned short port = c->chip->port;
		unsigned short eoiport = REOIR(port);
		unsigned char eoi = 0;

		if (c->type == T_NONE) {
			printf ("cx%d.%d: unexpected interrupt, livr=0x%x\n",
				c->board->num, c->num, livr);
			continue;       /* incorrect channel number? */
		}
		/* print (("cx%d.%d: interrupt, livr=0x%x\n",
			c->board->num, c->num, livr)); */

		/* Clear RTS to stop receiver data flow while we are busy
		 * processing the interrupt, thus avoiding underruns. */
		if (! c->sopt.norts) {
			outb (MSVR_RTS(port), 0);
			c->rts = 0;
		}

		switch (livr & 3) {
		case LIV_EXCEP:         /* receive exception */
		case LIV_RXDATA:        /* receive interrupt */
			switch (c->mode) {
			case M_ASYNC: eoi = cxrinta (c); break;
			case M_HDLC:  cxrinth (c);       break;
			default:;       /* No bisync and X.21 yet */
			}
			break;
		case LIV_TXDATA:        /* transmit interrupt */
			eoiport = TEOIR(port);
			switch (c->mode) {
			case M_ASYNC: cxtinta (c);       break;
			case M_HDLC:  eoi = cxtinth (c); break;
			default:;       /* No bisync and X.21 yet */
			}
			break;
		case LIV_MODEM:         /* modem/timer interrupt */
			eoiport = MEOIR(port);
			cxmint (c);
			break;
		}

		/* Raise RTS for this channel if and only if
		 * both receive buffers are empty. */
		if (! c->sopt.norts && (inb (CSR(port)) & CSRA_RXEN) &&
		    (inb (ARBSTS(port)) & BSTS_OWN24) &&
		    (inb (BRBSTS(port)) & BSTS_OWN24)) {
			outb (MSVR_RTS(port), MSV_RTS);
			c->rts = 1;
		}

		/* Exit from interrupt context. */
		outb (eoiport, eoi);
	}
}

/*
 * Process the received packet.
 */
void cxinput (cx_chan_t *c, void *buf, unsigned len)
{
	/* Make an mbuf. */
	struct mbuf *m = makembuf (buf, len);
	if (! m) {
		if (c->ifp->if_flags & IFF_DEBUG)
			printf ("cx%d.%d: no memory for packet\n",
				c->board->num, c->num);
		++c->ifp->if_iqdrops;
		return;
	}
	m->m_pkthdr.rcvif = c->ifp;
#ifdef DEBUG
	printmbuf (m);
#endif

#if NBPFILTER > 0
	/*
	 * Check if there's a BPF listener on this interface.
	 * If so, hand off the raw packet to bpf. 
	 */
	if (c->bpf)
		bpf_tap (c->bpf, buf, len);
#endif
#ifdef __bsdi__
	if (c->sopt.ext) {
		struct p2pcom *p = (struct p2pcom*) c->ifp;
		(*p->p2p_input) (p, m);
	} else
#endif
	sppp_input (c->ifp, m);
}

void cxswitch (cx_chan_t *c, cx_soft_opt_t new)
{
#ifdef __bsdi__
	if (new.ext && ! c->sopt.ext) {
		/* Switch to external ppp implementation (BSDI) */
		sppp_detach (c->ifp);
		bzero ((void*) c->ifp + IFNETSZ, IFSTRUCTSZ-IFNETSZ);
	} else if (! new.ext && c->sopt.ext) {
		/* Switch to built-in ppp implementation */
		bzero ((void*) c->ifp + IFNETSZ, IFSTRUCTSZ-IFNETSZ);
		sppp_attach (c->ifp);
	}
#else
	new.ext = 0;
#endif
	if (! new.ext) {
		struct sppp *sp = (struct sppp*) c->ifp;

		if (new.cisco)
			sp->pp_flags |= PP_CISCO;
		else
			sp->pp_flags &= ~PP_CISCO;
		if (new.keepalive)
			sp->pp_flags |= PP_KEEPALIVE;
		else
			sp->pp_flags &= ~PP_KEEPALIVE;
	}
	c->sopt = new;
}
#endif /* NCX */
