/*
 * Copyright (c) 1998-2002 Luigi Rizzo
 *
 * Work partly supported by: Cisco Systems, Inc. - NSITE lab, RTP, NC
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
 * interfaces, including VLANs (others are still usable for routing).
 * A FreeBSD host can implement multiple logical bridges, called
 * "clusters". Each cluster is made of a set of interfaces, and
 * identified by a "cluster-id" which is a number in the range 1..2^16-1.
 * 
 * Bridging is enabled by the sysctl variable
 *	net.link.ether.bridge
 * the grouping of interfaces into clusters is done with
 *	net.link.ether.bridge_cfg
 * containing a list of interfaces each optionally followed by
 * a colon and the cluster it belongs to (1 is the default).
 * Separators can be * spaces, commas or tabs, e.g.
 *	net.link.ether.bridge_cfg="fxp0:2 fxp1:2 dc0 dc1:1"
 * Optionally bridged packets can be passed through the firewall,
 * this is controlled by the variable
 *	net.link.ether.bridge_ipfw
 *
 * For each cluster there is a descriptor (cluster_softc) storing
 * the following data structures:
 * - a hash table with the MAC address and destination interface for each
 *   known node. The table is indexed using a hash of the source address.
 * - an array with the MAC addresses of the interfaces used in the cluster.
 *
 * Input packets are tapped near the beginning of ether_input(), and
 * analysed by bridge_in(). Depending on the result, the packet
 * can be forwarded to one or more output interfaces using bdg_forward(),
 * and/or sent to the upper layer (e.g. in case of multicast).
 *
 * Output packets are intercepted near the end of ether_output().
 * The correct destination is selected by bridge_dst_lookup(),
 * and then forwarding is done by bdg_forward().
 *
 * The arp code is also modified to let a machine answer to requests
 * irrespective of the port the request came from.
 *
 * In case of loops in the bridging topology, the bridge detects this
 * event and temporarily mutes output bridging on one of the ports.
 * Periodically, interfaces are unmuted by bdg_timeout().
 * Muting is only implemented as a safety measure, and also as
 * a mechanism to support a user-space implementation of the spanning
 * tree algorithm.
 *
 * To build a bridging kernel, use the following option
 *    option BRIDGE
 * and then at runtime set the sysctl variable to enable bridging.
 *
 * Only one interface per cluster is supposed to have addresses set (but
 * there are no substantial problems if you set addresses for none or
 * for more than one interface).
 * Bridging will act before routing, but nothing prevents a machine
 * from doing both (modulo bugs in the implementation...).
 *
 * THINGS TO REMEMBER
 *  - bridging is incompatible with multicast routing on the same
 *    machine. There is not an easy fix to this.
 *  - be very careful when bridging VLANs
 *  - loop detection is still not very robust.
 */

#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/protosw.h>
#include <sys/systm.h>
#include <sys/socket.h> /* for net/if.h */
#include <sys/ctype.h>	/* string functions */
#include <sys/kernel.h>
#include <sys/sysctl.h>

#if 0	/* XXX does not work yet */
#include <net/pfil.h>	/* for ipfilter */
#endif
#include <net/if.h>
#include <net/if_types.h>
#include <net/if_var.h>

#include <netinet/in.h> /* for struct arpcom */
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h> /* for struct arpcom */

#include <net/route.h>
#include <netinet/ip_fw.h>
#include <netinet/ip_dummynet.h>
#include <net/bridge.h>

/*--------------------*/

/*
 * For each cluster, source MAC addresses are stored into a hash
 * table which locates the port they reside on.
 */
#define HASH_SIZE 8192	/* Table size, must be a power of 2 */

typedef struct hash_table {		/* each entry.		*/
    struct ifnet *	name;
    u_char		etheraddr[6];
    u_int16_t		used;		/* also, padding	*/
} bdg_hash_table ;

/*
 * The hash function applied to MAC addresses. Out of the 6 bytes,
 * the last ones tend to vary more. Since we are on a little endian machine,
 * we have to do some gimmick...
 */
#define HASH_FN(addr)   (	\
    ntohs( ((u_int16_t *)addr)[1] ^ ((u_int16_t *)addr)[2] ) & (HASH_SIZE -1))

/*
 * This is the data structure where local addresses are stored.
 */
struct bdg_addr {
    u_char	etheraddr[6] ;
    u_int16_t	_padding ;
};

/*
 * The configuration of each cluster includes the cluster id, a pointer to
 * the hash table, and an array of local MAC addresses (of size "ports").
 */
struct cluster_softc {
    u_int16_t	cluster_id;
    u_int16_t	ports;
    bdg_hash_table *ht;
    struct bdg_addr	*my_macs;	/* local MAC addresses */
};


extern struct protosw inetsw[];			/* from netinet/ip_input.c */
extern u_char ip_protox[];			/* from netinet/ip_input.c */

static int n_clusters;				/* number of clusters */
static struct cluster_softc *clusters;

