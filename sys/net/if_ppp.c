/*
 * if_ppp.c - Point-to-Point Protocol (PPP) Asynchronous driver.
 *
 * Copyright (c) 1989 Carnegie Mellon University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by Carnegie Mellon University.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Drew D. Perkins
 * Carnegie Mellon University
 * 4910 Forbes Ave.
 * Pittsburgh, PA 15213
 * (412) 268-8576
 * ddp@andrew.cmu.edu
 *
 * Based on:
 *	@(#)if_sl.c	7.6.1.2 (Berkeley) 2/15/89
 *
 * Copyright (c) 1987 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
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
 * Converted to 4.3BSD Beta by Chris Torek.
 * Other changes made at Berkeley, based in part on code by Kirk Smith.
 *
 * Converted to 4.3BSD+ 386BSD by Brad Parker (brad@cayman.com)
 * Added VJ tcp header compression; more unified ioctls
 *
 * Extensively modified by Paul Mackerras (paulus@cs.anu.edu.au).
 * Cleaned up a lot of the mbuf-related code to fix bugs that
 * caused system crashes and packet corruption.  Changed pppstart
 * so that it doesn't just give up with a collision if the whole
 * packet doesn't fit in the output ring buffer.
 *
 * Added priority queueing for interactive IP packets, following
 * the model of if_sl.c, plus hooks for bpf.
 * Paul Mackerras (paulus@cs.anu.edu.au).
 */

/* $Id: if_ppp.c,v 1.14.2.4 1996/06/25 04:28:54 gpalmer Exp $ */
/* from if_sl.c,v 1.11 84/10/04 12:54:47 rick Exp */

#include "ppp.h"
#if NPPP > 0

#define VJC

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/mbuf.h>
#include <sys/buf.h>
#include <sys/dkstat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/tty.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/vnode.h>
#include <sys/kernel.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/route.h>

#if INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#endif

#include "bpfilter.h"
#if NBPFILTER > 0
#include <sys/time.h>
#include <net/bpf.h>
#endif

#ifdef VJC
#include <net/pppcompress.h>
#define HDROFF	MAX_HDR
/* HDROFF should really be 128, but other parts of the system will
   panic on TCP+IP headers bigger than MAX_HDR = MHLEN (100). */

#else
#define	HDROFF	(0)
#endif

#include <net/if_ppp.h>
#include <machine/cpu.h>

/* This is a FreeBSD-2.x kernel. */
#define CCOUNT(q)	((q)->c_cc)

#define	PPP_HIWAT	400	/* Don't start a new packet if HIWAT on que */
#define	PPP_MAXMTU	16384	/* Largest MTU we allow */

struct ppp_softc ppp_softc[NPPP];

void	pppattach __P((void));
int	pppopen __P((dev_t dev, struct tty *tp));
int	pppclose __P((struct tty *tp, int flag));
int	pppread __P((struct tty *tp, struct uio *uio, int flag));
int	pppwrite __P((struct tty *tp, struct uio *uio, int flag));
int	ppptioctl __P((struct tty *tp, int cmd, caddr_t data, int flag,
		       struct proc *));
int	pppoutput __P((struct ifnet *, struct mbuf *,
		       struct sockaddr *, struct rtentry *));
int	pppinput __P((int c, struct tty *tp));
int	pppioctl __P((struct ifnet *ifp, int cmd, caddr_t data));
int	pppstart __P((struct tty *tp));

static struct linesw pppdisc = {
	pppopen, pppclose, pppread, pppwrite, ppptioctl,
	pppinput, pppstart, ttymodem
};

extern struct	ppp_softc *pppalloc __P((pid_t pid));
extern void	pppdealloc __P((struct ppp_softc *sc));
extern struct	mbuf *ppp_dequeue __P((struct ppp_softc *sc));
extern int	ppppktin __P((struct ppp_softc *sc, struct mbuf *m, int ilen));

static int	pppasyncstart __P((struct ppp_softc *));
static u_short	pppfcs __P((u_short fcs, u_char *cp, int len));
static int	pppgetm __P((struct ppp_softc *sc));
static struct	mbuf *ppp_btom __P((struct ppp_softc *sc));
static void	pppdumpm __P((struct mbuf *m0, int pktlen));
static void	pppdumpb __P((u_char *b, int l));
static void	ppplogchar __P((struct ppp_softc *, int));

/*
 * Some useful mbuf macros not in mbuf.h.
 */
#define M_DATASTART(m)	\
	((m)->m_flags & M_EXT ? (m)->m_ext.ext_buf : \
	    (m)->m_flags & M_PKTHDR ? (m)->m_pktdat : (m)->m_dat)

#define M_DATASIZE(m)	\
	((m)->m_flags & M_EXT ? (m)->m_ext.ext_size : \
	    (m)->m_flags & M_PKTHDR ? MHLEN: MLEN)

/*
 * The following disgusting hack gets around the problem that IP TOS
 * can't be set yet.  We want to put "interactive" traffic on a high
 * priority queue.  To decide if traffic is interactive, we check that
 * a) it is TCP and b) one of its ports is telnet, rlogin or ftp control.
 */
static u_short interactive_ports[8] = {
	0,	513,	0,	0,
	0,	21,	0,	23,
};
#define INTERACTIVE(p) (interactive_ports[(p) & 7] == (p))

/*
 * Does c need to be escaped?
 */
#define ESCAPE_P(c)	(sc->sc_asyncmap[(c) >> 5] & (1 << ((c) & 0x1F)))

/*
 * Called from boot code to establish ppp interfaces.
 */
void
pppattach()
{
    register struct ppp_softc *sc;
    register int i = 0;

    linesw[PPPDISC] = pppdisc;

    for (sc = ppp_softc; i < NPPP; sc++) {
	sc->sc_if.if_name = "ppp";
	sc->sc_if.if_unit = i++;
	sc->sc_if.if_mtu = PPP_MTU;
	sc->sc_if.if_flags = IFF_POINTOPOINT | IFF_MULTICAST;
	sc->sc_if.if_type = IFT_PPP;
	sc->sc_if.if_hdrlen = PPP_HDRLEN;
	sc->sc_if.if_ioctl = pppioctl;
	sc->sc_if.if_output = pppoutput;
	sc->sc_if.if_snd.ifq_maxlen = IFQ_MAXLEN;
	sc->sc_inq.ifq_maxlen = IFQ_MAXLEN;
	sc->sc_fastq.ifq_maxlen = IFQ_MAXLEN;
	if_attach(&sc->sc_if);
#if NBPFILTER > 0
	bpfattach(&sc->sc_bpf, &sc->sc_if, DLT_PPP, PPP_HDRLEN);
#endif
    }
}

/*
 * Allocate a ppp interface unit and initialize it.
 */
