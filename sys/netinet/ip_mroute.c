/*
 * IP multicast forwarding procedures
 *
 * Written by David Waitzman, BBN Labs, August 1988.
 * Modified by Steve Deering, Stanford, February 1989.
 * Modified by Mark J. Steiglitz, Stanford, May, 1991
 * Modified by Van Jacobson, LBL, January 1993
 * Modified by Ajit Thyagarajan, PARC, August 1993
 *
 * MROUTING 1.8
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/igmp.h>
#include <netinet/igmp_var.h>
#include <netinet/ip_mroute.h>

#ifndef NTOHL
#if BYTE_ORDER != BIG_ENDIAN
#define NTOHL(d) ((d) = ntohl((d)))
#define NTOHS(d) ((d) = ntohs((u_short)(d)))
#define HTONL(d) ((d) = htonl((d)))
#define HTONS(d) ((d) = htons((u_short)(d)))
#else
#define NTOHL(d)
#define NTOHS(d)
#define HTONL(d)
#define HTONS(d)
#endif
#endif

#ifndef MROUTING
/*
 * Dummy routines and globals used when multicast routing is not compiled in.
 */

u_int		ip_mrtproto = 0;
struct socket  *ip_mrouter  = NULL;
struct mrtstat	mrtstat;


int
_ip_mrouter_cmd(cmd, so, m)
	int cmd;
	struct socket *so;
	struct mbuf *m;
{
	return(EOPNOTSUPP);
}

int (*ip_mrouter_cmd)(int, struct socket *, struct mbuf *) = _ip_mrouter_cmd;

int
_ip_mrouter_done()
{
	return(0);
}

int (*ip_mrouter_done)(void) = _ip_mrouter_done;

int
_ip_mforward(ip, ifp, m, imo)
	struct ip *ip;
	struct ifnet *ifp;
	struct mbuf *m;
	struct ip_moptions *imo;
{
	return(0);
}

int (*ip_mforward)(struct ip *, struct ifnet *, struct mbuf *,
		   struct ip_moptions *) = _ip_mforward;

int
_mrt_ioctl(int req, caddr_t data, struct proc *p)
{
	return EOPNOTSUPP;
}

int (*mrt_ioctl)(int, caddr_t, struct proc *) = _mrt_ioctl;

void multiencap_decap(struct mbuf *m) { /* XXX must fixup manually */
	rip_input(m);
}

int (*legal_vif_num)(int) = 0;

#else /* MROUTING */

#define INSIZ		sizeof(struct in_addr)
#define	same(a1, a2) \
	(bcmp((caddr_t)(a1), (caddr_t)(a2), INSIZ) == 0)

#define MT_MRTABLE MT_RTABLE	/* since nothing else uses it */

/*
 * Globals.  All but ip_mrouter and ip_mrtproto could be static,
 * except for netstat or debugging purposes.
 */
#ifndef MROUTE_LKM
struct socket  *ip_mrouter  = NULL;
struct mrtstat	mrtstat;

int		ip_mrtproto = IGMP_DVMRP;    /* for netstat only */
#else /* MROUTE_LKM */
extern struct mrtstat mrtstat;
extern int ip_mrtproto;
#endif

#define NO_RTE_FOUND 	0x1
#define RTE_FOUND	0x2

struct mbuf    *mfctable[MFCTBLSIZ];
struct vif	viftable[MAXVIFS];
u_int		mrtdebug = 0;	  /* debug level 	*/
u_int       	tbfdebug = 0;     /* tbf debug level 	*/

u_long timeout_val = 0;			/* count of outstanding upcalls */

/*
 * Define the token bucket filter structures
 * tbftable -> each vif has one of these for storing info 
 * qtable   -> each interface has an associated queue of pkts 
 */

struct tbf tbftable[MAXVIFS];
struct pkt_queue qtable[MAXVIFS][MAXQSIZE];

/*
 * 'Interfaces' associated with decapsulator (so we can tell
 * packets that went through it from ones that get reflected
 * by a broken gateway).  These interfaces are never linked into
 * the system ifnet list & no routes point to them.  I.e., packets
 * can't be sent this way.  They only exist as a placeholder for
 * multicast source verification.
 */
struct ifnet multicast_decap_if[MAXVIFS];

#define ENCAP_TTL 64
#define ENCAP_PROTO 4

/* prototype IP hdr for encapsulated packets */
struct ip multicast_encap_iphdr = {
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
static vifi_t	   numvifs = 0;
static void (*encap_oldrawip)() = 0;

/*
 * one-back cache used by multiencap_decap to locate a tunnel's vif
 * given a datagram's src ip address.
 */
static u_long last_encap_src;
static struct vif *last_encap_vif;

static u_long nethash_fc(u_long, u_long);
static struct mfc *mfcfind(u_long, u_long);
int get_sg_cnt(struct sioc_sg_req *);
int get_vif_cnt(struct sioc_vif_req *);
int get_vifs(caddr_t);
static int add_vif(struct vifctl *);
static int del_vif(vifi_t *);
static int add_mfc(struct mfcctl *);
static int del_mfc(struct delmfcctl *);
static void cleanup_cache(void *);
static int ip_mdq(struct mbuf *, struct ifnet *, u_long, struct mfc *,
		  struct ip_moptions *);
static void phyint_send(struct ip *, struct vif *, struct mbuf *);
static void srcrt_send(struct ip *, struct vif *, struct mbuf *);
static void encap_send(struct ip *, struct vif *, struct mbuf *);
void tbf_control(struct vif *, struct mbuf *, struct ip *, u_long,
		 struct ip_moptions *);
void tbf_queue(struct vif *, struct mbuf *, struct ip *, struct ip_moptions *);
void tbf_process_q(struct vif *);
void tbf_dequeue(struct vif *, int);
void tbf_reprocess_q(void *);
int tbf_dq_sel(struct vif *, struct ip *);
void tbf_send_packet(struct vif *, struct mbuf *, struct ip_moptions *);
void tbf_update_tokens(struct vif *);
static int priority(struct vif *, struct ip *);
static int ip_mrouter_init(struct socket *);
void multiencap_decap(struct mbuf *m);

/*
 * A simple hash function: returns MFCHASHMOD of the low-order octet of
 * the argument's network or subnet number and the multicast group assoc.
 */ 
static u_long
nethash_fc(m,n)
    register u_long m;
    register u_long n;
{
    struct in_addr in1;
    struct in_addr in2;

    in1.s_addr = m;
    m = in_netof(in1);
    while ((m & 0xff) == 0) m >>= 8;

    in2.s_addr = n;
    n = in_netof(in2);
    while ((n & 0xff) == 0) n >>= 8;

    return (MFCHASHMOD(m) ^ MFCHASHMOD(n));
}

/*
 * this is a direct-mapped cache used to speed the mapping from a
 * datagram source address to the associated multicast route.  Note
 * that unlike mrttable, the hash is on IP address, not IP net number.
 */
#define MFCHASHSIZ 1024
#define MFCHASH(a, g) ((((a) >> 20) ^ ((a) >> 10) ^ (a) ^ \
			((g) >> 20) ^ ((g) >> 10) ^ (g)) & (MFCHASHSIZ-1))
struct mfc *mfchash[MFCHASHSIZ];

/*
 * Find a route for a given origin IP address and Multicast group address
 * Type of service parameter to be added in the future!!!
 */
#define MFCFIND(o, g, rt) { \
	register u_int _mrhasho = o; \
	register u_int _mrhashg = g; \
	_mrhasho = MFCHASH(_mrhasho, _mrhashg); \
	++mrtstat.mrts_mfc_lookups; \
	rt = mfchash[_mrhasho]; \
	if ((rt == NULL) || \
	    ((o & rt->mfc_originmask.s_addr) != rt->mfc_origin.s_addr) || \
	     (g != rt->mfc_mcastgrp.s_addr)) \
	     if ((rt = mfcfind(o, g)) != NULL) \
		mfchash[_mrhasho] = rt; \
}

/*
 * Find route by examining hash table entries
 */
