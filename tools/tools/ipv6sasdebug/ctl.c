/*-
 * Copyright (c) 2014 Andrey V. Elsukov <ae@FreeBSD.org>
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
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_var.h>
#define	_WANT_RTENTRY
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <netinet/ip_carp.h>
#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>

#include <err.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern int sock6;
extern int V_ip6_use_deprecated;
extern int V_ip6_prefer_tempaddr;
extern int V_ip6_defhlim;
extern int in6_mask2len(struct in6_addr *, u_char *);
extern struct ifnet* ifnet_byname(const char *name);
static struct carpreq carp[CARP_MAXVHID];
static int has_carp = 0;

void
ifnet_getcarpstatus(const char *name)
{
	struct ifreq req;

	if (has_carp)
		return;
	memset(&req, 0, sizeof(req));
	memset(carp, 0, sizeof(struct carpreq) * CARP_MAXVHID);
	strncpy(req.ifr_name, name, sizeof(req.ifr_name));
	req.ifr_ifru.ifru_data = (caddr_t)carp;
	carp[0].carpr_count = CARP_MAXVHID;
	if (ioctl(sock6, SIOCGVH, &req) < 0)
		return;
	has_carp = 1;
}

int
carp_master(uint8_t vhid)
{
	int i;

	if (has_carp == 0)
		return (0);
	for (i = 0; i < carp[0].carpr_count; i++)
		if (carp[i].carpr_vhid == vhid)
			return (carp[i].carpr_state == CARP_MAXSTATE);
	return (0);
}

int
ifnet_getflags(const char *name, struct sockaddr_in6 *addr)
{
	struct ifreq req;

	memset(&req, 0, sizeof(req));
	strncpy(req.ifr_name, name, sizeof(req.ifr_name));
	if (ioctl(sock6, SIOCGIFFLAGS, &req) < 0)
		err(EXIT_FAILURE, "ioctl(SIOCGIFFLAGS)");
	return ((req.ifr_flags & 0xffff) | (req.ifr_flagshigh << 16));
}

void
ifnet_getndinfo(const char *name, uint32_t *linkmtu, uint32_t *maxmtu,
    uint32_t *flags, uint8_t *chlim)
{
	struct in6_ndireq req;

	strncpy(req.ifname, name, sizeof(req.ifname));
	if (ioctl(sock6, SIOCGIFINFO_IN6, &req) < 0)
		err(EXIT_FAILURE, "ioctl(SIOCGIFINFO_IN6)");
	*linkmtu = req.ndi.linkmtu;
	*maxmtu = req.ndi.maxmtu;
	*flags = req.ndi.flags;
	*chlim = req.ndi.chlim;
}

int
addr_getflags(const char *name, struct sockaddr_in6 *addr)
{
	struct in6_ifreq req;

	strncpy(req.ifr_name, name, sizeof(req.ifr_name));
	req.ifr_ifru.ifru_addr = *addr;
	if (ioctl(sock6, SIOCGIFAFLAG_IN6, &req) < 0)
		err(EXIT_FAILURE, "ioctl(SIOCGIFAFLAG_IN6)");
	return (req.ifr_ifru.ifru_flags6);
}

void
addr_getlifetime(const char *name, struct sockaddr_in6 *addr,
    struct in6_addrlifetime *lt)
{
	struct in6_ifreq req;

	strncpy(req.ifr_name, name, sizeof(req.ifr_name));
	req.ifr_ifru.ifru_addr = *addr;
	if (ioctl(sock6, SIOCGIFALIFETIME_IN6, &req) < 0)
		err(EXIT_FAILURE, "ioctl(SIOCGIFALIFETIME_IN6)");
	*lt = req.ifr_ifru.ifru_lifetime;
}

void
v_getsysctl(void)
{
	size_t len;

	len = sizeof(V_ip6_use_deprecated);
	if (sysctlbyname("net.inet6.ip6.use_deprecated",
	    &V_ip6_use_deprecated, &len, NULL, 0) == -1)
		err(EXIT_FAILURE, "net.inet6.ip6.use_deprecated");

	len = sizeof(V_ip6_prefer_tempaddr);
	if (sysctlbyname("net.inet6.ip6.prefer_tempaddr",
	    &V_ip6_prefer_tempaddr, &len, NULL, 0) == -1)
		err(EXIT_FAILURE, "net.inet6.ip6.prefer_tempaddr");

	len = sizeof(V_ip6_defhlim);
	if (sysctlbyname("net.inet6.ip6.hlim",
	    &V_ip6_defhlim, &len, NULL, 0) == -1)
		err(EXIT_FAILURE, "net.inet6.ip6.hlim");
}

void
addrsel_policy_populate(int add(struct in6_addrpolicy *), int debug)
{
	struct in6_addrpolicy *pp, *p;
	size_t len;
	int mib[] = { CTL_NET, PF_INET6, IPPROTO_IPV6, IPV6CTL_ADDRCTLPOLICY };
	char buf[NI_MAXHOST + IFNAMSIZ + 10];

	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]), NULL, &len, NULL, 0) < 0)
		err(EXIT_FAILURE, "sysctl(IPV6CTL_ADDRCTLPOLICY)");
	if ((pp = malloc(len)) == NULL)
		err(EXIT_FAILURE, "malloc");
	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]), pp, &len, NULL, 0) < 0)
		err(EXIT_FAILURE, "sysctl(IPV6CTL_ADDRCTLPOLICY)");
	if (debug)
		printf("\nIPv6 address selection policy table:\n"
		    "%-30s %5s %5s %8s\n", "Prefix", "Prec", "Label", "Use");
	for (p = pp; p < pp + len/sizeof(*p); p++) {
		add(p);
		if (debug) {
			if (getnameinfo((struct sockaddr *)&p->addr,
			    sizeof(p->addr), buf, sizeof(buf), NULL, 0,
			    NI_NUMERICHOST) != 0)
				continue;
			snprintf(buf, sizeof(buf), "%s/%d", buf,
			    in6_mask2len(&p->addrmask.sin6_addr, NULL));
			printf("%-30s %5d %5d %8llu\n", buf, p->preced,
			    p->label, (unsigned long long)p->use);
		}
	}
	free(pp);
}

struct {
	struct rt_msghdr	m_rtm;
	char			m_space[512];
} m_rtmsg;

void
in6_rtalloc(struct route_in6 *ro, u_int fibnum)
{
	static int seq = 0;
	struct rtentry *rt;
	struct sockaddr_dl *ifp;
	struct sockaddr_in6 *sa6;
	char *cp = m_rtmsg.m_space;
	pid_t pid;
	int i, l, s;

	s = socket(PF_ROUTE, SOCK_RAW, 0);
	if (s < 0)
		err(EXIT_FAILURE, "socket(PF_ROUTE)");
	memset(&m_rtmsg, 0, sizeof(m_rtmsg));

#define rtm m_rtmsg.m_rtm
	rtm.rtm_type = RTM_GET;
	rtm.rtm_version = RTM_VERSION;
	rtm.rtm_seq = ++seq;
	rtm.rtm_addrs = RTA_DST | RTA_IFP;

	sa6 = (struct sockaddr_in6 *)cp;
	*sa6 = ro->ro_dst;
	rtm.rtm_msglen = sizeof(m_rtmsg.m_rtm) + SA_SIZE(sa6);
	l = write(s, (char *)&m_rtmsg, rtm.rtm_msglen);
	if (l < 0)
		err(EXIT_FAILURE, "write()");
	pid = getpid();
	do {
		l = read(s, (char *)&m_rtmsg, sizeof(m_rtmsg));
	} while (l > 0 && (rtm.rtm_seq != seq || rtm.rtm_pid != pid));
	if (l < 0)
		err(EXIT_FAILURE, "read()");
	close(s);
	ro->ro_rt = NULL;
	if (rtm.rtm_errno != 0 || (rtm.rtm_addrs &
	    (RTA_GATEWAY | RTA_IFP)) != (RTA_GATEWAY | RTA_IFP))
		return;
	for (i = 1; i < RTA_IFP; i <<= 1) {
		if ((i & rtm.rtm_addrs) == 0)
			continue;
		if (i == RTA_GATEWAY)
			sa6 = (struct sockaddr_in6 *)cp;
		cp += SA_SIZE(cp);
	}
	ifp = (struct sockaddr_dl *)cp;
	if (sa6->sin6_family != AF_INET6)
		return;
	if (ifp->sdl_family != AF_LINK ||
	    ifp->sdl_nlen == 0)
		return;
	ifp->sdl_data[ifp->sdl_nlen] = '\0';
	if ((rt = calloc(1, sizeof(*rt))) == NULL)
		return;
	if ((rt->rt_gateway = calloc(1, sizeof(*sa6))) == NULL)
		err(EXIT_FAILURE, "calloc()");
	memcpy(rt->rt_gateway, sa6, sizeof(*sa6));
	rt->rt_fibnum = fibnum;
	rt->rt_ifp = ifnet_byname(ifp->sdl_data);
	ro->ro_rt = rt;
}
