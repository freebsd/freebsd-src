/*
 * Copyright (c) 1990,1991 Regents of The University of Michigan.
 * All Rights Reserved.
 */

#include <sys/param.h>
#include <sys/systm.h>
#ifdef ibm032
#include <sys/dir.h>
#endif ibm032
#include <sys/proc.h>
#ifndef BSD4_4
#include <sys/user.h>
#endif
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/mbuf.h>
#ifndef _IBMR2
#include <sys/kernel.h>
#endif _IBMR2
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <net/if.h>
/* #include <net/af.h> */
#include <net/route.h>
#include <netinet/in.h>
#undef s_net
#include <netinet/if_ether.h>
#ifdef _IBMR2
#include <net/spl.h>
#endif _IBMR2

#include "at.h"
#include "at_var.h"
#include "aarp.h"
#include "phase2.h"
#include <netatalk/at_extern.h>

static int at_scrub( struct ifnet *ifp, struct at_ifaddr *aa );
static int at_ifinit( struct ifnet *ifp, struct at_ifaddr *aa,
					struct sockaddr_at *sat );

#ifdef BSD4_4
# define sateqaddr(a,b)	((a)->sat_len == (b)->sat_len && \
		    (a)->sat_family == (b)->sat_family && \
		    (a)->sat_addr.s_net == (b)->sat_addr.s_net && \
		    (a)->sat_addr.s_node == (b)->sat_addr.s_node )
#else BSD4_4
atalk_hash( sat, hp )
    struct sockaddr_at	*sat;
    struct afhash	*hp;
{
    hp->afh_nethash = sat->sat_addr.s_net;
    hp->afh_hosthash = ( sat->sat_addr.s_net << 8 ) +
	    sat->sat_addr.s_node;
}

/*
 * Note the magic to get ifa_ifwithnet() to work without adding an
 * ifaddr entry for each net in our local range.
 */
int
atalk_netmatch( sat1, sat2 )
    struct sockaddr_at	*sat1, *sat2;
{
    struct at_ifaddr	*aa;

    for ( aa = at_ifaddr; aa; aa = aa->aa_next ) {
	if ( AA_SAT( aa ) == sat1 ) {
	    break;
	}
    }
    if ( aa ) {
	return( ntohs( aa->aa_firstnet ) <= ntohs( sat2->sat_addr.s_net ) &&
		ntohs( aa->aa_lastnet ) >= ntohs( sat2->sat_addr.s_net ));
    }
    return( sat1->sat_addr.s_net == sat2->sat_addr.s_net );
}
#endif BSD4_4