static struct mfc *
mfcfind(origin, mcastgrp)
    u_long origin; 
    u_long mcastgrp;
{
    register struct mbuf *mb_rt;
    register struct mfc *rt;
    register u_long hash;

    hash = nethash_fc(origin, mcastgrp);
    for (mb_rt = mfctable[hash]; mb_rt; mb_rt = mb_rt->m_next) {
	rt = mtod(mb_rt, struct mfc *);
	if (((origin & rt->mfc_originmask.s_addr) == rt->mfc_origin.s_addr) &&
	    (mcastgrp == rt->mfc_mcastgrp.s_addr) &&
	    (mb_rt->m_act == NULL))
	    return (rt);
    }
    mrtstat.mrts_mfc_misses++;
    return NULL;
}

/*
 * Macros to compute elapsed time efficiently
 * Borrowed from Van Jacobson's scheduling code
 */
#define TV_DELTA(a, b, delta) { \
	    register int xxs; \
		\
	    delta = (a).tv_usec - (b).tv_usec; \
	    if ((xxs = (a).tv_sec - (b).tv_sec)) { \
	       switch (xxs) { \
		      case 2: \
			  delta += 1000000; \
			      /* fall through */ \
		      case 1: \
			  delta += 1000000; \
			  break; \
		      default: \
			  delta += (1000000 * xxs); \
	       } \
	    } \
}

#define TV_LT(a, b) (((a).tv_usec < (b).tv_usec && \
	      (a).tv_sec <= (b).tv_sec) || (a).tv_sec < (b).tv_sec)

/*
 * Handle DVMRP setsockopt commands to modify the multicast routing tables.
 */
int
X_ip_mrouter_cmd(cmd, so, m)
    int cmd;
    struct socket *so;
    struct mbuf *m;
{
   if (cmd != DVMRP_INIT && so != ip_mrouter) return EACCES;

    switch (cmd) {
	case DVMRP_INIT:     return ip_mrouter_init(so);
	case DVMRP_DONE:     return ip_mrouter_done();
	case DVMRP_ADD_VIF:  return add_vif (mtod(m, struct vifctl *));
	case DVMRP_DEL_VIF:  return del_vif (mtod(m, vifi_t *));
	case DVMRP_ADD_MFC:  return add_mfc (mtod(m, struct mfcctl *));
	case DVMRP_DEL_MFC:  return del_mfc (mtod(m, struct delmfcctl *));
	default:             return EOPNOTSUPP;
    }
}

#ifndef MROUTE_LKM
int (*ip_mrouter_cmd)(int, struct socket *, struct mbuf *) = X_ip_mrouter_cmd;
#endif

/*
 * Handle ioctl commands to obtain information from the cache
 */
int
X_mrt_ioctl(cmd, data)
    int cmd;
    caddr_t data;
{
    int error = 0;

    switch (cmd) {
      case (SIOCGETVIFINF):		/* Read Virtual Interface (m/cast) */
	  return (get_vifs(data));
	  break;
      case (SIOCGETVIFCNT):
	  return (get_vif_cnt((struct sioc_vif_req *)data));
	  break;
      case (SIOCGETSGCNT):
	  return (get_sg_cnt((struct sioc_sg_req *)data));
	  break;
	default:
	  return (EINVAL);
	  break;
    }
    return error;
}

#ifndef MROUTE_LKM
int (*mrt_ioctl)(int, caddr_t, struct proc *) = X_mrt_ioctl;
#else
extern int (*mrt_ioctl)(int, caddr_t, struct proc *);
#endif

/*
 * returns the packet count for the source group provided
 */
int
get_sg_cnt(req)
    register struct sioc_sg_req *req;
{
    register struct mfc *rt;
    int s;

    s = splnet();
    MFCFIND(req->src.s_addr, req->grp.s_addr, rt);
    splx(s);
    if (rt != NULL)
	req->count = rt->mfc_pkt_cnt;
    else
	req->count = 0xffffffff;

    return 0;
}

/*
 * returns the input and output packet counts on the interface provided
 */
int
get_vif_cnt(req)
    register struct sioc_vif_req *req;
{
    register vifi_t vifi = req->vifi;

    req->icount = viftable[vifi].v_pkt_in;
    req->ocount = viftable[vifi].v_pkt_out;

    return 0;
}

int
get_vifs(data)
    char *data;
{
    struct vif_conf *vifc = (struct vif_conf *)data;
    struct vif_req *vifrp, vifr;
    int space, error=0;

    vifi_t vifi;
    int s;

    space = vifc->vifc_len;
    vifrp  = vifc->vifc_req;

    s = splnet();
    vifc->vifc_num=numvifs;

    for (vifi = 0; vifi <  numvifs; vifi++, vifrp++) {
	if (viftable[vifi].v_lcl_addr.s_addr != 0) {
	    vifr.v_flags=viftable[vifi].v_flags;
	    vifr.v_threshold=viftable[vifi].v_threshold;
	    vifr.v_lcl_addr=viftable[vifi].v_lcl_addr;
	    vifr.v_rmt_addr=viftable[vifi].v_rmt_addr;
	    strncpy(vifr.v_if_name,viftable[vifi].v_ifp->if_name,IFNAMSIZ);
	    if ((space -= sizeof(vifr)) < 0) {
		splx(s);
		return(ENOSPC);
	    }
	    error = copyout((caddr_t)&vifr,(caddr_t)vifrp,(u_int)(sizeof vifr));
	    if (error) {
		splx(s);
		return(error);
	    }
	}
    }
    splx(s);
    return 0;
}
/*
 * Enable multicast routing
 */
static int
ip_mrouter_init(so)
	struct socket *so;
{
    if (so->so_type != SOCK_RAW ||
	so->so_proto->pr_protocol != IPPROTO_IGMP) return EOPNOTSUPP;

    if (ip_mrouter != NULL) return EADDRINUSE;

    ip_mrouter = so;

    if (mrtdebug)
	log(LOG_DEBUG, "ip_mrouter_init\n");

    return 0;
}

/*
 * Disable multicast routing
 */
int
X_ip_mrouter_done()
{
    vifi_t vifi;
    int i;
    struct ifnet *ifp;
    struct ifreq ifr;
    struct mbuf *mb_rt;
    struct mbuf *m;
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
	    ((struct sockaddr_in *)&(ifr.ifr_addr))->sin_family = AF_INET;
	    ((struct sockaddr_in *)&(ifr.ifr_addr))->sin_addr.s_addr
								= INADDR_ANY;
	    ifp = viftable[vifi].v_ifp;
	    (*ifp->if_ioctl)(ifp, SIOCDELMULTI, (caddr_t)&ifr);
	}
    }
    bzero((caddr_t)qtable, sizeof(qtable));
    bzero((caddr_t)tbftable, sizeof(tbftable));
    bzero((caddr_t)viftable, sizeof(viftable));
    numvifs = 0;

    /*
     * Check if any outstanding timeouts remain
     */
    if (timeout_val != 0)
	for (i = 0; i < MFCTBLSIZ; i++) {
	    mb_rt = mfctable[i];
	    while (mb_rt) {
		if ( mb_rt->m_act != NULL) {
		    untimeout(cleanup_cache, (caddr_t)mb_rt);
		    while (mb_rt->m_act) {
		        m = mb_rt->m_act;
			mb_rt->m_act = m->m_act;
			rte = mtod(m, struct rtdetq *);
			m_freem(rte->m);
			m_free(m);
		    }
		    timeout_val--;
		}
	    mb_rt = mb_rt->m_next;
	    }
	    if (timeout_val == 0)
		break;
	}

    /*
     * Free all multicast forwarding cache entries.
     */
    for (i = 0; i < MFCTBLSIZ; i++)
	m_freem(mfctable[i]);

    bzero((caddr_t)mfctable, sizeof(mfctable));
    bzero((caddr_t)mfchash, sizeof(mfchash));

    /*
     * Reset de-encapsulation cache
     */
    last_encap_src = NULL;
    last_encap_vif = NULL;
 
    ip_mrouter = NULL;

    splx(s);

    if (mrtdebug)
	log(LOG_DEBUG, "ip_mrouter_done\n");

    return 0;
}

