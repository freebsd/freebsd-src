/*
 * Copyright (c) 1990,1994 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <net/if.h>
#include <net/route.h>
#include <net/netisr.h>

#include <netatalk/at.h>
#include <netatalk/at_var.h>
#include <netatalk/ddp_var.h>
#include <netatalk/at_extern.h>

static void at_pcbdisconnect( struct ddpcb *ddp );
static void at_sockaddr(struct ddpcb *ddp, struct sockaddr **addr);
static int at_pcbsetaddr(struct ddpcb *ddp, struct sockaddr *addr,
			  struct thread *td);
static int at_pcbconnect(struct ddpcb *ddp, struct sockaddr *addr, 
			 struct thread *td);
static void at_pcbdetach(struct socket *so, struct ddpcb *ddp);
static int at_pcballoc(struct socket *so);

struct ddpcb	*ddp_ports[ ATPORT_LAST ];
struct ddpcb	*ddpcb = NULL;
static u_long	ddp_sendspace = DDP_MAXSZ; /* Max ddp size + 1 (ddp_type) */
static u_long	ddp_recvspace = 10 * ( 587 + sizeof( struct sockaddr_at ));

static struct ifqueue atintrq1, atintrq2, aarpintrq;

static int
ddp_attach(struct socket *so, int proto, struct thread *td)
{
	struct ddpcb	*ddp;
	int		error = 0;
	int		s;
	

	ddp = sotoddpcb( so );
	if ( ddp != NULL ) {
	    return( EINVAL);
	}

	s = splnet();
	error = at_pcballoc( so );
	splx(s);
	if (error) {
	    return (error);
	}
	return (soreserve( so, ddp_sendspace, ddp_recvspace ));
}

static int
ddp_detach(struct socket *so)
{
	struct ddpcb	*ddp;
	int		s;
	
	ddp = sotoddpcb( so );
	if ( ddp == NULL ) {
	    return( EINVAL);
	}
	s = splnet();
	at_pcbdetach( so, ddp );
	splx(s);
	return(0);
}

static int      
ddp_bind(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	struct ddpcb	*ddp;
	int		error = 0;
	int		s;
	
	ddp = sotoddpcb( so );
	if ( ddp == NULL ) {
	    return( EINVAL);
	}
	s = splnet();
	error = at_pcbsetaddr(ddp, nam, td);
	splx(s);
	return (error);
}
    
static int
ddp_connect(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	struct ddpcb	*ddp;
	int		error = 0;
	int		s;
	
	ddp = sotoddpcb( so );
	if ( ddp == NULL ) {
	    return( EINVAL);
	}

	if ( ddp->ddp_fsat.sat_port != ATADDR_ANYPORT ) {
	    return(EISCONN);
	}

	s = splnet();
	error = at_pcbconnect( ddp, nam, td );
	splx(s);
	if ( error == 0 )
	    soisconnected( so );
	return(error);
}

static int
ddp_disconnect(struct socket *so)
{

	struct ddpcb	*ddp;
	int		s;
	
	ddp = sotoddpcb( so );
	if ( ddp == NULL ) {
	    return( EINVAL);
	}
	if ( ddp->ddp_fsat.sat_addr.s_node == ATADDR_ANYNODE ) {
	    return(ENOTCONN);
	}

	s = splnet();
	at_pcbdisconnect( ddp );
	ddp->ddp_fsat.sat_addr.s_node = ATADDR_ANYNODE;
	splx(s);
	soisdisconnected( so );
	return(0);
}

static int
ddp_shutdown(struct socket *so)
{
	struct ddpcb	*ddp;

	ddp = sotoddpcb( so );
	if ( ddp == NULL ) {
		return( EINVAL);
	}
	socantsendmore( so );
	return(0);
}

static int
ddp_send(struct socket *so, int flags, struct mbuf *m, struct sockaddr *addr,
            struct mbuf *control, struct thread *td)
{
	struct ddpcb	*ddp;
	int		error = 0;
	int		s;
	
	ddp = sotoddpcb( so );
	if ( ddp == NULL ) {
		return(EINVAL);
	}

    	if ( control && control->m_len ) {
		return(EINVAL);
    	}

	if ( addr ) {
		if ( ddp->ddp_fsat.sat_port != ATADDR_ANYPORT ) {
			return(EISCONN);
		}

		s = splnet();
		error = at_pcbconnect(ddp, addr, td);
		splx( s );
		if ( error ) {
			return(error);
		}
	} else {
		if ( ddp->ddp_fsat.sat_port == ATADDR_ANYPORT ) {
			return(ENOTCONN);
		}
	}

	s = splnet();
	error = ddp_output( m, so );
	if ( addr ) {
	    at_pcbdisconnect( ddp );
	}
	splx(s);
	return(error);
}

static int
ddp_abort(struct socket *so)
{
	struct ddpcb	*ddp;
	int		s;
	
	ddp = sotoddpcb( so );
	if ( ddp == NULL ) {
		return(EINVAL);
	}
	soisdisconnected( so );
	s = splnet();
	at_pcbdetach( so, ddp );
	splx(s);
	return(0);
}


static void
at_sockaddr(struct ddpcb *ddp, struct sockaddr **addr)
{
    *addr = dup_sockaddr((struct sockaddr *)&ddp->ddp_lsat, 0);
}

