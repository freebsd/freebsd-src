/*-
 * Copyright (c) 2020 Mariusz Zaborski <oshogbo@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <errno.h>
#include <netdb.h>

#include <atf-c.h>

#include <libcasper.h>
#include <casper/cap_net.h>

#define	TEST_DOMAIN_0	"example.com"
#define	TEST_DOMAIN_1	"freebsd.org"
#define	TEST_IPV4	"1.1.1.1"
#define	TEST_IPV6	"2001:4860:4860::8888"
#define	TEST_BIND_IPV4	"127.0.0.1"
#define	TEST_PORT	80
#define	TEST_PORT_STR	"80"

static cap_channel_t *
create_network_service(void)
{
	cap_channel_t *capcas, *capnet;

	capcas = cap_init();
	ATF_REQUIRE(capcas != NULL);

	capnet = cap_service_open(capcas, "system.net");
	ATF_REQUIRE(capnet != NULL);

	cap_close(capcas);
	return (capnet);
}

static int
test_getnameinfo_v4(cap_channel_t *chan, int family, const char *ip)
{
	struct sockaddr_in ipaddr;
	char capfn[MAXHOSTNAMELEN];
	char origfn[MAXHOSTNAMELEN];
	int capret, sysret;

	memset(&ipaddr, 0, sizeof(ipaddr));
	ipaddr.sin_family = family;
	inet_pton(family, ip, &ipaddr.sin_addr);

	capret = cap_getnameinfo(chan, (struct sockaddr *)&ipaddr, sizeof(ipaddr),
	    capfn, sizeof(capfn), NULL, 0, NI_NAMEREQD);
	if (capret != 0 && capret == ENOTCAPABLE)
		return (ENOTCAPABLE);

	sysret = getnameinfo((struct sockaddr *)&ipaddr, sizeof(ipaddr), origfn,
	    sizeof(origfn), NULL, 0, NI_NAMEREQD);
	if (sysret != 0) {
		atf_tc_skip("getnameinfo(%s) failed: %s",
		    ip, gai_strerror(sysret));
	}
	ATF_REQUIRE(capret == 0);
	ATF_REQUIRE(strcmp(origfn, capfn) == 0);

	return (0);
}

static int
test_getnameinfo_v6(cap_channel_t *chan, const char *ip)
{
	struct sockaddr_in6 ipaddr;
	char capfn[MAXHOSTNAMELEN];
	char origfn[MAXHOSTNAMELEN];
	int capret, sysret;

	memset(&ipaddr, 0, sizeof(ipaddr));
	ipaddr.sin6_family = AF_INET6;
	inet_pton(AF_INET6, ip, &ipaddr.sin6_addr);

	capret = cap_getnameinfo(chan, (struct sockaddr *)&ipaddr, sizeof(ipaddr),
	    capfn, sizeof(capfn), NULL, 0, NI_NAMEREQD);
	if (capret != 0 && capret == ENOTCAPABLE)
		return (ENOTCAPABLE);

	sysret = getnameinfo((struct sockaddr *)&ipaddr, sizeof(ipaddr), origfn,
	    sizeof(origfn), NULL, 0, NI_NAMEREQD);
	if (sysret != 0) {
		atf_tc_skip("getnameinfo(%s) failed: %s",
		    ip, gai_strerror(sysret));
	}
	ATF_REQUIRE(capret == 0);
	ATF_REQUIRE(strcmp(origfn, capfn) == 0);

	return (0);
}

static int
test_getnameinfo(cap_channel_t *chan, int family, const char *ip)
{

	if (family == AF_INET6) {
		return (test_getnameinfo_v6(chan, ip));
	}

	return (test_getnameinfo_v4(chan, family, ip));
}

static int
test_gethostbyaddr_v4(cap_channel_t *chan, int family, const char *ip)
{
	struct in_addr ipaddr;
	struct hostent *caphp, *orighp;

	memset(&ipaddr, 0, sizeof(ipaddr));
	inet_pton(AF_INET, ip, &ipaddr);

	caphp = cap_gethostbyaddr(chan, &ipaddr, sizeof(ipaddr), family);
	if (caphp == NULL && h_errno == ENOTCAPABLE)
		return (ENOTCAPABLE);

	orighp = gethostbyaddr(&ipaddr, sizeof(ipaddr), family);
	if (orighp == NULL)
		atf_tc_skip("gethostbyaddr(%s) failed", ip);
	ATF_REQUIRE(caphp != NULL);
	ATF_REQUIRE(strcmp(orighp->h_name, caphp->h_name) == 0);

	return (0);
}

static int
test_gethostbyaddr_v6(cap_channel_t *chan, const char *ip)
{
	struct in6_addr ipaddr;
	struct hostent *caphp, *orighp;

	memset(&ipaddr, 0, sizeof(ipaddr));
	inet_pton(AF_INET6, ip, &ipaddr);

	caphp = cap_gethostbyaddr(chan, &ipaddr, sizeof(ipaddr), AF_INET6);
	if (caphp == NULL && h_errno == ENOTCAPABLE)
		return (ENOTCAPABLE);

	orighp = gethostbyaddr(&ipaddr, sizeof(ipaddr), AF_INET6);
	if (orighp == NULL)
		atf_tc_skip("gethostbyaddr(%s) failed", ip);
	ATF_REQUIRE(caphp != NULL);
	ATF_REQUIRE(strcmp(orighp->h_name, caphp->h_name) == 0);

	return (0);
}

static int
test_gethostbyaddr(cap_channel_t *chan, int family, const char *ip)
{

	if (family == AF_INET6) {
		return (test_gethostbyaddr_v6(chan, ip));
	} else {
		return (test_gethostbyaddr_v4(chan, family, ip));
	}
}

static int
test_getaddrinfo(cap_channel_t *chan, int family, const char *domain,
    const char *servname)
{
	struct addrinfo hints, *capres, *origres, *res0, *res1;
	bool found;
	int capret, sysret;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = family;
	hints.ai_socktype = SOCK_STREAM;

	capret = cap_getaddrinfo(chan, domain, servname, &hints, &capres);
	if (capret != 0 && capret == ENOTCAPABLE)
		return (capret);

	sysret = getaddrinfo(domain, servname, &hints, &origres);
	if (sysret != 0)
		atf_tc_skip("getaddrinfo(%s) failed: %s",
		    domain, gai_strerror(sysret));
	ATF_REQUIRE(capret == 0);

	for (res0 = capres; res0 != NULL; res0 = res0->ai_next) {
		found = false;
		for (res1 = origres; res1 != NULL; res1 = res1->ai_next) {
			if (res1->ai_addrlen == res0->ai_addrlen &&
			    memcmp(res1->ai_addr, res0->ai_addr,
			    res0->ai_addrlen) == 0) {
				found = true;
				break;
			}
		}
		ATF_REQUIRE(found);
	}

	freeaddrinfo(capres);
	freeaddrinfo(origres);
	return (0);
}

static int
test_gethostbyname(cap_channel_t *chan, int family, const char *domain)
{
	struct hostent *caphp, *orighp;

	caphp = cap_gethostbyname2(chan, domain, family);
	if (caphp == NULL && h_errno == ENOTCAPABLE)
		return (h_errno);

	orighp = gethostbyname2(domain, family);
	if (orighp == NULL)
		atf_tc_skip("gethostbyname2(%s) failed", domain);

	ATF_REQUIRE(caphp != NULL);
	ATF_REQUIRE(strcmp(caphp->h_name, orighp->h_name) == 0);
	return (0);
}

static int
test_bind(cap_channel_t *chan, const char *ip)
{
	struct sockaddr_in ipv4;
	int capfd, ret, serrno;

	capfd = socket(AF_INET, SOCK_STREAM, 0);
	ATF_REQUIRE(capfd > 0);

	memset(&ipv4, 0, sizeof(ipv4));
	ipv4.sin_family = AF_INET;
	inet_pton(AF_INET, ip, &ipv4.sin_addr);

	ret = cap_bind(chan, capfd, (struct sockaddr *)&ipv4, sizeof(ipv4));
	serrno = errno;
	close(capfd);

	return (ret < 0 ? serrno : 0);
}

static int
test_connect(cap_channel_t *chan, const char *ip, unsigned short port)
{
	struct sockaddr_in ipv4;
	int capfd, ret, serrno;

	capfd = socket(AF_INET, SOCK_STREAM, 0);
	ATF_REQUIRE(capfd >= 0);

	memset(&ipv4, 0, sizeof(ipv4));
	ipv4.sin_family = AF_INET;
	ipv4.sin_port = htons(port);
	inet_pton(AF_INET, ip, &ipv4.sin_addr);

	ret = cap_connect(chan, capfd, (struct sockaddr *)&ipv4, sizeof(ipv4));
	serrno = errno;
	ATF_REQUIRE(close(capfd) == 0);

	if (ret < 0 && serrno != ENOTCAPABLE) {
		int sd;

		/*
		 * If the connection failed, it might be because we can't reach
		 * the destination host.  To check, try a plain connect() and
		 * see if it fails with the same error.
		 */
		sd = socket(AF_INET, SOCK_STREAM, 0);
		ATF_REQUIRE(sd >= 0);

		memset(&ipv4, 0, sizeof(ipv4));
		ipv4.sin_family = AF_INET;
		ipv4.sin_port = htons(port);
		inet_pton(AF_INET, ip, &ipv4.sin_addr);
		ret = connect(sd, (struct sockaddr *)&ipv4, sizeof(ipv4));
		ATF_REQUIRE(ret < 0);
		ATF_REQUIRE_MSG(errno == serrno, "errno %d != serrno %d",
		    errno, serrno);
		ATF_REQUIRE(close(sd) == 0);
		atf_tc_skip("connect(%s:%d) failed: %s",
		    ip, port, strerror(serrno));
	}

	return (ret < 0 ? serrno : 0);
}

