/*
 * IP multicast forwarding procedures
 *
 * Written by David Waitzman, BBN Labs, August 1988.
 * Modified by Steve Deering, Stanford, February 1989.
 * Modified by Mark J. Steiglitz, Stanford, May, 1991
 * Modified by Van Jacobson, LBL, January 1993
 * Modified by Ajit Thyagarajan, PARC, August 1993
 * Modified by Bill Fenner, PARC, April 1995
 *
 * MROUTING Revision: 3.5
 * $FreeBSD$
 */

#include "opt_mac.h"
#include "opt_mrouting.h"
#include "opt_random_ip_id.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mac.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sockio.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/igmp.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_encap.h>
#include <netinet/ip_mroute.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <machine/in_cksum.h>

/*
 * Control debugging code for rsvp and multicast routing code.
 * Can only set them with the debugger.
 */
static u_int    rsvpdebug;		/* non-zero enables debugging	*/

static u_int	mrtdebug;		/* any set of the flags below	*/
#define		DEBUG_MFC	0x02
#define		DEBUG_FORWARD	0x04
#define		DEBUG_EXPIRE	0x08
#define		DEBUG_XMIT	0x10

#define M_HASCL(m)	((m)->m_flags & M_EXT)

static MALLOC_DEFINE(M_MRTABLE, "mroutetbl", "multicast routing tables");

static struct mrtstat	mrtstat;
SYSCTL_STRUCT(_net_inet_ip, OID_AUTO, mrtstat, CTLFLAG_RW,
    &mrtstat, mrtstat,
    "Multicast Routing Statistics (struct mrtstat, netinet/ip_mroute.h)");

static struct mfc	*mfctable[MFCTBLSIZ];
static u_char		nexpire[MFCTBLSIZ];
static struct vif	viftable[MAXVIFS];

static struct callout_handle expire_upcalls_ch;

#define		EXPIRE_TIMEOUT	(hz / 4)	/* 4x / second		*/
#define		UPCALL_EXPIRE	6		/* number of timeouts	*/

/*
 * Define the token bucket filter structures
 * tbftable -> each vif has one of these for storing info 
 */

static struct tbf tbftable[MAXVIFS];
#define		TBF_REPROCESS	(hz / 100)	/* 100x / second */

/*
 * 'Interfaces' associated with decapsulator (so we can tell
 * packets that went through it from ones that get reflected
 * by a broken gateway).  These interfaces are never linked into
 * the system ifnet list & no routes point to them.  I.e., packets
 * can't be sent this way.  They only exist as a placeholder for
 * multicast source verification.
 */
static struct ifnet multicast_decap_if[MAXVIFS];

#define ENCAP_TTL 64
#define ENCAP_PROTO IPPROTO_IPIP	/* 4 */

/* prototype IP hdr for encapsulated packets */
static struct ip multicast_encap_iphdr = {
#if BYTE_ORDER == LITTLE_ENDIAN
	sizeof(struct ip) >> 2, IPVERSION,
#else
	IPVERSION, sizeof(struct ip) >> 2,
#endif
	0,				/* tos */
	sizeof(struct ip),		/* total length */
	0,				/* id */
	0,				/* frag offset */
	ENCAP_TTL, ENCAP_PROTO,	
	0,				/* checksum */
};

/*
 * Private variables.
 */
static vifi_t	   numvifs;
static const struct encaptab *encap_cookie;

/*
 * one-back cache used by mroute_encapcheck to locate a tunnel's vif
 * given a datagram's src ip address.
 */
static u_long last_encap_src;
static struct vif *last_encap_vif;

static u_long	X_ip_mcast_src(int vifi);
static int	X_ip_mforward(struct ip *ip, struct ifnet *ifp,
			struct mbuf *m, struct ip_moptions *imo);
static int	X_ip_mrouter_done(void);
static int	X_ip_mrouter_get(struct socket *so, struct sockopt *m);
static int	X_ip_mrouter_set(struct socket *so, struct sockopt *m);
static int	X_legal_vif_num(int vif);
static int	X_mrt_ioctl(int cmd, caddr_t data);

static int get_sg_cnt(struct sioc_sg_req *);
static int get_vif_cnt(struct sioc_vif_req *);
static int ip_mrouter_init(struct socket *, int);
static int add_vif(struct vifctl *);
static int del_vif(vifi_t);
static int add_mfc(struct mfcctl *);
static int del_mfc(struct mfcctl *);
static int socket_send(struct socket *, struct mbuf *, struct sockaddr_in *);
static int set_assert(int);
static void expire_upcalls(void *);
static int ip_mdq(struct mbuf *, struct ifnet *, struct mfc *, vifi_t);
static void phyint_send(struct ip *, struct vif *, struct mbuf *);
static void encap_send(struct ip *, struct vif *, struct mbuf *);
static void tbf_control(struct vif *, struct mbuf *, struct ip *, u_long);
static void tbf_queue(struct vif *, struct mbuf *);
static void tbf_process_q(struct vif *);
static void tbf_reprocess_q(void *);
static int tbf_dq_sel(struct vif *, struct ip *);
static void tbf_send_packet(struct vif *, struct mbuf *);
static void tbf_update_tokens(struct vif *);
static int priority(struct vif *, struct ip *);

/*
 * whether or not special PIM assert processing is enabled.
 */
static int pim_assert;
/*
 * Rate limit for assert notification messages, in usec
 */
#define ASSERT_MSG_TIME		3000000

/*
 * Hash function for a source, group entry
 */
#define MFCHASH(a, g) MFCHASHMOD(((a) >> 20) ^ ((a) >> 10) ^ (a) ^ \
			((g) >> 20) ^ ((g) >> 10) ^ (g))

/*
 * Find a route for a given origin IP address and Multicast group address
 * Type of service parameter to be added in the future!!!
 * Statistics are updated by the caller if needed
 * (mrtstat.mrts_mfc_lookups and mrtstat.mrts_mfc_misses)
 */
static struct mfc *
mfc_find(in_addr_t o, in_addr_t g)
{
    struct mfc *rt;

    for (rt = mfctable[MFCHASH(o,g)]; rt; rt = rt->mfc_next)
	if ((rt->mfc_origin.s_addr == o) &&
		(rt->mfc_mcastgrp.s_addr == g) && (rt->mfc_stall == NULL))
	    break;
    return rt;
}

/*
 * Macros to compute elapsed time efficiently
 * Borrowed from Van Jacobson's scheduling code
 */
#define TV_DELTA(a, b, delta) {					\
	int xxs;						\
	delta = (a).tv_usec - (b).tv_usec;			\
	if ((xxs = (a).tv_sec - (b).tv_sec)) {			\
		switch (xxs) {					\
		case 2:						\
		      delta += 1000000;				\
		      /* FALLTHROUGH */				\
		case 1:						\
		      delta += 1000000;				\
		      break;					\
		default:					\
		      delta += (1000000 * xxs);			\
		}						\
	}							\
}

#define TV_LT(a, b) (((a).tv_usec < (b).tv_usec && \
	      (a).tv_sec <= (b).tv_sec) || (a).tv_sec < (b).tv_sec)

/*
 * Handle MRT setsockopt commands to modify the multicast routing tables.
 */
