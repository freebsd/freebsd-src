/*
 * Copyright (c) 1990,1991 Regents of The University of Michigan.
 * All Rights Reserved.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#undef s_net
#include <netinet/if_ether.h>

#include <netatalk/at.h>
#include <netatalk/at_var.h>
#include <netatalk/at_extern.h>

struct at_ifaddr	*at_ifaddr;

static int aa_dorangeroute(struct ifaddr *ifa,
			u_int first, u_int last, int cmd);
static int aa_addsingleroute(struct ifaddr *ifa,
			struct at_addr *addr, struct at_addr *mask);
static int aa_delsingleroute(struct ifaddr *ifa,
			struct at_addr *addr, struct at_addr *mask);
static int aa_dosingleroute(struct ifaddr *ifa, struct at_addr *addr,
			struct at_addr *mask, int cmd, int flags);
static int at_scrub( struct ifnet *ifp, struct at_ifaddr *aa );
static int at_ifinit( struct ifnet *ifp, struct at_ifaddr *aa,
					struct sockaddr_at *sat );
static int aa_claim_addr(struct ifaddr *ifa, struct sockaddr *gw);

# define sateqaddr(a,b)	((a)->sat_len == (b)->sat_len && \
		    (a)->sat_family == (b)->sat_family && \
		    (a)->sat_addr.s_net == (b)->sat_addr.s_net && \
		    (a)->sat_addr.s_node == (b)->sat_addr.s_node )

int
at_control(struct socket *so, u_long cmd, caddr_t data,
		struct ifnet *ifp, struct thread *td )
{
    struct ifreq	*ifr = (struct ifreq *)data;
    struct sockaddr_at	*sat;
    struct netrange	*nr;
    struct at_aliasreq	*ifra = (struct at_aliasreq *)data;
    struct at_ifaddr	*aa0;
    struct at_ifaddr	*aa = 0;
    struct ifaddr	*ifa, *ifa0;

    /*
     * If we have an ifp, then find the matching at_ifaddr if it exists
     */
    if ( ifp ) {
	for ( aa = at_ifaddr; aa; aa = aa->aa_next ) {
	    if ( aa->aa_ifp == ifp ) break;
	}
    }

    /*
     * In this first switch table we are basically getting ready for
     * the second one, by getting the atalk-specific things set up
     * so that they start to look more similar to other protocols etc.
     */

    switch ( cmd ) {
    case SIOCAIFADDR:
    case SIOCDIFADDR:
	/*
	 * If we have an appletalk sockaddr, scan forward of where
	 * we are now on the at_ifaddr list to find one with a matching 
	 * address on this interface.
	 * This may leave aa pointing to the first address on the
	 * NEXT interface!
	 */
	if ( ifra->ifra_addr.sat_family == AF_APPLETALK ) {
	    for ( ; aa; aa = aa->aa_next ) {
		if ( aa->aa_ifp == ifp &&
			sateqaddr( &aa->aa_addr, &ifra->ifra_addr )) {
		    break;
		}
	    }
	}
	/*
	 * If we a retrying to delete an addres but didn't find such,
	 * then rewurn with an error
	 */
	if ( cmd == SIOCDIFADDR && aa == 0 ) {
	    return( EADDRNOTAVAIL );
	}
	/*FALLTHROUGH*/

    case SIOCSIFADDR:
	/* 
	 * If we are not superuser, then we don't get to do these ops.
	 */
	if ( suser(td) ) {
	    return( EPERM );
	}

	sat = satosat( &ifr->ifr_addr );
	nr = (struct netrange *)sat->sat_zero;
	if ( nr->nr_phase == 1 ) {
	    /*
	     * Look for a phase 1 address on this interface.
	     * This may leave aa pointing to the first address on the
	     * NEXT interface!
	     */
	    for ( ; aa; aa = aa->aa_next ) {
		if ( aa->aa_ifp == ifp &&
			( aa->aa_flags & AFA_PHASE2 ) == 0 ) {
		    break;
		}
	    }
	} else {		/* default to phase 2 */
	    /*
	     * Look for a phase 2 address on this interface.
	     * This may leave aa pointing to the first address on the
	     * NEXT interface!
	     */
	    for ( ; aa; aa = aa->aa_next ) {
		if ( aa->aa_ifp == ifp && ( aa->aa_flags & AFA_PHASE2 )) {
		    break;
		}
	    }
	}

	if ( ifp == 0 )
	    panic( "at_control" );

	/*
	 * If we failed to find an existing at_ifaddr entry, then we 
	 * allocate a fresh one. 
	 */
	if ( aa == (struct at_ifaddr *) 0 ) {
	    aa0 = malloc(sizeof(struct at_ifaddr), M_IFADDR, M_WAITOK | M_ZERO);
	    if (( aa = at_ifaddr ) != NULL ) {
		/*
		 * Don't let the loopback be first, since the first
		 * address is the machine's default address for
		 * binding.
		 * If it is, stick ourself in front, otherwise
		 * go to the back of the list.
		 */
		if ( at_ifaddr->aa_ifp->if_flags & IFF_LOOPBACK ) {
		    aa = aa0;
		    aa->aa_next = at_ifaddr;
		    at_ifaddr = aa;
		} else {
		    for ( ; aa->aa_next; aa = aa->aa_next )
			;
		    aa->aa_next = aa0;
		}
	    } else {
		at_ifaddr = aa0;
	    }
	    aa = aa0;

	    /*
	     * Find the end of the interface's addresses
	     * and link our new one on the end 
	     */
	    ifa = (struct ifaddr *)aa;
	    IFA_LOCK_INIT(ifa);
	    ifa->ifa_refcnt = 1;
	    TAILQ_INSERT_TAIL(&ifp->if_addrhead, ifa, ifa_link);

	    /*
	     * As the at_ifaddr contains the actual sockaddrs,
	     * and the ifaddr itself, link them al together correctly.
	     */
	    ifa->ifa_addr = (struct sockaddr *)&aa->aa_addr;
	    ifa->ifa_dstaddr = (struct sockaddr *)&aa->aa_addr;
	    ifa->ifa_netmask = (struct sockaddr *)&aa->aa_netmask;

	    /*
	     * Set/clear the phase 2 bit.
	     */
	    if ( nr->nr_phase == 1 ) {
		aa->aa_flags &= ~AFA_PHASE2;
	    } else {
		aa->aa_flags |= AFA_PHASE2;
	    }

 	    /*
	     * and link it all together
	     */
	    aa->aa_ifp = ifp;
	} else {
	    /*
	     * If we DID find one then we clobber any routes dependent on it..
	     */
	    at_scrub( ifp, aa );
	}
	break;

    case SIOCGIFADDR :
	sat = satosat( &ifr->ifr_addr );
	nr = (struct netrange *)sat->sat_zero;
	if ( nr->nr_phase == 1 ) {
	    /*
	     * If the request is specifying phase 1, then
	     * only look at a phase one address
	     */
	    for ( ; aa; aa = aa->aa_next ) {
		if ( aa->aa_ifp == ifp &&
			( aa->aa_flags & AFA_PHASE2 ) == 0 ) {
		    break;
		}
	    }
	} else {
	    /*
	     * default to phase 2
	     */
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

    /*
     * By the time this switch is run we should be able to assume that
     * the "aa" pointer is valid when needed.
     */
    switch ( cmd ) {
    case SIOCGIFADDR:

	/*
	 * copy the contents of the sockaddr blindly.
	 */
	sat = (struct sockaddr_at *)&ifr->ifr_addr;
	*sat = aa->aa_addr;

 	/* 
	 * and do some cleanups
	 */
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
	/*
	 * scrub all routes.. didn't we just DO this? XXX yes, del it
	 */
	at_scrub( ifp, aa );

	/*
	 * remove the ifaddr from the interface
	 */
	ifa0 = (struct ifaddr *)aa;
	TAILQ_REMOVE(&ifp->if_addrhead, ifa0, ifa_link);

	/*
	 * refs goes from 1->0 if no external refs. note.. 
	 * This will not free it ... looks for -1.
	 */
	IFAFREE(ifa0);

	/*
	 * Now remove the at_ifaddr from the parallel structure
	 * as well, or we'd be in deep trouble
	 */
	aa0 = aa;
	if ( aa0 == ( aa = at_ifaddr )) {
	    at_ifaddr = aa->aa_next;
	} else {
	    while ( aa->aa_next && ( aa->aa_next != aa0 )) {
		aa = aa->aa_next;
	    }

	    /*
	     * if we found it, remove it, otherwise we screwed up.
	     */
	    if ( aa->aa_next ) {
		aa->aa_next = aa0->aa_next;
	    } else {
		panic( "at_control" );
	    }
	}

	/*
	 * Now dump the memory we were using.
	 * Decrement the reference count.
	 * This should probably be the last reference
	 * as the count will go from 0 to -1.
	 * (unless there is still a route referencing this)
	 */
	IFAFREE(ifa0);
	break;

    default:
	if ( ifp == 0 || ifp->if_ioctl == 0 )
	    return( EOPNOTSUPP );
	return( (*ifp->if_ioctl)( ifp, cmd, data ));
    }
    return( 0 );
}

/* 
 * Given an interface and an at_ifaddr (supposedly on that interface)
 * remove  any routes that depend on this.
 * Why ifp is needed I'm not sure,
 * as aa->at_ifaddr.ifa_ifp should be the same.
 */
static int
at_scrub( ifp, aa )
    struct ifnet	*ifp;
    struct at_ifaddr	*aa;
{
    int			error;

    if ( aa->aa_flags & AFA_ROUTE ) {
	if (ifp->if_flags & IFF_LOOPBACK) {
		if ((error = aa_delsingleroute(&aa->aa_ifa,
					&aa->aa_addr.sat_addr,
					&aa->aa_netmask.sat_addr)) != 0) {
	    		return( error );
		}
	} else if (ifp->if_flags & IFF_POINTOPOINT) {
		if ((error = rtinit( &aa->aa_ifa, RTM_DELETE, RTF_HOST)) != 0)
	    		return( error );
	} else if (ifp->if_flags & IFF_BROADCAST) {
		error = aa_dorangeroute(&aa->aa_ifa,
				ntohs(aa->aa_firstnet),
				ntohs(aa->aa_lastnet),
				RTM_DELETE );
	}
	aa->aa_ifa.ifa_flags &= ~IFA_ROUTE;
	aa->aa_flags &= ~AFA_ROUTE;
    }
    return( 0 );
}

/*
 * given an at_ifaddr,a sockaddr_at and an ifp,
 * bang them all together at high speed and see what happens
 */
static int 
at_ifinit( ifp, aa, sat )
    struct ifnet	*ifp;
    struct at_ifaddr	*aa;
    struct sockaddr_at	*sat;
{
    struct netrange	nr, onr;
    struct sockaddr_at	oldaddr;
    int			s = splimp(), error = 0, i, j;
    int			netinc, nodeinc, nnets;
    u_short		net;

    /* 
     * save the old addresses in the at_ifaddr just in case we need them.
     */
    oldaddr = aa->aa_addr;
    onr.nr_firstnet = aa->aa_firstnet;
    onr.nr_lastnet = aa->aa_lastnet;

    /*
     * take the address supplied as an argument, and add it to the 
     * at_ifnet (also given). Remember ing to update
     * those parts of the at_ifaddr that need special processing
     */
    bzero( AA_SAT( aa ), sizeof( struct sockaddr_at ));
    bcopy( sat->sat_zero, &nr, sizeof( struct netrange ));
    bcopy( sat->sat_zero, AA_SAT( aa )->sat_zero, sizeof( struct netrange ));
    nnets = ntohs( nr.nr_lastnet ) - ntohs( nr.nr_firstnet ) + 1;
    aa->aa_firstnet = nr.nr_firstnet;
    aa->aa_lastnet = nr.nr_lastnet;

/* XXX ALC */
#if 0
    printf("at_ifinit: %s: %u.%u range %u-%u phase %d\n",
	ifp->if_name,
	ntohs(sat->sat_addr.s_net), sat->sat_addr.s_node,
	ntohs(aa->aa_firstnet), ntohs(aa->aa_lastnet),
	(aa->aa_flags & AFA_PHASE2) ? 2 : 1);
#endif

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
	/*
	 * we'd have to copy the dstaddr field over from the sat 
	 * but it's not clear that it would contain the right info..
	 */
#endif
    } else {
	/*
	 * We are a normal (probably ethernet) interface.
	 * apply the new address to the interface structures etc.
	 * We will probe this address on the net first, before
	 * applying it to ensure that it is free.. If it is not, then
	 * we will try a number of other randomly generated addresses
	 * in this net and then increment the net.  etc.etc. until
	 * we find an unused address.
	 */
	aa->aa_flags |= AFA_PROBING; /* if not loopback we Must probe? */
	AA_SAT( aa )->sat_len = sizeof(struct sockaddr_at);
	AA_SAT( aa )->sat_family = AF_APPLETALK;
	if ( aa->aa_flags & AFA_PHASE2 ) {
	    if ( sat->sat_addr.s_net == ATADDR_ANYNET ) {
		/*
		 * If we are phase 2, and the net was not specified
		 * then we select a random net within the supplied netrange.
		 * XXX use /dev/random?
		 */
		if ( nnets != 1 ) {
		    net = ntohs( nr.nr_firstnet ) + time_second % ( nnets - 1 );
		} else {
		    net = ntohs( nr.nr_firstnet );
		}
	    } else {
		/*
		 * if a net was supplied, then check that it is within
		 * the netrange. If it is not then replace the old values
		 * and return an error
		 */
		if ( ntohs( sat->sat_addr.s_net ) < ntohs( nr.nr_firstnet ) ||
			ntohs( sat->sat_addr.s_net ) > ntohs( nr.nr_lastnet )) {
		    aa->aa_addr = oldaddr;
		    aa->aa_firstnet = onr.nr_firstnet;
		    aa->aa_lastnet = onr.nr_lastnet;
		    splx(s);
		    return( EINVAL );
		}
		/*
		 * otherwise just use the new net number..
		 */
		net = ntohs( sat->sat_addr.s_net );
	    }
	} else {
	    /*
	     * we must be phase one, so just use whatever we were given.
	     * I guess it really isn't going to be used... RIGHT?
	     */
	    net = ntohs( sat->sat_addr.s_net );
	}

	/* 
	 * set the node part of the address into the ifaddr.
	 * If it's not specified, be random about it...
	 * XXX use /dev/random?
	 */
	if ( sat->sat_addr.s_node == ATADDR_ANYNODE ) {
	    AA_SAT( aa )->sat_addr.s_node = time_second;
	} else {
	    AA_SAT( aa )->sat_addr.s_node = sat->sat_addr.s_node;
	}

	/* 
	 * Copy the phase.
	 */
	AA_SAT( aa )->sat_range.r_netrange.nr_phase
		= ((aa->aa_flags & AFA_PHASE2) ? 2:1);

	/* 
	 * step through the nets in the range
	 * starting at the (possibly random) start point.
	 */
	for ( i = nnets, netinc = 1; i > 0; net = ntohs( nr.nr_firstnet ) +
		(( net - ntohs( nr.nr_firstnet ) + netinc ) % nnets ), i-- ) {
	    AA_SAT( aa )->sat_addr.s_net = htons( net );

	    /*
	     * using a rather strange stepping method,
	     * stagger through the possible node addresses
	     * Once again, starting at the (possibly random)
	     * initial node address.
	     */
	    for ( j = 0, nodeinc = time_second | 1; j < 256;
		    j++, AA_SAT( aa )->sat_addr.s_node += nodeinc ) {
		if ( AA_SAT( aa )->sat_addr.s_node > 253 ||
			AA_SAT( aa )->sat_addr.s_node < 1 ) {
		    continue;
		}
		aa->aa_probcnt = 10;

		/*
		 * start off the probes as an asynchronous activity.
		 * though why wait 200mSec?
		 */
		aa->aa_ch = timeout( aarpprobe, (caddr_t)ifp, hz / 5 );
		if ( tsleep( aa, PPAUSE|PCATCH, "at_ifinit", 0 )) {
		    /*
		     * theoretically we shouldn't time out here
		     * so if we returned with an error..
		     */
		    printf( "at_ifinit: why did this happen?!\n" );
		    aa->aa_addr = oldaddr;
		    aa->aa_firstnet = onr.nr_firstnet;
		    aa->aa_lastnet = onr.nr_lastnet;
		    splx( s ); 
		    return( EINTR );
		}

		/* 
		 * The async activity should have woken us up.
		 * We need to see if it was successful in finding
		 * a free spot, or if we need to iterate to the next 
		 * address to try.
		 */
		if (( aa->aa_flags & AFA_PROBING ) == 0 ) {
		    break;
		}
	    }

	    /*
	     * of course we need to break out through two loops...
	     */
	    if (( aa->aa_flags & AFA_PROBING ) == 0 ) {
		break;
	    }
	    /* reset node for next network */
	    AA_SAT( aa )->sat_addr.s_node = time_second;
	}

	/*
	 * if we are still trying to probe, then we have finished all
	 * the possible addresses, so we need to give up
	 */

	if ( aa->aa_flags & AFA_PROBING ) {
	    aa->aa_addr = oldaddr;
	    aa->aa_firstnet = onr.nr_firstnet;
	    aa->aa_lastnet = onr.nr_lastnet;
	    splx( s );
	    return( EADDRINUSE );
	}
    }

    /* 
     * Now that we have selected an address, we need to tell the interface
     * about it, just in case it needs to adjust something.
     */
    if ( ifp->if_ioctl &&
	    ( error = (*ifp->if_ioctl)( ifp, SIOCSIFADDR, (caddr_t)aa ))) {
	/*
	 * of course this could mean that it objects violently
	 * so if it does, we back out again..
	 */
	aa->aa_addr = oldaddr;
	aa->aa_firstnet = onr.nr_firstnet;
	aa->aa_lastnet = onr.nr_lastnet;
	splx( s );
	return( error );
    }

    /* 
     * set up the netmask part of the at_ifaddr
     * and point the appropriate pointer in the ifaddr to it.
     * probably pointless, but what the heck.. XXX
     */
    bzero(&aa->aa_netmask, sizeof(aa->aa_netmask));
    aa->aa_netmask.sat_len = sizeof(struct sockaddr_at);
    aa->aa_netmask.sat_family = AF_APPLETALK;
    aa->aa_netmask.sat_addr.s_net = 0xffff;
    aa->aa_netmask.sat_addr.s_node = 0;
    aa->aa_ifa.ifa_netmask =(struct sockaddr *) &(aa->aa_netmask); /* XXX */

    /*
     * Initialize broadcast (or remote p2p) address
     */
    bzero(&aa->aa_broadaddr, sizeof(aa->aa_broadaddr));
    aa->aa_broadaddr.sat_len = sizeof(struct sockaddr_at);
    aa->aa_broadaddr.sat_family = AF_APPLETALK;

    aa->aa_ifa.ifa_metric = ifp->if_metric;
    if (ifp->if_flags & IFF_BROADCAST) {
	aa->aa_broadaddr.sat_addr.s_net = htons(0);
	aa->aa_broadaddr.sat_addr.s_node = 0xff;
        aa->aa_ifa.ifa_broadaddr = (struct sockaddr *) &aa->aa_broadaddr;
	/* add the range of routes needed */
	error = aa_dorangeroute(&aa->aa_ifa,
		ntohs(aa->aa_firstnet), ntohs(aa->aa_lastnet), RTM_ADD );
    }
    else if (ifp->if_flags & IFF_POINTOPOINT) {
	struct at_addr	rtaddr, rtmask;

	bzero(&rtaddr, sizeof(rtaddr));
	bzero(&rtmask, sizeof(rtmask));
	/* fill in the far end if we know it here XXX */
        aa->aa_ifa.ifa_dstaddr = (struct sockaddr *) &aa->aa_dstaddr;
	error = aa_addsingleroute(&aa->aa_ifa, &rtaddr, &rtmask);
    }
    else if ( ifp->if_flags & IFF_LOOPBACK ) {
	struct at_addr	rtaddr, rtmask;

	bzero(&rtaddr, sizeof(rtaddr));
	bzero(&rtmask, sizeof(rtmask));
	rtaddr.s_net = AA_SAT( aa )->sat_addr.s_net;
	rtaddr.s_node = AA_SAT( aa )->sat_addr.s_node;
	rtmask.s_net = 0xffff;
	rtmask.s_node = 0x0; /* XXX should not be so.. should be HOST route */
	error = aa_addsingleroute(&aa->aa_ifa, &rtaddr, &rtmask);
    }


    /*
     * set the address of our "check if this addr is ours" routine.
     */
    aa->aa_ifa.ifa_claim_addr = aa_claim_addr;

    /*
     * of course if we can't add these routes we back out, but it's getting
     * risky by now XXX
     */
    if ( error ) {
	at_scrub( ifp, aa );
	aa->aa_addr = oldaddr;
	aa->aa_firstnet = onr.nr_firstnet;
	aa->aa_lastnet = onr.nr_lastnet;
	splx( s );
	return( error );
    }

    /*
     * note that the address has a route associated with it....
     */
    aa->aa_ifa.ifa_flags |= IFA_ROUTE;
    aa->aa_flags |= AFA_ROUTE;
    splx( s );
    return( 0 );
}