static void
test_extend_mode(cap_channel_t *capnet, int current)
{
	cap_net_limit_t *limit;
	const int rights[] = {
		CAPNET_ADDR2NAME,
		CAPNET_NAME2ADDR,
		CAPNET_DEPRECATED_ADDR2NAME,
		CAPNET_DEPRECATED_NAME2ADDR,
		CAPNET_CONNECT,
		CAPNET_BIND,
		CAPNET_CONNECTDNS
	};
	size_t i;

	for (i = 0; i < nitems(rights); i++) {
		if (current == rights[i])
			continue;

		limit = cap_net_limit_init(capnet, current | rights[i]);
		ATF_REQUIRE(limit != NULL);
		ATF_REQUIRE(cap_net_limit(limit) != 0);
	}
}

ATF_TC_WITHOUT_HEAD(capnet__getnameinfo);
ATF_TC_BODY(capnet__getnameinfo, tc)
{
	cap_channel_t *capnet;

	capnet = create_network_service();

	ATF_REQUIRE(test_getnameinfo(capnet, AF_INET, TEST_IPV4) == 0);
	ATF_REQUIRE(test_getnameinfo(capnet, AF_INET6, TEST_IPV6) == 0);

	cap_close(capnet);
}

ATF_TC_WITHOUT_HEAD(capnet__connect);
ATF_TC_BODY(capnet__connect, tc)
{
	cap_channel_t *capnet;

	capnet = create_network_service();

	ATF_REQUIRE(test_connect(capnet, TEST_IPV4, 80) == 0);

	cap_close(capnet);
}

ATF_TC_WITHOUT_HEAD(capnet__bind);
ATF_TC_BODY(capnet__bind, tc)
{
	cap_channel_t *capnet;

	capnet = create_network_service();

	ATF_REQUIRE(test_bind(capnet, TEST_BIND_IPV4) == 0);

	cap_close(capnet);
}

ATF_TC_WITHOUT_HEAD(capnet__getaddrinfo);
ATF_TC_BODY(capnet__getaddrinfo, tc)
{
	cap_channel_t *capnet;
	struct addrinfo hints, *capres;

	capnet = create_network_service();

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	ATF_REQUIRE(cap_getaddrinfo(capnet, TEST_IPV4, "80", &hints, &capres) ==
	    0);

	cap_close(capnet);
}

ATF_TC_WITHOUT_HEAD(capnet__gethostbyname);
ATF_TC_BODY(capnet__gethostbyname, tc)
{
	cap_channel_t *capnet;

	capnet = create_network_service();

	ATF_REQUIRE(test_gethostbyname(capnet, AF_INET, TEST_DOMAIN_0) == 0);

	cap_close(capnet);
}

ATF_TC_WITHOUT_HEAD(capnet__gethostbyaddr);
ATF_TC_BODY(capnet__gethostbyaddr, tc)
{
	cap_channel_t *capnet;

	capnet = create_network_service();

	ATF_REQUIRE(test_gethostbyaddr(capnet, AF_INET, TEST_IPV4) == 0);
	ATF_REQUIRE(test_gethostbyaddr(capnet, AF_INET6, TEST_IPV6) == 0);

	cap_close(capnet);
}

ATF_TC_WITHOUT_HEAD(capnet__getnameinfo_buffer);
ATF_TC_BODY(capnet__getnameinfo_buffer, tc)
{
	cap_channel_t *chan;
	struct sockaddr_in sin;
	int ret;
	struct {
		char host[sizeof(TEST_IPV4)];
		char host_canary;
		char serv[sizeof(TEST_PORT_STR)];
		char serv_canary;
	} buffers;

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(TEST_PORT);
	ret = inet_pton(AF_INET, TEST_IPV4, &sin.sin_addr);
	ATF_REQUIRE_EQ(1, ret);

	memset(&buffers, '!', sizeof(buffers));

	chan = create_network_service();
	ret = cap_getnameinfo(chan, (struct sockaddr *)&sin, sizeof(sin),
	    buffers.host, sizeof(buffers.host),
	    buffers.serv, sizeof(buffers.serv),
	    NI_NUMERICHOST | NI_NUMERICSERV);
	ATF_REQUIRE_EQ_MSG(0, ret, "%d", ret);

	// Verify that cap_getnameinfo worked with minimally sized buffers.
	ATF_CHECK_EQ(0, strcmp(TEST_IPV4, buffers.host));
	ATF_CHECK_EQ(0, strcmp(TEST_PORT_STR, buffers.serv));

	// Verify that cap_getnameinfo did not overflow the buffers.
	ATF_CHECK_EQ('!', buffers.host_canary);
	ATF_CHECK_EQ('!', buffers.serv_canary);

	cap_close(chan);
}

