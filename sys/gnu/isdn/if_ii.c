static char     _if_iiid[] = "@(#)$Id: if_ii.c,v 1.4 1995/11/16 10:10:50 bde Exp $";
/*******************************************************************************
 *  II - Version 0.1 $Revision: 1.4 $   $State: Exp $
 *
 * Copyright 1994 Dietmar Friede
 *******************************************************************************
 * Bug reports, patches, comments, suggestions should be sent to:
 *
 *	jkr@saarlink.de or jkrause@guug.de
 *
 *******************************************************************************
 */

/*
 * Copyright (c) 1994 Dietmar Friede (dietmar@friede.de) All rights reserved.
 * FSF/FSAG GNU Copyright applies
 *
 * A high level ip isdn driver.
 *
 * Uses loop driver as template. Small - and simple - is beautiful.
 */

#include "param.h"
#include "systm.h"
#include "mbuf.h"
#include "socket.h"
#include "errno.h"
#include "ioctl.h"
#include "protosw.h"

#include "net/if.h"
#include "net/if_types.h"
#include "net/netisr.h"
#include "net/route.h"

#ifdef	INET
#include "netinet/in.h"
#include "netinet/in_systm.h"
#include "netinet/in_var.h"
#include "netinet/ip.h"
#endif

#include "ii.h"
#include "gnu/isdn/isdn_ioctl.h"

#define	IIMTU	1500

static struct ifnet ii_if[NII];
static int      applnr[NII];
static int      next_if = 0;

static int	ii_ioctl __P((struct ifnet *ifp, int cmd, caddr_t data));
static int	iioutput __P((struct ifnet *ifp, struct mbuf *m,
			      struct sockaddr *dst, struct rtentry *rtp));

int
iiattach(int ap)
{
	register struct ifnet *ifp;

	if (next_if >= NII)
		return -1;

	applnr[next_if] = ap;
	ifp = &ii_if[next_if];
	ifp->if_unit = next_if;
	ifp->if_name = "ii";
	ifp->if_mtu = IIMTU;
	ifp->if_flags = IFF_SIMPLEX | IFF_POINTOPOINT ;
	ifp->if_ioctl = ii_ioctl;
	ifp->if_output = iioutput;
	ifp->if_type = IFT_ISDNBASIC;
	ifp->if_hdrlen = 0;
	ifp->if_addrlen = 0;
	ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;
	if_attach(ifp);
	/* ifp->if_flags |= IFF_RUNNING; */
	return next_if++;
}

static int
iioutput(struct ifnet * ifp, struct mbuf * m, struct sockaddr * dst,
	 struct rtentry * rtp)
{
	int             s, isr;
	register struct ifqueue *ifq = 0;

	if (dst->sa_family != AF_INET)
	{
		m_freem(m);
		return EAFNOSUPPORT;
	}
	s = splhigh();
	if (IF_QFULL(&ifp->if_snd))
	{
		IF_DROP(&ifp->if_snd);
		m_freem(m);
		ifp->if_oerrors++;
		isdn_output(applnr[ifp->if_unit]);
		splx(s);
		return (ENOBUFS);
	}
	IF_ENQUEUE(&ifp->if_snd, m);

	ifp->if_opackets++;
	ifp->if_obytes += m->m_pkthdr.len;

	isdn_output(applnr[ifp->if_unit]);
	splx(s);
	return (0);
}

int
ii_input(int no, int len, char *buf, int dir)
{
	int             error = 0;
	struct mbuf    *m;
	struct ifnet   *ifp = &(ii_if[no]);
	int             s;

	s = splhigh();
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == 0)
	{
		splx(s);
		return (0);
	}

	if (len >= MHLEN)
	{
		MCLGET(m, M_DONTWAIT);
		if ((m->m_flags & M_EXT) == 0)
		{
			(void) m_free(m);
			splx(s);
			return (0);
		}
	}
	bcopy((caddr_t) buf, mtod(m, caddr_t), len);
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = len;
	m->m_len = len;

	if (IF_QFULL(&ipintrq))
	{
		IF_DROP(&ipintrq);
		ifp->if_ierrors++;
		m_freem(m);
		splx(s);
		return(0);
	}
	IF_ENQUEUE(&ipintrq, m);
	ifp->if_ipackets++;
	schednetisr(NETISR_IP);
	splx(s);
	return(len);
}

void
ii_connect(int no)
{
	struct ifnet   *ifp = &ii_if[no];
	ifp->if_flags |= IFF_RUNNING;
}

void
ii_disconnect(int no)
{
	struct ifnet   *ifp = &ii_if[no];
	ifp->if_flags &= ~IFF_RUNNING;
}

int
ii_out(int no, char *buf, int len)
{
	struct ifnet   *ifp = &ii_if[no];
	struct mbuf    *m0, *m;
	int             l;

	IF_DEQUEUE(&ifp->if_snd, m);
	if (m == 0)
	{
		return (0);
	}
	/*
	 * Copy the mbuf chain into the transmit buf
	 */
	l = 0;
	for (m0 = m; m != 0; m = m->m_next)
	{
		if((l+= m->m_len) > len)
		{
			m_freem(m0);
			return(0);
		}
		bcopy(mtod(m, caddr_t), buf, m->m_len);
		buf += m->m_len;
	}
	m_freem(m0);

	return (l);
}

/*
 * Process an ioctl request.
 */
static int
ii_ioctl(ifp, cmd, data)
	register struct ifnet *ifp;
	int             cmd;
	caddr_t         data;
{
	struct ifaddr *ifa = (struct ifaddr *) data;
	struct ifreq *ifr = (struct ifreq *) data;
	int s;

	switch (cmd)
	{
	case SIOCSIFDSTADDR:
	case SIOCAIFADDR:
	case SIOCSIFADDR:
		if (ifa->ifa_addr->sa_family != AF_INET)
			return(EAFNOSUPPORT);
		ifp->if_flags |= IFF_UP;
	/* FALLTHROUGH */
	case SIOCSIFFLAGS:
		s= splhigh();
		if((!(ifp->if_flags & IFF_UP)) && (ifp->if_flags & IFF_RUNNING))
		{
			isdn_disconnect(applnr[ifp->if_unit],0);
			ifp->if_flags &= ~IFF_RUNNING;
		}
		break;
	case SIOCSIFMTU:
		ifr->ifr_metric = ifp->if_mtu;
		break;
	case SIOCGIFMTU:
		if(ifr->ifr_metric < 2048)
			return(EAFNOSUPPORT);
		ifp->if_mtu = ifr->ifr_metric;
		break;
	default:
printf("IIO %x",cmd);
		return(EINVAL);
	}
	return(0);
}
