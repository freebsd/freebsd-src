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
#include <sys/socket.h>
#include <arpa/inet.h>

#include <err.h>
#include <ifaddrs.h>
#include <libutil.h>
#include <netdb.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include "stub.h"

static SLIST_HEAD(, ifnet) iflist = SLIST_HEAD_INITIALIZER(iflist);
const struct sockaddr_in6 sa6_any = { sizeof(sa6_any), AF_INET6, 0, 0,
    IN6ADDR_ANY_INIT, 0 };
struct in6_ifaddrhead V_in6_ifaddrhead = TAILQ_HEAD_INITIALIZER(V_in6_ifaddrhead);
int V_ip6_use_deprecated;
int V_ip6_prefer_tempaddr;
int V_ip6_defhlim;
int sock6;

const char *
ip6_sprintf(char *buf, const struct in6_addr *addr)
{

	return (inet_ntop(AF_INET6, addr, buf, INET6_ADDRSTRLEN));
}

struct ifnet*
ifnet_byindex(uint32_t index)
{
	struct ifnet *ifp;

	SLIST_FOREACH(ifp, &iflist, if_link)
	    if (ifp->if_index == index)
		    return (ifp);
	return (NULL);
}

struct ifnet*
ifnet_byname(const char *name)
{
	struct ifnet *ifp;

	SLIST_FOREACH(ifp, &iflist, if_link)
	    if (strncmp(name, ifp->if_xname, IFNAMSIZ) == 0)
		    return (ifp);
	ifp = calloc(1, sizeof(struct ifnet));
	if (ifp == NULL)
		return (NULL);
	SLIST_INSERT_HEAD(&iflist, ifp, if_link);
	TAILQ_INIT(&ifp->if_addrhead);
	strlcpy(ifp->if_xname, name, IFNAMSIZ);
	ifp->if_flags = ifnet_getflags(name);
	ifp->if_index = if_nametoindex(name);
	ifnet_getndinfo(name, &ifp->if_ndifinfo.linkmtu,
	    &ifp->if_ndifinfo.maxmtu, &ifp->if_ndifinfo.flags,
	    &ifp->if_ndifinfo.chlim);
	ifnet_getcarpstatus(name);
	return (ifp);
}

static void
addr_add(char *name, struct ifaddrs *ifa)
{
	struct ifnet *ifp;
	struct in6_ifaddr *ia;
	struct sockaddr_in6 *addr;
	struct if_data *ifd;

	ifp = ifnet_byname(name);
	if (ifp == NULL)
		return;
	addr = (struct sockaddr_in6 *)ifa->ifa_addr;
	ia = calloc(1, sizeof(struct in6_ifaddr));
	TAILQ_INSERT_TAIL(&ifp->if_addrhead, &ia->ia_ifa, ifa_link);
	TAILQ_INSERT_TAIL(&V_in6_ifaddrhead, ia, ia_link);
	ia->ia_ifa.ifa_addr = (struct sockaddr *)&ia->ia_addr;
	ia->ia_ifp = ifp;
	ia->ia_addr = *addr;
	ia->ia6_flags = addr_getflags(name, addr);
	if (ifa->ifa_data != NULL) {
		ifd = ifa->ifa_data;
		ia->ia_ifa.ifa_carp = ifd->ifi_vhid;
	}
	addr_getlifetime(name, addr, &ia->ia6_lifetime);
}

static int
addrselpolicy_add(struct in6_addrpolicy *p)
{

	return (in6_src_ioctl(SIOCAADDRCTL_POLICY, (caddr_t)p));
}

static void
debug_list(void)
{
	struct ifnet *ifp;
	struct in6_ifaddr *ia;
	struct ifaddr *ifa;
	char buf[NI_MAXHOST + IFNAMSIZ + 1];

	printf("List of IPv6 interfaces:\n");
	SLIST_FOREACH(ifp, &iflist, if_link) {
		printf("%s: <%04x>\n", ifp->if_xname, ifp->if_flags);
		printf("\tlinkmtu %d, maxmtu %d, hlim %d, ND <%04x>\n",
		    ifp->if_ndifinfo.linkmtu, ifp->if_ndifinfo.maxmtu,
		    ifp->if_ndifinfo.chlim, ifp->if_ndifinfo.flags);
		TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link)
			printf("\tinet6 %s\n", ip6_sprintf(buf,
			    &((struct sockaddr_in6 *)
				    ifa->ifa_addr)->sin6_addr));
	}
	printf("\nList of IPv6 addresses:\n");
	TAILQ_FOREACH(ia, &V_in6_ifaddrhead, ia_link) {
		getnameinfo((struct sockaddr *)&ia->ia_addr, sizeof(ia->ia_addr),
		    buf, sizeof(buf), NULL, 0, NI_NUMERICHOST);
		printf("%s <%04x>\n", buf, ia->ia6_flags);
	}
	printf("\nSysctl variables:\n");
	printf("%s: %d\n", "net.inet6.ip6.use_deprecated", V_ip6_use_deprecated);
	printf("%s: %d\n", "net.inet6.ip6.prefer_tempaddr", V_ip6_prefer_tempaddr);
	printf("%s: %d\n", "net.inet6.ip6.hlim", V_ip6_defhlim);
}

static void
usage(void)
{

	printf("ipv6sasdebug [-vh] <IPv6 destination address>\n");
	printf("\t-v	show collected information related to IPv6 SAS\n");
	printf("\t-h	show usage message\n");
	exit(1);
}

int
main(int argc, char **argv)
{
	struct addrinfo hints, *res, *res0;
	struct sockaddr_in6 dst;
	struct in6_addr src;
	struct ifaddrs *ifap, *ifa;
	struct ifnet *ifp;
	int debug, ch, error;
	char buf[INET6_ADDRSTRLEN];

	debug = 0;
	while ((ch = getopt(argc, argv, ":vh")) != -1) {
		switch (ch) {
		case 'v':
			debug = 1;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET6;
	hints.ai_flags = AI_NUMERICHOST;
	error = getaddrinfo(argv[0], NULL, &hints, &res0);
	if (error != 0)
		err(EXIT_FAILURE, "%s", gai_strerror(error));
	for (res = res0; res; res = res->ai_next)
		if (res->ai_addr->sa_family == AF_INET6) {
			dst = *(struct sockaddr_in6 *)res->ai_addr;
			break;
		}
	freeaddrinfo(res0);

	v_getsysctl();
	sock6 = socket(AF_INET6, SOCK_DGRAM, 0);
	if (sock6 < 0)
		err(EXIT_FAILURE, "socket");
	if (getifaddrs(&ifap) != 0)
		err(EXIT_FAILURE, "getifaddrs");
	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;
		addr_add(ifa->ifa_name, ifa);
	}
	freeifaddrs(ifap);
	close(sock6);

	if (debug)
		debug_list();
	/* Populate the address selection policy table */
	addrsel_policy_init();
	addrsel_policy_populate(addrselpolicy_add, debug);

	ifp = NULL;
	error = in6_selectsrc(&dst, NULL, NULL, NULL, NULL, &ifp, &src);
	if (error == 0) {
		printf("\nin6_selectsrc returned %s address and %s"
		    " outgoing interface.\n", ip6_sprintf(buf, &src),
		    ifp->if_xname);
	} else {
		printf("in6_selectsrc() failed\n");
	}
	return (0);
}
