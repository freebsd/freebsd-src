/* $FreeBSD$ */
/*
 * Copyright (c) 1985, 1989
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

#ifndef lint
static const char sccsid[] = "@(#)getinfo.c	5.26 (Berkeley) 3/21/91";
static const char rcsid[] = "$Id: getinfo.c,v 8.29.8.2 2003/06/02 09:24:39 marka Exp $";
#endif /* not lint */

/*
 ******************************************************************************
 *
 *  getinfo.c --
 *
 *	Routines to create requests to name servers
 *	and interpret the answers.
 *
 *	Adapted from 4.3BSD BIND gethostnamadr.c
 *
 ******************************************************************************
 */

#include "port_before.h"

#include <sys/param.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "port_after.h"

#include <resolv.h>

#include "res.h"

static char *addr_list[MAXADDRS + 1];
static int addr_len[MAXADDRS + 1];
static int addr_type[MAXADDRS + 1];

static char *host_aliases[MAXALIASES];
static int   host_aliases_len[MAXALIASES];
static u_char  hostbuf[MAXDNAME];

typedef struct {
    char *name;
    char *domain[MAXDOMAINS];
    int   numDomains;
    char *address[MAXADDRS];
    char len[MAXADDRS];
    char type[MAXADDRS];
    int   numAddresses;
} ServerTable;

ServerTable server[MAXSERVERS];

typedef union {
    HEADER qb1;
    u_char qb2[NS_MAXMSG];
} querybuf;

typedef union {
    int32_t al;
    char ac;
} align;

#define GetShort(cp)	ns_get16(cp); cp += INT16SZ;


/*
 ******************************************************************************
 *
 *  GetAnswer --
 *
 *	Interprets an answer packet and retrieves the following
 *	information:
 *
 *  Results:
 *      SUCCESS         the info was retrieved.
 *      NO_INFO         the packet did not contain an answer.
 *	NONAUTH		non-authoritative information was found.
 *      ERROR           the answer was malformed.
 *      Other errors    returned in the packet header.
 *
 ******************************************************************************
 */

