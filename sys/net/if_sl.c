/*
 * Copyright (c) 1987, 1989, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)if_sl.c	8.6 (Berkeley) 2/1/94
 * $Id: if_sl.c,v 1.67 1998/02/13 12:46:14 phk Exp $
 */

/*
 * Serial Line interface
 *
 * Rick Adams
 * Center for Seismic Studies
 * 1300 N 17th Street, Suite 1450
 * Arlington, Virginia 22209
 * (703)276-7900
 * rick@seismo.ARPA
 * seismo!rick
 *
 * Pounded on heavily by Chris Torek (chris@mimsy.umd.edu, umcp-cs!chris).
 * N.B.: this belongs in netinet, not net, the way it stands now.
 * Should have a link-layer type designation, but wouldn't be
 * backwards-compatible.
 *
 * Converted to 4.3BSD Beta by Chris Torek.
 * Other changes made at Berkeley, based in part on code by Kirk Smith.
 * W. Jolitz added slip abort.
 *
 * Hacked almost beyond recognition by Van Jacobson (van@helios.ee.lbl.gov).
 * Added priority queuing for "interactive" traffic; hooks for TCP
 * header compression; ICMP filtering (at 2400 baud, some cretin
 * pinging you can use up all your bandwidth).  Made low clist behavior
 * more robust and slightly less likely to hang serial line.
 * Sped up a bunch of things.
 *
 * Note that splimp() is used throughout to block both (tty) input
 * interrupts and network activity; thus, splimp must be >= spltty.
 */

#include "sl.h"
#if NSL > 0

#include "bpfilter.h"
#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/dkstat.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/fcntl.h>
#include <sys/signalvar.h>
#include <sys/tty.h>
#include <sys/clist.h>
#include <sys/kernel.h>
#include <sys/conf.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/netisr.h>

#if INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#else
#error "Huh? Slip without inet?"
#endif

#include <net/slcompress.h>
#include <net/if_slvar.h>
#include <net/slip.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

static void slattach __P((void *));
PSEUDO_SET(slattach, if_sl);

/*
 * SLRMAX is a hard limit on input packet size.  To simplify the code
 * and improve performance, we require that packets fit in an mbuf
 * cluster, and if we get a compressed packet, there's enough extra
 * room to expand the header into a max length tcp/ip header (128
 * bytes).  So, SLRMAX can be at most
 *	MCLBYTES - 128
 *
 * SLMTU is the default transmit MTU. The transmit MTU should be kept
 * small enough so that interactive use doesn't suffer, but large
 * enough to provide good performance. 552 is a good choice for SLMTU
 * because it is high enough to not fragment TCP packets being routed
 * through this host. Packet fragmentation is bad with SLIP because
 * fragment headers aren't compressed. The previous assumptions about
 * the best MTU value don't really hold when using modern modems with
 * BTLZ data compression because the modem buffers play a much larger
 * role in interactive performance than the MTU. The MTU can be changed
 * at any time to suit the specific environment with ifconfig(8), and
 * its maximum value is defined as SLTMAX. SLTMAX must not be so large
 * that it would overflow the stack if BPF is configured (XXX; if_ppp.c
 * handles this better).
 *
 * SLIP_HIWAT is the amount of data that will be queued 'downstream'
 * of us (i.e., in clists waiting to be picked up by the tty output
 * interrupt).  If we queue a lot of data downstream, it's immune to
 * our t.o.s. queuing.
 * E.g., if SLIP_HIWAT is 1024, the interactive traffic in mixed
 * telnet/ftp will see a 1 sec wait, independent of the mtu (the
 * wait is dependent on the ftp window size but that's typically
 * 1k - 4k).  So, we want SLIP_HIWAT just big enough to amortize
 * the cost (in idle time on the wire) of the tty driver running
 * off the end of its clists & having to call back slstart for a
 * new packet.  For a tty interface with any buffering at all, this
 * cost will be zero.  Even with a totally brain dead interface (like
 * the one on a typical workstation), the cost will be <= 1 character
 * time.  So, setting SLIP_HIWAT to ~100 guarantees that we'll lose
 * at most 1% while maintaining good interactive response.
 */
