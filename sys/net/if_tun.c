/*
 * Copyright (c) 1988, Julian Onions. 
 *
 * This source may be freely distributed, however I would be interested
 * in any changes that are made.
 *
 *	from: Revision 1.13  88/07/11  08:28:51  jpo
 *	from: 90/02/06 15:03 - Fixed a bug in where
 *				TIOCGPGRP and TIOCSPGRP were mixed up
 *	$Id: if_tun.c,v 1.2 1993/10/16 17:43:24 rgrimes Exp $
 */

/* if_tun.c - tunnel interface module & driver */

/* UNFINISHED CONVERSION TO 386BSD -wfj */

#include "tun.h"
#if NTUN > 0

/*
 * Tunnel driver.
 *
 * This driver takes packets off the IP i/f and hands them up to a
 * user process to have it's wicked way with. This driver has it's
 * roots in a similar driver written by Phil Cockcroft (formerly) at
 * UCL. This driver is based much more on read/write/select mode of
 * operation though.
 * 
 * Julian Onions <jpo@cs.nott.ac.uk>
 * Nottingham University 1987.
 */

#include "param.h"
#include "systm.h"
#include "mbuf.h"
#include "buf.h"
#include "protosw.h"
#include "socket.h"
#include "ioctl.h"
#include "errno.h"
#include "syslog.h"

#include "net/if.h"
#include "net/netisr.h"
#include "net/route.h"

#ifdef INET
#include "netinet/in.h"
#include "netinet/in_systm.h"
#include "netinet/in_var.h"
#include "netinet/ip.h"
#include "netinet/if_ether.h"
#endif

#ifdef NS
#include "netns/ns.h"
#include "netns/ns_if.h"
#endif

#define TUNDEBUG        if (tundebug) printf

int             tundebug = 0;
struct tunctl
{
        u_short         tun_flags;      /* misc flags */
        struct ifnet    tun_if;         /* the interface */
        int             tun_pgrp;       /* the process group - if any */
        struct proc    *tun_rsel;       /* read select */
        struct proc    *tun_wsel;       /* write select (not used) */
}       tunctl[NTUN];

extern int      ifqmaxlen;

int             tunoutput (), tunioctl (), tuninit ();

/* tunnel open - must be superuser & the device must be configured in */
tunopen (dev, flag)
dev_t           dev;
{
        register int    unit;
        struct tunctl  *tp;
        register struct ifnet *ifp;

        if (!suser ())
                return EACCES;
        if ((unit = minor (dev)) >= NTUN)
                return (ENXIO);
        tp = &tunctl[unit];
        if (tp->tun_flags & TUN_OPEN)
                return ENXIO;
        if ((tp->tun_flags & TUN_INITED) == 0) {
                tp->tun_flags = TUN_INITED;
                tunattach (unit);
        }
        ifp = &tp->tun_if;
        tp->tun_flags |= TUN_OPEN;
        TUNDEBUG ("%s%d: open\n", ifp->if_name, ifp->if_unit);
        return (0);
}

/* tunclose - close the device - mark i/f down & delete routing info */
tunclose (dev, flag)
dev_t           dev;
{
        int             s;
        int             rcoll;
        register int    unit = minor (dev);
        register struct tunctl *tp = &tunctl[unit];
        register struct ifnet *ifp = &tp->tun_if;
        register struct mbuf *m;

        rcoll = tp->tun_flags & TUN_RCOLL;
        tp->tun_flags &= TUN_INITED;

        /*
         * junk all pending output
         */
        do {
                s = splimp ();
                IF_DEQUEUE (&ifp->if_snd, m);
                splx (s);
                if (m) /* actually - m_freem checks this - but what the hell */
                        m_freem (m);
        } while (m != 0);
        if (ifp->if_flags & IFF_UP) {
                s = splimp ();
                if_down (ifp);
                if (ifp->if_flags & IFF_RUNNING)
                        rtinit (ifp->if_addrlist, (int)SIOCDELRT, RTF_HOST);
                splx (s);
        }
        tp -> tun_pgrp = 0;
        if (tp -> tun_rsel)
                selwakeup (tp->tun_rsel, rcoll);
                
        tp -> tun_rsel = tp -> tun_wsel = (struct proc *)0;

        TUNDEBUG ("%s%d: closed\n", ifp->if_name, ifp->if_unit);
        return (0);
}

/*
 * attach an interface N.B. argument is not same as other drivers
 */
