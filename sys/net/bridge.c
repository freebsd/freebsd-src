/*
 * Copyright (c) 1998-2000 Luigi Rizzo
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * This code implements bridging in FreeBSD. It only acts on ethernet
 * type of interfaces (others are still usable for routing).
 * A bridging table holds the source MAC address/dest. interface for each
 * known node. The table is indexed using an hash of the source address.
 *
 * Input packets are tapped near the beginning of ether_input(), and
 * analysed by calling bridge_in(). Depending on the result, the packet
 * can be forwarded to one or more output interfaces using bdg_forward(),
 * and/or sent to the upper layer (e.g. in case of multicast).
 *
 * Output packets are intercepted near the end of ether_output(),
 * the correct destination is selected calling bdg_dst_lookup(),
 * and then forwarding is done using bdg_forward().
 * Bridging is controlled by the sysctl variable net.link.ether.bridge
 *
 * The arp code is also modified to let a machine answer to requests
 * irrespective of the port the request came from.
 *
 * In case of loops in the bridging topology, the bridge detects this
 * event and temporarily mutes output bridging on one of the ports.
 * Periodically, interfaces are unmuted by bdg_timeout().
 * Muting is only implemented as a safety measure, and also as
 * a mechanism to support a user-space implementation of the spanning
 * tree algorithm. In the final release, unmuting will only occur
 * because of explicit action of the user-level daemon.
 *
 * To build a bridging kernel, use the following option
 *    option BRIDGE
 * and then at runtime set the sysctl variable to enable bridging.
 *
 * Only one interface is supposed to have addresses set (but
 * there are no problems in practice if you set addresses for more
 * than one interface).
 * Bridging will act before routing, but nothing prevents a machine
 * from doing both (modulo bugs in the implementation...).
 *
 * THINGS TO REMEMBER
 *  - bridging requires some (small) modifications to the interface
 *    driver. Not all of them have been changed, see the "ed" and "de"
 *    drivers as examples on how to operate.
 *  - bridging is incompatible with multicast routing on the same
 *    machine. There is not an easy fix to this.
 *  - loop detection is still not very robust.
 *  - the interface of bdg_forward() could be improved.
 */

#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/socket.h> /* for net/if.h */
#include <sys/kernel.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_types.h>

#include <netinet/in.h> /* for struct arpcom */
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h> /* for struct arpcom */

#include "opt_ipfw.h" 
#include "opt_ipdn.h" 

#if defined(IPFIREWALL)
#include <net/route.h>
#include <netinet/ip_fw.h>
#if defined(DUMMYNET)
#include <netinet/ip_dummynet.h>
#endif
#endif

#include <net/bridge.h>

/*
 * For debugging, you can use the following macros.
 * remember, rdtsc() only works on Pentium-class machines

    quad_t ticks;
    DDB(ticks = rdtsc();)
    ... interesting code ...
    DDB(bdg_fw_ticks += (u_long)(rdtsc() - ticks) ; bdg_fw_count++ ;)

 *
 */

#define DDB(x) x
#define DEB(x)

static void bdginit(void *);
static void bdgtakeifaces(void);
static void flush_table(void);
static void bdg_promisc_on(void);
static void parse_bdg_cfg(void);

static int bdg_ipfw = 0 ;
int do_bridge = 0;
bdg_hash_table *bdg_table = NULL ;

/*
 * System initialization
 */

SYSINIT(interfaces, SI_SUB_PROTO_IF, SI_ORDER_FIRST, bdginit, NULL)

/*
 * We need additional info for the bridge. The bdg_ifp2sc[] array
 * provides a pointer to this struct using the if_index.
 * bdg_softc has a backpointer to the struct ifnet, the bridge
 * flags, and a cluster (bridging occurs only between port of the
 * same cluster).
 */
struct bdg_softc {
    struct ifnet *ifp ;
    /* ((struct arpcom *)ifp)->ac_enaddr is the eth. addr */
    int flags ;
#define	IFF_BDG_PROMISC 0x0001  /* set promisc mode on this if.  */
#define	IFF_MUTE        0x0002  /* mute this if for bridging.   */
#define	IFF_USED        0x0004  /* use this if for bridging.    */
    short cluster_id ; /* in network format */
    u_long magic;
} ;

static struct bdg_stats bdg_stats ;
static struct bdg_softc *ifp2sc = NULL ;
/* XXX make it static of size BDG_MAX_PORTS */

