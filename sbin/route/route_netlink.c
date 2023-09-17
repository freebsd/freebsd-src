#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <errno.h>

#include <sys/bitcount.h>
#include <sys/param.h>
#include <sys/linker.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/types.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <netlink/netlink.h>
#include <netlink/netlink_route.h>
#include <netlink/netlink_snl.h>
#include <netlink/netlink_snl_route.h>
#include <netlink/netlink_snl_route_compat.h>
#include <netlink/netlink_snl_route_parsers.h>

const char *routename(struct sockaddr *);
const char *netname(struct sockaddr *);
void printb(int, const char *);
extern const char routeflags[];
extern int verbose, debugonly;

int rtmsg_nl(int cmd, int rtm_flags, int fib, int rtm_addrs, struct sockaddr_storage *so,
    struct rt_metrics *rt_metrics);
int flushroutes_fib_nl(int fib, int af);
void monitor_nl(int fib);

struct nl_helper;
struct snl_msg_info;
static void print_getmsg(struct nl_helper *h, struct nlmsghdr *hdr,
    struct sockaddr *dst);
static void print_nlmsg(struct nl_helper *h, struct nlmsghdr *hdr,
    struct snl_msg_info *cinfo);

#define s6_addr32 __u6_addr.__u6_addr32
#define	bitcount32(x)	__bitcount32((uint32_t)(x))
static int
inet6_get_plen(const struct in6_addr *addr)
{

	return (bitcount32(addr->s6_addr32[0]) + bitcount32(addr->s6_addr32[1]) +
	    bitcount32(addr->s6_addr32[2]) + bitcount32(addr->s6_addr32[3]));
}

static void
ip6_writemask(struct in6_addr *addr6, uint8_t mask)
{
	uint32_t *cp;

	for (cp = (uint32_t *)addr6; mask >= 32; mask -= 32)
		*cp++ = 0xFFFFFFFF;
	if (mask > 0)
		*cp = htonl(mask ? ~((1 << (32 - mask)) - 1) : 0);
}

static struct sockaddr *
get_netmask(struct snl_state *ss, int family, int plen)
{
	if (family == AF_INET) {
		if (plen == 32)
			return (NULL);

		struct sockaddr_in *sin = snl_allocz(ss, sizeof(*sin));

		sin->sin_len = sizeof(*sin);
		sin->sin_family = family;
		sin->sin_addr.s_addr = htonl(plen ? ~((1 << (32 - plen)) - 1) : 0);

		return (struct sockaddr *)sin;
	} else if (family == AF_INET6) {
		if (plen == 128)
			return (NULL);

		struct sockaddr_in6 *sin6 = snl_allocz(ss, sizeof(*sin6));

		sin6->sin6_len = sizeof(*sin6);
		sin6->sin6_family = family;
		ip6_writemask(&sin6->sin6_addr, plen);

		return (struct sockaddr *)sin6;
	}
	return (NULL);
}

static void
nl_init_socket(struct snl_state *ss)
{
	if (snl_init(ss, NETLINK_ROUTE))
		return;

	if (modfind("netlink") == -1 && errno == ENOENT) {
		/* Try to load */
		if (kldload("netlink") == -1)
			err(1, "netlink is not loaded and load attempt failed");
		if (snl_init(ss, NETLINK_ROUTE))
			return;
	}

	err(1, "unable to open netlink socket");
}

struct nl_helper {
	struct snl_state ss_cmd;
};

static void
nl_helper_init(struct nl_helper *h)
{
	nl_init_socket(&h->ss_cmd);
}

static void
nl_helper_free(struct nl_helper *h)
{
	snl_free(&h->ss_cmd);
}

static struct sockaddr *
get_addr(struct sockaddr_storage *so, int rtm_addrs, int addr_type)
{
	struct sockaddr *sa = NULL;

	if (rtm_addrs & (1 << addr_type))
		sa = (struct sockaddr *)&so[addr_type];
	return (sa);
}