int
tunattach (unit)
int             unit;
{
        register struct ifnet *ifp = &tunctl[unit].tun_if;
        register struct sockaddr_in *sin;

        ifp->if_unit = unit;
        ifp->if_name = "tun";
        ifp->if_mtu = TUNMTU;
        ifp->if_ioctl = tunioctl;
        ifp->if_output = tunoutput;
        ifp->if_init = tuninit;
#ifndef BSD4_3
        sin = (struct sockaddr_in *) & ifp->if_addr;
        sin->sin_family = AF_INET;
#endif
        ifp->if_flags = IFF_POINTOPOINT;
        ifp->if_snd.ifq_maxlen = ifqmaxlen;
        ifp->if_collisions = ifp->if_ierrors = ifp->if_oerrors = 0;
        ifp->if_ipackets = ifp->if_opackets = 0;
        if_attach (ifp);
        TUNDEBUG ("%s%d: tunattach\n", ifp->if_name, ifp->if_unit);
        return 0;
}

int
tuninit (unit)
int             unit;
{
        register struct tunctl *tp = &tunctl[unit];
        register struct ifnet *ifp = &tp->tun_if;
#ifndef BSD4_3
        register struct sockaddr_in *sin;

        sin = (struct sockaddr_in *) & ifp->if_addr;
        if (sin->sin_addr.s_addr == 0)  /* if address still unknown */
                return;
        if_rtinit (ifp, RTF_UP);
#endif
        ifp->if_flags |= IFF_UP | IFF_RUNNING;
        tp->tun_flags |= TUN_IASET;
        TUNDEBUG ("%s%d: tuninit\n", ifp->if_name, ifp->if_unit);
        return 0;
}

/*
 * Process an ioctl request.
 * The problem here is 4.2 pass a struct ifreq * to this routine,
 * sun only passes a struct sockaddr * since 3.2 at least. This is
 * rather upsetting! Also, sun doesn't pass the SIOCDSTADDR ioctl through
 * so we can't detect this being set directly. This is the reason for
 * tuncheckready.
 * Under 4.3 and SunOs 4.0 we now get the SIOCSIFDSTADDR ioctl, and we get a
 * struct in_ifaddr * for data. (tte)
 */

#if !defined(BSD4_3) && defined(sun)

/*
 * workaround for not being able to detect DSTADDR being set.
 */

tuncheckready (ifp)
struct ifnet *ifp;
{
        struct tunctl *tp = &tunctl[ifp->if_unit];
        struct sockaddr_in *sin;

        sin = (struct sockaddr_in *) &ifp->if_dstaddr;
        if (sin->sin_addr.s_addr == 0)
                return 0;
        tp -> tun_flags |= TUN_DSTADDR;
        return 1;
}
#else
#define tuncheckready(dummy) 1
#endif /* !defined(BSD4_3) && defined(sun) */

int
tunioctl (ifp, cmd, data)
register struct ifnet *ifp;
int             cmd;
caddr_t         data;
{
#ifndef BSD4_3
#ifdef sun      
        struct sockaddr_in *sin = (struct sockaddr_in *) data;
#else
        struct sockaddr_in *sin;
        struct ifreq    *ifr = (struct ifreq *) data;
#endif
#endif /* BSD4_3 */

        int             s = splimp (), error = 0;
        struct tunctl  *tp = &tunctl[ifp->if_unit];

        switch (cmd)
        {
            case SIOCSIFADDR:
#ifndef BSD4_3
                if (ifp->if_flags & IFF_RUNNING)
                        if_rtinit (ifp, -1);    /* delete previous route */
#ifndef sun
                sin = (struct sockaddr_in *)&ifr -> ifr_addr;
#endif
                ifp->if_addr = *((struct sockaddr *) sin);
                ifp->if_net = in_netof (sin->sin_addr);
                ifp->if_host[0] = in_lnaof (sin->sin_addr);
#endif
                tuninit (ifp->if_unit);
                break;
            case SIOCSIFDSTADDR:
                tp->tun_flags |= TUN_DSTADDR;
#ifndef BSD4_3
#ifndef sun
                sin = (struct sockaddr_in *)&ifr -> ifr_addr;
#endif
                ifp->if_dstaddr = *((struct sockaddr *)sin);
#endif BSD4_3
                TUNDEBUG ("%s%d: destination addres set\n", ifp->if_name,
                        ifp -> if_unit);
                break;
            default:
                error = EINVAL;
        }
        splx (s);
        return (error);
}

