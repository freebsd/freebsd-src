/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Alexander V. Chernikov
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

#ifndef _NET_ROUTING_RTSOCK_COMMON_H_
#define _NET_ROUTING_RTSOCK_COMMON_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <ctype.h>
#include <poll.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/jail.h>
#include <sys/linker.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>

#include <arpa/inet.h>
#include <net/ethernet.h>

#include <netinet/in.h>
#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>

#include <ifaddrs.h>

#include <errno.h>
#include <err.h>
#include <sysexits.h>

#include <atf-c.h>
#include "freebsd_test_suite/macros.h"

#include "rtsock_print.h"
#include "params.h"

void rtsock_update_rtm_len(struct rt_msghdr *rtm);
void rtsock_validate_message(char *buffer, ssize_t len);
void rtsock_add_rtm_sa(struct rt_msghdr *rtm, int addr_type, struct sockaddr *sa);

void file_append_line(char *fname, char *text);

static int _rtm_seq = 42;


/*
 * Checks if the interface cloner module is present for @name.
 */
static int
_check_cloner(char *name)
{
	struct if_clonereq ifcr;
	char *cp, *buf;
	int idx;
	int s;
	int found = 0;

	s = socket(AF_LOCAL, SOCK_DGRAM, 0);
	if (s == -1)
		err(1, "socket(AF_LOCAL,SOCK_DGRAM)");

	memset(&ifcr, 0, sizeof(ifcr));

	if (ioctl(s, SIOCIFGCLONERS, &ifcr) < 0)
		err(1, "SIOCIFGCLONERS for count");

	buf = malloc(ifcr.ifcr_total * IFNAMSIZ);
	if (buf == NULL)
		err(1, "unable to allocate cloner name buffer");

	ifcr.ifcr_count = ifcr.ifcr_total;
	ifcr.ifcr_buffer = buf;

	if (ioctl(s, SIOCIFGCLONERS, &ifcr) < 0)
		err(1, "SIOCIFGCLONERS for names");

	/*
	 * In case some disappeared in the mean time, clamp it down.
	 */
	if (ifcr.ifcr_count > ifcr.ifcr_total)
		ifcr.ifcr_count = ifcr.ifcr_total;

	for (cp = buf, idx = 0; idx < ifcr.ifcr_count; idx++, cp += IFNAMSIZ) {
		if (!strcmp(cp, name)) {
			found = 1;
			break;
		}
	}

	free(buf);
	close(s);

	return (found);
}

static char *
iface_create(char *ifname_orig)
{
	struct ifreq ifr;
	int s;
	char prefix[IFNAMSIZ], ifname[IFNAMSIZ], *result;

	char *src, *dst;
	for (src = ifname_orig, dst = prefix; *src && isalpha(*src); src++)
		*dst++ = *src;
	*dst = '\0';

	memset(&ifr, 0, sizeof(struct ifreq));

	s = socket(AF_LOCAL, SOCK_DGRAM, 0);
	strlcpy(ifr.ifr_name, ifname_orig, sizeof(ifr.ifr_name));

	RLOG("creating iface %s %s", prefix, ifr.ifr_name);
	if (ioctl(s, SIOCIFCREATE2, &ifr) < 0)
		err(1, "SIOCIFCREATE2");

	strlcpy(ifname, ifr.ifr_name, IFNAMSIZ);
	RLOG("created interface %s", ifname);

	result = strdup(ifname);

	file_append_line(IFACES_FNAME, ifname);
	if (strstr(ifname, "epair") == ifname) {
		/* call returned epairXXXa, need to add epairXXXb */
		ifname[strlen(ifname) - 1] = 'b';
		file_append_line(IFACES_FNAME, ifname);
	}

	return (result);
}

static int
iface_destroy(char *ifname)
{
	struct ifreq ifr;
	int s;

	s = socket(AF_LOCAL, SOCK_DGRAM, 0);
	strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));

	RLOG("destroying interface %s", ifname);
	if (ioctl(s, SIOCIFDESTROY, &ifr) < 0)
		return (0);

	return (1);
}

/*
 * Open tunneling device such as tuntap and returns fd.
 */
