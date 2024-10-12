#include <sys/param.h>
#include <sys/module.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/linker.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/queue.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <netinet/icmp6.h>
#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>

#include <arpa/inet.h>

#include <ctype.h>
#include <netdb.h>
#include <errno.h>
#include <nlist.h>
#include <stdio.h>
#include <string.h>
#include <paths.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <libxo/xo.h>


#include <netlink/netlink.h>
#include <netlink/netlink_route.h>
#include <netlink/netlink_snl.h>
#include <netlink/netlink_snl_route.h>
#include <netlink/netlink_snl_route_compat.h>
#include <netlink/netlink_snl_route_parsers.h>

#include "ndp.h"

#define RTF_ANNOUNCE	RTF_PROTO2

static void
nl_init_socket(struct snl_state *ss)
{
	if (snl_init(ss, NETLINK_ROUTE))
		return;

	if (modfind("netlink") == -1 && errno == ENOENT) {
		/* Try to load */
		if (kldload("netlink") == -1)
			xo_err(1, "netlink is not loaded and load attempt failed");
		if (snl_init(ss, NETLINK_ROUTE))
			return;
	}

	xo_err(1, "unable to open netlink socket");
}

static bool
get_link_info(struct snl_state *ss, uint32_t ifindex,
    struct snl_parsed_link_simple *link)
{
	struct snl_writer nw;

	snl_init_writer(ss, &nw);

	struct nlmsghdr *hdr = snl_create_msg_request(&nw, RTM_GETLINK);
	struct ifinfomsg *ifmsg = snl_reserve_msg_object(&nw, struct ifinfomsg);
	if (ifmsg != NULL)
		ifmsg->ifi_index = ifindex;
	if (! (hdr = snl_finalize_msg(&nw)) || !snl_send_message(ss, hdr))
		return (false);

	hdr = snl_read_reply(ss, hdr->nlmsg_seq);

	if (hdr == NULL || hdr->nlmsg_type != RTM_NEWLINK)
		return (false);

	if (!snl_parse_nlmsg(ss, hdr, &snl_rtm_link_parser_simple, link))
		return (false);

	return (true);
}



static bool
has_l2(struct snl_state *ss, uint32_t ifindex)
{
	struct snl_parsed_link_simple link = {};

	if (!get_link_info(ss, ifindex, &link))
		return (false);

	return (valid_type(link.ifi_type) != 0);
}

static uint32_t
get_myfib(void)
{
	uint32_t fibnum = 0;
	size_t len = sizeof(fibnum);

	sysctlbyname("net.my_fibnum", (void *)&fibnum, &len, NULL, 0);

	return (fibnum);
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
#define s6_addr32 __u6_addr.__u6_addr32
#define IN6_MASK_ADDR(a, m)	do { \
	(a)->s6_addr32[0] &= (m)->s6_addr32[0]; \
	(a)->s6_addr32[1] &= (m)->s6_addr32[1]; \
	(a)->s6_addr32[2] &= (m)->s6_addr32[2]; \
	(a)->s6_addr32[3] &= (m)->s6_addr32[3]; \
} while (0)

static int
guess_ifindex(struct snl_state *ss, uint32_t fibnum, const struct sockaddr_in6 *dst)
{
	struct snl_writer nw;

	if (IN6_IS_ADDR_LINKLOCAL(&dst->sin6_addr))
		return (dst->sin6_scope_id);
	else if (IN6_IS_ADDR_MULTICAST(&dst->sin6_addr))
		return (0);


	snl_init_writer(ss, &nw);

	struct nlmsghdr *hdr = snl_create_msg_request(&nw, RTM_GETROUTE);
	struct rtmsg *rtm = snl_reserve_msg_object(&nw, struct rtmsg);
	rtm->rtm_family = AF_INET6;

	snl_add_msg_attr_ip(&nw, RTA_DST, (struct sockaddr *)dst);
	snl_add_msg_attr_u32(&nw, RTA_TABLE, fibnum);

	if (! (hdr = snl_finalize_msg(&nw)) || !snl_send_message(ss, hdr))
		return (0);

	hdr = snl_read_reply(ss, hdr->nlmsg_seq);

	if (hdr->nlmsg_type != NL_RTM_NEWROUTE) {
		/* No route found, unable to guess ifindex */
		return (0);
	}

	struct snl_parsed_route r = {};
	if (!snl_parse_nlmsg(ss, hdr, &snl_rtm_route_parser, &r))
		return (0);

	if (r.rta_multipath.num_nhops > 0 || (r.rta_rtflags & RTF_GATEWAY))
		return (0);

	/* Check if the interface is of supported type */
	if (has_l2(ss, r.rta_oif))
		return (r.rta_oif);

	/* Check the case when we matched the loopback route for P2P */
	snl_init_writer(ss, &nw);
	hdr = snl_create_msg_request(&nw, RTM_GETNEXTHOP);
	snl_reserve_msg_object(&nw, struct nhmsg);

	int off = snl_add_msg_attr_nested(&nw, NHA_FREEBSD);
	snl_add_msg_attr_u32(&nw, NHAF_KID, r.rta_knh_id);
	snl_add_msg_attr_u8(&nw, NHAF_FAMILY, AF_INET6);
	snl_add_msg_attr_u32(&nw, NHAF_TABLE, fibnum);
	snl_end_attr_nested(&nw, off);

	if (! (hdr = snl_finalize_msg(&nw)) || !snl_send_message(ss, hdr))
		return (0);

	hdr = snl_read_reply(ss, hdr->nlmsg_seq);

	if (hdr->nlmsg_type != NL_RTM_NEWNEXTHOP) {
		/* No nexthop found, unable to guess ifindex */
		return (0);
	}

	struct snl_parsed_nhop nh = {};
	if (!snl_parse_nlmsg(ss, hdr, &snl_nhmsg_parser, &nh))
		return (0);

	return (nh.nhaf_aif);
}