static int
X_ip_mrouter_set(struct socket *so, struct sockopt *sopt)
{
    int	error, optval;
    vifi_t	vifi;
    struct	vifctl vifc;
    struct	mfcctl mfc;

    if (so != ip_mrouter && sopt->sopt_name != MRT_INIT)
	return EPERM;

    error = 0;
    switch (sopt->sopt_name) {
    case MRT_INIT:
	error = sooptcopyin(sopt, &optval, sizeof optval, sizeof optval);
	if (error)
	    break;
	error = ip_mrouter_init(so, optval);
	break;

    case MRT_DONE:
	error = ip_mrouter_done();
	break;

    case MRT_ADD_VIF:
	error = sooptcopyin(sopt, &vifc, sizeof vifc, sizeof vifc);
	if (error)
	    break;
	error = add_vif(&vifc);
	break;

    case MRT_DEL_VIF:
	error = sooptcopyin(sopt, &vifi, sizeof vifi, sizeof vifi);
	if (error)
	    break;
	error = del_vif(vifi);
	break;

    case MRT_ADD_MFC:
    case MRT_DEL_MFC:
	error = sooptcopyin(sopt, &mfc, sizeof mfc, sizeof mfc);
	if (error)
	    break;
	if (sopt->sopt_name == MRT_ADD_MFC)
	    error = add_mfc(&mfc);
	else
	    error = del_mfc(&mfc);
	break;

    case MRT_ASSERT:
	error = sooptcopyin(sopt, &optval, sizeof optval, sizeof optval);
	if (error)
	    break;
	set_assert(optval);
	break;

    default:
	error = EOPNOTSUPP;
	break;
    }
    return error;
}

/*
 * Handle MRT getsockopt commands
 */
static int
X_ip_mrouter_get(struct socket *so, struct sockopt *sopt)
{
    int error;
    static int version = 0x0305; /* !!! why is this here? XXX */

    switch (sopt->sopt_name) {
    case MRT_VERSION:
	error = sooptcopyout(sopt, &version, sizeof version);
	break;

    case MRT_ASSERT:
	error = sooptcopyout(sopt, &pim_assert, sizeof pim_assert);
	break;

    default:
	error = EOPNOTSUPP;
	break;
    }
    return error;
}

/*
 * Handle ioctl commands to obtain information from the cache
 */
static int
X_mrt_ioctl(int cmd, caddr_t data)
{
    int error = 0;

    switch (cmd) {
    case (SIOCGETVIFCNT):
	error = get_vif_cnt((struct sioc_vif_req *)data);
	break;

    case (SIOCGETSGCNT):
	error = get_sg_cnt((struct sioc_sg_req *)data);
	break;

    default:
	error = EINVAL;
	break;
    }
    return error;
}

/*
 * returns the packet, byte, rpf-failure count for the source group provided
 */
static int
get_sg_cnt(struct sioc_sg_req *req)
{
    int s;
    struct mfc *rt;

    s = splnet();
    rt = mfc_find(req->src.s_addr, req->grp.s_addr);
    splx(s);
    if (rt == NULL) {
	req->pktcnt = req->bytecnt = req->wrong_if = 0xffffffff;
	return EADDRNOTAVAIL;
    }
    req->pktcnt = rt->mfc_pkt_cnt;
    req->bytecnt = rt->mfc_byte_cnt;
    req->wrong_if = rt->mfc_wrong_if;
    return 0;
}

/*
 * returns the input and output packet and byte counts on the vif provided
 */
static int
get_vif_cnt(struct sioc_vif_req *req)
{
    vifi_t vifi = req->vifi;

    if (vifi >= numvifs)
	return EINVAL;

    req->icount = viftable[vifi].v_pkt_in;
    req->ocount = viftable[vifi].v_pkt_out;
    req->ibytes = viftable[vifi].v_bytes_in;
    req->obytes = viftable[vifi].v_bytes_out;

    return 0;
}

/*
 * Enable multicast routing
 */
static int
ip_mrouter_init(struct socket *so, int version)
{
    if (mrtdebug)
	log(LOG_DEBUG, "ip_mrouter_init: so_type = %d, pr_protocol = %d\n",
	    so->so_type, so->so_proto->pr_protocol);

    if (so->so_type != SOCK_RAW || so->so_proto->pr_protocol != IPPROTO_IGMP)
	return EOPNOTSUPP;

    if (version != 1)
	return ENOPROTOOPT;

    if (ip_mrouter != NULL)
	return EADDRINUSE;

    ip_mrouter = so;

    bzero((caddr_t)mfctable, sizeof(mfctable));
    bzero((caddr_t)nexpire, sizeof(nexpire));

    pim_assert = 0;

    expire_upcalls_ch = timeout(expire_upcalls, NULL, EXPIRE_TIMEOUT);

    if (mrtdebug)
	log(LOG_DEBUG, "ip_mrouter_init\n");

    return 0;
}

/*
 * Disable multicast routing
 */
static int
X_ip_mrouter_done(void)
{
    vifi_t vifi;
    int i;
    struct ifnet *ifp;
    struct ifreq ifr;
    struct mfc *rt;
    struct rtdetq *rte;
    int s;

    s = splnet();

    /*
     * For each phyint in use, disable promiscuous reception of all IP
     * multicasts.
     */
    for (vifi = 0; vifi < numvifs; vifi++) {
	if (viftable[vifi].v_lcl_addr.s_addr != 0 &&
		!(viftable[vifi].v_flags & VIFF_TUNNEL)) {
	    struct sockaddr_in *so = (struct sockaddr_in *)&(ifr.ifr_addr);

	    so->sin_len = sizeof(struct sockaddr_in);
	    so->sin_family = AF_INET;
	    so->sin_addr.s_addr = INADDR_ANY;
	    ifp = viftable[vifi].v_ifp;
	    if_allmulti(ifp, 0);
	}
    }
    bzero((caddr_t)tbftable, sizeof(tbftable));
    bzero((caddr_t)viftable, sizeof(viftable));
    numvifs = 0;
    pim_assert = 0;

    untimeout(expire_upcalls, NULL, expire_upcalls_ch);

    /*
     * Free all multicast forwarding cache entries.
     */
    for (i = 0; i < MFCTBLSIZ; i++) {
	for (rt = mfctable[i]; rt != NULL; ) {
	    struct mfc *nr = rt->mfc_next;

	    for (rte = rt->mfc_stall; rte != NULL; ) {
		struct rtdetq *n = rte->next;

		m_freem(rte->m);
		free(rte, M_MRTABLE);
		rte = n;
	    }
	    free(rt, M_MRTABLE);
	    rt = nr;
	}
    }

    bzero((caddr_t)mfctable, sizeof(mfctable));

    /*
     * Reset de-encapsulation cache
     */
    last_encap_src = INADDR_ANY;
    last_encap_vif = NULL;
    if (encap_cookie) {
	encap_detach(encap_cookie);
	encap_cookie = NULL;
    }

    ip_mrouter = NULL;

    splx(s);

    if (mrtdebug)
	log(LOG_DEBUG, "ip_mrouter_done\n");

    return 0;
}

/*
 * Set PIM assert processing global
 */
static int
set_assert(int i)
{
    if ((i != 1) && (i != 0))
	return EINVAL;

    pim_assert = i;

    return 0;
}

/*
 * Decide if a packet is from a tunnelled peer.
 * Return 0 if not, 64 if so.  XXX yuck.. 64 ???
 */
static int
mroute_encapcheck(const struct mbuf *m, int off, int proto, void *arg)
{
    struct ip *ip = mtod(m, struct ip *);
    int hlen = ip->ip_hl << 2;

    /*
     * don't claim the packet if it's not to a multicast destination or if
     * we don't have an encapsulating tunnel with the source.
     * Note:  This code assumes that the remote site IP address
     * uniquely identifies the tunnel (i.e., that this site has
     * at most one tunnel with the remote site).
     */
    if (!IN_MULTICAST(ntohl(((struct ip *)((char *)ip+hlen))->ip_dst.s_addr)))
	return 0;
    if (ip->ip_src.s_addr != last_encap_src) {
	struct vif *vifp = viftable;
	struct vif *vife = vifp + numvifs;

	last_encap_src = ip->ip_src.s_addr;
	last_encap_vif = NULL;
	for ( ; vifp < vife; ++vifp)
	    if (vifp->v_rmt_addr.s_addr == ip->ip_src.s_addr) {
		if ((vifp->v_flags & (VIFF_TUNNEL|VIFF_SRCRT)) == VIFF_TUNNEL)
		    last_encap_vif = vifp;
		break;
	    }
    }
    if (last_encap_vif == NULL) {
	last_encap_src = INADDR_ANY;
	return 0;
    }
    return 64;
}

