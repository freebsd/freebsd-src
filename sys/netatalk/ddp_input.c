/*
 * Copyright (c) 1990,1994 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <net/netisr.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/syslog.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <netatalk/at.h>
#include <netatalk/at_var.h>
#include <netatalk/endian.h>
#include <netatalk/ddp.h>
#include <netatalk/ddp_var.h>
#include <netatalk/at_extern.h>

int		ddp_forward = 1;
int		ddp_firewall = 0;
extern int	ddp_cksum;
void     ddp_input( struct mbuf	*, struct ifnet *, struct elaphdr *, int );

/*
 * Could probably merge these two code segments a little better...
 */
static void
atintr( void )
{
    struct elaphdr	*elhp, elh;
    struct ifnet	*ifp;
    struct mbuf		*m;
    struct at_ifaddr	*aa;
    int			s;

    /*
     * First pull off all the phase 2 packets.
     */
    for (;;) {
	s = splimp();

	IF_DEQUEUE( &atintrq2, m );

	splx( s );

	if ( m == 0 ) {			/* no more queued packets */
	    break;
	}

	ifp = m->m_pkthdr.rcvif;
	ddp_input( m, ifp, (struct elaphdr *)NULL, 2 );
    }

    /*
     * Then pull off all the phase 1 packets.
     */
    for (;;) {
	s = splimp();

	IF_DEQUEUE( &atintrq1, m );

	splx( s );

	if ( m == 0 ) {			/* no more queued packets */
	    break;
	}

	ifp = m->m_pkthdr.rcvif;

	if ( m->m_len < SZ_ELAPHDR &&
		(( m = m_pullup( m, SZ_ELAPHDR )) == 0 )) {
	    ddpstat.ddps_tooshort++;
	    continue;
	}

	/*
	 * this seems a little dubios, but I don't know phase 1 so leave it.
	 */
	elhp = mtod( m, struct elaphdr *);
	m_adj( m, SZ_ELAPHDR );

	if ( elhp->el_type == ELAP_DDPEXTEND ) {
	    ddp_input( m, ifp, (struct elaphdr *)NULL, 1 );
	} else {
	    bcopy((caddr_t)elhp, (caddr_t)&elh, SZ_ELAPHDR );
	    ddp_input( m, ifp, &elh, 1 );
	}
    }
    return;
}

NETISR_SET(NETISR_ATALK, atintr);

struct route	forwro;