#define BDG_MUTED(ifp) (ifp2sc[ifp->if_index].flags & IFF_MUTE)
#define BDG_MUTE(ifp) ifp2sc[ifp->if_index].flags |= IFF_MUTE
#define BDG_CLUSTER(ifp) (ifp2sc[ifp->if_index].cluster)

#define BDG_SAMECLUSTER(ifp,src) \
	(src == NULL || BDG_CLUSTER(ifp) == BDG_CLUSTER(src) )

#ifdef __i386__
#define BDG_MATCH(a,b) ( \
    ((u_int16_t *)(a))[2] == ((u_int16_t *)(b))[2] && \
    *((u_int32_t *)(a)) == *((u_int32_t *)(b)) )
#define IS_ETHER_BROADCAST(a) ( \
	*((u_int32_t *)(a)) == 0xffffffff && \
	((u_int16_t *)(a))[2] == 0xffff )
#else
/* for machines that do not support unaligned access */
#define	BDG_MATCH(a,b)		(!bcmp(a, b, ETHER_ADDR_LEN) )
#define	IS_ETHER_BROADCAST(a)	(!bcmp(a, "\377\377\377\377\377\377", 6))
#endif


/*
 * For timing-related debugging, you can use the following macros.
 * remember, rdtsc() only works on Pentium-class machines

    quad_t ticks;
    DDB(ticks = rdtsc();)
    ... interesting code ...
    DDB(bdg_fw_ticks += (u_long)(rdtsc() - ticks) ; bdg_fw_count++ ;)

 *
 */

#define DDB(x) x
#define DEB(x)

static int bdginit(void);
static void parse_bdg_cfg(void);

static int bdg_ipf;		/* IPFilter enabled in bridge */
static int bdg_ipfw;

#if 0 /* debugging only */
static char *bdg_dst_names[] = {
	"BDG_NULL    ",
	"BDG_BCAST   ",
	"BDG_MCAST   ",
	"BDG_LOCAL   ",
	"BDG_DROP    ",
	"BDG_UNKNOWN ",
	"BDG_IN      ",
	"BDG_OUT     ",
	"BDG_FORWARD " };
#endif
/*
 * System initialization
 */

static struct bdg_stats bdg_stats ;
static struct callout_handle bdg_timeout_h ;

/*
 * Add an interface to a cluster, possibly creating a new entry in
 * the cluster table. This requires reallocation of the table and
 * updating pointers in ifp2sc.
 */
static struct cluster_softc *
add_cluster(u_int16_t cluster_id, struct arpcom *ac)
{
    struct cluster_softc *c = NULL;
    int i;

    for (i = 0; i < n_clusters ; i++)
	if (clusters[i].cluster_id == cluster_id)
	    goto found;

    /* Not found, need to reallocate */
    c = malloc((1+n_clusters) * sizeof (*c), M_IFADDR, M_DONTWAIT | M_ZERO);
    if (c == NULL) {/* malloc failure */
	printf("-- bridge: cannot add new cluster\n");
	return NULL;
    }
    c[n_clusters].ht = (struct hash_table *)
	    malloc(HASH_SIZE * sizeof(struct hash_table),
		M_IFADDR, M_WAITOK | M_ZERO);
    if (c[n_clusters].ht == NULL) {
	printf("-- bridge: cannot allocate hash table for new cluster\n");
	free(c, M_IFADDR);
	return NULL;
    }
    c[n_clusters].my_macs = (struct bdg_addr *)
	    malloc(BDG_MAX_PORTS * sizeof(struct bdg_addr),
		M_IFADDR, M_WAITOK | M_ZERO);
    if (c[n_clusters].my_macs == NULL) {
        printf("-- bridge: cannot allocate mac addr table for new cluster\n");
	free(c[n_clusters].ht, M_IFADDR);
        free(c, M_IFADDR);
        return NULL;
    }

    c[n_clusters].cluster_id = cluster_id;
    c[n_clusters].ports = 0;
    /*
     * now copy old descriptors here
     */
    if (n_clusters > 0) {
	for (i=0; i < n_clusters; i++)
	    c[i] = clusters[i];
	/*
	 * and finally update pointers in ifp2sc
	 */
	for (i = 0 ; i < if_index && i < BDG_MAX_PORTS; i++)
	    if (ifp2sc[i].cluster != NULL)
		ifp2sc[i].cluster = c + (ifp2sc[i].cluster - clusters);
	free(clusters, M_IFADDR);
    }
    clusters = c;
    i = n_clusters;		/* index of cluster entry */
    n_clusters++;
found:
    c = clusters + i;		/* the right cluster ... */
    bcopy(ac->ac_enaddr, &(c->my_macs[c->ports]), 6);
    c->ports++;
    return c;
}


/*
 * Turn off bridging, by clearing promisc mode on the interface,
 * marking the interface as unused, and clearing the name in the
 * stats entry.
 * Also dispose the hash tables associated with the clusters.
 */