#ifndef MROUTE_LKM
int (*ip_mrouter_done)(void) = X_ip_mrouter_done;
#endif

/*
 * Add a vif to the vif table
 */
static int
add_vif(vifcp)
    register struct vifctl *vifcp;
{
    register struct vif *vifp = viftable + vifcp->vifc_vifi;
    static struct sockaddr_in sin = {sizeof sin, AF_INET};
    struct ifaddr *ifa;
    struct ifnet *ifp;
    struct ifreq ifr;
    int error, s;
    struct tbf *v_tbf = tbftable + vifcp->vifc_vifi;

    if (vifcp->vifc_vifi >= MAXVIFS)  return EINVAL;
    if (vifp->v_lcl_addr.s_addr != 0) return EADDRINUSE;

    /* Find the interface with an address in AF_INET family */
    sin.sin_addr = vifcp->vifc_lcl_addr;
    ifa = ifa_ifwithaddr((struct sockaddr *)&sin);
    if (ifa == 0) return EADDRNOTAVAIL;
    ifp = ifa->ifa_ifp;

    if (vifcp->vifc_flags & VIFF_TUNNEL) {
	if ((vifcp->vifc_flags & VIFF_SRCRT) == 0) {
          if (encap_oldrawip == 0) {
              extern struct protosw inetsw[];
              extern u_char ip_protox[];
              register u_char pr = ip_protox[ENCAP_PROTO];

              encap_oldrawip = inetsw[pr].pr_input;
              inetsw[pr].pr_input = multiencap_decap;
		for (s = 0; s < MAXVIFS; ++s) {
		    multicast_decap_if[s].if_name = "mdecap";
		    multicast_decap_if[s].if_unit = s;
		}
	    }
	    ifp = &multicast_decap_if[vifcp->vifc_vifi];
	} else {
	    ifp = 0;
	}
    } else {
	/* Make sure the interface supports multicast */
	if ((ifp->if_flags & IFF_MULTICAST) == 0)
	    return EOPNOTSUPP;

	/* Enable promiscuous reception of all IP multicasts from the if */
	((struct sockaddr_in *)&(ifr.ifr_addr))->sin_family = AF_INET;
	((struct sockaddr_in *)&(ifr.ifr_addr))->sin_addr.s_addr = INADDR_ANY;
	s = splnet();
	error = (*ifp->if_ioctl)(ifp, SIOCADDMULTI, (caddr_t)&ifr);
	splx(s);
	if (error)
	    return error;
    }

    s = splnet();
    /* define parameters for the tbf structure */
    vifp->v_tbf = v_tbf;
    vifp->v_tbf->q_len = 0;
    vifp->v_tbf->n_tok = 0;
    vifp->v_tbf->last_pkt_t = 0;

    vifp->v_flags     = vifcp->vifc_flags;
    vifp->v_threshold = vifcp->vifc_threshold;
    vifp->v_lcl_addr  = vifcp->vifc_lcl_addr;
    vifp->v_rmt_addr  = vifcp->vifc_rmt_addr;
    vifp->v_ifp       = ifp;
    vifp->v_rate_limit= vifcp->vifc_rate_limit;
    /* initialize per vif pkt counters */
    vifp->v_pkt_in    = 0;
    vifp->v_pkt_out   = 0;
    splx(s);

    /* Adjust numvifs up if the vifi is higher than numvifs */
    if (numvifs <= vifcp->vifc_vifi) numvifs = vifcp->vifc_vifi + 1;

    if (mrtdebug)
	log(LOG_DEBUG, "add_vif #%d, lcladdr %x, %s %x, thresh %x, rate %d\n",
	    vifcp->vifc_vifi, 
	    ntohl(vifcp->vifc_lcl_addr.s_addr),
	    (vifcp->vifc_flags & VIFF_TUNNEL) ? "rmtaddr" : "mask",
	    ntohl(vifcp->vifc_rmt_addr.s_addr),
	    vifcp->vifc_threshold,
	    vifcp->vifc_rate_limit);    

    return 0;
}

/*
 * Delete a vif from the vif table
 */
static int
del_vif(vifip)
    vifi_t *vifip;
{
    register struct vif *vifp = viftable + *vifip;
    register vifi_t vifi;
    struct ifnet *ifp;
    struct ifreq ifr;
    int s;

    if (*vifip >= numvifs) return EINVAL;
    if (vifp->v_lcl_addr.s_addr == 0) return EADDRNOTAVAIL;

    s = splnet();

    if (!(vifp->v_flags & VIFF_TUNNEL)) {
	((struct sockaddr_in *)&(ifr.ifr_addr))->sin_family = AF_INET;
	((struct sockaddr_in *)&(ifr.ifr_addr))->sin_addr.s_addr = INADDR_ANY;
	ifp = vifp->v_ifp;
	(*ifp->if_ioctl)(ifp, SIOCDELMULTI, (caddr_t)&ifr);
    }

    if (vifp == last_encap_vif) {
	last_encap_vif = 0;
	last_encap_src = 0;
    }

    bzero((caddr_t)qtable[*vifip],
	  sizeof(qtable[*vifip]));
    bzero((caddr_t)vifp->v_tbf, sizeof(*(vifp->v_tbf)));
    bzero((caddr_t)vifp, sizeof (*vifp));

    /* Adjust numvifs down */
    for (vifi = numvifs; vifi > 0; vifi--)
	if (viftable[vifi-1].v_lcl_addr.s_addr != 0) break;
    numvifs = vifi;

    splx(s);

    if (mrtdebug)
      log(LOG_DEBUG, "del_vif %d, numvifs %d\n", *vifip, numvifs);

    return 0;
}

/*
 * Add an mfc entry
 */
static int
add_mfc(mfccp)
    struct mfcctl *mfccp;
{
    struct mfc *rt;
    struct mfc *rt1 = 0;
    register struct mbuf *mb_rt;
    struct mbuf *prev_mb_rt;
    u_long hash;
    struct mbuf *mb_ntry;
    struct rtdetq *rte;
    register u_short nstl;
    int s;
    int i;

    rt = mfcfind(mfccp->mfcc_origin.s_addr, mfccp->mfcc_mcastgrp.s_addr);

    /* If an entry already exists, just update the fields */
    if (rt) {
	if (mrtdebug)
	    log(LOG_DEBUG,"add_mfc update o %x g %x m %x p %x\n",
		ntohl(mfccp->mfcc_origin.s_addr),
		ntohl(mfccp->mfcc_mcastgrp.s_addr),
		ntohl(mfccp->mfcc_originmask.s_addr),
		mfccp->mfcc_parent);

	s = splnet();
	rt->mfc_parent = mfccp->mfcc_parent;
	for (i = 0; i < numvifs; i++)
	    VIFM_COPY(mfccp->mfcc_ttls[i], rt->mfc_ttls[i]);
	splx(s);
	return 0;
    }

