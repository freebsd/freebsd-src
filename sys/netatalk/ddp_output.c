/*
 * Copyright (c) 1990,1991 Regents of The University of Michigan.
 * All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation, and that the name of The University
 * of Michigan not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. This software is supplied as is without expressed or
 * implied warranties of any kind.
 *
 *	Research Systems Unix Group
 *	The University of Michigan
 *	c/o Mike Clark
 *	535 W. William Street
 *	Ann Arbor, Michigan
 *	+1-313-763-0525
 *	netatalk@itd.umich.edu
 */

#include <sys/types.h>
#include <sys/param.h>
#if defined( __FreeBSD__ )
#include <sys/systm.h>
#endif __FreeBSD__
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#undef s_net
#include <netinet/if_ether.h>

#include "at.h"
#include "at_var.h"
#include "endian.h"
#include "ddp.h"
#include "ddp_var.h"
#include <netatalk/at_extern.h>

int	ddp_cksum = 1;

int
ddp_output( struct ddpcb *ddp, struct mbuf *m)
{
#ifndef BSD4_4
    struct mbuf		*m0;
    int			len;
#endif BSD4_4
    struct ifnet	*ifp;
    struct at_ifaddr	*aa = NULL;
    struct ddpehdr	*deh;
    u_short		net;

#ifdef BSD4_4
    M_PREPEND( m, sizeof( struct ddpehdr ), M_WAIT );
#else BSD4_4
    for ( len = 0, m0 = m; m; m = m->m_next ) {
	len += m->m_len;
    }
    MGET( m, M_WAIT, MT_HEADER );
    if ( m == 0 ) {
	m_freem( m0 );
	return( ENOBUFS );
    }
    m->m_next = m0;
#endif BSD4_4

#ifndef BSD4_4
# define align(a)	(((a)+3)&0xfc)
    m->m_off = MMINOFF + align( SZ_ELAPHDR );
    m->m_len = sizeof( struct ddpehdr );
#endif BSD4_4

    deh = mtod( m, struct ddpehdr *);
    deh->deh_pad = 0;
    deh->deh_hops = 0;

#ifdef BSD4_4
    deh->deh_len = m->m_pkthdr.len;
#else BSD4_4
    deh->deh_len = len + sizeof( struct ddpehdr );
#endif BSD4_4

    deh->deh_dnet = ddp->ddp_fsat.sat_addr.s_net;
    deh->deh_dnode = ddp->ddp_fsat.sat_addr.s_node;
    deh->deh_dport = ddp->ddp_fsat.sat_port;
    deh->deh_snet = ddp->ddp_lsat.sat_addr.s_net;
    deh->deh_snode = ddp->ddp_lsat.sat_addr.s_node;
    deh->deh_sport = ddp->ddp_lsat.sat_port;

    /*
     * The checksum calculation is done after all of the other bytes have
     * been filled in.
     */
    if ( ddp_cksum ) {
	deh->deh_sum = at_cksum( m, sizeof( int ));
    } else {
	deh->deh_sum = 0;
    }
    deh->deh_bytes = htonl( deh->deh_bytes );

    return( ddp_route( m, &ddp->ddp_route ));
}

u_short
at_cksum( struct mbuf *m, int skip)
{
    u_char	*data, *end;
    u_long	cksum = 0;

    for (; m; m = m->m_next ) {
	for ( data = mtod( m, u_char * ), end = data + m->m_len; data < end;
		data++ ) {
	    if ( skip ) {
		skip--;
		continue;
	    }
	    cksum = ( cksum + *data ) << 1;
	    if ( cksum & 0x00010000 ) {
		cksum++;
	    }
	    cksum &= 0x0000ffff;
	}
    }

    if ( cksum == 0 ) {
	cksum = 0x0000ffff;
    }
    return( (u_short)cksum );
}