struct ppp_softc *
pppalloc(pid)
    pid_t pid;
{
    int nppp;
    struct ppp_softc *sc;

    for (nppp = 0, sc = ppp_softc; nppp < NPPP; nppp++, sc++)
	if (sc->sc_xfer == pid) {
	    sc->sc_xfer = 0;
	    break;
	}
    if (nppp >= NPPP)
	for (nppp = 0, sc = ppp_softc; nppp < NPPP; nppp++, sc++)
	    if (sc->sc_devp == NULL)
		break;
    if (nppp >= NPPP)
	return NULL;

    sc->sc_flags = 0;
    sc->sc_mru = PPP_MRU;
#ifdef VJC
    sl_compress_init(&sc->sc_comp);
#endif
    sc->sc_if.if_flags |= IFF_RUNNING;
    microtime(&sc->sc_if.if_lastchange);

    return sc;
}

/*
 * Deallocate a ppp unit.
 */
void
pppdealloc(sc)
	struct ppp_softc *sc;
{
    struct mbuf *m;

    if_down(&sc->sc_if);
    sc->sc_devp = NULL;
    sc->sc_xfer = 0;
    for (;;) {
	IF_DEQUEUE(&sc->sc_inq, m);
	if (m == NULL)
	    break;
	m_freem(m);
    }
    for (;;) {
	IF_DEQUEUE(&sc->sc_fastq, m);
	if (m == NULL)
	    break;
	m_freem(m);
    }
    sc->sc_if.if_flags &= ~(IFF_UP|IFF_RUNNING);
    microtime(&sc->sc_if.if_lastchange);
}

/*
 * Line specific open routine for async tty devices.
 * Attach the given tty to the first available ppp unit.
 */
/* ARGSUSED */
int
pppopen(dev, tp)
    dev_t dev;
    register struct tty *tp;
{
    struct proc *p = curproc;		/* XXX */
    register struct ppp_softc *sc;
    int error, s, i;

    if (error = suser(p->p_ucred, &p->p_acflag))
	return (error);

    if (tp->t_line == PPPDISC) {
	sc = (struct ppp_softc *) tp->t_sc;
	if (sc != NULL && sc->sc_devp == (void *) tp)
	    return (0);
    }

    if ((sc = pppalloc(p->p_pid)) == NULL)
	return ENXIO;

    if (sc->sc_outm != NULL) {
	m_freem(sc->sc_outm);
	sc->sc_outm = NULL;
    }

    if (pppgetm(sc) == 0) {
	sc->sc_if.if_flags &= ~(IFF_UP|IFF_RUNNING);
	microtime(&sc->sc_if.if_lastchange);
	return (ENOBUFS);
    }

    sc->sc_ilen = 0;
    bzero(sc->sc_asyncmap, sizeof(sc->sc_asyncmap));
    sc->sc_asyncmap[0] = 0xffffffff;
    sc->sc_asyncmap[3] = 0x60000000;
    sc->sc_rasyncmap = 0;
    sc->sc_devp = (void *) tp;
    sc->sc_start = pppasyncstart;

    tp->t_sc = (caddr_t) sc;
    ttyflush(tp, FREAD | FWRITE);
    /*
     * XXX we fudge t_canq to avoid providing pppselect() and FIONREAD.
     * I hope one char is enough.  The following actually gives CBSIZE
     * chars.
     */
    clist_alloc_cblocks(&tp->t_canq, 1, 1);
    clist_alloc_cblocks(&tp->t_outq, sc->sc_if.if_mtu + PPP_HIWAT,
			sc->sc_if.if_mtu + PPP_HIWAT);
    clist_alloc_cblocks(&tp->t_rawq, 0, 0);

    return (0);
}

/*
 * Line specific close routine.
 * Detach the tty from the ppp unit.
 * Mimics part of ttyclose().
 */
int
pppclose(tp, flag)
    struct tty *tp;
    int flag;
{
    register struct ppp_softc *sc;
    struct mbuf *m;
    int s;

    ttyflush(tp, FREAD | FWRITE);
    s = splimp();		/* paranoid; splnet probably ok */
    clist_free_cblocks(&tp->t_canq);
    clist_free_cblocks(&tp->t_outq);
    tp->t_line = 0;
    sc = (struct ppp_softc *)tp->t_sc;
    if (sc != NULL) {
	tp->t_sc = NULL;
	if (tp == (struct tty *) sc->sc_devp) {
	    m_freem(sc->sc_outm);
	    sc->sc_outm = NULL;
	    m_freem(sc->sc_m);
	    sc->sc_m = NULL;
	    pppdealloc(sc);
	}
    }
    splx(s);
    return (0);			/* success */
}

/*
 * Line specific (tty) read routine.
 */
int
pppread(tp, uio, flag)
    register struct tty *tp;
    struct uio *uio;
    int flag;
{
    register struct ppp_softc *sc = (struct ppp_softc *)tp->t_sc;
    struct mbuf *m, *m0;
    register int s;
    int error = 0;

    if ((tp->t_state & TS_CONNECTED) == 0)
	return 0;		/* end of file */
    if (sc == NULL || tp != (struct tty *) sc->sc_devp)
	return 0;
    s = splimp();
    while (sc->sc_inq.ifq_head == NULL && tp->t_line == PPPDISC) {
	if (tp->t_state & TS_ASYNC || flag & IO_NDELAY) {
	    splx(s);
	    return (EWOULDBLOCK);
	}
	error = ttysleep(tp, TSA_HUP_OR_INPUT(tp), TTIPRI | PCATCH, "pppin", 0);
	if (error)
	    return error;
    }
    if (tp->t_line != PPPDISC) {
	splx(s);
	return (-1);
    }

    /* Pull place-holder byte out of canonical queue */
    getc(&tp->t_canq);

    /* Get the packet from the input queue */
    IF_DEQUEUE(&sc->sc_inq, m0);
    splx(s);

    for (m = m0; m && uio->uio_resid; m = m->m_next)
	if (error = uiomove(mtod(m, u_char *), m->m_len, uio))
	    break;
    m_freem(m0);
    return (error);
}

/*
 * Line specific (tty) write routine.
 */
