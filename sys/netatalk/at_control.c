/*
 * Copyright (c) 1990,1991 Regents of The University of Michigan.
 * All Rights Reserved.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#undef s_net
#include <netinet/if_ether.h>

#include "at.h"
#include "at_var.h"
#include "aarp.h"
#include "phase2.h"
#include <netatalk/at_extern.h>

static int aa_addrangeroute(struct ifaddr *ifa, int first, int last);
static int aa_addsingleroute(struct ifaddr *ifa,
			struct at_addr *addr, struct at_addr *mask);
static int aa_delsingleroute(struct ifaddr *ifa,
			struct at_addr *addr, struct at_addr *mask);
static int aa_dosingleroute(struct ifaddr *ifa, struct at_addr *addr,
			struct at_addr *mask, int cmd, int flags);
static int at_scrub( struct ifnet *ifp, struct at_ifaddr *aa );
static int at_ifinit( struct ifnet *ifp, struct at_ifaddr *aa,
					struct sockaddr_at *sat );

# define sateqaddr(a,b)	((a)->sat_len == (b)->sat_len && \
		    (a)->sat_family == (b)->sat_family && \
		    (a)->sat_addr.s_net == (b)->sat_addr.s_net && \
		    (a)->sat_addr.s_node == (b)->sat_addr.s_node )

int
at_control( int cmd, caddr_t data, struct ifnet *ifp, struct proc *p )
{
    struct ifreq	*ifr = (struct ifreq *)data;
    struct sockaddr_at	*sat;
    struct netrange	*nr;
    struct at_aliasreq	*ifra = (struct at_aliasreq *)data;
    struct at_ifaddr	*aa0;
    struct at_ifaddr	*aa = 0;
    struct mbuf		*m;
    struct ifaddr	*ifa;

    if ( ifp ) {
	for ( aa = at_ifaddr; aa; aa = aa->aa_next ) {
	    if ( aa->aa_ifp == ifp ) break;
	}
    }

    switch ( cmd ) {
    case SIOCAIFADDR:
    case SIOCDIFADDR:
	if ( ifra->ifra_addr.sat_family == AF_APPLETALK ) {
	    for ( ; aa; aa = aa->aa_next ) {
		if ( aa->aa_ifp == ifp &&
			sateqaddr( &aa->aa_addr, &ifra->ifra_addr )) {
		    break;
		}
	    }
	}
	if ( cmd == SIOCDIFADDR && aa == 0 ) {
	    return( EADDRNOTAVAIL );
	}
	/*FALLTHROUGH*/

    case SIOCSIFADDR:
	if ( suser(p->p_ucred, &p->p_acflag) ) {
	    return( EPERM );
	}

	sat = satosat( &ifr->ifr_addr );
	nr = (struct netrange *)sat->sat_zero;
	if ( nr->nr_phase == 1 ) {
	    for ( ; aa; aa = aa->aa_next ) {
		if ( aa->aa_ifp == ifp &&
			( aa->aa_flags & AFA_PHASE2 ) == 0 ) {
		    break;
		}
	    }
	} else {		/* default to phase 2 */
	    for ( ; aa; aa = aa->aa_next ) {
		if ( aa->aa_ifp == ifp && ( aa->aa_flags & AFA_PHASE2 )) {
		    break;
		}
	    }
	}

	if ( ifp == 0 )
	    panic( "at_control" );

	if ( aa == (struct at_ifaddr *) 0 ) {
	    m = m_getclr( M_WAIT, MT_IFADDR );
	    if ( m == (struct mbuf *)NULL ) {
		return( ENOBUFS );
	    }

	    if (( aa = at_ifaddr ) != NULL ) {
		/*
		 * Don't let the loopback be first, since the first
		 * address is the machine's default address for
		 * binding.
		 */
		if ( at_ifaddr->aa_ifp->if_flags & IFF_LOOPBACK ) {
		    aa = mtod( m, struct at_ifaddr *);
		    aa->aa_next = at_ifaddr;
		    at_ifaddr = aa;
		} else {
		    for ( ; aa->aa_next; aa = aa->aa_next )
			;
		    aa->aa_next = mtod( m, struct at_ifaddr *);
		}
	    } else {
		at_ifaddr = mtod( m, struct at_ifaddr *);
	    }

	    aa = mtod( m, struct at_ifaddr *);

	    if (( ifa = ifp->if_addrlist ) != NULL ) {
		for ( ; ifa->ifa_next; ifa = ifa->ifa_next )
		    ;
		ifa->ifa_next = (struct ifaddr *)aa;
	    } else {
		ifp->if_addrlist = (struct ifaddr *)aa;
	    }

	    aa->aa_ifa.ifa_addr = (struct sockaddr *)&aa->aa_addr;
	    aa->aa_ifa.ifa_dstaddr = (struct sockaddr *)&aa->aa_addr;
	    aa->aa_ifa.ifa_netmask = (struct sockaddr *)&aa->aa_netmask;

	    /*
	     * Set/clear the phase 2 bit.
	     */
	    if ( nr->nr_phase == 1 ) {
		aa->aa_flags &= ~AFA_PHASE2;
	    } else {
		aa->aa_flags |= AFA_PHASE2;
	    }
	    aa->aa_ifp = ifp;
	} else {
	    at_scrub( ifp, aa );
	}
	break;

    case SIOCGIFADDR :
	sat = satosat( &ifr->ifr_addr );
	nr = (struct netrange *)sat->sat_zero;
	if ( nr->nr_phase == 1 ) {
	    for ( ; aa; aa = aa->aa_next ) {
		if ( aa->aa_ifp == ifp &&
			( aa->aa_flags & AFA_PHASE2 ) == 0 ) {
		    break;
		}
	    }
	} else {		/* default to phase 2 */
	    for ( ; aa; aa = aa->aa_next ) {
		if ( aa->aa_ifp == ifp && ( aa->aa_flags & AFA_PHASE2 )) {
		    break;
		}
	    }
	}

	if ( aa == (struct at_ifaddr *) 0 )
	    return( EADDRNOTAVAIL );
	break;
    }

    switch ( cmd ) {
    case SIOCGIFADDR:
	sat = (struct sockaddr_at *)&ifr->ifr_addr;
	*sat = aa->aa_addr;
	((struct netrange *)&sat->sat_zero)->nr_phase
		= (aa->aa_flags & AFA_PHASE2) ? 2 : 1;
	((struct netrange *)&sat->sat_zero)->nr_firstnet = aa->aa_firstnet;
	((struct netrange *)&sat->sat_zero)->nr_lastnet = aa->aa_lastnet;
	break;

    case SIOCSIFADDR:
	return( at_ifinit( ifp, aa, (struct sockaddr_at *)&ifr->ifr_addr ));

    case SIOCAIFADDR:
	if ( sateqaddr( &ifra->ifra_addr, &aa->aa_addr )) {
	    return( 0 );
	}
	return( at_ifinit( ifp, aa, (struct sockaddr_at *)&ifr->ifr_addr ));

    case SIOCDIFADDR:
	at_scrub( ifp, aa );
	if (( ifa = ifp->if_addrlist ) == (struct ifaddr *)aa ) {
	    ifp->if_addrlist = ifa->ifa_next;
	} else {
	    while ( ifa->ifa_next && ( ifa->ifa_next != (struct ifaddr *)aa )) {
		ifa = ifa->ifa_next;
	    }
	    if ( ifa->ifa_next ) {
		ifa->ifa_next = ((struct ifaddr *)aa)->ifa_next;
	    } else {
		panic( "at_control" );
	    }
	}

	aa0 = aa;
	if ( aa0 == ( aa = at_ifaddr )) {
	    at_ifaddr = aa->aa_next;
	} else {
	    while ( aa->aa_next && ( aa->aa_next != aa0 )) {
		aa = aa->aa_next;
	    }
	    if ( aa->aa_next ) {
		aa->aa_next = aa0->aa_next;
	    } else {
		panic( "at_control" );
	    }
	}
	m_free( dtom( aa0 ));
	break;

    default:
	if ( ifp == 0 || ifp->if_ioctl == 0 )
	    return( EOPNOTSUPP );
	return( (*ifp->if_ioctl)( ifp, cmd, data ));
    }
    return( 0 );
}

