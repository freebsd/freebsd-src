/*
 * Copyright (c) 1990,1991 Regents of The University of Michigan.
 * All Rights Reserved.
 *
 * $FreeBSD$
 */

#include "opt_atalk.h"
#include "opt_mac.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mac.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/syslog.h>

#include <net/if.h>

#include <netinet/in.h>
#undef s_net
#include <netinet/if_ether.h>

#include <netatalk/at.h>
#include <netatalk/at_var.h>
#include <netatalk/aarp.h>
#include <netatalk/phase2.h>
#include <netatalk/at_extern.h>

static void aarptfree( struct aarptab *aat);
static void at_aarpinput( struct arpcom *ac, struct mbuf *m);

#define AARPTAB_BSIZ	9
#define AARPTAB_NB	19
#define AARPTAB_SIZE	(AARPTAB_BSIZ * AARPTAB_NB)
static struct aarptab	aarptab[AARPTAB_SIZE];

#define AARPTAB_HASH(a) \
    ((((a).s_net << 8 ) + (a).s_node ) % AARPTAB_NB )

#define AARPTAB_LOOK(aat,addr) { \
    int		n; \
    aat = &aarptab[ AARPTAB_HASH(addr) * AARPTAB_BSIZ ]; \
    for ( n = 0; n < AARPTAB_BSIZ; n++, aat++ ) \
	if ( aat->aat_ataddr.s_net == (addr).s_net && \
	     aat->aat_ataddr.s_node == (addr).s_node ) \
	    break; \
	if ( n >= AARPTAB_BSIZ ) \
	    aat = 0; \
}

#define AARPT_AGE	(60 * 1)
#define AARPT_KILLC	20
#define AARPT_KILLI	3

# if !defined( __FreeBSD__ )
extern u_char			etherbroadcastaddr[6];
# endif /* __FreeBSD__ */

static u_char atmulticastaddr[ 6 ] = {
    0x09, 0x00, 0x07, 0xff, 0xff, 0xff,
};

u_char	at_org_code[ 3 ] = {
    0x08, 0x00, 0x07,
};
u_char	aarp_org_code[ 3 ] = {
    0x00, 0x00, 0x00,
};

static struct callout_handle aarptimer_ch =
    CALLOUT_HANDLE_INITIALIZER(&aarptimer_ch);

static void
aarptimer(void *ignored)
{
    struct aarptab	*aat;
    int			i, s;

    aarptimer_ch = timeout( aarptimer, (caddr_t)0, AARPT_AGE * hz );
    aat = aarptab;
    for ( i = 0; i < AARPTAB_SIZE; i++, aat++ ) {
	if ( aat->aat_flags == 0 || ( aat->aat_flags & ATF_PERM ))
	    continue;
	if ( ++aat->aat_timer < (( aat->aat_flags & ATF_COM ) ?
		AARPT_KILLC : AARPT_KILLI ))
	    continue;
	s = splimp();
	aarptfree( aat );
	splx( s );
    }
}

/* 
 * search through the network addresses to find one that includes
 * the given network.. remember to take netranges into
 * consideration.
 */
struct at_ifaddr *
at_ifawithnet(struct sockaddr_at  *sat )
{
    struct at_ifaddr	*aa;
    struct sockaddr_at	*sat2;

	for ( aa = at_ifaddr; aa; aa = aa->aa_next ) {
		sat2 = &(aa->aa_addr);
		if ( sat2->sat_addr.s_net == sat->sat_addr.s_net ) {
	    		break;
		}
		if( (aa->aa_flags & AFA_PHASE2 )
	 	&& (ntohs(aa->aa_firstnet) <= ntohs(sat->sat_addr.s_net))
	 	&& (ntohs(aa->aa_lastnet) >= ntohs(sat->sat_addr.s_net))) {
			break;
		}
	}
	return( aa );
}