int
pppwrite(tp, uio, flag)
    register struct tty *tp;
    struct uio *uio;
    int flag;
{
    register struct ppp_softc *sc = (struct ppp_softc *)tp->t_sc;
    struct mbuf *m, *m0, **mp;
    struct sockaddr dst;
    struct ppp_header *ph1, *ph2;
    int len, error;

    if ((tp->t_state & TS_CONNECTED) == 0)
	return 0;		/* wrote 0 bytes */
    if (tp->t_line != PPPDISC)
	return (EINVAL);
    if (sc == NULL || tp != (struct tty *) sc->sc_devp)
	return EIO;
    if (uio->uio_resid > sc->sc_if.if_mtu + PPP_HDRLEN ||
	uio->uio_resid < PPP_HDRLEN)
	return (EMSGSIZE);
    for (mp = &m0; uio->uio_resid; mp = &m->m_next) {
	MGET(m, M_WAIT, MT_DATA);
	if ((*mp = m) == NULL) {
	    m_freem(m0);
	    return (ENOBUFS);
	}
	m->m_len=0;
	if (uio->uio_resid >= MCLBYTES / 2)
	    MCLGET(m, M_DONTWAIT);
	len = M_TRAILINGSPACE(m);
	if (len > uio->uio_resid)
	    len = uio->uio_resid;
	if (error = uiomove(mtod(m, u_char *), len, uio)) {
	    m_freem(m0);
	    return (error);
	}
	m->m_len = len;
    }
    dst.sa_family = AF_UNSPEC;
    ph1 = (struct ppp_header *) &dst.sa_data;
    ph2 = mtod(m0, struct ppp_header *);
    *ph1 = *ph2;
    m0->m_data += PPP_HDRLEN;
    m0->m_len -= PPP_HDRLEN;
    return (pppoutput(&sc->sc_if, m0, &dst, (struct rtentry *)0));
}

/*
 * Line specific (tty) ioctl routine.
 * Provide a way to get the ppp unit number.
 * This discipline requires that tty device drivers call
 * the line specific l_ioctl routine from their ioctl routines.
 */
/* ARGSUSED */
int
ppptioctl(tp, cmd, data, flag, p)
    struct tty *tp;
    caddr_t data;
    int cmd, flag;
    struct proc *p;
{
    register struct ppp_softc *sc = (struct ppp_softc *) tp->t_sc;
    int s, error, flags, mru;

    if (sc == NULL || tp != (struct tty *) sc->sc_devp)
	return -1;

    switch (cmd) {
    case FIONREAD:
	*(int *)data = sc->sc_inq.ifq_len;
	break;

    case PPPIOCGUNIT:
	*(int *)data = sc->sc_if.if_unit;
	break;

    case PPPIOCGFLAGS:
	*(u_int *)data = sc->sc_flags;
	break;

    case PPPIOCSFLAGS:
	if (error = suser(p->p_ucred, &p->p_acflag))
	    return (error);
	flags = *(int *)data & SC_MASK;
	s = splimp();
	sc->sc_flags = (sc->sc_flags & ~SC_MASK) | flags;
	splx(s);
	break;

    case PPPIOCSASYNCMAP:
	if (error = suser(p->p_ucred, &p->p_acflag))
	    return (error);
	sc->sc_asyncmap[0] = *(u_int *)data;
	break;

    case PPPIOCGASYNCMAP:
	*(u_int *)data = sc->sc_asyncmap[0];
	break;

    case PPPIOCSRASYNCMAP:
	if (error = suser(p->p_ucred, &p->p_acflag))
	    return (error);
	sc->sc_rasyncmap = *(u_int *)data;
	break;

    case PPPIOCGRASYNCMAP:
	*(u_int *)data = sc->sc_rasyncmap;
	break;

    case PPPIOCSXASYNCMAP:
	if (error = suser(p->p_ucred, &p->p_acflag))
	    return (error);
	bcopy(data, sc->sc_asyncmap, sizeof(sc->sc_asyncmap));
	sc->sc_asyncmap[1] = 0;		    /* mustn't escape 0x20 - 0x3f */
	sc->sc_asyncmap[2] &= ~0x40000000;  /* mustn't escape 0x5e */
	sc->sc_asyncmap[3] |= 0x60000000;   /* must escape 0x7d, 0x7e */
	break;

    case PPPIOCGXASYNCMAP:
	bcopy(sc->sc_asyncmap, data, sizeof(sc->sc_asyncmap));
	break;

    case PPPIOCSMRU:
	if (error = suser(p->p_ucred, &p->p_acflag))
	    return (error);
	mru = *(int *)data;
	if (mru >= PPP_MRU && mru <= PPP_MAXMRU) {
	    sc->sc_mru = mru;
	    if (pppgetm(sc) == 0) {
		error = ENOBUFS;
		sc->sc_mru = PPP_MRU;
		if (pppgetm(sc) == 0)
		{
		    sc->sc_if.if_flags &= ~IFF_UP;
		    microtime(&sc->sc_if.if_lastchange);
		}
	    }
	}
	break;

    case PPPIOCGMRU:
	*(int *)data = sc->sc_mru;
	break;

#ifdef VJC
    case PPPIOCSMAXCID:
	if (error = suser(p->p_ucred, &p->p_acflag))
	    return (error);
	sl_compress_setup(&sc->sc_comp, *(int *)data);
	break;
#endif

    case PPPIOCXFERUNIT:
	if (error = suser(p->p_ucred, &p->p_acflag))
	    return (error);
	sc->sc_xfer = p->p_pid;
	break;

    default:
	return (-1);
    }
    return (0);
}

/*
 * FCS lookup table as calculated by genfcstab.
 */
