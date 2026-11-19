/*
 * Copyright (c) 2026 Maxim Konovalov <maxim@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Argument checks of the IPv6 address ioctls handled by in6_control().
 */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>
#include <arpa/inet.h>

#include <errno.h>
#include <ifaddrs.h>
#include <string.h>
#include <unistd.h>

#include <atf-c.h>

#define	IFNAME	"lo0"
#define	ADDR	"2001:db8::1"
#define	DSTADDR	"2001:db8::2"
#define	MASK128	"ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff"
#define	BADLEN	255

/*
 * The test cases change lo0, so run them only in a VNET jail of their
 * own, as the Makefile sets up, never in the host's network stack.
 */
static void
require_vnet_jail(void)
{
	size_t len;
	int vnet;

	len = sizeof(vnet);
	if (sysctlbyname("security.jail.vnet", &vnet, &len, NULL, 0) != 0 ||
	    vnet != 1)
		atf_tc_skip("not running in a VNET jail of its own");
}

static void
sin6_init(struct sockaddr_in6 *sin6, const char *addr, int len)
{
	memset(sin6, 0, sizeof(*sin6));
	sin6->sin6_family = AF_INET6;
	sin6->sin6_len = len;
	ATF_REQUIRE(inet_pton(AF_INET6, addr, &sin6->sin6_addr) == 1);
}

/*
 * Fill in a request to add ADDR/128 to IFNAME with DSTADDR as the
 * destination address, whose sin6_len is set to dstlen.
 */
static void
aliasreq_init(struct in6_aliasreq *ifra, int dstlen)
{
	memset(ifra, 0, sizeof(*ifra));
	strlcpy(ifra->ifra_name, IFNAME, sizeof(ifra->ifra_name));
	sin6_init(&ifra->ifra_addr, ADDR, sizeof(struct sockaddr_in6));
	sin6_init(&ifra->ifra_dstaddr, DSTADDR, dstlen);
	sin6_init(&ifra->ifra_prefixmask, MASK128,
	    sizeof(struct sockaddr_in6));
	ifra->ifra_flags = IN6_IFF_NODAD;
	ifra->ifra_lifetime.ia6t_vltime = ND6_INFINITE_LIFETIME;
	ifra->ifra_lifetime.ia6t_pltime = ND6_INFINITE_LIFETIME;
}

static void
delete_addr(int s)
{
	struct in6_ifreq ifr;

	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, IFNAME, sizeof(ifr.ifr_name));
	sin6_init(&ifr.ifr_addr, ADDR, sizeof(struct sockaddr_in6));
	ATF_REQUIRE_MSG(ioctl(s, SIOCDIFADDR_IN6, &ifr) == 0,
	    "SIOCDIFADDR_IN6: %s", strerror(errno));
}

/*
 * Return the getifaddrs(3) entry of the IPv6 address addr on IFNAME,
 * or NULL if there is none.
 */
static struct ifaddrs *
find_addr(struct ifaddrs *ifap, const struct in6_addr *addr)
{
	struct sockaddr_in6 sin6;
	struct ifaddrs *ifa;

	for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
		if (strcmp(ifa->ifa_name, IFNAME) != 0 ||
		    ifa->ifa_addr == NULL ||
		    ifa->ifa_addr->sa_family != AF_INET6 ||
		    ifa->ifa_addr->sa_len != sizeof(sin6))
			continue;
		memcpy(&sin6, ifa->ifa_addr, sizeof(sin6));
		if (IN6_ARE_ADDR_EQUAL(&sin6.sin6_addr, addr))
			return (ifa);
	}
	return (NULL);
}

ATF_TC(aifaddr_dstaddr);
ATF_TC_HEAD(aifaddr_dstaddr, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "SIOCAIFADDR_IN6 stores a valid destination address");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(aifaddr_dstaddr, tc)
{
	struct in6_aliasreq ifra;
	struct sockaddr_in6 dst;
	struct ifaddrs *ifa, *ifap;
	int s;

	require_vnet_jail();
	ATF_REQUIRE((s = socket(AF_INET6, SOCK_DGRAM, 0)) >= 0);

	aliasreq_init(&ifra, sizeof(struct sockaddr_in6));
	ATF_REQUIRE_MSG(ioctl(s, SIOCAIFADDR_IN6, &ifra) == 0,
	    "SIOCAIFADDR_IN6: %s", strerror(errno));

	ATF_REQUIRE_MSG(getifaddrs(&ifap) == 0, "getifaddrs: %s",
	    strerror(errno));
	ifa = find_addr(ifap, &ifra.ifra_addr.sin6_addr);
	ATF_REQUIRE_MSG(ifa != NULL, "%s not found on %s", ADDR, IFNAME);
	ATF_REQUIRE_MSG(ifa->ifa_dstaddr != NULL, "no destination address");
	ATF_REQUIRE_MSG(ifa->ifa_dstaddr->sa_family == AF_INET6,
	    "destination sa_family %u", ifa->ifa_dstaddr->sa_family);
	ATF_REQUIRE_MSG(ifa->ifa_dstaddr->sa_len == sizeof(dst),
	    "destination sa_len %u", ifa->ifa_dstaddr->sa_len);
	memcpy(&dst, ifa->ifa_dstaddr, sizeof(dst));
	ATF_REQUIRE_MSG(IN6_ARE_ADDR_EQUAL(&dst.sin6_addr,
	    &ifra.ifra_dstaddr.sin6_addr), "destination is not %s", DSTADDR);
	freeifaddrs(ifap);

	delete_addr(s);
	close(s);
}

ATF_TC(aifaddr_dstaddr_badlen);
ATF_TC_HEAD(aifaddr_dstaddr_badlen, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "SIOCAIFADDR_IN6 rejects a destination address with a bad length");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(aifaddr_dstaddr_badlen, tc)
{
	struct in6_aliasreq ifra;
	struct ifaddrs *ifap;
	int error, rv, s;

	require_vnet_jail();
	ATF_REQUIRE((s = socket(AF_INET6, SOCK_DGRAM, 0)) >= 0);

	aliasreq_init(&ifra, BADLEN);
	rv = ioctl(s, SIOCAIFADDR_IN6, &ifra);
	if (rv == 0) {
		/*
		 * The address was stored.  getifaddrs(3) then fails in
		 * this VNET until the address is deleted.
		 */
		error = 0;
		if (getifaddrs(&ifap) == 0)
			freeifaddrs(ifap);
		else
			error = errno;
		delete_addr(s);
		atf_tc_fail("SIOCAIFADDR_IN6 accepted sin6_len %d; "
		    "getifaddrs: %s", BADLEN,
		    error == 0 ? "ok" : strerror(error));
	}
	ATF_REQUIRE_ERRNO(EINVAL, rv == -1);

	ATF_REQUIRE_MSG(getifaddrs(&ifap) == 0, "getifaddrs: %s",
	    strerror(errno));
	freeifaddrs(ifap);
	close(s);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, aifaddr_dstaddr);
	ATF_TP_ADD_TC(tp, aifaddr_dstaddr_badlen);

	return (atf_no_error());
}