static void
bridge_off(void)
{
    struct ifnet *ifp ;
    int i, s;

    DEB(printf("bridge_off: n_clusters %d\n", n_clusters);)
    TAILQ_FOREACH(ifp, &ifnet, if_link) {
	struct bdg_softc *b;

	if (ifp->if_index >= BDG_MAX_PORTS)
	    continue;	/* make sure we do not go beyond the end */
	b = &(ifp2sc[ifp->if_index]);

	if ( b->flags & IFF_BDG_PROMISC ) {
	    s = splimp();
	    ifpromisc(ifp, 0);
	    splx(s);
	    b->flags &= ~(IFF_BDG_PROMISC|IFF_MUTE) ;
	    DEB(printf(">> now %s%d promisc OFF if_flags 0x%x bdg_flags 0x%x\n",
		    ifp->if_name, ifp->if_unit,
		    ifp->if_flags, b->flags);)
	}
	b->flags &= ~(IFF_USED) ;
	b->cluster = NULL;
	bdg_stats.s[ifp->if_index].name[0] = '\0';
    }
    /* flush_tables */

    s = splimp();
    for (i=0; i < n_clusters; i++) {
	free(clusters[i].ht, M_IFADDR);
	free(clusters[i].my_macs, M_IFADDR);
    }
    if (clusters != NULL)
	free(clusters, M_IFADDR);
    clusters = NULL;
    n_clusters =0;
    splx(s);
}

/*
 * set promisc mode on the interfaces we use.
 */
static void
bridge_on(void)
{
    struct ifnet *ifp ;
    int s ;

    TAILQ_FOREACH(ifp, &ifnet, if_link) {
	struct bdg_softc *b = &ifp2sc[ifp->if_index];

	if ( !(b->flags & IFF_USED) )
	    continue ;
	if ( !( ifp->if_flags & IFF_UP) ) {
	    s = splimp();
	    if_up(ifp);
	    splx(s);
	}
	if ( !(b->flags & IFF_BDG_PROMISC) ) {
	    int ret ;
	    s = splimp();
	    ret = ifpromisc(ifp, 1);
	    splx(s);
	    b->flags |= IFF_BDG_PROMISC ;
	    DEB(printf(">> now %s%d promisc ON if_flags 0x%x bdg_flags 0x%x\n",
		    ifp->if_name, ifp->if_unit,
		    ifp->if_flags, b->flags);)
	}
	if (b->flags & IFF_MUTE) {
	    DEB(printf(">> unmuting %s%d\n", ifp->if_name, ifp->if_unit);)
	    b->flags &= ~IFF_MUTE;
	}
    }
}

/**
 * reconfigure bridge.
 * This is also done every time we attach or detach an interface.
 * Main use is to make sure that we do not bridge on some old
 * (ejected) device. So, it would be really useful to have a
 * pointer to the modified device as an argument. Without it, we
 * have to scan all interfaces.
 */
static void
reconfigure_bridge(void)
{
    bridge_off();
    if (do_bridge) {
	if (if_index >= BDG_MAX_PORTS) {
	    printf("-- sorry too many interfaces (%d, max is %d),"
		" disabling bridging\n", if_index, BDG_MAX_PORTS);
	    do_bridge=0;
	    return;
	}
	parse_bdg_cfg();
	bridge_on();
    }
}

static char bridge_cfg[1024]; /* in BSS so initialized to all NULs */

/*
 * parse the config string, set IFF_USED, name and cluster_id
 * for all interfaces found.
 * The config string is a list of "if[:cluster]" with
 * a number of possible separators (see "sep"). In particular the
 * use of the space lets you set bridge_cfg with the output from
 * "ifconfig -l"
 */
static void
parse_bdg_cfg()
{
    char *p, *beg ;
    int l, cluster;
    static char *sep = ", \t";

    for (p = bridge_cfg; *p ; p++) {
	struct ifnet *ifp;
	int found = 0;
	char c;

	if (index(sep, *p))	/* skip separators */
	    continue ;
	/* names are lowercase and digits */
	for ( beg = p ; islower(*p) || isdigit(*p) ; p++ )
	    ;
	l = p - beg ;		/* length of name string */
	if (l == 0)		/* invalid name */
	    break ;
	if ( *p != ':' )	/* no ':', assume default cluster 1 */
	    cluster = 1 ;
	else			/* fetch cluster */
	    cluster = strtoul( p+1, &p, 10);
	c = *p;
	*p = '\0';
	/*
	 * now search in interface list for a matching name
	 */
	TAILQ_FOREACH(ifp, &ifnet, if_link) {
	    char buf[IFNAMSIZ];

	    snprintf(buf, sizeof(buf), "%s%d", ifp->if_name, ifp->if_unit);
	    if (!strncmp(beg, buf, max(l, strlen(buf)))) {
		struct bdg_softc *b = &ifp2sc[ifp->if_index];
		if (ifp->if_type != IFT_ETHER && ifp->if_type != IFT_L2VLAN) {
		    printf("%s is not an ethernet, continue\n", buf);
		    continue;
		}
		if (b->flags & IFF_USED) {
		    printf("%s already used, skipping\n", buf);
		    break;
		}
		b->cluster = add_cluster(htons(cluster), (struct arpcom *)ifp);
		b->flags |= IFF_USED ;
		sprintf(bdg_stats.s[ifp->if_index].name,
			"%s%d:%d", ifp->if_name, ifp->if_unit, cluster);

		DEB(printf("--++  found %s next c %d\n",
		    bdg_stats.s[ifp->if_index].name, c);)
		found = 1;
		break ;
	    }
	}
	if (!found)
	    printf("interface %s Not found in bridge\n", beg);
	*p = c;
	if (c == '\0')
	    break; /* no more */
    }
}