static int
rtmsg_nl_int(struct nl_helper *h, int cmd, int rtm_flags, int fib, int rtm_addrs,
    struct sockaddr_storage *so, struct rt_metrics *rt_metrics)
{
	struct snl_state *ss = &h->ss_cmd;
	struct snl_writer nw;
	int nl_type = 0, nl_flags = 0;

	snl_init_writer(ss, &nw);

	switch (cmd) {
	case RTSOCK_RTM_ADD:
		nl_type = RTM_NEWROUTE;
		nl_flags = NLM_F_CREATE | NLM_F_APPEND; /* Do append by default */
		break;
	case RTSOCK_RTM_CHANGE:
		nl_type = RTM_NEWROUTE;
		nl_flags = NLM_F_REPLACE;
		break;
	case RTSOCK_RTM_DELETE:
		nl_type = RTM_DELROUTE;
		break;
	case RTSOCK_RTM_GET:
		nl_type = RTM_GETROUTE;
		break;
	default:
		exit(1);
	}

	struct sockaddr *dst = get_addr(so, rtm_addrs, RTAX_DST);
	struct sockaddr *mask = get_addr(so, rtm_addrs, RTAX_NETMASK);
	struct sockaddr *gw = get_addr(so, rtm_addrs, RTAX_GATEWAY);

	if (dst == NULL)
		return (EINVAL);

	struct nlmsghdr *hdr = snl_create_msg_request(&nw, nl_type);
	hdr->nlmsg_flags |= nl_flags;

	int plen = 0;
	int rtm_type = RTN_UNICAST;

	switch (dst->sa_family) {
	case AF_INET:
	    {
		struct sockaddr_in *mask4 = (struct sockaddr_in *)mask;

		if ((rtm_flags & RTF_HOST) == 0 && mask4 != NULL)
			plen = bitcount32(mask4->sin_addr.s_addr);
		else
			plen = 32;
		break;
	    }
	case AF_INET6:
	    {
		struct sockaddr_in6 *mask6 = (struct sockaddr_in6 *)mask;

		if ((rtm_flags & RTF_HOST) == 0 && mask6 != NULL)
			plen = inet6_get_plen(&mask6->sin6_addr);
		else
			plen = 128;
		break;
	    }
	default:
		return (ENOTSUP);
	}

	if (rtm_flags & RTF_REJECT)
		rtm_type = RTN_PROHIBIT;
	else if (rtm_flags & RTF_BLACKHOLE)
		rtm_type = RTN_BLACKHOLE;

	struct rtmsg *rtm = snl_reserve_msg_object(&nw, struct rtmsg);
	rtm->rtm_family = dst->sa_family;
	rtm->rtm_protocol = RTPROT_STATIC;
	rtm->rtm_type = rtm_type;
	rtm->rtm_dst_len = plen;

	/* Request exact prefix match if mask is set */
	if ((cmd == RTSOCK_RTM_GET) && (mask != NULL))
		rtm->rtm_flags = RTM_F_PREFIX;

	snl_add_msg_attr_ip(&nw, RTA_DST, dst);
	snl_add_msg_attr_u32(&nw, RTA_TABLE, fib);

	uint32_t rta_oif = 0;

	if (gw != NULL) {
		if (rtm_flags & RTF_GATEWAY) {
			if (gw->sa_family == dst->sa_family)
				snl_add_msg_attr_ip(&nw, RTA_GATEWAY, gw);
			else
				snl_add_msg_attr_ipvia(&nw, RTA_VIA, gw);
			if (gw->sa_family == AF_INET6) {
				struct sockaddr_in6 *gw6 = (struct sockaddr_in6 *)gw;

				if (IN6_IS_ADDR_LINKLOCAL(&gw6->sin6_addr))
					rta_oif = gw6->sin6_scope_id;
			}
		} else {
			/* Should be AF_LINK */
			struct sockaddr_dl *sdl = (struct sockaddr_dl *)gw;
			if (sdl->sdl_index != 0)
				rta_oif = sdl->sdl_index;
		}
	}

