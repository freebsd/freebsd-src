#if !defined(lint) && !defined(SABER)
static const char sccsid[] = "@(#)ns_sort.c	4.10 (Berkeley) 3/3/91";
static const char rcsid[] = "$Id: ns_sort.c,v 8.6 2000/04/21 06:54:13 vixie Exp $";
#endif /* not lint */

/*
 * Copyright (c) 1986, 1990
 *    The Regents of the University of California.  All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 * 	This product includes software developed by the University of
 * 	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/*
 * Portions Copyright (c) 1996-2000 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/*
 * Sorting should really be handled by the resolver, but:
 *  1) There are too many brain dead resolvers out there that can't be replaced.
 *  2) It would be a pain to individually configure all those resolvers anyway.
 *
 * Here's the scoop:
 *
 * To enable address sorting in responses, you need to supply the sortlist
 * statement in the config file.  The sortlist statement takes an
 * address match list and interprets it even more specially than the
 * topology statement does.
 *
 * Each top level statement in the sortlist must itself be an explicit
 * address match list with one or two elements.  The first element
 * (which may be an IP address, an IP prefix, an ACL name or nested
 * address match list) of each top level list is checked against the
 * source address of the query until a match is found.
 *
 * Once the source address of the query has been matched, if the top level
 * statement contains only one element, the actual primitive element that
 * matched the source address is used to select the address in the response
 * to move to the beginning of the response.  If the statement is a list
 * of two elements, then the second element is treated like the address
 * match list in a topology statement.  Each top level element is assigned
 * a distance and the address in the response with the minimum distance is
 * moved to the beginning of the response.
 * 
 * In the following example, any queries received from any of the addresses
 * of the host itself will get responses preferring addresses on any of
 * the locally connected networks.  Next most preferred are addresses on
 * the 192.168.1/24 network, and after that either the 192.168.2/24 or
 * 192.168.3/24 network with no preference shown between these two networks.
 * Queries received from a host on the 192.168.1/24 network will prefer
 * other addresses on that network to the 192.168.2/24 and 192.168.3/24
 * networks.  Queries received from a host on the 192.168.4/24 or the
 * 192.168.5/24 network will only prefer other addresses on their
 * directly connected networks.
 *
 * sortlist {
 *            {
 *              localhost;
 *              {
 *                localnets;
 *                192.168.1/24;
 *                { 192,168.2/24; 192.168.3/24; };
 *              };
 *            };
 *            {
 *              192.168.1/24;
 *              {
 *                192.168.1/24;
 *                { 192.168.2/24; 192.168.3/24; };
 *              };
 *            };
 *            {
 *              192.168.2/24;
 *              {
 *                192.168.2/24;
 *                { 192.168.1/24; 192.168.3/24; };
 *              };
 *            };
 *            {
 *              192.168.3/24;
 *              {
 *                192.168.3/24;
 *                { 192.168.1/24; 192.168.2/24; };
 *              };
 *            };
 *	      {
 *              { 192.168.4/24; 192.168.5/24; };
 *            };
 * };
 *
 * 
 * The following example will give reasonable behaviour for the local host
 * and hosts on directly connected networks.  It is similar to the behavior
 * of the address sort in BIND 4.9.x.  Responses sent to queries from the
 * local host will favor any of the directly connected networks.  Responses
 * sent to queries from any other hosts on a directly connected network will
 * prefer addresses on that same network.  Responses to other queries will
 * not be sorted.
 *
 * sortlist {
 *            { localhost; localnets; };
 *            { localnets; };
 * };
 *
 * XXX - it wouldb e nice to have an ACL called "source" that matched the
 *       source address of a query so that a host could be configured to
 *       automatically prefer itself, and an ACL called "sourcenet", that
 *       would return the primitive IP match element that matched the source
 *       address so that you could do:
 *          { localnets; { sourcenet; { other stuff ...}; };
 *       and automatically get similar behaviour to what you get with:
 *          { localnets; };
 *
 */

#include "port_before.h"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <syslog.h>
#include <resolv.h>

#include <isc/eventlib.h>
#include <isc/logging.h>

#include "port_after.h"

#include "named.h"

static int sort_rr(u_char *cp, u_char *eom, int ancount, ip_match_list iml);

static int ip_match_address_elt(ip_match_list, struct in_addr,
				ip_match_element *);