static int
GetAnswer(union res_sockaddr_union *nsAddrPtr, int queryType,
	  char *msg, int msglen, Boolean iquery, HostInfo *hostPtr,
	  Boolean isServer, Boolean merge)
{
    register HEADER	*headerPtr;
    register const u_char	*cp;
    querybuf		answer;
    char		**aliasPtr;
    u_char		*eom, *bp, *ep;
    char		**addrPtr;
    int			*lenPtr;
    int			*typePtr;
    char		*namePtr;
    char		*dnamePtr;
    int			type, class;
    int			qdcount, ancount, arcount, nscount;
    int			origClass = 0;
    int			numAliases = 0;
    int			numAddresses = 0;
    int			n, i, j, k, l, m;
    int			dlen;
    int			status;
    int			numServers;
    size_t		s;
    Boolean		haveAnswer;
    Boolean		printedAnswers = FALSE;
    int			oldAliases;
    char		**newAliases;
    int			oldServers;
    ServerInfo		**newServers;
    int			oldAddresses;
    AddrInfo	 	**newAddresses;


    /*
     *  If the hostPtr was used before, free up the calloc'd areas.
     */
    if (!merge)
	    FreeHostInfoPtr(hostPtr);

    status = SendRequest(nsAddrPtr, (u_char *)msg, msglen, (u_char *) &answer,
			 sizeof(answer), &n);

    if (status != SUCCESS) {
	    if (res.options & RES_DEBUG2)
		    printf("SendRequest failed\n");
	    return (status);
    }
    eom = (u_char *) &answer + n;

    headerPtr = (HEADER *) &answer;

    if (headerPtr->rcode != NOERROR) {
	return (headerPtr->rcode);
    }

    qdcount = ntohs(headerPtr->qdcount);
    ancount = ntohs(headerPtr->ancount);
    arcount = ntohs(headerPtr->arcount);
    nscount = ntohs(headerPtr->nscount);

    /*
     * If there are no answer, n.s. or additional records
     * then return with an error.
     */
    if (ancount == 0 && nscount == 0 && arcount == 0) {
	return (NO_INFO);
    }


    bp = hostbuf;
    ep = hostbuf + sizeof(hostbuf);
    cp = (u_char *) &answer + HFIXEDSZ;

    /* Skip over question section. */
    while (qdcount-- > 0) {
	    n = dn_skipname(cp, eom);
	    if (n < 0)
		    return (ERROR);
	    cp += n + QFIXEDSZ;
	    if (cp > eom)
		    return (ERROR);
    }

    aliasPtr	= host_aliases;
    addrPtr	= addr_list;
    lenPtr	= addr_len;
    typePtr	= addr_type;
    haveAnswer	= FALSE;

    /*
     * Scan through the answer resource records.
     * Answers for address query types are saved.
     * Other query type answers are just printed.
     */
    if (ancount != 0) {

	if (headerPtr->ad)
	    printf("Answer crypto-validated by server:\n");

	if (!isServer && !headerPtr->aa) {
	    printf("Non-authoritative answer:\n");
	}

	if (queryType != T_A && queryType != T_AAAA &&	/* A6? */
	    !(iquery && queryType == T_PTR)) {
	    while (--ancount >= 0 && cp < eom) {
		if ((cp = Print_rr(cp, (u_char *)&answer,
				   eom, stdout)) == NULL) {
		    return(ERROR);
		}
	    }
	    printedAnswers = TRUE;
	} else {
	    while (--ancount >= 0 && cp < eom) {
		n = dn_expand(answer.qb2, eom, cp, (char *)bp, ep - bp);
		if (n < 0)
		    return(ERROR);
		cp += n;
		if (cp + 3 * INT16SZ + INT32SZ > eom)
			return (ERROR);
		type  = GetShort(cp);
		class = GetShort(cp);
		cp   += INT32SZ;	/* skip TTL */
		dlen  = GetShort(cp);
		if (cp + dlen > eom)
			return (ERROR);
		if (type == T_CNAME) {
		    /*
		     * Found an alias.
		     */
		    cp += dlen;
		    if (aliasPtr >= &host_aliases[MAXALIASES-1]) {
			continue;
		    }
		    *aliasPtr++ = (char *)bp;
		    s = strlen((char *)bp) + 1;
		    host_aliases_len[numAliases] = s;
		    numAliases++;
		    bp += s;
		    continue;
		} else if (type == T_PTR) {
		    /*
		     *  Found a "pointer" to the real name.
		     */
		    n = dn_expand(answer.qb2, eom, cp, (char *)bp, ep - bp);
		    if (n < 0) {
			cp += n;
			continue;
		    }
		    cp += n;
		    s = strlen((char *)bp) + 1;
		    hostPtr->name = Calloc(1, s);
		    memcpy(hostPtr->name, bp, s);
		    haveAnswer = TRUE;
		    break;
		} else if (type != T_A && type != T_AAAA) {
		    cp += dlen;
		    continue;
		}
		if (type == T_A && dlen != INADDRSZ)
			return (ERROR);
		if (type == T_AAAA && dlen != 16)
			return (ERROR);
		if (haveAnswer) {
		    /*
		     * If we've already got 1 address, we aren't interested
		     * in addresses with a different class.
		     */
		    if (class != origClass) {
			cp += dlen;
			continue;
		    }
		} else {
		    /*
		     * First address: record its class so we only save
		     * additonal ones with the same attributes.
		     */
		    origClass = class;
		    if (hostPtr->name == NULL) {
			s = strlen((char *)bp) + 1;
			hostPtr->name = Calloc(1, s);
			memcpy(hostPtr->name, bp, s);
		    }
		}
		bp += (((size_t)bp) % sizeof(align));

		if (bp + dlen >= &hostbuf[sizeof(hostbuf)]) {
		    if (res.options & RES_DEBUG) {
			printf("Size (%d) too big\n", dlen);
		    }
		    break;
		}
		if (numAddresses >= MAXADDRS) {
			printf("MAXADDRS exceeded: skipping address\n");
			cp += dlen;
			continue;
		}
		memcpy(*addrPtr++ = (char *)bp, cp, dlen);
		*lenPtr++ = dlen;
		*typePtr++ = (class == C_IN) ?
				      ((type == T_A) ? AF_INET : AF_INET6) :
				      AF_UNSPEC;
		bp += dlen;
		cp += dlen;
		numAddresses++;
		haveAnswer = TRUE;
	    }
	}
    }

    if ((queryType == T_A || queryType == T_AAAA || queryType == T_PTR) &&
	 haveAnswer) {

	/*
	 *  Go through the alias and address lists and return them
	 *  in the hostPtr variable.
	 */

	oldAliases = 0;
	if (merge && hostPtr->aliases != NULL) {
		while (hostPtr->aliases[oldAliases] != NULL)
			oldAliases++;
	}
	if (numAliases > 0) {
	    newAliases =
		(char **) Calloc(1 + numAliases + oldAliases, sizeof(char *));
	    if (merge && hostPtr->aliases != NULL) {
		memcpy(newAliases, hostPtr->aliases,
		       oldAliases * sizeof(char *)); 
		free(hostPtr->aliases);
	    }
	    hostPtr->aliases = newAliases;
	    k = oldAliases;
	    for (i = 0; i < numAliases; i++) {
		for (l = 0; l < k; l++)
		    if (!strcasecmp(hostPtr->aliases[l], host_aliases[i]))
			break;
		if (l < k)
		    continue;
		hostPtr->aliases[k] = Calloc(1, host_aliases_len[i]);
		memcpy(hostPtr->aliases[k], host_aliases[i],
		       host_aliases_len[i]);
		k++;
	    }
	    hostPtr->aliases[k] = NULL;
	}
	oldAddresses = 0;
	if (merge && hostPtr->addrList != NULL) {
		while (hostPtr->addrList[oldAddresses] != NULL)
			oldAddresses++;
	}
	if (numAddresses > 0) {
	    newAddresses =
		(AddrInfo **)Calloc(1+numAddresses, sizeof(AddrInfo *));
	    if (merge && hostPtr->addrList != NULL) {
		memcpy(newAddresses, hostPtr->addrList,
		       oldAddresses * sizeof(char *)); 
		free(hostPtr->addrList);
	    }
	    hostPtr->addrList = newAddresses;
	    k = oldAddresses;
	    for (i = 0; i < numAddresses; i++) {
		for (l = 0; l < k; l++)
		    if (hostPtr->addrList[l]->addrType == addr_type[i] &&
			hostPtr->addrList[l]->addrLen == addr_len[i] &&
			!memcmp(hostPtr->addrList[l]->addr, addr_list[i],
				addr_len[i]))
			break;
		if (l < k)
		    continue;
		hostPtr->addrList[k] = (AddrInfo*)Calloc(1, sizeof(AddrInfo));
		hostPtr->addrList[k]->addr = Calloc(1, addr_len[i]);
		hostPtr->addrList[k]->addrType = addr_type[i];
		hostPtr->addrList[k]->addrLen = addr_len[i];
		memcpy(hostPtr->addrList[k]->addr, addr_list[i], addr_len[i]);
		k++;
	    }
	    hostPtr->addrList[k] = NULL;
	}
#ifdef verbose
	if (headerPtr->aa || nscount == 0) {
	    hostPtr->servers = NULL;
	    return (SUCCESS);
	}
#else
	hostPtr->servers = NULL;
	return (SUCCESS);
#endif
    }

    /*
     * At this point, for the T_A query type, only empty answers remain.
     * For other query types, additional information might be found
     * in the additional resource records part.
     */

    if (!headerPtr->aa && (queryType != T_A) && (queryType != T_AAAA) &&
	(nscount > 0 || arcount > 0)) {
	if (printedAnswers) {
	    putchar('\n');
	}
	printf("Authoritative answers can be found from:\n");
    }

    cp = res_skip((u_char *)&answer, 2, eom);

    numServers = 0;
    if (queryType != T_A && queryType != T_AAAA) {
	/*
	 * If we don't need to save the record, just print it.
	 */
	while (--nscount >= 0 && cp < eom) {
	    if ((cp = Print_rr(cp, (u_char *) &answer, 
			       eom, stdout)) == NULL) {
		return(ERROR);
	    }
	}
    } else {
	while (--nscount >= 0 && cp < eom) {
	    /*
	     *  Go through the NS records and retrieve the names of hosts
	     *  that serve the requested domain.
	     */

	    n = dn_expand(answer.qb2, eom, cp, (char *)bp, ep - bp);
	    if (n < 0) {
		return(ERROR);
	    }
	    cp += n;
	    s = strlen((char *)bp) + 1;
	    dnamePtr = Calloc(1, s);   /* domain name */
	    memcpy(dnamePtr, bp, s);

	    if (cp + 3 * INT16SZ + INT32SZ > eom)
		    return (ERROR);
	    type  = GetShort(cp);
	    class = GetShort(cp);
	    cp   += INT32SZ;	/* skip TTL */
	    dlen  = GetShort(cp);
	    if (cp + dlen > eom)
		    return (ERROR);

	    if (type != T_NS) {
		cp += dlen;
	    } else {
		Boolean	found;

		n = dn_expand(answer.qb2, eom, cp, (char *)bp, ep - bp);
		if (n < 0) {
		    return(ERROR);
		}
		cp += n;
		s = strlen((char *)bp) + 1;
		namePtr = Calloc(1, s); /* server host name */
		memcpy(namePtr, bp, s);

		/*
		 * Store the information keyed by the server host name.
		 */
		found = FALSE;
		for (j = 0; j < numServers; j++) {
		    if (strcasecmp(namePtr, server[j].name) == 0) {
			found = TRUE;
			free(namePtr);
			break;
		    }
		}
		if (found) {
		    server[j].numDomains++;
		    if (server[j].numDomains <= MAXDOMAINS) {
			server[j].domain[server[j].numDomains-1] = dnamePtr;
		    }
		} else {
		    if (numServers >= MAXSERVERS) {
			break;
		    }
		    server[numServers].name = namePtr;
		    server[numServers].domain[0] = dnamePtr;
		    server[numServers].numDomains = 1;
		    server[numServers].numAddresses = 0;
		    numServers++;
		}
	    }
	}
    }

    /*
     * Additional resource records contain addresses of servers.
     */
    cp = res_skip((u_char*)&answer, 3, eom);

    if (queryType != T_A && queryType != T_AAAA) {
	/*
	 * If we don't need to save the record, just print it.
	 */
	while (--arcount >= 0 && cp < eom) {
	    if ((cp = Print_rr(cp, (u_char *) &answer,
			       eom, stdout)) == NULL) {
		return(ERROR);
	    }
	}
    } else {
	while (--arcount >= 0 && cp < eom) {
	    n = dn_expand(answer.qb2, eom, cp, (char *)bp, ep - bp);
	    if (n < 0) {
		break;
	    }
	    cp   += n;
	    if (cp + 3 * INT16SZ + INT32SZ > eom)
		    return (ERROR);
	    type  = GetShort(cp);
	    class = GetShort(cp);
	    cp   += INT32SZ;	/* skip TTL */
	    dlen  = GetShort(cp);
	    if (cp + dlen > eom)
		    return (ERROR);

	    if (type != T_A && type != T_AAAA)  {
		cp += dlen;
		continue;
	    } else {
		if (type == T_A && dlen != INADDRSZ)
			return (ERROR);
		if (type == T_AAAA && dlen != 16)
			return (ERROR);
		for (j = 0; j < numServers; j++) {
		    if (strcasecmp((char *)bp, server[j].name) == 0) {
			server[j].numAddresses++;
			if (server[j].numAddresses <= MAXADDRS) {
			    server[j].address[server[j].numAddresses-1] = 
				    				Calloc(1,dlen);
			    memcpy(server[j].address[server[j].numAddresses-1],
				   cp, dlen);
			    server[j].len[server[j].numAddresses-1] = dlen;
			    server[j].type[server[j].numAddresses-1] =
					   (type == T_A) ? AF_INET : AF_INET6;
			    break;
			}
		    }
		}
		cp += dlen;
	    }
	}
    }

    /*
     * If we are returning name server info, transfer it to the hostPtr.
     */
    oldServers = 0;
    if (merge && hostPtr->servers != NULL) {
	while (hostPtr->servers[oldServers] != NULL)
	    oldServers++;
    }
    if (numServers > 0) {
	newServers = (ServerInfo **) Calloc(numServers+oldServers+1,
				       sizeof(ServerInfo *));
	if (merge && hostPtr->servers != NULL) {
		memcpy(newServers, hostPtr->servers,
		       oldServers * sizeof(ServerInfo *));
		free(hostPtr->servers);
	}
	hostPtr->servers = newServers;
	k = oldServers;
	for (i = 0; i < numServers; i++) {
	    for (l = 0; l < k; l++)
		if (!strcasecmp(hostPtr->servers[l]->name, server[i].name))
		    break;
	    if (l < k) { 
		free(server[i].name);
		for (j = 0; j < server[i].numDomains; j++)
		     free(server[i].domain[j]);
	    } else {
		hostPtr->servers[l] = (ServerInfo *)
				 Calloc(1, sizeof(ServerInfo));
		hostPtr->servers[l]->name = server[i].name;
		k++;

		hostPtr->servers[l]->domains = (char **)
				Calloc(server[i].numDomains+1,sizeof(char *));
		for (j = 0; j < server[i].numDomains; j++) {
		    hostPtr->servers[l]->domains[j] = server[i].domain[j];
		}
		hostPtr->servers[l]->domains[j] = NULL;
	    }


	    oldAddresses = 0;
	    if (merge && hostPtr->servers[l]->addrList != NULL)
		while (hostPtr->servers[l]->addrList[oldAddresses] != NULL)
			oldAddresses++;
	    newAddresses = (AddrInfo **)
			Calloc(server[i].numAddresses+oldAddresses+1,
			       sizeof(AddrInfo *));
	    if (merge && hostPtr->servers[l]->addrList != NULL) {
		memcpy(newAddresses, hostPtr->servers[l]->addrList,
		       sizeof(AddrInfo *) * oldAddresses);
		free(hostPtr->servers[l]->addrList);
	    }
	    hostPtr->servers[l]->addrList = newAddresses;
	    m = oldAddresses;
	    for (j = 0; j < server[l].numAddresses; j++) {
		for (n = 0; n < m; n++)
		    if (hostPtr->servers[l]->addrList[n]->addrType ==
			server[i].type[j] &&
			hostPtr->servers[l]->addrList[n]->addrLen ==
			server[i].len[j] &&
			!memcmp(hostPtr->servers[l]->addrList[n]->addr,
				server[i].address[j], server[i].len[j]))
			break;
		if (n < m) {
		    free(server[i].address[j]);
		    continue;
		}
		hostPtr->servers[l]->addrList[m] =
				 (AddrInfo*)Calloc(1, sizeof(AddrInfo));
		hostPtr->servers[l]->addrList[m]->addr =
					 server[i].address[j];
		hostPtr->servers[l]->addrList[m]->addrType =
					 server[i].type[j];
		hostPtr->servers[l]->addrList[m]->addrLen =
					 server[i].len[j];
		m++;
	    }
	    hostPtr->servers[l]->addrList[m] = NULL;
	}
	hostPtr->servers[k] = NULL;
    }

    switch (queryType) {
	case T_AAAA:
	case T_A:
		return NONAUTH;
	case T_PTR:
		if (iquery)
			return NO_INFO;
		/* fall through */
	default:
		return SUCCESS;
    }
}

