/*
 * Copyright (c) 1990,1994 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include <sys/errno.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#if defined( __FreeBSD__ )
#include <sys/proc.h>
#endif __FreeBSD__
#ifdef ibm032
#include <sys/dir.h>
#endif ibm032
#ifndef	__FreeBSD__
#include <sys/user.h>
#endif
#include <sys/mbuf.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#ifdef _IBMR2
#include <net/spl.h>
#endif _IBMR2

#include "at.h"
#include "at_var.h"
#include "ddp_var.h"
#include "aarp.h"
#include "endian.h"
#include <netatalk/at_extern.h>

static void at_pcbdisconnect( struct ddpcb *ddp );
static void at_sockaddr( struct ddpcb *ddp, struct mbuf *addr );
static int at_pcbsetaddr( struct ddpcb *ddp, struct mbuf *addr, struct proc *p);
static int at_pcbconnect( struct ddpcb *ddp, struct mbuf *addr, struct proc *p);
static void at_pcbdetach( struct socket *so, struct ddpcb *ddp);
static int at_pcballoc( struct socket *so );

struct ddpcb	*ddp_ports[ ATPORT_LAST ];
struct ddpcb	*ddpcb = NULL;
u_long		ddp_sendspace = DDP_MAXSZ; /* Max ddp size + 1 (ddp_type) */
u_long		ddp_recvspace = 10 * ( 587 + sizeof( struct sockaddr_at ));

/*ARGSUSED*/
int
ddp_usrreq( struct socket *so, int req, struct mbuf *m,
		struct mbuf  *addr, struct mbuf *rights)
{
#if defined( __FreeBSD__ )
    struct proc *p = curproc;           /* XXX */
#endif __FreeBSD__
    struct ddpcb	*ddp;
    int			error = 0;

    ddp = sotoddpcb( so );

    if ( req == PRU_CONTROL ) {
	return( at_control( (int) m, (caddr_t) addr,
		(struct ifnet *) rights
#if defined( __FreeBSD__ )
			    , (struct proc *)p
#endif __FreeBSD__
	    ));
    }

    if ( rights && rights->m_len ) {
	error = EINVAL;
	goto release;
    }

    if ( ddp == NULL && req != PRU_ATTACH ) {
	error = EINVAL;
	goto release;
    }

    switch ( req ) {
    case PRU_ATTACH :
	if ( ddp != NULL ) {
	    error = EINVAL;
	    break;
	}
	if (( error = at_pcballoc( so )) != 0 ) {
	    break;
	}
	error = soreserve( so, ddp_sendspace, ddp_recvspace );
	break;

    case PRU_DETACH :
	at_pcbdetach( so, ddp );
	break;

    case PRU_BIND :
	error = at_pcbsetaddr( ddp, addr
#if defined( __FreeBSD__ )
			       , p 
#endif __FreeBSD__
	    );
	break;
    
    case PRU_SOCKADDR :
	at_sockaddr( ddp, addr );
	break;

    case PRU_CONNECT:
	if ( ddp->ddp_fsat.sat_port != ATADDR_ANYPORT ) {
	    error = EISCONN;
	    break;
	}

	error = at_pcbconnect( ddp, addr
#if defined( __FreeBSD__ )
	    , p
#endif __FreeBSD__
	    );
	if ( error == 0 )
	    soisconnected( so );
	break;

    case PRU_DISCONNECT:
	if ( ddp->ddp_fsat.sat_addr.s_node == ATADDR_ANYNODE ) {
	    error = ENOTCONN;
	    break;
	}
	at_pcbdisconnect( ddp );
	soisdisconnected( so );
	break;

    case PRU_SHUTDOWN:
	socantsendmore( so );
	break;

    case PRU_SEND: {
	int	s = 0;

	if ( addr ) {
	    if ( ddp->ddp_fsat.sat_port != ATADDR_ANYPORT ) {
		error = EISCONN;
		break;
	    }

	    s = splnet();
	    error = at_pcbconnect( ddp, addr
#if defined( __FreeBSD__ )
		, p
#endif __FreeBSD__
		);
	    if ( error ) {
		splx( s );
		break;
	    }
	} else {
	    if ( ddp->ddp_fsat.sat_port == ATADDR_ANYPORT ) {
		error = ENOTCONN;
		break;
	    }
	}

	error = ddp_output( ddp, m );
	m = NULL;
	if ( addr ) {
	    at_pcbdisconnect( ddp );
	    splx( s );
	}
	}
	break;

    case PRU_ABORT:
	soisdisconnected( so );
	at_pcbdetach( so, ddp );
	break;

    case PRU_LISTEN:
    case PRU_CONNECT2:
    case PRU_ACCEPT:
    case PRU_SENDOOB:
    case PRU_FASTTIMO:
    case PRU_SLOWTIMO:
    case PRU_PROTORCV:
    case PRU_PROTOSEND:
	error = EOPNOTSUPP;
	break;

    case PRU_RCVD:
    case PRU_RCVOOB:
	/*
	 * Don't mfree. Good architecture...
	 */
	return( EOPNOTSUPP );

    case PRU_SENSE:
	/*
	 * 1. Don't return block size.
	 * 2. Don't mfree.
	 */
	return( 0 );

    default:
	error = EOPNOTSUPP;
    }

release:
    if ( m != NULL ) {
	m_freem( m );
    }
    return( error );
}