int
iface_open(char *ifname)
{
	char path[256];

	snprintf(path, sizeof(path), "/dev/%s", ifname);

	RLOG("opening interface %s", ifname);
	int fd = open(path, O_RDWR|O_EXCL);
	if (fd == -1) {
		RLOG_ERRNO("unable to open interface %s", ifname);
		return (-1);
	}

	return (fd);
}

/*
 * Sets primary IPv4 addr.
 * Returns 0 on success.
 */
static inline int
iface_setup_addr(char *ifname, char *addr, int plen)
{
	char cmd[512];
	char *af;

	if (strchr(addr, ':'))
		af = "inet6";
	else
		af = "inet";
	RLOG("setting af_%s %s/%d on %s", af, addr, plen, ifname);
	snprintf(cmd, sizeof(cmd), "/sbin/ifconfig %s %s %s/%d", ifname,
		af, addr, plen);

	return system(cmd);
}

/*
 * Removes primary IPv4 prefix.
 * Returns 0 on success.
 */
static inline int
iface_delete_addr(char *ifname, char *addr)
{
	char cmd[512];

	if (strchr(addr, ':')) {
		RLOG("removing IPv6 %s from %s", addr, ifname);
		snprintf(cmd, sizeof(cmd), "/sbin/ifconfig %s inet6 %s delete", ifname, addr);
	} else {
		RLOG("removing IPv4 %s from %s", addr, ifname);
		snprintf(cmd, sizeof(cmd), "/sbin/ifconfig %s -alias %s", ifname, addr);
	}

	return system(cmd);
}

