/* IRC extension for IP connection tracking.
 * (C) 2000 by Harald Welte <laforge@gnumonks.org>
 * based on RR's ip_conntrack_ftp.h
 *
 * ip_conntrack_irc.h,v 1.6 2000/11/07 18:26:42 laforge Exp
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 *
 */
#ifndef _IP_CONNTRACK_IRC_H
#define _IP_CONNTRACK_IRC_H

/* We record seq number and length of irc ip/port text here: all in
   host order. */

/* This structure is per expected connection */
struct ip_ct_irc_expect
{
	/* length of IP address */
	u_int32_t len;
	/* Port that was to be used */
	u_int16_t port;
};

/* This structure exists only once per master */
struct ip_ct_irc_master {
};


#ifdef __KERNEL__

#include <linux/netfilter_ipv4/lockhelp.h>

#define IRC_PORT	6667

struct dccproto {
	char* match;
	int matchlen;
};

/* Protects irc part of conntracks */
DECLARE_LOCK_EXTERN(ip_irc_lock);

#endif /* __KERNEL__ */

#endif /* _IP_CONNTRACK_IRC_H */