/*
 * tunoutput - queue packets from higher level ready to put out.
 */
tunoutput (ifp, m0, dst)
struct ifnet   *ifp;
struct mbuf    *m0;
struct sockaddr *dst;
{
        struct tunctl  *tp;
        struct proc     *p;
        int             s;

        TUNDEBUG ("%s%d: tunoutput\n", ifp->if_name, ifp->if_unit);
        tp = &tunctl[ifp->if_unit];
        if ((tp->tun_flags & TUN_READY) != TUN_READY) {
                if(tuncheckready(ifp) == 0) {
                        TUNDEBUG ("%s%d: not ready 0%o\n", ifp->if_name,
                                ifp->if_unit, tp->tun_flags);
                        m_freem (m0);
                        return EHOSTDOWN;
                }
        }

        switch (dst->sa_family) {
#ifdef INET
            case AF_INET:
                s = splimp ();
                if (IF_QFULL (&ifp->if_snd))
                {
                        IF_DROP (&ifp->if_snd);
                        m_freem (m0);
                        splx (s);
                        ifp->if_collisions++;
                        return (ENOBUFS);
                }
                IF_ENQUEUE (&ifp->if_snd, m0);
                splx (s);
                ifp->if_opackets++;
                break;
#endif
            default:
                m_freem (m0);
                return EAFNOSUPPORT;
        }

        if (tp->tun_flags & TUN_RWAIT) {
                tp->tun_flags &= ~TUN_RWAIT;
                wakeup ((caddr_t) tp);
        }
        if (tp->tun_flags & TUN_ASYNC && tp -> tun_pgrp != 0) {
                if (tp->tun_pgrp > 0)
                        gsignal (tp->tun_pgrp, SIGIO);
                else if ((p = pfind (-tp->tun_pgrp)) != 0)
                        psignal (p, SIGIO);
        }
        if (tp->tun_rsel) {
                selwakeup (tp->tun_rsel, tp->tun_flags & TUN_RCOLL);
                tp->tun_flags &= ~TUN_RCOLL;
                tp->tun_rsel = (struct proc *) 0;
        }
        return 0;
}

/*
 * the cdevsw interface is now pretty minimal.
 */

tuncioctl (dev, cmd, data, flag)
dev_t           dev;
caddr_t         data;
{
        int     unit = minor(dev);
        struct tunctl *tp = &tunctl[unit];
        int     s;

        switch (cmd) {
            case TUNSDEBUG:
                tundebug = *(int *)data;
                break;

            case TUNGDEBUG:
                *(int *)data = tundebug;
                break;

            case FIONBIO:
                if (*(int *)data)
                        tp->tun_flags |= TUN_NBIO;
                else
                        tp->tun_flags &= ~TUN_NBIO;
                break;

            case FIOASYNC:
                if (*(int *)data)
                        tp->tun_flags |= TUN_ASYNC;
                else
                        tp->tun_flags &= ~TUN_ASYNC;
                break;

            case FIONREAD:
                s = splimp ();
                if (tp->tun_if.if_snd.ifq_head)
                        *(int *)data = tp->tun_if.if_snd.ifq_head->m_len;
                else    *(int *)data = 0;
                splx (s);
                break;

            case TIOCSPGRP:
                tp->tun_pgrp = *(int *)data;
                break;

            case TIOCGPGRP:
                *(int *)data = tp->tun_pgrp;
                break;

            default:
                return (ENOTTY);
        }
        return (0);
}

/*
 * The cdevsw read interface - reads a packet at a time, or at least as much
 * of a packet as can be read.
 */
tunread (dev, uio)
dev_t           dev;
struct uio     *uio;
{
        register struct ifnet *ifp;
        register struct mbuf *m, *m0;
        int             unit = minor (dev);
        int             len, s;
        int             error = 0;
        struct tunctl  *tp;

        tp = &tunctl[unit];
        ifp = &tp->tun_if;
        TUNDEBUG ("%s%d: read\n", ifp->if_name, ifp->if_unit);
        if ((tp->tun_flags & TUN_READY) != TUN_READY) {
                if(tuncheckready(ifp) == 0) {
                        TUNDEBUG ("%s%d: not ready 0%o\n", ifp->if_name,
                                ifp->if_unit, tp->tun_flags);
                        return EHOSTDOWN;
                }
        }

        tp->tun_flags &= ~TUN_RWAIT;