static uint32_t
fix_ifindex(struct snl_state *ss, uint32_t ifindex, const struct sockaddr_in6 *sa)
{
	if (ifindex == 0)
		ifindex = guess_ifindex(ss, get_myfib(), sa);
	return (ifindex);
}

static void
print_entry(struct snl_parsed_neigh *neigh, struct snl_parsed_link_simple *link)
{
	struct timeval now;
	char host_buf[NI_MAXHOST];
	int addrwidth;
	int llwidth;
	int ifwidth;
	char *ifname;

	getnameinfo(neigh->nda_dst, sizeof(struct sockaddr_in6), host_buf,
	    sizeof(host_buf), NULL, 0, (opts.nflag ? NI_NUMERICHOST : 0));

	gettimeofday(&now, 0);
	if (opts.tflag)
		ts_print(&now);

	struct sockaddr_dl sdl = {
		.sdl_family = AF_LINK,
		.sdl_type = link->ifi_type,
		.sdl_len = sizeof(struct sockaddr_dl),
	};

	if (neigh->nda_lladdr) {
		sdl.sdl_alen = NLA_DATA_LEN(neigh->nda_lladdr),
		memcpy(sdl.sdl_data, NLA_DATA(neigh->nda_lladdr), sdl.sdl_alen);
	}

	addrwidth = strlen(host_buf);
	if (addrwidth < W_ADDR)
		addrwidth = W_ADDR;
	llwidth = strlen(ether_str(&sdl));
	if (W_ADDR + W_LL - addrwidth > llwidth)
		llwidth = W_ADDR + W_LL - addrwidth;
	ifname = link->ifla_ifname;
	ifwidth = strlen(ifname);
	if (W_ADDR + W_LL + W_IF - addrwidth - llwidth > ifwidth)
		ifwidth = W_ADDR + W_LL + W_IF - addrwidth - llwidth;

	xo_open_instance("neighbor-cache");
	/* Compose format string for libxo, as it doesn't support *.* */
	char xobuf[200];
	snprintf(xobuf, sizeof(xobuf),
	    "{:address/%%-%d.%ds/%%s} {:mac-address/%%-%d.%ds/%%s} {:interface/%%%d.%ds/%%s}",
	    addrwidth, addrwidth, llwidth, llwidth, ifwidth, ifwidth);
	xo_emit(xobuf, host_buf, ether_str(&sdl), ifname);

	/* Print neighbor discovery specific information */
	time_t expire = (time_t)neigh->ndaf_next_ts;
	int expire_in = expire - now.tv_sec;
	if (expire > now.tv_sec)
		xo_emit("{d:/ %-9.9s}{e:expires_sec/%d}", sec2str(expire_in), expire_in);
	else if (expire == 0)
		xo_emit("{d:/ %-9.9s}{en:permanent/true}", "permanent");
	else
		xo_emit("{d:/ %-9.9s}{e:expires_sec/%d}", "expired", expire_in);

	const char *lle_state = "";
	switch (neigh->ndm_state) {
	case NUD_INCOMPLETE:
		lle_state = "I";
		break;
	case NUD_REACHABLE:
		lle_state = "R";
		break;
	case NUD_STALE:
		lle_state = "S";
		break;
	case NUD_DELAY:
		lle_state = "D";
		break;
	case NUD_PROBE:
		lle_state = "P";
		break;
	case NUD_FAILED:
		lle_state = "F";
		break;
	default:
		lle_state = "N";
		break;
	}
	xo_emit(" {:neighbor-state/%s}", lle_state);

	bool isrouter = neigh->ndm_flags & NTF_ROUTER;

	/*
	 * other flags. R: router, P: proxy, W: ??
	 */
	char flgbuf[8];
	snprintf(flgbuf, sizeof(flgbuf), "%s%s",
	    isrouter ? "R" : "",
	    (neigh->ndm_flags & NTF_PROXY) ? "p" : "");
	xo_emit(" {:nd-flags/%s}", flgbuf);

	if (neigh->nda_probes != 0)
		xo_emit("{u:/ %d}", neigh->nda_probes);

	xo_emit("\n");
	xo_close_instance("neighbor-cache");
}