static void
at_sockaddr( struct ddpcb *ddp, struct mbuf *addr)
{
    struct sockaddr_at	*sat;

    addr->m_len = sizeof( struct sockaddr_at );
    sat = mtod( addr, struct sockaddr_at *);
    *sat = ddp->ddp_lsat;
}

static int 
at_pcbsetaddr( struct ddpcb *ddp, struct mbuf *addr, struct proc *p )
{
    struct sockaddr_at	lsat, *sat;
    struct at_ifaddr	*aa;
    struct ddpcb	*ddpp;

    if ( ddp->ddp_lsat.sat_port != ATADDR_ANYPORT ) { /* shouldn't be bound */
	return( EINVAL );
    }

    if ( addr != 0 ) {			/* validate passed address */
	sat = mtod( addr, struct sockaddr_at *);
	if ( addr->m_len != sizeof( *sat )) {
	    return( EINVAL );
	}
	if ( sat->sat_family != AF_APPLETALK ) {
	    return( EAFNOSUPPORT );
	}

	if ( sat->sat_addr.s_node != ATADDR_ANYNODE ||
		sat->sat_addr.s_net != ATADDR_ANYNET ) {
	    for ( aa = at_ifaddr; aa; aa = aa->aa_next ) {
		if (( sat->sat_addr.s_net == AA_SAT( aa )->sat_addr.s_net ) &&
		 ( sat->sat_addr.s_node == AA_SAT( aa )->sat_addr.s_node )) {
		    break;
		}
	    }
	    if ( !aa ) {
		return( EADDRNOTAVAIL );
	    }
	}

	if ( sat->sat_port != ATADDR_ANYPORT ) {
	    if ( sat->sat_port < ATPORT_FIRST ||
		    sat->sat_port >= ATPORT_LAST ) {
		return( EINVAL );
	    }
#ifdef BSD4_4
	    if ( sat->sat_port < ATPORT_RESERVED &&
#if defined( __FreeBSD__ )
		 suser( p->p_ucred, &p->p_acflag )
#else
		    suser( u.u_cred, &u.u_acflag )
#endif __FreeBSD__
		) {
		return( EACCES );
	    }
#else BSD4_4
	    if ( sat->sat_port < ATPORT_RESERVED && ( !suser())) {
		return( EACCES );
	    }
#endif BSD4_4
	}
    } else {
	bzero( (caddr_t)&lsat, sizeof( struct sockaddr_at ));
#ifdef BSD4_4
	lsat.sat_len = sizeof(struct sockaddr_at);
	lsat.sat_addr.s_node = ATADDR_ANYNODE;
	lsat.sat_addr.s_net = ATADDR_ANYNET;
#endif BSD4_4
	lsat.sat_family = AF_APPLETALK;
	sat = &lsat;
    }

    if ( sat->sat_addr.s_node == ATADDR_ANYNODE &&
	    sat->sat_addr.s_net == ATADDR_ANYNET ) {
	if ( at_ifaddr == NULL ) {
	    return( EADDRNOTAVAIL );
	}
	sat->sat_addr = AA_SAT( at_ifaddr )->sat_addr;
    }
    ddp->ddp_lsat = *sat;

    /*
     * Choose port.
     */
    if ( sat->sat_port == ATADDR_ANYPORT ) {
	for ( sat->sat_port = ATPORT_RESERVED;
		sat->sat_port < ATPORT_LAST; sat->sat_port++ ) {
	    if ( ddp_ports[ sat->sat_port - 1 ] == 0 ) {
		break;
	    }
	}
	if ( sat->sat_port == ATPORT_LAST ) {
	    return( EADDRNOTAVAIL );
	}
	ddp->ddp_lsat.sat_port = sat->sat_port;
	ddp_ports[ sat->sat_port - 1 ] = ddp;
    } else {
	for ( ddpp = ddp_ports[ sat->sat_port - 1 ]; ddpp;
		ddpp = ddpp->ddp_pnext ) {
	    if ( ddpp->ddp_lsat.sat_addr.s_net == sat->sat_addr.s_net &&
		    ddpp->ddp_lsat.sat_addr.s_node == sat->sat_addr.s_node ) {
		break;
	    }
	}
	if ( ddpp != NULL ) {
	    return( EADDRINUSE );
	}
	ddp->ddp_pnext = ddp_ports[ sat->sat_port - 1 ];
	ddp_ports[ sat->sat_port - 1 ] = ddp;
	if ( ddp->ddp_pnext ) {
	    ddp->ddp_pnext->ddp_pprev = ddp;
	}
    }

    return( 0 );
}

