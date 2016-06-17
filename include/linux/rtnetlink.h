#ifndef __LINUX_RTNETLINK_H
#define __LINUX_RTNETLINK_H

#include <linux/netlink.h>

#define RTNL_DEBUG 1


/****
 *		Routing/neighbour discovery messages.
 ****/

/* Types of messages */

#define RTM_BASE	0x10

#define	RTM_NEWLINK	(RTM_BASE+0)
#define	RTM_DELLINK	(RTM_BASE+1)
#define	RTM_GETLINK	(RTM_BASE+2)

#define	RTM_NEWADDR	(RTM_BASE+4)
#define	RTM_DELADDR	(RTM_BASE+5)
#define	RTM_GETADDR	(RTM_BASE+6)

#define	RTM_NEWROUTE	(RTM_BASE+8)
#define	RTM_DELROUTE	(RTM_BASE+9)
#define	RTM_GETROUTE	(RTM_BASE+10)

#define	RTM_NEWNEIGH	(RTM_BASE+12)
#define	RTM_DELNEIGH	(RTM_BASE+13)
#define	RTM_GETNEIGH	(RTM_BASE+14)

#define	RTM_NEWRULE	(RTM_BASE+16)
#define	RTM_DELRULE	(RTM_BASE+17)
#define	RTM_GETRULE	(RTM_BASE+18)

#define	RTM_NEWQDISC	(RTM_BASE+20)
#define	RTM_DELQDISC	(RTM_BASE+21)
#define	RTM_GETQDISC	(RTM_BASE+22)

#define	RTM_NEWTCLASS	(RTM_BASE+24)
#define	RTM_DELTCLASS	(RTM_BASE+25)
#define	RTM_GETTCLASS	(RTM_BASE+26)

#define	RTM_NEWTFILTER	(RTM_BASE+28)
#define	RTM_DELTFILTER	(RTM_BASE+29)
#define	RTM_GETTFILTER	(RTM_BASE+30)

#define	RTM_MAX		(RTM_BASE+31)

/* 
   Generic structure for encapsulation of optional route information.
   It is reminiscent of sockaddr, but with sa_family replaced
   with attribute type.
 */

struct rtattr
{
	unsigned short	rta_len;
	unsigned short	rta_type;
};

/* Macros to handle rtattributes */

#define RTA_ALIGNTO	4
#define RTA_ALIGN(len) ( ((len)+RTA_ALIGNTO-1) & ~(RTA_ALIGNTO-1) )
#define RTA_OK(rta,len) ((len) > 0 && (rta)->rta_len >= sizeof(struct rtattr) && \
			 (rta)->rta_len <= (len))
#define RTA_NEXT(rta,attrlen)	((attrlen) -= RTA_ALIGN((rta)->rta_len), \
				 (struct rtattr*)(((char*)(rta)) + RTA_ALIGN((rta)->rta_len)))
#define RTA_LENGTH(len)	(RTA_ALIGN(sizeof(struct rtattr)) + (len))
#define RTA_SPACE(len)	RTA_ALIGN(RTA_LENGTH(len))
#define RTA_DATA(rta)   ((void*)(((char*)(rta)) + RTA_LENGTH(0)))
#define RTA_PAYLOAD(rta) ((int)((rta)->rta_len) - RTA_LENGTH(0))




/******************************************************************************
 *		Definitions used in routing table administration.
 ****/

struct rtmsg
{
	unsigned char		rtm_family;
	unsigned char		rtm_dst_len;
	unsigned char		rtm_src_len;
	unsigned char		rtm_tos;

	unsigned char		rtm_table;	/* Routing table id */
	unsigned char		rtm_protocol;	/* Routing protocol; see below	*/
	unsigned char		rtm_scope;	/* See below */	
	unsigned char		rtm_type;	/* See below	*/

	unsigned		rtm_flags;
};

/* rtm_type */

enum
{
	RTN_UNSPEC,
	RTN_UNICAST,		/* Gateway or direct route	*/
	RTN_LOCAL,		/* Accept locally		*/
	RTN_BROADCAST,		/* Accept locally as broadcast,
				   send as broadcast */
	RTN_ANYCAST,		/* Accept locally as broadcast,
				   but send as unicast */
	RTN_MULTICAST,		/* Multicast route		*/
	RTN_BLACKHOLE,		/* Drop				*/
	RTN_UNREACHABLE,	/* Destination is unreachable   */
	RTN_PROHIBIT,		/* Administratively prohibited	*/
	RTN_THROW,		/* Not in this table		*/
	RTN_NAT,		/* Translate this address	*/
	RTN_XRESOLVE,		/* Use external resolver	*/
};