#if NBPFILTER > 0
#define	BUFOFFSET	(128+sizeof(struct ifnet **)+SLIP_HDRLEN)
#else
#define	BUFOFFSET	(128+sizeof(struct ifnet **))
#endif
#define	SLRMAX		(MCLBYTES - BUFOFFSET)
#define	SLBUFSIZE	(SLRMAX + BUFOFFSET)
#ifndef SLMTU
#define	SLMTU		552		/* default MTU */
#endif
#define	SLTMAX		1500		/* maximum MTU */
#define	SLIP_HIWAT	roundup(50,CBSIZE)
#define	CLISTRESERVE	1024		/* Can't let clists get too low */

/*
 * SLIP ABORT ESCAPE MECHANISM:
 *	(inspired by HAYES modem escape arrangement)
 *	1sec escape 1sec escape 1sec escape { 1sec escape 1sec escape }
 *	within window time signals a "soft" exit from slip mode by remote end
 *	if the IFF_DEBUG flag is on.
 */
#define	ABT_ESC		'\033'	/* can't be t_intr - distant host must know it*/
#define	ABT_IDLE	1	/* in seconds - idle before an escape */
#define	ABT_COUNT	3	/* count of escapes for abort */
#define	ABT_WINDOW	(ABT_COUNT*2+2)	/* in seconds - time to count */

static struct sl_softc sl_softc[NSL];

#define FRAME_END	 	0xc0		/* Frame End */
#define FRAME_ESCAPE		0xdb		/* Frame Esc */
#define TRANS_FRAME_END	 	0xdc		/* transposed frame end */
#define TRANS_FRAME_ESCAPE 	0xdd		/* transposed frame esc */

static int slinit __P((struct sl_softc *));
static struct mbuf *sl_btom __P((struct sl_softc *, int));
static timeout_t sl_keepalive;
static timeout_t sl_outfill;
static int	slclose __P((struct tty *,int));
static int	slinput __P((int, struct tty *));
static int	slioctl __P((struct ifnet *, int, caddr_t));
static int	sltioctl __P((struct tty *, int, caddr_t, int, struct proc *));
static int	slopen __P((dev_t, struct tty *));
static int	sloutput __P((struct ifnet *,
	    struct mbuf *, struct sockaddr *, struct rtentry *));
static int	slstart __P((struct tty *));

static struct linesw slipdisc = {
	slopen,		slclose,	l_noread,	l_nowrite,
	sltioctl,	slinput,	slstart,	ttymodem,
	FRAME_END
};

/*
 * Called from boot code to establish sl interfaces.
 */
static void
slattach(dummy)
	void *dummy;
{
	register struct sl_softc *sc;
	register int i = 0;

	linesw[SLIPDISC] = slipdisc;

	for (sc = sl_softc; i < NSL; sc++) {
		sc->sc_if.if_name = "sl";
		sc->sc_if.if_unit = i++;
		sc->sc_if.if_mtu = SLMTU;
		sc->sc_if.if_flags =
		    IFF_POINTOPOINT | SC_AUTOCOMP | IFF_MULTICAST;
		sc->sc_if.if_type = IFT_SLIP;
		sc->sc_if.if_ioctl = slioctl;
		sc->sc_if.if_output = sloutput;
		sc->sc_if.if_snd.ifq_maxlen = 50;
		sc->sc_fastq.ifq_maxlen = 32;
		sc->sc_if.if_linkmib = sc;
		sc->sc_if.if_linkmiblen = sizeof *sc;
		if_attach(&sc->sc_if);
#if NBPFILTER > 0
		bpfattach(&sc->sc_if, DLT_SLIP, SLIP_HDRLEN);
#endif
	}
}

static int
slinit(sc)
	register struct sl_softc *sc;
{
	register caddr_t p;

	if (sc->sc_ep == (u_char *) 0) {
		MCLALLOC(p, M_WAIT);
		if (p)
			sc->sc_ep = (u_char *)p + SLBUFSIZE;
		else {
			printf("sl%d: can't allocate buffer\n", sc - sl_softc);
			return (0);
		}
	}
	sc->sc_buf = sc->sc_ep - SLRMAX;
	sc->sc_mp = sc->sc_buf;
	sl_compress_init(&sc->sc_comp, -1);
	return (1);
}