static u_short fcstab[256] = {
	0x0000,	0x1189,	0x2312,	0x329b,	0x4624,	0x57ad,	0x6536,	0x74bf,
	0x8c48,	0x9dc1,	0xaf5a,	0xbed3,	0xca6c,	0xdbe5,	0xe97e,	0xf8f7,
	0x1081,	0x0108,	0x3393,	0x221a,	0x56a5,	0x472c,	0x75b7,	0x643e,
	0x9cc9,	0x8d40,	0xbfdb,	0xae52,	0xdaed,	0xcb64,	0xf9ff,	0xe876,
	0x2102,	0x308b,	0x0210,	0x1399,	0x6726,	0x76af,	0x4434,	0x55bd,
	0xad4a,	0xbcc3,	0x8e58,	0x9fd1,	0xeb6e,	0xfae7,	0xc87c,	0xd9f5,
	0x3183,	0x200a,	0x1291,	0x0318,	0x77a7,	0x662e,	0x54b5,	0x453c,
	0xbdcb,	0xac42,	0x9ed9,	0x8f50,	0xfbef,	0xea66,	0xd8fd,	0xc974,
	0x4204,	0x538d,	0x6116,	0x709f,	0x0420,	0x15a9,	0x2732,	0x36bb,
	0xce4c,	0xdfc5,	0xed5e,	0xfcd7,	0x8868,	0x99e1,	0xab7a,	0xbaf3,
	0x5285,	0x430c,	0x7197,	0x601e,	0x14a1,	0x0528,	0x37b3,	0x263a,
	0xdecd,	0xcf44,	0xfddf,	0xec56,	0x98e9,	0x8960,	0xbbfb,	0xaa72,
	0x6306,	0x728f,	0x4014,	0x519d,	0x2522,	0x34ab,	0x0630,	0x17b9,
	0xef4e,	0xfec7,	0xcc5c,	0xddd5,	0xa96a,	0xb8e3,	0x8a78,	0x9bf1,
	0x7387,	0x620e,	0x5095,	0x411c,	0x35a3,	0x242a,	0x16b1,	0x0738,
	0xffcf,	0xee46,	0xdcdd,	0xcd54,	0xb9eb,	0xa862,	0x9af9,	0x8b70,
	0x8408,	0x9581,	0xa71a,	0xb693,	0xc22c,	0xd3a5,	0xe13e,	0xf0b7,
	0x0840,	0x19c9,	0x2b52,	0x3adb,	0x4e64,	0x5fed,	0x6d76,	0x7cff,
	0x9489,	0x8500,	0xb79b,	0xa612,	0xd2ad,	0xc324,	0xf1bf,	0xe036,
	0x18c1,	0x0948,	0x3bd3,	0x2a5a,	0x5ee5,	0x4f6c,	0x7df7,	0x6c7e,
	0xa50a,	0xb483,	0x8618,	0x9791,	0xe32e,	0xf2a7,	0xc03c,	0xd1b5,
	0x2942,	0x38cb,	0x0a50,	0x1bd9,	0x6f66,	0x7eef,	0x4c74,	0x5dfd,
	0xb58b,	0xa402,	0x9699,	0x8710,	0xf3af,	0xe226,	0xd0bd,	0xc134,
	0x39c3,	0x284a,	0x1ad1,	0x0b58,	0x7fe7,	0x6e6e,	0x5cf5,	0x4d7c,
	0xc60c,	0xd785,	0xe51e,	0xf497,	0x8028,	0x91a1,	0xa33a,	0xb2b3,
	0x4a44,	0x5bcd,	0x6956,	0x78df,	0x0c60,	0x1de9,	0x2f72,	0x3efb,
	0xd68d,	0xc704,	0xf59f,	0xe416,	0x90a9,	0x8120,	0xb3bb,	0xa232,
	0x5ac5,	0x4b4c,	0x79d7,	0x685e,	0x1ce1,	0x0d68,	0x3ff3,	0x2e7a,
	0xe70e,	0xf687,	0xc41c,	0xd595,	0xa12a,	0xb0a3,	0x8238,	0x93b1,
	0x6b46,	0x7acf,	0x4854,	0x59dd,	0x2d62,	0x3ceb,	0x0e70,	0x1ff9,
	0xf78f,	0xe606,	0xd49d,	0xc514,	0xb1ab,	0xa022,	0x92b9,	0x8330,
	0x7bc7,	0x6a4e,	0x58d5,	0x495c,	0x3de3,	0x2c6a,	0x1ef1,	0x0f78
};

/*
 * Calculate a new FCS given the current FCS and the new data.
 */
static u_short
pppfcs(fcs, cp, len)
    register u_short fcs;
    register u_char *cp;
    register int len;
{
    while (len--)
	fcs = PPP_FCS(fcs, *cp++);
    return (fcs);
}

/*
 * Queue a packet.  Start transmission if not active.
 * Packet is placed in Information field of PPP frame.
 */
int
pppoutput(ifp, m0, dst, rt)
	struct ifnet *ifp;
	struct mbuf *m0;
	struct sockaddr *dst;
	struct rtentry *rt;
{
    register struct ppp_softc *sc = &ppp_softc[ifp->if_unit];
    struct ppp_header *ph;
    int protocol, address, control;
    u_char *cp;
    int s, error;
    struct ip *ip;
    struct ifqueue *ifq;

    if (sc->sc_devp == NULL || (ifp->if_flags & IFF_RUNNING) == 0
	|| (ifp->if_flags & IFF_UP) == 0 && dst->sa_family != AF_UNSPEC) {
	error = ENETDOWN;	/* sort of */
	goto bad;
    }

    /*
     * Compute PPP header.
     */
    address = PPP_ALLSTATIONS;
    control = PPP_UI;
    ifq = &ifp->if_snd;
    switch (dst->sa_family) {
#ifdef INET
    case AF_INET:
	protocol = PPP_IP;
	if ((sc->sc_flags & SC_ENABLE_IP) == 0) {
	    error = ENETDOWN;
	    goto bad;
	}

	/*
	 * If this is a TCP packet to or from an "interactive" port,
	 * put the packet on the fastq instead.
	 */
	if ((ip = mtod(m0, struct ip *))->ip_p == IPPROTO_TCP) {
	    register int p = ntohl(((int *)ip)[ip->ip_hl]);
	    if (INTERACTIVE(p & 0xffff) || INTERACTIVE(p >> 16))
		ifq = &sc->sc_fastq;
	}

      /*
       * Most high-priority services set IPTOS_LOWDELAY.  This is a much more
       * generic mechanism than specific port numbers.  This same mechanism
       * is used by slip.
       */
      if (ip->ip_tos & IPTOS_LOWDELAY)
          ifq = &sc->sc_fastq;

	break;
#endif
#ifdef NS
    case AF_NS:
	protocol = PPP_XNS;
	break;
#endif
    case AF_UNSPEC:
	ph = (struct ppp_header *) dst->sa_data;
	address = ph->ph_address;
	control = ph->ph_control;
	protocol = ntohs(ph->ph_protocol);
	break;
    default:
	printf("ppp%d: af%d not supported\n", ifp->if_unit, dst->sa_family);
	error = EAFNOSUPPORT;
	goto bad;
    }

    /*
     * Add PPP header.  If no space in first mbuf, allocate another.
     * (This assumes M_LEADINGSPACE is always 0 for a cluster mbuf.)
     */
    if (M_LEADINGSPACE(m0) < PPP_HDRLEN) {
	m0 = m_prepend(m0, PPP_HDRLEN, M_DONTWAIT);
	if (m0 == 0) {
	    error = ENOBUFS;
	    goto bad;
	}
	m0->m_len = 0;
    } else
	m0->m_data -= PPP_HDRLEN;

    cp = mtod(m0, u_char *);
    *cp++ = address;
    *cp++ = control;
    *cp++ = protocol >> 8;
    *cp++ = protocol & 0xff;
    m0->m_len += PPP_HDRLEN;

    if (sc->sc_flags & SC_LOG_OUTPKT) {
	printf("ppp%d output: ", ifp->if_unit);
	pppdumpm(m0, -1);
    }

#if NBPFILTER > 0
    /* See if bpf wants to look at the packet. */
    if (sc->sc_bpf)
	bpf_mtap(sc->sc_bpf, m0);
#endif

    /*
     * Put the packet on the appropriate queue.
     */
    s = splimp();
    if (IF_QFULL(ifq)) {
	IF_DROP(ifq);
	splx(s);
	sc->sc_if.if_oerrors++;
	error = ENOBUFS;
	goto bad;
    }
    IF_ENQUEUE(ifq, m0);

    /*
     * Tell the device to send it out.
     */
    (*sc->sc_start)(sc);

    splx(s);
    return (0);

bad:
    m_freem(m0);
    return (error);
}

/*
 * Grab another packet off a queue and apply VJ compression,
 * address/control and/or protocol compression if appropriate.
 */
