/*
 * Copyright (c) 1999, Boris Popov
 * All rights reserved.
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
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */
#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/time.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>

/* IPX */
#include <netipx/ipx.h>
#include <netipx/ipx_if.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <netncp/ncp_lib.h>

#define	IPX_NODE_LEN	6

typedef u_long		IPXNet;
typedef u_short		IPXPort;
typedef union ipx_host	IPXNode;


void
ipx_fprint_node(FILE * file, IPXNode node){
	fprintf(file, "%02X%02X%02X%02X%02X%02X",
		(unsigned char) node.c_host[0],
		(unsigned char) node.c_host[1],
		(unsigned char) node.c_host[2],
		(unsigned char) node.c_host[3],
		(unsigned char) node.c_host[4],
		(unsigned char) node.c_host[5]
	    );
}

void
ipx_fprint_network(FILE * file, const IPXNet net){
	fprintf(file, "%08X", (u_int32_t)ntohl(net));
}

void
ipx_fprint_port(FILE * file, IPXPort port)
{
	fprintf(file, "%04X", ntohs(port));
}

void
ipx_fprint_addr(FILE * file, struct ipx_addr *ipx)
{
	ipx_fprint_network(file, ipx_netlong(*ipx));
	fprintf(file, ":");
	ipx_fprint_node(file, ipx->x_host);
	fprintf(file, ":");
	ipx_fprint_port(file, ipx->x_port);
}

void
ipx_print_node(IPXNode node)
{
	ipx_fprint_node(stdout, node);
}

void
ipx_print_network(IPXNet net)
{
	ipx_fprint_network(stdout, net);
}

void
ipx_print_port(IPXPort port)
{
	ipx_fprint_port(stdout, port);
}

void
ipx_print_addr(struct ipx_addr *ipx)
{
	ipx_fprint_addr(stdout, ipx);
}

int
ipx_sscanf_node(char *buf, unsigned char node[6])
{
	int i;
	int n[6];

	if ((i = sscanf(buf, "%2x%2x%2x%2x%2x%2x",
			&(n[0]), &(n[1]), &(n[2]),
			&(n[3]), &(n[4]), &(n[5]))) != 6)
	{
		return i;
	}
	for (i = 0; i < 6; i++)
	{
		node[i] = n[i];
	}
	return 6;
}

int
ipx_sscanf_saddr(char *buf, struct sockaddr_ipx *target)
{
	char *p;
	struct sockaddr_ipx addr;
	unsigned long sipx_net;

	addr.sipx_family = AF_IPX;
/*!!	addr.sipx_type = NCP_PTYPE;*/

	if (sscanf(buf, "%lx", &sipx_net) != 1)
	{
		return 1;
	}
	((union ipx_net_u*)(&addr.sipx_addr.x_net))->long_e = htonl(sipx_net);
	if ((p = strchr(buf, ':')) == NULL){
		return 1;
	}
	p += 1;
	if (ipx_sscanf_node(p, addr.sipx_node) != 6)
	{
		return 1;
	}
	if ((p = strchr(p, ':')) == NULL)
	{
		return 1;
	}
	p += 1;
	if (sscanf(p, "%hx", &addr.sipx_port) != 1)
	{
		return 1;
	}
	addr.sipx_port = htons(addr.sipx_port);
	*target = addr;
	return 0;
}


void ipx_assign_node(IPXNode *dest, IPXNode *src) {
	memcpy(dest, src, IPX_NODE_LEN);
}


static void	rt_xaddrs __P((caddr_t, caddr_t, struct rt_addrinfo *));
static int	if_ipxscan __P((int addrcount, struct sockaddr_dl *sdl, struct if_msghdr *ifm,
		    struct ifa_msghdr *ifam,struct ipx_addr *addr));

/*
 * Find an IPX interface. 
 * ifname specifies interface name, if NULL search for all interfaces
 *        if ifname[0]='0', also all interfaces, but return its name
 * addr   on input preferred net address can be specified or 0 for any,
 *        on return contains full address (except port)
 * returns 0 if interface was found
 */