static int
at_pcbconnect( struct ddpcb *ddp, struct mbuf *addr, struct proc *p)
{
    struct sockaddr_at	*sat = mtod( addr, struct sockaddr_at *);
    struct route	*ro;
    struct at_ifaddr	*aa = 0;
    struct ifnet	*ifp;
    u_short		hintnet = 0, net;

    if ( addr->m_len != sizeof( *sat ))
	return( EINVAL );
    if ( sat->sat_family != AF_APPLETALK ) {
	return( EAFNOSUPPORT );
    }

    /*
     * Under phase 2, network 0 means "the network".  We take "the
     * network" to mean the network the control block is bound to.
     * If the control block is not bound, there is an error.
     */
    if ( sat->sat_addr.s_net == ATADDR_ANYNET
		&& sat->sat_addr.s_node != ATADDR_ANYNODE ) {
	if ( ddp->ddp_lsat.sat_port == ATADDR_ANYPORT ) {
	    return( EADDRNOTAVAIL );
	}
	hintnet = ddp->ddp_lsat.sat_addr.s_net;
    }

    ro = &ddp->ddp_route;
    /*
     * If we've got an old route for this pcb, check that it is valid.
     * If we've changed our address, we may have an old "good looking"
     * route here.  Attempt to detect it.
     */
    if ( ro->ro_rt ) {
	if ( hintnet ) {
	    net = hintnet;
	} else {
	    net = sat->sat_addr.s_net;
	}
	aa = 0;
	if ( ifp = ro->ro_rt->rt_ifp ) {
	    for ( aa = at_ifaddr; aa; aa = aa->aa_next ) {
		if ( aa->aa_ifp == ifp &&
			ntohs( net ) >= ntohs( aa->aa_firstnet ) &&
			ntohs( net ) <= ntohs( aa->aa_lastnet )) {
		    printf("at_pcbconnect: found ifp net=%u\n", ntohs(net));
		    break;
		}
	    }
	}
	if ( aa == NULL || ( satosat( &ro->ro_dst )->sat_addr.s_net !=
		( hintnet ? hintnet : sat->sat_addr.s_net ) ||
		satosat( &ro->ro_dst )->sat_addr.s_node !=
		sat->sat_addr.s_node )) {
	    printf("at_pcbconnect: freeing ro->ro_rt=0x%x\n", ro->ro_rt);
#ifdef ultrix
	    rtfree( ro->ro_rt );
#else ultrix
	    RTFREE( ro->ro_rt );
#endif ultrix
	    ro->ro_rt = (struct rtentry *)0;
	}
    }

    /*
     * If we've got no route for this interface, try to find one.
     */
    if ( ro->ro_rt == (struct rtentry *)0 ||
	 ro->ro_rt->rt_ifp == (struct ifnet *)0 ) {
#ifdef BSD4_4
	ro->ro_dst.sa_len = sizeof( struct sockaddr_at );
#endif BSD4_4
	ro->ro_dst.sa_family = AF_APPLETALK;
	if ( hintnet ) {
	    satosat( &ro->ro_dst )->sat_addr.s_net = hintnet;
	} else {
	    satosat( &ro->ro_dst )->sat_addr.s_net = sat->sat_addr.s_net;
	}
	satosat( &ro->ro_dst )->sat_addr.s_node = sat->sat_addr.s_node;
	rtalloc( ro );
    }

    /*
     * Make sure any route that we have has a valid interface.
     */
    aa = 0;
    if ( ro->ro_rt && ( ifp = ro->ro_rt->rt_ifp )) {
	for ( aa = at_ifaddr; aa; aa = aa->aa_next ) {
	    if ( aa->aa_ifp == ifp ) {
		break;
	    }
	}
    }
    if ( aa == 0 ) {
	printf("at_pcbconnect: ro->ro_rt=0x%x\n", ro->ro_rt);
	if (ro->ro_rt)
	    printf("at_pcbconnect: ro->ro_rt->rt_ifp=0x%x", ro->ro_rt->rt_ifp);
	return( ENETUNREACH );
    }

    ddp->ddp_fsat = *sat;
    if ( ddp->ddp_lsat.sat_port == ATADDR_ANYPORT ) {
	return( at_pcbsetaddr( ddp, (struct mbuf *)0
#if defined( __FreeBSD__ )
			       , p
#endif __FreeBSD__
	    ));
    }
    return( 0 );
}