static int
at_scrub( ifp, aa )
    struct ifnet	*ifp;
    struct at_ifaddr	*aa;
{
    int			error;

    if ( aa->aa_flags & AFA_ROUTE ) {
	if (( error = rtinit( &(aa->aa_ifa), RTM_DELETE,
		( ifp->if_flags & IFF_LOOPBACK ) ? RTF_HOST : 0 )) != 0 ) {
	    return( error );
	}
	aa->aa_ifa.ifa_flags &= ~IFA_ROUTE;
	aa->aa_flags &= ~AFA_ROUTE;
    }
    return( 0 );
}

static int 
at_ifinit( ifp, aa, sat )
    struct ifnet	*ifp;
    struct at_ifaddr	*aa;
    struct sockaddr_at	*sat;
{
    struct netrange	nr, onr;
    struct sockaddr_at	oldaddr;
    int			s = splimp(), error = 0, i, j;
    int			flags = RTF_UP, netinc, nodeinc, nnets;
    u_short		net;

    oldaddr = aa->aa_addr;
    bzero( AA_SAT( aa ), sizeof( struct sockaddr_at ));
    bcopy( sat->sat_zero, &nr, sizeof( struct netrange ));
    bcopy( sat->sat_zero, AA_SAT( aa )->sat_zero, sizeof( struct netrange ));
    nnets = ntohs( nr.nr_lastnet ) - ntohs( nr.nr_firstnet ) + 1;