/*
*******************************************************************************
*
*  GetHostInfo --
*
*	Retrieves host name, address and alias information
*	for a domain.
*
*	Algorithm from res_nsearch().
*
*  Results:
*	ERROR		- res_nmkquery failed.
*	+ return values from GetAnswer()
*
*******************************************************************************
*/

int
GetHostInfoByName(nsAddrPtr, queryClass, queryType, name, hostPtr, isServer,
		  merge)
    union res_sockaddr_union *nsAddrPtr;
    int			queryClass;
    int			queryType;
    const char		*name;
    HostInfo		*hostPtr;
    Boolean		isServer;
    Boolean		merge;
{
    int			n;
    register int	result;
    register char	**domain;
    const char		*cp;
    Boolean		got_nodata = FALSE;
    union res_sockaddr_union ina;
    Boolean		tried_as_is = FALSE;
    char		tmp[NS_MAXDNAME];

    /* Catch explicit addresses */
    if ((queryType == T_A) && IsAddr(name, &ina)) {
	hostPtr->name = Calloc(strlen(name)+3, 1);
	(void)sprintf(hostPtr->name,"[%s]",name);
	switch (ina.sin.sin_family) {
	case AF_INET:
	    hostPtr->aliases = NULL;
	    hostPtr->servers = NULL;
	    hostPtr->addrList = (AddrInfo **)Calloc(2, sizeof(AddrInfo *));
	    hostPtr->addrList[0] = (AddrInfo *)Calloc(1, sizeof(AddrInfo));
	    hostPtr->addrList[0]->addr = Calloc(INT32SZ, sizeof(char));
	    memcpy(hostPtr->addrList[0]->addr, &ina.sin.sin_addr, INADDRSZ);
	    hostPtr->addrList[0]->addrType = AF_INET;
	    hostPtr->addrList[0]->addrLen = INADDRSZ;
	    hostPtr->addrList[1] = NULL;
	    break;
	case AF_INET6:
	    hostPtr->aliases = NULL;
	    hostPtr->servers = NULL;
	    hostPtr->addrList = (AddrInfo **)Calloc(2, sizeof(AddrInfo *));
	    hostPtr->addrList[0] = (AddrInfo *)Calloc(1, sizeof(AddrInfo));
	    hostPtr->addrList[0]->addr = Calloc(1, 16);
	    memcpy(hostPtr->addrList[0]->addr, &ina.sin6.sin6_addr, 16);
	    hostPtr->addrList[0]->addrType = AF_INET6;
	    hostPtr->addrList[0]->addrLen = 16;
	    hostPtr->addrList[1] = NULL;
	    break;
	}
	return(SUCCESS);
    }

    result = NXDOMAIN;
    for (cp = name, n = 0; *cp; cp++)
	    if (*cp == '.')
		    n++;
    if (n == 0 && (cp = res_hostalias(&res, name, tmp, sizeof tmp))) {
	    printf("Aliased to \"%s\"\n\n", cp);
	    return (GetHostDomain(nsAddrPtr, queryClass, queryType,
		    cp, (char *)NULL, hostPtr, isServer, merge));
    }

    /*
     * If there are dots in the name already, let's just give it a try
     * 'as is'.  The threshold can be set with the "ndots" option.
     */
    if (n >= (int)res.ndots) {
	    result = GetHostDomain(nsAddrPtr, queryClass, queryType,
				   name, (char *)NULL, hostPtr, isServer,
				   merge);
            if (result == SUCCESS)
	        return (result);
	    if (result == NO_INFO)
		got_nodata++;
            tried_as_is++;
    }

    /*
     * We do at least one level of search if
     *	- there is no dot and RES_DEFNAME is set, or
     *	- there is at least one dot, there is no trailing dot,
     *	  and RES_DNSRCH is set.
     */
    if ((n == 0 && (res.options & RES_DEFNAMES) != 0) ||
       (n != 0 && *--cp != '.' && (res.options & RES_DNSRCH) != 0))
	 for (domain = res.dnsrch; *domain != NULL; domain++) {
	    result = GetHostDomain(nsAddrPtr, queryClass, queryType,
				   name, *domain, hostPtr, isServer,
				   merge);
	    /*
	     * If no server present, give up.
	     * If name isn't found in this domain,
	     * keep trying higher domains in the search list
	     * (if that's enabled).
	     * On a NO_INFO error, keep trying, otherwise
	     * a wildcard entry of another type could keep us
	     * from finding this entry higher in the domain.
	     * If we get some other error (negative answer or
	     * server failure), then stop searching up,
	     * but try the input name below in case it's fully-qualified.
	     */
	    if (result == SUCCESS || result == NO_RESPONSE)
		    return result;
	    if (result == NO_INFO)
		    got_nodata++;
	    if ((result != NXDOMAIN && result != NO_INFO) ||
		(res.options & RES_DNSRCH) == 0)
		    break;
	}
    /* if we have not already tried the name "as is", do that now.
     * note that we do this regardless of how many dots were in the
     * name or whether it ends with a dot.
     */
    if (!tried_as_is &&
	(result = GetHostDomain(nsAddrPtr, queryClass, queryType,
				name, (char *)NULL, hostPtr, isServer, merge)
	 ) == SUCCESS)
	    return (result);
    if (got_nodata)
	result = NO_INFO;
    return (result);
}