    /* 
     * Find the entry for which the upcall was made and update
     */
    s = splnet();
    hash = nethash_fc(mfccp->mfcc_origin.s_addr, mfccp->mfcc_mcastgrp.s_addr);
    for (prev_mb_rt = mb_rt = mfctable[hash], nstl = 0; 
	 mb_rt; prev_mb_rt = mb_rt, mb_rt = mb_rt->m_next) {

	rt = mtod(mb_rt, struct mfc *);
	if (((rt->mfc_origin.s_addr & mfccp->mfcc_originmask.s_addr) 
	     == mfccp->mfcc_origin.s_addr) &&
	    (rt->mfc_mcastgrp.s_addr == mfccp->mfcc_mcastgrp.s_addr) &&
	    (mb_rt->m_act != NULL)) {

	    if (!nstl++) {
		if (mrtdebug)
		    log(LOG_DEBUG,"add_mfc o %x g %x m %x p %x dbg %x\n",
			ntohl(mfccp->mfcc_origin.s_addr),
			ntohl(mfccp->mfcc_mcastgrp.s_addr),
			ntohl(mfccp->mfcc_originmask.s_addr),
			mfccp->mfcc_parent, mb_rt->m_act);

		rt->mfc_origin     = mfccp->mfcc_origin;
		rt->mfc_originmask = mfccp->mfcc_originmask;
		rt->mfc_mcastgrp   = mfccp->mfcc_mcastgrp;
		rt->mfc_parent     = mfccp->mfcc_parent;
		for (i = 0; i < numvifs; i++)
		    VIFM_COPY(mfccp->mfcc_ttls[i], rt->mfc_ttls[i]);
		/* initialize pkt counters per src-grp */
		rt->mfc_pkt_cnt    = 0;
		rt1 = rt;
	    }

	    /* prevent cleanup of cache entry */
	    untimeout(cleanup_cache, (caddr_t)mb_rt);
	    timeout_val--;

	    /* free packets Qed at the end of this entry */
	    while (mb_rt->m_act) {
		mb_ntry = mb_rt->m_act;
		rte = mtod(mb_ntry, struct rtdetq *);
		ip_mdq(rte->m, rte->ifp, rte->tunnel_src, 
		       rt1, rte->imo);
		mb_rt->m_act = mb_ntry->m_act;
		m_freem(rte->m);
		m_free(mb_ntry);
	    }

	    /* 
	     * If more than one entry was created for a single upcall
	     * delete that entry
	     */
	    if (nstl > 1) {
		MFREE(mb_rt, prev_mb_rt->m_next);
		mb_rt = prev_mb_rt;
	    }
	}
    }

    /*
     * It is possible that an entry is being inserted without an upcall
     */
    if (nstl == 0) {
	if (mrtdebug)
	    log(LOG_DEBUG,"add_mfc no upcall h %d o %x g %x m %x p %x\n",
		hash, ntohl(mfccp->mfcc_origin.s_addr),
		ntohl(mfccp->mfcc_mcastgrp.s_addr),
		ntohl(mfccp->mfcc_originmask.s_addr),
		mfccp->mfcc_parent);
	
	for (prev_mb_rt = mb_rt = mfctable[hash];
	     mb_rt; prev_mb_rt = mb_rt, mb_rt = mb_rt->m_next) {
	    
	    rt = mtod(mb_rt, struct mfc *);
	    if (((rt->mfc_origin.s_addr & mfccp->mfcc_originmask.s_addr) 
		 == mfccp->mfcc_origin.s_addr) &&
		(rt->mfc_mcastgrp.s_addr == mfccp->mfcc_mcastgrp.s_addr)) {

		rt->mfc_origin     = mfccp->mfcc_origin;
		rt->mfc_originmask = mfccp->mfcc_originmask;
		rt->mfc_mcastgrp   = mfccp->mfcc_mcastgrp;
		rt->mfc_parent     = mfccp->mfcc_parent;
		for (i = 0; i < numvifs; i++)
		    VIFM_COPY(mfccp->mfcc_ttls[i], rt->mfc_ttls[i]);
		/* initialize pkt counters per src-grp */
		rt->mfc_pkt_cnt    = 0;
	    }
	}
	if (mb_rt == NULL) {
	    /* no upcall, so make a new entry */
	    MGET(mb_rt, M_DONTWAIT, MT_MRTABLE);
	    if (mb_rt == NULL) {
		splx(s);
		return ENOBUFS;
	    }
	    
	    rt = mtod(mb_rt, struct mfc *);
	    
	    /* insert new entry at head of hash chain */
	    rt->mfc_origin     = mfccp->mfcc_origin;
	    rt->mfc_originmask = mfccp->mfcc_originmask;
	    rt->mfc_mcastgrp   = mfccp->mfcc_mcastgrp;
	    rt->mfc_parent     = mfccp->mfcc_parent;
	    for (i = 0; i < numvifs; i++)
		VIFM_COPY(mfccp->mfcc_ttls[i], rt->mfc_ttls[i]);
	    /* initialize pkt counters per src-grp */
	    rt->mfc_pkt_cnt    = 0;
	    
	    /* link into table */
	    mb_rt->m_next  = mfctable[hash];
	    mfctable[hash] = mb_rt;
	    mb_rt->m_act = NULL;
	}
    }
    splx(s);
    return 0;
}

/*
 * Delete an mfc entry
 */
static int
del_mfc(mfccp)
    struct delmfcctl *mfccp;
{
    struct in_addr 	origin;
    struct in_addr 	mcastgrp;
    struct mfc 		*rt;
    struct mbuf 	*mb_rt;
    struct mbuf 	*prev_mb_rt;
    u_long 		hash;
    struct mfc 		**cmfc;
    struct mfc 		**cmfcend;
    int s;

    origin = mfccp->mfcc_origin;
    mcastgrp = mfccp->mfcc_mcastgrp;
    hash = nethash_fc(origin.s_addr, mcastgrp.s_addr);

    if (mrtdebug)
	log(LOG_DEBUG,"del_mfc orig %x mcastgrp %x\n",
	    ntohl(origin.s_addr), ntohl(mcastgrp.s_addr));

    for (prev_mb_rt = mb_rt = mfctable[hash]
	 ; mb_rt
	 ; prev_mb_rt = mb_rt, mb_rt = mb_rt->m_next) {
        rt = mtod(mb_rt, struct mfc *);
	if (origin.s_addr == rt->mfc_origin.s_addr &&
	    mcastgrp.s_addr == rt->mfc_mcastgrp.s_addr &&
	    mb_rt->m_act == NULL)
	    break;
    }
    if (mb_rt == NULL) {
	return ESRCH;
    }

    s = splnet();

    cmfc = mfchash;
    cmfcend = cmfc + MFCHASHSIZ;
    for ( ; cmfc < cmfcend; ++cmfc)
	if (*cmfc == rt)
	    *cmfc = 0;

    if (prev_mb_rt != mb_rt) {	/* if moved past head of list */
	MFREE(mb_rt, prev_mb_rt->m_next);
    } else			/* delete head of list, it is in the table */
        mfctable[hash] = m_free(mb_rt);

    splx(s);

    return 0;
}

/*
 * IP multicast forwarding function. This function assumes that the packet
 * pointed to by "ip" has arrived on (or is about to be sent to) the interface
 * pointed to by "ifp", and the packet is to be relayed to other networks
 * that have members of the packet's destination IP multicast group.
 *
 * The packet is returned unscathed to the caller, unless it is tunneled
 * or erroneous, in which case a non-zero return value tells the caller to
 * discard it.
 */

#define IP_HDR_LEN  20	/* # bytes of fixed IP header (excluding options) */
#define TUNNEL_LEN  12  /* # bytes of IP option for tunnel encapsulation  */

int
X_ip_mforward(ip, ifp, m, imo)
    register struct ip *ip;
    struct ifnet *ifp;
    struct mbuf *m;
    struct ip_moptions *imo;
{
    register struct mfc *rt;
    register u_char *ipoptions;
    u_long tunnel_src;
    static struct sockproto	k_igmpproto 	= { AF_INET, IPPROTO_IGMP };
    static struct sockaddr_in 	k_igmpsrc	= { sizeof k_igmpsrc, AF_INET };
    static struct sockaddr_in 	k_igmpdst 	= { sizeof k_igmpdst, AF_INET };
    register struct mbuf *mm;
    register struct ip *k_data;
    int s;

    if (mrtdebug > 1)
      log(LOG_DEBUG, "ip_mforward: src %x, dst %x, ifp %x (%s%d)\n",
          ntohl(ip->ip_src.s_addr), ntohl(ip->ip_dst.s_addr), ifp,
          ifp->if_name, ifp->if_unit);

