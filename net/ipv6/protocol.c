/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		PF_INET6 protocol dispatch tables.
 *
 * Version:	$Id: protocol.c,v 1.10 2001/05/18 02:25:49 davem Exp $
 *
 * Authors:	Pedro Roque	<roque@di.fc.ul.pt>
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 */

/*
 *      Changes:
 *
 *      Vince Laviano (vince@cs.stanford.edu)       16 May 2001
 *      - Removed unused variable 'inet6_protocol_base'
 *      - Modified inet6_del_protocol() to correctly maintain copy bit.
 */

#include <linux/errno.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <linux/sockios.h>
#include <linux/sched.h>
#include <linux/net.h>
#include <linux/in6.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <linux/brlock.h>

#include <net/sock.h>
#include <net/snmp.h>

#include <net/ipv6.h>
#include <net/protocol.h>

struct inet6_protocol *inet6_protos[MAX_INET_PROTOS];

void inet6_add_protocol(struct inet6_protocol *prot)
{
	unsigned char hash;
	struct inet6_protocol *p2;

	hash = prot->protocol & (MAX_INET_PROTOS - 1);
	br_write_lock_bh(BR_NETPROTO_LOCK);
	prot->next = inet6_protos[hash];
	inet6_protos[hash] = prot;
	prot->copy = 0;

	/*
	 *	Set the copy bit if we need to. 
	 */
	 
	p2 = (struct inet6_protocol *) prot->next;
	while(p2 != NULL) {
		if (p2->protocol == prot->protocol) {
			prot->copy = 1;
			break;
		}
		p2 = (struct inet6_protocol *) p2->next;
	}
	br_write_unlock_bh(BR_NETPROTO_LOCK);
}

/*
 *	Remove a protocol from the hash tables.
 */
 
int inet6_del_protocol(struct inet6_protocol *prot)
{
	struct inet6_protocol *p;
	struct inet6_protocol *lp = NULL;
	unsigned char hash;

	hash = prot->protocol & (MAX_INET_PROTOS - 1);
	br_write_lock_bh(BR_NETPROTO_LOCK);
	if (prot == inet6_protos[hash]) {
		inet6_protos[hash] = (struct inet6_protocol *) inet6_protos[hash]->next;
		br_write_unlock_bh(BR_NETPROTO_LOCK);
		return(0);
	}

	p = (struct inet6_protocol *) inet6_protos[hash];

        if (p != NULL && p->protocol == prot->protocol)
                lp = p;

	while(p != NULL) {
		/*
		 * We have to worry if the protocol being deleted is
		 * the last one on the list, then we may need to reset
		 * someone's copied bit.
		 */
		if (p->next != NULL && p->next == prot) {
			/*
			 * if we are the last one with this protocol and
			 * there is a previous one, reset its copy bit.
			 */
			if (prot->copy == 0 && lp != NULL)
				lp->copy = 0;
			p->next = prot->next;
			br_write_unlock_bh(BR_NETPROTO_LOCK);
			return(0);
		}
		if (p->next != NULL && p->next->protocol == prot->protocol) 
			lp = p->next;

		p = (struct inet6_protocol *) p->next;
	}
	br_write_unlock_bh(BR_NETPROTO_LOCK);
	return(-1);
}
