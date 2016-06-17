/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		Definitions for the UDP module.
 *
 * Version:	@(#)udp.h	1.0.2	05/07/93
 *
 * Authors:	Ross Biro, <bir7@leland.Stanford.Edu>
 *		Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *
 * Fixes:
 *		Alan Cox	: Turned on udp checksums. I don't want to
 *				  chase 'memory corruption' bugs that aren't!
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 */
#ifndef _UDP_H
#define _UDP_H

#include <linux/udp.h>
#include <net/sock.h>

#define UDP_HTABLE_SIZE		128

/* udp.c: This needs to be shared by v4 and v6 because the lookup
 *        and hashing code needs to work with different AF's yet
 *        the port space is shared.
 */
extern struct sock *udp_hash[UDP_HTABLE_SIZE];
extern rwlock_t udp_hash_lock;

extern int udp_port_rover;

static inline int udp_lport_inuse(u16 num)
{
	struct sock *sk = udp_hash[num & (UDP_HTABLE_SIZE - 1)];

	for(; sk != NULL; sk = sk->next) {
		if(sk->num == num)
			return 1;
	}
	return 0;
}

/* Note: this must match 'valbool' in sock_setsockopt */
#define UDP_CSUM_NOXMIT		1

/* Used by SunRPC/xprt layer. */
#define UDP_CSUM_NORCV		2

/* Default, as per the RFC, is to always do csums. */
#define UDP_CSUM_DEFAULT	0

extern struct proto udp_prot;


extern void	udp_err(struct sk_buff *, u32);
extern int	udp_connect(struct sock *sk,
			    struct sockaddr *usin, int addr_len);

extern int	udp_sendmsg(struct sock *sk, struct msghdr *msg, int len);

extern int	udp_rcv(struct sk_buff *skb);
extern int	udp_ioctl(struct sock *sk, int cmd, unsigned long arg);
extern int	udp_disconnect(struct sock *sk, int flags);

extern struct udp_mib udp_statistics[NR_CPUS*2];
#define UDP_INC_STATS(field)		SNMP_INC_STATS(udp_statistics, field)
#define UDP_INC_STATS_BH(field)		SNMP_INC_STATS_BH(udp_statistics, field)
#define UDP_INC_STATS_USER(field) 	SNMP_INC_STATS_USER(udp_statistics, field)

#endif	/* _UDP_H */
