/*-
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
 *	net.link.ether.bridge.enable
 * the grouping of interfaces into clusters is done with
 *	net.link.ether.bridge.config
 * containing a list of interfaces each optionally followed by
 * a colon and the cluster it belongs to (1 is the default).
 * Separators can be spaces, commas or tabs, e.g.
 *	net.link.ether.bridge.config="fxp0:2 fxp1:2 dc0 dc1:1"
 * Optionally bridged packets can be passed through the firewall,
 * this is controlled by the variable
 *	net.link.ether.bridge.ipfw
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
#include <sys/module.h>
#include <sys/sysctl.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>		/* for struct arpcom */
#include <net/if_types.h>
#include <net/if_var.h>
#include <net/pfil.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>

#include <net/route.h>
#include <netinet/ip_fw.h>
#include <netinet/ip_dummynet.h>
#include <net/bridge.h>

/*--------------------*/

#define	ETHER_ADDR_COPY(_dst,_src)	bcopy(_src, _dst, ETHER_ADDR_LEN)
#define	ETHER_ADDR_EQ(_a1,_a2)		(bcmp(_a1, _a2, ETHER_ADDR_LEN) == 0)

/*
 * For each cluster, source MAC addresses are stored into a hash
 * table which locates the port they reside on.
 */
#define HASH_SIZE 8192	/* Table size, must be a power of 2 */