/*
 * Perform a query on the concatenation of name and domain,
 * removing a trailing dot from name if domain is NULL.
 */
int
GetHostDomain(nsAddrPtr, queryClass, queryType, name, domain, hostPtr,
	      isServer, merge)
    union res_sockaddr_union	*nsAddrPtr;
    int			queryClass;
    int			queryType;
    const char		*name;
    char		*domain;
    HostInfo		*hostPtr;
    Boolean		isServer;
    Boolean		merge;
{
    querybuf buf;
    char nbuf[2*MAXDNAME+2];
    const char *longname = nbuf;
    int n;

    if (domain == NULL) {
	    /*
	     * Check for trailing '.';
	     * copy without '.' if present.
	     */
	    n = strlen(name) - 1;
	    if (name[n] == '.' && n < (int)sizeof(nbuf) - 1) {
		    memcpy(nbuf, name, n);
		    nbuf[n] = '\0';
	    } else
		    longname = name;
    } else {
	    (void)sprintf(nbuf, "%.*s.%.*s",
		    MAXDNAME, name, MAXDNAME, domain);
	    longname = nbuf;
    }
    n = res_nmkquery(&res, QUERY, longname, queryClass, queryType,
		     NULL, 0, 0, buf.qb2, sizeof(buf));
    if (n < 0) {
	if (res.options & RES_DEBUG) {
	    printf("Res_nmkquery failed\n");
	}
	return (ERROR);
    }

    n = GetAnswer(nsAddrPtr, queryType, (char *)&buf, n, 0, hostPtr,
		  isServer, merge);

    /*
     * GetAnswer didn't find a name, so set it to the specified one.
     */
    if (n == NONAUTH) {
	if (hostPtr->name == NULL) {
	    size_t len = strlen(longname) + 1;

	    hostPtr->name = Calloc(len, sizeof(char));
	    memcpy(hostPtr->name, longname, len);
	}
    }
    return(n);
}