ATF_TC_WITHOUT_HEAD(capnet__limits_addr2name_mode);
ATF_TC_BODY(capnet__limits_addr2name_mode, tc)
{
	cap_channel_t *capnet;
	cap_net_limit_t *limit;

	capnet = create_network_service();

	/* LIMIT */
	limit = cap_net_limit_init(capnet, CAPNET_ADDR2NAME);
	ATF_REQUIRE(limit != NULL);
	ATF_REQUIRE(cap_net_limit(limit) == 0);

	/* ALLOWED */
	ATF_REQUIRE(test_getnameinfo(capnet, AF_INET, TEST_IPV4) == 0);

	/* DISALLOWED */
	ATF_REQUIRE(test_gethostbyname(capnet, AF_INET, TEST_DOMAIN_0) ==
	    ENOTCAPABLE);
	ATF_REQUIRE(test_gethostbyaddr(capnet, AF_INET, TEST_IPV4) ==
	    ENOTCAPABLE);
	ATF_REQUIRE(test_getaddrinfo(capnet, AF_INET, TEST_DOMAIN_0, NULL) ==
	    ENOTCAPABLE);
	ATF_REQUIRE(test_bind(capnet, TEST_BIND_IPV4) == ENOTCAPABLE);
	ATF_REQUIRE(test_connect(capnet, TEST_IPV4, 80) == ENOTCAPABLE);

	test_extend_mode(capnet, CAPNET_ADDR2NAME);

	cap_close(capnet);
}

ATF_TC_WITHOUT_HEAD(capnet__limits_addr2name_family);
ATF_TC_BODY(capnet__limits_addr2name_family, tc)
{
	cap_channel_t *capnet;
	cap_net_limit_t *limit;
	int family[] = { AF_INET6, AF_INET };

	capnet = create_network_service();

	/* Limit to AF_INET6 and AF_INET. */
	limit = cap_net_limit_init(capnet, CAPNET_ADDR2NAME);
	ATF_REQUIRE(limit != NULL);
	cap_net_limit_addr2name_family(limit, family, nitems(family));
	ATF_REQUIRE(cap_net_limit(limit) == 0);

	ATF_REQUIRE(test_getnameinfo(capnet, AF_INET, TEST_IPV4) == 0);
	ATF_REQUIRE(test_getnameinfo(capnet, AF_INET6, TEST_IPV6) == 0);

	/* Limit to AF_INET6 and AF_INET. */
	limit = cap_net_limit_init(capnet, CAPNET_ADDR2NAME);
	ATF_REQUIRE(limit != NULL);
	cap_net_limit_addr2name_family(limit, &family[0], 1);
	cap_net_limit_addr2name_family(limit, &family[1], 1);
	ATF_REQUIRE(cap_net_limit(limit) == 0);

	ATF_REQUIRE(test_getnameinfo(capnet, AF_INET, TEST_IPV4) == 0);
	ATF_REQUIRE(test_getnameinfo(capnet, AF_INET6, TEST_IPV6) == 0);

	/* Limit to AF_INET6. */
	limit = cap_net_limit_init(capnet, CAPNET_ADDR2NAME);
	ATF_REQUIRE(limit != NULL);
	cap_net_limit_addr2name_family(limit, family, 1);
	ATF_REQUIRE(cap_net_limit(limit) == 0);

	ATF_REQUIRE(test_getnameinfo(capnet, AF_INET, TEST_IPV4) ==
	    ENOTCAPABLE);
	ATF_REQUIRE(test_getnameinfo(capnet, AF_INET6, TEST_IPV6) == 0);

	/* Unable to set empty limits. Empty limits means full access. */
	limit = cap_net_limit_init(capnet, CAPNET_ADDR2NAME);
	ATF_REQUIRE(cap_net_limit(limit) != 0);

	cap_close(capnet);
}

ATF_TC_WITHOUT_HEAD(capnet__limits_addr2name);
ATF_TC_BODY(capnet__limits_addr2name, tc)
{
	cap_channel_t *capnet;
	cap_net_limit_t *limit;
	struct sockaddr_in ipaddrv4;
	struct sockaddr_in6 ipaddrv6;

	capnet = create_network_service();

	/* Limit to TEST_IPV4 and TEST_IPV6. */
	memset(&ipaddrv4, 0, sizeof(ipaddrv4));
	memset(&ipaddrv6, 0, sizeof(ipaddrv6));

	ipaddrv4.sin_family = AF_INET;
	inet_pton(AF_INET, TEST_IPV4, &ipaddrv4.sin_addr);

	ipaddrv6.sin6_family = AF_INET6;
	inet_pton(AF_INET6, TEST_IPV6, &ipaddrv6.sin6_addr);

	limit = cap_net_limit_init(capnet, CAPNET_ADDR2NAME);
	ATF_REQUIRE(limit != NULL);

	cap_net_limit_addr2name(limit, (struct sockaddr *)&ipaddrv4,
	    sizeof(ipaddrv4));
	cap_net_limit_addr2name(limit, (struct sockaddr *)&ipaddrv6,
	    sizeof(ipaddrv6));
	ATF_REQUIRE(cap_net_limit(limit) == 0);

	ATF_REQUIRE(test_getnameinfo(capnet, AF_INET, TEST_IPV4) == 0);
	ATF_REQUIRE(test_getnameinfo(capnet, AF_INET6, TEST_IPV6) == 0);
	ATF_REQUIRE(test_getnameinfo(capnet, AF_INET, "127.0.0.1") ==
	    ENOTCAPABLE);

	/* Limit to AF_INET. */
	limit = cap_net_limit_init(capnet, CAPNET_ADDR2NAME);
	ATF_REQUIRE(limit != NULL);
	cap_net_limit_addr2name(limit, (struct sockaddr *)&ipaddrv4,
	    sizeof(ipaddrv4));
	ATF_REQUIRE(cap_net_limit(limit) == 0);

	ATF_REQUIRE(test_getnameinfo(capnet, AF_INET, TEST_IPV4) == 0);
	ATF_REQUIRE(test_getnameinfo(capnet, AF_INET6, TEST_IPV6) ==
	    ENOTCAPABLE);
	ATF_REQUIRE(test_getnameinfo(capnet, AF_INET, "127.0.0.1") ==
	    ENOTCAPABLE);

	/* Unable to set empty limits. Empty limits means full access. */
	limit = cap_net_limit_init(capnet, CAPNET_ADDR2NAME);
	ATF_REQUIRE(cap_net_limit(limit) != 0);

	cap_close(capnet);
}

ATF_TC_WITHOUT_HEAD(capnet__limits_deprecated_addr2name_mode);
ATF_TC_BODY(capnet__limits_deprecated_addr2name_mode, tc)
{
	cap_channel_t *capnet;
	cap_net_limit_t *limit;

	capnet = create_network_service();

	/* LIMIT */
	limit = cap_net_limit_init(capnet, CAPNET_DEPRECATED_ADDR2NAME);
	ATF_REQUIRE(limit != NULL);
	ATF_REQUIRE(cap_net_limit(limit) == 0);

	/* ALLOWED */
	ATF_REQUIRE(test_gethostbyaddr(capnet, AF_INET, TEST_IPV4) == 0);

	/* DISALLOWED */
	ATF_REQUIRE(test_gethostbyname(capnet, AF_INET, TEST_DOMAIN_0) ==
	    ENOTCAPABLE);
	ATF_REQUIRE(test_getnameinfo(capnet, AF_INET, TEST_IPV4) ==
	    ENOTCAPABLE);
	ATF_REQUIRE(test_getaddrinfo(capnet, AF_INET, TEST_DOMAIN_0, NULL) ==
	    ENOTCAPABLE);
	ATF_REQUIRE(test_bind(capnet, TEST_BIND_IPV4) == ENOTCAPABLE);
	ATF_REQUIRE(test_connect(capnet, TEST_IPV4, 80) == ENOTCAPABLE);

	cap_close(capnet);
}