/*
 * check whether a given address is a broadcast address for us..
 */
int
at_broadcast( sat )
    struct sockaddr_at	*sat;
{
    struct at_ifaddr	*aa;

    /*
     * If the node is not right, it can't be a broadcast 
     */
    if ( sat->sat_addr.s_node != ATADDR_BCAST ) {
	return( 0 );
    }

    /*
     * If the node was right then if the net is right, it's a broadcast
     */
    if ( sat->sat_addr.s_net == ATADDR_ANYNET ) {
	return( 1 );
    }

    /*
     * failing that, if the net is one we have, it's a broadcast as well.
     */
    for ( aa = at_ifaddr; aa; aa = aa->aa_next ) {
	if (( aa->aa_ifp->if_flags & IFF_BROADCAST )
	 && ( ntohs( sat->sat_addr.s_net ) >= ntohs( aa->aa_firstnet )
	 && ntohs( sat->sat_addr.s_net ) <= ntohs( aa->aa_lastnet ))) {
			return( 1 );
	}
    }
    return( 0 );
}

/*
 * aa_dorangeroute()
 *
 * Add a route for a range of networks from bot to top - 1.
 * Algorithm:
 *
 * Split the range into two subranges such that the middle
 * of the two ranges is the point where the highest bit of difference
 * between the two addresses makes its transition.
 * Each of the upper and lower ranges might not exist, or might be 
 * representable by 1 or more netmasks. In addition, if both
 * ranges can be represented by the same netmask, then they can be merged
 * by using the next higher netmask..
 */

