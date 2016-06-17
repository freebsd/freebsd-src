/*
 *	Definitions for the SECurity layer
 *
 *	Author:
 *		Robert Muchsel <muchsel@acm.org>
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */
 
#ifndef _LINUX_IPSEC_H
#define _LINUX_IPSEC_H

#include <linux/config.h>
#include <linux/socket.h>
#include <net/sock.h>
#include <linux/skbuff.h>

/* Values for the set/getsockopt calls */

/* These defines are compatible with NRL IPv6, however their semantics
   is different */

#define IPSEC_LEVEL_NONE	-1	/* send plaintext, accept any */
#define IPSEC_LEVEL_DEFAULT	0	/* encrypt/authenticate if possible */
					/* the default MUST be 0, because a */
					/* socket is initialized with 0's */
#define IPSEC_LEVEL_USE		1	/* use outbound, don't require inbound */
#define IPSEC_LEVEL_REQUIRE	2	/* require both directions */
#define IPSEC_LEVEL_UNIQUE	2	/* for compatibility only */

#ifdef __KERNEL__

/* skb bit flags set on packet input processing */

#define RCV_SEC			0x0f	/* options on receive */
#define RCV_AUTH		0x01	/* was authenticated */
#define RCV_CRYPT		0x02	/* was encrypted */
#define RCV_TUNNEL		0x04	/* was tunneled */
#define SND_SEC			0xf0	/* options on send, these are */
#define SND_AUTH		0x10	/* currently unused */
#define SND_CRYPT		0x20
#define SND_TUNNEL		0x40

/*
 *	FIXME: ignores network encryption for now..
 */
 
#ifdef CONFIG_NET_SECURITY
static __inline__ int ipsec_sk_policy(struct sock *sk, struct sk_buff *skb)
{
	return ((sk->authentication < IPSEC_LEVEL_REQUIRE) ||
		(skb->security & RCV_AUTH)) &&
		((sk->encryption < IPSEC_LEVEL_REQUIRE) ||
		(skb->security & RCV_CRYPT));
}

#else

static __inline__ int ipsec_sk_policy(struct sock *sk, struct sk_buff *skb)
{
	return 1;
}
#endif /* CONFIG */

#endif	/* __KERNEL__ */
#endif	/* _LINUX_IPSEC_H */