static void
aarpwhohas( struct arpcom *ac, struct sockaddr_at *sat )
{
    struct mbuf		*m;
    struct ether_header	*eh;
    struct ether_aarp	*ea;
    struct at_ifaddr	*aa;
    struct llc		*llc;
    struct sockaddr	sa;

    if (( m = m_gethdr( M_DONTWAIT, MT_DATA )) == NULL ) {
	return;
    }
#ifdef MAC
    mac_create_mbuf_linklayer(&ac->ac_if, m);
#endif
    m->m_len = sizeof( *ea );
    m->m_pkthdr.len = sizeof( *ea );
    MH_ALIGN( m, sizeof( *ea ));

    ea = mtod( m, struct ether_aarp *);
    bzero((caddr_t)ea, sizeof( *ea ));

    ea->aarp_hrd = htons( AARPHRD_ETHER );
    ea->aarp_pro = htons( ETHERTYPE_AT );
    ea->aarp_hln = sizeof( ea->aarp_sha );
    ea->aarp_pln = sizeof( ea->aarp_spu );
    ea->aarp_op = htons( AARPOP_REQUEST );
    bcopy((caddr_t)ac->ac_enaddr, (caddr_t)ea->aarp_sha,
	    sizeof( ea->aarp_sha ));

    /*
     * We need to check whether the output ethernet type should
     * be phase 1 or 2. We have the interface that we'll be sending
     * the aarp out. We need to find an AppleTalk network on that
     * interface with the same address as we're looking for. If the
     * net is phase 2, generate an 802.2 and SNAP header.
     */
    if ((aa = at_ifawithnet( sat )) == NULL) {
	m_freem( m );
	return;
    }

    eh = (struct ether_header *)sa.sa_data;

    if ( aa->aa_flags & AFA_PHASE2 ) {
	bcopy((caddr_t)atmulticastaddr, (caddr_t)eh->ether_dhost,
		sizeof( eh->ether_dhost ));
	eh->ether_type = htons(sizeof(struct llc) + sizeof(struct ether_aarp));
	M_PREPEND( m, sizeof( struct llc ), M_TRYWAIT );
	llc = mtod( m, struct llc *);
	llc->llc_dsap = llc->llc_ssap = LLC_SNAP_LSAP;
	llc->llc_control = LLC_UI;
	bcopy( aarp_org_code, llc->llc_org_code, sizeof( aarp_org_code ));
	llc->llc_ether_type = htons( ETHERTYPE_AARP );

	bcopy( &AA_SAT( aa )->sat_addr.s_net, ea->aarp_spnet,
	       sizeof( ea->aarp_spnet ));
	bcopy( &sat->sat_addr.s_net, ea->aarp_tpnet,
	       sizeof( ea->aarp_tpnet ));
	ea->aarp_spnode = AA_SAT( aa )->sat_addr.s_node;
	ea->aarp_tpnode = sat->sat_addr.s_node;
    } else {
	bcopy((caddr_t)etherbroadcastaddr, (caddr_t)eh->ether_dhost,
		sizeof( eh->ether_dhost ));
	eh->ether_type = htons( ETHERTYPE_AARP );

	ea->aarp_spa = AA_SAT( aa )->sat_addr.s_node;
	ea->aarp_tpa = sat->sat_addr.s_node;
    }

#ifdef NETATALKDEBUG
    printf("aarp: sending request for %u.%u\n",
	   ntohs(AA_SAT( aa )->sat_addr.s_net),
	   AA_SAT( aa )->sat_addr.s_node);
#endif /* NETATALKDEBUG */

    sa.sa_len = sizeof( struct sockaddr );
    sa.sa_family = AF_UNSPEC;
    (*ac->ac_if.if_output)(&ac->ac_if,
	m, &sa, NULL); 	/* XXX NULL should be routing information */
}