#define RTN_MAX RTN_XRESOLVE


/* rtm_protocol */

#define RTPROT_UNSPEC	0
#define RTPROT_REDIRECT	1	/* Route installed by ICMP redirects;
				   not used by current IPv4 */
#define RTPROT_KERNEL	2	/* Route installed by kernel		*/
#define RTPROT_BOOT	3	/* Route installed during boot		*/
#define RTPROT_STATIC	4	/* Route installed by administrator	*/

/* Values of protocol >= RTPROT_STATIC are not interpreted by kernel;
   they are just passed from user and back as is.
   It will be used by hypothetical multiple routing daemons.
   Note that protocol values should be standardized in order to
   avoid conflicts.
 */

#define RTPROT_GATED	8	/* Apparently, GateD */
#define RTPROT_RA	9	/* RDISC/ND router advertisements */
#define RTPROT_MRT	10	/* Merit MRT */
#define RTPROT_ZEBRA	11	/* Zebra */
#define RTPROT_BIRD	12	/* BIRD */
#define RTPROT_DNROUTED	13	/* DECnet routing daemon */
#define RTPROT_XORP	14	/* XORP */

/* rtm_scope

   Really it is not scope, but sort of distance to the destination.
   NOWHERE are reserved for not existing destinations, HOST is our
   local addresses, LINK are destinations, located on directly attached
   link and UNIVERSE is everywhere in the Universe.

   Intermediate values are also possible f.e. interior routes
   could be assigned a value between UNIVERSE and LINK.
*/

enum rt_scope_t
{
	RT_SCOPE_UNIVERSE=0,
/* User defined values  */
	RT_SCOPE_SITE=200,
	RT_SCOPE_LINK=253,
	RT_SCOPE_HOST=254,
	RT_SCOPE_NOWHERE=255
};

/* rtm_flags */

#define RTM_F_NOTIFY		0x100	/* Notify user of route change	*/
#define RTM_F_CLONED		0x200	/* This route is cloned		*/
#define RTM_F_EQUALIZE		0x400	/* Multipath equalizer: NI	*/
#define RTM_F_PREFIX		0x800	/* Prefix addresses		*/

/* Reserved table identifiers */

enum rt_class_t
{
	RT_TABLE_UNSPEC=0,
/* User defined values */
	RT_TABLE_DEFAULT=253,
	RT_TABLE_MAIN=254,
	RT_TABLE_LOCAL=255
};
#define RT_TABLE_MAX RT_TABLE_LOCAL



/* Routing message attributes */

enum rtattr_type_t
{
	RTA_UNSPEC,
	RTA_DST,
	RTA_SRC,
	RTA_IIF,
	RTA_OIF,
	RTA_GATEWAY,
	RTA_PRIORITY,
	RTA_PREFSRC,
	RTA_METRICS,
	RTA_MULTIPATH,
	RTA_PROTOINFO,
	RTA_FLOW,
	RTA_CACHEINFO
};

#define RTA_MAX RTA_CACHEINFO

#define RTM_RTA(r)  ((struct rtattr*)(((char*)(r)) + NLMSG_ALIGN(sizeof(struct rtmsg))))
#define RTM_PAYLOAD(n) NLMSG_PAYLOAD(n,sizeof(struct rtmsg))

/* RTM_MULTIPATH --- array of struct rtnexthop.
 *
 * "struct rtnexthop" describes all necessary nexthop information,
 * i.e. parameters of path to a destination via this nexthop.
 *
 * At the moment it is impossible to set different prefsrc, mtu, window
 * and rtt for different paths from multipath.
 */

struct rtnexthop
{
	unsigned short		rtnh_len;
	unsigned char		rtnh_flags;
	unsigned char		rtnh_hops;
	int			rtnh_ifindex;
};

/* rtnh_flags */

#define RTNH_F_DEAD		1	/* Nexthop is dead (used by multipath)	*/
#define RTNH_F_PERVASIVE	2	/* Do recursive gateway lookup	*/
#define RTNH_F_ONLINK		4	/* Gateway is forced on link	*/

