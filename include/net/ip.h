/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		Definitions for the IP module.
 *
 * Version:	@(#)ip.h	1.0.2	05/07/93
 *
 * Authors:	Ross Biro, <bir7@leland.Stanford.Edu>
 *		Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *		Alan Cox, <gw4pts@gw4pts.ampr.org>
 *
 * Changes:
 *		Mike McLagan    :       Routing by source
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 */
#ifndef _IP_H
#define _IP_H

#include <linux/config.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <linux/ip.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/in_route.h>
#include <net/route.h>
#include <net/arp.h>

#ifndef _SNMP_H
#include <net/snmp.h>
#endif

#include <net/sock.h>	/* struct sock */

struct inet_skb_parm
{
	struct ip_options	opt;		/* Compiled IP options		*/
	unsigned char		flags;

#define IPSKB_MASQUERADED	1
#define IPSKB_TRANSLATED	2
#define IPSKB_FORWARDED		4
};

struct ipcm_cookie
{
	u32			addr;
	int			oif;
	struct ip_options	*opt;
};

#define IPCB(skb) ((struct inet_skb_parm*)((skb)->cb))

struct ip_ra_chain
{
	struct ip_ra_chain	*next;
	struct sock		*sk;
	void			(*destructor)(struct sock *);
};

extern struct ip_ra_chain *ip_ra_chain;
extern rwlock_t ip_ra_lock;

/* IP flags. */
#define IP_CE		0x8000		/* Flag: "Congestion"		*/
#define IP_DF		0x4000		/* Flag: "Don't Fragment"	*/
#define IP_MF		0x2000		/* Flag: "More Fragments"	*/
#define IP_OFFSET	0x1FFF		/* "Fragment Offset" part	*/

#define IP_FRAG_TIME	(30 * HZ)		/* fragment lifetime	*/

extern void		ip_mc_dropsocket(struct sock *);
extern void		ip_mc_dropdevice(struct net_device *dev);
extern int		ip_mc_procinfo(char *, char **, off_t, int);
extern int		ip_mcf_procinfo(char *, char **, off_t, int);

/*
 *	Functions provided by ip.c
 */

extern int		ip_build_and_send_pkt(struct sk_buff *skb, struct sock *sk,
					      u32 saddr, u32 daddr,
					      struct ip_options *opt);
extern int		ip_rcv(struct sk_buff *skb, struct net_device *dev,
			       struct packet_type *pt);
extern int		ip_local_deliver(struct sk_buff *skb);
extern int		ip_mr_input(struct sk_buff *skb);
extern int		ip_output(struct sk_buff *skb);
extern int		ip_mc_output(struct sk_buff *skb);
extern int		ip_fragment(struct sk_buff *skb, int (*out)(struct sk_buff*));
extern int		ip_do_nat(struct sk_buff *skb);
extern void		ip_send_check(struct iphdr *ip);
extern int		ip_queue_xmit(struct sk_buff *skb, int ipfragok);
extern void		ip_init(void);
extern int		ip_build_xmit(struct sock *sk,
				      int getfrag (const void *,
						   char *,
						   unsigned int,
						   unsigned int),
				      const void *frag,
				      unsigned length,
				      struct ipcm_cookie *ipc,
				      struct rtable *rt,
				      int flags);

/*
 *	Map a multicast IP onto multicast MAC for type Token Ring.
 *      This conforms to RFC1469 Option 2 Multicasting i.e.
 *      using a functional address to transmit / receive 
 *      multicast packets.
 */

static inline void ip_tr_mc_map(u32 addr, char *buf)
{
	buf[0]=0xC0;
	buf[1]=0x00;
	buf[2]=0x00;
	buf[3]=0x04;
	buf[4]=0x00;
	buf[5]=0x00;
}

struct ip_reply_arg {
	struct iovec iov[2];   
	int          n_iov;    /* redundant */
	u32 	     csum; 
	int	     csumoffset; /* u16 offset of csum in iov[0].iov_base */
				 /* -1 if not needed */ 
}; 

void ip_send_reply(struct sock *sk, struct sk_buff *skb, struct ip_reply_arg *arg,
		   unsigned int len); 

extern __inline__ int ip_finish_output(struct sk_buff *skb);

struct ipv4_config
{
	int	log_martians;
	int	autoconfig;
	int	no_pmtu_disc;
};

extern struct ipv4_config ipv4_config;
extern struct ip_mib	ip_statistics[NR_CPUS*2];
#define IP_INC_STATS(field)		SNMP_INC_STATS(ip_statistics, field)
#define IP_INC_STATS_BH(field)		SNMP_INC_STATS_BH(ip_statistics, field)
#define IP_INC_STATS_USER(field) 	SNMP_INC_STATS_USER(ip_statistics, field)
extern struct linux_mib	net_statistics[NR_CPUS*2];
#define NET_INC_STATS(field)		SNMP_INC_STATS(net_statistics, field)
#define NET_INC_STATS_BH(field)		SNMP_INC_STATS_BH(net_statistics, field)
#define NET_INC_STATS_USER(field) 	SNMP_INC_STATS_USER(net_statistics, field)

extern int sysctl_local_port_range[2];
extern int sysctl_ip_default_ttl;

#ifdef CONFIG_INET
static inline int ip_send(struct sk_buff *skb)
{
	if (skb->len > skb->dst->pmtu)
		return ip_fragment(skb, ip_finish_output);
	else
		return ip_finish_output(skb);
}

/* The function in 2.2 was invalid, producing wrong result for
 * check=0xFEFF. It was noticed by Arthur Skawina _year_ ago. --ANK(000625) */
static inline
int ip_decrease_ttl(struct iphdr *iph)
{
	u32 check = iph->check;
	check += __constant_htons(0x0100);
	iph->check = check + (check>=0xFFFF);
	return --iph->ttl;
}

static inline
int ip_dont_fragment(struct sock *sk, struct dst_entry *dst)
{
	return (sk->protinfo.af_inet.pmtudisc == IP_PMTUDISC_DO ||
		(sk->protinfo.af_inet.pmtudisc == IP_PMTUDISC_WANT &&
		 !(dst->mxlock&(1<<RTAX_MTU))));
}

extern void __ip_select_ident(struct iphdr *iph, struct dst_entry *dst);

static inline void ip_select_ident(struct iphdr *iph, struct dst_entry *dst, struct sock *sk)
{
	if (iph->frag_off&__constant_htons(IP_DF)) {
		/* This is only to work around buggy Windows95/2000
		 * VJ compression implementations.  If the ID field
		 * does not change, they drop every other packet in
		 * a TCP stream using header compression.
		 */
		iph->id = ((sk && sk->daddr) ? htons(sk->protinfo.af_inet.id++) : 0);
	} else
		__ip_select_ident(iph, dst);
}

/*
 *	Map a multicast IP onto multicast MAC for type ethernet.
 */

static inline void ip_eth_mc_map(u32 addr, char *buf)
{
	addr=ntohl(addr);
	buf[0]=0x01;
	buf[1]=0x00;
	buf[2]=0x5e;
	buf[5]=addr&0xFF;
	addr>>=8;
	buf[4]=addr&0xFF;
	addr>>=8;
	buf[3]=addr&0x7F;
}

#endif

extern int	ip_call_ra_chain(struct sk_buff *skb);

/*
 *	Functions provided by ip_fragment.o
 */
 
struct sk_buff *ip_defrag(struct sk_buff *skb);
extern int ip_frag_nqueues;
extern atomic_t ip_frag_mem;

/*
 *	Functions provided by ip_forward.c
 */
 
extern int ip_forward(struct sk_buff *skb);
extern int ip_net_unreachable(struct sk_buff *skb);
 
/*
 *	Functions provided by ip_options.c
 */
 
extern void ip_options_build(struct sk_buff *skb, struct ip_options *opt, u32 daddr, struct rtable *rt, int is_frag);
extern int ip_options_echo(struct ip_options *dopt, struct sk_buff *skb);
extern void ip_options_fragment(struct sk_buff *skb);
extern int ip_options_compile(struct ip_options *opt, struct sk_buff *skb);
extern int ip_options_get(struct ip_options **optp, unsigned char *data, int optlen, int user);
extern void ip_options_undo(struct ip_options * opt);
extern void ip_forward_options(struct sk_buff *skb);
extern int ip_options_rcv_srr(struct sk_buff *skb);

/*
 *	Functions provided by ip_sockglue.c
 */

extern void	ip_cmsg_recv(struct msghdr *msg, struct sk_buff *skb);
extern int	ip_cmsg_send(struct msghdr *msg, struct ipcm_cookie *ipc);
extern int	ip_setsockopt(struct sock *sk, int level, int optname, char *optval, int optlen);
extern int	ip_getsockopt(struct sock *sk, int level, int optname, char *optval, int *optlen);
extern int	ip_ra_control(struct sock *sk, unsigned char on, void (*destructor)(struct sock *));

extern int 	ip_recv_error(struct sock *sk, struct msghdr *msg, int len);
extern void	ip_icmp_error(struct sock *sk, struct sk_buff *skb, int err, 
			      u16 port, u32 info, u8 *payload);
extern void	ip_local_error(struct sock *sk, int err, u32 daddr, u16 dport,
			       u32 info);

#endif	/* _IP_H */