ATF_TC_WITHOUT_HEAD(capnet__limits_deprecated_addr2name_family);
ATF_TC_BODY(capnet__limits_deprecated_addr2name_family, tc)
{
	cap_channel_t *capnet;
	cap_net_limit_t *limit;
	int family[] = { AF_INET6, AF_INET };

	capnet = create_network_service();

	/* Limit to AF_INET6 and AF_INET. */
	limit = cap_net_limit_init(capnet, CAPNET_DEPRECATED_ADDR2NAME);
	ATF_REQUIRE(limit != NULL);
	cap_net_limit_addr2name_family(limit, family, nitems(family));
	ATF_REQUIRE(cap_net_limit(limit) == 0);

	ATF_REQUIRE(test_gethostbyaddr(capnet, AF_INET, TEST_IPV4) == 0);
	ATF_REQUIRE(test_gethostbyaddr(capnet, AF_INET6, TEST_IPV6) == 0);
	ATF_REQUIRE(test_gethostbyaddr(capnet, PF_LINK, TEST_IPV4) ==
	    ENOTCAPABLE);

	/* Limit to AF_INET6 and AF_INET. */
	limit = cap_net_limit_init(capnet, CAPNET_DEPRECATED_ADDR2NAME);
	ATF_REQUIRE(limit != NULL);
	cap_net_limit_addr2name_family(limit, &family[0], 1);
	cap_net_limit_addr2name_family(limit, &family[1], 1);
	ATF_REQUIRE(cap_net_limit(limit) == 0);

	ATF_REQUIRE(test_gethostbyaddr(capnet, AF_INET, TEST_IPV4) == 0);
	ATF_REQUIRE(test_gethostbyaddr(capnet, AF_INET6, TEST_IPV6) == 0);
	ATF_REQUIRE(test_gethostbyaddr(capnet, PF_LINK, TEST_IPV4) ==
	    ENOTCAPABLE);

	/* Limit to AF_INET6. */
	limit = cap_net_limit_init(capnet, CAPNET_DEPRECATED_ADDR2NAME);
	ATF_REQUIRE(limit != NULL);
	cap_net_limit_addr2name_family(limit, family, 1);
	ATF_REQUIRE(cap_net_limit(limit) == 0);

	ATF_REQUIRE(test_gethostbyaddr(capnet, AF_INET, TEST_IPV4) ==
	    ENOTCAPABLE);
	ATF_REQUIRE(test_gethostbyaddr(capnet, AF_INET6, TEST_IPV6) == 0);
	ATF_REQUIRE(test_gethostbyaddr(capnet, PF_LINK, TEST_IPV4) ==
	    ENOTCAPABLE);

	/* Unable to set empty limits. Empty limits means full access. */
	limit = cap_net_limit_init(capnet, CAPNET_DEPRECATED_ADDR2NAME);
	ATF_REQUIRE(cap_net_limit(limit) != 0);

	cap_close(capnet);
}

ATF_TC_WITHOUT_HEAD(capnet__limits_deprecated_addr2name);
ATF_TC_BODY(capnet__limits_deprecated_addr2name, tc)
{
	cap_channel_t *capnet;
	cap_net_limit_t *limit;
	struct in_addr ipaddrv4;
	struct in6_addr ipaddrv6;

	capnet = create_network_service();

	/* Limit to TEST_IPV4 and TEST_IPV6. */
	memset(&ipaddrv4, 0, sizeof(ipaddrv4));
	memset(&ipaddrv6, 0, sizeof(ipaddrv6));

	inet_pton(AF_INET, TEST_IPV4, &ipaddrv4);
	inet_pton(AF_INET6, TEST_IPV6, &ipaddrv6);

	limit = cap_net_limit_init(capnet, CAPNET_DEPRECATED_ADDR2NAME);
	ATF_REQUIRE(limit != NULL);

	cap_net_limit_addr2name(limit, (struct sockaddr *)&ipaddrv4,
	    sizeof(ipaddrv4));
	cap_net_limit_addr2name(limit, (struct sockaddr *)&ipaddrv6,
	    sizeof(ipaddrv6));
	ATF_REQUIRE(cap_net_limit(limit) == 0);

	ATF_REQUIRE(test_gethostbyaddr(capnet, AF_INET, TEST_IPV4) == 0);
	ATF_REQUIRE(test_gethostbyaddr(capnet, AF_INET6, TEST_IPV6) == 0);
	ATF_REQUIRE(test_gethostbyaddr(capnet, AF_INET, "127.0.0.1") ==
	    ENOTCAPABLE);

	/* Limit to AF_INET. */
	limit = cap_net_limit_init(capnet, CAPNET_DEPRECATED_ADDR2NAME);
	ATF_REQUIRE(limit != NULL);
	cap_net_limit_addr2name(limit, (struct sockaddr *)&ipaddrv4,
	    sizeof(ipaddrv4));
	ATF_REQUIRE(cap_net_limit(limit) == 0);

	ATF_REQUIRE(test_gethostbyaddr(capnet, AF_INET, TEST_IPV4) == 0);
	ATF_REQUIRE(test_gethostbyaddr(capnet, AF_INET6, TEST_IPV6) ==
	    ENOTCAPABLE);
	ATF_REQUIRE(test_gethostbyaddr(capnet, AF_INET, "127.0.0.1") ==
	    ENOTCAPABLE);

	/* Unable to set empty limits. Empty limits means full access. */
	limit = cap_net_limit_init(capnet, CAPNET_DEPRECATED_ADDR2NAME);
	ATF_REQUIRE(cap_net_limit(limit) != 0);

	cap_close(capnet);
}


ATF_TC_WITHOUT_HEAD(capnet__limits_name2addr_mode);
ATF_TC_BODY(capnet__limits_name2addr_mode, tc)
{
	cap_channel_t *capnet;
	cap_net_limit_t *limit;

	capnet = create_network_service();

	/* LIMIT */
	limit = cap_net_limit_init(capnet, CAPNET_NAME2ADDR);
	ATF_REQUIRE(limit != NULL);
	ATF_REQUIRE(cap_net_limit(limit) == 0);

	/* ALLOWED */
	ATF_REQUIRE(test_getaddrinfo(capnet, AF_INET, TEST_DOMAIN_0, NULL) ==
	    0);

	/* DISALLOWED */
	ATF_REQUIRE(
	    test_gethostbyname(capnet, AF_INET, TEST_DOMAIN_0) == ENOTCAPABLE);
	ATF_REQUIRE(test_getnameinfo(capnet, AF_INET, TEST_IPV4) ==
	    ENOTCAPABLE);
	ATF_REQUIRE(test_gethostbyaddr(capnet, AF_INET, TEST_IPV4) ==
	    ENOTCAPABLE);
	ATF_REQUIRE(test_bind(capnet, TEST_BIND_IPV4) == ENOTCAPABLE);
	ATF_REQUIRE(test_connect(capnet, TEST_IPV4, 80) == ENOTCAPABLE);

	test_extend_mode(capnet, CAPNET_ADDR2NAME);

	cap_close(capnet);
}