/*
 * De-encapsulate a packet and feed it back through ip input (this
 * routine is called whenever IP gets a packet that mroute_encap_func()
 * claimed).
 */
static void
mroute_encap_input(struct mbuf *m, int off)
{
    struct ip *ip = mtod(m, struct ip *);
    int hlen = ip->ip_hl << 2;

    if (hlen > sizeof(struct ip))
	ip_stripoptions(m, (struct mbuf *) 0);
    m->m_data += sizeof(struct ip);
    m->m_len -= sizeof(struct ip);
    m->m_pkthdr.len -= sizeof(struct ip);

    m->m_pkthdr.rcvif = last_encap_vif->v_ifp;

    (void) IF_HANDOFF(&ipintrq, m, NULL);
    /*
     * normally we would need a "schednetisr(NETISR_IP)"
     * here but we were called by ip_input and it is going
     * to loop back & try to dequeue the packet we just
     * queued as soon as we return so we avoid the
     * unnecessary software interrrupt.
     */
}

extern struct domain inetdomain;
static struct protosw mroute_encap_protosw =
{ SOCK_RAW,	&inetdomain,	IPPROTO_IPV4,	PR_ATOMIC|PR_ADDR,
  mroute_encap_input,	0,	0,		rip_ctloutput,
  0,
  0,		0,		0,		0,
  &rip_usrreqs
};

/*
 * Add a vif to the vif table
 */
static int
add_vif(struct vifctl *vifcp)
{
    struct vif *vifp = viftable + vifcp->vifc_vifi;
    struct sockaddr_in sin = {sizeof sin, AF_INET};
    struct ifaddr *ifa;
    struct ifnet *ifp;
    int error, s;
    struct tbf *v_tbf = tbftable + vifcp->vifc_vifi;

    if (vifcp->vifc_vifi >= MAXVIFS)
	return EINVAL;
    if (vifp->v_lcl_addr.s_addr != INADDR_ANY)
	return EADDRINUSE;
    if (vifcp->vifc_lcl_addr.s_addr == INADDR_ANY)
	return EADDRNOTAVAIL;

    /* Find the interface with an address in AF_INET family */
    sin.sin_addr = vifcp->vifc_lcl_addr;
    ifa = ifa_ifwithaddr((struct sockaddr *)&sin);
    if (ifa == NULL)
	return EADDRNOTAVAIL;
    ifp = ifa->ifa_ifp;

    if (vifcp->vifc_flags & VIFF_TUNNEL) {
	if ((vifcp->vifc_flags & VIFF_SRCRT) == 0) {
	    /*
	     * An encapsulating tunnel is wanted.  Tell
	     * mroute_encap_input() to start paying attention
	     * to encapsulated packets.
	     */
	    if (encap_cookie == NULL) {
		encap_cookie = encap_attach_func(AF_INET, IPPROTO_IPV4,
				mroute_encapcheck,
				(struct protosw *)&mroute_encap_protosw, NULL);

		if (encap_cookie == NULL) {
		    printf("ip_mroute: unable to attach encap\n");
		    return EIO;	/* XXX */
		}
		for (s = 0; s < MAXVIFS; ++s) {
		    multicast_decap_if[s].if_name = "mdecap";
		    multicast_decap_if[s].if_unit = s;
		}
	    }
	    /*
	     * Set interface to fake encapsulator interface
	     */
	    ifp = &multicast_decap_if[vifcp->vifc_vifi];
	    /*
	     * Prepare cached route entry
	     */
	    bzero(&vifp->v_route, sizeof(vifp->v_route));
	} else {
	    log(LOG_ERR, "source routed tunnels not supported\n");
	    return EOPNOTSUPP;
	}
    } else {		/* Make sure the interface supports multicast */
	if ((ifp->if_flags & IFF_MULTICAST) == 0)
	    return EOPNOTSUPP;

	/* Enable promiscuous reception of all IP multicasts from the if */
	s = splnet();
	error = if_allmulti(ifp, 1);
	splx(s);
	if (error)
	    return error;
    }

    s = splnet();
    /* define parameters for the tbf structure */
    vifp->v_tbf = v_tbf;
    GET_TIME(vifp->v_tbf->tbf_last_pkt_t);
    vifp->v_tbf->tbf_n_tok = 0;
    vifp->v_tbf->tbf_q_len = 0;
    vifp->v_tbf->tbf_max_q_len = MAXQSIZE;
    vifp->v_tbf->tbf_q = vifp->v_tbf->tbf_t = NULL;

    vifp->v_flags     = vifcp->vifc_flags;
    vifp->v_threshold = vifcp->vifc_threshold;
    vifp->v_lcl_addr  = vifcp->vifc_lcl_addr;
    vifp->v_rmt_addr  = vifcp->vifc_rmt_addr;
    vifp->v_ifp       = ifp;
    /* scaling up here allows division by 1024 in critical code */
    vifp->v_rate_limit= vifcp->vifc_rate_limit * 1024 / 1000;
    vifp->v_rsvp_on   = 0;
    vifp->v_rsvpd     = NULL;
    /* initialize per vif pkt counters */
    vifp->v_pkt_in    = 0;
    vifp->v_pkt_out   = 0;
    vifp->v_bytes_in  = 0;
    vifp->v_bytes_out = 0;
    splx(s);

    /* Adjust numvifs up if the vifi is higher than numvifs */
    if (numvifs <= vifcp->vifc_vifi) numvifs = vifcp->vifc_vifi + 1;

    if (mrtdebug)
	log(LOG_DEBUG, "add_vif #%d, lcladdr %lx, %s %lx, thresh %x, rate %d\n",
	    vifcp->vifc_vifi, 
	    (u_long)ntohl(vifcp->vifc_lcl_addr.s_addr),
	    (vifcp->vifc_flags & VIFF_TUNNEL) ? "rmtaddr" : "mask",
	    (u_long)ntohl(vifcp->vifc_rmt_addr.s_addr),
	    vifcp->vifc_threshold,
	    vifcp->vifc_rate_limit);    

    return 0;
}

/*
 * Delete a vif from the vif table
 */
static int
del_vif(vifi_t vifi)
{
    struct vif *vifp;
    int s;

    if (vifi >= numvifs)
	return EINVAL;
    vifp = &viftable[vifi];
    if (vifp->v_lcl_addr.s_addr == INADDR_ANY)
	return EADDRNOTAVAIL;

    s = splnet();

    if (!(vifp->v_flags & VIFF_TUNNEL))
	if_allmulti(vifp->v_ifp, 0);

    if (vifp == last_encap_vif) {
	last_encap_vif = NULL;
	last_encap_src = INADDR_ANY;
    }

    /*
     * Free packets queued at the interface
     */
    while (vifp->v_tbf->tbf_q) {
	struct mbuf *m = vifp->v_tbf->tbf_q;

	vifp->v_tbf->tbf_q = m->m_act;
	m_freem(m);
    }

    bzero((caddr_t)vifp->v_tbf, sizeof(*(vifp->v_tbf)));
    bzero((caddr_t)vifp, sizeof (*vifp));

    if (mrtdebug)
	log(LOG_DEBUG, "del_vif %d, numvifs %d\n", vifi, numvifs);

    /* Adjust numvifs down */
    for (vifi = numvifs; vifi > 0; vifi--)
	if (viftable[vifi-1].v_lcl_addr.s_addr != INADDR_ANY)
	    break;
    numvifs = vifi;

    splx(s);

    return 0;
}

/*
 * update an mfc entry without resetting counters and S,G addresses.
 */
static void
update_mfc_params(struct mfc *rt, struct mfcctl *mfccp)
{
    int i;

    rt->mfc_parent = mfccp->mfcc_parent;
    for (i = 0; i < numvifs; i++)
	rt->mfc_ttls[i] = mfccp->mfcc_ttls[i];
}