/*
 * handler for net.link.ether.bridge
 */
static int
sysctl_bdg(SYSCTL_HANDLER_ARGS)
{
    int error, oldval = do_bridge ;

    error = sysctl_handle_int(oidp, oidp->oid_arg1, oidp->oid_arg2, req);
    DEB( printf("called sysctl for bridge name %s arg2 %d val %d->%d\n",
	oidp->oid_name, oidp->oid_arg2,
	oldval, do_bridge); )

    if (oldval != do_bridge)
	reconfigure_bridge();
    return error ;
}

/*
 * handler for net.link.ether.bridge_cfg
 */
static int
sysctl_bdg_cfg(SYSCTL_HANDLER_ARGS)
{
    int error = 0 ;
    char old_cfg[1024] ;

    strcpy(old_cfg, bridge_cfg) ;

    error = sysctl_handle_string(oidp, bridge_cfg, oidp->oid_arg2, req);
    DEB(
	printf("called sysctl for bridge name %s arg2 %d err %d val %s->%s\n",
		oidp->oid_name, oidp->oid_arg2,
		error,
		old_cfg, bridge_cfg);
	)
    if (strcmp(old_cfg, bridge_cfg))
	reconfigure_bridge();
    return error ;
}

static int
sysctl_refresh(SYSCTL_HANDLER_ARGS)
{
    if (req->newptr)
	reconfigure_bridge();
    
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

SYSCTL_INT(_net_link_ether, OID_AUTO, bridge_ipf, CTLFLAG_RW,
	    &bdg_ipf, 0,"Pass bridged pkts through IPFilter");

/*
 * The follow macro declares a variable, and maps it to
 * a SYSCTL_INT entry with the same name.
 */
#define SY(parent, var, comment)			\
	static int var ;				\
	SYSCTL_INT(parent, OID_AUTO, var, CTLFLAG_RW, &(var), 0, comment);

int bdg_ipfw_drops;
SYSCTL_INT(_net_link_ether, OID_AUTO, bridge_ipfw_drop,
	CTLFLAG_RW, &bdg_ipfw_drops,0,"");

int bdg_ipfw_colls;
SYSCTL_INT(_net_link_ether, OID_AUTO, bridge_ipfw_collisions,
	CTLFLAG_RW, &bdg_ipfw_colls,0,"");

SYSCTL_PROC(_net_link_ether, OID_AUTO, bridge_refresh, CTLTYPE_INT|CTLFLAG_WR,
	    NULL, 0, &sysctl_refresh, "I", "iface refresh");

#if 1 /* diagnostic vars */

SY(_net_link_ether, verbose, "Be verbose");
SY(_net_link_ether, bdg_split_pkts, "Packets split in bdg_forward");

SY(_net_link_ether, bdg_thru, "Packets through bridge");

SY(_net_link_ether, bdg_copied, "Packets copied in bdg_forward");

SY(_net_link_ether, bdg_copy, "Force copy in bdg_forward");
SY(_net_link_ether, bdg_predict, "Correctly predicted header location");

SY(_net_link_ether, bdg_fw_avg, "Cycle counter avg");
SY(_net_link_ether, bdg_fw_ticks, "Cycle counter item");
SY(_net_link_ether, bdg_fw_count, "Cycle counter count");
#endif

SYSCTL_STRUCT(_net_link_ether, PF_BDG, bdgstats,
	CTLFLAG_RD, &bdg_stats , bdg_stats, "bridge statistics");

static int bdg_loops ;

/*
 * called periodically to flush entries etc.
 */
static void
bdg_timeout(void *dummy)
{
    static int slowtimer;	/* in BSS so initialized to 0 */

    if (do_bridge) {
	static int age_index = 0 ; /* index of table position to age */
	int l = age_index + HASH_SIZE/4 ;
	int i;
	/*
	 * age entries in the forwarding table.
	 */
	if (l > HASH_SIZE)
	    l = HASH_SIZE ;

    for (i=0; i<n_clusters; i++) {
	bdg_hash_table *bdg_table = clusters[i].ht;
	for (; age_index < l ; age_index++)
	    if (bdg_table[age_index].used)
		bdg_table[age_index].used = 0 ;
	    else if (bdg_table[age_index].name) {
		/* printf("xx flushing stale entry %d\n", age_index); */
		bdg_table[age_index].name = NULL ;
	    }
    }
	if (age_index >= HASH_SIZE)
	    age_index = 0 ;

	if (--slowtimer <= 0 ) {
	    slowtimer = 5 ;

	    bridge_on() ; /* we just need unmute, really */
	    bdg_loops = 0 ;
	}
    }
    bdg_timeout_h = timeout(bdg_timeout, NULL, 2*hz );
}

/*
 * Find the right pkt destination:
 *	BDG_BCAST	is a broadcast
 *	BDG_MCAST	is a multicast
 *	BDG_LOCAL	is for a local address
 *	BDG_DROP	must be dropped
 *	other		ifp of the dest. interface (incl.self)
 *
 * We assume this is only called for interfaces for which bridging
 * is enabled, i.e. BDG_USED(ifp) is true.
 */
static __inline
struct ifnet *
bridge_dst_lookup(struct ether_header *eh, struct cluster_softc *c)
{
    struct ifnet *dst ;
    int index ;
    struct bdg_addr *p ;
    bdg_hash_table *bt;		/* pointer to entry in hash table */

    if (IS_ETHER_BROADCAST(eh->ether_dhost))
	return BDG_BCAST ;
    if (eh->ether_dhost[0] & 1)
	return BDG_MCAST ;
    /*
     * Lookup local addresses in case one matches.
     */
    for (index = c->ports, p = c->my_macs; index ; index--, p++ )
	if (BDG_MATCH(p->etheraddr, eh->ether_dhost) )
	    return BDG_LOCAL ;
    /*
     * Look for a possible destination in table
     */
    index= HASH_FN( eh->ether_dhost );
    bt = &(c->ht[index]);
    dst = bt->name;
    if ( dst && BDG_MATCH( bt->etheraddr, eh->ether_dhost) )
	return dst ;
    else
	return BDG_UNKNOWN ;
}

/**
 * bridge_in() is invoked to perform bridging decision on input packets.
 *
 * On Input:
 *   eh		Ethernet header of the incoming packet.
 *   ifp	interface the packet is coming from.
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

static struct ifnet *
bridge_in(struct ifnet *ifp, struct ether_header *eh)
{
    int index;
    struct ifnet *dst , *old ;
    bdg_hash_table *bt;			/* location in hash table */
    int dropit = BDG_MUTED(ifp) ;

    /*
     * hash the source address
     */
    index= HASH_FN(eh->ether_shost);
    bt = &(ifp2sc[ifp->if_index].cluster->ht[index]);
    bt->used = 1 ;
    old = bt->name ;
    if ( old ) { /* the entry is valid. */
	if (!BDG_MATCH( eh->ether_shost, bt->etheraddr) ) {
	    bdg_ipfw_colls++ ;
	    bt->name = NULL ;
	} else if (old != ifp) {
	    /*
	     * Found a loop. Either a machine has moved, or there
	     * is a misconfiguration/reconfiguration of the network.
	     * First, do not forward this packet!
	     * Record the relocation anyways; then, if loops persist,
	     * suspect a reconfiguration and disable forwarding
	     * from the old interface.
	     */
	    bt->name = ifp ; /* relocate address */
	    printf("-- loop (%d) %6D to %s%d from %s%d (%s)\n",
			bdg_loops, eh->ether_shost, ".",
			ifp->if_name, ifp->if_unit,
			old->if_name, old->if_unit,
			BDG_MUTED(old) ? "muted":"active");
	    dropit = 1 ;
	    if ( !BDG_MUTED(old) ) {
		if (++bdg_loops > 10)
		    BDG_MUTE(old) ;
	    }
	}
    }

    /*
     * now write the source address into the table
     */
    if (bt->name == NULL) {
	DEB(printf("new addr %6D at %d for %s%d\n",
	    eh->ether_shost, ".", index, ifp->if_name, ifp->if_unit);)
	bcopy(eh->ether_shost, bt->etheraddr, 6);
	bt->name = ifp ;
    }
    dst = bridge_dst_lookup(eh, ifp2sc[ifp->if_index].cluster);
    /*
     * bridge_dst_lookup can return the following values:
     *   BDG_BCAST, BDG_MCAST, BDG_LOCAL, BDG_UNKNOWN, BDG_DROP, ifp.
     * For muted interfaces, or when we detect a loop, the first 3 are
     * changed in BDG_LOCAL (we still listen to incoming traffic),
     * and others to BDG_DROP (no use for the local host).
     * Also, for incoming packets, ifp is changed to BDG_DROP if ifp == src.
     * These changes are not necessary for outgoing packets from ether_output().
     */
    BDG_STAT(ifp, BDG_IN);
    switch ((uintptr_t)dst) {
    case (uintptr_t)BDG_BCAST:
    case (uintptr_t)BDG_MCAST:
    case (uintptr_t)BDG_LOCAL:
    case (uintptr_t)BDG_UNKNOWN:
    case (uintptr_t)BDG_DROP:
	BDG_STAT(ifp, dst);
	break ;
    default :
	if (dst == ifp || dropit)
	    BDG_STAT(ifp, BDG_DROP);
	else
	    BDG_STAT(ifp, BDG_FORWARD);
	break ;
    }

    if ( dropit ) {
	if (dst == BDG_BCAST || dst == BDG_MCAST || dst == BDG_LOCAL)
	    dst = BDG_LOCAL ;
	else
	    dst = BDG_DROP ;
    } else {
	if (dst == ifp)
	    dst = BDG_DROP;
    }
    DEB(printf("bridge_in %6D ->%6D ty 0x%04x dst %s%d\n",
	eh->ether_shost, ".",
	eh->ether_dhost, ".",
	ntohs(eh->ether_type),
	(dst <= BDG_FORWARD) ? bdg_dst_names[(int)dst] :
		dst->if_name,
	(dst <= BDG_FORWARD) ? 0 : dst->if_unit); )

    return dst ;
}