ATF_TC_WITHOUT_HEAD(capnet__limits_name2addr_hosts);
ATF_TC_BODY(capnet__limits_name2addr_hosts, tc)
{
	cap_channel_t *capnet;
	cap_net_limit_t *limit;

	capnet = create_network_service();

	/* Limit to TEST_DOMAIN_0 and localhost only. */
	limit = cap_net_limit_init(capnet, CAPNET_NAME2ADDR);
	ATF_REQUIRE(limit != NULL);
	cap_net_limit_name2addr(limit, TEST_DOMAIN_0, NULL);
	cap_net_limit_name2addr(limit, "localhost", NULL);
	ATF_REQUIRE(cap_net_limit(limit) == 0);

	ATF_REQUIRE(test_getaddrinfo(capnet, AF_INET, TEST_DOMAIN_0, NULL) ==
	    0);
	ATF_REQUIRE(test_getaddrinfo(capnet, AF_INET, "localhost", NULL) == 0);
	ATF_REQUIRE(test_getaddrinfo(capnet, AF_INET, TEST_DOMAIN_1, NULL) ==
	    ENOTCAPABLE);

	/* Limit to TEST_DOMAIN_0 only. */
	limit = cap_net_limit_init(capnet, CAPNET_NAME2ADDR);
	ATF_REQUIRE(limit != NULL);
	cap_net_limit_name2addr(limit, TEST_DOMAIN_0, NULL);
	ATF_REQUIRE(cap_net_limit(limit) == 0);

	ATF_REQUIRE(test_getaddrinfo(capnet, AF_INET, "localhost", NULL) ==
	    ENOTCAPABLE);
	ATF_REQUIRE(test_getaddrinfo(capnet, AF_INET, TEST_DOMAIN_1, NULL) ==
	    ENOTCAPABLE);
	ATF_REQUIRE(test_getaddrinfo(capnet, AF_INET, TEST_DOMAIN_0, NULL) ==
	    0);

	/* Unable to set empty limits. Empty limits means full access. */
	limit = cap_net_limit_init(capnet, CAPNET_NAME2ADDR);
	ATF_REQUIRE(cap_net_limit(limit) != 0);

	/* Try to extend the limit. */
	limit = cap_net_limit_init(capnet, CAPNET_NAME2ADDR);
	ATF_REQUIRE(limit != NULL);
	cap_net_limit_name2addr(limit, TEST_DOMAIN_1, NULL);
	ATF_REQUIRE(cap_net_limit(limit) != 0);

	limit = cap_net_limit_init(capnet, CAPNET_NAME2ADDR);
	ATF_REQUIRE(limit != NULL);
	cap_net_limit_name2addr(limit, TEST_DOMAIN_0, NULL);
	cap_net_limit_name2addr(limit, TEST_DOMAIN_1, NULL);
	ATF_REQUIRE(cap_net_limit(limit) != 0);

	cap_close(capnet);
}

ATF_TC_WITHOUT_HEAD(capnet__limits_name2addr_hosts_servnames_strict);
ATF_TC_BODY(capnet__limits_name2addr_hosts_servnames_strict, tc)
{
	cap_channel_t *capnet;
	cap_net_limit_t *limit;

	capnet = create_network_service();

	/*
	 * Limit to TEST_DOMAIN_0 and HTTP service.
	 */
	limit = cap_net_limit_init(capnet, CAPNET_NAME2ADDR);
	ATF_REQUIRE(limit != NULL);
	cap_net_limit_name2addr(limit, TEST_DOMAIN_0, "http");
	ATF_REQUIRE(cap_net_limit(limit) == 0);

	ATF_REQUIRE(test_getaddrinfo(capnet, AF_INET, TEST_DOMAIN_0, "http") ==
	    0);
	ATF_REQUIRE(test_getaddrinfo(capnet, AF_INET, TEST_DOMAIN_0, NULL) ==
	    ENOTCAPABLE);
	ATF_REQUIRE(test_getaddrinfo(capnet, AF_INET, TEST_DOMAIN_0, "snmp") ==
	    ENOTCAPABLE);
	ATF_REQUIRE(test_getaddrinfo(capnet, AF_INET, TEST_DOMAIN_1, "http") ==
	    ENOTCAPABLE);

	/* Unable to set empty limits. Empty limits means full access. */
	limit = cap_net_limit_init(capnet, CAPNET_NAME2ADDR);
	ATF_REQUIRE(cap_net_limit(limit) != 0);

	cap_close(capnet);
}

ATF_TC_WITHOUT_HEAD(capnet__limits_name2addr_hosts_servnames_mix);
ATF_TC_BODY(capnet__limits_name2addr_hosts_servnames_mix, tc)
{
	cap_channel_t *capnet;
	cap_net_limit_t *limit;

	capnet = create_network_service();

	/*
	 * Limit to TEST_DOMAIN_0 and any servnamex, and any domain with
	 * servname HTTP.
	 */
	limit = cap_net_limit_init(capnet, CAPNET_NAME2ADDR);
	ATF_REQUIRE(limit != NULL);
	cap_net_limit_name2addr(limit, TEST_DOMAIN_0, NULL);
	cap_net_limit_name2addr(limit, NULL, "http");
	ATF_REQUIRE(cap_net_limit(limit) == 0);

	ATF_REQUIRE(test_getaddrinfo(capnet, AF_INET, TEST_DOMAIN_0, "http") ==
	    0);
	ATF_REQUIRE(test_getaddrinfo(capnet, AF_INET, TEST_DOMAIN_0, NULL) ==
	    0);
	ATF_REQUIRE(test_getaddrinfo(capnet, AF_INET, TEST_DOMAIN_1, "http") ==
	    0);
	ATF_REQUIRE(test_getaddrinfo(capnet, AF_INET, TEST_DOMAIN_0, NULL) ==
	    0);
	ATF_REQUIRE(test_getaddrinfo(capnet, AF_INET, TEST_DOMAIN_1, "snmp") ==
	    ENOTCAPABLE);

	/* Limit to HTTP servname only. */
	limit = cap_net_limit_init(capnet, CAPNET_NAME2ADDR);
	ATF_REQUIRE(limit != NULL);
	cap_net_limit_name2addr(limit, NULL, "http");
	ATF_REQUIRE(cap_net_limit(limit) == 0);

	ATF_REQUIRE(test_getaddrinfo(capnet, AF_INET, TEST_DOMAIN_0, "http") ==
	    0);
	ATF_REQUIRE(test_getaddrinfo(capnet, AF_INET, TEST_DOMAIN_0, NULL) ==
	    ENOTCAPABLE);
	ATF_REQUIRE(test_getaddrinfo(capnet, AF_INET, TEST_DOMAIN_1, "http") ==
	    0);
	ATF_REQUIRE(test_getaddrinfo(capnet, AF_INET, TEST_DOMAIN_0, NULL) ==
	    ENOTCAPABLE);
	ATF_REQUIRE(test_getaddrinfo(capnet, AF_INET, TEST_DOMAIN_1, "snmp") ==
	    ENOTCAPABLE);

	/* Unable to set empty limits. Empty limits means full access. */
	limit = cap_net_limit_init(capnet, CAPNET_NAME2ADDR);
	ATF_REQUIRE(cap_net_limit(limit) != 0);

	cap_close(capnet);
}