/*
 * Line specific open routine.
 * Attach the given tty to the first available sl unit.
 */
/* ARGSUSED */
static int
slopen(dev, tp)
	dev_t dev;
	register struct tty *tp;
{
	struct proc *p = curproc;		/* XXX */
	register struct sl_softc *sc;
	register int nsl;
	int s, error;

	error = suser(p->p_ucred, &p->p_acflag);
	if (error)
		return (error);

	if (tp->t_line == SLIPDISC)
		return (0);

	for (nsl = NSL, sc = sl_softc; --nsl >= 0; sc++)
		if (sc->sc_ttyp == NULL && !(sc->sc_flags & SC_STATIC)) {
			if (slinit(sc) == 0)
				return (ENOBUFS);
			tp->t_sc = (caddr_t)sc;
			sc->sc_ttyp = tp;
			sc->sc_if.if_baudrate = tp->t_ospeed;
			ttyflush(tp, FREAD | FWRITE);

			tp->t_line = SLIPDISC;
			/*
			 * We don't use t_canq or t_rawq, so reduce their
			 * cblock resources to 0.  Reserve enough cblocks
			 * for t_outq to guarantee that we can fit a full
			 * packet if the SLIP_HIWAT check allows slstart()
			 * to loop.  Use the same value for the cblock
			 * limit since the reserved blocks should always
			 * be enough.  Reserving cblocks probably makes
			 * the CLISTRESERVE check unnecessary and wasteful.
			 */
			clist_alloc_cblocks(&tp->t_canq, 0, 0);
			clist_alloc_cblocks(&tp->t_outq,
			    SLIP_HIWAT + 2 * sc->sc_if.if_mtu + 1,
			    SLIP_HIWAT + 2 * sc->sc_if.if_mtu + 1);
			clist_alloc_cblocks(&tp->t_rawq, 0, 0);

			s = splnet();
			if_up(&sc->sc_if);
			splx(s);
			return (0);
		}
	return (ENXIO);
}

/*
 * Line specific close routine.
 * Detach the tty from the sl unit.
 */
static int
slclose(tp,flag)
	struct tty *tp;
	int flag;
{
	register struct sl_softc *sc;
	int s;

	ttyflush(tp, FREAD | FWRITE);
	/*
	 * XXX the placement of the following spl is misleading.  tty
	 * interrupts must be blocked across line discipline switches
	 * and throughout closes to avoid races.
	 */
	s = splimp();		/* actually, max(spltty, splnet) */
	clist_free_cblocks(&tp->t_outq);
	tp->t_line = 0;
	sc = (struct sl_softc *)tp->t_sc;
	if (sc != NULL) {
		if (sc->sc_outfill) {
			sc->sc_outfill = 0;
			untimeout(sl_outfill, sc, sc->sc_ofhandle);
		}
		if (sc->sc_keepalive) {
			sc->sc_keepalive = 0;
			untimeout(sl_keepalive, sc, sc->sc_kahandle);
		}
		if_down(&sc->sc_if);
		sc->sc_flags &= SC_STATIC;
		sc->sc_ttyp = NULL;
		tp->t_sc = NULL;
		MCLFREE((caddr_t)(sc->sc_ep - SLBUFSIZE));
		sc->sc_ep = 0;
		sc->sc_mp = 0;
		sc->sc_buf = 0;
	}
	splx(s);
	return 0;
}

/*
 * Line specific (tty) ioctl routine.
 * Provide a way to get the sl unit number.
 */