struct mbuf *
ppp_dequeue(sc)
    struct ppp_softc *sc;
{
    int s;
    struct mbuf *m;
    u_char *cp;
    int address, control, protocol;

    s = splimp();
    IF_DEQUEUE(&sc->sc_fastq, m);
    if (m == NULL)
	IF_DEQUEUE(&sc->sc_if.if_snd, m);
    splx(s);
    if (m == NULL)
	return NULL;

    /*
     * Extract the ppp header of the new packet.
     * The ppp header will be in one mbuf.
     */
    cp = mtod(m, u_char *);
    address = cp[0];
    control = cp[1];
    protocol = (cp[2] << 8) + cp[3];

#ifdef VJC
    /*
     * If the packet is a TCP/IP packet, see if we can compress it.
     */
    if (protocol == PPP_IP && sc->sc_flags & SC_COMP_TCP) {
	struct ip *ip;
	int type;
	struct mbuf *mp;

	mp = m;
	ip = (struct ip *) (cp + PPP_HDRLEN);
	if (mp->m_len <= PPP_HDRLEN) {
	    mp = mp->m_next;
	    ip = mtod(mp, struct ip *);
	}
	/* this code assumes the IP/TCP header is in one non-shared mbuf */
	if (ip->ip_p == IPPROTO_TCP) {
	    type = sl_compress_tcp(mp, ip, &sc->sc_comp,
				   !(sc->sc_flags & SC_NO_TCP_CCID));
	    switch (type) {
	    case TYPE_UNCOMPRESSED_TCP:
		protocol = PPP_VJC_UNCOMP;
		break;
	    case TYPE_COMPRESSED_TCP:
		protocol = PPP_VJC_COMP;
		cp = mtod(m, u_char *);
		cp[0] = address;	/* header has moved */
		cp[1] = control;
		cp[2] = 0;
		break;
	    }
	    cp[3] = protocol;	/* update protocol in PPP header */
	}
    }
#endif	/* VJC */

#ifdef BSD_COMP
    if (protocol < PPP_COMP && (sc->sc_flags & SC_BSD_COMP)) {
	slen = m_totallen(m0);
	clen = pf_bsd_comp(sc->sc_bsd_db, cbuf, proto, m0, slen);
    }
#endif	/* BSD_COMP */

    /*
     * Compress the address/control and protocol, if possible.
     */
    if (sc->sc_flags & SC_COMP_AC && address == PPP_ALLSTATIONS &&
	control == PPP_UI && protocol != PPP_ALLSTATIONS &&
	protocol != PPP_LCP) {
	/* can compress address/control */
	m->m_data += 2;
	m->m_len -= 2;
    }
    if (sc->sc_flags & SC_COMP_PROT && protocol < 0xFF) {
	/* can compress protocol */
	if (mtod(m, u_char *) == cp) {
	    cp[2] = cp[1];	/* move address/control up */
	    cp[1] = cp[0];
	}
	++m->m_data;
	--m->m_len;
    }

    return m;
}

/*
 * This gets called from pppoutput when a new packet is
 * put on a queue.
 */
static int
pppasyncstart(sc)
    register struct ppp_softc *sc;
{
    register struct tty *tp = (struct tty *) sc->sc_devp;

    pppstart(tp);
    return 0;
}

/*
 * Start output on async tty interface.  Get another datagram
 * to send from the interface queue and start sending it.
 */
int
pppstart(tp)
    register struct tty *tp;
{
    register struct ppp_softc *sc = (struct ppp_softc *) tp->t_sc;
    register struct mbuf *m;
    register int len;
    register u_char *start, *stop, *cp;
    int n, s, ndone, done;
    struct mbuf *m2;

    if ((tp->t_state & TS_CONNECTED) == 0) {
	/* sorry, I can't talk now */
	return 0;
    }
    if (sc == NULL || tp != (struct tty *) sc->sc_devp) {
	(*tp->t_oproc)(tp);
	return 0;
    }

    for (;;) {
	/*
	 * Call output process whether or not there is any output.
	 * We are being called in lieu of ttstart and must do what
	 * it would.
	 */
	if (tp->t_oproc != NULL) {
	    (*tp->t_oproc)(tp);
	    if (CCOUNT(&tp->t_outq) > PPP_HIWAT)
		return 0;
	}

	/*
	 * See if we have an existing packet partly sent.
	 * If not, get a new packet and start sending it.
	 * We take packets on the priority queue ahead of those
	 * on the normal queue.
	 */
	m = sc->sc_outm;
	if (m == NULL) {
	    /*
	     * Get another packet to be sent
	     */
	    m = ppp_dequeue(sc);
	    if (m == NULL)
		return 0;

	    /*
	     * The extra PPP_FLAG will start up a new packet, and thus
	     * will flush any accumulated garbage.  We do this whenever
	     * the line may have been idle for some time.
	     */
	    if (CCOUNT(&tp->t_outq) == 0) {
		++sc->sc_bytessent;
		(void) putc(PPP_FLAG, &tp->t_outq);
	    }

	    /* Calculate the FCS for the first mbuf's worth. */
	    sc->sc_outfcs = pppfcs(PPP_INITFCS, mtod(m, u_char *), m->m_len);
	}

	for (;;) {
	    start = mtod(m, u_char *);
	    len = m->m_len;
	    stop = start + len;
	    while (len > 0) {
		/*
		 * Find out how many bytes in the string we can
		 * handle without doing something special.
		 */
		for (cp = start; cp < stop; cp++)
		    if (ESCAPE_P(*cp))
			break;
		n = cp - start;
		if (n) {
		    /* NetBSD (0.9 or later), 4.3-Reno or similar. */
		    ndone = n - b_to_q(start, n, &tp->t_outq);
		    len -= ndone;
		    start += ndone;
		    sc->sc_bytessent += ndone;

		    if (ndone < n)
			break;	/* packet doesn't fit */
		}
		/*
		 * If there are characters left in the mbuf,
		 * the first one must be special..
		 * Put it out in a different form.
		 */
		if (len) {
		    if (putc(PPP_ESCAPE, &tp->t_outq))
			break;
		    if (putc(*start ^ PPP_TRANS, &tp->t_outq)) {
			(void) unputc(&tp->t_outq);
			break;
		    }
		    sc->sc_bytessent += 2;
		    start++;
		    len--;
		}
	    }
	    /*
	     * If we didn't empty this mbuf, remember where we're up to.
	     * If we emptied the last mbuf, try to add the FCS and closing
	     * flag, and if we can't, leave sc_outm pointing to m, but with
	     * m->m_len == 0, to remind us to output the FCS and flag later.
	     */
	    done = len == 0;
	    if (done && m->m_next == NULL) {
		u_char *p, *q;
		int c;
		u_char endseq[8];

		/*
		 * We may have to escape the bytes in the FCS.
		 */
		p = endseq;
		c = ~sc->sc_outfcs & 0xFF;
		if (ESCAPE_P(c)) {
		    *p++ = PPP_ESCAPE;
		    *p++ = c ^ PPP_TRANS;
		} else
		    *p++ = c;
		c = (~sc->sc_outfcs >> 8) & 0xFF;
		if (ESCAPE_P(c)) {
		    *p++ = PPP_ESCAPE;
		    *p++ = c ^ PPP_TRANS;
		} else
		    *p++ = c;
		*p++ = PPP_FLAG;

		/*
		 * Try to output the FCS and flag.  If the bytes
		 * don't all fit, back out.
		 */
		for (q = endseq; q < p; ++q)
		    if (putc(*q, &tp->t_outq)) {
			done = 0;
			for (; q > endseq; --q)
			    unputc(&tp->t_outq);
			break;
		    }
	    }

	    if (!done) {
		m->m_data = start;
		m->m_len = len;
		sc->sc_outm = m;
		if (tp->t_oproc != NULL)
		    (*tp->t_oproc)(tp);
		return 0;	/* can't do any more at the moment */
	    }

	    /* Finished with this mbuf; free it and move on. */
	    MFREE(m, m2);
	    if (m2 == NULL)
		break;

	    m = m2;
	    sc->sc_outfcs = pppfcs(sc->sc_outfcs, mtod(m, u_char *), m->m_len);
	}

	/* Finished a packet */
	sc->sc_outm = NULL;
	sc->sc_bytessent++;	/* account for closing flag */
	sc->sc_if.if_opackets++;
	sc->sc_if.if_obytes = sc->sc_bytessent;
    }
    return 0;
}