/*
 * Forward a packet to dst -- which can be a single interface or
 * an entire cluster. The src port and muted interfaces are excluded.
 *
 * If src == NULL, the pkt comes from ether_output, and dst is the real
 * interface the packet is originally sent to. In this case, we must forward
 * it to the whole cluster.
 * We never call bdg_forward from ether_output on interfaces which are
 * not part of a cluster.
 *
 * If possible (i.e. we can determine that the caller does not need
 * a copy), the packet is consumed here, and bdg_forward returns NULL.
 * Otherwise, a pointer to a copy of the packet is returned.
 *
 * XXX be careful with eh, it can be a pointer into *m
 */
static struct mbuf *
bdg_forward(struct mbuf *m0, struct ether_header *const eh, struct ifnet *dst)
{
    struct ifnet *src;
    struct ifnet *ifp, *last;
    int shared = bdg_copy ; /* someone else is using the mbuf */
    int once = 0;      /* loop only once */
    struct ifnet *real_dst = dst ; /* real dst from ether_output */
    struct ip_fw_args args;
#ifdef PFIL_HOOKS
    struct packet_filter_hook *pfh;
    int rv;
#endif /* PFIL_HOOKS */

    /*
     * XXX eh is usually a pointer within the mbuf (some ethernet drivers
     * do that), so we better copy it before doing anything with the mbuf,
     * or we might corrupt the header.
     */
    struct ether_header save_eh = *eh ;

    DEB(quad_t ticks; ticks = rdtsc();)

    args.rule = NULL;		/* did we match a firewall rule ? */
    /* Fetch state from dummynet tag, ignore others */
    for (;m0->m_type == MT_TAG; m0 = m0->m_next)
	if (m0->m_tag_id == PACKET_TAG_DUMMYNET) {
	    args.rule = ((struct dn_pkt *)m0)->rule;
	    shared = 0;		/* For sure this is our own mbuf. */
	}
    if (args.rule == NULL)
	bdg_thru++; /* first time through bdg_forward, count packet */

    src = m0->m_pkthdr.rcvif;
    if (src == NULL)			/* packet from ether_output */
	dst = bridge_dst_lookup(eh, ifp2sc[real_dst->if_index].cluster);

    if (dst == BDG_DROP) { /* this should not happen */
	printf("xx bdg_forward for BDG_DROP\n");
	m_freem(m0);
	return NULL;
    }
    if (dst == BDG_LOCAL) { /* this should not happen as well */
	printf("xx ouch, bdg_forward for local pkt\n");
	return m0;
    }
    if (dst == BDG_BCAST || dst == BDG_MCAST || dst == BDG_UNKNOWN) {
	ifp = TAILQ_FIRST(&ifnet) ; /* scan all ports */
	once = 0 ;
	if (dst != BDG_UNKNOWN) /* need a copy for the local stack */
	    shared = 1 ;
    } else {
	ifp = dst ;
	once = 1 ;
    }
    if ( (u_int)(ifp) <= (u_int)BDG_FORWARD )
	panic("bdg_forward: bad dst");

    /*
     * Do filtering in a very similar way to what is done in ip_output.
     * Only if firewall is loaded, enabled, and the packet is not
     * from ether_output() (src==NULL, or we would filter it twice).
     * Additional restrictions may apply e.g. non-IP, short packets,
     * and pkts already gone through a pipe.
     */
    if (src != NULL && (
#ifdef PFIL_HOOKS
	((pfh = pfil_hook_get(PFIL_IN, &inetsw[ip_protox[IPPROTO_IP]].pr_pfh)) != NULL && bdg_ipf !=0) ||
#endif
	(IPFW_LOADED && bdg_ipfw != 0))) {

	int i;

	if (args.rule != NULL) /* packet already partially processed */
	    goto forward; /* HACK! I should obey the fw_one_pass */
	/*
	 * i need some amt of data to be contiguous, and in case others need
	 * the packet (shared==1) also better be in the first mbuf.
	 */
	i = min(m0->m_pkthdr.len, max_protohdr) ;
	if ( shared || m0->m_len < i) {
	    m0 = m_pullup(m0, i) ;
	    if (m0 == NULL) {
		printf("-- bdg: pullup failed.\n") ;
		return NULL ;
	    }
	}

#ifdef PFIL_HOOKS
	/*
	 * NetBSD-style generic packet filter, pfil(9), hooks.
	 * Enables ipf(8) in bridging.
	 */
	if (m0->m_pkthdr.len >= sizeof(struct ip) &&
		ntohs(save_eh.ether_type) == ETHERTYPE_IP) {
	    /*
	     * before calling the firewall, swap fields the same as IP does.
	     * here we assume the pkt is an IP one and the header is contiguous
	     */
	    struct ip *ip = mtod(m0, struct ip *);

	    ip->ip_len = ntohs(ip->ip_len);
	    ip->ip_off = ntohs(ip->ip_off);

	    for (; pfh; pfh = TAILQ_NEXT(pfh, pfil_link))
		if (pfh->pfil_func) {
		    rv = pfh->pfil_func(ip, ip->ip_hl << 2, src, 0, &m0);
		    if (rv != 0 || m0 == NULL)
			return m0;
		    ip = mtod(m0, struct ip *);
		}
	    /*
	     * If we get here, the firewall has passed the pkt, but the mbuf
	     * pointer might have changed. Restore ip and the fields ntohs()'d.
	     */
	    ip = mtod(m0, struct ip *);
	    ip->ip_len = htons(ip->ip_len);
	    ip->ip_off = htons(ip->ip_off);
	}
#endif /* PFIL_HOOKS */

	/*
	 * Prepare arguments and call the firewall.
	 */
	if (!IPFW_LOADED || bdg_ipfw == 0)
	    goto forward;	/* not using ipfw, accept the packet */

	/*
	 * XXX The following code is very similar to the one in
	 * if_ethersubr.c:ether_ipfw_chk()
	 */

	args.m = m0;		/* the packet we are looking at		*/
	args.oif = NULL;	/* this is an input packet		*/
	args.divert_rule = 0;	/* we do not support divert yet		*/
	args.next_hop = NULL;	/* we do not support forward yet	*/
	args.eh = &save_eh;	/* MAC header for bridged/MAC packets	*/
	i = ip_fw_chk_ptr(&args);
	m0 = args.m;		/* in case the firewall used the mbuf	*/

	if ( (i & IP_FW_PORT_DENY_FLAG) || m0 == NULL) /* drop */
	    return m0 ;

	if (i == 0) /* a PASS rule.  */
	    goto forward ;
	if (DUMMYNET_LOADED && (i & IP_FW_PORT_DYNT_FLAG)) {
	    /*
	     * Pass the pkt to dummynet, which consumes it.
	     * If shared, make a copy and keep the original.
	     */
	    struct mbuf *m ;

	    if (shared) {
		m = m_copypacket(m0, M_DONTWAIT);
		if (m == NULL)	/* copy failed, give up */
		    return m0;
	    } else {
		m = m0 ; /* pass the original to dummynet */
		m0 = NULL ; /* and nothing back to the caller */
	    }
	    /*
	     * Prepend the header, optimize for the common case of
	     * eh pointing into the mbuf.
	     */
	    if ( (void *)(eh + 1) == (void *)m->m_data) {
		m->m_data -= ETHER_HDR_LEN ;
		m->m_len += ETHER_HDR_LEN ;
		m->m_pkthdr.len += ETHER_HDR_LEN ;
		bdg_predict++;
	    } else {
		M_PREPEND(m, ETHER_HDR_LEN, M_DONTWAIT);
		if (m == NULL) /* nope... */
		    return m0 ;
		bcopy(&save_eh, mtod(m, struct ether_header *), ETHER_HDR_LEN);
	    }

	    args.oif = real_dst;
	    ip_dn_io_ptr(m, (i & 0xffff),DN_TO_BDG_FWD, &args);
	    return m0 ;
	}
	/*
	 * XXX at some point, add support for divert/forward actions.
	 * If none of the above matches, we have to drop the packet.
	 */
	bdg_ipfw_drops++ ;
	return m0 ;
    }
forward:
    /*
     * Again, bring up the headers in case of shared bufs to avoid
     * corruptions in the future.
     */
    if ( shared ) {
	int i = min(m0->m_pkthdr.len, max_protohdr) ;

	m0 = m_pullup(m0, i) ;
	if (m0 == NULL)
	    return NULL ;
    }
    /*
     * now real_dst is used to determine the cluster where to forward.
     * For packets coming from ether_input, this is the one of the 'src'
     * interface, whereas for locally generated packets (src==NULL) it
     * is the cluster of the original destination interface, which
     * was already saved into real_dst.
     */
    if (src != NULL)
	real_dst = src ;

    last = NULL;
    for (;;) {
	if (last) { /* need to forward packet leftover from previous loop */
	    struct mbuf *m ;
	    if (shared == 0 && once ) { /* no need to copy */
		m = m0 ;
		m0 = NULL ; /* original is gone */
	    } else {
		m = m_copypacket(m0, M_DONTWAIT);
		if (m == NULL) {
		    printf("bdg_forward: sorry, m_copypacket failed!\n");
		    return m0 ; /* the original is still there... */
		}
	    }
	    /*
	     * Add header (optimized for the common case of eh pointing
	     * already into the mbuf) and execute last part of ether_output:
	     * queue pkt and start output if interface not yet active.
	     */
	    if ( (void *)(eh + 1) == (void *)m->m_data) {
		m->m_data -= ETHER_HDR_LEN ;
		m->m_len += ETHER_HDR_LEN ;
		m->m_pkthdr.len += ETHER_HDR_LEN ;
		bdg_predict++;
	    } else {
		M_PREPEND(m, ETHER_HDR_LEN, M_DONTWAIT);
		if (!m && verbose)
		    printf("M_PREPEND failed\n");
		if (m == NULL)
		    return m0;
		bcopy(&save_eh, mtod(m, struct ether_header *), ETHER_HDR_LEN);
	    }
	    if (!IF_HANDOFF(&last->if_snd, m, last)) {
#if 0
		BDG_MUTE(last); /* should I also mute ? */
#endif
	    }
	    BDG_STAT(last, BDG_OUT);
	    last = NULL ;
	    if (once)
		break ;
	}
	if (ifp == NULL)
	    break ;
	/*
	 * If the interface is used for bridging, not muted, not full,
	 * up and running, is not the source interface, and belongs to
	 * the same cluster as the 'real_dst', then send here.
	 */
	if ( BDG_USED(ifp) && !BDG_MUTED(ifp) && !_IF_QFULL(&ifp->if_snd)  &&
	     (ifp->if_flags & (IFF_UP|IFF_RUNNING)) == (IFF_UP|IFF_RUNNING) &&
	     ifp != src && BDG_SAMECLUSTER(ifp, real_dst) )
	    last = ifp ;
	ifp = TAILQ_NEXT(ifp, if_link) ;
	if (ifp == NULL)
	    once = 1 ;
    }
    DEB(bdg_fw_ticks += (u_long)(rdtsc() - ticks) ; bdg_fw_count++ ;
	if (bdg_fw_count != 0) bdg_fw_avg = bdg_fw_ticks/bdg_fw_count; )
    return m0 ;
}

