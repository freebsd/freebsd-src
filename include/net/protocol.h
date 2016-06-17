/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		Definitions for the protocol dispatcher.
 *
 * Version:	@(#)protocol.h	1.0.2	05/07/93
 *
 * Author:	Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 *	Changes:
 *		Alan Cox	:	Added a name field and a frag handler
 *					field for later.
 *		Alan Cox	:	Cleaned up, and sorted types.
 *		Pedro Roque	:	inet6 protocols
 */
 
#ifndef _PROTOCOL_H
#define _PROTOCOL_H

#include <linux/config.h>
#include <linux/in6.h>
#if defined(CONFIG_IPV6) || defined (CONFIG_IPV6_MODULE)
#include <linux/ipv6.h>
#endif

#define MAX_INET_PROTOS	32		/* Must be a power of 2		*/


/* This is used to register protocols. */
struct inet_protocol 
{
	int			(*handler)(struct sk_buff *skb);
	void			(*err_handler)(struct sk_buff *skb, u32 info);
	struct inet_protocol	*next;
	unsigned char		protocol;
	unsigned char		copy:1;
	void			*data;
	const char		*name;
};

#if defined(CONFIG_IPV6) || defined (CONFIG_IPV6_MODULE)
struct inet6_protocol 
{
	int	(*handler)(struct sk_buff *skb);

	void	(*err_handler)(struct sk_buff *skb,
			       struct inet6_skb_parm *opt,
			       int type, int code, int offset,
			       __u32 info);
	struct inet6_protocol *next;
	unsigned char	protocol;
	unsigned char	copy:1;
	void		*data;
	const char	*name;
};

#endif

/* This is used to register socket interfaces for IP protocols.  */
struct inet_protosw {
	struct list_head list;

        /* These two fields form the lookup key.  */
	unsigned short	 type;	   /* This is the 2nd argument to socket(2). */
	int		 protocol; /* This is the L4 protocol number.  */

	struct proto	 *prot;
	struct proto_ops *ops;
  
	int              capability; /* Which (if any) capability do
				      * we need to use this socket
				      * interface?
                                      */
	char             no_check;   /* checksum on rcv/xmit/none? */
	unsigned char	 flags;      /* See INET_PROTOSW_* below.  */
};
#define INET_PROTOSW_REUSE 0x01	     /* Are ports automatically reusable? */
#define INET_PROTOSW_PERMANENT 0x02  /* Permanent protocols are unremovable. */

extern struct inet_protocol *inet_protocol_base;
extern struct inet_protocol *inet_protos[MAX_INET_PROTOS];
extern struct list_head inetsw[SOCK_MAX];

#if defined(CONFIG_IPV6) || defined (CONFIG_IPV6_MODULE)
extern struct inet6_protocol *inet6_protos[MAX_INET_PROTOS];
extern struct list_head inetsw6[SOCK_MAX];
#endif

extern void	inet_add_protocol(struct inet_protocol *prot);
extern int	inet_del_protocol(struct inet_protocol *prot);
extern void	inet_register_protosw(struct inet_protosw *p);
extern void	inet_unregister_protosw(struct inet_protosw *p);

#if defined(CONFIG_IPV6) || defined (CONFIG_IPV6_MODULE)
extern void	inet6_add_protocol(struct inet6_protocol *prot);
extern int	inet6_del_protocol(struct inet6_protocol *prot);
extern void	inet6_register_protosw(struct inet_protosw *p);
extern void	inet6_unregister_protosw(struct inet_protosw *p);
#endif

#endif	/* _PROTOCOL_H */