/*
 * fully initialize an mfc entry from the parameter.
 */
static void
init_mfc_params(struct mfc *rt, struct mfcctl *mfccp)
{
    rt->mfc_origin     = mfccp->mfcc_origin;
    rt->mfc_mcastgrp   = mfccp->mfcc_mcastgrp;

    update_mfc_params(rt, mfccp);

    /* initialize pkt counters per src-grp */
    rt->mfc_pkt_cnt    = 0;
    rt->mfc_byte_cnt   = 0;
    rt->mfc_wrong_if   = 0;
    rt->mfc_last_assert.tv_sec = rt->mfc_last_assert.tv_usec = 0;
}


/*
 * Add an mfc entry
 */
static int
add_mfc(struct mfcctl *mfccp)
{
    struct mfc *rt;
    u_long hash;
    struct rtdetq *rte;
    u_short nstl;
    int s;

    rt = mfc_find(mfccp->mfcc_origin.s_addr, mfccp->mfcc_mcastgrp.s_addr);

    /* If an entry already exists, just update the fields */
    if (rt) {
	if (mrtdebug & DEBUG_MFC)
	    log(LOG_DEBUG,"add_mfc update o %lx g %lx p %x\n",
		(u_long)ntohl(mfccp->mfcc_origin.s_addr),
		(u_long)ntohl(mfccp->mfcc_mcastgrp.s_addr),
		mfccp->mfcc_parent);

	s = splnet();
	update_mfc_params(rt, mfccp);
	splx(s);
	return 0;
    }

    /* 
     * Find the entry for which the upcall was made and update
     */
    s = splnet();
    hash = MFCHASH(mfccp->mfcc_origin.s_addr, mfccp->mfcc_mcastgrp.s_addr);
    for (rt = mfctable[hash], nstl = 0; rt; rt = rt->mfc_next) {

	if ((rt->mfc_origin.s_addr == mfccp->mfcc_origin.s_addr) &&
		(rt->mfc_mcastgrp.s_addr == mfccp->mfcc_mcastgrp.s_addr) &&
		(rt->mfc_stall != NULL)) {
  
	    if (nstl++)
		log(LOG_ERR, "add_mfc %s o %lx g %lx p %x dbx %p\n",
		    "multiple kernel entries",
		    (u_long)ntohl(mfccp->mfcc_origin.s_addr),
		    (u_long)ntohl(mfccp->mfcc_mcastgrp.s_addr),
		    mfccp->mfcc_parent, (void *)rt->mfc_stall);

	    if (mrtdebug & DEBUG_MFC)
		log(LOG_DEBUG,"add_mfc o %lx g %lx p %x dbg %p\n",
		    (u_long)ntohl(mfccp->mfcc_origin.s_addr),
		    (u_long)ntohl(mfccp->mfcc_mcastgrp.s_addr),
		    mfccp->mfcc_parent, (void *)rt->mfc_stall);

	    init_mfc_params(rt, mfccp);

	    rt->mfc_expire = 0;	/* Don't clean this guy up */
	    nexpire[hash]--;

	    /* free packets Qed at the end of this entry */
	    for (rte = rt->mfc_stall; rte != NULL; ) {
		struct rtdetq *n = rte->next;

		ip_mdq(rte->m, rte->ifp, rt, -1);
		m_freem(rte->m);
		free(rte, M_MRTABLE);
		rte = n;
	    }
	    rt->mfc_stall = NULL;
	}
    }

    /*
     * It is possible that an entry is being inserted without an upcall
     */
    if (nstl == 0) {
	if (mrtdebug & DEBUG_MFC)
	    log(LOG_DEBUG,"add_mfc no upcall h %lu o %lx g %lx p %x\n",
		hash, (u_long)ntohl(mfccp->mfcc_origin.s_addr),
		(u_long)ntohl(mfccp->mfcc_mcastgrp.s_addr),
		mfccp->mfcc_parent);
	
	for (rt = mfctable[hash]; rt != NULL; rt = rt->mfc_next) {
	    if ((rt->mfc_origin.s_addr == mfccp->mfcc_origin.s_addr) &&
		    (rt->mfc_mcastgrp.s_addr == mfccp->mfcc_mcastgrp.s_addr)) {
		init_mfc_params(rt, mfccp);
		if (rt->mfc_expire)
		    nexpire[hash]--;
		rt->mfc_expire = 0;
		break; /* XXX */
	    }
	}
	if (rt == NULL) {		/* no upcall, so make a new entry */
	    rt = (struct mfc *)malloc(sizeof(*rt), M_MRTABLE, M_NOWAIT);
	    if (rt == NULL) {
		splx(s);
		return ENOBUFS;
	    }
	    
	    init_mfc_params(rt, mfccp);
	    rt->mfc_expire     = 0;
	    rt->mfc_stall      = NULL;
	    
	    /* insert new entry at head of hash chain */
	    rt->mfc_next = mfctable[hash];
	    mfctable[hash] = rt;
	}
    }
    splx(s);
    return 0;
}

/*
 * Delete an mfc entry
 */
static int
del_mfc(struct mfcctl *mfccp)
{
    struct in_addr 	origin;
    struct in_addr 	mcastgrp;
    struct mfc 		*rt;
    struct mfc	 	**nptr;
    u_long 		hash;
    int s;

    origin = mfccp->mfcc_origin;
    mcastgrp = mfccp->mfcc_mcastgrp;

    if (mrtdebug & DEBUG_MFC)
	log(LOG_DEBUG,"del_mfc orig %lx mcastgrp %lx\n",
	    (u_long)ntohl(origin.s_addr), (u_long)ntohl(mcastgrp.s_addr));

    s = splnet();

    hash = MFCHASH(origin.s_addr, mcastgrp.s_addr);
    for (nptr = &mfctable[hash]; (rt = *nptr) != NULL; nptr = &rt->mfc_next)
	if (origin.s_addr == rt->mfc_origin.s_addr &&
		mcastgrp.s_addr == rt->mfc_mcastgrp.s_addr &&
		rt->mfc_stall == NULL)
	    break;
    if (rt == NULL) {
	splx(s);
	return EADDRNOTAVAIL;
    }

    *nptr = rt->mfc_next;
    free(rt, M_MRTABLE);

    splx(s);

    return 0;
}

/*
 * Send a message to mrouted on the multicast routing socket
 */
static int
socket_send(struct socket *s, struct mbuf *mm, struct sockaddr_in *src)
{
    if (s) {
	if (sbappendaddr(&s->so_rcv, (struct sockaddr *)src, mm, NULL) != 0) {
	    sorwakeup(s);
	    return 0;
	}
    }
    m_freem(mm);
    return -1;
}

/*
 * IP multicast forwarding function. This function assumes that the packet
 * pointed to by "ip" has arrived on (or is about to be sent to) the interface
 * pointed to by "ifp", and the packet is to be relayed to other networks
 * that have members of the packet's destination IP multicast group.
 *
 * The packet is returned unscathed to the caller, unless it is
 * erroneous, in which case a non-zero return value tells the caller to
 * discard it.
 */

#define TUNNEL_LEN  12  /* # bytes of IP option for tunnel encapsulation  */

static int
X_ip_mforward(struct ip *ip, struct ifnet *ifp,
	struct mbuf *m, struct ip_moptions *imo)
{
    struct mfc *rt;
    int s;
    vifi_t vifi;

    if (mrtdebug & DEBUG_FORWARD)
	log(LOG_DEBUG, "ip_mforward: src %lx, dst %lx, ifp %p\n",
	    (u_long)ntohl(ip->ip_src.s_addr), (u_long)ntohl(ip->ip_dst.s_addr),
	    (void *)ifp);