    if (ip->ip_hl < (IP_HDR_LEN + TUNNEL_LEN) >> 2 ||
	(ipoptions = (u_char *)(ip + 1))[1] != IPOPT_LSRR ) {
	/*
	 * Packet arrived via a physical interface.
	 */
	tunnel_src = 0;
    } else {
	/*
	 * Packet arrived through a source-route tunnel.
	 *
	 * A source-route tunneled packet has a single NOP option and a
	 * two-element
	 * loose-source-and-record-route (LSRR) option immediately following
	 * the fixed-size part of the IP header.  At this point in processing,
	 * the IP header should contain the following IP addresses:
	 *
	 *	original source          - in the source address field
	 *	destination group        - in the destination address field
	 *	remote tunnel end-point  - in the first  element of LSRR
	 *	one of this host's addrs - in the second element of LSRR
	 *
	 * NOTE: RFC-1075 would have the original source and remote tunnel
	 *	 end-point addresses swapped.  However, that could cause
	 *	 delivery of ICMP error messages to innocent applications
	 *	 on intermediate routing hosts!  Therefore, we hereby
	 *	 change the spec.
	 */
	
	/*
	 * Verify that the tunnel options are well-formed.
	 */
	if (ipoptions[0] != IPOPT_NOP ||
	    ipoptions[2] != 11 ||	/* LSRR option length   */
	    ipoptions[3] != 12 ||	/* LSRR address pointer */
	    (tunnel_src = *(u_long *)(&ipoptions[4])) == 0) {
	    mrtstat.mrts_bad_tunnel++;
	    if (mrtdebug)
		log(LOG_DEBUG,
		    "ip_mforward: bad tunnel from %u (%x %x %x %x %x %x)\n",
		    ntohl(ip->ip_src.s_addr),
		    ipoptions[0], ipoptions[1], ipoptions[2], ipoptions[3],
		    *(u_long *)(&ipoptions[4]), *(u_long *)(&ipoptions[8]));
	    return 1;
	}

	/*
	 * Delete the tunnel options from the packet.
	 */
	ovbcopy((caddr_t)(ipoptions + TUNNEL_LEN), (caddr_t)ipoptions,
		(unsigned)(m->m_len - (IP_HDR_LEN + TUNNEL_LEN)));
	m->m_len   -= TUNNEL_LEN;
	ip->ip_len -= TUNNEL_LEN;
	ip->ip_hl  -= TUNNEL_LEN >> 2;

	ifp = 0;
    }

    /*
     * Don't forward a packet with time-to-live of zero or one,
     * or a packet destined to a local-only group.
     */
    if (ip->ip_ttl <= 1 ||
	ntohl(ip->ip_dst.s_addr) <= INADDR_MAX_LOCAL_GROUP)
	return (int)tunnel_src;

    /*
     * Determine forwarding vifs from the forwarding cache table
     */
    s = splnet();
    MFCFIND(ip->ip_src.s_addr, ip->ip_dst.s_addr, rt);

    /* Entry exists, so forward if necessary */
    if (rt != NULL) {
	splx(s);
	return (ip_mdq(m, ifp, tunnel_src, rt, imo));
    }

    else {
	/*
	 * If we don't have a route for packet's origin,
	 * Make a copy of the packet &
	 * send message to routing daemon
	 */

	register struct mbuf *mb_rt;
	register struct mbuf *mb_ntry;
	register struct mbuf *mb0;
	register struct rtdetq *rte;
	register struct mbuf *rte_m;
	register u_long hash;

	mrtstat.mrts_no_route++;
	if (mrtdebug)
	    log(LOG_DEBUG, "ip_mforward: no rte s %x g %x\n",
		ntohl(ip->ip_src.s_addr),
		ntohl(ip->ip_dst.s_addr));

	/* is there an upcall waiting for this packet? */
	hash = nethash_fc(ip->ip_src.s_addr, ip->ip_dst.s_addr);
	for (mb_rt = mfctable[hash]; mb_rt; mb_rt = mb_rt->m_next) {
	    rt = mtod(mb_rt, struct mfc *);
	    if (((ip->ip_src.s_addr & rt->mfc_originmask.s_addr) == 
		 rt->mfc_origin.s_addr) &&
		(ip->ip_dst.s_addr == rt->mfc_mcastgrp.s_addr) &&
		(mb_rt->m_act != NULL))
		break;
	}

	if (mb_rt == NULL) {
	    /* no upcall, so make a new entry */
	    MGET(mb_rt, M_DONTWAIT, MT_MRTABLE);
	    if (mb_rt == NULL) {
		splx(s);
		return ENOBUFS;
	    }

	    rt = mtod(mb_rt, struct mfc *);

	    /* insert new entry at head of hash chain */
	    rt->mfc_origin.s_addr     = ip->ip_src.s_addr;
	    rt->mfc_originmask.s_addr = (u_long)0xffffffff;
	    rt->mfc_mcastgrp.s_addr   = ip->ip_dst.s_addr;

	    /* link into table */
	    hash = nethash_fc(rt->mfc_origin.s_addr, rt->mfc_mcastgrp.s_addr);
	    mb_rt->m_next  = mfctable[hash];
	    mfctable[hash] = mb_rt;
	    mb_rt->m_act = NULL;

	}

	/* determine if q has overflowed */
	for (rte_m = mb_rt, hash = 0; rte_m->m_act; rte_m = rte_m->m_act)
	    hash++;

	if (hash > MAX_UPQ) {
	    mrtstat.mrts_upq_ovflw++;
	    splx(s);
	    return 0;
	}

	/* add this packet and timing, ifp info to m_act */
	MGET(mb_ntry, M_DONTWAIT, MT_DATA);
	if (mb_ntry == NULL) {
	    splx(s);
	    return ENOBUFS;
	}

	mb_ntry->m_act = NULL;
	rte = mtod(mb_ntry, struct rtdetq *);

	mb0 = m_copy(m, 0, M_COPYALL);
	if (mb0 == NULL) {
	    splx(s);
	    return ENOBUFS;
	}

	rte->m 			= mb0;
	rte->ifp 		= ifp;
	rte->tunnel_src 	= tunnel_src;
	rte->imo		= imo;

	rte_m->m_act = mb_ntry;

	splx(s);

	if (hash == 0) {
	    /* 
	     * Send message to routing daemon to install 
	     * a route into the kernel table
	     */
	    k_igmpsrc.sin_addr = ip->ip_src;
	    k_igmpdst.sin_addr = ip->ip_dst;
	    
	    mm = m_copy(m, 0, M_COPYALL);
	    if (mm == NULL) {
		splx(s);
		return ENOBUFS;
	    }
	    
	    k_data = mtod(mm, struct ip *);
	    k_data->ip_p = 0;
	    
	    mrtstat.mrts_upcalls++;

          rip_ip_input(mm, ip_mrouter, (struct sockaddr *)&k_igmpsrc);
	    
	    /* set timer to cleanup entry if upcall is lost */
	    timeout(cleanup_cache, (caddr_t)mb_rt, 100);
	    timeout_val++;
	}
	
	return 0;
    }		
}

#ifndef MROUTE_LKM
int (*ip_mforward)(struct ip *, struct ifnet *, struct mbuf *,
		   struct ip_moptions *) = X_ip_mforward;
#endif

/*
 * Clean up the cache entry if upcall is not serviced
 */
static void
cleanup_cache(xmb_rt)
	void *xmb_rt;
{
    struct mbuf *mb_rt = xmb_rt;
    struct mfc *rt;
    u_long hash;
    struct mbuf *prev_m0;
    struct mbuf *m0;
    struct mbuf *m;
    struct rtdetq *rte;
    int s;

    rt = mtod(mb_rt, struct mfc *);
    hash = nethash_fc(rt->mfc_origin.s_addr, rt->mfc_mcastgrp.s_addr);

    if (mrtdebug)
	log(LOG_DEBUG, "ip_mforward: cleanup ipm %d h %d s %x g %x\n", 
	    ip_mrouter, hash, ntohl(rt->mfc_origin.s_addr), 
	    ntohl(rt->mfc_mcastgrp.s_addr));

    mrtstat.mrts_cache_cleanups++;

    /*
     * determine entry to be cleaned up in cache table
     */
    s = splnet();
    for (prev_m0 = m0 = mfctable[hash]; m0; prev_m0 = m0, m0 = m0->m_next)
	if (m0 == mb_rt)
	    break;

    /* 
     * drop all the packets
     * free the mbuf with the pkt, if, timing info
     */
    while (mb_rt->m_act) {
	m = mb_rt->m_act;
	mb_rt->m_act = m->m_act;

	rte = mtod(m, struct rtdetq *);
	m_freem(rte->m);
	m_free(m);
    }

    /* 
     * Delete the entry from the cache
     */
    if (prev_m0 != m0) {	/* if moved past head of list */
	MFREE(m0, prev_m0->m_next);
    } else			/* delete head of list, it is in the table */
	mfctable[hash] = m_free(m0);
    
    timeout_val--;
    splx(s);
}

