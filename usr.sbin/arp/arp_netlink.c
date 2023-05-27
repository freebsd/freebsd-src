#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <err.h>
#include <errno.h>
#include <netdb.h>

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

#include <libxo/xo.h>
#include "arp.h"

#define RTF_ANNOUNCE	RTF_PROTO2

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
	if (!snl_finalize_msg(&nw) || !snl_send_message(ss, hdr))
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

static int
guess_ifindex(struct snl_state *ss, uint32_t fibnum, struct in_addr addr)
{
	struct snl_writer nw;

	snl_init_writer(ss, &nw);

	struct nlmsghdr *hdr = snl_create_msg_request(&nw, RTM_GETROUTE);
	struct rtmsg *rtm = snl_reserve_msg_object(&nw, struct rtmsg);
	rtm->rtm_family = AF_INET;

	struct sockaddr_in dst = { .sin_family = AF_INET, .sin_addr = addr };
	snl_add_msg_attr_ip(&nw, RTA_DST, (struct sockaddr *)&dst);
	snl_add_msg_attr_u32(&nw, RTA_TABLE, fibnum);

	if (!snl_finalize_msg(&nw) || !snl_send_message(ss, hdr))
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
	snl_add_msg_attr_u8(&nw, NHAF_FAMILY, AF_INET);
	snl_add_msg_attr_u32(&nw, NHAF_TABLE, fibnum);
	snl_end_attr_nested(&nw, off);

	if (!snl_finalize_msg(&nw) || !snl_send_message(ss, hdr))
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
fix_ifindex(struct snl_state *ss, uint32_t ifindex, struct in_addr addr)
{
	if (ifindex == 0)
		ifindex = guess_ifindex(ss, get_myfib(), addr);
	return (ifindex);
}

static void
print_entry(struct snl_parsed_neigh *neigh, struct snl_parsed_link_simple *link)
{
	const char *host;
	struct hostent *hp;
	struct sockaddr_in *addr = (struct sockaddr_in *)neigh->nda_dst;

	xo_open_instance("arp-cache");

	if (!opts.nflag)
		hp = gethostbyaddr((caddr_t)&(addr->sin_addr),
		    sizeof(addr->sin_addr), AF_INET);
	else
		hp = 0;
	if (hp)
		host = hp->h_name;
	else {
		host = "?";
		if (h_errno == TRY_AGAIN)
			opts.nflag = true;
	}
	xo_emit("{:hostname/%s} ({:ip-address/%s}) at ", host,
	    inet_ntoa(addr->sin_addr));
	if (neigh->nda_lladdr != NULL) {
		struct sockaddr_dl sdl = {
			.sdl_family = AF_LINK,
			.sdl_type = link->ifi_type,
			.sdl_len = sizeof(struct sockaddr_dl),
			.sdl_alen = NLA_DATA_LEN(neigh->nda_lladdr),
		};
		memcpy(sdl.sdl_data, NLA_DATA(neigh->nda_lladdr), sdl.sdl_alen);

		if ((sdl.sdl_type == IFT_ETHER ||
		    sdl.sdl_type == IFT_L2VLAN ||
		    sdl.sdl_type == IFT_BRIDGE) &&
		    sdl.sdl_alen == ETHER_ADDR_LEN)
			xo_emit("{:mac-address/%s}",
			    ether_ntoa((struct ether_addr *)LLADDR(&sdl)));
		else {

			xo_emit("{:mac-address/%s}", link_ntoa(&sdl));
		}
	} else
		xo_emit("{d:/(incomplete)}{en:incomplete/true}");
	xo_emit(" on {:interface/%s}", link->ifla_ifname);

	if (neigh->ndaf_next_ts == 0)
		xo_emit("{d:/ permanent}{en:permanent/true}");
	else {
		time_t expire_time;
		struct timeval now;

		gettimeofday(&now, 0);
		if ((expire_time = neigh->ndaf_next_ts - now.tv_sec) > 0)
			xo_emit(" expires in {:expires/%d} seconds",
			    (int)expire_time);
		else
			xo_emit("{d:/ expired}{en:expired/true}");
	}

	if (neigh->ndm_flags & NTF_PROXY)
		xo_emit("{d:/ published}{en:published/true}");

	switch(link->ifi_type) {
	case IFT_ETHER:
		xo_emit(" [{:type/ethernet}]");
		break;
	case IFT_FDDI:
		xo_emit(" [{:type/fddi}]");
		break;
	case IFT_ATM:
		xo_emit(" [{:type/atm}]");
		break;
	case IFT_L2VLAN:
		xo_emit(" [{:type/vlan}]");
		break;
	case IFT_IEEE1394:
		xo_emit(" [{:type/firewire}]");
		break;
	case IFT_BRIDGE:
		xo_emit(" [{:type/bridge}]");
		break;
	case IFT_INFINIBAND:
		xo_emit(" [{:type/infiniband}]");
		break;
	default:
		break;
	}

	xo_emit("\n");

	xo_close_instance("arp-cache");
}

int
print_entries_nl(uint32_t ifindex, struct in_addr addr)
{
	struct snl_state ss_req = {}, ss_cmd = {};
	struct snl_parsed_link_simple link = {};
	struct snl_writer nw;

	nl_init_socket(&ss_req);
	snl_init_writer(&ss_req, &nw);

	struct nlmsghdr *hdr = snl_create_msg_request(&nw, RTM_GETNEIGH);
	struct ndmsg *ndmsg = snl_reserve_msg_object(&nw, struct ndmsg);
	if (ndmsg != NULL) {
		ndmsg->ndm_family = AF_INET;
		ndmsg->ndm_ifindex = ifindex;
	}

	if (!snl_finalize_msg(&nw) || !snl_send_message(&ss_req, hdr)) {
		snl_free(&ss_req);
		return (0);
	}

	uint32_t nlmsg_seq = hdr->nlmsg_seq;
	struct snl_errmsg_data e = {};
	int count = 0;
	nl_init_socket(&ss_cmd);

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

		print_entry(&neigh, &link);
		count++;
		snl_clear_lb(&ss_req);
	}

	snl_free(&ss_req);
	snl_free(&ss_cmd);

	return (count);
}