    if (ip->ip_hl < (sizeof(struct ip) + TUNNEL_LEN) >> 2 ||
		((u_char *)(ip + 1))[1] != IPOPT_LSRR ) {
	/*
	 * Packet arrived via a physical interface or
	 * an encapsulated tunnel.
	 */
    } else {
	/*
	 * Packet arrived through a source-route tunnel.
	 * Source-route tunnels are no longer supported.
	 */
	static int last_log;
	if (last_log != time_second) {
	    last_log = time_second;
	    log(LOG_ERR,
		"ip_mforward: received source-routed packet from %lx\n",
		(u_long)ntohl(ip->ip_src.s_addr));
	}
	return 1;
    }

    if ((imo) && ((vifi = imo->imo_multicast_vif) < numvifs)) {
	if (ip->ip_ttl < 255)
	    ip->ip_ttl++;	/* compensate for -1 in *_send routines */
	if (rsvpdebug && ip->ip_p == IPPROTO_RSVP) {
	    struct vif *vifp = viftable + vifi;

	    printf("Sending IPPROTO_RSVP from %lx to %lx on vif %d (%s%s%d)\n",
		(long)ntohl(ip->ip_src.s_addr), (long)ntohl(ip->ip_dst.s_addr),
		vifi,
		(vifp->v_flags & VIFF_TUNNEL) ? "tunnel on " : "",
		vifp->v_ifp->if_name, vifp->v_ifp->if_unit);
	}
	return ip_mdq(m, ifp, NULL, vifi);
    }
    if (rsvpdebug && ip->ip_p == IPPROTO_RSVP) {
	printf("Warning: IPPROTO_RSVP from %lx to %lx without vif option\n",
	    (long)ntohl(ip->ip_src.s_addr), (long)ntohl(ip->ip_dst.s_addr));
	if (!imo)
	    printf("In fact, no options were specified at all\n");
    }

    /*
     * Don't forward a packet with time-to-live of zero or one,
     * or a packet destined to a local-only group.
     */
    if (ip->ip_ttl <= 1 || ntohl(ip->ip_dst.s_addr) <= INADDR_MAX_LOCAL_GROUP)
	return 0;

    /*
     * Determine forwarding vifs from the forwarding cache table
     */
    s = splnet();
    ++mrtstat.mrts_mfc_lookups;
    rt = mfc_find(ip->ip_src.s_addr, ip->ip_dst.s_addr);

    /* Entry exists, so forward if necessary */
    if (rt != NULL) {
	splx(s);
	return ip_mdq(m, ifp, rt, -1);
    } else {
	/*
	 * If we don't have a route for packet's origin,
	 * Make a copy of the packet & send message to routing daemon
	 */

	struct mbuf *mb0;
	struct rtdetq *rte;
	u_long hash;
	int hlen = ip->ip_hl << 2;

	++mrtstat.mrts_mfc_misses;

	mrtstat.mrts_no_route++;
	if (mrtdebug & (DEBUG_FORWARD | DEBUG_MFC))
	    log(LOG_DEBUG, "ip_mforward: no rte s %lx g %lx\n",
		(u_long)ntohl(ip->ip_src.s_addr),
		(u_long)ntohl(ip->ip_dst.s_addr));

	/*
	 * Allocate mbufs early so that we don't do extra work if we are
	 * just going to fail anyway.  Make sure to pullup the header so
	 * that other people can't step on it.
	 */
	rte = (struct rtdetq *)malloc((sizeof *rte), M_MRTABLE, M_NOWAIT);
	if (rte == NULL) {
	    splx(s);
	    return ENOBUFS;
	}
	mb0 = m_copy(m, 0, M_COPYALL);
	if (mb0 && (M_HASCL(mb0) || mb0->m_len < hlen))
	    mb0 = m_pullup(mb0, hlen);
	if (mb0 == NULL) {
	    free(rte, M_MRTABLE);
	    splx(s);
	    return ENOBUFS;
	}

	/* is there an upcall waiting for this flow ? */
	hash = MFCHASH(ip->ip_src.s_addr, ip->ip_dst.s_addr);
	for (rt = mfctable[hash]; rt; rt = rt->mfc_next) {
	    if ((ip->ip_src.s_addr == rt->mfc_origin.s_addr) &&
		    (ip->ip_dst.s_addr == rt->mfc_mcastgrp.s_addr) &&
		    (rt->mfc_stall != NULL))
		break;
	}

	if (rt == NULL) {
	    int i;
	    struct igmpmsg *im;
	    struct sockaddr_in k_igmpsrc = { sizeof k_igmpsrc, AF_INET };
	    struct mbuf *mm;

	    /*
	     * Locate the vifi for the incoming interface for this packet.
	     * If none found, drop packet.
	     */
	    for (vifi=0; vifi<numvifs && viftable[vifi].v_ifp != ifp; vifi++)
		;
            if (vifi >= numvifs)	/* vif not found, drop packet */
		goto non_fatal;

	    /* no upcall, so make a new entry */
	    rt = (struct mfc *)malloc(sizeof(*rt), M_MRTABLE, M_NOWAIT);
	    if (rt == NULL)
		goto fail;
	    /* Make a copy of the header to send to the user level process */
	    mm = m_copy(mb0, 0, hlen);
	    if (mm == NULL)
		goto fail1;

	    /* 
	     * Send message to routing daemon to install 
	     * a route into the kernel table
	     */
	    
	    im = mtod(mm, struct igmpmsg *);
	    im->im_msgtype = IGMPMSG_NOCACHE;
	    im->im_mbz = 0;
	    im->im_vif = vifi;

	    mrtstat.mrts_upcalls++;

	    k_igmpsrc.sin_addr = ip->ip_src;
	    if (socket_send(ip_mrouter, mm, &k_igmpsrc) < 0) {
		log(LOG_WARNING, "ip_mforward: ip_mrouter socket queue full\n");
		++mrtstat.mrts_upq_sockfull;
fail1:
		free(rt, M_MRTABLE);
fail:
		free(rte, M_MRTABLE);
		m_freem(mb0);
		splx(s);
		return ENOBUFS;
	    }

	    /* insert new entry at head of hash chain */
	    rt->mfc_origin.s_addr     = ip->ip_src.s_addr;
	    rt->mfc_mcastgrp.s_addr   = ip->ip_dst.s_addr;
	    rt->mfc_expire	      = UPCALL_EXPIRE;
	    nexpire[hash]++;
	    for (i = 0; i < numvifs; i++)
		rt->mfc_ttls[i] = 0;
	    rt->mfc_parent = -1;

	    /* link into table */
	    rt->mfc_next   = mfctable[hash];
	    mfctable[hash] = rt;
	    rt->mfc_stall = rte;

	} else {
	    /* determine if q has overflowed */
	    int npkts = 0;
	    struct rtdetq **p;

	    /*
	     * XXX ouch! we need to append to the list, but we
	     * only have a pointer to the front, so we have to
	     * scan the entire list every time.
	     */
	    for (p = &rt->mfc_stall; *p != NULL; p = &(*p)->next)
		npkts++;

	    if (npkts > MAX_UPQ) {
		mrtstat.mrts_upq_ovflw++;
non_fatal:
		free(rte, M_MRTABLE);
		m_freem(mb0);
		splx(s);
		return 0;
	    }

	    /* Add this entry to the end of the queue */
	    *p = rte;
	}

	rte->m 			= mb0;
	rte->ifp 		= ifp;
	rte->next		= NULL;

	splx(s);

	return 0;
    }		
}

/*
 * Clean up the cache entry if upcall is not serviced
 */