/*
 * Allocate enough mbuf to handle current MRU.
 */
static int
pppgetm(sc)
    register struct ppp_softc *sc;
{
    struct mbuf *m, **mp;
    int len = HDROFF + sc->sc_mru + PPP_HDRLEN + PPP_FCSLEN;
    int s;

    s = splimp();
    for (mp = &sc->sc_m; (m = *mp) != NULL; mp = &m->m_next)
	if ((len -= M_DATASIZE(m)) <= 0) {
	    splx(s);
	    return (1);
	}

    for (;; mp = &m->m_next) {
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == 0) {
	    m_freem(sc->sc_m);
	    sc->sc_m = NULL;
	    splx(s);
	    printf("ppp%d: can't allocate mbuf\n", sc->sc_if.if_unit);
	    return (0);
	}
	*mp = m;
	MCLGET(m, M_DONTWAIT);
	if ((len -= M_DATASIZE(m)) <= 0) {
	    splx(s);
	    return (1);
	}
    }
}

/*
 * Copy mbuf chain.  Would like to use m_copy(), but we need a real copy
 * of the data, not just copies of pointers to the data.
 */
static struct mbuf *
ppp_btom(sc)
    struct ppp_softc *sc;
{
    register struct mbuf *m, **mp;
    struct mbuf *top = sc->sc_m;

    /*
     * First check current mbuf.  If we have more than a small mbuf,
     * return the whole cluster and set beginning of buffer to the
     * next mbuf.
     * Else, copy the current bytes into a small mbuf, attach the new
     * mbuf to the end of the chain and set beginning of buffer to the
     * current mbuf.
     */

    if (sc->sc_mc->m_len > MHLEN) {
	sc->sc_m = sc->sc_mc->m_next;
	sc->sc_mc->m_next = NULL;
    }
    else {
	/* rather than waste a whole cluster on <= MHLEN bytes,
	   alloc a small mbuf and copy to it */
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
	    return (NULL);

	bcopy(mtod(sc->sc_mc, caddr_t), mtod(m, caddr_t), sc->sc_mc->m_len);
	m->m_len = sc->sc_mc->m_len;
	for (mp = &top; *mp != sc->sc_mc; mp = &(*mp)->m_next)
	    ;
	*mp = m;
	sc->sc_m = sc->sc_mc;
    }

    /*
     * Try to allocate enough extra mbufs to handle the next packet.
     */
    if (pppgetm(sc) == 0) {
	m_freem(top);
	if (pppgetm(sc) == 0)
	{
	    sc->sc_if.if_flags &= ~IFF_UP;
	    microtime(&sc->sc_if.if_lastchange);
	}
	return (NULL);
    }

    return (top);
}

/*
 * PPP packet input routine.
 * The caller has checked and removed the FCS.
 * The return value is 1 if the packet was put on sc->sc_inq,
 * 0 otherwise.
 */
#define COMPTYPE(proto)	((proto) == PPP_VJC_COMP? TYPE_COMPRESSED_TCP: \
			 TYPE_UNCOMPRESSED_TCP)

int
ppppktin(sc, m, ilen)
    struct ppp_softc *sc;
    struct mbuf *m;
    int ilen;
{
    struct ifqueue *inq;
    int s, xlen, proto, rv;
    struct ppp_header hdr;

    sc->sc_if.if_ipackets++;
    rv = 0;

    hdr = *mtod(m, struct ppp_header *);
    proto = ntohs(hdr.ph_protocol);

#ifdef VJC
    /*
     * See if we have a VJ-compressed packet to uncompress.
     */
    if (proto == PPP_VJC_COMP || proto == PPP_VJC_UNCOMP) {
	char *pkttype = proto == PPP_VJC_COMP? "": "un";

	if (sc->sc_flags & SC_REJ_COMP_TCP) {
	    if (sc->sc_flags & SC_DEBUG)
		printf("ppp%d: %scomp pkt w/o compression; flags 0x%x\n",
			sc->sc_if.if_unit, pkttype, sc->sc_flags);
	    sc->sc_if.if_ierrors++;
	    return 0;
	}

	m->m_data += PPP_HDRLEN;
	m->m_len -= PPP_HDRLEN;
	ilen -= PPP_HDRLEN;
	xlen = sl_uncompress_tcp_part((u_char **)(&m->m_data),
					m->m_len, ilen,
					COMPTYPE(proto), &sc->sc_comp);

	if (xlen == 0) {
	    if (sc->sc_flags & SC_DEBUG)
		printf("ppp%d: sl_uncompress failed on type %scomp\n",
			sc->sc_if.if_unit, pkttype);
	    sc->sc_if.if_ierrors++;
	    return 0;
	}

	/* adjust the first mbuf by the decompressed amt */
	xlen += PPP_HDRLEN;
	m->m_len += xlen - ilen;
	ilen = xlen;
	m->m_data -= PPP_HDRLEN;
	proto = PPP_IP;

	/* put the ppp header back in place */
	hdr.ph_protocol = htons(PPP_IP);
	*mtod(m, struct ppp_header *) = hdr;
    }
#endif /* VJC */

    /* get this packet as an mbuf chain */
    if ((m = ppp_btom(sc)) == NULL) {
	sc->sc_if.if_ierrors++;
	return 0;
    }
    m->m_pkthdr.len = ilen;
    m->m_pkthdr.rcvif = &sc->sc_if;

#if NBPFILTER > 0
    /* See if bpf wants to look at the packet. */
    if (sc->sc_bpf)
	bpf_mtap(sc->sc_bpf, m);
#endif

    switch (proto) {
#ifdef INET
    case PPP_IP:
	/*
	 * IP packet - take off the ppp header and pass it up to IP.
	 */
	if ((sc->sc_if.if_flags & IFF_UP) == 0
	    || (sc->sc_flags & SC_ENABLE_IP) == 0) {
	    /* interface is down - drop the packet. */
	    m_freem(m);
	    return 0;
	}
	m->m_pkthdr.len -= PPP_HDRLEN;
	m->m_data += PPP_HDRLEN;
	m->m_len -= PPP_HDRLEN;
	schednetisr(NETISR_IP);
	inq = &ipintrq;
	break;
#endif

    default:
	/*
	 * Some other protocol - place on input queue for read().
	 */
	inq = &sc->sc_inq;
	rv = 1;
	break;
    }

    /*
     * Put the packet on the appropriate input queue.
     */
    s = splimp();
    if (IF_QFULL(inq)) {
	IF_DROP(inq);
	if (sc->sc_flags & SC_DEBUG)
	    printf("ppp%d: queue full\n", sc->sc_if.if_unit);
	sc->sc_if.if_ierrors++;
	sc->sc_if.if_iqdrops++;
	m_freem(m);
	rv = 0;
    } else
	IF_ENQUEUE(inq, m);

    splx(s);
    return rv;
}