int
iface_turn_up(char *ifname)
{
	struct ifreq ifr;
	int s;

	if ((s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
		RLOG_ERRNO("socket");
		return (-1);
	}
	memset(&ifr, 0, sizeof(struct ifreq));
	strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	if (ioctl(s, SIOCGIFFLAGS, (caddr_t)&ifr) < 0) {
		RLOG_ERRNO("ioctl(SIOCGIFFLAGS)");
		return (-1);
	}
	/* Update flags */
	if ((ifr.ifr_flags & IFF_UP) == 0) {
		ifr.ifr_flags |= IFF_UP;
		if (ioctl(s, SIOCSIFFLAGS, (caddr_t)&ifr) < 0) {
			RLOG_ERRNO("ioctl(SIOSGIFFLAGS)");
			return (-1);
		}
		RLOG("turned interface %s up", ifname);
	}

	return (0);
}

/*
 * Removes ND6_IFF_IFDISABLED from IPv6 interface flags.
 * Returns 0 on success.
 */
int
iface_enable_ipv6(char *ifname)
{
	struct in6_ndireq nd;
	int s;

	if ((s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
		err(1, "socket");
	}
	memset(&nd, 0, sizeof(nd));
	strlcpy(nd.ifname, ifname, sizeof(nd.ifname));
	if (ioctl(s, SIOCGIFINFO_IN6, (caddr_t)&nd) < 0) {
		RLOG_ERRNO("ioctl(SIOCGIFINFO_IN6)");
		return (-1);
	}
	/* Update flags */
	if ((nd.ndi.flags & ND6_IFF_IFDISABLED) != 0) {
		nd.ndi.flags &= ~ND6_IFF_IFDISABLED;
		if (ioctl(s, SIOCSIFINFO_IN6, (caddr_t)&nd) < 0) {
			RLOG_ERRNO("ioctl(SIOCSIFINFO_IN6)");
			return (-1);
		}
		RLOG("enabled IPv6 for %s", ifname);
	}

	return (0);
}

void
file_append_line(char *fname, char *text)
{
	FILE *f;

	f = fopen(fname, "a");
	fputs(text, f);
	fputs("\n", f);
	fclose(f);
}

static int
vnet_wait_interface(char *vnet_name, char *ifname)
{
	char buf[512], cmd[512], *line, *token;
	FILE *fp;
	int i;

	snprintf(cmd, sizeof(cmd), "/usr/sbin/jexec %s /sbin/ifconfig -l", vnet_name);
	for (int i = 0; i < 50; i++) {
		fp = popen(cmd, "r");
		line = fgets(buf, sizeof(buf), fp);
		/* cut last\n */
		if (line[0])
			line[strlen(line)-1] = '\0';
		while ((token = strsep(&line, " ")) != NULL) {
			if (strcmp(token, ifname) == 0)
				return (1);
		}

		/* sleep 100ms */
		usleep(1000 * 100);
	}

	return (0);
}

void
vnet_switch(char *vnet_name, char **ifnames, int count)
{
	char buf[512], cmd[512], *line;
	FILE *fp;
	int jid, len, ret;

	RLOG("switching to vnet %s with interface(s) %s", vnet_name, ifnames[0]);
	len = snprintf(cmd, sizeof(cmd),
	    "/usr/sbin/jail -i -c name=%s persist vnet", vnet_name);
	for (int i = 0; i < count && len < sizeof(cmd); i++) {
		len += snprintf(&cmd[len], sizeof(cmd) - len,
		    " vnet.interface=%s", ifnames[i]);
	}
	RLOG("jail cmd: \"%s\"\n", cmd);

	fp = popen(cmd, "r");
	if (fp == NULL)
		atf_tc_fail("jail creation failed");
	line = fgets(buf, sizeof(buf), fp);
	if (line == NULL)
		atf_tc_fail("empty output from jail(8)");
	jid = strtol(line, NULL, 10);
	if (jid <= 0) {
		atf_tc_fail("invalid jail output: %s", line);
	}

	RLOG("created jail jid=%d", jid);
	file_append_line(JAILS_FNAME, vnet_name);

	/* Wait while interface appearsh inside vnet */
	for (int i = 0; i < count; i++) {
		if (vnet_wait_interface(vnet_name, ifnames[i]))
			continue;
		atf_tc_fail("unable to move interface %s to jail %s",
		    ifnames[i], vnet_name);
	}

	if (jail_attach(jid) == -1) {
		RLOG_ERRNO("jail %s attach failed: ret=%d", vnet_name, errno);
		atf_tc_fail("jail attach failed");
	}

	RLOG("attached to the jail");
}

void
vnet_switch_one(char *vnet_name, char *ifname)
{
	char *ifnames[1];

	ifnames[0] = ifname;
	vnet_switch(vnet_name, ifnames, 1);
}


#define	SA_F_IGNORE_IFNAME	0x01
#define	SA_F_IGNORE_IFTYPE	0x02
#define	SA_F_IGNORE_MEMCMP	0x04
int
sa_equal_msg_flags(const struct sockaddr *a, const struct sockaddr *b, char *msg, size_t sz, int flags)
{
	char a_s[64], b_s[64];
	const struct sockaddr_in *a4, *b4;
	const struct sockaddr_in6 *a6, *b6;
	const struct sockaddr_dl *al, *bl;

	if (a == NULL) {
		snprintf(msg, sz, "first sa is NULL");
		return 0;
	}
	if (b == NULL) {
		snprintf(msg, sz, "second sa is NULL");
		return 0;
	}

	if (a->sa_family != b->sa_family) {
		snprintf(msg, sz, "family: %d vs %d", a->sa_family, b->sa_family);
		return 0;
	}
	if (a->sa_len != b->sa_len) {
		snprintf(msg, sz, "len: %d vs %d", a->sa_len, b->sa_len);
		return 0;
	}

	switch (a->sa_family) {
	case AF_INET:
		a4 = (const struct sockaddr_in *)a;
		b4 = (const struct sockaddr_in *)b;
		if (a4->sin_addr.s_addr != b4->sin_addr.s_addr) {
			inet_ntop(AF_INET, &a4->sin_addr, a_s, sizeof(a_s));
			inet_ntop(AF_INET, &b4->sin_addr, b_s, sizeof(b_s));
			snprintf(msg, sz, "addr diff: %s vs %s", a_s, b_s);
			return 0;
		}
		if (a4->sin_port != b4->sin_port) {
			snprintf(msg, sz, "port diff: %d vs %d",
					ntohs(a4->sin_port), ntohs(b4->sin_port));
			//return 0;
		}
		const uint32_t *a32, *b32;
		a32 = (const uint32_t *)a4->sin_zero;
		b32 = (const uint32_t *)b4->sin_zero;
		if ((*a32 != *b32) || (*(a32 + 1) != *(b32 + 1))) {
			snprintf(msg, sz, "zero diff: 0x%08X%08X vs 0x%08X%08X",
					ntohl(*a32), ntohl(*(a32 + 1)),
					ntohl(*b32), ntohl(*(b32 + 1)));
			return 0;
		}
		return 1;
	case AF_INET6:
		a6 = (const struct sockaddr_in6 *)a;
		b6 = (const struct sockaddr_in6 *)b;
		if (!IN6_ARE_ADDR_EQUAL(&a6->sin6_addr, &b6->sin6_addr)) {
			inet_ntop(AF_INET6, &a6->sin6_addr, a_s, sizeof(a_s));
			inet_ntop(AF_INET6, &b6->sin6_addr, b_s, sizeof(b_s));
			snprintf(msg, sz, "addr diff: %s vs %s", a_s, b_s);
			return 0;
		}
		if (a6->sin6_scope_id != b6->sin6_scope_id) {
			snprintf(msg, sz, "scope diff: %u vs %u", a6->sin6_scope_id, b6->sin6_scope_id);
			return 0;
		}
		break;
	case AF_LINK:
		al = (const struct sockaddr_dl *)a;
		bl = (const struct sockaddr_dl *)b;

		if (al->sdl_index != bl->sdl_index) {
			snprintf(msg, sz, "sdl_index diff: %u vs %u", al->sdl_index, bl->sdl_index);
			return 0;
		}

		if ((al->sdl_alen != bl->sdl_alen) || (memcmp(LLADDR(al), LLADDR(bl), al->sdl_alen) != 0)) {
			char abuf[64], bbuf[64];
			sa_print_hd(abuf, sizeof(abuf), LLADDR(al), al->sdl_alen);
			sa_print_hd(bbuf, sizeof(bbuf), LLADDR(bl), bl->sdl_alen);
			snprintf(msg, sz, "sdl_alen diff: {%s} (%d) vs {%s} (%d)",
			    abuf, al->sdl_alen, bbuf, bl->sdl_alen);
			return 0;
		}

		if (((flags & SA_F_IGNORE_IFTYPE) == 0) && (al->sdl_type != bl->sdl_type)) {
			snprintf(msg, sz, "sdl_type diff: %u vs %u", al->sdl_type, bl->sdl_type);
			return 0;
		}

		if (((flags & SA_F_IGNORE_IFNAME) == 0) && ((al->sdl_nlen != bl->sdl_nlen) ||
			    (memcmp(al->sdl_data, bl->sdl_data, al->sdl_nlen) != 0))) {
			char abuf[64], bbuf[64];
			memcpy(abuf, al->sdl_data, al->sdl_nlen);
			abuf[al->sdl_nlen] = '\0';
			memcpy(bbuf, bl->sdl_data, bl->sdl_nlen);
			abuf[bl->sdl_nlen] = '\0';
			snprintf(msg, sz, "sdl_nlen diff: {%s} (%d) vs {%s} (%d)",
			    abuf, al->sdl_nlen, bbuf, bl->sdl_nlen);
			return 0;
		}

		if (flags & SA_F_IGNORE_MEMCMP)
			return 1;
		break;
	}

	if (memcmp(a, b, a->sa_len)) {
		int i;
		for (i = 0; i < a->sa_len; i++)
			if (((const char *)a)[i] != ((const char *)b)[i])
				break;

		sa_print(a, 1);
		sa_print(b, 1);

		snprintf(msg, sz, "overall memcmp() reports diff for af %d offset %d",
				a->sa_family, i);
		return 0;
	}
	return 1;
}

int
sa_equal_msg(const struct sockaddr *a, const struct sockaddr *b, char *msg, size_t sz)
{

	return sa_equal_msg_flags(a, b, msg, sz, 0);
}

void
sa_fill_mask4(struct sockaddr_in *sin, int plen)
{

	memset(sin, 0, sizeof(struct sockaddr_in));
	sin->sin_family = AF_INET;
	sin->sin_len = sizeof(struct sockaddr_in);
	sin->sin_addr.s_addr = htonl(plen ? ~((1 << (32 - plen)) - 1) : 0);
}

void
sa_fill_mask6(struct sockaddr_in6 *sin6, uint8_t mask)
{
	uint32_t *cp;

	memset(sin6, 0, sizeof(struct sockaddr_in6));
	sin6->sin6_family = AF_INET6;
	sin6->sin6_len = sizeof(struct sockaddr_in6);

	for (cp = (uint32_t *)&sin6->sin6_addr; mask >= 32; mask -= 32)
		*cp++ = 0xFFFFFFFF;
	if (mask > 0)
		*cp = htonl(mask ? ~((1 << (32 - mask)) - 1) : 0);
}

/* 52:54:00:14:e3:10 */
#define	ETHER_MAC_MAX_LENGTH	17

int
sa_convert_str_to_sa(const char *_addr, struct sockaddr *sa)
{
	int error;

	int af = AF_UNSPEC;

	char *addr = strdup(_addr);
	int retcode = 0;

	/* classify AF by str */
	if (strchr(addr, ':')) {
		/* inet6 or ether */
		char *k;
		int delim_cnt = 0;
		for (k = addr; *k; k++)
			if (*k == ':')
				delim_cnt++;
		af = AF_INET6;

		if (delim_cnt == 5) {
			k = strchr(addr, '%');
			if (k != NULL && (k - addr) <= ETHER_MAC_MAX_LENGTH)
				af = AF_LINK;
		}
	} else if (strchr(addr, '.'))
		af = AF_INET;

	/* */
	char *delimiter;
	int ifindex = 0;
	char *ifname = NULL;
	if ((delimiter = strchr(addr, '%')) != NULL) {
		*delimiter = '\0';
		ifname = delimiter + 1;
		ifindex = if_nametoindex(ifname);
		if (ifindex == 0)
			RLOG("unable to find ifindex for '%s'", ifname);
		else
			RLOG("if %s mapped to %d", ifname, ifindex);
	}

	if (af == AF_INET6) {
		struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)sa;
		memset(sin6, 0, sizeof(struct sockaddr_in6));
		sin6->sin6_family = AF_INET6;
		sin6->sin6_len = sizeof(struct sockaddr_in6);
		sin6->sin6_scope_id = ifindex;
		error = inet_pton(AF_INET6, addr, &sin6->sin6_addr);
		if (error != 1)
			RLOG_ERRNO("inet_ntop() failed: ret=%d", error);
		else
			retcode = 1;
	} else if (af == AF_INET) {
		struct sockaddr_in *sin = (struct sockaddr_in *)sa;
		memset(sin, 0, sizeof(struct sockaddr_in));
		sin->sin_family = AF_INET;
		sin->sin_len = sizeof(struct sockaddr_in);
		error = inet_pton(AF_INET, addr, &sin->sin_addr);
		if (error != 1)
			RLOG("inet_ntop() failed: ret=%d", error);
		else
			retcode = 1;
	} else if (af == AF_LINK) {
		struct sockaddr_dl *sdl = (struct sockaddr_dl *)sa;
		memset(sdl, 0, sizeof(struct sockaddr_dl));
		sdl->sdl_family = AF_LINK;
		sdl->sdl_len = sizeof(struct sockaddr_dl);
		sdl->sdl_index = ifindex;
		sdl->sdl_alen = 6;
		struct ether_addr *ea = (struct ether_addr *)LLADDR(sdl);
		if (ether_aton_r(addr, ea) == NULL)
			RLOG("ether_aton() failed");
		else
			retcode = 1;
	}

	return (retcode);
}


int
rtsock_setup_socket()
{
	int fd;
	int af = AF_UNSPEC; /* 0 to capture messages from all AFs */
	fd = socket(PF_ROUTE, SOCK_RAW, af);

	ATF_REQUIRE_MSG(fd != -1, "rtsock open failed: %s", strerror(errno));

	/* Listen for our messages */
	int on = 1;
	if (setsockopt(fd, SOL_SOCKET,SO_USELOOPBACK, &on, sizeof(on)) < 0)
		RLOG_ERRNO("setsockopt failed");

	return (fd);
}

ssize_t
rtsock_send_rtm(int fd, struct rt_msghdr *rtm)
{
	int my_errno;
	ssize_t len;

	rtsock_update_rtm_len(rtm);

	len = write(fd, rtm, rtm->rtm_msglen);
	my_errno = errno;
	RTSOCK_ATF_REQUIRE_MSG(rtm, len == rtm->rtm_msglen,
	    "rtsock write failed: want %d got %zd (%s)",
	    rtm->rtm_msglen, len, strerror(my_errno));

	return (len);
}

struct rt_msghdr *
rtsock_read_rtm(int fd, char *buffer, size_t buflen)
{
	ssize_t len;
	struct pollfd pfd;
	int poll_delay = 5 * 1000; /* 5 seconds */

	/* Check for the data available to read first */
	memset(&pfd, 0, sizeof(pfd));
	pfd.fd = fd;
	pfd.events = POLLIN;

	if (poll(&pfd, 1, poll_delay) == 0)
		ATF_REQUIRE_MSG(1 == 0, "rtsock read timed out (%d seconds passed)",
		    poll_delay / 1000);

	len = read(fd, buffer, buflen);
	int my_errno = errno;
	ATF_REQUIRE_MSG(len > 0, "rtsock read failed: %s", strerror(my_errno));

	rtsock_validate_message(buffer, len);
	return ((struct rt_msghdr *)buffer);
}

struct rt_msghdr *
rtsock_read_rtm_reply(int fd, char *buffer, size_t buflen, int seq)
{
	struct rt_msghdr *rtm;
	int found = 0;

	while (true) {
		rtm = rtsock_read_rtm(fd, buffer, buflen);
		if (rtm->rtm_pid == getpid() && rtm->rtm_seq == seq)
			found = 1;
		if (found)
			RLOG("--- MATCHED RTSOCK MESSAGE ---");
		else
			RLOG("--- SKIPPED RTSOCK MESSAGE ---");
		rtsock_print_rtm(rtm);
		if (found)
			return (rtm);
	}

	/* NOTREACHED */
}

void
rtsock_prepare_route_message_base(struct rt_msghdr *rtm, int cmd)
{

	memset(rtm, 0, sizeof(struct rt_msghdr));
	rtm->rtm_type = cmd;
	rtm->rtm_version = RTM_VERSION;
	rtm->rtm_seq = _rtm_seq++;
}

void
rtsock_prepare_route_message(struct rt_msghdr *rtm, int cmd, struct sockaddr *dst,
  struct sockaddr *mask, struct sockaddr *gw)
{

	rtsock_prepare_route_message_base(rtm, cmd);
	if (dst != NULL)
		rtsock_add_rtm_sa(rtm, RTA_DST, dst);

	if (gw != NULL) {
		rtsock_add_rtm_sa(rtm, RTA_GATEWAY, gw);
		rtm->rtm_flags |= RTF_GATEWAY;
	}

	if (mask != NULL)
		rtsock_add_rtm_sa(rtm, RTA_NETMASK, mask);
}

void
rtsock_add_rtm_sa(struct rt_msghdr *rtm, int addr_type, struct sockaddr *sa)
{
	char *ptr = (char *)(rtm + 1);
	for (int i = 0; i < RTAX_MAX; i++) {
		if (rtm->rtm_addrs & (1 << i)) {
			/* add */
			ptr += ALIGN(((struct sockaddr *)ptr)->sa_len);
		}
	}

	rtm->rtm_addrs |= addr_type;
	memcpy(ptr, sa, sa->sa_len);
}

struct sockaddr *
rtsock_find_rtm_sa(struct rt_msghdr *rtm, int addr_type)
{
	char *ptr = (char *)(rtm + 1);
	for (int i = 0; i < RTAX_MAX; i++) {
		if (rtm->rtm_addrs & (1 << i)) {
			if (addr_type == (1 << i))
				return ((struct sockaddr *)ptr);
			/* add */
			ptr += ALIGN(((struct sockaddr *)ptr)->sa_len);
		}
	}

	return (NULL);
}

size_t
rtsock_calc_rtm_len(struct rt_msghdr *rtm)
{
	size_t len = sizeof(struct rt_msghdr);

	char *ptr = (char *)(rtm + 1);
	for (int i = 0; i < RTAX_MAX; i++) {
		if (rtm->rtm_addrs & (1 << i)) {
			/* add */
			int sa_len = ALIGN(((struct sockaddr *)ptr)->sa_len);
			len += sa_len;
			ptr += sa_len;
		}
	}

	return len;
}

void
rtsock_update_rtm_len(struct rt_msghdr *rtm)
{

	rtm->rtm_msglen = rtsock_calc_rtm_len(rtm);
}

static void
_validate_message_sockaddrs(char *buffer, int rtm_len, size_t offset, int rtm_addrs)
{
	struct sockaddr *sa;
	size_t parsed_len = offset;

	/* Offset denotes initial header size */
	sa = (struct sockaddr *)(buffer + offset);

	for (int i = 0; i < RTAX_MAX; i++) {
		if ((rtm_addrs & (1 << i)) == 0)
			continue;
		parsed_len += SA_SIZE(sa);
		RTSOCK_ATF_REQUIRE_MSG((struct rt_msghdr *)buffer, parsed_len <= rtm_len,
		    "SA %d: len %d exceeds msg size %d", i, (int)sa->sa_len, rtm_len);
		if (sa->sa_family == AF_LINK) {
			struct sockaddr_dl *sdl = (struct sockaddr_dl *)sa;
			int data_len = sdl->sdl_nlen + sdl->sdl_alen;
			data_len += offsetof(struct sockaddr_dl, sdl_data);

			RTSOCK_ATF_REQUIRE_MSG((struct rt_msghdr *)buffer,
			    data_len <= rtm_len,
			    "AF_LINK data size exceeds total len: %u vs %u, nlen=%d alen=%d",
			    data_len, rtm_len, sdl->sdl_nlen, sdl->sdl_alen);
		}
		sa = (struct sockaddr *)((char *)sa + SA_SIZE(sa));
	}
}

/*
 * Raises error if base syntax checks fails.
 */
void
rtsock_validate_message(char *buffer, ssize_t len)
{
	struct rt_msghdr *rtm;

	ATF_REQUIRE_MSG(len > 0, "read() return %zd, error: %s", len, strerror(errno));

	rtm = (struct rt_msghdr *)buffer;
	ATF_REQUIRE_MSG(rtm->rtm_version == RTM_VERSION, "unknown RTM_VERSION: expected %d got %d",
			RTM_VERSION, rtm->rtm_version);
	ATF_REQUIRE_MSG(rtm->rtm_msglen <= len, "wrong message length: expected %d got %d",
			(int)len, (int)rtm->rtm_msglen);

	switch (rtm->rtm_type) {
	case RTM_GET:
	case RTM_ADD:
	case RTM_DELETE:
	case RTM_CHANGE:
		_validate_message_sockaddrs(buffer, rtm->rtm_msglen,
		    sizeof(struct rt_msghdr), rtm->rtm_addrs);
		break;
	case RTM_DELADDR:
	case RTM_NEWADDR:
		_validate_message_sockaddrs(buffer, rtm->rtm_msglen,
		    sizeof(struct ifa_msghdr), ((struct ifa_msghdr *)buffer)->ifam_addrs);
		break;
	}
}

void
rtsock_validate_pid_ours(struct rt_msghdr *rtm)
{
	RTSOCK_ATF_REQUIRE_MSG(rtm, rtm->rtm_pid == getpid(), "expected pid %d, got %d",
	    getpid(), rtm->rtm_pid);
}

void
rtsock_validate_pid_user(struct rt_msghdr *rtm)
{
	RTSOCK_ATF_REQUIRE_MSG(rtm, rtm->rtm_pid > 0, "expected non-zero pid, got %d",
	    rtm->rtm_pid);
}

void
rtsock_validate_pid_kernel(struct rt_msghdr *rtm)
{
	RTSOCK_ATF_REQUIRE_MSG(rtm, rtm->rtm_pid == 0, "expected zero pid, got %d",
	    rtm->rtm_pid);
}

#endif