static void
expire_upcalls(void *unused)
{
    struct rtdetq *rte;
    struct mfc *mfc, **nptr;
    int i;
    int s;

    s = splnet();
    for (i = 0; i < MFCTBLSIZ; i++) {
	if (nexpire[i] == 0)
	    continue;
	nptr = &mfctable[i];
	for (mfc = *nptr; mfc != NULL; mfc = *nptr) {
	    /*
	     * Skip real cache entries
	     * Make sure it wasn't marked to not expire (shouldn't happen)
	     * If it expires now
	     */
	    if (mfc->mfc_stall != NULL && mfc->mfc_expire != 0 &&
		    --mfc->mfc_expire == 0) {
		if (mrtdebug & DEBUG_EXPIRE)
		    log(LOG_DEBUG, "expire_upcalls: expiring (%lx %lx)\n",
			(u_long)ntohl(mfc->mfc_origin.s_addr),
			(u_long)ntohl(mfc->mfc_mcastgrp.s_addr));
		/*
		 * drop all the packets
		 * free the mbuf with the pkt, if, timing info
		 */
		for (rte = mfc->mfc_stall; rte; ) {
		    struct rtdetq *n = rte->next;

		    m_freem(rte->m);
		    free(rte, M_MRTABLE);
		    rte = n;
		}
		++mrtstat.mrts_cache_cleanups;
		nexpire[i]--;

		*nptr = mfc->mfc_next;
		free(mfc, M_MRTABLE);
	    } else {
		nptr = &mfc->mfc_next;
	    }
	}
    }
    splx(s);
    expire_upcalls_ch = timeout(expire_upcalls, NULL, EXPIRE_TIMEOUT);
}

/*
 * Packet forwarding routine once entry in the cache is made
 */
static int
ip_mdq(struct mbuf *m, struct ifnet *ifp, struct mfc *rt, vifi_t xmt_vif)
{
    struct ip  *ip = mtod(m, struct ip *);
    vifi_t vifi;
    int plen = ip->ip_len;

/*
 * Macro to send packet on vif.  Since RSVP packets don't get counted on
 * input, they shouldn't get counted on output, so statistics keeping is
 * separate.
 */
#define MC_SEND(ip,vifp,m) {                             \
                if ((vifp)->v_flags & VIFF_TUNNEL)  	 \
                    encap_send((ip), (vifp), (m));       \
                else                                     \
                    phyint_send((ip), (vifp), (m));      \
}

    /*
     * If xmt_vif is not -1, send on only the requested vif.
     *
     * (since vifi_t is u_short, -1 becomes MAXUSHORT, which > numvifs.)
     */
    if (xmt_vif < numvifs) {
	MC_SEND(ip, viftable + xmt_vif, m);
	return 1;
    }

    /*
     * Don't forward if it didn't arrive from the parent vif for its origin.
     */
    vifi = rt->mfc_parent;
    if ((vifi >= numvifs) || (viftable[vifi].v_ifp != ifp)) {
	/* came in the wrong interface */
	if (mrtdebug & DEBUG_FORWARD)
	    log(LOG_DEBUG, "wrong if: ifp %p vifi %d vififp %p\n",
		(void *)ifp, vifi, (void *)viftable[vifi].v_ifp); 
	++mrtstat.mrts_wrong_if;
	++rt->mfc_wrong_if;
	/*
	 * If we are doing PIM assert processing, and we are forwarding
	 * packets on this interface, and it is a broadcast medium
	 * interface (and not a tunnel), send a message to the routing daemon.
	 */
	if (pim_assert && rt->mfc_ttls[vifi] &&
		(ifp->if_flags & IFF_BROADCAST) &&
		!(viftable[vifi].v_flags & VIFF_TUNNEL)) {
	    struct timeval now;
	    u_long delta;

	    GET_TIME(now);

	    TV_DELTA(rt->mfc_last_assert, now, delta);

	    if (delta > ASSERT_MSG_TIME) {
		struct sockaddr_in k_igmpsrc = { sizeof k_igmpsrc, AF_INET };
		struct igmpmsg *im;
		int hlen = ip->ip_hl << 2;
		struct mbuf *mm = m_copy(m, 0, hlen);

		if (mm && (M_HASCL(mm) || mm->m_len < hlen))
		    mm = m_pullup(mm, hlen);
		if (mm == NULL)
		    return ENOBUFS;

		rt->mfc_last_assert = now;

		im = mtod(mm, struct igmpmsg *);
		im->im_msgtype	= IGMPMSG_WRONGVIF;
		im->im_mbz		= 0;
		im->im_vif		= vifi;

		k_igmpsrc.sin_addr = im->im_src;

		if (socket_send(ip_mrouter, mm, &k_igmpsrc) < 0) {
		    log(LOG_WARNING,
			"ip_mforward: ip_mrouter socket queue full\n");
		    ++mrtstat.mrts_upq_sockfull;
		    return ENOBUFS;
		}
	    }
	}
	return 0;
    }

    /* If I sourced this packet, it counts as output, else it was input. */
    if (ip->ip_src.s_addr == viftable[vifi].v_lcl_addr.s_addr) {
	viftable[vifi].v_pkt_out++;
	viftable[vifi].v_bytes_out += plen;
    } else {
	viftable[vifi].v_pkt_in++;
	viftable[vifi].v_bytes_in += plen;
    }
    rt->mfc_pkt_cnt++;
    rt->mfc_byte_cnt += plen;

    /*
     * For each vif, decide if a copy of the packet should be forwarded.
     * Forward if:
     *		- the ttl exceeds the vif's threshold
     *		- there are group members downstream on interface
     */
    for (vifi = 0; vifi < numvifs; vifi++)
	if ((rt->mfc_ttls[vifi] > 0) && (ip->ip_ttl > rt->mfc_ttls[vifi])) {
	    viftable[vifi].v_pkt_out++;
	    viftable[vifi].v_bytes_out += plen;
	    MC_SEND(ip, viftable+vifi, m);
	}

    return 0;
}

/*
 * check if a vif number is legal/ok. This is used by ip_output.
 */
static int
X_legal_vif_num(int vif)
{
    return (vif >= 0 && vif < numvifs);
}

/*
 * Return the local address used by this vif
 */
static u_long
X_ip_mcast_src(int vifi)
{
    if (vifi >= 0 && vifi < numvifs)
	return viftable[vifi].v_lcl_addr.s_addr;
    else
	return INADDR_ANY;
}

static void
phyint_send(struct ip *ip, struct vif *vifp, struct mbuf *m)
{
    struct mbuf *mb_copy;
    int hlen = ip->ip_hl << 2;

    /*
     * Make a new reference to the packet; make sure that
     * the IP header is actually copied, not just referenced,
     * so that ip_output() only scribbles on the copy.
     */
    mb_copy = m_copy(m, 0, M_COPYALL);
    if (mb_copy && (M_HASCL(mb_copy) || mb_copy->m_len < hlen))
	mb_copy = m_pullup(mb_copy, hlen);
    if (mb_copy == NULL)
	return;

    if (vifp->v_rate_limit == 0)
	tbf_send_packet(vifp, mb_copy);
    else
	tbf_control(vifp, mb_copy, mtod(mb_copy, struct ip *), ip->ip_len);
}