#define	USED(ifp) (ifp2sc[ifp->if_index].flags & IFF_USED)
#define	MUTED(ifp) (ifp2sc[ifp->if_index].flags & IFF_MUTE)
#define	MUTE(ifp) ifp2sc[ifp->if_index].flags |= IFF_MUTE
#define	UNMUTE(ifp) ifp2sc[ifp->if_index].flags &= ~IFF_MUTE
#define	CLUSTER(ifp) (ifp2sc[ifp->if_index].cluster_id)
#define	IFP_CHK(ifp, x) \
	if (ifp2sc[ifp->if_index].magic != 0xDEADBEEF) { x ; }

#define	SAMECLUSTER(ifp,src) \
	(src == NULL || CLUSTER(ifp) == CLUSTER(src) )

/*
 * turn off promisc mode, optionally clear the IFF_USED flag
 */
static void
bdg_promisc_off(int clear_used)
{
    struct ifnet *ifp ;
    for (ifp = ifnet.tqh_first; ifp; ifp = ifp->if_link.tqe_next ) {
	if ( (ifp2sc[ifp->if_index].flags & IFF_BDG_PROMISC) ) {
	    int s, ret ;
	    s = splimp();
	    ret = ifpromisc(ifp, 0);
	    splx(s);
	    ifp2sc[ifp->if_index].flags &= ~(IFF_BDG_PROMISC|IFF_MUTE) ;
	    if (clear_used)
		ifp2sc[ifp->if_index].flags &= ~(IFF_USED) ;
	    printf(">> now %s%d promisc ON if_flags 0x%x bdg_flags 0x%x\n",
		    ifp->if_name, ifp->if_unit,
		    ifp->if_flags, ifp2sc[ifp->if_index].flags);
	}
    }
}

/*
 * set promisc mode on the interfaces we use.
 */
static void
bdg_promisc_on()
{
    struct ifnet *ifp ;
    int s ;

    for (ifp = ifnet.tqh_first; ifp; ifp = ifp->if_link.tqe_next ) {
	if ( !USED(ifp) )
	    continue ;
	if ( 0 == ( ifp->if_flags & IFF_UP) ) {
	    s = splimp();
	    if_up(ifp);
	    splx(s);
	}
	if ( !(ifp2sc[ifp->if_index].flags & IFF_BDG_PROMISC) ) {
	    int ret ;
	    s = splimp();
	    ret = ifpromisc(ifp, 1);
	    splx(s);
	    ifp2sc[ifp->if_index].flags |= IFF_BDG_PROMISC ;
	    printf(">> now %s%d promisc ON if_flags 0x%x bdg_flags 0x%x\n",
		    ifp->if_name, ifp->if_unit,
		    ifp->if_flags, ifp2sc[ifp->if_index].flags);
	}
	if (MUTED(ifp)) {
	    printf(">> unmuting %s%d\n", ifp->if_name, ifp->if_unit);
	    UNMUTE(ifp) ;
       }
    }
}

static int
sysctl_bdg(SYSCTL_HANDLER_ARGS)
{
    int error, oldval = do_bridge ;

    error = sysctl_handle_int(oidp,
	oidp->oid_arg1, oidp->oid_arg2, req);
    DEB( printf("called sysctl for bridge name %s arg2 %d val %d->%d\n",
	oidp->oid_name, oidp->oid_arg2,
	oldval, do_bridge); )

    if (bdg_table == NULL)
	do_bridge = 0 ;
    if (oldval != do_bridge) {
	flush_table();
	if (do_bridge == 0)
	    bdg_promisc_off( 0 ); /* leave IFF_USED set */
	else
	    bdg_promisc_on();
    }
    return error ;
}

static char bridge_cfg[256] = { "" } ;

/*
 * parse the config string, set IFF_USED, name and cluster_id
 * for all interfaces found.
 */