static void 
at_pcbdisconnect( struct ddpcb	*ddp )
{
    ddp->ddp_fsat.sat_addr.s_net = ATADDR_ANYNET;
    ddp->ddp_fsat.sat_addr.s_node = ATADDR_ANYNODE;
    ddp->ddp_fsat.sat_port = ATADDR_ANYPORT;
}

static int
at_pcballoc( struct socket *so )
{
    struct ddpcb	*ddp;
    struct mbuf		*m;

    m = m_getclr( M_WAIT, MT_PCB );
    ddp = mtod( m, struct ddpcb * );
    ddp->ddp_lsat.sat_port = ATADDR_ANYPORT;

    ddp->ddp_next = ddpcb;
    ddp->ddp_prev = NULL;
    ddp->ddp_pprev = NULL;
    ddp->ddp_pnext = NULL;
    if ( ddpcb ) {
	ddpcb->ddp_prev = ddp;
    }
    ddpcb = ddp;

    ddp->ddp_socket = so;
    so->so_pcb = (caddr_t)ddp;
    return( 0 );
}

static void
at_pcbdetach( struct socket *so, struct ddpcb *ddp)
{
    soisdisconnected( so );
    so->so_pcb = 0;
    sofree( so );

    /* remove ddp from ddp_ports list */
    if ( ddp->ddp_lsat.sat_port != ATADDR_ANYPORT &&
	    ddp_ports[ ddp->ddp_lsat.sat_port - 1 ] != NULL ) {
	if ( ddp->ddp_pprev != NULL ) {
	    ddp->ddp_pprev->ddp_pnext = ddp->ddp_pnext;
	} else {
	    ddp_ports[ ddp->ddp_lsat.sat_port - 1 ] = ddp->ddp_pnext;
	}
	if ( ddp->ddp_pnext != NULL ) {
	    ddp->ddp_pnext->ddp_pprev = ddp->ddp_pprev;
	}
    }

    if ( ddp->ddp_route.ro_rt ) {
	rtfree( ddp->ddp_route.ro_rt );
    }

    if ( ddp->ddp_prev ) {
	ddp->ddp_prev->ddp_next = ddp->ddp_next;
    } else {
	ddpcb = ddp->ddp_next;
    }
    if ( ddp->ddp_next ) {
	ddp->ddp_next->ddp_prev = ddp->ddp_prev;
    }

    (void) m_free( dtom( ddp ));
}

/*
 * For the moment, this just find the pcb with the correct local address.
 * In the future, this will actually do some real searching, so we can use
 * the sender's address to do de-multiplexing on a single port to many
 * sockets (pcbs).
 */
struct ddpcb *
ddp_search( struct sockaddr_at *from, struct sockaddr_at *to,
			struct at_ifaddr *aa)
{
    struct ddpcb	*ddp;

    /*
     * Check for bad ports.
     */
    if ( to->sat_port < ATPORT_FIRST || to->sat_port >= ATPORT_LAST ) {
	return( NULL );
    }

    /*
     * Make sure the local address matches the sent address.  What about
     * the interface?
     */
    for ( ddp = ddp_ports[ to->sat_port - 1 ]; ddp; ddp = ddp->ddp_pnext ) {
	/* XXX should we handle 0.YY? */

	/* XXXX.YY to socket on destination interface */
	if ( to->sat_addr.s_net == ddp->ddp_lsat.sat_addr.s_net &&
		to->sat_addr.s_node == ddp->ddp_lsat.sat_addr.s_node ) {
	    break;
	}

	/* 0.255 to socket on receiving interface */
	if ( to->sat_addr.s_node == ATADDR_BCAST && ( to->sat_addr.s_net == 0 ||
		to->sat_addr.s_net == ddp->ddp_lsat.sat_addr.s_net ) &&
		ddp->ddp_lsat.sat_addr.s_net == AA_SAT( aa )->sat_addr.s_net ) {
	    break;
	}

	/* XXXX.0 to socket on destination interface */
	if ( to->sat_addr.s_net == aa->aa_firstnet &&
		to->sat_addr.s_node == 0 &&
		ntohs( ddp->ddp_lsat.sat_addr.s_net ) >=
		ntohs( aa->aa_firstnet ) &&
		ntohs( ddp->ddp_lsat.sat_addr.s_net ) <=
		ntohs( aa->aa_lastnet )) {
	    break;
	}
    }
    return( ddp );
}

void 
ddp_init(void )
{
    atintrq1.ifq_maxlen = IFQ_MAXLEN;
    atintrq2.ifq_maxlen = IFQ_MAXLEN;
}

static void 
ddp_clean(void )
{
    struct ddpcb	*ddp;

    for ( ddp = ddpcb; ddp; ddp = ddp->ddp_next ) {
	at_pcbdetach( ddp->ddp_socket, ddp );
    }
}