typedef struct hash_table {		/* each entry.		*/
    struct ifnet *	name;
    u_char		etheraddr[ETHER_ADDR_LEN];
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
    u_char	etheraddr[ETHER_ADDR_LEN];
    u_int16_t	_padding;
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

static int n_clusters;				/* number of clusters */
static struct cluster_softc *clusters;

static struct mtx bdg_mtx;
#define	BDG_LOCK_INIT()		mtx_init(&bdg_mtx, "bridge", NULL, MTX_DEF)
#define	BDG_LOCK_DESTROY()	mtx_destroy(&bdg_mtx)
#define	BDG_LOCK()		mtx_lock(&bdg_mtx)
#define	BDG_UNLOCK()		mtx_unlock(&bdg_mtx)
#define	BDG_LOCK_ASSERT()	mtx_assert(&bdg_mtx, MA_OWNED)

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
#define	BDG_MATCH(a,b)		ETHER_ADDR_EQ(a,b)
#define	IS_ETHER_BROADCAST(a)	ETHER_ADDR_EQ(a,"\377\377\377\377\377\377")
#endif

SYSCTL_DECL(_net_link_ether);
SYSCTL_NODE(_net_link_ether, OID_AUTO, bridge, CTLFLAG_RD, 0,
	"Bridge parameters");
static char bridge_version[] = "031224";
SYSCTL_STRING(_net_link_ether_bridge, OID_AUTO, version, CTLFLAG_RD,
	bridge_version, 0, "software version");

#define BRIDGE_DEBUG
#ifdef BRIDGE_DEBUG
int	bridge_debug = 0;
SYSCTL_INT(_net_link_ether_bridge, OID_AUTO, debug, CTLFLAG_RW, &bridge_debug,
	    0, "control debugging printfs");
#define	DPRINTF(X)	if (bridge_debug) printf X
#else
#define	DPRINTF(X)
#endif

#ifdef BRIDGE_TIMING
/*
 * For timing-related debugging, you can use the following macros.
 * remember, rdtsc() only works on Pentium-class machines

    quad_t ticks;
    DDB(ticks = rdtsc();)
    ... interesting code ...
    DDB(bdg_fw_ticks += (u_long)(rdtsc() - ticks) ; bdg_fw_count++ ;)

 *
 */
#define DDB(x)	x

static int bdg_fw_avg;
SYSCTL_INT(_net_link_ether_bridge, OID_AUTO, fw_avg, CTLFLAG_RW,
	    &bdg_fw_avg, 0,"Cycle counter avg");
static int bdg_fw_ticks;
SYSCTL_INT(_net_link_ether_bridge, OID_AUTO, fw_ticks, CTLFLAG_RW,
	    &bdg_fw_ticks, 0,"Cycle counter item");
static int bdg_fw_count;
SYSCTL_INT(_net_link_ether_bridge, OID_AUTO, fw_count, CTLFLAG_RW,
	    &bdg_fw_count, 0,"Cycle counter count");
#else
#define	DDB(x)
#endif

static int bdginit(void);
static void parse_bdg_cfg(void);
static struct mbuf *bdg_forward(struct mbuf *, struct ifnet *);

static int bdg_ipf;		/* IPFilter enabled in bridge */
SYSCTL_INT(_net_link_ether_bridge, OID_AUTO, ipf, CTLFLAG_RW,
	    &bdg_ipf, 0,"Pass bridged pkts through IPFilter");
static int bdg_ipfw;
SYSCTL_INT(_net_link_ether_bridge, OID_AUTO, ipfw, CTLFLAG_RW,
	    &bdg_ipfw,0,"Pass bridged pkts through firewall");

static int bdg_copy;
SYSCTL_INT(_net_link_ether_bridge, OID_AUTO, copy, CTLFLAG_RW,
	&bdg_copy, 0, "Force packet copy in bdg_forward");

int bdg_ipfw_drops;
SYSCTL_INT(_net_link_ether_bridge, OID_AUTO, ipfw_drop,
	CTLFLAG_RW, &bdg_ipfw_drops,0,"");
int bdg_ipfw_colls;
SYSCTL_INT(_net_link_ether_bridge, OID_AUTO, ipfw_collisions,
	CTLFLAG_RW, &bdg_ipfw_colls,0,"");

static int bdg_thru;
SYSCTL_INT(_net_link_ether_bridge, OID_AUTO, packets, CTLFLAG_RW,
	&bdg_thru, 0, "Packets through bridge");
static int bdg_dropped;
SYSCTL_INT(_net_link_ether_bridge, OID_AUTO, dropped, CTLFLAG_RW,
	&bdg_dropped, 0, "Packets dropped in bdg_forward");
static int bdg_predict;
SYSCTL_INT(_net_link_ether_bridge, OID_AUTO, predict, CTLFLAG_RW,
	&bdg_predict, 0, "Correctly predicted header location");

#ifdef BRIDGE_DEBUG
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
#endif /* BRIDGE_DEBUG */

/*
 * System initialization
 */
static struct bdg_stats bdg_stats ;
SYSCTL_STRUCT(_net_link_ether_bridge, OID_AUTO, stats, CTLFLAG_RD,
	&bdg_stats, bdg_stats, "bridge statistics");

static struct callout bdg_callout;

/*
 * Add an interface to a cluster, possibly creating a new entry in
 * the cluster table. This requires reallocation of the table and
 * updating pointers in ifp2sc.
 */
static struct cluster_softc *
add_cluster(u_int16_t cluster_id, struct ifnet *ifp)
{
    struct cluster_softc *c = NULL;
    int i;

    BDG_LOCK_ASSERT();

    for (i = 0; i < n_clusters ; i++)
	if (clusters[i].cluster_id == cluster_id)
	    goto found;

    /* Not found, need to reallocate */
    c = malloc((1+n_clusters) * sizeof (*c), M_IFADDR, M_NOWAIT | M_ZERO);
    if (c == NULL) {/* malloc failure */
	printf("-- bridge: cannot add new cluster\n");
	goto bad;
    }
    c[n_clusters].ht = (struct hash_table *)
	    malloc(HASH_SIZE * sizeof(struct hash_table),
		M_IFADDR, M_NOWAIT | M_ZERO);
    if (c[n_clusters].ht == NULL) {
	printf("-- bridge: cannot allocate hash table for new cluster\n");
	goto bad;
    }
    c[n_clusters].my_macs = (struct bdg_addr *)
	    malloc(BDG_MAX_PORTS * sizeof(struct bdg_addr),
		M_IFADDR, M_NOWAIT | M_ZERO);
    if (c[n_clusters].my_macs == NULL) {
        printf("-- bridge: cannot allocate mac addr table for new cluster\n");
	free(c[n_clusters].ht, M_IFADDR);
	goto bad;
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
    ETHER_ADDR_COPY(c->my_macs[c->ports].etheraddr, IFP2AC(ifp)->ac_enaddr);
    c->ports++;
    return c;
bad:
    if (c)
	free(c, M_IFADDR);
    return NULL;
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
    int i;

    BDG_LOCK_ASSERT();

    DPRINTF(("%s: n_clusters %d\n", __func__, n_clusters));

    IFNET_RLOCK();
    TAILQ_FOREACH(ifp, &ifnet, if_link) {
	struct bdg_softc *b;

	if (ifp->if_index >= BDG_MAX_PORTS)
	    continue;	/* make sure we do not go beyond the end */
	b = &ifp2sc[ifp->if_index];

	if ( b->flags & IFF_BDG_PROMISC ) {
	    ifpromisc(ifp, 0);
	    b->flags &= ~(IFF_BDG_PROMISC|IFF_MUTE) ;
	    DPRINTF(("%s: %s promisc OFF if_flags 0x%x "
		"bdg_flags 0x%x\n", __func__, ifp->if_xname,
		ifp->if_flags, b->flags));
	}
	b->flags &= ~(IFF_USED) ;
	b->cluster = NULL;
	bdg_stats.s[ifp->if_index].name[0] = '\0';
    }
    IFNET_RUNLOCK();
    /* flush_tables */

    for (i=0; i < n_clusters; i++) {
	free(clusters[i].ht, M_IFADDR);
	free(clusters[i].my_macs, M_IFADDR);
    }
    if (clusters != NULL)
	free(clusters, M_IFADDR);
    clusters = NULL;
    n_clusters =0;
}

/*
 * set promisc mode on the interfaces we use.
 */
static void
bridge_on(void)
{
    struct ifnet *ifp ;

    BDG_LOCK_ASSERT();

    IFNET_RLOCK();
    TAILQ_FOREACH(ifp, &ifnet, if_link) {
	struct bdg_softc *b = &ifp2sc[ifp->if_index];

	if ( !(b->flags & IFF_USED) )
	    continue ;
	if ( !( ifp->if_flags & IFF_UP) ) {
	    if_up(ifp);
	}
	if ( !(b->flags & IFF_BDG_PROMISC) ) {
	    (void) ifpromisc(ifp, 1);
	    b->flags |= IFF_BDG_PROMISC ;
	    DPRINTF(("%s: %s promisc ON if_flags 0x%x bdg_flags 0x%x\n",
		__func__, ifp->if_xname, ifp->if_flags, b->flags));
	}
	if (b->flags & IFF_MUTE) {
	    DPRINTF(("%s: unmuting %s\n", __func__, ifp->if_xname));
	    b->flags &= ~IFF_MUTE;
	}
    }
    IFNET_RUNLOCK();
}

static char bridge_cfg[1024];		/* NB: in BSS so initialized to zero */

/**
 * reconfigure bridge.
 * This is also done every time we attach or detach an interface.
 * Main use is to make sure that we do not bridge on some old
 * (ejected) device. So, it would be really useful to have a
 * pointer to the modified device as an argument. Without it, we
 * have to scan all interfaces.
 */
static void
reconfigure_bridge_locked(void)
{
    BDG_LOCK_ASSERT();

    bridge_off();
    if (do_bridge) {
	if (if_index >= BDG_MAX_PORTS) {
	    printf("-- sorry too many interfaces (%d, max is %d),"
		" disabling bridging\n", if_index, BDG_MAX_PORTS);
	    do_bridge = 0;
	    return;
	}
	parse_bdg_cfg();
	bridge_on();
    }
}

static void
reconfigure_bridge(void)
{
    BDG_LOCK();
    reconfigure_bridge_locked();
    BDG_UNLOCK();
}

/*
 * parse the config string, set IFF_USED, name and cluster_id
 * for all interfaces found.
 * The config string is a list of "if[:cluster]" with
 * a number of possible separators (see "sep"). In particular the
 * use of the space lets you set bridge_cfg with the output from
 * "ifconfig -l"
 */
static void
parse_bdg_cfg(void)
{
    char *p, *beg;
    int l, cluster;
    static const char *sep = ", \t";

    BDG_LOCK_ASSERT();

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
	IFNET_RLOCK();		/* could sleep XXX */
	TAILQ_FOREACH(ifp, &ifnet, if_link) {

	    if (!strncmp(beg, ifp->if_xname, max(l, strlen(ifp->if_xname)))) {
		struct bdg_softc *b = &ifp2sc[ifp->if_index];
		if (ifp->if_type != IFT_ETHER && ifp->if_type != IFT_L2VLAN) {
		    printf("%s is not an ethernet, continue\n", ifp->if_xname);
		    continue;
		}
		if (b->flags & IFF_USED) {
		    printf("%s already used, skipping\n", ifp->if_xname);
		    break;
		}
		b->cluster = add_cluster(htons(cluster), ifp);
		b->flags |= IFF_USED ;
		snprintf(bdg_stats.s[ifp->if_index].name,
		    sizeof(bdg_stats.s[ifp->if_index].name),
		    "%s:%d", ifp->if_xname, cluster);

		DPRINTF(("%s: found %s next c %d\n", __func__,
		    bdg_stats.s[ifp->if_index].name, c));
		found = 1;
		break ;
	    }
	}
	IFNET_RUNLOCK();
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
    int enable = do_bridge;
    int error;

    error = sysctl_handle_int(oidp, &enable, 0, req);
    enable = (enable) ? 1 : 0;
    BDG_LOCK();
    if (enable != do_bridge) {
	do_bridge = enable;
	reconfigure_bridge_locked();
    }
    BDG_UNLOCK();
    return error ;
}
SYSCTL_PROC(_net_link_ether_bridge, OID_AUTO, enable, CTLTYPE_INT|CTLFLAG_RW,
	    &do_bridge, 0, &sysctl_bdg, "I", "Bridging");

/*
 * handler for net.link.ether.bridge_cfg
 */
static int
sysctl_bdg_cfg(SYSCTL_HANDLER_ARGS)
{
    int error;
    char *new_cfg;

    new_cfg = malloc(sizeof(bridge_cfg), M_TEMP, M_WAITOK);
    bcopy(bridge_cfg, new_cfg, sizeof(bridge_cfg));

    error = sysctl_handle_string(oidp, new_cfg, oidp->oid_arg2, req);
    if (error == 0) {
        BDG_LOCK();
	if (strcmp(new_cfg, bridge_cfg)) {
	    bcopy(new_cfg, bridge_cfg, sizeof(bridge_cfg));
	    reconfigure_bridge_locked();
	}
	BDG_UNLOCK();
    }

    free(new_cfg, M_TEMP);

    return error;
}
SYSCTL_PROC(_net_link_ether_bridge, OID_AUTO, config, CTLTYPE_STRING|CTLFLAG_RW,
	    &bridge_cfg, sizeof(bridge_cfg), &sysctl_bdg_cfg, "A",
	    "Bridge configuration");

static int
sysctl_refresh(SYSCTL_HANDLER_ARGS)
{
    if (req->newptr)
	reconfigure_bridge();

    return 0;
}
SYSCTL_PROC(_net_link_ether_bridge, OID_AUTO, refresh, CTLTYPE_INT|CTLFLAG_WR,
	    NULL, 0, &sysctl_refresh, "I", "iface refresh");

#ifndef BURN_BRIDGES
#define SYSCTL_OID_COMPAT(parent, nbr, name, kind, a1, a2, handler, fmt, descr)\
	static struct sysctl_oid sysctl__##parent##_##name##_compat = {	 \
		&sysctl_##parent##_children, { 0 },			 \
		nbr, kind, a1, a2, #name, handler, fmt, 0, descr };	 \
	DATA_SET(sysctl_set, sysctl__##parent##_##name##_compat)
#define SYSCTL_INT_COMPAT(parent, nbr, name, access, ptr, val, descr)	 \
	SYSCTL_OID_COMPAT(parent, nbr, name, CTLTYPE_INT|(access),	 \
		ptr, val, sysctl_handle_int, "I", descr)
#define SYSCTL_STRUCT_COMPAT(parent, nbr, name, access, ptr, type, descr)\
	SYSCTL_OID_COMPAT(parent, nbr, name, CTLTYPE_OPAQUE|(access),	 \
		ptr, sizeof(struct type), sysctl_handle_opaque,		 \
		"S," #type, descr)
#define SYSCTL_PROC_COMPAT(parent, nbr, name, access, ptr, arg, handler, fmt, descr) \
	SYSCTL_OID_COMPAT(parent, nbr, name, (access),			 \
		ptr, arg, handler, fmt, descr)

SYSCTL_INT_COMPAT(_net_link_ether, OID_AUTO, bridge_ipf, CTLFLAG_RW,
	    &bdg_ipf, 0,"Pass bridged pkts through IPFilter");
SYSCTL_INT_COMPAT(_net_link_ether, OID_AUTO, bridge_ipfw, CTLFLAG_RW,
	    &bdg_ipfw,0,"Pass bridged pkts through firewall");
SYSCTL_STRUCT_COMPAT(_net_link_ether, PF_BDG, bdgstats, CTLFLAG_RD,
	&bdg_stats, bdg_stats, "bridge statistics");
SYSCTL_PROC_COMPAT(_net_link_ether, OID_AUTO, bridge_cfg, 
	    CTLTYPE_STRING|CTLFLAG_RW,
	    &bridge_cfg, sizeof(bridge_cfg), &sysctl_bdg_cfg, "A",
	    "Bridge configuration");
SYSCTL_PROC_COMPAT(_net_link_ether, OID_AUTO, bridge_refresh,
	    CTLTYPE_INT|CTLFLAG_WR,
	    NULL, 0, &sysctl_refresh, "I", "iface refresh");
#endif

static int bdg_loops;
static int bdg_slowtimer = 0;
static int bdg_age_index = 0;	/* index of table position to age */

/*
 * called periodically to flush entries etc.
 */
static void
bdg_timeout(void *dummy)
{
    if (do_bridge) {
	int l, i;

	BDG_LOCK();
	/*
	 * age entries in the forwarding table.
	 */
	l = bdg_age_index + HASH_SIZE/4 ;
	if (l > HASH_SIZE)
	    l = HASH_SIZE;

	for (i = 0; i < n_clusters; i++) {
	    bdg_hash_table *bdg_table = clusters[i].ht;
	    for (; bdg_age_index < l; bdg_age_index++)
		if (bdg_table[bdg_age_index].used)
		    bdg_table[bdg_age_index].used = 0;
		else if (bdg_table[bdg_age_index].name) {
		    DPRINTF(("%s: flushing stale entry %d\n",
			__func__, bdg_age_index));
		    bdg_table[bdg_age_index].name = NULL;
		}
	}
	if (bdg_age_index >= HASH_SIZE)
	    bdg_age_index = 0;

	if (--bdg_slowtimer <= 0 ) {
	    bdg_slowtimer = 5;

	    bridge_on();	/* we just need unmute, really */
	    bdg_loops = 0;
	}
	BDG_UNLOCK();
    }
    callout_reset(&bdg_callout, 2*hz, bdg_timeout, NULL);
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
static __inline struct ifnet *
bridge_dst_lookup(struct ether_header *eh, struct cluster_softc *c)
{
    bdg_hash_table *bt;		/* pointer to entry in hash table */

    BDG_LOCK_ASSERT();

    if (ETHER_IS_MULTICAST(eh->ether_dhost))
	return IS_ETHER_BROADCAST(eh->ether_dhost) ? BDG_BCAST : BDG_MCAST;
    /*
     * Lookup local addresses in case one matches.  We optimize
     * for the common case of two interfaces.
     */
    KASSERT(c->ports != 0, ("lookup with no ports!"));
    switch (c->ports) {
	int i;
    default:
	for (i = c->ports-1; i > 1; i--) {
	    if (ETHER_ADDR_EQ(c->my_macs[i].etheraddr, eh->ether_dhost))
	        return BDG_LOCAL;
	}
	/* fall thru... */
    case 2:
	if (ETHER_ADDR_EQ(c->my_macs[1].etheraddr, eh->ether_dhost))
	    return BDG_LOCAL;
    case 1:
	if (ETHER_ADDR_EQ(c->my_macs[0].etheraddr, eh->ether_dhost))
	    return BDG_LOCAL;
    }
    /*
     * Look for a possible destination in table
     */
    bt = &c->ht[HASH_FN(eh->ether_dhost)];
    if (bt->name && ETHER_ADDR_EQ(bt->etheraddr, eh->ether_dhost))
	return bt->name;
    else
	return BDG_UNKNOWN;
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

static struct mbuf *
bridge_in(struct ifnet *ifp, struct mbuf *m)
{
    struct ether_header *eh;
    struct ifnet *dst, *old;
    bdg_hash_table *bt;			/* location in hash table */
    int dropit = BDG_MUTED(ifp);
    int index;

    eh = mtod(m, struct ether_header *);

    /*
     * hash the source address
     */
    BDG_LOCK();
    index = HASH_FN(eh->ether_shost);
    bt = &BDG_CLUSTER(ifp)->ht[index];
    bt->used = 1;
    old = bt->name;
    if (old) {				/* the entry is valid */
	if (!ETHER_ADDR_EQ(eh->ether_shost, bt->etheraddr)) {
	    bdg_ipfw_colls++;
	    bt->name = NULL;		/* NB: will overwrite below */
	} else if (old != ifp) {
	    /*
	     * Found a loop. Either a machine has moved, or there
	     * is a misconfiguration/reconfiguration of the network.
	     * First, do not forward this packet!
	     * Record the relocation anyways; then, if loops persist,
	     * suspect a reconfiguration and disable forwarding
	     * from the old interface.
	     */
	    bt->name = ifp;		/* relocate address */
	    printf("-- loop (%d) %6D to %s from %s (%s)\n",
			bdg_loops, eh->ether_shost, ".",
			ifp->if_xname, old->if_xname,
			BDG_MUTED(old) ? "muted":"active");
	    dropit = 1;
	    if (!BDG_MUTED(old)) {
		if (bdg_loops++ > 10)
		    BDG_MUTE(old);
	    }
	}
    }

    /*
     * now write the source address into the table
     */
    if (bt->name == NULL) {
	DPRINTF(("%s: new addr %6D at %d for %s\n",
	    __func__, eh->ether_shost, ".", index, ifp->if_xname));
	ETHER_ADDR_COPY(bt->etheraddr, eh->ether_shost);
	bt->name = ifp;
    }
    dst = bridge_dst_lookup(eh, BDG_CLUSTER(ifp));
    BDG_UNLOCK();

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
	break;
    default:
	if (dst == ifp || dropit)
	    BDG_STAT(ifp, BDG_DROP);
	else
	    BDG_STAT(ifp, BDG_FORWARD);
	break;
    }

    if (dropit) {
	if (dst == BDG_BCAST || dst == BDG_MCAST || dst == BDG_LOCAL)
	    dst = BDG_LOCAL;
	else
	    dst = BDG_DROP;
    } else {
	if (dst == ifp)
	    dst = BDG_DROP;
    }
    DPRINTF(("%s: %6D ->%6D ty 0x%04x dst %s\n", __func__,
	eh->ether_shost, ".",
	eh->ether_dhost, ".",
	ntohs(eh->ether_type),
	(dst <= BDG_FORWARD) ? bdg_dst_names[(uintptr_t)dst] :
		dst->if_xname));

    switch ((uintptr_t)dst) {
    case (uintptr_t)BDG_DROP:
	m_freem(m);
	return (NULL);

    case (uintptr_t)BDG_LOCAL:
	return (m);

    case (uintptr_t)BDG_BCAST:
    case (uintptr_t)BDG_MCAST:
        m = bdg_forward(m, dst);
#ifdef	DIAGNOSTIC
	if (m == NULL)
		if_printf(ifp, "bridge dropped %s packet\n",
		     dst == BDG_BCAST ? "broadcast" : "multicast");
#endif
	return (m);

    default:
        m = bdg_forward(m, dst);
	/*
	 * But in some cases the bridge may return the
	 * packet for us to free; sigh.
	 */
	if (m != NULL)
		m_freem(m);

    }

    return (NULL);
}

/*
 * Return 1 if it's ok to send a packet out the specified interface.
 * The interface must be:
 *	used for bridging,
 *	not muted,
 *	not full,
 *	up and running,
 *	not the source interface, and
 *	belong to the same cluster as the 'real_dst'.
 */
static __inline int
bridge_ifok(struct ifnet *ifp, struct ifnet *src, struct ifnet *dst)
{
    return (BDG_USED(ifp)
	&& !BDG_MUTED(ifp)
	&& !_IF_QFULL(&ifp->if_snd)
	&& (ifp->if_flags & (IFF_UP|IFF_RUNNING)) == (IFF_UP|IFF_RUNNING)
	&& ifp != src
	&& BDG_SAMECLUSTER(ifp, dst));
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
 */
static struct mbuf *
bdg_forward(struct mbuf *m0, struct ifnet *dst)
{
#define	EH_RESTORE(_m) do {						   \
    M_PREPEND((_m), ETHER_HDR_LEN, M_DONTWAIT);			   	   \
    if ((_m) == NULL) {							   \
	bdg_dropped++;							   \
	return NULL;							   \
    }									   \
    if (eh != mtod((_m), struct ether_header *))			   \
	bcopy(&save_eh, mtod((_m), struct ether_header *), ETHER_HDR_LEN); \
    else								   \
	bdg_predict++;							   \
} while (0);
    struct ether_header *eh;
    struct ifnet *src;
    struct ifnet *ifp, *last;
    int shared = bdg_copy;		/* someone else is using the mbuf */
    int error;
    struct ifnet *real_dst = dst;	/* real dst from ether_output */
    struct ip_fw_args args;
    struct ether_header save_eh;
    struct mbuf *m;

    DDB(quad_t ticks; ticks = rdtsc();)

    args.rule = ip_dn_claim_rule(m0);
    if (args.rule)
	shared = 0;			/* For sure this is our own mbuf. */
    else
	bdg_thru++;			/* count 1st time through bdg_forward */

    /*
     * The packet arrives with the Ethernet header at the front.
     */
    eh = mtod(m0, struct ether_header *);

    src = m0->m_pkthdr.rcvif;
    if (src == NULL) {			/* packet from ether_output */
	BDG_LOCK();
	dst = bridge_dst_lookup(eh, BDG_CLUSTER(real_dst));
	BDG_UNLOCK();
    }

    if (dst == BDG_DROP) {		/* this should not happen */
	printf("xx bdg_forward for BDG_DROP\n");
	m_freem(m0);
	bdg_dropped++;
	return NULL;
    }
    if (dst == BDG_LOCAL) {		/* this should not happen as well */
	printf("xx ouch, bdg_forward for local pkt\n");
	return m0;
    }
    if (dst == BDG_BCAST || dst == BDG_MCAST) {
	 /* need a copy for the local stack */
	 shared = 1;
    }

    /*
     * Do filtering in a very similar way to what is done in ip_output.
     * Only if firewall is loaded, enabled, and the packet is not
     * from ether_output() (src==NULL, or we would filter it twice).
     * Additional restrictions may apply e.g. non-IP, short packets,
     * and pkts already gone through a pipe.
     */
    if (src != NULL && (
	(inet_pfil_hook.ph_busy_count >= 0 && bdg_ipf != 0) ||
	(IPFW_LOADED && bdg_ipfw != 0))) {

	int i;

	if (args.rule != NULL && fw_one_pass)
	    goto forward; /* packet already partially processed */
	/*
	 * i need some amt of data to be contiguous, and in case others need
	 * the packet (shared==1) also better be in the first mbuf.
	 */
	i = min(m0->m_pkthdr.len, max_protohdr) ;
	if (shared || m0->m_len < i) {
	    m0 = m_pullup(m0, i);
	    if (m0 == NULL) {
		printf("%s: m_pullup failed\n", __func__);	/* XXXDPRINTF*/
		bdg_dropped++;
		return NULL;
	    }
	    eh = mtod(m0, struct ether_header *);
	}

	/*
	 * Processing below expects the Ethernet header is stripped.
	 * Furthermore, the mbuf chain might be replaced at various
	 * places.  To deal with this we copy the header to a temporary
	 * location, strip the header, and restore it as needed.
	 */
	bcopy(eh, &save_eh, ETHER_HDR_LEN);	/* local copy for restore */
	m_adj(m0, ETHER_HDR_LEN);		/* temporarily strip header */

	/*
	 * NetBSD-style generic packet filter, pfil(9), hooks.
	 * Enables ipf(8) in bridging.
	 */
	if (!IPFW_LOADED) { /* XXX: Prevent ipfw from being run twice. */
	if (inet_pfil_hook.ph_busy_count >= 0 &&
	    m0->m_pkthdr.len >= sizeof(struct ip) &&
	    ntohs(save_eh.ether_type) == ETHERTYPE_IP) {
	    /*
	     * before calling the firewall, swap fields the same as IP does.
	     * here we assume the pkt is an IP one and the header is contiguous
	     */
	    struct ip *ip = mtod(m0, struct ip *);

	    ip->ip_len = ntohs(ip->ip_len);
	    ip->ip_off = ntohs(ip->ip_off);

	    if (pfil_run_hooks(&inet_pfil_hook, &m0, src, PFIL_IN, NULL) != 0) {
		/* NB: hook should consume packet */
		return NULL;
	    }
	    if (m0 == NULL)			/* consumed by filter */
		return m0;
	    /*
	     * If we get here, the firewall has passed the pkt, but the mbuf
	     * pointer might have changed. Restore ip and the fields ntohs()'d.
	     */
	    ip = mtod(m0, struct ip *);
	    ip->ip_len = htons(ip->ip_len);
	    ip->ip_off = htons(ip->ip_off);
	}
	} /* XXX: Prevent ipfw from being run twice. */

	/*
	 * Prepare arguments and call the firewall.
	 */
	if (!IPFW_LOADED || bdg_ipfw == 0) {
	    EH_RESTORE(m0);	/* restore Ethernet header */
	    goto forward;	/* not using ipfw, accept the packet */
	}

	/*
	 * XXX The following code is very similar to the one in
	 * if_ethersubr.c:ether_ipfw_chk()
	 */

	args.m = m0;		/* the packet we are looking at		*/
	args.oif = NULL;	/* this is an input packet		*/
	args.next_hop = NULL;	/* we do not support forward yet	*/
	args.eh = &save_eh;	/* MAC header for bridged/MAC packets	*/
	i = ip_fw_chk_ptr(&args);
	m0 = args.m;		/* in case the firewall used the mbuf	*/

	if (m0 != NULL)
		EH_RESTORE(m0);	/* restore Ethernet header */

	if (i == IP_FW_DENY) /* drop */
	    return m0;

	KASSERT(m0 != NULL, ("bdg_forward: m0 is NULL"));

	if (i == 0) /* a PASS rule.  */
	    goto forward;
	if (DUMMYNET_LOADED && (i == IP_FW_DUMMYNET)) {
	    /*
	     * Pass the pkt to dummynet, which consumes it.
	     * If shared, make a copy and keep the original.
	     */
	    if (shared) {
		m = m_copypacket(m0, M_DONTWAIT);
		if (m == NULL) {	/* copy failed, give up */
		    bdg_dropped++;
		    return NULL;
		}
	    } else {
		m = m0 ; /* pass the original to dummynet */
		m0 = NULL ; /* and nothing back to the caller */
	    }

	    args.oif = real_dst;
	    ip_dn_io_ptr(m, DN_TO_BDG_FWD, &args);
	    return m0;
	}
	/*
	 * XXX at some point, add support for divert/forward actions.
	 * If none of the above matches, we have to drop the packet.
	 */
	bdg_ipfw_drops++;
	return m0;
    }
forward:
    /*
     * Again, bring up the headers in case of shared bufs to avoid
     * corruptions in the future.
     */
    if (shared) {
	int i = min(m0->m_pkthdr.len, max_protohdr);

	m0 = m_pullup(m0, i);
	if (m0 == NULL) {
	    bdg_dropped++;
	    return NULL;
	}
	/* NB: eh is not used below; no need to recalculate it */
    }

    /*
     * now real_dst is used to determine the cluster where to forward.
     * For packets coming from ether_input, this is the one of the 'src'
     * interface, whereas for locally generated packets (src==NULL) it
     * is the cluster of the original destination interface, which
     * was already saved into real_dst.
     */
    if (src != NULL)
	real_dst = src;

    last = NULL;
    if (dst == BDG_BCAST || dst == BDG_MCAST || dst == BDG_UNKNOWN) {
	/*
	 * Scan all ports and send copies to all but the last.
	 */
	IFNET_RLOCK();		/* XXX replace with generation # */
	TAILQ_FOREACH(ifp, &ifnet, if_link) {
	    if (bridge_ifok(ifp, src, real_dst)) {
		if (last) {
		    /*
		     * At this point we know two interfaces need a copy
		     * of the packet (last + ifp) so we must create a
		     * copy to handoff to last.
		     */
		    m = m_copypacket(m0, M_DONTWAIT);
		    if (m == NULL) {
			IFNET_RUNLOCK();
			printf("%s: , m_copypacket failed!\n", __func__);
			bdg_dropped++;
			return m0;	/* the original is still there... */
		    }
		    IFQ_HANDOFF(last, m, error);
		    if (!error)
			BDG_STAT(last, BDG_OUT);
		    else
			bdg_dropped++;
		}
		last = ifp;
	    }
	}
	IFNET_RUNLOCK();
    } else {
	if (bridge_ifok(dst, src, real_dst))
	    last = dst;
    }
    if (last) {
	if (shared) {			/* need to copy */
	    m = m_copypacket(m0, M_DONTWAIT);
	    if (m == NULL) {
		printf("%s: sorry, m_copypacket failed!\n", __func__);
		bdg_dropped++ ;
		return m0;		/* the original is still there... */
	    }
	} else {			/* consume original */
	    m = m0, m0 = NULL;
	}
	IFQ_HANDOFF(last, m, error);
	if (!error)
	    BDG_STAT(last, BDG_OUT);
	else
	    bdg_dropped++;
    }

    DDB(bdg_fw_ticks += (u_long)(rdtsc() - ticks) ; bdg_fw_count++ ;
	if (bdg_fw_count != 0) bdg_fw_avg = bdg_fw_ticks/bdg_fw_count; )
    return m0;
#undef EH_RESTORE
}

/*
 * initialization of bridge code.
 */
static int
bdginit(void)
{
    if (bootverbose)
	    printf("BRIDGE %s loaded\n", bridge_version);

    ifp2sc = malloc(BDG_MAX_PORTS * sizeof(struct bdg_softc),
		M_IFADDR, M_WAITOK | M_ZERO );
    if (ifp2sc == NULL)
	return ENOMEM;

    BDG_LOCK_INIT();

    n_clusters = 0;
    clusters = NULL;
    do_bridge = 0;

    bzero(&bdg_stats, sizeof(bdg_stats));

    bridge_in_ptr = bridge_in;
    bdg_forward_ptr = bdg_forward;
    bdgtakeifaces_ptr = reconfigure_bridge;

    bdgtakeifaces_ptr();		/* XXX does this do anything? */

    callout_init(&bdg_callout, NET_CALLOUT_MPSAFE);
    bdg_timeout(0);
    return 0 ;
}

static void
bdgdestroy(void)
{
    bridge_in_ptr = NULL;
    bdg_forward_ptr = NULL;
    bdgtakeifaces_ptr = NULL;

    callout_stop(&bdg_callout);
    BDG_LOCK();
    bridge_off();

    if (ifp2sc) {
	free(ifp2sc, M_IFADDR);
	ifp2sc = NULL;
    }
    BDG_LOCK_DESTROY();
}

/*
 * initialization code, both for static and dynamic loading.
 */
static int
bridge_modevent(module_t mod, int type, void *unused)
{
	int err;

	switch (type) {
	case MOD_LOAD:
		if (BDG_LOADED)
			err = EEXIST;
		else
			err = bdginit();
		break;
	case MOD_UNLOAD:
		do_bridge = 0;
		bdgdestroy();
		err = 0;
		break;
	default:
		err = EINVAL;
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