    onr.nr_firstnet = aa->aa_firstnet;
    onr.nr_lastnet = aa->aa_lastnet;
    aa->aa_firstnet = nr.nr_firstnet;
    aa->aa_lastnet = nr.nr_lastnet;

/* XXX ALC */
    printf("at_ifinit: %s: %u.%u range %u-%u phase %d\n",
	ifp->if_name,
	ntohs(sat->sat_addr.s_net), sat->sat_addr.s_node,
	ntohs(aa->aa_firstnet), ntohs(aa->aa_lastnet),
	(aa->aa_flags & AFA_PHASE2) ? 2 : 1);

    /*
     * We could eliminate the need for a second phase 1 probe (post
     * autoconf) if we check whether we're resetting the node. Note
     * that phase 1 probes use only nodes, not net.node pairs.  Under
     * phase 2, both the net and node must be the same.
     */
    if ( ifp->if_flags & IFF_LOOPBACK ) {
	AA_SAT( aa )->sat_len = sat->sat_len;
	AA_SAT( aa )->sat_family = AF_APPLETALK;
	AA_SAT( aa )->sat_addr.s_net = sat->sat_addr.s_net;
	AA_SAT( aa )->sat_addr.s_node = sat->sat_addr.s_node;
#if 0
    } else if ( fp->if_flags & IFF_POINTOPOINT) {
	/* unimplemented */
#endif
    } else {
	aa->aa_flags |= AFA_PROBING;
	AA_SAT( aa )->sat_len = sizeof(struct sockaddr_at);
	AA_SAT( aa )->sat_family = AF_APPLETALK;
	if ( aa->aa_flags & AFA_PHASE2 ) {
	    if ( sat->sat_addr.s_net == ATADDR_ANYNET ) {
		if ( nnets != 1 ) {
		    net = ntohs( nr.nr_firstnet ) + time.tv_sec % ( nnets - 1 );
		} else {
		    net = ntohs( nr.nr_firstnet );
		}
	    } else {
		if ( ntohs( sat->sat_addr.s_net ) < ntohs( nr.nr_firstnet ) ||
			ntohs( sat->sat_addr.s_net ) > ntohs( nr.nr_lastnet )) {
		    aa->aa_addr = oldaddr;
		    aa->aa_firstnet = onr.nr_firstnet;
		    aa->aa_lastnet = onr.nr_lastnet;
		    return( EINVAL );
		}
		net = ntohs( sat->sat_addr.s_net );
	    }
	} else {
	    net = ntohs( sat->sat_addr.s_net );
	}

	if ( sat->sat_addr.s_node == ATADDR_ANYNODE ) {
	    AA_SAT( aa )->sat_addr.s_node = time.tv_sec;
	} else {
	    AA_SAT( aa )->sat_addr.s_node = sat->sat_addr.s_node;
	}

	for ( i = nnets, netinc = 1; i > 0; net = ntohs( nr.nr_firstnet ) +
		(( net - ntohs( nr.nr_firstnet ) + netinc ) % nnets ), i-- ) {
	    AA_SAT( aa )->sat_addr.s_net = htons( net );

	    for ( j = 0, nodeinc = time.tv_sec | 1; j < 256;
		    j++, AA_SAT( aa )->sat_addr.s_node += nodeinc ) {
		if ( AA_SAT( aa )->sat_addr.s_node > 253 ||
			AA_SAT( aa )->sat_addr.s_node < 1 ) {
		    continue;
		}
		aa->aa_probcnt = 10;
		timeout( (timeout_func_t)aarpprobe, (caddr_t)ifp, hz / 5 );
		splx( s );
		if ( tsleep( aa, PPAUSE|PCATCH, "at_ifinit", 0 )) {
		    printf( "at_ifinit: why did this happen?!\n" );
		    aa->aa_addr = oldaddr;
		    aa->aa_firstnet = onr.nr_firstnet;
		    aa->aa_lastnet = onr.nr_lastnet;
		    return( EINTR );
		}
		s = splimp();
		if (( aa->aa_flags & AFA_PROBING ) == 0 ) {
		    break;
		}
	    }
	    if (( aa->aa_flags & AFA_PROBING ) == 0 ) {
		break;
	    }
	    /* reset node for next network */
	    AA_SAT( aa )->sat_addr.s_node = time.tv_sec;
	}

	if ( aa->aa_flags & AFA_PROBING ) {
	    aa->aa_addr = oldaddr;
	    aa->aa_firstnet = onr.nr_firstnet;
	    aa->aa_lastnet = onr.nr_lastnet;
	    splx( s );
	    return( EADDRINUSE );
	}
    }

