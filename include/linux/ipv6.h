#ifndef _IPV6_H
#define _IPV6_H

#include <linux/in6.h>
#include <asm/byteorder.h>

/* The latest drafts declared increase in minimal mtu up to 1280. */

#define IPV6_MIN_MTU	1280

/*
 *	Advanced API
 *	source interface/address selection, source routing, etc...
 *	*under construction*
 */


struct in6_pktinfo {
	struct in6_addr	ipi6_addr;
	int		ipi6_ifindex;
};


struct in6_ifreq {
	struct in6_addr	ifr6_addr;
	__u32		ifr6_prefixlen;
	int		ifr6_ifindex; 
};

#define IPV6_SRCRT_STRICT	0x01	/* this hop must be a neighbor	*/
#define IPV6_SRCRT_TYPE_0	0	/* IPv6 type 0 Routing Header	*/

/*
 *	routing header
 */
struct ipv6_rt_hdr {
	__u8		nexthdr;
	__u8		hdrlen;
	__u8		type;
	__u8		segments_left;

	/*
	 *	type specific data
	 *	variable length field
	 */
};


struct ipv6_opt_hdr {
	__u8 		nexthdr;
	__u8 		hdrlen;
	/* 
	 * TLV encoded option data follows.
	 */
};

#define ipv6_destopt_hdr ipv6_opt_hdr
#define ipv6_hopopt_hdr  ipv6_opt_hdr

#ifdef __KERNEL__
#define ipv6_optlen(p)  (((p)->hdrlen+1) << 3)
#endif

/*
 *	routing header type 0 (used in cmsghdr struct)
 */

struct rt0_hdr {
	struct ipv6_rt_hdr	rt_hdr;
	__u32			bitmap;		/* strict/loose bit map */
	struct in6_addr		addr[0];

#define rt0_type		rt_hdr.type
};

/*
 *	IPv6 fixed header
 *
 *	BEWARE, it is incorrect. The first 4 bits of flow_lbl
 *	are glued to priority now, forming "class".
 */

struct ipv6hdr {
#if defined(__LITTLE_ENDIAN_BITFIELD)
	__u8			priority:4,
				version:4;
#elif defined(__BIG_ENDIAN_BITFIELD)
	__u8			version:4,
				priority:4;
#else
#error	"Please fix <asm/byteorder.h>"
#endif
	__u8			flow_lbl[3];

	__u16			payload_len;
	__u8			nexthdr;
	__u8			hop_limit;

	struct	in6_addr	saddr;
	struct	in6_addr	daddr;
};

/*
 * This structure contains configuration options per IPv6 link.
 */
struct ipv6_devconf {
	__s32		forwarding;
	__s32		hop_limit;
	__s32		mtu6;
	__s32		accept_ra;
	__s32		accept_redirects;
	__s32		autoconf;
	__s32		dad_transmits;
	__s32		rtr_solicits;
	__s32		rtr_solicit_interval;
	__s32		rtr_solicit_delay;
#ifdef CONFIG_IPV6_PRIVACY
	__s32		use_tempaddr;
	__s32		temp_valid_lft;
	__s32		temp_prefered_lft;
	__s32		regen_max_retry;
	__s32		max_desync_factor;
#endif
	void		*sysctl;
};

/* index values for the variables in ipv6_devconf */
enum {
	DEVCONF_FORWARDING = 0,
	DEVCONF_HOPLIMIT,
	DEVCONF_MTU6,
	DEVCONF_ACCEPT_RA,
	DEVCONF_ACCEPT_REDIRECTS,
	DEVCONF_AUTOCONF,
	DEVCONF_DAD_TRANSMITS,
	DEVCONF_RTR_SOLICITS,
	DEVCONF_RTR_SOLICIT_INTERVAL,
	DEVCONF_RTR_SOLICIT_DELAY,
#ifdef CONFIG_IPV6_PRIVACY
	DEVCONF_USE_TEMPADDR,
	DEVCONF_TEMP_VALID_LFT,
	DEVCONF_TEMP_PREFERED_LFT,
	DEVCONF_REGEN_MAX_RETRY,
	DEVCONF_MAX_DESYNC_FACTOR,
#endif
	DEVCONF_MAX
};

#ifdef __KERNEL__

/* 
   This structure contains results of exthdrs parsing
   as offsets from skb->nh.
 */

struct inet6_skb_parm
{
	int			iif;
	__u16			ra;
	__u16			hop;
	__u16			auth;
	__u16			dst0;
	__u16			srcrt;
	__u16			dst1;
};

#endif

#endif