static int
aa_dorangeroute(struct ifaddr *ifa, u_int bot, u_int top, int cmd)
{
	u_int mask1;
	struct at_addr addr;
	struct at_addr mask;
	int error;

	/*
	 * slight sanity check
	 */
	if (bot > top) return (EINVAL);

	addr.s_node = 0;
	mask.s_node = 0;
	/*
	 * just start out with the lowest boundary
	 * and keep extending the mask till it's too big.
	 */
	
	 while (bot <= top) {
	 	mask1 = 1;
	 	while ((( bot & ~mask1) >= bot)
		   && (( bot | mask1) <= top)) {
			mask1 <<= 1;
			mask1 |= 1;
		}
		mask1 >>= 1;
		mask.s_net = htons(~mask1);
		addr.s_net = htons(bot);
		if(cmd == RTM_ADD) {
		error =	 aa_addsingleroute(ifa,&addr,&mask);
			if (error) {
				/* XXX clean up? */
				return (error);
			}
		} else {
			error =	 aa_delsingleroute(ifa,&addr,&mask);
		}
		bot = (bot | mask1) + 1;
	}
	return 0;
}

static int
aa_addsingleroute(struct ifaddr *ifa,
	struct at_addr *addr, struct at_addr *mask)
{
  int	error;

#if 0
  printf("aa_addsingleroute: %x.%x mask %x.%x ...\n",
    ntohs(addr->s_net), addr->s_node,
    ntohs(mask->s_net), mask->s_node);
#endif