static void
encap_send(struct ip *ip, struct vif *vifp, struct mbuf *m)
{
    struct mbuf *mb_copy;
    struct ip *ip_copy;
    int i, len = ip->ip_len;

    /*
     * XXX: take care of delayed checksums.
     * XXX: if network interfaces are capable of computing checksum for
     * encapsulated multicast data packets, we need to reconsider this.
     */
    if (m->m_pkthdr.csum_flags & CSUM_DELAY_DATA) {
	in_delayed_cksum(m);
	m->m_pkthdr.csum_flags &= ~CSUM_DELAY_DATA;
    }

    /*
     * copy the old packet & pullup its IP header into the
     * new mbuf so we can modify it.  Try to fill the new
     * mbuf since if we don't the ethernet driver will.
     */
    MGETHDR(mb_copy, M_DONTWAIT, MT_HEADER);
    if (mb_copy == NULL)
	return;
#ifdef MAC
    mac_create_mbuf_multicast_encap(m, vifp->v_ifp, mb_copy);
#endif
    mb_copy->m_data += max_linkhdr;
    mb_copy->m_len = sizeof(multicast_encap_iphdr);

    if ((mb_copy->m_next = m_copy(m, 0, M_COPYALL)) == NULL) {
	m_freem(mb_copy);
	return;
    }
    i = MHLEN - M_LEADINGSPACE(mb_copy);
    if (i > len)
	i = len;
    mb_copy = m_pullup(mb_copy, i);
    if (mb_copy == NULL)
	return;
    mb_copy->m_pkthdr.len = len + sizeof(multicast_encap_iphdr);

    /*
     * fill in the encapsulating IP header.
     */
    ip_copy = mtod(mb_copy, struct ip *);
    *ip_copy = multicast_encap_iphdr;
#ifdef RANDOM_IP_ID
    ip_copy->ip_id = ip_randomid();
#else
    ip_copy->ip_id = htons(ip_id++);
#endif
    ip_copy->ip_len += len;
    ip_copy->ip_src = vifp->v_lcl_addr;
    ip_copy->ip_dst = vifp->v_rmt_addr;

    /*
     * turn the encapsulated IP header back into a valid one.
     */
    ip = (struct ip *)((caddr_t)ip_copy + sizeof(multicast_encap_iphdr));
    --ip->ip_ttl;
    ip->ip_len = htons(ip->ip_len);
    ip->ip_off = htons(ip->ip_off);
    ip->ip_sum = 0;
    mb_copy->m_data += sizeof(multicast_encap_iphdr);
    ip->ip_sum = in_cksum(mb_copy, ip->ip_hl << 2);
    mb_copy->m_data -= sizeof(multicast_encap_iphdr);

    if (vifp->v_rate_limit == 0)
	tbf_send_packet(vifp, mb_copy);
    else
	tbf_control(vifp, mb_copy, ip, ip_copy->ip_len);
}

/*
 * Token bucket filter module
 */

static void
tbf_control(struct vif *vifp, struct mbuf *m, struct ip *ip, u_long p_len)
{
    struct tbf *t = vifp->v_tbf;

    if (p_len > MAX_BKT_SIZE) {		/* drop if packet is too large */
	mrtstat.mrts_pkt2large++;
	m_freem(m);
	return;
    }

    tbf_update_tokens(vifp);

    if (t->tbf_q_len == 0) {		/* queue empty...		*/
	if (p_len <= t->tbf_n_tok) {	/* send packet if enough tokens */
	    t->tbf_n_tok -= p_len;
	    tbf_send_packet(vifp, m);
	} else {			/* no, queue packet and try later */
	    tbf_queue(vifp, m);
	    timeout(tbf_reprocess_q, (caddr_t)vifp, TBF_REPROCESS);
	}
    } else if (t->tbf_q_len < t->tbf_max_q_len) {
	/* finite queue length, so queue pkts and process queue */
	tbf_queue(vifp, m);
	tbf_process_q(vifp);
    } else {
	/* queue full, try to dq and queue and process */
	if (!tbf_dq_sel(vifp, ip)) {
	    mrtstat.mrts_q_overflow++;
	    m_freem(m);
	} else {
	    tbf_queue(vifp, m);
	    tbf_process_q(vifp);
	}
    }
}

/* 
 * adds a packet to the queue at the interface
 */
static void
tbf_queue(struct vif *vifp, struct mbuf *m)
{
    int s = splnet();
    struct tbf *t = vifp->v_tbf;

    if (t->tbf_t == NULL)	/* Queue was empty */
	t->tbf_q = m;
    else			/* Insert at tail */
	t->tbf_t->m_act = m;

    t->tbf_t = m;		/* Set new tail pointer */

#ifdef DIAGNOSTIC
    /* Make sure we didn't get fed a bogus mbuf */
    if (m->m_act)
	panic("tbf_queue: m_act");
#endif
    m->m_act = NULL;

    t->tbf_q_len++;

    splx(s);
}

/* 
 * processes the queue at the interface
 */
static void
tbf_process_q(struct vif *vifp)
{
    int s = splnet();
    struct tbf *t = vifp->v_tbf;

    /* loop through the queue at the interface and send as many packets
     * as possible
     */
    while (t->tbf_q_len > 0) {
	struct mbuf *m = t->tbf_q;
	int len = mtod(m, struct ip *)->ip_len;

	/* determine if the packet can be sent */
	if (len > t->tbf_n_tok)	/* not enough tokens, we are done */
	    break;
	/* ok, reduce no of tokens, dequeue and send the packet. */
	t->tbf_n_tok -= len;

	t->tbf_q = m->m_act;
	if (--t->tbf_q_len == 0)
	    t->tbf_t = NULL;

	m->m_act = NULL;
	tbf_send_packet(vifp, m);
    }
    splx(s);
}

static void
tbf_reprocess_q(void *xvifp)
{
    struct vif *vifp = xvifp;

    if (ip_mrouter == NULL) 
	return;
    tbf_update_tokens(vifp);
    tbf_process_q(vifp);
    if (vifp->v_tbf->tbf_q_len)
	timeout(tbf_reprocess_q, (caddr_t)vifp, TBF_REPROCESS);
}

/* function that will selectively discard a member of the queue
 * based on the precedence value and the priority
 */
static int
tbf_dq_sel(struct vif *vifp, struct ip *ip)
{
    int s = splnet();
    u_int p;
    struct mbuf *m, *last;
    struct mbuf **np;
    struct tbf *t = vifp->v_tbf;

    p = priority(vifp, ip);

    np = &t->tbf_q;
    last = NULL;
    while ((m = *np) != NULL) {
	if (p > priority(vifp, mtod(m, struct ip *))) {
	    *np = m->m_act;
	    /* If we're removing the last packet, fix the tail pointer */
	    if (m == t->tbf_t)
		t->tbf_t = last;
	    m_freem(m);
	    /* It's impossible for the queue to be empty, but check anyways. */
	    if (--t->tbf_q_len == 0)
		t->tbf_t = NULL;
	    splx(s);
	    mrtstat.mrts_drop_sel++;
	    return 1;
	}
	np = &m->m_act;
	last = m;
    }
    splx(s);
    return 0;
}

static void
tbf_send_packet(struct vif *vifp, struct mbuf *m)
{
    int s = splnet();

    if (vifp->v_flags & VIFF_TUNNEL)	/* If tunnel options */
	ip_output(m, NULL, &vifp->v_route, IP_FORWARDING, NULL, NULL);
    else {
	struct ip_moptions imo;
	int error;
	static struct route ro; /* XXX check this */

	imo.imo_multicast_ifp  = vifp->v_ifp;
	imo.imo_multicast_ttl  = mtod(m, struct ip *)->ip_ttl - 1;
	imo.imo_multicast_loop = 1;
	imo.imo_multicast_vif  = -1;

	/*
	 * Re-entrancy should not be a problem here, because
	 * the packets that we send out and are looped back at us
	 * should get rejected because they appear to come from
	 * the loopback interface, thus preventing looping.
	 */
	error = ip_output(m, NULL, &ro, IP_FORWARDING, &imo, NULL);

	if (mrtdebug & DEBUG_XMIT)
	    log(LOG_DEBUG, "phyint_send on vif %d err %d\n", 
		(int)(vifp - viftable), error);
    }
    splx(s);
}

/* determine the current time and then
 * the elapsed time (between the last time and time now)
 * in milliseconds & update the no. of tokens in the bucket
 */
static void
tbf_update_tokens(struct vif *vifp)
{
    struct timeval tp;
    u_long tm;
    int s = splnet();
    struct tbf *t = vifp->v_tbf;

    GET_TIME(tp);

    TV_DELTA(tp, t->tbf_last_pkt_t, tm);

    /*
     * This formula is actually
     * "time in seconds" * "bytes/second".
     *
     * (tm / 1000000) * (v_rate_limit * 1000 * (1000/1024) / 8)
     *
     * The (1000/1024) was introduced in add_vif to optimize
     * this divide into a shift.
     */
    t->tbf_n_tok += tm * vifp->v_rate_limit / 1024 / 8;
    t->tbf_last_pkt_t = tp;

    if (t->tbf_n_tok > MAX_BKT_SIZE)
	t->tbf_n_tok = MAX_BKT_SIZE;

    splx(s);
}