    if ( ifp->if_ioctl &&
	    ( error = (*ifp->if_ioctl)( ifp, SIOCSIFADDR, (caddr_t)aa ))) {
	aa->aa_addr = oldaddr;
	aa->aa_firstnet = onr.nr_firstnet;
	aa->aa_lastnet = onr.nr_lastnet;
	splx( s );
	return( error );
    }

  /* Initialize interface netmask, which is silly for us */

    bzero(&aa->aa_netmask, sizeof(aa->aa_netmask));
    aa->aa_netmask.sat_len = sizeof(struct sockaddr_at);
    aa->aa_netmask.sat_family = AF_APPLETALK;
    aa->aa_ifa.ifa_netmask = (struct sockaddr *) &aa->aa_netmask;

  /* "Add a route to the network" */

    aa->aa_ifa.ifa_metric = ifp->if_metric;
    if (ifp->if_flags & IFF_BROADCAST) {
	bzero(&aa->aa_broadaddr, sizeof(aa->aa_broadaddr));
	aa->aa_broadaddr.sat_len = sat->sat_len;
	aa->aa_broadaddr.sat_family = AF_APPLETALK;
	aa->aa_broadaddr.sat_addr.s_net = htons(0);
	aa->aa_broadaddr.sat_addr.s_node = 0xff;
	aa->aa_ifa.ifa_broadaddr = (struct sockaddr *) &aa->aa_broadaddr;
	aa->aa_netmask.sat_addr.s_net = htons(0xffff);	/* XXX */
	aa->aa_netmask.sat_addr.s_node = htons(0);	/* XXX */
    } else if (ifp->if_flags & IFF_LOOPBACK) {
	aa->aa_ifa.ifa_dstaddr = aa->aa_ifa.ifa_addr;
	aa->aa_netmask.sat_addr.s_net = htons(0xffff);	/* XXX */
	aa->aa_netmask.sat_addr.s_node = htons(0xffff);	/* XXX */
	flags |= RTF_HOST;
    } else if (ifp->if_flags & IFF_POINTOPOINT) {
	aa->aa_ifa.ifa_dstaddr = aa->aa_ifa.ifa_addr;
	aa->aa_netmask.sat_addr.s_net = htons(0xffff);
	aa->aa_netmask.sat_addr.s_node = htons(0xffff);
	flags |= RTF_HOST;
    }
    error = rtinit(&(aa->aa_ifa), (int)RTM_ADD, flags);

#if 0
    if ( ifp->if_flags & IFF_LOOPBACK ) {
	struct at_addr	rtaddr, rtmask;

	bzero(&rtaddr, sizeof(rtaddr));
	bzero(&rtmask, sizeof(rtmask));
	rtaddr.s_net = AA_SAT( aa )->sat_addr.s_net;
	rtaddr.s_node = AA_SAT( aa )->sat_addr.s_node;
	rtmask.s_net = 0xffff;
	rtmask.s_node = 0xff;

	error = aa_addsingleroute(&aa->aa_ifa, &rtaddr, &rtmask);

    } else {

    /* Install routes for our own network, and then also for
       all networks above and below it in the network range */

	error = aa_addrangeroute(&aa->aa_ifa,
		ntohs(aa->aa_addr.sat_addr.s_net),
		ntohs(aa->aa_addr.sat_addr.s_net) + 1);
	if (!error
		&& ntohs(aa->aa_firstnet) < ntohs(aa->aa_addr.sat_addr.s_net))
	    error = aa_addrangeroute(&aa->aa_ifa,
		  ntohs(aa->aa_firstnet), ntohs(aa->aa_addr.sat_addr.s_net));
	if (!error
		&& ntohs(aa->aa_addr.sat_addr.s_net) < ntohs(aa->aa_lastnet))
	    error = aa_addrangeroute(&aa->aa_ifa,
		  ntohs(aa->aa_addr.sat_addr.s_net) + 1,
		  ntohs(aa->aa_lastnet) + 1);
    }
#endif