/* ARGSUSED */
static int
sltioctl(tp, cmd, data, flag, p)
	struct tty *tp;
	int cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct sl_softc *sc = (struct sl_softc *)tp->t_sc, *nc, *tmpnc;
	int s, nsl;

	s = splimp();
	switch (cmd) {
	case SLIOCGUNIT:
		*(int *)data = sc->sc_if.if_unit;
		break;

	case SLIOCSUNIT:
		if (sc->sc_if.if_unit != *(u_int *)data) {
			for (nsl = NSL, nc = sl_softc; --nsl >= 0; nc++) {
				if (   nc->sc_if.if_unit == *(u_int *)data
				    && nc->sc_ttyp == NULL
				   ) {
					tmpnc = malloc(sizeof *tmpnc, M_TEMP,
						       M_NOWAIT);
					if (tmpnc == NULL) {
						splx(s);
						return (ENOMEM);
					}
					*tmpnc = *nc;
					*nc = *sc;
					nc->sc_if = tmpnc->sc_if;
					tmpnc->sc_if = sc->sc_if;
					*sc = *tmpnc;
					free(tmpnc, M_TEMP);
					if (sc->sc_if.if_flags & IFF_UP) {
						if_down(&sc->sc_if);
						if (!(nc->sc_if.if_flags & IFF_UP))
							if_up(&nc->sc_if);
					} else if (nc->sc_if.if_flags & IFF_UP)
						if_down(&nc->sc_if);
					sc->sc_flags &= ~SC_STATIC;
					sc->sc_flags |= (nc->sc_flags & SC_STATIC);
					tp->t_sc = sc = nc;
					goto slfound;
				}
			}
			splx(s);
			return (ENXIO);
		}
	slfound:
		sc->sc_flags |= SC_STATIC;
		break;

	case SLIOCSKEEPAL:
		sc->sc_keepalive = *(u_int *)data * hz;
		if (sc->sc_keepalive) {
			sc->sc_flags |= SC_KEEPALIVE;
			sc->sc_kahandle = timeout(sl_keepalive, sc,
						  sc->sc_keepalive);
		} else {
			if ((sc->sc_flags & SC_KEEPALIVE) != 0) {
				untimeout(sl_keepalive, sc, sc->sc_kahandle);
				sc->sc_flags &= ~SC_KEEPALIVE;
			}
		}
		break;

	case SLIOCGKEEPAL:
		*(int *)data = sc->sc_keepalive / hz;
		break;

	case SLIOCSOUTFILL:
		sc->sc_outfill = *(u_int *)data * hz;
		if (sc->sc_outfill) {
			sc->sc_flags |= SC_OUTWAIT;
			sc->sc_ofhandle = timeout(sl_outfill, sc,
						  sc->sc_outfill);
		} else {
			if ((sc->sc_flags & SC_OUTWAIT) != 0) {
				untimeout(sl_outfill, sc, sc->sc_ofhandle);
				sc->sc_flags &= ~SC_OUTWAIT;
			}
		}
		break;

	case SLIOCGOUTFILL:
		*(int *)data = sc->sc_outfill / hz;
		break;

	default:
		splx(s);
		return (ENOIOCTL);
	}
	splx(s);
	return (0);
}

/*
 * Queue a packet.  Start transmission if not active.
 * Compression happens in slstart; if we do it here, IP TOS
 * will cause us to not compress "background" packets, because
 * ordering gets trashed.  It can be done for all packets in slstart.
 */
static int
sloutput(ifp, m, dst, rtp)
	struct ifnet *ifp;
	register struct mbuf *m;
	struct sockaddr *dst;
	struct rtentry *rtp;
{
	register struct sl_softc *sc = &sl_softc[ifp->if_unit];
	register struct ip *ip;
	register struct ifqueue *ifq;
	int s;

	/*
	 * `Cannot happen' (see slioctl).  Someday we will extend
	 * the line protocol to support other address families.
	 */
	if (dst->sa_family != AF_INET) {
		printf("sl%d: af%d not supported\n", sc->sc_if.if_unit,
			dst->sa_family);
		m_freem(m);
		sc->sc_if.if_noproto++;
		return (EAFNOSUPPORT);
	}