        s = splimp ();
        do {
                IF_DEQUEUE (&ifp->if_snd, m0);
                if (m0 == 0) {
                        if (tp -> tun_flags & TUN_NBIO) {
                                splx (s);
                                return EWOULDBLOCK;
                        }
                        tp->tun_flags |= TUN_RWAIT;
                        sleep ((caddr_t) tp, PZERO + 1);
                }
        } while (m0 == 0);
        splx (s);

        while (m0 && uio->uio_resid > 0 && error == 0) {
                len = MIN (uio->uio_resid, m0->m_len);
                if (len == 0)
                        break;
                error = uiomove (mtod (m0, caddr_t), len,
                                 UIO_READ, uio);
                MFREE (m0, m);
                m0 = m;
        }

        if (m0 != 0) {
                TUNDEBUG ("Dropping mbuf\n");
                m_freem (m0);
        }
        return error;
}

/*
 * the cdevsw write interface - an atomic write is a packet - or else!
 */

tunwrite (dev, uio)
int             dev;
struct uio     *uio;
{
        int             error = 0;
        int             unit = minor (dev);
        struct mbuf    *top, **mp, *m;
        struct ifnet   *ifp = &(tunctl[unit].tun_if);
        int             s;

        TUNDEBUG ("%s%d: tunwrite\n", ifp->if_name, ifp->if_unit);

        if (uio->uio_resid < 0 || uio->uio_resid > TUNMTU) {
                TUNDEBUG ("%s%d: len=%d!\n", ifp->if_name, ifp->if_unit,
                          uio->uio_resid);
                return EIO;
        }
        top = 0;
        mp = &top;
        while (error == 0 && uio->uio_resid > 0) {
                MGET (m, M_DONTWAIT, MT_DATA);
                if (m == 0) {
                        error = ENOBUFS;
                        break;
                }
                m->m_len = MIN (MLEN, uio->uio_resid);
                error = uiomove (mtod (m, caddr_t), m->m_len, UIO_WRITE, uio);
                *mp = m;
                mp = &m->m_next;
        }
        if (error) {
                if (top)
                        m_freem (top);
                return error;
        }

#ifdef BSD4_3
        /*
         * Place interface pointer before the data
         * for the receiving protocol.
         */
        if (top->m_off <= MMAXOFF &&
            top->m_off >= MMINOFF + sizeof(struct ifnet *)) {
                top->m_off -= sizeof(struct ifnet *);
                top->m_len += sizeof(struct ifnet *);
        } else {
                MGET(m, M_DONTWAIT, MT_HEADER);
                if (m == (struct mbuf *)0)
                        return (ENOBUFS);
                m->m_len = sizeof(struct ifnet *);
                m->m_next = top;
                top = m;
        }
        *(mtod(top, struct ifnet **)) = ifp;
#endif /* BSD4_3 */

        s = splimp ();
        if (IF_QFULL (&ipintrq)) {
                IF_DROP (&ipintrq);
                splx (s);
                ifp->if_collisions++;
                m_freem (top);
                return ENOBUFS;
        }
        IF_ENQUEUE (&ipintrq, top);
        splx (s);
        ifp->if_ipackets++;
        schednetisr (NETISR_IP);
        return error;
}

/*
 * tunselect - the select interface, this is only useful on reads really.
 * The write detect always returns true, write never blocks anyway, it either
 * accepts the packet or drops it.
 */
tunselect (dev, rw)
dev_t           dev;
int             rw;
{
        int             unit = minor (dev);
        register struct tunctl *tp = &tunctl[unit];
        struct ifnet   *ifp = &tp->tun_if;
        int             s = splimp ();

        TUNDEBUG ("%s%d: tunselect\n", ifp->if_name, ifp->if_unit);
        switch (rw) {
            case FREAD:
                if (ifp->if_snd.ifq_len > 0) {
                        splx (s);
                        TUNDEBUG ("%s%d: tunselect q=%d\n", ifp->if_name,
                                  ifp->if_unit, ifp->if_snd.ifq_len);
                        return 1;
                }
                if (tp->tun_rsel && tp->tun_rsel->p_wchan ==
                    (caddr_t) & selwait)
                        tp->tun_flags |= TUN_RCOLL;
                else
                        tp->tun_rsel = u.u_procp;
                break;
            case FWRITE:
                splx (s);
                return 1;
        }
        splx (s);
        TUNDEBUG ("%s%d: tunselect waiting\n", ifp->if_name, ifp->if_unit);
        return 0;
}
#endif  NTUN