	if (dst->sa_family == AF_INET6 && rta_oif == 0) {
		struct sockaddr_in6 *dst6 = (struct sockaddr_in6 *)dst;

		if (IN6_IS_ADDR_LINKLOCAL(&dst6->sin6_addr))
			rta_oif = dst6->sin6_scope_id;
	}

	if (rta_oif != 0)
		snl_add_msg_attr_u32(&nw, RTA_OIF, rta_oif);
	if (rtm_flags != 0)
		snl_add_msg_attr_u32(&nw, NL_RTA_RTFLAGS, rtm_flags);

	if (rt_metrics->rmx_mtu > 0) {
		int off = snl_add_msg_attr_nested(&nw, RTA_METRICS);
		snl_add_msg_attr_u32(&nw, RTAX_MTU, rt_metrics->rmx_mtu);
		snl_end_attr_nested(&nw, off);
	}

	if (rt_metrics->rmx_weight > 0)
		snl_add_msg_attr_u32(&nw, NL_RTA_WEIGHT, rt_metrics->rmx_weight);

	if (snl_finalize_msg(&nw) && snl_send_message(ss, hdr)) {
		struct snl_errmsg_data e = {};

		hdr = snl_read_reply(ss, hdr->nlmsg_seq);
		if (nl_type == NL_RTM_GETROUTE) {
			if (hdr->nlmsg_type == NL_RTM_NEWROUTE)
				print_getmsg(h, hdr, dst);
			else {
				snl_parse_errmsg(ss, hdr, &e);
				if (e.error == ESRCH)
					warn("route has not been found");
				else
					warn("message indicates error %d", e.error);
			}

			return (0);
		}

		if (snl_parse_errmsg(ss, hdr, &e))
			return (e.error);
	}
	return (EINVAL);
}

int
rtmsg_nl(int cmd, int rtm_flags, int fib, int rtm_addrs,
    struct sockaddr_storage *so, struct rt_metrics *rt_metrics)
{
	struct nl_helper h = {};

	nl_helper_init(&h);
	int error = rtmsg_nl_int(&h, cmd, rtm_flags, fib, rtm_addrs, so, rt_metrics);
	nl_helper_free(&h);

	return (error);
}

static void
get_ifdata(struct nl_helper *h, uint32_t ifindex, struct snl_parsed_link_simple *link)
{
	struct snl_state *ss = &h->ss_cmd;
	struct snl_writer nw;

	snl_init_writer(ss, &nw);
	struct nlmsghdr *hdr = snl_create_msg_request(&nw, NL_RTM_GETLINK);
	struct ifinfomsg *ifmsg = snl_reserve_msg_object(&nw, struct ifinfomsg);
	if (ifmsg != NULL)
		ifmsg->ifi_index = ifindex;
	if (!snl_finalize_msg(&nw) || !snl_send_message(ss, hdr))
		return;

	hdr = snl_read_reply(ss, hdr->nlmsg_seq);

	if (hdr != NULL && hdr->nlmsg_type == RTM_NEWLINK) {
		snl_parse_nlmsg(ss, hdr, &snl_rtm_link_parser_simple, link);
	}

	if (link->ifla_ifname == NULL) {
		char ifname[16];

		snprintf(ifname, sizeof(ifname), "if#%u", ifindex);
		int len = strlen(ifname);
		char *buf = snl_allocz(ss, len + 1);
		strlcpy(buf, ifname, len + 1);
		link->ifla_ifname = buf;
	}
}