int
aarpresolve( ac, m, destsat, desten )
    struct arpcom	*ac;
    struct mbuf		*m;
    struct sockaddr_at	*destsat;
    u_char		*desten;
{
    struct at_ifaddr	*aa;
    struct aarptab	*aat;
    int			s;

    if ( at_broadcast( destsat )) {
	m->m_flags |= M_BCAST;
	if ((aa = at_ifawithnet( destsat )) == NULL)  {
	    m_freem( m );
	    return( 0 );
	}
	if ( aa->aa_flags & AFA_PHASE2 ) {
	    bcopy( (caddr_t)atmulticastaddr, (caddr_t)desten,
		    sizeof( atmulticastaddr ));
	} else {
	    bcopy( (caddr_t)etherbroadcastaddr, (caddr_t)desten,
		    sizeof( etherbroadcastaddr ));
	}
	return( 1 );
    }

    s = splimp();
    AARPTAB_LOOK( aat, destsat->sat_addr );
    if ( aat == 0 ) {			/* No entry */
	aat = aarptnew( &destsat->sat_addr );
	if ( aat == 0 ) {
	    panic( "aarpresolve: no free entry" );
	}
	aat->aat_hold = m;
	aarpwhohas( ac, destsat );
	splx( s );
	return( 0 );
    }
    /* found an entry */
    aat->aat_timer = 0;
    if ( aat->aat_flags & ATF_COM ) {	/* entry is COMplete */
	bcopy( (caddr_t)aat->aat_enaddr, (caddr_t)desten,
		sizeof( aat->aat_enaddr ));
	splx( s );
	return( 1 );
    }
    /* entry has not completed */
    if ( aat->aat_hold ) {
	m_freem( aat->aat_hold );
    }
    aat->aat_hold = m;
    aarpwhohas( ac, destsat );
    splx( s );
    return( 0 );
}

void
aarpinput( ac, m )
    struct arpcom	*ac;
    struct mbuf		*m;
{
    struct arphdr	*ar;

    if ( ac->ac_if.if_flags & IFF_NOARP )
	goto out;

    if ( m->m_len < sizeof( struct arphdr )) {
	goto out;
    }

    ar = mtod( m, struct arphdr *);
    if ( ntohs( ar->ar_hrd ) != AARPHRD_ETHER ) {
	goto out;
    }
    
    if ( m->m_len < sizeof( struct arphdr ) + 2 * ar->ar_hln +
	    2 * ar->ar_pln ) {
	goto out;
    }
    
    switch( ntohs( ar->ar_pro )) {
    case ETHERTYPE_AT :
	at_aarpinput( ac, m );
	return;

    default:
	break;
    }

out:
    m_freem( m );
}