int
delete_nl(uint32_t ifindex, char *host)
{
	struct snl_state ss = {};
	struct snl_writer nw;
	struct sockaddr_in *dst;

	dst = getaddr(host);
	if (dst == NULL)
		return (1);

	nl_init_socket(&ss);

	ifindex = fix_ifindex(&ss, ifindex, dst->sin_addr);
	if (ifindex == 0) {
		xo_warnx("delete: cannot locate %s", host);
		snl_free(&ss);
		return (0);
	}

	snl_init_writer(&ss, &nw);
	struct nlmsghdr *hdr = snl_create_msg_request(&nw, RTM_DELNEIGH);
	struct ndmsg *ndmsg = snl_reserve_msg_object(&nw, struct ndmsg);
	if (ndmsg != NULL) {
		ndmsg->ndm_family = AF_INET;
		ndmsg->ndm_ifindex = ifindex;
	}
	snl_add_msg_attr_ip(&nw, NDA_DST, (struct sockaddr *)dst);

	if (!snl_finalize_msg(&nw) || !snl_send_message(&ss, hdr)) {
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
	} else
		printf("%s (%s) deleted\n", host, inet_ntoa(dst->sin_addr));

	snl_free(&ss);

	return (e.error != 0);
}

int
set_nl(uint32_t ifindex, struct sockaddr_in *dst, struct sockaddr_dl *sdl, char *host)
{
	struct snl_state ss = {};
	struct snl_writer nw;

	nl_init_socket(&ss);

	ifindex = fix_ifindex(&ss, ifindex, dst->sin_addr);
	if (ifindex == 0) {
		xo_warnx("delete: cannot locate %s", host);
		snl_free(&ss);
		return (0);
	}

	if (opts.expire_time != 0)
		opts.flags &= ~RTF_STATIC;

	snl_init_writer(&ss, &nw);
	struct nlmsghdr *hdr = snl_create_msg_request(&nw, RTM_NEWNEIGH);
	hdr->nlmsg_flags |= NLM_F_CREATE | NLM_F_REPLACE;
	struct ndmsg *ndmsg = snl_reserve_msg_object(&nw, struct ndmsg);
	if (ndmsg != NULL) {
		uint8_t nl_flags = 0;

		ndmsg->ndm_family = AF_INET;
		ndmsg->ndm_ifindex = ifindex;
		ndmsg->ndm_state = (opts.flags & RTF_STATIC) ? NUD_PERMANENT : NUD_NONE;

		if (opts.flags & RTF_ANNOUNCE)
			nl_flags |= NTF_PROXY;
		if (opts.flags & RTF_STATIC)
			nl_flags |= NTF_STICKY;
		ndmsg->ndm_flags = nl_flags;
	}
	snl_add_msg_attr_ip(&nw, NDA_DST, (struct sockaddr *)dst);
	snl_add_msg_attr(&nw, NDA_LLADDR, sdl->sdl_alen, LLADDR(sdl));
	
	if (opts.expire_time != 0) {
		struct timeval now;

		gettimeofday(&now, 0);
		int off = snl_add_msg_attr_nested(&nw, NDA_FREEBSD);
		snl_add_msg_attr_u32(&nw, NDAF_NEXT_STATE_TS, now.tv_sec + opts.expire_time);
		snl_end_attr_nested(&nw, off);
	}

	if (!snl_finalize_msg(&nw) || !snl_send_message(&ss, hdr)) {
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