    if ( error ) {
	aa->aa_addr = oldaddr;
	aa->aa_firstnet = onr.nr_firstnet;
	aa->aa_lastnet = onr.nr_lastnet;
	splx( s );
	return( error );
    }

    aa->aa_ifa.ifa_flags |= IFA_ROUTE;
    aa->aa_flags |= AFA_ROUTE;
    splx( s );
    return( 0 );
}

int
at_broadcast( sat )
    struct sockaddr_at	*sat;
{
    struct at_ifaddr	*aa;

    if ( sat->sat_addr.s_node != ATADDR_BCAST ) {
	return( 0 );
    }
    if ( sat->sat_addr.s_net == ATADDR_ANYNET ) {
	return( 1 );
    } else {
	for ( aa = at_ifaddr; aa; aa = aa->aa_next ) {
	    if (( aa->aa_ifp->if_flags & IFF_BROADCAST ) &&
		 ( ntohs( sat->sat_addr.s_net ) >= ntohs( aa->aa_firstnet ) &&
		 ntohs( sat->sat_addr.s_net ) <= ntohs( aa->aa_lastnet ))) {
		return( 1 );
	    }
	}
    }
    return( 0 );
}

/*
 * aa_addrangeroute()
 *
 * Add a route for a range of networks from bot to top - 1.
 * Algorithm:
 *
 * Split the range into three subranges such that the middle
 * subrange is from (base + 2^N) to (base + 2^N + 2^(N-1)) for
 * some N. Then add a route for the middle range and recurse on
 * the upper and lower sub-ranges. As a degenerate case, it may
 * be that the middle subrange is empty.
 */