/*
 * tty interface receiver interrupt.
 */
static unsigned paritytab[8] = {
    0x96696996, 0x69969669, 0x69969669, 0x96696996,
    0x69969669, 0x96696996, 0x96696996, 0x69969669
};

int
pppinput(c, tp)
    int c;
    register struct tty *tp;
{
    register struct ppp_softc *sc;
    struct mbuf *m;
    int ilen;

    tk_nin++;
    sc = (struct ppp_softc *) tp->t_sc;
    if (sc == NULL || tp != (struct tty *) sc->sc_devp)
	return 0;

    ++sc->sc_bytesrcvd;

    if ((tp->t_state & TS_CONNECTED) == 0) {
	if (sc->sc_flags & SC_DEBUG)
	    printf("ppp%d: no carrier\n", sc->sc_if.if_unit);
	goto flush;
    }
    if (c & TTY_ERRORMASK) {
	if (sc->sc_flags & SC_DEBUG)
	    printf("ppp%d: line error %x\n", sc->sc_if.if_unit, c & TTY_ERRORMASK);
	goto flush;
    }

    c &= TTY_CHARMASK;

    if (c & 0x80)
	sc->sc_flags |= SC_RCV_B7_1;
    else
	sc->sc_flags |= SC_RCV_B7_0;
    if (paritytab[c >> 5] & (1 << (c & 0x1F)))
	sc->sc_flags |= SC_RCV_ODDP;
    else
	sc->sc_flags |= SC_RCV_EVNP;

    if (sc->sc_flags & SC_LOG_RAWIN)
	ppplogchar(sc, c);

    if (c == PPP_FLAG) {
	ilen = sc->sc_ilen;
	sc->sc_ilen = 0;
	sc->sc_if.if_ibytes = sc->sc_bytesrcvd;

	if (sc->sc_rawin_count > 0)
	    ppplogchar(sc, -1);

	/*
	 * If SC_ESCAPED is set, then we've seen the packet
	 * abort sequence "}~".
	 */
	if (sc->sc_flags & (SC_FLUSH | SC_ESCAPED)
	    || ilen > 0 && sc->sc_fcs != PPP_GOODFCS) {
#ifdef VJC
	    /*
	     * If we've missed a packet, we must toss subsequent compressed
	     * packets which don't have an explicit connection ID.
	     */
	    sl_uncompress_tcp(NULL, 0, TYPE_ERROR, &sc->sc_comp);
#endif
	    if ((sc->sc_flags & (SC_FLUSH | SC_ESCAPED)) == 0){
		if (sc->sc_flags & SC_DEBUG)
		    printf("ppp%d: bad fcs\n", sc->sc_if.if_unit);
		sc->sc_if.if_ierrors++;
	    } else
		sc->sc_flags &= ~(SC_FLUSH | SC_ESCAPED);
	    return 0;
	}

	if (ilen < PPP_HDRLEN + PPP_FCSLEN) {
	    if (ilen) {
		if (sc->sc_flags & SC_DEBUG)
		    printf("ppp%d: too short (%d)\n", sc->sc_if.if_unit, ilen);
		sc->sc_if.if_ierrors++;
	    }
	    return 0;
	}

	/*
	 * Remove FCS trailer.  Somewhat painful...
	 */
	ilen -= 2;
	if (--sc->sc_mc->m_len == 0) {
	    for (m = sc->sc_m; m->m_next != sc->sc_mc; m = m->m_next)
		;
	    sc->sc_mc = m;
	}
	sc->sc_mc->m_len--;

	m = sc->sc_m;

	if (sc->sc_flags & SC_LOG_INPKT) {
	    printf("ppp%d: got %d bytes\n", sc->sc_if.if_unit, ilen);
	    pppdumpm(m, ilen);
	}

	if (ppppktin(sc, m, ilen)) {
	    /* Put a placeholder byte in canq for ttselect()/ttnread(). */
	    putc(0, &tp->t_canq);
	    ttwakeup(tp);
	}
	return 0;
    }

    if (sc->sc_flags & SC_FLUSH) {
	if (sc->sc_flags & SC_LOG_FLUSH)
	    ppplogchar(sc, c);
	return 0;
    }

    if (c < 0x20 && (sc->sc_rasyncmap & (1 << c)))
	return 0;

    if (sc->sc_flags & SC_ESCAPED) {
	sc->sc_flags &= ~SC_ESCAPED;
	c ^= PPP_TRANS;
    } else if (c == PPP_ESCAPE) {
	sc->sc_flags |= SC_ESCAPED;
	return 0;
    }

    /*
     * Initialize buffer on first octet received.
     * First octet could be address or protocol (when compressing
     * address/control).
     * Second octet is control.
     * Third octet is first or second (when compressing protocol)
     * octet of protocol.
     * Fourth octet is second octet of protocol.
     */
    if (sc->sc_ilen == 0) {
	/* reset the first input mbuf */
	m = sc->sc_m;
	m->m_len = 0;
	m->m_data = M_DATASTART(sc->sc_m) + HDROFF;
	sc->sc_mc = m;
	sc->sc_mp = mtod(m, char *);
	sc->sc_fcs = PPP_INITFCS;
	if (c != PPP_ALLSTATIONS) {
	    if (sc->sc_flags & SC_REJ_COMP_AC) {
		if (sc->sc_flags & SC_DEBUG)
		    printf("ppp%d: garbage received: 0x%x (need 0xFF)\n",
			   sc->sc_if.if_unit, c);
		goto flush;
	    }
	    *sc->sc_mp++ = PPP_ALLSTATIONS;
	    *sc->sc_mp++ = PPP_UI;
	    sc->sc_ilen += 2;
	    m->m_len += 2;
	}
    }
    if (sc->sc_ilen == 1 && c != PPP_UI) {
	if (sc->sc_flags & SC_DEBUG)
	    printf("ppp%d: missing UI (0x3), got 0x%x\n",
		   sc->sc_if.if_unit, c);
	goto flush;
    }
    if (sc->sc_ilen == 2 && (c & 1) == 1) {
	/* a compressed protocol */
	*sc->sc_mp++ = 0;
	sc->sc_ilen++;
	sc->sc_mc->m_len++;
    }
    if (sc->sc_ilen == 3 && (c & 1) == 0) {
	if (sc->sc_flags & SC_DEBUG)
	    printf("ppp%d: bad protocol %x\n", sc->sc_if.if_unit,
		   (sc->sc_mp[-1] << 8) + c);
	goto flush;
    }

    /* packet beyond configured mru? */
    if (++sc->sc_ilen > sc->sc_mru + PPP_HDRLEN + PPP_FCSLEN) {
	if (sc->sc_flags & SC_DEBUG)
	    printf("ppp%d: packet too big\n", sc->sc_if.if_unit);
	goto flush;
    }

    /* is this mbuf full? */
    m = sc->sc_mc;
    if (M_TRAILINGSPACE(m) <= 0) {
	sc->sc_mc = m = m->m_next;
	if (m == NULL) {
	    printf("ppp%d: too few input mbufs!\n", sc->sc_if.if_unit);
	    goto flush;
	}
	m->m_len = 0;
	m->m_data = M_DATASTART(m);
	sc->sc_mp = mtod(m, char *);
    }

    ++m->m_len;
    *sc->sc_mp++ = c;
    sc->sc_fcs = PPP_FCS(sc->sc_fcs, c);
    return 0;

 flush:
    if (!(sc->sc_flags & SC_FLUSH)) {
	sc->sc_if.if_ierrors++;
	sc->sc_flags |= SC_FLUSH;
	if (sc->sc_flags & SC_LOG_FLUSH)
	    ppplogchar(sc, c);
    }
    return 0;
}