static void
print_getmsg(struct nl_helper *h, struct nlmsghdr *hdr, struct sockaddr *dst)
{
	struct snl_state *ss = &h->ss_cmd;
	struct timespec ts;
	struct snl_parsed_route r = { .rtax_weight = RT_DEFAULT_WEIGHT };

	if (!snl_parse_nlmsg(ss, hdr, &snl_rtm_route_parser, &r))
		return;

	struct snl_parsed_link_simple link = {};
	get_ifdata(h, r.rta_oif, &link);

	if (r.rtax_mtu == 0)
		r.rtax_mtu = link.ifla_mtu;
	r.rta_rtflags |= (RTF_UP | RTF_DONE);

	(void)printf("   route to: %s\n", routename(dst));

	if (r.rta_dst)
		(void)printf("destination: %s\n", routename(r.rta_dst));
	struct sockaddr *mask = get_netmask(ss, r.rtm_family, r.rtm_dst_len);
	if (mask)
		(void)printf("       mask: %s\n", routename(mask));
	if (r.rta_gw && (r.rta_rtflags & RTF_GATEWAY))
		(void)printf("    gateway: %s\n", routename(r.rta_gw));
	(void)printf("        fib: %u\n", (unsigned int)r.rta_table);
	if (link.ifla_ifname)
		(void)printf("  interface: %s\n", link.ifla_ifname);
	(void)printf("      flags: ");
	printb(r.rta_rtflags, routeflags);

	struct rt_metrics rmx = {
		.rmx_mtu = r.rtax_mtu,
		.rmx_weight = r.rtax_weight,
		.rmx_expire = r.rta_expire,
	};

	printf("\n%9s %9s %9s %9s %9s %10s %9s\n", "recvpipe",
	    "sendpipe", "ssthresh", "rtt,msec", "mtu   ", "weight", "expire");
	printf("%8lu  ", rmx.rmx_recvpipe);
	printf("%8lu  ", rmx.rmx_sendpipe);
	printf("%8lu  ", rmx.rmx_ssthresh);
	printf("%8lu  ", 0UL);
	printf("%8lu  ", rmx.rmx_mtu);
	printf("%8lu  ", rmx.rmx_weight);
	if (rmx.rmx_expire > 0)
		clock_gettime(CLOCK_REALTIME_FAST, &ts);
	else
		ts.tv_sec = 0;
	printf("%8ld \n", (long)(rmx.rmx_expire - ts.tv_sec));
}

static void
print_prefix(struct nl_helper *h, char *buf, int bufsize, struct sockaddr *sa, int plen)
{
	int sz = 0;

	if (sa == NULL) {
		snprintf(buf, bufsize, "<NULL>");
		return;
	}

	switch (sa->sa_family) {
	case AF_INET:
		{
			struct sockaddr_in *sin = (struct sockaddr_in *)sa;
			char abuf[INET_ADDRSTRLEN];

			inet_ntop(AF_INET, &sin->sin_addr, abuf, sizeof(abuf));
			sz = snprintf(buf, bufsize, "%s", abuf);
			break;
		}
	case AF_INET6:
		{
			struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)sa;
			char abuf[INET6_ADDRSTRLEN];
			char *ifname = NULL;

			inet_ntop(AF_INET6, &sin6->sin6_addr, abuf, sizeof(abuf));
			if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr)) {
				struct snl_parsed_link_simple link = {};

				if (sin6->sin6_scope_id != 0) {
					get_ifdata(h, sin6->sin6_scope_id, &link);
					ifname = link.ifla_ifname;
				}
			}
			if (ifname == NULL)
				sz = snprintf(buf, bufsize, "%s", abuf);
			else
				sz = snprintf(buf, bufsize, "%s%%%s", abuf, ifname);
			break;
		}
	default:
		snprintf(buf, bufsize, "unknown_af#%d", sa->sa_family);
		plen = -1;
	}

	if (plen >= 0)
		snprintf(buf + sz, bufsize - sz, "/%d", plen);
}

static int
print_line_prefix(struct nlmsghdr *hdr, struct snl_msg_info *cinfo,
    const char *cmd, const char *name)
{
	struct timespec tp;
	struct tm tm;
	char buf[32];

	clock_gettime(CLOCK_REALTIME, &tp);
	localtime_r(&tp.tv_sec, &tm);

	strftime(buf, sizeof(buf), "%T", &tm);
	int len = printf("%s.%03ld PID %4u %s %s ", buf, tp.tv_nsec / 1000000,
	    cinfo->process_id, cmd, name);

	return (len);
}