void
ddp_input( m, ifp, elh, phase )
    struct mbuf		*m;
    struct ifnet	*ifp;
    struct elaphdr	*elh;
    int			phase;
{
    struct sockaddr_at	from, to;
    struct ddpshdr	*dsh, ddps;
    struct at_ifaddr	*aa;
    struct ddpehdr	*deh = NULL, ddpe;
    struct ddpcb	*ddp;
    int			dlen, mlen;
    u_short		cksum = 0;

    bzero( (caddr_t)&from, sizeof( struct sockaddr_at ));
    bzero( (caddr_t)&to, sizeof( struct sockaddr_at ));
    if ( elh ) {
	/*
	 * Extract the information in the short header.
	 * netowrk information is defaulted to ATADDR_ANYNET
	 * and node information comes from the elh info.
	 * We must be phase 1.
	 */
	ddpstat.ddps_short++;

	if ( m->m_len < sizeof( struct ddpshdr ) &&
		(( m = m_pullup( m, sizeof( struct ddpshdr ))) == 0 )) {
	    ddpstat.ddps_tooshort++;
	    return;
	}

	dsh = mtod( m, struct ddpshdr *);
	bcopy( (caddr_t)dsh, (caddr_t)&ddps, sizeof( struct ddpshdr ));
	ddps.dsh_bytes = ntohl( ddps.dsh_bytes );
	dlen = ddps.dsh_len;

	to.sat_addr.s_net = ATADDR_ANYNET;
	to.sat_addr.s_node = elh->el_dnode;
	to.sat_port = ddps.dsh_dport;
	from.sat_addr.s_net = ATADDR_ANYNET;
	from.sat_addr.s_node = elh->el_snode;
	from.sat_port = ddps.dsh_sport;

	/* 
	 * Make sure that we point to the phase1 ifaddr info 
	 * and that it's valid for this packet.
	 */
	for ( aa = at_ifaddr; aa; aa = aa->aa_next ) {
	    if ( (aa->aa_ifp == ifp)
	    && ( (aa->aa_flags & AFA_PHASE2) == 0)
	    && ( (to.sat_addr.s_node == AA_SAT( aa )->sat_addr.s_node)
	      || (to.sat_addr.s_node == ATADDR_BCAST))) {
		break;
	    }
	}
	/* 
	 * maybe we got a broadcast not meant for us.. ditch it.
	 */
	if ( aa == NULL ) {
	    m_freem( m );
	    return;
	}
    } else {
	/*
	 * There was no 'elh' passed on. This could still be
	 * either phase1 or phase2.
	 * We have a long header, but we may be running on a phase 1 net.
	 * Extract out all the info regarding this packet's src & dst.
	 */
	ddpstat.ddps_long++;

	if ( m->m_len < sizeof( struct ddpehdr ) &&
		(( m = m_pullup( m, sizeof( struct ddpehdr ))) == 0 )) {
	    ddpstat.ddps_tooshort++;
	    return;
	}

	deh = mtod( m, struct ddpehdr *);
	bcopy( (caddr_t)deh, (caddr_t)&ddpe, sizeof( struct ddpehdr ));
	ddpe.deh_bytes = ntohl( ddpe.deh_bytes );
	dlen = ddpe.deh_len;

	if (( cksum = ddpe.deh_sum ) == 0 ) {
	    ddpstat.ddps_nosum++;
	}

	from.sat_addr.s_net = ddpe.deh_snet;
	from.sat_addr.s_node = ddpe.deh_snode;
	from.sat_port = ddpe.deh_sport;
	to.sat_addr.s_net = ddpe.deh_dnet;
	to.sat_addr.s_node = ddpe.deh_dnode;
	to.sat_port = ddpe.deh_dport;

	if ( to.sat_addr.s_net == ATADDR_ANYNET ) {
	    /*
	     * The TO address doesn't specify a net,
	     * So by definition it's for this net.
	     * Try find ifaddr info with the right phase, 
	     * the right interface, and either to our node, a broadcast,
	     * or looped back (though that SHOULD be covered in the other
	     * cases).
	     *
	     * XXX If we have multiple interfaces, then the first with
	     * this node number will match (which may NOT be what we want,
	     * but it's probably safe in 99.999% of cases.
	     */
	    for ( aa = at_ifaddr; aa; aa = aa->aa_next ) {
		if ( phase == 1 && ( aa->aa_flags & AFA_PHASE2 )) {
		    continue;
		}
		if ( phase == 2 && ( aa->aa_flags & AFA_PHASE2 ) == 0 ) {
		    continue;
		}
		if ( (aa->aa_ifp == ifp)
		&& ( (to.sat_addr.s_node == AA_SAT( aa )->sat_addr.s_node)
		  || (to.sat_addr.s_node == ATADDR_BCAST)
		  || (ifp->if_flags & IFF_LOOPBACK))) {
		    break;
		}
	    }
	} else {
	    /* 
	     * A destination network was given. We just try to find 
	     * which ifaddr info matches it.
	     */
	    for ( aa = at_ifaddr; aa; aa = aa->aa_next ) {
		/*
		 * This is a kludge. Accept packets that are
		 * for any router on a local netrange.
		 */
		if ( to.sat_addr.s_net == aa->aa_firstnet &&
			to.sat_addr.s_node == 0 ) {
		    break;
		}
		/*
		 * Don't use ifaddr info for which we are totally outside the
		 * netrange, and it's not a startup packet.
		 * Startup packets are always implicitly allowed on to
		 * the next test.
		 */
		if ((( ntohs( to.sat_addr.s_net ) < ntohs( aa->aa_firstnet ))
		    || (ntohs( to.sat_addr.s_net ) > ntohs( aa->aa_lastnet )))
		 && (( ntohs( to.sat_addr.s_net ) < 0xff00)
		    || (ntohs( to.sat_addr.s_net ) > 0xfffe ))) {
		    continue;
		}

		/*
		 * Don't record a match either if we just don't have a match
		 * in the node address. This can have if the interface
		 * is in promiscuous mode for example.
		 */
		if (( to.sat_addr.s_node != AA_SAT( aa )->sat_addr.s_node)
		&& (to.sat_addr.s_node != ATADDR_BCAST) ) {
		    continue;
		}
		break;
	    }
	}
    }

    /*
     * Adjust the length, removing any padding that may have been added
     * at a link layer.  We do this before we attempt to forward a packet,
     * possibly on a different media.
     */
    mlen = m->m_pkthdr.len;
    if ( mlen < dlen ) {
	ddpstat.ddps_toosmall++;
	m_freem( m );
	return;
    }
    if ( mlen > dlen ) {
	m_adj( m, dlen - mlen );
    }

    /*
     * If it aint for a net on any of our interfaces,
     * or it IS for a net on a different interface than it came in on,
     * (and it is not looped back) then consider if we should forward it.
     * As we are not really a router this is a bit cheeky, but it may be
     * useful some day.
     */
    if ( (aa == NULL)
    || ( (to.sat_addr.s_node == ATADDR_BCAST)
      && (aa->aa_ifp != ifp)
      && (( ifp->if_flags & IFF_LOOPBACK ) == 0 ))) {
	/* 
	 * If we've explicitly disabled it, don't route anything
	 */
	if ( ddp_forward == 0 ) {
	    m_freem( m );
	    return;
	}
	/* 
	 * If the cached forwarding route is still valid, use it.
	 */
	if ( forwro.ro_rt
	&& ( satosat(&forwro.ro_dst)->sat_addr.s_net != to.sat_addr.s_net
	  || satosat(&forwro.ro_dst)->sat_addr.s_node != to.sat_addr.s_node )) {
	    RTFREE( forwro.ro_rt );
	    forwro.ro_rt = (struct rtentry *)0;
	}

	/*
	 * If we don't have a cached one (any more) or it's useless,
	 * Then get a new route.
	 * XXX this could cause a 'route leak'. check this!
	 */
	if ( forwro.ro_rt == (struct rtentry *)0
	|| forwro.ro_rt->rt_ifp == (struct ifnet *)0 ) {
	    forwro.ro_dst.sa_len = sizeof( struct sockaddr_at );
	    forwro.ro_dst.sa_family = AF_APPLETALK;
	    satosat(&forwro.ro_dst)->sat_addr.s_net = to.sat_addr.s_net;
	    satosat(&forwro.ro_dst)->sat_addr.s_node = to.sat_addr.s_node;
	    rtalloc(&forwro);
	}

	/* 
	 * If it's not going to get there on this hop, and it's
	 * already done too many hops, then throw it away.
	 */
	if ( (to.sat_addr.s_net != satosat( &forwro.ro_dst )->sat_addr.s_net)
	&& (ddpe.deh_hops == DDP_MAXHOPS) ) {
	    m_freem( m );
	    return;
	}

	/*
	 * A ddp router might use the same interface
	 * to forward the packet, which this would not effect.
	 * Don't allow packets to cross from one interface to another however.
	 */
	if ( ddp_firewall
	&& ( (forwro.ro_rt == NULL)
	  || (forwro.ro_rt->rt_ifp != ifp))) {
	    m_freem( m );
	    return;
	}

	/*
	 * Adjust the header.
	 * If it was a short header then it would have not gotten here,
	 * so we can assume there is room to drop the header in.
	 * XXX what about promiscuous mode, etc...
	 */
	ddpe.deh_hops++;
	ddpe.deh_bytes = htonl( ddpe.deh_bytes );
	bcopy( (caddr_t)&ddpe, (caddr_t)deh, sizeof( u_short )); /* XXX deh? */
	if ( ddp_route( m, &forwro )) {
	    ddpstat.ddps_cantforward++;
	} else {
	    ddpstat.ddps_forward++;
	}
	return;
    }

    /*
     * It was for us, and we have an ifaddr to use with it.
     */
    from.sat_len = sizeof( struct sockaddr_at );
    from.sat_family = AF_APPLETALK;

    /* 
     * We are no longer interested in the link layer.
     * so cut it off.
     */
    if ( elh ) {
	m_adj( m, sizeof( struct ddpshdr ));
    } else {
	if ( ddp_cksum && cksum && cksum != at_cksum( m, sizeof( int ))) {
	    ddpstat.ddps_badsum++;
	    m_freem( m );
	    return;
	}
	m_adj( m, sizeof( struct ddpehdr ));
    }

    /* 
     * Search for ddp protocol control blocks that match these
     * addresses. 
     */
    if (( ddp = ddp_search( &from, &to, aa )) == NULL ) {
	m_freem( m );
	return;
    }

    /* 
     * If we found one, deliver th epacket to the socket
     */
    if ( sbappendaddr( &ddp->ddp_socket->so_rcv, (struct sockaddr *)&from,
	    m, (struct mbuf *)0 ) == 0 ) {
	/* 
	 * If the socket is full (or similar error) dump the packet.
	 */
	ddpstat.ddps_nosockspace++;
	m_freem( m );
	return;
    }
    /*
     * And wake up whatever might be waiting for it
     */
    sorwakeup( ddp->ddp_socket );
}