int
at_control( int cmd, caddr_t data, struct ifnet *ifp, struct proc *p )
{
    struct ifreq	*ifr = (struct ifreq *)data;
    struct sockaddr_at	*sat;
    struct netrange	*nr;
#ifdef BSD4_4
    struct at_aliasreq	*ifra = (struct at_aliasreq *)data;
    struct at_ifaddr	*aa0;
#endif BSD4_4
    struct at_ifaddr	*aa = 0;
    struct mbuf		*m;
    struct ifaddr	*ifa;

    if ( ifp ) {
	for ( aa = at_ifaddr; aa; aa = aa->aa_next ) {
	    if ( aa->aa_ifp == ifp ) break;
	}
    }

    switch ( cmd ) {
#ifdef BSD4_4
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
#endif BSD4_4

    case SIOCSIFADDR:
#ifdef BSD4_4
	/*
	 * What a great idea this is: Let's reverse the meaning of
	 * the return...
	 */
#if defined( __FreeBSD__ )
	if ( suser(p->p_ucred, &p->p_acflag) ) {
#else
	if ( suser( u.u_cred, &u.u_acflag )) {
#endif
	    return( EPERM );
	}
#else BSD4_4
	if ( !suser()) {
	    return( EPERM );
	}
#endif BSD4_4

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

#ifdef BSD4_4
	    aa->aa_ifa.ifa_addr = (struct sockaddr *)&aa->aa_addr;
	    aa->aa_ifa.ifa_dstaddr = (struct sockaddr *)&aa->aa_addr;
	    aa->aa_ifa.ifa_netmask = (struct sockaddr *)&aa->aa_netmask;
#endif BSD4_4

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
#ifdef BSD4_4
	*(struct sockaddr_at *)&ifr->ifr_addr = aa->aa_addr;
#else BSD4_4
	ifr->ifr_addr = aa->aa_addr;
#endif BSD4_4
	break;

    case SIOCSIFADDR:
	return( at_ifinit( ifp, aa, (struct sockaddr_at *)&ifr->ifr_addr ));

#ifdef BSD4_4
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
#endif BSD4_4

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
#ifndef BSD4_4
    struct sockaddr_at	netsat;
    u_short		net;
#endif BSD4_4
    int			error;

    if ( aa->aa_flags & AFA_ROUTE ) {
#ifdef BSD4_4
	if (( error = rtinit( &(aa->aa_ifa), RTM_DELETE,
		( ifp->if_flags & IFF_LOOPBACK ) ? RTF_HOST : 0 )) != 0 ) {
	    return( error );
	}
	aa->aa_ifa.ifa_flags &= ~IFA_ROUTE;
#else BSD4_4
	if ( ifp->if_flags & IFF_LOOPBACK ) {
	    rtinit( &aa->aa_addr, &aa->aa_addr, SIOCDELRT, RTF_HOST );
	} else {
	    bzero( &netsat, sizeof( struct sockaddr_at ));
	    netsat.sat_family = AF_APPLETALK;
	    netsat.sat_addr.s_node = ATADDR_ANYNODE;

	    /*
	     * If the range is the full 0-fffe range, just use
	     * the default route.
	     */
	    if ( aa->aa_firstnet == htons( 0x0000 ) &&
		    aa->aa_lastnet == htons( 0xfffe )) {
		netsat.sat_addr.s_net = 0;
		rtinit((struct sockaddr *)&netsat, &aa->aa_addr,
			(int)SIOCDELRT, 0 );
	    } else {
		for ( net = ntohs( aa->aa_firstnet );
			net <= ntohs( aa->aa_lastnet ); net++ ) {
		    netsat.sat_addr.s_net = htons( net );
		    rtinit((struct sockaddr *)&netsat, &aa->aa_addr,
			    (int)SIOCDELRT, 0 );
		}
	    }
	}
#endif BSD4_4
	aa->aa_flags &= ~AFA_ROUTE;
    }
    return( 0 );
}

#if !defined( __FreeBSD__ )
extern struct timeval	time;
#endif __FreeBSD__

static int 
at_ifinit( ifp, aa, sat )
    struct ifnet	*ifp;
    struct at_ifaddr	*aa;
    struct sockaddr_at	*sat;
{
    struct netrange	nr, onr;
#ifdef BSD4_4
    struct sockaddr_at	oldaddr;
#else BSD4_4
    struct sockaddr	oldaddr;
#endif BSD4_4
    struct sockaddr_at	netaddr;
    int			s = splimp(), error = 0, i, j, netinc, nodeinc, nnets;
    u_short		net;

    oldaddr = aa->aa_addr;
    bzero( AA_SAT( aa ), sizeof( struct sockaddr_at ));
    bcopy( sat->sat_zero, &nr, sizeof( struct netrange ));
    nnets = ntohs( nr.nr_lastnet ) - ntohs( nr.nr_firstnet ) + 1;

    onr.nr_firstnet = aa->aa_firstnet;
    onr.nr_lastnet = aa->aa_lastnet;
    aa->aa_firstnet = nr.nr_firstnet;
    aa->aa_lastnet = nr.nr_lastnet;

    /*
     * We could eliminate the need for a second phase 1 probe (post
     * autoconf) if we check whether we're resetting the node. Note
     * that phase 1 probes use only nodes, not net.node pairs.  Under
     * phase 2, both the net and node must be the same.
     */
    if ( ifp->if_flags & IFF_LOOPBACK ) {
#ifdef BSD4_4
	AA_SAT( aa )->sat_len = sat->sat_len;
#endif BSD4_4
	AA_SAT( aa )->sat_family = AF_APPLETALK;
	AA_SAT( aa )->sat_addr.s_net = sat->sat_addr.s_net;
	AA_SAT( aa )->sat_addr.s_node = sat->sat_addr.s_node;
    } else {
	aa->aa_flags |= AFA_PROBING;
#ifdef BSD4_4
	AA_SAT( aa )->sat_len = sizeof(struct sockaddr_at);
#endif BSD4_4
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
		if (
#if defined( __FreeBSD__ )
		    tsleep( aa, PPAUSE|PCATCH, "at_ifinit", 0 )
#else
		    sleep( aa, PSLEP|PCATCH )
#endif
		    ) {
		    printf( "at_ifinit why did this happen?!\n" );
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
	splx( s );
	aa->aa_addr = oldaddr;
	aa->aa_firstnet = onr.nr_firstnet;
	aa->aa_lastnet = onr.nr_lastnet;
	return( error );
    }

#ifdef BSD4_4
    aa->aa_netmask.sat_len = 6/*sizeof(struct sockaddr_at)*/;
    aa->aa_netmask.sat_family = AF_APPLETALK;
    aa->aa_netmask.sat_addr.s_net = 0xffff;
    aa->aa_netmask.sat_addr.s_node = 0;
#if defined( __FreeBSD__ )
    aa->aa_ifa.ifa_netmask =(struct sockaddr *) &(aa->aa_netmask); /* XXX */
#endif __FreeBSD__
#endif BSD4_4

    if ( ifp->if_flags & IFF_LOOPBACK ) {
#ifndef BSD4_4
	rtinit( &aa->aa_addr, &aa->aa_addr, (int)SIOCADDRT,
		RTF_HOST|RTF_UP );
#else BSD4_4
	error = rtinit( &(aa->aa_ifa), (int)RTM_ADD,
#if !defined( __FreeBSD__ )
				RTF_HOST |
#else
				/* XXX not a host route? */
#endif __FreeBSD__
					RTF_UP );
#endif BSD4_4
    } else {
#ifndef BSD4_4
	/*
	 * rtrequest looks for point-to-point links first. The
	 * broadaddr is in the same spot as the destaddr. So, if
	 * ATADDR_ANYNET is 0, and we don't fill in the broadaddr, we
	 * get 0.0 routed out the ether interface.  So, initialize the
	 * broadaddr, even tho we don't use it.
	 *
	 * We *could* use the broadaddr field to reduce some of the
	 * sockaddr_at overloading that we've done.  E.g. Just send
	 * to INTERFACE-NET.255, and have the kernel reroute that
	 * to broadaddr, which would be 0.255 for phase 2 interfaces,
	 * and IFACE-NET.255 for phase 1 interfaces.
	 */
	((struct sockaddr_at *)&aa->aa_broadaddr)->sat_addr.s_net =
		sat->sat_addr.s_net;
	((struct sockaddr_at *)&aa->aa_broadaddr)->sat_addr.s_node =
		ATADDR_BCAST;

	bzero( &netaddr, sizeof( struct sockaddr_at ));
	netaddr.sat_family = AF_APPLETALK;
	netaddr.sat_addr.s_node = ATADDR_ANYNODE;
	if (( aa->aa_flags & AFA_PHASE2 ) == 0 ) {
	    netaddr.sat_addr.s_net = AA_SAT( aa )->sat_addr.s_net;
	    rtinit((struct sockaddr *)&netaddr, &aa->aa_addr,
		    (int)SIOCADDRT, RTF_UP );
	} else {
	    /*
	     * If the range is the full 0-fffe range, just use
	     * the default route.
	     */
	    if ( aa->aa_firstnet == htons( 0x0000 ) &&
		    aa->aa_lastnet == htons( 0xfffe )) {
		netaddr.sat_addr.s_net = 0;
		rtinit((struct sockaddr *)&netaddr, &aa->aa_addr,
			(int)SIOCADDRT, RTF_UP );
	    } else {
		for ( net = ntohs( aa->aa_firstnet );
			net <= ntohs( aa->aa_lastnet ); net++ ) {
		    netaddr.sat_addr.s_net = htons( net );
		    rtinit((struct sockaddr *)&netaddr, &aa->aa_addr,
			    (int)SIOCADDRT, RTF_UP );
		}
	    }
	}
#else BSD4_4
	error = rtinit( &(aa->aa_ifa), (int)RTM_ADD, RTF_UP );
#endif BSD4_4
    }
    if ( error ) {
	aa->aa_addr = oldaddr;
	aa->aa_firstnet = onr.nr_firstnet;
	aa->aa_lastnet = onr.nr_lastnet;
	splx( s );
	return( error );
    }

#ifdef BSD4_4
    aa->aa_ifa.ifa_flags |= IFA_ROUTE;
#endif BSD4_4
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