static const char *
get_action_name(struct nlmsghdr *hdr, int new_cmd)
{
	if (hdr->nlmsg_type == new_cmd) {
		//return ((hdr->nlmsg_flags & NLM_F_REPLACE) ? "replace" : "add");
		return ("add/repl");
	} else
		return ("delete");
}

static void
print_nlmsg_route_nhop(struct nl_helper *h, struct snl_parsed_route *r,
    struct rta_mpath_nh *nh, bool first)
{
	// gw 10.0.0.1 ifp vtnet0 mtu 1500 table inet.0
	if (nh->gw != NULL) {
		char gwbuf[128];
		print_prefix(h, gwbuf, sizeof(gwbuf), nh->gw, -1);
		printf("gw %s ", gwbuf);
	}

	if (nh->ifindex != 0) {
		struct snl_parsed_link_simple link = {};

		get_ifdata(h, nh->ifindex, &link);
		if (nh->rtax_mtu == 0)
			nh->rtax_mtu = link.ifla_mtu;
		printf("iface %s ", link.ifla_ifname);
		if (nh->rtax_mtu != 0)
			printf("mtu %d ", nh->rtax_mtu);
	}

	if (first) {
		switch (r->rtm_family) {
			case AF_INET:
				printf("table inet.%d", r->rta_table);
				break;
			case AF_INET6:
				printf("table inet6.%d", r->rta_table);
				break;
		}
	}

	printf("\n");
}

static void
print_nlmsg_route(struct nl_helper *h, struct nlmsghdr *hdr,
    struct snl_msg_info *cinfo)
{
	struct snl_parsed_route r = { .rtax_weight = RT_DEFAULT_WEIGHT };
	struct snl_state *ss = &h->ss_cmd;

	if (!snl_parse_nlmsg(ss, hdr, &snl_rtm_route_parser, &r))
		return;

	// 20:19:41.333 add route 10.0.0.0/24 gw 10.0.0.1 ifp vtnet0 mtu 1500 table inet.0

	const char *cmd = get_action_name(hdr, RTM_NEWROUTE);
	int len = print_line_prefix(hdr, cinfo, cmd, "route");

	char buf[128];
	print_prefix(h, buf, sizeof(buf), r.rta_dst, r.rtm_dst_len);
	len += strlen(buf) + 1;
	printf("%s ", buf);

	switch (r.rtm_type) {
	case RTN_BLACKHOLE:
		printf("blackhole\n");
		return;
	case RTN_UNREACHABLE:
		printf("unreach(reject)\n");
		return;
	case RTN_PROHIBIT:
		printf("prohibit(reject)\n");
		return;
	}

	if (r.rta_multipath.num_nhops != 0) {
		bool first = true;

		memset(buf, ' ', sizeof(buf));
		buf[len] = '\0';

		for (uint32_t i = 0; i < r.rta_multipath.num_nhops; i++) {
			struct rta_mpath_nh *nh = r.rta_multipath.nhops[i];

			if (!first)
				printf("%s", buf);
			print_nlmsg_route_nhop(h, &r, nh, first);
			first = false;
		}
	} else {
		struct rta_mpath_nh nh = {
			.gw = r.rta_gw,
			.ifindex = r.rta_oif,
			.rtax_mtu = r.rtax_mtu,
		};

		print_nlmsg_route_nhop(h, &r, &nh, true);
	}
}

static const char *operstate[] = {
	"UNKNOWN",	/* 0, IF_OPER_UNKNOWN */
	"NOTPRESENT",	/* 1, IF_OPER_NOTPRESENT */
	"DOWN",		/* 2, IF_OPER_DOWN */
	"LLDOWN",	/* 3, IF_OPER_LOWERLAYERDOWN */
	"TESTING",	/* 4, IF_OPER_TESTING */
	"DORMANT",	/* 5, IF_OPER_DORMANT */
	"UP",		/* 6, IF_OPER_UP */
};

static void
print_nlmsg_link(struct nl_helper *h, struct nlmsghdr *hdr,
    struct snl_msg_info *cinfo)
{
	struct snl_parsed_link l = {};
	struct snl_state *ss = &h->ss_cmd;