static int
priority(struct vif *vifp, struct ip *ip)
{
    int prio = 50; /* the lowest priority -- default case */

    /* temporary hack; may add general packet classifier some day */

    /*
     * The UDP port space is divided up into four priority ranges:
     * [0, 16384)     : unclassified - lowest priority
     * [16384, 32768) : audio - highest priority
     * [32768, 49152) : whiteboard - medium priority
     * [49152, 65536) : video - low priority
     *
     * Everything else gets lowest priority.
     */
    if (ip->ip_p == IPPROTO_UDP) {
	struct udphdr *udp = (struct udphdr *)(((char *)ip) + (ip->ip_hl << 2));
	switch (ntohs(udp->uh_dport) & 0xc000) {
	case 0x4000:
	    prio = 70;
	    break;
	case 0x8000:
	    prio = 60;
	    break;
	case 0xc000:
	    prio = 55;
	    break;
	}
    }
    return prio;
}

/*
 * End of token bucket filter modifications 
 */

static int
X_ip_rsvp_vif(struct socket *so, struct sockopt *sopt)
{
    int error, vifi, s;

    if (so->so_type != SOCK_RAW || so->so_proto->pr_protocol != IPPROTO_RSVP)
	return EOPNOTSUPP;

    error = sooptcopyin(sopt, &vifi, sizeof vifi, sizeof vifi);
    if (error)
	return error;
 
    s = splnet();

    if (vifi < 0 || vifi >= numvifs) {	/* Error if vif is invalid */
	splx(s);
	return EADDRNOTAVAIL;
    }

    if (sopt->sopt_name == IP_RSVP_VIF_ON) {
	/* Check if socket is available. */
	if (viftable[vifi].v_rsvpd != NULL) {
	    splx(s);
	    return EADDRINUSE;
	}

	viftable[vifi].v_rsvpd = so;
	/* This may seem silly, but we need to be sure we don't over-increment
	 * the RSVP counter, in case something slips up.
	 */
	if (!viftable[vifi].v_rsvp_on) {
	    viftable[vifi].v_rsvp_on = 1;
	    rsvp_on++;
	}
    } else { /* must be VIF_OFF */
	/*
	 * XXX as an additional consistency check, one could make sure
	 * that viftable[vifi].v_rsvpd == so, otherwise passing so as
	 * first parameter is pretty useless.
	 */
	viftable[vifi].v_rsvpd = NULL;
	/*
	 * This may seem silly, but we need to be sure we don't over-decrement
	 * the RSVP counter, in case something slips up.
	 */
	if (viftable[vifi].v_rsvp_on) {
	    viftable[vifi].v_rsvp_on = 0;
	    rsvp_on--;
	}
    }
    splx(s);
    return 0;
}

static void
X_ip_rsvp_force_done(struct socket *so)
{
    int vifi;
    int s;

    /* Don't bother if it is not the right type of socket. */
    if (so->so_type != SOCK_RAW || so->so_proto->pr_protocol != IPPROTO_RSVP)
	return;

    s = splnet();

    /* The socket may be attached to more than one vif...this
     * is perfectly legal.
     */
    for (vifi = 0; vifi < numvifs; vifi++) {
	if (viftable[vifi].v_rsvpd == so) {
	    viftable[vifi].v_rsvpd = NULL;
	    /* This may seem silly, but we need to be sure we don't
	     * over-decrement the RSVP counter, in case something slips up.
	     */
	    if (viftable[vifi].v_rsvp_on) {
		viftable[vifi].v_rsvp_on = 0;
		rsvp_on--;
	    }
	}
    }

    splx(s);
}

static void
X_rsvp_input(struct mbuf *m, int off)
{
    int vifi;
    struct ip *ip = mtod(m, struct ip *);
    struct sockaddr_in rsvp_src = { sizeof rsvp_src, AF_INET };
    int s;
    struct ifnet *ifp;

    if (rsvpdebug)
	printf("rsvp_input: rsvp_on %d\n",rsvp_on);

    /* Can still get packets with rsvp_on = 0 if there is a local member
     * of the group to which the RSVP packet is addressed.  But in this
     * case we want to throw the packet away.
     */
    if (!rsvp_on) {
	m_freem(m);
	return;
    }

    s = splnet();

    if (rsvpdebug)
	printf("rsvp_input: check vifs\n");

#ifdef DIAGNOSTIC
    if (!(m->m_flags & M_PKTHDR))
	panic("rsvp_input no hdr");
#endif

    ifp = m->m_pkthdr.rcvif;
    /* Find which vif the packet arrived on. */
    for (vifi = 0; vifi < numvifs; vifi++)
	if (viftable[vifi].v_ifp == ifp)
	    break;

    if (vifi == numvifs || viftable[vifi].v_rsvpd == NULL) {
	/*
	 * If the old-style non-vif-associated socket is set,
	 * then use it.  Otherwise, drop packet since there
	 * is no specific socket for this vif.
	 */
	if (ip_rsvpd != NULL) {
	    if (rsvpdebug)
		printf("rsvp_input: Sending packet up old-style socket\n");
	    rip_input(m, off);  /* xxx */
	} else {
	    if (rsvpdebug && vifi == numvifs)
		printf("rsvp_input: Can't find vif for packet.\n");
	    else if (rsvpdebug && viftable[vifi].v_rsvpd == NULL)
		printf("rsvp_input: No socket defined for vif %d\n",vifi);
	    m_freem(m);
	}
	splx(s);
	return;
    }
    rsvp_src.sin_addr = ip->ip_src;

    if (rsvpdebug && m)
	printf("rsvp_input: m->m_len = %d, sbspace() = %ld\n",
	       m->m_len,sbspace(&(viftable[vifi].v_rsvpd->so_rcv)));

    if (socket_send(viftable[vifi].v_rsvpd, m, &rsvp_src) < 0) {
	if (rsvpdebug)
	    printf("rsvp_input: Failed to append to socket\n");
    } else {
	if (rsvpdebug)
	    printf("rsvp_input: send packet up\n");
    }

    splx(s);
}

static int
ip_mroute_modevent(module_t mod, int type, void *unused)
{
    int s;

    switch (type) {
    case MOD_LOAD:
	s = splnet();
	/* XXX Protect against multiple loading */
	ip_mcast_src = X_ip_mcast_src;
	ip_mforward = X_ip_mforward;
	ip_mrouter_done = X_ip_mrouter_done;
	ip_mrouter_get = X_ip_mrouter_get;
	ip_mrouter_set = X_ip_mrouter_set;
	ip_rsvp_force_done = X_ip_rsvp_force_done;
	ip_rsvp_vif = X_ip_rsvp_vif;
	legal_vif_num = X_legal_vif_num;
	mrt_ioctl = X_mrt_ioctl;
	rsvp_input_p = X_rsvp_input;
	splx(s);
	break;

    case MOD_UNLOAD:
	if (ip_mrouter)
	    return EINVAL;

	s = splnet();
	ip_mcast_src = NULL;
	ip_mforward = NULL;
	ip_mrouter_done = NULL;
	ip_mrouter_get = NULL;
	ip_mrouter_set = NULL;
	ip_rsvp_force_done = NULL;
	ip_rsvp_vif = NULL;
	legal_vif_num = NULL;
	mrt_ioctl = NULL;
	rsvp_input_p = NULL;
	splx(s);
	break;
    }
    return 0;
}

static moduledata_t ip_mroutemod = {
    "ip_mroute",
    ip_mroute_modevent,
    0
};
DECLARE_MODULE(ip_mroute, ip_mroutemod, SI_SUB_PSEUDO, SI_ORDER_ANY);