static void
parse_bdg_cfg()
{
    char *p, *beg ;
    int i, l, cluster;
    struct bdg_softc *b;

    for (p= bridge_cfg; *p ; p++) {
	/* interface names begin with [a-z]  and continue up to ':' */
	if (*p < 'a' || *p > 'z')
	    continue ;
	for ( beg = p ; *p && *p != ':' ; p++ )
	    ;
	if (*p == 0) /* end of string, ':' not found */
	    return ;
	l = p - beg ; /* length of name string */
	p++ ;
	DDB(printf("-- match beg(%d) <%s> p <%s>\n", l, beg, p);)
	for (cluster = 0 ; *p && *p >= '0' && *p <= '9' ; p++)
	    cluster = cluster*10 + (*p -'0');
	/*
	 * now search in bridge strings
	 */
	for (i=0, b = ifp2sc ; i < if_index ; i++, b++) {
	    char buf[32];
	    struct ifnet *ifp = b->ifp ;

	    if (ifp == NULL)
		continue;
	    sprintf(buf, "%s%d", ifp->if_name, ifp->if_unit);
	    if (!strncmp(beg, buf, l)) { /* XXX not correct for >10 if! */
		b->cluster_id = htons(cluster) ;
		b->flags |= IFF_USED ;
		sprintf(bdg_stats.s[ifp->if_index].name+l,
			":%d", cluster);

		DDB(printf("--++  found %s\n",
		    bdg_stats.s[ifp->if_index].name);)
		break ;
	    }
	}
    }
}

static int
sysctl_bdg_cfg(SYSCTL_HANDLER_ARGS)
{
    int error = 0 ;
    char oldval[256] ;

    strcpy(oldval, bridge_cfg) ;

    error = sysctl_handle_string(oidp,
	    bridge_cfg, oidp->oid_arg2, req);
    DEB(
	printf("called sysctl for bridge name %s arg2 %d err %d val %s->%s\n",
		oidp->oid_name, oidp->oid_arg2,
		error,
		oldval, bridge_cfg);
	)
    if (strcmp(oldval, bridge_cfg)) {
	bdg_promisc_off( 1 );  /* reset previously-used interfaces */
	flush_table();
	parse_bdg_cfg();        /* and set new ones... */
	if (do_bridge)
	    bdg_promisc_on();   /* re-enable interfaces */
    }
    return error ;
}

static int
sysctl_refresh(SYSCTL_HANDLER_ARGS)
{
    if (req->newptr)
	    bdgtakeifaces();
    
    return 0;
}


SYSCTL_DECL(_net_link_ether);
SYSCTL_PROC(_net_link_ether, OID_AUTO, bridge_cfg, CTLTYPE_STRING|CTLFLAG_RW,
	    &bridge_cfg, sizeof(bridge_cfg), &sysctl_bdg_cfg, "A",
	    "Bridge configuration");

SYSCTL_PROC(_net_link_ether, OID_AUTO, bridge, CTLTYPE_INT|CTLFLAG_RW,
	    &do_bridge, 0, &sysctl_bdg, "I", "Bridging");

SYSCTL_INT(_net_link_ether, OID_AUTO, bridge_ipfw, CTLFLAG_RW,
	    &bdg_ipfw,0,"Pass bridged pkts through firewall");

int bdg_ipfw_drops;
SYSCTL_INT(_net_link_ether, OID_AUTO, bridge_ipfw_drop,
	CTLFLAG_RW, &bdg_ipfw_drops,0,"");

int bdg_ipfw_colls;
SYSCTL_INT(_net_link_ether, OID_AUTO, bridge_ipfw_collisions,
	CTLFLAG_RW, &bdg_ipfw_colls,0,"");

SYSCTL_PROC(_net_link_ether, OID_AUTO, bridge_refresh, CTLTYPE_INT|CTLFLAG_WR,
	    NULL, 0, &sysctl_refresh, "I", "iface refresh");

#if 1 /* diagnostic vars */
int bdg_in_count = 0 , bdg_in_ticks = 0 , bdg_fw_count = 0, bdg_fw_ticks = 0 ;
SYSCTL_INT(_net_link_ether, OID_AUTO, bdginc, CTLFLAG_RW, &bdg_in_count,0,"");
SYSCTL_INT(_net_link_ether, OID_AUTO, bdgint, CTLFLAG_RW, &bdg_in_ticks,0,"");
SYSCTL_INT(_net_link_ether, OID_AUTO, bdgfwc, CTLFLAG_RW, &bdg_fw_count,0,"");
SYSCTL_INT(_net_link_ether, OID_AUTO, bdgfwt, CTLFLAG_RW, &bdg_fw_ticks,0,"");
#endif

SYSCTL_STRUCT(_net_link_ether, PF_BDG, bdgstats,
        CTLFLAG_RD, &bdg_stats , bdg_stats, "bridge statistics");