	if (!snl_parse_nlmsg(ss, hdr, &snl_rtm_link_parser, &l))
		return;

	// 20:19:41.333 add iface#3 vtnet0 admin UP oper UP mtu 1500 table inet.0
	const char *cmd = get_action_name(hdr, RTM_NEWLINK);
	print_line_prefix(hdr, cinfo, cmd, "iface");

	printf("iface#%u %s ", l.ifi_index, l.ifla_ifname);
	printf("admin %s ", (l.ifi_flags & IFF_UP) ? "UP" : "DOWN");
	if (l.ifla_operstate < NL_ARRAY_LEN(operstate))
		printf("oper %s ", operstate[l.ifla_operstate]);
	if (l.ifla_mtu > 0)
		printf("mtu %u ", l.ifla_mtu);

	printf("\n");
}

static void
print_nlmsg_addr(struct nl_helper *h, struct nlmsghdr *hdr,
    struct snl_msg_info *cinfo)
{
	struct snl_parsed_addr attrs = {};
	struct snl_state *ss = &h->ss_cmd;

	if (!snl_parse_nlmsg(ss, hdr, &snl_rtm_addr_parser, &attrs))
		return;

	// add addr 192.168.1.1/24 iface vtnet0
	const char *cmd = get_action_name(hdr, RTM_NEWADDR);
	print_line_prefix(hdr, cinfo, cmd, "addr");

	char buf[128];
	struct sockaddr *addr = attrs.ifa_local ? attrs.ifa_local : attrs.ifa_address;
	print_prefix(h, buf, sizeof(buf), addr, attrs.ifa_prefixlen);
	printf("%s ", buf);

	struct snl_parsed_link_simple link = {};
	get_ifdata(h, attrs.ifa_index, &link);

	if (link.ifi_flags & IFF_POINTOPOINT) {
		char buf[64];
		print_prefix(h, buf, sizeof(buf), attrs.ifa_address, -1);
		printf("-> %s ", buf);
	}

	printf("iface %s ", link.ifla_ifname);

	printf("\n");
}

static const char *nudstate[] = {
	"INCOMPLETE",		/* 0x01(0) */
	"REACHABLE",		/* 0x02(1) */
	"STALE",		/* 0x04(2) */
	"DELAY",		/* 0x08(3) */
	"PROBE",		/* 0x10(4) */
	"FAILED",		/* 0x20(5) */
};

#define	NUD_INCOMPLETE		0x01	/* No lladdr, address resolution in progress */
#define	NUD_REACHABLE		0x02	/* reachable & recently resolved */
#define	NUD_STALE		0x04	/* has lladdr but it's stale */
#define	NUD_DELAY		0x08	/* has lladdr, is stale, probes delayed */
#define	NUD_PROBE		0x10	/* has lladdr, is stale, probes sent */
#define	NUD_FAILED		0x20	/* unused */


static void
print_nlmsg_neigh(struct nl_helper *h, struct nlmsghdr *hdr,
    struct snl_msg_info *cinfo)
{
	struct snl_parsed_neigh attrs = {};
	struct snl_state *ss = &h->ss_cmd;

	if (!snl_parse_nlmsg(ss, hdr, &snl_rtm_neigh_parser, &attrs))
		return;

	// add addr 192.168.1.1 state %s lladdr %s iface vtnet0
	const char *cmd = get_action_name(hdr, RTM_NEWNEIGH);
	print_line_prefix(hdr, cinfo, cmd, "neigh");

	char buf[128];
	print_prefix(h, buf, sizeof(buf), attrs.nda_dst, -1);
	printf("%s ", buf);

	struct snl_parsed_link_simple link = {};
	get_ifdata(h, attrs.nda_ifindex, &link);

	for (unsigned int i = 0; i < NL_ARRAY_LEN(nudstate); i++) {
		if ((1 << i) & attrs.ndm_state) {
			printf("state %s ", nudstate[i]);
			break;
		}
	}