/*
 * Process an ioctl request to interface.
 */
int
pppioctl(ifp, cmd, data)
    register struct ifnet *ifp;
    int cmd;
    caddr_t data;
{
    struct proc *p = curproc;	/* XXX */
    register struct ppp_softc *sc = &ppp_softc[ifp->if_unit];
    register struct ifaddr *ifa = (struct ifaddr *)data;
    register struct ifreq *ifr = (struct ifreq *)data;
    int s = splimp(), error = 0;


    switch (cmd) {
    case SIOCSIFFLAGS:
	if ((ifp->if_flags & IFF_RUNNING) == 0)
	    ifp->if_flags &= ~IFF_UP;
	break;

    case SIOCSIFADDR:
	if (ifa->ifa_addr->sa_family != AF_INET)
	    error = EAFNOSUPPORT;
	break;

    case SIOCSIFDSTADDR:
	if (ifa->ifa_addr->sa_family != AF_INET)
	    error = EAFNOSUPPORT;
	break;

    case SIOCSIFMTU:
	if (error = suser(p->p_ucred, &p->p_acflag))
	    return (error);
	if (ifr->ifr_mtu > PPP_MAXMTU)
	    error = EINVAL;
	else {
	    struct tty *tp;

	    sc->sc_if.if_mtu = ifr->ifr_mtu;
	    tp = (struct tty *) sc->sc_devp;
	    if (tp != NULL)
		clist_alloc_cblocks(&tp->t_outq, sc->sc_if.if_mtu + PPP_HIWAT,
				    sc->sc_if.if_mtu + PPP_HIWAT);
	}
	break;

    case SIOCGIFMTU:
	ifr->ifr_mtu = sc->sc_if.if_mtu;
	break;
    case SIOCADDMULTI:
    case SIOCDELMULTI:
	switch(ifr->ifr_addr.sa_family) {
#ifdef INET
	case AF_INET:
		break;
#endif
	default:
		error = EAFNOSUPPORT;
		break;
	}
	break;

    default:
	error = EINVAL;
    }
    splx(s);
    return (error);
}

#define MAX_DUMP_BYTES	128

static void
pppdumpm(m0, pktlen)
    struct mbuf *m0;
    int pktlen;
{
    char buf[3*MAX_DUMP_BYTES+4];
    char *bp = buf;
    struct mbuf *m;
    static char digits[] = "0123456789abcdef";

    for (m = m0; m && pktlen; m = m->m_next) {
	int l = m->m_len;
	u_char *rptr = (u_char *)m->m_data;

	if (pktlen > 0) {
	    if (l > pktlen)
		l = pktlen;
	    pktlen -= l;
	}
	while (l--) {
	    if (bp > buf + sizeof(buf) - 4)
		goto done;
	    *bp++ = digits[*rptr >> 4]; /* convert byte to ascii hex */
	    *bp++ = digits[*rptr++ & 0xf];
	}

	if (m->m_next) {
	    if (bp > buf + sizeof(buf) - 3)
		goto done;
	    *bp++ = '|';
	} else
	    *bp++ = ' ';
    }
done:
    if (m && pktlen)
	*bp++ = '>';
    *bp = 0;
    printf("%s\n", buf);
}

static void
ppplogchar(sc, c)
    struct ppp_softc *sc;
    int c;
{
    if (c >= 0)
	sc->sc_rawin[sc->sc_rawin_count++] = c;
    if (sc->sc_rawin_count >= sizeof(sc->sc_rawin)
	|| c < 0 && sc->sc_rawin_count > 0) {
	printf("ppp%d input: ", sc->sc_if.if_unit);
	pppdumpb(sc->sc_rawin, sc->sc_rawin_count);
	sc->sc_rawin_count = 0;
    }
}

static void
pppdumpb(b, l)
    u_char *b;
    int l;
{
    char buf[3*MAX_DUMP_BYTES+4];
    char *bp = buf;
    static char digits[] = "0123456789abcdef";

    while (l--) {
	if (bp >= buf + sizeof(buf) - 3) {
	    *bp++ = '>';
	    break;
	}
	*bp++ = digits[*b >> 4]; /* convert byte to ascii hex */
	*bp++ = digits[*b++ & 0xf];
	*bp++ = ' ';
    }

    *bp = 0;
    printf("%s\n", buf);
}

PSEUDO_SET(pppattach, if_ppp);

#endif	/* NPPP > 0 */