	if (sc->sc_ttyp == NULL || !(ifp->if_flags & IFF_UP)) {
		m_freem(m);
		return (ENETDOWN);
	}
	if ((sc->sc_ttyp->t_state & TS_CONNECTED) == 0) {
		m_freem(m);
		return (EHOSTUNREACH);
	}
	ifq = &sc->sc_if.if_snd;
	ip = mtod(m, struct ip *);
	if (sc->sc_if.if_flags & SC_NOICMP && ip->ip_p == IPPROTO_ICMP) {
		m_freem(m);
		return (ENETRESET);		/* XXX ? */
	}
	if (ip->ip_tos & IPTOS_LOWDELAY)
		ifq = &sc->sc_fastq;
	s = splimp();
	if (IF_QFULL(ifq)) {
		IF_DROP(ifq);
		m_freem(m);
		splx(s);
		sc->sc_if.if_oerrors++;
		return (ENOBUFS);
	}
	IF_ENQUEUE(ifq, m);
	if (sc->sc_ttyp->t_outq.c_cc == 0)
		slstart(sc->sc_ttyp);
	splx(s);
	return (0);
}

/*
 * Start output on interface.  Get another datagram
 * to send from the interface queue and map it to
 * the interface before starting output.
 */
static int
slstart(tp)
	register struct tty *tp;
{
	register struct sl_softc *sc = (struct sl_softc *)tp->t_sc;
	register struct mbuf *m;
	register u_char *cp;
	register struct ip *ip;
	int s;
	struct mbuf *m2;
#if NBPFILTER > 0
	u_char bpfbuf[SLTMAX + SLIP_HDRLEN];
	register int len = 0;
#endif

	for (;;) {
		/*
		 * Call output process whether or not there is more in the
		 * output queue.  We are being called in lieu of ttstart
		 * and must do what it would.
		 */
		(*tp->t_oproc)(tp);

		if (tp->t_outq.c_cc != 0) {
			if (sc != NULL)
				sc->sc_flags &= ~SC_OUTWAIT;
			if (tp->t_outq.c_cc > SLIP_HIWAT)
				return 0;
		}

		/*
		 * This happens briefly when the line shuts down.
		 */
		if (sc == NULL)
			return 0;

		/*
		 * Get a packet and send it to the interface.
		 */
		s = splimp();
		IF_DEQUEUE(&sc->sc_fastq, m);
		if (m)
			sc->sc_if.if_omcasts++;		/* XXX */
		else
			IF_DEQUEUE(&sc->sc_if.if_snd, m);
		splx(s);
		if (m == NULL)
			return 0;

		/*
		 * We do the header compression here rather than in sloutput
		 * because the packets will be out of order if we are using TOS
		 * queueing, and the connection id compression will get
		 * munged when this happens.
		 */
#if NBPFILTER > 0
		if (sc->sc_if.if_bpf) {
			/*
			 * We need to save the TCP/IP header before it's
			 * compressed.  To avoid complicated code, we just
			 * copy the entire packet into a stack buffer (since
			 * this is a serial line, packets should be short
			 * and/or the copy should be negligible cost compared
			 * to the packet transmission time).
			 */
			register struct mbuf *m1 = m;
			register u_char *cp = bpfbuf + SLIP_HDRLEN;

			len = 0;
			do {
				register int mlen = m1->m_len;

				bcopy(mtod(m1, caddr_t), cp, mlen);
				cp += mlen;
				len += mlen;
			} while (m1 = m1->m_next);
		}
#endif
		ip = mtod(m, struct ip *);
		if (ip->ip_v == IPVERSION && ip->ip_p == IPPROTO_TCP) {
			if (sc->sc_if.if_flags & SC_COMPRESS)
				*mtod(m, u_char *) |= sl_compress_tcp(m, ip,
				    &sc->sc_comp, 1);
		}
#if NBPFILTER > 0
		if (sc->sc_if.if_bpf) {
			/*
			 * Put the SLIP pseudo-"link header" in place.  The
			 * compressed header is now at the beginning of the
			 * mbuf.
			 */
			bpfbuf[SLX_DIR] = SLIPDIR_OUT;
			bcopy(mtod(m, caddr_t), &bpfbuf[SLX_CHDR], CHDR_LEN);
			bpf_tap(&sc->sc_if, bpfbuf, len + SLIP_HDRLEN);
		}
#endif

		/*
		 * If system is getting low on clists, just flush our
		 * output queue (if the stuff was important, it'll get
		 * retransmitted). Note that SLTMAX is used instead of
		 * the current if_mtu setting because connections that
		 * have already been established still use the original
		 * (possibly larger) mss.
		 */
		if (cfreecount < CLISTRESERVE + SLTMAX) {
			m_freem(m);
			sc->sc_if.if_collisions++;
			continue;
		}

		sc->sc_flags &= ~SC_OUTWAIT;
		/*
		 * The extra FRAME_END will start up a new packet, and thus
		 * will flush any accumulated garbage.  We do this whenever
		 * the line may have been idle for some time.
		 */
		if (tp->t_outq.c_cc == 0) {
			++sc->sc_if.if_obytes;
			(void) putc(FRAME_END, &tp->t_outq);
		}

		while (m) {
			register u_char *ep;

			cp = mtod(m, u_char *); ep = cp + m->m_len;
			while (cp < ep) {
				/*
				 * Find out how many bytes in the string we can
				 * handle without doing something special.
				 */
				register u_char *bp = cp;

				while (cp < ep) {
					switch (*cp++) {
					case FRAME_ESCAPE:
					case FRAME_END:
						--cp;
						goto out;
					}
				}
				out:
				if (cp > bp) {
					/*
					 * Put n characters at once
					 * into the tty output queue.
					 */
					if (b_to_q((char *)bp, cp - bp,
					    &tp->t_outq))
						break;
					sc->sc_if.if_obytes += cp - bp;
				}
				/*
				 * If there are characters left in the mbuf,
				 * the first one must be special..
				 * Put it out in a different form.
				 */
				if (cp < ep) {
					if (putc(FRAME_ESCAPE, &tp->t_outq))
						break;
					if (putc(*cp++ == FRAME_ESCAPE ?
					   TRANS_FRAME_ESCAPE : TRANS_FRAME_END,
					   &tp->t_outq)) {
						(void) unputc(&tp->t_outq);
						break;
					}
					sc->sc_if.if_obytes += 2;
				}
			}
			MFREE(m, m2);
			m = m2;
		}

		if (putc(FRAME_END, &tp->t_outq)) {
			/*
			 * Not enough room.  Remove a char to make room
			 * and end the packet normally.
			 * If you get many collisions (more than one or two
			 * a day) you probably do not have enough clists
			 * and you should increase "nclist" in param.c.
			 */
			(void) unputc(&tp->t_outq);
			(void) putc(FRAME_END, &tp->t_outq);
			sc->sc_if.if_collisions++;
		} else {
			++sc->sc_if.if_obytes;
			sc->sc_if.if_opackets++;
		}
	}
	return 0;
}