static int bdg_loops ;

/*
 * completely flush the bridge table.
 */
static void
flush_table()
{   
    int s,i;

    if (bdg_table == NULL)
	return ;
    s = splimp();
    for (i=0; i< HASH_SIZE; i++)
        bdg_table[i].name= NULL; /* clear table */
    splx(s);
}

/*
 * called periodically to flush entries etc.
 */
static void
bdg_timeout(void *dummy)
{
    static int slowtimer = 0 ;

    if (do_bridge) {
	static int age_index = 0 ; /* index of table position to age */
	int l = age_index + HASH_SIZE/4 ;
	/*
	 * age entries in the forwarding table.
	 */
	if (l > HASH_SIZE)
	    l = HASH_SIZE ;
	for (; age_index < l ; age_index++)
	    if (bdg_table[age_index].used)
		bdg_table[age_index].used = 0 ;
	    else if (bdg_table[age_index].name) {
		/* printf("xx flushing stale entry %d\n", age_index); */
		bdg_table[age_index].name = NULL ;
	    }
	if (age_index >= HASH_SIZE)
	    age_index = 0 ;

	if (--slowtimer <= 0 ) {
	    slowtimer = 5 ;

	    bdg_promisc_on() ; /* we just need unmute, really */
	    bdg_loops = 0 ;
	}
    }
    timeout(bdg_timeout, (void *)0, 2*hz );
}

/*
 * local MAC addresses are held in a small array. This makes comparisons
 * much faster.
 */
unsigned char bdg_addresses[6*BDG_MAX_PORTS];
int bdg_ports ;

/*
 * initialization of bridge code. This needs to be done after all
 * interfaces have been configured.
 */
static void
bdginit(void *dummy)
{

    if (bdg_table == NULL)
	bdg_table = (struct hash_table *)
		malloc(HASH_SIZE * sizeof(struct hash_table),
		    M_IFADDR, M_WAITOK);
    flush_table();

    ifp2sc = malloc(BDG_MAX_PORTS * sizeof(struct bdg_softc),
		M_IFADDR, M_WAITOK );
    bzero(ifp2sc, BDG_MAX_PORTS * sizeof(struct bdg_softc) );

    bzero(&bdg_stats, sizeof(bdg_stats) );
    bdgtakeifaces();
    bdg_timeout(0);
    do_bridge=0;
}
    
void
bdgtakeifaces(void)
{
    int i ;
    struct ifnet *ifp;
    struct arpcom *ac ;
    u_char *eth_addr ;
    struct bdg_softc *bp;

    bdg_ports = 0 ;
    eth_addr = bdg_addresses ;
    *bridge_cfg = '\0';

    printf("BRIDGE 990810, have %d interfaces\n", if_index);
    for (i = 0 , ifp = ifnet.tqh_first ; i < if_index ;
		i++, ifp = ifp->if_link.tqe_next)
	if (ifp->if_type == IFT_ETHER) { /* ethernet ? */
	    bp = &ifp2sc[ifp->if_index] ;
	    ac = (struct arpcom *)ifp;
	    sprintf(bridge_cfg + strlen(bridge_cfg),
		"%s%d:1,", ifp->if_name, ifp->if_unit);
	    printf("-- index %d %s type %d phy %d addrl %d addr %6D\n",
		    ifp->if_index,
		    bdg_stats.s[ifp->if_index].name,
		    (int)ifp->if_type, (int) ifp->if_physical,
		    (int)ifp->if_addrlen,
		    ac->ac_enaddr, "." );
	    bcopy(ac->ac_enaddr, eth_addr, 6);
	    eth_addr += 6 ;
	    bp->ifp = ifp ;
	    bp->flags = IFF_USED ;
	    bp->cluster_id = htons(1) ;
	    bp->magic = 0xDEADBEEF ;

	    sprintf(bdg_stats.s[ifp->if_index].name,
		"%s%d:%d", ifp->if_name, ifp->if_unit,
		ntohs(bp->cluster_id));
	    bdg_ports ++ ;
	}

}

/*
 * bridge_in() is invoked to perform bridging decision on input packets.
 *
 * On Input:
 *   eh		Ethernet header of the incoming packet.
 *
 * On Return: destination of packet, one of
 *   BDG_BCAST	broadcast
 *   BDG_MCAST  multicast
 *   BDG_LOCAL  is only for a local address (do not forward)
 *   BDG_DROP   drop the packet
 *   ifp	ifp of the destination interface.
 *
 * Forwarding is not done directly to give a chance to some drivers
 * to fetch more of the packet, or simply drop it completely.
 */