void
sort_response(u_char *cp, u_char *eom, int ancount, struct sockaddr_in *from) {
	struct in_addr address;
	struct ip_match_element imelement;
	ip_match_element imetl, imematch, imeprimitive;
	struct ip_match_list imlist;
	ip_match_list iml;
	int indirect, matched;

	if (server_options->sortlist == NULL)
		return;

	if (from->sin_family != AF_INET)
		return;

	address = from->sin_addr;

	for (imetl = server_options->sortlist->first; imetl != NULL;
	     imetl = imetl->next) {
		if (imetl->type == ip_match_indirect)
			imematch = imetl->u.indirect.list->first;
		else
			/*
			 * allow a bare pattern as a top level statement
			 * and treat it like {pattern;};
			 */
			imematch = imetl;

		switch (imematch->type) {
		case ip_match_pattern:
			indirect = 0;
			break;
		case ip_match_indirect:
			indirect = 1;
			break;
		case ip_match_localhost:
			imematch->u.indirect.list = local_addresses;
			indirect = 1;
			break;
		case ip_match_localnets:
			imematch->u.indirect.list = local_networks;
			indirect = 1;
			break;
		default:
			panic("unexpected ime type in ip_match_address()",
			      NULL);
		}
		if (indirect) {
			imeprimitive = NULL;
			matched = ip_match_address_elt(imematch->u.indirect.list,
						       address, &imeprimitive);
			if (matched >= 0) {
				if (imematch->flags & IP_MATCH_NEGATE)
					/* Don't sort */
					return;
			} else 
				continue;
		} else {
			if (ina_onnet(address, imematch->u.direct.address,
				      imematch->u.direct.mask)) {
				if (imematch->flags & IP_MATCH_NEGATE)
					/* Don't sort */
					return;
				else
					imeprimitive = imematch;
			} else
				continue;
		}
		if (imetl != imematch && imematch->next != NULL) {
			/*
			 * Not a bare pattern at the top level, but a two
			 * element list
			 */
			switch (imematch->next->type) {
			case ip_match_pattern:
			case ip_match_localhost:
			case ip_match_localnets:
				imelement = *(imematch->next);
				imelement.next = NULL;
				iml = &imlist;
				iml->first = iml->last = &imelement;
				break;
			case ip_match_indirect:
				iml = imematch->next->u.indirect.list;
				break;
			default:
				panic("unexpected ime type in ip_match_address()",
				      NULL);
			}
		} else if (imeprimitive) {
			imelement = *imeprimitive;
			imelement.next = NULL;
			iml = &imlist;
			iml->first = iml->last = &imelement;
		} else {
			/* Don't sort because we'd just use "any" */
			return;
		}
		sort_rr(cp, eom, ancount, iml);
		break;
	}

	return;
}

static int
sort_rr(u_char *cp, u_char *eom, int ancount, ip_match_list iml) {
	int type, class, dlen, n, c, distance, closest;
	struct in_addr inaddr;
	u_char *rr1 = NULL, *rrbest, *cpstart;

	rr1 = NULL;
	cpstart = cp;
	for (c = ancount; c > 0; --c) {
	    n = dn_skipname(cp, eom);
	    if (n < 0)
		return (1);		/* bogus, stop processing */
	    cp += n;
	    if (cp + QFIXEDSZ > eom)
		return (1);
	    GETSHORT(type, cp);
	    GETSHORT(class, cp);
	    cp += INT32SZ;
	    GETSHORT(dlen, cp);
	    if (dlen > eom - cp)
		return (1);		/* bogus, stop processing */
	    switch (type) {
	    case T_A:
	    	switch (class) {
	    	case C_IN:
	    	case C_HS:
	    		memcpy((char *)&inaddr, cp, INADDRSZ);
			/* Find the address with the minimum distance */
			if (rr1 == NULL) {
				rr1 = cp;
				rrbest = cp;
				closest = distance_of_address(iml, inaddr);
			} else {
				distance = distance_of_address(iml, inaddr);
				if (distance < closest) {
					rrbest = cp;
					closest = distance;
				}
			}
	    		break;
	    	}
	    	break;
	    }
	    cp += dlen;
	}
	if (rr1 != rrbest && rr1 != NULL) {
	    	memcpy((char *)&inaddr, rrbest, INADDRSZ);
		memcpy(rrbest, rr1, INADDRSZ);
	    	memcpy(rr1, (char *)&inaddr, INADDRSZ);
	}
	return (0);
}

/*
 * Just like ip_match_address(), but also returns a pointer to the primitive
 * element that matched.
 */

static int
ip_match_address_elt(ip_match_list iml, struct in_addr address,
		     ip_match_element *imep) {
	ip_match_element ime;
	int ret;
	int indirect;

	INSIST(iml != NULL);
	for (ime = iml->first; ime != NULL; ime = ime->next) {
		switch (ime->type) {
		case ip_match_pattern:
			indirect = 0;
			break;
		case ip_match_indirect:
			indirect = 1;
			break;
		case ip_match_localhost:
			ime->u.indirect.list = local_addresses;
			indirect = 1;
			break;
		case ip_match_localnets:
			ime->u.indirect.list = local_networks;
			indirect = 1;
			break;
		default:
			panic("unexpected ime type in ip_match_address()",
			      NULL);
		}
		if (indirect) {
			ret = ip_match_address_elt(ime->u.indirect.list,
						   address, imep);
			if (ret >= 0) {
				if (ime->flags & IP_MATCH_NEGATE)
					ret = (ret) ? 0 : 1;
				return (ret);
			}
		} else {
			if (ina_onnet(address, ime->u.direct.address,
				      ime->u.direct.mask)) {
				*imep = ime;
				if (ime->flags & IP_MATCH_NEGATE)
					return (0);
				else
					return (1);
			}
		}
	}
	return (-1);
}
