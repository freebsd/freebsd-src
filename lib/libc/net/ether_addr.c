/*
 * Copyright (c) 1995
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
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
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
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
 *
 * ethernet address conversion and lookup routines
 *
 * Written by Bill Paul <wpaul@ctr.columbia.edu>
 * Center for Telecommunications Research
 * Columbia University, New York City
 *
 *	$Id: ether_addr.c,v 1.1.4.1 1995/08/25 11:46:33 davidg Exp $
 */


#include <stdio.h>
#include <paths.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#ifdef YP
#include <rpc/rpc.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>
#endif

#ifndef _PATH_ETHERS
#define _PATH_ETHERS "/etc/ethers"
#endif

/*
 * Parse a string of text containing an ethernet address and hostname
 * and separate it into its component parts.
 */
int ether_line(l, e, hostname)
        char *l;
	struct ether_addr *e;
	char *hostname;
{
        int i, o[6];

        i = sscanf(l, "%x:%x:%x:%x:%x:%x %s", &o[0], &o[1], &o[2],
                                              &o[3], &o[4], &o[5],
					      hostname);
	if (i != 7)
                return (i);

        for (i=0; i<6; i++)
                e->octet[i] = o[i];
        return (0);
}

/*
 * Convert an ASCII representation of an ethernet address to
 * binary form.
 */
struct ether_addr *ether_aton(a)
        char *a;
{
        int i;
	static struct ether_addr o;

        i = sscanf(a, "%x:%x:%x:%x:%x:%x", o.octet[0], o.octet[1], o.octet[2],
                                           o.octet[3], o.octet[4], o.octet[5]);
        if (i != 6)
                return (NULL);
        return ((struct ether_addr *)&o);
}

/*
 * Convert a binary representation of an ethernet address to
 * an ASCII string.
 */
char *ether_ntoa(n)
        struct ether_addr *n;
{
        int i;
	static char a[18];

        i = sprintf(a,"%x:%x:%x:%x:%x:%x",n->octet[0],n->octet[1],n->octet[2],
                                          n->octet[3],n->octet[4],n->octet[5]);
        if (i < 11)
                return (NULL);
        return ((char *)&a);
}

/*
 * Map an ethernet address to a hostname. Use either /etc/ethers or
 * NIS/YP.
 */

int ether_ntohost(hostname, e)
	char *hostname;
	struct ether_addr *e;
{
	FILE *fp;
	char buf[BUFSIZ];
	struct ether_addr local_ether;
	char local_host[MAXHOSTNAMELEN];
#ifdef YP
	char *result;
	int resultlen;
	char *ether_a;
	char *yp_domain;
#endif
	if ((fp = fopen(_PATH_ETHERS, "r")) == NULL)
		return (1);

	while (fgets(buf,BUFSIZ,fp)) {
		if (buf[0] == '#')
			continue;
#ifdef YP
		if (buf[0] == '+') {
			if (yp_get_default_domain(&yp_domain))
				continue;
			ether_a = ether_ntoa(e);
			if (yp_match(yp_domain, "ethers.byaddr", ether_a,
				strlen(ether_a), &result, &resultlen)) {
				free(result);
				continue;
			}
			strncpy((char *)&buf, result, resultlen);
				free(result);
		}
#endif
		if (!ether_line(&buf, &local_ether, &local_host)) {
			if (!bcmp((char *)&local_ether.octet[0],
				(char *)&e->octet[0], 6)) {
			/* We have a match */
				strcpy(hostname, (char *)&local_host);
				fclose(fp);
				return(0);
			}
		}
	}
	fclose(fp);
	return (1);
}

/*
 * Map a hostname to an ethernet address using /etc/ethers or
 * NIS/YP.
 */
int ether_hostton(hostname, e)
	char *hostname;
	struct ether_addr *e;
{
	FILE *fp;
	char buf[BUFSIZ];
	struct ether_addr local_ether;
	char local_host[MAXHOSTNAMELEN];
#ifdef YP
	char *result;
	int resultlen;
	char *yp_domain;
#endif
	if ((fp = fopen(_PATH_ETHERS, "r")) == NULL)
		return (1);

	while (fgets(buf,BUFSIZ,fp)) {
		if (buf[0] == '#')
			continue;
#ifdef YP
		if (buf[0] == '+') {
			if (yp_get_default_domain(&yp_domain))
				continue;
			if (yp_match(yp_domain, "ethers.byname", hostname,
				strlen(hostname), &result, &resultlen)) {
				free(result);
				continue;
			}
			strncpy((char *)&buf, result, resultlen);
			free(result);
		}
#endif
		if (!ether_line(&buf, &local_ether, &local_host)) {
			if (!strcmp(hostname, (char *)&local_host)) {
				/* We have a match */
				bcopy((char *)&local_ether.octet[0],
					(char *)&e->octet[0], 6);
				fclose(fp);
				return(0);
			}
		}
	}
	fclose(fp);
	return (1);
}