/*
 * Packet forwarding routine once entry in the cache is made
 */
static int
ip_mdq(m, ifp, tunnel_src, rt, imo)
    register struct mbuf *m;
    register struct ifnet *ifp;
    register u_long tunnel_src;
    register struct mfc *rt;
    register struct ip_moptions *imo;
{
    register struct ip  *ip = mtod(m, struct ip *);
    register vifi_t vifi;
    register struct vif *vifp;

    /*
     * Don't forward if it didn't arrive from the parent vif for its origin.
     * Notes: v_ifp is zero for src route tunnels, multicast_decap_if
     * for encapsulated tunnels and a real ifnet for non-tunnels so
     * the first part of the if catches wrong physical interface or
     * tunnel type; v_rmt_addr is zero for non-tunneled packets so
     * the 2nd part catches both packets that arrive via a tunnel
     * that shouldn't and packets that arrive via the wrong tunnel.
     */
    vifi = rt->mfc_parent;
    if (viftable[vifi].v_ifp != ifp ||
	(ifp == 0 && viftable[vifi].v_rmt_addr.s_addr != tunnel_src)) {
	/* came in the wrong interface */
	if (mrtdebug)
	    log(LOG_DEBUG, "wrong if: ifp %x vifi %d\n",
		ifp, vifi); 
	++mrtstat.mrts_wrong_if;
	return (int)tunnel_src;
    }

    /* increment the interface and s-g counters */
    viftable[vifi].v_pkt_in++;
    rt->mfc_pkt_cnt++;

    /*
     * For each vif, decide if a copy of the packet should be forwarded.
     * Forward if:
     *		- the ttl exceeds the vif's threshold
     *		- there are group members downstream on interface
     */
#define MC_SEND(ip,vifp,m) {                             \
		(vifp)->v_pkt_out++;                     \
                if ((vifp)->v_flags & VIFF_SRCRT)        \
                    srcrt_send((ip), (vifp), (m));       \
                else if ((vifp)->v_flags & VIFF_TUNNEL)  \
                    encap_send((ip), (vifp), (m));       \
                else                                     \
                    phyint_send((ip), (vifp), (m));      \
                }                                  

/* If no options or the imo_multicast_vif option is 0, don't do this part 
 */
    if ((imo != NULL) && 
       (( vifi = imo->imo_multicast_vif - 1) < numvifs) /*&& (vifi>=0)*/) 
    {  
        MC_SEND(ip,viftable+vifi,m);
        return (1);        /* make sure we are done: No more physical sends */
    }

    for (vifp = viftable, vifi = 0; vifi < numvifs; vifp++, vifi++)
	if ((rt->mfc_ttls[vifi] > 0) &&
	    (ip->ip_ttl > rt->mfc_ttls[vifi]))
	    MC_SEND(ip, vifp, m);

    return 0;
}

/* check if a vif number is legal/ok. This is used by ip_output, to export
 * numvifs there, 
 */
int
X_legal_vif_num(vif)
    int vif;
{   if (vif>=0 && vif<=numvifs)
       return(1);
    else
       return(0);
}

#ifndef MROUTE_LKM
int (*legal_vif_num)(int) = X_legal_vif_num;
#endif

static void
phyint_send(ip, vifp, m)
    struct ip *ip;
    struct vif *vifp;
    struct mbuf *m;
{
    register struct mbuf *mb_copy;
    int hlen = ip->ip_hl << 2;
    register struct ip_moptions *imo;

    if ((mb_copy = m_copy(m, 0, M_COPYALL)) == NULL)
	return;

    /*
     * Make sure the header isn't in an cluster, because the sharing
     * in clusters defeats the whole purpose of making the copy above.
     */
    mb_copy = m_pullup(mb_copy, hlen);
    if (mb_copy == NULL)
	    return;

    MALLOC(imo, struct ip_moptions *, sizeof *imo, M_IPMOPTS, M_NOWAIT);
    if (imo == NULL) {
	m_freem(mb_copy);
	return;
    }

    imo->imo_multicast_ifp  = vifp->v_ifp;
    imo->imo_multicast_ttl  = ip->ip_ttl - 1;
    imo->imo_multicast_loop = 1;

    if (vifp->v_rate_limit <= 0)
	tbf_send_packet(vifp, mb_copy, imo);
    else
	tbf_control(vifp, mb_copy, mtod(mb_copy, struct ip *), ip->ip_len,
		    imo);
}

static void
srcrt_send(ip, vifp, m)
    struct ip *ip;
    struct vif *vifp;
    struct mbuf *m;
{
    struct mbuf *mb_copy, *mb_opts;
    int hlen = ip->ip_hl << 2;
    register struct ip *ip_copy;
    u_char *cp;

    /*
     * Make sure that adding the tunnel options won't exceed the
     * maximum allowed number of option bytes.
     */
    if (ip->ip_hl > (60 - TUNNEL_LEN) >> 2) {
	mrtstat.mrts_cant_tunnel++;
	if (mrtdebug)
	    log(LOG_DEBUG, "srcrt_send: no room for tunnel options, from %u\n",
		ntohl(ip->ip_src.s_addr));
	return;
    }

    if ((mb_copy = m_copy(m, 0, M_COPYALL)) == NULL)
	return;

    MGETHDR(mb_opts, M_DONTWAIT, MT_HEADER);
    if (mb_opts == NULL) {
	m_freem(mb_copy);
	return;
    }
    /*
     * 'Delete' the base ip header from the mb_copy chain
     */
    mb_copy->m_len -= hlen;
    mb_copy->m_data += hlen;
    /*
     * Make mb_opts be the new head of the packet chain.
     * Any options of the packet were left in the old packet chain head
     */
    mb_opts->m_next = mb_copy;
    mb_opts->m_len = hlen + TUNNEL_LEN;
    mb_opts->m_data += MSIZE - mb_opts->m_len;
    mb_opts->m_pkthdr.len = mb_copy->m_pkthdr.len + TUNNEL_LEN;
    /*
     * Copy the base ip header from the mb_copy chain to the new head mbuf
     */
    ip_copy = mtod(mb_opts, struct ip *);
    bcopy((caddr_t)ip_copy, mtod(mb_opts, caddr_t), hlen);
    ip_copy->ip_ttl--;
    ip_copy->ip_dst = vifp->v_rmt_addr;	  /* remote tunnel end-point */
    /*
     * Adjust the ip header length to account for the tunnel options.
     */
    ip_copy->ip_hl  += TUNNEL_LEN >> 2;
    ip_copy->ip_len += TUNNEL_LEN;
    /*
     * Add the NOP and LSRR after the base ip header
     */
    cp = mtod(mb_opts, u_char *) + IP_HDR_LEN;
    *cp++ = IPOPT_NOP;
    *cp++ = IPOPT_LSRR;
    *cp++ = 11; /* LSRR option length */
    *cp++ = 8;  /* LSSR pointer to second element */
    *(u_long*)cp = vifp->v_lcl_addr.s_addr;	/* local tunnel end-point */
    cp += 4;
    *(u_long*)cp = ip->ip_dst.s_addr;		/* destination group */

    if (vifp->v_rate_limit <= 0)
	tbf_send_packet(vifp, mb_opts, 0);
    else
	tbf_control(vifp, mb_opts, 
		    mtod(mb_opts, struct ip *), ip_copy->ip_len, 0);
}