struct ifnet *
bridge_in(struct ifnet *ifp, struct ether_header *eh)
{
    int index;
    struct ifnet *dst , *old ;
    int dropit = MUTED(ifp) ;

    /*
     * hash the source address
     */
    index= HASH_FN(eh->ether_shost);
    bdg_table[index].used = 1 ;
    old = bdg_table[index].name ;
    if ( old ) { /* the entry is valid. */
	IFP_CHK(old, printf("bridge_in-- reading table\n") );

        if (!BDG_MATCH( eh->ether_shost, bdg_table[index].etheraddr) ) {
	    bdg_ipfw_colls++ ;
	    bdg_table[index].name = NULL ;
        } else if (old != ifp) {
	    /*
	     * found a loop. Either a machine has moved, or there
	     * is a misconfiguration/reconfiguration of the network.
	     * First, do not forward this packet!
	     * Record the relocation anyways; then, if loops persist,
	     * suspect a reconfiguration and disable forwarding
	     * from the old interface.
	     */
	    bdg_table[index].name = ifp ; /* relocate address */
	    printf("-- loop (%d) %6D to %s%d from %s%d (%s)\n",
			bdg_loops, eh->ether_shost, ".",
			ifp->if_name, ifp->if_unit,
			old->if_name, old->if_unit,
			MUTED(old) ? "muted":"active");
	    dropit = 1 ;
	    if ( !MUTED(old) ) {
		if (++bdg_loops > 10)
		    MUTE(old) ;
	    }
        }
    }

    /*
     * now write the source address into the table
     */
    if (bdg_table[index].name == NULL) {
	DEB(printf("new addr %6D at %d for %s%d\n",
	    eh->ether_shost, ".", index, ifp->if_name, ifp->if_unit);)
	bcopy(eh->ether_shost, bdg_table[index].etheraddr, 6);
	bdg_table[index].name = ifp ;
    }
    dst = bridge_dst_lookup(eh);
    /* Return values:
     *   BDG_BCAST, BDG_MCAST, BDG_LOCAL, BDG_UNKNOWN, BDG_DROP, ifp.
     * For muted interfaces, the first 3 are changed in BDG_LOCAL,
     * and others to BDG_DROP. Also, for incoming packets, ifp is changed
     * to BDG_DROP in case ifp == src . These mods are not necessary
     * for outgoing packets from ether_output().
     */
    BDG_STAT(ifp, BDG_IN);
    switch ((int)dst) {
    case (int)BDG_BCAST:
    case (int)BDG_MCAST:
    case (int)BDG_LOCAL:
    case (int)BDG_UNKNOWN:
    case (int)BDG_DROP:
	BDG_STAT(ifp, dst);
	break ;
    default :
	if (dst == ifp || dropit )
	    BDG_STAT(ifp, BDG_DROP);
	else
	    BDG_STAT(ifp, BDG_FORWARD);
	break ;
    }

    if ( dropit ) {
	if (dst == BDG_BCAST || dst == BDG_MCAST || dst == BDG_LOCAL)
	    return BDG_LOCAL ;
	else
	    return BDG_DROP ;
    } else {
	return (dst == ifp ? BDG_DROP : dst ) ;
    }
}

/*
 * Forward to dst, excluding src port and muted interfaces.
 * The packet is freed if possible (i.e. surely not of interest for
 * the upper layer), otherwise a copy is left for use by the caller
 * (pointer in *m0).
 *
 * It would be more efficient to make bdg_forward() always consume
 * the packet, leaving to the caller the task to check if it needs a copy
 * and get one in case. As it is now, bdg_forward() can sometimes make
 * a copy whereas it is not necessary.
 *
 * INPUT:
 *    *m0  -- ptr to pkt (not null at call time)
 *    *dst -- destination (special value or ifnet *)
 *    (*m0)->m_pkthdr.rcvif -- NULL only for output pkts.
 * OUTPUT:
 *    *m0  -- pointer to the packet (NULL if still existent)
 */