	if (attrs.nda_lladdr != NULL) {
		int if_type = link.ifi_type;

		if ((if_type == IFT_ETHER || if_type == IFT_L2VLAN || if_type == IFT_BRIDGE) &&
		    NLA_DATA_LEN(attrs.nda_lladdr) == ETHER_ADDR_LEN) {
			struct ether_addr *ll;

			ll = (struct ether_addr *)NLA_DATA(attrs.nda_lladdr);
			printf("lladdr %s ", ether_ntoa(ll));
		} else {
			struct sockaddr_dl sdl = {
				.sdl_len = sizeof(sdl),
				.sdl_family = AF_LINK,
				.sdl_index = attrs.nda_ifindex,
				.sdl_type = if_type,
				.sdl_alen = NLA_DATA_LEN(attrs.nda_lladdr),
			};
			if (sdl.sdl_alen < sizeof(sdl.sdl_data)) {
				void *ll = NLA_DATA(attrs.nda_lladdr);

				memcpy(sdl.sdl_data, ll, sdl.sdl_alen);
				printf("lladdr %s ", link_ntoa(&sdl));
			}
		}
	}

	if (link.ifla_ifname != NULL)
		printf("iface %s ", link.ifla_ifname);
	printf("\n");
}

static void
print_nlmsg_generic(struct nl_helper *h, struct nlmsghdr *hdr, struct snl_msg_info *cinfo)
{
	const char *cmd = get_action_name(hdr, 0);
	print_line_prefix(hdr, cinfo, cmd, "unknown message");
	printf(" type %u\n", hdr->nlmsg_type);
}

static void
print_nlmsg(struct nl_helper *h, struct nlmsghdr *hdr, struct snl_msg_info *cinfo)
{
	switch (hdr->nlmsg_type) {
	case RTM_NEWLINK:
	case RTM_DELLINK:
		print_nlmsg_link(h, hdr, cinfo);
		break;
	case RTM_NEWADDR:
	case RTM_DELADDR:
		print_nlmsg_addr(h, hdr, cinfo);
		break;
	case RTM_NEWROUTE:
	case RTM_DELROUTE:
		print_nlmsg_route(h, hdr, cinfo);
		break;
	case RTM_NEWNEIGH:
	case RTM_DELNEIGH:
		print_nlmsg_neigh(h, hdr, cinfo);
		break;
	default:
		print_nlmsg_generic(h, hdr, cinfo);
	}

	snl_clear_lb(&h->ss_cmd);
}

void
monitor_nl(int fib)
{
	struct snl_state ss_event = {};
	struct nl_helper h;

	nl_init_socket(&ss_event);
	nl_helper_init(&h);

	int groups[] = {
		RTNLGRP_LINK,
		RTNLGRP_NEIGH,
		RTNLGRP_NEXTHOP,
#ifdef INET
		RTNLGRP_IPV4_IFADDR,
		RTNLGRP_IPV4_ROUTE,
#endif
#ifdef INET6
		RTNLGRP_IPV6_IFADDR,
		RTNLGRP_IPV6_ROUTE,
#endif
	};

	int optval = 1;
	socklen_t optlen = sizeof(optval);
	setsockopt(ss_event.fd, SOL_NETLINK, NETLINK_MSG_INFO, &optval, optlen);

	for (unsigned int i = 0; i < NL_ARRAY_LEN(groups); i++) {
		int error;
		int optval = groups[i];
		socklen_t optlen = sizeof(optval);
		error = setsockopt(ss_event.fd, SOL_NETLINK,
		    NETLINK_ADD_MEMBERSHIP, &optval, optlen);
		if (error != 0)
			warn("Unable to subscribe to group %d", optval);
	}

	struct snl_msg_info attrs = {};
	struct nlmsghdr *hdr;
	while ((hdr = snl_read_message_dbg(&ss_event, &attrs)) != NULL)
	{
		print_nlmsg(&h, hdr, &attrs);
		snl_clear_lb(&h.ss_cmd);
		snl_clear_lb(&ss_event);
	}

	snl_free(&ss_event);
	nl_helper_free(&h);
	exit(0);
}