/*
 * initialization of bridge code.
 */
static int
bdginit(void)
{
    printf("BRIDGE 020214 loaded\n");

    ifp2sc = malloc(BDG_MAX_PORTS * sizeof(struct bdg_softc),
		M_IFADDR, M_WAITOK | M_ZERO );
    if (ifp2sc == NULL)
	return ENOMEM ;

    bridge_in_ptr = bridge_in;
    bdg_forward_ptr = bdg_forward;
    bdgtakeifaces_ptr = reconfigure_bridge;

    n_clusters = 0;
    clusters = NULL;
    do_bridge=0;

    bzero(&bdg_stats, sizeof(bdg_stats) );
    bdgtakeifaces_ptr();
    bdg_timeout(0);
    return 0 ;
}

/*
 * initialization code, both for static and dynamic loading.
 */
static int
bridge_modevent(module_t mod, int type, void *unused)
{
	int s;
	int err = 0 ;

	switch (type) {
	case MOD_LOAD:
		if (BDG_LOADED) {
			err = EEXIST;
			break ;
		}
		s = splimp();
		err = bdginit();
		splx(s);
		break;
	case MOD_UNLOAD:
#if !defined(KLD_MODULE)
		printf("bridge statically compiled, cannot unload\n");
		err = EINVAL ;
#else
		s = splimp();
		do_bridge = 0;
		bridge_in_ptr = NULL;
		bdg_forward_ptr = NULL;
		bdgtakeifaces_ptr = NULL;
		untimeout(bdg_timeout, NULL, bdg_timeout_h);
		bridge_off();
		if (clusters)
		    free(clusters, M_IFADDR);
		free(ifp2sc, M_IFADDR);
		ifp2sc = NULL ;
		splx(s);
#endif
		break;
	default:
		err = EINVAL ;
		break;
	}
	return err;
}

static moduledata_t bridge_mod = {
	"bridge",
	bridge_modevent,
	0
};

DECLARE_MODULE(bridge, bridge_mod, SI_SUB_PSEUDO, SI_ORDER_ANY);
MODULE_VERSION(bridge, 1);