/*
 * Copy data buffer to mbuf chain; add ifnet pointer.
 */
static struct mbuf *
sl_btom(sc, len)
	register struct sl_softc *sc;
	register int len;
{
	register struct mbuf *m;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (NULL);

	/*
	 * If we have more than MHLEN bytes, it's cheaper to
	 * queue the cluster we just filled & allocate a new one
	 * for the input buffer.  Otherwise, fill the mbuf we
	 * allocated above.  Note that code in the input routine
	 * guarantees that packet will fit in a cluster.
	 */
	if (len >= MHLEN) {
		MCLGET(m, M_DONTWAIT);
		if ((m->m_flags & M_EXT) == 0) {
			/*
			 * we couldn't get a cluster - if memory's this
			 * low, it's time to start dropping packets.
			 */
			(void) m_free(m);
			return (NULL);
		}
		sc->sc_ep = mtod(m, u_char *) + SLBUFSIZE;
		m->m_data = (caddr_t)sc->sc_buf;
		m->m_ext.ext_buf = (caddr_t)((int)sc->sc_buf &~ MCLOFSET);
	} else
		bcopy((caddr_t)sc->sc_buf, mtod(m, caddr_t), len);

	m->m_len = len;
	m->m_pkthdr.len = len;
	m->m_pkthdr.rcvif = &sc->sc_if;
	return (m);
}

/*
 * tty interface receiver interrupt.
 */