static int 
at_pcbsetaddr(struct ddpcb *ddp, struct sockaddr *addr, struct thread *td)
{
    struct sockaddr_at	lsat, *sat;
    struct at_ifaddr	*aa;
    struct ddpcb	*ddpp;

    if ( ddp->ddp_lsat.sat_port != ATADDR_ANYPORT ) { /* shouldn't be bound */
	return( EINVAL );
    }

    if (addr != 0) {			/* validate passed address */
	sat = (struct sockaddr_at *)addr;
	if (sat->sat_family != AF_APPLETALK) {
	    return(EAFNOSUPPORT);
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
	    if ( sat->sat_port < ATPORT_RESERVED &&
		 suser(td) ) {
		return( EACCES );
	    }
	}
    } else {
	bzero( (caddr_t)&lsat, sizeof( struct sockaddr_at ));
	lsat.sat_len = sizeof(struct sockaddr_at);
	lsat.sat_addr.s_node = ATADDR_ANYNODE;
	lsat.sat_addr.s_net = ATADDR_ANYNET;
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
at_pcbconnect(struct ddpcb *ddp, struct sockaddr *addr, struct thread *td)
{
    struct sockaddr_at	*sat = (struct sockaddr_at *)addr;
    struct route	*ro;
    struct at_ifaddr	*aa = 0;
    struct ifnet	*ifp;
    u_short		hintnet = 0, net;

    if (sat->sat_family != AF_APPLETALK) {
	return(EAFNOSUPPORT);
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
	if ((ifp = ro->ro_rt->rt_ifp) != NULL) {
	    for ( aa = at_ifaddr; aa; aa = aa->aa_next ) {
		if ( aa->aa_ifp == ifp &&
			ntohs( net ) >= ntohs( aa->aa_firstnet ) &&
			ntohs( net ) <= ntohs( aa->aa_lastnet )) {
		    break;
		}
	    }
	}
	if ( aa == NULL || ( satosat( &ro->ro_dst )->sat_addr.s_net !=
		( hintnet ? hintnet : sat->sat_addr.s_net ) ||
		satosat( &ro->ro_dst )->sat_addr.s_node !=
		sat->sat_addr.s_node )) {
	    RTFREE( ro->ro_rt );
	    ro->ro_rt = (struct rtentry *)0;
	}
    }

    /*
     * If we've got no route for this interface, try to find one.
     */
    if ( ro->ro_rt == (struct rtentry *)0 ||
	 ro->ro_rt->rt_ifp == (struct ifnet *)0 ) {
	ro->ro_dst.sa_len = sizeof( struct sockaddr_at );
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
	return( ENETUNREACH );
    }

    ddp->ddp_fsat = *sat;
    if ( ddp->ddp_lsat.sat_port == ATADDR_ANYPORT ) {
	return(at_pcbsetaddr(ddp, (struct sockaddr *)0, td));
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

	MALLOC(ddp, struct ddpcb *, sizeof *ddp, M_PCB, M_WAITOK | M_ZERO);
	ddp->ddp_lsat.sat_port = ATADDR_ANYPORT;

	ddp->ddp_next = ddpcb;
	ddp->ddp_prev = NULL;
	ddp->ddp_pprev = NULL;
	ddp->ddp_pnext = NULL;
	if (ddpcb) {
		ddpcb->ddp_prev = ddp;
	}
	ddpcb = ddp;

	ddp->ddp_socket = so;
	so->so_pcb = (caddr_t)ddp;
	return(0);
}

static void
at_pcbdetach( struct socket *so, struct ddpcb *ddp)
{
    soisdisconnected( so );
    so->so_pcb = 0;
    sotryfree(so);

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
	RTFREE( ddp->ddp_route.ro_rt );
    }

    if ( ddp->ddp_prev ) {
	ddp->ddp_prev->ddp_next = ddp->ddp_next;
    } else {
	ddpcb = ddp->ddp_next;
    }
    if ( ddp->ddp_next ) {
	ddp->ddp_next->ddp_prev = ddp->ddp_prev;
    }
    FREE(ddp, M_PCB);
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
static int
at_setpeeraddr(struct socket *so, struct sockaddr **nam)
{
	return(EOPNOTSUPP);
}

static int
at_setsockaddr(struct socket *so, struct sockaddr **nam)
{
	struct ddpcb	*ddp;

	ddp = sotoddpcb( so );
	if ( ddp == NULL ) {
	    return( EINVAL);
	}
	at_sockaddr( ddp, nam );
	return(0);
}

void 
ddp_init(void)
{

	atintrq1.ifq_maxlen = IFQ_MAXLEN;
	atintrq2.ifq_maxlen = IFQ_MAXLEN;
	aarpintrq.ifq_maxlen = IFQ_MAXLEN;
	mtx_init(&atintrq1.ifq_mtx, "at1_inq", NULL, MTX_DEF);
	mtx_init(&atintrq2.ifq_mtx, "at2_inq", NULL, MTX_DEF);
	mtx_init(&aarpintrq.ifq_mtx, "aarp_inq", NULL, MTX_DEF);
	netisr_register(NETISR_ATALK1, at1intr, &atintrq1, 0);
	netisr_register(NETISR_ATALK2, at2intr, &atintrq2, 0);
	netisr_register(NETISR_AARP, aarpintr, &aarpintrq, 0);
}

#if 0
static void 
ddp_clean(void )
{
    struct ddpcb	*ddp;

    for ( ddp = ddpcb; ddp; ddp = ddp->ddp_next ) {
	at_pcbdetach( ddp->ddp_socket, ddp );
    }
}
#endif

struct pr_usrreqs ddp_usrreqs = {
	ddp_abort,
	pru_accept_notsupp,
	ddp_attach,
	ddp_bind,
	ddp_connect,
	pru_connect2_notsupp,
	at_control,
	ddp_detach,
	ddp_disconnect,
	pru_listen_notsupp,
	at_setpeeraddr,
	pru_rcvd_notsupp,
	pru_rcvoob_notsupp,
	ddp_send,
	pru_sense_null,
	ddp_shutdown,
	at_setsockaddr,
	sosend,
	soreceive,
	sopoll
};