ATF_TC_WITHOUT_HEAD(capnet__limits_name2addr_family);
ATF_TC_BODY(capnet__limits_name2addr_family, tc)
{
	cap_channel_t *capnet;
	cap_net_limit_t *limit;
	int family[] = { AF_INET6, AF_INET };

	capnet = create_network_service();

	/* Limit to AF_INET and AF_INET6. */
	limit = cap_net_limit_init(capnet, CAPNET_NAME2ADDR);
	ATF_REQUIRE(limit != NULL);
	cap_net_limit_name2addr(limit, TEST_DOMAIN_0, NULL);
	cap_net_limit_name2addr_family(limit, family, nitems(family));
	ATF_REQUIRE(cap_net_limit(limit) == 0);

	ATF_REQUIRE(test_getaddrinfo(capnet, AF_INET, TEST_DOMAIN_0, NULL) ==
	    0);
	ATF_REQUIRE(test_getaddrinfo(capnet, AF_INET6, TEST_DOMAIN_0, NULL) ==
	    0);
	ATF_REQUIRE(test_getaddrinfo(capnet, PF_LINK, TEST_DOMAIN_0, NULL) ==
	    ENOTCAPABLE);

	/* Limit to AF_INET and AF_INET6. */
	limit = cap_net_limit_init(capnet, CAPNET_NAME2ADDR);
	ATF_REQUIRE(limit != NULL);
	cap_net_limit_name2addr(limit, TEST_DOMAIN_0, NULL);
	cap_net_limit_name2addr_family(limit, &family[0], 1);
	cap_net_limit_name2addr_family(limit, &family[1], 1);
	ATF_REQUIRE(cap_net_limit(limit) == 0);

	ATF_REQUIRE(test_getaddrinfo(capnet, AF_INET, TEST_DOMAIN_0, NULL) ==
	    0);
	ATF_REQUIRE(test_getaddrinfo(capnet, AF_INET6, TEST_DOMAIN_0, NULL) ==
	    0);
	ATF_REQUIRE(test_getaddrinfo(capnet, PF_LINK, TEST_DOMAIN_0, NULL) ==
	    ENOTCAPABLE);

	/* Limit to AF_INET6 only. */
	limit = cap_net_limit_init(capnet, CAPNET_NAME2ADDR);
	ATF_REQUIRE(limit != NULL);
	cap_net_limit_name2addr(limit, TEST_DOMAIN_0, NULL);
	cap_net_limit_name2addr_family(limit, family, 1);
	ATF_REQUIRE(cap_net_limit(limit) == 0);

	ATF_REQUIRE(test_getaddrinfo(capnet, AF_INET, TEST_DOMAIN_0, NULL) ==
	    ENOTCAPABLE);
	ATF_REQUIRE(test_getaddrinfo(capnet, AF_INET6, TEST_DOMAIN_0, NULL) ==
	    0);
	ATF_REQUIRE(test_getaddrinfo(capnet, PF_LINK, TEST_DOMAIN_0, NULL) ==
	    ENOTCAPABLE);

	/* Unable to set empty limits. Empty limits means full access. */
	limit = cap_net_limit_init(capnet, CAPNET_NAME2ADDR);
	ATF_REQUIRE(cap_net_limit(limit) != 0);

	cap_close(capnet);
}

ATF_TC_WITHOUT_HEAD(capnet__limits_deprecated_name2addr_mode);
ATF_TC_BODY(capnet__limits_deprecated_name2addr_mode, tc)
{
	cap_channel_t *capnet;
	cap_net_limit_t *limit;

	capnet = create_network_service();

	/* LIMIT */
	limit = cap_net_limit_init(capnet, CAPNET_DEPRECATED_NAME2ADDR);
	ATF_REQUIRE(limit != NULL);
	ATF_REQUIRE(cap_net_limit(limit) == 0);

	/* ALLOWED */
	ATF_REQUIRE(test_gethostbyname(capnet, AF_INET, TEST_DOMAIN_0) == 0);

	/* DISALLOWED */
	ATF_REQUIRE(test_getnameinfo(capnet, AF_INET, TEST_IPV4) ==
	    ENOTCAPABLE);
	ATF_REQUIRE(test_gethostbyaddr(capnet, AF_INET, TEST_IPV4) ==
	    ENOTCAPABLE);
	ATF_REQUIRE(test_getaddrinfo(capnet, AF_INET, TEST_DOMAIN_0, NULL) ==
	    ENOTCAPABLE);
	ATF_REQUIRE(test_bind(capnet, TEST_BIND_IPV4) == ENOTCAPABLE);
	ATF_REQUIRE(test_connect(capnet, TEST_IPV4, 80) == ENOTCAPABLE);

	test_extend_mode(capnet, CAPNET_ADDR2NAME);

	cap_close(capnet);
}

ATF_TC_WITHOUT_HEAD(capnet__limits_deprecated_name2addr_hosts);
ATF_TC_BODY(capnet__limits_deprecated_name2addr_hosts, tc)
{
	cap_channel_t *capnet;
	cap_net_limit_t *limit;

	capnet = create_network_service();

	/* Limit to TEST_DOMAIN_0 and localhost only. */
	limit = cap_net_limit_init(capnet, CAPNET_DEPRECATED_NAME2ADDR);
	ATF_REQUIRE(limit != NULL);
	cap_net_limit_name2addr(limit, TEST_DOMAIN_0, NULL);
	cap_net_limit_name2addr(limit, "localhost", NULL);
	ATF_REQUIRE(cap_net_limit(limit) == 0);

	ATF_REQUIRE(test_gethostbyname(capnet, AF_INET, TEST_DOMAIN_0) == 0);
	ATF_REQUIRE(test_gethostbyname(capnet, AF_INET, "localhost") == 0);
	ATF_REQUIRE(
	    test_gethostbyname(capnet, AF_INET, TEST_DOMAIN_1) == ENOTCAPABLE);

	/* Limit to TEST_DOMAIN_0 only. */
	limit = cap_net_limit_init(capnet, CAPNET_DEPRECATED_NAME2ADDR);
	ATF_REQUIRE(limit != NULL);
	cap_net_limit_name2addr(limit, TEST_DOMAIN_0, NULL);
	ATF_REQUIRE(cap_net_limit(limit) == 0);

	ATF_REQUIRE(
	    test_gethostbyname(capnet, AF_INET, "localhost") == ENOTCAPABLE);
	ATF_REQUIRE(
	    test_gethostbyname(capnet, AF_INET, TEST_DOMAIN_1) == ENOTCAPABLE);
	ATF_REQUIRE(test_gethostbyname(capnet, AF_INET, TEST_DOMAIN_0) == 0);

	/* Unable to set empty limits. Empty limits means full access. */
	limit = cap_net_limit_init(capnet, CAPNET_DEPRECATED_NAME2ADDR);
	ATF_REQUIRE(cap_net_limit(limit) != 0);

	cap_close(capnet);
}

ATF_TC_WITHOUT_HEAD(capnet__limits_deprecated_name2addr_family);
ATF_TC_BODY(capnet__limits_deprecated_name2addr_family, tc)
{
	cap_channel_t *capnet;
	cap_net_limit_t *limit;
	int family[] = { AF_INET6, AF_INET };

	capnet = create_network_service();

	/* Limit to AF_INET and AF_INET6. */
	limit = cap_net_limit_init(capnet, CAPNET_DEPRECATED_NAME2ADDR);
	ATF_REQUIRE(limit != NULL);
	cap_net_limit_name2addr(limit, TEST_DOMAIN_0, NULL);
	cap_net_limit_name2addr_family(limit, family, nitems(family));
	ATF_REQUIRE(cap_net_limit(limit) == 0);

	ATF_REQUIRE(test_gethostbyname(capnet, AF_INET, TEST_DOMAIN_0) == 0);
	ATF_REQUIRE(test_gethostbyname(capnet, AF_INET6, TEST_DOMAIN_0) == 0);
	ATF_REQUIRE(
	    test_gethostbyname(capnet, PF_LINK, TEST_DOMAIN_0) == ENOTCAPABLE);

	/* Limit to AF_INET and AF_INET6. */
	limit = cap_net_limit_init(capnet, CAPNET_DEPRECATED_NAME2ADDR);
	ATF_REQUIRE(limit != NULL);
	cap_net_limit_name2addr(limit, TEST_DOMAIN_0, NULL);
	cap_net_limit_name2addr_family(limit, &family[0], 1);
	cap_net_limit_name2addr_family(limit, &family[1], 1);
	ATF_REQUIRE(cap_net_limit(limit) == 0);

	ATF_REQUIRE(test_gethostbyname(capnet, AF_INET, TEST_DOMAIN_0) == 0);
	ATF_REQUIRE(test_gethostbyname(capnet, AF_INET6, TEST_DOMAIN_0) == 0);
	ATF_REQUIRE(
	    test_gethostbyname(capnet, PF_LINK, TEST_DOMAIN_0) == ENOTCAPABLE);

	/* Limit to AF_INET6 only. */
	limit = cap_net_limit_init(capnet, CAPNET_DEPRECATED_NAME2ADDR);
	ATF_REQUIRE(limit != NULL);
	cap_net_limit_name2addr(limit, TEST_DOMAIN_0, NULL);
	cap_net_limit_name2addr_family(limit, family, 1);
	ATF_REQUIRE(cap_net_limit(limit) == 0);

	ATF_REQUIRE(
	    test_gethostbyname(capnet, AF_INET, TEST_DOMAIN_0) == ENOTCAPABLE);
	ATF_REQUIRE(test_gethostbyname(capnet, AF_INET6, TEST_DOMAIN_0) == 0);
	ATF_REQUIRE(
	    test_gethostbyname(capnet, PF_LINK, TEST_DOMAIN_0) == ENOTCAPABLE);

	/* Unable to set empty limits. Empty limits means full access. */
	limit = cap_net_limit_init(capnet, CAPNET_DEPRECATED_NAME2ADDR);
	ATF_REQUIRE(cap_net_limit(limit) != 0);

	cap_close(capnet);
}