/*
*******************************************************************************
*
*  GetHostInfoByAddr --
*
*	Performs a PTR lookup in in-addr.arpa to find the host name
*	that corresponds to the given address.
*
*  Results:
*	ERROR		- res_nmkquery failed.
*	+ return values from GetAnswer()
*
*******************************************************************************
*/

int
GetHostInfoByAddr(union res_sockaddr_union *nsAddrPtr,
		  union res_sockaddr_union *address,
		  HostInfo * hostPtr)
{
    int		n;
    querybuf	buf;
    char	qbuf[MAXDNAME];
    char	qbuf2[MAXDNAME];
    char	*p = NULL;
    int		ismapped = 0;

    switch (address->sin.sin_family) {
    case AF_INET:
        p = (char *) &address->sin.sin_addr.s_addr;
    mapped:
	(void)sprintf(qbuf, "%u.%u.%u.%u.in-addr.arpa",
		      ((unsigned)p[3 + (ismapped ? 12 : 0)] & 0xff),
		      ((unsigned)p[2 + (ismapped ? 12 : 0)] & 0xff),
		      ((unsigned)p[1 + (ismapped ? 12 : 0)] & 0xff),
		      ((unsigned)p[0 + (ismapped ? 12 : 0)] & 0xff));
	break;
    case AF_INET6:
	p = (char *)address->sin6.sin6_addr.s6_addr;
	if (IN6_IS_ADDR_V4MAPPED(&address->sin6.sin6_addr) ||
	    IN6_IS_ADDR_V4COMPAT(&address->sin6.sin6_addr)) {
		ismapped = 1;
		goto mapped;
	}
	(void)sprintf(qbuf,
		      "%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x."
		      "%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x."
		      "ip6.arpa",
		      p[15] & 0xf, (p[15] >> 4) & 0xf,
		      p[14] & 0xf, (p[14] >> 4) & 0xf,
		      p[13] & 0xf, (p[13] >> 4) & 0xf,
		      p[12] & 0xf, (p[12] >> 4) & 0xf,
		      p[11] & 0xf, (p[11] >> 4) & 0xf,
		      p[10] & 0xf, (p[10] >> 4) & 0xf,
		      p[9] & 0xf, (p[9] >> 4) & 0xf,
		      p[8] & 0xf, (p[8] >> 4) & 0xf,
		      p[7] & 0xf, (p[7] >> 4) & 0xf,
		      p[6] & 0xf, (p[6] >> 4) & 0xf,
		      p[5] & 0xf, (p[5] >> 4) & 0xf,
		      p[4] & 0xf, (p[4] >> 4) & 0xf,
		      p[3] & 0xf, (p[3] >> 4) & 0xf,
		      p[2] & 0xf, (p[2] >> 4) & 0xf,
		      p[1] & 0xf, (p[1] >> 4) & 0xf,
		      p[0] & 0xf, (p[0] >> 4) & 0xf);
	(void)sprintf(qbuf2,
		      "%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x."
		      "%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x."
		      "ip6.int",
		      p[15] & 0xf, (p[15] >> 4) & 0xf,
		      p[14] & 0xf, (p[14] >> 4) & 0xf,
		      p[13] & 0xf, (p[13] >> 4) & 0xf,
		      p[12] & 0xf, (p[12] >> 4) & 0xf,
		      p[11] & 0xf, (p[11] >> 4) & 0xf,
		      p[10] & 0xf, (p[10] >> 4) & 0xf,
		      p[9] & 0xf, (p[9] >> 4) & 0xf,
		      p[8] & 0xf, (p[8] >> 4) & 0xf,
		      p[7] & 0xf, (p[7] >> 4) & 0xf,
		      p[6] & 0xf, (p[6] >> 4) & 0xf,
		      p[5] & 0xf, (p[5] >> 4) & 0xf,
		      p[4] & 0xf, (p[4] >> 4) & 0xf,
		      p[3] & 0xf, (p[3] >> 4) & 0xf,
		      p[2] & 0xf, (p[2] >> 4) & 0xf,
		      p[1] & 0xf, (p[1] >> 4) & 0xf,
		      p[0] & 0xf, (p[0] >> 4) & 0xf);
	break;
    }
    n = res_nmkquery(&res, QUERY, qbuf, C_IN, T_PTR, NULL, 0, NULL,
		     buf.qb2, sizeof buf);
    if (n < 0) {
	if (res.options & RES_DEBUG) {
	    printf("res_nmkquery() failed\n");
	}
	return (ERROR);
    }
    n = GetAnswer(nsAddrPtr, T_PTR, (char *) &buf, n, 1, hostPtr, 1, 0);
    if (n == SUCCESS) {
	switch (address->sin.sin_family) {
	case AF_INET:
	    hostPtr->addrList = (AddrInfo **)Calloc(2, sizeof(AddrInfo *));
	    hostPtr->addrList[0] = (AddrInfo *)Calloc(1, sizeof(AddrInfo));
	    hostPtr->addrList[0]->addr = Calloc(INT32SZ, sizeof(char));
	    memcpy(hostPtr->addrList[0]->addr, p, INADDRSZ);
	    hostPtr->addrList[0]->addrType = AF_INET;
	    hostPtr->addrList[0]->addrLen = 4;
	    hostPtr->addrList[1] = NULL;
	    break;
	case AF_INET6:
	    hostPtr->addrList = (AddrInfo **)Calloc(2, sizeof(AddrInfo *));
	    hostPtr->addrList[0] = (AddrInfo *)Calloc(1, sizeof(AddrInfo));
	    hostPtr->addrList[0]->addr = Calloc(16, sizeof(char));
	    memcpy(hostPtr->addrList[0]->addr, p, 16);
	    hostPtr->addrList[0]->addrType = AF_INET6;
	    hostPtr->addrList[0]->addrLen = 16;
	    hostPtr->addrList[1] = NULL;
	    break;
	}
    }
    if (n == SUCCESS || ismapped || address->sin.sin_family != AF_INET6)
	    return n;
    n = res_nmkquery(&res, QUERY, qbuf2, C_IN, T_PTR, NULL, 0, NULL,
		     buf.qb2, sizeof buf);
    if (n < 0) {
	if (res.options & RES_DEBUG) {
	    printf("res_nmkquery() failed\n");
	}
	return (ERROR);
    }
    n = GetAnswer(nsAddrPtr, T_PTR, (char *) &buf, n, 1, hostPtr, 1, 0);
    if (n == SUCCESS) {
	hostPtr->addrList = (AddrInfo **)Calloc(2, sizeof(AddrInfo *));
	hostPtr->addrList[0] = (AddrInfo *)Calloc(1, sizeof(AddrInfo));
	hostPtr->addrList[0]->addr = Calloc(16, sizeof(char));
	memcpy(hostPtr->addrList[0]->addr, p, 16);
	hostPtr->addrList[0]->addrType = AF_INET6;
	hostPtr->addrList[0]->addrLen = 16;
	hostPtr->addrList[1] = NULL;
    }
    return n;
}