int
ddp_route( struct mbuf *m, struct route *ro)
{
    struct sockaddr_at	gate;
    struct elaphdr	*elh;
    struct mbuf		*m0;
    struct at_ifaddr	*aa = NULL;
    struct ifnet	*ifp = NULL;
    int			mlen;
    u_short		net;

    if ( ro->ro_rt && ( ifp = ro->ro_rt->rt_ifp )) {
#ifdef BSD4_4
	net = satosat( ro->ro_rt->rt_gateway )->sat_addr.s_net;
#else BSD4_4
	net = satosat( &ro->ro_rt->rt_gateway )->sat_addr.s_net;
#endif BSD4_4
	for ( aa = at_ifaddr; aa; aa = aa->aa_next ) {
	    if ( aa->aa_ifp == ifp &&
		    ntohs( net ) >= ntohs( aa->aa_firstnet ) &&
		    ntohs( net ) <= ntohs( aa->aa_lastnet )) {
		break;
	    }
	}
    }
    if ( aa == NULL ) {
printf( "ddp_route: oops\n" );
	m_freem( m );
	return( EINVAL );
    }

    /*
     * There are several places in the kernel where data is added to
     * an mbuf without ensuring that the mbuf pointer is aligned.
     * This is bad for transition routing, since phase 1 and phase 2
     * packets end up poorly aligned due to the three byte elap header.
     */
    if ( aa->aa_flags & AFA_PHASE2 ) {
#if defined( __FreeBSD__ )
	/* XXX don't need this because we can change if_ethersubr.c */
#else
	for ( mlen = 0, m0 = m; m0; m0 = m0->m_next ) {
	    mlen += m0->m_len;
	}
#ifndef MIN
#define MIN(x,y) ((x)<(y)?(x):(y))
#endif
	if (( m = m_pullup( m, MIN( MLEN, mlen ))) == 0 ) {
	    printf("ddp_route: m_pullup of %d (mlen=%d) failed\n",
		   MIN( MLEN, mlen ), mlen);
	    return( ENOBUFS );
	}
#endif __FreeBSD__
    } else {
# ifdef notdef
#ifdef BSD4_4
	M_PREPEND( m, SZ_ELAPHDR, M_DONTWAIT );
	if ( m == NULL ) {
	    return( ENOBUFS );
	}
#else BSD4_4
	m->m_off -= SZ_ELAPHDR;
	m->m_len += SZ_ELAPHDR;
#endif BSD4_4
# endif notdef

	MGET( m0, M_WAIT, MT_HEADER );
	if ( m0 == 0 ) {
	    m_freem( m );
	    printf("ddp_route: no buffers\n");
	    return( ENOBUFS );
	}
	m0->m_next = m;

#ifndef BSD4_4
	m0->m_off = MMINOFF + align( sizeof( struct ether_header ));
#else
	/* XXX perhaps we ought to align the header? */
#endif BSD4_4
	m0->m_len = SZ_ELAPHDR;
	m = m0;

	elh = mtod( m, struct elaphdr *);
	elh->el_snode = satosat( &aa->aa_addr )->sat_addr.s_node;
	elh->el_type = ELAP_DDPEXTEND;
	if ( ntohs( satosat( &ro->ro_dst )->sat_addr.s_net ) >=
		ntohs( aa->aa_firstnet ) &&
		ntohs( satosat( &ro->ro_dst )->sat_addr.s_net ) <=
		ntohs( aa->aa_lastnet )) {
	    elh->el_dnode = satosat( &ro->ro_dst )->sat_addr.s_node;
	} else {
#ifdef BSD4_4
	    elh->el_dnode = satosat( ro->ro_rt->rt_gateway )->sat_addr.s_node;
#else BSD4_4
	    elh->el_dnode = satosat( &ro->ro_rt->rt_gateway )->sat_addr.s_node;
#endif BSD4_4
	}
    }

    if ( ntohs( satosat( &ro->ro_dst )->sat_addr.s_net ) >=
	    ntohs( aa->aa_firstnet ) &&
	    ntohs( satosat( &ro->ro_dst )->sat_addr.s_net ) <=
	    ntohs( aa->aa_lastnet )) {
	gate = *satosat( &ro->ro_dst );
    } else {
#ifdef BSD4_4
	gate = *satosat( ro->ro_rt->rt_gateway );
#else BSD4_4
	gate = *satosat( &ro->ro_rt->rt_gateway );
#endif BSD4_4
    }
    ro->ro_rt->rt_use++;

#ifdef ultrix
    /*
     * SAIEW: We can't make changes to net/if_loop.c, so we don't route
     * further than this: if it's going to go through the lookback,
     * short-circuit to ddp_input(). Who needs queuing?
     *
     * Note: Passing NULL for the elaphdr is cool, since we'll only ever
     * try to send long form ddp throught the loopback.
     */
    if ( ifp->if_flags & IFF_LOOPBACK ) {
#ifdef notdef
	m->m_off += SZ_ELAPHDR;
	m->m_len -= SZ_ELAPHDR;
#endif notdef
	ddp_input( m, ifp, (struct elaphdr *)NULL, 2 );
	return( 0 );
    }
#endif ultrix

#ifdef _IBMR2
    /*
     * We can't make changes to the interface routines on RS6ks, and
     * they don't provide hooks for if_output, so we just resolve
     * our address here, and pass the packet as a raw ethernet packet.
     * This doesn't work particularly well, if we aren't *on* ethernet,
     * but it's ok for the moment.
     */
    if ( ! ( ifp->if_flags & IFF_LOOPBACK )) {
	struct ether_header	eh;

	if ( !aarpresolve(( struct arpcom *)ifp, m,
		&gate, eh.ether_dhost )) {
	    return( 0 );
	}
	eh.ether_type = htons( ETHERTYPE_AT );
	gate.sat_family = AF_UNSPEC;
	bcopy( &eh, (*(struct sockaddr *)&gate).sa_data,
		sizeof( (*(struct sockaddr *)&gate).sa_data ));
    }
#endif _IBMR2
    return((*ifp->if_output)( ifp, m, &gate
#if defined( __FreeBSD__ )
	, NULL /* XXX */
#endif __FreeBSD__
	));
}