int
print_entries_nl(uint32_t ifindex, struct sockaddr_in6 *addr, bool cflag)
{
	struct snl_state ss_req = {}, ss_cmd = {};
	struct snl_parsed_link_simple link = {};
	struct snl_writer nw;

	nl_init_socket(&ss_req);
	snl_init_writer(&ss_req, &nw);

	struct nlmsghdr *hdr = snl_create_msg_request(&nw, RTM_GETNEIGH);
	struct ndmsg *ndmsg = snl_reserve_msg_object(&nw, struct ndmsg);
	if (ndmsg != NULL) {
		ndmsg->ndm_family = AF_INET6;
		ndmsg->ndm_ifindex = ifindex;
	}

	if (! (hdr = snl_finalize_msg(&nw)) || !snl_send_message(&ss_req, hdr)) {
		snl_free(&ss_req);
		return (0);
	}

	uint32_t nlmsg_seq = hdr->nlmsg_seq;
	struct snl_errmsg_data e = {};
	int count = 0;
	nl_init_socket(&ss_cmd);

	/* Print header */
	if (!opts.tflag && !cflag) {
		char xobuf[200];
		snprintf(xobuf, sizeof(xobuf),
		    "{T:/%%-%d.%ds} {T:/%%-%d.%ds} {T:/%%%d.%ds} {T:/%%-9.9s} {T:/%%1s} {T:/%%5s}\n",
		    W_ADDR, W_ADDR, W_LL, W_LL, W_IF, W_IF);
		xo_emit(xobuf, "Neighbor", "Linklayer Address", "Netif", "Expire", "S", "Flags");
	}
	xo_open_list("neighbor-cache");

	while ((hdr = snl_read_reply_multi(&ss_req, nlmsg_seq, &e)) != NULL) {
		struct snl_parsed_neigh neigh = {};

		if (!snl_parse_nlmsg(&ss_req, hdr, &snl_rtm_neigh_parser, &neigh))
			continue;

		if (neigh.nda_ifindex != link.ifi_index) {
			snl_clear_lb(&ss_cmd);
			memset(&link, 0, sizeof(link));
			if (!get_link_info(&ss_cmd, neigh.nda_ifindex, &link))
				continue;
		}

		/* TODO: embed LL in the parser */
		struct sockaddr_in6 *dst = (struct sockaddr_in6 *)neigh.nda_dst;
		if (IN6_IS_ADDR_LINKLOCAL(&dst->sin6_addr))
			dst->sin6_scope_id = neigh.nda_ifindex;

		if (addr != NULL) {
			if (IN6_ARE_ADDR_EQUAL(&addr->sin6_addr,
			    &dst->sin6_addr) == 0 ||
			    addr->sin6_scope_id != dst->sin6_scope_id)
				continue;
		}

		if (cflag) {
			char dst_str[INET6_ADDRSTRLEN];

			inet_ntop(AF_INET6, &dst->sin6_addr, dst_str, sizeof(dst_str));
			delete_nl(neigh.nda_ifindex, dst_str, false); /* no warn */
		} else
			print_entry(&neigh, &link);

		count++;
		snl_clear_lb(&ss_req);
	}
	xo_close_list("neighbor-cache");

	snl_free(&ss_req);
	snl_free(&ss_cmd);

	return (count);
}