/*
*******************************************************************************
*
*  FreeHostInfoPtr --
*
*	Deallocates all the calloc'd areas for a HostInfo variable.
*
*******************************************************************************
*/

void
FreeHostInfoPtr(hostPtr)
    register HostInfo *hostPtr;
{
    int i, j;

    if (hostPtr->name != NULL) {
	free(hostPtr->name);
	hostPtr->name = NULL;
    }

    if (hostPtr->aliases != NULL) {
	i = 0;
	while (hostPtr->aliases[i] != NULL) {
	    free(hostPtr->aliases[i]);
	    i++;
	}
	free((char *)hostPtr->aliases);
	hostPtr->aliases = NULL;
    }

    if (hostPtr->addrList != NULL) {
	i = 0;
	while (hostPtr->addrList[i] != NULL) {
	    free(hostPtr->addrList[i]->addr);
	    free(hostPtr->addrList[i]);
	    i++;
	}
	free((char *)hostPtr->addrList);
	hostPtr->addrList = NULL;
    }

    if (hostPtr->servers != NULL) {
	i = 0;
	while (hostPtr->servers[i] != NULL) {

	    if (hostPtr->servers[i]->name != NULL) {
		free(hostPtr->servers[i]->name);
	    }

	    if (hostPtr->servers[i]->domains != NULL) {
		j = 0;
		while (hostPtr->servers[i]->domains[j] != NULL) {
		    free(hostPtr->servers[i]->domains[j]);
		    j++;
		}
		free((char *)hostPtr->servers[i]->domains);
	    }

	    if (hostPtr->servers[i]->addrList != NULL) {
		j = 0;
		while (hostPtr->servers[i]->addrList[j] != NULL) {
		    free(hostPtr->servers[i]->addrList[j]->addr);
		    free(hostPtr->servers[i]->addrList[j]);
		    j++;
		}
		free((char *)hostPtr->servers[i]->addrList);
	    }
	    free((char *)hostPtr->servers[i]);
	    i++;
	}
	free((char *)hostPtr->servers);
	hostPtr->servers = NULL;
    }
}