static void
print_flushed_route(struct snl_parsed_route *r, struct sockaddr *gw)
{
	struct sockaddr *sa = r->rta_dst;

	printf("%-20.20s ", r->rta_rtflags & RTF_HOST ?
	    routename(sa) : netname(sa));
	sa = gw;
	printf("%-20.20s ", routename(sa));
	printf("-fib %-3d ", r->rta_table);
	printf("done\n");
}

static int
flushroute_one(struct nl_helper *h, struct snl_parsed_route *r)
{
	struct snl_state *ss = &h->ss_cmd;
	struct snl_errmsg_data e = {};
	struct snl_writer nw;

	snl_init_writer(ss, &nw);

	struct nlmsghdr *hdr = snl_create_msg_request(&nw, NL_RTM_DELROUTE);
	struct rtmsg *rtm = snl_reserve_msg_object(&nw, struct rtmsg);
	rtm->rtm_family = r->rtm_family;
	rtm->rtm_dst_len = r->rtm_dst_len;

	snl_add_msg_attr_u32(&nw, RTA_TABLE, r->rta_table);
	snl_add_msg_attr_ip(&nw, RTA_DST, r->rta_dst);

	if (!snl_finalize_msg(&nw) || !snl_send_message(ss, hdr))
		return (ENOMEM);

	if (!snl_read_reply_code(ss, hdr->nlmsg_seq, &e)) {
		return (e.error);
		if (e.error == EPERM)
			errc(1, e.error, "RTM_DELROUTE failed:");
		else
			warnc(e.error, "RTM_DELROUTE failed:");
		return (true);
	};

	if (verbose) {
		struct snl_msg_info attrs = {};
		print_nlmsg(h, hdr, &attrs);
	}
	else {
		if (r->rta_multipath.num_nhops != 0) {
			for (uint32_t i = 0; i < r->rta_multipath.num_nhops; i++) {
				struct rta_mpath_nh *nh = r->rta_multipath.nhops[i];

				print_flushed_route(r, nh->gw);
			}

		} else
			print_flushed_route(r, r->rta_gw);
	}

	return (0);
}

int
flushroutes_fib_nl(int fib, int af)
{
	struct snl_state ss = {};
	struct snl_writer nw;
	struct nl_helper h = {};

	nl_init_socket(&ss);
	snl_init_writer(&ss, &nw);

	struct nlmsghdr *hdr = snl_create_msg_request(&nw, NL_RTM_GETROUTE);
	hdr->nlmsg_flags |= NLM_F_DUMP;
	struct rtmsg *rtm = snl_reserve_msg_object(&nw, struct rtmsg);
	rtm->rtm_family = af;
	snl_add_msg_attr_u32(&nw, RTA_TABLE, fib);

	if (!snl_finalize_msg(&nw) || !snl_send_message(&ss, hdr)) {
		snl_free(&ss);
		return (EINVAL);
	}

	struct snl_errmsg_data e = {};
	uint32_t nlm_seq = hdr->nlmsg_seq;

	nl_helper_init(&h);
	
	while ((hdr = snl_read_reply_multi(&ss, nlm_seq, &e)) != NULL) {
		struct snl_parsed_route r = { .rtax_weight = RT_DEFAULT_WEIGHT };
		int error;

		if (!snl_parse_nlmsg(&ss, hdr, &snl_rtm_route_parser, &r))
			continue;
		if (verbose) {
			struct snl_msg_info attrs = {};
			print_nlmsg(&h, hdr, &attrs);
		}
		if (r.rta_table != (uint32_t)fib || r.rtm_family != af)
			continue;
		if ((r.rta_rtflags & RTF_GATEWAY) == 0)
			continue;
		if (debugonly)
			continue;

		if ((error = flushroute_one(&h, &r)) != 0) {
			if (error == EPERM)
				errc(1, error, "RTM_DELROUTE failed:");
			else
				warnc(error, "RTM_DELROUTE failed:");
		}
		snl_clear_lb(&h.ss_cmd);
	}

	snl_free(&ss);
	nl_helper_free(&h);

	return (e.error);
}