ATF_TC_WITHOUT_HEAD(capnet__limits_bind_mode);
ATF_TC_BODY(capnet__limits_bind_mode, tc)
{
	cap_channel_t *capnet;
	cap_net_limit_t *limit;

	capnet = create_network_service();

	/* LIMIT */
	limit = cap_net_limit_init(capnet, CAPNET_BIND);
	ATF_REQUIRE(limit != NULL);
	ATF_REQUIRE(cap_net_limit(limit) == 0);

	/* ALLOWED */
	ATF_REQUIRE(test_bind(capnet, TEST_BIND_IPV4) == 0);

	/* DISALLOWED */
	ATF_REQUIRE(
	    test_gethostbyname(capnet, AF_INET, TEST_DOMAIN_0) == ENOTCAPABLE);
	ATF_REQUIRE(test_getnameinfo(capnet, AF_INET, TEST_IPV4) ==
	    ENOTCAPABLE);
	ATF_REQUIRE(test_gethostbyaddr(capnet, AF_INET, TEST_IPV4) ==
	    ENOTCAPABLE);
	ATF_REQUIRE(test_getaddrinfo(capnet, AF_INET, TEST_DOMAIN_0, NULL) ==
	    ENOTCAPABLE);
	ATF_REQUIRE(test_connect(capnet, TEST_IPV4, 80) == ENOTCAPABLE);

	test_extend_mode(capnet, CAPNET_ADDR2NAME);

	cap_close(capnet);
}

ATF_TC_WITHOUT_HEAD(capnet__limits_bind);
ATF_TC_BODY(capnet__limits_bind, tc)
{
	cap_channel_t *capnet;
	cap_net_limit_t *limit;
	struct sockaddr_in ipv4;

	capnet = create_network_service();

	limit = cap_net_limit_init(capnet, CAPNET_BIND);
	ATF_REQUIRE(limit != NULL);

	memset(&ipv4, 0, sizeof(ipv4));
	ipv4.sin_family = AF_INET;
	inet_pton(AF_INET, TEST_BIND_IPV4, &ipv4.sin_addr);

	cap_net_limit_bind(limit, (struct sockaddr *)&ipv4, sizeof(ipv4));
	ATF_REQUIRE(cap_net_limit(limit) == 0);

	ATF_REQUIRE(test_bind(capnet, TEST_BIND_IPV4) == 0);
	ATF_REQUIRE(test_bind(capnet, "127.0.0.2") == ENOTCAPABLE);

	cap_close(capnet);
}

ATF_TC_WITHOUT_HEAD(capnet__limits_connect_mode);
ATF_TC_BODY(capnet__limits_connect_mode, tc)
{
	cap_channel_t *capnet;
	cap_net_limit_t *limit;

	capnet = create_network_service();

	/* LIMIT */
	limit = cap_net_limit_init(capnet, CAPNET_CONNECT);
	ATF_REQUIRE(limit != NULL);
	ATF_REQUIRE(cap_net_limit(limit) == 0);

	/* ALLOWED */
	ATF_REQUIRE(test_connect(capnet, TEST_IPV4, 80) == 0);

	/* DISALLOWED */
	ATF_REQUIRE(
	    test_gethostbyname(capnet, AF_INET, TEST_DOMAIN_0) == ENOTCAPABLE);
	ATF_REQUIRE(test_getnameinfo(capnet, AF_INET, TEST_IPV4) ==
	    ENOTCAPABLE);
	ATF_REQUIRE(test_gethostbyaddr(capnet, AF_INET, TEST_IPV4) ==
	    ENOTCAPABLE);
	ATF_REQUIRE(test_getaddrinfo(capnet, AF_INET, TEST_DOMAIN_0, NULL) ==
	    ENOTCAPABLE);
	ATF_REQUIRE(test_bind(capnet, TEST_BIND_IPV4) == ENOTCAPABLE);

	test_extend_mode(capnet, CAPNET_ADDR2NAME);

	cap_close(capnet);
}

ATF_TC_WITHOUT_HEAD(capnet__limits_connect_dns_mode);
ATF_TC_BODY(capnet__limits_connect_dns_mode, tc)
{
	cap_channel_t *capnet;
	cap_net_limit_t *limit;

	capnet = create_network_service();

	/* LIMIT */
	limit = cap_net_limit_init(capnet, CAPNET_CONNECT | CAPNET_CONNECTDNS);
	ATF_REQUIRE(limit != NULL);
	ATF_REQUIRE(cap_net_limit(limit) == 0);

	/* ALLOWED */
	ATF_REQUIRE(test_connect(capnet, TEST_IPV4, 80) == 0);

	/* DISALLOWED */
	ATF_REQUIRE(
	    test_gethostbyname(capnet, AF_INET, TEST_DOMAIN_0) == ENOTCAPABLE);
	ATF_REQUIRE(test_getnameinfo(capnet, AF_INET, TEST_IPV4) ==
	    ENOTCAPABLE);
	ATF_REQUIRE(test_gethostbyaddr(capnet, AF_INET, TEST_IPV4) ==
	    ENOTCAPABLE);
	ATF_REQUIRE(test_getaddrinfo(capnet, AF_INET, TEST_DOMAIN_0, NULL) ==
	    ENOTCAPABLE);
	ATF_REQUIRE(test_bind(capnet, TEST_BIND_IPV4) == ENOTCAPABLE);

	test_extend_mode(capnet, CAPNET_ADDR2NAME);

	cap_close(capnet);
}