static int
slinput(c, tp)
	register int c;
	register struct tty *tp;
{
	register struct sl_softc *sc;
	register struct mbuf *m;
	register int len;
	int s;
#if NBPFILTER > 0
	u_char chdr[CHDR_LEN];
#endif

	tk_nin++;
	sc = (struct sl_softc *)tp->t_sc;
	if (sc == NULL)
		return 0;
	if (c & TTY_ERRORMASK || (tp->t_state & TS_CONNECTED) == 0) {
		sc->sc_flags |= SC_ERROR;
		return 0;
	}
	c &= TTY_CHARMASK;

	++sc->sc_if.if_ibytes;

	if (sc->sc_if.if_flags & IFF_DEBUG) {
		if (c == ABT_ESC) {
			/*
			 * If we have a previous abort, see whether
			 * this one is within the time limit.
			 */
			if (sc->sc_abortcount &&
			    time_second >= sc->sc_starttime + ABT_WINDOW)
				sc->sc_abortcount = 0;
			/*
			 * If we see an abort after "idle" time, count it;
			 * record when the first abort escape arrived.
			 */
			if (time_second >= sc->sc_lasttime + ABT_IDLE) {
				if (++sc->sc_abortcount == 1)
					sc->sc_starttime = time_second;
				if (sc->sc_abortcount >= ABT_COUNT) {
					slclose(tp,0);
					return 0;
				}
			}
		} else
			sc->sc_abortcount = 0;
		sc->sc_lasttime = time_second;
	}

	switch (c) {

	case TRANS_FRAME_ESCAPE:
		if (sc->sc_escape)
			c = FRAME_ESCAPE;
		break;

	case TRANS_FRAME_END:
		if (sc->sc_escape)
			c = FRAME_END;
		break;

	case FRAME_ESCAPE:
		sc->sc_escape = 1;
		return 0;

	case FRAME_END:
		sc->sc_flags &= ~SC_KEEPALIVE;
		if(sc->sc_flags & SC_ERROR) {
			sc->sc_flags &= ~SC_ERROR;
			goto newpack;
		}
		len = sc->sc_mp - sc->sc_buf;
		if (len < 3)
			/* less than min length packet - ignore */
			goto newpack;

#if NBPFILTER > 0
		if (sc->sc_if.if_bpf) {
			/*
			 * Save the compressed header, so we
			 * can tack it on later.  Note that we
			 * will end up copying garbage in some
			 * cases but this is okay.  We remember
			 * where the buffer started so we can
			 * compute the new header length.
			 */
			bcopy(sc->sc_buf, chdr, CHDR_LEN);
		}
#endif

		if ((c = (*sc->sc_buf & 0xf0)) != (IPVERSION << 4)) {
			if (c & 0x80)
				c = TYPE_COMPRESSED_TCP;
			else if (c == TYPE_UNCOMPRESSED_TCP)
				*sc->sc_buf &= 0x4f; /* XXX */
			/*
			 * We've got something that's not an IP packet.
			 * If compression is enabled, try to decompress it.
			 * Otherwise, if `auto-enable' compression is on and
			 * it's a reasonable packet, decompress it and then
			 * enable compression.  Otherwise, drop it.
			 */
			if (sc->sc_if.if_flags & SC_COMPRESS) {
				len = sl_uncompress_tcp(&sc->sc_buf, len,
							(u_int)c, &sc->sc_comp);
				if (len <= 0)
					goto error;
			} else if ((sc->sc_if.if_flags & SC_AUTOCOMP) &&
			    c == TYPE_UNCOMPRESSED_TCP && len >= 40) {
				len = sl_uncompress_tcp(&sc->sc_buf, len,
							(u_int)c, &sc->sc_comp);
				if (len <= 0)
					goto error;
				sc->sc_if.if_flags |= SC_COMPRESS;
			} else
				goto error;
		}
#if NBPFILTER > 0
		if (sc->sc_if.if_bpf) {
			/*
			 * Put the SLIP pseudo-"link header" in place.
			 * We couldn't do this any earlier since
			 * decompression probably moved the buffer
			 * pointer.  Then, invoke BPF.
			 */
			register u_char *hp = sc->sc_buf - SLIP_HDRLEN;

			hp[SLX_DIR] = SLIPDIR_IN;
			bcopy(chdr, &hp[SLX_CHDR], CHDR_LEN);
			bpf_tap(&sc->sc_if, hp, len + SLIP_HDRLEN);
		}
#endif
		m = sl_btom(sc, len);
		if (m == NULL)
			goto error;

		sc->sc_if.if_ipackets++;

		if ((sc->sc_if.if_flags & IFF_UP) == 0) {
			m_freem(m);
			goto newpack;
		}

		s = splimp();
		if (IF_QFULL(&ipintrq)) {
			IF_DROP(&ipintrq);
			sc->sc_if.if_ierrors++;
			sc->sc_if.if_iqdrops++;
			m_freem(m);
		} else {
			IF_ENQUEUE(&ipintrq, m);
			schednetisr(NETISR_IP);
		}
		splx(s);
		goto newpack;
	}
	if (sc->sc_mp < sc->sc_ep) {
		*sc->sc_mp++ = c;
		sc->sc_escape = 0;
		return 0;
	}

	/* can't put lower; would miss an extra frame */
	sc->sc_flags |= SC_ERROR;

error:
	sc->sc_if.if_ierrors++;
newpack:
	sc->sc_mp = sc->sc_buf = sc->sc_ep - SLRMAX;
	sc->sc_escape = 0;
	return 0;
}