/* Macros to handle hexthops */

#define RTNH_ALIGNTO	4
#define RTNH_ALIGN(len) ( ((len)+RTNH_ALIGNTO-1) & ~(RTNH_ALIGNTO-1) )
#define RTNH_OK(rtnh,len) ((rtnh)->rtnh_len >= sizeof(struct rtnexthop) && \
			   ((int)(rtnh)->rtnh_len) <= (len))
#define RTNH_NEXT(rtnh)	((struct rtnexthop*)(((char*)(rtnh)) + RTNH_ALIGN((rtnh)->rtnh_len)))
#define RTNH_LENGTH(len) (RTNH_ALIGN(sizeof(struct rtnexthop)) + (len))
#define RTNH_SPACE(len)	RTNH_ALIGN(RTNH_LENGTH(len))
#define RTNH_DATA(rtnh)   ((struct rtattr*)(((char*)(rtnh)) + RTNH_LENGTH(0)))

/* RTM_CACHEINFO */

struct rta_cacheinfo
{
	__u32	rta_clntref;
	__u32	rta_lastuse;
	__s32	rta_expires;
	__u32	rta_error;
	__u32	rta_used;

#define RTNETLINK_HAVE_PEERINFO 1
	__u32	rta_id;
	__u32	rta_ts;
	__u32	rta_tsage;
};

/* RTM_METRICS --- array of struct rtattr with types of RTAX_* */

enum
{
	RTAX_UNSPEC,
#define RTAX_UNSPEC RTAX_UNSPEC
	RTAX_LOCK,
#define RTAX_LOCK RTAX_LOCK
	RTAX_MTU,
#define RTAX_MTU RTAX_MTU
	RTAX_WINDOW,
#define RTAX_WINDOW RTAX_WINDOW
	RTAX_RTT,
#define RTAX_RTT RTAX_RTT
	RTAX_RTTVAR,
#define RTAX_RTTVAR RTAX_RTTVAR
	RTAX_SSTHRESH,
#define RTAX_SSTHRESH RTAX_SSTHRESH
	RTAX_CWND,
#define RTAX_CWND RTAX_CWND
	RTAX_ADVMSS,
#define RTAX_ADVMSS RTAX_ADVMSS
	RTAX_REORDERING,
#define RTAX_REORDERING RTAX_REORDERING
};

#define RTAX_MAX RTAX_REORDERING



/*********************************************************
 *		Interface address.
 ****/

struct ifaddrmsg
{
	unsigned char	ifa_family;
	unsigned char	ifa_prefixlen;	/* The prefix length		*/
	unsigned char	ifa_flags;	/* Flags			*/
	unsigned char	ifa_scope;	/* See above			*/
	int		ifa_index;	/* Link index			*/
};

enum
{
	IFA_UNSPEC,
	IFA_ADDRESS,
	IFA_LOCAL,
	IFA_LABEL,
	IFA_BROADCAST,
	IFA_ANYCAST,
	IFA_CACHEINFO
};

#define IFA_MAX IFA_CACHEINFO

/* ifa_flags */

#define IFA_F_SECONDARY		0x01

#define IFA_F_DEPRECATED	0x20
#define IFA_F_TENTATIVE		0x40
#define IFA_F_PERMANENT		0x80

struct ifa_cacheinfo
{
	__s32	ifa_prefered;
	__s32	ifa_valid;
};


#define IFA_RTA(r)  ((struct rtattr*)(((char*)(r)) + NLMSG_ALIGN(sizeof(struct ifaddrmsg))))
#define IFA_PAYLOAD(n) NLMSG_PAYLOAD(n,sizeof(struct ifaddrmsg))

/*
   Important comment:
   IFA_ADDRESS is prefix address, rather than local interface address.
   It makes no difference for normally configured broadcast interfaces,
   but for point-to-point IFA_ADDRESS is DESTINATION address,
   local address is supplied in IFA_LOCAL attribute.
 */

/**************************************************************
 *		Neighbour discovery.
 ****/

struct ndmsg
{
	unsigned char	ndm_family;
	unsigned char	ndm_pad1;
	unsigned short	ndm_pad2;
	int		ndm_ifindex;	/* Link index			*/
	__u16		ndm_state;
	__u8		ndm_flags;
	__u8		ndm_type;
};

enum
{
	NDA_UNSPEC,
	NDA_DST,
	NDA_LLADDR,
	NDA_CACHEINFO
};