int
bdg_forward(struct mbuf **m0, struct ether_header *const eh, struct ifnet *dst)
{
    struct ifnet *src = (*m0)->m_pkthdr.rcvif; /* could be NULL in output */
    struct ifnet *ifp, *last = NULL ;
    int error=0, s ;
    int canfree = 0 ; /* can free the buf at the end if set */
    int once = 0;      /* loop only once */
    struct mbuf *m ;

    if (dst == BDG_DROP) { /* this should not happen */
	printf("xx bdg_forward for BDG_DROP\n");
	m_freem(*m0) ;
	*m0 = NULL ;
	return 0;
    }
    if (dst == BDG_LOCAL) { /* this should not happen as well */
	printf("xx ouch, bdg_forward for local pkt\n");
	return 0;
    }
    if (dst == BDG_BCAST || dst == BDG_MCAST || dst == BDG_UNKNOWN) {
	ifp = ifnet.tqh_first ; /* scan all ports */
	once = 0 ;
	if (dst != BDG_UNKNOWN) /* need a copy for the local stack */
	    canfree = 0 ;
    } else {
	ifp = dst ;
	once = 1 ;
    }
    if ( (u_int)(ifp) <= (u_int)BDG_FORWARD )
	panic("bdg_forward: bad dst");

#ifdef IPFIREWALL
    /*
     * do filtering in a very similar way to what is done
     * in ip_output. Only for IP packets, and only pass/fail/dummynet
     * is supported. The tricky thing is to make sure that enough of
     * the packet (basically, Eth+IP+TCP/UDP headers) is contiguous
     * so that calls to m_pullup in ip_fw_chk will not kill the
     * ethernet header.
     */
    if (ip_fw_chk_ptr) {
	struct ip_fw_chain *rule = NULL ;
	int off;
	struct ip *ip ;

	m = *m0 ;
#ifdef DUMMYNET
	if (m->m_type == MT_DUMMYNET) {
	    /*
	     * the packet was already tagged, so part of the
	     * processing was already done, and we need to go down.
	     */
	    rule = (struct ip_fw_chain *)(m->m_data) ;
	    (*m0) = m = m->m_next ;

	    src = m->m_pkthdr.rcvif; /* could be NULL in output */
	    canfree = 1 ; /* for sure, a copy is not needed later. */
	    goto forward; /* HACK! I should obey the fw_one_pass */
	}
#endif
	if (bdg_ipfw == 0) /* this test must be here. */
	    goto forward ;
	if (src == NULL)
	    goto forward ; /* do not apply to packets from ether_output */
	if (ntohs(eh->ether_type) != ETHERTYPE_IP)
	    goto forward ; /* not an IP packet, ipfw is not appropriate */
	/*
	 * In this section, canfree=1 means m is the same as *m0.
	 * canfree==0 means m is a copy. We need to make a copy here
	 * (to be destroyed on exit from the firewall section) because
	 * the firewall itself might destroy the packet.
	 * (This is not very smart... i should really change ipfw to
	 * leave the pkt alive!)
	 */
	if (canfree == 0 ) {
	    /*
	     * Need to make a copy (and for good measure, make sure that
	     * the header is contiguous). The original is still in *m0
	     */
	    int needed = min(MHLEN, max_protohdr) ;
	    needed = min(needed, (*m0)->m_len ) ;

	    m = m_copypacket( (*m0), M_DONTWAIT);
	    if (m == NULL) {
	        printf("-- bdg: m_copypacket failed.\n") ;
		return ENOBUFS ;
	    }
	    if (m->m_len < needed && (m = m_pullup(m, needed)) == NULL) {
		printf("-- bdg: pullup failed.\n") ;
		return ENOBUFS ;
	    }
	}

	/*
	 * before calling the firewall, swap fields the same as IP does.
	 * here we assume the pkt is an IP one and the header is contiguous
	 */
	ip = mtod(m, struct ip *);
	NTOHS(ip->ip_len);
	NTOHS(ip->ip_off);

	/*
	 * The third parameter to the firewall code is the dst.  interface.
	 * Since we apply checks only on input pkts we use NULL.
	 */
	off = (*ip_fw_chk_ptr)(&ip, 0, NULL, NULL, &m, &rule, NULL);

	if (m == NULL) { /* pkt discarded by firewall */
	    /*
	     * At this point, if canfree==1, m and *m0 were the same
	     * thing, so just clear ptr. Otherwise, leave it alone, the
	     * upper layer might still make use of it somewhere.
	     */
	    if (canfree)
		*m0 = NULL ;
	    return 0 ;
	}

	/*
	 * If we get here, the firewall has passed the pkt, but the
	 * mbuf pointer might have changed. Restore the fields NTOHS()'d.
	 * Then, if canfree==1, also restore *m0.
	 */
	HTONS(ip->ip_len);
	HTONS(ip->ip_off);
	if (canfree) /* m was a reference to *m0, so update *m0 */
	    *m0 = m ;

	if (off == 0) { /* a PASS rule.  */
	    if (canfree == 0) /* destroy the copy */
		m_freem(m);
	    goto forward ;
	}
#ifdef DUMMYNET
	if (off & 0x10000) {  
	    /*
	     * pass the pkt to dummynet. Need to include m, dst, rule.
	     * Dummynet consumes the packet in all cases.
	     */
	    dummynet_io((off & 0xffff), DN_TO_BDG_FWD, m, dst, NULL, 0, rule, 0);
	    if (canfree) /* dummynet has consumed the original one */
		*m0 = NULL ;
	    return 0 ;
	}
#endif
	/*
	 * XXX add divert/forward actions...
	 */
	/* if none of the above matches, we have to drop the pkt */
	bdg_ipfw_drops++ ;
	m_freem(m);
	if (canfree == 0) /* m was a copy */
	    m_freem(*m0);
	*m0 = NULL ;
	return 0 ;
    }
forward:
#endif /* IPFIREWALL */
    /*
     * Now *m0 is the only pkt left. If canfree != 0 the pkt might be
     * used by the upper layers which could scramble header fields.
     * (basically ntoh*() etc.). To avoid problems, make sure that
     * all fields that might be changed by the local network stack are not
     * in a cluster by calling m_pullup on *m0. We lose some efficiency
     * but better than having packets corrupt!
     */
    if (canfree == 0 ) {
	int needed = min(MHLEN, max_protohdr) ;
	needed = min(needed, (*m0)->m_len ) ;

	if ((*m0)->m_len < needed && (*m0 = m_pullup(*m0, needed)) == NULL) {
	    printf("-- bdg: pullup failed.\n") ;
	    return ENOBUFS ;
	}
    }
    for (;;) {
	if (last) { /* need to forward packet */
	    if (canfree && once ) { /* no need to copy */
		m = *m0 ;
		*m0 = NULL ; /* original is gone */
	    } else /* on a P5-90, m_copypacket takes 540 ticks */
		m = m_copypacket(*m0, M_DONTWAIT);
	    if (m == NULL) {
		printf("bdg_forward: sorry, m_copy failed!\n");
		return ENOBUFS ; /* the original is still there... */
	    }
	    /*
	     * Last part of ether_output: add header, queue pkt and start
	     * output if interface not yet active.
	     */
	    M_PREPEND(m, ETHER_HDR_LEN, M_DONTWAIT);
	    if (m == NULL)
		    return ENOBUFS;
	    bcopy(eh, mtod(m, struct ether_header *), ETHER_HDR_LEN);
	    s = splimp();
	    if (IF_QFULL(&last->if_snd)) {
		IF_DROP(&last->if_snd);
#if 0
		MUTE(last); /* should I also mute ? */
#endif
		splx(s);
		m_freem(m); /* consume the pkt anyways */
		error = ENOBUFS ;
	    } else {
		last->if_obytes += m->m_pkthdr.len ;
		if (m->m_flags & M_MCAST)
		    last->if_omcasts++;
		IF_ENQUEUE(&last->if_snd, m);
		if ((last->if_flags & IFF_OACTIVE) == 0)
		    (*last->if_start)(last);
		splx(s);
	    }
	    BDG_STAT(last, BDG_OUT);
	    last = NULL ;
	    if (once)
		break ;
	}
	if (ifp == NULL)
	    break ;
	if (ifp != src &&       /* do not send to self */
		USED(ifp) &&	/* if used for bridging */
		! IF_QFULL(&ifp->if_snd) &&
		(ifp->if_flags & (IFF_UP|IFF_RUNNING)) ==
			 (IFF_UP|IFF_RUNNING) &&
		SAMECLUSTER(ifp, src) && !MUTED(ifp) )
	    last = ifp ;
	ifp = ifp->if_link.tqe_next ;
	if (ifp == NULL)
	    once = 1 ;
    }

    return error ;
}