static int
aa_addrangeroute(struct ifaddr *ifa, int bot, int top)
{
  int			base, mask, mbot, mtop;
  int			a, b, abit, bbit, error;
  struct at_addr	rtaddr, rtmask;

/* Special case the whole range */

  if (bot == 0 && top == 0xffff)
  {
    bzero(&rtaddr, sizeof(rtaddr));
    bzero(&rtmask, sizeof(rtmask));
    return(aa_addsingleroute(ifa, &rtaddr, &rtmask));
  }

  if (top <= bot)
    panic("aa_addrangeroute");

/* Mask out the high order bits on which both bounds agree */

  for (mask = 0xffff; (bot & mask) != (top & mask); mask <<= 1);
  base = bot & mask;
  a = bot & ~mask;
  b = top & ~mask;

/* Find suitable powers of two between a and b we can make a route with */

  for (bbit = 0x8000; bbit > b; bbit >>= 1);
  if (a == 0)
    abit = 0;
  else
  {
    for (abit = 0x0001; a > abit; abit <<= 1);
    if ((abit << 1) > bbit)
      bbit = abit;
    else
      bbit = abit << 1;
  }

/* Now we have a "square" middle chunk from abit to bbit, possibly empty */

  mbot = base + abit;
  mtop = base + bbit;
  mask = ~(bbit - 1);

/* Route to the middle chunk */

  if (mbot < mtop)
  {
    bzero(&rtaddr, sizeof(rtaddr));
    bzero(&rtmask, sizeof(rtmask));
    rtaddr.s_net = htons((u_short) mbot);
    rtmask.s_net = htons((u_short) mask);
    if ((error = aa_addsingleroute(ifa, &rtaddr, &rtmask)))
      return(error);
  }

/* Recurse on the upper and lower chunks we didn't get to */

  if (bot < mbot)
    if ((error = aa_addrangeroute(ifa, bot, mbot)))
    {
      if (mbot < mtop)
	aa_delsingleroute(ifa, &rtaddr, &rtmask);
      return(error);
    }
  if (mtop < top)
    if ((error = aa_addrangeroute(ifa, mtop, top)))
    {
      if (mbot < mtop)
	aa_delsingleroute(ifa, &rtaddr, &rtmask);
      return(error);
    }
  return(0);
}

static int
aa_addsingleroute(struct ifaddr *ifa,
	struct at_addr *addr, struct at_addr *mask)
{
  int	error;

  printf("aa_addsingleroute: %x.%x mask %x.%x ...\n",
    ntohs(addr->s_net), addr->s_node,
    ntohs(mask->s_net), mask->s_node);

  error = aa_dosingleroute(ifa, addr, mask, RTM_ADD, RTF_UP);
  if (error)
    printf("error %d\n", error);
  return(error);
}

static int
aa_delsingleroute(struct ifaddr *ifa,
	struct at_addr *addr, struct at_addr *mask)
{
  int	error;

  error = aa_dosingleroute(ifa, addr, mask, RTM_DELETE, 0);
  if (error)
    printf("aa_delsingleroute: error %d\n", error);
  return(error);
}

static int
aa_dosingleroute(struct ifaddr *ifa,
	struct at_addr *at_addr, struct at_addr *at_mask, int cmd, int flags)
{
  struct sockaddr_at	addr, mask;

  bzero(&addr, sizeof(addr));
  bzero(&mask, sizeof(mask));
  addr.sat_family = AF_APPLETALK;
  addr.sat_len = sizeof(struct sockaddr_at);
  addr.sat_addr.s_net = at_addr->s_net;
  addr.sat_addr.s_node = at_addr->s_node;
  mask.sat_addr.s_net = at_mask->s_net;
  mask.sat_addr.s_node = at_mask->s_node;
  if (at_mask->s_node)
    flags |= RTF_HOST;
  return(rtrequest(cmd, (struct sockaddr *) &addr, ifa->ifa_addr,
		(struct sockaddr *) &mask, flags, NULL));
}

#if 0

static void
aa_clean(void)
{
    struct at_ifaddr	*aa;
    struct ifaddr	*ifa;
    struct ifnet	*ifp;

    while ( aa = at_ifaddr ) {
	ifp = aa->aa_ifp;
	at_scrub( ifp, aa );
	at_ifaddr = aa->aa_next;
	if (( ifa = ifp->if_addrlist ) == (struct ifaddr *)aa ) {
	    ifp->if_addrlist = ifa->ifa_next;
	} else {
	    while ( ifa->ifa_next &&
		    ( ifa->ifa_next != (struct ifaddr *)aa )) {
		ifa = ifa->ifa_next;
	    }
	    if ( ifa->ifa_next ) {
		ifa->ifa_next = ((struct ifaddr *)aa)->ifa_next;
	    } else {
		panic( "at_entry" );
	    }
	}
    }
}

#endif