int
delete_nl(uint32_t ifindex, char *host, bool warn)
{
#define xo_warnx(...) do { if (warn) { xo_warnx(__VA_ARGS__); } } while(0)
	struct snl_state ss = {};
	struct snl_writer nw;
	struct sockaddr_in6 dst;

	int gai_error = getaddr(host, &dst);
	if (gai_error) {
		xo_warnx("%s: %s", host, gai_strerror(gai_error));
		return 1;
	}

	nl_init_socket(&ss);

	ifindex = fix_ifindex(&ss, ifindex, &dst);
	if (ifindex == 0) {
		xo_warnx("delete: cannot locate %s", host);
		snl_free(&ss);
		return (0);
	}

	snl_init_writer(&ss, &nw);
	struct nlmsghdr *hdr = snl_create_msg_request(&nw, RTM_DELNEIGH);
	struct ndmsg *ndmsg = snl_reserve_msg_object(&nw, struct ndmsg);
	if (ndmsg != NULL) {
		ndmsg->ndm_family = AF_INET6;
		ndmsg->ndm_ifindex = ifindex;
	}
	snl_add_msg_attr_ip(&nw, NDA_DST, (struct sockaddr *)&dst);

	if (! (hdr = snl_finalize_msg(&nw)) || !snl_send_message(&ss, hdr)) {
		snl_free(&ss);
		return (1);
	}

	struct snl_errmsg_data e = {};
	snl_read_reply_code(&ss, hdr->nlmsg_seq, &e);
	if (e.error != 0) {
		if (e.error_str != NULL)
			xo_warnx("delete %s: %s (%s)", host, strerror(e.error), e.error_str);
		else
			xo_warnx("delete %s: %s", host, strerror(e.error));
	} else {
		char host_buf[NI_MAXHOST];
		char ifix_buf[IFNAMSIZ];

		getnameinfo((struct sockaddr *)&dst,
		    dst.sin6_len, host_buf,
		    sizeof(host_buf), NULL, 0,
		    (opts.nflag ? NI_NUMERICHOST : 0));

		char *ifname = if_indextoname(ifindex, ifix_buf);
		if (ifname == NULL) {
			strlcpy(ifix_buf, "?", sizeof(ifix_buf));
			ifname = ifix_buf;
		}
		char abuf[INET6_ADDRSTRLEN];
		inet_ntop(AF_INET6, &dst.sin6_addr, abuf, sizeof(abuf));

		xo_open_instance("neighbor-cache");
		xo_emit("{:hostname/%s}{d:/ (%s) deleted\n}", host, host_buf);
		xo_emit("{e:address/%s}{e:interface/%s}", abuf, ifname);
		xo_close_instance("neighbor-cache");
	}
	snl_free(&ss);

	return (e.error != 0);
#undef xo_warnx /* see above */
}

int
set_nl(uint32_t ifindex, struct sockaddr_in6 *dst, struct sockaddr_dl *sdl, char *host)
{
	struct snl_state ss = {};
	struct snl_writer nw;

	nl_init_socket(&ss);

	ifindex = fix_ifindex(&ss, ifindex, dst);
	if (ifindex == 0) {
		xo_warnx("delete: cannot locate %s", host);
		snl_free(&ss);
		return (0);
	}

	snl_init_writer(&ss, &nw);
	struct nlmsghdr *hdr = snl_create_msg_request(&nw, RTM_NEWNEIGH);
	hdr->nlmsg_flags |= NLM_F_CREATE | NLM_F_REPLACE;
	struct ndmsg *ndmsg = snl_reserve_msg_object(&nw, struct ndmsg);
	if (ndmsg != NULL) {
		uint8_t nl_flags = NTF_STICKY;

		ndmsg->ndm_family = AF_INET6;
		ndmsg->ndm_ifindex = ifindex;
		ndmsg->ndm_state = NUD_PERMANENT;

		if (opts.flags & RTF_ANNOUNCE)
			nl_flags |= NTF_PROXY;
		ndmsg->ndm_flags = nl_flags;
	}
	snl_add_msg_attr_ip(&nw, NDA_DST, (struct sockaddr *)dst);
	snl_add_msg_attr(&nw, NDA_LLADDR, sdl->sdl_alen, LLADDR(sdl));

	if (! (hdr = snl_finalize_msg(&nw)) || !snl_send_message(&ss, hdr)) {
		snl_free(&ss);
		return (1);
	}

	struct snl_errmsg_data e = {};
	snl_read_reply_code(&ss, hdr->nlmsg_seq, &e);
	if (e.error != 0) {
		if (e.error_str != NULL)
			xo_warnx("set: %s: %s (%s)", host, strerror(e.error), e.error_str);
		else
			xo_warnx("set %s: %s", host, strerror(e.error));
	}
	snl_free(&ss);

	return (e.error != 0);
}