int
ipx_iffind(char *ifname,struct ipx_addr *addr){
	char name[32];
	int all=0, flags, foundit = 0, addrcount;
	struct	if_msghdr *ifm, *nextifm;
	struct	ifa_msghdr *ifam;
	struct	sockaddr_dl *sdl;
	char	*buf, *lim, *next;
	size_t	needed;
	int mib[6];
	
	if( ifname!=NULL ) {
	    strncpy(name,ifname,sizeof(name)-1);
	    if( name[0]==0 )
		all=1;
	} else
	    all = 1;

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_IPX;
	mib[4] = NET_RT_IFLIST;
	mib[5] = 0;

	if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0)
		return(1);
	if ((buf = malloc(needed)) == NULL)
		return(1);
	if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0) {
		free(buf);
		return(1);
	}
	lim = buf + needed;

	next = buf;
	while (next < lim) {
		ifm = (struct if_msghdr *)next;
		if (ifm->ifm_type == RTM_IFINFO) {
			sdl = (struct sockaddr_dl *)(ifm + 1);
			flags = ifm->ifm_flags;
		} else {
			fprintf(stderr, "if_ipxfind: out of sync parsing NET_RT_IFLIST\n");
			fprintf(stderr, "expected %d, got %d\n", RTM_IFINFO, ifm->ifm_type);
			fprintf(stderr, "msglen = %d\n", ifm->ifm_msglen);
			fprintf(stderr, "buf:%p, next:%p, lim:%p\n", buf, next, lim);
			free(buf);
			return(1);
		}

		next += ifm->ifm_msglen;
		ifam = NULL;
		addrcount = 0;
		while (next < lim) {
			nextifm = (struct if_msghdr *)next;
			if (nextifm->ifm_type != RTM_NEWADDR)
				break;
			if (ifam == NULL)
				ifam = (struct ifa_msghdr *)nextifm;
			addrcount++;
			next += nextifm->ifm_msglen;
		}

		if (all) {
			if ((flags & IFF_UP) == 0)
				continue; /* not up */
			strncpy(name, sdl->sdl_data, sdl->sdl_nlen);
			name[sdl->sdl_nlen] = '\0';
		} else {
			if (strlen(name) != sdl->sdl_nlen)
				continue; /* not same len */
			if (strncmp(name, sdl->sdl_data, sdl->sdl_nlen) != 0)
				continue; /* not same name */
		}

		foundit=if_ipxscan(addrcount, sdl, ifm, ifam, addr);
		if( foundit ) {
			if( ifname!=NULL && ifname[0]==0) {
			    strncpy(ifname,sdl->sdl_data, sdl->sdl_nlen);
			    ifname[sdl->sdl_nlen]=0;
			}
			break;
		}
	}
	free(buf);

	return foundit ? 0:1;
}


int
if_ipxscan(addrcount, sdl, ifm, ifam, addr)
	int addrcount;
	struct	sockaddr_dl *sdl;
	struct if_msghdr *ifm;
	struct ifa_msghdr *ifam;
	struct ipx_addr *addr;
{
	struct	rt_addrinfo info;
	struct sockaddr_ipx *sipx;
	int s;

	if ((s = socket(AF_IPX, SOCK_DGRAM, 0)) < 0) {
		perror("ifconfig: socket");
		return 0;
	}

	while (addrcount > 0) {
		info.rti_addrs = ifam->ifam_addrs;
		/* Expand the compacted addresses */
		rt_xaddrs((char *)(ifam + 1), ifam->ifam_msglen + (char *)ifam, &info);
		addrcount--;
		ifam = (struct ifa_msghdr *)((char *)ifam + ifam->ifam_msglen);
		if (info.rti_info[RTAX_IFA]->sa_family == AF_IPX) {
			sipx = (struct sockaddr_ipx *)info.rti_info[RTAX_IFA];
			if( ipx_nullnet(sipx->sipx_addr) ) continue;
			if( ipx_nullnet(*addr) || 
			    ipx_neteq(sipx->sipx_addr,*addr) ) {
			    *addr=sipx->sipx_addr;
			    close(s);
			    return(1);
			}
		}
	}
	close(s);
	return(0);
}
/*
 * Expand the compacted form of addresses as returned via the
 * configuration read via sysctl().
 */

#define ROUNDUP(a) \
	((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))
#define ADVANCE(x, n) (x += ROUNDUP((n)->sa_len))

static void
rt_xaddrs(cp, cplim, rtinfo)
	caddr_t cp, cplim;
	struct rt_addrinfo *rtinfo;
{
	struct sockaddr *sa;
	int i;

	memset(rtinfo->rti_info, 0, sizeof(rtinfo->rti_info));
	for (i = 0; (i < RTAX_MAX) && (cp < cplim); i++) {
		if ((rtinfo->rti_addrs & (1 << i)) == 0)
			continue;
		rtinfo->rti_info[i] = sa = (struct sockaddr *)cp;
		ADVANCE(cp, sa);
	}
}