#define NDA_MAX NDA_CACHEINFO

#define NDA_RTA(r)  ((struct rtattr*)(((char*)(r)) + NLMSG_ALIGN(sizeof(struct ndmsg))))
#define NDA_PAYLOAD(n) NLMSG_PAYLOAD(n,sizeof(struct ndmsg))

/*
 *	Neighbor Cache Entry Flags
 */

#define NTF_PROXY	0x08	/* == ATF_PUBL */
#define NTF_ROUTER	0x80

/*
 *	Neighbor Cache Entry States.
 */

#define NUD_INCOMPLETE	0x01
#define NUD_REACHABLE	0x02
#define NUD_STALE	0x04
#define NUD_DELAY	0x08
#define NUD_PROBE	0x10
#define NUD_FAILED	0x20

/* Dummy states */
#define NUD_NOARP	0x40
#define NUD_PERMANENT	0x80
#define NUD_NONE	0x00


struct nda_cacheinfo
{
	__u32		ndm_confirmed;
	__u32		ndm_used;
	__u32		ndm_updated;
	__u32		ndm_refcnt;
};

/****
 *		General form of address family dependent message.
 ****/

struct rtgenmsg
{
	unsigned char		rtgen_family;
};

/*****************************************************************
 *		Link layer specific messages.
 ****/

/* struct ifinfomsg
 * passes link level specific information, not dependent
 * on network protocol.
 */

struct ifinfomsg
{
	unsigned char	ifi_family;
	unsigned char	__ifi_pad;
	unsigned short	ifi_type;		/* ARPHRD_* */
	int		ifi_index;		/* Link index	*/
	unsigned	ifi_flags;		/* IFF_* flags	*/
	unsigned	ifi_change;		/* IFF_* change mask */
};

enum
{
	IFLA_UNSPEC,
	IFLA_ADDRESS,
	IFLA_BROADCAST,
	IFLA_IFNAME,
	IFLA_MTU,
	IFLA_LINK,
	IFLA_QDISC,
	IFLA_STATS,
	IFLA_COST,
#define IFLA_COST IFLA_COST
	IFLA_PRIORITY,
#define IFLA_PRIORITY IFLA_PRIORITY
	IFLA_MASTER,
#define IFLA_MASTER IFLA_MASTER
	IFLA_WIRELESS,		/* Wireless Extension event - see wireless.h */
#define IFLA_WIRELESS IFLA_WIRELESS
	IFLA_PROTINFO,		/* Protocol specific information for a link */
#define IFLA_PROTINFO IFLA_PROTINFO
};


#define IFLA_MAX IFLA_PROTINFO

#define IFLA_RTA(r)  ((struct rtattr*)(((char*)(r)) + NLMSG_ALIGN(sizeof(struct ifinfomsg))))
#define IFLA_PAYLOAD(n) NLMSG_PAYLOAD(n,sizeof(struct ifinfomsg))

/* ifi_flags.

   IFF_* flags.

   The only change is:
   IFF_LOOPBACK, IFF_BROADCAST and IFF_POINTOPOINT are
   more not changeable by user. They describe link media
   characteristics and set by device driver.

   Comments:
   - Combination IFF_BROADCAST|IFF_POINTOPOINT is invalid
   - If neither of these three flags are set;
     the interface is NBMA.

   - IFF_MULTICAST does not mean anything special:
   multicasts can be used on all not-NBMA links.
   IFF_MULTICAST means that this media uses special encapsulation
   for multicast frames. Apparently, all IFF_POINTOPOINT and
   IFF_BROADCAST devices are able to use multicasts too.
 */

/* IFLA_LINK.
   For usual devices it is equal ifi_index.
   If it is a "virtual interface" (f.e. tunnel), ifi_link
   can point to real physical interface (f.e. for bandwidth calculations),
   or maybe 0, what means, that real media is unknown (usual
   for IPIP tunnels, when route to endpoint is allowed to change)
 */

/* Subtype attributes for IFLA_PROTINFO */
enum
{
	IFLA_INET6_UNSPEC,
	IFLA_INET6_FLAGS,	/* link flags			*/
	IFLA_INET6_CONF,	/* sysctl parameters		*/
	IFLA_INET6_STATS,	/* statistics			*/
	IFLA_INET6_MCAST,	/* MC things. What of them?	*/
};