  error = aa_dosingleroute(ifa, addr, mask, RTM_ADD, RTF_UP);
  if (error)
    printf("aa_addsingleroute: error %d\n", error);
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
  mask.sat_family = AF_APPLETALK;
  mask.sat_len = sizeof(struct sockaddr_at);
  mask.sat_addr.s_net = at_mask->s_net;
  mask.sat_addr.s_node = at_mask->s_node;
  if (at_mask->s_node)
    flags |= RTF_HOST;
  return(rtrequest(cmd, (struct sockaddr *) &addr,
	(flags & RTF_HOST)?(ifa->ifa_dstaddr):(ifa->ifa_addr),
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

static int
aa_claim_addr(struct ifaddr *ifa, struct sockaddr *gw0)
{
	struct sockaddr_at *addr = (struct sockaddr_at *)ifa->ifa_addr;
	struct sockaddr_at *gw = (struct sockaddr_at *)gw0;

	switch (gw->sat_range.r_netrange.nr_phase) {
	case 1:
		if(addr->sat_range.r_netrange.nr_phase == 1)
			return 1;
	case 0:
	case 2:
		/*
		 * if it's our net (including 0),
		 * or netranges are valid, and we are in the range,
		 * then it's ours.
		 */
		if ((addr->sat_addr.s_net == gw->sat_addr.s_net)
		|| ((addr->sat_range.r_netrange.nr_lastnet)
		  && (ntohs(gw->sat_addr.s_net)
			>= ntohs(addr->sat_range.r_netrange.nr_firstnet ))
		  && (ntohs(gw->sat_addr.s_net)
			<= ntohs(addr->sat_range.r_netrange.nr_lastnet )))) {
			return 1;
		} 
		break;
	default:
		printf("atalk: bad phase\n");
	}
	return 0;
}