#if 0
/* As if we haven't got enough of this sort of think floating
around the kernel :) */

#define BPXLEN	48
#define BPALEN	16
#include <ctype.h>
char	hexdig[] = "0123456789ABCDEF";

static void
bprint( char *data, int len )
{
    char	xout[ BPXLEN ], aout[ BPALEN ];
    int		i = 0;

    bzero( xout, BPXLEN );
    bzero( aout, BPALEN );

    for ( ;; ) {
	if ( len < 1 ) {
	    if ( i != 0 ) {
		printf( "%s\t%s\n", xout, aout );
	    }
	    printf( "%s\n", "(end)" );
	    break;
	}

	xout[ (i*3) ] = hexdig[ ( *data & 0xf0 ) >> 4 ];
	xout[ (i*3) + 1 ] = hexdig[ *data & 0x0f ];

	if ( (u_char)*data < 0x7f && (u_char)*data > 0x20 ) {
	    aout[ i ] = *data;
	} else {
	    aout[ i ] = '.';
	}

	xout[ (i*3) + 2 ] = ' ';

	i++;
	len--;
	data++;

	if ( i > BPALEN - 2 ) {
	    printf( "%s\t%s\n", xout, aout );
	    bzero( xout, BPXLEN );
	    bzero( aout, BPALEN );
	    i = 0;
	    continue;
	}
    }
}

static void
m_printm( struct mbuf *m )
{
    for (; m; m = m->m_next ) {
	bprint( mtod( m, char * ), m->m_len );
    }
}
#endif