static void
at_aarpinput( struct arpcom *ac, struct mbuf *m)
{
    struct ether_aarp	*ea;
    struct at_ifaddr	*aa;
    struct aarptab	*aat;
    struct ether_header	*eh;
    struct llc		*llc;
    struct sockaddr_at	sat;
    struct sockaddr	sa;
    struct at_addr	spa, tpa, ma;
    int			op;
    u_short		net;

    ea = mtod( m, struct ether_aarp *);

    /* Check to see if from my hardware address */
    if ( !bcmp(( caddr_t )ea->aarp_sha, ( caddr_t )ac->ac_enaddr,
	    sizeof( ac->ac_enaddr ))) {
	m_freem( m );
	return;
    }

    op = ntohs( ea->aarp_op );
    bcopy( ea->aarp_tpnet, &net, sizeof( net ));

    if ( net != 0 ) { /* should be ATADDR_ANYNET? */
	sat.sat_len = sizeof(struct sockaddr_at);
	sat.sat_family = AF_APPLETALK;
	sat.sat_addr.s_net = net;
	if ((aa = at_ifawithnet( &sat )) == NULL) {
	    m_freem( m );
	    return;
	}
	bcopy( ea->aarp_spnet, &spa.s_net, sizeof( spa.s_net ));
	bcopy( ea->aarp_tpnet, &tpa.s_net, sizeof( tpa.s_net ));
    } else {
	/*
	 * Since we don't know the net, we just look for the first
	 * phase 1 address on the interface.
	 */
	for (aa = (struct at_ifaddr *)TAILQ_FIRST(&ac->ac_if.if_addrhead); aa;
		aa = (struct at_ifaddr *)aa->aa_ifa.ifa_link.tqe_next) {
	    if ( AA_SAT( aa )->sat_family == AF_APPLETALK &&
		    ( aa->aa_flags & AFA_PHASE2 ) == 0 ) {
		break;
	    }
	}
	if ( aa == NULL ) {
	    m_freem( m );
	    return;
	}
	tpa.s_net = spa.s_net = AA_SAT( aa )->sat_addr.s_net;
    }

    spa.s_node = ea->aarp_spnode;
    tpa.s_node = ea->aarp_tpnode;
    ma.s_net = AA_SAT( aa )->sat_addr.s_net;
    ma.s_node = AA_SAT( aa )->sat_addr.s_node;

    /*
     * This looks like it's from us.
     */
    if ( spa.s_net == ma.s_net && spa.s_node == ma.s_node ) {
	if ( aa->aa_flags & AFA_PROBING ) {
	    /*
	     * We're probing, someone either responded to our probe, or
	     * probed for the same address we'd like to use. Change the
	     * address we're probing for.
	     */
	    untimeout( aarpprobe, ac, aa->aa_ch );
	    wakeup( aa );
	    m_freem( m );
	    return;
	} else if ( op != AARPOP_PROBE ) {
	    /*
	     * This is not a probe, and we're not probing. This means
	     * that someone's saying they have the same source address
	     * as the one we're using. Get upset...
	     */
	    log( LOG_ERR,
		    "aarp: duplicate AT address!! %x:%x:%x:%x:%x:%x\n",
		    ea->aarp_sha[ 0 ], ea->aarp_sha[ 1 ], ea->aarp_sha[ 2 ],
		    ea->aarp_sha[ 3 ], ea->aarp_sha[ 4 ], ea->aarp_sha[ 5 ]);
	    m_freem( m );
	    return;
	}
    }

    AARPTAB_LOOK( aat, spa );
    if ( aat ) {
	if ( op == AARPOP_PROBE ) {
	    /*
	     * Someone's probing for spa, dealocate the one we've got,
	     * so that if the prober keeps the address, we'll be able
	     * to arp for him.
	     */
	    aarptfree( aat );
	    m_freem( m );
	    return;
	}

	bcopy(( caddr_t )ea->aarp_sha, ( caddr_t )aat->aat_enaddr,
		sizeof( ea->aarp_sha ));
	aat->aat_flags |= ATF_COM;
	if ( aat->aat_hold ) {
	    struct mbuf *mhold = aat->aat_hold;
	    aat->aat_hold = NULL;
	    sat.sat_len = sizeof(struct sockaddr_at);
	    sat.sat_family = AF_APPLETALK;
	    sat.sat_addr = spa;
	    (*ac->ac_if.if_output)( &ac->ac_if, mhold,
		    (struct sockaddr *)&sat, NULL); /* XXX */
	}
    } else if ((tpa.s_net == ma.s_net)
	   && (tpa.s_node == ma.s_node)
	   && (op != AARPOP_PROBE)
	   && ((aat = aarptnew( &spa )) != NULL)) {
	        bcopy(( caddr_t )ea->aarp_sha, ( caddr_t )aat->aat_enaddr,
		    sizeof( ea->aarp_sha ));
	        aat->aat_flags |= ATF_COM;
    }

    /*
     * Don't respond to responses, and never respond if we're
     * still probing.
     */
    if ( tpa.s_net != ma.s_net || tpa.s_node != ma.s_node ||
	    op == AARPOP_RESPONSE || ( aa->aa_flags & AFA_PROBING )) {
	m_freem( m );
	return;
    }

    bcopy(( caddr_t )ea->aarp_sha, ( caddr_t )ea->aarp_tha,
	    sizeof( ea->aarp_sha ));
    bcopy(( caddr_t )ac->ac_enaddr, ( caddr_t )ea->aarp_sha,
	    sizeof( ea->aarp_sha ));

    /* XXX */
    eh = (struct ether_header *)sa.sa_data;
    bcopy(( caddr_t )ea->aarp_tha, ( caddr_t )eh->ether_dhost,
	    sizeof( eh->ether_dhost ));

    if ( aa->aa_flags & AFA_PHASE2 ) {
	eh->ether_type = htons( sizeof( struct llc ) +
		sizeof( struct ether_aarp ));
	M_PREPEND( m, sizeof( struct llc ), M_DONTWAIT );
	if ( m == NULL ) {
	    return;
	}
	llc = mtod( m, struct llc *);
	llc->llc_dsap = llc->llc_ssap = LLC_SNAP_LSAP;
	llc->llc_control = LLC_UI;
	bcopy( aarp_org_code, llc->llc_org_code, sizeof( aarp_org_code ));
	llc->llc_ether_type = htons( ETHERTYPE_AARP );

	bcopy( ea->aarp_spnet, ea->aarp_tpnet, sizeof( ea->aarp_tpnet ));
	bcopy( &ma.s_net, ea->aarp_spnet, sizeof( ea->aarp_spnet ));
    } else {
	eh->ether_type = htons( ETHERTYPE_AARP );
    }

    ea->aarp_tpnode = ea->aarp_spnode;
    ea->aarp_spnode = ma.s_node;
    ea->aarp_op = htons( AARPOP_RESPONSE );

    sa.sa_len = sizeof( struct sockaddr );
    sa.sa_family = AF_UNSPEC;
    (*ac->ac_if.if_output)( &ac->ac_if, m, &sa, NULL); /* XXX */
    return;
}