static void
encap_send(ip, vifp, m)
    register struct ip *ip;
    register struct vif *vifp;
    register struct mbuf *m;
{
    register struct mbuf *mb_copy;
    register struct ip *ip_copy;
    int hlen = ip->ip_hl << 2;
    register int i, len = ip->ip_len;

    /*
     * copy the old packet & pullup it's IP header into the
     * new mbuf so we can modify it.  Try to fill the new
     * mbuf since if we don't the ethernet driver will.
     */
    MGET(mb_copy, M_DONTWAIT, MT_DATA);
    if (mb_copy == NULL)
	return;
    mb_copy->m_data += 16;
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
    ip_copy->ip_id = htons(ip_id++);
    ip_copy->ip_len += len;
    ip_copy->ip_src = vifp->v_lcl_addr;
    ip_copy->ip_dst = vifp->v_rmt_addr;

    /*
     * turn the encapsulated IP header back into a valid one.
     */
    ip = (struct ip *)((caddr_t)ip_copy + sizeof(multicast_encap_iphdr));
    --ip->ip_ttl;
    HTONS(ip->ip_len);
    HTONS(ip->ip_off);
    ip->ip_sum = 0;
#if defined(LBL) && !defined(ultrix)
    ip->ip_sum = ~oc_cksum((caddr_t)ip, ip->ip_hl << 2, 0);
#else
    mb_copy->m_data += sizeof(multicast_encap_iphdr);
    ip->ip_sum = in_cksum(mb_copy, ip->ip_hl << 2);
    mb_copy->m_data -= sizeof(multicast_encap_iphdr);
#endif

    if (vifp->v_rate_limit <= 0)
	tbf_send_packet(vifp, mb_copy, 0);
    else
	tbf_control(vifp, mb_copy, ip, ip_copy->ip_len, 0);
}

/*
 * De-encapsulate a packet and feed it back through ip input (this
 * routine is called whenever IP gets a packet with proto type
 * ENCAP_PROTO and a local destination address).
 */
void
#ifdef MROUTE_LKM
X_multiencap_decap(m)
#else
multiencap_decap(m)
#endif
    register struct mbuf *m;
{
    struct ifnet *ifp = m->m_pkthdr.rcvif;
    register struct ip *ip = mtod(m, struct ip *);
    register int hlen = ip->ip_hl << 2;
    register int s;
    register struct ifqueue *ifq;
    register struct vif *vifp;

    if (ip->ip_p != ENCAP_PROTO) {
    	rip_input(m);
	return;
    }
    /*
     * dump the packet if it's not to a multicast destination or if
     * we don't have an encapsulating tunnel with the source.
     * Note:  This code assumes that the remote site IP address
     * uniquely identifies the tunnel (i.e., that this site has
     * at most one tunnel with the remote site).
     */
    if (! IN_MULTICAST(ntohl(((struct ip *)((char *)ip + hlen))->ip_dst.s_addr))) {
	++mrtstat.mrts_bad_tunnel;
	m_freem(m);
	return;
    }
    if (ip->ip_src.s_addr != last_encap_src) {
	register struct vif *vife;
	
	vifp = viftable;
	vife = vifp + numvifs;
	last_encap_src = ip->ip_src.s_addr;
	last_encap_vif = 0;
	for ( ; vifp < vife; ++vifp)
	    if (vifp->v_rmt_addr.s_addr == ip->ip_src.s_addr) {
		if ((vifp->v_flags & (VIFF_TUNNEL|VIFF_SRCRT))
		    == VIFF_TUNNEL)
		    last_encap_vif = vifp;
		break;
	    }
    }
    if ((vifp = last_encap_vif) == 0) {
	last_encap_src = 0;
	mrtstat.mrts_cant_tunnel++; /*XXX*/
	m_freem(m);
	if (mrtdebug)
          log(LOG_DEBUG, "ip_mforward: no tunnel with %x\n",
		ntohl(ip->ip_src.s_addr));
	return;
    }
    ifp = vifp->v_ifp;

    if (hlen > IP_HDR_LEN)
      ip_stripoptions(m, (struct mbuf *) 0);
    m->m_data += IP_HDR_LEN;
    m->m_len -= IP_HDR_LEN;
    m->m_pkthdr.len -= IP_HDR_LEN;
    m->m_pkthdr.rcvif = ifp;

    ifq = &ipintrq;
    s = splimp();
    if (IF_QFULL(ifq)) {
	IF_DROP(ifq);
	m_freem(m);
    } else {
	IF_ENQUEUE(ifq, m);
	/*
	 * normally we would need a "schednetisr(NETISR_IP)"
	 * here but we were called by ip_input and it is going
	 * to loop back & try to dequeue the packet we just
	 * queued as soon as we return so we avoid the
	 * unnecessary software interrrupt.
	 */
    }
    splx(s);
}

/*
 * Token bucket filter module
 */
void
tbf_control(vifp, m, ip, p_len, imo)
	register struct vif *vifp;
	register struct mbuf *m;
	register struct ip *ip;
	register u_long p_len;
	struct ip_moptions *imo;
{
    tbf_update_tokens(vifp);

    /* if there are enough tokens, 
     * and the queue is empty,
     * send this packet out
     */

    if (vifp->v_tbf->q_len == 0) {
	if (p_len <= vifp->v_tbf->n_tok) {
	    vifp->v_tbf->n_tok -= p_len;
	    tbf_send_packet(vifp, m, imo);
	} else if (p_len > MAX_BKT_SIZE) {
	    /* drop if packet is too large */
	    mrtstat.mrts_pkt2large++;
	    m_freem(m);
	    return;
	} else {
	    /* queue packet and timeout till later */
	    tbf_queue(vifp, m, ip, imo);
	    timeout(tbf_reprocess_q, (caddr_t)vifp, 1);
	}
    } else if (vifp->v_tbf->q_len < MAXQSIZE) {
	/* finite queue length, so queue pkts and process queue */
	tbf_queue(vifp, m, ip, imo);
	tbf_process_q(vifp);
    } else {
	/* queue length too much, try to dq and queue and process */
	if (!tbf_dq_sel(vifp, ip)) {
	    mrtstat.mrts_q_overflow++;
	    m_freem(m);
	    return;
	} else {
	    tbf_queue(vifp, m, ip, imo);
	    tbf_process_q(vifp);
	}
    }
    return;
}

/* 
 * adds a packet to the queue at the interface
 */
void
tbf_queue(vifp, m, ip, imo) 
	register struct vif *vifp;
	register struct mbuf *m;
	register struct ip *ip;
	struct ip_moptions *imo;
{
    register u_long ql;
    register int index = (vifp - viftable);
    register int s = splnet();

    ql = vifp->v_tbf->q_len;

    qtable[index][ql].pkt_m = m;
    qtable[index][ql].pkt_len = (mtod(m, struct ip *))->ip_len;
    qtable[index][ql].pkt_ip = ip;
    qtable[index][ql].pkt_imo = imo;

    vifp->v_tbf->q_len++;
    splx(s);
}


/* 
 * processes the queue at the interface
 */
void
tbf_process_q(vifp)
    register struct vif *vifp;
{
    register struct pkt_queue pkt_1;
    register int index = (vifp - viftable);
    register int s = splnet();

    /* loop through the queue at the interface and send as many packets
     * as possible
     */
    while (vifp->v_tbf->q_len > 0) {
	/* locate the first packet */
	pkt_1.pkt_len = ((qtable[index][0]).pkt_len);
	pkt_1.pkt_m   = (qtable[index][0]).pkt_m;
	pkt_1.pkt_ip   = (qtable[index][0]).pkt_ip;
	pkt_1.pkt_imo = (qtable[index][0]).pkt_imo;

	/* determine if the packet can be sent */
	if (pkt_1.pkt_len <= vifp->v_tbf->n_tok) {
	    /* if so,
	     * reduce no of tokens, dequeue the queue,
	     * send the packet.
	     */
	    vifp->v_tbf->n_tok -= pkt_1.pkt_len;

	    tbf_dequeue(vifp, 0);

	    tbf_send_packet(vifp, pkt_1.pkt_m, pkt_1.pkt_imo);

	} else break;
    }
    splx(s);
}

/* 
 * removes the jth packet from the queue at the interface
 */