#define IFLA_INET6_MAX	IFLA_INET6_MCAST

/*****************************************************************
 *		Traffic control messages.
 ****/

struct tcmsg
{
	unsigned char	tcm_family;
	unsigned char	tcm__pad1;
	unsigned short	tcm__pad2;
	int		tcm_ifindex;
	__u32		tcm_handle;
	__u32		tcm_parent;
	__u32		tcm_info;
};

enum
{
	TCA_UNSPEC,
	TCA_KIND,
	TCA_OPTIONS,
	TCA_STATS,
	TCA_XSTATS,
	TCA_RATE,
};

#define TCA_MAX TCA_RATE

#define TCA_RTA(r)  ((struct rtattr*)(((char*)(r)) + NLMSG_ALIGN(sizeof(struct tcmsg))))
#define TCA_PAYLOAD(n) NLMSG_PAYLOAD(n,sizeof(struct tcmsg))


/* SUMMARY: maximal rtattr understood by kernel */

#define RTATTR_MAX		RTA_MAX

/* RTnetlink multicast groups */

#define RTMGRP_LINK		1
#define RTMGRP_NOTIFY		2
#define RTMGRP_NEIGH		4
#define RTMGRP_TC		8

#define RTMGRP_IPV4_IFADDR	0x10
#define RTMGRP_IPV4_MROUTE	0x20
#define RTMGRP_IPV4_ROUTE	0x40

#define RTMGRP_IPV6_IFADDR	0x100
#define RTMGRP_IPV6_MROUTE	0x200
#define RTMGRP_IPV6_ROUTE	0x400

#define RTMGRP_DECnet_IFADDR    0x1000
#define RTMGRP_DECnet_ROUTE     0x4000

/* End of information exported to user level */

#ifdef __KERNEL__

#include <linux/config.h>

static __inline__ int rtattr_strcmp(struct rtattr *rta, char *str)
{
	int len = strlen(str) + 1;
	return len > rta->rta_len || memcmp(RTA_DATA(rta), str, len);
}

extern int rtattr_parse(struct rtattr *tb[], int maxattr, struct rtattr *rta, int len);

extern struct sock *rtnl;

struct rtnetlink_link
{
	int (*doit)(struct sk_buff *, struct nlmsghdr*, void *attr);
	int (*dumpit)(struct sk_buff *, struct netlink_callback *cb);
};

extern struct rtnetlink_link * rtnetlink_links[NPROTO];
extern int rtnetlink_dump_ifinfo(struct sk_buff *skb, struct netlink_callback *cb);
extern int rtnetlink_send(struct sk_buff *skb, u32 pid, u32 group, int echo);
extern int rtnetlink_put_metrics(struct sk_buff *skb, unsigned *metrics);

extern void __rta_fill(struct sk_buff *skb, int attrtype, int attrlen, const void *data);

#define RTA_PUT(skb, attrtype, attrlen, data) \
({ if (skb_tailroom(skb) < (int)RTA_SPACE(attrlen)) goto rtattr_failure; \
   __rta_fill(skb, attrtype, attrlen, data); })

extern void rtmsg_ifinfo(int type, struct net_device *dev, unsigned change);

extern struct semaphore rtnl_sem;

#define rtnl_exlock()		do { } while(0)
#define rtnl_exunlock()		do { } while(0)
#define rtnl_exlock_nowait()	(0)

#define rtnl_shlock()		down(&rtnl_sem)
#define rtnl_shlock_nowait()	down_trylock(&rtnl_sem)

#define rtnl_shunlock()	do { up(&rtnl_sem); \
		             if (rtnl && rtnl->receive_queue.qlen) \
				     rtnl->data_ready(rtnl, 0); \
		        } while(0)

extern void rtnl_lock(void);
extern void rtnl_unlock(void);
extern void rtnetlink_init(void);

#define ASSERT_RTNL() do { if (down_trylock(&rtnl_sem) == 0)  { up(&rtnl_sem); \
printk("RTNL: assertion failed at " __FILE__ "(%d)\n", __LINE__); } \
		   } while(0)
#define BUG_TRAP(x) if (!(x)) { printk("KERNEL: assertion (" #x ") failed at " __FILE__ "(%d)\n", __LINE__); }


#endif /* __KERNEL__ */


#endif	/* __LINUX_RTNETLINK_H */