static void
aarptfree( struct aarptab *aat)
{

    if ( aat->aat_hold )
	m_freem( aat->aat_hold );
    aat->aat_hold = NULL;
    aat->aat_timer = aat->aat_flags = 0;
    aat->aat_ataddr.s_net = 0;
    aat->aat_ataddr.s_node = 0;
}

    struct aarptab *
aarptnew( addr )
    struct at_addr	*addr;
{
    int			n;
    int			oldest = -1;
    struct aarptab	*aat, *aato = NULL;
    static int		first = 1;

    if ( first ) {
	first = 0;
	aarptimer_ch = timeout( aarptimer, (caddr_t)0, hz );
    }
    aat = &aarptab[ AARPTAB_HASH( *addr ) * AARPTAB_BSIZ ];
    for ( n = 0; n < AARPTAB_BSIZ; n++, aat++ ) {
	if ( aat->aat_flags == 0 )
	    goto out;
	if ( aat->aat_flags & ATF_PERM )
	    continue;
	if ((int) aat->aat_timer > oldest ) {
	    oldest = aat->aat_timer;
	    aato = aat;
	}
    }
    if ( aato == NULL )
	return( NULL );
    aat = aato;
    aarptfree( aat );
out:
    aat->aat_ataddr = *addr;
    aat->aat_flags = ATF_INUSE;
    return( aat );
}