void
tbf_dequeue(vifp,j) 
    register struct vif *vifp;
    register int j;
{
    register u_long index = vifp - viftable;
    register int i;

    for (i=j+1; i <= vifp->v_tbf->q_len - 1; i++) {
	qtable[index][i-1].pkt_m   = qtable[index][i].pkt_m;
	qtable[index][i-1].pkt_len = qtable[index][i].pkt_len;
	qtable[index][i-1].pkt_ip = qtable[index][i].pkt_ip;
	qtable[index][i-1].pkt_imo = qtable[index][i].pkt_imo;
    }		
    qtable[index][i-1].pkt_m = NULL;
    qtable[index][i-1].pkt_len = NULL;
    qtable[index][i-1].pkt_ip = NULL;
    qtable[index][i-1].pkt_imo = NULL;

    vifp->v_tbf->q_len--;

    if (tbfdebug > 1)
	log(LOG_DEBUG, "tbf_dequeue: vif# %d qlen %d\n",vifp-viftable, i-1);
}

void
tbf_reprocess_q(xvifp)
	void *xvifp;
{
    register struct vif *vifp = xvifp;
    if (ip_mrouter == NULL) 
	return;

    tbf_update_tokens(vifp);

    tbf_process_q(vifp);

    if (vifp->v_tbf->q_len)
	timeout(tbf_reprocess_q, (caddr_t)vifp, 1);
}

/* function that will selectively discard a member of the queue
 * based on the precedence value and the priority obtained through
 * a lookup table - not yet implemented accurately!
 */
int
tbf_dq_sel(vifp, ip)
    register struct vif *vifp;
    register struct ip *ip;
{
    register int i;
    register int s = splnet();
    register u_int p;

    p = priority(vifp, ip);

    for(i=vifp->v_tbf->q_len-1;i >= 0;i--) {
	if (p > priority(vifp, qtable[vifp-viftable][i].pkt_ip)) {
	    m_freem(qtable[vifp-viftable][i].pkt_m);
	    tbf_dequeue(vifp,i);
	    splx(s);
	    mrtstat.mrts_drop_sel++;
	    return(1);
	}
    }
    splx(s);
    return(0);
}

void
tbf_send_packet(vifp, m, imo)
    register struct vif *vifp;
    register struct mbuf *m;
    struct ip_moptions *imo;
{
    int error;
    int s = splnet();

    /* if source route tunnels */
    if (vifp->v_flags & VIFF_SRCRT) {
	error = ip_output(m, (struct mbuf *)0, (struct route *)0,
			  IP_FORWARDING, imo);
	if (mrtdebug > 1)
	    log(LOG_DEBUG, "srcrt_send on vif %d err %d\n", vifp-viftable, error);
    } else if (vifp->v_flags & VIFF_TUNNEL) {
	/* If tunnel options */
	ip_output(m, (struct mbuf *)0, (struct route *)0,
		  IP_FORWARDING, imo);
    } else {
	/* if physical interface option, extract the options and then send */
	error = ip_output(m, (struct mbuf *)0, (struct route *)0,
			  IP_FORWARDING, imo);
	FREE(imo, M_IPMOPTS);

	if (mrtdebug > 1)
	    log(LOG_DEBUG, "phyint_send on vif %d err %d\n", vifp-viftable, error);
    }
    splx(s);
}

/* determine the current time and then
 * the elapsed time (between the last time and time now)
 * in milliseconds & update the no. of tokens in the bucket
 */
void
tbf_update_tokens(vifp)
    register struct vif *vifp;
{
    struct timeval tp;
    register u_long t;
    register u_long elapsed;
    register int s = splnet();

    GET_TIME(tp);

    t = tp.tv_sec*1000 + tp.tv_usec/1000;

    elapsed = (t - vifp->v_tbf->last_pkt_t) * vifp->v_rate_limit /8;
    vifp->v_tbf->n_tok += elapsed;
    vifp->v_tbf->last_pkt_t = t;

    if (vifp->v_tbf->n_tok > MAX_BKT_SIZE)
	vifp->v_tbf->n_tok = MAX_BKT_SIZE;

    splx(s);
}

static int
priority(vifp, ip)
    register struct vif *vifp;
    register struct ip *ip;
{
    register u_long graddr;
    register int prio;

    /* temporary hack; will add general packet classifier some day */

    prio = 50;  /* default priority */
    
    /* check for source route options and add option length to get dst */
    if (vifp->v_flags & VIFF_SRCRT)
	graddr = ntohl((ip+8)->ip_dst.s_addr);
    else
	graddr = ntohl(ip->ip_dst.s_addr);

    switch (graddr & 0xf) {
	case 0x0: break;
	case 0x1: if (graddr == 0xe0020001) prio = 65; /* MBone Audio */
		  break;
	case 0x2: break;
	case 0x3: break;
	case 0x4: break;
	case 0x5: break;
	case 0x6: break;
	case 0x7: break;
	case 0x8: break;
	case 0x9: break;
	case 0xa: if (graddr == 0xe000010a) prio = 85; /* IETF Low Audio 1 */
		  break;
	case 0xb: if (graddr == 0xe000010b) prio = 75; /* IETF Audio 1 */
		  break;
	case 0xc: if (graddr == 0xe000010c) prio = 60; /* IETF Video 1 */
		  break;
	case 0xd: if (graddr == 0xe000010d) prio = 80; /* IETF Low Audio 2 */
		  break;
	case 0xe: if (graddr == 0xe000010e) prio = 70; /* IETF Audio 2 */
		  break;
	case 0xf: if (graddr == 0xe000010f) prio = 55; /* IETF Video 2 */
		  break;
    }

    if (tbfdebug > 1) log(LOG_DEBUG, "graddr%x prio%d\n", graddr, prio);

    return prio;
}

/*
 * End of token bucket filter modifications 
 */

#ifdef MROUTE_LKM
#include <sys/conf.h>
#include <sys/exec.h>
#include <sys/sysent.h>
#include <sys/lkm.h>

MOD_MISC("ip_mroute_mod")

static int
ip_mroute_mod_handle(struct lkm_table *lkmtp, int cmd)
{
	int i;
	struct lkm_misc	*args = lkmtp->private.lkm_misc;
	int err = 0;

	switch(cmd) {
		static int (*old_ip_mrouter_cmd)();
		static int (*old_ip_mrouter_done)();
		static int (*old_ip_mforward)();
		static int (*old_mrt_ioctl)();
		static void (*old_proto4_input)();
		static int (*old_legal_vif_num)();
		extern struct protosw inetsw[];

	case LKM_E_LOAD:
		if(lkmexists(lkmtp) || ip_mrtproto)
		  return(EEXIST);
		old_ip_mrouter_cmd = ip_mrouter_cmd;
		ip_mrouter_cmd = X_ip_mrouter_cmd;
		old_ip_mrouter_done = ip_mrouter_done;
		ip_mrouter_done = X_ip_mrouter_done;
		old_ip_mforward = ip_mforward;
		ip_mforward = X_ip_mforward;
		old_mrt_ioctl = mrt_ioctl;
		mrt_ioctl = X_mrt_ioctl;
              old_proto4_input = inetsw[ip_protox[ENCAP_PROTO]].pr_input;
              inetsw[ip_protox[ENCAP_PROTO]].pr_input = X_multiencap_decap;
		old_legal_vif_num = legal_vif_num;
		legal_vif_num = X_legal_vif_num;
		ip_mrtproto = IGMP_DVMRP;

		printf("\nIP multicast routing loaded\n");
		break;

	case LKM_E_UNLOAD:
		if (ip_mrouter)
		  return EINVAL;

		ip_mrouter_cmd = old_ip_mrouter_cmd;
		ip_mrouter_done = old_ip_mrouter_done;
		ip_mforward = old_ip_mforward;
		mrt_ioctl = old_mrt_ioctl;
              inetsw[ip_protox[ENCAP_PROTO]].pr_input = old_proto4_input;
		legal_vif_num = old_legal_vif_num;
		ip_mrtproto = 0;
		break;

	default:
		err = EINVAL;
		break;
	}

	return(err);
}

int
ip_mroute_mod(struct lkm_table *lkmtp, int cmd, int ver) {
	DISPATCH(lkmtp, cmd, ver, ip_mroute_mod_handle, ip_mroute_mod_handle,
		 nosys);
}

#endif /* MROUTE_LKM */
#endif /* MROUTING */