/*
 * Process an ioctl request.
 */
static int
slioctl(ifp, cmd, data)
	register struct ifnet *ifp;
	int cmd;
	caddr_t data;
{
	register struct ifaddr *ifa = (struct ifaddr *)data;
	register struct ifreq *ifr = (struct ifreq *)data;
	register int s, error = 0;

	s = splimp();

	switch (cmd) {

	case SIOCSIFFLAGS:
		/*
		 * if.c will set the interface up even if we
		 * don't want it to.
		 */
		if (sl_softc[ifp->if_unit].sc_ttyp == NULL) {
			ifp->if_flags &= ~IFF_UP;
		}
		break;
	case SIOCSIFADDR:
		/*
		 * This is "historical" - set the interface up when
		 * setting the address.
		 */
		if (ifa->ifa_addr->sa_family == AF_INET) {
			if (sl_softc[ifp->if_unit].sc_ttyp != NULL)
				if_up(ifp);
		} else {
			error = EAFNOSUPPORT;
		}
		break;

	case SIOCSIFDSTADDR:
		if (ifa->ifa_addr->sa_family != AF_INET)
			error = EAFNOSUPPORT;
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		break;

	case SIOCSIFMTU:
		/*
		 * Set the interface MTU.
		 */
		if (ifr->ifr_mtu > SLTMAX)
			error = EINVAL;
		else {
			struct tty *tp;

			ifp->if_mtu = ifr->ifr_mtu;
			tp = sl_softc[ifp->if_unit].sc_ttyp;
			if (tp != NULL)
				clist_alloc_cblocks(&tp->t_outq,
				    SLIP_HIWAT + 2 * ifp->if_mtu + 1,
				    SLIP_HIWAT + 2 * ifp->if_mtu + 1);
		}
		break;

	default:
		error = EINVAL;
	}
	splx(s);
	return (error);
}

static void
sl_keepalive(chan)
	void *chan;
{
	struct sl_softc *sc = chan;

	if (sc->sc_keepalive) {
		if (sc->sc_flags & SC_KEEPALIVE)
			pgsignal (sc->sc_ttyp->t_pgrp, SIGURG, 1);
		else
			sc->sc_flags |= SC_KEEPALIVE;
		sc->sc_kahandle = timeout(sl_keepalive, sc, sc->sc_keepalive);
	} else {
		sc->sc_flags &= ~SC_KEEPALIVE;
	}
}

static void
sl_outfill(chan)
	void *chan;
{
	struct sl_softc *sc = chan;
	register struct tty *tp = sc->sc_ttyp;
	int s;

	if (sc->sc_outfill && tp != NULL) {
		if (sc->sc_flags & SC_OUTWAIT) {
			s = splimp ();
			++sc->sc_if.if_obytes;
			(void) putc(FRAME_END, &tp->t_outq);
			(*tp->t_oproc)(tp);
			splx (s);
		} else
			sc->sc_flags |= SC_OUTWAIT;
		sc->sc_ofhandle = timeout(sl_outfill, sc, sc->sc_outfill);
	} else {
		sc->sc_flags &= ~SC_OUTWAIT;
	}
}

#endif