ATF_TC_WITHOUT_HEAD(capnet__limits_connect);
ATF_TC_BODY(capnet__limits_connect, tc)
{
	cap_channel_t *capnet;
	cap_net_limit_t *limit;
	struct sockaddr_in ipv4;

	capnet = create_network_service();

	/* Limit only to TEST_IPV4 on port 80 and 443. */
	limit = cap_net_limit_init(capnet, CAPNET_CONNECT);
	ATF_REQUIRE(limit != NULL);
	memset(&ipv4, 0, sizeof(ipv4));
	ipv4.sin_family = AF_INET;
	ipv4.sin_port = htons(80);
	inet_pton(AF_INET, TEST_IPV4, &ipv4.sin_addr);
	cap_net_limit_connect(limit, (struct sockaddr *)&ipv4, sizeof(ipv4));

	ipv4.sin_port = htons(443);
	cap_net_limit_connect(limit, (struct sockaddr *)&ipv4, sizeof(ipv4));
	ATF_REQUIRE(cap_net_limit(limit) == 0);

	ATF_REQUIRE(test_connect(capnet, TEST_IPV4, 80) == 0);
	ATF_REQUIRE(test_connect(capnet, "8.8.8.8", 80) == ENOTCAPABLE);
	ATF_REQUIRE(test_connect(capnet, "8.8.8.8", 433) == ENOTCAPABLE);
	ATF_REQUIRE(test_connect(capnet, TEST_IPV4, 443) == 0);

	/* Limit only to TEST_IPV4 on port 443. */
	limit = cap_net_limit_init(capnet, CAPNET_CONNECT);
	cap_net_limit_connect(limit, (struct sockaddr *)&ipv4, sizeof(ipv4));
	ATF_REQUIRE(cap_net_limit(limit) == 0);

	ATF_REQUIRE(test_connect(capnet, TEST_IPV4, 433) == ENOTCAPABLE);
	ATF_REQUIRE(test_connect(capnet, "8.8.8.8", 80) == ENOTCAPABLE);
	ATF_REQUIRE(test_connect(capnet, "8.8.8.8", 433) == ENOTCAPABLE);
	ATF_REQUIRE(test_connect(capnet, TEST_IPV4, 443) == 0);

	/* Unable to set empty limits. Empty limits means full access. */
	limit = cap_net_limit_init(capnet, CAPNET_CONNECT);
	ATF_REQUIRE(cap_net_limit(limit) != 0);

	cap_close(capnet);
}

ATF_TC_WITHOUT_HEAD(capnet__limits_connecttodns);
ATF_TC_BODY(capnet__limits_connecttodns, tc)
{
	cap_channel_t *capnet;
	cap_net_limit_t *limit;
	struct addrinfo hints, *capres, *res;
	int family[] = { AF_INET };
	int error;

	capnet = create_network_service();

	limit = cap_net_limit_init(capnet, CAPNET_CONNECTDNS |
	    CAPNET_NAME2ADDR);
	ATF_REQUIRE(limit != NULL);
	cap_net_limit_name2addr(limit, TEST_IPV4, "80");
	cap_net_limit_name2addr_family(limit, family, 1);
	ATF_REQUIRE(cap_net_limit(limit) == 0);

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	ATF_REQUIRE(test_connect(capnet, "8.8.8.8", 433) == ENOTCAPABLE);
	ATF_REQUIRE(cap_getaddrinfo(capnet, TEST_IPV4, "80", &hints, &capres) ==
	    0);
	ATF_REQUIRE(test_connect(capnet, "8.8.8.8", 433) == ENOTCAPABLE);

	for (res = capres; res != NULL; res = res->ai_next) {
		int s;

		ATF_REQUIRE(res->ai_family == AF_INET);
		ATF_REQUIRE(res->ai_socktype == SOCK_STREAM);

		s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		ATF_REQUIRE(s >= 0);

		error = cap_connect(capnet, s, res->ai_addr,
		    res->ai_addrlen);
		if (error != 0 && errno != ENOTCAPABLE)
			atf_tc_skip("unable to connect: %s", strerror(errno));
		ATF_REQUIRE(error == 0);
		ATF_REQUIRE(close(s) == 0);
	}

	freeaddrinfo(capres);
	cap_close(capnet);
}


ATF_TC_WITHOUT_HEAD(capnet__limits_deprecated_connecttodns);
ATF_TC_BODY(capnet__limits_deprecated_connecttodns, tc)
{
	cap_channel_t *capnet;
	cap_net_limit_t *limit;
	struct hostent *caphp;
	struct in_addr ipaddr;
	struct sockaddr_in connaddr;
	int family[] = { AF_INET };
	int error, i;

	capnet = create_network_service();

	limit = cap_net_limit_init(capnet, CAPNET_CONNECTDNS |
	    CAPNET_DEPRECATED_NAME2ADDR);
	ATF_REQUIRE(limit != NULL);
	cap_net_limit_name2addr(limit, TEST_IPV4, NULL);
	cap_net_limit_name2addr_family(limit, family, 1);
	ATF_REQUIRE(cap_net_limit(limit) == 0);

	memset(&ipaddr, 0, sizeof(ipaddr));
	inet_pton(AF_INET, TEST_IPV4, &ipaddr);

	ATF_REQUIRE(test_connect(capnet, "8.8.8.8", 433) == ENOTCAPABLE);
	caphp = cap_gethostbyname2(capnet, TEST_IPV4, AF_INET);
	ATF_REQUIRE(caphp != NULL);
	ATF_REQUIRE(caphp->h_addrtype == AF_INET);
	ATF_REQUIRE(test_connect(capnet, "8.8.8.8", 433) == ENOTCAPABLE);

	for (i = 0; caphp->h_addr_list[i] != NULL; i++) {
		int s;

		s = socket(AF_INET, SOCK_STREAM, 0);
		ATF_REQUIRE(s >= 0);

		memset(&connaddr, 0, sizeof(connaddr));
		connaddr.sin_family = AF_INET;
		memcpy((char *)&connaddr.sin_addr.s_addr,
		    (char *)caphp->h_addr_list[i], caphp->h_length);
		connaddr.sin_port = htons(80);

		error = cap_connect(capnet, s, (struct sockaddr *)&connaddr,
		    sizeof(connaddr));
		if (error != 0 && errno != ENOTCAPABLE)
			atf_tc_skip("unable to connect: %s", strerror(errno));
		ATF_REQUIRE(error == 0);
		ATF_REQUIRE(close(s) == 0);
	}

	cap_close(capnet);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, capnet__connect);
	ATF_TP_ADD_TC(tp, capnet__bind);
	ATF_TP_ADD_TC(tp, capnet__getnameinfo);
	ATF_TP_ADD_TC(tp, capnet__getaddrinfo);
	ATF_TP_ADD_TC(tp, capnet__gethostbyname);
	ATF_TP_ADD_TC(tp, capnet__gethostbyaddr);

	ATF_TP_ADD_TC(tp, capnet__getnameinfo_buffer);

	ATF_TP_ADD_TC(tp, capnet__limits_addr2name_mode);
	ATF_TP_ADD_TC(tp, capnet__limits_addr2name_family);
	ATF_TP_ADD_TC(tp, capnet__limits_addr2name);

	ATF_TP_ADD_TC(tp, capnet__limits_deprecated_addr2name_mode);
	ATF_TP_ADD_TC(tp, capnet__limits_deprecated_addr2name_family);
	ATF_TP_ADD_TC(tp, capnet__limits_deprecated_addr2name);

	ATF_TP_ADD_TC(tp, capnet__limits_name2addr_mode);
	ATF_TP_ADD_TC(tp, capnet__limits_name2addr_hosts);
	ATF_TP_ADD_TC(tp, capnet__limits_name2addr_hosts_servnames_strict);
	ATF_TP_ADD_TC(tp, capnet__limits_name2addr_hosts_servnames_mix);
	ATF_TP_ADD_TC(tp, capnet__limits_name2addr_family);

	ATF_TP_ADD_TC(tp, capnet__limits_deprecated_name2addr_mode);
	ATF_TP_ADD_TC(tp, capnet__limits_deprecated_name2addr_hosts);
	ATF_TP_ADD_TC(tp, capnet__limits_deprecated_name2addr_family);

	ATF_TP_ADD_TC(tp, capnet__limits_bind_mode);
	ATF_TP_ADD_TC(tp, capnet__limits_bind);

	ATF_TP_ADD_TC(tp, capnet__limits_connect_mode);
	ATF_TP_ADD_TC(tp, capnet__limits_connect_dns_mode);
	ATF_TP_ADD_TC(tp, capnet__limits_connect);

	ATF_TP_ADD_TC(tp, capnet__limits_connecttodns);
	ATF_TP_ADD_TC(tp, capnet__limits_deprecated_connecttodns);

	return (atf_no_error());
}