void
aarpprobe( void *arg )
{
    struct arpcom	*ac = arg;
    struct mbuf		*m;
    struct ether_header	*eh;
    struct ether_aarp	*ea;
    struct at_ifaddr	*aa;
    struct llc		*llc;
    struct sockaddr	sa;

    /*
     * We need to check whether the output ethernet type should
     * be phase 1 or 2. We have the interface that we'll be sending
     * the aarp out. We need to find an AppleTalk network on that
     * interface with the same address as we're looking for. If the
     * net is phase 2, generate an 802.2 and SNAP header.
     */
    for (aa = (struct at_ifaddr *)TAILQ_FIRST(&ac->ac_if.if_addrhead); aa;
	    aa = (struct at_ifaddr *)aa->aa_ifa.ifa_link.tqe_next) {
	if ( AA_SAT( aa )->sat_family == AF_APPLETALK &&
		( aa->aa_flags & AFA_PROBING )) {
	    break;
	}
    }
    if ( aa == NULL ) {		/* serious error XXX */
	printf( "aarpprobe why did this happen?!\n" );
	return;
    }

    if ( aa->aa_probcnt <= 0 ) {
	aa->aa_flags &= ~AFA_PROBING;
	wakeup( aa );
	return;
    } else {
	aa->aa_ch = timeout( aarpprobe, (caddr_t)ac, hz / 5 );
    }

    if (( m = m_gethdr( M_DONTWAIT, MT_DATA )) == NULL ) {
	return;
    }
#ifdef MAC
    mac_create_mbuf_linklayer(&ac->ac_if, m);
#endif
    m->m_len = sizeof( *ea );
    m->m_pkthdr.len = sizeof( *ea );
    MH_ALIGN( m, sizeof( *ea ));

    ea = mtod( m, struct ether_aarp *);
    bzero((caddr_t)ea, sizeof( *ea ));

    ea->aarp_hrd = htons( AARPHRD_ETHER );
    ea->aarp_pro = htons( ETHERTYPE_AT );
    ea->aarp_hln = sizeof( ea->aarp_sha );
    ea->aarp_pln = sizeof( ea->aarp_spu );
    ea->aarp_op = htons( AARPOP_PROBE );
    bcopy((caddr_t)ac->ac_enaddr, (caddr_t)ea->aarp_sha,
	    sizeof( ea->aarp_sha ));

    eh = (struct ether_header *)sa.sa_data;

    if ( aa->aa_flags & AFA_PHASE2 ) {
	bcopy((caddr_t)atmulticastaddr, (caddr_t)eh->ether_dhost,
		sizeof( eh->ether_dhost ));
	eh->ether_type = htons( sizeof( struct llc ) +
		sizeof( struct ether_aarp ));
	M_PREPEND( m, sizeof( struct llc ), M_TRYWAIT );
	llc = mtod( m, struct llc *);
	llc->llc_dsap = llc->llc_ssap = LLC_SNAP_LSAP;
	llc->llc_control = LLC_UI;
	bcopy( aarp_org_code, llc->llc_org_code, sizeof( aarp_org_code ));
	llc->llc_ether_type = htons( ETHERTYPE_AARP );

	bcopy( &AA_SAT( aa )->sat_addr.s_net, ea->aarp_spnet,
		sizeof( ea->aarp_spnet ));
	bcopy( &AA_SAT( aa )->sat_addr.s_net, ea->aarp_tpnet,
		sizeof( ea->aarp_tpnet ));
	ea->aarp_spnode = ea->aarp_tpnode = AA_SAT( aa )->sat_addr.s_node;
    } else {
	bcopy((caddr_t)etherbroadcastaddr, (caddr_t)eh->ether_dhost,
		sizeof( eh->ether_dhost ));
	eh->ether_type = htons( ETHERTYPE_AARP );
	ea->aarp_spa = ea->aarp_tpa = AA_SAT( aa )->sat_addr.s_node;
    }

#ifdef NETATALKDEBUG
    printf("aarp: sending probe for %u.%u\n",
	   ntohs(AA_SAT( aa )->sat_addr.s_net),
	   AA_SAT( aa )->sat_addr.s_node);
#endif /* NETATALKDEBUG */

    sa.sa_len = sizeof( struct sockaddr );
    sa.sa_family = AF_UNSPEC;
    (*ac->ac_if.if_output)(&ac->ac_if, m, &sa, NULL); /* XXX */
    aa->aa_probcnt--;
}

void
aarp_clean(void)
{
    struct aarptab	*aat;
    int			i;

    untimeout( aarptimer, 0, aarptimer_ch );
    for ( i = 0, aat = aarptab; i < AARPTAB_SIZE; i++, aat++ ) {
	if ( aat->aat_hold ) {
	    m_freem( aat->aat_hold );
	    aat->aat_hold = NULL;
	}
    }
}
